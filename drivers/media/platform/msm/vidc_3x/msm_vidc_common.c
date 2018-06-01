/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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
#include <linux/bitops.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/boot_stats.h>
#include <asm/div64.h>
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_dcvs.h"

#define IS_ALREADY_IN_STATE(__p, __d) ({\
	int __rc = (__p >= __d);\
	__rc; \
})

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

#define MAX_SUPPORTED_INSTANCES 16

const char *const mpeg_video_vidc_extradata[] = {
	"Extradata none",
	"Extradata MB Quantization",
	"Extradata Interlace Video",
	"Extradata VC1 Framedisp",
	"Extradata VC1 Seqdisp",
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
	"Extradata YUV Stats",
	"Extradata ROI QP",
	"Extradata output crop",
	"Extradata display colour SEI",
	"Extradata light level SEI",
	"Extradata PQ Info",
	"Extradata display VUI",
	"Extradata vpx color space",
	"Extradata UBWC CR stats info",
	"Extradata enc frame QP"
};

struct getprop_buf {
	struct list_head list;
	void *data;
};

static void msm_comm_generate_session_error(struct msm_vidc_inst *inst);
static void msm_comm_generate_sys_error(struct msm_vidc_inst *inst);
static void handle_session_error(enum hal_command_response cmd, void *data);

bool msm_comm_turbo_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_TURBO);
}

static inline bool is_thumbnail_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_THUMBNAIL);
}

static inline bool is_low_power_session(struct msm_vidc_inst *inst)
{
	return !!(inst->flags & VIDC_LOW_POWER);
}

int msm_comm_g_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_g_ctrl(&inst->ctrl_handler, ctrl);
}

int msm_comm_s_ctrl(struct msm_vidc_inst *inst, struct v4l2_control *ctrl)
{
	return v4l2_s_ctrl(NULL, &inst->ctrl_handler, ctrl);
}

int msm_comm_g_ctrl_for_id(struct msm_vidc_inst *inst, int id)
{
	int rc = 0;
	struct v4l2_control ctrl = {
		.id = id,
	};

	rc = msm_comm_g_ctrl(inst, &ctrl);
	return rc ?: ctrl.value;
}

static struct v4l2_ctrl **get_super_cluster(struct msm_vidc_inst *inst,
				int num_ctrls)
{
	int c = 0;
	struct v4l2_ctrl **cluster = kmalloc(sizeof(struct v4l2_ctrl *) *
			num_ctrls, GFP_KERNEL);

	if (!cluster || !inst)
		return NULL;

	for (c = 0; c < num_ctrls; c++)
		cluster[c] =  inst->ctrls[c];

	return cluster;
}

int msm_comm_ctrl_init(struct msm_vidc_inst *inst,
		struct msm_vidc_ctrl *drv_ctrls, u32 num_ctrls,
		const struct v4l2_ctrl_ops *ctrl_ops)
{
	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg = {0};
	int ret_val = 0;

	if (!inst || !drv_ctrls || !ctrl_ops || !num_ctrls) {
		dprintk(VIDC_ERR, "%s - invalid input\n", __func__);
		return -EINVAL;
	}

	inst->ctrls = kcalloc(num_ctrls, sizeof(struct v4l2_ctrl *),
				GFP_KERNEL);
	if (!inst->ctrls) {
		dprintk(VIDC_ERR, "%s - failed to allocate ctrl\n", __func__);
		return -ENOMEM;
	}

	ret_val = v4l2_ctrl_handler_init(&inst->ctrl_handler, num_ctrls);

	if (ret_val) {
		dprintk(VIDC_ERR, "CTRL ERR: Control handler init failed, %d\n",
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
			dprintk(VIDC_ERR, "%s - invalid ctrl %s\n", __func__,
				 drv_ctrls[idx].name);
			return -EINVAL;
		}

		ret_val = inst->ctrl_handler.error;
		if (ret_val) {
			dprintk(VIDC_ERR,
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
		dprintk(VIDC_WARN,
			"Failed to setup super cluster\n");
		return -EINVAL;
	}

	v4l2_ctrl_cluster(num_ctrls, inst->cluster);

	return ret_val;
}

int msm_comm_ctrl_deinit(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	kfree(inst->ctrls);
	kfree(inst->cluster);
	v4l2_ctrl_handler_free(&inst->ctrl_handler);

	return 0;
}

static inline bool is_non_realtime_session(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct v4l2_control ctrl = {
		.id = V4L2_CID_MPEG_VIDC_VIDEO_PRIORITY
	};
	rc = msm_comm_g_ctrl(inst, &ctrl);
	return (!rc && ctrl.value);
}

enum multi_stream msm_comm_get_stream_output_mode(struct msm_vidc_inst *inst)
{
	switch (msm_comm_g_ctrl_for_id(inst,
				V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_MODE)) {
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_SECONDARY:
			return HAL_VIDEO_DECODER_SECONDARY;
		case V4L2_CID_MPEG_VIDC_VIDEO_STREAM_OUTPUT_PRIMARY:
		default:
			return HAL_VIDEO_DECODER_PRIMARY;
	}
}

static int msm_comm_get_mbs_per_frame(struct msm_vidc_inst *inst)
{
	int output_port_mbs, capture_port_mbs;

	output_port_mbs = inst->in_reconfig ?
			NUM_MBS_PER_FRAME(inst->reconfig_width,
				inst->reconfig_height) :
			NUM_MBS_PER_FRAME(inst->prop.width[OUTPUT_PORT],
				inst->prop.height[OUTPUT_PORT]);
	capture_port_mbs = NUM_MBS_PER_FRAME(inst->prop.width[CAPTURE_PORT],
		inst->prop.height[CAPTURE_PORT]);

	return max(output_port_mbs, capture_port_mbs);
}

static int msm_comm_get_mbs_per_sec(struct msm_vidc_inst *inst)
{
	int rc;
	u32 fps;
	struct v4l2_control ctrl;
	int mb_per_frame;

	mb_per_frame = msm_comm_get_mbs_per_frame(inst);

	ctrl.id = V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE;
	rc = msm_comm_g_ctrl(inst, &ctrl);
	if (!rc && ctrl.value) {
		fps = (ctrl.value >> 16) ? ctrl.value >> 16 : 1;
		/*
		 * Check if operating rate is less than fps.
		 * If Yes, then use fps to scale the clocks
		 */
		fps = fps > inst->prop.fps ? fps : inst->prop.fps;
		return (mb_per_frame * fps);
	} else
		return (mb_per_frame * inst->prop.fps);
}

int msm_comm_get_inst_load(struct msm_vidc_inst *inst,
		enum load_calc_quirks quirks)
{
	int load = 0;

	mutex_lock(&inst->lock);

	if (!(inst->state >= MSM_VIDC_OPEN_DONE &&
		inst->state < MSM_VIDC_STOP_DONE))
		goto exit;

	load = msm_comm_get_mbs_per_sec(inst);

	if (is_thumbnail_session(inst)) {
		if (quirks & LOAD_CALC_IGNORE_THUMBNAIL_LOAD)
			load = 0;
	}

	if (msm_comm_turbo_session(inst)) {
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

	if (is_non_realtime_session(inst) &&
		(quirks & LOAD_CALC_IGNORE_NON_REALTIME_LOAD)) {
		if (!inst->prop.fps) {
			dprintk(VIDC_INFO, "instance:%pK fps = 0\n", inst);
			load = 0;
		} else {
			load = msm_comm_get_mbs_per_sec(inst) / inst->prop.fps;
		}
	}

exit:
	mutex_unlock(&inst->lock);
	return load;
}

int msm_comm_get_load(struct msm_vidc_core *core,
	enum session_type type, enum load_calc_quirks quirks)
{
	struct msm_vidc_inst *inst = NULL;
	int num_mbs_per_sec = 0;

	if (!core) {
		dprintk(VIDC_ERR, "Invalid args: %pK\n", core);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->session_type != type)
			continue;

		num_mbs_per_sec += msm_comm_get_inst_load(inst, quirks);
	}
	mutex_unlock(&core->lock);

	return num_mbs_per_sec;
}

enum hal_domain get_hal_domain(int session_type)
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

enum hal_video_codec get_hal_codec(int fourcc)
{
	enum hal_video_codec codec;

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
	case V4L2_PIX_FMT_VP9:
		codec = HAL_VIDEO_CODEC_VP9;
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

static enum hal_uncompressed_format get_hal_uncompressed(int fourcc)
{
	enum hal_uncompressed_format format = HAL_UNUSED_COLOR;

	switch (fourcc) {
	case V4L2_PIX_FMT_NV12:
		format = HAL_COLOR_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		format = HAL_COLOR_FORMAT_NV21;
		break;
	case V4L2_PIX_FMT_NV12_UBWC:
		format = HAL_COLOR_FORMAT_NV12_UBWC;
		break;
	case V4L2_PIX_FMT_NV12_TP10_UBWC:
		format = HAL_COLOR_FORMAT_NV12_TP10_UBWC;
		break;
	case V4L2_PIX_FMT_RGB32:
		format = HAL_COLOR_FORMAT_RGBA8888;
		break;
	case V4L2_PIX_FMT_RGBA8888_UBWC:
		format = HAL_COLOR_FORMAT_RGBA8888_UBWC;
		break;
	default:
		format = HAL_UNUSED_COLOR;
		break;
	}

	return format;
}

static int msm_comm_vote_bus(struct msm_vidc_core *core)
{
	int rc = 0, vote_data_count = 0, i = 0;
	struct hfi_device *hdev;
	struct msm_vidc_inst *inst = NULL;
	struct vidc_bus_vote_data *vote_data = NULL;
	unsigned long core_freq = 0;

	if (!core) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s Invalid device handle: %pK\n",
			__func__, hdev);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list)
		++vote_data_count;

	vote_data = kcalloc(vote_data_count, sizeof(*vote_data),
			GFP_TEMPORARY);
	if (!vote_data) {
		dprintk(VIDC_ERR, "%s: failed to allocate memory\n", __func__);
		rc = -ENOMEM;
		goto fail_alloc;
	}

	core_freq = call_hfi_op(hdev, get_core_clock_rate,
			hdev->hfi_device_data, 0);

	list_for_each_entry(inst, &core->instances, list) {
		int codec = 0, yuv = 0;
		struct v4l2_control ctrl;

		codec = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[OUTPUT_PORT].fourcc :
			inst->fmts[CAPTURE_PORT].fourcc;

		yuv = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[CAPTURE_PORT].fourcc :
			inst->fmts[OUTPUT_PORT].fourcc;

		vote_data[i].domain = get_hal_domain(inst->session_type);
		vote_data[i].codec = get_hal_codec(codec);
		vote_data[i].width =  max(inst->prop.width[CAPTURE_PORT],
			inst->prop.width[OUTPUT_PORT]);
		vote_data[i].height = max(inst->prop.height[CAPTURE_PORT],
			inst->prop.height[OUTPUT_PORT]);

		ctrl.id = V4L2_CID_MPEG_VIDC_VIDEO_OPERATING_RATE;
		rc = msm_comm_g_ctrl(inst, &ctrl);
		if (!rc && ctrl.value)
			vote_data[i].fps = (ctrl.value >> 16) ?
				ctrl.value >> 16 : 1;
		else
			vote_data[i].fps = inst->prop.fps;

		if (msm_comm_turbo_session(inst))
			vote_data[i].power_mode = VIDC_POWER_TURBO;
		else if (is_low_power_session(inst))
			vote_data[i].power_mode = VIDC_POWER_LOW;
		else
			vote_data[i].power_mode = VIDC_POWER_NORMAL;
		if (i == 0) {
			vote_data[i].imem_ab_tbl = core->resources.imem_ab_tbl;
			vote_data[i].imem_ab_tbl_size =
				core->resources.imem_ab_tbl_size;
			vote_data[i].core_freq = core_freq;
		}

		/*
		 * TODO: support for OBP-DBP split mode hasn't been yet
		 * implemented, once it is, this part of code needs to be
		 * revisited since passing in accurate information to the bus
		 * governor will drastically reduce bandwidth
		 */
		vote_data[i].color_formats[0] = get_hal_uncompressed(yuv);
		vote_data[i].num_formats = 1;
		i++;
	}
	mutex_unlock(&core->lock);

	rc = call_hfi_op(hdev, vote_bus, hdev->hfi_device_data, vote_data,
			vote_data_count);
	if (rc)
		dprintk(VIDC_ERR, "Failed to scale bus: %d\n", rc);

	kfree(vote_data);
	return rc;

fail_alloc:
	mutex_unlock(&core->lock);
	return rc;
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
		dprintk(VIDC_ERR, "Invalid inputs, fmt = %pK, index = %d\n",
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
		dprintk(VIDC_ERR, "Invalid inputs, fmt = %pK\n", fmt);
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

static void handle_sys_init_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core;
	struct vidc_hal_sys_init_done *sys_init_msg;
	u32 index;

	if (!IS_HAL_SYS_CMD(cmd)) {
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
	sys_init_msg = &response->data.sys_init_done;
	if (!sys_init_msg) {
		dprintk(VIDC_ERR, "sys_init_done message not proper\n");
		return;
	}

	core->enc_codec_supported = sys_init_msg->enc_codec_supported;
	core->dec_codec_supported = sys_init_msg->dec_codec_supported;

	/* This should come from sys_init_done */
	core->resources.max_inst_count =
		sys_init_msg->max_sessions_supported ? :
		MAX_SUPPORTED_INSTANCES;

	core->resources.max_secure_inst_count =
		core->resources.max_secure_inst_count ? :
		core->resources.max_inst_count;

	if (core->id == MSM_VIDC_CORE_VENUS &&
		(core->dec_codec_supported & HAL_VIDEO_CODEC_H264))
		core->dec_codec_supported |=
		HAL_VIDEO_CODEC_MVC;

	core->codec_count = sys_init_msg->codec_count;
	memcpy(core->capabilities, sys_init_msg->capabilities,
		sys_init_msg->codec_count * sizeof(struct msm_vidc_capability));

	dprintk(VIDC_DBG,
		"%s: supported_codecs[%d]: enc = %#x, dec = %#x\n",
		__func__, core->codec_count, core->enc_codec_supported,
		core->dec_codec_supported);

	complete(&(core->completions[index]));

}

void put_inst(struct msm_vidc_inst *inst)
{
	void put_inst_helper(struct kref *kref)
	{
		struct msm_vidc_inst *inst = container_of(kref,
				struct msm_vidc_inst, kref);
		msm_vidc_destroy(inst);
	}

	if (!inst)
		return;

	kref_put(&inst->kref, put_inst_helper);
}

struct msm_vidc_inst *get_inst(struct msm_vidc_core *core,
		void *session_id)
{
	struct msm_vidc_inst *inst = NULL;
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
	 * always acquire inst->lock and release it in put_inst for a stronger
	 * locking system.
	 */
	inst = (matches && kref_get_unless_zero(&inst->kref)) ? inst : NULL;
	mutex_unlock(&core->lock);

	return inst;
}

static void handle_session_release_buf_done(enum hal_command_response cmd,
	void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct internal_buf *buf;
	struct list_head *ptr, *next;
	struct hal_buffer_info *buffer;
	u32 buf_found = false;
	u32 address;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid release_buf_done response\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	buffer = &response->data.buffer_info;
	address = buffer->buffer_addr;

	mutex_lock(&inst->scratchbufs.lock);
	list_for_each_safe(ptr, next, &inst->scratchbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		if (address == (u32)buf->handle->device_addr) {
			dprintk(VIDC_DBG, "releasing scratch: %pa\n",
					&buf->handle->device_addr);
			buf_found = true;
		}
	}
	mutex_unlock(&inst->scratchbufs.lock);

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		if (address == (u32)buf->handle->device_addr) {
			dprintk(VIDC_DBG, "releasing persist: %pa\n",
					&buf->handle->device_addr);
			buf_found = true;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	if (!buf_found)
		dprintk(VIDC_ERR, "invalid buffer received from firmware");
	if (IS_HAL_SESSION_CMD(cmd))
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	else
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);

	put_inst(inst);
}

static void handle_sys_release_res_done(
		enum hal_command_response cmd, void *data)
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
	complete(&core->completions[
			SYS_MSG_INDEX(HAL_SYS_RELEASE_RESOURCE_DONE)]);
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
			"Inst: %pK is in bad state can't change state to %d\n",
			inst, state);
		goto exit;
	}
	dprintk(VIDC_DBG, "Moved inst: %pK from state: %d to state: %d\n",
		   inst, inst->state, state);
	inst->state = state;
exit:
	mutex_unlock(&inst->lock);
}

