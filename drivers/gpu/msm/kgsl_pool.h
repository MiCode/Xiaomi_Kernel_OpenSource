/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_POOL_H
#define __KGSL_POOL_H

#include <linux/mm_types.h>
#include "kgsl_sharedmem.h"

static inline unsigned int
kgsl_gfp_mask(unsigned int page_order)
{
	unsigned int gfp_mask = __GFP_HIGHMEM;

	if (page_order > 0) {
		gfp_mask |= __GFP_COMP | __GFP_NORETRY | __GFP_NOWARN;
		gfp_mask &= ~__GFP_RECLAIM;
	} else
		gfp_mask |= GFP_KERNEL;

	if (kgsl_sharedmem_get_noretry())
		gfp_mask |= __GFP_NORETRY | __GFP_NOWARN;

	return gfp_mask;
}

void kgsl_pool_free_sgt(struct sg_table *sgt);
void kgsl_pool_free_pages(struct page **pages, unsigned int page_count);
void kgsl_init_page_pools(struct platform_device *pdev);
void kgsl_exit_page_pools(void);
int kgsl_pool_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align);
void kgsl_pool_free_page(struct page *p);
bool kgsl_pool_avaialable(int size);
#endif /* __KGSL_POOL_H */

