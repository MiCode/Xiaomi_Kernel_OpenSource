/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define TMEM_MEMMGR_FMT
#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/random.h>

#define PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS
#include "pmem_plat.h" PLAT_HEADER_MUST_BE_INCLUDED_BEFORE_OTHER_HEADERS

#include "private/mld_helper.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"

#ifdef TCORE_UT_FWK_SUPPORT
#include "private/ut_tests.h"
#include "private/ut_cmd.h"
DEFINE_UT_SUPPORT(memmgr);
#endif

/* Adjustable customization */
#define MIN_BUDDY_BLOCK_SIZE (SIZE_1K)

/* 1KB(0), 2KB(1), 4KB(2), 8KB(3), 16KB(4), 32KB(5), 64KB(6), 128KB(7),
 * 256KB(8), 512KB(9), 1MB(10), 2MB(11), 4MB(12), 8MB(13), 16MB(14)
 */
#define MAX_ORDER_NUM (14)

/* Force the pa address to be aligned with its input size */
#define FORCE_PA_ALIGH_WITH_SIZE (0)

/* clang-format off */
#define MEMMGR_LIST_DEBUG_ENABLE (0)
#define MEMMGR_OUT_OF_MEMORY_HIT_ENABLE (0)
#define MEMMGR_AREA_BLK_DEBUG_ENABLE (0)

#if (MEMMGR_LIST_DEBUG_ENABLE)
#define MEMMGR_LIST_CHECK(list) \
	do { \
		if (list.next == LIST_POISON1) { \
			pr_err("list is deleted before (LIST_POISON1)\n"); \
		} \
		if (list.prev == LIST_POISON2) { \
			pr_err("list is deleted ever (LIST_POISON2)\n"); \
		} \
	} while (0)
#else
#define MEMMGR_LIST_CHECK(list)  do {} while (0)
#endif
/* clang-format on */

#define MEMMGR_UT_PA_ADDR64_START (0x180000000ULL)
#define MEMMGR_BUDDY_POOL_SIZE (SIZE_512M)

#define CONTROL_BLOCK_COUNT                                                    \
	((MEMMGR_BUDDY_POOL_SIZE / MIN_BUDDY_BLOCK_SIZE) + 1)

#define CONTROL_AREA_COUNT (MAX_ORDER_NUM + 1)

#define ORDER_SIZE(order) (MIN_BUDDY_BLOCK_SIZE << order)

#define PA_TO_HANDLE(pa) (u32)((pa >> PMEM_64BIT_PHYS_SHIFT) & 0xFFFFFFFFULL)
#define HANDLE_TO_PA(handle)                                                   \
	(((u64)handle << PMEM_64BIT_PHYS_SHIFT)                                \
	 & ~((1 << PMEM_64BIT_PHYS_SHIFT) - 1))

enum BLK_STATE {
	BLK_BAD = 0,       /* out of managed range (make reset as bad) */
	BLK_INIT = 1,      /* in block list (initialized or not in area) */
	BLK_FREE = 2,      /* in area list (added to area) */
	BLK_USED = 3,      /* in block list: (allocated) */
	BLK_AREA_HEAD = 4, /* area head (not relevant) */
};

struct block_state {
	int state;
	const char *str;
};

#define STR(a) #a
struct block_state block_state_str[] = {
	{BLK_BAD, STR(BLK_BAD)},
	{BLK_INIT, STR(BLK_INIT)},
	{BLK_FREE, STR(BLK_FREE)},
	{BLK_USED, STR(BLK_USED)},
	{BLK_AREA_HEAD, STR(BLK_AREA_HEAD)},
};

struct Block {
	u64 phy_addr;
	u32 size;
	u32 ref_cnt;
	u32 status;
	struct list_head list;
};

struct memmgr_control_info {
	struct Block blocks[CONTROL_BLOCK_COUNT];
	struct Block areas[CONTROL_AREA_COUNT];
	u32 free_areas[CONTROL_AREA_COUNT];
	u32 managed_blocks;
	u64 pool_pa_start;
	u32 pool_size;
	u32 used_size;
	u32 free_size;
	struct mutex memmgr_lock;

#if MEMMGR_AREA_BLK_DEBUG_ENABLE
	u32 blk_init_state_dbg[CONTROL_BLOCK_COUNT];
	int area_buddy_cnt_dbg[CONTROL_AREA_COUNT];
#endif
};

static struct memmgr_control_info memmgr_ctrl_info;

static DEFINE_MUTEX(memmgr_region_lock);
static bool memmgr_region_ready;
static void set_region_ready(bool ready)
{
	mutex_lock(&memmgr_region_lock);
	memmgr_region_ready = ready;
	mutex_unlock(&memmgr_region_lock);
}

static bool is_region_ready(void)
{
	bool ret;

	mutex_lock(&memmgr_region_lock);
	ret = memmgr_region_ready;
	mutex_unlock(&memmgr_region_lock);
	return ret;
}

static bool is_valid_pa_range(u64 pa, u64 valid_pa, u32 valid_size)
{
	if ((valid_pa <= pa) && (pa < (valid_pa + valid_size)))
		return true;

	return false;
}

static int get_blk_order(int size)
{
	int order = 0, order_size = 0;
	int blk_size = MIN_BUDDY_BLOCK_SIZE;

	if (size > ORDER_SIZE(MAX_ORDER_NUM)) {
		pr_err("wrong requested size: 0x%x\n", size);
		return -1; /* invalid order */
	}

	for (order = 0; order <= MAX_ORDER_NUM; order++) {
		order_size = blk_size << order;
		if (order_size >= size)
			break;
	}

	return order;
}

static int get_blk_number(u64 region_start, u64 current_pa, u32 blk_size)
{
	return ((current_pa - region_start) / blk_size);
}

static u32 get_ordered_size(u32 size)
{
	int order;

	if (size < MIN_BUDDY_BLOCK_SIZE)
		return MIN_BUDDY_BLOCK_SIZE;

	order = get_blk_order(size);
	if (order < 0)
		return ORDER_SIZE(0);

	return ORDER_SIZE(order);
}

/* lock must be hold before entering */
static int find_first_satified_area(u32 size)
{
	int area_num;
	int chunk_size;

	for (area_num = 0; area_num <= MAX_ORDER_NUM; area_num++) {
		chunk_size = memmgr_ctrl_info.areas[area_num].size;
		if (size > chunk_size)
			continue;
		if (memmgr_ctrl_info.free_areas[area_num] == 1)
			return area_num;
	}

	return -1;
}

/* lock must be hold before entering */
static void add_buddy_to_area(struct Block *buddy, int area_num)
{
	MEMMGR_LIST_CHECK(buddy->list);
	list_add(&buddy->list, &(memmgr_ctrl_info.areas[area_num].list));
	memmgr_ctrl_info.free_areas[area_num] = 1;

#if MEMMGR_AREA_BLK_DEBUG_ENABLE
	memmgr_ctrl_info.area_buddy_cnt_dbg[area_num]++;

	pr_debug("area add cnt:%d %d %d %d (%d, %d, %d, %d)\n",
		 memmgr_ctrl_info.area_buddy_cnt_dbg[3],
		 memmgr_ctrl_info.area_buddy_cnt_dbg[2],
		 memmgr_ctrl_info.area_buddy_cnt_dbg[1],
		 memmgr_ctrl_info.area_buddy_cnt_dbg[0],
		 memmgr_ctrl_info.free_areas[3], memmgr_ctrl_info.free_areas[2],
		 memmgr_ctrl_info.free_areas[1],
		 memmgr_ctrl_info.free_areas[0]);
#endif
}

