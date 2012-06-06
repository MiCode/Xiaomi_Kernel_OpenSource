/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <mach/camera.h>
#include <media/v4l2-subdev.h>
#include "msm.h"
#include <media/msm_camera.h>
#include <media/msm_gestures.h>
#include <media/v4l2-ctrls.h>

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm_gesture: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

struct msm_gesture_ctrl {
	int queue_id;
	atomic_t active;
	struct v4l2_ctrl_handler ctrl_handler;
	int num_ctrls;
	struct v4l2_fh *p_eventHandle;
	struct v4l2_subdev *sd;
	struct msm_ges_evt event;
	int camera_opened;
};

static struct msm_gesture_ctrl g_gesture_ctrl;

int msm_gesture_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	D("%s\n", __func__);
	if (sub->type == V4L2_EVENT_ALL)
		sub->type = MSM_GES_APP_NOTIFY_EVENT;
	return v4l2_event_subscribe(fh, sub);
}

static int msm_gesture_send_ctrl(struct msm_gesture_ctrl *p_gesture_ctrl,
	int type, void *value, int length, uint32_t timeout)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s qid %d\n", __func__, p_gesture_ctrl->queue_id);
	ctrlcmd.type = type;
	ctrlcmd.timeout_ms = timeout;
	ctrlcmd.length = length;
	ctrlcmd.value = value;
	ctrlcmd.vnode_id = 0;
	ctrlcmd.queue_idx = p_gesture_ctrl->queue_id;
	ctrlcmd.config_ident = 0;

	rc = msm_server_send_ctrl(&ctrlcmd, MSM_GES_RESP_V4L2);
	return rc;
}