static int signal_session_msg_receipt(enum hal_command_response cmd,
		struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid(%pK) instance id\n", inst);
		return -EINVAL;
	}
	if (IS_HAL_SESSION_CMD(cmd)) {
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	} else {
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int wait_for_sess_signal_receipt(struct msm_vidc_inst *inst,
	enum hal_command_response cmd)
{
	int rc = 0;

	if (!IS_HAL_SESSION_CMD(cmd)) {
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	rc = wait_for_completion_timeout(
		&inst->completions[SESSION_MSG_INDEX(cmd)],
		msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "Wait interrupted or timed out: %d\n",
				SESSION_MSG_INDEX(cmd));
		msm_comm_kill_session(inst);
		WARN_ON(msm_vidc_debug_timeout);
		rc = -EIO;
	} else {
		rc = 0;
	}
	return rc;
}

static int wait_for_state(struct msm_vidc_inst *inst,
	enum instance_state flipped_state,
	enum instance_state desired_state,
	enum hal_command_response hal_cmd)
{
	int rc = 0;

	if (IS_ALREADY_IN_STATE(flipped_state, desired_state)) {
		dprintk(VIDC_INFO, "inst: %pK is already in state: %d\n",
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
}

static void msm_comm_generate_max_clients_error(struct msm_vidc_inst *inst)
{
	enum hal_command_response cmd = HAL_SESSION_ERROR;
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

static void print_cap(const char *type,
		struct hal_capability_supported *cap)
{
	dprintk(VIDC_DBG,
		"%-24s: %-8d %-8d %-8d\n",
		type, cap->min, cap->max, cap->step_size);
}

static void handle_session_init_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst = NULL;
	struct vidc_hal_session_init_done *session_init_done = NULL;
	struct msm_vidc_capability *capability = NULL;
	struct hfi_device *hdev;
	struct msm_vidc_core *core;
	u32 i, codec;

	if (!response) {
		dprintk(VIDC_ERR,
				"Failed to get valid response for session init\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
		response->session_id);

	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	if (response->status) {
		dprintk(VIDC_ERR,
			"Session init response from FW : %#x\n",
			response->status);
		if (response->status == VIDC_ERR_MAX_CLIENTS)
			msm_comm_generate_max_clients_error(inst);
		else
			msm_comm_generate_session_error(inst);

		signal_session_msg_receipt(cmd, inst);
		put_inst(inst);
		return;
	}

	core = inst->core;
	hdev = inst->core->device;
	codec = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[OUTPUT_PORT].fourcc :
			inst->fmts[CAPTURE_PORT].fourcc;

	/* check if capabilities are available for this session */
	for (i = 0; i < VIDC_MAX_SESSIONS; i++) {
		if (core->capabilities[i].codec ==
				get_hal_codec(codec) &&
			core->capabilities[i].domain ==
				get_hal_domain(inst->session_type)) {
			capability = &core->capabilities[i];
			break;
		}
	}

	if (capability) {
		dprintk(VIDC_DBG,
			"%s: capabilities available for codec 0x%x, domain %#x\n",
			__func__, capability->codec, capability->domain);
		memcpy(&inst->capability, capability,
			sizeof(struct msm_vidc_capability));
	} else {
		session_init_done = (struct vidc_hal_session_init_done *)
				&response->data.session_init_done;
		if (!session_init_done) {
			dprintk(VIDC_ERR,
				"%s: Failed to get valid response for session init\n",
				__func__);
			return;
		}
		capability = &session_init_done->capability;
		dprintk(VIDC_DBG,
			"%s: got capabilities for codec 0x%x, domain 0x%x\n",
			__func__, capability->codec,
			capability->domain);
		memcpy(&inst->capability, capability,
			sizeof(struct msm_vidc_capability));
	}
	inst->capability.pixelprocess_capabilities =
		call_hfi_op(hdev, get_core_capabilities, hdev->hfi_device_data);

	dprintk(VIDC_DBG,
		"Capability type : min      max      step size\n");
	print_cap("width", &inst->capability.width);
	print_cap("height", &inst->capability.height);
	print_cap("mbs_per_frame", &inst->capability.mbs_per_frame);
	print_cap("frame_rate", &inst->capability.frame_rate);
	print_cap("scale_x", &inst->capability.scale_x);
	print_cap("scale_y", &inst->capability.scale_y);
	print_cap("hier_p", &inst->capability.hier_p);
	print_cap("ltr_count", &inst->capability.ltr_count);
	print_cap("mbs_per_sec_low_power",
		&inst->capability.mbs_per_sec_power_save);

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
}

static void handle_event_change(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_inst *inst = NULL;
	struct msm_vidc_cb_event *event_notify = data;
	int event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
	struct v4l2_event seq_changed_event = {0};
	int rc = 0;
	struct hfi_device *hdev;
	u32 *ptr = NULL;

	if (!event_notify) {
		dprintk(VIDC_WARN, "Got an empty event from hfi\n");
		goto err_bad_event;
	}

	inst = get_inst(get_vidc_core(event_notify->device_id),
			event_notify->session_id);
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		goto err_bad_event;
	}
	hdev = inst->core->device;

	switch (event_notify->hal_event_type) {
	case HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES:
		event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;

		rc = msm_comm_g_ctrl_for_id(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_CONTINUE_DATA_TRANSFER);

		if (!IS_ERR_VALUE((unsigned long)rc) && rc == true) {
			event = V4L2_EVENT_SEQ_CHANGED_SUFFICIENT;

			if (msm_comm_get_stream_output_mode(inst) ==
				HAL_VIDEO_DECODER_SECONDARY) {
				struct hal_frame_size frame_sz;

				frame_sz.buffer_type = HAL_BUFFER_OUTPUT2;
				frame_sz.width = event_notify->width;
				frame_sz.height = event_notify->height;
				dprintk(VIDC_DBG,
					"Update OPB dimensions to firmware if buffer requirements are sufficient\n");
				rc = msm_comm_try_set_prop(inst,
					HAL_PARAM_FRAME_SIZE, &frame_sz);
			}

			dprintk(VIDC_DBG,
				"send session_continue after sufficient event\n");
			rc = call_hfi_op(hdev, session_continue,
					(void *) inst->session);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s - failed to send session_continue\n",
					__func__);
				goto err_bad_event;
			}
		}
		break;
	case HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES:
		event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
		break;
	case HAL_EVENT_RELEASE_BUFFER_REFERENCE:
	{
		struct v4l2_event buf_event = {0};
		struct buffer_info *binfo = NULL, *temp = NULL;
		u32 *ptr = NULL;

		dprintk(VIDC_DBG, "%s - inst: %pK buffer: %pa extra: %pa\n",
				__func__, inst, &event_notify->packet_buffer,
				&event_notify->extra_data_buffer);

		if (inst->state == MSM_VIDC_CORE_INVALID ||
				inst->core->state == VIDC_CORE_INVALID) {
			dprintk(VIDC_DBG,
					"Event release buf ref received in invalid state - discard\n");
			goto err_bad_event;
		}

		/*
		 * Get the buffer_info entry for the
		 * device address.
		 */
		binfo = device_to_uvaddr(&inst->registeredbufs,
				event_notify->packet_buffer);
		if (!binfo) {
			dprintk(VIDC_ERR,
					"%s buffer not found in registered list\n",
					__func__);
			goto err_bad_event;
		}

		/* Fill event data to be sent to client*/
		buf_event.type = V4L2_EVENT_RELEASE_BUFFER_REFERENCE;
		ptr = (u32 *)buf_event.u.data;
		ptr[0] = binfo->fd[0];
		ptr[1] = binfo->buff_off[0];

		dprintk(VIDC_DBG,
				"RELEASE REFERENCE EVENT FROM F/W - fd = %d offset = %d\n",
				ptr[0], ptr[1]);

		/* Decrement buffer reference count*/
		mutex_lock(&inst->registeredbufs.lock);
		list_for_each_entry(temp, &inst->registeredbufs.list,
				list) {
			if (temp == binfo) {
				buf_ref_put(inst, binfo);
				break;
			}
		}

		/*
		 * Release buffer and remove from list
		 * if reference goes to zero.
		 */
		if (unmap_and_deregister_buf(inst, binfo))
			dprintk(VIDC_ERR,
					"%s: buffer unmap failed\n", __func__);
		mutex_unlock(&inst->registeredbufs.lock);

		/*send event to client*/
		v4l2_event_queue_fh(&inst->event_handler, &buf_event);
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
	 * ptr[2] = flag to indicate bit depth or/and pic struct changed
	 * ptr[3] = bit depth
	 * ptr[4] = pic struct (progressive or interlaced)
	 * ptr[5] = colour space
	 */

	ptr = (u32 *)seq_changed_event.u.data;
	ptr[2] = 0x0;
	ptr[3] = inst->bit_depth;
	ptr[4] = inst->pic_struct;
	ptr[5] = inst->colour_space;

	if (inst->bit_depth != event_notify->bit_depth) {
		inst->bit_depth = event_notify->bit_depth;
		ptr[2] |= V4L2_EVENT_BITDEPTH_FLAG;
		ptr[3] = inst->bit_depth;
		event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
		dprintk(VIDC_DBG,
				"V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT due to bit-depth change\n");
	}

	if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_NV12 &&
		event_notify->pic_struct != MSM_VIDC_PIC_STRUCT_UNKNOWN &&
		inst->pic_struct != event_notify->pic_struct) {
		inst->pic_struct = event_notify->pic_struct;
		event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
		ptr[2] |= V4L2_EVENT_PICSTRUCT_FLAG;
		ptr[4] = inst->pic_struct;
		dprintk(VIDC_DBG,
				"V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT due to pic-struct change\n");
	}

	if (inst->bit_depth == MSM_VIDC_BIT_DEPTH_10
		&& inst->colour_space !=
		event_notify->colour_space) {
		inst->colour_space = event_notify->colour_space;
		event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
		ptr[2] |= V4L2_EVENT_COLOUR_SPACE_FLAG;
		ptr[5] = inst->colour_space;
		dprintk(VIDC_DBG,
				"V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT due to colour space change\n");
	}

	if (event == V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT) {
		dprintk(VIDC_DBG, "V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT\n");
		inst->reconfig_height = event_notify->height;
		inst->reconfig_width = event_notify->width;
		inst->in_reconfig = true;
	} else {
		dprintk(VIDC_DBG, "V4L2_EVENT_SEQ_CHANGED_SUFFICIENT\n");
		dprintk(VIDC_DBG,
			"event_notify->height = %d event_notify->width = %d\n",
			event_notify->height,
			event_notify->width);
		inst->prop.height[OUTPUT_PORT] = event_notify->height;
		inst->prop.width[OUTPUT_PORT] = event_notify->width;
	}

	inst->seqchanged_count++;

	if (inst->session_type == MSM_VIDC_DECODER)
		msm_dcvs_init_load(inst);

	rc = msm_vidc_check_session_supported(inst);
	if (!rc) {
		seq_changed_event.type = event;
		if (event == V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT) {
			u32 *ptr = NULL;

			ptr = (u32 *)seq_changed_event.u.data;
			ptr[0] = event_notify->height;
			ptr[1] = event_notify->width;
		}
		v4l2_event_queue_fh(&inst->event_handler, &seq_changed_event);
	} else if (rc == -ENOTSUPP) {
		msm_vidc_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_VIDC_HW_UNSUPPORTED);
	} else if (rc == -EBUSY) {
		msm_vidc_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_VIDC_HW_OVERLOAD);
	}

err_bad_event:
	put_inst(inst);
}

static void handle_session_prop_info(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct getprop_buf *getprop;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for prop info\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	getprop = kzalloc(sizeof(*getprop), GFP_KERNEL);
	if (!getprop) {
		dprintk(VIDC_ERR, "%s: getprop kzalloc failed\n", __func__);
		goto err_prop_info;
	}

	getprop->data = kmemdup(&response->data.property,
			sizeof(union hal_get_property), GFP_KERNEL);
	if (!getprop->data) {
		dprintk(VIDC_ERR, "%s: kmemdup failed\n", __func__);
		kfree(getprop);
		goto err_prop_info;
	}

	mutex_lock(&inst->pending_getpropq.lock);
	list_add_tail(&getprop->list, &inst->pending_getpropq.list);
	mutex_unlock(&inst->pending_getpropq.lock);

	signal_session_msg_receipt(cmd, inst);
err_prop_info:
	put_inst(inst);
}

static void handle_load_resource_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for load resource\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	if (response->status) {
		dprintk(VIDC_ERR,
				"Load resource response from FW : %#x\n",
				response->status);
		msm_comm_generate_session_error(inst);
	}

	put_inst(inst);
}

static void handle_start_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR, "Failed to get valid response for start\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
}

