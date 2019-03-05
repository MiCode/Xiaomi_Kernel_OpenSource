/*
 * drivers/staging/android/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
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
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <soc/qcom/secure_buffer.h>
#include "ion_system_heap.h"
#include "ion.h"
#include "ion_system_heap.h"
#include "ion_system_secure_heap.h"
#include "ion_secure_util.h"

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_ZERO | __GFP_NOWARN |
				     __GFP_NORETRY) & ~__GFP_RECLAIM;
static gfp_t low_order_gfp_flags  = GFP_HIGHUSER | __GFP_ZERO;

int order_to_index(unsigned int order)
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

struct pages_mem {
	struct page **pages;
	u32 size;
};

int ion_heap_is_system_heap_type(enum ion_heap_type type)
{
	return type == ((enum ion_heap_type)ION_HEAP_TYPE_SYSTEM);
}

static struct page *alloc_buffer_page(struct ion_system_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long order,
				      bool *from_pool)
{
	bool cached = ion_buffer_cached(buffer);
	struct page *page;
	struct ion_page_pool *pool;
	int vmid = get_secure_vmid(buffer->flags);
	struct device *dev = heap->heap.priv;

	if (vmid > 0)
		pool = heap->secure_pools[vmid][order_to_index(order)];
	else if (!cached)
		pool = heap->uncached_pools[order_to_index(order)];
	else
		pool = heap->cached_pools[order_to_index(order)];

	page = ion_page_pool_alloc(pool, from_pool);

	if (IS_ERR(page))
		return page;

	if ((MAKE_ION_ALLOC_DMA_READY && vmid <= 0) || !(*from_pool))
		ion_pages_sync_for_device(dev, page, PAGE_SIZE << order,
					  DMA_BIDIRECTIONAL);

	return page;
}

/*
 * For secure pages that need to be freed and not added back to the pool; the
 *  hyp_unassign should be called before calling this function
 */
void free_buffer_page(struct ion_system_heap *heap,
		      struct ion_buffer *buffer, struct page *page,
		      unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
	int vmid = get_secure_vmid(buffer->flags);

	if (!(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)) {
		struct ion_page_pool *pool;

		if (vmid > 0)
			pool = heap->secure_pools[vmid][order_to_index(order)];
		else if (cached)
			pool = heap->cached_pools[order_to_index(order)];
		else
			pool = heap->uncached_pools[order_to_index(order)];

		if (buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE)
			ion_page_pool_free_immediate(pool, page);
		else
			ion_page_pool_free(pool, page);
	} else {
		__free_pages(page, order);
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

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;
		from_pool = !(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC);
		page = alloc_buffer_page(heap, buffer, orders[i], &from_pool);
		if (IS_ERR(page))
			continue;

		info->page = page;
		info->order = orders[i];
		info->from_pool = from_pool;
		INIT_LIST_HEAD(&info->list);
		return info;
	}
	kfree(info);

	return ERR_PTR(-ENOMEM);
}

static struct page_info *alloc_from_pool_preferred(
		struct ion_system_heap *heap, struct ion_buffer *buffer,
		unsigned long size, unsigned int max_order)
{
	struct page *page;
	struct page_info *info;
	int i;

	if (buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)
		goto force_alloc;

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		page = alloc_from_secure_pool_order(heap, buffer, orders[i]);
		if (IS_ERR(page))
			continue;

		info->page = page;
		info->order = orders[i];
		info->from_pool = true;
		INIT_LIST_HEAD(&info->list);
		return info;
	}

	page = split_page_from_secure_pool(heap, buffer);
	if (!IS_ERR(page)) {
		info->page = page;
		info->order = 0;
		info->from_pool = true;
		INIT_LIST_HEAD(&info->list);
		return info;
	}

	kfree(info);
force_alloc:
	return alloc_largest_available(heap, buffer, size, max_order);
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

static int ion_heap_alloc_pages_mem(struct pages_mem *pages_mem)
{
	struct page **pages;
	unsigned int page_tbl_size;

	page_tbl_size = sizeof(struct page *) * (pages_mem->size >> PAGE_SHIFT);
	if (page_tbl_size > SZ_8K) {
		/*
		 * Do fallback to ensure we have a balance between
		 * performance and availability.
		 */
		pages = kmalloc(page_tbl_size,
				__GFP_COMP | __GFP_NORETRY |
				__GFP_NOWARN);
		if (!pages)
			pages = vmalloc(page_tbl_size);
	} else {
		pages = kmalloc(page_tbl_size, GFP_KERNEL);
	}

	if (!pages)
		return -ENOMEM;

	pages_mem->pages = pages;
	return 0;
}

