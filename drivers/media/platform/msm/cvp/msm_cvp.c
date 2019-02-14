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

void cvp_handle_session_register_buffer_done(enum hal_command_response cmd,
		void *resp)
{
	struct msm_cvp_cb_cmd_done *response = resp;
	struct msm_cvp_inst *inst;
	struct msm_cvp_internal_buffer *cbuf;
	struct v4l2_event event = {0};
	u32 *data;
	bool found;

	if (!response) {
		dprintk(CVP_ERR, "%s: invalid response\n", __func__);
		return;
	}
	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid session %pK\n", __func__,
			response->session_id);
		return;
	}

	mutex_lock(&inst->cvpbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpbufs.list, list) {
		if (response->data.regbuf.client_data ==
				cbuf->smem.device_addr) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpbufs.lock);
	if (!found) {
		dprintk(CVP_ERR, "%s: client_data %x not found\n",
			__func__, response->data.regbuf.client_data);
		goto exit;
	}
	print_cvp_internal_buffer(CVP_DBG, "register_done", inst, cbuf);

	event.type = V4L2_EVENT_MSM_CVP_REGISTER_BUFFER_DONE;
	data = (u32 *)event.u.data;
	data[0] = cbuf->buf.index;
	data[1] = cbuf->buf.type;
	data[2] = cbuf->buf.fd;
	data[3] = cbuf->buf.offset;
	v4l2_event_queue_fh(&inst->event_handler, &event);

exit:
	cvp_put_inst(inst);
}

void cvp_handle_session_unregister_buffer_done(enum hal_command_response cmd,
		void *resp)
{
	int rc;
	struct msm_cvp_cb_cmd_done *response = resp;
	struct msm_cvp_inst *inst;
	struct msm_cvp_internal_buffer *cbuf, *dummy;
	struct v4l2_event event = {0};
	u32 *data;
	bool found;

	if (!response) {
		dprintk(CVP_ERR, "%s: invalid response\n", __func__);
		return;
	}
	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid session %pK\n", __func__,
			response->session_id);
		return;
	}

	mutex_lock(&inst->cvpbufs.lock);
	found = false;
	list_for_each_entry_safe(cbuf, dummy, &inst->cvpbufs.list, list) {
		if (response->data.unregbuf.client_data ==
				cbuf->smem.device_addr) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpbufs.lock);
	if (!found) {
		dprintk(CVP_ERR, "%s: client_data %x not found\n",
			__func__, response->data.unregbuf.client_data);
		goto exit;
	}
	print_cvp_internal_buffer(CVP_DBG, "unregister_done", inst, cbuf);

	rc = msm_cvp_smem_unmap_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_cvp_internal_buffer(CVP_ERR, "unmap fail", inst, cbuf);
		goto exit;
	}

	event.type = V4L2_EVENT_MSM_CVP_UNREGISTER_BUFFER_DONE;
	data = (u32 *)event.u.data;
	data[0] = cbuf->buf.index;
	data[1] = cbuf->buf.type;
	data[2] = cbuf->buf.fd;
	data[3] = cbuf->buf.offset;
	v4l2_event_queue_fh(&inst->event_handler, &event);

	mutex_lock(&inst->cvpbufs.lock);
	list_del(&cbuf->list);
	mutex_unlock(&inst->cvpbufs.lock);
	kfree(cbuf);
	cbuf = NULL;
exit:
	cvp_put_inst(inst);
}

static void print_cvp_cycles(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core;
	struct msm_cvp_inst *temp;

	if (!inst || !inst->core)
		return;
	core = inst->core;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp->session_type == MSM_CVP_CORE) {
			dprintk(CVP_ERR, "session %#x, vpss %d ise %d\n",
				hash32_ptr(temp->session),
				temp->clk_data.vpss_cycles,
				temp->clk_data.ise_cycles);
		}
	}
	mutex_unlock(&core->lock);
}

static bool msm_cvp_clock_aggregation(struct msm_cvp_inst *inst,
		u32 vpss_cycles, u32 ise_cycles)
{
	struct msm_cvp_core *core;
	struct msm_cvp_inst *temp;
	u32 total_vpss_cycles = 0;
	u32 total_ise_cycles = 0;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return false;
	}
	core = inst->core;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp->session_type == MSM_CVP_CORE) {
			total_vpss_cycles += inst->clk_data.vpss_cycles;
			total_ise_cycles += inst->clk_data.ise_cycles;
		}
	}
	mutex_unlock(&core->lock);

	if ((total_vpss_cycles > MAX_CVP_VPSS_CYCLES) ||
		(total_ise_cycles > MAX_CVP_ISE_CYCLES))
		return false;

	return true;
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

