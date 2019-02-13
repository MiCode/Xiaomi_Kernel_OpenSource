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

#define V4L2_EVENT_SEQ_CHANGED_SUFFICIENT \
		V4L2_EVENT_MSM_CVP_PORT_SETTINGS_CHANGED_SUFFICIENT
#define V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT \
		V4L2_EVENT_MSM_CVP_PORT_SETTINGS_CHANGED_INSUFFICIENT
#define V4L2_EVENT_RELEASE_BUFFER_REFERENCE \
		V4L2_EVENT_MSM_CVP_RELEASE_BUFFER_REFERENCE
#define L_MODE V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED_AT_SLICE_BOUNDARY

const char *const mpeg_video_cvp_extradata[] = {
	"Extradata none",
	"Extradata MB Quantization",
	"Extradata Interlace Video",
	"Reserved",
	"Reserved",
	"Extradata timestamp",
	"Extradata S3D Frame Packing",
	"Extradata Frame Rate",
	"Extradata Panscan Window",
	"Extradata Recovery point SEI",
	"Extradata Multislice info",
	"Extradata number of concealed MB",
	"Extradata metadata filler",
	"Extradata input crop",
	"Extradata digital zoom",
	"Extradata aspect ratio",
	"Extradata mpeg2 seqdisp",
	"Extradata stream userdata",
	"Extradata frame QP",
	"Extradata frame bits info",
	"Extradata LTR",
	"Extradata macroblock metadata",
	"Extradata VQZip SEI",
	"Extradata HDR10+ Metadata",
	"Extradata ROI QP",
	"Extradata output crop",
	"Extradata display colour SEI",
	"Extradata light level SEI",
	"Extradata PQ Info",
	"Extradata display VUI",
	"Extradata vpx color space",
	"Extradata UBWC CR stats info",
};

static void handle_session_error(enum hal_command_response cmd, void *data);
static void msm_cvp_print_running_insts(struct msm_cvp_core *core);

int msm_cvp_comm_g_ctrl_for_id(struct msm_cvp_inst *inst, int id)
{
	int rc = 0;
	struct v4l2_control ctrl = {
		.id = id,
	};

	rc = msm_comm_g_ctrl(inst, &ctrl);
	return rc ? rc : ctrl.value;
}

static struct v4l2_ctrl **get_super_cluster(struct msm_cvp_inst *inst,
				int num_ctrls)
{
	int c = 0;
	struct v4l2_ctrl **cluster = kmalloc(sizeof(struct v4l2_ctrl *) *
			num_ctrls, GFP_KERNEL);

	if (!cluster || !inst) {
		kfree(cluster);
		return NULL;
	}

	for (c = 0; c < num_ctrls; c++)
		cluster[c] =  inst->ctrls[c];

	return cluster;
}

int msm_cvp_comm_hal_to_v4l2(int id, int value)
{
	dprintk(CVP_WARN, "Unknown control (%x, %d)\n", id, value);
	return -EINVAL;
}

int msm_cvp_comm_get_v4l2_profile(int fourcc, int profile)
{
	dprintk(CVP_DBG, "%s : Begin\n", __func__);
	return -EINVAL;
}

int msm_cvp_comm_get_v4l2_level(int fourcc, int level)
{
	switch (fourcc) {
	default:
		dprintk(CVP_WARN, "Unknown codec id %x\n", fourcc);
		return 0;
	}
}

int msm_cvp_comm_ctrl_init(struct msm_cvp_inst *inst,
		struct msm_cvp_ctrl *drv_ctrls, u32 num_ctrls,
		const struct v4l2_ctrl_ops *ctrl_ops)
{
	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg = {0};
	int ret_val = 0;

	if (!inst || !drv_ctrls || !ctrl_ops || !num_ctrls) {
		dprintk(CVP_ERR, "%s - invalid input\n", __func__);
		return -EINVAL;
	}

	inst->ctrls = kcalloc(num_ctrls, sizeof(struct v4l2_ctrl *),
				GFP_KERNEL);
	if (!inst->ctrls) {
		dprintk(CVP_ERR, "%s - failed to allocate ctrl\n", __func__);
		return -ENOMEM;
	}

	ret_val = v4l2_ctrl_handler_init(&inst->ctrl_handler, num_ctrls);

	if (ret_val) {
		dprintk(CVP_ERR, "CTRL ERR: Control handler init failed, %d\n",
				inst->ctrl_handler.error);
		return ret_val;
	}

	for (; idx < num_ctrls; idx++) {
		struct v4l2_ctrl *ctrl = NULL;

		if (IS_PRIV_CTRL(drv_ctrls[idx].id)) {
			/*add private control*/
			ctrl_cfg.def = drv_ctrls[idx].default_value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = drv_ctrls[idx].id;
			ctrl_cfg.max = drv_ctrls[idx].maximum;
			ctrl_cfg.min = drv_ctrls[idx].minimum;
			ctrl_cfg.menu_skip_mask =
				drv_ctrls[idx].menu_skip_mask;
			ctrl_cfg.name = drv_ctrls[idx].name;
			ctrl_cfg.ops = ctrl_ops;
			ctrl_cfg.step = drv_ctrls[idx].step;
			ctrl_cfg.type = drv_ctrls[idx].type;
			ctrl_cfg.qmenu = drv_ctrls[idx].qmenu;

			ctrl = v4l2_ctrl_new_custom(&inst->ctrl_handler,
					&ctrl_cfg, NULL);
		} else {
			if (drv_ctrls[idx].type == V4L2_CTRL_TYPE_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(
					&inst->ctrl_handler,
					ctrl_ops,
					drv_ctrls[idx].id,
					drv_ctrls[idx].maximum,
					drv_ctrls[idx].menu_skip_mask,
					drv_ctrls[idx].default_value);
			} else {
				ctrl = v4l2_ctrl_new_std(&inst->ctrl_handler,
					ctrl_ops,
					drv_ctrls[idx].id,
					drv_ctrls[idx].minimum,
					drv_ctrls[idx].maximum,
					drv_ctrls[idx].step,
					drv_ctrls[idx].default_value);
			}
		}

		if (!ctrl) {
			dprintk(CVP_ERR, "%s - invalid ctrl %s\n", __func__,
				 drv_ctrls[idx].name);
			return -EINVAL;
		}

		ret_val = inst->ctrl_handler.error;
		if (ret_val) {
			dprintk(CVP_ERR,
				"Error adding ctrl (%s) to ctrl handle, %d\n",
				drv_ctrls[idx].name, inst->ctrl_handler.error);
			return ret_val;
		}

		ctrl->flags |= drv_ctrls[idx].flags;
		inst->ctrls[idx] = ctrl;
	}

	/* Construct a super cluster of all controls */
	inst->cluster = get_super_cluster(inst, num_ctrls);
	if (!inst->cluster) {
		dprintk(CVP_WARN,
			"Failed to setup super cluster\n");
		return -EINVAL;
	}

	v4l2_ctrl_cluster(num_ctrls, inst->cluster);

	return ret_val;
}

int msm_cvp_comm_ctrl_deinit(struct msm_cvp_inst *inst)
{
	if (!inst) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	kfree(inst->ctrls);
	kfree(inst->cluster);
	v4l2_ctrl_handler_free(&inst->ctrl_handler);

	return 0;
}

int msm_cvp_comm_set_stream_output_mode(struct msm_cvp_inst *inst,
		enum multi_stream mode)
{
	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!is_decode_session(inst)) {
		dprintk(CVP_DBG, "%s: not a decode session %x\n",
			__func__, hash32_ptr(inst->session));
		return -EINVAL;
	}

	if (mode == HAL_VIDEO_DECODER_SECONDARY)
		inst->stream_output_mode = HAL_VIDEO_DECODER_SECONDARY;
	else
		inst->stream_output_mode = HAL_VIDEO_DECODER_PRIMARY;

	return 0;
}

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

static int msm_cvp_comm_get_mbs_per_sec(struct msm_cvp_inst *inst)
{
	int output_port_mbs, capture_port_mbs;
	int fps;

	output_port_mbs = inst->in_reconfig ?
			NUM_MBS_PER_FRAME(inst->reconfig_width,
				inst->reconfig_height) :
			NUM_MBS_PER_FRAME(inst->prop.width[OUTPUT_PORT],
				inst->prop.height[OUTPUT_PORT]);

	capture_port_mbs = NUM_MBS_PER_FRAME(inst->prop.width[CAPTURE_PORT],
		inst->prop.height[CAPTURE_PORT]);

	if ((inst->clk_data.operating_rate >> 16) > inst->prop.fps)
		fps = (inst->clk_data.operating_rate >> 16) ?
			inst->clk_data.operating_rate >> 16 : 1;
	else
		fps = inst->prop.fps;

	return max(output_port_mbs, capture_port_mbs) * fps;
}

int msm_cvp_comm_get_inst_load(struct msm_cvp_inst *inst,
		enum load_calc_quirks quirks)
{
	int load = 0;

	mutex_lock(&inst->lock);

	if (!(inst->state >= MSM_CVP_OPEN_DONE &&
		inst->state < MSM_CVP_STOP_DONE))
		goto exit;

	load = msm_cvp_comm_get_mbs_per_sec(inst);

	if (is_thumbnail_session(inst)) {
		if (quirks & LOAD_CALC_IGNORE_THUMBNAIL_LOAD)
			load = 0;
	}

	if (is_turbo_session(inst)) {
		if (!(quirks & LOAD_CALC_IGNORE_TURBO_LOAD))
			load = inst->core->resources.max_load;
	}

	/*  Clock and Load calculations for REALTIME/NON-REALTIME
	 *                        OPERATING RATE SET/NO OPERATING RATE SET
	 *
	 *                 | OPERATING RATE SET   | OPERATING RATE NOT SET |
	 * ----------------|--------------------- |------------------------|
	 * REALTIME        | load = res * op_rate |  load = res * fps      |
	 *                 | clk  = res * op_rate |  clk  = res * fps      |
	 * ----------------|----------------------|------------------------|
	 * NON-REALTIME    | load = res * 1 fps   |  load = res * 1 fps    |
	 *                 | clk  = res * op_rate |  clk  = res * fps      |
	 * ----------------|----------------------|------------------------|
	 */

	if (!is_realtime_session(inst) &&
		(quirks & LOAD_CALC_IGNORE_NON_REALTIME_LOAD)) {
		if (!inst->prop.fps) {
			dprintk(CVP_INFO, "instance:%pK fps = 0\n", inst);
			load = 0;
		} else {
			load =
			msm_cvp_comm_get_mbs_per_sec(inst)/inst->prop.fps;
		}
	}

exit:
	mutex_unlock(&inst->lock);
	return load;
}

int msm_cvp_comm_get_inst_load_per_core(struct msm_cvp_inst *inst,
		enum load_calc_quirks quirks)
{
	int load = msm_cvp_comm_get_inst_load(inst, quirks);

	if (inst->clk_data.core_id == CVP_CORE_ID_3)
		load = load / 2;

	return load;
}

int msm_cvp_comm_get_load(struct msm_cvp_core *core,
	enum session_type type, enum load_calc_quirks quirks)
{
	struct msm_cvp_inst *inst = NULL;
	int num_mbs_per_sec = 0;

	if (!core) {
		dprintk(CVP_ERR, "Invalid args: %pK\n", core);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->session_type != type)
			continue;

		num_mbs_per_sec += msm_cvp_comm_get_inst_load(inst, quirks);
	}
	mutex_unlock(&core->lock);

	return num_mbs_per_sec;
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

enum hal_uncompressed_format msm_cvp_comm_get_hal_uncompressed(int fourcc)
{
	return HAL_UNUSED_COLOR;
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

struct msm_cvp_format_constraint *msm_cvp_comm_get_pixel_fmt_constraints(
	struct msm_cvp_format_constraint fmt[], int size, int fourcc)
{
	int i;

