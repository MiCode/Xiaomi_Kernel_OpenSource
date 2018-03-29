/*
* Copyright (c) 2015 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*         Tiffany Lin <tiffany.lin@mediatek.com>
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

#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>

#include "mtkbuf-dma-cache-sg.h"
#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_drv_if.h"
#include "mtk_vcodec_pm.h"

static struct mtk_video_fmt mtk_video_formats[] = {
	{
		.name		= "4:2:0 2 Planes Y/CbCr",
		.fourcc		= V4L2_PIX_FMT_MT21,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 2,
	},
	{
		.name		= "H264 Encoded Stream",
		.fourcc		= V4L2_PIX_FMT_H264,
		.type		= MTK_FMT_DEC,
		.num_planes	= 1,
	},
	{
		.name		= "VP8 Encoded Stream",
		.fourcc		= V4L2_PIX_FMT_VP8,
		.type		= MTK_FMT_DEC,
		.num_planes	= 1,
	},
	{
		.name		= "VP9 Encoded Stream",
		.fourcc		= V4L2_PIX_FMT_VP9,
		.type		= MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.name		= "MPEG-4 Encoded Stream",
		.fourcc		= V4L2_PIX_FMT_MPEG4,
		.type		= MTK_FMT_DEC,
		.num_planes = 1,
	},
};

static const struct mtk_codec_framesizes mtk_vdec_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264,
		.stepwise = {  64, 2048, 16, 64, 1088, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_VP8,
		.stepwise = {  64, 2048, 16, 64, 1088, 16 },
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.stepwise = {  1, 2048, 16, 1, 1088, 16 },
	},
	{
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.stepwise = {  64, 2048, 16, 64, 1088, 16 },
	},
};

#define VCODEC_CAPABILITY_4K_DISABLED	0x10
#define VCODEC_DEC_4K_CODED_WIDTH	4096
#define VCODEC_DEC_4K_CODED_HEIGHT	2304
#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(mtk_vdec_framesizes)
#define NUM_FORMATS ARRAY_SIZE(mtk_video_formats)
#define NUM_CTRLS ARRAY_SIZE(controls)

const struct vb2_ops mtk_vdec_vb2_ops;

static struct mtk_video_fmt *mtk_vdec_find_format(struct v4l2_format *f,
							unsigned int t)
{
	struct mtk_video_fmt *fmt;
	unsigned int k;

	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == f->fmt.pix_mp.pixelformat &&
		    mtk_video_formats[k].type == t)
			return fmt;
	}

	return NULL;
}

static struct mtk_vcodec_ctrl controls[] = {
	{
		.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Minimum number of cap bufs",
		.minimum = 1,
		.maximum = 32,
		.step = 1,
		.default_value = 1,
		.is_volatile = 1,
	},
};

static struct mtk_q_data *mtk_vdec_get_q_data(struct mtk_vcodec_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->q_data[MTK_Q_DATA_SRC];

		return &ctx->q_data[MTK_Q_DATA_DST];
}

static struct vb2_buffer *get_display_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct vdec_fb *disp_frame_buffer;
	struct vb2_queue *vq;
	struct mtk_video_buf *dstbuf;
	struct vdec_fb *frame_buffer;

	mtk_v4l2_debug(3, "[%d]", ctx->idx);
	if (0 != vdec_if_get_param(ctx,
				   GET_PARAM_DISP_FRAME_BUFFER,
				   &disp_frame_buffer)) {
		mtk_v4l2_err("[%d]Cannot get param : GET_PARAM_DISP_FRAME_BUFFER",
			       ctx->idx);
		return NULL;
	}

	if (NULL == disp_frame_buffer) {
		mtk_v4l2_debug(3, "No display frame buffer");
		return NULL;
	}

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (!vq) {
		mtk_v4l2_err("Cannot find V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE queue!");
		return NULL;
	}

	frame_buffer = disp_frame_buffer;
	dstbuf = container_of(frame_buffer, struct mtk_video_buf, frame_buffer);
	mutex_lock(&dstbuf->lock);
	if (dstbuf->used) {
		vb2_set_plane_payload(&dstbuf->b, 0, ctx->picinfo.y_bs_sz);
		vb2_set_plane_payload(&dstbuf->b, 1, ctx->picinfo.c_bs_sz);

		dstbuf->ready_to_display = true;
		dstbuf->nonrealdisplay = false;
		mtk_v4l2_debug(2,
				"[%d]status=%x queue idx=%d to done_list %d",
				ctx->idx, frame_buffer->status,
				dstbuf->b.v4l2_buf.index,
				dstbuf->queued_in_vb2);

		v4l2_m2m_buf_done(&dstbuf->b, VB2_BUF_STATE_DONE);
		ctx->decoded_frame_cnt++;
	}
	mutex_unlock(&dstbuf->lock);
	return &dstbuf->b;
}

static struct vb2_buffer *get_free_buffer(struct mtk_vcodec_ctx *ctx)
{
	void *tmp_frame_addr;
	struct vb2_queue *vq;
	struct mtk_video_buf *dstbuf;
	struct vdec_fb *frame_buffer;

	if (0 != vdec_if_get_param(ctx,
				   GET_PARAM_FREE_FRAME_BUFFER,
				   &tmp_frame_addr)) {
		mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->idx);
		return NULL;
	}
	if (NULL == tmp_frame_addr) {
		mtk_v4l2_debug(3, " No free frame buffer");
		return NULL;
	}

	mtk_v4l2_debug(3, "[%d] tmp_frame_addr = 0x%p",
			 ctx->idx, tmp_frame_addr);

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	if (!vq) {
		mtk_v4l2_err("Cannot find V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE queue!");
		return NULL;
	}

	frame_buffer = tmp_frame_addr;
	dstbuf = container_of(frame_buffer, struct mtk_video_buf, frame_buffer);

	mutex_lock(&dstbuf->lock);
	if (dstbuf->used) {
		if ((dstbuf->queued_in_vb2) &&
			(dstbuf->queued_in_v4l2) &&
			(frame_buffer->status == FB_ST_FREE)) {
			mtk_v4l2_debug(2,
					 "[%d]status=%x queue idx=%d to rdy_queue %d",
					 ctx->idx, frame_buffer->status,
					 dstbuf->b.v4l2_buf.index,
					 dstbuf->queued_in_vb2);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->b);
		} else if ((dstbuf->queued_in_vb2 == false) &&
			   (dstbuf->queued_in_v4l2 == true)) {
			mtk_v4l2_debug(2,
					 "[%d]status=%x queue idx=%d to rdy_queue",
					 ctx->idx, frame_buffer->status,
					 dstbuf->b.v4l2_buf.index);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->b);
			dstbuf->queued_in_vb2 = true;
		} else {
			mtk_v4l2_debug(2,
					"[%d]status=%x err queue idx=%d %d %d",
					ctx->idx, frame_buffer->status,
					dstbuf->b.v4l2_buf.index,
					dstbuf->queued_in_vb2,
					dstbuf->queued_in_v4l2);
		}
		dstbuf->used = false;
	}
	mutex_unlock(&dstbuf->lock);
	return &dstbuf->b;
}

static void clean_display_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *framptr;

	do {
		framptr = get_display_buffer(ctx);
	} while (framptr);
}

static void clean_free_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *framptr;

	do {
		framptr = get_free_buffer(ctx);
	} while (framptr);
}

static int mtk_vdec_queue_res_chg_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
		V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->idx);
	v4l2_event_queue_fh(&ctx->fh, &ev_src_ch);

	return 0;
}

static void mtk_vdec_flush_decoder(struct mtk_vcodec_ctx *ctx)
{
	bool res_chg;
	int ret = 0;

	mtk_v4l2_debug(1, "[%d]", ctx->idx);

	ret = vdec_if_decode(ctx, NULL, NULL, &res_chg);
	if (ret)
		mtk_v4l2_err("DecodeFinal failed");

	clean_display_buffer(ctx);
	clean_free_buffer(ctx);
	ctx->state = MTK_STATE_FLUSH;
}

static bool mtk_vdec_check_res_change(struct mtk_vcodec_ctx *ctx)
{
	int dpbsize;

	if (0 != vdec_if_get_param(ctx,
				   GET_PARAM_PIC_INFO,
				   &ctx->last_decoded_picinfo)) {
		mtk_v4l2_err("[%d]Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
				ctx->idx);
		return false;
	}

	mtk_v4l2_debug(1,
			 "[%d]->new(%d,%d), old(%d,%d), real(%d,%d)\n",
			 ctx->idx, ctx->last_decoded_picinfo.pic_w,
			 ctx->last_decoded_picinfo.pic_h,
			 ctx->picinfo.pic_w, ctx->picinfo.pic_h,
			 ctx->last_decoded_picinfo.buf_w,
			 ctx->last_decoded_picinfo.buf_h);
	if (ctx->last_decoded_picinfo.pic_w == 0 ||
		ctx->last_decoded_picinfo.pic_h == 0 ||
		ctx->last_decoded_picinfo.buf_w == 0 ||
		ctx->last_decoded_picinfo.buf_h == 0)
		return false;

	if ((ctx->last_decoded_picinfo.pic_w != ctx->picinfo.pic_w) ||
	    (ctx->last_decoded_picinfo.pic_h != ctx->picinfo.pic_h)) {
		ctx->state = MTK_STATE_RES_CHANGE;

		mtk_v4l2_debug(1,
				"[%d]-> new(%d,%d), old(%d,%d), real(%d,%d)\n",
				 ctx->idx, ctx->last_decoded_picinfo.pic_w,
				 ctx->last_decoded_picinfo.pic_h,
				 ctx->picinfo.pic_w, ctx->picinfo.pic_h,
				 ctx->last_decoded_picinfo.buf_w,
				 ctx->last_decoded_picinfo.buf_h);

		vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
		if (dpbsize == 0)
			dpbsize = 1;
		ctx->dpb_count = dpbsize;

		return true;
	}

	return false;
}

void mtk_vdec_worker(struct work_struct *work)
{
	struct mtk_vcodec_ctx *ctx = container_of(work, struct mtk_vcodec_ctx,
				    decode_work);
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct vb2_buffer *src_buf, *dst_buf;
	struct mtk_vcodec_mem buf;
	struct vdec_fb *pfb;
	bool res_chg = false;
	int ret;
	struct mtk_video_buf *dst_buf_info, *src_buf_info;

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_debug(1, "[%d] [MTK_INST_ABORT]", ctx->idx);
		ctx->int_cond = 1;
		ctx->int_type = MTK_INST_WORK_THREAD_ABORT_DONE;
		wake_up_interruptible(&ctx->queue);
		return;
	}

	if (ctx->state != MTK_STATE_HEADER) {
		mtk_v4l2_debug(1, "[%d] %d", ctx->idx, ctx->state);
		return;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf == NULL) {
		mtk_v4l2_debug(1, "[%d] src_buf empty!!", ctx->idx);
		return;
	}

	dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_buf == NULL) {
		mtk_v4l2_debug(1, "[%d] dst_buf empty!!", ctx->idx);
		return;
	}

	src_buf_info = container_of(src_buf, struct mtk_video_buf, b);
	dst_buf_info = container_of(dst_buf, struct mtk_video_buf, b);

	buf.va = vb2_plane_vaddr(src_buf, 0);
	buf.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	buf.size = (unsigned int)src_buf->v4l2_planes[0].bytesused;
	if (!buf.va) {
		mtk_v4l2_err("[%d] idx=%d src_addr is NULL!!",
			       ctx->idx, src_buf->v4l2_buf.index);
		return;
	}

	pfb = &dst_buf_info->frame_buffer;
	pfb->base_y.va = vb2_plane_vaddr(dst_buf, 0);
	pfb->base_y.dma_addr = mtk_dma_sg_plane_dma_addr(dst_buf, 0);
	pfb->base_y.size = ctx->picinfo.y_bs_sz + ctx->picinfo.y_len_sz;

	pfb->base_c.va = vb2_plane_vaddr(dst_buf, 1);
	pfb->base_c.dma_addr = mtk_dma_sg_plane_dma_addr(dst_buf, 1);
	pfb->base_c.size = ctx->picinfo.c_bs_sz + ctx->picinfo.c_len_sz;
	pfb->status = 0;
	mtk_v4l2_debug(3, "===>[%d] vdec_if_decode() ===>", ctx->idx);
	mtk_v4l2_debug(3,
			"[%d] Bitstream VA=%p DMA=%llx Size=0x%zx vb=%p",
			ctx->idx, buf.va,
			(u64)buf.dma_addr,
			buf.size, src_buf);

	mtk_v4l2_debug(3,
			"idx=%d Framebuf  pfb=%p VA=%p Y_DMA=%llx C_DMA=%llx Size=0x%zx",
			dst_buf->v4l2_buf.index, pfb,
			pfb->base_y.va, (u64)pfb->base_y.dma_addr,
			(u64)pfb->base_c.dma_addr, pfb->base_y.size);

	if (src_buf_info->lastframe == true) {
		ret = vdec_if_decode(ctx, NULL, NULL, &res_chg);

		if (ret)
			mtk_v4l2_err(
				" <===[%d] lastframe src_buf[%d] dst_buf[%d] vdec_if_decode() ret=%d res_chg=%d===>",
				ctx->idx,
				src_buf->v4l2_buf.index,
				dst_buf->v4l2_buf.index,
				ret, res_chg);

		/* update src buf status */
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		src_buf_info->lastframe = false;
		v4l2_m2m_buf_done(&src_buf_info->b, VB2_BUF_STATE_DONE);

		/* update dst buf status */
		dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		dst_buf_info->used = false;

		clean_display_buffer(ctx);
		vb2_set_plane_payload(&dst_buf_info->b, 0, 0);
		vb2_set_plane_payload(&dst_buf_info->b, 1, 0);
		v4l2_m2m_buf_done(&dst_buf_info->b, VB2_BUF_STATE_DONE);
		clean_free_buffer(ctx);

		ctx->state = MTK_STATE_FLUSH;
	} else {

		dst_buf_info->b.v4l2_buf.timestamp
					= src_buf->v4l2_buf.timestamp;
		dst_buf_info->b.v4l2_buf.timecode
					= src_buf->v4l2_buf.timecode;
		mutex_lock(&dst_buf_info->lock);
		dst_buf_info->used = true;
		mutex_unlock(&dst_buf_info->lock);
		src_buf_info->used = true;

		ret = vdec_if_decode(ctx, &buf, pfb, &res_chg);
		if (ret)
			mtk_v4l2_err(
				" <===[%d] src_buf[%d]%d sz=0x%zx pts=(%lu %lu) dst_buf[%d] vdec_if_decode() ret=%d res_chg=%d===>",
				ctx->idx,
				src_buf->v4l2_buf.index,
				src_buf_info->lastframe,
				buf.size,
				src_buf->v4l2_buf.timestamp.tv_sec,
				src_buf->v4l2_buf.timestamp.tv_usec,
				dst_buf->v4l2_buf.index,
				ret, res_chg);

		if (res_chg == false) {
			src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
			v4l2_m2m_buf_done(&src_buf_info->b, VB2_BUF_STATE_DONE);
		}
		dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
			clean_display_buffer(ctx);
			clean_free_buffer(ctx);

		if (ret == 0) {
			if (res_chg == true) {
				mtk_vdec_check_res_change(ctx);
				mtk_vdec_flush_decoder(ctx);
				mtk_vdec_queue_res_chg_event(ctx);
			}
		}
	}
}

