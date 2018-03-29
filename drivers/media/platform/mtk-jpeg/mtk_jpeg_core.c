/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
 *         Rick Chang <rick.chang@mediatek.com>
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


#include <linux/clk.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <soc/mediatek/smi.h>
#include <asm/dma-iommu.h>

#include "mtk_jpeg_core.h"
#include "mtk_jpeg_parse.h"

static struct mtk_jpeg_fmt mtk_jpeg_formats[] = {
	{
		.name		= "JPEG JFIF",
		.fourcc		= V4L2_PIX_FMT_JPEG,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_OUTPUT,
	},
	{
		.name		= "YUV 4:2:0 contiguous 3-planar, Y/Cb/Cr",
		.fourcc		= V4L2_PIX_FMT_YUV420,
		.depth		= 12,
		.colplanes	= 1,
		.h_align	= 4,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_CAPTURE,
	},
	{
		.name		= "YUV 4:2:2 contiguous 3-planar, Y/Cb/Cr",
		.fourcc		= V4L2_PIX_FMT_YUV422P,
		.depth		= 16,
		.colplanes	= 1,
		.h_align	= 4,
		.v_align	= 3,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_CAPTURE,
	},
	{
		.name		= "Gray",
		.fourcc		= V4L2_PIX_FMT_GREY,
		.depth		= 8,
		.colplanes	= 1,
		.h_align	= 4,
		.v_align	= 3,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_CAPTURE,
	},
};
#define MTK_JPEG_NUM_FORMATS ARRAY_SIZE(mtk_jpeg_formats)

static int debug;
module_param(debug, int, S_IRUGO | S_IWUSR);

#ifdef CONFIG_MTK_IOMMU
static int mtk_jpeg_iommu_init(struct device *dev)
{
	struct device_node *np;
	struct platform_device *pdev;
	int err;

	np = of_parse_phandle(dev->of_node, "iommus", 0);
	if (!np) {
		pr_debug("can't find iommus node\n");
		return 0;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		of_node_put(np);
		pr_debug("can't find iommu device by node\n");
		return -1;
	}

	pr_debug("%s() %s\n", __func__, dev_name(&pdev->dev));

	err = arm_iommu_attach_device(dev, pdev->dev.archdata.iommu);

	if (err) {
		of_node_put(np);
		pr_err("iommu_dma_attach_device fail %d\n", err);
		return -1;
	}

	return 0;
}

static void mtk_jpeg_iommu_deinit(struct device *dev)
{
	arm_iommu_detach_device(dev);
}
#endif

static inline struct mtk_jpeg_ctx *mtk_jpeg_fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_jpeg_ctx, fh);
}

static int mtk_jpeg_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	strlcpy(cap->driver, MTK_JPEG_NAME " decoder", sizeof(cap->driver));
	strlcpy(cap->card, MTK_JPEG_NAME " decoder", sizeof(cap->card));
	cap->bus_info[0] = 0;
	cap->capabilities = V4L2_CAP_STREAMING |
			    V4L2_CAP_VIDEO_M2M |
			    V4L2_CAP_VIDEO_CAPTURE |
			    V4L2_CAP_VIDEO_OUTPUT;

	return 0;
}

static int mtk_jpeg_enum_fmt(struct mtk_jpeg_fmt *mtk_jpeg_formats, int n,
			     struct v4l2_fmtdesc *f, u32 type)
{
	int i, num = 0;

	for (i = 0; i < n; ++i) {
		if (mtk_jpeg_formats[i].flags & type) {
			if (num == f->index)
				break;
			++num;
		}
	}

	if (i >= n)
		return -EINVAL;

	strlcpy(f->description, mtk_jpeg_formats[i].name,
				 sizeof(f->description));
	f->pixelformat = mtk_jpeg_formats[i].fourcc;

	return 0;
}

static int mtk_jpeg_enum_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	return mtk_jpeg_enum_fmt(mtk_jpeg_formats, MTK_JPEG_NUM_FORMATS, f,
				 MTK_JPEG_FMT_FLAG_DEC_CAPTURE);
}

static int mtk_jpeg_enum_fmt_vid_out(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	return mtk_jpeg_enum_fmt(mtk_jpeg_formats, MTK_JPEG_NUM_FORMATS, f,
				 MTK_JPEG_FMT_FLAG_DEC_OUTPUT);
}

static struct mtk_jpeg_q_data *mtk_jpeg_get_q_data(struct mtk_jpeg_ctx *ctx,
						   enum v4l2_buf_type type)
{
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return &ctx->out_q;
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return &ctx->cap_q;
	return NULL;
}

