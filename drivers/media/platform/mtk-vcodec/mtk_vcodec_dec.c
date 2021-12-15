// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <linux/delay.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcodec_dec_pm.h"
#include "vdec_drv_if.h"

#define MTK_VDEC_MIN_W  64U
#define MTK_VDEC_MIN_H  64U
#define DFT_CFG_WIDTH   MTK_VDEC_MIN_W
#define DFT_CFG_HEIGHT  MTK_VDEC_MIN_H

static struct mtk_video_fmt
	mtk_video_formats[MTK_MAX_DEC_CODECS_SUPPORT] = { {0} };
static struct mtk_codec_framesizes
	mtk_vdec_framesizes[MTK_MAX_DEC_CODECS_SUPPORT] = { {0} };
static unsigned int default_out_fmt_idx;
static unsigned int default_cap_fmt_idx;

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(mtk_vdec_framesizes)
#define NUM_FORMATS ARRAY_SIZE(mtk_video_formats)
#ifdef CONFIG_VB2_MEDIATEK_DMA_CONTIG
static struct vb2_mem_ops vdec_ion_dma_contig_memops;
#endif

static void get_supported_format(struct mtk_vcodec_ctx *ctx)
{
	unsigned int i;

	if (mtk_video_formats[0].fourcc == 0) {
		if (vdec_if_get_param(ctx,
			GET_PARAM_CAPABILITY_SUPPORTED_FORMATS,
			&mtk_video_formats)  != 0) {
			mtk_v4l2_err("Error!! Cannot get supported format");
			return;
		}

		for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
			if (mtk_video_formats[i].fourcc != 0)
				mtk_v4l2_debug(1,
				  "fmt[%d] fourcc %d type %d planes %d",
				  i, mtk_video_formats[i].fourcc,
				  mtk_video_formats[i].type,
				  mtk_video_formats[i].num_planes);
		}
		for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
			if (mtk_video_formats[i].fourcc != 0 &&
				mtk_video_formats[i].type == MTK_FMT_DEC) {
				default_out_fmt_idx = i;
				break;
			}
		}
		for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
			if (mtk_video_formats[i].fourcc != 0 &&
				mtk_video_formats[i].type == MTK_FMT_FRAME) {
				default_cap_fmt_idx = i;
				break;
			}
		}
	}
}

static struct mtk_video_fmt *mtk_vdec_find_format(struct mtk_vcodec_ctx *ctx,
	struct v4l2_format *f, unsigned int t)
{
	struct mtk_video_fmt *fmt;
	unsigned int k;

	mtk_v4l2_debug(3, "[%d] fourcc %d", ctx->id, f->fmt.pix_mp.pixelformat);
	for (k = 0; k < MTK_MAX_DEC_CODECS_SUPPORT &&
		 mtk_video_formats[k].fourcc != 0; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == f->fmt.pix_mp.pixelformat &&
			mtk_video_formats[k].type == t)
			return fmt;
	}

	return NULL;
}

static struct mtk_video_fmt *mtk_find_fmt_by_pixel(unsigned int pixelformat)
{
	struct mtk_video_fmt *fmt;
	unsigned int k;

	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == pixelformat)
			return fmt;
	}
	mtk_v4l2_err("Error!! Cannot find fourcc: %d use default", pixelformat);

	return &mtk_video_formats[default_out_fmt_idx];
}

static struct mtk_q_data *mtk_vdec_get_q_data(struct mtk_vcodec_ctx *ctx,
	enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->q_data[MTK_Q_DATA_SRC];

	return &ctx->q_data[MTK_Q_DATA_DST];
}

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
	unsigned int i = 0;
	unsigned int num_planes = 0;

	mtk_v4l2_debug(4, "[%d]", ctx->id);

	mutex_lock(&ctx->buf_lock);
	if (vdec_if_get_param(ctx,
						  GET_PARAM_DISP_FRAME_BUFFER,
						  &disp_frame_buffer)) {
		mtk_v4l2_err("[%d]Cannot get param : GET_PARAM_DISP_FRAME_BUFFER",
					 ctx->id);
		mutex_unlock(&ctx->buf_lock);
		return NULL;
	}
	mutex_unlock(&ctx->buf_lock);

	if (disp_frame_buffer == NULL) {
		mtk_v4l2_debug(4, "No display frame buffer");
		return NULL;
	}
	if (!virt_addr_valid(disp_frame_buffer)) {
		mtk_v4l2_debug(3, "Bad display frame buffer %p",
			disp_frame_buffer);
		return NULL;
	}

	dstbuf = container_of(disp_frame_buffer, struct mtk_video_dec_buf,
						  frame_buffer);
	num_planes = dstbuf->vb.vb2_buf.num_planes;
	mutex_lock(&ctx->buf_lock);
	if (dstbuf->used) {
		for (i = 0; i < num_planes; i++) {
			vb2_set_plane_payload(&dstbuf->vb.vb2_buf, i,
				ctx->picinfo.fb_sz[i]);
		}

		dstbuf->ready_to_display = true;

		if (got_early_eos &&
			dstbuf->vb.vb2_buf.timestamp == ctx->input_max_ts)
			dstbuf->vb.flags |= V4L2_BUF_FLAG_LAST;

		dstbuf->vb.vb2_buf.timestamp =
				disp_frame_buffer->timestamp;

		mtk_v4l2_debug(2,
			"[%d]status=%x queue id=%d to done_list %d %d flag=%x pts=%llu",
			ctx->id, disp_frame_buffer->status,
			dstbuf->vb.vb2_buf.index,
			dstbuf->queued_in_vb2, got_early_eos,
			dstbuf->vb.flags, dstbuf->vb.vb2_buf.timestamp);

		v4l2_m2m_buf_done(&dstbuf->vb, VB2_BUF_STATE_DONE);
		ctx->decoded_frame_cnt++;
	}
	mutex_unlock(&ctx->buf_lock);
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
	int i;

	mutex_lock(&ctx->buf_lock);
	if (vdec_if_get_param(ctx,
						  GET_PARAM_FREE_FRAME_BUFFER,
						  &free_frame_buffer)) {
		mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
		mutex_unlock(&ctx->buf_lock);
		return NULL;
	}
	mutex_unlock(&ctx->buf_lock);

	if (free_frame_buffer == NULL) {
		mtk_v4l2_debug(4, " No free frame buffer");
		return NULL;
	}
	if (!virt_addr_valid(free_frame_buffer)) {
		mtk_v4l2_debug(3, "Bad free frame buffer %p",
			free_frame_buffer);
		return NULL;
	}
	mtk_v4l2_debug(4, "[%d] tmp_frame_addr = 0x%p",
				   ctx->id, free_frame_buffer);

	dstbuf = container_of(free_frame_buffer, struct mtk_video_dec_buf,
						  frame_buffer);
	dstbuf->flags |= REF_FREED;

	mutex_lock(&ctx->buf_lock);
	if (dstbuf->used) {
		for (i = 0; i < free_frame_buffer->num_planes; i++) {
			fput(free_frame_buffer->fb_base[i].dmabuf->file);
			mtk_v4l2_debug(4, "[Ref cnt] id=%d Ref put dma %p",
				free_frame_buffer->index, free_frame_buffer->fb_base[i].dmabuf);
		}
		dstbuf->used = false;
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
			v4l2_m2m_buf_queue_check(ctx->m2m_ctx, &dstbuf->vb);
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
			v4l2_m2m_buf_queue_check(ctx->m2m_ctx, &dstbuf->vb);
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
			mtk_v4l2_debug(4, "[%d]status=%x err queue id=%d %d %d",
				ctx->id, free_frame_buffer->status,
				dstbuf->vb.vb2_buf.index,
				dstbuf->queued_in_vb2,
				dstbuf->queued_in_v4l2);
		}
	}
	mutex_unlock(&ctx->buf_lock);
	return &dstbuf->vb.vb2_buf;
}

static struct vb2_buffer *get_free_bs_buffer(struct mtk_vcodec_ctx *ctx,
	struct mtk_vcodec_mem *current_bs)
{
	struct mtk_vcodec_mem *free_bs_buffer;
	struct mtk_video_dec_buf *srcbuf;

	mutex_lock(&ctx->buf_lock);
	if (vdec_if_get_param(ctx, GET_PARAM_FREE_BITSTREAM_BUFFER,
						  &free_bs_buffer) != 0) {
		mtk_v4l2_err("[%d] Cannot get param : GET_PARAM_FREE_BITSTREAM_BUFFER",
					 ctx->id);
		mutex_unlock(&ctx->buf_lock);
		return NULL;
	}
	mutex_unlock(&ctx->buf_lock);

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
	if (!virt_addr_valid(free_bs_buffer)) {
		mtk_v4l2_debug(3, "Bad free bitstream buffer %p",
			free_bs_buffer);
		return NULL;
	}

	srcbuf = container_of(free_bs_buffer,
		struct mtk_video_dec_buf, bs_buffer);
	mtk_v4l2_debug(2,
		"[%d] length=%zu size=%zu queue idx=%d to done_list %d",
		ctx->id, free_bs_buffer->length, free_bs_buffer->size,
		srcbuf->vb.vb2_buf.index,
		srcbuf->queued_in_vb2);

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

static void clean_free_fm_buffer(struct mtk_vcodec_ctx *ctx)
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

	v4l2_m2m_set_dst_buffered(ctx->m2m_ctx,
		ctx->input_driven);
}

static void mtk_vdec_queue_stop_play_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_eos = {
		.type = V4L2_EVENT_EOS,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_eos);
}

static void mtk_vdec_queue_noseqheader_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_eos = {
		.type = V4L2_EVENT_MTK_VDEC_NOHEADER,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_eos);
}

static void mtk_vdec_queue_error_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_error = {
		.type = V4L2_EVENT_MTK_VDEC_ERROR,
	};

	mtk_v4l2_debug(1, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_error);
}

static void mtk_vdec_flush_decoder(struct mtk_vcodec_ctx *ctx,
	struct mtk_vcodec_mem *current_bs)
{
	unsigned int src_chg = 0;
	int ret = 0;

	ctx->state = MTK_STATE_FLUSH;
	if (ctx->input_driven)
		wake_up(&ctx->fm_wq);

	ret = vdec_if_decode(ctx, NULL, NULL, &src_chg);
	if (ret) {
		mtk_v4l2_err("DecodeFinal failed, ret=%d", ret);
		return;
	}

	clean_free_bs_buffer(ctx, current_bs);
	clean_display_buffer(ctx, 0);
	clean_free_fm_buffer(ctx);
}

static void mtk_vdec_pic_info_update(struct mtk_vcodec_ctx *ctx)
{
	unsigned int dpbsize = 0;
	int ret;
	struct mtk_color_desc color_desc = {.is_hdr = 0};

	if (vdec_if_get_param(ctx,
						  GET_PARAM_PIC_INFO,
						  &ctx->last_decoded_picinfo)) {
		mtk_v4l2_err("[%d]Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
					 ctx->id);
		return;
	}

	if (ctx->last_decoded_picinfo.pic_w == 0 ||
		ctx->last_decoded_picinfo.pic_h == 0 ||
		ctx->last_decoded_picinfo.buf_w == 0 ||
		ctx->last_decoded_picinfo.buf_h == 0) {
		mtk_v4l2_err("Cannot get correct pic info");
		return;
	}

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		mtk_v4l2_err("Incorrect dpb size, ret=%d", ret);
	ctx->last_dpb_size = dpbsize;

	ret = vdec_if_get_param(ctx, GET_PARAM_COLOR_DESC, &color_desc);
	if (ret == 0)
		ctx->last_is_hdr = color_desc.is_hdr;

	mtk_v4l2_debug(1,
				   "[%d]-> new(%d,%d),dpb(%d), old(%d,%d),dpb(%d), bit(%d) real(%d,%d) hdr(%d,%d)",
				   ctx->id, ctx->last_decoded_picinfo.pic_w,
				   ctx->last_decoded_picinfo.pic_h,
				   ctx->last_dpb_size,
				   ctx->picinfo.pic_w, ctx->picinfo.pic_h,
				   ctx->dpb_size,
				   ctx->picinfo.bitdepth,
				   ctx->last_decoded_picinfo.buf_w,
				   ctx->last_decoded_picinfo.buf_h,
				   ctx->is_hdr, ctx->last_is_hdr);
}


