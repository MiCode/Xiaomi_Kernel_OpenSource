/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Huiguo.Zhu <huiguo.zhu@mediatek.com>
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

#include <linux/bug.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <media/v4l2-ioctl.h>
#include <linux/pm_runtime.h>
#include <media/videobuf2-dma-contig.h>

#include "mtk_nr_def.h"
#include "mtk_nr_ctrl.h"

static const struct mtk_nr_fmt mtk_nr_formats[] = {
	{
	 .pixelformat = V4L2_PIX_FMT_MT21C,
	 .depth = {8, 4},
	 .num_planes = 2,
	 .num_comp = 2,
	}
};

static inline struct mtk_nr_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_nr_ctx, fh);
}

static void mtk_nr_ctx_state_lock_set(u32 state, struct mtk_nr_ctx *ctx)
{
	mutex_lock(&ctx->slock);
	ctx->state |= state;
	mutex_unlock(&ctx->slock);
}

static void mtk_nr_ctx_state_lock_clear(u32 state, struct mtk_nr_ctx *ctx)
{
	mutex_lock(&ctx->slock);
	ctx->state &= ~state;
	mutex_unlock(&ctx->slock);
}

static void mtk_nr_ctx_lock(struct vb2_queue *vq)
{
	struct mtk_nr_ctx *ctx = vb2_get_drv_priv(vq);

	mutex_lock(&ctx->qlock);
}

static void mtk_nr_ctx_unlock(struct vb2_queue *vq)
{
	struct mtk_nr_ctx *ctx = vb2_get_drv_priv(vq);

	mutex_unlock(&ctx->qlock);
}

static bool mtk_nr_ctx_state_is_set(u32 mask, struct mtk_nr_ctx *ctx)
{
	bool ret;

	mutex_lock(&ctx->slock);
	ret = (ctx->state & mask) == mask;
	mutex_unlock(&ctx->slock);

	return ret;
}

static int mtk_nr_ctx_stop_req(struct mtk_nr_ctx *ctx)
{
	struct mtk_nr_ctx *curr_ctx;
	struct mtk_nr_dev *nr = ctx->nr_dev;
	int ret;

	curr_ctx = v4l2_m2m_get_curr_priv(nr->m2m.m2m_dev);
	if (!mtk_nr_m2m_pending(nr) || (curr_ctx != ctx))
		return 0;

	mtk_nr_ctx_state_lock_set(MTK_NR_CTX_STOP_REQ, ctx);
	ret = wait_event_timeout(nr->irq_queue,
				 !mtk_nr_ctx_state_is_set(MTK_NR_CTX_STOP_REQ, ctx),
				 MTK_NR_SHUTDOWN_TIMEOUT);

	return ret == 0 ? -ETIMEDOUT : ret;
}

static struct mtk_nr_frame *mtk_nr_ctx_get_frame(struct mtk_nr_ctx *ctx, enum v4l2_buf_type type)
{
	struct mtk_nr_frame *frame;

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		frame = &ctx->s_frame;
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		frame = &ctx->d_frame;
	} else {
		pr_err("[NR][M2M] Wrong buffer/video queue type %d\n", type);
		return ERR_PTR(-EINVAL);
	}

	return frame;
}

static void *mtk_nr_m2m_buf_remove(struct mtk_nr_ctx *ctx, enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
}

static void mtk_nr_m2m_job_finish(struct mtk_nr_ctx *ctx, int vb_state)
{
	struct vb2_buffer *src_vb, *dst_vb;
	struct vb2_v4l2_buffer *src_vbuf = NULL, *dst_vbuf = NULL;

	src_vb = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	src_vbuf = to_vb2_v4l2_buffer(src_vb);
	dst_vb = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	dst_vbuf = to_vb2_v4l2_buffer(dst_vb);

	dst_vbuf->vb2_buf.timestamp = src_vbuf->vb2_buf.timestamp;
	dst_vbuf->timecode = src_vbuf->timecode;
	dst_vbuf->flags &= ~V4L2_BUF_FLAG_TSTAMP_SRC_MASK;
	dst_vbuf->flags |= src_vbuf->flags & V4L2_BUF_FLAG_TSTAMP_SRC_MASK;

	v4l2_m2m_buf_done(src_vbuf, vb_state);
	v4l2_m2m_buf_done(dst_vbuf, vb_state);
	v4l2_m2m_job_finish(ctx->nr_dev->m2m.m2m_dev, ctx->m2m_ctx);
}

