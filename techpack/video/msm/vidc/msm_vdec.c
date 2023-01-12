// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#include "msm_vdec.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi.h"
#include "vidc_hfi_helper.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"
#include "msm_vidc_buffer_calculations.h"

#define MIN_NUM_DEC_OUTPUT_BUFFERS 4
#define MIN_NUM_DEC_CAPTURE_BUFFERS 4
/* Y=16(0-9bits), Cb(10-19bits)=Cr(20-29bits)=128, black by default */
#define DEFAULT_VIDEO_CONCEAL_COLOR_BLACK 0x8020010
#define MAX_VP9D_INST_COUNT 3

static const char *const mpeg_video_h264_profile[] = {
	"Baseline",
	"Constrained Baseline",
	"Main",
	"Extended",
	"High",
	"High 10",
	"High 422",
	"High 444 Predictive",
	"High 10 Intra",
	"High 422 Intra",
	"High 444 Intra",
	"CAVLC 444 Intra",
	"Scalable Baseline",
	"Scalable High",
	"Scalable High Intra",
	"Stereo High",
	"Multiview High",
	"Constrained High",
	NULL,
};

static const char *const mpeg_video_h264_level[] = {
	"1",
	"1b",
	"1.1",
	"1.2",
	"1.3",
	"2",
	"2.1",
	"2.2",
	"3",
	"3.1",
	"3.2",
	"4",
	"4.1",
	"4.2",
	"5",
	"5.1",
	"5.2",
	"6.0",
	"6.1",
	"6.2",
	NULL,
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
	NULL
};

static const char *const mpeg2_profile[] = {
	"Simple",
	"Main",
	"High",
	NULL
};

static const char *const mpeg2_level[] = {
	"0",
	"1",
	"2",
	"3",
	NULL
};

static struct msm_vidc_ctrl msm_vdec_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_UNKNOWN,
		.name = "Invalid control",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER,
		.name = "Decode Order",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
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
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA,
		.name = "Extradata Type",
		.type = V4L2_CTRL_TYPE_BITMASK,
		.minimum = EXTRADATA_NONE,
		.maximum = EXTRADATA_DEFAULT | EXTRADATA_ADVANCED,
		.default_value = EXTRADATA_DEFAULT,
		.qmenu = NULL,
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
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.name = "H264 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_HIGH) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH)
		),
		.qmenu = mpeg_video_h264_profile,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.name = "H264 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_6_2,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_5_0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_1_0) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_1B) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_1_1) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_1_2) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_1_3) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_2_0) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_2_1) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_2_2) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_3_0) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_3_1) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_3_2) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_4_0) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_4_1) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_4_2) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_5_0) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_5_1) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_5_2) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_6_0) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_6_1) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_6_2)
		),
		.qmenu = mpeg_video_h264_level,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		.name = "HEVC Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		.maximum = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10,
		.default_value = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN) |
		(1 << V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE) |
		(1 << V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10)
		),
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		.name = "HEVC Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_LEVEL_1,
		.maximum = V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2,
		.default_value = V4L2_MPEG_VIDEO_HEVC_LEVEL_5,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_1) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_2) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_2_1) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_3) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_3_1) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_4) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_4_1) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_5) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_5_2) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_6) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_6_1) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2)
		),
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_TIER,
		.name = "HEVC Tier",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_TIER_MAIN,
		.maximum = V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		.default_value = V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) |
		(1 << V4L2_MPEG_VIDEO_HEVC_TIER_HIGH)
		),
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP8_PROFILE,
		.name = "VP8 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_VP8_PROFILE_0,
		.maximum = V4L2_MPEG_VIDEO_VP8_PROFILE_0,
		.default_value = V4L2_MPEG_VIDEO_VP8_PROFILE_0,
		.menu_skip_mask = ~(1 << V4L2_MPEG_VIDEO_VP8_PROFILE_0),
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VP9_PROFILE,
		.name = "VP9 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_VP9_PROFILE_0,
		.maximum = V4L2_MPEG_VIDEO_VP9_PROFILE_2,
		.default_value = V4L2_MPEG_VIDEO_VP9_PROFILE_0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_VP9_PROFILE_0) |
		(1 << V4L2_MPEG_VIDEO_VP9_PROFILE_2)
		),
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL,
		.name = "VP9 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_11) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_21) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_3) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_31) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_4) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_41) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_5) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51)
		),
		.qmenu = vp9_level,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE,
		.name = "MPEG2 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_MAIN,
		.default_value = V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_MAIN,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_MAIN)
		),
		.qmenu = mpeg2_profile,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL,
		.name = "MPEG2 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0,
		.maximum = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_2,
		.default_value = V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_2,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0) |
			(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_1) |
			(1 << V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_2)
		),
		.qmenu = mpeg2_level,
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
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
		.name = "CAPTURE Count",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = SINGLE_OUTPUT_BUFFER,
		.maximum = MAX_NUM_OUTPUT_BUFFERS,
		.default_value = SINGLE_OUTPUT_BUFFER,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
		.name = "OUTPUT Count",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = SINGLE_INPUT_BUFFER,
		.maximum = MAX_NUM_INPUT_BUFFERS,
		.default_value = SINGLE_INPUT_BUFFER,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE,
		.name = "Frame Rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = (MINIMUM_FPS << 16),
		.maximum = (MAXIMUM_FPS << 16),
		.default_value = (DEFAULT_FPS << 16),
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY,
		.name = "Session Priority",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_ENABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE,
		.name = "Decoder Operating rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = (DEFAULT_FPS << 16),/* Power Vote min fps */
		.maximum = INT_MAX,
		.default_value =  (DEFAULT_FPS << 16),
		.step = 1,
		.qmenu = NULL,
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
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_HINT,
		.name = "Low Latency Hint",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_DISABLE_TIMESTAMP_REORDER,
		.name = "Disable TimeStamp Reorder",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_SUPERFRAME,
		.name = "Superframe",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VDEC_HEIF_MODE,
		.name = "HEIF Decoder",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)


