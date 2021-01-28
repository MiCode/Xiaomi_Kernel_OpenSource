// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <media/v4l2-event.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-dma-contig.h>
#include <soc/mediatek/smi.h>
#include <linux/delay.h>
#include <linux/semaphore.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_enc.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcodec_enc_pm.h"
#include "venc_drv_if.h"

#define MTK_VENC_MIN_W  160U
#define MTK_VENC_MIN_H  128U
#define MTK_VENC_MAX_W  1920U
#define MTK_VENC_MAX_H  1088U
#define DFT_CFG_WIDTH   MTK_VENC_MIN_W
#define DFT_CFG_HEIGHT  MTK_VENC_MIN_H

static void mtk_venc_worker(struct work_struct *work);
static struct mtk_video_fmt
	mtk_video_formats[MTK_MAX_ENC_CODECS_SUPPORT] = { {0} };
static struct mtk_codec_framesizes
	mtk_venc_framesizes[MTK_MAX_ENC_CODECS_SUPPORT] = { {0} };
static unsigned int default_out_fmt_idx;
static unsigned int default_cap_fmt_idx;
#ifdef CONFIG_VB2_MEDIATEK_DMA
static struct vb2_mem_ops venc_ion_dma_contig_memops;
#endif

inline unsigned int log2_enc(__u32 value)
{
	unsigned int x = 0;

	while (value > 1) {
		value >>= 1;
		x++;
	}
	return x;
}

static void get_supported_format(struct mtk_vcodec_ctx *ctx)
{
	unsigned int i;

	if (mtk_video_formats[0].fourcc == 0) {
		if (venc_if_get_param(ctx,
			GET_PARAM_CAPABILITY_SUPPORTED_FORMATS,
			&mtk_video_formats) != 0) {
			mtk_v4l2_err("Error!! Cannot get supported format");
			return;
		}
		for (i = 0; i < MTK_MAX_ENC_CODECS_SUPPORT; i++) {
			if (mtk_video_formats[i].fourcc != 0 &&
			    mtk_video_formats[i].type == MTK_FMT_FRAME) {
				default_out_fmt_idx = i;
				break;
			}
		}
		for (i = 0; i < MTK_MAX_ENC_CODECS_SUPPORT; i++) {
			if (mtk_video_formats[i].fourcc != 0 &&
			    mtk_video_formats[i].type == MTK_FMT_ENC) {
				default_cap_fmt_idx = i;
				break;
			}
		}
	}
}

static void get_supported_framesizes(struct mtk_vcodec_ctx *ctx)
{
	unsigned int i;

	if (mtk_venc_framesizes[0].fourcc == 0) {
		if (venc_if_get_param(ctx, GET_PARAM_CAPABILITY_FRAME_SIZES,
				      &mtk_venc_framesizes) != 0) {
			mtk_v4l2_err("[%d] Error!! Cannot get frame size",
				ctx->id);
			return;
		}

		for (i = 0; i < MTK_MAX_ENC_CODECS_SUPPORT; i++) {
			if (mtk_venc_framesizes[i].fourcc != 0) {
				mtk_v4l2_debug(1,
				"venc_fs[%d] fourcc %d s %d %d %d %d %d %d\n",
				i, mtk_venc_framesizes[i].fourcc,
				mtk_venc_framesizes[i].stepwise.min_width,
				mtk_venc_framesizes[i].stepwise.max_width,
				mtk_venc_framesizes[i].stepwise.step_width,
				mtk_venc_framesizes[i].stepwise.min_height,
				mtk_venc_framesizes[i].stepwise.max_height,
				mtk_venc_framesizes[i].stepwise.step_height);
			}
		}
	}
}

static void get_free_buffers(struct mtk_vcodec_ctx *ctx,
				struct venc_done_result *pResult)
{
	venc_if_get_param(ctx,
		GET_PARAM_FREE_BUFFERS,
		pResult);
}

void mtk_enc_put_buf(struct mtk_vcodec_ctx *ctx)
{
	struct venc_done_result rResult;
	struct venc_frm_buf *pfrm;
	struct mtk_vcodec_mem *pbs;
	struct mtk_video_enc_buf *bs_info, *frm_info;
	struct vb2_v4l2_buffer *dst_vb2_v4l2, *src_vb2_v4l2;
	struct vb2_buffer *dst_buf;

	mutex_lock(&ctx->buf_lock);
	do {
		dst_vb2_v4l2 = NULL;
		src_vb2_v4l2 = NULL;
		pfrm = NULL;
		pbs = NULL;

		memset(&rResult, 0, sizeof(rResult));
		get_free_buffers(ctx, &rResult);

		if (rResult.bs_va != 0 && virt_addr_valid(rResult.bs_va)) {
			pbs = (struct mtk_vcodec_mem *)rResult.bs_va;
			bs_info = container_of(pbs,
				struct mtk_video_enc_buf, bs_buf);
			dst_vb2_v4l2 = &bs_info->vb;
		}

		if (rResult.frm_va != 0 && virt_addr_valid(rResult.frm_va)) {
			pfrm = (struct venc_frm_buf *)rResult.frm_va;
			frm_info = container_of(pfrm,
				struct mtk_video_enc_buf, frm_buf);
			src_vb2_v4l2 = &frm_info->vb;
		}

		if (src_vb2_v4l2 != NULL && dst_vb2_v4l2 != NULL) {
			if (rResult.is_key_frm)
				dst_vb2_v4l2->flags |= V4L2_BUF_FLAG_KEYFRAME;

			dst_vb2_v4l2->vb2_buf.timestamp =
				src_vb2_v4l2->vb2_buf.timestamp;
			dst_vb2_v4l2->timecode = src_vb2_v4l2->timecode;
			dst_vb2_v4l2->flags |= src_vb2_v4l2->flags;
			dst_vb2_v4l2->sequence = src_vb2_v4l2->sequence;
			dst_buf = &dst_vb2_v4l2->vb2_buf;
			dst_buf->planes[0].bytesused = rResult.bs_size;
			v4l2_m2m_buf_done(src_vb2_v4l2, VB2_BUF_STATE_DONE);
			v4l2_m2m_buf_done(dst_vb2_v4l2, VB2_BUF_STATE_DONE);

			mtk_v4l2_debug(1, "venc_if_encode bs size=%d",
				rResult.bs_size);
		} else if (src_vb2_v4l2 == NULL && dst_vb2_v4l2 != NULL) {
			dst_buf = &dst_vb2_v4l2->vb2_buf;
			dst_buf->planes[0].bytesused = rResult.bs_size;
			v4l2_m2m_buf_done(dst_vb2_v4l2,
					VB2_BUF_STATE_DONE);
			mtk_v4l2_debug(0, "[Warning] bs size=%d, frm NULL!!",
				rResult.bs_size);
		} else {
			if (src_vb2_v4l2 == NULL)
				mtk_v4l2_debug(1, "NULL enc src buffer\n");

			if (dst_vb2_v4l2 == NULL)
				mtk_v4l2_debug(1, "NULL enc dst buffer\n");
		}
	} while (rResult.bs_va != 0 || rResult.frm_va != 0);
	mutex_unlock(&ctx->buf_lock);
}

static struct mtk_video_fmt *mtk_venc_find_format(struct v4l2_format *f,
						  unsigned int t)
{
	struct mtk_video_fmt *fmt;
	unsigned int k;

	mtk_v4l2_debug(3, "fourcc %d", f->fmt.pix_mp.pixelformat);
	for (k = 0; k < MTK_MAX_ENC_CODECS_SUPPORT &&
	     mtk_video_formats[k].fourcc != 0; k++) {
		fmt = &mtk_video_formats[k];
		if (fmt->fourcc == f->fmt.pix.pixelformat && fmt->type == t)
			return fmt;
	}

	return NULL;
}

static int vidioc_venc_check_supported_profile_level(__u32 fourcc,
	unsigned int pl, bool is_profile)
{
	struct v4l2_format f;
	int i = 0;

	f.fmt.pix.pixelformat = fourcc;
	if (mtk_venc_find_format(&f, MTK_FMT_ENC) == NULL)
		return false;

	for (i = 0; i < MTK_MAX_ENC_CODECS_SUPPORT; i++) {
		if (mtk_venc_framesizes[i].fourcc == fourcc) {
			if (is_profile) {
				if (mtk_venc_framesizes[i].profile & (1 << pl))
					return true;
				else
					return false;
			} else {
				if (mtk_venc_framesizes[i].level >= pl)
					return true;
				else
					return false;
			}
		}
	}
	return false;
}