	if (!fmt) {
		dprintk(CVP_ERR, "Invalid inputs, fmt = %pK\n", fmt);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (fmt[i].fourcc == fourcc)
			break;
	}
	if (i == size) {
		dprintk(CVP_INFO, "Format constraint not found.\n");
		return NULL;
	}
	return &fmt[i];
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

static void cvp_handle_session_dfs_cmd_done(enum hal_command_response cmd,
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
			dprintk(CVP_DBG, "releasing persist: %x\n",
					buf->smem.device_addr);
			buf_found = true;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	if (!buf_found)
		dprintk(CVP_ERR, "invalid buffer received from firmware");
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

static void print_cap(const char *type,
		struct hal_capability_supported *cap)
{
	dprintk(CVP_DBG,
		"%-24s: %-8d %-8d %-8d\n",
		type, cap->min, cap->max, cap->step_size);
}

//static int msm_cvp_comm_update_ctrl(struct msm_cvp_inst *inst,
//	u32 id, struct hal_capability_supported *capability)
//{
//	struct v4l2_ctrl *ctrl = NULL;
//	int rc = 0;
//
//	ctrl = v4l2_ctrl_find(&inst->ctrl_handler, id);
//	if (ctrl) {
//		v4l2_ctrl_modify_range(ctrl, capability->min,
//				capability->max, ctrl->step,
//				ctrl->default_value);
//		dprintk(CVP_DBG,
//			"%s: Updated Range = %lld --> %lld Def value = %lld\n",
//			ctrl->name, ctrl->minimum, ctrl->maximum,
//			ctrl->default_value);
//	} else {
//		dprintk(CVP_ERR,
//			"Failed to find Conrol %d\n", id);
//		rc = -EINVAL;
//	}
//
//	return rc;
//	}

static void msm_cvp_comm_update_ctrl_limits(struct msm_cvp_inst *inst)
{
	//msm_cvp_comm_update_ctrl(inst,
	//	V4L2_CID_MPEG_VIDC_VIDEO_FRAME_RATE,
	//	&inst->capability.frame_rate);
}

static void handle_session_init_done(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst = NULL;
	struct msm_cvp_capability *capability = NULL;
	struct hfi_device *hdev;
	struct msm_cvp_core *core;
	struct hal_profile_level *profile_level;
	u32 i, codec;

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

	core = inst->core;
	hdev = inst->core->device;
	codec = inst->session_type == MSM_CVP_DECODER ?
			inst->fmts[OUTPUT_PORT].fourcc :
			inst->fmts[CAPTURE_PORT].fourcc;

	/* check if capabilities are available for this session */
	for (i = 0; i < CVP_MAX_SESSIONS; i++) {
		if (core->capabilities[i].codec ==
				get_cvp_hal_codec(codec) &&
			core->capabilities[i].domain ==
				get_cvp_hal_domain(inst->session_type)) {
			capability = &core->capabilities[i];
			break;
		}
	}

	if (capability) {
		dprintk(CVP_DBG,
			"%s: capabilities for codec 0x%x, domain %#x\n",
			__func__, capability->codec, capability->domain);
		memcpy(&inst->capability, capability,
			sizeof(struct msm_cvp_capability));
	} else {
		dprintk(CVP_ERR,
			"Watch out : Some property may fail inst %pK\n", inst);
		dprintk(CVP_ERR,
			"Caps N/A for codec 0x%x, domain %#x\n",
			inst->capability.codec, inst->capability.domain);
	}
	inst->capability.pixelprocess_capabilities =
		call_hfi_op(hdev, get_core_capabilities, hdev->hfi_device_data);

	dprintk(CVP_DBG,
		"Capability type : min      max      step size\n");
	print_cap("width", &inst->capability.width);
	print_cap("height", &inst->capability.height);
	print_cap("mbs_per_frame", &inst->capability.mbs_per_frame);
	print_cap("mbs_per_sec", &inst->capability.mbs_per_sec);
	print_cap("frame_rate", &inst->capability.frame_rate);
	print_cap("bitrate", &inst->capability.bitrate);
	print_cap("peak_bitrate", &inst->capability.peakbitrate);
	print_cap("scale_x", &inst->capability.scale_x);
	print_cap("scale_y", &inst->capability.scale_y);
	print_cap("hier_p", &inst->capability.hier_p);
	print_cap("ltr_count", &inst->capability.ltr_count);
	print_cap("bframe", &inst->capability.bframe);
	print_cap("secure_output2_threshold",
		&inst->capability.secure_output2_threshold);
	print_cap("hier_b", &inst->capability.hier_b);
	print_cap("lcu_size", &inst->capability.lcu_size);
	print_cap("hier_p_hybrid", &inst->capability.hier_p_hybrid);
	print_cap("mbs_per_sec_low_power",
		&inst->capability.mbs_per_sec_power_save);
	print_cap("extradata", &inst->capability.extradata);
	print_cap("profile", &inst->capability.profile);
	print_cap("level", &inst->capability.level);
	print_cap("i_qp", &inst->capability.i_qp);
	print_cap("p_qp", &inst->capability.p_qp);
	print_cap("b_qp", &inst->capability.b_qp);
	print_cap("rc_modes", &inst->capability.rc_modes);
	print_cap("blur_width", &inst->capability.blur_width);
	print_cap("blur_height", &inst->capability.blur_height);
	print_cap("slice_delivery_mode", &inst->capability.slice_delivery_mode);
	print_cap("slice_bytes", &inst->capability.slice_bytes);
	print_cap("slice_mbs", &inst->capability.slice_mbs);
	print_cap("secure", &inst->capability.secure);
	print_cap("max_num_b_frames", &inst->capability.max_num_b_frames);
	print_cap("max_video_cores", &inst->capability.max_video_cores);
	print_cap("max_work_modes", &inst->capability.max_work_modes);
	print_cap("ubwc_cr_stats", &inst->capability.ubwc_cr_stats);

	dprintk(CVP_DBG, "profile count : %u\n",
		inst->capability.profile_level.profile_count);
	for (i = 0; i < inst->capability.profile_level.profile_count; i++) {
		profile_level =
			&inst->capability.profile_level.profile_level[i];
		dprintk(CVP_DBG, "profile : %u\n", profile_level->profile);
		dprintk(CVP_DBG, "level   : %u\n", profile_level->level);
	}

	signal_session_msg_receipt(cmd, inst);

	/*
	 * Update controls after informing session_init_done to avoid
	 * timeouts.
	 */

	msm_cvp_comm_update_ctrl_limits(inst);
	cvp_put_inst(inst);
}

static void handle_event_change(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_inst *inst = NULL;
	struct msm_cvp_cb_event *event_notify = data;
	int event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
	struct v4l2_event seq_changed_event = {0};
	int rc = 0;
	struct hfi_device *hdev;
	u32 *ptr = NULL;
	struct hal_buffer_requirements *bufreq;
	int extra_buff_count = 0;

	if (!event_notify) {
		dprintk(CVP_WARN, "Got an empty event from hfi\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(event_notify->device_id),
			event_notify->session_id);
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_WARN, "Got a response for an inactive session\n");
		goto err_bad_event;
	}
	hdev = inst->core->device;

	switch (event_notify->hal_event_type) {
	case HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES:
		event = V4L2_EVENT_SEQ_CHANGED_SUFFICIENT;
		break;
	case HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES:
		event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
		break;
	case HAL_EVENT_RELEASE_BUFFER_REFERENCE:
	{
		struct msm_video_buffer *mbuf;
		u32 planes[VIDEO_MAX_PLANES] = {0};

		dprintk(CVP_DBG,
			"%s: inst: %pK data_buffer: %x extradata_buffer: %x\n",
			__func__, inst, event_notify->packet_buffer,
			event_notify->extra_data_buffer);

		planes[0] = event_notify->packet_buffer;
		planes[1] = event_notify->extra_data_buffer;
		mbuf = msm_cvp_comm_get_buffer_using_device_planes(inst,
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, planes);
		if (!mbuf || !kref_cvp_get_mbuf(inst, mbuf)) {
			dprintk(CVP_ERR,
				"%s: data_addr %x, extradata_addr %x not found\n",
				__func__, planes[0], planes[1]);
		} else {
			//handle_release_buffer_reference(inst, mbuf);
			kref_cvp_put_mbuf(mbuf);
		}
		goto err_bad_event;
	}
	default:
		break;
	}

	/* Bit depth and pic struct changed event are combined into a single
	 * event (insufficient event) for the userspace. Currently bitdepth
	 * changes is only for HEVC and interlaced support is for all
	 * codecs except HEVC
	 * event data is now as follows:
	 * u32 *ptr = seq_changed_event.u.data;
	 * ptr[0] = height
	 * ptr[1] = width
	 * ptr[2] = bit depth
	 * ptr[3] = pic struct (progressive or interlaced)
	 * ptr[4] = colour space
	 * ptr[5] = crop_data(top)
	 * ptr[6] = crop_data(left)
	 * ptr[7] = crop_data(height)
	 * ptr[8] = crop_data(width)
	 * ptr[9] = profile
	 * ptr[10] = level
	 */

	inst->entropy_mode = event_notify->entropy_mode;
	inst->profile = event_notify->profile;
	inst->level = event_notify->level;
	inst->prop.crop_info.left =
		event_notify->crop_data.left;
	inst->prop.crop_info.top =
		event_notify->crop_data.top;
	inst->prop.crop_info.height =
		event_notify->crop_data.height;
	inst->prop.crop_info.width =
		event_notify->crop_data.width;
	/* HW returns progressive_only flag in pic_struct. */
	inst->pic_struct =
		event_notify->pic_struct ?
		MSM_CVP_PIC_STRUCT_PROGRESSIVE :
		MSM_CVP_PIC_STRUCT_MAYBE_INTERLACED;

	ptr = (u32 *)seq_changed_event.u.data;
	ptr[0] = event_notify->height;
	ptr[1] = event_notify->width;
	ptr[2] = event_notify->bit_depth;
	ptr[3] = event_notify->pic_struct;
	ptr[4] = event_notify->colour_space;
	ptr[5] = event_notify->crop_data.top;
	ptr[6] = event_notify->crop_data.left;
	ptr[7] = event_notify->crop_data.height;
	ptr[8] = event_notify->crop_data.width;
	ptr[9] = msm_cvp_comm_get_v4l2_profile(
		inst->fmts[OUTPUT_PORT].fourcc,
		event_notify->profile);
	ptr[10] = msm_cvp_comm_get_v4l2_level(
		inst->fmts[OUTPUT_PORT].fourcc,
		event_notify->level);

	dprintk(CVP_DBG,
		"Event payload: height = %u width = %u profile = %u level = %u\n",
			event_notify->height, event_notify->width,
			ptr[9], ptr[10]);

	dprintk(CVP_DBG,
		"Event payload: bit_depth = %u pic_struct = %u colour_space = %u\n",
		event_notify->bit_depth, event_notify->pic_struct,
			event_notify->colour_space);

	dprintk(CVP_DBG,
		"Event payload: CROP top = %u left = %u Height = %u Width = %u\n",
			event_notify->crop_data.top,
			event_notify->crop_data.left,
			event_notify->crop_data.height,
			event_notify->crop_data.width);

	mutex_lock(&inst->lock);
	inst->in_reconfig = true;
	inst->reconfig_height = event_notify->height;
	inst->reconfig_width = event_notify->width;
	inst->bit_depth = event_notify->bit_depth;

	if (msm_cvp_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		bufreq = get_cvp_buff_req_buffer(inst,
				HAL_BUFFER_OUTPUT);
		if (!bufreq) {
			mutex_unlock(&inst->lock);
			return;
		}

		/* No need to add extra buffers to DPBs */
		bufreq->buffer_count_min = event_notify->capture_buf_count;
		bufreq->buffer_count_min_host = bufreq->buffer_count_min;

		bufreq = get_cvp_buff_req_buffer(inst,
				HAL_BUFFER_OUTPUT2);
		if (!bufreq) {
			mutex_unlock(&inst->lock);
			return;
		}

		extra_buff_count = msm_cvp_get_extra_buff_count(inst,
						HAL_BUFFER_OUTPUT2);
		bufreq->buffer_count_min = event_notify->capture_buf_count;
		bufreq->buffer_count_min_host = bufreq->buffer_count_min +
							extra_buff_count;
	} else {

		bufreq = get_cvp_buff_req_buffer(inst,
				HAL_BUFFER_OUTPUT);
		if (!bufreq) {
			mutex_unlock(&inst->lock);
			return;
		}

		extra_buff_count = msm_cvp_get_extra_buff_count(inst,
						HAL_BUFFER_OUTPUT);
		bufreq->buffer_count_min = event_notify->capture_buf_count;
		bufreq->buffer_count_min_host = bufreq->buffer_count_min +
							extra_buff_count;
	}
	dprintk(CVP_DBG, "%s: buffer[%d] count: min %d min_host %d\n",
		__func__, bufreq->buffer_type, bufreq->buffer_count_min,
		bufreq->buffer_count_min_host);

	mutex_unlock(&inst->lock);

	rc = msm_cvp_check_session_supported(inst);
	if (!rc) {
		seq_changed_event.type = event;
		v4l2_event_queue_fh(&inst->event_handler, &seq_changed_event);
	} else if (rc == -ENOTSUPP) {
		msm_cvp_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_CVP_HW_UNSUPPORTED);
	} else if (rc == -EBUSY) {
		msm_cvp_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_CVP_HW_OVERLOAD);
	}

err_bad_event:
	cvp_put_inst(inst);
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

void msm_cvp_comm_validate_output_buffers(struct msm_cvp_inst *inst)
{
	struct internal_buf *binfo;
	u32 buffers_owned_by_driver = 0;
	struct hal_buffer_requirements *output_buf;

	output_buf = get_cvp_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);

