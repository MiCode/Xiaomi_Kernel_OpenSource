/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <mach/scm.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_smem.h"
#include "msm_vidc_debug.h"

#define MSM_VDEC_DVC_NAME "msm_vdec_8974"
#define MIN_NUM_OUTPUT_BUFFERS 4
#define MAX_NUM_OUTPUT_BUFFERS 6
#define DEFAULT_CONCEAL_COLOR 0x0

#define TZ_INFO_GET_FEATURE_VERSION_ID 0x3
#define TZ_DYNAMIC_BUFFER_FEATURE_ID 12
#define TZ_FEATURE_VERSION(major, minor, patch) \
	(((major & 0x3FF) << 22) | ((minor & 0x3FF) << 12) | (patch & 0xFFF))
struct tz_get_feature_version {
	u32 feature_id;
};

enum msm_vdec_ctrl_cluster {
	MSM_VDEC_CTRL_CLUSTER_MAX = 1 << 0,
};

static const char *const mpeg_video_vidc_divx_format[] = {
	"DIVX Format 3",
	"DIVX Format 4",
	"DIVX Format 5",
	"DIVX Format 6",
	NULL
};
static const char *mpeg_video_stream_format[] = {
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
static const char *const mpeg_video_vidc_extradata[] = {
	"Extradata none",
	"Extradata MB Quantization",
	"Extradata Interlace Video",
	"Extradata VC1 Framedisp",
	"Extradata VC1 Seqdisp",
	"Extradata timestamp",
	"Extradata S3D Frame Packing",
	"Extradata Frame Rate",
	"Extradata Panscan Window",
	"Extradata Recovery point SEI",
	"Extradata Closed Caption UD",
	"Extradata AFD UD",
	"Extradata Multislice info",
	"Extradata number of concealed MB",
	"Extradata metadata filler",
	"Extradata input crop",
	"Extradata digital zoom",
	"Extradata aspect ratio",
	"Extradata mpeg2 seqdisp",
};
static const char *const mpeg_vidc_video_alloc_mode_type[] = {
	"Buffer Allocation Static",
	"Buffer Allocation Ring Buffer",
	"Buffer Allocation Dynamic Buffer"
};

static const char *const perf_level[] = {
	"Nominal",
	"Performance",
	"Turbo"
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
		.step = 0,
		.cluster = 0,
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
		.step = 0,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE,
		.name = "Picture Type Decoding",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 15,
		.default_value = 15,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_KEEP_ASPECT_RATIO,
		.name = "Keep Aspect Ratio",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_POST_LOOP_DEBLOCKER_MODE,
		.name = "Deblocker Mode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_DIVX_FORMAT,
		.name = "Divx Format",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_4,
		.maximum = V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_6,
		.default_value = V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_4,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_4) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_5) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DIVX_FORMAT_6)
			),
		.qmenu = mpeg_video_vidc_divx_format,
		.step = 0,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_MB_ERROR_MAP_REPORTING,
		.name = "MB Error Map Reporting",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER,
		.name = "control",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE,
		.name = "Sync Frame Decode",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
		.maximum = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_ENABLE,
		.default_value = V4L2_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE_DISABLE,
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
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA,
		.name = "Extradata Type",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.maximum = V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP,
		.default_value = V4L2_MPEG_VIDC_EXTRADATA_NONE,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NONE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_CLOSED_CAPTION_UD) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_AFD_UD) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER) |
			(1 << V4L2_MPEG_VIDC_INDEX_EXTRADATA_INPUT_CROP) |
			(1 << V4L2_MPEG_VIDC_INDEX_EXTRADATA_DIGITAL_ZOOM) |
			(1 << V4L2_MPEG_VIDC_INDEX_EXTRADATA_ASPECT_RATIO) |
			(1 << V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP)
			),
		.qmenu = mpeg_video_vidc_extradata,
		.step = 0,
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
		.step = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_INPUT,
		.name = "Buffer allocation mode for input",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.maximum = V4L2_MPEG_VIDC_VIDEO_DYNAMIC,
		.default_value = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_STATIC) |
			(1 << V4L2_MPEG_VIDC_VIDEO_RING) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DYNAMIC)
			),
		.qmenu = mpeg_vidc_video_alloc_mode_type,
		.step = 0,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_OUTPUT,
		.name = "Buffer allocation mode for output",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.maximum = V4L2_MPEG_VIDC_VIDEO_DYNAMIC,
		.default_value = V4L2_MPEG_VIDC_VIDEO_STATIC,
		.menu_skip_mask = ~(
			(1 << V4L2_MPEG_VIDC_VIDEO_STATIC) |
			(1 << V4L2_MPEG_VIDC_VIDEO_RING) |
			(1 << V4L2_MPEG_VIDC_VIDEO_DYNAMIC)
			),
		.qmenu = mpeg_vidc_video_alloc_mode_type,
		.step = 0,
		.cluster = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY,
		.name = "Video frame assembly",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = V4L2_MPEG_VIDC_FRAME_ASSEMBLY_DISABLE,
		.maximum = V4L2_MPEG_VIDC_FRAME_ASSEMBLY_ENABLE,
		.default_value =  V4L2_MPEG_VIDC_FRAME_ASSEMBLY_DISABLE,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
		.cluster = 0,
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
		.menu_skip_mask = 0,
		.step = 1,
		.qmenu = NULL,
		.cluster = 0,
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)

static u32 get_frame_size_nv12(int plane,
					u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height);
}

static u32 get_frame_size_compressed(int plane,
					u32 height, u32 width)
{
	return (width * height * 3/2)/4;
}

