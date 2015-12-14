/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#include "msm_ba_debug.h"
#include "msm_ba_common.h"

struct msm_ba_input_config msm_ba_inp_cfg[] = {
	/* type, index, name, adv inp, dev id, sd name, signal status */
	{BA_INPUT_CVBS, 0, "CVBS-0", BA_IP_CVBS_0, 0, "adv7180", 1},
#ifdef CONFIG_MSM_S_PLATFORM
	{BA_INPUT_CVBS, 1, "CVBS-1", BA_IP_CVBS_0, 0, "adv7180", 1},
#else
	{BA_INPUT_CVBS, 1, "CVBS-1", BA_IP_CVBS_0, 1, "adv7180", 1},
	{BA_INPUT_CVBS, 2, "CVBS-2", BA_IP_CVBS_1, 1, "adv7180", 1},
#endif
	{BA_INPUT_HDMI, 1, "HDMI-1", BA_IP_HDMI_1, 2, "adv7481", 1},
};

static struct msm_ba_ctrl msm_ba_ctrls[] = {
	{
		.id = MSM_BA_PRIV_SD_NODE_ADDR,
		.name = "Sub-device Node Address",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 16,
		.default_value = 0,
		.step = 2,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
	},
	{
		.id = MSM_BA_PRIV_FPS,
		.name = "FPS in Q16 format",
		.type = V4L2_CTRL_TYPE_INTEGER,
		.minimum = 0,
		.maximum = 0x7fffffff,
		.default_value = 60 << 16,
		.step = 1,
		.menu_skip_mask = 0,
		.flags = V4L2_CTRL_FLAG_VOLATILE,
		.qmenu = NULL,
	},
};

#define BA_NUM_CTRLS ARRAY_SIZE(msm_ba_ctrls)

/* Assuming den is not zero, max 32 bits */
#define BA_FRAC_TO_Q16(q, num, den)	{	\
		uint32_t pwr;	\
		pwr = ilog2(den);	\
		(q) = (num) << (16 - pwr);	\
	}

struct msm_ba_dev *get_ba_dev(void)
{
	struct ba_ctxt *ba_ctxt;
	struct msm_ba_dev *dev_ctxt = NULL;

	ba_ctxt = msm_ba_get_ba_context();

	mutex_lock(&ba_ctxt->ba_cs);
	dev_ctxt = ba_ctxt->dev_ctxt;
	mutex_unlock(&ba_ctxt->ba_cs);

	return dev_ctxt;
}

void msm_ba_queue_v4l2_event(struct msm_ba_inst *inst,
		const struct v4l2_event *sd_event)
{
	v4l2_event_queue_fh(&inst->event_handler, sd_event);
	wake_up(&inst->kernel_event_queue);
}

static void msm_ba_print_event(struct v4l2_event *sd_event)
{
	switch (sd_event->type) {
	case V4L2_EVENT_MSM_BA_SIGNAL_IN_LOCK:
		dprintk(BA_DBG, "Signal in lock for ip_idx %d",
			((int *)sd_event->u.data)[0]);
		break;
	case V4L2_EVENT_MSM_BA_SIGNAL_LOST_LOCK:
		dprintk(BA_DBG, "Signal lost lock for ip_idx %d",
			((int *)sd_event->u.data)[0]);
		break;
	case V4L2_EVENT_MSM_BA_SOURCE_CHANGE:
		dprintk(BA_DBG, "Video source change 0x%x",
			((int *)sd_event->u.data)[1]);
		break;
	case V4L2_EVENT_MSM_BA_HDMI_HPD:
		dprintk(BA_DBG, "HDMI hotplug detected!");
		break;
	case V4L2_EVENT_MSM_BA_HDMI_CEC_MESSAGE:
		dprintk(BA_DBG, "HDMI CEC message!");
		break;
	case V4L2_EVENT_MSM_BA_CP:
		dprintk(BA_DBG, "Content protection detected!");
		break;
	case V4L2_EVENT_MSM_BA_ERROR:
		dprintk(BA_DBG, "Subdev error %d!",
			((int *)sd_event->u.data)[1]);
		break;
	default:
		dprintk(BA_ERR, "Unknown event: 0x%x", sd_event->type);
		break;
	}
}

