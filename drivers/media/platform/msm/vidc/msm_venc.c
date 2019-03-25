// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */
#include <linux/slab.h>
#include "msm_venc.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi.h"
#include "vidc_hfi_helper.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"
#include "msm_vidc_buffer_calculations.h"

#define MIN_BIT_RATE 32000
#define MAX_BIT_RATE 1200000000
#define DEFAULT_BIT_RATE 64000
#define MIN_BIT_RATE_RATIO 0
#define MAX_BIT_RATE_RATIO 100
#define MAX_HIER_CODING_LAYER 6
#define BIT_RATE_STEP 1
#define MAX_BASE_LAYER_PRIORITY_ID 63
#define MAX_SLICE_BYTE_SIZE ((MAX_BIT_RATE)>>3)
#define MIN_SLICE_BYTE_SIZE 512
#define MAX_SLICE_MB_SIZE (((4096 + 15) >> 4) * ((2304 + 15) >> 4))
#define QP_ENABLE_I 0x1
#define QP_ENABLE_P 0x2
#define QP_ENABLE_B 0x4
#define MIN_QP 0
#define MAX_QP 0x7F
#define MAX_QP_PACKED 0x7F7F7F
#define DEFAULT_QP 0xA
#define DEFAULT_QP_PACKED 0xA0A0A
#define MAX_INTRA_REFRESH_MBS ((7680 * 4320) >> 8)
#define MAX_LTR_FRAME_COUNT 10
#define MAX_NUM_B_FRAMES 1
#define MIN_CBRPLUS_W 1280
#define MIN_CBRPLUS_H 720

#define L_MODE V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY
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

static const char *const vp8_profile_level[] = {
	"Unused",
	"0.0",
	"1.0",
	"2.0",
	"3.0",
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

static struct msm_vidc_ctrl msm_venc_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDEO_UNKNOWN,
		.name = "Invalid control",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_GOP_SIZE,
		.name = "Intra Period for P frames",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 2*DEFAULT_FPS-1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP,
		.name = "HEVC I Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_QP,
		.maximum = MAX_QP,
		.default_value = DEFAULT_QP,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP,
		.name = "HEVC P Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_QP,
		.maximum = MAX_QP,
		.default_value = DEFAULT_QP,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP,
		.name = "HEVC B Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_QP,
		.maximum = MAX_QP,
		.default_value = DEFAULT_QP,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP,
		.name = "HEVC Quantization Range Minimum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_QP,
		.maximum = MAX_QP_PACKED,
		.default_value = DEFAULT_QP_PACKED,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP,
		.name = "HEVC Quantization Range Maximum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_QP,
		.maximum = MAX_QP_PACKED,
		.default_value = DEFAULT_QP_PACKED,
		.step = 1,
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
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT,
		.name = "OUTPUT Count",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_NUM_OUTPUT_BUFFERS,
		.maximum = MAX_NUM_OUTPUT_BUFFERS,
		.default_value = MIN_NUM_OUTPUT_BUFFERS,
		.step = 1,
		.qmenu = NULL,
	},

	{
		.id = V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME,
		.name = "Request I Frame",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		.name = "Video Bitrate Control",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		.maximum = V4L2_MPEG_VIDEO_BITRATE_MODE_CQ,
		.default_value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
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
		.id = V4L2_CID_MPEG_VIDEO_BITRATE,
		.name = "Bit Rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = BIT_RATE_STEP,
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
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_HIGH,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_HIGH) |
		(1 << V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH)
		),
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.name = "H264 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_6_2,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_6_2,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
		.name = "VP8 Profile Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3,
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
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		.name = "HEVC Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_LEVEL_1,
		.maximum = V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2,
		.default_value =
			V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2,
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
			EXTRADATA_ENC_INPUT_HDR10PLUS |
			EXTRADATA_ENC_INPUT_CVP,
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
		.maximum = MAX_HIER_CODING_LAYER,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER,
		.name = "Set Hier max layers",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_0,
		.maximum = V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_6,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER_0,
		.step = 1,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE,
		.name = "Set Hier coding type",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		.maximum = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		.default_value = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P)
		),
		.qmenu = NULL,
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
		.minimum = MIN_BIT_RATE_RATIO,
		.maximum = MAX_BIT_RATE_RATIO,
		.default_value = MIN_BIT_RATE_RATIO,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_BR,
		.name = "Set layer1 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE_RATIO,
		.maximum = MAX_BIT_RATE_RATIO,
		.default_value = MIN_BIT_RATE_RATIO,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_BR,
		.name = "Set layer2 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE_RATIO,
		.maximum = MAX_BIT_RATE_RATIO,
		.default_value = MIN_BIT_RATE_RATIO,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_BR,
		.name = "Set layer3 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE_RATIO,
		.maximum = MAX_BIT_RATE_RATIO,
		.default_value = MIN_BIT_RATE_RATIO,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_BR,
		.name = "Set layer4 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE_RATIO,
		.maximum = MAX_BIT_RATE_RATIO,
		.default_value = MIN_BIT_RATE_RATIO,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_BR,
		.name = "Set layer5 BR",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE_RATIO,
		.maximum = MAX_BIT_RATE_RATIO,
		.default_value = MIN_BIT_RATE_RATIO,
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
		.name = "Set Base Layer Priority ID for Hier-P",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_BASE_LAYER_PRIORITY_ID,
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
		.minimum = (MINIMUM_FPS << 16),
		.maximum = INT_MAX,
		.default_value = (DEFAULT_FPS << 16),
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
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
		.id = V4L2_CID_MPEG_VIDC_VENC_RC_TIMESTAMP_DISABLE,
		.name = "RC Timestamp disable",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
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
		.id = V4L2_CID_MPEG_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD,
		.name = "NAL Format",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_SIZE_0,
		.maximum = V4L2_MPEG_VIDEO_HEVC_SIZE_4,
		.default_value = V4L2_MPEG_VIDEO_HEVC_SIZE_0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEVC_SIZE_0) |
		(1 << V4L2_MPEG_VIDEO_HEVC_SIZE_4)
		),
		.qmenu = mpeg_video_stream_format,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_CVP_DISABLE,
		.name = "CVP Disable",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_NATIVE_RECORDER,
		.name = "Enable/Disable Native Recorder",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_BITRATE_SAVINGS,
		.name = "Enable/Disable bitrate savings",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_ENABLE,
		.step = 1,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_venc_ctrls)

static struct msm_vidc_format venc_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.type = OUTPUT_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "YCrCb Semiplanar 4:2:0",
		.description = "Y/CrCb 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.type = OUTPUT_PORT,
	},
	{
		.name = "TP10 UBWC 4:2:0",
		.description = "TP10 UBWC 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC,
		.type = OUTPUT_PORT,
	},
	{
		.name = "TME",
		.description = "TME MBI format",
		.fourcc = V4L2_PIX_FMT_TME,
		.type = CAPTURE_PORT,
		.input_min_count = 4,
		.output_min_count = 4,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 10bit",
		.description = "Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
		.type = OUTPUT_PORT,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 512 aligned",
		.description = "Y/CbCr 4:2:0 512 aligned",
		.fourcc = V4L2_PIX_FMT_NV12_512,
		.type = OUTPUT_PORT,
	},
};

