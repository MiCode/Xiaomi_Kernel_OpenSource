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

#include <linux/dma-direction.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "msm_vdec.h"
#include "msm_venc.h"
#include "msm_vidc_common.h"
#include <linux/delay.h>
#include "vidc_hfi_api.h"
#include "msm_vidc_clocks.h"
#include <linux/dma-buf.h>

#define MAX_EVENTS 30

static int try_get_ctrl(struct msm_vidc_inst *inst,
	struct v4l2_ctrl *ctrl);
static int msm_vidc_get_count(struct msm_vidc_inst *inst,
	struct v4l2_ctrl *ctrl);

static int get_poll_flags(void *instance)
{
	struct msm_vidc_inst *inst = instance;
	struct vb2_queue *outq = &inst->bufq[OUTPUT_PORT].vb2_bufq;
	struct vb2_queue *capq = &inst->bufq[CAPTURE_PORT].vb2_bufq;
	struct vb2_buffer *out_vb = NULL;
	struct vb2_buffer *cap_vb = NULL;
	unsigned long flags;
	int rc = 0;

	if (v4l2_event_pending(&inst->event_handler))
		rc |= POLLPRI;

	spin_lock_irqsave(&capq->done_lock, flags);
	if (!list_empty(&capq->done_list))
		cap_vb = list_first_entry(&capq->done_list, struct vb2_buffer,
								done_entry);
	if (cap_vb && (cap_vb->state == VB2_BUF_STATE_DONE
				|| cap_vb->state == VB2_BUF_STATE_ERROR))
		rc |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&capq->done_lock, flags);

	spin_lock_irqsave(&outq->done_lock, flags);
	if (!list_empty(&outq->done_list))
		out_vb = list_first_entry(&outq->done_list, struct vb2_buffer,
								done_entry);
	if (out_vb && (out_vb->state == VB2_BUF_STATE_DONE
				|| out_vb->state == VB2_BUF_STATE_ERROR))
		rc |= POLLOUT | POLLWRNORM;
	spin_unlock_irqrestore(&outq->done_lock, flags);

	return rc;
}

int msm_vidc_poll(void *instance, struct file *filp,
		struct poll_table_struct *wait)
{
	struct msm_vidc_inst *inst = instance;
	struct vb2_queue *outq = NULL;
	struct vb2_queue *capq = NULL;

	if (!inst)
		return -EINVAL;

	outq = &inst->bufq[OUTPUT_PORT].vb2_bufq;
	capq = &inst->bufq[CAPTURE_PORT].vb2_bufq;

	poll_wait(filp, &inst->event_handler.wait, wait);
	poll_wait(filp, &capq->done_wq, wait);
	poll_wait(filp, &outq->done_wq, wait);
	return get_poll_flags(inst);
}
EXPORT_SYMBOL(msm_vidc_poll);

int msm_vidc_querycap(void *instance, struct v4l2_capability *cap)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !cap)
		return -EINVAL;

	strlcpy(cap->driver, MSM_VIDC_DRV_NAME, sizeof(cap->driver));
	cap->bus_info[0] = 0;
	cap->version = MSM_VIDC_VERSION;
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE_MPLANE |
		V4L2_CAP_VIDEO_OUTPUT_MPLANE |
		V4L2_CAP_STREAMING;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;

	memset(cap->reserved, 0, sizeof(cap->reserved));

	if (inst->session_type == MSM_VIDC_DECODER)
		strlcpy(cap->card, MSM_VDEC_DVC_NAME, sizeof(cap->card));
	else if (inst->session_type == MSM_VIDC_ENCODER)
		strlcpy(cap->card, MSM_VENC_DVC_NAME, sizeof(cap->card));
	else
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(msm_vidc_querycap);

int msm_vidc_enum_fmt(void *instance, struct v4l2_fmtdesc *f)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_enum_fmt(instance, f);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_enum_fmt(instance, f);
	return -EINVAL;
}
EXPORT_SYMBOL(msm_vidc_enum_fmt);

static void msm_vidc_ctrl_get_range(struct v4l2_queryctrl *ctrl,
	struct hal_capability_supported *capability)

{
	ctrl->maximum = capability->max;
	ctrl->minimum = capability->min;
}

int msm_vidc_query_ctrl(void *instance, struct v4l2_queryctrl *ctrl)
{
	struct msm_vidc_inst *inst = instance;
	struct hal_profile_level_supported *prof_level_supported;
	int rc = 0, i = 0, profile_mask = 0, v4l2_prof_value = 0, max_level = 0;

	if (!inst || !ctrl)
		return -EINVAL;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDC_VIDEO_HYBRID_HIERP_MODE:
		msm_vidc_ctrl_get_range(ctrl,
			&inst->capability.hier_p_hybrid);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HIER_B_NUM_LAYERS:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.hier_b);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HIER_P_NUM_LAYERS:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.hier_p);
		break;
	case V4L2_CID_MPEG_VIDC_VENC_PARAM_LAYER_BITRATE:
	case  V4L2_CID_MPEG_VIDEO_BITRATE:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.bitrate);
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE_PEAK:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.peakbitrate);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_WIDTH:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.blur_width);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_BLUR_HEIGHT:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.blur_height);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_NUM_B_FRAMES:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.bframe);
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.slice_mbs);
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.slice_bytes);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE:
	case V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE:
		msm_vidc_ctrl_get_range(ctrl, &inst->capability.frame_rate);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP9_PROFILE:
	{
		prof_level_supported = &inst->capability.profile_level;
		for (i = 0; i < prof_level_supported->profile_count; i++) {
			v4l2_prof_value = msm_comm_hal_to_v4l2(ctrl->id,
				prof_level_supported->profile_level[i].profile);
			if (v4l2_prof_value == -EINVAL) {
				dprintk(VIDC_WARN, "Invalid profile");
				rc = -EINVAL;
			}
			profile_mask |= (1 << v4l2_prof_value);
		}
		ctrl->flags = profile_mask;
		break;
	}
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL:
	case V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL:
	{
		prof_level_supported = &inst->capability.profile_level;
		for (i = 0; i < prof_level_supported->profile_count; i++) {
			if (max_level < prof_level_supported->
				profile_level[i].level) {
				max_level = prof_level_supported->
					profile_level[i].level;
			}
		}
		ctrl->maximum = msm_comm_hal_to_v4l2(ctrl->id, max_level);
		if (ctrl->maximum == -EINVAL) {
			dprintk(VIDC_WARN, "Invalid max level");
			rc = -EINVAL;
		}
		break;
	}
	default:
		rc = -EINVAL;
	}
	return rc;
}
EXPORT_SYMBOL(msm_vidc_query_ctrl);