int mtk_vdec_put_fb(struct mtk_vcodec_ctx *ctx, int type)
{
	struct mtk_video_dec_buf *dst_buf_info, *src_buf_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2, *src_vb2_v4l2;
	struct vb2_buffer *src_buf, *dst_buf;
	struct vdec_fb *pfb;
	int i, ret = 0;

	mtk_v4l2_debug(1, "type = %d", type);
	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);

	src_vb2_v4l2 = container_of(src_buf, struct vb2_v4l2_buffer, vb2_buf);
	src_buf_info = container_of(src_vb2_v4l2, struct mtk_video_dec_buf, vb);

	if (src_buf_info == NULL || src_buf == NULL)
		return 0;

	if (src_buf_info->lastframe == EOS) {
		clean_display_buffer(ctx, src_buf->planes[0].bytesused != 0U);
		clean_free_fm_buffer(ctx);

		if (src_buf->planes[0].bytesused == 0U) {
			src_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
			vb2_set_plane_payload(&src_buf_info->vb.vb2_buf, 0, 0);
			if (src_buf_info != ctx->dec_flush_buf)
				v4l2_m2m_buf_done(&src_buf_info->vb,
					VB2_BUF_STATE_DONE);

			if (ctx->input_driven)
				ret = wait_event_interruptible(
					ctx->fm_wq,
					v4l2_m2m_num_dst_bufs_ready(
					ctx->m2m_ctx) > 0 ||
					ctx->state == MTK_STATE_FLUSH);

			/* update dst buf status */
			dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
			if (ctx->state == MTK_STATE_FLUSH
				|| ret != 0 || dst_buf == NULL) {
				mtk_v4l2_debug(0, "wait EOS dst break!state %d, ret %d, dst_buf %p",
				ctx->state, ret, dst_buf);
				return 0;
			}

			dst_vb2_v4l2 = container_of(dst_buf,
				struct vb2_v4l2_buffer, vb2_buf);
			dst_buf_info = container_of(dst_vb2_v4l2,
				struct mtk_video_dec_buf, vb);

			dst_buf_info->vb.vb2_buf.timestamp = 0;
			memset(&dst_buf_info->vb.timecode, 0, sizeof(struct v4l2_timecode));
			dst_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
			pfb = &dst_buf_info->frame_buffer;
			for (i = 0; i < pfb->num_planes; i++)
				vb2_set_plane_payload(&dst_buf_info->vb.vb2_buf,
					i, 0);
			v4l2_m2m_buf_done(&dst_buf_info->vb,
				VB2_BUF_STATE_DONE);
		}

		mtk_vdec_queue_stop_play_event(ctx);
	} else {
		if (!ctx->input_driven)
			dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
		clean_display_buffer(ctx,
			src_buf_info->lastframe == EOS_WITH_DATA);
		clean_free_fm_buffer(ctx);
	}

	return 0;
}

static void mtk_vdec_worker(struct work_struct *work)
{
	struct mtk_vcodec_ctx *ctx = container_of(work, struct mtk_vcodec_ctx,
		decode_work);
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct vb2_buffer *src_buf, *dst_buf;
	struct mtk_vcodec_mem *buf;
	struct vdec_fb *pfb = NULL;
	unsigned int src_chg = 0;
	bool res_chg = false;
	bool need_more_output = false;
	bool mtk_vcodec_unsupport = false;
	int ret;
	unsigned int i = 0;
	unsigned int num_planes;
	struct timeval worktvstart;
	struct timeval worktvstart1;
	struct timeval vputvend;
	struct mtk_video_dec_buf *dst_buf_info = NULL, *src_buf_info = NULL;
	struct vb2_v4l2_buffer *dst_vb2_v4l2, *src_vb2_v4l2;
	unsigned int fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	unsigned int dpbsize = 0;
	struct mtk_color_desc color_desc = {.is_hdr = 0};

	mutex_lock(&ctx->worker_lock);
	if (ctx->state != MTK_STATE_HEADER) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, " %d", ctx->state);
		mutex_unlock(&ctx->worker_lock);
		return;
	}

	do_gettimeofday(&worktvstart);
	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, "[%d] src_buf empty!!", ctx->id);
		mutex_unlock(&ctx->worker_lock);
		return;
	}

	dst_buf = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	if (dst_buf == NULL && !ctx->input_driven) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_debug(1, "[%d] dst_buf empty!!", ctx->id);
		mutex_unlock(&ctx->worker_lock);
		return;
	}

	src_vb2_v4l2 = container_of(src_buf, struct vb2_v4l2_buffer, vb2_buf);
	src_buf_info = container_of(src_vb2_v4l2, struct mtk_video_dec_buf, vb);

	if (!ctx->input_driven) {
		dst_vb2_v4l2 = container_of(dst_buf,
			struct vb2_v4l2_buffer, vb2_buf);
		dst_buf_info = container_of(dst_vb2_v4l2,
			struct mtk_video_dec_buf, vb);

		pfb = &dst_buf_info->frame_buffer;
		num_planes = dst_vb2_v4l2->vb2_buf.num_planes;
		pfb->num_planes = num_planes;
		pfb->index = dst_buf->index;

		mutex_lock(&ctx->buf_lock);
		for (i = 0; i < num_planes; i++) {
			pfb->fb_base[i].va = vb2_plane_vaddr(dst_buf, i);
#ifdef CONFIG_VB2_MEDIATEK_DMA_SG
			pfb->fb_base[i].dma_addr =
				mtk_dma_sg_plane_dma_addr(dst_buf, i);
#else
			pfb->fb_base[i].dma_addr =
				vb2_dma_contig_plane_dma_addr(dst_buf, i);
#endif
			pfb->fb_base[i].size = ctx->picinfo.fb_sz[i];
			pfb->fb_base[i].length = dst_buf->planes[i].length;
			pfb->fb_base[i].dmabuf = dst_buf->planes[i].dbuf;

			if (dst_buf_info->used == false) {
				get_file(dst_buf->planes[i].dbuf->file);
				mtk_v4l2_debug(4, "[Ref cnt] id=%d Ref get dma %p", dst_buf->index,
					dst_buf->planes[i].dbuf);
			}
		}
		pfb->status = 0;
		dst_buf_info->used = true;
		mutex_unlock(&ctx->buf_lock);

		mtk_v4l2_debug(1,
				"id=%d Framebuf  pfb=%p VA=%p Y_DMA=%lx C_DMA=%lx ion_buffer=(%p %p) Size=%zx, general_buf DMA=%lx fd=%d ",
			dst_buf->index, pfb,
			pfb->fb_base[0].va,
			(unsigned long)pfb->fb_base[0].dma_addr,
			(unsigned long)pfb->fb_base[1].dma_addr,
			pfb->fb_base[0].dmabuf,
			pfb->fb_base[1].dmabuf, pfb->fb_base[0].size,
			(unsigned long)
			dst_buf_info->frame_buffer.dma_general_addr,
			dst_buf_info->general_user_fd);
	}

	mtk_v4l2_debug(4, "===>[%d] vdec_if_decode() ===>", ctx->id);


	if (src_buf_info->lastframe == EOS) {
		mtk_v4l2_debug(4, "===>[%d] vdec_if_decode() EOS ===> %d %d",
			ctx->id, src_buf->planes[0].bytesused,
			src_buf->planes[0].length);

		vdec_if_decode(ctx, NULL, NULL, &src_chg);

		mtk_vdec_put_fb(ctx, -1);
		v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		src_buf_info->lastframe = NON_EOS;
		clean_free_bs_buffer(ctx, NULL);

		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mutex_unlock(&ctx->worker_lock);
		return;
	}

	buf = &src_buf_info->bs_buffer;
	buf->va = vb2_plane_vaddr(src_buf, 0);
	buf->dma_addr = (src_buf->planes[0].mem_priv == NULL) ?
		0 : vb2_dma_contig_plane_dma_addr(src_buf, 0);
	buf->size = (size_t)src_buf->planes[0].bytesused;
	buf->length = (size_t)src_buf->planes[0].length;
	buf->dmabuf = src_buf->planes[0].dbuf;
	buf->flags = src_vb2_v4l2->flags;
	buf->index = src_buf->index;

	if (buf->dma_addr == 0 || buf->dmabuf == NULL) {
		v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
		mtk_v4l2_err("[%d] id=%d src_addr is NULL!!",
					 ctx->id, src_buf->index);
		mutex_unlock(&ctx->worker_lock);
		return;
	}

	ctx->dec_params.timestamp = src_buf_info->vb.vb2_buf.timestamp;
	mtk_v4l2_debug(1,
	"[%d] Bs VA=%p DMA=%lx Size=%zx Len=%zx ion_buf = %p vb=%p eos=%d pts=%llu",
		ctx->id, buf->va, (unsigned long)buf->dma_addr,
		buf->size, buf->length,
		buf->dmabuf, src_buf,
		src_buf_info->lastframe,
		src_buf_info->vb.vb2_buf.timestamp);
	if (!ctx->input_driven) {
		dst_buf_info->flags &= ~CROP_CHANGED;
		dst_buf_info->flags &= ~REF_FREED;

		dst_buf_info->vb.vb2_buf.timestamp
			= src_buf_info->vb.vb2_buf.timestamp;
		dst_buf_info->vb.timecode
			= src_buf_info->vb.timecode;
	}
	src_buf_info->used = true;
	do_gettimeofday(&worktvstart1);
	ret = vdec_if_decode(ctx, buf, pfb, &src_chg);
	do_gettimeofday(&vputvend);
	mtk_vcodec_perf_log("vpud:%ld",
		(vputvend.tv_sec - worktvstart1.tv_sec) * 1000000 +
		(vputvend.tv_usec - worktvstart1.tv_usec));

	res_chg = ((src_chg & VDEC_RES_CHANGE) != 0U) ? true : false;
	need_more_output =
		((src_chg & VDEC_NEED_MORE_OUTPUT_BUF) != 0U) ? true : false;
	mtk_vcodec_unsupport = ((src_chg & VDEC_HW_NOT_SUPPORT) != 0) ?
						   true : false;
	if ((src_chg & VDEC_CROP_CHANGED) &&
		(!ctx->input_driven) && dst_buf_info != NULL)
		dst_buf_info->flags |= CROP_CHANGED;


	if (src_chg & VDEC_OUTPUT_NOT_GENERATED)
		src_vb2_v4l2->flags |= V4L2_BUF_FLAG_OUTPUT_NOT_GENERATED;

	if (!ctx->input_driven)
		mtk_vdec_put_fb(ctx, -1);

	if (ret < 0 || mtk_vcodec_unsupport) {
		mtk_v4l2_err(
			" <===[%d], src_buf[%d] last_frame = %d sz=0x%zx pts=%llu vdec_if_decode() ret=%d src_chg=%d===>",
			ctx->id,
			src_buf->index,
			src_buf_info->lastframe,
			buf->size,
			src_buf_info->vb.vb2_buf.timestamp,
			ret, src_chg);
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		clean_free_bs_buffer(ctx, &src_buf_info->bs_buffer);
		if (ret == -EIO) {
			/* ipi timeout / VPUD crashed ctx abort */
			ctx->state = MTK_STATE_ABORT;
			mtk_vdec_queue_error_event(ctx);
			v4l2_m2m_buf_done(&src_buf_info->vb,
				VB2_BUF_STATE_ERROR);
		} else if (mtk_vcodec_unsupport) {
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
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		src_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
		clean_free_bs_buffer(ctx, NULL);
		if (ctx->dec_flush_buf->lastframe == NON_EOS) {
			ctx->dec_flush_buf->lastframe = EOS;
			ctx->dec_flush_buf->vb.vb2_buf.planes[0].bytesused = 1;
			v4l2_m2m_buf_queue_check(ctx->m2m_ctx, &ctx->dec_flush_buf->vb);
		} else {
			mtk_v4l2_debug(1, "Stopping no need to queue dec_flush_buf.");
		}
	} else if ((ret == 0) && ((fourcc == V4L2_PIX_FMT_RV40) ||
		(fourcc == V4L2_PIX_FMT_RV30) ||
		(res_chg == false && need_more_output == false))) {
		/*
		 * we only return src buffer with VB2_BUF_STATE_DONE
		 * when decode success without resolution
		 * change except rv30/rv40.
		 */
		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		clean_free_bs_buffer(ctx, NULL);
	} else {    /* res_chg == true || need_more_output == true*/
		clean_free_bs_buffer(ctx, &src_buf_info->bs_buffer);
		mtk_v4l2_debug(1, "Need more capture buffer  r:%d n:%d\n",
			res_chg, need_more_output);
	}

	if (ret == 0 && res_chg) {
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
			mtk_vdec_flush_decoder(ctx, NULL);
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
		ret = vdec_if_get_param(ctx, GET_PARAM_COLOR_DESC, &color_desc);
		if (ret == 0) {
			ctx->is_hdr = color_desc.is_hdr;
			ctx->last_is_hdr = color_desc.is_hdr;
		} else {
			mtk_v4l2_err("[%d] GET_PARAM_COLOR_DESC fail=%d",
				 ctx->id, ret);
		}
	}

	v4l2_m2m_job_finish(dev->m2m_dev_dec, ctx->m2m_ctx);
	do_gettimeofday(&vputvend);
	mtk_vcodec_perf_log("worker:%ld",
		(vputvend.tv_sec - worktvstart.tv_sec) * 1000000 +
		(vputvend.tv_usec - worktvstart.tv_usec));

	mutex_unlock(&ctx->worker_lock);
}