struct msm_vidc_format_constraint enc_pix_format_constraints[] = {
	{
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
		.num_planes = 2,
		.y_max_stride = 8192,
		.y_buffer_alignment = 256,
		.uv_max_stride = 8192,
		.uv_buffer_alignment = 256,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12_512,
		.num_planes = 2,
		.y_max_stride = 8192,
		.y_buffer_alignment = 512,
		.uv_max_stride = 8192,
		.uv_buffer_alignment = 256,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.y_max_stride = 8192,
		.y_buffer_alignment = 256,
		.uv_max_stride = 8192,
		.uv_buffer_alignment = 256,
	},
};


static int msm_venc_set_csc(struct msm_vidc_inst *inst,
					u32 color_primaries, u32 custom_matrix);

int msm_venc_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_format *fmt = NULL;
	struct hal_buffer_requirements *buff_req_buffer = NULL;

	if (!inst) {
		dprintk(VIDC_ERR, "Invalid input = %pK\n", inst);
		return -EINVAL;
	}
	inst->prop.height[CAPTURE_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[CAPTURE_PORT] = DEFAULT_WIDTH;
	inst->prop.height[OUTPUT_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[OUTPUT_PORT] = DEFAULT_WIDTH;
	inst->prop.bframe_changed = false;
	inst->prop.extradata_ctrls = EXTRADATA_DEFAULT;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_DYNAMIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->clk_data.frame_rate = (DEFAULT_FPS << 16);

	inst->bufq[OUTPUT_PORT].num_planes = 2;
	inst->bufq[CAPTURE_PORT].num_planes = 1;
	inst->clk_data.operating_rate = (DEFAULT_FPS << 16);

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
	msm_vidc_init_buffer_size_calculators(inst);

	buff_req_buffer = get_buff_req_buffer(inst,
		HAL_BUFFER_EXTRADATA_INPUT);
	if (!buff_req_buffer) {
		dprintk(VIDC_ERR,
			"Failed to get extradata buff info\n");
		return -EINVAL;
	}

	buff_req_buffer->buffer_size =
		msm_vidc_calculate_enc_input_extra_size(inst);
	inst->bufq[OUTPUT_PORT].plane_sizes[1] =
		buff_req_buffer->buffer_size;

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
	struct hfi_vpe_color_space_conversion vpe_csc;
	struct msm_vidc_platform_resources *resources;
	u32 *bias_coeff = NULL;
	u32 *csc_limit = NULL;
	u32 *csc_matrix = NULL;
	struct hfi_device *hdev;

	hdev = inst->core->device;
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

	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VPE_COLOR_SPACE_CONVERSION,
		&vpe_csc, sizeof(vpe_csc));
	if (rc)
		dprintk(VIDC_ERR,
			"%s: set property failed\n", __func__);
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
		 * and it is not dependent on frame size
		 */
		inst->bufq[fmt->type].plane_sizes[0] =
			msm_vidc_calculate_enc_output_frame_size(inst);

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
		 * update extradata buffer size as it may change due to
		 * frame size change.
		 */
		inst->bufq[fmt->type].plane_sizes[0] =
			msm_vidc_calculate_enc_input_frame_size(inst);
		inst->bufq[fmt->type].plane_sizes[1] =
			msm_vidc_calculate_enc_input_extra_size(inst);
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

static int msm_venc_resolve_rc_enable(struct msm_vidc_inst *inst,
		struct v4l2_ctrl *ctrl)
{
	struct v4l2_ctrl *rc_mode;

	if (!ctrl->val) {
		dprintk(VIDC_DBG,
			"RC is not enabled. Setting RC OFF\n");
		inst->rc_type = RATE_CONTROL_OFF;
	} else {
		rc_mode = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
		inst->rc_type = rc_mode->val;
	}
	return 0;
}

static int msm_venc_resolve_rate_control(struct msm_vidc_inst *inst,
		struct v4l2_ctrl *ctrl)
{
	struct v4l2_ctrl *rc_enable;

	rc_enable = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE);
	if (!rc_enable->val) {
		dprintk(VIDC_ERR,
			"RC is not enabled.\n");
		return -EINVAL;
	}

	if ((ctrl->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ) &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC) {
		dprintk(VIDC_ERR, "CQ supported only for HEVC\n");
		return -EINVAL;
	}
	inst->rc_type = ctrl->val;

	return 0;
}

