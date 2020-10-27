// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ming Hsiu Tsai <minghsiu.tsai@mediatek.com>
 *         Rick Chang <rick.chang@mediatek.com>
 *         Xia Jiang <xia.jiang@mediatek.com>
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
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include <soc/mediatek/smi.h>

#include "mtk_jpeg_enc_hw.h"
#include "mtk_jpeg_dec_hw.h"
#include "mtk_jpeg_core.h"
#include "mtk_jpeg_dec_parse.h"

//#include <mach/mt_iommu.h>
//#include "mach/pseudo_m4u.h"
//#include "smi_port.h"
//#include "ion.h"
//#include "ion_drv.h"




static struct mtk_jpeg_fmt mtk_jpeg_enc_formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_JPEG,
		.colplanes	= 1,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_CAPTURE,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV12M,
		.hw_format	= JPEG_ENC_YUV_FORMAT_NV12,
		.h_sample	= {4, 4},
		.v_sample	= {4, 2},
		.colplanes	= 2,
		.h_align	= 4,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV21M,
		.hw_format	= JEPG_ENC_YUV_FORMAT_NV21,
		.h_sample	= {4, 4},
		.v_sample	= {4, 2},
		.colplanes	= 2,
		.h_align	= 4,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUYV,
		.hw_format	= JPEG_ENC_YUV_FORMAT_YUYV,
		.h_sample	= {8},
		.v_sample	= {4},
		.colplanes	= 1,
		.h_align	= 5,
		.v_align	= 3,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YVYU,
		.hw_format	= JPEG_ENC_YUV_FORMAT_YVYU,
		.h_sample	= {8},
		.v_sample	= {4},
		.colplanes	= 1,
		.h_align	= 5,
		.v_align	= 3,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
};

static struct mtk_jpeg_fmt mtk_jpeg_dec_formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_JPEG,
		.colplanes	= 1,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUV420M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 3,
		.h_align	= 5,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_CAPTURE,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUV422M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 4, 4},
		.colplanes	= 3,
		.h_align	= 5,
		.v_align	= 3,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_CAPTURE,
	},
};

#define MTK_JPEG_ENC_NUM_FORMATS ARRAY_SIZE(mtk_jpeg_enc_formats)
#define MTK_JPEG_DEC_NUM_FORMATS ARRAY_SIZE(mtk_jpeg_dec_formats)

enum {
	MTK_JPEG_BUF_FLAGS_INIT			= 0,
	MTK_JPEG_BUF_FLAGS_LAST_FRAME		= 1,
};

struct mtk_jpeg_src_buf {
	struct vb2_v4l2_buffer b;
	struct list_head list;
	int flags;
	struct mtk_jpeg_dec_param dec_param;
};

static int debug;
module_param(debug, int, 0644);

static inline struct mtk_jpeg_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_jpeg_ctx, ctrl_hdl);
}

static inline struct mtk_jpeg_ctx *mtk_jpeg_fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_jpeg_ctx, fh);
}

static inline struct mtk_jpeg_src_buf *mtk_jpeg_vb2_to_srcbuf(
							struct vb2_buffer *vb)
{
	return container_of(to_vb2_v4l2_buffer(vb), struct mtk_jpeg_src_buf, b);
}

static int mtk_jpeg_enc_querycap(struct file *file, void *priv,
				 struct v4l2_capability *cap)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);

	strscpy(cap->driver, jpeg->vdev->name, sizeof(cap->driver));
	strscpy(cap->card, MTK_JPEG_NAME " encoder", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(jpeg->dev));

	return 0;
}

static int mtk_jpeg_dec_querycap(struct file *file, void *priv,
				 struct v4l2_capability *cap)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);

	strscpy(cap->driver, MTK_JPEG_NAME, sizeof(cap->driver));
	strscpy(cap->card, MTK_JPEG_NAME " decoder", sizeof(cap->card));
	snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(jpeg->dev));

	return 0;
}

static int vidioc_jpeg_enc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_jpeg_ctx *ctx = ctrl_to_ctx(ctrl);

	pr_info("%s config id 0x%x", __func__, ctrl->id);

	switch (ctrl->id) {
	case V4L2_CID_JPEG_RESTART_INTERVAL:
		ctx->restart_interval = ctrl->val;
		break;
	case V4L2_CID_JPEG_COMPRESSION_QUALITY:
		ctx->enc_quality = ctrl->val;
		break;
	case V4L2_CID_JPEG_ACTIVE_MARKER:
		ctx->enable_exif = ctrl->val & V4L2_JPEG_ACTIVE_MARKER_APP1 ?
				   true : false;
		pr_info("%s %d  enable_exif %d\n", __func__, __LINE__, ctx->enable_exif);
		break;
	}

	return 0;
}

static const struct v4l2_ctrl_ops mtk_jpeg_enc_ctrl_ops = {
	.s_ctrl = vidioc_jpeg_enc_s_ctrl,
};

static int mtk_jpeg_enc_ctrls_setup(struct mtk_jpeg_ctx *ctx)
{
	const struct v4l2_ctrl_ops *ops = &mtk_jpeg_enc_ctrl_ops;
	struct v4l2_ctrl_handler *handler = &ctx->ctrl_hdl;

	v4l2_ctrl_handler_init(handler, 20);

	if (handler->error)
		pr_info("ctrls setup fail 1");


	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_RESTART_INTERVAL, 0, 100,
			  1, 0);

	if (handler->error)
		pr_info("ctrls setup fail 2");


	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_COMPRESSION_QUALITY, 48,
			  100, 1, 90);

	if (handler->error)
		pr_info("ctrls setup fail 3");


	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_ACTIVE_MARKER, 0,
			  V4L2_JPEG_ACTIVE_MARKER_APP1, 0, 0);

	if (handler->error)
		pr_info("ctrls setup fail 4");


	if (handler->error) {
		v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
		return handler->error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);

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

	f->pixelformat = mtk_jpeg_formats[i].fourcc;

	return 0;
}

static int mtk_jpeg_enc_enum_fmt_vid_cap(struct file *file, void *priv,
					 struct v4l2_fmtdesc *f)
{
	return mtk_jpeg_enum_fmt(mtk_jpeg_enc_formats,
				 MTK_JPEG_ENC_NUM_FORMATS, f,
				 MTK_JPEG_FMT_FLAG_ENC_CAPTURE);
}

static int mtk_jpeg_dec_enum_fmt_vid_cap(struct file *file, void *priv,
					 struct v4l2_fmtdesc *f)
{
	return mtk_jpeg_enum_fmt(mtk_jpeg_dec_formats,
				 MTK_JPEG_DEC_NUM_FORMATS, f,
				 MTK_JPEG_FMT_FLAG_DEC_CAPTURE);
}

