/*
 * videobuf2-dma-sg.c - dma scatter/gather memory allocator for videobuf2
 *
 * Copyright (C) 2010 Samsung Electronics
 *
 * Author: Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>

#include "mtkbuf-dma-cache-sg.h"

static int debug;
module_param(debug, int, 0644);

#define dprintk(level, fmt, arg...)					\
	do {								\
		if (debug >= level)					\
			pr_debug("mtk-dma-sg: " fmt, ## arg);	\
	} while (0)

struct vb2_dc_conf {
	struct device		*dev;
};

struct vb2_dma_sg_buf {
	void				*vaddr;
	struct page			**pages;
	int				write;
	int				offset;
	struct sg_table			sg_table;
	size_t				size;
	unsigned int			num_pages;
	atomic_t			refcount;
	struct vb2_vmarea_handler	handler;
	struct vm_area_struct		*vma;
	struct device			*dev;
	dma_addr_t			dma_addr;
};

static void vb2_dma_sg_put(void *buf_priv);

static int vb2_dma_sg_alloc_compacted(struct vb2_dma_sg_buf *buf,
		gfp_t gfp_flags)
{
	unsigned int last_page = 0;
	int size = buf->size;

	while (size > 0) {
		struct page *pages;
		int order;
		int i;

		order = get_order(size);
		/* Dont over allocate*/
		if ((PAGE_SIZE << order) > size)
			order--;

		pages = NULL;
		while (!pages) {
			pages = alloc_pages(GFP_KERNEL | __GFP_ZERO |
					__GFP_NOWARN | gfp_flags, order);
			if (pages)
				break;

			if (order == 0) {
				while (last_page--)
					__free_page(buf->pages[last_page]);
				return -ENOMEM;
			}
			order--;
		}

		split_page(pages, order);
		for (i = 0; i < (1 << order); i++)
			buf->pages[last_page++] = &pages[i];

		size -= PAGE_SIZE << order;
	}

	return 0;
}

static void *vb2_dma_sg_alloc(void *alloc_ctx, unsigned long size, gfp_t gfp_flags)
{
	struct vb2_dma_sg_buf *buf;
	int ret;
	int num_pages;
	struct vb2_dc_conf *conf = alloc_ctx;
	struct device *dev = conf->dev;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->vaddr = NULL;
	buf->write = 0;
	buf->offset = 0;
	buf->size = size;
	/* size is already page aligned */
	buf->num_pages = size >> PAGE_SHIFT;

	buf->pages = kcalloc(buf->num_pages, sizeof(struct page *),
			     GFP_KERNEL);
	if (!buf->pages)
		goto fail_pages_array_alloc;

	ret = vb2_dma_sg_alloc_compacted(buf, gfp_flags);
	if (ret)
		goto fail_pages_alloc;

	ret = sg_alloc_table_from_pages(&buf->sg_table, buf->pages,
			buf->num_pages, 0, size, GFP_KERNEL);
	if (ret)
		goto fail_table_alloc;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vb2_dma_sg_put;
	buf->handler.arg = buf;

	atomic_inc(&buf->refcount);

	ret = dma_map_sg(dev, buf->sg_table.sgl, buf->sg_table.nents, DMA_FROM_DEVICE);
	if (ret < 0)
		goto fail_table_alloc;
	if (ret > 1) {
		dma_unmap_sg(buf->dev, buf->sg_table.sgl, buf->sg_table.nents, DMA_FROM_DEVICE);
		pr_err("contiguous chunk is too small (%zu)\n", buf->size);
		goto fail_table_alloc;
	}
	buf->dma_addr = sg_dma_address(buf->sg_table.sgl);
	buf->dev = get_device(dev);

	dprintk(1, "%s: Allocated buffer of %d pages\n",
		__func__, buf->num_pages);
	return buf;

fail_table_alloc:
	num_pages = buf->num_pages;
	while (num_pages--)
		__free_page(buf->pages[num_pages]);
fail_pages_alloc:
	kfree(buf->pages);
fail_pages_array_alloc:
	kfree(buf);
	return NULL;
}

