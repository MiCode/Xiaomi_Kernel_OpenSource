/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/videodev2.h>
#include <linux/msm_ion.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <media/v4l2-fh.h>
#include "msm.h"
#include "msm_vb2.h"
#include "msm_sd.h"
#include "cam_hw_ops.h"
#include <media/ais/msm_ais_buf_mgr.h>


static struct v4l2_device *msm_v4l2_dev;
static struct list_head    ordered_sd_list;

static struct pm_qos_request msm_v4l2_pm_qos_request;

static struct msm_queue_head *msm_session_q;

/* This variable represent daemon status
 * true = daemon present (default state)
 * false = daemon is NOT present
 */
bool is_daemon_status = true;

/* config node envent queue */
static struct v4l2_fh  *msm_eventq;
spinlock_t msm_eventq_lock;

static struct pid *msm_pid;
spinlock_t msm_pid_lock;

/*
 * It takes 20 bytes + NULL character to write the
 * largest decimal value of an uint64_t
 */
#define LOGSYNC_PACKET_SIZE 21

#define msm_dequeue(queue, type, member) ({				\
	unsigned long flags;					\
	struct msm_queue_head *__q = (queue);			\
	type *node = 0;				\
	spin_lock_irqsave(&__q->lock, flags);			\
	if (!list_empty(&__q->list)) {				\
		__q->len--;					\
		node = list_first_entry(&__q->list,		\
				type, member);	\
		if ((node) && (&node->member) && (&node->member.next))	\
			list_del_init(&node->member);			\
	}							\
	spin_unlock_irqrestore(&__q->lock, flags);	\
	node;							\
})

#define msm_delete_sd_entry(queue, type, member, q_node) ({		\
	unsigned long flags;					\
	struct msm_queue_head *__q = (queue);			\
	type *node = 0;				\
	spin_lock_irqsave(&__q->lock, flags);			\
	if (!list_empty(&__q->list)) {				\
		list_for_each_entry(node, &__q->list, member)	\
			if (node->sd == q_node) {		\
				__q->len--;			\
				list_del_init(&node->member);	\
				kzfree(node);			\
				break;				\
			}					\
	}							\
	spin_unlock_irqrestore(&__q->lock, flags);		\
})

#define msm_delete_entry(queue, type, member, q_node) ({		\
	unsigned long flags;					\
	struct msm_queue_head *__q = (queue);			\
	type *node = 0;				\
	spin_lock_irqsave(&__q->lock, flags);			\
	if (!list_empty(&__q->list)) {				\
		list_for_each_entry(node, &__q->list, member)	\
			if (node == q_node) {			\
				__q->len--;			\
				list_del_init(&node->member);	\
				kzfree(node);			\
				break;				\
			}					\
	}							\
	spin_unlock_irqrestore(&__q->lock, flags);		\
})

#define msm_queue_drain(queue, type, member) do {			\
	unsigned long flags;					\
	struct msm_queue_head *__q = (queue);			\
	type *node;				\
	spin_lock_irqsave(&__q->lock, flags);			\
	while (!list_empty(&__q->list)) {			\
		__q->len--;					\
		node = list_first_entry(&__q->list,		\
			type, member);		\
		if (node) {					\
			if (&node->member) \
				list_del_init(&node->member);		\
			kzfree(node);	\
		}	\
	}	\
	spin_unlock_irqrestore(&__q->lock, flags);		\
} while (0)

typedef int (*msm_queue_func)(void *d1, void *d2);
#define msm_queue_traverse_action(queue, type, member, func, data) do {\
	unsigned long flags;					\
	struct msm_queue_head *__q = (queue);			\
	type *node = 0; \
	msm_queue_func __f = (func); \
	spin_lock_irqsave(&__q->lock, flags);			\
	if (!list_empty(&__q->list)) { \
		list_for_each_entry(node, &__q->list, member) \
			if (node && __f)  { \
				__f(node, data); \
			} \
	} \
	spin_unlock_irqrestore(&__q->lock, flags);			\
} while (0)

typedef int (*msm_queue_find_func)(void *d1, void *d2);
#define msm_queue_find(queue, type, member, func, data) ({\
	unsigned long flags;					\
	struct msm_queue_head *__q = (queue);			\
	type *node = NULL; \
	typeof(node) __ret = NULL; \
	msm_queue_find_func __f = (func); \
	spin_lock_irqsave(&__q->lock, flags);			\
	if (!list_empty(&__q->list)) { \
		list_for_each_entry(node, &__q->list, member) \
			if ((__f) && __f(node, data)) { \
				__ret = node; \
				break; \
			} \
	} \
	spin_unlock_irqrestore(&__q->lock, flags); \
	__ret; \
})

static void msm_init_queue(struct msm_queue_head *qhead)
{
	if (WARN_ON(!qhead))
		return;

	INIT_LIST_HEAD(&qhead->list);
	spin_lock_init(&qhead->lock);
	qhead->len = 0;
	qhead->max = 0;
}

static void msm_enqueue(struct msm_queue_head *qhead,
		struct list_head *entry)
{
	unsigned long flags;

	spin_lock_irqsave(&qhead->lock, flags);
	qhead->len++;
	if (qhead->len > qhead->max)
		qhead->max = qhead->len;
	list_add_tail(entry, &qhead->list);
	spin_unlock_irqrestore(&qhead->lock, flags);
}