int mtk_vdec_unlock(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(3, "[%d]", ctx->idx);
	ctx->dev->curr_ctx = -1;
	mutex_unlock(&ctx->dev->dec_mutex);
	return 0;
}

int mtk_vdec_lock(struct mtk_vcodec_ctx *ctx)
{
	mtk_v4l2_debug(3, "[%d]", ctx->idx);
	mutex_lock(&ctx->dev->dec_mutex);
	ctx->dev->curr_ctx = ctx->idx;
	return 0;
}

int m2mctx_vdec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq)
{
	struct mtk_vcodec_ctx *ctx = priv;
	int ret = 0;

	mtk_v4l2_debug(3, "[%d]", ctx->idx);

	src_vq->type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	src_vq->drv_priv	= ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_video_buf);
	src_vq->ops		= &mtk_vdec_vb2_ops;
	src_vq->mem_ops		= &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->vb2_mutex;

	ret = vb2_queue_init(src_vq);
	if (ret) {
		mtk_v4l2_err("Failed to initialize videobuf2 queue(output)");
		return ret;
	}
	dst_vq->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	dst_vq->drv_priv	= ctx;
	dst_vq->buf_struct_size = sizeof(struct mtk_video_buf);
	dst_vq->ops		= &mtk_vdec_vb2_ops;
	dst_vq->mem_ops		= &mtk_dma_sg_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->vb2_mutex;

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		mtk_v4l2_err("Failed to initialize videobuf2 queue(capture)");
	}

	return ret;
}

