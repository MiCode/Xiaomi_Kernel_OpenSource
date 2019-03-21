// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp.h"
#include <synx_api.h>

#define MSM_CVP_NOMINAL_CYCLES		(444 * 1000 * 1000)
#define MSM_CVP_UHD60E_VPSS_CYCLES	(111 * 1000 * 1000)
#define MSM_CVP_UHD60E_ISE_CYCLES	(175 * 1000 * 1000)
#define MAX_CVP_VPSS_CYCLES		(MSM_CVP_NOMINAL_CYCLES - \
		MSM_CVP_UHD60E_VPSS_CYCLES)
#define MAX_CVP_ISE_CYCLES		(MSM_CVP_NOMINAL_CYCLES - \
		MSM_CVP_UHD60E_ISE_CYCLES)

struct msm_cvp_fence_thread_data {
	struct msm_cvp_inst *inst;
	unsigned int device_id;
	struct cvp_kmd_hfi_fence_packet in_fence_pkt;
	unsigned int arg_type;
};

static struct msm_cvp_fence_thread_data fence_thread_data;

static void print_client_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct cvp_kmd_buffer *cbuf)
{
	if (!(tag & msm_cvp_debug) || !inst || !cbuf)
		return;

	dprintk(tag,
		"%s: %x : idx %2d fd %d off %d size %d type %d flags 0x%x\n",
		str, hash32_ptr(inst->session), cbuf->index, cbuf->fd,
		cbuf->offset, cbuf->size, cbuf->type, cbuf->flags);
}

static void print_cvp_internal_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct msm_cvp_internal_buffer *cbuf)
{
	if (!(tag & msm_cvp_debug) || !inst || !cbuf)
		return;

	dprintk(tag,
		"%s: %x : idx %2d fd %d off %d daddr %x size %d type %d flags 0x%x\n",
		str, hash32_ptr(inst->session), cbuf->buf.index, cbuf->buf.fd,
		cbuf->buf.offset, cbuf->smem.device_addr, cbuf->buf.size,
		cbuf->buf.type, cbuf->buf.flags);
}

static enum hal_buffer get_hal_buftype(const char *str, unsigned int type)
{
	enum hal_buffer buftype = HAL_BUFFER_NONE;

	if (type == CVP_KMD_BUFTYPE_INPUT)
		buftype = HAL_BUFFER_INPUT;
	else if (type == CVP_KMD_BUFTYPE_OUTPUT)
		buftype = HAL_BUFFER_OUTPUT;
	else if (type == CVP_KMD_BUFTYPE_INTERNAL_1)
		buftype = HAL_BUFFER_INTERNAL_SCRATCH_1;
	else if (type == CVP_KMD_BUFTYPE_INTERNAL_2)
		buftype = HAL_BUFFER_INTERNAL_SCRATCH_1;
	else
		dprintk(CVP_ERR, "%s: unknown buffer type %#x\n",
			str, type);

	return buftype;
}

static int msm_cvp_scale_clocks_and_bus(struct msm_cvp_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_cvp_set_clocks(inst->core);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: failed set_clocks for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

	rc = msm_cvp_comm_vote_bus(inst->core);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: failed vote_bus for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

exit:
	return rc;
}

static int msm_cvp_get_session_info(struct msm_cvp_inst *inst,
		struct cvp_kmd_session_info *session)
{
	int rc = 0;

	if (!inst || !inst->core || !session) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	session->session_id = hash32_ptr(inst->session);
	dprintk(CVP_DBG, "%s: id 0x%x\n", __func__, session->session_id);

	return rc;
}

static int msm_cvp_session_get_iova_addr(
	struct msm_cvp_inst *inst,
	struct msm_cvp_internal_buffer *cbuf,
	unsigned int search_fd, unsigned int search_size,
	unsigned int *iova,
	unsigned int *iova_size)
{
	bool found = false;

	mutex_lock(&inst->cvpbufs.lock);
	list_for_each_entry(cbuf, &inst->cvpbufs.list, list) {
		if (cbuf->buf.fd == search_fd) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpbufs.lock);
	if (!found)
		return -EINVAL;

	*iova = cbuf->smem.device_addr;
	if (search_size != cbuf->buf.size) {
		dprintk(CVP_ERR,
			"%s:: invalid size received fd = %d\n",
			__func__, search_fd);
		return -EINVAL;
	}
	*iova_size = cbuf->buf.size;
	return 0;
}

