/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include "vidc_hal_api.h"
#include "msm_smem.h"

#define MSM_VENC_DVC_NAME "msm_venc_8974"
#define DEFAULT_HEIGHT 720
#define DEFAULT_WIDTH 1280
#define MIN_NUM_OUTPUT_BUFFERS 2
#define MAX_NUM_OUTPUT_BUFFERS 8
#define MIN_BIT_RATE 64
#define MAX_BIT_RATE 8000
#define DEFAULT_BIT_RATE 64
#define BIT_RATE_STEP 1
#define MIN_FRAME_RATE 1
#define MAX_FRAME_RATE 120
#define DEFAULT_FRAME_RATE 30
#define MAX_SLICE_BYTE_SIZE 1024
#define MIN_SLICE_BYTE_SIZE 1024
#define MAX_SLICE_MB_SIZE 300
#define I_FRAME_QP 26
#define P_FRAME_QP 28
#define B_FRAME_QP 30
#define MAX_INTRA_REFRESH_MBS 300
#define L_MODE V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY

static const char *const mpeg_video_rate_control[] = {
	"No Rate Control",
	"VBR VFR",
	"VBR CFR",
	"CBR VFR",
	"CBR CFR",
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
static const struct msm_vidc_ctrl msm_venc_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE,
		.name = "Frame Rate",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_FRAME_RATE,
		.maximum = MAX_FRAME_RATE,
		.default_value = DEFAULT_FRAME_RATE,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD,
		.name = "IDR Period",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 10*MAX_FRAME_RATE,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES,
		.name = "Intra Period for P frames",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 10*DEFAULT_FRAME_RATE,
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
		.maximum = 10*DEFAULT_FRAME_RATE,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME,
		.name = "Request I Frame",
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL,
		.name = "Rate Control",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_CFR,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_OFF) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_VFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_VBR_CFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_VFR) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL_CBR_CFR)
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
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL,
		.name = "CABAC Model",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_1,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_1) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_2)
		),
		.qmenu = h264_video_entropy_cabac_model,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE,
		.name = "H264 Profile",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.maximum = V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH,
		.default_value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
		.step = 1,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL,
		.name = "H264 Level",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.maximum = V4L2_MPEG_VIDEO_H264_LEVEL_5_1,
		.default_value = V4L2_MPEG_VIDEO_H264_LEVEL_1_0,
		.step = 1,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION,
		.name = "Rotation",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_NONE,
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_270,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_ROTATION_NONE,
		.step = 0,
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
		.name = "I Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = I_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP,
		.name = "P Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = P_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP,
		.name = "B Frame Quantization",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 1,
		.maximum = 51,
		.default_value = B_FRAME_QP,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE,
		.name = "Slice Mode",
		.type = V4L2_CTRL_TYPE_MENU,
		.minimum = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.maximum = V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES,
		.default_value = V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE,
		.step = 1,
		.menu_skip_mask = 0,
	},
	{
		.id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES,
		.name = "Slice Byte Size",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = MIN_SLICE_BYTE_SIZE,
		.maximum = MAX_SLICE_BYTE_SIZE,
		.default_value = 0,
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
		.maximum = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM,
		.default_value = V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_NONE,
		.step = 0,
		.menu_skip_mask = ~(
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_NONE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_CYCLIC) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_ADAPTIVE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_CYCLIC_ADAPTIVE) |
		(1 << V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_RANDOM)
		),
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
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
		.maximum = L_MODE,
		.default_value = V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED,
		.step = 1,
		.menu_skip_mask = ~(
		(1 << V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED) |
		(1 << V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED) |
		(1 << L_MODE)
		),
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_venc_ctrls)

static u32 get_frame_size_nv12(int plane, u32 height, u32 width)
{
	int stride = (width + 31) & (~31);
	return height * stride * 3/2;
}
static u32 get_frame_size_nv21(int plane, u32 height, u32 width)
{
	return height * width * 2;
}

static u32 get_frame_size_compressed(int plane, u32 height, u32 width)
{
	return width * height / 2;
}
static struct hal_quantization
	venc_quantization = {I_FRAME_QP, P_FRAME_QP, B_FRAME_QP};
static struct hal_intra_period
	venc_intra_period = {2*DEFAULT_FRAME_RATE-1 , 0};
static struct hal_profile_level
	venc_profile_level = {V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE,
				V4L2_MPEG_VIDEO_H264_LEVEL_1_0};
