/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "msm_cam_server.h"
#include "msm_csid.h"
#include "msm_csic.h"
#include "msm_csiphy.h"
#include "msm_ispif.h"
#include "msm_sensor.h"
#include "msm_actuator.h"
#include "msm_vfe32.h"

#ifdef CONFIG_MSM_CAMERA_DEBUG
#define D(fmt, args...) pr_debug("msm: " fmt, ##args)
#else
#define D(fmt, args...) do {} while (0)
#endif

static struct msm_cam_server_dev g_server_dev;
static struct class *msm_class;
static dev_t msm_devno;

static long msm_server_send_v4l2_evt(void *evt);
static void msm_cam_server_subdev_notify(struct v4l2_subdev *sd,
	unsigned int notification, void *arg);

void msm_queue_init(struct msm_device_queue *queue, const char *name)
{
	D("%s\n", __func__);
	spin_lock_init(&queue->lock);
	queue->len = 0;
	queue->max = 0;
	queue->name = name;
	INIT_LIST_HEAD(&queue->list);
	init_waitqueue_head(&queue->wait);
}

void msm_enqueue(struct msm_device_queue *queue,
			struct list_head *entry)
{
	unsigned long flags;
	spin_lock_irqsave(&queue->lock, flags);
	queue->len++;
	if (queue->len > queue->max) {
		queue->max = queue->len;
		pr_info("%s: queue %s new max is %d\n", __func__,
			queue->name, queue->max);
	}
	list_add_tail(entry, &queue->list);
	wake_up(&queue->wait);
	D("%s: woke up %s\n", __func__, queue->name);
	spin_unlock_irqrestore(&queue->lock, flags);
}

void msm_drain_eventq(struct msm_device_queue *queue)
{
	unsigned long flags;
	struct msm_queue_cmd *qcmd;
	struct msm_isp_event_ctrl *isp_event;
	spin_lock_irqsave(&queue->lock, flags);
	while (!list_empty(&queue->list)) {
		qcmd = list_first_entry(&queue->list,
			struct msm_queue_cmd, list_eventdata);
		list_del_init(&qcmd->list_eventdata);
		isp_event =
			(struct msm_isp_event_ctrl *)
			qcmd->command;
		if (isp_event->isp_data.ctrl.value != NULL)
			kfree(isp_event->isp_data.ctrl.value);
		kfree(qcmd->command);
		free_qcmd(qcmd);
	}
	spin_unlock_irqrestore(&queue->lock, flags);
}

int32_t msm_find_free_queue(void)
{
	int i;
	for (i = 0; i < MAX_NUM_ACTIVE_CAMERA; i++) {
		struct msm_cam_server_queue *queue;
		queue = &g_server_dev.server_queue[i];
		if (!queue->queue_active)
			return i;
	}
	return -EINVAL;
}

uint32_t msm_cam_server_get_mctl_handle(void)
{
	uint32_t i;
	if ((g_server_dev.mctl_handle_cnt << 8) == 0)
		g_server_dev.mctl_handle_cnt++;
	for (i = 0; i < MAX_NUM_ACTIVE_CAMERA; i++) {
		if (g_server_dev.mctl[i].handle == 0) {
			g_server_dev.mctl[i].handle =
				(++g_server_dev.mctl_handle_cnt) << 8 | i;
			memset(&g_server_dev.mctl[i].mctl,
				   0, sizeof(g_server_dev.mctl[i].mctl));
			return g_server_dev.mctl[i].handle;
		}
	}
	return 0;
}

void msm_cam_server_free_mctl(uint32_t handle)
{
	uint32_t mctl_index;
	mctl_index = handle & 0xff;
	if ((mctl_index < MAX_NUM_ACTIVE_CAMERA) &&
		(g_server_dev.mctl[mctl_index].handle == handle))
		g_server_dev.mctl[mctl_index].handle = 0;
	else
		pr_err("%s: invalid free handle\n", __func__);
}

struct msm_cam_media_controller *msm_cam_server_get_mctl(uint32_t handle)
{
	uint32_t mctl_index;
	mctl_index = handle & 0xff;
	if ((mctl_index < MAX_NUM_ACTIVE_CAMERA) &&
		(g_server_dev.mctl[mctl_index].handle == handle))
		return &g_server_dev.mctl[mctl_index].mctl;
	return NULL;
}

static int msm_ctrl_cmd_done(void *arg)
{
	void __user *uptr;
	struct msm_queue_cmd *qcmd;
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
	struct msm_ctrl_cmd *command;
	D("%s\n", __func__);

	command = kzalloc(sizeof(struct msm_ctrl_cmd), GFP_KERNEL);
	if (!command) {
		pr_err("%s Insufficient memory. return", __func__);
		goto command_alloc_fail;
	}

	qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_KERNEL);
	if (!qcmd) {
		pr_err("%s Insufficient memory. return", __func__);
		goto qcmd_alloc_fail;
	}

	mutex_lock(&g_server_dev.server_queue_lock);

	if (copy_from_user(command, (void __user *)ioctl_ptr->ioctl_ptr,
					   sizeof(struct msm_ctrl_cmd))) {
		pr_err("%s: copy_from_user failed, size=%d\n",
			   __func__, sizeof(struct msm_ctrl_cmd));
		goto ctrl_cmd_done_error;
	}

	if (!g_server_dev.server_queue[command->queue_idx].queue_active) {
		pr_err("%s: Invalid queue\n", __func__);
		goto ctrl_cmd_done_error;
	}

	D("%s qid %d evtid %d %d\n", __func__, command->queue_idx,
		command->evt_id,
		g_server_dev.server_queue[command->queue_idx].evt_id);

	if (command->evt_id !=
		g_server_dev.server_queue[command->queue_idx].evt_id) {
		pr_err("Invalid event id from userspace\n");
		goto ctrl_cmd_done_error;
	}

	atomic_set(&qcmd->on_heap, 1);
	uptr = command->value;
	qcmd->command = command;

	if (command->length > 0) {
		command->value =
			g_server_dev.server_queue[command->queue_idx].ctrl_data;
		if (command->length > max_control_command_size) {
			pr_err("%s: user data %d is too big (max %d)\n",
				__func__, command->length,
				max_control_command_size);
			goto ctrl_cmd_done_error;
		}
		if (copy_from_user(command->value, uptr, command->length)) {
			pr_err("%s: copy_from_user failed, size=%d\n",
				__func__, sizeof(struct msm_ctrl_cmd));
			goto ctrl_cmd_done_error;
		}
	}
	msm_enqueue(&g_server_dev.server_queue
		[command->queue_idx].ctrl_q, &qcmd->list_control);
	mutex_unlock(&g_server_dev.server_queue_lock);
	return 0;

ctrl_cmd_done_error:
	mutex_unlock(&g_server_dev.server_queue_lock);
	free_qcmd(qcmd);
qcmd_alloc_fail:
	kfree(command);
command_alloc_fail:
	return -EINVAL;
}

/* send control command to config and wait for results*/
static int msm_server_control(struct msm_cam_server_dev *server_dev,
				struct msm_ctrl_cmd *out)
{
	int rc = 0;
	void *value;
	struct msm_queue_cmd *rcmd;
	struct msm_queue_cmd *event_qcmd;
	struct msm_ctrl_cmd *ctrlcmd;
	struct msm_device_queue *queue =
		&server_dev->server_queue[out->queue_idx].ctrl_q;

	struct v4l2_event v4l2_evt;
	struct msm_isp_event_ctrl *isp_event;
	void *ctrlcmd_data;

	event_qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_KERNEL);
	if (!event_qcmd) {
		pr_err("%s Insufficient memory. return", __func__);
		rc = -ENOMEM;
		goto event_qcmd_alloc_fail;
	}

	isp_event = kzalloc(sizeof(struct msm_isp_event_ctrl), GFP_KERNEL);
	if (!isp_event) {
		pr_err("%s Insufficient memory. return", __func__);
		rc = -ENOMEM;
		goto isp_event_alloc_fail;
	}

	D("%s\n", __func__);
	mutex_lock(&server_dev->server_queue_lock);
	if (++server_dev->server_evt_id == 0)
		server_dev->server_evt_id++;

	D("%s qid %d evtid %d\n", __func__, out->queue_idx,
		server_dev->server_evt_id);
	server_dev->server_queue[out->queue_idx].evt_id =
		server_dev->server_evt_id;
	v4l2_evt.type = V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_V4L2;
	v4l2_evt.id = 0;
	v4l2_evt.u.data[0] = out->queue_idx;
	/* setup event object to transfer the command; */
	isp_event->resptype = MSM_CAM_RESP_V4L2;
	isp_event->isp_data.ctrl = *out;
	isp_event->isp_data.ctrl.evt_id = server_dev->server_evt_id;

	if (out->value != NULL && out->length != 0) {
		ctrlcmd_data = kzalloc(out->length, GFP_KERNEL);
		if (!ctrlcmd_data) {
			rc = -ENOMEM;
			goto ctrlcmd_alloc_fail;
		}
		memcpy(ctrlcmd_data, out->value, out->length);
		isp_event->isp_data.ctrl.value = ctrlcmd_data;
	}

	atomic_set(&event_qcmd->on_heap, 1);
	event_qcmd->command = isp_event;

	msm_enqueue(&server_dev->server_queue[out->queue_idx].eventData_q,
				&event_qcmd->list_eventdata);

	/* now send command to config thread in userspace,
	 * and wait for results */
	v4l2_event_queue(server_dev->server_command_queue.pvdev,
					  &v4l2_evt);
	D("%s v4l2_event_queue: type = 0x%x\n", __func__, v4l2_evt.type);
	mutex_unlock(&server_dev->server_queue_lock);

	/* wait for config return status */
	D("Waiting for config status\n");
	rc = wait_event_interruptible_timeout(queue->wait,
		!list_empty_careful(&queue->list),
		msecs_to_jiffies(out->timeout_ms));
	D("Waiting is over for config status\n");
	if (list_empty_careful(&queue->list)) {
		if (!rc)
			rc = -ETIMEDOUT;
		if (rc < 0) {
			if (++server_dev->server_evt_id == 0)
				server_dev->server_evt_id++;
			pr_err("%s: wait_event error %d\n", __func__, rc);
			return rc;
		}
	}

	rcmd = msm_dequeue(queue, list_control);
	BUG_ON(!rcmd);
	D("%s Finished servicing ioctl\n", __func__);

	ctrlcmd = (struct msm_ctrl_cmd *)(rcmd->command);
	value = out->value;
	if (ctrlcmd->length > 0 && value != NULL &&
		ctrlcmd->length <= out->length)
		memcpy(value, ctrlcmd->value, ctrlcmd->length);

	memcpy(out, ctrlcmd, sizeof(struct msm_ctrl_cmd));
	out->value = value;

	kfree(ctrlcmd);
	free_qcmd(rcmd);
	D("%s: rc %d\n", __func__, rc);
	/* rc is the time elapsed. */
	if (rc >= 0) {
		/* TODO: Refactor msm_ctrl_cmd::status field */
		if (out->status == 0)
			rc = -1;
		else if (out->status == 1 || out->status == 4)
			rc = 0;
		else
			rc = -EINVAL;
	}
	return rc;

