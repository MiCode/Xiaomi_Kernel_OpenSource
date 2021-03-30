// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */
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
#define MIN_CHROMA_QP_OFFSET -12
#define MAX_INTRA_REFRESH_MBS ((7680 * 4320) >> 8)
#define MAX_LTR_FRAME_COUNT 10
#define MAX_NUM_B_FRAMES 1
#define MIN_CBRPLUS_W 640
#define MIN_CBRPLUS_H 480
#define MAX_CBR_W 1280
#define MAX_CBR_H 720
#define LEGACY_CBR_BUF_SIZE 500
#define CBR_PLUS_BUF_SIZE 1000
#define MAX_GOP 0xFFFFFFF
#define MAX_QPRANGE_BOOST 0x3333

#define MIN_NUM_ENC_OUTPUT_BUFFERS 4
#define MIN_NUM_ENC_CAPTURE_BUFFERS 5
#define VENC_MAX_TIMESTAMP_LIST_SIZE 2

static const char *const mpeg_video_rate_control[] = {
	"VBR",
	"CBR",
	"CBR VFR",
	"MBR",
	"MBR VFR",
	"CQ",
	NULL
};

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

static const char *const mpeg_video_stream_format[] = {
	"NAL Format Start Codes",
	"NAL Format One NAL Per Buffer",
	"NAL Format One Byte Length",
	"NAL Format Two Byte Length",
	"NAL Format Four Byte Length",
	NULL
};

static const char *const roi_map_type[] = {
	"None",
	"2-bit",
	"2-bit",
};

static struct msm_vidc_ctrl msm_venc_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_UNKNOWN,
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
		.maximum = MAX_GOP,
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
		.maximum = HEIC_GRID_DIMENSION,
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
		.default_value = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC,
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
		.qmenu = mpeg_video_h264_profile,
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
		.maximum = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES,
		.default_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE) |
		(1 << V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) |
		(1 << V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES)
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
		.maximum = DB_DISABLE_SLICE_BOUNDARY,
		.default_value = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED) |
		(1 << V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED) |
		(1 << DB_DISABLE_SLICE_BOUNDARY)
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
			EXTRADATA_ENC_INPUT_CVP | EXTRADATA_ENC_FRAME_QP,
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
		.maximum = ((1 << MAX_LTR_FRAME_COUNT) - 1),
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
		.minimum = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B,
		.maximum = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		.default_value = V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B) |
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
		.default_value = V4L2_MPEG_MSM_VIDC_ENABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE,
		.name = "Encoder Operating rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = (DEFAULT_FPS << 16),/* Power Vote min fps */
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
		.maximum = S32_MAX,
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
		.default_value = 1,
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
		.name = "HDR PQ information",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = INT_MIN,
		.maximum = INT_MAX,
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
		.id = V4L2_CID_MPEG_VIDEO_H264_CHROMA_QP_INDEX_OFFSET,
		.name = "Chroma QP Index Offset",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_CHROMA_QP_OFFSET,
		.maximum = INT_MAX,
		.default_value = INT_MAX,
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
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = V4L2_MPEG_VIDC_VIDEO_BRS_DISABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_BRS_ENABLE_ALL,
		.default_value = V4L2_MPEG_VIDC_VIDEO_BRS_ENABLE_ALL,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_BITRATE_BOOST,
		.name = "Bitrate boost margin",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 100,
		.default_value = 25,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_QPRANGE_BOOST,
		.name = "Bitrate boost QP range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_QPRANGE_BOOST,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VBV_DELAY,
		.name = "Set Vbv Delay",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 1000,
		.default_value = 0,
		.step = 500,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_SUPERFRAME,
		.name = "Superframe",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = VIDC_SUPERFRAME_MAX,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_CAPTURE_FRAME_RATE,
		.name = "Capture Frame Rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = (MINIMUM_FPS << 16),
		.maximum = (MAXIMUM_FPS << 16),
		.default_value = (DEFAULT_FPS << 16),
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_CVP_FRAME_RATE,
		.name = "CVP Frame Rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = (MINIMUM_FPS << 16),
		.maximum = (MAXIMUM_FPS << 16),
		.default_value = (DEFAULT_FPS << 16),
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE,
		.name = "ROI Type",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_NONE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_2BYTE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_NONE,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_NONE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_2BIT) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_2BYTE)
		),
		.qmenu = roi_map_type,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_venc_ctrls)

static struct msm_vidc_format_desc venc_input_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
	},
	{
		.name = "YCrCb Semiplanar 4:2:0",
		.description = "Y/CrCb 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
	},
	{
		.name = "TP10 UBWC 4:2:0",
		.description = "TP10 UBWC 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 10bit",
		.description = "Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 128 aligned",
		.description = "Y/CbCr 4:2:0 128 aligned",
		.fourcc = V4L2_PIX_FMT_NV12_128,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 512 aligned",
		.description = "Y/CbCr 4:2:0 512 aligned",
		.fourcc = V4L2_PIX_FMT_NV12_512,
	},
	{
		.name = "32bit RGBA UBWC 8:8:8:8",
		.description = "32-bit RGBA UBWC 8:8:8:8",
		.fourcc = V4L2_PIX_FMT_RGBA8888_UBWC,
	},
};

static struct msm_vidc_format_desc venc_output_formats[] = {
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
		.fourcc = V4L2_PIX_FMT_NV12_128,
		.num_planes = 2,
		.y_max_stride = 8192,
		.y_buffer_alignment = 128,
		.uv_max_stride = 8192,
		.uv_buffer_alignment = 32,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12_512,
		.num_planes = 2,
		.y_max_stride = 16384,
		.y_buffer_alignment = 512,
		.uv_max_stride = 16384,
		.uv_buffer_alignment = 256,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.y_max_stride = 16384,
		.y_buffer_alignment = 512,
		.uv_max_stride = 16384,
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

u32 v4l2_to_hfi_flip(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *hflip = NULL;
	struct v4l2_ctrl *vflip = NULL;
	u32 flip = HFI_FLIP_NONE;

	hflip = get_ctrl(inst, V4L2_CID_HFLIP);
	vflip = get_ctrl(inst, V4L2_CID_VFLIP);

	if ((hflip->val == V4L2_MPEG_MSM_VIDC_ENABLE) &&
		(vflip->val == V4L2_MPEG_MSM_VIDC_ENABLE))
		flip = HFI_FLIP_HORIZONTAL | HFI_FLIP_VERTICAL;
	else if (hflip->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		flip = HFI_FLIP_HORIZONTAL;
	else if (vflip->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		flip = HFI_FLIP_VERTICAL;

	return flip;
}

static int msm_venc_set_csc(struct msm_vidc_inst *inst,
					u32 color_primaries, u32 custom_matrix);

int msm_venc_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_format_desc *fmt_desc = NULL;
	struct v4l2_format *f = NULL;
	uint32_t vpu;

	if (!inst) {
		d_vpr_e("Invalid input = %pK\n", inst);
		return -EINVAL;
	}
	vpu = inst->core->platform_data->vpu_ver;
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	f->type = OUTPUT_MPLANE;
	f->fmt.pix_mp.height = DEFAULT_HEIGHT;
	f->fmt.pix_mp.width = DEFAULT_WIDTH;
	f->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
	f->fmt.pix_mp.num_planes = 1;
	f->fmt.pix_mp.plane_fmt[0].sizeimage =
		msm_vidc_calculate_enc_output_frame_size(inst);
	fmt_desc = msm_comm_get_pixel_fmt_fourcc(venc_output_formats,
		ARRAY_SIZE(venc_output_formats),
		f->fmt.pix_mp.pixelformat, inst->sid);
	if (!fmt_desc) {
		s_vpr_e(inst->sid, "Invalid fmt set : %x\n",
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
	f->fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12_UBWC;
	f->fmt.pix_mp.num_planes = 1;
	if (vpu == VPU_VERSION_IRIS2)
		f->fmt.pix_mp.num_planes = 2;
	f->fmt.pix_mp.plane_fmt[0].sizeimage =
		msm_vidc_calculate_enc_input_frame_size(inst);
	f->fmt.pix_mp.plane_fmt[1].sizeimage =
		msm_vidc_calculate_enc_input_extra_size(inst);
	fmt_desc = msm_comm_get_pixel_fmt_fourcc(venc_input_formats,
		ARRAY_SIZE(venc_input_formats), f->fmt.pix_mp.pixelformat,
		inst->sid);
	if (!fmt_desc) {
		s_vpr_e(inst->sid, "Invalid fmt set : %x\n",
			f->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}
	strlcpy(inst->fmts[INPUT_PORT].name, fmt_desc->name,
		sizeof(inst->fmts[INPUT_PORT].name));
	strlcpy(inst->fmts[INPUT_PORT].description, fmt_desc->description,
		sizeof(inst->fmts[INPUT_PORT].description));
	inst->prop.extradata_ctrls = EXTRADATA_NONE;
	inst->buffer_mode_set[INPUT_PORT] = HAL_BUFFER_MODE_DYNAMIC;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->clk_data.frame_rate = (DEFAULT_FPS << 16);

	inst->clk_data.operating_rate = (DEFAULT_FPS << 16);
	inst->clk_data.is_legacy_cbr = false;

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
	inst->static_rotation_flip_enabled = false;
	inst->external_blur = false;
	return rc;
}

int msm_venc_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f)
{
	const struct msm_vidc_format_desc *fmt_desc = NULL;
	int rc = 0;

	if (!inst || !f) {
		d_vpr_e("Invalid input, inst = %pK, f = %pK\n", inst, f);
		return -EINVAL;
	}
	if (f->type == OUTPUT_MPLANE) {
		fmt_desc = msm_comm_get_pixel_fmt_index(venc_output_formats,
			ARRAY_SIZE(venc_output_formats), f->index, inst->sid);
		f->flags = V4L2_FMT_FLAG_COMPRESSED;
	} else if (f->type == INPUT_MPLANE) {
		fmt_desc = msm_comm_get_pixel_fmt_index(venc_input_formats,
			ARRAY_SIZE(venc_input_formats), f->index, inst->sid);
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
	vpe_csc.custom_matrix_enabled = custom_matrix ? 7 : 0;

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
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
	return rc;
}

int msm_venc_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	int rc = 0;
	struct msm_vidc_format *fmt = NULL;
	struct msm_vidc_format_desc *fmt_desc = NULL;
	struct v4l2_pix_format_mplane *mplane = NULL;
	u32 color_format;

	if (!inst || !f) {
		d_vpr_e("Invalid input, inst = %pK, format = %pK\n", inst, f);
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
		fmt_desc = msm_comm_get_pixel_fmt_fourcc(venc_output_formats,
			ARRAY_SIZE(venc_output_formats),
			f->fmt.pix_mp.pixelformat, inst->sid);
		if (!fmt_desc) {
			s_vpr_e(inst->sid, "Invalid fmt set : %x\n",
				f->fmt.pix_mp.pixelformat);
			return -EINVAL;
		}
		strlcpy(fmt->name, fmt_desc->name, sizeof(fmt->name));
		strlcpy(fmt->description, fmt_desc->description,
			sizeof(fmt->description));

		fmt->v4l2_fmt.type = f->type;
		mplane = &fmt->v4l2_fmt.fmt.pix_mp;
		mplane->width = f->fmt.pix_mp.width;
		mplane->height = f->fmt.pix_mp.height;
		mplane->pixelformat = f->fmt.pix_mp.pixelformat;

		if (!inst->profile) {
			rc = msm_venc_set_default_profile(inst);
			if (rc) {
				s_vpr_e(inst->sid,
					"%s: Failed to set default profile type\n",
					__func__);
				goto exit;
			}
		}

		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			s_vpr_e(inst->sid, "Failed to open instance\n");
			goto exit;
		}

		mplane->plane_fmt[0].sizeimage =
			msm_vidc_calculate_enc_output_frame_size(inst);
		if (mplane->num_planes > 1)
			mplane->plane_fmt[1].sizeimage =
				msm_vidc_calculate_enc_output_extra_size(inst);

		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"%s: session not supported\n", __func__);
			goto exit;
		}
		update_log_ctxt(inst->sid, inst->session_type,
			mplane->pixelformat);
		memcpy(f, &fmt->v4l2_fmt, sizeof(struct v4l2_format));
	} else if (f->type == INPUT_MPLANE) {
		fmt = &inst->fmts[INPUT_PORT];
		fmt_desc = msm_comm_get_pixel_fmt_fourcc(venc_input_formats,
			ARRAY_SIZE(venc_input_formats),
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
			msm_vidc_calculate_enc_input_frame_size(inst);
		if (mplane->num_planes > 1)
			mplane->plane_fmt[1].sizeimage =
				msm_vidc_calculate_enc_input_extra_size(inst);
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
			goto exit;
		}

		memcpy(f, &fmt->v4l2_fmt, sizeof(struct v4l2_format));
	} else {
		s_vpr_e(inst->sid, "%s: Unsupported buf type: %d\n",
			__func__, f->type);
		rc = -EINVAL;
		goto exit;
	}
exit:
	return rc;
}

int msm_venc_set_default_profile(struct msm_vidc_inst *inst)
{
	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (get_v4l2_codec(inst) == V4L2_PIX_FMT_HEVC)
		inst->profile = HFI_HEVC_PROFILE_MAIN;
	else if (get_v4l2_codec(inst) == V4L2_PIX_FMT_H264)
		inst->profile = HFI_H264_PROFILE_HIGH;
	else
		s_vpr_e(inst->sid, "%s: Invalid codec type %#x\n",
			__func__, get_v4l2_codec(inst));
	return 0;
}

int msm_venc_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	struct v4l2_format *fmt;

