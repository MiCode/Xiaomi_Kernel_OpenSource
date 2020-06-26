/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ION Page Pool kernel interface header
 *
 * Copyright (C) 2011 Google, Inc.
 */

#ifndef _ION_MSM_PAGE_POOL_H
#define _ION_MSM_PAGE_POOL_H

#include <linux/mm_types.h>
#include <linux/mutex.h>
#include <linux/shrinker.h>
#include <linux/types.h>

/* ION page pool marks in bytes */
#ifdef CONFIG_ION_POOL_AUTO_REFILL
#define ION_POOL_FILL_MARK (CONFIG_ION_POOL_FILL_MARK * SZ_1M)
#define POOL_LOW_MARK_PERCENT	40UL
#define ION_POOL_LOW_MARK ((ION_POOL_FILL_MARK * POOL_LOW_MARK_PERCENT) / 100)
#else
#define ION_POOL_FILL_MARK 0UL
#define ION_POOL_LOW_MARK 0UL
#endif

/* if low watermark of zones have reached, defer the refill in this window */
#define ION_POOL_REFILL_DEFER_WINDOW_MS	10

/**
 * functions for creating and destroying a heap pool -- allows you
 * to keep a pool of pre allocated memory to use from your heap.  Keeping
 * a pool of memory that is ready for dma, ie any cached mapping have been
 * invalidated from the cache, provides a significant performance benefit on
 * many systems
 */

/**
 * struct ion_msm_page_pool - pagepool struct
 * @high_count:		number of highmem items in the pool
 * @low_count:		number of lowmem items in the pool
 * @count:		total number of pages/items in the pool
 * @high_items:		list of highmem items
 * @low_items:		list of lowmem items
 * @last_low_watermark_ktime: most recent time at which the zone watermarks were
 *			low
 * @mutex:		lock protecting this struct and especially the count
 *			item list
 * @gfp_mask:		gfp_mask to use from alloc
 * @order:		order of pages in the pool
 * @list:		plist node for list of pools
 * @cached:		it's cached pool or not
 * @heap_dev:		device for the ion heap associated with this pool
 *
 * Allows you to keep a pool of pre allocated pages to use from your heap.
 * Keeping a pool of pages that is ready for dma, ie any cached mapping have
 * been invalidated from the cache, provides a significant performance benefit
 * on many systems
 */
struct ion_msm_page_pool {
	int high_count;
	int low_count;
	atomic_t count;
	struct list_head high_items;
	struct list_head low_items;
	ktime_t last_low_watermark_ktime;
	struct mutex mutex;
	gfp_t gfp_mask;
	unsigned int order;
	struct plist_node list;
	bool cached;
	struct device *heap_dev;
};

struct ion_msm_page_pool *ion_msm_page_pool_create(gfp_t gfp_mask,
						   unsigned int order,
						   bool cached);
void ion_msm_page_pool_destroy(struct ion_msm_page_pool *pool);
struct page *ion_msm_page_pool_alloc(struct ion_msm_page_pool *pool,
				     bool *from_pool);
void ion_msm_page_pool_free(struct ion_msm_page_pool *pool, struct page *page);
struct page *ion_msm_page_pool_alloc_pool_only(struct ion_msm_page_pool *a);
void ion_msm_page_pool_free_immediate(struct ion_msm_page_pool *pool,
				      struct page *page);
int ion_msm_page_pool_total(struct ion_msm_page_pool *pool, bool high);
size_t ion_system_heap_secure_page_pool_total(struct ion_heap *heap, int vmid);

/** ion_msm_page_pool_shrink - shrinks the size of the memory cached in the pool
 * @pool:		the pool
 * @gfp_mask:		the memory type to reclaim
 * @nr_to_scan:		number of items to shrink in pages
 *
 * returns the number of items freed in pages
 */
int ion_msm_page_pool_shrink(struct ion_msm_page_pool *pool, gfp_t gfp_mask,
			     int nr_to_scan);

#ifdef CONFIG_ION_POOL_AUTO_REFILL
void ion_msm_page_pool_refill(struct ion_msm_page_pool *pool);

static __always_inline int get_pool_fillmark(struct ion_msm_page_pool *pool)
{
	return ION_POOL_FILL_MARK / (PAGE_SIZE << pool->order);
}

static __always_inline int get_pool_lowmark(struct ion_msm_page_pool *pool)
{
	return ION_POOL_LOW_MARK / (PAGE_SIZE << pool->order);
}

static __always_inline bool
pool_count_below_lowmark(struct ion_msm_page_pool *pool)
{
	return atomic_read(&pool->count) < get_pool_lowmark(pool);
}

static __always_inline bool
pool_fillmark_reached(struct ion_msm_page_pool *pool)
{
	return atomic_read(&pool->count) >= get_pool_fillmark(pool);
}
#else
static inline void ion_msm_page_pool_refill(struct ion_msm_page_pool *pool)
{
}

static __always_inline int get_pool_fillmark(struct ion_msm_page_pool *pool)
{
	return 0;
}

static __always_inline int get_pool_lowmark(struct ion_msm_page_pool *pool)
{
	return 0;
}

static __always_inline bool
pool_count_below_lowmark(struct ion_msm_page_pool *pool)
{
	return false;
}

static __always_inline bool
pool_fillmark_reached(struct ion_msm_page_pool *pool)
{
	return false;
}
#endif /* CONFIG_ION_POOL_AUTO_REFILL */
#endif /* _ION_MSM_PAGE_POOL_H */
