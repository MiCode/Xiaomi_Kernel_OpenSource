/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#define MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS MIN_NUM_OUTPUT_BUFFERS
#define MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS MIN_NUM_CAPTURE_BUFFERS
#define MIN_NUM_DEC_OUTPUT_BUFFERS 4
#define MIN_NUM_DEC_CAPTURE_BUFFERS 4
// Y=16(0-9bits), Cb(10-19bits)=Cr(20-29bits)=128, black by default
#define DEFAULT_VIDEO_CONCEAL_COLOR_BLACK 0x8020010
#define MB_SIZE_IN_PIXEL (16 * 16)
#define OPERATING_FRAME_RATE_STEP (1 << 16)
#define MAX_VP9D_INST_COUNT 6
#define MAX_4K_MBPF 38736 /* (4096 * 2304 / 256) */

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

static const char *const vp8_profile_level[] = {
	"Unused",
	"0.0",
	"1.0",
	"2.0",
	"3.0",
};

static const char *const vp9_profile[] = {
	"Unused",
	"0",
	"2_10",
};

static const char *const vp9_level[] = {
	"Unused",
	"1.0",
	"1.1",
	"2.0",
	"2.1",
	"3.0",
	"3.1",
	"4.0",
	"4.1",
	"5.0",
	"5.1",
	"6.0",
	"6.1",
};

static const char *const mpeg2_profile[] = {
	"Simple",
	"Main",
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

static const char *const mpeg_vidc_video_dpb_color_format[] = {
	"DPB Color Format None",
	"DPB Color Format UBWC",
	"DPB Color Format UBWC TP10",
};

static struct msm_vidc_ctrl msm_vdec_ctrls[] = {
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
		.type = V4L2_CTRL_TYPE_BITMASK,
		.minimum = 0,
		.maximum = (V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_I |
				V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_P |
				V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_B),
		.default_value = (V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_I |
				  V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_P |
				  V4L2_MPEG_VIDC_VIDEO_PICTYPE_DECODE_B),
		.step = 0,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE,
		.name = "Sync Frame Decode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
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
		.maximum = V4L2_MPEG_VIDC_EXTRADATA_UBWC_CR_STATS_INFO,
		.default_value = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NONE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_OUTPUT_CROP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_DISPLAY_COLOUR_SEI) |
			(1 <<
			V4L2_MPEG_VIDC_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VUI_DISPLAY) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VPX_COLORSPACE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_UBWC_CR_STATS_INFO)
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
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_6_2,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.menu_skip_mask = (
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN)
		),
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
		.name = "VP8 Profile Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0,
		.menu_skip_mask = 0,
		.qmenu = vp8_profile_level,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP9_PROFILE,
		.name = "VP9 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP9_PROFILE_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP9_PROFILE_P2_10,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP9_PROFILE_P0,
		.menu_skip_mask = 0,
		.qmenu = vp9_profile,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL,
		.name = "VP9 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61,
		.menu_skip_mask = 0,
		.qmenu = vp9_level,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT,
		.name = "Picture concealed color 8bit",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0x0,
		.maximum = 0xff3fcff,
		.default_value = DEFAULT_VIDEO_CONCEAL_COLOR_BLACK,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT,
		.name = "Picture concealed color 10bit",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0x0,
		.maximum = 0x3fffffff,
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
		.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		.name = "CAPTURE Count",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_NUM_CAPTURE_BUFFERS,
		.maximum = MAX_NUM_CAPTURE_BUFFERS,
		.default_value = MIN_NUM_CAPTURE_BUFFERS,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
	},
	{
		.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
		.name = "OUTPUT Count",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_NUM_OUTPUT_BUFFERS,
		.maximum = MAX_NUM_OUTPUT_BUFFERS,
		.default_value = MIN_NUM_OUTPUT_BUFFERS,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
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
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE,
		.name = "Set Decoder Operating rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE,
		.name = "Set Decoder Frame rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE,
		.name = "Low Latency Mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)

static u32 get_frame_size_compressed_full_yuv(int plane,
					u32 max_mbs_per_frame, u32 size_per_mb)
{
	u32 frame_size;

	if (max_mbs_per_frame > MAX_4K_MBPF)
		frame_size = (max_mbs_per_frame * size_per_mb * 3 / 2) / 4;
	else
		frame_size = (max_mbs_per_frame * size_per_mb * 3 / 2);

	/* multiply by 10/8 (1.25) to get size for 10 bit case */
	frame_size = frame_size + (frame_size >> 2);

	return frame_size;
}

static u32 get_frame_size_compressed(int plane,
					u32 max_mbs_per_frame, u32 size_per_mb)
{
	u32 frame_size;

	if (max_mbs_per_frame > MAX_4K_MBPF)
		frame_size = (max_mbs_per_frame * size_per_mb * 3 / 2) / 4;
	else
		frame_size = (max_mbs_per_frame * size_per_mb * 3/2)/2;

	/* multiply by 10/8 (1.25) to get size for 10 bit case */
	frame_size = frame_size + (frame_size >> 2);

	return frame_size;
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
		if (inst->flags & VIDC_SECURE) {
			dprintk(VIDC_DBG,
				"Change secure input buffer size from %u to %u\n",
				frame_size, ALIGN(frame_size/2, SZ_4K));
			frame_size = ALIGN(frame_size/2, SZ_4K);
		}

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

struct msm_vidc_format vdec_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.get_frame_size = get_frame_size_nv12,
		.type = CAPTURE_PORT,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 10bit",
		.description = "Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
		.get_frame_size = get_frame_size_p010,
		.type = CAPTURE_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
		.get_frame_size = get_frame_size_nv12_ubwc,
		.type = CAPTURE_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0 10bit",
		.description = "UBWC Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC,
		.get_frame_size = get_frame_size_tp10_ubwc,
		.type = CAPTURE_PORT,
	},
	{
		.name = "Mpeg2",
		.description = "Mpeg2 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 6,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 8,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 8,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.get_frame_size = get_frame_size_compressed_full_yuv,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 6,
	},
	{
		.name = "VP9",
		.description = "VP9 compressed format",
		.fourcc = V4L2_PIX_FMT_VP9,
		.get_frame_size = get_frame_size_compressed_full_yuv,
		.type = OUTPUT_PORT,
		.defer_outputs = true,
		.input_min_count = 4,
		.output_min_count = 11,
	},
};