static int mtk_jpeg_g_fmt(struct file *file, void *priv,
			  struct v4l2_format *f)
{
	struct vb2_queue *vq;
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	if (!V4L2_TYPE_IS_OUTPUT(f->type) && ctx->state == MTK_JPEG_INIT)
		return -EINVAL;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);
	if (q_data == NULL)
		return -EINVAL;

	pix->width = q_data->w;
	pix->height = q_data->h;
	pix->field = V4L2_FIELD_NONE;
	pix->pixelformat = q_data->fmt->fourcc;
	pix->bytesperline = 0;
	if (q_data->fmt->fourcc != V4L2_PIX_FMT_JPEG) {
		u32 bpl = q_data->w;

		if (q_data->fmt->colplanes == 1)
			bpl = (bpl * q_data->fmt->depth) >> 3;
		pix->bytesperline = bpl;
	}
	pix->sizeimage = q_data->size;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) g_fmt:%s wxh:%ux%u, size:%u\n",
		f->type, q_data->fmt->name,
		pix->width, pix->height, pix->sizeimage);

	return 0;
}

static struct mtk_jpeg_fmt *mtk_jpeg_find_format(struct mtk_jpeg_ctx *ctx,
						 u32 pixelformat,
						 unsigned int fmt_type)
{
	unsigned int k, fmt_flag;

	fmt_flag = (fmt_type == MTK_JPEG_FMT_TYPE_OUTPUT) ?
		   MTK_JPEG_FMT_FLAG_DEC_OUTPUT :
		   MTK_JPEG_FMT_FLAG_DEC_CAPTURE;

	for (k = 0; k < MTK_JPEG_NUM_FORMATS; k++) {
		struct mtk_jpeg_fmt *fmt = &mtk_jpeg_formats[k];

		if (fmt->fourcc == pixelformat && fmt->flags & fmt_flag)
			return fmt;
	}

	return NULL;
}

static void mtk_jpeg_bound_align_image(u32 *w, unsigned int wmin,
				       unsigned int wmax, unsigned int walign,
				       u32 *h, unsigned int hmin,
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

static int mtk_jpeg_try_fmt(struct v4l2_format *f, struct mtk_jpeg_fmt *fmt,
			    struct mtk_jpeg_ctx *ctx, int q_type)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;

	if (pix->field == V4L2_FIELD_ANY)
		pix->field = V4L2_FIELD_NONE;
	else if (pix->field != V4L2_FIELD_NONE)
		return -EINVAL;

	if (q_type == MTK_JPEG_FMT_TYPE_OUTPUT) {
		mtk_jpeg_bound_align_image(&pix->width, MTK_JPEG_MIN_WIDTH,
					   MTK_JPEG_MAX_WIDTH, 0,
					   &pix->height, MTK_JPEG_MIN_HEIGHT,
					   MTK_JPEG_MAX_HEIGHT, 0);
		if (pix->sizeimage < 0)
			pix->sizeimage = 0;
		/*
		 * Source size must be aligned to 128 and extra 128 bytes
		 */
		pix->sizeimage = mtk_jpeg_align(pix->sizeimage, 128) + 128;
		if (fmt->fourcc == V4L2_PIX_FMT_JPEG)
			pix->bytesperline = 0;
		return 0;
	}

	/* type is MTK_JPEG_FMT_TYPE_CAPTURE */
	mtk_jpeg_bound_align_image(&pix->width, MTK_JPEG_MIN_WIDTH,
				   MTK_JPEG_MAX_WIDTH, fmt->h_align,
				   &pix->height, MTK_JPEG_MIN_HEIGHT,
				   MTK_JPEG_MAX_HEIGHT, fmt->v_align);
	pix->sizeimage = (pix->width * pix->height * fmt->depth) >> 3;

	if (ctx->state != MTK_JPEG_INIT) {
		struct mtk_jpeg_q_data *q_data = &ctx->cap_q;

		if (pix->width < q_data->w)
			pix->width = q_data->w;
		if (pix->height < q_data->h)
			pix->height = q_data->h;
		if (pix->sizeimage < q_data->size)
			pix->sizeimage = q_data->size;
	}

	pix->bytesperline = (pix->width * fmt->depth) >> 3;

	return 0;
}

