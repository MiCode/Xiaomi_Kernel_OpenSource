/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
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

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_drv_if.h"
#include "mtk_vcodec_dec_pm.h"

static const struct mtk_video_fmt mtk_video_formats[] = {
	{
		.fourcc = V4L2_PIX_FMT_H264,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_VP8,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.type = MTK_FMT_DEC,
		.num_planes = 1,
	},
	{
		.fourcc = V4L2_PIX_FMT_MT21C,
		.type = MTK_FMT_FRAME,
		.num_planes = 2,
	},
};

#define NUM_FORMATS ARRAY_SIZE(mtk_video_formats)
#define DEFAULT_OUT_FMT_IDX	0
#define DEFAULT_CAP_FMT_IDX	3

static const struct mtk_codec_framesizes mtk_vdec_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc	= V4L2_PIX_FMT_VP8,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
	{
		.fourcc = V4L2_PIX_FMT_VP9,
		.stepwise = {  MTK_VDEC_MIN_W, MTK_VDEC_MAX_W, 16,
				MTK_VDEC_MIN_H, MTK_VDEC_MAX_H, 16 },
	},
};

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(mtk_vdec_framesizes)

/*
 * This function tries to clean all display buffers, the buffers will return
 * in display order.
 * Note the buffers returned from codec driver may still be in driver's
 * reference list.
 */
static struct vb2_buffer *get_display_buffer(struct mtk_vcodec_ctx *ctx,
		bool got_early_eos)
{
	struct vdec_fb *disp_frame_buffer = NULL;
	struct mtk_video_dec_buf *dstbuf;
	const struct mtk_video_fmt	*fmt;
	int i = 0;

	mtk_v4l2_debug(3, "[%d]", ctx->id);
	if (vdec_if_get_param(ctx,
			GET_PARAM_DISP_FRAME_BUFFER,
			&disp_frame_buffer)) {
		mtk_v4l2_err("[%d]Cannot get param : GET_PARAM_DISP_FRAME_BUFFER",
			ctx->id);
		return NULL;
	}

	if (disp_frame_buffer == NULL) {
		mtk_v4l2_debug(3, "No display frame buffer");
		return NULL;
	}

	dstbuf = container_of(disp_frame_buffer, struct mtk_video_dec_buf,
				frame_buffer);
	mutex_lock(&ctx->lock);
	if (dstbuf->used) {
		fmt = ctx->q_data[MTK_Q_DATA_DST].fmt;
		for (i = 0; i < fmt->num_planes; i++) {
			vb2_set_plane_payload(&dstbuf->vb.vb2_buf, i,
						ctx->picinfo.fb_sz[i]);
		}

		if (got_early_eos &&
			dstbuf->vb.vb2_buf.timestamp == ctx->input_max_ts)
			dstbuf->vb.flags |= V4L2_BUF_FLAG_LAST;

		mtk_v4l2_debug(2,
				"[%d]status=%x queue id=%d to done_list %d, %d, %x",
				ctx->id, disp_frame_buffer->status,
				dstbuf->vb.vb2_buf.index,
				dstbuf->queued_in_vb2, got_early_eos,
				dstbuf->vb.flags);

		v4l2_m2m_buf_done(&dstbuf->vb, VB2_BUF_STATE_DONE);
		ctx->decoded_frame_cnt++;
	}
	mutex_unlock(&ctx->lock);
	return &dstbuf->vb.vb2_buf;
}

/*
 * This function tries to clean all capture buffers that are not used as
 * reference buffers by codec driver any more
 * In this case, we need re-queue buffer to vb2 buffer if user space
 * already returns this buffer to v4l2 or this buffer is just the output of
 * previous sps/pps/resolution change decode, or do nothing if user
 * space still owns this buffer
 */
