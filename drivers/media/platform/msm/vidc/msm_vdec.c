// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include "msm_vdec.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"

#define MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS MIN_NUM_OUTPUT_BUFFERS
#define MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS MIN_NUM_CAPTURE_BUFFERS
#define MIN_NUM_DEC_OUTPUT_BUFFERS 4
#define MIN_NUM_DEC_CAPTURE_BUFFERS 4
/* Y=16(0-9bits), Cb(10-19bits)=Cr(20-29bits)=128, black by default */
#define DEFAULT_VIDEO_CONCEAL_COLOR_BLACK 0x8020010
#define MB_SIZE_IN_PIXEL (16 * 16)
#define DEFAULT_OPERATING_RATE 30
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

static struct msm_vidc_ctrl msm_vdec_ctrls[] = {
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
		.id = V4L2_CID_MPEG_VIDEO_VP9_PROFILE,
		.name = "VP9 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_VP9_PROFILE_0,
		.maximum = V4L2_MPEG_VIDEO_VP9_PROFILE_3,
		.default_value = V4L2_MPEG_VIDEO_VP9_PROFILE_0,
		.menu_skip_mask = 0,
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
		.default_value =  (DEFAULT_OPERATING_RATE << 16),
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

		inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
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

int msm_vdec_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_buffer_requirements *bufreq;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	v4l2_ctrl_unlock(ctrl);
	dprintk(VIDC_DBG,
		"%s: %x : control name = %s, id = 0x%x value = %d\n",
		__func__, hash32_ptr(inst->session), ctrl->name,
		ctrl->id, ctrl->val);

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
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
		bufreq = get_buff_req_buffer(inst, HAL_BUFFER_INPUT);
		if (!bufreq)
			return -EINVAL;

		bufreq->buffer_count_min =
			MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
		bufreq->buffer_count_min_host =
			MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
		bufreq->buffer_count_actual =
			MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;

		if (msm_comm_get_stream_output_mode(inst) ==
				HAL_VIDEO_DECODER_SECONDARY) {
			bufreq = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
			if (!bufreq)
				return -EINVAL;

			bufreq->buffer_count_min =
				MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
			bufreq->buffer_count_min_host =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
			bufreq->buffer_count_actual =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;

			bufreq = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT2);
			if (!bufreq)
				return -EINVAL;

			bufreq->buffer_count_min =
				MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
			bufreq->buffer_count_min_host =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
			bufreq->buffer_count_actual =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
		} else {
			bufreq = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
			if (!bufreq)
				return -EINVAL;

			bufreq->buffer_count_min =
				MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
			bufreq->buffer_count_min_host =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
			bufreq->buffer_count_actual =
				MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
		}
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags &= ~VIDC_SECURE;
		if (ctrl->val)
			inst->flags |= VIDC_SECURE;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		inst->bufq[CAPTURE_PORT].num_planes = 1;
		inst->bufq[CAPTURE_PORT].plane_sizes[1] = 0;
		if (ctrl->val != EXTRADATA_NONE) {
			inst->bufq[CAPTURE_PORT].num_planes = 2;
			inst->bufq[CAPTURE_PORT].plane_sizes[1] =
				VENUS_EXTRADATA_SIZE(
					inst->prop.height[CAPTURE_PORT],
					inst->prop.width[CAPTURE_PORT]);
		}
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
		inst->clk_data.low_latency_mode = (bool)ctrl->val;
		break;
	default:
		dprintk(VIDC_ERR,
			"Unknown control %#x\n", ctrl->id);
		break;
	}
	v4l2_ctrl_lock(ctrl);

	return rc;
}

struct v4l2_ctrl *msm_vdec_get_ctrl(struct msm_vidc_inst *inst, u32 id)
{
	int i;
	struct v4l2_ctrl *ctrl;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(msm_vdec_ctrls); i++) {
		ctrl = inst->ctrls[i];
		if (ctrl->id == id)
			return ctrl;
	}

	dprintk(VIDC_ERR, "%s: control id (%#x) not found\n", __func__, id);
	return NULL;
}

int msm_vdec_set_frame_size(struct msm_vidc_inst *inst)
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
	dprintk(VIDC_DBG, "%s: input wxh %dx%d\n", __func__,
		frame_sz.width, frame_sz.height);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_FRAME_SIZE, &frame_sz);
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
	bufreq = get_buff_req_buffer(inst, buffer_type);
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
	struct v4l2_ctrl *profile;
	struct v4l2_ctrl *level;
	struct hal_profile_level profile_level;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	profile = msm_vdec_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_PROFILE);
	if (!profile) {
		dprintk(VIDC_ERR,
			"%s: failed to get profile ctrl\n", __func__);
		return -EINVAL;
	}
	level = msm_vdec_get_ctrl(inst, V4L2_CID_MPEG_VIDEO_H264_LEVEL);
	if (!level) {
		dprintk(VIDC_ERR,
			"%s: failed to get level ctrl\n", __func__);
		return -EINVAL;
	}
	profile_level.profile = profile->val;
	profile_level.level = level->val;
	dprintk(VIDC_DBG, "%s: %#x %#x\n", __func__,
		profile_level.profile, profile_level.level);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_PROFILE_LEVEL_CURRENT, &profile_level);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_profile(struct msm_vidc_inst *inst)
{
	return msm_vdec_set_profile_level(inst);
}