static int mtk_jpeg_try_fmt_vid_cap(struct file *file, void *priv,
				    struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;
	struct mtk_jpeg_q_data *q_data = &ctx->cap_q;

	if (ctx->state != MTK_JPEG_INIT &&
	    f->fmt.pix.pixelformat != q_data->fmt->fourcc) {
		f->fmt.pix.pixelformat = q_data->fmt->fourcc;
	}

	fmt = mtk_jpeg_find_format(ctx, f->fmt.pix.pixelformat,
				   MTK_JPEG_FMT_TYPE_CAPTURE);
	if (!fmt) {
		v4l2_err(&ctx->jpeg->v4l2_dev,
			 "Fourcc format (0x%08x) invalid\n",
			 f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	return mtk_jpeg_try_fmt(f, fmt, ctx, MTK_JPEG_FMT_TYPE_CAPTURE);
}

static int mtk_jpeg_try_fmt_vid_out(struct file *file, void *priv,
				    struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(ctx,
				   f->fmt.pix.pixelformat,
				   MTK_JPEG_FMT_TYPE_OUTPUT);
	if (!fmt) {
		v4l2_err(&ctx->jpeg->v4l2_dev,
			 "Fourcc format (0x%08x) invalid\n",
			 f->fmt.pix.pixelformat);
		return -EINVAL;
	}

	return mtk_jpeg_try_fmt(f, fmt, ctx, MTK_JPEG_FMT_TYPE_OUTPUT);
}

static int mtk_jpeg_s_fmt(struct mtk_jpeg_ctx *ctx, struct v4l2_format *f)
{
	struct vb2_queue *vq;
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	unsigned int f_type;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);
	if (q_data == NULL)
		return -EINVAL;

	if (vb2_is_busy(vq)) {
		v4l2_err(&jpeg->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	f_type = V4L2_TYPE_IS_OUTPUT(f->type) ?
			 MTK_JPEG_FMT_TYPE_OUTPUT : MTK_JPEG_FMT_TYPE_CAPTURE;

	q_data->fmt = mtk_jpeg_find_format(ctx, pix->pixelformat, f_type);
	q_data->w = pix->width;
	q_data->h = pix->height;
	q_data->size = pix->sizeimage;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) s_fmt:%s wxh:%ux%u, size:%u\n",
		f->type, q_data->fmt->name,
		pix->width, pix->height, pix->sizeimage);

	return 0;
}

static int mtk_jpeg_s_fmt_vid_cap(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_try_fmt_vid_cap(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt(mtk_jpeg_fh_to_ctx(priv), f);
}

static int mtk_jpeg_s_fmt_vid_out(struct file *file, void *priv,
				  struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_try_fmt_vid_out(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt(mtk_jpeg_fh_to_ctx(priv), f);
}

static void mtk_jpeg_queue_src_chg_event(struct mtk_jpeg_ctx *ctx)
{
	static const struct v4l2_event ev_src_ch = {
		.type = V4L2_EVENT_SOURCE_CHANGE,
		.u.src_change.changes =
		V4L2_EVENT_SRC_CH_RESOLUTION,
	};

	v4l2_event_queue_fh(&ctx->fh, &ev_src_ch);
}

static int mtk_jpeg_subscribe_event(struct v4l2_fh *fh,
				    const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_SOURCE_CHANGE:
		return v4l2_src_change_event_subscribe(fh, sub);
	default:
		return -EINVAL;
	}
}

static int mtk_jpeg_g_crop(struct file *file, void *priv,
			struct v4l2_crop *cr)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_q_data *q_data = &ctx->out_q;

	if (V4L2_TYPE_IS_OUTPUT(cr->type))
		return -EINVAL;
	if (ctx->state == MTK_JPEG_INIT)
		return -EINVAL;

	cr->c.left = 0;
	cr->c.top = 0;
	cr->c.width = q_data->w;
	cr->c.height = q_data->h;
	return 0;
}


static const struct v4l2_ioctl_ops mtk_jpeg_ioctl_ops = {
	.vidioc_querycap                = mtk_jpeg_querycap,
	.vidioc_enum_fmt_vid_cap        = mtk_jpeg_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out        = mtk_jpeg_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_cap           = mtk_jpeg_g_fmt,
	.vidioc_g_fmt_vid_out           = mtk_jpeg_g_fmt,
	.vidioc_try_fmt_vid_cap         = mtk_jpeg_try_fmt_vid_cap,
	.vidioc_try_fmt_vid_out         = mtk_jpeg_try_fmt_vid_out,
	.vidioc_s_fmt_vid_cap           = mtk_jpeg_s_fmt_vid_cap,
	.vidioc_s_fmt_vid_out           = mtk_jpeg_s_fmt_vid_out,
	.vidioc_g_crop                  = mtk_jpeg_g_crop,
	.vidioc_reqbufs                 = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf                = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf                    = v4l2_m2m_ioctl_qbuf,
	.vidioc_dqbuf                   = v4l2_m2m_ioctl_dqbuf,
	.vidioc_streamon                = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff               = v4l2_m2m_ioctl_streamoff,

	.vidioc_subscribe_event         = mtk_jpeg_subscribe_event,
	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static int mtk_jpeg_queue_setup(struct vb2_queue *q,
				const struct v4l2_format *fmt,
				unsigned int *num_buffers,
				unsigned int *num_planes,
				unsigned int sizes[],
				void *alloc_ctxs[])
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_jpeg_q_data *q_data = NULL;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	q_data = mtk_jpeg_get_q_data(ctx, q->type);
	if (q_data == NULL)
		return -EINVAL;

	*num_planes = 1;
	sizes[0] = q_data->size;
	alloc_ctxs[0] = jpeg->alloc_ctx;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) buf_req count=%u size=%u\n",
		 q->type, *num_buffers, sizes[0]);

	return 0;
}