static int vidioc_venc_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	struct mtk_enc_params *p = &ctx->enc_params;
	int ret = 0;

	mtk_v4l2_debug(4, "[%d] id %d val %d array[0] %d array[1] %d",
				   ctx->id, ctrl->id, ctrl->val,
				   ctrl->p_new.p_u32[0], ctrl->p_new.p_u32[1]);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_BITRATE val = %d",
			       ctrl->val);
		p->bitrate = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_BITRATE;
		break;
	case V4L2_CID_MPEG_MTK_SEC_ENCODE:
		p->svp_mode = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_SEC_ENCODE;
		mtk_v4l2_debug(0, "[%d] V4L2_CID_MPEG_MTK_SEC_ENCODE id %d val %d array[0] %d array[1] %d",
			ctx->id, ctrl->id, ctrl->val,
		ctrl->p_new.p_u32[0], ctrl->p_new.p_u32[1]);
		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_B_FRAMES val = %d",
			       ctrl->val);
		p->num_b_frame = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE val = %d",
			       ctrl->val);
		p->rc_frame = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_H264_MAX_QP val = %d",
			       ctrl->val);
		p->h264_max_qp = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_HEADER_MODE val = %d",
			       ctrl->val);
		p->seq_hdr_mode = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE val = %d",
			       ctrl->val);
		p->rc_mb = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_H264_PROFILE val = %d",
			       ctrl->val);
		if (!vidioc_venc_check_supported_profile_level(
				V4L2_PIX_FMT_H264, ctrl->val, 1))
			return -EINVAL;
		p->profile = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_HEVC_PROFILE val = %d",
			       ctrl->val);
		if (!vidioc_venc_check_supported_profile_level(
				V4L2_PIX_FMT_H265, ctrl->val, 1))
			return -EINVAL;
		p->profile = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE val = %d",
			       ctrl->val);
		if (!vidioc_venc_check_supported_profile_level(
			    V4L2_PIX_FMT_MPEG4, ctrl->val, 1))
			return -EINVAL;
		p->profile = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_H264_LEVEL val = %d",
			       ctrl->val);
		if (!vidioc_venc_check_supported_profile_level(
				V4L2_PIX_FMT_H264, ctrl->val, 0))
			return -EINVAL;
		p->level = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_HEVC_LEVEL val = %d",
			       ctrl->val);
		if (!vidioc_venc_check_supported_profile_level(
				V4L2_PIX_FMT_H265, ctrl->val, 0))
			return -EINVAL;
		p->level = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_HEVC_TIER val = %d",
			ctrl->val);
		p->tier = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL val = %d",
			       ctrl->val);
		if (!vidioc_venc_check_supported_profile_level(
			    V4L2_PIX_FMT_MPEG4, ctrl->val, 0))
			return -EINVAL;
		p->level = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_H264_I_PERIOD val = %d",
			       ctrl->val);
		p->intra_period = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_INTRA_PERIOD;
		break;
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_GOP_SIZE val = %d",
			       ctrl->val);
		p->gop_size = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_GOP_SIZE;
		break;
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
		mtk_v4l2_debug(2, "V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME");
		p->force_intra = 1;
		ctx->param_change |= MTK_ENCODE_PARAM_FORCE_INTRA;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_SCENARIO:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_SCENARIO: %d",
			ctrl->val);
		p->scenario = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_SCENARIO;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_NONREFP:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_NONREFP: %d",
			ctrl->val);
		p->nonrefp = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_NONREFP;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE: %d",
			ctrl->val);
		p->detectframerate = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_DETECTED_FRAMERATE;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_RFS_ON:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_RFS_ON: %d",
			ctrl->val);
		p->rfs = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_RFS_ON;
		break;
	case V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR: %d",
			ctrl->val);
		p->prependheader = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_PREPEND_SPSPPS_TO_IDR;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE: %d",
			ctrl->val);
		p->operationrate = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_OPERATION_RATE;
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_VIDEO_BITRATE_MODE: %d",
			ctrl->val);
		p->bitratemode = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_BITRATE_MODE;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_ROI_ON:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_ROI_ON: %d",
			ctrl->val);
		p->roion = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_ROI_ON;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE: %d",
			ctrl->val);
		p->heif_grid_size = ctrl->val;
		ctx->param_change |= MTK_ENCODE_PARAM_GRID_SIZE;
		break;
	case V4L2_CID_MPEG_MTK_COLOR_DESC:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_COLOR_DESC: 0x%x",
			ctrl->val);
		memcpy(&p->color_desc, ctrl->p_new.p_u32,
		sizeof(struct mtk_color_desc));
		ctx->param_change |= MTK_ENCODE_PARAM_COLOR_DESC;
		break;
	case V4L2_CID_MPEG_MTK_MAX_WIDTH:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_MAX_WIDTH: %d",
			ctrl->val);
		p->max_w = ctrl->val;
		break;
	case V4L2_CID_MPEG_MTK_MAX_HEIGHT:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_MAX_HEIGHT: %d",
			ctrl->val);
		p->max_h = ctrl->val;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP val = %d",
			ctrl->val);
		p->i_qp = ctrl->val;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP val = %d",
			ctrl->val);
		p->p_qp = ctrl->val;
		break;
	case V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP:
		mtk_v4l2_debug(2,
			"V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP val = %d",
			ctrl->val);
		p->b_qp = ctrl->val;
		break;
	default:
		mtk_v4l2_err("ctrl-id=%d not support!", ctrl->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int vidioc_venc_g_ctrl(struct v4l2_ctrl *ctrl)
{
	struct mtk_vcodec_ctx *ctx = ctrl_to_ctx(ctrl);
	int ret = 0;
	int value = 0;
	struct venc_resolution_change *reschange;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_MTK_ENCODE_ROI_RC_QP:
		venc_if_get_param(ctx,
			GET_PARAM_ROI_RC_QP,
			&value);
		ctrl->val = value;
		break;
	case V4L2_CID_MPEG_MTK_RESOLUTION_CHANGE:
		reschange = (struct venc_resolution_change *)ctrl->p_new.p_u32;
		venc_if_get_param(ctx,
			GET_PARAM_RESOLUTION_CHANGE,
			reschange);
		break;
	default:
		mtk_v4l2_err("ctrl-id=%d not support!", ctrl->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct v4l2_ctrl_ops mtk_vcodec_enc_ctrl_ops = {
	.s_ctrl = vidioc_venc_s_ctrl,
	.g_volatile_ctrl = vidioc_venc_g_ctrl,
};

static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool output_queue)
{
	struct mtk_video_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < MTK_MAX_ENC_CODECS_SUPPORT &&
	     mtk_video_formats[i].fourcc != 0; ++i) {
		if (output_queue && mtk_video_formats[i].type != MTK_FMT_FRAME)
			continue;
		if (!output_queue && mtk_video_formats[i].type != MTK_FMT_ENC)
			continue;

		if (j == f->index) {
			fmt = &mtk_video_formats[i];
			f->pixelformat = fmt->fourcc;
			memset(f->reserved, 0, sizeof(f->reserved));
			return 0;
		}
		++j;
	}

	return -EINVAL;
}

static int vidioc_enum_framesizes(struct file *file, void *fh,
				  struct v4l2_frmsizeenum *fsize)
{
	unsigned int i = 0;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(fh);

	if (fsize->index != 0)
		return -EINVAL;

	get_supported_framesizes(ctx);

	for (i = 0; i < MTK_MAX_ENC_CODECS_SUPPORT &&
	     mtk_venc_framesizes[i].fourcc != 0; ++i) {
		if (fsize->pixel_format != mtk_venc_framesizes[i].fourcc)
			continue;

		fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
		fsize->stepwise = mtk_venc_framesizes[i].stepwise;
		fsize->reserved[0] = mtk_venc_framesizes[i].profile;
		fsize->reserved[1] = mtk_venc_framesizes[i].level;
		return 0;
	}

	return -EINVAL;
}

static int vidioc_enum_fmt_vid_cap_mplane(struct file *file, void *priv,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false);
}

static int vidioc_enum_fmt_vid_out_mplane(struct file *file, void *priv,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true);
}

static int vidioc_venc_querycap(struct file *file, void *priv,
				struct v4l2_capability *cap)
{
	strlcpy(cap->driver, MTK_VCODEC_ENC_NAME, sizeof(cap->driver));
	strlcpy(cap->bus_info, MTK_PLATFORM_STR, sizeof(cap->bus_info));
	strlcpy(cap->card, MTK_PLATFORM_STR, sizeof(cap->card));

	cap->device_caps  = V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	return 0;
}

static int vidioc_venc_s_parm(struct file *file, void *priv,
			      struct v4l2_streamparm *a)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	ctx->enc_params.framerate_num =
		a->parm.output.timeperframe.denominator;
	ctx->enc_params.framerate_denom =
		a->parm.output.timeperframe.numerator;
	ctx->param_change |= MTK_ENCODE_PARAM_FRAMERATE;

	a->parm.output.capability = V4L2_CAP_TIMEPERFRAME;

	return 0;
}

static int vidioc_venc_g_parm(struct file *file, void *priv,
			      struct v4l2_streamparm *a)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (a->type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return -EINVAL;

	a->parm.output.capability = V4L2_CAP_TIMEPERFRAME;
	a->parm.output.timeperframe.denominator =
		ctx->enc_params.framerate_num;
	a->parm.output.timeperframe.numerator =
		ctx->enc_params.framerate_denom;

	return 0;
}

static struct mtk_q_data *mtk_venc_get_q_data(struct mtk_vcodec_ctx *ctx,
					      enum v4l2_buf_type type)
{
	if (V4L2_TYPE_IS_OUTPUT(type))
		return &ctx->q_data[MTK_Q_DATA_SRC];

	return &ctx->q_data[MTK_Q_DATA_DST];
}

/* V4L2 specification suggests the driver corrects the format struct if any of
 * the dimensions is unsupported
 */
static int vidioc_try_fmt(struct v4l2_format *f, struct mtk_video_fmt *fmt,
			  struct mtk_vcodec_ctx *ctx)
{
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	int org_w, org_h, i;
	int bitsPP = 8;  /* bits per pixel */
	__u32 bs_fourcc;
	unsigned int step_width_in_pixel;
	unsigned int step_height_in_pixel;
	unsigned int saligned;
	unsigned int imagePixels;
	// for AFBC
	unsigned int block_w = 16;
	unsigned int block_h = 16;
	unsigned int block_count;

	struct mtk_codec_framesizes *spec_size_info = NULL;

	pix_fmt_mp->field = V4L2_FIELD_NONE;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pix_fmt_mp->num_planes = 1;
		pix_fmt_mp->plane_fmt[0].bytesperline = 0;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		get_supported_framesizes(ctx);
		if (ctx->q_data[MTK_Q_DATA_DST].fmt != NULL) {
			bs_fourcc =
				ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc;
		} else {
			bs_fourcc =
				mtk_video_formats[default_cap_fmt_idx].fourcc;
		}
		for (i = 0; i < MTK_MAX_ENC_CODECS_SUPPORT; i++) {
			if (mtk_venc_framesizes[i].fourcc == bs_fourcc)
				spec_size_info = &mtk_venc_framesizes[i];
		}
		if (!spec_size_info) {
			mtk_v4l2_err("fail to get spec_size_info");
			return -EINVAL;
		}

		mtk_v4l2_debug(1,
			       "pix_fmt_mp->pixelformat %d bs fmt %d min_w %d min_h %d max_w %d max_h %d\n",
			       pix_fmt_mp->pixelformat, bs_fourcc,
			       spec_size_info->stepwise.min_width,
			       spec_size_info->stepwise.min_height,
			       spec_size_info->stepwise.max_width,
			       spec_size_info->stepwise.max_height);

		if ((spec_size_info->stepwise.step_width &
		     (spec_size_info->stepwise.step_width - 1)) != 0)
			mtk_v4l2_err("Unsupport stepwise.step_width not 2^ %d\n",
				     spec_size_info->stepwise.step_width);
		if ((spec_size_info->stepwise.step_height &
		     (spec_size_info->stepwise.step_height - 1)) != 0)
			mtk_v4l2_err("Unsupport stepwise.step_height not 2^ %d\n",
				     spec_size_info->stepwise.step_height);

