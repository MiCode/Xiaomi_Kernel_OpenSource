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
#include "mtk_cam-feature.h"
#include "mtk_cam-smem.h"
#include "mtk_cam-pool.h"
#include "mtk_heap.h"

#ifndef CONFIG_MTK_SCP
#include <linux/platform_data/mtk_ccd.h>
#include <linux/rpmsg/mtk_ccd_rpmsg.h>
#include <linux/remoteproc/mtk_ccd_mem.h>
#include <uapi/linux/mtk_ccd_controls.h>
#endif

static int debug_cam_pool;
module_param(debug_cam_pool, int, 0644);

#undef dev_dbg
#define dev_dbg(dev, fmt, arg...)		\
	do {					\
		if (debug_cam_pool >= 1)	\
			dev_info(dev, fmt,	\
				## arg);	\
	} while (0)

#define WORKING_BUF_SIZE	round_up(CQ_BUF_SIZE, PAGE_SIZE)
#define MSG_BUF_SIZE		round_up(IPI_FRAME_BUF_SIZE, PAGE_SIZE)

int mtk_cam_working_buf_pool_alloc(struct mtk_cam_ctx *ctx)
{
	struct mem_obj smem;
	struct mtk_ccd *ccd;
	void *mem_priv;
	int dmabuf_fd;
	struct dma_buf *dbuf;

	ctx->buf_pool.working_buf_size = CAM_CQ_BUF_NUM * WORKING_BUF_SIZE;
	ctx->buf_pool.msg_buf_size = CAM_CQ_BUF_NUM * MSG_BUF_SIZE;
	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;

	/* working buffer */
	smem.len = ctx->buf_pool.working_buf_size;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	if (IS_ERR(mem_priv))
		return PTR_ERR(mem_priv);

	/* close fd in userspace driver */
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

	/* close fd in userspace driver */
	dmabuf_fd = mtk_ccd_get_buffer_fd(ccd, mem_priv);
	dbuf = mtk_ccd_get_buffer_dmabuf(ccd, mem_priv);
	if (dbuf)
		mtk_dma_buf_set_name(dbuf, "CAM_MEM_MSG_ID");
	ctx->buf_pool.msg_buf_va = smem.va;
	ctx->buf_pool.msg_buf_fd = dmabuf_fd;

	return 0;
}

int mtk_cam_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;

	INIT_LIST_HEAD(&ctx->buf_pool.cam_freelist.list);
	spin_lock_init(&ctx->buf_pool.cam_freelist.lock);
	ctx->buf_pool.cam_freelist.cnt = 0;

	for (i = 0; i < CAM_CQ_BUF_NUM; i++) {
		struct mtk_cam_working_buf_entry *buf = &ctx->buf_pool.working_buf[i];
		int offset, offset_msg;

		buf->ctx = ctx;
		offset = i * WORKING_BUF_SIZE;
		offset_msg = i * MSG_BUF_SIZE;

		buf->buffer.va = ctx->buf_pool.working_buf_va + offset;
		buf->buffer.iova = ctx->buf_pool.working_buf_iova + offset;
		buf->buffer.size = WORKING_BUF_SIZE;
		buf->msg_buffer.va = ctx->buf_pool.msg_buf_va + offset_msg;
		buf->msg_buffer.size = MSG_BUF_SIZE;
		buf->s_data = NULL;

		dev_dbg(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(%pad)\n",
			__func__, ctx->stream_id, i, &buf->buffer.iova);

		list_add_tail(&buf->list_entry, &ctx->buf_pool.cam_freelist.list);
		ctx->buf_pool.cam_freelist.cnt++;
	}

	dev_info(ctx->cam->dev,
		"%s: ctx(%d): cq buffers init, freebuf cnt(%d),working(%d),msgfd(%d)\n",
		__func__, ctx->stream_id, ctx->buf_pool.cam_freelist.cnt,
		ctx->buf_pool.working_buf_fd, ctx->buf_pool.msg_buf_fd);

	return 0;
}

void mtk_cam_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	struct mtk_ccd *ccd = ctx->cam->rproc_handle->priv;
	struct mem_obj smem;

	/* msg buffer */
	smem.va = ctx->buf_pool.working_buf_va;
	smem.iova = ctx->buf_pool.working_buf_iova;
	smem.len = ctx->buf_pool.working_buf_size;
	mtk_ccd_put_buffer(ccd, &smem);

	dev_dbg(ctx->cam->dev,
		"%s:ctx(%d):cq buffers release, mem iova(%pad), sz(%d)\n",
		__func__, ctx->stream_id, &smem.iova, smem.len);

	/* working buffer */
	smem.va = ctx->buf_pool.msg_buf_va;
	smem.iova = 0;
	smem.len = ctx->buf_pool.msg_buf_size;
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