static int mtk_jpeg_enc_enum_fmt_vid_out(struct file *file, void *priv,
					 struct v4l2_fmtdesc *f)
{
	return mtk_jpeg_enum_fmt(mtk_jpeg_enc_formats,
				 MTK_JPEG_ENC_NUM_FORMATS, f,
				 MTK_JPEG_FMT_FLAG_ENC_OUTPUT);
}

static int mtk_jpeg_dec_enum_fmt_vid_out(struct file *file, void *priv,
					 struct v4l2_fmtdesc *f)
{
	return mtk_jpeg_enum_fmt(mtk_jpeg_dec_formats, MTK_JPEG_DEC_NUM_FORMATS,
				 f, MTK_JPEG_FMT_FLAG_DEC_OUTPUT);
}

static struct mtk_jpeg_q_data *
mtk_jpeg_get_q_data(struct mtk_jpeg_ctx *ctx, enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->out_q;
	return &ctx->cap_q;
}

static struct mtk_jpeg_fmt *mtk_jpeg_find_format(u32 pixelformat,
						 unsigned int fmt_type)
{
	unsigned int k;
	struct mtk_jpeg_fmt *fmt;

	for (k = 0; k < MTK_JPEG_ENC_NUM_FORMATS; k++) {
		fmt = &mtk_jpeg_enc_formats[k];

		if (fmt->fourcc == pixelformat && fmt->flags & fmt_type)
			return fmt;
	}

	for (k = 0; k < MTK_JPEG_DEC_NUM_FORMATS; k++) {
		fmt = &mtk_jpeg_dec_formats[k];

		if (fmt->fourcc == pixelformat && fmt->flags & fmt_type)
			return fmt;
	}

	return NULL;
}

static int vidioc_try_fmt(struct v4l2_format *f, struct mtk_jpeg_fmt *fmt)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	int i;

	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->num_planes = fmt->colplanes;
	pix_mp->pixelformat = fmt->fourcc;

	if (fmt->fourcc == V4L2_PIX_FMT_JPEG) {
		pix_mp->height = clamp(pix_mp->height, MTK_JPEG_MIN_HEIGHT,
				       MTK_JPEG_MAX_HEIGHT);
		pix_mp->width = clamp(pix_mp->width, MTK_JPEG_MIN_WIDTH,
				      MTK_JPEG_MAX_WIDTH);
		pix_mp->plane_fmt[0].bytesperline = 0;
		pix_mp->plane_fmt[0].sizeimage =
				round_up(pix_mp->plane_fmt[0].sizeimage, 128);
		if (pix_mp->plane_fmt[0].sizeimage == 0)
			pix_mp->plane_fmt[0].sizeimage =
				MTK_JPEG_DEFAULT_SIZEIMAGE;
	} else {
		pix_mp->height = clamp(round_up(pix_mp->height, fmt->v_align),
				       MTK_JPEG_MIN_HEIGHT,
				       MTK_JPEG_MAX_HEIGHT);
		pix_mp->width = clamp(round_up(pix_mp->width, fmt->h_align),
				      MTK_JPEG_MIN_WIDTH, MTK_JPEG_MAX_WIDTH);

		pr_info("align_w h %d %d\n",
					 pix_mp->width,
					 pix_mp->height);

		for (i = 0; i < pix_mp->num_planes; i++) {
			struct v4l2_plane_pix_format *pfmt =
							&pix_mp->plane_fmt[i];
			u32 stride = pix_mp->width * fmt->h_sample[i] / 4;
			u32 h = pix_mp->height * fmt->v_sample[i] / 4;

			pfmt->bytesperline = stride;
			pfmt->sizeimage = stride * h;

			pr_info("plane %d bytesperline %d sizeimage %d\n",
					 i,
					 pfmt->bytesperline,
					 pfmt->sizeimage);
		}
	}

	return 0;
}

static int mtk_jpeg_g_fmt_vid_mplane(struct file *file, void *priv,
				     struct v4l2_format *f)
{
	struct vb2_queue *vq;
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int i;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);

	memset(pix_mp->reserved, 0, sizeof(pix_mp->reserved));
	pix_mp->width = q_data->w;
	pix_mp->height = q_data->h;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->pixelformat = q_data->fmt->fourcc;
	pix_mp->num_planes = q_data->fmt->colplanes;
	pix_mp->colorspace = ctx->colorspace;
	pix_mp->ycbcr_enc = ctx->ycbcr_enc;
	pix_mp->xfer_func = ctx->xfer_func;
	pix_mp->quantization = ctx->quantization;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) g_fmt:%c%c%c%c wxh:%ux%u\n",
		 f->type,
		 (pix_mp->pixelformat & 0xff),
		 (pix_mp->pixelformat >>  8 & 0xff),
		 (pix_mp->pixelformat >> 16 & 0xff),
		 (pix_mp->pixelformat >> 24 & 0xff),
		 pix_mp->width, pix_mp->height);

	for (i = 0; i < pix_mp->num_planes; i++) {
		struct v4l2_plane_pix_format *pfmt = &pix_mp->plane_fmt[i];

		pfmt->bytesperline = q_data->bytesperline[i];
		pfmt->sizeimage = q_data->sizeimage[i];
		memset(pfmt->reserved, 0, sizeof(pfmt->reserved));

		v4l2_dbg(1, debug, &jpeg->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i,
			 pfmt->bytesperline,
			 pfmt->sizeimage);
	}
	return 0;
}

static int mtk_jpeg_enc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
					       struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(f->fmt.pix_mp.pixelformat,
				   MTK_JPEG_FMT_FLAG_ENC_CAPTURE);
	if (!fmt)
		fmt = ctx->cap_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	return vidioc_try_fmt(f, fmt);
}

static int mtk_jpeg_dec_try_fmt_vid_cap_mplane(struct file *file, void *priv,
					       struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(f->fmt.pix_mp.pixelformat,
				   MTK_JPEG_FMT_FLAG_DEC_CAPTURE);
	if (!fmt)
		fmt = ctx->cap_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	if (ctx->state != MTK_JPEG_INIT) {
		mtk_jpeg_g_fmt_vid_mplane(file, priv, f);
		return 0;
	}

	return vidioc_try_fmt(f, fmt);
}

static int mtk_jpeg_enc_try_fmt_vid_out_mplane(struct file *file, void *priv,
					       struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(f->fmt.pix_mp.pixelformat,
				   MTK_JPEG_FMT_FLAG_ENC_OUTPUT);
	if (!fmt)
		fmt = ctx->out_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	return vidioc_try_fmt(f, fmt);
}

static int mtk_jpeg_dec_try_fmt_vid_out_mplane(struct file *file, void *priv,
					       struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(f->fmt.pix_mp.pixelformat,
				   MTK_JPEG_FMT_FLAG_DEC_OUTPUT);
	if (!fmt)
		fmt = ctx->out_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	if (ctx->state != MTK_JPEG_INIT) {
		mtk_jpeg_g_fmt_vid_mplane(file, priv, f);
		return 0;
	}

	return vidioc_try_fmt(f, fmt);
}

