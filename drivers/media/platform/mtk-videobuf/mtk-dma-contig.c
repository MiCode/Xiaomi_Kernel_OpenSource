/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Rick Chang <rick.chang@mediatek.com>
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

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-memops.h>

#include "mtk-dma-contig.h"

static int mtk_secure_mode;
static int debug;
module_param(debug, int, 0644);

#define CRITICAL     0
#define ALWAYS       0
#define VPUDBG       1
#define SPEW         2
#define INFO         3

#define dprintk(level, fmt, args...)					\
	do {								\
		if (debug >= level)					\
			pr_info("mtk-dma-contig: " fmt, ##args);	\
	} while (0)

struct vb2_dc_buf {
	struct device			*dev;
	void				*vaddr;
	unsigned long			size;
	void				*cookie;
	dma_addr_t			dma_addr;
	unsigned long			attrs;
	enum dma_data_direction		dma_dir;
	struct sg_table			*dma_sgt;
	struct frame_vector		*vec;

	/* MMAP related */
	struct vb2_vmarea_handler	handler;
	atomic_t			refcount;
	struct sg_table			*sgt_base;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
};


/*********************************************/
/*        scatterlist table functions        */
/*********************************************/

static unsigned long vb2_dc_get_contiguous_size(struct sg_table *sgt)
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

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static void *vb2_dc_cookie(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return &buf->dma_addr;
}

static void *vb2_dc_vaddr(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	if (!buf->vaddr && buf->db_attach)
		buf->vaddr = dma_buf_vmap(buf->db_attach->dmabuf);

	return buf->vaddr;
}

static unsigned int vb2_dc_num_users(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;

	return atomic_read(&buf->refcount);
}

static void vb2_dc_prepare(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* DMABUF exporter will flush the cache for us */
	if (!sgt || buf->db_attach)
		return;

	dma_sync_sg_for_device(buf->dev, sgt->sgl, sgt->orig_nents,
			       buf->dma_dir);
}

static void vb2_dc_finish(void *buf_priv)
{
	struct vb2_dc_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	/* DMABUF exporter will flush the cache for us */
	if (!sgt || buf->db_attach)
		return;

	dma_sync_sg_for_cpu(buf->dev, sgt->sgl, sgt->orig_nents, buf->dma_dir);
}

/*********************************************/
/*       callbacks for DMABUF buffers        */
/*********************************************/

static int vb2_dc_map_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;
	struct sg_table *sgt;
	unsigned long contig_size;

	if (WARN_ON(!buf->db_attach)) {
		dprintk(CRITICAL, "trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		dprintk(CRITICAL, "dmabuf buffer is already pinned\n");
		return 0;
	}

	if (mtk_secure_mode == 1) {
		dprintk(INFO, "bypass %s for secure buffer\n", __func__);
		buf->dma_addr = 0;
		buf->dma_sgt  = NULL;
		buf->vaddr    = NULL;
	} else {
		/* get the associated scatterlist for this buffer */
		sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
		if (IS_ERR(sgt)) {
			dprintk(CRITICAL, "Error getting dmabuf scatterlist\n");
			return -EINVAL;
		}

		/* checking if dmabuf is big enough to store contiguous chunk */
		contig_size = vb2_dc_get_contiguous_size(sgt);
		if (contig_size < buf->size) {
#ifdef CONFIG_MTK_IOMMU_V2
			dprintk(CRITICAL,
				"contiguous chunk is too small %lu/%lu b\n",
				contig_size, buf->size);
#endif
			dma_buf_unmap_attachment(buf->db_attach,
				sgt, buf->dma_dir);
#ifdef CONFIG_MTK_IOMMU_V2
			return -EFAULT;
#endif
		}

		buf->dma_addr = sg_dma_address(sgt->sgl);
		buf->dma_sgt = sgt;
		buf->vaddr = NULL;
	}

	return 0;
}

static void vb2_dc_unmap_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (WARN_ON(!buf->db_attach)) {
		dprintk(CRITICAL, "trying to unpin a not attached buffer\n");
		return;
	}

	if (mtk_secure_mode == 1) {
		dprintk(INFO, "bypass %s for secure buffer\n", __func__);
		buf->dma_addr = 0;
		buf->dma_sgt  = NULL;
		buf->vaddr    = NULL;
	} else {
		if (WARN_ON(!sgt)) {
			dprintk(CRITICAL,
				"dmabuf buffer is already unpinned\n");
			return;
		}

		if (buf->vaddr) {
			dma_buf_vunmap(buf->db_attach->dmabuf, buf->vaddr);
			buf->vaddr = NULL;
		}
		dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);

		buf->dma_addr = 0;
		buf->dma_sgt = NULL;
	}

}

