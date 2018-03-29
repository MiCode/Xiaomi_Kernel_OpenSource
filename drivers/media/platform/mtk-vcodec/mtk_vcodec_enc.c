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

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_enc.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "venc_drv_if.h"

static void mtk_venc_worker(struct work_struct *work);

static struct mtk_video_fmt mtk_video_formats[] = {
	{
		.name		= "4:2:0 3 Planes Y/Cb/Cr",
		.fourcc		= V4L2_PIX_FMT_YUV420,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 3,
	},
	{
		.name		= "4:2:0 3 Planes Y/Cr/Cb",
		.fourcc		= V4L2_PIX_FMT_YVU420,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 3,
	},
	{
		.name		= "4:2:0 2 Planes Y/CbCr",
		.fourcc		= V4L2_PIX_FMT_NV12,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 2,
	},
	{
		.name		= "4:2:0 2 Planes Y/CrCb",
		.fourcc		= V4L2_PIX_FMT_NV21,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 2,
	},
	{
		.name		= "4:2:0 3 none contiguous Planes Y/Cb/Cr",
		.fourcc		= V4L2_PIX_FMT_YUV420M,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 3,
	},
	{
		.name		= "4:2:0 3 none contiguous Planes Y/Cr/Cb",
		.fourcc		= V4L2_PIX_FMT_YVU420M,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 3,
	},
	{
		.name		= "4:2:0 2 none contiguous Planes Y/CbCr",
		.fourcc		= V4L2_PIX_FMT_NV12M,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 2,
	},
	{
		.name		= "4:2:0 2 none contiguous Planes Y/CrCb",
		.fourcc		= V4L2_PIX_FMT_NV21M,
		.type		= MTK_FMT_FRAME,
		.num_planes	= 2,
	},
	{
		.name		= "H264 Encoded Stream",
		.fourcc		= V4L2_PIX_FMT_H264,
		.type		= MTK_FMT_ENC,
		.num_planes	= 1,
	},
	{
		.name		= "VP8 Encoded Stream",
		.fourcc		= V4L2_PIX_FMT_VP8,
		.type		= MTK_FMT_ENC,
		.num_planes	= 1,
	},
};

#define NUM_FORMATS ARRAY_SIZE(mtk_video_formats)

static struct mtk_vcodec_ctrl controls[] = {
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = (1 << 30) - 1,
		.step = 1,
		.default_value = 4000000,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_B_FRAMES,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.step = 1,
		.default_value = 51,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		.maximum = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		.default_value = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_MAIN,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_4_2,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_4_0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 30,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = (1 << 16) - 1,
		.step = 1,
		.default_value = 30,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_DISABLED,
		.maximum = V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_NOT_CODED,
		.default_value =
			V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_DISABLED,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE,
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED,
		.maximum = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT,
		.default_value = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED,
		.menu_skip_mask = 0,
	},
};

#define NUM_CTRLS ARRAY_SIZE(controls)

static const struct mtk_codec_framesizes mtk_venc_framesizes[] = {
	{
		.fourcc	= V4L2_PIX_FMT_H264,
		.stepwise = {  160, 1920, 16, 128, 1088, 16 },
	},
	{
		.fourcc = V4L2_PIX_FMT_VP8,
		.stepwise = {  160, 1920, 16, 128, 1088, 16 },
	},
};

#define NUM_SUPPORTED_FRAMESIZE ARRAY_SIZE(mtk_venc_framesizes)

static int vidioc_venc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct mtk_enc_params *p = &ctx->enc_params;
	int ret = 0;

	mtk_v4l2_debug(1, "[%d] id = %d/%d, val = %d", ctrl->id,
			ctx->idx, ctrl->id - V4L2_CID_MPEG_BASE, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE:

		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_BITRATE val = %d",
			ctrl->val);
		p->bitrate = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_BITRATE;
		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_B_FRAMES val = %d",
			ctrl->val);
		p->num_b_frame = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE val = %d",
			ctrl->val);
		p->rc_frame = ctrl->val;
		break;

	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_H264_MAX_QP val = %d",
			ctrl->val);
		p->h264_max_qp = ctrl->val;
		break;

	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_HEADER_MODE val = %d",
			ctrl->val);
		p->seq_hdr_mode = ctrl->val;
		break;

	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE val = %d",
			ctrl->val);
		p->rc_mb = ctrl->val;
		break;

	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_H264_PROFILE val = %d",
			ctrl->val);
		p->h264_profile = ctrl->val;
		break;

	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_H264_LEVEL val = %d",
			ctrl->val);
		p->h264_level = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_H264_I_PERIOD val = %d",
			ctrl->val);
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_VIDEO_GOP_SIZE val = %d",
			ctrl->val);
		p->gop_size = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_INTRA_PERIOD;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE val = %d",
			ctrl->val);
		if (ctrl->val ==
			V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_NOT_CODED) {
			v4l2_err(&dev->v4l2_dev, "unsupported frame type %x\n",
				 ctrl->val);
			ret = -EINVAL;
			break;
		}
		if (ctrl->val == V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_I_FRAME)
			p->force_intra = 1;
		else if (ctrl->val ==
			V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_DISABLED)
			p->force_intra = 0;
		/* always allow user to insert I frame */
		ctrl->val = V4L2_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE_DISABLED;
		ctx->param_change |= MTK_ENCODE_PARAM_FRAME_TYPE;
		break;

	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE:
		mtk_v4l2_debug(1, "V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE val = %d",
			ctrl->val);
		if (ctrl->val == V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED)
			p->skip_frame = 0;
		else
			p->skip_frame = 1;
		/* always allow user to skip frame */
		ctrl->val = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED;
		ctx->param_change |= MTK_ENCODE_PARAM_SKIP_FRAME;
		break;

	default:
		mtk_v4l2_err("Invalid control, id=%d, val=%d\n",
				ctrl->id, ctrl->val);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops mtk_vcodec_enc_ctrl_ops = {
	.s_ctrl = vidioc_venc_s_ctrl,
};

