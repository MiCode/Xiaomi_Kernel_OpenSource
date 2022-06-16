// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "tlsf_alloc.h"
#include "tlsf_base.h"

static int32_t mapping_insert(size_t size, int32_t *fli, int32_t *sli)
{
	int32_t fl;
	int32_t sl;

	fl = tlsf_fls(size);
	if (fl < TLSF_SL_INDEX_LOG2) {
		pr_info("%s: block size(%lu/%d) too small", __func__, size, fl);
		return -1;
	}

	sl = (size >> (fl - TLSF_SL_INDEX_LOG2)) ^ (1 << TLSF_SL_INDEX_LOG2);

	*fli = fl;
	*sli = sl;

	return 0;
}

static int32_t mapping_search(size_t size, int32_t *fli, int32_t *sli)
{
	int32_t fl;
	size_t round;

	fl = tlsf_fls(size);
	if (fl < TLSF_SL_INDEX_LOG2) {
		pr_info("%s: block size(%lu/%d) is too small", __func__, size, fl);
		return -1;
	}

	round = (1 << (fl - TLSF_SL_INDEX_LOG2)) - 1;
	size += round;

	return mapping_insert(size, fli, sli);
}

static struct tlsf_block *search_suitable_block(struct tlsf_info *info,
	int32_t *fli, int32_t *sli)
{
	int32_t fl;
	int32_t sl;
	uint32_t sl_bitmap;
	uint32_t fl_bitmap;

	fl = *fli;
	sl = *sli;

	if ((fl < 0) || (fl >= TLSF_FL_INDEX_COUNT)) {
		pr_info("%s: invalid first level index(%d)", __func__, fl);
		return NULL;
	}

	sl_bitmap = info->sl_bitmap[fl] & (~0U << sl);
	if (sl_bitmap) {
		sl = tlsf_ffs(sl_bitmap);
	} else {
		fl_bitmap = info->fl_bitmap & (~0U << (fl + 1));
		if (!fl_bitmap)
			return NULL;

		fl = tlsf_ffs(fl_bitmap);
		sl = tlsf_ffs(info->sl_bitmap[fl]);
	}

	if ((fl < 0) || (fl >= TLSF_FL_INDEX_COUNT)) {
		pr_info("%s: invalid first level index(%d)", __func__, fl);
		return NULL;
	}

	if ((sl < 0) || (sl >= TLSF_SL_INDEX_COUNT)) {
		pr_info("%s: invalid second level index(%d)", __func__, sl);
		return NULL;
	}

	*fli = fl;
	*sli = sl;

	return info->block[fl][sl];
}

static void mark_block_as_free(struct tlsf_block *block)
{
	/* Link the block to the next block, first. */
	struct tlsf_block *next;

	next = link_block_to_next(block);
	set_block_prev_free(next);
	set_block_curr_free(block);
}

static void mark_block_as_used(struct tlsf_block *block)
{
	struct tlsf_block *next;

	next = block_next(block);
	set_block_prev_used(next);
	set_block_curr_used(block);
}

static struct tlsf_block *split_curr_block(struct tlsf_block *block,
	size_t size)
{
	struct tlsf_block *remaining = offset_to_block(block_to_ptr(block), size);

	const size_t remain_size =
		get_block_curr_size(block) - (size + BLOCK_HEADER_OVERHEAD);

	set_block_curr_size(remaining, remain_size);

	set_block_curr_size(block, size);
	mark_block_as_free(remaining);

	return remaining;
}

static int32_t insert_free_block(struct tlsf_info *info,
	struct tlsf_block *prev, int32_t fli, int sli)
{
	struct tlsf_block *next;

	if ((fli < 0) || (fli >= TLSF_FL_INDEX_COUNT)) {
		pr_info("%s: invalid first level index(%d)", __func__, fli);
		return -1;
	}

	if ((sli < 0) || (sli >= TLSF_SL_INDEX_COUNT)) {
		pr_info("%s: invalid second level index(%d)", __func__, sli);
		return -1;
	}

	next = info->block[fli][sli];

	prev->next_free = next;
	prev->prev_free = &info->block_null;
	next->prev_free = prev;

	info->block[fli][sli] = prev;
	info->fl_bitmap |= (1U << fli);
	info->sl_bitmap[fli] |= (1U << sli);

	return 0;
}

static int32_t remove_free_block(struct tlsf_info *info,
	struct tlsf_block *block, int32_t fli, int32_t sli)
{
	struct tlsf_block *prev;
	struct tlsf_block *next;

	if ((fli < 0) || (fli >= TLSF_FL_INDEX_COUNT)) {
		pr_info("%s: invalid first level index(%d)", __func__, fli);
		return -1;
	}

	if ((sli < 0) || (sli >= TLSF_SL_INDEX_COUNT)) {
		pr_info("%s: invalid second level index(%d)", __func__, sli);
		return -1;
	}

	prev = block->prev_free;
	next = block->next_free;

	next->prev_free = prev;
	prev->next_free = next;

	/* If this block is the head of the free list, set new head. */
	if (info->block[fli][sli] == block) {
		info->block[fli][sli] = next;

		/* If the new head is null, clear the bitmap. */
		if (next == &info->block_null) {
			info->sl_bitmap[fli] &= ~(1U << sli);

			/* If the second bitmap is now empty, clear the fl bitmap. */
			if (!info->sl_bitmap[fli])
				info->fl_bitmap &= ~(1U << fli);
		}
	}