static struct vb2_buffer *get_free_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_video_dec_buf *dstbuf;
	struct vdec_fb *free_frame_buffer = NULL;

	if (vdec_if_get_param(ctx,
				GET_PARAM_FREE_FRAME_BUFFER,
				&free_frame_buffer)) {
		mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
		return NULL;
	}
	if (free_frame_buffer == NULL) {
		mtk_v4l2_debug(3, " No free frame buffer");
		return NULL;
	}

	mtk_v4l2_debug(3, "[%d] tmp_frame_addr = 0x%p",
			ctx->id, free_frame_buffer);

	dstbuf = container_of(free_frame_buffer, struct mtk_video_dec_buf,
				frame_buffer);

	mutex_lock(&ctx->lock);
	if (dstbuf->used) {
		if ((dstbuf->queued_in_vb2) &&
		    (dstbuf->queued_in_v4l2) &&
		    (free_frame_buffer->status == FB_ST_FREE)) {
			/*
			 * After decode sps/pps or non-display buffer, we don't
			 * need to return capture buffer to user space, but
			 * just re-queue this capture buffer to vb2 queue.
			 * This reduce overheads that dq/q unused capture
			 * buffer. In this case, queued_in_vb2 = true.
			 */
			mtk_v4l2_debug(2,
				"[%d]status=%x queue id=%d to rdy_queue %d",
				ctx->id, free_frame_buffer->status,
				dstbuf->vb.vb2_buf.index,
				dstbuf->queued_in_vb2);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->vb);
		} else if ((dstbuf->queued_in_vb2 == false) &&
			   (dstbuf->queued_in_v4l2 == true)) {
			/*
			 * If buffer in v4l2 driver but not in vb2 queue yet,
			 * and we get this buffer from free_list, it means
			 * that codec driver do not use this buffer as
			 * reference buffer anymore. We should q buffer to vb2
			 * queue, so later work thread could get this buffer
			 * for decode. In this case, queued_in_vb2 = false
			 * means this buffer is not from previous decode
			 * output.
			 */
			mtk_v4l2_debug(2,
					"[%d]status=%x queue id=%d to rdy_queue",
					ctx->id, free_frame_buffer->status,
					dstbuf->vb.vb2_buf.index);
			v4l2_m2m_buf_queue(ctx->m2m_ctx, &dstbuf->vb);
			dstbuf->queued_in_vb2 = true;
		} else {
			/*
			 * Codec driver do not need to reference this capture
			 * buffer and this buffer is not in v4l2 driver.
			 * Then we don't need to do any thing, just add log when
			 * we need to debug buffer flow.
			 * When this buffer q from user space, it could
			 * directly q to vb2 buffer
			 */
			mtk_v4l2_debug(3, "[%d]status=%x err queue id=%d %d %d",
					ctx->id, free_frame_buffer->status,
					dstbuf->vb.vb2_buf.index,
					dstbuf->queued_in_vb2,
					dstbuf->queued_in_v4l2);
		}
		dstbuf->used = false;
	}
	mutex_unlock(&ctx->lock);
	return &dstbuf->vb.vb2_buf;
}

static struct vb2_buffer *get_free_bs_buffer(struct mtk_vcodec_ctx *ctx,
	struct mtk_vcodec_mem *current_bs)
{
	struct mtk_vcodec_mem *free_bs_buffer;
	struct mtk_video_dec_buf  *srcbuf;

	if (vdec_if_get_param(ctx, GET_PARAM_FREE_BITSTREAM_BUFFER,
						  &free_bs_buffer) != 0) {
		mtk_v4l2_err("[%d] Cannot get param : GET_PARAM_FREE_BITSTREAM_BUFFER",
					 ctx->id);
		return NULL;
	}

	if (free_bs_buffer == NULL) {
		mtk_v4l2_debug(3, "No free bitstream buffer");
		return NULL;
	}

	if (current_bs == free_bs_buffer) {
		mtk_v4l2_debug(4,
			"No free bitstream buffer except current bs: %p",
			current_bs);
		return NULL;
	}

	srcbuf = container_of(free_bs_buffer,
		struct mtk_video_dec_buf, bs_buffer);
	mtk_v4l2_debug(2,
		"[%d] length=%zu size=%zu queue idx=%d",
		ctx->id, free_bs_buffer->length, free_bs_buffer->size,
		srcbuf->vb.vb2_buf.index);

	v4l2_m2m_buf_done(&srcbuf->vb, VB2_BUF_STATE_DONE);
	return &srcbuf->vb.vb2_buf;
}

static void clean_free_bs_buffer(struct mtk_vcodec_ctx *ctx,
	struct mtk_vcodec_mem *current_bs)
{
	struct vb2_buffer *framptr;

	do {
		framptr = get_free_bs_buffer(ctx, current_bs);
	} while (framptr);
}

static void clean_display_buffer(struct mtk_vcodec_ctx *ctx, bool got_early_eos)
{
	struct vb2_buffer *framptr;

	do {
		framptr = get_display_buffer(ctx, got_early_eos);
	} while (framptr);
}

static void clean_free_buffer(struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *framptr;

	do {
		framptr = get_free_buffer(ctx);
	} while (framptr);
}

static void mtk_vdec_queue_res_chg_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
		V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_src_ch);
}

static void mtk_vdec_queue_stop_play_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_eos = {
		.type = V4L2_EVENT_EOS,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_eos);
}

static void mtk_vdec_queue_error_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_eos = {
		.type = V4L2_EVENT_VDEC_ERROR,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_eos);
}
static int mtk_vdec_flush_decoder(struct mtk_vcodec_ctx *ctx)
{
	unsigned int res_chg;
	int ret = 0;

	ret = vdec_if_decode(ctx, NULL, NULL, &res_chg);
	if (ret)
		mtk_v4l2_err("DecodeFinal failed, ret=%d", ret);

	clean_free_bs_buffer(ctx, NULL);
	clean_display_buffer(ctx, 0);
	clean_free_buffer(ctx);

	return 0;
}

