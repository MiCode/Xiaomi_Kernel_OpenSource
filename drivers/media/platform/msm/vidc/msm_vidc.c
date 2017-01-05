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
	int rc = 0;

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

struct buffer_info *get_registered_buf(struct msm_vidc_inst *inst,
		struct v4l2_buffer *b, int idx, int *plane)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	int i;
	int fd = b->m.planes[idx].reserved[0];
	u32 buff_off = b->m.planes[idx].reserved[1];
	u32 size = b->m.planes[idx].length;
	ion_phys_addr_t device_addr = b->m.planes[idx].m.userptr;

	if (fd < 0 || !plane) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}

	WARN(!mutex_is_locked(&inst->registeredbufs.lock),
		"Registered buf lock is not acquired for %s", __func__);

	*plane = 0;
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		for (i = 0; i < min(temp->num_planes, VIDEO_MAX_PLANES); i++) {
			bool ion_hndl_matches = temp->handle[i] ?
				msm_smem_compare_buffers(inst->mem_client, fd,
				temp->handle[i]->smem_priv) : false;
			bool device_addr_matches = device_addr ==
						temp->device_addr[i];
			bool contains_within = CONTAINS(temp->buff_off[i],
					temp->size[i], buff_off) ||
				CONTAINS(buff_off, size, temp->buff_off[i]);
			bool overlaps = OVERLAPS(buff_off, size,
					temp->buff_off[i], temp->size[i]);

			if (!temp->inactive &&
				(ion_hndl_matches || device_addr_matches) &&
				(contains_within || overlaps)) {
				dprintk(VIDC_DBG,
						"This memory region is already mapped\n");
				ret = temp;
				*plane = i;
				break;
			}
		}
		if (ret)
			break;
	}

err_invalid_input:
	return ret;
}

static struct msm_smem *get_same_fd_buffer(struct msm_vidc_inst *inst, int fd)
{
	struct buffer_info *temp;
	struct msm_smem *same_fd_handle = NULL;
	int i;

	if (!fd)
		return NULL;

	if (!inst || fd < 0) {
		dprintk(VIDC_ERR, "%s: Invalid input\n", __func__);
		goto err_invalid_input;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		for (i = 0; i < min(temp->num_planes, VIDEO_MAX_PLANES); i++) {
			bool ion_hndl_matches = temp->handle[i] ?
				msm_smem_compare_buffers(inst->mem_client, fd,
				temp->handle[i]->smem_priv) : false;
			if (ion_hndl_matches && temp->mapped[i])  {
				temp->same_fd_ref[i]++;
				dprintk(VIDC_INFO,
				"Found same fd buffer\n");
				same_fd_handle = temp->handle[i];
				break;
			}
		}
		if (same_fd_handle)
			break;
	}
	mutex_unlock(&inst->registeredbufs.lock);

err_invalid_input:
	return same_fd_handle;
}

struct buffer_info *device_to_uvaddr(struct msm_vidc_list *buf_list,
				ion_phys_addr_t device_addr)
{
	struct buffer_info *temp = NULL;
	bool found = false;
	int i;

	if (!buf_list || !device_addr) {
		dprintk(VIDC_ERR,
			"Invalid input- device_addr: %pa buf_list: %pK\n",
			&device_addr, buf_list);
		goto err_invalid_input;
	}

	mutex_lock(&buf_list->lock);
	list_for_each_entry(temp, &buf_list->list, list) {
		for (i = 0; i < min(temp->num_planes, VIDEO_MAX_PLANES); i++) {
			if (!temp->inactive &&
				temp->device_addr[i] == device_addr)  {
				dprintk(VIDC_INFO,
				"Found same fd buffer\n");
				found = true;
				break;
			}
		}

		if (found)
			break;
	}
	mutex_unlock(&buf_list->lock);

err_invalid_input:
	return temp;
}

