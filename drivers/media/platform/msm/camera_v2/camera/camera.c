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
 */

#include <linux/of.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/ioctl.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/videodev2.h>
#include <linux/msm_ion.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <media/v4l2-fh.h>
#include <media/videobuf2-v4l2.h>

#include "camera.h"
#include "msm.h"
#include "msm_vb2.h"

#define fh_to_private(__fh) \
	container_of(__fh, struct camera_v4l2_private, fh)

struct camera_v4l2_private {
	struct v4l2_fh fh;
	unsigned int stream_id;
	unsigned int is_vb2_valid; /*0 if no vb2 buffers on stream, else 1*/
	struct vb2_queue vb2_q;
	bool stream_created;
	struct mutex lock;
};

static void camera_pack_event(struct file *filep, int evt_id,
	int command, int value, struct v4l2_event *event)
{
	struct msm_v4l2_event_data *event_data =
		(struct msm_v4l2_event_data *)&event->u.data[0];
	struct msm_video_device *pvdev = video_drvdata(filep);
	struct camera_v4l2_private *sp = fh_to_private(filep->private_data);

	/* always MSM_CAMERA_V4L2_EVENT_TYPE */
	event->type = MSM_CAMERA_V4L2_EVENT_TYPE;
	event->id = evt_id;
	event_data->command = command;
	event_data->session_id = pvdev->vdev->num;
	event_data->stream_id = sp->stream_id;
	event_data->arg_value = value;
}

static int camera_check_event_status(struct v4l2_event *event)
{
	struct msm_v4l2_event_data *event_data =
		(struct msm_v4l2_event_data *)&event->u.data[0];

	if (event_data->status > MSM_CAMERA_ERR_EVT_BASE) {
		pr_err("%s : event_data status out of bounds\n",
				__func__);
		pr_err("%s : Line %d event_data->status 0X%x\n",
				__func__, __LINE__, event_data->status);

		switch (event_data->status) {
		case MSM_CAMERA_ERR_CMD_FAIL:
		case MSM_CAMERA_ERR_MAPPING:
			return -EFAULT;
		case MSM_CAMERA_ERR_DEVICE_BUSY:
			return -EBUSY;
		default:
			return -EFAULT;
		}
	}

	return 0;
}

static int camera_v4l2_querycap(struct file *filep, void *fh,
	struct v4l2_capability *cap)
{
	int rc;
	struct v4l2_event event;

	if (msm_is_daemon_present() == false)
		return 0;

	/* can use cap->driver to make differentiation */
	camera_pack_event(filep, MSM_CAMERA_GET_PARM,
		MSM_CAMERA_PRIV_QUERY_CAP, -1, &event);

	rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
	if (rc < 0)
		return rc;

	rc = camera_check_event_status(&event);

	return rc;
}