static int vidioc_enum_fmt(struct file *file, struct v4l2_fmtdesc *f,
			   bool out)
{
	struct mtk_video_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < NUM_FORMATS; ++i) {
		if (out && mtk_video_formats[i].type != MTK_FMT_FRAME)
			continue;
		else if (!out && mtk_video_formats[i].type != MTK_FMT_ENC)
			continue;

		if (j == f->index) {
			fmt = &mtk_video_formats[i];
			strlcpy(f->description, fmt->name,
				sizeof(f->description));
			f->pixelformat = fmt->fourcc;
			mtk_v4l2_debug(1, "f->index=%d i=%d fmt->name=%s",
					 f->index, i, fmt->name);
			return 0;
		}
		++j;
	}

	return -EINVAL;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,
				  struct v4l2_frmsizeenum *fsize)
{
	int i = 0;

	for (i = 0; i < NUM_SUPPORTED_FRAMESIZE; ++i) {
		if (fsize->pixel_format != mtk_venc_framesizes[i].fourcc)
			continue;

		if (!fsize->index) {
			fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
			fsize->stepwise = mtk_venc_framesizes[i].stepwise;
			mtk_v4l2_debug(1, "%d %d %d %d %d %d",
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

static int vidioc_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(file, f, false);
}

static int vidioc_enum_fmt_vid_out_mplane(struct file *file, void *prov,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(file, f, true);
}

static int vidioc_venc_streamon(struct file *file, void *priv,
				enum v4l2_buf_type type)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	return v4l2_m2m_streamon(file, ctx->m2m_ctx, type);
}

static int vidioc_venc_streamoff(struct file *file, void *priv,
				 enum v4l2_buf_type type)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	return v4l2_m2m_streamoff(file, ctx->m2m_ctx, type);
}

static int vidioc_venc_reqbufs(struct file *file, void *priv,
			       struct v4l2_requestbuffers *reqbufs)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	mtk_v4l2_debug(1, "[%d]-> type=%d count=%d",
			 ctx->idx, reqbufs->type, reqbufs->count);

	return v4l2_m2m_reqbufs(file, ctx->m2m_ctx, reqbufs);
}

static int vidioc_venc_querybuf(struct file *file, void *priv,
				struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	return v4l2_m2m_querybuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_venc_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;
#if MTK_V4L2_BENCHMARK
	struct timeval begin, end;

	do_gettimeofday(&begin);
#endif

	ret = v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);

#if MTK_V4L2_BENCHMARK
	do_gettimeofday(&end);

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->total_qbuf_cap_cnt++;
		ctx->total_qbuf_cap_time +=
			((end.tv_sec - begin.tv_sec) * 1000000 +
				end.tv_usec - begin.tv_usec);
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->total_qbuf_out_cnt++;
		ctx->total_qbuf_out_time +=
			((end.tv_sec - begin.tv_sec) * 1000000 +
				end.tv_usec - begin.tv_usec);
	}

#endif

	return ret;
}

static int vidioc_venc_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;
#if MTK_V4L2_BENCHMARK
	struct timeval begin, end;

	do_gettimeofday(&begin);
#endif

	ret = v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
#if MTK_V4L2_BENCHMARK

	do_gettimeofday(&end);
	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ctx->total_dqbuf_cap_cnt++;
		ctx->total_dqbuf_cap_time +=
			((end.tv_sec - begin.tv_sec) * 1000000 +
				end.tv_usec - begin.tv_usec);
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->total_dqbuf_out_cnt++;
		ctx->total_dqbuf_out_time +=
			((end.tv_sec - begin.tv_sec) * 1000000 +
				end.tv_usec - begin.tv_usec);
	}

#endif
	return ret;
}
static int vidioc_venc_expbuf(struct file *file, void *priv,
			      struct v4l2_exportbuffer *eb)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	int ret;
#if MTK_V4L2_BENCHMARK
	struct timeval begin, end;

	do_gettimeofday(&begin);
#endif

	ret = v4l2_m2m_expbuf(file, ctx->m2m_ctx, eb);

#if MTK_V4L2_BENCHMARK
	do_gettimeofday(&end);
	ctx->total_expbuf_time +=
		((end.tv_sec - begin.tv_sec) * 1000000 +
			end.tv_usec - begin.tv_usec);
#endif
	return ret;
}

static int vidioc_venc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	strncpy(cap->driver, MTK_VCODEC_ENC_NAME, sizeof(cap->driver) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
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

static int vidioc_venc_subscribe_event(struct v4l2_fh *fh,
		       const struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, 0, NULL);
}

static int vidioc_venc_s_parm(struct file *file, void *priv,
			      struct v4l2_streamparm *a)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (a->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->enc_params.framerate_num =
			a->parm.output.timeperframe.denominator;
		ctx->enc_params.framerate_denom =
			a->parm.output.timeperframe.numerator;
		ctx->param_change |= MTK_ENCODE_PARAM_FRAMERATE;

		mtk_v4l2_debug(1, "[%d] framerate = %d/%d",
				ctx->idx,
				 ctx->enc_params.framerate_num,
				 ctx->enc_params.framerate_denom);
	} else {
		mtk_v4l2_err("Non support param type %d",
				a->type);
		return -EINVAL;
	}
	return 0;
}

static struct mtk_q_data *mtk_venc_get_q_data(struct mtk_vcodec_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->q_data[MTK_Q_DATA_SRC];
	else
		return &ctx->q_data[MTK_Q_DATA_DST];
}

static struct mtk_video_fmt *mtk_venc_find_format(struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;
	unsigned int k;

	for (k = 0; k < NUM_FORMATS; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat)
			return fmt;
	}

	return NULL;
}

static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;
	char str[10];
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;

	mtk_vcodec_fmt2str(f->fmt.pix_mp.pixelformat, str);

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = mtk_venc_find_format(f);
		if (!fmt) {
			mtk_v4l2_err("failed to try output format %s\n", str);
			return -EINVAL;
		}
		if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
			mtk_v4l2_err("must be set encoding output size %s\n",
				       str);
			return -EINVAL;
		}

		pix_fmt_mp->plane_fmt[0].bytesperline =
			pix_fmt_mp->plane_fmt[0].sizeimage;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = mtk_venc_find_format(f);
		if (!fmt) {
			mtk_v4l2_err("failed to try output format %s\n", str);
			return -EINVAL;
		}

		if (fmt->num_planes != pix_fmt_mp->num_planes) {
			mtk_v4l2_err("failed to try output format %d %d %s\n",
				       fmt->num_planes, pix_fmt_mp->num_planes,
				       str);
		}

		v4l_bound_align_image(&pix_fmt_mp->width, 8, 1920, 1,
				      &pix_fmt_mp->height, 4, 1080, 1, 0);
	} else {
		pr_err("invalid buf type %d\n", f->type);
		return -EINVAL;
	}
	return 0;
}

