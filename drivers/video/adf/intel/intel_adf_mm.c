/*
 * Copyright (C) 2014, Intel Corporation.
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
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/slab.h>

#include "intel_adf_device.h"
#include "intel_adf_mm.h"

#define INTEL_DMA_FD_FLAGS		O_CLOEXEC

/*----------------------------------------------------------------------------*/
struct intel_dma_buf {
	struct intel_adf_mm *mm;

	struct mutex lock;
	struct kref buf_ref;
	enum intel_dma_buf_type type;
	/*back storage info*/
	struct intel_dc_buffer *buf;

	/*vmapping*/
	void *vmapping;
	int vmapping_ref;
};

static void intel_dma_buf_destroy(struct kref *ref)
{
	struct intel_dma_buf *priv =
		container_of(ref, struct intel_dma_buf, buf_ref);
	struct intel_dc_buffer *dc_buf = priv->buf;
	struct intel_adf_mm *mm = priv->mm;

	intel_dc_memory_free(mm->mem, dc_buf);
	kfree(priv);
}

static struct intel_dma_buf *intel_dma_buf_alloc(struct intel_adf_mm *mm,
	struct page **pages, u32 page_num, void *vaddr, u32 size,
	enum intel_dma_buf_type type)
{
	struct device *dev = mm->parent;
	struct intel_dma_buf *priv;
	struct intel_dc_buffer *buf;
	u32 handle;
	int err = 0;

	priv = kzalloc(sizeof(struct intel_dma_buf), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "%s: failed to allocate buffer private\n",
			__func__);
		return ERR_PTR(-ENOMEM);
	}

	/*import this buffer to DC memory*/
	handle = (u32)priv;
	buf = intel_dc_memory_import(mm->mem, handle, pages, page_num);
	if (IS_ERR(buf)) {
		dev_err(dev, "%s: failed to import pages\n", __func__);
		err = PTR_ERR(buf);
		goto out_err0;
	}

	priv->mm = mm;
	priv->type = type;
	priv->buf = buf;
	if (vaddr && (type == INTEL_DMA_BUF_ALLOCATED)) {
		priv->vmapping = vaddr;
		priv->vmapping_ref++;
	}
	kref_init(&priv->buf_ref);
	mutex_init(&priv->lock);

	return priv;
out_err0:
	kfree(priv);
	return ERR_PTR(err);
}

static inline void intel_dma_buf_get(struct intel_dma_buf *buf)
{
	kref_get(&buf->buf_ref);
}

static inline int intel_dma_buf_put(struct intel_dma_buf *buf)
{
	return kref_put(&buf->buf_ref, intel_dma_buf_destroy);
}

static inline u32 intel_dma_buf_get_gtt(struct intel_dma_buf *buf)
{
	struct intel_dc_buffer *dc_buf = buf->buf;
	return dc_buf->dc_mem_addr;
}

static inline u32 intel_dma_buf_get_size(struct intel_dma_buf *buf)
{
	struct intel_dc_buffer *dc_buf = buf->buf;
	return dc_buf->n_pages << PAGE_SHIFT;
}

/*---------------------------------------------------------------------------*/

static int intel_attach(struct dma_buf *buf, struct device *dev,
	struct dma_buf_attachment *attachment)
{
	intel_dma_buf_get(buf->priv);
	return 0;
}

static void intel_detach(struct dma_buf *buf,
	struct dma_buf_attachment *attachment)
{
	intel_dma_buf_put(buf->priv);
}

static struct sg_table *intel_map_dma_buf(
	struct dma_buf_attachment *attachment,
	enum dma_data_direction direction)
{
	struct intel_dma_buf *priv = attachment->dmabuf->priv;
	struct intel_dc_buffer *dc_buf = priv->buf;
	struct sg_table *table;
	int err;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&priv->lock);

	err = sg_alloc_table_from_pages(table, dc_buf->pages,
				dc_buf->n_pages,
				0,
				dc_buf->n_pages << PAGE_SHIFT,
				GFP_KERNEL);
	if (err)
		goto out_err0;

	mutex_unlock(&priv->lock);

	return table;
out_err0:
	kfree(table);
	mutex_unlock(&priv->lock);
	return ERR_PTR(err);
}

static void intel_unmap_dma_buf(struct dma_buf_attachment *attachment,
	struct sg_table *sg, enum dma_data_direction direction)
{
	struct intel_dma_buf *priv = attachment->dmabuf->priv;

	mutex_lock(&priv->lock);

	sg_free_table(sg);
	kfree(sg);

	mutex_unlock(&priv->lock);
}

static void intel_release(struct dma_buf *buf)
{

}

static void *intel_kmap_atomic(struct dma_buf *buf, unsigned long page_num)
{
	return NULL;
}

