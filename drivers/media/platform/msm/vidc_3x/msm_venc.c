// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/slab.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_dcvs.h"

#define MSM_VENC_DVC_NAME "msm_venc_8974"
#define MIN_NUM_OUTPUT_BUFFERS 4
#define MIN_NUM_CAPTURE_BUFFERS 4
#define MIN_BIT_RATE 32000
#define MAX_BIT_RATE 300000000
#define DEFAULT_BIT_RATE 64000
#define BIT_RATE_STEP 100
#define DEFAULT_FRAME_RATE 15
#define MAX_OPERATING_FRAME_RATE (300 << 16)
#define OPERATING_FRAME_RATE_STEP (1 << 16)
#define MAX_SLICE_BYTE_SIZE ((MAX_BIT_RATE)>>3)
#define MIN_SLICE_BYTE_SIZE 512
#define MAX_SLICE_MB_SIZE ((4096 * 2304) >> 8)
#define I_FRAME_QP 26
#define P_FRAME_QP 28
#define B_FRAME_QP 30
#define MAX_INTRA_REFRESH_MBS ((4096 * 2304) >> 8)
#define MAX_NUM_B_FRAMES 4
#define MAX_LTR_FRAME_COUNT 10
#define MAX_HYBRID_HIER_P_LAYERS 6

#define L_MODE V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY
#define CODING V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_CODING_EFFICIENCY
#define BITSTREAM_RESTRICT_ENABLED \
	V4L2_MPEG_VIDC_VIDEO_H264_VUI_BITSTREAM_RESTRICT_ENABLED
#define BITSTREAM_RESTRICT_DISABLED \
	V4L2_MPEG_VIDC_VIDEO_H264_VUI_BITSTREAM_RESTRICT_DISABLED
#define MIN_TIME_RESOLUTION 1
#define MAX_TIME_RESOLUTION 0xFFFFFF
#define DEFAULT_TIME_RESOLUTION 0x7530

/*
 * Default 601 to 709 conversion coefficients for resolution: 176x144 negative
 * coeffs are converted to s4.9 format (e.g. -22 converted to ((1 << 13) - 22)
 * 3x3 transformation matrix coefficients in s4.9 fixed point format
 */
static u32 vpe_csc_601_to_709_matrix_coeff[HAL_MAX_MATRIX_COEFFS] = {
	470, 8170, 8148, 0, 490, 50, 0, 34, 483
};

/* offset coefficients in s9 fixed point format */
static u32 vpe_csc_601_to_709_bias_coeff[HAL_MAX_BIAS_COEFFS] = {
	34, 0, 4
};

/* clamping value for Y/U/V([min,max] for Y/U/V) */
static u32 vpe_csc_601_to_709_limit_coeff[HAL_MAX_LIMIT_COEFFS] = {
	16, 235, 16, 240, 16, 240
};

static const char *const mpeg_video_rate_control[] = {
	"No Rate Control",
	"VBR VFR",
	"VBR CFR",
	"CBR VFR",
	"CBR CFR",
	"MBR CFR",
	"MBR VFR",
	NULL
};

static const char *const mpeg_video_rotation[] = {
	"No Rotation",
	"90 Degree Rotation",
	"180 Degree Rotation",
	"270 Degree Rotation",
	NULL
};

static const char *const h264_video_entropy_cabac_model[] = {
	"Model 0",
	"Model 1",
	"Model 2",
	NULL
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

static const char *const intra_refresh_modes[] = {
	"None",
	"Cyclic",
	"Adaptive",
	"Cyclic Adaptive",
	"Random"
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL,
		.name = "Video Framerate and Bitrate Control",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_MBR_VFR,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_VFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_VFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_CFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_MBR_CFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_MBR_VFR)
		),
		.qmenu = mpeg_video_rate_control,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE,
		.name = "Bitrate Control",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		.maximum = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR,
		.default_value = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) |
		(1 << V4L2_MPEG_VIDEO_BITRATE_MODE_CBR)
		),
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL,
		.name = "CABAC Model",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_1,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_1) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_2)
		),
		.qmenu = h264_video_entropy_cabac_model,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
		.name = "MPEG4 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		.maximum = CODING,
		.default_value = V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
		.name = "MPEG4 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
		.maximum = V4L2_MPEG_VIDEO_MPEG4_LEVEL_5,
		.default_value = V4L2_MPEG_VIDEO_MPEG4_LEVEL_0,
		.menu_skip_mask = 0,
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
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_5_2,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE,
		.name = "H263 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHLATENCY,
		.default_value = V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BASELINE,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BASELINE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_H320CODING) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BACKWARDCOMPATIBLE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_ISWV2) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_ISWV3) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHCOMPRESSION) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_INTERNET) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_INTERLACE) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHLATENCY)
		),
		.qmenu = h263_profile,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL,
		.name = "H263 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_7_0,
		.default_value = V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_1_0,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_1_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_2_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_3_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_4_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_5_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_6_0) |
		(1 << V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_7_0)
		),
		.qmenu = h263_level,
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
		.maximum = V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5_2,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_1,
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
		(1 << V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5_1)
		),
		.qmenu = hevc_tier_level,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION,
		.name = "Rotation",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_NONE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_270,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_NONE,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_NONE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_90) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_180) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_270)
		),
		.qmenu = mpeg_video_rotation,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP,
		.name = "H264 I Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP,
		.name = "H264 P Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = P_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP,
		.name = "H264 B Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = B_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP,
		.name = "H263 I Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 31,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP,
		.name = "H263 P Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 31,
		.default_value = P_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP,
		.name = "H263 B Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 31,
		.default_value = B_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VPX_I_FRAME_QP,
		.name = "VPX I Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VPX_P_FRAME_QP,
		.name = "VPX P Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = P_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP,
		.name = "H264 Minimum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = 1,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_MAX_QP,
		.name = "H264 Maximum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = 51,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VPX_MIN_QP,
		.name = "VPX Minimum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = 0,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_VPX_MAX_QP,
		.name = "VPX Maximum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 127,
		.default_value = 127,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP8_MIN_QP,
		.name = "VP8 Minimum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 128,
		.default_value = 1,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP8_MAX_QP,
		.name = "VP8 Maximum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 128,
		.default_value = 128,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP,
		.name = "MPEG4 Minimum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 31,
		.default_value = 1,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP,
		.name = "MPEG4 Maximum QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 31,
		.default_value = 31,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MIN_QP_PACKED,
		.name = "H264 Minimum QP PACKED",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0x00010101,
		.maximum = 0x00333333,
		.default_value = 0x00010101,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MAX_QP_PACKED,
		.name = "H264 Maximum QP PACKED",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0x00010101,
		.maximum = 0x00333333,
		.default_value = 0x00333333,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,
		.name = "Slice Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.maximum = V4L2_MPEG_VIDEO_MULTI_SLICE_GOB,
		.default_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE) |
		(1 << V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) |
		(1 << V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) |
		(1 << V4L2_MPEG_VIDEO_MULTI_SLICE_GOB)
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
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_GOB,
		.name = "Slice GOB",
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE,
		.name = "Intra Refresh Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_NONE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOMMODE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_NONE,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_NONE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_CYCLIC) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_ADAPTIVE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_CYCLIC_ADAPTIVE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOMMODE)
		),
		.qmenu = intra_refresh_modes,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS,
		.name = "Intra Refresh AIR MBS",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_INTRA_REFRESH_MBS,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF,
		.name = "Intra Refresh AIR REF",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_INTRA_REFRESH_MBS,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CIR_MBS,
		.name = "Intra Refresh CIR MBS",
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
		.maximum = V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_I_FRAME,
		.default_value =
			V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_I_FRAME,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE) |
		(1 << V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_I_FRAME)
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
		.maximum = V4L2_MPEG_VIDC_EXTRADATA_YUV_STATS,
		.default_value = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NONE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP) |
			(1ULL << V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP) |
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
			(1 << V4L2_MPEG_VIDC_EXTRADATA_LTR) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_METADATA_MBI) |
			(1ULL << V4L2_MPEG_VIDC_EXTRADATA_YUV_STATS)|
			(1 << V4L2_MPEG_VIDC_EXTRADATA_ROI_QP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_PQ_INFO) |
			(1ULL << V4L2_MPEG_VIDC_EXTRADATA_ENC_FRAME_QP)
			),
		.qmenu = mpeg_video_vidc_extradata,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_VUI_TIMING_INFO,
		.name = "H264 VUI Timing Info",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_H264_VUI_TIMING_INFO_DISABLED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_H264_VUI_TIMING_INFO_ENABLED,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_H264_VUI_TIMING_INFO_DISABLED,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER,
		.name = "H264 AU Delimiter",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_AU_DELIMITER_DISABLED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_AU_DELIMITER_ENABLED,
		.step = 1,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_AU_DELIMITER_DISABLED,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_SET_PERF_LEVEL,
		.name = "Encoder Performance Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL,
		.maximum = V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO,
		.default_value = V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL,
		.menu_skip_mask = ~(
			(1 << V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL) |
			(1 << V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO)),
		.qmenu = perf_level,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB,
		.name = "Intra Refresh CIR MBS",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = MAX_INTRA_REFRESH_MBS,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_VUI_BITSTREAM_RESTRICT,
		.name = "H264 VUI Timing Info",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = BITSTREAM_RESTRICT_DISABLED,
		.maximum = BITSTREAM_RESTRICT_ENABLED,
		.default_value = BITSTREAM_RESTRICT_ENABLED,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PRESERVE_TEXT_QUALITY,
		.name = "Preserve Text Qualty",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_PRESERVE_TEXT_QUALITY_DISABLED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_PRESERVE_TEXT_QUALITY_ENABLED,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_PRESERVE_TEXT_QUALITY_DISABLED,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE,
		.name = "Deinterlace for encoder",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE_DISABLED,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE_ENABLED,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE_DISABLED,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MPEG4_TIME_RESOLUTION,
		.name = "Vop time increment resolution",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_TIME_RESOLUTION,
		.maximum = MAX_TIME_RESOLUTION,
		.default_value = DEFAULT_TIME_RESOLUTION,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_SEQ_HEADER,
		.name = "Request Seq Header",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.minimum = 0,
		.maximum = 0,
		.default_value = 0,
		.step = 0,
		.menu_skip_mask = 0,
		.qmenu = NULL,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_LTRMODE,
		.name = "Ltr Mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = V4L2_MPEG_VIDC_VIDEO_LTR_MODE_DISABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_LTR_MODE_MANUAL,
		.default_value = V4L2_MPEG_VIDC_VIDEO_LTR_MODE_DISABLE,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE,
		.name = "Encoder Timestamp Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum =
			V4L2_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE_HONOR,
		.maximum =
			V4L2_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE_IGNORE,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE_HONOR,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE_HONOR) |
		(1 << V4L2_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE_IGNORE)),
		.qmenu = timestamp_mode,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE,
		.name = "VP8 Error Resilience mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE_DISABLED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE_ENABLED,
		.default_value =
			V4L2_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE_DISABLED,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_INITIAL_QP,
		.name = "Enable setting initial QP",
		.type = V4L2_CTRL_TYPE_BITMASK,
		.minimum = 0,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_INITIAL_QP_IFRAME |
			V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_INITIAL_QP_PFRAME |
			V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_INITIAL_QP_BFRAME,
		.default_value = 0,
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_I_FRAME_QP,
		.name = "Iframe initial QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 127,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_P_FRAME_QP,
		.name = "Pframe initial QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 127,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_B_FRAME_QP,
		.name = "Bframe initial QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 127,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP,
		.name = "Iframe initial QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP,
		.name = "Pframe initial QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP,
		.name = "Bframe initial QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = 1,
		.step = 1,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_X_RANGE,
		.name = "I-Frame X coordinate search range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 4,
		.maximum = 128,
		.default_value = 4,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_Y_RANGE,
		.name = "I-Frame Y coordinate search range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 4,
		.maximum = 128,
		.default_value = 4,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PFRAME_X_RANGE,
		.name = "P-Frame X coordinate search range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 4,
		.maximum = 128,
		.default_value = 4,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PFRAME_Y_RANGE,
		.name = "P-Frame Y coordinate search range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 4,
		.maximum = 128,
		.default_value = 4,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BFRAME_X_RANGE,
		.name = "B-Frame X coordinate search range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 4,
		.maximum = 128,
		.default_value = 4,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_BFRAME_Y_RANGE,
		.name = "B-Frame Y coordinate search range",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 4,
		.maximum = 128,
		.default_value = 4,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_NAL_SVC,
		.name = "Enable H264 SVC NAL",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_H264_NAL_SVC_DISABLED,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_H264_NAL_SVC_ENABLED,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_H264_NAL_SVC_DISABLED,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PERF_MODE,
		.name = "Set Encoder performance mode",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = V4L2_MPEG_VIDC_VIDEO_PERF_MAX_QUALITY,
		.maximum = V4L2_MPEG_VIDC_VIDEO_PERF_POWER_SAVE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_PERF_MAX_QUALITY,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MBI_STATISTICS_MODE,
		.name = "MBI Statistics Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_DEFAULT,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_3,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_DEFAULT,
		.menu_skip_mask = ~(
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_DEFAULT) |
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_1) |
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_2) |
			(1 << V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_3)),
		.qmenu = mbi_statistics,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CONFIG_QP,
		.name = "Set frame level QP",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 127,
		.default_value = 1,
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
		.minimum = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_ENABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_PRIORITY_REALTIME_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VQZIP_SEI,
		.name = "VQZIP SEI",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_VQZIP_SEI_DISABLE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_VQZIP_SEI_ENABLE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_VQZIP_SEI_DISABLE,
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
		.maximum = MAX_OPERATING_FRAME_RATE,
		.default_value = 0,
		.step = OPERATING_FRAME_RATE_STEP,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_TYPE,
		.name = "BITRATE TYPE",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_DISABLE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_ENABLE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_ENABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_PIC_ORDER_CNT,
		.name = "Set H264 Picture Order Count",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 2,
		.default_value = 0,
		.step = 2,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC,
		.name = "Set VPE Color space conversion coefficients",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_DISABLE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_ENABLE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE,
		.name = "Low Latency Mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_DISABLE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_ENABLE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_DISABLE,
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
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE_DISABLE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE_ENABLE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_FULL_RANGE_DISABLE,
		.step = 1,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_TRANSFER_CHARS,
		.name = "Set Color space transfer characterstics",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MSM_VIDC_TRANSFER_BT709_5,
		.maximum = MSM_VIDC_TRANSFER_BT_2020_12,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_TRANSFORM_8x8,
		.name = "Transform 8x8",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_H264_TRANSFORM_8x8_DISABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_H264_TRANSFORM_8x8_ENABLE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_H264_TRANSFORM_8x8_ENABLE,
		.step = 1,
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
};
struct msm_vidc_format_constraint enc_pix_format_constraints[] = {
	{
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.y_max_stride = 128,
		.y_buffer_alignment = 32,
		.uv_max_stride = 128,
		.uv_buffer_alignment = 16,
	},
	{
		.fourcc = V4L2_PIX_FMT_NV21,
		.num_planes = 2,
		.y_max_stride = 128,
		.y_buffer_alignment = 32,
		.uv_max_stride = 128,
		.uv_buffer_alignment = 16,
	},
};
#define NUM_CTRLS ARRAY_SIZE(msm_venc_ctrls)

