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

#include <linux/sched.h>
#include <linux/slab.h>
#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vidc_debug.h"
#include "msm_vdec.h"
#include "msm_venc.h"
#include "msm_vidc_common.h"
#include "msm_smem.h"
#include <linux/delay.h>
#include "vidc_hfi_api.h"

#define MAX_EVENTS 30

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
	struct vb2_queue *outq = &inst->bufq[OUTPUT_PORT].vb2_bufq;
	struct vb2_queue *capq = &inst->bufq[CAPTURE_PORT].vb2_bufq;

	poll_wait(filp, &inst->event_handler.wait, wait);
	poll_wait(filp, &capq->done_wq, wait);
	poll_wait(filp, &outq->done_wq, wait);
	return get_poll_flags(inst);
}

/* Kernel client alternative for msm_vidc_poll */
int msm_vidc_wait(void *instance)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0;

	wait_event(inst->kernel_event_queue, (rc = get_poll_flags(inst)));
	return rc;
}

int msm_vidc_get_iommu_domain_partition(void *instance, u32 flags,
		enum v4l2_buf_type buf_type, int *domain, int *partition)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;

	return msm_comm_get_domain_partition(inst, flags, buf_type, domain,
		partition);
}

int msm_vidc_querycap(void *instance, struct v4l2_capability *cap)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !cap)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_querycap(instance, cap);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_querycap(instance, cap);
	return -EINVAL;
}
int msm_vidc_s_parm(void *instance,
		struct v4l2_streamparm *a)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !a)
		return -EINVAL;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_s_parm(instance, a);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_s_parm(instance, a);
	return -EINVAL;
}
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
int msm_vidc_g_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_g_fmt(instance, f);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_g_fmt(instance, f);
	return -EINVAL;
}
int msm_vidc_s_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_s_ctrl(instance, control);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_s_ctrl(instance, control);
	return -EINVAL;
}
int msm_vidc_g_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_g_ctrl(instance, control);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_g_ctrl(instance, control);
	return -EINVAL;
}
int msm_vidc_reqbufs(void *instance, struct v4l2_requestbuffers *b)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !b)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_reqbufs(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_reqbufs(instance, b);
	return -EINVAL;
}

