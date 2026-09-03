// SPDX-License-Identifier: GPL-2.0-only
/*
 * unisoc_mm_cma.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gfp.h>
#include <trace/hooks/mm.h>

#define ALLOC_CMA 0x80 /* allow allocations from CMA areas */
#define ALLOC_KSWAPD		0x800

#define GFP_MOVABLE_MASK (__GFP_RECLAIMABLE|__GFP_MOVABLE)
#define GFP_MOVABLE_SHIFT 3

#ifdef CONFIG_GFP_HIGHUSER_ADJUST
static void alloc_highpage_movable_gfp_adjust(void *data, gfp_t *gfp_mask)
{}
static void set_skip_swapcache_flags(void *data, gfp_t *gfp_mask)
{}
static void should_alloc_pages_retry(void *data, gfp_t gfp_flags, int order, int *alloc_flags,
	int migratetype, struct zone *preferred_zone, struct page **page, bool *should_alloc_retry)
{
#ifdef CONFIG_CMA
	if (*alloc_flags & ALLOC_CMA)
		*should_alloc_retry = false;
	else if (((gfp_flags & GFP_MOVABLE_MASK) >> GFP_MOVABLE_SHIFT) == MIGRATE_MOVABLE) {
		*alloc_flags &= ~ALLOC_KSWAPD;
		*alloc_flags |= ALLOC_CMA;
		*should_alloc_retry = true;
	}
#endif
}

static void rmqueue_cma_fallback(void *data, struct zone *zone, unsigned int order
		, bool *try_cma)
{
	/*todo: judge if free_cma > total_free_pages / 2*/
	*try_cma = true;
}
#else
static void alloc_highpage_movable_gfp_adjust(void *data, gfp_t *gfp_mask)
{}
static void set_skip_swapcache_flags(void *data, gfp_t *gfp_mask)
{}
static void rmqueue_cma_fallback(struct zone *zone, unsigned int order
		, struct page **page)
{}
static void alloc_flags_cma_adjust(gfp_t gfp_mask, unsigned int *alloc_flags)
{}
#endif

static int sprd_cma_init(void)
{
	register_trace_android_vh_anon_gfp_adjust(
			alloc_highpage_movable_gfp_adjust, NULL);
	register_trace_android_rvh_set_skip_swapcache_flags(set_skip_swapcache_flags, NULL);
	register_trace_android_vh_should_alloc_pages_retry(should_alloc_pages_retry, NULL);
	register_trace_android_vh_try_cma_fallback(rmqueue_cma_fallback, NULL);
	return 0;
}
module_init(sprd_cma_init);
MODULE_LICENSE("GPL");