static int vidioc_try_decoder_cmd(struct file *file, void *priv,
	struct v4l2_decoder_cmd *cmd)
{
	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
		cmd->flags = 0; // don't support flags
		break;
	case V4L2_DEC_CMD_START:
		cmd->flags = 0; // don't support flags
		if (cmd->start.speed < 0)
			cmd->start.speed = 0;
		cmd->start.format = V4L2_DEC_START_FMT_NONE;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int vidioc_decoder_cmd(struct file *file, void *priv,
	struct v4l2_decoder_cmd *cmd)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *src_vq, *dst_vq;
	int ret;

	ret = vidioc_try_decoder_cmd(file, priv, cmd);
	if (ret)
		return ret;

	mtk_v4l2_debug(1, "decoder cmd= %u", cmd->cmd);
	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	switch (cmd->cmd) {
	case V4L2_DEC_CMD_STOP:
		src_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		if (!vb2_is_streaming(src_vq)) {
			mtk_v4l2_debug(1, "Output stream is off. No need to flush.");
			return 0;
		}
		if (!vb2_is_streaming(dst_vq)) {
			mtk_v4l2_debug(1, "Capture stream is off. No need to flush.");
			return 0;
		}
		if (ctx->dec_flush_buf->lastframe == NON_EOS) {
			ctx->dec_flush_buf->lastframe = EOS;
			ctx->dec_flush_buf->vb.vb2_buf.planes[0].bytesused = 0;
			v4l2_m2m_buf_queue_check(ctx->m2m_ctx, &ctx->dec_flush_buf->vb);
			v4l2_m2m_try_schedule(ctx->m2m_ctx);
		} else {
			mtk_v4l2_debug(1, "Stopping no need to queue cmd dec_flush_buf.");
		}
		break;

	case V4L2_DEC_CMD_START:
		vb2_clear_last_buffer_dequeued(dst_vq);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

void mtk_vdec_unlock(struct mtk_vcodec_ctx *ctx, u32 hw_id)
{
	if (hw_id >= MTK_VDEC_HW_NUM)
		return;

	mtk_v4l2_debug(4, "ctx %p [%d] hw_id %d sem_cnt %d",
		ctx, ctx->id, hw_id, ctx->dev->dec_sem[hw_id].count);

	if (hw_id < MTK_VDEC_HW_NUM) {
		ctx->hw_locked[hw_id] = 0;
		up(&ctx->dev->dec_sem[hw_id]);
	}
}

int mtk_vdec_lock(struct mtk_vcodec_ctx *ctx, u32 hw_id)
{
	unsigned int suspend_block_cnt = 0;
	int ret = -1;

	if (hw_id >= MTK_VDEC_HW_NUM)
		return -1;

	while (ctx->dev->is_codec_suspending == 1) {
		suspend_block_cnt++;
		if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
			mtk_v4l2_debug(4, "VDEC blocked by suspend\n");
			suspend_block_cnt = 0;
		}
		usleep_range(10000, 20000);
	}

	mtk_v4l2_debug(4, "ctx %p [%d] hw_id %d sem_cnt %d",
		ctx, ctx->id, hw_id, ctx->dev->dec_sem[hw_id].count);

	while (hw_id < MTK_VDEC_HW_NUM && ret != 0)
		ret = down_interruptible(&ctx->dev->dec_sem[hw_id]);

	ctx->hw_locked[hw_id] = 1;

	return ret;
}

void mtk_vcodec_dec_empty_queues(struct file *file, struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *src_buf = NULL, *dst_buf = NULL;
	int i = 0;
	struct v4l2_fh *fh = file->private_data;

	// error handle for release before stream-off
	// stream off both queue mannually.
	v4l2_m2m_streamoff(file, fh->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	v4l2_m2m_streamoff(file, fh->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

	while ((src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
		if (to_vb2_v4l2_buffer(src_buf) != &ctx->dec_flush_buf->vb)
			v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf),
				VB2_BUF_STATE_ERROR);

	while ((dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx))) {
		struct vb2_v4l2_buffer *vb2_v4l2 = NULL;

		for (i = 0; i < dst_buf->num_planes; i++)
			vb2_set_plane_payload(dst_buf, i, 0);

		vb2_v4l2 = container_of(dst_buf,
			struct vb2_v4l2_buffer, vb2_buf);

		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf),
			VB2_BUF_STATE_ERROR);
	}

	ctx->state = MTK_STATE_FREE;
}

void mtk_vcodec_dec_release(struct mtk_vcodec_ctx *ctx)
{
	vdec_if_deinit(ctx);
	vdec_check_release_lock(ctx);
}

