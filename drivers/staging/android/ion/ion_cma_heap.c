// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Linaro 2012
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 *
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/cma.h>
#include <linux/scatterlist.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/highmem.h>

#include "ion.h"
#include "ion_secure_util.h"

struct ion_cma_heap {
	struct ion_heap heap;
	struct cma *cma;
};

#define to_cma_heap(x) container_of(x, struct ion_cma_heap, heap)

/* ION CMA heap operations functions */
static int ion_cma_allocate(struct ion_heap *heap, struct ion_buffer *buffer,
			    unsigned long len,
			    unsigned long flags)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(heap);
	struct sg_table *table;
	struct page *pages;
	unsigned long size = PAGE_ALIGN(len);
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned long align = get_order(size);
	int ret;
	struct device *dev = heap->priv;

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	pages = cma_alloc(cma_heap->cma, nr_pages, align, false);
	if (!pages)
		return -ENOMEM;

	if (!(flags & ION_FLAG_SECURE)) {
		if (PageHighMem(pages)) {
			unsigned long nr_clear_pages = nr_pages;
			struct page *page = pages;

			while (nr_clear_pages > 0) {
				void *vaddr = kmap_atomic(page);

				memset(vaddr, 0, PAGE_SIZE);
				kunmap_atomic(vaddr);
				page++;
				nr_clear_pages--;
			}
		} else {
			memset(page_address(pages), 0, size);
		}
	}

	if (MAKE_ION_ALLOC_DMA_READY ||
	    (flags & ION_FLAG_SECURE) ||
	     (!ion_buffer_cached(buffer)))
		ion_pages_sync_for_device(dev, pages, size,
					  DMA_BIDIRECTIONAL);

	table = kmalloc(sizeof(*table), GFP_KERNEL);
	if (!table)
		goto err;

	ret = sg_alloc_table(table, 1, GFP_KERNEL);
	if (ret)
		goto free_mem;

	sg_set_page(table->sgl, pages, size, 0);

	buffer->priv_virt = pages;
	buffer->sg_table = table;
	return 0;

free_mem:
	kfree(table);
err:
	cma_release(cma_heap->cma, pages, nr_pages);
	return -ENOMEM;
}

static void ion_cma_free(struct ion_buffer *buffer)
{
	struct ion_cma_heap *cma_heap = to_cma_heap(buffer->heap);
	struct page *pages = buffer->priv_virt;
	unsigned long nr_pages = PAGE_ALIGN(buffer->size) >> PAGE_SHIFT;

	/* release memory */
	cma_release(cma_heap->cma, pages, nr_pages);
	/* release sg table */
	sg_free_table(buffer->sg_table);
	kfree(buffer->sg_table);
}

static struct ion_heap_ops ion_cma_ops = {
	.allocate = ion_cma_allocate,
	.free = ion_cma_free,
	.map_user = ion_heap_map_user,
	.map_kernel = ion_heap_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_cma_heap_create(struct ion_platform_heap *data)
{
	struct ion_cma_heap *cma_heap;
	struct device *dev = (struct device *)data->priv;

	if (!dev->cma_area)
		return ERR_PTR(-EINVAL);

	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);

	if (!cma_heap)
		return ERR_PTR(-ENOMEM);

	cma_heap->heap.ops = &ion_cma_ops;
	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	cma_heap->cma = dev->cma_area;
	cma_heap->heap.type = ION_HEAP_TYPE_DMA;
	return &cma_heap->heap;
}

static void ion_secure_cma_free(struct ion_buffer *buffer)
{
	if (ion_hyp_unassign_sg_from_flags(buffer->sg_table, buffer->flags,
					   true))
		return;

	ion_cma_free(buffer);
}

static int ion_secure_cma_allocate(struct ion_heap *heap,
				   struct ion_buffer *buffer, unsigned long len,
				   unsigned long flags)
{
	int ret;

	if (!(flags & ION_FLAGS_CP_MASK))
		return -EINVAL;

	ret = ion_cma_allocate(heap, buffer, len, flags);
	if (ret) {
		dev_err(heap->priv, "Unable to allocate cma buffer");
		goto out;
	}

	ret = ion_hyp_assign_sg_from_flags(buffer->sg_table, flags, true);
	if (ret) {
		if (ret == -EADDRNOTAVAIL) {
			goto out_free_buf;
		} else {
			ion_cma_free(buffer);
			goto out;
		}
	}

	return ret;

out_free_buf:
	ion_secure_cma_free(buffer);
out:
	return ret;
}

static void *ion_secure_cma_map_kernel(struct ion_heap *heap,
				       struct ion_buffer *buffer)
{
	if (!hlos_accessible_buffer(buffer)) {
		pr_info("%s: Mapping non-HLOS accessible buffer disallowed\n",
			__func__);
		return NULL;
	}
	return ion_heap_map_kernel(heap, buffer);
}

static int ion_secure_cma_map_user(struct ion_heap *mapper,
				   struct ion_buffer *buffer,
				   struct vm_area_struct *vma)
{
	if (!hlos_accessible_buffer(buffer)) {
		pr_info("%s: Mapping non-HLOS accessible buffer disallowed\n",
			__func__);
		return -EINVAL;
	}
	return ion_heap_map_user(mapper, buffer, vma);
}

static struct ion_heap_ops ion_secure_cma_ops = {
	.allocate = ion_secure_cma_allocate,
	.free = ion_secure_cma_free,
	.map_user = ion_secure_cma_map_user,
	.map_kernel = ion_secure_cma_map_kernel,
	.unmap_kernel = ion_heap_unmap_kernel,
};

struct ion_heap *ion_cma_secure_heap_create(struct ion_platform_heap *data)
{
	struct ion_cma_heap *cma_heap;
	struct device *dev = (struct device *)data->priv;

	if (!dev->cma_area)
		return ERR_PTR(-EINVAL);

	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);

	if (!cma_heap)
		return ERR_PTR(-ENOMEM);

	cma_heap->heap.ops = &ion_secure_cma_ops;
	/*
	 * get device from private heaps data, later it will be
	 * used to make the link with reserved CMA memory
	 */
	cma_heap->cma = dev->cma_area;
	cma_heap->heap.type = (enum ion_heap_type)ION_HEAP_TYPE_HYP_CMA;
	return &cma_heap->heap;
}