static void msm_ba_signal_sessions_event(struct v4l2_event *sd_event)
{
	struct msm_ba_inst *inst = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;
	unsigned int *ptr;

	uintptr_t arg;

	const struct v4l2_event event = {
		.id = 0,
		.type = sd_event->type,
		.u = sd_event->u};

	msm_ba_print_event(sd_event);
	dev_ctxt = get_ba_dev();
	ptr = (unsigned int *)sd_event->u.data;

	mutex_lock(&dev_ctxt->dev_cs);
	list_for_each_entry(inst, &(dev_ctxt->instances), list) {
		if (inst->ext_ops && inst->ext_ops->msm_ba_cb) {
			arg = ptr[1];
			inst->ext_ops->msm_ba_cb(
					inst, sd_event->id, (void *)arg);
		} else {
			msm_ba_queue_v4l2_event(inst, &event);
		}
	}
	mutex_unlock(&dev_ctxt->dev_cs);
}

void msm_ba_subdev_event_hndlr_delayed(struct work_struct *work)
{
	struct msm_ba_dev *dev_ctxt = NULL;
	struct msm_ba_sd_event *ba_sd_event = NULL;
	struct msm_ba_sd_event *ba_sd_event_tmp = NULL;

	dev_ctxt = get_ba_dev();

	if (!list_empty(&dev_ctxt->sd_events)) {
		list_for_each_entry_safe(ba_sd_event, ba_sd_event_tmp,
				&(dev_ctxt->sd_events), list) {
			list_del(&ba_sd_event->list);
			msm_ba_signal_sessions_event(&ba_sd_event->sd_event);
			kfree(ba_sd_event);
			break;
		}
	} else {
		dprintk(BA_ERR, "%s - queue empty!!!", __func__);
	}
}

struct v4l2_subdev *msm_ba_sd_find(const char *name)
{
	struct v4l2_subdev *sd = NULL;
	struct v4l2_subdev *sd_out = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;

	dev_ctxt = get_ba_dev();
	if (!list_empty(&(dev_ctxt->v4l2_dev.subdevs))) {
		list_for_each_entry(sd, &(dev_ctxt->v4l2_dev.subdevs), list)
			if (!strcmp(name, sd->name)) {
				sd_out = sd;
				break;
			}
	}
	return sd_out;
}

void msm_ba_add_inputs(struct v4l2_subdev *sd)
{
	struct msm_ba_input *input = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;
	int i;
	int str_length = 0;
	int rc;
	int start_index = 0;
	int end_index = 0;
	int dev_id = 0;
	int status = 0;

	dev_ctxt = get_ba_dev();
	if (!list_empty(&dev_ctxt->inputs))
		start_index = dev_ctxt->num_inputs;

	dev_id = msm_ba_inp_cfg[start_index].ba_out;
	end_index = sizeof(msm_ba_inp_cfg)/sizeof(msm_ba_inp_cfg[0]);
	for (i = start_index; i < end_index; i++) {
		str_length = strlen(msm_ba_inp_cfg[i].sd_name);
		rc = memcmp(sd->name, msm_ba_inp_cfg[i].sd_name, str_length);
		if (!rc && dev_id == msm_ba_inp_cfg[i].ba_out) {
			input = kzalloc(sizeof(*input), GFP_KERNEL);

			if (!input) {
				dprintk(BA_ERR, "Failed to allocate memory");
				break;
			}
			input->inputType = msm_ba_inp_cfg[i].inputType;
			input->name_index = msm_ba_inp_cfg[i].index;
			strlcpy(input->name, msm_ba_inp_cfg[i].name,
				sizeof(input->name));
			input->bridge_chip_ip = msm_ba_inp_cfg[i].ba_ip;
			input->ba_out = msm_ba_inp_cfg[i].ba_out;
			input->ba_ip_idx = i;
			input->prio = V4L2_PRIORITY_DEFAULT;
			input->sd = sd;
			rc = v4l2_subdev_call(
				sd, video, g_input_status, &status);
			if (rc)
				dprintk(BA_ERR,
					"g_input_status failed for sd: %s",
					sd->name);
			else
				input->signal_status = status;
			list_add_tail(&input->list, &dev_ctxt->inputs);
			dev_ctxt->num_inputs++;
			dprintk(BA_DBG, "Add input: name %s on %d",
				input->name, input->ba_out);
		}
	}
}