void msm_cam_copy_v4l2_subdev_fops(struct v4l2_file_operations *d1)
{
	*d1 = v4l2_subdev_fops;
}
EXPORT_SYMBOL(msm_cam_copy_v4l2_subdev_fops);

static const struct v4l2_file_operations *msm_cam_get_v4l2_subdev_fops_ptr(
	void)
{
	return &v4l2_subdev_fops;
}

/* index = session id */
static inline int __msm_queue_find_session(void *d1, void *d2)
{
	struct msm_session *session = d1;

	return (session->session_id == *(unsigned int *)d2) ? 1 : 0;
}

static inline int __msm_queue_find_stream(void *d1, void *d2)
{
	struct msm_stream *stream = d1;

	return (stream->stream_id == *(unsigned int *)d2) ? 1 : 0;
}

static inline int __msm_queue_find_command_ack_q(void *d1, void *d2)
{
	struct msm_command_ack *ack = d1;

	return (ack->stream_id == *(unsigned int *)d2) ? 1 : 0;
}

static void msm_pm_qos_add_request(void)
{
	pr_info("%s: add request", __func__);
	pm_qos_add_request(&msm_v4l2_pm_qos_request, PM_QOS_CPU_DMA_LATENCY,
	PM_QOS_DEFAULT_VALUE);
}

static void msm_pm_qos_remove_request(void)
{
	pr_info("%s: remove request", __func__);
	pm_qos_remove_request(&msm_v4l2_pm_qos_request);
}

void msm_pm_qos_update_request(int val)
{
	pr_info("%s: update request %d", __func__, val);
	pm_qos_update_request(&msm_v4l2_pm_qos_request, val);
}

struct msm_session *msm_session_find(unsigned int session_id)
{
	struct msm_session *session;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (WARN_ON(!session))
		return NULL;
	return session;
}
EXPORT_SYMBOL(msm_session_find);

int msm_create_stream(unsigned int session_id,
	unsigned int stream_id, struct vb2_queue *q)
{
	struct msm_session *session;
	struct msm_stream  *stream;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (!session)
		return -EINVAL;

	stream = kzalloc(sizeof(*stream), GFP_KERNEL);
	if (!stream)
		return -ENOMEM;

	stream->stream_id = stream_id;
	stream->vb2_q = q;
	spin_lock_init(&stream->stream_lock);
	msm_enqueue(&session->stream_q, &stream->list);
	session->stream_q.len++;

	INIT_LIST_HEAD(&stream->queued_list);

	return 0;
}
EXPORT_SYMBOL(msm_create_stream);

void msm_delete_stream(unsigned int session_id, unsigned int stream_id)
{
	struct msm_session *session = NULL;
	struct msm_stream  *stream = NULL;
	unsigned long flags;
	int try_count = 0;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);

	if (!session)
		return;

	while (1) {
		unsigned long wl_flags;

		if (try_count > 5) {
			pr_err("%s : not able to delete stream %d\n",
				__func__, __LINE__);
			break;
		}

		write_lock_irqsave(&session->stream_rwlock, wl_flags);
		try_count++;
		stream = msm_queue_find(&session->stream_q, struct msm_stream,
			list, __msm_queue_find_stream, &stream_id);

		if (!stream) {
			write_unlock_irqrestore(&session->stream_rwlock,
				wl_flags);
			return;
		}

		if (msm_vb2_get_stream_state(stream) != 1) {
			write_unlock_irqrestore(&session->stream_rwlock,
				wl_flags);
			continue;
		}

		spin_lock_irqsave(&(session->stream_q.lock), flags);
		list_del_init(&stream->list);
		session->stream_q.len--;
		kfree(stream);
		stream = NULL;
		spin_unlock_irqrestore(&(session->stream_q.lock), flags);
		write_unlock_irqrestore(&session->stream_rwlock, wl_flags);
		break;
	}

}
EXPORT_SYMBOL(msm_delete_stream);

static void msm_sd_unregister_subdev(struct video_device *vdev)
{
	struct v4l2_subdev *sd = video_get_drvdata(vdev);

	sd->devnode = NULL;
	kzfree(vdev);
}

static inline int __msm_sd_register_subdev(struct v4l2_subdev *sd)
{
	int rc = 0;
	struct video_device *vdev;

	if (!msm_v4l2_dev || !sd || !sd->name[0])
		return -EINVAL;

	rc = v4l2_device_register_subdev(msm_v4l2_dev, sd);
	if (rc < 0)
		return rc;

	/* Register a device node for every subdev marked with the
	 * V4L2_SUBDEV_FL_HAS_DEVNODE flag.
	 */
	if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE))
		return rc;

	vdev = kzalloc(sizeof(*vdev), GFP_KERNEL);
	if (!vdev) {
		rc = -ENOMEM;
		goto clean_up;
	}

	video_set_drvdata(vdev, sd);
	strlcpy(vdev->name, sd->name, sizeof(vdev->name));
	vdev->v4l2_dev = msm_v4l2_dev;
	vdev->fops = msm_cam_get_v4l2_subdev_fops_ptr();
	vdev->release = msm_sd_unregister_subdev;
	rc = __video_register_device(vdev, VFL_TYPE_SUBDEV, -1, 1,
		  sd->owner);
	if (rc < 0) {
		kzfree(vdev);
		goto clean_up;
	}

#if defined(CONFIG_MEDIA_CONTROLLER)
	sd->entity.info.v4l.major = VIDEO_MAJOR;
	sd->entity.info.v4l.minor = vdev->minor;
	sd->entity.name = video_device_node_name(vdev);