struct msm_vidc_format_desc vdec_output_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 10bit",
		.description = "Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0 10bit",
		.description = "UBWC Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC,
	},
};

struct msm_vidc_format_desc vdec_input_formats[] = {
	{
		.name = "Mpeg2",
		.description = "Mpeg2 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG2,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
	},
	{
		.name = "VP9",
		.description = "VP9 compressed format",
		.fourcc = V4L2_PIX_FMT_VP9,
	},
};

struct msm_vidc_format_constraint dec_pix_format_constraints[] = {
	{
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
		.num_planes = 2,
		.y_max_stride = 8192,
		.y_buffer_alignment = 256,
		.uv_max_stride = 8192,
		.uv_buffer_alignment = 256,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.y_max_stride = 8192,
		.y_buffer_alignment = 512,
		.uv_max_stride = 8192,
		.uv_buffer_alignment = 256,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.num_planes = 2,
		.y_max_stride = 8192,
		.y_buffer_alignment = 512,
		.uv_max_stride = 8192,
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
			get_v4l2_codec(inst) == V4L2_PIX_FMT_VP9)
			vp9d_instance_count++;
	}
	mutex_unlock(&core->lock);

	if (vp9d_instance_count > MAX_VP9D_INST_COUNT)
		return true;
	return false;
}