		if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_MT10 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_MT10S) {
			step_width_in_pixel =
				spec_size_info->stepwise.step_width * 4;
			step_height_in_pixel =
				spec_size_info->stepwise.step_height;
			bitsPP = 10;
			saligned = 6;
		} else if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_P010M ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_P010S) {
			step_width_in_pixel =
				spec_size_info->stepwise.step_width / 2;
			step_height_in_pixel =
				spec_size_info->stepwise.step_height;
			bitsPP = 16;
			saligned = 6;
		} else if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ABGR32 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ARGB32 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGB32 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGR32 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ARGB1010102 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ABGR1010102 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGBA1010102 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGRA1010102) {
			step_width_in_pixel = 1;
			step_height_in_pixel = 1;
			bitsPP = 32;
			saligned = 4;
		} else if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGB24 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGR24) {
			step_width_in_pixel = 1;
			step_height_in_pixel = 1;
			bitsPP = 24;
			saligned = 4;
		} else {
			step_width_in_pixel =
				spec_size_info->stepwise.step_width;
			step_height_in_pixel =
				spec_size_info->stepwise.step_height;
			bitsPP = 8;
			saligned = 6;
		}

		// Compute AFBC stream data size
		if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGB32_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGR32_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGBA1010102_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGRA1010102_AFBC) {
			step_width_in_pixel = 1;
			step_height_in_pixel = 1;
			block_w = 32;
			block_h = 8;
			bitsPP = 32;
			saligned = 4;
		} else if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_NV12_AFBC) {
			step_width_in_pixel = 1;
			step_height_in_pixel = 1;
			block_w = 16;
			block_h = 16;
			bitsPP = 12;
			saligned = 4;
		} else if (pix_fmt_mp->pixelformat ==
				V4L2_PIX_FMT_NV12_10B_AFBC) {
			step_width_in_pixel = 1;
			step_height_in_pixel = 1;
			block_w = 16;
			block_h = 16;
			bitsPP = 16;
			saligned = 4;
		}

		/* find next closer width stride align 16, height align 16,
		 * size align 64 rectangle without MBAFF encoder
		 * (with MBAFF height align should be 32)
		 * width height swappable
		 */

		if (pix_fmt_mp->height > pix_fmt_mp->width) {
			pix_fmt_mp->height = clamp(pix_fmt_mp->height,
				(spec_size_info->stepwise.min_height),
				(spec_size_info->stepwise.max_width));
			pix_fmt_mp->width = clamp(pix_fmt_mp->width,
				(spec_size_info->stepwise.min_width),
				(spec_size_info->stepwise.max_height));
			org_w = pix_fmt_mp->width;
			org_h = pix_fmt_mp->height;
			v4l_bound_align_image(&pix_fmt_mp->width,
				spec_size_info->stepwise.min_width,
				spec_size_info->stepwise.max_height,
				log2_enc(step_width_in_pixel),
				&pix_fmt_mp->height,
				spec_size_info->stepwise.min_height,
				spec_size_info->stepwise.max_width,
				log2_enc(step_height_in_pixel),
				saligned);

			if (pix_fmt_mp->width < org_w &&
			    (pix_fmt_mp->width +
			     step_width_in_pixel) <=
			    spec_size_info->stepwise.max_height)
				pix_fmt_mp->width +=
					step_width_in_pixel;
			if (pix_fmt_mp->height < org_h &&
			    (pix_fmt_mp->height +
			     step_height_in_pixel) <=
			    spec_size_info->stepwise.max_width)
				pix_fmt_mp->height +=
					step_height_in_pixel;
		} else {
			pix_fmt_mp->height = clamp(pix_fmt_mp->height,
				(spec_size_info->stepwise.min_height),
				(spec_size_info->stepwise.max_height));
			pix_fmt_mp->width = clamp(pix_fmt_mp->width,
				(spec_size_info->stepwise.min_width),
				(spec_size_info->stepwise.max_width));
			org_w = pix_fmt_mp->width;
			org_h = pix_fmt_mp->height;
			v4l_bound_align_image(&pix_fmt_mp->width,
				spec_size_info->stepwise.min_width,
				spec_size_info->stepwise.max_width,
				log2_enc(step_width_in_pixel),
				&pix_fmt_mp->height,
				spec_size_info->stepwise.min_height,
				spec_size_info->stepwise.max_height,
				log2_enc(step_height_in_pixel),
				saligned);

			if (pix_fmt_mp->width < org_w &&
			    (pix_fmt_mp->width +
			     step_width_in_pixel) <=
			    spec_size_info->stepwise.max_width)
				pix_fmt_mp->width +=
					step_width_in_pixel;
			if (pix_fmt_mp->height < org_h &&
			    (pix_fmt_mp->height +
			     step_height_in_pixel) <=
			    spec_size_info->stepwise.max_height)
				pix_fmt_mp->height +=
					step_height_in_pixel;
		}

		pix_fmt_mp->num_planes = fmt->num_planes;
		imagePixels = pix_fmt_mp->width * pix_fmt_mp->height;

		if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ABGR32 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ARGB32 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGB32 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGR32 ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ARGB1010102 ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_ABGR1010102 ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGBA1010102 ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGRA1010102 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGB24 ||
			pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGR24) {
			pix_fmt_mp->plane_fmt[0].sizeimage =
				imagePixels * bitsPP / 8;
			pix_fmt_mp->plane_fmt[0].bytesperline =
			pix_fmt_mp->width * bitsPP / 8;
			pix_fmt_mp->num_planes = 1U;
		} else if (pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGB32_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGR32_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_RGBA1010102_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_BGRA1010102_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_NV12_AFBC ||
		pix_fmt_mp->pixelformat == V4L2_PIX_FMT_NV12_10B_AFBC) {
			block_count =
			((pix_fmt_mp->width + (block_w - 1))/block_w)
			*((pix_fmt_mp->height + (block_h - 1))/block_h);

			pix_fmt_mp->plane_fmt[0].sizeimage =
			(block_count << 4) +
			(block_count * block_w * block_h * bitsPP / 8);
		mtk_v4l2_debug(0, "AFBC size:%d superblock(%dx%d) superblock_count(%d)\n",
		    pix_fmt_mp->plane_fmt[0].sizeimage,
		    block_w,
		    block_h,
		    block_count);
		} else if (pix_fmt_mp->num_planes == 1U) {
			pix_fmt_mp->plane_fmt[0].sizeimage =
				(imagePixels * bitsPP / 8) +
				(imagePixels * bitsPP / 8) / 2;
			pix_fmt_mp->plane_fmt[0].bytesperline =
				pix_fmt_mp->width * bitsPP / 8;
		} else if (pix_fmt_mp->num_planes == 2U) {
			pix_fmt_mp->plane_fmt[0].sizeimage =
				imagePixels * bitsPP / 8;
			pix_fmt_mp->plane_fmt[0].bytesperline =
				pix_fmt_mp->width * bitsPP / 8;
			pix_fmt_mp->plane_fmt[1].sizeimage =
				(imagePixels * bitsPP / 8) / 2;
			pix_fmt_mp->plane_fmt[1].bytesperline =
				pix_fmt_mp->width * bitsPP / 8;
		} else if (pix_fmt_mp->num_planes == 3U) {
			pix_fmt_mp->plane_fmt[0].sizeimage =
				imagePixels * bitsPP / 8;
			pix_fmt_mp->plane_fmt[0].bytesperline =
				pix_fmt_mp->width * bitsPP / 8;
			pix_fmt_mp->plane_fmt[1].sizeimage =
				(imagePixels * bitsPP / 8) / 4;
			pix_fmt_mp->plane_fmt[1].bytesperline =
				pix_fmt_mp->width * bitsPP / 8 / 2;
			pix_fmt_mp->plane_fmt[2].sizeimage =
				(imagePixels * bitsPP / 8) / 4;
			pix_fmt_mp->plane_fmt[2].bytesperline =
				pix_fmt_mp->width * bitsPP / 8 / 2;
		} else
			mtk_v4l2_err("Unsupport num planes = %d\n",
				     pix_fmt_mp->num_planes);

		mtk_v4l2_debug(0,
			       "w/h (%d, %d) -> (%d,%d), sizeimage[%d,%d,%d]",
			       org_w, org_h,
			       pix_fmt_mp->width, pix_fmt_mp->height,
			       pix_fmt_mp->plane_fmt[0].sizeimage,
			       pix_fmt_mp->plane_fmt[1].sizeimage,
			       pix_fmt_mp->plane_fmt[2].sizeimage);
	}

	for (i = 0; i < pix_fmt_mp->num_planes; i++)
		memset(&(pix_fmt_mp->plane_fmt[i].reserved[0]), 0x0,
		       sizeof(pix_fmt_mp->plane_fmt[0].reserved));

	pix_fmt_mp->flags = 0;
	memset(&pix_fmt_mp->reserved, 0x0,
	       sizeof(pix_fmt_mp->reserved));

	return 0;
}

static void mtk_venc_set_param(struct mtk_vcodec_ctx *ctx,
			       struct venc_enc_param *param)
{
	struct mtk_q_data *q_data_src = &ctx->q_data[MTK_Q_DATA_SRC];
	struct mtk_enc_params *enc_params = &ctx->enc_params;

	switch (q_data_src->fmt->fourcc) {
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV420:
		param->input_yuv_fmt = VENC_YUV_FORMAT_I420;
		break;
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YVU420:
		param->input_yuv_fmt = VENC_YUV_FORMAT_YV12;
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12:
		param->input_yuv_fmt = VENC_YUV_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV21:
		param->input_yuv_fmt = VENC_YUV_FORMAT_NV21;
		break;
	case V4L2_PIX_FMT_RGB24:
		param->input_yuv_fmt = VENC_YUV_FORMAT_24bitRGB888;
		break;
	case V4L2_PIX_FMT_BGR24:
		param->input_yuv_fmt = VENC_YUV_FORMAT_24bitBGR888;
		break;
	case V4L2_PIX_FMT_ARGB32:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitARGB8888;
		break;
	case V4L2_PIX_FMT_ABGR32:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitBGRA8888;
		break;
	case V4L2_PIX_FMT_BGR32:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitABGR8888;
		break;
	case V4L2_PIX_FMT_RGB32:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitRGBA8888;
		break;
	case V4L2_PIX_FMT_ARGB1010102:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitARGB1010102;
		break;
	case V4L2_PIX_FMT_ABGR1010102:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitABGR1010102;
		break;
	case V4L2_PIX_FMT_RGBA1010102:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitRGBA1010102;
		break;
	case V4L2_PIX_FMT_BGRA1010102:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitBGRA1010102;
		break;
	case V4L2_PIX_FMT_MT10:
	case V4L2_PIX_FMT_MT10S:
		param->input_yuv_fmt = VENC_YUV_FORMAT_MT10;
		break;
	case V4L2_PIX_FMT_P010M:
	case V4L2_PIX_FMT_P010S:
		param->input_yuv_fmt = VENC_YUV_FORMAT_P010;
		break;
	case V4L2_PIX_FMT_RGB32_AFBC:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitRGBA8888_AFBC;
		break;
	case V4L2_PIX_FMT_BGR32_AFBC:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitBGRA8888_AFBC;
		break;
	case V4L2_PIX_FMT_RGBA1010102_AFBC:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitRGBA1010102_AFBC;
		break;
	case V4L2_PIX_FMT_BGRA1010102_AFBC:
		param->input_yuv_fmt = VENC_YUV_FORMAT_32bitBGRA1010102_AFBC;
		break;
	case V4L2_PIX_FMT_NV12_AFBC:
		param->input_yuv_fmt = VENC_YUV_FORMAT_NV12_AFBC;
		break;
	case V4L2_PIX_FMT_NV12_10B_AFBC:
		param->input_yuv_fmt = VENC_YUV_FORMAT_NV12_10B_AFBC;
		break;

	default:
		mtk_v4l2_err("Unsupport fourcc =%d default use I420",
			q_data_src->fmt->fourcc);
		param->input_yuv_fmt = VENC_YUV_FORMAT_I420;
		break;
	}
	param->profile = enc_params->profile;
	param->level = enc_params->level;
	param->tier = enc_params->tier;

	/* Config visible resolution */
	param->width = q_data_src->visible_width;
	param->height = q_data_src->visible_height;
	/* Config coded resolution */
	param->buf_width = q_data_src->coded_width;
	param->buf_height = q_data_src->coded_height;
	param->frm_rate = enc_params->framerate_num /
			  enc_params->framerate_denom;
	param->intra_period = enc_params->intra_period;
	param->gop_size = enc_params->gop_size;
	param->bitrate = enc_params->bitrate;
	param->operationrate = enc_params->operationrate;
	param->scenario = enc_params->scenario;
	param->prependheader = enc_params->prependheader;
	param->bitratemode = enc_params->bitratemode;
	param->roion = enc_params->roion;
	param->heif_grid_size = enc_params->heif_grid_size;
	// will copy to vsi, pass after streamon
	param->color_desc = &enc_params->color_desc;
	param->max_w = enc_params->max_w;
	param->max_h = enc_params->max_h;
	param->num_b_frame = enc_params->num_b_frame;
	param->slbc_ready = ctx->use_slbc;
	param->i_qp = enc_params->i_qp;
	param->p_qp = enc_params->p_qp;
	param->b_qp = enc_params->b_qp;
	param->svp_mode = enc_params->svp_mode;

}

static int vidioc_venc_subscribe_evt(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	switch (sub->type) {
	case V4L2_EVENT_EOS:
		return v4l2_event_subscribe(fh, sub, 2, NULL);
	case V4L2_EVENT_MTK_VENC_ERROR:
		return v4l2_event_subscribe(fh, sub, 0, NULL);
	default:
		return v4l2_ctrl_subscribe_event(fh, sub);
	}
}

static void mtk_vdec_queue_stop_enc_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_eos = {
		.type = V4L2_EVENT_EOS,
	};

	mtk_v4l2_debug(0, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_eos);
}

static void mtk_venc_queue_error_event(struct mtk_vcodec_ctx *ctx)
{
	static const struct v4l2_event ev_error = {
		.type = V4L2_EVENT_MTK_VENC_ERROR,
	};

	mtk_v4l2_debug(0, "[%d]", ctx->id);
	v4l2_event_queue_fh(&ctx->fh, &ev_error);
}

static int vidioc_venc_s_fmt_cap(struct file *file, void *priv,
				 struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	int i, ret;
	struct mtk_video_fmt *fmt;

	mtk_v4l2_debug(4, "[%d] type %d", ctx->id, f->type);
	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		mtk_v4l2_err("fail to get vq");
		return -EINVAL;
	}

	if (vb2_is_busy(vq)) {
		mtk_v4l2_err("queue busy");
		return -EBUSY;
	}

	q_data = mtk_venc_get_q_data(ctx, f->type);
	if (!q_data) {
		mtk_v4l2_err("fail to get q data");
		return -EINVAL;
	}

	fmt = mtk_venc_find_format(f, MTK_FMT_ENC);
	if (!fmt) {
		f->fmt.pix.pixelformat =
			mtk_video_formats[default_cap_fmt_idx].fourcc;
		fmt = mtk_venc_find_format(f, MTK_FMT_ENC);
	}
	if (fmt == NULL) {
		mtk_v4l2_err("fail to get fmt");
		return -EINVAL;
	}

	q_data->fmt = fmt;
	ret = vidioc_try_fmt(f, q_data->fmt, ctx);
	if (ret)
		return ret;

	q_data->coded_width = f->fmt.pix_mp.width;
	q_data->coded_height = f->fmt.pix_mp.height;
	q_data->field = f->fmt.pix_mp.field;

	for (i = 0; i < f->fmt.pix_mp.num_planes; i++) {
		struct v4l2_plane_pix_format    *plane_fmt;

		plane_fmt = &f->fmt.pix_mp.plane_fmt[i];
		q_data->bytesperline[i] = plane_fmt->bytesperline;
		q_data->sizeimage[i] = plane_fmt->sizeimage;
	}

	if (ctx->state == MTK_STATE_FREE) {
		ret = venc_if_init(ctx, q_data->fmt->fourcc);
		if (ret) {
			mtk_v4l2_err("venc_if_init failed=%d, codec type=%x",
				     ret, q_data->fmt->fourcc);
			ctx->state = MTK_STATE_ABORT;
			mtk_venc_queue_error_event(ctx);
			return -EBUSY;
		}
		ctx->state = MTK_STATE_INIT;
	}

	return 0;
}

