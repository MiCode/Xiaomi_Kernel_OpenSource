/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/workqueue.h>
#include <soc/qcom/subsystem_restart.h>
#include <asm/div64.h>
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"

#define IS_ALREADY_IN_STATE(__p, __d) ({\
	int __rc = (__p >= __d);\
	__rc; \
})

#define IS_VALID_DCVS_SESSION(__cur_mbpf, __min_mbpf) \
		((__cur_mbpf) >= (__min_mbpf))

#define SUM_ARRAY(__arr, __start, __end) ({\
		int __index;\
		typeof((__arr)[0]) __sum = 0;\
		for (__index = (__start); __index <= (__end); __index++) {\
			if (__index >= 0 && __index < ARRAY_SIZE(__arr))\
				__sum += __arr[__index];\
		} \
		__sum;\
})

#define V4L2_EVENT_SEQ_CHANGED_SUFFICIENT \
		V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_SUFFICIENT
#define V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT \
		V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT
#define V4L2_EVENT_RELEASE_BUFFER_REFERENCE \
		V4L2_EVENT_MSM_VIDC_RELEASE_BUFFER_REFERENCE

#define IS_SESSION_CMD_VALID(cmd) (((cmd) >= SESSION_MSG_START) && \
		(((cmd) - SESSION_MSG_START) <= SESSION_MSG_END))
#define IS_SYS_CMD_VALID(cmd) (((cmd) >= SYS_MSG_START) && \
		(((cmd) - SYS_MSG_START) <= SYS_MSG_END))

struct getprop_buf {
	struct list_head list;
	void *data;
};

static void msm_comm_generate_session_error(struct msm_vidc_inst *inst);
static void msm_comm_generate_sys_error(struct msm_vidc_inst *inst);
static void handle_session_error(enum command_response cmd, void *data);
static void msm_comm_monitor_ftb(struct msm_vidc_inst *inst);
static int msm_comm_scale_clocks_dcvs(struct msm_vidc_inst *inst, bool fbd);
static int msm_comm_check_dcvs_supported(struct msm_vidc_inst *inst);

static inline bool is_turbo_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_TURBO);
}

static inline bool is_thumbnail_session(struct msm_vidc_inst *inst)
{
	if (inst->session_type == MSM_VIDC_DECODER) {
		int rc = 0;
		struct v4l2_control ctrl = {
			.id = V4L2_CID_MPEG_VIDC_VIDEO_SYNC_FRAME_DECODE
		};
		rc = v4l2_g_ctrl(&inst->ctrl_handler, &ctrl);
		if (!rc && ctrl.value)
			return true;
	}
	return false;
}

enum multi_stream msm_comm_get_stream_output_mode(struct msm_vidc_inst *inst)
{
	if (inst->session_type == MSM_VIDC_DECODER) {
		int rc = 0;
		struct v4l2_control ctrl = {
			.id = V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE
		};
		rc = v4l2_g_ctrl(&inst->ctrl_handler, &ctrl);
		if (!rc && ctrl.value)
			return HAL_VIDEO_DECODER_SECONDARY;
	}
	return HAL_VIDEO_DECODER_PRIMARY;


}
static int msm_comm_get_mbs_per_sec(struct msm_vidc_inst *inst)
{
	int height, width;
	height = max(inst->prop.height[CAPTURE_PORT],
		inst->prop.height[OUTPUT_PORT]);
	width = max(inst->prop.width[CAPTURE_PORT],
		inst->prop.width[OUTPUT_PORT]);
	return NUM_MBS_PER_SEC(height, width, inst->prop.fps);
}

static inline int msm_comm_get_mbs_per_frame(struct msm_vidc_inst *inst)
{
	int height, width;
	height = inst->prop.height[CAPTURE_PORT];
	width = inst->prop.width[CAPTURE_PORT];
	return NUM_MBS_PER_FRAME(height, width);
}

static inline int msm_comm_count_active_instances(struct msm_vidc_core *core)
{
	int active_instances = 0;
	struct msm_vidc_inst *inst = NULL;
	if (!core) {
		dprintk(VIDC_ERR, "%s: Invalid args: %p\n", __func__, core);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->state >= MSM_VIDC_START_DONE &&
			inst->state < MSM_VIDC_STOP_DONE)
			active_instances++;
	}
	mutex_unlock(&core->lock);
	return active_instances;
}

enum load_calc_quirks {
	LOAD_CALC_NO_QUIRKS = 0,
	LOAD_CALC_IGNORE_TURBO_LOAD = 1 << 0,
	LOAD_CALC_IGNORE_THUMBNAIL_LOAD = 1 << 1,
};

static int msm_comm_get_inst_load(struct msm_vidc_inst *inst,
		enum load_calc_quirks quirks)
{
	int load = 0;
	if (!(inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_STOP_DONE))
		return 0;

	load = msm_comm_get_mbs_per_sec(inst);

	if (is_thumbnail_session(inst)) {
		if (quirks & LOAD_CALC_IGNORE_THUMBNAIL_LOAD)
			load = 0;
	}

	if (is_turbo_session(inst)) {
		if (!(quirks & LOAD_CALC_IGNORE_TURBO_LOAD))
			load = inst->core->resources.max_load;
	}

	return load;
}

static int msm_comm_get_load(struct msm_vidc_core *core,
	enum session_type type, enum load_calc_quirks quirks)
{
	struct msm_vidc_inst *inst = NULL;
	int num_mbs_per_sec = 0;

	if (!core) {
		dprintk(VIDC_ERR, "Invalid args: %p\n", core);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->session_type != type)
			continue;

		mutex_lock(&inst->lock);
		num_mbs_per_sec += msm_comm_get_inst_load(inst, quirks);
		mutex_unlock(&inst->lock);
	}
	mutex_unlock(&core->lock);

	return num_mbs_per_sec;
}

static enum hal_domain get_hal_domain(int session_type)
{
	enum hal_domain domain;
	switch (session_type) {
	case MSM_VIDC_ENCODER:
		domain = HAL_VIDEO_DOMAIN_ENCODER;
		break;
	case MSM_VIDC_DECODER:
		domain = HAL_VIDEO_DOMAIN_DECODER;
		break;
	default:
		dprintk(VIDC_ERR, "Wrong domain\n");
		domain = HAL_UNUSED_DOMAIN;
		break;
	}

	return domain;
}

enum hal_video_codec get_hal_codec_type(int fourcc)
{
	enum hal_video_codec codec;
	dprintk(VIDC_DBG, "codec is 0x%x\n", fourcc);
	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H264_NO_SC:
		codec = HAL_VIDEO_CODEC_H264;
		break;
	case V4L2_PIX_FMT_H264_MVC:
		codec = HAL_VIDEO_CODEC_MVC;
		break;
	case V4L2_PIX_FMT_H263:
		codec = HAL_VIDEO_CODEC_H263;
		break;
	case V4L2_PIX_FMT_MPEG1:
		codec = HAL_VIDEO_CODEC_MPEG1;
		break;
	case V4L2_PIX_FMT_MPEG2:
		codec = HAL_VIDEO_CODEC_MPEG2;
		break;
	case V4L2_PIX_FMT_MPEG4:
		codec = HAL_VIDEO_CODEC_MPEG4;
		break;
	case V4L2_PIX_FMT_VC1_ANNEX_G:
	case V4L2_PIX_FMT_VC1_ANNEX_L:
		codec = HAL_VIDEO_CODEC_VC1;
		break;
	case V4L2_PIX_FMT_VP8:
		codec = HAL_VIDEO_CODEC_VP8;
		break;
	case V4L2_PIX_FMT_DIVX_311:
		codec = HAL_VIDEO_CODEC_DIVX_311;
		break;
	case V4L2_PIX_FMT_DIVX:
		codec = HAL_VIDEO_CODEC_DIVX;
		break;
	case V4L2_PIX_FMT_HEVC:
		codec = HAL_VIDEO_CODEC_HEVC;
		break;
	case V4L2_PIX_FMT_HEVC_HYBRID:
		codec = HAL_VIDEO_CODEC_HEVC_HYBRID;
		break;
	default:
		dprintk(VIDC_ERR, "Wrong codec: %d\n", fourcc);
		codec = HAL_UNUSED_CODEC;
		break;
	}

	return codec;
}

static int msm_comm_vote_bus(struct msm_vidc_core *core)
{
	int rc = 0, vote_data_count = 0, i = 0;
	struct hfi_device *hdev;
	struct msm_vidc_inst *inst = NULL;
	struct vidc_bus_vote_data *vote_data = NULL;

	if (!core) {
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, core);
		return -EINVAL;
	}

	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s Invalid device handle: %p\n",
			__func__, hdev);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		++vote_data_count;
	}

	vote_data = kzalloc(sizeof(*vote_data) * vote_data_count,
			GFP_TEMPORARY);
	if (!vote_data) {
		dprintk(VIDC_ERR, "%s: failed to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto fail_alloc;
	}

	list_for_each_entry(inst, &core->instances, list) {
		int codec = 0;

		mutex_lock(&inst->lock);

		codec = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[OUTPUT_PORT]->fourcc :
			inst->fmts[CAPTURE_PORT]->fourcc;

		vote_data[i].session = VIDC_VOTE_DATA_SESSION_VAL(
				get_hal_codec_type(codec),
				get_hal_domain(inst->session_type));
		vote_data[i].load = msm_comm_get_inst_load(inst,
				LOAD_CALC_NO_QUIRKS);

		mutex_unlock(&inst->lock);

		i++;
	}
	mutex_unlock(&core->lock);

	rc = call_hfi_op(hdev, vote_bus, hdev->hfi_device_data, vote_data,
			vote_data_count, core->idle_stats.fb_err_level);
	if (rc)
		dprintk(VIDC_ERR, "Failed to scale bus: %d\n", rc);

	kfree(vote_data);
	return rc;

fail_alloc:
	mutex_unlock(&core->lock);
	return rc;
}

static void msm_comm_unvote_buses(struct msm_vidc_core *core)
{
	struct hfi_device *hdev;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return;
	}

	hdev = core->device;
	if (call_hfi_op(hdev, unvote_bus, hdev->hfi_device_data))
		dprintk(VIDC_WARN,
				"Failed to unvote for buses\n");

}

struct msm_vidc_core *get_vidc_core(int core_id)
{
	struct msm_vidc_core *core;
	int found = 0;
	if (core_id > MSM_VIDC_CORES_MAX) {
		dprintk(VIDC_ERR, "Core id = %d is greater than max = %d\n",
			core_id, MSM_VIDC_CORES_MAX);
		return NULL;
	}
	mutex_lock(&vidc_driver->lock);
	list_for_each_entry(core, &vidc_driver->cores, list) {
		if (!core)
			return NULL;
		if (core->id == core_id) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&vidc_driver->lock);
	if (found)
		return core;
	return NULL;
}

const struct msm_vidc_format *msm_comm_get_pixel_fmt_index(
	const struct msm_vidc_format fmt[], int size, int index, int fmt_type)
{
	int i, k = 0;
	if (!fmt || index < 0) {
		dprintk(VIDC_ERR, "Invalid inputs, fmt = %p, index = %d\n",
						fmt, index);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (fmt[i].type != fmt_type)
			continue;
		if (k == index)
			break;
		k++;
	}
	if (i == size) {
		dprintk(VIDC_INFO, "Format not found\n");
		return NULL;
	}
	return &fmt[i];
}
struct msm_vidc_format *msm_comm_get_pixel_fmt_fourcc(
	struct msm_vidc_format fmt[], int size, int fourcc, int fmt_type)
{
	int i;
	if (!fmt) {
		dprintk(VIDC_ERR, "Invalid inputs, fmt = %p\n", fmt);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (fmt[i].fourcc == fourcc)
				break;
	}
	if (i == size) {
		dprintk(VIDC_INFO, "Format not found\n");
		return NULL;
	}
	return &fmt[i];
}

struct buf_queue *msm_comm_get_vb2q(
		struct msm_vidc_inst *inst, enum v4l2_buf_type type)
{
	if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return &inst->bufq[CAPTURE_PORT];
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		return &inst->bufq[OUTPUT_PORT];
	return NULL;
}

static void handle_sys_init_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core;
	struct vidc_hal_sys_init_done *sys_init_msg;
	unsigned int index;

	if (!IS_SYS_CMD_VALID(cmd)) {
		dprintk(VIDC_ERR, "%s - invalid cmd\n", __func__);
		return;
	}
	index = SYS_MSG_INDEX(cmd);

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for sys init\n");
		return;
	}
	core = get_vidc_core(response->device_id);
	if (!core) {
		dprintk(VIDC_ERR, "Wrong device_id received\n");
		return;
	}
	sys_init_msg = response->data;
	if (!sys_init_msg) {
		dprintk(VIDC_ERR, "sys_init_done message not proper\n");
		return;
	}
	core->enc_codec_supported = sys_init_msg->enc_codec_supported;
	core->dec_codec_supported = sys_init_msg->dec_codec_supported;
	if (core->id == MSM_VIDC_CORE_VENUS &&
		(core->dec_codec_supported & HAL_VIDEO_CODEC_H264))
			core->dec_codec_supported |=
				HAL_VIDEO_CODEC_MVC;
	dprintk(VIDC_DBG, "supported_codecs: enc = 0x%x, dec = 0x%x\n",
		core->enc_codec_supported, core->dec_codec_supported);
	dprintk(VIDC_DBG, "ptr[%d] = %p\n", index, &(core->completions[index]));
	complete(&(core->completions[index]));
}

static void handle_session_release_buf_done(enum command_response cmd,
	void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct internal_buf *buf;
	struct list_head *ptr, *next;
	struct hal_buffer_info *buffer;
	u32 buf_found = false;
	u32 address;

	if (!response || !response->data) {
		dprintk(VIDC_ERR, "Invalid release_buf_done response\n");
		return;
	}

	inst = (struct msm_vidc_inst *)response->session_id;
	buffer = (struct hal_buffer_info *) response->data;
	address = buffer->buffer_addr;

	list_for_each_safe(ptr, next, &inst->internalbufs) {
		buf = list_entry(ptr, struct internal_buf, list);
		if (address == (u32)buf->handle->device_addr) {
			dprintk(VIDC_DBG, "releasing scratch: 0x%pa\n",
					&buf->handle->device_addr);
			buf_found = true;
		}
	}

	list_for_each_safe(ptr, next, &inst->persistbufs) {
		buf = list_entry(ptr, struct internal_buf, list);
		if (address == (u32)buf->handle->device_addr) {
			dprintk(VIDC_DBG, "releasing persist: 0x%pa\n",
					&buf->handle->device_addr);
			buf_found = true;
		}
	}

	if (!buf_found)
		dprintk(VIDC_ERR, "invalid buffer received from firmware");
	if (IS_SESSION_CMD_VALID(cmd)) {
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	} else {
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);
		return;
	}
}

static void handle_sys_release_res_done(
	enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core;
	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for sys init\n");
		return;
	}
	core = get_vidc_core(response->device_id);
	if (!core) {
		dprintk(VIDC_ERR, "Wrong device_id received\n");
		return;
	}
	complete(&core->completions[SYS_MSG_INDEX(RELEASE_RESOURCE_DONE)]);
}

static void change_inst_state(struct msm_vidc_inst *inst,
	enum instance_state state)
{
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid parameter %s\n", __func__);
		return;
	}
	mutex_lock(&inst->lock);
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_DBG,
			"Inst: %p is in bad state can't change state\n",
			inst);
		goto exit;
	}
	dprintk(VIDC_DBG, "Moved inst: %p from state: %d to state: %d\n",
		   inst, inst->state, state);
	inst->state = state;
exit:
	mutex_unlock(&inst->lock);
}