struct buffer_info *get_registered_buf(struct list_head *list,
				int fd, u32 buff_off, u32 size, int *plane)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	int i;
	if (!list || fd < 0 || !plane) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}
	*plane = 0;
	list_for_each_entry(temp, list, list) {
		for (i = 0; (i < temp->num_planes)
			&& (i < VIDEO_MAX_PLANES); i++) {
			if (temp && temp->fd[i] == fd &&
					(CONTAINS(temp->buff_off[i],
					temp->size[i], buff_off)
					 || CONTAINS(buff_off,
					 size, temp->buff_off[i])
					 || OVERLAPS(buff_off, size,
					 temp->buff_off[i],
					 temp->size[i]))) {
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

struct buffer_info *get_same_fd_buffer(struct list_head *list,
		int fd, int *plane)
{
	struct buffer_info *temp;
	struct buffer_info *ret = NULL;
	int i;
	if (!list || fd < 0 || !plane) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}
	*plane = 0;
	list_for_each_entry(temp, list, list) {
		for (i = 0; (i < temp->num_planes)
			&& (i < VIDEO_MAX_PLANES); i++) {
			if (temp && temp->fd[i] == fd)  {
				dprintk(VIDC_INFO,
				"Found same fd buffer\n");
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

struct buffer_info *device_to_uvaddr(
	struct list_head *list, u32 device_addr)
{
	struct buffer_info *temp = NULL;
	int found = 0;
	int i;
	if (!list || !device_addr) {
		dprintk(VIDC_ERR, "Invalid input\n");
		goto err_invalid_input;
	}

	list_for_each_entry(temp, list, list) {
		for (i = 0; (i < temp->num_planes)
			&& (i < VIDEO_MAX_PLANES); i++) {
			if (temp && temp->device_addr[i]
					== device_addr)  {
				dprintk(VIDC_INFO,
				"Found same fd buffer\n");
				found = 1;
				break;
			}
		}
		if (found)
			break;
	}
err_invalid_input:
	return temp;
}

static inline void populate_buf_info(struct buffer_info *binfo,
			struct v4l2_buffer *b, u32 i)
{
	binfo->type = b->type;
	binfo->fd[i] = b->m.planes[i].reserved[0];
	binfo->buff_off[i] = b->m.planes[i].reserved[1];
	binfo->size[i] = b->m.planes[i].length;
	binfo->uvaddr[i] = b->m.planes[i].m.userptr;
	binfo->device_addr[i] = 0;
	binfo->handle[i] = NULL;
}

static struct msm_smem *map_buffer(struct msm_vidc_inst *inst,
		struct v4l2_plane *p, enum hal_buffer buffer_type)
{
	struct msm_smem *handle = NULL;
	handle = msm_smem_user_to_kernel(inst->mem_client,
				p->reserved[0],
				p->reserved[1],
				buffer_type);
	if (!handle) {
		dprintk(VIDC_ERR,
			"%s: Failed to get device buffer address\n", __func__);
		return NULL;
	}
	if (msm_smem_cache_operations(inst->mem_client, handle,
			SMEM_CACHE_CLEAN))
		dprintk(VIDC_WARN,
			"CACHE Clean failed: %d, %d, %d\n",
				p->reserved[0],
				p->reserved[1],
				p->length);
	return handle;
}

static inline enum hal_buffer get_hal_buffer_type(
		struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	if (inst->session_type == MSM_VIDC_DECODER) {
		if (b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			return HAL_BUFFER_INPUT;
		else /* V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
			return HAL_BUFFER_OUTPUT;
	} else {
		/* FIXME in the future.  See comment in msm_comm_get_\
		 * domain_partition. Same problem here. */
		if (b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			return HAL_BUFFER_OUTPUT;
		else /* V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE */
			return HAL_BUFFER_INPUT;
	}
	return -EINVAL;
}

int map_and_register_buf(struct msm_vidc_inst *inst, struct v4l2_buffer *b)
{
	struct buffer_info *binfo = NULL;
	struct buffer_info *temp = NULL;
	int plane = 0;
	int i, rc = 0;

	if (!b || !inst) {
		dprintk(VIDC_ERR, "%s: invalid input\n", __func__);
		return -EINVAL;
	}

	/* For kernel clients, we do not need to map the buffer again.*/
	if (!b->m.planes[0].reserved[0])
		return 0;

	if (b->length > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "Num planes exceeds max: %d, %d\n",
			b->length, VIDEO_MAX_PLANES);
		rc = -EINVAL;
		goto exit;
	}

	binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
	if (!binfo) {
		dprintk(VIDC_ERR, "Out of memory\n");
		rc = -ENOMEM;
		goto exit;
	}

	for (i = 0; i < b->length; ++i) {
		if (EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].length) {
			continue;
		}
		temp = get_registered_buf(&inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length, &plane);
		if (temp) {
			dprintk(VIDC_DBG,
				"This memory region has already been prepared\n");
			rc = -EINVAL;
			goto exit;
		}

		temp = get_same_fd_buffer(&inst->registered_bufs,
				b->m.planes[i].reserved[0], &plane);

		if (temp) {
			populate_buf_info(binfo, b, i);
			binfo->device_addr[i] =
			temp->handle[plane]->device_addr + binfo->buff_off[i];
			b->m.planes[i].m.userptr = binfo->device_addr[i];
		} else {
			populate_buf_info(binfo, b, i);
			binfo->handle[i] =
				map_buffer(inst, &b->m.planes[i],
					get_hal_buffer_type(inst, b));
			if (!binfo->handle[i]) {
				rc = -EINVAL;
				goto exit;
			}
			binfo->device_addr[i] =
				binfo->handle[i]->device_addr +
				binfo->buff_off[i];
			b->m.planes[i].m.userptr =
				binfo->device_addr[i];
			dprintk(VIDC_DBG, "Registering buffer: %d, %d, %d\n",
					b->m.planes[i].reserved[0],
					b->m.planes[i].reserved[1],
					b->m.planes[i].length);
		}
	}
	binfo->num_planes = b->length;
	list_add_tail(&binfo->list, &inst->registered_bufs);
	return 0;
exit:
	kfree(binfo);
	return rc;
}

int msm_vidc_prepare_buf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !b)
		return -EINVAL;

	if (map_and_register_buf(inst, b))
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_prepare_buf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_prepare_buf(instance, b);
	return -EINVAL;
}

int msm_vidc_release_buffers(void *instance, int buffer_type)
{
	struct list_head *ptr, *next;
	struct msm_vidc_inst *inst = instance;
	struct buffer_info *bi;
	struct v4l2_buffer buffer_info;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	int i, rc = 0;

	if (!inst)
		return -EINVAL;

	list_for_each_safe(ptr, next, &inst->registered_bufs) {
		bi = list_entry(ptr, struct buffer_info, list);
		if (bi->type == buffer_type) {
			buffer_info.type = bi->type;
			for (i = 0; (i < bi->num_planes)
				&& (i < VIDEO_MAX_PLANES); i++) {
				plane[i].reserved[0] = bi->fd[i];
				plane[i].reserved[1] = bi->buff_off[i];
				plane[i].length = bi->size[i];
				plane[i].m.userptr = bi->device_addr[i];
				buffer_info.m.planes = plane;
				dprintk(VIDC_DBG,
					"Releasing buffer: %d, %d, %d\n",
					buffer_info.m.planes[i].reserved[0],
					buffer_info.m.planes[i].reserved[1],
					buffer_info.m.planes[i].length);
			}
			buffer_info.length = bi->num_planes;
			if (inst->session_type == MSM_VIDC_DECODER)
				rc = msm_vdec_release_buf(instance,
					&buffer_info);
			if (inst->session_type == MSM_VIDC_ENCODER)
				rc = msm_venc_release_buf(instance,
					&buffer_info);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed Release buffer: %d, %d, %d\n",
					buffer_info.m.planes[0].reserved[0],
					buffer_info.m.planes[0].reserved[1],
					buffer_info.m.planes[0].length);
			list_del(&bi->list);
			for (i = 0; i < bi->num_planes; i++) {
				if (bi->handle[i])
					msm_smem_free(inst->mem_client,
							bi->handle[i]);
			}
			kfree(bi);
		}
	}
	return rc;
}

int msm_vidc_encoder_cmd(void *instance, struct v4l2_encoder_cmd *enc)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_cmd(instance, enc);
	return -EINVAL;
}

int msm_vidc_decoder_cmd(void *instance, struct v4l2_decoder_cmd *dec)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_cmd(instance, dec);
	return -EINVAL;
}

int msm_vidc_qbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	struct buffer_info *binfo;
	int plane = 0;
	int rc = 0;
	int i;

	if (!inst || !b)
		return -EINVAL;

	if (b->length > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "num planes exceeds max: %d\n",
			b->length);
		return -EINVAL;
	}
	for (i = 0; i < b->length; ++i) {
		if (!b->m.planes[i].reserved[0])
			continue;
		if (EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].length) {
			b->m.planes[i].m.userptr = 0;
			continue;
		}
		binfo = get_registered_buf(&inst->registered_bufs,
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length, &plane);
		if (!binfo) {
			dprintk(VIDC_ERR,
				"This buffer is not registered: %d, %d, %d\n",
				b->m.planes[i].reserved[0],
				b->m.planes[i].reserved[1],
				b->m.planes[i].length);
			goto err_invalid_buff;
		}
		b->m.planes[i].m.userptr = binfo->device_addr[i];
		dprintk(VIDC_DBG, "Queueing device address = 0x%x\n",
				binfo->device_addr[i]);

		if ((inst->fmts[OUTPUT_PORT]->fourcc ==
			V4L2_PIX_FMT_HEVC_HYBRID) &&  binfo->handle[i] &&
			(b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
			rc = msm_smem_cache_operations(inst->mem_client,
				binfo->handle[i], SMEM_CACHE_INVALIDATE);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to inv caches: %d\n", rc);
				goto err_invalid_buff;
			}
		}

		if (binfo->handle[i] &&
			(b->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
			rc = msm_smem_cache_operations(inst->mem_client,
					binfo->handle[i], SMEM_CACHE_CLEAN);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to clean caches: %d\n", rc);
				goto err_invalid_buff;
			}
		}
	}

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_qbuf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_qbuf(instance, b);

