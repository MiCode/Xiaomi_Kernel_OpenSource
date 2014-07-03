/*
 * drivers/gpu/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#include <asm/page.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion_priv.h"
#include <linux/dma-mapping.h>
#include <trace/events/kmem.h>

static unsigned int high_order_gfp_flags = (GFP_HIGHUSER |
					    __GFP_NOWARN | __GFP_NORETRY |
					    __GFP_NO_KSWAPD) & ~__GFP_WAIT;
static unsigned int low_order_gfp_flags  = (GFP_HIGHUSER |
					 __GFP_NOWARN);
static const unsigned int orders[] = {9, 8, 4, 0};
static const int num_orders = ARRAY_SIZE(orders);
static int order_to_index(unsigned int order)
{
	int i;
	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return -1;
}

static unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

struct ion_system_heap {
	struct ion_heap heap;
	struct ion_page_pool **uncached_pools;
	struct ion_page_pool **cached_pools;
};

struct page_info {
	struct page *page;
	bool from_pool;
	unsigned int order;
	struct list_head list;
};

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order,
				      bool *from_pool)
{
	bool cached = ion_buffer_cached(buffer);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	struct page *page;
	struct ion_page_pool *pool;

	if (!cached)
		pool = heap->uncached_pools[order_to_index(order)];
	else
		pool = heap->cached_pools[order_to_index(order)];
	page = ion_page_pool_alloc(pool, from_pool);
	if (!page)
		return 0;

	if (split_pages)
		split_page(page, order);
	return page;
}

static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page,
			     unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
	bool split_pages = ion_buffer_fault_user_mappings(buffer);
	int i;

	if ((buffer->flags & ION_FLAG_FREED_FROM_SHRINKER)) {
		if (split_pages) {
			for (i = 0; i < (1 << order); i++)
				__free_page(page + i);
		} else {
			__free_pages(page, order);
		}
	} else  {
		struct ion_page_pool *pool;
		if (cached)
			pool = heap->cached_pools[order_to_index(order)];
		else
			pool = heap->uncached_pools[order_to_index(order)];
		ion_page_pool_free(pool, page);
	}
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
	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_buffer_page(heap, buffer, orders[i], &from_pool);
		if (!page)
			continue;

		info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
		if (info) {
			info->page = page;
			info->order = orders[i];
			info->from_pool = from_pool;
		}
		return info;
	}
	return NULL;
}
static unsigned int process_info(struct page_info *info,
				 struct scatterlist *sg,
				 struct scatterlist *sg_sync,
				 struct pages_mem *data, unsigned int i)
{
	struct page *page = info->page;
	unsigned int j;

	if (sg_sync) {
		sg_set_page(sg_sync, page, (1 << info->order) * PAGE_SIZE, 0);
		sg_dma_address(sg_sync) = page_to_phys(page);
	}
	sg_set_page(sg, page, (1 << info->order) * PAGE_SIZE, 0);
	/*
	 * This is not correct - sg_dma_address needs a dma_addr_t
	 * that is valid for the the targeted device, but this works
	 * on the currently targeted hardware.
	 */
	sg_dma_address(sg) = page_to_phys(page);
	if (data) {
		for (j = 0; j < (1 << info->order); ++j)
			data->pages[i++] = nth_page(page, j);
	}
	list_del(&info->list);
	kfree(info);
	return i;
}