ctrlcmd_alloc_fail:
	kfree(isp_event);
isp_event_alloc_fail:
	kfree(event_qcmd);
event_qcmd_alloc_fail:
	return rc;
}

int msm_server_get_crop(struct msm_cam_v4l2_device *pcam,
				int idx, struct v4l2_crop *crop)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;

	BUG_ON(crop == NULL);

	ctrlcmd.type = MSM_V4L2_GET_CROP;
	ctrlcmd.length = sizeof(struct v4l2_crop);
	ctrlcmd.value = (void *)crop;
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.stream_type = pcam->dev_inst[idx]->image_mode;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in userspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	D("%s: rc = %d\n", __func__, rc);

	return rc;
}

/*send open command to server*/
int msm_send_open_server(struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s qid %d\n", __func__, pcam->server_queue_idx);
	ctrlcmd.type	   = MSM_V4L2_OPEN;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length	 = strnlen(g_server_dev.config_info.config_dev_name[0],
				MAX_DEV_NAME_LEN)+1;
	ctrlcmd.value    = (char *)g_server_dev.config_info.config_dev_name[0];
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

int msm_send_close_server(struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s qid %d\n", __func__, pcam->server_queue_idx);
	ctrlcmd.type	   = MSM_V4L2_CLOSE;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length	 = strnlen(g_server_dev.config_info.config_dev_name[0],
				MAX_DEV_NAME_LEN)+1;
	ctrlcmd.value    = (char *)g_server_dev.config_info.config_dev_name[0];
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

int msm_server_set_fmt(struct msm_cam_v4l2_device *pcam, int idx,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format *pix = &pfmt->fmt.pix;
	struct msm_ctrl_cmd ctrlcmd;
	struct img_plane_info plane_info;

	plane_info.width = pix->width;
	plane_info.height = pix->height;
	plane_info.pixelformat = pix->pixelformat;
	plane_info.buffer_type = pfmt->type;
	plane_info.ext_mode = pcam->dev_inst[idx]->image_mode;
	plane_info.num_planes = 1;
	D("%s: %d, %d, 0x%x\n", __func__,
		pfmt->fmt.pix.width, pfmt->fmt.pix.height,
		pfmt->fmt.pix.pixelformat);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		D("%s, Attention! Wrong buf-type %d\n", __func__, pfmt->type);

	for (i = 0; i < pcam->num_fmts; i++)
		if (pcam->usr_fmts[i].fourcc == pix->pixelformat)
			break;
	if (i == pcam->num_fmts) {
		pr_err("%s: User requested pixelformat %x not supported\n",
						__func__, pix->pixelformat);
		return -EINVAL;
	}

	ctrlcmd.type       = MSM_V4L2_VID_CAP_TYPE;
	ctrlcmd.length     = sizeof(struct img_plane_info);
	ctrlcmd.value      = (void *)&plane_info;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.vnode_id   = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	if (rc >= 0) {
		pcam->dev_inst[idx]->vid_fmt = *pfmt;
		pcam->dev_inst[idx]->sensor_pxlcode
					= pcam->usr_fmts[i].pxlcode;
		D("%s:inst=0x%x,idx=%d,width=%d,heigth=%d\n",
			 __func__, (u32)pcam->dev_inst[idx], idx,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix.width,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix.height);
		pcam->dev_inst[idx]->plane_info = plane_info;
	}

	return rc;
}

int msm_server_set_fmt_mplane(struct msm_cam_v4l2_device *pcam, int idx,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format_mplane *pix_mp = &pfmt->fmt.pix_mp;
	struct msm_ctrl_cmd ctrlcmd;
	struct img_plane_info plane_info;

	plane_info.width = pix_mp->width;
	plane_info.height = pix_mp->height;
	plane_info.pixelformat = pix_mp->pixelformat;
	plane_info.buffer_type = pfmt->type;
	plane_info.ext_mode = pcam->dev_inst[idx]->image_mode;
	plane_info.num_planes = pix_mp->num_planes;
	if (plane_info.num_planes <= 0 ||
		plane_info.num_planes > VIDEO_MAX_PLANES) {
		pr_err("%s Invalid number of planes set %d", __func__,
				plane_info.num_planes);
		return -EINVAL;
	}
	D("%s: %d, %d, 0x%x\n", __func__,
		pfmt->fmt.pix_mp.width, pfmt->fmt.pix_mp.height,
		pfmt->fmt.pix_mp.pixelformat);

	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pr_err("%s, Attention! Wrong buf-type %d\n",
			__func__, pfmt->type);
		return -EINVAL;
	}

	for (i = 0; i < pcam->num_fmts; i++)
		if (pcam->usr_fmts[i].fourcc == pix_mp->pixelformat)
			break;
	if (i == pcam->num_fmts) {
		pr_err("%s: User requested pixelformat %x not supported\n",
						__func__, pix_mp->pixelformat);
		return -EINVAL;
	}

	ctrlcmd.type       = MSM_V4L2_VID_CAP_TYPE;
	ctrlcmd.length     = sizeof(struct img_plane_info);
	ctrlcmd.value      = (void *)&plane_info;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.vnode_id   = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	if (rc >= 0) {
		pcam->dev_inst[idx]->vid_fmt = *pfmt;
		pcam->dev_inst[idx]->sensor_pxlcode
					= pcam->usr_fmts[i].pxlcode;
		D("%s:inst=0x%x,idx=%d,width=%d,heigth=%d\n",
			 __func__, (u32)pcam->dev_inst[idx], idx,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix_mp.width,
			 pcam->dev_inst[idx]->vid_fmt.fmt.pix_mp.height);
		pcam->dev_inst[idx]->plane_info = plane_info;
	}

	return rc;
}

int msm_server_streamon(struct msm_cam_v4l2_device *pcam, int idx)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s\n", __func__);
	ctrlcmd.type	   = MSM_V4L2_STREAM_ON;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length	 = 0;
	ctrlcmd.value    = NULL;
	ctrlcmd.stream_type = pcam->dev_inst[idx]->image_mode;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];


	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

int msm_server_streamoff(struct msm_cam_v4l2_device *pcam, int idx)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;

	D("%s, pcam = 0x%x\n", __func__, (u32)pcam);
	ctrlcmd.type        = MSM_V4L2_STREAM_OFF;
	ctrlcmd.timeout_ms  = 10000;
	ctrlcmd.length      = 0;
	ctrlcmd.value       = NULL;
	ctrlcmd.stream_type = pcam->dev_inst[idx]->image_mode;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

int msm_server_proc_ctrl_cmd(struct msm_cam_v4l2_device *pcam,
		struct msm_camera_v4l2_ioctl_t *ioctl_ptr, int is_set_cmd)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd, tmp_cmd, *cmd_ptr;
	uint8_t *ctrl_data = NULL;
	uint32_t cmd_len = sizeof(struct msm_ctrl_cmd);
	uint32_t value_len;

	if (copy_from_user(&tmp_cmd,
		(void __user *)ioctl_ptr->ioctl_ptr, cmd_len)) {
		pr_err("%s: copy_from_user failed.\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	value_len = tmp_cmd.length;
	ctrl_data = kzalloc(value_len+cmd_len, GFP_KERNEL);
	if (!ctrl_data) {
		pr_err("%s could not allocate memory\n", __func__);
		rc = -ENOMEM;
		goto end;
	}

	cmd_ptr = (struct msm_ctrl_cmd *) ctrl_data;
	*cmd_ptr = tmp_cmd;
	if (tmp_cmd.value && tmp_cmd.length > 0) {
		cmd_ptr->value = (void *)(ctrl_data+cmd_len);
		if (copy_from_user((void *)cmd_ptr->value,
				   (void __user *)tmp_cmd.value,
				   value_len)) {
			pr_err("%s: copy_from_user failed.\n", __func__);
			rc = -EINVAL;
			goto end;
		}
	} else {
		cmd_ptr->value = NULL;
	}

	D("%s: cmd type = %d, up1=0x%x, ulen1=%d, up2=0x%x, ulen2=%d\n",
		__func__, tmp_cmd.type, (uint32_t)ioctl_ptr->ioctl_ptr, cmd_len,
		(uint32_t)tmp_cmd.value, tmp_cmd.length);

	ctrlcmd.type = MSM_V4L2_SET_CTRL_CMD;
	ctrlcmd.length = cmd_len + value_len;
	ctrlcmd.value = (void *)ctrl_data;
	if (tmp_cmd.timeout_ms > 0)
		ctrlcmd.timeout_ms = tmp_cmd.timeout_ms;
	else
		ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];
	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	D("%s: msm_server_control rc=%d\n", __func__, rc);
	if (rc == 0) {
		if (tmp_cmd.value && tmp_cmd.length > 0 &&
			copy_to_user((void __user *)tmp_cmd.value,
				(void *)(ctrl_data+cmd_len), tmp_cmd.length)) {
			pr_err("%s: copy_to_user failed, size=%d\n",
				__func__, tmp_cmd.length);
			rc = -EINVAL;
			goto end;
		}

		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
			(void *)&tmp_cmd, cmd_len)) {
			pr_err("%s: copy_to_user failed in cpy, size=%d\n",
				__func__, cmd_len);
			rc = -EINVAL;
			goto end;
		}
	}
