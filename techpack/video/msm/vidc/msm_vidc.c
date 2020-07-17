// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-direction.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "msm_vidc.h"
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "msm_vdec.h"
#include "msm_venc.h"
#include "msm_cvp_internal.h"
#include "msm_vidc_common.h"
#include <linux/delay.h>
#include "vidc_hfi.h"
#include "vidc_hfi_helper.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_clocks.h"
#include "msm_vidc_buffer_calculations.h"
#include <linux/dma-buf.h>

#define MAX_EVENTS 30

static int try_get_ctrl_for_instance(struct msm_vidc_inst *inst,
	struct v4l2_ctrl *ctrl);

static int get_poll_flags(void *instance)
{
	struct msm_vidc_inst *inst = instance;
	struct vb2_queue *outq = &inst->bufq[INPUT_PORT].vb2_bufq;
	struct vb2_queue *capq = &inst->bufq[OUTPUT_PORT].vb2_bufq;
	struct vb2_buffer *out_vb = NULL;
	struct vb2_buffer *cap_vb = NULL;
	unsigned long flags = 0;
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

	outq = &inst->bufq[INPUT_PORT].vb2_bufq;
	capq = &inst->bufq[OUTPUT_PORT].vb2_bufq;

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

int msm_vidc_query_ctrl(void *instance, struct v4l2_queryctrl *q_ctrl)
{
	int rc = 0;
	struct msm_vidc_inst *inst = instance;
	struct v4l2_ctrl *ctrl;

	if (!inst || !q_ctrl) {
		d_vpr_e("%s: invalid params %pK %pK\n",
			__func__, inst, q_ctrl);
		return -EINVAL;
	}

	ctrl = v4l2_ctrl_find(&inst->ctrl_handler, q_ctrl->id);
	if (!ctrl) {
		s_vpr_e(inst->sid, "%s: get_ctrl failed for id %d\n",
			__func__, q_ctrl->id);
		return -EINVAL;
	}
	q_ctrl->minimum = ctrl->minimum;
	q_ctrl->maximum = ctrl->maximum;
	/* remove tier info for HEVC level */
	if (q_ctrl->id == V4L2_CID_MPEG_VIDEO_HEVC_LEVEL) {
		q_ctrl->minimum &= ~(0xF << 28);
		q_ctrl->maximum &= ~(0xF << 28);
	}
	if (ctrl->type == V4L2_CTRL_TYPE_MENU)
		q_ctrl->flags = ~(ctrl->menu_skip_mask);
	else
		q_ctrl->flags = 0;

	s_vpr_h(inst->sid, "query ctrl: %s: min %d, max %d, flags %#x\n",
		ctrl->name, q_ctrl->minimum, q_ctrl->maximum, q_ctrl->flags);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_query_ctrl);

int msm_vidc_s_fmt(void *instance, struct v4l2_format *f)
{
	int rc = 0;
	struct msm_vidc_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		rc = msm_vdec_s_fmt(instance, f);
	if (inst->session_type == MSM_VIDC_ENCODER)
		rc = msm_venc_s_fmt(instance, f);

	s_vpr_h(inst->sid,
		"s_fmt: type %d wxh %dx%d pixelfmt %#x num_planes %d size[0] %d size[1] %d in_reconfig %d\n",
		f->type, f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes,
		f->fmt.pix_mp.plane_fmt[0].sizeimage,
		f->fmt.pix_mp.plane_fmt[1].sizeimage, inst->in_reconfig);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_s_fmt);

int msm_vidc_g_fmt(void *instance, struct v4l2_format *f)
{
	int rc = 0;
	struct msm_vidc_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		rc = msm_vdec_g_fmt(instance, f);
	if (inst->session_type == MSM_VIDC_ENCODER)
		rc = msm_venc_g_fmt(instance, f);

	s_vpr_h(inst->sid,
		"g_fmt: type %d wxh %dx%d pixelfmt %#x num_planes %d size[0] %d size[1] %d in_reconfig %d\n",
		f->type, f->fmt.pix_mp.width, f->fmt.pix_mp.height,
		f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.num_planes,
		f->fmt.pix_mp.plane_fmt[0].sizeimage,
		f->fmt.pix_mp.plane_fmt[1].sizeimage, inst->in_reconfig);
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

int msm_vidc_g_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_vidc_inst *inst = instance;
	struct v4l2_ctrl *ctrl = NULL;
	int rc = 0;

	if (!inst || !control)
		return -EINVAL;

	ctrl = v4l2_ctrl_find(&inst->ctrl_handler, control->id);
	if (ctrl) {
		rc = try_get_ctrl_for_instance(inst, ctrl);
		if (!rc)
			control->value = ctrl->val;
	}

	return rc;
}
EXPORT_SYMBOL(msm_vidc_g_ctrl);

int msm_vidc_reqbufs(void *instance, struct v4l2_requestbuffers *b)
{
	struct msm_vidc_inst *inst = instance;
	struct buf_queue *q = NULL;
	int rc = 0;

	if (!inst || !b)
		return -EINVAL;
	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		s_vpr_e(inst->sid,
			"Failed to find buffer queue. type %d\n", b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_reqbufs(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);

	if (rc)
		s_vpr_e(inst->sid, "Failed to get reqbufs, %d\n", rc);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_reqbufs);

static bool valid_v4l2_buffer(struct v4l2_buffer *b,
		struct msm_vidc_inst *inst)
{
	struct v4l2_format *f;
	enum vidc_ports port =
		!V4L2_TYPE_IS_MULTIPLANAR(b->type) ? MAX_PORT_NUM :
		b->type == OUTPUT_MPLANE ? OUTPUT_PORT :
		b->type == INPUT_MPLANE ? INPUT_PORT :
								MAX_PORT_NUM;

	f = &inst->fmts[port].v4l2_fmt;
	return port != MAX_PORT_NUM &&
		f->fmt.pix_mp.num_planes == b->length;
}

int msm_vidc_release_buffer(void *instance, int type, unsigned int index)
{
	int rc = 0;
	struct msm_vidc_inst *inst = instance;
	struct msm_vidc_buffer *mbuf, *dummy;

	if (!inst) {
		d_vpr_e("%s: invalid inst\n", __func__);
		return -EINVAL;
	}

	if (!inst->in_reconfig &&
		inst->state > MSM_VIDC_LOAD_RESOURCES &&
		inst->state < MSM_VIDC_RELEASE_RESOURCES_DONE) {
		rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
		if (rc) {
			s_vpr_e(inst->sid,
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

		if (mbuf->flags & MSM_VIDC_FLAG_RBR_PENDING) {
			print_vidc_buffer(VIDC_HIGH,
				"skip rel buf (rbr pending)", inst, mbuf);
			continue;
		}

		print_vidc_buffer(VIDC_HIGH, "release buf", inst, mbuf);
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
	struct msm_vidc_client_data *client_data = NULL;
	int rc = 0;
	unsigned int i = 0;
	struct buf_queue *q = NULL;
	u32 cr = 0;

	if (!inst || !inst->core || !b || !valid_v4l2_buffer(b, inst)) {
		d_vpr_e("%s: invalid params %pK %pK\n", __func__, inst, b);
		return -EINVAL;
	}

	if (!IS_ALIGNED(b->m.planes[0].length, SZ_4K)) {
		s_vpr_e(inst->sid, "qbuf: buffer size not 4K aligned - %u\n",
			b->m.planes[0].length);
		return -EINVAL;
	}

	if ((inst->out_flush && b->type == OUTPUT_MPLANE) || inst->in_flush) {
		s_vpr_e(inst->sid,
			"%s: in flush, discarding qbuf, type %u, index %u\n",
			__func__, b->type, b->index);
		return -EINVAL;
	}

	for (i = 0; i < b->length; i++) {
		b->m.planes[i].m.fd =
				b->m.planes[i].reserved[MSM_VIDC_BUFFER_FD];
		b->m.planes[i].data_offset =
				b->m.planes[i].reserved[MSM_VIDC_DATA_OFFSET];
	}

	/* Compression ratio is valid only for Encoder YUV buffers. */
	if (inst->session_type == MSM_VIDC_ENCODER &&
			b->type == INPUT_MPLANE) {
		cr = b->m.planes[0].reserved[MSM_VIDC_COMP_RATIO];
		msm_comm_update_input_cr(inst, b->index, cr);
	}

	if (b->type == INPUT_MPLANE) {
		client_data = msm_comm_store_client_data(inst,
			b->m.planes[0].reserved[MSM_VIDC_INPUT_TAG_1]);
		if (!client_data) {
			s_vpr_e(inst->sid,
				"%s: failed to store client data\n", __func__);
			return -EINVAL;
		}
		msm_comm_store_input_tag(&inst->etb_data, b->index,
			client_data->id, 0, inst->sid);
	}
	/*
	 * set perf mode for image session buffers so that
	 * they will be processed quickly
	 */
	if (is_grid_session(inst) && b->type == INPUT_MPLANE)
		b->flags |= V4L2_BUF_FLAG_PERF_MODE;

	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		s_vpr_e(inst->sid,
			"Failed to find buffer queue. type %d\n", b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_qbuf(&q->vb2_bufq, b);
	mutex_unlock(&q->lock);
	if (rc)
		s_vpr_e(inst->sid, "Failed to qbuf, %d\n", rc);

	return rc;
}
EXPORT_SYMBOL(msm_vidc_qbuf);

int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0;
	unsigned int i = 0;
	struct buf_queue *q = NULL;
	u32 input_tag = 0, input_tag2 = 0;
	bool remove;

	if (!inst || !b || !valid_v4l2_buffer(b, inst)) {
		d_vpr_e("%s: invalid params, %pK %pK\n",
			__func__, inst, b);
		return -EINVAL;
	}

	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		s_vpr_e(inst->sid, "Failed to find buffer queue. type %d\n",
			b->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_dqbuf(&q->vb2_bufq, b, true);
	mutex_unlock(&q->lock);
	if (rc == -EAGAIN) {
		return rc;
	} else if (rc) {
		s_vpr_e(inst->sid, "Failed to dqbuf, %d\n", rc);
		return rc;
	}

	for (i = 0; i < b->length; i++) {
		b->m.planes[i].reserved[MSM_VIDC_BUFFER_FD] =
					b->m.planes[i].m.fd;
		b->m.planes[i].reserved[MSM_VIDC_DATA_OFFSET] =
					b->m.planes[i].data_offset;
	}
	/**
	 * Flush handling:
	 * Don't fetch tag - if flush issued at input/output port.
	 * Fetch tag - if atleast 1 ebd received after flush. (Flush_done
	 * event may be notified to userspace even before client
	 * dequeus all buffers at FBD, to avoid this race condition
	 * fetch tag atleast 1 ETB is successfully processed after flush)
	 */
	if (b->type == OUTPUT_MPLANE && !inst->in_flush &&
			!inst->out_flush && inst->clk_data.buffer_counter) {
		rc = msm_comm_fetch_input_tag(&inst->fbd_data, b->index,
				&input_tag, &input_tag2, inst->sid);
		if (rc) {
			s_vpr_e(inst->sid, "Failed to fetch input tag");
			return -EINVAL;
		}
		/**
		 * During flush input_tag & input_tag2 will be zero.
		 * Check before retrieving client data
		 */
		if (input_tag) {
			remove = !(b->flags & V4L2_BUF_FLAG_END_OF_SUBFRAME) &&
					!(b->flags & V4L2_BUF_FLAG_CODECCONFIG);
			msm_comm_fetch_client_data(inst, remove,
				input_tag, input_tag2,
				&b->m.planes[0].reserved[MSM_VIDC_INPUT_TAG_1],
				&b->m.planes[0].reserved[MSM_VIDC_INPUT_TAG_2]);
		}
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
		d_vpr_e("Failed to find buffer queue. type %d\n", i);
		return -EINVAL;
	}
	s_vpr_h(inst->sid, "Calling streamon\n");
	mutex_lock(&q->lock);
	rc = vb2_streamon(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc) {
		s_vpr_e(inst->sid, "streamon failed on port: %d\n", i);
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
		s_vpr_e(inst->sid, "Failed to find buffer queue. type %d\n", i);
		return -EINVAL;
	}

	if (!inst->in_reconfig) {
		s_vpr_h(inst->sid, "%s: inst %pK release resources\n",
			__func__, inst);
		rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
		if (rc)
			s_vpr_e(inst->sid,
				"%s: inst %pK move to rel res done failed\n",
				__func__, inst);
	}

	s_vpr_h(inst->sid, "Calling streamoff\n");
	mutex_lock(&q->lock);
	rc = vb2_streamoff(&q->vb2_bufq, i);
	mutex_unlock(&q->lock);
	if (rc)
		s_vpr_e(inst->sid, "streamoff failed on port: %d\n", i);
	return rc;
}
EXPORT_SYMBOL(msm_vidc_streamoff);

int msm_vidc_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize)
{
	struct msm_vidc_inst *inst = instance;
	struct msm_vidc_capability *capability = NULL;

	if (!inst || !fsize) {
		d_vpr_e("%s: invalid parameter: %pK %pK\n",
				__func__, inst, fsize);
		return -EINVAL;
	}
	if (!inst->core)
		return -EINVAL;

	capability = &inst->capability;
	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.min_width = capability->cap[CAP_FRAME_WIDTH].min;
	fsize->stepwise.max_width = capability->cap[CAP_FRAME_WIDTH].max;
	fsize->stepwise.step_width =
		capability->cap[CAP_FRAME_WIDTH].step_size;
	fsize->stepwise.min_height = capability->cap[CAP_FRAME_HEIGHT].min;
	fsize->stepwise.max_height = capability->cap[CAP_FRAME_HEIGHT].max;
	fsize->stepwise.step_height =
		capability->cap[CAP_FRAME_HEIGHT].step_size;
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
		d_vpr_e("%s: Invalid vb pointer", __func__);
		return;
	}

	inst = vb2_get_drv_priv(vb->vb2_queue);
	if (!inst) {
		d_vpr_e("%s: Invalid inst pointer", __func__);
		return;
	}

	q = msm_comm_get_vb2q(inst, vb->type);
	if (!q) {
		s_vpr_e(inst->sid,
			"%s: Failed to find buffer queue. type %d\n",
			__func__, vb->type);
		return;
	}

	if (q->vb2_bufq.streaming) {
		s_vpr_h(inst->sid, "%d PORT is streaming\n",
			vb->type);
		return;
	}

	rc = msm_vidc_release_buffer(inst, vb->type, vb->index);
	if (rc)
		s_vpr_e(inst->sid, "%s: Failed to release buffers: %d\n",
			__func__, rc);
}

static int msm_vidc_queue_setup(struct vb2_queue *q,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[], struct device *alloc_devs[])
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	unsigned int i = 0;
	struct msm_vidc_format *fmt;
	struct v4l2_format *f;

	if (!q || !num_buffers || !num_planes
		|| !sizes || !q->drv_priv) {
		d_vpr_e("Invalid input, q = %pK, %pK, %pK\n",
			q, num_buffers, num_planes);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core || !inst->core->device) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	switch (q->type) {
	case INPUT_MPLANE: {
		fmt = &inst->fmts[INPUT_PORT];
		if (*num_buffers < fmt->count_min_host) {
			s_vpr_h(inst->sid,
				"Client passed num buffers %d less than the min_host count %d\n",
				*num_buffers, fmt->count_min_host);
		}
		f = &fmt->v4l2_fmt;
		*num_planes = f->fmt.pix_mp.num_planes;
		if (*num_buffers < SINGLE_INPUT_BUFFER ||
			*num_buffers > MAX_NUM_INPUT_BUFFERS)
			fmt->count_actual = *num_buffers =
				SINGLE_INPUT_BUFFER;
		for (i = 0; i < *num_planes; i++)
			sizes[i] = f->fmt.pix_mp.plane_fmt[i].sizeimage;

		fmt->count_actual = *num_buffers;
		}
		break;
	case OUTPUT_MPLANE: {
		fmt = &inst->fmts[OUTPUT_PORT];
		if (inst->session_type != MSM_VIDC_DECODER &&
			inst->state > MSM_VIDC_LOAD_RESOURCES_DONE) {
			if (*num_buffers < fmt->count_min_host) {
				s_vpr_h(inst->sid,
					"Client passed num buffers %d less than the min_host count %d\n",
						*num_buffers,
						fmt->count_min_host);
			}
		}
		f = &fmt->v4l2_fmt;
		*num_planes = f->fmt.pix_mp.num_planes;
		if (*num_buffers < SINGLE_OUTPUT_BUFFER ||
			*num_buffers > MAX_NUM_OUTPUT_BUFFERS)
			fmt->count_actual = *num_buffers =
				SINGLE_OUTPUT_BUFFER;

		for (i = 0; i < *num_planes; i++)
			sizes[i] = f->fmt.pix_mp.plane_fmt[i].sizeimage;

		fmt->count_actual = *num_buffers;
		}
		break;
	default:
		s_vpr_e(inst->sid, "Invalid q type = %d\n", q->type);
		rc = -EINVAL;
		break;
	}

	s_vpr_h(inst->sid,
		"queue_setup:type %d num_buffers %d num_planes %d sizes[0] %d sizes[1] %d\n",
		q->type, *num_buffers, *num_planes, sizes[0], sizes[1]);
	return rc;
}

static inline int msm_vidc_verify_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc = 0, i = 0;

	/* For decoder No need to sanity till LOAD_RESOURCES */
	if (inst->session_type == MSM_VIDC_DECODER &&
			(inst->state < MSM_VIDC_LOAD_RESOURCES_DONE ||
			inst->state >= MSM_VIDC_RELEASE_RESOURCES_DONE)) {
		s_vpr_h(inst->sid, "No need to verify buffer counts\n");
		return 0;
	}

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		struct hal_buffer_requirements *req = &inst->buff_req.buffer[i];

		if (req && (req->buffer_type == HAL_BUFFER_OUTPUT)) {
			s_vpr_h(inst->sid, "Verifying Buffer : %d\n",
				req->buffer_type);
			if (req->buffer_count_actual <
					req->buffer_count_min_host ||
				req->buffer_count_min_host <
					req->buffer_count_min) {

				s_vpr_e(inst->sid,
						"Invalid data : Counts mismatch\n");
				s_vpr_e(inst->sid, "Min Count = %d ",
						req->buffer_count_min);
				s_vpr_e(inst->sid, "Min Host Count = %d ",
						req->buffer_count_min_host);
				s_vpr_e(inst->sid, "Min Actual Count = %d\n",
						req->buffer_count_actual);
				rc = -EINVAL;
				break;
			}
		}
	}
	return rc;
}

static int msm_vidc_set_properties(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (is_decode_session(inst))
		rc = msm_vdec_set_properties(inst);
	else if (is_encode_session(inst))
		rc = msm_venc_set_properties(inst);

	return rc;
}

static inline int start_streaming(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hfi_buffer_size_minimum b;
	struct v4l2_format *f;

	s_vpr_h(inst->sid, "%s: inst %pK\n", __func__, inst);
	hdev = inst->core->device;

	rc = msm_vidc_set_properties(inst);
	if (rc) {
		s_vpr_e(inst->sid, "%s: set props failed\n", __func__);
		goto fail_start;
	}

	b.buffer_type = HFI_BUFFER_OUTPUT;
	if (inst->session_type == MSM_VIDC_DECODER &&
		is_secondary_output_mode(inst))
		b.buffer_type = HFI_BUFFER_OUTPUT2;

	/* Check if current session is under HW capability */
	rc = msm_vidc_check_session_supported(inst);
	if (rc) {
		s_vpr_e(inst->sid, "This session is not supported\n");
		goto fail_start;
	}

	rc = msm_vidc_check_scaling_supported(inst);
	if (rc) {
		s_vpr_e(inst->sid, "scaling is not supported\n");
		goto fail_start;
	}

	/* Decide work mode for current session */
	rc = call_core_op(inst->core, decide_work_mode, inst);
	if (rc) {
		s_vpr_e(inst->sid, "Failed to decide work mode\n");
		goto fail_start;
	}

	/* Decide work route for current session */
	rc = call_core_op(inst->core, decide_work_route, inst);
	if (rc) {
		s_vpr_e(inst->sid, "Failed to decide work route\n");
		goto fail_start;
	}

	/* Assign Core and LP mode for current session */
	rc = call_core_op(inst->core, decide_core_and_power_mode, inst);
	if (rc) {
		s_vpr_e(inst->sid,
			"This session can't be submitted to HW %pK\n", inst);
		goto fail_start;
	}

	rc = msm_comm_try_get_bufreqs(inst);

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	b.buffer_size = f->fmt.pix_mp.plane_fmt[0].sizeimage;
	rc = call_hfi_op(hdev, session_set_property,
			inst->session, HFI_PROPERTY_PARAM_BUFFER_SIZE_MINIMUM,
			&b, sizeof(b));

	/* Verify if buffer counts are correct */
	rc = msm_vidc_verify_buffer_counts(inst);
	if (rc) {
		s_vpr_e(inst->sid,
			"This session has mis-match buffer counts%pK\n", inst);
		goto fail_start;
	}

	rc = msm_comm_set_scratch_buffers(inst);
	if (rc) {
		s_vpr_e(inst->sid, "Failed to set scratch buffers: %d\n", rc);
		goto fail_start;
	}
	rc = msm_comm_set_persist_buffers(inst);
	if (rc) {
		s_vpr_e(inst->sid, "Failed to set persist buffers: %d\n", rc);
		goto fail_start;
	}

	rc = msm_comm_set_recon_buffers(inst);
	if (rc) {
		s_vpr_e(inst->sid, "Failed to set recon buffers: %d\n", rc);
		goto fail_start;
	}

	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_set_dpb_only_buffers(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"Failed to set output buffers: %d\n", rc);
			goto fail_start;
		}
	}

	/*
	 * if batching enabled previously then you may chose
	 * to disable it based on recent configuration changes.
	 * if batching already disabled do not enable it again
	 * as sufficient extra buffers (required for batch mode
	 * on both ports) may not have been updated to client.
	 */
	if (inst->batch.enable)
		inst->batch.enable = is_batching_allowed(inst);
	s_vpr_hp(inst->sid, "%s: batching %s for inst %pK\n",
		__func__, inst->batch.enable ? "enabled" : "disabled", inst);

	msm_dcvs_try_enable(inst);

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

	msm_comm_scale_clocks_and_bus(inst, 1);

	rc = msm_comm_try_state(inst, MSM_VIDC_START_DONE);
	if (rc) {
		s_vpr_e(inst->sid,
			"Failed to move inst: %pK to start done state\n", inst);
		goto fail_start;
	}

	msm_clock_data_reset(inst);

	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		rc = msm_comm_queue_dpb_only_buffers(inst);
		if (rc) {
			s_vpr_e(inst->sid,
				"Failed to queue output buffers: %d\n", rc);
			goto fail_start;
		}
	}

