// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF CMA heap exporter
 * Copied from drivers/dma-buf/heaps/cma_heap.c as of commit b61614ec318a
 * ("dma-buf: heaps: Add CMA heap to dmabuf heaps")
 *
 * Copyright (C) 2012, 2019 Linaro Ltd.
 * Author: <benjamin.gaignard@linaro.org> for ST-Ericsson.
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/cma.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-contiguous.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/sched/signal.h>
#include <linux/list.h>

#include "heap-helpers.h"
#include "qcom_dma_heap_priv.h"

struct cma_heap {
	struct dma_heap *heap;
	struct cma *cma;
	struct list_head list;
};

LIST_HEAD(cma_heaps);

static void cma_heap_free(struct heap_helper_buffer *buffer)
{
	struct cma_heap *cma_heap;
	unsigned long nr_pages = buffer->pagecount;
	struct page *cma_pages = buffer->priv_virt;
	struct list_head *list_pos;

	/*
	 * We're guaranteed to find our heap in this list.  When
	 * cma_heap_free() was assigned as this buffer's free function in
	 * cma_heap_allocate() below, the heap was already in the linked
	 * list.  So, it will still be in the linked list now.
	 */

	list_for_each(list_pos, &cma_heaps) {
		cma_heap = container_of(list_pos, struct cma_heap, list);
		if (cma_heap->heap == buffer->heap)
			break;
	}

	/* free page list */
	kfree(buffer->pages);
	/* release memory */
	cma_release(cma_heap->cma, cma_pages, nr_pages);
	kfree(buffer);
}

/* dmabuf heap CMA operations functions */
struct dma_buf *cma_heap_allocate(struct dma_heap *heap,
				  unsigned long len,
				  unsigned long fd_flags,
				  unsigned long heap_flags)
{
	struct cma_heap *cma_heap;
	struct heap_helper_buffer *helper_buffer;
	struct page *cma_pages;
	size_t size = PAGE_ALIGN(len);
	unsigned long nr_pages = size >> PAGE_SHIFT;
	unsigned long align = get_order(size);
	struct dma_buf *dmabuf;
	int ret = -ENOMEM;
	pgoff_t pg;
	struct list_head *list_pos;

	/*
	 * A temporary workaround until dma_heap_get_drvdata() lands
	 * in ACK. Note that it's guaranteed that there is one entry in
	 * the list cma_heaps such that cma_heap->heap == heap.  This
	 * is because `heap` can be passed to this function only if
	 * it was created in __add_cma_heap() below - this function
	 * places `heap` in the linked list.
	 */
	list_for_each(list_pos, &cma_heaps) {
		cma_heap = container_of(list_pos, struct cma_heap, list);
		if (cma_heap->heap == heap)
			break;
	}

	if (align > CONFIG_CMA_ALIGNMENT)
		align = CONFIG_CMA_ALIGNMENT;

	helper_buffer = kzalloc(sizeof(*helper_buffer), GFP_KERNEL);
	if (!helper_buffer)
		return ERR_PTR(-ENOMEM);

	qcom_init_heap_helper_buffer(helper_buffer, cma_heap_free);
	helper_buffer->heap = heap;
	helper_buffer->size = len;

	cma_pages = cma_alloc(cma_heap->cma, nr_pages, align, false);
	if (!cma_pages)
		goto free_buf;

	if (PageHighMem(cma_pages)) {
		unsigned long nr_clear_pages = nr_pages;
		struct page *page = cma_pages;

		while (nr_clear_pages > 0) {
			void *vaddr = kmap_atomic(page);

			memset(vaddr, 0, PAGE_SIZE);
			kunmap_atomic(vaddr);
			/*
			 * Avoid wasting time zeroing memory if the process
			 * has been killed by SIGKILL
			 */
			if (fatal_signal_pending(current))
				goto free_cma;

			page++;
			nr_clear_pages--;
		}
	} else {
		memset(page_address(cma_pages), 0, size);
	}

	helper_buffer->pagecount = nr_pages;
	helper_buffer->pages = kmalloc_array(helper_buffer->pagecount,
					     sizeof(*helper_buffer->pages),
					     GFP_KERNEL);
	if (!helper_buffer->pages) {
		ret = -ENOMEM;
		goto free_cma;
	}

	for (pg = 0; pg < helper_buffer->pagecount; pg++)
		helper_buffer->pages[pg] = &cma_pages[pg];

	/* create the dmabuf */
	dmabuf = qcom_heap_helper_export_dmabuf(helper_buffer, fd_flags);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
	}

	helper_buffer->dmabuf = dmabuf;
	helper_buffer->priv_virt = cma_pages;

	return dmabuf;

free_pages:
	kfree(helper_buffer->pages);
free_cma:
	cma_release(cma_heap->cma, cma_pages, nr_pages);
free_buf:
	kfree(helper_buffer);
	return ERR_PTR(ret);
}

static const struct dma_heap_ops cma_heap_ops = {
	.allocate = cma_heap_allocate,
};

static int __add_cma_heap(struct platform_heap *heap_data, void *data)
{
	struct cma_heap *cma_heap;
	struct dma_heap_export_info exp_info;

	cma_heap = kzalloc(sizeof(*cma_heap), GFP_KERNEL);
	if (!cma_heap)
		return -ENOMEM;
	cma_heap->cma = heap_data->dev->cma_area;

	exp_info.name = heap_data->name;
	exp_info.ops = &cma_heap_ops;
	exp_info.priv = NULL;

	list_add(&cma_heap->list, &cma_heaps);

	cma_heap->heap = dma_heap_add(&exp_info);
	if (IS_ERR(cma_heap->heap)) {
		int ret = PTR_ERR(cma_heap->heap);

		kfree(cma_heap);
		return ret;
	}

	return 0;
}

int qcom_add_cma_heap(struct platform_heap *heap_data)
{
	return __add_cma_heap(heap_data, NULL);
}
