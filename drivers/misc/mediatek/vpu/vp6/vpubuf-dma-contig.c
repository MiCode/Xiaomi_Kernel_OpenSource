/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Fish Wu <fish.wu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include "vpu_cmn.h"
#include "vpubuf-core.h"
#include "vpubuf-dma-contig.h"
#include "vpubuf-memops.h"

#define VPU_DYNAMIC_MEMORY_BASE 0x60000000
#define VPU_DYNAMIC_MEMORY_SIZE 0x10000000

struct vpu_dc_conf {
	struct device *dev;
};

struct vpu_dc_buf {
	struct device *dev;
	void *vaddr;
	unsigned long size;
	dma_addr_t dma_addr;
	enum dma_data_direction dma_dir;
	struct sg_table *dma_sgt;
	struct frame_vector *vec;
	struct sg_table *remap_sgt;

	/* MMAP related */
	struct vpu_vmarea_handler handler;
	atomic_t refcount;
	struct sg_table *sgt_base;

	/* cache related */
	unsigned int is_cached;

	/* DMABUF related */
	struct dma_buf_attachment *db_attach;
};

/*********************************************/
/*        scatterlist table functions        */
/*********************************************/
#if 0
static unsigned long vpu_dc_get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected = sg_dma_address(s) + sg_dma_len(s);
		size += sg_dma_len(s);
	}

	return size;
}
#endif

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static struct sg_table *vpu_dc_get_base_sgt(struct vpu_dc_buf *buf)
{
	int ret;
	struct sg_table *sgt;

	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return NULL;

	ret = dma_get_sgtable(buf->dev, sgt, buf->vaddr, buf->dma_addr,
			      buf->size);
	if (ret < 0) {
		LOG_ERR("failed to get scatterlist from DMA API\n");
		kfree(sgt);
		return NULL;
	}

	return sgt;
}

static void *vpu_dc_cookie(void *buf_priv)
{
	struct vpu_dc_buf *buf = buf_priv;

	return &buf->dma_addr;
}

static void *vpu_dc_vaddr(void *buf_priv)
{
	struct vpu_dc_buf *buf = buf_priv;

	if (!buf->vaddr && buf->db_attach)
		buf->vaddr = dma_buf_vmap(buf->db_attach->dmabuf);

	return buf->vaddr;
}

static unsigned int vpu_dc_num_users(void *buf_priv)
{
	struct vpu_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static void vpu_dc_prepare(void *buf_priv)
{
	struct vpu_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* DMABUF exporter will flush the cache for us */
	if (!sgt || buf->db_attach)
		return;

	if (buf->is_cached)
		dma_sync_sg_for_device(buf->dev, sgt->sgl, sgt->orig_nents,
				       buf->dma_dir);
}

static void vpu_dc_finish(void *buf_priv)
{
	struct vpu_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* DMABUF exporter will flush the cache for us */
	if (!sgt || buf->db_attach)
		return;

	if (buf->is_cached)
		dma_sync_sg_for_cpu(buf->dev, sgt->sgl, sgt->orig_nents,
				    buf->dma_dir);
}

/*********************************************/
/*        callbacks for MMAP buffers         */
/*********************************************/

static void vpu_dc_put(void *buf_priv)
{
	struct vpu_dc_buf *buf = buf_priv;

	if (!atomic_dec_and_test(&buf->refcount))
		return;

	if (buf->dma_sgt) {
		sg_free_table(buf->dma_sgt);
		kfree(buf->dma_sgt);
	}

	if (buf->sgt_base) {
		sg_free_table(buf->sgt_base);
		kfree(buf->sgt_base);
	}
	dma_free_coherent(buf->dev, buf->size, buf->vaddr, buf->dma_addr);
	put_device(buf->dev);
	kfree(buf);
}

static void *vpu_dc_alloc(void *alloc_ctx, unsigned long size,
			  enum dma_data_direction dma_dir, gfp_t gfp_flags,
			  unsigned int is_cached)
{
	struct vpu_dc_conf *conf = alloc_ctx;
	struct device *dev = conf->dev;
	struct vpu_dc_buf *buf;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->vaddr = dma_alloc_coherent(dev, size, &buf->dma_addr,
						GFP_KERNEL | gfp_flags);
	if (!buf->vaddr) {
		LOG_ERR("dma_alloc_coherent of size %ld failed\n", size);
		kfree(buf);
		return ERR_PTR(-ENOMEM);
	}

	/* Prevent the device from being released while the buffer is used */
	buf->dev = get_device(dev);
	buf->size = size;
	buf->dma_dir = dma_dir;
	buf->is_cached = is_cached;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put = vpu_dc_put;
	buf->handler.arg = buf;

	if (!buf->dma_sgt)
		buf->dma_sgt = vpu_dc_get_base_sgt(buf);

	atomic_inc(&buf->refcount);

	return buf;
}

static int vpu_dc_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vpu_dc_buf *buf = buf_priv;
	int ret;

	if (!buf)
		return -EINVAL;

	/*
	 * dma_mmap_* uses vm_pgoff as in-buffer offset, but we want to
	 * map whole buffer
	 */
	vma->vm_pgoff = 0;

	if (buf->is_cached) {
#if 0	/* to do */
		ret = dma_mmap_attrs_cached(buf->dev, vma, buf->vaddr,
						  buf->dma_addr, buf->size,
						  NULL);
		if (ret) {
			LOG_ERR("Remapping memory failed, error: %d\n", ret);
			return ret;
		}
#endif
	} else {
		ret = dma_mmap_coherent(buf->dev, vma, buf->vaddr,
					buf->dma_addr, buf->size);
		if (ret) {
			LOG_ERR("Remapping memory failed, error: %d\n", ret);
			return ret;
		}
	}

	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;
	vma->vm_private_data = &buf->handler;
	vma->vm_ops = &vpu_common_vm_ops;

	vma->vm_ops->open(vma);

	pr_debug("%s: mapped dma addr 0x%08lx at 0x%08lx, size %ld\n",
		 __func__, (unsigned long)buf->dma_addr, vma->vm_start,
		 buf->size);

	return 0;
}

