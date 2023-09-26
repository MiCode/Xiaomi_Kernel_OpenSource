// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
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
#include "smi_public.h"

#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#include "smi_port.h"
#include "mmdvfs_pmqos.h"
#include "ion.h"
#include "ion_drv.h"
#include <linux/pm_qos.h>





static struct mtk_jpeg_fmt mtk_jpeg_formats[] = {
	{
		.fourcc		= V4L2_PIX_FMT_JPEG,
		.colplanes	= 1,
		.flags		= MTK_JPEG_FMT_FLAG_DEC_OUTPUT |
					MTK_JPEG_FMT_FLAG_ENC_CAPTURE,
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
	{
		.fourcc		= V4L2_PIX_FMT_NV12M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 2,
		.h_align	= 4,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV21M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 2,
		.h_align	= 4,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV12M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 1,
		.h_align	= 4,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_NV21M,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 2, 2},
		.colplanes	= 1,
		.h_align	= 4,
		.v_align	= 4,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YUYV,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 4, 4},
		.colplanes	= 1,
		.h_align	= 4,
		.v_align	= 3,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
	{
		.fourcc		= V4L2_PIX_FMT_YVYU,
		.h_sample	= {4, 2, 2},
		.v_sample	= {4, 4, 4},
		.colplanes	= 1,
		.h_align	= 4,
		.v_align	= 3,
		.flags		= MTK_JPEG_FMT_FLAG_ENC_OUTPUT,
	},
};

#define MTK_JPEG_NUM_FORMATS ARRAY_SIZE(mtk_jpeg_formats)

enum {
	MTK_JPEG_BUF_FLAGS_INIT			= 0,
	MTK_JPEG_BUF_FLAGS_LAST_FRAME		= 1,
};

struct mtk_jpeg_src_buf {
	struct vb2_v4l2_buffer b;
	struct list_head list;
	unsigned int flags;
	struct mtk_jpeg_dec_param dec_param;
	struct mtk_jpeg_enc_param enc_param;
};

#define MTK_MAX_CTRLS_HINT	20
static int debug;
static struct ion_client *g_ion_client;

//pmqos
static unsigned int cshot_spec_dts;
static struct pm_qos_request jpeg_qos_request;
static u64 g_freq_steps[MAX_FREQ_STEP];  //index 0 is max
static u32 freq_step_size;


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

static int mtk_jpeg_querycap(struct file *file, void *priv,
			     struct v4l2_capability *cap)
{
	int ret = 0;
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);

	strscpy(cap->driver, jpeg->vfd_jpeg->name, sizeof(cap->driver));
	strscpy(cap->card, jpeg->vfd_jpeg->name, sizeof(cap->card));
	ret = snprintf(cap->bus_info, sizeof(cap->bus_info), "platform:%s",
		 dev_name(jpeg->dev));
	if (ret < 0) {
		pr_info("Failed to querycap (%d)\n", ret);
		return ret;
	}
	return 0;
}
static int vidioc_jpeg_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_jpeg_ctx *ctx = ctrl_to_ctx(ctrl);
	struct jpeg_enc_param *p = &ctx->jpeg_param;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_JPEG_RESTART_INTERVAL:
		v4l2_dbg(2, debug, &jpeg->v4l2_dev, "V4L2_CID_JPEG_RESTART_INTERVAL val = %d",
			       ctrl->val);
		p->restart_interval = ctrl->val;
		break;
	case V4L2_CID_JPEG_COMPRESSION_QUALITY:
		v4l2_dbg(2, debug, &jpeg->v4l2_dev, "V4L2_CID_JPEG_COMPRESSION_QUALITY val = %d",
			       ctrl->val);
		p->enc_quality = ctrl->val;
		break;
	case V4L2_CID_JPEG_ENABLE_EXIF:
		v4l2_dbg(2, debug, &jpeg->v4l2_dev, "V4L2_CID_JPEG_ENABLE_EXIF val = %d",
			       ctrl->val);
		p->enable_exif = ctrl->val;
		break;
	case V4L2_CID_JPEG_DST_OFFSET:
		v4l2_dbg(2, debug, &jpeg->v4l2_dev, "V4L2_CID_JPEG_DST_OFFSET val = %d",
			       ctrl->val);
		p->dst_offset = ctrl->val;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
static const struct v4l2_ctrl_ops mtk_jpeg_ctrl_ops = {
	.s_ctrl = vidioc_jpeg_s_ctrl,
};

void mtk_jpeg_prepare_dvfs(void)
{
	int ret;
	int i;

	if (!freq_step_size) {
		pm_qos_add_request(&jpeg_qos_request, PM_QOS_VENC_FREQ,
				 PM_QOS_DEFAULT_VALUE);
		ret = mmdvfs_qos_get_freq_steps(PM_QOS_VENC_FREQ, g_freq_steps,
					&freq_step_size);
		if (ret < 0)
			pr_info("Failed to get venc freq steps (%d)\n", ret);

		for (i = 0 ; i < freq_step_size ; i++)
			pr_info("freq %d  %lx", i, g_freq_steps[i]);
	}

}

void mtk_jpeg_unprepare_dvfs(void)
{
	pm_qos_update_request(&jpeg_qos_request,  0);
	pm_qos_remove_request(&jpeg_qos_request);
}

void mtk_jpeg_start_dvfs(void)
{
	if (g_freq_steps[0] != 0) {
		pr_info("highest freq 0x%x", g_freq_steps[0]);
		pm_qos_update_request(&jpeg_qos_request,  g_freq_steps[0]);
	}
}

void mtk_jpeg_end_dvfs(void)
{
	pm_qos_update_request(&jpeg_qos_request,  0);
}


void mtk_jpeg_prepare_bw_request(struct mtk_jpeg_dev *jpeg)
{
	int i = 0;

	plist_head_init(&jpeg->jpegenc_rlist);
	for (i = 0 ; i < jpeg->ncore; i++) {
		mm_qos_add_request(&jpeg->jpegenc_rlist,
			 &jpeg->jpeg_y_rdma, jpeg->port_y_rdma[i]);
		mm_qos_add_request(&jpeg->jpegenc_rlist,
			 &jpeg->jpeg_c_rdma, jpeg->port_c_rdma[i]);
		mm_qos_add_request(&jpeg->jpegenc_rlist,
			 &jpeg->jpeg_qtbl, jpeg->port_qtbl[i]);
		mm_qos_add_request(&jpeg->jpegenc_rlist,
			 &jpeg->jpeg_bsdma, jpeg->port_bsdma[i]);
	}
}

void mtk_jpeg_update_bw_request(struct mtk_jpeg_ctx *ctx,
		 struct mtk_jpeg_enc_param *config)
{
	/* No spec, considering [picture size] x [target fps] */
	unsigned int cshot_spec = 0xffffffff;
	/* limiting FPS, Upper Bound FPS = 20 */
	unsigned int target_fps = 30;

	/* Support QoS */
	unsigned int emi_bw = 0;
	unsigned int picSize = 0;
	unsigned int limitedFPS = 0;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	/* Support QoS */
	picSize = (config->enc_w * config->enc_h) / 1000000;
	/* BW = encode width x height x bpp x 1.6 */
	/* Assume compress ratio is 0.6 */
	#if 0
	if (cfgEnc.encFormat == 0x0 || cfgEnc.encFormat == 0x1)
		picCost = ((picSize * 2) * 8/5) + 1;
	else
		picCost = ((picSize * 3/2) * 8/5) + 1;
	#endif


	cshot_spec = cshot_spec_dts;

	if ((picSize * target_fps) < cshot_spec) {
		emi_bw = picSize * target_fps;
	} else {
		limitedFPS = cshot_spec / picSize;
		emi_bw = (limitedFPS + 1) * picSize;
	}

	/* QoS requires Occupied BW */
	/* Data BW x 1.33 */
	emi_bw = picSize * target_fps;

	emi_bw = emi_bw * 4/3;