int msm_venc_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_mastering_display_colour_sei_payload *mdisp_sei = NULL;
	struct msm_vidc_content_light_level_sei_payload *cll_sei = NULL;
	struct hal_buffer_requirements *buff_req_buffer = NULL;
	u32 i_qp_min, i_qp_max, p_qp_min, p_qp_max, b_qp_min, b_qp_max;

	if (!inst || !inst->core || !inst->core->device || !ctrl) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	mdisp_sei = &(inst->hdr10_sei_params.disp_color_sei);
	cll_sei = &(inst->hdr10_sei_params.cll_sei);

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
		rc = msm_venc_resolve_rate_control(inst, ctrl);
		if (rc)
			dprintk(VIDC_ERR,
				"%s: set bitrate mode failed\n", __func__);
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
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE:
		inst->clk_data.frame_rate = ctrl->val;
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_frame_rate(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set frame rate failed\n",
					__func__);
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
	case V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE:
		inst->clk_data.operating_rate = ctrl->val;
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_operating_rate(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set operating rate failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
		inst->clk_data.low_latency_mode = !!ctrl->val;
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
		if (ctrl->val == EXTRADATA_NONE)
			inst->prop.extradata_ctrls = 0;
		else
			inst->prop.extradata_ctrls |= ctrl->val;

		if ((inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_ROI) ||
		(inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_HDR10PLUS)) {
			buff_req_buffer = get_buff_req_buffer(inst,
						HAL_BUFFER_EXTRADATA_INPUT);
			if (!buff_req_buffer) {
				dprintk(VIDC_ERR,
				"Failed to get extradata buff info\n");
				rc = -EINVAL;
				break;
			}

			buff_req_buffer->buffer_size =
				msm_vidc_calculate_enc_input_extra_size(inst);
			inst->bufq[OUTPUT_PORT].plane_sizes[1] =
					buff_req_buffer->buffer_size;
		}

		if (inst->prop.extradata_ctrls & EXTRADATA_ADVANCED) {
			inst->bufq[CAPTURE_PORT].num_planes = 2;

			buff_req_buffer = get_buff_req_buffer(inst,
						HAL_BUFFER_EXTRADATA_OUTPUT);
			if (!buff_req_buffer) {
				dprintk(VIDC_ERR,
				"Failed to get extradata buff info\n");
				rc = -EINVAL;
				break;
			}

			buff_req_buffer->buffer_size =
				msm_vidc_calculate_enc_output_extra_size(inst);
			inst->bufq[CAPTURE_PORT].plane_sizes[1] =
					buff_req_buffer->buffer_size;
		}

		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		rc = msm_venc_resolve_rc_enable(inst, ctrl);
		if (rc)
			dprintk(VIDC_ERR,
				"%s: set rc enable failed.\n", __func__);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
		inst->profile = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		inst->level = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
		inst->level |=
			(msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val) << 28);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
		i_qp_min = inst->capability.cap[CAP_I_FRAME_QP].min;
		i_qp_max = inst->capability.cap[CAP_I_FRAME_QP].max;
		p_qp_min = inst->capability.cap[CAP_P_FRAME_QP].min;
		p_qp_max = inst->capability.cap[CAP_P_FRAME_QP].max;
		b_qp_min = inst->capability.cap[CAP_B_FRAME_QP].min;
		b_qp_max = inst->capability.cap[CAP_B_FRAME_QP].max;
		if ((ctrl->val & 0xff) < i_qp_min ||
			((ctrl->val >> 8) & 0xff) < p_qp_min ||
			((ctrl->val >> 16) & 0xff) < b_qp_min ||
			(ctrl->val & 0xff) > i_qp_max ||
			((ctrl->val >> 8) & 0xff) > p_qp_max ||
			((ctrl->val >> 16) & 0xff) > b_qp_max) {
			dprintk(VIDC_ERR, "Invalid QP %#x\n", ctrl->val);
			return -EINVAL;
		}
		if (ctrl->id == V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP)
			inst->client_set_ctrls |= CLIENT_SET_MIN_QP;
		else
			inst->client_set_ctrls |= CLIENT_SET_MAX_QP;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP:
		i_qp_min = inst->capability.cap[CAP_I_FRAME_QP].min;
		i_qp_max = inst->capability.cap[CAP_I_FRAME_QP].max;
		if (ctrl->val < i_qp_min || ctrl->val > i_qp_max) {
			dprintk(VIDC_ERR, "Invalid I QP %#x\n", ctrl->val);
			return -EINVAL;
		}
		inst->client_set_ctrls |= CLIENT_SET_I_QP;
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_dyn_qp(inst, ctrl);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: setting dyn frame QP failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP:
		p_qp_min = inst->capability.cap[CAP_P_FRAME_QP].min;
		p_qp_max = inst->capability.cap[CAP_P_FRAME_QP].max;
		if (ctrl->val < p_qp_min || ctrl->val > p_qp_max) {
			dprintk(VIDC_ERR, "Invalid P QP %#x\n", ctrl->val);
			return -EINVAL;
		}
		inst->client_set_ctrls |= CLIENT_SET_P_QP;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP:
		b_qp_min = inst->capability.cap[CAP_B_FRAME_QP].min;
		b_qp_max = inst->capability.cap[CAP_B_FRAME_QP].max;
		if (ctrl->val < b_qp_min || ctrl->val > b_qp_max) {
			dprintk(VIDC_ERR, "Invalid B QP %#x\n", ctrl->val);
			return -EINVAL;
		}
		inst->client_set_ctrls |= CLIENT_SET_B_QP;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_hp_layer(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set dyn hp layer failed.\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_base_layer_priority_id(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: set baselayer id failed.\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_BR:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_layer_bitrate(inst);
			if (rc)
				dprintk(VIDC_ERR,
				"%s: set layer bitrate failed\n",
				__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
		if (inst->state == MSM_VIDC_START_DONE) {
			dprintk(VIDC_ERR,
			"%s: Dynamic setting of Bframe is not supported\n",
			__func__);
			return -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE:
	case V4L2_CID_ROTATE:
	case V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT:
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:
	case V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER:
	case V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_QP:
	case V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE:
	case V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE:
	case V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS:
	case V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX:
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
	case V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO:
	case V4L2_CID_MPEG_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT:
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS:
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM:
	case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB:
	case V4L2_CID_MPEG_VIDC_VENC_CVP_DISABLE:
	case V4L2_CID_MPEG_VIDC_VENC_NATIVE_RECORDER:
	case V4L2_CID_MPEG_VIDC_VENC_RC_TIMESTAMP_DISABLE:
		dprintk(VIDC_DBG, "Control set: ID : %x Val : %d\n",
			ctrl->id, ctrl->val);
		break;
	default:
		dprintk(VIDC_ERR, "Unsupported index: %x\n", ctrl->id);
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

int msm_venc_set_frame_size(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_frame_size frame_sz;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	frame_sz.buffer_type = HFI_BUFFER_INPUT;
	frame_sz.width = inst->prop.width[OUTPUT_PORT];
	frame_sz.height = inst->prop.height[OUTPUT_PORT];
	dprintk(VIDC_DBG, "%s: input %d %d\n", __func__,
			frame_sz.width, frame_sz.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_sz, sizeof(frame_sz));
	if (rc) {
		dprintk(VIDC_ERR, "%s: failed to set input frame size %d %d\n",
			__func__, frame_sz.width, frame_sz.height);
		return rc;
	}

	frame_sz.buffer_type = HFI_BUFFER_OUTPUT;
	frame_sz.width = inst->prop.width[CAPTURE_PORT];
	frame_sz.height = inst->prop.height[CAPTURE_PORT];
	dprintk(VIDC_DBG, "%s: output %d %d\n", __func__,
			frame_sz.width, frame_sz.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_sz, sizeof(frame_sz));
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed to set output frame size %d %d\n",
			__func__, frame_sz.width, frame_sz.height);
		return rc;
	}

	return rc;
}

int msm_venc_set_frame_rate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_frame_rate frame_rate;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	frame_rate.buffer_type = HFI_BUFFER_OUTPUT;
	frame_rate.frame_rate = inst->clk_data.frame_rate;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, frame_rate.frame_rate);

	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HFI_PROPERTY_CONFIG_FRAME_RATE,
		&frame_rate, sizeof(frame_rate));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

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
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SECURE);
	enable.enable = !!ctrl->val;

	if (enable.enable) {
		if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264 ||
			inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC)) {
			dprintk(VIDC_ERR,
				"%s: Secure mode only allowed for HEVC/H264\n",
				__func__);
			return -EINVAL;
		}
	}

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_SECURE_SESSION, &enable, sizeof(enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_priority(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY);
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_REALTIME, &enable, sizeof(enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_operating_rate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_operating_rate op_rate;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE);
	op_rate.operating_rate = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, op_rate.operating_rate >> 16);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_OPERATING_RATE, &op_rate, sizeof(op_rate));
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
	struct hfi_profile_level profile_level;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!inst->profile) {
		dprintk(VIDC_ERR,
			"%s: skip as client did not set profile\n",
			__func__);
		return -EINVAL;
	}
	profile_level.profile = inst->profile;
	profile_level.level = inst->level;

	dprintk(VIDC_DBG, "%s: %#x %#x\n", __func__,
		profile_level.profile, profile_level.level);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_PROFILE_LEVEL_CURRENT, &profile_level,
		sizeof(profile_level));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_idr_period(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_idr_period idr_period;

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
		HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD, &idr_period,
		sizeof(idr_period));
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}

	return rc;
}