/*********************************************/
/*         DMABUF ops for exporters          */
/*********************************************/

struct vpu_dc_attachment {
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};

static int vpu_dc_dmabuf_ops_attach(struct dma_buf *dbuf, struct device *dev,
				    struct dma_buf_attachment *dbuf_attach)
{
	struct vpu_dc_attachment *attach;
	unsigned int i;
	struct scatterlist *rd, *wr;
	struct sg_table *sgt;
	struct vpu_dc_buf *buf = dbuf->priv;
	int ret;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	sgt = &attach->sgt;
	/* Copy the buf->base_sgt scatter list to the attachment, as we can't
	 * map the same scatter list to multiple attachments at the same time.
	 */
	ret = sg_alloc_table(sgt, buf->sgt_base->orig_nents, GFP_KERNEL);
	if (ret) {
		kfree(attach);
		return -ENOMEM;
	}

	rd = buf->sgt_base->sgl;
	wr = sgt->sgl;
	for (i = 0; i < sgt->orig_nents; ++i) {
		sg_set_page(wr, sg_page(rd), rd->length, rd->offset);
		rd = sg_next(rd);
		wr = sg_next(wr);
	}

	attach->dma_dir = DMA_NONE;
	dbuf_attach->priv = attach;

	return 0;
}

static void vpu_dc_dmabuf_ops_detach(struct dma_buf *dbuf,
				     struct dma_buf_attachment *db_attach)
{
	struct vpu_dc_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	if (!attach)
		return;

	sgt = &attach->sgt;

	/* release the scatterlist cache */
	if (attach->dma_dir != DMA_NONE)
		dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
			     attach->dma_dir);

	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}

static struct sg_table *vpu_dc_dmabuf_ops_map(struct dma_buf_attachment
					      *db_attach,
					      enum dma_data_direction dma_dir)
{
	struct vpu_dc_attachment *attach = db_attach->priv;
	/* stealing dmabuf mutex to serialize map/unmap operations */
	struct mutex *lock = &db_attach->dmabuf->lock;
	struct sg_table *sgt;

	mutex_lock(lock);

	sgt = &attach->sgt;
	/* return previously mapped sg table */
	if (attach->dma_dir == dma_dir) {
		mutex_unlock(lock);
		return sgt;
	}