	pr_info("Width %d Height %d emi_bw %d cshot_spec %d\n",
		 config->enc_w, config->enc_h, emi_bw, cshot_spec);



	if (config->enc_format == JPEG_YUV_FORMAT_YUYV ||
		config->enc_format == JPEG_YUV_FORMAT_YVYU) {
		mm_qos_set_request(&jpeg->jpeg_y_rdma, emi_bw * 2,
				 0, BW_COMP_NONE);
		mm_qos_set_request(&jpeg->jpeg_c_rdma, emi_bw,
				 0, BW_COMP_NONE);
	} else {
		mm_qos_set_request(&jpeg->jpeg_y_rdma, emi_bw,
				 0, BW_COMP_NONE);
		mm_qos_set_request(&jpeg->jpeg_c_rdma, emi_bw * 1/2,
				 0, BW_COMP_NONE);
	}

	mm_qos_set_request(&jpeg->jpeg_qtbl, emi_bw, 0, BW_COMP_NONE);
	mm_qos_set_request(&jpeg->jpeg_bsdma, emi_bw, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&jpeg->jpegenc_rlist);

}

void mtk_jpeg_end_bw_request(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	mm_qos_set_request(&jpeg->jpeg_y_rdma, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&jpeg->jpeg_c_rdma, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&jpeg->jpeg_qtbl, 0, 0, BW_COMP_NONE);
	mm_qos_set_request(&jpeg->jpeg_bsdma, 0, 0, BW_COMP_NONE);
	mm_qos_update_all_request(&jpeg->jpegenc_rlist);
}



static void mtk_jpeg_remove_bw_request(struct mtk_jpeg_dev *jpeg)
{
	mm_qos_remove_all_request(&jpeg->jpegenc_rlist);
}


int mtk_jpeg_ctrls_setup(struct mtk_jpeg_ctx *ctx)
{
	const struct v4l2_ctrl_ops *ops = &mtk_jpeg_ctrl_ops;
	struct v4l2_ctrl_handler *handler = &ctx->ctrl_hdl;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	v4l2_ctrl_handler_init(handler, MTK_MAX_CTRLS_HINT);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_RESTART_INTERVAL,
			0, 100, 1, 0);
	if (handler->error) {
		v4l2_err(&jpeg->v4l2_dev, "V4L2_CID_JPEG_RESTART_INTERVAL Init control handler fail %d\n",
		handler->error);
		return handler->error;
	}
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_COMPRESSION_QUALITY,
			0, 100, 1, 90);
	if (handler->error) {
		v4l2_err(&jpeg->v4l2_dev, "V4L2_CID_JPEG_COMPRESSION_QUALITY Init control handler fail %d\n",
		handler->error);
		return handler->error;
	}
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_ENABLE_EXIF,
			0, 1, 1, 0);
	if (handler->error) {
		v4l2_err(&jpeg->v4l2_dev, "V4L2_CID_JPEG_ACTIVE_MARKER Init control handler fail %d\n",
				handler->error);
		return handler->error;
	}
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_JPEG_DST_OFFSET,
			0, 0x0FFFFFF0, 1, 0);
	if (handler->error) {
		v4l2_err(&jpeg->v4l2_dev, "V4L2_CID_JPEG_DST_OFFSET Init control handler fail %d\n",
				handler->error);
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

static int mtk_jpeg_enum_fmt_vid_cap(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (ctx->jpeg->mode ==  MTK_JPEG_ENC) {
		return mtk_jpeg_enum_fmt(mtk_jpeg_formats, MTK_JPEG_NUM_FORMATS,
				f, MTK_JPEG_FMT_FLAG_ENC_CAPTURE);
	}
	return mtk_jpeg_enum_fmt(mtk_jpeg_formats, MTK_JPEG_NUM_FORMATS, f,
				 MTK_JPEG_FMT_FLAG_DEC_CAPTURE);
}

static int mtk_jpeg_enum_fmt_vid_out(struct file *file, void *priv,
				     struct v4l2_fmtdesc *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);

	if (ctx->jpeg->mode ==  MTK_JPEG_ENC) {
		return mtk_jpeg_enum_fmt(mtk_jpeg_formats, MTK_JPEG_NUM_FORMATS,
				f, MTK_JPEG_FMT_FLAG_ENC_OUTPUT);
	}
	return mtk_jpeg_enum_fmt(mtk_jpeg_formats, MTK_JPEG_NUM_FORMATS, f,
				 MTK_JPEG_FMT_FLAG_DEC_OUTPUT);
}

static struct mtk_jpeg_q_data *mtk_jpeg_get_q_data(struct mtk_jpeg_ctx *ctx,
						   enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->out_q;
	return &ctx->cap_q;
}