struct msm_vidc_format vdec_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.get_frame_size = get_frame_size_nv12,
		.type = CAPTURE_PORT,
	},
	{
		.name = "Mpeg4",
		.description = "Mpeg4 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG4,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "Mpeg2",
		.description = "Mpeg2 compressed format",
		.fourcc = V4L2_PIX_FMT_MPEG2,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H263",
		.description = "H263 compressed format",
		.fourcc = V4L2_PIX_FMT_H263,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VC1",
		.description = "VC-1 compressed format",
		.fourcc = V4L2_PIX_FMT_VC1_ANNEX_G,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VC1 SP",
		.description = "VC-1 compressed format G",
		.fourcc = V4L2_PIX_FMT_VC1_ANNEX_L,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "HEVC",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "HEVC_HYBRID",
		.description = "HEVC compressed format",
		.fourcc = V4L2_PIX_FMT_HEVC_HYBRID,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "VP8",
		.description = "VP8 compressed format",
		.fourcc = V4L2_PIX_FMT_VP8,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "DIVX 311",
		.description = "DIVX 311 compressed format",
		.fourcc = V4L2_PIX_FMT_DIVX_311,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "DIVX",
		.description = "DIVX 4/5/6 compressed format",
		.fourcc = V4L2_PIX_FMT_DIVX,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	}
};

int msm_vdec_streamon(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
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

int msm_vdec_streamoff(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
{
	int rc = 0;
	struct buf_queue *q;

	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "Calling streamoff\n");
	mutex_lock(&q->lock);
	rc = vb2_streamoff(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "streamoff failed on port: %d\n", i);
	return rc;
}

int msm_vdec_prepare_buf(struct msm_vidc_inst *inst,
					struct v4l2_buffer *b)
{
	int rc = 0;
	struct vidc_buffer_addr_info buffer_info;
	int extra_idx = 0;
	int i;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			if (b->length != inst->fmts[CAPTURE_PORT]->num_planes) {
				dprintk(VIDC_ERR,
				"Planes mismatch: needed: %d, allocated: %d\n",
				inst->fmts[CAPTURE_PORT]->num_planes,
				b->length);
				rc = -EINVAL;
				break;
			}
			for (i = 0; (i < b->length)
				&& (i < VIDEO_MAX_PLANES); ++i) {
				dprintk(VIDC_DBG,
				"prepare plane: %d, device_addr = 0x%lx, size = %d\n",
				i, b->m.planes[i].m.userptr,
				b->m.planes[i].length);
			}
			buffer_info.buffer_size = b->m.planes[0].length;
			buffer_info.buffer_type =
				msm_comm_get_hal_output_buffer(inst);
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr =
				b->m.planes[0].m.userptr;
			extra_idx = EXTRADATA_IDX(b->length);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES) &&
				b->m.planes[extra_idx].m.userptr) {
				buffer_info.extradata_addr =
					b->m.planes[extra_idx].m.userptr;
				dprintk(VIDC_DBG,
				"extradata: 0x%lx\n",
				b->m.planes[extra_idx].m.userptr);
				buffer_info.extradata_size =
					b->m.planes[extra_idx].length;
			} else {
				buffer_info.extradata_addr = 0;
				buffer_info.extradata_size = 0;
			}
			rc = call_hfi_op(hdev, session_set_buffers,
					(void *)inst->session, &buffer_info);
			if (rc)
				dprintk(VIDC_ERR,
				"vidc_hal_session_set_buffers failed");
		break;
	default:
		dprintk(VIDC_ERR, "Buffer type not recognized: %d\n", b->type);
		break;
	}
	return rc;
}

int msm_vdec_release_buf(struct msm_vidc_inst *inst,
					struct v4l2_buffer *b)
{
	int rc = 0;
	struct vidc_buffer_addr_info buffer_info;
	struct msm_vidc_core *core = inst->core;
	int extra_idx = 0;
	int i;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	if (inst->state == MSM_VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"Core %p in bad state, ignoring release output buf\n",
				core);
		goto exit;
	}
	if (!inst->in_reconfig) {
		rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to move inst: %p to relase res done\n",
				inst);
			goto exit;
		}
	}
	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			if (b->length !=
				inst->fmts[CAPTURE_PORT]->num_planes) {
				dprintk(VIDC_ERR,
				"Planes mismatch: needed: %d, to release: %d\n",
				inst->fmts[CAPTURE_PORT]->num_planes,
				b->length);
				rc = -EINVAL;
				break;
			}
			for (i = 0; i < b->length; ++i) {
				dprintk(VIDC_DBG,
				"Release plane: %d device_addr = 0x%lx, size = %d\n",
				i, b->m.planes[i].m.userptr,
				b->m.planes[i].length);
			}
			buffer_info.buffer_size = b->m.planes[0].length;
			buffer_info.buffer_type =
				msm_comm_get_hal_output_buffer(inst);
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr =
				 b->m.planes[0].m.userptr;
			extra_idx = EXTRADATA_IDX(b->length);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)
				&& b->m.planes[extra_idx].m.userptr)
				buffer_info.extradata_addr =
					b->m.planes[extra_idx].m.userptr;
			else
				buffer_info.extradata_addr = 0;
			buffer_info.response_required = false;
			rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
			if (rc)
				dprintk(VIDC_ERR,
				"vidc_hal_session_release_buffers failed");
		break;
	default:
		dprintk(VIDC_ERR, "Buffer type not recognized: %d\n", b->type);
		break;
	}
exit:
	return rc;
}

int msm_vdec_qbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR, "Failed to find buffer queue for type = %d\n"
			, b->type);
		return -EINVAL;
	}
	mutex_lock(&q->lock);
	rc = vb2_qbuf(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "Failed to qbuf, %d\n", rc);
	return rc;
}
int msm_vdec_dqbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR, "Failed to find buffer queue for type = %d\n"
			, b->type);
		return -EINVAL;
	}
	mutex_lock(&q->lock);
	rc = vb2_dqbuf(&q->vb2_bufq, b, true);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_DBG, "Failed to dqbuf, %d\n", rc);
	return rc;
}