int msm_vdec_set_level(struct msm_vidc_inst *inst)
{
	return msm_vdec_set_profile_level(inst);
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

	ctrl = msm_vdec_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_DECODE_ORDER);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: failed to get output_order ctrl\n", __func__);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "%s: %d\n", __func__, ctrl->val);
	if (ctrl->val == V4L2_MPEG_MSM_VIDC_ENABLE)
		output_order = HAL_OUTPUT_ORDER_DECODE;
	else
		output_order = HAL_OUTPUT_ORDER_DISPLAY;

	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VDEC_OUTPUT_ORDER, &output_order);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_picture_type(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable_picture enable_picture;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_vdec_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: failed to get pcture_type ctrl\n", __func__);
		return -EINVAL;
	}
	enable_picture.picture_type = ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, enable_picture.picture_type);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VDEC_PICTURE_TYPE_DECODE, &enable_picture);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_sync_frame_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable hal_property;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_vdec_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: failed to get sync_frame_mode ctrl\n", __func__);
		return -EINVAL;
	}
	hal_property.enable = (bool)ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, hal_property.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VDEC_SYNC_FRAME_DECODE, &hal_property);
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

	ctrl = msm_vdec_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_SECURE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: failed to get output_order ctrl\n", __func__);
		return -EINVAL;
	}

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, ctrl->val);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_SECURE, &ctrl->val);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_output_stream_mode(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int fourcc;
	struct hfi_device *hdev;
	u32 output_stream_mode;
	struct hal_multi_stream multi_stream;
	struct hal_frame_size frame_sz;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	/* Decide split/combined mode here */
	output_stream_mode = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY;

	switch (output_stream_mode) {
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY:
		multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
		multi_stream.enable = true;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_VDEC_MULTI_STREAM, &multi_stream);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set prop multistream primary (output) failed\n",
				__func__, rc);
			return rc;
		}
		multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
		multi_stream.enable = false;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_VDEC_MULTI_STREAM, &multi_stream);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set prop multistream primary (output2) failed\n",
				__func__, rc);
			return rc;
		}
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
			dprintk(VIDC_ERR, "%s: invalid bitdepth\n", __func__);
			return -EINVAL;
		}
		rc = msm_comm_set_color_format(inst,
					HAL_BUFFER_OUTPUT, fourcc);
		if (rc)
			return rc;

		multi_stream.buffer_type = HAL_BUFFER_OUTPUT2;
		multi_stream.enable = true;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_VDEC_MULTI_STREAM, &multi_stream);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set prop multistream (output2) failed\n",
				__func__, rc);
			return rc;
		}
		multi_stream.buffer_type = HAL_BUFFER_OUTPUT;
		multi_stream.enable = false;
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_VDEC_MULTI_STREAM, &multi_stream);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set prop multistream (output) failed\n",
				__func__, rc);
			return rc;
		}
		frame_sz.buffer_type = HAL_BUFFER_OUTPUT2;
		frame_sz.width = inst->prop.width[CAPTURE_PORT];
		frame_sz.height = inst->prop.height[CAPTURE_PORT];
		dprintk(VIDC_DBG,
			"frame_size: hal buffer type %d, width %d, height %d\n",
			frame_sz.buffer_type, frame_sz.width, frame_sz.height);
		rc = call_hfi_op(hdev, session_set_property, inst->session,
				HAL_PARAM_FRAME_SIZE, &frame_sz);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: set prop frame_size failed\n",
				__func__, rc);
			return rc;
		}
		break;
	default:
		dprintk(VIDC_ERR,
			"%s: unknown multistream type %#x\n",
			__func__, output_stream_mode);
		rc = -EINVAL;
		return rc;
	}

	return rc;
}

int msm_vdec_set_priority(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_enable hal_property;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl = msm_vdec_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: failed to get output_order ctrl\n", __func__);
		return -EINVAL;
	}
	hal_property.enable = (bool)ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__, hal_property.enable);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_REALTIME, &hal_property);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}

