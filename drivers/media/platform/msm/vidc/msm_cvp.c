/* Copyright (c) 2018, 2020, The Linux Foundation. All rights reserved.
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

#include "msm_cvp.h"

#define MSM_VIDC_NOMINAL_CYCLES		(444 * 1000 * 1000)
#define MSM_VIDC_UHD60E_VPSS_CYCLES	(111 * 1000 * 1000)
#define MSM_VIDC_UHD60E_ISE_CYCLES	(175 * 1000 * 1000)
#define MAX_CVP_VPSS_CYCLES		(MSM_VIDC_NOMINAL_CYCLES - \
		MSM_VIDC_UHD60E_VPSS_CYCLES)
#define MAX_CVP_ISE_CYCLES		(MSM_VIDC_NOMINAL_CYCLES - \
		MSM_VIDC_UHD60E_ISE_CYCLES)

static void print_client_buffer(u32 tag, const char *str,
		struct msm_vidc_inst *inst, struct msm_cvp_buffer *cbuf)
{
	if (!(tag & msm_vidc_debug) || !inst || !cbuf)
		return;

	dprintk(tag,
		"%s: %x : idx %2d fd %d off %d size %d type %d flags 0x%x\n",
		str, hash32_ptr(inst->session), cbuf->index, cbuf->fd,
		cbuf->offset, cbuf->size, cbuf->type, cbuf->flags);
}

static void print_cvp_buffer(u32 tag, const char *str,
		struct msm_vidc_inst *inst, struct msm_vidc_cvp_buffer *cbuf)
{
	if (!(tag & msm_vidc_debug) || !inst || !cbuf)
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
		dprintk(VIDC_ERR, "%s: unknown buffer type %#x\n",
			str, type);

	return buftype;
}

void handle_session_register_buffer_done(enum hal_command_response cmd,
		void *resp)
{
	struct msm_vidc_cb_cmd_done *response = resp;
	struct msm_vidc_inst *inst;
	struct msm_vidc_cvp_buffer *cbuf;
	struct v4l2_event event = {0};
	u32 *data;
	bool found;

	if (!response) {
		dprintk(VIDC_ERR, "%s: invalid response\n", __func__);
		return;
	}
	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid session %pK\n", __func__,
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
		dprintk(VIDC_ERR, "%s: client_data %x not found\n",
			__func__, response->data.regbuf.client_data);
		goto exit;
	}
	print_cvp_buffer(VIDC_DBG, "register_done", inst, cbuf);

	event.type = V4L2_EVENT_MSM_VIDC_REGISTER_BUFFER_DONE;
	data = (u32 *)event.u.data;
	data[0] = cbuf->buf.index;
	data[1] = cbuf->buf.type;
	data[2] = cbuf->buf.fd;
	data[3] = cbuf->buf.offset;
	v4l2_event_queue_fh(&inst->event_handler, &event);

exit:
	put_inst(inst);
}

void handle_session_unregister_buffer_done(enum hal_command_response cmd,
		void *resp)
{
	int rc;
	struct msm_vidc_cb_cmd_done *response = resp;
	struct msm_vidc_inst *inst;
	struct msm_vidc_cvp_buffer *cbuf, *dummy;
	struct v4l2_event event = {0};
	u32 *data;
	bool found;

	if (!response) {
		dprintk(VIDC_ERR, "%s: invalid response\n", __func__);
		return;
	}
	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid session %pK\n", __func__,
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
		dprintk(VIDC_ERR, "%s: client_data %x not found\n",
			__func__, response->data.unregbuf.client_data);
		goto exit;
	}
	print_cvp_buffer(VIDC_DBG, "unregister_done", inst, cbuf);

	rc = msm_smem_unmap_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_cvp_buffer(VIDC_ERR, "unmap fail", inst, cbuf);
		goto exit;
	}

	event.type = V4L2_EVENT_MSM_VIDC_UNREGISTER_BUFFER_DONE;
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
	put_inst(inst);
}

static void print_cvp_cycles(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	struct msm_vidc_inst *temp;

	if (!inst || !inst->core)
		return;
	core = inst->core;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp->session_type == MSM_VIDC_CVP) {
			dprintk(VIDC_ERR, "session %#x, vpss %d ise %d\n",
				hash32_ptr(temp->session),
				temp->clk_data.vpss_cycles,
				temp->clk_data.ise_cycles);
		}
	}
	mutex_unlock(&core->lock);
}

static bool msm_cvp_check_session_supported(struct msm_vidc_inst *inst,
		u32 vpss_cycles, u32 ise_cycles)
{
	struct msm_vidc_core *core;
	struct msm_vidc_inst *temp;
	u32 total_vpss_cycles = 0;
	u32 total_ise_cycles = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return false;
	}
	core = inst->core;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp->session_type == MSM_VIDC_CVP) {
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

static int msm_cvp_scale_clocks_and_bus(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	rc = msm_vidc_set_clocks(inst->core);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed set_clocks for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

	rc = msm_comm_vote_bus(inst->core);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed vote_bus for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

exit:
	return rc;
}

static int msm_cvp_get_session_info(struct msm_vidc_inst *inst,
		struct msm_cvp_session_info *session)
{
	int rc = 0;

	if (!inst || !inst->core || !session) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	session->session_id = hash32_ptr(inst->session);
	dprintk(VIDC_DBG, "%s: id 0x%x\n", __func__, session->session_id);

	return rc;
}

static int msm_cvp_request_power(struct msm_vidc_inst *inst,
		struct msm_cvp_request_power *power)
{
	int rc = 0;

	if (!inst || !power) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dprintk(VIDC_DBG,
		"%s: clock_cycles_a %d, clock_cycles_b %d, ddr_bw %d sys_cache_bw %d\n",
		__func__, power->clock_cycles_a, power->clock_cycles_b,
		power->ddr_bw, power->sys_cache_bw);

	rc = msm_cvp_check_session_supported(inst, power->clock_cycles_a,
			power->clock_cycles_b);
	if (!rc) {
		dprintk(VIDC_ERR,
			"%s: session %#x rejected, cycles: vpss %d, ise %d\n",
			__func__, hash32_ptr(inst->session),
			power->clock_cycles_a, power->clock_cycles_b);
		print_cvp_cycles(inst);
		msm_comm_kill_session(inst);
		return -EOVERFLOW;
	}

	inst->clk_data.min_freq = max(power->clock_cycles_a,
		power->clock_cycles_b);
	/* convert client provided bps into kbps as expected by driver */
	inst->clk_data.ddr_bw = power->ddr_bw / 1000;
	inst->clk_data.sys_cache_bw = power->sys_cache_bw / 1000;
	rc = msm_cvp_scale_clocks_and_bus(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed to scale clocks and bus for inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));
		goto exit;
	}

	if (!inst->clk_data.min_freq && !inst->clk_data.ddr_bw &&
		!inst->clk_data.sys_cache_bw) {
		rc = msm_cvp_inst_pause(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: failed to pause inst %pK (%#x)\n",
				__func__, inst, hash32_ptr(inst->session));
			goto exit;
		}
	} else {
		rc = msm_cvp_inst_resume(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: failed to resume inst %pK (%#x)\n",
				__func__, inst, hash32_ptr(inst->session));
			goto exit;
		}
	}