	return 0;
}

static struct tlsf_block *locate_free_block(struct tlsf_info *info, size_t size)
{
	int32_t fli;
	int32_t sli;
	int32_t ret;
	struct tlsf_block *block;

	block = NULL;
	if (size) {
		ret = mapping_search(size, &fli, &sli);
		if (ret < 0) {
			pr_info("%s: failed to searh block size(%lu)", __func__, size);
			return NULL;
		}

		if (fli < TLSF_FL_INDEX_COUNT)
			block = search_suitable_block(info, &fli, &sli);
	}

	if (block) {
		ret = remove_free_block(info, block, fli, sli);
		if (ret < 0) {
			pr_info("%s: failed to remove free block", __func__);
			return NULL;
		}
	}

	return block;
}

static void insert_block_to_list(struct tlsf_info *info, struct tlsf_block *block)
{
	int ret;
	int32_t fli;
	int32_t sli;

	ret = mapping_insert(get_block_curr_size(block), &fli, &sli);
	if (ret < 0) {
		pr_info("%s: failed to do mapping insert", __func__);
		return;
	}

	(void)insert_free_block(info, block, fli, sli);
}

static int can_split_block(struct tlsf_block *block, size_t size)
{
	return get_block_curr_size(block) >= (sizeof(struct tlsf_block) + size);
}

static void trim_free_block(struct tlsf_info *info,
	struct tlsf_block *block, size_t size)
{
	struct tlsf_block *split;

	if (can_split_block(block, size)) {
		split = split_curr_block(block, size);
		link_block_to_next(block);
		set_block_prev_free(split);
		insert_block_to_list(info, split);
	}
}

static int32_t reset_info(struct tlsf_info *info)
{
	int32_t i;
	int32_t j;

	if (info == NULL)
		return -1;

	info->block_null.next_free = &info->block_null;
	info->block_null.prev_free = &info->block_null;

	info->fl_bitmap = 0;
	for (i = 0; i < TLSF_FL_INDEX_COUNT; ++i) {
		info->sl_bitmap[i] = 0;
		for (j = 0; j < TLSF_SL_INDEX_COUNT; ++j)
			info->block[i][j] = &info->block_null;
	}

	return 0;
}

int32_t tlsf_init(struct tlsf_info *info, void *mem, size_t size)
{
	struct tlsf_block *block;
	struct tlsf_block *next;
	int32_t ret;

	ret = reset_info(info);
	if (ret < 0)
		return ret;

	size = size - query_pool_overhead();
	size = align_down(size, TLSF_ALIGN_SIZE_BASE);

	block = offset_to_block(mem, 0);
	set_block_curr_size(block, size);
	set_block_curr_free(block);
	set_block_prev_used(block);
	insert_block_to_list(info, block);

	next = link_block_to_next(block);
	set_block_curr_size(next, 0);
	set_block_curr_used(next);
	set_block_prev_free(next);

	return 0;
}

void *tlsf_malloc(struct tlsf_info *info, size_t size)
{
	struct tlsf_block *block;

	if (size <= 0) {
		pr_info("%s: invalid malloc size(%lu)", __func__, size);
		return NULL;
	}

	size = align_up(size, TLSF_ALIGN_SIZE_BASE);
	if (size < TLSF_BLOCK_SIZE_MIN)
		size = TLSF_BLOCK_SIZE_MIN;

	block = locate_free_block(info, size);
	if (block) {
		trim_free_block(info, block, size);
		mark_block_as_used(block);
		return block_to_ptr(block);
	}

	return NULL;
}

static struct tlsf_block *block_absorb(struct tlsf_block *prev,
	struct tlsf_block *block)
{
	prev->block_size += get_block_curr_size(block) + BLOCK_HEADER_OVERHEAD;
	link_block_to_next(prev);

	return prev;
}

static void block_remove(struct tlsf_info *info, struct tlsf_block *block)
{
	int ret;
	int32_t fli;
	int32_t sli;

	fli = 0;
	sli = 0;
	ret = mapping_insert(get_block_curr_size(block), &fli, &sli);
	if (ret < 0) {
		pr_info("%s: failed to do mapping insert", __func__);
		return;
	}

	(void)remove_free_block(info, block, fli, sli);
}

static struct tlsf_block *merge_prev_block(struct tlsf_info *info,
	struct tlsf_block *block)
{
	struct tlsf_block *prev;

	if (is_block_prev_free(block)) {
		prev = block_prev(block);
		block_remove(info, prev);
		block = block_absorb(prev, block);
	}

	return block;
}

static struct tlsf_block *merge_next_block(struct tlsf_info *info,
	struct tlsf_block *block)
{
	struct tlsf_block *next = block_next(block);

	if (is_block_curr_free(next)) {
		block_remove(info, next);
		block = block_absorb(block, next);
	}

	return block;
}

void tlsf_free(struct tlsf_info *info, void *ptr)
{
	struct tlsf_block *block;

	if (ptr) {
		block = block_from_ptr(ptr);
		mark_block_as_free(block);
		block = merge_prev_block(info, block);
		block = merge_next_block(info, block);
		insert_block_to_list(info, block);
	}
}

int32_t tlsf_deinit(struct tlsf_info *info)
{
	return 0;
}