static struct mtk_jpeg_fmt *mtk_jpeg_find_format(struct mtk_jpeg_ctx *ctx,
						 u32 pixelformat,
						 u32 num_planes,
						 unsigned int fmt_type)
{
	unsigned int k, fmt_flag;

	if (ctx->jpeg->mode ==  MTK_JPEG_ENC) {
		fmt_flag = (fmt_type == MTK_JPEG_FMT_TYPE_OUTPUT) ?
				MTK_JPEG_FMT_FLAG_ENC_OUTPUT :
				MTK_JPEG_FMT_FLAG_ENC_CAPTURE;
	} else {
		fmt_flag = (fmt_type == MTK_JPEG_FMT_TYPE_OUTPUT) ?
		   MTK_JPEG_FMT_FLAG_DEC_OUTPUT :
		   MTK_JPEG_FMT_FLAG_DEC_CAPTURE;
	}

	for (k = 0; k < MTK_JPEG_NUM_FORMATS; k++) {
		struct mtk_jpeg_fmt *fmt = &mtk_jpeg_formats[k];

		if (fmt->fourcc == pixelformat &&
			fmt->flags & fmt_flag &&
			fmt->colplanes == num_planes)
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

static void mtk_jpeg_adjust_fmt_mplane(struct mtk_jpeg_ctx *ctx,
				       struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jpeg_q_data *q_data;
	int i;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);

	pix_mp->width = q_data->w;
	pix_mp->height = q_data->h;
	pix_mp->pixelformat = q_data->fmt->fourcc;
	pix_mp->num_planes = q_data->fmt->colplanes;

	for (i = 0; i < pix_mp->num_planes; i++) {
		pix_mp->plane_fmt[i].bytesperline = q_data->bytesperline[i];
		pix_mp->plane_fmt[i].sizeimage = q_data->sizeimage[i];
	}
}

static int mtk_jpeg_try_fmt_mplane(struct v4l2_format *f,
				   struct mtk_jpeg_fmt *fmt,
				   struct mtk_jpeg_ctx *ctx, int q_type)
{
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	unsigned int i, align_w, align_h;

	memset(pix_mp->reserved, 0, sizeof(pix_mp->reserved));
	pix_mp->field = V4L2_FIELD_NONE;

	if (ctx->state != MTK_JPEG_INIT) {
		mtk_jpeg_adjust_fmt_mplane(ctx, f);
		goto end;
	}

	pix_mp->num_planes = fmt->colplanes;
	pix_mp->pixelformat = fmt->fourcc;



	if (q_type == MTK_JPEG_FMT_TYPE_OUTPUT) {
		if (jpeg->mode == MTK_JPEG_ENC) {
			align_w = pix_mp->width;
			align_h = pix_mp->height;
			align_w = ((align_w + 1) >> 1) << 1;
			if (pix_mp->num_planes == 1U) {
				if (pix_mp->pixelformat == V4L2_PIX_FMT_YUYV ||
				    pix_mp->pixelformat == V4L2_PIX_FMT_YVYU) {
					align_w = align_w << 1;//jx fix bug
					mtk_jpeg_bound_align_image(&align_w,
						MTK_JPEG_MIN_WIDTH,
						MTK_JPEG_MAX_WIDTH,
						5,
						&align_h, MTK_JPEG_MIN_HEIGHT,
						MTK_JPEG_MAX_HEIGHT, 3);

					pix_mp->plane_fmt[0].bytesperline =
						align_w;
					pix_mp->plane_fmt[0].sizeimage =
						align_w * align_h;

					pr_info("bperline %d  imagesz %d align_w h %d %d\n",
					 pix_mp->plane_fmt[0].bytesperline,
					 pix_mp->plane_fmt[0].sizeimage,
					 align_w,
					 align_h);
				} else {
					mtk_jpeg_bound_align_image(&align_w,
					MTK_JPEG_MIN_WIDTH, MTK_JPEG_MAX_WIDTH,
					4, &align_h, MTK_JPEG_MIN_HEIGHT,
					MTK_JPEG_MAX_HEIGHT, 4);

					pix_mp->plane_fmt[0].bytesperline =
						align_w;
					pix_mp->plane_fmt[0].sizeimage =
					align_w * align_h +
					(align_w * align_h) / 2;


					pr_info("bperline NV21 %d imagesz %d align_w h %d %d\n",
					 pix_mp->plane_fmt[0].bytesperline,
					 pix_mp->plane_fmt[0].sizeimage,
					 align_w,
					 align_h);
				}

			} else if (pix_mp->num_planes == 2U) {
				mtk_jpeg_bound_align_image(&align_w,
					MTK_JPEG_MIN_WIDTH, MTK_JPEG_MAX_WIDTH,
					4, &align_h, MTK_JPEG_MIN_HEIGHT,
					MTK_JPEG_MAX_HEIGHT, 4);
				pix_mp->plane_fmt[0].bytesperline = align_w;
				pix_mp->plane_fmt[0].sizeimage =
					align_w * align_h;
				pix_mp->plane_fmt[1].bytesperline = align_w;
				pix_mp->plane_fmt[1].sizeimage =
					(align_w * align_h) / 2;

				pr_info("bperline %d imagesz %d align_w h %d %d\n",
					 pix_mp->plane_fmt[0].bytesperline,
					 pix_mp->plane_fmt[0].sizeimage,
					 align_w,
					 align_h);
				pr_info("bperline %d imagesz %d\n",
					 pix_mp->plane_fmt[1].bytesperline,
					 pix_mp->plane_fmt[1].sizeimage);

			} else {
				v4l2_err(&ctx->jpeg->v4l2_dev,
					"Unsupport num planes = %d\n",
					pix_mp->num_planes);
			}
			goto end;
		} else {
			struct v4l2_plane_pix_format *pfmt =
					&pix_mp->plane_fmt[0];

			mtk_jpeg_bound_align_image(&pix_mp->width,
					MTK_JPEG_MIN_WIDTH, MTK_JPEG_MAX_WIDTH,
					0, &pix_mp->height, MTK_JPEG_MIN_HEIGHT,
					   MTK_JPEG_MAX_HEIGHT, 0);

		memset(pfmt->reserved, 0, sizeof(pfmt->reserved));
		pfmt->bytesperline = 0;
		/* Source size must be aligned to 128 */
		pfmt->sizeimage = mtk_jpeg_align(pfmt->sizeimage, 128);
		if (pfmt->sizeimage == 0)
			pfmt->sizeimage = MTK_JPEG_DEFAULT_SIZEIMAGE;
		goto end;
		}
	}

	/* type is MTK_JPEG_FMT_TYPE_CAPTURE */
	if (jpeg->mode == MTK_JPEG_ENC) {
		mtk_jpeg_bound_align_image(&pix_mp->width, MTK_JPEG_MIN_WIDTH,
					   MTK_JPEG_MAX_WIDTH, 0,
					   &pix_mp->height, MTK_JPEG_MIN_HEIGHT,
					   MTK_JPEG_MAX_HEIGHT, 0);
		if (fmt->fourcc == V4L2_PIX_FMT_JPEG) {
			pix_mp->plane_fmt[0].sizeimage =
				mtk_jpeg_align(pix_mp->plane_fmt[0].sizeimage,
					       128);
			pix_mp->plane_fmt[0].bytesperline = 0;
		}
	} else {
		mtk_jpeg_bound_align_image(&pix_mp->width,
					MTK_JPEG_MIN_WIDTH,
				   MTK_JPEG_MAX_WIDTH, fmt->h_align,
				   &pix_mp->height, MTK_JPEG_MIN_HEIGHT,
				   MTK_JPEG_MAX_HEIGHT, fmt->v_align);

		for (i = 0; i < fmt->colplanes; i++) {
			struct v4l2_plane_pix_format *pfmt =
					&pix_mp->plane_fmt[i];
			u32 stride = pix_mp->width * fmt->h_sample[i] / 4;
			u32 h = pix_mp->height * fmt->v_sample[i] / 4;

			memset(pfmt->reserved, 0, sizeof(pfmt->reserved));
			pfmt->bytesperline = stride;
			pfmt->sizeimage = stride * h;
		}
	}
end:
	v4l2_dbg(2, debug, &jpeg->v4l2_dev, "wxh:%ux%u\n",
		 pix_mp->width, pix_mp->height);
	for (i = 0; i < pix_mp->num_planes; i++) {
		v4l2_dbg(2, debug, &jpeg->v4l2_dev,
			 "plane[%d] bpl=%u, size=%u\n",
			 i,
			 pix_mp->plane_fmt[i].bytesperline,
			 pix_mp->plane_fmt[i].sizeimage);
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

static int mtk_jpeg_try_fmt_vid_cap_mplane(struct file *file, void *priv,
					   struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(ctx, f->fmt.pix_mp.pixelformat,
					f->fmt.pix_mp.num_planes,
				   MTK_JPEG_FMT_TYPE_CAPTURE);
	if (!fmt)
		fmt = ctx->cap_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	return mtk_jpeg_try_fmt_mplane(f, fmt, ctx, MTK_JPEG_FMT_TYPE_CAPTURE);
}

static int mtk_jpeg_try_fmt_vid_out_mplane(struct file *file, void *priv,
					   struct v4l2_format *f)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_fmt *fmt;

	fmt = mtk_jpeg_find_format(ctx, f->fmt.pix_mp.pixelformat,
					 f->fmt.pix_mp.num_planes,
					 MTK_JPEG_FMT_TYPE_OUTPUT);
	if (!fmt)
		fmt = ctx->out_q.fmt;

	v4l2_dbg(2, debug, &ctx->jpeg->v4l2_dev, "(%d) try_fmt:%c%c%c%c\n",
		 f->type,
		 (fmt->fourcc & 0xff),
		 (fmt->fourcc >>  8 & 0xff),
		 (fmt->fourcc >> 16 & 0xff),
		 (fmt->fourcc >> 24 & 0xff));

	return mtk_jpeg_try_fmt_mplane(f, fmt, ctx, MTK_JPEG_FMT_TYPE_OUTPUT);
}

static int mtk_jpeg_s_fmt_mplane(struct mtk_jpeg_ctx *ctx,
				 struct v4l2_format *f)
{
	struct vb2_queue *vq;
	struct mtk_jpeg_q_data *q_data = NULL;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	unsigned int f_type;
	int i;

	vq = v4l2_m2m_get_vq(ctx->fh.m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_jpeg_get_q_data(ctx, f->type);

	if (vb2_is_busy(vq)) {
		v4l2_err(&jpeg->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	f_type = V4L2_TYPE_IS_OUTPUT(f->type) ?
			 MTK_JPEG_FMT_TYPE_OUTPUT : MTK_JPEG_FMT_TYPE_CAPTURE;

	q_data->fmt = mtk_jpeg_find_format(ctx, pix_mp->pixelformat,
		pix_mp->num_planes, f_type);
	q_data->w = pix_mp->width;
	q_data->h = pix_mp->height;
	q_data->align_h = pix_mp->height;

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

static int mtk_jpeg_s_fmt_vid_out_mplane(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_try_fmt_vid_out_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f);
}

static int mtk_jpeg_s_fmt_vid_cap_mplane(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	int ret;

	ret = mtk_jpeg_try_fmt_vid_cap_mplane(file, priv, f);
	if (ret)
		return ret;

	return mtk_jpeg_s_fmt_mplane(mtk_jpeg_fh_to_ctx(priv), f);
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
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	default:
		return -EINVAL;
	}
}

static int mtk_jpeg_g_selection(struct file *file, void *priv,
				struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	if (jpeg->mode == MTK_JPEG_ENC) {
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
	} else {
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
	}

	return 0;
}

static int mtk_jpeg_s_selection(struct file *file, void *priv,
				struct v4l2_selection *s)
{
	struct mtk_jpeg_ctx *ctx = mtk_jpeg_fh_to_ctx(priv);
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	if (jpeg->mode == MTK_JPEG_ENC) {
		if (s->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
			return -EINVAL;

		switch (s->target) {
		case V4L2_SEL_TGT_CROP:
			s->r.left = 0;
			s->r.top = 0;
			ctx->out_q.w = s->r.width;
			ctx->out_q.h = s->r.height;

			pr_info("%s crop width %d height %d",
				 __func__, ctx->out_q.w, ctx->out_q.h);
			break;
		default:
			return -EINVAL;
		}
	} else {
		if (s->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
			return -EINVAL;

		switch (s->target) {
		case V4L2_SEL_TGT_COMPOSE:
			s->r.left = 0;
			s->r.top = 0;
			ctx->out_q.w = s->r.width;
			ctx->out_q.h = s->r.height;
			break;
		default:
			return -EINVAL;
		}
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

	if (buf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		goto end;

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

static const struct v4l2_ioctl_ops mtk_jpeg_ioctl_ops = {
	.vidioc_querycap                = mtk_jpeg_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = mtk_jpeg_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_out_mplane = mtk_jpeg_enum_fmt_vid_out,
	.vidioc_try_fmt_vid_cap_mplane	= mtk_jpeg_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane	= mtk_jpeg_try_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_g_fmt_vid_out_mplane    = mtk_jpeg_g_fmt_vid_mplane,
	.vidioc_s_fmt_vid_cap_mplane    = mtk_jpeg_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane    = mtk_jpeg_s_fmt_vid_out_mplane,
	.vidioc_qbuf                    = mtk_jpeg_qbuf,
	.vidioc_subscribe_event         = mtk_jpeg_subscribe_event,
	.vidioc_g_selection		= mtk_jpeg_g_selection,
	.vidioc_s_selection		= mtk_jpeg_s_selection,

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
	if (q_data->fmt != mtk_jpeg_find_format(ctx, param->dst_fourcc, 3,
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
	int i;

	q_data = &ctx->out_q;
	q_data->w = param->pic_w;
	q_data->h = param->pic_h;

	q_data = &ctx->cap_q;
	q_data->w = param->dec_w;
	q_data->h = param->dec_h;
	q_data->fmt = mtk_jpeg_find_format(ctx,
					   param->dst_fourcc, 3,
					   MTK_JPEG_FMT_TYPE_CAPTURE);

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

static void mtk_jpeg_set_param(struct mtk_jpeg_ctx *ctx,
					struct mtk_jpeg_enc_param *param)
{
	struct mtk_jpeg_q_data *q_data_src = &ctx->out_q;
	struct jpeg_enc_param *jpeg_params = &ctx->jpeg_param;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	u32 width_even;
	u32 Is420;
	u32 padding_width;
	u32 padding_height;

	switch (q_data_src->fmt->fourcc) {
	case V4L2_PIX_FMT_YUYV:
		param->enc_format = JPEG_YUV_FORMAT_YUYV;
		break;
	case V4L2_PIX_FMT_YVYU:
		param->enc_format = JPEG_YUV_FORMAT_YVYU;
		break;
	case V4L2_PIX_FMT_NV12M:
		param->enc_format = JPEG_YUV_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21M:
		param->enc_format = JPEG_YUV_FORMAT_NV21;
		break;
	default:
		v4l2_err(&jpeg->v4l2_dev, "Unsupport fourcc =%d\n",
				q_data_src->fmt->fourcc);
		break;
	}
	param->enc_w = q_data_src->w;
	param->enc_h = q_data_src->h;
	param->align_h = q_data_src->align_h;

	pr_info("%s crop width %d height %d",
		 __func__, param->enc_w, param->enc_h);


	if (jpeg_params->enc_quality >= 97)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q97;
	else if (jpeg_params->enc_quality >= 95)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q95;
	else if (jpeg_params->enc_quality >= 90)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q90;
	else if (jpeg_params->enc_quality >= 85)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q85;
	else if (jpeg_params->enc_quality >= 78)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q78;
	else if (jpeg_params->enc_quality >= 72)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q72;
	else if (jpeg_params->enc_quality >= 60)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q60;
	else if (jpeg_params->enc_quality >= 52)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q52;
	else if (jpeg_params->enc_quality >= 44)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q44;
	else if (jpeg_params->enc_quality >= 38)
		param->enc_quality = JPEG_ENCODE_QUALITY_Q38;
	else
		param->enc_quality = JPEG_ENCODE_QUALITY_Q30;
	param->enable_exif = jpeg_params->enable_exif;
	param->restart_interval = jpeg_params->restart_interval;
	width_even = ((param->enc_w + 1) >> 1) << 1;
	Is420 = (param->enc_format == JPEG_YUV_FORMAT_NV12 ||
			param->enc_format == JPEG_YUV_FORMAT_NV21) ? 1:0;
	padding_width = mtk_jpeg_align(param->enc_w, 16);
	padding_height = mtk_jpeg_align(param->enc_h, Is420 ? 16 : 8);
	if (!Is420)
		width_even = width_even << 1;
	param->img_stride = mtk_jpeg_align(width_even, (Is420 ? 16 : 32));


	param->mem_stride = q_data_src->bytesperline[0];
	pr_info("%s mem_stride %d img_stride %d align_h %d",
		 __func__, param->mem_stride, param->img_stride,
		  param->align_h);

	param->total_encdu =
		((padding_width >> 4) * (padding_height >> (Is420 ? 4 : 3)) *
						 (Is420 ? 6 : 4)) - 1;

	mtk_jpeg_update_bw_request(ctx, param);

	v4l2_dbg(0, 2, &jpeg->v4l2_dev, "fmt %d, w,h %d,%d, enable_exif %d, enc_quality %d, restart_interval %d,img_stride %d, mem_stride %d, totalEncDu %d\n",
		param->enc_format, param->enc_w, param->enc_h,
		param->enable_exif, param->enc_quality, param->restart_interval,
		param->img_stride, param->mem_stride, param->total_encdu);

	pr_info("fmt %d, w,h %d,%d, enable_exif %d, enc_quality %d, restart_interval %d,img_stride %d, mem_stride %d, totalEncDu %d\n",
		param->enc_format, param->enc_w, param->enc_h,
		param->enable_exif, param->enc_quality, param->restart_interval,
		param->img_stride, param->mem_stride, param->total_encdu);

}
static void mtk_jpeg_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_jpeg_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_jpeg_dec_param *param;
	struct mtk_jpeg_enc_param *enc_param = NULL;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	bool header_valid;

	v4l2_dbg(2, debug, &jpeg->v4l2_dev, "(%d) buf_q id=%d, vb=%p\n",
		 vb->vb2_queue->type, vb->index, vb);

	if (vb->vb2_queue->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		goto end;

	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(vb);
	if (jpeg->mode ==  MTK_JPEG_ENC) {
		enc_param = &jpeg_src_buf->enc_param;
		memset(enc_param, 0, sizeof(*enc_param));
		mtk_jpeg_set_param(ctx, enc_param);
		if (jpeg_src_buf->flags & MTK_JPEG_BUF_FLAGS_LAST_FRAME) {
			v4l2_dbg(1, debug, &jpeg->v4l2_dev, "Got eos");
			goto end;
		}
		if (ctx->state == MTK_JPEG_INIT)
			ctx->state = MTK_JPEG_RUNNING;
	} else {
		param = &jpeg_src_buf->dec_param;
		memset(param, 0, sizeof(*param));

		if (jpeg_src_buf->flags & MTK_JPEG_BUF_FLAGS_LAST_FRAME) {
			v4l2_dbg(1, debug, &jpeg->v4l2_dev, "Got eos\n");
			goto end;
		}
			header_valid = mtk_jpeg_parse(param,
						(u8 *)vb2_plane_vaddr(vb, 0),
						vb2_get_plane_payload(vb, 0));
		if (!header_valid) {
			v4l2_err(&jpeg->v4l2_dev, "Header invalid.\n");
			vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
			return;
		}

		if (ctx->state == MTK_JPEG_INIT) {
			struct vb2_queue *dst_vq = v4l2_m2m_get_vq(
					ctx->fh.m2m_ctx,
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

			mtk_jpeg_queue_src_chg_event(ctx);
			mtk_jpeg_set_queue_data(ctx, param);
			ctx->state = vb2_is_streaming(dst_vq) ?
				MTK_JPEG_SOURCE_CHANGE : MTK_JPEG_RUNNING;
		}
	}
end:
	v4l2_m2m_buf_queue(ctx->fh.m2m_ctx, to_vb2_v4l2_buffer(vb));
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
	struct vb2_buffer *vb;
	int ret = 0;

	ret = pm_runtime_get_sync(ctx->jpeg->dev);
	if (ret < 0)
		goto err;

	return 0;
err:
	while ((vb = mtk_jpeg_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_QUEUED);
	return ret;
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
	    !V4L2_TYPE_IS_OUTPUT(q->type) &&
	    ctx->jpeg->mode == MTK_JPEG_DEC) {
		struct mtk_jpeg_src_buf *src_buf;

		vb = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
		src_buf = mtk_jpeg_vb2_to_srcbuf(vb);
		mtk_jpeg_set_queue_data(ctx, &src_buf->dec_param);
		ctx->state = MTK_JPEG_RUNNING;
	} else if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		ctx->state = MTK_JPEG_INIT;
	}

	while ((vb = mtk_jpeg_buf_remove(ctx, q->type)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(vb), VB2_BUF_STATE_ERROR);

	pm_runtime_put_sync(ctx->jpeg->dev);
}

static const struct vb2_ops mtk_jpeg_qops = {
	.queue_setup        = mtk_jpeg_queue_setup,
	.buf_prepare        = mtk_jpeg_buf_prepare,
	.buf_queue          = mtk_jpeg_buf_queue,
	.wait_prepare       = vb2_ops_wait_prepare,
	.wait_finish        = vb2_ops_wait_finish,
	.start_streaming    = mtk_jpeg_start_streaming,
	.stop_streaming     = mtk_jpeg_stop_streaming,
};

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

static void mtk_jpeg_set_enc_dst(struct mtk_jpeg_ctx *ctx,
				 struct vb2_buffer *dst_buf,
				 struct mtk_jpeg_enc_bs *bs)
{
	struct jpeg_enc_param *p = &ctx->jpeg_param;

	bs->dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0) &
				(~JPEG_ENC_DST_ADDR_OFFSET_MASK);

	bs->dma_addr += p->dst_offset;
	bs->dma_addr_offset = 0;
	bs->dma_addr_offsetmask = bs->dma_addr & JPEG_ENC_DST_ADDR_OFFSET_MASK;
	bs->size = mtk_jpeg_align(vb2_plane_size(dst_buf, 0), 128);
}

static int mtk_jpeg_set_enc_src(struct mtk_jpeg_ctx *ctx,
				struct vb2_buffer *src_buf,
				struct mtk_jpeg_enc_fb *fb)
{
	int i;

	for (i = 0; i < src_buf->num_planes; i++) {
		fb->fb_addr[i].dma_addr =
			vb2_dma_contig_plane_dma_addr(src_buf, i);
	}

	fb->num_planes = src_buf->num_planes;
	return 0;
}
static void mtk_jpeg_device_run(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	struct vb2_buffer *src_buf, *dst_buf;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	unsigned long flags;
	struct mtk_jpeg_src_buf *jpeg_src_buf;
	struct mtk_jpeg_bs bs;
	struct mtk_jpeg_fb fb;
	struct mtk_jpeg_enc_bs enc_bs;
	struct mtk_jpeg_enc_fb enc_fb;
	int i;

	src_buf = v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
	if (src_buf == NULL || dst_buf == NULL) {
		pr_info("null buffer pointer");
		return;
	}
	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(src_buf);

	if (jpeg_src_buf->flags & MTK_JPEG_BUF_FLAGS_LAST_FRAME) {
		for (i = 0; i < dst_buf->num_planes; i++)
			vb2_set_plane_payload(dst_buf, i, 0);
		buf_state = VB2_BUF_STATE_DONE;
		goto device_run_end;
	}

	if (jpeg->mode == MTK_JPEG_ENC) {
		mtk_jpeg_set_enc_dst(ctx, dst_buf, &enc_bs);
		mtk_jpeg_set_enc_src(ctx, src_buf, &enc_fb);
		spin_lock_irqsave(&jpeg->hw_lock[ctx->coreid], flags);
		mtk_jpeg_enc_reset(jpeg->reg_base[ctx->coreid]);
		mtk_jpeg_enc_set_config(jpeg->reg_base[ctx->coreid],
					&jpeg_src_buf->enc_param,
					&enc_bs, &enc_fb);
		mtk_jpeg_enc_start(jpeg->reg_base[ctx->coreid]);
	} else {
		if (mtk_jpeg_check_resolution_change(ctx,
				&jpeg_src_buf->dec_param)) {
			mtk_jpeg_queue_src_chg_event(ctx);
			ctx->state = MTK_JPEG_SOURCE_CHANGE;
			v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
			return;
		}

		mtk_jpeg_set_dec_src(ctx, src_buf, &bs);
		if (mtk_jpeg_set_dec_dst(ctx, &jpeg_src_buf->dec_param,
					dst_buf, &fb))
			goto device_run_end;

		spin_lock_irqsave(&jpeg->hw_lock[ctx->coreid], flags);
		mtk_jpeg_dec_reset(jpeg->reg_base[ctx->coreid]);
		mtk_jpeg_dec_set_config(jpeg->reg_base[ctx->coreid],
			&jpeg_src_buf->dec_param, &bs, &fb);

		mtk_jpeg_dec_start(jpeg->reg_base[ctx->coreid]);
	}

	spin_unlock_irqrestore(&jpeg->hw_lock[ctx->coreid], flags);
	return;

device_run_end:
	v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);
	if (src_buf != NULL)
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf), buf_state);

	if (dst_buf != NULL)
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf), buf_state);

	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
}

static int mtk_jpeg_job_ready(void *priv)
{
	struct mtk_jpeg_ctx *ctx = priv;

	return (ctx->state == MTK_JPEG_RUNNING) ? 1 : 0;
}

static void mtk_jpeg_job_abort(void *priv)
{
}

static const struct v4l2_m2m_ops mtk_jpeg_m2m_ops = {
	.device_run = mtk_jpeg_device_run,
	.job_ready  = mtk_jpeg_job_ready,
	.job_abort  = mtk_jpeg_job_abort,
};

static int mtk_jpeg_queue_init(void *priv, struct vb2_queue *src_vq,
			       struct vb2_queue *dst_vq)
{
	struct mtk_jpeg_ctx *ctx = priv;
	int ret;

	src_vq->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes = VB2_DMABUF | VB2_MMAP;
	src_vq->drv_priv = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_jpeg_src_buf);
	src_vq->ops = &mtk_jpeg_qops;
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
	dst_vq->ops = &mtk_jpeg_qops;
	dst_vq->mem_ops = &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->jpeg->lock;
	dst_vq->dev = ctx->jpeg->dev;
	ret = vb2_queue_init(dst_vq);

	return ret;
}

static void mtk_jpeg_clk_on(struct mtk_jpeg_dev *jpeg)
{
	int ret;
	pr_info("%s", __func__);

	smi_bus_prepare_enable(jpeg->larb_id[0], "JPEG");

	if (jpeg->mode == MTK_JPEG_DEC) {
		ret = clk_prepare_enable(jpeg->clk_jpeg_smi);
		if (ret)
			pr_info("clk_prepare_enable  failed");
		else
			pr_info("clk_prepare_enable  pass");
	}

	if (jpeg->mode == MTK_JPEG_ENC) {

		ret = clk_prepare_enable(jpeg->clk_jpeg[0]);
		if (ret)
			pr_info("clk_prepare_enable  failed");
	}
}

static void mtk_jpeg_clk_off(struct mtk_jpeg_dev *jpeg)
{
	pr_info("%s", __func__);
	if (jpeg->mode == MTK_JPEG_ENC)
		clk_disable_unprepare(jpeg->clk_jpeg[0]);

	if (jpeg->mode == MTK_JPEG_DEC)
		clk_disable_unprepare(jpeg->clk_jpeg_smi);


	smi_bus_disable_unprepare(jpeg->larb_id[0], "JPEG");
}

static void mtk_jpeg_clk_on_ctx(struct mtk_jpeg_ctx *ctx)
{
	int ret, larb_port_num;
	unsigned int larb_id, i;
	struct M4U_PORT_STRUCT port;
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	pr_info("%s +", __func__);

	smi_bus_prepare_enable(jpeg->larb_id[ctx->coreid], "JPEG");

	if (jpeg->mode == MTK_JPEG_DEC) {
		ret = clk_prepare_enable(jpeg->clk_jpeg_smi);
		if (ret)
			pr_info("clk_prepare_enable  failed");
		else
			pr_info("clk_prepare_enable  pass");
	}
	if (jpeg->mode == MTK_JPEG_ENC) {

		ret = clk_prepare_enable(jpeg->clk_jpeg[ctx->coreid]);
		if (ret)
			pr_info("clk_prepare_enable  failed");
		else
			pr_info("clk_prepare_enable  pass");


		larb_port_num = SMI_LARB_PORT_NUM[jpeg->larb_id[ctx->coreid]];
		larb_id = jpeg->larb_id[ctx->coreid];

		pr_info("port num %d  larb %d\n", larb_port_num, larb_id);
		//enable 34bits port configs & sram settings
		for (i = 0; i < larb_port_num; i++) {
			port.ePortID = MTK_M4U_ID(larb_id, i);
			port.Direction = 0;
			port.Distance = 1;
			port.domain = 0;
			port.Security = 0;
			port.Virtuality = 1;

			if (port.ePortID == jpeg->port_y_rdma[ctx->coreid] ||
			port.ePortID == jpeg->port_c_rdma[ctx->coreid] ||
			port.ePortID == jpeg->port_qtbl[ctx->coreid] ||
			port.ePortID == jpeg->port_bsdma[ctx->coreid])
				m4u_config_port(&port);
		}
	}
	enable_irq(jpeg->irq[ctx->coreid]);

	pr_info("%s -", __func__);
}


static void mtk_jpeg_clk_off_ctx(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;
	pr_info("%s  +", __func__);
	disable_irq(jpeg->irq[ctx->coreid]);

	if (jpeg->mode == MTK_JPEG_ENC)
		clk_disable_unprepare(jpeg->clk_jpeg[ctx->coreid]);

	if (jpeg->mode == MTK_JPEG_DEC)
		clk_disable_unprepare(jpeg->clk_jpeg_smi);


	smi_bus_disable_unprepare(jpeg->larb_id[ctx->coreid], "JPEG");

	pr_info("%s  -", __func__);
}


static void mtk_jpeg_clk_prepare(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	mtk_jpeg_clk_on_ctx(ctx);
	if (jpeg->mode == MTK_JPEG_ENC)
		mtk_jpeg_enc_reset(jpeg->reg_base[ctx->coreid]);
	else
		mtk_jpeg_dec_reset(jpeg->reg_base[0]);

}

static void mtk_jpeg_clk_unprepaer(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_dev *jpeg = ctx->jpeg;

	if (jpeg->mode == MTK_JPEG_ENC)
		mtk_jpeg_enc_reset(jpeg->reg_base[ctx->coreid]);
	else
		mtk_jpeg_dec_reset(jpeg->reg_base[0]);

	mtk_jpeg_clk_off_ctx(ctx);

}


static irqreturn_t mtk_jpeg_irq(int irq, void *priv)
{
	struct mtk_jpeg_dev *jpeg = priv;
	struct mtk_jpeg_ctx *ctx;
	struct vb2_buffer *src_buf = NULL, *dst_buf = NULL;
	struct mtk_jpeg_src_buf *jpeg_src_buf = NULL;
	enum vb2_buffer_state buf_state = VB2_BUF_STATE_ERROR;
	u32	irq_ret;
	u32 ret, result_size;
	int i;

	ctx = v4l2_m2m_get_curr_priv(jpeg->m2m_dev);
	if (!ctx) {
		v4l2_err(&jpeg->v4l2_dev, "Context is NULL\n");
		return IRQ_HANDLED;
	}

	pr_info("%s id %d+", __func__, ctx->coreid);

	src_buf = v4l2_m2m_src_buf_remove(ctx->fh.m2m_ctx);
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->fh.m2m_ctx);

	if (src_buf == NULL || dst_buf == NULL) {
		pr_info("%s null src or dst buffer\n", __func__);
		return IRQ_HANDLED;
	}

	jpeg_src_buf = mtk_jpeg_vb2_to_srcbuf(src_buf);

	if (jpeg->mode == MTK_JPEG_ENC) {
		ret = mtk_jpeg_enc_get_int_status(jpeg->reg_base[ctx->coreid]);
		irq_ret = mtk_jpeg_enc_enum_result(jpeg->reg_base[ctx->coreid],
			 ret, &result_size);
		if (irq_ret >= MTK_JPEG_ENC_RESULT_STALL)
			mtk_jpeg_enc_reset(jpeg->reg_base[ctx->coreid]);
		if (irq_ret != MTK_JPEG_ENC_RESULT_DONE) {
			v4l2_err(&jpeg->v4l2_dev, "encode failed\n");
			goto irq_end;
		}
		vb2_set_plane_payload(dst_buf, 0, result_size);
	} else {
		ret = mtk_jpeg_dec_get_int_status(jpeg->reg_base[ctx->coreid]);
		irq_ret = mtk_jpeg_dec_enum_result(ret);
		if (irq_ret >= MTK_JPEG_DEC_RESULT_UNDERFLOW)
			mtk_jpeg_dec_reset(jpeg->reg_base[ctx->coreid]);

		if (irq_ret != MTK_JPEG_DEC_RESULT_EOF_DONE) {
			v4l2_err(&jpeg->v4l2_dev, "decode failed\n");
			goto irq_end;
		}

	for (i = 0; i < dst_buf->num_planes; i++)
		vb2_set_plane_payload(dst_buf, i,
				jpeg_src_buf->dec_param.comp_size[i]);
	}

	buf_state = VB2_BUF_STATE_DONE;

irq_end:
	if (src_buf != NULL)
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf), buf_state);
	if (dst_buf != NULL)
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf), buf_state);
	v4l2_m2m_job_finish(jpeg->m2m_dev, ctx->fh.m2m_ctx);
	return IRQ_HANDLED;
}

static void mtk_jpeg_set_default_params(struct mtk_jpeg_ctx *ctx)
{
	struct mtk_jpeg_q_data *q = &ctx->out_q;
	unsigned int i, align_w, align_h;

	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;

	ctx->colorspace = V4L2_COLORSPACE_JPEG,
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;

	if (ctx->jpeg->mode == MTK_JPEG_ENC) {
		q->w = MTK_JPEG_MIN_WIDTH;
		q->h = MTK_JPEG_MIN_HEIGHT;
		q->fmt = mtk_jpeg_find_format(ctx, V4L2_PIX_FMT_YUYV, 1,
					      MTK_JPEG_FMT_TYPE_OUTPUT);
		align_w = q->w;
		align_h = q->h;
		align_w = ((align_w+1)>>1) << 1;
		align_w = align_w << 1;//jx change for bug
		v4l_bound_align_image(&align_w,
					MTK_JPEG_MIN_WIDTH,
					MTK_JPEG_MAX_WIDTH, 5,
					&align_h,
					MTK_JPEG_MIN_HEIGHT,
					MTK_JPEG_MAX_HEIGHT, 3, 0);
		if (align_w < MTK_JPEG_MIN_WIDTH &&
			(align_w + 32) <= MTK_JPEG_MAX_WIDTH)
			align_w += 32;
		if (align_h < MTK_JPEG_MIN_HEIGHT &&
			(align_h + 8) <= MTK_JPEG_MAX_HEIGHT)
			align_h += 8;
		q->sizeimage[0] = align_w * align_h + 64;
		q->bytesperline[0] = align_w;
	} else {
		q->fmt = mtk_jpeg_find_format(ctx, V4L2_PIX_FMT_JPEG, 1,
						      MTK_JPEG_FMT_TYPE_OUTPUT);
		q->w = MTK_JPEG_MIN_WIDTH;
		q->h = MTK_JPEG_MIN_HEIGHT;
		q->bytesperline[0] = 0;
		q->sizeimage[0] = MTK_JPEG_DEFAULT_SIZEIMAGE;
	}

	q = &ctx->cap_q;
	if (ctx->jpeg->mode == MTK_JPEG_ENC) {
		q->w = MTK_JPEG_MIN_WIDTH;
		q->h = MTK_JPEG_MIN_HEIGHT;
		q->fmt = mtk_jpeg_find_format(ctx, V4L2_PIX_FMT_JPEG, 1,
					MTK_JPEG_FMT_TYPE_CAPTURE);
		q->bytesperline[0] = 0;
		q->sizeimage[0] = MTK_JPEG_DEFAULT_SIZEIMAGE;
	} else {
		q->fmt = mtk_jpeg_find_format(ctx, V4L2_PIX_FMT_YUV420M, 3,
					MTK_JPEG_FMT_TYPE_CAPTURE);
		q->w = MTK_JPEG_MIN_WIDTH;
		q->h = MTK_JPEG_MIN_HEIGHT;

		for (i = 0; i < q->fmt->colplanes; i++) {
			u32 stride = q->w * q->fmt->h_sample[i] / 4;
			u32 h = q->h * q->fmt->v_sample[i] / 4;

			q->bytesperline[i] = stride;
			q->sizeimage[i] = stride * h;
			}
	}
}

static int mtk_jpeg_open(struct file *file)
{
	struct mtk_jpeg_dev *jpeg = video_drvdata(file);
	struct video_device *vfd = video_devdata(file);
	struct mtk_jpeg_ctx *ctx;
	int ret = 0;
	int i;



	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	if (down_interruptible(&jpeg->sem)) {
		ret = -ERESTARTSYS;
		goto free;
	}
	if (mutex_lock_interruptible(&jpeg->lock)) {
		ret = -ERESTARTSYS;
		goto free;
	}

	v4l2_fh_init(&ctx->fh, vfd);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);
	ctx->jpeg = jpeg;
	if (jpeg->mode == MTK_JPEG_ENC) {
		ret = mtk_jpeg_ctrls_setup(ctx);
		if (ret) {
			v4l2_err(&jpeg->v4l2_dev, "Failed to setup controls() (%d)\n",
					ret);
			goto error;
		}
	}
	ctx->coreid = MTK_JPEG_MAX_NCORE;

	for (i = 0; i < jpeg->ncore; i++) {
		if (jpeg->isused[i] == 0) {
			ctx->coreid = i;
			jpeg->isused[i] = 1;
			break;
		}

	}
	//pr_info("%s coreid %d corenum %d, isused(%d %d)\n", __func__,
	//	ctx->coreid, jpeg->ncore,
	//	jpeg->isused[0], jpeg->isused[1]);

	if (ctx->coreid == MTK_JPEG_MAX_NCORE) {
		pr_info("%s invalid coreid something wrong\n", __func__);
		ret = -ERESTARTSYS;
		goto error;
	}
	ctx->fh.m2m_ctx = v4l2_m2m_ctx_init(jpeg->m2m_dev, ctx,
					    mtk_jpeg_queue_init);
	if (IS_ERR(ctx->fh.m2m_ctx)) {
		ret = PTR_ERR(ctx->fh.m2m_ctx);
		goto error;
	}

	mtk_jpeg_set_default_params(ctx);
	mtk_jpeg_clk_prepare(ctx);

	mtk_jpeg_start_dvfs();

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

	mutex_lock(&jpeg->lock);

	mtk_jpeg_end_dvfs();

	mtk_jpeg_end_bw_request(ctx);

	mtk_jpeg_clk_unprepaer(ctx);
	jpeg->isused[ctx->coreid] = 0;

	//pr_info("%s coreid %d released num %d used(%d %d)\n", __func__,
	//	ctx->coreid, jpeg->ncore,
	//	jpeg->isused[0], jpeg->isused[1]);

	v4l2_m2m_ctx_release(ctx->fh.m2m_ctx);
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
	kfree(ctx);
	mutex_unlock(&jpeg->lock);
	up(&jpeg->sem);
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
	u32 id = 0;
	s32 ret;

	node = of_parse_phandle(jpeg->dev->of_node, "mediatek,larb", 0);
	if (!node)
		return -EINVAL;
	pdev = of_find_device_by_node(node);
	if (WARN_ON(!pdev)) {
		of_node_put(node);
		return -EINVAL;
	}

	if (pdev == NULL)
		return -EINVAL;

	ret = of_property_read_u32(pdev->dev.of_node, "mediatek,smi-id", &id);
	if (ret)
		return -EINVAL;

	pr_info("jpeg_clk_init id %d\n", id);
	jpeg->larb_id[0] = id;

	of_node_put(node);
	jpeg->larb[0] = &pdev->dev;

	if (jpeg->mode == MTK_JPEG_ENC) {
		jpeg->clk_jpeg[0] = devm_clk_get(jpeg->dev, "jpgenc");
		if (IS_ERR(jpeg->clk_jpeg[0]))
			return PTR_ERR_OR_ZERO(jpeg->clk_jpeg[0]);

		return 0;
	}

	jpeg->clk_jpeg[0] = devm_clk_get(jpeg->dev, "jpgdec");
	if (IS_ERR(jpeg->clk_jpeg[0]))
		return PTR_ERR(jpeg->clk_jpeg[0]);

	jpeg->clk_jpeg_smi = devm_clk_get(jpeg->dev, "jpgdec-smi");
	return PTR_ERR_OR_ZERO(jpeg->clk_jpeg_smi);
}