static int
mtk_cam_img_working_buf_pool_init(struct mtk_cam_ctx *ctx, int buf_num,
				  int working_buf_size,
				  dma_addr_t mem_iova, int mem_size,
				  void *mem_va)
{
	int i;

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
	ctx->img_buf_pool.working_img_buf_va = mem_va;
	ctx->img_buf_pool.working_img_buf_iova = mem_iova;

	for (i = 0; i < buf_num; i++) {
		struct mtk_cam_img_working_buf_entry *buf = &ctx->img_buf_pool.img_working_buf[i];
		int offset;

		offset = i * working_buf_size;

		buf->ctx = ctx;
		buf->img_buffer.va = ctx->img_buf_pool.working_img_buf_va + offset;
		buf->img_buffer.iova = ctx->img_buf_pool.working_img_buf_iova + offset;
		buf->img_buffer.size = working_buf_size;
		dev_info(ctx->cam->dev, "%s:ctx(%d):buf(%d), iova(%pad)\n",
			 __func__, ctx->stream_id, i, &buf->img_buffer.iova);

		list_add_tail(&buf->list_entry, &ctx->img_buf_pool.cam_freeimglist.list);
		ctx->img_buf_pool.cam_freeimglist.cnt++;
	}

	dev_info(ctx->cam->dev,
		 "%s: ctx(%d): image buffers init, freebuf cnt(%d)\n",
		 __func__, ctx->stream_id, ctx->img_buf_pool.cam_freeimglist.cnt);

	return 0;
}

static unsigned long _get_contiguous_size(struct sg_table *sgt)
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

static int mtk_cam_device_buf_init(struct mtk_cam_device_buf *buf,
				   struct dma_buf *dbuf,
				   struct device *dev, size_t expected_size)
{
	unsigned long size;

	memset(buf, 0, sizeof(*buf));

	buf->dbuf = dbuf;
	buf->db_attach = dma_buf_attach(dbuf, dev);
	if (IS_ERR(buf->db_attach)) {
		dev_info(dev, "failed to attach dbuf: %s\n", dev_name(dev));
		return -1;
	}

	buf->dma_sgt = dma_buf_map_attachment(buf->db_attach,
					      DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->dma_sgt)) {
		dev_info(dev, "failed to map attachment\n");
		goto fail_detach;
	}

	/* check size */
	size = _get_contiguous_size(buf->dma_sgt);
	if (expected_size > size) {
		dev_info(dev,
			 "%s: dma_sgt size(%zu) smaller than expected(%zu)\n",
			 __func__, size, expected_size);
		goto fail_attach_unmap;
	}

	buf->size = expected_size;
	buf->daddr = sg_dma_address(buf->dma_sgt->sgl);

	return 0;

fail_attach_unmap:
	dma_buf_unmap_attachment(buf->db_attach, buf->dma_sgt,
				 DMA_BIDIRECTIONAL);
	buf->dma_sgt = NULL;
fail_detach:
	dma_buf_detach(buf->dbuf, buf->db_attach);
	buf->db_attach = NULL;
	buf->dbuf = NULL;
	return -1;
}

static void mtk_cam_device_buf_uninit(struct mtk_cam_device_buf *buf)
{
	struct dma_buf_map map = DMA_BUF_MAP_INIT_VADDR(buf->vaddr);

	WARN_ON(!buf->dbuf || !buf->size);
	if (buf->dma_sgt) {
		dma_buf_unmap_attachment(buf->db_attach, buf->dma_sgt,
					 DMA_BIDIRECTIONAL);
		buf->dma_sgt = NULL;
		buf->daddr = 0;
	} else {
		pr_info("%s: failed, buf->dma_sgt is null\n", __func__);
	}

	if (buf->vaddr) {
		dma_buf_vunmap(buf->dbuf, &map);
		buf->vaddr = NULL;
	}

	if (buf->db_attach) {
		dma_buf_detach(buf->dbuf, buf->db_attach);
		buf->db_attach = NULL;
	} else {
		pr_info("%s: failed, buf->db_attach is null\n", __func__);
	}
}

static int mtk_cam_user_buf_attach_map(struct device *dev,
				       struct mtk_cam_internal_buf *user_buf,
				       struct mtk_cam_device_buf *buf)
{
	struct dma_buf *dbuf;

	dbuf = dma_buf_get(user_buf->fd);
	if (IS_ERR(dbuf)) {
		dev_info(dev, "%s:invalid fd %d",
			 __func__, user_buf->fd);
		return -EINVAL;
	}

	buf->dbuf = dbuf;

	return mtk_cam_device_buf_init(buf, dbuf, dev, user_buf->length);
}