static int mtk_jpeg_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_q_data *q_data = NULL;

	q_data = mtk_jpeg_get_q_data(ctx, vb->vb2_queue->type);
	if (q_data == NULL)
		return -EINVAL;
	vb2_set_plane_payload(vb, 0, q_data->size);

	return 0;
}

static bool mtk_jpeg_check_resolution_change(struct mtk_jpeg_ctx *ctx,
					struct mtk_jpeg_dec_param *param)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_q_data *q_data;

	q_data = &ctx->out_q;
	if (q_data->w != param->pic_w || q_data->h != param->pic_h) {
		v4l2_dbg(1, debug, &jpeg->v4l2_dev, "Picture size change\n");
		return true;
	}

	q_data = &ctx->cap_q;
	if (q_data->fmt != mtk_jpeg_find_format(ctx,
					   param->dst_fourcc,
					   MTK_JPEG_FMT_TYPE_CAPTURE)) {
		v4l2_dbg(1, debug, &jpeg->v4l2_dev, "format change\n");
	    return true;
	}
	return false;
}

static void mtk_jpeg_set_queue_data(struct mtk_jpeg_ctx *ctx,
					struct mtk_jpeg_dec_param *param)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_q_data *q_data;

	q_data = &ctx->out_q;
	q_data->w = param->pic_w;
	q_data->h = param->pic_h;

	q_data = &ctx->cap_q;
	q_data->w = param->dec_w;
	q_data->h = param->dec_h;
	q_data->size = param->dec_size;
	q_data->fmt = mtk_jpeg_find_format(ctx,
					   param->dst_fourcc,
					   MTK_JPEG_FMT_TYPE_CAPTURE);

	v4l2_dbg(1, debug, &jpeg->v4l2_dev,
		"set_parse cap:%s pic(%u, %u), buf(%u, %u)\n",
		q_data->fmt->name,
		param->pic_w, param->pic_h, param->dec_w, param->dec_h);
}

static void mtk_jpeg_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_dec_param *param;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
#if MTK_JPEG_BENCHMARK
	struct timeval begin, end;
#endif
	bool header_valid;

	v4l2_dbg(2, debug, &jpeg->v4l2_dev, "(%d) buf_q id=%d, vb=%p",
			 vb->vb2_queue->type, vb->v4l2_buf.index, vb);

	if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		struct mtk_jpeg_src_buf *jpeg_src_buf =
				container_of(vb, struct mtk_jpeg_src_buf, b);

		param = &jpeg_src_buf->dec_param;

		memset(param, 0, sizeof(*param));
#if MTK_JPEG_BENCHMARK
		do_gettimeofday(&begin);
#endif
		header_valid =
			mtk_jpeg_parse(param, (u8 *)vb2_plane_vaddr(vb, 0),
					vb2_get_plane_payload(vb, 0));
#if MTK_JPEG_BENCHMARK
		do_gettimeofday(&end);
		ctx->total_parse_cnt++;
		ctx->total_parse_time +=
				((end.tv_sec - begin.tv_sec) * 1000000 +
				end.tv_usec - begin.tv_usec);
#endif
		if (!header_valid) {
			v4l2_err(&jpeg->v4l2_dev, "Header invalid.\n");
			vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
			return;
		}

		/*
		 * Check resolution change first because some applications
		 * monitor the event for the process of first frame
		 */
		if (mtk_jpeg_check_resolution_change(ctx, param)) {
			struct vb2_queue *dst_vq = v4l2_m2m_get_vq(
				ctx->fh.m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE);
			mtk_jpeg_set_queue_data(ctx, &jpeg_src_buf->dec_param);
			mtk_jpeg_queue_src_chg_event(ctx);
			if (vb2_is_streaming(dst_vq))
				ctx->state = MTK_JPEG_SOURCE_CHANGE;
			else
				ctx->state = MTK_JPEG_RUNNING;
		} else if (ctx->state == MTK_JPEG_INIT) {
			mtk_jpeg_set_queue_data(ctx, param);
			ctx->state = MTK_JPEG_RUNNING;
		}
	}

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, vb);
}

