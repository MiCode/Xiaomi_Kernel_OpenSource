/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"

#define MSM_VDEC_DVC_NAME "msm_vdec_8974"
#define MIN_NUM_OUTPUT_BUFFERS 4
#define MIN_NUM_CAPTURE_BUFFERS 6
#define MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS 1
#define MAX_NUM_OUTPUT_BUFFERS VB2_MAX_FRAME
#define DEFAULT_VIDEO_CONCEAL_COLOR_BLACK 0x8010
#define MB_SIZE_IN_PIXEL (16 * 16)
#define MAX_OPERATING_FRAME_RATE (300 << 16)
#define OPERATING_FRAME_RATE_STEP (1 << 16)

static const char *const mpeg_video_vidc_divx_format[] = {
	"DIVX Format 3",
	"DIVX Format 4",
	"DIVX Format 5",
	"DIVX Format 6",
	NULL
};
static const char *const mpeg_video_stream_format[] = {
	"NAL Format Start Codes",
	"NAL Format One NAL Per Buffer",
	"NAL Format One Byte Length",
	"NAL Format Two Byte Length",
	"NAL Format Four Byte Length",
	NULL
};
static const char *const mpeg_video_output_order[] = {
	"Display Order",
	"Decode Order",
	NULL
};
static const char *const mpeg_vidc_video_alloc_mode_type[] = {
	"Buffer Allocation Static",
	"Buffer Allocation Dynamic Buffer"
};

static const char *const perf_level[] = {
	"Nominal",
	"Performance",
	"Turbo"
};

static const char *const h263_level[] = {
	"1.0",
	"2.0",
	"3.0",
	"4.0",
	"4.5",
	"5.0",
	"6.0",
	"7.0",
};

static const char *const h263_profile[] = {
	"Baseline",
	"H320 Coding",
	"Backward Compatible",
	"ISWV2",
	"ISWV3",
	"High Compression",
	"Internet",
	"Interlace",
	"High Latency",
};

static const char *const vp8_profile_level[] = {
	"Unused",
	"0.0",
	"1.0",
	"2.0",
	"3.0",
};

static const char *const mpeg2_profile[] = {
	"Simple",
	"Main",
	"422",
	"Snr Scalable",
	"Spatial Scalable",
	"High",
};

static const char *const mpeg2_level[] = {
	"0",
	"1",
	"2",
	"3",
};
static const char *const mpeg_vidc_video_entropy_mode[] = {
	"CAVLC Entropy Mode",
	"CABAC Entropy Mode",
};

static const char *const mpeg_vidc_video_h264_mvc_layout[] = {
	"Frame packing arrangement sequential",
	"Frame packing arrangement top-bottom",
};

static const char *const mpeg_vidc_video_dpb_color_format[] = {
	"DPB Color Format None",
	"DPB Color Format UBWC",
	"DPB Color Format UBWC TP10",
};

static struct msm_vidc_ctrl msm_vdec_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT,
		.name = "NAL Format",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.maximum = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH,
		.default_value = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_ONE_NAL_PER_BUFFER) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_ONE_BYTE_LENGTH) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_TWO_BYTE_LENGTH) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH)
		),
		.qmenu = mpeg_video_stream_format,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER,
		.name = "Output Order",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY,
		.maximum = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DISPLAY) |
			(1 << V4L2_MPEG_VIDC_VIDEO_OUTPUT_ORDER_DECODE)
			),
		.qmenu = mpeg_video_output_order,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE,
		.name = "Picture Type Decoding",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE,
		.name = "Sync Frame Decode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_ENABLE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SECURE,
		.name = "Secure mode",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 0,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA,
		.name = "Extradata Type",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.maximum = V4L2_MPEG_VIDC_EXTRADATA_VPX_COLORSPACE,
		.default_value = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NONE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_BITS_INFO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VQZIP_SEI) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_OUTPUT_CROP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_DISPLAY_COLOUR_SEI) |
			(1 <<
			V4L2_MPEG_VIDC_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VUI_DISPLAY) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VPX_COLORSPACE)
			),
		.qmenu = mpeg_video_vidc_extradata,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE,
		.name = "Video decoder multi stream",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum =
			V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY,
		.maximum =
			V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY,
		.default_value =
			V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY,
		.menu_skip_mask = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.name = "H264 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.name = "H264 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_5_2,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
		.name = "VP8 Profile Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED) |
			(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0) |
			(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1)
		),
		.qmenu = vp8_profile_level,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE,
		.name = "MPEG2 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_HIGH,
		.default_value = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_MAIN) |
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_422) |
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SNR_SCALABLE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SPATIAL_SCALABLE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_HIGH)
		),
		.qmenu = mpeg2_profile,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL,
		.name = "MPEG2 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0,
		.maximum = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_3,
		.default_value = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0) |
			(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_1) |
			(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_2) |
			(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_3)
		),
		.qmenu = mpeg2_level,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR,
		.name = "Picture concealed color",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0x0,
		.maximum = 0xffffff,
		.default_value = DEFAULT_VIDEO_CONCEAL_COLOR_BLACK,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BUFFER_SIZE_LIMIT,
		.name = "Buffer size limit",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_DPB_COLOR_FORMAT,
		.name = "Video decoder dpb color format",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_NONE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_TP10_UBWC,
		.default_value = V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_NONE,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_NONE) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_UBWC) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_TP10_UBWC)
			),
		.qmenu = mpeg_vidc_video_dpb_color_format,
	},
	{
		.id = V4L2_CID_VIDC_QBUF_MODE,
		.name = "Allows batching of buffers for power savings",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_VIDC_QBUF_STANDARD,
		.maximum = V4L2_VIDC_QBUF_BATCHED,
		.default_value = V4L2_VIDC_QBUF_STANDARD,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
		.name = "Entropy Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
		.maximum = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
		.default_value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC) |
		(1 << V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC)
		),
		.qmenu = mpeg_vidc_video_entropy_mode,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY,
		.name = "Session Priority",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_ENABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE,
		.name = "Set Decoder Operating rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_OPERATING_FRAME_RATE,
		.default_value = 0,
		.step = OPERATING_FRAME_RATE_STEP,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)