int mtk_cam_user_buf_deattach_unmap(struct mtk_cam_device_buf *buf)
{
	struct dma_buf *dbuf;

	if (!buf)
		return -EINVAL;

	dbuf = buf->dbuf;
	if (buf)
		mtk_cam_device_buf_uninit(buf);

	if (dbuf)
		dma_buf_put(dbuf);

	return 0;
}

int mtk_cam_user_img_working_buf_pool_init(struct mtk_cam_ctx *ctx,
					   int buf_num,
					   int working_buf_size)
{
	struct mtk_ccd *ccd;
	int ret = -EINVAL;

	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;
	if (!mtk_cam_user_buf_attach_map(ccd->dev,
					 &ctx->pipe->pre_alloc_mem.bufs[0],
					 &ctx->img_buf_pool.pre_alloc_img_buf)) {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): attached/mapped user memory(%d,%d), sz(%zu), daddr(%pad)\n",
			 __func__, ctx->stream_id,
			 ctx->pipe->pre_alloc_mem.bufs[0].fd,
			 ctx->pipe->pre_alloc_mem.bufs[0].length,
			 ctx->img_buf_pool.pre_alloc_img_buf.size,
			 &ctx->img_buf_pool.pre_alloc_img_buf.daddr);

		ret = mtk_cam_img_working_buf_pool_init(ctx, buf_num,
							working_buf_size,
							ctx->img_buf_pool.pre_alloc_img_buf.daddr,
							ctx->img_buf_pool.pre_alloc_img_buf.size,
							ctx->img_buf_pool.pre_alloc_img_buf.vaddr);
	} else {
		dev_info(ctx->cam->dev,
			 "%s:ctx(%d): failed to attach/map user memory(%d,%d)\n",
			 __func__, ctx->stream_id,
			 ctx->pipe->pre_alloc_mem.bufs[0].fd,
			 ctx->pipe->pre_alloc_mem.bufs[0].length);
	}

	return ret;
}

int
mtk_cam_internal_img_working_buf_pool_init(struct mtk_cam_ctx *ctx,
					   int buf_num,
					   int working_buf_size)
{
	struct mem_obj smem;
	struct mtk_ccd *ccd;
	void *mem_priv;
	struct dma_buf *dbuf;

	smem.len =  buf_num * working_buf_size;
	dev_info(ctx->cam->dev, "%s:ctx(%d) smem.len(%d)\n",
		 __func__, ctx->stream_id, smem.len);
	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	if (IS_ERR(mem_priv))
		return PTR_ERR(mem_priv);

	dbuf = mtk_ccd_get_buffer_dmabuf(ccd, mem_priv);
	if (dbuf)
		mtk_dma_buf_set_name(dbuf, "CAM_MEM_IMG_ID");

	return mtk_cam_img_working_buf_pool_init(ctx, buf_num, working_buf_size,
						 smem.iova, smem.len, smem.va);
}

void mtk_cam_img_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	ctx->img_buf_pool.working_img_buf_size = 0;
}

void mtk_cam_user_img_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	dev_info(ctx->cam->dev,
		 "%s:ctx(%d): deattach/unmap user memory(%d,%d), sz(%zu), daddr(%pad)\n",
		 __func__, ctx->stream_id,
		 ctx->pipe->pre_alloc_mem.bufs[0].fd,
		 ctx->pipe->pre_alloc_mem.bufs[0].length,
		 ctx->img_buf_pool.pre_alloc_img_buf.size,
		 &ctx->img_buf_pool.pre_alloc_img_buf.daddr);
	mtk_cam_user_buf_deattach_unmap(&ctx->img_buf_pool.pre_alloc_img_buf);
	ctx->img_buf_pool.pre_alloc_img_buf.daddr = 0;
	ctx->img_buf_pool.pre_alloc_img_buf.size = 0;
	mtk_cam_img_working_buf_pool_release(ctx);
}

