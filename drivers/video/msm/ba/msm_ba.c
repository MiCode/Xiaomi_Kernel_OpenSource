/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/msm_ba.h>

#include "msm_ba_internal.h"
#include "msm_ba_debug.h"
#include "msm_ba_common.h"

#define MSM_BA_DEV_NAME "msm_ba_8064"

#define MSM_BA_MAX_EVENTS			10

int msm_ba_poll(void *instance, struct file *filp,
		struct poll_table_struct *wait)
{
	struct msm_ba_inst *inst = instance;
	int rc = 0;

	if (!inst)
		return -EINVAL;

	poll_wait(filp, &inst->event_handler.wait, wait);
	if (v4l2_event_pending(&inst->event_handler))
		rc |= POLLPRI;

	return rc;
}
EXPORT_SYMBOL(msm_ba_poll);

int msm_ba_querycap(void *instance, struct v4l2_capability *cap)
{
	struct msm_ba_inst *inst = instance;

	if (!inst || !cap) {
		dprintk(BA_ERR,
			"Invalid input, inst = 0x%pK, cap = 0x%pK", inst, cap);
		return -EINVAL;
	}

	strlcpy(cap->driver, MSM_BA_DRV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, MSM_BA_DEV_NAME, sizeof(cap->card));
	cap->bus_info[0] = 0;
	cap->version = MSM_BA_VERSION;
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
		V4L2_CAP_STREAMING;
	memset(cap->reserved, 0x00, sizeof(cap->reserved));

	return 0;
}
EXPORT_SYMBOL(msm_ba_querycap);

int msm_ba_g_priority(void *instance, enum v4l2_priority *prio)
{
	struct msm_ba_inst *inst = instance;
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;

	if (!inst || !prio) {
		dprintk(BA_ERR,
			"Invalid prio, inst = 0x%pK, prio = 0x%pK", inst, prio);
		return -EINVAL;
	}

	ba_input = msm_ba_find_input(inst->sd_input.index);
	if (!ba_input) {
		dprintk(BA_ERR, "Could not find input index: %d",
				inst->sd_input.index);
		return -EINVAL;
	}
	*prio = ba_input->prio;

	return rc;
}
EXPORT_SYMBOL(msm_ba_g_priority);

int msm_ba_s_priority(void *instance, enum v4l2_priority prio)
{
	struct msm_ba_inst *inst = instance;
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;

	dprintk(BA_DBG, "Enter %s, prio: %d", __func__, prio);

	if (!inst)
		return -EINVAL;

	ba_input = msm_ba_find_input(inst->sd_input.index);
	if (!ba_input) {
		dprintk(BA_ERR, "Could not find input index: %d",
				inst->sd_input.index);
		return -EINVAL;
	}
	ba_input->prio = prio;
	inst->input_prio = prio;

	return rc;
}
EXPORT_SYMBOL(msm_ba_s_priority);

int msm_ba_s_parm(void *instance, struct v4l2_streamparm *a)
{
	struct msm_ba_inst *inst = instance;

	if (!inst || !a)
		return -EINVAL;

	return -EINVAL;
}
EXPORT_SYMBOL(msm_ba_s_parm);

int msm_ba_enum_input(void *instance, struct v4l2_input *input)
{
	struct msm_ba_input *ba_input = NULL;
	struct msm_ba_inst *inst = instance;
	int rc = 0;

	if (!inst || !input)
		return -EINVAL;

	if (input->index >= inst->dev_ctxt->num_inputs)
		return -EINVAL;

	ba_input = msm_ba_find_input(input->index);
	if (ba_input) {
		input->type = V4L2_INPUT_TYPE_CAMERA;
		input->std = V4L2_STD_ALL;
		strlcpy(input->name, ba_input->name, sizeof(input->name));
		if (BA_INPUT_HDMI == ba_input->input_type ||
			BA_INPUT_MHL == ba_input->input_type)
			input->capabilities = V4L2_IN_CAP_CUSTOM_TIMINGS;
		else
			input->capabilities = V4L2_IN_CAP_STD;
		dprintk(BA_DBG, "msm_ba_find_input: name %s", input->name);
	}
	return rc;
}
EXPORT_SYMBOL(msm_ba_enum_input);