static int signal_session_msg_receipt(enum command_response cmd,
		struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid(%p) instance id\n", inst);
		return -EINVAL;
	}
	if (IS_SESSION_CMD_VALID(cmd)) {
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	} else {
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int wait_for_sess_signal_receipt(struct msm_vidc_inst *inst,
	enum command_response cmd)
{
	int rc = 0;

	if (!IS_SESSION_CMD_VALID(cmd)) {
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	rc = wait_for_completion_timeout(
		&inst->completions[SESSION_MSG_INDEX(cmd)],
		msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "Wait interrupted or timedout: %d\n", rc);
		msm_comm_kill_session(inst);
		rc = -EIO;
	} else {
		rc = 0;
	}
	return rc;
}

static int wait_for_state(struct msm_vidc_inst *inst,
	enum instance_state flipped_state,
	enum instance_state desired_state,
	enum command_response hal_cmd)
{
	int rc = 0;
	if (IS_ALREADY_IN_STATE(flipped_state, desired_state)) {
		dprintk(VIDC_INFO, "inst: %p is already in state: %d\n",
						inst, inst->state);
		goto err_same_state;
	}
	dprintk(VIDC_DBG, "Waiting for hal_cmd: %d\n", hal_cmd);
	rc = wait_for_sess_signal_receipt(inst, hal_cmd);
	if (!rc)
		change_inst_state(inst, desired_state);
err_same_state:
	return rc;
}

void msm_vidc_queue_v4l2_event(struct msm_vidc_inst *inst, int event_type)
{
	struct v4l2_event event = {.id = 0, .type = event_type};
	v4l2_event_queue_fh(&inst->event_handler, &event);
	wake_up(&inst->kernel_event_queue);
}

static void msm_comm_generate_max_clients_error(struct msm_vidc_inst *inst)
{
	enum command_response cmd = SESSION_ERROR;
	struct msm_vidc_cb_cmd_done response = {0};

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	dprintk(VIDC_ERR, "%s: Too many clients\n", __func__);
	response.session_id = inst;
	response.status = VIDC_ERR_MAX_CLIENTS;
	handle_session_error(cmd, (void *)&response);
}

static void handle_session_init_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst = NULL;
	struct hfi_device *hdev;

	if (response) {
		struct vidc_hal_session_init_done *session_init_done =
			(struct vidc_hal_session_init_done *)
			response->data;
		inst = (struct msm_vidc_inst *)response->session_id;
		if (!inst || !inst->core || !inst->core->device) {
			dprintk(VIDC_ERR, "%s: invalid parameters (0x%p)\n",
				__func__, inst);
			return;
		}

		hdev = inst->core->device;

		if (!response->status && session_init_done) {
			inst->capability.width = session_init_done->width;
			inst->capability.height = session_init_done->height;
			inst->capability.frame_rate =
				session_init_done->frame_rate;
			inst->capability.scale_x = session_init_done->scale_x;
			inst->capability.scale_y = session_init_done->scale_y;
			inst->capability.hier_p = session_init_done->hier_p;
			inst->capability.ltr_count =
				session_init_done->ltr_count;
			inst->capability.pixelprocess_capabilities =
				call_hfi_op(hdev, get_core_capabilities);
			inst->capability.mbs_per_frame =
				session_init_done->mbs_per_frame;
			inst->capability.capability_set = true;
			inst->capability.buffer_mode[CAPTURE_PORT] =
				session_init_done->alloc_mode_out;
		} else {
			dprintk(VIDC_ERR,
				"Session init response from FW : 0x%x\n",
				response->status);
			if (response->status == VIDC_ERR_MAX_CLIENTS)
				msm_comm_generate_max_clients_error(inst);
			else
				msm_comm_generate_session_error(inst);
		}
		signal_session_msg_receipt(cmd, inst);
	} else {
		dprintk(VIDC_ERR,
				"Failed to get valid response for session init\n");
	}
}

static void handle_event_change(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct v4l2_control control = {0};
	struct msm_vidc_cb_event *event_notify;
	int event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
	int rc = 0;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		event_notify = (struct msm_vidc_cb_event *) response->data;
		switch (event_notify->hal_event_type) {
		case HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES:
			event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
			control.id =
				V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER;
			rc = v4l2_g_ctrl(&inst->ctrl_handler, &control);
			if (rc)
				dprintk(VIDC_WARN,
					"Failed to get Smooth streamng flag\n");
			if (!rc && control.value == true)
				event = V4L2_EVENT_SEQ_CHANGED_SUFFICIENT;
			break;
		case HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES:
			event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
			break;
		case HAL_EVENT_RELEASE_BUFFER_REFERENCE:
			{
				struct v4l2_event buf_event = {0};
				struct buffer_info *binfo = NULL;
				u32 *ptr = NULL;

				dprintk(VIDC_DBG,
					"%s - inst: %p buffer: 0x%pa extra: 0x%pa\n",
					__func__, inst,
					&event_notify->packet_buffer,
					&event_notify->extra_data_buffer);

				if (inst->state == MSM_VIDC_CORE_INVALID ||
					inst->core->state ==
						VIDC_CORE_INVALID) {
					dprintk(VIDC_DBG,
						"Event release buf ref received in invalid state - discard\n");
					return;
				}

				/*
				* Get the buffer_info entry for the
				* device address.
				*/
				binfo = device_to_uvaddr(inst,
					&inst->registered_bufs,
					event_notify->packet_buffer);
				if (!binfo) {
					dprintk(VIDC_ERR,
						"%s buffer not found in registered list\n",
						__func__);
					return;
				}

				/* Fill event data to be sent to client*/
				buf_event.type =
					V4L2_EVENT_RELEASE_BUFFER_REFERENCE;
				ptr = (u32 *)buf_event.u.data;
				ptr[0] = binfo->fd[0];
				ptr[1] = binfo->buff_off[0];

				dprintk(VIDC_DBG,
					"RELEASE REFERENCE EVENT FROM F/W - fd = %d offset = %d\n",
					ptr[0], ptr[1]);

				mutex_lock(&inst->sync_lock);
				/* Decrement buffer reference count*/
				buf_ref_put(inst, binfo);

				/*
				* Release buffer and remove from list
				* if reference goes to zero.
				*/
				if (unmap_and_deregister_buf(inst, binfo))
					dprintk(VIDC_ERR,
					"%s: buffer unmap failed\n", __func__);
				mutex_unlock(&inst->sync_lock);

				/*send event to client*/
				v4l2_event_queue_fh(&inst->event_handler,
					&buf_event);
				wake_up(&inst->kernel_event_queue);
				return;
			}
		default:
			break;
		}
		if (event == V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT) {
			dprintk(VIDC_DBG,
				"V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT\n");
			inst->reconfig_height = event_notify->height;
			inst->reconfig_width = event_notify->width;
			inst->in_reconfig = true;
		} else {
			dprintk(VIDC_DBG,
				"V4L2_EVENT_SEQ_CHANGED_SUFFICIENT\n");
			if (msm_comm_get_stream_output_mode(inst) !=
				HAL_VIDEO_DECODER_SECONDARY) {
				dprintk(VIDC_DBG,
					"event_notify->height = %d event_notify->width = %d\n",
					event_notify->height,
					event_notify->width);
				inst->prop.height[CAPTURE_PORT] =
					event_notify->height;
				inst->prop.width[CAPTURE_PORT] =
					event_notify->width;
				inst->prop.height[OUTPUT_PORT] =
					event_notify->height;
				inst->prop.width[OUTPUT_PORT] =
					event_notify->width;
			}
		}
		msm_comm_init_dcvs_load(inst);
		rc = msm_vidc_check_session_supported(inst);
		if (!rc)
			msm_vidc_queue_v4l2_event(inst, event);
		return;
	} else {
		dprintk(VIDC_ERR,
			"Failed to get valid response for event_change\n");
	}
}

static void handle_session_prop_info(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct getprop_buf *getprop;
	struct msm_vidc_inst *inst;
	if (!response || !response->data) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for prop info\n");
		return;
	}
	inst = (struct msm_vidc_inst *)response->session_id;
	mutex_lock(&inst->lock);
	getprop = kzalloc(sizeof(*getprop), GFP_KERNEL);
	if (!getprop) {
		dprintk(VIDC_ERR, "%s: getprop kzalloc failed\n", __func__);
		goto failed;
	}
	getprop->data = kmemdup(response->data, response->size, GFP_KERNEL);
	if (!getprop->data) {
		dprintk(VIDC_ERR, "%s: kmemdup failed\n", __func__);
		kfree(getprop);
		goto failed;
	}
	list_add_tail(&getprop->list, &inst->pending_getpropq);
	mutex_unlock(&inst->lock);
	signal_session_msg_receipt(cmd, inst);
	return;
failed:
	mutex_unlock(&inst->lock);
	return;
}

static void handle_load_resource_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		if (response->status) {
			dprintk(VIDC_ERR,
				"Load resource response from FW : 0x%x\n",
				response->status);
			msm_comm_generate_session_error(inst);
		}
	} else
		dprintk(VIDC_ERR,
			"Failed to get valid response for load resource\n");
}

static void handle_start_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
	} else {
		dprintk(VIDC_ERR,
			"Failed to get valid response for start\n");
	}
}

static void handle_stop_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
	} else {
		dprintk(VIDC_ERR,
			"Failed to get valid response for stop\n");
	}
}

static void handle_release_res_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		signal_session_msg_receipt(cmd, inst);
	} else {
		dprintk(VIDC_ERR,
			"Failed to get valid response for release resource\n");
	}
}
void validate_output_buffers(struct msm_vidc_inst *inst)
{
	struct internal_buf *binfo;
	u32 buffers_owned_by_driver = 0;
	struct hal_buffer_requirements *output_buf;
	output_buf = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!output_buf) {
		dprintk(VIDC_DBG,
			"This output buffer not required, buffer_type: %x\n",
			HAL_BUFFER_OUTPUT);
		return;
	}
	list_for_each_entry(binfo, &inst->outputbufs, list) {
		if (!binfo) {
			dprintk(VIDC_ERR, "Invalid parameter\n");
			return;
		}
		if (binfo->buffer_ownership != DRIVER) {
			dprintk(VIDC_DBG,
				"This buffer is with FW 0x%pa\n",
				&binfo->handle->device_addr);
			continue;
		}
		buffers_owned_by_driver++;
	}
	if (buffers_owned_by_driver != output_buf->buffer_count_actual)
		dprintk(VIDC_WARN,
			"OUTPUT Buffer count mismatch %d of %d\n",
			buffers_owned_by_driver,
			output_buf->buffer_count_actual);

	return;
}

int msm_comm_queue_output_buffers(struct msm_vidc_inst *inst)
{
	struct internal_buf *binfo;
	struct hfi_device *hdev;
	struct msm_smem *handle;
	struct vidc_frame_data frame_data = {0};
	struct hal_buffer_requirements *output_buf;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	output_buf = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!output_buf) {
		dprintk(VIDC_DBG,
			"This output buffer not required, buffer_type: %x\n",
			HAL_BUFFER_OUTPUT);
		return 0;
	}
	dprintk(VIDC_DBG,
		"output: num = %d, size = %d\n",
		output_buf->buffer_count_actual,
		output_buf->buffer_size);

	list_for_each_entry(binfo, &inst->outputbufs, list) {
		if (binfo->buffer_ownership != DRIVER)
			continue;
		handle = binfo->handle;
		frame_data.alloc_len = output_buf->buffer_size;
		frame_data.filled_len = 0;
		frame_data.offset = 0;
		frame_data.device_addr = handle->device_addr;
		frame_data.flags = 0;
		frame_data.extradata_addr = handle->device_addr +
		output_buf->buffer_size;
		frame_data.buffer_type = HAL_BUFFER_OUTPUT;
		rc = call_hfi_op(hdev, session_ftb,
			(void *) inst->session, &frame_data);
		binfo->buffer_ownership = FIRMWARE;
	}
	return 0;
}

static void handle_session_flush(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	int rc;
	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
			mutex_lock(&inst->lock);
			validate_output_buffers(inst);
			if (!inst->in_reconfig) {
				rc = msm_comm_queue_output_buffers(inst);
				if (rc) {
					dprintk(VIDC_ERR,
						"Failed to queue output buffers: %d\n",
						rc);
				}
			}
			mutex_unlock(&inst->lock);
		}
		msm_vidc_queue_v4l2_event(inst, V4L2_EVENT_MSM_VIDC_FLUSH_DONE);
	} else {
		dprintk(VIDC_ERR, "Failed to get valid response for flush\n");
	}
}

static void handle_sys_idle(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core = NULL;
	int rc = 0;

	if (!response) {
		rc = -EINVAL;
		goto exit;
	}

	core = get_vidc_core(response->device_id);
	if (!core) {
		dprintk(VIDC_ERR, "Received IDLE with invalid core\n");
		rc = -EINVAL;
		goto exit;
	}

	dprintk(VIDC_DBG, "SYS_IDLE received for core %p\n", core);
	if (core->resources.dynamic_bw_update) {
		mutex_lock(&core->lock);
		core->idle_stats.start_time = ktime_get();
		dprintk(VIDC_DBG, "%s: start idle time %llu us\n",
				__func__, ktime_to_us(
					core->idle_stats.start_time));
		core->idle_stats.idle = true;
		mutex_unlock(&core->lock);
	}

exit:
	if (rc)
		dprintk(VIDC_WARN, "Failed to handle idle message\n");
}

static void handle_session_error(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	int rc;
	struct hfi_device *hdev = NULL;
	struct msm_vidc_inst *inst = NULL;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for session error\n");
		return;
	}

	inst = (struct msm_vidc_inst *)response->session_id;

	if (!inst || !inst->session || !inst->core->device) {
		dprintk(VIDC_ERR,
				"Session (%p) not in a stable enough state to handle session error\n",
				inst);
		return;
	}

	hdev = inst->core->device;
	dprintk(VIDC_WARN, "Session error received for session %p\n", inst);
	change_inst_state(inst, MSM_VIDC_CORE_INVALID);

	mutex_lock(&inst->lock);
	dprintk(VIDC_DBG, "cleaning up inst: %p\n", inst);
	rc = call_hfi_op(hdev, session_clean, inst->session);
	if (rc)
		dprintk(VIDC_ERR, "Session (%p) clean failed: %d\n", inst, rc);

	inst->session = NULL;
	mutex_unlock(&inst->lock);

	if (response->status == VIDC_ERR_MAX_CLIENTS) {
		dprintk(VIDC_WARN,
			"send max clients reached error to client: %p\n",
			inst);
		msm_vidc_queue_v4l2_event(inst,
			V4L2_EVENT_MSM_VIDC_MAX_CLIENTS);
	} else {
		dprintk(VIDC_ERR,
			"send session error to client: %p\n",
			inst);
		msm_vidc_queue_v4l2_event(inst,
			V4L2_EVENT_MSM_VIDC_SYS_ERROR);
	}
}

struct sys_err_handler_data {
	struct msm_vidc_core *core;
	struct delayed_work work;
};


void hw_sys_error_handler(struct work_struct *work)
{
	struct msm_vidc_core *core = NULL;
	struct hfi_device *hdev = NULL;
	struct sys_err_handler_data *handler = NULL;
	int rc = 0;

	handler = container_of(work, struct sys_err_handler_data, work.work);
	if (!handler || !handler->core || !handler->core->device) {
		dprintk(VIDC_ERR, "%s - invalid work or core handle\n",
				__func__);
		goto exit;
	}

	core = handler->core;
	hdev = core->device;

	mutex_lock(&core->lock);
	/*
	* Restart the firmware to bring out of bad state.
	*/
	if ((core->state == VIDC_CORE_INVALID) &&
		hdev->resurrect_fw) {
		rc = call_hfi_op(hdev, resurrect_fw,
				hdev->hfi_device_data);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s - resurrect_fw failed: %d\n",
				__func__, rc);
		}
		core->state = VIDC_CORE_LOADED;
	} else {
		dprintk(VIDC_DBG,
			"fw unloaded after sys error, no need to resurrect\n");
	}
	mutex_unlock(&core->lock);

exit:
	/* free sys error handler, allocated in handle_sys_err */
	kfree(handler);
}

static void handle_sys_error(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core = NULL;
	struct sys_err_handler_data *handler = NULL;
	struct hfi_device *hdev = NULL;
	struct msm_vidc_inst *inst = NULL;
	int rc = 0;

	subsystem_crashed("venus");
	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for sys error\n");
		return;
	}

	core = get_vidc_core(response->device_id);
	if (!core) {
		dprintk(VIDC_ERR,
				"Got SYS_ERR but unable to identify core\n");
		return;
	}

	dprintk(VIDC_WARN, "SYS_ERROR %d received for core %p\n", cmd, core);
	mutex_lock(&core->lock);
	core->state = VIDC_CORE_INVALID;

	/*
	* 1. Delete each instance session from hfi list
	* 2. Notify all clients about hardware error.
	*/
	list_for_each_entry(inst, &core->instances,
			list) {
		mutex_lock(&inst->lock);
		inst->state = MSM_VIDC_CORE_INVALID;
		if (inst->core)
			hdev = inst->core->device;
		if (hdev && inst->session) {
			dprintk(VIDC_DBG,
			"cleaning up inst: 0x%p\n", inst);
			rc = call_hfi_op(hdev, session_clean,
				(void *) inst->session);
			if (rc)
				dprintk(VIDC_ERR,
					"Sess clean failed :%p\n",
					inst);
		}
		inst->session = NULL;
		mutex_unlock(&inst->lock);
		msm_vidc_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_VIDC_SYS_ERROR);
	}
	mutex_unlock(&core->lock);


	handler = kzalloc(sizeof(*handler), GFP_KERNEL);
	if (!handler) {
		dprintk(VIDC_ERR,
				"%s - failed to allocate sys error handler\n",
				__func__);
		return;
	}
	handler->core = core;
	INIT_DELAYED_WORK(&handler->work, hw_sys_error_handler);

	/*
	* Sleep for 5 sec to ensure venus has completed any
	* pending cache operations. Without this sleep, we see
	* device reset when firmware is unloaded after a sys
	* error.
	*/
	schedule_delayed_work(&handler->work, msecs_to_jiffies(5000));
}