static void mtk_vdec_update_fmt(struct mtk_vcodec_ctx *ctx,
				unsigned int pixelformat)
{
	const struct mtk_video_fmt *fmt;
	struct mtk_q_data *dst_q_data;
	unsigned int k;

	dst_q_data = &ctx->q_data[MTK_Q_DATA_DST];
	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == pixelformat) {
			mtk_v4l2_debug(1, "Update cap fourcc(%d -> %d)",
				dst_q_data->fmt->fourcc, pixelformat);
			dst_q_data->fmt = fmt;
			return;
		}
	}

	mtk_v4l2_err("Cannot get fourcc(%d), using init value", pixelformat);
}

static int mtk_vdec_pic_info_update(struct mtk_vcodec_ctx *ctx)
{
	unsigned int dpbsize = 0;
	int ret;

	if (vdec_if_get_param(ctx,
				GET_PARAM_PIC_INFO,
				&ctx->last_decoded_picinfo)) {
		mtk_v4l2_err("[%d]Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
				ctx->id);
		return -EINVAL;
	}

	if (ctx->last_decoded_picinfo.pic_w == 0 ||
		ctx->last_decoded_picinfo.pic_h == 0 ||
		ctx->last_decoded_picinfo.buf_w == 0 ||
		ctx->last_decoded_picinfo.buf_h == 0) {
		mtk_v4l2_err("Cannot get correct pic info");
		return -EINVAL;
	}

	if (ctx->last_decoded_picinfo.cap_fourcc != ctx->picinfo.cap_fourcc &&
		ctx->picinfo.cap_fourcc != 0)
		mtk_vdec_update_fmt(ctx, ctx->picinfo.cap_fourcc);

	if ((ctx->last_decoded_picinfo.pic_w == ctx->picinfo.pic_w) ||
	    (ctx->last_decoded_picinfo.pic_h == ctx->picinfo.pic_h))
		return 0;

	mtk_v4l2_debug(1,
			"[%d]-> new(%d,%d), old(%d,%d), real(%d,%d)",
			ctx->id, ctx->last_decoded_picinfo.pic_w,
			ctx->last_decoded_picinfo.pic_h,
			ctx->picinfo.pic_w, ctx->picinfo.pic_h,
			ctx->last_decoded_picinfo.buf_w,
			ctx->last_decoded_picinfo.buf_h);

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		mtk_v4l2_err("Incorrect dpb size, ret=%d", ret);

	ctx->last_dpb_size = dpbsize;

	return ret;
}