int msm_vdec_update_stream_output_mode(struct msm_vidc_inst *inst)
{
	struct v4l2_format *f;
	u32 format;
	u32 stream_output_mode;
	u32 fourcc;

	if (!inst) {
		d_vpr_e("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	format = f->fmt.pix_mp.pixelformat;
	stream_output_mode = HAL_VIDEO_DECODER_PRIMARY;
	if ((format == V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS) ||
		(format == V4L2_PIX_FMT_NV12)) {
		stream_output_mode = HAL_VIDEO_DECODER_SECONDARY;
	}

	msm_comm_set_stream_output_mode(inst,
		stream_output_mode);

	fourcc = V4L2_PIX_FMT_NV12_UBWC;
	if (inst->bit_depth == MSM_VIDC_BIT_DEPTH_10)
		fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC;

	inst->clk_data.dpb_fourcc = fourcc;

	return 0;
}

int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	struct msm_vidc_format *fmt = NULL;
	struct msm_vidc_format_desc *fmt_desc = NULL;
	struct v4l2_pix_format_mplane *mplane = NULL;
	int rc = 0;
	u32 color_format;

	if (!inst || !f) {
		d_vpr_e("%s: invalid parameters %pK %pK\n", __func__, inst, f);
		return -EINVAL;
	}

	/*
	 * First update inst format with new width/height/format
	 * Recalculate sizes/strides etc
	 * Perform necessary checks to continue with session
	 * Copy recalculated info into user format
	 */
	if (f->type == OUTPUT_MPLANE) {
		fmt = &inst->fmts[OUTPUT_PORT];
		fmt_desc = msm_comm_get_pixel_fmt_fourcc(vdec_output_formats,
			ARRAY_SIZE(vdec_output_formats),
			f->fmt.pix_mp.pixelformat, inst->sid);
		if (!fmt_desc) {
			s_vpr_e(inst->sid, "Invalid fmt set : %x\n",
				f->fmt.pix_mp.pixelformat);
			return -EINVAL;
		}
		strlcpy(fmt->name, fmt_desc->name, sizeof(fmt->name));
		strlcpy(fmt->description, fmt_desc->description,
			sizeof(fmt->description));

		inst->clk_data.opb_fourcc = f->fmt.pix_mp.pixelformat;

		fmt->v4l2_fmt.type = f->type;
		mplane = &fmt->v4l2_fmt.fmt.pix_mp;
		mplane->width = f->fmt.pix_mp.width;
		mplane->height = f->fmt.pix_mp.height;
		mplane->pixelformat = f->fmt.pix_mp.pixelformat;
		mplane->plane_fmt[0].sizeimage =
			msm_vidc_calculate_dec_output_frame_size(inst);

		if (mplane->num_planes > 1)
			mplane->plane_fmt[1].sizeimage =
				msm_vidc_calculate_dec_output_extra_size(inst);
		color_format = msm_comm_convert_color_fmt(
			f->fmt.pix_mp.pixelformat, inst->sid);
		mplane->plane_fmt[0].bytesperline =
			VENUS_Y_STRIDE(color_format, f->fmt.pix_mp.width);
		mplane->plane_fmt[0].reserved[0] =
			VENUS_Y_SCANLINES(color_format, f->fmt.pix_mp.height);
		inst->bit_depth = MSM_VIDC_BIT_DEPTH_8;
		if ((f->fmt.pix_mp.pixelformat ==
			V4L2_PIX_FMT_NV12_TP10_UBWC) ||
			(f->fmt.pix_mp.pixelformat ==
			V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS)) {
			inst->bit_depth = MSM_VIDC_BIT_DEPTH_10;
		}

		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
		}

		rc = msm_vdec_update_stream_output_mode(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s: failed to update output stream mode\n",
				__func__);
			goto err_invalid_fmt;
		}

		memcpy(f, &fmt->v4l2_fmt, sizeof(struct v4l2_format));
	} else if (f->type == INPUT_MPLANE) {
		fmt = &inst->fmts[INPUT_PORT];
		fmt_desc = msm_comm_get_pixel_fmt_fourcc(vdec_input_formats,
			ARRAY_SIZE(vdec_input_formats),
			f->fmt.pix_mp.pixelformat, inst->sid);
		if (!fmt_desc) {
			s_vpr_e(inst->sid, "Invalid fmt set : %x\n",
				f->fmt.pix_mp.pixelformat);
			return -EINVAL;
		}
		strlcpy(fmt->name, fmt_desc->name, sizeof(fmt->name));
		strlcpy(fmt->description, fmt_desc->description,
			sizeof(fmt->description));

		if (f->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_VP9) {
			if (msm_vidc_check_for_vp9d_overload(inst->core)) {
				s_vpr_e(inst->sid, "VP9 Decode overload\n");
				rc = -ENOMEM;
				goto err_invalid_fmt;
			}
		}

		fmt->v4l2_fmt.type = f->type;
		mplane = &fmt->v4l2_fmt.fmt.pix_mp;
		mplane->width = f->fmt.pix_mp.width;
		mplane->height = f->fmt.pix_mp.height;
		mplane->pixelformat = f->fmt.pix_mp.pixelformat;
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			s_vpr_e(inst->sid, "Failed to open instance\n");
			goto err_invalid_fmt;
		}

		mplane->plane_fmt[0].sizeimage =
			msm_vidc_calculate_dec_input_frame_size(inst, inst->buffer_size_limit);

		/* Driver can recalculate buffer count only for
		 * only for bitstream port. Decoder YUV port reconfig
		 * should not overwrite the FW calculated buffer
		 * count.
		 */
		rc = msm_vidc_calculate_buffer_counts(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s failed to calculate buffer count\n",
				__func__);
			return rc;
		}

		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
		}
		update_log_ctxt(inst->sid, inst->session_type,
			mplane->pixelformat);
		memcpy(f, &fmt->v4l2_fmt, sizeof(struct v4l2_format));
	}

	inst->batch.enable = is_batching_allowed(inst);
	msm_dcvs_try_enable(inst);