exit:
	return rc;
}

static int msm_cvp_register_buffer(struct msm_vidc_inst *inst,
		struct msm_cvp_buffer *buf)
{
	int rc = 0;
	bool found;
	struct hfi_device *hdev;
	struct msm_vidc_cvp_buffer *cbuf;
	struct vidc_register_buffer vbuf;

	if (!inst || !inst->core || !buf) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	print_client_buffer(VIDC_DBG, "register", inst, buf);

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
		print_client_buffer(VIDC_ERR, "duplicate", inst, buf);
		return -EINVAL;
	}

	cbuf = kzalloc(sizeof(struct msm_vidc_cvp_buffer), GFP_KERNEL);
	if (!cbuf) {
		dprintk(VIDC_ERR, "%s: cbuf alloc failed\n", __func__);
		return -ENOMEM;
	}

	memcpy(&cbuf->buf, buf, sizeof(struct msm_cvp_buffer));
	cbuf->smem.buffer_type = get_hal_buftype(__func__, buf->type);
	cbuf->smem.fd = buf->fd;
	cbuf->smem.offset = buf->offset;
	cbuf->smem.size = buf->size;
	rc = msm_smem_map_dma_buf(inst, &cbuf->smem);
	if (rc) {
		print_client_buffer(VIDC_ERR, "map failed", inst, buf);
		goto exit;
	}

	memset(&vbuf, 0, sizeof(struct vidc_register_buffer));
	vbuf.index = buf->index;
	vbuf.type = get_hal_buftype(__func__, buf->type);
	vbuf.size = buf->size;
	vbuf.device_addr = cbuf->smem.device_addr;
	vbuf.client_data = cbuf->smem.device_addr;
	vbuf.response_required = true;
	rc = call_hfi_op(hdev, session_register_buffer,
			(void *)inst->session, &vbuf);
	if (rc) {
		print_cvp_buffer(VIDC_ERR, "register failed", inst, cbuf);
		goto exit;
	}
	mutex_lock(&inst->cvpbufs.lock);
	list_add_tail(&cbuf->list, &inst->cvpbufs.list);
	mutex_unlock(&inst->cvpbufs.lock);
	return rc;

