// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <soc/qcom/subsystem_restart.h>
#include <asm/div64.h>
#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_clocks.h"
#include "msm_cvp.h"

#define IS_ALREADY_IN_STATE(__p, __d) (\
	(__p >= __d)\
)

static void handle_session_error(enum hal_command_response cmd, void *data);

enum multi_stream msm_cvp_comm_get_stream_output_mode(struct msm_cvp_inst *inst)
{
	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params, return default mode\n",
			__func__);
		return HAL_VIDEO_DECODER_PRIMARY;
	}

	if (!is_decode_session(inst))
		return HAL_VIDEO_DECODER_PRIMARY;

	if (inst->stream_output_mode == HAL_VIDEO_DECODER_SECONDARY)
		return HAL_VIDEO_DECODER_SECONDARY;
	else
		return HAL_VIDEO_DECODER_PRIMARY;
}

int msm_cvp_comm_get_inst_load(struct msm_cvp_inst *inst,
		enum load_calc_quirks quirks)
{
	return 0;
}

int msm_cvp_comm_get_inst_load_per_core(struct msm_cvp_inst *inst,
		enum load_calc_quirks quirks)
{
	int load = msm_cvp_comm_get_inst_load(inst, quirks);

	if (inst->clk_data.core_id == CVP_CORE_ID_3)
		load = load / 2;

	return load;
}

enum hal_domain get_cvp_hal_domain(int session_type)
{
	enum hal_domain domain;

	switch (session_type) {
	case MSM_CVP_CORE:
		domain = HAL_VIDEO_DOMAIN_CVP;
		break;
	default:
		dprintk(CVP_ERR, "Wrong domain %d\n", session_type);
		domain = HAL_UNUSED_DOMAIN;
		break;
	}

	return domain;
}

enum hal_video_codec get_cvp_hal_codec(int fourcc)
{
	enum hal_video_codec codec;

	switch (fourcc) {
	case V4L2_PIX_FMT_CVP:
		codec = HAL_VIDEO_CODEC_CVP;
		break;
	default:
		dprintk(CVP_ERR, "Wrong codec: %#x\n", fourcc);
		codec = HAL_UNUSED_CODEC;
		break;
	}

	return codec;
}

struct msm_cvp_core *get_cvp_core(int core_id)
{
	struct msm_cvp_core *core;
	int found = 0;

	if (core_id > MSM_CVP_CORES_MAX) {
		dprintk(CVP_ERR, "Core id = %d is greater than max = %d\n",
			core_id, MSM_CVP_CORES_MAX);
		return NULL;
	}
	mutex_lock(&cvp_driver->lock);
	list_for_each_entry(core, &cvp_driver->cores, list) {
		if (core->id == core_id) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&cvp_driver->lock);
	if (found)
		return core;
	return NULL;
}

struct buf_queue *msm_cvp_comm_get_vb2q(
		struct msm_cvp_inst *inst, enum v4l2_buf_type type)
{
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return &inst->bufq[CAPTURE_PORT];
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return &inst->bufq[OUTPUT_PORT];
	return NULL;
}

static void handle_sys_init_done(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_core *core;
	struct cvp_hal_sys_init_done *sys_init_msg;
	u32 index;

	if (!IS_HAL_SYS_CMD(cmd)) {
		dprintk(CVP_ERR, "%s - invalid cmd\n", __func__);
		return;
	}

	index = SYS_MSG_INDEX(cmd);

	if (!response) {
		dprintk(CVP_ERR,
			"Failed to get valid response for sys init\n");
		return;
	}
	core = get_cvp_core(response->device_id);
	if (!core) {
		dprintk(CVP_ERR, "Wrong device_id received\n");
		return;
	}
	sys_init_msg = &response->data.sys_init_done;
	if (!sys_init_msg) {
		dprintk(CVP_ERR, "sys_init_done message not proper\n");
		return;
	}

	core->enc_codec_supported = sys_init_msg->enc_codec_supported;
	core->dec_codec_supported = sys_init_msg->dec_codec_supported;

	/* This should come from sys_init_done */
	core->resources.max_inst_count =
		sys_init_msg->max_sessions_supported ?
		min_t(u32, sys_init_msg->max_sessions_supported,
		MAX_SUPPORTED_INSTANCES) : MAX_SUPPORTED_INSTANCES;

	core->resources.max_secure_inst_count =
		core->resources.max_secure_inst_count ?
		core->resources.max_secure_inst_count :
		core->resources.max_inst_count;

	if (core->id == MSM_CORE_CVP &&
		(core->dec_codec_supported & HAL_VIDEO_CODEC_H264))
		core->dec_codec_supported |=
			HAL_VIDEO_CODEC_MVC;

	core->codec_count = sys_init_msg->codec_count;
	memcpy(core->capabilities, sys_init_msg->capabilities,
		sys_init_msg->codec_count * sizeof(struct msm_cvp_capability));

	dprintk(CVP_DBG,
		"%s: supported_codecs[%d]: enc = %#x, dec = %#x\n",
		__func__, core->codec_count, core->enc_codec_supported,
		core->dec_codec_supported);

	complete(&(core->completions[index]));
}

static void put_inst_helper(struct kref *kref)
{
	struct msm_cvp_inst *inst = container_of(kref,
			struct msm_cvp_inst, kref);

	msm_cvp_destroy(inst);
}

void cvp_put_inst(struct msm_cvp_inst *inst)
{
	if (!inst)
		return;

	kref_put(&inst->kref, put_inst_helper);
}

struct msm_cvp_inst *cvp_get_inst(struct msm_cvp_core *core,
		void *session_id)
{
	struct msm_cvp_inst *inst = NULL;
	bool matches = false;

	if (!core || !session_id)
		return NULL;

	mutex_lock(&core->lock);
	/*
	 * This is as good as !list_empty(!inst->list), but at this point
	 * we don't really know if inst was kfree'd via close syscall before
	 * hardware could respond.  So manually walk thru the list of active
	 * sessions
	 */
	list_for_each_entry(inst, &core->instances, list) {
		if (inst == session_id) {
			/*
			 * Even if the instance is valid, we really shouldn't
			 * be receiving or handling callbacks when we've deleted
			 * our session with HFI
			 */
			matches = !!inst->session;
			break;
		}
	}

	/*
	 * kref_* is atomic_int backed, so no need for inst->lock.  But we can
	 * always acquire inst->lock and release it in cvp_put_inst
	 * for a stronger locking system.
	 */
	inst = (matches && kref_get_unless_zero(&inst->kref)) ? inst : NULL;
	mutex_unlock(&core->lock);

	return inst;
}