void mtk_vcodec_dec_set_default_params(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_q_data *q_data;

	ctx->m2m_ctx->q_lock = &ctx->dev->dev_mutex;
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	INIT_WORK(&ctx->decode_work, mtk_vdec_worker);
	ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;

	get_supported_format(ctx);

	q_data = &ctx->q_data[MTK_Q_DATA_SRC];
	memset(q_data, 0, sizeof(struct mtk_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	if (default_out_fmt_idx < MTK_MAX_DEC_CODECS_SUPPORT)
		q_data->fmt = &mtk_video_formats[default_out_fmt_idx];
	q_data->field = V4L2_FIELD_NONE;

	q_data->sizeimage[0] = DFT_CFG_WIDTH * DFT_CFG_HEIGHT;
	q_data->bytesperline[0] = 0;

	q_data = &ctx->q_data[MTK_Q_DATA_DST];
	memset(q_data, 0, sizeof(struct mtk_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->coded_width = DFT_CFG_WIDTH;
	q_data->coded_height = DFT_CFG_HEIGHT;
	if (default_cap_fmt_idx < MTK_MAX_DEC_CODECS_SUPPORT)
		q_data->fmt = &mtk_video_formats[default_cap_fmt_idx];
	q_data->field = V4L2_FIELD_NONE;

	v4l_bound_align_image(&q_data->coded_width,
						  MTK_VDEC_MIN_W,
						  MTK_VDEC_MAX_W, 4,
						  &q_data->coded_height,
						  MTK_VDEC_MIN_H,
						  MTK_VDEC_MAX_H, 5, 6);

	if (q_data->fmt->num_planes == 1) {
		q_data->sizeimage[0] =
			q_data->coded_width * q_data->coded_height * 3/2;
		q_data->bytesperline[0] = q_data->coded_width;

	} else if (q_data->fmt->num_planes == 2) {
		q_data->sizeimage[0] =
			q_data->coded_width * q_data->coded_height;
		q_data->bytesperline[0] = q_data->coded_width;
		q_data->sizeimage[1] = q_data->sizeimage[0] / 2;
		q_data->bytesperline[1] = q_data->coded_width;
	}
}

static int mtk_vdec_set_param(struct mtk_vcodec_ctx *ctx)
{
	unsigned long in[8] = {0};

	mtk_v4l2_debug(4,
				   "[%d] param change %d decode mode %d frame width %d frame height %d max width %d max height %d",
				   ctx->id, ctx->dec_param_change,
				   ctx->dec_params.decode_mode,
				   ctx->dec_params.frame_size_width,
				   ctx->dec_params.frame_size_height,
				   ctx->dec_params.fixed_max_frame_size_width,
				   ctx->dec_params.fixed_max_frame_size_height);

	if (ctx->dec_param_change & MTK_DEC_PARAM_DECODE_MODE) {
		in[0] = ctx->dec_params.decode_mode;
		if (vdec_if_set_param(ctx, SET_PARAM_DECODE_MODE, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_DECODE_MODE);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_FRAME_SIZE) {
		in[0] = ctx->dec_params.frame_size_width;
		in[1] = ctx->dec_params.frame_size_height;
		if (in[0] != 0 && in[1] != 0) {
			if (vdec_if_set_param(ctx,
				SET_PARAM_FRAME_SIZE, in) != 0) {
				mtk_v4l2_err("[%d] Error!! Cannot set param",
					ctx->id);
				return -EINVAL;
			}
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_FRAME_SIZE);
	}

	if (ctx->dec_param_change &
		MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE) {
		in[0] = ctx->dec_params.fixed_max_frame_size_width;
		in[1] = ctx->dec_params.fixed_max_frame_size_height;
		if (in[0] != 0 && in[1] != 0) {
			if (vdec_if_set_param(ctx,
				SET_PARAM_SET_FIXED_MAX_OUTPUT_BUFFER,
				in) != 0) {
				mtk_v4l2_err("[%d] Error!! Cannot set param",
					ctx->id);
				return -EINVAL;
			}
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_CRC_PATH) {
		in[0] = (unsigned long)ctx->dec_params.crc_path;
		if (vdec_if_set_param(ctx, SET_PARAM_CRC_PATH, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_CRC_PATH);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_GOLDEN_PATH) {
		in[0] = (unsigned long)ctx->dec_params.golden_path;
		if (vdec_if_set_param(ctx, SET_PARAM_GOLDEN_PATH, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_GOLDEN_PATH);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_WAIT_KEY_FRAME) {
		in[0] = (unsigned long)ctx->dec_params.wait_key_frame;
		if (vdec_if_set_param(ctx, SET_PARAM_WAIT_KEY_FRAME, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_WAIT_KEY_FRAME);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_NAL_SIZE_LENGTH) {
		in[0] = (unsigned long)ctx->dec_params.wait_key_frame;
		if (vdec_if_set_param(ctx, SET_PARAM_NAL_SIZE_LENGTH,
					in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_NAL_SIZE_LENGTH);
	}

	if (ctx->dec_param_change & MTK_DEC_PARAM_OPERATING_RATE) {
		in[0] = (unsigned long)ctx->dec_params.operating_rate;
		if (vdec_if_set_param(ctx, SET_PARAM_OPERATING_RATE, in) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot set param", ctx->id);
			return -EINVAL;
		}
		ctx->dec_param_change &= (~MTK_DEC_PARAM_OPERATING_RATE);
	}

	if (vdec_if_get_param(ctx, GET_PARAM_INPUT_DRIVEN,
		&ctx->input_driven)) {
		mtk_v4l2_err("[%d] Error!! Cannot get param : GET_PARAM_INPUT_DRIVEN ERR",
					 ctx->id);
		return -EINVAL;
	}

	return 0;
}

static int vidioc_vdec_qbuf(struct file *file, void *priv,
							struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_video_dec_buf *mtkbuf;
	struct vb2_v4l2_buffer  *vb2_v4l2;

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_err("[%d] Call on QBUF after unrecoverable error",
					 ctx->id);
		return -EIO;
	}

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, buf->type);
	if (buf->index >= vq->num_buffers) {
		mtk_v4l2_err("[%d] buffer index %d out of range %d",
			ctx->id, buf->index, vq->num_buffers);
		return -EINVAL;
	}

	vb = vq->bufs[buf->index];
	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	mtkbuf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->input_max_ts =
			(timeval_to_ns(&buf->timestamp) > ctx->input_max_ts) ?
			timeval_to_ns(&buf->timestamp) : ctx->input_max_ts;
		if (buf->m.planes[0].bytesused == 0) {
			mtkbuf->lastframe = EOS;
			mtk_v4l2_debug(1, "[%d] index=%d Eos BS(%d,%d) vb=%p pts=%llu",
				ctx->id, buf->index,
				buf->bytesused,
				buf->length, vb,
				timeval_to_ns(&buf->timestamp));
		} else if (buf->flags & V4L2_BUF_FLAG_LAST) {
			mtkbuf->lastframe = EOS_WITH_DATA;
			mtk_v4l2_debug(1, "[%d] id=%d EarlyEos BS(%d,%d) vb=%p pts=%llu",
				ctx->id, buf->index, buf->m.planes[0].bytesused,
				buf->length, vb,
				timeval_to_ns(&buf->timestamp));
		} else {
			mtkbuf->lastframe = NON_EOS;
			mtk_v4l2_debug(1, "[%d] id=%d getdata BS(%d,%d) vb=%p pts=%llu %llu",
				ctx->id, buf->index,
				buf->m.planes[0].bytesused,
				buf->length, vb,
				timeval_to_ns(&buf->timestamp),
				ctx->input_max_ts);
		}
	} else {
		if (buf->reserved == 0xFFFFFFFF)
			mtkbuf->general_user_fd = -1;
		else
			mtkbuf->general_user_fd = (int)buf->reserved;
		mtk_v4l2_debug(1, "[%d] id=%d FB (%d) vb=%p, general_buf_fd=%d, mtkbuf->general_buf_fd = %d",
				ctx->id, buf->index,
				buf->length, mtkbuf,
				buf->reserved, mtkbuf->general_user_fd);
	}

	if (buf->flags & V4L2_BUF_FLAG_NO_CACHE_CLEAN) {
		mtk_v4l2_debug(4, "[%d] No need for Cache clean, buf->index:%d. mtkbuf:%p",
			ctx->id, buf->index, mtkbuf);
		mtkbuf->flags |= NO_CAHCE_CLEAN;
	}

	if (buf->flags & V4L2_BUF_FLAG_NO_CACHE_INVALIDATE) {
		mtk_v4l2_debug(4, "[%d] No need for Cache invalidate, buf->index:%d. mtkbuf:%p",
			ctx->id, buf->index, mtkbuf);
		mtkbuf->flags |= NO_CAHCE_INVALIDATE;
	}

	return v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_vdec_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_video_dec_buf *mtkbuf;
	struct vb2_v4l2_buffer  *vb2_v4l2;

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_debug(4, "[%d] Call on DQBUF after unrecoverable error",
					 ctx->id);
		return -EIO;
	}

	ret = v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
	if (ctx->errormap_info[buf->index % VB2_MAX_FRAME])
		buf->flags |= V4L2_BUF_FLAG_ERROR;

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
		ret == 0) {
		vq = v4l2_m2m_get_vq(ctx->m2m_ctx, buf->type);
		if (buf->index >= vq->num_buffers) {
			mtk_v4l2_err("[%d] buffer index %d out of range %d",
				ctx->id, buf->index, vq->num_buffers);
			return -EINVAL;
		}
		vb = vq->bufs[buf->index];
		vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
		mtkbuf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);

		if (mtkbuf->flags & CROP_CHANGED)
			buf->flags |= V4L2_BUF_FLAG_CROP_CHANGED;
		if (mtkbuf->flags & REF_FREED)
			buf->flags |= V4L2_BUF_FLAG_REF_FREED;
		if (mtkbuf->general_user_fd < 0)
			buf->reserved = 0xFFFFFFFF;
		else
			buf->reserved = mtkbuf->general_user_fd;
		mtk_v4l2_debug(2,
			"dqbuf mtkbuf->general_buf_fd = %d",
			mtkbuf->general_user_fd);
	}

	return ret;
}

static int vidioc_vdec_querycap(struct file *file, void *priv,
	struct v4l2_capability *cap)
{
	strlcpy(cap->driver, MTK_VCODEC_DEC_NAME, sizeof(cap->driver));
	strlcpy(cap->bus_info, MTK_PLATFORM_STR, sizeof(cap->bus_info));
	strlcpy(cap->card, MTK_PLATFORM_STR, sizeof(cap->card));

	cap->device_caps  = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

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
	case V4L2_EVENT_MTK_VDEC_ERROR:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	case V4L2_EVENT_MTK_VDEC_NOHEADER:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static int vidioc_try_fmt(struct v4l2_format *f, struct mtk_video_fmt *fmt)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	unsigned int i;

	pix_fmt_mp->field = V4L2_FIELD_NONE;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		pix_fmt_mp->num_planes = 1;
		pix_fmt_mp->plane_fmt[0].bytesperline = 0;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		int tmp_w, tmp_h;

		pix_fmt_mp->height = clamp(pix_fmt_mp->height,
			MTK_VDEC_MIN_H,
			MTK_VDEC_MAX_H);
		pix_fmt_mp->width = clamp(pix_fmt_mp->width,
			MTK_VDEC_MIN_W,
			MTK_VDEC_MAX_W);

		/*
		 * Find next closer width align 64, heign align 64, size align
		 * 64 rectangle
		 * Note: This only get default value, the real HW needed value
		 *       only available when ctx in MTK_STATE_HEADER state
		 */
		tmp_w = pix_fmt_mp->width;
		tmp_h = pix_fmt_mp->height;
		v4l_bound_align_image(&pix_fmt_mp->width,
							  MTK_VDEC_MIN_W,
							  MTK_VDEC_MAX_W, 6,
							  &pix_fmt_mp->height,
							  MTK_VDEC_MIN_H,
							  MTK_VDEC_MAX_H, 6, 9);

		if (pix_fmt_mp->width < tmp_w &&
			(pix_fmt_mp->width + 64) <= MTK_VDEC_MAX_W)
			pix_fmt_mp->width += 64;
		if (pix_fmt_mp->height < tmp_h &&
			(pix_fmt_mp->height + 64) <= MTK_VDEC_MAX_H)
			pix_fmt_mp->height += 64;

		mtk_v4l2_debug(0,
			"before resize width=%d, height=%d, after resize width=%d, height=%d, sizeimage=%d",
			tmp_w, tmp_h, pix_fmt_mp->width,
			pix_fmt_mp->height,
			pix_fmt_mp->width * pix_fmt_mp->height);

		if (fmt->num_planes > 2)
			pix_fmt_mp->num_planes = 2;
		else
			pix_fmt_mp->num_planes = fmt->num_planes;

		pix_fmt_mp->plane_fmt[0].sizeimage =
				pix_fmt_mp->width * pix_fmt_mp->height;
		pix_fmt_mp->plane_fmt[0].bytesperline = pix_fmt_mp->width;

		if (pix_fmt_mp->num_planes == 2) {
			pix_fmt_mp->plane_fmt[1].sizeimage =
				(pix_fmt_mp->width * pix_fmt_mp->height) / 2;
			pix_fmt_mp->plane_fmt[1].bytesperline =
				pix_fmt_mp->width;
		} else if (pix_fmt_mp->num_planes == 1) {
			pix_fmt_mp->plane_fmt[0].sizeimage +=
				(pix_fmt_mp->width * pix_fmt_mp->height) / 2;
		}

	}

	for (i = 0; i < pix_fmt_mp->num_planes; i++)
		memset(&(pix_fmt_mp->plane_fmt[i].reserved[0]), 0x0,
			   sizeof(pix_fmt_mp->plane_fmt[0].reserved));

	pix_fmt_mp->flags = 0;
	memset(&pix_fmt_mp->reserved, 0x0, sizeof(pix_fmt_mp->reserved));
	return 0;
}

static int vidioc_vdec_g_crop(struct file *file, void *priv,
							  struct v4l2_crop *cr)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (!V4L2_TYPE_IS_VALID_CROP(cr->type)) {
		mtk_v4l2_debug(2, "[%d]Error!! Crop Buf type not allowed ERR",
					cr->type);
		return -EINVAL;
	}

	if (ctx->state < MTK_STATE_HEADER) {
		struct mtk_q_data *q_data;

		q_data = &ctx->q_data[MTK_Q_DATA_DST];
		/* set to default value if header info not ready yet*/
		cr->c.left = 0;
		cr->c.top = 0;
		cr->c.width = q_data->visible_width;
		cr->c.height = q_data->visible_height;
		return 0;
	}

	if ((ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H264) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H265) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_HEIF) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP8) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_VP9) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG1) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG2) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_MPEG4) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_H263) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMV1) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMV2) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMV3) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WVC1) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_WMVA) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_RV30) ||
	  (ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc == V4L2_PIX_FMT_RV40)) {
		if (vdec_if_get_param(ctx, GET_PARAM_CROP_INFO, cr) != 0) {
			mtk_v4l2_debug(2, "[%d]Error!! Cannot get param : GET_PARAM_CROP_INFO ERR",
						   ctx->id);
			cr->c.left = 0;
			cr->c.top = 0;
			cr->c.width = ctx->picinfo.pic_w;
			cr->c.height = ctx->picinfo.pic_h;
		}
		mtk_v4l2_debug(2, "Cropping info: l=%d t=%d w=%d h=%d",
					   cr->c.left, cr->c.top, cr->c.width,
					   cr->c.height);
	} else {
		cr->c.left = 0;
		cr->c.top = 0;
		cr->c.width = ctx->picinfo.pic_w;
		cr->c.height = ctx->picinfo.pic_h;
		mtk_v4l2_debug(2, "Cropping info: w=%d h=%d fw=%d fh=%d",
			cr->c.width, cr->c.height,
			ctx->picinfo.buf_w,
			ctx->picinfo.buf_h);
	}

	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
	struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	fmt = mtk_vdec_find_format(ctx, f, MTK_FMT_FRAME);
	if (!fmt && default_cap_fmt_idx < MTK_MAX_DEC_CODECS_SUPPORT) {
		f->fmt.pix.pixelformat =
			mtk_video_formats[default_cap_fmt_idx].fourcc;
		fmt = mtk_vdec_find_format(ctx, f, MTK_FMT_FRAME);
	}
	if (!fmt)
		return -EINVAL;

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_try_fmt_vid_out_mplane(struct file *file, void *priv,
	struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct mtk_video_fmt *fmt;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	fmt = mtk_vdec_find_format(ctx, f, MTK_FMT_DEC);
	if (!fmt && default_out_fmt_idx < MTK_MAX_DEC_CODECS_SUPPORT) {
		f->fmt.pix.pixelformat =
			mtk_video_formats[default_out_fmt_idx].fourcc;
		fmt = mtk_vdec_find_format(ctx, f, MTK_FMT_DEC);
	}

	if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
		mtk_v4l2_err("sizeimage of output format must be given");
		return -EINVAL;
	}
	if (!fmt)
		return -EINVAL;

	return vidioc_try_fmt(f, fmt);
}

