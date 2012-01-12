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