static int msm_cvp_map_buf(struct msm_cvp_inst *inst,
	struct cvp_kmd_buffer *buf)
{
	int rc = 0;
	bool found;
	struct msm_cvp_internal_buffer *cbuf;
	struct hal_session *session;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	session = (struct hal_session *)inst->session;
	mutex_lock(&inst->cvpbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpbufs.list, list) {
		if (cbuf->buf.fd == buf->fd &&
			cbuf->buf.offset == buf->offset) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpbufs.lock);
	if (found) {
		print_client_buffer(CVP_ERR, "duplicate", inst, buf);
		return -EINVAL;
	}

	cbuf = kzalloc(sizeof(struct msm_cvp_internal_buffer), GFP_KERNEL);
	if (!cbuf) {
		dprintk(CVP_ERR, "%s: cbuf alloc failed\n", __func__);
		return -ENOMEM;
	}
	mutex_lock(&inst->cvpbufs.lock);
	list_add_tail(&cbuf->list, &inst->cvpbufs.list);
	mutex_unlock(&inst->cvpbufs.lock);

	memcpy(&cbuf->buf, buf, sizeof(struct cvp_kmd_buffer));
	cbuf->smem.buffer_type = get_hal_buftype(__func__, buf->type);
	cbuf->smem.fd = buf->fd;
	cbuf->smem.offset = buf->offset;
	cbuf->smem.size = buf->size;
	cbuf->smem.flags = buf->flags;
	rc = msm_cvp_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_client_buffer(CVP_ERR, "map failed", inst, buf);
		goto exit;
	}

	if (buf->index) {
		rc = cvp_dsp_register_buffer((uint32_t)cbuf->smem.device_addr,
			buf->index, buf->size, hash32_ptr(session));
		if (rc) {
			dprintk(CVP_ERR,
				"%s: failed dsp registration for fd=%d rc=%d",
				__func__, buf->fd, rc);
			goto exit;
		}
	}
	return rc;

exit:
	if (cbuf->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
	mutex_lock(&inst->cvpbufs.lock);
	list_del(&cbuf->list);
	mutex_unlock(&inst->cvpbufs.lock);
	kfree(cbuf);
	cbuf = NULL;

	return rc;
}

static bool _cvp_msg_pending(struct msm_cvp_inst *inst,
			struct cvp_session_queue *sq,
			struct session_msg **msg)
{
	struct session_msg *mptr = NULL;
	bool result = false;

	spin_lock(&sq->lock);
	if (!kref_read(&inst->kref)) {
		/* The session is being deleted */
		spin_unlock(&sq->lock);
		*msg = NULL;
		return true;
	}
	result = list_empty(&sq->msgs);
	if (!result) {
		mptr = list_first_entry(&sq->msgs, struct session_msg, node);
		list_del_init(&mptr->node);
		sq->msg_count--;
	}
	spin_unlock(&sq->lock);
	*msg = mptr;
	return !result;
}


static int msm_cvp_session_receive_hfi(struct msm_cvp_inst *inst,
			struct cvp_kmd_hfi_packet *out_pkt)
{
	unsigned long wait_time;
	struct session_msg *msg = NULL;
	struct cvp_session_queue *sq;

	if (!inst) {
		dprintk(CVP_ERR, "%s invalid session\n", __func__);
		return -EINVAL;
	}

	sq = &inst->session_queue;

	wait_time = msecs_to_jiffies(CVP_MAX_WAIT_TIME);

	if (wait_event_timeout(sq->wq,
		_cvp_msg_pending(inst, sq, &msg), wait_time) == 0) {
		dprintk(CVP_ERR, "session queue wait timeout\n");
		return -ETIMEDOUT;
	}

	if (msg == NULL) {
		dprintk(CVP_ERR, "%s: session is deleted, no msg\n", __func__);
		return -EINVAL;
	}