err_invalid_fmt:
	return rc;
}

int msm_vdec_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	struct v4l2_format *fmt;

	if (f->type == OUTPUT_MPLANE) {
		fmt = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
			msm_vidc_calculate_dec_output_frame_size(inst);
		if (fmt->fmt.pix_mp.num_planes > 1)
			fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
				msm_vidc_calculate_dec_output_extra_size(inst);
		memcpy(f, fmt, sizeof(struct v4l2_format));
	} else if (f->type == INPUT_MPLANE) {
		fmt = &inst->fmts[INPUT_PORT].v4l2_fmt;
		fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
			msm_vidc_calculate_dec_input_frame_size(inst, inst->buffer_size_limit);
		memcpy(f, fmt, sizeof(struct v4l2_format));
	} else {
		s_vpr_e(inst->sid, "%s: Unsupported buf type: %d\n",
			__func__, f->type);
		return -EINVAL;
	}

	return 0;
}

int msm_vdec_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f)
{
	const struct msm_vidc_format_desc *fmt_desc = NULL;
	int rc = 0;

	if (!inst || !f) {
		d_vpr_e("Invalid input, inst = %pK, f = %pK\n", inst, f);
		return -EINVAL;
	}
	if (f->type == OUTPUT_MPLANE) {
		fmt_desc = msm_comm_get_pixel_fmt_index(vdec_output_formats,
			ARRAY_SIZE(vdec_output_formats), f->index, inst->sid);
	} else if (f->type == INPUT_MPLANE) {
		fmt_desc = msm_comm_get_pixel_fmt_index(vdec_input_formats,
			ARRAY_SIZE(vdec_input_formats), f->index, inst->sid);
		f->flags = V4L2_FMT_FLAG_COMPRESSED;
	}

	memset(f->reserved, 0, sizeof(f->reserved));
	if (fmt_desc) {
		strlcpy(f->description, fmt_desc->description,
				sizeof(f->description));
		f->pixelformat = fmt_desc->fourcc;
	} else {
		s_vpr_h(inst->sid, "No more formats found\n");
		rc = -EINVAL;
	}
	return rc;
}