static u32 get_frame_size_nv12(int plane,
					u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height);
}

static u32 get_frame_size_nv12_ubwc(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12_UBWC, width, height);
}

static u32 get_frame_size_compressed_full_yuv(int plane,
					u32 max_mbs_per_frame, u32 size_per_mb)
{
	return (max_mbs_per_frame * size_per_mb * 3 / 2);
}

static u32 get_frame_size_compressed(int plane,
					u32 max_mbs_per_frame, u32 size_per_mb)
{
	return (max_mbs_per_frame * size_per_mb * 3/2)/2;
}

static u32 get_frame_size_nv12_ubwc_10bit(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12_BPP10_UBWC, width, height);
}

static u32 get_frame_size(struct msm_vidc_inst *inst,
					const struct msm_vidc_format *fmt,
					int fmt_type, int plane)
{
	u32 frame_size = 0;

	if (fmt_type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		frame_size = fmt->get_frame_size(plane,
					inst->capability.mbs_per_frame.max,
					MB_SIZE_IN_PIXEL);
		if (inst->buffer_size_limit &&
			(inst->buffer_size_limit < frame_size)) {
			frame_size = inst->buffer_size_limit;
			dprintk(VIDC_DBG, "input buffer size limited to %d\n",
				frame_size);
		} else {
			dprintk(VIDC_DBG, "set input buffer size to %d\n",
				frame_size);
		}
	} else if (fmt_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		frame_size = fmt->get_frame_size(plane,
					inst->capability.height.max,
					inst->capability.width.max);
		dprintk(VIDC_DBG, "set output buffer size to %d\n",
			frame_size);
	} else {
		dprintk(VIDC_WARN, "Wrong format type\n");
	}
	return frame_size;
}

static int is_ctrl_valid_for_codec(struct msm_vidc_inst *inst,
					struct v4l2_ctrl *ctrl)
{
	int rc = 0;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		if (inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_H264_MVC &&
			ctrl->val != V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH) {
			dprintk(VIDC_ERR,
					"Profile %#x not supported for MVC\n",
					ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		if (inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_H264_MVC &&
			ctrl->val >= V4L2_MPEG_VIDEO_H264_LEVEL_5_2) {
			dprintk(VIDC_ERR, "Level %#x not supported for MVC\n",
					ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		break;
	default:
		break;
	}
	return rc;
}

struct msm_vidc_format vdec_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.get_frame_size = get_frame_size_nv12,
		.type = CAPTURE_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
		.num_planes = 2,
		.get_frame_size = get_frame_size_nv12_ubwc,
		.type = CAPTURE_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0 10bit",
		.description = "UBWC Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC,
		.num_planes = 2,
		.get_frame_size = get_frame_size_nv12_ubwc_10bit,
		.type = CAPTURE_PORT,
	},
	{
		.name = "Mpeg4",
		.description = "Mpeg4 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "Mpeg2",
		.description = "Mpeg2 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H263",
		.description = "H263 compressed format",
		.fourcc = V4L2_PIX_FMT_H263,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VC1",
		.description = "VC-1 compressed format",
		.fourcc = V4L2_PIX_FMT_VC1_ANNEX_G,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VC1 SP",
		.description = "VC-1 compressed format G",
		.fourcc = V4L2_PIX_FMT_VC1_ANNEX_L,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H264_MVC",
		.description = "H264_MVC compressed format",
		.fourcc = V4L2_PIX_FMT_H264_MVC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VP9",
		.description = "VP9 compressed format",
		.fourcc = V4L2_PIX_FMT_VP9,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed_full_yuv,
		.type = OUTPUT_PORT,
	},
};

int msm_vdec_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	const struct msm_vidc_format *fmt = NULL;
	struct hfi_device *hdev;
	int rc = 0, i = 0, stride = 0, scanlines = 0, color_format = 0;
	unsigned int *plane_sizes = NULL, extra_idx = 0;

	if (!inst || !f || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, format = %pK\n", inst, f);
		return -EINVAL;
	}

	hdev = inst->core->device;
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		fmt = &inst->fmts[CAPTURE_PORT];
	else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = &inst->fmts[OUTPUT_PORT];
	else
		return -ENOTSUPP;