static void mtk_vdec_worker(struct work_struct *work)
{
	struct mtk_vcodec_ctx *ctx = container_of(work, struct mtk_vcodec_ctx,
				decode_work);
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct mtk_video_dec_buf *mtk_buf;
	struct mtk_vcodec_mem *buf;
	struct vdec_fb *pfb;
	unsigned int i = 0;
	unsigned int src_chg = 0;
	bool res_chg = false;
	bool need_more_output = false;
	bool mtk_vcodec_unsupport = false;

	int ret;
	struct mtk_video_dec_buf *dst_buf_info, *src_buf_info;
	unsigned int fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	unsigned int dpbsize = 0;

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, "[%d] src_buf empty!!", ctx->id);
		return;
	}

	dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_buf == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, "[%d] dst_buf empty!!", ctx->id);
		return;
	}

	src_buf_info = container_of(src_buf, struct mtk_video_dec_buf, vb);
	dst_buf_info = container_of(dst_buf, struct mtk_video_dec_buf, vb);

	pfb = &dst_buf_info->frame_buffer;
	pfb->num_planes = dst_buf->vb2_buf.num_planes;
	pfb->index = dst_buf->vb2_buf.index;
	for (i = 0; i < dst_buf->vb2_buf.num_planes; i++) {
		pfb->fb_base[i].va = vb2_plane_vaddr(&dst_buf->vb2_buf, i);
#ifdef CONFIG_VB2_MEDIATEK_DMA_SG
		pfb->fb_base[i].dma_addr =
			mtk_dma_sg_plane_dma_addr(&dst_buf->vb2_buf, i);
#else
		pfb->fb_base[i].dma_addr =
			vb2_dma_contig_plane_dma_addr(&dst_buf->vb2_buf, i);
#endif
		pfb->fb_base[i].size = ctx->picinfo.fb_sz[i];
		pfb->fb_base[i].length = dst_buf->vb2_buf.planes[i].length;
		pfb->fb_base[i].dmabuf = dst_buf->vb2_buf.planes[i].dbuf;
		mtk_v4l2_debug(3,
				"id=%d Framebuf  pfb=%p VA=%p Y_DMA=%p Size=%zx",
				dst_buf->vb2_buf.index,
				pfb, pfb->fb_base[i].va,
				&pfb->fb_base[i].dma_addr,
				pfb->fb_base[i].size);
	}

	pfb->status = 0;
	mtk_v4l2_debug(3, "===>[%d] vdec_if_decode() ===>", ctx->id);

	if (src_buf_info->lastframe) {
		mtk_v4l2_debug(1, "Got empty flush input buffer.");
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		src_buf_info->lastframe = NON_EOS;

		/* update dst buf status */
		dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		mutex_lock(&ctx->lock);
		dst_buf_info->used = false;
		mutex_unlock(&ctx->lock);

		vdec_if_decode(ctx, NULL, NULL, &src_chg);
		clean_free_bs_buffer(ctx, NULL);
		clean_display_buffer(ctx,
			src_buf->vb2_buf.planes[0].bytesused != 0U);
		if (src_buf->vb2_buf.planes[0].bytesused == 0U) {
			src_buf->flags |= V4L2_BUF_FLAG_LAST;
			vb2_set_plane_payload(&src_buf_info->vb.vb2_buf, 0, 0);
			v4l2_m2m_buf_done(&src_buf_info->vb,
				VB2_BUF_STATE_DONE);

			for (i = 0; i < pfb->num_planes; i++)
				vb2_set_plane_payload(
					&dst_buf_info->vb.vb2_buf, i, 0);
			dst_buf->flags |= V4L2_BUF_FLAG_LAST;
			v4l2_m2m_buf_done(&dst_buf_info->vb,
				VB2_BUF_STATE_DONE);
		}

		clean_free_buffer(ctx);
		mtk_vdec_queue_stop_play_event(ctx);
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		return;
	}
	buf = &src_buf_info->bs_buffer;
	buf->va = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	buf->dma_addr = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);
	buf->size = (size_t)src_buf->vb2_buf.planes[0].bytesused;
	buf->length = (size_t)src_buf->vb2_buf.planes[0].length;
	buf->dmabuf = src_buf->vb2_buf.planes[0].dbuf;
	buf->flags = src_buf->flags;
	buf->index = src_buf->vb2_buf.index;
	if (buf->va == NULL && buf->dmabuf == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_err("[%d] id=%d src_addr is NULL!!!",
				ctx->id, src_buf->vb2_buf.index);
		return;
	}

	dst_buf->flags &= ~CROP_CHANGED;
	dst_buf->flags &= ~REF_FREED;

	mtk_v4l2_debug(3, "[%d] Bitstream VA=%p DMA=%pad Size=%zx vb=%p",
			ctx->id, buf->va, &buf->dma_addr, buf->size, src_buf);
	dst_buf_info->vb.vb2_buf.timestamp
			= src_buf_info->vb.vb2_buf.timestamp;
	dst_buf_info->vb.timecode
			= src_buf_info->vb.timecode;
	mutex_lock(&ctx->lock);
	dst_buf_info->used = true;
	mutex_unlock(&ctx->lock);
	src_buf_info->used = true;

	ret = vdec_if_decode(ctx, buf, pfb, &src_chg);
	res_chg = ((src_chg & VDEC_RES_CHANGE) != 0U) ? true : false;
	need_more_output = ((src_chg & VDEC_NEED_MORE_OUTPUT_BUF) != 0U) ?
		true : false;
	mtk_vcodec_unsupport = ((src_chg & VDEC_HW_NOT_SUPPORT) != 0) ?
		true : false;
	if (src_chg & VDEC_CROP_CHANGED)
		dst_buf_info->flags |= CROP_CHANGED;

	if (ret < 0 || mtk_vcodec_unsupport) {
		mtk_v4l2_err(
			" <===[%d], src_buf[%d] sz=0x%zx pts=%llu dst_buf[%d] vdec_if_decode() ret=%d res_chg=%d===>",
			ctx->id,
			src_buf->vb2_buf.index,
			buf->size,
			src_buf_info->vb.vb2_buf.timestamp,
			dst_buf->vb2_buf.index,
			ret, res_chg);
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		if (ret == -EIO) {
			mutex_lock(&ctx->lock);
			src_buf_info->error = true;
			mutex_unlock(&ctx->lock);
		}
		clean_free_bs_buffer(ctx, &src_buf_info->bs_buffer);
		if (mtk_vcodec_unsupport) {
			/*
			 * If cncounter the src unsupport (fatal) during play,
			 * egs: width/height, bitdepth, level, then teturn
			 * error event to user to stop play it
			 */
			mtk_v4l2_err(" <=== [%d] vcodec not support the source!===>",
				ctx->id);
			ctx->state = MTK_STATE_FLUSH;
			mtk_vdec_queue_error_event(ctx);
			v4l2_m2m_buf_done(&src_buf_info->vb,
				VB2_BUF_STATE_DONE);
		} else
			v4l2_m2m_buf_done(&src_buf_info->vb,
			VB2_BUF_STATE_ERROR);
	} else if (src_buf_info->lastframe == EOS_WITH_DATA &&
		need_more_output == false) {
		/*
		 * Getting early eos bitstream buffer, after decode this
		 * buffer, need to flush decoder. Use the flush_buf
		 * as normal EOS, and flush decoder.
		 */
		mtk_v4l2_debug(0, "[%d] EarlyEos: decode last frame %d",
			ctx->id, src_buf->planes[0].bytesused);
		src_buf->flags |= V4L2_BUF_FLAG_LAST;
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		clean_free_bs_buffer(ctx, NULL);
		mtk_buf = (struct mtk_video_dec_buf *)ctx->empty_flush_buf;
		mtk_buf->lastframe = EOS;
		v4l2_m2m_buf_queue(ctx->m2m_ctx, &ctx->empty_flush_buf->vb);
	} else if ((ret == 0) && ((fourcc == V4L2_PIX_FMT_RV40) ||
		(fourcc == V4L2_PIX_FMT_RV30) ||
		(res_chg == false && need_more_output == false))) {
		/*
		 * we only return src buffer with VB2_BUF_STATE_DONE
		 * when decode success without resolution
		 * change except rv30/rv40.
		 */
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		v4l2_m2m_buf_done(&src_buf_info->vb, VB2_BUF_STATE_DONE);
		clean_free_bs_buffer(ctx, &src_buf_info->bs_buffer);
	} else {    /* res_chg == true || need_more_output == true*/
		clean_free_bs_buffer(ctx, &src_buf_info->bs_buffer);
		mtk_v4l2_debug(1, "Need more capture buffer  r:%d n:%d\n",
			res_chg, need_more_output);
	}


	dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	clean_display_buffer(ctx, src_buf_info->lastframe == EOS_WITH_DATA);
	clean_free_buffer(ctx);

	if (!ret && res_chg) {
		if ((fourcc == V4L2_PIX_FMT_RV40) ||
			(fourcc == V4L2_PIX_FMT_RV30)) {
			/*
			 * For rv30/rv40 stream, encountering a resolution
			 * change the current frame needs to refer to the
			 * previous frame,so driver should not flush decode,
			 * but the driver should sends a
			 * V4L2_EVENT_SOURCE_CHANGE
			 * event for source change to app.
			 * app should set new crop to mdp directly.
			 */
			mtk_v4l2_debug(0, "RV30/RV40 RPR res_chg:%d\n",
				res_chg);
			mtk_vdec_queue_res_chg_event(ctx);
		} else {
			mtk_vdec_pic_info_update(ctx);
			/*
			 * On encountering a resolution change in the stream.
			 * The driver must first process and decode all
			 * remaining buffers from before the resolution change
			 * point, so call flush decode here
			 */
			mtk_vdec_flush_decoder(ctx);
			/*
			 * After all buffers containing decoded frames from
			 * before the resolution change point ready to be
			 * dequeued on the CAPTURE queue, the driver sends a
			 * V4L2_EVENT_SOURCE_CHANGE event for source change
			 * type V4L2_EVENT_SRC_CH_RESOLUTION
			 */
			mtk_vdec_queue_res_chg_event(ctx);
		}

	} else if (ret == 0) {
		ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
		if (dpbsize != 0) {
			ctx->dpb_size = dpbsize;
			ctx->last_dpb_size = dpbsize;
		} else {
			mtk_v4l2_err("[%d] GET_PARAM_DPB_SIZE fail=%d",
				 ctx->id, ret);
		}
	}

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
}