static int vidioc_vdec_g_selection(struct file *file, void *priv,
	struct v4l2_selection *s)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct mtk_q_data *q_data;

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	q_data = &ctx->q_data[MTK_Q_DATA_DST];

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.pic_w;
		s->r.height = ctx->picinfo.pic_h;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_CROP_BOUNDS:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.buf_w;
		s->r.height = ctx->picinfo.buf_h;
		break;
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_CROP:
		if (vdec_if_get_param(ctx, GET_PARAM_CROP_INFO, &(s->r))) {
			/* set to default value if header info not ready yet*/
			s->r.left = 0;
			s->r.top = 0;
			s->r.width = q_data->visible_width;
			s->r.height = q_data->visible_height;
		}
		break;
	default:
		return -EINVAL;
	}

	if (ctx->state < MTK_STATE_HEADER) {
		/* set to default value if header info not ready yet*/
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = q_data->visible_width;
		s->r.height = q_data->visible_height;
		return 0;
	}

	return 0;
}

static int vidioc_vdec_s_selection(struct file *file, void *priv,
	struct v4l2_selection *s)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_CROP:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->picinfo.pic_w;
		s->r.height = ctx->picinfo.pic_h;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int vidioc_vdec_s_fmt(struct file *file, void *priv,
							 struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp;
	struct mtk_q_data *q_data;
	int ret = 0;
	struct mtk_video_fmt *fmt;
	uint64_t size[2];

	mtk_v4l2_debug(4, "[%d] type %d", ctx->id, f->type);

	q_data = mtk_vdec_get_q_data(ctx, f->type);
	if (!q_data)
		return -EINVAL;

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

	fmt = mtk_vdec_find_format(ctx, f,
		(f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) ?
		MTK_FMT_DEC : MTK_FMT_FRAME);
	if (fmt == NULL) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
			&& default_out_fmt_idx < MTK_MAX_DEC_CODECS_SUPPORT) {
			f->fmt.pix.pixelformat =
				mtk_video_formats[default_out_fmt_idx].fourcc;
			fmt = mtk_vdec_find_format(ctx, f, MTK_FMT_DEC);
		} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
			&& default_cap_fmt_idx < MTK_MAX_DEC_CODECS_SUPPORT) {
			f->fmt.pix.pixelformat =
				mtk_video_formats[default_cap_fmt_idx].fourcc;
			fmt = mtk_vdec_find_format(ctx, f, MTK_FMT_FRAME);
		}
	}
	if (!fmt)
		return -EINVAL;

	q_data->fmt = fmt;
	vidioc_try_fmt(f, q_data->fmt);

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q_data->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
		q_data->coded_width = pix_mp->width;
		q_data->coded_height = pix_mp->height;
		size[0] = pix_mp->width;
		size[1] = pix_mp->height;

		ctx->colorspace = f->fmt.pix_mp.colorspace;
		ctx->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
		ctx->quantization = f->fmt.pix_mp.quantization;
		ctx->xfer_func = f->fmt.pix_mp.xfer_func;

		if (ctx->state == MTK_STATE_FREE) {
			ret = vdec_if_init(ctx, q_data->fmt->fourcc);
			v4l2_m2m_set_dst_buffered(ctx->m2m_ctx,
				ctx->input_driven);
			if (ctx->input_driven)
				init_waitqueue_head(&ctx->fm_wq);
			if (ret) {
				mtk_v4l2_err("[%d]: vdec_if_init() fail ret=%d",
							 ctx->id, ret);
				ctx->state = MTK_STATE_ABORT;
				return -EINVAL;
			}
			vdec_if_set_param(ctx,
				SET_PARAM_FRAME_SIZE, (void *) size);
			ctx->state = MTK_STATE_INIT;
		}
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		vdec_if_set_param(ctx, SET_PARAM_FB_NUM_PLANES,
			(void *) &q_data->fmt->num_planes);

	return 0;
}

static int vidioc_enum_framesizes(struct file *file, void *priv,
	struct v4l2_frmsizeenum *fsize)
{
	int i = 0;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (fsize->index != 0)
		return -EINVAL;

	if (mtk_vdec_framesizes[0].fourcc == 0) {
		if (vdec_if_get_param(ctx, GET_PARAM_CAPABILITY_FRAME_SIZES,
			&mtk_vdec_framesizes) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get frame size",
				ctx->id);
			return -EINVAL;
		}

		for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT; i++) {
			if (mtk_vdec_framesizes[i].fourcc != 0) {
				mtk_v4l2_debug(1,
				"vdec_fs[%d] fourcc %d s %d %d %d %d %d %d\n",
				i, mtk_vdec_framesizes[i].fourcc,
				mtk_vdec_framesizes[i].stepwise.min_width,
				mtk_vdec_framesizes[i].stepwise.max_width,
				mtk_vdec_framesizes[i].stepwise.step_width,
				mtk_vdec_framesizes[i].stepwise.min_height,
				mtk_vdec_framesizes[i].stepwise.max_height,
				mtk_vdec_framesizes[i].stepwise.step_height);
			}
		}
	}

	for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT &&
		 mtk_vdec_framesizes[i].fourcc != 0; ++i) {
		if (fsize->pixel_format != mtk_vdec_framesizes[i].fourcc)
			continue;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->reserved[0] = mtk_vdec_framesizes[i].profile;
		fsize->reserved[1] = mtk_vdec_framesizes[i].level;
		fsize->stepwise = mtk_vdec_framesizes[i].stepwise;
		if (!(ctx->dev->dec_capability &
			  VCODEC_CAPABILITY_4K_DISABLED)) {
			mtk_v4l2_debug(1, "4K is enabled");
			fsize->stepwise.max_width =
				VCODEC_DEC_4K_CODED_WIDTH;
			fsize->stepwise.max_height =
				VCODEC_DEC_4K_CODED_HEIGHT;
		}
		mtk_v4l2_debug(1, "%x, %d %d %d %d %d %d %d %d",
					   ctx->dev->dec_capability,
					   fsize->stepwise.min_width,
					   fsize->stepwise.max_width,
					   fsize->stepwise.step_width,
					   fsize->stepwise.min_height,
					   fsize->stepwise.max_height,
					   fsize->stepwise.step_height,
					   fsize->reserved[0],
					   fsize->reserved[1]);
		return 0;
	}

	return -EINVAL;
}

static int vidioc_enum_fmt(struct mtk_vcodec_ctx *ctx, struct v4l2_fmtdesc *f,
	bool output_queue)
{
	struct mtk_video_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < MTK_MAX_DEC_CODECS_SUPPORT &&
		 mtk_video_formats[i].fourcc != 0; i++) {
		if ((output_queue == true) &&
			(mtk_video_formats[i].type != MTK_FMT_DEC))
			continue;
		else if ((output_queue == false) &&
				 (mtk_video_formats[i].type != MTK_FMT_FRAME))
			continue;

		if (j == f->index)
			break;
		++j;
	}

	if (i == MTK_MAX_DEC_CODECS_SUPPORT ||
		mtk_video_formats[i].fourcc == 0)
		return -EINVAL;

	fmt = &mtk_video_formats[i];

	f->pixelformat = fmt->fourcc;
	f->flags = 0;
	memset(f->reserved, 0, sizeof(f->reserved));

	if (mtk_video_formats[i].type != MTK_FMT_DEC)
		f->flags |= V4L2_FMT_FLAG_COMPRESSED;

	return 0;
}

static int vidioc_vdec_enum_fmt_vid_cap_mplane(struct file *file, void *priv,
	struct v4l2_fmtdesc *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	return vidioc_enum_fmt(ctx, f, false);
}

static int vidioc_vdec_enum_fmt_vid_out_mplane(struct file *file, void *priv,
	struct v4l2_fmtdesc *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	return vidioc_enum_fmt(ctx, f, true);
}

static int vidioc_vdec_g_fmt(struct file *file, void *priv,
							 struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	u32     fourcc;
	unsigned int i = 0;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		mtk_v4l2_err("no vb2 queue for type=%d", f->type);
		return -EINVAL;
	}

	q_data = mtk_vdec_get_q_data(ctx, f->type);

	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->colorspace = ctx->colorspace;
	pix_mp->ycbcr_enc = ctx->ycbcr_enc;
	pix_mp->quantization = ctx->quantization;
	pix_mp->xfer_func = ctx->xfer_func;

	if ((f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
		(ctx->state >= MTK_STATE_HEADER)) {
		/* Until STREAMOFF is called on the CAPTURE queue
		 * (acknowledging the event), the driver operates as if
		 * the resolution hasn't changed yet.
		 * So we just return picinfo yet, and update picinfo in
		 * stop_streaming hook function
		 */
		for (i = 0; i < q_data->fmt->num_planes; i++) {
			q_data->sizeimage[i] = ctx->picinfo.fb_sz[i];
			q_data->bytesperline[i] =
				ctx->last_decoded_picinfo.buf_w;
		}
		q_data->coded_width = ctx->picinfo.buf_w;
		q_data->coded_height = ctx->picinfo.buf_h;
		fourcc = ctx->picinfo.fourcc;
		q_data->fmt = mtk_find_fmt_by_pixel(fourcc);

		/*
		 * Width and height are set to the dimensions
		 * of the movie, the buffer is bigger and
		 * further processing stages should crop to this
		 * rectangle.
		 */
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
		if (fourcc == V4L2_PIX_FMT_RV30 ||
			fourcc == V4L2_PIX_FMT_RV40) {
			pix_mp->width = 1920;
			pix_mp->height = 1088;
		} else {
			pix_mp->width = q_data->coded_width;
			pix_mp->height = q_data->coded_height;
		}
		/*
		 * Set pixelformat to the format in which mt vcodec
		 * outputs the decoded frame
		 */
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;

		if (fourcc == V4L2_PIX_FMT_RV30 ||
			fourcc == V4L2_PIX_FMT_RV40) {
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->plane_fmt[i].bytesperline = 1920;
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		} else {
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->plane_fmt[i].bytesperline =
					q_data->bytesperline[i];
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		}

		mtk_v4l2_debug(1, "fourcc:(%d %d),bytesperline:%d,sizeimage:%d,%d,%d\n",
			ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc,
			q_data->fmt->fourcc,
			pix_mp->plane_fmt[0].bytesperline,
			pix_mp->plane_fmt[0].sizeimage,
			pix_mp->plane_fmt[1].bytesperline,
			pix_mp->plane_fmt[1].sizeimage);

	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/*
		 * This is run on OUTPUT
		 * The buffer contains compressed image
		 * so width and height have no meaning.
		 * Assign value here to pass v4l2-compliance test
		 */
		pix_mp->width = q_data->visible_width;
		pix_mp->height = q_data->visible_height;
		pix_mp->plane_fmt[0].bytesperline = q_data->bytesperline[0];
		pix_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_mp->pixelformat = q_data->fmt->fourcc;
		pix_mp->num_planes = q_data->fmt->num_planes;
	} else {
		pix_mp->num_planes = q_data->fmt->num_planes;
		pix_mp->pixelformat = q_data->fmt->fourcc;
		fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;

		if (fourcc == V4L2_PIX_FMT_RV30 ||
			fourcc == V4L2_PIX_FMT_RV40) {
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->width = 1920;
				pix_mp->height = 1088;
				pix_mp->plane_fmt[i].bytesperline = 1920;
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		} else {
			pix_mp->width = q_data->coded_width;
			pix_mp->height = q_data->coded_height;
			for (i = 0; i < pix_mp->num_planes; i++) {
				pix_mp->plane_fmt[i].bytesperline =
					q_data->bytesperline[i];
				pix_mp->plane_fmt[i].sizeimage =
					q_data->sizeimage[i];
			}
		}

		mtk_v4l2_debug(1,
					   " [%d] type=%d state=%d Format information could not be read, not ready yet!",
					   ctx->id, f->type, ctx->state);
	}

	return 0;
}