void mtk_vcodec_vdec_release(struct mtk_vcodec_ctx *ctx)
{
	vdec_if_release(ctx);
	ctx->state = MTK_STATE_FREE;
}

void mtk_vcodec_dec_set_default_params(struct mtk_vcodec_ctx *ctx)
{
	INIT_WORK(&ctx->decode_work, mtk_vdec_worker);
}

static int vidioc_vdec_streamon(struct file *file, void *priv,
				enum v4l2_buf_type type)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	mtk_v4l2_debug(3, "[%d] (%d)", ctx->idx, type);

	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int vidioc_vdec_streamoff(struct file *file, void *priv,
				 enum v4l2_buf_type type)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	mtk_v4l2_debug(3, "[%d] (%d)", ctx->idx, type);
	return v4l2_m2m_streamoff(file, ctx->m2m_ctx, type);
}

static int vidioc_vdec_reqbufs(struct file *file, void *priv,
			       struct v4l2_requestbuffers *reqbufs)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;

	mtk_v4l2_debug(3, "[%d] (%d) count=%d", ctx->idx,
			 reqbufs->type, reqbufs->count);

	ret = v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);

	mtk_v4l2_debug(1, "[%d] (%d) count=%d ret=%d",
			 ctx->idx, reqbufs->type, reqbufs->count, ret);

	return ret;
}