static int camera_v4l2_s_crop(struct file *filep, void *fh,
	const struct v4l2_crop *crop)
{
	int rc = 0;
	struct v4l2_event event;

	if (msm_is_daemon_present() == false)
		return 0;

	if (crop->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {

		camera_pack_event(filep, MSM_CAMERA_SET_PARM,
			MSM_CAMERA_PRIV_S_CROP, -1, &event);

		rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		if (rc < 0)
			return rc;

		rc = camera_check_event_status(&event);
	}

	return rc;
}

static int camera_v4l2_g_crop(struct file *filep, void *fh,
	struct v4l2_crop *crop)
{
	int rc = 0;
	struct v4l2_event event;

	if (msm_is_daemon_present() == false)
		return 0;

	if (crop->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		camera_pack_event(filep, MSM_CAMERA_GET_PARM,
			MSM_CAMERA_PRIV_G_CROP, -1, &event);

		rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		if (rc < 0)
			return rc;

		rc = camera_check_event_status(&event);
	}

	return rc;
}

static int camera_v4l2_queryctrl(struct file *filep, void *fh,
	struct v4l2_queryctrl *ctrl)
{
	int rc = 0;
	struct v4l2_event event;

	if (msm_is_daemon_present() == false)
		return 0;

	if (ctrl->type == V4L2_CTRL_TYPE_MENU) {

		camera_pack_event(filep, MSM_CAMERA_GET_PARM,
			ctrl->id, -1, &event);

		rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		if (rc < 0)
			return rc;

		rc = camera_check_event_status(&event);
	}

	return rc;
}

static int camera_v4l2_g_ctrl(struct file *filep, void *fh,
	struct v4l2_control *ctrl)
{
	int rc = 0;
	struct v4l2_event event;
	struct msm_video_device *pvdev = video_drvdata(filep);
	unsigned int session_id = pvdev->vdev->num;

	if (ctrl->id >= V4L2_CID_PRIVATE_BASE) {
		if (ctrl->id == MSM_CAMERA_PRIV_G_SESSION_ID) {
			ctrl->value = session_id;
		} else {
			camera_pack_event(filep, MSM_CAMERA_GET_PARM,
					ctrl->id, -1, &event);

			rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
			if (rc < 0)
				return rc;

			rc = camera_check_event_status(&event);
		}
	}

	return rc;
}

static int camera_v4l2_s_ctrl(struct file *filep, void *fh,
	struct v4l2_control *ctrl)
{
	int rc = 0;
	struct v4l2_event event;
	struct msm_v4l2_event_data *event_data;

	if (ctrl->id >= V4L2_CID_PRIVATE_BASE) {
		camera_pack_event(filep, MSM_CAMERA_SET_PARM, ctrl->id,
		ctrl->value, &event);

		rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		if (rc < 0)
			return rc;
		event_data = (struct msm_v4l2_event_data *)event.u.data;
		ctrl->value = event_data->ret_value;
		rc = camera_check_event_status(&event);
	}

	return rc;
}

static int camera_v4l2_reqbufs(struct file *filep, void *fh,
	struct v4l2_requestbuffers *req)
{
	int ret;
	struct msm_session *session;
	struct camera_v4l2_private *sp = fh_to_private(fh);
	struct msm_video_device *pvdev = video_drvdata(filep);
	unsigned int session_id = pvdev->vdev->num;

	session = msm_session_find(session_id);
	if (WARN_ON(!session))
		return -EIO;
	mutex_lock(&sp->lock);
	ret = vb2_reqbufs(&sp->vb2_q, req);
	mutex_unlock(&sp->lock);
	return ret;
}

static int camera_v4l2_querybuf(struct file *filep, void *fh,
	struct v4l2_buffer *pb)
{
	return 0;
}

static int camera_v4l2_qbuf(struct file *filep, void *fh,
	struct v4l2_buffer *pb)
{
	int ret;
	struct msm_session *session;
	struct camera_v4l2_private *sp = fh_to_private(fh);
		struct msm_video_device *pvdev = video_drvdata(filep);
	unsigned int session_id = pvdev->vdev->num;

	session = msm_session_find(session_id);
	if (WARN_ON(!session))
		return -EIO;
	mutex_lock(&sp->lock);
	ret = vb2_qbuf(&sp->vb2_q, pb);
	mutex_unlock(&sp->lock);
	return ret;
}

static int camera_v4l2_dqbuf(struct file *filep, void *fh,
	struct v4l2_buffer *pb)
{
	int ret;
	struct msm_session *session;
	struct camera_v4l2_private *sp = fh_to_private(fh);
		struct msm_video_device *pvdev = video_drvdata(filep);
	unsigned int session_id = pvdev->vdev->num;

	session = msm_session_find(session_id);
	if (WARN_ON(!session))
		return -EIO;
	mutex_lock(&sp->lock);
	ret = vb2_dqbuf(&sp->vb2_q, pb, filep->f_flags & O_NONBLOCK);
	mutex_unlock(&sp->lock);
	return ret;
}

static int camera_v4l2_streamon(struct file *filep, void *fh,
	enum v4l2_buf_type buf_type)
{
	struct v4l2_event event;
	int rc;
	struct camera_v4l2_private *sp = fh_to_private(fh);

	mutex_lock(&sp->lock);
	rc = vb2_streamon(&sp->vb2_q, buf_type);
	mutex_unlock(&sp->lock);

	if (msm_is_daemon_present() == false)
		return 0;

	camera_pack_event(filep, MSM_CAMERA_SET_PARM,
		MSM_CAMERA_PRIV_STREAM_ON, -1, &event);

	rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
	if (rc < 0)
		return rc;

	rc = camera_check_event_status(&event);
	return rc;
}

static int camera_v4l2_streamoff(struct file *filep, void *fh,
		enum v4l2_buf_type buf_type)
{
	struct v4l2_event event;
	int rc = 0;
	struct camera_v4l2_private *sp = fh_to_private(fh);

	if (msm_is_daemon_present() != false) {
		camera_pack_event(filep, MSM_CAMERA_SET_PARM,
			MSM_CAMERA_PRIV_STREAM_OFF, -1, &event);

		rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		if (rc < 0)
			return rc;
		rc = camera_check_event_status(&event);
	}
	mutex_lock(&sp->lock);
	vb2_streamoff(&sp->vb2_q, buf_type);
	mutex_unlock(&sp->lock);
	return rc;
}

static int camera_v4l2_g_fmt_vid_cap_mplane(struct file *filep, void *fh,
	struct v4l2_format *pfmt)
{
	int rc = -EINVAL;

	if (msm_is_daemon_present() == false)
		return 0;

	if (pfmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		struct v4l2_event event;

		camera_pack_event(filep, MSM_CAMERA_GET_PARM,
			MSM_CAMERA_PRIV_G_FMT, -1, &event);

		rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		if (rc < 0)
			return rc;

		rc = camera_check_event_status(&event);
	}

	return rc;
}

static int camera_v4l2_s_fmt_vid_cap_mplane(struct file *filep, void *fh,
	struct v4l2_format *pfmt)
{
	int rc = 0;
	int i = 0;
	struct v4l2_event event;
	struct camera_v4l2_private *sp = fh_to_private(fh);
	struct msm_v4l2_format_data *user_fmt;

	if (pfmt->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {

		mutex_lock(sp->vb2_q.lock);
		if (WARN_ON(!sp->vb2_q.drv_priv)) {
			rc = -ENOMEM;
			mutex_unlock(sp->vb2_q.lock);
			goto done;
	}
		memcpy(sp->vb2_q.drv_priv, pfmt->fmt.raw_data,
			sizeof(struct msm_v4l2_format_data));
		user_fmt = (struct msm_v4l2_format_data *)sp->vb2_q.drv_priv;

		pr_debug("%s: num planes :%c\n", __func__,
					user_fmt->num_planes);
		/* num_planes need to bound checked, otherwise for loop
		 * can execute forever
		 */
		if (WARN_ON(user_fmt->num_planes > VIDEO_MAX_PLANES)) {
			rc = -EINVAL;
			mutex_unlock(sp->vb2_q.lock);
			goto done;
		}
		for (i = 0; i < user_fmt->num_planes; i++)
			pr_debug("%s: plane size[%d]\n", __func__,
					user_fmt->plane_sizes[i]);
		mutex_unlock(sp->vb2_q.lock);
		if (msm_is_daemon_present() != false) {
			camera_pack_event(filep, MSM_CAMERA_SET_PARM,
				MSM_CAMERA_PRIV_S_FMT, -1, &event);

			rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
			if (rc < 0)
				goto done;
			rc = camera_check_event_status(&event);
			if (rc < 0)
				goto done;
		}
		sp->is_vb2_valid = 1;
	}
done:
	return rc;
}

static int camera_v4l2_try_fmt_vid_cap_mplane(struct file *filep, void *fh,
	struct v4l2_format *pfmt)
{
	return 0;
}


static int camera_v4l2_g_parm(struct file *filep, void *fh,
	struct v4l2_streamparm *a)
{
	/* TODO */
	return 0;
}

static int camera_v4l2_s_parm(struct file *filep, void *fh,
	struct v4l2_streamparm *parm)
{
	int rc = 0;
	struct v4l2_event event;
	struct msm_v4l2_event_data *event_data =
		(struct msm_v4l2_event_data *)&event.u.data[0];
	struct camera_v4l2_private *sp = fh_to_private(fh);

	camera_pack_event(filep, MSM_CAMERA_SET_PARM,
		MSM_CAMERA_PRIV_NEW_STREAM, -1, &event);

	rc = msm_create_stream(event_data->session_id,
		event_data->stream_id, &sp->vb2_q);
	if (rc < 0)
		return rc;

	if (msm_is_daemon_present() != false) {
		rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		if (rc < 0)
			goto error;

		rc = camera_check_event_status(&event);
		if (rc < 0)
			goto error;
	}
	/* use stream_id as stream index */
	parm->parm.capture.extendedmode = sp->stream_id;
	sp->stream_created = true;

	return rc;

error:
	msm_delete_stream(event_data->session_id,
		event_data->stream_id);
	return rc;
}

static int camera_v4l2_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct camera_v4l2_private *sp = fh_to_private(fh);

	mutex_lock(&sp->lock);
	rc = v4l2_event_subscribe(&sp->fh, sub, 5, NULL);
	mutex_unlock(&sp->lock);

	return rc;
}

static int camera_v4l2_unsubscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	int rc = 0;
	struct camera_v4l2_private *sp = fh_to_private(fh);

	mutex_lock(&sp->lock);
	rc = v4l2_event_unsubscribe(&sp->fh, sub);
	mutex_unlock(&sp->lock);

	return rc;
}

