// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Louis Kuo <louis.kuo@mediatek.com>
 */

#include <linux/device.h>
#include <linux/dma-iommu.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>
#include <linux/mm.h>
#include <linux/remoteproc.h>
#include <linux/spinlock.h>

#include "mtk_cam.h"
#include "mtk_cam-smem.h"
#include "mtk_cam-pool.h"

#ifndef CONFIG_MTK_SCP
#include <linux/platform_data/mtk_ccd.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#include <uapi/linux/mtk_ccd_controls.h>
#endif

int mtk_cam_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;
	struct mem_obj smem;
	struct mem_obj smem_ipi;
	struct mtk_ccd *ccd;
	void *mem_priv;
	void *mem_ipi_priv;
	int dmabuf_fd;
	int dmabuf_ipi_fd;
	const int working_buf_size = round_up(CQ_BUF_SIZE, PAGE_SIZE);
	const int msg_buf_size = round_up(IPI_FRAME_BUF_SIZE, PAGE_SIZE);

	INIT_LIST_HEAD(&ctx->buf_pool.cam_freelist.list);
	spin_lock_init(&ctx->buf_pool.cam_freelist.lock);
	ctx->buf_pool.cam_freelist.cnt = 0;
	ctx->buf_pool.working_buf_size = CAM_CQ_BUF_NUM * working_buf_size;
	smem.len = ctx->buf_pool.working_buf_size;

	ctx->buf_pool.msg_buf_size = CAM_CQ_BUF_NUM * msg_buf_size;
	smem_ipi.len = ctx->buf_pool.msg_buf_size;


	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	dmabuf_fd = mtk_ccd_get_buffer_fd(ccd, mem_priv, 0);

	mem_ipi_priv = mtk_ccd_get_buffer(ccd, &smem_ipi);
	dmabuf_ipi_fd = mtk_ccd_get_buffer_fd(ccd, mem_ipi_priv, 0);

	ctx->buf_pool.working_buf_va = smem.va;
	ctx->buf_pool.working_buf_iova = smem.iova;
	ctx->buf_pool.working_buf_fd = dmabuf_fd;
	ctx->buf_pool.msg_buf_va = smem_ipi.va;
	ctx->buf_pool.msg_buf_fd = dmabuf_ipi_fd;

	for (i = 0; i < CAM_CQ_BUF_NUM; i++) {
		struct mtk_cam_working_buf_entry *buf = &ctx->buf_pool.working_buf[i];
		int offset, offset_msg;

		offset = i * working_buf_size;
		offset_msg = i * msg_buf_size;

		buf->buffer.va = ctx->buf_pool.working_buf_va + offset;
		buf->buffer.iova = ctx->buf_pool.working_buf_iova + offset;
		buf->buffer.size = working_buf_size;
		buf->msg_buffer.va = ctx->buf_pool.msg_buf_va + offset_msg;
		buf->msg_buffer.size = msg_buf_size;
		buf->req = NULL;
		dev_info(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(%pad)\n",
			__func__, ctx->stream_id, i, &buf->buffer.iova);

		list_add_tail(&buf->list_entry, &ctx->buf_pool.cam_freelist.list);
		ctx->buf_pool.cam_freelist.cnt++;
	}

	dev_info(ctx->cam->dev,
		"%s: ctx(%d): cq buffers init, freebuf cnt(%d),fd(%d),ipi_fd(%d)\n",
		__func__, ctx->stream_id, ctx->buf_pool.cam_freelist.cnt,
		dmabuf_fd, dmabuf_ipi_fd);

	return 0;
}

void mtk_cam_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	struct mtk_ccd *ccd = ctx->cam->rproc_handle->priv;
	struct mem_obj smem;
	int fd;

	smem.va = ctx->buf_pool.working_buf_va;
	smem.iova = ctx->buf_pool.working_buf_iova;
	smem.len = ctx->buf_pool.working_buf_size;
	fd = ctx->buf_pool.working_buf_fd;

	mtk_ccd_put_buffer_fd(ccd, &smem, fd);
	mtk_ccd_put_buffer(ccd, &smem);

	dev_dbg(ctx->cam->dev,
		"%s:ctx(%d):cq buffers release, mem iova(%pad), sz(%d)\n",
		__func__, ctx->stream_id, &smem.iova, smem.len);

	smem.va = ctx->buf_pool.msg_buf_va;
	smem.iova = 0;
	smem.len = ctx->buf_pool.msg_buf_size;
	fd = ctx->buf_pool.msg_buf_fd;
	mtk_ccd_put_buffer_fd(ccd, &smem, fd);
	mtk_ccd_put_buffer(ccd, &smem);

	dev_dbg(ctx->cam->dev,
		"%s:ctx(%d):msg buffers release, mem(%p), sz(%d)\n",
		__func__, ctx->stream_id, smem.va, smem.len);

}