/* lock must be hold before entering */
static void remove_buddy_from_area(struct Block *buddy, int area_num)
{
	MEMMGR_LIST_CHECK(buddy->list);
	list_del(&buddy->list);

	if (list_empty(&memmgr_ctrl_info.areas[area_num].list))
		memmgr_ctrl_info.free_areas[area_num] = 0;

#if MEMMGR_AREA_BLK_DEBUG_ENABLE
	if (memmgr_ctrl_info.area_buddy_cnt_dbg > 0) {
		memmgr_ctrl_info.area_buddy_cnt_dbg[area_num]--;
	} else {
		pr_err("system error, area:%d already had no buddy!\n",
		       area_num);
	}

	pr_debug("area re cnt:%d %d %d %d (%d, %d, %d, %d)\n",
		 memmgr_ctrl_info.area_buddy_cnt_dbg[3],
		 memmgr_ctrl_info.area_buddy_cnt_dbg[2],
		 memmgr_ctrl_info.area_buddy_cnt_dbg[1],
		 memmgr_ctrl_info.area_buddy_cnt_dbg[0],
		 memmgr_ctrl_info.free_areas[3], memmgr_ctrl_info.free_areas[2],
		 memmgr_ctrl_info.free_areas[1],
		 memmgr_ctrl_info.free_areas[0]);
#endif
}

/* lock must be hold before entering */
static struct Block *get_buddy_removed_by_area(int area_num)
{
	struct Block *buddy = NULL, *tmp;

	if (list_empty(&memmgr_ctrl_info.areas[area_num].list)) {
		pr_err("no buddy is found in area %d\n", area_num);
		return NULL;
	}

	list_for_each_entry_safe(
		buddy, tmp, &memmgr_ctrl_info.areas[area_num].list, list) {
		break; /* find the first entry */
	}

	remove_buddy_from_area(buddy, area_num);
	return buddy;
}

/* lock must be hold before entering */
static int try_buddy_split(struct Block *src_buddy, u32 target_size)
{
	int src_order, target_order;
	int area_num, split_size, split_blk_start;
	struct Block *split_buddy;
	u64 split_pa;
	int target_merge_in_area;

	if (INVALID(src_buddy)) {
		pr_err("input buddy is invalid\n");
		return TMEM_GENERAL_ERROR;
	}

	src_order = get_blk_order(src_buddy->size);
	target_order = get_blk_order(target_size);

	if (src_order == target_order) {
		pr_err("no split is required\n");
		return TMEM_GENERAL_ERROR;
	}

	pr_debug("split size from 0x%x to 0x%x\n", src_buddy->size,
		 target_size);
	pr_debug("search order from %d to %d\n", src_order, (target_order + 1));

	/* split buddy from src order to target order */
	for (area_num = src_order; area_num > target_order; area_num--) {
		split_size = src_buddy->size / 2;
		split_pa = src_buddy->phy_addr + split_size;
		split_blk_start =
			get_blk_number(memmgr_ctrl_info.pool_pa_start, split_pa,
				       MIN_BUDDY_BLOCK_SIZE);
		split_buddy = &memmgr_ctrl_info.blocks[split_blk_start];
		pr_debug("area: %d, split sz: 0x%x, split blk:%d\n", area_num,
			 split_size, split_blk_start);

		/* extra check: make sure pa is matched */
		if (split_pa != split_buddy->phy_addr) {
			pr_err("wrong phyical address in blk:%d (0x%llx != 0x%llx)\n",
			       split_blk_start, split_pa,
			       split_buddy->phy_addr);
			return TMEM_GENERAL_ERROR;
		}

		/* extra check: make sure blk state is correct */
		if (split_buddy->status != BLK_INIT) {
			pr_err("system error in blk state: %s!\n",
			       block_state_str[split_buddy->status].str);
			return TMEM_GENERAL_ERROR;
		}

		/* update source buddy */
		src_buddy->size = split_size;

		/* update split buddy */
		split_buddy->size = split_size;
		split_buddy->ref_cnt = 0;
		split_buddy->status = BLK_FREE;

		/* extra check: make sure we don't insert into wrong area */
		target_merge_in_area = get_blk_order(split_buddy->size);
		if (target_merge_in_area < 0) {
			pr_err("system error in split buddy!\n");
			return TMEM_GENERAL_ERROR;
		} else if (memmgr_ctrl_info.areas[target_merge_in_area].size
			   != split_buddy->size) {
			pr_err("system error split to wrong area:%d, sz:0x%x\n",
			       (area_num - 1), split_buddy->size);
			return TMEM_GENERAL_ERROR;
		}

		add_buddy_to_area(split_buddy, target_merge_in_area);

		/* buddy split ended */
		if (split_size == target_size) {
			pr_debug("split ended and found blk:%d, sz:0x%x\n",
				 get_blk_number(memmgr_ctrl_info.pool_pa_start,
						src_buddy->phy_addr,
						MIN_BUDDY_BLOCK_SIZE),
				 src_buddy->size);
			return TMEM_OK;
		}
	}

	pr_err("split end with error!\n");
	return TMEM_GENERAL_ERROR;
}

static bool is_left_side_buddy(u64 src_pa, u32 modular_sz)
{
	if (0 == (src_pa % modular_sz))
		return true;

	return false;
}

static u64 get_merged_buddy_pa(u64 src_pa, u32 modular_sz)
{
	/* return target is right buddy */
	if (is_left_side_buddy(src_pa, modular_sz))
		return (src_pa + (modular_sz / 2));

	/* return target is left buddy */
	return (src_pa - (modular_sz / 2));
}