static void handle_session_close(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct hfi_device *hdev = NULL;

	if (response) {
		inst = (struct msm_vidc_inst *)response->session_id;
		if (!inst || !inst->core || !inst->core->device) {
			dprintk(VIDC_ERR, "%s invalid params\n", __func__);
			return;
		}
		hdev = inst->core->device;
		mutex_lock(&inst->lock);
		if (inst->session) {
			dprintk(VIDC_DBG, "cleaning up inst: 0x%p\n", inst);
			call_hfi_op(hdev, session_clean,
				(void *) inst->session);
		}
		inst->session = NULL;
		mutex_unlock(&inst->lock);
		signal_session_msg_receipt(cmd, inst);
		show_stats(inst);
	} else {
		dprintk(VIDC_ERR,
			"Failed to get valid response for session close\n");
	}
}

static struct vb2_buffer *get_vb_from_device_addr(struct buf_queue *bufq,
		unsigned long dev_addr)
{
	struct vb2_buffer *vb = NULL;
	struct vb2_queue *q = NULL;
	int found = 0;
	if (!bufq) {
		dprintk(VIDC_ERR, "Invalid parameter\n");
		return NULL;
	}
	q = &bufq->vb2_bufq;
	mutex_lock(&bufq->lock);
	list_for_each_entry(vb, &q->queued_list, queued_entry) {
		if (vb->v4l2_planes[0].m.userptr == dev_addr &&
			vb->state == VB2_BUF_STATE_ACTIVE) {
			found = 1;
			dprintk(VIDC_DBG, "Found v4l2_buf index : %d\n",
					vb->v4l2_buf.index);
			break;
		}
	}
	mutex_unlock(&bufq->lock);
	if (!found) {
		dprintk(VIDC_DBG,
			"Failed to find buffer in queued list: 0x%lx, qtype = %d\n",
			dev_addr, q->type);
		vb = NULL;
	}
	return vb;
}

static void handle_ebd(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct vb2_buffer *vb;
	struct msm_vidc_inst *inst;
	struct vidc_hal_ebd *empty_buf_done;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}

	inst = (struct msm_vidc_inst *)response->session_id;
	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid response from vidc_hal\n",
			__func__);
		return;
	}
	vb = get_vb_from_device_addr(&inst->bufq[OUTPUT_PORT],
				response->clnt_data);
	if (vb) {
		vb->v4l2_planes[0].bytesused = response->input_done.filled_len;
		vb->v4l2_planes[0].data_offset = response->input_done.offset;
		if (vb->v4l2_planes[0].data_offset > vb->v4l2_planes[0].length)
			dprintk(VIDC_INFO, "data_offset overflow length\n");
		if (vb->v4l2_planes[0].bytesused > vb->v4l2_planes[0].length)
			dprintk(VIDC_INFO, "bytesused overflow length\n");
		if (vb->v4l2_planes[0].m.userptr !=
			response->input_done.packet_buffer)
			dprintk(VIDC_INFO, "Unexpected buffer address\n");
		empty_buf_done = (struct vidc_hal_ebd *)&response->input_done;
		if (empty_buf_done) {
			if (empty_buf_done->status == VIDC_ERR_NOT_SUPPORTED) {
				dprintk(VIDC_INFO,
					"Failed : Unsupported input stream\n");
				vb->v4l2_buf.flags |=
					V4L2_QCOM_BUF_INPUT_UNSUPPORTED;
			}
			if (empty_buf_done->status == VIDC_ERR_BITSTREAM_ERR) {
				dprintk(VIDC_INFO,
					"Failed : Corrupted input stream\n");
				vb->v4l2_buf.flags |=
					V4L2_QCOM_BUF_DATA_CORRUPT;
			}
			if (empty_buf_done->status ==
				VIDC_ERR_START_CODE_NOT_FOUND) {
				vb->v4l2_buf.flags |=
					V4L2_MSM_VIDC_BUF_START_CODE_NOT_FOUND;
				dprintk(VIDC_INFO,
					"Failed: Start code not found\n");
			}
		}
		dprintk(VIDC_DBG,
			"Got ebd from hal: device_addr: 0x%pa, alloc: %d, status: 0x%x, pic_type: 0x%x, flags: 0x%x\n",
			&empty_buf_done->packet_buffer,
			empty_buf_done->alloc_len, empty_buf_done->status,
			empty_buf_done->picture_type, empty_buf_done->flags);

		mutex_lock(&inst->bufq[OUTPUT_PORT].lock);
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		mutex_unlock(&inst->bufq[OUTPUT_PORT].lock);
		wake_up(&inst->kernel_event_queue);
		msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_EBD);
	}
}

int buf_ref_get(struct msm_vidc_inst *inst, struct buffer_info *binfo)
{
	int cnt = 0;

	if (!inst || !binfo)
		return -EINVAL;

	mutex_lock(&inst->lock);
	atomic_inc(&binfo->ref_count);
	cnt = atomic_read(&binfo->ref_count);
	if (cnt > 2) {
		dprintk(VIDC_DBG, "%s: invalid ref_cnt: %d\n", __func__, cnt);
		cnt = -EINVAL;
	}
	dprintk(VIDC_DBG, "REF_GET[%d] fd[0] = %d\n", cnt, binfo->fd[0]);
	mutex_unlock(&inst->lock);
	return cnt;
}

int buf_ref_put(struct msm_vidc_inst *inst, struct buffer_info *binfo)
{
	int rc = 0;
	int cnt;
	bool release_buf = false;
	bool qbuf_again = false;

	if (!inst || !binfo)
		return -EINVAL;

	mutex_lock(&inst->lock);
	atomic_dec(&binfo->ref_count);
	cnt = atomic_read(&binfo->ref_count);
	dprintk(VIDC_DBG, "REF_PUT[%d] fd[0] = %d\n", cnt, binfo->fd[0]);
	if (cnt == 0)
		release_buf = true;
	else if (cnt == 1)
		qbuf_again = true;
	else {
		dprintk(VIDC_DBG, "%s: invalid ref_cnt: %d\n", __func__, cnt);
		cnt = -EINVAL;
	}
	mutex_unlock(&inst->lock);

	if (cnt < 0)
		return cnt;

	rc = output_buffer_cache_invalidate(inst, binfo);
	if (rc)
		return rc;

	if (release_buf) {
		/*
		* We can not delete binfo here as we need to set the user
		* virtual address saved in binfo->uvaddr to the dequeued v4l2
		* buffer.
		*
		* We will set the pending_deletion flag to true here and delete
		* binfo from registered list in dqbuf after setting the uvaddr.
		*/
		dprintk(VIDC_DBG, "fd[0] = %d -> pending_deletion = true\n",
			binfo->fd[0]);
		binfo->pending_deletion = true;
	} else if (qbuf_again) {
		rc = qbuf_dynamic_buf(inst, binfo);
		if (!rc)
			return rc;
	}
	return cnt;
}

static void handle_dynamic_buffer(struct msm_vidc_inst *inst,
		ion_phys_addr_t device_addr, u32 flags)
{
	struct buffer_info *binfo = NULL;

	/*
	 * Update reference count and release OR queue back the buffer,
	 * only when firmware is not holding a reference.
	 */
	if (inst->buffer_mode_set[CAPTURE_PORT] == HAL_BUFFER_MODE_DYNAMIC) {
		binfo = device_to_uvaddr(inst, &inst->registered_bufs,
				device_addr);
		if (!binfo) {
			dprintk(VIDC_ERR,
				"%s buffer not found in registered list\n",
				__func__);
			return;
		}
		if (flags & HAL_BUFFERFLAG_READONLY) {
			dprintk(VIDC_DBG,
				"FBD fd[0] = %d -> Reference with f/w, addr: 0x%pa\n",
				binfo->fd[0], &device_addr);
		} else {
			dprintk(VIDC_DBG,
				"FBD fd[0] = %d -> FBD_ref_released, addr: 0x%pa\n",
				binfo->fd[0], &device_addr);
			buf_ref_put(inst, binfo);
		}
	}
}

static int handle_multi_stream_buffers(struct msm_vidc_inst *inst,
		ion_phys_addr_t dev_addr)
{
	struct internal_buf *binfo;
	struct msm_smem *handle;
	bool found = false;
	mutex_lock(&inst->lock);
	list_for_each_entry(binfo, &inst->outputbufs, list) {
		if (!binfo) {
			dprintk(VIDC_ERR, "Invalid parameter\n");
			break;
		}
		handle = binfo->handle;
		if (handle && dev_addr == handle->device_addr) {
			if (binfo->buffer_ownership == DRIVER) {
				dprintk(VIDC_ERR,
					"FW returned same buffer: 0x%pa\n",
					&dev_addr);
				break;
			}
			binfo->buffer_ownership = DRIVER;
			found = true;
			break;
		}
	}
	if (!found)
		dprintk(VIDC_ERR,
			"Failed to find output buffer in queued list: 0x%pa\n",
			&dev_addr);
	mutex_unlock(&inst->lock);
	return 0;
}

enum hal_buffer msm_comm_get_hal_output_buffer(struct msm_vidc_inst *inst)
{
	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY)
		return HAL_BUFFER_OUTPUT2;
	else
		return HAL_BUFFER_OUTPUT;
}

static void handle_fbd(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct msm_vidc_inst *inst;
	struct vb2_buffer *vb = NULL;
	struct vidc_hal_fbd *fill_buf_done;
	enum hal_buffer buffer_type;
	int extra_idx = 0;
	int64_t time_usec = 0;
	int rc = 0;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}
	inst = (struct msm_vidc_inst *)response->session_id;
	fill_buf_done = (struct vidc_hal_fbd *)&response->output_done;
	buffer_type = msm_comm_get_hal_output_buffer(inst);
	if (fill_buf_done->buffer_type == buffer_type) {
		vb = get_vb_from_device_addr(&inst->bufq[CAPTURE_PORT],
				fill_buf_done->packet_buffer1);
	} else {
		if (handle_multi_stream_buffers(inst,
				fill_buf_done->packet_buffer1))
			dprintk(VIDC_ERR,
				"Failed : Output buffer not found 0x%pa\n",
				&fill_buf_done->packet_buffer1);
		return;
	}
	if (vb) {
		vb->v4l2_planes[0].bytesused = fill_buf_done->filled_len1;
		vb->v4l2_planes[0].data_offset = fill_buf_done->offset1;
		vb->v4l2_planes[0].reserved[2] = fill_buf_done->start_x_coord;
		vb->v4l2_planes[0].reserved[3] = fill_buf_done->start_y_coord;
		vb->v4l2_planes[0].reserved[4] = fill_buf_done->frame_width;
		vb->v4l2_planes[0].reserved[5] = fill_buf_done->frame_height;
		if (vb->v4l2_planes[0].data_offset > vb->v4l2_planes[0].length)
			dprintk(VIDC_INFO,
				"fbd:Overflow data_offset = %d; length = %d\n",
				vb->v4l2_planes[0].data_offset,
				vb->v4l2_planes[0].length);
		if (vb->v4l2_planes[0].bytesused > vb->v4l2_planes[0].length)
			dprintk(VIDC_INFO,
				"fbd:Overflow bytesused = %d; length = %d\n",
				vb->v4l2_planes[0].bytesused,
				vb->v4l2_planes[0].length);
		if (!(fill_buf_done->flags1 &
			HAL_BUFFERFLAG_TIMESTAMPINVALID) &&
			fill_buf_done->filled_len1) {
			time_usec = fill_buf_done->timestamp_hi;
			time_usec = (time_usec << 32) |
				fill_buf_done->timestamp_lo;
		} else {
			time_usec = 0;
			dprintk(VIDC_DBG,
					"Set zero timestamp for buffer 0x%pa, filled: %d, (hi:%u, lo:%u)\n",
					&fill_buf_done->packet_buffer1,
					fill_buf_done->filled_len1,
					fill_buf_done->timestamp_hi,
					fill_buf_done->timestamp_lo);
		}
		vb->v4l2_buf.timestamp =
			ns_to_timeval(time_usec * NSEC_PER_USEC);
		vb->v4l2_buf.flags = 0;
		extra_idx =
			EXTRADATA_IDX(inst->fmts[CAPTURE_PORT]->num_planes);
		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			vb->v4l2_planes[extra_idx].m.userptr =
				(unsigned long)fill_buf_done->extra_data_buffer;
			vb->v4l2_planes[extra_idx].bytesused =
				vb->v4l2_planes[extra_idx].length;
			vb->v4l2_planes[extra_idx].data_offset = 0;
		}

		handle_dynamic_buffer(inst, fill_buf_done->packet_buffer1,
					fill_buf_done->flags1);
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_READONLY)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_FLAG_READONLY;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_EOS)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_FLAG_EOS;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_CODECCONFIG)
			vb->v4l2_buf.flags &= ~V4L2_QCOM_BUF_FLAG_CODECCONFIG;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_SYNCFRAME)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_FLAG_IDRFRAME;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_EOSEQ)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_FLAG_EOSEQ;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DECODEONLY)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_FLAG_DECODEONLY;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DATACORRUPT)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_DATA_CORRUPT;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DROP_FRAME)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_DROP_FRAME;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_MBAFF)
			vb->v4l2_buf.flags |= V4L2_MSM_BUF_FLAG_MBAFF;
		if (fill_buf_done->flags1 &
			HAL_BUFFERFLAG_TS_DISCONTINUITY)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_TS_DISCONTINUITY;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_TS_ERROR)
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_TS_ERROR;
		switch (fill_buf_done->picture_type) {
		case HAL_PICTURE_IDR:
			vb->v4l2_buf.flags |= V4L2_QCOM_BUF_FLAG_IDRFRAME;
			vb->v4l2_buf.flags |= V4L2_BUF_FLAG_KEYFRAME;
			break;
		case HAL_PICTURE_I:
			vb->v4l2_buf.flags |= V4L2_BUF_FLAG_KEYFRAME;
			break;
		case HAL_PICTURE_P:
			vb->v4l2_buf.flags |= V4L2_BUF_FLAG_PFRAME;
			break;
		case HAL_PICTURE_B:
			vb->v4l2_buf.flags |= V4L2_BUF_FLAG_BFRAME;
			break;
		case HAL_FRAME_NOTCODED:
		case HAL_UNUSED_PICT:
			/* Do we need to care about these? */
		case HAL_FRAME_YUV:
			break;
		default:
			break;
		}
		if (msm_vidc_dcvs_mode && inst->dcvs_mode) {
			msm_comm_monitor_ftb(inst);
			rc = msm_comm_scale_clocks_dcvs(inst, true);
			if (rc)
				dprintk(VIDC_WARN,
					"%s: Failed to scale clocks in DCVS: %d\n",
					__func__, rc);
		}
		inst->count.fbd++;
		if (fill_buf_done->filled_len1)
			msm_vidc_debugfs_update(inst,
				MSM_VIDC_DEBUGFS_EVENT_FBD);

		if (extra_idx && (extra_idx < VIDEO_MAX_PLANES)) {
			dprintk(VIDC_DBG,
				"extradata: userptr = %p;"
				" bytesused = %d; length = %d\n",
				(u8 *)vb->v4l2_planes[extra_idx].m.userptr,
				vb->v4l2_planes[extra_idx].bytesused,
				vb->v4l2_planes[extra_idx].length);
		}
		dprintk(VIDC_DBG,
		"Got fbd from hal: device_addr: 0x%pa, alloc: %d, filled: %d, offset: %d, ts: %lld, flags: 0x%x, crop: %d %d %d %d, pic_type: 0x%x\n",
		&fill_buf_done->packet_buffer1, fill_buf_done->alloc_len1,
		fill_buf_done->filled_len1, fill_buf_done->offset1, time_usec,
		fill_buf_done->flags1, fill_buf_done->start_x_coord,
		fill_buf_done->start_y_coord, fill_buf_done->frame_width,
		fill_buf_done->frame_height, fill_buf_done->picture_type);

		mutex_lock(&inst->bufq[CAPTURE_PORT].lock);
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		mutex_unlock(&inst->bufq[CAPTURE_PORT].lock);
		wake_up(&inst->kernel_event_queue);
	}
}

static void handle_seq_hdr_done(enum command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct msm_vidc_inst *inst;
	struct vb2_buffer *vb;
	struct vidc_hal_fbd *fill_buf_done;
	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}
	inst = (struct msm_vidc_inst *)response->session_id;
	fill_buf_done = (struct vidc_hal_fbd *)&response->output_done;
	vb = get_vb_from_device_addr(&inst->bufq[CAPTURE_PORT],
				fill_buf_done->packet_buffer1);
	if (!vb) {
		dprintk(VIDC_ERR,
				"Failed to find video buffer for seq_hdr_done\n");
		return;
	}

	vb->v4l2_planes[0].bytesused = fill_buf_done->filled_len1;
	vb->v4l2_planes[0].data_offset = fill_buf_done->offset1;

	vb->v4l2_buf.flags = V4L2_QCOM_BUF_FLAG_CODECCONFIG;
	vb->v4l2_buf.timestamp = ns_to_timeval(0);

	dprintk(VIDC_DBG, "Filled length = %d; offset = %d; flags %x\n",
				vb->v4l2_planes[0].bytesused,
				vb->v4l2_planes[0].data_offset,
				vb->v4l2_buf.flags);
	mutex_lock(&inst->bufq[CAPTURE_PORT].lock);
	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	mutex_unlock(&inst->bufq[CAPTURE_PORT].lock);
}