	memcpy(out_pkt, &msg->pkt, sizeof(struct hfi_msg_session_hdr));
	kmem_cache_free(inst->session_queue.msg_cache, msg);

	return 0;
}

static int msm_cvp_session_process_hfi(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *in_pkt)
{
	int i, pkt_idx, rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_internal_buffer *cbuf;
	struct buf_desc *buf_ptr;
	unsigned int offset, buf_num;

	if (!inst || !inst->core || !in_pkt) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	pkt_idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)in_pkt);
	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet %d, %x\n", __func__,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
		return pkt_idx;
	}
	offset = cvp_hfi_defs[pkt_idx].buf_offset;
	buf_num = cvp_hfi_defs[pkt_idx].buf_num;

	if (offset != 0 && buf_num != 0) {
		buf_ptr = (struct buf_desc *)&in_pkt->pkt_data[offset];

		for (i = 0; i < buf_num; i++) {
			if (!buf_ptr[i].fd)
				continue;

			rc = msm_cvp_session_get_iova_addr(inst, cbuf,
						buf_ptr[i].fd,
						buf_ptr[i].size,
						&buf_ptr[i].fd,
						&buf_ptr[i].size);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: buf %d unregistered. rc=%d\n",
					__func__, i, rc);
				return rc;
			}
		}
	}
	rc = call_hfi_op(hdev, session_cvp_hfi_send,
			(void *)inst->session, in_pkt);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: Failed in call_hfi_op %d, %x\n",
			__func__, in_pkt->pkt_data[0], in_pkt->pkt_data[1]);
	}

	if (cvp_hfi_defs[pkt_idx].resp != HAL_NO_RESP) {
		rc = wait_for_sess_signal_receipt(inst,
			cvp_hfi_defs[pkt_idx].resp);
		if (rc)
			dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d %d, %x %d\n",
				__func__, rc,
				in_pkt->pkt_data[0],
				in_pkt->pkt_data[1],
				cvp_hfi_defs[pkt_idx].resp);

	}

	return rc;
}