	f->fmt.pix_mp.pixelformat = fmt->fourcc;
	f->fmt.pix_mp.num_planes = fmt->num_planes;
	if (inst->in_reconfig) {
		inst->prop.height[OUTPUT_PORT] = inst->reconfig_height;
		inst->prop.width[OUTPUT_PORT] = inst->reconfig_width;

		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
					"%s: unsupported session\n", __func__);
			goto exit;
		}
	}

	f->fmt.pix_mp.height = inst->prop.height[CAPTURE_PORT];
	f->fmt.pix_mp.width = inst->prop.width[CAPTURE_PORT];
	stride = inst->prop.width[CAPTURE_PORT];
	scanlines = inst->prop.height[CAPTURE_PORT];

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		plane_sizes = &inst->bufq[OUTPUT_PORT].plane_sizes[0];
		for (i = 0; i < fmt->num_planes; ++i) {
			if (!plane_sizes[i]) {
				f->fmt.pix_mp.plane_fmt[i].sizeimage =
					get_frame_size(inst, fmt, f->type, i);
				plane_sizes[i] = f->fmt.pix_mp.plane_fmt[i].
					sizeimage;
			} else
				f->fmt.pix_mp.plane_fmt[i].sizeimage =
					plane_sizes[i];
		}
		f->fmt.pix_mp.height = inst->prop.height[OUTPUT_PORT];
		f->fmt.pix_mp.width = inst->prop.width[OUTPUT_PORT];
		f->fmt.pix_mp.plane_fmt[0].bytesperline =
			(__u16)inst->prop.width[OUTPUT_PORT];
		f->fmt.pix_mp.plane_fmt[0].reserved[0] =
			(__u16)inst->prop.height[OUTPUT_PORT];
	} else {
		switch (fmt->fourcc) {
		case V4L2_PIX_FMT_NV12:
			color_format = COLOR_FMT_NV12;
			break;
		case V4L2_PIX_FMT_NV12_UBWC:
			color_format = COLOR_FMT_NV12_UBWC;
			break;
		case V4L2_PIX_FMT_NV12_TP10_UBWC:
			color_format = COLOR_FMT_NV12_BPP10_UBWC;
			break;
		default:
			dprintk(VIDC_WARN, "Color format not recognized\n");
			rc = -ENOTSUPP;
			goto exit;
		}

		stride = VENUS_Y_STRIDE(color_format,
				inst->prop.width[CAPTURE_PORT]);
		scanlines = VENUS_Y_SCANLINES(color_format,
				inst->prop.height[CAPTURE_PORT]);

		f->fmt.pix_mp.plane_fmt[0].sizeimage =
			fmt->get_frame_size(0,
			inst->prop.height[CAPTURE_PORT],
			inst->prop.width[CAPTURE_PORT]);

		extra_idx = EXTRADATA_IDX(fmt->num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
				VENUS_EXTRADATA_SIZE(
					inst->prop.height[CAPTURE_PORT],
					inst->prop.width[CAPTURE_PORT]);
		}

		for (i = 0; i < fmt->num_planes; ++i)
			inst->bufq[CAPTURE_PORT].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;

		f->fmt.pix_mp.height = inst->prop.height[CAPTURE_PORT];
		f->fmt.pix_mp.width = inst->prop.width[CAPTURE_PORT];
		f->fmt.pix_mp.plane_fmt[0].bytesperline =
			(__u16)stride;
		f->fmt.pix_mp.plane_fmt[0].reserved[0] =
			(__u16)scanlines;
	}

exit:
	return rc;
}

int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	struct msm_vidc_format *fmt = NULL;
	struct hal_frame_size frame_sz;
	unsigned int extra_idx = 0;
	int rc = 0;
	int ret = 0;
	int i;
	int max_input_size = 0;

	if (!inst || !f) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->fmt.pix_mp.pixelformat,
			CAPTURE_PORT);
		if (!fmt || fmt->type != CAPTURE_PORT) {
			dprintk(VIDC_ERR,
				"Format: %d not supported on CAPTURE port\n",
				f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}
		memcpy(&inst->fmts[fmt->type], fmt,
				sizeof(struct msm_vidc_format));

		inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		msm_comm_set_color_format(inst,
				msm_comm_get_hal_output_buffer(inst),
				f->fmt.pix_mp.pixelformat);

		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
			frame_sz.buffer_type = HAL_BUFFER_OUTPUT2;
			frame_sz.width = inst->prop.width[CAPTURE_PORT];
			frame_sz.height = inst->prop.height[CAPTURE_PORT];
			dprintk(VIDC_DBG,
				"buffer type = %d width = %d, height = %d\n",
				frame_sz.buffer_type, frame_sz.width,
				frame_sz.height);
			ret = msm_comm_try_set_prop(inst,
				HAL_PARAM_FRAME_SIZE, &frame_sz);
		}

		f->fmt.pix_mp.plane_fmt[0].sizeimage =
			inst->fmts[fmt->type].get_frame_size(0,
			f->fmt.pix_mp.height, f->fmt.pix_mp.width);

		extra_idx = EXTRADATA_IDX(inst->fmts[fmt->type].num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
				VENUS_EXTRADATA_SIZE(
					inst->prop.height[CAPTURE_PORT],
					inst->prop.width[CAPTURE_PORT]);
		}

		f->fmt.pix_mp.num_planes = inst->fmts[fmt->type].num_planes;
		for (i = 0; i < inst->fmts[fmt->type].num_planes; ++i) {
			inst->bufq[CAPTURE_PORT].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;

		fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
				ARRAY_SIZE(vdec_formats),
				f->fmt.pix_mp.pixelformat,
				OUTPUT_PORT);
		if (!fmt || fmt->type != OUTPUT_PORT) {
			dprintk(VIDC_ERR,
			"Format: %d not supported on OUTPUT port\n",
			f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}
		memcpy(&inst->fmts[fmt->type], fmt,
				sizeof(struct msm_vidc_format));

		rc = msm_comm_try_state(inst, MSM_VIDC_CORE_INIT_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to initialize instance\n");
			goto err_invalid_fmt;
		}

		if (!(get_hal_codec(inst->fmts[fmt->type].fourcc) &
			inst->core->dec_codec_supported)) {
			dprintk(VIDC_ERR,
				"Codec(%#x) is not present in the supported codecs list(%#x)\n",
				get_hal_codec(inst->fmts[fmt->type].fourcc),
				inst->core->dec_codec_supported);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}

		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
			goto err_invalid_fmt;
		}

		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
		}

		frame_sz.buffer_type = HAL_BUFFER_INPUT;
		frame_sz.width = inst->prop.width[OUTPUT_PORT];
		frame_sz.height = inst->prop.height[OUTPUT_PORT];
		dprintk(VIDC_DBG,
			"buffer type = %d width = %d, height = %d\n",
			frame_sz.buffer_type, frame_sz.width,
			frame_sz.height);
		msm_comm_try_set_prop(inst, HAL_PARAM_FRAME_SIZE, &frame_sz);

		max_input_size = get_frame_size(
inst, &inst->fmts[fmt->type], f->type, 0);
		if (f->fmt.pix_mp.plane_fmt[0].sizeimage > max_input_size ||
			!f->fmt.pix_mp.plane_fmt[0].sizeimage) {
			f->fmt.pix_mp.plane_fmt[0].sizeimage = max_input_size;
		}

		f->fmt.pix_mp.num_planes = inst->fmts[fmt->type].num_planes;
		for (i = 0; i < inst->fmts[fmt->type].num_planes; ++i) {
			inst->bufq[OUTPUT_PORT].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}

	}