static void cvp_handle_session_cmd_done(enum hal_command_response cmd,
	void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst;

	if (!response) {
		dprintk(CVP_ERR, "%s: Invalid release_buf_done response\n",
			__func__);
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_ERR, "%s: Got response for an inactive session\n",
			__func__);
		return;
	}

	dprintk(CVP_DBG, "%s: inst=%pK\n", __func__, inst);

	if (IS_HAL_SESSION_CMD(cmd)) {
		dprintk(CVP_INFO, "%s: calling completion for id=%d",
			__func__, SESSION_MSG_INDEX(cmd));
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	} else
		dprintk(CVP_ERR,
			"%s: Invalid inst cmd response: %d\n", __func__, cmd);
	cvp_put_inst(inst);
}

static void handle_session_set_buf_done(enum hal_command_response cmd,
	void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst;

	if (!response) {
		dprintk(CVP_ERR, "Invalid set_buf_done response\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN, "set_buf_done has an inactive session\n");
		return;
	}

	if (response->status) {
		dprintk(CVP_ERR,
			"set ARP buffer error from FW : %#x\n",
			response->status);
	}

	if (IS_HAL_SESSION_CMD(cmd))
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	else
		dprintk(CVP_ERR, "set_buf_done: invalid cmd: %d\n", cmd);
	cvp_put_inst(inst);

}

static void handle_session_release_buf_done(enum hal_command_response cmd,
	void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst;
	struct internal_buf *buf;
	struct list_head *ptr, *next;
	struct hal_buffer_info *buffer;
	u32 buf_found = false;
	u32 address;

	if (!response) {
		dprintk(CVP_ERR, "Invalid release_buf_done response\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN, "Got a response for an inactive session\n");
		return;
	}

	buffer = &response->data.buffer_info;
	address = buffer->buffer_addr;

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		if (address == buf->smem.device_addr) {
			dprintk(CVP_DBG, "releasing persist: %#x\n",
					buf->smem.device_addr);
			buf_found = true;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	if (!buf_found)
		dprintk(CVP_WARN, "invalid buffer %#x from firmware\n",
				address);
	if (IS_HAL_SESSION_CMD(cmd))
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	else
		dprintk(CVP_ERR, "Invalid inst cmd response: %d\n", cmd);

	cvp_put_inst(inst);
}

static void handle_sys_release_res_done(
		enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_core *core;

	if (!response) {
		dprintk(CVP_ERR,
			"Failed to get valid response for sys init\n");
		return;
	}
	core = get_cvp_core(response->device_id);
	if (!core) {
		dprintk(CVP_ERR, "Wrong device_id received\n");
		return;
	}
	complete(&core->completions[
			SYS_MSG_INDEX(HAL_SYS_RELEASE_RESOURCE_DONE)]);
}

void change_cvp_inst_state(struct msm_cvp_inst *inst, enum instance_state state)
{
	if (!inst) {
		dprintk(CVP_ERR, "Invalid parameter %s\n", __func__);
		return;
	}
	mutex_lock(&inst->lock);
	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_DBG,
			"Inst: %pK is in bad state can't change state to %d\n",
			inst, state);
		goto exit;
	}
	dprintk(CVP_DBG, "Moved inst: %pK from state: %d to state: %d\n",
		   inst, inst->state, state);
	inst->state = state;
exit:
	mutex_unlock(&inst->lock);
}

static int signal_session_msg_receipt(enum hal_command_response cmd,
		struct msm_cvp_inst *inst)
{
	if (!inst) {
		dprintk(CVP_ERR, "Invalid(%pK) instance id\n", inst);
		return -EINVAL;
	}
	if (IS_HAL_SESSION_CMD(cmd)) {
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	} else {
		dprintk(CVP_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

int wait_for_sess_signal_receipt(struct msm_cvp_inst *inst,
	enum hal_command_response cmd)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!IS_HAL_SESSION_CMD(cmd)) {
		dprintk(CVP_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	hdev = (struct hfi_device *)(inst->core->device);
	rc = wait_for_completion_timeout(
		&inst->completions[SESSION_MSG_INDEX(cmd)],
		msecs_to_jiffies(
			inst->core->resources.msm_cvp_hw_rsp_timeout));
	if (!rc) {
		dprintk(CVP_ERR, "Wait interrupted or timed out: %d\n",
				SESSION_MSG_INDEX(cmd));
		msm_cvp_comm_kill_session(inst);
		rc = -EIO;
	} else {
		rc = 0;
	}
	return rc;
}

static int wait_for_state(struct msm_cvp_inst *inst,
	enum instance_state flipped_state,
	enum instance_state desired_state,
	enum hal_command_response hal_cmd)
{
	int rc = 0;

	if (IS_ALREADY_IN_STATE(flipped_state, desired_state)) {
		dprintk(CVP_INFO, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto err_same_state;
	}
	dprintk(CVP_DBG, "Waiting for hal_cmd: %d\n", hal_cmd);
	rc = wait_for_sess_signal_receipt(inst, hal_cmd);
	if (!rc)
		change_cvp_inst_state(inst, desired_state);
err_same_state:
	return rc;
}

void msm_cvp_queue_v4l2_event(struct msm_cvp_inst *inst, int event_type)
{
	struct v4l2_event event = {.id = 0, .type = event_type};

	v4l2_event_queue_fh(&inst->event_handler, &event);
}

static void msm_cvp_comm_generate_max_clients_error(struct msm_cvp_inst *inst)
{
	enum hal_command_response cmd = HAL_SESSION_ERROR;
	struct msm_cvp_cb_cmd_done response = {0};

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	dprintk(CVP_ERR, "%s: Too many clients\n", __func__);
	response.session_id = inst;
	response.status = CVP_ERR_MAX_CLIENTS;
	handle_session_error(cmd, (void *)&response);
}

static void handle_session_init_done(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst = NULL;

	if (!response) {
		dprintk(CVP_ERR,
				"Failed to get valid response for session init\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
		response->session_id);

	if (!inst) {
		dprintk(CVP_WARN, "Got a response for an inactive session\n");
		return;
	}

	if (response->status) {
		dprintk(CVP_ERR,
			"Session init response from FW : %#x\n",
			response->status);
		if (response->status == CVP_ERR_MAX_CLIENTS)
			msm_cvp_comm_generate_max_clients_error(inst);
		else
			msm_cvp_comm_generate_session_error(inst);

		signal_session_msg_receipt(cmd, inst);
		cvp_put_inst(inst);
		return;
	}

	if (inst->session_type == MSM_CVP_CORE) {
		dprintk(CVP_DBG, "%s: cvp session %#x\n",
			__func__, hash32_ptr(inst->session));
		signal_session_msg_receipt(cmd, inst);
		cvp_put_inst(inst);
		return;
	}

	dprintk(CVP_ERR, "%s Session type must be CVP\n", __func__);
	return;
}

static void handle_event_change(enum hal_command_response cmd, void *data)
{
	dprintk(CVP_WARN, "%s is not supported on CVP!\n", __func__);
}

static void handle_release_res_done(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst;

	if (!response) {
		dprintk(CVP_ERR,
			"Failed to get valid response for release resource\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	cvp_put_inst(inst);
}

static void handle_session_flush(enum hal_command_response cmd, void *data)
{
	dprintk(CVP_WARN, "%s is not supported on CVP!\n", __func__);
}

static void handle_session_error(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct hfi_device *hdev = NULL;
	struct msm_cvp_inst *inst = NULL;
	int event = V4L2_EVENT_MSM_CVP_SYS_ERROR;

	if (!response) {
		dprintk(CVP_ERR,
			"Failed to get valid response for session error\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN, "Got a response for an inactive session\n");
		return;
	}

	hdev = inst->core->device;
	dprintk(CVP_ERR, "Session error received for inst %pK session %x\n",
		inst, hash32_ptr(inst->session));

	if (response->status == CVP_ERR_MAX_CLIENTS) {
		dprintk(CVP_WARN, "Too many clients, rejecting %pK", inst);
		event = V4L2_EVENT_MSM_CVP_MAX_CLIENTS;

		/*
		 * Clean the HFI session now. Since inst->state is moved to
		 * INVALID, forward thread doesn't know FW has valid session
		 * or not. This is the last place driver knows that there is
		 * no session in FW. Hence clean HFI session now.
		 */

		msm_cvp_comm_session_clean(inst);
	} else if (response->status == CVP_ERR_NOT_SUPPORTED) {
		dprintk(CVP_WARN, "Unsupported bitstream in %pK", inst);
		event = V4L2_EVENT_MSM_CVP_HW_UNSUPPORTED;
	} else {
		dprintk(CVP_WARN, "Unknown session error (%d) for %pK\n",
				response->status, inst);
		event = V4L2_EVENT_MSM_CVP_SYS_ERROR;
	}

	/* change state before sending error to client */
	change_cvp_inst_state(inst, MSM_CVP_CORE_INVALID);
	msm_cvp_queue_v4l2_event(inst, event);
	cvp_put_inst(inst);
}

static void msm_comm_clean_notify_client(struct msm_cvp_core *core)
{
	struct msm_cvp_inst *inst = NULL;

	if (!core) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return;
	}

	dprintk(CVP_WARN, "%s: Core %pK\n", __func__, core);
	mutex_lock(&core->lock);

	list_for_each_entry(inst, &core->instances, list) {
		mutex_lock(&inst->lock);
		inst->state = MSM_CVP_CORE_INVALID;
		mutex_unlock(&inst->lock);
		dprintk(CVP_WARN,
			"%s Send sys error for inst %pK\n", __func__, inst);
		msm_cvp_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_CVP_SYS_ERROR);
	}
	mutex_unlock(&core->lock);
}

static void handle_sys_error(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_core *core = NULL;
	struct hfi_device *hdev = NULL;
	struct msm_cvp_inst *inst = NULL;
	int rc = 0;

	subsystem_crashed("cvpss");
	if (!response) {
		dprintk(CVP_ERR,
			"Failed to get valid response for sys error\n");
		return;
	}

	core = get_cvp_core(response->device_id);
	if (!core) {
		dprintk(CVP_ERR,
				"Got SYS_ERR but unable to identify core\n");
		return;
	}
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == CVP_CORE_UNINIT) {
		dprintk(CVP_ERR,
			"%s: Core %pK already moved to state %d\n",
			 __func__, core, core->state);
		mutex_unlock(&core->lock);
		return;
	}

	dprintk(CVP_WARN, "SYS_ERROR received for core %pK\n", core);
	/* msm_cvp_noc_error_info(core) is disabled as of now */
	call_hfi_op(hdev, flush_debug_queue, hdev->hfi_device_data);
	list_for_each_entry(inst, &core->instances, list) {
		dprintk(CVP_WARN,
			"%s: Send sys error for inst %pK\n", __func__, inst);
		change_cvp_inst_state(inst, MSM_CVP_CORE_INVALID);
		msm_cvp_queue_v4l2_event(inst, V4L2_EVENT_MSM_CVP_SYS_ERROR);
		if (!core->trigger_ssr)
			msm_cvp_comm_print_inst_info(inst);
	}

	/* handle the hw error before core released to get full debug info */
	msm_cvp_handle_hw_error(core);
	if (response->status == CVP_ERR_NOC_ERROR) {
		dprintk(CVP_WARN, "Got NOC error");
		MSM_CVP_ERROR(true);
	}

	dprintk(CVP_DBG, "Calling core_release\n");
	rc = call_hfi_op(hdev, core_release, hdev->hfi_device_data);
	if (rc) {
		dprintk(CVP_ERR, "core_release failed\n");
		mutex_unlock(&core->lock);
		return;
	}
	core->state = CVP_CORE_UNINIT;
	mutex_unlock(&core->lock);

	dprintk(CVP_WARN, "SYS_ERROR handled.\n");
}

void msm_cvp_comm_session_clean(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev = NULL;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid params\n", __func__);
		return;
	}
	if (!inst->session) {
		dprintk(CVP_DBG, "%s: inst %pK session already cleaned\n",
			__func__, inst);
		return;
	}

	hdev = inst->core->device;
	mutex_lock(&inst->lock);
	dprintk(CVP_DBG, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_clean,
			(void *)inst->session);
	if (rc) {
		dprintk(CVP_ERR,
			"Session clean failed :%pK\n", inst);
	}
	inst->session = NULL;
	mutex_unlock(&inst->lock);
}

static void handle_session_close(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst;

	if (!response) {
		dprintk(CVP_ERR,
			"Failed to get valid response for session close\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	show_stats(inst);
	cvp_put_inst(inst);
}

static void handle_operation_config(enum hal_command_response cmd, void *data)
{
	dprintk(CVP_ERR,
			"%s: is called\n",
			__func__);
}

enum hal_buffer msm_cvp_comm_get_hal_output_buffer(struct msm_cvp_inst *inst)
{
	if (msm_cvp_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY)
		return HAL_BUFFER_OUTPUT2;
	else
		return HAL_BUFFER_OUTPUT;
}

void cvp_handle_cmd_response(enum hal_command_response cmd, void *data)
{
	dprintk(CVP_DBG, "Command response = %d\n", cmd);
	switch (cmd) {
	case HAL_SYS_INIT_DONE:
		handle_sys_init_done(cmd, data);
		break;
	case HAL_SYS_RELEASE_RESOURCE_DONE:
		handle_sys_release_res_done(cmd, data);
		break;
	case HAL_SESSION_INIT_DONE:
		handle_session_init_done(cmd, data);
		break;
	case HAL_SESSION_CVP_OPERATION_CONFIG:
		handle_operation_config(cmd, data);
		break;
	case HAL_SESSION_RELEASE_RESOURCE_DONE:
		handle_release_res_done(cmd, data);
		break;
	case HAL_SESSION_END_DONE:
	case HAL_SESSION_ABORT_DONE:
		handle_session_close(cmd, data);
		break;
	case HAL_SESSION_EVENT_CHANGE:
		handle_event_change(cmd, data);
		break;
	case HAL_SESSION_FLUSH_DONE:
		handle_session_flush(cmd, data);
		break;
	case HAL_SYS_WATCHDOG_TIMEOUT:
	case HAL_SYS_ERROR:
		handle_sys_error(cmd, data);
		break;
	case HAL_SESSION_ERROR:
		handle_session_error(cmd, data);
		break;
	case HAL_SESSION_SET_BUFFER_DONE:
		handle_session_set_buf_done(cmd, data);
		break;
	case HAL_SESSION_RELEASE_BUFFER_DONE:
		handle_session_release_buf_done(cmd, data);
		break;
	case HAL_SESSION_DFS_CONFIG_CMD_DONE:
	case HAL_SESSION_DFS_FRAME_CMD_DONE:
	case HAL_SESSION_DME_CONFIG_CMD_DONE:
	case HAL_SESSION_DME_BASIC_CONFIG_CMD_DONE:
	case HAL_SESSION_DME_FRAME_CMD_DONE:
	case HAL_SESSION_PERSIST_CMD_DONE:
		cvp_handle_session_cmd_done(cmd, data);
		break;
	default:
		dprintk(CVP_DBG, "response unhandled: %d\n", cmd);
		break;
	}
}

static inline enum msm_cvp_thermal_level msm_comm_cvp_thermal_level(int level)
{
	switch (level) {
	case 0:
		return CVP_THERMAL_NORMAL;
	case 1:
		return CVP_THERMAL_LOW;
	case 2:
		return CVP_THERMAL_HIGH;
	default:
		return CVP_THERMAL_CRITICAL;
	}
}

static bool is_core_turbo(struct msm_cvp_core *core, unsigned long freq)
{
	int i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u32 max_freq = 0;

	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	for (i = 0; i < core->resources.allowed_clks_tbl_size; i++) {
		if (max_freq < allowed_clks_tbl[i].clock_rate)
			max_freq = allowed_clks_tbl[i].clock_rate;
	}
	return freq >= max_freq;
}

static bool is_thermal_permissible(struct msm_cvp_core *core)
{
	enum msm_cvp_thermal_level tl;
	unsigned long freq = 0;
	bool is_turbo = false;

	if (!core->resources.thermal_mitigable)
		return true;

	if (msm_cvp_thermal_mitigation_disabled) {
		dprintk(CVP_DBG,
			"Thermal mitigation not enabled. debugfs %d\n",
			msm_cvp_thermal_mitigation_disabled);
		return true;
	}

	tl = msm_comm_cvp_thermal_level(cvp_driver->thermal_level);
	freq = core->curr_freq;

	is_turbo = is_core_turbo(core, freq);
	dprintk(CVP_DBG,
		"Core freq %ld Thermal level %d Turbo mode %d\n",
		freq, tl, is_turbo);

	if (is_turbo && tl >= CVP_THERMAL_LOW) {
		dprintk(CVP_ERR,
			"Video session not allowed. Turbo mode %d Thermal level %d\n",
			is_turbo, tl);
		return false;
	}
	return true;
}

static int msm_comm_session_abort(struct msm_cvp_inst *inst)
{
	int rc = 0, abort_completion = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	abort_completion = SESSION_MSG_INDEX(HAL_SESSION_ABORT_DONE);

	dprintk(CVP_WARN, "%s: inst %pK session %x\n", __func__,
		inst, hash32_ptr(inst->session));
	rc = call_hfi_op(hdev, session_abort, (void *)inst->session);
	if (rc) {
		dprintk(CVP_ERR,
			"%s session_abort failed rc: %d\n", __func__, rc);
		goto exit;
	}
	rc = wait_for_completion_timeout(
			&inst->completions[abort_completion],
			msecs_to_jiffies(
				inst->core->resources.msm_cvp_hw_rsp_timeout));
	if (!rc) {
		dprintk(CVP_ERR, "%s: inst %pK session %x abort timed out\n",
				__func__, inst, hash32_ptr(inst->session));
		msm_cvp_comm_generate_sys_error(inst);
		rc = -EBUSY;
	} else {
		rc = 0;
	}
exit:
	return rc;
}

static void handle_thermal_event(struct msm_cvp_core *core)
{
	int rc = 0;
	struct msm_cvp_inst *inst;

	if (!core || !core->device) {
		dprintk(CVP_ERR, "%s Invalid params\n", __func__);
		return;
	}
	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (!inst->session)
			continue;

		mutex_unlock(&core->lock);
		if (inst->state >= MSM_CVP_OPEN_DONE &&
			inst->state < MSM_CVP_CLOSE_DONE) {
			dprintk(CVP_WARN, "%s: abort inst %pK\n",
				__func__, inst);
			rc = msm_comm_session_abort(inst);
			if (rc) {
				dprintk(CVP_ERR,
					"%s session_abort failed rc: %d\n",
					__func__, rc);
				goto err_sess_abort;
			}
			change_cvp_inst_state(inst, MSM_CVP_CORE_INVALID);
			dprintk(CVP_WARN,
				"%s Send sys error for inst %pK\n",
				__func__, inst);
			msm_cvp_queue_v4l2_event(inst,
					V4L2_EVENT_MSM_CVP_SYS_ERROR);
		} else {
			msm_cvp_comm_generate_session_error(inst);
		}
		mutex_lock(&core->lock);
	}
	mutex_unlock(&core->lock);
	return;

err_sess_abort:
	msm_comm_clean_notify_client(core);
}

void msm_cvp_comm_handle_thermal_event(void)
{
	struct msm_cvp_core *core;

	list_for_each_entry(core, &cvp_driver->cores, list) {
		if (!is_thermal_permissible(core)) {
			dprintk(CVP_WARN,
				"Thermal level critical, stop all active sessions!\n");
			handle_thermal_event(core);
		}
	}
}

int msm_cvp_comm_check_core_init(struct msm_cvp_core *core)
{
	int rc = 0;

	mutex_lock(&core->lock);
	if (core->state >= CVP_CORE_INIT_DONE) {
		dprintk(CVP_INFO, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto exit;
	}
	dprintk(CVP_DBG, "Waiting for SYS_INIT_DONE\n");
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(HAL_SYS_INIT_DONE)],
		msecs_to_jiffies(core->resources.msm_cvp_hw_rsp_timeout));
	if (!rc) {
		dprintk(CVP_ERR, "%s: Wait interrupted or timed out: %d\n",
				__func__, SYS_MSG_INDEX(HAL_SYS_INIT_DONE));
		rc = -EIO;
		goto exit;
	} else {
		core->state = CVP_CORE_INIT_DONE;
		rc = 0;
	}
	dprintk(CVP_DBG, "SYS_INIT_DONE!!!\n");
exit:
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_comm_init_core_done(struct msm_cvp_inst *inst)
{
	int rc = 0;

	rc = msm_cvp_comm_check_core_init(inst->core);
	if (rc) {
		dprintk(CVP_ERR, "%s - failed to initialize core\n", __func__);
		msm_cvp_comm_generate_sys_error(inst);
		return rc;
	}
	change_cvp_inst_state(inst, MSM_CVP_CORE_INIT_DONE);
	return rc;
}

static int msm_comm_init_core(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_core *core;

	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;

	core = inst->core;
	hdev = core->device;
	mutex_lock(&core->lock);
	if (core->state >= CVP_CORE_INIT) {
		dprintk(CVP_DBG, "CVP core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_inited;
	}
	if (!core->capabilities) {
		core->capabilities = kcalloc(CVP_MAX_SESSIONS,
				sizeof(struct msm_cvp_capability), GFP_KERNEL);
		if (!core->capabilities) {
			dprintk(CVP_ERR,
				"%s: failed to allocate capabilities\n",
				__func__);
			rc = -ENOMEM;
			goto fail_cap_alloc;
		}
	} else {
		dprintk(CVP_WARN,
			"%s: capabilities memory is expected to be freed\n",
			__func__);
	}
	dprintk(CVP_DBG, "%s: core %pK\n", __func__, core);
	rc = call_hfi_op(hdev, core_init, hdev->hfi_device_data);
	if (rc) {
		dprintk(CVP_ERR, "Failed to init core, id = %d\n",
				core->id);
		goto fail_core_init;
	}
	core->state = CVP_CORE_INIT;
	core->smmu_fault_handled = false;
	core->trigger_ssr = false;

core_already_inited:
	change_cvp_inst_state(inst, MSM_CVP_CORE_INIT);
	mutex_unlock(&core->lock);

	rc = msm_cvp_comm_scale_clocks_and_bus(inst);
	return rc;

fail_core_init:
	kfree(core->capabilities);
fail_cap_alloc:
	core->capabilities = NULL;
	core->state = CVP_CORE_UNINIT;
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_cvp_deinit_core(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == CVP_CORE_UNINIT) {
		dprintk(CVP_INFO, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_uninited;
	}
	mutex_unlock(&core->lock);

	msm_cvp_comm_scale_clocks_and_bus(inst);

	mutex_lock(&core->lock);

	if (!core->resources.never_unload_fw) {
		cancel_delayed_work(&core->fw_unload_work);

		/*
		 * Delay unloading of firmware. This is useful
		 * in avoiding firmware download delays in cases where we
		 * will have a burst of back to back video playback sessions
		 * e.g. thumbnail generation.
		 */
		schedule_delayed_work(&core->fw_unload_work,
			msecs_to_jiffies(core->state == CVP_CORE_INIT_DONE ?
			core->resources.msm_cvp_firmware_unload_delay : 0));

		dprintk(CVP_DBG, "firmware unload delayed by %u ms\n",
			core->state == CVP_CORE_INIT_DONE ?
			core->resources.msm_cvp_firmware_unload_delay : 0);
	}

core_already_uninited:
	change_cvp_inst_state(inst, MSM_CVP_CORE_UNINIT);
	mutex_unlock(&core->lock);
	return 0;
}

int msm_cvp_comm_force_cleanup(struct msm_cvp_inst *inst)
{
	msm_cvp_comm_kill_session(inst);
	return msm_cvp_deinit_core(inst);
}

static int msm_comm_session_init_done(int flipped_state,
	struct msm_cvp_inst *inst)
{
	int rc;

	dprintk(CVP_DBG, "inst %pK: waiting for session init done\n", inst);
	rc = wait_for_state(inst, flipped_state, MSM_CVP_OPEN_DONE,
			HAL_SESSION_INIT_DONE);
	if (rc) {
		dprintk(CVP_ERR, "Session init failed for inst %pK\n", inst);
		msm_cvp_comm_generate_sys_error(inst);
		return rc;
	}

	return rc;
}

static int msm_comm_session_init(int flipped_state,
	struct msm_cvp_inst *inst)
{
	int rc = 0;
	int fourcc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (IS_ALREADY_IN_STATE(flipped_state, MSM_CVP_OPEN)) {
		dprintk(CVP_INFO, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	if (inst->session_type == MSM_CVP_CORE) {
		fourcc = V4L2_PIX_FMT_CVP;
	} else {
		dprintk(CVP_ERR, "Invalid session\n");
		return -EINVAL;
	}

	rc = msm_cvp_comm_init_clocks_and_bus_data(inst);
	if (rc) {
		dprintk(CVP_ERR, "Failed to initialize clocks and bus data\n");
		goto exit;
	}

	dprintk(CVP_DBG, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_init, hdev->hfi_device_data,
			inst, get_cvp_hal_domain(inst->session_type),
			get_cvp_hal_codec(fourcc),
			&inst->session);

	if (rc || !inst->session) {
		dprintk(CVP_ERR,
			"Failed to call session init for: %pK, %pK, %d, %d\n",
			inst->core->device, inst,
			inst->session_type, fourcc);
		rc = -EINVAL;
		goto exit;
	}
	change_cvp_inst_state(inst, MSM_CVP_OPEN);

exit:
	return rc;
}

static int msm_comm_session_close(int flipped_state,
			struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_CVP_CLOSE)) {
		dprintk(CVP_INFO,
			"inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	hdev = inst->core->device;
	dprintk(CVP_DBG, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_end, (void *) inst->session);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to send close\n");
		goto exit;
	}
	change_cvp_inst_state(inst, MSM_CVP_CLOSE);
exit:
	return rc;
}

int msm_cvp_comm_suspend(int core_id)
{
	struct hfi_device *hdev;
	struct msm_cvp_core *core;
	int rc = 0;

	core = get_cvp_core(core_id);
	if (!core) {
		dprintk(CVP_ERR,
			"%s: Failed to find core for core_id = %d\n",
			__func__, core_id);
		return -EINVAL;
	}

	hdev = (struct hfi_device *)core->device;
	if (!hdev) {
		dprintk(CVP_ERR, "%s Invalid device handle\n", __func__);
		return -EINVAL;
	}

	rc = call_hfi_op(hdev, suspend, hdev->hfi_device_data);
	if (rc)
		dprintk(CVP_WARN, "Failed to suspend\n");

	return rc;
}

static int get_flipped_state(int present_state,
	int desired_state)
{
	int flipped_state = present_state;

	if (flipped_state < MSM_CVP_STOP
			&& desired_state > MSM_CVP_STOP) {
		flipped_state = MSM_CVP_STOP + (MSM_CVP_STOP - flipped_state);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	} else if (flipped_state > MSM_CVP_STOP
			&& desired_state < MSM_CVP_STOP) {
		flipped_state = MSM_CVP_STOP -
			(flipped_state - MSM_CVP_STOP + 1);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	}
	return flipped_state;
}

struct hal_buffer_requirements *get_cvp_buff_req_buffer(
		struct msm_cvp_inst *inst, enum hal_buffer buffer_type)
{
	int i;

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		if (inst->buff_req.buffer[i].buffer_type == buffer_type)
			return &inst->buff_req.buffer[i];
	}
	dprintk(CVP_ERR, "Failed to get buff req for : %x", buffer_type);
	return NULL;
}

int msm_cvp_comm_try_state(struct msm_cvp_inst *inst, int state)
{
	int rc = 0;
	int flipped_state;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params %pK", __func__, inst);
		return -EINVAL;
	}
	dprintk(CVP_DBG,
		"Trying to move inst: %pK (%#x) from: %#x to %#x\n",
		inst, hash32_ptr(inst->session), inst->state, state);

	mutex_lock(&inst->sync_lock);
	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR, "%s: inst %pK is in invalid\n",
			__func__, inst);
		rc = -EINVAL;
		goto exit;
	}

	flipped_state = get_flipped_state(inst->state, state);
	dprintk(CVP_DBG,
		"inst: %pK (%#x) flipped_state = %#x\n",
		inst, hash32_ptr(inst->session), flipped_state);
	switch (flipped_state) {
	case MSM_CVP_CORE_UNINIT_DONE:
	case MSM_CVP_CORE_INIT:
		rc = msm_comm_init_core(inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_CORE_INIT_DONE:
		rc = msm_comm_init_core_done(inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_OPEN:
		rc = msm_comm_session_init(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_OPEN_DONE:
		rc = msm_comm_session_init_done(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_LOAD_RESOURCES:
		dprintk(CVP_WARN, "Deprecated state LOAD_RESOURCES\n");
	case MSM_CVP_LOAD_RESOURCES_DONE:
		dprintk(CVP_WARN, "Deprecated state LOAD_RESOURCES_DONE\n");
	case MSM_CVP_START:
		dprintk(CVP_WARN, "Deprecated state START\n");
	case MSM_CVP_START_DONE:
		dprintk(CVP_WARN, "Deprecated state START_DONE\n");
	case MSM_CVP_STOP:
		dprintk(CVP_WARN, "Deprecated state STOP\n");
	case MSM_CVP_STOP_DONE:
		dprintk(CVP_WARN, "Deprecated state STOP_DONE\n");
	case MSM_CVP_RELEASE_RESOURCES:
		dprintk(CVP_WARN, "Deprecated state RELEASE_SOURCES\n");
	case MSM_CVP_RELEASE_RESOURCES_DONE:
		dprintk(CVP_WARN, "Deprecated state RELEASE_RESOURCES_DONE\n");
	case MSM_CVP_CLOSE:
		rc = msm_comm_session_close(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_CLOSE_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_CVP_CLOSE_DONE,
				HAL_SESSION_END_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		msm_cvp_comm_session_clean(inst);
	case MSM_CVP_CORE_UNINIT:
	case MSM_CVP_CORE_INVALID:
		dprintk(CVP_DBG, "Sending core uninit\n");
		rc = msm_cvp_deinit_core(inst);
		if (rc || state == get_flipped_state(inst->state, state))
			break;
	default:
		dprintk(CVP_ERR, "State not recognized\n");
		rc = -EINVAL;
		break;
	}

exit:
	mutex_unlock(&inst->sync_lock);

	if (rc) {
		dprintk(CVP_ERR,
				"Failed to move from state: %d to %d\n",
				inst->state, state);
		msm_cvp_comm_kill_session(inst);
	} else {
		trace_msm_cvp_common_state_change((void *)inst,
				inst->state, state);
	}
	return rc;
}

enum hal_buffer cvp_get_hal_buffer_type(unsigned int type,
		unsigned int plane_num)
{
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (plane_num == 0)
			return HAL_BUFFER_INPUT;
		else
			return HAL_BUFFER_EXTRADATA_INPUT;
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (plane_num == 0)
			return HAL_BUFFER_OUTPUT;
		else
			return HAL_BUFFER_EXTRADATA_OUTPUT;
	} else {
		return -EINVAL;
	}
}

int msm_cvp_comm_num_queued_bufs(struct msm_cvp_inst *inst, u32 type)
{
	int count = 0;
	struct msm_video_buffer *mbuf;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return 0;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
		if (mbuf->vvb.vb2_buf.type != type)
			continue;
		if (!(mbuf->flags & MSM_CVP_FLAG_QUEUED))
			continue;
		count++;
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return count;
}

int msm_cvp_comm_set_buffer_count(struct msm_cvp_inst *inst,
	int host_count, int act_count, enum hal_buffer type)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct hal_buffer_count_actual buf_count;

	hdev = inst->core->device;

	buf_count.buffer_type = type;
	buf_count.buffer_count_actual = act_count;
	buf_count.buffer_count_min_host = host_count;
	dprintk(CVP_DBG, "%s: %x : hal_buffer %d min_host %d actual %d\n",
		__func__, hash32_ptr(inst->session), type,
		host_count, act_count);
	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HAL_PARAM_BUFFER_COUNT_ACTUAL, &buf_count);
	if (rc)
		dprintk(CVP_ERR,
			"Failed to set actual buffer count %d for buffer type %d\n",
			act_count, type);
	return rc;
}

int msm_cvp_noc_error_info(struct msm_cvp_core *core)
{
	struct hfi_device *hdev;

	if (!core || !core->device) {
		dprintk(CVP_WARN, "%s: Invalid parameters: %pK\n",
			__func__, core);
		return -EINVAL;
	}

	if (!core->resources.non_fatal_pagefaults)
		return 0;

	if (!core->smmu_fault_handled)
		return 0;

	hdev = core->device;
	call_hfi_op(hdev, noc_error_info, hdev->hfi_device_data);

	return 0;
}

int msm_cvp_trigger_ssr(struct msm_cvp_core *core,
	enum hal_ssr_trigger_type type)
{
	if (!core) {
		dprintk(CVP_WARN, "%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}
	core->ssr_type = type;
	schedule_work(&core->ssr_work);
	return 0;
}

void msm_cvp_ssr_handler(struct work_struct *work)
{
	int rc;
	struct msm_cvp_core *core;
	struct hfi_device *hdev;

	core = container_of(work, struct msm_cvp_core, ssr_work);
	if (!core || !core->device) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return;
	}
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == CVP_CORE_INIT_DONE) {
		dprintk(CVP_WARN, "%s: ssr type %d\n", __func__,
			core->ssr_type);
		/*
		 * In current implementation user-initiated SSR triggers
		 * a fatal error from hardware. However, there is no way
		 * to know if fatal error is due to SSR or not. Handle
		 * user SSR as non-fatal.
		 */
		core->trigger_ssr = true;
		rc = call_hfi_op(hdev, core_trigger_ssr,
				hdev->hfi_device_data, core->ssr_type);
		if (rc) {
			dprintk(CVP_ERR, "%s: trigger_ssr failed\n",
				__func__);
			core->trigger_ssr = false;
		}
	} else {
		dprintk(CVP_WARN, "%s: video core %pK not initialized\n",
			__func__, core);
	}
	mutex_unlock(&core->lock);
}

void msm_cvp_comm_generate_session_error(struct msm_cvp_inst *inst)
{
	enum hal_command_response cmd = HAL_SESSION_ERROR;
	struct msm_cvp_cb_cmd_done response = {0};

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	dprintk(CVP_WARN, "%s: inst %pK\n", __func__, inst);
	response.session_id = inst;
	response.status = CVP_ERR_FAIL;
	handle_session_error(cmd, (void *)&response);
}

void msm_cvp_comm_generate_sys_error(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core;
	enum hal_command_response cmd = HAL_SYS_ERROR;
	struct msm_cvp_cb_cmd_done response  = {0};

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	dprintk(CVP_WARN, "%s: inst %pK\n", __func__, inst);
	core = inst->core;
	response.device_id = (u32) core->id;
	handle_sys_error(cmd, (void *) &response);

}

int msm_cvp_comm_kill_session(struct msm_cvp_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid input parameters\n", __func__);
		return -EINVAL;
	} else if (!inst->session) {
		dprintk(CVP_ERR, "%s: no session to kill for inst %pK\n",
			__func__, inst);
		return 0;
	}

	dprintk(CVP_ERR, "%s: inst %pK, session %x state %d\n", __func__,
		inst, hash32_ptr(inst->session), inst->state);
	/*
	 * We're internally forcibly killing the session, if fw is aware of
	 * the session send session_abort to firmware to clean up and release
	 * the session, else just kill the session inside the driver.
	 */
	if ((inst->state >= MSM_CVP_OPEN_DONE &&
			inst->state < MSM_CVP_CLOSE_DONE) ||
			inst->state == MSM_CVP_CORE_INVALID) {
		rc = msm_comm_session_abort(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: inst %pK session %x abort failed\n",
				__func__, inst, hash32_ptr(inst->session));
			change_cvp_inst_state(inst, MSM_CVP_CORE_INVALID);
		}
	}

	change_cvp_inst_state(inst, MSM_CVP_CLOSE_DONE);
	msm_cvp_comm_session_clean(inst);

	dprintk(CVP_WARN, "%s: inst %pK session %x handled\n", __func__,
		inst, hash32_ptr(inst->session));
	return rc;
}

int msm_cvp_comm_smem_alloc(struct msm_cvp_inst *inst,
		size_t size, u32 align, u32 flags, enum hal_buffer buffer_type,
		int map_kernel, struct msm_smem *smem)
{
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(CVP_ERR, "%s: invalid inst: %pK\n", __func__, inst);
		return -EINVAL;
	}
	rc = msm_cvp_smem_alloc(size, align, flags, buffer_type, map_kernel,
				&(inst->core->resources), inst->session_type,
				smem);
	return rc;
}

void msm_cvp_comm_smem_free(struct msm_cvp_inst *inst, struct msm_smem *mem)
{
	if (!inst || !inst->core || !mem) {
		dprintk(CVP_ERR,
			"%s: invalid params: %pK %pK\n", __func__, inst, mem);
		return;
	}
	msm_cvp_smem_free(mem);
}

void msm_cvp_fw_unload_handler(struct work_struct *work)
{
	struct msm_cvp_core *core = NULL;
	struct hfi_device *hdev = NULL;
	int rc = 0;

	core = container_of(work, struct msm_cvp_core, fw_unload_work.work);
	if (!core || !core->device) {
		dprintk(CVP_ERR, "%s - invalid work or core handle\n",
				__func__);
		return;
	}

	hdev = core->device;

	mutex_lock(&core->lock);
	if (list_empty(&core->instances) &&
		core->state != CVP_CORE_UNINIT) {
		if (core->state > CVP_CORE_INIT) {
			dprintk(CVP_DBG, "Calling cvp_hal_core_release\n");
			rc = call_hfi_op(hdev, core_release,
					hdev->hfi_device_data);
			if (rc) {
				dprintk(CVP_ERR,
					"Failed to release core, id = %d\n",
					core->id);
				mutex_unlock(&core->lock);
				return;
			}
		}
		core->state = CVP_CORE_UNINIT;
		kfree(core->capabilities);
		core->capabilities = NULL;
	}
	mutex_unlock(&core->lock);
}

void msm_cvp_comm_print_inst_info(struct msm_cvp_inst *inst)
{
	struct msm_video_buffer *mbuf;
	struct internal_buf *buf;
	bool is_decode = false;
	enum cvp_ports port;
	bool is_secure = false;

	if (!inst) {
		dprintk(CVP_ERR, "%s - invalid param %pK\n",
			__func__, inst);
		return;
	}

	is_decode = inst->session_type == MSM_CVP_DECODER;
	port = is_decode ? OUTPUT_PORT : CAPTURE_PORT;
	is_secure = inst->flags & CVP_SECURE;
	dprintk(CVP_ERR,
			"---Buffer details for inst: %pK of type: %d---\n",
			inst, inst->session_type);
	mutex_lock(&inst->registeredbufs.lock);
	dprintk(CVP_ERR, "registered buffer list:\n");
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list)
		print_video_buffer(CVP_ERR, "buf", inst, mbuf);
	mutex_unlock(&inst->registeredbufs.lock);

	mutex_lock(&inst->persistbufs.lock);
	dprintk(CVP_ERR, "persist buffer list:\n");
	list_for_each_entry(buf, &inst->persistbufs.list, list)
		dprintk(CVP_ERR, "type: %d addr: %x size: %u\n",
				buf->buffer_type, buf->smem.device_addr,
				buf->smem.size);
	mutex_unlock(&inst->persistbufs.lock);
}

void print_video_buffer(u32 tag, const char *str, struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	struct vb2_buffer *vb2 = NULL;

	if (!(tag & msm_cvp_debug) || !inst || !mbuf)
		return;

	vb2 = &mbuf->vvb.vb2_buf;

	if (vb2->num_planes == 1)
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d daddr %x size %d filled %d flags 0x%x ts %lld refcnt %d mflags 0x%x\n",
			str, vb2->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, mbuf->smem[0].device_addr,
			vb2->planes[0].length, vb2->planes[0].bytesused,
			mbuf->vvb.flags, mbuf->vvb.vb2_buf.timestamp,
			mbuf->smem[0].refcount, mbuf->flags);
	else
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d daddr %x size %d filled %d flags 0x%x ts %lld refcnt %d mflags 0x%x, extradata: fd %d off %d daddr %x size %d filled %d refcnt %d\n",
			str, vb2->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, mbuf->smem[0].device_addr,
			vb2->planes[0].length, vb2->planes[0].bytesused,
			mbuf->vvb.flags, mbuf->vvb.vb2_buf.timestamp,
			mbuf->smem[0].refcount, mbuf->flags,
			vb2->planes[1].m.fd, vb2->planes[1].data_offset,
			mbuf->smem[1].device_addr, vb2->planes[1].length,
			vb2->planes[1].bytesused, mbuf->smem[1].refcount);
}

int msm_cvp_comm_unmap_video_buffer(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	int rc = 0, i;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}
	if (mbuf->vvb.vb2_buf.num_planes > VIDEO_MAX_PLANES) {
		dprintk(CVP_ERR, "%s: invalid num_planes %d\n", __func__,
			mbuf->vvb.vb2_buf.num_planes);
		return -EINVAL;
	}

	for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
		u32 refcount = mbuf->smem[i].refcount;

		while (refcount) {
			if (msm_cvp_smem_unmap_dma_buf(inst, &mbuf->smem[i]))
				print_video_buffer(CVP_ERR,
					"unmap failed for buf", inst, mbuf);
			refcount--;
		}
	}

	return rc;
}