exit:
	if (cbuf->smem.device_addr)
		msm_smem_unmap_dma_buf(inst, &cbuf->smem);
	kfree(cbuf);
	cbuf = NULL;

	return rc;
}

static int msm_cvp_unregister_buffer(struct msm_vidc_inst *inst,
		struct msm_cvp_buffer *buf)
{
	int rc = 0;
	bool found;
	struct hfi_device *hdev;
	struct msm_vidc_cvp_buffer *cbuf;
	struct vidc_unregister_buffer vbuf;

	if (!inst || !inst->core || !buf) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	print_client_buffer(VIDC_DBG, "unregister", inst, buf);

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
		print_client_buffer(VIDC_ERR, "invalid", inst, buf);
		return -EINVAL;
	}

	memset(&vbuf, 0, sizeof(struct vidc_unregister_buffer));
	vbuf.index = cbuf->buf.index;
	vbuf.type = get_hal_buftype(__func__, cbuf->buf.type);
	vbuf.size = cbuf->buf.size;
	vbuf.device_addr = cbuf->smem.device_addr;
	vbuf.client_data = cbuf->smem.device_addr;
	vbuf.response_required = true;
	rc = call_hfi_op(hdev, session_unregister_buffer,
			(void *)inst->session, &vbuf);
	if (rc)
		print_cvp_buffer(VIDC_ERR, "unregister failed", inst, cbuf);

	return rc;
}

int msm_vidc_cvp(struct msm_vidc_inst *inst, struct msm_vidc_arg *arg)
{
	int rc = 0;

	if (!inst || !arg) {
		dprintk(VIDC_ERR, "%s: invalid args\n", __func__);
		return -EINVAL;
	}

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
	default:
		dprintk(VIDC_ERR, "%s: unknown arg type 0x%x\n",
				__func__, arg->type);
		rc = -ENOTSUPP;
		break;
	}

	return rc;
}

static struct msm_vidc_ctrl msm_cvp_ctrls[] = {
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

int msm_cvp_ctrl_init(struct msm_vidc_inst *inst,
		const struct v4l2_ctrl_ops *ctrl_ops)
{
	return msm_comm_ctrl_init(inst, msm_cvp_ctrls,
		ARRAY_SIZE(msm_cvp_ctrls), ctrl_ops);
}

int msm_cvp_inst_pause(struct msm_vidc_inst *inst)
{
	int rc;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = call_hfi_op(hdev, session_pause, (void *)inst->session);
	if (rc)
		dprintk(VIDC_ERR, "%s: failed to pause inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));

	return rc;
}

int msm_cvp_inst_resume(struct msm_vidc_inst *inst)
{
	int rc;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	rc = call_hfi_op(hdev, session_resume, (void *)inst->session);
	if (rc)
		dprintk(VIDC_ERR, "%s: failed to resume inst %pK (%#x)\n",
			__func__, inst, hash32_ptr(inst->session));

	return rc;
}

int msm_cvp_inst_deinit(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_cvp_buffer *cbuf, *temp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	dprintk(VIDC_DBG, "%s: inst %pK (%#x)\n", __func__,
		inst, hash32_ptr(inst->session));

	rc = msm_comm_try_state(inst, MSM_VIDC_CLOSE_DONE);
	if (rc)
		dprintk(VIDC_ERR, "%s: close failed\n", __func__);

	mutex_lock(&inst->cvpbufs.lock);
	list_for_each_entry_safe(cbuf, temp, &inst->cvpbufs.list, list) {
		print_cvp_buffer(VIDC_ERR, "unregistered", inst, cbuf);
		rc = msm_smem_unmap_dma_buf(inst, &cbuf->smem);
		if (rc)
			dprintk(VIDC_ERR, "%s: unmap failed\n", __func__);
		list_del(&cbuf->list);
		kfree(cbuf);
	}
	mutex_unlock(&inst->cvpbufs.lock);

	inst->clk_data.min_freq = 0;
	inst->clk_data.ddr_bw = 0;
	inst->clk_data.sys_cache_bw = 0;
	rc = msm_cvp_scale_clocks_and_bus(inst);
	if (rc)
		dprintk(VIDC_ERR, "%s: failed to scale_clocks_and_bus\n",
			__func__);

	return rc;
}

int msm_cvp_inst_init(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	dprintk(VIDC_DBG, "%s: inst %pK (%#x)\n", __func__,
		inst, hash32_ptr(inst->session));

	/* set default frequency */
	inst->clk_data.core_id = VIDC_CORE_ID_2;
	inst->clk_data.min_freq = 1000;
	inst->clk_data.ddr_bw = 1000;
	inst->clk_data.sys_cache_bw = 1000;

	return rc;
}
