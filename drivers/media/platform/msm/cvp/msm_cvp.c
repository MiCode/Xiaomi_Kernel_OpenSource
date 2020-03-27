// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp.h"
#include "cvp_hfi.h"
#include "cvp_core_hfi.h"
#include "msm_cvp_buf.h"

struct cvp_power_level {
	unsigned long core_sum;
	unsigned long op_core_sum;
	unsigned long bw_sum;
};

static int msm_cvp_get_session_info(struct msm_cvp_inst *inst,
		struct cvp_kmd_session_info *session)
{
	int rc = 0;
	struct msm_cvp_inst *s;

	if (!inst || !inst->core || !session) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	s->cur_cmd_type = CVP_KMD_GET_SESSION_INFO;
	session->session_id = hash32_ptr(inst->session);
	dprintk(CVP_DBG, "%s: id 0x%x\n", __func__, session->session_id);

	s->cur_cmd_type = 0;
	cvp_put_inst(s);
	return rc;
}



static bool cvp_msg_pending(struct cvp_session_queue *sq,
				struct cvp_session_msg **msg, u64 *ktid)
{
	struct cvp_session_msg *mptr, *dummy;
	bool result = false;

	mptr = NULL;
	spin_lock(&sq->lock);
	if (sq->state != QUEUE_ACTIVE) {
		/* The session is being deleted */
		spin_unlock(&sq->lock);
		*msg = NULL;
		return true;
	}
	result = list_empty(&sq->msgs);
	if (!result) {
		if (!ktid) {
			mptr =
			list_first_entry(&sq->msgs, struct cvp_session_msg,
					node);
			list_del_init(&mptr->node);
			sq->msg_count--;
		} else {
			result = true;
			list_for_each_entry_safe(mptr, dummy, &sq->msgs, node) {
				if (*ktid == mptr->pkt.client_data.kdata) {
					list_del_init(&mptr->node);
					sq->msg_count--;
					result = false;
					break;
				}
			}
			if (result)
				mptr = NULL;
		}
	}
	spin_unlock(&sq->lock);
	*msg = mptr;
	return !result;
}

static int cvp_wait_process_message(struct msm_cvp_inst *inst,
				struct cvp_session_queue *sq, u64 *ktid,
				unsigned long timeout,
				struct cvp_kmd_hfi_packet *out)
{
	struct cvp_session_msg *msg = NULL;
	struct cvp_hfi_msg_session_hdr *hdr;
	int rc = 0;

	if (wait_event_timeout(sq->wq,
		cvp_msg_pending(sq, &msg, ktid), timeout) == 0) {
		dprintk(CVP_WARN, "session queue wait timeout\n");
		rc = -ETIMEDOUT;
		goto exit;
	}

	if (msg == NULL) {
		dprintk(CVP_WARN, "%s: queue state %d, msg cnt %d\n", __func__,
					sq->state, sq->msg_count);

		if (inst->state >= MSM_CVP_CLOSE_DONE ||
				sq->state != QUEUE_ACTIVE) {
			rc = -ECONNRESET;
			goto exit;
		}

		msm_cvp_comm_kill_session(inst);
		goto exit;
	}

	if (out)
		memcpy(out, &msg->pkt, sizeof(struct cvp_hfi_msg_session_hdr));

	kmem_cache_free(cvp_driver->msg_cache, msg);
	hdr = (struct cvp_hfi_msg_session_hdr *)out;
	msm_cvp_unmap_frame(inst, hdr->client_data.kdata);

exit:
	return rc;
}

static int msm_cvp_session_receive_hfi(struct msm_cvp_inst *inst,
			struct cvp_kmd_hfi_packet *out_pkt)
{
	unsigned long wait_time;
	struct cvp_session_queue *sq;
	struct msm_cvp_inst *s;
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s invalid session\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	s->cur_cmd_type = CVP_KMD_RECEIVE_MSG_PKT;
	wait_time = msecs_to_jiffies(CVP_MAX_WAIT_TIME);
	sq = &inst->session_queue;

	rc = cvp_wait_process_message(inst, sq, NULL, wait_time, out_pkt);

	s->cur_cmd_type = 0;
	cvp_put_inst(inst);
	return rc;
}

static int msm_cvp_session_process_hfi(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *in_pkt,
	unsigned int in_offset,
	unsigned int in_buf_num)
{
	int pkt_idx, pkt_type, rc = 0;
	struct cvp_hfi_device *hdev;
	unsigned int offset, buf_num, signal;
	struct cvp_session_queue *sq;
	struct msm_cvp_inst *s;

	if (!inst || !inst->core || !in_pkt) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_SEND_CMD_PKT;
	hdev = inst->core->device;

	pkt_idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)in_pkt);
	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet %d, %x\n", __func__,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
		offset = in_offset;
		buf_num = in_buf_num;
		signal = HAL_NO_RESP;
	} else {
		offset = cvp_hfi_defs[pkt_idx].buf_offset;
		buf_num = cvp_hfi_defs[pkt_idx].buf_num;
		signal = cvp_hfi_defs[pkt_idx].resp;
	}
	if (signal == HAL_NO_RESP) {
		/* Frame packets are not allowed before session starts*/
		sq = &inst->session_queue;
		spin_lock(&sq->lock);
		if (sq->state != QUEUE_ACTIVE) {
			spin_unlock(&sq->lock);
			dprintk(CVP_ERR, "%s: invalid queue state\n", __func__);
			rc = -EINVAL;
			goto exit;
		}
		spin_unlock(&sq->lock);
	}

	if (in_offset && in_buf_num) {
		offset = in_offset;
		buf_num = in_buf_num;
	}

	pkt_type = in_pkt->pkt_data[1];
	if (pkt_type == HFI_CMD_SESSION_CVP_SET_PERSIST_BUFFERS)
		rc = msm_cvp_map_user_persist(inst, in_pkt, offset, buf_num);
	else if (pkt_type == HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS)
		rc = msm_cvp_mark_user_persist(inst, in_pkt, offset, buf_num);
	else
		rc = msm_cvp_map_frame(inst, in_pkt, offset, buf_num);

	if (rc)
		goto exit;

	rc = call_hfi_op(hdev, session_send, (void *)inst->session, in_pkt);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: Failed in call_hfi_op %d, %x\n",
			__func__, in_pkt->pkt_data[0], in_pkt->pkt_data[1]);
		goto exit;
	}

	if (signal != HAL_NO_RESP) {
		rc = wait_for_sess_signal_receipt(inst, signal);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d %d, %x %d\n",
				__func__, rc,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1],
				signal);
			goto exit;
		}
		if (pkt_type == HFI_CMD_SESSION_CVP_RELEASE_PERSIST_BUFFERS)
			rc = msm_cvp_unmap_user_persist(inst, in_pkt,
					offset, buf_num);

	}
exit:
	inst->cur_cmd_type = 0;
	cvp_put_inst(inst);
	return rc;
}

static bool cvp_fence_wait(struct cvp_fence_queue *q,
			struct cvp_fence_command **fence,
			enum queue_state *state)
{
	struct cvp_fence_command *f;

	*fence = NULL;
	mutex_lock(&q->lock);
	*state = q->state;
	if (*state != QUEUE_ACTIVE) {
		mutex_unlock(&q->lock);
		return true;
	}

	if (list_empty(&q->wait_list)) {
		mutex_unlock(&q->lock);
		return false;
	}

	f = list_first_entry(&q->wait_list, struct cvp_fence_command, list);
	list_del_init(&f->list);
	list_add_tail(&q->sched_list, &f->list);

	mutex_unlock(&q->lock);
	*fence = f;

	return true;
}