static void handle_stop_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR, "Failed to get valid response for stop\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
}

static void handle_release_res_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for release resource\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
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
	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
		if (binfo->buffer_ownership != DRIVER) {
			dprintk(VIDC_DBG,
				"This buffer is with FW %pa\n",
				&binfo->handle->device_addr);
			continue;
		}
		buffers_owned_by_driver++;
	}
	mutex_unlock(&inst->outputbufs.lock);

	if (buffers_owned_by_driver != output_buf->buffer_count_actual)
		dprintk(VIDC_WARN,
			"OUTPUT Buffer count mismatch %d of %d\n",
			buffers_owned_by_driver,
			output_buf->buffer_count_actual);

}

int msm_comm_queue_output_buffers(struct msm_vidc_inst *inst)
{
	struct internal_buf *binfo;
	struct hfi_device *hdev;
	struct msm_smem *handle;
	struct vidc_frame_data frame_data = {0};
	struct hal_buffer_requirements *output_buf, *extra_buf;
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

	extra_buf = get_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_OUTPUT);

	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
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
		frame_data.extradata_size = extra_buf ?
			extra_buf->buffer_size : 0;
		rc = call_hfi_op(hdev, session_ftb,
			(void *) inst->session, &frame_data);
		binfo->buffer_ownership = FIRMWARE;
	}
	mutex_unlock(&inst->outputbufs.lock);

	return 0;
}

static void handle_session_flush(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct v4l2_event flush_event = {0};
	u32 *ptr = NULL;
	enum hal_flush flush_type;
	int rc;

	if (!response) {
		dprintk(VIDC_ERR, "Failed to get valid response for flush\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
		validate_output_buffers(inst);
		if (!inst->in_reconfig) {
			rc = msm_comm_queue_output_buffers(inst);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed to queue output buffers: %d\n",
						rc);
			}
		}
	}
	atomic_dec(&inst->in_flush);
	flush_event.type = V4L2_EVENT_MSM_VIDC_FLUSH_DONE;
	ptr = (u32 *)flush_event.u.data;

	flush_type = response->data.flush_type;
	switch (flush_type) {
	case HAL_FLUSH_INPUT:
		ptr[0] = V4L2_QCOM_CMD_FLUSH_OUTPUT;
		break;
	case HAL_FLUSH_OUTPUT:
		ptr[0] = V4L2_QCOM_CMD_FLUSH_CAPTURE;
		break;
	case HAL_FLUSH_ALL:
		ptr[0] |= V4L2_QCOM_CMD_FLUSH_CAPTURE;
		ptr[0] |= V4L2_QCOM_CMD_FLUSH_OUTPUT;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid flush type received!");
		goto exit;
	}

	dprintk(VIDC_DBG,
		"Notify flush complete, flush_type: %x\n", flush_type);
	v4l2_event_queue_fh(&inst->event_handler, &flush_event);

exit:
	put_inst(inst);
}

static void handle_session_error(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct hfi_device *hdev = NULL;
	struct msm_vidc_inst *inst = NULL;
	int event = V4L2_EVENT_MSM_VIDC_SYS_ERROR;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for session error\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	hdev = inst->core->device;
	dprintk(VIDC_WARN, "Session error received for session %pK\n", inst);
	change_inst_state(inst, MSM_VIDC_CORE_INVALID);

	if (response->status == VIDC_ERR_MAX_CLIENTS) {
		dprintk(VIDC_WARN, "Too many clients, rejecting %pK", inst);
		event = V4L2_EVENT_MSM_VIDC_MAX_CLIENTS;

		/*
		 * Clean the HFI session now. Since inst->state is moved to
		 * INVALID, forward thread doesn't know FW has valid session
		 * or not. This is the last place driver knows that there is
		 * no session in FW. Hence clean HFI session now.
		 */

		msm_comm_session_clean(inst);
	} else if (response->status == VIDC_ERR_NOT_SUPPORTED) {
		dprintk(VIDC_WARN, "Unsupported bitstream in %pK", inst);
		event = V4L2_EVENT_MSM_VIDC_HW_UNSUPPORTED;
	} else {
		dprintk(VIDC_WARN, "Unknown session error (%d) for %pK\n",
				response->status, inst);
		event = V4L2_EVENT_MSM_VIDC_SYS_ERROR;
	}

	msm_vidc_queue_v4l2_event(inst, event);
	put_inst(inst);
}

static void msm_comm_clean_notify_client(struct msm_vidc_core *core)
{
	struct msm_vidc_inst *inst = NULL;

	if (!core) {
		dprintk(VIDC_ERR, "%s: Invalid params\n", __func__);
		return;
	}

	dprintk(VIDC_WARN, "%s: Core %pK\n", __func__, core);
	mutex_lock(&core->lock);
	core->state = VIDC_CORE_INVALID;

	list_for_each_entry(inst, &core->instances, list) {
		mutex_lock(&inst->lock);
		inst->state = MSM_VIDC_CORE_INVALID;
		mutex_unlock(&inst->lock);
		dprintk(VIDC_WARN,
			"%s Send sys error for inst %pK\n", __func__, inst);
		msm_vidc_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_VIDC_SYS_ERROR);
	}
	mutex_unlock(&core->lock);
}

static void handle_sys_error(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core = NULL;
	struct hfi_device *hdev = NULL;
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

	dprintk(VIDC_WARN, "SYS_ERROR %d received for core %pK\n", cmd, core);
	msm_comm_clean_notify_client(core);

	hdev = core->device;
	mutex_lock(&core->lock);
	if (core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_DBG, "Calling core_release\n");
		rc = call_hfi_op(hdev, core_release,
						 hdev->hfi_device_data);
		if (rc) {
			dprintk(VIDC_ERR, "core_release failed\n");
			mutex_unlock(&core->lock);
			return;
		}
		core->state = VIDC_CORE_UNINIT;
	}
	mutex_unlock(&core->lock);
}

void msm_comm_session_clean(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev = NULL;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return;
	}

	hdev = inst->core->device;
	mutex_lock(&inst->lock);
	if (hdev && inst->session) {
		dprintk(VIDC_DBG, "cleaning up instance: %pK\n", inst);
		rc = call_hfi_op(hdev, session_clean,
				(void *)inst->session);
		if (rc) {
			dprintk(VIDC_ERR,
				"Session clean failed :%pK\n", inst);
		}
		inst->session = NULL;
	}
	mutex_unlock(&inst->lock);
}

static void handle_session_close(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for session close\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	show_stats(inst);
	put_inst(inst);
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
		if (vb->planes[0].m.userptr == dev_addr &&
			vb->state == VB2_BUF_STATE_ACTIVE) {
			found = 1;
			dprintk(VIDC_DBG, "Found v4l2_buf index : %d\n",
					vb->index);
			break;
		}
	}
	mutex_unlock(&bufq->lock);
	if (!found) {
		dprintk(VIDC_DBG,
			"Failed to find buffer in queued list: %#lx, qtype = %d\n",
			dev_addr, q->type);
		vb = NULL;
	}
	return vb;
}

static void handle_ebd(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct vb2_buffer *vb;
	struct msm_vidc_inst *inst;
	struct vidc_hal_ebd *empty_buf_done;
	struct vb2_v4l2_buffer *vbuf = NULL;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	vb = get_vb_from_device_addr(&inst->bufq[OUTPUT_PORT],
			response->input_done.packet_buffer);
	if (vb) {
		vbuf = to_vb2_v4l2_buffer(vb);
		vb->planes[0].bytesused = response->input_done.filled_len;
		vb->planes[0].data_offset = response->input_done.offset;
		if (vb->planes[0].data_offset > vb->planes[0].length)
			dprintk(VIDC_INFO, "data_offset overflow length\n");
		if (vb->planes[0].bytesused > vb->planes[0].length)
			dprintk(VIDC_INFO, "bytesused overflow length\n");
		if (vb->planes[0].m.userptr !=
			response->clnt_data)
			dprintk(VIDC_INFO, "Client data != bufaddr\n");
		empty_buf_done = (struct vidc_hal_ebd *)&response->input_done;
		if (empty_buf_done) {
			if (empty_buf_done->status == VIDC_ERR_NOT_SUPPORTED) {
				dprintk(VIDC_INFO,
					"Failed : Unsupported input stream\n");
				vbuf->flags |=
					V4L2_QCOM_BUF_INPUT_UNSUPPORTED;
			}
			if (empty_buf_done->status == VIDC_ERR_BITSTREAM_ERR) {
				dprintk(VIDC_INFO,
					"Failed : Corrupted input stream\n");
				vbuf->flags |=
					V4L2_QCOM_BUF_DATA_CORRUPT;
			}
			if (empty_buf_done->status ==
				VIDC_ERR_START_CODE_NOT_FOUND) {
				vbuf->flags |=
					V4L2_MSM_VIDC_BUF_START_CODE_NOT_FOUND;
				dprintk(VIDC_INFO,
					"Failed: Start code not found\n");
			}
			if (empty_buf_done->flags & HAL_BUFFERFLAG_SYNCFRAME)
				vbuf->flags |=
					V4L2_QCOM_BUF_FLAG_IDRFRAME |
					V4L2_BUF_FLAG_KEYFRAME;
		}
		dprintk(VIDC_DBG,
			"Got ebd from hal: device_addr: %pa, alloc: %d, status: %#x, pic_type: %#x, flags: %#x\n",
			&empty_buf_done->packet_buffer,
			empty_buf_done->alloc_len, empty_buf_done->status,
			empty_buf_done->picture_type, empty_buf_done->flags);

		mutex_lock(&inst->bufq[OUTPUT_PORT].lock);
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		mutex_unlock(&inst->bufq[OUTPUT_PORT].lock);
		msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_EBD);
	}

	put_inst(inst);
}

int buf_ref_get(struct msm_vidc_inst *inst, struct buffer_info *binfo)
{
	int cnt = 0;

	if (!inst || !binfo)
		return -EINVAL;

	atomic_inc(&binfo->ref_count);
	cnt = atomic_read(&binfo->ref_count);
	if (cnt > 2) {
		dprintk(VIDC_DBG, "%s: invalid ref_cnt: %d\n", __func__, cnt);
		cnt = -EINVAL;
	}
	if (cnt == 2)
		inst->buffers_held_in_driver++;

	dprintk(VIDC_DBG, "REF_GET[%d] fd[0] = %d\n", cnt, binfo->fd[0]);

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

	atomic_dec(&binfo->ref_count);
	cnt = atomic_read(&binfo->ref_count);
	dprintk(VIDC_DBG, "REF_PUT[%d] fd[0] = %d\n", cnt, binfo->fd[0]);
	if (!cnt)
		release_buf = true;
	else if (cnt == 1)
		qbuf_again = true;
	else {
		dprintk(VIDC_DBG, "%s: invalid ref_cnt: %d\n", __func__, cnt);
		cnt = -EINVAL;
	}

	if (cnt < 0)
		return cnt;

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
		inst->buffers_held_in_driver--;
		rc = qbuf_dynamic_buf(inst, binfo);
		if (!rc)
			return rc;
	}
	return cnt;
}

static void handle_dynamic_buffer(struct msm_vidc_inst *inst,
		ion_phys_addr_t device_addr, u32 flags)
{
	struct buffer_info *binfo = NULL, *temp = NULL;

	/*
	 * Update reference count and release OR queue back the buffer,
	 * only when firmware is not holding a reference.
	 */
	if (inst->buffer_mode_set[CAPTURE_PORT] == HAL_BUFFER_MODE_DYNAMIC) {
		binfo = device_to_uvaddr(&inst->registeredbufs, device_addr);
		if (!binfo) {
			dprintk(VIDC_ERR,
				"%s buffer not found in registered list\n",
				__func__);
			return;
		}
		if (flags & HAL_BUFFERFLAG_READONLY) {
			dprintk(VIDC_DBG,
				"FBD fd[0] = %d -> Reference with f/w, addr: %pa\n",
				binfo->fd[0], &device_addr);
		} else {
			dprintk(VIDC_DBG,
				"FBD fd[0] = %d -> FBD_ref_released, addr: %pa\n",
				binfo->fd[0], &device_addr);

			mutex_lock(&inst->registeredbufs.lock);
			list_for_each_entry(temp, &inst->registeredbufs.list,
							list) {
				if (temp == binfo) {
					buf_ref_put(inst, binfo);
					break;
				}
			}
			mutex_unlock(&inst->registeredbufs.lock);
		}
	}
}

