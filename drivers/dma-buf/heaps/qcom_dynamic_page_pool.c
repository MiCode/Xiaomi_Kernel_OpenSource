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
 */

#include <linux/list.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/spinlock.h>
#include <linux/sched/signal.h>
#include <uapi/linux/sched/types.h>

#include "qcom_dynamic_page_pool.h"
#include "reserve_dynamic_page_pool.h"

static LIST_HEAD(pool_list);
static DEFINE_MUTEX(pool_list_lock);

#define DEFAULT_TOTAL_POOLS_MAX_PAGES 0UL

int msm_total_pools_max = DEFAULT_TOTAL_POOLS_MAX_PAGES;
EXPORT_SYMBOL(msm_total_pools_max);

atomic_t msm_total_pools_size;
EXPORT_SYMBOL(msm_total_pools_size);

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
	atomic_add((1 << pool->order), &msm_total_pools_size);
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
	atomic_sub((1 << pool->order), &msm_total_pools_size);
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
	spin_lock_init(&pool->lock);

	mutex_lock(&pool_list_lock);
	list_add(&pool->list, &pool_list);
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

	/* Remove us from the pool list */
	mutex_lock(&pool_list_lock);
	list_del(&pool->list);
	mutex_unlock(&pool_list_lock);

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
		if (!can_do_shrink(pool, high))
			break;
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
		if (is_reserve_vmid(vmid)) {
			mutex_lock(&pool_list_lock);
			list_del(&pool_list[i]->list);
			mutex_unlock(&pool_list_lock);
		}
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

static ssize_t
msm_total_pools_max_show(struct kobject *kobj, struct kobj_attribute *attr,
                    char *buf)
{
	return sprintf(buf, "%d\n",msm_total_pools_max);
}

static ssize_t
msm_total_pools_max_store(struct kobject *kobj, struct kobj_attribute *attr,
                    const char *buf, size_t count)
{
	int ret;
	int val;

	ret = kstrtoint(buf, 0, &val);
	if (ret)
		return ret;

	if (val < 0)
		return -EINVAL;

	msm_total_pools_max = val;
	printk(KERN_EMERG "the dmabuf_heap_pool_max is %d\n", msm_total_pools_max);

	return count;
}

static struct kobj_attribute msm_total_pools_max_attr =
	__ATTR_RW(msm_total_pools_max);

static struct attribute *dma_heap_sysfs_attrs[] = {
	&msm_total_pools_max_attr.attr,
	NULL,
};

ATTRIBUTE_GROUPS(dma_heap_sysfs);

static struct kobject *dma_heap_pool_kobject;

static int create_total_max_node(void) {
	int ret;

	dma_heap_pool_kobject = kobject_create_and_add("dma_mi_pool", kernel_kobj);
	if (!dma_heap_pool_kobject)
		return -ENOMEM;

	ret = sysfs_create_groups(dma_heap_pool_kobject, dma_heap_sysfs_groups);
	if (ret) {
		kobject_put(dma_heap_pool_kobject);
		return ret;
	}

	return 0;
}

int dynamic_page_pool_init_shrinker(void)
{
	int ret;
	static bool registered;

	if (registered)
		return 0;

	create_total_max_node();

	ret = register_shrinker(&pool_shrinker);
	if (ret)
		return ret;

	registered = true;
	return 0;
}