static int cvp_fence_dme(struct msm_cvp_inst *inst, u32 *synx,
			struct cvp_hfi_cmd_session_hdr *pkt)
{
	int i;
	int rc = 0;
	unsigned long timeout;
	int h_synx;
	u64 ktid;
	unsigned long timeout_ms = 1000;
	int synx_state = SYNX_STATE_SIGNALED_SUCCESS;
	struct cvp_hfi_device *hdev;
	struct cvp_session_queue *sq;
	struct synx_session ssid;
	u32 hfi_err = HFI_ERR_NONE;
	struct cvp_hfi_msg_session_hdr *hdr;

	dprintk(CVP_DBG, "%s %s\n", current->comm, __func__);

	hdev = inst->core->device;
	sq = &inst->session_queue_fence;
	ssid = inst->synx_session_id;
	ktid = pkt->client_data.kdata;

	i = 0;
	while (i < HFI_DME_BUF_NUM - 1) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_wait(ssid, h_synx, timeout_ms);
			if (rc) {
				dprintk(CVP_ERR, "%s %s: synx_wait %d failed\n",
					current->comm, __func__, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
				goto exit;
			}
			/*
			 * Increase loop count to skip fence
			 * waiting on downscale image where i == 1.
			 */
			if (i == FENCE_DME_ICA_ENABLED_IDX)
				++i;
		}
		++i;
	}

	rc = call_hfi_op(hdev, session_send, (void *)inst->session,
			(struct cvp_kmd_hfi_packet *)pkt);
	if (rc) {
		dprintk(CVP_ERR, "%s %s: Failed in call_hfi_op %d, %x\n",
			current->comm, __func__, pkt->size, pkt->packet_type);
		synx_state = SYNX_STATE_SIGNALED_ERROR;
		goto exit;
	}

	timeout = msecs_to_jiffies(CVP_MAX_WAIT_TIME);
	rc = cvp_wait_process_message(inst, sq, &ktid, timeout,
				(struct cvp_kmd_hfi_packet *)pkt);
	hdr = (struct cvp_hfi_msg_session_hdr *)pkt;
	hfi_err = hdr->error_type;
	if (rc) {
		dprintk(CVP_ERR, "%s %s: cvp_wait_process_message rc %d\n",
			current->comm, __func__, rc);
		synx_state = SYNX_STATE_SIGNALED_ERROR;
		goto exit;
	}
	if (hfi_err == HFI_ERR_SESSION_FLUSHED) {
		dprintk(CVP_DBG, "%s %s: cvp_wait_process_message flushed\n",
			current->comm, __func__);
		synx_state = SYNX_STATE_SIGNALED_CANCEL;
	} else if (hfi_err == HFI_ERR_SESSION_STREAM_CORRUPT) {
		dprintk(CVP_ERR, "%s %s: cvp_wait_process_msg non-fatal %d\n",
		current->comm, __func__, hfi_err);
		synx_state = SYNX_STATE_SIGNALED_SUCCESS;
	} else if (hfi_err != HFI_ERR_NONE) {
		dprintk(CVP_ERR, "%s %s: cvp_wait_process_message hfi err %d\n",
			current->comm, __func__, hfi_err);
		synx_state = SYNX_STATE_SIGNALED_CANCEL;
	}

exit:
	if (synx[FENCE_DME_ICA_ENABLED_IDX]) {
		h_synx = synx[FENCE_DME_DS_IDX];

		rc = synx_signal(ssid, h_synx, synx_state);
		if (rc) {
			dprintk(CVP_ERR, "%s %s: synx_signal %d failed\n",
				current->comm, __func__, FENCE_DME_DS_IDX);
			synx_state = SYNX_STATE_SIGNALED_ERROR;
		}
	}

	h_synx = synx[FENCE_DME_OUTPUT_IDX];
	rc = synx_signal(ssid, h_synx, synx_state);
	if (rc)
		dprintk(CVP_ERR, "%s %s: synx_signal %d failed\n",
			current->comm, __func__, FENCE_DME_OUTPUT_IDX);

	return rc;
}

static int cvp_fence_proc(struct msm_cvp_inst *inst, u32 *synx,
			struct cvp_hfi_cmd_session_hdr *pkt)
{
	int i;
	int rc = 0;
	unsigned long timeout;
	int h_synx;
	u64 ktid;
	unsigned long timeout_ms = 1000;
	int synx_state = SYNX_STATE_SIGNALED_SUCCESS;
	struct cvp_hfi_device *hdev;
	struct cvp_session_queue *sq;
	struct synx_session ssid;
	u32 in, out;
	u32 hfi_err = HFI_ERR_NONE;
	struct cvp_hfi_msg_session_hdr *hdr;

	dprintk(CVP_DBG, "%s %s\n", current->comm, __func__);

	hdev = inst->core->device;
	sq = &inst->session_queue_fence;
	ssid = inst->synx_session_id;
	ktid = pkt->client_data.kdata;

	in = synx[0] >> 16;
	out = synx[0] & 0xFFFF;

	i = 1;
	while (i <= in) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_wait(ssid, h_synx, timeout_ms);
			if (rc) {
				synx_state = synx_get_status(ssid, h_synx);
				if (synx_state == SYNX_STATE_SIGNALED_CANCEL) {
					dprintk(CVP_DBG,
					"%s: synx_wait %d cancel %d state %d\n",
					current->comm, i, rc, synx_state);
				} else {
					dprintk(CVP_ERR,
					"%s: synx_wait %d failed %d state %d\n",
					current->comm, i, rc, synx_state);
					synx_state = SYNX_STATE_SIGNALED_ERROR;
				}
				goto exit;
			}
		}
		++i;
	}

	rc = call_hfi_op(hdev, session_send, (void *)inst->session,
			(struct cvp_kmd_hfi_packet *)pkt);
	if (rc) {
		dprintk(CVP_ERR, "%s %s: Failed in call_hfi_op %d, %x\n",
			current->comm, __func__, pkt->size, pkt->packet_type);
		synx_state = SYNX_STATE_SIGNALED_ERROR;
		goto exit;
	}

	timeout = msecs_to_jiffies(CVP_MAX_WAIT_TIME);
	rc = cvp_wait_process_message(inst, sq, &ktid, timeout,
				(struct cvp_kmd_hfi_packet *)pkt);
	hdr = (struct cvp_hfi_msg_session_hdr *)pkt;
	hfi_err = hdr->error_type;
	if (rc) {
		dprintk(CVP_ERR, "%s %s: cvp_wait_process_message rc %d\n",
			current->comm, __func__, rc);
		synx_state = SYNX_STATE_SIGNALED_ERROR;
		goto exit;
	}
	if (hfi_err == HFI_ERR_SESSION_FLUSHED) {
		dprintk(CVP_DBG, "%s %s: cvp_wait_process_message flushed\n",
			current->comm, __func__);
		synx_state = SYNX_STATE_SIGNALED_CANCEL;
	} else if (hfi_err != HFI_ERR_NONE) {
		dprintk(CVP_ERR, "%s %s: cvp_wait_process_message hfi err %d\n",
			current->comm, __func__, hfi_err);
		synx_state = SYNX_STATE_SIGNALED_CANCEL;
	}

exit:
	i = in + 1;
	while (i <= in + out) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				current->comm, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
		++i;
	}

	return rc;
}

static int cvp_alloc_fence_data(struct cvp_fence_command **f, u32 size)
{
	struct cvp_fence_command *fcmd;

	fcmd = kzalloc(sizeof(struct cvp_fence_command), GFP_KERNEL);
	if (!fcmd)
		return -ENOMEM;

	fcmd->pkt = kzalloc(size, GFP_KERNEL);
	if (!fcmd->pkt) {
		kfree(fcmd);
		return -ENOMEM;
	}

	*f = fcmd;
	return 0;
}

