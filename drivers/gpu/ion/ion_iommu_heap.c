/*
 * Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/io.h>
#include <linux/msm_ion.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/iommu.h>
#include <linux/pfn.h>
#include <linux/dma-mapping.h>
#include "ion_priv.h"

#include <asm/mach/map.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <mach/iommu_domains.h>
#include <trace/events/kmem.h>

struct ion_iommu_heap {
	struct ion_heap heap;
	struct ion_page_pool **cached_pools;
	struct ion_page_pool **uncached_pools;
};

/*
 * We will attempt to allocate high-order pages and store those in an
 * sg_list. However, some APIs expect an array of struct page * where
 * each page is of size PAGE_SIZE. We use this extra structure to
 * carry around an array of such pages (derived from the high-order
 * pages with nth_page).
 */
struct ion_iommu_priv_data {
	struct page **pages;
	unsigned int pages_uses_vmalloc;
	int nrpages;
	unsigned long size;
};

#define MAX_VMAP_RETRIES 10
#define BAD_ORDER	-1

static const unsigned int orders[] = {9, 8, 4, 0};
static const int num_orders = ARRAY_SIZE(orders);
static unsigned int low_gfp_flags = __GFP_HIGHMEM | GFP_KERNEL | __GFP_ZERO;
static unsigned int high_gfp_flags = (__GFP_HIGHMEM | __GFP_NORETRY
				| __GFP_NO_KSWAPD | __GFP_NOWARN |
				 __GFP_IO | __GFP_FS | __GFP_ZERO);

struct page_info {
	struct page *page;
	unsigned int order;
	struct list_head list;
};

static int order_to_index(unsigned int order)
{
	int i;
	for (i = 0; i < num_orders; i++)
		if (order == orders[i])
			return i;
	BUG();
	return BAD_ORDER;
}

static unsigned int order_to_size(int order)
{
	return PAGE_SIZE << order;
}

static struct page_info *alloc_largest_available(struct ion_iommu_heap *heap,
						unsigned long size,
						unsigned int max_order,
						unsigned long flags)
{
	struct page *page;
	struct page_info *info;
	int i;

	for (i = 0; i < num_orders; i++) {
		gfp_t gfp;
		int idx = order_to_index(orders[i]);
		struct ion_page_pool *pool;

		if (idx == BAD_ORDER)
			continue;

		if (ION_IS_CACHED(flags)) {
			pool = heap->cached_pools[idx];
			BUG_ON(!pool);
		} else {
			pool = heap->uncached_pools[idx];
			BUG_ON(!pool);
		}

		if (size < order_to_size(orders[i]))
			continue;
		if (max_order < orders[i])
			continue;

		if (orders[i]) {
			gfp = high_gfp_flags;
		} else {
			gfp = low_gfp_flags;
		}
		trace_alloc_pages_iommu_start(gfp, orders[i]);
		if (flags & ION_FLAG_POOL_FORCE_ALLOC)
			page = alloc_pages(gfp, orders[i]);
		else
			page = ion_page_pool_alloc(pool);
		trace_alloc_pages_iommu_end(gfp, orders[i]);
		if (!page) {
			trace_alloc_pages_iommu_fail(gfp, orders[i]);
			continue;
		}

		info = kmalloc(sizeof(struct page_info), GFP_KERNEL);
		if (info) {
			info->page = page;
			info->order = orders[i];
		}
		return info;
	}
	return NULL;
}