	if (f->type == OUTPUT_MPLANE) {
		fmt = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
			msm_vidc_calculate_enc_output_frame_size(inst);
		if (fmt->fmt.pix_mp.num_planes > 1)
			fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
				msm_vidc_calculate_enc_output_extra_size(inst);
		memcpy(f, fmt, sizeof(struct v4l2_format));
	} else if (f->type == INPUT_MPLANE) {
		fmt = &inst->fmts[INPUT_PORT].v4l2_fmt;
		fmt->fmt.pix_mp.plane_fmt[0].sizeimage =
			msm_vidc_calculate_enc_input_frame_size(inst);
		if (fmt->fmt.pix_mp.num_planes > 1) {
			fmt->fmt.pix_mp.plane_fmt[1].sizeimage =
				msm_vidc_calculate_enc_input_extra_size(inst);
		}
		memcpy(f, fmt, sizeof(struct v4l2_format));
	} else {
		s_vpr_e(inst->sid, "%s: Unsupported buf type: %d\n",
			__func__, f->type);
		return -EINVAL;
	}

	return 0;
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
	u32 codec;

	if (!ctrl->val) {
		s_vpr_h(inst->sid, "RC is not enabled. Setting RC OFF\n");
		inst->rc_type = RATE_CONTROL_OFF;
	} else {
		rc_mode = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
		inst->rc_type = rc_mode->val;
	}

	codec = get_v4l2_codec(inst);
	if (msm_vidc_lossless_encode
		&& (codec == V4L2_PIX_FMT_HEVC ||
			codec == V4L2_PIX_FMT_H264)) {
		s_vpr_h(inst->sid,
			"Reset RC mode to RC_LOSSLESS for HEVC lossless encoding\n");
		inst->rc_type = RATE_CONTROL_LOSSLESS;
	}
	return 0;
}

static int msm_venc_resolve_rate_control(struct msm_vidc_inst *inst,
		struct v4l2_ctrl *ctrl)
{
	if (inst->rc_type == RATE_CONTROL_LOSSLESS) {
		s_vpr_h(inst->sid,
			"Skip RC mode when enabling lossless encoding\n");
		return 0;
	}

	if (inst->rc_type == RATE_CONTROL_OFF) {
		s_vpr_e(inst->sid, "RC is not enabled.\n");
		return -EINVAL;
	}

	if ((ctrl->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ) &&
		get_v4l2_codec(inst) != V4L2_PIX_FMT_HEVC) {
		s_vpr_e(inst->sid, "CQ supported only for HEVC\n");
		return -EINVAL;
	}
	inst->rc_type = ctrl->val;
	return 0;
}

static int msm_venc_update_bitrate(struct msm_vidc_inst *inst)
{
	u32 cabac_max_bitrate = 0;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (get_v4l2_codec(inst) == V4L2_PIX_FMT_H264) {
		cabac_max_bitrate = inst->capability.cap[CAP_CABAC_BITRATE].max;
		if ((inst->clk_data.bitrate > cabac_max_bitrate) &&
			(inst->entropy_mode == HFI_H264_ENTROPY_CABAC)) {
			s_vpr_h(inst->sid,
				"%s: update bitrate %u to max allowed cabac bitrate %u\n",
				__func__, inst->clk_data.bitrate,
				cabac_max_bitrate);
			inst->clk_data.bitrate = cabac_max_bitrate;
		}
	}
	return 0;
}