static void cvp_free_fence_data(struct cvp_fence_command *f)
{
	kfree(f->pkt);
	f->pkt = NULL;
	kfree(f);
	f = NULL;
}

static int cvp_import_synx(struct msm_cvp_inst *inst, u32 type, u32 *fence,
				u32 *synx)
{
	int rc = 0;
	int i;
	int start = 0, end = 0;
	struct cvp_fence_type *f;
	struct synx_import_params params;
	s32 h_synx;
	struct synx_session ssid;

	f = (struct cvp_fence_type *)fence;
	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = 0;
		end = HFI_DME_BUF_NUM;
		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in = fence[0];
		u32 out = fence[1];

		if (in > MAX_HFI_FENCE_SIZE || out > MAX_HFI_FENCE_SIZE
			|| in > MAX_HFI_FENCE_SIZE - out) {
			dprintk(CVP_ERR, "%s: failed!\n", __func__);
			rc = -EINVAL;
			return rc;
		}

		synx[0] = (in << 16) | out;
		start = 1;
		end = in + out + 1;
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = f[i].h_synx;

		if (h_synx) {
			params.h_synx = h_synx;
			params.secure_key = f[i].secure_key;
			params.new_h_synx = &synx[i];

			rc = synx_import(ssid, &params);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: synx_import failed\n",
					__func__);
				return rc;
			}
		}
	}

	return rc;
}

static int cvp_release_synx(struct msm_cvp_inst *inst, u32 type, u32 *synx)
{
	int rc = 0;
	int i;
	s32 h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;

	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = 0;
		end = HFI_DME_BUF_NUM;

		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in = synx[0] >> 16;
		u32 out = synx[0] & 0xFFFF;

		start = 1;
		end = in + out + 1;

		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_release(ssid, h_synx);
			if (rc)
				dprintk(CVP_ERR,
				"%s: synx_release %d failed\n",
				__func__, i);
		}
	}

	return rc;
}

static int cvp_fence_thread(void *data)
{
	int rc = 0;
	struct msm_cvp_inst *inst;
	struct cvp_fence_queue *q;
	enum queue_state state;
	struct cvp_fence_command *f;
	struct cvp_hfi_cmd_session_hdr *pkt;
	u32 *synx;
	u64 ktid;

	dprintk(CVP_DBG, "Enter %s\n", current->comm);

	inst = (struct msm_cvp_inst *)data;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid inst %pK\n", current->comm, inst);
		rc = -EINVAL;
		goto exit;
	}

	q = &inst->fence_cmd_queue;

wait:
	dprintk(CVP_DBG, "%s starts wait\n", current->comm);

	f = NULL;
	wait_event_interruptible(q->wq, cvp_fence_wait(q, &f, &state));
	if (state != QUEUE_ACTIVE)
		goto exit;

	if (!f)
		goto wait;

	pkt = f->pkt;
	synx = (u32 *)f->synx;

	ktid = pkt->client_data.kdata & (FENCE_BIT - 1);
	dprintk(CVP_DBG, "%s starts working on frame %llu frameID %llu\n",
		current->comm, ktid, f->frame_id);

	switch (f->type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
		rc = cvp_fence_dme(inst, synx, pkt);
		break;
	case HFI_CMD_SESSION_CVP_FD_FRAME:
		rc = cvp_fence_proc(inst, synx, pkt);
		break;
	default:
		dprintk(CVP_ERR, "%s: unknown hfi cmd type 0x%x\n",
			__func__, f->type);
		rc = -EINVAL;
		goto exit;
		break;
	}

	mutex_lock(&q->lock);
	cvp_release_synx(inst, f->type, synx);
	list_del_init(&f->list);
	mutex_unlock(&q->lock);

	dprintk(CVP_DBG, "%s is done with frame %llu frameID %llu\n",
		current->comm, ktid, f->frame_id);

	cvp_free_fence_data(f);

	goto wait;

exit:
	dprintk(CVP_DBG, "%s exit\n", current->comm);
	cvp_put_inst(inst);
	do_exit(rc);
}

static int msm_cvp_session_process_hfi_fence(struct msm_cvp_inst *inst,
					struct cvp_kmd_arg *arg)
{
	int rc = 0;
	int idx;
	struct cvp_kmd_hfi_fence_packet *fence_pkt;
	struct cvp_hfi_cmd_session_hdr *pkt;
	unsigned int offset, buf_num, in_offset, in_buf_num;
	struct msm_cvp_inst *s;
	struct cvp_fence_command *f;
	struct cvp_fence_queue *q;
	u32 *fence;
	enum op_mode mode;

	if (!inst || !inst->core || !arg || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	q = &inst->fence_cmd_queue;

	mutex_lock(&q->lock);
	mode = q->mode;
	mutex_unlock(&q->lock);

	if (mode == OP_DRAINING) {
		dprintk(CVP_DBG, "%s: flush in progress\n", __func__);
		rc = -EBUSY;
		goto exit;
	}

	in_offset = arg->buf_offset;
	in_buf_num = arg->buf_num;

	fence_pkt = &arg->data.hfi_fence_pkt;
	pkt = (struct cvp_hfi_cmd_session_hdr *)&fence_pkt->pkt_data;
	fence = (u32 *)&fence_pkt->fence_data;
	idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)pkt);

	if (idx < 0 || pkt->size > MAX_HFI_FENCE_OFFSET) {
		dprintk(CVP_ERR, "%s incorrect packet %d %#x\n", __func__,
				pkt->size, pkt->packet_type);
		goto exit;
	}

	if (in_offset && in_buf_num) {
		offset = in_offset;
		buf_num = in_buf_num;
	} else {
		offset = cvp_hfi_defs[idx].buf_offset;
		buf_num = cvp_hfi_defs[idx].buf_num;
	}

	rc = msm_cvp_map_frame(inst, (struct cvp_kmd_hfi_packet *)pkt, offset,
				buf_num);
	if (rc)
		goto exit;

	rc = cvp_alloc_fence_data(&f, pkt->size);
	if (rc)
		goto exit;

	f->type = cvp_hfi_defs[idx].type;
	f->frame_id = arg->data.hfi_fence_pkt.frame_id;
	f->mode = OP_NORMAL;

	dprintk(CVP_DBG, "%s: frameID %llu\n", __func__, f->frame_id);

	memcpy(f->pkt, pkt, pkt->size);

	f->pkt->client_data.kdata |= FENCE_BIT;

	rc = cvp_import_synx(inst, f->type, fence, f->synx);
	if (rc) {
		kfree(f);
		goto exit;
	}

	mutex_lock(&q->lock);
	list_add_tail(&f->list, &inst->fence_cmd_queue.wait_list);
	mutex_unlock(&q->lock);

	wake_up(&inst->fence_cmd_queue.wq);

exit:
	cvp_put_inst(s);
	return rc;
}

static inline int div_by_1dot5(unsigned int a)
{
	unsigned long i = a << 1;

	return (unsigned int) i/3;
}

static inline int max_3(unsigned int a, unsigned int b, unsigned int c)
{
	return (a >= b) ? ((a >= c) ? a : c) : ((b >= c) ? b : c);
}

static bool is_subblock_profile_existed(struct msm_cvp_inst *inst)
{
	return (inst->prop.od_cycles ||
			inst->prop.mpu_cycles ||
			inst->prop.fdu_cycles ||
			inst->prop.ica_cycles);
}

