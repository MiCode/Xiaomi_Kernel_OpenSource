// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */
#include <linux/slab.h>
#include "msm_venc.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"

#define MIN_BIT_RATE 32000
#define MAX_BIT_RATE 1200000000
#define DEFAULT_BIT_RATE 64000
#define BIT_RATE_STEP 1
#define DEFAULT_FRAME_RATE 15
#define DEFAULT_OPERATING_RATE DEFAULT_FRAME_RATE
#define OPERATING_FRAME_RATE_STEP (1 << 16)
#define MAX_SLICE_BYTE_SIZE ((MAX_BIT_RATE)>>3)
#define MIN_SLICE_BYTE_SIZE 512
#define MAX_SLICE_MB_SIZE ((4096 * 2304) >> 8)
#define QP_ENABLE_I 0x1
#define QP_ENABLE_P 0x2
#define QP_ENABLE_B 0x4
#define MAX_INTRA_REFRESH_MBS ((7680 * 4320) >> 8)
#define MAX_LTR_FRAME_COUNT 10
#define MAX_NUM_B_FRAMES 1

#define L_MODE V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY
#define MIN_TIME_RESOLUTION 1
#define MAX_TIME_RESOLUTION 0xFFFFFF
#define DEFAULT_TIME_RESOLUTION 0x7530
#define MIN_NUM_ENC_OUTPUT_BUFFERS 4
#define MIN_NUM_ENC_CAPTURE_BUFFERS 5

static const char *const mpeg_video_rate_control[] = {
	"VBR CFR",
	"CBR CFR",
	"MBR CFR",
	"CBR VFR",
	"MBR VFR",
	"CQ",
	NULL
};

static const char *const h264_video_entropy_cabac_model[] = {
	"Model 0",
	"Model 1",
	"Model 2",
	NULL
};

static const char *const vp8_profile_level[] = {
	"Unused",
	"0.0",
	"1.0",
	"2.0",
	"3.0",
};

static const char *const perf_level[] = {
	"Nominal",
	"Performance",
	"Turbo"
};

static const char *const mbi_statistics[] = {
	"Camcorder Default",
	"Mode 1",
	"Mode 2",
	"Mode 3"
};

static const char *const timestamp_mode[] = {
	"Honor",
	"Ignore",
};

static const char *const mpeg_video_stream_format[] = {
	"NAL Format Start Codes",
	"NAL Format One NAL Per Buffer",
	"NAL Format One Byte Length",
	"NAL Format Two Byte Length",
	"NAL Format Four Byte Length",
	NULL
};

static struct msm_vidc_ctrl msm_venc_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		.name = "Intra Period for P frames",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 2*DEFAULT_FRAME_RATE-1,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP,
		.name = "HEVC I Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP,
		.name = "HEVC P Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP,
		.name = "HEVC B Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP,
		.name = "HEVC Quantization Range Minimum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = 1,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP,
		.name = "HEVC Quantization Range Maximum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = 127,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_B_FRAMES,
		.name = "Intra Period for B frames",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_NUM_B_FRAMES,
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
		.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
		.name = "Request I Frame",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 0,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		.name = "Video Bitrate Control",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		.maximum = V4L2_MPEG_VIDEO_BITRATE_MODE_CQ,
		.default_value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_CBR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_MBR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_MBR_VFR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		),
		.qmenu = mpeg_video_rate_control,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY,
		.name = "Compression quality",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_FRAME_QUALITY,
		.maximum = MAX_FRAME_QUALITY,
		.default_value = DEFAULT_FRAME_QUALITY,
		.step = FRAME_QUALITY_STEP,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE,
		.name = "Image grid size",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 512,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE,
		.name = "Bit Rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = BIT_RATE_STEP,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
		.name = "Entropy Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
		.maximum = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
		.default_value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC) |
		(1 << V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC)
		),
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.name = "H264 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_HIGH) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH)
		),
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.name = "H264 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN,
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
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_6_2) |
		(1 << V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN)
		),
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
		.name = "VP8 Profile Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3)
		),
		.qmenu = vp8_profile_level,
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
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		.name = "HEVC Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_LEVEL_1,
		.maximum = V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN,
		.default_value =
			V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN,
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
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2) |
		(1 << V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN)
		),
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_TIER,
		.name = "HEVC Tier",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_TIER_MAIN,
		.maximum = V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		.default_value = V4L2_MPEG_VIDEO_HEVC_TIER_MAIN,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEVC_TIER_MAIN) |
		(1 << V4L2_MPEG_VIDEO_HEVC_TIER_HIGH)
		),
	},
	{
		.id = V4L2_CID_ROTATE,
		.name = "Rotation",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 270,
		.default_value = 0,
		.step = 90,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,
		.name = "Slice Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.maximum = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES,
		.default_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE) |
		(1 << V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) |
		(1 << V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES)
		),
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES,
		.name = "Slice Byte Size",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_SLICE_BYTE_SIZE,
		.maximum = MAX_SLICE_BYTE_SIZE,
		.default_value = MIN_SLICE_BYTE_SIZE,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB,
		.name = "Slice MB Size",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = MAX_SLICE_MB_SIZE,
		.default_value = 1,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM,
		.name = "Random Intra Refresh MBs",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_INTRA_REFRESH_MBS,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB,
		.name = "Cyclic Intra Refresh MBs",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_INTRA_REFRESH_MBS,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA,
		.name = "H.264 Loop Filter Alpha Offset",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = -6,
		.maximum = 6,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA,
		.name = "H.264 Loop Filter Beta Offset",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = -6,
		.maximum = 6,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
		.name = "H.264 Loop Filter Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
		.maximum = L_MODE,
		.default_value = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED) |
		(1 << V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED) |
		(1 << L_MODE)
		),
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR,
		.name = "Prepend SPS/PPS to IDR",
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
		.maximum = EXTRADATA_ADVANCED | EXTRADATA_ENC_INPUT_ROI |
			EXTRADATA_ENC_INPUT_HDR10PLUS,
		.default_value = EXTRADATA_NONE,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO,
		.name = "H264 VUI Timing Info",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER,
		.name = "AU Delimiter",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.step = 1,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME,
		.name = "H264 Use LTR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = (MAX_LTR_FRAME_COUNT - 1),
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT,
		.name = "Ltr Count",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_LTR_FRAME_COUNT,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME,
		.name = "H264 Mark LTR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = (MAX_LTR_FRAME_COUNT - 1),
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER,
		.name = "Set Hier layers",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 6,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE,
		.name = "Set Hier coding type",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B,
		.maximum = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		.default_value = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_QP,
		.name = "Set layer0 QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.default_value = 51,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_QP,
		.name = "Set layer1 QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.default_value = 51,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_QP,
		.name = "Set layer2 QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.default_value = 51,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_QP,
		.name = "Set layer3 QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.default_value = 51,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_QP,
		.name = "Set layer4 QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.default_value = 51,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_QP,
		.name = "Set layer5 QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 51,
		.default_value = 51,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_BR,
		.name = "Set layer0 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_BR,
		.name = "Set layer1 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_BR,
		.name = "Set layer2 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_BR,
		.name = "Set layer3 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_BR,
		.name = "Set layer4 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_BR,
		.name = "Set layer5 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE,
		.name = "VP8 Error Resilience mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID,
		.name = "Set Base Layer ID for Hier-P",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 6,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH,
		.name = "SAR Width",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 7680,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT,
		.name = "SAR Height",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 7680,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
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
		.name = "Set Encoder Operating rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = (DEFAULT_OPERATING_RATE << 16),
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC,
		.name = "Set VPE Color space conversion coefficients",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
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
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS,
		.name = "Set Blur width/height",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 0xFFFFFFFF,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM,
		.name = "Transform 8x8",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_ENABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE,
		.name = "Set Color space",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MSM_VIDC_RESERVED_1,
		.maximum = MSM_VIDC_BT2020,
		.default_value = MSM_VIDC_RESERVED_1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE,
		.name = "Set Color space range",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS,
		.name = "Set Color space transfer characterstics",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MSM_VIDC_TRANSFER_BT709_5,
		.maximum = MSM_VIDC_TRANSFER_HLG,
		.default_value = MSM_VIDC_TRANSFER_601_6_625,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS,
		.name = "Set Color space matrix coefficients",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MSM_VIDC_MATRIX_BT_709_5,
		.maximum = MSM_VIDC_MATRIX_BT_2020_CONST,
		.default_value = MSM_VIDC_MATRIX_601_6_625,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE,
		.name = "Frame Rate based Rate Control",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX,
		.name = "Enable/Disable CSC Custom Matrix",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_HFLIP,
		.name = "Enable/Disable Horizontal Flip",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_VFLIP,
		.name = "Enable/Disable Vertical Flip",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_HDR_INFO,
		.name = "Enable/Disable HDR INFO",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT,
		.name = "NAL Format",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.maximum = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH,
		.default_value = V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_STARTCODES) |
		(1 << V4L2_MPEG_VIDC_VIDEO_NAL_FORMAT_FOUR_BYTE_LENGTH)
		),
		.qmenu = mpeg_video_stream_format,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_venc_ctrls)