static inline void populate_buf_info(struct buffer_info *binfo,
			struct v4l2_buffer *b, u32 i)
{
	if (i >= VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "%s: Invalid input\n", __func__);
		return;
	}
	binfo->type = b->type;
	binfo->fd[i] = b->m.planes[i].reserved[0];
	binfo->buff_off[i] = b->m.planes[i].reserved[1];
	binfo->size[i] = b->m.planes[i].length;
	binfo->uvaddr[i] = b->m.planes[i].m.userptr;
	binfo->num_planes = b->length;
	binfo->memory = b->memory;
	binfo->v4l2_index = b->index;
	binfo->timestamp.tv_sec = b->timestamp.tv_sec;
	binfo->timestamp.tv_usec = b->timestamp.tv_usec;
	dprintk(VIDC_DBG, "%s: fd[%d] = %d b->index = %d",
			__func__, i, binfo->fd[i], b->index);
}

static inline void repopulate_v4l2_buffer(struct v4l2_buffer *b,
					struct buffer_info *binfo)
{
	int i = 0;

	b->type = binfo->type;
	b->length = binfo->num_planes;
	b->memory = binfo->memory;
	b->index = binfo->v4l2_index;
	b->timestamp.tv_sec = binfo->timestamp.tv_sec;
	b->timestamp.tv_usec = binfo->timestamp.tv_usec;
	binfo->dequeued = false;
	for (i = 0; i < binfo->num_planes; ++i) {
		b->m.planes[i].reserved[0] = binfo->fd[i];
		b->m.planes[i].reserved[1] = binfo->buff_off[i];
		b->m.planes[i].length = binfo->size[i];
		b->m.planes[i].m.userptr = binfo->device_addr[i];
		dprintk(VIDC_DBG, "%s %d %d %d %pa\n", __func__, binfo->fd[i],
				binfo->buff_off[i], binfo->size[i],
				&binfo->device_addr[i]);
	}
}

static struct msm_smem *map_buffer(struct msm_vidc_inst *inst,
		struct v4l2_plane *p, enum hal_buffer buffer_type)
{
	struct msm_smem *handle = NULL;

	handle = msm_comm_smem_user_to_kernel(inst,
				p->reserved[0],
				p->reserved[1],
				buffer_type);
	if (!handle) {
		dprintk(VIDC_ERR,
			"%s: Failed to get device buffer address\n", __func__);
		return NULL;
	}
	return handle;
}

