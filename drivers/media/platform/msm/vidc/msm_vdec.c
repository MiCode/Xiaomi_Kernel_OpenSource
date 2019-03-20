// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <soc/qcom/scm.h>
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
#define MAX_VP9D_INST_COUNT 6

static const char *const vp8_profile_level[] = {
	"Unused",
	"0.0",
	"1.0",
	"2.0",
	"3.0",
	NULL
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER,
		.name = "Decode Order",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_MSM_VIDC_DISABLE,
		.maximum = V4L2_MPEG_MSM_VIDC_ENABLE,
		.default_value = V4L2_MPEG_MSM_VIDC_DISABLE,
		.step = 1,
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
		.menu_skip_mask = 0,
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
		.menu_skip_mask = 0,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
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
		(1 << V4L2_MPEG_VIDEO_VP9_PROFILE_1) |
		(1 << V4L2_MPEG_VIDEO_VP9_PROFILE_2)
		),
		.qmenu = NULL,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL,
		.name = "VP9 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED,
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61,
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
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_6) |
		(1 << V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61)
		),
		.qmenu = vp9_level,
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
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
		.flags = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
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
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE,
		.name = "Frame Rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = (MINIMUM_FPS << 16),
		.maximum = (MAXIMUM_FPS << 16),
		.default_value = (DEFAULT_FPS << 16),
		.step = 1,
		.menu_skip_mask = 0,
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
		.name = "Set Decoder Operating rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = (MINIMUM_FPS << 16),
		.maximum = INT_MAX,
		.default_value =  (DEFAULT_FPS << 16),
		.step = 1,
		.menu_skip_mask = 0,
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
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)

struct msm_vidc_format vdec_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.type = CAPTURE_PORT,
	},
	{
		.name = "YCbCr Semiplanar 4:2:0 10bit",
		.description = "Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS,
		.type = CAPTURE_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0",
		.description = "UBWC Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12_UBWC,
		.type = CAPTURE_PORT,
	},
	{
		.name = "UBWC YCbCr Semiplanar 4:2:0 10bit",
		.description = "UBWC Y/CbCr 4:2:0 10bit",
		.fourcc = V4L2_PIX_FMT_NV12_TP10_UBWC,
		.type = CAPTURE_PORT,
	},
	{
		.name = "Mpeg2",
		.description = "Mpeg2 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 6,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 8,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 8,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
		.input_min_count = 4,
		.output_min_count = 6,
	},
	{
		.name = "VP9",
		.description = "VP9 compressed format",
		.fourcc = V4L2_PIX_FMT_VP9,
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
		.y_max_stride = 8192,
		.y_buffer_alignment = 256,
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

int msm_vdec_update_stream_output_mode(struct msm_vidc_inst *inst)
{
	u32 format;
	u32 stream_output_mode;
	u32 fourcc;

	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	format = inst->fmts[CAPTURE_PORT].fourcc;
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
	unsigned int extra_idx = 0;
	int rc = 0;
	int i;
	u32 max_input_size = 0;

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

		inst->clk_data.opb_fourcc = f->fmt.pix_mp.pixelformat;
		if (inst->fmts[fmt->type].fourcc == f->fmt.pix_mp.pixelformat &&
			inst->prop.width[CAPTURE_PORT] == f->fmt.pix_mp.width &&
			inst->prop.height[CAPTURE_PORT] ==
				f->fmt.pix_mp.height) {
			dprintk(VIDC_DBG, "No change in CAPTURE port params\n");
			return 0;
		}
		memcpy(&inst->fmts[fmt->type], fmt,
				sizeof(struct msm_vidc_format));

		inst->bit_depth = MSM_VIDC_BIT_DEPTH_8;
		if ((f->fmt.pix_mp.pixelformat ==
			V4L2_PIX_FMT_NV12_TP10_UBWC) ||
			(f->fmt.pix_mp.pixelformat ==
			V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS)) {
			inst->bit_depth = MSM_VIDC_BIT_DEPTH_10;
		}
		inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
		}

		f->fmt.pix_mp.plane_fmt[0].sizeimage =
			msm_vidc_calculate_dec_output_frame_size(inst);

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

		rc = msm_vdec_update_stream_output_mode(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: failed to update output stream mode\n",
				__func__);
			goto err_invalid_fmt;
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

		max_input_size = msm_vidc_calculate_dec_input_frame_size(inst);
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
	inst->prop.extradata_ctrls = EXTRADATA_DEFAULT;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_DYNAMIC;
	inst->stream_output_mode = HAL_VIDEO_DECODER_PRIMARY;
	/* To start with, in port is 1 plane and out is 2 */
	inst->bufq[OUTPUT_PORT].num_planes = 1;
	inst->bufq[CAPTURE_PORT].num_planes = 2;
	inst->bufq[CAPTURE_PORT].plane_sizes[1] =
		msm_vidc_calculate_dec_output_extra_size(inst);

	inst->clk_data.frame_rate = (DEFAULT_FPS << 16);
	inst->clk_data.operating_rate = (DEFAULT_FPS << 16);
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
	msm_vidc_init_buffer_size_calculators(inst);

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

int msm_vdec_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	dprintk(VIDC_DBG,
		"%s: %x : control name = %s, id = 0x%x value = %d\n",
		__func__, hash32_ptr(inst->session), ctrl->name,
		ctrl->id, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE:
		inst->profile = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL:
		inst->level = msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
		inst->level |=
			(msm_comm_v4l2_to_hfi(ctrl->id, ctrl->val) << 28);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER:
	case V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE:
	case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT:
	case V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT:
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		inst->flags &= ~VIDC_THUMBNAIL;
		if (ctrl->val)
			inst->flags |= VIDC_THUMBNAIL;

		msm_dcvs_try_enable(inst);
		rc = msm_vidc_set_buffer_count_for_thumbnail(inst);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to set buffer count\n");
			return rc;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags &= ~VIDC_SECURE;
		if (ctrl->val)
			inst->flags |= VIDC_SECURE;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE:
		inst->clk_data.frame_rate = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		if (ctrl->val == EXTRADATA_NONE)
			inst->prop.extradata_ctrls = 0;
		else
			inst->prop.extradata_ctrls |= ctrl->val;
		/*
		 * nothing to do here as inst->bufq[CAPTURE_PORT].num_planes
		 * and inst->bufq[CAPTURE_PORT].plane_sizes[1] are already
		 * initialized to proper values
		 */
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BUFFER_SIZE_LIMIT:
		inst->buffer_size_limit = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY:
		inst->flags &= ~VIDC_REALTIME;
		if (ctrl->val)
			inst->flags |= VIDC_REALTIME;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE:
		inst->clk_data.operating_rate = ctrl->val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_MODE:
		inst->clk_data.low_latency_mode = !!ctrl->val;
		break;
	default:
		dprintk(VIDC_ERR,
			"Unknown control %#x\n", ctrl->id);
		break;
	}

	return rc;
}