static u32 get_frame_size_compressed(int plane, u32 height, u32 width)
{
	int sz = ALIGN(height, 32) * ALIGN(width, 32) * 3 / 2;

	return ALIGN(sz, SZ_4K);
}

static struct msm_vidc_format venc_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.get_frame_size = get_frame_size_nv12,
		.type = OUTPUT_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
		.get_frame_size = get_frame_size_nv12_ubwc,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "YCrCb Semiplanar 4:2:0",
		.description = "Y/CrCb 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.get_frame_size = get_frame_size_nv21,
		.type = OUTPUT_PORT,
	},
	{
		.name = "TP10 UBWC 4:2:0",
		.description = "TP10 UBWC 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC,
		.get_frame_size = get_frame_size_tp10_ubwc,
		.type = OUTPUT_PORT,
	},
	{
		.name = "TME",
		.description = "TME MBI format",
		.fourcc = V4L2_PIX_FMT_TME,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 10bit",
		.description = "Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
		.get_frame_size = get_frame_size_p010,
		.type = OUTPUT_PORT,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 512 aligned",
		.description = "Y/CbCr 4:2:0 512 aligned",
		.fourcc = V4L2_PIX_FMT_NV12_512,
		.get_frame_size = get_frame_size_nv12_512,
		.type = OUTPUT_PORT,
	},
};

struct msm_vidc_format_constraint enc_pix_format_constraints[] = {
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
	{
		.fourcc = V4L2_PIX_FMT_NV12_512,
		.num_planes = 2,
		.y_stride_multiples = 512,
		.y_max_stride = 8192,
		.y_min_plane_buffer_height_multiple = 512,
		.y_buffer_alignment = 512,
		.uv_stride_multiples = 512,
		.uv_max_stride = 8192,
		.uv_min_plane_buffer_height_multiple = 256,
		.uv_buffer_alignment = 256,
	},
};


static int msm_venc_set_csc(struct msm_vidc_inst *inst,
					u32 color_primaries, u32 custom_matrix);

int msm_venc_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_format *fmt = NULL;

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
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_DYNAMIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->prop.fps = DEFAULT_FPS;
	inst->capability.pixelprocess_capabilities = 0;
	/* To start with, both ports are 1 plane each */
	inst->bufq[OUTPUT_PORT].num_planes = 1;
	inst->bufq[CAPTURE_PORT].num_planes = 1;
	inst->clk_data.operating_rate = 0;

	inst->buff_req.buffer[1].buffer_type = HAL_BUFFER_INPUT;
	inst->buff_req.buffer[1].buffer_count_min_host =
	inst->buff_req.buffer[1].buffer_count_actual =
		MIN_NUM_ENC_OUTPUT_BUFFERS;
	inst->buff_req.buffer[2].buffer_type = HAL_BUFFER_OUTPUT;
	inst->buff_req.buffer[2].buffer_count_min_host =
	inst->buff_req.buffer[2].buffer_count_actual =
		MIN_NUM_ENC_CAPTURE_BUFFERS;
	inst->buff_req.buffer[3].buffer_type = HAL_BUFFER_OUTPUT2;
	inst->buff_req.buffer[3].buffer_count_min_host =
	inst->buff_req.buffer[3].buffer_count_actual =
		MIN_NUM_ENC_CAPTURE_BUFFERS;
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

	/* By default, initialize OUTPUT port to UBWC YUV format */
	fmt = msm_comm_get_pixel_fmt_fourcc(venc_formats,
		ARRAY_SIZE(venc_formats), V4L2_PIX_FMT_NV12_UBWC,
			OUTPUT_PORT);
	if (!fmt || fmt->type != OUTPUT_PORT) {
		dprintk(VIDC_ERR,
			"venc_formats corrupted\n");
		return -EINVAL;
	}
	memcpy(&inst->fmts[fmt->type], fmt,
			sizeof(struct msm_vidc_format));

	/* By default, initialize CAPTURE port to H264 encoder */
	fmt = msm_comm_get_pixel_fmt_fourcc(venc_formats,
		ARRAY_SIZE(venc_formats), V4L2_PIX_FMT_H264,
			CAPTURE_PORT);
	if (!fmt || fmt->type != CAPTURE_PORT) {
		dprintk(VIDC_ERR,
			"venc_formats corrupted\n");
		return -EINVAL;
	}
	memcpy(&inst->fmts[fmt->type], fmt,
			sizeof(struct msm_vidc_format));

	return rc;
}

int msm_venc_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;

	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, f = %pK\n", inst, f);
		return -EINVAL;
	}
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_index(venc_formats,
			ARRAY_SIZE(venc_formats), f->index, CAPTURE_PORT);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_index(venc_formats,
			ARRAY_SIZE(venc_formats), f->index, OUTPUT_PORT);
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

