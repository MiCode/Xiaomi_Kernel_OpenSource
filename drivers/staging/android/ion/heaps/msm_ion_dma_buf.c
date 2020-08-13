// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2011 Google, Inc.
 * Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/msm_dma_iommu_mapping.h>
#define CREATE_TRACE_POINTS
#include <trace/events/ion.h>

#include "msm_ion_priv.h"
#include "ion_secure_util.h"

static void *msm_ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = ion_heap_map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "ion_heap_map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

static void msm_ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	if (buffer->kmap_cnt == 0) {
		pr_warn_ratelimited("ION client likely missing a call to dma_buf_vmap, pid:%d\n",
				    current->pid);
		return;
	}

	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		ion_heap_unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sg(table->sgl, sg, table->nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		sg_dma_address(new_sg) = 0;
		sg_dma_len(new_sg) = 0;
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void free_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

static int msm_ion_dma_buf_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a;
	struct sg_table *table;
	struct ion_buffer *buffer = dmabuf->priv;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	a->mapped = false;
	INIT_LIST_HEAD(&a->list);

	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void msm_ion_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct ion_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);
	free_duped_table(a->table);

	kfree(a);
}

bool ion_buffer_cached(struct ion_buffer *buffer)
{
	return !!(buffer->flags & ION_FLAG_CACHED);
}

static struct sg_table
*msm_ion_map_dma_buf(struct dma_buf_attachment *attachment,
		     enum dma_data_direction direction)
{
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct sg_table *table;
	int count, map_attrs;
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	unsigned long ino = file_inode(attachment->dmabuf->file)->i_ino;

	table = a->table;

	map_attrs = attachment->dma_map_attrs;
	if (!(buffer->flags & ION_FLAG_CACHED) ||
	    !hlos_accessible_buffer(buffer))
		map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->lock);
	if (map_attrs & DMA_ATTR_SKIP_CPU_SYNC)
		trace_ion_dma_map_cmo_skip(attachment->dev,
					   ino,
					   ion_buffer_cached(buffer),
					   hlos_accessible_buffer(buffer),
					   attachment->dma_map_attrs,
					   direction);
	else
		trace_ion_dma_map_cmo_apply(attachment->dev,
					    ino,
					    ion_buffer_cached(buffer),
					    hlos_accessible_buffer(buffer),
					    attachment->dma_map_attrs,
					    direction);

	if (map_attrs & DMA_ATTR_DELAYED_UNMAP) {
		count = msm_dma_map_sg_attrs(attachment->dev, table->sgl,
					     table->nents, direction,
					     attachment->dmabuf, map_attrs);
	} else {
		count = dma_map_sg_attrs(attachment->dev, table->sgl,
					 table->nents, direction,
					 map_attrs);
	}

	if (count <= 0) {
		mutex_unlock(&buffer->lock);
		return ERR_PTR(-ENOMEM);
	}

	a->mapped = true;
	mutex_unlock(&buffer->lock);
	return table;
}

void ion_prepare_sgl_for_force_dma_sync(struct sg_table *table)
{
	struct scatterlist *sg;
	int i;

	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		/*
		 * this will set up dma addresses for the sglist -- it is not
		 * technically correct as per the dma api -- a specific
		 * device isn't really taking ownership here.  However, in
		 * practice on our systems the only dma_address space is
		 * physical addresses.
		 */
		for_each_sg(table->sgl, sg, table->nents, i) {
			sg_dma_address(sg) = sg_phys(sg);
			sg_dma_len(sg) = sg->length;
		}
	}
}

static void msm_ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	int map_attrs;
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_dma_buf_attachment *a = attachment->priv;
	unsigned long ino = file_inode(attachment->dmabuf->file)->i_ino;

	map_attrs = attachment->dma_map_attrs;
	if (!(buffer->flags & ION_FLAG_CACHED) ||
	    !hlos_accessible_buffer(buffer))
		map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->lock);
	if (map_attrs & DMA_ATTR_SKIP_CPU_SYNC)
		trace_ion_dma_unmap_cmo_skip(attachment->dev,
					     ino,
					     ion_buffer_cached(buffer),
					     hlos_accessible_buffer(buffer),
					     attachment->dma_map_attrs,
					     direction);
	else
		trace_ion_dma_unmap_cmo_apply(attachment->dev,
					      ino,
					      ion_buffer_cached(buffer),
					      hlos_accessible_buffer(buffer),
					      attachment->dma_map_attrs,
					      direction);

	if (map_attrs & DMA_ATTR_DELAYED_UNMAP)
		msm_dma_unmap_sg_attrs(attachment->dev, table->sgl,
				       table->nents, direction,
				       attachment->dmabuf,
				       map_attrs);
	else
		dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents,
				   direction, map_attrs);
	a->mapped = false;
	mutex_unlock(&buffer->lock);
}