int msm_vdec_reqbufs(struct msm_vidc_inst *inst, struct v4l2_requestbuffers *b)
{
	struct buf_queue *q = NULL;
	int rc = 0;
	if (!inst || !b) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %p, buffer = %p\n", inst, b);
		return -EINVAL;
	}
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR, "Failed to find buffer queue for type = %d\n"
			, b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_reqbufs(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "Failed to get reqbufs, %d\n", rc);
	return rc;
}

int msm_vdec_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	const struct msm_vidc_format *fmt = NULL;
	unsigned int *plane_sizes = NULL;
	struct hfi_device *hdev;
	int stride, scanlines;
	int extra_idx = 0;
	int rc = 0;
	int i;
	struct hal_buffer_requirements *buff_req_buffer;
	if (!inst || !f || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %p, format = %p\n", inst, f);
		return -EINVAL;
	}
	hdev = inst->core->device;
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		fmt = inst->fmts[CAPTURE_PORT];
	else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = inst->fmts[OUTPUT_PORT];

	if (fmt) {
		f->fmt.pix_mp.pixelformat = fmt->fourcc;
		f->fmt.pix_mp.num_planes = fmt->num_planes;
		if (inst->in_reconfig == true) {
			if (msm_comm_get_stream_output_mode(inst) ==
				HAL_VIDEO_DECODER_PRIMARY) {
				inst->prop.height[CAPTURE_PORT] =
					inst->reconfig_height;
				inst->prop.width[CAPTURE_PORT] =
					inst->reconfig_width;
				inst->prop.height[OUTPUT_PORT] =
					inst->reconfig_height;
				inst->prop.width[OUTPUT_PORT] =
					inst->reconfig_width;
			} else {
				inst->prop.height[OUTPUT_PORT] =
					inst->reconfig_height;
				inst->prop.width[OUTPUT_PORT] =
					inst->reconfig_width;
			}
			rc = msm_vidc_check_session_supported(inst);
			if (rc) {
				dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
				goto exit;
			}
		}
		f->fmt.pix_mp.height = inst->prop.height[CAPTURE_PORT];
		f->fmt.pix_mp.width = inst->prop.width[CAPTURE_PORT];
		stride = inst->prop.width[CAPTURE_PORT];
		scanlines = inst->prop.height[CAPTURE_PORT];
		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: Failed : Buffer requirements\n", __func__);
			goto exit;
		}
		if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			plane_sizes =
			&inst->bufq[OUTPUT_PORT].vb2_bufq.plane_sizes[0];
			for (i = 0; i < fmt->num_planes; ++i) {
				if (plane_sizes[i] == 0) {
					f->fmt.pix_mp.plane_fmt[i].sizeimage =
						fmt->get_frame_size(i,
						inst->capability.height.max,
						inst->capability.width.max);
					plane_sizes[i] =
					f->fmt.pix_mp.plane_fmt[i].sizeimage;
				} else
					f->fmt.pix_mp.plane_fmt[i].sizeimage =
						plane_sizes[i];
			}
		} else {
			switch (fmt->fourcc) {
			case V4L2_PIX_FMT_NV12:
				call_hfi_op(hdev, get_stride_scanline,
					COLOR_FMT_NV12,
					inst->prop.width[CAPTURE_PORT],
					inst->prop.height[CAPTURE_PORT],
					&stride, &scanlines);
				break;
			default:
				dprintk(VIDC_WARN,
					"Color format not recognized\n");
			}
			buff_req_buffer =
				get_buff_req_buffer(inst,
					msm_comm_get_hal_output_buffer(inst));
			if (buff_req_buffer)
				f->fmt.pix_mp.plane_fmt[0].sizeimage =
				buff_req_buffer->buffer_size;
			else
				f->fmt.pix_mp.plane_fmt[0].sizeimage = 0;

			extra_idx = EXTRADATA_IDX(fmt->num_planes);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
				buff_req_buffer =
					get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);
				if (buff_req_buffer)
					f->fmt.pix_mp.plane_fmt[extra_idx].
						sizeimage =
						buff_req_buffer->buffer_size;
				else
					f->fmt.pix_mp.plane_fmt[extra_idx].
						sizeimage = 0;
			}
			for (i = 0; i < fmt->num_planes; ++i)
				inst->bufq[CAPTURE_PORT].
					vb2_bufq.plane_sizes[i] =
					f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
		if (stride && scanlines) {
			f->fmt.pix_mp.plane_fmt[0].bytesperline =
				(__u16)stride;
			f->fmt.pix_mp.plane_fmt[0].reserved[0] =
				(__u16)scanlines;
		} else {
			f->fmt.pix_mp.plane_fmt[0].bytesperline =
				(__u16)inst->prop.width[CAPTURE_PORT];
			f->fmt.pix_mp.plane_fmt[0].reserved[0] =
				(__u16)inst->prop.height[CAPTURE_PORT];
		}
	} else {
		dprintk(VIDC_ERR,
			"Buf type not recognized, type = %d\n",
			f->type);
		rc = -EINVAL;
	}