/* lock must be hold before entering */
static int try_buddy_merge(struct Block *free_buddy)
{
	int src_order;
	int area_num, target_size;
	struct Block *src_buddy, *target_buddy;
	u64 target_pa;
	int target_blk_num, src_blk_num;
	int target_merge_in_area;

	if (INVALID(free_buddy)) {
		pr_err("input buddy is invalid\n");
		return TMEM_GENERAL_ERROR;
	}

	src_order = get_blk_order(free_buddy->size);
	if (src_order < 0) {
		pr_err("%s:%d invalid block order\n", __func__, __LINE__);
		return TMEM_GENERAL_ERROR;
	}

	src_buddy = free_buddy;
	pr_debug("search order from %d to %d, start sz:0x%x\n", src_order,
		 MAX_ORDER_NUM - 1, free_buddy->size);

	/* try merge from src order to target order */
	for (area_num = src_order; area_num < MAX_ORDER_NUM; area_num++) {
		src_blk_num = get_blk_number(memmgr_ctrl_info.pool_pa_start,
					     src_buddy->phy_addr,
					     MIN_BUDDY_BLOCK_SIZE);

		/* find out target buddy which can be merged */
		target_size = ORDER_SIZE(area_num) * 2;
		target_pa =
			get_merged_buddy_pa(src_buddy->phy_addr, target_size);
		target_blk_num =
			get_blk_number(memmgr_ctrl_info.pool_pa_start,
				       target_pa, MIN_BUDDY_BLOCK_SIZE);
		target_buddy = &memmgr_ctrl_info.blocks[target_blk_num];

		pr_debug("area: %d, merge sz: 0x%x, merge blk:%d\n", area_num,
			 target_size, target_blk_num);

		/* we only merge with buddy in area */
		if (target_buddy->status != BLK_FREE) {
			pr_debug(
				"merge ended at non-free blk:%d, area:%d, st:%s\n",
				target_blk_num, area_num,
				block_state_str[target_buddy->status].str);
			break;
		}

		/* we only merge with the same size buddy */
		if (target_buddy->size != (target_size / 2)) {
			pr_debug(
				"merge ended at non-match size blk:%d, area:%d, sz:0x%x\n",
				target_blk_num, area_num, target_buddy->size);
			break;
		}

		/* unlink target buddy from area */
		remove_buddy_from_area(target_buddy, area_num);

		/* merge source buddy with target buddy */
		if (is_left_side_buddy(src_buddy->phy_addr, target_size)) {
			src_buddy->ref_cnt = 0;
			src_buddy->size = target_size;
			src_buddy->status = BLK_FREE;
			target_buddy->ref_cnt = 0;
			target_buddy->size = 0;
			target_buddy->status = BLK_INIT;
			pr_debug(
				"merge [L]src blk:%d, pa:0x%llx, sz: 0x%x (%d)\n",
				src_blk_num, src_buddy->phy_addr, target_size,
				area_num);
			pr_debug("merge [R]target blk:%d, pa:0x%llx\n",
				 target_blk_num, target_buddy->phy_addr);
		} else {
			target_buddy->ref_cnt = 0;
			target_buddy->size = target_size;
			target_buddy->status = BLK_FREE;
			src_buddy->ref_cnt = 0;
			src_buddy->size = 0;
			src_buddy->status = BLK_INIT;
			pr_debug(
				"merge [L]target blk:%d, pa:0x%llx, sz: 0x%x (%d)\n",
				target_blk_num, target_buddy->phy_addr,
				target_size, area_num);
			pr_debug("merge [R]src blk:%d, pa:0x%llx\n",
				 src_blk_num, src_buddy->phy_addr);

			/* target is now changed to src for next round merge */
			src_buddy = target_buddy;
		}

		if (area_num == (MAX_ORDER_NUM - 1)) {
			pr_debug("merge ended at area end, area:%d\n",
				 area_num);
		}
	}

	/* update source buddy info. in case if no merge happened */
	src_buddy->ref_cnt = 0;
	src_buddy->status = BLK_FREE;

	/* insert source buddy to area */
	target_merge_in_area = get_blk_order(src_buddy->size);
	if (target_merge_in_area < 0) {
		pr_err("%s:%d invalid block order\n", __func__, __LINE__);
		return TMEM_GENERAL_ERROR;
	}

	add_buddy_to_area(src_buddy, target_merge_in_area);
	return TMEM_OK;
}

/* lock must be hold before entering */
static void memmgr_dump(void)
{
	int blk_num;
	int blk_state;

	pr_debug(
		"============================================================\n");
	pr_debug("* buddy blocks status dump:\n");

	for (blk_num = 0; blk_num < CONTROL_BLOCK_COUNT; blk_num++) {
		if (memmgr_ctrl_info.blocks[blk_num].size != 0) {
			blk_state = memmgr_ctrl_info.blocks[blk_num].status;
			pr_debug(
				"  blk[%d] pa: 0x%llx, sz: 0x%x, ref: %d, st: %s\n",
				blk_num,
				memmgr_ctrl_info.blocks[blk_num].phy_addr,
				memmgr_ctrl_info.blocks[blk_num].size,
				memmgr_ctrl_info.blocks[blk_num].ref_cnt,
				block_state_str[blk_state].str);
		}
	}

	pr_debug("* used blocks status dump:\n");
	for (blk_num = 0; blk_num < CONTROL_BLOCK_COUNT; blk_num++) {
		if (memmgr_ctrl_info.blocks[blk_num].status == BLK_USED) {
			blk_state = memmgr_ctrl_info.blocks[blk_num].status;
			pr_debug(
				"  blk[%d] pa: 0x%llx, sz: 0x%x, ref: %d, st: %s\n",
				blk_num,
				memmgr_ctrl_info.blocks[blk_num].phy_addr,
				memmgr_ctrl_info.blocks[blk_num].size,
				memmgr_ctrl_info.blocks[blk_num].ref_cnt,
				block_state_str[blk_state].str);
		}
	}

	pr_debug("* used sz: 0x%x, free sz: 0x%x\n", memmgr_ctrl_info.used_size,
		 memmgr_ctrl_info.free_size);
	pr_debug(
		"============================================================\n");
}

int memmgr_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		 u32 clean)
{
	int area_num;
	int blk_num;
	int adjust_size;
	struct Block *unlink_buddy;

	pr_debug("alloc sz: 0x%x, align: 0x%x, clean: %d\n", size, alignment,
		 clean);

	if (!is_region_ready()) {
		pr_err("memmgr region is still not ready!\n");
		return TMEM_GENERAL_ERROR;
	}

	/* check requested size */
	if (size > ORDER_SIZE(MAX_ORDER_NUM)) {
		pr_err("wrong requested size: 0x%x\n", size);
		return TMEM_GENERAL_ERROR;
	}

	/* adjust size */
	adjust_size = get_ordered_size(size);
	if (adjust_size != size) {
		pr_debug("change size from 0x%x to 0x%x\n", size, adjust_size);
		size = adjust_size;
	}

	/* adjust alignment */
	if ((alignment == 0) || (alignment > size)) {
		pr_debug("change alignment from 0x%x to 0x%x\n", alignment,
			 size);
		alignment = size;
	}

	/* invalid alignment check */
	if (alignment < size) {
		pr_err("wrong requested alignment: 0x%x, sz:0x%x\n", alignment,
		       size);
		return TMEM_GENERAL_ERROR;
	}

	mutex_lock(&memmgr_ctrl_info.memmgr_lock);

	/* find first valid buddy */
	area_num = find_first_satified_area(size);
	if ((memmgr_ctrl_info.free_size < size) || (area_num < 0)) {
#if MEMMGR_OUT_OF_MEMORY_HIT_ENABLE
		pr_err("out of memory, remained: 0x%x, used: 0x%x (%d)\n",
		       memmgr_ctrl_info.free_size, memmgr_ctrl_info.used_size,
		       area_num);
#endif
		goto ma_alloc_failed;
	}

	unlink_buddy = get_buddy_removed_by_area(area_num);
	if (INVALID(unlink_buddy)) {
		pr_err("system error in areas: %d!\n", area_num);
		goto ma_alloc_failed;
	}

	if (unlink_buddy->status != BLK_FREE) {
		pr_err("system error in blk state: %s!\n",
		       block_state_str[unlink_buddy->status].str);
		goto ma_alloc_failed;
	}

	blk_num = get_blk_number(memmgr_ctrl_info.pool_pa_start,
				 unlink_buddy->phy_addr, MIN_BUDDY_BLOCK_SIZE);
	pr_debug("unlink blk[%d] pa: 0x%llx, sz: 0x%x, ref: %d, st: %s\n",
		 blk_num, unlink_buddy->phy_addr, unlink_buddy->size,
		 unlink_buddy->ref_cnt,
		 block_state_str[unlink_buddy->status].str);

	if (unlink_buddy->size < size) {
		pr_err("invalid buddy found, area:%d!\n", area_num);
		goto ma_alloc_failed;
	}

	/* try split buddy if required */
	if (unlink_buddy->size > size) {
		if (try_buddy_split(unlink_buddy, size) == TMEM_GENERAL_ERROR) {
			pr_err("split buddy failed!\n");
			goto ma_alloc_failed;
		}
	}

	/* update buddy block information */
	unlink_buddy->ref_cnt++;
	unlink_buddy->status = BLK_USED;
	memmgr_ctrl_info.used_size += size;
	memmgr_ctrl_info.free_size -= size;

	/* update return information */
	*refcount = unlink_buddy->ref_cnt;
	*sec_handle = PA_TO_HANDLE(unlink_buddy->phy_addr);

	mutex_unlock(&memmgr_ctrl_info.memmgr_lock);
	return TMEM_OK;

ma_alloc_failed:
	memmgr_dump();
	mutex_unlock(&memmgr_ctrl_info.memmgr_lock);
	return TMEM_GENERAL_ERROR;
}