static int ion_iommu_buffer_zero(struct ion_iommu_priv_data *data,
				bool is_cached)
{
	int i, j, k;
	unsigned int npages_to_vmap;
	unsigned int total_pages;
	void *ptr = NULL;
	/*
	 * It's cheaper just to use writecombine memory and skip the
	 * cache vs. using a cache memory and trying to flush it afterwards
	 */
	pgprot_t pgprot = pgprot_writecombine(pgprot_kernel);

	/*
	 * As an optimization, we manually zero out all of the
	 * pages in one fell swoop here. To safeguard against
	 * insufficient vmalloc space, we only vmap
	 * `npages_to_vmap' at a time, starting with a
	 * conservative estimate of 1/8 of the total number of
	 * vmalloc pages available. Note that the `pages'
	 * array is composed of all 4K pages, irrespective of
	 * the size of the pages on the sg list.
	 */
	npages_to_vmap = ((VMALLOC_END - VMALLOC_START)/8)
			>> PAGE_SHIFT;
	total_pages = data->nrpages;
	for (i = 0; i < total_pages; i += npages_to_vmap) {
		npages_to_vmap = min(npages_to_vmap, total_pages - i);
		for (j = 0; j < MAX_VMAP_RETRIES && npages_to_vmap;
			++j) {
			ptr = vmap(&data->pages[i], npages_to_vmap,
					VM_IOREMAP, pgprot);
			if (ptr)
				break;
			else
				npages_to_vmap >>= 1;
		}
		if (!ptr)
			return -ENOMEM;

		memset(ptr, 0, npages_to_vmap * PAGE_SIZE);
		if (is_cached) {
			/*
			 * invalidate the cache to pick up the zeroing
			 */
			for (k = 0; k < npages_to_vmap; k++) {
				void *p = kmap_atomic(data->pages[i + k]);
				phys_addr_t phys = page_to_phys(
							data->pages[i + k]);

				dmac_inv_range(p, p + PAGE_SIZE);
				outer_inv_range(phys, phys + PAGE_SIZE);
				kunmap_atomic(p);
			}
		}
		vunmap(ptr);
	}

	return 0;
}

static int ion_iommu_heap_allocate(struct ion_heap *heap,
				      struct ion_buffer *buffer,
				      unsigned long size, unsigned long align,
				      unsigned long flags)
{
	int ret, i;
	struct list_head pages_list;
	struct page_info *info, *tmp_info;
	struct ion_iommu_priv_data *data = NULL;
	struct ion_iommu_heap *iommu_heap =
		container_of(heap, struct ion_iommu_heap, heap);

	if (msm_use_iommu()) {
		struct scatterlist *sg;
		struct sg_table *table;
		int j;
		unsigned int num_large_pages = 0;
		unsigned long size_remaining = PAGE_ALIGN(size);
		unsigned int max_order = ION_IS_CACHED(flags) ? 0 : orders[0];
		unsigned int page_tbl_size;

		data = kmalloc(sizeof(*data), GFP_KERNEL);
		if (!data)
			return -ENOMEM;

		INIT_LIST_HEAD(&pages_list);
		while (size_remaining > 0) {
			info = alloc_largest_available(iommu_heap,
						size_remaining,
						max_order,
						flags);
			if (!info) {
				ret = -ENOMEM;
				goto err_free_data;
			}
			list_add_tail(&info->list, &pages_list);
			size_remaining -= order_to_size(info->order);
			max_order = info->order;
			num_large_pages++;
		}

		data->size = PFN_ALIGN(size);
		data->nrpages = data->size >> PAGE_SHIFT;
		data->pages_uses_vmalloc = 0;
		page_tbl_size = sizeof(struct page *) * data->nrpages;

		if (page_tbl_size > SZ_8K) {
			/*
			 * Do fallback to ensure we have a balance between
			 * performance and availability.
			 */
			data->pages = kmalloc(page_tbl_size,
					      __GFP_COMP | __GFP_NORETRY |
					      __GFP_NO_KSWAPD | __GFP_NOWARN);
			if (!data->pages) {
				data->pages = vmalloc(page_tbl_size);
				data->pages_uses_vmalloc = 1;
			}
		} else {
			data->pages = kmalloc(page_tbl_size, GFP_KERNEL);
		}
		if (!data->pages) {
			ret = -ENOMEM;
			goto err_free_data;
		}

		table = buffer->sg_table =
				kzalloc(sizeof(struct sg_table), GFP_KERNEL);

		if (!table) {
			ret = -ENOMEM;
			goto err1;
		}
		ret = sg_alloc_table(table, num_large_pages, GFP_KERNEL);
		if (ret)
			goto err2;

		i = 0;
		sg = table->sgl;
		list_for_each_entry_safe(info, tmp_info, &pages_list, list) {
			struct page *page = info->page;
			sg_set_page(sg, page, order_to_size(info->order), 0);
			sg_dma_address(sg) = sg_phys(sg);
			sg = sg_next(sg);
			for (j = 0; j < (1 << info->order); ++j)
				data->pages[i++] = nth_page(page, j);
			list_del(&info->list);
			kfree(info);
		}


		if (flags & ION_FLAG_POOL_FORCE_ALLOC) {
			ret = ion_iommu_buffer_zero(data, ION_IS_CACHED(flags));
			if (ret) {
				pr_err("Couldn't vmap the pages for zeroing\n");
				goto err3;
			}


			if (!ION_IS_CACHED(flags))
				dma_sync_sg_for_device(NULL, table->sgl,
						table->nents,
						DMA_BIDIRECTIONAL);

		}
		buffer->priv_virt = data;
		return 0;

	} else {
		return -ENOMEM;
	}


err3:
	sg_free_table(buffer->sg_table);
err2:
	kfree(buffer->sg_table);
	buffer->sg_table = 0;
err1:
	if (data->pages_uses_vmalloc)
		vfree(data->pages);
	else
		kfree(data->pages);
err_free_data:
	kfree(data);

	list_for_each_entry_safe(info, tmp_info, &pages_list, list) {
		if (info->page)
			__free_pages(info->page, info->order);
		list_del(&info->list);
		kfree(info);
	}
	return ret;
}