int msm_vidc_s_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_s_fmt(instance, f);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_s_fmt(instance, f);
	return -EINVAL;
}
EXPORT_SYMBOL(msm_vidc_s_fmt);

int msm_vidc_g_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_vidc_inst *inst = instance;
	int i, rc = 0, color_format = 0;
	enum vidc_ports port;
	u32 num_planes;

	if (!inst || !f) {
		dprintk(VIDC_ERR,
			"Invalid input, inst = %pK, format = %pK\n", inst, f);
		return -EINVAL;
	}
	if (inst->in_reconfig) {
		inst->prop.height[OUTPUT_PORT] = inst->reconfig_height;
		inst->prop.width[OUTPUT_PORT] = inst->reconfig_width;
	}

	port = f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
		OUTPUT_PORT : CAPTURE_PORT;

	f->fmt.pix_mp.pixelformat = inst->fmts[port].fourcc;
	f->fmt.pix_mp.height = inst->prop.height[port];
	f->fmt.pix_mp.width = inst->prop.width[port];
	num_planes = f->fmt.pix_mp.num_planes = inst->bufq[port].num_planes;
	for (i = 0; i < num_planes; ++i)
		f->fmt.pix_mp.plane_fmt[i].sizeimage =
			inst->bufq[port].plane_sizes[i];
	switch (inst->fmts[port].fourcc) {
	case V4L2_PIX_FMT_NV12:
		color_format = COLOR_FMT_NV12;
		break;
	case V4L2_PIX_FMT_NV12_UBWC:
		color_format = COLOR_FMT_NV12_UBWC;
		break;
	case V4L2_PIX_FMT_NV12_TP10_UBWC:
		color_format = COLOR_FMT_NV12_BPP10_UBWC;
		break;
	case V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS:
		color_format = COLOR_FMT_P010;
		break;
	default:
		dprintk(VIDC_DBG,
			"Invalid : g_fmt called on %s port with Invalid fourcc 0x%x\n",
			port == OUTPUT_PORT ? "OUTPUT" : "CAPTURE",
			inst->fmts[port].fourcc);
		goto exit;
	}

	f->fmt.pix_mp.plane_fmt[0].bytesperline = VENUS_Y_STRIDE(color_format,
			inst->prop.width[port]);
	f->fmt.pix_mp.plane_fmt[0].reserved[0] = VENUS_Y_SCANLINES(color_format,
			inst->prop.height[port]);

exit:
	return rc;
}
EXPORT_SYMBOL(msm_vidc_g_fmt);

int msm_vidc_s_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	return msm_comm_s_ctrl(instance, control);
}
EXPORT_SYMBOL(msm_vidc_s_ctrl);

int msm_vidc_g_crop(void *instance, struct v4l2_crop *crop)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !crop)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_ENCODER) {
		dprintk(VIDC_ERR,
			"Session = %pK : Encoder Crop is not implemented yet\n",
				inst);
		return -EPERM;
	}

	crop->c.left = inst->prop.crop_info.left;
	crop->c.top = inst->prop.crop_info.top;
	crop->c.width = inst->prop.crop_info.width;
	crop->c.height = inst->prop.crop_info.height;

	return 0;
}
EXPORT_SYMBOL(msm_vidc_g_crop);

int msm_vidc_g_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_vidc_inst *inst = instance;
	struct v4l2_ctrl *ctrl = NULL;
	int rc = 0;

	if (!inst || !control)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(&inst->ctrl_handler, control->id);
	if (ctrl) {
		rc = try_get_ctrl(inst, ctrl);
		if (!rc)
			control->value = ctrl->val;
	}

	return rc;
}
EXPORT_SYMBOL(msm_vidc_g_ctrl);

int msm_vidc_g_ext_ctrl(void *instance, struct v4l2_ext_controls *control)
{
	struct msm_vidc_inst *inst = instance;
	struct v4l2_ext_control *ext_control;
	struct v4l2_ctrl ctrl;
	int i = 0, rc = 0;

	if (!inst || !control)
		return -EINVAL;

	ext_control = control->controls;

	for (i = 0; i < control->count; i++) {
		switch (ext_control[i].id) {
		case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
			ctrl.id = ext_control[i].id;
			ctrl.val = ext_control[i].value;

			msm_vidc_get_count(inst, &ctrl);
			ext_control->value = ctrl.val;
			break;
		default:
			dprintk(VIDC_ERR,
				"This control %x is not supported yet\n",
					ext_control[i].id);
			rc = -EINVAL;
			break;
		}
	}
	return rc;
}
EXPORT_SYMBOL(msm_vidc_g_ext_ctrl);

int msm_vidc_s_ext_ctrl(void *instance, struct v4l2_ext_controls *control)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_s_ext_ctrl(instance, control);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_s_ext_ctrl(instance, control);
	return -EINVAL;
}
EXPORT_SYMBOL(msm_vidc_s_ext_ctrl);

int msm_vidc_reqbufs(void *instance, struct v4l2_requestbuffers *b)
{
	struct msm_vidc_inst *inst = instance;
	struct buf_queue *q = NULL;
	int rc = 0;

	if (!inst || !b)
		return -EINVAL;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n",
				b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_reqbufs(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);

	if (rc)
		dprintk(VIDC_ERR, "Failed to get reqbufs, %d\n", rc);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_reqbufs);

static bool valid_v4l2_buffer(struct v4l2_buffer *b,
		struct msm_vidc_inst *inst) {
	enum vidc_ports port =
		!V4L2_TYPE_IS_MULTIPLANAR(b->type) ? MAX_PORT_NUM :
		b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? CAPTURE_PORT :
		b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? OUTPUT_PORT :
								MAX_PORT_NUM;

	return port != MAX_PORT_NUM &&
		inst->bufq[port].num_planes == b->length;
}