static void vb2ops_vdec_stateful_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *src_buf;
	struct mtk_vcodec_mem src_mem;
	unsigned int src_chg = 0;
	bool res_chg = false;
	bool mtk_vcodec_unsupport = false;
	bool wait_seq_header = false;
	int ret = 0;
	unsigned long frame_size[2];
	unsigned int i = 0;
	unsigned int dpbsize = 1;
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct mtk_video_dec_buf *buf = NULL;
	struct mtk_q_data *dst_q_data;
	unsigned int fourcc;

	mtk_v4l2_debug(3, "[%d] (%d) id=%d, vb=%p",
			ctx->id, vb->vb2_queue->type,
			vb->index, vb);
	/*
	 * check if this buffer is ready to be used after decode
	 */
	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		vb2_v4l2 = to_vb2_v4l2_buffer(vb);
		buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
		mutex_lock(&ctx->lock);
		if (buf->used == false) {
			v4l2_m2m_buf_queue(ctx->m2m_ctx, vb2_v4l2);
			buf->queued_in_vb2 = true;
			buf->queued_in_v4l2 = true;
		} else {
			buf->queued_in_vb2 = false;
			buf->queued_in_v4l2 = true;
		}
		mutex_unlock(&ctx->lock);
		return;
	}

	v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));

	if (ctx->state != MTK_STATE_INIT) {
		mtk_v4l2_debug(3, "[%d] already init driver %d",
				ctx->id, ctx->state);
		return;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (!src_buf) {
		mtk_v4l2_err("No src buffer");
		return;
	}
	buf = container_of(src_buf, struct mtk_video_dec_buf, vb);
	if (buf->lastframe) {
		/* This shouldn't happen. Just in case. */
		mtk_v4l2_err("Invalid flush buffer.");
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		return;
	}

	src_mem.va = vb2_plane_vaddr(&src_buf->vb2_buf, 0);
	src_mem.dma_addr = vb2_dma_contig_plane_dma_addr(&src_buf->vb2_buf, 0);
	src_mem.size = (size_t)src_buf->vb2_buf.planes[0].bytesused;
	src_mem.length = (size_t)src_buf->vb2_buf.planes[0].length;
	src_mem.dmabuf = src_buf->vb2_buf.planes[0].dbuf;
	src_mem.flags = src_buf->flags;
	src_mem.index = src_buf->vb2_buf.index;
	mtk_v4l2_debug(2,
		"[%d] buf id=%d va=%p dma=%p size=%zx length=%zu dmabuf=%p",
		ctx->id, src_buf->vb2_buf.index,
		src_mem.va, &src_mem.dma_addr,
		src_mem.size, src_mem.length,
		src_mem.dmabuf);
	if (src_mem.va != NULL) {
		mtk_v4l2_debug(0, "[%d] %x %x %x %x %x %x %x %x %x\n",
					   ctx->id,
					   ((char *)src_mem.va)[0],
					   ((char *)src_mem.va)[1],
					   ((char *)src_mem.va)[2],
					   ((char *)src_mem.va)[3],
					   ((char *)src_mem.va)[4],
					   ((char *)src_mem.va)[5],
					   ((char *)src_mem.va)[6],
					   ((char *)src_mem.va)[7],
					   ((char *)src_mem.va)[8]);
	}


	frame_size[0] = ctx->dec_params.frame_size_width;
	frame_size[1] = ctx->dec_params.frame_size_height;
	vdec_if_set_param(ctx, SET_PARAM_FRAME_SIZE, frame_size);
	ret = vdec_if_decode(ctx, &src_mem, NULL, &src_chg);
	mtk_vdec_set_param(ctx);

	/* src_chg bit0 for res change flag, bit1 for realloc mv buf flag,
	 * bit2 for not support flag, other bits are reserved
	 */
	res_chg = ((src_chg & VDEC_RES_CHANGE) != 0U) ? true : false;
	mtk_vcodec_unsupport = ((src_chg & VDEC_HW_NOT_SUPPORT) != 0) ?
						   true : false;
	wait_seq_header = ((src_chg & VDEC_NEED_SEQ_HEADER) != 0U) ?
					  true : false;

	if (ret || !res_chg || mtk_vcodec_unsupport || wait_seq_header) {
		/*
		 * fb == NULL menas to parse SPS/PPS header or
		 * resolution info in src_mem. Decode can fail
		 * if there is no SPS header or picture info
		 * in bs
		 */
		vb2_v4l2 = to_vb2_v4l2_buffer(vb);
		buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);

		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		if (ret == -EIO) {
			mtk_v4l2_err("[%d] Unrecoverable error in vdec_if_decode.",
					ctx->id);
			ctx->state = MTK_STATE_ABORT;
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_ERROR);
		} else {
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
		}
		mtk_v4l2_debug(ret ? 0 : 1,
			       "[%d] vdec_if_decode() src_buf=%d, size=%zu, fail=%d, res_chg=%d",
			       ctx->id, src_buf->vb2_buf.index,
			       src_mem.size, ret, res_chg);
		/* If not support the source, eg: w/h,
		 * bitdepth, level, we need to stop to play it
		 */
		if (mtk_vcodec_unsupport || buf->lastframe  != NON_EOS) {
			mtk_v4l2_err("[%d]Error!! Codec driver not support the file!",
						 ctx->id);
			mtk_vdec_queue_error_event(ctx);
		}
		return;
	}

	if (res_chg) {
		mtk_v4l2_debug(3, "[%d] vdec_if_decode() res_chg: %d\n",
			ctx->id, res_chg);
		mtk_vdec_queue_res_chg_event(ctx);

		/* remove all framebuffer.
		 * framebuffer with old byteused cannot use.
		 */
		while (v4l2_m2m_dst_buf_remove(ctx->m2m_ctx) != NULL)
			mtk_v4l2_debug(3, "[%d] v4l2_m2m_dst_buf_remove()",
				ctx->id);
	}


	if (vdec_if_get_param(ctx, GET_PARAM_PIC_INFO, &ctx->picinfo)) {
		mtk_v4l2_err("[%d]Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
				ctx->id);
		return;
	}

	ctx->last_decoded_picinfo = ctx->picinfo;
	dst_q_data = &ctx->q_data[MTK_Q_DATA_DST];

	fourcc = ctx->picinfo.cap_fourcc;
	dst_q_data->fmt = mtk_find_fmt_by_pixel(fourcc);
	for (i = 0; i < dst_q_data->fmt->num_planes; i++) {
		dst_q_data->sizeimage[i] = ctx->picinfo.fb_sz[i];
		dst_q_data->bytesperline[i] = ctx->picinfo.buf_w;
	}

	mtk_v4l2_debug(2, "[%d] vdec_if_init() OK wxh=%dx%d pic wxh=%dx%d sz[0]=0x%x sz[1]=0x%x",
			ctx->id,
			ctx->picinfo.buf_w, ctx->picinfo.buf_h,
			ctx->picinfo.pic_w, ctx->picinfo.pic_h,
			dst_q_data->sizeimage[0],
			dst_q_data->sizeimage[1]);

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		mtk_v4l2_err("[%d] GET_PARAM_DPB_SIZE fail=%d", ctx->id, ret);

	ctx->dpb_size = dpbsize;
	ctx->last_dpb_size = dpbsize;
	ctx->state = MTK_STATE_HEADER;
	mtk_v4l2_debug(1, "[%d] dpbsize=%d", ctx->id, ctx->dpb_size);
}