	/* release any previous cache */
	if (attach->dma_dir != DMA_NONE) {
		dma_unmap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
			     attach->dma_dir);
		attach->dma_dir = DMA_NONE;
	}

	/* mapping to the client with new direction */
	sgt->nents = dma_map_sg(db_attach->dev, sgt->sgl, sgt->orig_nents,
				dma_dir);
	if (!sgt->nents) {
		LOG_ERR("failed to map scatterlist\n");
		mutex_unlock(lock);
		return ERR_PTR(-EIO);
	}

	attach->dma_dir = dma_dir;

	mutex_unlock(lock);

	return sgt;
}

static void vpu_dc_dmabuf_ops_unmap(struct dma_buf_attachment *db_attach,
				    struct sg_table *sgt,
				    enum dma_data_direction dma_dir)
{
	/* nothing to be done here */
}

static void vpu_dc_dmabuf_ops_release(struct dma_buf *dbuf)
{
	/* drop reference obtained in vpu_dc_get_dmabuf */
	vpu_dc_put(dbuf->priv);
}

static void *vpu_dc_dmabuf_ops_kmap(struct dma_buf *dbuf, unsigned long pgnum)
{
	struct vpu_dc_buf *buf = dbuf->priv;

	return buf->vaddr + pgnum * PAGE_SIZE;
}

static void *vpu_dc_dmabuf_ops_vmap(struct dma_buf *dbuf)
{
	struct vpu_dc_buf *buf = dbuf->priv;

	return buf->vaddr;
}

static int vpu_dc_dmabuf_ops_mmap(struct dma_buf *dbuf,
				  struct vm_area_struct *vma)
{
	return vpu_dc_mmap(dbuf->priv, vma);
}

static struct dma_buf_ops vpu_dc_dmabuf_ops = {
	.attach = vpu_dc_dmabuf_ops_attach,
	.detach = vpu_dc_dmabuf_ops_detach,
	.map_dma_buf = vpu_dc_dmabuf_ops_map,
	.unmap_dma_buf = vpu_dc_dmabuf_ops_unmap,
	.map = vpu_dc_dmabuf_ops_kmap,
	.map_atomic = vpu_dc_dmabuf_ops_kmap,
	.vmap = vpu_dc_dmabuf_ops_vmap,
	.mmap = vpu_dc_dmabuf_ops_mmap,
	.release = vpu_dc_dmabuf_ops_release,
};



static struct dma_buf *vpu_dc_get_dmabuf(void *buf_priv, unsigned long flags)
{
	struct vpu_dc_buf *buf = buf_priv;
	struct dma_buf *dbuf;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	/* coherent memory is suitable for export */
	buf->is_cached = 0;

	exp_info.ops = &vpu_dc_dmabuf_ops;
	exp_info.size = buf->size;
	exp_info.flags = flags;
	exp_info.priv = buf;

	if (!buf->sgt_base)
		buf->sgt_base = vpu_dc_get_base_sgt(buf);

	if (WARN_ON(!buf->sgt_base))
		return NULL;

	dbuf = dma_buf_export(&exp_info);
	if (IS_ERR(dbuf))
		return NULL;

	/* dmabuf keeps reference to vpu buffer */
	atomic_inc(&buf->refcount);

	return dbuf;
}

/*********************************************/
/*       callbacks for USERPTR buffers       */
/*********************************************/
#if 0	/* to do */
static void vpu_dc_put_userptr(void *buf_priv)
{
	struct vpu_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;
	int i;
	struct page **pages;

	if (sgt) {
		DEFINE_DMA_ATTRS(attrs);

		dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
		/*
		 * No need to sync to CPU, it's already synced to the CPU
		 * since the finish() memop will have been called before this.
		 */
		dma_unmap_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
				   buf->dma_dir, &attrs);
		pages = frame_vector_pages(buf->vec);
		/* sgt should exist only if vector contains pages... */
		WARN_ON(IS_ERR(pages));
		for (i = 0; i < frame_vector_count(buf->vec); i++)
			set_page_dirty_lock(pages[i]);
		sg_free_table(sgt);
		kfree(sgt);
	}
	vpu_destroy_framevec(buf->vec);
	kfree(buf);
}
#endif
/*
 * For some kind of reserved memory there might be no struct page available,
 * so all that can be done to support such 'pages' is to try to convert
 * pfn to dma address or at the last resort just assume that
 * dma address == physical address (like it has been assumed in earlier version
 * of videobuf2-dma-contig
 */