static u32 get_frame_size_nv12(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height);
}

static u32 get_frame_size_nv12_ubwc(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12_UBWC, width, height);
}

static u32 get_frame_size_rgba(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_RGBA8888, width, height);
}

static u32 get_frame_size_rgba_ubwc(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_RGBA8888_UBWC, width, height);
}

static u32 get_frame_size_nv21(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV21, width, height);
}

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
		.num_planes = 1,
		.get_frame_size = get_frame_size_nv12,
		.type = OUTPUT_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_nv12_ubwc,
		.type = OUTPUT_PORT,
	},
	{
		.name = "RGBA 8:8:8:8",
		.description = "RGBA 8:8:8:8",
		.fourcc = V4L2_PIX_FMT_RGB32,
		.num_planes = 1,
		.get_frame_size = get_frame_size_rgba,
		.type = OUTPUT_PORT,
	},
	{
		.name = "UBWC RGBA 8:8:8:8",
		.description = "UBWC RGBA 8:8:8:8",
		.fourcc = V4L2_PIX_FMT_RGBA8888_UBWC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_rgba_ubwc,
		.type = OUTPUT_PORT,
	},
	{
		.name = "Mpeg4",
		.description = "Mpeg4 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
	},
	{
		.name = "H263",
		.description = "H263 compressed format",
		.fourcc = V4L2_PIX_FMT_H263,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = CAPTURE_PORT,
	},
	{
		.name = "YCrCb Semiplanar 4:2:0",
		.description = "Y/CrCb 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.num_planes = 1,
		.get_frame_size = get_frame_size_nv21,
		.type = OUTPUT_PORT,
	},
};

static void msm_venc_update_plane_count(struct msm_vidc_inst *inst, int type)
{
	struct v4l2_ctrl *ctrl = NULL;
	u32 extradata = 0;

	if (!inst)
		return;

	inst->fmts[type].num_planes = 1;

	ctrl = v4l2_ctrl_find(&inst->ctrl_handler,
		V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA);

	if (ctrl)
		extradata = v4l2_ctrl_g_ctrl(ctrl);

	if (type == CAPTURE_PORT) {
		switch (extradata) {
		case V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO:
		case V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB:
		case V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER:
		case V4L2_MPEG_VIDC_EXTRADATA_LTR:
		case V4L2_MPEG_VIDC_EXTRADATA_ENC_FRAME_QP:
		case V4L2_MPEG_VIDC_EXTRADATA_METADATA_MBI:
			inst->fmts[CAPTURE_PORT].num_planes = 2;
		default:
			break;
		}
	} else if (type == OUTPUT_PORT) {
		switch (extradata) {
		case V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP:
		case V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM:
		case V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO:
		case V4L2_MPEG_VIDC_EXTRADATA_YUV_STATS:
		case V4L2_MPEG_VIDC_EXTRADATA_ROI_QP:
		case V4L2_MPEG_VIDC_EXTRADATA_PQ_INFO:
			inst->fmts[OUTPUT_PORT].num_planes = 2;
			break;
		default:
			break;
		}
	}
}

static int msm_venc_set_csc(struct msm_vidc_inst *inst);

static int msm_venc_queue_setup(struct vb2_queue *q,
				unsigned int *num_buffers,
				unsigned int *num_planes, unsigned int sizes[],
				struct device *alloc_ctxs[])
{
	int i, temp, rc = 0;
	struct msm_vidc_inst *inst;
	struct hal_buffer_count_actual new_buf_count;
	enum hal_property property_id;
	struct hfi_device *hdev;
	struct hal_buffer_requirements *buff_req;
	u32 extra_idx = 0;
	struct hal_buffer_requirements *buff_req_buffer = NULL;

	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input\n");
		return -EINVAL;
	}
	inst = q->drv_priv;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to open instance\n");
		return rc;
	}

	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
		return rc;
	}

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		*num_planes = 1;

		buff_req = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
		if (buff_req) {

			/* Pretend as if the FW itself is asking for additional
			 * buffers, which are required for DCVS
			 */
			unsigned int min_req_buffers =
				buff_req->buffer_count_min +
				msm_dcvs_get_extra_buff_count(inst);
			*num_buffers = max(*num_buffers, min_req_buffers);
		}

		if (*num_buffers < MIN_NUM_CAPTURE_BUFFERS ||
				*num_buffers > VB2_MAX_FRAME) {
			int temp = *num_buffers;

			*num_buffers = clamp_val(*num_buffers,
					MIN_NUM_CAPTURE_BUFFERS,
					VB2_MAX_FRAME);
			dprintk(VIDC_INFO,
				"Changing buffer count on CAPTURE_MPLANE from %d to %d for best effort encoding\n",
				temp, *num_buffers);
		}

		msm_venc_update_plane_count(inst, CAPTURE_PORT);
		*num_planes = inst->fmts[CAPTURE_PORT].num_planes;

		for (i = 0; i < *num_planes; i++) {
			int extra_idx = EXTRADATA_IDX(*num_planes);

			buff_req_buffer = get_buff_req_buffer(inst,
					HAL_BUFFER_OUTPUT);

			sizes[i] = buff_req_buffer ?
				buff_req_buffer->buffer_size : 0;

			if (extra_idx && i == extra_idx &&
					extra_idx < VIDEO_MAX_PLANES) {
				buff_req_buffer = get_buff_req_buffer(inst,
						HAL_BUFFER_EXTRADATA_OUTPUT);
				if (!buff_req_buffer) {
					dprintk(VIDC_ERR,
						"%s: failed - invalid buffer req\n",
						__func__);
					return -EINVAL;
				}

				sizes[i] = buff_req_buffer->buffer_size;
			}
		}

		dprintk(VIDC_DBG, "actual output buffer count set to fw = %d\n",
				*num_buffers);
		property_id = HAL_PARAM_BUFFER_COUNT_ACTUAL;
		new_buf_count.buffer_type = HAL_BUFFER_OUTPUT;
		new_buf_count.buffer_count_actual = *num_buffers;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			property_id, &new_buf_count);

		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = 1;

		*num_buffers = inst->buff_req.buffer[0].buffer_count_actual =
			max(*num_buffers,
				 inst->buff_req.buffer[0].buffer_count_min);

		temp = *num_buffers;

		*num_buffers = clamp_val(*num_buffers,
				MIN_NUM_OUTPUT_BUFFERS,
				VB2_MAX_FRAME);
		dprintk(VIDC_INFO,
			"Changing buffer count on OUTPUT_MPLANE from %d to %d for best effort encoding\n",
			temp, *num_buffers);

		property_id = HAL_PARAM_BUFFER_COUNT_ACTUAL;
		new_buf_count.buffer_type = HAL_BUFFER_INPUT;
		new_buf_count.buffer_count_actual = *num_buffers;

		dprintk(VIDC_DBG, "actual input buffer count set to fw = %d\n",
				*num_buffers);
		msm_venc_update_plane_count(inst, OUTPUT_PORT);
		*num_planes = inst->fmts[OUTPUT_PORT].num_planes;

		rc = call_hfi_op(hdev, session_set_property, inst->session,
					property_id, &new_buf_count);
		if (rc)
			dprintk(VIDC_ERR, "failed to set count to fw\n");

		dprintk(VIDC_DBG, "size = %d, alignment = %d, count = %d\n",
				inst->buff_req.buffer[0].buffer_size,
				inst->buff_req.buffer[0].buffer_alignment,
				inst->buff_req.buffer[0].buffer_count_actual);
		sizes[0] = inst->fmts[OUTPUT_PORT].get_frame_size(
				0, inst->prop.height[OUTPUT_PORT],
				inst->prop.width[OUTPUT_PORT]);

		extra_idx =
			EXTRADATA_IDX(inst->fmts[OUTPUT_PORT].num_planes);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			buff_req_buffer = get_buff_req_buffer(inst,
				HAL_BUFFER_EXTRADATA_INPUT);
			if (!buff_req_buffer) {
				dprintk(VIDC_ERR,
					"%s: failed - invalid buffer req\n",
					__func__);
				return -EINVAL;
			}

			sizes[extra_idx] = buff_req_buffer->buffer_size;
		}

		break;
	default:
		dprintk(VIDC_ERR, "Invalid q type = %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_venc_toggle_hier_p(struct msm_vidc_inst *inst, int layers)
{
	int num_enh_layers = 0;
	u32 property_id = 0;
	struct hfi_device *hdev = NULL;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_VP8)
		return 0;

	num_enh_layers = layers ? : 0;
	dprintk(VIDC_DBG, "%s Hier-P in firmware\n",
			num_enh_layers ? "Enable" : "Disable");

	hdev = inst->core->device;
	property_id = HAL_PARAM_VENC_HIER_P_MAX_ENH_LAYERS;
	rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, property_id,
			(void *)&num_enh_layers);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed with error = %d\n", __func__, rc);
	}
	return rc;
}