static int vb2ops_vdec_queue_setup(struct vb2_queue *vq,
	unsigned int *nbuffers,
	unsigned int *nplanes,
	unsigned int sizes[],
	struct device *alloc_devs[])
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_q_data *q_data;
	unsigned int i;

	q_data = mtk_vdec_get_q_data(ctx, vq->type);

	if (q_data == NULL) {
		mtk_v4l2_err("vq->type=%d err\n", vq->type);
		return -EINVAL;
	}

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++) {
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
		}
	} else {
		if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			*nplanes = q_data->fmt->num_planes;
		else
			*nplanes = 1;

		for (i = 0; i < *nplanes; i++)
			sizes[i] = q_data->sizeimage[i];
	}

	mtk_v4l2_debug(1,
				   "[%d]\t type = %d, get %d plane(s), %d buffer(s) of size 0x%x 0x%x ",
				   ctx->id, vq->type, *nplanes, *nbuffers,
				   sizes[0], sizes[1]);

	return 0;
}

static int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_q_data *q_data;
	struct dma_buf_attachment *buf_att;
	struct sg_table *sgt;
	unsigned int plane = 0;
	unsigned int i;
	struct mtk_video_dec_buf *mtkbuf;
	struct vb2_v4l2_buffer *vb2_v4l2;

	mtk_v4l2_debug(4, "[%d] (%d) id=%d",
				   ctx->id, vb->vb2_queue->type, vb->index);

	q_data = mtk_vdec_get_q_data(ctx, vb->vb2_queue->type);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			mtk_v4l2_err("data will not fit into plane %d (%lu < %d)",
						 i, vb2_plane_size(vb, i),
						 q_data->sizeimage[i]);
		}
	}

	// Check if need to proceed cache operations
	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	mtkbuf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (mtkbuf->general_user_fd > -1) {
			struct dma_buf_attachment *buf_att;
			struct sg_table *sgt;

			mtkbuf->frame_buffer.dma_general_buf =
				dma_buf_get(mtkbuf->general_user_fd);

			if (IS_ERR(mtkbuf->frame_buffer.dma_general_buf)) {
				mtk_v4l2_err("%s dma_general_buf is err 0x%p.\n",
					__func__,
					mtkbuf->frame_buffer.dma_general_buf);

				mtk_vdec_queue_error_event(ctx);
				return -EINVAL;
			}

			buf_att = dma_buf_attach(
				mtkbuf->frame_buffer.dma_general_buf,
				&ctx->dev->plat_dev->dev);
			sgt = dma_buf_map_attachment(buf_att, DMA_TO_DEVICE);
			if (IS_ERR_OR_NULL(sgt)) {
				mtk_v4l2_err("dma_buf_map_attachment fail %d.\n", sgt);
				dma_buf_detach(mtkbuf->frame_buffer.dma_general_buf,
					buf_att);
				return -EINVAL;
			}
			mtkbuf->frame_buffer.dma_general_addr =
				sg_dma_address(sgt->sgl);
			dma_buf_unmap_attachment(buf_att, sgt, DMA_TO_DEVICE);
			dma_buf_detach(mtkbuf->frame_buffer.dma_general_buf,
				buf_att);

		} else
			mtkbuf->frame_buffer.dma_general_buf = 0;
		mtk_v4l2_debug(4,
			"dma_buf_get general_buf fd=%d, dma_buf=%p, DMA=%lx",
			mtkbuf->general_user_fd,
			mtkbuf->frame_buffer.dma_general_buf,
			(unsigned long)mtkbuf->frame_buffer.dma_general_addr);
	}
	if (vb->vb2_queue->memory == VB2_MEMORY_DMABUF &&
		!(mtkbuf->flags & NO_CAHCE_CLEAN) &&
		!(ctx->dec_params.svp_mode)) {
		if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			struct mtk_vcodec_mem src_mem;

			mtk_v4l2_debug(4, "[%d] Cache sync+", ctx->id);

			buf_att = dma_buf_attach(vb->planes[0].dbuf,
				&ctx->dev->plat_dev->dev);

			sgt = dma_buf_map_attachment(buf_att, DMA_TO_DEVICE);
			if (IS_ERR_OR_NULL(sgt)) {
				mtk_v4l2_err("dma_buf_map_attachment fail %d.\n", sgt);
				dma_buf_detach(vb->planes[0].dbuf, buf_att);
				return -EINVAL;
			}
			dma_sync_sg_for_device(&ctx->dev->plat_dev->dev,
				sgt->sgl,
				sgt->orig_nents,
				DMA_TO_DEVICE);
			dma_buf_unmap_attachment(buf_att, sgt, DMA_TO_DEVICE);

			src_mem.dma_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
			src_mem.size = (size_t)vb->planes[0].bytesused;
			dma_buf_detach(vb->planes[0].dbuf, buf_att);

			mtk_v4l2_debug(4,
			   "[%d] Cache sync- TD for %lx sz=%d dev %p",
			   ctx->id,
			   (unsigned long)src_mem.dma_addr,
			   (unsigned int)src_mem.size,
			   &ctx->dev->plat_dev->dev);
		} else {
			for (plane = 0; plane < vb->num_planes; plane++) {
				struct vdec_fb dst_mem;

				mtk_v4l2_debug(4, "[%d] Cache sync+", ctx->id);

				buf_att = dma_buf_attach(vb->planes[plane].dbuf,
					&ctx->dev->plat_dev->dev);
				sgt = dma_buf_map_attachment(buf_att,
					DMA_TO_DEVICE);
				if (IS_ERR_OR_NULL(sgt)) {
					mtk_v4l2_err("dma_buf_map_attachment fail %d.\n", sgt);
					dma_buf_detach(vb->planes[plane].dbuf, buf_att);
					return -EINVAL;
				}
				dma_sync_sg_for_device(&ctx->dev->plat_dev->dev,
					sgt->sgl,
					sgt->orig_nents,
					DMA_TO_DEVICE);
				dma_buf_unmap_attachment(buf_att,
					sgt, DMA_TO_DEVICE);

				dst_mem.fb_base[plane].dma_addr =
					vb2_dma_contig_plane_dma_addr(vb,
					plane);
				dst_mem.fb_base[plane].size =
					ctx->picinfo.fb_sz[plane];
				dma_buf_detach(vb->planes[plane].dbuf, buf_att);

				mtk_v4l2_debug(4,
				 "[%d] Cache sync- TD for %lx sz=%d dev %p",
				 ctx->id,
				 (unsigned long)dst_mem.fb_base[plane].dma_addr,
				 (unsigned int)dst_mem.fb_base[plane].size,
				 &ctx->dev->plat_dev->dev);
			}
		}
	}
	return 0;
}

static void vb2ops_vdec_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_buffer *src_buf;
	struct mtk_vcodec_mem *src_mem;
	unsigned int src_chg = 0;
	bool res_chg = false;
	bool mtk_vcodec_unsupport = false;
	bool need_seq_header = false;
	int ret = 0;
	unsigned long frame_size[2];
	unsigned int i = 0;
	unsigned int dpbsize = 1;
	unsigned int bs_fourcc, fm_fourcc;
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct mtk_video_dec_buf *buf = NULL;
	struct mtk_q_data *dst_q_data;
	u32 fourcc;
	int last_frame_type = 0;
	struct mtk_color_desc color_desc;
	struct vb2_queue *dst_vq;

	mtk_v4l2_debug(4, "[%d] (%d) id=%d, vb=%p",
				   ctx->id, vb->vb2_queue->type,
				   vb->index, vb);
	/*
	 * check if this buffer is ready to be used after decode
	 */
	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		vb2_v4l2 = to_vb2_v4l2_buffer(vb);
		buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
		mutex_lock(&ctx->buf_lock);
		if (buf->used == false) {
			v4l2_m2m_buf_queue_check(ctx->m2m_ctx, vb2_v4l2);
			buf->queued_in_vb2 = true;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;
		} else {
			buf->queued_in_vb2 = false;
			buf->queued_in_v4l2 = true;
			buf->ready_to_display = false;
		}
		mutex_unlock(&ctx->buf_lock);
		if (ctx->input_driven)
			wake_up(&ctx->fm_wq);
		return;
	}

	v4l2_m2m_buf_queue_check(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));

	if (ctx->state != MTK_STATE_INIT) {
		mtk_v4l2_debug(4, "[%d] already init driver %d",
					   ctx->id, ctx->state);
		return;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (!src_buf ||
		to_vb2_v4l2_buffer(src_buf) == &ctx->dec_flush_buf->vb) {
		mtk_v4l2_err("No src buffer %p", src_buf);
		return;
	}

	vb2_v4l2 = to_vb2_v4l2_buffer(vb);
	buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
	src_mem = &buf->bs_buffer;
	src_mem->va = vb2_plane_vaddr(src_buf, 0);
	src_mem->dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	src_mem->size = (size_t)src_buf->planes[0].bytesused;
	src_mem->length = (size_t)src_buf->planes[0].length;
	src_mem->dmabuf = src_buf->planes[0].dbuf;
	src_mem->flags = vb2_v4l2->flags;
	src_mem->index = vb->index;

	mtk_v4l2_debug(2,
		"[%d] buf id=%d va=%p DMA=%lx size=%zx length=%zu dmabuf=%p",
		ctx->id, src_buf->index,
		src_mem->va, (unsigned long)src_mem->dma_addr,
		src_mem->size, src_mem->length,
		src_mem->dmabuf);

	if (src_mem->va != NULL) {
		mtk_v4l2_debug(0, "[%d] %x %x %x %x %x %x %x %x %x\n",
					   ctx->id,
					   ((char *)src_mem->va)[0],
					   ((char *)src_mem->va)[1],
					   ((char *)src_mem->va)[2],
					   ((char *)src_mem->va)[3],
					   ((char *)src_mem->va)[4],
					   ((char *)src_mem->va)[5],
					   ((char *)src_mem->va)[6],
					   ((char *)src_mem->va)[7],
					   ((char *)src_mem->va)[8]);
	}

	frame_size[0] = ctx->dec_params.frame_size_width;
	frame_size[1] = ctx->dec_params.frame_size_height;

	vdec_if_set_param(ctx, SET_PARAM_FRAME_SIZE, frame_size);

	if (ctx->dec_param_change & MTK_DEC_PARAM_DECODE_MODE)
		vdec_if_set_param(ctx, SET_PARAM_DECODE_MODE, &ctx->dec_params.decode_mode);

	ret = vdec_if_decode(ctx, src_mem, NULL, &src_chg);
	mtk_vdec_set_param(ctx);

	/* src_chg bit0 for res change flag, bit1 for realloc mv buf flag,
	 * bit2 for not support flag, other bits are reserved
	 */
	res_chg = ((src_chg & VDEC_RES_CHANGE) != 0U) ? true : false;
	mtk_vcodec_unsupport = ((src_chg & VDEC_HW_NOT_SUPPORT) != 0) ?
						   true : false;
	need_seq_header = ((src_chg & VDEC_NEED_SEQ_HEADER) != 0U) ?
					  true : false;
	if (ret || !res_chg || mtk_vcodec_unsupport
		|| need_seq_header) {
		/*
		 * fb == NULL menas to parse SPS/PPS header or
		 * resolution info in src_mem. Decode can fail
		 * if there is no SPS header or picture info
		 * in bs
		 */
		vb2_v4l2 = to_vb2_v4l2_buffer(vb);
		buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
		last_frame_type = buf->lastframe;

		if (need_seq_header)
			vb2_v4l2->flags |= V4L2_BUF_FLAG_OUTPUT_NOT_GENERATED;

		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
		if (!src_buf) {
			mtk_v4l2_err("[%d]Error!!src_buf is NULL!");
			return;
		}
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf),
						  VB2_BUF_STATE_DONE);
		mtk_v4l2_debug(ret ? 0 : 1,
			"[%d] vdec_if_decode() src_buf=%d, size=%zu, fail=%d, res_chg=%d, mtk_vcodec_unsupport=%d",
			ctx->id, src_buf->index,
			src_mem->size, ret, res_chg,
			mtk_vcodec_unsupport);

		/* If not support the source, eg: w/h,
		 * bitdepth, level, we need to stop to play it
		 */
		if (need_seq_header) {
			vb2_v4l2->flags |= V4L2_BUF_FLAG_OUTPUT_NOT_GENERATED;
			mtk_v4l2_debug(3, "[%d]Error!! Need seq header!",
						 ctx->id);
			mtk_vdec_queue_noseqheader_event(ctx);
		} else if (mtk_vcodec_unsupport || last_frame_type != NON_EOS) {
			mtk_v4l2_err("[%d]Error!! Codec driver not support the file!",
						 ctx->id);
			mtk_vdec_queue_error_event(ctx);
		} else if (ret == -EIO) {
			/* ipi timeout / VPUD crashed ctx abort */
			ctx->state = MTK_STATE_ABORT;
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

	ret = vdec_if_get_param(ctx, GET_PARAM_PIC_INFO,
		&ctx->last_decoded_picinfo);
	if (ret) {
		mtk_v4l2_err("[%d]Error!! Cannot get param : GET_PARAM_PICTURE_INFO ERR",
					 ctx->id);
		return;
	}

	dst_q_data = &ctx->q_data[MTK_Q_DATA_DST];
	fourcc = ctx->last_decoded_picinfo.fourcc;
	dst_q_data->fmt = mtk_find_fmt_by_pixel(fourcc);

	for (i = 0; i < dst_q_data->fmt->num_planes; i++) {
		dst_q_data->sizeimage[i] = ctx->last_decoded_picinfo.fb_sz[i];
		dst_q_data->bytesperline[i] = ctx->last_decoded_picinfo.buf_w;
	}

	bs_fourcc = ctx->q_data[MTK_Q_DATA_SRC].fmt->fourcc;
	fm_fourcc = ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
	mtk_v4l2_debug(0,
				   "[%d] Init Vdec OK wxh=%dx%d pic wxh=%dx%d bitdepth:%d lo:%d sz[0]=0x%x sz[1]=0x%x",
				   ctx->id,
				   ctx->last_decoded_picinfo.buf_w,
				   ctx->last_decoded_picinfo.buf_h,
				   ctx->last_decoded_picinfo.pic_w,
				   ctx->last_decoded_picinfo.pic_h,
				   ctx->last_decoded_picinfo.bitdepth,
				   ctx->last_decoded_picinfo.layout_mode,
				   dst_q_data->sizeimage[0],
				   dst_q_data->sizeimage[1]);

	mtk_v4l2_debug(0, "[%d] bs %c%c%c%c fm %c%c%c%c, num_planes %d, fb_sz[0] %d, fb_sz[1] %d",
				   ctx->id,
				   bs_fourcc & 0xFF, (bs_fourcc >> 8) & 0xFF,
				   (bs_fourcc >> 16) & 0xFF,
				   (bs_fourcc >> 24) & 0xFF,
				   fm_fourcc & 0xFF, (fm_fourcc >> 8) & 0xFF,
				   (fm_fourcc >> 16) & 0xFF,
				   (fm_fourcc >> 24) & 0xFF,
				   dst_q_data->fmt->num_planes,
				   ctx->last_decoded_picinfo.fb_sz[0],
				   ctx->last_decoded_picinfo.fb_sz[1]);

	ret = vdec_if_get_param(ctx, GET_PARAM_DPB_SIZE, &dpbsize);
	if (dpbsize == 0)
		mtk_v4l2_err("[%d] GET_PARAM_DPB_SIZE fail=%d", ctx->id, ret);

	ctx->last_dpb_size = dpbsize;

	ret = vdec_if_get_param(ctx, GET_PARAM_COLOR_DESC, &color_desc);
	if (ret == 0) {
		ctx->last_is_hdr = color_desc.is_hdr;
	} else {
		mtk_v4l2_err("[%d] GET_PARAM_COLOR_DESC fail=%d",
		ctx->id, ret);
	}

	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

	if (!vb2_is_streaming(dst_vq)) {
		ctx->picinfo = ctx->last_decoded_picinfo;
		ctx->dpb_size = dpbsize;
		ctx->is_hdr = color_desc.is_hdr;
	}

	ctx->state = MTK_STATE_HEADER;
	mtk_v4l2_debug(1, "[%d] dpbsize=%d", ctx->id, ctx->last_dpb_size);

}

