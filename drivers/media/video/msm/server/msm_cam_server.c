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

#include <linux/of.h>
#include "msm_cam_server.h"
#include "msm_csid.h"
#include "msm_csic.h"
#include "msm_csiphy.h"
#include "msm_ispif.h"
#include "msm_sensor.h"
#include "msm_actuator.h"
#include "msm_csi_register.h"

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

void msm_setup_v4l2_event_queue(struct v4l2_fh *eventHandle,
	struct video_device *pvdev)
{
	v4l2_fh_init(eventHandle, pvdev);
	v4l2_fh_add(eventHandle);
}

void msm_destroy_v4l2_event_queue(struct v4l2_fh *eventHandle)
{
	v4l2_fh_del(eventHandle);
	v4l2_fh_exit(eventHandle);
}

int msm_cam_server_config_interface_map(u32 extendedmode,
	uint32_t mctl_handle, int vnode_id, int is_bayer_sensor)
{
	int i = 0;
	int rc = 0;
	int old_handle;
	int interface;

	if (vnode_id >= MAX_NUM_ACTIVE_CAMERA) {
		pr_err("%s: invalid msm_dev node id = %d", __func__, vnode_id);
		return -EINVAL;
	}
	D("%s: extendedmode = %d, vnode_id = %d, is_bayer_sensor = %d",
		__func__, extendedmode, vnode_id, is_bayer_sensor);
	switch (extendedmode) {
	case MSM_V4L2_EXT_CAPTURE_MODE_RDI:
		interface = RDI_0;
		break;
	case MSM_V4L2_EXT_CAPTURE_MODE_RDI1:
		interface = RDI_1;
		break;
	case MSM_V4L2_EXT_CAPTURE_MODE_RDI2:
		interface = RDI_2;
		break;
	default:
		interface = PIX_0;
		break;
	}

	for (i = 0; i < INTF_MAX; i++) {
		if (g_server_dev.interface_map_table[i].interface ==
							interface) {
			if (is_bayer_sensor && interface == PIX_0) {
				if (g_server_dev.
					interface_map_table[i].mctl_handle &&
					!g_server_dev.interface_map_table[i].
						is_bayer_sensor) {
					/* in simultaneous camera usecase
					 * SoC does not use PIX interface */
					g_server_dev.interface_map_table[i].
						mctl_handle = 0;
				}
			}

			if (!is_bayer_sensor && interface == PIX_0) {
				if (g_server_dev.
					interface_map_table[i].mctl_handle &&
					g_server_dev.interface_map_table[i].
						is_bayer_sensor) {
					/* In case of simultaneous camera,
					 * the YUV sensor could use PIX
					 * interface to only queue the preview
					 * or video buffers, but does not
					 * expect any notifications directly.
					 * (preview/video data is updated from
					 * postprocessing in such scenario).
					 * In such case, there is no need to
					 * update the mctl_handle in the intf
					 * map table, since the notification
					 * will not be sent directly. */
					break;
				}
			}

			old_handle =
				g_server_dev.interface_map_table[i].mctl_handle;
			if (old_handle == 0) {
				g_server_dev.interface_map_table[i].mctl_handle
					= mctl_handle;
				g_server_dev.interface_map_table[i].
					is_bayer_sensor = is_bayer_sensor;
				g_server_dev.interface_map_table[i].vnode_id
					= vnode_id;
			} else {
				if (!g_server_dev.interface_map_table[i].
					is_bayer_sensor &&
					(extendedmode ==
					MSM_V4L2_EXT_CAPTURE_MODE_PREVIEW ||
					extendedmode ==
					MSM_V4L2_EXT_CAPTURE_MODE_VIDEO ||
					extendedmode ==
					MSM_V4L2_EXT_CAPTURE_MODE_MAIN ||
					extendedmode ==
					MSM_V4L2_EXT_CAPTURE_MODE_THUMBNAIL)) {
					D("%s: SoC sensor, image_mode = %d",
					__func__, extendedmode);
					break;
				}
				if (old_handle != mctl_handle) {
					pr_err("%s: iface_map[%d] is set: %d\n",
						__func__, i, old_handle);
					rc = -EINVAL;
				}
			}
			break;
		}
	}

	if (i == INTF_MAX)
		rc = -EINVAL;
	return rc;
}

void msm_cam_server_clear_interface_map(uint32_t mctl_handle)
{
	int i;
	for (i = 0; i < INTF_MAX; i++)
		if (g_server_dev.interface_map_table[i].
			mctl_handle == mctl_handle)
			g_server_dev.interface_map_table[i].
				mctl_handle = 0;
}

struct iommu_domain *msm_cam_server_get_domain()
{
	return g_server_dev.domain;
}

int msm_cam_server_get_domain_num()
{
	return g_server_dev.domain_num;
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


static void msm_cam_server_send_error_evt(
		struct msm_cam_media_controller *pmctl, int evt_type)
{
	struct v4l2_event v4l2_ev;
	v4l2_ev.id = 0;
	v4l2_ev.type = evt_type;
	ktime_get_ts(&v4l2_ev.timestamp);
	v4l2_event_queue(pmctl->pcam_ptr->pvdev, &v4l2_ev);
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
		if (command->length > MAX_SERVER_PAYLOAD_LENGTH) {
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
				uint32_t id, struct msm_ctrl_cmd *out)
{
	int rc = 0;
	uint8_t wait_count;
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
	v4l2_evt.id = id;
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
	/* wait event may be interrupted by sugnal,
	 * in this case -ERESTARTSYS is returned and retry is needed.
	 * Now we only retry once. */
	wait_count = 2;
	do {
		rc = wait_event_interruptible_timeout(queue->wait,
			!list_empty_careful(&queue->list),
			msecs_to_jiffies(out->timeout_ms));
		wait_count--;
		if (rc != -ERESTARTSYS)
			break;
		D("%s: wait_event interrupted by signal, remain_count = %d",
			__func__, wait_count);
	} while (wait_count > 0);
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
	/* rc is the time elapsed.
	 * This means that the communication with the daemon itself was
	 * successful(irrespective of the handling of the ctrlcmd).
	 * So, just reset the rc to 0 to indicate success.
	 * Its upto the caller to parse the ctrlcmd to check the status. We
	 * dont need to parse it here. */
	if (rc >= 0)
		rc = 0;

	return rc;

ctrlcmd_alloc_fail:
	kfree(isp_event);
isp_event_alloc_fail:
	kfree(event_qcmd);
event_qcmd_alloc_fail:
	return rc;
}

int msm_server_private_general(struct msm_cam_v4l2_device *pcam,
		struct msm_camera_v4l2_ioctl_t *ioctl_ptr)
{
	struct msm_ctrl_cmd ctrlcmd;
	void *temp_data = NULL;
	int rc;
	if (ioctl_ptr->len > 0) {
		temp_data = kzalloc(ioctl_ptr->len, GFP_KERNEL);
		if (!temp_data) {
			pr_err("%s could not allocate memory\n", __func__);
			rc = -ENOMEM;
			goto end;
		}
		if (copy_from_user((void *)temp_data,
			(void __user *)ioctl_ptr->ioctl_ptr,
			ioctl_ptr->len)) {
			ERR_COPY_FROM_USER();
			rc = -EFAULT;
			goto copy_from_user_failed;
		}
	}

	mutex_lock(&pcam->vid_lock);
	ctrlcmd.type = MSM_V4L2_PRIVATE_CMD;
	ctrlcmd.length = ioctl_ptr->len;
	ctrlcmd.value = temp_data;
	ctrlcmd.timeout_ms = 1000;
	ctrlcmd.status = ioctl_ptr->id;
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[0];
	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);
	if (rc < 0)
		pr_err("%s: send event failed\n", __func__);
	else {
		if (ioctl_ptr->len > 0) {
			if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
				(void *)temp_data,
				ioctl_ptr->len)) {
				ERR_COPY_TO_USER();
				rc = -EFAULT;
			}
		}
	}
	mutex_unlock(&pcam->vid_lock);

	kfree(temp_data);
	return rc;
