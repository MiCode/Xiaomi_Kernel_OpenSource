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
#define MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS MIN_NUM_OUTPUT_BUFFERS
#define MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS MIN_NUM_CAPTURE_BUFFERS
#define MIN_NUM_DEC_OUTPUT_BUFFERS 4
#define MIN_NUM_DEC_CAPTURE_BUFFERS 4
// Y=16(0-9bits), Cb(10-19bits)=Cr(20-29bits)=128, black by default
#define DEFAULT_VIDEO_CONCEAL_COLOR_BLACK 0x8020010
#define MB_SIZE_IN_PIXEL (16 * 16)
#define OPERATING_FRAME_RATE_STEP (1 << 16)
#define MAX_VP9D_INST_COUNT 6

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
		.maximum = V4L2_MPEG_VIDC_EXTRADATA_UBWC_CR_STATS_INFO,
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
		.maximum = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51,
		.default_value = V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51,
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
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_DISABLE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_ENABLE,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_DISABLE,
		.step = 1,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)

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
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.get_frame_size = get_frame_size_compressed_full_yuv,
		.type = OUTPUT_PORT,
		.defer_outputs = false,
	},
	{
		.name = "VP9",
		.description = "VP9 compressed format",
		.fourcc = V4L2_PIX_FMT_VP9,
		.get_frame_size = get_frame_size_compressed_full_yuv,
		.type = OUTPUT_PORT,
		.defer_outputs = true,
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
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_DYNAMIC;
	/* To start with, both ports are 1 plane each */
	inst->bufq[OUTPUT_PORT].num_planes = 1;
	inst->bufq[CAPTURE_PORT].num_planes = 1;
	inst->prop.fps = DEFAULT_FPS;
	inst->clk_data.operating_rate = 0;

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
	int rc = 0, temp;
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
	struct hal_buffer_requirements *bufreq;

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
		case V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION:
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
		case V4L2_MPEG_VIDC_EXTRADATA_FRAME_BITS_INFO:
		case V4L2_MPEG_VIDC_EXTRADATA_VQZIP_SEI:
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
			temp_ctrl = TRY_GET_CTRL(
				V4L2_CID_MPEG_VIDC_VIDEO_DPB_COLOR_FORMAT);
			switch (temp_ctrl->val) {
			case V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_UBWC:
				temp = V4L2_PIX_FMT_NV12_UBWC;
				break;
			case V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_TP10_UBWC:
				temp = V4L2_PIX_FMT_NV12_TP10_UBWC;
				break;
			case V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_NONE:
			default:
				dprintk(VIDC_DBG,
					"set default dpb color format as NV12_UBWC\n");
				temp = V4L2_PIX_FMT_NV12_UBWC;
				break;
			}
			rc = msm_comm_set_color_format(inst,
				HAL_BUFFER_OUTPUT, temp);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s Failed setting output color format: %#x\n",
					__func__, rc);
				break;
			}

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
		if (ctrl->val ==
			V4L2_CID_MPEG_VIDC_VIDEO_LOWLATENCY_ENABLE)
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
			"Control: Name = %s, ID = 0x%x Value = %d\n",
				ctrl->name, ctrl->id, ctrl->val);
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
			rc = msm_vidc_update_host_buff_counts(inst);
			break;
		case V4L2_CID_MPEG_VIDC_VIDEO_DPB_COLOR_FORMAT:
			control.id =
				V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE;
			switch (ext_control[i].value) {
			case V4L2_MPEG_VIDC_VIDEO_DPB_COLOR_FMT_NONE:
				if (!msm_comm_g_ctrl_for_id(inst, control.id)) {
					rc = msm_comm_release_output_buffers(
						inst, false);
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
					if (rc) {
						dprintk(VIDC_ERR,
							"%s Failed to get buffer requirements : %d\n",
							__func__, rc);
						break;
					}
				}
				rc = msm_vidc_update_host_buff_counts(inst);
				inst->clk_data.dpb_fourcc = fourcc;
				control.id =
				V4L2_CID_MPEG_VIDC_VIDEO_DPB_COLOR_FORMAT;
				control.value = ext_control[i].value;
				rc = msm_comm_s_ctrl(inst, &control);
				if (rc)
					dprintk(VIDC_ERR,
						"%s: set control dpb color format %d failed\n",
						__func__, control.value);
				break;
			default:
				dprintk(VIDC_ERR,
					"%s Unsupported output color format\n",
					__func__);
				rc = -ENOTSUPP;
				break;
			}
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