static int handle_multi_stream_buffers(struct msm_vidc_inst *inst,
		ion_phys_addr_t dev_addr)
{
	struct internal_buf *binfo;
	struct msm_smem *handle;
	bool found = false;

	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
		handle = binfo->handle;
		if (handle && dev_addr == handle->device_addr) {
			if (binfo->buffer_ownership == DRIVER) {
				dprintk(VIDC_ERR,
					"FW returned same buffer: %pa\n",
					&dev_addr);
				break;
			}
			binfo->buffer_ownership = DRIVER;
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->outputbufs.lock);

	if (!found) {
		dprintk(VIDC_ERR,
			"Failed to find output buffer in queued list: %pa\n",
			&dev_addr);
	}

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

static void handle_fbd(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct msm_vidc_inst *inst;
	struct vb2_buffer *vb = NULL;
	struct vidc_hal_fbd *fill_buf_done;
	enum hal_buffer buffer_type;
	int extra_idx = 0;
	int64_t time_usec = 0;
	static int first_enc_frame = 1;
	struct vb2_v4l2_buffer *vbuf = NULL;
	struct buffer_info *buffer_info = NULL;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	fill_buf_done = (struct vidc_hal_fbd *)&response->output_done;
	buffer_type = msm_comm_get_hal_output_buffer(inst);
	if (fill_buf_done->buffer_type == buffer_type) {
		vb = get_vb_from_device_addr(&inst->bufq[CAPTURE_PORT],
				fill_buf_done->packet_buffer1);
	} else {
		if (handle_multi_stream_buffers(inst,
				fill_buf_done->packet_buffer1))
			dprintk(VIDC_ERR,
				"Failed : Output buffer not found %pa\n",
				&fill_buf_done->packet_buffer1);
		goto err_handle_fbd;
	}

	if (vb) {
		vbuf = to_vb2_v4l2_buffer(vb);
		vb->planes[0].bytesused = fill_buf_done->filled_len1;
		vb->planes[0].data_offset = fill_buf_done->offset1;
		if (vb->planes[0].data_offset > vb->planes[0].length)
			dprintk(VIDC_INFO,
				"fbd:Overflow data_offset = %d; length = %d\n",
				vb->planes[0].data_offset,
				vb->planes[0].length);
		if (vb->planes[0].bytesused > vb->planes[0].length)
			dprintk(VIDC_INFO,
				"fbd:Overflow bytesused = %d; length = %d\n",
				vb->planes[0].bytesused,
				vb->planes[0].length);

		buffer_info = device_to_uvaddr(&inst->registeredbufs,
			fill_buf_done->packet_buffer1);

		if (!buffer_info) {
			dprintk(VIDC_ERR,
				"%s buffer not found in registered list\n",
				__func__);
			return;
		}

		buffer_info->crop_data.nLeft = fill_buf_done->start_x_coord;
		buffer_info->crop_data.nTop = fill_buf_done->start_y_coord;
		buffer_info->crop_data.nWidth = fill_buf_done->frame_width;
		buffer_info->crop_data.nHeight = fill_buf_done->frame_height;
		buffer_info->crop_data.width_height[0] =
						inst->prop.width[CAPTURE_PORT];
		buffer_info->crop_data.width_height[1] =
						inst->prop.height[CAPTURE_PORT];

		if (!(fill_buf_done->flags1 &
			HAL_BUFFERFLAG_TIMESTAMPINVALID)) {
			time_usec = fill_buf_done->timestamp_hi;
			time_usec = (time_usec << 32) |
				fill_buf_done->timestamp_lo;
		} else {
			time_usec = 0;
			dprintk(VIDC_DBG,
					"Set zero timestamp for buffer %pa, filled: %d, (hi:%u, lo:%u)\n",
					&fill_buf_done->packet_buffer1,
					fill_buf_done->filled_len1,
					fill_buf_done->timestamp_hi,
					fill_buf_done->timestamp_lo);
		}
		vb->timestamp = (time_usec * NSEC_PER_USEC);
		vbuf->flags = 0;
		extra_idx =
			EXTRADATA_IDX(inst->fmts[CAPTURE_PORT].num_planes);
		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			vb->planes[extra_idx].m.userptr =
				(unsigned long)fill_buf_done->extra_data_buffer;
			vb->planes[extra_idx].bytesused =
				vb->planes[extra_idx].length;
			vb->planes[extra_idx].data_offset = 0;
		}

		handle_dynamic_buffer(inst, fill_buf_done->packet_buffer1,
					fill_buf_done->flags1);
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_READONLY)
			vbuf->flags |= V4L2_QCOM_BUF_FLAG_READONLY;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_EOS)
			vbuf->flags |= V4L2_QCOM_BUF_FLAG_EOS;
		/* if (fill_buf_done->flags1 & HAL_BUFFERFLAG_ENDOFFRAME)
		 * vb->v4l2_buf.flags |= V4L2_QCOM_BUF_FLAG_ENDOFFRAME;
		 */
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_CODECCONFIG)
			vbuf->flags &= ~V4L2_QCOM_BUF_FLAG_CODECCONFIG;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_SYNCFRAME)
			vbuf->flags |= V4L2_QCOM_BUF_FLAG_IDRFRAME;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_EOSEQ)
			vbuf->flags |= V4L2_QCOM_BUF_FLAG_EOSEQ;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DECODEONLY)
			vbuf->flags |= V4L2_QCOM_BUF_FLAG_DECODEONLY;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DATACORRUPT)
			vbuf->flags |= V4L2_QCOM_BUF_DATA_CORRUPT;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DROP_FRAME)
			vbuf->flags |= V4L2_QCOM_BUF_DROP_FRAME;
		if (fill_buf_done->flags1 & HAL_BUFFERFLAG_MBAFF)
			vbuf->flags |= V4L2_MSM_BUF_FLAG_MBAFF;

		switch (fill_buf_done->picture_type) {
		case HAL_PICTURE_IDR:
			vbuf->flags |= V4L2_QCOM_BUF_FLAG_IDRFRAME;
			vbuf->flags |= V4L2_BUF_FLAG_KEYFRAME;
			break;
		case HAL_PICTURE_I:
			vbuf->flags |= V4L2_BUF_FLAG_KEYFRAME;
			break;
		case HAL_PICTURE_P:
			vbuf->flags |= V4L2_BUF_FLAG_PFRAME;
			break;
		case HAL_PICTURE_B:
			vbuf->flags |= V4L2_BUF_FLAG_BFRAME;
			break;
		case HAL_FRAME_NOTCODED:
		case HAL_UNUSED_PICT:
			/* Do we need to care about these? */
		case HAL_FRAME_YUV:
			break;
		default:
			break;
		}

		inst->count.fbd++;

		if (extra_idx && extra_idx < VIDEO_MAX_PLANES) {
			dprintk(VIDC_DBG,
				"extradata: userptr = %pK;"
				" bytesused = %d; length = %d\n",
				(u8 *)vb->planes[extra_idx].m.userptr,
				vb->planes[extra_idx].bytesused,
				vb->planes[extra_idx].length);
		}
		if (first_enc_frame == 1) {
			boot_stats_init();
			pr_debug("KPI: First Encoded frame received\n");
			first_enc_frame++;
		}
		dprintk(VIDC_DBG,
		"Got fbd from hal: device_addr: %pa, alloc: %d, filled: %d, offset: %d, ts: %lld, flags: %#x, crop: %d %d %d %d, pic_type: %#x, mark_data: %#x\n",
		&fill_buf_done->packet_buffer1, fill_buf_done->alloc_len1,
		fill_buf_done->filled_len1, fill_buf_done->offset1, time_usec,
		fill_buf_done->flags1, fill_buf_done->start_x_coord,
		fill_buf_done->start_y_coord, fill_buf_done->frame_width,
		fill_buf_done->frame_height, fill_buf_done->picture_type,
		fill_buf_done->mark_data);

		mutex_lock(&inst->bufq[CAPTURE_PORT].lock);
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		mutex_unlock(&inst->bufq[CAPTURE_PORT].lock);
		msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_FBD);
	}

err_handle_fbd:
	put_inst(inst);
}

static void handle_seq_hdr_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct msm_vidc_inst *inst;
	struct vb2_buffer *vb;
	struct vidc_hal_fbd *fill_buf_done;
	struct vb2_v4l2_buffer *vbuf;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_WARN, "Got a response for an inactive session\n");
		return;
	}

	fill_buf_done = (struct vidc_hal_fbd *)&response->output_done;
	vb = get_vb_from_device_addr(&inst->bufq[CAPTURE_PORT],
				fill_buf_done->packet_buffer1);
	if (!vb) {
		dprintk(VIDC_ERR,
				"Failed to find video buffer for seq_hdr_done: %pa\n",
				&fill_buf_done->packet_buffer1);
		goto err_seq_hdr_done;
	}
	vbuf = to_vb2_v4l2_buffer(vb);
	vb->planes[0].bytesused = fill_buf_done->filled_len1;
	vb->planes[0].data_offset = fill_buf_done->offset1;

	vbuf->flags = V4L2_QCOM_BUF_FLAG_CODECCONFIG;
	vb->timestamp = 0;

	dprintk(VIDC_DBG, "Filled length = %d; offset = %d; flags %x\n",
				vb->planes[0].bytesused,
				vb->planes[0].data_offset,
				vbuf->flags);
	mutex_lock(&inst->bufq[CAPTURE_PORT].lock);
	vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	mutex_unlock(&inst->bufq[CAPTURE_PORT].lock);

err_seq_hdr_done:
	put_inst(inst);
}

void handle_cmd_response(enum hal_command_response cmd, void *data)
{
	dprintk(VIDC_DBG, "Command response = %d\n", cmd);
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
	case HAL_SESSION_PROPERTY_INFO:
		handle_session_prop_info(cmd, data);
		break;
	case HAL_SESSION_LOAD_RESOURCE_DONE:
		handle_load_resource_done(cmd, data);
		break;
	case HAL_SESSION_START_DONE:
		handle_start_done(cmd, data);
		break;
	case HAL_SESSION_ETB_DONE:
		handle_ebd(cmd, data);
		break;
	case HAL_SESSION_FTB_DONE:
		handle_fbd(cmd, data);
		break;
	case HAL_SESSION_STOP_DONE:
		handle_stop_done(cmd, data);
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
	case HAL_SESSION_GET_SEQ_HDR_DONE:
		handle_seq_hdr_done(cmd, data);
		break;
	case HAL_SYS_WATCHDOG_TIMEOUT:
	case HAL_SYS_ERROR:
		handle_sys_error(cmd, data);
		break;
	case HAL_SESSION_ERROR:
		handle_session_error(cmd, data);
		break;
	case HAL_SESSION_RELEASE_BUFFER_DONE:
		handle_session_release_buf_done(cmd, data);
		break;
	default:
		dprintk(VIDC_DBG, "response unhandled: %d\n", cmd);
		break;
	}
}

int msm_comm_scale_clocks(struct msm_vidc_core *core)
{
	int num_mbs_per_sec =
		msm_comm_get_load(core, MSM_VIDC_ENCODER, LOAD_CALC_NO_QUIRKS) +
		msm_comm_get_load(core, MSM_VIDC_DECODER, LOAD_CALC_NO_QUIRKS);
	return msm_comm_scale_clocks_load(core, num_mbs_per_sec,
				LOAD_CALC_NO_QUIRKS);
}

int msm_comm_scale_clocks_load(struct msm_vidc_core *core,
		int num_mbs_per_sec, enum load_calc_quirks quirks)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_inst *inst = NULL;
	unsigned long instant_bitrate = 0;
	int num_sessions = 0;
	struct vidc_clk_scale_data clk_scale_data = { {0} };
	int codec = 0;

	if (!core) {
		dprintk(VIDC_ERR, "%s Invalid args: %pK\n", __func__, core);
		return -EINVAL;
	}

	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s Invalid device handle: %pK\n",
			__func__, hdev);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {

		codec = inst->session_type == MSM_VIDC_DECODER ?
			inst->fmts[OUTPUT_PORT].fourcc :
			inst->fmts[CAPTURE_PORT].fourcc;

		if (msm_comm_turbo_session(inst))
			clk_scale_data.power_mode[num_sessions] =
				VIDC_POWER_TURBO;
		else if (is_low_power_session(inst))
			clk_scale_data.power_mode[num_sessions] =
				VIDC_POWER_LOW;
		else
			clk_scale_data.power_mode[num_sessions] =
				VIDC_POWER_NORMAL;

		if (inst->dcvs_mode)
			clk_scale_data.load[num_sessions] = inst->dcvs.load;
		else
			clk_scale_data.load[num_sessions] =
				msm_comm_get_inst_load(inst, quirks);

		clk_scale_data.session[num_sessions] =
				VIDC_VOTE_DATA_SESSION_VAL(
				get_hal_codec(codec),
				get_hal_domain(inst->session_type));
		num_sessions++;

		if (inst->instant_bitrate > instant_bitrate)
			instant_bitrate = inst->instant_bitrate;

	}
	clk_scale_data.num_sessions = num_sessions;
	mutex_unlock(&core->lock);


	rc = call_hfi_op(hdev, scale_clocks,
		hdev->hfi_device_data, num_mbs_per_sec,
		&clk_scale_data, instant_bitrate);
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

static inline enum msm_vidc_thermal_level msm_comm_vidc_thermal_level(int level)
{
	switch (level) {
	case 0:
		return VIDC_THERMAL_NORMAL;
	case 1:
		return VIDC_THERMAL_LOW;
	case 2:
		return VIDC_THERMAL_HIGH;
	default:
		return VIDC_THERMAL_CRITICAL;
	}
}

static unsigned long msm_comm_get_clock_rate(struct msm_vidc_core *core)
{
	struct hfi_device *hdev;
	unsigned long freq = 0;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = core->device;

	freq = call_hfi_op(hdev, get_core_clock_rate, hdev->hfi_device_data, 1);
	dprintk(VIDC_DBG, "clock freq %ld\n", freq);

	return freq;
}

static bool is_core_turbo(struct msm_vidc_core *core, unsigned long freq)
{
	int i = 0;
	struct msm_vidc_platform_resources *res = &core->resources;
	struct load_freq_table *table = res->load_freq_tbl;
	u32 max_freq = 0;

	for (i = 0; i < res->load_freq_tbl_size; i++) {
		if (max_freq < table[i].freq)
			max_freq = table[i].freq;
	}
	return freq >= max_freq;
}

static bool is_thermal_permissible(struct msm_vidc_core *core)
{
	enum msm_vidc_thermal_level tl;
	unsigned long freq = 0;
	bool is_turbo = false;

	if (!core->resources.thermal_mitigable)
		return true;

	if (!msm_vidc_thermal_mitigation_disabled) {
		dprintk(VIDC_DBG,
			"Thermal mitigation not enabled. debugfs %d\n",
			msm_vidc_thermal_mitigation_disabled);
		return true;
	}

	tl = msm_comm_vidc_thermal_level(vidc_driver->thermal_level);
	freq = msm_comm_get_clock_rate(core);

	is_turbo = is_core_turbo(core, freq);
	dprintk(VIDC_DBG,
		"Core freq %ld Thermal level %d Turbo mode %d\n",
		freq, tl, is_turbo);

	if (is_turbo && tl >= VIDC_THERMAL_LOW) {
		dprintk(VIDC_ERR,
			"Video session not allowed. Turbo mode %d Thermal level %d\n",
			is_turbo, tl);
		return false;
	}
	return true;
}

static int msm_comm_session_abort(struct msm_vidc_inst *inst)
{
	int rc = 0, abort_completion = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	abort_completion = SESSION_MSG_INDEX(HAL_SESSION_ABORT_DONE);

	rc = call_hfi_op(hdev, session_abort, (void *)inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s session_abort failed rc: %d\n", __func__, rc);
		return rc;
	}
	rc = wait_for_completion_timeout(
			&inst->completions[abort_completion],
			msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR,
				"%s: Wait interrupted or timed out [%pK]: %d\n",
				__func__, inst, abort_completion);
		WARN_ON(msm_vidc_debug_timeout);
		rc = -EBUSY;
	} else {
		rc = 0;
	}
	msm_comm_session_clean(inst);
	return rc;
}

static void handle_thermal_event(struct msm_vidc_core *core)
{
	int rc = 0;
	struct msm_vidc_inst *inst;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return;
	}
	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (!inst->session)
			continue;

		mutex_unlock(&core->lock);
		if (inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_CLOSE_DONE) {
			dprintk(VIDC_WARN, "%s: abort inst %pK\n",
				__func__, inst);
			rc = msm_comm_session_abort(inst);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s session_abort failed rc: %d\n",
					__func__, rc);
				goto err_sess_abort;
			}
			change_inst_state(inst, MSM_VIDC_CORE_INVALID);
			dprintk(VIDC_WARN,
				"%s Send sys error for inst %pK\n",
				__func__, inst);
			msm_vidc_queue_v4l2_event(inst,
					V4L2_EVENT_MSM_VIDC_SYS_ERROR);
		} else {
			msm_comm_generate_session_error(inst);
		}
		mutex_lock(&core->lock);
	}
	mutex_unlock(&core->lock);
	return;