static int msm_venc_set_csc(struct msm_vidc_inst *inst,
					u32 color_primaries, u32 custom_matrix)
{
	int rc = 0;
	int count = 0;
	struct hal_vpe_color_space_conversion vpe_csc;
	struct msm_vidc_platform_resources *resources;
	u32 *bias_coeff = NULL;
	u32 *csc_limit = NULL;
	u32 *csc_matrix = NULL;

	resources = &(inst->core->resources);
	bias_coeff =
		resources->csc_coeff_data->vpe_csc_custom_bias_coeff;
	csc_limit =
		resources->csc_coeff_data->vpe_csc_custom_limit_coeff;
	csc_matrix =
		resources->csc_coeff_data->vpe_csc_custom_matrix_coeff;

	vpe_csc.input_color_primaries = color_primaries;
	/* Custom bias, matrix & limit */
	vpe_csc.custom_matrix_enabled = custom_matrix;

	if (vpe_csc.custom_matrix_enabled && bias_coeff != NULL
			&& csc_limit != NULL && csc_matrix != NULL) {
		while (count < HAL_MAX_MATRIX_COEFFS) {
			if (count < HAL_MAX_BIAS_COEFFS)
				vpe_csc.csc_bias[count] =
					bias_coeff[count];
			if (count < HAL_MAX_LIMIT_COEFFS)
				vpe_csc.csc_limit[count] =
					csc_limit[count];
			vpe_csc.csc_matrix[count] =
				csc_matrix[count];
			count = count + 1;
		}
	}
	rc = msm_comm_try_set_prop(inst,
			HAL_PARAM_VPE_COLOR_SPACE_CONVERSION, &vpe_csc);
	if (rc)
		dprintk(VIDC_ERR, "Setting VPE coefficients failed\n");

	return rc;
}

int msm_venc_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	int rc = 0;
	int i = 0;
	struct msm_vidc_format *fmt = NULL;

	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, format = %pK\n", inst, f);
		return -EINVAL;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {

		fmt = msm_comm_get_pixel_fmt_fourcc(venc_formats,
			ARRAY_SIZE(venc_formats), f->fmt.pix_mp.pixelformat,
			CAPTURE_PORT);
		if (!fmt || fmt->type != CAPTURE_PORT) {
			dprintk(VIDC_ERR,
				"Format: %d not supported on CAPTURE port\n",
				f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto exit;
		}
		memcpy(&inst->fmts[fmt->type], fmt,
				sizeof(struct msm_vidc_format));

		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
			goto exit;
		}

		inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto exit;
		}

		/*
		 * update bitstream buffer size based on width & height
		 * updating extradata buffer size is not required as
		 * it is already updated when extradata control is set
		 */
		inst->bufq[fmt->type].plane_sizes[0] =
			inst->fmts[fmt->type].get_frame_size(0,
			f->fmt.pix_mp.height, f->fmt.pix_mp.width);

		f->fmt.pix_mp.num_planes = inst->bufq[fmt->type].num_planes;
		for (i = 0; i < inst->bufq[fmt->type].num_planes; i++) {
			f->fmt.pix_mp.plane_fmt[i].sizeimage =
				inst->bufq[fmt->type].plane_sizes[i];
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_fourcc(venc_formats,
			ARRAY_SIZE(venc_formats), f->fmt.pix_mp.pixelformat,
			OUTPUT_PORT);
		if (!fmt || fmt->type != OUTPUT_PORT) {
			dprintk(VIDC_ERR,
				"Format: %d not supported on OUTPUT port\n",
				f->fmt.pix_mp.pixelformat);
				rc = -EINVAL;
			goto exit;
		}
		memcpy(&inst->fmts[fmt->type], fmt,
			sizeof(struct msm_vidc_format));

		inst->clk_data.opb_fourcc = f->fmt.pix_mp.pixelformat;
		inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto exit;
		}

		/*
		 * update bitstream buffer size based on width & height
		 * updating extradata buffer size is not required as
		 * it is already updated when extradata control is set
		 */
		inst->bufq[fmt->type].plane_sizes[0] =
			inst->fmts[fmt->type].get_frame_size(0,
			f->fmt.pix_mp.height, f->fmt.pix_mp.width);
		f->fmt.pix_mp.num_planes = inst->bufq[fmt->type].num_planes;
		for (i = 0; i < inst->bufq[fmt->type].num_planes; i++) {
			f->fmt.pix_mp.plane_fmt[i].sizeimage =
				inst->bufq[fmt->type].plane_sizes[i];
		}

	} else {
		dprintk(VIDC_ERR, "%s - Unsupported buf type: %d\n",
			__func__, f->type);
		rc = -EINVAL;
		goto exit;
	}
exit:
	return rc;
}

int msm_venc_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops)
{
	return msm_comm_ctrl_init(inst, msm_venc_ctrls,
			ARRAY_SIZE(msm_venc_ctrls), ctrl_ops);
}

struct v4l2_ctrl *msm_venc_get_ctrl(struct msm_vidc_inst *inst, u32 id)
{
	int i;
	struct v4l2_ctrl *ctrl;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(msm_venc_ctrls); i++) {
		ctrl = inst->ctrls[i];
		if (ctrl->id == id)
			return ctrl;
	}

	dprintk(VIDC_ERR, "%s: control id (%#x) not found\n", __func__, id);
	return NULL;
}