static void mtk_nr_m2m_job_cancel(struct mtk_nr_ctx *ctx)
{
	int ret;

	ret = mtk_nr_ctx_stop_req(ctx);
	if (ret == -ETIMEDOUT || ctx->state & MTK_NR_CTX_ABORT) {
		mtk_nr_ctx_state_lock_clear(MTK_NR_CTX_STOP_REQ | MTK_NR_CTX_ABORT, ctx);
		mtk_nr_m2m_job_finish(ctx, VB2_BUF_STATE_ERROR);
	}
}

static int mtk_nr_m2m_queue_setup(struct vb2_queue *vq,
				  unsigned int *num_buffers, unsigned int *num_planes,
				  unsigned int sizes[], struct device *alloc_devs[])
{
	struct mtk_nr_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_nr_frame *frame;
	int i;

	frame = mtk_nr_ctx_get_frame(ctx, vq->type);
	if (IS_ERR(frame))
		return -EINVAL;

	if (!frame->fmt)
		return -EINVAL;

	*num_planes = frame->fmt->num_planes;
	for (i = 0; i < frame->fmt->num_planes; i++) {
		sizes[i] = frame->payload[i];
		alloc_devs[i] = ctx->nr_dev->larb[i];
	}

	return 0;
}

static int mtk_nr_m2m_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_nr_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_nr_frame *frame;
	int i;

	frame = mtk_nr_ctx_get_frame(ctx, vb->vb2_queue->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	if (!V4L2_TYPE_IS_OUTPUT(vb->vb2_queue->type)) {
		for (i = 0; i < frame->fmt->num_planes; i++)
			vb2_set_plane_payload(vb, i, frame->payload[i]);
	}

	return 0;
}

static void mtk_nr_m2m_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_nr_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);

	if (ctx->m2m_ctx)
		v4l2_m2m_buf_queue(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static int mtk_nr_m2m_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_nr_ctx *ctx = q->drv_priv;
	int ret;

	ret = pm_runtime_get_sync(&ctx->nr_dev->pdev->dev);

	return ret > 0 ? 0 : ret;
}

static void mtk_nr_m2m_stop_streaming(struct vb2_queue *q)
{
	struct mtk_nr_ctx *ctx = q->drv_priv;
	struct vb2_buffer *vb;

	mtk_nr_m2m_job_cancel(ctx);
	vb = mtk_nr_m2m_buf_remove(ctx, q->type);
	while (vb != NULL) {
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_ERROR);
		vb = mtk_nr_m2m_buf_remove(ctx, q->type);
	}

	pm_runtime_put_sync(&ctx->nr_dev->pdev->dev);
}

static struct vb2_ops mtk_nr_m2m_qops = {
	.queue_setup = mtk_nr_m2m_queue_setup,
	.buf_prepare = mtk_nr_m2m_buf_prepare,
	.buf_queue = mtk_nr_m2m_buf_queue,
	.wait_prepare = mtk_nr_ctx_unlock,
	.wait_finish = mtk_nr_ctx_lock,
	.stop_streaming = mtk_nr_m2m_stop_streaming,
	.start_streaming = mtk_nr_m2m_start_streaming,
};

static const struct mtk_nr_fmt *mtk_nr_get_format(int index)
{
	if (index >= ARRAY_SIZE(mtk_nr_formats))
		return NULL;
	return &mtk_nr_formats[index];
}

static void mtk_nr_set_frame_size(struct mtk_nr_frame *frame, int width, int height)
{
	frame->f_width = width;
	frame->f_height = height;
	frame->crop.width = width;
	frame->crop.height = height;
	frame->crop.left = 0;
	frame->crop.top = 0;
}

static void mtk_nr_prepare_addr(struct mtk_nr_ctx *ctx, struct vb2_buffer *vb,
			struct mtk_nr_frame *frame, struct mtk_nr_addr *addr)
{
	addr->y = vb2_dma_contig_plane_dma_addr(vb, 0);
	addr->c = vb2_dma_contig_plane_dma_addr(vb, 1);
}

