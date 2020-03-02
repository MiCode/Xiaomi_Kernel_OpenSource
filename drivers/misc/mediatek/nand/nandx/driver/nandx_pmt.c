/*
 * Copyright (C) 2017 MediaTek Inc.
 * Licensed under either
 *     BSD Licence, (see NOTICE for more details)
 *     GNU General Public License, version 2.0, (see NOTICE for more details)
 */
#include "nandx_util.h"
#include "nandx_errno.h"
#include "nandx_info.h"
#include "nandx_core.h"
#include "nandx_ops.h"
#include "nandx_bmt.h"
#include "nandx_pmt.h"

static struct pmt_handler *handler;

struct pt_resident *nandx_pmt_get_partition(u64 addr)
{
	struct pt_resident *pt = handler->pmt;
	u32 i;

	if (handler->pmt == NULL) {
		pr_err("no valid pmt table!!\n");
		return NULL;
	}

	for (i = 0; i < handler->part_num; i++) {
		if (pt->offset > addr)
			return (struct pt_resident *)(pt - 1);
		pt++;
	}

	return NULL;
}

static int nandx_read_pmt_table(struct nandx_chip_info *info,
				struct nandx_ops *ops)
{
	int ret;
	u8 *buf = ops->data;
	u32 temp;

	ret = nandx_core_read(ops, 1, MODE_SLC);
	if ((ret < 0) && (ret != -ENANDFLIPS))
		return ret;

	if (!is_valid_pt(buf) && !is_valid_mpt(buf))
		return 0;

	temp = *((u32 *)buf + 1);
	handler->sys_slc_ratio = (temp >> 16) & 0xFF;
	handler->usr_slc_ratio = (temp) & 0xFF;
	temp = /*PT_SIG_SIZE */ 8;
	memcpy(handler->pmt, buf + temp,
	       PART_MAX_COUNT * sizeof(struct pt_resident));
	temp += PART_MAX_COUNT * sizeof(struct pt_resident);
	memcpy(handler->block_bitmap, buf + temp,
	       div_up(info->block_num, 32) * sizeof(u32));

	return 1;
}

int nandx_pmt_init(struct nandx_chip_info *info, u32 start_blk)
{
	struct pt_resident *pt;
	struct nandx_ops ops;
	u32 ppb, sppb;
	u32 i, temp;
	u8 *buf;
	u32 *block_bitmap;
	bool pmt_found = false;
	int ret;

	handler = mem_alloc(1, sizeof(struct pmt_handler));
	if (handler == NULL)
		return -ENOMEM;

	pt = mem_alloc(PART_MAX_COUNT, sizeof(struct pt_resident));
	if (pt == NULL) {
		ret = -ENOMEM;
		goto freehandler;
	}
	memset(pt, 0, PART_MAX_COUNT * sizeof(struct pt_resident));
	/* restore pt to do pmt release later */
	handler->pmt = pt;

	ppb = info->block_size / info->page_size;
	sppb = ppb / info->wl_page_num;

	temp = div_up(info->block_num, 32);
	block_bitmap = mem_alloc(temp, sizeof(u32));
	if (block_bitmap == NULL) {
		ret = -ENOMEM;
		goto freept;
	}
	memset(block_bitmap, 0xff, temp * sizeof(u32));
	handler->block_bitmap = block_bitmap;

	buf = mem_alloc(1, info->page_size);
	if (buf == NULL) {
		ret = -ENOMEM;
		goto freeblock;
	}

	/* setup pmt_handler */
	handler->info = info;
	handler->start_blk = start_blk;

	nandx_get_device(FL_READING);

	/* go to find valid pmt */
	ops.col = 0;
	ops.len = info->page_size;
	ops.data = buf;
	ops.oob = NULL;
	ops.row = nandx_bmt_get_mapped_block(start_blk);
	ops.row *= ppb;

	for (i = 0; i < sppb; i++) {
		ret = nandx_read_pmt_table(info, &ops);
		if (ret < 0) {
			ops.row++;
			continue;
		}

		if (ret > 0) {
			pmt_found = true;
			handler->pmt_page = i;
		} else if (pmt_found) {
			break;
		}

		ops.row++;
	}

	if (!pmt_found) {
		/* Not found main pmt, try to find mirror pmt */
		ops.row = start_blk + 1;
		ops.row = nandx_bmt_get_mapped_block(ops.row) * ppb;
		ret = nandx_read_pmt_table(info, &ops);
		if (ret > 0) {
			pmt_found = true;
			pr_info("find the latest mpt at page %d\n", ops.row);
			/* recovery main pmt from mirror pmt */
			ops.row = nandx_bmt_get_mapped_block(start_blk) * ppb;
			ret = nandx_core_erase(&ops.row, 1, MODE_SLC);
			if (ret == 0) {
				ret = nandx_core_write(&ops, 1, MODE_SLC);
				if (ret == 0)
					handler->pmt_page = 0;
			}
		}
	} else {
		pr_info("find the latest pt at page %d\n", ops.row - 1);
	}

	nandx_release_device();

	if (!pmt_found) {
		pr_err("failed to find valid pt!\n");
		ret = -EIO;
		goto freebuf;
	}

	handler->part_num = 0;
	for (i = 0; i < PART_MAX_COUNT; i++) {
		handler->part_num++;
		if (!strcmp(pt->name, "BMTPOOL"))
			break;
		pt++;
	}
	mem_free(buf);
	return 0;

freebuf:
	mem_free(buf);
freeblock:
	mem_free(block_bitmap);
freept:
	mem_free(pt);
freehandler:
	mem_free(handler);

	return ret;
}