static void vb2ops_vdec_buf_finish(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2;
	struct mtk_video_dec_buf *buf;
	unsigned int plane = 0;
	struct mtk_video_dec_buf *mtkbuf;

	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return;

	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	buf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);
	mutex_lock(&ctx->buf_lock);
	buf->queued_in_v4l2 = false;
	buf->queued_in_vb2 = false;
	mutex_unlock(&ctx->buf_lock);

	// Check if need to proceed cache operations for Capture Queue
	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	mtkbuf = container_of(vb2_v4l2, struct mtk_video_dec_buf, vb);

	if (mtkbuf->frame_buffer.dma_general_buf != 0) {
		dma_buf_put(mtkbuf->frame_buffer.dma_general_buf);
		mtkbuf->frame_buffer.dma_general_buf = 0;
		mtk_v4l2_debug(4,
			"dma_buf_put general_buf fd=%d, dma_buf=%p, DMA=%lx",
			mtkbuf->general_user_fd,
			mtkbuf->frame_buffer.dma_general_buf,
			(unsigned long)mtkbuf->frame_buffer.dma_general_addr);
	}

	if (vb->vb2_queue->memory == VB2_MEMORY_DMABUF &&
		!(mtkbuf->flags & NO_CAHCE_INVALIDATE) &&
		!(ctx->dec_params.svp_mode)) {
		for (plane = 0; plane < buf->frame_buffer.num_planes; plane++) {
			struct vdec_fb dst_mem;
			struct dma_buf_attachment *buf_att;
			struct sg_table *sgt;

			mtk_v4l2_debug(4, "[%d] Cache sync+", ctx->id);

			buf_att = dma_buf_attach(vb->planes[plane].dbuf,
				&ctx->dev->plat_dev->dev);
			sgt = dma_buf_map_attachment(buf_att, DMA_FROM_DEVICE);
			if (IS_ERR_OR_NULL(sgt)) {
				mtk_v4l2_err("dma_buf_map_attachment fail %d.\n", sgt);
				dma_buf_detach(vb->planes[plane].dbuf, buf_att);
				return;
			}
			dma_sync_sg_for_cpu(&ctx->dev->plat_dev->dev, sgt->sgl,
				sgt->orig_nents, DMA_FROM_DEVICE);
			dma_buf_unmap_attachment(buf_att, sgt, DMA_FROM_DEVICE);

			dst_mem.fb_base[plane].dma_addr =
				vb2_dma_contig_plane_dma_addr(vb, plane);
			dst_mem.fb_base[plane].size = ctx->picinfo.fb_sz[plane];
			dma_buf_detach(vb->planes[plane].dbuf, buf_att);

			mtk_v4l2_debug(4,
				"[%d] Cache sync- FD for %lx sz=%d dev %p pfb %p",
				ctx->id,
				(unsigned long)dst_mem.fb_base[plane].dma_addr,
				(unsigned int)dst_mem.fb_base[plane].size,
				&ctx->dev->plat_dev->dev,
				&buf->frame_buffer);
		}
	}
}

static void vb2ops_vdec_buf_cleanup(struct vb2_buffer *vb)
{
	int i;
	struct mtk_vcodec_ctx *ctx;
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
		struct vb2_v4l2_buffer, vb2_buf);
	struct mtk_video_dec_buf *buf = container_of(vb2_v4l2,
		struct mtk_video_dec_buf, vb);

	ctx = vb2_get_drv_priv(vb->vb2_queue);
	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
		!vb2_is_streaming(vb->vb2_queue)) {
		mutex_lock(&ctx->buf_lock);
		if (buf->used == true) {
			for (i = 0; i < buf->frame_buffer.num_planes; i++) {
				fput(buf->frame_buffer.fb_base[i].dmabuf->file);
				mtk_v4l2_debug(4, "[Ref cnt] id=%d Ref put dma %p",
					buf->frame_buffer.index,
					buf->frame_buffer.fb_base[i].dmabuf);
			}
		}
		mutex_unlock(&ctx->buf_lock);
	}
}

static int vb2ops_vdec_buf_init(struct vb2_buffer *vb)
{
	struct vb2_v4l2_buffer *vb2_v4l2 = container_of(vb,
		struct vb2_v4l2_buffer, vb2_buf);
	struct mtk_video_dec_buf *buf = container_of(vb2_v4l2,
		struct mtk_video_dec_buf, vb);
	int i;

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		mtk_v4l2_debug(4, "[%d] pfb=%p used %d ready_to_display %d queued_in_v4l2 %d",
			vb->vb2_queue->type, &buf->frame_buffer,
			buf->used, buf->ready_to_display, buf->queued_in_v4l2);
		/* User could use different struct dma_buf*
		 * with the same index & enter this buf_init.
		 * once this buffer buf->used == true will reset in mistake
		 * VB2 use kzalloc for struct mtk_video_dec_buf,
		 * so init could be no need
		 */
		if (buf->used == false) {
			buf->ready_to_display = false;
			buf->queued_in_v4l2 = false;
		}
		for (i = 0; i < VIDEO_MAX_PLANES; i++)
			buf->frame_buffer.fb_base[i].buf_fd = -1;
	} else if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/* Do not reset EOS for 1st buffer with Early EOS*/
		/* buf->lastframe = NON_EOS; */
	} else {
		mtk_v4l2_err("%s: unknown queue type", __func__);
		return -EINVAL;
	}

	return 0;
}

static int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	unsigned long total_frame_bufq_count;

	mtk_v4l2_debug(4, "[%d] (%d) state=(%x)", ctx->id, q->type, ctx->state);

	if (ctx->state == MTK_STATE_FLUSH)
		ctx->state = MTK_STATE_HEADER;

	//SET_PARAM_TOTAL_FRAME_BUFQ_COUNT for SW DEC
	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		total_frame_bufq_count = q->num_buffers;
		if (vdec_if_set_param(ctx,
			SET_PARAM_TOTAL_FRAME_BUFQ_COUNT,
			&total_frame_bufq_count)) {
			mtk_v4l2_err("[%d] Error!! Cannot set param",
				ctx->id);
		}
	}

	mtk_vdec_set_param(ctx);
	return 0;
}