static void mtk_nr_m2m_get_bufs(struct mtk_nr_ctx *ctx)
{
	struct mtk_nr_frame *s_frame, *d_frame;
	struct vb2_buffer *src_vb, *dst_vb;
	struct vb2_v4l2_buffer *src_vbuf, *dst_vbuf;

	s_frame = &ctx->s_frame;
	d_frame = &ctx->d_frame;

	src_vb = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	mtk_nr_prepare_addr(ctx, src_vb, s_frame, &(ctx->s_buf.addr));

	dst_vb = v4l2_m2m_next_dst_buf(ctx->m2m_ctx);
	mtk_nr_prepare_addr(ctx, dst_vb, d_frame, &(ctx->d_addr));

	src_vbuf = to_vb2_v4l2_buffer(src_vb);
	dst_vbuf = to_vb2_v4l2_buffer(dst_vb);
	dst_vbuf->vb2_buf.timestamp = src_vbuf->vb2_buf.timestamp;
}

static int mtk_nr_process_done(void *priv)
{
	struct mtk_nr_dev *nr = priv;
	struct mtk_nr_ctx *ctx;

	ctx = v4l2_m2m_get_curr_priv(nr->m2m.m2m_dev);
	mutex_lock(&ctx->slock);

	if (test_and_clear_bit(ST_M2M_PEND, &nr->state)) {
		if (test_and_clear_bit(ST_M2M_SUSPENDING, &nr->state)) {
			set_bit(ST_M2M_SUSPENDED, &nr->state);
			wake_up(&nr->irq_queue);
			goto done_unlock;
		}

		mutex_unlock(&ctx->slock);
		mtk_nr_m2m_job_finish(ctx, VB2_BUF_STATE_DONE);

		if (ctx->state & MTK_NR_CTX_STOP_REQ) {
			ctx->state &= ~MTK_NR_CTX_STOP_REQ;
			wake_up(&nr->irq_queue);
		}

		return 0;
	}

done_unlock:
	mutex_unlock(&ctx->slock);

	return 0;
}

static void mtk_nr_wait_frame_done(struct mtk_nr_dev *nr_dev)
{
	if (wait_event_interruptible_timeout(nr_dev->wait_nr_irq_handle,
			atomic_read(&nr_dev->wait_nr_irq_flag), HZ / 10) == 0) {

		pr_err("[NR][IRQ] Irq timeout\n");
	}
	atomic_set(&nr_dev->wait_nr_irq_flag, 0);
}

static void mtk_nr_m2m_worker(struct work_struct *work)
{
	struct NR_PROCESS_PARAM_T process_param;
	struct mtk_nr_ctx *ctx = container_of(work, struct mtk_nr_ctx, work);
	struct mtk_nr_dev *nr;

	nr = ctx->nr_dev;
	set_bit(ST_M2M_PEND, &nr->state);

	if (ctx->state & MTK_NR_CTX_STOP_REQ) {
		ctx->state &= ~MTK_NR_CTX_STOP_REQ;
		ctx->state |= MTK_NR_CTX_ABORT;
		wake_up(&nr->irq_queue);
		goto put_device;
	}

	mtk_nr_m2m_get_bufs(ctx);

	process_param.u2SrcFrmWidth = ctx->s_frame.pitch[0];
	process_param.u2SrcFrmHeight = ctx->s_frame.payload[0] / ctx->s_frame.pitch[0];
	process_param.u2SrcPicWidth = ctx->s_frame.f_width;
	process_param.u2SrcPicHeight = ctx->s_frame.f_height;

	process_param.u4InputAddrMvaYCurr = ctx->s_buf.addr.y;
	process_param.u4InputAddrMvaCbcrCurr = ctx->s_buf.addr.c;

	if (ctx->nr_level.u4FnrLevel == 0) {
		process_param.u4InputAddrMvaYLast = process_param.u4InputAddrMvaYCurr;
		process_param.u4InputAddrMvaCbcrLast = process_param.u4InputAddrMvaCbcrCurr;
	}

	process_param.u4OutputAddrMvaY = ctx->d_addr.y;
	process_param.u4OutputAddrMvaCbcr = ctx->d_addr.c;

	process_param.u4TotalLevel = ctx->nr_level.u4TotalLevel;
	process_param.u4BnrLevel = ctx->nr_level.u4BnrLevel;
	process_param.u4MnrLevel = ctx->nr_level.u4MnrLevel;
	process_param.u4FnrLevel = ctx->nr_level.u4FnrLevel;

	MTK_NR_Config_DispSysCfg(ctx->nr_dev->bdpsys_reg_base);
	MTK_NR_Process(&process_param, (unsigned long)ctx->nr_dev->nr_reg_base);
	mtk_nr_wait_frame_done(ctx->nr_dev);

	ctx->state &= ~MTK_NR_PARAMS;

	mtk_nr_process_done(nr);

	return;

put_device:
	ctx->state &= ~MTK_NR_PARAMS;
}