void msm_venc_decide_bframe(struct msm_vidc_inst *inst)
{
	u32 width = inst->prop.width[OUTPUT_PORT];
	u32 height = inst->prop.height[OUTPUT_PORT];
	u32 num_mbs_per_frame, num_mbs_per_sec;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *bframe_ctrl;
	struct msm_vidc_platform_resources *res;

	res = &inst->core->resources;
	bframe_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	num_mbs_per_frame = NUM_MBS_PER_FRAME(width, height);
	if (num_mbs_per_frame > res->max_bframe_mbs_per_frame)
		goto disable_bframe;

	num_mbs_per_sec = num_mbs_per_frame *
		(inst->clk_data.frame_rate >> 16);
	if (num_mbs_per_sec > res->max_bframe_mbs_per_sec)
		goto disable_bframe;

	ctrl = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	if (ctrl->val > 1)
		goto disable_bframe;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (ctrl->val)
		goto disable_bframe;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
		goto disable_bframe;

	if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264) {
		ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_PROFILE);
		if ((ctrl->val != V4L2_MPEG_VIDEO_H264_PROFILE_MAIN) &&
			(ctrl->val != V4L2_MPEG_VIDEO_H264_PROFILE_HIGH))
			goto disable_bframe;
	} else if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		goto disable_bframe;

	if (inst->clk_data.low_latency_mode)
		goto disable_bframe;

	if (!bframe_ctrl->val) {
		ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_NATIVE_RECORDER);
		if (ctrl->val) {
			/*
			 * Native recorder is enabled and bframe is not enabled
			 * Hence, forcefully enable bframe
			 */
			inst->prop.bframe_changed = true;
			bframe_ctrl->val = MAX_NUM_B_FRAMES;
			dprintk(VIDC_DBG, "Bframe is forcefully enabled\n");
		} else {
			/*
			 * Native recorder is not enabled
			 * B-Frame is not enabled by client
			 */
			goto disable_bframe;
		}
	}
	dprintk(VIDC_DBG, "Bframe can be enabled!\n");

	return;
disable_bframe:
	if (bframe_ctrl->val) {
		/*
		 * Client wanted to enable bframe but,
		 * conditions to enable are not met
		 * Hence, forcefully disable bframe
		 */
		inst->prop.bframe_changed = true;
		bframe_ctrl->val = 0;
		dprintk(VIDC_DBG, "Bframe is forcefully disabled!\n");
	} else {
		dprintk(VIDC_DBG, "Bframe is disabled\n");
	}
}

int msm_venc_set_adaptive_bframes(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	enable.enable = true;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_ADAPTIVE_B, &enable, sizeof(enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

void msm_venc_adjust_gop_size(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *hier_ctrl;
	struct v4l2_ctrl *bframe_ctrl;
	struct v4l2_ctrl *gop_size_ctrl;

	gop_size_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_GOP_SIZE);
	if (inst->prop.bframe_changed) {
		/*
		 * BFrame size was explicitly change
		 * Hence, adjust GOP size accordingly
		 */
		bframe_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
		if (!bframe_ctrl->val)
			/* Forcefully disabled */
			gop_size_ctrl->val = gop_size_ctrl->val *
					(1 + MAX_NUM_B_FRAMES);
		else
			/* Forcefully enabled */
			gop_size_ctrl->val = gop_size_ctrl->val /
					(1 + MAX_NUM_B_FRAMES);
	}

	/*
	 * Layer encoding needs GOP size to be multiple of subgop size
	 * And subgop size is 2 ^ number of enhancement layers
	 */
	hier_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);
	if (hier_ctrl->val > 1) {
		u32 min_gop_size;
		u32 num_subgops;

		min_gop_size = (1 << (hier_ctrl->val - 1));
		num_subgops = (gop_size_ctrl->val + (min_gop_size >> 1)) /
				min_gop_size;
		if (num_subgops)
			gop_size_ctrl->val = num_subgops * min_gop_size;
		else
			gop_size_ctrl->val = min_gop_size;
	}
}

int msm_venc_set_intra_period(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_intra_period intra_period;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	msm_venc_adjust_gop_size(inst);

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_GOP_SIZE);
	intra_period.pframes = ctrl->val;

	/*
	 * At this point we have already made decision on bframe
	 * Control value gives updated bframe value.
	 */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	intra_period.bframes = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__, intra_period.pframes,
		intra_period.bframes);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD, &intra_period,
		sizeof(intra_period));
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}

	if (intra_period.bframes) {
		/* Enable adaptive bframes as nbframes!= 0 */
		rc = msm_venc_set_adaptive_bframes(inst);
		if (rc) {
			dprintk(VIDC_ERR, "%s: set property failed\n",
				__func__);
			return rc;
		}
	}
	return rc;
}

int msm_venc_set_request_keyframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	dprintk(VIDC_DBG, "%s\n", __func__);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_REQUEST_SYNC_FRAME, NULL, 0);
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}

	return rc;
}