int msm_vdec_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct msm_vidc_format_desc *fmt_desc = NULL;
	struct v4l2_format *f = NULL;

	if (!inst || !inst->core) {
		d_vpr_e("Invalid input = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;

	inst->prop.extradata_ctrls = EXTRADATA_DEFAULT;
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	f->type = OUTPUT_MPLANE;
	f->fmt.pix_mp.height = DEFAULT_HEIGHT;
	f->fmt.pix_mp.width = DEFAULT_WIDTH;
	f->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12_UBWC;
	f->fmt.pix_mp.num_planes = 2;
	f->fmt.pix_mp.plane_fmt[0].sizeimage =
		msm_vidc_calculate_dec_output_frame_size(inst);
	f->fmt.pix_mp.plane_fmt[1].sizeimage =
		msm_vidc_calculate_dec_output_extra_size(inst);
	fmt_desc = msm_comm_get_pixel_fmt_fourcc(vdec_output_formats,
		ARRAY_SIZE(vdec_output_formats),
		f->fmt.pix_mp.pixelformat, inst->sid);
	if (!fmt_desc) {
		s_vpr_e(inst->sid, "Invalid fmt set: %x\n",
			f->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	strlcpy(inst->fmts[OUTPUT_PORT].name, fmt_desc->name,
		sizeof(inst->fmts[OUTPUT_PORT].name));
	strlcpy(inst->fmts[OUTPUT_PORT].description, fmt_desc->description,
		sizeof(inst->fmts[OUTPUT_PORT].description));
	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	f->type = INPUT_MPLANE;
	f->fmt.pix_mp.height = DEFAULT_HEIGHT;
	f->fmt.pix_mp.width = DEFAULT_WIDTH;
	f->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
	f->fmt.pix_mp.num_planes = 1;
	f->fmt.pix_mp.plane_fmt[0].sizeimage =
		msm_vidc_calculate_dec_input_frame_size(inst, inst->buffer_size_limit);
	fmt_desc = msm_comm_get_pixel_fmt_fourcc(vdec_input_formats,
		ARRAY_SIZE(vdec_input_formats), f->fmt.pix_mp.pixelformat,
		inst->sid);
	if (!fmt_desc) {
		s_vpr_e(inst->sid, "Invalid fmt set: %x\n",
			f->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	strlcpy(inst->fmts[INPUT_PORT].name, fmt_desc->name,
		sizeof(inst->fmts[INPUT_PORT].name));
	strlcpy(inst->fmts[INPUT_PORT].description, fmt_desc->description,
		sizeof(inst->fmts[INPUT_PORT].description));
	inst->buffer_mode_set[INPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_DYNAMIC;
	inst->stream_output_mode = HAL_VIDEO_DECODER_PRIMARY;


	inst->clk_data.frame_rate = (DEFAULT_FPS << 16);
	inst->clk_data.operating_rate = (DEFAULT_FPS << 16);
	if (core->resources.decode_batching) {
		inst->batch.enable = true;
		inst->batch.size = MAX_DEC_BATCH_SIZE;
	}

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
	msm_vidc_init_buffer_size_calculators(inst);

	return rc;
}

int msm_vdec_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		d_vpr_e("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	s_vpr_h(inst->sid, "%s: control name = %s, id = 0x%x value = %d\n",
		__func__, ctrl->name, ctrl->id, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE:
		inst->profile = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val,
							inst->sid);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL:
		inst->level = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val,
							inst->sid);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
		inst->level |=
			(msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val,
				inst->sid) << 28);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER:
	case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT:
	case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT:
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE:
		inst->clk_data.frame_rate = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		inst->flags &= ~VIDC_THUMBNAIL;
		if (ctrl->val)
			inst->flags |= VIDC_THUMBNAIL;

		inst->batch.enable = is_batching_allowed(inst);
		rc = msm_vidc_calculate_buffer_counts(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s: failed to calculate thumbnail buffer count\n",
				__func__);
			return rc;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags &= ~VIDC_SECURE;
		if (ctrl->val)
			inst->flags |= VIDC_SECURE;
		if (msm_comm_check_for_inst_overload(inst->core)) {
			s_vpr_e(inst->sid,
				"%s: Instance count reached Max limit, rejecting session",
				__func__);
			return -ENOTSUPP;
		}
		msm_comm_memory_prefetch(inst);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		if (ctrl->val == EXTRADATA_NONE)
			inst->prop.extradata_ctrls = 0;
		else
			inst->prop.extradata_ctrls |= ctrl->val;
		/*
		 * nothing to do here as inst->bufq[OUTPUT_PORT].num_planes
		 * and inst->bufq[OUTPUT_PORT].plane_sizes[1] are already
		 * initialized to proper values
		 */
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BUFFER_SIZE_LIMIT:
		inst->buffer_size_limit = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE:
		inst->flags &= ~VIDC_TURBO;
		if (ctrl->val == INT_MAX)
			inst->flags |= VIDC_TURBO;
		else
			inst->clk_data.operating_rate = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
		inst->clk_data.low_latency_mode = !!ctrl->val;
		inst->batch.enable = is_batching_allowed(inst);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_HINT:
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DISABLE_TIMESTAMP_REORDER:
		break;
	case V4L2_CID_MPEG_VIDC_VDEC_HEIF_MODE:
		if(get_v4l2_codec(inst) != V4L2_PIX_FMT_HEVC)
			break;
		inst->flags &= ~VIDC_TURBO;
		if (ctrl->val)
			inst->flags |= VIDC_TURBO;
		if (inst->state < MSM_VIDC_LOAD_RESOURCES)
			msm_vidc_calculate_buffer_counts(inst);
		break;
	default:
		s_vpr_e(inst->sid, "Unknown control %#x\n", ctrl->id);
		break;
	}

	return rc;
}

int msm_vdec_set_frame_size(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_frame_size frame_size;
	struct v4l2_format *f;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	frame_size.buffer_type = HFI_BUFFER_INPUT;
	frame_size.width = f->fmt.pix_mp.width;
	frame_size.height = f->fmt.pix_mp.height;
	s_vpr_h(inst->sid, "%s: input wxh %dx%d\n", __func__,
		frame_size.width, frame_size.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_size, sizeof(frame_size));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_color_format(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_format_constraint *fmt_constraint;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = msm_comm_set_color_format(inst,
			msm_comm_get_hal_output_buffer(inst),
			inst->clk_data.opb_fourcc);
	if (rc) {
		s_vpr_e(inst->sid, "%s: set color format (%#x) failed\n",
			__func__, inst->clk_data.opb_fourcc);
		return rc;
	}
	fmt_constraint = msm_comm_get_pixel_fmt_constraints(
			dec_pix_format_constraints,
			ARRAY_SIZE(dec_pix_format_constraints),
			inst->clk_data.opb_fourcc, inst->sid);
	if (fmt_constraint) {
		rc = msm_comm_set_color_format_constraints(inst,
				msm_comm_get_hal_output_buffer(inst),
				fmt_constraint);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s: Set constraints for color format %#x failed\n",
				__func__, inst->clk_data.opb_fourcc);
			return rc;
		}
	}

	return rc;
}

int msm_vdec_set_input_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_format *fmt;
	enum hal_buffer buffer_type;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	buffer_type = HAL_BUFFER_INPUT;
	fmt = &inst->fmts[INPUT_PORT];
	rc = msm_comm_set_buffer_count(inst,
			fmt->count_min,
			fmt->count_actual,
			buffer_type);
	if (rc) {
		s_vpr_e(inst->sid, "%s: failed to set bufreqs(%#x)\n",
			__func__, buffer_type);
		return -EINVAL;
	}

	return rc;
}