static struct hal_h264_entropy_control
	venc_h264_entropy_control = {V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC,
				V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL_0};
static struct hal_multi_slice_control
	venc_multi_slice_control = {V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_SINGLE ,
				0};

static const struct msm_vidc_format venc_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 1,
		.get_frame_size = get_frame_size_nv12,
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
		.name = "YCrCb Semiplanar 4:2:0",
		.description = "Y/CrCb 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.num_planes = 1,
		.get_frame_size = get_frame_size_nv21,
		.type = OUTPUT_PORT,
	},
};

static int msm_venc_queue_setup(struct vb2_queue *q, unsigned int *num_buffers,
			unsigned int *num_planes, unsigned long sizes[],
			void *alloc_ctxs[])
{
	int i, rc = 0;
	struct msm_vidc_inst *inst;
	struct hal_frame_size frame_sz;
	unsigned long flags;
	if (!q || !q->drv_priv) {
		pr_err("Invalid input, q = %p\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		*num_planes = 1;
		if (*num_buffers < MIN_NUM_OUTPUT_BUFFERS ||
				*num_buffers > MAX_NUM_OUTPUT_BUFFERS)
			*num_buffers = MIN_NUM_OUTPUT_BUFFERS;
		for (i = 0; i < *num_planes; i++) {
			sizes[i] = inst->fmts[OUTPUT_PORT]->get_frame_size(
					i, inst->height, inst->width);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			pr_err("Failed to open instance\n");
			break;
		}
		frame_sz.buffer_type = HAL_BUFFER_INPUT;
		frame_sz.width = inst->width;
		frame_sz.height = inst->height;
		pr_debug("width = %d, height = %d\n",
				frame_sz.width, frame_sz.height);
		rc = vidc_hal_session_set_property((void *)inst->session,
				HAL_PARAM_FRAME_SIZE, &frame_sz);
		if (rc) {
			pr_err("Failed to set hal property for framesize\n");
			break;
		}
		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			pr_err("Failed to get buffer requirements: %d\n", rc);
			break;
		}
		*num_planes = 1;
		spin_lock_irqsave(&inst->lock, flags);
		*num_buffers = inst->buff_req.buffer[0].buffer_count_actual;
		spin_unlock_irqrestore(&inst->lock, flags);
		pr_debug("size = %d, alignment = %d, count = %d\n",
				inst->buff_req.buffer[0].buffer_size,
				inst->buff_req.buffer[0].buffer_alignment,
				inst->buff_req.buffer[0].buffer_count_actual);
		for (i = 0; i < *num_planes; i++) {
			sizes[i] = inst->fmts[CAPTURE_PORT]->get_frame_size(
					i, inst->height, inst->width);
		}

		break;
	default:
		pr_err("Invalid q type = %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static inline int start_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	unsigned long flags;
	struct vb2_buf_entry *temp;
	struct list_head *ptr, *next;
	rc = msm_comm_set_scratch_buffers(inst);
	if (rc) {
		pr_err("Failed to set scratch buffers: %d\n", rc);
		goto fail_start;
	}
	rc = msm_comm_try_state(inst, MSM_VIDC_START_DONE);
	if (rc) {
		pr_err("Failed to move inst: %p to start done state\n",
				inst);
		goto fail_start;
	}
	spin_lock_irqsave(&inst->lock, flags);
	if (!list_empty(&inst->pendingq)) {
		list_for_each_safe(ptr, next, &inst->pendingq) {
			temp = list_entry(ptr, struct vb2_buf_entry, list);
			rc = msm_comm_qbuf(temp->vb);
			if (rc) {
				pr_err("Failed to qbuf to hardware\n");
				break;
			}
			list_del(&temp->list);
			kfree(temp);
		}
	}
	spin_unlock_irqrestore(&inst->lock, flags);
	return rc;
fail_start:
	return rc;
}

static int msm_venc_start_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	if (!q || !q->drv_priv) {
		pr_err("Invalid input, q = %p\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	pr_debug("Streamon called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		if (inst->vb2_bufq[CAPTURE_PORT].streaming)
			rc = start_streaming(inst);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (inst->vb2_bufq[OUTPUT_PORT].streaming)
			rc = start_streaming(inst);
		break;
	default:
		pr_err("Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_venc_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	if (!q || !q->drv_priv) {
		pr_err("Invalid input, q = %p\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	pr_debug("Streamoff called on: %d capability\n", q->type);
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		rc = msm_comm_try_state(inst, MSM_VIDC_CLOSE_DONE);
		break;
	default:
		pr_err("Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	if (rc)
		pr_err("Failed to move inst: %p, cap = %d to state: %d\n",
				inst, q->type, MSM_VIDC_CLOSE_DONE);
	return rc;
}

static void msm_venc_buf_queue(struct vb2_buffer *vb)
{
	int rc;
	rc = msm_comm_qbuf(vb);
	if (rc)
		pr_err("Failed to queue buffer: %d\n", rc);
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

static int msm_venc_op_s_ctrl(struct v4l2_ctrl *ctrl)
{

	int rc = 0;
	struct v4l2_control control;
	struct hal_frame_rate frame_rate;
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
	u32 control_idx = 0;
	u32 property_id = 0;
	u32 property_val = 0;
	void *pdata;
	struct msm_vidc_inst *inst = container_of(ctrl->handler,
					struct msm_vidc_inst, ctrl_handler);

	control.id = ctrl->id;
	control.value = ctrl->val;

	switch (control.id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE:
			property_id =
				HAL_CONFIG_FRAME_RATE;
			frame_rate.frame_rate = control.value;
			frame_rate.buffer_type = HAL_BUFFER_OUTPUT;
			pdata = &frame_rate;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_IDR_PERIOD:
		property_id =
			HAL_CONFIG_VENC_IDR_PERIOD;
		idr_period.idr_period = control.value;
			pdata = &idr_period;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_NUM_P_FRAMES:
		property_id =
			HAL_CONFIG_VENC_INTRA_PERIOD;
		intra_period.pframes = control.value;
		venc_intra_period.pframes = control.value;
		intra_period.bframes = venc_intra_period.bframes;
		pdata = &intra_period;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES:
		property_id =
			HAL_CONFIG_VENC_INTRA_PERIOD;
		intra_period.bframes = control.value;
		venc_intra_period.bframes = control.value;
		intra_period.pframes = venc_intra_period.pframes;
		pdata = &intra_period;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_REQUEST_IFRAME:
		property_id =
			HAL_CONFIG_VENC_REQUEST_IFRAME;
		request_iframe.enable = control.value;
		pdata = &request_iframe;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_RATE_CONTROL:
		property_id =
			HAL_PARAM_VENC_RATE_CONTROL;
		property_val = control.value;
		pdata = &property_val;
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		property_id =
			HAL_CONFIG_VENC_TARGET_BITRATE;
		bitrate.bit_rate = control.value;
		pdata = &bitrate;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		property_id =
			HAL_PARAM_VENC_H264_ENTROPY_CONTROL;
		h264_entropy_control.entropy_mode = control.value;
		venc_h264_entropy_control.entropy_mode = control.value;
		h264_entropy_control.cabac_model =
			venc_h264_entropy_control.cabac_model;
		pdata = &h264_entropy_control;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_H264_CABAC_MODEL:
		property_id =
			HAL_PARAM_VENC_H264_ENTROPY_CONTROL;
		h264_entropy_control.cabac_model = control.value;
		venc_h264_entropy_control.cabac_model = control.value;
		h264_entropy_control.entropy_mode =
			venc_h264_entropy_control.entropy_mode;
		pdata = &h264_entropy_control;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;

		switch (control.value) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			control.value = HAL_H264_PROFILE_BASELINE;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			control.value = HAL_H264_PROFILE_MAIN;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_EXTENDED:
			control.value = HAL_H264_PROFILE_EXTENDED;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
			control.value = HAL_H264_PROFILE_HIGH;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_10:
			control.value = HAL_H264_PROFILE_HIGH10;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_422:
			control.value = HAL_H264_PROFILE_HIGH422;
			break;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH_444_PREDICTIVE:
			control.value = HAL_H264_PROFILE_HIGH444;
			break;
		default:
			break;
			}
		profile_level.profile = control.value;
		venc_profile_level.profile = control.value;
		profile_level.level = venc_profile_level.level;
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		property_id =
			HAL_PARAM_PROFILE_LEVEL_CURRENT;

		switch (control.value) {
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_0:
			control.value = HAL_H264_LEVEL_1;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1B:
			control.value = HAL_H264_LEVEL_1b;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_1:
			control.value = HAL_H264_LEVEL_11;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_2:
			control.value = HAL_H264_LEVEL_12;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_1_3:
			control.value = HAL_H264_LEVEL_13;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_0:
			control.value = HAL_H264_LEVEL_2;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_1:
			control.value = HAL_H264_LEVEL_21;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_2_2:
			control.value = HAL_H264_LEVEL_22;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_0:
			control.value = HAL_H264_LEVEL_3;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_1:
			control.value = HAL_H264_LEVEL_31;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_3_2:
			control.value = HAL_H264_LEVEL_32;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_0:
			control.value = HAL_H264_LEVEL_4;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_1:
			control.value = HAL_H264_LEVEL_41;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_4_2:
			control.value = HAL_H264_LEVEL_42;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_0:
			control.value = HAL_H264_LEVEL_3;
			break;
		case V4L2_MPEG_VIDEO_H264_LEVEL_5_1:
			control.value = HAL_H264_LEVEL_51;
			break;
		default:
			break;
		}
		profile_level.level = control.value;
		venc_profile_level.level = control.value;
		profile_level.profile = venc_profile_level.profile;
		pdata = &profile_level;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ROTATION:
		property_id =
			HAL_CONFIG_VPE_OPERATIONS;
		operations.rotate = control.value;
		pdata = &operations;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
		property_id =
			HAL_PARAM_VENC_SESSION_QP;
		quantization.qpi = control.value;
		venc_quantization.qpi = control.value;
		quantization.qpp = venc_quantization.qpp;
		quantization.qpb = venc_quantization.qpb;
		pdata = &quantization;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
		property_id =
			HAL_PARAM_VENC_SESSION_QP;
		quantization.qpp = control.value;
		venc_quantization.qpp = control.value;
		quantization.qpi = venc_quantization.qpi;
		quantization.qpb = venc_quantization.qpb;
		pdata = &quantization;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
		property_id =
			HAL_PARAM_VENC_SESSION_QP;
		quantization.qpb = control.value;
		venc_quantization.qpb = control.value;
		quantization.qpi = venc_quantization.qpi;
		quantization.qpp = venc_quantization.qpp;
		pdata = &quantization;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		property_id =
			HAL_PARAM_VENC_MULTI_SLICE_CONTROL;
		multi_slice_control.multi_slice = control.value;
		venc_multi_slice_control.multi_slice = control.value;
		multi_slice_control.slice_size =
			venc_multi_slice_control.slice_size;
		pdata = &multi_slice_control;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		property_id =
			HAL_PARAM_VENC_MULTI_SLICE_CONTROL;
		multi_slice_control.multi_slice =
			venc_multi_slice_control.multi_slice;
		multi_slice_control.slice_size = control.value;
		venc_multi_slice_control.slice_size = control.value;
		pdata = &multi_slice_control;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_INTRA_REFRESH_MODE:
		property_id =
			HAL_PARAM_VENC_INTRA_REFRESH;
		intra_refresh.mode = control.value;
		pdata = &intra_refresh;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_AIR_MBS:
		property_id =
			HAL_PARAM_VENC_INTRA_REFRESH;
		intra_refresh.air_mbs = control.value;
		pdata = &intra_refresh;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_AIR_REF:
		property_id =
			HAL_PARAM_VENC_INTRA_REFRESH;
		intra_refresh.air_ref = control.value;
		pdata = &intra_refresh;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_CIR_MBS:
		property_id =
			HAL_PARAM_VENC_INTRA_REFRESH;
		intra_refresh.cir_mbs = control.value;
		pdata = &intra_refresh;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		property_id =
			HAL_PARAM_VENC_H264_DEBLOCK_CONTROL;
		h264_db_control.mode = control.value;
		pdata = &h264_db_control;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:
		property_id =
			HAL_PARAM_VENC_H264_DEBLOCK_CONTROL;
		h264_db_control.slice_alpha_offset = control.value;
		pdata = &h264_db_control;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:
		property_id =
			HAL_PARAM_VENC_H264_DEBLOCK_CONTROL;
		h264_db_control.slicebeta_offset = control.value;
		pdata = &h264_db_control;
	default:
		break;
	}
	if (property_id) {
		pr_err("Control: HAL property=%d,ctrl_id=%d,ctrl_value=%d\n",
				property_id,
				msm_venc_ctrls[control_idx].id,
				control.value);
		rc = vidc_hal_session_set_property((void *)inst->session,
				property_id, pdata);
	}
	if (rc)
		pr_err("Failed to set hal property for framesize\n");
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
		pr_err("Invalid input = %p\n", inst);
		return -EINVAL;
	}
	inst->fmts[CAPTURE_PORT] = &venc_formats[1];
	inst->fmts[OUTPUT_PORT] = &venc_formats[0];
	inst->height = DEFAULT_HEIGHT;
	inst->width = DEFAULT_WIDTH;
	return rc;
}

int msm_venc_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_s_ctrl(&inst->ctrl_handler, ctrl);
}
int msm_venc_g_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_g_ctrl(&inst->ctrl_handler, ctrl);
}

int msm_venc_querycap(struct msm_vidc_inst *inst, struct v4l2_capability *cap)
{
	if (!inst || !cap) {
		pr_err("Invalid input, inst = %p, cap = %p\n", inst, cap);
		return -EINVAL;
	}
	strlcpy(cap->driver, MSM_VIDC_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_VENC_DVC_NAME, sizeof(cap->card));
	cap->bus_info[0] = 0;
	cap->version = MSM_VIDC_VERSION;
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
		pr_err("Invalid input, inst = %p, f = %p\n", inst, f);
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

	memset(f->reserved, 0 , sizeof(f->reserved));
	if (fmt) {
		strlcpy(f->description, fmt->description,
				sizeof(f->description));
		f->pixelformat = fmt->fourcc;
	} else {
		pr_err("No more formats found\n");
		rc = -EINVAL;
	}
	return rc;
}

int msm_venc_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;
	int i;
	if (!inst || !f) {
		pr_err("Invalid input, inst = %p, format = %p\n", inst, f);
		return -EINVAL;
	}
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_fourcc(venc_formats,
			ARRAY_SIZE(venc_formats), f->fmt.pix_mp.pixelformat,
			CAPTURE_PORT);
		if (fmt && fmt->type != CAPTURE_PORT) {
			pr_err("Format: %d not supported on CAPTURE port\n",
					f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto exit;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		inst->width = f->fmt.pix_mp.width;
		inst->height = f->fmt.pix_mp.height;
		fmt = msm_comm_get_pixel_fmt_fourcc(venc_formats,
			ARRAY_SIZE(venc_formats), f->fmt.pix_mp.pixelformat,
			OUTPUT_PORT);
		if (fmt && fmt->type != OUTPUT_PORT) {
			pr_err("Format: %d not supported on OUTPUT port\n",
					f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto exit;
		}
	}

	if (fmt) {
		for (i = 0; i < fmt->num_planes; ++i) {
			f->fmt.pix_mp.plane_fmt[i].sizeimage =
				fmt->get_frame_size(i, f->fmt.pix_mp.height,
						f->fmt.pix_mp.width);
		}
		inst->fmts[fmt->type] = fmt;
		if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
			if (rc) {
				pr_err("Failed to open instance\n");
				goto exit;
			}
		}
	} else {
		pr_err("Buf type not recognized, type = %d\n",
					f->type);
		rc = -EINVAL;
	}
exit:
	return rc;
}

int msm_venc_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;
	int i;
	if (!inst || !f) {
		pr_err("Invalid input, inst = %p, format = %p\n", inst, f);
		return -EINVAL;
	}
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		fmt = inst->fmts[CAPTURE_PORT];
	else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		fmt = inst->fmts[OUTPUT_PORT];

	if (fmt) {
		f->fmt.pix_mp.pixelformat = fmt->fourcc;
		f->fmt.pix_mp.height = inst->height;
		f->fmt.pix_mp.width = inst->width;
		for (i = 0; i < fmt->num_planes; ++i) {
			f->fmt.pix_mp.plane_fmt[i].sizeimage =
			fmt->get_frame_size(i, inst->height, inst->width);
		}
	} else {
		pr_err("Buf type not recognized, type = %d\n",
					f->type);
		rc = -EINVAL;
	}
	return rc;
}

int msm_venc_reqbufs(struct msm_vidc_inst *inst, struct v4l2_requestbuffers *b)
{
	struct vb2_queue *q = NULL;
	int rc = 0;
	if (!inst || !b) {
		pr_err("Invalid input, inst = %p, buffer = %p\n", inst, b);
		return -EINVAL;
	}
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		pr_err("Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}

	rc = vb2_reqbufs(q, b);
	if (rc)
		pr_err("Failed to get reqbufs, %d\n", rc);
	return rc;
}

int msm_venc_prepare_buf(struct msm_vidc_inst *inst,
					struct v4l2_buffer *b)
{
	int rc = 0;
	int i;
	struct vidc_buffer_addr_info buffer_info;

	switch (b->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		for (i = 0; i < b->length; i++) {
			pr_err("device_addr = %ld, size = %d\n",
				b->m.planes[i].m.userptr,
				b->m.planes[i].length);
			buffer_info.buffer_size = b->m.planes[i].length;
			buffer_info.buffer_type = HAL_BUFFER_OUTPUT;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr =
				b->m.planes[i].m.userptr;
			buffer_info.extradata_size = 0;
			buffer_info.extradata_addr = 0;
			rc = vidc_hal_session_set_buffers((void *)inst->session,
					&buffer_info);
			if (rc)
				pr_err("vidc_hal_session_set_buffers failed");
		}
		break;
	default:
		pr_err("Buffer type not recognized: %d\n", b->type);
		break;
	}
	return rc;
}

int msm_venc_qbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct vb2_queue *q = NULL;
	int rc = 0;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		pr_err("Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}
	rc = vb2_qbuf(q, b);
	if (rc)
		pr_err("Failed to qbuf, %d\n", rc);
	return rc;
}

int msm_venc_dqbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct vb2_queue *q = NULL;
	int rc = 0;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		pr_err("Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}
	rc = vb2_dqbuf(q, b, true);
	if (rc)
		pr_err("Failed to qbuf, %d\n", rc);
	return rc;
}

int msm_venc_streamon(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
{
	int rc = 0;
	struct vb2_queue *q;
	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		pr_err("Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	pr_debug("Calling streamon\n");
	rc = vb2_streamon(q, i);
	if (rc)
		pr_err("streamon failed on port: %d\n", i);
	return rc;
}

int msm_venc_streamoff(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
{
	int rc = 0;
	struct vb2_queue *q;
	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		pr_err("Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	pr_debug("Calling streamoff\n");
	rc = vb2_streamoff(q, i);
	if (rc)
		pr_err("streamoff failed on port: %d\n", i);
	return rc;
}

int msm_venc_ctrl_init(struct msm_vidc_inst *inst)
{

	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg;
	int ret_val = 0;
	ret_val = v4l2_ctrl_handler_init(&inst->ctrl_handler, NUM_CTRLS);
	if (ret_val) {
		pr_err("CTRL ERR: Control handler init failed, %d\n",
			inst->ctrl_handler.error);
		return ret_val;
	}

	for (; idx < NUM_CTRLS; idx++) {
		if (IS_PRIV_CTRL(msm_venc_ctrls[idx].id)) {
			ctrl_cfg.def = msm_venc_ctrls[idx].default_value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = msm_venc_ctrls[idx].id;
			ctrl_cfg.max = msm_venc_ctrls[idx].maximum;
			ctrl_cfg.min = msm_venc_ctrls[idx].minimum;
			ctrl_cfg.menu_skip_mask =
				msm_venc_ctrls[idx].menu_skip_mask;
			ctrl_cfg.name = msm_venc_ctrls[idx].name;
			ctrl_cfg.ops = &msm_venc_ctrl_ops;
			ctrl_cfg.step = msm_venc_ctrls[idx].step;
			ctrl_cfg.type = msm_venc_ctrls[idx].type;
			ctrl_cfg.qmenu = msm_venc_ctrls[idx].qmenu;
			v4l2_ctrl_new_custom(&inst->ctrl_handler,
				&ctrl_cfg, NULL);
		} else {
			if (msm_venc_ctrls[idx].type == V4L2_CTRL_TYPE_MENU) {
				v4l2_ctrl_new_std_menu(&inst->ctrl_handler,
					&msm_venc_ctrl_ops,
					msm_venc_ctrls[idx].id,
					msm_venc_ctrls[idx].maximum,
					msm_venc_ctrls[idx].menu_skip_mask,
					msm_venc_ctrls[idx].default_value);
			} else {
				v4l2_ctrl_new_std(&inst->ctrl_handler,
					&msm_venc_ctrl_ops,
					msm_venc_ctrls[idx].id,
					msm_venc_ctrls[idx].minimum,
					msm_venc_ctrls[idx].maximum,
					msm_venc_ctrls[idx].step,
					msm_venc_ctrls[idx].default_value);
			}
		}
	}
	ret_val = inst->ctrl_handler.error;
	if (ret_val)
		pr_err("CTRL ERR: Error adding ctrls to ctrl handle, %d\n",
				inst->ctrl_handler.error);
	return ret_val;
}