static int mtk_jpeg_s_fmt_mplane(struct mtk_jpeg_ctx *ctx,
				 struct v4l2_format *f, unsigned int fmt_type)
{
	struct vb2_queue *vq;
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int i;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);

	if (vb2_is_busy(vq)) {
		v4l2_err(&jpeg->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	q_data->fmt = mtk_jpeg_find_format(pix_mp->pixelformat, fmt_type);
	q_data->w = pix_mp->width;
	q_data->h = pix_mp->height;
	ctx->colorspace = pix_mp->colorspace;
	ctx->ycbcr_enc = pix_mp->ycbcr_enc;
	ctx->xfer_func = pix_mp->xfer_func;
	ctx->quantization = pix_mp->quantization;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) s_fmt:%c%c%c%c wxh:%ux%u\n",
		 f->type,
		 (q_data->fmt->fourcc & 0xff),
		 (q_data->fmt->fourcc >>  8 & 0xff),
		 (q_data->fmt->fourcc >> 16 & 0xff),
		 (q_data->fmt->fourcc >> 24 & 0xff),
		 q_data->w, q_data->h);

	for (i = 0; i < q_data->fmt->colplanes; i++) {
		q_data->bytesperline[i] = pix_mp->plane_fmt[i].bytesperline;
		q_data->sizeimage[i] = pix_mp->plane_fmt[i].sizeimage;

		v4l2_dbg(1, debug, &jpeg->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i, q_data->bytesperline[i], q_data->sizeimage[i]);
	}

	return 0;
}

static int mtk_jpeg_enc_s_fmt_vid_out_mplane(struct file *file, void *priv,
					     struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_enc_try_fmt_vid_out_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f,
				     MTK_JPEG_FMT_FLAG_ENC_OUTPUT);
}

static int mtk_jpeg_dec_s_fmt_vid_out_mplane(struct file *file, void *priv,
					     struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_dec_try_fmt_vid_out_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f,
				     MTK_JPEG_FMT_FLAG_DEC_OUTPUT);
}

static int mtk_jpeg_enc_s_fmt_vid_cap_mplane(struct file *file, void *priv,
					     struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_enc_try_fmt_vid_cap_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f,
				     MTK_JPEG_FMT_FLAG_ENC_CAPTURE);
}

static int mtk_jpeg_dec_s_fmt_vid_cap_mplane(struct file *file, void *priv,
					     struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_dec_try_fmt_vid_cap_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f,
				     MTK_JPEG_FMT_FLAG_DEC_CAPTURE);
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
	}

	return v4l2_ctrl_subscribe_event(fh, sub);
}