int msm_venc_set_rate_control(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	u32 hfi_rc, codec;
	u32 height, width, mbpf;
	struct hfi_vbv_hrd_buf_size hrd_buf_size;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	inst->clk_data.is_cbr_plus = false;
	codec = inst->fmts[CAPTURE_PORT].fourcc;
	height = inst->prop.height[OUTPUT_PORT];
	width = inst->prop.width[OUTPUT_PORT];
	mbpf = NUM_MBS_PER_FRAME(height, width);

	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_MBR_VFR)
		inst->rc_type = V4L2_MPEG_VIDEO_BITRATE_MODE_MBR;
	else if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR &&
			   inst->clk_data.low_latency_mode)
		inst->rc_type = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;

	if ((inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR ||
		inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR) &&
		(codec != V4L2_PIX_FMT_VP8)) {
		hrd_buf_size.vbv_hrd_buf_size = 500;
		inst->clk_data.low_latency_mode = true;

		if ((width > MIN_CBRPLUS_W && height > MIN_CBRPLUS_H) ||
			(width > MIN_CBRPLUS_H && height > MIN_CBRPLUS_W) ||
			mbpf > NUM_MBS_PER_FRAME(720, 1280)) {
			hrd_buf_size.vbv_hrd_buf_size = 1000;
			inst->clk_data.is_cbr_plus = true;
		}

		dprintk(VIDC_DBG, "Set hrd_buf_size %d",
				hrd_buf_size.vbv_hrd_buf_size);

		rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session,
			HFI_PROPERTY_CONFIG_VENC_VBV_HRD_BUF_SIZE,
			(void *)&hrd_buf_size, sizeof(hrd_buf_size));
		if (rc) {
			dprintk(VIDC_ERR, "%s: set HRD_BUF_SIZE %u failed\n",
					__func__,
					hrd_buf_size.vbv_hrd_buf_size);
			inst->clk_data.is_cbr_plus = false;
		}
	}

	switch (inst->rc_type) {
	case RATE_CONTROL_OFF:
		hfi_rc = HFI_RATE_CONTROL_OFF;
		break;
	case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR:
		hfi_rc = HFI_RATE_CONTROL_CBR_CFR;
		break;
	case V4L2_MPEG_VIDEO_BITRATE_MODE_VBR:
		hfi_rc = HFI_RATE_CONTROL_VBR_CFR;
		break;
	case V4L2_MPEG_VIDEO_BITRATE_MODE_MBR:
		hfi_rc = HFI_RATE_CONTROL_MBR_CFR;
		break;
	case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR:
		hfi_rc = HFI_RATE_CONTROL_CBR_VFR;
		break;
	case V4L2_MPEG_VIDEO_BITRATE_MODE_CQ:
		hfi_rc = HFI_RATE_CONTROL_CQ;
		break;
	default:
		hfi_rc = HFI_RATE_CONTROL_OFF;
		dprintk(VIDC_ERR,
			"Invalid Rate control setting: %d Default RCOFF\n",
			inst->rc_type);
		break;
	}
	dprintk(VIDC_DBG, "%s: %d\n", __func__, inst->rc_type);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_RATE_CONTROL, &hfi_rc,
		sizeof(u32));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_input_timestamp_rc(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_RC_TIMESTAMP_DISABLE);
	/*
	 * HFI values:
	 * 0 - time delta is calculated based on buffer timestamp
	 * 1 - ignores buffer timestamp and fw derives time delta based
	 *     on input frame rate.
	 */
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_DISABLE_RC_TIMESTAMP, &enable,
		sizeof(enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_bitrate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_bitrate bitrate;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->layer_bitrate) {
		dprintk(VIDC_DBG, "%s: Layer bitrate is enabled\n", __func__);
		return 0;
	}

	enable.enable = 0;
	dprintk(VIDC_DBG, "%s: bitrate type: %d\n",
		__func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_BITRATE_TYPE, &enable,
		sizeof(enable));
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		return rc;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE);
	bitrate.bit_rate = ctrl->val;
	bitrate.layer_id = MSM_VIDC_ALL_LAYER_ID;
	dprintk(VIDC_DBG, "%s: %d\n", __func__, bitrate.bit_rate);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE, &bitrate,
		sizeof(bitrate));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_layer_bitrate(struct msm_vidc_inst *inst)
{
	int rc = 0, i = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *bitrate = NULL;
	struct v4l2_ctrl *layer = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	struct v4l2_ctrl *layer_br_ratios[MAX_HIER_CODING_LAYER] = {NULL};
	struct hfi_bitrate layer_br;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);

	if (!max_layer->val || !layer->val) {
		dprintk(VIDC_DBG,
			"%s: Hierp layer not set. Ignore layer bitrate\n",
			__func__);
		goto error;
	}

	if (max_layer->val < layer->val) {
		dprintk(VIDC_DBG,
			"%s: Hierp layer greater than max isn't allowed\n",
			__func__);
		goto error;
	}

	layer_br_ratios[0] = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_BR);
	layer_br_ratios[1] = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_BR);
	layer_br_ratios[2] = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_BR);
	layer_br_ratios[3] = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_BR);
	layer_br_ratios[4] = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_BR);
	layer_br_ratios[5] = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_BR);

	/* Set layer bitrates only when highest layer br ratio is 100. */
	if (layer_br_ratios[layer->val-1]->val != MAX_BIT_RATE_RATIO ||
		layer_br_ratios[0]->val == 0) {
		dprintk(VIDC_DBG,
			"%s: Improper layer bitrate ratio\n",
			__func__);
		goto error;
	}

	for (i = layer->val - 1; i > 0; --i) {
		if (layer_br_ratios[i]->val == 0) {
			dprintk(VIDC_DBG,
				"%s: Layer ratio must be non-zero\n",
				__func__);
			goto error;
		}
		layer_br_ratios[i]->val -= layer_br_ratios[i-1]->val;
	}

	enable.enable = 1;
	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_BITRATE_TYPE, &enable,
		sizeof(enable));
	if (rc) {
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);
		goto error;
	}

	bitrate = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE);
	for (i = 0; i < layer->val; ++i) {
		layer_br.bit_rate =
			bitrate->val * layer_br_ratios[i]->val / 100;
		layer_br.layer_id = i;
		dprintk(VIDC_DBG,
			"%s: Bitrate for Layer[%u]: [%u]\n",
			__func__, layer_br.layer_id, layer_br.bit_rate);

		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE, &layer_br,
			sizeof(layer_br));
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set property failed for layer: %u\n",
				__func__, layer_br.layer_id);
			goto error;
		}
	}

	inst->layer_bitrate = true;
	return rc;

error:
	inst->layer_bitrate = false;
	return rc;
}