void msm_ba_del_inputs(struct v4l2_subdev *sd)
{
	struct msm_ba_input *input = NULL;
	struct list_head *ptr;
	struct list_head *next;
	struct msm_ba_dev *dev_ctxt = NULL;

	dev_ctxt = get_ba_dev();

	list_for_each_safe(ptr, next, &(dev_ctxt->inputs)) {
		input = list_entry(ptr, struct msm_ba_input, list);
		if (input->sd == sd) {
			list_del(&input->list);
			kfree(input);
		}
	}
}

void msm_ba_set_out_in_use(struct v4l2_subdev *sd, int on)
{
	struct msm_ba_input *input = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;

	dev_ctxt = get_ba_dev();

	if (!list_empty(&(dev_ctxt->inputs))) {
		list_for_each_entry(input, &(dev_ctxt->inputs), list)
			if (input->sd == sd)
				input->ba_out_in_use = on;
	}
}

int msm_ba_find_ip_in_use_from_sd(struct v4l2_subdev *sd)
{
	struct msm_ba_input *input = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;
	int ba_ip = BA_IP_MAX;

	dev_ctxt = get_ba_dev();

	if (!list_empty(&(dev_ctxt->inputs))) {
		list_for_each_entry(input, &(dev_ctxt->inputs), list)
			if (input->sd == sd &&
				input->in_use) {
				ba_ip = input->bridge_chip_ip;
				break;
			}
	}
	return ba_ip;
}

void msm_ba_reset_ip_in_use_from_sd(struct v4l2_subdev *sd)
{
	struct msm_ba_input *input = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;

	dev_ctxt = get_ba_dev();

	if (!list_empty(&(dev_ctxt->inputs))) {
		list_for_each_entry(input, &(dev_ctxt->inputs), list)
			if (input->sd == sd &&
				input->in_use) {
				input->in_use = 0;
				break;
			}
	}
}

struct msm_ba_input *msm_ba_find_input_from_sd(struct v4l2_subdev *sd,
			int bridge_chip_ip)
{
	struct msm_ba_input *input = NULL;
	struct msm_ba_input *input_out = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;

	dev_ctxt = get_ba_dev();

	if (!list_empty(&(dev_ctxt->inputs))) {
		list_for_each_entry(input, &(dev_ctxt->inputs), list)
			if (input->sd == sd &&
				input->bridge_chip_ip == bridge_chip_ip) {
				input_out = input;
				break;
			}
	}
	return input_out;
}

struct msm_ba_input *msm_ba_find_input(int ba_input_idx)
{
	struct msm_ba_input *input = NULL;
	struct msm_ba_input *input_out = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;

	dev_ctxt = get_ba_dev();

	if (!list_empty(&(dev_ctxt->inputs))) {
		list_for_each_entry(input, &(dev_ctxt->inputs), list)
			if (input->ba_ip_idx == ba_input_idx) {
				input_out = input;
				break;
			}
	}
	return input_out;
}

struct msm_ba_input *msm_ba_find_output(int ba_output)
{
	struct msm_ba_input *input = NULL;
	struct msm_ba_input *input_out = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;

	dev_ctxt = get_ba_dev();

	if (!list_empty(&(dev_ctxt->inputs))) {
		list_for_each_entry(input, &(dev_ctxt->inputs), list) {
			if (input->ba_out == ba_output) {
				input_out = input;
				break;
			}
		}
	}
	return input_out;
}

int msm_ba_g_fps(void *instance, int *fps_q16)
{
	struct msm_ba_inst *inst = instance;
	struct v4l2_subdev *sd = NULL;
	struct v4l2_subdev_frame_interval sd_frame_int;
	int rc = 0;

	if (!inst || !fps_q16)
		return -EINVAL;

	sd = inst->sd;
	if (!sd) {
		dprintk(BA_ERR, "No sd registered");
		return -EINVAL;
	}
	rc = v4l2_subdev_call(sd, video, g_frame_interval, &sd_frame_int);
	if (rc) {
		dprintk(BA_ERR, "get frame interval failed %d for sd: %s",
				rc, sd->name);
	} else {
		/* subdevice returns frame interval not fps! */
		if (sd_frame_int.interval.numerator) {
			BA_FRAC_TO_Q16(*fps_q16,
				sd_frame_int.interval.denominator,
				sd_frame_int.interval.numerator);
		} else {
			*fps_q16 =
				sd_frame_int.interval.denominator << 16;
		}
	}
	return rc;
}