err_sess_abort:
	msm_comm_clean_notify_client(core);
}

void msm_comm_handle_thermal_event(void)
{
	struct msm_vidc_core *core;

	list_for_each_entry(core, &vidc_driver->cores, list) {
		if (!is_thermal_permissible(core)) {
			dprintk(VIDC_WARN,
				"Thermal level critical, stop all active sessions!\n");
			handle_thermal_event(core);
		}
	}
}

int msm_comm_check_core_init(struct msm_vidc_core *core)
{
	int rc = 0;

	mutex_lock(&core->lock);
	if (core->state >= VIDC_CORE_INIT_DONE) {
		dprintk(VIDC_INFO, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto exit;
	}
	dprintk(VIDC_DBG, "Waiting for SYS_INIT_DONE\n");
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(HAL_SYS_INIT_DONE)],
		msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "%s: Wait interrupted or timed out: %d\n",
				__func__, SYS_MSG_INDEX(HAL_SYS_INIT_DONE));
		WARN_ON(msm_vidc_debug_timeout);
		rc = -EIO;
		goto exit;
	} else {
		core->state = VIDC_CORE_INIT_DONE;
		rc = 0;
	}
	dprintk(VIDC_DBG, "SYS_INIT_DONE!!!\n");
exit:
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_comm_init_core_done(struct msm_vidc_inst *inst)
{
	int rc = 0;

	rc = msm_comm_check_core_init(inst->core);
	if (rc) {
		dprintk(VIDC_ERR, "%s - failed to initialize core\n", __func__);
		msm_comm_generate_sys_error(inst);
		return rc;
	}
	change_inst_state(inst, MSM_VIDC_CORE_INIT_DONE);
	return rc;
}

static int msm_comm_init_core(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_core *core;

	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;

	core = inst->core;
	hdev = core->device;
	mutex_lock(&core->lock);
	if (core->state >= VIDC_CORE_INIT) {
		dprintk(VIDC_INFO, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_inited;
	}
	if (!core->capabilities) {
		core->capabilities = kzalloc(VIDC_MAX_SESSIONS *
				sizeof(struct msm_vidc_capability), GFP_KERNEL);
		if (!core->capabilities) {
			dprintk(VIDC_ERR,
				"%s: failed to allocate capabilities\n",
				__func__);
			rc = -ENOMEM;
			goto fail_cap_alloc;
		}
	} else {
		dprintk(VIDC_WARN,
			"%s: capabilities memory is expected to be freed\n",
			__func__);
	}

	rc = call_hfi_op(hdev, core_init, hdev->hfi_device_data);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init core, id = %d\n",
				core->id);
		goto fail_core_init;
	}
	core->state = VIDC_CORE_INIT;
	core->smmu_fault_handled = false;
core_already_inited:
	change_inst_state(inst, MSM_VIDC_CORE_INIT);
	mutex_unlock(&core->lock);
	return rc;

fail_core_init:
	kfree(core->capabilities);
fail_cap_alloc:
	core->capabilities = NULL;
	core->state = VIDC_CORE_UNINIT;
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_vidc_deinit_core(struct msm_vidc_inst *inst)
{
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

	if (!core->resources.never_unload_fw) {
		cancel_delayed_work(&core->fw_unload_work);

		/*
		 * Delay unloading of firmware. This is useful
		 * in avoiding firmware download delays in cases where we
		 * will have a burst of back to back video playback sessions
		 * e.g. thumbnail generation.
		 */
		schedule_delayed_work(&core->fw_unload_work,
			msecs_to_jiffies(core->state == VIDC_CORE_INVALID ?
					0 : msm_vidc_firmware_unload_delay));

		dprintk(VIDC_DBG, "firmware unload delayed by %u ms\n",
			core->state == VIDC_CORE_INVALID ?
			0 : msm_vidc_firmware_unload_delay);
	}

core_already_uninited:
	change_inst_state(inst, MSM_VIDC_CORE_UNINIT);
	mutex_unlock(&core->lock);
	return 0;
}

int msm_comm_force_cleanup(struct msm_vidc_inst *inst)
{
	msm_comm_kill_session(inst);
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
		dprintk(VIDC_INFO, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	if (inst->session_type == MSM_VIDC_DECODER) {
		fourcc = inst->fmts[OUTPUT_PORT].fourcc;
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		fourcc = inst->fmts[CAPTURE_PORT].fourcc;
	} else {
		dprintk(VIDC_ERR, "Invalid session\n");
		return -EINVAL;
	}

	rc = call_hfi_op(hdev, session_init, hdev->hfi_device_data,
			inst, get_hal_domain(inst->session_type),
			get_hal_codec(fourcc),
			&inst->session);

	if (rc || !inst->session) {
		dprintk(VIDC_ERR,
			"Failed to call session init for: %pK, %pK, %d, %d\n",
			inst->core->device, inst,
			inst->session_type, fourcc);
		rc = -EINVAL;
		goto exit;
	}
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
		if (temp->state >= MSM_VIDC_OPEN_DONE &&
				temp->state < MSM_VIDC_STOP_DONE) {
			char properties[4] = "";

			if (is_thumbnail_session(temp))
				strlcat(properties, "N", sizeof(properties));

			if (msm_comm_turbo_session(temp))
				strlcat(properties, "T", sizeof(properties));

			dprintk(VIDC_ERR, "%4d|%4d|%4d|%4d|%4s\n",
					temp->session_type,
					max(temp->prop.width[CAPTURE_PORT],
						temp->prop.width[OUTPUT_PORT]),
					max(temp->prop.height[CAPTURE_PORT],
						temp->prop.height[OUTPUT_PORT]),
					temp->prop.fps, properties);
		}
	}
	mutex_unlock(&core->lock);
}

static int msm_vidc_load_resources(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	int num_mbs_per_sec = 0, max_load_adj = 0;
	struct msm_vidc_core *core;
	enum load_calc_quirks quirks = LOAD_CALC_IGNORE_TURBO_LOAD |
		LOAD_CALC_IGNORE_THUMBNAIL_LOAD |
		LOAD_CALC_IGNORE_NON_REALTIME_LOAD;

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

	max_load_adj = core->resources.max_load +
		inst->capability.mbs_per_frame.max;

	if (num_mbs_per_sec > max_load_adj) {
		dprintk(VIDC_ERR, "HW is overloaded, needed: %d max: %d\n",
			num_mbs_per_sec, max_load_adj);
		msm_vidc_print_running_insts(core);
		inst->state = MSM_VIDC_CORE_INVALID;
		msm_comm_kill_session(inst);
		return -EBUSY;
	}

	hdev = core->device;
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_LOAD_RESOURCES)) {
		dprintk(VIDC_INFO, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
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
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
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
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	dprintk(VIDC_DBG, "Send Stop to hal\n");
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
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	dprintk(VIDC_DBG,
		"Send release res to hal\n");
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
			"inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	dprintk(VIDC_DBG,
		"Send session close to hal\n");
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

int msm_comm_suspend(int core_id)
{
	struct hfi_device *hdev;
	struct msm_vidc_core *core;
	int rc = 0;

	core = get_vidc_core(core_id);
	if (!core) {
		dprintk(VIDC_ERR,
			"%s: Failed to find core for core_id = %d\n",
			__func__, core_id);
		return -EINVAL;
	}

	hdev = (struct hfi_device *)core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s Invalid device handle\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	if (core->state == VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"%s - fw is not in proper state, skip suspend\n",
				__func__);
		rc = -EINVAL;
		goto exit;
	}

	rc = call_hfi_op(hdev, suspend, hdev->hfi_device_data);
	if (rc)
		dprintk(VIDC_WARN, "Failed to suspend\n");

exit:
	mutex_unlock(&core->lock);
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
	u32 smem_flags = 0, buffer_size;
	struct hal_buffer_requirements *output_buf, *extradata_buf;
	int i;
	struct hfi_device *hdev;
	struct hal_buffer_size_minimum b;

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
	b.buffer_type = buffer_type;
	b.buffer_size = buffer_size;
	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HAL_PARAM_BUFFER_SIZE_MINIMUM,
		&b);

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

			binfo->handle = handle;
			binfo->buffer_type = buffer_type;
			binfo->buffer_ownership = DRIVER;
			dprintk(VIDC_DBG, "Output buffer address: %pa\n",
					&handle->device_addr);

			if (inst->buffer_mode_set[CAPTURE_PORT] ==
				HAL_BUFFER_MODE_STATIC) {
				struct vidc_buffer_addr_info buffer_info = {0};

				buffer_info.buffer_size =
					output_buf->buffer_size;
				buffer_info.buffer_type = buffer_type;
				buffer_info.num_buffers = 1;
				buffer_info.align_device_addr =
					handle->device_addr;
				buffer_info.extradata_addr =
					handle->device_addr +
					output_buf->buffer_size;
				if (extradata_buf)
					buffer_info.extradata_size =
						extradata_buf->buffer_size;
				rc = call_hfi_op(hdev, session_set_buffers,
					(void *) inst->session, &buffer_info);
				if (rc) {
					dprintk(VIDC_ERR,
						"%s : session_set_buffers failed\n",
						__func__);
					goto fail_set_buffers;
				}
			}
			mutex_lock(&inst->outputbufs.lock);
			list_add_tail(&binfo->list, &inst->outputbufs.list);
			mutex_unlock(&inst->outputbufs.lock);
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

static inline char *get_buffer_name(enum hal_buffer buffer_type)
{
	switch (buffer_type) {
	case HAL_BUFFER_INPUT: return "input";
	case HAL_BUFFER_OUTPUT: return "output";
	case HAL_BUFFER_OUTPUT2: return "output_2";
	case HAL_BUFFER_EXTRADATA_INPUT: return "input_extra";
	case HAL_BUFFER_EXTRADATA_OUTPUT: return "output_extra";
	case HAL_BUFFER_EXTRADATA_OUTPUT2: return "output2_extra";
	case HAL_BUFFER_INTERNAL_SCRATCH: return "scratch";
	case HAL_BUFFER_INTERNAL_SCRATCH_1: return "scratch_1";
	case HAL_BUFFER_INTERNAL_SCRATCH_2: return "scratch_2";
	case HAL_BUFFER_INTERNAL_PERSIST: return "persist";
	case HAL_BUFFER_INTERNAL_PERSIST_1: return "persist_1";
	case HAL_BUFFER_INTERNAL_CMD_QUEUE: return "queue";
	default: return "????";
	}
}

static int set_internal_buf_on_fw(struct msm_vidc_inst *inst,
				enum hal_buffer buffer_type,
				struct msm_smem *handle, bool reuse)
{
	struct vidc_buffer_addr_info buffer_info;
	struct hfi_device *hdev;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device || !handle) {
		dprintk(VIDC_ERR, "%s - invalid params\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	rc = msm_comm_smem_cache_operations(inst,
					handle, SMEM_CACHE_CLEAN);
	if (rc) {
		dprintk(VIDC_WARN,
			"Failed to clean cache. Undefined behavior\n");
	}

	buffer_info.buffer_size = handle->size;
	buffer_info.buffer_type = buffer_type;
	buffer_info.num_buffers = 1;
	buffer_info.align_device_addr = handle->device_addr;
	dprintk(VIDC_DBG, "%s %s buffer : %pa\n",
				reuse ? "Reusing" : "Allocated",
				get_buffer_name(buffer_type),
				&buffer_info.align_device_addr);

	rc = call_hfi_op(hdev, session_set_buffers,
		(void *) inst->session, &buffer_info);
	if (rc) {
		dprintk(VIDC_ERR,
			"vidc_hal_session_set_buffers failed\n");
		return rc;
	}
	return 0;
}

static bool reuse_internal_buffers(struct msm_vidc_inst *inst,
		enum hal_buffer buffer_type, struct msm_vidc_list *buf_list)
{
	struct internal_buf *buf;
	int rc = 0;
	bool reused = false;

	if (!inst || !buf_list) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return false;
	}

	mutex_lock(&buf_list->lock);
	list_for_each_entry(buf, &buf_list->list, list) {
		if (!buf->handle) {
			reused = false;
			break;
		}

		if (buf->buffer_type != buffer_type)
			continue;

		/*
		 * Persist buffer size won't change with resolution. If they
		 * are in queue means that they are already allocated and
		 * given to HW. HW can use them without reallocation. These
		 * buffers are not released as part of port reconfig. So
		 * driver no need to set them again.
		 */

		if (buffer_type != HAL_BUFFER_INTERNAL_PERSIST
			&& buffer_type != HAL_BUFFER_INTERNAL_PERSIST_1) {

			rc = set_internal_buf_on_fw(inst, buffer_type,
					buf->handle, true);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s: session_set_buffers failed\n",
					__func__);
				reused = false;
				break;
			}
		}
		reused = true;
		dprintk(VIDC_DBG,
			"Re-using internal buffer type : %d\n", buffer_type);
	}
	mutex_unlock(&buf_list->lock);
	return reused;
}

static int allocate_and_set_internal_bufs(struct msm_vidc_inst *inst,
			struct hal_buffer_requirements *internal_bufreq,
			struct msm_vidc_list *buf_list)
{
	struct msm_smem *handle;
	struct internal_buf *binfo;
	u32 smem_flags = 0;
	int rc = 0;
	int i = 0;

	if (!inst || !internal_bufreq || !buf_list)
		return -EINVAL;

	if (!internal_bufreq->buffer_size)
		return 0;

	if (inst->flags & VIDC_SECURE)
		smem_flags |= SMEM_SECURE;

	for (i = 0; i < internal_bufreq->buffer_count_actual; i++) {
		handle = msm_comm_smem_alloc(inst, internal_bufreq->buffer_size,
				1, smem_flags, internal_bufreq->buffer_type, 0);
		if (!handle) {
			dprintk(VIDC_ERR,
				"Failed to allocate scratch memory\n");
			rc = -ENOMEM;
			goto err_no_mem;
		}

		binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!binfo) {
			dprintk(VIDC_ERR, "Out of memory\n");
			rc = -ENOMEM;
			goto fail_kzalloc;
		}

		binfo->handle = handle;
		binfo->buffer_type = internal_bufreq->buffer_type;

		rc = set_internal_buf_on_fw(inst, internal_bufreq->buffer_type,
				handle, false);
		if (rc)
			goto fail_set_buffers;