exit:
	return rc;
}
int msm_vdec_s_parm(struct msm_vidc_inst *inst, struct v4l2_streamparm *a)
{
	u64 us_per_frame = 0;
	int rc = 0, fps = 0;
	if (a->parm.output.timeperframe.denominator) {
		switch (a->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			us_per_frame = a->parm.output.timeperframe.numerator *
				(u64)USEC_PER_SEC;
			do_div(us_per_frame, a->parm.output.\
					timeperframe.denominator);
			break;
		default:
			dprintk(VIDC_ERR,
					"Scale clocks : Unknown buffer type %d\n",
					a->type);
			break;
		}
	}

	if (!us_per_frame) {
		dprintk(VIDC_ERR,
			"Failed to scale clocks : time between frames is 0\n");
		rc = -EINVAL;
		goto exit;
	}

	fps = USEC_PER_SEC;
	do_div(fps, us_per_frame);

	if ((fps % 15 == 14) || (fps % 24 == 23))
		fps = fps + 1;
	else if ((fps % 24 == 1) || (fps % 15 == 1))
		fps = fps - 1;

	if (inst->prop.fps != fps) {
		dprintk(VIDC_PROF, "reported fps changed for %p: %d->%d\n",
				inst, inst->prop.fps, fps);
		inst->prop.fps = fps;
		mutex_lock(&inst->core->sync_lock);
		msm_comm_scale_clocks_and_bus(inst);
		mutex_unlock(&inst->core->sync_lock);
	}
exit:
	return rc;
}
int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	struct msm_vidc_format *fmt = NULL;
	struct hal_frame_size frame_sz;
	int extra_idx = 0;
	int rc = 0;
	int ret = 0;
	int i;
	struct hal_buffer_requirements *buff_req_buffer;
	int max_input_size = 0;

	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %p, format = %p\n", inst, f);
		return -EINVAL;
	}
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {

		fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->fmt.pix_mp.pixelformat,
			CAPTURE_PORT);
		if (!fmt || (fmt && fmt->type != CAPTURE_PORT)) {
			dprintk(VIDC_ERR,
				"Format: %d not supported on CAPTURE port\n",
				f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}
		inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_PRIMARY) {
			inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
			inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;
		}
		inst->fmts[fmt->type] = fmt;
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
		ret = ret || msm_comm_try_get_bufreqs(inst);
		if (ret) {
			for (i = 0; i < fmt->num_planes; ++i) {
				f->fmt.pix_mp.plane_fmt[i].sizeimage =
					fmt->get_frame_size(i,
						inst->capability.height.max,
						inst->capability.width.max);
			}
		} else {
			buff_req_buffer =
				get_buff_req_buffer(inst,
					msm_comm_get_hal_output_buffer(inst));
			if (buff_req_buffer)
				f->fmt.pix_mp.plane_fmt[0].sizeimage =
				buff_req_buffer->buffer_size;
			else
				f->fmt.pix_mp.plane_fmt[0].sizeimage = 0;
			extra_idx = EXTRADATA_IDX(fmt->num_planes);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
				buff_req_buffer =
					get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);
				if (buff_req_buffer)
					f->fmt.pix_mp.plane_fmt[1].sizeimage =
					buff_req_buffer->buffer_size;
				else
					f->fmt.pix_mp.plane_fmt[1].sizeimage =
					0;
			}
		}
		f->fmt.pix_mp.num_planes = fmt->num_planes;
		for (i = 0; i < fmt->num_planes; ++i) {
			inst->bufq[CAPTURE_PORT].vb2_bufq.plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		inst->prop.width[OUTPUT_PORT] = f->fmt.pix_mp.width;
		inst->prop.height[OUTPUT_PORT] = f->fmt.pix_mp.height;
		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_PRIMARY) {
			inst->prop.width[CAPTURE_PORT] = f->fmt.pix_mp.width;
			inst->prop.height[CAPTURE_PORT] = f->fmt.pix_mp.height;
		}
		rc = msm_vidc_check_session_supported(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: session not supported\n", __func__);
			goto err_invalid_fmt;
		}
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
		inst->fmts[fmt->type] = fmt;
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
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

		max_input_size = fmt->get_frame_size(0,
					inst->capability.height.max,
					inst->capability.width.max);

		if (f->fmt.pix_mp.plane_fmt[0].sizeimage > max_input_size ||
			f->fmt.pix_mp.plane_fmt[0].sizeimage == 0) {
			f->fmt.pix_mp.plane_fmt[0].sizeimage = max_input_size;
		}

		f->fmt.pix_mp.num_planes = fmt->num_planes;
		for (i = 0; i < fmt->num_planes; ++i) {
			inst->bufq[OUTPUT_PORT].vb2_bufq.plane_sizes[i] =
				f->fmt.pix_mp.plane_fmt[i].sizeimage;
		}
	}
err_invalid_fmt:
	return rc;
}

int msm_vdec_querycap(struct msm_vidc_inst *inst, struct v4l2_capability *cap)
{
	if (!inst || !cap) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %p, cap = %p\n", inst, cap);
		return -EINVAL;
	}
	strlcpy(cap->driver, MSM_VIDC_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_VDEC_DVC_NAME, sizeof(cap->card));
	cap->bus_info[0] = 0;
	cap->version = MSM_VIDC_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
						V4L2_CAP_VIDEO_OUTPUT_MPLANE |
						V4L2_CAP_STREAMING;
	memset(cap->reserved, 0, sizeof(cap->reserved));
	return 0;
}

int msm_vdec_enum_fmt(struct msm_vidc_inst *inst, struct v4l2_fmtdesc *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;
	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %p, f = %p\n", inst, f);
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

	memset(f->reserved, 0 , sizeof(f->reserved));
	if (fmt) {
		strlcpy(f->description, fmt->description,
				sizeof(f->description));
		f->pixelformat = fmt->fourcc;
	} else {
		dprintk(VIDC_INFO, "No more formats found\n");
		rc = -EINVAL;
	}
	return rc;
}