static int mtk_jpeg_probe(struct platform_device *pdev)
{
	struct mtk_jpeg_dev *jpeg;
	struct resource *res;
	int ret;
	int i;
	struct device_node *node = NULL;

	jpeg = devm_kzalloc(&pdev->dev, sizeof(*jpeg), GFP_KERNEL);
	if (!jpeg)
		return -ENOMEM;

	mutex_init(&jpeg->lock);
	spin_lock_init(&jpeg->hw_lock[0]);
	spin_lock_init(&jpeg->hw_lock[1]);
	jpeg->dev = &pdev->dev;
	jpeg->variant = of_device_get_match_data(jpeg->dev);
	jpeg->mode = jpeg->variant->jpeg_mode;
	node = pdev->dev.of_node;

	i = 0;
	res = platform_get_resource(pdev, IORESOURCE_MEM, i);
	if (!res) {
		pr_info("no base found i %d\n", i);
		ret = -EINVAL;
		return ret;
	}
	jpeg->reg_base[i] = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(jpeg->reg_base[i])) {
		ret = PTR_ERR(jpeg->reg_base[i]);
		return ret;
	}
	jpeg->isused[i] = 0;
	jpeg->ncore = 1;
	sema_init(&jpeg->sem, jpeg->ncore);

	pr_info("jpeg %d core platform\n", jpeg->ncore);
	//res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	//jpeg->reg_base = devm_ioremap_resource(&pdev->dev, res);
	//if (IS_ERR(jpeg->reg_base)) {
	//	ret = PTR_ERR(jpeg->reg_base);
	//	return ret;
	//}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	jpeg->irq[0] = platform_get_irq(pdev, 0);
	pr_info("%s irq0 %d\n", __func__, jpeg->irq[0]);
	if (!res || jpeg->irq[0] < 0) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to get jpeg_irq %d.\n",
			jpeg->irq[0]);
		ret = -EINVAL;
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, jpeg->irq[0], mtk_jpeg_irq, 0,
			       pdev->name, jpeg);


	if (ret) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to request jpeg_irq %d (%d)\n",
			jpeg->irq[0], ret);
		ret = -EINVAL;
		goto err_req_irq;
	}

	disable_irq(jpeg->irq[0]);

	ret = of_property_read_u32(node, "cshot-spec", &cshot_spec_dts);
	if (ret) {
		pr_info("cshot spec read failed:%d\n", ret);
		pr_info("init cshot spec as 0xFFFFFFFF\n");
		cshot_spec_dts = 0xFFFFFFFF;
	}


	ret = of_property_read_u32_index(node, "port-id",
		MTK_JPEG_PORT_INDEX_YRDMA, &jpeg->port_y_rdma[0]);
	if (ret)
		pr_info("YRDMA read failed:%d\n", ret);


	ret = of_property_read_u32_index(node, "port-id",
		MTK_JPEG_PORT_INDEX_CRDMA, &jpeg->port_c_rdma[0]);
	if (ret)
		pr_info("CRDMA read failed:%d\n", ret);


	ret = of_property_read_u32_index(node, "port-id",
		MTK_JPEG_PORT_INDEX_QTBLE, &jpeg->port_qtbl[0]);
	if (ret)
		pr_info("Qtable read failed:%d\n", ret);


	ret = of_property_read_u32_index(node, "port-id",
		MTK_JPEG_PORT_INDEX_BSDMA, &jpeg->port_bsdma[0]);
	if (ret)
		pr_info("BSDMA read failed:%d\n", ret);

	ret = mtk_jpeg_clk_init(jpeg);
	if (ret) {
		dev_err(&pdev->dev, "Failed to init clk, err %d\n", ret);
		goto err_clk_init;
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

	jpeg->vfd_jpeg = video_device_alloc();
	if (!jpeg->vfd_jpeg) {
		ret = -ENOMEM;
		goto err_vfd_jpeg_alloc;
	}
	if (jpeg->mode == MTK_JPEG_ENC) {
		snprintf(jpeg->vfd_jpeg->name, sizeof(jpeg->vfd_jpeg->name),
			"%s-enc", MTK_JPEG_NAME);
	} else {
		snprintf(jpeg->vfd_jpeg->name, sizeof(jpeg->vfd_jpeg->name),
			"%s-dec", MTK_JPEG_NAME);
	}
	jpeg->vfd_jpeg->fops = &mtk_jpeg_fops;
	jpeg->vfd_jpeg->ioctl_ops = &mtk_jpeg_ioctl_ops;
	jpeg->vfd_jpeg->minor = -1;
	jpeg->vfd_jpeg->release = video_device_release;
	jpeg->vfd_jpeg->lock = &jpeg->lock;
	jpeg->vfd_jpeg->v4l2_dev = &jpeg->v4l2_dev;
	jpeg->vfd_jpeg->vfl_dir = VFL_DIR_M2M;
	jpeg->vfd_jpeg->device_caps = V4L2_CAP_STREAMING |
				      V4L2_CAP_VIDEO_M2M_MPLANE;

	ret = video_register_device(jpeg->vfd_jpeg, VFL_TYPE_GRABBER, -1);
	if (ret) {
		v4l2_err(&jpeg->v4l2_dev, "Failed to register video device\n");
		goto err_vfd_jpeg_register;
	}

	mtk_jpeg_prepare_bw_request(jpeg);

	mtk_jpeg_prepare_dvfs();

	video_set_drvdata(jpeg->vfd_jpeg, jpeg);
	v4l2_info(&jpeg->v4l2_dev,
		  "jpeg device %d registered as /dev/video%d (%d,%d)\n",
		jpeg->mode, jpeg->vfd_jpeg->num, VIDEO_MAJOR,
		jpeg->vfd_jpeg->minor);

	platform_set_drvdata(pdev, jpeg);

	pm_runtime_enable(&pdev->dev);

	g_ion_client = ion_client_create(g_ion_device, "jpegenc");
	if (!g_ion_client)
		pr_info("%s create ion client fail\n", __func__);

	return 0;

err_vfd_jpeg_register:
	video_device_release(jpeg->vfd_jpeg);

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
	video_unregister_device(jpeg->vfd_jpeg);
	video_device_release(jpeg->vfd_jpeg);
	v4l2_m2m_release(jpeg->m2m_dev);
	v4l2_device_unregister(&jpeg->v4l2_dev);
	if (g_ion_client)
		ion_client_destroy(g_ion_client);

	mtk_jpeg_remove_bw_request(jpeg);

	mtk_jpeg_unprepare_dvfs();

	return 0;
}