static void vb2_dma_sg_put(void *buf_priv)
{
	struct vb2_dma_sg_buf *buf = buf_priv;
	int i = buf->num_pages;

	if (atomic_dec_and_test(&buf->refcount)) {
		dprintk(1, "%s: Freeing buffer of %d pages\n", __func__,
			buf->num_pages);
		dma_unmap_sg(buf->dev, buf->sg_table.sgl, buf->sg_table.nents, DMA_FROM_DEVICE);
		put_device(buf->dev);
		if (buf->vaddr)
			vm_unmap_ram(buf->vaddr, buf->num_pages);
		sg_free_table(&buf->sg_table);
		while (--i >= 0)
			__free_page(buf->pages[i]);
		kfree(buf->pages);
		kfree(buf);
	}
}

static inline int vma_is_io(struct vm_area_struct *vma)
{
	return !!(vma->vm_flags & (VM_IO | VM_PFNMAP));
}

static void *vb2_dma_sg_get_userptr(void *alloc_ctx, unsigned long vaddr,
				    unsigned long size, int write)
{
	struct vb2_dma_sg_buf *buf;
	unsigned long first, last;
	int num_pages_from_user;
	struct vm_area_struct *vma;
	struct vb2_dc_conf *conf = alloc_ctx;
	struct device *dev = conf->dev;
	int ret;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return NULL;

	buf->vaddr = NULL;
	buf->write = write;
	buf->offset = vaddr & ~PAGE_MASK;
	buf->size = size;

	first = (vaddr           & PAGE_MASK) >> PAGE_SHIFT;
	last  = ((vaddr + size - 1) & PAGE_MASK) >> PAGE_SHIFT;
	buf->num_pages = last - first + 1;

	buf->pages = kcalloc(buf->num_pages, sizeof(struct page *),
			     GFP_KERNEL);
	if (!buf->pages)
		goto userptr_fail_alloc_pages;

	vma = find_vma(current->mm, vaddr);
	if (!vma) {
		dprintk(1, "no vma for address %lu\n", vaddr);
		goto userptr_fail_find_vma;
	}

	if (vma->vm_end < vaddr + size) {
		dprintk(1, "vma at %lu is too small for %lu bytes\n",
			vaddr, size);
		goto userptr_fail_find_vma;
	}

	buf->vma = vb2_get_vma(vma);
	if (!buf->vma) {
		dprintk(1, "failed to copy vma\n");
		goto userptr_fail_find_vma;
	}

	if (vma_is_io(buf->vma)) {
		for (num_pages_from_user = 0;
		     num_pages_from_user < buf->num_pages;
		     ++num_pages_from_user, vaddr += PAGE_SIZE) {
			unsigned long pfn;

			if (follow_pfn(vma, vaddr, &pfn)) {
				dprintk(1, "no page for address %lu\n", vaddr);
				break;
			}
			buf->pages[num_pages_from_user] = pfn_to_page(pfn);
		}
	} else
		num_pages_from_user = get_user_pages(current, current->mm,
					     vaddr & PAGE_MASK,
					     buf->num_pages,
					     write,
					     1, /* force */
					     buf->pages,
					     NULL);

	if (num_pages_from_user != buf->num_pages)
		goto userptr_fail_get_user_pages;

	if (sg_alloc_table_from_pages(&buf->sg_table, buf->pages,
			buf->num_pages, buf->offset, size, 0))
		goto userptr_fail_alloc_table_from_pages;

	ret = dma_map_sg(buf->dev, buf->sg_table.sgl, buf->sg_table.nents, DMA_FROM_DEVICE);
	if (ret < 0)
		goto userptr_fail_map_sg;
	if (ret > 1) {
		dma_unmap_sg(buf->dev, buf->sg_table.sgl, buf->sg_table.nents, DMA_FROM_DEVICE);
		pr_err("contiguous chunk is too small (%zu)\n", buf->size);
		goto userptr_fail_map_sg;
	}
	buf->dma_addr = sg_dma_address(buf->sg_table.sgl);
	buf->dev = get_device(dev);

	return buf;

userptr_fail_map_sg:
userptr_fail_alloc_table_from_pages:
userptr_fail_get_user_pages:
	dprintk(1, "get_user_pages requested/got: %d/%d]\n",
		buf->num_pages, num_pages_from_user);
	if (!vma_is_io(buf->vma))
		while (--num_pages_from_user >= 0)
			put_page(buf->pages[num_pages_from_user]);
	vb2_put_vma(buf->vma);
userptr_fail_find_vma:
	kfree(buf->pages);
userptr_fail_alloc_pages:
	kfree(buf);
	return NULL;
}

/*
 * @put_userptr: inform the allocator that a USERPTR buffer will no longer
 *		 be used
 */