static const struct mtk_nr_fmt *mtk_nr_find_fmt(u32 *pixelformat, u32 *mbus_code, u32 index)
{
	const struct mtk_nr_fmt *fmt, *def_fmt = NULL;
	unsigned int i;

	if (index >= ARRAY_SIZE(mtk_nr_formats))
		return NULL;

	for (i = 0; i < ARRAY_SIZE(mtk_nr_formats); ++i) {
		fmt = mtk_nr_get_format(i);
		if ((pixelformat != NULL) && (fmt->pixelformat == *pixelformat))
			return fmt;
		if (index == i)
			def_fmt = fmt;
	}
	return def_fmt;

}

static int mtk_nr_m2m_querycap(struct file *file, void *fh, struct v4l2_capability *cap)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);
	struct mtk_nr_dev *nr = ctx->nr_dev;

	strlcpy(cap->driver, MTK_NR_MODULE_NAME, sizeof(cap->driver));
	strlcpy(cap->card, nr->pdev->name, sizeof(cap->card));
	strlcpy(cap->bus_info, "platform", sizeof(cap->bus_info));
	cap->device_caps = V4L2_CAP_STREAMING |
	    V4L2_CAP_VIDEO_M2M_MPLANE |
	    V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE;

	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int mtk_nr_m2m_enum_fmt_mplane(struct file *file, void *priv, struct v4l2_fmtdesc *f)
{
	const struct mtk_nr_fmt *fmt;

	fmt = mtk_nr_find_fmt(NULL, NULL, f->index);
	if (!fmt)
		return -EINVAL;

	f->pixelformat = fmt->pixelformat;

	return 0;
}

static int mtk_nr_m2m_g_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);
	struct mtk_nr_frame *frame;
	struct v4l2_pix_format_mplane *pix_mp;
	int i;

	frame = mtk_nr_ctx_get_frame(ctx, f->type);
	if (IS_ERR(frame))
		return PTR_ERR(frame);

	pix_mp = &f->fmt.pix_mp;

	pix_mp->width = frame->f_width;
	pix_mp->height = frame->f_height;
	pix_mp->field = ctx->field_mode;
	pix_mp->pixelformat = frame->fmt->pixelformat;
	pix_mp->colorspace = V4L2_COLORSPACE_REC709;
	pix_mp->num_planes = frame->fmt->num_planes;

	for (i = 0; i < pix_mp->num_planes; ++i) {
		pix_mp->plane_fmt[i].bytesperline = (frame->f_width * frame->fmt->depth[i]) / 8;
		pix_mp->plane_fmt[i].sizeimage =
		    pix_mp->plane_fmt[i].bytesperline * frame->f_height;
	}

	return 0;
}

static void mtk_nr_bound_align_image(u32 *w, unsigned int wmin, unsigned int wmax,
				     unsigned int walign, u32 *h, unsigned int hmin,
				     unsigned int hmax, unsigned int halign)
{
	int width, height, w_step, h_step;

	width = *w;
	height = *h;
	w_step = 1 << walign;
	h_step = 1 << halign;

	v4l_bound_align_image(w, wmin, wmax, walign, h, hmin, hmax, halign, 0);
	if (*w < width && (*w + w_step) <= wmax)
		*w += w_step;
	if (*h < height && (*h + h_step) <= hmax)
		*h += h_step;
}

