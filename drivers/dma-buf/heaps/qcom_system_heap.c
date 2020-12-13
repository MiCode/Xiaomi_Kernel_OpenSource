// SPDX-License-Identifier: GPL-2.0
/*
 * DMABUF System heap exporter
 * Originally copied from: drivers/dma-buf/heaps/system_heap.c as of commit
 * 263e38f82cbb ("dma-buf: heaps: Remove redundant heap identifier from system
 * heap name")
 *
 * Additions taken from modifications to drivers/dma-buf/heaps/system-heap.c,
 * from patches submitted, are listed below:
 *
 * Addition that modifies dma_buf ops to use SG tables taken from
 * drivers/dma-buf/heaps/system-heap.c in:
 * https://lore.kernel.org/lkml/20201017013255.43568-2-john.stultz@linaro.org/
 *
 * Addition that skips unneeded syncs in the dma_buf ops taken from
 * https://lore.kernel.org/lkml/20201017013255.43568-5-john.stultz@linaro.org/
 *
 * Addition that tries to allocate higher order pages taken from
 * https://lore.kernel.org/lkml/20201017013255.43568-6-john.stultz@linaro.org/
 *
 * Addition that implements an uncached heap taken from
 * https://lore.kernel.org/lkml/20201017013255.43568-8-john.stultz@linaro.org/,
 * with our own modificaitons made to account for core kernel changes that are
 * a part of the patch series.
 *
 * Pooling functionality taken from:
 * Git-repo: https://git.linaro.org/people/john.stultz/android-dev.git
 * Branch: dma-buf-heap-perf
 * Git-commit: 6f080eb67dce63c6efa57ef564ca4cd762ccebb0
 * Git-commit: 6fb9593b928c4cb485bef4e88c59c6b9fdf11352
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019, 2020 Linaro Ltd.
 *
 * Portions based off of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 *
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/dma-heap.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/qcom_dma_heap.h>

#include "qcom_dynamic_page_pool.h"
#include "qcom_sg_ops.h"
#include "qcom_system_heap.h"

static int system_heap_clear_pages(struct page **pages, int num, pgprot_t pgprot)
{
	void *addr = vmap(pages, num, VM_MAP, pgprot);

	if (!addr)
		return -ENOMEM;
	memset(addr, 0, PAGE_SIZE * num);
	vunmap(addr);
	return 0;
}

static int system_heap_zero_buffer(struct qcom_sg_buffer *buffer)
{
	struct sg_table *sgt = &buffer->sg_table;
	struct sg_page_iter piter;
	struct page *pages[32];
	int p = 0;
	int ret = 0;

	for_each_sgtable_page(sgt, &piter, 0) {
		pages[p++] = sg_page_iter_page(&piter);
		if (p == ARRAY_SIZE(pages)) {
			ret = system_heap_clear_pages(pages, p, PAGE_KERNEL);
			if (ret)
				return ret;
			p = 0;
		}
	}
	if (p)
		ret = system_heap_clear_pages(pages, p, PAGE_KERNEL);

	return ret;
}

static void system_heap_free(struct qcom_sg_buffer *buffer)
{
	struct qcom_system_heap *sys_heap;
	struct sg_table *table;
	struct scatterlist *sg;
	int i, j;

	sys_heap = dma_heap_get_drvdata(buffer->heap);

	/* Zero the buffer pages before adding back to the pool */
	if (!buffer->secure)
		system_heap_zero_buffer(buffer);

	table = &buffer->sg_table;
	for_each_sg(table->sgl, sg, table->nents, i) {
		struct page *page = sg_page(sg);

		for (j = 0; j < NUM_ORDERS; j++) {
			if (compound_order(page) == orders[j])
				break;
		}
		dynamic_page_pool_free(sys_heap->pool_list[j], page);
	}
	sg_free_table(table);
	kfree(buffer);
}

static struct page *alloc_largest_available(struct dynamic_page_pool **pools,
					    unsigned long size,
					    unsigned int max_order)
{
	struct page *page;
	int i;

	for (i = 0; i < NUM_ORDERS; i++) {
		if (size <  (PAGE_SIZE << orders[i]))
			continue;
		if (max_order < orders[i])
			continue;
		page = dynamic_page_pool_alloc(pools[i]);
		if (!page)
			continue;
		return page;
	}
	return NULL;
}