copy_from_user_failed:
	kfree(temp_data);
end:
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
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[
						pcam->server_queue_idx];

	/* send command to config thread in userspace, and get return value */
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);
	D("%s: rc = %d\n", __func__, rc);

	return rc;
}

/*send open command to server*/
int msm_send_open_server(struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	int idx = pcam->server_queue_idx;
	D("%s qid %d\n", __func__, pcam->server_queue_idx);
	ctrlcmd.type	   = MSM_V4L2_OPEN;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length = strnlen(
		g_server_dev.config_info.config_dev_name[idx],
		MAX_DEV_NAME_LEN)+1;
	ctrlcmd.value = (char *)g_server_dev.config_info.config_dev_name[idx];
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[idx];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);

	return rc;
}

int msm_send_close_server(struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;
	struct msm_ctrl_cmd ctrlcmd;
	D("%s qid %d\n", __func__, pcam->server_queue_idx);
	ctrlcmd.type	   = MSM_V4L2_CLOSE;
	ctrlcmd.timeout_ms = 10000;
	ctrlcmd.length	 = strnlen(g_server_dev.config_info.config_dev_name[
				pcam->server_queue_idx], MAX_DEV_NAME_LEN)+1;
	ctrlcmd.value    = (char *)g_server_dev.config_info.config_dev_name[
				pcam->server_queue_idx];
	ctrlcmd.vnode_id = pcam->vnode_id;
	ctrlcmd.queue_idx = pcam->server_queue_idx;
	ctrlcmd.config_ident = g_server_dev.config_info.config_dev_id[
						pcam->server_queue_idx];

	/* send command to config thread in usersspace, and get return value */
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);

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
	plane_info.inst_handle = pcam->dev_inst[idx]->inst_handle;
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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);

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
	plane_info.inst_handle = pcam->dev_inst[idx]->inst_handle;

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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);
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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);

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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);

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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);
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
		tmp_cmd.status = cmd_ptr->status = ctrlcmd.status;
		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
			(void *)cmd_ptr, cmd_len)) {
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
	ctrl_data = NULL;
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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);

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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);

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
	rc = msm_server_control(&g_server_dev, 0, &ctrlcmd);
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
	struct v4l2_event ev;

	D("%s: fh = 0x%x\n", __func__, (u32)fh);

	/* Undequeue all pending events and free associated
	 * msm_isp_event_ctrl  */
	while (v4l2_event_pending(fh)) {
		struct msm_isp_event_ctrl *isp_event;
		rc = v4l2_event_dequeue(fh, &ev, O_NONBLOCK);
		if (rc) {
			pr_err("%s: v4l2_event_dequeue failed %d",
						__func__, rc);
			break;
		}
		isp_event = (struct msm_isp_event_ctrl *)
			(*((uint32_t *)ev.u.data));
		if (isp_event) {
			if (isp_event->isp_data.isp_msg.len != 0 &&
				isp_event->isp_data.isp_msg.data != NULL) {
				kfree(isp_event->isp_data.isp_msg.data);
				isp_event->isp_data.isp_msg.len = 0;
				isp_event->isp_data.isp_msg.data = NULL;
			}
			kfree(isp_event);
			*((uint32_t *)ev.u.data) = 0;
		}
	}

	rc = v4l2_event_unsubscribe(fh, sub);
	D("%s: rc = %d\n", __func__, rc);
	return rc;
}

/* open an active camera session to manage the streaming logic */
static int msm_cam_server_open_session(struct msm_cam_server_dev *ps,
	struct msm_cam_v4l2_device *pcam)
{
	int rc = 0;

	D("%s\n", __func__);

	if (!ps || !pcam) {
		pr_err("%s NULL pointer passed in!\n", __func__);
		return rc;
	}

	/*
	 * The number of camera instance should be controlled by the
	 * resource manager. Currently supporting two active instances
	 */
	if (atomic_read(&ps->number_pcam_active) > 1) {
		pr_err("%s Cannot have more than two active camera %d\n",
			__func__, atomic_read(&ps->number_pcam_active));
		return -EINVAL;
	}
	/* book keeping this camera session*/
	ps->pcam_active[pcam->server_queue_idx] = pcam;
	ps->opened_pcam[pcam->vnode_id] = pcam;
	atomic_inc(&ps->number_pcam_active);

	D("config pcam = 0x%p\n", pcam);

	/* initialization the media controller module*/
	msm_mctl_init(pcam);

	return rc;
}

/* close an active camera session to server */
static int msm_cam_server_close_session(struct msm_cam_server_dev *ps,
	struct msm_cam_v4l2_device *pcam)
{
	int i;
	int rc = 0;
	D("%s\n", __func__);

	if (!ps || !pcam) {
		D("%s NULL pointer passed in!\n", __func__);
		return rc;
	}

	atomic_dec(&ps->number_pcam_active);
	ps->pcam_active[pcam->server_queue_idx] = NULL;
	ps->opened_pcam[pcam->vnode_id] = NULL;
	for (i = 0; i < INTF_MAX; i++) {
		if (ps->interface_map_table[i].mctl_handle ==
			pcam->mctl_handle)
			ps->interface_map_table[i].mctl_handle = 0;
	}
	msm_mctl_free(pcam);
	return rc;
}