static int vidioc_vdec_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	mtk_v4l2_debug(3, "[%d] (%d) id=%d", ctx->idx,
			 buf->type, buf->index);
	return v4l2_m2m_querybuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_vdec_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *buf)
{
	int ret;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_video_buf *mtkbuf;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, buf->type);
	vb = vq->bufs[buf->index];
	mtkbuf = container_of(vb, struct mtk_video_buf, b);

	mtk_v4l2_debug(2, "[%d] (%d) id=%d (%d,%d, %d) ",
			 ctx->idx, buf->type, buf->index, buf->bytesused,
			 buf->m.planes[0].bytesused, buf->length);

	if ((buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    (buf->m.planes[0].bytesused == 0)) {
		mtkbuf->lastframe = true;
		mtk_v4l2_debug(1, "[%d] (%d) id=%d lastframe=%d (%d,%d, %d) vb=%p",
			 ctx->idx, buf->type, buf->index,
			 mtkbuf->lastframe, buf->bytesused,
			 buf->m.planes[0].bytesused, buf->length,
			 vb);
	}

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);

	if (ret && (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
		unsigned int i;

		for (i = 0;
		     i < ctx->q_data[MTK_Q_DATA_DST].fmt->num_planes;
		     i++) {
			mtk_v4l2_debug(1,
					 "[%d] (%d) id=%d, plane=%d, %d, %d, %d",
					 ctx->idx,
					 buf->type, buf->index, i,
					 buf->m.planes[i].bytesused,
					 buf->length,
					 buf->m.planes[i].data_offset);
		}
	}

	return ret;
}

static int vidioc_vdec_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;

	ret = v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);

	if ((!ret) && (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
		mtk_v4l2_debug(2,
				"[%d] id=%d, byteused=%d,%d timestamp=%lu!!",
				ctx->idx, buf->index,
				buf->m.planes[0].bytesused,
				buf->m.planes[1].bytesused,
				buf->timestamp.tv_sec);
	}

	return ret;
}

static int vidioc_vdec_expbuf(struct file *file, void *priv,
			      struct v4l2_exportbuffer *eb)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	return v4l2_m2m_expbuf(file, ctx->m2m_ctx, eb);
}

static int vidioc_vdec_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	strlcpy(cap->driver, MTK_VCODEC_DEC_NAME, sizeof(cap->driver));
	cap->bus_info[0] = 0;
	/*
	 * This is only a mem-to-mem video device. The capture and output
	 * device capability flags are left only for backward compatibility
	 * and are scheduled for removal.
	 */
	cap->capabilities = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING |
			    V4L2_CAP_VIDEO_CAPTURE_MPLANE |
			    V4L2_CAP_VIDEO_OUTPUT_MPLANE;
	return 0;
}

static int vidioc_vdec_subscribe_evt(struct v4l2_fh *fh,
				     const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	default:
		return -EINVAL;
	}
}

static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	char str[10];

	mtk_v4l2_debug(3, "Type is %d", f->type);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = mtk_vdec_find_format(f, MTK_FMT_DEC);
		if (!fmt) {
			mtk_vcodec_fmt2str(f->fmt.pix_mp.pixelformat, str);
			mtk_v4l2_err("Unsupported format for source fmt=%s.",
				       str);
			return -EINVAL;
		}

		if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
			mtk_v4l2_err("sizeimage of output format must be given");
			return -EINVAL;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = mtk_vdec_find_format(f, MTK_FMT_FRAME);
		if (!fmt) {
			mtk_vcodec_fmt2str(f->fmt.pix_mp.pixelformat, str);
			mtk_v4l2_err("Unsupported format for destination fmt=%s.",
				       str);
			return -EINVAL;
		}
	}

	return 0;
}