static void mtk_venc_set_param(struct mtk_vcodec_ctx *ctx, void *param)
{
	struct venc_enc_prm *p = (struct venc_enc_prm *)param;
	struct mtk_q_data *q_data_src = &ctx->q_data[MTK_Q_DATA_SRC];
	struct mtk_enc_params *enc_params = &ctx->enc_params;
	unsigned int frame_rate;

	frame_rate = enc_params->framerate_num / enc_params->framerate_denom;

	switch (q_data_src->fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YUV420M:
		p->input_fourcc = VENC_YUV_FORMAT_420;
		break;
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YVU420M:
		p->input_fourcc = VENC_YUV_FORMAT_YV12;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M:
		p->input_fourcc = VENC_YUV_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV21M:
		p->input_fourcc = VENC_YUV_FORMAT_NV21;
		break;
	}
	p->h264_profile = enc_params->h264_profile;
	p->h264_level = enc_params->h264_level;
	p->width = q_data_src->width;
	p->height = q_data_src->height;
	p->buf_width = q_data_src->bytesperline[0];
	p->buf_height = ((q_data_src->height + 0xf) & (~0xf));
	p->frm_rate = frame_rate;
	p->intra_period = enc_params->gop_size;
	p->bitrate = enc_params->bitrate;

	ctx->param_change = MTK_ENCODE_PARAM_NONE;

	mtk_v4l2_debug(1,
			"fmt 0x%x, P/L %d/%d, w/h %d/%d, buf %d/%d, fps/bps %d/%d, gop %d",
			p->input_fourcc, p->h264_profile, p->h264_level, p->width,
			p->height, p->buf_width, p->buf_height, p->frm_rate,
			p->bitrate, p->intra_period);
}

static int vidioc_venc_s_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct mtk_vcodec_dev *dev = video_drvdata(file);
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	struct venc_enc_prm param = {0};
	int i, ret;

	ret = vidioc_try_fmt(file, priv, f);
	if (ret)
		return ret;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		v4l2_err(&dev->v4l2_dev, "fail to get vq\n");
		return -EINVAL;
	}

	if (vb2_is_busy(vq)) {
		v4l2_err(&dev->v4l2_dev, "queue busy\n");
		return -EBUSY;
	}

	q_data = mtk_venc_get_q_data(ctx, f->type);
	if (!q_data) {
		v4l2_err(&dev->v4l2_dev, "fail to get q data\n");
		return -EINVAL;
	}

	q_data->fmt = mtk_venc_find_format(f);
	if (!q_data->fmt) {
		v4l2_err(&dev->v4l2_dev, "q data null format\n");
		return -EINVAL;
	}

	q_data->width		= f->fmt.pix_mp.width;
	q_data->height		= f->fmt.pix_mp.height;
	q_data->colorspace	= f->fmt.pix_mp.colorspace;
	q_data->field		= f->fmt.pix_mp.field;

	for (i = 0; i < f->fmt.pix_mp.num_planes; i++) {
		struct v4l2_plane_pix_format	*plane_fmt;

		plane_fmt = &f->fmt.pix_mp.plane_fmt[i];
		q_data->bytesperline[i]	= plane_fmt->bytesperline;
		q_data->sizeimage[i]	= plane_fmt->sizeimage;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q_data->width		= f->fmt.pix_mp.width;
		q_data->height		= f->fmt.pix_mp.height;

		if (ctx->state == MTK_STATE_FREE && ctx->q_data[MTK_Q_DATA_DST].fmt) {

			ret = venc_if_create(ctx,
					     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "venc_if_create failed=%d, codec type=%x\n",
				ret, ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc);
				return -EINVAL;
			}

			mtk_venc_set_param(ctx, &param);
			ret = venc_if_set_param(ctx,
						VENC_SET_PARAM_ENC, &param);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "venc_if_set_param failed=%d\n",
					 ret);
				venc_if_release(ctx);
			}

			INIT_WORK(&ctx->encode_work, mtk_venc_worker);
			ctx->state = MTK_STATE_INIT;
		} else if (ctx->state != MTK_STATE_FREE) {
			mtk_venc_set_param(ctx, &param);
			ret = venc_if_set_param(ctx,
						VENC_SET_PARAM_ENC, &param);
			if (ret) {
				v4l2_err(&dev->v4l2_dev, "venc_if_set_param failed=%d\n",
					 ret);
				return -EINVAL;
			}
		}

		for (i = 0; i < MTK_VCODEC_MAX_PLANES; i++)
			ctx->q_data[MTK_Q_DATA_SRC].sizeimage[i] = param.sizeimage[i];

		ctx->q_data[MTK_Q_DATA_SRC].bytesperline[0] = ALIGN(q_data->width, 16);

		if (q_data->fmt->num_planes == 2) {
			ctx->q_data[MTK_Q_DATA_SRC].bytesperline[1] =
				ALIGN(q_data->width, 16);
			ctx->q_data[MTK_Q_DATA_SRC].bytesperline[2] = 0;
		} else {
			ctx->q_data[MTK_Q_DATA_SRC].bytesperline[1] =
				ALIGN(q_data->width, 16) / 2;
			ctx->q_data[MTK_Q_DATA_SRC].bytesperline[2] =
				ALIGN(q_data->width, 16) / 2;
		}

		pix_fmt_mp->plane_fmt[0].sizeimage = q_data->sizeimage[0];
		pix_fmt_mp->plane_fmt[0].bytesperline =
			ctx->q_data[MTK_Q_DATA_SRC].bytesperline[0];
		pix_fmt_mp->plane_fmt[1].sizeimage = q_data->sizeimage[1];
		pix_fmt_mp->plane_fmt[1].bytesperline =
			ctx->q_data[MTK_Q_DATA_SRC].bytesperline[1];
		pix_fmt_mp->plane_fmt[2].sizeimage = q_data->sizeimage[2];
		pix_fmt_mp->plane_fmt[2].bytesperline =
			ctx->q_data[MTK_Q_DATA_SRC].bytesperline[2];
	}

	mtk_v4l2_debug(0,
			 "[%d]: t=%d wxh=%dx%d fmt=%c%c%c%c sz=0x%x-%x-%x",
			 ctx->idx,
			 f->type,
			 q_data->width, q_data->height,
			 (f->fmt.pix_mp.pixelformat & 0xff),
			 (f->fmt.pix_mp.pixelformat >>  8) & 0xff,
			 (f->fmt.pix_mp.pixelformat >> 16) & 0xff,
			 (f->fmt.pix_mp.pixelformat >> 24) & 0xff,
			 q_data->sizeimage[0],
			 q_data->sizeimage[1],
			 q_data->sizeimage[2]);

	return 0;
}

