// SPDX-License-Identifier: GPL-2.0
/*
 * Dynamic page pool system
 *
 * Taken from:
 * mm/dynamic_page_pool.c
 * Git-repo: https://git.linaro.org/people/john.stultz/android-dev.git
 * Branch: dma-buf-heap-perf
 * Git-commit: 458ea8030852755867bdc0384aa40f97aba7a572
 *
 * Based on the ION page pool code
 * Copyright (C) 2011 Google, Inc.
 *
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/sched/signal.h>

#include "qcom_dynamic_page_pool.h"

static LIST_HEAD(pool_list);
static DEFINE_MUTEX(pool_list_lock);

static inline
struct page *dynamic_page_pool_alloc_pages(struct dynamic_page_pool *pool)
{
	if (fatal_signal_pending(current))
		return NULL;
	return alloc_pages(pool->gfp_mask, pool->order);
}

static void dynamic_page_pool_free_pages(struct dynamic_page_pool *pool,
				     struct page *page)
{
	__free_pages(page, pool->order);
}

static void dynamic_page_pool_add(struct dynamic_page_pool *pool, struct page *page)
{
	mutex_lock(&pool->mutex);
	if (PageHighMem(page)) {
		list_add_tail(&page->lru, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&page->lru, &pool->low_items);
		pool->low_count++;
	}

	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    1 << pool->order);
	mutex_unlock(&pool->mutex);
}

static struct page *dynamic_page_pool_remove(struct dynamic_page_pool *pool, bool high)
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

	list_del(&page->lru);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    -(1 << pool->order));
	return page;
}

struct page *dynamic_page_pool_alloc(struct dynamic_page_pool *pool)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	mutex_lock(&pool->mutex);
	if (pool->high_count)
		page = dynamic_page_pool_remove(pool, true);
	else if (pool->low_count)
		page = dynamic_page_pool_remove(pool, false);
	mutex_unlock(&pool->mutex);

	if (!page)
		page = dynamic_page_pool_alloc_pages(pool);

	return page;
}

void dynamic_page_pool_free(struct dynamic_page_pool *pool, struct page *page)
{
	BUG_ON(pool->order != compound_order(page));

	dynamic_page_pool_add(pool, page);
}

static int dynamic_page_pool_total(struct dynamic_page_pool *pool, bool high)
{
	int count = pool->low_count;

	if (high)
		count += pool->high_count;

	return count << pool->order;
}

struct dynamic_page_pool *dynamic_page_pool_create(gfp_t gfp_mask, unsigned int order)
{
	struct dynamic_page_pool *pool = kmalloc(sizeof(*pool), GFP_KERNEL);

	if (!pool)
		return NULL;
	pool->high_count = 0;
	pool->low_count = 0;
	INIT_LIST_HEAD(&pool->low_items);
	INIT_LIST_HEAD(&pool->high_items);
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	mutex_init(&pool->mutex);

	mutex_lock(&pool_list_lock);
	list_add(&pool->list, &pool_list);
	mutex_unlock(&pool_list_lock);

	return pool;
}

void dynamic_page_pool_destroy(struct dynamic_page_pool *pool)
{
	struct page *page;

	/* Remove us from the pool list */
	mutex_lock(&pool_list_lock);
	list_del(&pool->list);
	mutex_unlock(&pool_list_lock);

	/* Free any remaining pages in the pool */
	mutex_lock(&pool->mutex);
	while (true) {
		if (pool->low_count)
			page = dynamic_page_pool_remove(pool, false);
		else if (pool->high_count)
			page = dynamic_page_pool_remove(pool, true);
		else
			break;

		dynamic_page_pool_free_pages(pool, page);
	}
	mutex_unlock(&pool->mutex);

	kfree(pool);
}

int dynamic_page_pool_do_shrink(struct dynamic_page_pool *pool, gfp_t gfp_mask,
				int nr_to_scan)
{
	int freed = 0;
	bool high;

	if (current_is_kswapd())
		high = true;
	else
		high = !!(gfp_mask & __GFP_HIGHMEM);

	if (nr_to_scan == 0)
		return dynamic_page_pool_total(pool, high);

	while (freed < nr_to_scan) {
		struct page *page;

		mutex_lock(&pool->mutex);
		if (pool->low_count) {
			page = dynamic_page_pool_remove(pool, false);
		} else if (high && pool->high_count) {
			page = dynamic_page_pool_remove(pool, true);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		dynamic_page_pool_free_pages(pool, page);
		freed += (1 << pool->order);
	}

	return freed;
}

static int dynamic_page_pool_shrink(gfp_t gfp_mask, int nr_to_scan)
{
	struct dynamic_page_pool *pool;
	int nr_total = 0;
	int nr_freed;
	int only_scan = 0;

	if (!nr_to_scan)
		only_scan = 1;

	mutex_lock(&pool_list_lock);
	list_for_each_entry(pool, &pool_list, list) {
		if (only_scan) {
			nr_total += dynamic_page_pool_do_shrink(pool,
								gfp_mask,
								nr_to_scan);
		} else {
			nr_freed = dynamic_page_pool_do_shrink(pool,
							       gfp_mask,
							       nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
	}
	mutex_unlock(&pool_list_lock);

	return nr_total;
}

static unsigned long dynamic_page_pool_shrink_count(struct shrinker *shrinker,
						    struct shrink_control *sc)
{
	return dynamic_page_pool_shrink(sc->gfp_mask, 0);
}

static unsigned long dynamic_page_pool_shrink_scan(struct shrinker *shrinker,
						   struct shrink_control *sc)
{
	int to_scan = sc->nr_to_scan;

	if (to_scan == 0)
		return 0;

	return dynamic_page_pool_shrink(sc->gfp_mask, to_scan);
}

struct shrinker pool_shrinker = {
	.count_objects = dynamic_page_pool_shrink_count,
	.scan_objects = dynamic_page_pool_shrink_scan,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};

int dynamic_page_pool_init_shrinker(void)
{
	return register_shrinker(&pool_shrinker);
}