static int mtk_nr_m2m_try_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);
	struct mtk_nr_dev *nr = ctx->nr_dev;
	const struct mtk_nr_variant *variant = nr->variant;
	struct v4l2_pix_format_mplane *pix_mp;
	const struct mtk_nr_fmt *fmt;
	u32 max_w, max_h, mod_x, mod_y;
	u32 min_w, min_h, tmp_w, tmp_h;
	int i;

	pix_mp = &f->fmt.pix_mp;

	fmt = mtk_nr_find_fmt(&pix_mp->pixelformat, NULL, 0);
	if (!fmt) {
		pr_err("[NR][E] %s, pixelformat format 0x%x invalid\n", __func__,
			  pix_mp->pixelformat);
		return -EINVAL;
	}

	max_w = variant->pix_max->target_w;
	max_h = variant->pix_max->target_h;

	mod_x = ffs(variant->pix_align->org_w) - 1;
	mod_y = ffs(variant->pix_align->org_h) - 1;


	if (V4L2_TYPE_IS_OUTPUT(f->type)) {
		min_w = variant->pix_min->org_w;
		min_h = variant->pix_min->org_h;
	} else {
		min_w = variant->pix_min->target_w;
		min_h = variant->pix_min->target_h;
	}

	tmp_w = pix_mp->width;
	tmp_h = pix_mp->height;

	mtk_nr_bound_align_image(&pix_mp->width, min_w, max_w, mod_x,
				 &pix_mp->height, min_h, max_h, mod_y);

	pix_mp->num_planes = fmt->num_planes;

	if (pix_mp->width >= 1280)	/* HD */
		pix_mp->colorspace = V4L2_COLORSPACE_REC709;
	else			/* SD */
		pix_mp->colorspace = V4L2_COLORSPACE_SMPTE170M;

	for (i = 0; i < pix_mp->num_planes; ++i) {
		int bpl = (pix_mp->width * fmt->depth[i]) >> 3;
		int sizeimage = bpl * pix_mp->height;

		pix_mp->plane_fmt[i].bytesperline = bpl;

		if (pix_mp->plane_fmt[i].sizeimage < sizeimage)
			pix_mp->plane_fmt[i].sizeimage = sizeimage;
	}

	return 0;
}

static int mtk_nr_m2m_s_fmt_mplane(struct file *file, void *fh, struct v4l2_format *f)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);
	struct vb2_queue *vq;
	struct mtk_nr_frame *frame;
	struct v4l2_pix_format_mplane *pix;
	int i, ret;

	ret = mtk_nr_m2m_try_fmt_mplane(file, fh, f);
	if (ret)
		return ret;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);

	if (vb2_is_streaming(vq)) {
		pr_err("[NR][M2M] queue%d busy\n", f->type);
		return -EBUSY;
	}

	if (V4L2_TYPE_IS_OUTPUT(f->type))
		frame = &ctx->s_frame;
	else
		frame = &ctx->d_frame;

	pix = &f->fmt.pix_mp;
	frame->fmt = mtk_nr_find_fmt(&pix->pixelformat, NULL, 0);
	if (!frame->fmt)
		return -EINVAL;

	for (i = 0; i < frame->fmt->num_planes; i++) {
		frame->payload[i] = pix->plane_fmt[i].sizeimage;
		frame->pitch[i] = pix->plane_fmt[i].bytesperline;
	}

	mtk_nr_set_frame_size(frame, pix->width, pix->height);

	if (V4L2_TYPE_IS_OUTPUT(f->type))
		ctx->field_mode = f->fmt.pix_mp.field;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		mtk_nr_ctx_state_lock_set(MTK_NR_PARAMS | MTK_NR_DST_FMT, ctx);
	else
		mtk_nr_ctx_state_lock_set(MTK_NR_PARAMS | MTK_NR_SRC_FMT, ctx);

	ctx->d_addr.y = 0;
	ctx->d_addr.c = 0;
	ctx->s_buf.vb = NULL;
	ctx->s_buf.addr.y = 0;
	ctx->s_buf.addr.c = 0;

	return 0;
}

static int mtk_nr_m2m_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *reqbufs)
{
	int ret;
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	if (reqbufs->count == 0) {
		if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			mtk_nr_ctx_state_lock_clear(MTK_NR_SRC_FMT, ctx);
		else
			mtk_nr_ctx_state_lock_clear(MTK_NR_DST_FMT, ctx);
	}

	ret = v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);

	return ret;
}

static int mtk_nr_m2m_expbuf(struct file *file, void *fh, struct v4l2_exportbuffer *eb)
{
	int ret;
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	ret = v4l2_m2m_expbuf(file, ctx->m2m_ctx, eb);

	return ret;
}

static int mtk_nr_m2m_querybuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret;
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	ret = v4l2_m2m_querybuf(file, ctx->m2m_ctx, buf);

	return ret;
}

static int mtk_nr_m2m_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret;
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);

	return ret;
}

static int mtk_nr_m2m_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	int ret;
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	ret = v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);

	return ret;
}