static void *mtk_jpeg_buf_remove(struct mtk_jpeg_ctx *ctx,
				 enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static int mtk_jpeg_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	int ret = 0;

	ret = pm_runtime_get_sync(ctx->jpeg->dev);

	return ret > 0 ? 0 : ret;
}

static void mtk_jpeg_stop_streaming(struct vb2_queue *q)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_buffer *vb;

	/*
	 * STREAMOFF is an acknowledgment for source change event.
	 * Before STREAMOFF, we still have to return the old resolution and
	 * subsampling. Update capture queue when the stream is off.
	 */
	if (ctx->state == MTK_JPEG_SOURCE_CHANGE &&
	    !V4L2_TYPE_IS_OUTPUT(q->type)) {
		ctx->state = MTK_JPEG_RUNNING;
	}

	vb = mtk_jpeg_buf_remove(ctx, q->type);
	while (vb != NULL) {
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
		vb = mtk_jpeg_buf_remove(ctx, q->type);
	}

	pm_runtime_put_sync(ctx->jpeg->dev);
}

static struct vb2_ops mtk_jpeg_qops = {
	.queue_setup        = mtk_jpeg_queue_setup,
	.buf_prepare        = mtk_jpeg_buf_prepare,
	.buf_queue          = mtk_jpeg_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.start_streaming    = mtk_jpeg_start_streaming,
	.stop_streaming     = mtk_jpeg_stop_streaming,
};

static void mtk_jpeg_device_run(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;

	queue_work(ctx->jpeg->workqueue, &ctx->work);
}

static int mtk_jpeg_job_ready(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;

	return (ctx->state == MTK_JPEG_RUNNING) ? 1 : 0;
}

static void mtk_jpeg_job_abort(void *priv)
{
}

static struct v4l2_m2m_ops mtk_jpeg_m2m_ops = {
	.device_run = mtk_jpeg_device_run,
	.job_ready  = mtk_jpeg_job_ready,
	.job_abort  = mtk_jpeg_job_abort,
};

static int mtk_jpeg_queue_init(void *priv, struct vb2_queue *src_vq,
			       struct vb2_queue *dst_vq)
{
	struct mtk_jpeg_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	src_vq->io_modes = VB2_MMAP | VB2_USERPTR;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_jpeg_src_buf);
	src_vq->ops = &mtk_jpeg_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->jpeg->lock;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	dst_vq->io_modes = VB2_MMAP | VB2_USERPTR;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mtk_jpeg_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->jpeg->lock;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static void mtk_jpeg_clk_on(struct mtk_jpeg_dev *jpeg)
{
	int ret;

	ret = mtk_smi_larb_get(jpeg->larb);
	if (ret)
		dev_err(jpeg->dev, "mtk_smi_larb_get larbvdec fail %d\n", ret);
	clk_prepare_enable(jpeg->clk_venc_jdec_smi);
	clk_prepare_enable(jpeg->clk_venc_jdec);
}

static void mtk_jpeg_clk_off(struct mtk_jpeg_dev *jpeg)
{
	clk_disable_unprepare(jpeg->clk_venc_jdec);
	clk_disable_unprepare(jpeg->clk_venc_jdec_smi);
	mtk_smi_larb_put(jpeg->larb);
}

static void mtk_jpeg_set_dec_src(struct mtk_jpeg_ctx *ctx,
				 struct vb2_buffer *src_buf,
				 struct mtk_jpeg_bs *bs)
{
	bs->str_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	bs->end_addr = bs->str_addr +
			 mtk_jpeg_align(vb2_get_plane_payload(src_buf, 0), 16);
	bs->size = mtk_jpeg_align(vb2_plane_size(src_buf, 0), 128);
}

static int mtk_jpeg_set_dec_dst(struct mtk_jpeg_ctx *ctx,
				struct mtk_jpeg_dec_param *param,
				struct vb2_buffer *dst_buf,
				struct mtk_jpeg_fb *fb)
{
	if (vb2_plane_size(dst_buf, 0) < param->dec_size) {
		dev_err(ctx->jpeg->dev,
			"buffer size is underflow (%lu < %u)\n",
			vb2_plane_size(dst_buf, 0),
			param->dec_size);
		return -EINVAL;
	}

	fb->plane_addr[0] = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	fb->plane_addr[1] = fb->plane_addr[0] + param->y_size;
	fb->plane_addr[2] = fb->plane_addr[1] + param->uv_size;

	return 0;
}