int msm_venc_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct msm_vidc_mastering_display_colour_sei_payload *mdisp_sei = NULL;
	struct msm_vidc_content_light_level_sei_payload *cll_sei = NULL;
	u32 i_qp_min, i_qp_max, p_qp_min, p_qp_max, b_qp_min, b_qp_max;
	struct v4l2_format *f;
	u32 codec;
	u32 sid;

	if (!inst || !inst->core || !inst->core->device || !ctrl) {
		d_vpr_e("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mdisp_sei = &(inst->hdr10_sei_params.disp_color_sei);
	cll_sei = &(inst->hdr10_sei_params.cll_sei);
	codec = get_v4l2_codec(inst);
	sid = inst->sid;

	s_vpr_h(sid, "%s: name %s, id 0x%x value %d\n",
		__func__, ctrl->name, ctrl->id, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		if (inst->state == MSM_VIDC_START_DONE) {
			if (inst->all_intra) {
				s_vpr_h(sid,
					"%s: ignore dynamic gop size for all intra\n",
					__func__);
				break;
			}
			rc = msm_venc_set_intra_period(inst);
			if (rc)
				s_vpr_e(sid, "%s: set intra period failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_request_keyframe(inst);
			if (rc)
				s_vpr_e(sid, "%s: set bitrate failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	{
		rc = msm_venc_resolve_rate_control(inst, ctrl);
		if (rc)
			s_vpr_e(sid, "%s: set bitrate mode failed\n", __func__);
		if (inst->state < MSM_VIDC_LOAD_RESOURCES)
			msm_vidc_calculate_buffer_counts(inst);
		break;
	}
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		inst->clk_data.bitrate = ctrl->val;
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_update_bitrate(inst);
			if (rc)
				s_vpr_e(sid, "%s: Update bitrate failed\n",
					__func__);
			rc = msm_venc_set_bitrate(inst);
			if (rc)
				s_vpr_e(sid, "%s: set bitrate failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE:
		inst->clk_data.frame_rate = ctrl->val;
		/* For HEIC image encode, set fps to 1 */
		if (is_grid_session(inst)) {
			s_vpr_h(sid, "%s: set fps to 1 for HEIC\n",
					__func__);
			inst->clk_data.frame_rate = 1 << 16;
		}
		if (inst->state < MSM_VIDC_LOAD_RESOURCES)
			msm_vidc_calculate_buffer_counts(inst);
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_frame_rate(inst, true);
			if (rc)
				s_vpr_e(sid, "%s: set frame rate failed\n",
					__func__);
			msm_comm_release_timestamps(inst);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		if (codec != V4L2_PIX_FMT_HEVC && codec != V4L2_PIX_FMT_H264) {
			s_vpr_e(sid,
				"Slice mode not supported for encoder %#x\n",
				codec);
			rc = -ENOTSUPP;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags &= ~VIDC_SECURE;
		if (ctrl->val)
			inst->flags |= VIDC_SECURE;
		f = &inst->fmts[INPUT_PORT].v4l2_fmt;
		f->fmt.pix_mp.num_planes = 1;
		s_vpr_h(sid, "%s: num planes %d for secure sessions\n",
					__func__, f->fmt.pix_mp.num_planes);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_ltr_useframe(inst);
			if (rc)
				s_vpr_e(sid, "%s: ltr useframe failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_ltr_markframe(inst);
			if (rc)
				s_vpr_e(sid, "%s: ltr markframe failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE:
		inst->flags &= ~VIDC_TURBO;
		if (ctrl->val == INT_MAX)
			inst->flags |= VIDC_TURBO;
		else
			inst->clk_data.operating_rate = ctrl->val;
		/* For HEIC image encode, set operating rate to 1 */
		if (is_grid_session(inst)) {
			s_vpr_h(sid, "%s: set operating rate to 1 for HEIC\n",
					__func__);
			inst->clk_data.operating_rate = 1 << 16;
		}
		if (inst->state < MSM_VIDC_LOAD_RESOURCES)
			msm_vidc_calculate_buffer_counts(inst);
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_operating_rate(inst);
			if (rc)
				s_vpr_e(sid, "%s: set operating rate failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
		inst->clk_data.low_latency_mode = !!ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VENC_HDR_INFO: {
		u32 info_type = ((u32)ctrl->val >> 28) & 0xF;
		u32 val = (ctrl->val & 0xFFFFFFF);

		s_vpr_h(sid, "Ctrl:%d, HDR Info with value %u (%#X)",
				info_type, val, ctrl->val);
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
			s_vpr_e(sid,
				"Unknown Ctrl:%d, not part of HDR Info with value %u",
				info_type, val);
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
			f = &inst->fmts[INPUT_PORT].v4l2_fmt;
			f->fmt.pix_mp.num_planes = 2;
			f->fmt.pix_mp.plane_fmt[1].sizeimage =
				msm_vidc_calculate_enc_input_extra_size(inst);
		}

		if ((inst->prop.extradata_ctrls & EXTRADATA_ADVANCED) ||
		(inst->prop.extradata_ctrls & EXTRADATA_ENC_FRAME_QP)) {
			f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
			f->fmt.pix_mp.num_planes = 2;
			f->fmt.pix_mp.plane_fmt[1].sizeimage =
				msm_vidc_calculate_enc_output_extra_size(inst);
		}

		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		rc = msm_venc_resolve_rc_enable(inst, ctrl);
		if (rc)
			s_vpr_e(sid, "%s: set rc enable failed\n", __func__);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		inst->profile = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val, sid);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		inst->level = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val, sid);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
		inst->level |=
			(msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val, sid) << 28);
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
			s_vpr_e(sid, "Invalid QP %#x\n", ctrl->val);
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
			s_vpr_e(sid, "Invalid I QP %#x\n", ctrl->val);
			return -EINVAL;
		}
		inst->client_set_ctrls |= CLIENT_SET_I_QP;
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_dyn_qp(inst, ctrl);
			if (rc)
				s_vpr_e(sid,
					"%s: setting dyn frame QP failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP:
		p_qp_min = inst->capability.cap[CAP_P_FRAME_QP].min;
		p_qp_max = inst->capability.cap[CAP_P_FRAME_QP].max;
		if (ctrl->val < p_qp_min || ctrl->val > p_qp_max) {
			s_vpr_e(sid, "Invalid P QP %#x\n", ctrl->val);
			return -EINVAL;
		}
		inst->client_set_ctrls |= CLIENT_SET_P_QP;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP:
		b_qp_min = inst->capability.cap[CAP_B_FRAME_QP].min;
		b_qp_max = inst->capability.cap[CAP_B_FRAME_QP].max;
		if (ctrl->val < b_qp_min || ctrl->val > b_qp_max) {
			s_vpr_e(sid, "Invalid B QP %#x\n", ctrl->val);
			return -EINVAL;
		}
		inst->client_set_ctrls |= CLIENT_SET_B_QP;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_hp_layer(inst);
			if (rc)
				s_vpr_e(sid, "%s: set dyn hp layer failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_base_layer_priority_id(inst);
			if (rc)
				s_vpr_e(sid, "%s: set baselayer id failed\n",
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
				s_vpr_e(sid, "%s: set layer bitrate failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
		if (inst->state == MSM_VIDC_START_DONE) {
			s_vpr_e(sid,
				"%s: Dynamic setting of Bframe is not supported\n",
				__func__);
			return -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS:
		if (inst->state < MSM_VIDC_START_DONE) {
			if ((ctrl->val != MSM_VIDC_BLUR_INTERNAL) &&
				(ctrl->val != MSM_VIDC_BLUR_DISABLE)) {
				inst->external_blur = true;
			}
		} else if (inst->state == MSM_VIDC_START_DONE) {
			if (!inst->external_blur) {
				s_vpr_e(sid, "%s: external blur not enabled", __func__);
				break;
			}
			if (ctrl->val == MSM_VIDC_BLUR_EXTERNAL_DYNAMIC) {
				s_vpr_h(sid,
					"%s: external blur setting already enabled\n",
					__func__);
				break;
			} else if (ctrl->val == MSM_VIDC_BLUR_INTERNAL) {
				s_vpr_e(sid,
					"%s: cannot change to internal blur config dynamically\n",
					__func__);
				break;
			} else {
				rc = msm_venc_set_blur_resolution(inst);
				if (rc)
					s_vpr_e(sid,
						"%s: set blur resolution failed\n",
						__func__);
			}
		}
		break;
	case V4L2_CID_HFLIP:
	case V4L2_CID_VFLIP:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_dynamic_flip(inst);
			if (rc)
				s_vpr_e(sid, "%s: set flip failed\n", __func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_CVP_FRAME_RATE:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_cvp_skipratio(inst);
			if (rc)
				s_vpr_e(sid,
				"%s: set cvp skip ratio failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY:
		if (inst->state == MSM_VIDC_START_DONE) {
			rc = msm_venc_set_frame_quality(inst);
			if (rc)
				s_vpr_e(sid,
				"%s: set frame quality failed\n",
					__func__);
		}
		break;
	case V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE:
		/* For HEIC image encode, set fps to 1 */
		if (ctrl->val) {
			s_vpr_h(sid, "%s: set fps to 1 for HEIC\n",
					__func__);
			inst->clk_data.frame_rate = 1 << 16;
			s_vpr_h(sid, "%s: set operating rate to 1 for HEIC\n",
					__func__);
			inst->clk_data.operating_rate = 1 << 16;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE:
		inst->full_range = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VENC_BITRATE_BOOST:
		inst->boost_enabled = true;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		inst->entropy_mode = msm_comm_v4l2_to_hfi(
			V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE,
			ctrl->val, inst->sid);
		break;
	case V4L2_CID_MPEG_VIDC_CAPTURE_FRAME_RATE:
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE:
	case V4L2_CID_ROTATE:
	case V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT:
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
	case V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS:
	case V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC:
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX:
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
	case V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO:
	case V4L2_CID_MPEG_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH:
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT:
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM:
	case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB:
	case V4L2_CID_MPEG_VIDC_VENC_NATIVE_RECORDER:
	case V4L2_CID_MPEG_VIDC_VENC_RC_TIMESTAMP_DISABLE:
	case V4L2_CID_MPEG_VIDEO_VBV_DELAY:
	case V4L2_CID_MPEG_VIDEO_H264_CHROMA_QP_INDEX_OFFSET:
	case V4L2_CID_MPEG_VIDC_VENC_BITRATE_SAVINGS:
	case V4L2_CID_MPEG_VIDC_VENC_QPRANGE_BOOST:
	case V4L2_CID_MPEG_VIDC_SUPERFRAME:
		s_vpr_h(sid, "Control set: ID : 0x%x Val : %d\n",
			ctrl->id, ctrl->val);
		break;
	default:
		s_vpr_e(sid, "Unsupported index: 0x%x\n", ctrl->id);
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
	struct v4l2_format *f;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	frame_sz.buffer_type = HFI_BUFFER_INPUT;
	frame_sz.width = f->fmt.pix_mp.width;
	frame_sz.height = f->fmt.pix_mp.height;
	s_vpr_h(inst->sid, "%s: input %d %d\n", __func__,
			frame_sz.width, frame_sz.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_sz, sizeof(frame_sz));
	if (rc) {
		s_vpr_e(inst->sid, "%s: failed to set input frame size %d %d\n",
			__func__, frame_sz.width, frame_sz.height);
		return rc;
	}

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	frame_sz.buffer_type = HFI_BUFFER_OUTPUT;
	frame_sz.width = f->fmt.pix_mp.width;
	frame_sz.height = f->fmt.pix_mp.height;
	/* firmware needs grid size in output where as
	 * client sends out full resolution in output port */
	if (is_grid_session(inst)) {
		frame_sz.width = frame_sz.height = HEIC_GRID_DIMENSION;
	}
	s_vpr_h(inst->sid, "%s: output %d %d\n", __func__,
			frame_sz.width, frame_sz.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_sz, sizeof(frame_sz));
	if (rc) {
		s_vpr_e(inst->sid,
			"%s: failed to set output frame size %d %d\n",
			__func__, frame_sz.width, frame_sz.height);
		return rc;
	}

	return rc;
}

int msm_venc_set_frame_rate(struct msm_vidc_inst *inst, bool external_requested)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_frame_rate frame_rate;
	struct msm_vidc_capability *capability;
	u32 fps_max;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;
	capability = &inst->capability;

	/* Check frame rate */
	if (inst->all_intra)
		fps_max = capability->cap[CAP_ALLINTRA_MAX_FPS].max;
	else
		fps_max = capability->cap[CAP_FRAMERATE].max;

	if (inst->clk_data.frame_rate >> 16 > fps_max) {
		s_vpr_e(inst->sid,
			"%s: Unsupported frame rate, fps %u, max_fps %u\n",
			__func__, inst->clk_data.frame_rate >> 16, fps_max);
		return -ENOTSUPP;
	}

	frame_rate.buffer_type = HFI_BUFFER_OUTPUT;
	frame_rate.frame_rate = inst->clk_data.frame_rate;

	s_vpr_h(inst->sid, "%s: %#x\n", __func__, frame_rate.frame_rate);

	if (external_requested) {
		rc = call_hfi_op(hdev, session_set_property,
			inst->session, HFI_PROPERTY_CONFIG_FRAME_RATE,
			&frame_rate, sizeof(frame_rate));
	} else {
		s_vpr_l(inst->sid, "Auto frame rate set");
		rc = call_hfi_op(hdev, session_set_property,
		inst->session, HFI_PROPERTY_CONFIG_VENC_AUTO_FRAME_RATE,
		&frame_rate, sizeof(frame_rate));
	}
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_store_timestamp(struct msm_vidc_inst *inst, u64 timestamp_us)
{
	struct msm_vidc_timestamps *entry, *node, *prev = NULL;
	int count = 0;
	int rc = 0;
	struct v4l2_ctrl *superframe_ctrl = NULL;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (!inst->core->resources.enc_auto_dynamic_fps ||
		is_image_session(inst))
		return rc;

	mutex_lock(&inst->timestamps.lock);
	list_for_each_entry(node, &inst->timestamps.list, list) {
		count++;
		if (timestamp_us <= node->timestamp_us) {
			s_vpr_e(inst->sid, "%s: invalid ts %llu, exist %llu\n",
				__func__, timestamp_us, node->timestamp_us);
			goto unlock;
		}
	}

	/* Maintain a sliding window */
	entry = NULL;
	if (count >= VENC_MAX_TIMESTAMP_LIST_SIZE) {
		entry = list_first_entry(&inst->timestamps.list,
			struct msm_vidc_timestamps, list);
		list_del_init(&entry->list);
	}
	if (!entry) {
		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			s_vpr_e(inst->sid, "%s: ts malloc failure\n",
				__func__);
			rc = -ENOMEM;
			goto unlock;
		}
	}

	entry->timestamp_us = timestamp_us;
	entry->framerate = inst->clk_data.frame_rate;
	prev = list_last_entry(&inst->timestamps.list,
		struct msm_vidc_timestamps, list);
	list_add_tail(&entry->list, &inst->timestamps.list);

	if (!count)
		goto unlock;

	entry->framerate = msm_comm_calc_framerate(inst,
		timestamp_us, prev->timestamp_us);

	/* if framerate changed and stable for 2 frames, set to firmware */
	if (entry->framerate == prev->framerate &&
		entry->framerate != inst->clk_data.frame_rate) {
		superframe_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_SUPERFRAME);
		if (superframe_ctrl->val > 1)
			inst->clk_data.frame_rate = entry->framerate * superframe_ctrl->val;
		else
			inst->clk_data.frame_rate = entry->framerate;
		s_vpr_l(inst->sid, "%s: updated fps to %u\n",
			__func__, (inst->clk_data.frame_rate >> 16));
		msm_venc_set_frame_rate(inst, false);
	}

unlock:
	mutex_unlock(&inst->timestamps.lock);
	return rc;
}

int msm_venc_set_color_format(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_format_constraint *fmt_constraints;
	struct v4l2_format *f;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	rc = msm_comm_set_color_format(inst, HAL_BUFFER_INPUT,
				f->fmt.pix_mp.pixelformat);
	if (rc)
		return rc;

	fmt_constraints = msm_comm_get_pixel_fmt_constraints(
			enc_pix_format_constraints,
			ARRAY_SIZE(enc_pix_format_constraints),
			f->fmt.pix_mp.pixelformat, inst->sid);
	if (fmt_constraints) {
		rc = msm_comm_set_color_format_constraints(inst,
				HAL_BUFFER_INPUT,
				fmt_constraints);
		if (rc) {
			s_vpr_e(inst->sid, "Set constraints for %d failed\n",
				f->fmt.pix_mp.pixelformat);
			return rc;
		}
	}

	return rc;
}

int msm_venc_set_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_format *fmt;
	enum hal_buffer buffer_type;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	buffer_type = HAL_BUFFER_INPUT;
	fmt = &inst->fmts[INPUT_PORT];
	rc = msm_comm_set_buffer_count(inst,
			fmt->count_min,
			fmt->count_actual,
			buffer_type);
	if (rc) {
		s_vpr_e(inst->sid, "%s: failed to set bufcounts(%#x)\n",
			__func__, buffer_type);
		return -EINVAL;
	}

	buffer_type = HAL_BUFFER_OUTPUT;
	fmt = &inst->fmts[OUTPUT_PORT];
	rc = msm_comm_set_buffer_count(inst,
			fmt->count_min,
			fmt->count_actual,
			buffer_type);
	if (rc) {
		s_vpr_e(inst->sid, "%s: failed to set buf counts(%#x)\n",
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
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SECURE);
	enable.enable = !!ctrl->val;

	if (enable.enable) {
		codec = get_v4l2_codec(inst);
		if (!(codec == V4L2_PIX_FMT_H264 ||
			codec == V4L2_PIX_FMT_HEVC)) {
			s_vpr_e(inst->sid,
				"%s: Secure mode only allowed for HEVC/H264\n",
				__func__);
			return -EINVAL;
		}
	}

	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_SECURE_SESSION, &enable, sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_priority(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	enable.enable = is_realtime_session(inst);

	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_REALTIME, &enable, sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_operating_rate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_operating_rate op_rate;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	hdev = inst->core->device;
	op_rate.operating_rate = inst->clk_data.operating_rate;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, op_rate.operating_rate >> 16);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_OPERATING_RATE, &op_rate, sizeof(op_rate));
	if (rc) {
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
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
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!inst->profile) {
		s_vpr_e(inst->sid, "%s: skip as client did not set profile\n",
			__func__);
		return -EINVAL;
	}
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

int msm_venc_set_idr_period(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_idr_period idr_period;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_H264 && codec != V4L2_PIX_FMT_HEVC)
		return 0;

	idr_period.idr_period = 1;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, idr_period.idr_period);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_IDR_PERIOD, &idr_period,
		sizeof(idr_period));
	if (rc) {
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
		return rc;
	}

	return rc;
}

int msm_venc_set_adaptive_bframes(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	enable.enable = true;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_ADAPTIVE_B, &enable, sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

void msm_venc_adjust_gop_size(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *hier_ctrl;
	struct v4l2_ctrl *gop_size_ctrl;
	s32 val;

	gop_size_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_GOP_SIZE);

	/*
	 * Layer encoding needs GOP size to be multiple of subgop size
	 * And subgop size is 2 ^ number of enhancement layers
	 */
	hier_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	if (hier_ctrl->val > 1) {
		u32 min_gop_size;
		u32 num_subgops;

		min_gop_size = (1 << (hier_ctrl->val - 1));
		num_subgops = (gop_size_ctrl->val + (min_gop_size >> 1)) /
				min_gop_size;
		if (num_subgops)
			val = num_subgops * min_gop_size;
		else
			val = min_gop_size;

		update_ctrl(gop_size_ctrl, val, inst->sid);
	}
}

int msm_venc_set_intra_period(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *gop_size = NULL;
	struct v4l2_ctrl *bframes = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	struct v4l2_ctrl *frame_t = NULL;
	struct hfi_intra_period intra_period = {
		.pframes = 0,
		.bframes = 0
	};
	struct hfi_adaptive_p_b_intra_period adaptive_p_b_intra_period = {
		.nframes = 0
	};
	u32 codec;
	bool adaptive_bframes = false;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	frame_t = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE);
	gop_size = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_GOP_SIZE);
	max_layer = get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	codec = get_v4l2_codec(inst);

	intra_period.pframes = gop_size->val;

	if (!max_layer->val && codec == V4L2_PIX_FMT_H264) {
		/*
		 * At this point we've already made decision on bframe.
		 * Control value gives updated bframe value.
		 */
		bframes = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
		intra_period.bframes = bframes->val;
		if (intra_period.bframes)
			adaptive_bframes = true;
	}

	if (max_layer->val > 1) {
		if (frame_t->val ==
			V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B) {
			if (codec == V4L2_PIX_FMT_HEVC) {
				adaptive_p_b_intra_period.nframes =
					gop_size->val;
				adaptive_bframes = true;
			} else {
				d_vpr_e("%s: Hier-B supported for HEVC only\n",
						__func__);
				return -EINVAL;
			}
		} else if (frame_t->val ==
			V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P) {
			msm_venc_adjust_gop_size(inst);
			intra_period.pframes = gop_size->val;
			intra_period.bframes = 0;
			adaptive_bframes = false;
		}
	}

	if (inst->state == MSM_VIDC_START_DONE &&
			!intra_period.pframes && !intra_period.bframes) {
		s_vpr_h(inst->sid,
			"%s: Switch from IPPP to All Intra is not allowed\n",
			__func__);
		return rc;
	}

	if (frame_t->val ==
			V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B &&
			codec == V4L2_PIX_FMT_HEVC) {
		s_vpr_h(inst->sid, "%s: nframes: %d\n",
				__func__, adaptive_p_b_intra_period.nframes);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD,
				&adaptive_p_b_intra_period,
				sizeof(adaptive_p_b_intra_period));

	} else {
		s_vpr_h(inst->sid, "%s: pframes: %d bframes: %d\n",
				__func__, intra_period.pframes,
				intra_period.bframes);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HFI_PROPERTY_CONFIG_VENC_INTRA_PERIOD,
				&intra_period, sizeof(intra_period));
	}

	if (rc) {
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
		return rc;
	}

	if (adaptive_bframes) {
		rc = msm_venc_set_adaptive_bframes(inst);
		if (rc) {
			s_vpr_e(inst->sid, "%s: set property failed\n",
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
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	s_vpr_h(inst->sid, "%s\n", __func__);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_REQUEST_SYNC_FRAME, NULL, 0);
	if (rc) {
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
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
	struct v4l2_format *f;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	hdev = inst->core->device;
	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	codec = get_v4l2_codec(inst);
	height = f->fmt.pix_mp.height;
	width = f->fmt.pix_mp.width;
	mbpf = NUM_MBS_PER_FRAME(height, width);

	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_MBR_VFR)
		inst->rc_type = V4L2_MPEG_VIDEO_BITRATE_MODE_MBR;
	else if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR &&
			   inst->clk_data.low_latency_mode)
		inst->rc_type = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;

	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR ||
		inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR)
		inst->clk_data.low_latency_mode = true;

	switch (inst->rc_type) {
	case RATE_CONTROL_OFF:
	case RATE_CONTROL_LOSSLESS:
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
		s_vpr_e(inst->sid,
			"Invalid Rate control setting: %d Default RCOFF\n",
			inst->rc_type);
		break;
	}
	s_vpr_h(inst->sid, "%s: %d\n", __func__, inst->rc_type);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_RATE_CONTROL, &hfi_rc,
		sizeof(u32));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}



int msm_venc_set_vbv_delay(struct msm_vidc_inst *inst)
{
	int rc = 0;
	bool is_legacy_cbr;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	u32 codec, height, width, buf_size;
	struct hfi_vbv_hrd_buf_size hrd_buf_size;
	struct v4l2_format *f;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	hdev = inst->core->device;
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	codec = get_v4l2_codec(inst);
	height = f->fmt.pix_mp.height;
	width = f->fmt.pix_mp.width;

	/* vbv delay is required for CBR_CFR and CBR_VFR only */
	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_CBR &&
		inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR)
		return 0;

	/* Default behavior */
	is_legacy_cbr = false;
	buf_size = CBR_PLUS_BUF_SIZE;

	/*
	 * Client can set vbv delay only when
	 * resolution is between VGA and 720p
	 */
	if (res_is_greater_than_or_equal_to(width, height, MIN_CBRPLUS_W,
		MIN_CBRPLUS_H) && res_is_less_than_or_equal_to(width, height,
		MAX_CBR_W, MAX_CBR_H)) {
		ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_VBV_DELAY);
		if (ctrl->val == LEGACY_CBR_BUF_SIZE) {
			is_legacy_cbr = true;
			buf_size = LEGACY_CBR_BUF_SIZE;
			goto set_vbv_delay;
		} else if (ctrl->val == CBR_PLUS_BUF_SIZE) {
			is_legacy_cbr = false;
			buf_size = CBR_PLUS_BUF_SIZE;
			goto set_vbv_delay;
		}
	}

	/* Enable legacy cbr if resolution < MIN_CBRPLUS (720p) */
	if (res_is_less_than(width, height, MAX_CBR_W, MAX_CBR_H)) {
		is_legacy_cbr = true;
		buf_size = LEGACY_CBR_BUF_SIZE;
		goto set_vbv_delay;
	}

set_vbv_delay:
	inst->clk_data.is_legacy_cbr = is_legacy_cbr;
	hrd_buf_size.vbv_hrd_buf_size = buf_size;
	s_vpr_h(inst->sid, "%s: %d\n", __func__, hrd_buf_size.vbv_hrd_buf_size);
	rc = call_hfi_op(hdev, session_set_property,
		(void *)inst->session,
		HFI_PROPERTY_CONFIG_VENC_VBV_HRD_BUF_SIZE,
		(void *)&hrd_buf_size, sizeof(hrd_buf_size));
	if (rc) {
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
	}
	return rc;
}


int msm_venc_set_input_timestamp_rc(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
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

	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_DISABLE_RC_TIMESTAMP, &enable,
		sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_bitrate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_bitrate bitrate;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		return 0;

	if (inst->layer_bitrate) {
		s_vpr_h(inst->sid, "%s: Layer bitrate is enabled\n", __func__);
		return 0;
	}

	enable.enable = 0;
	s_vpr_h(inst->sid, "%s: bitrate type: %d\n",
		__func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_BITRATE_TYPE, &enable,
		sizeof(enable));
	if (rc) {
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
		return rc;
	}

	bitrate.bit_rate = inst->clk_data.bitrate;
	bitrate.layer_id = MSM_VIDC_ALL_LAYER_ID;
	s_vpr_h(inst->sid, "%s: %d\n", __func__, bitrate.bit_rate);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE, &bitrate,
		sizeof(bitrate));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_layer_bitrate(struct msm_vidc_inst *inst)
{
	int rc = 0, i = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *layer = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	struct v4l2_ctrl *layer_br_ratios[MAX_HIER_CODING_LAYER] = {NULL};
	struct hfi_bitrate layer_br;
	struct hfi_enable enable;
	u32 bitrate;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);

	if (!max_layer->val || !layer->val) {
		s_vpr_h(inst->sid,
			"%s: Hier-P layer not set. Ignore layer bitrate\n",
			__func__);
		goto error;
	}

	if (max_layer->val < layer->val) {
		s_vpr_h(inst->sid,
			"%s: Hier-P layer greater than max isn't allowed\n",
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
		s_vpr_h(inst->sid, "%s: Improper layer bitrate ratio\n",
			__func__);
		goto error;
	}

	for (i = layer->val - 1; i > 0; --i) {
		if (layer_br_ratios[i]->val == 0) {
			s_vpr_h(inst->sid, "%s: Layer ratio must be non-zero\n",
				__func__);
			goto error;
		}
		layer_br_ratios[i]->val -= layer_br_ratios[i-1]->val;
	}

	enable.enable = 1;
	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_BITRATE_TYPE, &enable,
		sizeof(enable));
	if (rc) {
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
		goto error;
	}

	bitrate = inst->clk_data.bitrate;
	for (i = 0; i < layer->val; ++i) {
		layer_br.bit_rate =
			bitrate * layer_br_ratios[i]->val / 100;
		layer_br.layer_id = i;
		s_vpr_h(inst->sid, "%s: Bitrate for Layer[%u]: [%u]\n",
			__func__, layer_br.layer_id, layer_br.bit_rate);

		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_CONFIG_VENC_TARGET_BITRATE, &layer_br,
			sizeof(layer_br));
		if (rc) {
			s_vpr_e(inst->sid,
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
	struct hfi_quantization qp;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	qp.layer_id = MSM_VIDC_ALL_LAYER_ID;
	qp.enable = 0;
	qp.enable = QP_ENABLE_I | QP_ENABLE_P | QP_ENABLE_B;

	i_qp = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP);
	p_qp = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP);
	b_qp = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP);

	/*
	 * When RC is ON:
	 *   Enable QP types which have been set by client.
	 * When RC is OFF:
	 *   I_QP value must be set by client.
	 *   If other QP value is invalid, then, assign I_QP value to it.
	 */
	if (inst->rc_type != RATE_CONTROL_OFF) {
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
			s_vpr_e(inst->sid,
				"%s: Client value is not valid\n", __func__);
			return -EINVAL;
		}
		if (!(inst->client_set_ctrls & CLIENT_SET_P_QP))
			p_qp->val = i_qp->val;
		if (!(inst->client_set_ctrls & CLIENT_SET_B_QP))
			b_qp->val = i_qp->val;
	}

	qp.qp_packed = i_qp->val | p_qp->val << 8 | b_qp->val << 16;

	s_vpr_h(inst->sid, "%s: layers %#x frames %#x qp_packed %#x\n",
		__func__, qp.layer_id, qp.enable, qp.qp_packed);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_FRAME_QP, &qp, sizeof(qp));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_qp_range(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_quantization_range qp_range;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->client_set_ctrls & CLIENT_SET_MIN_QP) &&
		!(inst->client_set_ctrls & CLIENT_SET_MAX_QP) &&
		!inst->boost_qp_enabled) {
		s_vpr_h(inst->sid,
			"%s: Client didn't set QP range\n", __func__);
		return 0;
	}

	qp_range.min_qp.layer_id = MSM_VIDC_ALL_LAYER_ID;
	qp_range.max_qp.layer_id = MSM_VIDC_ALL_LAYER_ID;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP);
	if (inst->boost_qp_enabled &&
		!(inst->client_set_ctrls & CLIENT_SET_MIN_QP))
		qp_range.min_qp.qp_packed = inst->boost_min_qp;
	else
		qp_range.min_qp.qp_packed = ctrl->val;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP);
	if (inst->boost_qp_enabled &&
		!(inst->client_set_ctrls & CLIENT_SET_MAX_QP))
		qp_range.max_qp.qp_packed = inst->boost_max_qp;
	else
		qp_range.max_qp.qp_packed = ctrl->val;

	s_vpr_h(inst->sid, "%s: layers %#x qp_min %#x qp_max %#x\n",
			__func__, qp_range.min_qp.layer_id,
			qp_range.min_qp.qp_packed, qp_range.max_qp.qp_packed);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_SESSION_QP_RANGE, &qp_range,
		sizeof(qp_range));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