static int mtk_vdec_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);

	mtk_v4l2_debug(4, "[%d] id 0x%x val %d array[0] %d array[1] %d",
				   ctx->id, ctrl->id, ctrl->val,
				   ctrl->p_new.p_u32[0], ctrl->p_new.p_u32[1]);

	if (ctrl->id == V4L2_CID_MPEG_MTK_SEC_DECODE) {
		ctx->dec_params.svp_mode = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_SEC_DECODE;
#ifdef CONFIG_VB2_MEDIATEK_DMA
		mtk_dma_contig_set_secure_mode(&ctx->dev->plat_dev->dev,
					ctx->dec_params.svp_mode);
#endif
		mtk_v4l2_debug(0, "[%d] V4L2_CID_MPEG_MTK_SEC_DECODE id %d val %d",
			ctx->id, ctrl->id, ctrl->val);
	}

	switch (ctrl->id) {
	case V4L2_CID_MPEG_MTK_DECODE_MODE:
		ctx->dec_params.decode_mode = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_DECODE_MODE;
		break;
	case V4L2_CID_MPEG_MTK_SEC_DECODE:
		ctx->dec_params.svp_mode = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_SEC_DECODE;
#ifdef CONFIG_VB2_MEDIATEK_DMA
		mtk_dma_contig_set_secure_mode(&ctx->dev->plat_dev->dev,
					ctx->dec_params.svp_mode);
#endif
		break;
	case V4L2_CID_MPEG_MTK_FRAME_SIZE:
		if (ctx->dec_params.frame_size_width == 0)
			ctx->dec_params.frame_size_width = ctrl->val;
		else if (ctx->dec_params.frame_size_height == 0)
			ctx->dec_params.frame_size_height = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_FRAME_SIZE;
		break;
	case V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER:
		if (ctx->dec_params.fixed_max_frame_size_width == 0)
			ctx->dec_params.fixed_max_frame_size_width = ctrl->val;
		else if (ctx->dec_params.fixed_max_frame_size_height == 0)
			ctx->dec_params.fixed_max_frame_size_height = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE;
		break;
	case V4L2_CID_MPEG_MTK_CRC_PATH:
		ctx->dec_params.crc_path = ctrl->p_new.p_char;
		ctx->dec_param_change |= MTK_DEC_PARAM_CRC_PATH;
		break;
	case V4L2_CID_MPEG_MTK_GOLDEN_PATH:
		ctx->dec_params.golden_path = ctrl->p_new.p_char;
		ctx->dec_param_change |= MTK_DEC_PARAM_GOLDEN_PATH;
		break;
	case V4L2_CID_MPEG_MTK_SET_WAIT_KEY_FRAME:
		ctx->dec_params.wait_key_frame = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_WAIT_KEY_FRAME;
		break;
	case V4L2_CID_MPEG_MTK_SET_NAL_SIZE_LENGTH:
		ctx->dec_params.nal_size_length = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_NAL_SIZE_LENGTH;
		break;
	case V4L2_CID_MPEG_MTK_OPERATING_RATE:
		ctx->dec_params.operating_rate = ctrl->val;
		ctx->dec_param_change |= MTK_DEC_PARAM_OPERATING_RATE;
		break;
	default:
		mtk_v4l2_err("ctrl-id=%x not support!", ctrl->id);
		return -EINVAL;
	}

	return 0;
}