int msm_vdec_set_operating_rate(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct v4l2_ctrl *ctrl;
	struct hal_operating_rate operating_rate;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (is_decode_session(inst))
		return 0;

	ctrl = msm_vdec_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: failed to get output_order ctrl\n", __func__);
		return -EINVAL;
	}
	operating_rate.operating_rate = ctrl->val;

	dprintk(VIDC_DBG, "%s: %#x\n", __func__,
			operating_rate.operating_rate);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_CONFIG_OPERATING_RATE, &operating_rate);
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
	struct hal_conceal_color conceal_color;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	ctrl_8b = msm_vdec_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_8BIT);
	if (!ctrl_8b) {
		dprintk(VIDC_ERR,
			"%s: failed to get conceal_color_8bit ctrl\n",
			__func__);
		return -EINVAL;
	}
	ctrl_10b = msm_vdec_get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_CONCEAL_COLOR_10BIT);
	if (!ctrl_10b) {
		dprintk(VIDC_ERR,
			"%s: failed to get conceal_color_10bit ctrl\n",
			__func__);
		return -EINVAL;
	}
	conceal_color.conceal_color_8bit = ctrl_8b->val;
	conceal_color.conceal_color_10bit = ctrl_10b->val;

	dprintk(VIDC_DBG, "%s: %#x %#x\n", __func__,
		conceal_color.conceal_color_8bit,
		conceal_color.conceal_color_10bit);
	rc = call_hfi_op(hdev, session_set_property, inst->session,
			HAL_PARAM_VDEC_CONCEAL_COLOR, &conceal_color);
	if (rc)
		dprintk(VIDC_ERR, "%s: set property failed\n", __func__);

	return rc;
}


int msm_vdec_set_extradata(struct msm_vidc_inst *inst)
{
	uint32_t display_info = HAL_EXTRADATA_VUI_DISPLAY_INFO;
	struct v4l2_ctrl *ctrl;

	ctrl = msm_vdec_get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: failed to get output_order ctrl\n", __func__);
		return -EINVAL;
	}

	switch (inst->fmts[OUTPUT_PORT].fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_HEVC:
		display_info = HAL_EXTRADATA_VUI_DISPLAY_INFO;
		break;
	case V4L2_PIX_FMT_VP8:
	case V4L2_PIX_FMT_VP9:
		display_info = HAL_EXTRADATA_VPX_COLORSPACE;
		break;
	case V4L2_PIX_FMT_MPEG2:
		display_info = HAL_EXTRADATA_MPEG2_SEQDISP;
		break;
	}

	if (ctrl->val == EXTRADATA_NONE) {
		// Disable all Extradata
		msm_comm_set_extradata(inst, HAL_EXTRADATA_OUTPUT_CROP, 0x0);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_INTERLACE_VIDEO, 0x0);
		msm_comm_set_extradata(inst, display_info, 0x0);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_UBWC_CR_STATS_INFO, 0x0);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_NUM_CONCEALED_MB, 0x0);
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
				HAL_EXTRADATA_MASTERING_DISPLAY_COLOUR_SEI,
				0x0);
			msm_comm_set_extradata(inst,
				HAL_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI, 0x0);
		}
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_STREAM_USERDATA, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_TIMESTAMP, 0x0);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_S3D_FRAME_PACKING, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_FRAME_RATE, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_PANSCAN_WINDOW, 0x0);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_RECOVERY_POINT_SEI, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ASPECT_RATIO, 0x0);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_FRAME_QP, 0x0);
	}
	if (ctrl->val & EXTRADATA_DEFAULT) {
		// Enable Default Extradata
		msm_comm_set_extradata(inst, HAL_EXTRADATA_OUTPUT_CROP, 0x1);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_INTERLACE_VIDEO, 0x1);
		msm_comm_set_extradata(inst, display_info, 0x1);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_NUM_CONCEALED_MB, 0x1);
		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC) {
			msm_comm_set_extradata(inst,
				HAL_EXTRADATA_MASTERING_DISPLAY_COLOUR_SEI,
				0x1);
			msm_comm_set_extradata(inst,
				HAL_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI, 0x1);
		}
	}
	if (ctrl->val & EXTRADATA_ADVANCED) {
		// Enable Advanced Extradata
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_STREAM_USERDATA, 0x1);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_TIMESTAMP, 0x1);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_S3D_FRAME_PACKING, 0x1);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_FRAME_RATE, 0x1);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_PANSCAN_WINDOW, 0x1);
		msm_comm_set_extradata(inst,
			HAL_EXTRADATA_RECOVERY_POINT_SEI, 0x1);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_ASPECT_RATIO, 0x1);
		msm_comm_set_extradata(inst, HAL_EXTRADATA_FRAME_QP, 0x1);
	}

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
		rc = msm_vdec_set_profile(inst);
		if (rc)
			goto exit;
		rc = msm_vdec_set_level(inst);
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
	rc = msm_vdec_set_output_buffer_counts(inst);
	if (rc)
		goto exit;
	rc = msm_vdec_set_output_stream_mode(inst);
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

int msm_vdec_s_parm(struct msm_vidc_inst *inst, struct v4l2_streamparm *a)
{
	return msm_vidc_comm_s_parm(inst, a);
}

int msm_vdec_ctrl_init(struct msm_vidc_inst *inst,
	const struct v4l2_ctrl_ops *ctrl_ops)
{
	return msm_comm_ctrl_init(inst, msm_vdec_ctrls,
		ARRAY_SIZE(msm_vdec_ctrls), ctrl_ops);
}