static void set_all_intra_preconditions(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl = NULL, *ctrl_t = NULL;

	/* Disable multi slice */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
	if (ctrl->val) {
		d_vpr_h("Disable multi slice for all intra\n");
		update_ctrl(ctrl, V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
			inst->sid);
	}

	/* Disable LTR */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (ctrl->val) {
		s_vpr_h(inst->sid, "Disable LTR for all intra\n");
		update_ctrl(ctrl, 0, inst->sid);
	}

	/* Disable Layer encoding */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);
	ctrl_t = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	if (ctrl->val || ctrl_t->val) {
		s_vpr_h(inst->sid, "Disable layer encoding for all intra\n");
		update_ctrl(ctrl, 0, inst->sid);
		update_ctrl(ctrl_t, 0, inst->sid);
	}

	/* Disable IR */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM);
	ctrl_t = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB);
	if (ctrl->val || ctrl_t->val) {
		s_vpr_h(inst->sid, "Disable IR for all intra\n");
		update_ctrl(ctrl, 0, inst->sid);
		update_ctrl(ctrl_t, 0, inst->sid);
	}

	return;
}

static void set_heif_preconditions(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl = NULL;

	/* Reset PFrames */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_GOP_SIZE);
	if (ctrl->val) {
		d_vpr_h("Reset P-frame count for HEIF\n");
		update_ctrl(ctrl, 0, inst->sid);
	}

	/* Reset BFrames */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	if (ctrl->val) {
		s_vpr_h(inst->sid, "Reset B-frame count for HEIF\n");
		update_ctrl(ctrl, 0, inst->sid);
	}

	return;
}