err_invalid_buff:
	return -EINVAL;
}

int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	struct buffer_info *buffer_info;
	int i = 0, rc = 0;
	bool skip_invalidate = false;

	if (!inst || !b)
		return -EINVAL;

	if (b->length > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "num planes exceed maximum: %d\n",
			b->length);
		return -EINVAL;
	}
	if (!b->m.planes[0].reserved[0])
		skip_invalidate = true;
	if (inst->session_type == MSM_VIDC_DECODER)
		rc = msm_vdec_dqbuf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		rc = msm_venc_dqbuf(instance, b);

	if (rc)
		goto fail_dq_buf;

	for (i = 0; i < b->length; i++) {
		if ((EXTRADATA_IDX(b->length) &&
			(i == EXTRADATA_IDX(b->length)) &&
			!b->m.planes[i].m.userptr) || skip_invalidate) {
			continue;
		}
		buffer_info = device_to_uvaddr(
				&inst->registered_bufs,
				b->m.planes[i].m.userptr);
		b->m.planes[i].m.userptr = buffer_info->uvaddr[i];
		if (!b->m.planes[i].m.userptr) {
			dprintk(VIDC_ERR,
			"Failed to find user virtual address, 0x%lx, %d, %d\n",
			b->m.planes[i].m.userptr, b->type, i);
			goto fail_dq_buf;
		}
		if (buffer_info->handle[i] &&
			(b->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
			rc = msm_smem_cache_operations(inst->mem_client,
				buffer_info->handle[i], SMEM_CACHE_INVALIDATE);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to clean caches: %d\n", rc);
				goto fail_dq_buf;
			}
		}
	}
	return rc;