err_invalid_fmt:
	return rc;
}

int msm_vdec_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;

	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, f = %pK\n", inst, f);
		return -EINVAL;
	}
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_index(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->index, CAPTURE_PORT);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_index(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->index, OUTPUT_PORT);
		f->flags = V4L2_FMT_FLAG_COMPRESSED;
	}

	memset(f->reserved, 0, sizeof(f->reserved));
	if (fmt) {
		strlcpy(f->description, fmt->description,
				sizeof(f->description));
		f->pixelformat = fmt->fourcc;
	} else {
		dprintk(VIDC_DBG, "No more formats found\n");
		rc = -EINVAL;
	}
	return rc;
}

static int set_actual_buffer_count(struct msm_vidc_inst *inst,
			int count, enum hal_buffer type)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_buffer_count_actual buf_count;

	hdev = inst->core->device;

	buf_count.buffer_type = type;
	buf_count.buffer_count_actual = count;
	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HAL_PARAM_BUFFER_COUNT_ACTUAL, &buf_count);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to set actual buffer count %d for buffer type %d\n",
			count, type);
	return rc;
}

static int msm_vdec_queue_setup(
	struct vb2_queue *q,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[])
{
	int i, rc = 0;
	struct msm_vidc_inst *inst;
	struct hal_buffer_requirements *bufreq;
	int extra_idx = 0;
	int min_buff_count = 0;

	if (!q || !num_buffers || !num_planes
		|| !sizes || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK, %pK, %pK\n",
			q, num_buffers, num_planes);
		return -EINVAL;
	}
	inst = q->drv_priv;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR,
				"%s: Failed : Buffer requirements\n", __func__);
		goto exit;
	}

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = inst->fmts[OUTPUT_PORT].num_planes;
		if (*num_buffers < MIN_NUM_OUTPUT_BUFFERS ||
				*num_buffers > MAX_NUM_OUTPUT_BUFFERS)
			*num_buffers = MIN_NUM_OUTPUT_BUFFERS;
		for (i = 0; i < *num_planes; i++) {
			sizes[i] = get_frame_size(inst,
					&inst->fmts[OUTPUT_PORT], q->type, i);
		}
		rc = set_actual_buffer_count(inst, *num_buffers,
			HAL_BUFFER_INPUT);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		dprintk(VIDC_DBG, "Getting bufreqs on capture plane\n");
		*num_planes = inst->fmts[CAPTURE_PORT].num_planes;
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
			break;
		}
		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
			break;
		}

		bufreq = get_buff_req_buffer(inst,
			msm_comm_get_hal_output_buffer(inst));
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"No buffer requirement for buffer type %x\n",
				HAL_BUFFER_OUTPUT);
			rc = -EINVAL;
			break;
		}

		/* Pretend as if FW itself is asking for
		 * additional buffers.
		 * *num_buffers += MSM_VIDC_ADDITIONAL_BUFS_FOR_DCVS
		 * is wrong since it will end up increasing the count
		 * on every call to reqbufs if *num_bufs is larger
		 * than min requirement.
		 */
		*num_buffers = max(*num_buffers, bufreq->buffer_count_min
			+ msm_dcvs_get_extra_buff_count(inst));

		min_buff_count = (!!(inst->flags & VIDC_THUMBNAIL)) ?
			MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS :
				MIN_NUM_CAPTURE_BUFFERS;

		*num_buffers = clamp_val(*num_buffers,
			min_buff_count, VB2_MAX_FRAME);

		dprintk(VIDC_DBG, "Set actual output buffer count: %d\n",
				*num_buffers);
		rc = set_actual_buffer_count(inst, *num_buffers,
					msm_comm_get_hal_output_buffer(inst));
		if (rc)
			break;

		if (*num_buffers != bufreq->buffer_count_actual) {
			rc = msm_comm_try_get_bufreqs(inst);
			if (rc) {
				dprintk(VIDC_WARN,
					"Failed to get buf req, %d\n", rc);
				break;
			}
		}
		dprintk(VIDC_DBG, "count =  %d, size = %d, alignment = %d\n",
				inst->buff_req.buffer[1].buffer_count_actual,
				inst->buff_req.buffer[1].buffer_size,
				inst->buff_req.buffer[1].buffer_alignment);
		sizes[0] = inst->bufq[CAPTURE_PORT].plane_sizes[0];

		/*
		 * Set actual buffer count to firmware for DPB buffers.
		 * Firmware mandates setting of minimum buffer size
		 * and actual buffer count for both OUTPUT and OUTPUT2.
		 * Hence we are setting back the same buffer size
		 * information back to firmware.
		 */
		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_OUTPUT);
			if (!bufreq) {
				rc = -EINVAL;
				break;
			}

			rc = set_actual_buffer_count(inst,
				bufreq->buffer_count_actual,
				HAL_BUFFER_OUTPUT);
			if (rc)
				break;
		}

		extra_idx =
			EXTRADATA_IDX(inst->fmts[CAPTURE_PORT].num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			sizes[extra_idx] =
				VENUS_EXTRADATA_SIZE(
					inst->prop.height[CAPTURE_PORT],
					inst->prop.width[CAPTURE_PORT]);
		}
		break;
	default:
		dprintk(VIDC_ERR, "Invalid q type = %d\n", q->type);
		rc = -EINVAL;
	}