void handle_cmd_response(enum command_response cmd, void *data)
{
	dprintk(VIDC_DBG, "Command response = %d\n", cmd);
	switch (cmd) {
	case SYS_INIT_DONE:
		handle_sys_init_done(cmd, data);
		break;
	case RELEASE_RESOURCE_DONE:
		handle_sys_release_res_done(cmd, data);
		break;
	case SESSION_INIT_DONE:
		handle_session_init_done(cmd, data);
		break;
	case SESSION_PROPERTY_INFO:
		handle_session_prop_info(cmd, data);
		break;
	case SESSION_LOAD_RESOURCE_DONE:
		handle_load_resource_done(cmd, data);
		break;
	case SESSION_START_DONE:
		handle_start_done(cmd, data);
		break;
	case SESSION_ETB_DONE:
		handle_ebd(cmd, data);
		break;
	case SESSION_FTB_DONE:
		handle_fbd(cmd, data);
		break;
	case SESSION_STOP_DONE:
		handle_stop_done(cmd, data);
		break;
	case SESSION_RELEASE_RESOURCE_DONE:
		handle_release_res_done(cmd, data);
		break;
	case SESSION_END_DONE:
	case SESSION_ABORT_DONE:
		handle_session_close(cmd, data);
		break;
	case VIDC_EVENT_CHANGE:
		handle_event_change(cmd, data);
		break;
	case SESSION_FLUSH_DONE:
		handle_session_flush(cmd, data);
		break;
	case SESSION_GET_SEQ_HDR_DONE:
		handle_seq_hdr_done(cmd, data);
		break;
	case SYS_WATCHDOG_TIMEOUT:
		handle_sys_error(cmd, data);
		break;
	case SYS_ERROR:
		handle_sys_error(cmd, data);
		break;
	case SESSION_ERROR:
		handle_session_error(cmd, data);
		break;
	case SESSION_RELEASE_BUFFER_DONE:
		handle_session_release_buf_done(cmd, data);
		break;
	case SYS_IDLE:
		handle_sys_idle(cmd, data);
		break;
	default:
		dprintk(VIDC_ERR, "response unhandled\n");
		break;
	}
}

static inline int get_pending_bufs_fw(struct msm_vidc_inst *inst)
{
	int fw_out_qsize = 0;
	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&inst->lock);
	if (inst->state >= MSM_VIDC_OPEN_DONE &&
		inst->state < MSM_VIDC_STOP_DONE)
		fw_out_qsize = inst->count.ftb - inst->count.fbd;
	mutex_unlock(&inst->lock);

	return fw_out_qsize;
}

static inline void msm_comm_print_dcvs_stats(struct dcvs_stats *dcvs)
{
	dprintk(VIDC_DBG,
		"DCVS: Load_Low %d, Load High %d\n",
		dcvs->load_low,
		dcvs->load_high);

	dprintk(VIDC_DBG,
		"DCVS: ThrDispBufLow %d, ThrDispBufHigh %d\n",
		dcvs->threshold_disp_buf_low,
		dcvs->threshold_disp_buf_high);

	dprintk(VIDC_DBG,
		"DCVS: min_threshold %d, max_threshold %d\n",
		dcvs->min_threshold,
		dcvs->max_threshold);
}

void msm_comm_init_dcvs_load(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	struct hal_buffer_requirements *output_buf_req;
	struct dcvs_stats *dcvs;

	dprintk(VIDC_DBG, "Init DCVS Load\n");

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
		return;
	}

	core = inst->core;
	dcvs = &inst->dcvs;
	mutex_lock(&inst->lock);
	dcvs->load = msm_comm_get_inst_load(inst, LOAD_CALC_NO_QUIRKS);
	mutex_unlock(&inst->lock);

	if (dcvs->load >= DCVS_NOMINAL_LOAD) {
		dcvs->load_low = DCVS_NOMINAL_LOAD;
		dcvs->load_high = core->resources.max_load;
	}

	mutex_lock(&inst->lock);
	output_buf_req = get_buff_req_buffer(inst,
		msm_comm_get_hal_output_buffer(inst));
	mutex_unlock(&inst->lock);

	if (!output_buf_req) {
		dprintk(VIDC_ERR,
			"%s: No buffer requirement for buffer type %x\n",
			__func__, HAL_BUFFER_OUTPUT);
		return;
	}

	/* calculating the min and max threshold */
	if (output_buf_req->buffer_count_actual) {
		dcvs->min_threshold = DCVS_MIN_DRAIN_RATE;
		dcvs->max_threshold =
			output_buf_req->buffer_count_actual -
			(DCVS_BUFFER_WITH_DEC + DCVS_BUFFER_SAFEGUARD +
			DCVS_BUFFER_RELEASED_DEC);

	if (dcvs->max_threshold - dcvs->min_threshold <
		DCVS_BUFFER_SAFEGUARD)
		dcvs->max_threshold =
			dcvs->min_threshold + DCVS_BUFFER_SAFEGUARD;

	dcvs->threshold_disp_buf_low =
		clamp(dcvs->threshold_disp_buf_low,
				dcvs->min_threshold,
				dcvs->max_threshold);

	dcvs->threshold_disp_buf_high =
		clamp(dcvs->threshold_disp_buf_high,
				dcvs->min_threshold,
				dcvs->max_threshold);
	}

	if (dcvs->threshold_disp_buf_high - dcvs->threshold_disp_buf_low
		< DCVS_MIN_THRESHOLD_DIFF) {
		if (dcvs->max_threshold - dcvs->min_threshold <=
			DCVS_MIN_THRESHOLD_DIFF) {
			dcvs->threshold_disp_buf_low = dcvs->min_threshold;
			dcvs->threshold_disp_buf_high = dcvs->max_threshold;
		} else if (dcvs->threshold_disp_buf_low ==
			dcvs->min_threshold) {
			dcvs->threshold_disp_buf_high =
				dcvs->threshold_disp_buf_low +
				DCVS_MIN_THRESHOLD_DIFF;
		} else {
			dcvs->threshold_disp_buf_high = dcvs->max_threshold;
			dcvs->threshold_disp_buf_low =
				dcvs->max_threshold - DCVS_MIN_THRESHOLD_DIFF;
		}
	}
	msm_comm_print_dcvs_stats(dcvs);
}

void msm_comm_init_dcvs(struct msm_vidc_inst *inst)
{
	dprintk(VIDC_DBG, "Init DCVS Struct\n");

	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
		return;
	}

	inst->dcvs = (struct dcvs_stats){ {0} };
	inst->dcvs.threshold_disp_buf_high = DCVS_NOMINAL_THRESHOLD;
	inst->dcvs.threshold_disp_buf_low = DCVS_TURBO_THRESHOLD;
}

static void msm_comm_monitor_ftb(struct msm_vidc_inst *inst)
{
	int new_ftb = 0;
	int new_ftb_temp = 0;
	int i;
	struct dcvs_stats *dcvs;

	if (!inst) {
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, inst);
		return;
	}

	dcvs = &inst->dcvs;
	/* update FTB stats only in nominal mode */
	if (dcvs->load == dcvs->load_low) {
		dcvs->num_ftb[dcvs->ftb_index] =
			inst->count.ftb - dcvs->prev_ftb_count;
		dcvs->ftb_index =
			(dcvs->ftb_index + 1) % DCVS_FTB_WINDOW;
		if (dcvs->ftb_counter < DCVS_FTB_WINDOW)
			dcvs->ftb_counter++;
	}
	dcvs->prev_ftb_count = inst->count.ftb;

	if (dcvs->load == dcvs->load_low
		&& dcvs->change_initial_freq
		&& dcvs->ftb_counter == DCVS_FTB_WINDOW) {

		/*
		* Low threshold =
		* min( max(avg of 4 FTB in stats window), max threshold)
		*/
		for (i = 0; i <= dcvs->ftb_counter - DCVS_FTB_STAT_SAMPLES;
			i++) {
			new_ftb_temp = SUM_ARRAY(dcvs->num_ftb, i,
				i + DCVS_FTB_STAT_SAMPLES - 1);
			new_ftb_temp = DIV_ROUND_UP(new_ftb_temp,
				DCVS_FTB_STAT_SAMPLES);
			new_ftb = max(new_ftb_temp, new_ftb);
		}
		dprintk(VIDC_PROF,
			"DCVS: Max FTB_count for Low_thr %d\n", new_ftb);

		new_ftb = min(new_ftb, dcvs->max_threshold);
		dcvs->threshold_disp_buf_low = new_ftb;

		/*
		* High Threshold =
		* max(max(8, Low threshold + 4),
		* round( (sumof(num_ftb[])*8)/DCVS_FTB_WINDOW)
		*/
		new_ftb = 0;
		for (i = 0; i < dcvs->ftb_counter; i++)
			new_ftb += dcvs->num_ftb[i];

		new_ftb = ((new_ftb * DCVS_EMP_THRESHOLD_HIGH) /
			DCVS_FTB_WINDOW);
		dprintk(VIDC_PROF,
			"DCVS: Avg FTB_count for High_Thr %d\n",
			new_ftb);

		if (dcvs->threshold_disp_buf_low +
			DCVS_TURBO_THRESHOLD > DCVS_NOMINAL_THRESHOLD) {
			dcvs->threshold_disp_buf_high =
				dcvs->threshold_disp_buf_low +
				DCVS_TURBO_THRESHOLD;
		} else {
			dcvs->threshold_disp_buf_high =
				DCVS_NOMINAL_THRESHOLD;
		}

		if (new_ftb > dcvs->threshold_disp_buf_high)
			dcvs->threshold_disp_buf_high = new_ftb;

		dcvs->threshold_disp_buf_high =
			clamp(dcvs->threshold_disp_buf_high,
				dcvs->min_threshold,
				dcvs->max_threshold);

		/*
		* Ensure that high threshold is
		* always greater than low threshold
		*/

		if (dcvs->threshold_disp_buf_high <
				dcvs->threshold_disp_buf_low) {
			dcvs->threshold_disp_buf_high =
				dcvs->threshold_disp_buf_low +
				DCVS_BUFFER_SAFEGUARD;
		}
	}

	dprintk(VIDC_DBG,
		"DCVS: threshold_low %d, threshold_high %d, load %d\n",
		dcvs->threshold_disp_buf_low,
		dcvs->threshold_disp_buf_high,
		dcvs->load);
}

/*
* In DCVS scale_clocks will be done both in qbuf and FBD
* 1 indicates call made from fbd that lowers clock
* 0 indicates call made from qbuf that increases clock
* based on DCVS algorithm
*/
static int msm_comm_scale_clocks_dcvs(struct msm_vidc_inst *inst, bool fbd)
{
	int rc = 0;
	int fw_pending_bufs = 0;
	int total_output_buf = 0;
	int buffers_outside_fw = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	struct hal_buffer_requirements *output_buf_req;
	struct dcvs_stats *dcvs;
	u32 codecs_enabled = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	hdev = core->device;
	dcvs = &inst->dcvs;
	fw_pending_bufs = get_pending_bufs_fw(inst) +
		(fbd ? 0 : 1);

	mutex_lock(&inst->lock);
	output_buf_req = get_buff_req_buffer(inst,
		msm_comm_get_hal_output_buffer(inst));
	mutex_unlock(&inst->lock);

	if (!output_buf_req) {
		dprintk(VIDC_ERR,
			"%s: No buffer requirement for buffer type %x\n",
			__func__, HAL_BUFFER_OUTPUT);
		return -EINVAL;
	}

	codecs_enabled = VIDC_VOTE_DATA_SESSION_VAL(
			get_hal_codec_type(inst->fmts[OUTPUT_PORT]->fourcc),
			get_hal_domain(inst->session_type));

	/* Total number of output buffers */
	total_output_buf = output_buf_req->buffer_count_actual;

	/* Buffers outside FW are with display */
	buffers_outside_fw = total_output_buf - fw_pending_bufs;

	if (fbd) {
		/*
		* scale clock only after display has buffers
		* more than high threshold
		*/
		if (!dcvs->change_initial_freq &&
			buffers_outside_fw > dcvs->threshold_disp_buf_high)
			dcvs->change_initial_freq = true;

		if (buffers_outside_fw > dcvs->threshold_disp_buf_high &&
			!dcvs->prev_freq_increased) {
			dcvs->load = dcvs->load_low;
			dcvs->prev_freq_lowered = true;
		} else
			dcvs->prev_freq_lowered = false;

		if (dcvs->prev_freq_lowered) {
			dprintk(VIDC_PROF,
				"DCVS: fbd clock set = %d tot_output_buf = %d buffers_outside_fw %d threshold_high %d\n",
				dcvs->load,
				total_output_buf,
				buffers_outside_fw,
				dcvs->threshold_disp_buf_high);

			/* HFI call to scale clock */
			rc = call_hfi_op(hdev, scale_clocks,
				hdev->hfi_device_data, dcvs->load,
				codecs_enabled);
			if (rc)
				dprintk(VIDC_ERR,
					"Failed to set clock rate in FBD: %d\n",
					rc);
		}
	} else {
		if (buffers_outside_fw <= dcvs->threshold_disp_buf_low &&
			!dcvs->prev_freq_lowered) {
			dcvs->load = dcvs->load_high;
			dcvs->prev_freq_increased = true;
		} else
			dcvs->prev_freq_increased = false;

		if (dcvs->prev_freq_increased) {
			dprintk(VIDC_PROF,
				"DCVS: ftb clock set = %d tot_output_buf = %d buffers_outside_fw %d, threshold_low %d\n",
				dcvs->load,
				total_output_buf,
				buffers_outside_fw,
				dcvs->threshold_disp_buf_low);

			/* HFI call to scale clock */
			rc = call_hfi_op(hdev, scale_clocks,
				hdev->hfi_device_data, dcvs->load,
				codecs_enabled);
			if (rc)
				dprintk(VIDC_ERR,
				"Failed to set clock rate in FTB %d\n",
				rc);
		}
	}
	return rc;
}

static int msm_comm_scale_clocks(struct msm_vidc_core *core)
{
	int num_mbs_per_sec;
	u32 codecs_enabled = 0;
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_inst *inst = NULL;

	if (!core) {
		dprintk(VIDC_ERR, "%s Invalid args: %p\n", __func__, core);
		return -EINVAL;
	}

	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s Invalid device handle: %p\n",
			__func__, hdev);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		int codec = 0;

		codec = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[OUTPUT_PORT]->fourcc :
			inst->fmts[CAPTURE_PORT]->fourcc;

		codecs_enabled |= VIDC_VOTE_DATA_SESSION_VAL(
				get_hal_codec_type(codec),
				get_hal_domain(inst->session_type));

	}
	mutex_unlock(&core->lock);
	num_mbs_per_sec =
		msm_comm_get_load(core, MSM_VIDC_ENCODER, LOAD_CALC_NO_QUIRKS) +
		msm_comm_get_load(core, MSM_VIDC_DECODER, LOAD_CALC_NO_QUIRKS);


	dprintk(VIDC_INFO, "num_mbs_per_sec = %d codecs_enabled 0x%x\n",
			num_mbs_per_sec, codecs_enabled);
	rc = call_hfi_op(hdev, scale_clocks,
		hdev->hfi_device_data, num_mbs_per_sec, codecs_enabled);
	if (rc)
		dprintk(VIDC_ERR, "Failed to set clock rate: %d\n", rc);
	return rc;
}

void msm_comm_scale_clocks_and_bus(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return;
	}
	core = inst->core;
	hdev = core->device;

	if (msm_comm_scale_clocks(core)) {
		dprintk(VIDC_WARN,
				"Failed to scale clocks. Performance might be impacted\n");
	}
	if (msm_comm_vote_bus(core)) {
		dprintk(VIDC_WARN,
				"Failed to scale DDR bus. Performance might be impacted\n");
	}
}

static int msm_comm_unset_ocmem(struct msm_vidc_core *core)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = core->device;

	if (core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"Core is in bad state. Cannot unset ocmem\n");
		return -EIO;
	}

	init_completion(
		&core->completions[SYS_MSG_INDEX(RELEASE_RESOURCE_DONE)]);

	rc = call_hfi_op(hdev, unset_ocmem, hdev->hfi_device_data);
	if (rc) {
		dprintk(VIDC_INFO, "Failed to unset OCMEM on driver\n");
		goto release_ocmem_failed;
	}
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(RELEASE_RESOURCE_DONE)],
		msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "%s: Wait interrupted or timed out: %d\n",
				__func__, SYS_MSG_INDEX(RELEASE_RESOURCE_DONE));
		rc = -EIO;
	}
release_ocmem_failed:
	return rc;
}

static int msm_comm_init_core_done(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core = inst->core;
	int rc = 0;
	mutex_lock(&core->lock);
	if (core->state >= VIDC_CORE_INIT_DONE) {
		dprintk(VIDC_INFO, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_inited;
	}
	dprintk(VIDC_DBG, "Waiting for SYS_INIT_DONE\n");
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(SYS_INIT_DONE)],
		msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "%s: Wait interrupted or timed out: %d\n",
				__func__, SYS_MSG_INDEX(SYS_INIT_DONE));
		rc = -EIO;
		goto exit;
	} else {
		core->state = VIDC_CORE_INIT_DONE;
	}
	dprintk(VIDC_DBG, "SYS_INIT_DONE!!!\n");
