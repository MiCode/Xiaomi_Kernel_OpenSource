/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#define MSM_VDEC_DVC_NAME "msm_vdec_8974"
#define MAX_PLANES 1
#define DEFAULT_HEIGHT 720
#define DEFAULT_WIDTH 1280
#define MIN_NUM_OUTPUT_BUFFERS 2
#define MAX_NUM_OUTPUT_BUFFERS 6

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
static const struct msm_vidc_ctrl msm_vdec_ctrls[] = {
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
	},
};

#define NUM_CTRLS ARRAY_SIZE(msm_vdec_ctrls)

static u32 get_frame_size_nv12(int plane,
					u32 height, u32 width)
{
	return (ALIGN(height, 32) * ALIGN(width, 32) * 3) / 2;
}
static u32 get_frame_size_nv21(int plane,
					u32 height, u32 width)
{
	return height * width * 2;
}

static u32 get_frame_size_compressed(int plane,
					u32 height, u32 width)
{
	return 0x500000;
}

static const struct msm_vidc_format vdec_formats[] = {
	{
		.name = "YCbCr Semiplanar 4:2:0",
		.description = "Y/CbCr 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 1,
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
		.name = "H264",
		.description = "H264 compressed format",
		.fourcc = V4L2_PIX_FMT_H264,
		.num_planes = 1,
		.get_frame_size = get_frame_size_compressed,
		.type = OUTPUT_PORT,
	},
	{
		.name = "YCrCb Semiplanar 4:2:0",
		.description = "Y/CrCb 4:2:0",
		.fourcc = V4L2_PIX_FMT_NV21,
		.num_planes = 1,
		.get_frame_size = get_frame_size_nv21,
		.type = CAPTURE_PORT,
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

int msm_vdec_streamoff(struct msm_vidc_inst *inst, enum v4l2_buf_type i)
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

int msm_vdec_prepare_buf(struct msm_vidc_inst *inst,
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
			if (rc) {
				pr_err("vidc_hal_session_set_buffers failed");
				break;
			}
		}
		break;
	default:
		pr_err("Buffer type not recognized: %d\n", b->type);
		break;
	}
	return rc;
}

int msm_vdec_release_buf(struct msm_vidc_inst *inst,
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
			pr_debug("Release device_addr = %ld, size = %d\n",
				b->m.planes[i].m.userptr,
				b->m.planes[i].length);
			buffer_info.buffer_size = b->m.planes[i].length;
			buffer_info.buffer_type = HAL_BUFFER_OUTPUT;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr =
				 b->m.planes[i].m.userptr;
			buffer_info.extradata_addr = 0;
			rc = vidc_hal_session_release_buffers(
				(void *)inst->session, &buffer_info);
			if (rc)
				pr_err("vidc_hal_session_release_buffers failed");
		}
		break;
	default:
		pr_err("Buffer type not recognized: %d\n", b->type);
		break;
	}
	return rc;
}

int msm_vdec_qbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
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
int msm_vdec_dqbuf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
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
		pr_err("Failed to dqbuf, %d\n", rc);
	return rc;
}

int msm_vdec_reqbufs(struct msm_vidc_inst *inst, struct v4l2_requestbuffers *b)
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

int msm_vdec_g_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
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
		if (inst->in_reconfig == true) {
			inst->height = inst->reconfig_height;
			inst->width = inst->reconfig_width;
		}
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