static int mtk_jpeg_enc_g_selection(struct file *file, void *priv,
				    struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
	case V4L2_SEL_TGT_CROP_BOUNDS:
	case V4L2_SEL_TGT_CROP_DEFAULT:
		s->r.width = ctx->out_q.w;
		s->r.height = ctx->out_q.h;
		s->r.left = 0;
		s->r.top = 0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int mtk_jpeg_dec_g_selection(struct file *file, void *priv,
				    struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
	case V4L2_SEL_TGT_COMPOSE_DEFAULT:
		s->r.width = ctx->out_q.w;
		s->r.height = ctx->out_q.h;
		s->r.left = 0;
		s->r.top = 0;
		break;
	case V4L2_SEL_TGT_COMPOSE_BOUNDS:
	case V4L2_SEL_TGT_COMPOSE_PADDED:
		s->r.width = ctx->cap_q.w;
		s->r.height = ctx->cap_q.h;
		s->r.left = 0;
		s->r.top = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mtk_jpeg_enc_s_selection(struct file *file, void *priv,
				    struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_CROP:
		s->r.left = 0;
		s->r.top = 0;
		ctx->out_q.w = min(s->r.width, ctx->out_q.w);
		ctx->out_q.h = min(s->r.height, ctx->out_q.h);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mtk_jpeg_dec_s_selection(struct file *file, void *priv,
				    struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (s->target) {
	case V4L2_SEL_TGT_COMPOSE:
		s->r.left = 0;
		s->r.top = 0;
		s->r.width = ctx->out_q.w;
		s->r.height = ctx->out_q.h;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mtk_jpeg_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct v4l2_fh *fh = file->private_data;
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_jpeg_src_buf *jpeg_src_buf;

	if (buf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->dst_offset = buf->m.planes[0].data_offset;
		pr_info("%s %d data_offset %d\n", __func__, __LINE__, buf->m.planes[0].data_offset);
		goto end;
	}

	vq = v4l2_m2m_get_vq(fh->m2m_ctx, buf->type);
	if (buf->index >= vq->num_buffers) {
		dev_err(ctx->jpeg->dev, "buffer index out of range\n");
		return -EINVAL;
	}

	vb = vq->bufs[buf->index];
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(vb);
	jpeg_src_buf->flags = (buf->m.planes[0].bytesused == 0) ?
		MTK_JPEG_BUF_FLAGS_LAST_FRAME : MTK_JPEG_BUF_FLAGS_INIT;
end:
	return v4l2_m2m_qbuf(file, fh->m2m_ctx, buf);
}

static const struct v4l2_ioctl_ops mtk_jpeg_enc_ioctl_ops = {
	.vidioc_querycap                = mtk_jpeg_enc_querycap,
	.vidioc_enum_fmt_vid_cap	= mtk_jpeg_enc_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out	= mtk_jpeg_enc_enum_fmt_vid_out,
	.vidioc_try_fmt_vid_cap_mplane	= mtk_jpeg_enc_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mtk_jpeg_enc_try_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = mtk_jpeg_enc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane    = mtk_jpeg_enc_s_fmt_vid_out_mplane,
	.vidioc_qbuf                    = mtk_jpeg_qbuf,
	.vidioc_subscribe_event         = mtk_jpeg_subscribe_event,
	.vidioc_g_selection		= mtk_jpeg_enc_g_selection,
	.vidioc_s_selection		= mtk_jpeg_enc_s_selection,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs                 = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf                = v4l2_m2m_ioctl_querybuf,
	.vidioc_dqbuf                   = v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf                  = v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon                = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff               = v4l2_m2m_ioctl_streamoff,

	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static const struct v4l2_ioctl_ops mtk_jpeg_dec_ioctl_ops = {
	.vidioc_querycap                = mtk_jpeg_dec_querycap,
	.vidioc_enum_fmt_vid_cap	= mtk_jpeg_dec_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out	= mtk_jpeg_dec_enum_fmt_vid_out,
	.vidioc_try_fmt_vid_cap_mplane	= mtk_jpeg_dec_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mtk_jpeg_dec_try_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = mtk_jpeg_dec_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane    = mtk_jpeg_dec_s_fmt_vid_out_mplane,
	.vidioc_qbuf                    = mtk_jpeg_qbuf,
	.vidioc_subscribe_event         = mtk_jpeg_subscribe_event,
	.vidioc_g_selection		= mtk_jpeg_dec_g_selection,
	.vidioc_s_selection		= mtk_jpeg_dec_s_selection,

	.vidioc_create_bufs		= v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf		= v4l2_m2m_ioctl_prepare_buf,
	.vidioc_reqbufs                 = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf                = v4l2_m2m_ioctl_querybuf,
	.vidioc_dqbuf                   = v4l2_m2m_ioctl_dqbuf,
	.vidioc_expbuf                  = v4l2_m2m_ioctl_expbuf,
	.vidioc_streamon                = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff               = v4l2_m2m_ioctl_streamoff,

	.vidioc_unsubscribe_event	= v4l2_event_unsubscribe,
};

static int mtk_jpeg_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers,
				unsigned int *num_planes,
				unsigned int sizes[],
				struct device *alloc_ctxs[])
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct mtk_jpeg_q_data *q_data = NULL;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int i;

	v4l2_dbg(1, debug, &jpeg->v4l2_dev, "(%d) buf_req count=%u\n",
		 q->type, *num_buffers);

	q_data = mtk_jpeg_get_q_data(ctx, q->type);
	if (!q_data)
		return -EINVAL;

	if (*num_planes) {
		for (i = 0; i < *num_planes; i++)
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
		return 0;
	}

	*num_planes = q_data->fmt->colplanes;
	for (i = 0; i < q_data->fmt->colplanes; i++) {
		sizes[i] = q_data->sizeimage[i];
		v4l2_dbg(1, debug, &jpeg->v4l2_dev, "sizeimage[%d]=%u\n",
			 i, sizes[i]);
	}

	return 0;
}

static int mtk_jpeg_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_q_data *q_data = NULL;
	int i;

	q_data = mtk_jpeg_get_q_data(ctx, vb->vb2_queue->type);
	if (!q_data)
		return -EINVAL;

	for (i = 0; i < q_data->fmt->colplanes; i++)
		vb2_set_plane_payload(vb, i, q_data->sizeimage[i]);

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
	if (q_data->fmt !=
	    mtk_jpeg_find_format(param->dst_fourcc,
				 MTK_JPEG_FMT_FLAG_DEC_CAPTURE)) {
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
	int i;

	q_data = &ctx->out_q;
	q_data->w = param->pic_w;
	q_data->h = param->pic_h;

	q_data = &ctx->cap_q;
	q_data->w = param->dec_w;
	q_data->h = param->dec_h;
	q_data->fmt = mtk_jpeg_find_format(param->dst_fourcc,
					   MTK_JPEG_FMT_FLAG_DEC_CAPTURE);

	for (i = 0; i < q_data->fmt->colplanes; i++) {
		q_data->bytesperline[i] = param->mem_stride[i];
		q_data->sizeimage[i] = param->comp_size[i];
	}

	v4l2_dbg(1, debug, &jpeg->v4l2_dev,
		 "set_parse cap:%c%c%c%c pic(%u, %u), buf(%u, %u)\n",
		 (param->dst_fourcc & 0xff),
		 (param->dst_fourcc >>  8 & 0xff),
		 (param->dst_fourcc >> 16 & 0xff),
		 (param->dst_fourcc >> 24 & 0xff),
		 param->pic_w, param->pic_h,
		 param->dec_w, param->dec_h);
}

static void mtk_jpeg_enc_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	v4l2_dbg(2, debug, &jpeg->v4l2_dev, "(%d) buf_q id=%d, vb=%p\n",
		 vb->vb2_queue->type, vb->index, vb);

	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static void mtk_jpeg_dec_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_dec_param *param;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	bool header_valid;

	v4l2_dbg(2, debug, &jpeg->v4l2_dev, "(%d) buf_q id=%d, vb=%p\n",
		 vb->vb2_queue->type, vb->index, vb);

	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		goto end;

	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(vb);
	param = &jpeg_src_buf->dec_param;
	memset(param, 0, sizeof(*param));

	if (jpeg_src_buf->flags & MTK_JPEG_BUF_FLAGS_LAST_FRAME) {
		v4l2_dbg(1, debug, &jpeg->v4l2_dev, "Got eos\n");
		goto end;
	}
	header_valid = mtk_jpeg_parse(param, (u8 *)vb2_plane_vaddr(vb, 0),
				      vb2_get_plane_payload(vb, 0));
	if (!header_valid) {
		v4l2_err(&jpeg->v4l2_dev, "Header invalid.\n");
		vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
		return;
	}

	if (ctx->state == MTK_JPEG_INIT) {
		struct vb2_queue *dst_vq = v4l2_m2m_get_vq(
			ctx->fh.m2m_ctx, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

		mtk_jpeg_queue_src_chg_event(ctx);
		mtk_jpeg_set_queue_data(ctx, param);
		ctx->state = vb2_is_streaming(dst_vq) ?
				MTK_JPEG_SOURCE_CHANGE : MTK_JPEG_RUNNING;
	}
end:
	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static struct vb2_v4l2_buffer *mtk_jpeg_buf_remove(struct mtk_jpeg_ctx *ctx,
				 enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	else
		return v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
}

static void mtk_jpeg_enc_stop_streaming(struct vb2_queue *q)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb;

	while ((vb = mtk_jpeg_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);

}

static void mtk_jpeg_dec_stop_streaming(struct vb2_queue *q)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_v4l2_buffer *vb;

	/*
	 * STREAMOFF is an acknowledgment for source change event.
	 * Before STREAMOFF, we still have to return the old resolution and
	 * subsampling. Update capture queue when the stream is off.
	 */
	if (ctx->state == MTK_JPEG_SOURCE_CHANGE &&
	    V4L2_TYPE_IS_CAPTURE(q->type)) {
		struct mtk_jpeg_src_buf *src_buf;

		vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
		src_buf = mtk_jpeg_vb2_to_srcbuf(&vb->vb2_buf);
		mtk_jpeg_set_queue_data(ctx, &src_buf->dec_param);
		ctx->state = MTK_JPEG_RUNNING;
	} else if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		ctx->state = MTK_JPEG_INIT;
	}

	while ((vb = mtk_jpeg_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(vb, VB2_BUF_STATE_ERROR);
}

static const struct vb2_ops mtk_jpeg_dec_qops = {
	.queue_setup        = mtk_jpeg_queue_setup,
	.buf_prepare        = mtk_jpeg_buf_prepare,
	.buf_queue          = mtk_jpeg_dec_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.stop_streaming     = mtk_jpeg_dec_stop_streaming,
};

static const struct vb2_ops mtk_jpeg_enc_qops = {
	.queue_setup        = mtk_jpeg_queue_setup,
	.buf_prepare        = mtk_jpeg_buf_prepare,
	.buf_queue          = mtk_jpeg_enc_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.stop_streaming     = mtk_jpeg_enc_stop_streaming,
};

static void mtk_jpeg_set_dec_src(struct mtk_jpeg_ctx *ctx,
				 struct vb2_buffer *src_buf,
				 struct mtk_jpeg_bs *bs)
{
	bs->str_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	bs->end_addr = bs->str_addr +
		       round_up(vb2_get_plane_payload(src_buf, 0), 16);
	bs->size = round_up(vb2_plane_size(src_buf, 0), 128);
}

static int mtk_jpeg_set_dec_dst(struct mtk_jpeg_ctx *ctx,
				struct mtk_jpeg_dec_param *param,
				struct vb2_buffer *dst_buf,
				struct mtk_jpeg_fb *fb)
{
	int i;

	if (param->comp_num != dst_buf->num_planes) {
		dev_err(ctx->jpeg->dev, "plane number mismatch (%u != %u)\n",
			param->comp_num, dst_buf->num_planes);
		return -EINVAL;
	}

	for (i = 0; i < dst_buf->num_planes; i++) {
		if (vb2_plane_size(dst_buf, i) < param->comp_size[i]) {
			dev_err(ctx->jpeg->dev,
				"buffer size is underflow (%lu < %u)\n",
				vb2_plane_size(dst_buf, 0),
				param->comp_size[i]);
			return -EINVAL;
		}
		fb->plane_addr[i] = vb2_dma_contig_plane_dma_addr(dst_buf, i);
	}

	return 0;
}

static void mtk_jpeg_set_enc_dst(struct mtk_jpeg_ctx *ctx, void __iomem *base,
				 struct vb2_buffer *dst_buf,
				 struct mtk_jpeg_enc_bs *bs)
{
	bs->dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	bs->dma_addr += ctx->dst_offset;
	bs->dma_addr_offset = 0;
	bs->dma_addr_offsetmask = bs->dma_addr & JPEG_ENC_DST_ADDR_OFFSET_MASK;
	bs->size = vb2_plane_size(dst_buf, 0);

	mtk_jpeg_enc_set_dst_addr(base, bs->dma_addr, bs->size,
				  bs->dma_addr_offset,
				  bs->dma_addr_offsetmask);
}

static void mtk_jpeg_set_enc_src(struct mtk_jpeg_ctx *ctx, void __iomem *base,
				 struct vb2_buffer *src_buf)
{
	int i;
	dma_addr_t	dma_addr;

	mtk_jpeg_enc_set_img_size(base, ctx->out_q.w, ctx->out_q.h);
	mtk_jpeg_enc_set_blk_num(base, ctx->out_q.fmt->fourcc, ctx->out_q.w,
				 ctx->out_q.h);
	mtk_jpeg_enc_set_stride(base, ctx->out_q.fmt->fourcc, ctx->out_q.w,
				ctx->out_q.h, ctx->out_q.bytesperline[0]);

	for (i = 0; i < src_buf->num_planes; i++) {
		dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, i) +
			   src_buf->planes[i].data_offset;
		mtk_jpeg_enc_set_src_addr(base, dma_addr, i);
	}
}

static void mtk_jpeg_enc_device_run(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	unsigned long flags;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	struct mtk_jpeg_enc_bs enc_bs;
	int i, ret;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(&src_buf->vb2_buf);

	if (jpeg_src_buf->flags & MTK_JPEG_BUF_FLAGS_LAST_FRAME) {
		for (i = 0; i < dst_buf->vb2_buf.num_planes; i++)
			vb2_set_plane_payload(&dst_buf->vb2_buf, i, 0);
		buf_state = VB2_BUF_STATE_DONE;
		goto enc_end;
	}

	ret = pm_runtime_get_sync(jpeg->dev);
	if (ret < 0)
		goto enc_end;

	spin_lock_irqsave(&jpeg->hw_lock, flags);
	mtk_jpeg_enc_reset(jpeg->reg_base);

	mtk_jpeg_set_enc_dst(ctx, jpeg->reg_base, &dst_buf->vb2_buf, &enc_bs);
	mtk_jpeg_set_enc_src(ctx, jpeg->reg_base, &src_buf->vb2_buf);
	mtk_jpeg_enc_set_config(jpeg->reg_base, ctx->out_q.fmt->hw_format,
				ctx->enable_exif, ctx->enc_quality,
				ctx->restart_interval);
	mtk_jpeg_enc_start(jpeg->reg_base);
	ctx->state = MTK_JPEG_RUNNING;
	spin_unlock_irqrestore(&jpeg->hw_lock, flags);
	return;

enc_end:
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
}

static void mtk_jpeg_dec_device_run(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	unsigned long flags;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	struct mtk_jpeg_bs bs;
	struct mtk_jpeg_fb fb;
	int i, ret;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(&src_buf->vb2_buf);

	if (jpeg_src_buf->flags & MTK_JPEG_BUF_FLAGS_LAST_FRAME) {
		for (i = 0; i < dst_buf->vb2_buf.num_planes; i++)
			vb2_set_plane_payload(&dst_buf->vb2_buf, i, 0);
		buf_state = VB2_BUF_STATE_DONE;
		goto dec_end;
	}

	if (mtk_jpeg_check_resolution_change(ctx, &jpeg_src_buf->dec_param)) {
		mtk_jpeg_queue_src_chg_event(ctx);
		ctx->state = MTK_JPEG_SOURCE_CHANGE;
		v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
		return;
	}

	ret = pm_runtime_get_sync(jpeg->dev);
	if (ret < 0)
		goto dec_end;

	mtk_jpeg_set_dec_src(ctx, &src_buf->vb2_buf, &bs);
	if (mtk_jpeg_set_dec_dst(ctx, &jpeg_src_buf->dec_param,
				 &dst_buf->vb2_buf, &fb))
		goto dec_end;

	spin_lock_irqsave(&jpeg->hw_lock, flags);
	mtk_jpeg_dec_reset(jpeg->reg_base);
	mtk_jpeg_dec_set_config(jpeg->reg_base,
				&jpeg_src_buf->dec_param, &bs, &fb);

	mtk_jpeg_dec_start(jpeg->reg_base);
	spin_unlock_irqrestore(&jpeg->hw_lock, flags);
	return;

dec_end:
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
}

static int mtk_jpeg_enc_job_ready(void *priv)
{
		return 1;
}

static int mtk_jpeg_dec_job_ready(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;

	return (ctx->state == MTK_JPEG_RUNNING) ? 1 : 0;
}

static const struct v4l2_m2m_ops mtk_jpeg_enc_m2m_ops = {
	.device_run = mtk_jpeg_enc_device_run,
	.job_ready  = mtk_jpeg_enc_job_ready,
};

static const struct v4l2_m2m_ops mtk_jpeg_dec_m2m_ops = {
	.device_run = mtk_jpeg_dec_device_run,
	.job_ready  = mtk_jpeg_dec_job_ready,
};

static int mtk_jpeg_dec_queue_init(void *priv, struct vb2_queue *src_vq,
				   struct vb2_queue *dst_vq)
{
	struct mtk_jpeg_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_jpeg_src_buf);
	src_vq->ops = &mtk_jpeg_dec_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->jpeg->lock;
	src_vq->dev = ctx->jpeg->dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mtk_jpeg_dec_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->jpeg->lock;
	dst_vq->dev = ctx->jpeg->dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static int mtk_jpeg_enc_queue_init(void *priv, struct vb2_queue *src_vq,
				   struct vb2_queue *dst_vq)
{
	struct mtk_jpeg_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_jpeg_src_buf);
	src_vq->ops = &mtk_jpeg_enc_qops;
	src_vq->mem_ops = &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->jpeg->lock;
	src_vq->dev = ctx->jpeg->dev;
	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	dst_vq->drv_priv = ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops = &mtk_jpeg_enc_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->jpeg->lock;
	dst_vq->dev = ctx->jpeg->dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static void mtk_jpeg_clk_on(struct mtk_jpeg_dev *jpeg)
{
	int ret, i;
	//struct M4U_PORT_STRUCT port;

	ret = mtk_smi_larb_get(jpeg->larb);
	if (ret)
		dev_err(jpeg->dev, "mtk_smi_larb_get larbvdec fail %d\n", ret);

	for (i = 0; i < jpeg->variant->num_clocks; i++) {
		ret = clk_prepare_enable(jpeg->clocks[i]);
		if (ret) {
			while (--i >= 0)
				clk_disable_unprepare(jpeg->clocks[i]);
		}
	}

	enable_irq(jpeg->irq);
}

static void mtk_jpeg_clk_off(struct mtk_jpeg_dev *jpeg)
{
	int i;

	disable_irq(jpeg->irq);
	for (i = jpeg->variant->num_clocks - 1; i >= 0; i--)
		clk_disable_unprepare(jpeg->clocks[i]);
	mtk_smi_larb_put(jpeg->larb);
}

static irqreturn_t mtk_jpeg_enc_irq(int irq, void *priv)
{
	struct mtk_jpeg_dev *jpeg = priv;
	struct mtk_jpeg_ctx *ctx;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	u32 enc_irq_ret;
	u32 enc_ret, result_size;

	spin_lock(&jpeg->hw_lock);

	ctx = v4l2_m2m_get_curr_priv(jpeg->m2m_dev);
	if (!ctx) {
		v4l2_err(&jpeg->v4l2_dev, "Context is NULL\n");
		return IRQ_HANDLED;
	}

	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(&src_buf->vb2_buf);

	enc_ret = mtk_jpeg_enc_get_and_clear_int_status(jpeg->reg_base);
	enc_irq_ret = mtk_jpeg_enc_enum_result(jpeg->reg_base, enc_ret);

	if (enc_irq_ret >= MTK_JPEG_ENC_RESULT_STALL)
		mtk_jpeg_enc_reset(jpeg->reg_base);



	if (enc_irq_ret != MTK_JPEG_ENC_RESULT_DONE) {
		pr_info("encode failed\n");
		goto enc_end;
	}


	result_size = mtk_jpeg_enc_get_file_size(jpeg->reg_base);

	pr_info("%s %d reult_size %d", __func__, __LINE__, result_size);
	vb2_set_plane_payload(&dst_buf->vb2_buf, 0, result_size);

	buf_state = VB2_BUF_STATE_DONE;

enc_end:
	v4l2_m2m_buf_done(src_buf, buf_state);

	v4l2_m2m_buf_done(dst_buf, buf_state);


	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);

	spin_unlock(&jpeg->hw_lock);


	return IRQ_HANDLED;
}

static irqreturn_t mtk_jpeg_dec_irq(int irq, void *priv)
{
	struct mtk_jpeg_dev *jpeg = priv;
	struct mtk_jpeg_ctx *ctx;
	struct vb2_v4l2_buffer *src_buf, *dst_buf;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	u32 dec_irq_ret;
	u32 dec_ret;
	int i;

	spin_lock(&jpeg->hw_lock);

	dec_ret = mtk_jpeg_dec_get_int_status(jpeg->reg_base);
	dec_irq_ret = mtk_jpeg_dec_enum_result(dec_ret);
	ctx = v4l2_m2m_get_curr_priv(jpeg->m2m_dev);
	if (!ctx) {
		v4l2_err(&jpeg->v4l2_dev, "Context is NULL\n");
		return IRQ_HANDLED;
	}

	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(&src_buf->vb2_buf);

	if (dec_irq_ret >= MTK_JPEG_DEC_RESULT_UNDERFLOW)
		mtk_jpeg_dec_reset(jpeg->reg_base);

	if (dec_irq_ret != MTK_JPEG_DEC_RESULT_EOF_DONE) {
		dev_err(jpeg->dev, "decode failed\n");
		goto dec_end;
	}

	for (i = 0; i < dst_buf->vb2_buf.num_planes; i++)
		vb2_set_plane_payload(&dst_buf->vb2_buf, i,
				      jpeg_src_buf->dec_param.comp_size[i]);

	buf_state = VB2_BUF_STATE_DONE;

dec_end:
	v4l2_m2m_buf_done(src_buf, buf_state);
	v4l2_m2m_buf_done(dst_buf, buf_state);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
	spin_unlock(&jpeg->hw_lock);
	pm_runtime_put_sync(ctx->jpeg->dev);
	return IRQ_HANDLED;
}

static void mtk_jpeg_set_enc_default_params(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_q_data *q = &ctx->out_q;
	struct v4l2_pix_format_mplane *pix_mp;

	pix_mp = kmalloc(sizeof(*pix_mp), GFP_KERNEL);

	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	ctx->colorspace = V4L2_COLORSPACE_JPEG,
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	pix_mp->width = MTK_JPEG_MIN_WIDTH;
	pix_mp->height = MTK_JPEG_MIN_HEIGHT;

	q->fmt = mtk_jpeg_find_format(V4L2_PIX_FMT_YUYV,
				      MTK_JPEG_FMT_FLAG_ENC_OUTPUT);
	vidioc_try_fmt(container_of(pix_mp, struct v4l2_format,
				    fmt.pix_mp), q->fmt);
	q->w = pix_mp->width;
	q->h = pix_mp->height;
	q->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
	q->bytesperline[0] = pix_mp->plane_fmt[0].bytesperline;

	q = &ctx->cap_q;
	q->fmt = mtk_jpeg_find_format(V4L2_PIX_FMT_JPEG,
				      MTK_JPEG_FMT_FLAG_ENC_CAPTURE);
	pix_mp->width = MTK_JPEG_MIN_WIDTH;
	pix_mp->height = MTK_JPEG_MIN_HEIGHT;
	vidioc_try_fmt(container_of(pix_mp, struct v4l2_format,
				    fmt.pix_mp), q->fmt);
	q->w = pix_mp->width;
	q->h = pix_mp->height;
	q->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
	q->bytesperline[0] = pix_mp->plane_fmt[0].bytesperline;
}

static void mtk_jpeg_set_dec_default_params(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_q_data *q = &ctx->out_q;
	struct v4l2_pix_format_mplane *pix_mp;
	int i;

	pix_mp = kmalloc(sizeof(*pix_mp), GFP_KERNEL);

	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	ctx->colorspace = V4L2_COLORSPACE_JPEG,
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;
	pix_mp->width = MTK_JPEG_MIN_WIDTH;
	pix_mp->height = MTK_JPEG_MIN_HEIGHT;

	q->fmt = mtk_jpeg_find_format(V4L2_PIX_FMT_JPEG,
				      MTK_JPEG_FMT_FLAG_DEC_OUTPUT);
	vidioc_try_fmt(container_of(pix_mp, struct v4l2_format,
				    fmt.pix_mp), q->fmt);
	q->w = pix_mp->width;
	q->h = pix_mp->height;
	q->sizeimage[0] = pix_mp->plane_fmt[0].sizeimage;
	q->bytesperline[0] = pix_mp->plane_fmt[0].bytesperline;

	q = &ctx->cap_q;
	q->fmt = mtk_jpeg_find_format(V4L2_PIX_FMT_YUV420M,
				      MTK_JPEG_FMT_FLAG_DEC_CAPTURE);
	pix_mp->width = MTK_JPEG_MIN_WIDTH;
	pix_mp->height = MTK_JPEG_MIN_HEIGHT;
	vidioc_try_fmt(container_of(pix_mp, struct v4l2_format,
				    fmt.pix_mp), q->fmt);
	q->w = pix_mp->width;
	q->h = pix_mp->height;
	for (i = 0; i < q->fmt->colplanes; i++) {
		q->sizeimage[i] = pix_mp->plane_fmt[i].sizeimage;
		q->bytesperline[i] = pix_mp->plane_fmt[i].bytesperline;
	}
}

static int mtk_jpeg_enc_open(struct file *file)
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
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(jpeg->m2m_dev, ctx,
					    mtk_jpeg_enc_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}

	ret = mtk_jpeg_enc_ctrls_setup(ctx);
	if (ret) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to setup jpeg enc controls\n");
		goto error;
	}
	mtk_jpeg_set_enc_default_params(ctx);

	mutex_unlock(&jpeg->lock);

	pr_info("%s\n", __func__);
	return 0;

error:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	mutex_unlock(&jpeg->lock);
free:
	kfree(ctx);
	return ret;
}

