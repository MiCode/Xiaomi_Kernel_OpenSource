// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "mem-buf-private.h"

static struct sg_table *dup_hh_sgl_desc_to_sgt(struct hh_sgl_desc *sgl_desc)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg;

	if (!sgl_desc || !sgl_desc->n_sgl_entries)
		return ERR_PTR(-EINVAL);

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, sgl_desc->n_sgl_entries, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	for_each_sg(new_table->sgl, sg, new_table->nents, i) {
		sg_set_page(sg, phys_to_page(sgl_desc->sgl_entries[i].ipa_base),
			    sgl_desc->sgl_entries[i].size, 0);
		sg_dma_address(sg) = 0;
		sg_dma_len(sg) = 0;
	}

	return new_table;
}

static void free_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

struct mem_buf_dma_buf_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool dma_mapped;
};

static int mem_buf_dma_buf_attach(struct dma_buf *dmabuf,
				  struct dma_buf_attachment *attachment)
{
	struct mem_buf_dma_buf_attachment *a;
	struct sg_table *table;
	struct mem_buf_import *import_buf = dmabuf->priv;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_hh_sgl_desc_to_sgt(import_buf->sgl_desc);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	a->dma_mapped = false;
	INIT_LIST_HEAD(&a->list);

	attachment->priv = a;

	mutex_lock(&import_buf->lock);
	list_add(&a->list, &import_buf->attachments);
	mutex_unlock(&import_buf->lock);

	return 0;
}

static void mem_buf_dma_buf_detatch(struct dma_buf *dmabuf,
				    struct dma_buf_attachment *attachment)
{
	struct mem_buf_dma_buf_attachment *a = attachment->priv;
	struct mem_buf_import *import_buf = dmabuf->priv;

	mutex_lock(&import_buf->lock);
	list_del(&a->list);
	mutex_unlock(&import_buf->lock);
	free_duped_table(a->table);
	kfree(a);
}

static struct sg_table *mem_buf_dma_map_attachment(
					struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct mem_buf_dma_buf_attachment *a = attachment->priv;
	struct mem_buf_import *buffer = attachment->dmabuf->priv;
	struct sg_table *table;
	int count, map_attrs;

	table = a->table;
	map_attrs = attachment->dma_map_attrs;

	mutex_lock(&buffer->lock);
	count = dma_map_sg_attrs(attachment->dev, table->sgl, table->nents,
				 direction, map_attrs);

	if (count <= 0) {
		mutex_unlock(&buffer->lock);
		return ERR_PTR(-ENOMEM);
	}

	a->dma_mapped = true;
	mutex_unlock(&buffer->lock);
	return table;
}

static void mem_buf_dma_unmap_attachment(struct dma_buf_attachment *attachment,
					 struct sg_table *table,
					 enum dma_data_direction direction)
{
	int map_attrs;
	struct mem_buf_import *buffer = attachment->dmabuf->priv;
	struct mem_buf_dma_buf_attachment *a = attachment->priv;

	map_attrs = attachment->dma_map_attrs;

	mutex_lock(&buffer->lock);
	dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents, direction,
			   map_attrs);
	a->dma_mapped = false;
	mutex_unlock(&buffer->lock);
}

static int mem_buf_map_user(struct mem_buf_import *import_buf,
			    struct vm_area_struct *vma)
{
	struct hh_sgl_desc *sgl_desc = import_buf->sgl_desc;
	unsigned long addr = vma->vm_start;
	unsigned long offset = vma->vm_pgoff * PAGE_SIZE;
	int i, ret;

	for (i = 0; i < sgl_desc->n_sgl_entries; i++) {
		struct page *page =
			phys_to_page(sgl_desc->sgl_entries[i].ipa_base);
		unsigned long remainder = vma->vm_end - addr;
		unsigned long len = sgl_desc->sgl_entries[i].size;

		if (offset >= len) {
			offset -= len;
			continue;
		} else if (offset) {
			page += offset / PAGE_SIZE;
			len = sgl_desc->sgl_entries[i].size - offset;
			offset = 0;
		}

		len = min(len, remainder);
		ret = remap_pfn_range(vma, addr, page_to_pfn(page), len,
				      vma->vm_page_prot);
		if (ret)
			return ret;
		addr += len;
		if (addr >= vma->vm_end)
			return 0;
	}

	return 0;
}