	if (!output_buf) {
		dprintk(CVP_DBG,
			"This output buffer not required, buffer_type: %x\n",
			HAL_BUFFER_OUTPUT);
		return;
	}
	mutex_lock(&inst->outputbufs.lock);
	if (list_empty(&inst->outputbufs.list)) {
		dprintk(CVP_DBG, "%s: no OUTPUT buffers allocated\n",
			__func__);
		mutex_unlock(&inst->outputbufs.lock);
		return;
	}
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
		if (binfo->buffer_ownership != DRIVER) {
			dprintk(CVP_DBG,
				"This buffer is with FW %x\n",
				binfo->smem.device_addr);
			continue;
		}
		buffers_owned_by_driver++;
	}
	mutex_unlock(&inst->outputbufs.lock);

	if (buffers_owned_by_driver != output_buf->buffer_count_actual) {
		dprintk(CVP_WARN,
			"OUTPUT Buffer count mismatch %d of %d\n",
			buffers_owned_by_driver,
			output_buf->buffer_count_actual);
		msm_cvp_handle_hw_error(inst->core);
	}
}

int msm_cvp_comm_queue_output_buffers(struct msm_cvp_inst *inst)
{
	struct internal_buf *binfo;
	struct hfi_device *hdev;
	struct cvp_frame_data frame_data = {0};
	struct hal_buffer_requirements *output_buf, *extra_buf;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	output_buf = get_cvp_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!output_buf) {
		dprintk(CVP_DBG,
			"This output buffer not required, buffer_type: %x\n",
			HAL_BUFFER_OUTPUT);
		return 0;
	}
	dprintk(CVP_DBG,
		"output: num = %d, size = %d\n",
		output_buf->buffer_count_actual,
		output_buf->buffer_size);

	extra_buf = get_cvp_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_OUTPUT);

	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
		if (binfo->buffer_ownership != DRIVER)
			continue;
		if (binfo->mark_remove)
			continue;
		frame_data.alloc_len = output_buf->buffer_size;
		frame_data.filled_len = 0;
		frame_data.offset = 0;
		frame_data.device_addr = binfo->smem.device_addr;
		frame_data.flags = 0;
		frame_data.extradata_addr = binfo->smem.device_addr +
		output_buf->buffer_size;
		frame_data.buffer_type = HAL_BUFFER_OUTPUT;
		frame_data.extradata_size = extra_buf ?
			extra_buf->buffer_size : 0;
		//rc = call_hfi_op(hdev, session_ftb,
		//	(void *) inst->session, &frame_data);
		binfo->buffer_ownership = FIRMWARE;
	}
	mutex_unlock(&inst->outputbufs.lock);

	return 0;
}

static void handle_session_flush(enum hal_command_response cmd, void *data)
{
	struct msm_cvp_cb_cmd_done *response = data;
	struct msm_cvp_inst *inst;
	struct v4l2_event flush_event = {0};
	u32 *ptr = NULL;
	enum hal_flush flush_type;
	int rc;

	if (!response) {
		dprintk(CVP_ERR, "Failed to get valid response for flush\n");
		return;
	}

	inst = cvp_get_inst(get_cvp_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(CVP_WARN, "Got a response for an inactive session\n");
		return;
	}

	mutex_lock(&inst->flush_lock);
	if (msm_cvp_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {

		if (!(inst->fmts[OUTPUT_PORT].defer_outputs &&
				inst->in_reconfig))
			msm_cvp_comm_validate_output_buffers(inst);

		if (!inst->in_reconfig) {
			rc = msm_cvp_comm_queue_output_buffers(inst);
			if (rc) {
				dprintk(CVP_ERR,
						"Failed to queue output buffers: %d\n",
						rc);
			}
		}
	}
	inst->in_flush = false;
	flush_event.type = V4L2_EVENT_MSM_CVP_FLUSH_DONE;
	ptr = (u32 *)flush_event.u.data;

	flush_type = response->data.flush_type;
	switch (flush_type) {
	case HAL_FLUSH_INPUT:
		ptr[0] = V4L2_CMD_FLUSH_OUTPUT;
		break;
	case HAL_FLUSH_OUTPUT:
		ptr[0] = V4L2_CMD_FLUSH_CAPTURE;
		break;
	case HAL_FLUSH_ALL:
		ptr[0] |= V4L2_CMD_FLUSH_CAPTURE;
		ptr[0] |= V4L2_CMD_FLUSH_OUTPUT;
		break;
	default:
		dprintk(CVP_ERR, "Invalid flush type received!");
		goto exit;
	}

	dprintk(CVP_DBG,
		"Notify flush complete, flush_type: %x\n", flush_type);
	v4l2_event_queue_fh(&inst->event_handler, &flush_event);

exit:
	mutex_unlock(&inst->flush_lock);
	cvp_put_inst(inst);
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
	msm_cvp_noc_error_info(core);
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

struct vb2_buffer *msm_cvp_comm_get_vb_using_video_buffer(
		struct msm_cvp_inst *inst, struct msm_video_buffer *mbuf)
{
	u32 port = 0;
	struct vb2_buffer *vb = NULL;
	struct vb2_queue *q = NULL;
	bool found = false;

	if (mbuf->vvb.vb2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		port = CAPTURE_PORT;
	} else if (mbuf->vvb.vb2_buf.type ==
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		port = OUTPUT_PORT;
	} else {
		dprintk(CVP_ERR, "%s: invalid type %d\n",
			__func__, mbuf->vvb.vb2_buf.type);
		return NULL;
	}

	mutex_lock(&inst->bufq[port].lock);
	found = false;
	q = &inst->bufq[port].vb2_bufq;
	if (!q->streaming) {
		dprintk(CVP_ERR, "port %d is not streaming", port);
		goto unlock;
	}
	list_for_each_entry(vb, &q->queued_list, queued_entry) {
		if (vb->state != VB2_BUF_STATE_ACTIVE)
			continue;
		if (msm_cvp_comm_compare_vb2_planes(inst, mbuf, vb)) {
			found = true;
			break;
		}
	}
unlock:
	mutex_unlock(&inst->bufq[port].lock);
	if (!found) {
		print_video_buffer(CVP_ERR, "vb2 not found for", inst, mbuf);
		return NULL;
	}

	return vb;
}

int msm_cvp_comm_vb2_buffer_done(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	struct vb2_buffer *vb2;
	struct vb2_v4l2_buffer *vbuf;
	u32 i, port;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}

	if (mbuf->vvb.vb2_buf.type ==
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		port = CAPTURE_PORT;
	else if (mbuf->vvb.vb2_buf.type ==
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		port = OUTPUT_PORT;
	else
		return -EINVAL;

	vb2 = msm_cvp_comm_get_vb_using_video_buffer(inst, mbuf);
	if (!vb2)
		return -EINVAL;

	/*
	 * access vb2 buffer under q->lock and if streaming only to
	 * ensure the buffer was not free'd by vb2 framework while
	 * we are accessing it here.
	 */
	mutex_lock(&inst->bufq[port].lock);
	if (inst->bufq[port].vb2_bufq.streaming) {
		vbuf = to_vb2_v4l2_buffer(vb2);
		vbuf->flags = mbuf->vvb.flags;
		vb2->timestamp = mbuf->vvb.vb2_buf.timestamp;
		for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
			vb2->planes[i].bytesused =
				mbuf->vvb.vb2_buf.planes[i].bytesused;
			vb2->planes[i].data_offset =
				mbuf->vvb.vb2_buf.planes[i].data_offset;
		}
		vb2_buffer_done(vb2, VB2_BUF_STATE_DONE);
	} else {
		dprintk(CVP_ERR, "%s: port %d is not streaming\n",
			__func__, port);
	}
	mutex_unlock(&inst->bufq[port].lock);

	return 0;
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

static void handle_dfs(enum hal_command_response cmd, void *data)
{
	dprintk(CVP_ERR, "%s: is called\n", __func__);
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
	case HAL_SESSION_CVP_DFS:
		handle_dfs(cmd, data);
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
	case HAL_SESSION_REGISTER_BUFFER_DONE:
		cvp_handle_session_register_buffer_done(cmd, data);
		break;
	case HAL_SESSION_UNREGISTER_BUFFER_DONE:
		cvp_handle_session_unregister_buffer_done(cmd, data);
		break;
	case HAL_SESSION_DFS_CONFIG_CMD_DONE:
	case HAL_SESSION_DFS_FRAME_CMD_DONE:
		cvp_handle_session_dfs_cmd_done(cmd, data);
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

bool cvp_is_batching_allowed(struct msm_cvp_inst *inst)
{
	bool allowed = false;

	if (!inst || !inst->core)
		return false;

	/*
	 * Enable decode batching based on below conditions
	 * - platform supports batching
	 * - decode session and H264/HEVC/VP9 format
	 * - session resolution <= 1080p
	 * - low latency not enabled
	 * - not a thumbnail session
	 * - UBWC color format
	 */
	if (inst->core->resources.decode_batching && is_decode_session(inst) &&
		(inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_H264 ||
		inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_HEVC ||
		inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9) &&
		(msm_cvp_get_mbs_per_frame(inst) <=
		NUM_MBS_PER_FRAME(MAX_DEC_BATCH_HEIGHT, MAX_DEC_BATCH_WIDTH)) &&
		!inst->clk_data.low_latency_mode &&
		!is_thumbnail_session(inst) &&
		(inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_NV12_UBWC ||
		inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_NV12_TP10_UBWC))
		allowed = true;

	return allowed;
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
		dprintk(CVP_INFO, "Video core: %d is already in state: %d\n",
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

static int msm_comm_init_buffer_count(struct msm_cvp_inst *inst)
{
	int extra_buff_count = 0;
	struct hal_buffer_requirements *bufreq;
	int rc = 0;
	int port;

	if (!is_decode_session(inst) && !is_encode_session(inst))
		return 0;

	if (is_decode_session(inst))
		port = OUTPUT_PORT;
	else
		port = CAPTURE_PORT;

	/* Update input buff counts */
	bufreq = get_cvp_buff_req_buffer(inst, HAL_BUFFER_INPUT);
	if (!bufreq)
		return -EINVAL;

	extra_buff_count = msm_cvp_get_extra_buff_count(inst,
				HAL_BUFFER_INPUT);
	bufreq->buffer_count_min = inst->fmts[port].input_min_count;
	/* batching needs minimum batch size count of input buffers */
	if (inst->core->resources.decode_batching &&
		is_decode_session(inst) &&
		bufreq->buffer_count_min < inst->batch.size)
		bufreq->buffer_count_min = inst->batch.size;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
				bufreq->buffer_count_min + extra_buff_count;

	dprintk(CVP_DBG, "%s: %x : input min %d min_host %d actual %d\n",
		__func__, hash32_ptr(inst->session),
		bufreq->buffer_count_min, bufreq->buffer_count_min_host,
		bufreq->buffer_count_actual);

	rc = msm_cvp_comm_set_buffer_count(inst,
			bufreq->buffer_count_min,
			bufreq->buffer_count_actual, HAL_BUFFER_INPUT);
	if (rc) {
		dprintk(CVP_ERR,
			"%s: Failed to set in buffer count to FW\n",
			__func__);
		return -EINVAL;
	}

	bufreq = get_cvp_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_INPUT);
	if (!bufreq)
		return -EINVAL;

	bufreq->buffer_count_min = inst->fmts[port].input_min_count;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
				bufreq->buffer_count_min + extra_buff_count;

	/* Update output buff count */
	bufreq = get_cvp_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!bufreq)
		return -EINVAL;

	extra_buff_count = msm_cvp_get_extra_buff_count(inst,
				HAL_BUFFER_OUTPUT);
	bufreq->buffer_count_min = inst->fmts[port].output_min_count;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
		bufreq->buffer_count_min + extra_buff_count;

	dprintk(CVP_DBG, "%s: %x : output min %d min_host %d actual %d\n",
		__func__, hash32_ptr(inst->session),
		bufreq->buffer_count_min, bufreq->buffer_count_min_host,
		bufreq->buffer_count_actual);

	rc = msm_cvp_comm_set_buffer_count(inst,
		bufreq->buffer_count_min,
		bufreq->buffer_count_actual, HAL_BUFFER_OUTPUT);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to set out buffer count to FW\n");
		return -EINVAL;
	}

	bufreq = get_cvp_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_OUTPUT);
	if (!bufreq)
		return -EINVAL;

	bufreq->buffer_count_min = inst->fmts[port].output_min_count;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
		bufreq->buffer_count_min + extra_buff_count;

	return 0;
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

	rc = msm_comm_init_buffer_count(inst);
	if (rc) {
		dprintk(CVP_ERR, "Failed to initialize buff counts\n");
		goto exit;
	}
	change_cvp_inst_state(inst, MSM_CVP_OPEN);

exit:
	return rc;
}

static void msm_cvp_print_running_insts(struct msm_cvp_core *core)
{
	struct msm_cvp_inst *temp;
	int op_rate = 0;

	dprintk(CVP_ERR, "Running instances:\n");
	dprintk(CVP_ERR, "%4s|%4s|%4s|%4s|%4s|%4s\n",
			"type", "w", "h", "fps", "opr", "prop");

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp->state >= MSM_CVP_OPEN_DONE &&
				temp->state < MSM_CVP_STOP_DONE) {
			char properties[4] = "";

			if (is_thumbnail_session(temp))
				strlcat(properties, "N", sizeof(properties));

			if (is_turbo_session(temp))
				strlcat(properties, "T", sizeof(properties));

			if (is_realtime_session(temp))
				strlcat(properties, "R", sizeof(properties));

			if (temp->clk_data.operating_rate)
				op_rate = temp->clk_data.operating_rate >> 16;
			else
				op_rate = temp->prop.fps;

			dprintk(CVP_ERR, "%4d|%4d|%4d|%4d|%4d|%4s\n",
					temp->session_type,
					max(temp->prop.width[CAPTURE_PORT],
						temp->prop.width[OUTPUT_PORT]),
					max(temp->prop.height[CAPTURE_PORT],
						temp->prop.height[OUTPUT_PORT]),
					temp->prop.fps, op_rate, properties);
		}
	}
	mutex_unlock(&core->lock);
}

