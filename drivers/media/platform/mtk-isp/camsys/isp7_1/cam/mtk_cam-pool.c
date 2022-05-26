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
#include "mtk_heap.h"

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
	struct mtk_ccd *ccd;
	void *mem_priv;
	int dmabuf_fd;
	struct dma_buf *dbuf;
	const int working_buf_size = round_up(CQ_BUF_SIZE, PAGE_SIZE);
	const int msg_buf_size = round_up(IPI_FRAME_BUF_SIZE, PAGE_SIZE);

	INIT_LIST_HEAD(&ctx->buf_pool.cam_freelist.list);
	spin_lock_init(&ctx->buf_pool.cam_freelist.lock);
	ctx->buf_pool.cam_freelist.cnt = 0;
	ctx->buf_pool.working_buf_size = CAM_CQ_BUF_NUM * working_buf_size;
	ctx->buf_pool.msg_buf_size = CAM_CQ_BUF_NUM * msg_buf_size;
	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;

	/* working buffer */
	smem.len = ctx->buf_pool.working_buf_size;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	if (IS_ERR(mem_priv))
		return PTR_ERR(mem_priv);
	dmabuf_fd = mtk_ccd_get_buffer_fd(ccd, mem_priv);
	dbuf = mtk_ccd_get_buffer_dmabuf(ccd, mem_priv);
	if (dbuf)
		mtk_dma_buf_set_name(dbuf, "CAM_MEM_CQ_ID");
	ctx->buf_pool.working_buf_va = smem.va;
	ctx->buf_pool.working_buf_iova = smem.iova;
	ctx->buf_pool.working_buf_fd = dmabuf_fd;

	/* msg buffer */
	smem.len = ctx->buf_pool.msg_buf_size;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	if (IS_ERR(mem_priv))
		return PTR_ERR(mem_priv);
	dmabuf_fd = mtk_ccd_get_buffer_fd(ccd, mem_priv);
	dbuf = mtk_ccd_get_buffer_dmabuf(ccd, mem_priv);
	if (dbuf)
		mtk_dma_buf_set_name(dbuf, "CAM_MEM_MSG_ID");
	ctx->buf_pool.msg_buf_va = smem.va;
	ctx->buf_pool.msg_buf_fd = dmabuf_fd;

	for (i = 0; i < CAM_CQ_BUF_NUM; i++) {
		struct mtk_cam_working_buf_entry *buf = &ctx->buf_pool.working_buf[i];
		int offset, offset_msg;

		buf->ctx = ctx;
		offset = i * working_buf_size;
		offset_msg = i * msg_buf_size;

		buf->buffer.va = ctx->buf_pool.working_buf_va + offset;
		buf->buffer.iova = ctx->buf_pool.working_buf_iova + offset;
		buf->buffer.size = working_buf_size;
		buf->msg_buffer.va = ctx->buf_pool.msg_buf_va + offset_msg;
		buf->msg_buffer.size = msg_buf_size;
		buf->s_data = NULL;

		dev_dbg(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(%pad)\n",
			__func__, ctx->stream_id, i, &buf->buffer.iova);

		/* meta buffer */
		smem.len = mtk_cam_get_meta_size(MTKCAM_IPI_RAW_META_STATS_1);
		mem_priv = mtk_ccd_get_buffer(ccd, &smem);
		if (IS_ERR(mem_priv))
			return PTR_ERR(mem_priv);
		buf->meta_buffer.fd = mtk_ccd_get_buffer_fd(ccd, mem_priv);
		dbuf = mtk_ccd_get_buffer_dmabuf(ccd, mem_priv);
		if (dbuf)
			mtk_dma_buf_set_name(dbuf, "CAM_MEM_META_ID");
		buf->meta_buffer.va = smem.va;
		buf->meta_buffer.iova = smem.iova;
		buf->meta_buffer.size = smem.len;

		dev_dbg(ctx->cam->dev,
			 "%s:meta_buf[%d]:va(%d),iova(%pad),fd(%d),size(%d)\n",
			 __func__, i, buf->meta_buffer.va, &buf->meta_buffer.iova,
			 buf->meta_buffer.fd, buf->meta_buffer.size);

		list_add_tail(&buf->list_entry, &ctx->buf_pool.cam_freelist.list);
		ctx->buf_pool.cam_freelist.cnt++;
	}

	dev_info(ctx->cam->dev,
		"%s: ctx(%d): cq buffers init, freebuf cnt(%d),fd(%d)\n",
		__func__, ctx->stream_id, ctx->buf_pool.cam_freelist.cnt,
		dmabuf_fd);

	return 0;
}