int msm_vdec_set_frame_size(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_frame_size frame_size;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	frame_size.buffer_type = HFI_BUFFER_INPUT;
	frame_size.width = inst->prop.width[OUTPUT_PORT];
	frame_size.height = inst->prop.height[OUTPUT_PORT];
	dprintk(VIDC_DBG, "%s: input wxh %dx%d\n", __func__,
		frame_size.width, frame_size.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_size, sizeof(frame_size));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_color_format(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_format_constraint *fmt_constraint;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = msm_comm_set_color_format(inst,
			msm_comm_get_hal_output_buffer(inst),
			inst->clk_data.opb_fourcc);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: set color format (%#x) failed\n",
			__func__, inst->clk_data.opb_fourcc);
		return rc;
	}
	fmt_constraint = msm_comm_get_pixel_fmt_constraints(
			dec_pix_format_constraints,
			ARRAY_SIZE(dec_pix_format_constraints),
			inst->clk_data.opb_fourcc);
	if (fmt_constraint) {
		rc = msm_comm_set_color_format_constraints(inst,
				msm_comm_get_hal_output_buffer(inst),
				fmt_constraint);
		if (rc) {
			dprintk(VIDC_ERR,
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
	struct hal_buffer_requirements *bufreq;
	enum hal_buffer buffer_type;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	buffer_type = HAL_BUFFER_INPUT;
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

int msm_vdec_set_output_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_buffer_requirements *bufreq;
	enum hal_buffer buffer_type;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	buffer_type = msm_comm_get_hal_output_buffer(inst);
	/* Correct buffer counts is always stored in HAL_BUFFER_OUTPUT */
	bufreq = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!bufreq) {
		dprintk(VIDC_ERR, "%s: failed to set bufreqs(%#x)\n",
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

int msm_vdec_set_profile_level(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_profile_level profile_level;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

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

int msm_vdec_set_output_order(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	u32 output_order;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER);
	dprintk(VIDC_DBG, "%s: %d\n", __func__, ctrl->val);
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		output_order = HFI_OUTPUT_ORDER_DECODE;
	else
		output_order = HFI_OUTPUT_ORDER_DISPLAY;

	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_OUTPUT_ORDER, &output_order,
		sizeof(u32));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_picture_type(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable_picture enable_picture;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE);
	enable_picture.picture_type = ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, enable_picture.picture_type);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_PICTURE_TYPE_DECODE, &enable_picture,
		sizeof(enable_picture));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_sync_frame_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_enable hfi_property;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE);
	hfi_property.enable = (bool)ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, hfi_property.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_THUMBNAIL_MODE, &hfi_property,
		sizeof(hfi_property));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_secure_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SECURE);

	if (ctrl->val) {
		if (!(inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_HEVC ||
			inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_H264 ||
			inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9 ||
			inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_MPEG2)) {
			dprintk(VIDC_ERR,
				"%s: Secure allowed for HEVC/H264/VP9/MPEG2\n",
				__func__);
			return -EINVAL;
		}
	}

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, ctrl->val);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_SECURE_SESSION, &ctrl->val, sizeof(u32));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_output_stream_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_multi_stream multi_stream;
	struct hfi_frame_size frame_sz;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (is_primary_output_mode(inst)) {
		multi_stream.buffer_type = HFI_BUFFER_OUTPUT;
		multi_stream.enable = true;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM, &multi_stream,
			sizeof(multi_stream));
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set prop multistream primary failed : %d\n",
				__func__, rc);
			return rc;
		}
		multi_stream.buffer_type = HFI_BUFFER_OUTPUT2;
		multi_stream.enable = false;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_VDEC_MULTI_STREAM, &multi_stream,
			sizeof(multi_stream));
		if (rc) {
			dprintk(VIDC_ERR,
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
			dprintk(VIDC_ERR,
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
			dprintk(VIDC_ERR,
				"%s: set prop multistream secondary2 failed : %d\n",
				__func__, rc);
			return rc;
		}
		frame_sz.buffer_type = HFI_BUFFER_OUTPUT2;
		frame_sz.width = inst->prop.width[CAPTURE_PORT];
		frame_sz.height = inst->prop.height[CAPTURE_PORT];
		dprintk(VIDC_DBG,
			"frame_size: hal buffer type %d, width %d, height %d\n",
			frame_sz.buffer_type, frame_sz.width, frame_sz.height);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
			HFI_PROPERTY_PARAM_FRAME_SIZE, &frame_sz,
			sizeof(frame_sz));
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set prop frame_size failed\n",
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
	struct v4l2_ctrl *ctrl;
	struct hfi_enable hfi_property;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY);
	hfi_property.enable = (bool)ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, hfi_property.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_REALTIME, &hfi_property,
		sizeof(hfi_property));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_operating_rate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hfi_operating_rate operating_rate;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (is_decode_session(inst))
		return 0;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE);
	operating_rate.operating_rate = ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__,
			operating_rate.operating_rate);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_CONFIG_OPERATING_RATE, &operating_rate,
		sizeof(operating_rate));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

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
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl_8b = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT);
	ctrl_10b = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT);
	conceal_color.conceal_color_8bit = ctrl_8b->val;
	conceal_color.conceal_color_10bit = ctrl_10b->val;

	dprintk(VIDC_DBG, "%s: %#x %#x\n", __func__,
		conceal_color.conceal_color_8bit,
		conceal_color.conceal_color_10bit);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_VDEC_CONCEAL_COLOR, &conceal_color,
		sizeof(conceal_color));
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}