static int map_imem_addresses(struct msm_cam_media_controller *mctl)
{
	int rc = 0;
	rc = msm_iommu_map_contig_buffer(
		(unsigned long)IMEM_Y_PING_OFFSET, mctl->domain_num, 0,
		((IMEM_Y_SIZE + IMEM_CBCR_SIZE + 4095) & (~4095)),
		SZ_4K, IOMMU_WRITE | IOMMU_READ,
		(unsigned long *)&mctl->ping_imem_y);
	mctl->ping_imem_cbcr = mctl->ping_imem_y + IMEM_Y_SIZE;
	if (rc < 0) {
		pr_err("%s: ping iommu mapping returned error %d\n",
			__func__, rc);
		mctl->ping_imem_y = 0;
		mctl->ping_imem_cbcr = 0;
	}
	msm_iommu_map_contig_buffer(
		(unsigned long)IMEM_Y_PONG_OFFSET, mctl->domain_num, 0,
		((IMEM_Y_SIZE + IMEM_CBCR_SIZE + 4095) & (~4095)),
		SZ_4K, IOMMU_WRITE | IOMMU_READ,
		(unsigned long *)&mctl->pong_imem_y);
	mctl->pong_imem_cbcr = mctl->pong_imem_y + IMEM_Y_SIZE;
	if (rc < 0) {
		pr_err("%s: pong iommu mapping returned error %d\n",
			 __func__, rc);
		mctl->pong_imem_y = 0;
		mctl->pong_imem_cbcr = 0;
	}
	return rc;
}

static void unmap_imem_addresses(struct msm_cam_media_controller *mctl)
{
	msm_iommu_unmap_contig_buffer(mctl->ping_imem_y,
		mctl->domain_num, 0,
		((IMEM_Y_SIZE + IMEM_CBCR_SIZE + 4095) & (~4095)));
	msm_iommu_unmap_contig_buffer(mctl->pong_imem_y,
		mctl->domain_num, 0,
		((IMEM_Y_SIZE + IMEM_CBCR_SIZE + 4095) & (~4095)));
	mctl->ping_imem_y = 0;
	mctl->ping_imem_cbcr = 0;
	mctl->pong_imem_y = 0;
	mctl->pong_imem_cbcr = 0;
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
		int i;
		mutex_lock(&g_server_dev.server_lock);
		for (i = 0; i < MAX_NUM_ACTIVE_CAMERA; i++) {
			if (g_server_dev.pcam_active[i]) {
				struct msm_cam_media_controller *pmctl = NULL;

				pmctl = msm_cam_server_get_mctl(
				g_server_dev.pcam_active[i]->mctl_handle);
				if (pmctl && pmctl->mctl_release) {
					pmctl->mctl_release(pmctl);
					/*so that it isn't closed again*/
					pmctl->mctl_release = NULL;
				}
				if (pmctl)
					msm_cam_server_send_error_evt(pmctl,
						V4L2_EVENT_PRIVATE_START +
						MSM_CAM_APP_NOTIFY_ERROR_EVENT);
			}
		}
		sub.type = V4L2_EVENT_ALL;
		v4l2_event_unsubscribe(
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
	struct msm_cam_media_controller *pmctl;

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
			MAX_SERVER_PAYLOAD_LENGTH, GFP_KERNEL);
	if (queue->ctrl_data == NULL) {
		pr_err("%s: Could not allocate memory\n", __func__);
		rc = -ENOMEM;
		goto error;
	}
	msm_queue_init(&queue->ctrl_q, "control");
	msm_queue_init(&queue->eventData_q, "eventdata");
	queue->queue_active = 1;
	rc = msm_cam_server_open_session(&g_server_dev, pcam);
	if (rc < 0) {
		pr_err("%s: cam_server_open_session failed %d\n",
			__func__, rc);
		goto error;
	}

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl) {
		pr_err("%s: invalid mctl controller", __func__);
		goto error;
	}
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		pmctl->domain = msm_cam_server_get_domain();
		pmctl->domain_num = msm_cam_server_get_domain_num();
#endif
	rc = map_imem_addresses(pmctl);
	if (rc < 0) {
		pr_err("%sFailed to map imem addresses %d\n", __func__, rc);
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
	struct msm_cam_media_controller *pmctl;

	mutex_lock(&g_server_dev.server_queue_lock);
	queue = &g_server_dev.server_queue[pcam->server_queue_idx];
	queue->queue_active = 0;
	kfree(queue->ctrl_data);
	queue->ctrl_data = NULL;
	msm_queue_drain(&queue->ctrl_q, list_control);
	msm_drain_eventq(&queue->eventData_q);
	mutex_unlock(&g_server_dev.server_queue_lock);

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl) {
		pr_err("%s: invalid mctl controller", __func__);
		return -EINVAL;
	}
	unmap_imem_addresses(pmctl);

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
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
	.vidioc_default = msm_ioctl_server,
};

static uint32_t msm_camera_server_find_mctl(
		unsigned int notification, void *arg)
{
	int i;
	uint32_t interface;
	switch (notification) {
	case NOTIFY_ISP_MSG_EVT:
		if (((struct isp_msg_event *)arg)->msg_id ==
			MSG_ID_RDI0_UPDATE_ACK)
			interface = RDI_0;
		else if (((struct isp_msg_event *)arg)->msg_id ==
			MSG_ID_RDI1_UPDATE_ACK)
			interface = RDI_1;
		else
			interface = PIX_0;
		break;
	case NOTIFY_VFE_MSG_OUT:
		if (((struct isp_msg_output *)arg)->output_id ==
					MSG_ID_OUTPUT_TERTIARY1)
			interface = RDI_0;
		else if (((struct isp_msg_output *)arg)->output_id ==
						MSG_ID_OUTPUT_TERTIARY2)
			interface = RDI_1;
		else
			interface = PIX_0;
		break;
	case NOTIFY_VFE_BUF_EVT: {
		struct msm_vfe_resp *rp;
		struct msm_frame_info *frame_info;
		uint8_t vnode_id;

		rp = (struct msm_vfe_resp *)arg;
		frame_info = rp->evt_msg.data;
		if (!frame_info) {
			interface = PIX_0;
			break;
		}
		if (frame_info->inst_handle) {
			vnode_id = GET_DEVID_MODE(frame_info->inst_handle);
			if (vnode_id < MAX_NUM_ACTIVE_CAMERA &&
				g_server_dev.opened_pcam[vnode_id]) {
				return g_server_dev.
					opened_pcam[vnode_id]->mctl_handle;
			} else {
				pr_err("%s: cannot find mctl handle", __func__);
				return 0;
			}
		} else {
			if (frame_info->path == VFE_MSG_OUTPUT_TERTIARY1)
				interface = RDI_0;
			else if (frame_info->path == VFE_MSG_OUTPUT_TERTIARY2)
				interface = RDI_1;
			else
				interface = PIX_0;
		}
		}
		break;
	case NOTIFY_AXI_RDI_SOF_COUNT: {
		struct rdi_count_msg *msg = (struct rdi_count_msg *)arg;
		interface = msg->rdi_interface;
		}
		break;
	case NOTIFY_VFE_MSG_STATS:
	case NOTIFY_VFE_MSG_COMP_STATS:
	case NOTIFY_VFE_CAMIF_ERROR:
	default:
		interface = PIX_0;
		break;
	}
	for (i = 0; i < INTF_MAX; i++) {
		if (interface == g_server_dev.interface_map_table[i].interface)
			break;
	}
	if (i == INTF_MAX) {
		pr_err("%s: Cannot find valid interface map\n", __func__);
		return -EINVAL;
	} else
		return g_server_dev.interface_map_table[i].mctl_handle;
}