#endif
	sd->devnode = vdev;
	return 0;

clean_up:
	if (sd->devnode)
		video_unregister_device(sd->devnode);
	return rc;
}

static void msm_add_sd_in_position(struct msm_sd_subdev *msm_subdev,
	struct list_head *sd_list)
{
	struct msm_sd_subdev *temp_sd;

	list_for_each_entry(temp_sd, sd_list, list) {
		if (temp_sd == msm_subdev) {
			return;
		}
		if (msm_subdev->close_seq < temp_sd->close_seq) {
			list_add_tail(&msm_subdev->list, &temp_sd->list);
			return;
		}
	}
	list_add_tail(&msm_subdev->list, sd_list);
}

int msm_sd_register(struct msm_sd_subdev *msm_subdev)
{
	if (WARN_ON(!msm_subdev))
		return -EINVAL;

	if (WARN_ON(!msm_v4l2_dev) || WARN_ON(!msm_v4l2_dev->dev))
		return -EIO;

	msm_add_sd_in_position(msm_subdev, &ordered_sd_list);
	return __msm_sd_register_subdev(&msm_subdev->sd);
}
EXPORT_SYMBOL(msm_sd_register);

int msm_sd_unregister(struct msm_sd_subdev *msm_subdev)
{
	if (WARN_ON(!msm_subdev))
		return -EINVAL;

	v4l2_device_unregister_subdev(&msm_subdev->sd);
	return 0;
}
EXPORT_SYMBOL(msm_sd_unregister);

static struct v4l2_subdev *msm_sd_find(const char *name)
{
	unsigned long flags;
	struct v4l2_subdev *subdev = NULL;
	struct v4l2_subdev *subdev_out = NULL;

	spin_lock_irqsave(&msm_v4l2_dev->lock, flags);
	if (!list_empty(&msm_v4l2_dev->subdevs)) {
		list_for_each_entry(subdev, &msm_v4l2_dev->subdevs, list)
			if (!strcmp(name, subdev->name)) {
				subdev_out = subdev;
				break;
			}
	}
	spin_unlock_irqrestore(&msm_v4l2_dev->lock, flags);

	return subdev_out;
}

int msm_create_session(unsigned int session_id, struct video_device *vdev)
{
	struct msm_session *session = NULL;

	if (!msm_session_q) {
		pr_err("%s : session queue not available Line %d\n",
				__func__, __LINE__);
		return -ENODEV;
	}

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (session) {
		pr_err("%s: Session exist session_id=%d\n",
				__func__, session_id);
		return -EINVAL;
	}

	session = kzalloc(sizeof(*session), GFP_KERNEL);
	if (!session)
		return -ENOMEM;

	session->session_id = session_id;
	session->event_q.vdev = vdev;
	msm_init_queue(&session->command_ack_q);
	msm_init_queue(&session->stream_q);
	msm_enqueue(msm_session_q, &session->list);
	mutex_init(&session->lock);
	mutex_init(&session->lock_q);
	mutex_init(&session->close_lock);
	rwlock_init(&session->stream_rwlock);
	return 0;
}
EXPORT_SYMBOL(msm_create_session);