int msm_vdec_s_fmt(struct msm_vidc_inst *inst, struct v4l2_format *f)
{
	const struct msm_vidc_format *fmt = NULL;
	int rc = 0;
	int i;
	if (!inst || !f) {
		pr_err("Invalid input, inst = %p, format = %p\n", inst, f);
		return -EINVAL;
	}
	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		inst->width = f->fmt.pix_mp.width;
		inst->height = f->fmt.pix_mp.height;
		fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->fmt.pix_mp.pixelformat,
			CAPTURE_PORT);
		if (fmt && fmt->type != CAPTURE_PORT) {
			pr_err("Format: %d not supported on CAPTURE port\n",
					f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = msm_comm_get_pixel_fmt_fourcc(vdec_formats,
			ARRAY_SIZE(vdec_formats), f->fmt.pix_mp.pixelformat,
			OUTPUT_PORT);
		if (fmt && fmt->type != OUTPUT_PORT) {
			pr_err("Format: %d not supported on OUTPUT port\n",
					f->fmt.pix_mp.pixelformat);
			rc = -EINVAL;
			goto err_invalid_fmt;
		}
	}

	if (fmt) {
		for (i = 0; i < fmt->num_planes; ++i) {
			f->fmt.pix_mp.plane_fmt[i].sizeimage =
				fmt->get_frame_size(i, f->fmt.pix_mp.height,
						f->fmt.pix_mp.width);
		}
		inst->fmts[fmt->type] = fmt;
		if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			rc = msm_comm_try_state(inst, MSM_VIDC_OPEN);
			if (rc) {
				pr_err("Failed to open instance\n");
				goto err_invalid_fmt;
			}
		}
	} else {
		pr_err("Buf type not recognized, type = %d\n",
					f->type);
		rc = -EINVAL;
	}
err_invalid_fmt:
	return rc;
}