int msm_vdec_set_output_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_format *fmt;
	enum hal_buffer buffer_type;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	buffer_type = msm_comm_get_hal_output_buffer(inst);
	/* Correct buffer counts is always stored in HAL_BUFFER_OUTPUT */
	fmt = &inst->fmts[OUTPUT_PORT];
	if (buffer_type == HAL_BUFFER_OUTPUT2) {
		/*
		 * For split mode set DPB count as well
		 * For DPB actual count is same as min output count
		 */
		rc = msm_comm_set_buffer_count(inst,
			fmt->count_min,
			fmt->count_min,
			HAL_BUFFER_OUTPUT);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s: failed to set buffer count(%#x)\n",
				__func__, buffer_type);
			return -EINVAL;
		}
	}
	rc = msm_comm_set_buffer_count(inst,
			fmt->count_min,
			fmt->count_actual,
			buffer_type);
	if (rc) {
		s_vpr_e(inst->sid, "%s: failed to set bufreqs(%#x)\n",
			__func__, buffer_type);
		return -EINVAL;
	}

	return rc;
}

int msm_vdec_set_profile_level(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_profile_level profile_level;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	profile_level.profile = inst->profile;
	profile_level.level = inst->level;

	s_vpr_h(inst->sid, "%s: %#x %#x\n", __func__,
		profile_level.profile, profile_level.level);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT, &profile_level,
		sizeof(profile_level));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_output_order(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	u32 output_order;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER);
	s_vpr_h(inst->sid, "%s: %d\n", __func__, ctrl->val);
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		output_order = HFI_OUTPUT_ORDER_DECODE;
	else
		output_order = HFI_OUTPUT_ORDER_DISPLAY;

	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_OUTPUT_ORDER, &output_order,
		sizeof(u32));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_sync_frame_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable hfi_property;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE);
	hfi_property.enable = (bool)ctrl->val;

	s_vpr_h(inst->sid, "%s: %#x\n", __func__, hfi_property.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_THUMBNAIL_MODE, &hfi_property,
		sizeof(hfi_property));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_secure_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SECURE);

	codec = get_v4l2_codec(inst);
	if (ctrl->val) {
		if (!(codec == V4L2_PIX_FMT_HEVC ||
			codec == V4L2_PIX_FMT_H264 ||
			codec == V4L2_PIX_FMT_VP9)) {
			s_vpr_e(inst->sid,
				"%s: Secure allowed for HEVC/H264/VP9\n",
				__func__);
			return -EINVAL;
		}
	}

	s_vpr_h(inst->sid, "%s: %#x\n", __func__, ctrl->val);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_SECURE_SESSION, &ctrl->val, sizeof(u32));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_output_stream_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_multi_stream multi_stream;
	struct hfi_frame_size frame_sz;
	struct v4l2_format *f;
	u32 sid;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;
	sid = inst->sid;

	if (is_primary_output_mode(inst)) {
		multi_stream.buffer_type = HFI_BUFFER_OUTPUT;
		multi_stream.enable = true;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM, &multi_stream,
			sizeof(multi_stream));
		if (rc) {
			s_vpr_e(sid,
				"%s: set prop multistream primary failed: %d\n",
				__func__, rc);
			return rc;
		}
		multi_stream.buffer_type = HFI_BUFFER_OUTPUT2;
		multi_stream.enable = false;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM, &multi_stream,
			sizeof(multi_stream));
		if (rc) {
			s_vpr_e(sid,
				"%s: set prop multistream primary2 failed : %d\n",
				__func__, rc);
			return rc;
		}
	} else {
		rc = msm_comm_set_color_format(inst,
			HAL_BUFFER_OUTPUT, inst->clk_data.dpb_fourcc);
		if (rc)
			return rc;

		multi_stream.buffer_type = HFI_BUFFER_OUTPUT2;
		multi_stream.enable = true;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM, &multi_stream,
			sizeof(multi_stream));
		if (rc) {
			s_vpr_e(sid,
				"%s: set prop multistream secondary failed : %d\n",
				__func__, rc);
			return rc;
		}
		multi_stream.buffer_type = HFI_BUFFER_OUTPUT;
		multi_stream.enable = false;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM, &multi_stream,
			sizeof(multi_stream));
		if (rc) {
			s_vpr_e(sid,
				"%s: set prop multistream secondary2 failed: %d\n",
				__func__, rc);
			return rc;
		}
		frame_sz.buffer_type = HFI_BUFFER_OUTPUT2;
		f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		frame_sz.width = f->fmt.pix_mp.width;
		frame_sz.height = f->fmt.pix_mp.height;
		s_vpr_h(sid,
			"frame_size: hal buffer type %d, width %d, height %d\n",
			frame_sz.buffer_type, frame_sz.width, frame_sz.height);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_sz,
			sizeof(frame_sz));
		if (rc) {
			s_vpr_e(sid, "%s: set prop frame_size failed\n",
				__func__);
			return rc;
		}
	}

	return rc;
}