end:
	D("%s: END, type = %d, vaddr = 0x%x, vlen = %d, status = %d, rc = %d\n",
		__func__, tmp_cmd.type, (uint32_t)tmp_cmd.value,
		tmp_cmd.length, tmp_cmd.status, rc);
	kfree(ctrl_data);
	return rc;
}

int msm_server_s_ctrl(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	uint8_t ctrl_data[max_control_command_size];

	WARN_ON(ctrl == NULL);
	if (ctrl == NULL) {
		pr_err("%s Invalid control\n", __func__);
		return -EINVAL;
	}

	memset(ctrl_data, 0, sizeof(ctrl_data));

	ctrlcmd.type = MSM_V4L2_SET_CTRL;
	ctrlcmd.length = sizeof(struct v4l2_control);
	ctrlcmd.value = (void *)ctrl_data;
	memcpy(ctrlcmd.value, ctrl, ctrlcmd.length);
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	return rc;
}

int msm_server_g_ctrl(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_control *ctrl)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	uint8_t ctrl_data[max_control_command_size];

	WARN_ON(ctrl == NULL);
	if (ctrl == NULL) {
		pr_err("%s Invalid control\n", __func__);
		return -EINVAL;
	}

	memset(ctrl_data, 0, sizeof(ctrl_data));

	ctrlcmd.type = MSM_V4L2_GET_CTRL;
	ctrlcmd.length = sizeof(struct v4l2_control);
	ctrlcmd.value = (void *)ctrl_data;
	memcpy(ctrlcmd.value, ctrl, ctrlcmd.length);
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);

	ctrl->value = ((struct v4l2_control *)ctrlcmd.value)->value;

	return rc;
}

int msm_server_q_ctrl(struct msm_cam_v4l2_device *pcam,
			struct v4l2_queryctrl *queryctrl)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	uint8_t ctrl_data[max_control_command_size];

	WARN_ON(queryctrl == NULL);
	memset(ctrl_data, 0, sizeof(ctrl_data));

	ctrlcmd.type = MSM_V4L2_QUERY_CTRL;
	ctrlcmd.length = sizeof(struct v4l2_queryctrl);
	ctrlcmd.value = (void *)ctrl_data;
	memcpy(ctrlcmd.value, queryctrl, ctrlcmd.length);
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];

	/* send command to config thread in userspace, and get return value */
	rc = msm_server_control(&g_server_dev, &ctrlcmd);
	D("%s: rc = %d\n", __func__, rc);

	if (rc >= 0)
		memcpy(queryctrl, ctrlcmd.value, sizeof(struct v4l2_queryctrl));

	return rc;
}

int msm_server_get_fmt(struct msm_cam_v4l2_device *pcam,
		 int idx, struct v4l2_format *pfmt)
{
	struct v4l2_pix_format *pix = &pfmt->fmt.pix;

	pix->width        = pcam->dev_inst[idx]->vid_fmt.fmt.pix.width;
	pix->height       = pcam->dev_inst[idx]->vid_fmt.fmt.pix.height;
	pix->field        = pcam->dev_inst[idx]->vid_fmt.fmt.pix.field;
	pix->pixelformat  = pcam->dev_inst[idx]->vid_fmt.fmt.pix.pixelformat;
	pix->bytesperline = pcam->dev_inst[idx]->vid_fmt.fmt.pix.bytesperline;
	pix->colorspace   = pcam->dev_inst[idx]->vid_fmt.fmt.pix.colorspace;
	if (pix->bytesperline < 0)
		return pix->bytesperline;

	pix->sizeimage    = pix->height * pix->bytesperline;

	return 0;
}

int msm_server_get_fmt_mplane(struct msm_cam_v4l2_device *pcam,
		 int idx, struct v4l2_format *pfmt)
{
	*pfmt = pcam->dev_inst[idx]->vid_fmt;
	return 0;
}

int msm_server_try_fmt(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format *pix = &pfmt->fmt.pix;

	D("%s: 0x%x\n", __func__, pix->pixelformat);
	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		pr_err("%s: pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE!\n",
							__func__);
		return -EINVAL;
	}

	/* check if the format is supported by this host-sensor combo */
	for (i = 0; i < pcam->num_fmts; i++) {
		D("%s: usr_fmts.fourcc: 0x%x\n", __func__,
			pcam->usr_fmts[i].fourcc);
		if (pcam->usr_fmts[i].fourcc == pix->pixelformat)
			break;
	}

	if (i == pcam->num_fmts) {
		pr_err("%s: Format %x not found\n", __func__, pix->pixelformat);
		return -EINVAL;
	}
	return rc;
}

int msm_server_try_fmt_mplane(struct msm_cam_v4l2_device *pcam,
				 struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_pix_format_mplane *pix_mp = &pfmt->fmt.pix_mp;

	D("%s: 0x%x\n", __func__, pix_mp->pixelformat);
	if (pfmt->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		pr_err("%s: Incorrect format type %d ",
			__func__, pfmt->type);
		return -EINVAL;
	}

	/* check if the format is supported by this host-sensor combo */
	for (i = 0; i < pcam->num_fmts; i++) {
		D("%s: usr_fmts.fourcc: 0x%x\n", __func__,
			pcam->usr_fmts[i].fourcc);
		if (pcam->usr_fmts[i].fourcc == pix_mp->pixelformat)
			break;
	}

	if (i == pcam->num_fmts) {
		pr_err("%s: Format %x not found\n",
			__func__, pix_mp->pixelformat);
		return -EINVAL;
	}
	return rc;
}