int msm_venc_set_frame_quality(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_heic_frame_quality frame_quality;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY);
	frame_quality.frame_quality = ctrl->val;

	s_vpr_h(inst->sid, "%s: frame quality: %d\n", __func__,
		frame_quality.frame_quality);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_HEIC_FRAME_QUALITY, &frame_quality,
		sizeof(frame_quality));
	if (rc)
		s_vpr_e(inst->sid, "%s: set frame quality failed\n", __func__);

	return rc;
}

int msm_venc_set_image_grid(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_heic_grid_enable grid_enable;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
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

	s_vpr_h(inst->sid, "%s: grid enable: %d\n", __func__,
		grid_enable.grid_enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_HEIC_GRID_ENABLE, &grid_enable,
		sizeof(grid_enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set grid enable failed\n", __func__);

	return rc;
}

int msm_venc_set_image_properties(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	if (!is_image_session(inst) && !is_grid_session(inst))
		return 0;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_CQ) {
		d_vpr_e("%s: invalid rate control mode\n", __func__);
		return -EINVAL;
	}

	rc = msm_venc_set_frame_quality(inst);
	if (rc) {
		s_vpr_e(inst->sid,
			"%s: set image property failed\n", __func__);
		return rc;
	}

	rc = msm_venc_set_image_grid(inst);
	if (rc) {
		s_vpr_e(inst->sid,
			"%s: set image property failed\n", __func__);
		return rc;
	}

	set_all_intra_preconditions(inst);
	set_heif_preconditions(inst);
	return rc;
}

int msm_venc_set_entropy_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_h264_entropy_control entropy;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (get_v4l2_codec(inst) != V4L2_PIX_FMT_H264)
		return 0;

	entropy.entropy_mode = inst->entropy_mode;
	entropy.cabac_model = HFI_H264_CABAC_MODEL_2;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, entropy.entropy_mode);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_H264_ENTROPY_CONTROL, &entropy,
		sizeof(entropy));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_slice_control_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct v4l2_ctrl *ctrl_t;
	struct hfi_multi_slice_control multi_slice_control;
	struct v4l2_format *f;
	int temp = 0;
	u32 mb_per_frame, fps, mbps, bitrate, max_slices;
	u32 slice_val, slice_mode, max_avg_slicesize;
	u32 rc_mode, output_width, output_height;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_HEVC && codec != V4L2_PIX_FMT_H264)
		return 0;

	slice_mode = HFI_MULTI_SLICE_OFF;
	slice_val = 0;

	bitrate = inst->clk_data.bitrate;
	fps = inst->clk_data.frame_rate >> 16;
	rc_mode = inst->rc_type;
	if (fps > 60 || (!(rc_mode == RATE_CONTROL_OFF ||
		 rc_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR ||
		 rc_mode == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR))) {
		goto set_and_exit;
	}

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	output_width = f->fmt.pix_mp.width;
	output_height = f->fmt.pix_mp.height;
	if ((codec == V4L2_PIX_FMT_HEVC) &&
		(output_height < 128 || output_width < 384))
		goto set_and_exit;

	if ((codec == V4L2_PIX_FMT_H264) &&
		(output_height < 128 || output_width < 192))
		goto set_and_exit;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
	if (ctrl->val == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB) {
		temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
		slice_mode = HFI_MULTI_SLICE_BY_MB_COUNT;
	} else if (ctrl->val == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_BYTES) {
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
			if (rc_mode != RATE_CONTROL_OFF) {
				max_avg_slicesize =
					((bitrate / fps) / 8) / max_slices;
				slice_val = max(slice_val, max_avg_slicesize);
			}
		}
	}

	if (slice_mode == HFI_MULTI_SLICE_OFF) {
		update_ctrl(ctrl, V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
			inst->sid);
		update_ctrl(ctrl_t, 0, inst->sid);
	}

set_and_exit:
	multi_slice_control.multi_slice = slice_mode;
	multi_slice_control.slice_size = slice_val;

	hdev = inst->core->device;
	s_vpr_h(inst->sid, "%s: %d %d\n", __func__,
			multi_slice_control.multi_slice,
			multi_slice_control.slice_size);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_MULTI_SLICE_CONTROL,
		&multi_slice_control, sizeof(multi_slice_control));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_intra_refresh_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct hfi_intra_refresh intra_refresh;
	struct v4l2_format *f;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (!(inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR ||
		inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR))
		return 0;

	/* Firmware supports only random mode */
	intra_refresh.mode = HFI_INTRA_REFRESH_RANDOM;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM);
	intra_refresh.mbs = 0;
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	if (ctrl->val) {
		u32 num_mbs_per_frame = 0;
		u32 width = f->fmt.pix_mp.width;
		u32 height = f->fmt.pix_mp.height;

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

	s_vpr_h(inst->sid, "%s: %d %d\n", __func__,
			intra_refresh.mode, intra_refresh.mbs);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_INTRA_REFRESH, &intra_refresh,
		sizeof(intra_refresh));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_chroma_qp_offset(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *chr;
	struct v4l2_ctrl *ctrl_cs;
	struct hfi_chroma_qp_offset chroma_qp;
	struct v4l2_format *f;
	u32 codec, width, height, mbpf;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	chr = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_CHROMA_QP_INDEX_OFFSET);
	if (chr->val == INT_MAX || (chr->val != 0 && chr->val != -12))
		return 0;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;
	mbpf = NUM_MBS_PER_FRAME(width, height);
	ctrl_cs = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
	codec = get_v4l2_codec(inst);

	/**
	 * Set chroma qp offset to HEVC & VBR_CFR rc
	 * 10 bit: only BT2020
	 *  8 bit: only mbpf >= num_mbs(7680, 3840)
	 */
	if (codec != V4L2_PIX_FMT_HEVC ||
		inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
		return 0;

	if ((inst->bit_depth == MSM_VIDC_BIT_DEPTH_10 &&
		ctrl_cs->val != MSM_VIDC_BT2020) ||
		(inst->bit_depth == MSM_VIDC_BIT_DEPTH_8 &&
		mbpf < NUM_MBS_PER_FRAME(7680, 3840)))
		return 0;

	/**
	 * client sets one chroma offset only in range [-12, 0]
	 * firmware expects chroma cb offset and cr offset in
	 * range [0, 12], firmware subtracts 12 from driver set values.
	 */
	chroma_qp.chroma_offset = (chr->val + 12) << 16 | (chr->val + 12);
	s_vpr_h(inst->sid, "%s: %x\n", __func__, chroma_qp.chroma_offset);

	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_HEVC_PPS_CB_CR_OFFSET, &chroma_qp,
		sizeof(chroma_qp));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_bitrate_savings_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *cac = NULL;
	struct v4l2_ctrl *profile = NULL;
	struct hfi_enable enable;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	enable.enable = 0;
	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) {
		s_vpr_h(inst->sid,
			"Disable bitrate savings for non-VBR_CFR\n");
		goto setprop;
	}

	codec = get_v4l2_codec(inst);
	profile = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE);
	cac = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_BITRATE_SAVINGS);

	if (codec == V4L2_PIX_FMT_HEVC &&
		profile->val == V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10)
		enable.enable = !!(cac->val & V4L2_MPEG_VIDC_VIDEO_BRS_ENABLE_10BIT);
	else
		enable.enable = !!(cac->val & V4L2_MPEG_VIDC_VIDEO_BRS_ENABLE_8BIT);