static int msm_ba_try_get_ctrl(struct msm_ba_inst *inst,
			struct v4l2_ctrl *ctrl)
{
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;

	if (!inst) {
		dprintk(BA_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}

	dprintk(BA_DBG, "%s ctrl->id: 0x%x", __func__, ctrl->id);

	switch (ctrl->id) {
	case MSM_BA_PRIV_SD_NODE_ADDR:
		ba_input = msm_ba_find_input(inst->sd_input.index);
		if (ba_input) {
			ctrl->val = ba_input->ba_node_addr;
			dprintk(BA_DBG,
				"%s: SD NODE ADDR ctrl->id:0x%x ctrl->val:%d",
				__func__, ctrl->id, ctrl->val);
		} else {
			dprintk(BA_ERR, "%s Could not find input",
				__func__);
			rc = -EINVAL;
		}
		break;
	case MSM_BA_PRIV_FPS:
		rc = msm_ba_g_fps(inst, &ctrl->val);
		break;
	default:
		dprintk(BA_ERR, "%s id: 0x%x not supported",
			__func__, ctrl->id);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_ba_try_set_ctrl(struct msm_ba_inst *inst,
			struct v4l2_ctrl *ctrl)
{
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;

	if (!inst) {
		dprintk(BA_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}

	dprintk(BA_DBG, "%s ctrl->id: 0x%x", __func__, ctrl->id);

	switch (ctrl->id) {
	case MSM_BA_PRIV_SD_NODE_ADDR:
		ba_input = msm_ba_find_input(inst->sd_input.index);
		if (ba_input) {
			ba_input->ba_node_addr = ctrl->val;
			dprintk(BA_DBG,
				"%s: SD NODE ADDR ctrl->id:0x%x node_addr:%d",
				__func__, ctrl->id, ba_input->ba_node_addr);
		} else {
			dprintk(BA_ERR, "%s Could not find input",
				__func__);
			rc = -EINVAL;
		}
		break;
	default:
		dprintk(BA_ERR, "%s id: 0x%x not supported",
			__func__, ctrl->id);
		rc = -EINVAL;
		break;
	}
	return rc;
}

static int msm_ba_op_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	int c = 0;
	struct msm_ba_inst *inst = container_of(ctrl->handler,
				struct msm_ba_inst, ctrl_handler);
	if (!inst) {
		dprintk(BA_ERR, "%s invalid parameters", __func__);
		return -EINVAL;
	}

	for (c = 0; c < ctrl->ncontrols; ++c) {
		if (ctrl->cluster[c]->is_new) {
			rc = msm_ba_try_set_ctrl(inst, ctrl->cluster[c]);
			if (rc) {
				dprintk(BA_ERR, "Failed setting 0x%x",
						ctrl->cluster[c]->id);
				break;
			}
		}
	}
	return rc;
}

static int msm_ba_op_g_volatile_ctrl(struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	int c = 0;
	struct msm_ba_inst *inst = container_of(ctrl->handler,
				struct msm_ba_inst, ctrl_handler);
	struct v4l2_ctrl *master = ctrl->cluster[0];

	for (c = 0; c < master->ncontrols; c++) {
		if (master->cluster[c]->id == ctrl->id) {
			rc = msm_ba_try_get_ctrl(inst, ctrl);
			if (rc) {
				dprintk(BA_ERR, "Failed getting 0x%x",
					ctrl->id);
				return rc;
			}
		}
	}
	return rc;
}

static const struct v4l2_ctrl_ops msm_ba_ctrl_ops = {

	.g_volatile_ctrl = msm_ba_op_g_volatile_ctrl,
	.s_ctrl = msm_ba_op_s_ctrl,
};

const struct v4l2_ctrl_ops *msm_ba_get_ctrl_ops(void)
{
	return &msm_ba_ctrl_ops;
}

static struct v4l2_ctrl **msm_ba_get_super_cluster(struct msm_ba_inst *inst,
				int *size)
{
	int c = 0;
	int sz = 0;
	struct v4l2_ctrl **cluster = kmalloc(sizeof(struct v4l2_ctrl *) *
			BA_NUM_CTRLS, GFP_KERNEL);

	if (!size || !cluster || !inst)
		return NULL;

	for (c = 0; c < BA_NUM_CTRLS; c++)
		cluster[sz++] = inst->ctrls[c];

	*size = sz;
	return cluster;
}

/*
 * Controls init function.
 * Caller is expected to call deinit in case of failure.
 */
int msm_ba_ctrl_init(struct msm_ba_inst *inst)
{
	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg = {0};
	int rc = 0;
	int cluster_size = 0;

	if (!inst) {
		dprintk(BA_ERR, "%s - invalid instance", __func__);
		return -EINVAL;
	}

	inst->ctrls = kzalloc(sizeof(struct v4l2_ctrl *) * BA_NUM_CTRLS,
				GFP_KERNEL);
	if (!inst->ctrls) {
		dprintk(BA_ERR, "%s - failed to allocate ctrl", __func__);
		return -ENOMEM;
	}

	rc = v4l2_ctrl_handler_init(&inst->ctrl_handler, BA_NUM_CTRLS);

	if (rc) {
		dprintk(BA_ERR, "CTRL ERR: Control handler init failed, %d",
				inst->ctrl_handler.error);
		return rc;
	}
	for (; idx < BA_NUM_CTRLS; idx++) {
		struct v4l2_ctrl *ctrl = NULL;

		if (BA_IS_PRIV_CTRL(msm_ba_ctrls[idx].id)) {
			/* add private control */
			ctrl_cfg.def = msm_ba_ctrls[idx].default_value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = msm_ba_ctrls[idx].id;
			ctrl_cfg.max = msm_ba_ctrls[idx].maximum;
			ctrl_cfg.min = msm_ba_ctrls[idx].minimum;
			ctrl_cfg.menu_skip_mask =
				msm_ba_ctrls[idx].menu_skip_mask;
			ctrl_cfg.name = msm_ba_ctrls[idx].name;
			ctrl_cfg.ops = &msm_ba_ctrl_ops;
			ctrl_cfg.step = msm_ba_ctrls[idx].step;
			ctrl_cfg.type = msm_ba_ctrls[idx].type;
			ctrl_cfg.qmenu = msm_ba_ctrls[idx].qmenu;

			ctrl = v4l2_ctrl_new_custom(&inst->ctrl_handler,
					&ctrl_cfg, NULL);
		} else {
			if (msm_ba_ctrls[idx].type == V4L2_CTRL_TYPE_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(
					&inst->ctrl_handler,
					&msm_ba_ctrl_ops,
					msm_ba_ctrls[idx].id,
					msm_ba_ctrls[idx].maximum,
					msm_ba_ctrls[idx].menu_skip_mask,
					msm_ba_ctrls[idx].default_value);
			} else {
				ctrl = v4l2_ctrl_new_std(&inst->ctrl_handler,
					&msm_ba_ctrl_ops,
					msm_ba_ctrls[idx].id,
					msm_ba_ctrls[idx].minimum,
					msm_ba_ctrls[idx].maximum,
					msm_ba_ctrls[idx].step,
					msm_ba_ctrls[idx].default_value);
			}
		}

		switch (msm_ba_ctrls[idx].id) {
		case MSM_BA_PRIV_SD_NODE_ADDR:
		case MSM_BA_PRIV_FPS:
			if (ctrl)
				ctrl->flags |= msm_ba_ctrls[idx].flags;
			break;
		}

		rc = inst->ctrl_handler.error;
		if (rc) {
			dprintk(BA_ERR,
					"Error adding ctrl (%s) to ctrl handle, %d",
					msm_ba_ctrls[idx].name,
					inst->ctrl_handler.error);
			return rc;
		}

		inst->ctrls[idx] = ctrl;
	}

	/* Construct a super cluster of all controls */
	inst->cluster = msm_ba_get_super_cluster(inst, &cluster_size);
	if (!inst->cluster || !cluster_size) {
		dprintk(BA_WARN,
				"Failed to setup super cluster");
		return -EINVAL;
	}
	v4l2_ctrl_cluster(cluster_size, inst->cluster);

	return rc;
}

void msm_ba_ctrl_deinit(struct msm_ba_inst *inst)
{
	kfree(inst->ctrls);
	kfree(inst->cluster);
	v4l2_ctrl_handler_free(&inst->ctrl_handler);
}