static void msm_cam_server_subdev_notify(struct v4l2_subdev *sd,
				unsigned int notification, void *arg)
{
	int rc = -EINVAL;
	uint32_t mctl_handle = 0;
	struct msm_cam_media_controller *p_mctl = NULL;
	int is_gesture_evt =
		(notification == NOTIFY_GESTURE_EVT)
		|| (notification == NOTIFY_GESTURE_CAM_EVT);

	if (!is_gesture_evt) {
		mctl_handle = msm_camera_server_find_mctl(notification, arg);
		if (mctl_handle < 0) {
			pr_err("%s: Couldn't find mctl instance!\n", __func__);
			return;
		}
	}
	switch (notification) {
	case NOTIFY_ISP_MSG_EVT:
	case NOTIFY_VFE_MSG_OUT:
	case NOTIFY_VFE_PIX_SOF_COUNT:
	case NOTIFY_VFE_MSG_STATS:
	case NOTIFY_VFE_MSG_COMP_STATS:
	case NOTIFY_VFE_BUF_EVT:
		p_mctl = msm_cam_server_get_mctl(mctl_handle);
		if (p_mctl && p_mctl->isp_notify && p_mctl->vfe_sdev)
			rc = p_mctl->isp_notify(p_mctl,
				p_mctl->vfe_sdev, notification, arg);
		break;
	case NOTIFY_VFE_IRQ:{
		struct msm_vfe_cfg_cmd cfg_cmd;
		struct msm_camvfe_params vfe_params;
		cfg_cmd.cmd_type = CMD_VFE_PROCESS_IRQ;
		vfe_params.vfe_cfg = &cfg_cmd;
		vfe_params.data = arg;
		rc = v4l2_subdev_call(sd,
			core, ioctl, 0, &vfe_params);
	}
		break;
	case NOTIFY_AXI_IRQ:
		rc = v4l2_subdev_call(sd, core, ioctl, VIDIOC_MSM_AXI_IRQ, arg);
		break;
	case NOTIFY_AXI_RDI_SOF_COUNT:
		p_mctl = msm_cam_server_get_mctl(mctl_handle);
		if (p_mctl && p_mctl->axi_sdev)
			rc = v4l2_subdev_call(p_mctl->axi_sdev, core, ioctl,
				VIDIOC_MSM_AXI_RDI_COUNT_UPDATE, arg);
		break;
	case NOTIFY_PCLK_CHANGE:
		p_mctl = v4l2_get_subdev_hostdata(sd);
		if (p_mctl) {
			if (p_mctl->axi_sdev)
				rc = v4l2_subdev_call(p_mctl->axi_sdev, video,
				s_crystal_freq, *(uint32_t *)arg, 0);
			else
				rc = v4l2_subdev_call(p_mctl->vfe_sdev, video,
				s_crystal_freq, *(uint32_t *)arg, 0);
		}
		break;
	case NOTIFY_GESTURE_EVT:
		rc = v4l2_subdev_call(g_server_dev.gesture_device,
			core, ioctl, VIDIOC_MSM_GESTURE_EVT, arg);
		break;
	case NOTIFY_GESTURE_CAM_EVT:
		rc = v4l2_subdev_call(g_server_dev.gesture_device,
			core, ioctl, VIDIOC_MSM_GESTURE_CAM_EVT, arg);
		break;
	case NOTIFY_VFE_CAMIF_ERROR: {
		p_mctl = msm_cam_server_get_mctl(mctl_handle);
		if (p_mctl)
			msm_cam_server_send_error_evt(p_mctl,
				V4L2_EVENT_PRIVATE_START +
				MSM_CAM_APP_NOTIFY_ERROR_EVENT);
		break;
	}
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

/* Helper function to get the irq_idx corresponding
 * to the irq_num. */
int get_irq_idx_from_irq_num(int irq_num)
{
	int i;
	for (i = 0; i < CAMERA_SS_IRQ_MAX; i++)
		if (irq_num == g_server_dev.hw_irqmap[i].irq_num)
			return g_server_dev.hw_irqmap[i].irq_idx;

	return -EINVAL;
}

static struct v4l2_subdev  *msm_cam_find_subdev_node(
	struct v4l2_subdev **sd_list, u32 revision_num)
{
	int i = 0;
	for (i = 0; sd_list[i] != NULL; i++) {
		if (sd_list[i]->entity.revision == revision_num) {
			return sd_list[i];
			break;
		}
	}
	return NULL;
}

int msm_mctl_find_sensor_subdevs(struct msm_cam_media_controller *p_mctl,
	uint8_t csiphy_core_index, uint8_t csid_core_index)
{
	int rc = -ENODEV;

	v4l2_set_subdev_hostdata(p_mctl->sensor_sdev, p_mctl);

	rc = msm_csi_register_subdevs(p_mctl, csiphy_core_index,
		csid_core_index, &g_server_dev);
	if (rc < 0)
		pr_err("%s: Could not find sensor subdevs\n", __func__);

	return rc;
}

int msm_mctl_find_flash_subdev(struct msm_cam_media_controller *p_mctl,
	uint8_t index)
{
	if (index < MAX_NUM_FLASH_DEV)
		p_mctl->flash_sdev = g_server_dev.flash_device[index];
	return 0;
}

static irqreturn_t msm_camera_server_parse_irq(int irq_num, void *data)
{
	unsigned long flags;
	int irq_idx, i, rc;
	u32 status = 0;
	struct intr_table_entry *ind_irq_tbl;
	struct intr_table_entry *comp_irq_tbl;
	bool subdev_handled = 0;

	irq_idx = get_irq_idx_from_irq_num(irq_num);
	if (irq_idx < 0) {
		pr_err("server_parse_irq: no clients for irq #%d. returning ",
			irq_num);
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&g_server_dev.intr_table_lock, flags);
	ind_irq_tbl = &g_server_dev.irq_lkup_table.ind_intr_tbl[0];
	comp_irq_tbl = &g_server_dev.irq_lkup_table.comp_intr_tbl[0];
	if (ind_irq_tbl[irq_idx].is_composite) {
		for (i = 0; i < comp_irq_tbl[irq_idx].num_hwcore; i++) {
			if (comp_irq_tbl[irq_idx].subdev_list[i]) {
				rc = v4l2_subdev_call(
					comp_irq_tbl[irq_idx].subdev_list[i],
					core, interrupt_service_routine,
					status, &subdev_handled);
				if ((rc < 0) || !subdev_handled) {
					pr_err("server_parse_irq:Error\n"
						"handling irq %d rc = %d",
						irq_num, rc);
					/* Dispatch the irq to the remaining
					 * subdevs in the list. */
					continue;
				}
			}
		}
	} else {
		rc = v4l2_subdev_call(ind_irq_tbl[irq_idx].subdev_list[0],
			core, interrupt_service_routine,
			status, &subdev_handled);
		if ((rc < 0) || !subdev_handled) {
			pr_err("server_parse_irq: Error handling irq %d rc = %d",
				irq_num, rc);
			spin_unlock_irqrestore(&g_server_dev.intr_table_lock,
				flags);
			return IRQ_HANDLED;
		}
	}
	spin_unlock_irqrestore(&g_server_dev.intr_table_lock, flags);
	return IRQ_HANDLED;
}

/* Helper function to get the irq_idx corresponding
 * to the camera hwcore. This function should _only_
 * be invoked when the IRQ Router is configured
 * non-composite mode. */
int get_irq_idx_from_camhw_idx(int cam_hw_idx)
{
	int i;
	for (i = 0; i < MSM_CAM_HW_MAX; i++)
		if (cam_hw_idx == g_server_dev.hw_irqmap[i].cam_hw_idx)
			return g_server_dev.hw_irqmap[i].irq_idx;

	return -EINVAL;
}

static inline void update_compirq_subdev_info(
	struct intr_table_entry *irq_entry,
	uint32_t cam_hw_mask, uint8_t cam_hw_id,
	int *num_hwcore)
{
	if (cam_hw_mask & (0x1 << cam_hw_id)) {
		/* If the mask has been set for this cam hwcore
		 * update the subdev ptr......*/
		irq_entry->subdev_list[cam_hw_id] =
			g_server_dev.subdev_table[cam_hw_id];
		(*num_hwcore)++;
	} else {
		/*....else, just clear it, so that the irq will
		 * not be dispatched to this hw. */
		irq_entry->subdev_list[cam_hw_id] = NULL;
	}
}

static int msm_server_update_composite_irq_info(
	struct intr_table_entry *irq_req)
{
	int num_hwcore = 0, rc = 0;
	struct intr_table_entry *comp_irq_tbl =
		&g_server_dev.irq_lkup_table.comp_intr_tbl[0];

	comp_irq_tbl[irq_req->irq_idx].is_composite = 1;
	comp_irq_tbl[irq_req->irq_idx].irq_trigger_type =
		irq_req->irq_trigger_type;
	comp_irq_tbl[irq_req->irq_idx].num_hwcore = irq_req->num_hwcore;

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_MICRO, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_CCI, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_CSI0, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_CSI1, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_CSI2, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_CSI3, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_ISPIF, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_CPP, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_VFE0, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_VFE1, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_JPEG0, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_JPEG1, &num_hwcore);

	update_compirq_subdev_info(&comp_irq_tbl[irq_req->irq_idx],
		irq_req->cam_hw_mask, MSM_CAM_HW_JPEG2, &num_hwcore);

	if (num_hwcore != irq_req->num_hwcore) {
		pr_warn("%s Mismatch!! requested cam hwcores: %d, Mask set %d",
			__func__, irq_req->num_hwcore, num_hwcore);
		rc = -EINVAL;
	}
	return rc;
}

int msm_cam_server_request_irq(void *arg)
{
	unsigned long flags;
	int rc = 0;
	struct intr_table_entry *irq_req =  (struct intr_table_entry *)arg;
	struct intr_table_entry *ind_irq_tbl =
		&g_server_dev.irq_lkup_table.ind_intr_tbl[0];
	struct intr_table_entry *comp_irq_tbl =
		&g_server_dev.irq_lkup_table.comp_intr_tbl[0];

	if (!irq_req || !irq_req->irq_num || !irq_req->num_hwcore) {
		pr_err("%s Invalid input ", __func__);
		return -EINVAL;
	}

	if (!g_server_dev.irqr_device) {
		/* This either means, the current target does not
		 * have a IRQ Router hw or the IRQ Router device is
		 * not probed yet. The latter should not happen.
		 * In any case, just return back without updating
		 * the interrupt lookup table. */
		pr_info("%s IRQ Router hw is not present. ", __func__);
		return -ENXIO;
	}

	if (irq_req->is_composite) {
		if (irq_req->irq_idx >= CAMERA_SS_IRQ_0 &&
				irq_req->irq_idx < CAMERA_SS_IRQ_MAX) {
			spin_lock_irqsave(&g_server_dev.intr_table_lock, flags);
			/* Update the composite irq information into
			 * the composite irq lookup table.... */
			if (msm_server_update_composite_irq_info(irq_req)) {
				pr_err("%s Invalid configuration", __func__);
				spin_unlock_irqrestore(
					&g_server_dev.intr_table_lock, flags);
				return -EINVAL;
			}
			spin_unlock_irqrestore(&g_server_dev.intr_table_lock,
				flags);
			/*...and then update the corresponding entry
			 * in the individual irq lookup table to indicate
			 * that this IRQ is a composite irq and needs to be
			 * sent to multiple subdevs. */
			ind_irq_tbl[irq_req->irq_idx].is_composite = 1;
			rc = request_irq(comp_irq_tbl[irq_req->irq_idx].irq_num,
				msm_camera_server_parse_irq,
				irq_req->irq_trigger_type,
				ind_irq_tbl[irq_req->irq_idx].dev_name,
				ind_irq_tbl[irq_req->irq_idx].data);
			if (rc < 0) {
				pr_err("%s: request_irq failed for %s\n",
					__func__, irq_req->dev_name);
				return -EBUSY;
			}
		} else {
			pr_err("%s Invalid irq_idx %d ",
				__func__, irq_req->irq_idx);
			return -EINVAL;
		}
	} else {
		if (irq_req->cam_hw_idx >= MSM_CAM_HW_MICRO &&
				irq_req->cam_hw_idx < MSM_CAM_HW_MAX) {
			/* Update the irq information into
			 * the individual irq lookup table.... */
			irq_req->irq_idx =
				get_irq_idx_from_camhw_idx(irq_req->cam_hw_idx);
			if (irq_req->irq_idx < 0) {
				pr_err("%s Invalid hw index %d ", __func__,
					irq_req->cam_hw_idx);
				return -EINVAL;
			}
			spin_lock_irqsave(&g_server_dev.intr_table_lock, flags);
			/* Make sure the composite irq is not configured for
			 * this IRQ already. */
			BUG_ON(ind_irq_tbl[irq_req->irq_idx].is_composite);

			ind_irq_tbl[irq_req->irq_idx] = *irq_req;
			/* irq_num is stored inside the server's hw_irqmap
			 * during the device subdevice registration. */
			ind_irq_tbl[irq_req->irq_idx].irq_num =
			g_server_dev.hw_irqmap[irq_req->irq_idx].irq_num;

			/*...and clear the corresponding entry in the
			 * compsoite irq lookup table to indicate that this
			 * IRQ will only be dispatched to single subdev. */
			memset(&comp_irq_tbl[irq_req->irq_idx], 0,
					sizeof(struct intr_table_entry));
			D("%s Saving Entry %d %d %d %p",
			__func__,
			ind_irq_tbl[irq_req->irq_idx].irq_num,
			ind_irq_tbl[irq_req->irq_idx].cam_hw_idx,
			ind_irq_tbl[irq_req->irq_idx].is_composite,
			ind_irq_tbl[irq_req->irq_idx].subdev_list[0]);

			spin_unlock_irqrestore(&g_server_dev.intr_table_lock,
				flags);

			rc = request_irq(ind_irq_tbl[irq_req->irq_idx].irq_num,
				msm_camera_server_parse_irq,
				irq_req->irq_trigger_type,
				ind_irq_tbl[irq_req->irq_idx].dev_name,
				ind_irq_tbl[irq_req->irq_idx].data);
			if (rc < 0) {
				pr_err("%s: request_irq failed for %s\n",
					__func__, irq_req->dev_name);
				return -EBUSY;
			}
		} else {
			pr_err("%s Invalid hw index %d ", __func__,
				irq_req->cam_hw_idx);
			return -EINVAL;
		}
	}
	D("%s Successfully requested for IRQ for device %s ", __func__,
		irq_req->dev_name);
	return rc;
}

int msm_cam_server_update_irqmap(
	struct msm_cam_server_irqmap_entry *irqmap_entry)
{
	if (!irqmap_entry || (irqmap_entry->irq_idx < CAMERA_SS_IRQ_0 ||
		irqmap_entry->irq_idx >= CAMERA_SS_IRQ_MAX)) {
		pr_err("%s Invalid irqmap entry ", __func__);
		return -EINVAL;
	}
	g_server_dev.hw_irqmap[irqmap_entry->irq_idx] = *irqmap_entry;
	return 0;
}

static int msm_cam_server_register_subdev(struct v4l2_device *v4l2_dev,
	struct v4l2_subdev *sd)
{
	int rc = 0;
	struct video_device *vdev;

	if (v4l2_dev == NULL || sd == NULL || !sd->name[0]) {
		pr_err("%s Invalid input ", __func__);
		return -EINVAL;
	}

	rc = v4l2_device_register_subdev(v4l2_dev, sd);
	if (rc < 0) {
		pr_err("%s v4l2 subdev register failed for %s ret = %d",
			__func__, sd->name, rc);
		return rc;
	}

	/* Register a device node for every subdev marked with the
	 * V4L2_SUBDEV_FL_HAS_DEVNODE flag.
	 */
	if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE))
		return rc;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		pr_err("%s Not enough memory ", __func__);
		rc = -ENOMEM;
		goto clean_up;
	}

	video_set_drvdata(vdev, sd);
	strlcpy(vdev->name, sd->name, sizeof(vdev->name));
	vdev->v4l2_dev = v4l2_dev;
	vdev->fops = &v4l2_subdev_fops;
	vdev->release = msm_cam_release_subdev_node;
	rc = __video_register_device(vdev, VFL_TYPE_SUBDEV, -1, 1,
						  sd->owner);
	if (rc < 0) {
		pr_err("%s Error registering video device %s", __func__,
			sd->name);
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
	return rc;
}