static int vidioc_venc_g_fmt(struct file *file, void *priv,
			     struct v4l2_format *f)
{
	struct v4l2_pix_format_mplane *pix = &f->fmt.pix_mp;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	int i;

	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq)
		return -EINVAL;

	q_data = mtk_venc_get_q_data(ctx, f->type);

	pix->width = q_data->width;
	pix->height = q_data->height;
	pix->pixelformat = q_data->fmt->fourcc;
	pix->field = q_data->field;
	pix->colorspace = q_data->colorspace;
	pix->num_planes = q_data->fmt->num_planes;

	for (i = 0; i < pix->num_planes; i++) {
		pix->plane_fmt[i].bytesperline = q_data->bytesperline[i];
		pix->plane_fmt[i].sizeimage = q_data->sizeimage[i];
	}

	mtk_v4l2_debug(1,
			 "[%d]<- type=%d wxh=%dx%d fmt=%c%c%c%c sz[0]=0x%x sz[1]=0x%x",
			 ctx->idx, f->type,
			 pix->width, pix->height,
			 (pix->pixelformat & 0xff),
			 (pix->pixelformat >>  8) & 0xff,
			 (pix->pixelformat >> 16) & 0xff,
			 (pix->pixelformat >> 24) & 0xff,
			 pix->plane_fmt[0].sizeimage,
			 pix->plane_fmt[1].sizeimage);

	return 0;
}

static int vidioc_venc_g_ctrl(struct file *file, void *fh,
			      struct v4l2_control *ctrl)
{
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
		ctrl->value = 1;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int vidioc_venc_s_crop(struct file *file, void *fh,
			      const struct v4l2_crop *a)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct mtk_q_data *q_data;

	if (a->c.left || a->c.top)
		return -EINVAL;

	q_data = mtk_venc_get_q_data(ctx, a->type);
	if (!q_data)
		return -EINVAL;

	return 0;
}

static int vidioc_venc_g_crop(struct file *file, void *fh,
					struct v4l2_crop *a)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(fh);
	struct mtk_q_data *q_data;

	if (a->c.left || a->c.top)
		return -EINVAL;

	q_data = mtk_venc_get_q_data(ctx, a->type);
	if (!q_data)
		return -EINVAL;

	a->c.width = q_data->width;
	a->c.height = q_data->height;

	return 0;
}


const struct v4l2_ioctl_ops mtk_venc_ioctl_ops = {
	.vidioc_streamon		= vidioc_venc_streamon,
	.vidioc_streamoff		= vidioc_venc_streamoff,

	.vidioc_reqbufs			= vidioc_venc_reqbufs,
	.vidioc_querybuf		= vidioc_venc_querybuf,
	.vidioc_qbuf			= vidioc_venc_qbuf,
	.vidioc_expbuf			= vidioc_venc_expbuf,
	.vidioc_dqbuf			= vidioc_venc_dqbuf,

	.vidioc_querycap		= vidioc_venc_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = vidioc_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes = vidioc_enum_framesizes,

	.vidioc_subscribe_event		= vidioc_venc_subscribe_event,

	.vidioc_s_parm			= vidioc_venc_s_parm,

	.vidioc_s_fmt_vid_cap_mplane	= vidioc_venc_s_fmt,
	.vidioc_s_fmt_vid_out_mplane	= vidioc_venc_s_fmt,

	.vidioc_g_fmt_vid_cap_mplane	= vidioc_venc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane	= vidioc_venc_g_fmt,

	.vidioc_g_ctrl			= vidioc_venc_g_ctrl,

	.vidioc_s_crop			= vidioc_venc_s_crop,
	.vidioc_g_crop			= vidioc_venc_g_crop,

};

static int vb2ops_venc_queue_setup(struct vb2_queue *vq,
				   const struct v4l2_format *fmt,
				   unsigned int *nbuffers,
				   unsigned int *nplanes,
				   unsigned int sizes[], void *alloc_ctxs[])
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_q_data *q_data;

	q_data = mtk_venc_get_q_data(ctx, vq->type);

	if (*nbuffers < 1)
		*nbuffers = 1;
	if (*nbuffers > MTK_VIDEO_MAX_FRAME)
		*nbuffers = MTK_VIDEO_MAX_FRAME;

	*nplanes = q_data->fmt->num_planes;

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		unsigned int i;

		for (i = 0; i < *nplanes; i++) {
			sizes[i] = q_data->sizeimage[i];
			alloc_ctxs[i] = ctx->dev->alloc_ctx;
		}
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		sizes[0] = q_data->sizeimage[0];
		alloc_ctxs[0] = ctx->dev->alloc_ctx;
	} else {
		return -EINVAL;
	}

	mtk_v4l2_debug(2,
			 "[%d]get %d buffer(s) of size 0x%x each",
			 ctx->idx, *nbuffers, sizes[0]);

	return 0;
}

static int vb2ops_venc_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_q_data *q_data;
	int i;

	q_data = mtk_venc_get_q_data(ctx, vb->vb2_queue->type);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			mtk_v4l2_debug(2,
					 "data will not fit into plane %d (%lu < %d)",
					 i, vb2_plane_size(vb, i),
					 q_data->sizeimage[i]);
			return -EINVAL;
		}
	}

	return 0;
}