static void kref_free_mbuf(struct kref *kref)
{
	struct msm_video_buffer *mbuf = container_of(kref,
			struct msm_video_buffer, kref);

	kfree(mbuf);
}

void kref_cvp_put_mbuf(struct msm_video_buffer *mbuf)
{
	if (!mbuf)
		return;

	kref_put(&mbuf->kref, kref_free_mbuf);
}

bool kref_cvp_get_mbuf(struct msm_cvp_inst *inst, struct msm_video_buffer *mbuf)
{
	struct msm_video_buffer *temp;
	bool matches = false;
	bool ret = false;

	if (!inst || !mbuf)
		return false;

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		if (temp == mbuf) {
			matches = true;
			break;
		}
	}
	ret = (matches && kref_get_unless_zero(&mbuf->kref)) ? true : false;
	mutex_unlock(&inst->registeredbufs.lock);

	return ret;
}

static int set_internal_buf_on_fw(struct msm_cvp_inst *inst,
				enum hal_buffer buffer_type,
				struct msm_smem *handle, bool reuse)
{
	struct cvp_buffer_addr_info buffer_info;
	struct hfi_device *hdev;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device || !handle) {
		dprintk(CVP_ERR, "%s - invalid params\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	buffer_info.buffer_size = handle->size;
	buffer_info.buffer_type = buffer_type;
	buffer_info.num_buffers = 1;
	buffer_info.align_device_addr = handle->device_addr;
	dprintk(CVP_DBG, "%s %s buffer : %x\n",
			reuse ? "Reusing" : "Allocated",
			"INTERNAL_PERSIST_1",
			buffer_info.align_device_addr);

	rc = call_hfi_op(hdev, session_set_buffers,
			(void *) inst->session, &buffer_info);
	if (rc) {
		dprintk(CVP_ERR, "cvp_session_set_buffers failed\n");
		return rc;
	}
	return 0;
}

static int allocate_and_set_internal_bufs(struct msm_cvp_inst *inst,
			struct hal_buffer_requirements *internal_bufreq,
			struct msm_cvp_list *buf_list)
{
	struct internal_buf *binfo;
	u32 smem_flags = SMEM_UNCACHED;
	int rc = 0;

	if (!inst || !internal_bufreq || !buf_list) {
		dprintk(CVP_ERR, "%s Invalid input\n", __func__);
		return -EINVAL;
	}

	if (!internal_bufreq->buffer_size)
		return 0;

	/* PERSIST buffer requires secure mapping */
	smem_flags |= SMEM_SECURE;

	binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
	if (!binfo) {
		dprintk(CVP_ERR, "%s Out of memory\n", __func__);
		rc = -ENOMEM;
		goto fail_kzalloc;
	}

	rc = msm_cvp_smem_alloc(internal_bufreq->buffer_size, 1, smem_flags,
			internal_bufreq->buffer_type, 0,
			&(inst->core->resources), inst->session_type,
			&binfo->smem);
	if (rc) {
		dprintk(CVP_ERR, "Failed to allocate ARP memory\n");
		goto err_no_mem;
	}

	binfo->buffer_type = internal_bufreq->buffer_type;

	rc = set_internal_buf_on_fw(inst, internal_bufreq->buffer_type,
			&binfo->smem, false);
	if (rc)
		goto fail_set_buffers;

	mutex_lock(&buf_list->lock);
	list_add_tail(&binfo->list, &buf_list->list);
	mutex_unlock(&buf_list->lock);
	return rc;

fail_set_buffers:
	msm_cvp_smem_free(&binfo->smem);
err_no_mem:
	kfree(binfo);
fail_kzalloc:
	return rc;
}


/* Set ARP buffer for CVP firmware to handle concurrency */
int cvp_comm_set_arp_buffers(struct msm_cvp_inst *inst)
{
	int rc = 0, idx = 0;
	struct hal_buffer_requirements *internal_buf = NULL;
	struct msm_cvp_list *buf_list = &inst->persistbufs;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	idx = ffs(HAL_BUFFER_INTERNAL_PERSIST_1);
	internal_buf = &inst->buff_req.buffer[idx];
	internal_buf->buffer_type = HAL_BUFFER_INTERNAL_PERSIST_1;
	internal_buf->buffer_size = ARP_BUF_SIZE;

	rc = allocate_and_set_internal_bufs(inst, internal_buf, buf_list);
	if (rc)
		goto error;

	rc = wait_for_sess_signal_receipt(inst, HAL_SESSION_SET_BUFFER_DONE);
	if (rc) {
		dprintk(CVP_WARN, "wait for set_buffer_done timeout %d\n", rc);
		goto error;
	}

	return rc;

error:
	cvp_comm_release_persist_buffers(inst);
	return rc;
}

int cvp_comm_release_persist_buffers(struct msm_cvp_inst *inst)
{
	struct msm_smem *handle;
	struct list_head *ptr, *next;
	struct internal_buf *buf;
	struct cvp_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_cvp_core *core;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(CVP_ERR, "Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}

	core = inst->core;
	if (!core) {
		dprintk(CVP_ERR, "Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(CVP_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}

	dprintk(CVP_DBG, "release persist buffer!\n");
	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		handle = &buf->smem;
		if (!handle) {
			dprintk(CVP_ERR, "%s invalid smem\n", __func__);
			mutex_unlock(&inst->persistbufs.lock);
			return -EINVAL;
		}
		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		buffer_info.response_required = true;
		rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
		if (!rc) {
			mutex_unlock(&inst->persistbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
					HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc)
				dprintk(CVP_WARN,
					"%s: wait for signal failed, rc %d\n",
						__func__, rc);
			mutex_lock(&inst->persistbufs.lock);
		} else {
			dprintk(CVP_WARN,
					"Rel prst buf fail:%x, %d\n",
					buffer_info.align_device_addr,
					buffer_info.buffer_size);
		}
		list_del(&buf->list);
		msm_cvp_smem_free(handle);
		kfree(buf);
	}
	mutex_unlock(&inst->persistbufs.lock);
	return rc;
}