static int msm_cam_server_fill_sdev_irqnum(int cam_hw_idx,
	int irq_num)
{
	int rc = 0, irq_idx;
	irq_idx = get_irq_idx_from_camhw_idx(cam_hw_idx);
	if (irq_idx < 0) {
		pr_err("%s Invalid cam_hw_idx %d ", __func__, cam_hw_idx);
		rc = -EINVAL;
	} else {
		g_server_dev.hw_irqmap[irq_idx].irq_num = irq_num;
	}
	return rc;
}

int msm_cam_register_subdev_node(struct v4l2_subdev *sd,
	struct msm_cam_subdev_info *sd_info)
{
	int err = 0, cam_hw_idx;
	uint8_t sdev_type, index;

	sdev_type = sd_info->sdev_type;
	index     = sd_info->sd_index;

	switch (sdev_type) {
	case SENSOR_DEV:
		if (index >= MAX_NUM_SENSOR_DEV) {
			pr_err("%s Invalid sensor idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.sensor_device[index] = sd;
		break;

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
		cam_hw_idx = MSM_CAM_HW_CSI0 + index;
		g_server_dev.csid_device[index] = sd;
		if (g_server_dev.irqr_device) {
			g_server_dev.subdev_table[cam_hw_idx] = sd;
			err = msm_cam_server_fill_sdev_irqnum(cam_hw_idx,
				sd_info->irq_num);
		}
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
		if (index >= MAX_NUM_ISPIF_DEV) {
			pr_err("%s Invalid ISPIF idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		cam_hw_idx = MSM_CAM_HW_ISPIF + index;
		g_server_dev.ispif_device[index] = sd;
		if (g_server_dev.irqr_device) {
			g_server_dev.subdev_table[cam_hw_idx] = sd;
			err = msm_cam_server_fill_sdev_irqnum(cam_hw_idx,
				sd_info->irq_num);
		}
		break;

	case VFE_DEV:
		if (index >= MAX_NUM_VFE_DEV) {
			pr_err("%s Invalid VFE idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		cam_hw_idx = MSM_CAM_HW_VFE0 + index;
		g_server_dev.vfe_device[index] = sd;
		if (g_server_dev.irqr_device) {
			g_server_dev.subdev_table[cam_hw_idx] = sd;
			err = msm_cam_server_fill_sdev_irqnum(cam_hw_idx,
				sd_info->irq_num);
		}
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
		if (index >= MAX_NUM_AXI_DEV) {
			pr_err("%s Invalid AXI idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.axi_device[index] = sd;
		break;

	case GESTURE_DEV:
		g_server_dev.gesture_device = sd;
		break;

	case IRQ_ROUTER_DEV:
		g_server_dev.irqr_device = sd;

	case CPP_DEV:
		if (index >= MAX_NUM_CPP_DEV) {
			pr_err("%s Invalid CPP idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.cpp_device[index] = sd;
		break;
	case CCI_DEV:
		g_server_dev.cci_device = sd;
		if (g_server_dev.irqr_device) {
			if (index >= MAX_NUM_CCI_DEV) {
				pr_err("%s Invalid CCI idx %d", __func__,
					index);
				err = -EINVAL;
				break;
			}
			cam_hw_idx = MSM_CAM_HW_CCI + index;
			g_server_dev.subdev_table[cam_hw_idx] = sd;
			err = msm_cam_server_fill_sdev_irqnum(MSM_CAM_HW_CCI,
				sd_info->irq_num);
		}
		break;

	case FLASH_DEV:
		if (index >= MAX_NUM_FLASH_DEV) {
			pr_err("%s Invalid flash idx %d", __func__, index);
			err = -EINVAL;
			break;
		}
		g_server_dev.flash_device[index] = sd;
		break;

	default:
		break;
	}

	if (err < 0)
		return err;

	err = msm_cam_server_register_subdev(&g_server_dev.v4l2_dev, sd);
	return err;
}

#ifdef CONFIG_MSM_IOMMU
static int camera_register_domain(void)
{
	struct msm_iova_partition camera_fw_partition = {
		.start = SZ_128K,
		.size = SZ_2G - SZ_128K,
	};
	struct msm_iova_layout camera_fw_layout = {
		.partitions = &camera_fw_partition,
		.npartitions = 1,
		.client_name = "camera_isp",
		.domain_flags = 0,
	};

	return msm_register_domain(&camera_fw_layout);
}
#endif

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
	g_server_dev.video_dev->vfl_type = VFL_TYPE_GRABBER;

	video_set_drvdata(g_server_dev.video_dev, &g_server_dev);

	strlcpy(g_server_dev.media_dev.model, QCAMERA_SERVER_NAME,
		sizeof(g_server_dev.media_dev.model));
	g_server_dev.media_dev.dev = &pdev->dev;
	rc = media_device_register(&g_server_dev.media_dev);
	g_server_dev.v4l2_dev.mdev = &g_server_dev.media_dev;
	media_entity_init(&g_server_dev.video_dev->entity, 0, NULL, 0);
	g_server_dev.video_dev->entity.type = MEDIA_ENT_T_DEVNODE_V4L;
	g_server_dev.video_dev->entity.group_id = QCAMERA_VNODE_GROUP_ID;

	rc = video_register_device(g_server_dev.video_dev,
		VFL_TYPE_GRABBER, 100);

	g_server_dev.video_dev->entity.name =
		video_device_node_name(g_server_dev.video_dev);

	mutex_init(&g_server_dev.server_lock);
	mutex_init(&g_server_dev.server_queue_lock);
	spin_lock_init(&g_server_dev.intr_table_lock);
	memset(&g_server_dev.irq_lkup_table, 0,
			sizeof(struct irqmgr_intr_lkup_table));
	g_server_dev.camera_info.num_cameras = 0;
	atomic_set(&g_server_dev.number_pcam_active, 0);
	g_server_dev.server_evt_id = 0;

	/*initialize fake video device and event queue*/

	g_server_dev.server_command_queue.pvdev = g_server_dev.video_dev;
	msm_setup_v4l2_event_queue(
		&g_server_dev.server_command_queue.eventHandle,
		g_server_dev.server_command_queue.pvdev);

	for (i = 0; i < MAX_NUM_ACTIVE_CAMERA; i++) {
		struct msm_cam_server_queue *queue;
		queue = &g_server_dev.server_queue[i];
		queue->queue_active = 0;
		msm_queue_init(&queue->ctrl_q, "control");
		msm_queue_init(&queue->eventData_q, "eventdata");
		g_server_dev.pcam_active[i] = NULL;
	}

	for (i = 0; i < INTF_MAX; i++) {
		g_server_dev.interface_map_table[i].interface = 0x01 << i;
		g_server_dev.interface_map_table[i].mctl_handle = 0;
	}
#ifdef CONFIG_MSM_IOMMU
	g_server_dev.domain_num = camera_register_domain();
	if (g_server_dev.domain_num < 0) {
		pr_err("%s: could not register domain\n", __func__);
		rc = -ENODEV;
		return rc;
	}
	g_server_dev.domain =
		msm_get_iommu_domain(g_server_dev.domain_num);
	if (!g_server_dev.domain) {
		pr_err("%s: cannot find domain\n", __func__);
		rc = -ENODEV;
		return rc;
	}
#endif
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
	int i = 0;
	struct msm_cam_media_controller *pmctl = NULL;
	*p_active = 0;

	for (i = 0; i < MAX_NUM_ACTIVE_CAMERA; i++) {
		if (NULL != g_server_dev.pcam_active[i]) {
			pr_info("%s: Active camera present return", __func__);
			return 0;
		}
	}

	rc = msm_cam_server_open_session(&g_server_dev, pcam);
	if (rc < 0) {
		pr_err("%s: cam_server_open_session failed %d\n",
		__func__, rc);
		return rc;
	}

	pmctl = msm_cam_server_get_mctl(pcam->mctl_handle);
	if (!pmctl || !pmctl->mctl_open) {
		D("%s: media contoller is not inited\n",
			 __func__);
		rc = -ENODEV;
		return rc;
	}

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		pmctl->domain = msm_cam_server_get_domain();
		pmctl->domain_num = msm_cam_server_get_domain_num();
#endif

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

	if (pmctl->mctl_release)
		pmctl->mctl_release(pmctl);

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
		MAX_SERVER_PAYLOAD_LENGTH, GFP_KERNEL);
	if (!queue->ctrl_data) {
		pr_err("%s: Could not find memory\n", __func__);
		return -ENOMEM;
	}
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
	v4l2_evt.type = V4L2_EVENT_PRIVATE_START + ctrl_id;
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
	config_cam->p_mctl = msm_cam_server_get_mctl(
		g_server_dev.pcam_active[config_cam->dev_num]->mctl_handle);
	if (!config_cam->p_mctl) {
		pr_err("%s: cannot find mctl\n", __func__);
		return -ENODEV;
	}

	INIT_HLIST_HEAD(&config_cam->p_mctl->stats_info.pmem_stats_list);
	spin_lock_init(&config_cam->p_mctl->stats_info.pmem_stats_spinlock);

	config_cam->p_mctl->config_device = config_cam;
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	kref_get(&config_cam->p_mctl->refcount);
#endif
	fp->private_data = config_cam;
	return rc;
}

static int msm_set_mctl_subdev(struct msm_cam_media_controller *pmctl,
	struct msm_mctl_set_sdev_data *set_data)
{
	int rc = 0;
	struct v4l2_subdev *temp_sdev = NULL;
	switch (set_data->sdev_type) {
	case CSIPHY_DEV:
		pmctl->csiphy_sdev = msm_cam_find_subdev_node
			(&g_server_dev.csiphy_device[0], set_data->revision);
		temp_sdev = pmctl->csiphy_sdev;
		break;
	case CSID_DEV:
		pmctl->csid_sdev = msm_cam_find_subdev_node
			(&g_server_dev.csid_device[0], set_data->revision);
		temp_sdev = pmctl->csid_sdev;
		break;
	case CSIC_DEV:
		pmctl->csic_sdev = msm_cam_find_subdev_node
			(&g_server_dev.csic_device[0], set_data->revision);
		temp_sdev = pmctl->csic_sdev;
		break;
	case ISPIF_DEV:
		pmctl->ispif_sdev = msm_cam_find_subdev_node
			(&g_server_dev.ispif_device[0], set_data->revision);
		temp_sdev = pmctl->ispif_sdev;
		break;
	case VFE_DEV:
		pmctl->vfe_sdev = msm_cam_find_subdev_node
			(&g_server_dev.vfe_device[0], set_data->revision);
		temp_sdev = pmctl->vfe_sdev;
		break;
	case AXI_DEV:
		pmctl->axi_sdev = msm_cam_find_subdev_node
			(&g_server_dev.axi_device[0], set_data->revision);
		temp_sdev = pmctl->axi_sdev;
		break;
	case VPE_DEV:
		pmctl->vpe_sdev = msm_cam_find_subdev_node
			(&g_server_dev.vpe_device[0], set_data->revision);
		temp_sdev = pmctl->vpe_sdev;
		break;
	default:
		rc = -EINVAL;
		break;
	}
	if (temp_sdev != NULL)
		v4l2_set_subdev_hostdata(temp_sdev, pmctl);
	else
		pr_err("%s: Could not find subdev\n", __func__);
	return rc;
}

static int msm_unset_mctl_subdev(struct msm_cam_media_controller *pmctl,
	struct msm_mctl_set_sdev_data *set_data)
{
	int rc = 0;
	switch (set_data->sdev_type) {
	case CSIPHY_DEV:
		pmctl->csiphy_sdev = NULL;
		break;
	case CSID_DEV:
		pmctl->csid_sdev = NULL;
		break;
	case CSIC_DEV:
		pmctl->csic_sdev = NULL;
		break;
	case ISPIF_DEV:
		pmctl->ispif_sdev = NULL;
		break;
	case VFE_DEV:
		pmctl->vfe_sdev = NULL;
		break;
	case AXI_DEV:
		pmctl->axi_sdev = NULL;
		break;
	case VPE_DEV:
		pmctl->vpe_sdev = NULL;
		break;
	default:
		rc = -EINVAL;
		break;
	}
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
			(void __user *)arg, config_cam->p_mctl->client,
			config_cam->p_mctl->domain_num);
		break;

	case MSM_CAM_IOCTL_UNREGISTER_PMEM:
		return msm_pmem_table_del(
			&config_cam->p_mctl->stats_info.pmem_stats_list,
			(void __user *)arg, config_cam->p_mctl->client,
			config_cam->p_mctl->domain_num);
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
			rc = -EFAULT;
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
			rc = -EFAULT;
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
					k_msg_value = NULL;
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
		k_isp_event = NULL;

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

	case MSM_CAM_IOCTL_SET_MCTL_SDEV:{
		struct msm_mctl_set_sdev_data set_data;
		if (copy_from_user(&set_data, (void __user *)arg,
			sizeof(struct msm_mctl_set_sdev_data))) {
			ERR_COPY_FROM_USER();
			rc = -EINVAL;
			break;
		}
		rc = msm_set_mctl_subdev(config_cam->p_mctl, &set_data);
		break;
	}

	case MSM_CAM_IOCTL_UNSET_MCTL_SDEV:{
		struct msm_mctl_set_sdev_data set_data;
		if (copy_from_user(&set_data, (void __user *)arg,
			sizeof(struct msm_mctl_set_sdev_data))) {
			ERR_COPY_FROM_USER();
			rc = -EINVAL;
			break;
		}
		rc = msm_unset_mctl_subdev(config_cam->p_mctl, &set_data);
		break;
	}

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
	struct v4l2_event_subscription sub;
	struct msm_cam_config_dev *config_cam = f->private_data;

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	D("%s Decrementing ref count of config node ", __func__);
	kref_put(&config_cam->p_mctl->refcount, msm_release_ion_client);
#endif
	sub.type = V4L2_EVENT_ALL;
	msm_server_v4l2_unsubscribe_event(
		&config_cam->config_stat_event_queue.eventHandle,
		&sub);
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

	/* v4l2_fh support */
	spin_lock_init(&config_cam->config_stat_event_queue.pvdev->fh_lock);
	INIT_LIST_HEAD(&config_cam->config_stat_event_queue.pvdev->fh_list);
	msm_setup_v4l2_event_queue(
		&config_cam->config_stat_event_queue.eventHandle,
		config_cam->config_stat_event_queue.pvdev);
	config_cam->dev_num = dev_num;

	return rc;

config_setup_fail:
	kfree(config_cam);
	return rc;
}

static int __devinit msm_camera_probe(struct platform_device *pdev)
{
	int rc = 0, i;
	memset(&g_server_dev, 0, sizeof(struct msm_cam_server_dev));
	/*for now just create two config nodes
	  put logic here later to know how many configs to create*/
	g_server_dev.config_info.num_config_nodes = 2;

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

	return rc;
}

static int __exit msm_camera_exit(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id msm_cam_server_dt_match[] = {
	{.compatible = "qcom,cam_server"},
}

MODULE_DEVICE_TABLE(of, msm_cam_server_dt_match);

static struct platform_driver msm_cam_server_driver = {
	.probe = msm_camera_probe,
	.remove = msm_camera_exit,
	.driver = {
		.name = "msm_cam_server",
		.owner = THIS_MODULE,
		.of_match_table = msm_cam_server_dt_match,
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