static int msm_cvp_load_resources(int flipped_state,
	struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	int num_mbs_per_sec = 0, max_load_adj = 0;
	struct msm_cvp_core *core;
	enum load_calc_quirks quirks = LOAD_CALC_IGNORE_TURBO_LOAD |
		LOAD_CALC_IGNORE_THUMBNAIL_LOAD |
		LOAD_CALC_IGNORE_NON_REALTIME_LOAD;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR,
			"%s: inst %pK is in invalid state\n", __func__, inst);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_CVP_LOAD_RESOURCES)) {
		dprintk(CVP_INFO, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	core = inst->core;

	num_mbs_per_sec =
		msm_cvp_comm_get_load(core, MSM_CVP_DECODER, quirks) +
		msm_cvp_comm_get_load(core, MSM_CVP_ENCODER, quirks);

	max_load_adj = core->resources.max_load +
		inst->capability.mbs_per_frame.max;

	if (num_mbs_per_sec > max_load_adj) {
		dprintk(CVP_ERR, "HW is overloaded, needed: %d max: %d\n",
			num_mbs_per_sec, max_load_adj);
		msm_cvp_print_running_insts(core);
		msm_cvp_comm_kill_session(inst);
		return -EBUSY;
	}

	hdev = core->device;
	dprintk(CVP_DBG, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_load_res, (void *) inst->session);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to send load resources\n");
		goto exit;
	}
	change_cvp_inst_state(inst, MSM_CVP_LOAD_RESOURCES);
exit:
	return rc;
}

