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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <mach/board.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <media/msm_isp.h>
#include <linux/proc_fs.h>
#include <linux/debugfs.h>

#include "msm_cpp.h"
#include "msm.h"

#define CONFIG_MSM_CPP_DBG 0

#if CONFIG_MSM_CPP_DBG
#define CPP_DBG(fmt, args...) pr_info(fmt, ##args)
#else
#define CPP_DBG(fmt, args...) pr_debug(fmt, ##args)
#endif

static int cpp_open_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	uint32_t i;
	struct cpp_device *cpp_dev = v4l2_get_subdevdata(sd);
	CPP_DBG("%s\n", __func__);

	mutex_lock(&cpp_dev->mutex);
	if (cpp_dev->cpp_open_cnt == MAX_ACTIVE_CPP_INSTANCE) {
		pr_err("No free CPP instance\n");
		mutex_unlock(&cpp_dev->mutex);
		return -ENODEV;
	}

	for (i = 0; i < MAX_ACTIVE_CPP_INSTANCE; i++) {
		if (cpp_dev->cpp_subscribe_list[i].active == 0) {
			cpp_dev->cpp_subscribe_list[i].active = 1;
			cpp_dev->cpp_subscribe_list[i].vfh = &fh->vfh;
			break;
		}
	}
	if (i == MAX_ACTIVE_CPP_INSTANCE) {
		pr_err("No free instance\n");
		mutex_unlock(&cpp_dev->mutex);
		return -ENODEV;
	}

	CPP_DBG("open %d %p\n", i, &fh->vfh);
	cpp_dev->cpp_open_cnt++;
	mutex_unlock(&cpp_dev->mutex);
	return 0;
}

static int cpp_close_node(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	uint32_t i;
	struct cpp_device *cpp_dev = v4l2_get_subdevdata(sd);
	mutex_lock(&cpp_dev->mutex);
	for (i = 0; i < MAX_ACTIVE_CPP_INSTANCE; i++) {
		if (cpp_dev->cpp_subscribe_list[i].vfh == &fh->vfh) {
			cpp_dev->cpp_subscribe_list[i].active = 0;
			cpp_dev->cpp_subscribe_list[i].vfh = NULL;
			break;
		}
	}
	if (i == MAX_ACTIVE_CPP_INSTANCE) {
		pr_err("Invalid close\n");
		mutex_unlock(&cpp_dev->mutex);
		return -ENODEV;
	}

	CPP_DBG("close %d %p\n", i, &fh->vfh);
	cpp_dev->cpp_open_cnt--;
	mutex_unlock(&cpp_dev->mutex);
	return 0;
}

static const struct v4l2_subdev_internal_ops msm_cpp_internal_ops = {
	.open = cpp_open_node,
	.close = cpp_close_node,
};

static int msm_cpp_notify_frame_done(struct cpp_device *cpp_dev)
{
	struct v4l2_event v4l2_evt;
	struct msm_queue_cmd *frame_qcmd;
	struct msm_queue_cmd *event_qcmd;
	struct msm_cpp_frame_info_t *processed_frame;
	struct msm_device_queue *queue = &cpp_dev->processing_q;

	if (queue->len > 0) {
		frame_qcmd = msm_dequeue(queue, list_frame);
		processed_frame = frame_qcmd->command;

		event_qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_KERNEL);
		if (!event_qcmd) {
			pr_err("%s Insufficient memory. return", __func__);
			return -ENOMEM;
		}
		atomic_set(&event_qcmd->on_heap, 1);
		event_qcmd->command = processed_frame;
		CPP_DBG("fid %d\n", processed_frame->frame_id);
		msm_enqueue(&cpp_dev->eventData_q, &event_qcmd->list_eventdata);

		v4l2_evt.id = processed_frame->inst_id;
		v4l2_evt.type = V4L2_EVENT_CPP_FRAME_DONE;
		v4l2_event_queue(cpp_dev->subdev.devnode, &v4l2_evt);
	}
	return 0;
}

static int msm_cpp_send_frame_to_hardware(struct cpp_device *cpp_dev)
{
	struct msm_queue_cmd *frame_qcmd;
	struct msm_cpp_frame_info_t *process_frame;
	struct msm_device_queue *queue;

	if (cpp_dev->processing_q.len < MAX_CPP_PROCESSING_FRAME) {
		while (cpp_dev->processing_q.len < MAX_CPP_PROCESSING_FRAME) {
			if (cpp_dev->realtime_q.len != 0) {
				queue = &cpp_dev->realtime_q;
			} else if (cpp_dev->offline_q.len != 0) {
				queue = &cpp_dev->offline_q;
			} else {
				pr_debug("%s: All frames queued\n", __func__);
				break;
			}
			frame_qcmd = msm_dequeue(queue, list_frame);
			/*TBD Code to actually sending to harware*/
			process_frame = frame_qcmd->command;

			msm_enqueue(&cpp_dev->processing_q,
						&frame_qcmd->list_frame);
		}
	}
	return 0;
}