int msm_vdec_set_priority(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_enable hfi_property;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	hfi_property.enable = is_realtime_session(inst);

	s_vpr_h(inst->sid, "%s: %#x\n", __func__, hfi_property.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_REALTIME, &hfi_property,
		sizeof(hfi_property));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_seqchng_at_syncframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	u32 codec;
	struct hfi_device *hdev;
	struct hfi_enable hfi_property;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;
	hfi_property.enable = is_low_latency_hint(inst);

	if (!hfi_property.enable)
		return 0;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_HEVC || codec == V4L2_PIX_FMT_H264)) {
		s_vpr_e(inst->sid,
			"%s:  low latency hint supported for HEVC/H264\n",
				__func__);
		return -EINVAL;
	}
	s_vpr_h(inst->sid, "%s: %#x\n", __func__, hfi_property.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_SEQCHNG_AT_SYNCFRM, &hfi_property,
		sizeof(hfi_property));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_conceal_color(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl_8b;
	struct v4l2_ctrl *ctrl_10b;
	struct hfi_conceal_color conceal_color;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl_8b = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT);
	ctrl_10b = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT);
	conceal_color.conceal_color_8bit = ctrl_8b->val;
	conceal_color.conceal_color_10bit = ctrl_10b->val;

	s_vpr_h(inst->sid, "%s: %#x %#x\n", __func__,
		conceal_color.conceal_color_8bit,
		conceal_color.conceal_color_10bit);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_CONCEAL_COLOR, &conceal_color,
		sizeof(conceal_color));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}