core_already_inited:
	change_inst_state(inst, MSM_VIDC_CORE_INIT_DONE);
	rc = 0;
exit:
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_comm_init_core(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core = inst->core;
	struct hfi_device *hdev;

	if (!core || !core->device)
		return -EINVAL;
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state >= VIDC_CORE_INIT) {
		dprintk(VIDC_INFO, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_inited;
	}
	mutex_unlock(&core->lock);

	rc = msm_comm_vote_bus(core);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to scale DDR bus: %d\n", rc);
		goto fail_vote_bus;
	}

	mutex_lock(&core->lock);
	if (core->state < VIDC_CORE_LOADED) {
		rc = call_hfi_op(hdev, load_fw, hdev->hfi_device_data);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to load video firmware\n");
			goto fail_load_fw;
		}
		core->state = VIDC_CORE_LOADED;
	}
	mutex_unlock(&core->lock);

	rc = msm_comm_scale_clocks(core);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to scale clocks: %d\n", rc);
		goto fail_core_init;
	}

	mutex_lock(&core->lock);
	if (core->state == VIDC_CORE_LOADED) {
		init_completion(&core->completions
			[SYS_MSG_INDEX(SYS_INIT_DONE)]);
		rc = call_hfi_op(hdev, core_init, hdev->hfi_device_data);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to init core, id = %d\n",
				core->id);
			goto fail_core_init;
		}
		core->state = VIDC_CORE_INIT;
	}

core_already_inited:
	change_inst_state(inst, MSM_VIDC_CORE_INIT);
	mutex_unlock(&core->lock);
	return rc;
fail_core_init:
	call_hfi_op(hdev, unload_fw, hdev->hfi_device_data);
fail_load_fw:
	msm_comm_unvote_buses(core);
fail_vote_bus:
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_vidc_deinit_core(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == VIDC_CORE_UNINIT) {
		dprintk(VIDC_INFO, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_uninited;
	}
	mutex_unlock(&core->lock);

	msm_comm_scale_clocks_and_bus(inst);

	mutex_lock(&core->lock);
	if (list_empty(&core->instances)) {
		if (core->state > VIDC_CORE_INIT) {
			if (core->resources.ocmem_size) {
				if (inst->state != MSM_VIDC_CORE_INVALID)
					msm_comm_unset_ocmem(core);
				call_hfi_op(hdev, free_ocmem,
						hdev->hfi_device_data);
			}
			dprintk(VIDC_DBG, "Calling vidc_hal_core_release\n");
			rc = call_hfi_op(hdev, core_release,
					hdev->hfi_device_data);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to release core, id = %d\n",
					core->id);
				goto exit;
			}
		}

		core->state = VIDC_CORE_UNINIT;

		call_hfi_op(hdev, unload_fw, hdev->hfi_device_data);
		msm_comm_unvote_buses(core);
	}

core_already_uninited:
	change_inst_state(inst, MSM_VIDC_CORE_UNINIT);
exit:
	mutex_unlock(&core->lock);
	return rc;
}

int msm_comm_force_cleanup(struct msm_vidc_inst *inst)
{
	return msm_vidc_deinit_core(inst);
}

static int msm_comm_session_init(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc = 0;
	int fourcc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_OPEN)) {
		dprintk(VIDC_INFO, "inst: %p is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	if (inst->session_type == MSM_VIDC_DECODER) {
		fourcc = inst->fmts[OUTPUT_PORT]->fourcc;
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		fourcc = inst->fmts[CAPTURE_PORT]->fourcc;
	} else {
		dprintk(VIDC_ERR, "Invalid session\n");
		return -EINVAL;
	}
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_INIT_DONE)]);
	mutex_lock(&inst->lock);
	inst->session = call_hfi_op(hdev, session_init, hdev->hfi_device_data,
			inst, get_hal_domain(inst->session_type),
			get_hal_codec_type(fourcc));
	mutex_unlock(&inst->lock);
	if (!inst->session) {
		dprintk(VIDC_ERR,
			"Failed to call session init for: %p, %p, %d, %d\n",
			inst->core->device, inst,
			inst->session_type, fourcc);
		goto exit;
	}
	inst->ftb_count = 0;
	change_inst_state(inst, MSM_VIDC_OPEN);
exit:
	return rc;
}

static void msm_vidc_print_running_insts(struct msm_vidc_core *core)
{
	struct msm_vidc_inst *temp;
	dprintk(VIDC_ERR, "Running instances:\n");
	dprintk(VIDC_ERR, "%4s|%4s|%4s|%4s|%4s\n",
			"type", "w", "h", "fps", "prop");

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		mutex_lock(&temp->lock);
		if (temp->state >= MSM_VIDC_OPEN_DONE &&
				temp->state < MSM_VIDC_STOP_DONE) {
			char properties[4] = "";

			if (is_thumbnail_session(temp))
				strlcat(properties, "N", sizeof(properties));

			if (is_turbo_session(temp))
				strlcat(properties, "T", sizeof(properties));

			dprintk(VIDC_ERR, "%4d|%4d|%4d|%4d|%4s\n",
					temp->session_type,
					temp->prop.width[CAPTURE_PORT],
					temp->prop.height[CAPTURE_PORT],
					temp->prop.fps, properties);
		}
		mutex_unlock(&temp->lock);
	}
	mutex_unlock(&core->lock);
}

static int msm_vidc_load_resources(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	int num_mbs_per_sec = 0;
	int height, width;
	struct msm_vidc_core *core;
	enum load_calc_quirks quirks = LOAD_CALC_IGNORE_TURBO_LOAD |
		LOAD_CALC_IGNORE_THUMBNAIL_LOAD;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	if (core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"Core is in bad state can't do load res\n");
		return -EINVAL;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"Instance is in invalid state can't do load res\n");
		return -EINVAL;
	}

	num_mbs_per_sec =
		msm_comm_get_load(core, MSM_VIDC_DECODER, quirks) +
		msm_comm_get_load(core, MSM_VIDC_ENCODER, quirks);

	if (num_mbs_per_sec > core->resources.max_load) {
		dprintk(VIDC_ERR, "HW is overloaded, needed: %d max: %d\n",
			num_mbs_per_sec, core->resources.max_load);
		msm_vidc_print_running_insts(core);
		inst->state = MSM_VIDC_CORE_INVALID;
		msm_comm_kill_session(inst);
		return -EBUSY;
	}

	hdev = core->device;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_LOAD_RESOURCES)) {
		dprintk(VIDC_INFO, "inst: %p is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	if (core->resources.ocmem_size) {
		height = max(inst->prop.height[CAPTURE_PORT],
			inst->prop.height[OUTPUT_PORT]);
		width = max(inst->prop.width[CAPTURE_PORT],
			inst->prop.width[OUTPUT_PORT]);
		rc = msm_comm_vote_bus(core);
		if (!rc) {
			mutex_lock(&core->lock);
			rc = call_hfi_op(hdev, alloc_ocmem,
					hdev->hfi_device_data,
					core->resources.ocmem_size);
			mutex_unlock(&core->lock);
			if (rc) {
				dprintk(VIDC_WARN,
				"Failed to allocate OCMEM. Performance will be impacted\n");
			}
		} else {
			dprintk(VIDC_WARN,
			"Failed to vote for OCMEM BW. Performance will be impacted\n");
		}
	}
	rc = call_hfi_op(hdev, session_load_res, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send load resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_LOAD_RESOURCES);
exit:
	return rc;
}

static int msm_vidc_start(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_VIDC_CORE_INVALID ||
			inst->core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"Core is in bad state can't do start\n");
		return -EINVAL;
	}

	hdev = inst->core->device;

	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_START)) {
		dprintk(VIDC_INFO,
			"inst: %p is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_START_DONE)]);
	rc = call_hfi_op(hdev, session_start, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send start\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_START);
exit:
	return rc;
}

static int msm_vidc_stop(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_STOP)) {
		dprintk(VIDC_INFO,
			"inst: %p is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	dprintk(VIDC_DBG, "Send Stop to hal\n");
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_STOP_DONE)]);
	rc = call_hfi_op(hdev, session_stop, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to send stop\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_STOP);
exit:
	return rc;
}

static int msm_vidc_release_res(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_RELEASE_RESOURCES)) {
		dprintk(VIDC_INFO,
			"inst: %p is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	dprintk(VIDC_DBG,
		"Send release res to hal\n");
	init_completion(
	&inst->completions[SESSION_MSG_INDEX(SESSION_RELEASE_RESOURCE_DONE)]);
	rc = call_hfi_op(hdev, session_release_res, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send release resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_RELEASE_RESOURCES);
exit:
	return rc;
}

static int msm_comm_session_close(int flipped_state,
			struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_CLOSE)) {
		dprintk(VIDC_INFO,
			"inst: %p is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	dprintk(VIDC_DBG,
		"Send session close to hal\n");
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_END_DONE)]);
	rc = call_hfi_op(hdev, session_end, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send close\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_CLOSE);
exit:
	return rc;
}

static int get_flipped_state(int present_state,
	int desired_state)
{
	int flipped_state = present_state;
	if (flipped_state < MSM_VIDC_STOP
			&& desired_state > MSM_VIDC_STOP) {
		flipped_state = MSM_VIDC_STOP + (MSM_VIDC_STOP - flipped_state);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	} else if (flipped_state > MSM_VIDC_STOP
			&& desired_state < MSM_VIDC_STOP) {
		flipped_state = MSM_VIDC_STOP -
			(flipped_state - MSM_VIDC_STOP + 1);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	}
	return flipped_state;
}

struct hal_buffer_requirements *get_buff_req_buffer(
		struct msm_vidc_inst *inst, enum hal_buffer buffer_type)
{
	int i;
	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		if (inst->buff_req.buffer[i].buffer_type == buffer_type)
			return &inst->buff_req.buffer[i];
	}
	return NULL;
}

static int set_output_buffers(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	int rc = 0;
	struct msm_smem *handle;
	struct internal_buf *binfo;
	struct vidc_buffer_addr_info buffer_info = {0};
	u32 smem_flags = 0, buffer_size;
	struct hal_buffer_requirements *output_buf, *extradata_buf;
	int i;
	struct hfi_device *hdev;

	hdev = inst->core->device;

	output_buf = get_buff_req_buffer(inst, buffer_type);
	if (!output_buf) {
		dprintk(VIDC_DBG,
			"This output buffer not required, buffer_type: %x\n",
			buffer_type);
		return 0;
	}
	dprintk(VIDC_DBG,
		"output: num = %d, size = %d\n",
		output_buf->buffer_count_actual,
		output_buf->buffer_size);

	buffer_size = output_buf->buffer_size;

	extradata_buf = get_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_OUTPUT);
	if (extradata_buf) {
		dprintk(VIDC_DBG,
			"extradata: num = %d, size = %d\n",
			extradata_buf->buffer_count_actual,
			extradata_buf->buffer_size);
		buffer_size += extradata_buf->buffer_size;
	} else {
		dprintk(VIDC_DBG,
			"This extradata buffer not required, buffer_type: %x\n",
			buffer_type);
	}

	if (inst->flags & VIDC_SECURE)
		smem_flags |= SMEM_SECURE;

	if (output_buf->buffer_size) {
		for (i = 0; i < output_buf->buffer_count_actual;
				i++) {
			handle = msm_comm_smem_alloc(inst,
					buffer_size, 1, smem_flags,
					buffer_type, 0);
			if (!handle) {
				dprintk(VIDC_ERR,
					"Failed to allocate output memory\n");
				rc = -ENOMEM;
				goto err_no_mem;
			}
			rc = msm_comm_smem_cache_operations(inst,
					handle, SMEM_CACHE_CLEAN);
			if (rc) {
				dprintk(VIDC_WARN,
					"Failed to clean cache may cause undefined behavior\n");
			}
			binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
			if (!binfo) {
				dprintk(VIDC_ERR, "Out of memory\n");
				rc = -ENOMEM;
				goto fail_kzalloc;
			}
			mutex_lock(&inst->lock);
			binfo->handle = handle;
			buffer_info.buffer_size = output_buf->buffer_size;
			buffer_info.buffer_type = buffer_type;
			binfo->buffer_type = buffer_type;
			buffer_info.num_buffers = 1;
			binfo->buffer_ownership = DRIVER;
			buffer_info.align_device_addr = handle->device_addr;
			buffer_info.extradata_addr = handle->device_addr +
				output_buf->buffer_size;
			if (extradata_buf) {
				buffer_info.extradata_size =
					extradata_buf->buffer_size;
			}
			dprintk(VIDC_DBG, "Output buffer address: 0x%pa\n",
					&buffer_info.align_device_addr);
			dprintk(VIDC_DBG, "Output extradata address: 0x%pa\n",
					&buffer_info.extradata_addr);
			rc = call_hfi_op(hdev, session_set_buffers,
					(void *) inst->session, &buffer_info);
			mutex_unlock(&inst->lock);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s : session_set_buffers failed\n",
					__func__);
				goto fail_set_buffers;
			}
			mutex_lock(&inst->lock);
			list_add_tail(&binfo->list, &inst->outputbufs);
			mutex_unlock(&inst->lock);
		}
	}
	return rc;
fail_set_buffers:
	kfree(binfo);
fail_kzalloc:
	msm_comm_smem_free(inst, handle);
err_no_mem:
	return rc;
}

static int set_scratch_buffers(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	int rc = 0;
	struct msm_smem *handle;
	struct internal_buf *binfo;
	struct vidc_buffer_addr_info buffer_info;
	u32 smem_flags = 0;
	struct hal_buffer_requirements *scratch_buf;
	int i;
	struct hfi_device *hdev;

	hdev = inst->core->device;

	scratch_buf = get_buff_req_buffer(inst, buffer_type);
	if (!scratch_buf) {
		dprintk(VIDC_DBG,
			"This scratch buffer not required, buffer_type: %x\n",
			buffer_type);
		return 0;
	}
	dprintk(VIDC_DBG,
		"scratch: num = %d, size = %d\n",
		scratch_buf->buffer_count_actual,
		scratch_buf->buffer_size);

	if (inst->flags & VIDC_SECURE)
		smem_flags |= SMEM_SECURE;

	if (scratch_buf->buffer_size) {
		for (i = 0; i < scratch_buf->buffer_count_actual;
				i++) {
			handle = msm_comm_smem_alloc(inst,
				scratch_buf->buffer_size, 1, smem_flags,
				buffer_type, 0);
			if (!handle) {
				dprintk(VIDC_ERR,
					"Failed to allocate scratch memory\n");
				rc = -ENOMEM;
				goto err_no_mem;
			}
			rc = msm_comm_smem_cache_operations(inst,
					handle, SMEM_CACHE_CLEAN);
			if (rc) {
				dprintk(VIDC_WARN,
				"Failed to clean cache may cause undefined behavior\n");
			}
			binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
			if (!binfo) {
				dprintk(VIDC_ERR, "Out of memory\n");
				rc = -ENOMEM;
				goto fail_kzalloc;
			}
			binfo->handle = handle;
			buffer_info.buffer_size = scratch_buf->buffer_size;
			buffer_info.buffer_type = buffer_type;
			binfo->buffer_type = buffer_type;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr = handle->device_addr;
			dprintk(VIDC_DBG, "Scratch buffer address: 0x%pa\n",
					&buffer_info.align_device_addr);
			rc = call_hfi_op(hdev, session_set_buffers,
				(void *) inst->session, &buffer_info);
			if (rc) {
				dprintk(VIDC_ERR,
					"vidc_hal_session_set_buffers failed\n");
				goto fail_set_buffers;
			}
			mutex_lock(&inst->lock);
			list_add_tail(&binfo->list, &inst->internalbufs);
			mutex_unlock(&inst->lock);
		}
	}
	return rc;
fail_set_buffers:
	kfree(binfo);
fail_kzalloc:
	msm_comm_smem_free(inst, handle);
err_no_mem:
	return rc;
}