long msm_cpp_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	struct cpp_device *cpp_dev = v4l2_get_subdevdata(sd);
	struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
	int rc = 0;

	CPP_DBG("%s: %d\n", __func__, __LINE__);
	mutex_lock(&cpp_dev->mutex);
	CPP_DBG("%s cmd: %d\n", __func__, cmd);
	switch (cmd) {
	case VIDIOC_MSM_CPP_CFG: {
		struct msm_queue_cmd *frame_qcmd;
		struct msm_cpp_frame_info_t *new_frame =
			kzalloc(sizeof(struct msm_cpp_frame_info_t),
					GFP_KERNEL);
		if (!new_frame) {
			pr_err("%s Insufficient memory. return", __func__);
			mutex_unlock(&cpp_dev->mutex);
			return -ENOMEM;
		}

		COPY_FROM_USER(rc, new_frame,
			       (void __user *)ioctl_ptr->ioctl_ptr,
			       sizeof(struct msm_cpp_frame_info_t));
		if (rc) {
			ERR_COPY_FROM_USER();
			kfree(new_frame);
			mutex_unlock(&cpp_dev->mutex);
			return -EINVAL;
		}

		frame_qcmd = kzalloc(sizeof(struct msm_queue_cmd), GFP_KERNEL);
		if (!frame_qcmd) {
			pr_err("%s Insufficient memory. return", __func__);
			kfree(new_frame);
			mutex_unlock(&cpp_dev->mutex);
			return -ENOMEM;
		}

		atomic_set(&frame_qcmd->on_heap, 1);
		frame_qcmd->command = new_frame;
		if (new_frame->frame_type == MSM_CPP_REALTIME_FRAME) {
			msm_enqueue(&cpp_dev->realtime_q,
						&frame_qcmd->list_frame);
		} else if (new_frame->frame_type == MSM_CPP_OFFLINE_FRAME) {
			msm_enqueue(&cpp_dev->offline_q,
						&frame_qcmd->list_frame);
		} else {
			pr_err("%s: Invalid frame type\n", __func__);
			kfree(new_frame);
			kfree(frame_qcmd);
			mutex_unlock(&cpp_dev->mutex);
			return -EINVAL;
		}
		break;
	}
	case VIDIOC_MSM_CPP_GET_EVENTPAYLOAD: {
		struct msm_device_queue *queue = &cpp_dev->eventData_q;
		struct msm_queue_cmd *event_qcmd;
		struct msm_cpp_frame_info_t *process_frame;
		event_qcmd = msm_dequeue(queue, list_eventdata);
		process_frame = event_qcmd->command;
		CPP_DBG("fid %d\n", process_frame->frame_id);
		if (copy_to_user((void __user *)ioctl_ptr->ioctl_ptr,
				process_frame,
				sizeof(struct msm_cpp_frame_info_t))) {
					mutex_unlock(&cpp_dev->mutex);
					return -EINVAL;
		}
		kfree(process_frame);
		kfree(event_qcmd);
		break;
	}
	}
	mutex_unlock(&cpp_dev->mutex);
	CPP_DBG("%s: %d\n", __func__, __LINE__);
	return 0;
}

int msm_cpp_subscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	CPP_DBG("%s\n", __func__);
	return v4l2_event_subscribe(fh, sub, MAX_CPP_V4l2_EVENTS);
}

int msm_cpp_unsubscribe_event(struct v4l2_subdev *sd, struct v4l2_fh *fh,
	struct v4l2_event_subscription *sub)
{
	CPP_DBG("%s\n", __func__);
	return v4l2_event_unsubscribe(fh, sub);
}

static struct v4l2_subdev_core_ops msm_cpp_subdev_core_ops = {
	.ioctl = msm_cpp_subdev_ioctl,
	.subscribe_event = msm_cpp_subscribe_event,
	.unsubscribe_event = msm_cpp_unsubscribe_event,
};

static const struct v4l2_subdev_ops msm_cpp_subdev_ops = {
	.core = &msm_cpp_subdev_core_ops,
};

static int msm_cpp_enable_debugfs(struct cpp_device *cpp_dev);

static struct v4l2_file_operations msm_cpp_v4l2_subdev_fops;

static long msm_cpp_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct v4l2_fh *vfh = file->private_data;

	switch (cmd) {
	case VIDIOC_DQEVENT:
		if (!(sd->flags & V4L2_SUBDEV_FL_HAS_EVENTS))
			return -ENOIOCTLCMD;

		return v4l2_event_dequeue(vfh, arg, file->f_flags & O_NONBLOCK);

	case VIDIOC_SUBSCRIBE_EVENT:
		return v4l2_subdev_call(sd, core, subscribe_event, vfh, arg);

	case VIDIOC_UNSUBSCRIBE_EVENT:
		return v4l2_subdev_call(sd, core, unsubscribe_event, vfh, arg);

	case VIDIOC_MSM_CPP_GET_INST_INFO: {
		uint32_t i;
		struct cpp_device *cpp_dev = v4l2_get_subdevdata(sd);
		struct msm_camera_v4l2_ioctl_t *ioctl_ptr = arg;
		struct msm_cpp_frame_info_t inst_info;
		for (i = 0; i < MAX_ACTIVE_CPP_INSTANCE; i++) {
			if (cpp_dev->cpp_subscribe_list[i].vfh == vfh) {
				inst_info.inst_id = i;
				break;
			}
		}
		if (copy_to_user(
				(void __user *)ioctl_ptr->ioctl_ptr, &inst_info,
				sizeof(struct msm_cpp_frame_info_t))) {
			return -EINVAL;
		}
	}
	break;
	default:
		return v4l2_subdev_call(sd, core, ioctl, cmd, arg);
	}

	return 0;
}