static inline int msm_venc_power_save_mode_enable(struct msm_vidc_inst *inst)
{
	u32 rc = 0;
	u32 prop_id = 0, power_save_min = 0, power_save_max = 0, inst_load = 0;
	void *pdata = NULL;
	struct hfi_device *hdev = NULL;
	enum hal_perf_mode venc_mode;
	enum load_calc_quirks quirks = LOAD_CALC_IGNORE_TURBO_LOAD |
		LOAD_CALC_IGNORE_THUMBNAIL_LOAD;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	inst_load = msm_comm_get_inst_load(inst, quirks);
	power_save_min = inst->capability.mbs_per_sec_power_save.min;
	power_save_max = inst->capability.mbs_per_sec_power_save.max;

	dprintk(VIDC_DBG,
		"Power Save Mode min mb's %d max mb's %d inst load %d\n",
		power_save_min, power_save_max, inst_load);

	if (!power_save_min || !power_save_max)
		return rc;

	hdev = inst->core->device;
	if (inst_load >= power_save_min) {
		prop_id = HAL_CONFIG_VENC_PERF_MODE;
		venc_mode = HAL_PERF_MODE_POWER_SAVE;
		pdata = &venc_mode;
		rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session, prop_id, pdata);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: Failed to set power save mode for inst: %pK\n",
				__func__, inst);
			goto fail_power_mode_set;
		}
		inst->flags |= VIDC_LOW_POWER;
		msm_dcvs_enc_set_power_save_mode(inst, true);
		dprintk(VIDC_INFO, "Power Save Mode set for inst: %pK\n", inst);
	}

fail_power_mode_set:
	return rc;
}

static inline int start_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	msm_venc_power_save_mode_enable(inst);
	if (inst->capability.pixelprocess_capabilities &
		HAL_VIDEO_ENCODER_SCALING_CAPABILITY)
		rc = msm_vidc_check_scaling_supported(inst);
	if (rc) {
		dprintk(VIDC_ERR, "H/w scaling is not in valid range\n");
		return -EINVAL;
	}
	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to get Buffer Requirements : %d\n", rc);
		goto fail_start;
	}
	rc = msm_comm_set_scratch_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to set scratch buffers: %d\n", rc);
		goto fail_start;
	}
	rc = msm_comm_set_persist_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to set persist buffers: %d\n", rc);
		goto fail_start;
	}

	msm_comm_scale_clocks_and_bus(inst);

	rc = msm_comm_try_state(inst, MSM_VIDC_START_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
		goto fail_start;
	}
	msm_dcvs_init_load(inst);

fail_start:
	return rc;
}

static int msm_venc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct msm_vidc_inst *inst;
	int rc = 0;

	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
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

static void msm_venc_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;

	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "%s - Invalid input, q = %pK\n", __func__, q);
		return;
	}

	inst = q->drv_priv;
	dprintk(VIDC_DBG, "Streamoff called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
		break;
	default:
		dprintk(VIDC_ERR, "Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}

	msm_comm_scale_clocks_and_bus(inst);

	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK, cap = %d to state: %d\n",
			inst, q->type, MSM_VIDC_CLOSE_DONE);
}

static void msm_venc_buf_queue(struct vb2_buffer *vb)
{
	int rc = msm_comm_qbuf(vb2_get_drv_priv(vb->vb2_queue), vb);

	if (rc)
		dprintk(VIDC_ERR, "Failed to queue buffer: %d\n", rc);
}

static const struct vb2_ops msm_venc_vb2q_ops = {
	.queue_setup = msm_venc_queue_setup,
	.start_streaming = msm_venc_start_streaming,
	.buf_queue = msm_venc_buf_queue,
	.stop_streaming = msm_venc_stop_streaming,
};

const struct vb2_ops *msm_venc_get_vb2q_ops(void)
{
	return &msm_venc_vb2q_ops;
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

/* Helper function to translate V4L2_* to HAL_* */
static inline int venc_v4l2_to_hal(int id, int value)
{
	switch (id) {
	/* MPEG4 */
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0:
			return HAL_MPEG4_LEVEL_0;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_0B:
			return HAL_MPEG4_LEVEL_0b;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_1:
			return HAL_MPEG4_LEVEL_1;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_2:
			return HAL_MPEG4_LEVEL_2;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_3:
			return HAL_MPEG4_LEVEL_3;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_4:
			return HAL_MPEG4_LEVEL_4;
		case V4L2_MPEG_VIDEO_MPEG4_LEVEL_5:
			return HAL_MPEG4_LEVEL_5;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE:
			return HAL_MPEG4_PROFILE_SIMPLE;
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE:
			return HAL_MPEG4_PROFILE_ADVANCEDSIMPLE;
		default:
			goto unknown_value;
		}
	/* H264 */
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			return HAL_H264_PROFILE_BASELINE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
			return HAL_H264_PROFILE_CONSTRAINED_BASE;
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
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH:
			return HAL_H264_PROFILE_CONSTRAINED_HIGH;
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
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_2:
			return HAL_H264_LEVEL_52;
		default:
			goto unknown_value;
		}
	/* H263 */
	case V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BASELINE:
			return HAL_H263_PROFILE_BASELINE;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_H320CODING:
			return HAL_H263_PROFILE_H320CODING;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_BACKWARDCOMPATIBLE:
			return HAL_H263_PROFILE_BACKWARDCOMPATIBLE;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_ISWV2:
			return HAL_H263_PROFILE_ISWV2;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_ISWV3:
			return HAL_H263_PROFILE_ISWV3;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHCOMPRESSION:
			return HAL_H263_PROFILE_HIGHCOMPRESSION;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_INTERNET:
			return HAL_H263_PROFILE_INTERNET;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_INTERLACE:
			return HAL_H263_PROFILE_INTERLACE;
		case V4L2_MPEG_VIDC_VIDEO_H263_PROFILE_HIGHLATENCY:
			return HAL_H263_PROFILE_HIGHLATENCY;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC:
			return HAL_H264_ENTROPY_CAVLC;
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC:
			return HAL_H264_ENTROPY_CABAC;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL:
		switch (value) {
		case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0:
			return HAL_H264_CABAC_MODEL_0;
		case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_1:
			return HAL_H264_CABAC_MODEL_1;
		case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_2:
			return HAL_H264_CABAC_MODEL_2;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_1_0:
			return HAL_H263_LEVEL_10;
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_2_0:
			return HAL_H263_LEVEL_20;
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_3_0:
			return HAL_H263_LEVEL_30;
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_4_0:
			return HAL_H263_LEVEL_40;
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_4_5:
			return HAL_H263_LEVEL_45;
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_5_0:
			return HAL_H263_LEVEL_50;
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_6_0:
			return HAL_H263_LEVEL_60;
		case V4L2_MPEG_VIDC_VIDEO_H263_LEVEL_7_0:
			return HAL_H263_LEVEL_70;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0:
			return HAL_VPX_PROFILE_VERSION_0;
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1:
			return HAL_VPX_PROFILE_VERSION_1;
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_2:
			return HAL_VPX_PROFILE_VERSION_2;
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3:
			return HAL_VPX_PROFILE_VERSION_3;
		case V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED:
			return HAL_VPX_PROFILE_UNUSED;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN:
			return HAL_HEVC_PROFILE_MAIN;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN10:
			return HAL_HEVC_PROFILE_MAIN10;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_PROFILE_MAIN_STILL_PIC:
			return HAL_HEVC_PROFILE_MAIN_STILL_PIC;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_1:
			return HAL_HEVC_MAIN_TIER_LEVEL_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_2:
			return HAL_HEVC_MAIN_TIER_LEVEL_2;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_2_1:
			return HAL_HEVC_MAIN_TIER_LEVEL_2_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_3:
			return HAL_HEVC_MAIN_TIER_LEVEL_3;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_3_1:
			return HAL_HEVC_MAIN_TIER_LEVEL_3_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_4:
			return HAL_HEVC_MAIN_TIER_LEVEL_4;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_4_1:
			return HAL_HEVC_MAIN_TIER_LEVEL_4_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5:
			return HAL_HEVC_MAIN_TIER_LEVEL_5;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5_1:
			return HAL_HEVC_MAIN_TIER_LEVEL_5_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_5_2:
			return HAL_HEVC_MAIN_TIER_LEVEL_5_2;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_6:
			return HAL_HEVC_MAIN_TIER_LEVEL_6;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_6_1:
			return HAL_HEVC_MAIN_TIER_LEVEL_6_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_MAIN_TIER_LEVEL_6_2:
			return HAL_HEVC_MAIN_TIER_LEVEL_6_2;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_1:
			return HAL_HEVC_HIGH_TIER_LEVEL_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_2:
			return HAL_HEVC_HIGH_TIER_LEVEL_2;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_2_1:
			return HAL_HEVC_HIGH_TIER_LEVEL_2_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_3:
			return HAL_HEVC_HIGH_TIER_LEVEL_3;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_3_1:
			return HAL_HEVC_HIGH_TIER_LEVEL_3_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_4:
			return HAL_HEVC_HIGH_TIER_LEVEL_4;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_4_1:
			return HAL_HEVC_HIGH_TIER_LEVEL_4_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5:
			return HAL_HEVC_HIGH_TIER_LEVEL_5;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5_1:
			return HAL_HEVC_HIGH_TIER_LEVEL_5_1;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_5_2:
			return HAL_HEVC_HIGH_TIER_LEVEL_5_2;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_6:
			return HAL_HEVC_HIGH_TIER_LEVEL_6;
		case V4L2_MPEG_VIDC_VIDEO_HEVC_LEVEL_HIGH_TIER_LEVEL_6_1:
			return HAL_HEVC_HIGH_TIER_LEVEL_6_1;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION:
		switch (value) {
		case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_NONE:
			return HAL_ROTATE_NONE;
		case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_90:
			return HAL_ROTATE_90;
		case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_180:
			return HAL_ROTATE_180;
		case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_270:
			return HAL_ROTATE_270;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED:
			return HAL_H264_DB_MODE_DISABLE;
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED:
			return HAL_H264_DB_MODE_ALL_BOUNDARY;
		case L_MODE:
			return HAL_H264_DB_MODE_SKIP_SLICE_BOUNDARY;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_MBI_STATISTICS_MODE:
		switch (value) {
		case V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_DEFAULT:
			return HAL_STATISTICS_MODE_DEFAULT;
		case V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_1:
			return HAL_STATISTICS_MODE_1;
		case V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_2:
			return HAL_STATISTICS_MODE_2;
		case V4L2_CID_MPEG_VIDC_VIDEO_MBI_MODE_3:
			return HAL_STATISTICS_MODE_3;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_TYPE:
		switch (value) {
		case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_DEFAULT:
			return HAL_IFRAMESIZE_TYPE_DEFAULT;
		case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_MEDIUM:
			return HAL_IFRAMESIZE_TYPE_MEDIUM;
		case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_HUGE:
			return HAL_IFRAMESIZE_TYPE_HUGE;
		case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_UNLIMITED:
			return HAL_IFRAMESIZE_TYPE_UNLIMITED;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE:
		switch (value) {
		case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_NONE:
			return HAL_INTRA_REFRESH_NONE;
		case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_CYCLIC:
			return HAL_INTRA_REFRESH_CYCLIC;
		case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOMMODE:
			return HAL_INTRA_REFRESH_RANDOM;
		case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_ADAPTIVE:
			return HAL_INTRA_REFRESH_ADAPTIVE;
		case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_CYCLIC_ADAPTIVE:
			return HAL_INTRA_REFRESH_CYCLIC_ADAPTIVE;
		default:
			goto unknown_value;
		}
	}

unknown_value:
	dprintk(VIDC_WARN, "Unknown control (%x, %d)\n", id, value);
	return -EINVAL;
}

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
			WARN_ON(VIDC_DBG_WARN_ENABLE); \
		} \
		__temp; \
	})