static void vb2ops_venc_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_video_enc_buf *buf =
			container_of(vb, struct mtk_video_enc_buf, b);

	if ((vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
		(ctx->param_change != MTK_ENCODE_PARAM_NONE)) {
		mtk_v4l2_debug(1,
				"[%d] Before id=%d encode parameter change %x",
				ctx->idx, vb->v4l2_buf.index,
				ctx->param_change);
		buf->param_change = ctx->param_change;
		if (buf->param_change & MTK_ENCODE_PARAM_BITRATE) {
			buf->enc_params.bitrate = ctx->enc_params.bitrate;
			mtk_v4l2_debug(1, "[%d] change param br=%d",
				ctx->idx,
				 buf->enc_params.bitrate);
		}
		if (ctx->param_change & MTK_ENCODE_PARAM_FRAMERATE) {
			buf->enc_params.framerate_num =
				ctx->enc_params.framerate_num;
			buf->enc_params.framerate_denom =
				ctx->enc_params.framerate_denom;
			mtk_v4l2_debug(1, "[%d] change param fr=%d",
					ctx->idx,
					buf->enc_params.framerate_num /
					buf->enc_params.framerate_denom);
		}
		if (ctx->param_change & MTK_ENCODE_PARAM_INTRA_PERIOD) {
			buf->enc_params.gop_size = ctx->enc_params.gop_size;
			mtk_v4l2_debug(1, "[%d] change param intra period=%d",
					ctx->idx,
					 buf->enc_params.gop_size);
		}
		if (ctx->param_change & MTK_ENCODE_PARAM_FRAME_TYPE) {
			buf->enc_params.force_intra =
				ctx->enc_params.force_intra;
			mtk_v4l2_debug(1, "[%d] change param force I=%d",
					ctx->idx,
					 buf->enc_params.force_intra);
		}
		if (ctx->param_change & MTK_ENCODE_PARAM_SKIP_FRAME) {
			buf->enc_params.skip_frame =
				ctx->enc_params.skip_frame;
			mtk_v4l2_debug(1, "[%d] change param skip frame=%d",
					ctx->idx,
					 buf->enc_params.skip_frame);
		}
		ctx->param_change = MTK_ENCODE_PARAM_NONE;
	}

	v4l2_m2m_buf_queue(ctx->m2m_ctx, vb);
}

static int vb2ops_venc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct v4l2_device *v4l2_dev = &ctx->dev->v4l2_dev;
	int ret;
	struct venc_enc_prm param;

#if MTK_V4L2_BENCHMARK
	struct timeval begin, end;

	do_gettimeofday(&begin);
#endif

	/* Once state turn into MTK_STATE_ABORT, we need stop_streaming to clear it */
	if ((ctx->state == MTK_STATE_ABORT) || (ctx->state == MTK_STATE_FREE))
		return -EINVAL;

	if (!(vb2_start_streaming_called(&ctx->m2m_ctx->out_q_ctx.q) &
	      vb2_start_streaming_called(&ctx->m2m_ctx->cap_q_ctx.q))) {
		mtk_v4l2_debug(1, "[%d]-> out=%d cap=%d",
				 ctx->idx,
		 vb2_start_streaming_called(&ctx->m2m_ctx->out_q_ctx.q),
		 vb2_start_streaming_called(&ctx->m2m_ctx->cap_q_ctx.q));
		return 0;
	}

		mtk_venc_set_param(ctx, &param);
		ret = venc_if_set_param(ctx,
					VENC_SET_PARAM_ENC, &param);
		if (ret) {
			v4l2_err(v4l2_dev, "venc_if_set_param failed=%d\n",
				 ret);
		ctx->state = MTK_STATE_ABORT;
			return -EINVAL;
		}

		if ((ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc ==
				V4L2_PIX_FMT_H264) &&
			(ctx->enc_params.seq_hdr_mode !=
			V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE)) {
			ret = venc_if_set_param(ctx,
						VENC_SET_PARAM_PREPEND_HEADER,
						0);
			if (ret) {
					v4l2_err(v4l2_dev,
						 "venc_if_set_param failed=%d\n",
						 ret);
			ctx->state = MTK_STATE_ABORT;
					return -EINVAL;
				}
			ctx->state = MTK_STATE_HEADER;
	}

#if MTK_V4L2_BENCHMARK
	do_gettimeofday(&end);
	ctx->total_enc_dec_init_time =
		((end.tv_sec - begin.tv_sec) * 1000000 +
			end.tv_usec - begin.tv_usec);

#endif

	return 0;
}