static void ion_iommu_heap_free(struct ion_buffer *buffer)
{
	int i;
	struct scatterlist *sg;
	struct sg_table *table = buffer->sg_table;
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	bool cached = ion_buffer_cached(buffer);
	struct ion_iommu_heap *iommu_heap =
	     container_of(buffer->heap, struct	ion_iommu_heap, heap);

	if (!table)
		return;
	if (!data)
		return;

	if (!(buffer->flags & ION_FLAG_POOL_FORCE_ALLOC))
		ion_iommu_buffer_zero(data, ION_IS_CACHED(buffer->flags));

	for_each_sg(table->sgl, sg, table->nents, i) {
		int order = get_order(sg_dma_len(sg));
		int idx = order_to_index(order);
		struct ion_page_pool *pool;

		if (idx == BAD_ORDER) {
			WARN_ON(1);
			continue;
		}

		if (cached)
			pool = iommu_heap->cached_pools[idx];
		else
			pool = iommu_heap->uncached_pools[idx];

		if (buffer->flags & ION_FLAG_POOL_FORCE_ALLOC)
			__free_pages(sg_page(sg), order);
		else
			ion_page_pool_free(pool, sg_page(sg));
	}

	sg_free_table(table);
	kfree(table);
	table = 0;
	if (data->pages_uses_vmalloc)
		vfree(data->pages);
	else
		kfree(data->pages);
	kfree(data);
}

void *ion_iommu_heap_map_kernel(struct ion_heap *heap,
				struct ion_buffer *buffer)
{
	struct ion_iommu_priv_data *data = buffer->priv_virt;
	pgprot_t page_prot = PAGE_KERNEL;

	if (!data)
		return NULL;

	if (!ION_IS_CACHED(buffer->flags))
		page_prot = pgprot_writecombine(page_prot);

	buffer->vaddr = vmap(data->pages, data->nrpages, VM_IOREMAP, page_prot);

	return buffer->vaddr;
}

void ion_iommu_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	if (!buffer->vaddr)
		return;

	vunmap(buffer->vaddr);
	buffer->vaddr = NULL;
}

int ion_iommu_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			       struct vm_area_struct *vma)
{
	struct sg_table *table = buffer->sg_table;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	struct scatterlist *sg;
	int i;

	if (!ION_IS_CACHED(buffer->flags))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sg_dma_len(sg);

		if (offset >= sg_dma_len(sg)) {
			offset -= sg_dma_len(sg);
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sg_dma_len(sg) - offset;
			offset = 0;
		}
		len = min(len, remainder);
		remap_pfn_range(vma, addr, page_to_pfn(page), len,
				vma->vm_page_prot);
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static struct sg_table *ion_iommu_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	return buffer->sg_table;
}

static void ion_iommu_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
}