int msm_vdec_set_extradata(struct msm_vidc_inst *inst)
{
	uint32_t display_info = HFI_PROPERTY_PARAM_VUI_DISPLAY_INFO_EXTRADATA;
	u32 value = 0x0;

	switch (inst->fmts[OUTPUT_PORT].fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_HEVC:
		display_info = HFI_PROPERTY_PARAM_VUI_DISPLAY_INFO_EXTRADATA;
		break;
	case V4L2_PIX_FMT_VP8:
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
	if (inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9 ||
		inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_HDR10_HIST_EXTRADATA, 0x1);
	}

	msm_comm_set_extradata(inst,
		HFI_PROPERTY_PARAM_VDEC_NUM_CONCEALED_MB, 0x1);
	if (inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VDEC_MASTER_DISP_COL_SEI_EXTRADATA,
			0x1);
		msm_comm_set_extradata(inst,
			HFI_PROPERTY_PARAM_VDEC_CLL_SEI_EXTRADATA, 0x1);
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
		rc = msm_vdec_set_picture_type(inst);
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
	rc = msm_vdec_set_operating_rate(inst);
	if (rc)
		goto exit;

exit:
	if (rc)
		dprintk(VIDC_ERR, "%s: failed with %d\n", __func__, rc);
	else
		dprintk(VIDC_DBG, "%s: set properties successful\n", __func__);

	return rc;
}

int msm_vdec_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops)
{
	return msm_comm_ctrl_init(inst, msm_vdec_ctrls,
		ARRAY_SIZE(msm_vdec_ctrls), ctrl_ops);
}