static int msm_vdec_queue_setup(struct vb2_queue *q,
				const struct v4l2_format *fmt,
				unsigned int *num_buffers,
				unsigned int *num_planes, unsigned int sizes[],
				void *alloc_ctxs[])
{
	int i, rc = 0;
	struct msm_vidc_inst *inst;
	struct hal_buffer_requirements *bufreq;
	int extra_idx = 0;
	struct hfi_device *hdev;
	struct hal_buffer_count_actual new_buf_count;
	enum hal_property property_id;
	if (!q || !num_buffers || !num_planes
		|| !sizes || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %p, %p, %p\n",
			q, num_buffers, num_planes);
		return -EINVAL;
	}
	inst = q->drv_priv;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = inst->fmts[OUTPUT_PORT]->num_planes;
		if (*num_buffers < MIN_NUM_OUTPUT_BUFFERS ||
				*num_buffers > MAX_NUM_OUTPUT_BUFFERS)
			*num_buffers = MIN_NUM_OUTPUT_BUFFERS;
		for (i = 0; i < *num_planes; i++) {
			sizes[i] = inst->fmts[OUTPUT_PORT]->get_frame_size(
					i, inst->capability.height.max,
					inst->capability.width.max);
		}
		property_id = HAL_PARAM_BUFFER_COUNT_ACTUAL;
		new_buf_count.buffer_type = HAL_BUFFER_INPUT;
		new_buf_count.buffer_count_actual = *num_buffers;
		rc = call_hfi_op(hdev, session_set_property,
				inst->session, property_id, &new_buf_count);
		if (rc) {
			dprintk(VIDC_WARN,
				"Failed to set new buffer count(%d) on FW, err: %d\n",
				new_buf_count.buffer_count_actual, rc);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		dprintk(VIDC_DBG, "Getting bufreqs on capture plane\n");
		*num_planes = inst->fmts[CAPTURE_PORT]->num_planes;
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to open instance\n");
			break;
		}
		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to get buffer requirements: %d\n", rc);
			break;
		}
		mutex_lock(&inst->lock);
		bufreq = get_buff_req_buffer(inst,
			msm_comm_get_hal_output_buffer(inst));
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"No buffer requirement for buffer type %x\n",
				HAL_BUFFER_OUTPUT);
			rc = -EINVAL;
			mutex_unlock(&inst->lock);
			break;
		}
		*num_buffers = max(*num_buffers, bufreq->buffer_count_min);
		if (*num_buffers != bufreq->buffer_count_actual) {
			property_id = HAL_PARAM_BUFFER_COUNT_ACTUAL;
			new_buf_count.buffer_type =
				msm_comm_get_hal_output_buffer(inst);
			new_buf_count.buffer_count_actual = *num_buffers;
			rc = call_hfi_op(hdev, session_set_property,
				inst->session, property_id, &new_buf_count);
		}
		mutex_unlock(&inst->lock);
		dprintk(VIDC_DBG, "count =  %d, size = %d, alignment = %d\n",
				inst->buff_req.buffer[1].buffer_count_actual,
				inst->buff_req.buffer[1].buffer_size,
				inst->buff_req.buffer[1].buffer_alignment);
		sizes[0] = bufreq->buffer_size;
		extra_idx =
			EXTRADATA_IDX(inst->fmts[CAPTURE_PORT]->num_planes);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			bufreq = get_buff_req_buffer(inst,
					HAL_BUFFER_EXTRADATA_OUTPUT);
			if (bufreq)
				sizes[extra_idx] = bufreq->buffer_size;
			else
				sizes[extra_idx] = 0;
		}
		break;
	default:
		dprintk(VIDC_ERR, "Invalid q type = %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_vdec_queue_output_buffers(struct msm_vidc_inst *inst)
{
	struct internal_buf *binfo;
	struct hfi_device *hdev;
	struct msm_smem *handle;
	struct vidc_frame_data frame_data = {0};
	struct hal_buffer_requirements *output_buf, *extradata_buf;
	int rc = 0;
	hdev = inst->core->device;

	output_buf = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!output_buf) {
		dprintk(VIDC_DBG,
			"This output buffer not required, buffer_type: %x\n",
			HAL_BUFFER_OUTPUT);
		return 0;
	}
	dprintk(VIDC_DBG,
		"output: num = %d, size = %d\n",
		output_buf->buffer_count_actual,
		output_buf->buffer_size);

	extradata_buf = get_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_OUTPUT);
	if (!extradata_buf) {
		dprintk(VIDC_DBG,
			"This extradata buffer not required, buffer_type: %x\n",
			HAL_BUFFER_EXTRADATA_OUTPUT);
		return 0;
	}

	hdev = inst->core->device;

	mutex_lock(&inst->lock);
	if (!list_empty(&inst->outputbufs)) {
		list_for_each_entry(binfo, &inst->outputbufs, list) {
			if (!binfo) {
				dprintk(VIDC_ERR, "Invalid parameter\n");
				mutex_unlock(&inst->lock);
				return -EINVAL;
			}
			handle = binfo->handle;
			frame_data.alloc_len = output_buf->buffer_size;
			frame_data.filled_len = 0;
			frame_data.offset = 0;
			frame_data.device_addr = handle->device_addr;
			frame_data.flags = 0;
			frame_data.extradata_addr = handle->device_addr +
				output_buf->buffer_size;
			frame_data.buffer_type = HAL_BUFFER_OUTPUT;
			rc = call_hfi_op(hdev, session_ftb,
					(void *) inst->session, &frame_data);
			binfo->buffer_ownership = FIRMWARE;
		}
	}
	mutex_unlock(&inst->lock);
	return 0;
}

static inline int start_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct vb2_buf_entry *temp;
	struct hfi_device *hdev;
	struct list_head *ptr, *next;

	hdev = inst->core->device;
	inst->in_reconfig = false;
	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY)
		rc = msm_comm_check_scaling_supported(inst);
	if (rc) {
		dprintk(VIDC_ERR, "H/w scaling is not in valid range");
		return -EINVAL;
	}
	rc = msm_comm_set_scratch_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to set scratch buffers: %d\n", rc);
		goto fail_start;
	}
	rc = msm_comm_set_persist_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to set persist buffers: %d\n", rc);
		goto fail_start;
	}

	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_set_output_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to set output buffers: %d\n", rc);
			goto fail_start;
		}
	}
	mutex_lock(&inst->core->sync_lock);
	msm_comm_scale_clocks_and_bus(inst);
	mutex_unlock(&inst->core->sync_lock);

	rc = msm_comm_try_state(inst, MSM_VIDC_START_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %p to start done state\n", inst);
		goto fail_start;
	}
	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_vdec_queue_output_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to queue output buffers: %d\n", rc);
			goto fail_start;
		}
	}
	mutex_lock(&inst->sync_lock);
	if (!list_empty(&inst->pendingq)) {
		list_for_each_safe(ptr, next, &inst->pendingq) {
			temp = list_entry(ptr, struct vb2_buf_entry, list);
			rc = msm_comm_qbuf(temp->vb);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to qbuf to hardware\n");
				break;
			}
			list_del(&temp->list);
			kfree(temp);
		}
	}
	mutex_unlock(&inst->sync_lock);
	return rc;
