// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include "msm_cvp.h"

#define MSM_CVP_NOMINAL_CYCLES		(444 * 1000 * 1000)
#define MSM_CVP_UHD60E_VPSS_CYCLES	(111 * 1000 * 1000)
#define MSM_CVP_UHD60E_ISE_CYCLES	(175 * 1000 * 1000)
#define MAX_CVP_VPSS_CYCLES		(MSM_CVP_NOMINAL_CYCLES - \
		MSM_CVP_UHD60E_VPSS_CYCLES)
#define MAX_CVP_ISE_CYCLES		(MSM_CVP_NOMINAL_CYCLES - \
		MSM_CVP_UHD60E_ISE_CYCLES)

static void print_client_buffer(u32 tag, const char *str,
		struct msm_cvp_inst *inst, struct msm_cvp_buffer *cbuf)
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

	if (type == MSM_CVP_BUFTYPE_INPUT)
		buftype = HAL_BUFFER_INPUT;
	else if (type == MSM_CVP_BUFTYPE_OUTPUT)
		buftype = HAL_BUFFER_OUTPUT;
	else if (type == MSM_CVP_BUFTYPE_INTERNAL_1)
		buftype = HAL_BUFFER_INTERNAL_SCRATCH_1;
	else if (type == MSM_CVP_BUFTYPE_INTERNAL_2)
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
		struct msm_cvp_session_info *session)
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

/* DFS feature system call handling */
static int msm_cvp_session_cvp_dfs_config(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dfs_config *dfs_config)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_internal_dfsconfig internal_dfs_config;

	dprintk(CVP_DBG, "%s:: Enter inst = %pK\n", __func__, inst);

	if (!inst || !inst->core || !dfs_config) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	memcpy(&internal_dfs_config.dfs_config.cvp_dfs_config,
		dfs_config,	sizeof(struct msm_cvp_dfs_config));

	rc = call_hfi_op(hdev, session_cvp_dfs_config,
			(void *)inst->session, &internal_dfs_config);
	if (!rc) {
		rc = wait_for_sess_signal_receipt(inst,
			HAL_SESSION_DFS_CONFIG_CMD_DONE);
		if (rc)
			dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d\n",
				__func__, rc);
	} else {
		dprintk(CVP_ERR,
			"%s: Failed in call_hfi_op for session_cvp_dfs_config\n",
			__func__);
	}
	return rc;
}