fail_dq_buf:
	dprintk(VIDC_DBG,
			"Failed to dqbuf, capability: %d\n", b->type);
	return -EINVAL;
}

int msm_vidc_streamon(void *instance, enum v4l2_buf_type i)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_streamon(instance, i);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_streamon(instance, i);
	return -EINVAL;
}

int msm_vidc_streamoff(void *instance, enum v4l2_buf_type i)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_streamoff(instance, i);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_streamoff(instance, i);
	return -EINVAL;
}


int msm_vidc_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize)
{
	struct msm_vidc_inst *inst = instance;
	struct msm_vidc_core_capability *capability = NULL;

	if (!inst || !fsize) {
		dprintk(VIDC_ERR, "%s: invalid parameter: %p %p\n",
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
static void *vidc_get_userptr(void *alloc_ctx, unsigned long vaddr,
				unsigned long size, int write)
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
	q->io_flags = 0;
	q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_COPY;

	if (sess == MSM_VIDC_DECODER)
		q->ops = msm_vdec_get_vb2q_ops();
	else if (sess == MSM_VIDC_ENCODER)
		q->ops = msm_venc_get_vb2q_ops();
	q->mem_ops = &msm_vidc_vb2_mem_ops;
	q->drv_priv = inst;
	return vb2_queue_init(q);
}

static int setup_event_queue(void *inst,
				struct video_device *pvdev)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;
	spin_lock_init(&pvdev->fh_lock);
	INIT_LIST_HEAD(&pvdev->fh_list);

	v4l2_fh_init(&vidc_inst->event_handler, pvdev);
	v4l2_fh_add(&vidc_inst->event_handler);

	return rc;
}

int msm_vidc_subscribe_event(void *inst, const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_subscribe(&vidc_inst->event_handler, sub, MAX_EVENTS, NULL);
	return rc;
}


int msm_vidc_unsubscribe_event(void *inst, const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_unsubscribe(&vidc_inst->event_handler, sub);
	return rc;
}

int msm_vidc_dqevent(void *inst, struct v4l2_event *event)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	if (!inst || !event)
		return -EINVAL;

	rc = v4l2_event_dequeue(&vidc_inst->event_handler, event, false);
	return rc;
}

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

	pr_info(VIDC_DBG_TAG "Opening video instance: %p, %d\n",
		VIDC_INFO, inst, session_type);
	mutex_init(&inst->sync_lock);
	mutex_init(&inst->bufq[CAPTURE_PORT].lock);
	mutex_init(&inst->bufq[OUTPUT_PORT].lock);
	mutex_init(&inst->lock);
	inst->session_type = session_type;
	INIT_LIST_HEAD(&inst->pendingq);
	INIT_LIST_HEAD(&inst->internalbufs);
	INIT_LIST_HEAD(&inst->persistbufs);
	INIT_LIST_HEAD(&inst->ctrl_clusters);
	INIT_LIST_HEAD(&inst->registered_bufs);
	init_waitqueue_head(&inst->kernel_event_queue);
	inst->state = MSM_VIDC_CORE_UNINIT_DONE;
	inst->core = core;

	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}
	inst->mem_client = msm_smem_new_client(SMEM_ION,
					&inst->core->resources);
	if (!inst->mem_client) {
		dprintk(VIDC_ERR, "Failed to create memory client\n");
		goto fail_mem_client;
	}
	if (session_type == MSM_VIDC_DECODER) {
		msm_vdec_inst_init(inst);
		msm_vdec_ctrl_init(inst);
	} else if (session_type == MSM_VIDC_ENCODER) {
		msm_venc_inst_init(inst);
		msm_venc_ctrl_init(inst);
	}

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
	rc = msm_comm_try_state(inst, MSM_VIDC_CORE_INIT);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to move video instance to init state\n");
		goto fail_init;
	}
	inst->debugfs_root =
		msm_vidc_debugfs_init_inst(inst, core->debugfs_root);

	setup_event_queue(inst, &core->vdev[session_type].vdev);

	mutex_lock(&core->sync_lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->sync_lock);
	return inst;