static int vidioc_venc_s_fmt_out(struct file *file, void *priv,
				 struct v4l2_format *f)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct mtk_q_data *q_data;
	int ret, i;
	struct mtk_video_fmt *fmt;

	mtk_v4l2_debug(4, "[%d] type %d", ctx->id, f->type);
	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, f->type);
	if (!vq) {
		mtk_v4l2_err("fail to get vq");
		return -EINVAL;
	}

	if (vb2_is_busy(vq)) {
		mtk_v4l2_err("queue busy");
		return -EBUSY;
	}

	q_data = mtk_venc_get_q_data(ctx, f->type);
	if (!q_data) {
		mtk_v4l2_err("fail to get q data");
		return -EINVAL;
	}

	fmt = mtk_venc_find_format(f, MTK_FMT_FRAME);
	if (!fmt) {
		f->fmt.pix.pixelformat =
			mtk_video_formats[default_out_fmt_idx].fourcc;
		fmt = mtk_venc_find_format(f, MTK_FMT_FRAME);
	}

	q_data->visible_width = f->fmt.pix_mp.width;
	q_data->visible_height = f->fmt.pix_mp.height;
	q_data->fmt = fmt;
	ret = vidioc_try_fmt(f, q_data->fmt, ctx);
	if (ret)
		return ret;

	q_data->coded_width = f->fmt.pix_mp.width;
	q_data->coded_height = f->fmt.pix_mp.height;

	q_data->field = f->fmt.pix_mp.field;
	ctx->colorspace = f->fmt.pix_mp.colorspace;
	ctx->ycbcr_enc = f->fmt.pix_mp.ycbcr_enc;
	ctx->quantization = f->fmt.pix_mp.quantization;
	ctx->xfer_func = f->fmt.pix_mp.xfer_func;

	for (i = 0; i < f->fmt.pix_mp.num_planes; i++) {
		struct v4l2_plane_pix_format *plane_fmt;

		plane_fmt = &f->fmt.pix_mp.plane_fmt[i];
		q_data->bytesperline[i] = plane_fmt->bytesperline;
		q_data->sizeimage[i] = plane_fmt->sizeimage;
	}

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

	pix->width = q_data->coded_width;
	pix->height = q_data->coded_height;
	pix->pixelformat = q_data->fmt->fourcc;
	pix->field = q_data->field;
	pix->num_planes = q_data->fmt->num_planes;
	for (i = 0; i < pix->num_planes; i++) {
		pix->plane_fmt[i].bytesperline = q_data->bytesperline[i];
		pix->plane_fmt[i].sizeimage = q_data->sizeimage[i];
		memset(&(pix->plane_fmt[i].reserved[0]), 0x0,
		       sizeof(pix->plane_fmt[i].reserved));
	}

	pix->flags = 0;
	pix->colorspace = ctx->colorspace;
	pix->ycbcr_enc = ctx->ycbcr_enc;
	pix->quantization = ctx->quantization;
	pix->xfer_func = ctx->xfer_func;
	mtk_v4l2_debug(4, "[%d] type %d", ctx->id, f->type);

	return 0;
}

static int vidioc_try_fmt_vid_cap_mplane(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	fmt = mtk_venc_find_format(f, MTK_FMT_ENC);
	if (!fmt) {
		f->fmt.pix.pixelformat =
			mtk_video_formats[default_cap_fmt_idx].fourcc;
		fmt = mtk_venc_find_format(f, MTK_FMT_ENC);
	}
	if (fmt == NULL) {
		mtk_v4l2_err("fail to get fmt");
		return -EINVAL;
	}

	f->fmt.pix_mp.colorspace = ctx->colorspace;
	f->fmt.pix_mp.ycbcr_enc = ctx->ycbcr_enc;
	f->fmt.pix_mp.quantization = ctx->quantization;
	f->fmt.pix_mp.xfer_func = ctx->xfer_func;

	return vidioc_try_fmt(f, fmt, ctx);
}

static int vidioc_try_fmt_vid_out_mplane(struct file *file, void *priv,
					 struct v4l2_format *f)
{
	struct mtk_video_fmt *fmt;
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	fmt = mtk_venc_find_format(f, MTK_FMT_FRAME);
	if (!fmt) {
		f->fmt.pix.pixelformat =
			mtk_video_formats[default_out_fmt_idx].fourcc;
		fmt = mtk_venc_find_format(f, MTK_FMT_FRAME);
	}
	if (fmt == NULL) {
		mtk_v4l2_err("fail to get fmt");
		return -EINVAL;
	}

	if (!f->fmt.pix_mp.colorspace) {
		f->fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
		f->fmt.pix_mp.ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
		f->fmt.pix_mp.quantization = V4L2_QUANTIZATION_DEFAULT;
		f->fmt.pix_mp.xfer_func = V4L2_XFER_FUNC_DEFAULT;
	}

	return vidioc_try_fmt(f, fmt, ctx);
}

static int vidioc_venc_g_selection(struct file *file, void *priv,
				   struct v4l2_selection *s)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct mtk_q_data *q_data;

	if (!V4L2_TYPE_IS_OUTPUT(s->type))
		return -EINVAL;

	if (s->target != V4L2_SEL_TGT_COMPOSE &&
	    s->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

	q_data = mtk_venc_get_q_data(ctx, s->type);
	if (!q_data)
		return -EINVAL;

	s->r.top = 0;
	s->r.left = 0;
	s->r.width = q_data->visible_width;
	s->r.height = q_data->visible_height;

	return 0;
}

static int vidioc_venc_s_selection(struct file *file, void *priv,
				   struct v4l2_selection *s)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct mtk_q_data *q_data;


	if (!V4L2_TYPE_IS_OUTPUT(s->type))
		return -EINVAL;

	if (s->target != V4L2_SEL_TGT_COMPOSE &&
	    s->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

	q_data = mtk_venc_get_q_data(ctx, s->type);
	if (!q_data)
		return -EINVAL;

	s->r.top = 0;
	s->r.left = 0;
	q_data->visible_width = s->r.width;
	q_data->visible_height = s->r.height;

	return 0;
}

static int vidioc_venc_qbuf(struct file *file, void *priv,
			    struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *vq;
	struct vb2_buffer *vb;
	struct mtk_video_enc_buf *mtkbuf;
	struct vb2_v4l2_buffer *vb2_v4l2;

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_err("[%d] Call on QBUF after unrecoverable error",
			     ctx->id);
		return -EIO;
	}

	// Check if need to proceed cache operations
	vq = v4l2_m2m_get_vq(ctx->m2m_ctx, buf->type);
	vb = vq->bufs[buf->index];
	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	mtkbuf = container_of(vb2_v4l2, struct mtk_video_enc_buf, vb);

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (buf->m.planes[0].bytesused == 0) {
			mtkbuf->lastframe = EOS;
			mtk_v4l2_debug(1, "[%d] index=%d Eos FB(%d,%d) vb=%p pts=%llu",
				ctx->id, buf->index,
				buf->bytesused,
				buf->length, vb, vb->timestamp);
		} else if (buf->flags & V4L2_BUF_FLAG_LAST) {
			mtkbuf->lastframe = EOS_WITH_DATA;
			mtk_v4l2_debug(1, "[%d] id=%d EarlyEos FB(%d,%d) vb=%p pts=%llu",
				ctx->id, buf->index, buf->m.planes[0].bytesused,
				buf->length, vb, vb->timestamp);
		} else {
			mtkbuf->lastframe = NON_EOS;
			mtk_v4l2_debug(1, "[%d] id=%d getdata FB(%d,%d) vb=%p pts=%llu ",
				ctx->id, buf->index,
				buf->m.planes[0].bytesused,
				buf->length, mtkbuf, vb->timestamp);
		}
	} else
		mtk_v4l2_debug(1, "[%d] id=%d BS (%d) vb=%p",
				ctx->id, buf->index,
				buf->length, mtkbuf);

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

	if (buf->flags & V4L2_BUF_FLAG_ROI && buf->reserved2 != 0) {
		mtk_v4l2_debug(1, "[%d] Have ROI info map 1, buf->index:%d. mtkbuf:%p, pa:0x%x",
			ctx->id, buf->index, mtkbuf, buf->reserved2);
		mtkbuf->roimap = buf->reserved2;
		mtkbuf->frm_buf.roimap = buf->reserved2;
	}
	if (buf->flags & V4L2_BUF_FLAG_HDR_META && buf->reserved2 != 0) {
		struct dma_buf_attachment *buf_att;
		struct sg_table *sgt;

		mtkbuf->frm_buf.has_meta = 1;
		mtkbuf->frm_buf.meta_dma = dma_buf_get(buf->reserved2);

		if (IS_ERR(mtkbuf->frm_buf.meta_dma)) {
			mtk_v4l2_err("%s meta_dma is err 0x%p.\n", __func__,
				mtkbuf->frm_buf.meta_dma);

			mtk_venc_queue_error_event(ctx);
			return -EINVAL;
		}

		buf_att = dma_buf_attach(mtkbuf->frm_buf.meta_dma,
			&ctx->dev->plat_dev->dev);
		sgt = dma_buf_map_attachment(buf_att, DMA_TO_DEVICE);
		mtkbuf->frm_buf.meta_addr = sg_dma_address(sgt->sgl);
		dma_buf_unmap_attachment(buf_att, sgt, DMA_TO_DEVICE);
		dma_buf_detach(mtkbuf->frm_buf.meta_dma, buf_att);

		mtk_v4l2_debug(1, "[%d] Have HDR info meta fd, buf->index:%d. mtkbuf:%p, fd:%u",
			ctx->id, buf->index, mtkbuf, buf->reserved2);
	}

	return v4l2_m2m_qbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_venc_dqbuf(struct file *file, void *priv,
			     struct v4l2_buffer *buf)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);

	if (ctx->state == MTK_STATE_ABORT) {
		mtk_v4l2_err("[%d] Call on QBUF after unrecoverable error",
			     ctx->id);
		return -EIO;
	}

	return v4l2_m2m_dqbuf(file, ctx->m2m_ctx, buf);
}