static __maybe_unused int mtk_jpeg_pm_suspend(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	if (jpeg->mode == MTK_JPEG_ENC)
		mtk_jpeg_enc_reset(jpeg->reg_base[0]);
	else
		mtk_jpeg_dec_reset(jpeg->reg_base[0]);

	mtk_jpeg_clk_off(jpeg);

	return 0;
}

static __maybe_unused int mtk_jpeg_pm_resume(struct device *dev)
{
	struct mtk_jpeg_dev *jpeg = dev_get_drvdata(dev);

	mtk_jpeg_clk_on(jpeg);
	if (jpeg->mode == MTK_JPEG_ENC)
		mtk_jpeg_enc_reset(jpeg->reg_base[0]);
	else
		mtk_jpeg_dec_reset(jpeg->reg_base[0]);

	return 0;
}

static __maybe_unused int mtk_jpeg_suspend(struct device *dev)
{
	int ret;

	if (pm_runtime_suspended(dev))
		return 0;

	ret = mtk_jpeg_pm_suspend(dev);
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

static struct mtk_jpeg_variant jpeg_dec_drvdata = {
	.jpeg_mode	= MTK_JPEG_DEC,
};

static struct mtk_jpeg_variant jpeg_enc_drvdata = {
	.jpeg_mode	= MTK_JPEG_ENC,
};


static const struct of_device_id mtk_jpeg_match[] = {
	{
		.compatible = "mediatek,mt8173-jpgdec",
		.data       = &jpeg_dec_drvdata,
	},
	{
		.compatible = "mediatek,mt2701-jpgdec",
		.data       = &jpeg_dec_drvdata,
	},
	{
		.compatible = "mediatek,mt2701-jpgenc",
		.data       = &jpeg_enc_drvdata,
	},
	{
		.compatible = "mediatek,jpgenc",
		.data       = &jpeg_enc_drvdata,
	},
	{
		.compatible = "mediatek,mt2712-jpgdec",
		.data       = &jpeg_dec_drvdata,
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
		//.pm             = &mtk_jpeg_pm_ops,
	},
};

module_platform_driver(mtk_jpeg_driver);

MODULE_DESCRIPTION("MediaTek JPEG codec driver");
MODULE_LICENSE("GPL v2");