fail_start:
	if (rc)
		s_vpr_e(inst->sid, "%s: inst %pK failed to start\n",
			__func__, inst);
	return rc;
}

static int msm_vidc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct msm_vidc_inst *inst;
	int rc = 0;
	struct hfi_device *hdev;

	if (!q || !q->drv_priv) {
		d_vpr_e("Invalid input, q = %pK\n", q);
		return -EINVAL;
	}
	inst = q->drv_priv;
	if (!inst || !inst->core || !inst->core->device) {
		d_vpr_e("%s: invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	s_vpr_h(inst->sid, "Streamon called on: %d capability for inst: %pK\n",
		q->type, inst);
	switch (q->type) {
	case INPUT_MPLANE:
		if (inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		break;
	case OUTPUT_MPLANE:
		if (inst->bufq[INPUT_PORT].vb2_bufq.streaming)
			rc = start_streaming(inst);
		break;
	default:
		s_vpr_e(inst->sid,
			"Queue type is not supported: %d\n", q->type);
		rc = -EINVAL;
		goto stream_start_failed;
	}
	if (rc) {
		s_vpr_e(inst->sid, "Streamon failed: %d, inst: %pK\n",
			q->type, inst);
		goto stream_start_failed;
	}

	rc = msm_comm_qbufs(inst);
	if (rc) {
		s_vpr_e(inst->sid,
			"Failed to commit buffers queued before STREAM_ON: %d\n",
			rc);
		goto stream_start_failed;
	}

	rc = msm_vidc_send_pending_eos_buffers(inst);
	if (rc) {
		s_vpr_e(inst->sid, "Failed : Send pending EOS: %d\n", rc);
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
					print_vb2_buffer("return vb", inst, vb);
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

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	s_vpr_h(inst->sid, "%s: inst %pK\n", __func__, inst);

	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc)
		s_vpr_e(inst->sid, "Failed to move inst: %pK to state %d\n",
				inst, MSM_VIDC_RELEASE_RESOURCES_DONE);

	if (is_encode_session(inst)) {
		inst->all_intra = false;
	}

	msm_clock_data_reset(inst);

	return rc;
}

static void msm_vidc_stop_streaming(struct vb2_queue *q)
{
	struct msm_vidc_inst *inst;
	int rc = 0;

	if (!q || !q->drv_priv) {
		d_vpr_e("Invalid input, q = %pK\n", q);
		return;
	}

	inst = q->drv_priv;
	s_vpr_h(inst->sid, "Streamoff called on: %d capability\n", q->type);
	switch (q->type) {
	case INPUT_MPLANE:
		if (!inst->bufq[OUTPUT_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	case OUTPUT_MPLANE:
		if (!inst->bufq[INPUT_PORT].vb2_bufq.streaming)
			rc = stop_streaming(inst);
		break;
	default:
		s_vpr_e(inst->sid, "Q-type is not supported: %d\n", q->type);
		rc = -EINVAL;
		break;
	}

	msm_comm_scale_clocks_and_bus(inst, 1);

	if (rc)
		s_vpr_e(inst->sid,
			"Failed STOP Streaming inst = %pK on cap = %d\n",
			inst, q->type);
}

static int msm_vidc_queue_buf(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb2)
{
	int rc = 0;
	struct msm_vidc_buffer *mbuf;

	if (!inst || !vb2) {
		d_vpr_e("%s: invalid params %pK, %pK\n",
			__func__, inst, vb2);
		return -EINVAL;
	}

	mbuf = msm_comm_get_vidc_buffer(inst, vb2);
	if (IS_ERR_OR_NULL(mbuf)) {
		/*
		 * if the buffer has RBR_PENDING flag (-EEXIST) then don't queue
		 * it now, it will be queued via msm_comm_qbuf_rbr() as part of
		 * RBR event processing.
		 */
		if (PTR_ERR(mbuf) == -EEXIST)
			return 0;
		s_vpr_e(inst->sid, "%s: failed to get vidc-buf\n", __func__);
		return -EINVAL;
	}
	if (!kref_get_mbuf(inst, mbuf)) {
		s_vpr_e(inst->sid, "%s: mbuf not found\n", __func__);
		return -EINVAL;
	}
	rc = msm_comm_qbuf(inst, mbuf);
	if (rc)
		s_vpr_e(inst->sid, "%s: failed qbuf\n", __func__);
	kref_put_mbuf(mbuf);

	return rc;
}

static int msm_vidc_queue_buf_decode_batch(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb2)
{
	int rc;
	struct msm_vidc_buffer *mbuf;

	if (!inst || !vb2) {
		d_vpr_e("%s: invalid params %pK, %pK\n",
			__func__, inst, vb2);
		return -EINVAL;
	}

	mbuf = msm_comm_get_vidc_buffer(inst, vb2);
	if (IS_ERR_OR_NULL(mbuf)) {
		s_vpr_e(inst->sid, "%s: failed to get vidc-buf\n", __func__);
		return -EINVAL;
	}
	if (!kref_get_mbuf(inst, mbuf)) {
		s_vpr_e(inst->sid, "%s: mbuf not found\n", __func__);
		return -EINVAL;
	}
	/*
	 * If this buffer has RBR_EPNDING then it will not be queued
	 * but it may trigger full batch queuing in below function.
	 */
	rc = msm_comm_qbuf_decode_batch(inst, mbuf);
	if (rc)
		s_vpr_e(inst->sid, "%s: failed qbuf\n", __func__);
	kref_put_mbuf(mbuf);

	return rc;
}

static int msm_vidc_queue_buf_batch(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb2)
{
	int rc;

	if (!inst || !vb2) {
		d_vpr_e("%s: invalid params %pK, %pK\n",
			__func__, inst, vb2);
		return -EINVAL;
	}

	if (inst->session_type == MSM_VIDC_DECODER &&
			vb2->type == OUTPUT_MPLANE)
		rc = msm_vidc_queue_buf_decode_batch(inst, vb2);
	else
		rc = msm_vidc_queue_buf(inst, vb2);

	return rc;
}

static void msm_vidc_buf_queue(struct vb2_buffer *vb2)
{
	int rc = 0;
	struct msm_vidc_inst *inst = NULL;

	inst = vb2_get_drv_priv(vb2->vb2_queue);
	if (!inst) {
		d_vpr_e("%s: invalid inst\n", __func__);
		return;
	}

	if (inst->batch.enable)
		rc = msm_vidc_queue_buf_batch(inst, vb2);
	else
		rc = msm_vidc_queue_buf(inst, vb2);

	if (rc) {
		print_vb2_buffer("failed vb2-qbuf", inst, vb2);
		msm_comm_generate_session_error(inst);
	}
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

	if (type == OUTPUT_MPLANE) {
		q = &inst->bufq[OUTPUT_PORT].vb2_bufq;
	} else if (type == INPUT_MPLANE) {
		q = &inst->bufq[INPUT_PORT].vb2_bufq;
	} else {
		s_vpr_e(inst->sid, "buf_type = %d not recognised\n", type);
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
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	v4l2_fh_init(&vidc_inst->event_handler, pvdev);
	v4l2_fh_add(&vidc_inst->event_handler);

	return 0;
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

int msm_vidc_private(void *vidc_inst, unsigned int cmd,
		struct msm_vidc_arg *arg)
{
	int rc = 0;
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)vidc_inst;

	if (!inst || !arg) {
		d_vpr_e("%s: invalid args\n", __func__);
		return -EINVAL;
	}
	if (cmd != VIDIOC_VIDEO_CMD) {
		s_vpr_e(inst->sid,
			"%s: invalid private cmd %#x\n", __func__, cmd);
		return -ENOIOCTLCMD;
	}

	if (inst->session_type == MSM_VIDC_CVP) {
		rc = msm_vidc_cvp(inst, arg);
	} else {
		s_vpr_e(inst->sid,
			"%s: private cmd %#x not supported for session_type %d\n",
			__func__, cmd, inst->session_type);
		rc = -EINVAL;
	}

	return rc;
}
EXPORT_SYMBOL(msm_vidc_private);

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

	int rc = 0;
	struct msm_vidc_inst *inst;
	const char *ctrl_name = NULL;

	if (!ctrl) {
		d_vpr_e("%s: invalid parameters for ctrl\n", __func__);
		return -EINVAL;
	}

	inst = container_of(ctrl->handler,
		struct msm_vidc_inst, ctrl_handler);
	if (!inst) {
		d_vpr_e("%s: invalid parameters for inst\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_try_set_ctrl(inst, ctrl);
	if (rc) {
		s_vpr_e(inst->sid, "Failed setting %x\n", ctrl->id);
		ctrl_name = v4l2_ctrl_get_name(ctrl->id);
		s_vpr_e(inst->sid, "Failed setting control: Inst = %pK (%s)\n",
			inst, ctrl_name ? ctrl_name : "Invalid ctrl");
	}

	return rc;
}

static int try_get_ctrl_for_instance(struct msm_vidc_inst *inst,
	struct v4l2_ctrl *ctrl)
{
	int rc = 0;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		ctrl->val = msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_H264_PROFILE,
			inst->profile, inst->sid);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		ctrl->val = msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
			inst->profile, inst->sid);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		ctrl->val = msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_H264_LEVEL,
			inst->level, inst->sid);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		ctrl->val = msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
			inst->level, inst->sid);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		ctrl->val = msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
			inst->level, inst->sid);
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		ctrl->val = inst->fmts[OUTPUT_PORT].count_min_host;
		s_vpr_h(inst->sid, "g_min: hal_buffer %d min buffers %d\n",
			HAL_BUFFER_OUTPUT, ctrl->val);
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
		ctrl->val = inst->fmts[INPUT_PORT].count_min_host;
		s_vpr_h(inst->sid, "g_min: hal_buffer %d min buffers %d\n",
			HAL_BUFFER_INPUT, ctrl->val);
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_EXTRADATA:
		ctrl->val = inst->prop.extradata_ctrls;
		break;
	case V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE:
	{
		uint32_t vpu_ver;

		if (!inst->core || !inst->core->platform_data)
			return -EINVAL;
		vpu_ver = inst->core->platform_data->vpu_ver;
		ctrl->val = (vpu_ver == VPU_VERSION_IRIS1 ||
				vpu_ver == VPU_VERSION_IRIS2) ?
				V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_2BYTE :
				V4L2_CID_MPEG_VIDC_VIDEO_ROI_TYPE_2BIT;
		break;
	}
	default:
		break;
	}

	return rc;
}