int msm_ba_g_input(void *instance, unsigned int *index)
{
	struct msm_ba_inst *inst = instance;
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;

	if (!inst || !index)
		return -EINVAL;

	do {
		/* First find current input */
		ba_input = msm_ba_find_input(inst->sd_input.index);
		if (ba_input) {
			if (BA_INPUT_USERTYPE_KERNEL ==
				ba_input->input_user_type) {
				inst->sd_input.index++;
				continue;
			}
			break;
		}
	} while (ba_input);

	if (ba_input)
		*index = inst->sd_input.index;
	else
		rc = -ENOENT;

	return rc;
}
EXPORT_SYMBOL(msm_ba_g_input);

int msm_ba_s_input(void *instance, unsigned int index)
{
	struct msm_ba_inst *inst = instance;
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;
	int rc_sig = 0;

	if (!inst)
		return -EINVAL;
	if (index > inst->dev_ctxt->num_inputs)
		return -EINVAL;

	/* Find requested input */
	ba_input = msm_ba_find_input(index);
	if (!ba_input) {
		dprintk(BA_ERR, "Could not find input index: %d", index);
		return -EINVAL;
	}
	if (!ba_input->sd) {
		dprintk(BA_ERR, "No sd registered");
		return -EINVAL;
	}
	if (ba_input->in_use &&
		ba_input->prio == V4L2_PRIORITY_RECORD &&
		ba_input->prio != inst->input_prio) {
		dprintk(BA_WARN, "Input %d in use", index);
		return -EBUSY;
	}
	if (ba_input->ba_out_in_use) {
		if (inst->ext_ops) {
			if (inst->restore) {
				dprintk(BA_DBG, "Stream off in set input: %d",
					ba_input->bridge_chip_ip);
				rc_sig = v4l2_subdev_call(ba_input->sd,
							video, s_stream, 0);
				if (rc_sig)
					dprintk(BA_ERR,
					"%s: Error in stream off. rc_sig %d",
					__func__, rc_sig);
			}
		} else {
			dprintk(BA_WARN, "Sd %d in use", ba_input->ba_out);
			return -EBUSY;
		}
	}
	rc = v4l2_subdev_call(ba_input->sd, video, s_routing,
			ba_input->bridge_chip_ip, 0, 0);
	if (rc) {
		dprintk(BA_ERR, "Error: %d setting input: %d",
			rc, ba_input->bridge_chip_ip);
		return rc;
	}
	msm_ba_reset_ip_in_use_from_sd(ba_input->sd);
	inst->sd_input.index = index;
	strlcpy(inst->sd_input.name, ba_input->name,
		sizeof(inst->sd_input.name));
	inst->sd = ba_input->sd;
	ba_input->in_use = 1;
	/* get current signal status */
	rc_sig = v4l2_subdev_call(
		ba_input->sd, video, g_input_status, &ba_input->signal_status);
	dprintk(BA_DBG, "Set input %s : %d - signal status: %d",
		ba_input->name, index, ba_input->signal_status);
	if (!rc_sig && !ba_input->signal_status) {
		struct v4l2_event sd_event = {
			.id = 0,
			.type = V4L2_EVENT_MSM_BA_SIGNAL_IN_LOCK};
		int *ptr = (int *)sd_event.u.data;
		ptr[0] = index;
		ptr[1] = ba_input->signal_status;
		msm_ba_queue_v4l2_event(inst, &sd_event);
	}
	return rc;
}
EXPORT_SYMBOL(msm_ba_s_input);

int msm_ba_enum_output(void *instance, struct v4l2_output *output)
{
	struct msm_ba_input *ba_input = NULL;
	struct msm_ba_inst *inst = instance;
	int rc = 0;

	if (!inst || !output)
		return -EINVAL;

	ba_input = msm_ba_find_output(output->index);
	if (!ba_input)
		return -EINVAL;
	output->type = V4L2_OUTPUT_TYPE_ANALOG;
	output->std = V4L2_STD_ALL;
	strlcpy(output->name, ba_input->sd->name, sizeof(output->name));
	output->capabilities = V4L2_OUT_CAP_STD;

	return rc;
}
EXPORT_SYMBOL(msm_ba_enum_output);

int msm_ba_g_output(void *instance, unsigned int *index)
{
	struct msm_ba_inst *inst = instance;
	int rc = 0;

	if (!inst || !index)
		return -EINVAL;

	*index = inst->sd_output.index;

	return rc;
}
EXPORT_SYMBOL(msm_ba_g_output);