static int ion_system_heap_allocate(struct ion_heap *heap,
				     struct ion_buffer *buffer,
				     unsigned long size, unsigned long align,
				     unsigned long flags)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table;
	struct sg_table table_sync;
	struct scatterlist *sg;
	struct scatterlist *sg_sync;
	int ret;
	struct list_head pages;
	struct list_head pages_from_pool;
	struct page_info *info, *tmp_info;
	int i = 0;
	unsigned int nents_sync = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	struct pages_mem data;
	unsigned int sz;
	bool split_pages = ion_buffer_fault_user_mappings(buffer);

	data.size = 0;
	INIT_LIST_HEAD(&pages);
	INIT_LIST_HEAD(&pages_from_pool);
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining, max_order);
		if (!info)
			goto err;

		sz = (1 << info->order) * PAGE_SIZE;

		if (info->from_pool) {
			list_add_tail(&info->list, &pages_from_pool);
		} else {
			list_add_tail(&info->list, &pages);
			data.size += sz;
			++nents_sync;
		}
		size_remaining -= sz;
		max_order = info->order;
		i++;
	}

	ret = ion_heap_alloc_pages_mem(&data);

	if (ret)
		goto err;

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto err_free_data_pages;

	if (split_pages)
		ret = sg_alloc_table(table, PAGE_ALIGN(size) / PAGE_SIZE,
				     GFP_KERNEL);
	else
		ret = sg_alloc_table(table, i, GFP_KERNEL);

	if (ret)
		goto err1;

	if (nents_sync) {
		ret = sg_alloc_table(&table_sync, nents_sync, GFP_KERNEL);
		if (ret)
			goto err_free_sg;
	}

	i = 0;
	sg = table->sgl;
	sg_sync = table_sync.sgl;

	/*
	 * We now have two separate lists. One list contains pages from the
	 * pool and the other pages from buddy. We want to merge these
	 * together while preserving the ordering of the pages (higher order
	 * first).
	 */
	do {
		if (!list_empty(&pages))
			info = list_first_entry(&pages, struct page_info, list);
		else
			info = NULL;
		if (!list_empty(&pages_from_pool))
			tmp_info = list_first_entry(&pages_from_pool,
							struct page_info, list);
		else
			tmp_info = NULL;

		if (info && tmp_info) {
			if (info->order >= tmp_info->order) {
				i = process_info(info, sg, sg_sync, &data, i);
				sg_sync = sg_next(sg_sync);
			} else {
				i = process_info(tmp_info, sg, 0, 0, i);
			}
		} else if (info) {
			i = process_info(info, sg, sg_sync, &data, i);
			sg_sync = sg_next(sg_sync);
		} else if (tmp_info) {
			i = process_info(tmp_info, sg, 0, 0, i);
		} else {
			BUG();
		}
		sg = sg_next(sg);

	} while (sg);

	ret = ion_heap_pages_zero(data.pages, data.size >> PAGE_SHIFT);
	if (ret) {
		pr_err("Unable to zero pages\n");
		goto err_free_sg2;
	}

	if (nents_sync)
		dma_sync_sg_for_device(NULL, table_sync.sgl, table_sync.nents,
				       DMA_BIDIRECTIONAL);

	buffer->priv_virt = table;
	if (nents_sync)
		sg_free_table(&table_sync);
	ion_heap_free_pages_mem(&data);
	return 0;
err_free_sg2:
	/* We failed to zero buffers. Bypass pool */
	buffer->flags |= ION_FLAG_FREED_FROM_SHRINKER;

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg->length));
	if (nents_sync)
		sg_free_table(&table_sync);
err_free_sg:
	sg_free_table(table);
err1:
	kfree(table);
err_free_data_pages:
	ion_heap_free_pages_mem(&data);
err:
	list_for_each_entry_safe(info, tmp_info, &pages, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	list_for_each_entry_safe(info, tmp_info, &pages_from_pool, list) {
		free_buffer_page(sys_heap, buffer, info->page, info->order);
		kfree(info);
	}
	return -ENOMEM;
}

void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	LIST_HEAD(pages);
	int i;

	if (!(buffer->flags & ION_FLAG_FREED_FROM_SHRINKER))
		ion_heap_buffer_zero(buffer);

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg_dma_len(sg)));
	sg_free_table(table);
	kfree(table);
}

struct sg_table *ion_system_heap_map_dma(struct ion_heap *heap,
					 struct ion_buffer *buffer)
{
	return buffer->priv_virt;
}

void ion_system_heap_unmap_dma(struct ion_heap *heap,
			       struct ion_buffer *buffer)
{
	return;
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
};

static int ion_system_heap_shrink(struct shrinker *shrinker,
				  struct shrink_control *sc) {

	struct ion_heap *heap = container_of(shrinker, struct ion_heap,
					     shrinker);
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int nr_total = 0;
	int nr_freed = 0;
	int i;

	if (sc->nr_to_scan == 0)
		goto end;

	/* shrink the free list first, no point in zeroing the memory if
	   we're just going to reclaim it. Also, skip any possible
	   page pooling */
	nr_freed += ion_heap_freelist_drain_from_shrinker(
		heap, sc->nr_to_scan * PAGE_SIZE) / PAGE_SIZE;

	if (nr_freed >= sc->nr_to_scan)
		goto end;

	for (i = 0; i < num_orders; i++) {
		nr_freed += ion_page_pool_shrink(sys_heap->uncached_pools[i],
						sc->gfp_mask, sc->nr_to_scan);
		if (nr_freed >= sc->nr_to_scan)
			goto end;

		nr_freed += ion_page_pool_shrink(sys_heap->cached_pools[i],
						sc->gfp_mask, sc->nr_to_scan);
		if (nr_freed >= sc->nr_to_scan)
			goto end;
	}

end:
	/* total number of items is whatever the page pools are holding
	   plus whatever's in the freelist */
	for (i = 0; i < num_orders; i++) {
		nr_total += ion_page_pool_shrink(
			sys_heap->uncached_pools[i], sc->gfp_mask, 0);
		nr_total += ion_page_pool_shrink(
			sys_heap->cached_pools[i], sc->gfp_mask, 0);
	}
	nr_total += ion_heap_freelist_size(heap) / PAGE_SIZE;
	return nr_total;

}

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	int i;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->uncached_pools[i];
		seq_printf(s,
			"%d order %u highmem pages in uncached pool = %lu total\n",
			pool->high_count, pool->order,
			(1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s,
			"%d order %u lowmem pages in uncached pool = %lu total\n",
			pool->low_count, pool->order,
			(1 << pool->order) * PAGE_SIZE * pool->low_count);
	}

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = sys_heap->cached_pools[i];
		seq_printf(s,
			"%d order %u highmem pages in cached pool = %lu total\n",
			pool->high_count, pool->order,
			(1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s,
			"%d order %u lowmem pages in cached pool = %lu total\n",
			pool->low_count, pool->order,
			(1 << pool->order) * PAGE_SIZE * pool->low_count);
	}

	return 0;
}