static int msm_gesture_proc_ctrl_cmd(struct msm_gesture_ctrl *p_gesture_ctrl,
	struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_ctrl_cmd *tmp_cmd = NULL;
	uint8_t *ctrl_data = NULL;
	void __user *uptr_cmd;
	void __user *uptr_value;
	uint32_t cmd_len = sizeof(struct msm_ctrl_cmd);
	uint32_t value_len;

	tmp_cmd = (struct msm_ctrl_cmd *)ctrl->value;
	uptr_cmd = (void __user *)ctrl->value;
	uptr_value = (void __user *)tmp_cmd->value;
	value_len = tmp_cmd->length;

	D("%s: cmd type = %d, up1=0x%x, ulen1=%d, up2=0x%x, ulen2=%d\n",
		__func__, tmp_cmd->type, (uint32_t)uptr_cmd, cmd_len,
		(uint32_t)uptr_value, tmp_cmd->length);

	ctrl_data = kzalloc(value_len + cmd_len, GFP_KERNEL);
	if (ctrl_data == 0) {
		pr_err("%s could not allocate memory\n", __func__);
		rc = -ENOMEM;
		goto end;
	}
	tmp_cmd = (struct msm_ctrl_cmd *)ctrl_data;
	if (copy_from_user((void *)ctrl_data, uptr_cmd,
			cmd_len)) {
		pr_err("%s: copy_from_user failed.\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	tmp_cmd->value = (void *)(ctrl_data + cmd_len);
	if (uptr_value && tmp_cmd->length > 0) {
		if (copy_from_user((void *)tmp_cmd->value, uptr_value,
			value_len)) {
			pr_err("%s: copy_from_user failed, size=%d\n",
				__func__, value_len);
			rc = -EINVAL;
			goto end;
		}
	} else
		tmp_cmd->value = NULL;

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_send_ctrl((struct msm_ctrl_cmd *)ctrl_data,
			MSM_GES_RESP_V4L2);
	D("%s: msm_server_control rc=%d\n", __func__, rc);
	if (rc == 0) {
		if (uptr_value && tmp_cmd->length > 0 &&
			copy_to_user((void __user *)uptr_value,
				(void *)(ctrl_data + cmd_len),
				tmp_cmd->length)) {
			pr_err("%s: copy_to_user failed, size=%d\n",
				__func__, tmp_cmd->length);
			rc = -EINVAL;
			goto end;
		}
		tmp_cmd->value = uptr_value;
		if (copy_to_user((void __user *)uptr_cmd,
			(void *)tmp_cmd, cmd_len)) {
			pr_err("%s: copy_to_user failed in cpy, size=%d\n",
				__func__, cmd_len);
			rc = -EINVAL;
			goto end;
		}
	}
end:
	D("%s: END, type = %d, vaddr = 0x%x, vlen = %d, status = %d, rc = %d\n",
		__func__, tmp_cmd->type, (uint32_t)tmp_cmd->value,
		tmp_cmd->length, tmp_cmd->status, rc);
	kfree(ctrl_data);
	return rc;
}

static int msm_gesture_s_ctrl(struct v4l2_subdev *sd,
	struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_gesture_ctrl *p_gesture_ctrl = &g_gesture_ctrl;
	D("%s ctrl->id %d\n", __func__, ctrl->id);
	rc = msm_gesture_proc_ctrl_cmd(p_gesture_ctrl, ctrl);
	if (rc != 0) {
		pr_err("%s set ctrl failed %d\n", __func__, rc);
		return -EINVAL;
	}
	return rc;
}

static int msm_gesture_s_ctrl_ops(struct v4l2_ctrl *ctrl)
{
	int rc = 0;
	struct v4l2_control control;
	struct msm_gesture_ctrl *p_gesture_ctrl = &g_gesture_ctrl;
	control.id = ctrl->id;
	control.value = ctrl->val;
	D("%s ctrl->id 0x%x\n", __func__, ctrl->id);
	rc = msm_gesture_proc_ctrl_cmd(p_gesture_ctrl, &control);
	if (rc != 0) {
		pr_err("%s proc ctrl failed %d\n", __func__, rc);
		return -EINVAL;
	}
	return rc;
}

static int msm_gesture_s_ctrl_ext(struct v4l2_subdev *sd,
	struct v4l2_ext_controls *ctrls)
{
	int rc = 0;
	struct v4l2_control control;
	struct msm_gesture_ctrl *p_gesture_ctrl = &g_gesture_ctrl;
	if ((ctrls->count < 1) || (NULL == ctrls->controls)) {
		pr_err("%s invalid ctrl failed\n", __func__);
		return -EINVAL;
	}
	control.id = ctrls->controls->id;
	control.value = ctrls->controls->value;
	D("%s ctrl->id %d\n", __func__, control.id);
	rc = msm_gesture_proc_ctrl_cmd(p_gesture_ctrl, &control);
	if (rc != 0) {
		pr_err("%s proc ctrl failed %d\n", __func__, rc);
		return -EINVAL;
	}
	return rc;
}

static int msm_gesture_handle_event(struct v4l2_subdev *sd,
	struct msm_gesture_ctrl *p_gesture_ctrl, void* arg)
{
	int rc = 0;
	struct v4l2_event *evt = (struct v4l2_event *)arg;
	struct msm_ges_evt *p_ges_evt = NULL;
	D("%s: Received gesture evt 0x%x ", __func__, evt->type);
	p_gesture_ctrl->event.evt_len = 0;
	p_gesture_ctrl->event.evt_data = NULL;

	p_ges_evt = (struct msm_ges_evt *)evt->u.data;
	D("%s: event data %p len %d", __func__,
		p_ges_evt->evt_data,
		p_ges_evt->evt_len);

	if (p_ges_evt->evt_len > 0) {
		p_gesture_ctrl->event.evt_data =
			kzalloc(p_ges_evt->evt_len, GFP_KERNEL);

		if (NULL == p_gesture_ctrl->event.evt_data) {
			pr_err("%s: cannot allocate event", __func__);
			rc = -ENOMEM;
		} else {
			if (copy_from_user(
				(void *)p_gesture_ctrl->event.evt_data,
				(void __user *)p_ges_evt->evt_data,
				p_ges_evt->evt_len)) {
				pr_err("%s: copy_from_user failed",
					__func__);
				rc = -EFAULT;
			} else {
				D("%s: copied the event", __func__);
				p_gesture_ctrl->event.evt_len =
					p_ges_evt->evt_len;
			}
		}
	}

	if (rc == 0) {
		ktime_get_ts(&evt->timestamp);
		v4l2_event_queue(&sd->devnode, evt);
	}
	D("%s: exit rc %d ", __func__, rc);
	return rc;
}

static int msm_gesture_get_evt_payload(struct v4l2_subdev *sd,
	struct msm_gesture_ctrl *p_gesture_ctrl, void* arg)
{
	int rc = 0;
	struct msm_ges_evt *p_ges_evt = (struct msm_ges_evt *)arg;
	D("%s: enter ", __func__);
	if (NULL != p_gesture_ctrl->event.evt_data) {
		D("%s: event data %p len %d", __func__,
			p_gesture_ctrl->event.evt_data,
			p_gesture_ctrl->event.evt_len);

		if (copy_to_user((void __user *)p_ges_evt->evt_data,
			p_gesture_ctrl->event.evt_data,
			p_gesture_ctrl->event.evt_len)) {
			pr_err("%s: copy_to_user failed.\n", __func__);
			rc = -EFAULT;
		} else {
			D("%s: copied the event", __func__);
			p_ges_evt->evt_len = p_gesture_ctrl->event.evt_len;
		}
	}
	D("%s: exit rc %d ", __func__, rc);
	return rc;
}

static int msm_gesture_handle_cam_event(struct v4l2_subdev *sd,
	struct msm_gesture_ctrl *p_gesture_ctrl, int cam_evt)
{
	int rc = 0;
	D("%s: cam_evt %d ", __func__, cam_evt);

	if ((cam_evt != MSM_V4L2_GES_CAM_OPEN)
		&& (cam_evt != MSM_V4L2_GES_CAM_CLOSE)) {
		pr_err("%s: error invalid event %d ", __func__, cam_evt);
		return -EINVAL;
	}

	p_gesture_ctrl->camera_opened =
		(cam_evt == MSM_V4L2_GES_CAM_OPEN);

	if (atomic_read(&p_gesture_ctrl->active) == 0) {
		D("%s gesture not active\n", __func__);
		return 0;
	}

	rc = msm_gesture_send_ctrl(p_gesture_ctrl, cam_evt, NULL,
		0, 2000);
	if (rc != 0) {
		pr_err("%s gesture ctrl failed %d\n", __func__, rc);
		rc = -EINVAL;
	}
	D("%s exit rc %d\n", __func__, rc);
	return rc;
}

long msm_gesture_ioctl(struct v4l2_subdev *sd,
	 unsigned int cmd, void *arg)
{
	int rc = 0;
	struct msm_gesture_ctrl *p_gesture_ctrl = &g_gesture_ctrl;
	D("%s\n", __func__);
	switch (cmd) {
	case MSM_GES_IOCTL_CTRL_COMMAND: {
		struct v4l2_control *ctrl = (struct v4l2_control *)arg;
		D("%s MSM_GES_IOCTL_CTRL_COMMAND arg %p size %d\n", __func__,
			arg, sizeof(ctrl));
		rc = msm_gesture_s_ctrl(sd, ctrl);
		break;
	}
	case VIDIOC_MSM_GESTURE_EVT: {
		rc = msm_gesture_handle_event(sd, p_gesture_ctrl, arg);
		break;
	}
	case VIDIOC_MSM_GESTURE_CAM_EVT: {
		int cam_evt = *((int *)arg);
		rc = msm_gesture_handle_cam_event(sd, p_gesture_ctrl, cam_evt);
		break;
	}
	case MSM_GES_GET_EVT_PAYLOAD: {
		rc = msm_gesture_get_evt_payload(sd, p_gesture_ctrl, arg);
		break;
	}
	default:
		pr_err("%s: Invalid ioctl %d", __func__, cmd);
		break;
	}
	D("%s exit rc %d\n", __func__, rc);
	return rc;
}

static const struct v4l2_ctrl_ops msm_gesture_ctrl_ops = {
	.s_ctrl = msm_gesture_s_ctrl_ops,
};

static const struct v4l2_ctrl_config msm_gesture_ctrl_filter = {
	.ops = &msm_gesture_ctrl_ops,
	.id = MSM_GESTURE_CID_CTRL_CMD,
	.name = "Gesture ctrl",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.flags = V4L2_CTRL_FLAG_SLIDER,
	.max = 0x7fffffff,
	.step = 1,
	.min = 0x80000000,
};

static int msm_gesture_init_ctrl(struct v4l2_subdev *sd,
	struct msm_gesture_ctrl *p_gesture_ctrl)
{
	int rc = 0;
	p_gesture_ctrl->num_ctrls = 1;
	p_gesture_ctrl->ctrl_handler.error = 0;
	v4l2_ctrl_handler_init(&p_gesture_ctrl->ctrl_handler,
		p_gesture_ctrl->num_ctrls);
	v4l2_ctrl_new_custom(&p_gesture_ctrl->ctrl_handler,
		&msm_gesture_ctrl_filter, p_gesture_ctrl);
	if (p_gesture_ctrl->ctrl_handler.error) {
		int err = p_gesture_ctrl->ctrl_handler.error;
		D("%s: error adding control %d", __func__, err);
		p_gesture_ctrl->ctrl_handler.error = 0;
	}
	sd->ctrl_handler = &p_gesture_ctrl->ctrl_handler;
	return rc;
}

static int msm_gesture_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rc = 0, rc_err = 0;
	struct msm_gesture_ctrl *p_gesture_ctrl = &g_gesture_ctrl;
	D("%s\n", __func__);
	if (atomic_read(&p_gesture_ctrl->active) != 0) {
		pr_err("%s already opened\n", __func__);
		return -EINVAL;
	}
	memset(&p_gesture_ctrl->event, 0x0, sizeof(struct msm_ges_evt));
	rc = msm_server_open_client(&p_gesture_ctrl->queue_id);
	if (rc != 0) {
		pr_err("%s open failed %d\n", __func__, rc);
		rc = -EINVAL;
		goto err;
	}

	rc = msm_gesture_init_ctrl(sd, p_gesture_ctrl);
	if (rc != 0) {
		pr_err("%s init ctrl failed %d\n", __func__, rc);
		rc = -EINVAL;
		goto err;
	}

	rc = msm_gesture_send_ctrl(p_gesture_ctrl, MSM_V4L2_GES_OPEN, NULL,
		0, 10000);
	if (rc != 0) {
		pr_err("%s gesture ctrl failed %d\n", __func__, rc);
		rc = -EINVAL;
		goto err;
	}

	atomic_inc(&p_gesture_ctrl->active);

	return rc;

err:
	rc_err = msm_server_close_client(p_gesture_ctrl->queue_id);
	if (rc_err != 0)
		pr_err("%s failed %d\n", __func__, rc);
	return rc;
}

static int msm_gesture_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct msm_gesture_ctrl *p_gesture_ctrl = &g_gesture_ctrl;
	D("%s\n", __func__);
	if (atomic_read(&p_gesture_ctrl->active) == 0) {
		pr_err("%s already closed\n", __func__);
		return -EINVAL;
	}

	rc = msm_gesture_send_ctrl(p_gesture_ctrl, MSM_V4L2_GES_CLOSE, NULL,
		0, 10000);
	if (rc != 0)
		pr_err("%s gesture ctrl failed %d\n", __func__, rc);

	rc = msm_server_close_client(p_gesture_ctrl->queue_id);
	if (rc != 0)
		pr_err("%s failed %d\n", __func__, rc);

	v4l2_ctrl_handler_free(&p_gesture_ctrl->ctrl_handler);
	kfree(p_gesture_ctrl->event.evt_data);

	atomic_dec(&p_gesture_ctrl->active);
	g_gesture_ctrl.queue_id = -1;
	return 0;
}