static void *mem_buf_map_kernel(struct mem_buf_import *import_buf)
{
	void *vaddr;
	int npages = PAGE_ALIGN(import_buf->size) / PAGE_SIZE;
	struct page **pages =
		vmalloc(array_size(npages, sizeof(struct page *)));
	struct page **tmp = pages;
	struct hh_sgl_desc *sgl_desc = import_buf->sgl_desc;
	int i, j, n_pages_this_seg;
	u64 seg_ipa_base, seg_size;
	struct page *page;

	if (!pages)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < sgl_desc->n_sgl_entries; i++) {
		seg_ipa_base = sgl_desc->sgl_entries[i].ipa_base;
		seg_size = sgl_desc->sgl_entries[i].size;
		n_pages_this_seg = PAGE_ALIGN(seg_size) / PAGE_SIZE;
		page = phys_to_page(seg_ipa_base);

		BUG_ON(i >= npages);
		for (j = 0; j < n_pages_this_seg; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(pages, npages, VM_MAP, PAGE_KERNEL);
	vfree(pages);

	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static void mem_buf_unmap_kernel(struct mem_buf_import *import_buf)
{
	vunmap(import_buf->vaddr);
}

static int mem_buf_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct mem_buf_import *import_buf = dmabuf->priv;
	int ret = 0;

	mutex_lock(&import_buf->lock);
	ret = mem_buf_map_user(import_buf, vma);
	mutex_unlock(&import_buf->lock);

	if (ret)
		pr_err_ratelimited("%s: failure mapping buffer to userspace\n",
				   __func__);

	return ret;
}

static void mem_buf_dma_buf_release(struct dma_buf *dmabuf)
{
	struct mem_buf_import *import_buf = dmabuf->priv;

	mem_buf_unimport_dma_buf(import_buf);
}

static void *mem_buf_buffer_kmap_get(struct mem_buf_import *import_buf)
{
	void *vaddr;

	if (import_buf->kmap_cnt) {
		import_buf->kmap_cnt++;
		return import_buf->vaddr;
	}
	vaddr = mem_buf_map_kernel(import_buf);
	if (IS_ERR(vaddr))
		return vaddr;
	import_buf->vaddr = vaddr;
	import_buf->kmap_cnt++;
	return vaddr;
}

static void mem_buf_buffer_kmap_put(struct mem_buf_import *import_buf)
{
	if (import_buf->kmap_cnt == 0) {
		pr_warn_ratelimited("membuf client likely missing a call to dma_buf_kmap or dma_buf_vmap, pid:%d\n",
				    current->pid);
		return;
	}

	import_buf->kmap_cnt--;
	if (!import_buf->kmap_cnt) {
		mem_buf_unmap_kernel(import_buf);
		import_buf->vaddr = NULL;
	}
}

static void *mem_buf_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct mem_buf_import *import_buf = dmabuf->priv;
	void *vaddr;

	mutex_lock(&import_buf->lock);
	vaddr = mem_buf_buffer_kmap_get(import_buf);
	mutex_unlock(&import_buf->lock);

	return vaddr;
}

static void mem_buf_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct mem_buf_import *import_buf = dmabuf->priv;

	mutex_lock(&import_buf->lock);
	mem_buf_buffer_kmap_put(import_buf);
	mutex_unlock(&import_buf->lock);
}

static void *mem_buf_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	/*
	 * TODO: Once clients remove their hacks where they assume kmap(ed)
	 * addresses are virtually contiguous implement this properly
	 */
	void *vaddr = mem_buf_dma_buf_vmap(dmabuf);

	if (IS_ERR(vaddr))
		return vaddr;

	return vaddr + offset * PAGE_SIZE;
}

static void mem_buf_dma_buf_kunmap(struct dma_buf *dmabuf, unsigned long offset,
			       void *ptr)
{
	/*
	 * TODO: Once clients remove their hacks where they assume kmap(ed)
	 * addresses are virtually contiguous implement this properly
	 */
	mem_buf_dma_buf_vunmap(dmabuf, ptr);
}

static int mem_buf_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					    enum dma_data_direction direction)
{
	struct mem_buf_import *import_buf = dmabuf->priv;
	struct mem_buf_dma_buf_attachment *a;

	mutex_lock(&import_buf->lock);
	list_for_each_entry(a, &import_buf->attachments, list) {
		if (!a->dma_mapped)
			continue;

		dma_sync_sg_for_cpu(a->dev, a->table->sgl,
				    a->table->nents, direction);
	}
	mutex_unlock(&import_buf->lock);
	return 0;
}