int msm_vidc_release_buffer(void *instance, int type, unsigned int index)
{
	int rc = 0;
	struct msm_vidc_inst *inst = instance;
	struct msm_vidc_buffer *mbuf, *dummy;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid inst\n", __func__);
		return -EINVAL;
	}

	if (!inst->in_reconfig &&
		inst->state > MSM_VIDC_LOAD_RESOURCES &&
		inst->state < MSM_VIDC_RELEASE_RESOURCES_DONE) {
		rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
		if (rc) {
			dprintk(VIDC_ERR,
					"%s: Failed to move inst: %pK to release res done\n",
					__func__, inst);
		}
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(mbuf, dummy, &inst->registeredbufs.list,
			list) {
		struct vb2_buffer *vb2 = &mbuf->vvb.vb2_buf;

		if (vb2->type != type || vb2->index != index)
			continue;

		if (mbuf->flags & MSM_VIDC_FLAG_RBR_PENDING) {
			print_vidc_buffer(VIDC_DBG,
				"skip rel buf (rbr pending)", inst, mbuf);
			continue;
		}

		print_vidc_buffer(VIDC_DBG, "release buf", inst, mbuf);
		msm_comm_unmap_vidc_buffer(inst, mbuf);
		list_del(&mbuf->list);
		kref_put_mbuf(mbuf);
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return rc;
}
EXPORT_SYMBOL(msm_vidc_release_buffer);

int msm_vidc_qbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0, i = 0;
	struct buf_queue *q = NULL;
	u32 cr = 0;

	if (!inst || !inst->core || !b || !valid_v4l2_buffer(b, inst)) {
		dprintk(VIDC_ERR, "%s: invalid params, inst %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	for (i = 0; i < b->length; i++) {
		b->m.planes[i].m.fd = b->m.planes[i].reserved[0];
		b->m.planes[i].data_offset = b->m.planes[i].reserved[1];
	}

	msm_comm_qbuf_cache_operations(inst, b);

	/* Compression ratio is valid only for Encoder YUV buffers. */
	if (inst->session_type == MSM_VIDC_ENCODER &&
			b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		cr = b->m.planes[0].reserved[2];
		msm_comm_update_input_cr(inst, b->index, cr);
	}

	if (inst->session_type == MSM_VIDC_DECODER &&
			b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		msm_comm_store_mark_data(&inst->etb_data, b->index,
			b->m.planes[0].reserved[3], b->m.planes[0].reserved[4]);
	}

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
EXPORT_SYMBOL(msm_vidc_qbuf);

int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0, i = 0;
	struct buf_queue *q = NULL;

	if (!inst || !b || !valid_v4l2_buffer(b, inst)) {
		dprintk(VIDC_ERR, "%s: invalid params, inst %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_dqbuf(&q->vb2_bufq, b, true);
	mutex_unlock(&q->lock);
	if (rc == -EAGAIN) {
		return rc;
	} else if (rc) {
		dprintk(VIDC_ERR, "Failed to dqbuf, %d\n", rc);
		return rc;
	}

	msm_comm_dqbuf_cache_operations(inst, b);
	for (i = 0; i < b->length; i++) {
		b->m.planes[i].reserved[0] = b->m.planes[i].m.fd;
		b->m.planes[i].reserved[1] = b->m.planes[i].data_offset;
	}

	if (inst->session_type == MSM_VIDC_DECODER &&
			b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		msm_comm_fetch_mark_data(&inst->fbd_data, b->index,
			&b->m.planes[0].reserved[3],
			&b->m.planes[0].reserved[4]);
	}

	return rc;
}
EXPORT_SYMBOL(msm_vidc_dqbuf);

int msm_vidc_streamon(void *instance, enum v4l2_buf_type i)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0;
	struct buf_queue *q;

	if (!inst)
		return -EINVAL;

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
	if (rc) {
		dprintk(VIDC_ERR, "streamon failed on port: %d\n", i);
		msm_comm_kill_session(inst);
	}
	return rc;
}
EXPORT_SYMBOL(msm_vidc_streamon);

int msm_vidc_streamoff(void *instance, enum v4l2_buf_type i)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0;
	struct buf_queue *q;

	if (!inst)
		return -EINVAL;

	q = msm_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}

	if (!inst->in_reconfig) {
		dprintk(VIDC_DBG, "%s: inst %pK release resources\n",
			__func__, inst);
		rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
		if (rc)
			dprintk(VIDC_ERR,
				"%s: inst %pK move to rel res done failed\n",
				__func__, inst);
	}

	dprintk(VIDC_DBG, "Calling streamoff\n");
	mutex_lock(&q->lock);
	rc = vb2_streamoff(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(VIDC_ERR, "streamoff failed on port: %d\n", i);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_streamoff);

int msm_vidc_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize)
{
	struct msm_vidc_inst *inst = instance;
	struct msm_vidc_capability *capability = NULL;

	if (!inst || !fsize) {
		dprintk(VIDC_ERR, "%s: invalid parameter: %pK %pK\n",
				__func__, inst, fsize);
		return -EINVAL;
	}
	if (!inst->core)
		return -EINVAL;

	capability = &inst->capability;
	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = capability->width.min;
	fsize->stepwise.max_width = capability->width.max;
	fsize->stepwise.step_width = capability->width.step_size;
	fsize->stepwise.min_height = capability->height.min;
	fsize->stepwise.max_height = capability->height.max;
	fsize->stepwise.step_height = capability->height.step_size;
	return 0;
}
EXPORT_SYMBOL(msm_vidc_enum_framesizes);

static void *vidc_get_userptr(struct device *dev, unsigned long vaddr,
			unsigned long size, enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

static void vidc_put_userptr(void *buf_priv)
{
}

static const struct vb2_mem_ops msm_vidc_vb2_mem_ops = {
	.get_userptr = vidc_get_userptr,
	.put_userptr = vidc_put_userptr,
};

static void msm_vidc_cleanup_buffer(struct vb2_buffer *vb)
{
	int rc = 0;
	struct buf_queue *q = NULL;
	struct msm_vidc_inst *inst = NULL;

	if (!vb) {
		dprintk(VIDC_ERR, "%s : Invalid vb pointer %pK",
			__func__, vb);
		return;
	}

	inst = vb2_get_drv_priv(vb->vb2_queue);
	if (!inst) {
		dprintk(VIDC_ERR, "%s : Invalid inst pointer",
			__func__);
		return;
	}

	q = msm_comm_get_vb2q(inst, vb->type);
	if (!q) {
		dprintk(VIDC_ERR,
			"%s : Failed to find buffer queue for type = %d\n",
			__func__, vb->type);
		return;
	}

	if (q->vb2_bufq.streaming) {
		dprintk(VIDC_DBG, "%d PORT is streaming\n",
			vb->type);
		return;
	}

	rc = msm_vidc_release_buffer(inst, vb->type, vb->index);
	if (rc)
		dprintk(VIDC_ERR, "%s : Failed to release buffers : %d\n",
			__func__, rc);
}

static int set_buffer_count(struct msm_vidc_inst *inst,
	int host_count, int act_count, enum hal_buffer type)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_buffer_count_actual buf_count;

	hdev = inst->core->device;

	buf_count.buffer_type = type;
	buf_count.buffer_count_actual = act_count;
	buf_count.buffer_count_min_host = host_count;
	dprintk(VIDC_DBG, "%s : Act count = %d Host count = %d\n",
		__func__, act_count, host_count);
	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HAL_PARAM_BUFFER_COUNT_ACTUAL, &buf_count);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to set actual buffer count %d for buffer type %d\n",
			act_count, type);
	return rc;
}