exit:
	return rc;
}

static inline int set_max_internal_buffers_size(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct {
		enum hal_buffer type;
		struct hal_buffer_requirements *req;
		size_t size;
	} internal_buffers[] = {
		{ HAL_BUFFER_INTERNAL_SCRATCH, NULL, 0},
		{ HAL_BUFFER_INTERNAL_SCRATCH_1, NULL, 0},
		{ HAL_BUFFER_INTERNAL_SCRATCH_2, NULL, 0},
		{ HAL_BUFFER_INTERNAL_PERSIST, NULL, 0},
		{ HAL_BUFFER_INTERNAL_PERSIST_1, NULL, 0},
	};

	struct hal_frame_size frame_sz;
	int i;

	frame_sz.buffer_type = HAL_BUFFER_INPUT;
	frame_sz.width = inst->capability.width.max;
	frame_sz.height =
		(inst->capability.mbs_per_frame.max * 256) /
		inst->capability.width.max;

	dprintk(VIDC_DBG,
		"Max buffer reqs, buffer type = %d width = %d, height = %d, max_mbs_per_frame = %d\n",
		frame_sz.buffer_type, frame_sz.width,
		frame_sz.height, inst->capability.mbs_per_frame.max);

	msm_comm_try_set_prop(inst, HAL_PARAM_FRAME_SIZE, &frame_sz);
	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s Failed to get max buf req, %d\n", __func__, rc);
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(internal_buffers); i++) {
		internal_buffers[i].req =
			get_buff_req_buffer(inst, internal_buffers[i].type);
		internal_buffers[i].size = internal_buffers[i].req ?
			internal_buffers[i].req->buffer_size : 0;
	}

	frame_sz.buffer_type = HAL_BUFFER_INPUT;
	frame_sz.width = inst->prop.width[OUTPUT_PORT];
	frame_sz.height = inst->prop.height[OUTPUT_PORT];

	msm_comm_try_set_prop(inst, HAL_PARAM_FRAME_SIZE, &frame_sz);
	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s Failed to get back old buf req, %d\n",
			__func__, rc);
		return rc;
	}

	dprintk(VIDC_DBG,
			"Old buffer reqs, buffer type = %d width = %d, height = %d\n",
			frame_sz.buffer_type, frame_sz.width,
			frame_sz.height);

	for (i = 0; i < ARRAY_SIZE(internal_buffers); i++) {
		if (internal_buffers[i].req) {
			internal_buffers[i].req->buffer_size =
				internal_buffers[i].size;
			dprintk(VIDC_DBG,
				"Changing buffer type : %d size to : %zd\n",
				internal_buffers[i].type,
				internal_buffers[i].size);
		}
	}
	return 0;
}

static inline int start_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	bool slave_side_cp = inst->core->resources.slave_side_cp;
	struct hal_buffer_size_minimum b;
	unsigned int buffer_size;
	struct msm_vidc_format *fmt = NULL;

	fmt = &inst->fmts[CAPTURE_PORT];
	buffer_size = fmt->get_frame_size(0,
		inst->prop.height[CAPTURE_PORT],
		inst->prop.width[CAPTURE_PORT]);
	hdev = inst->core->device;

	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_vidc_check_scaling_supported(inst);
		b.buffer_type = HAL_BUFFER_OUTPUT2;
	} else {
		b.buffer_type = HAL_BUFFER_OUTPUT;
	}

	b.buffer_size = buffer_size;
	rc = call_hfi_op(hdev, session_set_property,
		 inst->session, HAL_PARAM_BUFFER_SIZE_MINIMUM,
		 &b);
	if (rc) {
		dprintk(VIDC_ERR, "H/w scaling is not in valid range\n");
		return -EINVAL;
	}
	if ((inst->flags & VIDC_SECURE) && !inst->in_reconfig &&
		!slave_side_cp) {
		rc = set_max_internal_buffers_size(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set max scratch buffer size: %d\n",
				rc);
			goto fail_start;
		}
	}
	rc = msm_comm_set_scratch_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to set scratch buffers: %d\n", rc);
		goto fail_start;
	}
	rc = msm_comm_set_persist_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to set persist buffers: %d\n", rc);
		goto fail_start;
	}

	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_set_output_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set output buffers: %d\n", rc);
			goto fail_start;
		}
	}

	/*
	 * For seq_changed_insufficient, driver should set session_continue
	 * to firmware after the following sequence
	 * - driver raises insufficient event to v4l2 client
	 * - all output buffers have been flushed and freed
	 * - v4l2 client queries buffer requirements and splits/combines OPB-DPB
	 * - v4l2 client sets new set of buffers to firmware
	 * - v4l2 client issues CONTINUE to firmware to resume decoding of
	 *   submitted ETBs.
	 */
	if (inst->in_reconfig) {
		dprintk(VIDC_DBG, "send session_continue after reconfig\n");
		rc = call_hfi_op(hdev, session_continue,
			(void *) inst->session);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s - failed to send session_continue\n",
				__func__);
			goto fail_start;
		}
	}
	inst->in_reconfig = false;

	msm_comm_scale_clocks_and_bus(inst);

	rc = msm_comm_try_state(inst, MSM_VIDC_START_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
		goto fail_start;
	}
	msm_dcvs_init_load(inst);
	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_queue_output_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to queue output buffers: %d\n", rc);
			goto fail_start;
		}
	}