static void vb2_dc_detach_dmabuf(void *mem_priv)
{
	struct vb2_dc_buf *buf = mem_priv;

	/* if vb2 works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_addr))
		vb2_dc_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

static void *vb2_dc_attach_dmabuf(struct device *dev, struct dma_buf *dbuf,
	unsigned long size, enum dma_data_direction dma_dir)
{
	struct vb2_dc_buf *buf;
	struct dma_buf_attachment *dba;

	if (dbuf->size < size) {
		dprintk(CRITICAL, "size too small. dbuf->size:%lu, size:%lu\n",
			dbuf->size, size);
		return ERR_PTR(-EFAULT);
	}

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = dev;
	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		dprintk(CRITICAL, "failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}

	buf->dma_dir = dma_dir;
	buf->size = size;
	buf->db_attach = dba;

	return buf;
}

/*********************************************/
/*       DMA CONTIG exported functions       */
/*********************************************/

const struct vb2_mem_ops mtk_dma_contig_memops = {
	.cookie		= vb2_dc_cookie,
	.vaddr		= vb2_dc_vaddr,
	.prepare	= vb2_dc_prepare,
	.finish		= vb2_dc_finish,
	.map_dmabuf	= vb2_dc_map_dmabuf,
	.unmap_dmabuf	= vb2_dc_unmap_dmabuf,
	.attach_dmabuf	= vb2_dc_attach_dmabuf,
	.detach_dmabuf	= vb2_dc_detach_dmabuf,
	.num_users	= vb2_dc_num_users,
};
EXPORT_SYMBOL_GPL(mtk_dma_contig_memops);

/**
 * mtk_dma_contig_set_max_seg_size() - configure DMA max segment size
 * @dev:	device for configuring DMA parameters
 * @size:	size of DMA max segment size to set
 *
 * To allow mapping the scatter-list into a single chunk in the DMA
 * address space, the device is required to have the DMA max segment
 * size parameter set to a value larger than the buffer size. Otherwise,
 * the DMA-mapping subsystem will split the mapping into max segment
 * size chunks. This function sets the DMA max segment size
 * parameter to let DMA-mapping map a buffer as a single chunk in DMA
 * address space.
 * This code assumes that the DMA-mapping subsystem will merge all
 * scatterlist segments if this is really possible (for example when
 * an IOMMU is available and enabled).
 * Ideally, this parameter should be set by the generic bus code, but it
 * is left with the default 64KiB value due to historical litmiations in
 * other subsystems (like limited USB host drivers) and there no good
 * place to set it to the proper value.
 * This function should be called from the drivers, which are known to
 * operate on platforms with IOMMU and provide access to shared buffers
 * (either USERPTR or DMABUF). This should be done before initializing
 * videobuf2 queue.
 */
int mtk_dma_contig_set_max_seg_size(struct device *dev, unsigned int size)
{
	if (!dev->dma_parms) {
		dev->dma_parms = kzalloc(sizeof(*dev->dma_parms), GFP_KERNEL);
		if (!dev->dma_parms)
			return -ENOMEM;
	}
	if (dma_get_max_seg_size(dev) < size)
		return dma_set_max_seg_size(dev, size);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_dma_contig_set_max_seg_size);

/*
 * mtk_dma_contig_clear_max_seg_size() - release resources for DMA parameters
 * @dev:	device for configuring DMA parameters
 *
 * This function releases resources allocated to configure DMA parameters
 * (see vb2_dma_contig_set_max_seg_size() function). It should be called from
 * device drivers on driver remove.
 */
void mtk_dma_contig_clear_max_seg_size(struct device *dev)
{
	kfree(dev->dma_parms);
	dev->dma_parms = NULL;
}
EXPORT_SYMBOL_GPL(mtk_dma_contig_clear_max_seg_size);

/*
 * mtk_dma_contig_set_secure_mode() - set secure mode to bypass buffer processes
 * @dev:	device for configuring DMA parameters
 *
 * This function is used for set hint for normal and secure buffer processes.
 */
void mtk_dma_contig_set_secure_mode(struct device *dev, int secure_mode)
{
	mtk_secure_mode = secure_mode;
	dprintk(CRITICAL, "Set secure mode : %d\n", secure_mode);
}
EXPORT_SYMBOL_GPL(mtk_dma_contig_set_secure_mode);

MODULE_DESCRIPTION("DMA-contig memory handling routines for videobuf2");
MODULE_LICENSE("GPL");