struct msm_vidc_format_constraint dec_pix_format_constraints[] = {
	{
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
		.num_planes = 2,
		.y_stride_multiples = 256,
		.y_max_stride = 8192,
		.y_min_plane_buffer_height_multiple = 32,
		.y_buffer_alignment = 256,
		.uv_stride_multiples = 256,
		.uv_max_stride = 8192,
		.uv_min_plane_buffer_height_multiple = 16,
		.uv_buffer_alignment = 256,
	},
};

static bool msm_vidc_check_for_vp9d_overload(struct msm_vidc_core *core)
{
	u32 vp9d_instance_count = 0;
	struct msm_vidc_inst *inst = NULL;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->session_type == MSM_VIDC_DECODER &&
			inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9)
			vp9d_instance_count++;
	}
	mutex_unlock(&core->lock);

	if (vp9d_instance_count > MAX_VP9D_INST_COUNT)
		return true;
	return false;
}

int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	struct msm_vidc_format *fmt = NULL;
	struct msm_vidc_format_constraint *fmt_constraint = NULL;
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

		if (inst->fmts[fmt->type].fourcc == f->fmt.pix_mp.pixelformat &&
			inst->prop.width[CAPTURE_PORT] == f->fmt.pix_mp.width &&
			inst->prop.height[CAPTURE_PORT] ==
				f->fmt.pix_mp.height) {
			dprintk(VIDC_DBG, "No change in CAPTURE port params\n");
			return 0;
		}
		memcpy(&inst->fmts[fmt->type], fmt,
				sizeof(struct msm_vidc_format));

		inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
		}

		msm_comm_set_color_format(inst,
				msm_comm_get_hal_output_buffer(inst),
				f->fmt.pix_mp.pixelformat);

		fmt_constraint =
		msm_comm_get_pixel_fmt_constraints(dec_pix_format_constraints,
			ARRAY_SIZE(dec_pix_format_constraints),
			f->fmt.pix_mp.pixelformat);

		if (!fmt_constraint) {
			dprintk(VIDC_INFO,
				"Format constraint not required for %d on CAPTURE port\n",
				f->fmt.pix_mp.pixelformat);
		} else {
			rc = msm_comm_set_color_format_constraints(inst,
				msm_comm_get_hal_output_buffer(inst),
				fmt_constraint);
			if (rc) {
				dprintk(VIDC_ERR,
					"Set constraint for %d failed on CAPTURE port\n",
					f->fmt.pix_mp.pixelformat);
				rc = -EINVAL;
				goto err_invalid_fmt;
			}
		}

		inst->clk_data.opb_fourcc = f->fmt.pix_mp.pixelformat;
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

		extra_idx = EXTRADATA_IDX(inst->bufq[fmt->type].num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
				VENUS_EXTRADATA_SIZE(
					inst->prop.height[CAPTURE_PORT],
					inst->prop.width[CAPTURE_PORT]);
		}

		f->fmt.pix_mp.num_planes = inst->bufq[fmt->type].num_planes;
		for (i = 0; i < inst->bufq[fmt->type].num_planes; i++) {
			inst->bufq[CAPTURE_PORT].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}

	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {

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

		if (inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9) {
			if (msm_vidc_check_for_vp9d_overload(inst->core)) {
				dprintk(VIDC_ERR, "VP9 Decode overload\n");
				rc = -ENOTSUPP;
				goto err_invalid_fmt;
			}
		}

		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
			goto err_invalid_fmt;
		}

		if (inst->fmts[fmt->type].fourcc == f->fmt.pix_mp.pixelformat &&
			inst->prop.width[OUTPUT_PORT] == f->fmt.pix_mp.width &&
			inst->prop.height[OUTPUT_PORT] ==
				f->fmt.pix_mp.height) {
			dprintk(VIDC_DBG, "No change in OUTPUT port params\n");
			return 0;
		}
		inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;
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

		f->fmt.pix_mp.num_planes = inst->bufq[fmt->type].num_planes;
		for (i = 0; i < inst->bufq[fmt->type].num_planes; ++i) {
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

int msm_vdec_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct msm_vidc_format *fmt = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "Invalid input = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
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
	inst->stream_output_mode = HAL_VIDEO_DECODER_PRIMARY;
	/* To start with, both ports are 1 plane each */
	inst->bufq[OUTPUT_PORT].num_planes = 1;
	inst->bufq[CAPTURE_PORT].num_planes = 1;
	inst->prop.fps = DEFAULT_FPS;
	inst->clk_data.operating_rate = 0;
	if (core->resources.decode_batching)
		inst->batch.size = MAX_DEC_BATCH_SIZE;

	/* By default, initialize CAPTURE port to UBWC YUV format */
	fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
		ARRAY_SIZE(vdec_formats), V4L2_PIX_FMT_NV12_UBWC,
			CAPTURE_PORT);
	if (!fmt || fmt->type != CAPTURE_PORT) {
		dprintk(VIDC_ERR,
			"vdec_formats corrupted\n");
		return -EINVAL;
	}
	memcpy(&inst->fmts[fmt->type], fmt,
			sizeof(struct msm_vidc_format));

	inst->buff_req.buffer[1].buffer_type = HAL_BUFFER_INPUT;
	inst->buff_req.buffer[1].buffer_count_min_host =
	inst->buff_req.buffer[1].buffer_count_actual =
		MIN_NUM_DEC_OUTPUT_BUFFERS;
	inst->buff_req.buffer[2].buffer_type = HAL_BUFFER_OUTPUT;
	inst->buff_req.buffer[2].buffer_count_min_host =
	inst->buff_req.buffer[2].buffer_count_actual =
		MIN_NUM_DEC_CAPTURE_BUFFERS;
	inst->buff_req.buffer[3].buffer_type = HAL_BUFFER_OUTPUT2;
	inst->buff_req.buffer[3].buffer_count_min_host =
	inst->buff_req.buffer[3].buffer_count_actual =
		MIN_NUM_DEC_CAPTURE_BUFFERS;
	inst->buff_req.buffer[4].buffer_type = HAL_BUFFER_EXTRADATA_INPUT;
	inst->buff_req.buffer[5].buffer_type = HAL_BUFFER_EXTRADATA_OUTPUT;
	inst->buff_req.buffer[6].buffer_type = HAL_BUFFER_EXTRADATA_OUTPUT2;
	inst->buff_req.buffer[7].buffer_type = HAL_BUFFER_INTERNAL_SCRATCH;
	inst->buff_req.buffer[8].buffer_type = HAL_BUFFER_INTERNAL_SCRATCH_1;
	inst->buff_req.buffer[9].buffer_type = HAL_BUFFER_INTERNAL_SCRATCH_2;
	inst->buff_req.buffer[10].buffer_type = HAL_BUFFER_INTERNAL_PERSIST;
	inst->buff_req.buffer[11].buffer_type = HAL_BUFFER_INTERNAL_PERSIST_1;
	inst->buff_req.buffer[12].buffer_type = HAL_BUFFER_INTERNAL_CMD_QUEUE;
	inst->buff_req.buffer[13].buffer_type = HAL_BUFFER_INTERNAL_RECON;

	/* By default, initialize OUTPUT port to H264 decoder */
	fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
		ARRAY_SIZE(vdec_formats), V4L2_PIX_FMT_H264,
			OUTPUT_PORT);
	if (!fmt || fmt->type != OUTPUT_PORT) {
		dprintk(VIDC_ERR,
			"vdec_formats corrupted\n");
		return -EINVAL;
	}
	memcpy(&inst->fmts[fmt->type], fmt,
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

int msm_vdec_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0, fourcc = 0;
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
	struct hal_buffer_requirements *bufreq;
	struct hal_buffer_requirements *bufreq_out2;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

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
	case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
		property_id = HAL_PARAM_VDEC_OUTPUT_ORDER;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE:
		property_id = HAL_PARAM_VDEC_PICTURE_TYPE_DECODE;
		enable_picture.picture_type = ctrl->val;
		pdata = &enable_picture;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		switch (ctrl->val) {
		case V4L2_MPEG_MSM_VIDC_DISABLE:
			inst->flags &= ~VIDC_THUMBNAIL;
			break;
		case V4L2_MPEG_MSM_VIDC_ENABLE:
			inst->flags |= VIDC_THUMBNAIL;
			break;
		}

		property_id = HAL_PARAM_VDEC_SYNC_FRAME_DECODE;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		msm_dcvs_try_enable(inst);

		bufreq = get_buff_req_buffer(inst,
				HAL_BUFFER_INPUT);
		if (!bufreq) {
			dprintk(VIDC_ERR,
					"Failed : No buffer requirements : %x\n",
					HAL_BUFFER_OUTPUT);
			return -EINVAL;
		}
		bufreq->buffer_count_min =
			MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
		bufreq->buffer_count_min_host =
			MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
		bufreq->buffer_count_actual =
			MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;

		if (msm_comm_get_stream_output_mode(inst) ==
				HAL_VIDEO_DECODER_SECONDARY) {

			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_OUTPUT);
			if (!bufreq) {
				dprintk(VIDC_ERR,
					"Failed : No buffer requirements: %x\n",
						HAL_BUFFER_OUTPUT);
				return -EINVAL;
			}

			bufreq->buffer_count_min =
				MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
			bufreq->buffer_count_min_host =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
			bufreq->buffer_count_actual =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;

			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_OUTPUT2);
			if (!bufreq) {
				dprintk(VIDC_ERR,
					"Failed : No buffer requirements: %x\n",
						HAL_BUFFER_OUTPUT2);
				return -EINVAL;
			}

			bufreq->buffer_count_min =
				MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
			bufreq->buffer_count_min_host =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
			bufreq->buffer_count_actual =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;

		} else {

			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_OUTPUT);
			if (!bufreq) {
				dprintk(VIDC_ERR,
					"Failed : No buffer requirements: %x\n",
						HAL_BUFFER_OUTPUT);
				return -EINVAL;
			}
			bufreq->buffer_count_min =
				MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
			bufreq->buffer_count_min_host =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
			bufreq->buffer_count_actual =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;

		}

		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		property_id = HAL_PARAM_SECURE;
		inst->flags |= VIDC_SECURE;
		property_val = !!(inst->flags & VIDC_SECURE);
		pdata = &property_val;
		dprintk(VIDC_DBG, "Setting secure mode to: %d\n",
				!!(inst->flags & VIDC_SECURE));
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		property_id = HAL_PARAM_INDEX_EXTRADATA;
		extra.index = msm_comm_get_hal_extradata_index(ctrl->val);
		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO:
		case V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP:
		case V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING:
		case V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE:
		case V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW:
		case V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI:
		case V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB:
		case V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO:
		case V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP:
		case V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA:
		case V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP:
		case V4L2_MPEG_VIDC_EXTRADATA_OUTPUT_CROP:
		case V4L2_MPEG_VIDC_EXTRADATA_DISPLAY_COLOUR_SEI:
		case V4L2_MPEG_VIDC_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI:
		case V4L2_MPEG_VIDC_EXTRADATA_VUI_DISPLAY:
		case V4L2_MPEG_VIDC_EXTRADATA_VPX_COLORSPACE:
		case V4L2_MPEG_VIDC_EXTRADATA_UBWC_CR_STATS_INFO:
			inst->bufq[CAPTURE_PORT].num_planes = 2;
			inst->bufq[CAPTURE_PORT].plane_sizes[EXTRADATA_IDX(2)] =
				VENUS_EXTRADATA_SIZE(
				inst->prop.height[CAPTURE_PORT],
				inst->prop.width[CAPTURE_PORT]);
			break;
		default:
			rc = -ENOTSUPP;
			break;
		}
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
			/* Release DPBs if it was previously split mode */
			rc = msm_comm_release_output_buffers(inst, false);
			if (rc)
				dprintk(VIDC_ERR,
					"%s Release output buffers failed\n",
					__func__);

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
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed:Disabling OUTPUT2 port : %d\n",
					rc);
				break;
			}
			/*
			 * If stream output mode was secondary earlier then
			 * populate output bufreqs with output2 bufreqs
			 */
			if (is_secondary_output_mode(inst)) {
				msm_comm_copy_bufreqs(inst, HAL_BUFFER_OUTPUT2,
					HAL_BUFFER_OUTPUT);
				msm_comm_copy_bufreqs(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT2,
					HAL_BUFFER_EXTRADATA_OUTPUT);
			}

			/* reset output2 buffer requirements */
			msm_comm_reset_bufreqs(inst, HAL_BUFFER_OUTPUT2);
			msm_comm_reset_bufreqs(inst,
				HAL_BUFFER_EXTRADATA_OUTPUT2);

			msm_comm_set_stream_output_mode(inst,
				HAL_VIDEO_DECODER_PRIMARY);
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY:
			switch (inst->bit_depth) {
			case MSM_VIDC_BIT_DEPTH_8:
				fourcc = V4L2_PIX_FMT_NV12_UBWC;
				break;
			case MSM_VIDC_BIT_DEPTH_10:
				fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC;
				break;
			default:
				fourcc = V4L2_PIX_FMT_NV12_UBWC;
				dprintk(VIDC_ERR,
					"Invalid bit depth. Setting DPB as NV12UBWC");
				break;
			}

			rc = msm_comm_set_color_format(inst,
						HAL_BUFFER_OUTPUT, fourcc);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s Failed setting output color format : %d\n",
					__func__, rc);
				break;
			}
			inst->clk_data.dpb_fourcc = fourcc;

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
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed setting OUTPUT2 size : %d\n",
					rc);
				break;
			}

			/* Populate output2 bufreqs with output bufreqs */
			msm_comm_copy_bufreqs(inst, HAL_BUFFER_OUTPUT,
				HAL_BUFFER_OUTPUT2);
			msm_comm_copy_bufreqs(inst,
				HAL_BUFFER_EXTRADATA_OUTPUT,
				HAL_BUFFER_EXTRADATA_OUTPUT2);

			bufreq_out2 = get_buff_req_buffer(inst,
						HAL_BUFFER_OUTPUT2);
			if (!bufreq_out2)
				break;

			rc = msm_comm_set_buffer_count(inst,
				bufreq_out2->buffer_count_min,
				bufreq_out2->buffer_count_actual,
				HAL_BUFFER_OUTPUT2);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s: Failed to set opb buffer count to FW\n");
				break;
			}

			msm_comm_set_stream_output_mode(inst,
				HAL_VIDEO_DECODER_SECONDARY);
			break;
		default:
			dprintk(VIDC_ERR,
				"Failed : Unsupported multi stream setting\n");
			rc = -ENOTSUPP;
			break;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_LEVEL);
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = msm_comm_v4l2_to_hal(ctrl->id,
				ctrl->val);
		profile_level.level = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_PROFILE);
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = msm_comm_v4l2_to_hal(ctrl->id,
				ctrl->val);
		profile_level.profile = msm_comm_v4l2_to_hal(
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
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
		property_id = HAL_CONFIG_REALTIME;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		switch (ctrl->val) {
		case V4L2_MPEG_MSM_VIDC_DISABLE:
			inst->flags &= ~VIDC_REALTIME;
			break;
		case V4L2_MPEG_MSM_VIDC_ENABLE:
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
		if (((ctrl->val >> 16) < inst->capability.frame_rate.min ||
			(ctrl->val >> 16) > inst->capability.frame_rate.max) &&
			ctrl->val != INT_MAX) {
			dprintk(VIDC_ERR, "Invalid operating rate %u\n",
				(ctrl->val >> 16));
			rc = -ENOTSUPP;
		} else if (ctrl->val == INT_MAX) {
			dprintk(VIDC_DBG,
				"inst(%pK) Request for turbo mode\n", inst);
			inst->clk_data.turbo_mode = true;
		} else if (msm_vidc_validate_operating_rate(inst, ctrl->val)) {
			dprintk(VIDC_ERR, "Failed to set operating rate\n");
			rc = -ENOTSUPP;
		} else {
			dprintk(VIDC_DBG,
				"inst(%pK) operating rate changed from %d to %d\n",
				inst, inst->clk_data.operating_rate >> 16,
					ctrl->val >> 16);
			inst->clk_data.operating_rate = ctrl->val;
			inst->clk_data.turbo_mode = false;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
		if (ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE)
			hal_property.enable = 1;
		else
			hal_property.enable = 0;
		inst->clk_data.low_latency_mode = (bool) hal_property.enable;
		break;
	default:
		break;
	}

	v4l2_ctrl_lock(ctrl);
#undef TRY_GET_CTRL

	if (!rc && property_id) {
		dprintk(VIDC_DBG,
			"Control: %x : Name = %s, ID = 0x%x Value = %d\n",
			hash32_ptr(inst->session), ctrl->name,
			ctrl->id, ctrl->val);
		rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, property_id, pdata);
	}

	return rc;
}