static inline enum hal_buffer get_hal_buffer_type(
		struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	if (b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return HAL_BUFFER_INPUT;
	else if (b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return HAL_BUFFER_OUTPUT;
	else
		return -EINVAL;
}

static inline bool is_dynamic_buffer_mode(struct v4l2_buffer *b,
				struct msm_vidc_inst *inst)
{
	enum vidc_ports port = b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
		OUTPUT_PORT : CAPTURE_PORT;
	return inst->buffer_mode_set[port] == HAL_BUFFER_MODE_DYNAMIC;
}


static inline void save_v4l2_buffer(struct v4l2_buffer *b,
						struct buffer_info *binfo)
{
	int i = 0;

	for (i = 0; i < b->length; ++i) {
		if (EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].length) {
			continue;
		}
		populate_buf_info(binfo, b, i);
	}
}

int map_and_register_buf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buffer_info *binfo = NULL;
	struct buffer_info *temp = NULL, *iterator = NULL;
	int plane = 0;
	int i = 0, rc = 0;
	struct msm_smem *same_fd_handle = NULL;

	if (!b || !inst) {
		dprintk(VIDC_ERR, "%s: invalid input\n", __func__);
		return -EINVAL;
	}

	binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
	if (!binfo) {
		dprintk(VIDC_ERR, "Out of memory\n");
		rc = -ENOMEM;
		goto exit;
	}
	if (b->length > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "Num planes exceeds max: %d, %d\n",
			b->length, VIDEO_MAX_PLANES);
		rc = -EINVAL;
		goto exit;
	}

	dprintk(VIDC_DBG, "[MAP] Create binfo = %pK fd = %d type = %d\n",
			binfo, b->m.planes[0].reserved[0], b->type);

	for (i = 0; i < b->length; ++i) {
		rc = 0;
		if (EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].length) {
			continue;
		}
		mutex_lock(&inst->registeredbufs.lock);
		temp = get_registered_buf(inst, b, i, &plane);
		if (temp && !is_dynamic_buffer_mode(b, inst)) {
			dprintk(VIDC_DBG,
				"This memory region has already been prepared\n");
			rc = 0;
			mutex_unlock(&inst->registeredbufs.lock);
			goto exit;
		}

		if (temp && is_dynamic_buffer_mode(b, inst) && !i) {
			/*
			 * Buffer is already present in registered list
			 * increment ref_count, populate new values of v4l2
			 * buffer in existing buffer_info struct.
			 *
			 * We will use the saved buffer info and queue it when
			 * we receive RELEASE_BUFFER_REFERENCE EVENT from f/w.
			 */
			dprintk(VIDC_DBG, "[MAP] Buffer already prepared\n");
			temp->inactive = false;
			list_for_each_entry(iterator,
				&inst->registeredbufs.list, list) {
				if (iterator == temp) {
					rc = buf_ref_get(inst, temp);
					save_v4l2_buffer(b, temp);
					break;
				}
			}
		}
		mutex_unlock(&inst->registeredbufs.lock);
		/*
		 * rc == 1,
		 * buffer is mapped, fw has released all reference, so skip
		 * mapping and queue it immediately.
		 *
		 * rc == 2,
		 * buffer is mapped and fw is holding a reference, hold it in
		 * the driver and queue it later when fw has released
		 */
		if (rc == 1) {
			rc = 0;
			goto exit;
		} else if (rc >= 2) {
			rc = -EEXIST;
			goto exit;
		}

		same_fd_handle = get_same_fd_buffer(
				inst, b->m.planes[i].reserved[0]);

		populate_buf_info(binfo, b, i);
		if (same_fd_handle) {
			binfo->device_addr[i] =
			same_fd_handle->device_addr + binfo->buff_off[i];
			b->m.planes[i].m.userptr = binfo->device_addr[i];
			binfo->mapped[i] = false;
			binfo->handle[i] = same_fd_handle;
		} else {
			binfo->handle[i] = map_buffer(inst, &b->m.planes[i],
					get_hal_buffer_type(inst, b));
			if (!binfo->handle[i]) {
				rc = -EINVAL;
				goto exit;
			}

			binfo->mapped[i] = true;
			binfo->device_addr[i] = binfo->handle[i]->device_addr +
				binfo->buff_off[i];
			b->m.planes[i].m.userptr = binfo->device_addr[i];
		}

		/* We maintain one ref count for all planes*/
		if (!i && is_dynamic_buffer_mode(b, inst)) {
			rc = buf_ref_get(inst, binfo);
			if (rc < 0)
				goto exit;
		}
		dprintk(VIDC_DBG,
			"%s: [MAP] binfo = %pK, handle[%d] = %pK, device_addr = %pa, fd = %d, offset = %d, mapped = %d\n",
			__func__, binfo, i, binfo->handle[i],
			&binfo->device_addr[i], binfo->fd[i],
			binfo->buff_off[i], binfo->mapped[i]);
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_add_tail(&binfo->list, &inst->registeredbufs.list);
	mutex_unlock(&inst->registeredbufs.lock);
	return 0;

exit:
	kfree(binfo);
	return rc;
}
int unmap_and_deregister_buf(struct msm_vidc_inst *inst,
			struct buffer_info *binfo)
{
	int i = 0;
	struct buffer_info *temp = NULL;
	bool found = false, keep_node = false;

	if (!inst || !binfo) {
		dprintk(VIDC_ERR, "%s invalid param: %pK %pK\n",
			__func__, inst, binfo);
		return -EINVAL;
	}

	WARN(!mutex_is_locked(&inst->registeredbufs.lock),
		"Registered buf lock is not acquired for %s", __func__);

