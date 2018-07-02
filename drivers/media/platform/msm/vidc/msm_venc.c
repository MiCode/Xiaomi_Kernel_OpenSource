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
#define MAX_INTRA_REFRESH_MBS ((4096 * 2304) >> 8)
#define MAX_NUM_B_FRAMES 4
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
	"RC OFF",
	"CBR VFR",
	"MBR VFR",
	NULL
};

static const char *const mpeg_video_flip[] = {
	"No Flip",
	"Horizontal Flip",
	"Vertical Flip",
	"Both",
	NULL
};

static const char *const h264_video_entropy_cabac_model[] = {
	"Model 0",
	"Model 1",
	"Model 2",
	NULL
};

static const char *const hevc_tier_level[] = {
	"Main Tier Level 1",
	"Main Tier Level 2",
	"Main Tier Level 2.1",
	"Main Tier Level 3",
	"Main Tier Level 3.1",
	"Main Tier Level 4",
	"Main Tier Level 4.1",
	"Main Tier Level 5",
	"Main Tier Level 5.1",
	"Main Tier Level 5.2",
	"Main Tier Level 6",
	"Main Tier Level 6.1",
	"Main Tier Level 6.2",
	"High Tier Level 1",
	"High Tier Level 2",
	"High Tier Level 2.1",
	"High Tier Level 3",
	"High Tier Level 3.1",
	"High Tier Level 4",
	"High Tier Level 4.1",
	"High Tier Level 5",
	"High Tier Level 5.1",
	"High Tier Level 5.2",
	"High Tier Level 6",
	"High Tier Level 6.1",
	"High Tier Level 6.2",
	"Level unknown",
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

static const char *const hevc_profile[] = {
	"Main",
	"Main10",
	"Main Still Pic",
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

static const char *const iframe_sizes[] = {
	"Default",
	"Medium",
	"Huge",
	"Unlimited"
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD,
		.name = "IDR Period",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = INT_MAX,
		.default_value = DEFAULT_FRAME_RATE,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP,
		.name = "I Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP,
		.name = "P Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = P_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP,
		.name = "B Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = B_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MIN,
		.name = "I Frame Quantization Range Minimum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MIN,
		.name = "P Frame Quantization Range Minimum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = P_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MIN,
		.name = "B Frame Quantization Range Minimum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = B_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MAX,
		.name = "I Frame Quantization Range Maximum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MAX,
		.name = "P Frame Quantization Range Maximum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = P_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MAX,
		.name = "B Frame Quantization Range Maximum",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = B_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK,
		.name = "QP mask for diff frame types",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 7,
		.default_value = 7,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ADAPTIVE_B,
		.name = "Adaptive B frames",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_ENABLE,
		.step = 1,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME,
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
		.maximum = V4L2_MPEG_VIDEO_BITRATE_MODE_MBR_VFR,
		.default_value = V4L2_MPEG_VIDEO_BITRATE_MODE_RC_OFF,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_CBR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_MBR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_RC_OFF) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_MBR_VFR)
		),
		.qmenu = mpeg_video_rate_control,
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
		.id = V4L2_CID_MPEG_VIDEO_BITRATE_PEAK,
		.name = "Peak Bit Rate",
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE,
		.name = "HEVC Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN,
		.maximum = V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN_STILL_PIC,
		.default_value = V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN,
		.menu_skip_mask =  ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN10) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN_STILL_PIC)
		),
		.qmenu = hevc_profile,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL,
		.name = "HEVC Tier and Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_1,
		.maximum = V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_UNKNOWN,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_UNKNOWN,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_2_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_3) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_3_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_4) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_4_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5_2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_2_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_3) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_3_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_4) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_4_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_UNKNOWN)
		),
		.qmenu = hevc_tier_level,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_TME_PROFILE,
		.name = "TME Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_0,
		.maximum = V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_3,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_1) |
		(1 << V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_TME_PROFILE_3)
		),
		.qmenu = tme_profile,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_TME_LEVEL,
		.name = "TME Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_TME_LEVEL_INTEGER,
		.maximum = V4L2_MPEG_VIDC_VIDEO_TME_LEVEL_INTEGER,
		.default_value = V4L2_MPEG_VIDC_VIDEO_TME_LEVEL_INTEGER,
		.menu_skip_mask =  ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_TME_LEVEL_INTEGER)
		),
		.qmenu = tme_level,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_TME_PAYLOAD_VERSION,
		.name = "TME Payload Version",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 0xFFFFFFF,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_READ_ONLY,
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
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_DELIVERY_MODE,
		.name = "Slice delivery mode",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE_CYCLIC,
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
		.id = V4L2_CID_MPEG_VIDEO_HEADER_MODE,
		.name = "Sequence Header Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE,
		.maximum = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		.default_value =
			V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE) |
		(1 << V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME)
		),
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
			(1 << V4L2_MPEG_VIDC_EXTRADATA_LTR) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_ROI_QP)
			),
		.qmenu = mpeg_video_vidc_extradata,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_HIER_P_NUM_LAYERS,
		.name = "Set Hier P num layers",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 6,
		.default_value = 0,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_HIER_B_NUM_LAYERS,
		.name = "Set Hier B num layers",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 3,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_HYBRID_HIERP_MODE,
		.name = "Set Hybrid Hier P mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 5,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MAX_HIERP_LAYERS,
		.name = "Set Max Hier P num layers sessions",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 6,
		.default_value = 0,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_LAYER_ID,
		.name = "Layer ID for different settings",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MSM_VIDC_ALL_LAYER_ID,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_PARAM_SAR_WIDTH,
		.name = "SAR Width",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 4096,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_PARAM_SAR_HEIGHT,
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
		.id = V4L2_CID_MPEG_VIDC_VENC_PARAM_LAYER_BITRATE,
		.name = "Layer wise bitrate for H264/H265 Hybrid HP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_BIT_RATE,
		.maximum = MAX_BIT_RATE,
		.default_value = DEFAULT_BIT_RATE,
		.step = BIT_RATE_STEP,
		.menu_skip_mask = 0,
		.qmenu = NULL,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE,
		.name = "Set Encoder Frame rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = INT_MAX,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_TYPE,
		.name = "BITRATE TYPE",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_ENABLE,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BLUR_WIDTH,
		.name = "Set Blur width",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 2048,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BLUR_HEIGHT,
		.name = "Set Blur height",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 2048,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_TYPE,
		.name = "Bounds of I-frame size",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_DEFAULT,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_UNLIMITED,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_DEFAULT,
		.menu_skip_mask = ~(
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_DEFAULT) |
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_MEDIUM) |
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_HUGE) |
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_UNLIMITED)),
		.qmenu = iframe_sizes,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FLIP,
		.name = "Flip",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_FLIP_NONE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_FLIP_BOTH,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_FLIP_NONE,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_FLIP_NONE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_FLIP_HORI) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_FLIP_VERT) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_FLIP_BOTH)
		),
		.qmenu = mpeg_video_flip,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_HDR_INFO,
		.name = "Enable/Disable HDR INFO",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_00,
		.name = "RGB PRIMARIES[0][0]",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_01,
		.name = "RGB PRIMARIES[0][1]",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_10,
		.name = "RGB PRIMARIES[1][0]",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_11,
		.name = "RGB PRIMARIES[1][1]",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_20,
		.name = "RGB PRIMARIES[2][0]",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_21,
		.name = "RGB PRIMARIES[2][1]",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_WHITEPOINT_X,
		.name = "WHITE POINT X",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_WHITEPOINT_Y,
		.name = "WHITE POINT Y",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_MAX_DISP_LUM,
		.name = "MAX DISPLAY LUMINANCE",
		.type =  V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_MIN_DISP_LUM,
		.name = "MIN DISPLAY LUMINANCE",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_MAX_CLL,
		.name = "MAX CLL",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VENC_MAX_FLL,
		.name = "MAX FLL",
		.type = V4L2_CTRL_TYPE_U32,
		.minimum = 0,
		.maximum = UINT_MAX,
		.default_value = 0,
		.step = 1,
		.qmenu = NULL,
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
};