void mtk_cam_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_working_buf_entry *buf;
	struct mtk_ccd *ccd = ctx->cam->rproc_handle->priv;
	struct mem_obj smem;
	int fd, i;

	/* meta buffer */
	for (i = 0; i < CAM_CQ_BUF_NUM; i++) {
		buf = &ctx->buf_pool.working_buf[i];

		smem.va = buf->meta_buffer.va;
		smem.iova = buf->meta_buffer.iova;
		smem.len = buf->meta_buffer.size;
		fd = buf->meta_buffer.fd;

		mtk_ccd_put_buffer_fd(ccd, &smem, fd);
		mtk_ccd_put_buffer(ccd, &smem);
		dev_dbg(ctx->cam->dev,
			"%s:ctx(%d):meta buffers[%d] release, mem iova(%pad), sz(%d)\n",
			__func__, ctx->stream_id, i, &smem.iova, smem.len);

		buf->meta_buffer.size = 0;
	}

	/* msg buffer */
	smem.va = ctx->buf_pool.working_buf_va;
	smem.iova = ctx->buf_pool.working_buf_iova;
	smem.len = ctx->buf_pool.working_buf_size;
	fd = ctx->buf_pool.working_buf_fd;

	mtk_ccd_put_buffer_fd(ccd, &smem, fd);
	mtk_ccd_put_buffer(ccd, &smem);

	dev_dbg(ctx->cam->dev,
		"%s:ctx(%d):cq buffers release, mem iova(%pad), sz(%d)\n",
		__func__, ctx->stream_id, &smem.iova, smem.len);

	/* working buffer */
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

void
mtk_cam_working_buf_put(struct mtk_cam_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;
	int cnt;

	spin_lock(&ctx->buf_pool.cam_freelist.lock);

	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.cam_freelist.list);
	cnt = ++ctx->buf_pool.cam_freelist.cnt;

	spin_unlock(&ctx->buf_pool.cam_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova, cnt);
}

struct mtk_cam_working_buf_entry*
mtk_cam_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_working_buf_entry *buf_entry;
	int cnt;

	/* get from free list */
	spin_lock(&ctx->buf_pool.cam_freelist.lock);
	if (list_empty(&ctx->buf_pool.cam_freelist.list)) {
		spin_unlock(&ctx->buf_pool.cam_freelist.lock);

		dev_info(ctx->cam->dev, "%s:ctx(%d):no free buf\n",
			 __func__, ctx->stream_id);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.cam_freelist.list,
				     struct mtk_cam_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	cnt = --ctx->buf_pool.cam_freelist.cnt;
	buf_entry->ctx = ctx;

	spin_unlock(&ctx->buf_pool.cam_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(%pad), free cnt(%d)\n",
		__func__, ctx->stream_id, &buf_entry->buffer.iova, cnt);

	return buf_entry;
}

