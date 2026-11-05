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
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/sched/signal.h>
#include <uapi/linux/sched/types.h>

#include "qcom_dynamic_page_pool.h"

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
#include <trace/events/mi_low_lat_mem.h>
EXPORT_TRACEPOINT_SYMBOL_GPL(mi_smart_refill_pool_shrink);
#endif

static LIST_HEAD(pool_list);
static DEFINE_MUTEX(pool_list_lock);

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
struct dynamic_page_pool *mi_smart_refill_find_pool(int id)
{
	struct dynamic_page_pool *pool = NULL;
	struct mi_smart_refill_data *data;
	struct dynamic_page_pool *found_pool = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pool, &pool_list, list) {
		data = (struct mi_smart_refill_data *)(pool->sr_data);
		if (IS_ERR_OR_NULL(data))
			continue;
		if (id == data->id) {
			found_pool = pool;
			break;
		}
	}
	rcu_read_unlock();
	return found_pool;
}
EXPORT_SYMBOL_GPL(mi_smart_refill_find_pool);

void mi_smart_refill_pools_init(struct list_head *list, int *dmabuf_pool_nums)
{
	struct dynamic_page_pool *pool = NULL;
	struct mi_smart_refill_data *data;

	*dmabuf_pool_nums = 0;
	rcu_read_lock();
	list_for_each_entry_rcu(pool, &pool_list, list) {
		data = (struct mi_smart_refill_data *)(pool->sr_data);
		if (IS_ERR_OR_NULL(data))
			continue;
		if (data->active) {
			list_add(&data->list, list);
			(*dmabuf_pool_nums)++;
		}
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(mi_smart_refill_pools_init);

void mi_smart_refill_update(void)
{
	struct dynamic_page_pool *pool = NULL;
	struct mi_smart_refill_data *data;

	rcu_read_lock();
	list_for_each_entry_rcu(pool, &pool_list, list) {
		data = (struct mi_smart_refill_data *)(pool->sr_data);
		if (IS_ERR_OR_NULL(data))
			continue;
		data->count = atomic_read(&pool->count);
	}
	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(mi_smart_refill_update);
#endif

void dynamic_page_pool_add(struct dynamic_page_pool *pool, struct page *page)
{
	unsigned long flags;

	spin_lock_irqsave(&pool->lock, flags);
	if (PageHighMem(page)) {
		list_add_tail(&page->lru, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&page->lru, &pool->low_items);
		pool->low_count++;
	}

	atomic_inc(&pool->count);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    1 << pool->order);
	spin_unlock_irqrestore(&pool->lock, flags);
}

struct page *dynamic_page_pool_remove(struct dynamic_page_pool *pool, bool high)
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

void dynamic_page_pool_free(struct dynamic_page_pool *pool, struct page *page)
{
	BUG_ON(pool->order != compound_order(page));

	dynamic_page_pool_add(pool, page);
}

int dynamic_page_pool_total(struct dynamic_page_pool *pool, bool high)
{
	int count = pool->low_count;

	if (high)
		count += pool->high_count;

	return count << pool->order;
}

#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
int mi_smart_refill_pool_init(struct dynamic_page_pool *pool)
{
	struct mi_smart_refill_data *data;

	pool->sr_data = NULL;
	pool->sr_data = kzalloc(sizeof(struct mi_smart_refill_data), GFP_KERNEL);
	if (!pool->sr_data)
		return -1;

	data = (struct mi_smart_refill_data *)(pool->sr_data);
	data->id = -1;
	data->order = pool->order;
	return 0;
}
#endif

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
	pool->in_pool_list = false;
	spin_lock_init(&pool->lock);
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	if (mi_smart_refill_pool_init(pool)) {
		kfree(pool);
		return NULL;
	}
#endif

	mutex_lock(&pool_list_lock);
	list_add(&pool->list, &pool_list);
	pool->in_pool_list = true;
	mutex_unlock(&pool_list_lock);

	return pool;
}

void dynamic_page_pool_destroy(struct dynamic_page_pool *pool)
{
	struct page *page, *tmp;
	LIST_HEAD(pages);
	int num_pages = 0;
	int ret = DYNAMIC_POOL_SUCCESS;
	unsigned long flags;

	/* 仅当仍挂在全局链表时才移除；专用池（reserve/camera）已在创建时脱链 */
	if (pool->in_pool_list) {
		mutex_lock(&pool_list_lock);
		list_del(&pool->list);
		pool->in_pool_list = false;
		mutex_unlock(&pool_list_lock);
	}

	/* Free any remaining pages in the pool */
	spin_lock_irqsave(&pool->lock, flags);
	while (true) {
		if (pool->low_count)
			page = dynamic_page_pool_remove(pool, false);
		else if (pool->high_count)
			page = dynamic_page_pool_remove(pool, true);
		else
			break;

		list_add(&page->lru, &pages);
		num_pages++;
	}
	spin_unlock_irqrestore(&pool->lock, flags);

	if (num_pages && pool->prerelease_callback)
		ret = pool->prerelease_callback(pool, &pages, num_pages);

	if (ret != DYNAMIC_POOL_SUCCESS) {
		pr_err("Failed to reclaim pages when destroying the pool!\n");
		return;
	}

	list_for_each_entry_safe(page, tmp, &pages, lru) {
		list_del(&page->lru);
		__free_pages(page, pool->order);
	}

	kfree(pool);
}

static int dynamic_page_pool_do_shrink(struct dynamic_page_pool *pool, gfp_t gfp_mask,
				       int nr_to_scan)
{
	int freed = 0;
	bool high;
	struct page *page, *tmp;
	LIST_HEAD(pages);
	int ret = DYNAMIC_POOL_SUCCESS;

	if (current_is_kswapd())
		high = true;
	else
		high = !!(gfp_mask & __GFP_HIGHMEM);

	if (nr_to_scan == 0)
		return dynamic_page_pool_total(pool, high);
#if IS_ENABLED(CONFIG_MI_LOW_LAT_MEM)
	trace_mi_smart_refill_pool_shrink(pool->sr_data, pool->gfp_mask,
			atomic_read(&pool->count), &nr_to_scan);
#endif
	while (freed < nr_to_scan) {
		unsigned long flags;

		spin_lock_irqsave(&pool->lock, flags);
		if (pool->low_count) {
			page = dynamic_page_pool_remove(pool, false);
		} else if (high && pool->high_count) {
			page = dynamic_page_pool_remove(pool, true);
		} else {
			spin_unlock_irqrestore(&pool->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&pool->lock, flags);
		list_add(&page->lru, &pages);
		freed += (1 << pool->order);
	}

	if (freed && pool->prerelease_callback)
		ret = pool->prerelease_callback(pool, &pages, freed >> pool->order);

	if (ret != DYNAMIC_POOL_SUCCESS) {
		pr_err("Failed to reclaim secure page pool pages!\n");
		return 0;
	}

	list_for_each_entry_safe(page, tmp, &pages, lru) {
		list_del(&page->lru);
		__free_pages(page, pool->order);
	}

	return freed;
}

void dynamic_page_pool_shrink_high_and_low(struct dynamic_page_pool **pools_list,
					   int num_pools, int nr_to_scan)
{
	int i, remaining = nr_to_scan;

	mutex_lock(&pool_list_lock);
	for (i = 0; i < num_pools; i++) {
		remaining -= dynamic_page_pool_do_shrink(pools_list[i], __GFP_HIGHMEM,
							 remaining);

		if (remaining <= 0) {
			mutex_unlock(&pool_list_lock);
			return;
		}
	}
	mutex_unlock(&pool_list_lock);
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

struct dynamic_page_pool **dynamic_page_pool_create_pools(int vmid,
							  prerelease_callback callback)
{
	struct dynamic_page_pool **pool_list;
	int i;
	int ret;

	pool_list = kmalloc_array(NUM_ORDERS, sizeof(*pool_list), GFP_KERNEL);
	if (!pool_list)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < NUM_ORDERS; i++) {
		pool_list[i] = dynamic_page_pool_create(order_flags[i],
							orders[i]);
		if (IS_ERR_OR_NULL(pool_list[i])) {
			int j;

			pr_err("%s: page pool creation failed for the order %u pool!\n",
			       __func__, orders[i]);
			for (j = 0; j < i; j++)
				dynamic_page_pool_destroy(pool_list[j]);

			ret = -ENOMEM;
			goto free_pool_arr;
		}
                pool_list[i]->vmid = vmid;
		pool_list[i]->prerelease_callback = callback;
		atomic_set(&pool_list[i]->count, 0);
		pool_list[i]->last_low_watermark_ktime = 0;

	}

	return pool_list;

free_pool_arr:
	kfree(pool_list);

	return ERR_PTR(ret);
}

void dynamic_page_pool_release_pools(struct dynamic_page_pool **pool_list)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		dynamic_page_pool_destroy(pool_list[i]);

	kfree(pool_list);
}

static struct shrinker pool_shrinker = {
	.count_objects = dynamic_page_pool_shrink_count,
	.scan_objects = dynamic_page_pool_shrink_scan,
	.seeks = DEFAULT_SEEKS,
	.batch = 0,
};

int dynamic_page_pool_init_shrinker(void)
{
	int ret;
	static bool registered;

	if (registered)
		return 0;

	ret = register_shrinker(&pool_shrinker, "dynamic_page_pool");
	if (ret)
		return ret;

	registered = true;
	return 0;
}