int memmgr_free(u32 sec_handle)
{
	int blk_num;
	struct Block *reclaim_buddy;
	u32 reclaim_size;
	u64 pa;

	if (!is_region_ready()) {
		pr_err("memmgr region is still not ready!\n");
		return TMEM_GENERAL_ERROR;
	}

	mutex_lock(&memmgr_ctrl_info.memmgr_lock);

	/* check if handle is valid */
	pa = HANDLE_TO_PA(sec_handle);
	if (!is_valid_pa_range(pa, memmgr_ctrl_info.pool_pa_start,
			       memmgr_ctrl_info.pool_size)) {
		pr_err("invalid pa addr: 0x%llx\n", pa);
		goto mr_free_failed;
	}

	/* validate block status */
	blk_num = get_blk_number(memmgr_ctrl_info.pool_pa_start, pa,
				 MIN_BUDDY_BLOCK_SIZE);
	pr_debug("try free handle: 0x%x, pa: 0x%llx, blk: %d\n", sec_handle, pa,
		 blk_num);

	reclaim_buddy = &memmgr_ctrl_info.blocks[blk_num];
	if (reclaim_buddy->status != BLK_USED) {
		pr_err("invalid blk state: %s, num: %d\n",
		       block_state_str[reclaim_buddy->status].str, blk_num);
		goto mr_free_failed;
	}

	/* try merge if required */
	reclaim_size = reclaim_buddy->size;
	if (try_buddy_merge(reclaim_buddy) == TMEM_GENERAL_ERROR) {
		pr_err("merge buddy failed!\n");
		goto mr_free_failed;
	}

	/* update buddy block information */
	memmgr_ctrl_info.used_size -= reclaim_size;
	memmgr_ctrl_info.free_size += reclaim_size;

	mutex_unlock(&memmgr_ctrl_info.memmgr_lock);
	return TMEM_OK;

mr_free_failed:
	memmgr_dump();
	mutex_unlock(&memmgr_ctrl_info.memmgr_lock);
	return TMEM_GENERAL_ERROR;
}

static void memmgr_reset_blocks_info(void)
{
	memset(&memmgr_ctrl_info, 0x0, sizeof(struct memmgr_control_info));
	mutex_init(&memmgr_ctrl_info.memmgr_lock);
}

static int memmgr_region_checks(u64 pa, u32 size)
{
	COMPILE_ASSERT(PMEM_PHYS_LIMIT_MIN_ALLOC_SIZE >= MIN_BUDDY_BLOCK_SIZE);

	if (size < MIN_BUDDY_BLOCK_SIZE) {
		pr_err("region size is invalid: 0x%x\n", size);
		return TMEM_GENERAL_ERROR;
	}

	if (size > MEMMGR_BUDDY_POOL_SIZE) {
		pr_err("buddy expects size < 0x%x, but input 0x%x\n",
		       MEMMGR_BUDDY_POOL_SIZE, size);
		return TMEM_GENERAL_ERROR;
	}

	if ((pa % ORDER_SIZE(MAX_ORDER_NUM)) != 0) {
		pr_err("pa 0x%llx is not aligned to max order size 0x%x\n", pa,
		       ORDER_SIZE(MAX_ORDER_NUM));
		return TMEM_GENERAL_ERROR;
	}

#if FORCE_PA_ALIGH_WITH_SIZE
	if ((pa % size) != 0) {
		pr_err("pa 0x%llx is not aligned to its size 0x%x\n", pa, size);
		return TMEM_GENERAL_ERROR;
	}
#endif

	return TMEM_OK;
}

int memmgr_add_region(u64 pa, u32 size)
{
	int blk_num, area_num;
	int remained_size, chunk_size;
	u64 next_pa;
	struct Block *ref_buddy;

	if (memmgr_region_checks(pa, size))
		return TMEM_GENERAL_ERROR;

	memmgr_reset_blocks_info();
	mutex_lock(&memmgr_ctrl_info.memmgr_lock);

	memmgr_ctrl_info.pool_pa_start = pa;
	memmgr_ctrl_info.pool_size = size;
	memmgr_ctrl_info.free_size = size;
	memmgr_ctrl_info.used_size = 0;
	memmgr_ctrl_info.managed_blocks = size / MIN_BUDDY_BLOCK_SIZE;

	/* init blocks */
	for (blk_num = 0; blk_num < memmgr_ctrl_info.managed_blocks;
	     blk_num++) {
		memmgr_ctrl_info.blocks[blk_num].phy_addr =
			memmgr_ctrl_info.pool_pa_start
			+ (MIN_BUDDY_BLOCK_SIZE * blk_num);
		memmgr_ctrl_info.blocks[blk_num].size = 0;
		memmgr_ctrl_info.blocks[blk_num].ref_cnt = 0;
		memmgr_ctrl_info.blocks[blk_num].status = BLK_INIT;
		INIT_LIST_HEAD(&memmgr_ctrl_info.blocks[blk_num].list);
	}

	/* init area */
	for (area_num = 0; area_num <= MAX_ORDER_NUM; area_num++) {
		memmgr_ctrl_info.areas[area_num].phy_addr = 0ULL;
		memmgr_ctrl_info.areas[area_num].size = ORDER_SIZE(area_num);
		memmgr_ctrl_info.areas[area_num].ref_cnt = 0;
		memmgr_ctrl_info.areas[area_num].status = BLK_AREA_HEAD;
		INIT_LIST_HEAD(&memmgr_ctrl_info.areas[area_num].list);
	}

	/* dispatch buddy to area, and we assume pa is aligned to pool size */
	remained_size = memmgr_ctrl_info.pool_size;
	next_pa = memmgr_ctrl_info.pool_pa_start;

	pr_debug("dispatch start pa:0x%llx, remained:0x%x\n", next_pa,
		 remained_size);
	for (area_num = MAX_ORDER_NUM; area_num >= 0; area_num--) {
		chunk_size = memmgr_ctrl_info.areas[area_num].size;
		while ((remained_size - chunk_size) >= 0) {
			blk_num = get_blk_number(memmgr_ctrl_info.pool_pa_start,
						 next_pa, MIN_BUDDY_BLOCK_SIZE);
			ref_buddy = &memmgr_ctrl_info.blocks[blk_num];
			pr_debug(
				"area %d (chunk:0x%x), pa:0x%llx, blk_num:%d\n",
				area_num, chunk_size, next_pa, blk_num);

			if (ref_buddy->phy_addr != next_pa) {
				pr_debug(
					"dispatch error, blk pa (0x%llx) != next pa (0x%llx)\n",
					ref_buddy->phy_addr, next_pa);
				mutex_unlock(&memmgr_ctrl_info.memmgr_lock);
				return TMEM_GENERAL_ERROR;
			}
			ref_buddy->size = chunk_size;
			ref_buddy->status = BLK_FREE;

			/* insert buddy to area */
			add_buddy_to_area(ref_buddy, area_num);

			remained_size -= chunk_size;
			next_pa += chunk_size;
			pr_debug("remained size: 0x%x, next pa:0x%llx\n",
				 remained_size, next_pa);
		}
	}

#if MEMMGR_AREA_BLK_DEBUG_ENABLE
	/* save blk state for debug */
	for (blk_num = 0; blk_num < memmgr_ctrl_info.managed_blocks;
	     blk_num++) {
		memmgr_ctrl_info.blk_init_state_dbg[blk_num] =
			memmgr_ctrl_info.blocks[blk_num].status;
	}
#endif

	mutex_unlock(&memmgr_ctrl_info.memmgr_lock);
	set_region_ready(true);
	return TMEM_OK;
}