static int msm_cvp_thread_fence_run(void *data)
{
	int i, pkt_idx, rc = 0;
	unsigned long timeout_ms = 1000;
	int synx_obj;
	struct hfi_device *hdev;
	struct msm_cvp_fence_thread_data *fence_thread_data;
	struct cvp_kmd_hfi_fence_packet *in_fence_pkt;
	struct cvp_kmd_hfi_packet *in_pkt;
	struct msm_cvp_inst *inst;
	int *fence;
	struct msm_cvp_internal_buffer *cbuf;
	struct buf_desc *buf_ptr;
	unsigned int offset, buf_num;

	if (!data) {
		dprintk(CVP_ERR, "%s Wrong input data %pK\n", __func__, data);
		do_exit(-EINVAL);
	}

	fence_thread_data = data;
	inst = cvp_get_inst(get_cvp_core(fence_thread_data->device_id),
				(void *)fence_thread_data->inst);
	if (!inst) {
		dprintk(CVP_ERR, "%s Wrong inst %pK\n", __func__, inst);
		do_exit(-EINVAL);
	}
	in_fence_pkt = (struct cvp_kmd_hfi_fence_packet *)
					&fence_thread_data->in_fence_pkt;
	in_pkt = (struct cvp_kmd_hfi_packet *)(in_fence_pkt);
	fence = (int *)(in_fence_pkt->fence_data);
	hdev = inst->core->device;

	pkt_idx = get_pkt_index((struct cvp_hal_session_cmd_pkt *)in_pkt);
	if (pkt_idx < 0) {
		dprintk(CVP_ERR, "%s incorrect packet %d, %x\n", __func__,
			in_pkt->pkt_data[0],
			in_pkt->pkt_data[1]);
		do_exit(pkt_idx);
	}

	offset = cvp_hfi_defs[pkt_idx].buf_offset;
	buf_num = cvp_hfi_defs[pkt_idx].buf_num;

	if (offset != 0 && buf_num != 0) {
		buf_ptr = (struct buf_desc *)&in_pkt->pkt_data[offset];

		for (i = 0; i < buf_num; i++) {
			if (!buf_ptr[i].fd)
				continue;

			rc = msm_cvp_session_get_iova_addr(inst, cbuf,
				buf_ptr[i].fd,
				buf_ptr[i].size,
				&buf_ptr[i].fd,
				&buf_ptr[i].size);
			if (rc) {
				dprintk(CVP_ERR,
					"%s: buf %d unregistered. rc=%d\n",
					__func__, i, rc);
				do_exit(rc);
			}
		}
	}

	//wait on synx before signaling HFI
	switch (fence_thread_data->arg_type) {
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	{
		for (i = 0; i < HFI_DME_BUF_NUM-1; i++) {
			if (fence[(i<<1)]) {
				rc = synx_import(fence[(i<<1)],
					fence[((i<<1)+1)], &synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_import failed\n",
						__func__);
					do_exit(rc);
				}
				rc = synx_wait(synx_obj, timeout_ms);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_wait failed\n",
						__func__);
					do_exit(rc);
				}
				rc = synx_release(synx_obj);
				if (rc) {
					dprintk(CVP_ERR,
						"%s: synx_release failed\n",
						__func__);
					do_exit(rc);
				}
			}
		}

		rc = call_hfi_op(hdev, session_cvp_hfi_send,
				(void *)inst->session, in_pkt);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: Failed in call_hfi_op %d, %x\n",
				__func__, in_pkt->pkt_data[0],
				in_pkt->pkt_data[1]);
			do_exit(rc);
		}

		rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_DME_FRAME_CMD_DONE);
		if (rc)	{
			dprintk(CVP_ERR, "%s: wait for signal failed, rc %d\n",
			__func__, rc);
			do_exit(rc);
		}
		rc = synx_import(fence[((HFI_DME_BUF_NUM-1)<<1)],
				fence[((HFI_DME_BUF_NUM-1)<<1)+1],
				&synx_obj);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_import failed\n", __func__);
			do_exit(rc);
		}
		rc = synx_signal(synx_obj, SYNX_STATE_SIGNALED_SUCCESS);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_signal failed\n", __func__);
			do_exit(rc);
		}
		if (synx_get_status(synx_obj) != SYNX_STATE_SIGNALED_SUCCESS) {
			dprintk(CVP_ERR, "%s: synx_get_status failed\n",
					__func__);
			do_exit(rc);
		}
		rc = synx_release(synx_obj);
		if (rc) {
			dprintk(CVP_ERR, "%s: synx_release failed\n", __func__);
			do_exit(rc);
		}
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown hfi cmd type 0x%x\n",
			__func__, fence_thread_data->arg_type);
		rc = -EINVAL;
		do_exit(rc);
		break;
	}

	do_exit(0);
}

static int msm_cvp_session_process_hfifence(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_arg *arg)
{
	static int thread_num;
	struct task_struct *thread;
	int rc = 0;
	char thread_fence_name[32];

	dprintk(CVP_DBG, "%s:: Enter inst = %d", __func__, inst);
	if (!inst || !inst->core || !arg) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	thread_num = thread_num + 1;
	fence_thread_data.inst = inst;
	fence_thread_data.device_id = (unsigned int)inst->core->id;
	memcpy(&fence_thread_data.in_fence_pkt, &arg->data.hfi_fence_pkt,
				sizeof(struct cvp_kmd_hfi_fence_packet));
	fence_thread_data.arg_type = arg->type;
	snprintf(thread_fence_name, sizeof(thread_fence_name),
				"thread_fence_%d", thread_num);
	thread = kthread_run(msm_cvp_thread_fence_run,
			&fence_thread_data, thread_fence_name);

	return rc;
}