static void aggregate_power_update(struct msm_cvp_core *core,
	struct cvp_power_level *nrt_pwr,
	struct cvp_power_level *rt_pwr,
	unsigned int max_clk_rate)
{
	struct msm_cvp_inst *inst;
	int i;
	unsigned long fdu_sum[2] = {0}, od_sum[2] = {0}, mpu_sum[2] = {0};
	unsigned long ica_sum[2] = {0}, fw_sum[2] = {0};
	unsigned long op_fdu_max[2] = {0}, op_od_max[2] = {0};
	unsigned long op_mpu_max[2] = {0}, op_ica_max[2] = {0};
	unsigned long op_fw_max[2] = {0}, bw_sum[2] = {0}, op_bw_max[2] = {0};

	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state == MSM_CVP_CORE_INVALID ||
			inst->state == MSM_CVP_CORE_UNINIT ||
			!is_subblock_profile_existed(inst))
			continue;
		if (inst->prop.priority <= CVP_RT_PRIO_THRESHOLD) {
			/* Non-realtime session use index 0 */
			i = 0;
		} else {
			i = 1;
		}
		dprintk(CVP_PROF, "pwrUpdate %pK fdu %u od %u mpu %u ica %u\n",
			inst->prop.fdu_cycles,
			inst->prop.od_cycles,
			inst->prop.mpu_cycles,
			inst->prop.ica_cycles);

		dprintk(CVP_PROF, "pwrUpdate fw %u fdu_o %u od_o %u mpu_o %u\n",
			inst->prop.fw_cycles,
			inst->prop.fdu_op_cycles,
			inst->prop.od_op_cycles,
			inst->prop.mpu_op_cycles);

		dprintk(CVP_PROF, "pwrUpdate ica_o %u fw_o %u bw %u bw_o %u\n",
			inst->prop.ica_op_cycles,
			inst->prop.fw_op_cycles,
			inst->prop.ddr_bw,
			inst->prop.ddr_op_bw);

		fdu_sum[i] += inst->prop.fdu_cycles;
		od_sum[i] += inst->prop.od_cycles;
		mpu_sum[i] += inst->prop.mpu_cycles;
		ica_sum[i] += inst->prop.ica_cycles;
		fw_sum[i] += inst->prop.fw_cycles;
		op_fdu_max[i] =
			(op_fdu_max[i] >= inst->prop.fdu_op_cycles) ?
			op_fdu_max[i] : inst->prop.fdu_op_cycles;
		op_od_max[i] =
			(op_od_max[i] >= inst->prop.od_op_cycles) ?
			op_od_max[i] : inst->prop.od_op_cycles;
		op_mpu_max[i] =
			(op_mpu_max[i] >= inst->prop.mpu_op_cycles) ?
			op_mpu_max[i] : inst->prop.mpu_op_cycles;
		op_ica_max[i] =
			(op_ica_max[i] >= inst->prop.ica_op_cycles) ?
			op_ica_max[i] : inst->prop.ica_op_cycles;
		op_fw_max[i] =
			(op_fw_max[i] >= inst->prop.fw_op_cycles) ?
			op_fw_max[i] : inst->prop.fw_op_cycles;
		bw_sum[i] += inst->prop.ddr_bw;
		op_bw_max[i] =
			(op_bw_max[i] >= inst->prop.ddr_op_bw) ?
			op_bw_max[i] : inst->prop.ddr_op_bw;
	}

	for (i = 0; i < 2; i++) {
		fdu_sum[i] = max_3(fdu_sum[i], od_sum[i], mpu_sum[i]);
		fdu_sum[i] = max_3(fdu_sum[i], ica_sum[i], fw_sum[i]);

		op_fdu_max[i] = max_3(op_fdu_max[i], op_od_max[i],
			op_mpu_max[i]);
		op_fdu_max[i] = max_3(op_fdu_max[i],
			op_ica_max[i], op_fw_max[i]);
		op_fdu_max[i] =
			(op_fdu_max[i] > max_clk_rate) ?
			max_clk_rate : op_fdu_max[i];
		bw_sum[i] = (bw_sum[i] >= op_bw_max[i]) ?
			bw_sum[i] : op_bw_max[i];
	}

	nrt_pwr->core_sum += fdu_sum[0];
	nrt_pwr->op_core_sum = (nrt_pwr->op_core_sum >= op_fdu_max[0]) ?
			nrt_pwr->op_core_sum : op_fdu_max[0];
	nrt_pwr->bw_sum += bw_sum[0];
	rt_pwr->core_sum += fdu_sum[1];
	rt_pwr->op_core_sum = (rt_pwr->op_core_sum >= op_fdu_max[1]) ?
			rt_pwr->op_core_sum : op_fdu_max[1];
	rt_pwr->bw_sum += bw_sum[1];
}


static void aggregate_power_request(struct msm_cvp_core *core,
	struct cvp_power_level *nrt_pwr,
	struct cvp_power_level *rt_pwr,
	unsigned int max_clk_rate)
{
	struct msm_cvp_inst *inst;
	int i;
	unsigned long core_sum[2] = {0}, ctlr_sum[2] = {0}, fw_sum[2] = {0};
	unsigned long op_core_max[2] = {0}, op_ctlr_max[2] = {0};
	unsigned long op_fw_max[2] = {0}, bw_sum[2] = {0}, op_bw_max[2] = {0};

	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state == MSM_CVP_CORE_INVALID ||
			inst->state == MSM_CVP_CORE_UNINIT ||
			is_subblock_profile_existed(inst))
			continue;
		if (inst->prop.priority <= CVP_RT_PRIO_THRESHOLD) {
			/* Non-realtime session use index 0 */
			i = 0;
		} else {
			i = 1;
		}
		dprintk(CVP_PROF, "pwrReq sess %pK core %u ctl %u fw %u\n",
			inst, inst->power.clock_cycles_a,
			inst->power.clock_cycles_b,
			inst->power.reserved[0]);
		dprintk(CVP_PROF, "pwrReq op_core %u op_ctl %u op_fw %u\n",
			inst->power.reserved[1],
			inst->power.reserved[2],
			inst->power.reserved[3]);

		core_sum[i] += inst->power.clock_cycles_a;
		ctlr_sum[i] += inst->power.clock_cycles_b;
		fw_sum[i] += inst->power.reserved[0];
		op_core_max[i] =
			(op_core_max[i] >= inst->power.reserved[1]) ?
			op_core_max[i] : inst->power.reserved[1];
		op_ctlr_max[i] =
			(op_ctlr_max[i] >= inst->power.reserved[2]) ?
			op_ctlr_max[i] : inst->power.reserved[2];
		op_fw_max[i] =
			(op_fw_max[i] >= inst->power.reserved[3]) ?
			op_fw_max[i] : inst->power.reserved[3];
		bw_sum[i] += inst->power.ddr_bw;
		op_bw_max[i] =
			(op_bw_max[i] >= inst->power.reserved[4]) ?
			op_bw_max[i] : inst->power.reserved[4];
	}

	for (i = 0; i < 2; i++) {
		core_sum[i] = max_3(core_sum[i], ctlr_sum[i], fw_sum[i]);
		op_core_max[i] = max_3(op_core_max[i],
			op_ctlr_max[i], op_fw_max[i]);
		op_core_max[i] =
			(op_core_max[i] > max_clk_rate) ?
			max_clk_rate : op_core_max[i];
		bw_sum[i] = (bw_sum[i] >= op_bw_max[i]) ?
			bw_sum[i] : op_bw_max[i];
	}

	nrt_pwr->core_sum += core_sum[0];
	nrt_pwr->op_core_sum = (nrt_pwr->op_core_sum >= op_core_max[0]) ?
			nrt_pwr->op_core_sum : op_core_max[0];
	nrt_pwr->bw_sum += bw_sum[0];
	rt_pwr->core_sum += core_sum[1];
	rt_pwr->op_core_sum = (rt_pwr->op_core_sum >= op_core_max[1]) ?
			rt_pwr->op_core_sum : op_core_max[1];
	rt_pwr->bw_sum += bw_sum[1];
}