static int set_persist_buffers(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	int rc = 0;
	struct msm_smem *handle;
	struct internal_buf *binfo;
	struct vidc_buffer_addr_info buffer_info;
	u32 smem_flags = 0;
	struct hal_buffer_requirements *persist_buf;
	int i;
	struct hfi_device *hdev;

	hdev = inst->core->device;

	persist_buf = get_buff_req_buffer(inst, buffer_type);
	if (!persist_buf) {
		dprintk(VIDC_DBG,
			"This persist buffer not required, buffer_type: %x\n",
			buffer_type);
		return 0;
	}

	dprintk(VIDC_DBG,
		"persist: num = %d, size = %d\n",
		persist_buf->buffer_count_actual,
		persist_buf->buffer_size);
	if (!list_empty(&inst->persistbufs)) {
		dprintk(VIDC_ERR,
			"Persist buffers already allocated\n");
		return rc;
	}

	if (inst->flags & VIDC_SECURE)
		smem_flags |= SMEM_SECURE;

	if (persist_buf->buffer_size) {
		for (i = 0; i < persist_buf->buffer_count_actual; i++) {
			handle = msm_comm_smem_alloc(inst,
				persist_buf->buffer_size, 1, smem_flags,
				buffer_type, 0);
			if (!handle) {
				dprintk(VIDC_ERR,
					"Failed to allocate persist memory\n");
				rc = -ENOMEM;
				goto err_no_mem;
			}
			rc = msm_comm_smem_cache_operations(inst,
					handle, SMEM_CACHE_CLEAN);
			if (rc) {
				dprintk(VIDC_WARN,
				"Failed to clean cache may cause undefined behavior\n");
			}
			binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
			if (!binfo) {
				dprintk(VIDC_ERR, "Out of memory\n");
				rc = -ENOMEM;
				goto fail_kzalloc;
			}
			binfo->handle = handle;
			buffer_info.buffer_size = persist_buf->buffer_size;
			buffer_info.buffer_type = buffer_type;
			binfo->buffer_type = buffer_type;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr = handle->device_addr;
			dprintk(VIDC_DBG, "Persist buffer address: 0x%pa\n",
					&buffer_info.align_device_addr);
			rc = call_hfi_op(hdev, session_set_buffers,
					(void *) inst->session, &buffer_info);
			if (rc) {
				dprintk(VIDC_ERR,
					"vidc_hal_session_set_buffers failed\n");
				goto fail_set_buffers;
			}
			mutex_lock(&inst->lock);
			list_add_tail(&binfo->list, &inst->persistbufs);
			mutex_unlock(&inst->lock);
		}
	}
	return rc;
fail_set_buffers:
	kfree(binfo);
fail_kzalloc:
	msm_comm_smem_free(inst, handle);
err_no_mem:
	return rc;
}

int msm_comm_try_state(struct msm_vidc_inst *inst, int state)
{
	int rc = 0;
	int flipped_state;
	struct msm_vidc_core *core;
	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %p\n", inst);
		return -EINVAL;
	}
	dprintk(VIDC_DBG,
			"Trying to move inst: %p from: 0x%x to 0x%x\n",
			inst, inst->state, state);
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %p\n", inst);
		return -EINVAL;
	}
	mutex_lock(&inst->sync_lock);
	if (inst->state == MSM_VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"Core is in bad state can't change the state\n");
		rc = -EINVAL;
		goto exit;
	}
	flipped_state = get_flipped_state(inst->state, state);
	dprintk(VIDC_DBG,
			"flipped_state = 0x%x\n", flipped_state);
	switch (flipped_state) {
	case MSM_VIDC_CORE_UNINIT_DONE:
	case MSM_VIDC_CORE_INIT:
		rc = msm_comm_init_core(inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_CORE_INIT_DONE:
		rc = msm_comm_init_core_done(inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_OPEN:
		rc = msm_comm_session_init(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_OPEN_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_OPEN_DONE,
			SESSION_INIT_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_LOAD_RESOURCES:
		rc = msm_vidc_load_resources(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_LOAD_RESOURCES_DONE:
	case MSM_VIDC_START:
		rc = msm_vidc_start(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_START_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_START_DONE,
				SESSION_START_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_STOP:
		rc = msm_vidc_stop(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_STOP_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_STOP_DONE,
				SESSION_STOP_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		dprintk(VIDC_DBG, "Moving to Stop Done state\n");
	case MSM_VIDC_RELEASE_RESOURCES:
		rc = msm_vidc_release_res(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_RELEASE_RESOURCES_DONE:
		rc = wait_for_state(inst, flipped_state,
			MSM_VIDC_RELEASE_RESOURCES_DONE,
			SESSION_RELEASE_RESOURCE_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		dprintk(VIDC_DBG,
				"Moving to release resources done state\n");
	case MSM_VIDC_CLOSE:
		rc = msm_comm_session_close(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_CLOSE_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_CLOSE_DONE,
				SESSION_END_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_CORE_UNINIT:
		dprintk(VIDC_DBG, "Sending core uninit\n");
		rc = msm_vidc_deinit_core(inst);
		if (rc || state == get_flipped_state(inst->state, state))
			break;
	default:
		dprintk(VIDC_ERR, "State not recognized\n");
		rc = -EINVAL;
		break;
	}
exit:
	mutex_unlock(&inst->sync_lock);
	if (rc)
		dprintk(VIDC_ERR,
				"Failed to move from state: %d to %d\n",
				inst->state, state);
	else
		trace_msm_vidc_common_state_change((void *)inst,
				inst->state, state);
	return rc;
}

int msm_comm_compute_idle_time(struct msm_vidc_inst *inst)
{
	int j = 0, rc = 0, last_sample_index = 0;
	struct msm_vidc_idle_stats *idle_stats;
	struct msm_vidc_core *core;
	const ktime_t zero_ktime = ktime_set(0, 0);
	ktime_t cumulative_idle_time = zero_ktime;
	u64 temp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	core = inst->core;

	mutex_lock(&core->lock);
	idle_stats = &core->idle_stats;
	last_sample_index = idle_stats->last_sample_index;

	idle_stats->samples[last_sample_index] = idle_stats->idle ?
		ktime_sub(ktime_get(), idle_stats->start_time) : zero_ktime;

	idle_stats->sample_count++;
	if (idle_stats->sample_count > IDLE_TIME_WINDOW_SIZE)
		idle_stats->sample_count = IDLE_TIME_WINDOW_SIZE;

	for (j = 0; j < idle_stats->sample_count; j++) {
		cumulative_idle_time = ktime_add(cumulative_idle_time,
			idle_stats->samples[j]);
	}

	temp = ktime_to_ns(cumulative_idle_time);
	do_div(temp, idle_stats->sample_count);
	idle_stats->avg_idle_time = ns_to_ktime(temp);

	idle_stats->idle = true;
	idle_stats->last_sample_index++;
	idle_stats->last_sample_index %= sizeof(idle_stats->samples);
	dprintk(VIDC_DBG, "%s: Idle stats: current %llu us, average %llu us\n",
		__func__, ktime_to_us(idle_stats->samples[last_sample_index]),
		ktime_to_us(idle_stats->avg_idle_time));
	mutex_unlock(&core->lock);

	idle_stats->fb_err_level = ktime_compare(idle_stats->avg_idle_time,
			zero_ktime) == 0 ? 3 : 0;
	if (idle_stats->fb_err_level != idle_stats->prev_fb_err_level) {
		idle_stats->prev_fb_err_level = idle_stats->fb_err_level;
		if (msm_comm_vote_bus(core)) {
			dprintk(VIDC_WARN,
			"Failed to scale DDR bus. Performance might be impacted\n");
		}
	}

	return rc;
}

int msm_comm_qbuf(struct vb2_buffer *vb)
{
	int rc = 0;
	struct vb2_queue *q;
	struct msm_vidc_inst *inst;
	struct vb2_buf_entry *entry;
	struct vidc_frame_data frame_data;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	int extra_idx = 0;

	if (!vb || !vb->vb2_queue)  {
		dprintk(VIDC_ERR, "%s: Invalid input: %p\n",
			__func__, vb);
		return -EINVAL;
	}

	q = vb->vb2_queue;
	inst = q->drv_priv;
	if (!inst) {
		dprintk(VIDC_ERR, "%s: Invalid input: %p\n",
			__func__, vb);
		return -EINVAL;
	}

	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
			"Invalid input: %p, %p, %p\n", inst, core, vb);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s: Invalid input: %p\n",
			__func__, hdev);
		return -EINVAL;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID ||
		core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR, "Core is in bad state. Can't Queue\n");
		return -EINVAL;
	}
	if (inst->state != MSM_VIDC_START_DONE) {
			entry = kzalloc(sizeof(*entry), GFP_KERNEL);
			if (!entry) {
				dprintk(VIDC_ERR, "Out of memory\n");
				goto err_no_mem;
			}
			entry->vb = vb;
			mutex_lock(&inst->sync_lock);
			list_add_tail(&entry->list, &inst->pendingq);
			mutex_unlock(&inst->sync_lock);
	} else {
		int64_t time_usec = timeval_to_ns(&vb->v4l2_buf.timestamp);
		do_div(time_usec, NSEC_PER_USEC);
		memset(&frame_data, 0 , sizeof(struct vidc_frame_data));
		frame_data.alloc_len = vb->v4l2_planes[0].length;
		frame_data.filled_len = vb->v4l2_planes[0].bytesused;
		frame_data.offset = vb->v4l2_planes[0].data_offset;
		frame_data.device_addr = vb->v4l2_planes[0].m.userptr;
		frame_data.timestamp = time_usec;
		frame_data.flags = 0;
		frame_data.clnt_data = frame_data.device_addr;
		if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
			(frame_data.filled_len > frame_data.alloc_len ||
			frame_data.offset > frame_data.alloc_len)) {
			dprintk(VIDC_ERR,
				"Buffer will overflow, not queueing it\n");
			rc = -EINVAL;
			goto err_bad_input;
		}

		if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
			frame_data.buffer_type = HAL_BUFFER_INPUT;
			if (vb->v4l2_buf.flags & V4L2_QCOM_BUF_FLAG_EOS) {
				frame_data.flags |= HAL_BUFFERFLAG_EOS;
				dprintk(VIDC_DBG,
					"Received EOS on output capability\n");
			}

			if (vb->v4l2_buf.flags &
				V4L2_MSM_BUF_FLAG_YUV_601_709_CLAMP) {
				frame_data.flags |=
					HAL_BUFFERFLAG_YUV_601_709_CSC_CLAMP;
				dprintk(VIDC_DBG,
					"Received buff with 601to709 clamp\n");
			}

			if (vb->v4l2_buf.flags &
					V4L2_QCOM_BUF_FLAG_CODECCONFIG) {
				frame_data.flags |= HAL_BUFFERFLAG_CODECCONFIG;
				dprintk(VIDC_DBG,
					"Received CODECCONFIG on output cap\n");
			}

			if (vb->v4l2_buf.flags &
					V4L2_QCOM_BUF_FLAG_DECODEONLY) {
				frame_data.flags |= HAL_BUFFERFLAG_DECODEONLY;
				dprintk(VIDC_DBG,
					"Received DECODEONLY on output cap\n");
			}

			if (vb->v4l2_buf.flags &
				V4L2_QCOM_BUF_TIMESTAMP_INVALID)
				frame_data.timestamp = LLONG_MAX;

			if (vb->v4l2_buf.flags &
					V4L2_QCOM_BUF_TS_DISCONTINUITY) {
				frame_data.flags |=
					HAL_BUFFERFLAG_TS_DISCONTINUITY;
				dprintk(VIDC_DBG,
					"Received TS_DISCONTINUE on output\n");
			}

			if (vb->v4l2_buf.flags & V4L2_QCOM_BUF_TS_ERROR) {
				frame_data.flags |=
					HAL_BUFFERFLAG_TS_ERROR;
				dprintk(VIDC_DBG,
					"Received TS_ERROR on output cap\n");
			}

			extra_idx =
				EXTRADATA_IDX(inst->fmts[OUTPUT_PORT]->
					num_planes);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES) &&
					vb->v4l2_planes[extra_idx].m.userptr) {
				frame_data.extradata_addr =
					vb->v4l2_planes[extra_idx].m.userptr;
				frame_data.flags |= HAL_BUFFERFLAG_EXTRADATA;
			}

			dprintk(VIDC_DBG,
				"Sending etb to hal: device_addr: 0x%pa, alloc: %d, filled: %d, offset: %d, ts: %lld, flags = 0x%x, v4l2_buf index = %d\n",
				&frame_data.device_addr, frame_data.alloc_len,
				frame_data.filled_len, frame_data.offset,
				frame_data.timestamp, frame_data.flags,
				vb->v4l2_buf.index);

			if (core->resources.dynamic_bw_update)
				msm_comm_compute_idle_time(inst);

			rc = call_hfi_op(hdev, session_etb, (void *)
					inst->session, &frame_data);
			if (!rc)
				msm_vidc_debugfs_update(inst,
					MSM_VIDC_DEBUGFS_EVENT_ETB);
			dprintk(VIDC_DBG, "Sent etb to HAL\n");
		} else if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			struct vidc_seq_hdr seq_hdr;
			frame_data.filled_len = 0;
			frame_data.offset = 0;
			frame_data.alloc_len = vb->v4l2_planes[0].length;
			frame_data.buffer_type =
				msm_comm_get_hal_output_buffer(inst);

			extra_idx =
			EXTRADATA_IDX(inst->fmts[CAPTURE_PORT]->num_planes);
			if (extra_idx && (extra_idx < VIDEO_MAX_PLANES) &&
				vb->v4l2_planes[extra_idx].m.userptr) {
				frame_data.extradata_addr =
					vb->v4l2_planes[extra_idx].m.userptr;
				frame_data.extradata_size =
					vb->v4l2_planes[extra_idx].length;
			}

			dprintk(VIDC_DBG,
				"Sending ftb to hal: device_addr: 0x%pa, alloc: %d, buffer_type: %d, ts: %lld, flags = 0x%x, v4l2_buf index = %d\n",
				&frame_data.device_addr, frame_data.alloc_len,
				frame_data.buffer_type, frame_data.timestamp,
				frame_data.flags, vb->v4l2_buf.index);

			if (core->resources.dynamic_bw_update)
				msm_comm_compute_idle_time(inst);

			if (atomic_read(&inst->get_seq_hdr_cnt) &&
			   inst->session_type == MSM_VIDC_ENCODER) {
				seq_hdr.seq_hdr = vb->v4l2_planes[0].
					m.userptr;
				seq_hdr.seq_hdr_len = vb->v4l2_planes[0].length;
				rc = call_hfi_op(hdev, session_get_seq_hdr,
					(void *) inst->session, &seq_hdr);
				if (!rc) {
					inst->vb2_seq_hdr = vb;
					dprintk(VIDC_DBG, "Seq_hdr: %p\n",
						inst->vb2_seq_hdr);
				}
				atomic_dec(&inst->get_seq_hdr_cnt);
			} else {
				rc = msm_comm_check_dcvs_supported(inst);
				if (!rc) {
					inst->dcvs_mode = true;
					dprintk(VIDC_DBG,
						"%s: session DCVS supported, dcvs_mode = %d\n",
						__func__, inst->dcvs_mode);
				} else {
					inst->dcvs_mode = false;
					dprintk(VIDC_DBG,
						"%s: session DCVS not supported, dcvs_mode = %d\n",
						__func__, inst->dcvs_mode);
				}
				if (msm_vidc_dcvs_mode && inst->dcvs_mode) {
					rc = msm_comm_scale_clocks_dcvs(
							inst, false);
					if (rc)
						dprintk(VIDC_ERR,
							"%s: Failed to scale clocks in DCVS: %d\n",
							__func__, rc);
				}
				rc = call_hfi_op(hdev, session_ftb,
					(void *) inst->session, &frame_data);
				if (!rc)
					msm_vidc_debugfs_update(inst,
						MSM_VIDC_DEBUGFS_EVENT_FTB);
			}
			inst->ftb_count++;
		} else {
			dprintk(VIDC_ERR,
				"This capability is not supported: %d\n",
				q->type);
			rc = -EINVAL;
		}
	}
err_bad_input:
	if (rc)
		dprintk(VIDC_ERR, "Failed to queue buffer\n");
err_no_mem:
	return rc;
}

int msm_comm_try_get_bufreqs(struct msm_vidc_inst *inst)
{
	struct buffer_requirements buf_req;
	int rc = 0;
	int i = 0;
	union hal_get_property hprop;

	rc = msm_comm_try_get_prop(inst,
					HAL_PARAM_GET_BUFFER_REQUIREMENTS,
					&hprop);
	if (rc) {
		dprintk(VIDC_ERR, "%s Error rc:%d\n", __func__, rc);
		return rc;
	}
	buf_req = hprop.buf_req;
	memcpy(&inst->buff_req, &buf_req,
			sizeof(struct buffer_requirements));
	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		dprintk(VIDC_DBG,
				"buffer type: %d, count : %d, size: %d\n",
				inst->buff_req.buffer[i].buffer_type,
				inst->buff_req.buffer[i].buffer_count_actual,
				inst->buff_req.buffer[i].buffer_size);
	}
	dprintk(VIDC_PROF, "Input buffers: %d, Output buffers: %d\n",
			inst->buff_req.buffer[0].buffer_count_actual,
			inst->buff_req.buffer[1].buffer_count_actual);
	return rc;
}

