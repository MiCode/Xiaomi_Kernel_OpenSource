/*
 * drivers/gpu/ion/ion_mem_pool.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion_priv.h"

struct ion_page_pool_item {
	struct page *page;
	struct list_head list;
};

static void *ion_page_pool_alloc_pages(struct ion_page_pool *pool)
{
	struct page *page;
	struct scatterlist sg;

	page = alloc_pages(pool->gfp_mask & ~__GFP_ZERO, pool->order);

	if (!page)
		return NULL;

	if (pool->gfp_mask & __GFP_ZERO)
		if (ion_heap_high_order_page_zero(
				page, pool->order, pool->should_invalidate))
			goto error_free_pages;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, PAGE_SIZE << pool->order, 0);
	sg_dma_address(&sg) = sg_phys(&sg);
	dma_sync_sg_for_device(NULL, &sg, 1, DMA_BIDIRECTIONAL);

	return page;
error_free_pages:
	__free_pages(page, pool->order);
	return NULL;
}

static void ion_page_pool_free_pages(struct ion_page_pool *pool,
				     struct page *page)
{
	__free_pages(page, pool->order);
}

static int ion_page_pool_add(struct ion_page_pool *pool, struct page *page)
{
	struct ion_page_pool_item *item;

	item = kmalloc(sizeof(struct ion_page_pool_item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;

	mutex_lock(&pool->mutex);
	item->page = page;
	if (PageHighMem(page)) {
		list_add_tail(&item->list, &pool->high_items);
		pool->high_count++;
	} else {
		list_add_tail(&item->list, &pool->low_items);
		pool->low_count++;
	}
	mutex_unlock(&pool->mutex);
	return 0;
}

static struct page *ion_page_pool_remove(struct ion_page_pool *pool, bool high)
{
	struct ion_page_pool_item *item;
	struct page *page;

	if (high) {
		BUG_ON(!pool->high_count);
		item = list_first_entry(&pool->high_items,
					struct ion_page_pool_item, list);
		pool->high_count--;
	} else {
		BUG_ON(!pool->low_count);
		item = list_first_entry(&pool->low_items,
					struct ion_page_pool_item, list);
		pool->low_count--;
	}

	list_del(&item->list);
	page = item->page;
	kfree(item);
	return page;
}

void *ion_page_pool_alloc(struct ion_page_pool *pool)
{
	struct page *page = NULL;

	BUG_ON(!pool);

	mutex_lock(&pool->mutex);
	if (pool->high_count)
		page = ion_page_pool_remove(pool, true);
	else if (pool->low_count)
		page = ion_page_pool_remove(pool, false);
	mutex_unlock(&pool->mutex);

	if (!page)
		page = ion_page_pool_alloc_pages(pool);

	return page;
}

void ion_page_pool_free(struct ion_page_pool *pool, struct page* page)
{
	int ret;

	ret = ion_page_pool_add(pool, page);
	if (ret)
		ion_page_pool_free_pages(pool, page);
}

static int ion_page_pool_total(struct ion_page_pool *pool, bool high)
{
	int total = 0;

	total += high ? (pool->high_count + pool->low_count) *
		(1 << pool->order) :
			pool->low_count * (1 << pool->order);
	return total;
}

int ion_page_pool_shrink(struct ion_page_pool *pool, gfp_t gfp_mask,
				int nr_to_scan)
{
	int nr_freed = 0;
	int i;
	bool high;

	high = gfp_mask & __GFP_HIGHMEM;

	if (nr_to_scan == 0)
		return ion_page_pool_total(pool, high);

	for (i = 0; i < nr_to_scan; i++) {
		struct page *page;

		mutex_lock(&pool->mutex);
		if (high && pool->high_count) {
			page = ion_page_pool_remove(pool, true);
		} else if (pool->low_count) {
			page = ion_page_pool_remove(pool, false);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		ion_page_pool_free_pages(pool, page);
		nr_freed += (1 << pool->order);
	}

	return nr_freed;
}

struct ion_page_pool *ion_page_pool_create(gfp_t gfp_mask, unsigned int order,
	bool should_invalidate)
{
	struct ion_page_pool *pool = kmalloc(sizeof(struct ion_page_pool),
					     GFP_KERNEL);
	if (!pool)
		return NULL;
	pool->high_count = 0;
	pool->low_count = 0;
	INIT_LIST_HEAD(&pool->low_items);
	INIT_LIST_HEAD(&pool->high_items);
	pool->gfp_mask = gfp_mask;
	pool->order = order;
	pool->should_invalidate = should_invalidate;
	mutex_init(&pool->mutex);
	plist_node_init(&pool->list, order);

	return pool;
}

void ion_page_pool_destroy(struct ion_page_pool *pool)
{
	kfree(pool);
}

static int __init ion_page_pool_init(void)
{
	return 0;
}

static void __exit ion_page_pool_exit(void)
{
}

module_init(ion_page_pool_init);
module_exit(ion_page_pool_exit);
