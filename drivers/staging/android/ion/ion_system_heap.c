/*
 * drivers/staging/android/ion/ion_system_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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
#include <linux/msm_ion.h>
#include <linux/scatterlist.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion.h"
#include "ion_priv.h"
#include <linux/dma-mapping.h>
#include <trace/events/kmem.h>
#include <soc/qcom/secure_buffer.h>

static gfp_t high_order_gfp_flags = (GFP_HIGHUSER | __GFP_NOWARN |
				     __GFP_NO_KSWAPD | __GFP_NORETRY)
				     & ~__GFP_WAIT;
static gfp_t low_order_gfp_flags  = (GFP_HIGHUSER | __GFP_NOWARN);

#ifndef CONFIG_ALLOC_BUFFERS_IN_4K_CHUNKS
static const unsigned int orders[] = {4, 0};
#else
static const unsigned int orders[] = {0};
#endif

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
	struct ion_page_pool **secure_pools[VMID_LAST];
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
	bool prefetch = buffer->flags & ION_FLAG_POOL_PREFETCH;
	struct page *page;
	struct ion_page_pool *pool;
	int vmid = get_secure_vmid(buffer->flags);

	if (*from_pool) {
		if (vmid > 0)
			pool = heap->secure_pools[vmid][order_to_index(order)];
		else if (!cached)
			pool = heap->uncached_pools[order_to_index(order)];
		else
			pool = heap->cached_pools[order_to_index(order)];

		if (prefetch)
			page = ion_page_pool_prefetch(pool, from_pool);
		else
			page = ion_page_pool_alloc(pool, from_pool);
	} else {
		gfp_t gfp_mask = low_order_gfp_flags;
		if (order)
			gfp_mask = high_order_gfp_flags;
		page = alloc_pages(gfp_mask, order);
	}
	if (!page)
		return 0;

	return page;
}

/*
 * For secure pages that need to be freed and not added back to the pool; the
 *  hyp_unassign should be called before calling this function
 */
static void free_buffer_page(struct ion_system_heap *heap,
			     struct ion_buffer *buffer, struct page *page,
			     unsigned int order)
{
	bool cached = ion_buffer_cached(buffer);
	bool prefetch = buffer->flags & ION_FLAG_POOL_PREFETCH;
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
			ion_page_pool_free(pool, page, prefetch);
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

	info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
	if (!info)
		return NULL;

	for (i = 0; i < num_orders; i++) {
		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;
		from_pool = !(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC);
		page = alloc_buffer_page(heap, buffer, orders[i], &from_pool);
		if (!page)
			continue;

		info->page = page;
		info->order = orders[i];
		info->from_pool = from_pool;
		INIT_LIST_HEAD(&info->list);
		return info;
	}
	kfree(info);

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
	int vmid = get_secure_vmid(buffer->flags);

	if (align > PAGE_SIZE)
		return -EINVAL;

	if (size / PAGE_SIZE > totalram_pages / 2)
		return -ENOMEM;

	data.size = 0;
	INIT_LIST_HEAD(&pages);
	INIT_LIST_HEAD(&pages_from_pool);
	while (size_remaining > 0) {
		info = alloc_largest_available(sys_heap, buffer, size_remaining,
						max_order);
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

	ret = msm_ion_heap_alloc_pages_mem(&data);

	if (ret)
		goto err;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		goto err_free_data_pages;

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
		} else {
			BUG();
		}
		sg = sg_next(sg);

	} while (sg);

	ret = msm_ion_heap_pages_zero(data.pages, data.size >> PAGE_SHIFT);
	if (ret) {
		pr_err("Unable to zero pages\n");
		goto err_free_sg2;
	}

	if (nents_sync) {
		dma_sync_sg_for_device(NULL, table_sync.sgl, table_sync.nents,
				       DMA_BIDIRECTIONAL);
		if (vmid > 0) {
			ret = ion_system_secure_heap_assign_sg(&table_sync,
								vmid);
			if (ret)
				goto err_free_sg2;
		}
	}

	buffer->priv_virt = table;
	if (nents_sync)
		sg_free_table(&table_sync);
	msm_ion_heap_free_pages_mem(&data);
	return 0;