int msm_comm_try_get_prop(struct msm_vidc_inst *inst, enum hal_property ptype,
				union hal_get_property *hprop)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct getprop_buf *buf;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID ||
			inst->core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"Core is in bad state can't query get_bufreqs()\n");
		return -EAGAIN;
	}
	hdev = inst->core->device;
	mutex_lock(&inst->sync_lock);
	if (inst->state < MSM_VIDC_OPEN_DONE || inst->state >= MSM_VIDC_CLOSE) {
		dprintk(VIDC_ERR,
			"%s Not in proper state\n", __func__);
		rc = -EAGAIN;
		goto exit;
	}
	init_completion(
		&inst->completions[SESSION_MSG_INDEX(SESSION_PROPERTY_INFO)]);
	switch (ptype) {
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
		rc = call_hfi_op(hdev, session_get_property,
					(void *) inst->session, ptype);
		if (rc) {
			dprintk(VIDC_ERR, "%s Failed: PROFILE_LEVEL_INFO\n",
						__func__);
			rc = -EAGAIN;
			goto exit;
		}
		break;
	case HAL_PARAM_GET_BUFFER_REQUIREMENTS:
		rc = call_hfi_op(hdev, session_get_buf_req,
						(void *) inst->session);
		if (rc) {
			dprintk(VIDC_ERR, "Failed to get property\n");
			rc = -EAGAIN;
			goto exit;
		}
		break;
	default:
		rc = -EAGAIN;
		dprintk(VIDC_ERR, "%s id:%d not supported\n", __func__, ptype);
		break;
	}
	rc = wait_for_completion_timeout(
		&inst->completions[SESSION_MSG_INDEX(SESSION_PROPERTY_INFO)],
		msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR,
			"%s: Wait interrupted or timed out [%p]: %d\n",
			__func__, inst,
			SESSION_MSG_INDEX(SESSION_PROPERTY_INFO));
		inst->state = MSM_VIDC_CORE_INVALID;
		msm_comm_kill_session(inst);
		rc = -EIO;
		goto exit;
	}

	if (!list_empty(&inst->pending_getpropq)) {
		buf = list_first_entry(&inst->pending_getpropq,
					struct getprop_buf, list);
		*hprop = *((union hal_get_property *) buf->data);
		kfree(buf->data);
		list_del(&buf->list);
		kfree(buf);
		rc = 0;
	} else {
		dprintk(VIDC_ERR, "%s getprop list empty\n", __func__);
		rc = -EINVAL;
	}
exit:
	mutex_unlock(&inst->sync_lock);
	return rc;
}

int msm_comm_release_output_buffers(struct msm_vidc_inst *inst)
{
	struct msm_smem *handle;
	struct list_head *ptr, *next;
	struct internal_buf *buf;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %p\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %p\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %p\n", hdev);
		return -EINVAL;
	}
	mutex_lock(&inst->lock);
	if (!list_empty(&inst->outputbufs)) {
		list_for_each_safe(ptr, next, &inst->outputbufs) {
			buf = list_entry(ptr, struct internal_buf,
					list);
			handle = buf->handle;
			buffer_info.buffer_size = handle->size;
			buffer_info.buffer_type = buf->buffer_type;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr = handle->device_addr;
			if (inst->state != MSM_VIDC_CORE_INVALID &&
					core->state != VIDC_CORE_INVALID) {
				buffer_info.response_required = false;
				rc = call_hfi_op(hdev, session_release_buffers,
					(void *)inst->session, &buffer_info);
				if (rc)
					dprintk(VIDC_WARN,
						"Rel output buf fail:0x%pa, %d\n",
						&buffer_info.align_device_addr,
						buffer_info.buffer_size);
			}
			list_del(&buf->list);
			mutex_unlock(&inst->lock);
			msm_comm_smem_free(inst, buf->handle);
			kfree(buf);
			mutex_lock(&inst->lock);
		}
	}
	mutex_unlock(&inst->lock);
	return rc;
}

int msm_comm_release_scratch_buffers(struct msm_vidc_inst *inst)
{
	struct msm_smem *handle;
	struct list_head *ptr, *next;
	struct internal_buf *buf;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %p\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %p\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %p\n", hdev);
		return -EINVAL;
	}
	mutex_lock(&inst->lock);
	if (!list_empty(&inst->internalbufs)) {
		list_for_each_safe(ptr, next, &inst->internalbufs) {
			buf = list_entry(ptr, struct internal_buf,
					list);
			handle = buf->handle;
			buffer_info.buffer_size = handle->size;
			buffer_info.buffer_type = buf->buffer_type;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr = handle->device_addr;
			if (inst->state != MSM_VIDC_CORE_INVALID &&
					core->state != VIDC_CORE_INVALID) {
				buffer_info.response_required = true;
				init_completion(
				   &inst->completions[SESSION_MSG_INDEX
				   (SESSION_RELEASE_BUFFER_DONE)]);
				rc = call_hfi_op(hdev, session_release_buffers,
					(void *)inst->session, &buffer_info);
				if (rc)
					dprintk(VIDC_WARN,
						"Rel scrtch buf fail:0x%pa, %d\n",
						&buffer_info.align_device_addr,
						buffer_info.buffer_size);
				mutex_unlock(&inst->lock);
				rc = wait_for_sess_signal_receipt(inst,
					SESSION_RELEASE_BUFFER_DONE);
				if (rc) {
					change_inst_state(inst,
						MSM_VIDC_CORE_INVALID);
					msm_comm_kill_session(inst);
				}
				mutex_lock(&inst->lock);
			}
			list_del(&buf->list);
			mutex_unlock(&inst->lock);
			msm_comm_smem_free(inst, buf->handle);
			kfree(buf);
			mutex_lock(&inst->lock);
		}
	}
	mutex_unlock(&inst->lock);
	return rc;
}

int msm_comm_release_persist_buffers(struct msm_vidc_inst *inst)
{
	struct msm_smem *handle;
	struct list_head *ptr, *next;
	struct internal_buf *buf;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %p\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %p\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %p\n", hdev);
		return -EINVAL;
	}
	mutex_lock(&inst->lock);
	if (!list_empty(&inst->persistbufs)) {
		list_for_each_safe(ptr, next, &inst->persistbufs) {
			buf = list_entry(ptr, struct internal_buf,
					list);
			handle = buf->handle;
			buffer_info.buffer_size = handle->size;
			buffer_info.buffer_type = buf->buffer_type;
			buffer_info.num_buffers = 1;
			buffer_info.align_device_addr = handle->device_addr;
			if (inst->state != MSM_VIDC_CORE_INVALID &&
					core->state != VIDC_CORE_INVALID) {
				buffer_info.response_required = true;
				init_completion(
				   &inst->completions[SESSION_MSG_INDEX
				   (SESSION_RELEASE_BUFFER_DONE)]);
				rc = call_hfi_op(hdev, session_release_buffers,
					(void *)inst->session, &buffer_info);
				if (rc)
					dprintk(VIDC_WARN,
						"Rel prst buf fail:0x%pa, %d\n",
						&buffer_info.align_device_addr,
						buffer_info.buffer_size);
				mutex_unlock(&inst->lock);
				rc = wait_for_sess_signal_receipt(inst,
					SESSION_RELEASE_BUFFER_DONE);
				if (rc) {
					change_inst_state(inst,
						MSM_VIDC_CORE_INVALID);
					msm_comm_kill_session(inst);
				}
				mutex_lock(&inst->lock);
			}
			list_del(&buf->list);
			mutex_unlock(&inst->lock);
			msm_comm_smem_free(inst, buf->handle);
			kfree(buf);
			mutex_lock(&inst->lock);
		}
	}
	mutex_unlock(&inst->lock);
	return rc;
}

int msm_comm_try_set_prop(struct msm_vidc_inst *inst,
	enum hal_property ptype, void *pdata)
{
	int rc = 0;
	struct hfi_device *hdev;
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid input: %p\n", inst);
		return -EINVAL;
	}

	if (!inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	mutex_lock(&inst->sync_lock);
	if (inst->state < MSM_VIDC_OPEN_DONE || inst->state >= MSM_VIDC_CLOSE) {
		dprintk(VIDC_ERR, "Not in proper state to set property\n");
		rc = -EAGAIN;
		goto exit;
	}
	rc = call_hfi_op(hdev, session_set_property, (void *)inst->session,
			ptype, pdata);
	if (rc)
		dprintk(VIDC_ERR, "Failed to set hal property for framesize\n");
exit:
	mutex_unlock(&inst->sync_lock);
	return rc;
}

int msm_comm_set_output_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (msm_comm_release_output_buffers(inst))
		dprintk(VIDC_WARN, "Failed to release output buffers\n");

	rc = set_output_buffers(inst, HAL_BUFFER_OUTPUT);
	if (rc)
		goto error;
	return rc;
error:
	msm_comm_release_output_buffers(inst);
	return rc;
}

int msm_comm_set_scratch_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (msm_comm_release_scratch_buffers(inst))
		dprintk(VIDC_WARN, "Failed to release scratch buffers\n");

	rc = set_scratch_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH);
	if (rc)
		goto error;

	rc = set_scratch_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH_1);
	if (rc)
		goto error;

	rc = set_scratch_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH_2);
	if (rc)
		goto error;

	return rc;
error:
	msm_comm_release_scratch_buffers(inst);
	return rc;
}

int msm_comm_set_persist_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	rc = set_persist_buffers(inst, HAL_BUFFER_INTERNAL_PERSIST);
	if (rc)
		goto error;

	rc = set_persist_buffers(inst, HAL_BUFFER_INTERNAL_PERSIST_1);
	if (rc)
		goto error;
	return rc;
error:
	msm_comm_release_persist_buffers(inst);
	return rc;
}

static void msm_comm_flush_in_invalid_state(struct msm_vidc_inst *inst)
{
	struct list_head *ptr, *next;
	struct vb2_buffer *vb;
	if (!list_empty(&inst->bufq[CAPTURE_PORT].
				vb2_bufq.queued_list)) {
		list_for_each_safe(ptr, next,
				&inst->bufq[CAPTURE_PORT].
				vb2_bufq.queued_list) {
			vb = container_of(ptr,
					struct vb2_buffer,
					queued_entry);
			if (vb) {
				vb->v4l2_planes[0].bytesused = 0;
				vb->v4l2_planes[0].data_offset = 0;
				mutex_lock(&inst->bufq[CAPTURE_PORT].lock);
				vb2_buffer_done(vb,
						VB2_BUF_STATE_DONE);
				mutex_unlock(&inst->bufq[CAPTURE_PORT].lock);
			}
		}
	}
	if (!list_empty(&inst->bufq[OUTPUT_PORT].
				vb2_bufq.queued_list)) {
		list_for_each_safe(ptr, next,
				&inst->bufq[OUTPUT_PORT].
				vb2_bufq.queued_list) {
			vb = container_of(ptr,
					struct vb2_buffer,
					queued_entry);
			if (vb) {
				vb->v4l2_planes[0].bytesused = 0;
				vb->v4l2_planes[0].data_offset = 0;
				mutex_lock(&inst->bufq[OUTPUT_PORT].lock);
				vb2_buffer_done(vb,
						VB2_BUF_STATE_DONE);
				mutex_unlock(&inst->bufq[OUTPUT_PORT].lock);
			}
		}
	}

	msm_vidc_queue_v4l2_event(inst, V4L2_EVENT_MSM_VIDC_FLUSH_DONE);
	return;
}

void msm_comm_flush_dynamic_buffers(struct msm_vidc_inst *inst)
{
	struct buffer_info *binfo = NULL, *dummy = NULL;
	struct list_head *list = NULL;

	if (inst->buffer_mode_set[CAPTURE_PORT] != HAL_BUFFER_MODE_DYNAMIC)
		return;

	/*
	* dynamic buffer mode:- if flush is called during seek
	* driver should not queue any new buffer it has been holding.
	*
	* Each dynamic o/p buffer can have one of following ref_count:
	* ref_count : 0 - f/w has released reference and sent fbd back.
	*		  The buffer has been returned back to client.
	*
	* ref_count : 1 - f/w is holding reference. f/w may have released
	*                 fbd as read_only OR fbd is pending. f/w will
	*		  release reference before sending flush_done.
	*
	* ref_count : 2 - f/w is holding reference, f/w has released fbd as
	*                 read_only, which client has queued back to driver.
	*                 driver holds this buffer and will queue back
	*                 only when f/w releases the reference. During
	*		  flush_done, f/w will release the reference but driver
	*		  should not queue back the buffer to f/w.
	*		  Flush all buffers with ref_count 2.
	*/
	mutex_lock(&inst->lock);
	if (!list_empty(&inst->registered_bufs)) {
		struct v4l2_event buf_event = {0};
		u32 *ptr = NULL;
		list = &inst->registered_bufs;
		list_for_each_entry_safe(binfo, dummy, list, list) {
			if (binfo &&
			(binfo->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) &&
			(atomic_read(&binfo->ref_count) == 2)) {
				atomic_dec(&binfo->ref_count);
				buf_event.type =
				V4L2_EVENT_MSM_VIDC_RELEASE_UNQUEUED_BUFFER;
				ptr = (u32 *)buf_event.u.data;
				ptr[0] = binfo->fd[0];
				ptr[1] = binfo->buff_off[0];
				ptr[2] = binfo->uvaddr[0];
				ptr[3] = (u32) binfo->timestamp.tv_sec;
				ptr[4] = (u32) binfo->timestamp.tv_usec;
				ptr[5] = binfo->v4l2_index;
				dprintk(VIDC_DBG,
					"released buffer held in driver before issuing flush: 0x%pa fd[0]: %d\n",
					&binfo->device_addr[0], binfo->fd[0]);
				/*send event to client*/
				v4l2_event_queue_fh(&inst->event_handler,
					&buf_event);
				wake_up(&inst->kernel_event_queue);
			}
		}
	}
	mutex_unlock(&inst->lock);
}

int msm_comm_flush(struct msm_vidc_inst *inst, u32 flags)
{
	int rc =  0;
	bool ip_flush = false;
	bool op_flush = false;
	struct list_head *ptr, *next;
	struct vb2_buf_entry *temp;
	struct mutex *lock;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %p\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %p\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %p\n", hdev);
		return -EINVAL;
	}

	ip_flush = flags & V4L2_QCOM_CMD_FLUSH_OUTPUT;
	op_flush = flags & V4L2_QCOM_CMD_FLUSH_CAPTURE;

	if (ip_flush && !op_flush) {
		dprintk(VIDC_INFO, "Input only flush not supported\n");
		return 0;
	}
	mutex_lock(&inst->sync_lock);
	msm_comm_flush_dynamic_buffers(inst);
	mutex_unlock(&inst->sync_lock);
	if (inst->state == MSM_VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"Core %p and inst %p are in bad state\n",
					core, inst);
		msm_comm_flush_in_invalid_state(inst);
		return 0;
	}

	mutex_lock(&inst->sync_lock);
	if (inst->in_reconfig && !ip_flush && op_flush) {
		if (!list_empty(&inst->pendingq)) {
			/*Execution can never reach here since port reconfig
			 * wont happen unless pendingq is emptied out
			 * (both pendingq and flush being secured with same
			 * lock). Printing a message here incase this breaks.*/
			dprintk(VIDC_WARN,
			"FLUSH BUG: Pending q not empty! It should be empty\n");
		}
		rc = call_hfi_op(hdev, session_flush, inst->session,
				HAL_FLUSH_OUTPUT);
		if (!rc && (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY))
			rc = call_hfi_op(hdev, session_flush, inst->session,
				HAL_FLUSH_OUTPUT2);

	} else {
		if (!list_empty(&inst->pendingq)) {
			/*If flush is called after queueing buffers but before
			 * streamon driver should flush the pending queue*/
			list_for_each_safe(ptr, next, &inst->pendingq) {
				temp =
				list_entry(ptr, struct vb2_buf_entry, list);
				if (temp->vb->v4l2_buf.type ==
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
					lock = &inst->bufq[CAPTURE_PORT].lock;
				else
					lock = &inst->bufq[OUTPUT_PORT].lock;
				temp->vb->v4l2_planes[0].bytesused = 0;
				mutex_lock(lock);
				vb2_buffer_done(temp->vb, VB2_BUF_STATE_DONE);
				mutex_unlock(lock);
				list_del(&temp->list);
				kfree(temp);
			}
		}

		/*Do not send flush in case of session_error */
		if (!(inst->state == MSM_VIDC_CORE_INVALID &&
			  core->state != VIDC_CORE_INVALID))
			rc = call_hfi_op(hdev, session_flush, inst->session,
				HAL_FLUSH_ALL);
	}
	mutex_unlock(&inst->sync_lock);
	return rc;
}