	/*
	 * Make sure the buffer to be unmapped and deleted
	 * from the registered list is present in the list.
	 */
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		if (temp == binfo) {
			found = true;
			break;
		}
	}

	/*
	 * Free the buffer info only if
	 * - buffer info has not been deleted from registered list
	 * - vidc client has called dqbuf on the buffer
	 * - no references are held on the buffer
	 */
	if (!found || !temp || !temp->pending_deletion || !temp->dequeued)
		goto exit;

	for (i = 0; i < temp->num_planes; i++) {
		dprintk(VIDC_DBG,
			"%s: [UNMAP] binfo = %pK, handle[%d] = %pK, device_addr = %pa, fd = %d, offset = %d, mapped = %d\n",
			__func__, temp, i, temp->handle[i],
			&temp->device_addr[i], temp->fd[i],
			temp->buff_off[i], temp->mapped[i]);
		/*
		 * Unmap the handle only if the buffer has been mapped and no
		 * other buffer has a reference to this buffer.
		 * In case of buffers with same fd, we will map the buffer only
		 * once and subsequent buffers will refer to the mapped buffer's
		 * device address.
		 * For buffers which share the same fd, do not unmap and keep
		 * the buffer info in registered list.
		 */
		if (temp->handle[i] && temp->mapped[i] &&
			!temp->same_fd_ref[i]) {
			msm_comm_smem_free(inst,
				temp->handle[i]);
		}

		if (temp->same_fd_ref[i])
			keep_node = true;
		else {
			temp->fd[i] = 0;
			temp->handle[i] = 0;
			temp->device_addr[i] = 0;
			temp->uvaddr[i] = 0;
		}
	}
	if (!keep_node) {
		dprintk(VIDC_DBG, "[UNMAP] AND-FREED binfo: %pK\n", temp);
		list_del(&temp->list);
		kfree(temp);
	} else {
		temp->inactive = true;
		dprintk(VIDC_DBG, "[UNMAP] NOT-FREED binfo: %pK\n", temp);
	}
exit:
	return 0;
}


int qbuf_dynamic_buf(struct msm_vidc_inst *inst,
			struct buffer_info *binfo)
{
	struct v4l2_buffer b = {0};
	struct v4l2_plane plane[VIDEO_MAX_PLANES] = { {0} };
	struct buf_queue *q = NULL;
	int rc = 0;

	if (!binfo) {
		dprintk(VIDC_ERR, "%s invalid param: %pK\n", __func__, binfo);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "%s fd[0] = %d\n", __func__, binfo->fd[0]);

	b.m.planes = plane;
	repopulate_v4l2_buffer(&b, binfo);

	q = msm_comm_get_vb2q(inst, (&b)->type);
	if (!q) {
		dprintk(VIDC_ERR, "Failed to find buffer queue for type = %d\n"
				, (&b)->type);
		return -EINVAL;
	}

	mutex_lock(&q->lock);
	rc = vb2_qbuf(&q->vb2_bufq, &b);
	mutex_unlock(&q->lock);

	if (rc)
		dprintk(VIDC_ERR, "Failed to qbuf, %d\n", rc);
	return rc;
}

int output_buffer_cache_invalidate(struct msm_vidc_inst *inst,
				struct buffer_info *binfo)
{
	int i = 0;
	int rc = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid inst: %pK\n", __func__, inst);
		return -EINVAL;
	}

	if (!binfo) {
		dprintk(VIDC_ERR, "%s: invalid buffer info: %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	for (i = 0; i < binfo->num_planes; i++) {
		if (binfo->handle[i]) {
			struct msm_smem smem = *binfo->handle[i];

			smem.offset = (unsigned int)(binfo->buff_off[i]);
			smem.size   = binfo->size[i];
			rc = msm_comm_smem_cache_operations(inst,
				&smem, SMEM_CACHE_INVALIDATE);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s: Failed to clean caches: %d\n",
					__func__, rc);
				return -EINVAL;
			}
		} else
			dprintk(VIDC_DBG, "%s: NULL handle for plane %d\n",
					__func__, i);
	}
	return 0;
}

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