static void vb2_dma_sg_put_userptr(void *buf_priv)
{
	struct vb2_dma_sg_buf *buf = buf_priv;
	int i = buf->num_pages;

	dprintk(1, "%s: Releasing userspace buffer of %d pages\n",
	       __func__, buf->num_pages);
	if (buf->vaddr)
		vm_unmap_ram(buf->vaddr, buf->num_pages);
	dma_unmap_sg(buf->dev, buf->sg_table.sgl, buf->sg_table.nents, DMA_FROM_DEVICE);
	put_device(buf->dev);
	sg_free_table(&buf->sg_table);
	while (--i >= 0) {
		if (buf->write)
			set_page_dirty_lock(buf->pages[i]);
		if (!vma_is_io(buf->vma))
			put_page(buf->pages[i]);
	}
	kfree(buf->pages);
	vb2_put_vma(buf->vma);
	kfree(buf);
}

static void *vb2_dma_sg_vaddr(void *buf_priv)
{
	struct vb2_dma_sg_buf *buf = buf_priv;

	BUG_ON(!buf);

	if (!buf->vaddr)
		buf->vaddr = vm_map_ram(buf->pages,
					buf->num_pages,
					-1,
					PAGE_KERNEL);

	/* add offset in case userptr is not page-aligned */
	return buf->vaddr + buf->offset;
}

static unsigned int vb2_dma_sg_num_users(void *buf_priv)
{
	struct vb2_dma_sg_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static int vb2_dma_sg_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_dma_sg_buf *buf = buf_priv;
	unsigned long uaddr = vma->vm_start;
	unsigned long usize = vma->vm_end - vma->vm_start;
	int i = 0;

	if (!buf) {
		pr_err("No memory to map\n");
		return -EINVAL;
	}

	do {
		int ret;

		ret = vm_insert_page(vma, uaddr, buf->pages[i++]);
		if (ret) {
			pr_err("Remapping memory, error: %d\n", ret);
			return ret;
		}

		uaddr += PAGE_SIZE;
		usize -= PAGE_SIZE;
	} while (usize > 0);


	/*
	 * Use common vm_area operations to track buffer refcount.
	 */
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	return 0;
}

static void *vb2_dma_sg_cookie(void *buf_priv)
{
	struct vb2_dma_sg_buf *buf = buf_priv;

	return &buf->dma_addr;
}

static void vb2_dma_sg_finish(void *buf_priv)
{
	struct vb2_dma_sg_buf *buf = buf_priv;

	dma_sync_sg_for_cpu(buf->dev, buf->sg_table.sgl,
					buf->sg_table.nents,
					DMA_FROM_DEVICE);
}

const struct vb2_mem_ops mtk_dma_sg_memops = {
	.alloc		= vb2_dma_sg_alloc,
	.put		= vb2_dma_sg_put,
	.get_userptr	= vb2_dma_sg_get_userptr,
	.put_userptr	= vb2_dma_sg_put_userptr,
	.vaddr		= vb2_dma_sg_vaddr,
	.mmap		= vb2_dma_sg_mmap,
	.num_users	= vb2_dma_sg_num_users,
	.cookie		= vb2_dma_sg_cookie,
	.finish		= vb2_dma_sg_finish,
};
EXPORT_SYMBOL_GPL(mtk_dma_sg_memops);

void *mtk_dma_sg_init_ctx(struct device *dev)
{
	struct vb2_dc_conf *conf;
	int err;

	/*
	 * if device has no max_seg_size set, we assume that there is no limit
	 * and force it to DMA_BIT_MASK(32) to always use contiguous mappings
	 * in DMA address space
	 */
	if (!dev->dma_parms) {
		dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return ERR_PTR(-ENOMEM);
		err = dma_set_max_seg_size(dev, DMA_BIT_MASK(32));
		if (err)
			return ERR_PTR(err);
	}

	conf = kzalloc(sizeof(*conf), GFP_KERNEL);
	if (!conf)
		return ERR_PTR(-ENOMEM);

	conf->dev = dev;

	return conf;
}
EXPORT_SYMBOL_GPL(mtk_dma_sg_init_ctx);

void mtk_dma_sg_cleanup_ctx(void *alloc_ctx)
{
	kfree(alloc_ctx);
}
EXPORT_SYMBOL_GPL(mtk_dma_sg_cleanup_ctx);

MODULE_DESCRIPTION("dma scatter/gather memory handling routines for videobuf2");
MODULE_AUTHOR("Andrzej Pietrasiewicz");
MODULE_LICENSE("GPL");
