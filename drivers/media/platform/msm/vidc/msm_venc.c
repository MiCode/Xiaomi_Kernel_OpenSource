// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */
#include <linux/slab.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"

#define MSM_VENC_DVC_NAME "msm_venc_8974"
#define MIN_BIT_RATE 32000
#define MAX_BIT_RATE 300000000
#define DEFAULT_BIT_RATE 64000
#define BIT_RATE_STEP 1
#define DEFAULT_FRAME_RATE 15
#define OPERATING_FRAME_RATE_STEP (1 << 16)
#define MAX_SLICE_BYTE_SIZE ((MAX_BIT_RATE)>>3)
#define MIN_SLICE_BYTE_SIZE 512
#define MAX_SLICE_MB_SIZE ((4096 * 2304) >> 8)
#define I_FRAME_QP 127
#define P_FRAME_QP 127
#define B_FRAME_QP 127
#define QP_ENABLE_I 0x1
#define QP_ENABLE_P 0x2
#define QP_ENABLE_B 0x4
#define MAX_INTRA_REFRESH_MBS ((4096 * 2304) >> 8)
#define MAX_LTR_FRAME_COUNT 10
#define MAX_HYBRID_HIER_P_LAYERS 6

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

static const char *const hevc_level[] = {
	"Level 1",
	"Level 2",
	"Level 2.1",
	"Level 3",
	"Level 3.1",
	"Level 4",
	"Level 4.1",
	"Level 5",
	"Level 5.1",
	"Level 5.2",
	"Level 6",
	"Level 6.1",
	"Level 6.2",
	"Level unknown",
	NULL
};

static const char *const tme_profile[] = {
	"0",
	"1",
	"2",
	"3",
};

static const char *const tme_level[] = {
	"Integer",
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
		.maximum = I_FRAME_QP,
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
		.maximum = I_FRAME_QP,
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
		.maximum = I_FRAME_QP,
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
		.maximum = I_FRAME_QP,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP,
		.name = "HEVC Quantization Range Maximum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = I_FRAME_QP,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_B_FRAMES,
		.name = "Intra Period for B frames",
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
		.name = "Video Framerate and Bitrate Control",
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
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.name = "H264 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN,
		.menu_skip_mask = 0,
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
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
		.name = "HEVC Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		.maximum = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10,
		.default_value = V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN,
		.menu_skip_mask =  0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
		.name = "HEVC Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_LEVEL_1,
		.maximum = V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN,
		.default_value =
			V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_HEVC_TIER,
		.name = "HEVC Tier",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEVC_TIER_MAIN,
		.maximum = V4L2_MPEG_VIDEO_HEVC_TIER_HIGH,
		.default_value = V4L2_MPEG_VIDEO_HEVC_TIER_MAIN,
		.menu_skip_mask = 0,
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
		.type = V4L2_CTRL_TYPE_BITMASK,
		.minimum = 0,
		.maximum = 0xF,
		.default_value = 0,
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
		.minimum = 1,
		.maximum = 4096,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT,
		.name = "SAR Height",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 2160,
		.default_value = 1,
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
		.default_value = 0,
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
		.minimum = MSM_VIDC_BT709_5,
		.maximum = MSM_VIDC_BT2020,
		.default_value = MSM_VIDC_BT601_6_625,
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
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
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

int msm_venc_set_extradata(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_buffer_requirements *buff_req_buffer = NULL;
	int extra_idx = 0;

	if (!ctrl->val) {
		// Disable all Extradata
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ASPECT_RATIO, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_LTR_INFO, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ROI_QP, 0x0);
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
					HAL_EXTRADATA_HDR10PLUS_METADATA, 0x0);
		}
		inst->bufq[OUTPUT_PORT].num_planes = 1;
		inst->bufq[CAPTURE_PORT].num_planes = 1;
	}
	if (ctrl->val & 0x2) {
		// Enable Advanced Extradata
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ASPECT_RATIO, 0x1);
		inst->bufq[OUTPUT_PORT].num_planes = 2;
		msm_comm_set_extradata(inst, HAL_EXTRADATA_LTR_INFO, 0x1);
		inst->bufq[CAPTURE_PORT].num_planes = 2;
	}
	if (ctrl->val & 0x4) {
		// Enable ROIQP Extradata
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ROI_QP, 0x1);
		inst->bufq[OUTPUT_PORT].num_planes = 2;
	}
	if (ctrl->val & 0x8) {
		// Enable HDR10+ Extradata
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
					HAL_EXTRADATA_HDR10PLUS_METADATA, 0x1);
		}
		inst->bufq[OUTPUT_PORT].num_planes = 2;
	}

	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to get buffer requirements: %d\n", rc);
		return rc;
	}

	extra_idx = EXTRADATA_IDX(inst->bufq[OUTPUT_PORT].num_planes);
	if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
		buff_req_buffer = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_INPUT);

		inst->bufq[OUTPUT_PORT].plane_sizes[extra_idx] =
				buff_req_buffer ?
				buff_req_buffer->buffer_size : 0;
	}

	extra_idx = EXTRADATA_IDX(inst->bufq[CAPTURE_PORT].num_planes);
	if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
		buff_req_buffer = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);

		inst->bufq[CAPTURE_PORT].plane_sizes[extra_idx] =
			buff_req_buffer ?
			buff_req_buffer->buffer_size : 0;
	}

	return rc;
}