static int mtk_jpeg_dec_open(struct file *file)
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
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(jpeg->m2m_dev, ctx,
					    mtk_jpeg_dec_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, 0);
	ret = v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);
	if (ret) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to setup jpeg dec controls\n");
		goto error;
	}
	mtk_jpeg_set_dec_default_params(ctx);

	mutex_unlock(&jpeg->lock);
	return 0;

error:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
	mutex_unlock(&jpeg->lock);
free:
	kfree(ctx);
	return ret;
}

static int mtk_jpeg_release(struct file *file)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(file->private_data);

	if (ctx->state == MTK_JPEG_RUNNING && jpeg->variant->is_encoder)
		pm_runtime_put_sync(ctx->jpeg->dev);
	mutex_lock(&jpeg->lock);
	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	kfree(ctx);
	mutex_unlock(&jpeg->lock);
	return 0;
}

static const struct v4l2_file_operations mtk_jpeg_enc_fops = {
	.owner          = THIS_MODULE,
	.open           = mtk_jpeg_enc_open,
	.release        = mtk_jpeg_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = v4l2_m2m_fop_mmap,
};

static const struct v4l2_file_operations mtk_jpeg_dec_fops = {
	.owner          = THIS_MODULE,
	.open           = mtk_jpeg_dec_open,
	.release        = mtk_jpeg_release,
	.poll           = v4l2_m2m_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap           = v4l2_m2m_fop_mmap,
};