fail_start:
	return rc;
}

static inline int stop_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;

	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
	return rc;
}

static int msm_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	struct hfi_device *hdev;

	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	dprintk(VIDC_DBG, "Streamon called on: %d capability for inst: %pK\n",
		q->type, inst);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (inst->bufq[CAPTURE_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		break;
	default:
		dprintk(VIDC_ERR, "Queue type is not supported: %d\n", q->type);
		rc = -EINVAL;
		goto stream_start_failed;
	}
	if (rc) {
		dprintk(VIDC_ERR,
			"Streamon failed on: %d capability for inst: %pK\n",
			q->type, inst);
		goto stream_start_failed;
	}

	rc = msm_comm_qbuf(inst, NULL);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to commit buffers queued before STREAM_ON to hardware: %d\n",
				rc);
		goto stream_start_failed;
	}

stream_start_failed:
	return rc;
}

static void msm_vdec_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;

	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK\n", q);
		return;
	}

	inst = q->drv_priv;
	dprintk(VIDC_DBG, "Streamoff called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (!inst->bufq[CAPTURE_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (!inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	default:
		dprintk(VIDC_ERR,
			"Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}

	msm_comm_scale_clocks_and_bus(inst);

	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK, cap = %d to state: %d\n",
			inst, q->type, MSM_VIDC_RELEASE_RESOURCES_DONE);
}

static void msm_vdec_buf_queue(struct vb2_buffer *vb)
{
	int rc = msm_comm_qbuf(vb2_get_drv_priv(vb->vb2_queue), vb);

	if (rc)
		dprintk(VIDC_ERR, "Failed to queue buffer: %d\n", rc);
}

static void msm_vdec_buf_cleanup(struct vb2_buffer *vb)
{
	int rc = 0;
	struct buf_queue *q = NULL;
	struct msm_vidc_inst *inst = NULL;

	if (!vb) {
		dprintk(VIDC_ERR, "%s : Invalid vb pointer %pK",
			__func__, vb);
		return;
	}

	inst = vb2_get_drv_priv(vb->vb2_queue);
	if (!inst) {
		dprintk(VIDC_ERR, "%s : Invalid inst pointer",
			__func__);
		return;
	}

	q = msm_comm_get_vb2q(inst, vb->type);
	if (!q) {
		dprintk(VIDC_ERR,
			"%s : Failed to find buffer queue for type = %d\n",
				__func__, vb->type);
		return;
	}

	if (q->vb2_bufq.streaming) {
		dprintk(VIDC_DBG, "%d PORT is streaming\n",
			vb->type);
		return;
	}

	rc = msm_vidc_release_buffers(inst, vb->type);
	if (rc)
		dprintk(VIDC_ERR, "%s : Failed to release buffers : %d\n",
			__func__, rc);
}

static const struct vb2_ops msm_vdec_vb2q_ops = {
	.queue_setup = msm_vdec_queue_setup,
	.start_streaming = msm_vdec_start_streaming,
	.buf_queue = msm_vdec_buf_queue,
	.buf_cleanup = msm_vdec_buf_cleanup,
	.stop_streaming = msm_vdec_stop_streaming,
};

const struct vb2_ops *msm_vdec_get_vb2q_ops(void)
{
	return &msm_vdec_vb2q_ops;
}