static struct dma_buf *system_heap_allocate(struct dma_heap *heap,
					       unsigned long len,
					       unsigned long fd_flags,
					       unsigned long heap_flags)
{
	struct qcom_system_heap *sys_heap;
	struct qcom_sg_buffer *buffer;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	unsigned long size_remaining = len;
	unsigned int max_order = orders[0];
	struct dma_buf *dmabuf;
	struct sg_table *table;
	struct scatterlist *sg;
	struct list_head pages;
	struct page *page, *tmp_page;
	int i, ret = -ENOMEM;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	sys_heap = dma_heap_get_drvdata(heap);

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);
	buffer->heap = heap;
	buffer->len = len;
	buffer->uncached = sys_heap->uncached;
	buffer->free = system_heap_free;

	INIT_LIST_HEAD(&pages);
	i = 0;
	while (size_remaining > 0) {
		/*
		 * Avoid trying to allocate memory if the process
		 * has been killed by SIGKILL
		 */
		if (fatal_signal_pending(current))
			goto free_buffer;

		page = alloc_largest_available(sys_heap->pool_list,
					       size_remaining,
					       max_order);
		if (!page)
			goto free_buffer;

		list_add_tail(&page->lru, &pages);
		size_remaining -= page_size(page);
		max_order = compound_order(page);
		i++;
	}

	table = &buffer->sg_table;
	if (sg_alloc_table(table, i, GFP_KERNEL))
		goto free_buffer;

	sg = table->sgl;
	list_for_each_entry_safe(page, tmp_page, &pages, lru) {
		sg_set_page(sg, page, page_size(page), 0);
		sg = sg_next(sg);
		list_del(&page->lru);
	}

	/* create the dmabuf */
	exp_info.ops = &qcom_sg_buf_ops;
	exp_info.size = buffer->len;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;
	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		ret = PTR_ERR(dmabuf);
		goto free_pages;
	}

	/*
	 * For uncached buffers, we need to initially flush cpu cache, since
	 * the __GFP_ZERO on the allocation means the zeroing was done by the
	 * cpu and thus it is likely cached. Map (and implicitly flush) and
	 * unmap it now so we don't get corruption later on.
	 */
	if (buffer->uncached) {
		dma_map_sgtable(sys_heap->dev, table, DMA_BIDIRECTIONAL, 0);
		dma_unmap_sgtable(sys_heap->dev, table, DMA_BIDIRECTIONAL, 0);
	}

	return dmabuf;

free_pages:
	for_each_sgtable_sg(table, sg, i) {
		struct page *p = sg_page(sg);

		__free_pages(p, compound_order(p));
	}
	sg_free_table(table);
free_buffer:
	list_for_each_entry_safe(page, tmp_page, &pages, lru)
		__free_pages(page, compound_order(page));
	kfree(buffer);

	return ERR_PTR(ret);
}

static const struct dma_heap_ops system_heap_ops = {
	.allocate = system_heap_allocate,
};

int qcom_system_heap_create(char *name, bool uncached)
{
	struct dma_heap_export_info exp_info;
	struct dma_heap *heap;
	struct qcom_system_heap *sys_heap;
	struct device *heap_dev;
	int ret;

	sys_heap = kzalloc(sizeof(*sys_heap), GFP_KERNEL);
	if (!sys_heap) {
		ret = -ENOMEM;
		goto out;
	}

	exp_info.name = name;
	exp_info.ops = &system_heap_ops;
	exp_info.priv = sys_heap;

	sys_heap->uncached = uncached;

	if (uncached) {
		heap_dev = kzalloc(sizeof(*heap_dev), __GFP_ZERO);
		if (!heap_dev) {
			ret = -ENOMEM;
			goto free_heap;
		}
		heap_dev->coherent_dma_mask = DMA_BIT_MASK(64);
		heap_dev->dma_mask = &heap_dev->coherent_dma_mask;
		sys_heap->dev = heap_dev;
	}

	sys_heap->pool_list = dynamic_page_pool_create_pools();
	if (IS_ERR(sys_heap->pool_list)) {
		ret = PTR_ERR(sys_heap->pool_list);
		goto free_dev;
	}

	heap = dma_heap_add(&exp_info);
	if (IS_ERR(heap)) {
		ret = PTR_ERR(heap);
		goto free_pools;
	}

	pr_info("%s: DMA-BUF Heap: Created '%s'\n", __func__, name);
	return 0;

free_pools:
	dynamic_page_pool_release_pools(sys_heap->pool_list);

free_dev:
	kfree(heap_dev);

free_heap:
	kfree(sys_heap);

out:
	pr_err("%s: Failed to create '%s', error is %d\n", __func__, name, ret);

	return ret;
}