int msm_vdec_s_ext_ctrl(struct msm_vidc_inst *inst,
	struct v4l2_ext_controls *ctrl)
{
	int rc = 0, i = 0;
	struct v4l2_ext_control *ext_control;
	struct v4l2_control control;
	struct hal_conceal_color conceal_color = {0};
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device || !ctrl) {
		dprintk(VIDC_ERR,
			"%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	v4l2_try_ext_ctrls(&inst->ctrl_handler, ctrl);

	ext_control = ctrl->controls;

	for (i = 0; i < ctrl->count; i++) {
		switch (ext_control[i].id) {
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE:
			control.value = ext_control[i].value;
			control.id =
				V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE;
			rc = msm_comm_s_ctrl(inst, &control);
			if (rc)
				dprintk(VIDC_ERR,
					"%s Failed setting stream output mode : %d\n",
					__func__, rc);
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT:
			conceal_color.conceal_color_8bit = ext_control[i].value;
			i++;
			switch (ext_control[i].id) {
			case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT:
				conceal_color.conceal_color_10bit =
					ext_control[i].value;
				dprintk(VIDC_DBG,
					"conceal color: 8bit=0x%x 10bit=0x%x",
					conceal_color.conceal_color_8bit,
					conceal_color.conceal_color_10bit);
				rc = call_hfi_op(hdev, session_set_property,
						inst->session,
						HAL_PARAM_VDEC_CONCEAL_COLOR,
							&conceal_color);
				if (rc) {
					dprintk(VIDC_ERR,
							"%s Failed setting conceal color",
							__func__);
				}
				break;
			default:
				dprintk(VIDC_ERR,
						"%s Could not find CONCEAL_COLOR_10BIT ext_control",
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
