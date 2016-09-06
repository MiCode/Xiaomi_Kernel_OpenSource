/* Copyright (c) 2008-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/fb.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fdtable.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/dma-buf.h>
#include <linux/pm_runtime.h>
#include <linux/rbtree.h>
#include <linux/ashmem.h>
#include <linux/major.h>
#include <linux/io.h>
#include <linux/mman.h>
#include <linux/sort.h>
#include <linux/security.h>
#include <linux/compat.h>
#include <asm/cacheflush.h>

#include "kgsl.h"
#include "kgsl_debugfs.h"
#include "kgsl_cffdump.h"
#include "kgsl_log.h"
#include "kgsl_sharedmem.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include "kgsl_sync.h"
#include "adreno.h"
#include "kgsl_compat.h"
#include "kgsl_trace.h"

#undef MODULE_PARAM_PREFIX
#define MODULE_PARAM_PREFIX "kgsl."

static gfp_t high_order_gfp_flags = (__GFP_HIGHMEM | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_WAIT;
static gfp_t low_order_gfp_flags  = (__GFP_HIGHMEM | __GFP_ZERO | __GFP_NOWARN |
				     GFP_KERNEL);


static const unsigned int orders[] = {8, 4, 1, 0};

static const unsigned int orders_reserved[] = {32, 256, 1024, 2048};


static const bool reserve_only[] = {true, true, false, false};

static const int num_orders = ARRAY_SIZE(orders);

struct kgsl_page_pool {
	bool reserve_only;
	unsigned int reserve_count;
	int count;
	int alloc_count;
	struct list_head items;
	struct mutex mutex;
	gfp_t gfp_mask;
	unsigned int order;
	struct plist_node list;
};

struct kgsl_heap {
	struct shrinker shrinker;
	struct kgsl_page_pool *pools[4];
};

struct kgsl_heap kgsl_heap;

static void *kgsl_page_pool_alloc_pages(struct kgsl_page_pool *pool)
{
	struct page *page;
	trace_kgsl_page_pool_alloc_pages_begin(pool->order);
	page = alloc_pages(pool->gfp_mask, pool->order);
	pool->alloc_count++;
	trace_kgsl_page_pool_alloc_pages_end(pool->order, page);
	if (!page)
		return NULL;
	return page;
}

static void kgsl_page_pool_free_pages(struct kgsl_page_pool *pool,
				     struct page *page)
{
	__free_pages(page, pool->order);
	pool->alloc_count--;
}

static void kgsl_page_pool_zero(struct kgsl_page_pool *pool, struct page *page)
{
	int i;
	trace_kgsl_page_pool_zero_begin(pool->order);
	for (i = 0; i < (1 << pool->order); i++) {
		struct page *p;
		void *kaddr;
		p = nth_page(page, i);
		kaddr = kmap_atomic(p);
		clear_page(kaddr);
		dmac_flush_range(kaddr, kaddr + PAGE_SIZE);
		kunmap_atomic(kaddr);
	}
	trace_kgsl_page_pool_zero_end(pool->order);

}

static int kgsl_page_pool_add(struct kgsl_page_pool *pool, struct page *page)
{
	mutex_lock(&pool->mutex);
	list_add_tail(&page->lru, &pool->items);
	pool->count++;
	mutex_unlock(&pool->mutex);
	return 0;
}

static struct page *kgsl_page_pool_remove(struct kgsl_page_pool *pool)
{
	struct page *page;
	BUG_ON(!pool->count);
	page = list_first_entry(&pool->items, struct page, lru);
	pool->count--;
	list_del(&page->lru);
	return page;
}

static struct page *kgsl_page_pool_alloc(struct kgsl_page_pool *pool)
{
	struct page *page = NULL;
	trace_kgsl_page_pool_alloc_begin(pool->order);
	BUG_ON(!pool);
	mutex_lock(&pool->mutex);
	if (pool->count)
		page = kgsl_page_pool_remove(pool);
	mutex_unlock(&pool->mutex);

	if (!page && !pool->reserve_only) {

		page = kgsl_page_pool_alloc_pages(pool);
	}


	if (page)
		kgsl_page_pool_zero(pool, page);

	trace_kgsl_page_pool_alloc_end(pool->order);

	return page;
}

static void kgsl_page_pool_free(struct kgsl_page_pool *pool, struct page *page)
{
	int ret;
	BUG_ON(pool->order != compound_order(page));

	ret = kgsl_page_pool_add(pool, page);
	if (ret)
		kgsl_page_pool_free_pages(pool, page);
}

static int kgsl_page_pool_total(struct kgsl_page_pool *pool)
{
	if (pool->reserve_only)
		return 0;

	if (pool->count >= pool->reserve_count)
		return (pool->count - pool->reserve_count) << pool->order;
	else
		return 0;
}

static int kgsl_page_pool_shrink(struct kgsl_page_pool *pool, gfp_t gfp_mask,
				 int nr_to_scan)
{
	int freed = 0;
	if (nr_to_scan == 0)
		return kgsl_page_pool_total(pool);

	if (pool->reserve_only)
		return 0;

	while (freed < nr_to_scan) {
		struct page *page;

		mutex_lock(&pool->mutex);
		if (pool->count > pool->reserve_count) {
			page = kgsl_page_pool_remove(pool);
		} else {
			mutex_unlock(&pool->mutex);
			break;
		}
		mutex_unlock(&pool->mutex);
		kgsl_page_pool_free_pages(pool, page);
		freed += (1 << pool->order);
	}

	return freed;
}

static void kgsl_page_pool_destroy(struct kgsl_page_pool *pool)
{
	kfree(pool);
}

static struct kgsl_page_pool *kgsl_page_pool_create(gfp_t gfp_mask, unsigned int order,
						    unsigned int reserve, bool reserve_only)
{
	struct kgsl_page_pool *pool = kmalloc(sizeof(struct kgsl_page_pool),
					     GFP_KERNEL);
	if (!pool)
		return NULL;
	pool->reserve_only = reserve_only;
	pool->count = 0;
	pool->alloc_count = 0;
	pool->reserve_count = 0;
	INIT_LIST_HEAD(&pool->items);
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	mutex_init(&pool->mutex);
	plist_node_init(&pool->list, order);

	if (reserve) {
		unsigned int i;
		for (i = 0; i < reserve; i++) {
			struct page *page = kgsl_page_pool_alloc_pages(pool);
			if (page == NULL) {


				kgsl_page_pool_destroy(pool);
				return NULL;
			}
			kgsl_page_pool_add(pool, page);
		}

		pool->reserve_count = reserve;

	}

	return pool;
}

static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -EPERM;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

static struct page *alloc_buffer_page(unsigned long order)
{
	struct kgsl_page_pool *pool = kgsl_heap.pools[order_to_index(order)];
	struct page *page;

	page = kgsl_page_pool_alloc(pool);
	return page;
}

static void free_buffer_page(struct page *page, unsigned int order)
{
	struct kgsl_page_pool *pool;
	pool = kgsl_heap.pools[order_to_index(order)];
	kgsl_page_pool_free(pool, page);
}

struct page *kgsl_heap_alloc(unsigned long size)
{
	struct page *page;
	int i;

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;

		page = alloc_buffer_page(orders[i]);
		if (!page)
			continue;

		return page;
	}


	return NULL;
}

void kgsl_heap_free(struct page *page)
{
	unsigned int order = compound_order(page);
	free_buffer_page(page, order);
}

static unsigned long kgsl_heap_shrink_count(struct shrinker *shrinker, struct shrink_control *sc)
{
	int i;
	int nr_total = 0;

	for (i = 0; i < num_orders; i++) {
		struct kgsl_page_pool *pool = kgsl_heap.pools[i];
		nr_total += kgsl_page_pool_total(pool);
	}

	return nr_total;
}

static unsigned long kgsl_heap_shrink_scan(struct shrinker *shrinker, struct shrink_control *sc)
{
	int nr_to_scan = sc->nr_to_scan;
	int nr_total = 0;
	int i, nr_freed;

	if (nr_to_scan == 0)
		return 0;

	for (i = 0; i < num_orders; i++) {
		struct kgsl_page_pool *pool = kgsl_heap.pools[i];

		nr_freed = kgsl_page_pool_shrink(pool, sc->gfp_mask, nr_to_scan);

		nr_total += nr_freed;
		if (nr_total >= nr_to_scan)
			break;
	}

	return nr_total;
}


int kgsl_pool_total_pages(void)
{
	int i;
	int nr_total = 0;

	for (i = 0; i < num_orders; i++) {
		struct kgsl_page_pool *pool = kgsl_heap.pools[i];
		nr_total += pool->alloc_count << pool->order;
	}
	return nr_total;
}

int kgsl_pool_total_reserved_pages(void)
{
	int i;
	int nr_total = 0;

	for (i = 0; i < num_orders; i++) {
		struct kgsl_page_pool *pool = kgsl_heap.pools[i];
		nr_total += pool->reserve_count  << pool->order;
	}
	return nr_total;
}

int kgsl_pool_info(char *buf)
{
	int i;
	char tmp_buf[200];

	sprintf(buf, "total pages %d, reserved pages %d\n",
				kgsl_pool_total_pages(),
				kgsl_pool_total_reserved_pages());

	strcat(buf, "\nindex  order  flag  reserved  count  alloc\n");
	for (i = 0; i < num_orders; i++) {
		struct kgsl_page_pool *pool = kgsl_heap.pools[i];
		sprintf(tmp_buf, "%-5d  %-5d  %-4d  %-8d  %-5d  %-5d\n",
				i,
				pool->order,
				pool->reserve_only,
				pool->reserve_count,
				pool->count,
				pool->alloc_count);
		strcat(buf, tmp_buf);
	}
	return strlen(buf);
}

int kgsl_heap_init(void)
{
	int i;

	for (i = 0; i < num_orders; i++) {
		struct kgsl_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i] > 4)
			gfp_flags = high_order_gfp_flags;
		pool = kgsl_page_pool_create(gfp_flags, orders[i], orders_reserved[i], reserve_only[i]);
		if (!pool)
			goto destroy_pools;
		kgsl_heap.pools[i] = pool;
	}

	kgsl_heap.shrinker.count_objects = kgsl_heap_shrink_count;
	kgsl_heap.shrinker.scan_objects = kgsl_heap_shrink_scan;
	kgsl_heap.shrinker.seeks = DEFAULT_SEEKS;
	kgsl_heap.shrinker.batch = 0;
	register_shrinker(&kgsl_heap.shrinker);

	return 0;

destroy_pools:
	while (i--)
		kgsl_page_pool_destroy(kgsl_heap.pools[i]);
	return -ENOMEM;
}