static long camera_v4l2_vidioc_private_ioctl(struct file *filep, void *fh,
	bool valid_prio, unsigned int cmd, void *arg)
{
	struct camera_v4l2_private *sp = fh_to_private(fh);
	struct msm_video_device *pvdev = video_drvdata(filep);
	struct msm_camera_private_ioctl_arg *k_ioctl = arg;
	long rc = -EINVAL;

	if (WARN_ON(!k_ioctl || !pvdev))
		return -EIO;

	if (cmd != VIDIOC_MSM_CAMERA_PRIVATE_IOCTL_CMD)
		return -EINVAL;

	switch (k_ioctl->id) {
	case MSM_CAMERA_PRIV_IOCTL_ID_RETURN_BUF: {
		struct msm_camera_return_buf ptr;
		struct msm_camera_return_buf __user *tmp = NULL;

		MSM_CAM_GET_IOCTL_ARG_PTR(&tmp, &k_ioctl->ioctl_ptr,
			sizeof(tmp));
		if (copy_from_user(&ptr, tmp,
			sizeof(struct msm_camera_return_buf))) {
			return -EFAULT;
		}
		rc = msm_vb2_return_buf_by_idx(pvdev->vdev->num, sp->stream_id,
			ptr.index);
		}
		break;
	default:
		pr_debug("unimplemented id %d", k_ioctl->id);
		return -EINVAL;
	}
	return rc;
}