static int msm_venc_set_csc(struct msm_vidc_inst *inst,
					u32 color_primaries, u32 custom_matrix);

static struct v4l2_ctrl *get_ctrl_from_cluster(int id,
		struct v4l2_ctrl **cluster, int ncontrols)
{
	int c;

	for (c = 0; c < ncontrols; ++c)
		if (cluster[c]->id == id)
			return cluster[c];
	return NULL;
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
	struct hal_idr_period idr_period;
	struct hal_vpe_rotation vpe_rotation;
	struct hal_intra_refresh intra_refresh;
	struct hal_multi_slice_control multi_slice_control;
	struct hal_h264_db_control h264_db_control;
	struct hal_enable enable;
	struct hal_quantization quant;
	u32 property_id = 0, property_val = 0;
	void *pdata = NULL;
	struct v4l2_ctrl *temp_ctrl = NULL;
	struct hfi_device *hdev;
	struct hal_extradata_enable extra;
	struct hal_ltr_use use_ltr;
	struct hal_ltr_mark mark_ltr;
	struct hal_hybrid_hierp hyb_hierp;
	u32 hier_p_layers = 0;
	int max_hierp_layers;
	int baselayerid = 0;
	struct hal_video_signal_info signal_info = {0};
	struct hal_vui_timing_info vui_timing_info = {0};
	enum hal_iframesize_type iframesize_type = HAL_IFRAMESIZE_TYPE_DEFAULT;
	u32 color_primaries, custom_matrix;
	struct hal_nal_stream_format_select stream_format;

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