static long msm_cpp_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_cpp_subdev_do_ioctl);
}

static int __devinit cpp_probe(struct platform_device *pdev)
{
	struct cpp_device *cpp_dev;
	struct msm_cam_subdev_info sd_info;
	int rc = 0;
	CDBG("%s: device id = %d\n", __func__, pdev->id);
	cpp_dev = kzalloc(sizeof(struct cpp_device), GFP_KERNEL);
	if (!cpp_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}
	v4l2_subdev_init(&cpp_dev->subdev, &msm_cpp_subdev_ops);
	cpp_dev->subdev.internal_ops = &msm_cpp_internal_ops;
	snprintf(cpp_dev->subdev.name, ARRAY_SIZE(cpp_dev->subdev.name),
		 "cpp");
	cpp_dev->subdev.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	cpp_dev->subdev.flags |= V4L2_SUBDEV_FL_HAS_EVENTS;
	v4l2_set_subdevdata(&cpp_dev->subdev, cpp_dev);
	platform_set_drvdata(pdev, &cpp_dev->subdev);
	mutex_init(&cpp_dev->mutex);

	cpp_dev->pdev = pdev;

	media_entity_init(&cpp_dev->subdev.entity, 0, NULL, 0);
	cpp_dev->subdev.entity.type = MEDIA_ENT_T_DEVNODE_V4L;
	cpp_dev->subdev.entity.group_id = CPP_DEV;
	cpp_dev->subdev.entity.name = pdev->name;
	sd_info.sdev_type = CPP_DEV;
	sd_info.sd_index = pdev->id;
	msm_cam_register_subdev_node(&cpp_dev->subdev, &sd_info);
	msm_cpp_v4l2_subdev_fops.owner = v4l2_subdev_fops.owner;
	msm_cpp_v4l2_subdev_fops.open = v4l2_subdev_fops.open;
	msm_cpp_v4l2_subdev_fops.unlocked_ioctl = msm_cpp_subdev_fops_ioctl;
	msm_cpp_v4l2_subdev_fops.release = v4l2_subdev_fops.release;
	msm_cpp_v4l2_subdev_fops.poll = v4l2_subdev_fops.poll;

	cpp_dev->subdev.devnode->fops = &msm_cpp_v4l2_subdev_fops;
	cpp_dev->subdev.entity.revision = cpp_dev->subdev.devnode->num;
	msm_cpp_enable_debugfs(cpp_dev);
	msm_queue_init(&cpp_dev->eventData_q, "eventdata");
	msm_queue_init(&cpp_dev->offline_q, "frame");
	msm_queue_init(&cpp_dev->realtime_q, "frame");
	msm_queue_init(&cpp_dev->processing_q, "frame");
	cpp_dev->cpp_open_cnt = 0;

	return rc;
}

static struct platform_driver cpp_driver = {
	.probe = cpp_probe,
	.driver = {
		.name = MSM_CPP_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_cpp_init_module(void)
{
	return platform_driver_register(&cpp_driver);
}

static void __exit msm_cpp_exit_module(void)
{
	platform_driver_unregister(&cpp_driver);
}

static int msm_cpp_debugfs_stream_s(void *data, u64 val)
{
	struct cpp_device *cpp_dev = data;
	CPP_DBG("CPP processing frame E\n");
	while (1) {
		mutex_lock(&cpp_dev->mutex);
		msm_cpp_notify_frame_done(cpp_dev);
		msm_cpp_send_frame_to_hardware(cpp_dev);
		mutex_unlock(&cpp_dev->mutex);
		msleep(20);
	}
	CPP_DBG("CPP processing frame X\n");
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cpp_debugfs_stream, NULL,
			msm_cpp_debugfs_stream_s, "%llu\n");

static int msm_cpp_enable_debugfs(struct cpp_device *cpp_dev)
{
	struct dentry *debugfs_base;
	debugfs_base = debugfs_create_dir("msm_camera", NULL);
	if (!debugfs_base)
		return -ENOMEM;

	if (!debugfs_create_file("test", S_IRUGO | S_IWUSR, debugfs_base,
			(void *)cpp_dev, &cpp_debugfs_stream))
		return -ENOMEM;

	return 0;
}

module_init(msm_cpp_init_module);
module_exit(msm_cpp_exit_module);
MODULE_DESCRIPTION("MSM CPP driver");
MODULE_LICENSE("GPL v2");