int msm_vdec_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "Invalid input = %pK\n", inst);
		return -EINVAL;
	}
	inst->prop.height[CAPTURE_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[CAPTURE_PORT] = DEFAULT_WIDTH;
	inst->prop.height[OUTPUT_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[OUTPUT_PORT] = DEFAULT_WIDTH;
	inst->capability.height.min = MIN_SUPPORTED_HEIGHT;
	inst->capability.height.max = DEFAULT_HEIGHT;
	inst->capability.width.min = MIN_SUPPORTED_WIDTH;
	inst->capability.width.max = DEFAULT_WIDTH;
	inst->capability.secure_output2_threshold.min = 0;
	inst->capability.secure_output2_threshold.max = 0;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_DYNAMIC;
	inst->prop.fps = DEFAULT_FPS;
	inst->operating_rate = 0;
	memcpy(&inst->fmts[OUTPUT_PORT], &vdec_formats[2],
			sizeof(struct msm_vidc_format));
	memcpy(&inst->fmts[CAPTURE_PORT], &vdec_formats[0],
			sizeof(struct msm_vidc_format));
	return rc;
}

static struct v4l2_ctrl *get_ctrl_from_cluster(int id,
		struct v4l2_ctrl **cluster, int ncontrols)
{
	int c;

	for (c = 0; c < ncontrols; ++c)
		if (cluster[c]->id == id)
			return cluster[c];
	return NULL;
}

static int vdec_v4l2_to_hal(int id, int value)
{
	switch (id) {
	/* H264 */
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			return HAL_H264_PROFILE_BASELINE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
			return HAL_H264_PROFILE_CONSTRAINED_BASE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH:
			return HAL_H264_PROFILE_CONSTRAINED_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			return HAL_H264_PROFILE_MAIN;
		case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
			return HAL_H264_PROFILE_EXTENDED;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
			return HAL_H264_PROFILE_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10:
			return HAL_H264_PROFILE_HIGH10;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422:
			return HAL_H264_PROFILE_HIGH422;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE:
			return HAL_H264_PROFILE_HIGH444;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_0:
			return HAL_H264_LEVEL_1;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1B:
			return HAL_H264_LEVEL_1b;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_1:
			return HAL_H264_LEVEL_11;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_2:
			return HAL_H264_LEVEL_12;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_3:
			return HAL_H264_LEVEL_13;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_0:
			return HAL_H264_LEVEL_2;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_1:
			return HAL_H264_LEVEL_21;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_2:
			return HAL_H264_LEVEL_22;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_0:
			return HAL_H264_LEVEL_3;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_1:
			return HAL_H264_LEVEL_31;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_2:
			return HAL_H264_LEVEL_32;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_0:
			return HAL_H264_LEVEL_4;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_1:
			return HAL_H264_LEVEL_41;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_2:
			return HAL_H264_LEVEL_42;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_0:
			return HAL_H264_LEVEL_5;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_1:
			return HAL_H264_LEVEL_51;
		default:
			goto unknown_value;
		}
	}
unknown_value:
	dprintk(VIDC_WARN, "Unknown control (%x, %d)\n", id, value);
	return -EINVAL;
}

int msm_vdec_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_nal_stream_format_supported stream_format;
	struct hal_enable_picture enable_picture;
	struct hal_enable hal_property;
	enum hal_property property_id = 0;
	u32 property_val = 0;
	void *pdata = NULL;
	struct hfi_device *hdev;
	struct hal_extradata_enable extra;
	struct hal_multi_stream multi_stream;
	struct v4l2_ctrl *temp_ctrl = NULL;
	struct hal_profile_level profile_level;
	struct hal_frame_size frame_sz;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = is_ctrl_valid_for_codec(inst, ctrl);
	if (rc)
		return rc;

	/* Small helper macro for quickly getting a control and err checking */