int msm_vidc_release_buffer(void *instance, int buffer_type,
		unsigned int buffer_index)
{
	struct msm_vidc_inst *inst = instance;
	struct buffer_info *bi, *dummy;
	int i, rc = 0;
	int found_buf = 0;
	struct vb2_buf_entry *temp, *next;

	if (!inst)
		return -EINVAL;

	if (!inst->in_reconfig &&
		inst->state > MSM_VIDC_LOAD_RESOURCES &&
		inst->state < MSM_VIDC_RELEASE_RESOURCES_DONE) {
		rc = msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);
		if (rc) {
			dprintk(VIDC_ERR,
					"Failed to move inst: %pK to release res done\n",
					inst);
		}
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(bi, dummy, &inst->registeredbufs.list, list) {
		if (bi->type == buffer_type && bi->v4l2_index == buffer_index) {
			found_buf = 1;
			list_del(&bi->list);
			for (i = 0; i < bi->num_planes; i++) {
				if (bi->handle[i] && bi->mapped[i]) {
					dprintk(VIDC_DBG,
						"%s: [UNMAP] binfo = %pK, handle[%d] = %pK, device_addr = %pa, fd = %d, offset = %d, mapped = %d\n",
						__func__, bi, i, bi->handle[i],
						&bi->device_addr[i], bi->fd[i],
						bi->buff_off[i], bi->mapped[i]);
					msm_comm_smem_free(inst,
							bi->handle[i]);
					found_buf = 2;
				}
			}
			kfree(bi);
			break;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);

	switch (found_buf) {
	case 0:
		dprintk(VIDC_DBG,
			"%s: No buffer(type: %d) found for index %d\n",
			__func__, buffer_type, buffer_index);
		break;
	case 1:
		dprintk(VIDC_WARN,
			"%s: Buffer(type: %d) found for index %d.",
			__func__, buffer_type, buffer_index);
		dprintk(VIDC_WARN, "zero planes mapped.\n");
		break;
	case 2:
		dprintk(VIDC_DBG,
			"%s: Released buffer(type: %d) for index %d\n",
			__func__, buffer_type, buffer_index);
		break;
	default:
		break;
	}

	mutex_lock(&inst->pendingq.lock);
	list_for_each_entry_safe(temp, next, &inst->pendingq.list, list) {
		if (temp->vb->type == buffer_type) {
			list_del(&temp->list);
			kfree(temp);
		}
	}
	mutex_unlock(&inst->pendingq.lock);

	return rc;
}
EXPORT_SYMBOL(msm_vidc_release_buffer);

int msm_vidc_qbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	struct buffer_info *binfo;
	int plane = 0;
	int rc = 0;
	int i;
	struct buf_queue *q = NULL;

	if (!inst || !inst->core || !b || !valid_v4l2_buffer(b, inst))
		return -EINVAL;

	if (inst->state == MSM_VIDC_CORE_INVALID ||
		inst->core->state == VIDC_CORE_INVALID)
		return -EINVAL;

	rc = map_and_register_buf(inst, b);
	if (rc == -EEXIST) {
		if (atomic_read(&inst->in_flush) &&
			is_dynamic_buffer_mode(b, inst)) {
			dprintk(VIDC_ERR,
				"Flush in progress, do not hold any buffers in driver\n");
			msm_comm_flush_dynamic_buffers(inst);
		}
		return 0;
	}
	if (rc)
		return rc;

	for (i = 0; i < b->length; ++i) {
		if (EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].length) {
			b->m.planes[i].m.userptr = 0;
			continue;
		}
		mutex_lock(&inst->registeredbufs.lock);
		binfo = get_registered_buf(inst, b, i, &plane);
		mutex_unlock(&inst->registeredbufs.lock);
		if (!binfo) {
			dprintk(VIDC_ERR,
				"This buffer is not registered: %d, %d, %d\n",
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
			goto err_invalid_buff;
		}
		b->m.planes[i].m.userptr = binfo->device_addr[i];
		dprintk(VIDC_DBG, "Queueing device address = %pa\n",
				&binfo->device_addr[i]);

		if (binfo->handle[i] &&
			(b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
			rc = msm_comm_smem_cache_operations(inst,
					binfo->handle[i], SMEM_CACHE_CLEAN);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to clean caches: %d\n", rc);
				goto err_invalid_buff;
			}
		}
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

err_invalid_buff:
	return -EINVAL;
}
EXPORT_SYMBOL(msm_vidc_qbuf);

int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	struct buffer_info *buffer_info = NULL;
	int i = 0, rc = 0;
	struct buf_queue *q = NULL;

	if (!inst || !b || !valid_v4l2_buffer(b, inst))
		return -EINVAL;

	q = msm_comm_get_vb2q(inst, b->type);
	if (!q) {
		dprintk(VIDC_ERR,
			"Failed to find buffer queue for type = %d\n", b->type);
		return -EINVAL;
	}
	mutex_lock(&q->lock);
	rc = vb2_dqbuf(&q->vb2_bufq, b, true);
	mutex_unlock(&q->lock);
	if (rc) {
		dprintk(VIDC_DBG, "Failed to dqbuf, %d\n", rc);
		return rc;
	}

	for (i = 0; i < b->length; i++) {
		if (EXTRADATA_IDX(b->length) &&
			i == EXTRADATA_IDX(b->length)) {
			continue;
		}
		buffer_info = device_to_uvaddr(&inst->registeredbufs,
			b->m.planes[i].m.userptr);

		if (!buffer_info) {
			dprintk(VIDC_ERR,
				"%s no buffer info registered for buffer addr: %#lx\n",
				__func__, b->m.planes[i].m.userptr);
			return -EINVAL;
		}

		b->m.planes[i].m.userptr = buffer_info->uvaddr[i];
		b->m.planes[i].reserved[0] = buffer_info->fd[i];
		b->m.planes[i].reserved[1] = buffer_info->buff_off[i];
	}

	if (!buffer_info) {
		dprintk(VIDC_ERR,
			"%s: error - no buffer info found in registered list\n",
			__func__);
		return -EINVAL;
	}

	rc = output_buffer_cache_invalidate(inst, buffer_info);
	if (rc)
		return rc;


	if (is_dynamic_buffer_mode(b, inst)) {
		buffer_info->dequeued = true;

		dprintk(VIDC_DBG, "[DEQUEUED]: fd[0] = %d\n",
			buffer_info->fd[0]);
		mutex_lock(&inst->registeredbufs.lock);
		rc = unmap_and_deregister_buf(inst, buffer_info);
		mutex_unlock(&inst->registeredbufs.lock);
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
	if (rc)
		dprintk(VIDC_ERR, "streamon failed on port: %d\n", i);
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
		rc = set_buffer_count(inst, bufreq->buffer_count_actual,
			*num_buffers, HAL_BUFFER_INPUT);
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
		rc = set_buffer_count(inst, bufreq->buffer_count_actual,
			*num_buffers, buffer_type);
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
			inst->state < MSM_VIDC_LOAD_RESOURCES_DONE) {
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
	struct vb2_buf_entry *temp, *next;

	hdev = inst->core->device;

	/* Check if current session is under HW capability */
	rc = msm_vidc_check_session_supported(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"This session is not supported %pK\n", inst);
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

	b.buffer_size = inst->bufq[CAPTURE_PORT].plane_sizes[0];
	rc = call_hfi_op(hdev, session_set_property,
			inst->session, HAL_PARAM_BUFFER_SIZE_MINIMUM,
			&b);

	rc = msm_comm_try_get_bufreqs(inst);

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
	if (rc) {
		mutex_lock(&inst->pendingq.lock);
		list_for_each_entry_safe(temp, next, &inst->pendingq.list,
				list) {
			vb2_buffer_done(temp->vb,
					VB2_BUF_STATE_QUEUED);
			list_del(&temp->list);
			kfree(temp);
		}
		mutex_unlock(&inst->pendingq.lock);
	}
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

stream_start_failed:
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

static void msm_vidc_buf_queue(struct vb2_buffer *vb)
{
	int rc = msm_comm_qbuf(vb2_get_drv_priv(vb->vb2_queue), vb);

	if (rc)
		dprintk(VIDC_ERR, "Failed to queue buffer: %d\n", rc);
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
	q->allow_zero_bytesused = 1;
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

static int set_actual_buffer_count(struct msm_vidc_inst *inst,
	int count, enum hal_buffer type)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_buffer_count_actual buf_count;

	hdev = inst->core->device;

	buf_count.buffer_type = type;
	buf_count.buffer_count_min_host = count;
	buf_count.buffer_count_actual = count;
	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HAL_PARAM_BUFFER_COUNT_ACTUAL,
		&buf_count);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to set actual count %d for buffer type %d\n",
			count, type);
	return rc;
}