static int msm_venc_validate_qp_value(struct msm_vidc_inst *inst,
					struct v4l2_ctrl *ctrl)
{
	int rc = 0, min, max;
	struct v4l2_ctrl *temp_ctrl = NULL;
	int qp_value = ctrl->val;

#define VALIDATE_BOUNDARIES(__min, __max, __val) ({\
	int __rc = __val >= __min && \
			__val <= __max; \
	if (!__rc) \
		dprintk(VIDC_ERR, "QP beyond range: min(%d) max(%d) val(%d)", \
				__min, __max, __val); \
	__rc; \
})

	switch (inst->fmts[CAPTURE_PORT].fourcc) {
	case V4L2_PIX_FMT_VP8:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_VPX_MAX_QP);
		if (!temp_ctrl) {
			dprintk(VIDC_ERR,
				"failed to get control");
			return -EINVAL;
		}
		max = temp_ctrl->maximum;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_VPX_MIN_QP);
		if (!temp_ctrl) {
			dprintk(VIDC_ERR,
				"failed to get control");
			return -EINVAL;
		}
		min = temp_ctrl->minimum;
		if (!VALIDATE_BOUNDARIES(min, max, qp_value))
			rc = -EINVAL;
		break;
	case V4L2_PIX_FMT_H263:
	case V4L2_PIX_FMT_MPEG4:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP);
		if (!temp_ctrl) {
			dprintk(VIDC_ERR,
				"failed to get control");
			return -EINVAL;
		}
		max = temp_ctrl->maximum;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP);
		if (!temp_ctrl) {
			dprintk(VIDC_ERR,
				"failed to get control");
			return -EINVAL;
		}
		min = temp_ctrl->minimum;
		if (!VALIDATE_BOUNDARIES(min, max, qp_value))
			rc = -EINVAL;
		break;
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_HEVC:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_MAX_QP);
		if (!temp_ctrl) {
			dprintk(VIDC_ERR,
				"failed to get control");
			return -EINVAL;
		}
		max = temp_ctrl->maximum;
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_MIN_QP);
		if (!temp_ctrl) {
			dprintk(VIDC_ERR,
				"failed to get control");
			return -EINVAL;
		}
		min = temp_ctrl->minimum;
		if (!VALIDATE_BOUNDARIES(min, max, qp_value))
			rc = -EINVAL;
		break;
	default:
		dprintk(VIDC_ERR, "%s Invalid Codec\n", __func__);
		return -EINVAL;
	}
	return rc;
#undef VALIDATE_BOUNDARIES
}