int msm_venc_set_frame_qp(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *i_qp = NULL;
	struct v4l2_ctrl *p_qp = NULL;
	struct v4l2_ctrl *b_qp = NULL;
	struct v4l2_ctrl *rc_enable = NULL;
	struct hfi_quantization qp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	qp.layer_id = MSM_VIDC_ALL_LAYER_ID;
	qp.enable = 0;
	qp.enable = QP_ENABLE_I | QP_ENABLE_P | QP_ENABLE_B;

	i_qp = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP);
	p_qp = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP);
	b_qp = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP);
	rc_enable = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE);

	/*
	 * When RC is ON:
	 *   Enable QP types which have been set by client.
	 * When RC is OFF:
	 *   I_QP value must be set by client.
	 *   If other QP value is invalid, then, assign I_QP value to it.
	 */
	if (rc_enable->val) {
		if (!(inst->client_set_ctrls & CLIENT_SET_I_QP))
			qp.enable &= ~QP_ENABLE_I;
		if (!(inst->client_set_ctrls & CLIENT_SET_P_QP))
			qp.enable &= ~QP_ENABLE_P;
		if (!(inst->client_set_ctrls & CLIENT_SET_B_QP))
			qp.enable &= ~QP_ENABLE_B;

		if (!qp.enable)
			return 0;
	} else {
		if (!(inst->client_set_ctrls & CLIENT_SET_I_QP)) {
			dprintk(VIDC_WARN,
				"%s: Client value is not valid\n", __func__);
			return -EINVAL;
		}
		if (!(inst->client_set_ctrls & CLIENT_SET_P_QP))
			p_qp->val = i_qp->val;
		if (!(inst->client_set_ctrls & CLIENT_SET_B_QP))
			b_qp->val = i_qp->val;
	}

	/* B frame QP is not supported for VP8. */
	if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_VP8)
		qp.enable &= ~QP_ENABLE_B;

	qp.qp_packed = i_qp->val | p_qp->val << 8 | b_qp->val << 16;

	dprintk(VIDC_DBG, "%s: layers %#x frames %#x qp_packed %#x\n",
		__func__, qp.layer_id, qp.enable, qp.qp_packed);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_FRAME_QP, &qp, sizeof(qp));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_qp_range(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_quantization_range qp_range;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->client_set_ctrls & CLIENT_SET_MIN_QP) &&
		!(inst->client_set_ctrls & CLIENT_SET_MAX_QP)) {
		dprintk(VIDC_DBG,
			"%s: Client didn't set QP range.\n", __func__);
		return 0;
	}

	qp_range.min_qp.layer_id = MSM_VIDC_ALL_LAYER_ID;
	qp_range.max_qp.layer_id = MSM_VIDC_ALL_LAYER_ID;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP);
	qp_range.min_qp.qp_packed = ctrl->val;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP);
	qp_range.max_qp.qp_packed = ctrl->val;

	dprintk(VIDC_DBG,
			"%s: layers %#x qp_min %#x qp_max %#x\n",
			__func__, qp_range.min_qp.layer_id,
			qp_range.min_qp.qp_packed, qp_range.max_qp.qp_packed);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_SESSION_QP_RANGE, &qp_range,
		sizeof(qp_range));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_frame_quality(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_heic_frame_quality frame_quality;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY);
	frame_quality.frame_quality = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, frame_quality.frame_quality);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_HEIC_FRAME_QUALITY, &frame_quality,
		sizeof(frame_quality));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_grid(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_heic_grid_enable grid_enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE);

	/* Need a change in HFI if we want to pass size */
	if (!ctrl->val)
		grid_enable.grid_enable = false;
	else
		grid_enable.grid_enable = true;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, grid_enable.grid_enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_HEIC_GRID_ENABLE, &grid_enable,
		sizeof(grid_enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_entropy_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_h264_entropy_control entropy;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE);
	entropy.entropy_mode = msm_comm_v4l2_to_hfi(
			V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
			ctrl->val);
	entropy.cabac_model = HFI_H264_CABAC_MODEL_2;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, entropy.entropy_mode);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_H264_ENTROPY_CONTROL, &entropy,
		sizeof(entropy));
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
	struct hfi_multi_slice_control multi_slice_control;
	int temp = 0;
	u32 mb_per_frame, fps, mbps, bitrate, max_slices;
	u32 slice_val, slice_mode, max_avg_slicesize;
	u32 rc_mode, output_width, output_height;
	struct v4l2_ctrl *rc_enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	slice_mode = HFI_MULTI_SLICE_OFF;
	slice_val = 0;

	bitrate = inst->clk_data.bitrate;
	fps = inst->clk_data.frame_rate;
	rc_mode = inst->rc_type;
	rc_enable = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE);
	if (fps > 60 ||
		(rc_enable->val &&
		 rc_mode != V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR &&
		 rc_mode != V4L2_MPEG_VIDEO_BITRATE_MODE_CBR)) {
		goto set_and_exit;
	}

	output_width = inst->prop.width[OUTPUT_PORT];
	output_height = inst->prop.height[OUTPUT_PORT];

	if (output_height < 128 ||
		(inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC &&
		 output_width < 384) ||
		(inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		 output_width < 192)) {
		goto set_and_exit;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
	if (ctrl->val == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) {
		temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
		slice_mode = HFI_MULTI_SLICE_BY_MB_COUNT;
	} else if (ctrl->val == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) {
		temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES;
		slice_mode = HFI_MULTI_SLICE_BY_BYTE_COUNT;
	} else {
		goto set_and_exit;
	}

	ctrl_t = get_ctrl(inst, temp);
	slice_val = ctrl_t->val;

	/* Update Slice Config */
	mb_per_frame = NUM_MBS_PER_FRAME(output_height, output_width);
	mbps = NUM_MBS_PER_SEC(output_height, output_width, fps);

	if (slice_mode == HFI_MULTI_SLICE_BY_MB_COUNT) {
		if (output_width <= 4096 || output_height <= 4096 ||
			mb_per_frame <= NUM_MBS_PER_FRAME(4096, 2160) ||
			mbps <= NUM_MBS_PER_SEC(4096, 2160, 60)) {
			max_slices = inst->capability.cap[CAP_SLICE_MB].max ?
				inst->capability.cap[CAP_SLICE_MB].max : 1;
			slice_val = max(slice_val, mb_per_frame / max_slices);
		}
	} else {
		if (output_width <= 1920 || output_height <= 1920 ||
			mb_per_frame <= NUM_MBS_PER_FRAME(1088, 1920) ||
			mbps <= NUM_MBS_PER_SEC(1088, 1920, 60)) {
			max_slices = inst->capability.cap[CAP_SLICE_BYTE].max ?
				inst->capability.cap[CAP_SLICE_BYTE].max : 1;
			max_avg_slicesize = ((bitrate / fps) / 8) / max_slices;
			slice_val = max(slice_val, max_avg_slicesize);
		}
	}

	if (slice_mode == HFI_MULTI_SLICE_OFF) {
		ctrl->val = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE;
		ctrl_t->val = 0;
	}

set_and_exit:
	multi_slice_control.multi_slice = slice_mode;
	multi_slice_control.slice_size = slice_val;

	hdev = inst->core->device;
	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
			multi_slice_control.multi_slice,
			multi_slice_control.slice_size);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_CONTROL,
		&multi_slice_control, sizeof(multi_slice_control));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_intra_refresh_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct v4l2_ctrl *rc_mode = NULL;
	struct hfi_intra_refresh intra_refresh;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc_mode = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
	if (!(rc_mode->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR ||
		rc_mode->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR))
		return 0;

	/* Firmware supports only random mode */
	intra_refresh.mode = HFI_INTRA_REFRESH_RANDOM;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM);
	intra_refresh.mbs = 0;
	if (ctrl->val) {
		u32 num_mbs_per_frame = 0;
		u32 width = inst->prop.width[CAPTURE_PORT];
		u32 height = inst->prop.height[CAPTURE_PORT];

		num_mbs_per_frame = NUM_MBS_PER_FRAME(height, width);
		intra_refresh.mbs = num_mbs_per_frame / ctrl->val;
		if (num_mbs_per_frame % ctrl->val) {
			intra_refresh.mbs++;
		}
	} else {
		ctrl = get_ctrl(inst,
			V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB);
		intra_refresh.mbs = ctrl->val;
	}
	if (!intra_refresh.mbs) {
		intra_refresh.mode = HFI_INTRA_REFRESH_NONE;
		intra_refresh.mbs = 0;
	}

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
			intra_refresh.mode, intra_refresh.mbs);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH, &intra_refresh,
		sizeof(intra_refresh));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_bitrate_savings_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_BITRATE_SAVINGS);
	enable.enable = !!ctrl->val;
	if (!ctrl->val && inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) {
		dprintk(VIDC_DBG,
			"Can't disable bitrate savings for non-VBR_CFR\n");
		enable.enable = 1;
	}

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_BITRATE_SAVINGS, &enable,
		sizeof(enable));
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
	struct hfi_h264_db_control h264_db_control;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE);
	ctrl_a = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA);
	ctrl_b = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA);
	h264_db_control.mode = msm_comm_v4l2_to_hfi(
			V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
			ctrl->val);
	h264_db_control.slice_alpha_offset = ctrl_a->val;
	h264_db_control.slice_beta_offset = ctrl_b->val;

	dprintk(VIDC_DBG, "%s: %d %d %d\n", __func__,
		h264_db_control.mode, h264_db_control.slice_alpha_offset,
		h264_db_control.slice_beta_offset);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_H264_DEBLOCK_CONTROL, &h264_db_control,
		sizeof(h264_db_control));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_sequence_header_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264 ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR);
	if (ctrl->val)
		enable.enable = true;
	else
		enable.enable = false;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_SYNC_FRAME_SEQUENCE_HEADER, &enable,
		sizeof(enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_au_delimiter_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264 ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER);
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_GENERATE_AUDNAL, &enable,
		sizeof(enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_enable_hybrid_hp(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl = NULL;
	struct v4l2_ctrl *layer = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264)
		return 0;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (ctrl->val)
		return 0;

	ctrl = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	layer = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);
	if (ctrl->val != layer->val)
		return 0;

	/*
	 * Hybrid HP is enabled only for H264 when
	 * LTR and B-frame are both disabled,
	 * Layer encoding has higher priority over B-frame
	 * Hence, no need to check for B-frame
	 * Rate control type is VBR and
	 * Max layer equals layer count.
	 */

	inst->hybrid_hp = true;

	return 0;
}