static int vidioc_g_crop(struct file *file, void *priv,
			 struct v4l2_crop *cr)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	mtk_v4l2_debug(3, "[%d]", ctx->idx);

	if (ctx->state < MTK_STATE_HEADER) {
		mtk_v4l2_err("Cannont set crop");
		return -EINVAL;
	}

	if ((ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H264) ||
	    (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP8) ||
	    (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP9) ||
	    (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG4)) {
		char str[10];

		if (0 != vdec_if_get_param(ctx,
					   GET_PARAM_CROP_INFO,
					   cr)) {
			mtk_v4l2_debug(2,
					 "[%d]Error!! Cannot get param : GET_PARAM_CROP_INFO ERR",
					 ctx->idx);
			cr->c.left = 0;
			cr->c.top = 0;
			cr->c.width = ctx->picinfo.buf_w;
			cr->c.height = ctx->picinfo.buf_h;
		}
		memset(str, 0, sizeof(str));
		mtk_vcodec_fmt2str(ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc,
				   str);
		mtk_v4l2_debug(2, "[%d] src fmt=%s", ctx->idx, str);
		mtk_v4l2_debug(2, "Cropping info: l=%d t=%d w=%d h=%d",
				 cr->c.left, cr->c.top, cr->c.width,
				 cr->c.height);
	} else {
		cr->c.left = 0;
		cr->c.top = 0;
		cr->c.width = ctx->picinfo.pic_w;
		cr->c.height = ctx->picinfo.pic_h;
		mtk_v4l2_debug(2, "Cropping info: w=%d h=%d fw=%d fh=%d",
				 cr->c.width, cr->c.height, ctx->picinfo.buf_w,
				 ctx->picinfo.buf_h);
	}
	return 0;
}

static int vidioc_vdec_s_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp;
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	int ret = 0;

	mtk_v4l2_debug(3, "[%d]", ctx->idx);

	ret = vidioc_try_fmt(file, priv, f);
	if (ret) {
		mtk_v4l2_err("vidioc_try_fmt fail");
		return ret;
	}

	pix_mp = &f->fmt.pix_mp;

	if ((f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->out_q_ctx.q)) {
		mtk_v4l2_err("out_q_ctx buffers already requested");
		ret = -EBUSY;
	}

	if ((f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
	    vb2_is_busy(&ctx->m2m_ctx->cap_q_ctx.q)) {
		mtk_v4l2_err("cap_q_ctx buffers already requested");
		ret = -EBUSY;
	}

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_vdec_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		char str[10];

		q_data->fmt = mtk_vdec_find_format(f, MTK_FMT_DEC);
		q_data->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
		mtk_vcodec_fmt2str(f->fmt.pix_mp.pixelformat, str);
		mtk_v4l2_debug(2,
				 "[%d] type=%d wxh=%dx%d fmt=%s sz[0]=0x%x",
				 ctx->idx, f->type, pix_mp->width,
				 pix_mp->height, str, q_data->sizeimage[0]);

		pix_mp->plane_fmt[0].bytesperline = 0;
		pix_mp->width = 0;
		pix_mp->height = 0;

		if (ctx->state == MTK_STATE_FREE) {
			ret = vdec_if_create(ctx, q_data->fmt->fourcc);
		if (ret) {
			mtk_v4l2_err("[%d]: vdec_if_create() fail ret=%d",
				       ctx->idx, ret);
			return -EINVAL;
		}
			ctx->state = MTK_STATE_INIT;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		char str[10];

		q_data->fmt = mtk_vdec_find_format(f, MTK_FMT_FRAME);
		mtk_vcodec_fmt2str(f->fmt.pix_mp.pixelformat, str);
		mtk_v4l2_debug(2, "[%d] type=%d wxh=%dx%d fmt=%s",
				 ctx->idx, f->type,
				 pix_mp->width, pix_mp->height, str);
	}

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *priv,
				struct v4l2_frmsizeenum *fsize)
{
	int i = 0;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	for (i = 0; i < NUM_SUPPORTED_FRAMESIZE; ++i) {
		if (fsize->pixel_format != mtk_vdec_framesizes[i].fourcc)
			continue;

		if (!fsize->index) {
			fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
			fsize->stepwise = mtk_vdec_framesizes[i].stepwise;
			if (!(ctx->dev->dec_capability & VCODEC_CAPABILITY_4K_DISABLED)) {
				mtk_v4l2_debug(0, "4K is enabled\n");
				fsize->stepwise.max_width = VCODEC_DEC_4K_CODED_WIDTH;
				fsize->stepwise.max_height = VCODEC_DEC_4K_CODED_HEIGHT;
			}
			mtk_v4l2_debug(0, "%x, %d %d %d %d %d %d",
					ctx->dev->dec_capability,
					fsize->stepwise.min_width,
					fsize->stepwise.max_width,
					fsize->stepwise.step_width,
					fsize->stepwise.min_height,
					fsize->stepwise.max_height,
					fsize->stepwise.step_height);
			return 0;
		}
	}

	return -EINVAL;

}

static int vidioc_enum_fmt(struct file *file, struct v4l2_fmtdesc *f,
			   bool out)
{
	struct mtk_video_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < NUM_FORMATS; i++) {
		if ((out == true) && (mtk_video_formats[i].type != MTK_FMT_DEC))
			continue;
		else if ((out == false) &&
			 (mtk_video_formats[i].type != MTK_FMT_FRAME))
			continue;

		if (j == f->index)
			break;
		++j;
	}

	if (i == NUM_FORMATS)
		return -EINVAL;

	fmt = &mtk_video_formats[i];

	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	f->flags = 0;
	if (mtk_video_formats[i].type != MTK_FMT_DEC)
		f->flags |= V4L2_FMT_FLAG_COMPRESSED;

	mtk_v4l2_debug(1, "f->index=%d i=%d fmt->name=%s",
			f->index, i, fmt->name);
	return 0;
}

static int vidioc_vdec_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(file, f, false);
}

static int vidioc_vdec_enum_fmt_vid_out_mplane(struct file *file, void *priv,
					       struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(file, f, true);
}