static void ion_heap_free_pages_mem(struct pages_mem *pages_mem)
{
	kvfree(pages_mem->pages);
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
	struct sg_table table_sync = {0};
	struct scatterlist *sg;
	struct scatterlist *sg_sync;
	int ret = -ENOMEM;
	struct list_head pages;
	struct list_head pages_from_pool;
	struct page_info *info, *tmp_info;
	int i = 0;
	unsigned int nents_sync = 0;
	unsigned long size_remaining = PAGE_ALIGN(size);
	unsigned int max_order = orders[0];
	struct pages_mem data;
	unsigned int sz;
	int vmid = get_secure_vmid(buffer->flags);

	if (size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

	if (ion_heap_is_system_heap_type(buffer->heap->type) &&
	    is_secure_vmid_valid(vmid)) {
		pr_info("%s: System heap doesn't support secure allocations\n",
			__func__);
		return -EINVAL;
	}

	data.size = 0;
	INIT_LIST_HEAD(&pages);
	INIT_LIST_HEAD(&pages_from_pool);

	while (size_remaining > 0) {
		if (is_secure_vmid_valid(vmid))
			info = alloc_from_pool_preferred(
					sys_heap, buffer, size_remaining,
					max_order);
		else
			info = alloc_largest_available(
					sys_heap, buffer, size_remaining,
					max_order);

		if (IS_ERR(info)) {
			ret = PTR_ERR(info);
			goto err;
		}

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

	table = kzalloc(sizeof(*table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto err_free_data_pages;
	}

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
		info = list_first_entry_or_null(&pages, struct page_info, list);
		tmp_info = list_first_entry_or_null(&pages_from_pool,
						    struct page_info, list);
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
		}
		sg = sg_next(sg);

	} while (sg);

	if (nents_sync) {
		if (vmid > 0) {
			ret = ion_hyp_assign_sg(&table_sync, &vmid, 1, true);
			if (ret)
				goto err_free_sg2;
		}
	}

	buffer->sg_table = table;
	if (nents_sync)
		sg_free_table(&table_sync);
	ion_heap_free_pages_mem(&data);
	return 0;

err_free_sg2:
	/* We failed to zero buffers. Bypass pool */
	buffer->private_flags |= ION_PRIV_FLAG_SHRINKER_FREE;

	if (vmid > 0)
		ion_hyp_unassign_sg(table, &vmid, 1, true, false);

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
	return ret;
}

void ion_system_heap_free(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	struct sg_table *table = buffer->sg_table;
	struct scatterlist *sg;
	int i;
	int vmid = get_secure_vmid(buffer->flags);

	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) &&
	    !(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)) {
		if (vmid < 0)
			ion_heap_buffer_zero(buffer);
	} else if (vmid > 0) {
		if (ion_hyp_unassign_sg(table, &vmid, 1, true, false))
			return;
	}

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				 get_order(sg->length));
	sg_free_table(table);
	kfree(table);
}

static int ion_system_heap_shrink(struct ion_heap *heap, gfp_t gfp_mask,
				 int nr_to_scan)
{
	struct ion_system_heap *sys_heap;
	int nr_total = 0;
	int i, j, nr_freed = 0;
	int only_scan = 0;
	struct ion_page_pool *pool;

	sys_heap = container_of(heap, struct ion_system_heap, heap);

	if (!nr_to_scan)
		only_scan = 1;

	for (i = 0; i < NUM_ORDERS; i++) {
		nr_freed = 0;

		for (j = 0; j < VMID_LAST; j++) {
			if (is_secure_vmid_valid(j))
				nr_freed += ion_secure_page_pool_shrink(
						sys_heap, j, i, nr_to_scan);
		}

		pool = sys_heap->uncached_pools[i];
		nr_freed += ion_page_pool_shrink(pool, gfp_mask, nr_to_scan);

		pool = sys_heap->cached_pools[i];
		nr_freed += ion_page_pool_shrink(pool, gfp_mask, nr_to_scan);
		nr_total += nr_freed;

		if (!only_scan) {
			nr_to_scan -= nr_freed;
			/* shrink completed */
			if (nr_to_scan <= 0)
				break;
		}
	}

	return nr_total;
}