int mtk_cam_img_working_buf_pool_init(struct mtk_cam_ctx *ctx, int buf_num,
									  int working_buf_size)
{
	int i;
	struct mem_obj smem;
	struct mtk_ccd *ccd;
	void *mem_priv;
	int dmabuf_fd;
	struct dma_buf *dbuf;

	if (buf_num > CAM_IMG_BUF_NUM) {
		dev_info(ctx->cam->dev,
		"%s: ctx(%d): image buffers number too large(%d)\n",
		__func__, ctx->stream_id, buf_num);
		WARN_ON(1);
		return 0;
	}

	INIT_LIST_HEAD(&ctx->img_buf_pool.cam_freeimglist.list);
	spin_lock_init(&ctx->img_buf_pool.cam_freeimglist.lock);
	ctx->img_buf_pool.cam_freeimglist.cnt = 0;
	ctx->img_buf_pool.working_img_buf_size = buf_num * working_buf_size;
	smem.len = ctx->img_buf_pool.working_img_buf_size;
	dev_info(ctx->cam->dev, "%s:ctx(%d) smem.len(%d)\n",
			__func__, ctx->stream_id, smem.len);
	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	if (IS_ERR(mem_priv))
		return PTR_ERR(mem_priv);
	dmabuf_fd = mtk_ccd_get_buffer_fd(ccd, mem_priv);
	dbuf = mtk_ccd_get_buffer_dmabuf(ccd, mem_priv);
	if (dbuf)
		mtk_dma_buf_set_name(dbuf, "CAM_MEM_IMG_ID");
	ctx->img_buf_pool.working_img_buf_va = smem.va;
	ctx->img_buf_pool.working_img_buf_iova = smem.iova;
	ctx->img_buf_pool.working_img_buf_fd = dmabuf_fd;

	for (i = 0; i < buf_num; i++) {
		struct mtk_cam_img_working_buf_entry *buf = &ctx->img_buf_pool.img_working_buf[i];
		int offset;

		offset = i * working_buf_size;

		buf->ctx = ctx;
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
	ctx->img_buf_pool.working_img_buf_size = 0;

	dev_info(ctx->cam->dev,
		"%s:ctx(%d):cq buffers release, mem iova(0x%x), sz(%d)\n",
		__func__, ctx->stream_id, smem.iova, smem.len);
}

void mtk_cam_img_working_buf_put(struct mtk_cam_img_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;
	int cnt;

	spin_lock(&ctx->img_buf_pool.cam_freeimglist.lock);

	list_add_tail(&buf_entry->list_entry,
		      &ctx->img_buf_pool.cam_freeimglist.list);
	cnt = ++ctx->img_buf_pool.cam_freeimglist.cnt;

	spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%x), free cnt(%d)\n",
		__func__, ctx->stream_id, buf_entry->img_buffer.iova, cnt);
}

struct mtk_cam_img_working_buf_entry*
mtk_cam_img_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_cam_img_working_buf_entry *buf_entry;
	int cnt;

	/* get from free list */
	spin_lock(&ctx->img_buf_pool.cam_freeimglist.lock);
	if (list_empty(&ctx->img_buf_pool.cam_freeimglist.list)) {
		spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

		dev_info(ctx->cam->dev, "%s:ctx(%d):no free buf\n",
			 __func__, ctx->stream_id);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->img_buf_pool.cam_freeimglist.list,
				     struct mtk_cam_img_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	cnt = --ctx->img_buf_pool.cam_freeimglist.cnt;

	spin_unlock(&ctx->img_buf_pool.cam_freeimglist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):iova(0x%x), free cnt(%d)\n",
		__func__, ctx->stream_id, buf_entry->img_buffer.iova, cnt);

	return buf_entry;
}

int mtk_cam_sv_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;

	INIT_LIST_HEAD(&ctx->buf_pool.sv_freelist.list);
	spin_lock_init(&ctx->buf_pool.sv_freelist.lock);
	ctx->buf_pool.sv_freelist.cnt = 0;

	for (i = 0; i < CAMSV_WORKING_BUF_NUM; i++) {
		struct mtk_camsv_working_buf_entry *buf = &ctx->buf_pool.sv_working_buf[i];
		buf->ctx = ctx;

		list_add_tail(&buf->list_entry,
			      &ctx->buf_pool.sv_freelist.list);
		ctx->buf_pool.sv_freelist.cnt++;
	}
	dev_info(ctx->cam->dev, "%s:ctx(%d):freebuf cnt(%d)\n", __func__,
		 ctx->stream_id, ctx->buf_pool.sv_freelist.cnt);

	return 0;
}