static int msm_cvp_start(int flipped_state, struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR,
			"%s: inst %pK is in invalid\n", __func__, inst);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_CVP_START)) {
		dprintk(CVP_INFO,
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	hdev = inst->core->device;
	dprintk(CVP_DBG, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_start, (void *) inst->session);
	if (rc) {
		dprintk(CVP_ERR,
			"Failed to send start\n");
		goto exit;
	}
	change_cvp_inst_state(inst, MSM_CVP_START);
exit:
	return rc;
}

static int msm_cvp_stop(int flipped_state, struct msm_cvp_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR,
			"%s: inst %pK is in invalid state\n", __func__, inst);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_CVP_STOP)) {
		dprintk(CVP_INFO,
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	hdev = inst->core->device;
	dprintk(CVP_DBG, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_stop, (void *) inst->session);
	if (rc) {
		dprintk(CVP_ERR, "%s: inst %pK session_stop failed\n",
				__func__, inst);
		goto exit;
	}
	change_cvp_inst_state(inst, MSM_CVP_STOP);
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

int msm_cvp_comm_reset_bufreqs(struct msm_cvp_inst *inst,
	enum hal_buffer buf_type)
{
	struct hal_buffer_requirements *bufreqs;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	bufreqs = get_cvp_buff_req_buffer(inst, buf_type);
	if (!bufreqs) {
		dprintk(CVP_ERR, "%s: invalid buf type %d\n",
			__func__, buf_type);
		return -EINVAL;
	}
	bufreqs->buffer_size = bufreqs->buffer_region_size =
	bufreqs->buffer_count_min = bufreqs->buffer_count_min_host =
	bufreqs->buffer_count_actual = bufreqs->contiguous =
	bufreqs->buffer_alignment = 0;

	return 0;
}

int msm_cvp_comm_copy_bufreqs(struct msm_cvp_inst *inst,
	enum hal_buffer src_type, enum hal_buffer dst_type)
{
	struct hal_buffer_requirements *src_bufreqs;
	struct hal_buffer_requirements *dst_bufreqs;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	src_bufreqs = get_cvp_buff_req_buffer(inst, src_type);
	dst_bufreqs = get_cvp_buff_req_buffer(inst, dst_type);
	if (!src_bufreqs || !dst_bufreqs) {
		dprintk(CVP_ERR, "%s: invalid buf type: src %d dst %d\n",
			__func__, src_type, dst_type);
		return -EINVAL;
	}
	dst_bufreqs->buffer_size = src_bufreqs->buffer_size;
	dst_bufreqs->buffer_region_size = src_bufreqs->buffer_region_size;
	dst_bufreqs->buffer_count_min = src_bufreqs->buffer_count_min;
	dst_bufreqs->buffer_count_min_host = src_bufreqs->buffer_count_min_host;
	dst_bufreqs->buffer_count_actual = src_bufreqs->buffer_count_actual;
	dst_bufreqs->contiguous = src_bufreqs->contiguous;
	dst_bufreqs->buffer_alignment = src_bufreqs->buffer_alignment;

	return 0;
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
		rc = msm_cvp_load_resources(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_LOAD_RESOURCES_DONE:
	case MSM_CVP_START:
		rc = msm_cvp_start(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_START_DONE:
		dprintk(CVP_ERR, "Deprecated HFI packet: START_DONE\n");
			break;
	case MSM_CVP_STOP:
		rc = msm_cvp_stop(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_CVP_STOP_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_CVP_STOP_DONE,
				HAL_SESSION_STOP_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		dprintk(CVP_DBG, "Moving to Stop Done state\n");
	case MSM_CVP_RELEASE_RESOURCES:
		dprintk(CVP_ERR, "Deprecated state RELEASE_SOURCES\n");
	case MSM_CVP_RELEASE_RESOURCES_DONE:
		dprintk(CVP_ERR, "Deprecated state RELEASE_SOURCES_DONE\n");
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

int msm_cvp_comm_cmd(void *instance, union msm_v4l2_cmd *cmd)
{
	return 0;
}

static void populate_frame_data(struct cvp_frame_data *data,
		struct msm_video_buffer *mbuf, struct msm_cvp_inst *inst)
{
	u64 time_usec;
	int extra_idx;
	struct vb2_buffer *vb;
	struct vb2_v4l2_buffer *vbuf;

	if (!inst || !mbuf || !data) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK %pK\n",
			__func__, inst, mbuf, data);
		return;
	}

	vb = &mbuf->vvb.vb2_buf;
	vbuf = to_vb2_v4l2_buffer(vb);

	time_usec = vb->timestamp;
	do_div(time_usec, NSEC_PER_USEC);

	data->alloc_len = vb->planes[0].length;
	data->device_addr = mbuf->smem[0].device_addr;
	data->timestamp = time_usec;
	data->flags = 0;
	data->clnt_data = data->device_addr;

	if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		data->buffer_type = HAL_BUFFER_INPUT;
		data->filled_len = vb->planes[0].bytesused;
		data->offset = vb->planes[0].data_offset;

		if (vbuf->flags & V4L2_BUF_FLAG_EOS)
			data->flags |= HAL_BUFFERFLAG_EOS;

		if (vbuf->flags & V4L2_BUF_FLAG_CODECCONFIG)
			data->flags |= HAL_BUFFERFLAG_CODECCONFIG;

		if (inst->session_type == MSM_CVP_DECODER) {
			msm_cvp_comm_fetch_mark_data(&inst->etb_data, vb->index,
				&data->mark_data, &data->mark_target);
		}

	} else if (vb->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		data->buffer_type = msm_cvp_comm_get_hal_output_buffer(inst);
	}

	extra_idx = EXTRADATA_IDX(vb->num_planes);
	if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
		data->extradata_addr = mbuf->smem[extra_idx].device_addr;
		data->extradata_size = vb->planes[extra_idx].length;
		data->flags |= HAL_BUFFERFLAG_EXTRADATA;
	}
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

static int num_pending_qbufs(struct msm_cvp_inst *inst, u32 type)
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
		/* Count only deferred buffers */
		if (!(mbuf->flags & MSM_CVP_FLAG_DEFERRED))
			continue;
		count++;
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return count;
}

static int msm_comm_qbuf_to_hfi(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	int rc = 0;
	struct hfi_device *hdev;
	enum msm_cvp_debugfs_event e = { 0 };
	struct cvp_frame_data frame_data = {0};

	if (!inst || !inst->core || !inst->core->device || !mbuf) {
		dprintk(CVP_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	populate_frame_data(&frame_data, mbuf, inst);
	/* mbuf is not deferred anymore */
	mbuf->flags &= ~MSM_CVP_FLAG_DEFERRED;
	mbuf->flags |= MSM_CVP_FLAG_QUEUED;
	msm_cvp_debugfs_update(inst, e);

//err_bad_input:
	return rc;
}

int msm_cvp_comm_qbuf(struct msm_cvp_inst *inst, struct msm_video_buffer *mbuf)
{
	int rc = 0;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR, "%s: inst is in bad state\n", __func__);
		return -EINVAL;
	}

	if (inst->state != MSM_CVP_START_DONE) {
		mbuf->flags |= MSM_CVP_FLAG_DEFERRED;
		print_video_buffer(CVP_DBG, "qbuf deferred", inst, mbuf);
		return 0;
	}

	rc = msm_cvp_comm_scale_clocks_and_bus(inst);
	if (rc)
		dprintk(CVP_ERR, "%s: scale clocks failed\n", __func__);

	print_video_buffer(CVP_DBG, "qbuf", inst, mbuf);
	rc = msm_comm_qbuf_to_hfi(inst, mbuf);
	if (rc)
		dprintk(CVP_ERR, "%s: Failed qbuf to hfi: %d\n", __func__, rc);

	return rc;
}

/*
 * msm_comm_qbuf_decode_batch - count the buffers which are not queued to
 *              firmware yet (count includes rbr pending buffers too) and
 *              queue the buffers at once if full batch count reached.
 *              Don't queue rbr pending buffers as they would be queued
 *              when rbr event arrived from firmware.
 */
int msm_cvp_comm_qbuf_decode_batch(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	int rc = 0;
	u32 count = 0;
	struct msm_video_buffer *buf;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR, "%s: inst is in bad state\n", __func__);
		return -EINVAL;
	}

	if (inst->state != MSM_CVP_START_DONE) {
		mbuf->flags |= MSM_CVP_FLAG_DEFERRED;
		print_video_buffer(CVP_DBG, "qbuf deferred", inst, mbuf);
		return 0;
	}

	/*
	 * Don't defer buffers initially to avoid startup
	 * latency increase due to batching
	 */
	if (inst->clk_data.buffer_counter > SKIP_BATCH_WINDOW) {
		count = num_pending_qbufs(inst,
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		if (count < inst->batch.size) {
			print_video_buffer(CVP_DBG,
				"batch-qbuf deferred", inst, mbuf);
			return 0;
		}
	}

	rc = msm_cvp_comm_scale_clocks_and_bus(inst);
	if (rc)
		dprintk(CVP_ERR, "%s: scale clocks failed\n", __func__);

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(buf, &inst->registeredbufs.list, list) {
		/* Don't queue if buffer is not CAPTURE_MPLANE */
		if (buf->vvb.vb2_buf.type !=
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
			goto loop_end;
		/* Don't queue if buffer is not a deferred buffer */
		if (!(buf->flags & MSM_CVP_FLAG_DEFERRED))
			goto loop_end;
		/* Don't queue if RBR event is pending on this buffer */
		if (buf->flags & MSM_CVP_FLAG_RBR_PENDING)
			goto loop_end;

		print_video_buffer(CVP_DBG, "batch-qbuf", inst, buf);
		rc = msm_comm_qbuf_to_hfi(inst, buf);
		if (rc) {
			dprintk(CVP_ERR, "%s: Failed qbuf to hfi: %d\n",
				__func__, rc);
			break;
		}
loop_end:
		/* Queue pending buffers till the current buffer only */
		if (buf == mbuf)
			break;
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return rc;
}

int msm_cvp_comm_try_get_prop(struct msm_cvp_inst *inst,
	enum hal_property ptype, union hal_get_property *hprop)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct getprop_buf *buf;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	mutex_lock(&inst->sync_lock);
	if (inst->state < MSM_CVP_OPEN_DONE ||
			inst->state >= MSM_CVP_CLOSE) {

		/* No need to check inst->state == MSM_CVP_INVALID since
		 * INVALID is > CLOSE_DONE. When core went to INVALID state,
		 * we put all the active instances in INVALID. So > CLOSE_DONE
		 * is enough check to have.
		 */

		dprintk(CVP_ERR,
			"In Wrong state to call Buf Req: Inst %pK or Core %pK\n",
				inst, inst->core);
		rc = -EAGAIN;
		mutex_unlock(&inst->sync_lock);
		goto exit;
	}
	mutex_unlock(&inst->sync_lock);

	switch (ptype) {
	case HAL_PARAM_GET_BUFFER_REQUIREMENTS:
		rc = call_hfi_op(hdev, session_get_buf_req, inst->session);
		break;
	default:
		rc = -EAGAIN;
		break;
	}

	if (rc) {
		dprintk(CVP_ERR, "Can't query hardware for property: %d\n",
				rc);
		goto exit;
	}

	rc = wait_for_completion_timeout(&inst->completions[
			SESSION_MSG_INDEX(HAL_SESSION_PROPERTY_INFO)],
		msecs_to_jiffies(
			inst->core->resources.msm_cvp_hw_rsp_timeout));
	if (!rc) {
		dprintk(CVP_ERR,
			"%s: Wait interrupted or timed out [%pK]: %d\n",
			__func__, inst,
			SESSION_MSG_INDEX(HAL_SESSION_PROPERTY_INFO));
		msm_cvp_comm_kill_session(inst);
		rc = -ETIMEDOUT;
		goto exit;
	} else {
		/* wait_for_completion_timeout returns jiffies before expiry */
		rc = 0;
	}

	mutex_lock(&inst->pending_getpropq.lock);
	if (!list_empty(&inst->pending_getpropq.list)) {
		buf = list_first_entry(&inst->pending_getpropq.list,
					struct getprop_buf, list);
		*hprop = *(union hal_get_property *)buf->data;
		kfree(buf->data);
		list_del(&buf->list);
		kfree(buf);
	} else {
		dprintk(CVP_ERR, "%s getprop list empty\n", __func__);
		rc = -EINVAL;
	}
	mutex_unlock(&inst->pending_getpropq.lock);
exit:
	return rc;
}

int msm_cvp_comm_release_output_buffers(struct msm_cvp_inst *inst,
	bool force_release)
{
	struct msm_smem *handle;
	struct internal_buf *buf, *dummy;
	struct cvp_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_cvp_core *core;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(CVP_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	mutex_lock(&inst->outputbufs.lock);
	if (list_empty(&inst->outputbufs.list)) {
		dprintk(CVP_DBG, "%s - No OUTPUT buffers allocated\n",
			__func__);
		mutex_unlock(&inst->outputbufs.lock);
		return 0;
	}
	mutex_unlock(&inst->outputbufs.lock);

	core = inst->core;
	if (!core) {
		dprintk(CVP_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(CVP_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}
	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry_safe(buf, dummy, &inst->outputbufs.list, list) {
		handle = &buf->smem;

		if ((buf->buffer_ownership == FIRMWARE) && !force_release) {
			dprintk(CVP_INFO, "DPB is with f/w. Can't free it\n");
			/*
			 * mark this buffer to avoid sending it to video h/w
			 * again, this buffer belongs to old resolution and
			 * it will be removed when video h/w returns it.
			 */
			buf->mark_remove = true;
			continue;
		}

		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		if (inst->buffer_mode_set[CAPTURE_PORT] ==
				HAL_BUFFER_MODE_STATIC) {
			buffer_info.response_required = false;
			rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
			if (rc) {
				dprintk(CVP_WARN,
					"Rel output buf fail:%x, %d\n",
					buffer_info.align_device_addr,
					buffer_info.buffer_size);
			}
		}

		list_del(&buf->list);
		msm_cvp_comm_smem_free(inst, &buf->smem);
		kfree(buf);
	}

	mutex_unlock(&inst->outputbufs.lock);
	return rc;
}

static enum hal_buffer scratch_buf_sufficient(struct msm_cvp_inst *inst,
				enum hal_buffer buffer_type)
{
	struct hal_buffer_requirements *bufreq = NULL;
	struct internal_buf *buf;
	int count = 0;

	if (!inst) {
		dprintk(CVP_ERR, "%s - invalid param\n", __func__);
		goto not_sufficient;
	}

	bufreq = get_cvp_buff_req_buffer(inst, buffer_type);
	if (!bufreq)
		goto not_sufficient;

	/* Check if current scratch buffers are sufficient */
	mutex_lock(&inst->scratchbufs.lock);

	list_for_each_entry(buf, &inst->scratchbufs.list, list) {
		if (buf->buffer_type == buffer_type &&
			buf->smem.size >= bufreq->buffer_size)
			count++;
	}
	mutex_unlock(&inst->scratchbufs.lock);

	if (count != bufreq->buffer_count_actual)
		goto not_sufficient;

	dprintk(CVP_DBG,
		"Existing scratch buffer is sufficient for buffer type %#x\n",
		buffer_type);

	return buffer_type;

not_sufficient:
	return HAL_BUFFER_NONE;
}

int msm_cvp_comm_release_scratch_buffers(struct msm_cvp_inst *inst,
					bool check_for_reuse)
{
	struct msm_smem *handle;
	struct internal_buf *buf, *dummy;
	struct cvp_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_cvp_core *core;
	struct hfi_device *hdev;
	enum hal_buffer sufficiency = HAL_BUFFER_NONE;

	if (!inst) {
		dprintk(CVP_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(CVP_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(CVP_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}

	if (check_for_reuse) {
		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH);

		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH_1);

		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH_2);
	}

	mutex_lock(&inst->scratchbufs.lock);
	list_for_each_entry_safe(buf, dummy, &inst->scratchbufs.list, list) {
		handle = &buf->smem;
		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		buffer_info.response_required = true;
		rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
		if (!rc) {
			mutex_unlock(&inst->scratchbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc)
				dprintk(CVP_WARN,
					"%s: wait for signal failed, rc %d\n",
					__func__, rc);
			mutex_lock(&inst->scratchbufs.lock);
		} else {
			dprintk(CVP_WARN,
				"Rel scrtch buf fail:%x, %d\n",
				buffer_info.align_device_addr,
				buffer_info.buffer_size);
		}

		/*If scratch buffers can be reused, do not free the buffers*/
		if (sufficiency & buf->buffer_type)
			continue;

		list_del(&buf->list);
		msm_cvp_comm_smem_free(inst, handle);
		kfree(buf);
	}

	mutex_unlock(&inst->scratchbufs.lock);
	return rc;
}

void msm_cvp_comm_release_eos_buffers(struct msm_cvp_inst *inst)
{
	struct eos_buf *buf, *next;

	if (!inst) {
		dprintk(CVP_ERR,
			"Invalid instance pointer = %pK\n", inst);
		return;
	}

	mutex_lock(&inst->eosbufs.lock);
	list_for_each_entry_safe(buf, next, &inst->eosbufs.list, list) {
		list_del(&buf->list);
		msm_cvp_comm_smem_free(inst, &buf->smem);
		kfree(buf);
	}
	INIT_LIST_HEAD(&inst->eosbufs.list);
	mutex_unlock(&inst->eosbufs.lock);
}


int msm_cvp_comm_release_recon_buffers(struct msm_cvp_inst *inst)
{
	struct recon_buf *buf, *next;

	if (!inst) {
		dprintk(CVP_ERR,
			"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}

	mutex_lock(&inst->reconbufs.lock);
	list_for_each_entry_safe(buf, next, &inst->reconbufs.list, list) {
		list_del(&buf->list);
		kfree(buf);
	}
	INIT_LIST_HEAD(&inst->reconbufs.list);
	mutex_unlock(&inst->reconbufs.lock);

	return 0;
}

int msm_cvp_comm_try_set_prop(struct msm_cvp_inst *inst,
	enum hal_property ptype, void *pdata)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(CVP_ERR, "Invalid input: %pK\n", inst);
		return -EINVAL;
	}

	if (!inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	mutex_lock(&inst->sync_lock);
	if (inst->state < MSM_CVP_OPEN_DONE || inst->state >= MSM_CVP_CLOSE) {
		dprintk(CVP_ERR, "Not in proper state to set property\n");
		rc = -EAGAIN;
		goto exit;
	}
	rc = call_hfi_op(hdev, session_set_property, (void *)inst->session,
			ptype, pdata);
	if (rc)
		dprintk(CVP_ERR, "Failed to set hal property for framesize\n");
exit:
	mutex_unlock(&inst->sync_lock);
	return rc;
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

static void msm_comm_flush_in_invalid_state(struct msm_cvp_inst *inst)
{
	struct list_head *ptr, *next;
	enum cvp_ports ports[] = {OUTPUT_PORT, CAPTURE_PORT};
	int c = 0;

	/* before flush ensure venus released all buffers */
	msm_cvp_comm_try_state(inst, MSM_CVP_RELEASE_RESOURCES_DONE);

	for (c = 0; c < ARRAY_SIZE(ports); ++c) {
		enum cvp_ports port = ports[c];

		mutex_lock(&inst->bufq[port].lock);
		list_for_each_safe(ptr, next,
				&inst->bufq[port].vb2_bufq.queued_list) {
			struct vb2_buffer *vb = container_of(ptr,
					struct vb2_buffer, queued_entry);
			if (vb->state == VB2_BUF_STATE_ACTIVE) {
				vb->planes[0].bytesused = 0;
				print_cvp_vb2_buffer(CVP_ERR,
					"flush in invalid", inst, vb);
				vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			} else {
				dprintk(CVP_WARN,
					"%s VB is in state %d not in ACTIVE state\n"
					, __func__, vb->state);
			}
		}
		mutex_unlock(&inst->bufq[port].lock);
	}
	msm_cvp_queue_v4l2_event(inst, V4L2_EVENT_MSM_CVP_FLUSH_DONE);
}

int msm_cvp_comm_flush(struct msm_cvp_inst *inst, u32 flags)
{
	int i, rc =  0;
	bool ip_flush = false;
	bool op_flush = false;
	struct msm_video_buffer *mbuf, *next;
	struct msm_cvp_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR,
				"Invalid params, inst %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	hdev = core->device;

	ip_flush = flags & V4L2_CMD_FLUSH_OUTPUT;
	op_flush = flags & V4L2_CMD_FLUSH_CAPTURE;

	if (ip_flush && !op_flush) {
		dprintk(CVP_WARN,
			"Input only flush not supported, making it flush all\n");
		op_flush = true;
		return 0;
	}

	msm_cvp_clock_data_reset(inst);

	if (inst->state == MSM_CVP_CORE_INVALID) {
		dprintk(CVP_ERR,
				"Core %pK and inst %pK are in bad state\n",
					core, inst);
		msm_comm_flush_in_invalid_state(inst);
		return 0;
	}

	mutex_lock(&inst->flush_lock);
	/* enable in flush */
	inst->in_flush = true;

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(mbuf, next, &inst->registeredbufs.list, list) {
		/* don't flush input buffers if input flush is not requested */
		if (!ip_flush && mbuf->vvb.vb2_buf.type ==
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			continue;

		/* flush only deferred or rbr pending buffers */
		if (!(mbuf->flags & MSM_CVP_FLAG_DEFERRED ||
			mbuf->flags & MSM_CVP_FLAG_RBR_PENDING))
			continue;

		/*
		 * flush buffers which are queued by client already,
		 * the refcount will be two or more for those buffers.
		 */
		if (!(mbuf->smem[0].refcount >= 2))
			continue;

		print_video_buffer(CVP_DBG, "flush buf", inst, mbuf);
		msm_cvp_comm_flush_video_buffer(inst, mbuf);

		for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
			if (msm_cvp_smem_unmap_dma_buf(inst, &mbuf->smem[i]))
				print_video_buffer(CVP_ERR,
					"dqbuf: unmap failed.", inst, mbuf);
			if (msm_cvp_smem_unmap_dma_buf(inst, &mbuf->smem[i]))
				print_video_buffer(CVP_ERR,
					"dqbuf: unmap failed..", inst, mbuf);
		}
		if (!mbuf->smem[0].refcount) {
			list_del(&mbuf->list);
			kref_cvp_put_mbuf(mbuf);
		} else {
			/* buffer is no more a deferred buffer */
			mbuf->flags &= ~MSM_CVP_FLAG_DEFERRED;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);

	hdev = inst->core->device;
	if (ip_flush) {
		dprintk(CVP_DBG, "Send flush on all ports to firmware\n");
		rc = call_hfi_op(hdev, session_flush, inst->session,
			HAL_FLUSH_ALL);
	} else {
		dprintk(CVP_DBG, "Send flush on output port to firmware\n");
		rc = call_hfi_op(hdev, session_flush, inst->session,
			HAL_FLUSH_OUTPUT);
	}
	mutex_unlock(&inst->flush_lock);
	if (rc) {
		dprintk(CVP_ERR,
			"Sending flush to firmware failed, flush out all buffers\n");
		msm_comm_flush_in_invalid_state(inst);
		/* disable in_flush */
		inst->in_flush = false;
	}

	return rc;
}

enum hal_extradata_id msm_cvp_comm_get_hal_extradata_index(
	enum v4l2_mpeg_cvp_extradata index)
{
	int ret = 0;

	switch (index) {
	case V4L2_MPEG_CVP_EXTRADATA_NONE:
		ret = HAL_EXTRADATA_NONE;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_INTERLACE_VIDEO:
		ret = HAL_EXTRADATA_INTERLACE_VIDEO;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_TIMESTAMP:
		ret = HAL_EXTRADATA_TIMESTAMP;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_S3D_FRAME_PACKING:
		ret = HAL_EXTRADATA_S3D_FRAME_PACKING;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_FRAME_RATE:
		ret = HAL_EXTRADATA_FRAME_RATE;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_PANSCAN_WINDOW:
		ret = HAL_EXTRADATA_PANSCAN_WINDOW;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_RECOVERY_POINT_SEI:
		ret = HAL_EXTRADATA_RECOVERY_POINT_SEI;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_NUM_CONCEALED_MB:
		ret = HAL_EXTRADATA_NUM_CONCEALED_MB;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_ASPECT_RATIO:
		ret = HAL_EXTRADATA_ASPECT_RATIO;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_MPEG2_SEQDISP:
		ret = HAL_EXTRADATA_MPEG2_SEQDISP;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_STREAM_USERDATA:
		ret = HAL_EXTRADATA_STREAM_USERDATA;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_FRAME_QP:
		ret = HAL_EXTRADATA_FRAME_QP;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_LTR:
		ret = HAL_EXTRADATA_LTR_INFO;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_ROI_QP:
		ret = HAL_EXTRADATA_ROI_QP;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_OUTPUT_CROP:
		ret = HAL_EXTRADATA_OUTPUT_CROP;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_DISPLAY_COLOUR_SEI:
		ret = HAL_EXTRADATA_MASTERING_DISPLAY_COLOUR_SEI;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI:
		ret = HAL_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_VUI_DISPLAY:
		ret = HAL_EXTRADATA_VUI_DISPLAY_INFO;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_VPX_COLORSPACE:
		ret = HAL_EXTRADATA_VPX_COLORSPACE;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_UBWC_CR_STATS_INFO:
		ret = HAL_EXTRADATA_UBWC_CR_STATS_INFO;
		break;
	case V4L2_MPEG_CVP_EXTRADATA_HDR10PLUS_METADATA:
		ret = HAL_EXTRADATA_HDR10PLUS_METADATA;
		break;
	default:
		dprintk(CVP_WARN, "Extradata not found: %d\n", index);
		break;
	}
	return ret;
};

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

static int msm_cvp_load_supported(struct msm_cvp_inst *inst)
{
	int num_mbs_per_sec = 0, max_load_adj = 0;
	enum load_calc_quirks quirks = LOAD_CALC_IGNORE_TURBO_LOAD |
		LOAD_CALC_IGNORE_THUMBNAIL_LOAD |
		LOAD_CALC_IGNORE_NON_REALTIME_LOAD;

	if (inst->state == MSM_CVP_OPEN_DONE) {
		max_load_adj = inst->core->resources.max_load;
		num_mbs_per_sec = msm_cvp_comm_get_load(inst->core,
					MSM_CVP_DECODER, quirks);
		num_mbs_per_sec += msm_cvp_comm_get_load(inst->core,
					MSM_CVP_ENCODER, quirks);
		if (num_mbs_per_sec > max_load_adj) {
			dprintk(CVP_ERR,
				"H/W is overloaded. needed: %d max: %d\n",
				num_mbs_per_sec,
				max_load_adj);
			msm_cvp_print_running_insts(inst->core);
			return -EBUSY;
		}
	}
	return 0;
}

int msm_cvp_check_scaling_supported(struct msm_cvp_inst *inst)
{
	u32 x_min, x_max, y_min, y_max;
	u32 input_height, input_width, output_height, output_width;

	if (inst->grid_enable > 0) {
		dprintk(CVP_DBG, "Skip scaling check for HEIC\n");
		return 0;
	}

	input_height = inst->prop.height[OUTPUT_PORT];
	input_width = inst->prop.width[OUTPUT_PORT];
	output_height = inst->prop.height[CAPTURE_PORT];
	output_width = inst->prop.width[CAPTURE_PORT];

	if (!input_height || !input_width || !output_height || !output_width) {
		dprintk(CVP_ERR,
			"Invalid : Input height = %d width = %d",
			input_height, input_width);
		dprintk(CVP_ERR,
			" output height = %d width = %d\n",
			output_height, output_width);
		return -ENOTSUPP;
	}

	if (!inst->capability.scale_x.min ||
		!inst->capability.scale_x.max ||
		!inst->capability.scale_y.min ||
		!inst->capability.scale_y.max) {

		if (input_width * input_height !=
			output_width * output_height) {
			dprintk(CVP_ERR,
				"%s: scaling is not supported (%dx%d != %dx%d)\n",
				__func__, input_width, input_height,
				output_width, output_height);
			return -ENOTSUPP;
		}

		dprintk(CVP_DBG, "%s: supported WxH = %dx%d\n",
			__func__, input_width, input_height);
		return 0;
	}

	x_min = (1<<16)/inst->capability.scale_x.min;
	y_min = (1<<16)/inst->capability.scale_y.min;
	x_max = inst->capability.scale_x.max >> 16;
	y_max = inst->capability.scale_y.max >> 16;

	if (input_height > output_height) {
		if (input_height > x_min * output_height) {
			dprintk(CVP_ERR,
				"Unsupported height min height %d vs %d\n",
				input_height / x_min, output_height);
			return -ENOTSUPP;
		}
	} else {
		if (output_height > x_max * input_height) {
			dprintk(CVP_ERR,
				"Unsupported height max height %d vs %d\n",
				x_max * input_height, output_height);
			return -ENOTSUPP;
		}
	}
	if (input_width > output_width) {
		if (input_width > y_min * output_width) {
			dprintk(CVP_ERR,
				"Unsupported width min width %d vs %d\n",
				input_width / y_min, output_width);
			return -ENOTSUPP;
		}
	} else {
		if (output_width > y_max * input_width) {
			dprintk(CVP_ERR,
				"Unsupported width max width %d vs %d\n",
				y_max * input_width, output_width);
			return -ENOTSUPP;
		}
	}
	return 0;
}

int msm_cvp_check_session_supported(struct msm_cvp_inst *inst)
{
	struct msm_cvp_capability *capability;
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_cvp_core *core;
	u32 output_height, output_width, input_height, input_width;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_WARN, "%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}
	capability = &inst->capability;
	hdev = inst->core->device;
	core = inst->core;
	rc = msm_cvp_load_supported(inst);
	if (rc) {
		dprintk(CVP_WARN,
			"%s: Hardware is overloaded\n", __func__);
		return rc;
	}

	if (!is_thermal_permissible(core)) {
		dprintk(CVP_WARN,
			"Thermal level critical, stop all active sessions!\n");
		return -ENOTSUPP;
	}

	output_height = inst->prop.height[CAPTURE_PORT];
	output_width = inst->prop.width[CAPTURE_PORT];
	input_height = inst->prop.height[OUTPUT_PORT];
	input_width = inst->prop.width[OUTPUT_PORT];

	if (inst->session_type == MSM_CVP_ENCODER && (input_width % 2 != 0 ||
			input_height % 2 != 0 || output_width % 2 != 0 ||
			output_height % 2 != 0)) {
		dprintk(CVP_ERR,
			"Height and Width should be even numbers for NV12\n");
		dprintk(CVP_ERR,
			"Input WxH = (%u)x(%u), Output WxH = (%u)x(%u)\n",
			input_width, input_height,
			output_width, output_height);
		rc = -ENOTSUPP;
	}

	output_height = ALIGN(inst->prop.height[CAPTURE_PORT], 16);
	output_width = ALIGN(inst->prop.width[CAPTURE_PORT], 16);

	if (!rc) {
		if (output_width < capability->width.min ||
			output_height < capability->height.min) {
			dprintk(CVP_ERR,
				"Unsupported WxH = (%u)x(%u), min supported is - (%u)x(%u)\n",
				output_width,
				output_height,
				capability->width.min,
				capability->height.min);
			rc = -ENOTSUPP;
		}
		if (!rc && output_width > capability->width.max) {
			dprintk(CVP_ERR,
				"Unsupported width = %u supported max width = %u\n",
				output_width,
				capability->width.max);
				rc = -ENOTSUPP;
		}

		if (!rc && output_height * output_width >
			capability->width.max * capability->height.max) {
			dprintk(CVP_ERR,
			"Unsupported WxH = (%u)x(%u), max supported is - (%u)x(%u)\n",
			output_width, output_height,
			capability->width.max, capability->height.max);
			rc = -ENOTSUPP;
		}
	}
	if (rc) {
		dprintk(CVP_ERR,
			"%s: Resolution unsupported\n", __func__);
	}
	return rc;
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

int msm_cvp_comm_set_color_format(struct msm_cvp_inst *inst,
		enum hal_buffer buffer_type, int fourcc)
{
	struct hal_uncompressed_format_select hal_fmt = {0};
	enum hal_uncompressed_format format = HAL_UNUSED_COLOR;
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s - invalid param\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	format = msm_cvp_comm_get_hal_uncompressed(fourcc);
	if (format == HAL_UNUSED_COLOR) {
		dprintk(CVP_ERR, "Using unsupported colorformat %#x\n",
				fourcc);
		rc = -ENOTSUPP;
		goto exit;
	}

	hal_fmt.buffer_type = buffer_type;
	hal_fmt.format = format;

	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT, &hal_fmt);
	if (rc)
		dprintk(CVP_ERR,
			"Failed to set input color format\n");
	else
		dprintk(CVP_DBG, "Setting uncompressed colorformat to %#x\n",
				format);

exit:
	return rc;
}

int msm_cvp_comm_s_parm(struct msm_cvp_inst *inst, struct v4l2_streamparm *a)
{
	u32 property_id = 0;
	u64 us_per_frame = 0;
	void *pdata;
	int rc = 0, fps = 0;
	struct hal_frame_rate frame_rate;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device || !a) {
		dprintk(CVP_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	property_id = HAL_CONFIG_FRAME_RATE;

	if (a->parm.output.timeperframe.denominator) {
		switch (a->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			us_per_frame = a->parm.output.timeperframe.numerator *
				(u64)USEC_PER_SEC;
			do_div(us_per_frame,
				a->parm.output.timeperframe.denominator);
			break;
		default:
			dprintk(CVP_ERR,
					"Scale clocks : Unknown buffer type %d\n",
					a->type);
			break;
		}
	}

	if (!us_per_frame) {
		dprintk(CVP_ERR,
				"Failed to scale clocks : time between frames is 0\n");
		rc = -EINVAL;
		goto exit;
	}

	fps = us_per_frame > USEC_PER_SEC ?
		0 : USEC_PER_SEC / (u32)us_per_frame;

	if (fps % 15 == 14 || fps % 24 == 23)
		fps = fps + 1;
	else if ((fps > 1) && (fps % 24 == 1 || fps % 15 == 1))
		fps = fps - 1;

	if (fps < inst->capability.frame_rate.min ||
			fps > inst->capability.frame_rate.max) {
		dprintk(CVP_ERR,
			"FPS is out of limits : fps = %d Min = %d, Max = %d\n",
			fps, inst->capability.frame_rate.min,
			inst->capability.frame_rate.max);
		rc = -EINVAL;
		goto exit;
	}

	dprintk(CVP_PROF, "reported fps changed for %pK: %d->%d\n",
			inst, inst->prop.fps, fps);
	inst->prop.fps = fps;
	if (inst->session_type == MSM_CVP_ENCODER &&
		get_cvp_hal_codec(inst->fmts[CAPTURE_PORT].fourcc) !=
			HAL_VIDEO_CODEC_TME) {
		frame_rate.frame_rate = inst->prop.fps * BIT(16);
		frame_rate.buffer_type = HAL_BUFFER_OUTPUT;
		pdata = &frame_rate;
		rc = call_hfi_op(hdev, session_set_property,
			inst->session, property_id, pdata);
		if (rc)
			dprintk(CVP_WARN,
				"Failed to set frame rate %d\n", rc);
	}
exit:
	return rc;
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
			"%s session, %s, Codec type: %s HxW: %d x %d fps: %d bitrate: %d bit-depth: %s\n",
			is_decode ? "Decode" : "Encode",
			is_secure ? "Secure" : "Non-Secure",
			inst->fmts[port].name,
			inst->prop.height[port], inst->prop.width[port],
			inst->prop.fps, inst->prop.bitrate,
			!inst->bit_depth ? "8" : "10");

	dprintk(CVP_ERR,
			"---Buffer details for inst: %pK of type: %d---\n",
			inst, inst->session_type);
	mutex_lock(&inst->registeredbufs.lock);
	dprintk(CVP_ERR, "registered buffer list:\n");
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list)
		print_video_buffer(CVP_ERR, "buf", inst, mbuf);
	mutex_unlock(&inst->registeredbufs.lock);

	mutex_lock(&inst->scratchbufs.lock);
	dprintk(CVP_ERR, "scratch buffer list:\n");
	list_for_each_entry(buf, &inst->scratchbufs.list, list)
		dprintk(CVP_ERR, "type: %d addr: %x size: %u\n",
				buf->buffer_type, buf->smem.device_addr,
				buf->smem.size);
	mutex_unlock(&inst->scratchbufs.lock);

	mutex_lock(&inst->persistbufs.lock);
	dprintk(CVP_ERR, "persist buffer list:\n");
	list_for_each_entry(buf, &inst->persistbufs.list, list)
		dprintk(CVP_ERR, "type: %d addr: %x size: %u\n",
				buf->buffer_type, buf->smem.device_addr,
				buf->smem.size);
	mutex_unlock(&inst->persistbufs.lock);

	mutex_lock(&inst->outputbufs.lock);
	dprintk(CVP_ERR, "dpb buffer list:\n");
	list_for_each_entry(buf, &inst->outputbufs.list, list)
		dprintk(CVP_ERR, "type: %d addr: %x size: %u\n",
				buf->buffer_type, buf->smem.device_addr,
				buf->smem.size);
	mutex_unlock(&inst->outputbufs.lock);
}

int msm_cvp_comm_session_continue(void *instance)
{
	struct msm_cvp_inst *inst = instance;
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;
	hdev = inst->core->device;
	mutex_lock(&inst->lock);
	if (inst->state >= MSM_CVP_RELEASE_RESOURCES_DONE ||
			inst->state < MSM_CVP_START_DONE) {
		dprintk(CVP_DBG,
			"Inst %pK : Not in valid state to call %s\n",
				inst, __func__);
		goto sess_continue_fail;
	}
	dprintk(CVP_ERR,
				"session_continue called in wrong state for decoder");

sess_continue_fail:
	mutex_unlock(&inst->lock);
	return rc;
}

u32 cvp_get_frame_size_nv12(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12, width, height);
}

u32 cvp_get_frame_size_nv12_ubwc(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12_UBWC, width, height);
}

u32 cvp_get_frame_size_rgba(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_RGBA8888, width, height);
}

u32 cvp_get_frame_size_nv21(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV21, width, height);
}

u32 cvp_get_frame_size_tp10_ubwc(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12_BPP10_UBWC, width, height);
}

u32 cvp_get_frame_size_p010(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_P010, width, height);
}

u32 cvp_get_frame_size_nv12_512(int plane, u32 height, u32 width)
{
	return VENUS_BUFFER_SIZE(COLOR_FMT_NV12_512, width, height);
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

void print_cvp_vb2_buffer(u32 tag, const char *str, struct msm_cvp_inst *inst,
		struct vb2_buffer *vb2)
{
	if (!(tag & msm_cvp_debug) || !inst || !vb2)
		return;

	if (vb2->num_planes == 1)
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d\n",
			str, vb2->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, vb2->planes[0].length,
			vb2->planes[0].bytesused);
	else
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d, extradata: fd %d off %d size %d filled %d\n",
			str, vb2->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, vb2->planes[0].length,
			vb2->planes[0].bytesused, vb2->planes[1].m.fd,
			vb2->planes[1].data_offset, vb2->planes[1].length,
			vb2->planes[1].bytesused);
}