int msm_venc_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_mastering_display_colour_sei_payload *mdisp_sei = NULL;
	struct msm_vidc_content_light_level_sei_payload *cll_sei = NULL;
	struct hal_buffer_requirements *buff_req_buffer = NULL;

	if (!inst || !inst->core || !inst->core->device || !ctrl) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	mdisp_sei = &(inst->hdr10_sei_params.disp_color_sei);
	cll_sei = &(inst->hdr10_sei_params.cll_sei);

	/*
	 * Unlock the control prior to setting to the hardware. Otherwise
	 * lower level code that attempts to do a get_ctrl() will end up
	 * deadlocking.
	 */
	v4l2_ctrl_unlock(ctrl);

	dprintk(VIDC_DBG,
		"%s: %x : name %s, id 0x%x value %d\n",
		__func__, hash32_ptr(inst->session), ctrl->name,
		ctrl->id, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_intra_period(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set intra period failed.\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_request_keyframe(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set bitrate failed\n", __func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	{
		struct v4l2_ctrl *rc_enable = try_get_ctrl(
			V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE, ctrl);
		if (!rc_enable->val) {
			dprintk(VIDC_ERR,
				"RC is not enabled. Cannot set RC mode\n");
			rc = -ENOTSUPP;
			break;
		}
		if ((ctrl->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ) &&
			inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC) {
			dprintk(VIDC_ERR, "CQ supported only for HEVC\n");
			rc = -ENOTSUPP;
			break;
		}
		break;
	}
	case V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY:
		inst->frame_quality = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE:
		inst->grid_enable = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		inst->clk_data.bitrate = ctrl->val;
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_bitrate(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set bitrate failed\n", __func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC &&
			inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264) {
			dprintk(VIDC_ERR,
				"Slice mode not supported for encoder %#x\n",
				inst->fmts[CAPTURE_PORT].fourcc);
			rc = -ENOTSUPP;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags &= ~VIDC_SECURE;
		if (ctrl->val)
			inst->flags |= VIDC_SECURE;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_ltr_useframe(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: ltr useframe failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_ltr_markframe(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: ltr markframe failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DYN_QP:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_dyn_qp(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: ltr markframe failed\n",
					__func__);
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
			dprintk(VIDC_DBG, "inst(%pK) Request for turbo mode\n",
				inst);
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
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_operating_rate(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set operating rate failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
		inst->clk_data.low_latency_mode = (bool)ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VENC_HDR_INFO: {
		u32 info_type = (ctrl->val >> 28);
		u32 val = (ctrl->val & 0xFFFFFFF);

		switch (info_type) {
		case MSM_VIDC_RGB_PRIMARY_00:
			mdisp_sei->nDisplayPrimariesX[0] = val;
			break;
		case MSM_VIDC_RGB_PRIMARY_01:
			mdisp_sei->nDisplayPrimariesY[0] = val;
			break;
		case MSM_VIDC_RGB_PRIMARY_10:
			mdisp_sei->nDisplayPrimariesX[1] = val;
			break;
		case MSM_VIDC_RGB_PRIMARY_11:
			mdisp_sei->nDisplayPrimariesY[1] = val;
			break;
		case MSM_VIDC_RGB_PRIMARY_20:
			mdisp_sei->nDisplayPrimariesX[2] = val;
			break;
		case MSM_VIDC_RGB_PRIMARY_21:
			mdisp_sei->nDisplayPrimariesY[2] = val;
			break;
		case MSM_VIDC_WHITEPOINT_X:
			mdisp_sei->nWhitePointX = val;
			break;
		case MSM_VIDC_WHITEPOINT_Y:
			mdisp_sei->nWhitePointY = val;
			break;
		case MSM_VIDC_MAX_DISP_LUM:
			mdisp_sei->nMaxDisplayMasteringLuminance = val;
			break;
		case MSM_VIDC_MIN_DISP_LUM:
			mdisp_sei->nMinDisplayMasteringLuminance = val;
			break;
		case MSM_VIDC_RGB_MAX_CLL:
			cll_sei->nMaxContentLight = val;
			break;
		case MSM_VIDC_RGB_MAX_FLL:
			cll_sei->nMaxPicAverageLight = val;
			break;
		default:
			dprintk(VIDC_ERR,
				"Unknown Ctrl:%d, not part of HDR Info",
					info_type);
			}
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		inst->bufq[OUTPUT_PORT].num_planes = 1;
		inst->bufq[CAPTURE_PORT].num_planes = 1;

		if (ctrl->val & EXTRADATA_ADVANCED)
			inst->bufq[CAPTURE_PORT].num_planes = 2;

		if ((ctrl->val & EXTRADATA_ENC_INPUT_ROI) ||
			(ctrl->val & EXTRADATA_ENC_INPUT_HDR10PLUS))
			inst->bufq[OUTPUT_PORT].num_planes = 2;

		/* Needs internal calculation of extradata */
		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
			break;
		}

		if (inst->bufq[OUTPUT_PORT].num_planes == 2) {
			buff_req_buffer = get_buff_req_buffer(inst,
						HAL_BUFFER_EXTRADATA_INPUT);
			if (!buff_req_buffer) {
				dprintk(VIDC_ERR,
				"Failed to get extradata buff info\n");
				rc = -EINVAL;
				break;
			}

			inst->bufq[OUTPUT_PORT].plane_sizes[1] =
					buff_req_buffer->buffer_size;
		}

		if (inst->bufq[CAPTURE_PORT].num_planes == 2) {
			buff_req_buffer = get_buff_req_buffer(inst,
						HAL_BUFFER_EXTRADATA_OUTPUT);
			if (!buff_req_buffer) {
				dprintk(VIDC_ERR,
				"Failed to get extradata buff info\n");
				rc = -EINVAL;
				break;
			}

			inst->bufq[CAPTURE_PORT].plane_sizes[1] =
					buff_req_buffer->buffer_size;
		}

		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
	case V4L2_CID_ROTATE:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER:
	case V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT:
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:
	case V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER:
	case V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE:
	case V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
	case V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE:
	case V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE:
	case V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS:
	case V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX:
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
	case V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO:
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT:
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS:
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
			dprintk(VIDC_DBG, "Control set: ID : %x Val : %d\n",
			ctrl->id, ctrl->val);
		break;
	default:
		dprintk(VIDC_ERR, "Unsupported index: %x\n", ctrl->id);
		rc = -ENOTSUPP;
		break;
	}

	v4l2_ctrl_lock(ctrl);
	return rc;
}

int msm_venc_set_frame_size(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_frame_size frame_sz;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	frame_sz.buffer_type = HAL_BUFFER_INPUT;
	frame_sz.width = inst->prop.width[OUTPUT_PORT];
	frame_sz.height = inst->prop.height[OUTPUT_PORT];
	dprintk(VIDC_DBG, "%s: input %d %d\n", __func__,
			frame_sz.width, frame_sz.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_FRAME_SIZE, &frame_sz);
	if (rc) {
		dprintk(VIDC_ERR, "%s: failed to set input frame size %d %d\n",
			__func__, frame_sz.width, frame_sz.height);
		return rc;
	}

	frame_sz.buffer_type = HAL_BUFFER_OUTPUT;
	frame_sz.width = inst->prop.width[CAPTURE_PORT];
	frame_sz.height = inst->prop.height[CAPTURE_PORT];
	dprintk(VIDC_DBG, "%s: output %d %d\n", __func__,
			frame_sz.width, frame_sz.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_FRAME_SIZE, &frame_sz);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed to set output frame size %d %d\n",
			__func__, frame_sz.width, frame_sz.height);
		return rc;
	}

	return rc;
}

int msm_venc_set_color_format(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_format_constraint *fmt_constraints;

	rc = msm_comm_set_color_format(inst, HAL_BUFFER_INPUT,
				inst->fmts[OUTPUT_PORT].fourcc);
	if (rc)
		return rc;

	fmt_constraints = msm_comm_get_pixel_fmt_constraints(
			enc_pix_format_constraints,
			ARRAY_SIZE(enc_pix_format_constraints),
			inst->fmts[OUTPUT_PORT].fourcc);
	if (fmt_constraints) {
		rc = msm_comm_set_color_format_constraints(inst,
				HAL_BUFFER_INPUT,
				fmt_constraints);
		if (rc) {
			dprintk(VIDC_ERR, "Set constraints for %d failed\n",
				inst->fmts[OUTPUT_PORT].fourcc);
			return rc;
		}
	}

	return rc;
}

int msm_venc_set_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hal_buffer_requirements *bufreq;
	enum hal_buffer buffer_type;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	buffer_type = HAL_BUFFER_INPUT;
	bufreq = get_buff_req_buffer(inst, buffer_type);
	if (!bufreq) {
		dprintk(VIDC_ERR,
			"%s: failed to get bufreqs(%#x)\n",
			__func__, buffer_type);
		return -EINVAL;
	}
	rc = msm_comm_set_buffer_count(inst,
			bufreq->buffer_count_min,
			bufreq->buffer_count_actual,
			buffer_type);
	if (rc) {
		dprintk(VIDC_ERR, "%s: failed to set bufreqs(%#x)\n",
			__func__, buffer_type);
		return -EINVAL;
	}

	buffer_type = HAL_BUFFER_OUTPUT;
	bufreq = get_buff_req_buffer(inst, buffer_type);
	if (!bufreq) {
		dprintk(VIDC_ERR, "%s: failed to get bufreqs(%#x)\n",
			__func__, buffer_type);
		return -EINVAL;
	}
	rc = msm_comm_set_buffer_count(inst,
			bufreq->buffer_count_min,
			bufreq->buffer_count_actual,
			buffer_type);
	if (rc) {
		dprintk(VIDC_ERR, "%s: failed to set bufreqs(%#x)\n",
			__func__, buffer_type);
		return -EINVAL;
	}

	return rc;
}

int msm_venc_set_secure_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SECURE);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get secure mode failed\n", __func__);
		return -EINVAL;
	}
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_SECURE, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_priority(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get priority failed\n", __func__);
		return -EINVAL;
	}
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_REALTIME, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_operating_rate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_operating_rate op_rate;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get operating rate failed\n", __func__);
		return -EINVAL;
	}
	op_rate.operating_rate = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, op_rate.operating_rate >> 16);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_OPERATING_RATE, &op_rate);
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}

	return rc;
}