static int vidioc_try_encoder_cmd(struct file *file, void *priv,
	struct v4l2_encoder_cmd *cmd)
{
	switch (cmd->cmd) {
	case V4L2_ENC_CMD_STOP:
	case V4L2_ENC_CMD_START:
		cmd->flags = 0; // don't support flags
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int vidioc_encoder_cmd(struct file *file, void *priv,
	struct v4l2_encoder_cmd *cmd)
{
	struct mtk_vcodec_ctx *ctx = fh_to_ctx(priv);
	struct vb2_queue *src_vq, *dst_vq;
	int ret;

	ret = vidioc_try_encoder_cmd(file, priv, cmd);
	if (ret)
		return ret;

	mtk_v4l2_debug(0, "[%d] encoder cmd= %u", ctx->id, cmd->cmd);
	dst_vq = v4l2_m2m_get_vq(ctx->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	switch (cmd->cmd) {
	case V4L2_ENC_CMD_STOP:
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
		ctx->enc_flush_buf->lastframe = EOS;
		v4l2_m2m_buf_queue_check(ctx->m2m_ctx, &ctx->enc_flush_buf->vb);
		v4l2_m2m_try_schedule(ctx->m2m_ctx);
		break;

	case V4L2_ENC_CMD_START:
		vb2_clear_last_buffer_dequeued(dst_vq);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

const struct v4l2_ioctl_ops mtk_venc_ioctl_ops = {
	.vidioc_streamon                = v4l2_m2m_ioctl_streamon,
	.vidioc_streamoff               = v4l2_m2m_ioctl_streamoff,

	.vidioc_reqbufs                 = v4l2_m2m_ioctl_reqbufs,
	.vidioc_querybuf                = v4l2_m2m_ioctl_querybuf,
	.vidioc_qbuf                    = vidioc_venc_qbuf,
	.vidioc_dqbuf                   = vidioc_venc_dqbuf,

	.vidioc_querycap                = vidioc_venc_querycap,
	.vidioc_enum_fmt_vid_cap_mplane = vidioc_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out_mplane,
	.vidioc_enum_framesizes         = vidioc_enum_framesizes,

	.vidioc_try_fmt_vid_cap_mplane  = vidioc_try_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_out_mplane  = vidioc_try_fmt_vid_out_mplane,
	.vidioc_expbuf                  = v4l2_m2m_ioctl_expbuf,

	.vidioc_s_parm                  = vidioc_venc_s_parm,
	.vidioc_g_parm                  = vidioc_venc_g_parm,
	.vidioc_s_fmt_vid_cap_mplane    = vidioc_venc_s_fmt_cap,
	.vidioc_s_fmt_vid_out_mplane    = vidioc_venc_s_fmt_out,

	.vidioc_g_fmt_vid_cap_mplane    = vidioc_venc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane    = vidioc_venc_g_fmt,

	.vidioc_create_bufs             = v4l2_m2m_ioctl_create_bufs,
	.vidioc_prepare_buf             = v4l2_m2m_ioctl_prepare_buf,

	.vidioc_subscribe_event         = vidioc_venc_subscribe_evt,
	.vidioc_unsubscribe_event       = v4l2_event_unsubscribe,

	.vidioc_g_selection             = vidioc_venc_g_selection,
	.vidioc_s_selection             = vidioc_venc_s_selection,

	.vidioc_encoder_cmd             = vidioc_encoder_cmd,
	.vidioc_try_encoder_cmd         = vidioc_try_encoder_cmd,
};

static int vb2ops_venc_queue_setup(struct vb2_queue *vq,
				   unsigned int *nbuffers,
				   unsigned int *nplanes,
				   unsigned int sizes[],
				   struct device *alloc_devs[])
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vq);
	struct mtk_q_data *q_data;
	unsigned int i;

	q_data = mtk_venc_get_q_data(ctx, vq->type);

	if (q_data == NULL)
		return -EINVAL;

	if (*nplanes) {
		for (i = 0; i < *nplanes; i++)
			if (sizes[i] < q_data->sizeimage[i])
				return -EINVAL;
	} else {
		*nplanes = q_data->fmt->num_planes;
		for (i = 0; i < *nplanes; i++)
			sizes[i] = q_data->sizeimage[i];
	}

	mtk_v4l2_debug(2, "[%d] nplanes %d sizeimage %d %d %d",
		       ctx->id,
		       *nplanes,
		       q_data->sizeimage[0],
		       q_data->sizeimage[1],
		       q_data->sizeimage[2]);

	return 0;
}

static int vb2ops_venc_buf_prepare(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_q_data *q_data;
	int i;
	struct mtk_video_enc_buf *mtkbuf;
	struct vb2_v4l2_buffer *vb2_v4l2;

	if (vb->vb2_queue->memory != VB2_MEMORY_DMABUF)
		return 0;

	q_data = mtk_venc_get_q_data(ctx, vb->vb2_queue->type);

	// Check if need to proceed cache operations
	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	mtkbuf = container_of(vb2_v4l2, struct mtk_video_enc_buf, vb);

	for (i = 0; i < q_data->fmt->num_planes; i++) {
		if (vb2_plane_size(vb, i) < q_data->sizeimage[i]) {
			mtk_v4l2_err("data will not fit into plane %d (%lu < %d)",
				i,
				vb2_plane_size(vb, i),
				q_data->sizeimage[i]);
			return -EINVAL;
		}

		// Check if need to proceed cache operations
		vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
		mtkbuf = container_of(vb2_v4l2, struct mtk_video_enc_buf, vb);
		if (!(mtkbuf->flags & NO_CAHCE_CLEAN)) {
			struct mtk_vcodec_mem src_mem;
			struct dma_buf_attachment *buf_att;
			struct sg_table *sgt;

			buf_att = dma_buf_attach(vb->planes[i].dbuf,
				&ctx->dev->plat_dev->dev);
			sgt = dma_buf_map_attachment(buf_att, DMA_TO_DEVICE);
			dma_sync_sg_for_device(&ctx->dev->plat_dev->dev,
				sgt->sgl,
				sgt->orig_nents,
				DMA_TO_DEVICE);
			dma_buf_unmap_attachment(buf_att, sgt, DMA_TO_DEVICE);

			src_mem.dma_addr = vb2_dma_contig_plane_dma_addr(vb, i);
			src_mem.size = (size_t)(vb->planes[i].bytesused -
				vb->planes[i].data_offset);
			dma_buf_detach(vb->planes[i].dbuf, buf_att);

			mtk_v4l2_debug(4, "[%d] Cache sync TD for %lx sz=%d dev %p ",
				ctx->id,
				(unsigned long)src_mem.dma_addr,
				(unsigned int)src_mem.size,
				&ctx->dev->plat_dev->dev);
		}
	}

	return 0;
}

static void vb2ops_venc_buf_finish(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct mtk_video_enc_buf *mtkbuf;
	struct vb2_v4l2_buffer *vb2_v4l2;

	vb2_v4l2 = container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	mtkbuf = container_of(vb2_v4l2, struct mtk_video_enc_buf, vb);

	if (vb2_v4l2->flags & V4L2_BUF_FLAG_LAST)
		mtk_v4l2_debug(0, "[%d] type(%d) flags=%x idx=%d pts=%llu",
			ctx->id, vb->vb2_queue->type, vb2_v4l2->flags,
			vb->index, vb->timestamp);

	if (vb->vb2_queue->memory == VB2_MEMORY_DMABUF &&
		!(mtkbuf->flags & NO_CAHCE_INVALIDATE)) {
		if (vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			struct mtk_vcodec_mem dst_mem;
			struct dma_buf_attachment *buf_att;
			struct sg_table *sgt;

			buf_att = dma_buf_attach(vb->planes[0].dbuf,
				&ctx->dev->plat_dev->dev);
			sgt = dma_buf_map_attachment(buf_att, DMA_FROM_DEVICE);
			dma_sync_sg_for_cpu(&ctx->dev->plat_dev->dev, sgt->sgl,
				sgt->orig_nents, DMA_FROM_DEVICE);
			dma_buf_unmap_attachment(buf_att, sgt, DMA_FROM_DEVICE);

			dst_mem.dma_addr = vb2_dma_contig_plane_dma_addr(vb, 0);
			dst_mem.size = (size_t)vb->planes[0].bytesused;
			dma_buf_detach(vb->planes[0].dbuf, buf_att);
			mtk_v4l2_debug(4,
				"[%d] Cache sync FD for %lx sz=%d dev %p",
				ctx->id,
				(unsigned long)dst_mem.dma_addr,
				(unsigned int)dst_mem.size,
				&ctx->dev->plat_dev->dev);
		}
	}
}


static void vb2ops_venc_buf_queue(struct vb2_buffer *vb)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(vb->vb2_queue);
	struct vb2_v4l2_buffer *vb2_v4l2 =
		container_of(vb, struct vb2_v4l2_buffer, vb2_buf);

	struct mtk_video_enc_buf *mtk_buf =
		container_of(vb2_v4l2, struct mtk_video_enc_buf, vb);

	if ((vb->vb2_queue->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) &&
	    (ctx->param_change != MTK_ENCODE_PARAM_NONE)) {
		mtk_v4l2_debug(1, "[%d] Before id=%d encode parameter change %x",
			       ctx->id,
			       mtk_buf->vb.vb2_buf.index,
			       ctx->param_change);
		mtk_buf->param_change = ctx->param_change;
		mtk_buf->enc_params = ctx->enc_params;
		ctx->param_change = MTK_ENCODE_PARAM_NONE;
	}

	v4l2_m2m_buf_queue_check(ctx->m2m_ctx, to_vb2_v4l2_buffer(vb));
}

static int vb2ops_venc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct venc_enc_param param;
	int ret;
	int i;

	mtk_v4l2_debug(4, "[%d] (%d) state=(%x)", ctx->id, q->type, ctx->state);
	/* Once state turn into MTK_STATE_ABORT, we need stop_streaming
	 * to clear it
	 */
	if (ctx->state == MTK_STATE_ABORT || ctx->state == MTK_STATE_FREE) {
		ret = -EIO;
		goto err_set_param;
	}

	/* Do the initialization when both start_streaming have been called */
	if (V4L2_TYPE_IS_OUTPUT(q->type)) {
		if (!vb2_start_streaming_called(&ctx->m2m_ctx->cap_q_ctx.q))
			return 0;
	} else {
		if (!vb2_start_streaming_called(&ctx->m2m_ctx->out_q_ctx.q))
			return 0;
	}

	memset(&param, 0, sizeof(param));
	mtk_venc_set_param(ctx, &param);
	ret = venc_if_set_param(ctx, VENC_SET_PARAM_ENC, &param);

	mtk_v4l2_debug(0,
	"fmt 0x%x, P/L %d/%d, w/h %d/%d, buf %d/%d, fps/bps %d/%d(%d), gop %d, ip# %d opr %d async %d grid size %d/%d b#%d, slbc %d",
	param.input_yuv_fmt, param.profile,
	param.level, param.width, param.height,
	param.buf_width, param.buf_height,
	param.frm_rate, param.bitrate, param.bitratemode,
	param.gop_size, param.intra_period,
	param.operationrate, ctx->async_mode,
	(param.heif_grid_size>>16), param.heif_grid_size&0xffff,
	param.num_b_frame, param.slbc_ready);

	if (ret) {
		mtk_v4l2_err("venc_if_set_param failed=%d", ret);
		ctx->state = MTK_STATE_ABORT;
		mtk_venc_queue_error_event(ctx);
		goto err_set_param;
	}
	ctx->param_change = MTK_ENCODE_PARAM_NONE;

	if ((ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H264 ||
	     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H265 ||
	     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_HEIF ||
	     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_MPEG4) &&
	    (ctx->enc_params.seq_hdr_mode !=
	     V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE)) {
		ret = venc_if_set_param(ctx,
					VENC_SET_PARAM_PREPEND_HEADER,
					NULL);
		if (ret) {
			mtk_v4l2_err("venc_if_set_param failed=%d", ret);
			ctx->state = MTK_STATE_ABORT;
			mtk_venc_queue_error_event(ctx);
			goto err_set_param;
		}
		ctx->state = MTK_STATE_HEADER;
	} else {
		ctx->state = MTK_STATE_INIT;
	}

	return 0;

err_set_param:
	for (i = 0; i < q->num_buffers; ++i) {
		if (q->bufs[i]->state == VB2_BUF_STATE_ACTIVE) {
			mtk_v4l2_debug(0, "[%d] id=%d, type=%d, %d -> VB2_BUF_STATE_QUEUED",
				       ctx->id, i, q->type,
				       (int)q->bufs[i]->state);
			v4l2_m2m_buf_done(to_vb2_v4l2_buffer(q->bufs[i]),
					  VB2_BUF_STATE_QUEUED);
		}
	}

	return ret;
}

static void vb2ops_venc_stop_streaming(struct vb2_queue *q)
{
	struct mtk_vcodec_ctx *ctx = vb2_get_drv_priv(q);
	struct vb2_buffer *src_buf, *dst_buf;
	struct venc_done_result enc_result;
	int ret;

	mtk_v4l2_debug(2, "[%d]-> type=%d", ctx->id, q->type);

	ret = venc_if_encode(ctx,
		VENC_START_OPT_ENCODE_FRAME_FINAL,
		NULL, NULL, &enc_result);
	if (!ctx->async_mode)
		mtk_enc_put_buf(ctx);

	if (ret)
		mtk_v4l2_err("venc_if_deinit failed=%d", ret);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		while ((dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx))) {
			dst_buf->planes[0].bytesused = 0;
			v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf),
					  VB2_BUF_STATE_ERROR);
		}
	} else {
		while ((src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
			v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf),
					  VB2_BUF_STATE_ERROR);
	}

	if ((q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
	     vb2_is_streaming(&ctx->m2m_ctx->out_q_ctx.q)) ||
	    (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
	     vb2_is_streaming(&ctx->m2m_ctx->cap_q_ctx.q))) {
		mtk_v4l2_debug(1, "[%d]-> q type %d out=%d cap=%d",
			       ctx->id, q->type,
			       vb2_is_streaming(&ctx->m2m_ctx->out_q_ctx.q),
			       vb2_is_streaming(&ctx->m2m_ctx->cap_q_ctx.q));
		return;
	}

}