static void *intel_kmap(struct dma_buf *buf, unsigned long page_num)
{
	return NULL;
}

static int intel_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	int page_num = 0;
	int i;
	unsigned long address = 0;
	int ret;
	unsigned long pfn;
	struct intel_dma_buf *priv = vma->vm_private_data;
	struct intel_dc_buffer *dc_buf = priv->buf;

	page_num = PAGE_ALIGN((vma->vm_end - vma->vm_start)) >> PAGE_SHIFT;

	if (page_num != dc_buf->n_pages)
		return VM_FAULT_NOPAGE;

	address = (unsigned long)vmf->virtual_address;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	for (i = 0; i < page_num; i++) {
		pfn = page_to_pfn(dc_buf->pages[i]);

		ret = vm_insert_mixed(vma, address, pfn);
		if (unlikely((ret == -EBUSY) || (ret != 0 && i > 0)))
			break;
		else if (unlikely(ret != 0)) {
			ret = (ret == -ENOMEM) ? VM_FAULT_OOM : VM_FAULT_SIGBUS;
			return ret;
		}

		address += PAGE_SIZE;
	}

	return VM_FAULT_NOPAGE;
}

static void intel_vm_open(struct vm_area_struct *vma)
{
}

static void intel_vm_close(struct vm_area_struct *vma)
{
}

static struct vm_operations_struct intel_vm_ops = {
	.fault = intel_vm_fault,
	.open = intel_vm_open,
	.close = intel_vm_close
};

static int intel_mmap(struct dma_buf *buf, struct vm_area_struct *vma)
{
	struct intel_dma_buf *priv = buf->priv;

	if (vma->vm_pgoff != 0)
		return -EACCES;
	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EACCES;

	vma->vm_ops = &intel_vm_ops;
	vma->vm_private_data = (void *)priv;
	vma->vm_flags |= VM_IO | VM_MIXEDMAP | VM_DONTEXPAND;

	return 0;
}

static void *intel_vmap(struct dma_buf *buf)
{
	struct intel_dma_buf *priv = buf->priv;
	struct intel_dc_buffer *dc_buf = priv->buf;

	mutex_lock(&priv->lock);

	if (priv->vmapping_ref)
		goto out_success;

	priv->vmapping = vmap(dc_buf->pages, dc_buf->n_pages,
				0, PAGE_KERNEL);
	if (!priv->vmapping) {
		mutex_unlock(&priv->lock);
		return ERR_PTR(-ENOMEM);
	}
out_success:
	priv->vmapping_ref++;
	mutex_unlock(&priv->lock);
	return priv->vmapping;
}

static void intel_vunmap(struct dma_buf *buf, void *vaddr)
{
	struct intel_dma_buf *priv = buf->priv;

	mutex_lock(&priv->lock);
	if (--priv->vmapping_ref)
		goto out_success;

	vunmap(priv->vmapping);
	priv->vmapping_ref = 0;
	priv->vmapping = NULL;

out_success:
	mutex_unlock(&priv->lock);
}

static struct dma_buf_ops intel_dma_buf_ops = {
	.attach = intel_attach,
	.detach = intel_detach,
	.map_dma_buf = intel_map_dma_buf,
	.unmap_dma_buf = intel_unmap_dma_buf,
	.release = intel_release,
	.kmap_atomic = intel_kmap_atomic,
	.kmap = intel_kmap,
	.mmap = intel_mmap,
	.vmap = intel_vmap,
	.vunmap = intel_vunmap,
};

static int export_dma_buf(struct intel_dma_buf *priv, struct dma_buf **buf)
{
	struct dma_buf *dma_buf;

	/*export dma buf*/
	dma_buf = dma_buf_export((void *)priv, &intel_dma_buf_ops,
		intel_dma_buf_get_size(priv), INTEL_DMA_FD_FLAGS);
	if (IS_ERR(dma_buf))
		return PTR_ERR(dma_buf);

	*buf = dma_buf;

	return 0;
}

/**
 * intel_adf_mm_alloc_buf - allocate a buffer and export it as a dma buffer.
 *
 * @mm: memory manager
 * @size: buffer size in bytes
 * @buf: dma_buf which is exported
 *
 * Returns
 */
int intel_adf_mm_alloc_buf(struct intel_adf_mm *mm, u32 size,
	struct dma_buf **buf)
{
	struct intel_dma_buf *priv;
	struct device *dev;
	struct page **pages;
	u32 page_num;
	void *vaddr;
	int i;
	int err = 0;

	if (!mm || !buf)
		return -EINVAL;

	dev = mm->parent;