static struct v4l2_subdev_core_ops msm_gesture_core_ops = {
	.s_ctrl = msm_gesture_s_ctrl,
	.s_ext_ctrls = msm_gesture_s_ctrl_ext,
	.ioctl = msm_gesture_ioctl,
	.subscribe_event = msm_gesture_subscribe_event,
};

static struct v4l2_subdev_video_ops msm_gesture_video_ops;

static struct v4l2_subdev_ops msm_gesture_subdev_ops = {
	.core = &msm_gesture_core_ops,
	.video  = &msm_gesture_video_ops,
};

static const struct v4l2_subdev_internal_ops msm_gesture_internal_ops = {
	.open = msm_gesture_open,
	.close = msm_gesture_close,
};

static int msm_gesture_node_register(void)
{
	struct msm_gesture_ctrl *p_gesture_ctrl = &g_gesture_ctrl;
	struct v4l2_subdev *gesture_subdev =
		kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	D("%s\n", __func__);
	if (!gesture_subdev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	};

	v4l2_subdev_init(gesture_subdev, &msm_gesture_subdev_ops);
	gesture_subdev->internal_ops = &msm_gesture_internal_ops;
	gesture_subdev->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(gesture_subdev->name,
			 sizeof(gesture_subdev->name), "gesture");

	media_entity_init(&gesture_subdev->entity, 0, NULL, 0);
	gesture_subdev->entity.type = MEDIA_ENT_T_DEVNODE_V4L;
	gesture_subdev->entity.group_id = GESTURE_DEV;
	gesture_subdev->entity.name = gesture_subdev->name;

	/* events */
	gesture_subdev->flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	gesture_subdev->nevents = MAX_GES_EVENTS;

	msm_cam_register_subdev_node(gesture_subdev, GESTURE_DEV, 0);

	gesture_subdev->entity.revision = gesture_subdev->devnode.num;

	atomic_set(&p_gesture_ctrl->active, 0);
	p_gesture_ctrl->queue_id = -1;
	p_gesture_ctrl->event.evt_data = NULL;
	p_gesture_ctrl->event.evt_len = 0;
	return 0;
}

static int __init msm_gesture_init_module(void)
{
	return msm_gesture_node_register();
}

module_init(msm_gesture_init_module);
MODULE_DESCRIPTION("MSM Gesture driver");
MODULE_LICENSE("GPL v2");