void
mtk_cam_sv_working_buf_put(struct mtk_camsv_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	spin_lock(&ctx->buf_pool.sv_freelist.lock);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.sv_freelist.list);
	ctx->buf_pool.sv_freelist.cnt++;
	spin_unlock(&ctx->buf_pool.sv_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
}

struct mtk_camsv_working_buf_entry*
mtk_cam_sv_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_camsv_working_buf_entry *buf_entry;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	spin_lock(&ctx->buf_pool.sv_freelist.lock);
	if (list_empty(&ctx->buf_pool.sv_freelist.list)) {
		spin_unlock(&ctx->buf_pool.sv_freelist.lock);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.sv_freelist.list,
				     struct mtk_camsv_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->buf_pool.sv_freelist.cnt--;
	spin_unlock(&ctx->buf_pool.sv_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
	return buf_entry;
}

int mtk_cam_mraw_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;
	const int working_buf_size = round_up(CQ_BUF_SIZE, PAGE_SIZE);

	INIT_LIST_HEAD(&ctx->buf_pool.mraw_freelist.list);
	spin_lock_init(&ctx->buf_pool.mraw_freelist.lock);
	ctx->buf_pool.mraw_freelist.cnt = 0;

	for (i = 0; i < MRAW_WORKING_BUF_NUM; i++) {
		struct mtk_mraw_working_buf_entry *buf
				= &ctx->buf_pool.mraw_working_buf[i];
		int offset;

		offset = i * working_buf_size;

		buf->buffer.va = ctx->buf_pool.working_buf_va + offset;
		buf->buffer.iova = ctx->buf_pool.working_buf_iova + offset;
		buf->buffer.size = working_buf_size;
		buf->s_data = NULL;
		dev_dbg(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(%pad)\n",
			__func__, ctx->stream_id, i, &buf->buffer.iova);

		list_add_tail(&buf->list_entry,
			      &ctx->buf_pool.mraw_freelist.list);
		ctx->buf_pool.mraw_freelist.cnt++;
	}

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):freebuf cnt(%d)\n", __func__,
		 ctx->stream_id, ctx->buf_pool.mraw_freelist.cnt);

	return 0;
}

void mtk_cam_mraw_working_buf_put(struct mtk_cam_ctx *ctx,
			     struct mtk_mraw_working_buf_entry *buf_entry)
{
	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	if (!buf_entry)
		return;

	spin_lock(&ctx->buf_pool.mraw_freelist.lock);
	list_add_tail(&buf_entry->list_entry,
		      &ctx->buf_pool.mraw_freelist.list);
	ctx->buf_pool.mraw_freelist.cnt++;
	spin_unlock(&ctx->buf_pool.mraw_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
}

struct mtk_mraw_working_buf_entry*
mtk_cam_mraw_working_buf_get(struct mtk_cam_ctx *ctx)
{
	struct mtk_mraw_working_buf_entry *buf_entry;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	spin_lock(&ctx->buf_pool.mraw_freelist.lock);
	if (list_empty(&ctx->buf_pool.mraw_freelist.list)) {
		spin_unlock(&ctx->buf_pool.mraw_freelist.lock);
		return NULL;
	}

	buf_entry = list_first_entry(&ctx->buf_pool.mraw_freelist.list,
				     struct mtk_mraw_working_buf_entry,
				     list_entry);
	list_del(&buf_entry->list_entry);
	ctx->buf_pool.mraw_freelist.cnt--;
	spin_unlock(&ctx->buf_pool.mraw_freelist.lock);

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):e\n", __func__, ctx->stream_id);
	return buf_entry;
}