enum hal_extradata_id msm_comm_get_hal_extradata_index(
	enum v4l2_mpeg_vidc_extradata index)
{
	int ret = 0;
	switch (index) {
	case V4L2_MPEG_VIDC_EXTRADATA_NONE:
		ret = HAL_EXTRADATA_NONE;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_MB_QUANTIZATION:
		ret = HAL_EXTRADATA_MB_QUANTIZATION;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_INTERLACE_VIDEO:
		ret = HAL_EXTRADATA_INTERLACE_VIDEO;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_VC1_FRAMEDISP:
		ret = HAL_EXTRADATA_VC1_FRAMEDISP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_VC1_SEQDISP:
		ret = HAL_EXTRADATA_VC1_SEQDISP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_TIMESTAMP:
		ret = HAL_EXTRADATA_TIMESTAMP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_S3D_FRAME_PACKING:
		ret = HAL_EXTRADATA_S3D_FRAME_PACKING;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_FRAME_RATE:
		ret = HAL_EXTRADATA_FRAME_RATE;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_PANSCAN_WINDOW:
		ret = HAL_EXTRADATA_PANSCAN_WINDOW;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_RECOVERY_POINT_SEI:
		ret = HAL_EXTRADATA_RECOVERY_POINT_SEI;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_MULTISLICE_INFO:
		ret = HAL_EXTRADATA_MULTISLICE_INFO;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_NUM_CONCEALED_MB:
		ret = HAL_EXTRADATA_NUM_CONCEALED_MB;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_METADATA_FILLER:
		ret = HAL_EXTRADATA_METADATA_FILLER;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_ASPECT_RATIO:
		ret = HAL_EXTRADATA_ASPECT_RATIO;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_INPUT_CROP:
		ret = HAL_EXTRADATA_INPUT_CROP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_DIGITAL_ZOOM:
		ret = HAL_EXTRADATA_DIGITAL_ZOOM;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_MPEG2_SEQDISP:
		ret = HAL_EXTRADATA_MPEG2_SEQDISP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_STREAM_USERDATA:
		ret = HAL_EXTRADATA_STREAM_USERDATA;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_FRAME_QP:
		ret = HAL_EXTRADATA_FRAME_QP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_FRAME_BITS_INFO:
		ret = HAL_EXTRADATA_FRAME_BITS_INFO;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_LTR:
		ret = HAL_EXTRADATA_LTR_INFO;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_METADATA_MBI:
		ret = HAL_EXTRADATA_METADATA_MBI;
		break;
	default:
		dprintk(VIDC_WARN, "Extradata not found: %d\n", index);
		break;
	}
	return ret;
};

enum hal_buffer_layout_type msm_comm_get_hal_buffer_layout(
	enum v4l2_mpeg_vidc_video_mvc_layout index)
{
	int ret = 0;
	switch (index) {
	case V4L2_MPEG_VIDC_VIDEO_MVC_SEQUENTIAL:
		ret = HAL_BUFFER_LAYOUT_SEQ;
		break;
	case V4L2_MPEG_VIDC_VIDEO_MVC_TOP_BOTTOM:
		ret = HAL_BUFFER_LAYOUT_TOP_BOTTOM;
		break;
	default:
		break;
	}
	return ret;
}

int msm_comm_get_domain_partition(struct msm_vidc_inst *inst, u32 flags,
	enum v4l2_buf_type buf_type, int *domain, int *partition)
{
	struct hfi_device *hdev;
	u32 hal_buffer_type = 0;
	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;

	hdev = inst->core->device;

	/*
	 * TODO: Due to the way in which the underlying smem mechanism
	 * maps buffer types to corresponding IOMMU domains, we need to
	 * pass in HAL_BUFFER_OUTPUT for input buffers (and vice versa)
	 * so that buffers are mapped into the correct domains. In the
	 * future, we should try to remove this workaround.
	 */
	switch (buf_type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		hal_buffer_type = (inst->session_type == MSM_VIDC_ENCODER) ?
			HAL_BUFFER_INPUT : HAL_BUFFER_OUTPUT;
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		hal_buffer_type = (inst->session_type == MSM_VIDC_ENCODER) ?
			HAL_BUFFER_OUTPUT : HAL_BUFFER_INPUT;
		break;
	default:
		dprintk(VIDC_ERR, "v4l2 buf type not found %d\n", buf_type);
		return -ENOTSUPP;
	}
	return call_hfi_op(hdev, iommu_get_domain_partition,
		hdev->hfi_device_data, flags, hal_buffer_type, domain,
		partition);
};

int msm_vidc_trigger_ssr(struct msm_vidc_core *core,
	enum hal_ssr_trigger_type type)
{
	int rc = 0;
	struct hfi_device *hdev;
	if (!core || !core->device) {
		dprintk(VIDC_WARN, "Invalid parameters: %p\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (core->state == VIDC_CORE_INIT_DONE)
		rc = call_hfi_op(hdev, core_trigger_ssr,
				hdev->hfi_device_data, type);
	return rc;
}

static int msm_vidc_load_supported(struct msm_vidc_inst *inst)
{
	int num_mbs_per_sec = 0;
	enum load_calc_quirks quirks = LOAD_CALC_IGNORE_TURBO_LOAD |
		LOAD_CALC_IGNORE_THUMBNAIL_LOAD;

	if (inst->state == MSM_VIDC_OPEN_DONE) {
		num_mbs_per_sec = msm_comm_get_load(inst->core,
					MSM_VIDC_DECODER, quirks);
		num_mbs_per_sec += msm_comm_get_load(inst->core,
					MSM_VIDC_ENCODER, quirks);
		if (num_mbs_per_sec > inst->core->resources.max_load) {
			dprintk(VIDC_ERR,
				"H/W is overloaded. needed: %d max: %d\n",
				num_mbs_per_sec,
				inst->core->resources.max_load);
			msm_vidc_print_running_insts(inst->core);
			return -EINVAL;
		}
	}
	return 0;
}

int msm_vidc_check_scaling_supported(struct msm_vidc_inst *inst)
{
	u32 x_min, x_max, y_min, y_max;
	u32 input_height, input_width, output_height, output_width;

	input_height = inst->prop.height[OUTPUT_PORT];
	input_width = inst->prop.width[OUTPUT_PORT];
	output_height = inst->prop.height[CAPTURE_PORT];
	output_width = inst->prop.width[CAPTURE_PORT];

	if (!input_height || !input_width || !output_height || !output_width) {
		dprintk(VIDC_ERR,
			"Invalid : Input height = %d width = %d"
			" output height = %d width = %d\n",
			input_height, input_width, output_height,
			output_width);
		return -ENOTSUPP;
	}

	if (!inst->capability.scale_x.min ||
		!inst->capability.scale_x.max ||
		!inst->capability.scale_y.min ||
		!inst->capability.scale_y.max) {

		if ((input_width * input_height) !=
			(output_width * output_height)) {
			dprintk(VIDC_ERR,
				"%s: scaling is not supported (%dx%d != %dx%d)\n",
				__func__, input_width, input_height,
				output_width, output_height);
			return -ENOTSUPP;
		} else {
			dprintk(VIDC_DBG, "%s: supported WxH = %dx%d\n",
				__func__, input_width, input_height);
			return 0;
		}
	}

	x_min = (1<<16)/inst->capability.scale_x.min;
	y_min = (1<<16)/inst->capability.scale_y.min;
	x_max = inst->capability.scale_x.max >> 16;
	y_max = inst->capability.scale_y.max >> 16;

	if (input_height > output_height) {
		if (input_height/output_height > x_min) {
			dprintk(VIDC_ERR,
				"Unsupported height downscale ratio %d vs %d\n",
				input_height/output_height, x_min);
			return -ENOTSUPP;
		}
	} else {
		if (input_height/output_height > x_max) {
			dprintk(VIDC_ERR,
				"Unsupported height upscale ratio %d vs %d\n",
				input_height/output_height, x_max);
			return -ENOTSUPP;
		}
	}
	if (input_width > output_width) {
		if (input_width/output_width > y_min) {
			dprintk(VIDC_ERR,
				"Unsupported width downscale ratio %d vs %d\n",
				input_width/output_width, y_min);
			return -ENOTSUPP;
		}
	} else {
		if (input_width/output_width > y_max) {
			dprintk(VIDC_ERR,
				"Unsupported width upscale ratio %d vs %d\n",
				input_width/output_width, y_max);
			return -ENOTSUPP;
		}
	}
	return 0;
}

static int msm_comm_check_dcvs_supported(struct msm_vidc_inst *inst)
{
	int rc = 0;
	int num_mbs_per_frame = 0;
	int instance_count = 0;
	bool codec_supported = false;
	struct msm_vidc_inst *temp = NULL;
	struct msm_vidc_core *core;
	enum hal_video_codec codec;
	struct hal_buffer_requirements *output_buf_req;
	struct dcvs_stats *dcvs;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_WARN, "%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	dcvs = &inst->dcvs;
	instance_count = msm_comm_count_active_instances(core);

	if (instance_count == 1 && inst->session_type == MSM_VIDC_DECODER) {

		mutex_lock(&inst->lock);
		num_mbs_per_frame = msm_comm_get_mbs_per_frame(inst);
		output_buf_req = get_buff_req_buffer(inst,
			msm_comm_get_hal_output_buffer(inst));
		mutex_unlock(&inst->lock);

		codec = get_hal_codec_type(inst->fmts[OUTPUT_PORT]->fourcc);
		codec_supported = (codec == HAL_VIDEO_CODEC_H264);
		if (!(codec_supported &&
			IS_VALID_DCVS_SESSION(num_mbs_per_frame,
				DCVS_MIN_SUPPORTED_MBPERFRAME)))
				return -ENOTSUPP;

		if (!output_buf_req) {
			dprintk(VIDC_ERR,
				"%s: No buffer requirement for buffer type %x\n",
				__func__, HAL_BUFFER_OUTPUT);
			return -EINVAL;
		}
		if (inst->count.ftb - dcvs->prev_ftb_count >
			output_buf_req->buffer_count_actual)
				dcvs->prev_ftb_count = inst->count.ftb;
	} else {
		rc = -ENOTSUPP;
		/*
		*.For multiple instance use case with 4K, clocks will be scaled
		* as per load in streamon, but the clocks may be scaled
		* down as DCVS is running for first playback instance
		* Rescaling the core clock for multiple instance use case
		*/
		if (!dcvs->is_clock_scaled) {
			if (!msm_comm_scale_clocks(core)) {
				dcvs->is_clock_scaled = true;
				dprintk(VIDC_DBG,
					"%s: Scaled clocks = %d\n",
					__func__, dcvs->is_clock_scaled);
			} else {
				dprintk(VIDC_DBG,
					"%s: Failed to Scale clocks. Perf might be impacted\n",
					__func__);
			}
		}
		/*
		* For multiple instance use case turn OFF DCVS algorithm
		* immediately
		*/
		if (instance_count > 1) {
			mutex_lock(&core->lock);
			list_for_each_entry(temp, &core->instances, list)
				temp->dcvs_mode = false;
			mutex_unlock(&core->lock);
		}
	}
	return rc;
}
int msm_vidc_check_session_supported(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core_capability *capability;
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_WARN, "%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}
	capability = &inst->capability;
	hdev = inst->core->device;
	rc = msm_vidc_load_supported(inst);
	if (rc) {
		change_inst_state(inst, MSM_VIDC_CORE_INVALID);
		msm_vidc_queue_v4l2_event(inst,
					V4L2_EVENT_MSM_VIDC_HW_OVERLOAD);
		dprintk(VIDC_WARN,
			"%s: Hardware is overloaded\n", __func__);
		return rc;
	}
	if (!rc && inst->capability.capability_set) {
		if (inst->prop.width[CAPTURE_PORT] < capability->width.min ||
			inst->prop.height[CAPTURE_PORT] <
			capability->height.min) {
			dprintk(VIDC_ERR,
				"Unsupported WxH = (%u)x(%u), min supported is - (%u)x(%u)\n",
				inst->prop.width[CAPTURE_PORT],
				inst->prop.height[CAPTURE_PORT],
				capability->width.min,
				capability->height.min);
			rc = -ENOTSUPP;
		}
		if (!rc && (inst->prop.width[CAPTURE_PORT] >
			capability->width.max)) {
			dprintk(VIDC_ERR,
				"Unsupported width = %u supported max width = %u",
				inst->prop.width[CAPTURE_PORT],
				capability->width.max);
				rc = -ENOTSUPP;
		}

		if (!rc && (inst->prop.height[CAPTURE_PORT]
			* inst->prop.width[CAPTURE_PORT] >
			capability->width.max * capability->height.max)) {
			dprintk(VIDC_ERR,
			"Unsupported WxH = (%u)x(%u), max supported is - (%u)x(%u)\n",
			inst->prop.width[CAPTURE_PORT],
			inst->prop.height[CAPTURE_PORT],
			capability->width.max, capability->height.max);
			rc = -ENOTSUPP;
		}
	}
	if (rc) {
		change_inst_state(inst, MSM_VIDC_CORE_INVALID);
		msm_vidc_queue_v4l2_event(inst,
					V4L2_EVENT_MSM_VIDC_HW_UNSUPPORTED);
		dprintk(VIDC_ERR,
			"%s: Resolution unsupported\n", __func__);
	}
	return rc;
}

static void msm_comm_generate_session_error(struct msm_vidc_inst *inst)
{
	enum command_response cmd = SESSION_ERROR;
	struct msm_vidc_cb_cmd_done response = {0};

	dprintk(VIDC_WARN, "msm_comm_generate_session_error\n");
	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}

	response.session_id = inst;
	response.status = VIDC_ERR_FAIL;
	handle_session_error(cmd, (void *)&response);
}

static void msm_comm_generate_sys_error(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	enum command_response cmd = SYS_ERROR;
	struct msm_vidc_cb_cmd_done response  = {0};
	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	core = inst->core;
	response.device_id = (u32) core->id;
	handle_sys_error(cmd, (void *) &response);

}

int msm_comm_kill_session(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return -EINVAL;
	} else if (!inst->session) {
		/* There's no hfi session to kill */
		return 0;
	}

	/*
	 * We're internally forcibly killing the session, if fw is aware of
	 * the session send session_abort to firmware to clean up and release
	 * the session, else just kill the session inside the driver.
	 */
	if (inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_CLOSE_DONE) {
		struct hfi_device *hdev = inst->core->device;
		int abort_completion = SESSION_MSG_INDEX(SESSION_ABORT_DONE);

		rc = call_hfi_op(hdev, session_abort, (void *) inst->session);
		if (rc) {
			dprintk(VIDC_ERR, "session_abort failed rc: %d\n", rc);
			return rc;
		}

		init_completion(&inst->completions[abort_completion]);
		rc = wait_for_completion_timeout(
				&inst->completions[abort_completion],
				msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
		if (!rc) {
			dprintk(VIDC_ERR,
					"%s: Wait interrupted or timed out [%p]: %d\n",
					__func__, inst, abort_completion);
			msm_comm_generate_sys_error(inst);
		} else {
			change_inst_state(inst, MSM_VIDC_CLOSE_DONE);
		}
	} else {
		dprintk(VIDC_WARN,
				"Inactive session %p, triggering an internal session error\n",
				inst);
		msm_comm_generate_session_error(inst);

	}

	return rc;
}

static inline int power_on_for_smem(struct msm_vidc_inst *inst)
{
	struct hfi_device *hdev = NULL;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s: invalid inst handle\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	rc = call_hfi_op(hdev, power_enable, hdev->hfi_device_data);
	if (rc)
		dprintk(VIDC_ERR, "%s: failed to power on fw\n", __func__);
	return rc;
}

struct msm_smem *msm_comm_smem_alloc(struct msm_vidc_inst *inst,
			size_t size, u32 align, u32 flags,
			enum hal_buffer buffer_type, int map_kernel)
{
	struct msm_smem *m = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid inst: %p\n", __func__, inst);
		return NULL;
	}
	mutex_lock(&inst->core->lock);
	if (power_on_for_smem(inst))
		goto err_power_on;

	m = msm_smem_alloc(inst->mem_client, size, align,
				flags, buffer_type, map_kernel);
err_power_on:
	mutex_unlock(&inst->core->lock);
	return m;
}

void msm_comm_smem_free(struct msm_vidc_inst *inst, struct msm_smem *mem)
{
	if (!inst || !inst->core || !mem) {
		dprintk(VIDC_ERR,
			"%s: invalid params: %p %p\n", __func__, inst, mem);
		return;
	}
	mutex_lock(&inst->core->lock);
	if (power_on_for_smem(inst))
		goto err_power_on;

	msm_smem_free(inst->mem_client, mem);
err_power_on:
	mutex_unlock(&inst->core->lock);
}

int msm_comm_smem_cache_operations(struct msm_vidc_inst *inst,
		struct msm_smem *mem, enum smem_cache_ops cache_ops)
{
	if (!inst || !mem) {
		dprintk(VIDC_ERR,
			"%s: invalid params: %p %p\n", __func__, inst, mem);
		return -EINVAL;
	}
	return msm_smem_cache_operations(inst->mem_client, mem, cache_ops);
}

struct msm_smem *msm_comm_smem_user_to_kernel(struct msm_vidc_inst *inst,
			int fd, u32 offset, enum hal_buffer buffer_type)
{
	struct msm_smem *m = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid inst: %p\n", __func__, inst);
		return NULL;
	}
	mutex_lock(&inst->core->lock);
	if (power_on_for_smem(inst))
		goto err_power_on;

	m = msm_smem_user_to_kernel(inst->mem_client,
			fd, offset, buffer_type);
err_power_on:
	mutex_unlock(&inst->core->lock);
	return m;
}

int msm_comm_smem_get_domain_partition(struct msm_vidc_inst *inst,
			u32 flags, enum hal_buffer buffer_type,
			int *domain_num, int *partition_num)
{
	if (!inst || !domain_num || !partition_num) {
		dprintk(VIDC_ERR, "%s: invalid params: %p %p %p\n",
			__func__, inst, domain_num, partition_num);
		return -EINVAL;
	}
	return msm_smem_get_domain_partition(inst->mem_client, flags,
			buffer_type, domain_num, partition_num);
}