/**
 * adjust_bw_freqs(): calculate CVP clock freq and bw required to sustain
 * required use case.
 * Bandwidth vote will be best-effort, not returning error if the request
 * b/w exceeds max limit.
 * Clock vote from non-realtime sessions will be best effort, not returning
 * error if the aggreated session clock request exceeds max limit.
 * Clock vote from realtime session will be hard request. If aggregated
 * session clock request exceeds max limit, the function will return
 * error.
 */
static int adjust_bw_freqs(void)
{
	struct msm_cvp_core *core;
	struct iris_hfi_device *hdev;
	struct bus_info *bus;
	struct clock_set *clocks;
	struct clock_info *cl;
	struct allowed_clock_rates_table *tbl = NULL;
	unsigned int tbl_size;
	unsigned int cvp_min_rate, cvp_max_rate, max_bw, min_bw;
	struct cvp_power_level rt_pwr = {0}, nrt_pwr = {0};
	unsigned long tmp, core_sum, op_core_sum, bw_sum;
	int i, rc = 0;

	core = list_first_entry(&cvp_driver->cores, struct msm_cvp_core, list);

	hdev = core->device->hfi_device_data;
	clocks = &core->resources.clock_set;
	cl = &clocks->clock_tbl[clocks->count - 1];
	tbl = core->resources.allowed_clks_tbl;
	tbl_size = core->resources.allowed_clks_tbl_size;
	cvp_min_rate = tbl[0].clock_rate;
	cvp_max_rate = tbl[tbl_size - 1].clock_rate;
	bus = &core->resources.bus_set.bus_tbl[1];
	max_bw = bus->range[1];
	min_bw = max_bw/10;

	aggregate_power_request(core, &nrt_pwr, &rt_pwr, cvp_max_rate);
	dprintk(CVP_DBG, "PwrReq nrt %u %u rt %u %u\n",
		nrt_pwr.core_sum, nrt_pwr.op_core_sum,
		rt_pwr.core_sum, rt_pwr.op_core_sum);
	aggregate_power_update(core, &nrt_pwr, &rt_pwr, cvp_max_rate);
	dprintk(CVP_DBG, "PwrUpdate nrt %u %u rt %u %u\n",
		nrt_pwr.core_sum, nrt_pwr.op_core_sum,
		rt_pwr.core_sum, rt_pwr.op_core_sum);

	if (rt_pwr.core_sum > cvp_max_rate) {
		dprintk(CVP_WARN, "%s clk vote out of range %lld\n",
			__func__, rt_pwr.core_sum);
		return -ENOTSUPP;
	}

	core_sum = rt_pwr.core_sum + nrt_pwr.core_sum;
	op_core_sum = (rt_pwr.op_core_sum >= nrt_pwr.op_core_sum) ?
		rt_pwr.op_core_sum : nrt_pwr.op_core_sum;

	core_sum = (core_sum >= op_core_sum) ?
		core_sum : op_core_sum;

	if (core_sum > cvp_max_rate) {
		core_sum = cvp_max_rate;
	} else	if (core_sum < cvp_min_rate) {
		core_sum = cvp_min_rate;
	} else {
		for (i = 1; i < tbl_size; i++)
			if (core_sum <= tbl[i].clock_rate)
				break;
		core_sum = tbl[i].clock_rate;
	}

	bw_sum = rt_pwr.bw_sum + nrt_pwr.bw_sum;
	bw_sum = (bw_sum > max_bw) ? max_bw : bw_sum;
	bw_sum = (bw_sum < min_bw) ? min_bw : bw_sum;

	dprintk(CVP_PROF, "%s %lld %lld\n", __func__,
		core_sum, bw_sum);
	if (!cl->has_scaling) {
		dprintk(CVP_ERR, "Cannot scale CVP clock\n");
		return -EINVAL;
	}

	tmp = core->curr_freq;
	core->curr_freq = core_sum;
	rc = msm_cvp_set_clocks(core);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to set clock rate %u %s: %d %s\n",
			core_sum, cl->name, rc, __func__);
		core->curr_freq = tmp;
		return rc;
	}
	hdev->clk_freq = core->curr_freq;
	rc = icc_set_bw(bus->client, bw_sum, 0);
	if (rc)
		dprintk(CVP_ERR, "Failed voting bus %s to ab %u\n",
			bus->name, bw_sum);

	return rc;
}

/**
 * Use of cvp_kmd_request_power structure
 * clock_cycles_a: CVP core clock freq
 * clock_cycles_b: CVP controller clock freq
 * ddr_bw: b/w vote in Bps
 * reserved[0]: CVP firmware required clock freq
 * reserved[1]: CVP core operational clock freq
 * reserved[2]: CVP controller operational clock freq
 * reserved[3]: CVP firmware operational clock freq
 * reserved[4]: CVP operational b/w vote
 *
 * session's power record only saves normalized freq or b/w vote
 */
static int msm_cvp_request_power(struct msm_cvp_inst *inst,
		struct cvp_kmd_request_power *power)
{
	int rc = 0;
	struct msm_cvp_core *core;
	struct msm_cvp_inst *s;

	if (!inst || !power) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_REQUEST_POWER;
	core = inst->core;

	mutex_lock(&core->lock);

	memcpy(&inst->power, power, sizeof(*power));

	/* Normalize CVP controller clock freqs */
	inst->power.clock_cycles_b = div_by_1dot5(inst->power.clock_cycles_b);
	inst->power.reserved[0] = div_by_1dot5(inst->power.reserved[0]);
	inst->power.reserved[2] = div_by_1dot5(inst->power.reserved[2]);
	inst->power.reserved[3] = div_by_1dot5(inst->power.reserved[3]);

	/* Convert bps to KBps */
	inst->power.ddr_bw = inst->power.ddr_bw >> 10;

	rc = adjust_bw_freqs();
	if (rc) {
		memset(&inst->power, 0x0, sizeof(inst->power));
		dprintk(CVP_ERR, "Instance %pK power request out of range\n");
	}

	mutex_unlock(&core->lock);
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);

	return rc;
}

static int msm_cvp_update_power(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct msm_cvp_core *core;
	struct msm_cvp_inst *s;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_UPDATE_POWER;
	core = inst->core;

	mutex_lock(&core->lock);
	rc = adjust_bw_freqs();
	mutex_unlock(&core->lock);
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);

	return rc;
}

static int msm_cvp_register_buffer(struct msm_cvp_inst *inst,
		struct cvp_kmd_buffer *buf)
{
	struct cvp_hfi_device *hdev;
	struct cvp_hal_session *session;
	struct msm_cvp_inst *s;
	int rc = 0;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!buf->index)
		return 0;

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_REGISTER_BUFFER;
	session = (struct cvp_hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		rc = -EINVAL;
		goto exit;
	}
	hdev = inst->core->device;
	print_client_buffer(CVP_DBG, "register", inst, buf);

	rc = msm_cvp_map_buf_dsp(inst, buf);
exit:
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);
	return rc;
}

static int msm_cvp_unregister_buffer(struct msm_cvp_inst *inst,
		struct cvp_kmd_buffer *buf)
{
	struct msm_cvp_inst *s;
	int rc = 0;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!buf->index)
		return 0;

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	inst->cur_cmd_type = CVP_KMD_UNREGISTER_BUFFER;
	print_client_buffer(CVP_DBG, "unregister", inst, buf);

	rc = msm_cvp_unmap_buf_dsp(inst, buf);
	inst->cur_cmd_type = 0;
	cvp_put_inst(s);
	return rc;
}