static int msm_cvp_session_cvp_dfs_config(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dfsconfig *dfs_config)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_dfsconfig vdfs_config;

	dprintk(CVP_DBG, "%s:: Enter inst = %pK\n", __func__, inst);

	if (!inst || !inst->core || !dfs_config) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	memcpy(&vdfs_config, dfs_config, sizeof(struct msm_cvp_dfsconfig));

	rc = call_hfi_op(hdev, session_cvp_dfs_config,
			(void *)inst->session, &vdfs_config);
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
	struct msm_cvp_dfsframe *dfs_frame)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_dfsframe vdfs_frame;

	dprintk(CVP_DBG, "%s:: Enter inst = %pK\n", __func__, inst);

	if (!inst || !inst->core || !dfs_frame) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	memcpy(&vdfs_frame, dfs_frame, sizeof(vdfs_frame));

	rc = call_hfi_op(hdev, session_cvp_dfs_frame,
			(void *)inst->session, &vdfs_frame);

	if (rc) {
		dprintk(CVP_ERR,
			"%s: Failed in call_hfi_op for session_cvp_dfs_frame\n",
			__func__);
	}

	return rc;
}

static int msm_cvp_session_cvp_dfs_frame_response(
	struct msm_cvp_inst *inst,
	struct msm_cvp_dfsframe *dfs_frame)
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


static int msm_cvp_send_cmd(struct msm_cvp_inst *inst,
		struct msm_cvp_send_cmd *send_cmd)
{
	int rc = 0;
	bool found;
	struct hfi_device *hdev;
	struct msm_cvp_internal_send_cmd  *csend_cmd;
	//struct cvp_register_buffer vbuf;
	struct cvp_frame_data input_frame;

	dprintk(CVP_DBG, "%s:: Enter 1", __func__);
	if (!inst || !inst->core || !send_cmd) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	//print_client_buffer(CVP_DBG, "register", inst, send_cmd);

	mutex_lock(&inst->cvpbufs.lock);
	found = false;
	list_for_each_entry(csend_cmd, &inst->cvpbufs.list, list) {
		if (csend_cmd->send_cmd.cmd_address_fd ==
				send_cmd->cmd_address_fd &&
			csend_cmd->send_cmd.cmd_size == send_cmd->cmd_size) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->cvpbufs.lock);
	if (found)
		return -EINVAL;

	csend_cmd = kzalloc(
		sizeof(struct msm_cvp_internal_send_cmd), GFP_KERNEL);
	if (!csend_cmd) {
		dprintk(CVP_ERR, "%s: csend_cmd alloc failed\n", __func__);
		return -ENOMEM;
	}
	mutex_lock(&inst->cvpbufs.lock);
	list_add_tail(&csend_cmd->list, &inst->cvpbufs.list);
	mutex_unlock(&inst->cvpbufs.lock);

	memset(&input_frame, 0, sizeof(struct cvp_frame_data));

	rc = call_hfi_op(hdev, session_cvp_send_cmd,
			(void *)inst->session, &input_frame);
	if (rc)
		goto exit;

	return rc;

exit:
	if (csend_cmd->smem.device_addr)
		msm_cvp_smem_unmap_dma_buf(inst, &csend_cmd->smem);
	mutex_lock(&inst->cvpbufs.lock);
	list_del(&csend_cmd->list);
	mutex_unlock(&inst->cvpbufs.lock);
	kfree(csend_cmd);
	csend_cmd = NULL;

	return rc;
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

	rc = msm_cvp_clock_aggregation(inst, power->clock_cycles_a,
			power->clock_cycles_b);
	if (!rc) {
		dprintk(CVP_ERR,
			"%s: session %#x rejected, cycles: vpss %d, ise %d\n",
			__func__, hash32_ptr(inst->session),
			power->clock_cycles_a, power->clock_cycles_b);
		print_cvp_cycles(inst);
		msm_cvp_comm_kill_session(inst);
		return -EOVERFLOW;
	}