static int msm_vidc_get_count(struct msm_vidc_inst *inst,
	struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct hal_buffer_requirements *bufreq, *newreq;
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
			bufreq->buffer_count_min_host = ctrl->val;
		} else {
			ctrl->val = bufreq->buffer_count_min_host;
		}
		rc = set_actual_buffer_count(inst,
				bufreq->buffer_count_min_host,
			HAL_BUFFER_INPUT);
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


		if (inst->in_reconfig) {
			rc = msm_comm_try_get_bufreqs(inst);
			newreq = get_buff_req_buffer(inst,
				buffer_type);
			if (!newreq) {
				dprintk(VIDC_ERR,
					"Failed to find new bufreqs = %d\n",
					buffer_type);
				return 0;
			}
			ctrl->val = newreq->buffer_count_min;
		}
		if (inst->session_type == MSM_VIDC_DECODER &&
				!inst->in_reconfig &&
			inst->state < MSM_VIDC_LOAD_RESOURCES_DONE) {
			dprintk(VIDC_DBG,
				"Clients updates Buffer count from %d to %d\n",
				bufreq->buffer_count_min_host, ctrl->val);
			bufreq->buffer_count_min_host = ctrl->val;
		}
		if (ctrl->val > bufreq->buffer_count_min_host) {
			dprintk(VIDC_DBG,
				"Buffer count Host changed from %d to %d\n",
				bufreq->buffer_count_min_host,
				ctrl->val);
			bufreq->buffer_count_min_host = ctrl->val;
		} else {
			ctrl->val = bufreq->buffer_count_min_host;
		}
		rc = set_actual_buffer_count(inst,
				bufreq->buffer_count_min_host,
			HAL_BUFFER_OUTPUT);

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
			msm_comm_try_get_bufreqs(inst);

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
		msm_comm_try_get_bufreqs(inst);
		bufreq = get_buff_req_buffer(inst, HAL_BUFFER_INPUT);
		if (!bufreq) {
			dprintk(VIDC_ERR,
				"Failed to find bufreqs for buffer type = %d\n",
					HAL_BUFFER_INPUT);
			return -EINVAL;
		}
		ctrl->val = bufreq->buffer_count_min_host;
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

	INIT_MSM_VIDC_LIST(&inst->pendingq);
	INIT_MSM_VIDC_LIST(&inst->scratchbufs);
	INIT_MSM_VIDC_LIST(&inst->freqs);
	INIT_MSM_VIDC_LIST(&inst->persistbufs);
	INIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	INIT_MSM_VIDC_LIST(&inst->outputbufs);
	INIT_MSM_VIDC_LIST(&inst->registeredbufs);

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

	DEINIT_MSM_VIDC_LIST(&inst->pendingq);
	DEINIT_MSM_VIDC_LIST(&inst->scratchbufs);
	DEINIT_MSM_VIDC_LIST(&inst->persistbufs);
	DEINIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	DEINIT_MSM_VIDC_LIST(&inst->outputbufs);
	DEINIT_MSM_VIDC_LIST(&inst->registeredbufs);

	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}