static int mtk_jpeg_clk_init(struct mtk_jpeg_dev *jpeg)
{
	struct device_node *node;
	struct platform_device *pdev;
	int i;
	u32 id = 0;
	s32 ret = 0;

	node = of_parse_phandle(jpeg->dev->of_node, "mediatek,larb", 0);
	if (!node)
		return -EINVAL;
	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -EINVAL;
	}

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,larb-id", &id);
	if (ret)
		return -EINVAL;

	pr_info("jpeg_clk_init id %d\n", id);
	jpeg->larb_id = id;

	of_node_put(node);

	jpeg->larb = &pdev->dev;

	for (i = 0; i < jpeg->variant->num_clocks; i++) {
		jpeg->clocks[i] = devm_clk_get(jpeg->dev,
					       jpeg->variant->clk_names[i]);
		if (IS_ERR(jpeg->clocks[i])) {
			pr_info("failed to get clock: %s\n",
				jpeg->variant->clk_names[i]);
			return PTR_ERR(jpeg->clocks[i]);
		}
	}

	return 0;
}

static int mtk_jpeg_probe(struct platform_device *pdev)
{
	struct mtk_jpeg_dev *jpeg;
	struct resource *res;
	int ret;

	jpeg = devm_kzalloc(&pdev->dev, sizeof(*jpeg), GFP_KERNEL);
	if (!jpeg)
		return -ENOMEM;

	pr_info("%s 1\n", __func__);

	mutex_init(&jpeg->lock);
	spin_lock_init(&jpeg->hw_lock);
	jpeg->dev = &pdev->dev;
	jpeg->variant = of_device_get_match_data(jpeg->dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	jpeg->reg_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(jpeg->reg_base)) {
		ret = PTR_ERR(jpeg->reg_base);
		return ret;
	}

	pr_info("%s 2\n", __func__);

	jpeg->irq = platform_get_irq(pdev, 0);
	if (jpeg->irq < 0) {
		pr_info("Failed to get jpeg_irq %d.\n", jpeg->irq);
		return jpeg->irq;
	}

	pr_info("%s 3\n", __func__);
	if (jpeg->variant->is_encoder)
		ret = devm_request_irq(&pdev->dev, jpeg->irq, mtk_jpeg_enc_irq,
				       0, pdev->name, jpeg);
	else
		ret = devm_request_irq(&pdev->dev, jpeg->irq, mtk_jpeg_dec_irq,
				       0, pdev->name, jpeg);
	if (ret) {
		pr_info("Failed to request jpeg_irq %d (%d)\n",
			jpeg->irq, ret);
		goto err_req_irq;
	}

	pr_info("%s 4\n", __func__);
	disable_irq(jpeg->irq);

	ret = mtk_jpeg_clk_init(jpeg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to init clk, err %d\n", ret);
		goto err_clk_init;
	}

	pr_info("%s 5\n", __func__);

	ret = v4l2_device_register(&pdev->dev, &jpeg->v4l2_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register v4l2 device\n");
		ret = -EINVAL;
		goto err_dev_register;
	}

	if (jpeg->variant->is_encoder)
		jpeg->m2m_dev = v4l2_m2m_init(&mtk_jpeg_enc_m2m_ops);
	else
		jpeg->m2m_dev = v4l2_m2m_init(&mtk_jpeg_dec_m2m_ops);
	if (IS_ERR(jpeg->m2m_dev)) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to init mem2mem device\n");
		ret = PTR_ERR(jpeg->m2m_dev);
		goto err_m2m_init;
	}

	pr_info("%s 6\n", __func__);
	jpeg->vdev = video_device_alloc();
	if (!jpeg->vdev) {
		ret = -ENOMEM;
		goto err_vfd_jpeg_alloc;
	}
	snprintf(jpeg->vdev->name, sizeof(jpeg->vdev->name),
		 "%s-%s", MTK_JPEG_NAME,
		 jpeg->variant->is_encoder ? "enc" : "dec");
	if (jpeg->variant->is_encoder) {
		jpeg->vdev->fops = &mtk_jpeg_enc_fops;
		jpeg->vdev->ioctl_ops = &mtk_jpeg_enc_ioctl_ops;
	} else {
		jpeg->vdev->fops = &mtk_jpeg_dec_fops;
		jpeg->vdev->ioctl_ops = &mtk_jpeg_dec_ioctl_ops;
	}

	pr_info("%s 7\n", __func__);
	jpeg->vdev->minor = -1;
	jpeg->vdev->release = video_device_release;
	jpeg->vdev->lock = &jpeg->lock;
	jpeg->vdev->v4l2_dev = &jpeg->v4l2_dev;
	jpeg->vdev->vfl_dir = VFL_DIR_M2M;
	jpeg->vdev->device_caps = V4L2_CAP_STREAMING |
				      V4L2_CAP_VIDEO_M2M_MPLANE;

	ret = video_register_device(jpeg->vdev, VFL_TYPE_VIDEO, 3);
	if (ret) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to register video device\n");
		goto err_vfd_jpeg_register;
	}

	pr_info("%s 8\n", __func__);
	video_set_drvdata(jpeg->vdev, jpeg);
	v4l2_info(&jpeg->v4l2_dev,
		  "jpeg %s device registered as /dev/video%d (%d,%d)\n",
		  jpeg->variant->is_encoder ? "enc" : "dec", jpeg->vdev->num,
		  VIDEO_MAJOR, jpeg->vdev->minor);


	platform_set_drvdata(pdev, jpeg);

	pm_runtime_enable(&pdev->dev);
	pr_info("%s 9\n", __func__);

	/* Set DMA mask to 64 bits */
	ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		pr_info("mtk-jpeg unable to set coherent mask to 64");
		ret = dma_set_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
		if (ret)
			goto err_vfd_jpeg_register;
	}
	return 0;