fail_start:
	return rc;
}

static inline int stop_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %p to start done state\n", inst);
	return rc;
}

static int msm_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	int pdata = DEFAULT_CONCEAL_COLOR;
	struct hfi_device *hdev;
	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %p\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	dprintk(VIDC_DBG,
		"Streamon called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (inst->bufq[CAPTURE_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		rc = call_hfi_op(hdev, session_set_property,
			(void *) inst->session,
			HAL_PARAM_VDEC_CONCEAL_COLOR,
			(void *) &pdata);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		break;
	default:
		dprintk(VIDC_ERR, "Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_vdec_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %p\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	dprintk(VIDC_DBG, "Streamoff called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (!inst->bufq[CAPTURE_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (!inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	default:
		dprintk(VIDC_ERR,
			"Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}

	mutex_lock(&inst->core->sync_lock);
	msm_comm_scale_clocks_and_bus(inst);
	mutex_unlock(&inst->core->sync_lock);

	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %p, cap = %d to state: %d\n",
			inst, q->type, MSM_VIDC_RELEASE_RESOURCES_DONE);
	return rc;
}

static void msm_vdec_buf_queue(struct vb2_buffer *vb)
{
	int rc;
	rc = msm_comm_qbuf(vb);
	if (rc)
		dprintk(VIDC_ERR, "Failed to queue buffer: %d\n", rc);
}

int msm_vdec_cmd(struct msm_vidc_inst *inst, struct v4l2_decoder_cmd *dec)
{
	int rc = 0;
	struct msm_vidc_core *core = inst->core;

	if (!dec || !inst || !inst->core) {
		dprintk(VIDC_ERR, "%s invalid params", __func__);
		return -EINVAL;
	}
	switch (dec->cmd) {
	case V4L2_DEC_QCOM_CMD_FLUSH:
		rc = msm_comm_flush(inst, dec->flags);
		break;
	case V4L2_DEC_CMD_STOP:
		if (core->state != VIDC_CORE_INVALID &&
			inst->state ==  MSM_VIDC_CORE_INVALID) {
			rc = msm_comm_recover_from_session_error(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed to recover from session_error: %d\n",
					rc);
		}
		rc = msm_comm_release_scratch_buffers(inst);
		if (rc)
			dprintk(VIDC_ERR,
				"Failed to release scratch buffers: %d\n", rc);
		rc = msm_comm_release_persist_buffers(inst);
		if (rc)
			pr_err("Failed to release persist buffers: %d\n", rc);
		if (inst->state == MSM_VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_INVALID) {
			dprintk(VIDC_ERR,
				"Core %p in bad state, Sending CLOSE event\n",
					core);
			msm_vidc_queue_v4l2_event(inst,
					V4L2_EVENT_MSM_VIDC_CLOSE_DONE);
			goto exit;
		}
		rc = msm_comm_try_state(inst, MSM_VIDC_CLOSE_DONE);
		/* Clients rely on this event for joining poll thread.
		 * This event should be returned even if firmware has
		 * failed to respond */
		msm_vidc_queue_v4l2_event(inst, V4L2_EVENT_MSM_VIDC_CLOSE_DONE);
		break;
	default:
		dprintk(VIDC_ERR, "Unknown Decoder Command\n");
		rc = -ENOTSUPP;
		goto exit;
	}
	if (rc) {
		dprintk(VIDC_ERR, "Failed to exec decoder cmd %d\n", dec->cmd);
		goto exit;
	}
exit:
	return rc;
}


static const struct vb2_ops msm_vdec_vb2q_ops = {
	.queue_setup = msm_vdec_queue_setup,
	.start_streaming = msm_vdec_start_streaming,
	.buf_queue = msm_vdec_buf_queue,
	.stop_streaming = msm_vdec_stop_streaming,
};

const struct vb2_ops *msm_vdec_get_vb2q_ops(void)
{
	return &msm_vdec_vb2q_ops;
}

int msm_vdec_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid input = %p\n", inst);
		return -EINVAL;
	}
	inst->fmts[OUTPUT_PORT] = &vdec_formats[1];
	inst->fmts[CAPTURE_PORT] = &vdec_formats[0];
	inst->prop.height[CAPTURE_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[CAPTURE_PORT] = DEFAULT_WIDTH;
	inst->prop.height[OUTPUT_PORT] = DEFAULT_HEIGHT;
	inst->prop.width[OUTPUT_PORT] = DEFAULT_WIDTH;
	inst->capability.height.min = MIN_SUPPORTED_HEIGHT;
	inst->capability.height.max = DEFAULT_HEIGHT;
	inst->capability.width.min = MIN_SUPPORTED_WIDTH;
	inst->capability.width.max = DEFAULT_WIDTH;
	inst->capability.buffer_mode[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->capability.buffer_mode[CAPTURE_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[OUTPUT_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->buffer_mode_set[CAPTURE_PORT] = HAL_BUFFER_MODE_STATIC;
	inst->prop.fps = 30;
	return rc;
}

static inline enum buffer_mode_type get_buf_type(int val)
{
	switch (val) {
	case V4L2_MPEG_VIDC_VIDEO_STATIC:
		return HAL_BUFFER_MODE_STATIC;
	case V4L2_MPEG_VIDC_VIDEO_RING:
		return HAL_BUFFER_MODE_RING;
	case V4L2_MPEG_VIDC_VIDEO_DYNAMIC:
		return HAL_BUFFER_MODE_DYNAMIC;
	default:
		dprintk(VIDC_ERR, "%s: invalid buf type: %d", __func__, val);
	}
	return 0;
}

static int check_tz_dynamic_buffer_support(void)
{
	int rc = 0;
	struct tz_get_feature_version tz_feature_id;
	unsigned int resp = 0;

	tz_feature_id.feature_id = TZ_DYNAMIC_BUFFER_FEATURE_ID;
	rc = scm_call(SCM_SVC_INFO,
		  TZ_INFO_GET_FEATURE_VERSION_ID, &tz_feature_id,
		  sizeof(tz_feature_id), &resp, sizeof(resp));
	if ((rc) || (resp != TZ_FEATURE_VERSION(1, 1, 0))) {
		dprintk(VIDC_DBG,
			"Dyamic buffer mode not supported, failed to get tz feature version id : %u, rc : %d, response : %u\n",
			tz_feature_id.feature_id, rc, resp);
		rc = -ENOTSUPP;
	}
	return rc;
}

static int try_set_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_nal_stream_format_supported stream_format;
	struct hal_enable_picture enable_picture;
	struct hal_enable hal_property;
	enum hal_property property_id = 0;
	u32 property_val = 0;
	void *pdata = NULL;
	struct hfi_device *hdev;
	struct hal_extradata_enable extra;
	struct hal_buffer_alloc_mode alloc_mode;
	struct hal_multi_stream multi_stream;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
		property_id =
		HAL_PARAM_NAL_STREAM_FORMAT_SELECT;
		stream_format.nal_stream_format_supported =
		(0x00000001 << ctrl->val);
		pdata = &stream_format;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
		property_id = HAL_PARAM_VDEC_OUTPUT_ORDER;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE:
		property_id =
			HAL_PARAM_VDEC_PICTURE_TYPE_DECODE;
		enable_picture.picture_type = ctrl->val;
		pdata = &enable_picture;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_KEEP_ASPECT_RATIO:
		property_id =
			HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_POST_LOOP_DEBLOCKER_MODE:
		property_id =
			HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_DIVX_FORMAT:
		property_id = HAL_PARAM_DIVX_FORMAT;
		property_val = ctrl->val;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_MB_ERROR_MAP_REPORTING:
		property_id =
			HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER:
		property_id =
			HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE:
		property_id =
			HAL_PARAM_VDEC_SYNC_FRAME_DECODE;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_SECURE:
		inst->flags |= VIDC_SECURE;
		dprintk(VIDC_DBG, "Setting secure mode to: %d\n",
				!!(inst->flags & VIDC_SECURE));
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		property_id = HAL_PARAM_INDEX_EXTRADATA;
		extra.index = msm_comm_get_hal_extradata_index(ctrl->val);
		extra.enable = 1;
		pdata = &extra;
		break;
	case V4L2_CID_MPEG_VIDC_SET_PERF_LEVEL:
		switch (ctrl->val) {
		case V4L2_CID_MPEG_VIDC_PERF_LEVEL_NOMINAL:
			inst->flags &= ~VIDC_TURBO;
			break;
		case V4L2_CID_MPEG_VIDC_PERF_LEVEL_TURBO:
			inst->flags |= VIDC_TURBO;
			break;
		default:
			dprintk(VIDC_ERR, "Perf mode %x not supported",
					ctrl->val);
			rc = -ENOTSUPP;
			break;
		}

		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_INPUT:
		if (ctrl->val == V4L2_MPEG_VIDC_VIDEO_DYNAMIC) {
			rc = -ENOTSUPP;
			break;
		}
		property_id = HAL_PARAM_BUFFER_ALLOC_MODE;
		alloc_mode.buffer_mode = get_buf_type(ctrl->val);
		alloc_mode.buffer_type = HAL_BUFFER_INPUT;
		inst->buffer_mode_set[OUTPUT_PORT] = alloc_mode.buffer_mode;
		pdata = &alloc_mode;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_ASSEMBLY:
	{
		property_id = HAL_PARAM_VDEC_FRAME_ASSEMBLY;
		hal_property.enable = ctrl->val;
		pdata = &hal_property;
		break;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_ALLOC_MODE_OUTPUT:
		property_id = HAL_PARAM_BUFFER_ALLOC_MODE;
		alloc_mode.buffer_mode = get_buf_type(ctrl->val);
		if (!(alloc_mode.buffer_mode &
			inst->capability.buffer_mode[CAPTURE_PORT])) {
			dprintk(VIDC_DBG,
				"buffer mode[%d] not supported for Capture Port\n",
				ctrl->val);
			rc = -ENOTSUPP;
			break;
		}
		if ((alloc_mode.buffer_mode == HAL_BUFFER_MODE_DYNAMIC) &&
			(inst->flags & VIDC_SECURE) &&
			check_tz_dynamic_buffer_support()) {
				rc = -ENOTSUPP;
				break;
		}
		alloc_mode.buffer_type = HAL_BUFFER_OUTPUT;
		pdata = &alloc_mode;
		inst->buffer_mode_set[CAPTURE_PORT] = alloc_mode.buffer_mode;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE:
		if (ctrl->val && !(inst->capability.pixelprocess_capabilities &
				HAL_VIDEO_DECODER_MULTI_STREAM_CAPABILITY)) {
			dprintk(VIDC_ERR, "Downscaling not supported: 0x%x",
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
			if (rc)
				dprintk(VIDC_ERR,
					"Failed :Disabling OUTPUT port : %d\n",
					rc);
			break;
		default:
			dprintk(VIDC_ERR,
				"Failed : Unsupported multi stream setting\n");
			rc = -ENOTSUPP;
			break;
		}
		break;
	default:
		break;
	}

	if (!rc && property_id) {
		dprintk(VIDC_DBG,
			"Control: HAL property=0x%x,ctrl: id=0x%x,value=0x%x\n",
			property_id, ctrl->id, ctrl->val);
			rc = call_hfi_op(hdev, session_set_property, (void *)
				inst->session, property_id, pdata);
	}

	return rc;
}

static int msm_vdec_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0, c = 0;
	struct msm_vidc_inst *inst = container_of(ctrl->handler,
				struct msm_vidc_inst, ctrl_handler);
	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}
	rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %p to start done state\n", inst);
		goto failed_open_done;
	}

	for (c = 0; c < ctrl->ncontrols; ++c) {
		if (ctrl->cluster[c]->is_new) {
			rc = try_set_ctrl(inst, ctrl->cluster[c]);
			if (rc) {
				dprintk(VIDC_ERR, "Failed setting %x",
						ctrl->cluster[c]->id);
				break;
			}
		}
	}

failed_open_done:
	if (rc)
		dprintk(VIDC_ERR, "Failed to set hal property for framesize\n");
	return rc;
}

static int msm_vdec_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
}

static const struct v4l2_ctrl_ops msm_vdec_ctrl_ops = {

	.s_ctrl = msm_vdec_op_s_ctrl,
	.g_volatile_ctrl = msm_vdec_op_g_volatile_ctrl,
};

const struct v4l2_ctrl_ops *msm_vdec_get_ctrl_ops(void)
{
	return &msm_vdec_ctrl_ops;
}

int msm_vdec_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_s_ctrl(NULL, &inst->ctrl_handler, ctrl);
}
int msm_vdec_g_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_g_ctrl(&inst->ctrl_handler, ctrl);
}