setprop:
	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_BITRATE_SAVINGS, &enable,
		sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
	else
		rc = msm_venc_set_bitrate_boost_margin(inst, enable.enable);

	return rc;
}

int msm_venc_set_bitrate_boost_margin(struct msm_vidc_inst *inst, u32 enable)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct hfi_bitrate_boost_margin boost_margin;
	int minqp, maxqp;
	uint32_t vpu;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;
	vpu = inst->core->platform_data->vpu_ver;

	if (!enable) {
		boost_margin.margin = 0;
		goto setprop;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_BITRATE_BOOST);

	/*
	 * For certain SOC, default value should be 0 unless client enabled
	 */
	if (!inst->boost_enabled && vpu == VPU_VERSION_AR50_LITE) {
		ctrl->val = 0;
		update_ctrl(ctrl, 0, inst->sid);
	}
	/* Mapped value to 0, 15, 25 or 50*/
	if (ctrl->val >= 50)
		boost_margin.margin = 50;
	else if (ctrl->val >= 25)
		boost_margin.margin = (u32)(ctrl->val/25) * 25;
	else
		boost_margin.margin = (u32)(ctrl->val/15) * 15;

setprop:
	s_vpr_h(inst->sid, "%s: %d\n", __func__, boost_margin.margin);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_BITRATE_BOOST, &boost_margin,
		sizeof(boost_margin));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	/* Boost QP range is only enabled when bitrate boost is enabled
	 * and boost QP range is set by client
	 */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_QPRANGE_BOOST);
	if (enable && ctrl->val) {
		minqp = ctrl->val & 0xFF;
		maxqp = (ctrl->val >> 8) & 0xFF;
		inst->boost_qp_enabled = true;
		inst->boost_min_qp = minqp | (minqp << 8) | (minqp << 16);
		inst->boost_max_qp = maxqp | (maxqp << 8) | (maxqp << 16);
	}

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
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_H264 && codec != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE);
	ctrl_a = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA);
	ctrl_b = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA);
	h264_db_control.mode = msm_comm_v4l2_to_hfi(
			V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
			ctrl->val, inst->sid);
	h264_db_control.slice_alpha_offset = ctrl_a->val;
	h264_db_control.slice_beta_offset = ctrl_b->val;

	s_vpr_h(inst->sid, "%s: %d %d %d\n", __func__,
		h264_db_control.mode, h264_db_control.slice_alpha_offset,
		h264_db_control.slice_beta_offset);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_H264_DEBLOCK_CONTROL, &h264_db_control,
		sizeof(h264_db_control));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_sequence_header_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_H264 || codec == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR);
	if (ctrl->val)
		enable.enable = true;
	else
		enable.enable = false;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_SYNC_FRAME_SEQUENCE_HEADER, &enable,
		sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_au_delimiter_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable enable;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_H264 || codec == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER);
	enable.enable = !!ctrl->val;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_GENERATE_AUDNAL, &enable,
		sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_enable_hybrid_hp(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl = NULL;
	struct v4l2_ctrl *layer = NULL;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	if (get_v4l2_codec(inst) != V4L2_PIX_FMT_H264)
		return 0;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (ctrl->val)
		return 0;

	ctrl = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	layer = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);
	if (ctrl->val == 0 || ctrl->val != layer->val)
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
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	if (max_layer->val <= 0) {
		s_vpr_h(inst->sid, "%s: Layer id can only be set with Hier-P\n",
			__func__);
		return 0;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID);
	baselayerid = ctrl->val;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, baselayerid);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_BASELAYER_PRIORITYID, &baselayerid,
		sizeof(baselayerid));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_hb_max_layer(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *frame_t = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	u32 hb_layer = 0;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_HEVC)
		return 0;

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	frame_t = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE);
	if (max_layer->val < 2 ||
		frame_t->val != V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_B) {
		s_vpr_h(inst->sid,
			"%s: Hier-B not requested for this session\n",
			__func__);
		return 0;
	}
	hb_layer = max_layer->val - 1;

	s_vpr_h(inst->sid, "%s: Hier-B max layer: %d\n",
		__func__, hb_layer);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_HIER_B_MAX_NUM_ENH_LAYER,
		&hb_layer, sizeof(hb_layer));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_hp_max_layer(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *frame_t = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	u32 hp_layer = 0;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_H264 && codec != V4L2_PIX_FMT_HEVC)
		return 0;

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	frame_t = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE);
	if (max_layer->val < 2 ||
		frame_t->val != V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P) {
		s_vpr_h(inst->sid,
			"%s: Hier-P not requested for this session\n",
			__func__);
		return 0;
	}

	rc = msm_venc_enable_hybrid_hp(inst);
	if (rc) {
		s_vpr_e(inst->sid, "%s: get hybrid hier-P decision failed\n",
			__func__);
		return rc;
	}

	/*
	 * We send enhancement layer count to FW,
	 * hence, input 0/1 indicates absence of layer encoding.
	 */
	if (max_layer->val)
		hp_layer = max_layer->val - 1;

	if (inst->hybrid_hp) {
		s_vpr_h(inst->sid, "%s: Hybrid hier-P layer: %d\n",
			__func__, hp_layer);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VENC_HIER_P_HYBRID_MODE,
			&hp_layer, sizeof(hp_layer));
	} else {
		s_vpr_h(inst->sid, "%s: Hier-P max layer: %d\n",
			__func__, hp_layer);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VENC_HIER_P_MAX_NUM_ENH_LAYER,
			&hp_layer, sizeof(hp_layer));
	}
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);
	return rc;
}

int msm_venc_set_hp_layer(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *frame_t = NULL;
	struct v4l2_ctrl *ctrl = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	u32 hp_layer = 0;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	frame_t = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE);
	if (frame_t->val != V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P) {
		s_vpr_h(inst->sid,
			"%s: Hier-P layer can be set for P type frame only\n",
			__func__);
		return 0;
	}

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_H264 && codec != V4L2_PIX_FMT_HEVC)
		return 0;

	if (inst->hybrid_hp) {
		s_vpr_e(inst->sid,
			"%s: Setting layer isn't allowed with hybrid hp\n",
			__func__);
		return 0;
	}

	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	ctrl = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);
	s_vpr_h(inst->sid, "%s: heir_layer: %d, max_hier_layer: %d\n",
			__func__, ctrl->val, max_layer->val);
	if (max_layer->val < ctrl->val) {
		s_vpr_e(inst->sid,
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

	s_vpr_h(inst->sid, "%s: Hier-P enhancement layer: %d\n",
		__func__, hp_layer);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_HIER_P_ENH_LAYER,
		&hp_layer, sizeof(hp_layer));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

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
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_H264 || codec == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl_cs = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
	ctrl_fr = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE);
	ctrl_tr = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS);
	ctrl_mc = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS);

	memset(&signal_info, 0, sizeof(struct hfi_video_signal_metadata));
	if (inst->full_range == COLOR_RANGE_UNSPECIFIED &&
		ctrl_cs->val == MSM_VIDC_RESERVED_1)
		signal_info.enable = false;
	else
		signal_info.enable = true;

	if (signal_info.enable) {
		signal_info.video_format = MSM_VIDC_NTSC;
		signal_info.video_full_range = ctrl_fr->val;
		if (ctrl_cs->val != MSM_VIDC_RESERVED_1) {
			signal_info.color_description = 1;
			signal_info.color_primaries = ctrl_cs->val;
			signal_info.transfer_characteristics = ctrl_tr->val;
			signal_info.matrix_coeffs = ctrl_mc->val;
		}
	}

	s_vpr_h(inst->sid, "%s: %d %d %d %d\n", __func__,
		signal_info.color_primaries, signal_info.video_full_range,
		signal_info.transfer_characteristics,
		signal_info.matrix_coeffs);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_VIDEO_SIGNAL_INFO, &signal_info,
		sizeof(signal_info));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_rotation(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *rotation = NULL;
	struct hfi_device *hdev;
	struct hfi_vpe_rotation_type vpe_rotation;

	hdev = inst->core->device;
	rotation = get_ctrl(inst, V4L2_CID_ROTATE);

	vpe_rotation.rotation = HFI_ROTATE_NONE;
	if (rotation->val == 90)
		vpe_rotation.rotation = HFI_ROTATE_90;
	else if (rotation->val == 180)
		vpe_rotation.rotation = HFI_ROTATE_180;
	else if (rotation->val ==  270)
		vpe_rotation.rotation = HFI_ROTATE_270;

	vpe_rotation.flip = v4l2_to_hfi_flip(inst);

	s_vpr_h(inst->sid, "Set rotation = %d, flip = %d\n",
			vpe_rotation.rotation, vpe_rotation.flip);
	rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session,
				HFI_PROPERTY_PARAM_VPE_ROTATION,
				&vpe_rotation, sizeof(vpe_rotation));
	if (rc) {
		s_vpr_e(inst->sid, "Set rotation/flip failed\n");
		return rc;
	}

	/* Mark static rotation/flip set */
	inst->static_rotation_flip_enabled = false;
	if ((vpe_rotation.rotation != HFI_ROTATE_NONE ||
		vpe_rotation.flip != HFI_FLIP_NONE) &&
		inst->state < MSM_VIDC_START_DONE)
		inst->static_rotation_flip_enabled = true;

	return rc;
}