void mtk_cam_working_buf_put(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_working_buf_entry *buf_entry)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->buf_pool.cam_freelist.lock, flags);

	if (!buf_entry) {
		dev_dbg(ctx->cam->dev, "%s: buf_entry can' be null, free cnt%d\n",
			__func__, ctx->buf_pool.cam_freelist.cnt);
		spin_unlock_irqrestore(&ctx->buf_pool.cam_freelist.lock, flags);
		return;
	}

	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.cam_freelist.list);
	ctx->buf_pool.cam_freelist.cnt++;
	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova,
		ctx->buf_pool.cam_freelist.cnt);

	spin_unlock_irqrestore(&ctx->buf_pool.cam_freelist.lock, flags);
}

struct mtk_cam_working_buf_entry*
mtk_cam_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_working_buf_entry *buf_entry;
	unsigned long flags;

	/* get from free list */
	spin_lock_irqsave(&ctx->buf_pool.cam_freelist.lock, flags);
	if (list_empty(&ctx->buf_pool.cam_freelist.list)) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):no free buf, free cnt(%d)\n",
			 __func__, ctx->stream_id,
			 ctx->buf_pool.cam_freelist.cnt);
		spin_unlock_irqrestore(&ctx->buf_pool.cam_freelist.lock, flags);

		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.cam_freelist.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->buf_pool.cam_freelist.cnt--;
	buf_entry->ctx = ctx;
	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova,
		ctx->buf_pool.cam_freelist.cnt);

	spin_unlock_irqrestore(&ctx->buf_pool.cam_freelist.lock, flags);

	return buf_entry;
}

int mtk_cam_img_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;
	struct mem_obj smem;
	struct mtk_ccd *ccd;
	struct mtk_cam_video_device *vdev;
	void *mem_priv;
	int dmabuf_fd;
	int working_buf_size;

	vdev = &ctx->pipe->vdev_nodes[MTK_RAW_MAIN_STREAM_OUT - MTK_RAW_SINK_NUM];
	working_buf_size = vdev->active_fmt.fmt.pix_mp.plane_fmt[0].sizeimage;
	INIT_LIST_HEAD(&ctx->img_buf_pool.cam_freeimglist.list);
	spin_lock_init(&ctx->img_buf_pool.cam_freeimglist.lock);
	ctx->img_buf_pool.cam_freeimglist.cnt = 0;
	ctx->img_buf_pool.working_img_buf_size = CAM_IMG_BUF_NUM * working_buf_size;
	smem.len = ctx->img_buf_pool.working_img_buf_size;
	dev_info(ctx->cam->dev, "%s:ctx(%d) smem.len(%d)\n",
			__func__, ctx->stream_id, smem.len);
	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	dmabuf_fd = mtk_ccd_get_buffer_fd(ccd, mem_priv, 0);

	ctx->img_buf_pool.working_img_buf_va = smem.va;
	ctx->img_buf_pool.working_img_buf_iova = smem.iova;
	ctx->img_buf_pool.working_img_buf_fd = dmabuf_fd;

	for (i = 0; i < CAM_IMG_BUF_NUM; i++) {
		struct mtk_cam_img_working_buf_entry *buf = &ctx->img_buf_pool.img_working_buf[i];
		int offset;

		offset = i * working_buf_size;

		buf->img_buffer.va = ctx->img_buf_pool.working_img_buf_va + offset;
		buf->img_buffer.iova = ctx->img_buf_pool.working_img_buf_iova + offset;
		buf->img_buffer.size = working_buf_size;
		dev_info(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(0x%x)\n",
			__func__, ctx->stream_id, i, buf->img_buffer.iova);

		list_add_tail(&buf->list_entry, &ctx->img_buf_pool.cam_freeimglist.list);
		ctx->img_buf_pool.cam_freeimglist.cnt++;
	}

	dev_info(ctx->cam->dev,
		 "%s: ctx(%d): image buffers init, freebuf cnt(%d)\n",
		 __func__, ctx->stream_id, ctx->img_buf_pool.cam_freeimglist.cnt);

	return 0;
}

void mtk_cam_img_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	int fd;
	struct mtk_ccd *ccd = ctx->cam->rproc_handle->priv;
	struct mem_obj smem;

	smem.va = ctx->img_buf_pool.working_img_buf_va;
	smem.iova = ctx->img_buf_pool.working_img_buf_iova;
	smem.len = ctx->img_buf_pool.working_img_buf_size;
	fd = ctx->img_buf_pool.working_img_buf_fd;
	mtk_ccd_put_buffer_fd(ccd, &smem, fd);
	mtk_ccd_put_buffer(ccd, &smem);

	dev_info(ctx->cam->dev,
		"%s:ctx(%d):cq buffers release, mem iova(0x%x), sz(%d)\n",
		__func__, ctx->stream_id, smem.iova, smem.len);
}

