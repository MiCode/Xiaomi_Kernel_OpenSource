/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __TLSF_ALLOC_H__
#define __TLSF_ALLOC_H__

#include <linux/types.h>

#define TLSF_ALIGN_SIZE_LOG2  (3)
#define TLSF_ALIGN_SIZE_BASE  (1 << TLSF_ALIGN_SIZE_LOG2)

#define TLSF_SL_INDEX_LOG2    (3)
#define TLSF_SL_INDEX_COUNT   (1 << TLSF_SL_INDEX_LOG2)
#define TLSF_FL_INDEX_MAX     (30)
#define TLSF_FL_INDEX_SHIFT   (TLSF_SL_INDEX_LOG2 + TLSF_ALIGN_SIZE_LOG2)
#define TLSF_FL_INDEX_COUNT   (TLSF_FL_INDEX_MAX - TLSF_FL_INDEX_SHIFT + 1)

#define TLSF_BLOCK_SIZE_MAX   (1UL << TLSF_FL_INDEX_MAX)
#define TLSF_BLOCK_SIZE_MIN   (sizeof(struct tlsf_block) - sizeof(struct tlsf_block *))
#define TLSF_BLOCK_CURR_FREE  (0x01)
#define TLSF_BLOCK_PREV_FREE  (0x02)

struct tlsf_block {
	struct tlsf_block *prev_block;
	size_t            block_size;
	struct tlsf_block *next_free;
	struct tlsf_block *prev_free;
};

struct tlsf_info {
	struct tlsf_block block_null;

	uint32_t fl_bitmap;
	uint32_t sl_bitmap[TLSF_FL_INDEX_COUNT];

	struct tlsf_block *block[TLSF_FL_INDEX_COUNT][TLSF_SL_INDEX_COUNT];
};

#ifdef __cplusplus
extern "C" {
#endif

int32_t tlsf_init(struct tlsf_info *info,
	void *mem, size_t size);

void *tlsf_malloc(struct tlsf_info *info, size_t size);

void tlsf_free(struct tlsf_info *info, void *buffer);

int32_t tlsf_deinit(struct tlsf_info *info);

#ifdef __cplusplus
}
#endif

#endif  // __TLSF_ALLOC_H__