	/*
	 * Unlock the control prior to setting to the hardware. Otherwise
	 * lower level code that attempts to do a get_ctrl() will end up
	 * deadlocking.
	 */
	v4l2_ctrl_unlock(ctrl);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD:
		if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_H264 &&
			inst->fmts[CAPTURE_PORT].fourcc !=
				V4L2_PIX_FMT_H264_NO_SC &&
			inst->fmts[CAPTURE_PORT].fourcc !=
				V4L2_PIX_FMT_HEVC) {
			dprintk(VIDC_ERR,
				"Control %#x only valid for H264 and HEVC\n",
				ctrl->id);
			rc = -ENOTSUPP;
			break;
		}

		property_id = HAL_CONFIG_VENC_IDR_PERIOD;
		idr_period.idr_period = ctrl->val;
		pdata = &idr_period;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES:
	case V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES:
	{
		int num_p, num_b;

		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES);
		num_b = temp_ctrl->val;

		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES);
		num_p = temp_ctrl->val;

		if (ctrl->id == V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES)
			num_p = ctrl->val;
		else if (ctrl->id == V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES)
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
	case V4L2_CID_MPEG_VIDC_VIDEO_ADAPTIVE_B:
		property_id = HAL_PARAM_VENC_ADAPTIVE_B;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME:
		property_id = HAL_CONFIG_VENC_REQUEST_IFRAME;
		request_iframe.enable = true;
		pdata = &request_iframe;
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	{
		struct v4l2_ctrl *hybrid_hp = TRY_GET_CTRL(
			V4L2_CID_MPEG_VIDC_VIDEO_HYBRID_HIERP_MODE);
		if ((ctrl->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR)
			&& hybrid_hp->val) {
			dprintk(VIDC_ERR,
				"CBR_VFR not allowed with Hybrid HP\n");
			rc = -ENOTSUPP;
			break;
		}
		property_id = HAL_PARAM_VENC_RATE_CONTROL;
		property_val = ctrl->val;
		pdata = &property_val;
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
	case V4L2_CID_MPEG_VIDEO_BITRATE_PEAK:
	{
		struct v4l2_ctrl *avg_bitrate = TRY_GET_CTRL(
			V4L2_CID_MPEG_VIDEO_BITRATE);

		if (ctrl->val < avg_bitrate->val) {
			dprintk(VIDC_ERR,
				"Peak bitrate (%d) is lower than average bitrate (%d)\n",
				ctrl->val, avg_bitrate->val);
			rc = -EINVAL;
			break;
		} else if (ctrl->val < avg_bitrate->val * 2) {
			dprintk(VIDC_WARN,
				"Peak bitrate (%d) ideally should be twice the average bitrate (%d)\n",
				ctrl->val, avg_bitrate->val);
		}

		property_id = HAL_CONFIG_VENC_MAX_BITRATE;
		bitrate.bit_rate = ctrl->val;
		bitrate.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &bitrate;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		property_id = HAL_PARAM_VENC_H264_ENTROPY_CONTROL;
		h264_entropy = msm_comm_v4l2_to_hal(
			V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE, ctrl->val);
		pdata = &h264_entropy;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_LEVEL);

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
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_PROFILE);

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
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE:
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.level = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.profile = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_TME_PROFILE:
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_TME_LEVEL);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.level = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_TME_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_TME_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_TME_PROFILE);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = msm_comm_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.profile = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_TME_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_ROTATE:
	{
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_FLIP);
		property_id = HAL_PARAM_VPE_ROTATION;

		if (ctrl->val != 0 && ctrl->val != 90
			&& ctrl->val != 180 && ctrl->val != 270) {
			dprintk(VIDC_ERR, "Invalid rotation angle");
			rc = -ENOTSUPP;
		}

		vpe_rotation.rotate = ctrl->val;
		vpe_rotation.flip = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_FLIP,
				temp_ctrl->val);
		pdata = &vpe_rotation;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_FLIP:
	{
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_ROTATE);
		property_id = HAL_PARAM_VPE_ROTATION;
		vpe_rotation.rotate = temp_ctrl->val;
		vpe_rotation.flip = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_FLIP,
				ctrl->val);
		pdata = &vpe_rotation;
		break;
	}
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
			temp_ctrl = TRY_GET_CTRL(temp);

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
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);

		property_id = HAL_PARAM_VENC_MULTI_SLICE_CONTROL;
		multi_slice_control.multi_slice = temp_ctrl->val;
		multi_slice_control.slice_size = ctrl->val;
		pdata = &multi_slice_control;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_DELIVERY_MODE: {
		bool codecs_supported =
			inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC ||
			inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264 ||
			inst->fmts[CAPTURE_PORT].fourcc ==
							V4L2_PIX_FMT_H264_NO_SC;

		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
		if (codecs_supported && temp_ctrl->val ==
				V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) {
			property_id = HAL_PARAM_VENC_SLICE_DELIVERY_MODE;
			enable.enable = true;
		} else {
			dprintk(VIDC_WARN,
				"Failed : slice delivery mode is not valid\n");
			enable.enable = false;
		}
		pdata = &enable;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE_CYCLIC:
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

		alpha = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA);
		beta = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA);

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

		mode = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE);
		beta = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA);

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

		mode = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE);
		alpha = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA);
		property_id = HAL_PARAM_VENC_H264_DEBLOCK_CONTROL;
		h264_db_control.slice_alpha_offset = alpha->val;
		h264_db_control.slice_beta_offset = ctrl->val;
		h264_db_control.mode = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
				mode->val);
		pdata = &h264_db_control;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		property_id = HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER;

		switch (ctrl->val) {
		case V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE:
			enable.enable = 0;
			break;
		case V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME:
			enable.enable = 1;
			break;
		default:
			rc = -ENOTSUPP;
			break;
		}
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
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA: {
		struct hal_buffer_requirements *buff_req_buffer = NULL;
		int extra_idx = 0;

		property_id = HAL_PARAM_INDEX_EXTRADATA;
		extra.index = msm_comm_get_hal_extradata_index(ctrl->val);
		extra.enable = 1;

		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO:
		case V4L2_MPEG_VIDC_EXTRADATA_ROI_QP:
			inst->bufq[OUTPUT_PORT].num_planes = 2;
			break;
		case V4L2_MPEG_VIDC_EXTRADATA_LTR:
			inst->bufq[CAPTURE_PORT].num_planes = 2;
			break;
		default:
			rc = -ENOTSUPP;
			break;
		}

		pdata = &extra;
		rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session, property_id, pdata);

		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
			break;
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
		property_id = 0;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER:
		property_id = HAL_PARAM_VENC_GENERATE_AUDNAL;

		switch (ctrl->val) {
		case V4L2_MPEG_MSM_VIDC_DISABLE:
			enable.enable = 0;
			break;
		case V4L2_MPEG_MSM_VIDC_ENABLE:
			enable.enable = 1;
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
		use_ltr.use_constraint = false;
		use_ltr.frames = 0;
		pdata = &use_ltr;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME:
		property_id = HAL_CONFIG_VENC_MARKLTRFRAME;
		mark_ltr.mark_frame = ctrl->val;
		pdata = &mark_ltr;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HIER_P_NUM_LAYERS:
		property_id = HAL_CONFIG_VENC_HIER_P_NUM_FRAMES;
		hier_p_layers = ctrl->val;
		if (hier_p_layers > inst->capability.hier_p.max) {
			dprintk(VIDC_ERR,
				"Error setting hier p num layers %d max supported is %d\n",
				hier_p_layers, inst->capability.hier_p.max);
			rc = -ENOTSUPP;
			break;
		}
		pdata = &hier_p_layers;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE:
		property_id = HAL_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HYBRID_HIERP_MODE:
	{
		struct v4l2_ctrl *rate_control;

		rate_control =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_BITRATE_MODE);
		if ((rate_control->val == V4L2_MPEG_VIDEO_BITRATE_MODE_CBR_VFR)
			&& ctrl->val) {
			dprintk(VIDC_ERR,
				"Hybrid HP not allowed with CBR_VFR\n");
			rc = -ENOTSUPP;
			break;
		}
		property_id = HAL_PARAM_VENC_HIER_P_HYBRID_MODE;
		hyb_hierp.layers = ctrl->val;
		pdata = &hyb_hierp;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_MAX_HIERP_LAYERS:
		property_id = HAL_PARAM_VENC_HIER_P_MAX_ENH_LAYERS;
		max_hierp_layers = ctrl->val;
		if (max_hierp_layers > inst->capability.hier_p.max) {
			dprintk(VIDC_ERR,
				"Error max HP layers(%d)>max supported(%d)\n",
				max_hierp_layers, inst->capability.hier_p.max);
			rc = -ENOTSUPP;
			break;
		}
		pdata = &max_hierp_layers;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BASELAYER_ID:
		property_id = HAL_CONFIG_VENC_BASELAYER_PRIORITYID;
		baselayerid = ctrl->val;
		pdata = &baselayerid;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP: {
		struct v4l2_ctrl *qpp, *qpb, *mask;

		property_id = HAL_CONFIG_VENC_FRAME_QP;
		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP);
		qpb = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP);
		mask = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK);

		quant.qpi = ctrl->val;
		quant.qpp = qpp->val;
		quant.qpb = qpb->val;
		quant.enable = mask->val;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP: {
		struct v4l2_ctrl *qpi, *qpb, *mask;

		property_id = HAL_CONFIG_VENC_FRAME_QP;
		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP);
		qpb = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP);
		mask = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK);

		quant.qpp = ctrl->val;
		quant.qpi = qpi->val;
		quant.qpb = qpb->val;
		quant.enable = mask->val;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP: {
		struct v4l2_ctrl *qpp, *qpi, *mask;

		property_id = HAL_CONFIG_VENC_FRAME_QP;
		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP);
		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP);
		mask = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK);

		quant.qpb = ctrl->val;
		quant.qpp = qpp->val;
		quant.qpi = qpi->val;
		quant.enable = mask->val;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK: {
		struct v4l2_ctrl *qpi, *qpp, *qpb;

		property_id = HAL_CONFIG_VENC_FRAME_QP;
		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP);
		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP);
		qpb = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP);

		quant.qpi = qpi->val;
		quant.qpp = qpp->val;
		quant.qpb = qpb->val;
		quant.enable = ctrl->val;
		quant.layer_id = MSM_VIDC_ALL_LAYER_ID;
		pdata = &quant;
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
			property_id = HAL_CONFIG_OPERATING_RATE;
			operating_rate.operating_rate =
				inst->clk_data.operating_rate;
			pdata = &operating_rate;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_TYPE:
	{
		property_id = HAL_PARAM_VENC_BITRATE_TYPE;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE:
	{
		signal_info.color_space = ctrl->val;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE);
		signal_info.full_range = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS);
		signal_info.transfer_chars = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS);
		signal_info.matrix_coeffs = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE:
	{
		signal_info.full_range = ctrl->val;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
		signal_info.color_space = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS);
		signal_info.transfer_chars = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS);
		signal_info.matrix_coeffs = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS:
	{
		signal_info.transfer_chars = ctrl->val;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE);
		signal_info.full_range = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
		signal_info.color_space = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS);
		signal_info.matrix_coeffs = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_MATRIX_COEFFS:
	{
		signal_info.matrix_coeffs = ctrl->val;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE);
		signal_info.full_range = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS);
		signal_info.transfer_chars = temp_ctrl ? temp_ctrl->val : 0;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
		signal_info.color_space = temp_ctrl ? temp_ctrl->val : 0;
		property_id = HAL_PARAM_VENC_VIDEO_SIGNAL_INFO;
		pdata = &signal_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC:
		if (ctrl->val != V4L2_MPEG_MSM_VIDC_ENABLE)
			break;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
		color_primaries = temp_ctrl->val;
		temp_ctrl =
		   TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX);
		custom_matrix = temp_ctrl->val;
		rc = msm_venc_set_csc(inst, color_primaries, custom_matrix);
		if (rc)
			dprintk(VIDC_ERR, "fail to set csc: %d\n", rc);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_CUSTOM_MATRIX:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_COLOR_SPACE);
		color_primaries = temp_ctrl->val;
		rc = msm_venc_set_csc(inst, color_primaries, ctrl->val);
		if (rc)
			dprintk(VIDC_ERR, "fail to set csc: %d\n", rc);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
	{
		property_id = HAL_PARAM_VENC_LOW_LATENCY;
		if (ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE)
			enable.enable = 1;
		else
			enable.enable = 0;
		pdata = &enable;
		inst->clk_data.low_latency_mode = (bool) enable.enable;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
		property_id = HAL_PARAM_VENC_H264_TRANSFORM_8x8;
		switch (ctrl->val) {
		case V4L2_MPEG_MSM_VIDC_ENABLE:
			enable.enable = 1;
			break;
		case V4L2_MPEG_MSM_VIDC_DISABLE:
			enable.enable = 0;
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
	case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_TYPE:
		property_id = HAL_PARAM_VENC_IFRAMESIZE_TYPE;
		iframesize_type = msm_comm_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_TYPE,
				ctrl->val);
		pdata = &iframesize_type;
		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
	{
		property_id = HAL_PARAM_VENC_DISABLE_RC_TIMESTAMP;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	}
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

		rc_mode = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_BITRATE_MODE);

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
	case V4L2_CID_MPEG_VIDC_VENC_PARAM_SAR_WIDTH:
	case V4L2_CID_MPEG_VIDC_VENC_PARAM_SAR_HEIGHT:
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_WIDTH:
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_HEIGHT:
	case V4L2_CID_MPEG_VIDC_VIDEO_LAYER_ID:
	case V4L2_CID_MPEG_VIDC_VENC_PARAM_LAYER_BITRATE:
	case V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MIN:
	case V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MIN:
	case V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MIN:
	case V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MAX:
	case V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MAX:
	case V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MAX:
	case V4L2_CID_MPEG_VIDC_VENC_HDR_INFO:
	case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_00:
	case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_01:
	case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_10:
	case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_11:
	case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_20:
	case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_21:
	case V4L2_CID_MPEG_VIDC_VENC_WHITEPOINT_X:
	case V4L2_CID_MPEG_VIDC_VENC_WHITEPOINT_Y:
	case V4L2_CID_MPEG_VIDC_VENC_MAX_DISP_LUM:
	case V4L2_CID_MPEG_VIDC_VENC_MIN_DISP_LUM:
	case V4L2_CID_MPEG_VIDC_VENC_MAX_CLL:
	case V4L2_CID_MPEG_VIDC_VENC_MAX_FLL:
		dprintk(VIDC_DBG, "Set the control : %#x using ext ctrl\n",
			ctrl->id);
		break;
	default:
		dprintk(VIDC_ERR, "Unsupported index: %x\n", ctrl->id);
		rc = -ENOTSUPP;
		break;
	}

	v4l2_ctrl_lock(ctrl);