static int ion_iommu_heap_debug_show(struct ion_heap *heap, struct seq_file *s,
					void *unused)
{

	struct ion_iommu_heap *iommu_heap = container_of(heap,
							struct ion_iommu_heap,
							heap);
	int i;
	unsigned long total = 0;

	seq_printf(s, "Cached Pools:\n");
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = iommu_heap->cached_pools[i];
		seq_printf(s, "%d order %u highmem pages in pool = %lx total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in pool = %lx total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);

		total += (1 << pool->order) * PAGE_SIZE *
			  (pool->low_count + pool->high_count);
	}

	seq_printf(s, "Uncached Pools:\n");
	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool = iommu_heap->uncached_pools[i];
		seq_printf(s, "%d order %u highmem pages in pool = %lx total\n",
			   pool->high_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->high_count);
		seq_printf(s, "%d order %u lowmem pages in pool = %lx total\n",
			   pool->low_count, pool->order,
			   (1 << pool->order) * PAGE_SIZE * pool->low_count);

		total += (1 << pool->order) * PAGE_SIZE *
			  (pool->low_count + pool->high_count);
	}
	seq_printf(s, "Total bytes in pool: %lx\n", total);
	return 0;
}

static struct ion_heap_ops iommu_heap_ops = {
	.allocate = ion_iommu_heap_allocate,
	.free = ion_iommu_heap_free,
	.map_user = ion_iommu_heap_map_user,
	.map_kernel = ion_iommu_heap_map_kernel,
	.unmap_kernel = ion_iommu_heap_unmap_kernel,
	.map_dma = ion_iommu_heap_map_dma,
	.unmap_dma = ion_iommu_heap_unmap_dma,
};

struct ion_heap *ion_iommu_heap_create(struct ion_platform_heap *heap_data)
{
	struct ion_iommu_heap *iommu_heap;
	int i;

	iommu_heap = kzalloc(sizeof(struct ion_iommu_heap), GFP_KERNEL);
	if (!iommu_heap)
		return ERR_PTR(-ENOMEM);

	iommu_heap->heap.ops = &iommu_heap_ops;
	iommu_heap->heap.type = ION_HEAP_TYPE_IOMMU;
	iommu_heap->uncached_pools = kzalloc(
			      sizeof(struct ion_page_pool *) * num_orders,
			      GFP_KERNEL);
	if (!iommu_heap->uncached_pools)
		goto err_alloc_uncached_pools;

	iommu_heap->cached_pools = kzalloc(
			      sizeof(struct ion_page_pool *) * num_orders,
			      GFP_KERNEL);

	if (!iommu_heap->cached_pools)
		goto err_alloc_cached_pools;

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags;

		if (orders[i])
			gfp_flags = high_gfp_flags | __GFP_ZERO;
		else
			gfp_flags = low_gfp_flags | __GFP_ZERO;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_cached_pool;
		iommu_heap->cached_pools[i] = pool;
	}

	for (i = 0; i < num_orders; i++) {
		struct ion_page_pool *pool;
		gfp_t gfp_flags;

		if (orders[i])
			gfp_flags = high_gfp_flags | __GFP_ZERO;
		else
			gfp_flags = low_gfp_flags | __GFP_ZERO;
		pool = ion_page_pool_create(gfp_flags, orders[i]);
		if (!pool)
			goto err_create_uncached_pool;
		iommu_heap->uncached_pools[i] = pool;
	}
	iommu_heap->heap.debug_show = ion_iommu_heap_debug_show;
	return &iommu_heap->heap;

err_create_uncached_pool:
	for (i = 0; i < num_orders; i++)
		if (iommu_heap->cached_pools[i])
			ion_page_pool_destroy(iommu_heap->uncached_pools[i]);


err_create_cached_pool:
	for (i = 0; i < num_orders; i++)
		if (iommu_heap->uncached_pools[i])
			ion_page_pool_destroy(iommu_heap->cached_pools[i]);

	kfree(iommu_heap->cached_pools);
err_alloc_cached_pools:
	kfree(iommu_heap->uncached_pools);
err_alloc_uncached_pools:
	kfree(iommu_heap);
	return ERR_PTR(-ENOMEM);
}

void ion_iommu_heap_destroy(struct ion_heap *heap)
{
	struct ion_iommu_heap *iommu_heap =
	     container_of(heap, struct  ion_iommu_heap, heap);

	kfree(iommu_heap);
	iommu_heap = NULL;
}