int msm_create_command_ack_q(unsigned int session_id, unsigned int stream_id)
{
	struct msm_session *session;
	struct msm_command_ack *cmd_ack;

	if (!msm_session_q) {
		pr_err("%s : Session queue not available Line %d\n",
				__func__, __LINE__);
		return -ENODEV;
	}

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (!session) {
		pr_err("%s : Session not found Line %d\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	mutex_lock(&session->lock);
	cmd_ack = kzalloc(sizeof(*cmd_ack), GFP_KERNEL);
	if (!cmd_ack) {
		mutex_unlock(&session->lock);
		pr_err("%s : memory not available Line %d\n",
				__func__, __LINE__);
		return -ENOMEM;
	}

	msm_init_queue(&cmd_ack->command_q);
	INIT_LIST_HEAD(&cmd_ack->list);
	init_completion(&cmd_ack->wait_complete);
	cmd_ack->stream_id = stream_id;

	msm_enqueue(&session->command_ack_q, &cmd_ack->list);
	session->command_ack_q.len++;
	mutex_unlock(&session->lock);
	return 0;
}
EXPORT_SYMBOL(msm_create_command_ack_q);

void msm_delete_command_ack_q(unsigned int session_id, unsigned int stream_id)
{
	struct msm_session *session;
	struct msm_command_ack *cmd_ack;
	unsigned long flags;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (!session)
		return;
	mutex_lock(&session->lock);

	cmd_ack = msm_queue_find(&session->command_ack_q,
		struct msm_command_ack,	list, __msm_queue_find_command_ack_q,
		&stream_id);
	if (!cmd_ack) {
		mutex_unlock(&session->lock);
		return;
	}

	msm_queue_drain(&cmd_ack->command_q, struct msm_command, list);

	spin_lock_irqsave(&(session->command_ack_q.lock), flags);
	list_del_init(&cmd_ack->list);
	kzfree(cmd_ack);
	session->command_ack_q.len--;
	spin_unlock_irqrestore(&(session->command_ack_q.lock), flags);
	mutex_unlock(&session->lock);
}
EXPORT_SYMBOL(msm_delete_command_ack_q);

static inline int __msm_sd_close_subdevs(struct msm_sd_subdev *msm_sd,
	struct msm_sd_close_ioctl *sd_close)
{
	struct v4l2_subdev *sd;

	sd = &msm_sd->sd;
	pr_debug("%s: Shutting down subdev %s", __func__, sd->name);

	v4l2_subdev_call(sd, core, ioctl, MSM_SD_SHUTDOWN, sd_close);
	v4l2_subdev_call(sd, core, s_power, 0);

	return 0;
}

static inline int __msm_sd_notify_freeze_subdevs(struct msm_sd_subdev *msm_sd,
	int enable)
{
	struct v4l2_subdev *sd;

	sd = &msm_sd->sd;

	if (enable)
		v4l2_subdev_call(sd, core, ioctl, MSM_SD_NOTIFY_FREEZE, NULL);
	else
		v4l2_subdev_call(sd, core, ioctl, MSM_SD_UNNOTIFY_FREEZE, NULL);

	return 0;
}

static inline int __msm_destroy_session_streams(void *d1, void *d2)
{
	struct msm_stream *stream = d1;
	unsigned long flags;

	pr_err("%s: Error: Destroyed list is not empty\n", __func__);
	spin_lock_irqsave(&stream->stream_lock, flags);
	INIT_LIST_HEAD(&stream->queued_list);
	spin_unlock_irqrestore(&stream->stream_lock, flags);
	return 0;
}

static void msm_destroy_session_streams(struct msm_session *session)
{

	if (!session)
		return;

	msm_queue_traverse_action(&session->stream_q, struct msm_stream, list,
		__msm_destroy_session_streams, NULL);

	msm_queue_drain(&session->stream_q, struct msm_stream, list);
}

static inline int __msm_remove_session_cmd_ack_q(void *d1, void *d2)
{
	struct msm_command_ack *cmd_ack = d1;

	if (!(&cmd_ack->command_q))
		return 0;

	msm_queue_drain(&cmd_ack->command_q, struct msm_command, list);

	return 0;
}

static void msm_remove_session_cmd_ack_q(struct msm_session *session)
{
	if ((!session) || !(&session->command_ack_q))
		return;

	mutex_lock(&session->lock);
	/* to ensure error handling purpose, it needs to detach all subdevs
	 * which are being connected to streams */
	msm_queue_traverse_action(&session->command_ack_q,
		struct msm_command_ack,	list,
		__msm_remove_session_cmd_ack_q, NULL);

	msm_queue_drain(&session->command_ack_q, struct msm_command_ack, list);

	mutex_unlock(&session->lock);
}

int msm_destroy_session(unsigned int session_id)
{
	struct msm_session *session;
	struct v4l2_subdev *buf_mgr_subdev;
	struct msm_sd_close_ioctl session_info;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (!session)
		return -EINVAL;

	msm_destroy_session_streams(session);
	msm_remove_session_cmd_ack_q(session);
	mutex_destroy(&session->lock);
	mutex_destroy(&session->lock_q);
	mutex_destroy(&session->close_lock);
	msm_delete_entry(msm_session_q, struct msm_session,
		list, session);
	buf_mgr_subdev = msm_sd_find("msm_buf_mngr");
	if (buf_mgr_subdev) {
		session_info.session = session_id;
		session_info.stream = 0;
		v4l2_subdev_call(buf_mgr_subdev, core, ioctl,
			MSM_SD_SHUTDOWN, &session_info);
	} else {
		pr_err("%s: Buff manger device node is NULL\n", __func__);
	}

	return 0;
}
EXPORT_SYMBOL(msm_destroy_session);

static int __msm_close_destry_session_notify_apps(void *d1, void *d2)
{
	struct v4l2_event event;
	struct msm_v4l2_event_data *event_data =
		(struct msm_v4l2_event_data *)&event.u.data[0];
	struct msm_session *session = d1;

	event.type = MSM_CAMERA_V4L2_EVENT_TYPE;
	event.id   = MSM_CAMERA_MSM_NOTIFY;
	event_data->command = MSM_CAMERA_PRIV_SHUTDOWN;

	v4l2_event_queue(session->event_q.vdev, &event);

	return 0;
}

static int __msm_wakeup_all_cmdack_session_stream(void *d1, void *d2)
{
	struct msm_stream *stream = d1;
	struct msm_session *session = d2;
	struct msm_command_ack *cmd_ack = NULL;
	unsigned long spin_flags = 0;

	cmd_ack = msm_queue_find(&session->command_ack_q,
		struct msm_command_ack, list,
		__msm_queue_find_command_ack_q,
		&stream->stream_id);
	if (cmd_ack) {
		spin_lock_irqsave(&(session->command_ack_q.lock),
			spin_flags);
		complete(&cmd_ack->wait_complete);
		spin_unlock_irqrestore(&(session->command_ack_q.lock),
			spin_flags);
	}
	return 0;
}

static int __msm_close_wakeup_all_cmdack_session(void *d1, void *d2)
{
	struct msm_stream  *stream = NULL;
	struct msm_session *session = d1;

	stream = msm_queue_find(&session->stream_q, struct msm_stream,
		list, __msm_wakeup_all_cmdack_session_stream, d1);
	return 0;
}

static long msm_private_ioctl(struct file *file, void *fh,
	bool valid_prio, unsigned int cmd, void *arg)
{
	int rc = 0;
	struct msm_v4l2_event_data *event_data = arg;
	struct v4l2_event event;
	struct msm_session *session;
	unsigned int session_id;
	unsigned int stream_id;
	unsigned long spin_flags = 0;
	struct msm_sd_subdev *msm_sd;

	if (cmd == MSM_CAM_V4L2_IOCTL_DAEMON_DISABLED) {
		is_daemon_status = false;
		return 0;
	}

	if (!event_data)
		return -EINVAL;

	memset(&event, 0, sizeof(struct v4l2_event));
	session_id = event_data->session_id;
	stream_id = event_data->stream_id;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);

	if (!session)
		return -EINVAL;

	switch (cmd) {
	case MSM_CAM_V4L2_IOCTL_NOTIFY: {
		if (WARN_ON(!session->event_q.vdev)) {
			rc = -EFAULT;
			break;
		}
		event.type = event_data->v4l2_event_type;
		event.id = event_data->v4l2_event_id;
		memcpy(&event.u.data, event_data,
			sizeof(struct msm_v4l2_event_data));
		v4l2_event_queue(session->event_q.vdev,
			&event);
	}
		break;

	case MSM_CAM_V4L2_IOCTL_CMD_ACK: {
		struct msm_command_ack *cmd_ack;
		struct msm_command *ret_cmd;

		ret_cmd = kzalloc(sizeof(*ret_cmd), GFP_KERNEL);
		if (!ret_cmd) {
			rc = -ENOMEM;
			break;
		}

		cmd_ack = msm_queue_find(&session->command_ack_q,
			struct msm_command_ack, list,
			__msm_queue_find_command_ack_q,
			&stream_id);
		if (WARN_ON(!cmd_ack)) {
			kzfree(ret_cmd);
			rc = -EFAULT;
			break;
		}

		spin_lock_irqsave(&(session->command_ack_q.lock),
		   spin_flags);
		event.type = event_data->v4l2_event_type;
		event.id = event_data->v4l2_event_id;
		memcpy(&event.u.data, event_data,
			sizeof(struct msm_v4l2_event_data));
		memcpy(&ret_cmd->event, &event, sizeof(struct v4l2_event));
		msm_enqueue(&cmd_ack->command_q, &ret_cmd->list);
		complete(&cmd_ack->wait_complete);
		spin_unlock_irqrestore(&(session->command_ack_q.lock),
		   spin_flags);
	}
		break;

	case MSM_CAM_V4L2_IOCTL_NOTIFY_DEBUG: {
		if (event_data->status) {
			pr_err("%s:Notifying subdevs about potential sof freeze\n",
				__func__);
		} else {
			pr_err("%s:Notifying subdevs about sof recover\n",
				__func__);
		}

		if (!list_empty(&msm_v4l2_dev->subdevs)) {
			list_for_each_entry(msm_sd, &ordered_sd_list, list)
				__msm_sd_notify_freeze_subdevs(msm_sd,
					event_data->status);
		}
	}
		break;

	case MSM_CAM_V4L2_IOCTL_NOTIFY_ERROR:
		/* send v4l2_event to HAL next*/
		msm_queue_traverse_action(msm_session_q,
			struct msm_session, list,
			__msm_close_destry_session_notify_apps, NULL);
		break;

	default:
		rc = -ENOTTY;
		break;
	}

	return rc;
}

static int msm_unsubscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static int msm_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, 5, NULL);
}