int msm_ba_s_output(void *instance, unsigned int index)
{
	struct msm_ba_inst *inst = instance;
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;

	if (!inst)
		return -EINVAL;

	ba_input = msm_ba_find_output(index);
	if (ba_input) {
		if (!ba_input->sd) {
			dprintk(BA_ERR, "No sd registered");
			return -EINVAL;
		}
		ba_input->ba_out = index;
		inst->sd_output.index = index;
		inst->sd = ba_input->sd;
		inst->sd_input.index = ba_input->ba_ip_idx;
	} else {
		dprintk(BA_ERR, "Could not find output index: %d", index);
		rc = -EINVAL;
	}
	return rc;
}
EXPORT_SYMBOL(msm_ba_s_output);

int msm_ba_enum_fmt(void *instance, struct v4l2_fmtdesc *f)
{
	struct msm_ba_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	return -EINVAL;
}
EXPORT_SYMBOL(msm_ba_enum_fmt);

int msm_ba_s_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_ba_inst *inst = instance;

	if (!inst || !f)
		return -EINVAL;

	return -EINVAL;
}
EXPORT_SYMBOL(msm_ba_s_fmt);

int msm_ba_g_fmt(void *instance, struct v4l2_format *f)
{
	struct msm_ba_inst *inst = instance;
	struct v4l2_subdev *sd = NULL;
	struct msm_ba_input *ba_input = NULL;
	v4l2_std_id new_std = V4L2_STD_UNKNOWN;
	struct v4l2_dv_timings sd_dv_timings;
	struct v4l2_mbus_framefmt sd_mbus_fmt;
	int rc = 0;

	if (!inst || !f)
		return -EINVAL;

	sd = inst->sd;
	if (!sd) {
		dprintk(BA_ERR, "No sd registered");
		return -EINVAL;
	}
	ba_input = msm_ba_find_input(inst->sd_input.index);
	if (!ba_input) {
		dprintk(BA_ERR, "Could not find input index: %d",
				inst->sd_input.index);
		return -EINVAL;
	}
	if (BA_INPUT_HDMI != ba_input->input_type) {
		rc = v4l2_subdev_call(sd, video, querystd, &new_std);
		if (rc) {
			dprintk(BA_ERR, "querystd failed %d for sd: %s",
				rc, sd->name);
			return -EINVAL;
		}
		inst->sd_input.std = new_std;
	} else {
		rc = v4l2_subdev_call(sd, video, g_dv_timings, &sd_dv_timings);
		if (rc) {
			dprintk(BA_ERR, "g_dv_timings failed %d for sd: %s",
				rc, sd->name);
			return -EINVAL;
		}
	}

	rc = v4l2_subdev_call(sd, video, g_mbus_fmt, &sd_mbus_fmt);
	if (rc) {
		dprintk(BA_ERR, "g_mbus_fmt failed %d for sd: %s",
				rc, sd->name);
	} else {
		f->fmt.pix.height = sd_mbus_fmt.height;
		f->fmt.pix.width = sd_mbus_fmt.width;
		switch (sd_mbus_fmt.code) {
		case V4L2_MBUS_FMT_YUYV8_2X8:
			f->fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
			break;
		case V4L2_MBUS_FMT_YVYU8_2X8:
			f->fmt.pix.pixelformat = V4L2_PIX_FMT_YVYU;
			break;
		case V4L2_MBUS_FMT_VYUY8_2X8:
			f->fmt.pix.pixelformat = V4L2_PIX_FMT_VYUY;
			break;
		case V4L2_MBUS_FMT_UYVY8_2X8:
			f->fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
			break;
		default:
			dprintk(BA_ERR, "Unknown sd_mbus_fmt.code 0x%x",
				sd_mbus_fmt.code);
			f->fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
			break;
		}
	}
	return rc;
}
EXPORT_SYMBOL(msm_ba_g_fmt);

int msm_ba_s_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_ba_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	return v4l2_s_ctrl(NULL, &inst->ctrl_handler, control);
}
EXPORT_SYMBOL(msm_ba_s_ctrl);

int msm_ba_g_ctrl(void *instance, struct v4l2_control *control)
{
	struct msm_ba_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	return v4l2_g_ctrl(&inst->ctrl_handler, control);
}
EXPORT_SYMBOL(msm_ba_g_ctrl);

int msm_ba_s_ext_ctrl(void *instance, struct v4l2_ext_controls *control)
{
	struct msm_ba_inst *inst = instance;

	if (!inst || !control)
		return -EINVAL;

	return -EINVAL;
}
EXPORT_SYMBOL(msm_ba_s_ext_ctrl);