static int msm_vidc_queue_setup(struct vb2_queue *q,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[])
{
	struct msm_vidc_inst *inst;
	int i, rc = 0;
	struct hal_buffer_requirements *bufreq;
	enum hal_buffer buffer_type;

	if (!q || !num_buffers || !num_planes
		|| !sizes || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK, %pK, %pK\n",
			q, num_buffers, num_planes);
		return -EINVAL;
	}
	inst = q->drv_priv;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE: {
		bufreq = get_buff_req_buffer(inst,
			HAL_BUFFER_INPUT);
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"Failed : No buffer requirements : %x\n",
				HAL_BUFFER_INPUT);
			return -EINVAL;
		}
		if (*num_buffers < bufreq->buffer_count_min_host) {
			dprintk(VIDC_DBG,
				"Client passed num buffers %d less than the min_host count %d\n",
				*num_buffers, bufreq->buffer_count_min_host);
		}
		*num_planes = inst->bufq[OUTPUT_PORT].num_planes;
		if (*num_buffers < MIN_NUM_OUTPUT_BUFFERS ||
			*num_buffers > MAX_NUM_OUTPUT_BUFFERS)
			bufreq->buffer_count_actual = *num_buffers =
				MIN_NUM_OUTPUT_BUFFERS;
		for (i = 0; i < *num_planes; i++)
			sizes[i] = inst->bufq[OUTPUT_PORT].plane_sizes[i];

		bufreq->buffer_count_actual = *num_buffers;
		rc = set_buffer_count(inst, bufreq->buffer_count_min_host,
			bufreq->buffer_count_actual, HAL_BUFFER_INPUT);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE: {
		buffer_type = msm_comm_get_hal_output_buffer(inst);
		bufreq = get_buff_req_buffer(inst,
			buffer_type);
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"Failed : No buffer requirements : %x\n",
				buffer_type);
			return -EINVAL;
		}
		if (inst->session_type != MSM_VIDC_DECODER &&
			inst->state > MSM_VIDC_LOAD_RESOURCES_DONE) {
			if (*num_buffers < bufreq->buffer_count_min_host) {
				dprintk(VIDC_DBG,
					"Client passed num buffers %d less than the min_host count %d\n",
						*num_buffers,
						bufreq->buffer_count_min_host);
			}
		}
		*num_planes = inst->bufq[CAPTURE_PORT].num_planes;
		if (*num_buffers < MIN_NUM_CAPTURE_BUFFERS ||
			*num_buffers > MAX_NUM_CAPTURE_BUFFERS)
			bufreq->buffer_count_actual = *num_buffers =
				MIN_NUM_CAPTURE_BUFFERS;

		for (i = 0; i < *num_planes; i++)
			sizes[i] = inst->bufq[CAPTURE_PORT].plane_sizes[i];

		bufreq->buffer_count_actual = *num_buffers;
		rc = set_buffer_count(inst, bufreq->buffer_count_min_host,
			bufreq->buffer_count_actual, buffer_type);
		}
		break;
	default:
		dprintk(VIDC_ERR, "Invalid q type = %d\n", q->type);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static inline int msm_vidc_verify_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc = 0, i = 0;

	/* For decoder No need to sanity till LOAD_RESOURCES */
	if (inst->session_type == MSM_VIDC_DECODER &&
			(inst->state < MSM_VIDC_LOAD_RESOURCES_DONE ||
			inst->state >= MSM_VIDC_RELEASE_RESOURCES_DONE)) {
		dprintk(VIDC_DBG,
			"No need to verify buffer counts : %pK\n", inst);
		return 0;
	}

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		struct hal_buffer_requirements *req = &inst->buff_req.buffer[i];

		if (req && (msm_comm_get_hal_output_buffer(inst) ==
				req->buffer_type)) {
			dprintk(VIDC_DBG, "Verifying Buffer : %d\n",
				req->buffer_type);
			if (req->buffer_count_actual <
					req->buffer_count_min_host ||
				req->buffer_count_min_host <
					req->buffer_count_min) {

				dprintk(VIDC_ERR,
					"Invalid data : Counts mismatch\n");
				dprintk(VIDC_ERR,
					"Min Count = %d ",
						req->buffer_count_min);
				dprintk(VIDC_ERR,
					"Min Host Count = %d ",
						req->buffer_count_min_host);
				dprintk(VIDC_ERR,
					"Min Actual Count = %d\n",
						req->buffer_count_actual);
				rc = -EINVAL;
				break;
			}
		}
	}
	return rc;
}

static inline int start_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_buffer_size_minimum b;

	hdev = inst->core->device;

	/* Check if current session is under HW capability */
	rc = msm_vidc_check_session_supported(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"This session is not supported %pK\n", inst);
		goto fail_start;
	}

	rc = msm_vidc_check_scaling_supported(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"This session scaling is not supported %pK\n", inst);
		goto fail_start;
	}

	/* Decide work mode for current session */
	rc = msm_vidc_decide_work_mode(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to decide work mode for session %pK\n", inst);
		goto fail_start;
	}

	/* Assign Core and LP mode for current session */
	rc = msm_vidc_decide_core_and_power_mode(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"This session can't be submitted to HW %pK\n", inst);
		goto fail_start;
	}

	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		b.buffer_type = HAL_BUFFER_OUTPUT2;
	} else {
		b.buffer_type = HAL_BUFFER_OUTPUT;
	}

	rc = msm_comm_try_get_bufreqs(inst);

	b.buffer_size = inst->bufq[CAPTURE_PORT].plane_sizes[0];
	rc = call_hfi_op(hdev, session_set_property,
			inst->session, HAL_PARAM_BUFFER_SIZE_MINIMUM,
			&b);

	/* Verify if buffer counts are correct */
	rc = msm_vidc_verify_buffer_counts(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"This session has mis-match buffer counts%pK\n", inst);
		goto fail_start;
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

	rc = msm_comm_set_recon_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to set recon buffers: %d\n", rc);
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

	/*
	 * For seq_changed_insufficient, driver should set session_continue
	 * to firmware after the following sequence
	 * - driver raises insufficient event to v4l2 client
	 * - all output buffers have been flushed and freed
	 * - v4l2 client queries buffer requirements and splits/combines OPB-DPB
	 * - v4l2 client sets new set of buffers to firmware
	 * - v4l2 client issues CONTINUE to firmware to resume decoding of
	 *   submitted ETBs.
	 */
	rc = msm_comm_session_continue(inst);
	if (rc)
		goto fail_start;

	msm_comm_scale_clocks_and_bus(inst);

	rc = msm_comm_try_state(inst, MSM_VIDC_START_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to start done state\n", inst);
		goto fail_start;
	}

	msm_clock_data_reset(inst);

	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_queue_output_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to queue output buffers: %d\n", rc);
			goto fail_start;
		}
	}