static int try_set_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_request_iframe request_iframe;
	struct hal_bitrate bitrate;
	struct hal_profile_level profile_level;
	struct hal_h264_entropy_control h264_entropy_control;
	struct hal_quantization quantization;
	struct hal_intra_period intra_period;
	struct hal_idr_period idr_period;
	struct hal_operations operations;
	struct hal_intra_refresh intra_refresh;
	struct hal_multi_slice_control multi_slice_control;
	struct hal_h264_db_control h264_db_control;
	struct hal_enable enable;
	struct hal_h264_vui_timing_info vui_timing_info;
	struct hal_quantization_range qp_range;
	struct hal_h264_vui_bitstream_restrc vui_bitstream_restrict;
	struct hal_preserve_text_quality preserve_text_quality;
	u32 property_id = 0, property_val = 0;
	void *pdata = NULL;
	struct v4l2_ctrl *temp_ctrl = NULL;
	struct hfi_device *hdev;
	struct hal_extradata_enable extra;
	struct hal_mpeg4_time_resolution time_res;
	struct hal_ltr_use use_ltr;
	struct hal_ltr_mark mark_ltr;
	struct hal_hybrid_hierp hyb_hierp;
	u32 hier_p_layers = 0, hier_b_layers = 0, mbi_statistics_mode = 0;
	enum hal_perf_mode venc_mode;
	int max_hierp_layers;
	int baselayerid = 0;
	int frameqp = 0;
	int pic_order_cnt = 0;
	struct hal_video_signal_info signal_info = {0};
	enum hal_iframesize_type iframesize_type = HAL_IFRAMESIZE_TYPE_DEFAULT;

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
		u32 max_num_b_frames;

		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES);
		num_b = temp_ctrl->val;

		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES);
		num_p = temp_ctrl->val;

		if (ctrl->id == V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES)
			num_p = ctrl->val;
		else if (ctrl->id == V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES)
			num_b = ctrl->val;

		max_num_b_frames = num_b ? MAX_NUM_B_FRAMES : 0;
		property_id = HAL_PARAM_VENC_MAX_NUM_B_FRAMES;
		pdata = &max_num_b_frames;
		rc = call_hfi_op(hdev, session_set_property,
			(void *)inst->session, property_id, pdata);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed : Setprop MAX_NUM_B_FRAMES %d\n",
				rc);
			break;
		}

		property_id = HAL_CONFIG_VENC_INTRA_PERIOD;
		intra_period.pframes = num_p;
		intra_period.bframes = num_b;

		/*
		 *Incase firmware does not have B-Frame support,
		 *offload the b-frame count to p-frame to make up
		 *for the requested Intraperiod
		 */
		if (!inst->capability.bframe.max) {
			intra_period.pframes = num_p + num_b;
			intra_period.bframes = 0;
			dprintk(VIDC_DBG,
				"No bframe support, changing pframe from %d to %d\n",
				num_p, intra_period.pframes);
		}
		pdata = &intra_period;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME:
		property_id = HAL_CONFIG_VENC_REQUEST_IFRAME;
		request_iframe.enable = true;
		pdata = &request_iframe;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL:
	case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
	{
		int final_mode = 0;
		struct v4l2_ctrl update_ctrl = {.id = 0};

		/* V4L2_CID_MPEG_VIDEO_BITRATE_MODE and _RATE_CONTROL
		 * manipulate the same thing.  If one control's state
		 * changes, try to mirror the state in the other control's
		 * value
		 */
		if (ctrl->id == V4L2_CID_MPEG_VIDEO_BITRATE_MODE) {
			if (ctrl->val == V4L2_MPEG_VIDEO_BITRATE_MODE_VBR) {
				final_mode = HAL_RATE_CONTROL_VBR_CFR;
				update_ctrl.val =
				V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR;
			} else {/* ...if (ctrl->val == _BITRATE_MODE_CBR) */
				final_mode = HAL_RATE_CONTROL_CBR_CFR;
				update_ctrl.val =
				V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_CFR;
			}

			update_ctrl.id = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL;

		} else if (ctrl->id == V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL) {
			switch (ctrl->val) {
			case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF:
			case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_VFR:
			case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR:
				update_ctrl.val =
					V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_VFR:
			case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_CFR:
			case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_MBR_CFR:
			case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_MBR_VFR:
				update_ctrl.val =
					V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
				break;
			}

			final_mode = ctrl->val;
			update_ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
		}

		if (update_ctrl.id) {
			temp_ctrl = TRY_GET_CTRL(update_ctrl.id);
			temp_ctrl->val = update_ctrl.val;
		}

		property_id = HAL_PARAM_VENC_RATE_CONTROL;
		property_val = final_mode;
		pdata = &property_val;

		break;
	}
	case V4L2_CID_MPEG_VIDEO_BITRATE:
	{
		property_id = HAL_CONFIG_VENC_TARGET_BITRATE;
		bitrate.bit_rate = ctrl->val;
		bitrate.layer_id = 0;
		pdata = &bitrate;
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
		bitrate.layer_id = 0;
		pdata = &bitrate;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		temp_ctrl = TRY_GET_CTRL(
			V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL);

		property_id = HAL_PARAM_VENC_H264_ENTROPY_CONTROL;
		h264_entropy_control.entropy_mode = venc_v4l2_to_hal(
			V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE, ctrl->val);
		h264_entropy_control.cabac_model = venc_v4l2_to_hal(
			V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL,
			temp_ctrl->val);
		pdata = &h264_entropy_control;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE);

		property_id = HAL_PARAM_VENC_H264_ENTROPY_CONTROL;
		h264_entropy_control.cabac_model = venc_v4l2_to_hal(
			V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE, ctrl->val);
		h264_entropy_control.entropy_mode = venc_v4l2_to_hal(
			V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL,
			temp_ctrl->val);
		pdata = &h264_entropy_control;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = venc_v4l2_to_hal(ctrl->id,
						ctrl->val);
		profile_level.level = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = venc_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.profile = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_LEVEL);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = venc_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.level = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		dprintk(VIDC_DBG, "\nprofile: %d\n",
			   profile_level.profile);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_PROFILE);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = venc_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.profile = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		dprintk(VIDC_DBG, "\nLevel: %d\n",
			   profile_level.level);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = venc_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.level = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_H263_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = venc_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.profile = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_H263_PROFILE,
				ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
				ctrl->val);
		profile_level.level = HAL_VPX_PROFILE_UNUSED;
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE:
		temp_ctrl =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.profile = venc_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.level = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE);

		property_id = HAL_PARAM_PROFILE_LEVEL_CURRENT;
		profile_level.level = venc_v4l2_to_hal(ctrl->id,
							ctrl->val);
		profile_level.profile = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE,
				temp_ctrl->val);
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION:
	{
		struct v4l2_ctrl *deinterlace = NULL;

		if (!(inst->capability.pixelprocess_capabilities &
			HAL_VIDEO_ENCODER_ROTATION_CAPABILITY)) {
			dprintk(VIDC_ERR, "Rotation not supported: %#x\n",
				ctrl->id);
			rc = -ENOTSUPP;
			break;
		}
		deinterlace =
			TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE);
		if (ctrl->val && deinterlace && deinterlace->val !=
				V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE_DISABLED) {
			dprintk(VIDC_ERR,
				"Rotation not supported with deinterlacing\n");
			rc = -EINVAL;
			break;
		}
		property_id = HAL_CONFIG_VPE_OPERATIONS;
		operations.rotate = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_ROTATION,
				ctrl->val);
		operations.flip = HAL_FLIP_NONE;
		pdata = &operations;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP: {
		struct v4l2_ctrl *qpp, *qpb;

		rc = msm_venc_validate_qp_value(inst, ctrl);
		if (rc)
			break;
		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP);
		qpb = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpi = ctrl->val;
		quantization.qpp = qpp->val;
		quantization.qpb = qpb->val;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP: {
		struct v4l2_ctrl *qpi, *qpb;

		rc = msm_venc_validate_qp_value(inst, ctrl);
		if (rc)
			break;
		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP);
		qpb = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpp = ctrl->val;
		quantization.qpi = qpi->val;
		quantization.qpb = qpb->val;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP: {
		struct v4l2_ctrl *qpi, *qpp;

		rc = msm_venc_validate_qp_value(inst, ctrl);
		if (rc)
			break;
		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP);
		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpb = ctrl->val;
		quantization.qpi = qpi->val;
		quantization.qpp = qpp->val;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP: {
		struct v4l2_ctrl *qpp, *qpb;

		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP);
		qpb = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpi = ctrl->val;
		quantization.qpp = qpp->val;
		quantization.qpb = qpb->val;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP: {
		struct v4l2_ctrl *qpi, *qpb;

		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP);
		qpb = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpp = ctrl->val;
		quantization.qpi = qpi->val;
		quantization.qpb = qpb->val;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H263_B_FRAME_QP: {
		struct v4l2_ctrl *qpi, *qpp;

		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP);
		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpb = ctrl->val;
		quantization.qpi = qpi->val;
		quantization.qpp = qpp->val;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_VPX_I_FRAME_QP: {
		struct v4l2_ctrl *qpp;

		qpp = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_VPX_P_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpi = ctrl->val;
		quantization.qpp = qpp->val;
		/* Bframes are not supported for VPX */
		quantization.qpb = 0;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_VPX_P_FRAME_QP: {
		struct v4l2_ctrl *qpi;

		qpi = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_VPX_I_FRAME_QP);

		property_id = HAL_PARAM_VENC_SESSION_QP;
		quantization.qpp = ctrl->val;
		quantization.qpi = qpi->val;
		/* Bframes are not supported for VPX */
		quantization.qpb = 0;
		quantization.layer_id = 0;

		pdata = &quantization;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP: {
		struct v4l2_ctrl *qp_max;

		qp_max = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_MAX_QP);
		if (ctrl->val >= qp_max->val) {
			dprintk(VIDC_ERR,
					"Bad range: Min QP (%d) > Max QP(%d)\n",
					ctrl->val, qp_max->val);
			rc = -ERANGE;
			break;
		}

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = qp_max->val;
		qp_range.min_qp = ctrl->val;

		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP: {
		struct v4l2_ctrl *qp_min;

		qp_min = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_H264_MIN_QP);
		if (ctrl->val <= qp_min->val) {
			dprintk(VIDC_ERR,
					"Bad range: Max QP (%d) < Min QP(%d)\n",
					ctrl->val, qp_min->val);
			rc = -ERANGE;
			break;
		}

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = ctrl->val;
		qp_range.min_qp = qp_min->val;

		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP: {
		struct v4l2_ctrl *qp_max;

		qp_max = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP);
		if (ctrl->val >= qp_max->val) {
			dprintk(VIDC_ERR,
					"Bad range: Min QP (%d) > Max QP(%d)\n",
					ctrl->val, qp_max->val);
			rc = -ERANGE;
			break;
		}

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = qp_max->val;
		qp_range.min_qp = ctrl->val;

		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP: {
		struct v4l2_ctrl *qp_min;

		qp_min = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP);
		if (ctrl->val <= qp_min->val) {
			dprintk(VIDC_ERR,
					"Bad range: Max QP (%d) < Min QP(%d)\n",
					ctrl->val, qp_min->val);
			rc = -ERANGE;
			break;
		}

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = ctrl->val;
		qp_range.min_qp = qp_min->val;

		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_VPX_MIN_QP: {
		struct v4l2_ctrl *qp_max;

		qp_max = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_VPX_MAX_QP);
		if (ctrl->val >= qp_max->val) {
			dprintk(VIDC_ERR,
					"Bad range: Min QP (%d) > Max QP(%d)\n",
					ctrl->val, qp_max->val);
			rc = -ERANGE;
			break;
		}
		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = qp_max->val;
		qp_range.min_qp = ctrl->val;
		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_VPX_MAX_QP: {
		struct v4l2_ctrl *qp_min;

		qp_min = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_VPX_MIN_QP);
		if (ctrl->val <= qp_min->val) {
			dprintk(VIDC_ERR,
					"Bad range: Max QP (%d) < Min QP(%d)\n",
					ctrl->val, qp_min->val);
			rc = -ERANGE;
			break;
		}
		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = ctrl->val;
		qp_range.min_qp = qp_min->val;
		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_MIN_QP: {
		struct v4l2_ctrl *qp_max;

		qp_max = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_VP8_MAX_QP);
		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = qp_max->val;
		qp_range.min_qp = ctrl->val;
		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_MAX_QP: {
		struct v4l2_ctrl *qp_min;

		qp_min = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_VP8_MIN_QP);
		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE;
		qp_range.layer_id = 0;
		qp_range.max_qp = ctrl->val;
		qp_range.min_qp = qp_min->val;
		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_MIN_QP_PACKED: {
		struct v4l2_ctrl *qp_max;

		qp_max = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MAX_QP_PACKED);
		if (ctrl->val >= qp_max->val) {
			dprintk(VIDC_ERR,
					"Bad range: Min QP PACKED (0x%x) > Max QP PACKED (0x%x)\n",
					ctrl->val, qp_max->val);
			rc = -ERANGE;
			break;
		}

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE_PACKED;
		qp_range.layer_id = 0;
		qp_range.max_qp = qp_max->val;
		qp_range.min_qp = ctrl->val;

		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_MAX_QP_PACKED: {
		struct v4l2_ctrl *qp_min;

		qp_min = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MIN_QP_PACKED);
		if (ctrl->val <= qp_min->val) {
			dprintk(VIDC_ERR,
					"Bad range: Max QP PACKED (%d) < Min QP PACKED (%d)\n",
					ctrl->val, qp_min->val);
			rc = -ERANGE;
			break;
		}

		property_id = HAL_PARAM_VENC_SESSION_QP_RANGE_PACKED;
		qp_range.layer_id = 0;
		qp_range.max_qp = ctrl->val;
		qp_range.min_qp = qp_min->val;

		pdata = &qp_range;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE: {
		int temp = 0;

		switch (ctrl->val) {
		case V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB:
			temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
			break;
		case V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES:
			temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES;
			break;
		case V4L2_MPEG_VIDEO_MULTI_SLICE_GOB:
			temp = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_GOB;
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
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_GOB:
		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);

		property_id = HAL_PARAM_VENC_MULTI_SLICE_CONTROL;
		multi_slice_control.multi_slice = temp_ctrl->val;
		multi_slice_control.slice_size = ctrl->val;
		pdata = &multi_slice_control;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_DELIVERY_MODE: {
		bool codec_avc =
			inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264 ||
			inst->fmts[CAPTURE_PORT].fourcc ==
							V4L2_PIX_FMT_H264_NO_SC;

		temp_ctrl = TRY_GET_CTRL(V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE);
		if (codec_avc && temp_ctrl->val ==
				V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) {
			property_id = HAL_PARAM_VENC_SLICE_DELIVERY_MODE;
			enable.enable = true;
		} else {
			dprintk(VIDC_WARN,
			"Failed : slice delivery mode is not supported\n");
			enable.enable = false;
		}
		pdata = &enable;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE: {
		struct v4l2_ctrl *air_mbs, *air_ref, *cir_mbs;
		bool is_cont_intra_supported = false;

		air_mbs = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS);
		air_ref = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF);
		cir_mbs = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_CIR_MBS);

		is_cont_intra_supported =
		(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264) ||
		(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC);

		if (is_cont_intra_supported) {
			if (ctrl->val != HAL_INTRA_REFRESH_NONE)
				enable.enable = true;
			else
				enable.enable = false;

			rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session,
				HAL_PARAM_VENC_CONSTRAINED_INTRA_PRED, &enable);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to set constrained intra\n");
				rc = -EINVAL;
				break;
			}
		}

		property_id = HAL_PARAM_VENC_INTRA_REFRESH;
		intra_refresh.mode = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE,
				ctrl->val);

		intra_refresh.air_mbs = air_mbs->val;
		intra_refresh.air_ref = air_ref->val;
		intra_refresh.cir_mbs = cir_mbs->val;

		pdata = &intra_refresh;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS: {
		struct v4l2_ctrl *ir_mode, *air_ref, *cir_mbs;

		ir_mode = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE);
		air_ref = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF);
		cir_mbs = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_CIR_MBS);

		property_id = HAL_PARAM_VENC_INTRA_REFRESH;

		intra_refresh.air_mbs = ctrl->val;
		intra_refresh.mode = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE,
				ir_mode->val);
		intra_refresh.air_ref = air_ref->val;
		intra_refresh.cir_mbs = cir_mbs->val;

		pdata = &intra_refresh;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF: {
		struct v4l2_ctrl *ir_mode, *air_mbs, *cir_mbs;

		ir_mode = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE);
		air_mbs = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS);
		cir_mbs = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_CIR_MBS);

		property_id = HAL_PARAM_VENC_INTRA_REFRESH;

		intra_refresh.air_ref = ctrl->val;
		intra_refresh.air_mbs = air_mbs->val;
		intra_refresh.mode = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE,
				ir_mode->val);
		intra_refresh.cir_mbs = cir_mbs->val;

		pdata = &intra_refresh;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_CIR_MBS: {
		struct v4l2_ctrl *ir_mode, *air_mbs, *air_ref;

		ir_mode = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE);
		air_mbs = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS);
		air_ref = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF);

		property_id = HAL_PARAM_VENC_INTRA_REFRESH;

		intra_refresh.cir_mbs = ctrl->val;
		intra_refresh.air_mbs = air_mbs->val;
		intra_refresh.air_ref = air_ref->val;
		intra_refresh.mode = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE,
				ir_mode->val);

		pdata = &intra_refresh;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB: {
		struct v4l2_ctrl *air_mbs, *air_ref;

		air_mbs = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS);
		air_ref = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF);

		property_id = HAL_PARAM_VENC_INTRA_REFRESH;

		intra_refresh.cir_mbs = ctrl->val;
		intra_refresh.air_mbs = air_mbs->val;
		intra_refresh.air_ref = air_ref->val;
		intra_refresh.mode = HAL_INTRA_REFRESH_CYCLIC;

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
		h264_db_control.mode = venc_v4l2_to_hal(
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
		h264_db_control.mode = venc_v4l2_to_hal(
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
		h264_db_control.mode = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE,
				mode->val);
		pdata = &h264_db_control;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		property_id = HAL_PARAM_VENC_SYNC_FRAME_SEQUENCE_HEADER;

		switch (ctrl->val) {
		case V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE:
			enable.enable = false;
			break;
		case V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_I_FRAME:
			enable.enable = true;
			break;
		case V4L2_MPEG_VIDEO_HEADER_MODE_JOINED_WITH_1ST_FRAME:
		default:
			rc = -ENOTSUPP;
			break;
		}
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags |= VIDC_SECURE;
		dprintk(VIDC_INFO, "Setting secure mode to: %d\n",
				!!(inst->flags & VIDC_SECURE));
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		property_id = HAL_PARAM_INDEX_EXTRADATA;
		extra.index = msm_comm_get_hal_extradata_index(ctrl->val);
		extra.enable = 1;
		pdata = &extra;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_VUI_TIMING_INFO:
	{
		struct v4l2_ctrl *rc_mode;
		bool cfr = false;

		property_id = HAL_PARAM_VENC_H264_VUI_TIMING_INFO;
		rc_mode = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL);

		switch (rc_mode->val) {
		case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR:
		case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_CFR:
			cfr = true;
			break;
		default:
			cfr = false;
			break;
		}

		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_H264_VUI_TIMING_INFO_DISABLED:
			vui_timing_info.enable = 0;
			break;
		case V4L2_MPEG_VIDC_VIDEO_H264_VUI_TIMING_INFO_ENABLED:
			vui_timing_info.enable = 1;
			vui_timing_info.fixed_frame_rate = cfr;
			vui_timing_info.time_scale = NSEC_PER_SEC;
		}

		pdata = &vui_timing_info;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_AU_DELIMITER:
		property_id = HAL_PARAM_VENC_GENERATE_AUDNAL;

		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_AU_DELIMITER_DISABLED:
			enable.enable = false;
			break;
		case V4L2_MPEG_VIDC_VIDEO_AU_DELIMITER_ENABLED:
			enable.enable = true;
			break;
		default:
			rc = -ENOTSUPP;
			break;
		}

		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_SET_PERF_LEVEL:
		switch (ctrl->val) {
		case V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL:
			if (inst->flags & VIDC_TURBO) {
				inst->flags &= ~VIDC_TURBO;
				msm_dcvs_init_load(inst);
			}
			break;
		case V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO:
			inst->flags |= VIDC_TURBO;
			break;
		default:
			dprintk(VIDC_ERR, "Perf mode %x not supported\n",
					ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		msm_comm_scale_clocks_and_bus(inst);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_VUI_BITSTREAM_RESTRICT:
		property_id = HAL_PARAM_VENC_H264_VUI_BITSTREAM_RESTRC;
		vui_bitstream_restrict.enable = ctrl->val;
		pdata = &vui_bitstream_restrict;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PRESERVE_TEXT_QUALITY:
		property_id = HAL_PARAM_VENC_PRESERVE_TEXT_QUALITY;
		preserve_text_quality.enable = ctrl->val;
		pdata = &preserve_text_quality;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG4_TIME_RESOLUTION:
		property_id = HAL_PARAM_VENC_MPEG4_TIME_RESOLUTION;
		time_res.time_increment_resolution = ctrl->val;
		pdata = &time_res;
		break;

	case V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE:
	{
		struct v4l2_ctrl *rotation = NULL;

		if (!(inst->capability.pixelprocess_capabilities &
			HAL_VIDEO_ENCODER_DEINTERLACE_CAPABILITY)) {
			dprintk(VIDC_ERR, "Deinterlace not supported: %#x\n",
					ctrl->id);
			rc = -ENOTSUPP;
			break;
		}
		rotation = TRY_GET_CTRL(V4L2_CID_MPEG_VIDC_VIDEO_ROTATION);
		if (ctrl->val && rotation && rotation->val !=
			V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_NONE) {
			dprintk(VIDC_ERR,
				"Deinterlacing not supported with rotation");
			rc = -EINVAL;
			break;
		}
		property_id = HAL_CONFIG_VPE_DEINTERLACE;
		switch (ctrl->val) {
		case V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE_ENABLED:
			enable.enable = true;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_DEINTERLACE_DISABLED:
		default:
			enable.enable = false;
			break;
		}
		pdata = &enable;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_SEQ_HEADER:
		atomic_inc(&inst->seq_hdr_reqs);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_USELTRFRAME:
		property_id = HAL_CONFIG_VENC_USELTRFRAME;
		use_ltr.ref_ltr = ctrl->val;
		use_ltr.use_constraint = true;
		use_ltr.frames = 0;
		pdata = &use_ltr;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MARKLTRFRAME:
		if (ctrl->val < inst->capability.ltr_count.min ||
			ctrl->val >= inst->capability.ltr_count.max) {
			dprintk(VIDC_ERR,
				"Error setting markltr %d range: [%d,%d)\n",
				ctrl->val, inst->capability.ltr_count.min,
				inst->capability.ltr_count.max);
			rc = -ENOTSUPP;
			break;
		}
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
	case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE:
		property_id = HAL_PARAM_VENC_DISABLE_RC_TIMESTAMP;
		enable.enable = (ctrl->val ==
		V4L2_MPEG_VIDC_VIDEO_RATE_CONTROL_TIMESTAMP_MODE_IGNORE);
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VPX_ERROR_RESILIENCE:
		property_id = HAL_PARAM_VENC_VPX_ERROR_RESILIENCE_MODE;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_NAL_SVC:
		property_id = HAL_PARAM_VENC_H264_NAL_SVC_EXT;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PERF_MODE:
		property_id = HAL_CONFIG_VENC_PERF_MODE;

		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_PERF_POWER_SAVE:
			inst->flags |= VIDC_LOW_POWER;
			venc_mode = HAL_PERF_MODE_POWER_SAVE;
			break;
		case V4L2_MPEG_VIDC_VIDEO_PERF_MAX_QUALITY:
			inst->flags &= ~VIDC_LOW_POWER;
			venc_mode = HAL_PERF_MODE_POWER_MAX_QUALITY;
			break;
		default:
			dprintk(VIDC_ERR, "Power save mode %x not supported\n",
					ctrl->val);
			rc = -ENOTSUPP;
			property_id = 0;
			break;
		}
		pdata = &venc_mode;

		msm_dcvs_enc_set_power_save_mode(inst,
			ctrl->val == V4L2_MPEG_VIDC_VIDEO_PERF_POWER_SAVE);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HIER_B_NUM_LAYERS:
		if (inst->fmts[CAPTURE_PORT].fourcc != V4L2_PIX_FMT_HEVC) {
			dprintk(VIDC_ERR, "Hier B supported for HEVC only\n");
			rc = -ENOTSUPP;
			break;
		}
		property_id = HAL_PARAM_VENC_HIER_B_MAX_ENH_LAYERS;
		hier_b_layers = ctrl->val;
		pdata = &hier_b_layers;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HYBRID_HIERP_MODE:
		property_id = HAL_PARAM_VENC_HIER_P_HYBRID_MODE;
		hyb_hierp.layers = ctrl->val;
		pdata = &hyb_hierp;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MBI_STATISTICS_MODE:
		property_id = HAL_PARAM_VENC_MBI_STATISTICS_MODE;
		mbi_statistics_mode = venc_v4l2_to_hal(
			V4L2_CID_MPEG_VIDC_VIDEO_MBI_STATISTICS_MODE,
			ctrl->val);
		pdata = &mbi_statistics_mode;
		break;
	case V4L2_CID_VIDC_QBUF_MODE:
		property_id = HAL_PARAM_SYNC_BASED_INTERRUPT;
		enable.enable = ctrl->val == V4L2_VIDC_QBUF_BATCHED;
		pdata = &enable;
		break;
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
	case V4L2_CID_MPEG_VIDC_VIDEO_CONFIG_QP:
		/* Sanity check for the QP boundaries as we are using
		 * same control to set dynamic QP for all the codecs
		 */
		rc = msm_venc_validate_qp_value(inst, ctrl);
		if (rc) {
			dprintk(VIDC_ERR, "Invalid QP Config QP Range\n");
			break;
		}
		property_id = HAL_CONFIG_VENC_FRAME_QP;
		frameqp = ctrl->val;
		pdata = &frameqp;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_I_FRAME_QP:
	{
		rc = msm_venc_validate_qp_value(inst, ctrl);
		if (rc) {
			dprintk(VIDC_ERR, "Invalid Initial I QP\n");
			break;
		}
		/*
		 * Defer sending property from here, set_ext_ctrl
		 * will send it based on the rc value.
		 */
		property_id = 0;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_B_FRAME_QP:
	{
		rc = msm_venc_validate_qp_value(inst, ctrl);
		if (rc) {
			dprintk(VIDC_ERR, "Invalid Initial B QP\n");
			break;
		}
		/*
		 * Defer sending property from here, set_ext_ctrl
		 * will send it based on the rc value.
		 */
		property_id = 0;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_P_FRAME_QP:
	{
		rc = msm_venc_validate_qp_value(inst, ctrl);
		if (rc) {
			dprintk(VIDC_ERR, "Invalid Initial P QP\n");
			break;
		}
		/*
		 * Defer sending property from here, set_ext_ctrl
		 * will send it based on the rc value.
		 */
		property_id = 0;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_VQZIP_SEI:
		property_id = HAL_PARAM_VENC_VQZIP_SEI;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
		property_id = HAL_CONFIG_REALTIME;
		/* firmware has inverted values for realtime and
		 * non-realtime priority
		 */
		enable.enable = !(ctrl->val);
		pdata = &enable;
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
			inst, inst->prop.operating_rate >> 16,
				ctrl->val >> 16);
		inst->prop.operating_rate = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VENC_BITRATE_TYPE:
	{
		property_id = HAL_PARAM_VENC_BITRATE_TYPE;
		enable.enable = ctrl->val;
		pdata = &enable;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_PIC_ORDER_CNT:
	{
		property_id = HAL_PARAM_VENC_H264_PIC_ORDER_CNT;
		pic_order_cnt = ctrl->val;
		pdata = &pic_order_cnt;
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
		if (ctrl->val == V4L2_CID_MPEG_VIDC_VIDEO_VPE_CSC_ENABLE) {
			rc = msm_venc_set_csc(inst);
			if (rc)
				dprintk(VIDC_ERR, "fail to set csc: %d\n", rc);
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
	{
		property_id = HAL_PARAM_VENC_LOW_LATENCY;
		if (ctrl->val ==
			V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_ENABLE)
			enable.enable = true;
		else
			enable.enable = false;
		pdata = &enable;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_TRANSFORM_8x8:
		property_id = HAL_PARAM_VENC_H264_TRANSFORM_8x8;
		switch (ctrl->val) {
		case V4L2_MPEG_VIDC_VIDEO_H264_TRANSFORM_8x8_ENABLE:
			enable.enable = true;
			break;
		case V4L2_MPEG_VIDC_VIDEO_H264_TRANSFORM_8x8_DISABLE:
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
	case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_TYPE:
		property_id = HAL_PARAM_VENC_IFRAMESIZE_TYPE;
		iframesize_type = venc_v4l2_to_hal(
				V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_SIZE_TYPE,
				ctrl->val);
		pdata = &iframesize_type;
		break;
	default:
		dprintk(VIDC_ERR, "Unsupported index: %x\n", ctrl->id);
		rc = -ENOTSUPP;
		break;
	}

	v4l2_ctrl_lock(ctrl);

	if (!rc && property_id) {
		dprintk(VIDC_DBG, "Control: HAL property=%x,ctrl_value=%d\n",
				property_id,
				ctrl->val);
		rc = call_hfi_op(hdev, session_set_property,
				(void *)inst->session, property_id, pdata);
	}

	return rc;
}
#undef TRY_GET_CTRL

static int try_set_ext_ctrl(struct msm_vidc_inst *inst,
	struct v4l2_ext_controls *ctrl)
{
	int rc = 0, i;
	struct v4l2_ext_control *control;
	struct hfi_device *hdev;
	struct hal_ltr_mode ltr_mode;
	struct hal_vc1e_perf_cfg_type search_range = { {0} };
	u32 property_id = 0;
	void *pdata = NULL;
	struct msm_vidc_capability *cap = NULL;
	struct hal_initial_quantization quant;
	struct hal_aspect_ratio sar;
	struct hal_bitrate bitrate;
	struct hal_frame_size blur_res;
	struct v4l2_control temp_ctrl;

	if (!inst || !inst->core || !inst->core->device || !ctrl) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	cap = &inst->capability;

	control = ctrl->controls;
	for (i = 0; i < ctrl->count; i++) {
		switch (control[i].id) {
		case V4L2_CID_MPEG_VIDC_VIDEO_LTRMODE:
			if (control[i].value !=
				V4L2_MPEG_VIDC_VIDEO_LTR_MODE_DISABLE) {
				rc = msm_venc_toggle_hier_p(inst, false);
				if (rc)
					break;
			}
			ltr_mode.mode = control[i].value;
			ltr_mode.trust_mode = 1;
			property_id = HAL_PARAM_VENC_LTRMODE;
			pdata = &ltr_mode;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT:
			ltr_mode.count =  control[i].value;
			if (ltr_mode.count > cap->ltr_count.max) {
				dprintk(VIDC_ERR,
					"Invalid LTR count %d. Supported max: %d\n",
					ltr_mode.count,
					cap->ltr_count.max);
				/*
				 * FIXME: Return an error (-EINVALID)
				 * here once VP8 supports LTR count
				 * capability
				 */
				ltr_mode.count = 1;
			}
			ltr_mode.trust_mode = 1;
			property_id = HAL_PARAM_VENC_LTRMODE;
			pdata = &ltr_mode;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_INITIAL_QP:
			property_id = HAL_PARAM_VENC_ENABLE_INITIAL_QP;
			quant.init_qp_enable = control[i].value;
			pdata = &quant;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_I_FRAME_QP:
			quant.qpi = control[i].value;
			property_id = HAL_PARAM_VENC_ENABLE_INITIAL_QP;
			pdata = &quant;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_P_FRAME_QP:
			quant.qpp = control[i].value;
			property_id = HAL_PARAM_VENC_ENABLE_INITIAL_QP;
			pdata = &quant;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_B_FRAME_QP:
			quant.qpb = control[i].value;
			property_id = HAL_PARAM_VENC_ENABLE_INITIAL_QP;
			pdata = &quant;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_I_FRAME_QP:
			/* Sanity check for the QP boundaries as we are using
			 * same control to set Initial QP for all the codecs
			 */
			temp_ctrl.id =
				V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_I_FRAME_QP;
			temp_ctrl.value = control[i].value;

			rc = msm_comm_s_ctrl(inst, &temp_ctrl);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s Failed setting Initial I Frame QP : %d\n",
					__func__, rc);
				break;
			}
			quant.qpi = control[i].value;
			property_id = HAL_PARAM_VENC_ENABLE_INITIAL_QP;
			pdata = &quant;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_P_FRAME_QP:
			temp_ctrl.id =
				V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_P_FRAME_QP;
			temp_ctrl.value = control[i].value;
			rc = msm_comm_s_ctrl(inst, &temp_ctrl);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s Failed setting Initial P Frame QP : %d\n",
					__func__, rc);
				break;
			}
			quant.qpp = control[i].value;
			property_id = HAL_PARAM_VENC_ENABLE_INITIAL_QP;
			pdata = &quant;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_B_FRAME_QP:
			temp_ctrl.id =
				V4L2_CID_MPEG_VIDC_VIDEO_INITIAL_B_FRAME_QP;
			temp_ctrl.value = control[i].value;
			rc = msm_comm_s_ctrl(inst, &temp_ctrl);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s Failed setting Initial B Frame QP : %d\n",
					__func__, rc);
				break;
			}
			quant.qpb = control[i].value;
			property_id = HAL_PARAM_VENC_ENABLE_INITIAL_QP;
			pdata = &quant;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_X_RANGE:
			search_range.i_frame.x_subsampled = control[i].value;
			property_id = HAL_PARAM_VENC_SEARCH_RANGE;
			pdata = &search_range;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_IFRAME_Y_RANGE:
			search_range.i_frame.y_subsampled = control[i].value;
			property_id = HAL_PARAM_VENC_SEARCH_RANGE;
			pdata = &search_range;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_PFRAME_X_RANGE:
			search_range.p_frame.x_subsampled = control[i].value;
			property_id = HAL_PARAM_VENC_SEARCH_RANGE;
			pdata = &search_range;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_PFRAME_Y_RANGE:
			search_range.p_frame.y_subsampled = control[i].value;
			property_id = HAL_PARAM_VENC_SEARCH_RANGE;
			pdata = &search_range;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_BFRAME_X_RANGE:
			search_range.b_frame.x_subsampled = control[i].value;
			property_id = HAL_PARAM_VENC_SEARCH_RANGE;
			pdata = &search_range;
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_BFRAME_Y_RANGE:
			search_range.b_frame.y_subsampled = control[i].value;
			property_id = HAL_PARAM_VENC_SEARCH_RANGE;
			pdata = &search_range;
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
		case V4L2_CID_MPEG_VIDC_VENC_PARAM_LAYER_BITRATE:
		{
			if (control[i].value) {
				bitrate.layer_id = i;
				bitrate.bit_rate = control[i].value;
				property_id = HAL_CONFIG_VENC_TARGET_BITRATE;
				pdata = &bitrate;
				dprintk(VIDC_DBG, "bitrate for layer(%d)=%d\n",
					i, bitrate.bit_rate);
				rc = call_hfi_op(hdev, session_set_property,
					(void *)inst->session, property_id,
					 pdata);
				if (rc) {
					dprintk(VIDC_DBG, "prop %x failed\n",
						property_id);
					return rc;
				}
				if (i == MAX_HYBRID_HIER_P_LAYERS - 1) {
					dprintk(VIDC_DBG, "HAL property=%x\n",
						property_id);
					property_id = 0;
					rc = 0;
				}
			}
			break;
		}
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

static int msm_venc_op_s_ctrl(struct v4l2_ctrl *ctrl)
{

	int rc = 0, c = 0;

	struct msm_vidc_inst *inst = container_of(ctrl->handler,
					struct msm_vidc_inst, ctrl_handler);

	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);

	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
		goto failed_open_done;
	}

	for (c = 0; c < ctrl->ncontrols; ++c) {
		if (ctrl->cluster[c]->is_new) {
			struct v4l2_ctrl *temp = ctrl->cluster[c];

			rc = try_set_ctrl(inst, temp);
			if (rc) {
				dprintk(VIDC_ERR, "Failed setting %s (%x)\n",
						v4l2_ctrl_get_name(temp->id),
						temp->id);
				break;
			}
		}
	}
failed_open_done:
	if (rc)
		dprintk(VIDC_ERR, "Failed setting control: %x (%s)",
				ctrl->id, v4l2_ctrl_get_name(ctrl->id));
	return rc;
}

static int msm_venc_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops msm_venc_ctrl_ops = {

	.s_ctrl = msm_venc_op_s_ctrl,
	.g_volatile_ctrl = msm_venc_op_g_volatile_ctrl,
};

const struct v4l2_ctrl_ops *msm_venc_get_ctrl_ops(void)
{
	return &msm_venc_ctrl_ops;
}

int msm_venc_inst_init(struct msm_vidc_inst *inst)
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
	inst->capability.alloc_mode_in = HAL_BUFFER_MODE_STATIC;
	inst->capability.alloc_mode_out = HAL_BUFFER_MODE_STATIC;
	inst->capability.secure_output2_threshold.min = 0;
	inst->capability.secure_output2_threshold.max = 0;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->prop.fps = DEFAULT_FPS;
	inst->prop.operating_rate = 0;
	inst->capability.pixelprocess_capabilities = 0;
	memcpy(&inst->fmts[CAPTURE_PORT], &venc_formats[4],
						sizeof(struct msm_vidc_format));
	memcpy(&inst->fmts[OUTPUT_PORT], &venc_formats[0],
						sizeof(struct msm_vidc_format));
	return rc;
}

int msm_venc_s_ext_ctrl(struct msm_vidc_inst *inst,
	struct v4l2_ext_controls *ctrl)
{
	int rc = 0;

	rc = try_set_ext_ctrl(inst, ctrl);
	if (rc) {
		dprintk(VIDC_ERR, "Error setting extended control\n");
		return rc;
	}
	return rc;
}

int msm_venc_querycap(struct msm_vidc_inst *inst, struct v4l2_capability *cap)
{
	if (!inst || !cap) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, cap = %pK\n", inst, cap);
		return -EINVAL;
	}
	strlcpy(cap->driver, MSM_VIDC_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_VENC_DVC_NAME, sizeof(cap->card));
	cap->bus_info[0] = 0;
	//cap->version = MSM_VIDC_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
						V4L2_CAP_VIDEO_OUTPUT_MPLANE |
						V4L2_CAP_STREAMING;
	memset(cap->reserved, 0, sizeof(cap->reserved));
	return 0;
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

static int msm_venc_set_csc(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int count = 0;
	struct hal_vpe_color_space_conversion vpe_csc;

	while (count < HAL_MAX_MATRIX_COEFFS) {
		if (count < HAL_MAX_BIAS_COEFFS)
			vpe_csc.csc_bias[count] =
				vpe_csc_601_to_709_bias_coeff[count];
		if (count < HAL_MAX_LIMIT_COEFFS)
			vpe_csc.csc_limit[count] =
				vpe_csc_601_to_709_limit_coeff[count];
		vpe_csc.csc_matrix[count] =
			vpe_csc_601_to_709_matrix_coeff[count];
		count = count + 1;
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
	int rc = 0;
	int i;
	struct hfi_device *hdev;
	struct msm_vidc_format_constraint *fmt_constraint;

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

		msm_venc_update_plane_count(inst, CAPTURE_PORT);
		fmt->num_planes = inst->fmts[CAPTURE_PORT].num_planes;

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
		dprintk(VIDC_DBG, "width = %d, height = %d\n",
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
		memcpy(&inst->fmts[fmt->type], fmt,
						sizeof(struct msm_vidc_format));

		msm_venc_update_plane_count(inst, OUTPUT_PORT);
		fmt->num_planes = inst->fmts[OUTPUT_PORT].num_planes;

		rc = msm_comm_set_color_format(inst, HAL_BUFFER_INPUT,
						fmt->fourcc);
		if (rc) {
			dprintk(VIDC_ERR, "%s: set color format (%#x) failed\n",
				__func__, fmt->fourcc);
			return rc;
		}

		fmt_constraint = msm_comm_get_pixel_fmt_constraints(
					enc_pix_format_constraints,
					ARRAY_SIZE(enc_pix_format_constraints),
					fmt->fourcc);
		if (fmt_constraint) {
			rc = msm_comm_set_color_format_constraints(inst,
				msm_comm_get_hal_output_buffer(inst),
				fmt_constraint);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s: Set constraints for color format %#x failed\n",
					__func__, fmt->fourcc);
				return rc;
			}
		}
	} else {
		dprintk(VIDC_ERR, "%s - Unsupported buf type: %d\n",
			__func__, f->type);
		rc = -EINVAL;
		goto exit;
	}

	f->fmt.pix_mp.num_planes = fmt->num_planes;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		struct hal_frame_size frame_sz = {0};
		struct hal_buffer_requirements *bufreq = NULL;

		frame_sz.width = inst->prop.width[CAPTURE_PORT];
		frame_sz.height = inst->prop.height[CAPTURE_PORT];
		frame_sz.buffer_type = HAL_BUFFER_OUTPUT;
		rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, HAL_PARAM_FRAME_SIZE,
				&frame_sz);
		if (rc) {
			dprintk(VIDC_ERR,
					"Failed to set OUTPUT framesize\n");
			goto exit;
		}
		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_WARN,
				"%s : Getting buffer reqs failed: %d\n",
					__func__, rc);
			goto exit;
		}
		bufreq = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
		f->fmt.pix_mp.plane_fmt[0].sizeimage =
			bufreq ? bufreq->buffer_size : 0;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		struct hal_buffer_requirements *bufreq = NULL;
		int extra_idx = 0;

		for (i = 0; i < inst->fmts[fmt->type].num_planes; ++i) {
			f->fmt.pix_mp.plane_fmt[i].sizeimage =
				inst->fmts[fmt->type].get_frame_size(i,
				f->fmt.pix_mp.height, f->fmt.pix_mp.width);
		}
		extra_idx = EXTRADATA_IDX(inst->fmts[fmt->type].num_planes);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_INPUT);
			f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
				bufreq ? bufreq->buffer_size : 0;
		}
	}