int msm_vdec_set_extradata(struct msm_vidc_inst *inst)
{
	uint32_t display_info = HFI_PROPERTY_PARAM_VUI_DISPLAY_INFO_EXTRADATA;
	u32 value = 0x0;
	u32 codec;

	codec = get_v4l2_codec(inst);
	switch (codec) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_HEVC:
		display_info = HFI_PROPERTY_PARAM_VUI_DISPLAY_INFO_EXTRADATA;
		break;
	case V4L2_PIX_FMT_VP9:
		display_info =
			HFI_PROPERTY_PARAM_VDEC_VPX_COLORSPACE_EXTRADATA;
		break;
	case V4L2_PIX_FMT_MPEG2:
		display_info = HFI_PROPERTY_PARAM_VDEC_MPEG2_SEQDISP_EXTRADATA;
		break;
	}

	/* Enable Default Extradata */
	msm_comm_set_index_extradata(inst,
		MSM_VIDC_EXTRADATA_OUTPUT_CROP, 0x1);
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_INTERLACE_VIDEO_EXTRADATA, 0x1);
	msm_comm_set_extradata(inst, display_info, 0x1);

	if (codec == V4L2_PIX_FMT_VP9 || codec == V4L2_PIX_FMT_HEVC) {
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VDEC_HDR10_HIST_EXTRADATA, 0x1);
	}

	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_NUM_CONCEALED_MB, 0x1);
	if (codec == V4L2_PIX_FMT_HEVC) {
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VDEC_MASTER_DISP_COL_SEI_EXTRADATA,
			0x1);
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VDEC_CLL_SEI_EXTRADATA, 0x1);
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VDEC_STREAM_USERDATA_EXTRADATA,
			0x1);
	}

	/* Enable / Disable Advanced Extradata */
	if (inst->prop.extradata_ctrls & EXTRADATA_ADVANCED)
		value = 0x1;
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_STREAM_USERDATA_EXTRADATA, value);
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_TIMESTAMP_EXTRADATA, value);
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_S3D_FRAME_PACKING_EXTRADATA, value);
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_FRAME_RATE_EXTRADATA, value);
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_PANSCAN_WNDW_EXTRADATA, value);
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_RECOVERY_POINT_SEI_EXTRADATA, value);
	msm_comm_set_index_extradata(inst,
		MSM_VIDC_EXTRADATA_ASPECT_RATIO, value);
	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_FRAME_QP_EXTRADATA, value);

	return 0;
}

int msm_vdec_set_properties(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!in_port_reconfig(inst)) {
		/* do not allow these settings in port reconfiration */
		rc = msm_vdec_set_frame_size(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_input_buffer_counts(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_profile_level(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_output_order(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_sync_frame_mode(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_secure_mode(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_extradata(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_priority(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_conceal_color(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_seqchng_at_syncframe(inst);
		if (rc)
			goto exit;
	}

	rc = msm_vdec_set_color_format(inst);
	if (rc)
		goto exit;
	rc = msm_vdec_set_output_stream_mode(inst);
	if (rc)
		goto exit;
	rc = msm_vdec_set_output_buffer_counts(inst);
	if (rc)
		goto exit;

exit:
	if (rc)
		s_vpr_e(inst->sid, "%s: failed with %d\n", __func__, rc);
	else
		s_vpr_h(inst->sid, "%s: set properties successful\n", __func__);

	return rc;
}

int msm_vdec_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops)
{
	return msm_comm_ctrl_init(inst, msm_vdec_ctrls,
		ARRAY_SIZE(msm_vdec_ctrls), ctrl_ops);
}