static int msm_cvp_session_cvp_dfs_frame(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dfs_frame *dfs_frame)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_internal_dfsframe internal_dfs_frame;
	struct msm_cvp_dfs_frame_kmd *dest_ptr = &internal_dfs_frame.dfs_frame;
	struct msm_cvp_dfs_frame_kmd src_frame;
	struct msm_cvp_internal_buffer *cbuf;

	dprintk(CVP_DBG, "%s:: Enter inst = %pK\n", __func__, inst);

	if (!inst || !inst->core || !dfs_frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	src_frame = *(struct msm_cvp_dfs_frame_kmd *)dfs_frame;
	hdev = inst->core->device;
	memset(&internal_dfs_frame, 0,
		sizeof(struct msm_cvp_internal_dfsframe));

	memcpy(&internal_dfs_frame.dfs_frame, dfs_frame,
		CVP_DFS_FRAME_CMD_SIZE*sizeof(unsigned int));

	rc = msm_cvp_session_get_iova_addr(inst, cbuf,
			src_frame.left_view_buffer_fd,
			src_frame.left_view_buffer_size,
			&dest_ptr->left_view_buffer_fd,
			&dest_ptr->left_view_buffer_size);
	if (rc) {
		dprintk(CVP_ERR, "%s:: left buffer not registered. rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = msm_cvp_session_get_iova_addr(inst, cbuf,
			src_frame.right_view_buffer_fd,
			src_frame.right_view_buffer_size,
			&dest_ptr->right_view_buffer_fd,
			&dest_ptr->right_view_buffer_size);
	if (rc) {
		dprintk(CVP_ERR, "%s:: right buffer not registered. rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = msm_cvp_session_get_iova_addr(inst, cbuf,
			src_frame.disparity_map_buffer_fd,
			src_frame.disparity_map_buffer_size,
			&dest_ptr->disparity_map_buffer_fd,
			&dest_ptr->disparity_map_buffer_size);
	if (rc) {
		dprintk(CVP_ERR, "%s:: disparity map not registered. rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = msm_cvp_session_get_iova_addr(inst, cbuf,
			src_frame.occlusion_mask_buffer_fd,
			src_frame.occlusion_mask_buffer_size,
			&dest_ptr->occlusion_mask_buffer_fd,
			&dest_ptr->occlusion_mask_buffer_size);
	if (rc) {
		dprintk(CVP_ERR, "%s:: occlusion mask not registered. rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = call_hfi_op(hdev, session_cvp_dfs_frame,
			(void *)inst->session, &internal_dfs_frame);

	if (rc) {
		dprintk(CVP_ERR,
			"%s: Failed in call_hfi_op for session_cvp_dfs_frame\n",
			__func__);
	}

	return rc;
}

static int msm_cvp_session_cvp_dfs_frame_response(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dfs_frame *dfs_frame)
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

/* DME feature system call handling */
static int msm_cvp_session_cvp_dme_config(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dme_config *dme_config)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_internal_dmeconfig internal_dme_config;

	dprintk(CVP_DBG, "%s:: Enter inst = %d", __func__, inst);

	if (!inst || !inst->core || !dme_config) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	memcpy(&internal_dme_config.dme_config.cvp_dme_config,
		dme_config, sizeof(struct msm_cvp_dme_config));

	rc = call_hfi_op(hdev, session_cvp_dme_config,
			(void *)inst->session, &internal_dme_config);
	if (!rc) {
		rc = wait_for_sess_signal_receipt(inst,
			HAL_SESSION_DME_CONFIG_CMD_DONE);
		if (rc)
			dprintk(CVP_ERR,
				"%s: wait for signal failed, rc %d\n",
				__func__, rc);
	} else {
		dprintk(CVP_ERR, "%s Failed in call_hfi_op\n", __func__);
	}
	return rc;
}

static int msm_cvp_session_cvp_dme_frame(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dme_frame *dme_frame)
{
	int i, rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_internal_dmeframe internal_dme_frame;
	struct msm_cvp_dme_frame_kmd *dest_ptr = &internal_dme_frame.dme_frame;
	struct msm_cvp_dme_frame_kmd src_frame;
	struct msm_cvp_internal_buffer *cbuf;

	dprintk(CVP_DBG, "%s:: Enter inst = %d", __func__, inst);

	if (!inst || !inst->core || !dme_frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	src_frame = *(struct msm_cvp_dme_frame_kmd *)dme_frame;
	hdev = inst->core->device;
	memset(&internal_dme_frame, 0,
		sizeof(struct msm_cvp_internal_dmeframe));

	memcpy(&internal_dme_frame.dme_frame, dme_frame,
		CVP_DME_FRAME_CMD_SIZE*sizeof(unsigned int));

	for (i = 0; i < CVP_DME_BUF_NUM; i++) {
		if (!src_frame.bufs[i].fd) {
			dest_ptr->bufs[i].fd = src_frame.bufs[i].fd;
			dest_ptr->bufs[i].size = src_frame.bufs[i].size;
			continue;
		}

		rc = msm_cvp_session_get_iova_addr(inst, cbuf,
				src_frame.bufs[i].fd,
				src_frame.bufs[i].size,
				&dest_ptr->bufs[i].fd,
				&dest_ptr->bufs[i].size);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: %d buffer not registered. rc=%d\n",
				__func__, i, rc);
			return rc;
		}

	}

	rc = call_hfi_op(hdev, session_cvp_dme_frame,
			(void *)inst->session, &internal_dme_frame);

	if (rc) {
		dprintk(CVP_ERR,
			"%s:: Failed in call_hfi_op\n",
			__func__);
	}

	return rc;
}

static int msm_cvp_session_cvp_persist(
	struct msm_cvp_inst *inst,
	struct msm_cvp_persist_buf *pbuf_cmd)
{
	int i, rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_internal_persist_cmd internal_pcmd;
	struct msm_cvp_persist_kmd *dest_ptr = &internal_pcmd.persist_cmd;
	struct msm_cvp_persist_kmd src_frame;
	struct msm_cvp_internal_buffer *cbuf;

	dprintk(CVP_DBG, "%s:: Enter inst = %d", __func__, inst);

	if (!inst || !inst->core || !pbuf_cmd) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	src_frame = *(struct msm_cvp_persist_kmd *)pbuf_cmd;
	hdev = inst->core->device;
	memset(&internal_pcmd, 0,
		sizeof(struct msm_cvp_internal_persist_cmd));

	memcpy(&internal_pcmd.persist_cmd, pbuf_cmd,
		CVP_PERSIST_CMD_SIZE*sizeof(unsigned int));

	for (i = 0; i < CVP_PSRSIST_BUF_NUM; i++) {
		if (!src_frame.bufs[i].fd) {
			dest_ptr->bufs[i].fd = src_frame.bufs[i].fd;
			dest_ptr->bufs[i].size = src_frame.bufs[i].size;
			continue;
		}

		rc = msm_cvp_session_get_iova_addr(inst, cbuf,
				src_frame.bufs[i].fd,
				src_frame.bufs[i].size,
				&dest_ptr->bufs[i].fd,
				&dest_ptr->bufs[i].size);
		if (rc) {
			dprintk(CVP_ERR,
				"%s:: %d buffer not registered. rc=%d\n",
				__func__, i, rc);
			return rc;
		}
	}

	rc = call_hfi_op(hdev, session_cvp_persist,
			(void *)inst->session, &internal_pcmd);

	if (rc)
		dprintk(CVP_ERR, "%s: Failed in call_hfi_op\n", __func__);

	return rc;
}

static int msm_cvp_session_cvp_dme_frame_response(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dme_frame *dme_frame)
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
	struct msm_cvp_persist_buf *pbuf_cmd)
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
		struct msm_cvp_send_cmd *send_cmd)
{
	dprintk(CVP_ERR, "%s: UMD gave a deprecated cmd", __func__);