static int msm_cvp_session_cvp_dfs_frame_response(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *dfs_frame)
{
	int rc = 0;

	dprintk(CVP_DBG, "%s:: Enter inst = %pK\n", __func__, inst);

	if (!inst || !inst->core || !dfs_frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	rc = wait_for_sess_signal_receipt(inst,
			HAL_SESSION_DFS_FRAME_CMD_DONE);
	if (rc)
		dprintk(CVP_ERR,
			"%s: wait for signal failed, rc %d\n",
			__func__, rc);
	return rc;
}

static int msm_cvp_session_cvp_dme_frame_response(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *dme_frame)
{
	int rc = 0;

	dprintk(CVP_DBG, "%s:: Enter inst = %d", __func__, inst);

	if (!inst || !inst->core || !dme_frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	rc = wait_for_sess_signal_receipt(inst,
			HAL_SESSION_DME_FRAME_CMD_DONE);
	if (rc)
		dprintk(CVP_ERR,
			"%s: wait for signal failed, rc %d\n",
			__func__, rc);
	return rc;
}

static int msm_cvp_session_cvp_persist_response(
	struct msm_cvp_inst *inst,
	struct cvp_kmd_hfi_packet *pbuf_cmd)
{
	int rc = 0;

	dprintk(CVP_DBG, "%s:: Enter inst = %d", __func__, inst);

	if (!inst || !inst->core || !pbuf_cmd) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	rc = wait_for_sess_signal_receipt(inst,
			HAL_SESSION_PERSIST_CMD_DONE);
	if (rc)
		dprintk(CVP_ERR,
			"%s: wait for signal failed, rc %d\n",
			__func__, rc);
	return rc;
}



static int msm_cvp_send_cmd(struct msm_cvp_inst *inst,
		struct cvp_kmd_send_cmd *send_cmd)
{
	dprintk(CVP_ERR, "%s: UMD gave a deprecated cmd", __func__);

	return 0;
}

static int msm_cvp_request_power(struct msm_cvp_inst *inst,
		struct cvp_kmd_request_power *power)
{
	int rc = 0;

	if (!inst || !power) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dprintk(CVP_DBG,
		"%s: clock_cycles_a %d, clock_cycles_b %d, ddr_bw %d sys_cache_bw %d\n",
		__func__, power->clock_cycles_a, power->clock_cycles_b,
		power->ddr_bw, power->sys_cache_bw);

	return rc;
}

static int msm_cvp_register_buffer(struct msm_cvp_inst *inst,
		struct cvp_kmd_buffer *buf)
{
	struct hfi_device *hdev;
	struct hal_session *session;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	session = (struct hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	print_client_buffer(CVP_DBG, "register", inst, buf);

	return msm_cvp_map_buf(inst, buf);

}

static int msm_cvp_unregister_buffer(struct msm_cvp_inst *inst,
		struct cvp_kmd_buffer *buf)
{
	int rc = 0;
	bool found;
	struct hfi_device *hdev;
	struct msm_cvp_internal_buffer *cbuf;
	struct hal_session *session;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	session = (struct hal_session *)inst->session;
	if (!session) {
		dprintk(CVP_ERR, "%s: invalid session\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	print_client_buffer(CVP_DBG, "unregister", inst, buf);

	mutex_lock(&inst->cvpbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpbufs.list, list) {
		if (cbuf->buf.fd == buf->fd &&
			cbuf->buf.offset == buf->offset) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpbufs.lock);
	if (!found) {
		print_client_buffer(CVP_ERR, "invalid", inst, buf);
		return -EINVAL;
	}

	if (buf->index) {
		rc = cvp_dsp_deregister_buffer((uint32_t)cbuf->smem.device_addr,
			buf->index, buf->size, hash32_ptr(session));
		if (rc) {
			dprintk(CVP_ERR,
				"%s: failed dsp registration for fd = %d rc=%d",
				__func__, buf->fd, rc);
		}
	}

	if (cbuf->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);

	list_del(&cbuf->list);
	kfree(cbuf);

	return rc;
}

int msm_cvp_handle_syscall(struct msm_cvp_inst *inst, struct cvp_kmd_arg *arg)
{
	int rc = 0;

	if (!inst || !arg) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s:: arg->type = %x", __func__, arg->type);

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
	case CVP_KMD_HFI_SEND_CMD:
	{
		struct cvp_kmd_send_cmd *send_cmd =
			(struct cvp_kmd_send_cmd *)&arg->data.send_cmd;

		rc = msm_cvp_send_cmd(inst, send_cmd);
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
	case CVP_KMD_HFI_DFS_CONFIG_CMD:
	case CVP_KMD_HFI_DFS_FRAME_CMD:
	case CVP_KMD_HFI_DME_CONFIG_CMD:
	case CVP_KMD_HFI_DME_FRAME_CMD:
	case CVP_KMD_HFI_PERSIST_CMD:
	{
		struct cvp_kmd_hfi_packet *in_pkt =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_process_hfi(inst, in_pkt);
		break;
	}
	case CVP_KMD_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct cvp_kmd_hfi_packet *dfs_frame =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_cvp_dfs_frame_response(inst, dfs_frame);
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_CMD_RESPONSE:
	{
		struct cvp_kmd_hfi_packet *dme_frame =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_cvp_dme_frame_response(inst, dme_frame);
		break;
	}
	case CVP_KMD_HFI_PERSIST_CMD_RESPONSE:
	{
		struct cvp_kmd_hfi_packet *pbuf_cmd =
			(struct cvp_kmd_hfi_packet *)&arg->data.hfi_pkt;

		rc = msm_cvp_session_cvp_persist_response(inst, pbuf_cmd);
		break;
	}
	case CVP_KMD_HFI_DME_FRAME_FENCE_CMD:
	{
		rc = msm_cvp_session_process_hfifence(inst, arg);
		break;
	}
	default:
		dprintk(CVP_ERR, "%s: unknown arg type 0x%x\n",
				__func__, arg->type);
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

static struct msm_cvp_ctrl msm_cvp_ctrls[] = {
	{
		.id = V4L2_CID_MPEG_VIDC_VIDEO_SECURE,
		.name = "Secure mode",
		.type = V4L2_CTRL_TYPE_BUTTON,
		.minimum = 0,
		.maximum = 1,
		.default_value = 0,
		.step = 1,
		.menu_skip_mask = 0,
		.qmenu = NULL,
	},
};

int msm_cvp_control_init(struct msm_cvp_inst *inst,
		const struct v4l2_ctrl_ops *ctrl_ops)
{
	return msm_cvp_comm_ctrl_init(inst, msm_cvp_ctrls,
		ARRAY_SIZE(msm_cvp_ctrls), ctrl_ops);
}

int msm_cvp_session_pause(struct msm_cvp_inst *inst)
{
	int rc;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = call_hfi_op(hdev, session_pause, (void *)inst->session);
	if (rc)
		dprintk(CVP_ERR, "%s: failed to pause inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));

	return rc;
}

int msm_cvp_session_resume(struct msm_cvp_inst *inst)
{
	int rc;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = call_hfi_op(hdev, session_resume, (void *)inst->session);
	if (rc)
		dprintk(CVP_ERR, "%s: failed to resume inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));

	return rc;
}

int msm_cvp_session_deinit(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct msm_cvp_internal_buffer *cbuf, *temp;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s: inst %pK (%#x)\n", __func__,
		inst, hash32_ptr(inst->session));

	rc = msm_cvp_comm_try_state(inst, MSM_CVP_CLOSE_DONE);
	if (rc)
		dprintk(CVP_ERR, "%s: close failed\n", __func__);

	mutex_lock(&inst->cvpbufs.lock);
	list_for_each_entry_safe(cbuf, temp, &inst->cvpbufs.list, list) {
		print_cvp_internal_buffer(CVP_ERR, "unregistered", inst, cbuf);
		rc = msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
		if (rc)
			dprintk(CVP_ERR, "%s: unmap failed\n", __func__);
		list_del(&cbuf->list);
		kfree(cbuf);
	}
	mutex_unlock(&inst->cvpbufs.lock);

	inst->clk_data.min_freq = 0;
	inst->clk_data.ddr_bw = 0;
	inst->clk_data.sys_cache_bw = 0;
	rc = msm_cvp_scale_clocks_and_bus(inst);
	if (rc)
		dprintk(CVP_ERR, "%s: failed to scale_clocks_and_bus\n",
			__func__);

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
	inst->clk_data.core_id = CVP_CORE_ID_2;
	inst->clk_data.min_freq = 1000;
	inst->clk_data.ddr_bw = 1000;
	inst->clk_data.sys_cache_bw = 1000;

	return rc;
}