int msm_venc_check_dynamic_flip_constraints(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *blur = NULL;
	struct v4l2_format *f = NULL;
	bool scalar_enable = false;
	bool blur_enable = false;
	u32 input_height, input_width;

	/* Dynamic flip is not allowed with scalar when static
	 * rotation/flip is disabled
	 */
	scalar_enable = vidc_scalar_enabled(inst);

	/* Check blur configs
	 * blur value = 0 -> enable auto blur
	 * blur value  = 2 or input resolution -> disable all blur
	 * For other values -> enable external blur
	 * Dynamic flip is not allowed with external blur enabled
	 */
	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	input_height = f->fmt.pix_mp.height;
	input_width = f->fmt.pix_mp.width;

	blur = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS);
	if (blur->val != 0 && blur->val != 2 &&
		((blur->val & 0xFFFF) != input_height ||
		(blur->val & 0x7FFF0000) >> 16 != input_width))
		blur_enable = true;
	s_vpr_h(inst->sid, "Blur = %u, height = %u, width = %u\n",
			blur->val, input_height, input_width);
	if (blur_enable) {
		/* Reject dynamic flip with external blur enabled */
		s_vpr_e(inst->sid,
			"Unsupported dynamic flip with external blur\n");
		rc = -EINVAL;
	} else if (scalar_enable && !inst->static_rotation_flip_enabled) {
		/* Reject dynamic flip with scalar enabled */
		s_vpr_e(inst->sid, "Unsupported dynamic flip with scalar\n");
		rc = -EINVAL;
	} else if (handle_vpss_restrictions(inst)) {
		s_vpr_e(inst->sid, "Unsupported resolution for dynamic flip\n");
		rc = -EINVAL;
	}

	return rc;
}

int msm_venc_set_dynamic_flip(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	u32 dynamic_flip;

	hdev = inst->core->device;

	rc = msm_venc_check_dynamic_flip_constraints(inst);
	if (rc) {
		d_vpr_e("%s: Dynamic flip unsupported\n", __func__);
		return rc;
	}

	/* Require IDR frame first */
	s_vpr_h(inst->sid, "Set dynamic IDR frame\n");
	rc = msm_venc_set_request_keyframe(inst);
	if (rc) {
		s_vpr_e(inst->sid, "%s: Dynamic IDR failed\n", __func__);
		return rc;
	}

	dynamic_flip = v4l2_to_hfi_flip(inst);
	s_vpr_h(inst->sid, "Dynamic flip = %d\n", dynamic_flip);
	rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session,
				HFI_PROPERTY_CONFIG_VPE_FLIP,
				&dynamic_flip, sizeof(dynamic_flip));
	if (rc) {
		s_vpr_e(inst->sid, "Set dynamic flip failed\n");
		return rc;
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
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (get_v4l2_codec(inst) != V4L2_PIX_FMT_H264 &&
		get_v4l2_codec(inst) != V4L2_PIX_FMT_HEVC)
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
		s_vpr_e(inst->sid, "%s: msm_venc_set_csc failed\n", __func__);

	return rc;
}

int msm_venc_set_8x8_transform(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct hfi_enable enable;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (get_v4l2_codec(inst) != V4L2_PIX_FMT_H264) {
		s_vpr_h(inst->sid, "%s: skip as codec is not H264\n",
			__func__);
		return 0;
	}

	if (inst->profile != HFI_H264_PROFILE_HIGH &&
		inst->profile != HFI_H264_PROFILE_CONSTRAINED_HIGH) {
		s_vpr_h(inst->sid, "%s: skip due to %#x\n",
			__func__, inst->profile);
		return 0;
	}

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM);
	enable.enable = !!ctrl->val;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, enable.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_H264_8X8_TRANSFORM, &enable,
		sizeof(enable));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_vui_timing_info(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_vui_timing_info timing_info;
	bool cfr, native_recorder;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_H264 && codec != V4L2_PIX_FMT_HEVC)
		return 0;

	native_recorder = false;
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_NATIVE_RECORDER);
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		native_recorder = true;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO);
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_DISABLE && native_recorder == false)
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
	timing_info.time_scale = (inst->clk_data.frame_rate >> 16) * USEC_PER_SEC;

	s_vpr_h(inst->sid, "%s: %d %d\n", __func__, timing_info.enable,
		timing_info.fixed_frame_rate);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_VUI_TIMING_INFO, &timing_info,
		sizeof(timing_info));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_nal_stream_format(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_nal_stream_format_select stream_format;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (codec != V4L2_PIX_FMT_H264 && codec != V4L2_PIX_FMT_HEVC)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD);
	stream_format.nal_stream_format_select = BIT(ctrl->val);

	/*
	 * Secure encode session supports 0x00000001 satrtcode based
	 * encoding only
	 */
	if (is_secure_session(inst) &&
		ctrl->val != V4L2_MPEG_VIDEO_HEVC_SIZE_0) {
		s_vpr_e(inst->sid,
			"%s: Invalid stream format setting for secure session\n",
			__func__);
		return -EINVAL;
	}

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
		s_vpr_e(inst->sid,
			"%s: Invalid stream format setting. Setting default\n",
			__func__);
		stream_format.nal_stream_format_select =
			HFI_NAL_FORMAT_STARTCODES;
		break;
	}

	s_vpr_h(inst->sid, "%s: %#x\n", __func__,
			stream_format.nal_stream_format_select);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_NAL_STREAM_FORMAT_SELECT, &stream_format,
		sizeof(stream_format));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	bool is_ltr = true;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_ltr_mode ltr;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (!ctrl->val)
		return 0;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_HEVC || codec == V4L2_PIX_FMT_H264)) {
		is_ltr = false;
		goto disable_ltr;
	}

	if (!(inst->rc_type == RATE_CONTROL_OFF ||
		inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR ||
		inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR)) {
			is_ltr = false;
			goto disable_ltr;
		}

	if (ctrl->val > inst->capability.cap[CAP_LTR_COUNT].max) {
		s_vpr_e(inst->sid, "%s: invalid ltr count %d, max %d\n",
			__func__, ctrl->val,
			inst->capability.cap[CAP_LTR_COUNT].max);
		return -EINVAL;
	}
	ltr.ltr_count =  ctrl->val;
	ltr.ltr_mode = HFI_LTR_MODE_MANUAL;
	ltr.trust_mode = 1;
	s_vpr_h(inst->sid, "%s: %d %d\n", __func__,
			ltr.ltr_mode, ltr.ltr_count);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_LTRMODE, &ltr, sizeof(ltr));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

disable_ltr:
	/*
	 * Forcefully setting LTR count to zero when
	 * client sets unsupported codec/rate control.
	 */
	if (!is_ltr) {
		update_ctrl(ctrl, 0, inst->sid);
		s_vpr_h(inst->sid, "LTR is forcefully disabled!\n");
	}
	return rc;
}

int msm_venc_set_ltr_useframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_ltr_use use_ltr;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (!ctrl->val)
		return 0;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_HEVC || codec == V4L2_PIX_FMT_H264))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME);
	use_ltr.ref_ltr = ctrl->val;
	use_ltr.use_constrnt = true;
	use_ltr.frames = 0;
	s_vpr_h(inst->sid, "%s: %d\n", __func__, use_ltr.ref_ltr);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_USELTRFRAME, &use_ltr,
		sizeof(use_ltr));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_ltr_markframe(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_ltr_mark mark_ltr;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	if (!ctrl->val)
		return 0;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_HEVC || codec == V4L2_PIX_FMT_H264))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME);
	mark_ltr.mark_frame = ctrl->val;

	s_vpr_h(inst->sid, "%s: %d\n", __func__, mark_ltr.mark_frame);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_MARKLTRFRAME, &mark_ltr,
		sizeof(mark_ltr));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_dyn_qp(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_quantization qp;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (inst->rc_type != RATE_CONTROL_OFF) {
		s_vpr_e(inst->sid, "%s: Dyn qp is set only when RC is OFF\n",
			__func__);
		return -EINVAL;
	}

	qp.qp_packed = ctrl->val | ctrl->val << 8 | ctrl->val << 16;
	qp.enable = QP_ENABLE_I | QP_ENABLE_P | QP_ENABLE_B;
	qp.layer_id = MSM_VIDC_ALL_LAYER_ID;

	s_vpr_h(inst->sid, "%s: %#x\n", __func__,
		ctrl->val);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_FRAME_QP, &qp, sizeof(qp));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_aspect_ratio(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_aspect_ratio sar;
	u32 codec;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	codec = get_v4l2_codec(inst);
	if (!(codec == V4L2_PIX_FMT_H264 || codec == V4L2_PIX_FMT_HEVC))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH);
	if (!ctrl->val)
		return 0;
	sar.aspect_width = ctrl->val;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT);
	if (!ctrl->val)
		return 0;
	sar.aspect_height = ctrl->val;

	s_vpr_h(inst->sid, "%s: %d %d\n", __func__,
		sar.aspect_width, sar.aspect_height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_ASPECT_RATIO, &sar, sizeof(sar));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_blur_resolution(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl = NULL;
	struct hfi_frame_size frame_sz;
	struct v4l2_format *f;
	bool disable_blur = false;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS);

	frame_sz.buffer_type = HFI_BUFFER_INPUT;
	frame_sz.height = ctrl->val & 0xFFFF;
	frame_sz.width = (ctrl->val & 0x7FFF0000) >> 16;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;

	/*
	 * 0x0 is default value, internal blur enabled, external blur disabled
	 * 0x1 means dynamic external blur, blur resolution will be set
	 *     after start, internal blur disabled
	 * 0x2 means disable both internal and external blur
	 */
	if (ctrl->val == MSM_VIDC_BLUR_DISABLE) {
		s_vpr_h(inst->sid,
			"Disable internal/external blur\n");
		disable_blur = true;
	} else if (ctrl->val == MSM_VIDC_BLUR_INTERNAL) {
		if (check_blur_restrictions(inst)) {
			s_vpr_h(inst->sid,
				"Internal blur restrictions not met. Disabling blur..\n");
			disable_blur = true;
		}
	} else {
		if (check_blur_restrictions(inst)) {
			s_vpr_e(inst->sid,
				"External blur is unsupported with rotation/flip/scalar\n");
			disable_blur = true;
		} else if (frame_sz.width > f->fmt.pix_mp.width ||
			frame_sz.height > f->fmt.pix_mp.height) {
			s_vpr_e(inst->sid,
				"external blur wxh[%ux%u] exceeds input wxh[%ux%u]\n",
				frame_sz.width, frame_sz.height,
				f->fmt.pix_mp.width, f->fmt.pix_mp.height);
			disable_blur = true;
		}
		if (inst->state < MSM_VIDC_START_DONE && disable_blur)
			inst->external_blur = false;
	}

	if (disable_blur) {
		/*
		 * Use original input width/height (before VPSS) to inform FW
		 * to disable all blur.
		 */
		frame_sz.width = f->fmt.pix_mp.width;
		frame_sz.height = f->fmt.pix_mp.height;
	}

	s_vpr_h(inst->sid, "%s: type %u, height %u, width %u\n", __func__,
		frame_sz.buffer_type, frame_sz.height, frame_sz.width);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_VENC_BLUR_FRAME_SIZE, &frame_sz,
		sizeof(frame_sz));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_hdr_info(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *profile = NULL;
	struct hfi_device *hdev;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (get_v4l2_codec(inst) != V4L2_PIX_FMT_HEVC)
		return 0;

	profile = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE);
	if (profile->val != V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10)
		return 0;

	/* No conversion to HFI needed as both structures are same */
	s_vpr_h(inst->sid, "%s: setting hdr info\n", __func__);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VENC_HDR10_PQ_SEI, &inst->hdr10_sei_params,
		sizeof(inst->hdr10_sei_params));
	if (rc)
		s_vpr_e(inst->sid, "%s: set property failed\n", __func__);

	return rc;
}