static void mtk_jpeg_worker(struct work_struct *work)
{
	struct mtk_jpeg_ctx *ctx =
				container_of(work, struct mtk_jpeg_ctx, work);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct vb2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	unsigned long flags;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	struct mtk_jpeg_bs bs;
	struct mtk_jpeg_fb fb;
#if MTK_JPEG_BENCHMARK
	struct timeval begin, end;
#endif

	mutex_lock(&jpeg->dev_lock);
	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	jpeg_src_buf = container_of(src_buf, struct mtk_jpeg_src_buf, b);

	spin_lock_irqsave(&jpeg->irq_lock, flags);
	mtk_jpeg_set_dec_src(ctx, src_buf, &bs);
	if (mtk_jpeg_set_dec_dst(ctx, &jpeg_src_buf->dec_param, dst_buf, &fb)) {
		spin_unlock_irqrestore(&jpeg->irq_lock, flags);
		dev_err(jpeg->dev, "Invalid parameter\n");
		goto worker_end;
	}

	ctx->dec_irq_ret = 0;
	mtk_jpeg_dec_reset(jpeg->dec_reg_base);
	mtk_jpeg_dec_set_config(jpeg->dec_reg_base,
				&jpeg_src_buf->dec_param, &bs, &fb);

#if MTK_JPEG_BENCHMARK
	do_gettimeofday(&begin);
#endif
	mtk_jpeg_dec_start(jpeg->dec_reg_base);
	spin_unlock_irqrestore(&jpeg->irq_lock, flags);

	if (!wait_for_completion_timeout(&ctx->completion,
					 msecs_to_jiffies(4000))) {
		dev_err(jpeg->dev, "decode timeout\n");
		goto worker_end;
	}
#if MTK_JPEG_BENCHMARK
	do_gettimeofday(&end);
	ctx->total_enc_dec_cnt++;
	ctx->total_enc_dec_time +=
		((end.tv_sec - begin.tv_sec) * 1000000 +
			end.tv_usec - begin.tv_usec);
#endif

	if (ctx->dec_irq_ret >= MTK_JPEG_DEC_RESULT_UNDERFLOW)
		mtk_jpeg_dec_reset(jpeg->dec_reg_base);

	if (ctx->dec_irq_ret != MTK_JPEG_DEC_RESULT_EOF_DONE) {
		dev_err(jpeg->dev, "decode failed\n");
		goto worker_end;
	}

	vb2_set_plane_payload(dst_buf, 0, jpeg_src_buf->dec_param.y_size +
			(jpeg_src_buf->dec_param.uv_size << 1));
	buf_state = VB2_BUF_STATE_DONE;

worker_end:
	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	mutex_unlock(&jpeg->dev_lock);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
}

static irqreturn_t mtk_jpeg_dec_irq(int irq, void *priv)
{
	struct mtk_jpeg_dev *jpeg = priv;
	struct mtk_jpeg_ctx *ctx;
	u32 dec_ret;

	ctx = v4l2_m2m_get_curr_priv(jpeg->m2m_dev);
	if (ctx == NULL) {
		v4l2_err(&jpeg->v4l2_dev, "Context is NULL\n");
		mutex_unlock(&jpeg->dev_lock);
		return IRQ_HANDLED;
	}

	spin_lock(&jpeg->irq_lock);
	dec_ret = mtk_jpeg_dec_get_int_status(jpeg->dec_reg_base);
	ctx->dec_irq_ret = mtk_jpeg_dec_enum_result(dec_ret);
	spin_unlock(&jpeg->irq_lock);

	complete(&ctx->completion);
	return IRQ_HANDLED;
}

static int mtk_jpeg_open(struct file *file)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_jpeg_ctx *ctx;
	int ret = 0;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (mutex_lock_interruptible(&jpeg->lock)) {
		ret = -ERESTARTSYS;
		goto free;
	}

	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->jpeg = jpeg;
	ctx->out_q.fmt = mtk_jpeg_find_format(ctx, V4L2_PIX_FMT_JPEG,
					      MTK_JPEG_FMT_TYPE_OUTPUT);
	ctx->cap_q.fmt = mtk_jpeg_find_format(ctx, V4L2_PIX_FMT_YUV420,
					      MTK_JPEG_FMT_TYPE_CAPTURE);
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(jpeg->m2m_dev, ctx,
					    mtk_jpeg_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}

	init_completion(&ctx->completion);
	INIT_WORK(&ctx->work, mtk_jpeg_worker);

	mutex_unlock(&jpeg->lock);
#if MTK_JPEG_BENCHMARK
	do_gettimeofday(&ctx->jpeg_enc_dec_start);
#endif
	return 0;

error:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&jpeg->lock);
free:
	kfree(ctx);
	return ret;
}

static int mtk_jpeg_release(struct file *file)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(file->private_data);