static const struct vb2_ops mtk_venc_vb2_ops = {
	.queue_setup            = vb2ops_venc_queue_setup,
	.buf_prepare            = vb2ops_venc_buf_prepare,
	.buf_queue              = vb2ops_venc_buf_queue,
	.wait_prepare           = vb2_ops_wait_prepare,
	.wait_finish            = vb2_ops_wait_finish,
	.buf_finish             = vb2ops_venc_buf_finish,
	.start_streaming        = vb2ops_venc_start_streaming,
	.stop_streaming         = vb2ops_venc_stop_streaming,
};

static int mtk_venc_encode_header(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;
	int ret;
	struct vb2_buffer *src_buf, *dst_buf;
	struct vb2_v4l2_buffer *dst_vb2_v4l2, *src_vb2_v4l2;
	struct mtk_video_enc_buf *dst_buf_info;
	struct mtk_vcodec_mem *bs_buf;
	struct venc_done_result enc_result;

	memset(&enc_result, 0, sizeof(enc_result));
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	if (!dst_buf) {
		mtk_v4l2_debug(1, "No dst buffer");
		return -EINVAL;
	}
	dst_vb2_v4l2 = to_vb2_v4l2_buffer(dst_buf);
	dst_buf_info = container_of(dst_vb2_v4l2, struct mtk_video_enc_buf, vb);

	bs_buf = &dst_buf_info->bs_buf;
	bs_buf->va = vb2_plane_vaddr(dst_buf, 0);
	bs_buf->dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	bs_buf->size = (size_t)dst_buf->planes[0].length;
	bs_buf->dmabuf = dst_buf->planes[0].dbuf;

	mtk_v4l2_debug(1,
		       "[%d] buf id=%d va=0x%p dma_addr=0x%llx size=%zu",
		       ctx->id,
		       dst_buf->index, bs_buf->va,
		       (u64)bs_buf->dma_addr,
		       bs_buf->size);

	ret = venc_if_encode(ctx,
			     VENC_START_OPT_ENCODE_SEQUENCE_HEADER,
			     NULL, bs_buf, &enc_result);

	get_free_buffers(ctx, &enc_result);

	if (enc_result.bs_va == 0) {
		dst_buf->planes[0].bytesused = 0;
		ctx->state = MTK_STATE_ABORT;
		mtk_venc_queue_error_event(ctx);
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf),
				  VB2_BUF_STATE_ERROR);
		mtk_v4l2_err("%s venc_if_encode failed=%d",
			__func__, ret);
		return -EINVAL;
	}
	src_buf = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	if (src_buf) {
		src_vb2_v4l2 = to_vb2_v4l2_buffer(src_buf);
		dst_vb2_v4l2->vb2_buf.timestamp =
			src_vb2_v4l2->vb2_buf.timestamp;
		dst_vb2_v4l2->timecode = src_vb2_v4l2->timecode;
	} else
		mtk_v4l2_err("No timestamp for the header buffer.");

	ctx->state = MTK_STATE_HEADER;
	dst_buf->planes[0].bytesused = enc_result.bs_size;
	v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf), VB2_BUF_STATE_DONE);

	return 0;
}