int msm_server_v4l2_subscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;

	D("%s: fh = 0x%x, type = 0x%x", __func__, (u32)fh, sub->type);
	if (sub->type == V4L2_EVENT_ALL) {
		/*sub->type = MSM_ISP_EVENT_START;*/
		sub->type = V4L2_EVENT_PRIVATE_START + MSM_CAM_RESP_CTRL;
		D("sub->type start = 0x%x\n", sub->type);
		do {
			rc = v4l2_event_subscribe(fh, sub, 30);
			if (rc < 0) {
				D("%s: failed for evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
			/* unsubscribe all events here and return */
			sub->type = V4L2_EVENT_ALL;
			v4l2_event_unsubscribe(fh, sub);
			return rc;
			} else
				D("%s: subscribed evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
			sub->type++;
			D("sub->type while = 0x%x\n", sub->type);
		} while (sub->type !=
			V4L2_EVENT_PRIVATE_START + MSM_SVR_RESP_MAX);
	} else {
		D("sub->type not V4L2_EVENT_ALL = 0x%x\n", sub->type);
		rc = v4l2_event_subscribe(fh, sub, 30);
		if (rc < 0)
			D("%s: failed for evtType = 0x%x, rc = %d\n",
						__func__, sub->type, rc);
	}

	D("%s: rc = %d\n", __func__, rc);
	return rc;
}

int msm_server_v4l2_unsubscribe_event(struct v4l2_fh *fh,
			struct v4l2_event_subscription *sub)
{
	int rc = 0;

	D("%s: fh = 0x%x\n", __func__, (u32)fh);
	rc = v4l2_event_unsubscribe(fh, sub);
	D("%s: rc = %d\n", __func__, rc);
	return rc;
}

/* open an active camera session to manage the streaming logic */
static int msm_cam_server_open_session(struct msm_cam_server_dev *ps,
	struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	struct msm_cam_media_controller *pmctl;

	D("%s\n", __func__);

	if (!ps || !pcam) {
		pr_err("%s NULL pointer passed in!\n", __func__);
		return rc;
	}

	/* The number of camera instance should be controlled by the
		resource manager. Currently supporting one active instance
		until multiple instances are supported */
	if (atomic_read(&ps->number_pcam_active) > 0) {
		pr_err("%s Cannot have more than one active camera %d\n",
			__func__, atomic_read(&ps->number_pcam_active));
		return -EINVAL;
	}
	/* book keeping this camera session*/
	ps->pcam_active = pcam;
	atomic_inc(&ps->number_pcam_active);

	D("config pcam = 0x%p\n", ps->pcam_active);

	/* initialization the media controller module*/
	msm_mctl_init(pcam);

	/*for single VFE msms (8660, 8960v1), just populate the session
	with our VFE devices that registered*/
	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	pmctl->axi_sdev = ps->axi_device[0];
	pmctl->isp_sdev = ps->isp_subdev[0];
	return rc;
}

/* close an active camera session to server */
static int msm_cam_server_close_session(struct msm_cam_server_dev *ps,
	struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	D("%s\n", __func__);

	if (!ps || !pcam) {
		D("%s NULL pointer passed in!\n", __func__);
		return rc;
	}


	atomic_dec(&ps->number_pcam_active);
	ps->pcam_active = NULL;

	msm_mctl_free(pcam);
	return rc;
}

static long msm_ioctl_server(struct file *file, void *fh,
		bool valid_prio, int cmd, void *arg)
{
	int rc = -EINVAL;
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
	struct msm_camera_info temp_cam_info;
	struct msm_cam_config_dev_info temp_config_info;
	struct msm_mctl_node_info temp_mctl_info;
	int i;

	D("%s: cmd %d\n", __func__, _IOC_NR(cmd));

	switch (cmd) {
	case MSM_CAM_V4L2_IOCTL_GET_CAMERA_INFO:
		if (copy_from_user(&temp_cam_info,
				(void __user *)ioctl_ptr->ioctl_ptr,
				sizeof(struct msm_camera_info))) {
			pr_err("%s Copy from user failed for cmd %d",
				__func__, cmd);
			rc = -EINVAL;
			return rc;
		}
		for (i = 0; i < g_server_dev.camera_info.num_cameras; i++) {
			if (copy_to_user((void __user *)
			temp_cam_info.video_dev_name[i],
			g_server_dev.camera_info.video_dev_name[i],
			strnlen(g_server_dev.camera_info.video_dev_name[i],
				MAX_DEV_NAME_LEN))) {
				pr_err("%s Copy to user failed for cmd %d",
					__func__, cmd);
				rc = -EINVAL;
				return rc;
			}
			temp_cam_info.has_3d_support[i] =
				g_server_dev.camera_info.has_3d_support[i];
			temp_cam_info.is_internal_cam[i] =
				g_server_dev.camera_info.is_internal_cam[i];
			temp_cam_info.s_mount_angle[i] =
				g_server_dev.camera_info.s_mount_angle[i];
			temp_cam_info.sensor_type[i] =
				g_server_dev.camera_info.sensor_type[i];

		}
		temp_cam_info.num_cameras =
			g_server_dev.camera_info.num_cameras;
		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
			&temp_cam_info,	sizeof(struct msm_camera_info))) {
			pr_err("%s Copy to user failed for cmd %d",
				__func__, cmd);
			rc = -EINVAL;
			return rc;
		}
		rc = 0;
		break;

	case MSM_CAM_V4L2_IOCTL_GET_CONFIG_INFO:
		if (copy_from_user(&temp_config_info,
			(void __user *)ioctl_ptr->ioctl_ptr,
			sizeof(struct msm_cam_config_dev_info))) {
			pr_err("%s Copy from user failed for cmd %d",
				__func__, cmd);
			rc = -EINVAL;
			return rc;
		}
		for (i = 0;
		 i < g_server_dev.config_info.num_config_nodes; i++) {
			if (copy_to_user(
			(void __user *)temp_config_info.config_dev_name[i],
			g_server_dev.config_info.config_dev_name[i],
			strnlen(g_server_dev.config_info.config_dev_name[i],
				MAX_DEV_NAME_LEN))) {
				pr_err("%s Copy to user failed for cmd %d",
					__func__, cmd);
				rc = -EINVAL;
				return rc;
			}
		}
		temp_config_info.num_config_nodes =
			g_server_dev.config_info.num_config_nodes;
		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
			&temp_config_info,
			sizeof(struct msm_cam_config_dev_info))) {
			pr_err("%s Copy to user failed for cmd %d",
				__func__, cmd);
			rc = -EINVAL;
			return rc;
		}
		rc = 0;
		break;
	case MSM_CAM_V4L2_IOCTL_GET_MCTL_INFO:
		if (copy_from_user(&temp_mctl_info,
				(void __user *)ioctl_ptr->ioctl_ptr,
				sizeof(struct msm_mctl_node_info))) {
			pr_err("%s Copy from user failed for cmd %d",
				__func__, cmd);
			rc = -EINVAL;
			return rc;
		}
		for (i = 0; i < g_server_dev.mctl_node_info.num_mctl_nodes;
				i++) {
			if (copy_to_user((void __user *)
			temp_mctl_info.mctl_node_name[i],
			g_server_dev.mctl_node_info.mctl_node_name[i], strnlen(
			g_server_dev.mctl_node_info.mctl_node_name[i],
			MAX_DEV_NAME_LEN))) {
				pr_err("%s Copy to user failed for cmd %d",
					__func__, cmd);
				rc = -EINVAL;
				return rc;
			}
		}
		temp_mctl_info.num_mctl_nodes =
			g_server_dev.mctl_node_info.num_mctl_nodes;
		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
			&temp_mctl_info, sizeof(struct msm_mctl_node_info))) {
			pr_err("%s Copy to user failed for cmd %d",
				__func__, cmd);
			rc = -EINVAL;
			return rc;
		}
		rc = 0;
	break;

	case MSM_CAM_V4L2_IOCTL_CTRL_CMD_DONE:
		D("%s: MSM_CAM_IOCTL_CTRL_CMD_DONE\n", __func__);
		rc = msm_ctrl_cmd_done(arg);
		break;

	case MSM_CAM_V4L2_IOCTL_GET_EVENT_PAYLOAD: {
		struct msm_queue_cmd *event_cmd;
		struct msm_isp_event_ctrl u_isp_event;
		struct msm_isp_event_ctrl *k_isp_event;
		struct msm_device_queue *queue;
		void __user *u_ctrl_value = NULL;
		if (copy_from_user(&u_isp_event,
			(void __user *)ioctl_ptr->ioctl_ptr,
			sizeof(struct msm_isp_event_ctrl))) {
			pr_err("%s Copy from user failed for cmd %d",
				__func__, cmd);
			rc = -EINVAL;
			return rc;
		}

		mutex_lock(&g_server_dev.server_queue_lock);
		if (!g_server_dev.server_queue
			[u_isp_event.isp_data.ctrl.queue_idx].queue_active) {
			pr_err("%s: Invalid queue\n", __func__);
			mutex_unlock(&g_server_dev.server_queue_lock);
			rc = -EINVAL;
			return rc;
		}
		queue = &g_server_dev.server_queue
			[u_isp_event.isp_data.ctrl.queue_idx].eventData_q;
		event_cmd = msm_dequeue(queue, list_eventdata);
		if (!event_cmd) {
			pr_err("%s: No event payload\n", __func__);
			rc = -EINVAL;
			mutex_unlock(&g_server_dev.server_queue_lock);
			return rc;
		}
		k_isp_event = (struct msm_isp_event_ctrl *)
				event_cmd->command;
		free_qcmd(event_cmd);

		/* Save the pointer of the user allocated command buffer*/
		u_ctrl_value = u_isp_event.isp_data.ctrl.value;

		/* Copy the event structure into user struct*/
		u_isp_event = *k_isp_event;

		/* Restore the saved pointer of the user
		 * allocated command buffer. */
		u_isp_event.isp_data.ctrl.value = u_ctrl_value;

		/* Copy the ctrl cmd, if present*/
		if (k_isp_event->isp_data.ctrl.length > 0 &&
			k_isp_event->isp_data.ctrl.value != NULL) {
			void *k_ctrl_value =
				k_isp_event->isp_data.ctrl.value;
			if (copy_to_user(u_ctrl_value, k_ctrl_value,
				 k_isp_event->isp_data.ctrl.length)) {
				pr_err("%s Copy to user failed for cmd %d",
					__func__, cmd);
				kfree(k_isp_event->isp_data.ctrl.value);
				kfree(k_isp_event);
				rc = -EINVAL;
				mutex_unlock(&g_server_dev.server_queue_lock);
				break;
			}
			kfree(k_isp_event->isp_data.ctrl.value);
		}
		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
			&u_isp_event, sizeof(struct msm_isp_event_ctrl))) {
			pr_err("%s Copy to user failed for cmd %d",
				__func__, cmd);
			kfree(k_isp_event);
			mutex_unlock(&g_server_dev.server_queue_lock);
			rc = -EINVAL;
			return rc;
		}
		kfree(k_isp_event);
		mutex_unlock(&g_server_dev.server_queue_lock);
		rc = 0;
		break;
	}

	case MSM_CAM_IOCTL_SEND_EVENT:
		rc = msm_server_send_v4l2_evt(arg);
		break;

	default:
		pr_err("%s: Invalid IOCTL = %d", __func__, cmd);
		break;
	}
	return rc;
}

static int msm_open_server(struct file *fp)
{
	int rc = 0;
	D("%s: open %s\n", __func__, fp->f_path.dentry->d_name.name);
	mutex_lock(&g_server_dev.server_lock);
	g_server_dev.use_count++;
	if (g_server_dev.use_count == 1)
		fp->private_data =
			&g_server_dev.server_command_queue.eventHandle;
	mutex_unlock(&g_server_dev.server_lock);
	return rc;
}

static int msm_close_server(struct file *fp)
{
	struct v4l2_event_subscription sub;
	D("%s\n", __func__);
	mutex_lock(&g_server_dev.server_lock);
	if (g_server_dev.use_count > 0)
		g_server_dev.use_count--;
	mutex_unlock(&g_server_dev.server_lock);

	if (g_server_dev.use_count == 0) {
		mutex_lock(&g_server_dev.server_lock);
		if (g_server_dev.pcam_active) {
			struct v4l2_event v4l2_ev;
			struct msm_cam_media_controller *pmctl = NULL;
			int rc;

			pmctl = msm_cam_server_get_mctl(
				g_server_dev.pcam_active->mctl_handle);
			if (pmctl && pmctl->mctl_release) {
				rc = pmctl->mctl_release(pmctl);
				if (rc < 0)
					pr_err("mctl_release fails %d\n", rc);
				/*so that it isn't closed again*/
				pmctl->mctl_release = NULL;
			}

			v4l2_ev.id = 0;
			v4l2_ev.type = V4L2_EVENT_PRIVATE_START
				+ MSM_CAM_APP_NOTIFY_ERROR_EVENT;
			ktime_get_ts(&v4l2_ev.timestamp);
			v4l2_event_queue(
				g_server_dev.pcam_active->pvdev, &v4l2_ev);
		}
		sub.type = V4L2_EVENT_ALL;
		msm_server_v4l2_unsubscribe_event(
			&g_server_dev.server_command_queue.eventHandle, &sub);
		mutex_unlock(&g_server_dev.server_lock);
	}
	return 0;
}