int msm_venc_set_profile_level(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *profile;
	struct v4l2_ctrl *level;
	struct v4l2_ctrl *tier;
	struct hal_profile_level profile_level;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	switch (inst->fmts[CAPTURE_PORT].fourcc) {
	case V4L2_PIX_FMT_H264:
		profile = msm_venc_get_ctrl(inst,
				V4L2_CID_MPEG_VIDEO_H264_PROFILE);
		level = msm_venc_get_ctrl(inst,
				V4L2_CID_MPEG_VIDEO_H264_LEVEL);
		if (!profile || !level) {
			dprintk(VIDC_ERR,
				"%s: get h264 profile & level failed\n",
				__func__);
			return -EINVAL;
		}

		profile_level.profile =
			msm_comm_v4l2_to_hal(profile->id, profile->val);
		profile_level.level =
			msm_comm_v4l2_to_hal(level->id, level->val);
		break;
	case V4L2_PIX_FMT_HEVC:
		profile = msm_venc_get_ctrl(inst,
				V4L2_CID_MPEG_VIDEO_HEVC_PROFILE);
		level = msm_venc_get_ctrl(inst,
				V4L2_CID_MPEG_VIDEO_HEVC_LEVEL);
		tier = msm_venc_get_ctrl(inst,
				V4L2_CID_MPEG_VIDEO_HEVC_TIER);
		if (!profile || !level || !tier) {
			dprintk(VIDC_ERR,
				"%s: get hevc profile & level failed\n",
				__func__);
			return -EINVAL;
		}

		profile_level.profile =
			msm_comm_v4l2_to_hal(profile->id, profile->val);
		profile_level.level =
			msm_comm_v4l2_to_hal(level->id, level->val);
		break;
	case V4L2_PIX_FMT_VP8:
		level = msm_venc_get_ctrl(inst,
				V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL);
		if (!level) {
			dprintk(VIDC_ERR,

				"%s: get vp8 level failed\n", __func__);
			return -EINVAL;
		}
		profile_level.profile =
			HAL_VP8_PROFILE_MAIN;
		profile_level.level =
			msm_comm_v4l2_to_hal(level->id, level->val);
		break;
	default:
		dprintk(VIDC_ERR, "%s: unknown fourcc %#x\n", __func__,
			inst->fmts[CAPTURE_PORT].fourcc);
		return -EINVAL;
	}

	dprintk(VIDC_DBG, "%s: %#x %#x\n", __func__,
		profile_level.profile, profile_level.level);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_PROFILE_LEVEL_CURRENT, &profile_level);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_idr_period(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_idr_period idr_period;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	idr_period.idr_period = 1;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, idr_period.idr_period);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_IDR_PERIOD, &idr_period);
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}

	return rc;
}

int msm_venc_set_intra_period(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_intra_period intra_period;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_GOP_SIZE);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get num pframes failed\n", __func__);
		return -EINVAL;
	}
	intra_period.pframes = ctrl->val;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get num bframes failed\n", __func__);
		return -EINVAL;
	}
	intra_period.bframes = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__, intra_period.pframes,
		intra_period.bframes);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_INTRA_PERIOD, &intra_period);
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}
	return rc;
}

int msm_venc_set_request_keyframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_request_iframe request_iframe;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get request iframe failed\n", __func__);
		return -EINVAL;
	}
	request_iframe.enable = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, request_iframe.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_REQUEST_IFRAME, &request_iframe);
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}

	return rc;
}