static void vb2ops_vdec_stop_streaming(struct vb2_queue *q)
{
	struct vb2_buffer *src_buf = NULL, *dst_buf = NULL;
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb2_v4l2 = NULL;
	struct mtk_video_dec_buf *src_buf_info = NULL;
	unsigned int i = 0;

	mtk_v4l2_debug(4, "[%d] (%d) state=(%x) ctx->decoded_frame_cnt=%d",
		ctx->id, q->type, ctx->state, ctx->decoded_frame_cnt);

	ctx->input_max_ts = 0;

	if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (ctx->state >= MTK_STATE_HEADER) {
			src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
			if (src_buf != NULL) {
				vb2_v4l2 = to_vb2_v4l2_buffer(src_buf);
				src_buf_info = container_of(vb2_v4l2,
					struct mtk_video_dec_buf, vb);
				/* for bs buffer reuse case & avoid put to done twice*/
				mtk_vdec_flush_decoder(ctx, &src_buf_info->bs_buffer);
			} else {
				mtk_vdec_flush_decoder(ctx, NULL);
			}
		}
		while ((src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
			if (to_vb2_v4l2_buffer(src_buf)
				!= &ctx->dec_flush_buf->vb) {

				vb2_v4l2 = to_vb2_v4l2_buffer(src_buf);

				mtk_v4l2_debug(2,
					"[%d] src_buf_remove queue id=%d",
					ctx->id, vb2_v4l2->vb2_buf.index);

				v4l2_m2m_buf_done(vb2_v4l2,
					VB2_BUF_STATE_ERROR);
			}
		ctx->dec_flush_buf->lastframe = NON_EOS;
		return;
	}

	if (ctx->state >= MTK_STATE_HEADER) {

		/* Until STREAMOFF is called on the CAPTURE queue
		 * (acknowledging the event), the driver operates
		 * as if the resolution hasn't changed yet, i.e.
		 * VIDIOC_G_FMT< etc. return previous resolution.
		 * So we update picinfo here
		 */
		ctx->picinfo = ctx->last_decoded_picinfo;
		ctx->dpb_size = ctx->last_dpb_size;
		ctx->is_hdr = ctx->last_is_hdr;

		mtk_v4l2_debug(2,
			"[%d]-> new(%d,%d), old(%d,%d), real(%d,%d) bit:%d\n",
			ctx->id, ctx->last_decoded_picinfo.pic_w,
			ctx->last_decoded_picinfo.pic_h,
			ctx->picinfo.pic_w, ctx->picinfo.pic_h,
			ctx->last_decoded_picinfo.buf_w,
			ctx->last_decoded_picinfo.buf_h,
			ctx->picinfo.bitdepth);

		mtk_vdec_flush_decoder(ctx, NULL);
	}

	while ((dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx))) {
		for (i = 0; i < dst_buf->num_planes; i++)
			vb2_set_plane_payload(dst_buf, i, 0);

		vb2_v4l2 = to_vb2_v4l2_buffer(dst_buf);

		mtk_v4l2_debug(2,
			"[%d] dst_buf_remove queue id=%d",
			ctx->id, vb2_v4l2->vb2_buf.index);

		v4l2_m2m_buf_done(vb2_v4l2,
						  VB2_BUF_STATE_ERROR);
	}

}

static void m2mops_vdec_device_run(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;
	struct mtk_vcodec_dev *dev = ctx->dev;

	queue_work(dev->decode_workqueue, &ctx->decode_work);
}

static int m2mops_vdec_job_ready(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mtk_v4l2_debug(4, "[%d]", ctx->id);

	if (ctx->state == MTK_STATE_ABORT)
		return 0;

	if ((ctx->last_decoded_picinfo.pic_w != ctx->picinfo.pic_w) ||
		(ctx->last_decoded_picinfo.pic_h != ctx->picinfo.pic_h) ||
		(ctx->last_dpb_size != ctx->dpb_size) ||
		(ctx->last_is_hdr != ctx->is_hdr))
		return 0;

	if (ctx->state != MTK_STATE_HEADER)
		return 0;

	return 1;
}

static void m2mops_vdec_job_abort(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;

	mtk_v4l2_debug(4, "[%d]", ctx->id);
	ctx->state = MTK_STATE_ABORT;
}

static int mtk_vdec_g_v_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;
	static unsigned int value;
	struct mtk_color_desc *color_desc;

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= MTK_STATE_HEADER)
			ctrl->val = ctx->dpb_size;
		else {
			mtk_v4l2_debug(0, "Seqinfo not ready");
			ctrl->val = 0;
		}
		break;
	case V4L2_CID_MPEG_MTK_FRAME_INTERVAL:
		if (vdec_if_get_param(ctx,
			GET_PARAM_FRAME_INTERVAL, &value) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		ctrl->p_new.p_u32 = &value;
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_MTK_FRAME_INTERVAL val = %u",
					   *(ctrl->p_new.p_u32));
		break;
	case V4L2_CID_MPEG_MTK_COLOR_DESC:
		color_desc = (struct mtk_color_desc *)ctrl->p_new.p_u32;
		if (vdec_if_get_param(ctx, GET_PARAM_COLOR_DESC, color_desc)
		    != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_ASPECT_RATIO:
		if (vdec_if_get_param(ctx, GET_PARAM_ASPECT_RATIO, &ctrl->val)
		    != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_FIX_BUFFERS:
		if (vdec_if_get_param(ctx,
		    GET_PARAM_PLATFORM_SUPPORTED_FIX_BUFFERS, &ctrl->val)
		    != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_FIX_BUFFERS_SVP:
		if (vdec_if_get_param(ctx,
		    GET_PARAM_PLATFORM_SUPPORTED_FIX_BUFFERS_SVP, &ctrl->val)
		    != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_INTERLACING:
		if (vdec_if_get_param(ctx, GET_PARAM_INTERLACING, &ctrl->val)
		    != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MTK_CODEC_TYPE:
		if (vdec_if_get_param(ctx, GET_PARAM_CODEC_TYPE, &ctrl->val)
			!= 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get param", ctx->id);
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
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
#ifdef CONFIG_VB2_MEDIATEK_DMA_CONTIG
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
#ifdef CONFIG_VB2_MEDIATEK_DMA_CONTIG
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
	case V4L2_CID_MPEG_MTK_QUEUED_FRAMEBUF_COUNT:
		ctx->dec_params.queued_frame_buf_count = ctrl->val;
		break;
	case V4L2_CID_MPEG_MTK_LOG:
		mtk_vcodec_set_log(ctx, ctrl->p_new.p_char);
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

int mtk_vcodec_dec_ctrls_setup(struct mtk_vcodec_ctx *ctx)
{
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, MTK_MAX_CTRLS_HINT);

	/* g_volatile_ctrl */
	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		0, 32, 1, 1);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
		&mtk_vcodec_dec_ctrl_ops,
		V4L2_CID_MPEG_MTK_FRAME_INTERVAL,
		16666, 41719, 1, 33333);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_ASPECT_RATIO,
				0, 0xF000F, 1, 0x10001);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_FIX_BUFFERS,
				0, 0xF, 1, 0);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_FIX_BUFFERS_SVP,
				0, 0xF, 1, 0);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl,
				&mtk_interlacing_ctrl, NULL);
	if (ctrl)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE;

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl,
				&mtk_codec_type_ctrl, NULL);

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &mtk_color_desc_ctrl, NULL);
	if (ctrl)
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
				0, 4096, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_QUEUED_FRAMEBUF_COUNT,
				0, 64, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctx->ctrl_hdl,
				&mtk_vcodec_dec_ctrl_ops,
				V4L2_CID_MPEG_MTK_LOG,
				0, 255, 1, 0);

	if (ctx->ctrl_hdl.error) {
		mtk_v4l2_err("Adding control failed %d",
					 ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);
	return 0;
}

const struct v4l2_m2m_ops mtk_vdec_m2m_ops = {
	.device_run     = m2mops_vdec_device_run,
	.job_ready      = m2mops_vdec_job_ready,
	.job_abort      = m2mops_vdec_job_abort,
};

static const struct vb2_ops mtk_vdec_vb2_ops = {
	.queue_setup    = vb2ops_vdec_queue_setup,
	.buf_prepare    = vb2ops_vdec_buf_prepare,
	.buf_queue      = vb2ops_vdec_buf_queue,
	.wait_prepare   = vb2_ops_wait_prepare,
	.wait_finish    = vb2_ops_wait_finish,
	.buf_init       = vb2ops_vdec_buf_init,
	.buf_finish     = vb2ops_vdec_buf_finish,
	.buf_cleanup = vb2ops_vdec_buf_cleanup,
	.start_streaming        = vb2ops_vdec_start_streaming,
	.stop_streaming = vb2ops_vdec_stop_streaming,
};

const struct v4l2_ioctl_ops mtk_vdec_ioctl_ops = {
	.vidioc_streamon        = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff       = v4l2_m2m_ioctl_streamoff,
	.vidioc_reqbufs         = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf        = v4l2_m2m_ioctl_querybuf,
	.vidioc_expbuf          = v4l2_m2m_ioctl_expbuf,

	.vidioc_qbuf            = vidioc_vdec_qbuf,
	.vidioc_dqbuf           = vidioc_vdec_dqbuf,

	.vidioc_try_fmt_vid_cap_mplane  = vidioc_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane  = vidioc_try_fmt_vid_out_mplane,

	.vidioc_s_fmt_vid_cap_mplane    = vidioc_vdec_s_fmt,
	.vidioc_s_fmt_vid_out_mplane    = vidioc_vdec_s_fmt,
	.vidioc_g_fmt_vid_cap_mplane    = vidioc_vdec_g_fmt,
	.vidioc_g_fmt_vid_out_mplane    = vidioc_vdec_g_fmt,

	.vidioc_create_bufs             = v4l2_m2m_ioctl_create_bufs,

	.vidioc_enum_fmt_vid_cap_mplane = vidioc_vdec_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_vdec_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,

	.vidioc_querycap                = vidioc_vdec_querycap,
	.vidioc_subscribe_event         = vidioc_vdec_subscribe_evt,
	.vidioc_unsubscribe_event       = v4l2_event_unsubscribe,
	.vidioc_g_selection             = vidioc_vdec_g_selection,
	.vidioc_s_selection             = vidioc_vdec_s_selection,

	.vidioc_decoder_cmd     = vidioc_decoder_cmd,
	.vidioc_try_decoder_cmd = vidioc_try_decoder_cmd,

	.vidioc_g_crop                  = vidioc_vdec_g_crop,
};

#ifdef CONFIG_VB2_MEDIATEK_DMA_CONTIG
static int vdec_dc_ion_map_dmabuf(void *mem_priv)
{
	return mtk_dma_contig_memops.map_dmabuf(mem_priv);
}
#endif

int mtk_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
	struct vb2_queue *dst_vq)
{
	struct mtk_vcodec_ctx *ctx = priv;
	int ret = 0;

	mtk_v4l2_debug(4, "[%d]", ctx->id);

	src_vq->type            = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes        = VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv        = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_video_dec_buf);
	src_vq->ops             = &mtk_vdec_vb2_ops;
#ifdef CONFIG_VB2_MEDIATEK_DMA_CONTIG
	vdec_ion_dma_contig_memops = mtk_dma_contig_memops;
	vdec_ion_dma_contig_memops.map_dmabuf = vdec_dc_ion_map_dmabuf;

	src_vq->mem_ops         = &vdec_ion_dma_contig_memops;
	mtk_v4l2_debug(4, "src_vq use mtk_dma_contig_memops");
#else
	src_vq->mem_ops         = &vb2_dma_contig_memops;
	mtk_v4l2_debug(4, "src_vq use vb2_dma_contig_memops");
#endif
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock            = &ctx->dev->dev_mutex;
	src_vq->dev             = &ctx->dev->plat_dev->dev;
	src_vq->allow_zero_bytesused = 1;

	ret = vb2_queue_init(src_vq);
	if (ret) {
		mtk_v4l2_err("Failed to initialize videobuf2 queue(output)");
		return ret;
	}
	dst_vq->type            = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes        = VB2_DMABUF | VB2_MMAP;
	dst_vq->drv_priv        = ctx;
	dst_vq->buf_struct_size = sizeof(struct mtk_video_dec_buf);
	dst_vq->ops             = &mtk_vdec_vb2_ops;
#ifdef CONFIG_VB2_MEDIATEK_DMA_SG
	dst_vq->mem_ops         = &mtk_dma_sg_memops;
#elif defined(CONFIG_VB2_MEDIATEK_DMA_CONTIG)
	dst_vq->mem_ops         = &vdec_ion_dma_contig_memops;
	mtk_v4l2_debug(4, "dst_vq use mtk_dma_contig_memops");
#else
	dst_vq->mem_ops         = &vb2_dma_contig_memops;
	mtk_v4l2_debug(4, "dst_vq use vb2_dma_contig_memops");
#endif
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock            = &ctx->dev->dev_mutex;
	dst_vq->dev             = &ctx->dev->plat_dev->dev;
	dst_vq->allow_zero_bytesused = 1;

	ret = vb2_queue_init(dst_vq);
	if (ret) {
		vb2_queue_release(src_vq);
		mtk_v4l2_err("Failed to initialize videobuf2 queue(capture)");
	}

	return ret;
}