void mtk_cam_internal_img_working_buf_pool_release(struct mtk_cam_ctx *ctx)
{
	struct mtk_ccd *ccd = ctx->cam->rproc_handle->priv;
	struct mem_obj smem;

	smem.va = ctx->img_buf_pool.working_img_buf_va;
	smem.iova = ctx->img_buf_pool.working_img_buf_iova;
	smem.len = ctx->img_buf_pool.working_img_buf_size;
	mtk_ccd_put_buffer(ccd, &smem);

	dev_info(ctx->cam->dev,
		"%s:ctx(%d):img buffers release, mem iova(0x%x), sz(%d)\n",
		__func__, ctx->stream_id, smem.iova, smem.len);
	mtk_cam_img_working_buf_pool_release(ctx);

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

long mtk_cam_generic_buf_alloc(struct mtk_cam_ctx *ctx, u32 size)
{
	struct mem_obj smem;
	struct mtk_ccd *ccd;
	void *mem_priv;
	struct dma_buf *dbuf;

	if (ctx->generic_buf.size) {
		dev_info(ctx->cam->dev, "%s: buf already existed(size %d)\n",
		 __func__, ctx->generic_buf.size);
		mtk_cam_generic_buf_release(ctx);
	}

	smem.len = size;
	dev_info(ctx->cam->dev, "%s: ctx(%d) smem.len(%d)\n",
		 __func__, ctx->stream_id, smem.len);
	ccd = (struct mtk_ccd *)ctx->cam->rproc_handle->priv;
	mem_priv = mtk_ccd_get_buffer(ccd, &smem);
	if (IS_ERR(mem_priv))
		return PTR_ERR(mem_priv);

	dbuf = mtk_ccd_get_buffer_dmabuf(ccd, mem_priv);
	if (dbuf)
		mtk_dma_buf_set_name(dbuf, "CAM_MEM_GEN_ID");

	ctx->generic_buf.size = smem.len;
	ctx->generic_buf.va = smem.va;
	ctx->generic_buf.iova = smem.iova;

	return 0;
}

void mtk_cam_generic_buf_release(struct mtk_cam_ctx *ctx)
{
	struct mtk_ccd *ccd = ctx->cam->rproc_handle->priv;
	struct mem_obj smem;

	if (!ctx->generic_buf.size)
		return;

	/* msg buffer */
	smem.va = ctx->generic_buf.va;
	smem.iova = ctx->generic_buf.iova;
	smem.len = ctx->generic_buf.size;
	mtk_ccd_put_buffer(ccd, &smem);

	ctx->generic_buf.va = 0;
	ctx->generic_buf.iova = 0;
	ctx->generic_buf.size = 0;

	dev_dbg(ctx->cam->dev,
		"%s: ctx(%d):cq buffers release, mem iova(%pad), sz(%d)\n",
		__func__, ctx->stream_id, &smem.iova, smem.len);
}

int mtk_cam_sv_working_buf_pool_init(struct mtk_cam_ctx *ctx)
{
	int i;

	INIT_LIST_HEAD(&ctx->buf_pool.sv_freelist.list);
	spin_lock_init(&ctx->buf_pool.sv_freelist.lock);
	ctx->buf_pool.sv_freelist.cnt = 0;

	for (i = 0; i < CAMSV_WORKING_BUF_NUM; i++) {
		struct mtk_camsv_working_buf_entry *buf = &ctx->buf_pool.sv_working_buf[i];
		int offset, offset_msg;

		buf->ctx = ctx;
		offset = i * WORKING_BUF_SIZE;
		offset_msg = i * MSG_BUF_SIZE;

		buf->buffer.va = ctx->buf_pool.working_buf_va + offset;
		buf->buffer.iova = ctx->buf_pool.working_buf_iova + offset;
		buf->buffer.size = WORKING_BUF_SIZE;
		buf->msg_buffer.va = ctx->buf_pool.msg_buf_va + offset_msg;
		buf->msg_buffer.size = MSG_BUF_SIZE;
		buf->s_data = NULL;

		list_add_tail(&buf->list_entry,
			      &ctx->buf_pool.sv_freelist.list);
		ctx->buf_pool.sv_freelist.cnt++;
	}
	dev_info(ctx->cam->dev, "%s:ctx(%d):freebuf cnt(%d),working(%d),msgfd(%d)\n", __func__,
		ctx->stream_id, ctx->buf_pool.sv_freelist.cnt,
		ctx->buf_pool.working_buf_fd, ctx->buf_pool.msg_buf_fd);

	return 0;
}

void
mtk_cam_sv_working_buf_put(struct mtk_camsv_working_buf_entry *buf_entry)
{
	struct mtk_cam_ctx *ctx = buf_entry->ctx;

	dev_dbg(ctx->cam->dev, "%s:ctx(%d):s\n", __func__, ctx->stream_id);

	if (!buf_entry)
		return;

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

int mtk_cam_get_internl_buf_num(int user_reserved_exp_num,
				struct mtk_cam_scen *scen, int hw_mode)
{
	int exp_no;
	int buf_require = 0;

	/* check exposure number */
	exp_no = mtk_cam_scen_get_max_exp_num(scen);
	if (user_reserved_exp_num > 1 || mtk_cam_scen_is_switchable_hdr(scen))
		// dcif may have to double for during skip frame
		buf_require = max(buf_require, exp_no);

	if (mtk_cam_scen_is_ext_isp(scen))
		buf_require = max(buf_require, 2);

	if (mtk_cam_scen_is_time_shared(scen))
		buf_require = max(buf_require, CAM_IMG_BUF_NUM);

	if (mtk_cam_hw_mode_is_dc(hw_mode))
		buf_require = max(buf_require, exp_no * 2);

	if (mtk_cam_scen_is_rgbw_supported(scen))
		buf_require *= 2; // bayer + w

	return buf_require;
}