int memmgr_remove_region(void)
{
	if (memmgr_ctrl_info.used_size != 0) {
		pr_err("memory is still occupied, used: 0x%x\n",
		       memmgr_ctrl_info.used_size);
		return TMEM_GENERAL_ERROR;
	}

	memmgr_reset_blocks_info();
	set_region_ready(false);
	return TMEM_OK;
}

#ifdef TCORE_UT_FWK_SUPPORT
struct UT_Handle {
	u32 handle;
	struct list_head list;
};
static struct UT_Handle g_stress_handle_list;

static struct UT_Handle *get_ut_handle_removed(void)
{
	struct UT_Handle *ut_handle = NULL, *tmp;

	if (list_empty(&g_stress_handle_list.list)) {
		pr_debug("no ut handle is found\n");
		return NULL;
	}

	list_for_each_entry_safe(ut_handle, tmp, &g_stress_handle_list.list,
				 list) {
		list_del(&ut_handle->list);
		/* get the first entry */
		break;
	}

	return ut_handle;
}

static u32 *g_ut_handle_list;
static enum UT_RET_STATE memmgr_ut_init(struct ut_params *params)
{
	if (INVALID(g_ut_handle_list)) {
		g_ut_handle_list = mld_kmalloc(
			sizeof(u32) * CONTROL_BLOCK_COUNT, GFP_KERNEL);
	}
	ASSERT_NOTNULL(g_ut_handle_list, "alloc ut memory for handles");

	INIT_LIST_HEAD(&g_stress_handle_list.list);
	ASSERT_TRUE(list_empty(&g_stress_handle_list.list),
		    "init handle list head");
	return UT_STATE_PASS;
}

static enum UT_RET_STATE memmgr_ut_deinit(struct ut_params *params)
{
	struct UT_Handle *handle, *tmp;

	UNUSED(params);

	list_for_each_entry_safe(handle, tmp, &g_stress_handle_list.list,
							 list) {
		list_del(&handle->list);
		mld_kfree(handle);
	}

	if (VALID(g_ut_handle_list)) {
		mld_kfree(g_ut_handle_list);
		g_ut_handle_list = NULL;
	}

	return UT_STATE_PASS;
}

/* halt check is required after calling this API */
static enum UT_RET_STATE memmgr_ut_add_region(void)
{
	ASSERT_FALSE(is_region_ready(), "check region existence");
	ASSERT_EQ(0, memmgr_add_region(MEMMGR_UT_PA_ADDR64_START,
				       MEMMGR_BUDDY_POOL_SIZE),
		  "add region for mgr");
	ASSERT_TRUE(is_region_ready(), "check region existence");

	ASSERT_EQ64(MEMMGR_UT_PA_ADDR64_START, memmgr_ctrl_info.pool_pa_start,
		    "check pool pa start");
	ASSERT_EQ(MEMMGR_BUDDY_POOL_SIZE, memmgr_ctrl_info.pool_size,
		  "check pool size");
	ASSERT_EQ(0, memmgr_ctrl_info.used_size, "check used size");
	ASSERT_EQ(MEMMGR_BUDDY_POOL_SIZE, memmgr_ctrl_info.free_size,
		  "check free size");
	ASSERT_EQ((MEMMGR_BUDDY_POOL_SIZE / MIN_BUDDY_BLOCK_SIZE),
		  memmgr_ctrl_info.managed_blocks, "check managed blocks");
	return UT_STATE_PASS;
}

/* halt check is required after calling this API */
static enum UT_RET_STATE memmgr_ut_remove_region(void)
{
	ASSERT_EQ(0, memmgr_remove_region(), "remove region from mgr");
	ASSERT_FALSE(is_region_ready(), "check region existence");
	ASSERT_EQ64(0ULL, memmgr_ctrl_info.pool_pa_start,
		    "check pool pa start");
	ASSERT_EQ(0, memmgr_ctrl_info.pool_size, "check pool size");
	ASSERT_EQ(0, memmgr_ctrl_info.used_size, "check used size");
	ASSERT_EQ(0, memmgr_ctrl_info.free_size, "check free size");
	ASSERT_EQ(0, memmgr_ctrl_info.managed_blocks, "check managed blocks");
	return UT_STATE_PASS;
}

/* halt check is required after calling this API */
static enum UT_RET_STATE memmgr_ut_area_state_test(void)
{
	int remained_sz_for_area;
	int area_num;

	remained_sz_for_area = memmgr_ctrl_info.pool_size;
	for (area_num = MAX_ORDER_NUM; area_num >= 0; area_num--) {
		ASSERT_EQ(ORDER_SIZE(area_num),
			  memmgr_ctrl_info.areas[area_num].size,
			  "check area managed size");
		ASSERT_EQ(BLK_AREA_HEAD,
			  memmgr_ctrl_info.areas[area_num].status,
			  "check area status");
		if (remained_sz_for_area / ORDER_SIZE(area_num)) {
			ASSERT_EQ(1, memmgr_ctrl_info.free_areas[area_num],
				  "check free areas expect exist");
			ASSERT_FALSE(
				list_empty(&(
					memmgr_ctrl_info.areas[area_num].list)),
				"check areas list");
		} else {
			ASSERT_EQ(0, memmgr_ctrl_info.free_areas[area_num],
				  "check free areas expect not exist");
			ASSERT_TRUE(
				list_empty(&(
					memmgr_ctrl_info.areas[area_num].list)),
				"check areas list");
		}
		remained_sz_for_area =
			remained_sz_for_area % ORDER_SIZE(area_num);
	}

#if MEMMGR_AREA_BLK_DEBUG_ENABLE
	{
		int blk_num;

		for (blk_num = 0; blk_num < memmgr_ctrl_info.managed_blocks;
		     blk_num++) {
			ASSERT_EQ(memmgr_ctrl_info.blk_init_state_dbg[blk_num],
				  memmgr_ctrl_info.blocks[blk_num].status,
				  "check blk state");
		}
	}
#endif
	return UT_STATE_PASS;
}