#ifdef __arch_pfn_to_dma
static inline dma_addr_t vpu_dc_pfn_to_dma(struct device *dev,
					   unsigned long pfn)
{
	return (dma_addr_t)__arch_pfn_to_dma(dev, pfn);
}
#elif defined(__pfn_to_bus)
static inline dma_addr_t vpu_dc_pfn_to_dma(struct device *dev,
					   unsigned long pfn)
{
	return (dma_addr_t)__pfn_to_bus(pfn);
}
#elif defined(__pfn_to_phys)
static inline dma_addr_t vpu_dc_pfn_to_dma(struct device *dev,
					   unsigned long pfn)
{
	return (dma_addr_t)__pfn_to_phys(pfn);
}
#else
static inline dma_addr_t vpu_dc_pfn_to_dma(struct device *dev,
					   unsigned long pfn)
{
	/* really, we cannot do anything better at this point */
	return (dma_addr_t)(pfn) << PAGE_SHIFT;
}
#endif
#if 0	/* to do */
static void *vpu_dc_get_userptr(void *alloc_ctx, unsigned long vaddr,
				unsigned long size,
				enum dma_data_direction dma_dir)
{
	struct vpu_dc_conf *conf = alloc_ctx;
	struct vpu_dc_buf *buf;
	struct frame_vector *vec;
	unsigned long offset;
	int n_pages, i;
	int ret = 0;
	struct sg_table *sgt;
	unsigned long contig_size;
	unsigned long dma_align = dma_get_cache_alignment();
	DEFINE_DMA_ATTRS(attrs);

	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);

	/* Only cache aligned DMA transfers are reliable */
	if (!IS_ALIGNED(vaddr | size, dma_align)) {
		pr_debug("user data must be aligned to %lu bytes\n", dma_align);
		return ERR_PTR(-EINVAL);
	}

	if (!size) {
		pr_debug("size is zero\n");
		return ERR_PTR(-EINVAL);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = conf->dev;
	buf->dma_dir = dma_dir;

	offset = vaddr & ~PAGE_MASK;
	vec = vpu_create_framevec(vaddr, size, dma_dir == DMA_FROM_DEVICE);
	if (IS_ERR(vec)) {
		ret = PTR_ERR(vec);
		goto fail_buf;
	}
	buf->vec = vec;
	n_pages = frame_vector_count(vec);
	ret = frame_vector_to_pages(vec);
	if (ret < 0) {
		unsigned long *nums = frame_vector_pfns(vec);

		/*
		 * Failed to convert to pages... Check the memory is physically
		 * contiguous and use direct mapping
		 */
		for (i = 1; i < n_pages; i++)
			if (nums[i - 1] + 1 != nums[i])
				goto fail_pfnvec;
		buf->dma_addr = vpu_dc_pfn_to_dma(buf->dev, nums[0]);
		goto out;
	}

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		ret = -ENOMEM;
		goto fail_pfnvec;
	}

	ret = sg_alloc_table_from_pages(sgt, frame_vector_pages(vec), n_pages,
					offset, size, GFP_KERNEL);
	if (ret) {
		LOG_ERR("failed to initialize sg table\n");
		goto fail_sgt;
	}

	/*
	 * No need to sync to the device, this will happen later when the
	 * prepare() memop is called.
	 */
	sgt->nents = dma_map_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
				      buf->dma_dir, &attrs);
	if (sgt->nents <= 0) {
		LOG_ERR("failed to map scatterlist\n");
		ret = -EIO;
		goto fail_sgt_init;
	}

	contig_size = vpu_dc_get_contiguous_size(sgt);
	if (contig_size < size) {
		LOG_ERR("contiguous mapping is too small %lu/%lu\n",
		       contig_size, size);
		ret = -EFAULT;
		goto fail_map_sg;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;
out:
	buf->size = size;

	return buf;

fail_map_sg:
	dma_unmap_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
			   buf->dma_dir, &attrs);

fail_sgt_init:
	sg_free_table(sgt);

fail_sgt:
	kfree(sgt);

fail_pfnvec:
	vpu_destroy_framevec(vec);