void ion_pages_sync_for_device(struct device *dev, struct page *page,
			       size_t size, enum dma_data_direction dir)
{
	struct scatterlist sg;

	sg_init_table(&sg, 1);
	sg_set_page(&sg, page, size, 0);
	/*
	 * This is not correct - sg_dma_address needs a dma_addr_t that is valid
	 * for the targeted device, but this works on the currently targeted
	 * hardware.
	 */
	sg_dma_address(&sg) = page_to_phys(page);
	dma_sync_sg_for_device(dev, &sg, 1, dir);
}

static int msm_ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	int ret = 0;

	if (!hlos_accessible_buffer(buffer)) {
		pr_err_ratelimited("%s: this buffer cannot be mapped to userspace\n",
				   __func__);
		return -EINVAL;
	}

	if (!(buffer->flags & ION_FLAG_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = ion_heap_map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);

	return ret;
}

static void msm_ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;

	msm_dma_buf_freed(buffer);
	ion_free(buffer);
}

static void *msm_ion_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	void *vaddr = ERR_PTR(-EINVAL);

	if (hlos_accessible_buffer(buffer)) {
		mutex_lock(&buffer->lock);
		vaddr = msm_ion_buffer_kmap_get(buffer);
		mutex_unlock(&buffer->lock);
	} else {
		pr_warn_ratelimited("heap %s doesn't support map_kernel\n",
				    buffer->heap->name);
	}

	return vaddr;
}

static void msm_ion_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct ion_buffer *buffer = dmabuf->priv;

	if (hlos_accessible_buffer(buffer)) {
		mutex_lock(&buffer->lock);
		msm_ion_buffer_kmap_put(buffer);
		mutex_unlock(&buffer->lock);
	}
}

static int ion_sgl_sync_range(struct device *dev, struct scatterlist *sgl,
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

static int msm_ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					    enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;
	unsigned long ino = file_inode(dmabuf->file)->i_ino;
	int ret = 0;

	if (!hlos_accessible_buffer(buffer)) {
		trace_ion_begin_cpu_access_cmo_skip(NULL, ino,
						    ion_buffer_cached(buffer),
						    false, direction);
		ret = -EPERM;
		goto out;
	}

	if (!(buffer->flags & ION_FLAG_CACHED)) {
		trace_ion_begin_cpu_access_cmo_skip(NULL, ino, false, true,
						    direction);
		goto out;
	}

	mutex_lock(&buffer->lock);

	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct device *dev = msm_ion_heap_device(buffer->heap);
		struct sg_table *table = buffer->sg_table;

		dma_sync_sg_for_cpu(dev, table->sgl, table->nents, direction);

		trace_ion_begin_cpu_access_cmo_apply(dev, ino, true, true,
						     direction);

		mutex_unlock(&buffer->lock);
		goto out;
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped) {
			trace_ion_begin_cpu_access_notmapped(a->dev,
							     ino,
							     true, true,
							     direction);
			continue;
		}

		dma_sync_sg_for_cpu(a->dev, a->table->sgl,
				    a->table->nents, direction);

		trace_ion_begin_cpu_access_cmo_apply(a->dev, ino, true,
						     true, direction);
	}
	mutex_unlock(&buffer->lock);
out:
	return ret;
}

static int msm_ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					  enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;
	unsigned long ino = file_inode(dmabuf->file)->i_ino;
	int ret = 0;

	if (!hlos_accessible_buffer(buffer)) {
		trace_ion_end_cpu_access_cmo_skip(NULL, ino,
						  ion_buffer_cached(buffer),
						  false, direction);
		ret = -EPERM;
		goto out;
	}

	if (!(buffer->flags & ION_FLAG_CACHED)) {
		trace_ion_end_cpu_access_cmo_skip(NULL, ino, false, true,
						  direction);
		goto out;
	}

	mutex_lock(&buffer->lock);
	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct device *dev = msm_ion_heap_device(buffer->heap);
		struct sg_table *table = buffer->sg_table;

		dma_sync_sg_for_device(dev, table->sgl, table->nents,
				       direction);

		trace_ion_end_cpu_access_cmo_apply(dev, ino, true,
						   true, direction);
		mutex_unlock(&buffer->lock);
		goto out;
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped) {
			trace_ion_end_cpu_access_notmapped(a->dev,
							   ino,
							   true, true,
							   direction);
			continue;
		}

		dma_sync_sg_for_device(a->dev, a->table->sgl, a->table->nents,
				       direction);

		trace_ion_end_cpu_access_cmo_apply(a->dev, ino, true,
						   true, direction);
	}
	mutex_unlock(&buffer->lock);

out:
	return ret;
}