int msm_venc_set_base_layer_priority_id(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	u32 baselayerid;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	if (max_layer->val <= 0) {
		dprintk(VIDC_DBG, "%s: Layer id can only be set with Hierp\n",
			__func__);
		return 0;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID);
	baselayerid = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, baselayerid);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_BASELAYER_PRIORITYID, &baselayerid,
		sizeof(baselayerid));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_hp_max_layer(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	u32 hp_layer = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);

	rc = msm_venc_enable_hybrid_hp(inst);
	if (rc) {
		dprintk(VIDC_ERR, "%s: get hybrid hp decision failed\n",
			__func__);
		return rc;
	}

	/*
	 * We send enhancement layer count to FW,
	 * hence, input 0/1 indicates absence of layer encoding.
	 */
	if (ctrl->val)
		hp_layer = ctrl->val - 1;

	if (inst->hybrid_hp) {
		dprintk(VIDC_DBG, "%s: Hybrid hierp layer: %d\n",
			__func__, hp_layer);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VENC_HIER_P_HYBRID_MODE,
			&hp_layer, sizeof(hp_layer));
	} else {
		dprintk(VIDC_DBG, "%s: Hierp max layer: %d\n",
			__func__, hp_layer);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VENC_HIER_P_MAX_NUM_ENH_LAYER,
			&hp_layer, sizeof(hp_layer));
	}
	if (rc)
		dprintk(VIDC_ERR,
			"%s: set property failed\n", __func__);
	return rc;
}

int msm_venc_set_hp_layer(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	u32 hp_layer = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	if (inst->hybrid_hp) {
		dprintk(VIDC_WARN,
			"%s: Setting layer isn't allowed with hybrid hp\n",
			__func__);
		return 0;
	}

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	ctrl = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);

	if (max_layer->val < ctrl->val) {
		dprintk(VIDC_WARN,
			"%s: HP layer count greater than max isn't allowed\n",
			__func__);
		return 0;
	}

	/*
	 * We send enhancement layer count to FW,
	 * hence, input 0/1 indicates absence of layer encoding.
	 */
	if (ctrl->val)
		hp_layer = ctrl->val - 1;

	dprintk(VIDC_DBG, "%s: Hierp enhancement layer: %d\n",
		__func__, hp_layer);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_HIER_P_ENH_LAYER,
		&hp_layer, sizeof(hp_layer));
	if (rc)
		dprintk(VIDC_ERR,
			"%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_vpx_error_resilience(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_VP8)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE);
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE, &enable,
		sizeof(enable));
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
	struct hfi_video_signal_metadata signal_info;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264 ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl_cs = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
	ctrl_fr = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE);
	ctrl_tr = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS);
	ctrl_mc = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS);
	if (ctrl_cs->val == MSM_VIDC_RESERVED_1)
		return 0;

	signal_info.enable = true;
	signal_info.video_format = MSM_VIDC_NTSC;
	signal_info.video_full_range = ctrl_fr->val;
	signal_info.color_description = 1;
	signal_info.color_primaries = ctrl_cs->val;
	signal_info.transfer_characteristics = ctrl_tr->val;
	signal_info.matrix_coeffs = ctrl_mc->val;

	dprintk(VIDC_DBG, "%s: %d %d %d %d\n", __func__,
		signal_info.color_primaries, signal_info.video_full_range,
		signal_info.transfer_characteristics,
		signal_info.matrix_coeffs);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_VIDEO_SIGNAL_INFO, &signal_info,
		sizeof(signal_info));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_rotation(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *rotation = NULL;
	struct v4l2_ctrl *hflip = NULL;
	struct v4l2_ctrl *vflip = NULL;
	struct hfi_device *hdev;
	struct hfi_vpe_rotation_type vpe_rotation;
	struct hfi_frame_size frame_sz;

	hdev = inst->core->device;

	rotation = get_ctrl(inst, V4L2_CID_ROTATE);

	vpe_rotation.rotation = HFI_ROTATE_NONE;
	if (rotation->val == 90)
		vpe_rotation.rotation = HFI_ROTATE_90;
	else if (rotation->val == 180)
		vpe_rotation.rotation = HFI_ROTATE_180;
	else if (rotation->val ==  270)
		vpe_rotation.rotation = HFI_ROTATE_270;

	hflip = get_ctrl(inst, V4L2_CID_HFLIP);
	vflip = get_ctrl(inst, V4L2_CID_VFLIP);

	vpe_rotation.flip = HFI_FLIP_NONE;
	if ((hflip->val == V4L2_MPEG_MSM_VIDC_ENABLE) &&
		(vflip->val == V4L2_MPEG_MSM_VIDC_ENABLE))
		vpe_rotation.flip = HFI_FLIP_HORIZONTAL | HFI_FLIP_VERTICAL;
	else if (hflip->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		vpe_rotation.flip = HFI_FLIP_HORIZONTAL;
	else if (vflip->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		vpe_rotation.flip = HFI_FLIP_VERTICAL;

	dprintk(VIDC_DBG, "Set rotation = %d, flip = %d\n",
			vpe_rotation.rotation, vpe_rotation.flip);
	rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session,
				HFI_PROPERTY_PARAM_VPE_ROTATION,
				&vpe_rotation, sizeof(vpe_rotation));
	if (rc) {
		dprintk(VIDC_ERR, "Set rotation/flip failed\n");
		return rc;
	}

	/* flip the output resolution if required */
	if (vpe_rotation.rotation == HFI_ROTATE_90 ||
		vpe_rotation.rotation == HFI_ROTATE_270) {
		frame_sz.buffer_type = HFI_BUFFER_OUTPUT;
		frame_sz.width = inst->prop.height[CAPTURE_PORT];
		frame_sz.height = inst->prop.width[CAPTURE_PORT];
		dprintk(VIDC_DBG, "CAPTURE port width = %d, height = %d\n",
			frame_sz.width, frame_sz.height);
		rc = call_hfi_op(hdev, session_set_property, (void *)
			inst->session, HFI_PROPERTY_PARAM_FRAME_SIZE,
			&frame_sz, sizeof(frame_sz));
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set framesize\n");
			return rc;
		}
	}
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

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC);
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_DISABLE)
		return 0;

	ctrl_cs = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
	ctrl_cm = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX);

	color_primaries = ctrl_cs->val;
	custom_matrix = ctrl_cm->val;
	rc = msm_venc_set_csc(inst, color_primaries, custom_matrix);
	if (rc)
		dprintk(VIDC_ERR, "%s: msm_venc_set_csc failed\n", __func__);

	return rc;
}