void nandx_pmt_exit(void)
{
	mem_free(handler->block_bitmap);
	mem_free(handler->pmt);
	mem_free(handler);
}

int nandx_pmt_update(void)
{
	struct nandx_chip_info *info;
	struct nandx_ops ops;
	u8 *buf;
	u32 ppb, sppb, len = 0;
	int ret;

	info = handler->info;
	ppb = info->block_size / info->page_size;
	sppb = ppb / info->wl_page_num;

	buf = mem_alloc(1, info->page_size);
	if (buf == NULL)
		return -ENOMEM;
	memset(buf, 0, info->page_size);

	pr_debug("%s !\n", __func__);
	/* reconstruct pmt buffer */
	len = 4;
	*(u32 *)(buf + len) = handler->usr_slc_ratio;
	*(u32 *)(buf + len) |= handler->sys_slc_ratio << 16;
	len += 4;
	memcpy(buf + len, handler->pmt,
	       PART_MAX_COUNT * sizeof(struct pt_resident));
	len += PART_MAX_COUNT * sizeof(struct pt_resident);
	memcpy(buf + len, handler->block_bitmap,
	       div_up(info->block_num, 32) * sizeof(u32));

	/* erase and write together */
	nandx_get_device(FL_WRITING);

	ops.col = 0;
	ops.len = info->page_size;
	ops.data = buf;
	ops.oob = NULL;
	if (handler->pmt_page == sppb - 1) {
		ops.row = handler->start_blk + 1;
		ops.row = nandx_bmt_get_mapped_block(ops.row) * ppb;
		ret = nandx_core_erase(&ops.row, 1, MODE_SLC);
		if (ret) {
			pr_err("erase mpt failed!\n");
		} else {
			*(u32 *)buf = MPT_SIG;
			ret = nandx_core_write(&ops, 1, MODE_SLC);
			if (ret)
				pr_err("write mpt fail\n");
		}

		ops.row = handler->start_blk;
		ops.row = nandx_bmt_get_mapped_block(ops.row) * ppb;
		ret = nandx_core_erase(&ops.row, 1, MODE_SLC);
		if (ret) {
			pr_err("erase pt block failed!\n");
			goto release_device;
		}

		handler->pmt_page = 0;
	} else {
		handler->pmt_page++;
	}

	*(u32 *)buf = PT_SIG;
	ops.row = handler->start_blk;
	ops.row = nandx_bmt_get_mapped_block(ops.row) * ppb;
	ops.row += handler->pmt_page;
	ret = nandx_core_write(&ops, 1, MODE_SLC);
	if (ret)
		pr_err("write pt failed at page %d!\n", ops.row);

release_device:
	nandx_release_device();

	mem_free(buf);

	return ret;
}