exit:
	return rc;
}

int msm_venc_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;
	int i;
	u32 height, width, num_planes;
	unsigned int extra_idx = 0;
	struct hal_buffer_requirements *bufreq = NULL;

	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, format = %pK\n", inst, f);
		return -EINVAL;
	}

	rc = msm_comm_try_get_bufreqs(inst);
	if (rc) {
		dprintk(VIDC_WARN, "Getting buffer requirements failed: %d\n",
				rc);
		return rc;
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = &inst->fmts[CAPTURE_PORT];
		height = inst->prop.height[CAPTURE_PORT];
		width = inst->prop.width[CAPTURE_PORT];
		msm_venc_update_plane_count(inst, CAPTURE_PORT);
		num_planes = inst->fmts[CAPTURE_PORT].num_planes;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = &inst->fmts[OUTPUT_PORT];
		height = inst->prop.height[OUTPUT_PORT];
		width = inst->prop.width[OUTPUT_PORT];
		msm_venc_update_plane_count(inst, OUTPUT_PORT);
		num_planes = inst->fmts[OUTPUT_PORT].num_planes;
	} else {
		dprintk(VIDC_ERR, "Invalid type: %x\n", f->type);
		return -ENOTSUPP;
	}

	f->fmt.pix_mp.pixelformat = fmt->fourcc;
	f->fmt.pix_mp.height = height;
	f->fmt.pix_mp.width = width;
	f->fmt.pix_mp.num_planes = num_planes;

	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		for (i = 0; i < num_planes; ++i) {
			f->fmt.pix_mp.plane_fmt[i].sizeimage =
				fmt->get_frame_size(i, height, width);
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		bufreq = get_buff_req_buffer(inst,
				HAL_BUFFER_OUTPUT);

		f->fmt.pix_mp.plane_fmt[0].sizeimage =
			bufreq ? bufreq->buffer_size : 0;
	}
	extra_idx = EXTRADATA_IDX(num_planes);
	if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			bufreq = get_buff_req_buffer(inst,
						HAL_BUFFER_EXTRADATA_OUTPUT);
		else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			bufreq = get_buff_req_buffer(inst,
						HAL_BUFFER_EXTRADATA_INPUT);

		f->fmt.pix_mp.plane_fmt[extra_idx].sizeimage =
			bufreq ? bufreq->buffer_size : 0;
	}

	for (i = 0; i < num_planes; ++i) {
		if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			inst->bufq[OUTPUT_PORT].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			inst->bufq[CAPTURE_PORT].plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
	}
	return rc;
}

