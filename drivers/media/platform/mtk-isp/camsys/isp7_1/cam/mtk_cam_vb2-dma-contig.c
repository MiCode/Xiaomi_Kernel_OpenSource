// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/refcount.h>
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <media/videobuf2-v4l2.h>
#include <media/videobuf2-memops.h>

#include "mtk_cam_vb2-dma-contig.h"

#include "mtk_cam-trace.h"

struct mtk_cam_vb2_buf {
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
	refcount_t			refcount;
	struct sg_table			*sgt_base;

	/* DMABUF related */
	struct dma_buf_attachment	*db_attach;
};

/*********************************************/
/*        scatterlist table functions        */
/*********************************************/

static unsigned long mtk_cam_vb2_get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	dma_addr_t expected = sg_dma_address(sgt->sgl);
	unsigned int i;
	unsigned long size = 0;

	for_each_sgtable_dma_sg(sgt, s, i) {
		if (sg_dma_address(s) != expected)
			break;
		expected += sg_dma_len(s);
		size += sg_dma_len(s);
	}
	return size;
}

/*********************************************/
/*         callbacks for all buffers         */
/*********************************************/

static void *mtk_cam_vb2_cookie(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;

	return &buf->dma_addr;
}

static void *mtk_cam_vb2_vaddr(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;

	MTK_CAM_TRACE_FUNC_BEGIN(BUFFER);

	if (!buf->vaddr && buf->db_attach)
		buf->vaddr = dma_buf_vmap(buf->db_attach->dmabuf);

	MTK_CAM_TRACE_END(BUFFER);
	return buf->vaddr;
}

static unsigned int mtk_cam_vb2_num_users(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;

	return refcount_read(&buf->refcount);
}

static void mtk_cam_vb2_prepare(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (!sgt)
		return;

	dma_sync_sgtable_for_device(buf->dev, sgt, buf->dma_dir);
}

static void mtk_cam_vb2_finish(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (!sgt)
		return;

	dma_sync_sgtable_for_cpu(buf->dev, sgt, buf->dma_dir);
}

/*********************************************/
/*       callbacks for DMABUF buffers        */
/*********************************************/

static int mtk_cam_vb2_map_dmabuf(void *mem_priv)
{
	struct mtk_cam_vb2_buf *buf = mem_priv;
	struct sg_table *sgt;
	unsigned long contig_size;

	if (WARN_ON(!buf->db_attach)) {
		pr_info("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->dma_sgt)) {
		pr_info("dmabuf buffer is already pinned\n");
		return 0;
	}

	MTK_CAM_TRACE_FUNC_BEGIN(BUFFER);

	/* get the associated scatterlist for this buffer */
	sgt = dma_buf_map_attachment(buf->db_attach, buf->dma_dir);
	if (IS_ERR(sgt)) {
		pr_info("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	/* checking if dmabuf is big enough to store contiguous chunk */
	contig_size = mtk_cam_vb2_get_contiguous_size(sgt);
	if (contig_size < buf->size) {
		pr_info("contiguous chunk is too small %lu/%lu\n",
		       contig_size, buf->size);
		dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);
		return -EFAULT;
	}

	buf->dma_addr = sg_dma_address(sgt->sgl);
	buf->dma_sgt = sgt;
	buf->vaddr = NULL;

	MTK_CAM_TRACE_END(BUFFER);
	return 0;
}

static void mtk_cam_vb2_unmap_dmabuf(void *mem_priv)
{
	struct mtk_cam_vb2_buf *buf = mem_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (WARN_ON(!buf->db_attach)) {
		pr_info("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!sgt)) {
		pr_info("dmabuf buffer is already unpinned\n");
		return;
	}

	MTK_CAM_TRACE_FUNC_BEGIN(BUFFER);

	if (buf->vaddr) {
		dma_buf_vunmap(buf->db_attach->dmabuf, buf->vaddr);
		buf->vaddr = NULL;
	}
	dma_buf_unmap_attachment(buf->db_attach, sgt, buf->dma_dir);

	buf->dma_addr = 0;
	buf->dma_sgt = NULL;

	MTK_CAM_TRACE_END(BUFFER);
}

static void mtk_cam_vb2_detach_dmabuf(void *mem_priv)
{
	struct mtk_cam_vb2_buf *buf = mem_priv;

	/* if vb2 works correctly you should never detach mapped buffer */
	if (WARN_ON(buf->dma_addr))
		mtk_cam_vb2_unmap_dmabuf(buf);

	/* detach this attachment */
	dma_buf_detach(buf->db_attach->dmabuf, buf->db_attach);
	kfree(buf);
}

static void *mtk_cam_vb2_attach_dmabuf(struct device *dev, struct dma_buf *dbuf,
	unsigned long size, enum dma_data_direction dma_dir)
{
	struct mtk_cam_vb2_buf *buf;
	struct dma_buf_attachment *dba;

	if (dbuf->size < size)
		return ERR_PTR(-EFAULT);

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->dev = dev;
	/* create attachment for the dmabuf with the user device */
	dba = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(dba)) {
		pr_info("failed to attach dmabuf\n");
		kfree(buf);
		return dba;
	}

	/* always skip cache operations, we handle it manually */
	dba->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	buf->dma_dir = dma_dir;
	buf->size = size;
	buf->db_attach = dba;

	return buf;
}

/*********************************************/
/*       DMA CONTIG exported functions       */
/*********************************************/

const struct vb2_mem_ops mtk_cam_dma_contig_memops = {
	/* .alloc = */
	/* .put = */
	/* .get_dmabuf = */
	.cookie		= mtk_cam_vb2_cookie,
	.vaddr		= mtk_cam_vb2_vaddr,
	/* .mmap = */
	/* .get_userptr = */
	/* .put_userptr	= */
	.prepare	= mtk_cam_vb2_prepare,
	.finish		= mtk_cam_vb2_finish,
	.map_dmabuf	= mtk_cam_vb2_map_dmabuf,
	.unmap_dmabuf	= mtk_cam_vb2_unmap_dmabuf,
	.attach_dmabuf	= mtk_cam_vb2_attach_dmabuf,
	.detach_dmabuf	= mtk_cam_vb2_detach_dmabuf,
	.num_users	= mtk_cam_vb2_num_users,
};
EXPORT_SYMBOL_GPL(mtk_cam_dma_contig_memops);

void mtk_cam_vb2_sync_for_device(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (!sgt)
		return;

	dma_sync_sgtable_for_device(buf->dev, sgt, buf->dma_dir);
}

void mtk_cam_vb2_sync_for_cpu(void *buf_priv)
{
	struct mtk_cam_vb2_buf *buf = buf_priv;
	struct sg_table *sgt = buf->dma_sgt;

	if (!sgt)
		return;

	dma_sync_sgtable_for_cpu(buf->dev, sgt, buf->dma_dir);
}

MODULE_DESCRIPTION("DMA-contig memory handling routines for mtk-cam videobuf2");
MODULE_LICENSE("GPL");
