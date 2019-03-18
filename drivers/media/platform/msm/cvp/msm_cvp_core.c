// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-direction.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "msm_cvp_core.h"
#include "msm_cvp_internal.h"
#include "msm_cvp_debug.h"
#include "msm_cvp.h"
#include "msm_cvp_common.h"
#include <linux/delay.h>
#include "cvp_hfi_api.h"
#include "msm_cvp_clocks.h"
#include <linux/dma-buf.h>

#define MAX_EVENTS 30

static int try_get_ctrl(struct msm_cvp_inst *inst,
	struct v4l2_ctrl *ctrl);

static int get_poll_flags(void *instance)
{
	struct msm_cvp_inst *inst = instance;
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

int msm_cvp_poll(void *instance, struct file *filp,
		struct poll_table_struct *wait)
{
	struct msm_cvp_inst *inst = instance;
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
EXPORT_SYMBOL(msm_cvp_poll);

int msm_cvp_querycap(void *instance, struct v4l2_capability *cap)
{
	return -EINVAL;
}
EXPORT_SYMBOL(msm_cvp_querycap);

int msm_cvp_enum_fmt(void *instance, struct v4l2_fmtdesc *f)
{
	struct msm_cvp_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	return -EINVAL;
}
EXPORT_SYMBOL(msm_cvp_enum_fmt);

int msm_cvp_query_ctrl(void *instance, struct v4l2_queryctrl *ctrl)
{
	return -EINVAL;
}
EXPORT_SYMBOL(msm_cvp_query_ctrl);

int msm_cvp_s_fmt(void *instance, struct v4l2_format *f)
{
	int rc = 0;
	struct msm_cvp_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	dprintk(CVP_DBG,
		"s_fmt: %x : type %d wxh %dx%d pixelfmt %#x num_planes %d size[0] %d size[1] %d in_reconfig %d\n",
		hash32_ptr(inst->session), f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes,
		f->fmt.pix_mp.plane_fmt[0].sizeimage,
		f->fmt.pix_mp.plane_fmt[1].sizeimage, inst->in_reconfig);
	return rc;
}
EXPORT_SYMBOL(msm_cvp_s_fmt);

int msm_cvp_g_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_cvp_inst *inst = instance;
	int i, rc = 0, color_format = 0;
	enum cvp_ports port;
	u32 num_planes;

	if (!inst || !f) {
		dprintk(CVP_ERR,
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
	case V4L2_PIX_FMT_NV12_512:
		color_format = COLOR_FMT_NV12_512;
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
		dprintk(CVP_DBG,
			"Invalid : g_fmt called on %s port with Invalid fourcc 0x%x\n",
			port == OUTPUT_PORT ? "OUTPUT" : "CAPTURE",
			inst->fmts[port].fourcc);
		goto exit;
	}

	f->fmt.pix_mp.plane_fmt[0].bytesperline = VENUS_Y_STRIDE(color_format,
			inst->prop.width[port]);
	f->fmt.pix_mp.plane_fmt[0].reserved[0] = VENUS_Y_SCANLINES(color_format,
			inst->prop.height[port]);
	f->fmt.pix_mp.plane_fmt[0].sizeimage = VENUS_BUFFER_SIZE(color_format,
			inst->prop.width[port], inst->prop.height[port]);

	dprintk(CVP_DBG,
		"g_fmt: %x : type %d wxh %dx%d pixelfmt %#x num_planes %d size[0] %d size[1] %d in_reconfig %d\n",
		hash32_ptr(inst->session), f->type,
		f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes,
		f->fmt.pix_mp.plane_fmt[0].sizeimage,
		f->fmt.pix_mp.plane_fmt[1].sizeimage, inst->in_reconfig);
exit:
	return rc;
}
EXPORT_SYMBOL(msm_cvp_g_fmt);

int msm_cvp_s_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_cvp_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	return msm_comm_s_ctrl(instance, control);
}
EXPORT_SYMBOL(msm_cvp_s_ctrl);

int msm_cvp_g_crop(void *instance, struct v4l2_crop *crop)
{
	return -EINVAL;
}
EXPORT_SYMBOL(msm_cvp_g_crop);

int msm_cvp_g_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_cvp_inst *inst = instance;
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
EXPORT_SYMBOL(msm_cvp_g_ctrl);

int msm_cvp_g_ext_ctrl(void *instance, struct v4l2_ext_controls *control)
{
	struct msm_cvp_inst *inst = instance;
	struct v4l2_ext_control *ext_control;
	int i = 0, rc = 0;

	if (!inst || !control)
		return -EINVAL;

	ext_control = control->controls;

	for (i = 0; i < control->count; i++) {
		switch (ext_control[i].id) {
		default:
			dprintk(CVP_ERR,
				"This control %x is not supported yet\n",
					ext_control[i].id);
			break;
		}
	}
	return rc;
}
EXPORT_SYMBOL(msm_cvp_g_ext_ctrl);

int msm_cvp_s_ext_ctrl(void *instance, struct v4l2_ext_controls *control)
{
	struct msm_cvp_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	return -EINVAL;
}
EXPORT_SYMBOL(msm_cvp_s_ext_ctrl);

int msm_cvp_reqbufs(void *instance, struct v4l2_requestbuffers *b)
{
	struct msm_cvp_inst *inst = instance;
	struct buf_queue *q = NULL;
	int rc = 0;

	if (!inst || !b)
		return -EINVAL;
	q = msm_cvp_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(CVP_ERR,
			"Failed to find buffer queue for type = %d\n",
				b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_reqbufs(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);

	if (rc)
		dprintk(CVP_ERR, "Failed to get reqbufs, %d\n", rc);
	return rc;
}
EXPORT_SYMBOL(msm_cvp_reqbufs);

static bool valid_v4l2_buffer(struct v4l2_buffer *b,
		struct msm_cvp_inst *inst)
{
	enum cvp_ports port =
		!V4L2_TYPE_IS_MULTIPLANAR(b->type) ? MAX_PORT_NUM :
		b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ? CAPTURE_PORT :
		b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? OUTPUT_PORT :
								MAX_PORT_NUM;

	return port != MAX_PORT_NUM &&
		inst->bufq[port].num_planes == b->length;
}

int msm_cvp_release_buffer(void *instance, int type, unsigned int index)
{
	int rc = 0;
	struct msm_cvp_inst *inst = instance;
	struct msm_video_buffer *mbuf, *dummy;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid inst\n", __func__);
		return -EINVAL;
	}

	if (!inst->in_reconfig &&
		inst->state > MSM_CVP_LOAD_RESOURCES &&
		inst->state < MSM_CVP_RELEASE_RESOURCES_DONE) {
		rc = msm_cvp_comm_try_state(inst,
			MSM_CVP_RELEASE_RESOURCES_DONE);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: Failed to move inst: %pK to rel res done\n",
					__func__, inst);
		}
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(mbuf, dummy, &inst->registeredbufs.list,
			list) {
		struct vb2_buffer *vb2 = &mbuf->vvb.vb2_buf;

		if (vb2->type != type || vb2->index != index)
			continue;

		if (mbuf->flags & MSM_CVP_FLAG_RBR_PENDING) {
			print_video_buffer(CVP_DBG,
				"skip rel buf (rbr pending)", inst, mbuf);
			continue;
		}

		print_video_buffer(CVP_DBG, "release buf", inst, mbuf);
		msm_cvp_comm_unmap_video_buffer(inst, mbuf);
		list_del(&mbuf->list);
		kref_cvp_put_mbuf(mbuf);
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return rc;
}
EXPORT_SYMBOL(msm_cvp_release_buffer);

int msm_cvp_qbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_cvp_inst *inst = instance;
	int rc = 0, i = 0;
	struct buf_queue *q = NULL;

	if (!inst || !inst->core || !b || !valid_v4l2_buffer(b, inst)) {
		dprintk(CVP_ERR, "%s: invalid params, inst %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	for (i = 0; i < b->length; i++) {
		b->m.planes[i].m.fd = b->m.planes[i].reserved[0];
		b->m.planes[i].data_offset = b->m.planes[i].reserved[1];
	}

	q = msm_cvp_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(CVP_ERR,
			"Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_qbuf(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(CVP_ERR, "Failed to qbuf, %d\n", rc);

	return rc;
}
EXPORT_SYMBOL(msm_cvp_qbuf);

int msm_cvp_dqbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_cvp_inst *inst = instance;
	int rc = 0, i = 0;
	struct buf_queue *q = NULL;

	if (!inst || !b || !valid_v4l2_buffer(b, inst)) {
		dprintk(CVP_ERR, "%s: invalid params, inst %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	q = msm_cvp_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(CVP_ERR,
			"Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_dqbuf(&q->vb2_bufq, b, true);
	mutex_unlock(&q->lock);
	if (rc == -EAGAIN) {
		return rc;
	} else if (rc) {
		dprintk(CVP_ERR, "Failed to dqbuf, %d\n", rc);
		return rc;
	}

	for (i = 0; i < b->length; i++) {
		b->m.planes[i].reserved[0] = b->m.planes[i].m.fd;
		b->m.planes[i].reserved[1] = b->m.planes[i].data_offset;
	}

	return rc;
}
EXPORT_SYMBOL(msm_cvp_dqbuf);

int msm_cvp_streamon(void *instance, enum v4l2_buf_type i)
{
	struct msm_cvp_inst *inst = instance;
	int rc = 0;
	struct buf_queue *q;

	if (!inst)
		return -EINVAL;

	q = msm_cvp_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(CVP_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "Calling streamon\n");
	mutex_lock(&q->lock);
	rc = vb2_streamon(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc) {
		dprintk(CVP_ERR, "streamon failed on port: %d\n", i);
		msm_cvp_comm_kill_session(inst);
	}
	return rc;
}
EXPORT_SYMBOL(msm_cvp_streamon);

int msm_cvp_streamoff(void *instance, enum v4l2_buf_type i)
{
	struct msm_cvp_inst *inst = instance;
	int rc = 0;
	struct buf_queue *q;

	if (!inst)
		return -EINVAL;

	q = msm_cvp_comm_get_vb2q(inst, i);
	if (!q) {
		dprintk(CVP_ERR,
			"Failed to find buffer queue for type = %d\n", i);
		return -EINVAL;
	}

	if (!inst->in_reconfig) {
		dprintk(CVP_DBG, "%s: inst %pK release resources\n",
			__func__, inst);
		rc = msm_cvp_comm_try_state(inst,
			MSM_CVP_RELEASE_RESOURCES_DONE);
		if (rc)
			dprintk(CVP_ERR,
				"%s: inst %pK move to rel res done failed\n",
				__func__, inst);
	}

	dprintk(CVP_DBG, "Calling streamoff\n");
	mutex_lock(&q->lock);
	rc = vb2_streamoff(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		dprintk(CVP_ERR, "streamoff failed on port: %d\n", i);
	return rc;
}
EXPORT_SYMBOL(msm_cvp_streamoff);

int msm_cvp_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize)
{
	struct msm_cvp_inst *inst = instance;
	struct msm_cvp_capability *capability = NULL;

	if (!inst || !fsize) {
		dprintk(CVP_ERR, "%s: invalid parameter: %pK %pK\n",
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
EXPORT_SYMBOL(msm_cvp_enum_framesizes);

static void *cvp_get_userptr(struct device *dev, unsigned long vaddr,
			unsigned long size, enum dma_data_direction dma_dir)
{
	return (void *)0xdeadbeef;
}

static void cvp_put_userptr(void *buf_priv)
{
}

static const struct vb2_mem_ops msm_cvp_vb2_mem_ops = {
	.get_userptr = cvp_get_userptr,
	.put_userptr = cvp_put_userptr,
};

static void msm_cvp_cleanup_buffer(struct vb2_buffer *vb)
{
	int rc = 0;
	struct buf_queue *q = NULL;
	struct msm_cvp_inst *inst = NULL;

	if (!vb) {
		dprintk(CVP_ERR, "%s : Invalid vb pointer %pK",
			__func__, vb);
		return;
	}

	inst = vb2_get_drv_priv(vb->vb2_queue);
	if (!inst) {
		dprintk(CVP_ERR, "%s : Invalid inst pointer",
			__func__);
		return;
	}

	q = msm_cvp_comm_get_vb2q(inst, vb->type);
	if (!q) {
		dprintk(CVP_ERR,
			"%s : Failed to find buffer queue for type = %d\n",
			__func__, vb->type);
		return;
	}

	if (q->vb2_bufq.streaming) {
		dprintk(CVP_DBG, "%d PORT is streaming\n",
			vb->type);
		return;
	}

	rc = msm_cvp_release_buffer(inst, vb->type, vb->index);
	if (rc)
		dprintk(CVP_ERR, "%s : Failed to release buffers : %d\n",
			__func__, rc);
}

static int msm_cvp_queue_setup(struct vb2_queue *q,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[])
{
	struct msm_cvp_inst *inst;
	int i, rc = 0;
	struct hal_buffer_requirements *bufreq;
	enum hal_buffer buffer_type;

	if (!q || !num_buffers || !num_planes
		|| !sizes || !q->drv_priv) {
		dprintk(CVP_ERR, "Invalid input, q = %pK, %pK, %pK\n",
			q, num_buffers, num_planes);
		return -EINVAL;
	}
	inst = q->drv_priv;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	switch (q->type) {
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE: {
		bufreq = get_cvp_buff_req_buffer(inst,
			HAL_BUFFER_INPUT);
		if (!bufreq) {
			dprintk(CVP_ERR,
				"Failed : No buffer requirements : %x\n",
				HAL_BUFFER_INPUT);
			return -EINVAL;
		}
		if (*num_buffers < bufreq->buffer_count_min_host) {
			dprintk(CVP_DBG,
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
		rc = msm_cvp_comm_set_buffer_count(inst,
			bufreq->buffer_count_min,
			bufreq->buffer_count_actual, HAL_BUFFER_INPUT);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE: {
		buffer_type = msm_cvp_comm_get_hal_output_buffer(inst);
		bufreq = get_cvp_buff_req_buffer(inst,
			buffer_type);
		if (!bufreq) {
			dprintk(CVP_ERR,
				"Failed : No buffer requirements : %x\n",
				buffer_type);
			return -EINVAL;
		}
		if (inst->session_type != MSM_CVP_DECODER &&
			inst->state > MSM_CVP_LOAD_RESOURCES_DONE) {
			if (*num_buffers < bufreq->buffer_count_min_host) {
				dprintk(CVP_DBG,
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
		rc = msm_cvp_comm_set_buffer_count(inst,
			bufreq->buffer_count_min,
			bufreq->buffer_count_actual, buffer_type);
		}
		break;
	default:
		dprintk(CVP_ERR, "Invalid q type = %d\n", q->type);
		rc = -EINVAL;
		break;
	}

	dprintk(CVP_DBG,
		"queue_setup: %x : type %d num_buffers %d num_planes %d sizes[0] %d sizes[1] %d\n",
		hash32_ptr(inst->session), q->type, *num_buffers,
		*num_planes, sizes[0], sizes[1]);
	return rc;
}

static int msm_cvp_start_streaming(struct vb2_queue *q, unsigned int count)
{
	dprintk(CVP_ERR, "Invalid input, q = %pK\n", q);
	return -EINVAL;
}

static void msm_cvp_stop_streaming(struct vb2_queue *q)
{
	dprintk(CVP_INFO, "%s: No streaming use case supported\n",
		__func__);
}

static int msm_cvp_queue_buf(struct msm_cvp_inst *inst,
		struct vb2_buffer *vb2)
{
	int rc = 0;
	struct msm_video_buffer *mbuf;

	if (!inst || !vb2) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	mbuf = msm_cvp_comm_get_video_buffer(inst, vb2);
	if (IS_ERR_OR_NULL(mbuf)) {
		/*
		 * if the buffer has RBR_PENDING flag (-EEXIST) then don't queue
		 * it now, it will be queued via msm_cvp_comm_qbuf_rbr() as
		 * part of RBR event processing.
		 */
		if (PTR_ERR(mbuf) == -EEXIST)
			return 0;
		dprintk(CVP_ERR, "%s: failed to get cvp-buf\n", __func__);
		return -EINVAL;
	}
	if (!kref_cvp_get_mbuf(inst, mbuf)) {
		dprintk(CVP_ERR, "%s: mbuf not found\n", __func__);
		return -EINVAL;
	}
	rc = msm_cvp_comm_qbuf(inst, mbuf);
	if (rc)
		dprintk(CVP_ERR, "%s: failed qbuf\n", __func__);
	kref_cvp_put_mbuf(mbuf);

	return rc;
}

static int msm_cvp_queue_buf_batch(struct msm_cvp_inst *inst,
		struct vb2_buffer *vb2)
{
	int rc;

	if (!inst || !vb2) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_queue_buf(inst, vb2);

	return rc;
}

static void msm_cvp_buf_queue(struct vb2_buffer *vb2)
{
	int rc = 0;
	struct msm_cvp_inst *inst = NULL;

	inst = vb2_get_drv_priv(vb2->vb2_queue);
	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid inst\n", __func__);
		return;
	}

	if (inst->batch.enable)
		rc = msm_cvp_queue_buf_batch(inst, vb2);
	else
		rc = msm_cvp_queue_buf(inst, vb2);
	if (rc) {
		print_cvp_vb2_buffer(CVP_ERR, "failed vb2-qbuf", inst, vb2);
		msm_cvp_comm_generate_session_error(inst);
	}
}

static const struct vb2_ops msm_cvp_vb2q_ops = {
	.queue_setup = msm_cvp_queue_setup,
	.start_streaming = msm_cvp_start_streaming,
	.buf_queue = msm_cvp_buf_queue,
	.buf_cleanup = msm_cvp_cleanup_buffer,
	.stop_streaming = msm_cvp_stop_streaming,
};

static inline int vb2_bufq_init(struct msm_cvp_inst *inst,
		enum v4l2_buf_type type, enum session_type sess)
{
	struct vb2_queue *q = NULL;

	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		q = &inst->bufq[CAPTURE_PORT].vb2_bufq;
	} else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q = &inst->bufq[OUTPUT_PORT].vb2_bufq;
	} else {
		dprintk(CVP_ERR, "buf_type = %d not recognised\n", type);
		return -EINVAL;
	}

	q->type = type;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	q->ops = &msm_cvp_vb2q_ops;

	q->mem_ops = &msm_cvp_vb2_mem_ops;
	q->drv_priv = inst;
	q->allow_zero_bytesused = !V4L2_TYPE_IS_OUTPUT(type);
	q->copy_timestamp = 1;
	return vb2_queue_init(q);
}

static int setup_event_queue(void *inst,
				struct video_device *pvdev)
{
	struct msm_cvp_inst *cvp_inst = (struct msm_cvp_inst *)inst;

	v4l2_fh_init(&cvp_inst->event_handler, pvdev);
	v4l2_fh_add(&cvp_inst->event_handler);

	return 0;
}

int msm_cvp_subscribe_event(void *inst,
	const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_cvp_inst *cvp_inst = (struct msm_cvp_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_subscribe(&cvp_inst->event_handler,
		sub, MAX_EVENTS, NULL);
	return rc;
}
EXPORT_SYMBOL(msm_cvp_subscribe_event);

int msm_cvp_unsubscribe_event(void *inst,
	const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_cvp_inst *cvp_inst = (struct msm_cvp_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_unsubscribe(&cvp_inst->event_handler, sub);
	return rc;
}
EXPORT_SYMBOL(msm_cvp_unsubscribe_event);

int msm_cvp_dqevent(void *inst, struct v4l2_event *event)
{
	int rc = 0;
	struct msm_cvp_inst *cvp_inst = (struct msm_cvp_inst *)inst;

	if (!inst || !event)
		return -EINVAL;

	rc = v4l2_event_dequeue(&cvp_inst->event_handler, event, false);
	return rc;
}
EXPORT_SYMBOL(msm_cvp_dqevent);

int msm_cvp_private(void *cvp_inst, unsigned int cmd,
		struct cvp_kmd_arg *arg)
{
	int rc = 0;
	struct msm_cvp_inst *inst = (struct msm_cvp_inst *)cvp_inst;

	if (!inst || !arg) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}

	if (inst->session_type == MSM_CVP_CORE) {
		rc = msm_cvp_handle_syscall(inst, arg);
	} else {
		dprintk(CVP_ERR,
			"%s: private cmd %#x not supported for session_type %d\n",
			__func__, cmd, inst->session_type);
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(msm_cvp_private);

static bool msm_cvp_check_for_inst_overload(struct msm_cvp_core *core)
{
	u32 instance_count = 0;
	u32 secure_instance_count = 0;
	struct msm_cvp_inst *inst = NULL;
	bool overload = false;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		instance_count++;
		/* This flag is not updated yet for the current instance */
		if (inst->flags & CVP_SECURE)
			secure_instance_count++;
	}
	mutex_unlock(&core->lock);

	/* Instance count includes current instance as well. */

	if ((instance_count > core->resources.max_inst_count) ||
		(secure_instance_count > core->resources.max_secure_inst_count))
		overload = true;
	return overload;
}

static int msm_cvp_try_set_ctrl(void *instance, struct v4l2_ctrl *ctrl)
{
	return -EINVAL;
}

static int msm_cvp_op_s_ctrl(struct v4l2_ctrl *ctrl)
{

	int rc = 0, c = 0;
	struct msm_cvp_inst *inst;

	if (!ctrl) {
		dprintk(CVP_ERR, "%s invalid parameters for ctrl\n", __func__);
		return -EINVAL;
	}

	inst = container_of(ctrl->handler,
		struct msm_cvp_inst, ctrl_handler);
	if (!inst) {
		dprintk(CVP_ERR, "%s invalid parameters for inst\n", __func__);
		return -EINVAL;
	}

	for (c = 0; c < ctrl->ncontrols; ++c) {
		if (ctrl->cluster[c]->is_new) {
			rc = msm_cvp_try_set_ctrl(inst, ctrl->cluster[c]);
			if (rc) {
				dprintk(CVP_ERR, "Failed setting %x\n",
					ctrl->cluster[c]->id);
				break;
			}
		}
	}
	if (rc)
		dprintk(CVP_ERR, "Failed setting control: Inst = %pK (%s)\n",
				inst, v4l2_ctrl_get_name(ctrl->id));
	return rc;
}
static int try_get_ctrl(struct msm_cvp_inst *inst, struct v4l2_ctrl *ctrl)
{
	switch (ctrl->id) {
	default:
		/*
		 * Other controls aren't really volatile, shouldn't need to
		 * modify ctrl->value
		 */
		break;
	}

	return 0;
}

static int msm_cvp_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0, c = 0;
	struct msm_cvp_inst *inst;
	struct v4l2_ctrl *master;

	if (!ctrl) {
		dprintk(CVP_ERR, "%s invalid parameters for ctrl\n", __func__);
		return -EINVAL;
	}

	inst = container_of(ctrl->handler,
		struct msm_cvp_inst, ctrl_handler);
	if (!inst) {
		dprintk(CVP_ERR, "%s invalid parameters for inst\n", __func__);
		return -EINVAL;
	}
	master = ctrl->cluster[0];
	if (!master) {
		dprintk(CVP_ERR, "%s invalid parameters for master\n",
			__func__);
		return -EINVAL;
	}

	for (c = 0; c < master->ncontrols; ++c) {
		if (master->cluster[c]->flags & V4L2_CTRL_FLAG_VOLATILE) {
			rc = try_get_ctrl(inst, master->cluster[c]);
			if (rc) {
				dprintk(CVP_ERR, "Failed getting %x\n",
					master->cluster[c]->id);
				return rc;
			}
		}
	}
	if (rc)
		dprintk(CVP_ERR, "Failed getting control: Inst = %pK (%s)\n",
				inst, v4l2_ctrl_get_name(ctrl->id));
	return rc;
}

static const struct v4l2_ctrl_ops msm_cvp_ctrl_ops = {

	.s_ctrl = msm_cvp_op_s_ctrl,
	.g_volatile_ctrl = msm_cvp_op_g_volatile_ctrl,
};

static int _init_session_queue(struct msm_cvp_inst *inst)
{
	spin_lock_init(&inst->session_queue.lock);
	INIT_LIST_HEAD(&inst->session_queue.msgs);
	inst->session_queue.msg_count = 0;
	init_waitqueue_head(&inst->session_queue.wq);
	inst->session_queue.msg_cache = KMEM_CACHE(session_msg, 0);
	if (!inst->session_queue.msg_cache) {
		dprintk(CVP_ERR, "Failed to allocate msg quque\n");
		return -ENOMEM;
	}
	return 0;
}

static void _deinit_session_queue(struct msm_cvp_inst *inst)
{
	struct session_msg *msg, *tmpmsg;

	/* free all messages */
	spin_lock(&inst->session_queue.lock);
	list_for_each_entry_safe(msg, tmpmsg, &inst->session_queue.msgs, node) {
		list_del_init(&msg->node);
		kmem_cache_free(inst->session_queue.msg_cache, msg);
	}
	inst->session_queue.msg_count = 0;
	spin_unlock(&inst->session_queue.lock);

	wake_up_all(&inst->session_queue.wq);

	kmem_cache_destroy(inst->session_queue.msg_cache);
}

void *msm_cvp_open(int core_id, int session_type)
{
	struct msm_cvp_inst *inst = NULL;
	struct msm_cvp_core *core = NULL;
	int rc = 0;
	int i = 0;

	if (core_id >= MSM_CVP_CORES_MAX ||
			session_type >= MSM_CVP_MAX_DEVICES) {
		dprintk(CVP_ERR, "Invalid input, core_id = %d, session = %d\n",
			core_id, session_type);
		goto err_invalid_core;
	}
	core = get_cvp_core(core_id);
	if (!core) {
		dprintk(CVP_ERR,
			"Failed to find core for core_id = %d\n", core_id);
		goto err_invalid_core;
	}

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		dprintk(CVP_ERR, "Failed to allocate memory\n");
		rc = -ENOMEM;
		goto err_invalid_core;
	}

	pr_info(CVP_DBG_TAG "Opening video instance: %pK, %d\n",
		"info", inst, session_type);
	mutex_init(&inst->sync_lock);
	mutex_init(&inst->bufq[CAPTURE_PORT].lock);
	mutex_init(&inst->bufq[OUTPUT_PORT].lock);
	mutex_init(&inst->lock);
	mutex_init(&inst->flush_lock);

	INIT_MSM_CVP_LIST(&inst->scratchbufs);
	INIT_MSM_CVP_LIST(&inst->freqs);
	INIT_MSM_CVP_LIST(&inst->input_crs);
	INIT_MSM_CVP_LIST(&inst->persistbufs);
	INIT_MSM_CVP_LIST(&inst->pending_getpropq);
	INIT_MSM_CVP_LIST(&inst->outputbufs);
	INIT_MSM_CVP_LIST(&inst->registeredbufs);
	INIT_MSM_CVP_LIST(&inst->cvpbufs);
	INIT_MSM_CVP_LIST(&inst->reconbufs);
	INIT_MSM_CVP_LIST(&inst->eosbufs);
	INIT_MSM_CVP_LIST(&inst->etb_data);
	INIT_MSM_CVP_LIST(&inst->fbd_data);
	INIT_MSM_CVP_LIST(&inst->dfs_config);

	kref_init(&inst->kref);

	inst->session_type = session_type;
	inst->state = MSM_CVP_CORE_UNINIT_DONE;
	inst->core = core;
	inst->clk_data.min_freq = 0;
	inst->clk_data.curr_freq = 0;
	inst->clk_data.ddr_bw = 0;
	inst->clk_data.sys_cache_bw = 0;
	inst->clk_data.bitrate = 0;
	inst->clk_data.core_id = CVP_CORE_ID_DEFAULT;
	inst->bit_depth = MSM_CVP_BIT_DEPTH_8;
	inst->pic_struct = MSM_CVP_PIC_STRUCT_PROGRESSIVE;
	inst->colour_space = MSM_CVP_BT601_6_525;
	inst->profile = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
	inst->level = V4L2_MPEG_VIDEO_H264_LEVEL_1_0;
	inst->entropy_mode = V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC;

	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}

	if (session_type == MSM_CVP_CORE) {
		msm_cvp_session_init(inst);
		rc = msm_cvp_control_init(inst, &msm_cvp_ctrl_ops);
	}
	if (rc) {
		dprintk(CVP_ERR, "Failed control initialization\n");
		goto fail_bufq_capture;
	}

	rc = vb2_bufq_init(inst, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
			session_type);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to initialize vb2 queue on capture port\n");
		goto fail_bufq_capture;
	}
	rc = vb2_bufq_init(inst, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			session_type);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to initialize vb2 queue on capture port\n");
		goto fail_bufq_output;
	}

	setup_event_queue(inst, &core->vdev[session_type].vdev);

	mutex_lock(&core->lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->lock);


	rc = _init_session_queue(inst);
	if (rc)
		goto fail_init;

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CORE_INIT_DONE);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move video instance to init state\n");
		goto fail_init;
	}

	msm_cvp_dcvs_try_enable(inst);
	core->resources.max_inst_count = 1;
	if (msm_cvp_check_for_inst_overload(core)) {
		dprintk(CVP_ERR,
			"Instance count reached Max limit, rejecting session");
		goto fail_init;
	}

	msm_cvp_comm_scale_clocks_and_bus(inst);

	inst->debugfs_root =
		msm_cvp_debugfs_init_inst(inst, core->debugfs_root);

	if (inst->session_type == MSM_CVP_CORE) {
		rc = msm_cvp_comm_try_state(inst, MSM_CVP_OPEN_DONE);
		if (rc) {
			dprintk(CVP_ERR,
				"Failed to move video instance to open done state\n");
			goto fail_init;
		}
		rc = cvp_comm_set_persist_buffers(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"Failed to set ARP buffers\n");
			goto fail_init;
		}

	}

	return inst;
fail_init:
	_deinit_session_queue(inst);
	mutex_lock(&core->lock);
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);
	vb2_queue_release(&inst->bufq[OUTPUT_PORT].vb2_bufq);
fail_bufq_output:
	vb2_queue_release(&inst->bufq[CAPTURE_PORT].vb2_bufq);
fail_bufq_capture:
	msm_cvp_comm_ctrl_deinit(inst);
	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->bufq[CAPTURE_PORT].lock);
	mutex_destroy(&inst->bufq[OUTPUT_PORT].lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->flush_lock);

	DEINIT_MSM_CVP_LIST(&inst->scratchbufs);
	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_MSM_CVP_LIST(&inst->pending_getpropq);
	DEINIT_MSM_CVP_LIST(&inst->outputbufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpbufs);
	DEINIT_MSM_CVP_LIST(&inst->registeredbufs);
	DEINIT_MSM_CVP_LIST(&inst->eosbufs);
	DEINIT_MSM_CVP_LIST(&inst->freqs);
	DEINIT_MSM_CVP_LIST(&inst->input_crs);
	DEINIT_MSM_CVP_LIST(&inst->etb_data);
	DEINIT_MSM_CVP_LIST(&inst->fbd_data);
	DEINIT_MSM_CVP_LIST(&inst->dfs_config);

	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}
EXPORT_SYMBOL(msm_cvp_open);

static void msm_cvp_cleanup_instance(struct msm_cvp_inst *inst)
{
	struct msm_video_buffer *temp, *dummy;
	struct getprop_buf *temp_prop, *dummy_prop;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(temp, dummy, &inst->registeredbufs.list,
			list) {
		print_video_buffer(CVP_ERR, "undequeud buf", inst, temp);
		msm_cvp_comm_unmap_video_buffer(inst, temp);
		list_del(&temp->list);
		kref_cvp_put_mbuf(temp);
	}
	mutex_unlock(&inst->registeredbufs.lock);

	msm_cvp_comm_free_freq_table(inst);

	msm_cvp_comm_free_input_cr_table(inst);

	if (msm_cvp_comm_release_scratch_buffers(inst, false))
		dprintk(CVP_ERR,
			"Failed to release scratch buffers\n");

	if (msm_cvp_comm_release_recon_buffers(inst))
		dprintk(CVP_ERR,
			"Failed to release recon buffers\n");

	if (cvp_comm_release_persist_buffers(inst))
		dprintk(CVP_ERR,
			"Failed to release persist buffers\n");

	if (msm_cvp_comm_release_mark_data(inst))
		dprintk(CVP_ERR,
			"Failed to release mark_data buffers\n");

	msm_cvp_comm_release_eos_buffers(inst);

	if (msm_cvp_comm_release_output_buffers(inst, true))
		dprintk(CVP_ERR,
			"Failed to release output buffers\n");

	if (inst->extradata_handle)
		msm_cvp_comm_smem_free(inst, inst->extradata_handle);

	mutex_lock(&inst->pending_getpropq.lock);
	if (!list_empty(&inst->pending_getpropq.list)) {
		dprintk(CVP_ERR,
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

int msm_cvp_destroy(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core;
	int i = 0;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;

	mutex_lock(&core->lock);
	/* inst->list lives in core->instances */
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	msm_cvp_comm_ctrl_deinit(inst);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);

	for (i = 0; i < MAX_PORT_NUM; i++)
		vb2_queue_release(&inst->bufq[i].vb2_bufq);

	DEINIT_MSM_CVP_LIST(&inst->scratchbufs);
	DEINIT_MSM_CVP_LIST(&inst->persistbufs);
	DEINIT_MSM_CVP_LIST(&inst->pending_getpropq);
	DEINIT_MSM_CVP_LIST(&inst->outputbufs);
	DEINIT_MSM_CVP_LIST(&inst->cvpbufs);
	DEINIT_MSM_CVP_LIST(&inst->registeredbufs);
	DEINIT_MSM_CVP_LIST(&inst->eosbufs);
	DEINIT_MSM_CVP_LIST(&inst->freqs);
	DEINIT_MSM_CVP_LIST(&inst->input_crs);
	DEINIT_MSM_CVP_LIST(&inst->etb_data);
	DEINIT_MSM_CVP_LIST(&inst->fbd_data);
	DEINIT_MSM_CVP_LIST(&inst->dfs_config);

	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->bufq[CAPTURE_PORT].lock);
	mutex_destroy(&inst->bufq[OUTPUT_PORT].lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->flush_lock);

	msm_cvp_debugfs_deinit_inst(inst);
	_deinit_session_queue(inst);

	pr_info(CVP_DBG_TAG "Closed cvp instance: %pK\n",
			"info", inst);
	kfree(inst);
	return 0;
}

static void close_helper(struct kref *kref)
{
	struct msm_cvp_inst *inst = container_of(kref,
			struct msm_cvp_inst, kref);

	msm_cvp_destroy(inst);
}

int msm_cvp_close(void *instance)
{
	struct msm_cvp_inst *inst = instance;
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	msm_cvp_cleanup_instance(inst);

	/*
	 * deinit instance after REL_RES_DONE to ensure hardware
	 * released all buffers.
	 */
	if (inst->session_type == MSM_CVP_CORE)
		msm_cvp_session_deinit(inst);


	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CORE_UNINIT);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move inst %pK to uninit state\n", inst);
		rc = msm_cvp_comm_force_cleanup(inst);
	}

	msm_cvp_comm_session_clean(inst);

	kref_put(&inst->kref, close_helper);
	return 0;
}
EXPORT_SYMBOL(msm_cvp_close);

int msm_cvp_suspend(int core_id)
{
	return msm_cvp_comm_suspend(core_id);
}
EXPORT_SYMBOL(msm_cvp_suspend);