#define TRY_GET_CTRL(__ctrl_id) ({ \
		struct v4l2_ctrl *__temp; \
		__temp = get_ctrl_from_cluster( \
			__ctrl_id, \
			ctrl->cluster, ctrl->ncontrols); \
		if (!__temp) { \
			dprintk(VIDC_ERR, "Can't find %s (%x) in cluster\n", \
				#__ctrl_id, __ctrl_id); \
			/* Clusters are hardcoded, if we can't find */ \
			/* something then things are massively screwed up */ \
			MSM_VIDC_ERROR(1); \
		} \
		__temp; \
	})

	v4l2_ctrl_unlock(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
		property_id = HAL_PARAM_NAL_STREAM_FORMAT_SELECT;
		stream_format.nal_stream_format_supported = BIT(ctrl->val);
		pdata = &stream_format;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
		property_id = HAL_PARAM_VDEC_OUTPUT_ORDER;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE:
		property_id = HAL_PARAM_VDEC_PICTURE_TYPE_DECODE;
		if (ctrl->val ==
			V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_ON)
			enable_picture.picture_type = HAL_PICTURE_I;
		else
			enable_picture.picture_type = HAL_PICTURE_I |
				HAL_PICTURE_P | HAL_PICTURE_B |
				HAL_PICTURE_IDR;
		pdata = &enable_picture;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE:
			inst->flags &= ~VIDC_THUMBNAIL;
			break;
		case V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_ENABLE:
			inst->flags |= VIDC_THUMBNAIL;
			break;
		}

		property_id = HAL_PARAM_VDEC_SYNC_FRAME_DECODE;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags |= VIDC_SECURE;
		dprintk(VIDC_DBG, "Setting secure mode to: %d\n",
				!!(inst->flags & VIDC_SECURE));
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		property_id = HAL_PARAM_INDEX_EXTRADATA;
		extra.index = msm_comm_get_hal_extradata_index(ctrl->val);
		extra.enable = 1;
		pdata = &extra;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE:
		if (ctrl->val && !(inst->capability.pixelprocess_capabilities &
				HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY)) {
			dprintk(VIDC_ERR, "Downscaling not supported: %#x\n",
				ctrl->id);
			rc = -ENOTSUPP;
			break;
		}
		switch (ctrl->val) {
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY:
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
			multi_stream.enable = true;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed : Enabling OUTPUT port : %d\n",
					rc);
				break;
			}
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
			multi_stream.enable = false;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed:Disabling OUTPUT2 port : %d\n",
					rc);
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY:
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
			multi_stream.enable = true;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed :Enabling OUTPUT2 port : %d\n",
					rc);
					break;
			}
			multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
			multi_stream.enable = false;
			pdata = &multi_stream;
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_VDEC_MULTI_STREAM,
				pdata);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed disabling OUTPUT port : %d\n",
					rc);
				break;
			}

			frame_sz.buffer_type = HAL_BUFFER_OUTPUT2;
			frame_sz.width = inst->prop.width[CAPTURE_PORT];
			frame_sz.height = inst->prop.height[CAPTURE_PORT];
			pdata = &frame_sz;
			dprintk(VIDC_DBG,
				"buffer type = %d width = %d, height = %d\n",
				frame_sz.buffer_type, frame_sz.width,
				frame_sz.height);
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_FRAME_SIZE, pdata);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed setting OUTPUT2 size : %d\n",
					rc);

			break;
		default:
			dprintk(VIDC_ERR,
				"Failed : Unsupported multi stream setting\n");
			rc = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR:
		property_id = HAL_PARAM_VDEC_CONCEAL_COLOR;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_LEVEL);
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = vdec_v4l2_to_hal(ctrl->id,
				ctrl->val);
		profile_level.level = vdec_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_PROFILE);
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = vdec_v4l2_to_hal(ctrl->id,
				ctrl->val);
		profile_level.profile = vdec_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BUFFER_SIZE_LIMIT:
		dprintk(VIDC_DBG,
			"Limiting input buffer size from %u to %u\n",
			inst->buffer_size_limit, ctrl->val);
		inst->buffer_size_limit = ctrl->val;
		break;
	case V4L2_CID_VIDC_QBUF_MODE:
		property_id = HAL_PARAM_SYNC_BASED_INTERRUPT;
		hal_property.enable = ctrl->val == V4L2_VIDC_QBUF_BATCHED;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
		property_id = HAL_CONFIG_REALTIME;
		/* firmware has inverted values for realtime and
		 * non-realtime priority
		 */
		hal_property.enable = !(ctrl->val);
		pdata = &hal_property;
		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE:
			inst->flags &= ~VIDC_REALTIME;
			break;
		case V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_ENABLE:
			inst->flags |= VIDC_REALTIME;
			break;
		default:
			dprintk(VIDC_WARN,
				"inst(%pK) invalid priority ctrl value %#x\n",
				inst, ctrl->val);
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE:
		dprintk(VIDC_DBG,
			"inst(%pK) operating rate changed from %d to %d\n",
			inst, inst->operating_rate >> 16, ctrl->val >> 16);
		inst->operating_rate = ctrl->val;
		break;
	default:
		break;
	}

	v4l2_ctrl_lock(ctrl);
#undef TRY_GET_CTRL

	if (!rc && property_id) {
		dprintk(VIDC_DBG,
			"Control: HAL property=%#x,ctrl: id=%#x,value=%#x\n",
			property_id, ctrl->id, ctrl->val);
		rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, property_id, pdata);
	}

	return rc;
}

int msm_vdec_s_ext_ctrl(struct msm_vidc_inst *inst,
	struct v4l2_ext_controls *ctrl)
{
	int rc = 0, i = 0, fourcc = 0;
	struct v4l2_ext_control *ext_control;
	struct v4l2_control control;

	if (!inst || !inst->core || !ctrl) {
		dprintk(VIDC_ERR,
			"%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	ext_control = ctrl->controls;
	control.id =
		V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE;

	for (i = 0; i < ctrl->count; i++) {
		switch (ext_control[i].id) {
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE:
			control.value = ext_control[i].value;

			rc = msm_comm_s_ctrl(inst, &control);
			if (rc)
				dprintk(VIDC_ERR,
					"%s Failed setting stream output mode : %d\n",
					__func__, rc);
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_DPB_COLOR_FORMAT:
			switch (ext_control[i].value) {
			case V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_NONE:
				if (!msm_comm_g_ctrl_for_id(inst, control.id)) {
					rc = msm_comm_release_output_buffers(
						inst);
					if (rc)
						dprintk(VIDC_ERR,
							"%s Release output buffers failed\n",
							__func__);
				}
				break;
			case V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_UBWC:
			case V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_TP10_UBWC:
				if (ext_control[i].value ==
					V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_UBWC)
					fourcc = V4L2_PIX_FMT_NV12_UBWC;
				else
					fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC;
				if (msm_comm_g_ctrl_for_id(inst, control.id)) {
					rc = msm_comm_set_color_format(inst,
						HAL_BUFFER_OUTPUT, fourcc);
					if (rc) {
						dprintk(VIDC_ERR,
							"%s Failed setting output color format : %d\n",
							__func__, rc);
						break;
					}
					rc = msm_comm_try_get_bufreqs(inst);
					if (rc)
						dprintk(VIDC_ERR,
							"%s Failed to get buffer requirements : %d\n",
							__func__, rc);
				}
				break;
			default:
				dprintk(VIDC_ERR,
					"%s Unsupported output color format\n",
					__func__);
				rc = -ENOTSUPP;
				break;
			}
			break;
		default:
			dprintk(VIDC_ERR
				, "%s Unsupported set control %d",
				__func__, ext_control[i].id);
			rc = -ENOTSUPP;
			break;
		}
	}

	return rc;
}

int msm_vdec_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops)
{
	return msm_comm_ctrl_init(inst, msm_vdec_ctrls,
		ARRAY_SIZE(msm_vdec_ctrls), ctrl_ops);
}