static void vb2ops_venc_stop_streaming(struct vb2_queue *q)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct v4l2_device *v4l2_dev = &ctx->dev->v4l2_dev;
	struct vb2_buffer *src_buf, *dst_buf;
	int ret;

	mtk_v4l2_debug(2, "[%d]-> type=%d", ctx->idx, q->type);

	if ((ctx->state == MTK_STATE_HEADER) ||
		(ctx->state == MTK_STATE_INIT)) {
		ctx->state = MTK_STATE_ABORT;
		queue_work(ctx->dev->encode_workqueue, &ctx->encode_work);
		ret = mtk_vcodec_wait_for_done_ctx(ctx,
					   MTK_INST_WORK_THREAD_ABORT_DONE,
					   WAIT_INTR_TIMEOUT, true);
	}

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		while ((dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx))) {
			dst_buf->v4l2_planes[0].bytesused = 0;
			v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_DONE);
		}
	} else {
		while ((src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
			v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
	}

	if ((q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	     vb2_is_streaming(&ctx->m2m_ctx->out_q_ctx.q)) ||
	     (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
	     vb2_is_streaming(&ctx->m2m_ctx->cap_q_ctx.q))) {
		mtk_v4l2_debug(1, "[%d]-> q type %d out=%d cap=%d",
				 ctx->idx, q->type,
				 vb2_is_streaming(&ctx->m2m_ctx->out_q_ctx.q),
				 vb2_is_streaming(&ctx->m2m_ctx->cap_q_ctx.q));
		return;
	}

	ret = venc_if_release(ctx);
	if (ret)
			v4l2_err(v4l2_dev, "venc_if_release failed=%d\n", ret);

	ctx->state = MTK_STATE_FREE;
}

static struct vb2_ops mtk_venc_vb2_ops = {
	.queue_setup			= vb2ops_venc_queue_setup,
	.buf_prepare			= vb2ops_venc_buf_prepare,
	.buf_queue			= vb2ops_venc_buf_queue,
	.wait_prepare			= vb2_ops_wait_prepare,
	.wait_finish			= vb2_ops_wait_finish,
	.start_streaming		= vb2ops_venc_start_streaming,
	.stop_streaming			= vb2ops_venc_stop_streaming,
};

static int mtk_venc_encode_header(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;
	struct v4l2_device *v4l2_dev = &ctx->dev->v4l2_dev;
	int ret;
	struct vb2_buffer *dst_buf;
	struct mtk_vcodec_mem bs_buf;
	struct venc_done_result enc_result;

	dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	if (!dst_buf) {
		mtk_v4l2_debug(1, "No dst buffer");
		return -EINVAL;
	}

	bs_buf.va = vb2_plane_vaddr(dst_buf, 0);
	bs_buf.dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	bs_buf.size = (unsigned int)dst_buf->v4l2_planes[0].length;

	mtk_v4l2_debug(1,
			"[%d] buf idx=%d va=0x%p dma_addr=0x%llx size=0x%zx",
			ctx->idx,
			dst_buf->v4l2_buf.index,
			bs_buf.va,
			(u64)bs_buf.dma_addr,
			bs_buf.size);

	ret = venc_if_encode(ctx,
			     VENC_START_OPT_ENCODE_SEQUENCE_HEADER,
			     0, &bs_buf, &enc_result);

	if (ret) {
		dst_buf->v4l2_planes[0].bytesused = 0;
		ctx->state = MTK_STATE_ABORT;
		v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_ERROR);
		v4l2_err(v4l2_dev, "venc_if_encode failed=%d", ret);
		return -EINVAL;
	}

	ctx->state = MTK_STATE_HEADER;
	dst_buf->v4l2_planes[0].bytesused = enc_result.bs_size;

#if defined(DEBUG)
{
	int i;

	mtk_v4l2_debug(1, "[%d] venc_if_encode header len=%d",
			ctx->idx,
			 enc_result.bs_size);
	for (i = 0; i < enc_result.bs_size; i++) {
		unsigned char *p = (unsigned char *)bs_buf.va;

		mtk_v4l2_debug(1, "[%d] buf[%d]=0x%2x",
				ctx->idx, i, p[i]);
	}
}
#endif
	v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_DONE);

	return 0;
}

static int mtk_venc_param_change(struct mtk_vcodec_ctx *ctx, void *priv)
{
	struct vb2_buffer *vb = priv;
	struct mtk_video_enc_buf *buf =
			container_of(vb, struct mtk_video_enc_buf, b);
	int ret = 0;

	if (buf->param_change == MTK_ENCODE_PARAM_NONE)
		return 0;

	mtk_v4l2_debug(1, "encode parameters change id=%d", vb->v4l2_buf.index);
	if (buf->param_change & MTK_ENCODE_PARAM_BITRATE) {
		struct venc_enc_prm enc_prm;

		enc_prm.bitrate = buf->enc_params.bitrate;
		mtk_v4l2_debug(1, "change param br=%d",
				 enc_prm.bitrate);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_ADJUST_BITRATE,
					 &enc_prm);
	}
	if (buf->param_change & MTK_ENCODE_PARAM_FRAMERATE) {
		struct venc_enc_prm enc_prm;

		enc_prm.frm_rate = buf->enc_params.framerate_num /
				   buf->enc_params.framerate_denom;
		mtk_v4l2_debug(1, "change param fr=%d",
				 enc_prm.frm_rate);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_ADJUST_FRAMERATE,
					 &enc_prm);
	}
	if (buf->param_change & MTK_ENCODE_PARAM_INTRA_PERIOD) {
		mtk_v4l2_debug(1, "change param intra period=%d",
				 buf->enc_params.gop_size);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_I_FRAME_INTERVAL,
					 &buf->enc_params.gop_size);
	}
	if (buf->param_change & MTK_ENCODE_PARAM_FRAME_TYPE) {
		mtk_v4l2_debug(1, "change param force I=%d",
				 buf->enc_params.force_intra);
		if (buf->enc_params.force_intra)
			ret |= venc_if_set_param(ctx,
						 VENC_SET_PARAM_FORCE_INTRA,
						 0);
	}
	if (buf->param_change & MTK_ENCODE_PARAM_SKIP_FRAME) {
		mtk_v4l2_debug(1, "change param skip frame=%d",
				 buf->enc_params.skip_frame);
		if (buf->enc_params.skip_frame)
			ret |= venc_if_set_param(ctx,
						 VENC_SET_PARAM_SKIP_FRAME,
						 0);
	}
	buf->param_change = MTK_ENCODE_PARAM_NONE;

	if (ret) {
		ctx->state = MTK_STATE_ABORT;
		mtk_v4l2_err("venc_if_set_param %d failed=%d\n",
			MTK_ENCODE_PARAM_FRAME_TYPE, ret);
		return -1;
	}

	return 0;
}