void print_cvp_v4l2_buffer(u32 tag, const char *str, struct msm_cvp_inst *inst,
		struct v4l2_buffer *v4l2)
{
	if (!(tag & msm_cvp_debug) || !inst || !v4l2)
		return;

	if (v4l2->length == 1)
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d\n",
			str, v4l2->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			v4l2->index, v4l2->m.planes[0].m.fd,
			v4l2->m.planes[0].data_offset,
			v4l2->m.planes[0].length,
			v4l2->m.planes[0].bytesused);
	else
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d, extradata: fd %d off %d size %d filled %d\n",
			str, v4l2->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			v4l2->index, v4l2->m.planes[0].m.fd,
			v4l2->m.planes[0].data_offset,
			v4l2->m.planes[0].length,
			v4l2->m.planes[0].bytesused,
			v4l2->m.planes[1].m.fd,
			v4l2->m.planes[1].data_offset,
			v4l2->m.planes[1].length,
			v4l2->m.planes[1].bytesused);
}

bool msm_cvp_comm_compare_vb2_plane(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf, struct vb2_buffer *vb2, u32 i)
{
	struct vb2_buffer *vb;

	if (!inst || !mbuf || !vb2) {
		dprintk(CVP_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, vb2);
		return false;
	}

	vb = &mbuf->vvb.vb2_buf;
	if (vb->planes[i].m.fd == vb2->planes[i].m.fd &&
		vb->planes[i].length == vb2->planes[i].length) {
		return true;
	}

	return false;
}