static enum UT_RET_STATE memmgr_ut_api_test(struct ut_params *params)
{
	UNUSED(params);
	BEGIN_UT_TEST;

/* handle/PA convert test (need to use fixed pattern) */
#if (PMEM_64BIT_PHYS_SHIFT == 10)
	EXPECT_EQ(0x60001F, PA_TO_HANDLE(0x180007C00), "pa to handle");
	EXPECT_EQ(0x60001E, PA_TO_HANDLE(0x180007800), "pa to handle");
	EXPECT_EQ64(0x180007C00ULL, HANDLE_TO_PA(0x60001F), "handle to pa");
	EXPECT_EQ64(0x180007800ULL, HANDLE_TO_PA(0x60001E), "handle to pa");
#else
#error("please define new test cases for pa/handle conversion!")
#endif

/* order api check */
#if (MAX_ORDER_NUM == 14)
	EXPECT_EQ(0, get_blk_order(SIZE_1K), "check order 0");
	EXPECT_EQ(1, get_blk_order(SIZE_2K), "check order 1");
	EXPECT_EQ(2, get_blk_order(SIZE_4K), "check order 2");
	EXPECT_EQ(3, get_blk_order(SIZE_8K), "check order 3");
	EXPECT_EQ(4, get_blk_order(SIZE_16K), "check order 4");
	EXPECT_EQ(5, get_blk_order(SIZE_32K), "check order 5");
	EXPECT_EQ(6, get_blk_order(SIZE_64K), "check order 6");
	EXPECT_EQ(7, get_blk_order(SIZE_128K), "check order 7");
	EXPECT_EQ(8, get_blk_order(SIZE_256K), "check order 8");
	EXPECT_EQ(9, get_blk_order(SIZE_512K), "check order 9");
	EXPECT_EQ(10, get_blk_order(SIZE_1M), "check order 10");
	EXPECT_EQ(11, get_blk_order(SIZE_2M), "check order 11");
	EXPECT_EQ(12, get_blk_order(SIZE_4M), "check order 12");
	EXPECT_EQ(13, get_blk_order(SIZE_8M), "check order 13");
	EXPECT_EQ(14, get_blk_order(SIZE_16M), "check order 14");
#else
#error("please check supported max order configuration!")
#endif
	EXPECT_EQ(-1, get_blk_order(SIZE_32M), "check order 15");

	/* block number checks */
	EXPECT_EQ(0, get_blk_number(0x800000ULL, 0x800000ULL, SIZE_4K),
		  "check blk number");
	EXPECT_EQ(0, get_blk_number(0x800000ULL, 0x800FFFULL, SIZE_4K),
		  "check blk number");
	EXPECT_EQ(1, get_blk_number(0x800000ULL, 0x801000ULL, SIZE_4K),
		  "check blk number");
	EXPECT_EQ(1, get_blk_number(0x800000ULL, 0x801FFFULL, SIZE_4K),
		  "check blk number");

	/* side of buddy checks */
	EXPECT_TRUE(is_left_side_buddy(0x800000ULL, 0x2000),
		    "buddy side check");
	EXPECT_FALSE(is_left_side_buddy(0x801000ULL, 0x2000),
		     "buddy side check");
	EXPECT_TRUE(is_left_side_buddy(0x804000ULL, 0x4000),
		    "buddy side check");
	EXPECT_FALSE(is_left_side_buddy(0x806000ULL, 0x4000),
		     "buddy side check");
	EXPECT_EQ64(0x801000ULL, get_merged_buddy_pa(0x800000ULL, 0x2000),
		    "buddy side pa check");
	EXPECT_EQ64(0x800000ULL, get_merged_buddy_pa(0x801000ULL, 0x2000),
		    "buddy side pa check");
	EXPECT_EQ64(0x806000ULL, get_merged_buddy_pa(0x804000ULL, 0x4000),
		    "buddy side pa check");
	EXPECT_EQ64(0x804000ULL, get_merged_buddy_pa(0x806000ULL, 0x4000),
		    "buddy side pa check");

	/* get order size */
	EXPECT_EQ(SIZE_1K, get_ordered_size(SIZE_512B), "check small size");
	EXPECT_EQ(SIZE_1K, get_ordered_size(SIZE_1K), "check order size");
	EXPECT_EQ(SIZE_2K, get_ordered_size(SIZE_1K + SIZE_512B),
		  "check order size");
	EXPECT_EQ(SIZE_4K, get_ordered_size(SIZE_2K + SIZE_512B),
		  "check order size");

	/* region checks */
	EXPECT_NE(0, memmgr_region_checks(MEMMGR_UT_PA_ADDR64_START, SIZE_512B),
		  "region minimal size check");
	EXPECT_NE(0, memmgr_region_checks(MEMMGR_UT_PA_ADDR64_START,
					  MEMMGR_BUDDY_POOL_SIZE + SIZE_1M),
		  "region maximun size check");
	EXPECT_NE(0, memmgr_region_checks(MEMMGR_UT_PA_ADDR64_START
						  + (MEMMGR_BUDDY_POOL_SIZE / 2)
						  + SIZE_1K,
					  MEMMGR_BUDDY_POOL_SIZE),
		  "region pa alignment check");

#if FORCE_PA_ALIGH_WITH_SIZE
	EXPECT_NE(0, memmgr_region_checks(MEMMGR_UT_PA_ADDR64_START
						  + MEMMGR_BUDDY_POOL_SIZE / 2,
					  MEMMGR_BUDDY_POOL_SIZE),
		  "region pa alignment check");
#else
	EXPECT_EQ(0, memmgr_region_checks(MEMMGR_UT_PA_ADDR64_START
						  + MEMMGR_BUDDY_POOL_SIZE / 2,
					  MEMMGR_BUDDY_POOL_SIZE),
		  "region pa alignment check");
#endif

	END_UT_TEST;
}

static enum UT_RET_STATE memmgr_ut_region_test(struct ut_params *params)
{
	struct Block *out_bound_blk;
	struct Block *in_bound_first_blk, *in_bound_last_blk;
	u64 in_bound_last_blk_pa;

	UNUSED(params);
	BEGIN_UT_TEST;

	/* add region */
	ASSERT_EQ(0, memmgr_ut_add_region(), "add region");

	in_bound_first_blk = &memmgr_ctrl_info.blocks[0];
	EXPECT_EQ64(MEMMGR_UT_PA_ADDR64_START, in_bound_first_blk->phy_addr,
		    "check in bound first blk pa");
	EXPECT_EQ(ORDER_SIZE(MAX_ORDER_NUM), in_bound_first_blk->size,
		  "check in bound first blk size");
	EXPECT_EQ(BLK_FREE, in_bound_first_blk->status,
		  "check in bound first blk status");
	EXPECT_EQ(0, in_bound_first_blk->ref_cnt,
		  "check in bound first blk ref cnt");

	in_bound_last_blk =
		&memmgr_ctrl_info.blocks[memmgr_ctrl_info.managed_blocks - 1];
	in_bound_last_blk_pa =
		(MEMMGR_UT_PA_ADDR64_START + MEMMGR_BUDDY_POOL_SIZE)
		- MIN_BUDDY_BLOCK_SIZE;
	EXPECT_EQ64(in_bound_last_blk_pa, in_bound_last_blk->phy_addr,
		    "check in bound last blk pa");
	EXPECT_EQ(0, in_bound_last_blk->size, "check in bound last blk size");
	EXPECT_EQ(BLK_INIT, in_bound_last_blk->status,
		  "check in bound last blk status");
	EXPECT_EQ(0, in_bound_last_blk->ref_cnt,
		  "check in bound last blk ref cnt");

	if (memmgr_ctrl_info.managed_blocks < CONTROL_BLOCK_COUNT) {
		out_bound_blk =
			&memmgr_ctrl_info
				 .blocks[memmgr_ctrl_info.managed_blocks];
		EXPECT_EQ64(0ULL, out_bound_blk->phy_addr,
			    "check out bound blk pa");
		EXPECT_EQ(0, out_bound_blk->size, "check out bound blk size");
		EXPECT_EQ(BLK_BAD, out_bound_blk->status,
			  "check out bound blk status");
		EXPECT_EQ(0, out_bound_blk->ref_cnt,
			  "check out blk block ref cnt");
	}