u64 nandx_pmt_get_start_address(struct pt_resident *pt)
{
	return pt->offset;
}

bool nandx_pmt_is_raw_partition(struct pt_resident *pt)
{
	/* TODO: || (pt->ext.type == REGION_SLC_MODE) */
	return pt->ext.type == REGION_LOW_PAGE ? true : false;
}

struct pmt_handler *nandx_get_pmt_handler(void)
{
	return handler;
}

/**
 * nandx_pmt_addr_to_row - translate logical address to physical row address
 * @addr: the logical address
 * @blk: physical block number got
 * @page: page number in the physical block got
 *
 * Attention:
 * This function is not designed to deal with blocks in PMT and BMT pool.
 */
int nandx_pmt_addr_to_row(u64 addr, u32 *blk, u32 *page)
{
	struct nandx_chip_info *info = handler->info;
	struct pt_resident *pt;
	u32 ppb = info->block_size / info->page_size;
	u64 start, len;
	u32 total, slc, div;
	bool is_slc;

	pt = nandx_pmt_get_partition(addr);
	if (!pt) {
		NANDX_ASSERT(0);
		return -EINVAL;
	}

	if (nandx_pmt_is_raw_partition(pt)) {
		is_slc = true;
		start = nandx_pmt_get_start_address(pt);
	} else {
		len = ((struct pt_resident *)(pt + 1))->offset - pt->offset;
		total = (u32)div_down(len, info->block_size);
		if (!strcmp(pt->name, "ANDROID")) {
			slc = total * handler->sys_slc_ratio / 100;
		} else {
			/* user data partition */
			total -= PMT_POOL_SIZE;
			slc = total * handler->usr_slc_ratio / 100;
		}
		slc = div_round_up(slc, info->wl_page_num);

		/* slc buffer start address */
		start = pt->offset + (u64)info->block_size * (total - slc);

		if (addr >= start) {
			is_slc = true;
		} else {
			is_slc = false;
			start = nandx_pmt_get_start_address(pt);
		}
	}

	div = is_slc ? info->wl_page_num : 1;

	len = addr - start;
	*blk = (u32)div_down(start, info->block_size) +
	    (u32)div_down(len, info->block_size / div);
	*page = (u32)div_down(len, info->page_size) % (ppb / div);

	return 0;
}

/**
 * nandx_pmt_blk_is_slc - check whether the block is a SLC MODE block
 * @addr: the check point
 *
 * Return true if it is raw partition or slc buffer of non raw partition.
 */
/* Todo: this algorithm is wrong, should update it. */
bool nandx_pmt_blk_is_slc(u64 addr)
{
	struct nandx_chip_info *info = handler->info;
	struct pt_resident *pt;
	u32 total, slc;
	u64 len;

	pt = nandx_pmt_get_partition(addr);
	if (!pt) {
		NANDX_ASSERT(0);
		return false;
	}

	if (nandx_pmt_is_raw_partition(pt))
		return true;

	len = ((struct pt_resident *)(pt + 1))->offset - pt->offset;
	total = (u32)div_down(len, info->block_size);

	if (!strcmp(pt->name, "ANDROID")) {
		slc = total * handler->sys_slc_ratio / 100;
	} else {
		/* user data partition, BMTPOOL is in the behind of user data */
		total -= PMT_POOL_SIZE;
		slc = total * handler->usr_slc_ratio / 100;
	}

	slc = div_round_up(slc, info->wl_page_num);

	len = pt->offset + (u64)info->block_size * (total - slc);

	return (len <= addr) ? true : false;
}