static int msm_cvp_session_create(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct synx_initialization_params params;

	if (!inst || !inst->core)
		return -EINVAL;

	if (inst->state >= MSM_CVP_CLOSE_DONE)
		return -ECONNRESET;

	if (inst->state != MSM_CVP_CORE_INIT_DONE ||
		inst->state > MSM_CVP_OPEN_DONE) {
		dprintk(CVP_ERR,
			"%s Incorrect CVP state %d to create session\n",
			__func__, inst->state);
		return -EINVAL;
	}

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_OPEN_DONE);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to move instance to open done state\n");
		goto fail_init;
	}

	rc = cvp_comm_set_arp_buffers(inst);
	if (rc) {
		dprintk(CVP_ERR,
				"Failed to set ARP buffers\n");
		goto fail_init;
	}

	params.name = "cvp-kernel-client";
	if (synx_initialize(&inst->synx_session_id, &params)) {
		dprintk(CVP_ERR, "%s synx_initialize failed\n", __func__);
		rc = -EFAULT;
	}

fail_init:
	return rc;
}

static int session_state_check_init(struct msm_cvp_inst *inst)
{
	mutex_lock(&inst->lock);
	if (inst->state == MSM_CVP_OPEN || inst->state == MSM_CVP_OPEN_DONE) {
		mutex_unlock(&inst->lock);
		return 0;
	}
	mutex_unlock(&inst->lock);

	return msm_cvp_session_create(inst);
}

static int cvp_fence_thread_start(struct msm_cvp_inst *inst)
{
	u32 tnum = 0;
	u32 i = 0;
	int rc = 0;
	char tname[16];
	struct task_struct *thread;
	struct cvp_fence_queue *q;
	struct cvp_session_queue *sq;

	if (!inst->prop.fthread_nr)
		return 0;

	q = &inst->fence_cmd_queue;
	mutex_lock(&q->lock);
	q->state = QUEUE_ACTIVE;
	mutex_unlock(&q->lock);

	for (i = 0; i < inst->prop.fthread_nr; ++i) {
		if (!cvp_get_inst_validate(inst->core, inst)) {
			rc = -ECONNRESET;
			goto exit;
		}

		snprintf(tname, sizeof(tname), "fthread_%d", tnum++);
		thread = kthread_run(cvp_fence_thread, inst, tname);
		if (!thread) {
			dprintk(CVP_ERR, "%s create %s fail", __func__, tname);
			rc = -ECHILD;
			goto exit;
		}
	}

	sq = &inst->session_queue_fence;
	spin_lock(&sq->lock);
	sq->state = QUEUE_ACTIVE;
	spin_unlock(&sq->lock);

exit:
	if (rc) {
		mutex_lock(&q->lock);
		q->state = QUEUE_STOP;
		mutex_unlock(&q->lock);
		wake_up_all(&q->wq);
	}
	return rc;
}

static int cvp_fence_thread_stop(struct msm_cvp_inst *inst)
{
	struct cvp_fence_queue *q;
	struct cvp_session_queue *sq;

	if (!inst->prop.fthread_nr)
		return 0;

	q = &inst->fence_cmd_queue;

	mutex_lock(&q->lock);
	q->state = QUEUE_STOP;
	mutex_unlock(&q->lock);

	sq = &inst->session_queue_fence;
	spin_lock(&sq->lock);
	sq->state = QUEUE_STOP;
	spin_unlock(&sq->lock);

	wake_up_all(&q->wq);
	wake_up_all(&sq->wq);

	return 0;
}

static int msm_cvp_session_start(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_session_queue *sq;

	sq = &inst->session_queue;
	spin_lock(&sq->lock);
	if (sq->msg_count) {
		dprintk(CVP_ERR, "session start failed queue not empty%d\n",
			sq->msg_count);
		spin_unlock(&sq->lock);
		return -EINVAL;
	}
	sq->state = QUEUE_ACTIVE;
	spin_unlock(&sq->lock);

	return cvp_fence_thread_start(inst);
}

static int msm_cvp_session_stop(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_session_queue *sq;
	struct cvp_kmd_session_control *sc = &arg->data.session_ctrl;

	sq = &inst->session_queue;

	spin_lock(&sq->lock);
	if (sq->msg_count) {
		dprintk(CVP_ERR, "session stop incorrect: queue not empty%d\n",
			sq->msg_count);
		sc->ctrl_data[0] = sq->msg_count;
		spin_unlock(&sq->lock);
		return -EUCLEAN;
	}
	sq->state = QUEUE_STOP;

	spin_unlock(&sq->lock);

	wake_up_all(&inst->session_queue.wq);

	return cvp_fence_thread_stop(inst);
}

static int msm_cvp_session_ctrl(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_kmd_session_control *ctrl = &arg->data.session_ctrl;
	int rc = 0;
	unsigned int ctrl_type;

	ctrl_type = ctrl->ctrl_type;

	if (!inst && ctrl_type != SESSION_CREATE) {
		dprintk(CVP_ERR, "%s invalid session\n", __func__);
		return -EINVAL;
	}

	switch (ctrl_type) {
	case SESSION_STOP:
		rc = msm_cvp_session_stop(inst, arg);
		break;
	case SESSION_START:
		rc = msm_cvp_session_start(inst, arg);
		break;
	case SESSION_CREATE:
		rc = msm_cvp_session_create(inst);
	case SESSION_DELETE:
		break;
	case SESSION_INFO:
	default:
		dprintk(CVP_ERR, "%s Unsupported session ctrl%d\n",
			__func__, ctrl->ctrl_type);
		rc = -EINVAL;
	}
	return rc;
}

static int msm_cvp_get_sysprop(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_kmd_sys_properties *props = &arg->data.sys_properties;
	struct cvp_hfi_device *hdev;
	struct iris_hfi_device *hfi;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	hfi = hdev->hfi_device_data;

	switch (props->prop_data.prop_type) {
	case CVP_KMD_PROP_HFI_VERSION:
	{
		props->prop_data.data = hfi->version;
		break;
	}
	default:
		dprintk(CVP_ERR, "unrecognized sys property %d\n",
			props->prop_data.prop_type);
		rc = -EFAULT;
	}
	return rc;
}

static int msm_cvp_set_sysprop(struct msm_cvp_inst *inst,
		struct cvp_kmd_arg *arg)
{
	struct cvp_kmd_sys_properties *props = &arg->data.sys_properties;
	struct cvp_kmd_sys_property *prop_array;
	struct cvp_session_prop *session_prop;
	int i, rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (props->prop_num >= MAX_KMD_PROP_NUM) {
		dprintk(CVP_ERR, "Too many properties %d to set\n",
			props->prop_num);
		return -E2BIG;
	}

	prop_array = &arg->data.sys_properties.prop_data;
	session_prop = &inst->prop;