	inst->clk_data.min_freq = max(power->clock_cycles_a,
		power->clock_cycles_b);
	/* convert client provided bps into kbps as expected by driver */
	inst->clk_data.ddr_bw = power->ddr_bw / 1000;
	inst->clk_data.sys_cache_bw = power->sys_cache_bw / 1000;
	rc = msm_cvp_scale_clocks_and_bus(inst);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: failed to scale clocks and bus for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

	if (!inst->clk_data.min_freq && !inst->clk_data.ddr_bw &&
		!inst->clk_data.sys_cache_bw) {
		rc = msm_cvp_session_pause(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: failed to pause inst %pK (%#x)\n",
				__func__, inst, hash32_ptr(inst->session));
			goto exit;
		}
	} else {
		rc = msm_cvp_session_resume(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: failed to resume inst %pK (%#x)\n",
				__func__, inst, hash32_ptr(inst->session));
			goto exit;
		}
	}

exit:
	return rc;
}

static int msm_cvp_register_buffer(struct msm_cvp_inst *inst,
		struct msm_cvp_buffer *buf)
{
	int rc = 0;
	bool found;
	struct hfi_device *hdev;
	struct msm_cvp_internal_buffer *cbuf;
	struct cvp_register_buffer vbuf;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	print_client_buffer(CVP_DBG, "register", inst, buf);

	mutex_lock(&inst->cvpbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpbufs.list, list) {
		if (cbuf->buf.index == buf->index &&
			cbuf->buf.fd == buf->fd &&
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
	rc = msm_cvp_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_client_buffer(CVP_ERR, "map failed", inst, buf);
		goto exit;
	}

	memset(&vbuf, 0, sizeof(struct cvp_register_buffer));
	vbuf.index = buf->index;
	vbuf.type = get_hal_buftype(__func__, buf->type);
	vbuf.size = buf->size;
	vbuf.device_addr = cbuf->smem.device_addr;
	vbuf.client_data = cbuf->smem.device_addr;
	vbuf.response_required = true;
	rc = call_hfi_op(hdev, session_register_buffer,
			(void *)inst->session, &vbuf);
	if (rc) {
		print_cvp_internal_buffer(CVP_ERR,
			"register failed", inst, cbuf);
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
	struct cvp_unregister_buffer vbuf;

	if (!inst || !inst->core || !buf) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	print_client_buffer(CVP_DBG, "unregister", inst, buf);

	mutex_lock(&inst->cvpbufs.lock);
	found = false;
	list_for_each_entry(cbuf, &inst->cvpbufs.list, list) {
		if (cbuf->buf.index == buf->index &&
			cbuf->buf.fd == buf->fd &&
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

	memset(&vbuf, 0, sizeof(struct cvp_unregister_buffer));
	vbuf.index = cbuf->buf.index;
	vbuf.type = get_hal_buftype(__func__, cbuf->buf.type);
	vbuf.size = cbuf->buf.size;
	vbuf.device_addr = cbuf->smem.device_addr;
	vbuf.client_data = cbuf->smem.device_addr;
	vbuf.response_required = true;
	rc = call_hfi_op(hdev, session_unregister_buffer,
			(void *)inst->session, &vbuf);
	if (rc)
		print_cvp_internal_buffer(CVP_ERR,
			"unregister failed", inst, cbuf);

	return rc;
}

int msm_cvp_handle_syscall(struct msm_cvp_inst *inst, struct msm_cvp_arg *arg)
{
	int rc = 0;

	if (!inst || !arg) {
		dprintk(CVP_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}
	dprintk(CVP_DBG, "%s:: arg->type = %d", __func__, arg->type);

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
		struct msm_cvp_dfsconfig *dfsconfig =
			(struct msm_cvp_dfsconfig *)&arg->data.dfsconfig;

		rc = msm_cvp_session_cvp_dfs_config(inst, dfsconfig);
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD:
	{
		struct msm_cvp_dfsframe *dfsframe =
			(struct msm_cvp_dfsframe *)&arg->data.dfsframe;

		rc = msm_cvp_session_cvp_dfs_frame(inst, dfsframe);
		break;
	}
	case MSM_CVP_HFI_DFS_FRAME_CMD_RESPONSE:
	{
		struct msm_cvp_dfsframe *dfsframe =
			(struct msm_cvp_dfsframe *)&arg->data.dfsframe;

		rc = msm_cvp_session_cvp_dfs_frame_response(inst, dfsframe);
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