err_vfd_jpeg_register:
	video_device_release(jpeg->vdev);

err_vfd_jpeg_alloc:
	v4l2_m2m_release(jpeg->m2m_dev);

err_m2m_init:
	v4l2_device_unregister(&jpeg->v4l2_dev);

err_dev_register:

err_clk_init:

err_req_irq:

	return ret;
}

static int mtk_jpeg_remove(struct platform_device *pdev)
{
	struct mtk_jpeg_dev *jpeg = platform_get_drvdata(pdev);

	pm_runtime_disable(&pdev->dev);
	video_unregister_device(jpeg->vdev);
	video_device_release(jpeg->vdev);
	v4l2_m2m_release(jpeg->m2m_dev);
	v4l2_device_unregister(&jpeg->v4l2_dev);

	return 0;
}

static __maybe_unused int mtk_jpeg_pm_suspend(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	mtk_jpeg_clk_off(jpeg);

	return 0;
}

static __maybe_unused int mtk_jpeg_pm_resume(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	mtk_jpeg_clk_on(jpeg);

	return 0;
}

static __maybe_unused int mtk_jpeg_suspend(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);
	unsigned long flags;
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	spin_lock_irqsave(&jpeg->hw_lock, flags);

	ret = mtk_jpeg_pm_suspend(dev);

	spin_unlock_irqrestore(&jpeg->hw_lock, flags);
	return ret;
}