err_free_sg2:
	/* We failed to zero buffers. Bypass pool */
	buffer->flags |= ION_PRIV_FLAG_SHRINKER_FREE;

	if (vmid > 0)
		ion_system_secure_heap_unassign_sg(table, vmid);

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
	msm_ion_heap_free_pages_mem(&data);
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
	struct sg_table *table = buffer->priv_virt;
	struct scatterlist *sg;
	LIST_HEAD(pages);
	int i;
	int vmid = get_secure_vmid(buffer->flags);

	if (!(buffer->private_flags & ION_PRIV_FLAG_SHRINKER_FREE) &&
	    !(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)) {
		if (vmid < 0)
			msm_ion_heap_sg_table_zero(table, buffer->size);
	} else if (vmid > 0) {
		if (ion_system_secure_heap_unassign_sg(table, vmid))
			return;
	}

	for_each_sg(table->sgl, sg, table->nents, i)
		free_buffer_page(sys_heap, buffer, sg_page(sg),
				get_order(sg->length));
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
}

static int ion_secure_page_pool_shrink(
		struct ion_system_heap *sys_heap,
		int vmid, int order_idx, int nr_to_scan)
{
	int ret, freed = 0;
	int order = orders[order_idx];
	struct page *page, *tmp;
	struct sg_table sgt;
	struct scatterlist *sg;
	struct ion_page_pool *pool = sys_heap->secure_pools[vmid][order_idx];
	LIST_HEAD(pages);

	if (nr_to_scan == 0)
		return ion_page_pool_total(pool, true);

	while (freed < nr_to_scan) {
		page = ion_page_pool_alloc_pool_only(pool);
		if (!page)
			break;
		list_add(&page->lru, &pages);
		freed += (1 << order);
	}

	if (!freed)
		return freed;

	ret = sg_alloc_table(&sgt, (freed >> order), GFP_KERNEL);
	if (ret)
		goto out1;
	sg = sgt.sgl;
	list_for_each_entry(page, &pages, lru) {
		sg_set_page(sg, page, (1 << order) * PAGE_SIZE, 0);
		sg_dma_address(sg) = page_to_phys(page);
		sg = sg_next(sg);
	}

	if (ion_system_secure_heap_unassign_sg(&sgt, vmid))
		goto out2;

	list_for_each_entry_safe(page, tmp, &pages, lru) {
		list_del(&page->lru);
		ion_page_pool_free_immediate(pool, page);
	}

	sg_free_table(&sgt);
	return freed;

out1:
	/* Restore pages to secure pool */
	list_for_each_entry_safe(page, tmp, &pages, lru) {
		list_del(&page->lru);
		ion_page_pool_free(pool, page, false);
	}
	return 0;
out2:
	/*
	 * The security state of the pages is unknown after a failure;
	 * They can neither be added back to the secure pool nor buddy system.
	 */
	sg_free_table(&sgt);
	return 0;
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

	for (i = 0; i < num_orders; i++) {
		nr_freed = 0;

		for (j = 0; j < VMID_LAST; j++) {
			if (is_secure_vmid_valid(j))
				nr_freed += ion_secure_page_pool_shrink(
						sys_heap, j, i, nr_to_scan);
		}

		pool = sys_heap->uncached_pools[i];
		nr_freed += ion_page_pool_shrink(pool, gfp_mask,
						nr_to_scan);

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
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
	.map_user = ion_heap_map_user,
	.shrink = ion_system_heap_shrink,
};

static int ion_system_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
				      void *unused)
{

	struct ion_system_heap *sys_heap = container_of(heap,
							struct ion_system_heap,
							heap);
	bool use_seq = s != NULL;
	unsigned long uncached_total = 0;
	unsigned long cached_total = 0;
	unsigned long secure_total = 0;
	struct ion_page_pool *pool;
	int i, j;

	for (i = 0; i < num_orders; i++) {
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

	for (i = 0; i < num_orders; i++) {
		pool = sys_heap->cached_pools[i];
		if (use_seq) {
			seq_printf(s,
				"%d order %u highmem pages in cached pool = %lu total\n",
				pool->high_count, pool->order,
				(1 << pool->order) * PAGE_SIZE * pool->high_count);
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

	for (i = 0; i < num_orders; i++) {
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
	for (i = 0; i < num_orders; i++)
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
	int i;
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

	for (i = 0; i < VMID_LAST; i++) {
		if (is_secure_vmid_valid(i)) {
			heap->secure_pools[i] = kzalloc(pools_size, GFP_KERNEL);
			if (!heap->secure_pools[i])
				goto err_create_secure_pools;
			if (ion_system_heap_create_pools(heap->secure_pools[i]))
				goto err_create_secure_pools;
		}
	}

	if (ion_system_heap_create_pools(heap->uncached_pools))
		goto err_create_uncached_pools;

	if (ion_system_heap_create_pools(heap->cached_pools))
		goto err_create_cached_pools;

	heap->heap.debug_show = ion_system_heap_debug_show;
	return &heap->heap;

err_create_cached_pools:
	ion_system_heap_destroy_pools(heap->uncached_pools);
err_create_uncached_pools:
	kfree(heap->cached_pools);
err_create_secure_pools:
	for (i = 0; i < VMID_LAST; i++) {
		if (heap->secure_pools[i]) {
			ion_system_heap_destroy_pools(heap->secure_pools[i]);
			kfree(heap->secure_pools[i]);
		}
	}
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
	int i, j;

	for (i = 0; i < VMID_LAST; i++) {
		if (!is_secure_vmid_valid(i))
			continue;
		for (j = 0; j < num_orders; j++)
			ion_secure_page_pool_shrink(sys_heap, i, j, UINT_MAX);

		ion_system_heap_destroy_pools(sys_heap->secure_pools[i]);
	}
	ion_system_heap_destroy_pools(sys_heap->uncached_pools);
	ion_system_heap_destroy_pools(sys_heap->cached_pools);
	kfree(sys_heap->uncached_pools);
	kfree(sys_heap->cached_pools);
	kfree(sys_heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	int order = get_order(len);
	struct page *page;
	struct sg_table *table;
	unsigned long i;
	int ret;

	if (align > (PAGE_SIZE << order))
		return -EINVAL;

	page = alloc_pages(low_order_gfp_flags | __GFP_ZERO, order);
	if (!page)
		return -ENOMEM;

	split_page(page, order);

	len = PAGE_ALIGN(len);
	for (i = len >> PAGE_SHIFT; i < (1 << order); i++)
		__free_page(page + i);

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		ret = -ENOMEM;
		goto out;
	}

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto out;

	sg_set_page(table->sgl, page, len, 0);

	buffer->priv_virt = table;

	ion_pages_sync_for_device(NULL, page, len, DMA_BIDIRECTIONAL);

	return 0;

out:
	for (i = 0; i < len >> PAGE_SHIFT; i++)
		__free_page(page + i);
	kfree(table);
	return ret;
}

void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	unsigned long pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;
	unsigned long i;

	for (i = 0; i < pages; i++)
		__free_page(page + i);
	sg_free_table(table);
	kfree(table);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	struct sg_table *table = buffer->priv_virt;
	struct page *page = sg_page(table->sgl);
	*addr = page_to_phys(page);
	*len = buffer->size;
	return 0;
}

struct sg_table *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						struct ion_buffer *buffer)
{
	return buffer->priv_virt;
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