static int vidioc_vdec_g_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	char str[10];

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		mtk_v4l2_err("no vb2 queue for type=%d",
					   f->type);
		return -EINVAL;
	}

	q_data = mtk_vdec_get_q_data(ctx, f->type);

	if ((f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
	    (ctx->state >= MTK_STATE_HEADER)) {
		/* previous decode might have resolution change that make
		 * picinfo changed update context'x picinfo here to make sure
		 * we know what report to user space */
		memcpy(&ctx->picinfo, &ctx->last_decoded_picinfo,
			sizeof(struct vdec_pic_info));
		q_data->sizeimage[0] = ctx->picinfo.y_bs_sz +
					ctx->picinfo.y_len_sz;
		q_data->sizeimage[1] = ctx->picinfo.c_bs_sz +
					ctx->picinfo.c_len_sz;
		q_data->bytesperline[0] = ctx->last_decoded_picinfo.buf_w;
		q_data->bytesperline[1] = ctx->last_decoded_picinfo.buf_w;

		/*
		 * Width and height are set to the dimensions
		 * of the movie, the buffer is bigger and
		 * further processing stages should crop to this
		 * rectangle.
		 */
		pix_mp->width = ctx->picinfo.buf_w;
		pix_mp->height = ctx->picinfo.buf_h;
		pix_mp->field = V4L2_FIELD_NONE;

		/*
		 * Set pixelformat to the format in which mt vcodec
		 * outputs the decoded frame
		 */
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->plane_fmt[1].bytesperline = q_data->bytesperline[1];
		pix_mp->plane_fmt[1].sizeimage = q_data->sizeimage[1];

	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/*
		 * This is run on OUTPUT
		 * The buffer contains compressed image
		 * so width and height have no meaning
		 */
		pix_mp->width = 0;
		pix_mp->height = 0;
		pix_mp->field = V4L2_FIELD_NONE;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->num_planes = q_data->fmt->num_planes;
	} else {
		mtk_v4l2_debug(0, "[%d] state=%d Format could not be read %d, not ready yet!",
				ctx->idx, ctx->state, f->type);
		return -EINVAL;
	}

	mtk_vcodec_fmt2str(f->fmt.pix_mp.pixelformat, str);
	mtk_v4l2_debug(2,
			 "[%d] type=%d wxh=%dx%d fmt=%s pix->num_planes=%d, sz[0]=0x%x sz[1]=0x%x",
			 ctx->idx, f->type, pix_mp->width, pix_mp->height, str,
			 pix_mp->num_planes, pix_mp->plane_fmt[0].sizeimage,
			 pix_mp->plane_fmt[1].sizeimage);

	return 0;
}

static int vidioc_vdec_g_ctrl(struct file *file, void *fh,
			      struct v4l2_control *ctrl)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(fh);
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= MTK_STATE_HEADER) {
			ctrl->value = ctx->dpb_count;
		} else {
			mtk_v4l2_err("Decoding not initialised");
			return -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int vb2ops_vdec_queue_setup(struct vb2_queue *vq,
				   const struct v4l2_format *fmt,
				   unsigned int *nbuffers,
				   unsigned int *nplanes,
				   unsigned int sizes[], void *alloc_ctxs[])
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_q_data *q_data;

	q_data = mtk_vdec_get_q_data(ctx, vq->type);

	switch (vq->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*nplanes = 1;
		if (*nbuffers < 1)
			*nbuffers = 1;
		if (*nbuffers > MTK_VIDEO_MAX_FRAME)
			*nbuffers = MTK_VIDEO_MAX_FRAME;

		sizes[0] = q_data->sizeimage[0];
		alloc_ctxs[0] = ctx->dev->alloc_ctx;
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (ctx->state < MTK_STATE_HEADER)
			return -EINVAL;
		/*
		 * Output plane count is 2 - one for Y and one for CbCr
		 */
		*nplanes = 2;
		if (*nbuffers < ctx->dpb_count)
			*nbuffers = ctx->dpb_count;
		if (*nbuffers > MTK_VIDEO_MAX_FRAME)
			*nbuffers = MTK_VIDEO_MAX_FRAME;

		mtk_v4l2_debug(1,
				 "[%d]\t get %d plane(s), ctx->dpb_count = %d, %d buffer(s)",
				 ctx->idx, *nplanes, ctx->dpb_count, *nbuffers);

		sizes[0] = q_data->sizeimage[0];
		sizes[1] = q_data->sizeimage[1];
		alloc_ctxs[0] = ctx->dev->alloc_ctx;
		alloc_ctxs[1] = ctx->dev->alloc_ctx;
		break;
	default:
		break;
	}

	mtk_v4l2_debug(1,
			"[%d]\t type = %d, get %d plane(s), %d buffer(s) of size 0x%x 0x%x ",
			ctx->idx, vq->type, *nplanes, *nbuffers,
			sizes[0], sizes[1]);

	return 0;
}

static int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_q_data *q_data;
	int i;

	mtk_v4l2_debug(3, "[%d] (%d) id=%d",
			 ctx->idx, vb->vb2_queue->type, vb->v4l2_buf.index);

	q_data = mtk_vdec_get_q_data(ctx, vb->vb2_queue->type);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) > q_data->sizeimage[i]) {
			mtk_v4l2_err("data will not fit into plane %d (%lu < %d)",
				       i, vb2_plane_size(vb, i),
				       q_data->sizeimage[i]);
		}
	}

	return 0;
}