static __maybe_unused int mtk_jpeg_resume(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_jpeg_pm_resume(dev);

	return ret;
}

static const struct dev_pm_ops mtk_jpeg_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mtk_jpeg_suspend, mtk_jpeg_resume)
	SET_RUNTIME_PM_OPS(mtk_jpeg_pm_suspend, mtk_jpeg_pm_resume, NULL)
};

static struct mtk_jpeg_variant mt8173_jpeg_drvdata = {
	.is_encoder	= false,
	.clk_names	= {"jpgdec-smi", "jpgdec"},
	.num_clocks	= 2,
};

static struct mtk_jpeg_variant mt2701_jpeg_drvdata = {
	.is_encoder	= false,
	.clk_names	= {"jpgdec-smi", "jpgdec"},
	.num_clocks	= 2,
};

static struct mtk_jpeg_variant mtk_jpeg_drvdata = {
	.is_encoder	= true,
	.clk_names	= {"jpgenc"},
	.num_clocks	= 1,
};

static const struct of_device_id mtk_jpeg_match[] = {
	{
		.compatible = "mediatek,mt8173-jpgdec",
		.data = &mt8173_jpeg_drvdata,
	},
	{
		.compatible = "mediatek,mt2701-jpgdec",
		.data = &mt2701_jpeg_drvdata,
	},
	{
		.compatible = "mediatek,mtk-jpgenc",
		.data = &mtk_jpeg_drvdata,
	},
	{},
};

MODULE_DEVICE_TABLE(of, mtk_jpeg_match);

static struct platform_driver mtk_jpeg_driver = {
	.probe = mtk_jpeg_probe,
	.remove = mtk_jpeg_remove,
	.driver = {
		.name           = MTK_JPEG_NAME,
		.of_match_table = mtk_jpeg_match,
		.pm             = &mtk_jpeg_pm_ops,
	},
};


module_platform_driver(mtk_jpeg_driver);
//module_init(jpeg_init);
//module_exit(jpeg_exit);
MODULE_DESCRIPTION("MediaTek JPEG codec driver");
MODULE_LICENSE("GPL v2");
