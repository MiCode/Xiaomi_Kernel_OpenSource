// SPDX-License-Identifier: GPL-2.0
/*
 * ION Memory Allocator system heap exporter
 *
 * Copyright (C) 2011 Google, Inc.
 */

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "ion_page_pool.h"

#define NUM_ORDERS ARRAY_SIZE(orders)

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = GFP_HIGHUSER | __GFP_ZERO;
#ifdef CONFIG_ARM64
static const unsigned int orders[] = {8, 4, 0};
#else
static const unsigned int orders[] = {4, 1, 0};
#endif

struct page_info {
	struct page *page;
	bool from_pool;
	unsigned int order;
	struct list_head list;
};

static int order_to_index(unsigned int order)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static inline unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool *pools[NUM_ORDERS];
};

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order,
				      bool *from_pool)
{
	struct ion_page_pool *pool = heap->pools[order_to_index(order)];
	struct page *page = ion_page_pool_alloc(pool, from_pool);

	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page)
{
	struct ion_page_pool *pool;
	unsigned int order = compound_order(page);

	/* go to system */
	if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) {
		__free_pages(page, order);
		return;
	}

	pool = heap->pools[order_to_index(order)];

	ion_page_pool_free(pool, page);
}

static struct page_info *alloc_largest_available(struct ion_system_heap *heap,
						struct ion_buffer *buffer,
						unsigned long size,
						unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;
	bool from_pool;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i], &from_pool);
		if (!page)
			continue;
		info = kmalloc(sizeof(*info), GFP_KERNEL);
		if (info) {
			info->page = page;
			info->order = orders[i];
			info->from_pool = from_pool;
		}
		return info;
	}

	return NULL;
}

void set_sg_info(struct page_info *info, struct scatterlist *sg)
{
	sg_set_page(sg, info->page, (1 << info->order) * PAGE_SIZE, 0);
	list_del(&info->list);
	kfree(info);
}

static int ion_system_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size,
				    unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages, pages_from_pool;
	int i = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	struct timespec64 val_start, val_end;
	u64 time_start, time_end;
	unsigned int sz;
	unsigned long pool_sz = 0, buddy_sz = 0;
	unsigned int buddy_orders[NUM_ORDERS] = {0};
	struct page_info *info, *tmp_info;

	if (size / PAGE_SIZE > totalram_pages() / 2)
		return -ENOMEM;

	ktime_get_real_ts64(&val_start);
	time_start = val_start.tv_sec * 1000000LL + val_start.tv_nsec / 1000;
	INIT_LIST_HEAD(&pages);
	INIT_LIST_HEAD(&pages_from_pool);
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining,
					       max_order);
		if (!info)
			goto free_pages;
		sz = (1 << info->order) * PAGE_SIZE;
		if (info->from_pool) {
			pool_sz += sz;
			list_add_tail(&info->list, &pages_from_pool);
		} else {
			int index;

			for (index = 0; index < NUM_ORDERS; index++) {
				if (info->order == orders[index]) {
					buddy_orders[index]++;
					break;
				}
			}
			buddy_sz += sz;
			list_add_tail(&info->list, &pages);
		}
		size_remaining -= sz;
		max_order = info->order;
		i++;
	}
	ktime_get_real_ts64(&val_end);
	time_end = val_end.tv_sec * 1000000LL + val_end.tv_nsec / 1000;
	pr_info("%s,tid:%-5d, size:%8ld, time:%11lldus, pool:%ld, bud: %ld, ord 8:%d, 4:%d, 0:%d\n",
		__func__, current->pid, size, time_end - time_start, pool_sz, buddy_sz,
		buddy_orders[0], buddy_orders[1], buddy_orders[2]);
	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto free_pages;

	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_table;

	sg = table->sgl;
	do {
		info = list_first_entry_or_null(&pages, struct page_info, list);
		tmp_info = list_first_entry_or_null(&pages_from_pool,
						    struct page_info, list);
		if (info && tmp_info) {
			if (info->order >= tmp_info->order)
				set_sg_info(info, sg);
			else
				set_sg_info(tmp_info, sg);
		} else if (info) {
			set_sg_info(info, sg);
		} else if (tmp_info) {
			set_sg_info(tmp_info, sg);
		} else {
			WARN_ON(1);
		}
		sg = sg_next(sg);
	} while (sg);

	buffer->sg_table = table;

	ion_buffer_prep_noncached(buffer);

	return 0;

free_table:
	kfree(table);
free_pages:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page);
		kfree(info);
	}
	list_for_each_entry_safe(info, tmp_info, &pages_from_pool, list) {
		free_buffer_page(sys_heap, buffer, info->page);
		kfree(info);
	}
	return -ENOMEM;
}

static void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_system_heap *sys_heap = container_of(buffer->heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;

	/* zero the buffer before goto page pool */
	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE))
		ion_buffer_zero(buffer);

	for_each_sg(table->sgl, sg, table->orig_nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg));
	sg_free_table(table);
	kfree(table);
}

static int ion_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
				  int nr_to_scan)
{
	struct ion_page_pool *pool;
	struct ion_system_heap *sys_heap;
	int nr_total = 0;
	int i, nr_freed;
	int only_scan = 0;

	sys_heap = container_of(heap, struct ion_system_heap, heap);

	if (!nr_to_scan)
		only_scan = 1;

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->pools[i];

		if (only_scan) {
			nr_total += ion_page_pool_shrink(pool,
							 gfp_mask,
							 nr_to_scan);

		} else {
			nr_freed = ion_page_pool_shrink(pool,
							gfp_mask,
							nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
	}
	return nr_total;
}

static long ion_system_get_pool_size(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap;
	long total_pages = 0;
	int i;

	sys_heap = container_of(heap, struct ion_system_heap, heap);
	for (i = 0; i < NUM_ORDERS; i++)
		total_pages += ion_page_pool_nr_pages(sys_heap->pools[i]);

	return total_pages;
}

static void ion_system_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (pools[i])
			ion_page_pool_destroy(pools[i]);
}

static int ion_system_heap_create_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		/*
		 * Enable NOWARN on larger order allocations, as
		 * we will fall back to 0-order if things fail.
		 * This avoids warning noise in dmesg.
		 */
		if (orders[i] > 0)
			gfp_flags |= __GFP_NOWARN;

		if (orders[i])
			gfp_flags = high_order_gfp_flags;
#ifdef CONFIG_ARM64
		if (orders[i] == 4)
			gfp_flags |= __GFP_KSWAPD_RECLAIM;
#endif

		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		pools[i] = pool;
	}

	return 0;

err_create_pool:
	ion_system_heap_destroy_pools(pools);
	return -ENOMEM;
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.shrink = ion_system_heap_shrink,
	.get_pool_size = ion_system_get_pool_size,
};

static struct ion_system_heap system_heap = {
	.heap = {
		.ops = &system_heap_ops,
		.type = ION_HEAP_TYPE_SYSTEM,
		.flags = ION_HEAP_FLAG_DEFER_FREE,
		.name = "ion_system_heap",
	}
};

static int __init ion_system_heap_init(void)
{
	int ret = ion_system_heap_create_pools(system_heap.pools);
	if (ret)
		return ret;

	return ion_device_add_heap(&system_heap.heap);
}

static void __exit ion_system_heap_exit(void)
{
	ion_device_remove_heap(&system_heap.heap);
	ion_system_heap_destroy_pools(system_heap.pools);
}

subsys_initcall(ion_system_heap_init);
module_exit(ion_system_heap_exit);
MODULE_LICENSE("GPL v2");