int msm_venc_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_request_iframe request_iframe;
	struct hal_bitrate bitrate;
	struct hal_operating_rate operating_rate;
	struct hal_profile_level profile_level;
	enum hal_h264_entropy h264_entropy;
	struct hal_intra_period intra_period;
	struct hal_intra_refresh intra_refresh;
	struct hal_multi_slice_control multi_slice_control;
	struct hal_h264_db_control h264_db_control;
	struct hal_enable enable;
	struct hal_quantization quant;
	struct hal_quantization_range qp_range;
	struct hal_aspect_ratio sar;
	struct hal_frame_size blur_res;
	u32 property_id = 0, property_val = 0;
	void *pdata = NULL;
	struct v4l2_ctrl *temp_ctrl = NULL;
	struct hfi_device *hdev;
	struct hal_ltr_use use_ltr;
	struct hal_ltr_mark mark_ltr;
	int baselayerid = 0;
	struct hal_video_signal_info signal_info = {0};
	struct hal_vui_timing_info vui_timing_info = {0};
	u32 color_primaries, custom_matrix;
	struct hal_nal_stream_format_select stream_format;
	struct hal_heic_frame_quality frame_quality;
	struct hal_heic_grid_enable grid_enable;
	struct hal_ltr_mode ltr_mode;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	/*
	 * Unlock the control prior to setting to the hardware. Otherwise
	 * lower level code that attempts to do a get_ctrl() will end up
	 * deadlocking.
	 */
	v4l2_ctrl_unlock(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
	{
		unsigned int num_p, num_b;

		temp_ctrl = try_get_ctrl(V4L2_CID_MPEG_VIDEO_B_FRAMES, ctrl);
		num_b = temp_ctrl->val;

		temp_ctrl = try_get_ctrl(V4L2_CID_MPEG_VIDEO_GOP_SIZE, ctrl);
		num_p = temp_ctrl->val;

		if (ctrl->id == V4L2_CID_MPEG_VIDEO_GOP_SIZE)
			num_p = ctrl->val;
		else if (ctrl->id == V4L2_CID_MPEG_VIDEO_B_FRAMES)
			num_b = ctrl->val;

		if ((num_b < inst->capability.bframe.min) ||
			(num_b > inst->capability.bframe.max)) {
			dprintk(VIDC_ERR,
				"Error setting num b frames %d min, max supported is %d, %d\n",
				num_b, inst->capability.bframe.min,
				inst->capability.bframe.max);
			rc = -ENOTSUPP;
			break;
		}

		property_id = HAL_CONFIG_VENC_INTRA_PERIOD;
		intra_period.pframes = num_p;
		intra_period.bframes = num_b;

		pdata = &intra_period;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME:
		property_id = HAL_CONFIG_VENC_REQUEST_IFRAME;
		request_iframe.enable = true;
		pdata = &request_iframe;
		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		property_id = HAL_PARAM_VENC_RATE_CONTROL;
		property_val = HAL_RATE_CONTROL_OFF;
		pdata = &property_val;
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
		property_id = HAL_PARAM_VENC_RATE_CONTROL;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	}
	case V4L2_CID_MPEG_VIDC_COMPRESSION_QUALITY:
	{
		if (ctrl->val < MIN_FRAME_QUALITY ||
			ctrl->val > MAX_FRAME_QUALITY) {
			dprintk(VIDC_ERR,
				"Frame quality value %d is not supported\n",
				ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		property_id = HAL_CONFIG_HEIC_FRAME_QUALITY;
		frame_quality.frame_quality = ctrl->val;
		inst->frame_quality = ctrl->val;
		pdata = &frame_quality;
		break;
	}
	case V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE:
	{
		property_id = HAL_CONFIG_HEIC_GRID_ENABLE;
		grid_enable.grid_enable = ctrl->val;
		inst->grid_enable = ctrl->val;
		pdata = &grid_enable;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_BITRATE:
	{
		property_id = HAL_CONFIG_VENC_TARGET_BITRATE;
		bitrate.bit_rate = ctrl->val;
		bitrate.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &bitrate;
		inst->clk_data.bitrate = ctrl->val;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		property_id = HAL_PARAM_VENC_H264_ENTROPY_CONTROL;
		h264_entropy = msm_comm_v4l2_to_hal(
			V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE, ctrl->val);
		pdata = &h264_entropy;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		temp_ctrl = try_get_ctrl(V4L2_CID_MPEG_VIDEO_H264_LEVEL, ctrl);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.level = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		dprintk(VIDC_DBG, "\nprofile: %d\n",
			   profile_level.profile);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		temp_ctrl =
			try_get_ctrl(V4L2_CID_MPEG_VIDEO_H264_PROFILE, ctrl);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.profile = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		dprintk(VIDC_DBG, "\nLevel: %d\n",
			   profile_level.level);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = HAL_VP8_PROFILE_MAIN;
		profile_level.level = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
				ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE: {
		struct v4l2_ctrl *hevc_tier = try_get_ctrl(
			V4L2_CID_MPEG_VIDEO_HEVC_TIER, ctrl);
		temp_ctrl =
			try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_LEVEL, ctrl);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.level = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
				temp_ctrl->val);

		if (profile_level.level != HAL_HEVC_LEVEL_UNKNOWN)
			profile_level.level |= (msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_HEVC_TIER,
				hevc_tier->val) << 28);

		pdata = &profile_level;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL: {
		struct v4l2_ctrl *hevc_tier = try_get_ctrl(
			V4L2_CID_MPEG_VIDEO_HEVC_TIER, ctrl);
		temp_ctrl =
			try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_PROFILE, ctrl);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		if (profile_level.level != HAL_HEVC_LEVEL_UNKNOWN)
			profile_level.level |= (msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_HEVC_TIER,
				hevc_tier->val) << 28);

		profile_level.profile = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER: {
		struct v4l2_ctrl *hevc_profile = try_get_ctrl(
			V4L2_CID_MPEG_VIDEO_HEVC_PROFILE, ctrl);
		temp_ctrl = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_LEVEL, ctrl);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
				temp_ctrl->val);

		if (profile_level.level != HAL_HEVC_LEVEL_UNKNOWN)
			profile_level.level |= (msm_comm_v4l2_to_hal(
				ctrl->id,
				ctrl->val) << 28);

		profile_level.profile = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
				hevc_profile->val);
		pdata = &profile_level;
		break;
	}
	case V4L2_CID_ROTATE:
	{
		if (ctrl->val != 0 && ctrl->val != 90
			&& ctrl->val != 180 && ctrl->val != 270) {
			dprintk(VIDC_ERR, "Invalid rotation angle");
			rc = -ENOTSUPP;
		}
		dprintk(VIDC_DBG, "Rotation %d\n", ctrl->val);
		break;
	}
	case V4L2_CID_HFLIP:
		dprintk(VIDC_DBG, "Horizontal Flip %d\n", ctrl->val);
		break;
	case V4L2_CID_VFLIP:
		dprintk(VIDC_DBG, "Vertical Flip %d\n", ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE: {
		int temp = 0;

		if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC &&
			inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264) {
			return rc;
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

		if (temp)
			temp_ctrl = try_get_ctrl(temp, ctrl);

		property_id = HAL_PARAM_VENC_MULTI_SLICE_CONTROL;
		multi_slice_control.multi_slice = ctrl->val;
		multi_slice_control.slice_size = temp ? temp_ctrl->val : 0;

		pdata = &multi_slice_control;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC &&
			inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264) {
			return rc;
		}
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE, ctrl);

		property_id = HAL_PARAM_VENC_MULTI_SLICE_CONTROL;
		multi_slice_control.multi_slice = temp_ctrl->val;
		multi_slice_control.slice_size = ctrl->val;
		pdata = &multi_slice_control;
		break;
	case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB:
	{
		property_id = HAL_PARAM_VENC_INTRA_REFRESH;

		intra_refresh.mode   = HAL_INTRA_REFRESH_CYCLIC;
		intra_refresh.ir_mbs = ctrl->val;

		pdata = &intra_refresh;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM:
	{
		property_id = HAL_PARAM_VENC_INTRA_REFRESH;

		intra_refresh.mode   = HAL_INTRA_REFRESH_RANDOM;
		intra_refresh.ir_mbs = ctrl->val;

		pdata = &intra_refresh;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
	{
		struct v4l2_ctrl *alpha, *beta;

		alpha = try_get_ctrl(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA,
				ctrl);
		beta = try_get_ctrl(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA,
				ctrl);

		property_id = HAL_PARAM_VENC_H264_DEBLOCK_CONTROL;
		h264_db_control.slice_alpha_offset = alpha->val;
		h264_db_control.slice_beta_offset = beta->val;
		h264_db_control.mode = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
				ctrl->val);
		pdata = &h264_db_control;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:
	{
		struct v4l2_ctrl *mode, *beta;

		mode = try_get_ctrl(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
				ctrl);
		beta = try_get_ctrl(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA,
				ctrl);

		property_id = HAL_PARAM_VENC_H264_DEBLOCK_CONTROL;
		h264_db_control.slice_alpha_offset = ctrl->val;
		h264_db_control.slice_beta_offset = beta->val;
		h264_db_control.mode = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
				mode->val);
		pdata = &h264_db_control;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:
	{
		struct v4l2_ctrl *mode, *alpha;

		mode = try_get_ctrl(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
				ctrl);
		alpha = try_get_ctrl(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA,
				ctrl);
		property_id = HAL_PARAM_VENC_H264_DEBLOCK_CONTROL;
		h264_db_control.slice_alpha_offset = alpha->val;
		h264_db_control.slice_beta_offset = ctrl->val;
		h264_db_control.mode = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
				mode->val);
		pdata = &h264_db_control;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_PREPEND_SPSPPS_TO_IDR:
		property_id = HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags |= VIDC_SECURE;
		property_id = HAL_PARAM_SECURE;
		property_val = !!(inst->flags & VIDC_SECURE);
		pdata = &property_val;
		dprintk(VIDC_INFO, "Setting secure mode to: %d\n",
				!!(inst->flags & VIDC_SECURE));
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		rc = msm_venc_set_extradata(inst, ctrl);
		return rc;
	case V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER:
		property_id = HAL_PARAM_VENC_GENERATE_AUDNAL;

		switch (ctrl->val) {
		case V4L2_MPEG_MSM_VIDC_DISABLE:
			enable.enable = false;
			break;
		case V4L2_MPEG_MSM_VIDC_ENABLE:
			enable.enable = true;
			break;
		default:
			rc = -ENOTSUPP;
			break;
		}

		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME:
		property_id = HAL_CONFIG_VENC_USELTRFRAME;
		use_ltr.ref_ltr = ctrl->val;
		use_ltr.use_constraint = true;
		use_ltr.frames = 0;
		pdata = &use_ltr;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME:
		property_id = HAL_CONFIG_VENC_MARKLTRFRAME;
		mark_ltr.mark_frame = ctrl->val;
		pdata = &mark_ltr;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER:
		dprintk(VIDC_DBG, "Hier coding num layers  : ", ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE:
		dprintk(VIDC_DBG, "Hier coding type  : ", ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_QP:
		dprintk(VIDC_DBG, "Hier layer QP  : ", ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L0_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L1_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L2_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L3_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L4_BR:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_L5_BR:
		dprintk(VIDC_DBG, "Hier layer BR  : ", ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE:
		property_id = HAL_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID:
		property_id = HAL_CONFIG_VENC_BASELAYER_PRIORITYID;
		baselayerid = ctrl->val;
		pdata = &baselayerid;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP: {
		struct v4l2_ctrl *qpp, *qpb;

		quant.enable = QP_ENABLE_I;
		property_id = HAL_CONFIG_VENC_FRAME_QP;
		qpp = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP, ctrl);
		if (qpp->val)
			quant.enable |= QP_ENABLE_P;

		qpb = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP, ctrl);
		if (qpb->val)
			quant.enable |= QP_ENABLE_B;

		quant.qpi = ctrl->val;
		quant.qpp = qpp->val;
		quant.qpb = qpb->val;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP: {
		struct v4l2_ctrl *qpi, *qpb;

		quant.enable = QP_ENABLE_P;
		property_id = HAL_CONFIG_VENC_FRAME_QP;
		qpi = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP, ctrl);
		if (qpi->val)
			quant.enable |= QP_ENABLE_I;

		qpb = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP, ctrl);
		if (qpb->val)
			quant.enable |= QP_ENABLE_B;

		quant.qpp = ctrl->val;
		quant.qpi = qpi->val;
		quant.qpb = qpb->val;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP: {
		struct v4l2_ctrl *qpp, *qpi;

		quant.enable = QP_ENABLE_B;
		property_id = HAL_CONFIG_VENC_FRAME_QP;
		qpp = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP, ctrl);
		if (qpp->val)
			quant.enable |= QP_ENABLE_P;

		qpi = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP, ctrl);
		if (qpi->val)
			quant.enable |= QP_ENABLE_I;

		quant.qpb = ctrl->val;
		quant.qpp = qpp->val;
		quant.qpi = qpi->val;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP: {
		struct v4l2_ctrl *qpi_max;

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qpi_max = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP, ctrl);

		qp_range.qpi_min = ctrl->val;
		qp_range.qpp_min = ctrl->val;
		qp_range.qpb_min = ctrl->val;
		qp_range.qpi_max = qpi_max->val;
		qp_range.qpp_max = qpi_max->val;
		qp_range.qpb_max = qpi_max->val;
		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP: {
		struct v4l2_ctrl *qpi_min;

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qpi_min = try_get_ctrl(V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP, ctrl);

		qp_range.qpi_min = qpi_min->val;
		qp_range.qpp_min = qpi_min->val;
		qp_range.qpb_min = qpi_min->val;
		qp_range.qpi_max = ctrl->val;
		qp_range.qpp_max = ctrl->val;
		qp_range.qpb_max = ctrl->val;
		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_DYN_QP: {
		property_id = HAL_CONFIG_VENC_FRAME_QP;

		quant.qpi = ctrl->val;
		quant.qpp = ctrl->val;
		quant.qpb = ctrl->val;
		quant.enable = QP_ENABLE_I | QP_ENABLE_P | QP_ENABLE_B;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH: {
		struct v4l2_ctrl *sar_height;

		property_id = HAL_PROPERTY_PARAM_VENC_ASPECT_RATIO;
		sar_height = try_get_ctrl(
			V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT, ctrl);

		sar.aspect_width = ctrl->val;
		sar.aspect_height = sar_height->val;
		pdata = &sar;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT: {
		struct v4l2_ctrl *sar_width;

		property_id = HAL_PROPERTY_PARAM_VENC_ASPECT_RATIO;
		sar_width = try_get_ctrl(
			V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH, ctrl);

		sar.aspect_width = sar_width->val;
		sar.aspect_height = ctrl->val;
		pdata = &sar;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_DIMENSIONS: {
		property_id = HAL_CONFIG_VENC_BLUR_RESOLUTION;

		blur_res.width = (0xFFFF & ctrl->val);
		blur_res.height = (0xFFFF0000 & ctrl->val) >> 16;
		blur_res.buffer_type = HAL_BUFFER_INPUT;
		pdata = &sar;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
		property_id = HAL_CONFIG_REALTIME;
		enable.enable = ctrl->val;
		pdata = &enable;
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
		if (((ctrl->val >> 16) <
			(int) (inst->capability.frame_rate.min) ||
			(ctrl->val >> 16) >
			(int) (inst->capability.frame_rate.max)) &&
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
			property_id = HAL_CONFIG_OPERATING_RATE;
			operating_rate.operating_rate =
				inst->clk_data.operating_rate;
			pdata = &operating_rate;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE:
	{
		signal_info.color_space = ctrl->val;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE, ctrl);
		signal_info.full_range = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS, ctrl);
		signal_info.transfer_chars = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS, ctrl);
		signal_info.matrix_coeffs = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE:
	{
		signal_info.full_range = ctrl->val;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE, ctrl);
		signal_info.color_space = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS, ctrl);
		signal_info.transfer_chars = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS, ctrl);
		signal_info.matrix_coeffs = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS:
	{
		signal_info.transfer_chars = ctrl->val;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE, ctrl);
		signal_info.full_range = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE, ctrl);
		signal_info.color_space = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS, ctrl);
		signal_info.matrix_coeffs = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS:
	{
		signal_info.matrix_coeffs = ctrl->val;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE, ctrl);
		signal_info.full_range = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS, ctrl);
		signal_info.transfer_chars = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE, ctrl);
		signal_info.color_space = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC:
		if (ctrl->val != V4L2_MPEG_MSM_VIDC_ENABLE)
			break;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE, ctrl);
		color_primaries = temp_ctrl->val;
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX, ctrl);
		custom_matrix = temp_ctrl->val;
		rc = msm_venc_set_csc(inst, color_primaries, custom_matrix);
		if (rc)
			dprintk(VIDC_ERR, "fail to set csc: %d\n", rc);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX:
		temp_ctrl = try_get_ctrl(
			V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE, ctrl);
		color_primaries = temp_ctrl->val;
		rc = msm_venc_set_csc(inst, color_primaries, ctrl->val);
		if (rc)
			dprintk(VIDC_ERR, "fail to set csc: %d\n", rc);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
	{
		property_id = HAL_PARAM_VENC_LOW_LATENCY;
		if (ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE)
			enable.enable = true;
		else
			enable.enable = false;
		pdata = &enable;
		inst->clk_data.low_latency_mode = (bool) enable.enable;
		msm_dcvs_try_enable(inst);
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
		property_id = HAL_PARAM_VENC_H264_TRANSFORM_8x8;
		switch (ctrl->val) {
		case V4L2_MPEG_MSM_VIDC_ENABLE:
			enable.enable = true;
			break;
		case V4L2_MPEG_MSM_VIDC_DISABLE:
			enable.enable = false;
			break;
		default:
			dprintk(VIDC_ERR,
				"Invalid H264 8x8 transform control value %d\n",
				ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VUI_TIMING_INFO:
	{
		struct v4l2_ctrl *rc_mode;
		bool cfr = false;

		property_id = HAL_PARAM_VENC_VUI_TIMING_INFO;
		pdata = &vui_timing_info;

		if (ctrl->val != V4L2_MPEG_MSM_VIDC_ENABLE) {
			vui_timing_info.enable = 0;
			break;
		}

		rc_mode = try_get_ctrl(V4L2_CID_MPEG_VIDEO_BITRATE_MODE, ctrl);

		switch (rc_mode->val) {
		case V4L2_MPEG_VIDEO_BITRATE_MODE_VBR:
		case V4L2_MPEG_VIDEO_BITRATE_MODE_CBR:
		case V4L2_MPEG_VIDEO_BITRATE_MODE_MBR:
			cfr = true;
			break;
		default:
			cfr = false;
		}

		vui_timing_info.enable = 1;
		vui_timing_info.fixed_frame_rate = cfr;
		vui_timing_info.time_scale = NSEC_PER_SEC;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
	{
		property_id = HAL_PARAM_NAL_STREAM_FORMAT_SELECT;
		stream_format.nal_stream_format_select = BIT(ctrl->val);
		pdata = &stream_format;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT:
		ltr_mode.count =  ctrl->val;
		if (ltr_mode.count > inst->capability.ltr_count.max) {
			dprintk(VIDC_ERR,
				"Invalid LTR count %d. Supported max: %d\n",
				ltr_mode.count,
				inst->capability.ltr_count.max);
			rc = -EINVAL;
		}
		ltr_mode.mode = HAL_LTR_MODE_MANUAL;
		ltr_mode.trust_mode = 1;
		property_id = HAL_PARAM_VENC_LTRMODE;
		pdata = &ltr_mode;
		break;
	case V4L2_CID_MPEG_VIDC_VENC_HDR_INFO:
		dprintk(VIDC_DBG, "Set the control : %#x using ext ctrl\n",
			ctrl->id);
		break;
	default:
		dprintk(VIDC_ERR, "Unsupported index: %x\n", ctrl->id);
		rc = -ENOTSUPP;
		break;
	}

	v4l2_ctrl_lock(ctrl);

	if (!rc && property_id) {
		dprintk(VIDC_DBG,
			"Control: %x : Name = %s, ID = 0x%x Value = %d\n",
			hash32_ptr(inst->session), ctrl->name,
			ctrl->id, ctrl->val);
		rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session, property_id, pdata);
	}

	return rc;
}

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
	struct msm_vidc_format *fmt = NULL;
	struct msm_vidc_format_constraint *fmt_constraint = NULL;
	int rc = 0;
	struct hfi_device *hdev;
	int extra_idx = 0, i = 0;
	struct hal_buffer_requirements *buff_req_buffer;
	struct hal_frame_size frame_sz;

	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, format = %pK\n", inst, f);
		return -EINVAL;
	}

	if (!inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

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

		frame_sz.buffer_type = HAL_BUFFER_OUTPUT;
		frame_sz.width = inst->prop.width[CAPTURE_PORT];
		frame_sz.height = inst->prop.height[CAPTURE_PORT];
		dprintk(VIDC_DBG, "CAPTURE port width = %d, height = %d\n",
			frame_sz.width, frame_sz.height);
		rc = call_hfi_op(hdev, session_set_property, (void *)
			inst->session, HAL_PARAM_FRAME_SIZE, &frame_sz);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set framesize for CAPTURE port\n");
			goto exit;
		}

		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
			return rc;
		}

		/*
		 * Get CAPTURE plane size from HW. This may change based on
		 * settings like Slice delivery mode. HW should decide howmuch
		 * it needs.
		 */

		buff_req_buffer = get_buff_req_buffer(inst,
			HAL_BUFFER_OUTPUT);

		f->fmt.pix_mp.plane_fmt[0].sizeimage = buff_req_buffer ?
				buff_req_buffer->buffer_size : 0;

		/*
		 * Get CAPTURE plane Extradata size from HW. This may change
		 * with no of Extradata's enabled. HW should decide howmuch
		 * it needs.
		 */

		extra_idx = EXTRADATA_IDX(inst->bufq[fmt->type].num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			buff_req_buffer = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);
			f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
				buff_req_buffer ?
				buff_req_buffer->buffer_size : 0;
		}

		f->fmt.pix_mp.num_planes = inst->bufq[fmt->type].num_planes;
		for (i = 0; i < inst->bufq[fmt->type].num_planes; i++) {
			inst->bufq[fmt->type].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
		/*
		 * Input extradata buffer size may change upon updating
		 * CAPTURE plane buffer size.
		 */

		extra_idx = EXTRADATA_IDX(inst->bufq[OUTPUT_PORT].num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			buff_req_buffer = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_INPUT);
			inst->bufq[OUTPUT_PORT].plane_sizes[extra_idx] =
				buff_req_buffer ?
				buff_req_buffer->buffer_size : 0;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		struct hal_frame_size frame_sz;

		inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto exit;
		}

		frame_sz.buffer_type = HAL_BUFFER_INPUT;
		frame_sz.width = inst->prop.width[OUTPUT_PORT];
		frame_sz.height = inst->prop.height[OUTPUT_PORT];
		dprintk(VIDC_DBG, "OUTPUT port width = %d, height = %d\n",
				frame_sz.width, frame_sz.height);
		rc = call_hfi_op(hdev, session_set_property, (void *)
			inst->session, HAL_PARAM_FRAME_SIZE, &frame_sz);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set framesize for Output port\n");
			goto exit;
		}

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
		inst->clk_data.opb_fourcc = f->fmt.pix_mp.pixelformat;
		memcpy(&inst->fmts[fmt->type], fmt,
				sizeof(struct msm_vidc_format));

		f->fmt.pix_mp.plane_fmt[0].sizeimage =
			inst->fmts[fmt->type].get_frame_size(0,
			f->fmt.pix_mp.height, f->fmt.pix_mp.width);

		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
			return rc;
		}

		/*
		 * Get OUTPUT plane Extradata size from HW. This may change
		 * with no of Extradata's enabled. HW should decide howmuch
		 * it needs.
		 */

		extra_idx = EXTRADATA_IDX(inst->bufq[fmt->type].num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			buff_req_buffer = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_INPUT);
			f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
				buff_req_buffer ?
				buff_req_buffer->buffer_size : 0;
		}

		f->fmt.pix_mp.num_planes = inst->bufq[fmt->type].num_planes;

		for (i = 0; i < inst->bufq[fmt->type].num_planes; i++) {
			inst->bufq[fmt->type].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}

		msm_comm_set_color_format(inst, HAL_BUFFER_INPUT, fmt->fourcc);

		fmt_constraint =
		msm_comm_get_pixel_fmt_constraints(enc_pix_format_constraints,
			ARRAY_SIZE(enc_pix_format_constraints),
			f->fmt.pix_mp.pixelformat);

		if (!fmt_constraint) {
			dprintk(VIDC_INFO,
				"Format constraint not required for %d on OUTPUT port\n",
				f->fmt.pix_mp.pixelformat);
		} else {
			rc = msm_comm_set_color_format_constraints(inst,
				HAL_BUFFER_INPUT,
				fmt_constraint);
			if (rc) {
				dprintk(VIDC_ERR,
					"Set constraint for %d failed on CAPTURE port\n",
					f->fmt.pix_mp.pixelformat);
				rc = -EINVAL;
				goto exit;
			}
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