static const struct v4l2_ctrl_ops mtk_vcodec_dec_ctrl_ops = {
	.g_volatile_ctrl = mtk_vdec_g_v_ctrl,
	.s_ctrl = mtk_vdec_s_ctrl,
};

static const struct v4l2_ctrl_config mtk_color_desc_ctrl = {
	.ops = &mtk_vcodec_dec_ctrl_ops,
	.id = V4L2_CID_MPEG_MTK_COLOR_DESC,
	.name = "MTK Color Description for HDR",
	.type = V4L2_CTRL_TYPE_U32,
	.min = 0x00000000,
	.max = 0x00ffffff,
	.step = 1,
	.def = 0,
	.dims = { sizeof(struct mtk_color_desc)/sizeof(u32) },
};

static const struct v4l2_ctrl_config mtk_interlacing_ctrl = {
	.ops = &mtk_vcodec_dec_ctrl_ops,
	.id = V4L2_CID_MPEG_MTK_INTERLACING,
	.name = "MTK Query Interlacing",
	.type = V4L2_CTRL_TYPE_BOOLEAN,
	.min = 0,
	.max = 1,
	.step = 1,
	.def = 0,
};

static const struct v4l2_ctrl_config mtk_codec_type_ctrl = {
	.ops = &mtk_vcodec_dec_ctrl_ops,
	.id = V4L2_CID_MPEG_MTK_CODEC_TYPE,
	.name = "MTK Query HW/SW Codec Type",
	.type = V4L2_CTRL_TYPE_U32,
	.min = 0,
	.max = 10,
	.step = 1,
	.def = 0,
};