static const struct v4l2_ctrl_ops msm_vidc_ctrl_ops = {

	.s_ctrl = msm_vidc_op_s_ctrl,
};

static struct msm_vidc_inst_smem_ops  msm_vidc_smem_ops = {
	.smem_map_dma_buf = msm_smem_map_dma_buf,
	.smem_unmap_dma_buf = msm_smem_unmap_dma_buf,
};

void *msm_vidc_open(int core_id, int session_type)
{
	struct msm_vidc_inst *inst = NULL;
	struct msm_vidc_core *core = NULL;
	int rc = 0;
	int i = 0;

	if (core_id >= MSM_VIDC_CORES_MAX ||
			session_type >= MSM_VIDC_MAX_DEVICES) {
		d_vpr_e("Invalid input, core_id = %d, session = %d\n",
			core_id, session_type);
		goto err_invalid_core;
	}
	core = get_vidc_core(core_id);
	if (!core) {
		d_vpr_e("Failed to find core for core_id = %d\n", core_id);
		goto err_invalid_core;
	}

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst) {
		d_vpr_e("Failed to allocate memory\n");
		rc = -ENOMEM;
		goto err_invalid_core;
	}
	mutex_lock(&core->lock);
	rc = get_sid(&inst->sid, session_type);
	mutex_unlock(&core->lock);
	if (rc) {
		d_vpr_e("Total instances count reached to max value\n");
		goto err_invalid_sid;
	}

	pr_info(VIDC_DBG_TAG "Opening video instance: %pK, %d\n",
		"high", inst->sid, get_codec_name(inst->sid),
		inst, session_type);
	mutex_init(&inst->sync_lock);
	mutex_init(&inst->bufq[OUTPUT_PORT].lock);
	mutex_init(&inst->bufq[INPUT_PORT].lock);
	mutex_init(&inst->lock);
	mutex_init(&inst->flush_lock);

	INIT_MSM_VIDC_LIST(&inst->scratchbufs);
	INIT_MSM_VIDC_LIST(&inst->input_crs);
	INIT_MSM_VIDC_LIST(&inst->persistbufs);
	INIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	INIT_MSM_VIDC_LIST(&inst->outputbufs);
	INIT_MSM_VIDC_LIST(&inst->registeredbufs);
	INIT_MSM_VIDC_LIST(&inst->cvpbufs);
	INIT_MSM_VIDC_LIST(&inst->refbufs);
	INIT_MSM_VIDC_LIST(&inst->eosbufs);
	INIT_MSM_VIDC_LIST(&inst->client_data);
	INIT_MSM_VIDC_LIST(&inst->etb_data);
	INIT_MSM_VIDC_LIST(&inst->fbd_data);
	INIT_MSM_VIDC_LIST(&inst->window_data);

	INIT_DELAYED_WORK(&inst->batch_work, msm_vidc_batch_handler);
	kref_init(&inst->kref);

	inst->session_type = session_type;
	inst->state = MSM_VIDC_CORE_UNINIT_DONE;
	inst->core = core;
	inst->clk_data.core_id = VIDC_CORE_ID_DEFAULT;
	inst->clk_data.dpb_fourcc = V4L2_PIX_FMT_NV12_UBWC;
	inst->clk_data.opb_fourcc = V4L2_PIX_FMT_NV12_UBWC;
	inst->bit_depth = MSM_VIDC_BIT_DEPTH_8;
	inst->pic_struct = MSM_VIDC_PIC_STRUCT_PROGRESSIVE;
	inst->colour_space = MSM_VIDC_BT601_6_525;
	inst->smem_ops = &msm_vidc_smem_ops;
	inst->rc_type = V4L2_MPEG_VIDEO_BITRATE_MODE_VBR;
	inst->dpb_extra_binfo = NULL;
	inst->all_intra = false;
	inst->max_filled_len = 0;
	inst->entropy_mode = HFI_H264_ENTROPY_CABAC;
	inst->full_range = COLOR_RANGE_UNSPECIFIED;

	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}

	if (session_type == MSM_VIDC_DECODER) {
		msm_vdec_inst_init(inst);
		rc = msm_vdec_ctrl_init(inst, &msm_vidc_ctrl_ops);
	} else if (session_type == MSM_VIDC_ENCODER) {
		msm_venc_inst_init(inst);
		rc = msm_venc_ctrl_init(inst, &msm_vidc_ctrl_ops);
	} else if (session_type == MSM_VIDC_CVP) {
		msm_cvp_inst_init(inst);
		rc = msm_cvp_ctrl_init(inst, &msm_vidc_ctrl_ops);
	}
	if (rc) {
		s_vpr_e(inst->sid, "Failed control initialization\n");
		goto fail_bufq_capture;
	}

	rc = vb2_bufq_init(inst, OUTPUT_MPLANE, session_type);
	if (rc) {
		s_vpr_e(inst->sid,
			"Failed to initialize vb2 queue on capture port\n");
		goto fail_bufq_capture;
	}
	rc = vb2_bufq_init(inst, INPUT_MPLANE, session_type);
	if (rc) {
		s_vpr_e(inst->sid,
			"Failed to initialize vb2 queue on capture port\n");
		goto fail_bufq_output;
	}

	setup_event_queue(inst, &core->vdev[session_type].vdev);

	mutex_lock(&core->lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->lock);

	rc = msm_comm_try_state(inst, MSM_VIDC_CORE_INIT_DONE);
	if (rc) {
		s_vpr_e(inst->sid,
			"Failed to move video instance to init state\n");
		goto fail_init;
	}

	if (msm_comm_check_for_inst_overload(core)) {
		s_vpr_e(inst->sid,
			"Instance count reached Max limit, rejecting session");
		goto fail_init;
	}

	msm_comm_scale_clocks_and_bus(inst, 1);

	inst->debugfs_root =
		msm_vidc_debugfs_init_inst(inst, core->debugfs_root);

	if (inst->session_type == MSM_VIDC_CVP) {
		rc = msm_comm_try_state(inst, MSM_VIDC_OPEN_DONE);
		if (rc) {
			s_vpr_e(inst->sid,
				"Failed to move video instance to open done state\n");
			goto fail_init;
		}
	}

	return inst;