int msm_venc_set_adaptive_bframes(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	enable.enable = true;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_ADAPTIVE_B, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_rate_control(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *bitrate_mode;
	struct v4l2_ctrl *hier_layers;
	struct v4l2_ctrl *hier_type;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	bitrate_mode = msm_venc_get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
	if (!bitrate_mode) {
		dprintk(VIDC_ERR, "%s: get bitrate mode failed\n", __func__);
		return -EINVAL;
	}
	if (bitrate_mode->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC) {
		dprintk(VIDC_ERR,
			"%s: CQ supported only for HEVC\n", __func__);
		return -EINVAL;
	}
	if ((bitrate_mode->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR) &&
		(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264)) {
		hier_layers = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);
		hier_type = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE);
		if ((hier_layers && hier_layers->val) &&
			(hier_type && hier_type->val ==
			V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P)){
			dprintk(VIDC_ERR,
				"%s: CBR_VFR not allowed with Hybrid HP\n",
				__func__);
			return -EINVAL;
		}
	}
	dprintk(VIDC_DBG, "%s: %d\n", __func__, bitrate_mode->val);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_RATE_CONTROL, &bitrate_mode->val);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_input_timestamp_rc(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get frame level rc failed\n", __func__);
		return -EINVAL;
	}

	/*
	 * 0 - rate control considers buffer timestamps
	 * 1 - rate control igonres buffer timestamp and
	 *     calculates timedelta based on frame rate
	 */
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_DISABLE_RC_TIMESTAMP, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_bitrate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_bitrate bitrate;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get bitrate failed\n", __func__);
		return -EINVAL;
	}
	bitrate.bit_rate = ctrl->val;
	bitrate.layer_id = MSM_VIDC_ALL_LAYER_ID;
	dprintk(VIDC_DBG, "%s: %d\n", __func__, bitrate.bit_rate);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_TARGET_BITRATE, &bitrate);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_frame_qp(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_quantization qp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	qp.layer_id = MSM_VIDC_ALL_LAYER_ID;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get qpi failed\n", __func__);
		return -EINVAL;
	}
	qp.qpi = ctrl->val;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get qpp failed\n", __func__);
		return -EINVAL;
	}
	qp.qpp = ctrl->val;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get qpb failed\n", __func__);
		return -EINVAL;
	}
	qp.qpb = ctrl->val;

	/* This should happen based on which controls are set */
	qp.enable = QP_ENABLE_I | QP_ENABLE_P | QP_ENABLE_B;
	dprintk(VIDC_DBG, "%s: layers %#x frames %#x qpi %#x qpp %#x qpb %#x\n",
		__func__, qp.layer_id, qp.enable, qp.qpi, qp.qpp, qp.qpb);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_FRAME_QP, &qp);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_qp_range(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_quantization_range qp_range;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	qp_range.layer_id = MSM_VIDC_ALL_LAYER_ID;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get qpi_min failed\n", __func__);
		return -EINVAL;
	}
	qp_range.qpi_min = ctrl->val;
	qp_range.qpp_min = ctrl->val;
	qp_range.qpb_min = ctrl->val;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get qpi_max failed\n", __func__);
		return -EINVAL;
	}
	qp_range.qpi_max = ctrl->val;
	qp_range.qpp_max = ctrl->val;
	qp_range.qpb_max = ctrl->val;

	dprintk(VIDC_DBG,
		"%s: layers %#x qpi_min %#x qpi_max %#x qpp_min %#x qpp_max %#x qpb_min %#x qpb_max %#x\n",
		__func__, qp_range.layer_id,
		qp_range.qpi_min, qp_range.qpi_max,
		qp_range.qpp_min, qp_range.qpp_max,
		qp_range.qpb_min, qp_range.qpb_max);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_SESSION_QP_RANGE, &qp_range);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_frame_quality(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *ctrl_t;
	struct hal_heic_frame_quality frame_quality;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl_t = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
	if (!ctrl_t) {
		dprintk(VIDC_ERR, "%s: get bitrate mode failed\n", __func__);
		return -EINVAL;
	}
	if (ctrl_t->val != V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		return 0;

	ctrl = msm_venc_get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get frame quality failed\n", __func__);
		return -EINVAL;
	}
	frame_quality.frame_quality = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, frame_quality.frame_quality);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_HEIC_FRAME_QUALITY, &frame_quality);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_grid(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *ctrl_t;
	struct hal_heic_grid_enable grid_enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl_t = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
	if (!ctrl_t) {
		dprintk(VIDC_ERR, "%s: get bitrate mode failed\n", __func__);
		return -EINVAL;
	}
	if (ctrl_t->val != V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		return 0;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get grid enable failed\n", __func__);
		return -EINVAL;
	}

	/* Need a change in HFI if we want to pass size */
	if (!ctrl->val)
		grid_enable.grid_enable = false;
	else
		grid_enable.grid_enable = true;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, grid_enable.grid_enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_HEIC_GRID_ENABLE, &grid_enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_entropy_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_h264_entropy_control entropy;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get entropy mode failed\n", __func__);
		return -EINVAL;
	}
	entropy.entropy_mode = msm_comm_v4l2_to_hal(
			V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
			ctrl->val);

	dprintk(VIDC_DBG, "%s: %d\n", __func__, entropy.entropy_mode);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_H264_ENTROPY_CONTROL, &entropy);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_slice_control_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *ctrl_t;
	struct hal_multi_slice_control multi_slice_control;
	int temp = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get multi slice mode failed\n", __func__);
		return -EINVAL;
	}
	switch (ctrl->val) {
	case V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB:
		temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
		break;
	case V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES:
		temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES;
		break;
	case V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE:
	default:
		temp = 0;
		break;
	}
	if (temp) {
		ctrl_t = msm_venc_get_ctrl(inst, temp);
		if (!ctrl_t) {
			dprintk(VIDC_ERR,
				"%s: get slice mode failed\n", __func__);
			return -EINVAL;
		}
	}
	multi_slice_control.multi_slice = ctrl->val;
	multi_slice_control.slice_size = temp ? ctrl_t->val : 0;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
			multi_slice_control.multi_slice,
			multi_slice_control.slice_size);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_MULTI_SLICE_CONTROL,
			&multi_slice_control);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_intra_refresh_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_intra_refresh intra_refresh;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get intra_refresh random failed\n", __func__);
		return -EINVAL;
	}
	if (ctrl->val) {
		/* ignore cyclic mode if random mode is set */
		intra_refresh.mode = HAL_INTRA_REFRESH_RANDOM;
		intra_refresh.ir_mbs = ctrl->val;
	} else {
		ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB);
		if (!ctrl) {
			dprintk(VIDC_ERR,
				"%s: get intra_refresh cyclic failed\n",
				__func__);
			return -EINVAL;
		}
		if (ctrl->val)
			return 0;

		intra_refresh.mode = HAL_INTRA_REFRESH_CYCLIC;
		intra_refresh.ir_mbs = ctrl->val;
	}
	if (!intra_refresh.ir_mbs)
		return 0;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
			intra_refresh.mode, intra_refresh.ir_mbs);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_INTRA_REFRESH, &intra_refresh);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_loop_filter_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *ctrl_a;
	struct v4l2_ctrl *ctrl_b;
	struct hal_h264_db_control h264_db_control;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get loop filter mode failed\n", __func__);
		return -EINVAL;
	}

	ctrl_a = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA);
	if (!ctrl_a) {
		dprintk(VIDC_ERR,
			"%s: get loop filter alpha failed\n", __func__);
		return -EINVAL;
	}
	ctrl_b = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA);
	if (!ctrl_b) {
		dprintk(VIDC_ERR,
			"%s: get loop filter beta failed\n", __func__);
		return -EINVAL;
	}
	h264_db_control.mode = msm_comm_v4l2_to_hal(
			V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
			ctrl->val);
	h264_db_control.slice_alpha_offset = ctrl_a->val;
	h264_db_control.slice_beta_offset = ctrl_b->val;

	dprintk(VIDC_DBG, "%s: %d %d %d\n", __func__,
		h264_db_control.mode, h264_db_control.slice_alpha_offset,
		h264_db_control.slice_beta_offset);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HAL_PARAM_VENC_H264_DEBLOCK_CONTROL, &h264_db_control);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_sequence_header_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get sequence header mode failed\n", __func__);
		return -EINVAL;
	}

	if (ctrl->val)
		enable.enable = true;
	else
		enable.enable = false;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_au_delimiter_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get au delimiter failed\n", __func__);
		return -EINVAL;
	}
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_GENERATE_AUDNAL, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_hierp_check(struct msm_vidc_inst *inst, u32 value)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (value > inst->capability.hier_p.max) {
		dprintk(VIDC_ERR,
			"%s: hierp value (%d) > max supported (%d)\n",
			__func__, value,
			inst->capability.hier_p.max);
		return -EINVAL;
	}

	return 0;
}