bool msm_cvp_comm_compare_vb2_planes(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf, struct vb2_buffer *vb2)
{
	int i = 0;
	struct vb2_buffer *vb;

	if (!inst || !mbuf || !vb2) {
		dprintk(CVP_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, vb2);
		return false;
	}

	vb = &mbuf->vvb.vb2_buf;

	if (vb->num_planes != vb2->num_planes)
		return false;

	for (i = 0; i < vb->num_planes; i++) {
		if (!msm_cvp_comm_compare_vb2_plane(inst, mbuf, vb2, i))
			return false;
	}

	return true;
}

bool msm_cvp_comm_compare_dma_plane(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf, unsigned long *dma_planes, u32 i)
{
	if (!inst || !mbuf || !dma_planes) {
		dprintk(CVP_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, dma_planes);
		return false;
	}

	if ((unsigned long)mbuf->smem[i].dma_buf == dma_planes[i])
		return true;

	return false;
}

bool msm_cvp_comm_compare_dma_planes(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf, unsigned long *dma_planes)
{
	int i = 0;
	struct vb2_buffer *vb;

	if (!inst || !mbuf || !dma_planes) {
		dprintk(CVP_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, dma_planes);
		return false;
	}

	vb = &mbuf->vvb.vb2_buf;
	for (i = 0; i < vb->num_planes; i++) {
		if (!msm_cvp_comm_compare_dma_plane(inst, mbuf, dma_planes, i))
			return false;
	}

	return true;
}


bool msm_cvp_comm_compare_device_plane(struct msm_video_buffer *mbuf,
		u32 type, u32 *planes, u32 i)
{
	if (!mbuf || !planes) {
		dprintk(CVP_ERR, "%s: invalid params, %pK %pK\n",
			__func__, mbuf, planes);
		return false;
	}

	if (mbuf->vvb.vb2_buf.type == type &&
		mbuf->smem[i].device_addr == planes[i])
		return true;

	return false;
}

bool msm_cvp_comm_compare_device_planes(struct msm_video_buffer *mbuf,
		u32 type, u32 *planes)
{
	int i = 0;

	if (!mbuf || !planes)
		return false;

	for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
		if (!msm_cvp_comm_compare_device_plane(mbuf, type, planes, i))
			return false;
	}

	return true;
}

struct msm_video_buffer *msm_cvp_comm_get_buffer_using_device_planes(
		struct msm_cvp_inst *inst, u32 type, u32 *planes)
{
	struct msm_video_buffer *mbuf;
	bool found = false;

	mutex_lock(&inst->registeredbufs.lock);
	found = false;
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
		if (msm_cvp_comm_compare_device_planes(mbuf, type, planes)) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);
	if (!found) {
		dprintk(CVP_ERR,
			"%s: data_addr %x, extradata_addr %x not found\n",
			__func__, planes[0], planes[1]);
		mbuf = NULL;
	}

	return mbuf;
}