fail_start:
	if (rc)
		dprintk(VIDC_ERR, "%s: inst %pK session %x failed to start\n",
			__func__, inst, hash32_ptr(inst->session));
	return rc;
}

static int msm_vidc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	struct hfi_device *hdev;

	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
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

	rc = msm_vidc_send_pending_eos_buffers(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed : Send pending EOS buffs for Inst = %pK, %d\n",
				inst, rc);
		goto stream_start_failed;
	}

stream_start_failed:
	if (rc) {
		struct msm_vidc_buffer *temp, *next;
		struct vb2_buffer *vb;

		mutex_lock(&inst->registeredbufs.lock);
		list_for_each_entry_safe(temp, next, &inst->registeredbufs.list,
					list) {
			if (temp->vvb.vb2_buf.type != q->type)
				continue;
			/*
			 * queued_list lock is already acquired before
			 * vb2_stream so no need to acquire it again.
			 */
			list_for_each_entry(vb, &q->queued_list, queued_entry) {
				if (msm_comm_compare_vb2_planes(inst, temp,
						vb)) {
					print_vb2_buffer(VIDC_ERR, "return vb",
						inst, vb);
					vb2_buffer_done(vb,
						VB2_BUF_STATE_QUEUED);
					break;
				}
			}
			msm_comm_unmap_vidc_buffer(inst, temp);
			list_del(&temp->list);
			kref_put_mbuf(temp);
		}
		mutex_unlock(&inst->registeredbufs.lock);
	}
	return rc;
}

static inline int stop_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;

	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst: %pK to state %d\n",
				inst, MSM_VIDC_RELEASE_RESOURCES_DONE);

	msm_clock_data_reset(inst);

	return rc;
}

static void msm_vidc_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;

	if (!q || !q->drv_priv) {
		dprintk(VIDC_ERR, "Invalid input, q = %pK\n", q);
		return;
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

	msm_comm_scale_clocks_and_bus(inst);

	if (rc)
		dprintk(VIDC_ERR,
			"Failed STOP Streaming inst = %pK on cap = %d\n",
			inst, q->type);
}

static void msm_vidc_buf_queue(struct vb2_buffer *vb2)
{
	int rc = 0;
	struct msm_vidc_inst *inst = NULL;
	struct msm_vidc_buffer *mbuf = NULL;

	inst = vb2_get_drv_priv(vb2->vb2_queue);
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid inst\n", __func__);
		return;
	}

	mbuf = msm_comm_get_vidc_buffer(inst, vb2);
	if (IS_ERR_OR_NULL(mbuf)) {
		if (PTR_ERR(mbuf) == -EEXIST)
			return;
		print_vb2_buffer(VIDC_ERR, "failed to get vidc-buf",
			inst, vb2);
		rc = -EINVAL;
		goto error;
	}
	if (!kref_get_mbuf(inst, mbuf)) {
		dprintk(VIDC_ERR, "%s: mbuf not found\n", __func__);
		rc = -EINVAL;
		goto error;
	}

	rc = msm_comm_qbuf(inst, mbuf);
	if (rc)
		print_vidc_buffer(VIDC_ERR, "failed qbuf", inst, mbuf);

	kref_put_mbuf(mbuf);

error:
	if (rc)
		msm_comm_generate_session_error(inst);
}

static const struct vb2_ops msm_vidc_vb2q_ops = {
	.queue_setup = msm_vidc_queue_setup,
	.start_streaming = msm_vidc_start_streaming,
	.buf_queue = msm_vidc_buf_queue,
	.buf_cleanup = msm_vidc_cleanup_buffer,
	.stop_streaming = msm_vidc_stop_streaming,
};

static inline int vb2_bufq_init(struct msm_vidc_inst *inst,
		enum v4l2_buf_type type, enum session_type sess)
{
	struct vb2_queue *q = NULL;

	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		q = &inst->bufq[CAPTURE_PORT].vb2_bufq;
	} else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q = &inst->bufq[OUTPUT_PORT].vb2_bufq;
	} else {
		dprintk(VIDC_ERR, "buf_type = %d not recognised\n", type);
		return -EINVAL;
	}

	q->type = type;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	q->ops = &msm_vidc_vb2q_ops;

	q->mem_ops = &msm_vidc_vb2_mem_ops;
	q->drv_priv = inst;
	q->allow_zero_bytesused = !V4L2_TYPE_IS_OUTPUT(type);
	q->copy_timestamp = 1;
	return vb2_queue_init(q);
}

static int setup_event_queue(void *inst,
				struct video_device *pvdev)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	v4l2_fh_init(&vidc_inst->event_handler, pvdev);
	v4l2_fh_add(&vidc_inst->event_handler);

	return rc;
}

int msm_vidc_subscribe_event(void *inst,
	const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_subscribe(&vidc_inst->event_handler,
		sub, MAX_EVENTS, NULL);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_subscribe_event);

int msm_vidc_unsubscribe_event(void *inst,
	const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_unsubscribe(&vidc_inst->event_handler, sub);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_unsubscribe_event);

int msm_vidc_dqevent(void *inst, struct v4l2_event *event)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	if (!inst || !event)
		return -EINVAL;

	rc = v4l2_event_dequeue(&vidc_inst->event_handler, event, false);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_dqevent);