static const struct v4l2_ioctl_ops camera_v4l2_ioctl_ops = {
	.vidioc_querycap = camera_v4l2_querycap,
	.vidioc_s_crop = camera_v4l2_s_crop,
	.vidioc_g_crop = camera_v4l2_g_crop,
	.vidioc_queryctrl = camera_v4l2_queryctrl,
	.vidioc_g_ctrl = camera_v4l2_g_ctrl,
	.vidioc_s_ctrl = camera_v4l2_s_ctrl,
	.vidioc_reqbufs = camera_v4l2_reqbufs,
	.vidioc_querybuf = camera_v4l2_querybuf,
	.vidioc_qbuf = camera_v4l2_qbuf,
	.vidioc_dqbuf = camera_v4l2_dqbuf,
	.vidioc_streamon =  camera_v4l2_streamon,
	.vidioc_streamoff = camera_v4l2_streamoff,
	.vidioc_g_fmt_vid_cap_mplane = camera_v4l2_g_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_cap_mplane = camera_v4l2_s_fmt_vid_cap_mplane,
	.vidioc_try_fmt_vid_cap_mplane = camera_v4l2_try_fmt_vid_cap_mplane,

	/* Stream type-dependent parameter ioctls */
	.vidioc_g_parm = camera_v4l2_g_parm,
	.vidioc_s_parm = camera_v4l2_s_parm,

	/* event subscribe/unsubscribe */
	.vidioc_subscribe_event = camera_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = camera_v4l2_unsubscribe_event,
	.vidioc_default = camera_v4l2_vidioc_private_ioctl,
};