int msm_vdec_querycap(struct msm_vidc_inst *inst, struct v4l2_capability *cap)
{
	if (!inst || !cap) {
		pr_err("Invalid input, inst = %p, cap = %p\n", inst, cap);
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
		pr_err("Invalid input, inst = %p, f = %p\n", inst, f);
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
		pr_err("No more formats found\n");
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
	struct hal_frame_size frame_sz;
	unsigned long flags;
	if (!q || !q->drv_priv) {
		pr_err("Invalid input, q = %p\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		*num_planes = 1;
		if (*num_buffers < MIN_NUM_OUTPUT_BUFFERS ||
				*num_buffers > MAX_NUM_OUTPUT_BUFFERS)
			*num_buffers = MIN_NUM_OUTPUT_BUFFERS;
		for (i = 0; i < *num_planes; i++) {
			sizes[i] = inst->fmts[OUTPUT_PORT]->get_frame_size(
					i, inst->height, inst->width);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		pr_debug("Getting bufreqs on capture plane\n");
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			pr_err("Failed to open instance\n");
			break;
		}
		frame_sz.buffer_type = HAL_BUFFER_OUTPUT;
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
		*num_buffers = inst->buff_req.buffer[1].buffer_count_actual;
		spin_unlock_irqrestore(&inst->lock, flags);
		pr_debug("size = %d, alignment = %d\n",
				inst->buff_req.buffer[1].buffer_size,
				inst->buff_req.buffer[1].buffer_alignment);
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
	struct v4l2_control control;
	struct hal_nal_stream_format_supported stream_format;
	struct hal_enable_picture enable_picture;
	struct hal_enable hal_property;
	u32 control_idx = 0;
	enum hal_property property_id = 0;
	u32 property_val = 0;
	void *pdata;
	rc = msm_comm_set_scratch_buffers(inst);
	if (rc) {
		pr_err("Failed to set scratch buffers: %d\n", rc);
		goto fail_start;
	}
	for (; control_idx < NUM_CTRLS; control_idx++) {
		control.id = msm_vdec_ctrls[control_idx].id;
		rc = v4l2_g_ctrl(&inst->ctrl_handler, &control);
		if (rc) {
			pr_err("Failed to get control value for ID=%d\n",
				   msm_vdec_ctrls[control_idx].id);
		} else {
			property_id = 0;
			switch (control.id) {
			case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_FORMAT:
				property_id =
					HAL_PARAM_NAL_STREAM_FORMAT_SELECT;
				stream_format.nal_stream_format_supported =
					(0x00000001 << control.value);
				pdata = &stream_format;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_OUTPUT_ORDER:
				property_id = HAL_PARAM_VDEC_OUTPUT_ORDER;
				property_val = control.value;
				pdata = &property_val;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_ENABLE_PICTURE_TYPE:
				property_id =
					HAL_PARAM_VDEC_PICTURE_TYPE_DECODE;
				enable_picture.picture_type = control.value;
				pdata = &enable_picture;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_KEEP_ASPECT_RATIO:
				property_id =
				HAL_PARAM_VDEC_OUTPUT2_KEEP_ASPECT_RATIO;
				hal_property.enable = control.value;
				pdata = &hal_property;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_POST_LOOP_DEBLOCKER_MODE:
				property_id =
					HAL_CONFIG_VDEC_POST_LOOP_DEBLOCKER;
				hal_property.enable = control.value;
				pdata = &hal_property;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_DIVX_FORMAT:
				property_id = HAL_PARAM_DIVX_FORMAT;
				property_val = control.value;
				pdata = &property_val;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_MB_ERROR_MAP_REPORTING:
				property_id =
					HAL_CONFIG_VDEC_MB_ERROR_MAP_REPORTING;
				hal_property.enable = control.value;
				pdata = &hal_property;
				break;
			case V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER:
				property_id =
					HAL_PARAM_VDEC_CONTINUE_DATA_TRANSFER;
				hal_property.enable = control.value;
				pdata = &hal_property;
				break;
			default:
				break;
			}
			if (property_id) {
				pr_err("Control: HAL property=%x,ctrl_id=%x,ctrl_value=%d\n",
					   property_id,
					   msm_vdec_ctrls[control_idx].id,
					   control.value);
				rc = vidc_hal_session_set_property((void *)
						inst->session, property_id,
						pdata);
			}
			if (rc)
				pr_err("Failed to set hal property for framesize\n");
		}
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

static int msm_vdec_start_streaming(struct vb2_queue *q, unsigned int count)
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
		inst->in_reconfig = false;
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

static int msm_vdec_stop_streaming(struct vb2_queue *q)
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
		if (!inst->vb2_bufq[CAPTURE_PORT].streaming)
			rc = msm_comm_try_state(inst, MSM_VIDC_CLOSE_DONE);
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		if (!inst->vb2_bufq[OUTPUT_PORT].streaming)
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

static void msm_vdec_buf_queue(struct vb2_buffer *vb)
{
	int rc;
	rc = msm_comm_qbuf(vb);
	if (rc)
		pr_err("Failed to queue buffer: %d\n", rc);
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
		pr_err("Invalid input = %p\n", inst);
		return -EINVAL;
	}
	inst->fmts[OUTPUT_PORT] = &vdec_formats[1];
	inst->fmts[CAPTURE_PORT] = &vdec_formats[0];
	inst->height = DEFAULT_HEIGHT;
	inst->width = DEFAULT_WIDTH;
	return rc;
}

static int msm_vdec_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	return 0;
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
int msm_vdec_ctrl_init(struct msm_vidc_inst *inst)
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
		if (IS_PRIV_CTRL(msm_vdec_ctrls[idx].id)) {
			/*add private control*/
			ctrl_cfg.def = msm_vdec_ctrls[idx].default_value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = msm_vdec_ctrls[idx].id;
			/*ctrl_cfg.is_private =
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

			v4l2_ctrl_new_custom(&inst->ctrl_handler,
					&ctrl_cfg, NULL);
		} else {
			if (msm_vdec_ctrls[idx].type == V4L2_CTRL_TYPE_MENU) {
				v4l2_ctrl_new_std_menu(&inst->ctrl_handler,
					&msm_vdec_ctrl_ops,
					msm_vdec_ctrls[idx].id,
					msm_vdec_ctrls[idx].maximum,
					msm_vdec_ctrls[idx].menu_skip_mask,
					msm_vdec_ctrls[idx].default_value);
			} else {
				v4l2_ctrl_new_std(&inst->ctrl_handler,
					&msm_vdec_ctrl_ops,
					msm_vdec_ctrls[idx].id,
					msm_vdec_ctrls[idx].minimum,
					msm_vdec_ctrls[idx].maximum,
					msm_vdec_ctrls[idx].step,
					msm_vdec_ctrls[idx].default_value);
			}
		}
	}
	ret_val = inst->ctrl_handler.error;
	if (ret_val)
		pr_err("CTRL ERR: Error adding ctrls to ctrl handle, %d\n",
				inst->ctrl_handler.error);
	return ret_val;
}