fail_buf:
	kfree(buf);

	return ERR_PTR(ret);
}
#endif
/*********************************************/
/*       callbacks for DMABUF buffers        */
/*********************************************/
static struct sg_table *vpu_dup_sg_table(struct sg_table *table)
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
	for_each_sg(table->sgl, sg, table->orig_nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		sg_dma_address(new_sg) = 0;
		sg_dma_len(new_sg) = 0;
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void vpu_del_dup_sg_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

static int vpu_dc_map_dmabuf(void *mem_priv)
{
	struct vpu_dc_buf *buf = mem_priv;
	struct sg_table *sgt, *dup_sgt;

	if (WARN_ON(!buf->db_attach)) {
		LOG_ERR("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		LOG_ERR("dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
	if (IS_ERR(sgt)) {
		LOG_ERR("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	/* dup sgt */
	dup_sgt = vpu_dup_sg_table(sgt);

	/* use dup_sgt to do remap */
	dup_sgt->nents = dma_map_sg(buf->dev, dup_sgt->sgl,
						dup_sgt->orig_nents,
						DMA_BIDIRECTIONAL);
	if (!dup_sgt->nents) {
		LOG_ERR("failed to map scatterlist\n");
		vpu_del_dup_sg_table(dup_sgt);
		return -EIO;
	}

	buf->dma_addr = sg_dma_address(dup_sgt->sgl);
	buf->dma_sgt = sgt;
	buf->remap_sgt = dup_sgt;
	buf->vaddr = NULL;

	return 0;
}

static void vpu_dc_unmap_dmabuf(void *mem_priv)
{
	struct vpu_dc_buf *buf = mem_priv;
	struct sg_table *sgt = buf->dma_sgt;
	struct sg_table *dup_sgt = buf->remap_sgt;

	if (WARN_ON(!buf->db_attach)) {
		LOG_ERR("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		LOG_ERR("dmabuf buffer is already unpinned\n");
		return;
	}

	if (buf->vaddr) {
		dma_buf_vunmap(buf->db_attach->dmabuf, buf->vaddr);
		buf->vaddr = NULL;
	}

	/* unmap dup_sgt */
	dma_unmap_sg(buf->dev, dup_sgt->sgl, dup_sgt->orig_nents,
			DMA_BIDIRECTIONAL);

	/* del dup_sgt */
	vpu_del_dup_sg_table(dup_sgt);

	dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);

	buf->dma_addr = 0;
	buf->dma_sgt = NULL;
}

static void vpu_dc_detach_dmabuf(void *mem_priv)
{
	struct vpu_dc_buf *buf = mem_priv;

	/* if vpu works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_addr))
		vpu_dc_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

static void *vpu_dc_attach_dmabuf(void *alloc_ctx, struct dma_buf *dbuf,
				  unsigned long size,
				  enum dma_data_direction dma_dir)
{
	struct vpu_dc_conf *conf = alloc_ctx;
	struct vpu_dc_buf *buf;
	struct dma_buf_attachment *dba;

	if (dbuf->size < size)
		return ERR_PTR(-EFAULT);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = conf->dev;
	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		LOG_ERR("failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}

	buf->dma_dir = dma_dir;
	buf->size = size;
	buf->db_attach = dba;
	buf->is_cached = 0;

	return buf;
}

/*********************************************/
/*       DMA CONTIG exported functions       */
/*********************************************/

const struct vpu_mem_ops vpu_dma_contig_memops = {
	.alloc = vpu_dc_alloc,
	.put = vpu_dc_put,
	.get_dmabuf = vpu_dc_get_dmabuf,
	.cookie = vpu_dc_cookie,
	.vaddr = vpu_dc_vaddr,
	.mmap = vpu_dc_mmap,
	.get_userptr = NULL,	/* vpu_dc_get_userptr, */
	.put_userptr = NULL,	/* vpu_dc_put_userptr, */
	.prepare = vpu_dc_prepare,
	.finish = vpu_dc_finish,
	.map_dmabuf = vpu_dc_map_dmabuf,
	.unmap_dmabuf = vpu_dc_unmap_dmabuf,
	.attach_dmabuf = vpu_dc_attach_dmabuf,
	.detach_dmabuf = vpu_dc_detach_dmabuf,
	.num_users = vpu_dc_num_users,
};

void *vpu_dma_contig_init_ctx(struct device *dev)
{
	struct vpu_dc_conf *conf;

	conf = kzalloc(sizeof(*conf), GFP_KERNEL);
	if (!conf)
		return ERR_PTR(-ENOMEM);

	conf->dev = dev;

	return conf;
}

void vpu_dma_contig_cleanup_ctx(void *alloc_ctx)
{
	if (!IS_ERR_OR_NULL(alloc_ctx))
		kfree(alloc_ctx);
}