	/* make sure areas is recovery correctly */
	ASSERT_EQ(0, memmgr_ut_area_state_test(), "area checks");

	/* remove region */
	ASSERT_EQ(0, memmgr_ut_remove_region(), "remove region");

	END_UT_TEST;
}

static enum UT_RET_STATE memmgr_ut_alloc_simple_test(struct ut_params *params)
{
	int ret;
	int area_num;
	u32 alignment, chunk_size, handle, ref_count;
	u32 free_size, used_size;

	UNUSED(params);
	BEGIN_UT_TEST;

	/* add region */
	ASSERT_EQ(0, memmgr_ut_add_region(), "add region");

	/* alloc alignment check */
	ret = memmgr_alloc(SIZE_512B, SIZE_1K, &ref_count, &handle, 0);
	ASSERT_NE(0, ret, "wrong alignment check");

	/* out of memory check */
	ret = memmgr_alloc(0, MEMMGR_BUDDY_POOL_SIZE * 2, &ref_count, &handle,
			   0);
	ASSERT_NE(0, ret, "out of memory check");

	/* alloc one and free one */
	for (area_num = 0; area_num <= MAX_ORDER_NUM; area_num++) {
		chunk_size = ORDER_SIZE(area_num);
		alignment = chunk_size;
		free_size = memmgr_ctrl_info.free_size;
		used_size = memmgr_ctrl_info.used_size;

		ret = memmgr_alloc(alignment, chunk_size, &ref_count, &handle,
				   0);
		ASSERT_EQ(0, ret, "alloc buddy memory");
		ASSERT_EQ(1, ref_count, "reference count check");
		ASSERT_NE(0, handle, "handle check");
		free_size -= chunk_size;
		used_size += chunk_size;
		ASSERT_EQ(free_size, memmgr_ctrl_info.free_size,
			  "free size check");
		ASSERT_EQ(used_size, memmgr_ctrl_info.used_size,
			  "used size check");

		ret = memmgr_free(handle);
		ASSERT_EQ(0, ret, "free buddy memory");
		free_size += chunk_size;
		used_size -= chunk_size;
		ASSERT_EQ(free_size, memmgr_ctrl_info.free_size,
			  "free size check");
		ASSERT_EQ(used_size, memmgr_ctrl_info.used_size,
			  "used size check");

		/* make sure areas is recovery correctly */
		ASSERT_EQ(0, memmgr_ut_area_state_test(), "area checks");
	}

	/* remove region */
	ASSERT_EQ(0, memmgr_ut_remove_region(), "remove region");

	END_UT_TEST;
}

static enum UT_RET_STATE
memmgr_ut_alloc_saturation_test(struct ut_params *params)
{
	int ret;
	int area_num, chunk_num;
	u32 alignment, chunk_size, ref_count;
	u32 one_more_handle;
	int round = params->param1;
	u32 free_size, used_size;
	int max_items;

	BEGIN_UT_TEST;

	/* add region */
	ASSERT_EQ(0, memmgr_ut_add_region(), "add region");

	while (round-- > 0) {
		/* alloc until maximum POOL size */
		for (area_num = 0; area_num <= MAX_ORDER_NUM; area_num++) {
			chunk_size = ORDER_SIZE(area_num);
			max_items = (MEMMGR_BUDDY_POOL_SIZE / chunk_size);

			/* alloc until full */
			for (chunk_num = 0; chunk_num < max_items;
			     chunk_num++) {
				alignment = chunk_size;
				free_size = memmgr_ctrl_info.free_size;
				used_size = memmgr_ctrl_info.used_size;

				ret = memmgr_alloc(
					alignment, chunk_size, &ref_count,
					&g_ut_handle_list[chunk_num], 0);
				ASSERT_EQ(0, ret, "alloc buddy memory");
				ASSERT_EQ(1, ref_count,
					  "reference count check");
				ASSERT_NE(0, g_ut_handle_list[chunk_num],
					  "handle check");
				free_size -= chunk_size;
				used_size += chunk_size;
				ASSERT_EQ(free_size, memmgr_ctrl_info.free_size,
					  "free size check");
				ASSERT_EQ(used_size, memmgr_ctrl_info.used_size,
					  "used size check");
			}

			ASSERT_EQ(0, memmgr_ctrl_info.free_size,
				  "zero free size check");
			ASSERT_EQ(MEMMGR_BUDDY_POOL_SIZE,
				  memmgr_ctrl_info.used_size,
				  "full used size check");

			/* one more allocation (expect fail) */
			ret = memmgr_alloc(alignment, chunk_size, &ref_count,
					   &one_more_handle, 0);
			ASSERT_NE(0, ret, "alloc one more buddy memory");
			ASSERT_EQ(0, memmgr_ctrl_info.free_size,
				  "zero free size check");
			ASSERT_EQ(MEMMGR_BUDDY_POOL_SIZE,
				  memmgr_ctrl_info.used_size,
				  "full used size check");

			/* free all chunk */
			for (chunk_num = (max_items - 1); chunk_num >= 0;
			     chunk_num--) {
				ret = memmgr_free(g_ut_handle_list[chunk_num]);
				ASSERT_EQ(0, ret, "free buddy memory");
				free_size += chunk_size;
				used_size -= chunk_size;
				ASSERT_EQ(free_size, memmgr_ctrl_info.free_size,
					  "free size check");
				ASSERT_EQ(used_size, memmgr_ctrl_info.used_size,
					  "used size check");
			}

			/* make sure areas is recovery correctly */
			ASSERT_EQ(0, memmgr_ut_area_state_test(),
				  "area checks");
		}
	}

	/* remove region */
	ASSERT_EQ(0, memmgr_ut_remove_region(), "remove region");

	END_UT_TEST;
}


static enum UT_RET_STATE
memmgr_ut_alloc_random_size_test(struct ut_params *params)
{
	int ret;
	u32 rand_val;
	int chunk_order, chunk_size;
	u32 alignment, ref_count;
	int round = params->param1;
	int area_num;
	struct UT_Handle *ut_handle;
	int handle_cnt = 0;

	BEGIN_UT_TEST;

	/* add region */
	ASSERT_EQ(0, memmgr_ut_add_region(), "add region");

	while (round-- > 0) {
		/* generate chunk size */
		rand_val = (u32)get_random_int();
		chunk_order = rand_val % (MAX_ORDER_NUM + 1);
		ASSERT_GE(MAX_ORDER_NUM, chunk_order,
			  "random order range check");
		chunk_size = ORDER_SIZE(chunk_order);
		alignment = chunk_size;

		pr_debug("order: %d, sz: 0x%x, align: 0x%x, op: ALLOC\n",
			 chunk_order, chunk_size, alignment);

		ut_handle = mld_kmalloc(sizeof(struct UT_Handle), GFP_KERNEL);
		ASSERT_NOTNULL(ut_handle, "alloc ut handle for stress");
		INIT_LIST_HEAD(&ut_handle->list);

		/* get predict result */
		area_num = find_first_satified_area(chunk_size);

		ret = memmgr_alloc(alignment, chunk_size, &ref_count,
				   &ut_handle->handle, 0);

		if (area_num < 0) {
			ASSERT_NE(0, ret,
				  "alloc buddy memory expect fail check");
			mld_kfree(ut_handle);
		} else {
			ASSERT_EQ(0, ret,
				  "alloc buddy memory expect success check");
			ASSERT_EQ(1, ref_count, "reference count check");
			list_add_tail(&(ut_handle->list),
				      &g_stress_handle_list.list);
			pr_debug("got handle 0x%x, sz:0x%x\n",
				 ut_handle->handle, chunk_size);
			ASSERT_FALSE(list_empty(&g_stress_handle_list.list),
				     "handle list check");
			handle_cnt++;
		}
	}

	pr_debug("random alloc size test finished!\n");

	/* free all alloc memory */
	while (handle_cnt-- > 0) {
		ut_handle = get_ut_handle_removed();
		ASSERT_NOTNULL(ut_handle, "handle pointer check");
		pr_debug("free handle 0x%x (%d)\n", ut_handle->handle,
			 handle_cnt);
		ret = memmgr_free(ut_handle->handle);
		ASSERT_EQ(0, ret, "free buddy memory");
		mld_kfree(ut_handle);
	}

	/* make sure areas is recovery correctly */
	ASSERT_EQ(0, memmgr_ut_area_state_test(), "area checks");

	/* remove region */
	ASSERT_EQ(0, memmgr_ut_remove_region(), "remove region");

	END_UT_TEST;
}