	for (i = 0; i < props->prop_num; i++) {
		switch (prop_array[i].prop_type) {
		case CVP_KMD_PROP_SESSION_TYPE:
			session_prop->type = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_KERNELMASK:
			session_prop->kernel_mask = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_PRIORITY:
			session_prop->priority = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_SECURITY:
			session_prop->is_secure = prop_array[i].data;
			break;
		case CVP_KMD_PROP_SESSION_DSPMASK:
			session_prop->dsp_mask = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FDU:
			session_prop->fdu_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_ICA:
			session_prop->ica_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_OD:
			session_prop->od_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_MPU:
			session_prop->mpu_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FW:
			session_prop->fw_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_DDR:
			session_prop->ddr_bw = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_SYSCACHE:
			session_prop->ddr_cache = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FDU_OP:
			session_prop->fdu_op_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_ICA_OP:
			session_prop->ica_op_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_OD_OP:
			session_prop->od_op_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_MPU_OP:
			session_prop->mpu_op_cycles = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_FW_OP:
			session_prop->fw_op_cycles =
				div_by_1dot5(prop_array[i].data);
			break;
		case CVP_KMD_PROP_PWR_DDR_OP:
			session_prop->ddr_op_bw = prop_array[i].data;
			break;
		case CVP_KMD_PROP_PWR_SYSCACHE_OP:
			session_prop->ddr_op_cache = prop_array[i].data;
			break;
		default:
			dprintk(CVP_ERR,
				"unrecognized sys property to set %d\n",
				prop_array[i].prop_type);
			rc = -EFAULT;
		}
	}
	return rc;
}

static int cvp_cancel_input_synx(struct msm_cvp_inst *inst, u32 type, u32 *synx)
{
	int rc = 0;
	int i;
	int h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;
	int synx_state = SYNX_STATE_SIGNALED_CANCEL;

	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = 1;
		end = HFI_DME_BUF_NUM - 1;
		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in, out;

		in = synx[0] >> 16;
		out = synx[0] & 0xFFFF;

		start = 1;
		end = in + 1;
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc && rc != -EALREADY) {
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				__func__, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
	}

	return rc;
}