static int camera_v4l2_fh_open(struct file *filep)
{
	struct msm_video_device *pvdev = video_drvdata(filep);
	struct camera_v4l2_private *sp;
	unsigned long stream_id;

	sp = kzalloc(sizeof(*sp), GFP_KERNEL);
	if (!sp)
		return -ENOMEM;

	filep->private_data = &sp->fh;

	/* stream_id = open id */
	stream_id = atomic_read(&pvdev->opened);
	sp->stream_id = find_first_zero_bit(
		(const unsigned long *)&stream_id, MSM_CAMERA_STREAM_CNT_BITS);
	pr_debug("%s: Found stream_id=%d\n", __func__, sp->stream_id);

	mutex_init(&sp->lock);

	v4l2_fh_init(&sp->fh, pvdev->vdev);
	v4l2_fh_add(&sp->fh);

	return 0;
}

static int camera_v4l2_fh_release(struct file *filep)
{
	struct camera_v4l2_private *sp = fh_to_private(filep->private_data);

	if (sp) {
		v4l2_fh_del(&sp->fh);
		v4l2_fh_exit(&sp->fh);
		mutex_destroy(&sp->lock);
		kzfree(sp);
	}

	return 0;
}

static int camera_v4l2_vb2_q_init(struct file *filep)
{
	struct camera_v4l2_private *sp = fh_to_private(filep->private_data);
	struct vb2_queue *q = &sp->vb2_q;

	memset(q, 0, sizeof(struct vb2_queue));

	/* free up this buffer when stream is done */
	q->drv_priv =
		kzalloc(sizeof(struct msm_v4l2_format_data), GFP_KERNEL);
	if (!q->drv_priv) {
		pr_err("%s : memory not available\n", __func__);
		return -ENOMEM;
	}
	q->lock = kzalloc(sizeof(struct mutex), GFP_KERNEL);
	if (!q->lock) {
		kzfree(q->drv_priv);
		return -ENOMEM;
	}
	mutex_init(q->lock);

	q->mem_ops = msm_vb2_get_q_mem_ops();
	q->ops = msm_vb2_get_q_ops();

	/* default queue type */
	q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	q->io_modes = VB2_USERPTR;
	q->buf_struct_size = sizeof(struct msm_vb2_buffer);
	q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
	return vb2_queue_init(q);
}

static void camera_v4l2_vb2_q_release(struct file *filep)
{
	struct camera_v4l2_private *sp = filep->private_data;

	kzfree(sp->vb2_q.drv_priv);
	mutex_lock(&sp->lock);
	vb2_queue_release(&sp->vb2_q);
	mutex_destroy(sp->vb2_q.lock);
	kzfree(sp->vb2_q.lock);
	mutex_unlock(&sp->lock);
}