int msm_venc_hybrid_hp_check(struct msm_vidc_inst *inst, bool *hyb_hp)
{
	struct v4l2_ctrl *bitrate_mode;
	struct v4l2_ctrl *rc_enable;
	*hyb_hp = false;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	bitrate_mode = msm_venc_get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
	if (!bitrate_mode) {
		dprintk(VIDC_ERR, "%s: get bitrate mode failed\n", __func__);
		return -EINVAL;
	}

	rc_enable = msm_venc_get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE);
	if (!rc_enable) {
		dprintk(VIDC_ERR, "%s: get bitrate mode failed\n", __func__);
		return -EINVAL;
	}

	if (rc_enable->val &&
		bitrate_mode->val == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
		*hyb_hp = true;
	return 0;
}

int msm_venc_set_base_layer_id(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	u32 baselayerid;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get base layer id failed\n", __func__);
		return -EINVAL;
	}

	baselayerid = ctrl->val;
	rc = msm_venc_hierp_check(inst, baselayerid);
	if (rc) {
		dprintk(VIDC_ERR, "%s: hierp check failed\n", __func__);
		return rc;
	}

	dprintk(VIDC_DBG, "%s: %d\n", __func__, baselayerid);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_BASELAYER_PRIORITYID, &baselayerid);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_hierp_layers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	u32 hierp_layers;
	struct hal_hybrid_hierp hyb_hierp;
	bool hyb_hp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get heirp num layers failed\n", __func__);
		return -EINVAL;
	}

	rc = msm_venc_hierp_check(inst, ctrl->val);
	if (rc) {
		dprintk(VIDC_ERR, "%s: hierp check failed\n", __func__);
		return rc;
	}

	rc = msm_venc_hybrid_hp_check(inst, &hyb_hp);
	if (rc) {
		dprintk(VIDC_ERR, "%s: hybrid hp check failed\n", __func__);
		return rc;
	}

	if (hyb_hp) {
		hyb_hierp.layers = ctrl->val;
		dprintk(VIDC_DBG, "%s: %d\n", __func__, hyb_hierp.layers);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_VENC_HIER_P_HYBRID_MODE, &hyb_hierp);
		if (rc)
			dprintk(VIDC_ERR,
				"%s: set property failed\n", __func__);
	} else {
		hierp_layers = ctrl->val;
		dprintk(VIDC_DBG, "%s: %d\n", __func__, hierp_layers);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_CONFIG_VENC_HIER_P_NUM_FRAMES,
				&hierp_layers);
		if (rc)
			dprintk(VIDC_ERR,
				"%s: set property failed\n", __func__);
	}
	return rc;
}

int msm_venc_set_vpx_error_resilience(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_VP8)
		return 0;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get vpx error resilience failed\n", __func__);
		return -EINVAL;
	}
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_video_signal_info(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl_cs;
	struct v4l2_ctrl *ctrl_fr;
	struct v4l2_ctrl *ctrl_tr;
	struct v4l2_ctrl *ctrl_mc;
	struct hal_video_signal_info signal_info;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	ctrl_cs = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
	ctrl_fr = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE);
	ctrl_tr = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS);
	ctrl_mc = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS);
	if (!ctrl_cs || !ctrl_fr || !ctrl_tr || !ctrl_mc) {
		dprintk(VIDC_ERR, "%s: get ctrls\n", __func__);
		return -EINVAL;
	}
	if (ctrl_cs->val == MSM_VIDC_RESERVED_1)
		return 0;

	signal_info.color_space = ctrl_cs->val;
	signal_info.full_range = ctrl_fr->val;
	signal_info.transfer_chars = ctrl_tr->val;
	signal_info.matrix_coeffs = ctrl_mc->val;

	dprintk(VIDC_DBG, "%s: %d %d %d %d\n", __func__,
		signal_info.color_space, signal_info.full_range,
		signal_info.transfer_chars, signal_info.matrix_coeffs);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_VIDEO_SIGNAL_INFO, &signal_info);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_video_csc(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *ctrl_cs;
	struct v4l2_ctrl *ctrl_cm;
	u32 color_primaries, custom_matrix;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get csc failed\n", __func__);
		return -EINVAL;
	}
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_DISABLE)
		return 0;

	ctrl_cs = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
	ctrl_cm = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX);
	if (!ctrl_cs || !ctrl_cm) {
		dprintk(VIDC_ERR, "%s: get ctrls\n", __func__);
		return -EINVAL;
	}

	color_primaries = ctrl_cs->val;
	custom_matrix = ctrl_cm->val;
	rc = msm_venc_set_csc(inst, color_primaries, custom_matrix);
	if (rc)
		dprintk(VIDC_ERR, "%s: msm_venc_set_csc failed\n", __func__);

	return rc;
}

int msm_venc_set_low_latency_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get lowlatency mode failed\n", __func__);
		return -EINVAL;
	}
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_LOW_LATENCY, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_8x8_transform(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *profile;
	struct hal_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264) {
		profile = msm_venc_get_ctrl(inst,
				V4L2_CID_MPEG_VIDEO_H264_PROFILE);
		if (!profile) {
			dprintk(VIDC_ERR,
				"%s: get h264 profile failed\n", __func__);
			return -EINVAL;
		}
		if (profile->val == V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE ||
			profile->val ==
			V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE)
			return 0;
	}

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get 8x8 transform failed\n", __func__);
		return -EINVAL;
	}
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_H264_TRANSFORM_8x8, &enable);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_vui_timing_info(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *ctrl_t;
	struct hal_vui_timing_info timing_info;
	bool cfr;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get vui timing info failed\n", __func__);
		return -EINVAL;
	}
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_DISABLE)
		return 0;

	ctrl_t = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
	if (!ctrl_t) {
		dprintk(VIDC_ERR, "%s: get bitrate mode failed\n", __func__);
		return -EINVAL;
	}
	switch (ctrl_t->val) {
	case V4L2_MPEG_VIDEO_BITRATE_MODE_VBR:
	case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR:
	case V4L2_MPEG_VIDEO_BITRATE_MODE_MBR:
		cfr = true;
		break;
	default:
		cfr = false;
		break;
	}

	timing_info.enable = 1;
	timing_info.fixed_frame_rate = cfr;
	timing_info.time_scale = NSEC_PER_SEC;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__, timing_info.enable,
		timing_info.fixed_frame_rate);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_VUI_TIMING_INFO, &timing_info);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}