static int cvp_cancel_output_synx(struct msm_cvp_inst *inst, u32 type,
					u32 *synx)
{
	int rc = 0;
	int i;
	int h_synx;
	struct synx_session ssid;
	int start = 0, end = 0;
	int synx_state = SYNX_STATE_SIGNALED_CANCEL;

	ssid = inst->synx_session_id;

	switch (type) {
	case HFI_CMD_SESSION_CVP_DME_FRAME:
	{
		start = FENCE_DME_OUTPUT_IDX;
		end = FENCE_DME_OUTPUT_IDX + 1;
		break;
	}
	case HFI_CMD_SESSION_CVP_FD_FRAME:
	{
		u32 in, out;

		in = synx[0] >> 16;
		out = synx[0] & 0xFFFF;

		start = in + 1;
		end = in + out + 1;
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown fence type\n", __func__);
		rc = -EINVAL;
		return rc;
	}

	for (i = start; i < end; ++i) {
		h_synx = synx[i];
		if (h_synx) {
			rc = synx_signal(ssid, h_synx, synx_state);
			if (rc) {
				dprintk(CVP_ERR, "%s: synx_signal %d failed\n",
				__func__, i);
				synx_state = SYNX_STATE_SIGNALED_ERROR;
			}
		}
	}

	return rc;
}

static int cvp_drain_fence_cmd_queue_partial(struct msm_cvp_inst *inst)
{
	unsigned long wait_time;
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f;
	int rc = 0;
	int count = 0, max_count = 0;

	q = &inst->fence_cmd_queue;

	mutex_lock(&q->lock);

	list_for_each_entry(f, &q->sched_list, list) {
		if (f->mode == OP_FLUSH)
			continue;
		++count;
	}

	list_for_each_entry(f, &q->wait_list, list) {
		if (f->mode == OP_FLUSH)
			continue;
		++count;
	}

	mutex_unlock(&q->lock);
	wait_time = count * CVP_MAX_WAIT_TIME * 1000;

	dprintk(CVP_DBG, "%s: wait %d us for %d fence command\n",
			__func__, wait_time, count);

	count = 0;
	max_count = wait_time / 100;

retry:
	mutex_lock(&q->lock);
	f = list_first_entry(&q->sched_list, struct cvp_fence_command, list);

	/* Wait for all normal frames to finish before return */
	if ((f && f->mode == OP_FLUSH) ||
		(list_empty(&q->sched_list) && list_empty(&q->wait_list))) {
		mutex_unlock(&q->lock);
		return rc;
	}

	mutex_unlock(&q->lock);
	usleep_range(100, 200);
	++count;
	if (count < max_count) {
		goto retry;
	} else {
		rc = -ETIMEDOUT;
		dprintk(CVP_ERR, "%s: timed out!\n", __func__);
	}

	return rc;
}

static int cvp_drain_fence_sched_list(struct msm_cvp_inst *inst)
{
	unsigned long wait_time;
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f;
	int rc = 0;
	int count = 0, max_count = 0;
	u64 ktid;

	q = &inst->fence_cmd_queue;

	mutex_lock(&q->lock);
	list_for_each_entry(f, &q->sched_list, list) {
		ktid = f->pkt->client_data.kdata & (FENCE_BIT - 1);
		dprintk(CVP_DBG, "%s: frame %llu is in sched_list\n",
			__func__, ktid);
		dprintk(CVP_DBG, "%s: frameID %llu is in sched_list\n",
			__func__, f->frame_id);
		++count;
	}
	mutex_unlock(&q->lock);
	wait_time = count * CVP_MAX_WAIT_TIME * 1000;

	dprintk(CVP_DBG, "%s: wait %d us for %d fence command\n",
			__func__, wait_time, count);

	count = 0;
	max_count = wait_time / 100;

retry:
	mutex_lock(&q->lock);
	if (list_empty(&q->sched_list)) {
		mutex_unlock(&q->lock);
		return rc;
	}

	mutex_unlock(&q->lock);
	usleep_range(100, 200);
	++count;
	if (count < max_count) {
		goto retry;
	} else {
		rc = -ETIMEDOUT;
		dprintk(CVP_ERR, "%s: timed out!\n", __func__);
	}

	return rc;
}

static int cvp_flush_all(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct msm_cvp_inst *s;
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f, *d;
	struct cvp_hfi_device *hdev;
	u64 ktid;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	q = &inst->fence_cmd_queue;
	hdev = inst->core->device;

	mutex_lock(&q->lock);
	q->mode = OP_DRAINING;

	list_for_each_entry_safe(f, d, &q->wait_list, list) {
		ktid = f->pkt->client_data.kdata & (FENCE_BIT - 1);

		dprintk(CVP_DBG, "%s: flush frame %llu from wait_list\n",
			__func__, ktid);
		dprintk(CVP_DBG, "%s: flush frameID %llu from wait_list\n",
			__func__, f->frame_id);

		list_del_init(&f->list);
		msm_cvp_unmap_frame(inst, f->pkt->client_data.kdata);
		cvp_cancel_output_synx(inst, f->type, f->synx);
		cvp_release_synx(inst, f->type, f->synx);
		cvp_free_fence_data(f);
	}

	list_for_each_entry(f, &q->sched_list, list) {
		ktid = f->pkt->client_data.kdata & (FENCE_BIT - 1);

		dprintk(CVP_DBG, "%s: flush frame %llu from sched_list\n",
			__func__, ktid);
		dprintk(CVP_DBG, "%s: flush frameID %llu from sched_list\n",
			__func__, f->frame_id);
		cvp_cancel_input_synx(inst, f->type, f->synx);
	}

	mutex_unlock(&q->lock);

	dprintk(CVP_DBG, "%s: send flush to fw\n", __func__);

	/* Send flush to FW */
	rc = call_hfi_op(hdev, session_flush, (void *)inst->session);
	if (rc) {
		dprintk(CVP_WARN, "%s: continue flush without fw. rc %d\n",
		__func__, rc);
		goto exit;
	}

	/* Wait for FW response */
	rc = wait_for_sess_signal_receipt(inst, HAL_SESSION_FLUSH_DONE);
	if (rc)
		dprintk(CVP_WARN, "%s: wait for signal failed, rc %d\n",
		__func__, rc);

	dprintk(CVP_DBG, "%s: received flush from fw\n", __func__);

exit:
	rc = cvp_drain_fence_sched_list(inst);

	mutex_lock(&q->lock);
	q->mode = OP_NORMAL;
	mutex_unlock(&q->lock);

	cvp_put_inst(s);
	return rc;
}

static void cvp_mark_fence_command(struct msm_cvp_inst *inst, u64 frame_id)
{
	int found = false;
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f;

	q = &inst->fence_cmd_queue;

	list_for_each_entry(f, &q->sched_list, list) {
		if (found) {
			f->mode = OP_FLUSH;
			continue;
		}

		if (f->frame_id >= frame_id) {
			found = true;
			f->mode = OP_FLUSH;
		}
	}

	list_for_each_entry(f, &q->wait_list, list) {
		if (found) {
			f->mode = OP_FLUSH;
			continue;
		}

		if (f->frame_id >= frame_id) {
			found = true;
			f->mode = OP_FLUSH;
		}
	}
}

static int cvp_flush_frame(struct msm_cvp_inst *inst, u64 frame_id)
{
	int rc = 0;
	struct msm_cvp_inst *s;
	struct cvp_fence_queue *q;
	struct cvp_fence_command *f, *d;
	u64 ktid;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	s = cvp_get_inst_validate(inst->core, inst);
	if (!s)
		return -ECONNRESET;

	q = &inst->fence_cmd_queue;

	mutex_lock(&q->lock);
	q->mode = OP_DRAINING;

	cvp_mark_fence_command(inst, frame_id);

	list_for_each_entry_safe(f, d, &q->wait_list, list) {
		if (f->mode != OP_FLUSH)
			continue;

		ktid = f->pkt->client_data.kdata & (FENCE_BIT - 1);

		dprintk(CVP_DBG, "%s: flush frame %llu from wait_list\n",
			__func__, ktid);
		dprintk(CVP_DBG, "%s: flush frameID %llu from wait_list\n",
			__func__, f->frame_id);

		list_del_init(&f->list);
		msm_cvp_unmap_frame(inst, f->pkt->client_data.kdata);
		cvp_cancel_output_synx(inst, f->type, f->synx);
		cvp_release_synx(inst, f->type, f->synx);
		cvp_free_fence_data(f);
	}

	list_for_each_entry(f, &q->sched_list, list) {
		if (f->mode != OP_FLUSH)
			continue;

		ktid = f->pkt->client_data.kdata & (FENCE_BIT - 1);

		dprintk(CVP_DBG, "%s: flush frame %llu from sched_list\n",
			__func__, ktid);
		dprintk(CVP_DBG, "%s: flush frameID %llu from sched_list\n",
			__func__, f->frame_id);
		cvp_cancel_input_synx(inst, f->type, f->synx);
	}

	mutex_unlock(&q->lock);

	rc = cvp_drain_fence_cmd_queue_partial(inst);
	if (rc)
		dprintk(CVP_WARN, "%s: continue flush. rc %d\n",
		__func__, rc);

	rc = cvp_flush_all(inst);

	cvp_put_inst(s);
	return rc;
}

int msm_cvp_handle_syscall(struct msm_cvp_inst *inst, struct cvp_kmd_arg *arg)
{
	int rc = 0;

	if (!inst || !arg) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s: arg->type = %x", __func__, arg->type);

	if (arg->type != CVP_KMD_SESSION_CONTROL &&
		arg->type != CVP_KMD_SET_SYS_PROPERTY &&
		arg->type != CVP_KMD_GET_SYS_PROPERTY) {

		rc = session_state_check_init(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"Incorrect session state %d for command %#x",
				inst->state, arg->type);
			return rc;
		}
	}

	switch (arg->type) {
	case CVP_KMD_GET_SESSION_INFO:
	{
		struct cvp_kmd_session_info *session =
			(struct cvp_kmd_session_info *)&arg->data.session;

		rc = msm_cvp_get_session_info(inst, session);
		break;
	}
	case CVP_KMD_REQUEST_POWER:
	{
		struct cvp_kmd_request_power *power =
			(struct cvp_kmd_request_power *)&arg->data.req_power;

		rc = msm_cvp_request_power(inst, power);
		break;
	}
	case CVP_KMD_UPDATE_POWER:
	{
		rc = msm_cvp_update_power(inst);
		break;
	}
	case CVP_KMD_REGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *buf =
			(struct cvp_kmd_buffer *)&arg->data.regbuf;

		rc = msm_cvp_register_buffer(inst, buf);
		break;
	}
	case CVP_KMD_UNREGISTER_BUFFER:
	{
		struct cvp_kmd_buffer *buf =
			(struct cvp_kmd_buffer *)&arg->data.unregbuf;

		rc = msm_cvp_unregister_buffer(inst, buf);
		break;
	}
	case CVP_KMD_RECEIVE_MSG_PKT:
	{
		struct cvp_kmd_hfi_packet *out_pkt =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;
		rc = msm_cvp_session_receive_hfi(inst, out_pkt);
		break;
	}
	case CVP_KMD_SEND_CMD_PKT:
	{
		struct cvp_kmd_hfi_packet *in_pkt =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_process_hfi(inst, in_pkt,
				arg->buf_offset, arg->buf_num);
		break;
	}
	case CVP_KMD_SEND_FENCE_CMD_PKT:
	{
		rc = msm_cvp_session_process_hfi_fence(inst, arg);
		break;
	}
	case CVP_KMD_SESSION_CONTROL:
		rc = msm_cvp_session_ctrl(inst, arg);
		break;
	case CVP_KMD_GET_SYS_PROPERTY:
		rc = msm_cvp_get_sysprop(inst, arg);
		break;
	case CVP_KMD_SET_SYS_PROPERTY:
		rc = msm_cvp_set_sysprop(inst, arg);
		break;
	case CVP_KMD_FLUSH_ALL:
		rc = cvp_flush_all(inst);
		break;
	case CVP_KMD_FLUSH_FRAME:
		rc = cvp_flush_frame(inst, arg->data.frame_id);
		break;
	default:
		dprintk(CVP_DBG, "%s: unknown arg type %#x\n",
				__func__, arg->type);
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

int msm_cvp_session_deinit(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct cvp_hal_session *session;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s: inst %pK (%#x)\n", __func__,
		inst, hash32_ptr(inst->session));

	session = (struct cvp_hal_session *)inst->session;
	if (!session)
		return rc;

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CLOSE_DONE);
	if (rc)
		dprintk(CVP_ERR, "%s: close failed\n", __func__);

	rc = msm_cvp_session_deinit_buffers(inst);
	return rc;
}

int msm_cvp_session_init(struct msm_cvp_inst *inst)
{
	int rc = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dprintk(CVP_DBG, "%s: inst %pK (%#x)\n", __func__,
		inst, hash32_ptr(inst->session));

	/* set default frequency */
	inst->clk_data.core_id = 0;
	inst->clk_data.min_freq = 1000;
	inst->clk_data.ddr_bw = 1000;
	inst->clk_data.sys_cache_bw = 1000;

	inst->prop.type = HFI_SESSION_CV;
	if (inst->session_type == MSM_CVP_KERNEL)
		inst->prop.type = HFI_SESSION_DME;

	inst->prop.kernel_mask = 0xFFFFFFFF;
	inst->prop.priority = 0;
	inst->prop.is_secure = 0;
	inst->prop.dsp_mask = 0;
	inst->prop.fthread_nr = 2;

	return rc;
}
