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
#include <media/msm_vidc.h>
#include "msm_vidc_internal.h"
#include "msm_vdec.h"
#include "msm_venc.h"
#include "msm_vidc_common.h"
#include "msm_smem.h"

int msm_vidc_poll(void *instance, struct file *filp,
		struct poll_table_struct *wait)
{
	int rc = 0;
	struct msm_vidc_inst *inst = instance;
	struct vb2_queue *outq = &inst->vb2_bufq[OUTPUT_PORT];
	struct vb2_queue *capq = &inst->vb2_bufq[CAPTURE_PORT];
	struct vb2_buffer *out_vb = NULL;
	struct vb2_buffer *cap_vb = NULL;
	unsigned long flags;
	poll_wait(filp, &inst->event_handler.events->wait, wait);
	if (v4l2_event_pending(&inst->event_handler))
		return POLLPRI;
	if (!outq->streaming && !capq->streaming) {
		pr_err("Returning POLLERR from here: %d, %d\n",
			outq->streaming, capq->streaming);
		return POLLERR;
	}
	poll_wait(filp, &inst->event_handler.events->wait, wait);
	if (v4l2_event_pending(&inst->event_handler))
		return POLLPRI;
	poll_wait(filp, &capq->done_wq, wait);
	poll_wait(filp, &outq->done_wq, wait);
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

int msm_vidc_querycap(void *instance, struct v4l2_capability *cap)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_querycap(instance, cap);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_querycap(instance, cap);
	return -EINVAL;
}
int msm_vidc_enum_fmt(void *instance, struct v4l2_fmtdesc *f)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_enum_fmt(instance, f);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_enum_fmt(instance, f);
	return -EINVAL;
}
int msm_vidc_s_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_s_fmt(instance, f);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_s_fmt(instance, f);
	return -EINVAL;
}
int msm_vidc_g_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_g_fmt(instance, f);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_g_fmt(instance, f);
	return -EINVAL;
}
int msm_vidc_s_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_s_ctrl(instance, control);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_s_ctrl(instance, control);
	return -EINVAL;
}
int msm_vidc_g_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_g_ctrl(instance, control);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_g_ctrl(instance, control);
	return -EINVAL;
}
int msm_vidc_reqbufs(void *instance, struct v4l2_requestbuffers *b)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_reqbufs(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_reqbufs(instance, b);
	return -EINVAL;
}

int msm_vidc_prepare_buf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_prepare_buf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_prepare_buf(instance, b);
	return -EINVAL;
}

int msm_vidc_release_buf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_release_buf(instance, b);
	return -EINVAL;
}

int msm_vidc_qbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_qbuf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_qbuf(instance, b);
	return -EINVAL;
}

int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_dqbuf(instance, b);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_dqbuf(instance, b);
	return -EINVAL;
}

int msm_vidc_streamon(void *instance, enum v4l2_buf_type i)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_streamon(instance, i);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_streamon(instance, i);
	return -EINVAL;
}

int msm_vidc_streamoff(void *instance, enum v4l2_buf_type i)
{
	struct msm_vidc_inst *inst = instance;
	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vdec_streamoff(instance, i);
	if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_venc_streamoff(instance, i);
	return -EINVAL;
}

void *vidc_get_userptr(void *alloc_ctx, unsigned long vaddr,
				unsigned long size, int write)
{
	return NULL;
}