static bool msm_vidc_check_for_inst_overload(struct msm_vidc_core *core)
{
	u32 instance_count = 0;
	u32 secure_instance_count = 0;
	struct msm_vidc_inst *inst = NULL;
	bool overload = false;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		instance_count++;
		/* This flag is not updated yet for the current instance */
		if (inst->flags & VIDC_SECURE)
			secure_instance_count++;
	}
	mutex_unlock(&core->lock);

	/* Instance count includes current instance as well. */

	if ((instance_count > core->resources.max_inst_count) ||
		(secure_instance_count > core->resources.max_secure_inst_count))
		overload = true;
	return overload;
}

static int msm_vidc_try_set_ctrl(void *instance, struct v4l2_ctrl *ctrl)
{
	struct msm_vidc_inst *inst = instance;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_s_ctrl(instance, ctrl);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_s_ctrl(instance, ctrl);
	return -EINVAL;
}

static int msm_vidc_op_s_ctrl(struct v4l2_ctrl *ctrl)
{

	int rc = 0, c = 0;
	struct msm_vidc_inst *inst;

	if (!ctrl) {
		dprintk(VIDC_ERR, "%s invalid parameters for ctrl\n", __func__);
		return -EINVAL;
	}

	inst = container_of(ctrl->handler,
		struct msm_vidc_inst, ctrl_handler);
	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters for inst\n", __func__);
		return -EINVAL;
	}

	for (c = 0; c < ctrl->ncontrols; ++c) {
		if (ctrl->cluster[c]->is_new) {
			rc = msm_vidc_try_set_ctrl(inst, ctrl->cluster[c]);
			if (rc) {
				dprintk(VIDC_ERR, "Failed setting %x\n",
					ctrl->cluster[c]->id);
				break;
			}
		}
	}
	if (rc)
		dprintk(VIDC_ERR, "Failed setting control: Inst = %pK (%s)\n",
				inst, v4l2_ctrl_get_name(ctrl->id));
	return rc;
}

static int msm_vidc_get_count(struct msm_vidc_inst *inst,
	struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_buffer_requirements *bufreq;
	enum hal_buffer buffer_type;

	if (ctrl->id == V4L2_CID_MIN_BUFFERS_FOR_OUTPUT) {
		bufreq = get_buff_req_buffer(inst, HAL_BUFFER_INPUT);
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"Failed to find bufreqs for buffer type = %d\n",
					HAL_BUFFER_INPUT);
			return 0;
		}
		if (inst->bufq[OUTPUT_PORT].vb2_bufq.streaming) {
			ctrl->val = bufreq->buffer_count_min_host;
			return 0;
		}
		if (ctrl->val > bufreq->buffer_count_min_host) {
			dprintk(VIDC_DBG,
				"Buffer count Host changed from %d to %d\n",
					bufreq->buffer_count_min_host,
					ctrl->val);
			bufreq->buffer_count_actual =
			bufreq->buffer_count_min =
			bufreq->buffer_count_min_host =
				ctrl->val;
		} else {
			ctrl->val = bufreq->buffer_count_min_host;
		}
		rc = set_buffer_count(inst,
			bufreq->buffer_count_min_host,
			bufreq->buffer_count_actual,
			HAL_BUFFER_INPUT);

		msm_vidc_update_host_buff_counts(inst);
		ctrl->val = bufreq->buffer_count_min_host;
		return rc;

	} else if (ctrl->id == V4L2_CID_MIN_BUFFERS_FOR_CAPTURE) {

		buffer_type = msm_comm_get_hal_output_buffer(inst);
		bufreq = get_buff_req_buffer(inst,
			buffer_type);
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"Failed to find bufreqs for buffer type = %d\n",
					buffer_type);
			return 0;
		}
		if (inst->bufq[CAPTURE_PORT].vb2_bufq.streaming) {
			if (ctrl->val != bufreq->buffer_count_min_host)
				return -EINVAL;
			else
				return 0;
		}

		if (inst->session_type == MSM_VIDC_DECODER &&
				!inst->in_reconfig &&
			inst->state < MSM_VIDC_LOAD_RESOURCES_DONE) {
			dprintk(VIDC_DBG,
				"Clients updates Buffer count from %d to %d\n",
				bufreq->buffer_count_min_host, ctrl->val);
			bufreq->buffer_count_actual =
			bufreq->buffer_count_min =
			bufreq->buffer_count_min_host =
				ctrl->val;
		}
		if (ctrl->val > bufreq->buffer_count_min_host) {
			dprintk(VIDC_DBG,
				"Buffer count Host changed from %d to %d\n",
				bufreq->buffer_count_min_host,
				ctrl->val);
			bufreq->buffer_count_actual =
			bufreq->buffer_count_min =
			bufreq->buffer_count_min_host =
				ctrl->val;
		} else {
			ctrl->val = bufreq->buffer_count_min_host;
		}
		rc = set_buffer_count(inst,
			bufreq->buffer_count_min_host,
			bufreq->buffer_count_actual,
			HAL_BUFFER_OUTPUT);

		msm_vidc_update_host_buff_counts(inst);
		ctrl->val = bufreq->buffer_count_min_host;

		return rc;
	}
	return -EINVAL;
}

static int try_get_ctrl(struct msm_vidc_inst *inst, struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_buffer_requirements *bufreq = NULL;
	enum hal_buffer buffer_type;

	switch (ctrl->id) {

	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		ctrl->val = msm_comm_hal_to_v4l2(
			V4L2_CID_MPEG_VIDEO_H264_PROFILE,
			inst->profile);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE:
		ctrl->val = msm_comm_hal_to_v4l2(
			V4L2_CID_MPEG_VIDC_VIDEO_HEVC_PROFILE,
			inst->profile);
		break;

	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		ctrl->val = msm_comm_hal_to_v4l2(
			V4L2_CID_MPEG_VIDEO_H264_LEVEL,
			inst->level);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		ctrl->val = msm_comm_hal_to_v4l2(
			V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
			inst->level);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL:
		ctrl->val = msm_comm_hal_to_v4l2(
			V4L2_CID_MPEG_VIDC_VIDEO_HEVC_TIER_LEVEL,
			inst->level);
		break;

	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		ctrl->val = inst->entropy_mode;
		break;

	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (inst->in_reconfig)
			msm_vidc_update_host_buff_counts(inst);
		buffer_type = msm_comm_get_hal_output_buffer(inst);
		bufreq = get_buff_req_buffer(inst,
			buffer_type);
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"Failed to find bufreqs for buffer type = %d\n",
					buffer_type);
			return -EINVAL;
		}
		ctrl->val = bufreq->buffer_count_min_host;
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
		bufreq = get_buff_req_buffer(inst, HAL_BUFFER_INPUT);
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"Failed to find bufreqs for buffer type = %d\n",
					HAL_BUFFER_INPUT);
			return -EINVAL;
		}

		if (inst->session_type == MSM_VIDC_DECODER &&
			!(inst->flags & VIDC_THUMBNAIL) &&
			inst->fmts[OUTPUT_PORT].fourcc ==
				V4L2_PIX_FMT_VP9 &&
			bufreq->buffer_count_min_host <
				MIN_NUM_OUTPUT_BUFFERS_VP9)
			bufreq->buffer_count_min_host =
				MIN_NUM_OUTPUT_BUFFERS_VP9;

		ctrl->val = bufreq->buffer_count_min_host;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_TME_PAYLOAD_VERSION:
		ctrl->val = inst->capability.tme_version;
		break;
	default:
		/*
		 * Other controls aren't really volatile, shouldn't need to
		 * modify ctrl->value
		 */
		break;
	}

	return rc;
}