static void ion_system_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;
	for (i = 0; i < num_orders; i++)
		if (pools[i])
			ion_page_pool_destroy(pools[i]);
}

/**
 * ion_system_heap_create_pools - Creates pools for all orders
 *
 * If this fails you don't need to destroy any pools. It's all or
 * nothing. If it succeeds you'll eventually need to use
 * ion_system_heap_destroy_pools to destroy the pools.
 */
static int ion_system_heap_create_pools(struct ion_page_pool **pools)
{
	int i;
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i])
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_pool;
		pools[i] = pool;
	}
	return 0;
err_create_pool:
	ion_system_heap_destroy_pools(pools);
	return 1;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_system_heap *heap;
	int pools_size = sizeof(struct ion_page_pool *) * num_orders;

	heap = kzalloc(sizeof(struct ion_system_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	heap->uncached_pools = kzalloc(pools_size, GFP_KERNEL);
	if (!heap->uncached_pools)
		goto err_alloc_uncached_pools;

	heap->cached_pools = kzalloc(pools_size, GFP_KERNEL);
	if (!heap->cached_pools)
		goto err_alloc_cached_pools;

	if (ion_system_heap_create_pools(heap->uncached_pools))
		goto err_create_uncached_pools;

	if (ion_system_heap_create_pools(heap->cached_pools))
		goto err_create_cached_pools;

	heap->heap.shrinker.shrink = ion_system_heap_shrink;
	heap->heap.shrinker.seeks = DEFAULT_SEEKS;
	heap->heap.shrinker.batch = 0;
	register_shrinker(&heap->heap.shrinker);
	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;

err_create_cached_pools:
	ion_system_heap_destroy_pools(heap->uncached_pools);
err_create_uncached_pools:
	kfree(heap->cached_pools);
err_alloc_cached_pools:
	kfree(heap->uncached_pools);
err_alloc_uncached_pools:
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);

	ion_system_heap_destroy_pools(sys_heap->uncached_pools);
	ion_system_heap_destroy_pools(sys_heap->cached_pools);
	kfree(sys_heap->uncached_pools);
	kfree(sys_heap->cached_pools);
	kfree(sys_heap);
}

struct kmalloc_buffer_info {
	struct sg_table *table;
	void *vaddr;
};

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	int ret;
	struct kmalloc_buffer_info *info;

	info = kmalloc(sizeof(struct kmalloc_buffer_info), GFP_KERNEL);
	if (!info) {
		ret = -ENOMEM;
		goto out;
	}

	info->table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!info->table) {
		ret = -ENOMEM;
		goto kfree_info;
	}

	ret = sg_alloc_table(info->table, 1, GFP_KERNEL);
	if (ret)
		goto kfree_table;

	info->vaddr = kzalloc(len, GFP_KERNEL);
	if (!info->vaddr) {
		ret = -ENOMEM;
		goto sg_free_table;
	}

	sg_set_page(info->table->sgl, virt_to_page(info->vaddr), len,
		    0);
	sg_dma_address(info->table->sgl) = virt_to_phys(info->vaddr);
	dma_sync_sg_for_device(NULL, info->table->sgl, 1, DMA_BIDIRECTIONAL);

	buffer->priv_virt = info;
	return 0;

sg_free_table:
	sg_free_table(info->table);
kfree_table:
	kfree(info->table);
kfree_info:
	kfree(info);
out:
	return ret;
}

void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	struct kmalloc_buffer_info *info = buffer->priv_virt;
	sg_free_table(info->table);
	kfree(info->table);
	kfree(info->vaddr);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	struct kmalloc_buffer_info *info = buffer->priv_virt;
	*addr = virt_to_phys(info->vaddr);
	*len = buffer->size;
	return 0;
}

struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	struct kmalloc_buffer_info *info = buffer->priv_virt;
	return info->table;
}

void ion_system_contig_heap_unmap_dma(struct ion_heap *heap,
				      struct ion_buffer *buffer)
{
}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_contig_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