int msm_venc_set_nal_stream_format(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_nal_stream_format_select stream_format;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: get nal stream format failed\n", __func__);
		return -EINVAL;
	}
	stream_format.nal_stream_format_select = BIT(ctrl->val);

	dprintk(VIDC_DBG, "%s: %#x\n", __func__,
			stream_format.nal_stream_format_select);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_NAL_STREAM_FORMAT_SELECT, &stream_format);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_ltr_mode ltr_mode;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get ltr count failed\n", __func__);
		return -EINVAL;
	}
	if (!ctrl->val)
		return 0;
	if (ctrl->val > inst->capability.ltr_count.max) {
		dprintk(VIDC_ERR, "%s: invalid ltr count %d, max %d\n",
			__func__, ctrl->val > inst->capability.ltr_count.max);
		return -EINVAL;
	}
	ltr_mode.count =  ctrl->val;
	ltr_mode.mode = HAL_LTR_MODE_MANUAL;
	ltr_mode.trust_mode = 1;
	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
			ltr_mode.mode, ltr_mode.count);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_LTRMODE, &ltr_mode);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_useframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_ltr_use use_ltr;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get ltr frame failed\n", __func__);
		return -EINVAL;
	}
	use_ltr.ref_ltr = ctrl->val;
	use_ltr.use_constraint = false;
	use_ltr.frames = 0;
	dprintk(VIDC_DBG, "%s: %d\n", __func__, use_ltr.ref_ltr);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_USELTRFRAME, &use_ltr);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_markframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_ltr_mark mark_ltr;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get ltr count failed\n", __func__);
		return -EINVAL;
	}
	mark_ltr.mark_frame = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, mark_ltr.mark_frame);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_MARKLTRFRAME, &mark_ltr);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_dyn_qp(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_quantization quant;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_DYN_QP);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get dyn qp ctrl failed\n", __func__);
		return -EINVAL;
	}

	quant.qpi = ctrl->val;
	quant.qpp = ctrl->val;
	quant.qpb = ctrl->val;
	quant.enable = QP_ENABLE_I | QP_ENABLE_P | QP_ENABLE_B;
	quant.layer_id = MSM_VIDC_ALL_LAYER_ID;

	dprintk(VIDC_DBG, "%s: %d\n", __func__,
			ctrl->val);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_FRAME_QP, &quant);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_aspect_ratio(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_aspect_ratio sar;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get sar width failed\n", __func__);
		return -EINVAL;
	}
	if (!ctrl->val)
		return 0;
	sar.aspect_width = ctrl->val;

	ctrl = msm_venc_get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get sar height failed\n", __func__);
		return -EINVAL;
	}
	if (!ctrl->val)
		return 0;
	sar.aspect_height = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
		sar.aspect_width, sar.aspect_height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PROPERTY_PARAM_VENC_ASPECT_RATIO, &sar);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_blur_resolution(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_frame_size blur_res;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_venc_get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS);
	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get blur width failed\n", __func__);
		return -EINVAL;
	}
	if (!ctrl->val)
		return 0;

	blur_res.width = (0xFFFF & ctrl->val);
	blur_res.height = (0xFFFF0000 & ctrl->val) >> 16;
	blur_res.buffer_type = HAL_BUFFER_INPUT;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
			blur_res.width, blur_res.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_VENC_BLUR_RESOLUTION, &blur_res);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_hdr_info(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	dprintk(VIDC_DBG, "%s: setting hdr info\n", __func__);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VENC_HDR10_PQ_SEI, &inst->hdr10_sei_params);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_extradata(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *ctrl = msm_venc_get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA);

	if (!ctrl) {
		dprintk(VIDC_ERR, "%s: get extradata control failed\n",
			__func__);
		return -EINVAL;
	}

	if (ctrl->val == EXTRADATA_NONE) {
		// Disable all Extradata
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ASPECT_RATIO, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_LTR_INFO, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ROI_QP, 0x0);
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
					HAL_EXTRADATA_HDR10PLUS_METADATA, 0x0);
		}
	}

	if (ctrl->val & EXTRADATA_ADVANCED)
		// Enable Advanced Extradata - LTR Info
		msm_comm_set_extradata(inst, HAL_EXTRADATA_LTR_INFO, 0x1);

	if (ctrl->val & EXTRADATA_ENC_INPUT_ROI)
		// Enable ROIQP Extradata
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ROI_QP, 0x1);

	if (ctrl->val & EXTRADATA_ENC_INPUT_HDR10PLUS) {
		// Enable HDR10+ Extradata
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
					HAL_EXTRADATA_HDR10PLUS_METADATA, 0x1);
		}
	}

	return rc;
}

int msm_venc_set_properties(struct msm_vidc_inst *inst)
{
	int rc = 0;

	rc = msm_venc_set_frame_size(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_color_format(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_buffer_counts(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_operating_rate(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_secure_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_priority(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_profile_level(inst);
	if (rc)
		goto exit;
	/*
	 * set adaptive bframes before intra period as
	 * intra period setting may enable adaptive bframes
	 * if bframes are present (even though client might not
	 * have enabled adaptive bframes setting)
	 */
	rc = msm_venc_set_adaptive_bframes(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_intra_period(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_idr_period(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_rate_control(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_input_timestamp_rc(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_bitrate(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_frame_qp(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_qp_range(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_frame_quality(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_grid(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_entropy_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_slice_control_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_intra_refresh_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_loop_filter_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_sequence_header_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_au_delimiter_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_base_layer_id(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_hierp_layers(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_vpx_error_resilience(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_low_latency_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_video_signal_info(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_video_csc(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_8x8_transform(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_vui_timing_info(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_nal_stream_format(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_ltr_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_aspect_ratio(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_blur_resolution(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_hdr_info(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_extradata(inst);
	if (rc)
		goto exit;

exit:
	if (rc)
		dprintk(VIDC_ERR, "%s: failed with %d\n", __func__, rc);
	else
		dprintk(VIDC_DBG, "%s: set properties successful\n", __func__);

	return rc;
}

int msm_venc_s_parm(struct msm_vidc_inst *inst, struct v4l2_streamparm *a)
{
	int fps = 0;
	u64 us_per_frame = 0;

	if (!inst || !a) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (a->parm.output.timeperframe.denominator) {
		switch (a->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			us_per_frame = a->parm.output.timeperframe.numerator *
				(u64)USEC_PER_SEC;
			do_div(us_per_frame,
				a->parm.output.timeperframe.denominator);
			break;
		default:
			dprintk(VIDC_ERR, "%s: unknown parm type %d\n",
					__func__, a->type);
			break;
		}
	}
	if (!us_per_frame) {
		dprintk(VIDC_ERR, "%s: time between frames is 0\n", __func__);
		return -EINVAL;
	}
	fps = us_per_frame > USEC_PER_SEC ?
		0 : USEC_PER_SEC / (u32)us_per_frame;

	inst->prop.fps = fps;
	return 0;
}