static int mtk_vcodec_dec_ctrls_setup(struct mtk_vcodec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, 1);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
				0, 32, 1, 1);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	v4l2_ctrl_new_std_menu(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_VIDEO_VP9_PROFILE,
				V4L2_MPEG_VIDEO_VP9_PROFILE_0,
				0, V4L2_MPEG_VIDEO_VP9_PROFILE_0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_FRAME_INTERVAL,
		16666, 41719, 1, 33333);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_ASPECT_RATIO,
				0, 0xF000F, 1, 0x10001);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_FIX_BUFFERS,
				0, 0xF, 1, 0);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_FIX_BUFFERS_SVP,
				0, 0xF, 1, 0);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl,
				&mtk_interlacing_ctrl, NULL);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;
	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl,
				&mtk_codec_type_ctrl, NULL);
	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &mtk_color_desc_ctrl, NULL);
	ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;


	/* s_ctrl */
	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_DECODE_MODE,
		0, 32, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_SEC_DECODE,
		0, 32, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_FRAME_SIZE,
		0, 65535, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER,
		0, 65535, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_CRC_PATH,
		0, 255, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_GOLDEN_PATH,
		0, 255, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_SET_WAIT_KEY_FRAME,
				0, 255, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_OPERATING_RATE,
				0, 1024, 1, 0);

	if (ctx->ctrl_hdl.error) {
		mtk_v4l2_err("Adding control failed %d",
				ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);
	return 0;
}

static void mtk_init_vdec_params(struct mtk_vcodec_ctx *ctx)
{
}

static struct vb2_ops mtk_vdec_frame_vb2_ops = {
	.queue_setup	= vb2ops_vdec_queue_setup,
	.buf_prepare	= vb2ops_vdec_buf_prepare,
	.wait_prepare	= vb2_ops_wait_prepare,
	.wait_finish	= vb2_ops_wait_finish,
	.start_streaming	= vb2ops_vdec_start_streaming,

	.buf_queue	= vb2ops_vdec_stateful_buf_queue,
	.buf_init	= vb2ops_vdec_buf_init,
	.buf_finish	= vb2ops_vdec_buf_finish,
	.stop_streaming	= vb2ops_vdec_stop_streaming,
};

const struct mtk_vcodec_dec_pdata mtk_frame_pdata = {
	.init_vdec_params = mtk_init_vdec_params,
	.ctrls_setup = mtk_vcodec_dec_ctrls_setup,
	.vdec_vb2_ops = &mtk_vdec_frame_vb2_ops,
	.vdec_formats = mtk_video_formats,
	.num_formats = NUM_FORMATS,
	.default_out_fmt = &mtk_video_formats[DEFAULT_OUT_FMT_IDX],
	.default_cap_fmt = &mtk_video_formats[DEFAULT_CAP_FMT_IDX],
	.vdec_framesizes = mtk_vdec_framesizes,
	.num_framesizes = NUM_SUPPORTED_FRAMESIZE,
	.worker = mtk_vdec_worker,
	.flush_decoder = mtk_vdec_flush_decoder,
};