static int mtk_nr_m2m_streamon(struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret;
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	if (V4L2_TYPE_IS_OUTPUT(type)) {
		if (!mtk_nr_ctx_state_is_set(MTK_NR_SRC_FMT, ctx))
			return -EINVAL;
	} else if (!mtk_nr_ctx_state_is_set(MTK_NR_DST_FMT, ctx)) {
		return -EINVAL;
	}

	ret = v4l2_m2m_streamon(file, ctx->m2m_ctx, type);

	return ret;
}

static int mtk_nr_m2m_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	int ret;
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	ret = v4l2_m2m_streamoff(file, ctx->m2m_ctx, type);

	return ret;
}

static int mtk_nr_m2m_set_level(struct file *file, void *fh, unsigned int level)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(fh);

	ctx->nr_level.u4TotalLevel = (level & 0xFF000000) >> 24;
	ctx->nr_level.u4BnrLevel = (level & 0x00FF0000) >> 16;
	ctx->nr_level.u4MnrLevel = (level & 0x0000FF00) >> 8;
	ctx->nr_level.u4FnrLevel = level & 0x000000FF;

	return 0;
}

static const struct v4l2_ioctl_ops mtk_nr_m2m_ioctl_ops = {
	.vidioc_querycap = mtk_nr_m2m_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = mtk_nr_m2m_enum_fmt_mplane,
	.vidioc_enum_fmt_vid_out_mplane = mtk_nr_m2m_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane = mtk_nr_m2m_g_fmt_mplane,
	.vidioc_g_fmt_vid_out_mplane = mtk_nr_m2m_g_fmt_mplane,
	.vidioc_try_fmt_vid_cap_mplane = mtk_nr_m2m_try_fmt_mplane,
	.vidioc_try_fmt_vid_out_mplane = mtk_nr_m2m_try_fmt_mplane,
	.vidioc_s_fmt_vid_cap_mplane = mtk_nr_m2m_s_fmt_mplane,
	.vidioc_s_fmt_vid_out_mplane = mtk_nr_m2m_s_fmt_mplane,
	.vidioc_reqbufs = mtk_nr_m2m_reqbufs,
	.vidioc_expbuf = mtk_nr_m2m_expbuf,
	.vidioc_querybuf = mtk_nr_m2m_querybuf,
	.vidioc_qbuf = mtk_nr_m2m_qbuf,
	.vidioc_dqbuf = mtk_nr_m2m_dqbuf,
	.vidioc_streamon = mtk_nr_m2m_streamon,
	.vidioc_streamoff = mtk_nr_m2m_streamoff,
	.vidioc_s_input = mtk_nr_m2m_set_level,
};

static int mtk_nr_m2m_queue_init(void *priv, struct vb2_queue *src_vq, struct vb2_queue *dst_vq)
{
	struct mtk_nr_ctx *ctx = priv;
	int ret;

	memset(src_vq, 0, sizeof(*src_vq));
	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	src_vq->drv_priv = ctx;
	src_vq->ops = &mtk_nr_m2m_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->dev = &ctx->nr_dev->pdev->dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	memset(dst_vq, 0, sizeof(*dst_vq));
	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	dst_vq->drv_priv = ctx;
	dst_vq->ops = &mtk_nr_m2m_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->dev = &ctx->nr_dev->pdev->dev;

	return vb2_queue_init(dst_vq);
}

static int mtk_nr_m2m_open(struct file *file)
{
	struct mtk_nr_dev *nr = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_nr_ctx *ctx = NULL;
	int ret;

	if (mutex_lock_interruptible(&nr->lock))
		return -ERESTARTSYS;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto err_ctx_alloc;
	}

	mutex_init(&ctx->qlock);
	mutex_init(&ctx->slock);
	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;

	ctx->nr_dev = nr;

	ctx->s_frame.fmt = mtk_nr_get_format(0);
	ctx->d_frame.fmt = mtk_nr_get_format(0);

	ctx->state = MTK_NR_CTX_M2M;
	ctx->flags = 0;

	INIT_WORK(&ctx->work, mtk_nr_m2m_worker);
	ctx->m2m_ctx = v4l2_m2m_ctx_init(nr->m2m.m2m_dev, ctx, mtk_nr_m2m_queue_init);
	if (IS_ERR(ctx->m2m_ctx)) {
		pr_err("[NR][M2M] Failed to initialize m2m context\n");
		ret = PTR_ERR(ctx->m2m_ctx);
		goto err_ctx_alloc;
	}
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	if (nr->m2m.refcnt++ == 0)
		set_bit(ST_M2M_OPEN, &nr->state);

	mutex_unlock(&nr->lock);

	return 0;