static int mtk_venc_param_change(struct mtk_vcodec_ctx *ctx)
{
	struct venc_enc_param enc_prm;
	struct vb2_buffer *vb = v4l2_m2m_next_src_buf(ctx->m2m_ctx);
	struct vb2_v4l2_buffer *vb2_v4l2 =
		container_of(vb, struct vb2_v4l2_buffer, vb2_buf);
	struct mtk_video_enc_buf *mtk_buf =
		container_of(vb2_v4l2, struct mtk_video_enc_buf, vb);

	int ret = 0;

	memset(&enc_prm, 0, sizeof(enc_prm));
	if (mtk_buf->param_change == MTK_ENCODE_PARAM_NONE)
		return 0;

	if (mtk_buf->param_change & MTK_ENCODE_PARAM_BITRATE) {
		enc_prm.bitrate = mtk_buf->enc_params.bitrate;
		mtk_v4l2_debug(1, "[%d] id=%d, change param br=%d",
			       ctx->id,
			       mtk_buf->vb.vb2_buf.index,
			       enc_prm.bitrate);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_ADJUST_BITRATE,
					 &enc_prm);
	}
	if (mtk_buf->param_change & MTK_ENCODE_PARAM_SEC_ENCODE) {
		enc_prm.svp_mode = mtk_buf->enc_params.svp_mode;
		mtk_v4l2_debug(0, "[%d] change param svp=%d",
				ctx->id,
				enc_prm.svp_mode);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_SEC_MODE,
					 &enc_prm);
	}
	if (!ret && mtk_buf->param_change & MTK_ENCODE_PARAM_FRAMERATE) {
		enc_prm.frm_rate = mtk_buf->enc_params.framerate_num /
				   mtk_buf->enc_params.framerate_denom;
		mtk_v4l2_debug(1, "[%d] id=%d, change param fr=%d",
			       ctx->id,
			       mtk_buf->vb.vb2_buf.index,
			       enc_prm.frm_rate);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_ADJUST_FRAMERATE,
					 &enc_prm);
	}
	if (!ret && mtk_buf->param_change & MTK_ENCODE_PARAM_GOP_SIZE) {
		enc_prm.gop_size = mtk_buf->enc_params.gop_size;
		mtk_v4l2_debug(1, "change param intra period=%d",
			       enc_prm.gop_size);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_GOP_SIZE,
					 &enc_prm);
	}
	if (!ret && mtk_buf->param_change & MTK_ENCODE_PARAM_FORCE_INTRA) {
		mtk_v4l2_debug(1, "[%d] id=%d, change param force I=%d",
			       ctx->id,
			       mtk_buf->vb.vb2_buf.index,
			       mtk_buf->enc_params.force_intra);
		if (mtk_buf->enc_params.force_intra)
			ret |= venc_if_set_param(ctx,
						 VENC_SET_PARAM_FORCE_INTRA,
						 NULL);
	}

	if (!ret && mtk_buf->param_change & MTK_ENCODE_PARAM_SCENARIO) {
		enc_prm.scenario = mtk_buf->enc_params.scenario;
		if (mtk_buf->enc_params.scenario)
			ret |= venc_if_set_param(ctx,
						 VENC_SET_PARAM_SCENARIO,
						 &enc_prm);
		mtk_v4l2_debug(0, "[%d] idx=%d, change param scenario=%d async_mode=%d",
			       ctx->id,
			       mtk_buf->vb.vb2_buf.index,
			       mtk_buf->enc_params.scenario,
			       ctx->async_mode);
	}

	if (!ret && mtk_buf->param_change & MTK_ENCODE_PARAM_NONREFP) {
		enc_prm.nonrefp = mtk_buf->enc_params.nonrefp;
		mtk_v4l2_debug(1, "[%d] idx=%d, change param nonref=%d",
			       ctx->id,
			       mtk_buf->vb.vb2_buf.index,
			       mtk_buf->enc_params.nonrefp);
		ret |= venc_if_set_param(ctx,
					 VENC_SET_PARAM_NONREFP,
					 &enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_DETECTED_FRAMERATE) {
		enc_prm.detectframerate = mtk_buf->enc_params.detectframerate;
		mtk_v4l2_debug(1, "[%d] idx=%d, change param detectfr=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				mtk_buf->enc_params.detectframerate);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_DETECTED_FRAMERATE,
					&enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_RFS_ON) {
		enc_prm.rfs = mtk_buf->enc_params.rfs;
		mtk_v4l2_debug(1, "[%d] idx=%d, change param rfs=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				mtk_buf->enc_params.rfs);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_RFS_ON,
					&enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_PREPEND_SPSPPS_TO_IDR) {
		enc_prm.prependheader = mtk_buf->enc_params.prependheader;
		mtk_v4l2_debug(1, "[%d] idx=%d, prepend spspps idr=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				mtk_buf->enc_params.prependheader);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_PREPEND_SPSPPS_TO_IDR,
					&enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_OPERATION_RATE) {
		enc_prm.operationrate = mtk_buf->enc_params.operationrate;
		mtk_v4l2_debug(1, "[%d] idx=%d, operationrate=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				mtk_buf->enc_params.operationrate);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_OPERATION_RATE,
					&enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_BITRATE_MODE) {
		enc_prm.bitratemode = mtk_buf->enc_params.bitratemode;
		mtk_v4l2_debug(1, "[%d] idx=%d, bitratemode=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				mtk_buf->enc_params.bitratemode);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_BITRATE_MODE,
					&enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_ROI_ON) {
		enc_prm.roion = mtk_buf->enc_params.roion;
		mtk_v4l2_debug(1, "[%d] idx=%d, roion=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				mtk_buf->enc_params.roion);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_ROI_ON,
					&enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_GRID_SIZE) {
		enc_prm.heif_grid_size = mtk_buf->enc_params.heif_grid_size;
		mtk_v4l2_err("[%d] idx=%d, heif_grid_size=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				mtk_buf->enc_params.heif_grid_size);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_HEIF_GRID_SIZE,
					&enc_prm);
	}

	if (!ret &&
	mtk_buf->param_change & MTK_ENCODE_PARAM_COLOR_DESC) {
		// avoid much copies
		enc_prm.color_desc = &mtk_buf->enc_params.color_desc;
		mtk_v4l2_err("[%d] idx=%d, color_primaries=%d range=%d",
				ctx->id,
				mtk_buf->vb.vb2_buf.index,
				enc_prm.color_desc->color_primaries,
				enc_prm.color_desc->full_range);
		ret |= venc_if_set_param(ctx,
					VENC_SET_PARAM_COLOR_DESC,
					&enc_prm);
	}

	mtk_buf->param_change = MTK_ENCODE_PARAM_NONE;

	if (ret) {
		ctx->state = MTK_STATE_ABORT;
		mtk_venc_queue_error_event(ctx);
		mtk_v4l2_err("venc_if_set_param %d failed=%d",
			     mtk_buf->param_change, ret);
		return -1;
	}

	return 0;
}

void mtk_venc_check_queue_cnt(struct mtk_vcodec_ctx *ctx, struct vb2_queue *vq)
{
	int done_list_cnt = 0;
	int rdy_q_cnt = 0;
	struct vb2_buffer *vb;
	unsigned long flags;

	spin_lock_irqsave(&vq->done_lock, flags);
	list_for_each_entry(vb, &vq->done_list, queued_entry)
		done_list_cnt++;
	spin_unlock_irqrestore(&vq->done_lock, flags);

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		rdy_q_cnt = ctx->m2m_ctx->out_q_ctx.num_rdy;
	else
		rdy_q_cnt = ctx->m2m_ctx->cap_q_ctx.num_rdy;

	mtk_v4l2_debug(0,
		"[%d] type %d queued_cnt %d done_cnt %d rdy_q_cnt %d tatal %d",
		ctx->id, vq->type, vq->queued_count,
		done_list_cnt, rdy_q_cnt, vq->num_buffers);
}

/*
 * v4l2_m2m_streamoff() holds dev_mutex and waits mtk_venc_worker()
 * to call v4l2_m2m_job_finish().
 * If mtk_venc_worker() tries to acquire dev_mutex, it will deadlock.
 * So this function must not try to acquire dev->dev_mutex.
 * This means v4l2 ioctls and mtk_venc_worker() can run at the same time.
 * mtk_venc_worker() should be carefully implemented to avoid bugs.
 */
static void mtk_venc_worker(struct work_struct *work)
{
	struct mtk_vcodec_ctx *ctx = container_of(work, struct mtk_vcodec_ctx,
					encode_work);
	struct mtk_q_data *q_data_src = &ctx->q_data[MTK_Q_DATA_SRC];
	struct vb2_buffer *src_buf, *dst_buf;
	struct venc_frm_buf *pfrm_buf;
	struct mtk_vcodec_mem *pbs_buf;
	struct venc_done_result enc_result;
	int ret, i, length;
	struct vb2_v4l2_buffer *dst_vb2_v4l2, *src_vb2_v4l2, *pend_src_vb2_v4l2;
	struct mtk_video_enc_buf *dst_buf_info, *src_buf_info;

	mutex_lock(&ctx->worker_lock);
	memset(&enc_result, 0, sizeof(enc_result));
	if (ctx->state == MTK_STATE_ABORT) {
		v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);
		mtk_v4l2_debug(1, " %d", ctx->state);
		mutex_unlock(&ctx->worker_lock);
		return;
	}

	/* check dst_buf, dst_buf may be removed in device_run
	 * to stored encdoe header so we need check dst_buf and
	 * call job_finish here to prevent recursion
	 */
	dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx);
	if (!dst_buf) {
		v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);
		mutex_unlock(&ctx->worker_lock);
		return;
	}

	src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx);
	if (!src_buf) {
		v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);
		mutex_unlock(&ctx->worker_lock);
		return;
	}
	src_vb2_v4l2 = to_vb2_v4l2_buffer(src_buf);
	dst_vb2_v4l2 = to_vb2_v4l2_buffer(dst_buf);

	src_buf_info = container_of(src_vb2_v4l2, struct mtk_video_enc_buf, vb);
	dst_buf_info = container_of(dst_vb2_v4l2, struct mtk_video_enc_buf, vb);

	pbs_buf = &dst_buf_info->bs_buf;
	pfrm_buf = &src_buf_info->frm_buf;

	pbs_buf->va = vb2_plane_vaddr(dst_buf, 0);
	pbs_buf->dma_addr = vb2_dma_contig_plane_dma_addr(dst_buf, 0);
	pbs_buf->size = (size_t)dst_buf->planes[0].length;
	pbs_buf->dmabuf = dst_buf->planes[0].dbuf;

	if (src_buf_info->lastframe == EOS) {
		if (ctx->oal_vcodec == 1) {
			ret = venc_if_encode(ctx,
					 VENC_START_OPT_ENCODE_FRAME_FINAL,
					 NULL, pbs_buf, &enc_result);

			pend_src_vb2_v4l2 =
				to_vb2_v4l2_buffer(ctx->pend_src_buf);
			dst_vb2_v4l2->flags |= pend_src_vb2_v4l2->flags;
			dst_vb2_v4l2->vb2_buf.timestamp =
				pend_src_vb2_v4l2->vb2_buf.timestamp;
			dst_vb2_v4l2->timecode = pend_src_vb2_v4l2->timecode;
			dst_vb2_v4l2->sequence = pend_src_vb2_v4l2->sequence;
			dst_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
			if (enc_result.is_key_frm)
				dst_vb2_v4l2->flags |= V4L2_BUF_FLAG_KEYFRAME;

			if (ret) {
				dst_buf->planes[0].bytesused = 0;
				v4l2_m2m_buf_done(pend_src_vb2_v4l2,
						VB2_BUF_STATE_ERROR);
				v4l2_m2m_buf_done(dst_vb2_v4l2,
						VB2_BUF_STATE_ERROR);
				mtk_v4l2_err("last venc_if_encode failed=%d",
									ret);
				if (ret == -EIO) {
					ctx->state = MTK_STATE_ABORT;
					mtk_venc_queue_error_event(ctx);
				}
			} else {
				dst_buf->planes[0].bytesused =
							enc_result.bs_size;
				v4l2_m2m_buf_done(pend_src_vb2_v4l2,
							VB2_BUF_STATE_DONE);
				v4l2_m2m_buf_done(dst_vb2_v4l2,
							VB2_BUF_STATE_DONE);
			}

			ctx->pend_src_buf = NULL;
		} else {
			ret = venc_if_encode(ctx,
					VENC_START_OPT_ENCODE_FRAME_FINAL,
					NULL, NULL, &enc_result);
			dst_vb2_v4l2->vb2_buf.timestamp =
				src_vb2_v4l2->vb2_buf.timestamp;
			dst_vb2_v4l2->timecode = src_vb2_v4l2->timecode;
			dst_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
			dst_buf->planes[0].bytesused = 0;

			if (ret) {
				mtk_v4l2_err("last venc_if_encode failed=%d",
									ret);
				if (ret == -EIO) {
					ctx->state = MTK_STATE_ABORT;
					mtk_venc_queue_error_event(ctx);
				}
			} else if (!ctx->async_mode)
				mtk_enc_put_buf(ctx);

			mtk_venc_check_queue_cnt(ctx, src_buf->vb2_queue);
			mtk_venc_check_queue_cnt(ctx, dst_buf->vb2_queue);

			v4l2_m2m_buf_done(dst_vb2_v4l2,
				VB2_BUF_STATE_DONE);
		}
		mtk_vdec_queue_stop_enc_event(ctx);

		if (src_buf->planes[0].bytesused == 0U) {
			src_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
			vb2_set_plane_payload(&src_buf_info->vb.vb2_buf, 0, 0);
			v4l2_m2m_buf_done(src_vb2_v4l2,
				VB2_BUF_STATE_DONE);
		}
		v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);
		mutex_unlock(&ctx->worker_lock);
		return;
	} else if (src_buf_info->lastframe == EOS_WITH_DATA) {
		/*
		 * Getting early eos frame buffer, after encode this
		 * buffer, need to flush encoder. Use the flush_buf
		 * as normal EOS, and flush encoder.
		 */
		mtk_v4l2_debug(0, "[%d] EarlyEos: encode last frame %d",
			ctx->id, src_buf->planes[0].bytesused);
		src_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
		dst_vb2_v4l2->flags |= V4L2_BUF_FLAG_LAST;
		ctx->enc_flush_buf->lastframe = EOS;
		v4l2_m2m_buf_queue_check(ctx->m2m_ctx, &ctx->enc_flush_buf->vb);
	}

	for (i = 0; i < src_buf->num_planes ; i++) {
		pfrm_buf->fb_addr[i].va = vb2_plane_vaddr(src_buf, i) +
			(size_t)src_buf->planes[i].data_offset;
		pfrm_buf->fb_addr[i].dma_addr =
			vb2_dma_contig_plane_dma_addr(src_buf, i) +
			(size_t)src_buf->planes[i].data_offset;
		pfrm_buf->fb_addr[i].size =
				(size_t)(src_buf->planes[i].bytesused-
				src_buf->planes[i].data_offset);
		pfrm_buf->fb_addr[i].dmabuf =
				src_buf->planes[i].dbuf;
		pfrm_buf->fb_addr[i].data_offset =
				src_buf->planes[i].data_offset;

		mtk_v4l2_debug(2, "fb_addr[%d].va %p, offset %d, dma_addr %p, size %d\n",
			i, pfrm_buf->fb_addr[i].va,
			src_buf->planes[i].data_offset,
			(void *)pfrm_buf->fb_addr[i].dma_addr,
			(int)pfrm_buf->fb_addr[i].size);
	}
	pfrm_buf->num_planes = src_buf->num_planes;
	pfrm_buf->timestamp = src_vb2_v4l2->vb2_buf.timestamp;
	length = q_data_src->coded_width * q_data_src->coded_height;

	mtk_v4l2_debug(2,
			"Framebuf VA=%p PA=%llx Size=0x%zx Offset=%d;VA=%p PA=0x%llx Size=0x%zx Offset=%d;VA=%p PA=0x%llx Size=%zu Offset=%d",
			pfrm_buf->fb_addr[0].va,
			(u64)pfrm_buf->fb_addr[0].dma_addr,
			pfrm_buf->fb_addr[0].size,
			src_buf->planes[0].data_offset,
			pfrm_buf->fb_addr[1].va,
			(u64)pfrm_buf->fb_addr[1].dma_addr,
			pfrm_buf->fb_addr[1].size,
			src_buf->planes[1].data_offset,
			pfrm_buf->fb_addr[2].va,
			(u64)pfrm_buf->fb_addr[2].dma_addr,
			pfrm_buf->fb_addr[2].size,
			src_buf->planes[2].data_offset);

	pfrm_buf->roimap = src_buf_info->roimap;
	ret = venc_if_encode(ctx, VENC_START_OPT_ENCODE_FRAME,
				 pfrm_buf, pbs_buf, &enc_result);
	if (ret) {
		dst_buf->planes[0].bytesused = 0;
		v4l2_m2m_buf_done(src_vb2_v4l2, VB2_BUF_STATE_ERROR);
		v4l2_m2m_buf_done(dst_vb2_v4l2, VB2_BUF_STATE_ERROR);
		mtk_v4l2_err("venc_if_encode failed=%d", ret);
		if (ret == -EIO) {
			ctx->state = MTK_STATE_ABORT;
			mtk_venc_queue_error_event(ctx);
		}
	} else if (!ctx->async_mode)
		mtk_enc_put_buf(ctx);

	v4l2_m2m_job_finish(ctx->dev->m2m_dev_enc, ctx->m2m_ctx);

	mtk_v4l2_debug(1, "<=== src_buf[%d] dst_buf[%d] venc_if_encode ret=%d Size=%u===>",
			src_buf->index, dst_buf->index, ret,
			enc_result.bs_size);

	mutex_unlock(&ctx->worker_lock);
}

static void m2mops_venc_device_run(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;

	mtk_venc_param_change(ctx);

	if ((ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H264 ||
	     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H265 ||
	     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_HEIF ||
	     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_MPEG4 ||
	     ctx->q_data[MTK_Q_DATA_DST].fmt->fourcc == V4L2_PIX_FMT_H263) &&
	    (ctx->state != MTK_STATE_HEADER)) {
		/* encode h264 sps/pps header */
		mtk_venc_encode_header(ctx);
		queue_work(ctx->dev->encode_workqueue, &ctx->encode_work);
		return;
	}

	queue_work(ctx->dev->encode_workqueue, &ctx->encode_work);
}

static int m2mops_venc_job_ready(void *m2m_priv)
{
	struct mtk_vcodec_ctx *ctx = m2m_priv;

	if (ctx->state == MTK_STATE_ABORT || ctx->state == MTK_STATE_FREE) {
		mtk_v4l2_debug(4, "[%d]Not ready: state=0x%x.",
			       ctx->id, ctx->state);
		return 0;
	}

	return 1;
}

static void m2mops_venc_job_abort(void *priv)
{
	struct mtk_vcodec_ctx *ctx = priv;

	mtk_v4l2_debug(4, "[%d]", ctx->id);
	ctx->state = MTK_STATE_ABORT;
}

const struct v4l2_m2m_ops mtk_venc_m2m_ops = {
	.device_run     = m2mops_venc_device_run,
	.job_ready      = m2mops_venc_job_ready,
	.job_abort      = m2mops_venc_job_abort
};