int msm_venc_reqbufs(struct msm_vidc_inst *inst, struct v4l2_requestbuffers *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;

	if (!inst || !b) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, buffer = %pK\n", inst, b);
		return -EINVAL;
	}
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR,
		"Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_reqbufs(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_DBG, "Failed to get reqbufs, %d\n", rc);
	return rc;
}

int msm_venc_prepare_buf(struct msm_vidc_inst *inst,
					struct v4l2_buffer *b)
{
	int rc = 0;
	int i;
	struct vidc_buffer_addr_info buffer_info = {0};
	struct hfi_device *hdev;
	int extra_idx = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	if (inst->state == MSM_VIDC_CORE_INVALID ||
			inst->core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"Core %pK in bad state, ignoring prepare buf\n",
				inst->core);
		goto exit;
	}

	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (b->length != inst->fmts[CAPTURE_PORT].num_planes) {
			dprintk(VIDC_ERR,
				"Planes mismatch: needed: %d, allocated: %d\n",
				inst->fmts[CAPTURE_PORT].num_planes,
				b->length);
			rc = -EINVAL;
			break;
		}

		for (i = 0; i < min_t(int, b->length, VIDEO_MAX_PLANES); i++) {
			dprintk(VIDC_DBG, "device_addr = %#lx, size = %d\n",
				b->m.planes[i].m.userptr,
				b->m.planes[i].length);
		}
		buffer_info.buffer_size = b->m.planes[0].length;
		buffer_info.buffer_type = HAL_BUFFER_OUTPUT;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr =
			b->m.planes[0].m.userptr;

		extra_idx = EXTRADATA_IDX(b->length);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			buffer_info.extradata_addr =
				b->m.planes[extra_idx].m.userptr;
			dprintk(VIDC_DBG, "extradata: %#lx\n",
					b->m.planes[extra_idx].m.userptr);
			buffer_info.extradata_size =
				b->m.planes[extra_idx].length;
		}

		rc = call_hfi_op(hdev, session_set_buffers,
				(void *)inst->session, &buffer_info);
		if (rc)
			dprintk(VIDC_ERR,
					"vidc_hal_session_set_buffers failed\n");
		break;
	default:
		dprintk(VIDC_ERR,
			"Buffer type not recognized: %d\n", b->type);
		break;
	}