#undef TRY_GET_CTRL

	if (!rc && property_id) {
		dprintk(VIDC_DBG,
			"Control: Name = %s, ID = 0x%x Value = %d\n",
				ctrl->name, ctrl->id, ctrl->val);
		rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session, property_id, pdata);
	}

	return rc;
}

int msm_venc_ext_layer_id_update(struct v4l2_ext_control *control,
		u32 *property_id,
		void **pdata,
		struct hal_quantization *qp,
		struct hal_quantization_range *qp_range,
		struct hal_bitrate *bitrate)
{
	switch (control->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP:
		qp->qpi = control->value;
		*property_id =
			HAL_CONFIG_VENC_FRAME_QP;
		*pdata = qp;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP:
		qp->qpp = control->value;
		*property_id =
			HAL_CONFIG_VENC_FRAME_QP;
		*pdata = qp;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP:
		qp->qpb = control->value;
		*property_id =
			HAL_CONFIG_VENC_FRAME_QP;
		*pdata = qp;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_QP_MASK:
		qp->enable = control->value;
		*property_id =
			HAL_CONFIG_VENC_FRAME_QP;
		*pdata = qp;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MIN:
		qp_range->qpi_min = control->value;
		*property_id =
			HAL_PARAM_VENC_SESSION_QP_RANGE;
		*pdata = qp_range;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MIN:
		qp_range->qpp_min = control->value;
		*property_id =
			HAL_PARAM_VENC_SESSION_QP_RANGE;
		*pdata = qp_range;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MIN:
		qp_range->qpb_min = control->value;
		*property_id =
			HAL_PARAM_VENC_SESSION_QP_RANGE;
		*pdata = qp_range;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP_MAX:
		qp_range->qpi_max = control->value;
		*property_id =
			HAL_PARAM_VENC_SESSION_QP_RANGE;
		*pdata = qp_range;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP_MAX:
		qp_range->qpp_max = control->value;
		*property_id =
			HAL_PARAM_VENC_SESSION_QP_RANGE;
		*pdata = qp_range;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP_MAX:
		qp_range->qpb_max = control->value;
		*property_id =
			HAL_PARAM_VENC_SESSION_QP_RANGE;
		*pdata = qp_range;
		break;
	case V4L2_CID_MPEG_VIDC_VENC_PARAM_LAYER_BITRATE:
		bitrate->bit_rate = control->value;
		*property_id =
			HAL_CONFIG_VENC_TARGET_BITRATE;
		*pdata = bitrate;
		break;
	}
	return 0;
}