static int camera_v4l2_open(struct file *filep)
{
	int rc = 0;
	struct v4l2_event event;
	struct msm_video_device *pvdev = video_drvdata(filep);
	unsigned long opn_idx, idx;

	if (WARN_ON(!pvdev))
		return -EIO;

	mutex_lock(&pvdev->video_drvdata_mutex);
	rc = camera_v4l2_fh_open(filep);
	if (rc < 0) {
		pr_err("%s : camera_v4l2_fh_open failed Line %d rc %d\n",
				__func__, __LINE__, rc);
		goto fh_open_fail;
	}

	opn_idx = atomic_read(&pvdev->opened);
	idx = opn_idx;
	/* every stream has a vb2 queue */
	rc = camera_v4l2_vb2_q_init(filep);
	if (rc < 0) {
		pr_err("%s : vb2 queue init fails Line %d rc %d\n",
				__func__, __LINE__, rc);
		goto vb2_q_fail;
	}

	if (!atomic_read(&pvdev->opened)) {
		pm_stay_awake(&pvdev->vdev->dev);

		/* Disable power collapse latency */
		msm_pm_qos_update_request(CAMERA_DISABLE_PC_LATENCY);

		/* create a new session when first opened */
		rc = msm_create_session(pvdev->vdev->num, pvdev->vdev);
		if (rc < 0) {
			pr_err("%s : session creation failed Line %d rc %d\n",
					__func__, __LINE__, rc);
			goto session_fail;
		}

		rc = msm_create_command_ack_q(pvdev->vdev->num,
			find_first_zero_bit((const unsigned long *)&opn_idx,
				MSM_CAMERA_STREAM_CNT_BITS));
		if (rc < 0) {
			pr_err("%s : creation of command_ack queue failed\n",
					__func__);
			pr_err("%s : Line %d rc %d\n", __func__, __LINE__, rc);
			goto command_ack_q_fail;
		}

		if (msm_is_daemon_present() != false) {
			camera_pack_event(filep, MSM_CAMERA_NEW_SESSION,
				0, -1, &event);
			rc = msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
			if (rc < 0) {
				pr_err("%s : NEW_SESSION event failed,rc %d\n",
					__func__, rc);
				goto post_fail;
			}

			rc = camera_check_event_status(&event);
			if (rc < 0)
				goto post_fail;
		}
	} else {
		rc = msm_create_command_ack_q(pvdev->vdev->num,
			find_first_zero_bit((const unsigned long *)&opn_idx,
				MSM_CAMERA_STREAM_CNT_BITS));
		if (rc < 0) {
			pr_err("%s : creation of command_ack queue failed Line %d rc %d\n",
					__func__, __LINE__, rc);
			goto stream_fail;
		}
	}
	idx |= (1UL <<
		find_first_zero_bit((const unsigned long *)&opn_idx,
		MSM_CAMERA_STREAM_CNT_BITS));
	atomic_cmpxchg(&pvdev->opened, opn_idx, idx);
	mutex_unlock(&pvdev->video_drvdata_mutex);

	return rc;

post_fail:
	msm_delete_command_ack_q(pvdev->vdev->num, 0);
command_ack_q_fail:
	msm_destroy_session(pvdev->vdev->num);
session_fail:
	msm_pm_qos_update_request(CAMERA_ENABLE_PC_LATENCY);
	pm_relax(&pvdev->vdev->dev);
stream_fail:
	camera_v4l2_vb2_q_release(filep);
vb2_q_fail:
	camera_v4l2_fh_release(filep);
fh_open_fail:
	mutex_unlock(&pvdev->video_drvdata_mutex);
	return rc;
}

static unsigned int camera_v4l2_poll(struct file *filep,
	struct poll_table_struct *wait)
{
	int rc = 0;
	struct camera_v4l2_private *sp = fh_to_private(filep->private_data);

	if (sp->is_vb2_valid == 1)
		rc = vb2_poll(&sp->vb2_q, filep, wait);

	poll_wait(filep, &sp->fh.wait, wait);
	if (v4l2_event_pending(&sp->fh))
		rc |= POLLPRI;

	return rc;
}