static int msm_vidc_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0, c = 0;
	struct msm_vidc_inst *inst;
	struct v4l2_ctrl *master;

	if (!ctrl) {
		dprintk(VIDC_ERR, "%s invalid parameters for ctrl\n", __func__);
		return -EINVAL;
	}

	inst = container_of(ctrl->handler,
		struct msm_vidc_inst, ctrl_handler);
	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters for inst\n", __func__);
		return -EINVAL;
	}
	master = ctrl->cluster[0];
	if (!master) {
		dprintk(VIDC_ERR, "%s invalid parameters for master\n",
			__func__);
		return -EINVAL;
	}

	for (c = 0; c < master->ncontrols; ++c) {
		if (master->cluster[c]->flags & V4L2_CTRL_FLAG_VOLATILE) {
			rc = try_get_ctrl(inst, master->cluster[c]);
			if (rc) {
				dprintk(VIDC_ERR, "Failed getting %x\n",
					master->cluster[c]->id);
				return rc;
			}
		}
	}
	if (rc)
		dprintk(VIDC_ERR, "Failed getting control: Inst = %pK (%s)\n",
				inst, v4l2_ctrl_get_name(ctrl->id));
	return rc;
}

static const struct v4l2_ctrl_ops msm_vidc_ctrl_ops = {

	.s_ctrl = msm_vidc_op_s_ctrl,
	.g_volatile_ctrl = msm_vidc_op_g_volatile_ctrl,
};

void *msm_vidc_open(int core_id, int session_type)
{
	struct msm_vidc_inst *inst = NULL;
	struct msm_vidc_core *core = NULL;
	int rc = 0;
	int i = 0;

	if (core_id >= MSM_VIDC_CORES_MAX ||
			session_type >= MSM_VIDC_MAX_DEVICES) {
		dprintk(VIDC_ERR, "Invalid input, core_id = %d, session = %d\n",
			core_id, session_type);
		goto err_invalid_core;
	}
	core = get_vidc_core(core_id);
	if (!core) {
		dprintk(VIDC_ERR,
			"Failed to find core for core_id = %d\n", core_id);
		goto err_invalid_core;
	}

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		dprintk(VIDC_ERR, "Failed to allocate memory\n");
		rc = -ENOMEM;
		goto err_invalid_core;
	}

	pr_info(VIDC_DBG_TAG "Opening video instance: %pK, %d\n",
		VIDC_MSG_PRIO2STRING(VIDC_INFO), inst, session_type);
	mutex_init(&inst->sync_lock);
	mutex_init(&inst->bufq[CAPTURE_PORT].lock);
	mutex_init(&inst->bufq[OUTPUT_PORT].lock);
	mutex_init(&inst->lock);
	mutex_init(&inst->flush_lock);

	INIT_MSM_VIDC_LIST(&inst->scratchbufs);
	INIT_MSM_VIDC_LIST(&inst->freqs);
	INIT_MSM_VIDC_LIST(&inst->input_crs);
	INIT_MSM_VIDC_LIST(&inst->persistbufs);
	INIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	INIT_MSM_VIDC_LIST(&inst->outputbufs);
	INIT_MSM_VIDC_LIST(&inst->registeredbufs);
	INIT_MSM_VIDC_LIST(&inst->reconbufs);
	INIT_MSM_VIDC_LIST(&inst->eosbufs);
	INIT_MSM_VIDC_LIST(&inst->etb_data);
	INIT_MSM_VIDC_LIST(&inst->fbd_data);

	kref_init(&inst->kref);

	inst->session_type = session_type;
	inst->state = MSM_VIDC_CORE_UNINIT_DONE;
	inst->core = core;
	inst->clk_data.min_freq = 0;
	inst->clk_data.curr_freq = 0;
	inst->clk_data.bitrate = 0;
	inst->clk_data.core_id = VIDC_CORE_ID_DEFAULT;
	inst->bit_depth = MSM_VIDC_BIT_DEPTH_8;
	inst->pic_struct = MSM_VIDC_PIC_STRUCT_PROGRESSIVE;
	inst->colour_space = MSM_VIDC_BT601_6_525;
	inst->profile = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
	inst->level = V4L2_MPEG_VIDEO_H264_LEVEL_1_0;
	inst->entropy_mode = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC;

	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}
	inst->mem_client = msm_smem_new_client(SMEM_ION,
					&inst->core->resources, session_type);
	if (!inst->mem_client) {
		dprintk(VIDC_ERR, "Failed to create memory client\n");
		goto fail_mem_client;
	}

	if (session_type == MSM_VIDC_DECODER) {
		msm_vdec_inst_init(inst);
		rc = msm_vdec_ctrl_init(inst, &msm_vidc_ctrl_ops);
	} else if (session_type == MSM_VIDC_ENCODER) {
		msm_venc_inst_init(inst);
		rc = msm_venc_ctrl_init(inst, &msm_vidc_ctrl_ops);
	}

	if (rc)
		goto fail_bufq_capture;

	rc = vb2_bufq_init(inst, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			session_type);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to initialize vb2 queue on capture port\n");
		goto fail_bufq_capture;
	}
	rc = vb2_bufq_init(inst, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			session_type);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to initialize vb2 queue on capture port\n");
		goto fail_bufq_output;
	}

	setup_event_queue(inst, &core->vdev[session_type].vdev);

	mutex_lock(&core->lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->lock);

	rc = msm_comm_try_state(inst, MSM_VIDC_CORE_INIT_DONE);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move video instance to init state\n");
		goto fail_init;
	}

	msm_dcvs_try_enable(inst);
	if (msm_vidc_check_for_inst_overload(core)) {
		dprintk(VIDC_ERR,
			"Instance count reached Max limit, rejecting session");
		goto fail_init;
	}

	msm_comm_scale_clocks_and_bus(inst);

	inst->debugfs_root =
		msm_vidc_debugfs_init_inst(inst, core->debugfs_root);

	return inst;
