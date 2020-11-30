// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2019, 2020 Linaro Ltd.
 *
 * Portions based off of Andrew Davis' SRAM heap:
 * Copyright (C) 2019 Texas Instruments Incorporated - http://www.ti.com/
 *	Andrew F. Davis <afd@ti.com>
 *
 * These ops were base of the ops in drivers/dma-buf/heaps/system-heap.c from
 * https://lore.kernel.org/lkml/20201017013255.43568-2-john.stultz@linaro.org/
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

#include "qcom_sg_ops.h"

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sgtable_sg(table, sg, i) {
		sg_set_page(new_sg, sg_page(sg), sg->length, sg->offset);
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static int qcom_sg_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;
	struct sg_table *table;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(&buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;

	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void qcom_sg_detach(struct dma_buf *dmabuf,
			       struct dma_buf_attachment *attachment)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a = attachment->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(a->table);
	kfree(a->table);
	kfree(a);
}

static struct sg_table *qcom_sg_map_dma_buf(struct dma_buf_attachment *attachment,
						enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct sg_table *table = a->table;
	struct qcom_sg_buffer *buffer;
	unsigned long attrs = 0;
	int ret;

	buffer = attachment->dmabuf->priv;
	if (buffer->secure)
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	ret = dma_map_sgtable(attachment->dev, table, direction, attrs);
	if (ret)
		return ERR_PTR(ret);

	a->mapped = true;
	return table;
}

static void qcom_sg_unmap_dma_buf(struct dma_buf_attachment *attachment,
				      struct sg_table *table,
				      enum dma_data_direction direction)
{
	struct dma_heap_attachment *a = attachment->priv;
	struct qcom_sg_buffer *buffer;
	unsigned long attrs = 0;

	buffer = attachment->dmabuf->priv;
	if (buffer->secure)
		attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	a->mapped = false;
	dma_unmap_sgtable(attachment->dev, table, direction, attrs);
}

static int qcom_sg_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
						enum dma_data_direction direction)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	if (buffer->secure)
		return -EINVAL;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->len);

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sgtable_for_cpu(a->dev, a->table, direction);
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int qcom_sg_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					      enum dma_data_direction direction)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct dma_heap_attachment *a;

	if (buffer->secure)
		return -EINVAL;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->len);

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sgtable_for_device(a->dev, a->table, direction);
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static int qcom_sg_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	struct sg_table *table = &buffer->sg_table;
	unsigned long addr = vma->vm_start;
	struct sg_page_iter piter;
	int ret;

	if (buffer->secure)
		return -EINVAL;

	for_each_sgtable_page(table, &piter, vma->vm_pgoff) {
		struct page *page = sg_page_iter_page(&piter);

		ret = remap_pfn_range(vma, addr, page_to_pfn(page), PAGE_SIZE,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += PAGE_SIZE;
		if (addr >= vma->vm_end)
			return 0;
	}
	return 0;
}

static void *qcom_sg_do_vmap(struct qcom_sg_buffer *buffer)
{
	struct sg_table *table = &buffer->sg_table;
	int npages = PAGE_ALIGN(buffer->len) / PAGE_SIZE;
	struct page **pages = vmalloc(sizeof(struct page *) * npages);
	struct page **tmp = pages;
	struct sg_page_iter piter;
	void *vaddr;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	for_each_sgtable_page(table, &piter, 0) {
		WARN_ON(tmp - pages >= npages);
		*tmp++ = sg_page_iter_page(&piter);
	}

	vaddr = vmap(pages, npages, VM_MAP, PAGE_KERNEL);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static void *qcom_sg_vmap(struct dma_buf *dmabuf)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	void *vaddr;

	if (buffer->secure)
		return ERR_PTR(-EINVAL);

	mutex_lock(&buffer->lock);
	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		vaddr = buffer->vaddr;
		goto out;
	}

	vaddr = qcom_sg_do_vmap(buffer);
	if (IS_ERR(vaddr))
		goto out;

	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
out:
	mutex_unlock(&buffer->lock);

	return vaddr;
}

static void qcom_sg_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;

	if (buffer->secure)
		return;

	mutex_lock(&buffer->lock);
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
	mutex_unlock(&buffer->lock);
}

static void qcom_sg_release(struct dma_buf *dmabuf)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;

	buffer->free(buffer);
}

static int qcom_sg_get_flags(struct dma_buf *dmabuf,
				 unsigned long *flags)
{
	struct qcom_sg_buffer *buffer = dmabuf->priv;
	*flags = buffer->vmids;

	return 0;
}

const struct dma_buf_ops qcom_sg_buf_ops = {
	.attach = qcom_sg_attach,
	.detach = qcom_sg_detach,
	.map_dma_buf = qcom_sg_map_dma_buf,
	.unmap_dma_buf = qcom_sg_unmap_dma_buf,
	.begin_cpu_access = qcom_sg_dma_buf_begin_cpu_access,
	.end_cpu_access = qcom_sg_dma_buf_end_cpu_access,
	.mmap = qcom_sg_mmap,
	.vmap = qcom_sg_vmap,
	.vunmap = qcom_sg_vunmap,
	.release = qcom_sg_release,
	.get_flags = qcom_sg_get_flags,
};