int msm_venc_set_8x8_transform(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264) {
		dprintk(VIDC_DBG, "%s: skip due to %#x\n",
			__func__, inst->fmts[CAPTURE_PORT].fourcc);
		return 0;
	}

	if (inst->profile != HFI_H264_PROFILE_HIGH &&
		inst->profile != HFI_H264_PROFILE_CONSTRAINED_HIGH) {
		dprintk(VIDC_DBG, "%s: skip due to %#x\n",
			__func__, inst->profile);
		return 0;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM);
	enable.enable = !!ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_H264_8X8_TRANSFORM, &enable,
		sizeof(enable));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_vui_timing_info(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_vui_timing_info timing_info;
	bool cfr;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO);
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_DISABLE)
		return 0;

	switch (inst->rc_type) {
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
		HFI_PROPERTY_PARAM_VENC_VUI_TIMING_INFO, &timing_info,
		sizeof(timing_info));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_nal_stream_format(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_nal_stream_format_select stream_format;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
		inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD);
	stream_format.nal_stream_format_select = BIT(ctrl->val);
	switch (ctrl->val) {
	case V4L2_MPEG_VIDEO_HEVC_SIZE_0:
		stream_format.nal_stream_format_select =
			HFI_NAL_FORMAT_STARTCODES;
		break;
	case V4L2_MPEG_VIDEO_HEVC_SIZE_4:
		stream_format.nal_stream_format_select =
			HFI_NAL_FORMAT_FOUR_BYTE_LENGTH;
		break;
	default:
		dprintk(VIDC_ERR,
			"%s: Invalid stream format setting. Setting default\n",
			__func__);
		stream_format.nal_stream_format_select =
			HFI_NAL_FORMAT_STARTCODES;
		break;
	}

	dprintk(VIDC_DBG, "%s: %#x\n", __func__,
			stream_format.nal_stream_format_select);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT, &stream_format,
		sizeof(stream_format));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_ltr_mode ltr;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (!ctrl->val)
		return 0;
	if (ctrl->val > inst->capability.cap[CAP_LTR_COUNT].max) {
		dprintk(VIDC_ERR, "%s: invalid ltr count %d, max %d\n",
			__func__, ctrl->val,
			inst->capability.cap[CAP_LTR_COUNT].max);
		return -EINVAL;
	}
	ltr.ltr_count =  ctrl->val;
	ltr.ltr_mode = HFI_LTR_MODE_MANUAL;
	ltr.trust_mode = 1;
	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
			ltr.ltr_mode, ltr.ltr_count);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_LTRMODE, &ltr, sizeof(ltr));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_useframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_ltr_use use_ltr;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME);
	use_ltr.ref_ltr = ctrl->val;
	use_ltr.use_constrnt = false;
	use_ltr.frames = 0;
	dprintk(VIDC_DBG, "%s: %d\n", __func__, use_ltr.ref_ltr);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_USELTRFRAME, &use_ltr,
		sizeof(use_ltr));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_markframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_ltr_mark mark_ltr;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME);
	mark_ltr.mark_frame = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d\n", __func__, mark_ltr.mark_frame);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_MARKLTRFRAME, &mark_ltr,
		sizeof(mark_ltr));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_dyn_qp(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_quantization qp;
	struct v4l2_ctrl *rc_enable;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc_enable = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE);
	if (rc_enable->val) {
		dprintk(VIDC_ERR, "%s: Dyn qp is set only when RC is OFF\n",
			__func__);
		return -EINVAL;
	}

	qp.qp_packed = ctrl->val | ctrl->val << 8 | ctrl->val << 16;
	qp.enable = QP_ENABLE_I | QP_ENABLE_P | QP_ENABLE_B;
	qp.layer_id = MSM_VIDC_ALL_LAYER_ID;

	/* B frame QP is not supported for VP8. */
	if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_VP8)
		qp.enable &= ~QP_ENABLE_B;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__,
		ctrl->val);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_FRAME_QP, &qp, sizeof(qp));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_aspect_ratio(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_aspect_ratio sar;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264 ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH);
	if (!ctrl->val)
		return 0;
	sar.aspect_width = ctrl->val;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT);
	if (!ctrl->val)
		return 0;
	sar.aspect_height = ctrl->val;

	dprintk(VIDC_DBG, "%s: %d %d\n", __func__,
		sar.aspect_width, sar.aspect_height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_ASPECT_RATIO, &sar, sizeof(sar));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_blur_resolution(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_frame_size frame_sz;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS);

	frame_sz.buffer_type = HFI_BUFFER_INPUT;
	frame_sz.height = ctrl->val & 0xFFFF;
	frame_sz.width = (ctrl->val & 0xFFFF0000) >> 16;
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_BLUR_FRAME_SIZE, &frame_sz,
		sizeof(frame_sz));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_hdr_info(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *profile = NULL;
	struct hfi_device *hdev;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC)
		return 0;

	profile = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE);
	if (profile->val != V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10)
		return 0;

	/* No conversion to HFI needed as both structures are same */
	dprintk(VIDC_DBG, "%s: setting hdr info\n", __func__);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_HDR10_PQ_SEI, &inst->hdr10_sei_params,
		sizeof(inst->hdr10_sei_params));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_extradata(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *cvp_ctrl;
	u32 value = 0x0;

	if (inst->prop.extradata_ctrls == EXTRADATA_NONE) {
		// Disable all Extradata
		msm_comm_set_index_extradata(inst,
			MSM_VIDC_EXTRADATA_ASPECT_RATIO, 0x0);
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_LTR_INFO, 0x0);
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_ROI_QP_EXTRADATA, 0x0);
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_HDR10PLUS_METADATA_EXTRADATA,
			0x0);
		}
	}

	if (inst->prop.extradata_ctrls & EXTRADATA_ADVANCED)
		// Enable Advanced Extradata - LTR Info
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_LTR_INFO, 0x1);

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_ROI)
		// Enable ROIQP Extradata
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_ROI_QP_EXTRADATA, 0x1);

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_HDR10PLUS) {
		// Enable HDR10+ Extradata
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_HDR10PLUS_METADATA_EXTRADATA,
			0x1);
		}
	}

	cvp_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_CVP_DISABLE);
	if (cvp_ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE) {
		if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_CVP) {
			dprintk(VIDC_ERR,
				"%s: invalid params\n", __func__);
			return -EINVAL;
		}
	} else {
		/*
		 * For now, enable CVP metadata only if client provides it.
		 * Once the kernel-mode CVP metadata implementation
		 * is completed, this condition should be removed.
		 */
		if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_CVP)
			value = 0x1;

	}
	rc = msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VENC_CVP_METADATA_EXTRADATA, value);

	return rc;
}

int msm_venc_set_properties(struct msm_vidc_inst *inst)
{
	int rc = 0;

	rc = msm_venc_set_frame_size(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_frame_rate(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_secure_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_priority(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_color_format(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_sequence_header_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_profile_level(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_8x8_transform(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_bitrate(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_entropy_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_rate_control(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_bitrate_savings_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_input_timestamp_rc(inst);
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
	rc = msm_venc_set_au_delimiter_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_vui_timing_info(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_hdr_info(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_vpx_error_resilience(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_nal_stream_format(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_slice_control_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_loop_filter_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_intra_refresh_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_ltr_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_hp_max_layer(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_hp_layer(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_base_layer_priority_id(inst);
	if (rc)
		goto exit;
	msm_venc_decide_bframe(inst);
	rc = msm_venc_set_idr_period(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_intra_period(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_aspect_ratio(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_video_signal_info(inst);
	if (rc)
		goto exit;
	/*
	 * Layer bitrate is preferred over cumulative bitrate.
	 * Cumulative bitrate is set only when we fall back.
	 */
	rc = msm_venc_set_layer_bitrate(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_bitrate(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_video_csc(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_blur_resolution(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_extradata(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_operating_rate(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_buffer_counts(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_rotation(inst);
	if (rc)
		goto exit;

exit:
	if (rc)
		dprintk(VIDC_ERR, "%s: failed with %d\n", __func__, rc);
	else
		dprintk(VIDC_DBG, "%s: set properties successful\n", __func__);

	return rc;
}
