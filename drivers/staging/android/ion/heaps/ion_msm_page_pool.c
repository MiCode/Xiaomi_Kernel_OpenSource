// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator page pool helpers
 *
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/sched/signal.h>

#include "msm_ion_priv.h"
#include "ion_msm_page_pool.h"

static inline struct page
*ion_msm_page_pool_alloc_pages(struct ion_msm_page_pool *pool)
{
	if (fatal_signal_pending(current))
		return NULL;
	return alloc_pages(pool->gfp_mask, pool->order);
}

static void ion_msm_page_pool_free_pages(struct ion_msm_page_pool *pool,
					 struct page *page)
{
	__free_pages(page, pool->order);
}

static void ion_msm_page_pool_add(struct ion_msm_page_pool *pool,
				  struct page *page)
{
	mutex_lock(&pool->mutex);
	if (PageHighMem(page)) {
		list_add_tail(&page->lru, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&page->lru, &pool->low_items);
		pool->low_count++;
	}

	atomic_inc(&pool->count);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    (1 << pool->order));
	mutex_unlock(&pool->mutex);
}

#ifdef CONFIG_ION_POOL_AUTO_REFILL
/* do a simple check to see if we are in any low memory situation */
static bool pool_refill_ok(struct ion_msm_page_pool *pool)
{
	struct zonelist *zonelist;
	struct zoneref *z;
	struct zone *zone;
	int mark;
	enum zone_type classzone_idx = gfp_zone(pool->gfp_mask);
	s64 delta;

	/* check if we are within the refill defer window */
	delta = ktime_ms_delta(ktime_get(), pool->last_low_watermark_ktime);
	if (delta < ION_POOL_REFILL_DEFER_WINDOW_MS)
		return false;

	zonelist = node_zonelist(numa_node_id(), pool->gfp_mask);
	/*
	 * make sure that if we allocate a pool->order page from buddy,
	 * we don't put the zone watermarks go below the high threshold.
	 * This makes sure there's no unwanted repetitive refilling and
	 * reclaiming of buddy pages on the pool.
	 */
	for_each_zone_zonelist(zone, z, zonelist, classzone_idx) {
		mark = high_wmark_pages(zone);
		mark += 1 << pool->order;
		if (!zone_watermark_ok_safe(zone, pool->order, mark,
					    classzone_idx)) {
			pool->last_low_watermark_ktime = ktime_get();
			return false;
		}
	}

	return true;
}

void ion_msm_page_pool_refill(struct ion_msm_page_pool *pool)
{
	struct page *page;
	gfp_t gfp_refill = (pool->gfp_mask | __GFP_RECLAIM) & ~__GFP_NORETRY;
	struct device *dev = pool->heap_dev;

	/* skip refilling order 0 pools */
	if (!pool->order)
		return;

	while (!pool_fillmark_reached(pool) && pool_refill_ok(pool)) {
		page = alloc_pages(gfp_refill, pool->order);
		if (!page)
			break;
		if (!pool->cached)
			ion_pages_sync_for_device(dev, page,
						  PAGE_SIZE << pool->order,
						  DMA_BIDIRECTIONAL);
		ion_msm_page_pool_add(pool, page);
	}
}
#endif /* CONFIG_ION_PAGE_POOL_REFILL */

static struct page *ion_msm_page_pool_remove(struct ion_msm_page_pool *pool,
					     bool high)
{
	struct page *page;

	if (high) {
		BUG_ON(!pool->high_count);
		page = list_first_entry(&pool->high_items, struct page, lru);
		pool->high_count--;
	} else {
		BUG_ON(!pool->low_count);
		page = list_first_entry(&pool->low_items, struct page, lru);
		pool->low_count--;
	}

	atomic_dec(&pool->count);
	list_del(&page->lru);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
					-(1 << pool->order));
	return page;
}

struct page *ion_msm_page_pool_alloc(struct ion_msm_page_pool *pool,
				     bool *from_pool)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	if (fatal_signal_pending(current))
		return ERR_PTR(-EINTR);

	if (*from_pool && mutex_trylock(&pool->mutex)) {
		if (pool->high_count)
			page = ion_msm_page_pool_remove(pool, true);
		else if (pool->low_count)
			page = ion_msm_page_pool_remove(pool, false);
		mutex_unlock(&pool->mutex);
	}
	if (!page) {
		page = ion_msm_page_pool_alloc_pages(pool);
		*from_pool = false;
	}

	if (!page)
		return ERR_PTR(-ENOMEM);
	return page;
}

/*
 * Tries to allocate from only the specified Pool and returns NULL otherwise
 */
struct page *ion_msm_page_pool_alloc_pool_only(struct ion_msm_page_pool *pool)
{
	struct page *page = NULL;

	if (!pool)
		return ERR_PTR(-EINVAL);

	if (mutex_trylock(&pool->mutex)) {
		if (pool->high_count)
			page = ion_msm_page_pool_remove(pool, true);
		else if (pool->low_count)
			page = ion_msm_page_pool_remove(pool, false);
		mutex_unlock(&pool->mutex);
	}

	if (!page)
		return ERR_PTR(-ENOMEM);
	return page;
}

void ion_msm_page_pool_free(struct ion_msm_page_pool *pool, struct page *page)
{
	ion_msm_page_pool_add(pool, page);
}

void ion_msm_page_pool_free_immediate(struct ion_msm_page_pool *pool,
				      struct page *page)
{
	ion_msm_page_pool_free_pages(pool, page);
}

int ion_msm_page_pool_total(struct ion_msm_page_pool *pool, bool high)
{
	int count = pool->low_count;

	if (high)
		count += pool->high_count;

	return count << pool->order;
}

int ion_msm_page_pool_shrink(struct ion_msm_page_pool *pool, gfp_t gfp_mask,
			     int nr_to_scan)
{
	int freed = 0;
	bool high;

	if (current_is_kswapd())
		high = true;
	else
		high = !!(gfp_mask & __GFP_HIGHMEM);

	if (nr_to_scan == 0)
		return ion_msm_page_pool_total(pool, high);

	while (freed < nr_to_scan) {
		struct page *page;

		mutex_lock(&pool->mutex);
		if (pool->low_count) {
			page = ion_msm_page_pool_remove(pool, false);
		} else if (high && pool->high_count) {
			page = ion_msm_page_pool_remove(pool, true);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		ion_msm_page_pool_free_pages(pool, page);
		freed += (1 << pool->order);
	}

	return freed;
}

struct ion_msm_page_pool *ion_msm_page_pool_create(gfp_t gfp_mask,
						   unsigned int order,
						   bool cached)
{
	struct ion_msm_page_pool *pool = kzalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool)
		return NULL;
	INIT_LIST_HEAD(&pool->low_items);
	INIT_LIST_HEAD(&pool->high_items);
	pool->gfp_mask = gfp_mask;
	pool->order = order;
	mutex_init(&pool->mutex);
	plist_node_init(&pool->list, order);
	if (cached)
		pool->cached = true;

	return pool;
}

void ion_msm_page_pool_destroy(struct ion_msm_page_pool *pool)
{
	kfree(pool);
}