int msm_venc_set_extradata(struct msm_vidc_inst *inst)
{
	int rc = 0;
	u32 codec;

	codec = get_v4l2_codec(inst);
	if (inst->prop.extradata_ctrls == EXTRADATA_NONE) {
		// Disable all Extradata
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_LTR_INFO, 0x0);
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_ROI_QP_EXTRADATA, 0x0);
		if (codec == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_HDR10PLUS_METADATA_EXTRADATA,
			0x0);
		}
	}

	if (inst->prop.extradata_ctrls & EXTRADATA_ADVANCED)
		// Enable Advanced Extradata - LTR Info
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_LTR_INFO, 0x1);

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_FRAME_QP)
		// Enable AvgQP Extradata
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_FRAME_QP_EXTRADATA, 0x1);

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_ROI)
		// Enable ROIQP Extradata
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_ROI_QP_EXTRADATA, 0x1);

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_HDR10PLUS) {
		// Enable HDR10+ Extradata
		if (codec == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VENC_HDR10PLUS_METADATA_EXTRADATA,
			0x1);
		}
	}

	if(!msm_vidc_cvp_usage)
		inst->prop.extradata_ctrls &= ~EXTRADATA_ENC_INPUT_CVP;

	/* CVP extradata is common between user space and external CVP kernel to kernel.
	   Hence, skipping here and will be set after msm_vidc_prepare_preprocess in start_streaming*/

	return rc;
}

int msm_venc_set_lossless(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_enable enable;

	hdev = inst->core->device;

	if (inst->rc_type != RATE_CONTROL_LOSSLESS)
		return 0;

	s_vpr_h(inst->sid, "%s: enable lossless encoding\n", __func__);
	enable.enable = 1;
	rc = call_hfi_op(hdev, session_set_property,
		inst->session,
		HFI_PROPERTY_PARAM_VENC_LOSSLESS_ENCODING,
		&enable, sizeof(enable));

	if (rc)
		s_vpr_e(inst->sid, "Failed to set lossless mode\n");

	return rc;
}
int msm_venc_set_cvp_skipratio(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_ctrl *capture_rate_ctrl;
	struct v4l2_ctrl *cvp_rate_ctrl;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	if (!is_cvp_supported(inst)) {
		s_vpr_h(inst->sid, "%s cvp is not supported", __func__);
		return rc;
	}

	if (!msm_vidc_cvp_usage)
		return 0;

	capture_rate_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_CAPTURE_FRAME_RATE);
	cvp_rate_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_CVP_FRAME_RATE);

	rc = msm_comm_set_cvp_skip_ratio(inst,
			capture_rate_ctrl->val, cvp_rate_ctrl->val);
	if (rc)
		s_vpr_e(inst->sid, "Failed to set cvp skip ratio\n");

	return rc;
}

int msm_venc_update_entropy_mode(struct msm_vidc_inst *inst)
{
	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (get_v4l2_codec(inst) == V4L2_PIX_FMT_H264) {
		if ((inst->profile == HFI_H264_PROFILE_BASELINE ||
			inst->profile == HFI_H264_PROFILE_CONSTRAINED_BASE)
			&& inst->entropy_mode == HFI_H264_ENTROPY_CABAC) {
			inst->entropy_mode = HFI_H264_ENTROPY_CAVLC;
			s_vpr_h(inst->sid,
				"%s: profile %d entropy %d\n",
				__func__, inst->profile,
				inst->entropy_mode);
		}
	}

	return 0;
}

int handle_all_intra_restrictions(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *ctrl = NULL;
	u32 n_fps, fps_max;
	struct msm_vidc_capability *capability;
	struct v4l2_format *f;
	enum hal_video_codec codec;
	struct hfi_intra_period intra_period;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ)
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_GOP_SIZE);
	intra_period.pframes = ctrl->val;
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	intra_period.bframes = ctrl->val;

	if (!intra_period.pframes && !intra_period.bframes)
		inst->all_intra = true;
	else
		return 0;

	s_vpr_h(inst->sid, "All Intra(IDRs) Encoding\n");
	/* check codec and profile */
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	codec = get_hal_codec(f->fmt.pix_mp.pixelformat, inst->sid);
	if (codec != HAL_VIDEO_CODEC_HEVC && codec != HAL_VIDEO_CODEC_H264) {
		s_vpr_e(inst->sid, "Unsupported codec for all intra\n");
		return -ENOTSUPP;
	}
	if (codec == HAL_VIDEO_CODEC_HEVC &&
		inst->profile == HFI_HEVC_PROFILE_MAIN10) {
		s_vpr_e(inst->sid, "Unsupported HEVC profile for all intra\n");
		return -ENOTSUPP;
	}

	/* CBR_CFR is one of the advertised rc mode for HEVC encoding.
	 * However, all-intra is intended for quality bitstream. Hence,
	 * fallback to VBR RC mode if client needs all-intra encoding.
	 */
	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR)
		inst->rc_type = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;

	/* check supported bit rate mode and frame rate */
	capability = &inst->capability;
	n_fps = inst->clk_data.frame_rate >> 16;
	fps_max = capability->cap[CAP_ALLINTRA_MAX_FPS].max;
	s_vpr_h(inst->sid, "%s: rc_type %u, fps %u, fps_max %u\n",
		__func__, inst->rc_type, n_fps, fps_max);
	if ((inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR &&
		inst->rc_type != RATE_CONTROL_OFF &&
		inst->rc_type != RATE_CONTROL_LOSSLESS) ||
		n_fps > fps_max) {
		s_vpr_e(inst->sid, "Unsupported bitrate mode or frame rate\n");
		return -ENOTSUPP;
	}

	set_all_intra_preconditions(inst);

	return 0;
}

int check_blur_restrictions(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *cac = NULL;
	struct v4l2_ctrl *profile = NULL;
	struct v4l2_ctrl *blur = NULL;
	struct v4l2_format *f;
	bool scalar_enable = false;
	bool sup_resolution = false;
	bool sup_codec = false;
	bool is_10_bit = false;
	u32 input_height, input_width;
	u32 codec;

	/* Only need to check static VPSS conditions */
	if (inst->state == MSM_VIDC_START_DONE)
		return 0;

	scalar_enable = vidc_scalar_enabled(inst);
	blur = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS);
	/* Minimum restrictions to enable any type of blur */
	if (scalar_enable || inst->static_rotation_flip_enabled) {
		return -ENOTSUPP;
	}
	if (blur->val != MSM_VIDC_BLUR_INTERNAL) {
		/* below restrictions applicable for internal blur only */
		return 0;
	}

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	input_height = f->fmt.pix_mp.height;
	input_width = f->fmt.pix_mp.width;

	/* Adaptive blur restrictions */
	cac = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VENC_BITRATE_SAVINGS);
	codec = get_v4l2_codec(inst);
	profile = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_PROFILE);

	if (codec == V4L2_PIX_FMT_HEVC &&
		profile->val == V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10) {
		is_10_bit = true;
	}
	if (res_is_greater_than(input_width, input_height, 352, 240) &&
		res_is_less_than_or_equal_to(input_width, input_height,
		3840, 2160)) {
		sup_resolution = true;
	}

	if (codec == V4L2_PIX_FMT_HEVC || codec == V4L2_PIX_FMT_H264)
		sup_codec = true;

	if (inst->rc_type != V4L2_MPEG_VIDEO_BITRATE_MODE_VBR ||
		!cac->val || is_10_bit || !sup_codec || inst->all_intra ||
		!sup_resolution) {
		return -ENOTSUPP;
	}

	return 0;
}

int handle_vpss_restrictions(struct msm_vidc_inst *inst)
{
	struct v4l2_ctrl *rotation = NULL;
	struct v4l2_ctrl *hflip = NULL;
	struct v4l2_ctrl *vflip = NULL;
	struct v4l2_format *f;
	struct msm_vidc_vpss_capability *vpss_caps;
	u32 vpss_caps_count;
	bool rotation_flip_enable = false;
	u32 i,input_height, input_width;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	input_height = f->fmt.pix_mp.height;
	input_width = f->fmt.pix_mp.width;

	vpss_caps = inst->core->resources.vpss_caps;
	vpss_caps_count = inst->core->resources.vpss_caps_count;

	/* check customer specified VPSS resolutions */
	if (vpss_caps) {
		for (i = 0; i < vpss_caps_count; i++) {
			if (input_width == vpss_caps[i].width &&
				input_height == vpss_caps[i].height) {
				s_vpr_h(inst->sid,
					"supported resolution found for VPSS, width = %d, height = %d\n",
					input_width, input_height);
				return 0;
			}
		}
	}

	/* check rotation and flip contraint for VPSS
	 * any rotation or flip sessions with non-multiple of 8
	 * resolution is rejected.
	 */
	rotation = get_ctrl(inst, V4L2_CID_ROTATE);
	hflip = get_ctrl(inst, V4L2_CID_HFLIP);
	vflip = get_ctrl(inst, V4L2_CID_VFLIP);
	if (rotation->val != 0 ||
		hflip->val != V4L2_MPEG_MSM_VIDC_DISABLE ||
		vflip->val != V4L2_MPEG_MSM_VIDC_DISABLE)
		rotation_flip_enable = true;

	if (rotation_flip_enable) {
		if ((input_width & 7) != 0) {
			s_vpr_e(inst->sid, "Unsupported width = %d for VPSS\n",
				input_width);
			return -ENOTSUPP;
		}
		if ((input_height & 7) != 0) {
			s_vpr_e(inst->sid, "Unsupported height = %d for VPSS\n",
				input_height);
			return -ENOTSUPP;
		}
	}
	return 0;
}

int msm_venc_set_properties(struct msm_vidc_inst *inst)
{
	int rc = 0;

	rc = msm_venc_update_entropy_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_update_bitrate(inst);
	if (rc)
		goto exit;
	rc = handle_all_intra_restrictions(inst);
	if (rc)
		goto exit;
	rc = handle_vpss_restrictions(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_frame_size(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_frame_rate(inst, true);
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
	rc = msm_venc_set_entropy_mode(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_rate_control(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_vbv_delay(inst);
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
	rc = msm_venc_set_image_properties(inst);
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
	rc = msm_venc_set_hb_max_layer(inst);
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
	/*
	 * Downscalar and Static rotation/flip has higher priority
	 * than blur.
	 */
	rc = msm_venc_set_rotation(inst);
	if (rc)
		goto exit;
	rc = msm_venc_set_chroma_qp_offset(inst);
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
	rc = msm_venc_set_lossless(inst);
	if (rc)
		goto exit;

exit:
	if (rc)
		s_vpr_e(inst->sid, "%s: failed with %d\n", __func__, rc);
	else
		s_vpr_h(inst->sid, "%s: set properties successful\n", __func__);

	return rc;
}