static void vb2ops_vdec_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_video_buf *buf = container_of(vb, struct mtk_video_buf, b);

	mtk_v4l2_debug(3, "[%d] (%d) id=%d, vb=%p",
			 ctx->idx, vb->vb2_queue->type,
			 vb->v4l2_buf.index, vb);
	/*
	 * check if this buffer is ready to be used after decode
	 */
	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		struct vb2_buffer *src_buf;
		struct mtk_vcodec_mem buf;
		int ret = 0;
		int dpbsize = 1;

		v4l2_m2m_buf_queue(ctx->m2m_ctx, vb);
		if (ctx->state == MTK_STATE_INIT) {
			src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
			if (!src_buf) {
				mtk_v4l2_err("No src buffer");
				return;
			}

			buf.va	= vb2_plane_vaddr(src_buf, 0);
			buf.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
			buf.size = (unsigned int)src_buf->v4l2_planes[0].bytesused;
			mtk_v4l2_debug(2,
					"[%d] buf idx=%d va=%p dma=%llx size=0x%zu",
					ctx->idx, src_buf->v4l2_buf.index,
					buf.va, (u64)buf.dma_addr, buf.size);

			ret = vdec_if_init(ctx, &buf, &ctx->picinfo);
			if (ret) {
				src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
				v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
				mtk_v4l2_err("[%d] vdec_if_init() src_buf=%d, size=0x%zu, fail=%d",
						ctx->idx, src_buf->v4l2_buf.index,
						buf.size, ret);
				return;
			}

			memcpy(&ctx->last_decoded_picinfo, &ctx->picinfo,
				sizeof(struct vdec_pic_info));
			ctx->q_data[MTK_Q_DATA_DST].sizeimage[0] =
						ctx->picinfo.y_bs_sz +
						ctx->picinfo.y_len_sz;
			ctx->q_data[MTK_Q_DATA_DST].bytesperline[0] =
							ctx->picinfo.buf_w;
			ctx->q_data[MTK_Q_DATA_DST].sizeimage[1] =
						ctx->picinfo.c_bs_sz +
						ctx->picinfo.c_len_sz;
			ctx->q_data[MTK_Q_DATA_DST].bytesperline[1] =
							ctx->picinfo.buf_w;
			mtk_v4l2_debug(2, "[%d] vdec_if_init() OK wxh=%dx%d pic wxh=%dx%d sz[0]=0x%x sz[1]=0x%x",
					 ctx->idx,
					 ctx->picinfo.buf_w, ctx->picinfo.buf_h,
					 ctx->picinfo.pic_w, ctx->picinfo.pic_h,
					 ctx->q_data[MTK_Q_DATA_DST].sizeimage[0],
					 ctx->q_data[MTK_Q_DATA_DST].sizeimage[1]);

			vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
			if (dpbsize == 0) {
				mtk_v4l2_debug(2,
						 "[%d] vdec_if_get_param()  GET_PARAM_DPB_SIZE fail=%d",
						 ctx->idx, ret);
				dpbsize = 1;
			}
			ctx->dpb_count = dpbsize;
			ctx->state = MTK_STATE_HEADER;
			mtk_v4l2_debug(1, "[%d] state=%d dpbsize=%d",
					ctx->idx, ctx->state, ctx->dpb_count);
		} else {
			mtk_v4l2_debug(1, "[%d] already init driver %d",
					 ctx->idx, ctx->state);
		}

	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		mutex_lock(&buf->lock);
		if (buf->used == false) {
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb);
			buf->queued_in_vb2 = true;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;
		} else {
			buf->queued_in_vb2 = false;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;
		}
		mutex_unlock(&buf->lock);
	} else {
		mtk_v4l2_err("[%d] (%d) id=%d", ctx->idx,
			       vb->vb2_queue->type, vb->v4l2_buf.index);
	}
}

static void vb2ops_vdec_buf_finish(struct vb2_buffer *vb)
{
	if (vb->vb2_queue->type ==
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		struct mtk_video_buf *buf;

		buf = container_of(vb, struct mtk_video_buf, b);
		mutex_lock(&buf->lock);
		buf->queued_in_v4l2 = false;
		buf->queued_in_vb2 = false;
		mutex_unlock(&buf->lock);
	}
}

static int vb2ops_vdec_buf_init(struct vb2_buffer *vb)
{
	struct mtk_video_buf *buf = container_of(vb, struct mtk_video_buf, b);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->used = false;
		buf->ready_to_display = false;
		buf->nonrealdisplay = false;
		buf->queued_in_v4l2 = false;
		mutex_init(&buf->lock);
	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		buf->lastframe = false;
	} else {
		mtk_v4l2_err("vb2ops_vdec_buf_init: unknown queue type");
		return -EINVAL;
	}

	return 0;
}

static int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);

	mtk_v4l2_debug(1, "[%d] (%d) state=%d",
			 ctx->idx, q->type, ctx->state);

	if (ctx->state == MTK_STATE_FLUSH)
		ctx->state = MTK_STATE_HEADER;

	return 0;
}

static void vb2ops_vdec_stop_streaming(struct vb2_queue *q)
{
	struct vb2_buffer *src_buf = NULL, *dst_buf = NULL;
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);

	mtk_v4l2_debug(1, "[%d] (%d) state=(%x)",
					ctx->idx, q->type, ctx->state);
	mtk_v4l2_debug(2, "[%d] (%d) state=(%x) ctx->decoded_frame_cnt=%d",
			ctx->idx, q->type, ctx->state, ctx->decoded_frame_cnt);

	if (ctx->state == MTK_STATE_HEADER)  {

		ctx->state = MTK_STATE_ABORT;
		queue_work(ctx->dev->decode_workqueue, &ctx->decode_work);
		mtk_vcodec_wait_for_done_ctx(ctx,
					MTK_INST_WORK_THREAD_ABORT_DONE,
					WAIT_INTR_TIMEOUT, true);
		mtk_vdec_flush_decoder(ctx);
	}

	mtk_v4l2_debug(2, "[%d] (%d) state=(%x) ctx->decoded_frame_cnt=%d",
			ctx->idx, q->type, ctx->state, ctx->decoded_frame_cnt);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		while (dst_buf) {
			vb2_set_plane_payload(dst_buf, 0, 0);
			vb2_set_plane_payload(dst_buf, 1, 0);
			v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_ERROR);
			dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		}
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		while (src_buf) {
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_ERROR);
			src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		}
	}

}

static void m2mops_vdec_device_run(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;
	struct mtk_vcodec_dev *dev = ctx->dev;

	mtk_v4l2_debug(3, "[%d] state=%x", ctx->idx, ctx->state);

	queue_work(dev->decode_workqueue, &ctx->decode_work);
}