		mutex_lock(&buf_list->lock);
		list_add_tail(&binfo->list, &buf_list->list);
		mutex_unlock(&buf_list->lock);
	}
	return rc;

fail_set_buffers:
	kfree(binfo);
fail_kzalloc:
	msm_comm_smem_free(inst, handle);
err_no_mem:
	return rc;

}

static int set_internal_buffers(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type, struct msm_vidc_list *buf_list)
{
	struct hal_buffer_requirements *internal_buf;

	internal_buf = get_buff_req_buffer(inst, buffer_type);
	if (!internal_buf) {
		dprintk(VIDC_DBG,
			"This internal buffer not required, buffer_type: %x\n",
			buffer_type);
		return 0;
	}

	dprintk(VIDC_DBG, "Buffer type %s: num = %d, size = %d\n",
		get_buffer_name(buffer_type),
		internal_buf->buffer_count_actual, internal_buf->buffer_size);

	/*
	 * Try reusing existing internal buffers first.
	 * If it's not possible to reuse, allocate new buffers.
	 */
	if (reuse_internal_buffers(inst, buffer_type, buf_list))
		return 0;

	return allocate_and_set_internal_bufs(inst, internal_buf,
				buf_list);
}

int msm_comm_try_state(struct msm_vidc_inst *inst, int state)
{
	int rc = 0;
	int flipped_state;
	struct msm_vidc_core *core;

	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	dprintk(VIDC_DBG,
			"Trying to move inst: %pK from: %#x to %#x\n",
			inst, inst->state, state);
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", inst);
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
			"flipped_state = %#x\n", flipped_state);
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
			HAL_SESSION_INIT_DONE);
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
				HAL_SESSION_START_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_STOP:
		rc = msm_vidc_stop(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_STOP_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_STOP_DONE,
				HAL_SESSION_STOP_DONE);
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
			HAL_SESSION_RELEASE_RESOURCE_DONE);
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
				HAL_SESSION_END_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		msm_comm_session_clean(inst);
	case MSM_VIDC_CORE_UNINIT:
	case MSM_VIDC_CORE_INVALID:
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

int msm_vidc_comm_cmd(void *instance, union msm_v4l2_cmd *cmd)
{
	struct msm_vidc_inst *inst = instance;
	struct v4l2_decoder_cmd *dec = NULL;
	struct v4l2_encoder_cmd *enc = NULL;
	struct msm_vidc_core *core;
	int which_cmd = 0, flags = 0, rc = 0;

	if (!inst || !inst->core || !cmd) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	if (inst->session_type == MSM_VIDC_ENCODER) {
		enc = (struct v4l2_encoder_cmd *)cmd;
		which_cmd = enc->cmd;
		flags = enc->flags;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		dec = (struct v4l2_decoder_cmd *)cmd;
		which_cmd = dec->cmd;
		flags = dec->flags;
	}


	switch (which_cmd) {
	case V4L2_QCOM_CMD_FLUSH:
		if (core->state != VIDC_CORE_INVALID &&
			inst->state ==  MSM_VIDC_CORE_INVALID) {
			rc = msm_comm_kill_session(inst);
			if (rc)
				dprintk(VIDC_ERR,
					"Fail to clean session: %d\n",
					rc);
		}
		rc = msm_comm_flush(inst, flags);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to flush buffers: %d\n", rc);
		}
		break;
	case V4L2_DEC_QCOM_CMD_RECONFIG_HINT:
	{
		u32 *ptr = NULL;
		struct hal_buffer_requirements *output_buf;

		rc = msm_comm_try_get_bufreqs(inst);
		if (rc) {
			dprintk(VIDC_ERR,
					"Getting buffer requirements failed: %d\n",
					rc);
			break;
		}

		output_buf = get_buff_req_buffer(inst,
				msm_comm_get_hal_output_buffer(inst));
		if (output_buf) {
			if (dec) {
				ptr = (u32 *)dec->raw.data;
				ptr[0] = output_buf->buffer_size;
				ptr[1] = output_buf->buffer_count_actual;
				dprintk(VIDC_DBG,
					"Reconfig hint, size is %u, count is %u\n",
					ptr[0], ptr[1]);
			} else {
				dprintk(VIDC_ERR, "Null decoder\n");
			}
		} else {
			dprintk(VIDC_DBG,
					"This output buffer not required, buffer_type: %x\n",
					HAL_BUFFER_OUTPUT);
		}
		break;
	}
	default:
		dprintk(VIDC_ERR, "Unknown Command %d\n", which_cmd);
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static void populate_frame_data(struct vidc_frame_data *data,
		const struct vb2_buffer *vb, struct msm_vidc_inst *inst)
{
	u64 time_usec;
	int extra_idx;
	enum v4l2_buf_type type = vb->type;
	enum vidc_ports port = type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ?
		OUTPUT_PORT : CAPTURE_PORT;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	time_usec = vb->timestamp;
	do_div(time_usec, NSEC_PER_USEC);

	data->alloc_len = vb->planes[0].length;
	data->device_addr = vb->planes[0].m.userptr;
	data->timestamp = time_usec;
	data->flags = 0;
	data->clnt_data = data->device_addr;

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		bool pic_decoding_mode = msm_comm_g_ctrl_for_id(inst,
				V4L2_CID_MPEG_VIDC_VIDEO_PICTYPE_DEC_MODE);

		data->buffer_type = HAL_BUFFER_INPUT;
		data->filled_len = vb->planes[0].bytesused;
		data->offset = vb->planes[0].data_offset;

		if (vbuf->flags & V4L2_QCOM_BUF_FLAG_EOS)
			data->flags |= HAL_BUFFERFLAG_EOS;

		if (vbuf->flags & V4L2_MSM_BUF_FLAG_YUV_601_709_CLAMP)
			data->flags |= HAL_BUFFERFLAG_YUV_601_709_CSC_CLAMP;

		if (vbuf->flags & V4L2_QCOM_BUF_FLAG_CODECCONFIG)
			data->flags |= HAL_BUFFERFLAG_CODECCONFIG;

		if (vbuf->flags & V4L2_QCOM_BUF_FLAG_DECODEONLY)
			data->flags |= HAL_BUFFERFLAG_DECODEONLY;

		if (vbuf->flags & V4L2_QCOM_BUF_TIMESTAMP_INVALID)
			data->timestamp = LLONG_MAX;

		/* XXX: This is a dirty hack necessitated by the firmware,
		 * which refuses to issue FBDs for non I-frames in Picture Type
		 * Decoding mode, unless we pass in non-zero value in mark_data
		 * and mark_target.
		 */
		data->mark_data = data->mark_target =
			pic_decoding_mode ? 0xdeadbeef : 0;

	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		data->buffer_type = msm_comm_get_hal_output_buffer(inst);
	}

	extra_idx = EXTRADATA_IDX(inst->fmts[port].num_planes);
	if (extra_idx && extra_idx < VIDEO_MAX_PLANES &&
			vb->planes[extra_idx].m.userptr) {
		data->extradata_addr = vb->planes[extra_idx].m.userptr;
		data->extradata_size = vb->planes[extra_idx].length;
		data->flags |= HAL_BUFFERFLAG_EXTRADATA;
	}
}

static unsigned int count_single_batch(struct msm_vidc_list *list,
		enum v4l2_buf_type type)
{
	struct vb2_buf_entry *buf;
	int count = 0;
	struct vb2_v4l2_buffer *vbuf = NULL;

	mutex_lock(&list->lock);
	list_for_each_entry(buf, &list->list, list) {
		if (buf->vb->type != type)
			continue;

		++count;

		vbuf = to_vb2_v4l2_buffer(buf->vb);
		if (!(vbuf->flags & V4L2_MSM_BUF_FLAG_DEFER))
			goto found_batch;
	}
	 /* don't have a full batch */
	count = 0;

found_batch:
	mutex_unlock(&list->lock);
	return count;
}

static unsigned int count_buffers(struct msm_vidc_list *list,
		enum v4l2_buf_type type)
{
	struct vb2_buf_entry *buf;
	int count = 0;

	mutex_lock(&list->lock);
	list_for_each_entry(buf, &list->list, list) {
		if (buf->vb->type != type)
			continue;

		++count;
	}
	mutex_unlock(&list->lock);

	return count;
}

static void log_frame(struct msm_vidc_inst *inst, struct vidc_frame_data *data,
		enum v4l2_buf_type type)
{
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		dprintk(VIDC_DBG,
				"Sending etb (%pa) to hal: filled: %d, ts: %lld, flags = %#x\n",
				&data->device_addr, data->filled_len,
				data->timestamp, data->flags);
		msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_ETB);

		if (msm_vidc_bitrate_clock_scaling &&
			inst->session_type == MSM_VIDC_DECODER &&
			!inst->dcvs_mode)
			inst->instant_bitrate =
				data->filled_len * 8 * inst->prop.fps;
		else
			inst->instant_bitrate = 0;
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		dprintk(VIDC_DBG,
				"Sending ftb (%pa) to hal: size: %d, ts: %lld, flags = %#x\n",
				&data->device_addr, data->alloc_len,
				data->timestamp, data->flags);
		msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_FTB);
	}

	msm_dcvs_check_and_scale_clocks(inst,
			type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

	if (msm_vidc_bitrate_clock_scaling && !inst->dcvs_mode &&
		type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
		inst->session_type == MSM_VIDC_DECODER)
		if (msm_comm_scale_clocks(inst->core))
			dprintk(VIDC_WARN,
				"Failed to scale clocks. Performance might be impacted\n");
}

static int request_seq_header(struct msm_vidc_inst *inst,
		struct vidc_frame_data *data)
{
	struct vidc_seq_hdr seq_hdr = {
		.seq_hdr = data->device_addr,
		.seq_hdr_len = data->alloc_len,
	};

	dprintk(VIDC_DBG, "Requesting sequence header in %pa\n",
			&seq_hdr.seq_hdr);
	return call_hfi_op(inst->core->device, session_get_seq_hdr,
			inst->session, &seq_hdr);
}

/*
 * Attempts to queue `vb` to hardware.  If, for various reasons, the buffer
 * cannot be queued to hardware, the buffer will be staged for commit in the
 * pending queue.  Once the hardware reaches a good state (or if `vb` is NULL,
 * the subsequent *_qbuf will commit the previously staged buffers to hardware.
 */
int msm_comm_qbuf(struct msm_vidc_inst *inst, struct vb2_buffer *vb)
{
	int rc = 0;
	int capture_count, output_count;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	struct {
		struct vidc_frame_data *data;
		int count;
	} etbs, ftbs;
	bool defer = false, batch_mode;
	struct vb2_buf_entry *temp, *next;
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);

	if (!inst) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	hdev = core->device;

	if (inst->state == MSM_VIDC_CORE_INVALID ||
		core->state == VIDC_CORE_INVALID ||
		core->state == VIDC_CORE_UNINIT) {
		dprintk(VIDC_ERR, "Core is in bad state. Can't Queue\n");
		return -EINVAL;
	}

	/* Stick the buffer into the pendinq, we'll pop it out later on
	 * if we want to commit it to hardware
	 */
	if (vb) {
		temp = kzalloc(sizeof(*temp), GFP_KERNEL);
		if (!temp) {
			dprintk(VIDC_ERR, "Out of memory\n");
			goto err_no_mem;
		}

		temp->vb = vb;
		mutex_lock(&inst->pendingq.lock);
		list_add_tail(&temp->list, &inst->pendingq.list);
		mutex_unlock(&inst->pendingq.lock);
	}

	batch_mode = msm_comm_g_ctrl_for_id(inst, V4L2_CID_VIDC_QBUF_MODE)
		== V4L2_VIDC_QBUF_BATCHED;
	capture_count = (batch_mode ? &count_single_batch : &count_buffers)
		(&inst->pendingq, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	output_count = (batch_mode ? &count_single_batch : &count_buffers)
		(&inst->pendingq, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

	/*
	 * Somewhat complicated logic to prevent queuing the buffer to hardware.
	 * Don't queue if:
	 * 1) Hardware isn't ready (that's simple)
	 */
	defer = defer ?: inst->state != MSM_VIDC_START_DONE;

	/*
	 * 2) The client explicitly tells us not to because it wants this
	 * buffer to be batched with future frames.  The batch size (on both
	 * capabilities) is completely determined by the client.
	 */
	defer = defer ?: vbuf && vbuf->flags & V4L2_MSM_BUF_FLAG_DEFER;

	/* 3) If we're in batch mode, we must have full batches of both types */
	defer = defer ?: batch_mode && (!output_count || !capture_count);

	if (defer) {
		dprintk(VIDC_DBG, "Deferring queue of %pK\n", vb);
		return 0;
	}

	dprintk(VIDC_DBG, "%sing %d etbs and %d ftbs\n",
			batch_mode ? "Batch" : "Process",
			output_count, capture_count);

	etbs.data = kcalloc(output_count, sizeof(*etbs.data), GFP_KERNEL);
	ftbs.data = kcalloc(capture_count, sizeof(*ftbs.data), GFP_KERNEL);
	/* Note that it's perfectly normal for (e|f)tbs.data to be NULL if
	 * we're not in batch mode (i.e. (output|capture)_count == 0)
	 */
	if ((!etbs.data && output_count) ||
			(!ftbs.data && capture_count)) {
		dprintk(VIDC_ERR, "Failed to alloc memory for batching\n");
		kfree(etbs.data);
		etbs.data = NULL;

		kfree(ftbs.data);
		ftbs.data = NULL;
		goto err_no_mem;
	}

	etbs.count = ftbs.count = 0;

	/*
	 * Try to collect all pending buffers into 2 batches of ftb and etb
	 * Note that these "batches" might be empty if we're no in batching mode
	 * and the pendingq is empty
	 */
	mutex_lock(&inst->pendingq.lock);
	list_for_each_entry_safe(temp, next, &inst->pendingq.list, list) {
		struct vidc_frame_data *frame_data = NULL;

		switch (temp->vb->type) {
		case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
			if (ftbs.count < capture_count && ftbs.data)
				frame_data = &ftbs.data[ftbs.count++];
			break;
		case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
			if (etbs.count < output_count && etbs.data)
				frame_data = &etbs.data[etbs.count++];
			break;
		default:
			break;
		}

		if (!frame_data)
			continue;

		populate_frame_data(frame_data, temp->vb, inst);

		list_del(&temp->list);
		kfree(temp);
	}
	mutex_unlock(&inst->pendingq.lock);

	/* Finally commit all our frame(s) to H/W */
	if (batch_mode) {
		int ftb_index = 0, c = 0;

		for (c = 0; atomic_read(&inst->seq_hdr_reqs) > 0; ++c) {
			rc = request_seq_header(inst, &ftbs.data[c]);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed requesting sequence header: %d\n",
						rc);
				goto err_bad_input;
			}

			atomic_dec(&inst->seq_hdr_reqs);
		}

		ftb_index = c;
		rc = call_hfi_op(hdev, session_process_batch, inst->session,
				etbs.count, etbs.data,
				ftbs.count - ftb_index, &ftbs.data[ftb_index]);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to queue batch of %d ETBs and %d FTBs\n",
				etbs.count, ftbs.count);
			goto err_bad_input;
		}

		for (c = ftb_index; c < ftbs.count; ++c) {
			log_frame(inst, &ftbs.data[c],
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		}

		for (c = 0; c < etbs.count; ++c) {
			log_frame(inst, &etbs.data[c],
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		}
	}

	if (!batch_mode && etbs.count) {
		int c = 0;

		for (c = 0; c < etbs.count; ++c) {
			struct vidc_frame_data *frame_data = &etbs.data[c];

			rc = call_hfi_op(hdev, session_etb, inst->session,
					frame_data);
			if (rc) {
				dprintk(VIDC_ERR, "Failed to issue etb: %d\n",
						rc);
				goto err_bad_input;
			}

			log_frame(inst, frame_data,
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
		}
	}

	if (!batch_mode && ftbs.count) {
		int c = 0;

		for (c = 0; atomic_read(&inst->seq_hdr_reqs) > 0; ++c) {
			rc = request_seq_header(inst, &ftbs.data[c]);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed requesting sequence header: %d\n",
						rc);
				goto err_bad_input;
			}

			atomic_dec(&inst->seq_hdr_reqs);
		}

		for (; c < ftbs.count; ++c) {
			struct vidc_frame_data *frame_data = &ftbs.data[c];

			rc = call_hfi_op(hdev, session_ftb,
					inst->session, frame_data);
			if (rc) {
				dprintk(VIDC_ERR, "Failed to issue ftb: %d\n",
						rc);
				goto err_bad_input;
			}

			log_frame(inst, frame_data,
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
		}
	}

err_bad_input:
	if (rc)
		dprintk(VIDC_ERR, "Failed to queue buffer\n");

	kfree(etbs.data);
	kfree(ftbs.data);
err_no_mem:
	return rc;
}