int msm_cvp_comm_flush_video_buffer(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	struct vb2_buffer *vb;
	u32 port;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}

	vb = msm_cvp_comm_get_vb_using_video_buffer(inst, mbuf);
	if (!vb) {
		print_video_buffer(CVP_ERR,
			"vb not found for buf", inst, mbuf);
		return -EINVAL;
	}

	if (mbuf->vvb.vb2_buf.type ==
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		port = CAPTURE_PORT;
	else if (mbuf->vvb.vb2_buf.type ==
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		port = OUTPUT_PORT;
	else
		return -EINVAL;

	mutex_lock(&inst->bufq[port].lock);
	if (inst->bufq[port].vb2_bufq.streaming) {
		vb->planes[0].bytesused = 0;
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	} else {
		dprintk(CVP_ERR, "%s: port %d is not streaming\n",
			__func__, port);
	}
	mutex_unlock(&inst->bufq[port].lock);

	return 0;
}

int msm_cvp_comm_qbuf_cache_operations(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	int rc = 0, i;
	struct vb2_buffer *vb;
	bool skip;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}
	vb = &mbuf->vvb.vb2_buf;

	for (i = 0; i < vb->num_planes; i++) {
		unsigned long offset, size;
		enum smem_cache_ops cache_op;

		skip = true;
		if (inst->session_type == MSM_CVP_DECODER) {
			if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
				if (!i) { /* bitstream */
					skip = false;
					offset = vb->planes[i].data_offset;
					size = vb->planes[i].bytesused;
					cache_op = SMEM_CACHE_CLEAN_INVALIDATE;
				}
			} else if (vb->type ==
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
				if (!i) { /* yuv */
					skip = false;
					offset = 0;
					size = vb->planes[i].length;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		} else if (inst->session_type == MSM_CVP_ENCODER) {
			if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
				if (!i) { /* yuv */
					skip = false;
					offset = vb->planes[i].data_offset;
					size = vb->planes[i].bytesused;
					cache_op = SMEM_CACHE_CLEAN_INVALIDATE;
				}
			} else if (vb->type ==
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
				if (!i) { /* bitstream */
					skip = false;
					offset = 0;
					size = vb->planes[i].length;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		}

		if (!skip) {
			rc = msm_cvp_smem_cache_operations(
					mbuf->smem[i].dma_buf,
					cache_op, offset, size);
			if (rc)
				print_video_buffer(CVP_ERR,
					"qbuf cache ops failed", inst, mbuf);
		}
	}

	return rc;
}

int msm_cvp_comm_dqbuf_cache_operations(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	int rc = 0, i;
	struct vb2_buffer *vb;
	bool skip;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}
	vb = &mbuf->vvb.vb2_buf;

	for (i = 0; i < vb->num_planes; i++) {
		unsigned long offset, size;
		enum smem_cache_ops cache_op;

		skip = true;
		if (inst->session_type == MSM_CVP_DECODER) {
			if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
				/* bitstream and extradata */
				/* we do not need cache operations */
			} else if (vb->type ==
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
				if (!i) { /* yuv */
					skip = false;
					offset = vb->planes[i].data_offset;
					size = vb->planes[i].bytesused;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		} else if (inst->session_type == MSM_CVP_ENCODER) {
			if (vb->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
				/* yuv and extradata */
				/* we do not need cache operations */
			} else if (vb->type ==
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
				if (!i) { /* bitstream */
					skip = false;
					/*
					 * Include vp8e header bytes as well
					 * by making offset equal to zero
					 */
					offset = 0;
					size = vb->planes[i].bytesused +
						vb->planes[i].data_offset;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		}

		if (!skip) {
			rc = msm_cvp_smem_cache_operations(
					mbuf->smem[i].dma_buf,
					cache_op, offset, size);
			if (rc)
				print_video_buffer(CVP_ERR,
					"dqbuf cache ops failed", inst, mbuf);
		}
	}

	return rc;
}

struct msm_video_buffer *msm_cvp_comm_get_video_buffer(
		struct msm_cvp_inst *inst,
		struct vb2_buffer *vb2)
{
	int rc = 0;
	struct vb2_v4l2_buffer *vbuf;
	struct vb2_buffer *vb;
	unsigned long dma_planes[VB2_MAX_PLANES] = {0};
	struct msm_video_buffer *mbuf;
	bool found = false;
	int i;

	if (!inst || !vb2) {
		dprintk(CVP_ERR, "%s: invalid params\n", __func__);
		return NULL;
	}

	for (i = 0; i < vb2->num_planes; i++) {
		/*
		 * always compare dma_buf addresses which is guaranteed
		 * to be same across the processes (duplicate fds).
		 */
		dma_planes[i] = (unsigned long)msm_cvp_smem_get_dma_buf(
				vb2->planes[i].m.fd);
		if (!dma_planes[i])
			return NULL;
		msm_cvp_smem_put_dma_buf((struct dma_buf *)dma_planes[i]);
	}

	mutex_lock(&inst->registeredbufs.lock);
	/*
	 * for encoder input, client may queue the same buffer with different
	 * fd before driver returned old buffer to the client. This buffer
	 * should be treated as new buffer Search the list with fd so that
	 * it will be treated as new msm_video_buffer.
	 */
	if (is_encode_session(inst) && vb2->type ==
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
			if (msm_cvp_comm_compare_vb2_planes(inst, mbuf, vb2)) {
				found = true;
				break;
			}
		}
	} else {
		list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
			if (msm_cvp_comm_compare_dma_planes(inst, mbuf,
					dma_planes)) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		/* this is new vb2_buffer */
		mbuf = kzalloc(sizeof(struct msm_video_buffer), GFP_KERNEL);
		if (!mbuf) {
			dprintk(CVP_ERR, "%s: alloc msm_video_buffer failed\n",
				__func__);
			rc = -ENOMEM;
			goto exit;
		}
		kref_init(&mbuf->kref);
	}

	/* Initially assume all the buffer are going to be deferred */
	mbuf->flags |= MSM_CVP_FLAG_DEFERRED;

	vbuf = to_vb2_v4l2_buffer(vb2);
	memcpy(&mbuf->vvb, vbuf, sizeof(struct vb2_v4l2_buffer));
	vb = &mbuf->vvb.vb2_buf;

	for (i = 0; i < vb->num_planes; i++) {
		mbuf->smem[i].buffer_type =
			cvp_get_hal_buffer_type(vb->type, i);
		mbuf->smem[i].fd = vb->planes[i].m.fd;
		mbuf->smem[i].offset = vb->planes[i].data_offset;
		mbuf->smem[i].size = vb->planes[i].length;
		rc = msm_cvp_smem_map_dma_buf(inst, &mbuf->smem[i]);
		if (rc) {
			dprintk(CVP_ERR, "%s: map failed.\n", __func__);
			goto exit;
		}
		/* increase refcount as we get both fbd and rbr */
		rc = msm_cvp_smem_map_dma_buf(inst, &mbuf->smem[i]);
		if (rc) {
			dprintk(CVP_ERR, "%s: map failed..\n", __func__);
			goto exit;
		}
	}
	/* dma cache operations need to be performed after dma_map */
	msm_cvp_comm_qbuf_cache_operations(inst, mbuf);

	/* add the new buffer to list */
	if (!found)
		list_add_tail(&mbuf->list, &inst->registeredbufs.list);

	mutex_unlock(&inst->registeredbufs.lock);

	/*
	 * Return mbuf if decode batching is enabled as this buffer
	 * may trigger queuing full batch to firmware, also this buffer
	 * will not be queued to firmware while full batch queuing,
	 * it will be queued when rbr event arrived from firmware.
	 */
	if (rc == -EEXIST && !inst->batch.enable)
		return ERR_PTR(rc);

	return mbuf;

exit:
	dprintk(CVP_ERR, "%s: rc %d\n", __func__, rc);
	msm_cvp_comm_unmap_video_buffer(inst, mbuf);
	if (!found)
		kref_cvp_put_mbuf(mbuf);
	mutex_unlock(&inst->registeredbufs.lock);

	return ERR_PTR(rc);
}

void msm_cvp_comm_put_video_buffer(struct msm_cvp_inst *inst,
		struct msm_video_buffer *mbuf)
{
	struct msm_video_buffer *temp;
	bool found = false;
	int i = 0;

	if (!inst || !mbuf) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return;
	}

	mutex_lock(&inst->registeredbufs.lock);
	/* check if mbuf was not removed by any chance */
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		if (msm_cvp_comm_compare_vb2_planes(inst, mbuf,
				&temp->vvb.vb2_buf)) {
			found = true;
			break;
		}
	}
	if (!found) {
		print_video_buffer(CVP_ERR, "buf was removed", inst, mbuf);
		goto unlock;
	}

	print_video_buffer(CVP_DBG, "dqbuf", inst, mbuf);
	for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
		if (msm_cvp_smem_unmap_dma_buf(inst, &mbuf->smem[i]))
			print_video_buffer(CVP_ERR,
				"dqbuf: unmap failed.", inst, mbuf);

		if (!(mbuf->vvb.flags & V4L2_BUF_FLAG_READONLY)) {
			/* rbr won't come for this buffer */
			if (msm_cvp_smem_unmap_dma_buf(inst, &mbuf->smem[i]))
				print_video_buffer(CVP_ERR,
					"dqbuf: unmap failed..", inst, mbuf);
		} else {
			/* RBR event expected */
			mbuf->flags |= MSM_CVP_FLAG_RBR_PENDING;
		}
	}
	/*
	 * remove the entry if plane[0].refcount is zero else
	 * don't remove as client queued same buffer that's why
	 * plane[0].refcount is not zero
	 */
	if (!mbuf->smem[0].refcount) {
		list_del(&mbuf->list);
		kref_cvp_put_mbuf(mbuf);
	}
unlock:
	mutex_unlock(&inst->registeredbufs.lock);
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

void msm_cvp_comm_store_mark_data(struct msm_cvp_list *data_list,
		u32 index, u32 mark_data, u32 mark_target)
{
	struct msm_cvp_buf_data *pdata = NULL;
	bool found = false;

	if (!data_list) {
		dprintk(CVP_ERR, "%s: invalid params %pK\n",
			__func__, data_list);
		return;
	}

	mutex_lock(&data_list->lock);
	list_for_each_entry(pdata, &data_list->list, list) {
		if (pdata->index == index) {
			pdata->mark_data = mark_data;
			pdata->mark_target = mark_target;
			found = true;
			break;
		}
	}

	if (!found) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata)  {
			dprintk(CVP_WARN, "%s: malloc failure.\n", __func__);
			goto exit;
		}
		pdata->index = index;
		pdata->mark_data = mark_data;
		pdata->mark_target = mark_target;
		list_add_tail(&pdata->list, &data_list->list);
	}

exit:
	mutex_unlock(&data_list->lock);
}

void msm_cvp_comm_fetch_mark_data(struct msm_cvp_list *data_list,
		u32 index, u32 *mark_data, u32 *mark_target)
{
	struct msm_cvp_buf_data *pdata = NULL;

	if (!data_list || !mark_data || !mark_target) {
		dprintk(CVP_ERR, "%s: invalid params %pK %pK %pK\n",
			__func__, data_list, mark_data, mark_target);
		return;
	}

	*mark_data = *mark_target = 0;
	mutex_lock(&data_list->lock);
	list_for_each_entry(pdata, &data_list->list, list) {
		if (pdata->index == index) {
			*mark_data = pdata->mark_data;
			*mark_target = pdata->mark_target;
			/* clear after fetch */
			pdata->mark_data = pdata->mark_target = 0;
			break;
		}
	}
	mutex_unlock(&data_list->lock);
}

int msm_cvp_comm_release_mark_data(struct msm_cvp_inst *inst)
{
	struct msm_cvp_buf_data *pdata, *next;

	if (!inst) {
		dprintk(CVP_ERR, "%s: invalid params %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	mutex_lock(&inst->etb_data.lock);
	list_for_each_entry_safe(pdata, next, &inst->etb_data.list, list) {
		list_del(&pdata->list);
		kfree(pdata);
	}
	mutex_unlock(&inst->etb_data.lock);

	mutex_lock(&inst->fbd_data.lock);
	list_for_each_entry_safe(pdata, next, &inst->fbd_data.list, list) {
		list_del(&pdata->list);
		kfree(pdata);
	}
	mutex_unlock(&inst->fbd_data.lock);

	return 0;
}

int msm_cvp_comm_set_color_format_constraints(struct msm_cvp_inst *inst,
		enum hal_buffer buffer_type,
		struct msm_cvp_format_constraint *pix_constraint)
{
	struct hal_uncompressed_plane_actual_constraints_info
		*pconstraint = NULL;
	u32 num_planes = 2;
	u32 size = 0;
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(CVP_ERR, "%s - invalid param\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	size = sizeof(buffer_type)
			+ sizeof(u32)
			+ num_planes
			* sizeof(struct hal_uncompressed_plane_constraints);

	pconstraint = kzalloc(size, GFP_KERNEL);
	if (!pconstraint) {
		dprintk(CVP_ERR, "No memory cannot alloc constrain\n");
		rc = -ENOMEM;
		goto exit;
	}

	pconstraint->buffer_type = buffer_type;
	pconstraint->num_planes = pix_constraint->num_planes;
	//set Y plan constraints
	dprintk(CVP_INFO, "Set Y plan constraints.\n");
	pconstraint->rg_plane_format[0].stride_multiples =
			pix_constraint->y_stride_multiples;
	pconstraint->rg_plane_format[0].max_stride =
			pix_constraint->y_max_stride;
	pconstraint->rg_plane_format[0].min_plane_buffer_height_multiple =
			pix_constraint->y_min_plane_buffer_height_multiple;
	pconstraint->rg_plane_format[0].buffer_alignment =
			pix_constraint->y_buffer_alignment;

	//set UV plan constraints
	dprintk(CVP_INFO, "Set UV plan constraints.\n");
	pconstraint->rg_plane_format[1].stride_multiples =
			pix_constraint->uv_stride_multiples;
	pconstraint->rg_plane_format[1].max_stride =
			pix_constraint->uv_max_stride;
	pconstraint->rg_plane_format[1].min_plane_buffer_height_multiple =
			pix_constraint->uv_min_plane_buffer_height_multiple;
	pconstraint->rg_plane_format[1].buffer_alignment =
			pix_constraint->uv_buffer_alignment;

	rc = call_hfi_op(hdev,
			session_set_property,
			inst->session,
			HAL_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO,
			pconstraint);
	if (rc)
		dprintk(CVP_ERR,
			"Failed to set input color format constraint\n");
	else
		dprintk(CVP_DBG, "Set color format constraint success\n");

exit:
	if (!pconstraint)
		kfree(pconstraint);
	return rc;
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
int cvp_comm_set_persist_buffers(struct msm_cvp_inst *inst)
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