int msm_ba_streamon(void *instance, enum v4l2_buf_type i)
{
	struct msm_ba_inst *inst = instance;
	struct v4l2_subdev *sd = NULL;
	int rc = 0;

	if (!inst)
		return -EINVAL;

	sd = inst->sd;
	if (!sd) {
		dprintk(BA_ERR, "No sd registered");
		return -EINVAL;
	}
	rc = v4l2_subdev_call(sd, video, s_stream, 1);
	if (rc)
		dprintk(BA_ERR, "Stream on failed on input: %d",
			inst->sd_input.index);
	else
		msm_ba_set_out_in_use(sd, 1);

	dprintk(BA_DBG, "Stream on: %s : %d",
		inst->sd_input.name, inst->sd_input.index);

	return rc;
}
EXPORT_SYMBOL(msm_ba_streamon);

int msm_ba_streamoff(void *instance, enum v4l2_buf_type i)
{
	struct msm_ba_inst *inst = instance;
	struct v4l2_subdev *sd = NULL;
	int rc = 0;

	if (!inst)
		return -EINVAL;

	sd = inst->sd;
	if (!sd) {
		dprintk(BA_ERR, "No sd registered");
		return -EINVAL;
	}
	rc = v4l2_subdev_call(sd, video, s_stream, 0);
	if (rc)
		dprintk(BA_ERR, "Stream off failed on input: %d",
			inst->sd_input.index);

	dprintk(BA_DBG, "Stream off: %s : %d",
		inst->sd_input.name, inst->sd_input.index);
	msm_ba_set_out_in_use(sd, 0);
	return rc;
}
EXPORT_SYMBOL(msm_ba_streamoff);

long msm_ba_private_ioctl(void *instance, int cmd, void *arg)
{
	long rc = 0;
	struct msm_ba_inst *inst = instance;
	struct v4l2_subdev *sd = NULL;
	int *s_ioctl = arg;

	dprintk(BA_DBG, "Enter %s with command: 0x%x", __func__, cmd);

	if (!inst)
		return -EINVAL;

	switch (cmd) {
	case VIDIOC_HDMI_RX_CEC_S_LOGICAL: {
		dprintk(BA_DBG, "VIDIOC_HDMI_RX_CEC_S_LOGICAL");
		sd = inst->sd;
		if (!sd) {
			dprintk(BA_ERR, "No sd registered");
			return -EINVAL;
		}
		if (s_ioctl) {
			rc = v4l2_subdev_call(sd, core, ioctl, cmd, s_ioctl);
			if (rc)
				dprintk(BA_ERR, "%s failed: %ld on cmd: 0x%x",
					__func__, rc, cmd);
		} else {
			dprintk(BA_ERR, "%s: NULL argument provided", __func__);
			rc = -EINVAL;
		}
	}
		break;
	case VIDIOC_HDMI_RX_CEC_CLEAR_LOGICAL: {
		dprintk(BA_DBG, "VIDIOC_HDMI_RX_CEC_CLEAR_LOGICAL");
		sd = inst->sd;
		if (!sd) {
			dprintk(BA_ERR, "No sd registered");
			return -EINVAL;
		}
		rc = v4l2_subdev_call(sd, core, ioctl, cmd, s_ioctl);
		if (rc)
			dprintk(BA_ERR, "%s failed: %ld on cmd: 0x%x",
				__func__, rc, cmd);
	}
		break;
	case VIDIOC_HDMI_RX_CEC_G_PHYSICAL: {
		dprintk(BA_DBG, "VIDIOC_HDMI_RX_CEC_G_PHYSICAL");
		sd = inst->sd;
		if (!sd) {
			dprintk(BA_ERR, "No sd registered");
			return -EINVAL;
		}
		if (s_ioctl) {
			rc = v4l2_subdev_call(sd, core, ioctl, cmd, s_ioctl);
			if (rc)
				dprintk(BA_ERR, "%s failed: %ld on cmd: 0x%x",
					__func__, rc, cmd);
		} else {
			dprintk(BA_ERR, "%s: NULL argument provided", __func__);
			rc = -EINVAL;
		}
	}
		break;
	case VIDIOC_HDMI_RX_CEC_G_CONNECTED: {
		dprintk(BA_DBG, "VIDIOC_HDMI_RX_CEC_G_CONNECTED");
		sd = inst->sd;
		if (!sd) {
			dprintk(BA_ERR, "No sd registered");
			return -EINVAL;
		}
		if (s_ioctl) {
			rc = v4l2_subdev_call(sd, core, ioctl, cmd, s_ioctl);
			if (rc)
				dprintk(BA_ERR, "%s failed: %ld on cmd: 0x%x",
					__func__, rc, cmd);
		} else {
			dprintk(BA_ERR, "%s: NULL argument provided", __func__);
			rc = -EINVAL;
		}
	}
		break;
	case VIDIOC_HDMI_RX_CEC_S_ENABLE: {
		dprintk(BA_DBG, "VIDIOC_HDMI_RX_CEC_S_ENABLE");
		sd = inst->sd;
		if (!sd) {
			dprintk(BA_ERR, "No sd registered");
			return -EINVAL;
		}
		if (s_ioctl) {
			rc = v4l2_subdev_call(sd, core, ioctl, cmd, s_ioctl);
			if (rc)
				dprintk(BA_ERR, "%s failed: %ld on cmd: 0x%x",
					__func__, rc, cmd);
		} else {
			dprintk(BA_ERR, "%s: NULL argument provided", __func__);
			rc = -EINVAL;
		}
	}
		break;
	default:
		dprintk(BA_WARN, "Not a typewriter! Command: 0x%x", cmd);
		rc = -ENOTTY;
		break;
	}
	return rc;
}
EXPORT_SYMBOL(msm_ba_private_ioctl);