static void mtk_venc_worker(struct work_struct *work)
{
	struct mtk_vcodec_ctx *ctx = container_of(work, struct mtk_vcodec_ctx,
				    encode_work);
	struct vb2_buffer *src_buf, *dst_buf;
	struct venc_frm_buf frm_buf;
	struct mtk_vcodec_mem bs_buf;
	struct venc_done_result enc_result;
	int ret;
#if MTK_V4L2_BENCHMARK
	struct timeval begin, end;
	struct timeval begin1, end1;

	do_gettimeofday(&begin);
#endif

	mutex_lock(&ctx->dev->dev_mutex);

	if (ctx->state == MTK_STATE_ABORT) {
		v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);
		mtk_v4l2_debug(0, "[%d] [MTK_INST_ABORT]", ctx->idx);
		ctx->int_cond = 1;
		ctx->int_type = MTK_INST_WORK_THREAD_ABORT_DONE;
		wake_up_interruptible(&ctx->queue);
		mutex_unlock(&ctx->dev->dev_mutex);
		return;
	}

	if ((ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc ==
				V4L2_PIX_FMT_H264) &&
		(ctx->state != MTK_STATE_HEADER)) {
		/* encode h264 sps/pps header */
#if MTK_V4L2_BENCHMARK
		do_gettimeofday(&begin1);
#endif
		mtk_venc_encode_header(ctx);
#if MTK_V4L2_BENCHMARK
		do_gettimeofday(&end1);
		ctx->total_enc_hdr_time +=
			((end1.tv_sec - begin1.tv_sec) * 1000000 +
				end1.tv_usec - begin1.tv_usec);
#endif

		v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);
		mutex_unlock(&ctx->dev->dev_mutex);
		return;
	}

	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (!src_buf) {
		mutex_unlock(&ctx->dev->dev_mutex);
		return;
	}

	dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	if (!dst_buf) {
		mutex_unlock(&ctx->dev->dev_mutex);
		return;
	}

	mtk_venc_param_change(ctx, src_buf);

	frm_buf.fb_addr.va = vb2_plane_vaddr(src_buf, 0);
	frm_buf.fb_addr.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 0);
	frm_buf.fb_addr.size = (unsigned int)src_buf->v4l2_planes[0].length;
	frm_buf.fb_addr1.va = vb2_plane_vaddr(src_buf, 1);
	frm_buf.fb_addr1.dma_addr = vb2_dma_contig_plane_dma_addr(src_buf, 1);
	frm_buf.fb_addr1.size = (unsigned int)src_buf->v4l2_planes[1].length;
	if (src_buf->num_planes == 3) {
		frm_buf.fb_addr2.va = vb2_plane_vaddr(src_buf, 2);
		frm_buf.fb_addr2.dma_addr =
			vb2_dma_contig_plane_dma_addr(src_buf, 2);
		frm_buf.fb_addr2.size =
			(unsigned int)src_buf->v4l2_planes[2].length;
	} else {
		frm_buf.fb_addr2.va = NULL;
		frm_buf.fb_addr2.dma_addr = 0;
		frm_buf.fb_addr2.size = 0;
	}
	bs_buf.va = vb2_plane_vaddr(dst_buf, 0);
	bs_buf.dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	bs_buf.size = (unsigned int)dst_buf->v4l2_planes[0].length;

	mtk_v4l2_debug(2,
			"Framebuf VA=%p PA=%llx Size=0x%zx;VA=%p PA=0x%llx Size=0x%zx;VA=%p PA=0x%llx Size=0x%zx",
			 frm_buf.fb_addr.va,
			 (u64)frm_buf.fb_addr.dma_addr,
			 frm_buf.fb_addr.size,
			 frm_buf.fb_addr1.va,
			 (u64)frm_buf.fb_addr1.dma_addr,
			 frm_buf.fb_addr1.size,
			 frm_buf.fb_addr2.va,
			 (u64)frm_buf.fb_addr2.dma_addr,
			 frm_buf.fb_addr2.size);

	ret = venc_if_encode(ctx, VENC_START_OPT_ENCODE_FRAME,
			     &frm_buf, &bs_buf, &enc_result);

		src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	if (enc_result.msg == VENC_MESSAGE_OK) {
		v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_DONE);
		dst_buf->v4l2_planes[0].bytesused = enc_result.bs_size;
	} else {
		v4l2_m2m_buf_done(src_buf, VB2_BUF_STATE_ERROR);
		dst_buf->v4l2_planes[0].bytesused = 0;
	}

	if (enc_result.is_key_frm)
		dst_buf->v4l2_buf.flags |= V4L2_BUF_FLAG_KEYFRAME;

	if (ret) {
		ctx->state = MTK_STATE_ABORT;
		v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_ERROR);
		mtk_v4l2_err("venc_if_encode failed=%d", ret);
	} else {
		v4l2_m2m_buf_done(dst_buf, VB2_BUF_STATE_DONE);
		mtk_v4l2_debug(2, "venc_if_encode bs size=%d",
				 enc_result.bs_size);
	}

#if MTK_V4L2_BENCHMARK
	do_gettimeofday(&end);
	ctx->total_enc_dec_cnt++;
	ctx->total_enc_dec_time +=
		((end.tv_sec - begin.tv_sec) * 1000000 +
			end.tv_usec - begin.tv_usec);
#endif

	v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);

	mtk_v4l2_debug(1, "<=== src_buf[%d] dst_buf[%d] venc_if_encode ret=%d Size=%u===>",
			src_buf->v4l2_buf.index, dst_buf->v4l2_buf.index, ret,
			enc_result.bs_size);
	mutex_unlock(&ctx->dev->dev_mutex);

}

static void m2mops_venc_device_run(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;

	queue_work(ctx->dev->encode_workqueue, &ctx->encode_work);
}

static int m2mops_venc_job_ready(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	if (!v4l2_m2m_num_dst_bufs_ready(ctx->m2m_ctx)) {
		mtk_v4l2_debug(3,
				 "[%d]Not ready: not enough video dst buffers.",
				 ctx->idx);
		return 0;
	}

	if (!v4l2_m2m_num_src_bufs_ready(ctx->m2m_ctx)) {
			mtk_v4l2_debug(3,
					 "[%d]Not ready: not enough video src buffers.",
					 ctx->idx);
			return 0;
		}

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_debug(3,
				 "[%d]Not ready: state=0x%x.",
				 ctx->idx, ctx->state);
		return 0;
	}

	if (ctx->state == MTK_STATE_FREE) {
		mtk_v4l2_debug(3,
				 "[%d]Not ready: state=0x%x.",
				 ctx->idx, ctx->state);
		return 0;
	}

	mtk_v4l2_debug(3, "[%d]ready!", ctx->idx);

	return 1;
}

static void m2mops_venc_job_abort(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;

	mtk_v4l2_debug(3, "[%d]type=%d", ctx->idx, ctx->type);
	ctx->state = MTK_STATE_ABORT;

	v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);
}

static void m2mops_venc_lock(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mutex_lock(&ctx->dev->dev_mutex);
}

static void m2mops_venc_unlock(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	mutex_unlock(&ctx->dev->dev_mutex);
}

const struct v4l2_m2m_ops mtk_venc_m2m_ops = {
	.device_run			= m2mops_venc_device_run,
	.job_ready			= m2mops_venc_job_ready,
	.job_abort			= m2mops_venc_job_abort,
	.lock				= m2mops_venc_lock,
	.unlock				= m2mops_venc_unlock,
};

#define IS_MTK_VENC_PRIV(x) ((V4L2_CTRL_ID2CLASS(x) == V4L2_CTRL_CLASS_MPEG) &&\
			     V4L2_CTRL_DRIVER_PRIV(x))