void vidc_put_userptr(void *buf_priv)
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
		q = &inst->vb2_bufq[CAPTURE_PORT];
	} else if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		q = &inst->vb2_bufq[OUTPUT_PORT];
	} else {
		pr_err("buf_type = %d not recognised\n", type);
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

int msm_vidc_open(void *vidc_inst, int core_id, int session_type)
{
	struct msm_vidc_inst *inst = (struct msm_vidc_inst *)vidc_inst;
	struct msm_vidc_core *core = NULL;
	unsigned long flags;
	int rc = 0;
	int i = 0;
	if (core_id >= MSM_VIDC_CORES_MAX ||
			session_type >= MSM_VIDC_MAX_DEVICES) {
		pr_err("Invalid input, core_id = %d, session = %d\n",
			core_id, session_type);
		goto err_invalid_core;
	}
	core = get_vidc_core(core_id);
	if (!core) {
		pr_err("Failed to find core for core_id = %d\n", core_id);
		goto err_invalid_core;
	}

	mutex_init(&inst->sync_lock);
	spin_lock_init(&inst->lock);
	inst->session_type = session_type;
	INIT_LIST_HEAD(&inst->pendingq);
	INIT_LIST_HEAD(&inst->internalbufs);
	INIT_LIST_HEAD(&inst->extradatabufs);
	inst->state = MSM_VIDC_CORE_UNINIT_DONE;
	inst->core = core;
	for (i = SESSION_MSG_INDEX(SESSION_MSG_START);
		i <= SESSION_MSG_INDEX(SESSION_MSG_END); i++) {
		init_completion(&inst->completions[i]);
	}
	inst->mem_client = msm_smem_new_client(SMEM_ION);
	if (!inst->mem_client) {
		pr_err("Failed to create memory client\n");
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
		pr_err("Failed to initialize vb2 queue on capture port\n");
		goto fail_init;
	}
	rc = vb2_bufq_init(inst, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
			session_type);
	if (rc) {
		pr_err("Failed to initialize vb2 queue on capture port\n");
		goto fail_init;
	}
	rc = msm_comm_try_state(inst, MSM_VIDC_CORE_INIT);
	if (rc) {
		pr_err("Failed to move video instance to init state\n");
		goto fail_init;
	}
	spin_lock_irqsave(&core->lock, flags);
	list_add_tail(&inst->list, &core->instances);
	spin_unlock_irqrestore(&core->lock, flags);
	return rc;
fail_init:
	msm_smem_delete_client(inst->mem_client);
fail_mem_client:
	kfree(inst);
	inst = NULL;
err_invalid_core:
	return rc;
}

static void cleanup_instance(struct msm_vidc_inst *inst)
{
	unsigned long flags;
	struct list_head *ptr, *next;
	struct vb2_buf_entry *entry;
	struct internal_buf *buf;
	struct extradata_buf *ebuf;
	if (inst) {
		spin_lock_irqsave(&inst->lock, flags);
		list_for_each_safe(ptr, next, &inst->pendingq) {
			entry = list_entry(ptr, struct vb2_buf_entry, list);
			list_del(&entry->list);
			kfree(entry);
		}
		list_for_each_safe(ptr, next, &inst->internalbufs) {
			buf = list_entry(ptr, struct internal_buf, list);
			list_del(&buf->list);
			msm_smem_free(inst->mem_client, buf->handle);
			kfree(buf);
		}
		list_for_each_safe(ptr, next, &inst->extradatabufs) {
			ebuf = list_entry(ptr, struct extradata_buf, list);
			list_del(&ebuf->list);
			msm_smem_free(inst->mem_client, ebuf->handle);
			kfree(ebuf);
		}
		spin_unlock_irqrestore(&inst->lock, flags);
		msm_smem_delete_client(inst->mem_client);
	}
}

int msm_vidc_close(void *instance)
{
	struct msm_vidc_inst *inst = instance;
	struct msm_vidc_inst *temp;
	struct msm_vidc_core *core;
	struct list_head *ptr, *next;
	int rc = 0;
	core = inst->core;
	mutex_lock(&core->sync_lock);
	list_for_each_safe(ptr, next, &core->instances) {
		temp = list_entry(ptr, struct msm_vidc_inst, list);
		if (temp == inst)
			list_del(&inst->list);
	}
	mutex_unlock(&core->sync_lock);
	rc = msm_comm_try_state(inst, MSM_VIDC_CORE_UNINIT);
	if (rc)
		pr_err("Failed to move video instance to uninit state\n");
	cleanup_instance(inst);
	pr_debug("Closed the instance\n");
	return 0;
}