#if MTK_JPEG_BENCHMARK
	struct timeval end;
	uint32_t total_time;

	do_gettimeofday(&end);
	total_time = (end.tv_sec - ctx->jpeg_enc_dec_start.tv_sec) * 1000000 +
		 end.tv_usec - ctx->jpeg_enc_dec_start.tv_usec;
	v4l2_err(&jpeg->v4l2_dev, "\n\nMTK_JPEG_BENCHMARK");
	v4l2_err(&jpeg->v4l2_dev, "  total_enc_dec_cnt: %u ",
				ctx->total_enc_dec_cnt);
	v4l2_err(&jpeg->v4l2_dev, "  total_enc_dec_time: %u us",
				ctx->total_enc_dec_time);
	v4l2_err(&jpeg->v4l2_dev, "  total_parse_cnt: %u ",
				ctx->total_parse_cnt);
	v4l2_err(&jpeg->v4l2_dev, "  total_parse_time: %u us",
				ctx->total_parse_time);
	v4l2_err(&jpeg->v4l2_dev, "  total_time: %u us",
		total_time);
	if (ctx->total_enc_dec_cnt) {
		v4l2_err(&jpeg->v4l2_dev, "  dec fps: %u",
			1000000 /
			(ctx->total_enc_dec_time / ctx->total_enc_dec_cnt));
		v4l2_err(&jpeg->v4l2_dev, "  avg fps: %u",
			1000000 /
			(total_time / ctx->total_enc_dec_cnt));
	}
#endif
	mutex_lock(&jpeg->lock);

	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);

	mutex_unlock(&jpeg->lock);
	return 0;
}

static const struct v4l2_file_operations mtk_jpeg_fops = {
	.owner          = THIS_MODULE,
	.open           = mtk_jpeg_open,
	.release        = mtk_jpeg_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = v4l2_m2m_fop_mmap,
};

static int mtk_jpeg_clk_init(struct mtk_jpeg_dev *jpeg)
{
	struct device_node *node;
	struct platform_device *pdev;

	node = of_parse_phandle(jpeg->dev->of_node, "larb", 0);
	if (!node)
		return -EINVAL;
	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -EINVAL;
	}
	jpeg->larb = &pdev->dev;

	jpeg->clk_venc_jdec = devm_clk_get(jpeg->dev, "venc-jpgdec");
	if (jpeg->clk_venc_jdec == NULL)
		return -EINVAL;

	jpeg->clk_venc_jdec_smi = devm_clk_get(jpeg->dev, "venc-jpgdec-smi");
	if (jpeg->clk_venc_jdec_smi == NULL)
		return -EINVAL;

	return 0;
}

static int mtk_jpeg_probe(struct platform_device *pdev)
{
	struct mtk_jpeg_dev *jpeg;
	struct resource *res;
	u32 dec_irq;
	int ret;

	jpeg = devm_kzalloc(&pdev->dev, sizeof(*jpeg), GFP_KERNEL);
	if (!jpeg)
		return -ENOMEM;

	mutex_init(&jpeg->lock);
	mutex_init(&jpeg->dev_lock);
	spin_lock_init(&jpeg->irq_lock);
	jpeg->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	jpeg->dec_reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(jpeg->dec_reg_base)) {
		ret = PTR_ERR(jpeg->dec_reg_base);
		dev_err(&pdev->dev, "devm_ioremap_resource failed.\n");
		return ret;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	dec_irq = platform_get_irq(pdev, 0);
	if (res == NULL || dec_irq < 0) {
		dev_err(&pdev->dev, "Failed to get dec_irq %d.\n", dec_irq);
		ret = -EINVAL;
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, dec_irq, mtk_jpeg_dec_irq, 0,
			       pdev->name, jpeg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request dec_irq %d (%d)\n",
			dec_irq, ret);
		ret = -EINVAL;
		goto err_req_irq;
	}

	ret = mtk_jpeg_clk_init(jpeg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to init clk, err %d\n", ret);
		ret = -EINVAL;
		goto err_clk_init;
	}

	jpeg->workqueue = alloc_workqueue(MTK_JPEG_NAME,
					  WQ_UNBOUND | WQ_MEM_RECLAIM, 1);
	if (!jpeg->workqueue) {
		dev_err(&pdev->dev, "unable to alloc workqueue\n");
		ret = -ENOMEM;
		goto err_alloc_workqueue;
	}

	ret = v4l2_device_register(&pdev->dev, &jpeg->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_dev_register;
	}

	jpeg->m2m_dev = v4l2_m2m_init(&mtk_jpeg_m2m_ops);
	if (IS_ERR(jpeg->m2m_dev)) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(jpeg->m2m_dev);
		goto err_m2m_init;
	}

	jpeg->alloc_ctx = vb2_dma_contig_init_ctx(&pdev->dev);
	if (IS_ERR(jpeg->alloc_ctx)) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to init memory allocator\n");
		ret = PTR_ERR(jpeg->alloc_ctx);
		goto err_alloc_ctx;
	}

	jpeg->dec_vdev = video_device_alloc();
	if (!jpeg->dec_vdev) {
		ret = -ENOMEM;
		goto err_dec_vdev_alloc;
	}
	snprintf(jpeg->dec_vdev->name, sizeof(jpeg->dec_vdev->name),
		 "%s-dec", MTK_JPEG_NAME);
	jpeg->dec_vdev->fops = &mtk_jpeg_fops;
	jpeg->dec_vdev->ioctl_ops = &mtk_jpeg_ioctl_ops;
	jpeg->dec_vdev->minor = -1;
	jpeg->dec_vdev->release = video_device_release;
	jpeg->dec_vdev->lock = &jpeg->lock;
	jpeg->dec_vdev->v4l2_dev = &jpeg->v4l2_dev;
	jpeg->dec_vdev->vfl_dir = VFL_DIR_M2M;

	ret = video_register_device(jpeg->dec_vdev, VFL_TYPE_GRABBER, 3);
	if (ret) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to register video device\n");
		goto err_dec_vdev_register;
	}