err_ctx_alloc:
	mutex_unlock(&nr->lock);

	return ret;
}

static int mtk_nr_m2m_release(struct file *file)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_nr_dev *nr = ctx->nr_dev;

	flush_workqueue(nr->workqueue);
	mutex_lock(&nr->lock);
	v4l2_m2m_ctx_release(ctx->m2m_ctx);
	mutex_destroy(&ctx->qlock);

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	if (nr->m2m.refcnt > 0) {
		nr->m2m.refcnt--;
	} else {
		clear_bit(ST_M2M_OPEN, &nr->state);
		nr->m2m.refcnt = 0;
	}
	kfree(ctx);

	mutex_unlock(&nr->lock);

	return 0;
}

static unsigned int mtk_nr_m2m_poll(struct file *file, struct poll_table_struct *wait)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_nr_dev *nr = ctx->nr_dev;
	int ret;

	if (mutex_lock_interruptible(&nr->lock))
		return -ERESTARTSYS;

	ret = v4l2_m2m_poll(file, ctx->m2m_ctx, wait);
	mutex_unlock(&nr->lock);

	return ret;
}

static int mtk_nr_m2m_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct mtk_nr_ctx *ctx = fh_to_ctx(file->private_data);
	struct mtk_nr_dev *nr = ctx->nr_dev;
	int ret;

	if (mutex_lock_interruptible(&nr->lock))
		return -ERESTARTSYS;

	ret = v4l2_m2m_mmap(file, ctx->m2m_ctx, vma);

	mutex_unlock(&nr->lock);

	return ret;
}


static const struct v4l2_file_operations mtk_nr_m2m_fops = {
	.owner = THIS_MODULE,
	.open = mtk_nr_m2m_open,
	.release = mtk_nr_m2m_release,
	.poll = mtk_nr_m2m_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = mtk_nr_m2m_mmap,
};

static void mtk_nr_m2m_device_run(void *priv)
{
	struct mtk_nr_ctx *ctx = priv;

	queue_work(ctx->nr_dev->workqueue, &ctx->work);
}

static void mtk_nr_m2m_job_abort(void *priv)
{
	mtk_nr_m2m_job_cancel((struct mtk_nr_ctx *)priv);
}


static struct v4l2_m2m_ops mtk_nr_m2m_ops = {
	.device_run = mtk_nr_m2m_device_run,
	.job_abort = mtk_nr_m2m_job_abort,
};

int mtk_nr_register_m2m_device(struct mtk_nr_dev *nr)
{
	int ret;

	nr->vdev.fops = &mtk_nr_m2m_fops;
	nr->vdev.ioctl_ops = &mtk_nr_m2m_ioctl_ops;
	nr->vdev.release = video_device_release_empty;
	nr->vdev.lock = &nr->lock;
	nr->vdev.vfl_dir = VFL_DIR_M2M;
	nr->vdev.v4l2_dev = &nr->v4l2_dev;
	snprintf(nr->vdev.name, sizeof(nr->vdev.name), "%s:m2m", MTK_NR_MODULE_NAME);
	video_set_drvdata(&nr->vdev, nr);

	nr->m2m.m2m_dev = v4l2_m2m_init(&mtk_nr_m2m_ops);
	if (IS_ERR(nr->m2m.m2m_dev)) {
		pr_err("[NR]failed to init v4l2-m2m device\n");
		ret = PTR_ERR(nr->m2m.m2m_dev);
		goto err_m2m_init;
	}

	v4l2_disable_ioctl_locking(&nr->vdev, VIDIOC_QBUF);
	v4l2_disable_ioctl_locking(&nr->vdev, VIDIOC_DQBUF);
	v4l2_disable_ioctl_locking(&nr->vdev, VIDIOC_S_CTRL);

	ret = video_register_device(&nr->vdev, VFL_TYPE_GRABBER, 2);
	if (ret) {
		pr_err("[NR]failed to register video device\n");
		goto err_vdev_register;
	}

	return 0;

err_vdev_register:
	v4l2_m2m_release(nr->m2m.m2m_dev);

err_m2m_init:

	return ret;
}

void mtk_nr_unregister_m2m_device(struct mtk_nr_dev *nr)
{
	video_device_release(&nr->vdev);
	v4l2_m2m_release(nr->m2m.m2m_dev);
}