void mtk_vcodec_enc_set_default_params(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_q_data *q_data;

	ctx->m2m_ctx->q_lock = &ctx->dev->dev_mutex;
	ctx->fh.m2m_ctx = ctx->m2m_ctx;
	ctx->fh.ctrl_handler = &ctx->ctrl_hdl;
	INIT_WORK(&ctx->encode_work, mtk_venc_worker);

	ctx->colorspace = V4L2_COLORSPACE_REC709;
	ctx->ycbcr_enc = V4L2_YCBCR_ENC_DEFAULT;
	ctx->quantization = V4L2_QUANTIZATION_DEFAULT;
	ctx->xfer_func = V4L2_XFER_FUNC_DEFAULT;

	get_supported_format(ctx);

	q_data = &ctx->q_data[MTK_Q_DATA_SRC];
	memset(q_data, 0, sizeof(struct mtk_q_data));
	q_data->visible_width = DFT_CFG_WIDTH;
	q_data->visible_height = DFT_CFG_HEIGHT;
	q_data->coded_width = DFT_CFG_WIDTH;
	q_data->coded_height = DFT_CFG_HEIGHT;
	q_data->field = V4L2_FIELD_NONE;

	q_data->fmt = &mtk_video_formats[default_out_fmt_idx];

	v4l_bound_align_image(&q_data->coded_width,
			      MTK_VENC_MIN_W,
			      MTK_VENC_MAX_W, 4,
			      &q_data->coded_height,
			      MTK_VENC_MIN_H,
			      MTK_VENC_MAX_H, 5, 6);

	if (q_data->coded_width < DFT_CFG_WIDTH &&
	    (q_data->coded_width + 16) <= MTK_VENC_MAX_W)
		q_data->coded_width += 16;
	if (q_data->coded_height < DFT_CFG_HEIGHT &&
	    (q_data->coded_height + 32) <= MTK_VENC_MAX_H)
		q_data->coded_height += 32;

	q_data->sizeimage[0] =
		q_data->coded_width * q_data->coded_height +
		((ALIGN(q_data->coded_width, 16) * 2) * 16);
	q_data->bytesperline[0] = q_data->coded_width;
	q_data->sizeimage[1] =
		(q_data->coded_width * q_data->coded_height) / 2 +
		(ALIGN(q_data->coded_width, 16) * 16);
	q_data->bytesperline[1] = q_data->coded_width;

	q_data = &ctx->q_data[MTK_Q_DATA_DST];
	memset(q_data, 0, sizeof(struct mtk_q_data));
	q_data->coded_width = DFT_CFG_WIDTH;
	q_data->coded_height = DFT_CFG_HEIGHT;
	q_data->fmt = &mtk_video_formats[default_cap_fmt_idx];
	q_data->field = V4L2_FIELD_NONE;
	ctx->q_data[MTK_Q_DATA_DST].sizeimage[0] =
		DFT_CFG_WIDTH * DFT_CFG_HEIGHT;
	ctx->q_data[MTK_Q_DATA_DST].bytesperline[0] = 0;

}

static const struct v4l2_ctrl_config mtk_enc_color_desc_ctrl = {
	.ops = &mtk_vcodec_enc_ctrl_ops,
	.id = V4L2_CID_MPEG_MTK_COLOR_DESC,
	.name = "Video encode color description for HDR",
	.type = V4L2_CTRL_TYPE_U32,
	.flags = V4L2_CTRL_FLAG_WRITE_ONLY,
	.min = 0x00000000,
	.max = 0xffffffff,
	.step = 1,
	.def = 0,
	.dims = { sizeof(struct mtk_color_desc)/sizeof(u32) }
};

int mtk_vcodec_enc_ctrls_setup(struct mtk_vcodec_ctx *ctx)
{
	const struct v4l2_ctrl_ops *ops = &mtk_vcodec_enc_ctrl_ops;
	struct v4l2_ctrl_handler *handler = &ctx->ctrl_hdl;
	struct v4l2_ctrl_config cfg;
	struct v4l2_ctrl *ctrl;

	v4l2_ctrl_handler_init(handler, MTK_MAX_CTRLS_HINT);

	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_BITRATE,
			  0, 400000000, 1, 20000000);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_MTK_SEC_ENCODE,
			  0, 2, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_B_FRAMES,
			  0, 3, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
			  0, 1, 1, 1);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
			  0, 51, 1, 51);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_H264_I_PERIOD,
			  0, 65535, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_GOP_SIZE,
			  0, 65535, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE,
			  0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
			  0, 1, 1, 0);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP,
			  0, 51, 1, 51);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP,
			  0, 51, 1, 51);
	v4l2_ctrl_new_std(handler, ops, V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP,
			  0, 51, 1, 51);
	v4l2_ctrl_new_std_menu(handler, ops,
		V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		0, V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		0, V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		0, V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		0, V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		V4L2_MPEG_VIDEO_H264_LEVEL_4_2,
		0, V4L2_MPEG_VIDEO_H264_LEVEL_1_0);
	v4l2_ctrl_new_std_menu(handler, ops,
		V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		V4L2_MPEG_VIDEO_HEVC_LEVEL_4,
		0, V4L2_MPEG_VIDEO_HEVC_LEVEL_4);
	v4l2_ctrl_new_std_menu(handler, ops,
		V4L2_CID_MPEG_VIDEO_HEVC_TIER,
		V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		0, V4L2_MPEG_VIDEO_HEVC_TIER_MAIN);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
		V4L2_MPEG_VIDEO_MPEG4_LEVEL_5,
		0, V4L2_MPEG_VIDEO_MPEG4_LEVEL_0);
	v4l2_ctrl_new_std_menu(handler, ops, V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		V4L2_MPEG_VIDEO_BITRATE_MODE_CQ,
		0, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_SCENARIO;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode scenario";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_NONREFP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode nonrefp";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode detect framerate";
	cfg.min = 0;
	cfg.max = 32;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_RFS_ON;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode slice loss indication";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode slice loss indication";
	cfg.min = 0;
	cfg.max = 1;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode operation rate";
	cfg.min = 0;
	cfg.max = 2048;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_ROI_ON;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode roi switch";
	cfg.min = 0;
	cfg.max = 8;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode heif grid size";
	cfg.min = 0;
	cfg.max = (3840<<16)+2176;
	cfg.step = 16;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	ctrl = v4l2_ctrl_new_custom(&ctx->ctrl_hdl,
		&mtk_enc_color_desc_ctrl, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_MAX_WIDTH;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode max width";
	cfg.min = 0;
	cfg.max = 3840;
	cfg.step = 16;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_MAX_HEIGHT;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_WRITE_ONLY;
	cfg.name = "Video encode max height";
	cfg.min = 0;
	cfg.max = 3840;
	cfg.step = 16;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	/* g_volatile_ctrl */
	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_ENCODE_ROI_RC_QP;
	cfg.type = V4L2_CTRL_TYPE_INTEGER;
	cfg.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_READ_ONLY;
	cfg.name = "Video encode roi rc qp";
	cfg.min = 0;
	cfg.max = 2048;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	memset(&cfg, 0, sizeof(cfg));
	cfg.id = V4L2_CID_MPEG_MTK_RESOLUTION_CHANGE;
	cfg.type = V4L2_CTRL_TYPE_U32;
	cfg.flags = V4L2_CTRL_FLAG_VOLATILE |
		V4L2_CTRL_FLAG_READ_ONLY;
	cfg.name = "Video encode resolution change";
	cfg.min = 0x00000000;
	cfg.max = 0x00ffffff;
	cfg.step = 1;
	cfg.def = 0;
	cfg.ops = ops;
	cfg.dims[0] = sizeof(struct venc_resolution_change)/sizeof(u32);
	ctrl = v4l2_ctrl_new_custom(handler, &cfg, NULL);

	if (handler->error) {
		mtk_v4l2_err("Init control handler fail %d",
			     handler->error);
		return handler->error;
	}

	v4l2_ctrl_handler_setup(&ctx->ctrl_hdl);

	return 0;
}

#ifdef CONFIG_VB2_MEDIATEK_DMA
static int venc_dc_ion_map_dmabuf(void *mem_priv)
{
	return mtk_dma_contig_memops.map_dmabuf(mem_priv);
}
#endif

int mtk_vcodec_enc_queue_init(void *priv, struct vb2_queue *src_vq,
			      struct vb2_queue *dst_vq)
{
	struct mtk_vcodec_ctx *ctx = priv;
	int ret;

	/* Note: VB2_USERPTR works with dma-contig because mt8173
	 * support iommu
	 * https://patchwork.kernel.org/patch/8335461/
	 * https://patchwork.kernel.org/patch/7596181/
	 */
	src_vq->type            = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	src_vq->io_modes        = VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	src_vq->drv_priv        = ctx;
	src_vq->buf_struct_size = sizeof(struct mtk_video_enc_buf);
	src_vq->ops             = &mtk_venc_vb2_ops;
#ifdef CONFIG_VB2_MEDIATEK_DMA
	venc_ion_dma_contig_memops = mtk_dma_contig_memops;
	venc_ion_dma_contig_memops.map_dmabuf = venc_dc_ion_map_dmabuf;

	src_vq->mem_ops         = &venc_ion_dma_contig_memops;
	mtk_v4l2_debug(4, "src_vq use mtk_dma_contig_memops");
#else
	src_vq->mem_ops         = &vb2_dma_contig_memops;
	mtk_v4l2_debug(4, "src_vq use vb2_dma_contig_memops");
#endif
	src_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	src_vq->lock            = &ctx->dev->dev_mutex;
	src_vq->allow_zero_bytesused = 1;
	src_vq->dev             = &ctx->dev->plat_dev->dev;

	ret = vb2_queue_init(src_vq);
	if (ret)
		return ret;

	dst_vq->type            = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	dst_vq->io_modes        = VB2_DMABUF | VB2_MMAP | VB2_USERPTR;
	dst_vq->drv_priv        = ctx;
	dst_vq->buf_struct_size = sizeof(struct mtk_video_enc_buf);
	dst_vq->ops             = &mtk_venc_vb2_ops;
#ifdef CONFIG_VB2_MEDIATEK_DMA
	dst_vq->mem_ops         = &venc_ion_dma_contig_memops;
	mtk_v4l2_debug(4, "dst_vq use mtk_dma_contig_memops");
#else
	dst_vq->mem_ops         = &vb2_dma_contig_memops;
	mtk_v4l2_debug(4, "dst_vq use vb2_dma_contig_memops");
#endif
	dst_vq->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	dst_vq->lock            = &ctx->dev->dev_mutex;
	dst_vq->allow_zero_bytesused = 1;
	dst_vq->dev             = &ctx->dev->plat_dev->dev;

	return vb2_queue_init(dst_vq);
}

void mtk_venc_unlock(struct mtk_vcodec_ctx *ctx, u32 hw_id)
{

	if (hw_id >= MTK_VENC_HW_NUM)
		return;

	mtk_v4l2_debug(4, "ctx %p [%d] hw_id %d sem_cnt %d",
		ctx, ctx->id, hw_id, ctx->dev->enc_sem[hw_id].count);

	if (hw_id < MTK_VENC_HW_NUM)
		up(&ctx->dev->enc_sem[hw_id]);
}

void mtk_venc_lock(struct mtk_vcodec_ctx *ctx, u32 hw_id)
{
	unsigned int suspend_block_cnt = 0;
	int ret = -1;

	if (hw_id >= MTK_VENC_HW_NUM)
		return;

	while (ctx->dev->is_codec_suspending == 1) {
		suspend_block_cnt++;
		if (suspend_block_cnt > SUSPEND_TIMEOUT_CNT) {
			mtk_v4l2_debug(4, "VENC blocked by suspend\n");
			suspend_block_cnt = 0;
		}
		usleep_range(10000, 20000);
	}

	mtk_v4l2_debug(4, "ctx %p [%d] hw_id %d sem_cnt %d",
		ctx, ctx->id, hw_id, ctx->dev->enc_sem[hw_id].count);

	while (hw_id < MTK_VENC_HW_NUM && ret != 0
		&& !ctx->lock_abort)
		ret = down_interruptible(&ctx->dev->enc_sem[hw_id]);
}

void mtk_vcodec_enc_empty_queues(struct file *file, struct mtk_vcodec_ctx *ctx)
{
	struct vb2_buffer *src_buf = NULL, *dst_buf = NULL;
	struct v4l2_fh *fh = file->private_data;

	// error handle for release before stream-off
	//  stream off both queue mannually.
	v4l2_m2m_streamoff(file, fh->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
	v4l2_m2m_streamoff(file, fh->m2m_ctx,
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

	while ((src_buf = v4l2_m2m_src_buf_remove(ctx->m2m_ctx)))
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(src_buf),
			VB2_BUF_STATE_ERROR);

	while ((dst_buf = v4l2_m2m_dst_buf_remove(ctx->m2m_ctx))) {
		dst_buf->planes[0].bytesused = 0;
		v4l2_m2m_buf_done(to_vb2_v4l2_buffer(dst_buf),
			VB2_BUF_STATE_ERROR);
	}

	ctx->state = MTK_STATE_FREE;
}

void mtk_vcodec_enc_release(struct mtk_vcodec_ctx *ctx)
{
	int ret = venc_if_deinit(ctx);

	if (ret)
		mtk_v4l2_err("venc_if_deinit failed=%d", ret);
}