int msm_ba_save_restore_input(void *instance, enum msm_ba_save_restore_ip sr)
{
	struct msm_ba_inst *inst = instance;
	struct msm_ba_input *ba_input = NULL;
	int rc = 0;

	if (!inst)
		return -EINVAL;

	if (BA_SR_RESTORE_IP == sr &&
		inst->restore) {
		dprintk(BA_DBG, "Restoring input: %d",
			inst->saved_input);
		rc = v4l2_subdev_call(inst->sd, video, s_routing,
				inst->saved_input, 0, 0);
		if (rc)
			dprintk(BA_ERR, "Failed to restore input: %d",
				inst->saved_input);
		msm_ba_reset_ip_in_use_from_sd(inst->sd);
		ba_input = msm_ba_find_input_from_sd(inst->sd,
					inst->saved_input);
		if (ba_input)
			ba_input->in_use = 1;
		else
			dprintk(BA_WARN, "Could not find input %d from sd: %s",
				inst->saved_input, inst->sd->name);
		inst->restore = 0;
		inst->saved_input = BA_IP_MAX;
		dprintk(BA_DBG, "Stream on from save restore");
		rc = msm_ba_streamon(inst, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
	} else if (BA_SR_SAVE_IP == sr) {
		ba_input = msm_ba_find_input(inst->sd_input.index);
		if (ba_input == NULL) {
			dprintk(BA_ERR, "Could not find input %d",
				inst->sd_input.index);
		} else if (ba_input->ba_out_in_use) {
			inst->restore = 1;
			inst->saved_input =
				msm_ba_find_ip_in_use_from_sd(inst->sd);
			if (inst->saved_input == BA_IP_MAX) {
				dprintk(BA_ERR, "Could not find input to save");
				inst->restore = 0;
			}
			dprintk(BA_DBG, "Saving input: %d",
				inst->saved_input);
			rc = -EBUSY;
		}
	} else {
		dprintk(BA_DBG, "Nothing to do in save and restore");
	}
	return rc;
}
EXPORT_SYMBOL(msm_ba_save_restore_input);

void msm_ba_release_subdev_node(struct video_device *vdev)
{
	struct v4l2_subdev *sd = video_get_drvdata(vdev);

	sd->devnode = NULL;
	kfree(vdev);
}

static int msm_ba_register_v4l2_subdev(struct v4l2_device *v4l2_dev,
				struct v4l2_subdev *sd)
{
	struct video_device *vdev;
	int rc = 0;

	dprintk(BA_DBG, "Enter %s: v4l2_dev 0x%pK, v4l2_subdev 0x%pK",
			  __func__, v4l2_dev, sd);
	if (NULL == v4l2_dev || NULL == sd || !sd->name[0]) {
		dprintk(BA_ERR, "Invalid input");
		return -EINVAL;
	}
	rc = v4l2_device_register_subdev(v4l2_dev, sd);
	if (rc < 0) {
		dprintk(BA_ERR,
			"%s(%d), V4L2 subdev register failed for %s rc: %d",
			__func__, __LINE__, sd->name, rc);
		return rc;
	}
	if (sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE) {
		vdev = video_device_alloc();
		if (NULL == vdev) {
			dprintk(BA_ERR, "%s Not enough memory", __func__);
			return -ENOMEM;
		}
		video_set_drvdata(vdev, sd);
		strlcpy(vdev->name, sd->name, sizeof(vdev->name));
		vdev->v4l2_dev = v4l2_dev;
		vdev->fops = &v4l2_subdev_fops;
		vdev->release = msm_ba_release_subdev_node;
		rc = __video_register_device(vdev, VFL_TYPE_SUBDEV, -1, 1,
								sd->owner);
		if (rc < 0) {
			dprintk(BA_ERR, "%s Error registering video device %s",
					__func__, sd->name);
			kfree(vdev);
		} else {
#if defined(CONFIG_MEDIA_CONTROLLER)
			sd->entity.info.v4l.major = VIDEO_MAJOR;
			sd->entity.info.v4l.minor = vdev->minor;
			sd->entity.name = video_device_node_name(vdev);
#endif
			sd->devnode = vdev;
		}
	}
	dprintk(BA_DBG, "Exit %s with rc: %d", __func__, rc);

	return rc;
}

int msm_ba_register_subdev_node(struct v4l2_subdev *sd)
{
	struct ba_ctxt *ba_ctxt;
	int rc = 0;

	ba_ctxt = msm_ba_get_ba_context();
	rc = msm_ba_register_v4l2_subdev(&ba_ctxt->dev_ctxt->v4l2_dev, sd);
	if (!rc) {
		ba_ctxt->dev_ctxt->num_ba_subdevs++;
		msm_ba_add_inputs(sd);
	}

	return rc;
}
EXPORT_SYMBOL(msm_ba_register_subdev_node);

static void __msm_ba_sd_unregister(struct v4l2_subdev *sub_dev)
{
	struct ba_ctxt *ba_ctxt;

	ba_ctxt = msm_ba_get_ba_context();
	mutex_lock(&ba_ctxt->ba_cs);

	v4l2_device_unregister_subdev(sub_dev);
	ba_ctxt->dev_ctxt->num_ba_subdevs--;
	msm_ba_del_inputs(sub_dev);

	dprintk(BA_DBG, "%s(%d), BA Unreg Sub Device : num ba devices %d : %s",
		__func__, __LINE__,
		ba_ctxt->dev_ctxt->num_ba_subdevs, sub_dev->name);

	mutex_unlock(&ba_ctxt->ba_cs);
}

int msm_ba_unregister_subdev_node(struct v4l2_subdev *sub_dev)
{
	struct ba_ctxt *ba_ctxt;

	ba_ctxt = msm_ba_get_ba_context();
	if (!ba_ctxt || !ba_ctxt->dev_ctxt)
		return -ENODEV;
	if (!sub_dev)
		return -EINVAL;
	__msm_ba_sd_unregister(sub_dev);

	return 0;
}
EXPORT_SYMBOL(msm_ba_unregister_subdev_node);

static int msm_ba_setup_event_queue(void *inst,
				struct video_device *pvdev)
{
	int rc = 0;
	struct msm_ba_inst *ba_inst = (struct msm_ba_inst *)inst;

	v4l2_fh_init(&ba_inst->event_handler, pvdev);
	v4l2_fh_add(&ba_inst->event_handler);

	return rc;
}

int msm_ba_subscribe_event(void *inst,
			const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_ba_inst *ba_inst = (struct msm_ba_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_subscribe(&ba_inst->event_handler, sub,
						MSM_BA_MAX_EVENTS, NULL);
	return rc;
}
EXPORT_SYMBOL(msm_ba_subscribe_event);

int msm_ba_unsubscribe_event(void *inst,
			const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct msm_ba_inst *ba_inst = (struct msm_ba_inst *)inst;

	if (!inst || !sub)
		return -EINVAL;

	rc = v4l2_event_unsubscribe(&ba_inst->event_handler, sub);
	return rc;
}
EXPORT_SYMBOL(msm_ba_unsubscribe_event);

void msm_ba_subdev_event_hndlr(struct v4l2_subdev *sd,
				unsigned int notification, void *arg)
{
	struct msm_ba_dev *dev_ctxt = NULL;
	struct msm_ba_input *ba_input;
	struct msm_ba_sd_event *ba_sd_event;
	int bridge_chip_ip;

	if (!sd || !arg) {
		dprintk(BA_ERR, "%s null v4l2 subdev or arg", __func__);
		return;
	}

	bridge_chip_ip = ((int *)((struct v4l2_event *)arg)->u.data)[0];
	ba_input = msm_ba_find_input_from_sd(sd, bridge_chip_ip);
	if (!ba_input) {
		dprintk(BA_WARN, "Could not find input %d from sd: %s",
			bridge_chip_ip, sd->name);
		return;
	}

	ba_sd_event = kzalloc(sizeof(*ba_sd_event), GFP_KERNEL);
	if (!ba_sd_event) {
		dprintk(BA_ERR, "%s out of memory", __func__);
		return;
	}

	dev_ctxt = get_ba_dev();

	ba_sd_event->sd_event = *(struct v4l2_event *)arg;
	((int *)ba_sd_event->sd_event.u.data)[0] = ba_input->ba_ip_idx;
	mutex_lock(&dev_ctxt->dev_cs);
	list_add_tail(&ba_sd_event->list, &dev_ctxt->sd_events);
	mutex_unlock(&dev_ctxt->dev_cs);

	schedule_delayed_work(&dev_ctxt->sd_events_work, 0);
}

void *msm_ba_open(const struct msm_ba_ext_ops *ext_ops)
{
	struct msm_ba_inst *inst = NULL;
	struct msm_ba_dev *dev_ctxt = NULL;
	int rc = 0;

	dev_ctxt = get_ba_dev();

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);

	if (!inst) {
		dprintk(BA_ERR, "Failed to allocate memory");
		return NULL;
	}

	mutex_init(&inst->inst_cs);

	init_waitqueue_head(&inst->kernel_event_queue);
	inst->state = MSM_BA_DEV_UNINIT_DONE;
	inst->dev_ctxt = dev_ctxt;
	rc = msm_ba_ctrl_init(inst);
	if (rc) {
		dprintk(BA_WARN, "Failed to initialize controls: %d", rc);
		msm_ba_ctrl_deinit(inst);
	}

	if (!list_empty(&(inst->dev_ctxt->v4l2_dev.subdevs)))
		inst->sd = list_first_entry(&(inst->dev_ctxt->v4l2_dev.subdevs),
			struct v4l2_subdev, list);

	msm_ba_setup_event_queue(inst, dev_ctxt->vdev);

	mutex_lock(&dev_ctxt->dev_cs);
	list_add_tail(&inst->list, &dev_ctxt->instances);
	mutex_unlock(&dev_ctxt->dev_cs);

	dev_ctxt->state = BA_DEV_INIT;
	dev_ctxt->state = BA_DEV_INIT_DONE;
	inst->state = MSM_BA_DEV_INIT_DONE;
	inst->sd_input.index = 0;
	inst->input_prio = V4L2_PRIORITY_DEFAULT;

	inst->debugfs_root =
		msm_ba_debugfs_init_inst(inst, dev_ctxt->debugfs_root);

	inst->ext_ops = ext_ops;

	return inst;
}
EXPORT_SYMBOL(msm_ba_open);

int msm_ba_close(void *instance)
{
	struct msm_ba_inst *inst = instance;
	struct msm_ba_inst *temp;
	struct msm_ba_dev *dev_ctxt;
	struct list_head *ptr;
	struct list_head *next;
	int rc = 0;

	if (!inst)
		return -EINVAL;

	dev_ctxt = inst->dev_ctxt;
	mutex_lock(&dev_ctxt->dev_cs);

	list_for_each_safe(ptr, next, &dev_ctxt->instances) {
		temp = list_entry(ptr, struct msm_ba_inst, list);
		if (temp == inst)
			list_del(&inst->list);
	}
	mutex_unlock(&dev_ctxt->dev_cs);

	msm_ba_ctrl_deinit(inst);

	v4l2_fh_del(&inst->event_handler);
	v4l2_fh_exit(&inst->event_handler);

	debugfs_remove_recursive(inst->debugfs_root);

	dprintk(BA_DBG, "Closed BA instance: %pK", inst);
	kfree(inst);

	return rc;
}
EXPORT_SYMBOL(msm_ba_close);