static unsigned int msm_poll_server(struct file *fp,
					struct poll_table_struct *wait)
{
	int rc = 0;

	D("%s\n", __func__);
	poll_wait(fp,
		 &g_server_dev.server_command_queue.eventHandle.wait,
		 wait);
	if (v4l2_event_pending(&g_server_dev.server_command_queue.eventHandle))
		rc |= POLLPRI;

	return rc;
}

int msm_server_get_usecount(void)
{
	return g_server_dev.use_count;
}

int msm_server_update_sensor_info(struct msm_cam_v4l2_device *pcam,
	struct msm_camera_sensor_info *sdata)
{
	int rc = 0;

	if (!pcam || !sdata) {
		pr_err("%s Input data is null ", __func__);
		return -EINVAL;
	}

	g_server_dev.camera_info.video_dev_name
	[g_server_dev.camera_info.num_cameras]
	= video_device_node_name(pcam->pvdev);
	D("%s Connected video device %s\n", __func__,
		g_server_dev.camera_info.video_dev_name
		[g_server_dev.camera_info.num_cameras]);

	g_server_dev.camera_info.s_mount_angle
	[g_server_dev.camera_info.num_cameras]
	= sdata->sensor_platform_info->mount_angle;

	g_server_dev.camera_info.is_internal_cam
	[g_server_dev.camera_info.num_cameras]
	= sdata->camera_type;

	g_server_dev.mctl_node_info.mctl_node_name
	[g_server_dev.mctl_node_info.num_mctl_nodes]
	= video_device_node_name(pcam->mctl_node.pvdev);

	pr_info("%s mctl_node_name[%d] = %s\n", __func__,
		g_server_dev.mctl_node_info.num_mctl_nodes,
		g_server_dev.mctl_node_info.mctl_node_name
		[g_server_dev.mctl_node_info.num_mctl_nodes]);

	/*Temporary solution to store info in media device structure
	  until we can expand media device structure to support more
	  device info*/
	snprintf(pcam->media_dev.serial,
			sizeof(pcam->media_dev.serial),
			"%s-%d-%d", QCAMERA_NAME,
			sdata->sensor_platform_info->mount_angle,
			sdata->camera_type);

	g_server_dev.camera_info.num_cameras++;
	g_server_dev.mctl_node_info.num_mctl_nodes++;

	D("%s done, rc = %d\n", __func__, rc);
	D("%s number of sensors connected is %d\n", __func__,
		g_server_dev.camera_info.num_cameras);

	return rc;
}

int msm_server_begin_session(struct msm_cam_v4l2_device *pcam,
	int server_q_idx)
{
	int rc = -EINVAL, ges_evt;
	struct msm_cam_server_queue *queue;

	if (!pcam) {
		pr_err("%s pcam passed is null ", __func__);
		return rc;
	}

	ges_evt = MSM_V4L2_GES_CAM_OPEN;
	D("%s send gesture evt\n", __func__);
	msm_cam_server_subdev_notify(g_server_dev.gesture_device,
		NOTIFY_GESTURE_CAM_EVT, &ges_evt);

	pcam->server_queue_idx = server_q_idx;
	queue = &g_server_dev.server_queue[server_q_idx];
	queue->ctrl_data = kzalloc(sizeof(uint8_t) *
			max_control_command_size, GFP_KERNEL);
	msm_queue_init(&queue->ctrl_q, "control");
	msm_queue_init(&queue->eventData_q, "eventdata");
	queue->queue_active = 1;

	rc = msm_cam_server_open_session(&g_server_dev, pcam);
	if (rc < 0) {
		pr_err("%s: cam_server_open_session failed %d\n",
			__func__, rc);
		goto error;
	}

	return rc;
error:
	ges_evt = MSM_V4L2_GES_CAM_CLOSE;
	msm_cam_server_subdev_notify(g_server_dev.gesture_device,
		NOTIFY_GESTURE_CAM_EVT, &ges_evt);

	queue->queue_active = 0;
	msm_drain_eventq(&queue->eventData_q);
	msm_queue_drain(&queue->ctrl_q, list_control);
	kfree(queue->ctrl_data);
	queue->ctrl_data = NULL;
	queue = NULL;
	return rc;
}

int msm_server_end_session(struct msm_cam_v4l2_device *pcam)
{
	int rc = -EINVAL, ges_evt;
	struct msm_cam_server_queue *queue;

	mutex_lock(&g_server_dev.server_queue_lock);
	queue = &g_server_dev.server_queue[pcam->server_queue_idx];
	queue->queue_active = 0;
	kfree(queue->ctrl_data);
	queue->ctrl_data = NULL;
	msm_queue_drain(&queue->ctrl_q, list_control);
	msm_drain_eventq(&queue->eventData_q);
	mutex_unlock(&g_server_dev.server_queue_lock);

	rc = msm_cam_server_close_session(&g_server_dev, pcam);
	if (rc < 0)
		pr_err("msm_cam_server_close_session fails %d\n", rc);

	ges_evt = MSM_V4L2_GES_CAM_CLOSE;
	msm_cam_server_subdev_notify(g_server_dev.gesture_device,
			NOTIFY_GESTURE_CAM_EVT, &ges_evt);

	return rc;
}

/* Init a config node for ISP control,
 * which will create a config device (/dev/config0/ and plug in
 * ISP's operation "v4l2_ioctl_ops*"
 */
static const struct v4l2_file_operations msm_fops_server = {
	.owner = THIS_MODULE,
	.open  = msm_open_server,
	.poll  = msm_poll_server,
	.unlocked_ioctl = video_ioctl2,
	.release = msm_close_server,
};

static const struct v4l2_ioctl_ops msm_ioctl_ops_server = {
	.vidioc_subscribe_event = msm_server_v4l2_subscribe_event,
	.vidioc_default = msm_ioctl_server,
};

static void msm_cam_server_subdev_notify(struct v4l2_subdev *sd,
				unsigned int notification, void *arg)
{
	int rc = -EINVAL;
	struct msm_sensor_ctrl_t *s_ctrl;
	struct msm_camera_sensor_info *sinfo;
	struct msm_camera_device_platform_data *camdev;
	uint8_t csid_core = 0;

	if (notification == NOTIFY_CID_CHANGE ||
		notification == NOTIFY_ISPIF_STREAM ||
		notification == NOTIFY_PCLK_CHANGE ||
		notification == NOTIFY_CSIPHY_CFG ||
		notification == NOTIFY_CSID_CFG ||
		notification == NOTIFY_CSIC_CFG) {
		s_ctrl = get_sctrl(sd);
		sinfo = (struct msm_camera_sensor_info *) s_ctrl->sensordata;
		camdev = sinfo->pdata;
		csid_core = camdev->csid_core;
	}

	switch (notification) {
	case NOTIFY_CID_CHANGE:
		/* reconfig the ISPIF*/
		if (g_server_dev.ispif_device) {
			struct msm_ispif_params_list ispif_params;
			ispif_params.len = 1;
			ispif_params.params[0].intftype = PIX0;
			ispif_params.params[0].cid_mask = 0x0001;
			ispif_params.params[0].csid = csid_core;

			rc = v4l2_subdev_call(
				g_server_dev.ispif_device, core, ioctl,
				VIDIOC_MSM_ISPIF_CFG, &ispif_params);
			if (rc < 0)
				return;
		}
		break;
	case NOTIFY_ISPIF_STREAM:
		/* call ISPIF stream on/off */
		rc = v4l2_subdev_call(g_server_dev.ispif_device, video,
				s_stream, (int)arg);
		if (rc < 0)
			return;

		break;
	case NOTIFY_ISP_MSG_EVT:
	case NOTIFY_VFE_MSG_OUT:
	case NOTIFY_VFE_MSG_STATS:
	case NOTIFY_VFE_MSG_COMP_STATS:
	case NOTIFY_VFE_BUF_EVT:
	case NOTIFY_VFE_BUF_FREE_EVT:
		if (g_server_dev.isp_subdev[0] &&
			g_server_dev.isp_subdev[0]->isp_notify) {
			rc = g_server_dev.isp_subdev[0]->isp_notify(
				g_server_dev.vfe_device[0], notification, arg);
		}
		break;
	case NOTIFY_VFE_IRQ:{
		struct msm_vfe_cfg_cmd cfg_cmd;
		struct msm_camvfe_params vfe_params;
		cfg_cmd.cmd_type = CMD_VFE_PROCESS_IRQ;
		vfe_params.vfe_cfg = &cfg_cmd;
		vfe_params.data = arg;
		rc = v4l2_subdev_call(g_server_dev.vfe_device[0],
			core, ioctl, 0, &vfe_params);
	}
		break;
	case NOTIFY_AXI_IRQ:
		rc = v4l2_subdev_call(g_server_dev.axi_device[0],
			core, ioctl, VIDIOC_MSM_AXI_IRQ, arg);
		break;
	case NOTIFY_PCLK_CHANGE:
		if (g_server_dev.axi_device[0])
			rc = v4l2_subdev_call(g_server_dev.axi_device[0], video,
			s_crystal_freq, *(uint32_t *)arg, 0);
		else
			rc = v4l2_subdev_call(g_server_dev.vfe_device[0], video,
			s_crystal_freq, *(uint32_t *)arg, 0);
		break;
	case NOTIFY_CSIPHY_CFG:
		rc = v4l2_subdev_call(g_server_dev.csiphy_device[csid_core],
			core, ioctl, VIDIOC_MSM_CSIPHY_CFG, arg);
		break;
	case NOTIFY_CSID_CFG:
		rc = v4l2_subdev_call(g_server_dev.csid_device[csid_core],
			core, ioctl, VIDIOC_MSM_CSID_CFG, arg);
		break;
	case NOTIFY_CSIC_CFG:
		rc = v4l2_subdev_call(g_server_dev.csic_device[csid_core],
			core, ioctl, VIDIOC_MSM_CSIC_CFG, arg);
		break;
	case NOTIFY_GESTURE_EVT:
		rc = v4l2_subdev_call(g_server_dev.gesture_device,
			core, ioctl, VIDIOC_MSM_GESTURE_EVT, arg);
		break;
	case NOTIFY_GESTURE_CAM_EVT:
		rc = v4l2_subdev_call(g_server_dev.gesture_device,
			core, ioctl, VIDIOC_MSM_GESTURE_CAM_EVT, arg);
		break;
	default:
		break;
	}

	return;
}