static int mem_buf_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction)
{
	struct mem_buf_import *import_buf = dmabuf->priv;
	struct mem_buf_dma_buf_attachment *a;

	mutex_lock(&import_buf->lock);
	list_for_each_entry(a, &import_buf->attachments, list) {
		if (!a->dma_mapped)
			continue;

		dma_sync_sg_for_device(a->dev, a->table->sgl, a->table->nents,
				       direction);
	}
	mutex_unlock(&import_buf->lock);
	return 0;
}

static int mem_buf_sgl_sync_range(struct device *dev, struct scatterlist *sgl,
				  unsigned int nents, unsigned long offset,
				  unsigned long length,
				  enum dma_data_direction dir, bool for_cpu)
{
	int i;
	struct scatterlist *sg;
	unsigned int len = 0;
	dma_addr_t sg_dma_addr;

	for_each_sg(sgl, sg, nents, i) {
		if (sg_dma_len(sg) == 0)
			break;

		if (i > 0) {
			pr_warn_ratelimited("Partial cmo only supported with 1 segment\n"
				"is dma_set_max_seg_size being set on dev:%s\n",
				dev_name(dev));
			return -EINVAL;
		}
	}

	for_each_sg(sgl, sg, nents, i) {
		unsigned int sg_offset, sg_left, size = 0;

		if (i == 0)
			sg_dma_addr = sg_dma_address(sg);

		len += sg->length;
		if (len <= offset) {
			sg_dma_addr += sg->length;
			continue;
		}

		sg_left = len - offset;
		sg_offset = sg->length - sg_left;

		size = (length < sg_left) ? length : sg_left;
		if (for_cpu)
			dma_sync_single_range_for_cpu(dev, sg_dma_addr,
						      sg_offset, size, dir);
		else
			dma_sync_single_range_for_device(dev, sg_dma_addr,
							 sg_offset, size, dir);

		offset += size;
		length -= size;
		sg_dma_addr += sg->length;

		if (length == 0)
			break;
	}

	return 0;
}

static int mem_buf_dma_buf_begin_cpu_access_partial(struct dma_buf *dmabuf,
						enum dma_data_direction dir,
						unsigned int offset,
						unsigned int len)
{
	struct mem_buf_import *import_buf = dmabuf->priv;
	struct mem_buf_dma_buf_attachment *a;
	int ret = 0;

	mutex_lock(&import_buf->lock);
	list_for_each_entry(a, &import_buf->attachments, list) {
		if (!a->dma_mapped)
			continue;

		ret = mem_buf_sgl_sync_range(a->dev, a->table->sgl,
					     a->table->nents, offset, len, dir,
					     true);

	}
	mutex_unlock(&import_buf->lock);
	return ret;
}

static int mem_buf_dma_buf_end_cpu_access_partial(struct dma_buf *dmabuf,
					      enum dma_data_direction direction,
					      unsigned int offset,
					      unsigned int len)
{
	struct mem_buf_import *import_buf = dmabuf->priv;
	struct mem_buf_dma_buf_attachment *a;
	int ret = 0;

	mutex_lock(&import_buf->lock);
	list_for_each_entry(a, &import_buf->attachments, list) {
		ret = mem_buf_sgl_sync_range(a->dev, a->table->sgl,
					     a->table->nents, offset, len,
					     direction, false);
	}
	mutex_unlock(&import_buf->lock);
	return ret;
}

const struct dma_buf_ops mem_buf_dma_buf_ops = {
	.map_dma_buf = mem_buf_dma_map_attachment,
	.unmap_dma_buf = mem_buf_dma_unmap_attachment,
	.mmap = mem_buf_mmap,
	.release = mem_buf_dma_buf_release,
	.attach = mem_buf_dma_buf_attach,
	.detach = mem_buf_dma_buf_detatch,
	.begin_cpu_access = mem_buf_dma_buf_begin_cpu_access,
	.end_cpu_access = mem_buf_dma_buf_end_cpu_access,
	.begin_cpu_access_partial = mem_buf_dma_buf_begin_cpu_access_partial,
	.end_cpu_access_partial = mem_buf_dma_buf_end_cpu_access_partial,
	.map = mem_buf_dma_buf_kmap,
	.unmap = mem_buf_dma_buf_kunmap,
	.vmap = mem_buf_dma_buf_vmap,
	.vunmap = mem_buf_dma_buf_vunmap,
};