fail_init:
	vb2_queue_release(&inst->bufq[OUTPUT_PORT].vb2_bufq);
fail_bufq_output:
	vb2_queue_release(&inst->bufq[CAPTURE_PORT].vb2_bufq);
fail_bufq_capture:
	if (session_type == MSM_VIDC_DECODER)
		msm_vdec_ctrl_deinit(inst);
	else if (session_type == MSM_VIDC_ENCODER)
		msm_venc_ctrl_deinit(inst);
	msm_smem_delete_client(inst->mem_client);
fail_mem_client:
	kfree(inst);
	inst = NULL;
err_invalid_core:
	return inst;
}

static void cleanup_instance(struct msm_vidc_inst *inst)
{
	struct list_head *ptr, *next;
	struct vb2_buf_entry *entry;
	struct internal_buf *buf;
	if (inst) {
		mutex_lock(&inst->lock);
		if (!list_empty(&inst->pendingq)) {
			list_for_each_safe(ptr, next, &inst->pendingq) {
				entry = list_entry(ptr, struct vb2_buf_entry,
						list);
				list_del(&entry->list);
				kfree(entry);
			}
		}
		if (!list_empty(&inst->internalbufs)) {
			list_for_each_safe(ptr, next, &inst->internalbufs) {
				buf = list_entry(ptr, struct internal_buf,
						list);
				list_del(&buf->list);
				mutex_unlock(&inst->lock);
				msm_smem_free(inst->mem_client, buf->handle);
				kfree(buf);
				mutex_lock(&inst->lock);
			}
		}
		if (!list_empty(&inst->persistbufs)) {
			list_for_each_safe(ptr, next, &inst->persistbufs) {
				buf = list_entry(ptr, struct internal_buf,
						list);
				list_del(&buf->list);
				mutex_unlock(&inst->lock);
				msm_smem_free(inst->mem_client, buf->handle);
				kfree(buf);
				mutex_lock(&inst->lock);
			}
		}
		if (inst->extradata_handle) {
			mutex_unlock(&inst->lock);
			msm_smem_free(inst->mem_client, inst->extradata_handle);
			mutex_lock(&inst->lock);
		}
		mutex_unlock(&inst->lock);
		msm_smem_delete_client(inst->mem_client);
		debugfs_remove_recursive(inst->debugfs_root);
	}
}

int msm_vidc_close(void *instance)
{
	struct msm_vidc_inst *inst = instance;
	struct msm_vidc_inst *temp;
	struct msm_vidc_core *core;
	struct list_head *ptr, *next;
	struct buffer_info *bi;
	int rc = 0;
	int i;

	if (!inst)
		return -EINVAL;

	list_for_each_safe(ptr, next, &inst->registered_bufs) {
		bi = list_entry(ptr, struct buffer_info, list);
		if (bi->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			list_del(&bi->list);
			for (i = 0; (i < bi->num_planes)
				&& (i < VIDEO_MAX_PLANES); i++) {
				if (bi->handle[i])
					msm_smem_free(inst->mem_client,
							bi->handle[i]);
			}
			kfree(bi);
		}
	}

	core = inst->core;
	mutex_lock(&core->sync_lock);
	list_for_each_safe(ptr, next, &core->instances) {
		temp = list_entry(ptr, struct msm_vidc_inst, list);
		if (temp == inst)
			list_del(&inst->list);
	}
	mutex_unlock(&core->sync_lock);

	if (inst->session_type == MSM_VIDC_DECODER)
		msm_vdec_ctrl_deinit(inst);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		msm_venc_ctrl_deinit(inst);

	cleanup_instance(inst);
	if (inst->state != MSM_VIDC_CORE_INVALID &&
		core->state != VIDC_CORE_INVALID)
		rc = msm_comm_try_state(inst, MSM_VIDC_CORE_UNINIT);
	else
		rc = msm_comm_force_cleanup(inst);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to move video instance to uninit state\n");
	for (i = 0; i < MAX_PORT_NUM; i++)
		vb2_queue_release(&inst->bufq[i].vb2_bufq);

	pr_info(VIDC_DBG_TAG "Closed video instance: %p\n", VIDC_INFO, inst);
	kfree(inst);
	return 0;
}