void msm_cam_release_subdev_node(struct video_device *vdev)
{
	struct v4l2_subdev *sd = video_get_drvdata(vdev);
	sd->devnode = NULL;
	kfree(vdev);
}

int msm_cam_register_subdev_node(struct v4l2_subdev *sd,
	enum msm_cam_subdev_type sdev_type, uint8_t index)
{
	struct video_device *vdev;
	int err = 0;

	switch (sdev_type) {
	case CSIPHY_DEV:
		if (index >= MAX_NUM_CSIPHY_DEV) {
			pr_err("%s Invalid CSIPHY idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.csiphy_device[index] = sd;
		break;

	case CSID_DEV:
		if (index >= MAX_NUM_CSID_DEV) {
			pr_err("%s Invalid CSID idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.csid_device[index] = sd;
		break;

	case CSIC_DEV:
		if (index >= MAX_NUM_CSIC_DEV) {
			pr_err("%s Invalid CSIC idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.csic_device[index] = sd;
		break;

	case ISPIF_DEV:
		g_server_dev.ispif_device = sd;
		break;

	case VFE_DEV:
		if (index >= MAX_NUM_VFE_DEV) {
			pr_err("%s Invalid VFE idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.vfe_device[index] = sd;
		break;

	case VPE_DEV:
		if (index >= MAX_NUM_VPE_DEV) {
			pr_err("%s Invalid VPE idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.vpe_device[index] = sd;
		break;

	case AXI_DEV:
		if (index >= MAX_NUM_VPE_DEV) {
			pr_err("%s Invalid AXI idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.axi_device[index] = sd;
		break;

	case GESTURE_DEV:
		g_server_dev.gesture_device = sd;
		break;
	default:
		break;
	}

	if (err < 0)
		return err;

	err = v4l2_device_register_subdev(&g_server_dev.v4l2_dev, sd);
	if (err < 0) {
		pr_err("%s v4l2 subdev register failed for %d ret = %d",
			__func__, sdev_type, err);
		return err;
	}

	/* Register a device node for every subdev marked with the
	 * V4L2_SUBDEV_FL_HAS_DEVNODE flag.
	 */
	if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE))
		return err;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		err = -ENOMEM;
		goto clean_up;
	}

	video_set_drvdata(vdev, sd);
	strlcpy(vdev->name, sd->name, sizeof(vdev->name));
	vdev->v4l2_dev = &g_server_dev.v4l2_dev;
	vdev->fops = &v4l2_subdev_fops;
	vdev->release = msm_cam_release_subdev_node;
	err = __video_register_device(vdev, VFL_TYPE_SUBDEV, -1, 1,
						  sd->owner);
	if (err < 0) {
		kfree(vdev);
		goto clean_up;
	}
#if defined(CONFIG_MEDIA_CONTROLLER)
	sd->entity.info.v4l.major = VIDEO_MAJOR;
	sd->entity.info.v4l.minor = vdev->minor;
#endif
	sd->devnode = vdev;
	return 0;

clean_up:
	if (sd->devnode)
		video_unregister_device(sd->devnode);
	return err;
}

static int msm_setup_server_dev(struct platform_device *pdev)
{
	int rc = -ENODEV, i;

	D("%s\n", __func__);
	g_server_dev.server_pdev = pdev;
	g_server_dev.v4l2_dev.dev = &pdev->dev;
	g_server_dev.v4l2_dev.notify = msm_cam_server_subdev_notify;
	rc = v4l2_device_register(g_server_dev.v4l2_dev.dev,
			&g_server_dev.v4l2_dev);
	if (rc < 0)
		return -EINVAL;

	g_server_dev.video_dev = video_device_alloc();
	if (g_server_dev.video_dev == NULL) {
		pr_err("%s: video_device_alloc failed\n", __func__);
		return rc;
	}

	strlcpy(g_server_dev.video_dev->name, pdev->name,
			sizeof(g_server_dev.video_dev->name));

	g_server_dev.video_dev->v4l2_dev = &g_server_dev.v4l2_dev;
	g_server_dev.video_dev->fops = &msm_fops_server;
	g_server_dev.video_dev->ioctl_ops = &msm_ioctl_ops_server;
	g_server_dev.video_dev->release   = video_device_release;
	g_server_dev.video_dev->minor = 100;
	g_server_dev.video_dev->vfl_type = 1;

	video_set_drvdata(g_server_dev.video_dev, &g_server_dev);

	strlcpy(g_server_dev.media_dev.model, "qcamera",
		sizeof(g_server_dev.media_dev.model));
	g_server_dev.media_dev.dev = &pdev->dev;
	rc = media_device_register(&g_server_dev.media_dev);
	g_server_dev.v4l2_dev.mdev = &g_server_dev.media_dev;

	rc = video_register_device(g_server_dev.video_dev,
		VFL_TYPE_GRABBER, 100);

	mutex_init(&g_server_dev.server_lock);
	mutex_init(&g_server_dev.server_queue_lock);
	g_server_dev.pcam_active = NULL;
	g_server_dev.camera_info.num_cameras = 0;
	atomic_set(&g_server_dev.number_pcam_active, 0);
	g_server_dev.server_evt_id = 0;

	/*initialize fake video device and event queue*/

	g_server_dev.server_command_queue.pvdev = g_server_dev.video_dev;
	rc = msm_setup_v4l2_event_queue(
		&g_server_dev.server_command_queue.eventHandle,
		g_server_dev.server_command_queue.pvdev);

	if (rc < 0) {
		pr_err("%s failed to initialize event queue\n", __func__);
		video_device_release(g_server_dev.server_command_queue.pvdev);
		return rc;
	}

	for (i = 0; i < MAX_NUM_ACTIVE_CAMERA; i++) {
		struct msm_cam_server_queue *queue;
		queue = &g_server_dev.server_queue[i];
		queue->queue_active = 0;
		msm_queue_init(&queue->ctrl_q, "control");
		msm_queue_init(&queue->eventData_q, "eventdata");
	}
	return rc;
}

static long msm_server_send_v4l2_evt(void *evt)
{
	struct v4l2_event *v4l2_ev = (struct v4l2_event *)evt;
	int rc = 0;

	if (NULL == evt) {
		pr_err("%s: evt is NULL\n", __func__);
		return -EINVAL;
	}

	D("%s: evt type 0x%x\n", __func__, v4l2_ev->type);
	if ((v4l2_ev->type >= MSM_GES_APP_EVT_MIN) &&
		(v4l2_ev->type < MSM_GES_APP_EVT_MAX)) {
		msm_cam_server_subdev_notify(g_server_dev.gesture_device,
			NOTIFY_GESTURE_EVT, v4l2_ev);
	} else {
		pr_err("%s: Invalid evt %d\n", __func__, v4l2_ev->type);
		rc = -EINVAL;
	}
	D("%s: end\n", __func__);

	return rc;
}

int msm_cam_server_open_mctl_session(struct msm_cam_v4l2_device *pcam,
	int *p_active)
{
	int rc = 0;
	struct msm_cam_media_controller *pmctl = NULL;
	D("%s: %p", __func__, g_server_dev.pcam_active);
	*p_active = 0;
	if (g_server_dev.pcam_active) {
		D("%s: Active camera present return", __func__);
		return 0;
	}
	rc = msm_cam_server_open_session(&g_server_dev, pcam);
	if (rc < 0) {
		pr_err("%s: cam_server_open_session failed %d\n",
		__func__, rc);
		return rc;
	}

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl->mctl_open) {
		D("%s: media contoller is not inited\n",
			 __func__);
		rc = -ENODEV;
		return rc;
	}

	D("%s: call mctl_open\n", __func__);
	rc = pmctl->mctl_open(pmctl, MSM_APPS_ID_V4L2);

	if (rc < 0) {
		pr_err("%s: HW open failed rc = 0x%x\n",  __func__, rc);
		return rc;
	}
	pmctl->pcam_ptr = pcam;
	*p_active = 1;
	return rc;
}

int msm_cam_server_close_mctl_session(struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	struct msm_cam_media_controller *pmctl = NULL;

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl) {
		D("%s: invalid handle\n", __func__);
		return -ENODEV;
	}

	if (pmctl->mctl_release) {
		rc = pmctl->mctl_release(pmctl);
		if (rc < 0)
			pr_err("mctl_release fails %d\n", rc);
	}

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	kref_put(&pmctl->refcount, msm_release_ion_client);
#endif

	rc = msm_cam_server_close_session(&g_server_dev, pcam);
	if (rc < 0)
		pr_err("msm_cam_server_close_session fails %d\n", rc);

	return rc;
}

int msm_server_open_client(int *p_qidx)
{
	int rc = 0;
	int server_q_idx = 0;
	struct msm_cam_server_queue *queue = NULL;

	mutex_lock(&g_server_dev.server_lock);
	server_q_idx = msm_find_free_queue();
	if (server_q_idx < 0) {
		mutex_unlock(&g_server_dev.server_lock);
		return server_q_idx;
	}

	*p_qidx = server_q_idx;
	queue = &g_server_dev.server_queue[server_q_idx];
	queue->ctrl_data = kzalloc(sizeof(uint8_t) *
		max_control_command_size, GFP_KERNEL);
	msm_queue_init(&queue->ctrl_q, "control");
	msm_queue_init(&queue->eventData_q, "eventdata");
	queue->queue_active = 1;
	mutex_unlock(&g_server_dev.server_lock);
	return rc;
}

int msm_server_send_ctrl(struct msm_ctrl_cmd *out,
	int ctrl_id)
{
	int rc = 0;
	void *value;
	struct msm_queue_cmd *rcmd;
	struct msm_queue_cmd *event_qcmd;
	struct msm_ctrl_cmd *ctrlcmd;
	struct msm_cam_server_dev *server_dev = &g_server_dev;
	struct msm_device_queue *queue =
		&server_dev->server_queue[out->queue_idx].ctrl_q;

	struct v4l2_event v4l2_evt;
	struct msm_isp_event_ctrl *isp_event;
	isp_event = kzalloc(sizeof(struct msm_isp_event_ctrl), GFP_KERNEL);
	if (!isp_event) {
		pr_err("%s Insufficient memory. return", __func__);
		return -ENOMEM;
	}
	event_qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_KERNEL);
	if (!event_qcmd) {
		pr_err("%s Insufficient memory. return", __func__);
		kfree(isp_event);
		return -ENOMEM;
	}

	D("%s\n", __func__);
	mutex_lock(&server_dev->server_queue_lock);
	if (++server_dev->server_evt_id == 0)
		server_dev->server_evt_id++;

	D("%s qid %d evtid %d\n", __func__, out->queue_idx,
		server_dev->server_evt_id);
	server_dev->server_queue[out->queue_idx].evt_id =
		server_dev->server_evt_id;
	v4l2_evt.type = V4L2_EVENT_PRIVATE_START + ctrl_id;
	v4l2_evt.u.data[0] = out->queue_idx;
	/* setup event object to transfer the command; */
	isp_event->resptype = MSM_CAM_RESP_V4L2;
	isp_event->isp_data.ctrl = *out;
	isp_event->isp_data.ctrl.evt_id = server_dev->server_evt_id;

	atomic_set(&event_qcmd->on_heap, 1);
	event_qcmd->command = isp_event;

	msm_enqueue(&server_dev->server_queue[out->queue_idx].eventData_q,
				&event_qcmd->list_eventdata);

	/* now send command to config thread in userspace,
	 * and wait for results */
	v4l2_event_queue(server_dev->server_command_queue.pvdev,
					  &v4l2_evt);
	D("%s v4l2_event_queue: type = 0x%x\n", __func__, v4l2_evt.type);
	mutex_unlock(&server_dev->server_queue_lock);

	/* wait for config return status */
	D("Waiting for config status\n");
	rc = wait_event_interruptible_timeout(queue->wait,
		!list_empty_careful(&queue->list),
		msecs_to_jiffies(out->timeout_ms));
	D("Waiting is over for config status\n");
	if (list_empty_careful(&queue->list)) {
		if (!rc)
			rc = -ETIMEDOUT;
		if (rc < 0) {
			kfree(isp_event);
			pr_err("%s: wait_event error %d\n", __func__, rc);
			return rc;
		}
	}

	rcmd = msm_dequeue(queue, list_control);
	BUG_ON(!rcmd);
	D("%s Finished servicing ioctl\n", __func__);

	ctrlcmd = (struct msm_ctrl_cmd *)(rcmd->command);
	value = out->value;
	if (ctrlcmd->length > 0 && value != NULL &&
		ctrlcmd->length <= out->length)
		memcpy(value, ctrlcmd->value, ctrlcmd->length);

	memcpy(out, ctrlcmd, sizeof(struct msm_ctrl_cmd));
	out->value = value;

	kfree(ctrlcmd);
	free_qcmd(rcmd);
	kfree(isp_event);
	D("%s: rc %d\n", __func__, rc);
	/* rc is the time elapsed. */
	if (rc >= 0) {
		/* TODO: Refactor msm_ctrl_cmd::status field */
		if (out->status == 0)
			rc = -1;
		else if (out->status == 1 || out->status == 4)
			rc = 0;
		else
			rc = -EINVAL;
	}
	return rc;
}

int msm_server_close_client(int idx)
{
	int rc = 0;
	struct msm_cam_server_queue *queue = NULL;
	mutex_lock(&g_server_dev.server_lock);
	queue = &g_server_dev.server_queue[idx];
	queue->queue_active = 0;
	kfree(queue->ctrl_data);
	queue->ctrl_data = NULL;
	msm_queue_drain(&queue->ctrl_q, list_control);
	msm_drain_eventq(&queue->eventData_q);
	mutex_unlock(&g_server_dev.server_lock);
	return rc;
}

static unsigned int msm_poll_config(struct file *fp,
					struct poll_table_struct *wait)
{
	int rc = 0;
	struct msm_cam_config_dev *config = fp->private_data;
	if (config == NULL)
		return -EINVAL;

	D("%s\n", __func__);

	poll_wait(fp,
	&config->config_stat_event_queue.eventHandle.wait, wait);
	if (v4l2_event_pending(&config->config_stat_event_queue.eventHandle))
		rc |= POLLPRI;
	return rc;
}

static int msm_mmap_config(struct file *fp, struct vm_area_struct *vma)
{
	struct msm_cam_config_dev *config_cam = fp->private_data;
	int rc = 0;
	int phyaddr;
	int retval;
	unsigned long size;

	D("%s: phy_addr=0x%x", __func__, config_cam->mem_map.cookie);
	phyaddr = (int)config_cam->mem_map.cookie;
	if (!phyaddr) {
		pr_err("%s: no physical memory to map", __func__);
		return -EFAULT;
	}
	memset(&config_cam->mem_map, 0,
		sizeof(struct msm_mem_map_info));
	size = vma->vm_end - vma->vm_start;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	retval = remap_pfn_range(vma, vma->vm_start,
					phyaddr >> PAGE_SHIFT,
					size, vma->vm_page_prot);
	if (retval) {
		pr_err("%s: remap failed, rc = %d",
					__func__, retval);
		rc = -ENOMEM;
		goto end;
	}
	D("%s: phy_addr=0x%x: %08lx-%08lx, pgoff %08lx\n",
			__func__, (uint32_t)phyaddr,
			vma->vm_start, vma->vm_end, vma->vm_pgoff);
end:
	return rc;
}

static int msm_open_config(struct inode *inode, struct file *fp)
{
	int rc;
	struct msm_cam_config_dev *config_cam = container_of(inode->i_cdev,
		struct msm_cam_config_dev, config_cdev);

	D("%s: open %s\n", __func__, fp->f_path.dentry->d_name.name);

	rc = nonseekable_open(inode, fp);
	if (rc < 0) {
		pr_err("%s: nonseekable_open error %d\n", __func__, rc);
		return rc;
	}
	config_cam->use_count++;

	/* assume there is only one active camera possible*/
	config_cam->p_mctl =
		msm_cam_server_get_mctl(g_server_dev.pcam_active->mctl_handle);

	INIT_HLIST_HEAD(&config_cam->p_mctl->stats_info.pmem_stats_list);
	spin_lock_init(&config_cam->p_mctl->stats_info.pmem_stats_spinlock);

	config_cam->p_mctl->config_device = config_cam;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	kref_get(&config_cam->p_mctl->refcount);
#endif
	fp->private_data = config_cam;
	return rc;
}

static long msm_ioctl_config(struct file *fp, unsigned int cmd,
	unsigned long arg)
{

	int rc = 0;
	struct v4l2_event ev;
	struct msm_cam_config_dev *config_cam = fp->private_data;
	struct v4l2_event_subscription temp_sub;

	D("%s: cmd %d\n", __func__, _IOC_NR(cmd));
	ev.id = 0;

	switch (cmd) {
	/* memory management shall be handeld here*/
	case MSM_CAM_IOCTL_REGISTER_PMEM:
		return msm_register_pmem(
			&config_cam->p_mctl->stats_info.pmem_stats_list,
			(void __user *)arg, config_cam->p_mctl->client);
		break;

	case MSM_CAM_IOCTL_UNREGISTER_PMEM:
		return msm_pmem_table_del(
			&config_cam->p_mctl->stats_info.pmem_stats_list,
			(void __user *)arg, config_cam->p_mctl->client);
		break;

	case VIDIOC_SUBSCRIBE_EVENT:
		if (copy_from_user(&temp_sub,
			(void __user *)arg,
			sizeof(struct v4l2_event_subscription))) {
				pr_err("%s copy_from_user failed for cmd %d ",
					__func__, cmd);
				rc = -EINVAL;
				return rc;
		}
		rc = msm_server_v4l2_subscribe_event
			(&config_cam->config_stat_event_queue.eventHandle,
				 &temp_sub);
		if (rc < 0) {
			pr_err("%s: cam_v4l2_subscribe_event failed rc=%d\n",
				__func__, rc);
			return rc;
		}
		break;

	case VIDIOC_UNSUBSCRIBE_EVENT:
		if (copy_from_user(&temp_sub, (void __user *)arg,
			  sizeof(struct v4l2_event_subscription))) {
			rc = -EINVAL;
			return rc;
		}
		rc = msm_server_v4l2_unsubscribe_event
			(&config_cam->config_stat_event_queue.eventHandle,
			 &temp_sub);
		if (rc < 0) {
			pr_err("%s: cam_v4l2_unsubscribe_event failed rc=%d\n",
				__func__, rc);
			return rc;
		}
		break;

	case VIDIOC_DQEVENT: {
		void __user *u_msg_value = NULL, *user_ptr = NULL;
		struct msm_isp_event_ctrl u_isp_event;
		struct msm_isp_event_ctrl *k_isp_event;

		/* First, copy the v4l2 event structure from userspace */
		D("%s: VIDIOC_DQEVENT\n", __func__);
		if (copy_from_user(&ev, (void __user *)arg,
				sizeof(struct v4l2_event)))
			break;
		/* Next, get the pointer to event_ctrl structure
		 * embedded inside the v4l2_event.u.data array. */
		user_ptr = (void __user *)(*((uint32_t *)ev.u.data));

		/* Next, copy the userspace event ctrl structure */
		if (copy_from_user((void *)&u_isp_event, user_ptr,
				   sizeof(struct msm_isp_event_ctrl))) {
			break;
		}
		/* Save the pointer of the user allocated command buffer*/
		u_msg_value = u_isp_event.isp_data.isp_msg.data;

		/* Dequeue the event queued into the v4l2 queue*/
		rc = v4l2_event_dequeue(
			&config_cam->config_stat_event_queue.eventHandle,
			&ev, fp->f_flags & O_NONBLOCK);
		if (rc < 0) {
			pr_err("no pending events?");
			break;
		}
		/* Use k_isp_event to point to the event_ctrl structure
		 * embedded inside v4l2_event.u.data */
		k_isp_event = (struct msm_isp_event_ctrl *)
				(*((uint32_t *)ev.u.data));
		/* Copy the event structure into user struct. */
		u_isp_event = *k_isp_event;
		if (ev.type != (V4L2_EVENT_PRIVATE_START +
				MSM_CAM_RESP_DIV_FRAME_EVT_MSG) &&
				ev.type != (V4L2_EVENT_PRIVATE_START +
				MSM_CAM_RESP_MCTL_PP_EVENT)) {

			/* Restore the saved pointer of the
			 * user allocated command buffer. */
			u_isp_event.isp_data.isp_msg.data = u_msg_value;

			if (ev.type == (V4L2_EVENT_PRIVATE_START +
					MSM_CAM_RESP_STAT_EVT_MSG)) {
				if (k_isp_event->isp_data.isp_msg.len > 0) {
					void *k_msg_value =
					k_isp_event->isp_data.isp_msg.data;
					if (copy_to_user(u_msg_value,
							k_msg_value,
					 k_isp_event->isp_data.isp_msg.len)) {
						rc = -EINVAL;
						break;
					}
					kfree(k_msg_value);
				}
			}
		}
		/* Copy the event ctrl structure back
		 * into user's structure. */
		if (copy_to_user(user_ptr,
				(void *)&u_isp_event, sizeof(
				struct msm_isp_event_ctrl))) {
			rc = -EINVAL;
			break;
		}
		kfree(k_isp_event);

		/* Copy the v4l2_event structure back to the user*/
		if (copy_to_user((void __user *)arg, &ev,
				sizeof(struct v4l2_event))) {
			rc = -EINVAL;
			break;
		}
		}

		break;

	case MSM_CAM_IOCTL_V4L2_EVT_NOTIFY:
		rc = msm_v4l2_evt_notify(config_cam->p_mctl, cmd, arg);
		break;

	case MSM_CAM_IOCTL_SET_MEM_MAP_INFO:
		if (copy_from_user(&config_cam->mem_map, (void __user *)arg,
				sizeof(struct msm_mem_map_info)))
			rc = -EINVAL;
		break;

	default:{
		/* For the rest of config command, forward to media controller*/
		struct msm_cam_media_controller *p_mctl = config_cam->p_mctl;
		if (p_mctl && p_mctl->mctl_cmd) {
			rc = config_cam->p_mctl->mctl_cmd(p_mctl, cmd, arg);
		} else {
			rc = -EINVAL;
			pr_err("%s: media controller is null\n", __func__);
		}

		break;
	} /* end of default*/
	} /* end of switch*/
	return rc;
}

static int msm_close_config(struct inode *node, struct file *f)
{
	struct v4l2_event ev;
	struct v4l2_event_subscription sub;
	struct msm_isp_event_ctrl *isp_event;
	struct msm_cam_config_dev *config_cam = f->private_data;

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	D("%s Decrementing ref count of config node ", __func__);
	kref_put(&config_cam->p_mctl->refcount, msm_release_ion_client);
#endif
	sub.type = V4L2_EVENT_ALL;
	msm_server_v4l2_unsubscribe_event(
		&config_cam->config_stat_event_queue.eventHandle,
		&sub);
	while (v4l2_event_pending(
		&config_cam->config_stat_event_queue.eventHandle)) {
		v4l2_event_dequeue(
			&config_cam->config_stat_event_queue.eventHandle,
			&ev, O_NONBLOCK);
		isp_event = (struct msm_isp_event_ctrl *)
			(*((uint32_t *)ev.u.data));
		if (isp_event) {
			if (isp_event->isp_data.isp_msg.len != 0 &&
				isp_event->isp_data.isp_msg.data != NULL)
				kfree(isp_event->isp_data.isp_msg.data);
			kfree(isp_event);
		}
	}
	return 0;
}

static const struct file_operations msm_fops_config = {
	.owner = THIS_MODULE,
	.open  = msm_open_config,
	.poll  = msm_poll_config,
	.unlocked_ioctl = msm_ioctl_config,
	.mmap	= msm_mmap_config,
	.release = msm_close_config,
};

static int msm_setup_config_dev(int node, char *device_name)
{
	int rc = -ENODEV;
	struct device *device_config;
	int dev_num = node;
	dev_t devno;
	struct msm_cam_config_dev *config_cam;

	config_cam = kzalloc(sizeof(*config_cam), GFP_KERNEL);
	if (!config_cam) {
		pr_err("%s: could not allocate memory for config_device\n",
			__func__);
		return -ENOMEM;
	}

	D("%s\n", __func__);

	devno = MKDEV(MAJOR(msm_devno), dev_num+1);
	device_config = device_create(msm_class, NULL, devno, NULL, "%s%d",
		device_name, dev_num);

	if (IS_ERR(device_config)) {
		rc = PTR_ERR(device_config);
		pr_err("%s: error creating device: %d\n", __func__, rc);
		goto config_setup_fail;
	}

	cdev_init(&config_cam->config_cdev, &msm_fops_config);
	config_cam->config_cdev.owner = THIS_MODULE;

	rc = cdev_add(&config_cam->config_cdev, devno, 1);
	if (rc < 0) {
		pr_err("%s: error adding cdev: %d\n", __func__, rc);
		device_destroy(msm_class, devno);
		goto config_setup_fail;
	}
	g_server_dev.config_info.config_dev_name[dev_num] =
		dev_name(device_config);
	D("%s Connected config device %s\n", __func__,
		g_server_dev.config_info.config_dev_name[dev_num]);
	g_server_dev.config_info.config_dev_id[dev_num] =
		dev_num;

	config_cam->config_stat_event_queue.pvdev = video_device_alloc();
	if (config_cam->config_stat_event_queue.pvdev == NULL) {
		pr_err("%s: video_device_alloc failed\n", __func__);
		goto config_setup_fail;
	}

	rc = msm_setup_v4l2_event_queue(
		&config_cam->config_stat_event_queue.eventHandle,
		config_cam->config_stat_event_queue.pvdev);
	if (rc < 0) {
		pr_err("%s failed to initialize event queue\n", __func__);
		video_device_release(config_cam->config_stat_event_queue.pvdev);
		goto config_setup_fail;
	}

	return rc;

config_setup_fail:
	kfree(config_cam);
	return rc;
}

static int msm_camera_probe(struct platform_device *pdev)
{
	int rc = 0, i;
	/*for now just create a config 0 node
	  put logic here later to know how many configs to create*/
	g_server_dev.config_info.num_config_nodes = 1;

	rc = msm_isp_init_module(g_server_dev.config_info.num_config_nodes);
	if (rc < 0) {
		pr_err("Failed to initialize isp\n");
		return rc;
	}

	if (!msm_class) {
		rc = alloc_chrdev_region(&msm_devno, 0,
		g_server_dev.config_info.num_config_nodes+1, "msm_camera");
		if (rc < 0) {
			pr_err("%s: failed to allocate chrdev: %d\n", __func__,
			rc);
			return rc;
		}

		msm_class = class_create(THIS_MODULE, "msm_camera");
		if (IS_ERR(msm_class)) {
			rc = PTR_ERR(msm_class);
			pr_err("%s: create device class failed: %d\n",
			__func__, rc);
			return rc;
		}
	}

	D("creating server and config nodes\n");
	rc = msm_setup_server_dev(pdev);
	if (rc < 0) {
		pr_err("%s: failed to create server dev: %d\n", __func__,
		rc);
		return rc;
	}

	for (i = 0; i < g_server_dev.config_info.num_config_nodes; i++) {
		rc = msm_setup_config_dev(i, "config");
		if (rc < 0) {
			pr_err("%s:failed to create config dev: %d\n",
			 __func__, rc);
			return rc;
		}
	}

	msm_isp_register(&g_server_dev);
	return rc;
}

static int __exit msm_camera_exit(struct platform_device *pdev)
{
	msm_isp_unregister(&g_server_dev);
	return 0;
}

static struct platform_driver msm_cam_server_driver = {
	.probe = msm_camera_probe,
	.remove = msm_camera_exit,
	.driver = {
		.name = "msm_cam_server",
		.owner = THIS_MODULE,
	},
};

static int __init msm_cam_server_init(void)
{
	return platform_driver_register(&msm_cam_server_driver);
}

static void __exit msm_cam_server_exit(void)
{
	platform_driver_unregister(&msm_cam_server_driver);
}

module_init(msm_cam_server_init);
module_exit(msm_cam_server_exit);
MODULE_DESCRIPTION("msm camera server");
MODULE_LICENSE("GPL v2");