int msm_comm_try_get_bufreqs(struct msm_vidc_inst *inst)
{
	int rc = 0, i = 0;
	union hal_get_property hprop;

	rc = msm_comm_try_get_prop(inst, HAL_PARAM_GET_BUFFER_REQUIREMENTS,
					&hprop);
	if (rc) {
		dprintk(VIDC_ERR, "Failed getting buffer requirements: %d", rc);
		return rc;
	}

	dprintk(VIDC_DBG, "Buffer requirements:\n");
	dprintk(VIDC_DBG, "%15s %8s %8s\n", "buffer type", "count", "size");
	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		struct hal_buffer_requirements req = hprop.buf_req.buffer[i];

		inst->buff_req.buffer[i] = req;
		dprintk(VIDC_DBG, "%15s %8d %8d\n",
				get_buffer_name(req.buffer_type),
				req.buffer_count_actual, req.buffer_size);
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

	hdev = inst->core->device;
	mutex_lock(&inst->sync_lock);
	if (inst->state < MSM_VIDC_OPEN_DONE ||
			inst->state >= MSM_VIDC_CLOSE) {

		/* No need to check inst->state == MSM_VIDC_INVALID since
		 * INVALID is > CLOSE_DONE. When core went to INVALID state,
		 * we put all the active instances in INVALID. So > CLOSE_DONE
		 * is enough check to have.
		 */

		dprintk(VIDC_ERR,
			"In Wrong state to call Buf Req: Inst %pK or Core %pK\n",
				inst, inst->core);
		rc = -EAGAIN;
		mutex_unlock(&inst->sync_lock);
		goto exit;
	}
	mutex_unlock(&inst->sync_lock);

	switch (ptype) {
	case HAL_PARAM_PROFILE_LEVEL_CURRENT:
	case HAL_CONFIG_VDEC_ENTROPY:
		rc = call_hfi_op(hdev, session_get_property, inst->session,
				ptype);
		break;
	case HAL_PARAM_GET_BUFFER_REQUIREMENTS:
		rc = call_hfi_op(hdev, session_get_buf_req, inst->session);
		break;
	default:
		rc = -EAGAIN;
		break;
	}

	if (rc) {
		dprintk(VIDC_ERR, "Can't query hardware for property: %d\n",
				rc);
		goto exit;
	}

	rc = wait_for_completion_timeout(&inst->completions[
			SESSION_MSG_INDEX(HAL_SESSION_PROPERTY_INFO)],
		msecs_to_jiffies(msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR,
			"%s: Wait interrupted or timed out [%pK]: %d\n",
			__func__, inst,
			SESSION_MSG_INDEX(HAL_SESSION_PROPERTY_INFO));
		inst->state = MSM_VIDC_CORE_INVALID;
		msm_comm_kill_session(inst);
		WARN_ON(msm_vidc_debug_timeout);
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
		dprintk(VIDC_ERR, "%s getprop list empty\n", __func__);
		rc = -EINVAL;
	}
	mutex_unlock(&inst->pending_getpropq.lock);
exit:
	return rc;
}

int msm_comm_release_output_buffers(struct msm_vidc_inst *inst)
{
	struct msm_smem *handle;
	struct internal_buf *buf, *dummy;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	mutex_lock(&inst->outputbufs.lock);
	if (list_empty(&inst->outputbufs.list)) {
		dprintk(VIDC_DBG, "%s - No OUTPUT buffers allocated\n",
			__func__);
		mutex_unlock(&inst->outputbufs.lock);
		return 0;
	}
	mutex_unlock(&inst->outputbufs.lock);

	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}
	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry_safe(buf, dummy, &inst->outputbufs.list, list) {
		handle = buf->handle;
		if (!handle) {
			dprintk(VIDC_ERR, "%s - invalid handle\n", __func__);
			goto exit;
		}

		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		if (inst->buffer_mode_set[CAPTURE_PORT] ==
			HAL_BUFFER_MODE_STATIC &&
			inst->state != MSM_VIDC_CORE_INVALID &&
				core->state != VIDC_CORE_INVALID) {
			buffer_info.response_required = false;
			rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
			if (rc) {
				dprintk(VIDC_WARN,
					"Rel output buf fail:%pa, %d\n",
					&buffer_info.align_device_addr,
					buffer_info.buffer_size);
			}
		}

		list_del(&buf->list);
		msm_comm_smem_free(inst, buf->handle);
		kfree(buf);
	}

exit:
	mutex_unlock(&inst->outputbufs.lock);
	return rc;
}

static enum hal_buffer scratch_buf_sufficient(struct msm_vidc_inst *inst,
				enum hal_buffer buffer_type)
{
	struct hal_buffer_requirements *bufreq = NULL;
	struct internal_buf *buf;
	int count = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "%s - invalid param\n", __func__);
		goto not_sufficient;
	}

	bufreq = get_buff_req_buffer(inst, buffer_type);
	if (!bufreq)
		goto not_sufficient;

	/* Check if current scratch buffers are sufficient */
	mutex_lock(&inst->scratchbufs.lock);

	list_for_each_entry(buf, &inst->scratchbufs.list, list) {
		if (!buf->handle) {
			dprintk(VIDC_ERR, "%s: invalid buf handle\n", __func__);
			mutex_unlock(&inst->scratchbufs.lock);
			goto not_sufficient;
		}
		if (buf->buffer_type == buffer_type &&
			buf->handle->size >= bufreq->buffer_size)
			count++;
	}
	mutex_unlock(&inst->scratchbufs.lock);

	if (count != bufreq->buffer_count_actual)
		goto not_sufficient;

	dprintk(VIDC_DBG,
		"Existing scratch buffer is sufficient for buffer type %#x\n",
		buffer_type);

	return buffer_type;

not_sufficient:
	return HAL_BUFFER_NONE;
}

int msm_comm_release_scratch_buffers(struct msm_vidc_inst *inst,
					bool check_for_reuse)
{
	struct msm_smem *handle;
	struct internal_buf *buf, *dummy;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	enum hal_buffer sufficiency = HAL_BUFFER_NONE;

	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %pK\n", hdev);
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
		if (!buf->handle) {
			dprintk(VIDC_ERR, "%s - buf->handle NULL\n", __func__);
			rc = -EINVAL;
			goto exit;
		}

		handle = buf->handle;
		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		if (inst->state != MSM_VIDC_CORE_INVALID &&
				core->state != VIDC_CORE_INVALID) {
			buffer_info.response_required = true;
			rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
			if (rc) {
				dprintk(VIDC_WARN,
					"Rel scrtch buf fail:%pa, %d\n",
					&buffer_info.align_device_addr,
					buffer_info.buffer_size);
			}
			mutex_unlock(&inst->scratchbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc) {
				change_inst_state(inst,
					MSM_VIDC_CORE_INVALID);
				msm_comm_kill_session(inst);
			}
			mutex_lock(&inst->scratchbufs.lock);
		}

		/*If scratch buffers can be reused, do not free the buffers*/
		if (sufficiency & buf->buffer_type)
			continue;

		list_del(&buf->list);
		msm_comm_smem_free(inst, buf->handle);
		kfree(buf);
	}

exit:
	mutex_unlock(&inst->scratchbufs.lock);
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
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		handle = buf->handle;
		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		if (inst->state != MSM_VIDC_CORE_INVALID &&
				core->state != VIDC_CORE_INVALID) {
			buffer_info.response_required = true;
			rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
			if (rc) {
				dprintk(VIDC_WARN,
					"Rel prst buf fail:%pa, %d\n",
					&buffer_info.align_device_addr,
					buffer_info.buffer_size);
			}
			mutex_unlock(&inst->persistbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc) {
				change_inst_state(inst, MSM_VIDC_CORE_INVALID);
				msm_comm_kill_session(inst);
			}
			mutex_lock(&inst->persistbufs.lock);
		}
		list_del(&buf->list);
		msm_comm_smem_free(inst, buf->handle);
		kfree(buf);
	}
	mutex_unlock(&inst->persistbufs.lock);
	return rc;
}

int msm_comm_try_set_prop(struct msm_vidc_inst *inst,
	enum hal_property ptype, void *pdata)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(VIDC_ERR, "Invalid input: %pK\n", inst);
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

	if (msm_comm_release_scratch_buffers(inst, true))
		dprintk(VIDC_WARN, "Failed to release scratch buffers\n");

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH,
		&inst->scratchbufs);
	if (rc)
		goto error;

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH_1,
		&inst->scratchbufs);
	if (rc)
		goto error;

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH_2,
		&inst->scratchbufs);
	if (rc)
		goto error;

	return rc;
error:
	msm_comm_release_scratch_buffers(inst, false);
	return rc;
}

int msm_comm_set_persist_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_PERSIST,
		&inst->persistbufs);
	if (rc)
		goto error;

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_PERSIST_1,
		&inst->persistbufs);
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
	enum vidc_ports ports[] = {OUTPUT_PORT, CAPTURE_PORT};
	int c = 0;

	for (c = 0; c < ARRAY_SIZE(ports); ++c) {
		enum vidc_ports port = ports[c];

		dprintk(VIDC_DBG, "Flushing buffers of type %d in bad state\n",
				port);
		mutex_lock(&inst->bufq[port].lock);
		list_for_each_safe(ptr, next, &inst->bufq[port].
				vb2_bufq.queued_list) {
			struct vb2_buffer *vb = container_of(ptr,
					struct vb2_buffer, queued_entry);

			vb->planes[0].bytesused = 0;
			vb->planes[0].data_offset = 0;

			vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
		}
		mutex_unlock(&inst->bufq[port].lock);
	}

	msm_vidc_queue_v4l2_event(inst, V4L2_EVENT_MSM_VIDC_FLUSH_DONE);
}

void msm_comm_flush_dynamic_buffers(struct msm_vidc_inst *inst)
{
	struct buffer_info *binfo = NULL;

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
	mutex_lock(&inst->registeredbufs.lock);
	if (!list_empty(&inst->registeredbufs.list)) {
		struct v4l2_event buf_event = {0};
		u32 *ptr = NULL;

		list_for_each_entry(binfo, &inst->registeredbufs.list, list) {
			if (binfo->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
				atomic_read(&binfo->ref_count) == 2) {

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
					"released buffer held in driver before issuing flush: %pa fd[0]: %d\n",
					&binfo->device_addr[0], binfo->fd[0]);
				/*send event to client*/
				v4l2_event_queue_fh(&inst->event_handler,
					&buf_event);
			}
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);
}