static int m2mops_vdec_job_ready(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mtk_v4l2_debug(3, "[%d]", ctx->idx);

	if (!v4l2_m2m_num_src_bufs_ready(ctx->m2m_ctx)) {
		mtk_v4l2_debug(3,
				 "[%d] not ready: not enough video src buffers.",
				 ctx->idx);
		return 0;
	}

	if (!v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx)) {
		mtk_v4l2_debug(3,
				 "[%d] not ready: not enough video dst buffers.",
				 ctx->idx);
		return 0;
	}

	if (ctx->state != MTK_STATE_HEADER) {
		mtk_v4l2_debug(1, "[%d] state= %x",
				ctx->idx, ctx->state);
		return 0;
	}

	mtk_v4l2_debug(3, "[%d] ready!", ctx->idx);

	return 1;
}

static void m2mops_vdec_job_abort(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;

	v4l2_m2m_job_finish(ctx->dev->m2m_dev_dec, ctx->m2m_ctx);

	mtk_v4l2_debug(3, "[%d] type=%d", ctx->idx, ctx->type);
}

static int mtk_vdec_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);

	mtk_v4l2_debug(1, "[%d]", ctx->idx);

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= MTK_STATE_HEADER) {
			ctrl->val = ctx->dpb_count;
		} else {
			mtk_v4l2_err("Seqinfo not ready");
			return -EINVAL;
		}
		break;
	default:
		mtk_v4l2_err("ctrl-id=%d not support!", ctrl->id);
		return -EINVAL;
	}
	return 0;
}

static const struct v4l2_ctrl_ops mtk_vcodec_dec_ctrl_ops = {
	.g_volatile_ctrl = mtk_vdec_g_v_ctrl,
};

int mtk_vdec_ctrls_setup(struct mtk_vcodec_ctx *ctx)
{
	int i;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, NUM_CTRLS);
	if (ctx->ctrl_hdl.error) {
		mtk_v4l2_err("Init control handler fail %d",
			       ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}

	for (i = 0; i < NUM_CTRLS; i++) {
		if ((controls[i].type == V4L2_CTRL_TYPE_MENU) ||
		    (controls[i].type == V4L2_CTRL_TYPE_INTEGER_MENU)) {
			ctx->ctrls[i] = v4l2_ctrl_new_std_menu(&ctx->ctrl_hdl,
					       &mtk_vcodec_dec_ctrl_ops,
					       controls[i].id,
					       controls[i].maximum,
					       0,
					       controls[i].default_value);
		} else {
			ctx->ctrls[i] = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
					  &mtk_vcodec_dec_ctrl_ops,
					  controls[i].id,
					  controls[i].minimum,
					  controls[i].maximum,
					  controls[i].step,
					  controls[i].default_value);
		}

		if (ctx->ctrl_hdl.error) {
			mtk_v4l2_err("Adding control (%d) failed %d",
				       i, ctx->ctrl_hdl.error);
			return ctx->ctrl_hdl.error;
		}
		if (controls[i].is_volatile && ctx->ctrls[i])
			ctx->ctrls[i]->flags |= V4L2_CTRL_FLAG_VOLATILE;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);
	return 0;
}

void mtk_vdec_ctrls_free(struct mtk_vcodec_ctx *ctx)
{
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
}

static void m2mops_vdec_lock(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mtk_v4l2_debug(3, "[%d]", ctx->idx);
	mutex_lock(&ctx->dev->dev_mutex);
}

static void m2mops_vdec_unlock(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mtk_v4l2_debug(3, "[%d]", ctx->idx);
	mutex_unlock(&ctx->dev->dev_mutex);
}

const struct v4l2_m2m_ops mtk_vdec_m2m_ops = {
	.device_run			= m2mops_vdec_device_run,
	.job_ready			= m2mops_vdec_job_ready,
	.job_abort			= m2mops_vdec_job_abort,
	.lock				= m2mops_vdec_lock,
	.unlock				= m2mops_vdec_unlock,
};

const struct vb2_ops mtk_vdec_vb2_ops = {
	.queue_setup			= vb2ops_vdec_queue_setup,
	.buf_prepare			= vb2ops_vdec_buf_prepare,
	.buf_queue			= vb2ops_vdec_buf_queue,
	.wait_prepare			= vb2_ops_wait_prepare,
	.wait_finish			= vb2_ops_wait_finish,
	.buf_init			= vb2ops_vdec_buf_init,
	.buf_finish			= vb2ops_vdec_buf_finish,
	.start_streaming		= vb2ops_vdec_start_streaming,
	.stop_streaming			= vb2ops_vdec_stop_streaming,
};

const struct v4l2_ioctl_ops mtk_vdec_ioctl_ops = {
	.vidioc_streamon		= vidioc_vdec_streamon,
	.vidioc_streamoff		= vidioc_vdec_streamoff,

	.vidioc_reqbufs			= vidioc_vdec_reqbufs,
	.vidioc_querybuf		= vidioc_vdec_querybuf,
	.vidioc_qbuf			= vidioc_vdec_qbuf,
	.vidioc_expbuf			= vidioc_vdec_expbuf,
	.vidioc_dqbuf			= vidioc_vdec_dqbuf,

	.vidioc_s_fmt_vid_cap_mplane	= vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= vidioc_vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane	= vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= vidioc_vdec_g_fmt,

	.vidioc_enum_fmt_vid_cap_mplane = vidioc_vdec_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,

	.vidioc_g_ctrl			= vidioc_vdec_g_ctrl,
	.vidioc_querycap		= vidioc_vdec_querycap,
	.vidioc_subscribe_event		= vidioc_vdec_subscribe_evt,
	.vidioc_g_crop			= vidioc_g_crop,
};