static const char *const *mtk_vcodec_enc_get_menu(u32 id)
{
	static const char *const mtk_vcodec_enc_video_frame_skip[] = {
		"Disabled",
		"Level Limit",
		"VBV/CPB Limit",
		NULL,
	};
	static const char *const mtk_vcodec_enc_video_force_frame[] = {
		"Disabled",
		"I Frame",
		"Not Coded",
		NULL,
	};
	switch (id) {
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE:
		return mtk_vcodec_enc_video_frame_skip;
	case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
		return mtk_vcodec_enc_video_force_frame;
	}
	return NULL;
}

int mtk_venc_ctrls_setup(struct mtk_vcodec_ctx *ctx)
{
	struct v4l2_ctrl_config cfg;
	int i;

	v4l2_ctrl_handler_init(&ctx->ctrl_hdl, NUM_CTRLS);
	if (ctx->ctrl_hdl.error) {
		v4l2_err(&ctx->dev->v4l2_dev, "Init control handler fail %d\n",
			 ctx->ctrl_hdl.error);
		return ctx->ctrl_hdl.error;
	}
	for (i = 0; i < NUM_CTRLS; i++) {
		if (IS_MTK_VENC_PRIV(controls[i].id)) {
			memset(&cfg, 0, sizeof(struct v4l2_ctrl_config));
			cfg.ops = &mtk_vcodec_enc_ctrl_ops;
			cfg.id = controls[i].id;
			cfg.min = controls[i].minimum;
			cfg.max = controls[i].maximum;
			cfg.def = controls[i].default_value;
			cfg.name = controls[i].name;
			cfg.type = controls[i].type;
			cfg.flags = 0;
			if (cfg.type == V4L2_CTRL_TYPE_MENU) {
				cfg.step = 0;
				cfg.menu_skip_mask = cfg.menu_skip_mask;
				cfg.qmenu = mtk_vcodec_enc_get_menu(cfg.id);
			} else {
				cfg.step = controls[i].step;
				cfg.menu_skip_mask = 0;
			}
			v4l2_ctrl_new_custom(&ctx->ctrl_hdl, &cfg, NULL);
		} else {
			if ((controls[i].type == V4L2_CTRL_TYPE_MENU) ||
			    (controls[i].type == V4L2_CTRL_TYPE_INTEGER_MENU)) {
				v4l2_ctrl_new_std_menu(
					&ctx->ctrl_hdl,
					&mtk_vcodec_enc_ctrl_ops,
					controls[i].id,
					controls[i].maximum, 0,
					controls[i].default_value);
			} else {
				v4l2_ctrl_new_std(
					&ctx->ctrl_hdl,
					&mtk_vcodec_enc_ctrl_ops,
					controls[i].id,
					controls[i].minimum,
					controls[i].maximum,
					controls[i].step,
					controls[i].default_value);
			}
		}

		if (ctx->ctrl_hdl.error) {
			v4l2_err(&ctx->dev->v4l2_dev,
				 "Adding control (%d) failed %d\n",
				 i, ctx->ctrl_hdl.error);
			return ctx->ctrl_hdl.error;
		}
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);
	return 0;
}

void mtk_venc_ctrls_free(struct mtk_vcodec_ctx *ctx)
{
	v4l2_ctrl_handler_free(&ctx->ctrl_hdl);
}

int m2mctx_venc_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq)
{
	struct mtk_vcodec_ctx *ctx = priv;
	int ret;

	src_vq->type		= V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	src_vq->drv_priv	= ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_video_enc_buf);
	src_vq->ops		= &mtk_venc_vb2_ops;
	src_vq->mem_ops		= &vb2_dma_contig_memops;
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock = &ctx->vb2_mutex;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes	= VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	dst_vq->drv_priv	= ctx;
	dst_vq->buf_struct_size = sizeof(struct v4l2_m2m_buffer);
	dst_vq->ops		= &mtk_venc_vb2_ops;
	dst_vq->mem_ops		= &vb2_dma_contig_memops;
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock = &ctx->vb2_mutex;

	return vb2_queue_init(dst_vq);
}

int mtk_venc_unlock(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = ctx->dev;

	dev->curr_ctx = -1;
	mutex_unlock(&dev->enc_mutex);
	return 0;
}

int mtk_venc_lock(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = ctx->dev;

	mutex_lock(&dev->enc_mutex);
	dev->curr_ctx = ctx->idx;
	return 0;
}

void mtk_vcodec_venc_release(struct mtk_vcodec_ctx *ctx)
{
	venc_if_release(ctx);

#if MTK_V4L2_BENCHMARK
	mtk_v4l2_debug(0, "\n\nMTK_V4L2_BENCHMARK");

	mtk_v4l2_debug(0, "  total_enc_dec_cnt: %d ", ctx->total_enc_dec_cnt);
	mtk_v4l2_debug(0, "  total_enc_dec_time: %d us",
				ctx->total_enc_dec_time);
	mtk_v4l2_debug(0, "  total_enc_dec_init_time: %d us",
				ctx->total_enc_dec_init_time);
	mtk_v4l2_debug(0, "  total_enc_hdr_time: %d us",
				ctx->total_enc_hdr_time);
	mtk_v4l2_debug(0, "  total_qbuf_out_time: %d us",
				ctx->total_qbuf_out_time);
	mtk_v4l2_debug(0, "  total_qbuf_out_cnt: %d ",
				ctx->total_qbuf_out_cnt);
	mtk_v4l2_debug(0, "  total_qbuf_cap_time: %d us",
				ctx->total_qbuf_cap_time);
	mtk_v4l2_debug(0, "  total_qbuf_cap_cnt: %d ",
				ctx->total_qbuf_cap_cnt);

	mtk_v4l2_debug(0, "  total_dqbuf_out_time: %d us",
				ctx->total_dqbuf_out_time);
	mtk_v4l2_debug(0, "  total_dqbuf_out_cnt: %d ",
				ctx->total_dqbuf_out_cnt);
	mtk_v4l2_debug(0, "  total_dqbuf_cap_time: %d us",
				ctx->total_dqbuf_cap_time);
	mtk_v4l2_debug(0, "  total_dqbuf_cap_cnt: %d ",
				ctx->total_dqbuf_cap_cnt);

	mtk_v4l2_debug(0, "  total_expbuf_time: %d us",
				ctx->total_expbuf_time);

#endif

}