exit:
	return rc;
}

int msm_venc_release_buf(struct msm_vidc_inst *inst,
					struct v4l2_buffer *b)
{
	int i, rc = 0, extra_idx = 0;
	struct vidc_buffer_addr_info buffer_info = {0};
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to release res done state\n",
			inst);
		goto exit;
	}
	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE: {
		if (b->length !=
			inst->fmts[CAPTURE_PORT].num_planes) {
			dprintk(VIDC_ERR,
					"Planes mismatch: needed: %d, to release: %d\n",
					inst->fmts[CAPTURE_PORT].num_planes,
					b->length);
			rc = -EINVAL;
			break;
		}
		for (i = 0; i < b->length; i++) {
			dprintk(VIDC_DBG,
				"Release device_addr = %#lx, size = %d, %d\n",
				b->m.planes[i].m.userptr,
				b->m.planes[i].length, inst->state);
		}
		buffer_info.buffer_size = b->m.planes[0].length;
		buffer_info.buffer_type = HAL_BUFFER_OUTPUT;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr =
			b->m.planes[0].m.userptr;
		extra_idx = EXTRADATA_IDX(b->length);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES))
			buffer_info.extradata_addr =
			b->m.planes[extra_idx].m.userptr;
		buffer_info.response_required = false;
		rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
		if (rc)
			dprintk(VIDC_ERR,
					"vidc_hal_session_release_buffers failed\n");
		}
		break;
	default:
		dprintk(VIDC_ERR, "Buffer type not recognized: %d\n", b->type);
		break;
	}
exit:
	return rc;
}

int msm_venc_qbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;

	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}
	mutex_lock(&q->lock);
	rc = vb2_qbuf(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "Failed to qbuf, %d\n", rc);
	return rc;
}

int msm_venc_dqbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;

	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}
	mutex_lock(&q->lock);
	rc = vb2_dqbuf(&q->vb2_bufq, b, true);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_DBG, "Failed to dqbuf, %d\n", rc);
	return rc;
}

int msm_venc_streamon(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
{
	int rc = 0;
	struct buf_queue *q;

	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "Calling streamon\n");
	mutex_lock(&q->lock);
	rc = vb2_streamon(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "streamon failed on port: %d\n", i);
	return rc;
}

int msm_venc_streamoff(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
{
	int rc = 0;
	struct buf_queue *q;

	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "Calling streamoff on port: %d\n", i);
	mutex_lock(&q->lock);
	rc = vb2_streamoff(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "streamoff failed on port: %d\n", i);
	return rc;
}

int msm_venc_ctrl_init(struct msm_vidc_inst *inst)
{
	return msm_comm_ctrl_init(inst, msm_venc_ctrls,
			ARRAY_SIZE(msm_venc_ctrls), &msm_venc_ctrl_ops);
}