void msm_comm_flush_pending_dynamic_buffers(struct msm_vidc_inst *inst)
{
	struct buffer_info *binfo = NULL;

	if (!inst)
		return;

	if (inst->buffer_mode_set[CAPTURE_PORT] != HAL_BUFFER_MODE_DYNAMIC)
		return;

	if (list_empty(&inst->pendingq.list) ||
		list_empty(&inst->registeredbufs.list))
		return;

	/*
	 * Dynamic Buffer mode - Since pendingq is not empty
	 * no output buffers have been sent to firmware yet.
	 * Hence remove reference to all pendingq o/p buffers
	 * before flushing them.
	 */

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(binfo, &inst->registeredbufs.list, list) {
		if (binfo->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
			dprintk(VIDC_DBG,
				"%s: binfo = %pK device_addr = %pa\n",
				__func__, binfo, &binfo->device_addr[0]);
			buf_ref_put(inst, binfo);
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);
}

int msm_comm_flush(struct msm_vidc_inst *inst, u32 flags)
{
	int rc =  0;
	bool ip_flush = false;
	bool op_flush = false;
	struct vb2_buf_entry *temp, *next;
	struct mutex *lock;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}

	ip_flush = flags & V4L2_QCOM_CMD_FLUSH_OUTPUT;
	op_flush = flags & V4L2_QCOM_CMD_FLUSH_CAPTURE;

	if (ip_flush && !op_flush) {
		dprintk(VIDC_INFO, "Input only flush not supported\n");
		return 0;
	}

	msm_comm_flush_dynamic_buffers(inst);

	if (inst->state == MSM_VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_INVALID ||
			core->state == VIDC_CORE_UNINIT) {
		dprintk(VIDC_ERR,
				"Core %pK and inst %pK are in bad state\n",
					core, inst);
		msm_comm_flush_in_invalid_state(inst);
		return 0;
	}

	if (inst->in_reconfig && !ip_flush && op_flush) {
		mutex_lock(&inst->pendingq.lock);
		if (!list_empty(&inst->pendingq.list)) {
			/*
			 * Execution can never reach here since port reconfig
			 * wont happen unless pendingq is emptied out
			 * (both pendingq and flush being secured with same
			 * lock). Printing a message here incase this breaks.
			 */
			dprintk(VIDC_WARN,
			"FLUSH BUG: Pending q not empty! It should be empty\n");
		}
		mutex_unlock(&inst->pendingq.lock);
		atomic_inc(&inst->in_flush);
		dprintk(VIDC_DBG, "Send flush Output to firmware\n");
		rc = call_hfi_op(hdev, session_flush, inst->session,
				HAL_FLUSH_OUTPUT);
	} else {
		msm_comm_flush_pending_dynamic_buffers(inst);
		/*
		 * If flush is called after queueing buffers but before
		 * streamon driver should flush the pending queue
		 */
		mutex_lock(&inst->pendingq.lock);
		list_for_each_entry_safe(temp, next,
				&inst->pendingq.list, list) {
			enum v4l2_buf_type type = temp->vb->type;

			if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
				lock = &inst->bufq[CAPTURE_PORT].lock;
			else
				lock = &inst->bufq[OUTPUT_PORT].lock;

			temp->vb->planes[0].bytesused = 0;

			mutex_lock(lock);
			vb2_buffer_done(temp->vb, VB2_BUF_STATE_DONE);
			msm_vidc_debugfs_update(inst,
				type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE ?
					MSM_VIDC_DEBUGFS_EVENT_FBD :
					MSM_VIDC_DEBUGFS_EVENT_EBD);
			list_del(&temp->list);
			mutex_unlock(lock);

			kfree(temp);
		}
		mutex_unlock(&inst->pendingq.lock);

		/*Do not send flush in case of session_error */
		if (!(inst->state == MSM_VIDC_CORE_INVALID &&
			  core->state != VIDC_CORE_INVALID))
			atomic_inc(&inst->in_flush);
			dprintk(VIDC_DBG, "Send flush all to firmware\n");
			rc = call_hfi_op(hdev, session_flush, inst->session,
				HAL_FLUSH_ALL);
	}

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
		ret = HAL_EXTRADATA_DEC_FRAME_QP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_ENC_FRAME_QP:
		ret = HAL_EXTRADATA_ENC_FRAME_QP;
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
	case V4L2_MPEG_VIDC_EXTRADATA_VQZIP_SEI:
		ret = HAL_EXTRADATA_VQZIP_SEI;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_YUV_STATS:
		ret = HAL_EXTRADATA_YUV_STATS;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_ROI_QP:
		ret = HAL_EXTRADATA_ROI_QP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_OUTPUT_CROP:
		ret = HAL_EXTRADATA_OUTPUT_CROP;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_DISPLAY_COLOUR_SEI:
		ret = HAL_EXTRADATA_MASTERING_DISPLAY_COLOUR_SEI;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI:
		ret = HAL_EXTRADATA_CONTENT_LIGHT_LEVEL_SEI;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_VUI_DISPLAY:
		ret = HAL_EXTRADATA_VUI_DISPLAY_INFO;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_VPX_COLORSPACE:
		ret = HAL_EXTRADATA_VPX_COLORSPACE;
		break;
	case V4L2_MPEG_VIDC_EXTRADATA_PQ_INFO:
		ret = HAL_EXTRADATA_PQ_INFO;
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

int msm_vidc_trigger_ssr(struct msm_vidc_core *core,
	enum hal_ssr_trigger_type type)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!core || !core->device) {
		dprintk(VIDC_WARN, "Invalid parameters: %pK\n", core);
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
	int num_mbs_per_sec = 0, max_load_adj = 0;
	enum load_calc_quirks quirks = LOAD_CALC_IGNORE_TURBO_LOAD |
		LOAD_CALC_IGNORE_THUMBNAIL_LOAD |
		LOAD_CALC_IGNORE_NON_REALTIME_LOAD;

	if (inst->state == MSM_VIDC_OPEN_DONE) {
		max_load_adj = inst->core->resources.max_load +
			inst->capability.mbs_per_frame.max;
		num_mbs_per_sec = msm_comm_get_load(inst->core,
					MSM_VIDC_DECODER, quirks);
		num_mbs_per_sec += msm_comm_get_load(inst->core,
					MSM_VIDC_ENCODER, quirks);
		if (num_mbs_per_sec > max_load_adj) {
			dprintk(VIDC_ERR,
				"H/W is overloaded. needed: %d max: %d\n",
				num_mbs_per_sec,
				max_load_adj);
			msm_vidc_print_running_insts(inst->core);
			return -EBUSY;
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
			"Invalid : Input height = %d width = %d output height = %d width = %d\n",
			input_height, input_width, output_height,
			output_width);
		return -ENOTSUPP;
	}

	if (!inst->capability.scale_x.min ||
		!inst->capability.scale_x.max ||
		!inst->capability.scale_y.min ||
		!inst->capability.scale_y.max) {

		if (input_width * input_height !=
			output_width * output_height) {
			dprintk(VIDC_ERR,
				"%s: scaling is not supported (%dx%d != %dx%d)\n",
				__func__, input_width, input_height,
				output_width, output_height);
			return -ENOTSUPP;
		}
		dprintk(VIDC_DBG, "%s: supported WxH = %dx%d\n",
			__func__, input_width, input_height);
		return 0;
	}

	x_min = (1<<16)/inst->capability.scale_x.min;
	y_min = (1<<16)/inst->capability.scale_y.min;
	x_max = inst->capability.scale_x.max >> 16;
	y_max = inst->capability.scale_y.max >> 16;

	if (input_height > output_height) {
		if (input_height > x_min * output_height) {
			dprintk(VIDC_ERR,
				"Unsupported height downscale ratio %d vs %d\n",
				input_height/output_height, x_min);
			return -ENOTSUPP;
		}
	} else {
		if (output_height > x_max * input_height) {
			dprintk(VIDC_ERR,
				"Unsupported height upscale ratio %d vs %d\n",
				input_height/output_height, x_max);
			return -ENOTSUPP;
		}
	}
	if (input_width > output_width) {
		if (input_width > y_min * output_width) {
			dprintk(VIDC_ERR,
				"Unsupported width downscale ratio %d vs %d\n",
				input_width/output_width, y_min);
			return -ENOTSUPP;
		}
	} else {
		if (output_width > y_max * input_width) {
			dprintk(VIDC_ERR,
				"Unsupported width upscale ratio %d vs %d\n",
				input_width/output_width, y_max);
			return -ENOTSUPP;
		}
	}
	return 0;
}

int msm_vidc_check_session_supported(struct msm_vidc_inst *inst)
{
	struct msm_vidc_capability *capability;
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_core *core;
	int mbs_per_frame = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_WARN, "%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}
	capability = &inst->capability;
	hdev = inst->core->device;
	core = inst->core;
	rc = msm_vidc_load_supported(inst);
	if (rc) {
		change_inst_state(inst, MSM_VIDC_CORE_INVALID);
		msm_comm_kill_session(inst);
		dprintk(VIDC_WARN,
			"%s: Hardware is overloaded\n", __func__);
		return rc;
	}

	if (!is_thermal_permissible(core)) {
		dprintk(VIDC_WARN,
			"Thermal level critical, stop all active sessions!\n");
		return -ENOTSUPP;
	}

	if (!rc) {
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
		if (!rc && inst->prop.width[CAPTURE_PORT] >
			capability->width.max) {
			dprintk(VIDC_ERR,
				"Unsupported width = %u supported max width = %u",
				inst->prop.width[CAPTURE_PORT],
				capability->width.max);
				rc = -ENOTSUPP;
		}

		if (!rc && inst->prop.height[CAPTURE_PORT] >
			capability->height.max) {
			dprintk(VIDC_ERR,
				"Unsupported height = %u supported max height = %u",
				inst->prop.height[CAPTURE_PORT],
				capability->height.max);
				rc = -ENOTSUPP;
		}

		mbs_per_frame = msm_comm_get_mbs_per_frame(inst);
		if (!rc && mbs_per_frame > capability->mbs_per_frame.max) {
			dprintk(VIDC_ERR,
			"Unsupported mbs per frame = %u, max supported is - %u\n",
			mbs_per_frame,
			capability->mbs_per_frame.max);
			rc = -ENOTSUPP;
		}
	}
	if (rc) {
		change_inst_state(inst, MSM_VIDC_CORE_INVALID);
		msm_comm_kill_session(inst);
		dprintk(VIDC_ERR,
			"%s: Resolution unsupported\n", __func__);
	}
	return rc;
}

static void msm_comm_generate_session_error(struct msm_vidc_inst *inst)
{
	enum hal_command_response cmd = HAL_SESSION_ERROR;
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
	enum hal_command_response cmd = HAL_SYS_ERROR;
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
	if ((inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_CLOSE_DONE) ||
			inst->state == MSM_VIDC_CORE_INVALID) {
		rc = msm_comm_session_abort(inst);
		if (rc == -EBUSY) {
			msm_comm_generate_sys_error(inst);
			return 0;
		} else if (rc) {
			return rc;
		}
		change_inst_state(inst, MSM_VIDC_CLOSE_DONE);
		msm_comm_generate_session_error(inst);
	} else {
		dprintk(VIDC_WARN,
				"Inactive session %pK, triggering an internal session error\n",
				inst);
		msm_comm_generate_session_error(inst);

	}

	return rc;
}

struct msm_smem *msm_comm_smem_alloc(struct msm_vidc_inst *inst,
			size_t size, u32 align, u32 flags,
			enum hal_buffer buffer_type, int map_kernel)
{
	struct msm_smem *m = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid inst: %pK\n", __func__, inst);
		return NULL;
	}
	m = msm_smem_alloc(inst->mem_client, size, align,
				flags, buffer_type, map_kernel);
	return m;
}

void msm_comm_smem_free(struct msm_vidc_inst *inst, struct msm_smem *mem)
{
	if (!inst || !inst->core || !mem) {
		dprintk(VIDC_ERR,
			"%s: invalid params: %pK %pK\n", __func__, inst, mem);
		return;
	}
	msm_smem_free(inst->mem_client, mem);
}

int msm_comm_smem_cache_operations(struct msm_vidc_inst *inst,
		struct msm_smem *mem, enum smem_cache_ops cache_ops)
{
	if (!inst || !mem) {
		dprintk(VIDC_ERR,
			"%s: invalid params: %pK %pK\n", __func__, inst, mem);
		return -EINVAL;
	}
	return msm_smem_cache_operations(inst->mem_client, mem, cache_ops);
}

struct msm_smem *msm_comm_smem_user_to_kernel(struct msm_vidc_inst *inst,
			int fd, u32 offset, enum hal_buffer buffer_type)
{
	struct msm_smem *m = NULL;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid inst: %pK\n", __func__, inst);
		return NULL;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR, "Core in Invalid state, returning from %s\n",
			__func__);
		return NULL;
	}

	m = msm_smem_user_to_kernel(inst->mem_client,
			fd, offset, buffer_type);
	return m;
}

void msm_vidc_fw_unload_handler(struct work_struct *work)
{
	struct msm_vidc_core *core = NULL;
	struct hfi_device *hdev = NULL;
	int rc = 0;

	core = container_of(work, struct msm_vidc_core, fw_unload_work.work);
	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s - invalid work or core handle\n",
				__func__);
		return;
	}

	hdev = core->device;

	mutex_lock(&core->lock);
	if (list_empty(&core->instances) &&
		core->state != VIDC_CORE_UNINIT) {
		if (core->state > VIDC_CORE_INIT) {
			dprintk(VIDC_DBG, "Calling vidc_hal_core_release\n");
			rc = call_hfi_op(hdev, core_release,
					hdev->hfi_device_data);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to release core, id = %d\n",
					core->id);
				mutex_unlock(&core->lock);
				return;
			}
		}
		core->state = VIDC_CORE_UNINIT;
		kfree(core->capabilities);
		core->capabilities = NULL;
	}
	mutex_unlock(&core->lock);
}

int msm_comm_set_color_format(struct msm_vidc_inst *inst,
		enum hal_buffer buffer_type, int fourcc)
{
	struct hal_uncompressed_format_select hal_fmt = {0};
	enum hal_uncompressed_format format = HAL_UNUSED_COLOR;
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s - invalid param\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	format = get_hal_uncompressed(fourcc);
	if (format == HAL_UNUSED_COLOR) {
		dprintk(VIDC_ERR, "Using unsupported colorformat %#x\n",
				fourcc);
		rc = -ENOTSUPP;
		goto exit;
	}

	hal_fmt.buffer_type = buffer_type;
	hal_fmt.format = format;

	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HAL_PARAM_UNCOMPRESSED_FORMAT_SELECT, &hal_fmt);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to set input color format\n");
	else
		dprintk(VIDC_DBG, "Setting uncompressed colorformat to %#x\n",
				format);

exit:
	return rc;
}

int msm_vidc_comm_s_parm(struct msm_vidc_inst *inst, struct v4l2_streamparm *a)
{
	u32 property_id = 0;
	u64 us_per_frame = 0, fps_u64 = 0;
	void *pdata;
	int rc = 0, fps = 0;
	struct hal_frame_rate frame_rate;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device || !a) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
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
			do_div(us_per_frame, a->parm.output.
				timeperframe.denominator);
			break;
		default:
			dprintk(VIDC_ERR,
					"Scale clocks : Unknown buffer type %d\n",
					a->type);
			break;
		}
	}

	if (!us_per_frame) {
		dprintk(VIDC_ERR,
				"Failed to scale clocks : time between frames is 0\n");
		rc = -EINVAL;
		goto exit;
	}

	fps_u64 = USEC_PER_SEC;
	do_div(fps_u64, us_per_frame);
	fps = fps_u64;

	if (fps % 15 == 14 || fps % 24 == 23)
		fps = fps + 1;
	else if ((fps > 1) && (fps % 24 == 1 || fps % 15 == 1))
		fps = fps - 1;

	if (inst->prop.fps != fps) {
		dprintk(VIDC_PROF, "reported fps changed for %pK: %d->%d\n",
				inst, inst->prop.fps, fps);
		inst->prop.fps = fps;
		frame_rate.frame_rate = inst->prop.fps * BIT(16);
		frame_rate.buffer_type = HAL_BUFFER_OUTPUT;
		pdata = &frame_rate;
		if (inst->session_type == MSM_VIDC_ENCODER) {
			rc = call_hfi_op(hdev, session_set_property,
				inst->session, property_id, pdata);

			if (rc)
				dprintk(VIDC_WARN,
					"Failed to set frame rate %d\n", rc);
		} else {
			msm_dcvs_init_load(inst);
		}
		msm_comm_scale_clocks_and_bus(inst);
	}
exit:
	return rc;
}