static const struct v4l2_ioctl_ops g_msm_ioctl_ops = {
	.vidioc_subscribe_event = msm_subscribe_event,
	.vidioc_unsubscribe_event = msm_unsubscribe_event,
	.vidioc_default = msm_private_ioctl,
};

static unsigned int msm_poll(struct file *f,
	struct poll_table_struct *pll_table)
{
	int rc = 0;
	struct v4l2_fh *eventq = f->private_data;

	if (WARN_ON(!eventq))
		return 0;

	poll_wait(f, &eventq->wait, pll_table);

	if (v4l2_event_pending(eventq))
		rc = POLLIN | POLLRDNORM;

	return rc;
}

static void msm_print_event_error(struct v4l2_event *event)
{
	struct msm_v4l2_event_data *event_data =
		(struct msm_v4l2_event_data *)&event->u.data[0];

	pr_err("Evt_type=%x Evt_id=%d Evt_cmd=%x\n", event->type,
		event->id, event_data->command);
	pr_err("Evt_session_id=%d Evt_stream_id=%d Evt_arg=%d\n",
		event_data->session_id, event_data->stream_id,
		event_data->arg_value);
}

/* something seriously wrong if msm_close is triggered
 *   !!! user space imaging server is shutdown !!!
 */
int msm_post_event(struct v4l2_event *event, int timeout)
{
	int rc = 0;
	struct video_device *vdev;
	struct msm_session *session;
	struct msm_v4l2_event_data *event_data =
		(struct msm_v4l2_event_data *)&event->u.data[0];
	struct msm_command_ack *cmd_ack;
	struct msm_command *cmd;
	int session_id, stream_id;
	unsigned long flags = 0;

	session_id = event_data->session_id;
	stream_id = event_data->stream_id;

	spin_lock_irqsave(&msm_eventq_lock, flags);
	if (!msm_eventq) {
		spin_unlock_irqrestore(&msm_eventq_lock, flags);
		pr_err("%s : msm event queue not available Line %d\n",
				__func__, __LINE__);
		return -ENODEV;
	}
	spin_unlock_irqrestore(&msm_eventq_lock, flags);

	vdev = msm_eventq->vdev;

	/* send to imaging server and wait for ACK */
	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (WARN_ON(!session)) {
		pr_err("%s : session not found Line %d\n",
				__func__, __LINE__);
		return -EIO;
	}
	mutex_lock(&session->lock);
	cmd_ack = msm_queue_find(&session->command_ack_q,
		struct msm_command_ack, list,
		__msm_queue_find_command_ack_q, &stream_id);
	if (WARN_ON(!cmd_ack)) {
		mutex_unlock(&session->lock);
		pr_err("%s : cmd_ack not found Line %d\n",
				__func__, __LINE__);
		return -EIO;
	}

	/* re-init wait_complete */
	reinit_completion(&cmd_ack->wait_complete);

	v4l2_event_queue(vdev, event);

	if (timeout < 0) {
		mutex_unlock(&session->lock);
		pr_debug("%s : timeout cannot be negative Line %d\n",
				__func__, __LINE__);
		return rc;
	}

	/* should wait on session based condition */
	rc = wait_for_completion_timeout(&cmd_ack->wait_complete,
			msecs_to_jiffies(timeout));


	if (list_empty_careful(&cmd_ack->command_q.list)) {
		if (!rc) {
			pr_err("%s: Timed out\n", __func__);
			msm_print_event_error(event);
			mutex_unlock(&session->lock);
			return -ETIMEDOUT;
		}
		pr_err("%s: Error: No timeout but list empty!", __func__);
		msm_print_event_error(event);
		mutex_unlock(&session->lock);
		return -EINVAL;
	}

	cmd = msm_dequeue(&cmd_ack->command_q,
		struct msm_command, list);
	if (!cmd) {
		mutex_unlock(&session->lock);
		pr_err("%s : cmd dequeue failed Line %d\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	event_data = (struct msm_v4l2_event_data *)cmd->event.u.data;

	/* compare cmd_ret and event */
	if (WARN_ON(event->type != cmd->event.type) ||
			WARN_ON(event->id != cmd->event.id)) {
		pr_err("%s : Either event type or id didnot match Line %d\n",
				__func__, __LINE__);
		pr_err("%s : event->type %d event->id %d\n", __func__,
				event->type, event->id);
		pr_err("%s : cmd->event.type %d cmd->event.id %d\n", __func__,
				cmd->event.type, cmd->event.id);
		rc = -EINVAL;
	}

	*event = cmd->event;

	kzfree(cmd);
	mutex_unlock(&session->lock);
	return rc;
}
EXPORT_SYMBOL(msm_post_event);

static int msm_close(struct file *filep)
{
	int rc = 0;
	unsigned long flags;
	struct msm_video_device *pvdev = video_drvdata(filep);
	struct msm_sd_close_ioctl sd_close;
	struct msm_sd_subdev *msm_sd;

	/* stop all hardware blocks immediately */
	if (!list_empty(&msm_v4l2_dev->subdevs))
		list_for_each_entry(msm_sd, &ordered_sd_list, list)
			__msm_sd_close_subdevs(msm_sd, &sd_close);

	/* remove msm_v4l2_pm_qos_request */
	msm_pm_qos_remove_request();

	/* send v4l2_event to HAL next*/
	msm_queue_traverse_action(msm_session_q, struct msm_session, list,
		__msm_close_destry_session_notify_apps, NULL);

	msm_queue_traverse_action(msm_session_q, struct msm_session, list,
		__msm_close_wakeup_all_cmdack_session, NULL);

	spin_lock_irqsave(&msm_eventq_lock, flags);
	msm_eventq = NULL;
	spin_unlock_irqrestore(&msm_eventq_lock, flags);
	v4l2_fh_release(filep);

	spin_lock_irqsave(&msm_pid_lock, flags);
	put_pid(msm_pid);
	msm_pid = NULL;
	spin_unlock_irqrestore(&msm_pid_lock, flags);

	atomic_set(&pvdev->opened, 0);

	return rc;
}

static inline void msm_list_switch(struct list_head *l1,
	struct list_head *l2)
{
	l1->next = l2->next;
	l2->prev = l1->prev;
	l1->prev->next = l2;
	l2->next->prev = l1;
	l1->prev = l2;
	l2->next = l1;
}

static int msm_open(struct file *filep)
{
	int rc;
	unsigned long flags;
	struct msm_video_device *pvdev = video_drvdata(filep);

	if (WARN_ON(!pvdev))
		return -EIO;

	/* !!! only ONE open is allowed !!! */
	if (atomic_read(&pvdev->opened))
		return -EBUSY;

	atomic_set(&pvdev->opened, 1);

	spin_lock_irqsave(&msm_pid_lock, flags);
	msm_pid = get_pid(task_pid(current));
	spin_unlock_irqrestore(&msm_pid_lock, flags);

	/* create event queue */
	rc = v4l2_fh_open(filep);
	if (rc  < 0)
		return rc;

	spin_lock_irqsave(&msm_eventq_lock, flags);
	msm_eventq = filep->private_data;
	spin_unlock_irqrestore(&msm_eventq_lock, flags);

	/* register msm_v4l2_pm_qos_request */
	msm_pm_qos_add_request();

	return rc;
}

static struct v4l2_file_operations msm_fops = {
	.owner  = THIS_MODULE,
	.open   = msm_open,
	.poll   = msm_poll,
	.release = msm_close,
	.ioctl   = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

struct msm_session *msm_get_session(unsigned int session_id)
{
	struct msm_session *session;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (!session)
		return ERR_PTR(-EINVAL);

	return session;
}
EXPORT_SYMBOL(msm_get_session);


struct msm_stream *msm_get_stream(struct msm_session *session,
	unsigned int stream_id)
{
	struct msm_stream *stream;

	stream = msm_queue_find(&session->stream_q, struct msm_stream,
		list, __msm_queue_find_stream, &stream_id);

	if (!stream)
		return ERR_PTR(-EINVAL);

	return stream;
}
EXPORT_SYMBOL(msm_get_stream);

struct vb2_queue *msm_get_stream_vb2q(unsigned int session_id,
	unsigned int stream_id)
{
	struct msm_session *session;
	struct msm_stream *stream;

	session = msm_queue_find(msm_session_q, struct msm_session,
		list, __msm_queue_find_session, &session_id);
	if (!session)
		return NULL;

	stream = msm_queue_find(&session->stream_q, struct msm_stream,
		list, __msm_queue_find_stream, &stream_id);
	if (!stream)
		return NULL;

	return stream->vb2_q;
}
EXPORT_SYMBOL(msm_get_stream_vb2q);

struct msm_stream *msm_get_stream_from_vb2q(struct vb2_queue *q)
{
	struct msm_session *session;
	struct msm_stream *stream;
	unsigned long flags1;
	unsigned long flags2;

	spin_lock_irqsave(&msm_session_q->lock, flags1);
	list_for_each_entry(session, &(msm_session_q->list), list) {
		spin_lock_irqsave(&(session->stream_q.lock), flags2);
		list_for_each_entry(
			stream, &(session->stream_q.list), list) {
			if (stream->vb2_q == q) {
				spin_unlock_irqrestore
					(&(session->stream_q.lock), flags2);
				spin_unlock_irqrestore
					(&msm_session_q->lock, flags1);
				return stream;
			}
		}
		spin_unlock_irqrestore(&(session->stream_q.lock), flags2);
	}
	spin_unlock_irqrestore(&msm_session_q->lock, flags1);
	return NULL;
}
EXPORT_SYMBOL(msm_get_stream_from_vb2q);

struct msm_session *msm_get_session_from_vb2q(struct vb2_queue *q)
{
	struct msm_session *session;
	struct msm_stream *stream;
	unsigned long flags1;
	unsigned long flags2;

	spin_lock_irqsave(&msm_session_q->lock, flags1);
	list_for_each_entry(session, &(msm_session_q->list), list) {
		spin_lock_irqsave(&(session->stream_q.lock), flags2);
		list_for_each_entry(
			stream, &(session->stream_q.list), list) {
			if (stream->vb2_q == q) {
				spin_unlock_irqrestore
					(&(session->stream_q.lock), flags2);
				spin_unlock_irqrestore
					(&msm_session_q->lock, flags1);
				return session;
			}
		}
		spin_unlock_irqrestore(&(session->stream_q.lock), flags2);
	}
	spin_unlock_irqrestore(&msm_session_q->lock, flags1);
	return NULL;
}
EXPORT_SYMBOL(msm_get_session_from_vb2q);


#ifdef CONFIG_COMPAT
long msm_copy_camera_private_ioctl_args(unsigned long arg,
	struct msm_camera_private_ioctl_arg *k_ioctl,
	void __user **tmp_compat_ioctl_ptr)
{
	struct msm_camera_private_ioctl_arg up_ioctl;

	if (WARN_ON(!arg || !k_ioctl || !tmp_compat_ioctl_ptr))
		return -EIO;

	if (copy_from_user(&up_ioctl,
		(struct msm_camera_private_ioctl_arg *)arg,
		sizeof(struct msm_camera_private_ioctl_arg)))
		return -EFAULT;

	k_ioctl->id = up_ioctl.id;
	k_ioctl->size = up_ioctl.size;
	k_ioctl->result = up_ioctl.result;
	k_ioctl->reserved = up_ioctl.reserved;
	*tmp_compat_ioctl_ptr = compat_ptr(up_ioctl.ioctl_ptr);

	return 0;
}
EXPORT_SYMBOL(msm_copy_camera_private_ioctl_args);
#endif

static void msm_sd_notify(struct v4l2_subdev *sd,
	unsigned int notification, void *arg)
{
	int rc = 0;
	struct v4l2_subdev *subdev = NULL;

	if (WARN_ON(!sd) || WARN_ON(!arg))
		return;

	/* Check if subdev exists before processing*/
	if (!msm_sd_find(sd->name))
		return;

	switch (notification) {
	case MSM_SD_NOTIFY_GET_SD: {
		struct msm_sd_req_sd *get_sd = arg;

		get_sd->subdev = msm_sd_find(get_sd->name);
		/* TODO: might need to add ref count on ret_sd */
	}
		break;

	case MSM_SD_NOTIFY_PUT_SD: {
		struct msm_sd_req_sd *put_sd = arg;

		subdev = msm_sd_find(put_sd->name);
	}
		break;

	case MSM_SD_NOTIFY_REQ_CB: {
		struct msm_sd_req_vb2_q *req_sd = arg;

		rc = msm_vb2_request_cb(req_sd);
		if (rc < 0)
			return;
	}
		break;

	default:
		break;
	}
}

static ssize_t write_logsync(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char lbuf[LOGSYNC_PACKET_SIZE] = {0};
	uint64_t seq_num = 0;

	if (copy_from_user(lbuf, buf, sizeof(lbuf)))
		return -EFAULT;

	if (kstrtoull(lbuf, 0, &seq_num) < 0)
		pr_err("LOGSYNC (Kernel): Bad or malformed sequence number\n");
	else
		pr_debug("LOGSYNC (Kernel): seq_num = %llu\n", seq_num);

	return count;
}


static const struct file_operations logsync_fops = {
		.write = write_logsync,
};

static int msm_probe(struct platform_device *pdev)
{
	struct msm_video_device *pvdev = NULL;
	static struct dentry *cam_debugfs_root;
	int rc = 0;

	msm_v4l2_dev = kzalloc(sizeof(*msm_v4l2_dev),
		GFP_KERNEL);
	if (!msm_v4l2_dev) {
		rc = -ENOMEM;
		goto probe_end;
	}

	pvdev = kzalloc(sizeof(struct msm_video_device),
		GFP_KERNEL);
	if (!pvdev) {
		rc = -ENOMEM;
		goto pvdev_fail;
	}

	pvdev->vdev = video_device_alloc();
	if (!pvdev->vdev) {
		rc = -ENOMEM;
		goto video_fail;
	}

#if defined(CONFIG_MEDIA_CONTROLLER)
	msm_v4l2_dev->mdev = kzalloc(sizeof(struct media_device),
		GFP_KERNEL);
	if (!msm_v4l2_dev->mdev) {
		rc = -ENOMEM;
		goto mdev_fail;
	}
	strlcpy(msm_v4l2_dev->mdev->model, MSM_CONFIGURATION_NAME,
			sizeof(msm_v4l2_dev->mdev->model));
	msm_v4l2_dev->mdev->dev = &(pdev->dev);

	rc = media_device_register(msm_v4l2_dev->mdev);
	if (WARN_ON(rc < 0))
		goto media_fail;

	if (WARN_ON((rc == media_entity_init(&pvdev->vdev->entity,
			0, NULL, 0)) < 0))
		goto entity_fail;

	pvdev->vdev->entity.type = MEDIA_ENT_T_DEVNODE_V4L;
	pvdev->vdev->entity.group_id = QCAMERA_VNODE_GROUP_ID;
#endif

	msm_v4l2_dev->notify = msm_sd_notify;

	pvdev->vdev->v4l2_dev = msm_v4l2_dev;

	rc = v4l2_device_register(&(pdev->dev), pvdev->vdev->v4l2_dev);
	if (WARN_ON(rc < 0))
		goto register_fail;

	strlcpy(pvdev->vdev->name, "msm-config", sizeof(pvdev->vdev->name));
	pvdev->vdev->release  = video_device_release;
	pvdev->vdev->fops     = &msm_fops;
	pvdev->vdev->ioctl_ops = &g_msm_ioctl_ops;
	pvdev->vdev->minor     = -1;
	pvdev->vdev->vfl_type  = VFL_TYPE_GRABBER;
	rc = video_register_device(pvdev->vdev,
		VFL_TYPE_GRABBER, -1);
	if (WARN_ON(rc < 0))
		goto v4l2_fail;

#if defined(CONFIG_MEDIA_CONTROLLER)
	/* FIXME: How to get rid of this messy? */
	pvdev->vdev->entity.name = video_device_node_name(pvdev->vdev);
#endif

	atomic_set(&pvdev->opened, 0);
	video_set_drvdata(pvdev->vdev, pvdev);

	msm_session_q = kzalloc(sizeof(*msm_session_q), GFP_KERNEL);
	if (!msm_session_q)
		goto v4l2_fail;

	msm_init_queue(msm_session_q);
	spin_lock_init(&msm_eventq_lock);
	spin_lock_init(&msm_pid_lock);
	INIT_LIST_HEAD(&ordered_sd_list);

	cam_debugfs_root = debugfs_create_dir(MSM_CAM_LOGSYNC_FILE_BASEDIR,
						NULL);
	if (!cam_debugfs_root) {
		pr_warn("NON-FATAL: failed to create logsync base directory\n");
	} else {
		if (!debugfs_create_file(MSM_CAM_LOGSYNC_FILE_NAME,
					 0666,
					 cam_debugfs_root,
					 NULL,
					 &logsync_fops))
			pr_warn("NON-FATAL: failed to create logsync debugfs file\n");
	}

	rc = cam_ahb_clk_init(pdev);
	if (rc < 0) {
		pr_err("%s: failed to register ahb clocks\n", __func__);
		goto v4l2_fail;
	}

	goto probe_end;

v4l2_fail:
	v4l2_device_unregister(pvdev->vdev->v4l2_dev);
register_fail:
#if defined(CONFIG_MEDIA_CONTROLLER)
	media_entity_cleanup(&pvdev->vdev->entity);
entity_fail:
	media_device_unregister(msm_v4l2_dev->mdev);
media_fail:
	kzfree(msm_v4l2_dev->mdev);
mdev_fail:
#endif
	video_device_release(pvdev->vdev);
video_fail:
	kzfree(pvdev);
pvdev_fail:
	kzfree(msm_v4l2_dev);
probe_end:
	return rc;
}

static const struct of_device_id msm_dt_match[] = {
	{.compatible = "qcom,msm-cam"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_dt_match);

static struct platform_driver msm_driver = {
	.probe = msm_probe,
	.driver = {
		.name = "msm",
		.owner = THIS_MODULE,
		.of_match_table = msm_dt_match,
	},
};

static int __init msm_init(void)
{
	return platform_driver_register(&msm_driver);
}

static void __exit msm_exit(void)
{
	platform_driver_unregister(&msm_driver);
}


module_init(msm_init);
module_exit(msm_exit);
MODULE_DESCRIPTION("MSM V4L2 Camera");
MODULE_LICENSE("GPL v2");