fail_init:
	mutex_lock(&core->lock);
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);
	vb2_queue_release(&inst->bufq[OUTPUT_PORT].vb2_bufq);
fail_bufq_output:
	vb2_queue_release(&inst->bufq[CAPTURE_PORT].vb2_bufq);
fail_bufq_capture:
	msm_comm_ctrl_deinit(inst);
	msm_smem_delete_client(inst->mem_client);
fail_mem_client:
	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->bufq[CAPTURE_PORT].lock);
	mutex_destroy(&inst->bufq[OUTPUT_PORT].lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->flush_lock);

	DEINIT_MSM_VIDC_LIST(&inst->scratchbufs);
	DEINIT_MSM_VIDC_LIST(&inst->persistbufs);
	DEINIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	DEINIT_MSM_VIDC_LIST(&inst->outputbufs);
	DEINIT_MSM_VIDC_LIST(&inst->registeredbufs);
	DEINIT_MSM_VIDC_LIST(&inst->eosbufs);
	DEINIT_MSM_VIDC_LIST(&inst->freqs);
	DEINIT_MSM_VIDC_LIST(&inst->input_crs);
	DEINIT_MSM_VIDC_LIST(&inst->etb_data);
	DEINIT_MSM_VIDC_LIST(&inst->fbd_data);

	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}
EXPORT_SYMBOL(msm_vidc_open);

static void msm_vidc_cleanup_instance(struct msm_vidc_inst *inst)
{
	struct msm_vidc_buffer *temp, *dummy;
	struct getprop_buf *temp_prop, *dummy_prop;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(temp, dummy, &inst->registeredbufs.list,
			list) {
		print_vidc_buffer(VIDC_ERR, "undequeud buf", inst, temp);
		msm_comm_unmap_vidc_buffer(inst, temp);
		list_del(&temp->list);
		kref_put_mbuf(temp);
	}
	mutex_unlock(&inst->registeredbufs.lock);

	msm_comm_free_freq_table(inst);

	msm_comm_free_input_cr_table(inst);

	if (msm_comm_release_scratch_buffers(inst, false))
		dprintk(VIDC_ERR,
			"Failed to release scratch buffers\n");

	if (msm_comm_release_recon_buffers(inst))
		dprintk(VIDC_ERR,
			"Failed to release recon buffers\n");

	if (msm_comm_release_persist_buffers(inst))
		dprintk(VIDC_ERR,
			"Failed to release persist buffers\n");

	if (msm_comm_release_mark_data(inst))
		dprintk(VIDC_ERR,
			"Failed to release mark_data buffers\n");

	msm_comm_release_eos_buffers(inst);

	if (msm_comm_release_output_buffers(inst, true))
		dprintk(VIDC_ERR,
			"Failed to release output buffers\n");

	if (inst->extradata_handle)
		msm_comm_smem_free(inst, inst->extradata_handle);

	mutex_lock(&inst->pending_getpropq.lock);
	if (!list_empty(&inst->pending_getpropq.list)) {
		dprintk(VIDC_ERR,
			"pending_getpropq not empty for instance %pK\n",
			inst);
		list_for_each_entry_safe(temp_prop, dummy_prop,
			&inst->pending_getpropq.list, list) {
			kfree(temp_prop->data);
			list_del(&temp_prop->list);
			kfree(temp_prop);
		}
	}
	mutex_unlock(&inst->pending_getpropq.lock);
}

int msm_vidc_destroy(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	int i = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;

	mutex_lock(&core->lock);
	/* inst->list lives in core->instances */
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	msm_comm_ctrl_deinit(inst);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);

	for (i = 0; i < MAX_PORT_NUM; i++)
		vb2_queue_release(&inst->bufq[i].vb2_bufq);

	DEINIT_MSM_VIDC_LIST(&inst->scratchbufs);
	DEINIT_MSM_VIDC_LIST(&inst->persistbufs);
	DEINIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	DEINIT_MSM_VIDC_LIST(&inst->outputbufs);
	DEINIT_MSM_VIDC_LIST(&inst->registeredbufs);
	DEINIT_MSM_VIDC_LIST(&inst->eosbufs);
	DEINIT_MSM_VIDC_LIST(&inst->freqs);
	DEINIT_MSM_VIDC_LIST(&inst->input_crs);
	DEINIT_MSM_VIDC_LIST(&inst->etb_data);
	DEINIT_MSM_VIDC_LIST(&inst->fbd_data);

	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->bufq[CAPTURE_PORT].lock);
	mutex_destroy(&inst->bufq[OUTPUT_PORT].lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->flush_lock);

	msm_vidc_debugfs_deinit_inst(inst);

	pr_info(VIDC_DBG_TAG "Closed video instance: %pK\n",
			VIDC_MSG_PRIO2STRING(VIDC_INFO), inst);
	kfree(inst);
	return 0;
}

static void close_helper(struct kref *kref)
{
	struct msm_vidc_inst *inst = container_of(kref,
			struct msm_vidc_inst, kref);

	msm_vidc_destroy(inst);
}

int msm_vidc_close(void *instance)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/*
	 * Make sure that HW stop working on these buffers that
	 * we are going to free.
	 */
	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move inst %pK to rel resource done state\n",
			inst);

	msm_vidc_cleanup_instance(inst);

	rc = msm_comm_try_state(inst, MSM_VIDC_CORE_UNINIT);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move inst %pK to uninit state\n", inst);
		rc = msm_comm_force_cleanup(inst);
	}

	msm_comm_session_clean(inst);
	msm_smem_delete_client(inst->mem_client);

	kref_put(&inst->kref, close_helper);
	return 0;
}
EXPORT_SYMBOL(msm_vidc_close);

int msm_vidc_suspend(int core_id)
{
	return msm_comm_suspend(core_id);
}
EXPORT_SYMBOL(msm_vidc_suspend);