void mtk_cam_img_working_buf_put(struct mtk_cam_ctx *ctx,
			     struct mtk_cam_img_working_buf_entry *buf_entry)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->img_buf_pool.cam_freeimglist.lock, flags);

	if (!buf_entry) {
		dev_dbg(ctx->cam->dev, "%s: buf_entry can' be null, free cnt%d\n",
			__func__, ctx->img_buf_pool.cam_freeimglist.cnt);
		spin_unlock_irqrestore(&ctx->img_buf_pool.cam_freeimglist.lock, flags);
		return;
	}

	list_add_tail(&buf_entry->list_entry,
		      &ctx->img_buf_pool.cam_freeimglist.list);
	ctx->img_buf_pool.cam_freeimglist.cnt++;
	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%x), free cnt(%d)\n",
		__func__, ctx->stream_id, buf_entry->img_buffer.iova,
		ctx->img_buf_pool.cam_freeimglist.cnt);

	spin_unlock_irqrestore(&ctx->img_buf_pool.cam_freeimglist.lock, flags);
}

struct mtk_cam_img_working_buf_entry*
mtk_cam_img_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_img_working_buf_entry *buf_entry;
	unsigned long flags;

	/* get from free list */
	spin_lock_irqsave(&ctx->img_buf_pool.cam_freeimglist.lock, flags);
	if (list_empty(&ctx->img_buf_pool.cam_freeimglist.list)) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d):no free buf, free cnt(%d)\n",
			 __func__, ctx->stream_id,
			 ctx->img_buf_pool.cam_freeimglist.cnt);
		spin_unlock_irqrestore(&ctx->img_buf_pool.cam_freeimglist.lock, flags);

		return NULL;
	}

	buf_entry = list_first_entry(&ctx->img_buf_pool.cam_freeimglist.list,
				     struct mtk_cam_img_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->img_buf_pool.cam_freeimglist.cnt--;
	buf_entry->ctx = ctx;
	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%x), free cnt(%d)\n",
		__func__, ctx->stream_id, buf_entry->img_buffer.iova,
		ctx->img_buf_pool.cam_freeimglist.cnt);

	spin_unlock_irqrestore(&ctx->img_buf_pool.cam_freeimglist.lock, flags);

	return buf_entry;
}

int mtk_cam_sv_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;

	INIT_LIST_HEAD(&ctx->buf_pool.sv_freelist.list);
	spin_lock_init(&ctx->buf_pool.sv_freelist.lock);
	ctx->buf_pool.sv_freelist.cnt = 0;

	for (i = 0; i < CAMSV_CQ_BUF_NUM; i++) {
		struct mtk_camsv_working_buf_entry *buf = &ctx->buf_pool.sv_working_buf[i];

		list_add_tail(&buf->list_entry,
			      &ctx->buf_pool.sv_freelist.list);
		ctx->buf_pool.sv_freelist.cnt++;
	}
	dev_info(ctx->cam->dev, "%s:ctx(%d):freebuf cnt(%d)\n", __func__,
		 ctx->stream_id, ctx->buf_pool.sv_freelist.cnt);

	return 0;
}

void mtk_cam_sv_working_buf_put(struct mtk_cam_ctx *ctx,
			     struct mtk_camsv_working_buf_entry *buf_entry)
{
	unsigned long flags;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	if (!buf_entry)
		return;

	spin_lock_irqsave(&ctx->buf_pool.sv_freelist.lock, flags);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.sv_freelist.list);
	ctx->buf_pool.sv_freelist.cnt++;
	spin_unlock_irqrestore(&ctx->buf_pool.sv_freelist.lock, flags);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
}

struct mtk_camsv_working_buf_entry*
mtk_cam_sv_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsv_working_buf_entry *buf_entry;
	unsigned long flags;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	spin_lock_irqsave(&ctx->buf_pool.sv_freelist.lock, flags);
	if (list_empty(&ctx->buf_pool.sv_freelist.list)) {
		spin_unlock_irqrestore(&ctx->buf_pool.sv_freelist.lock, flags);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.sv_freelist.list,
				     struct mtk_camsv_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->buf_pool.sv_freelist.cnt--;
	spin_unlock_irqrestore(&ctx->buf_pool.sv_freelist.lock, flags);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
	return buf_entry;
}