fail_init:
	mutex_lock(&core->lock);
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);
	vb2_queue_release(&inst->bufq[INPUT_PORT].vb2_bufq);
fail_bufq_output:
	vb2_queue_release(&inst->bufq[OUTPUT_PORT].vb2_bufq);
fail_bufq_capture:
	msm_comm_ctrl_deinit(inst);
	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->bufq[OUTPUT_PORT].lock);
	mutex_destroy(&inst->bufq[INPUT_PORT].lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->flush_lock);

	DEINIT_MSM_VIDC_LIST(&inst->scratchbufs);
	DEINIT_MSM_VIDC_LIST(&inst->persistbufs);
	DEINIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	DEINIT_MSM_VIDC_LIST(&inst->outputbufs);
	DEINIT_MSM_VIDC_LIST(&inst->cvpbufs);
	DEINIT_MSM_VIDC_LIST(&inst->registeredbufs);
	DEINIT_MSM_VIDC_LIST(&inst->eosbufs);
	DEINIT_MSM_VIDC_LIST(&inst->input_crs);
	DEINIT_MSM_VIDC_LIST(&inst->client_data);
	DEINIT_MSM_VIDC_LIST(&inst->etb_data);
	DEINIT_MSM_VIDC_LIST(&inst->fbd_data);
	DEINIT_MSM_VIDC_LIST(&inst->window_data);