int msm_venc_s_ext_ctrl(struct msm_vidc_inst *inst,
	struct v4l2_ext_controls *ctrl)
{
	int rc = 0, i;
	struct v4l2_ext_control *control;
	struct hfi_device *hdev;
	struct hal_ltr_mode ltr_mode;
	u32 property_id = 0;
	void *pdata = NULL;
	struct msm_vidc_capability *cap = NULL;
	struct hal_aspect_ratio sar;
	struct hal_bitrate bitrate;
	struct hal_frame_size blur_res;
	struct hal_quantization_range qp_range;
	struct hal_quantization qp;
	struct msm_vidc_mastering_display_colour_sei_payload *mdisp_sei = NULL;
	struct msm_vidc_content_light_level_sei_payload *cll_sei = NULL;

	if (!inst || !inst->core || !inst->core->device || !ctrl) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	/* This will check the range for contols and clip if necessary */
	v4l2_try_ext_ctrls(&inst->ctrl_handler, ctrl);

	hdev = inst->core->device;
	cap = &inst->capability;

	control = ctrl->controls;

	mdisp_sei = &(inst->hdr10_sei_params.disp_color_sei);
	cll_sei = &(inst->hdr10_sei_params.cll_sei);

	for (i = 0; i < ctrl->count; i++) {
		switch (control[i].id) {
		case V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT:
			ltr_mode.count =  control[i].value;
			if (ltr_mode.count > cap->ltr_count.max) {
				dprintk(VIDC_ERR,
					"Invalid LTR count %d. Supported max: %d\n",
					ltr_mode.count,
					cap->ltr_count.max);
				rc = -EINVAL;
			}
			ltr_mode.mode = HAL_LTR_MODE_MANUAL;
			ltr_mode.trust_mode = 1;
			property_id = HAL_PARAM_VENC_LTRMODE;
			pdata = &ltr_mode;
			break;
		case V4L2_CID_MPEG_VIDC_VENC_PARAM_SAR_WIDTH:
			sar.aspect_width = control[i].value;
			property_id = HAL_PROPERTY_PARAM_VENC_ASPECT_RATIO;
			pdata = &sar;
			break;
		case V4L2_CID_MPEG_VIDC_VENC_PARAM_SAR_HEIGHT:
			sar.aspect_height = control[i].value;
			property_id = HAL_PROPERTY_PARAM_VENC_ASPECT_RATIO;
			pdata = &sar;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_WIDTH:
			property_id = HAL_CONFIG_VENC_BLUR_RESOLUTION;
			blur_res.width = control[i].value;
			blur_res.buffer_type = HAL_BUFFER_INPUT;
			property_id = HAL_CONFIG_VENC_BLUR_RESOLUTION;
			pdata = &blur_res;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_HEIGHT:
			blur_res.height = control[i].value;
			blur_res.buffer_type = HAL_BUFFER_INPUT;
			property_id = HAL_CONFIG_VENC_BLUR_RESOLUTION;
			pdata = &blur_res;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_LAYER_ID:
			qp.layer_id = control[i].value;
			/* Enable QP for all frame types by default */
			qp.enable = 7;
			qp_range.layer_id = control[i].value;
			bitrate.layer_id = control[i].value;
			i++;
			while (i < ctrl->count) {
				msm_venc_ext_layer_id_update(
						&control[i],
						&property_id,
						&pdata,
						&qp,
						&qp_range,
						&bitrate);
				i++;
			}
			break;
		case V4L2_CID_MPEG_VIDC_VENC_HDR_INFO:
			if (control[i].value ==
				V4L2_MPEG_MSM_VIDC_DISABLE ||
					!mdisp_sei || !cll_sei)
				break;
			i++;
			while (i < ctrl->count) {
				switch (control[i].id) {
				case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_00:
					mdisp_sei->nDisplayPrimariesX[0] =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_01:
					mdisp_sei->nDisplayPrimariesY[0] =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_10:
					mdisp_sei->nDisplayPrimariesX[1] =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_11:
					mdisp_sei->nDisplayPrimariesY[1] =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_20:
					mdisp_sei->nDisplayPrimariesX[2] =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_RGB_PRIMARY_21:
					mdisp_sei->nDisplayPrimariesY[2] =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_WHITEPOINT_X:
					mdisp_sei->nWhitePointX =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_WHITEPOINT_Y:
					mdisp_sei->nWhitePointY =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_MAX_DISP_LUM:
					mdisp_sei->nMaxDisplayMasteringLuminance
						= control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_MIN_DISP_LUM:
					mdisp_sei->nMinDisplayMasteringLuminance
						= control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_MAX_CLL:
					cll_sei->nMaxContentLight =
						control[i].value;
					break;
				case V4L2_CID_MPEG_VIDC_VENC_MAX_FLL:
					cll_sei->nMaxPicAverageLight =
						control[i].value;
					break;
				default:
					dprintk(VIDC_ERR,
							"Unknown Ctrl:%d, not part of HDR Info",
							control[i].id);
				}
				i++;
			}
			property_id =
				HAL_PARAM_VENC_HDR10_PQ_SEI;
			pdata = &inst->hdr10_sei_params;
			break;
		default:
			dprintk(VIDC_ERR, "Invalid id set: %d\n",
				control[i].id);
			rc = -ENOTSUPP;
			break;
		}
		if (rc)
			break;
	}

	if (!rc && property_id) {
		dprintk(VIDC_DBG, "Control: HAL property=%x\n", property_id);
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