EXPORT_SYMBOL(msm_vidc_open);

static void cleanup_instance(struct msm_vidc_inst *inst)
{
	struct vb2_buf_entry *entry, *dummy;

	if (inst) {

		mutex_lock(&inst->pendingq.lock);
		list_for_each_entry_safe(entry, dummy, &inst->pendingq.list,
				list) {
			list_del(&entry->list);
			kfree(entry);
		}
		mutex_unlock(&inst->pendingq.lock);

		msm_comm_free_freq_table(inst);

		if (msm_comm_release_scratch_buffers(inst, false)) {
			dprintk(VIDC_ERR,
				"Failed to release scratch buffers\n");
		}

		if (msm_comm_release_persist_buffers(inst)) {
			dprintk(VIDC_ERR,
				"Failed to release persist buffers\n");
		}

		/*
		 * At this point all buffes should be with driver
		 * irrespective of scenario
		 */
		msm_comm_validate_output_buffers(inst);

		if (msm_comm_release_output_buffers(inst, true)) {
			dprintk(VIDC_ERR,
				"Failed to release output buffers\n");
		}

		if (inst->extradata_handle)
			msm_comm_smem_free(inst, inst->extradata_handle);

		debugfs_remove_recursive(inst->debugfs_root);

		mutex_lock(&inst->pending_getpropq.lock);
		WARN_ON(!list_empty(&inst->pending_getpropq.list));
		mutex_unlock(&inst->pending_getpropq.lock);
	}
}

