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

int msm_vidc_prepare_buf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !b)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_prepare_buf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_prepare_buf(instance, b);
	return -EINVAL;
}

int msm_vidc_release_buf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !b)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_release_buf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_release_buf(instance, b);
	return -EINVAL;
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

	if (!inst || !b)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_qbuf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_qbuf(instance, b);
	return -EINVAL;
}

int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;

	if (!inst || !b)
		return -EINVAL;

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_dqbuf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_dqbuf(instance, b);
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

int msm_vidc_subscribe_event(void *inst, struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_vidc_inst *vidc_inst = (struct msm_vidc_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_subscribe(&vidc_inst->event_handler, sub, MAX_EVENTS);
	return rc;
}


int msm_vidc_unsubscribe_event(void *inst, struct v4l2_event_subscription *sub)
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

	setup_event_queue(inst, &core->vdev[core_id].vdev);

	mutex_lock(&core->lock);
	list_add_tail(&inst->list, &core->instances);
	mutex_unlock(&core->lock);
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
	int rc = 0;
	int i;

	if (!inst)
		return -EINVAL;

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