static enum UT_RET_STATE
memmgr_ut_alloc_random_op_test(struct ut_params *params)
{
	int ret;
	u32 rand_val;
	int chunk_order, chunk_size;
	u32 alignment, ref_count;
	int is_alloc_op;
	int round = params->param1;
	int area_num;
	struct UT_Handle *ut_handle;

	BEGIN_UT_TEST;

	/* add region */
	ASSERT_EQ(0, memmgr_ut_add_region(), "add region");

	while (round-- > 0) {
		/* generate chunk size */
		rand_val = (u32)get_random_int();
		chunk_order = rand_val % (MAX_ORDER_NUM + 1);
		ASSERT_GE(MAX_ORDER_NUM, chunk_order,
			  "random order range check");
		chunk_size = ORDER_SIZE(chunk_order);
		alignment = chunk_size;

		/* generate operation to perform */
		rand_val = (u32)get_random_int();
		is_alloc_op = rand_val % 2;
		ASSERT_GE(1, is_alloc_op, "random operation range check");

		pr_debug("order: %d, sz: 0x%x, align: 0x%x, op: %s\n",
			 chunk_order, chunk_size, alignment,
			 is_alloc_op ? "ALLOC" : "FREE");

		/* perform operation */
		if (is_alloc_op) {
			ut_handle = mld_kmalloc(sizeof(struct UT_Handle),
						GFP_KERNEL);
			ASSERT_NOTNULL(ut_handle, "alloc ut handle for stress");
			INIT_LIST_HEAD(&ut_handle->list);

			/* get predict result */
			area_num = find_first_satified_area(chunk_size);

			ret = memmgr_alloc(alignment, chunk_size, &ref_count,
					   &ut_handle->handle, 0);

			if (area_num < 0) {
				ASSERT_NE(
					0, ret,
					"alloc buddy memory expect fail check");
				mld_kfree(ut_handle);
			} else {
				ASSERT_EQ(
					0, ret,
					"alloc buddy memory expect success check");
				ASSERT_EQ(1, ref_count,
					  "reference count check");
				list_add_tail(&(ut_handle->list),
					      &g_stress_handle_list.list);
				pr_debug("got handle 0x%x, sz:0x%x\n",
					 ut_handle->handle, chunk_size);
				ASSERT_FALSE(
					list_empty(&g_stress_handle_list.list),
					"handle list check");
			}
		} else {
			ut_handle = get_ut_handle_removed();
			if (VALID(ut_handle)) {
				pr_debug("free handle 0x%x\n",
					 ut_handle->handle);
				ret = memmgr_free(ut_handle->handle);
				ASSERT_EQ(0, ret, "free buddy memory");
				mld_kfree(ut_handle);
			} else {
				pr_debug("do nothing\n");
			}
		}
	}

	pr_debug("random op test finished!\n");

	/* free all alloc memory */
	while (NULL != (ut_handle = get_ut_handle_removed())) {
		pr_debug("free handle 0x%x\n", ut_handle->handle);
		ret = memmgr_free(ut_handle->handle);
		ASSERT_EQ(0, ret, "free buddy memory");
		mld_kfree(ut_handle);
	}

	/* make sure areas is recovery correctly */
	ASSERT_EQ(0, memmgr_ut_area_state_test(), "area checks");

	/* remove region */
	ASSERT_EQ(0, memmgr_ut_remove_region(), "remove region");

	END_UT_TEST;
}

#define MRMMGR_SATURATION_STREE_ROUND (5ULL)
#define MRMMGR_RANDOM_SIZE_ROUND (50000ULL)
#define MRMMGR_RANDOM_OP_ROUND (200000ULL)
static enum UT_RET_STATE memmgr_ut_run_all(struct ut_params *params)
{
	int ret;

	BEGIN_UT_TEST;

	ret = memmgr_ut_api_test(params);
	ASSERT_EQ(0, ret, "api test");

	ret = memmgr_ut_region_test(params);
	ASSERT_EQ(0, ret, "region test");

	ret = memmgr_ut_alloc_simple_test(params);
	ASSERT_EQ(0, ret, "alloc simple test");

	params->param1 = MRMMGR_SATURATION_STREE_ROUND;
	ret = memmgr_ut_alloc_saturation_test(params);
	ASSERT_EQ(0, ret, "alloc saturation test");

	params->param1 = MRMMGR_RANDOM_SIZE_ROUND;
	ret = memmgr_ut_alloc_random_size_test(params);
	ASSERT_EQ(0, ret, "alloc random size test");

	params->param1 = MRMMGR_RANDOM_OP_ROUND;
	ret = memmgr_ut_alloc_random_op_test(params);
	ASSERT_EQ(0, ret, "alloc random op test");

	END_UT_TEST;
}

BEGIN_TEST_SUITE(PMEM_UT_MEMMGR_BASE, PMEM_UT_MEMMGR_MAX, memmgr_ut_run,
		 memmgr_ut_init)
DEFINE_TEST_CASE(PMEM_UT_MEMMGR_API, memmgr_ut_api_test)
DEFINE_TEST_CASE(PMEM_UT_MEMMGR_REGION, memmgr_ut_region_test)
DEFINE_TEST_CASE(PMEM_UT_MEMMGR_SIMPLE_ALLOC, memmgr_ut_alloc_simple_test)
DEFINE_TEST_CASE_PARAM1(PMEM_UT_MEMMGR_SATURATION,
			memmgr_ut_alloc_saturation_test, 1)
DEFINE_TEST_CASE_PARAM1(PMEM_UT_MEMMGR_SATURATION_STREEE,
			memmgr_ut_alloc_saturation_test,
			MRMMGR_SATURATION_STREE_ROUND)
DEFINE_TEST_CASE_PARAM1(PMEM_UT_MEMMGR_RANDOM_SIZE,
			memmgr_ut_alloc_random_size_test,
			MRMMGR_SATURATION_STREE_ROUND)
DEFINE_TEST_CASE_PARAM1(PMEM_UT_MEMMGR_RANDOM_OPERATION,
			memmgr_ut_alloc_random_op_test, MRMMGR_RANDOM_OP_ROUND)
DEFINE_TEST_CASE(PMEM_UT_MEMMGR_ALL, memmgr_ut_run_all)
END_TEST_SUITE(memmgr_ut_deinit)
REGISTER_TEST_SUITE(PMEM_UT_MEMMGR_BASE, PMEM_UT_MEMMGR_MAX, memmgr_ut_run)
#endif