int msm_vidc_destroy(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	int i = 0;

	if (!inst || !inst->core)
		return -EINVAL;

	core = inst->core;

	mutex_lock(&core->lock);
	/* inst->list lives in core->instances */
	list_del(&inst->list);
	mutex_unlock(&core->lock);

	msm_comm_ctrl_deinit(inst);

	DEINIT_MSM_VIDC_LIST(&inst->pendingq);
	DEINIT_MSM_VIDC_LIST(&inst->scratchbufs);
	DEINIT_MSM_VIDC_LIST(&inst->persistbufs);
	DEINIT_MSM_VIDC_LIST(&inst->pending_getpropq);
	DEINIT_MSM_VIDC_LIST(&inst->outputbufs);
	DEINIT_MSM_VIDC_LIST(&inst->registeredbufs);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);

	for (i = 0; i < MAX_PORT_NUM; i++)
		vb2_queue_release(&inst->bufq[i].vb2_bufq);

	mutex_destroy(&inst->sync_lock);
	mutex_destroy(&inst->bufq[CAPTURE_PORT].lock);
	mutex_destroy(&inst->bufq[OUTPUT_PORT].lock);
	mutex_destroy(&inst->lock);

	pr_info(VIDC_DBG_TAG "Closed video instance: %pK\n",
			VIDC_MSG_PRIO2STRING(VIDC_INFO), inst);
	kfree(inst);
	return 0;
}

int msm_vidc_close(void *instance)
{
	void close_helper(struct kref *kref)
	{
		struct msm_vidc_inst *inst = container_of(kref,
				struct msm_vidc_inst, kref);

		msm_vidc_destroy(inst);
	}

	struct msm_vidc_inst *inst = instance;
	struct buffer_info *bi, *dummy;
	int rc = 0;

	if (!inst || !inst->core)
		return -EINVAL;

	/*
	 * Make sure that HW stop working on these buffers that
	 * we are going to free.
	 */
	if (inst->state != MSM_VIDC_CORE_INVALID &&
		inst->core->state != VIDC_CORE_INVALID)
		rc = msm_comm_try_state(inst,
				MSM_VIDC_RELEASE_RESOURCES_DONE);

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(bi, dummy, &inst->registeredbufs.list, list) {
			int i = 0;

			list_del(&bi->list);

			for (i = 0; i < min(bi->num_planes, VIDEO_MAX_PLANES);
					i++) {
				if (bi->handle[i] && bi->mapped[i])
					msm_comm_smem_free(inst, bi->handle[i]);
			}

			kfree(bi);
		}
	mutex_unlock(&inst->registeredbufs.lock);

	cleanup_instance(inst);
	if (inst->state != MSM_VIDC_CORE_INVALID &&
		inst->core->state != VIDC_CORE_INVALID)
		rc = msm_comm_try_state(inst, MSM_VIDC_CORE_UNINIT);
	else
		rc = msm_comm_force_cleanup(inst);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move video instance to uninit state\n");

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

