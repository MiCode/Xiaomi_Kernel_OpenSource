/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __TLSF_BASE_H__
#define __TLSF_BASE_H__

#include <linux/types.h>
#include <linux/printk.h>

static int32_t tlsf_fls(uint32_t value)
{
	int32_t bit = value ? (32 - __builtin_clz(value) - 1) : -1;

	return bit;
}

static int32_t tlsf_ffs(uint32_t value)
{
	int32_t bit = __builtin_ffs(value) - 1;

	return bit;
}

static inline size_t align_up(size_t x, size_t align)
{
	return (x + (align - 1)) & ~(align - 1);
}

static inline size_t align_down(size_t x, size_t align)
{
	return x - (x & (align - 1));
}

static inline void set_block_curr_size(struct tlsf_block *block, size_t size)
{
	const size_t old = block->block_size;

	block->block_size =
		size | (old & (TLSF_BLOCK_CURR_FREE | TLSF_BLOCK_PREV_FREE));
}

static inline size_t get_block_curr_size(struct tlsf_block *block)
{
	size_t size = block->block_size &
		~(TLSF_BLOCK_CURR_FREE | TLSF_BLOCK_PREV_FREE);

	return size;
}

static int is_block_prev_free(const struct tlsf_block *block)
{
	return block->block_size & TLSF_BLOCK_PREV_FREE;
}

static inline void set_block_prev_free(struct tlsf_block *block)
{
	block->block_size |= TLSF_BLOCK_PREV_FREE;
}

static inline void set_block_prev_used(struct tlsf_block *block)
{
	block->block_size &= ~TLSF_BLOCK_PREV_FREE;
}

static inline int is_block_curr_free(const struct tlsf_block *block)
{
	return block->block_size & TLSF_BLOCK_CURR_FREE;
}

static inline void set_block_curr_free(struct tlsf_block *block)
{
	block->block_size |= TLSF_BLOCK_CURR_FREE;
}

static inline void set_block_curr_used(struct tlsf_block *block)
{
	block->block_size &= ~TLSF_BLOCK_CURR_FREE;
}

/********************************************************************************************
 *                                  [free block header]       [used block header]           |
 *  | struct tlsf_block *prev_block    |  previous free block end  |                        |
 *  |----------------------------------|---------------------------|------------------------|
 *  | size_t            block_size     |  current block size       |  current block size    |
 *  | struct tlsf_block *next_free     |  user data start here     |  user data start here  |
 *  | struct tlsf_block *prev_fre      |                           |                        |
 *  |                                  |                           |                        |
 *  |                                  |                           |                        |
 *  |                                  |                           |                        |
 *  ----------------------------------------------------------------------------------------|
 *  | zero size block                  |(0 | curr_busy | prev free)|
 ********************************************************************************************/
#define BLOCK_HEADER_OVERLAP    (sizeof(struct tlsf_block *))
#define BLOCK_HEADER_OVERHEAD   (sizeof(size_t))
#define BLOCK_USER_DATA_START   (offsetof(struct tlsf_block, block_size) + sizeof(size_t))

static struct tlsf_block *block_from_ptr(const void *ptr)
{
	return (struct tlsf_block *)((uint8_t *)ptr - BLOCK_USER_DATA_START);
}

static void *block_to_ptr(const struct tlsf_block *block)
{
	return (void *)((uint8_t *)block + BLOCK_USER_DATA_START);
}

static struct tlsf_block *offset_to_block(const void *ptr, size_t size)
{
	return (struct tlsf_block *)((size_t)ptr + size - BLOCK_HEADER_OVERLAP);
}

static struct tlsf_block *block_prev(const struct tlsf_block *block)
{
	return block->prev_block;
}

static struct tlsf_block *block_next(struct tlsf_block *block)
{
	struct tlsf_block *next;

	next = offset_to_block(block_to_ptr(block),
		get_block_curr_size(block));
	return next;
}

static struct tlsf_block *link_block_to_next(struct tlsf_block *block)
{
	struct tlsf_block *next;

	next = block_next(block);
	next->prev_block = block;
	return next;
}

static inline size_t query_pool_overhead(void)
{
	return 2 * BLOCK_HEADER_OVERHEAD;
}

#endif  // __TLSF_BASE_H__