static struct ion_heap_ops system_heap_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = ion_system_heap_shrink,
};

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(
					heap, struct ion_system_heap, heap);
	bool use_seq = s;
	unsigned long uncached_total = 0;
	unsigned long cached_total = 0;
	unsigned long secure_total = 0;
	struct ion_page_pool *pool;
	int i, j;

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->uncached_pools[i];
		if (use_seq) {
			seq_printf(s,
				   "%d order %u highmem pages in uncached pool = %lu total\n",
				   pool->high_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->high_count);
			seq_printf(s,
				   "%d order %u lowmem pages in uncached pool = %lu total\n",
				   pool->low_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->low_count);
		}

		uncached_total += (1 << pool->order) * PAGE_SIZE *
			pool->high_count;
		uncached_total += (1 << pool->order) * PAGE_SIZE *
			pool->low_count;
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		pool = sys_heap->cached_pools[i];
		if (use_seq) {
			seq_printf(s,
				   "%d order %u highmem pages in cached pool = %lu total\n",
				   pool->high_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->high_count);
			seq_printf(s,
				   "%d order %u lowmem pages in cached pool = %lu total\n",
				   pool->low_count, pool->order,
				   (1 << pool->order) * PAGE_SIZE *
					pool->low_count);
		}

		cached_total += (1 << pool->order) * PAGE_SIZE *
			pool->high_count;
		cached_total += (1 << pool->order) * PAGE_SIZE *
			pool->low_count;
	}

	for (i = 0; i < NUM_ORDERS; i++) {
		for (j = 0; j < VMID_LAST; j++) {
			if (!is_secure_vmid_valid(j))
				continue;
			pool = sys_heap->secure_pools[j][i];

			if (use_seq) {
				seq_printf(s,
					   "VMID %d: %d order %u highmem pages in secure pool = %lu total\n",
					   j, pool->high_count, pool->order,
					   (1 << pool->order) * PAGE_SIZE *
						pool->high_count);
				seq_printf(s,
					   "VMID  %d: %d order %u lowmem pages in secure pool = %lu total\n",
					   j, pool->low_count, pool->order,
					   (1 << pool->order) * PAGE_SIZE *
						pool->low_count);
			}

			secure_total += (1 << pool->order) * PAGE_SIZE *
					 pool->high_count;
			secure_total += (1 << pool->order) * PAGE_SIZE *
					 pool->low_count;
		}
	}

	if (use_seq) {
		seq_puts(s, "--------------------------------------------\n");
		seq_printf(s, "uncached pool = %lu cached pool = %lu secure pool = %lu\n",
			   uncached_total, cached_total, secure_total);
		seq_printf(s, "pool total (uncached + cached + secure) = %lu\n",
			   uncached_total + cached_total + secure_total);
		seq_puts(s, "--------------------------------------------\n");
	} else {
		pr_info("-------------------------------------------------\n");
		pr_info("uncached pool = %lu cached pool = %lu secure pool = %lu\n",
			uncached_total, cached_total, secure_total);
		pr_info("pool total (uncached + cached + secure) = %lu\n",
			uncached_total + cached_total + secure_total);
		pr_info("-------------------------------------------------\n");
	}

	return 0;
}

static void ion_system_heap_destroy_pools(struct ion_page_pool **pools)
{
	int i;

	for (i = 0; i < NUM_ORDERS; i++)
		if (pools[i]) {
			ion_page_pool_destroy(pools[i]);
			pools[i] = NULL;
		}
}

/**
 * ion_system_heap_create_pools - Creates pools for all orders
 *
 * If this fails you don't need to destroy any pools. It's all or
 * nothing. If it succeeds you'll eventually need to use
 * ion_system_heap_destroy_pools to destroy the pools.
 */
static int ion_system_heap_create_pools(struct ion_page_pool **pools,
					bool cached)
{
	int i;
	for (i = 0; i < NUM_ORDERS; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags = low_order_gfp_flags;

		if (orders[i])
			gfp_flags = high_order_gfp_flags;
		pool = ion_page_pool_create(gfp_flags, orders[i], cached);
		if (!pool)
			goto err_create_pool;
		pools[i] = pool;
	}
	return 0;
err_create_pool:
	ion_system_heap_destroy_pools(pools);
	return -ENOMEM;
}

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *data)
{
	struct ion_system_heap *heap;
	int i;

	heap = kzalloc(sizeof(*heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->heap.ops = &system_heap_ops;
	heap->heap.type = ION_HEAP_TYPE_SYSTEM;
	heap->heap.flags = ION_HEAP_FLAG_DEFER_FREE;

	for (i = 0; i < VMID_LAST; i++)
		if (is_secure_vmid_valid(i))
			if (ion_system_heap_create_pools(
					heap->secure_pools[i], false))
				goto destroy_secure_pools;

	if (ion_system_heap_create_pools(heap->uncached_pools, false))
		goto destroy_secure_pools;

	if (ion_system_heap_create_pools(heap->cached_pools, true))
		goto destroy_uncached_pools;

	mutex_init(&heap->split_page_mutex);

	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;

destroy_uncached_pools:
	ion_system_heap_destroy_pools(heap->uncached_pools);
destroy_secure_pools:
	for (i = 0; i < VMID_LAST; i++) {
		if (heap->secure_pools[i])
			ion_system_heap_destroy_pools(heap->secure_pools[i]);
	}
	kfree(heap);
	return ERR_PTR(-ENOMEM);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long flags)
{
	int order = get_order(len);
	struct page *page;
	struct sg_table *table;
	unsigned long i;
	int ret;

	page = alloc_pages(low_order_gfp_flags | __GFP_NOWARN, order);
	if (!page)
		return -ENOMEM;

	split_page(page, order);

	len = PAGE_ALIGN(len);
	for (i = len >> PAGE_SHIFT; i < (1 << order); i++)
		__free_page(page + i);

	table = kmalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto free_pages;
	}

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_table;

	sg_set_page(table->sgl, page, len, 0);

	buffer->sg_table = table;

	ion_pages_sync_for_device(NULL, page, len, DMA_BIDIRECTIONAL);

	return 0;

free_table:
	kfree(table);
free_pages:
	for (i = 0; i < len >> PAGE_SHIFT; i++)
		__free_page(page + i);

	return ret;
}

static void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->sg_table;
	struct page *page = sg_page(table->sgl);
	unsigned long pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;
	unsigned long i;

	for (i = 0; i < pages; i++)
		__free_page(page + i);
	sg_free_table(table);
	kfree(table);
}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
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