static int camera_v4l2_close(struct file *filep)
{
	struct v4l2_event event;
	struct msm_video_device *pvdev = video_drvdata(filep);
	struct camera_v4l2_private *sp = fh_to_private(filep->private_data);
	unsigned int opn_idx, mask;
	struct msm_session *session;

	if (WARN_ON(!pvdev))
		return -EIO;

	session = msm_session_find(pvdev->vdev->num);
	if (WARN_ON(!session))
		return -EIO;

	mutex_lock(&pvdev->video_drvdata_mutex);
	mutex_lock(&session->close_lock);
	opn_idx = atomic_read(&pvdev->opened);
	mask = (1 << sp->stream_id);
	opn_idx &= ~mask;
	atomic_set(&pvdev->opened, opn_idx);

	if (msm_is_daemon_present() != false && sp->stream_created == true) {
		pr_debug("%s: close stream_id=%d\n", __func__, sp->stream_id);
		camera_pack_event(filep, MSM_CAMERA_SET_PARM,
			MSM_CAMERA_PRIV_DEL_STREAM, -1, &event);
		msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
	}

	if (sp->stream_created == true)
		sp->stream_created = false;

	if (atomic_read(&pvdev->opened) == 0) {
		if (msm_is_daemon_present() != false) {
			camera_pack_event(filep, MSM_CAMERA_DEL_SESSION,
				0, -1, &event);
			msm_post_event(&event, MSM_POST_EVT_TIMEOUT);
		}
		msm_delete_command_ack_q(pvdev->vdev->num, 0);
		msm_delete_stream(pvdev->vdev->num, sp->stream_id);
		mutex_unlock(&session->close_lock);
		/* This should take care of both normal close
		 * and application crashes
		 */
		camera_v4l2_vb2_q_release(filep);
		msm_destroy_session(pvdev->vdev->num);

		/* Enable power collapse latency */
		msm_pm_qos_update_request(CAMERA_ENABLE_PC_LATENCY);
		pm_relax(&pvdev->vdev->dev);
	} else {
		msm_delete_command_ack_q(pvdev->vdev->num,
			sp->stream_id);

		camera_v4l2_vb2_q_release(filep);
		msm_delete_stream(pvdev->vdev->num, sp->stream_id);
		mutex_unlock(&session->close_lock);
	}

	camera_v4l2_fh_release(filep);
	mutex_unlock(&pvdev->video_drvdata_mutex);

	return 0;
}

#ifdef CONFIG_COMPAT
static long camera_handle_internal_compat_ioctl(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	long rc = 0;
	struct msm_camera_private_ioctl_arg k_ioctl;
	void __user *tmp_compat_ioctl_ptr = NULL;

	rc = msm_copy_camera_private_ioctl_args(arg,
		&k_ioctl, &tmp_compat_ioctl_ptr);
	if (rc < 0) {
		pr_err("Subdev cmd %d failed\n", cmd);
		return rc;
	}
	switch (k_ioctl.id) {
	case MSM_CAMERA_PRIV_IOCTL_ID_RETURN_BUF: {
		if (k_ioctl.size != sizeof(struct msm_camera_return_buf)) {
			pr_debug("Invalid size for id %d with size %d",
				k_ioctl.id, k_ioctl.size);
			return -EINVAL;
		}
		k_ioctl.ioctl_ptr = (__force __u64 __user)tmp_compat_ioctl_ptr;
		if (!k_ioctl.ioctl_ptr) {
			pr_debug("Invalid ptr for id %d", k_ioctl.id);
			return -EINVAL;
		}
		rc = camera_v4l2_vidioc_private_ioctl(file, file->private_data,
			0, cmd, (void *)&k_ioctl);
		}
		break;
	default:
		pr_debug("unimplemented id %d", k_ioctl.id);
		return -EINVAL;
	}
	return rc;
}

static long camera_v4l2_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case VIDIOC_MSM_CAMERA_PRIVATE_IOCTL_CMD: {
		ret = camera_handle_internal_compat_ioctl(file, cmd, arg);
		if (ret < 0) {
			pr_debug("Subdev cmd %d fail\n", cmd);
			return ret;
		}
		}
		break;
	default:
		ret = -ENOIOCTLCMD;
		break;

	}
	return ret;
}
#endif
static struct v4l2_file_operations camera_v4l2_fops = {
	.owner   = THIS_MODULE,
	.open	= camera_v4l2_open,
	.poll	= camera_v4l2_poll,
	.release = camera_v4l2_close,
	.unlocked_ioctl   = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = camera_v4l2_compat_ioctl,
#endif
};

