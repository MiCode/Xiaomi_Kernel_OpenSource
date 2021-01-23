// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#ifndef CVP_MDT_ENABLED
#include <soc/qcom/subsystem_restart.h>
#endif
#include <asm/div64.h>
#include "msm_cvp_common.h"
#include "cvp_hfi_api.h"
#include "msm_cvp_debug.h"
#include "msm_cvp_clocks.h"
#include "msm_cvp.h"
#include "cvp_core_hfi.h"

#define IS_ALREADY_IN_STATE(__p, __d) (\
	(__p >= __d)\
)

static void handle_session_error(enum hal_command_response cmd, void *data);

static void dump_hfi_queue(struct iris_hfi_device *device)
{
	struct cvp_hfi_queue_header *queue;
	struct cvp_iface_q_info *qinfo;
	int i;
	u32 *read_ptr, read_idx;

	dprintk(CVP_ERR, "HFI queues in order of cmd(rd, wr), msg and dbg:\n");

	/*
	 * mb() to ensure driver reads the updated header values from
	 * main memory.
	 */
	mb();
	for (i = 0; i <= CVP_IFACEQ_DBGQ_IDX; i++) {
		qinfo = &device->iface_queues[i];
		queue = (struct cvp_hfi_queue_header *)qinfo->q_hdr;
		if (!queue) {
			dprintk(CVP_ERR, "HFI queue not init, fail to dump\n");
			return;
		}
		dprintk(CVP_ERR, "queue details: %d %d\n",
				queue->qhdr_read_idx, queue->qhdr_write_idx);
		if (queue->qhdr_read_idx != queue->qhdr_write_idx) {
			read_idx = queue->qhdr_read_idx;
			read_ptr = (u32 *)((qinfo->q_array.align_virtual_addr) +
				(read_idx << 2));
			dprintk(CVP_ERR, "queue payload: %x %x %x %x\n",
				read_ptr[0], read_ptr[1],
				read_ptr[2], read_ptr[3]);
		}

	}
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

	/* This should come from sys_init_done */
	core->resources.max_inst_count =
		sys_init_msg->max_sessions_supported ?
		min_t(u32, sys_init_msg->max_sessions_supported,
		MAX_SUPPORTED_INSTANCES) : MAX_SUPPORTED_INSTANCES;

	core->resources.max_secure_inst_count =
		core->resources.max_secure_inst_count ?
		core->resources.max_secure_inst_count :
		core->resources.max_inst_count;

	memcpy(core->capabilities, sys_init_msg->capabilities,
		sys_init_msg->codec_count * sizeof(struct msm_cvp_capability));

	dprintk(CVP_CORE,
		"%s: max_inst_count %d, max_secure_inst_count %d\n",
		__func__, core->resources.max_inst_count,
		core->resources.max_secure_inst_count);

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

struct msm_cvp_inst *cvp_get_inst_validate(struct msm_cvp_core *core,
		void *session_id)
{
	int rc = 0;
	struct cvp_hfi_device *hdev;
	struct msm_cvp_inst *s;

	s = cvp_get_inst(core, session_id);
	if (!s) {
		dprintk(CVP_ERR, "%s session doesn't exit\n",
			__builtin_return_address(0));
		return NULL;
	}

	hdev = s->core->device;
	rc = call_hfi_op(hdev, validate_session, s->session, __func__);
	if (rc) {
		cvp_put_inst(s);
		s = NULL;
	}

	return s;
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

	if (response->status)
		dprintk(CVP_ERR, "HFI MSG error %d cmd response %d\n",
			response->status, cmd);

	dprintk(CVP_SESS, "%s: inst=%pK\n", __func__, inst);

	if (IS_HAL_SESSION_CMD(cmd)) {
		dprintk(CVP_INFO, "%s: calling completion for index = %d",
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
	struct cvp_internal_buf *buf;
	struct list_head *ptr, *next;
	u32 buf_found = false;
	u32 address;

	if (!response) {
		dprintk(CVP_ERR, "Invalid release_buf_done response\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN,
			"%s: Got a response for an inactive session\n",
			__func__);
		return;
	}

	address = response->data.buffer_addr;

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		buf = list_entry(ptr, struct cvp_internal_buf, list);
		if (address == buf->smem->device_addr + buf->offset) {
			dprintk(CVP_SESS, "releasing persist: %#x\n",
					buf->smem->device_addr);
			buf_found = true;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

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
		dprintk(CVP_SESS,
			"Inst: %pK is in bad state can't change state to %d\n",
			inst, state);
		goto exit;
	}
	dprintk(CVP_SESS, "Moved inst: %pK from state: %d to state: %d\n",
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
	struct cvp_hfi_device *hdev;

	if (!IS_HAL_SESSION_CMD(cmd)) {
		dprintk(CVP_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	hdev = (struct cvp_hfi_device *)(inst->core->device);
	rc = wait_for_completion_timeout(
		&inst->completions[SESSION_MSG_INDEX(cmd)],
		msecs_to_jiffies(
			inst->core->resources.msm_cvp_hw_rsp_timeout));
	if (!rc) {
		dprintk(CVP_WARN, "Wait interrupted or timed out: %d\n",
				SESSION_MSG_INDEX(cmd));
		call_hfi_op(hdev, flush_debug_queue, hdev->hfi_device_data);
		dump_hfi_queue(hdev->hfi_device_data);
		rc = -EIO;
	} else if (inst->state == MSM_CVP_CORE_INVALID) {
		rc = -ECONNRESET;
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
	dprintk(CVP_SESS, "Waiting for hal_cmd: %d\n", hal_cmd);
	rc = wait_for_sess_signal_receipt(inst, hal_cmd);
	if (!rc)
		change_cvp_inst_state(inst, desired_state);
err_same_state:
	return rc;
}

void msm_cvp_notify_event(struct msm_cvp_inst *inst, int event_type)
{
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
		dprintk(CVP_WARN, "%s:Got a response for an inactive session\n",
				__func__);
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

	dprintk(CVP_SESS, "%s: cvp session %#x\n", __func__,
		hash32_ptr(inst->session));

	signal_session_msg_receipt(cmd, inst);
	cvp_put_inst(inst);
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
		dprintk(CVP_WARN, "%s:Got a response for an inactive session\n",
				__func__);
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	cvp_put_inst(inst);
}

static void handle_session_flush(enum hal_command_response cmd, void *data)
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
		dprintk(CVP_WARN, "%s:Got a response for an inactive session\n",
				__func__);
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	cvp_put_inst(inst);
}

static void handle_session_error(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct cvp_hfi_device *hdev = NULL;
	struct msm_cvp_inst *inst = NULL;
	int event = CVP_SYS_ERROR_EVENT;

	if (!response) {
		dprintk(CVP_ERR,
			"Failed to get valid response for session error\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN, "%s: response for an inactive session\n",
				__func__);
		return;
	}

	hdev = inst->core->device;
	dprintk(CVP_ERR, "Session error received for inst %pK session %x\n",
		inst, hash32_ptr(inst->session));

	if (response->status == CVP_ERR_MAX_CLIENTS) {
		dprintk(CVP_WARN, "Too many clients, rejecting %pK", inst);
		event = CVP_MAX_CLIENTS_EVENT;

		/*
		 * Clean the HFI session now. Since inst->state is moved to
		 * INVALID, forward thread doesn't know FW has valid session
		 * or not. This is the last place driver knows that there is
		 * no session in FW. Hence clean HFI session now.
		 */

		msm_cvp_comm_session_clean(inst);
	} else if (response->status == CVP_ERR_NOT_SUPPORTED) {
		dprintk(CVP_WARN, "Unsupported bitstream in %pK", inst);
		event = CVP_HW_UNSUPPORTED_EVENT;
	} else {
		dprintk(CVP_WARN, "Unknown session error (%d) for %pK\n",
				response->status, inst);
		event = CVP_SYS_ERROR_EVENT;
	}

	/* change state before sending error to client */
	change_cvp_inst_state(inst, MSM_CVP_CORE_INVALID);
	msm_cvp_notify_event(inst, event);
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
		msm_cvp_notify_event(inst,
				CVP_SYS_ERROR_EVENT);
	}
	mutex_unlock(&core->lock);
}

static void handle_sys_error(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_core *core = NULL;
	struct cvp_hfi_device *hdev = NULL;
	struct msm_cvp_inst *inst = NULL;
	int i, rc = 0;
	unsigned long flags = 0;
	enum cvp_core_state cur_state;

#ifndef CVP_MDT_ENABLED
	subsystem_crashed("evass");
#endif
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

	cur_state = core->state;
	core->state = CVP_CORE_UNINIT;
	dprintk(CVP_WARN, "SYS_ERROR from core %pK ssr_sess_cnt %lld\n",
			core, core->ssr_sess_cnt);
	core->ssr_sess_cnt = 0;
	msm_cvp_noc_error_info(core);
	call_hfi_op(hdev, flush_debug_queue, hdev->hfi_device_data);
	list_for_each_entry(inst, &core->instances, list) {
		dprintk(CVP_WARN,
			"%s: sys error inst %#x kref %x, cmd %x state %x\n",
				__func__, inst, kref_read(&inst->kref),
				inst->cur_cmd_type, inst->state);
		if (inst->state != MSM_CVP_CORE_INVALID) {
			change_cvp_inst_state(inst, MSM_CVP_CORE_INVALID);
			if (cvp_stop_clean_fence_queue(inst))
				dprintk(CVP_ERR, "Failed to clean fences\n");
			for (i = 0; i < ARRAY_SIZE(inst->completions); i++)
				complete(&inst->completions[i]);
			spin_lock_irqsave(&inst->event_handler.lock, flags);
			inst->event_handler.event = CVP_SSR_EVENT;
			spin_unlock_irqrestore(
				&inst->event_handler.lock, flags);
			wake_up_all(&inst->event_handler.wq);
		}

		if (!core->trigger_ssr)
			msm_cvp_print_inst_bufs(inst);
	}

	/* handle the hw error before core released to get full debug info */
	msm_cvp_handle_hw_error(core);
	if (response->status == CVP_ERR_NOC_ERROR) {
		dprintk(CVP_WARN, "Got NOC error");
		MSM_CVP_ERROR(true);
	}

	dprintk(CVP_CORE, "Calling core_release\n");
	rc = call_hfi_op(hdev, core_release, hdev->hfi_device_data);
	if (rc) {
		dprintk(CVP_ERR, "core_release failed\n");
		core->state = cur_state;
		mutex_unlock(&core->lock);
		return;
	}
	mutex_unlock(&core->lock);

	dprintk(CVP_WARN, "SYS_ERROR handled.\n");
}

void msm_cvp_comm_session_clean(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct cvp_hfi_device *hdev = NULL;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid params\n", __func__);
		return;
	}
	if (!inst->session) {
		dprintk(CVP_SESS, "%s: inst %pK session already cleaned\n",
			__func__, inst);
		return;
	}

	hdev = inst->core->device;
	mutex_lock(&inst->lock);
	dprintk(CVP_SESS, "%s: inst %pK\n", __func__, inst);
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
		dprintk(CVP_WARN, "%s: response for an inactive session\n",
				__func__);
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	show_stats(inst);
	cvp_put_inst(inst);
}

void cvp_handle_cmd_response(enum hal_command_response cmd, void *data)
{
	dprintk(CVP_HFI, "Command response = %d\n", cmd);
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
	case HAL_SESSION_PERSIST_SET_DONE:
	case HAL_SESSION_PERSIST_REL_DONE:
	case HAL_SESSION_CHROMA_SET_DONE:
	case HAL_SESSION_CHROMA_REL_DONE:
	case HAL_SESSION_TME_CONFIG_CMD_DONE:
	case HAL_SESSION_ODT_CONFIG_CMD_DONE:
	case HAL_SESSION_OD_CONFIG_CMD_DONE:
	case HAL_SESSION_NCC_CONFIG_CMD_DONE:
	case HAL_SESSION_ICA_CONFIG_CMD_DONE:
	case HAL_SESSION_HCD_CONFIG_CMD_DONE:
	case HAL_SESSION_DCM_CONFIG_CMD_DONE:
	case HAL_SESSION_DC_CONFIG_CMD_DONE:
	case HAL_SESSION_PYS_HCD_CONFIG_CMD_DONE:
	case HAL_SESSION_FD_CONFIG_CMD_DONE:
	case HAL_SESSION_MODEL_BUF_CMD_DONE:
	case HAL_SESSION_ICA_FRAME_CMD_DONE:
	case HAL_SESSION_FD_FRAME_CMD_DONE:
		cvp_handle_session_cmd_done(cmd, data);
		break;
	default:
		dprintk(CVP_HFI, "response unhandled: %d\n", cmd);
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
		dprintk(CVP_CORE,
			"Thermal mitigation not enabled. debugfs %d\n",
			msm_cvp_thermal_mitigation_disabled);
		return true;
	}

	tl = msm_comm_cvp_thermal_level(cvp_driver->thermal_level);
	freq = core->curr_freq;

	is_turbo = is_core_turbo(core, freq);
	dprintk(CVP_CORE,
		"Core freq %ld Thermal level %d Turbo mode %d\n",
		freq, tl, is_turbo);

	if (is_turbo && tl >= CVP_THERMAL_LOW) {
		dprintk(CVP_ERR,
			"CVP session not allowed. Turbo mode %d Thermal level %d\n",
			is_turbo, tl);
		return false;
	}
	return true;
}

static int msm_comm_session_abort(struct msm_cvp_inst *inst)
{
	int rc = 0, abort_completion = 0;
	struct cvp_hfi_device *hdev;

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
		call_hfi_op(hdev, flush_debug_queue, hdev->hfi_device_data);
		dump_hfi_queue(hdev->hfi_device_data);
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
			msm_cvp_notify_event(inst,
					CVP_SYS_ERROR_EVENT);
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
	struct cvp_hfi_device *hdev;

	mutex_lock(&core->lock);
	if (core->state >= CVP_CORE_INIT_DONE) {
		dprintk(CVP_INFO, "CVP core: %d is already in state: %d\n",
				core->id, core->state);
		goto exit;
	}
	dprintk(CVP_CORE, "Waiting for SYS_INIT_DONE\n");
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(HAL_SYS_INIT_DONE)],
		msecs_to_jiffies(core->resources.msm_cvp_hw_rsp_timeout));
	if (!rc) {
		dprintk(CVP_ERR, "%s: Wait interrupted or timed out: %d\n",
				__func__, SYS_MSG_INDEX(HAL_SYS_INIT_DONE));
		hdev = core->device;
		call_hfi_op(hdev, flush_debug_queue, hdev->hfi_device_data);
		dump_hfi_queue(hdev->hfi_device_data);
		rc = -EIO;
		goto exit;
	} else {
		core->state = CVP_CORE_INIT_DONE;
		rc = 0;
	}
	dprintk(CVP_CORE, "SYS_INIT_DONE!!!\n");
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
	struct cvp_hfi_device *hdev;
	struct msm_cvp_core *core;

	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;

	core = inst->core;
	hdev = core->device;
	mutex_lock(&core->lock);
	if (core->state >= CVP_CORE_INIT) {
		dprintk(CVP_CORE, "CVP core: %d is already in state: %d\n",
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
	dprintk(CVP_CORE, "%s: core %pK\n", __func__, core);
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

	return rc;

fail_core_init:
	kfree(core->capabilities);
fail_cap_alloc:
	core->capabilities = NULL;
	core->state = CVP_CORE_UNINIT;
	mutex_unlock(&core->lock);
	return rc;
}

int msm_cvp_deinit_core(struct msm_cvp_inst *inst)
{
	struct msm_cvp_core *core;
	struct cvp_hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == CVP_CORE_UNINIT) {
		dprintk(CVP_INFO, "CVP core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_uninited;
	}

	if (!core->resources.never_unload_fw) {
		cancel_delayed_work(&core->fw_unload_work);

		/*
		 * Delay unloading of firmware. This is useful
		 * in avoiding firmware download delays in cases where we
		 * will have a burst of back to back cvp sessions
		 */
		schedule_delayed_work(&core->fw_unload_work,
			msecs_to_jiffies(core->state == CVP_CORE_INIT_DONE ?
			core->resources.msm_cvp_firmware_unload_delay : 0));

		dprintk(CVP_CORE, "firmware unload delayed by %u ms\n",
			core->state == CVP_CORE_INIT_DONE ?
			core->resources.msm_cvp_firmware_unload_delay : 0);
	}

core_already_uninited:
	change_cvp_inst_state(inst, MSM_CVP_CORE_UNINIT);
	mutex_unlock(&core->lock);
	return 0;
}

static int msm_comm_session_init_done(int flipped_state,
	struct msm_cvp_inst *inst)
{
	int rc;

	dprintk(CVP_SESS, "inst %pK: waiting for session init done\n", inst);
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
	struct cvp_hfi_device *hdev;

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

	dprintk(CVP_SESS, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_init, hdev->hfi_device_data,
			inst, &inst->session);

	if (rc || !inst->session) {
		dprintk(CVP_ERR,
			"Failed to call session init for: %pK, %pK, %d\n",
			inst->core->device, inst, inst->session_type);
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
	struct cvp_hfi_device *hdev;

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
	dprintk(CVP_SESS, "%s: inst %pK\n", __func__, inst);
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
	struct cvp_hfi_device *hdev;
	struct msm_cvp_core *core;
	int rc = 0;

	core = get_cvp_core(core_id);
	if (!core) {
		dprintk(CVP_ERR,
			"%s: Failed to find core for core_id = %d\n",
			__func__, core_id);
		return -EINVAL;
	}

	hdev = (struct cvp_hfi_device *)core->device;
	if (!hdev) {
		dprintk(CVP_ERR, "%s Invalid device handle\n", __func__);
		return -EINVAL;
	}

	rc = call_hfi_op(hdev, suspend, hdev->hfi_device_data);
	if (rc)
		dprintk(CVP_WARN, "Failed to suspend\n");

	return rc;
}

static int get_flipped_state(int present_state, int desired_state)
{
	int flipped_state = present_state;

	if (flipped_state < MSM_CVP_CLOSE && desired_state > MSM_CVP_CLOSE) {
		flipped_state = MSM_CVP_CLOSE + (MSM_CVP_CLOSE - flipped_state);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	} else if (flipped_state > MSM_CVP_CLOSE
			&& desired_state < MSM_CVP_CLOSE) {
		flipped_state = MSM_CVP_CLOSE -
			(flipped_state - MSM_CVP_CLOSE + 1);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	}
	return flipped_state;
}

int msm_cvp_comm_try_state(struct msm_cvp_inst *inst, int state)
{
	int rc = 0;
	int flipped_state;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params %pK", __func__, inst);
		return -EINVAL;
	}
	dprintk(CVP_SESS,
		"Trying to move inst: %pK (%#x) from: %#x to %#x\n",
		inst, hash32_ptr(inst->session), inst->state, state);

	mutex_lock(&inst->sync_lock);
	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR, "%s: inst %pK is in invalid\n",
			__func__, inst);
		mutex_unlock(&inst->sync_lock);
		return -EINVAL;
	}

	flipped_state = get_flipped_state(inst->state, state);
	dprintk(CVP_SESS,
		"inst: %pK (%#x) flipped_state = %#x %x\n",
		inst, hash32_ptr(inst->session), flipped_state, state);
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
	case MSM_CVP_CLOSE:
		dprintk(CVP_INFO, "to CVP_CLOSE state\n");
		rc = msm_comm_session_close(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_CLOSE_DONE:
		dprintk(CVP_INFO, "to CVP_CLOSE_DONE state\n");
		rc = wait_for_state(inst, flipped_state, MSM_CVP_CLOSE_DONE,
				HAL_SESSION_END_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		msm_cvp_comm_session_clean(inst);
	case MSM_CVP_CORE_UNINIT:
	case MSM_CVP_CORE_INVALID:
		dprintk(CVP_INFO, "Sending core uninit\n");
		rc = msm_cvp_deinit_core(inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	default:
		dprintk(CVP_ERR, "State not recognized\n");
		rc = -EINVAL;
		break;
	}

	mutex_unlock(&inst->sync_lock);

	if (rc) {
		dprintk(CVP_ERR,
				"Failed to move from state: %d to %d\n",
				inst->state, state);
		msm_cvp_comm_kill_session(inst);
	}
	return rc;
}

int msm_cvp_noc_error_info(struct msm_cvp_core *core)
{
	struct cvp_hfi_device *hdev;

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
	int rc, max_retries = CVP_MAX_SSR_RETRIES;
	struct msm_cvp_core *core;
	struct cvp_hfi_device *hdev;

	core = container_of(work, struct msm_cvp_core, ssr_work);
	if (!core || !core->device) {
		dprintk(CVP_ERR, "%s: Invalid params\n", __func__);
		return;
	}
	hdev = core->device;

	if (core->ssr_type == SSR_SESSION_ABORT) {
		struct msm_cvp_inst *inst = NULL, *s;

		dprintk(CVP_ERR, "Session abort triggered\n");
		list_for_each_entry(inst, &core->instances, list) {
			dprintk(CVP_WARN,
				"Session to abort: inst %#x cmd %x ref %x\n",
				inst, inst->cur_cmd_type,
				kref_read(&inst->kref));
			break;
		}

		if (inst != NULL) {
			s = cvp_get_inst_validate(inst->core, inst);
			if (!s)
				return;

			call_hfi_op(hdev, flush_debug_queue,
				hdev->hfi_device_data);
			dump_hfi_queue(hdev->hfi_device_data);
			msm_cvp_comm_kill_session(inst);
			cvp_put_inst(s);
		} else {
			dprintk(CVP_WARN, "No active CVP session to abort\n");
		}

		return;
	}

send_again:
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
			if (rc == -EAGAIN) {
				core->trigger_ssr = false;
				mutex_unlock(&core->lock);
				usleep_range(500, 1000);
				dprintk(CVP_WARN, "Retry ssr\n");
				if (max_retries-- > 0)
					goto send_again;
			}
			dprintk(CVP_ERR, "%s: trigger_ssr failed\n",
				__func__);
			core->trigger_ssr = false;
		}
	} else {
		dprintk(CVP_WARN, "%s: cvp core %pK not initialized\n",
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
	unsigned long flags = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s: invalid input parameters\n", __func__);
		return -EINVAL;
	} else if (!inst->session) {
		dprintk(CVP_ERR, "%s: no session to kill for inst %pK\n",
			__func__, inst);
		return 0;
	}
	dprintk(CVP_WARN, "%s: inst %pK, session %x state %d\n", __func__,
		inst, hash32_ptr(inst->session), inst->state);
	/*
	 * We're internally forcibly killing the session, if fw is aware of
	 * the session send session_abort to firmware to clean up and release
	 * the session, else just kill the session inside the driver.
	 */
	if (inst->state >= MSM_CVP_OPEN_DONE &&
			inst->state < MSM_CVP_CLOSE_DONE) {
		rc = msm_comm_session_abort(inst);
		if (rc) {
			dprintk(CVP_ERR,
				"%s: inst %pK session %x abort failed\n",
				__func__, inst, hash32_ptr(inst->session));
			change_cvp_inst_state(inst, MSM_CVP_CORE_INVALID);
		} else {
			change_cvp_inst_state(inst, MSM_CVP_CORE_UNINIT);
		}
	}

	if (inst->state >= MSM_CVP_CORE_UNINIT) {
		spin_lock_irqsave(&inst->event_handler.lock, flags);
		inst->event_handler.event = CVP_SSR_EVENT;
		spin_unlock_irqrestore(&inst->event_handler.lock, flags);
		wake_up_all(&inst->event_handler.wq);
	}

	return rc;
}

void msm_cvp_fw_unload_handler(struct work_struct *work)
{
	struct msm_cvp_core *core = NULL;
	struct cvp_hfi_device *hdev = NULL;
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
			dprintk(CVP_CORE, "Calling cvp_hal_core_release\n");
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

static int set_internal_buf_on_fw(struct msm_cvp_inst *inst,
				struct msm_cvp_smem *handle)
{
	struct cvp_hfi_device *hdev;
	int rc = 0;
	u32 iova;
	u32 size;

	if (!inst || !inst->core || !inst->core->device || !handle) {
		dprintk(CVP_ERR, "%s - invalid params\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	iova = handle->device_addr;
	size = handle->size;

	dprintk(CVP_SESS, "%s: allocated ARP buffer : %x\n", __func__, iova);

	rc = call_hfi_op(hdev, session_set_buffers,
			(void *) inst->session, iova, size);
	if (rc) {
		dprintk(CVP_ERR, "cvp_session_set_buffers failed\n");
		return rc;
	}
	return 0;
}

/* Set ARP buffer for CVP firmware to handle concurrency */
int cvp_comm_set_arp_buffers(struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct cvp_internal_buf *buf;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	buf = cvp_allocate_arp_bufs(inst, ARP_BUF_SIZE);
	if (!buf) {
		rc = -ENOMEM;
		goto error;
	}

	rc = set_internal_buf_on_fw(inst, buf->smem);
	if (rc)
		goto error;

	rc = wait_for_sess_signal_receipt(inst, HAL_SESSION_SET_BUFFER_DONE);
	if (rc) {
		dprintk(CVP_WARN, "wait for set_buffer_done timeout %d\n", rc);
		goto error;
	}

	return rc;

error:
	if (rc != -ENOMEM)
		cvp_release_arp_buffers(inst);
	return rc;
}