static int msm_ion_dma_buf_begin_cpu_access_partial(struct dma_buf *dmabuf,
						enum dma_data_direction dir,
						unsigned int offset,
						unsigned int len)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;
	unsigned long ino = file_inode(dmabuf->file)->i_ino;
	int ret = 0;

	if (!hlos_accessible_buffer(buffer)) {
		trace_ion_begin_cpu_access_cmo_skip(NULL, ino,
						    ion_buffer_cached(buffer),
						    false, dir);
		ret = -EPERM;
		goto out;
	}

	if (!(buffer->flags & ION_FLAG_CACHED)) {
		trace_ion_begin_cpu_access_cmo_skip(NULL, ino, false, true,
						    dir);
		goto out;
	}

	mutex_lock(&buffer->lock);
	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct device *dev = msm_ion_heap_device(buffer->heap);
		struct sg_table *table = buffer->sg_table;

		ret = ion_sgl_sync_range(dev, table->sgl, table->nents,
					 offset, len, dir, true);

		if (!ret)
			trace_ion_begin_cpu_access_cmo_apply(dev, ino,
							     true, true, dir);
		else
			trace_ion_begin_cpu_access_cmo_skip(dev, ino,
							    true, true, dir);
		mutex_unlock(&buffer->lock);
		goto out;
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		int tmp = 0;

		if (!a->mapped) {
			trace_ion_begin_cpu_access_notmapped(a->dev,
							     ino,
							     true, true,
							     dir);
			continue;
		}

		tmp = ion_sgl_sync_range(a->dev, a->table->sgl, a->table->nents,
					 offset, len, dir, true);

		if (!tmp) {
			trace_ion_begin_cpu_access_cmo_apply(a->dev, ino,
							     true, true, dir);
		} else {
			trace_ion_begin_cpu_access_cmo_skip(a->dev, ino,
							    true, true, dir);
			ret = tmp;
		}
	}
	mutex_unlock(&buffer->lock);

out:
	return ret;
}

static int msm_ion_dma_buf_end_cpu_access_partial(struct dma_buf *dmabuf,
					      enum dma_data_direction direction,
					      unsigned int offset,
					      unsigned int len)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;
	unsigned long ino = file_inode(dmabuf->file)->i_ino;

	int ret = 0;

	if (!hlos_accessible_buffer(buffer)) {
		trace_ion_end_cpu_access_cmo_skip(NULL, ino,
						  ion_buffer_cached(buffer),
						  false, direction);
		ret = -EPERM;
		goto out;
	}

	if (!(buffer->flags & ION_FLAG_CACHED)) {
		trace_ion_end_cpu_access_cmo_skip(NULL, ino, false,
						  true, direction);
		goto out;
	}

	mutex_lock(&buffer->lock);
	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct device *dev = msm_ion_heap_device(buffer->heap);
		struct sg_table *table = buffer->sg_table;

		ret = ion_sgl_sync_range(dev, table->sgl, table->nents,
					 offset, len, direction, false);

		if (!ret)
			trace_ion_end_cpu_access_cmo_apply(dev, ino,
							   true, true,
							   direction);
		else
			trace_ion_end_cpu_access_cmo_skip(dev, ino,
							  true, true,
							  direction);

		mutex_unlock(&buffer->lock);
		goto out;
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		int tmp = 0;

		if (!a->mapped) {
			trace_ion_end_cpu_access_notmapped(a->dev,
							   ino,
							   true, true,
							   direction);
			continue;
		}

		tmp = ion_sgl_sync_range(a->dev, a->table->sgl, a->table->nents,
					 offset, len, direction, false);

		if (!tmp) {
			trace_ion_end_cpu_access_cmo_apply(a->dev, ino,
							   true, true,
							   direction);

		} else {
			trace_ion_end_cpu_access_cmo_skip(a->dev, ino, true,
							  true, direction);
			ret = tmp;
		}
	}
	mutex_unlock(&buffer->lock);

out:
	return ret;
}

static int msm_ion_dma_buf_get_flags(struct dma_buf *dmabuf,
				 unsigned long *flags)
{
	struct ion_buffer *buffer = dmabuf->priv;
	*flags = buffer->flags;

	return 0;
}

const struct dma_buf_ops msm_ion_dma_buf_ops = {
	.map_dma_buf = msm_ion_map_dma_buf,
	.unmap_dma_buf = msm_ion_unmap_dma_buf,
	.mmap = msm_ion_mmap,
	.release = msm_ion_dma_buf_release,
	.attach = msm_ion_dma_buf_attach,
	.detach = msm_ion_dma_buf_detatch,
	.begin_cpu_access = msm_ion_dma_buf_begin_cpu_access,
	.end_cpu_access = msm_ion_dma_buf_end_cpu_access,
	.begin_cpu_access_partial = msm_ion_dma_buf_begin_cpu_access_partial,
	.end_cpu_access_partial = msm_ion_dma_buf_end_cpu_access_partial,
	.vmap = msm_ion_dma_buf_vmap,
	.vunmap = msm_ion_dma_buf_vunmap,
	.get_flags = msm_ion_dma_buf_get_flags,
};