static struct v4l2_ctrl **get_cluster(int type, int *size)
{
	int c = 0, sz = 0;
	struct v4l2_ctrl **cluster = kmalloc(sizeof(struct v4l2_ctrl *) *
			NUM_CTRLS, GFP_KERNEL);

	if (type <= 0 || !size || !cluster)
		return NULL;

	for (c = 0; c < NUM_CTRLS; c++) {
		if (msm_vdec_ctrls[c].cluster & type) {
			cluster[sz] = msm_vdec_ctrls[c].priv;
			++sz;
		}
	}

	*size = sz;
	return cluster;
}

int msm_vdec_ctrl_init(struct msm_vidc_inst *inst)
{
	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg = {0};
	int ret_val = 0;

	ret_val = v4l2_ctrl_handler_init(&inst->ctrl_handler, NUM_CTRLS);

	if (ret_val) {
		dprintk(VIDC_ERR, "CTRL ERR: Control handler init failed, %d\n",
				inst->ctrl_handler.error);
		return ret_val;
	}

	for (; idx < NUM_CTRLS; idx++) {
		struct v4l2_ctrl *ctrl = NULL;
		if (IS_PRIV_CTRL(msm_vdec_ctrls[idx].id)) {
			/*add private control*/
			ctrl_cfg.def = msm_vdec_ctrls[idx].default_value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = msm_vdec_ctrls[idx].id;
			/* ctrl_cfg.is_private =
			 * msm_vdec_ctrls[idx].is_private;
			 * ctrl_cfg.is_volatile =
			 * msm_vdec_ctrls[idx].is_volatile;*/
			ctrl_cfg.max = msm_vdec_ctrls[idx].maximum;
			ctrl_cfg.min = msm_vdec_ctrls[idx].minimum;
			ctrl_cfg.menu_skip_mask =
				msm_vdec_ctrls[idx].menu_skip_mask;
			ctrl_cfg.name = msm_vdec_ctrls[idx].name;
			ctrl_cfg.ops = &msm_vdec_ctrl_ops;
			ctrl_cfg.step = msm_vdec_ctrls[idx].step;
			ctrl_cfg.type = msm_vdec_ctrls[idx].type;
			ctrl_cfg.qmenu = msm_vdec_ctrls[idx].qmenu;

			ctrl = v4l2_ctrl_new_custom(&inst->ctrl_handler,
					&ctrl_cfg, NULL);
		} else {
			if (msm_vdec_ctrls[idx].type == V4L2_CTRL_TYPE_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(
					&inst->ctrl_handler,
					&msm_vdec_ctrl_ops,
					msm_vdec_ctrls[idx].id,
					msm_vdec_ctrls[idx].maximum,
					msm_vdec_ctrls[idx].menu_skip_mask,
					msm_vdec_ctrls[idx].default_value);
			} else {
				ctrl = v4l2_ctrl_new_std(&inst->ctrl_handler,
					&msm_vdec_ctrl_ops,
					msm_vdec_ctrls[idx].id,
					msm_vdec_ctrls[idx].minimum,
					msm_vdec_ctrls[idx].maximum,
					msm_vdec_ctrls[idx].step,
					msm_vdec_ctrls[idx].default_value);
			}
		}


		msm_vdec_ctrls[idx].priv = ctrl;
	}
	ret_val = inst->ctrl_handler.error;
	if (ret_val)
		dprintk(VIDC_ERR,
			"Error adding ctrls to ctrl handle, %d\n",
			inst->ctrl_handler.error);

	/* Construct clusters */
	for (idx = 1; idx < MSM_VDEC_CTRL_CLUSTER_MAX; ++idx) {
		struct msm_vidc_ctrl_cluster *temp = NULL;
		struct v4l2_ctrl **cluster = NULL;
		int cluster_size = 0;

		cluster = get_cluster(idx, &cluster_size);
		if (!cluster || !cluster_size) {
			dprintk(VIDC_WARN, "Failed to setup cluster of type %d",
					idx);
			continue;
		}

		v4l2_ctrl_cluster(cluster_size, cluster);

		temp = kzalloc(sizeof(*temp), GFP_KERNEL);
		if (!temp) {
			ret_val = -ENOMEM;
			break;
		}

		temp->cluster = cluster;
		INIT_LIST_HEAD(&temp->list);
		list_add_tail(&temp->list, &inst->ctrl_clusters);
	}
	return ret_val;
}

int msm_vdec_ctrl_deinit(struct msm_vidc_inst *inst)
{
	struct msm_vidc_ctrl_cluster *curr, *next;
	list_for_each_entry_safe(curr, next, &inst->ctrl_clusters, list) {
		kfree(curr->cluster);
		kfree(curr);
	}
	v4l2_ctrl_handler_free(&inst->ctrl_handler);
	return 0;
}