	vaddr =  __vmalloc(size, (GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO),
		__pgprot((pgprot_val(PAGE_KERNEL) & ~_PAGE_CACHE_MASK) |
		_PAGE_CACHE_WC));
	if (!vaddr) {
		dev_err(dev, "%s: failed to allocate buffer\n", __func__);
		return -ENOMEM;
	}

	/*get the page list*/
	page_num = PAGE_ALIGN(size) >> PAGE_SHIFT;
	pages = kzalloc(page_num * sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		dev_err(dev, "%s: failed to allocate page list\n", __func__);
		err = -ENOMEM;
		goto out_err0;
	}

	/*populate the page list*/
	for (i = 0; i < page_num; i++)
		pages[i] = vmalloc_to_page(vaddr + i * PAGE_SIZE);

	priv = intel_dma_buf_alloc(mm, pages, page_num, vaddr, size,
		INTEL_DMA_BUF_ALLOCATED);
	if (IS_ERR(priv)) {
		dev_err(dev, "%s: failed to allocate private\n", __func__);
		err = PTR_ERR(priv);
		goto out_err1;
	}

	err = export_dma_buf(priv, buf);
	if (err) {
		dev_err(dev, "%s: failed to export dma buffer\n", __func__);
		goto out_err2;
	}

	return 0;
out_err2:
	intel_dma_buf_put(priv);
out_err1:
	kfree(pages);
out_err0:
	vfree(vaddr);
	return err;
}

void intel_adf_mm_free_buf(struct dma_buf *buf)
{
	struct intel_dma_buf *priv;

	if (buf) {
		priv = buf->priv;
		intel_dma_buf_put(priv);
	}
}

int intel_adf_mm_export(struct intel_adf_mm *mm, struct page **pages,
	u32 page_num, void *vaddr, enum intel_dma_buf_type type,
	struct dma_buf **buf)
{
	struct device *dev = mm->parent;
	struct intel_dma_buf *priv;
	u32 size;
	int err;

	if (!mm || !pages || !buf || !page_num)
		return -EINVAL;

	size = page_num << PAGE_SHIFT;

	priv = intel_dma_buf_alloc(mm, pages, page_num, vaddr, size, type);
	if (IS_ERR(priv)) {
		dev_err(dev, "%s: failed to allocate private\n", __func__);
		return PTR_ERR(priv);
	}

	err = export_dma_buf(priv, buf);
	if (err) {
		dev_err(dev, "%s: failed to export dma buffer\n", __func__);
		goto err_out0;
	}

	return 0;
err_out0:
	intel_dma_buf_put(priv);
	return err;
}

int intel_adf_mm_fd(struct dma_buf *buf)
{
	if (!buf)
		return -EINVAL;

	return dma_buf_fd(buf, INTEL_DMA_FD_FLAGS);
}

int intel_adf_mm_gtt(struct dma_buf *buf, u32 *gtt)
{
	struct intel_dma_buf *priv;

	if (!buf || !gtt)
		return -EINVAL;

	priv = buf->priv;

	if (!priv)
		return -EINVAL;

	*gtt = intel_dma_buf_get_gtt(priv);

	return 0;
}

static inline struct adf_device *dev_to_adf_device(struct device *dev)
{
	return adf_obj_to_device(container_of(dev, struct adf_obj, dev));
}

static ssize_t display_memory_show(struct device *device,
	struct device_attribute *attr, char *buf)
{
	struct intel_adf_device *dev =
		to_intel_dev(dev_to_adf_device(device));
	struct intel_adf_mm *mm = &dev->mm;
	size_t n_total, n_alloc, n_free, n_bufs;

	intel_dc_memory_status(mm->mem, &n_total, &n_alloc, &n_free,
		&n_bufs);
	return snprintf(buf, PAGE_SIZE,
			"Total %lu MB, allocated %lu MB, free %lu MB, allocated %u buffers\n",
		pages_to_mb(n_total),
		pages_to_mb(n_alloc),
		pages_to_mb(n_free),
		(int) n_bufs);
}

static struct device_attribute mm_attrs[] = {
	__ATTR_RO(display_memory),
};

int intel_adf_mm_init(struct intel_adf_mm *mm, struct device *parent,
	struct intel_dc_memory *mem)
{
	int err;
	int i;

	if (!mm || !parent || !mem)
		return -EINVAL;

	memset(mm, 0, sizeof(struct intel_adf_mm));

	mm->parent = parent;
	mm->mem = mem;

	for (i = 0; i < ARRAY_SIZE(mm_attrs); i++) {
		err = device_create_file(parent, &mm_attrs[i]);
		if (err)
			goto out_err0;
	}

	return 0;
out_err0:
	return err;
}

void intel_adf_mm_destroy(struct intel_adf_mm *mm)
{
	if (mm) {
		mm->parent = NULL;
		mm->mem = NULL;
	}
}