int camera_init_v4l2(struct device *dev, unsigned int *session)
{
	struct msm_video_device *pvdev;
	struct v4l2_device *v4l2_dev = NULL;
	int rc = 0;

	pvdev = kzalloc(sizeof(struct msm_video_device),
		GFP_KERNEL);
	if (WARN_ON(!pvdev)) {
		rc = -ENOMEM;
		goto init_end;
	}

	pvdev->vdev = video_device_alloc();
	if (WARN_ON(!pvdev->vdev)) {
		rc = -ENOMEM;
		goto video_fail;
	}

	v4l2_dev = kzalloc(sizeof(struct v4l2_device), GFP_KERNEL);
	if (WARN_ON(!v4l2_dev)) {
		rc = -ENOMEM;
		goto v4l2_fail;
	}

#if defined(CONFIG_MEDIA_CONTROLLER)
	v4l2_dev->mdev = kzalloc(sizeof(struct media_device),
							 GFP_KERNEL);
	if (!v4l2_dev->mdev) {
		rc = -ENOMEM;
		goto mdev_fail;
	}
	media_device_init(v4l2_dev->mdev);
	strlcpy(v4l2_dev->mdev->model, MSM_CAMERA_NAME,
			sizeof(v4l2_dev->mdev->model));

	v4l2_dev->mdev->dev = dev;

	rc = media_device_register(v4l2_dev->mdev);
	if (WARN_ON(rc < 0))
		goto media_fail;

	rc = media_entity_pads_init(&pvdev->vdev->entity, 0, NULL);
	if (WARN_ON(rc < 0))
		goto entity_fail;
	pvdev->vdev->entity.function = QCAMERA_VNODE_GROUP_ID;
#endif

	v4l2_dev->notify = NULL;
	pvdev->vdev->v4l2_dev = v4l2_dev;

	rc = v4l2_device_register(dev, pvdev->vdev->v4l2_dev);
	if (WARN_ON(rc < 0))
		goto register_fail;

	strlcpy(pvdev->vdev->name, "msm-sensor", sizeof(pvdev->vdev->name));
	pvdev->vdev->release  = video_device_release;
	pvdev->vdev->fops     = &camera_v4l2_fops;
	pvdev->vdev->ioctl_ops = &camera_v4l2_ioctl_ops;
	pvdev->vdev->minor     = -1;
	pvdev->vdev->vfl_type  = VFL_TYPE_GRABBER;
	pvdev->vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
	rc = video_register_device(pvdev->vdev,
		VFL_TYPE_GRABBER, -1);
	if (WARN_ON(rc < 0))
		goto video_register_fail;
#if defined(CONFIG_MEDIA_CONTROLLER)
	/* FIXME: How to get rid of this messy? */
	pvdev->vdev->entity.name = video_device_node_name(pvdev->vdev);
#endif

	*session = pvdev->vdev->num;
	atomic_set(&pvdev->opened, 0);
	mutex_init(&pvdev->video_drvdata_mutex);
	video_set_drvdata(pvdev->vdev, pvdev);
	device_init_wakeup(&pvdev->vdev->dev, 1);
	goto init_end;

video_register_fail:
	v4l2_device_unregister(pvdev->vdev->v4l2_dev);
register_fail:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&pvdev->vdev->entity);
entity_fail:
	media_device_unregister(v4l2_dev->mdev);
media_fail:
	kzfree(v4l2_dev->mdev);
mdev_fail:
#endif
	kfree(v4l2_dev);
v4l2_fail:
	video_device_release(pvdev->vdev);
video_fail:
	kfree(pvdev);
init_end:
	return rc;
}