	return 0;
}

static int msm_cvp_request_power(struct msm_cvp_inst *inst,
		struct msm_cvp_request_power *power)
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
		struct msm_cvp_buffer *buf)
{
	int rc = 0;
	bool found;
	struct hfi_device *hdev;
	struct msm_cvp_internal_buffer *cbuf;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	print_client_buffer(CVP_DBG, "register", inst, buf);

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

	memcpy(&cbuf->buf, buf, sizeof(struct msm_cvp_buffer));
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

static int msm_cvp_unregister_buffer(struct msm_cvp_inst *inst,
		struct msm_cvp_buffer *buf)
{
	int rc = 0;
	bool found;
	struct hfi_device *hdev;
	struct msm_cvp_internal_buffer *cbuf;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
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
			if (cbuf->smem.device_addr)
				msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
			list_del(&cbuf->list);
			kfree(cbuf);
			break;
		}
	}
	mutex_unlock(&inst->cvpbufs.lock);
	if (!found) {
		print_client_buffer(CVP_ERR, "invalid", inst, buf);
		return -EINVAL;
	}

	return rc;
}

int msm_cvp_handle_syscall(struct msm_cvp_inst *inst, struct msm_cvp_arg *arg)
{
	int rc = 0;

	if (!inst || !arg) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s:: arg->type = %x", __func__, arg->type);

	switch (arg->type) {
	case MSM_CVP_GET_SESSION_INFO:
	{
		struct msm_cvp_session_info *session =
			(struct msm_cvp_session_info *)&arg->data.session;

		rc = msm_cvp_get_session_info(inst, session);
		break;
	}
	case MSM_CVP_REQUEST_POWER:
	{
		struct msm_cvp_request_power *power =
			(struct msm_cvp_request_power *)&arg->data.req_power;

		rc = msm_cvp_request_power(inst, power);
		break;
	}
	case MSM_CVP_REGISTER_BUFFER:
	{
		struct msm_cvp_buffer *buf =
			(struct msm_cvp_buffer *)&arg->data.regbuf;

		rc = msm_cvp_register_buffer(inst, buf);
		break;
	}
	case MSM_CVP_UNREGISTER_BUFFER:
	{
		struct msm_cvp_buffer *buf =
			(struct msm_cvp_buffer *)&arg->data.unregbuf;

		rc = msm_cvp_unregister_buffer(inst, buf);
		break;
	}
	case MSM_CVP_HFI_SEND_CMD:
	{
		//struct msm_cvp_buffer *buf =
		//(struct msm_cvp_buffer *)&arg->data.unregbuf;
		struct msm_cvp_send_cmd *send_cmd =
			(struct msm_cvp_send_cmd *)&arg->data.send_cmd;

		rc = msm_cvp_send_cmd(inst, send_cmd);
		break;
	}
	case MSM_CVP_HFI_DFS_CONFIG_CMD:
	{
		struct msm_cvp_dfs_config *dfs_config =
			(struct msm_cvp_dfs_config *)&arg->data.dfs_config;

		rc = msm_cvp_session_cvp_dfs_config(inst, dfs_config);
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD:
	{
		struct msm_cvp_dfs_frame *dfs_frame =
			(struct msm_cvp_dfs_frame *)&arg->data.dfs_frame;

		rc = msm_cvp_session_cvp_dfs_frame(inst, dfs_frame);
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dfs_frame *dfs_frame =
			(struct msm_cvp_dfs_frame *)&arg->data.dfs_frame;

		rc = msm_cvp_session_cvp_dfs_frame_response(inst, dfs_frame);
		break;
	}
	case MSM_CVP_HFI_DME_CONFIG_CMD:
	{
		struct msm_cvp_dme_config *dme_config =
			(struct msm_cvp_dme_config *)&arg->data.dme_config;

		rc = msm_cvp_session_cvp_dme_config(inst, dme_config);
		break;
	}
	case MSM_CVP_HFI_DME_FRAME_CMD:
	{
		struct msm_cvp_dme_frame *dme_frame =
			(struct msm_cvp_dme_frame *)&arg->data.dme_frame;

		rc = msm_cvp_session_cvp_dme_frame(inst, dme_frame);
		break;
	}
	case MSM_CVP_HFI_DME_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dme_frame *dmeframe =
			(struct msm_cvp_dme_frame *)&arg->data.dme_frame;

		rc = msm_cvp_session_cvp_dme_frame_response(inst, dmeframe);
		break;
	}
	case MSM_CVP_HFI_PERSIST_CMD:
	{
		struct msm_cvp_persist_buf *pbuf_cmd =
			(struct msm_cvp_persist_buf *)&arg->data.pbuf_cmd;

		rc = msm_cvp_session_cvp_persist(inst, pbuf_cmd);
		break;
	}
	case MSM_CVP_HFI_PERSIST_CMD_RESPONSE:
	{
		struct msm_cvp_persist_buf *pbuf_cmd =
			(struct msm_cvp_persist_buf *)&arg->data.pbuf_cmd;

		rc = msm_cvp_session_cvp_persist_response(inst, pbuf_cmd);
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