err_invalid_sid:
	put_sid(inst->sid);
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
	struct list_head *ptr, *next;
	enum vidc_ports ports[] = {INPUT_PORT, OUTPUT_PORT};
	int c = 0;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return;
	}

	for (c = 0; c < ARRAY_SIZE(ports); ++c) {
		enum vidc_ports port = ports[c];

		mutex_lock(&inst->bufq[port].lock);
		list_for_each_safe(ptr, next,
				&inst->bufq[port].vb2_bufq.queued_list) {
			struct vb2_buffer *vb = container_of(ptr,
					struct vb2_buffer, queued_entry);
			if (vb->state == VB2_BUF_STATE_ACTIVE) {
				vb->planes[0].bytesused = 0;
				print_vb2_buffer("undequeud vb2", inst, vb);
				vb2_buffer_done(vb, VB2_BUF_STATE_ERROR);
			}
		}
		mutex_unlock(&inst->bufq[port].lock);
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

	cancel_batch_work(inst);

	msm_comm_free_input_cr_table(inst);

	if (msm_comm_release_scratch_buffers(inst, false))
		s_vpr_e(inst->sid, "Failed to release scratch buffers\n");

	if (msm_comm_release_recon_buffers(inst))
		s_vpr_e(inst->sid, "Failed to release recon buffers\n");

	if (msm_comm_release_persist_buffers(inst))
		s_vpr_e(inst->sid, "Failed to release persist buffers\n");

	if (msm_comm_release_input_tag(inst))
		s_vpr_e(inst->sid, "Failed to release input_tag buffers\n");

	msm_comm_release_client_data(inst, true);

	msm_comm_release_window_data(inst);

	msm_comm_release_eos_buffers(inst);

	if (msm_comm_release_dpb_only_buffers(inst, true))
		s_vpr_e(inst->sid, "Failed to release output buffers\n");

	if (inst->extradata_handle)
		msm_comm_smem_free(inst, inst->extradata_handle);

	mutex_lock(&inst->pending_getpropq.lock);
	if (!list_empty(&inst->pending_getpropq.list)) {
		s_vpr_e(inst->sid, "pending_getpropq not empty\n");
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
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	core = inst->core;

	for (i = 0; i < MAX_PORT_NUM; i++)
		vb2_queue_release(&inst->bufq[i].vb2_bufq);

	mutex_lock(&core->lock);
	/* inst->list lives in core->instances */
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	msm_comm_ctrl_deinit(inst);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);

	DEINIT_MSM_VIDC_LIST(&inst->scratchbufs);
	DEINIT_MSM_VIDC_LIST(&inst->persistbufs);
	DEINIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	DEINIT_MSM_VIDC_LIST(&inst->outputbufs);
	DEINIT_MSM_VIDC_LIST(&inst->cvpbufs);
	DEINIT_MSM_VIDC_LIST(&inst->registeredbufs);
	DEINIT_MSM_VIDC_LIST(&inst->eosbufs);
	DEINIT_MSM_VIDC_LIST(&inst->input_crs);
	DEINIT_MSM_VIDC_LIST(&inst->client_data);
	DEINIT_MSM_VIDC_LIST(&inst->etb_data);
	DEINIT_MSM_VIDC_LIST(&inst->fbd_data);
	DEINIT_MSM_VIDC_LIST(&inst->window_data);

	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->bufq[OUTPUT_PORT].lock);
	mutex_destroy(&inst->bufq[INPUT_PORT].lock);
	mutex_destroy(&inst->lock);
	mutex_destroy(&inst->flush_lock);

	msm_vidc_debugfs_deinit_inst(inst);

	pr_info(VIDC_DBG_TAG "Closed video instance: %pK\n",
			"high", inst->sid, get_codec_name(inst->sid),
			inst);
	put_sid(inst->sid);
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
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/*
	 * Make sure that HW stop working on these buffers that
	 * we are going to free.
	 */
	rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
	if (rc)
		s_vpr_e(inst->sid, "Failed: move to rel resource done state\n");

	/*
	 * deinit instance after REL_RES_DONE to ensure hardware
	 * released all buffers.
	 */
	if (inst->session_type == MSM_VIDC_CVP)
		msm_cvp_inst_deinit(inst);

	msm_vidc_cleanup_instance(inst);

	rc = msm_comm_try_state(inst, MSM_VIDC_CORE_UNINIT);
	if (rc) {
		s_vpr_e(inst->sid,
			"Failed to move inst %pK to uninit state\n", inst);
		rc = msm_comm_force_cleanup(inst);
	}

	msm_comm_session_clean(inst);

	kref_put(&inst->kref, close_helper);
	return 0;
}
EXPORT_SYMBOL(msm_vidc_close);

int msm_vidc_suspend(int core_id)
{
	return msm_comm_suspend(core_id);
}
EXPORT_SYMBOL(msm_vidc_suspend);