#ifdef CONFIG_MTK_IOMMU
	ret = mtk_jpeg_iommu_init(&pdev->dev);
	if (ret) {
		v4l2_err(&jpeg->v4l2_dev,
			 "Failed to attach iommu device err = %d\n", ret);
		goto err_dec_vdev_register;
	}
#endif

	video_set_drvdata(jpeg->dec_vdev, jpeg);
	v4l2_info(&jpeg->v4l2_dev,
		  "decoder device registered as /dev/video%d\n",
		  jpeg->dec_vdev->num);

	platform_set_drvdata(pdev, jpeg);

	pm_runtime_enable(&pdev->dev);

	return 0;

err_dec_vdev_register:
	video_device_release(jpeg->dec_vdev);

err_dec_vdev_alloc:
	vb2_dma_contig_cleanup_ctx(jpeg->alloc_ctx);

err_alloc_ctx:
	v4l2_m2m_release(jpeg->m2m_dev);

err_m2m_init:
	v4l2_device_unregister(&jpeg->v4l2_dev);

err_dev_register:
	destroy_workqueue(jpeg->workqueue);

err_alloc_workqueue:

err_clk_init:

err_req_irq:

	return ret;
}

static int mtk_jpeg_remove(struct platform_device *pdev)
{
	struct mtk_jpeg_dev *jpeg = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
#ifdef CONFIG_MTK_IOMMU
	mtk_jpeg_iommu_deinit(&pdev->dev);
#endif
	video_unregister_device(jpeg->dec_vdev);
	video_device_release(jpeg->dec_vdev);
	vb2_dma_contig_cleanup_ctx(jpeg->alloc_ctx);
	v4l2_m2m_release(jpeg->m2m_dev);
	v4l2_device_unregister(&jpeg->v4l2_dev);
	destroy_workqueue(jpeg->workqueue);

	return 0;
}

#if defined(CONFIG_PM_RUNTIME) || defined(CONFIG_PM_SLEEP)
static int mtk_jpeg_pm_suspend(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	mtk_jpeg_dec_reset(jpeg->dec_reg_base);
	mtk_jpeg_clk_off(jpeg);

	return 0;
}

static int mtk_jpeg_pm_resume(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	mtk_jpeg_clk_on(jpeg);
	mtk_jpeg_dec_reset(jpeg->dec_reg_base);

	return 0;
}
#endif /* CONFIG_PM_RUNTIME || CONFIG_PM_SLEEP */

#ifdef CONFIG_PM_SLEEP
static int mtk_jpeg_suspend(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_jpeg_pm_suspend(dev);
	return ret;
}

static int mtk_jpeg_resume(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_jpeg_pm_resume(dev);

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops mtk_jpeg_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_jpeg_suspend, mtk_jpeg_resume)
	SET_RUNTIME_PM_OPS(mtk_jpeg_pm_suspend, mtk_jpeg_pm_resume, NULL)
};

static const struct of_device_id mtk_jpeg_match[] = {
	{
		.compatible = "mediatek,mt2701-jpgdec",
		.data       = NULL,
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_jpeg_match);

static struct platform_driver mtk_jpeg_driver = {
	.probe = mtk_jpeg_probe,
	.remove = mtk_jpeg_remove,
	.driver = {
		.owner          = THIS_MODULE,
		.name           = MTK_JPEG_NAME,
		.of_match_table = mtk_jpeg_match,
		.pm             = &mtk_jpeg_pm_ops,
	},
};

module_platform_driver(mtk_jpeg_driver);

MODULE_DESCRIPTION("MediaTek JPEG codec driver");
MODULE_LICENSE("GPL v2");
