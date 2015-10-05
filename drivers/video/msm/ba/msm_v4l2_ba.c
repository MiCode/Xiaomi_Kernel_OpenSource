/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/msm_ba.h>

#include "msm_ba_internal.h"
#include "msm_ba_debug.h"

#define BASE_DEVICE_NUMBER 35

static struct ba_ctxt *gp_ba_ctxt;

struct ba_ctxt *msm_ba_get_ba_context(void)
{
	return gp_ba_ctxt;
}

void msm_ba_set_ba_context(struct ba_ctxt *ba_ctxt)
{
	gp_ba_ctxt = ba_ctxt;
}

static inline struct msm_ba_inst *get_ba_inst(struct file *filp, void *fh)
{
	return container_of(filp->private_data,
					struct msm_ba_inst, event_handler);
}

static int msm_ba_v4l2_open(struct file *filp)
{
	struct video_device *vdev = video_devdata(filp);
	struct msm_ba_inst *ba_inst;

	ba_inst = msm_ba_open(NULL);
	if (!ba_inst) {
		dprintk(BA_ERR,
			"Failed to create video instance");
		return -ENOMEM;
	}
	clear_bit(V4L2_FL_USES_V4L2_FH, &vdev->flags);
	filp->private_data = &(ba_inst->event_handler);
	return 0;
}

static int msm_ba_v4l2_close(struct file *filp)
{
	int rc = 0;
	struct msm_ba_inst *ba_inst;

	ba_inst = get_ba_inst(filp, NULL);

	rc = msm_ba_close(ba_inst);
	return rc;
}

static int msm_ba_v4l2_querycap(struct file *filp, void *fh,
					struct v4l2_capability *cap)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(filp, fh);

	return msm_ba_querycap((void *)ba_inst, cap);
}

static int msm_ba_v4l2_g_priority(struct file *filp, void *fh,
					enum v4l2_priority *prio)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(filp, fh);

	return msm_ba_g_priority((void *)ba_inst, prio);
}

static int msm_ba_v4l2_s_priority(struct file *filp, void *fh,
					enum v4l2_priority prio)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(filp, fh);

	return msm_ba_s_priority((void *)ba_inst, prio);
}

int msm_ba_v4l2_enum_input(struct file *file, void *fh,
					struct v4l2_input *input)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_enum_input((void *)ba_inst, input);
}

int msm_ba_v4l2_g_input(struct file *file, void *fh,
					unsigned int *index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_input((void *)ba_inst, index);
}

int msm_ba_v4l2_s_input(struct file *file, void *fh,
					unsigned int index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_input((void *)ba_inst, index);
}

int msm_ba_v4l2_enum_output(struct file *file, void *fh,
					struct v4l2_output *output)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_enum_output((void *)ba_inst, output);
}

int msm_ba_v4l2_g_output(struct file *file, void *fh,
					unsigned int *index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_output((void *)ba_inst, index);
}

int msm_ba_v4l2_s_output(struct file *file, void *fh,
					unsigned int index)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_output((void *)ba_inst, index);
}

int msm_ba_v4l2_enum_fmt(struct file *file, void *fh,
					struct v4l2_fmtdesc *f)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_enum_fmt((void *)ba_inst, f);
}

int msm_ba_v4l2_s_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_fmt((void *)ba_inst, f);
}

int msm_ba_v4l2_g_fmt(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_fmt((void *)ba_inst, f);
}

int msm_ba_v4l2_s_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_ctrl((void *)ba_inst, a);
}

int msm_ba_v4l2_g_ctrl(struct file *file, void *fh,
					struct v4l2_control *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_g_ctrl((void *)ba_inst, a);
}

int msm_ba_v4l2_s_ext_ctrl(struct file *file, void *fh,
					struct v4l2_ext_controls *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_ext_ctrl((void *)ba_inst, a);
}

int msm_ba_v4l2_streamon(struct file *file, void *fh,
					enum v4l2_buf_type i)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_streamon((void *)ba_inst, i);
}

int msm_ba_v4l2_streamoff(struct file *file, void *fh,
					enum v4l2_buf_type i)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_streamoff((void *)ba_inst, i);
}

static int msm_ba_v4l2_subscribe_event(struct v4l2_fh *fh,
			const struct v4l2_event_subscription *sub)
{
	struct msm_ba_inst *ba_inst = container_of(fh,
			struct msm_ba_inst, event_handler);

	return msm_ba_subscribe_event((void *)ba_inst, sub);
}

static int msm_ba_v4l2_unsubscribe_event(struct v4l2_fh *fh,
			const struct v4l2_event_subscription *sub)
{
	struct msm_ba_inst *ba_inst = container_of(fh,
			struct msm_ba_inst, event_handler);

	return msm_ba_unsubscribe_event((void *)ba_inst, sub);
}

static int msm_ba_v4l2_s_parm(struct file *file, void *fh,
					struct v4l2_streamparm *a)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(file, fh);

	return msm_ba_s_parm((void *)ba_inst, a);
}

static int msm_ba_v4l2_g_parm(struct file *file, void *fh,
					struct v4l2_streamparm *a)
{
	return 0;
}

static const struct v4l2_ioctl_ops msm_ba_v4l2_ioctl_ops = {
	.vidioc_querycap = msm_ba_v4l2_querycap,
	.vidioc_g_priority = msm_ba_v4l2_g_priority,
	.vidioc_s_priority = msm_ba_v4l2_s_priority,
	.vidioc_enum_fmt_vid_cap = msm_ba_v4l2_enum_fmt,
	.vidioc_enum_fmt_vid_out = msm_ba_v4l2_enum_fmt,
	.vidioc_s_fmt_vid_cap = msm_ba_v4l2_s_fmt,
	.vidioc_s_fmt_vid_cap_mplane = msm_ba_v4l2_s_fmt,
	.vidioc_g_fmt_vid_cap = msm_ba_v4l2_g_fmt,
	.vidioc_g_fmt_vid_cap_mplane = msm_ba_v4l2_g_fmt,
	.vidioc_streamon = msm_ba_v4l2_streamon,
	.vidioc_streamoff = msm_ba_v4l2_streamoff,
	.vidioc_s_ctrl = msm_ba_v4l2_s_ctrl,
	.vidioc_g_ctrl = msm_ba_v4l2_g_ctrl,
	.vidioc_s_ext_ctrls = msm_ba_v4l2_s_ext_ctrl,
	.vidioc_subscribe_event = msm_ba_v4l2_subscribe_event,
	.vidioc_unsubscribe_event = msm_ba_v4l2_unsubscribe_event,
	.vidioc_s_parm = msm_ba_v4l2_s_parm,
	.vidioc_g_parm = msm_ba_v4l2_g_parm,
	.vidioc_enum_input = msm_ba_v4l2_enum_input,
	.vidioc_g_input = msm_ba_v4l2_g_input,
	.vidioc_s_input = msm_ba_v4l2_s_input,
	.vidioc_enum_output = msm_ba_v4l2_enum_output,
	.vidioc_g_output = msm_ba_v4l2_g_output,
	.vidioc_s_output = msm_ba_v4l2_s_output,
};

static unsigned int msm_ba_v4l2_poll(struct file *filp,
	struct poll_table_struct *pt)
{
	struct msm_ba_inst *ba_inst = get_ba_inst(filp, NULL);

	return msm_ba_poll((void *)ba_inst, filp, pt);
}

void msm_ba_release_video_device(struct video_device *pvdev)
{
}

static const struct v4l2_file_operations msm_ba_v4l2_ba_fops = {
	.owner = THIS_MODULE,
	.open = msm_ba_v4l2_open,
	.release = msm_ba_v4l2_close,
	.ioctl = video_ioctl2,
	.poll = msm_ba_v4l2_poll,
};

static int msm_ba_device_init(struct platform_device *pdev,
					struct msm_ba_dev **ret_dev_ctxt)
{
	struct msm_ba_dev *dev_ctxt;
	int nr = BASE_DEVICE_NUMBER;
	int rc = 0;

	dprintk(BA_INFO, "Enter %s", __func__);
	if ((NULL == ret_dev_ctxt) ||
			(NULL != *ret_dev_ctxt))
		return -EINVAL;

	dev_ctxt = kzalloc(sizeof(struct msm_ba_dev), GFP_KERNEL);
	if (NULL == dev_ctxt)
		return -ENOMEM;

	INIT_LIST_HEAD(&dev_ctxt->inputs);
	INIT_LIST_HEAD(&dev_ctxt->instances);
	INIT_LIST_HEAD(&dev_ctxt->sd_events);
	INIT_DELAYED_WORK(&dev_ctxt->sd_events_work,
		msm_ba_subdev_event_hndlr_delayed);
	mutex_init(&dev_ctxt->dev_cs);

	dev_ctxt->state = BA_DEV_UNINIT;

	strlcpy(dev_ctxt->v4l2_dev.name, MSM_BA_DRV_NAME,
		sizeof(dev_ctxt->v4l2_dev.name));
	dev_ctxt->v4l2_dev.dev = &pdev->dev;
	dev_ctxt->v4l2_dev.notify = msm_ba_subdev_event_hndlr;

	rc = v4l2_device_register(dev_ctxt->v4l2_dev.dev, &dev_ctxt->v4l2_dev);
	if (!rc) {
		dev_ctxt->vdev = video_device_alloc();
		if (NULL == dev_ctxt->vdev) {
			v4l2_device_unregister(&dev_ctxt->v4l2_dev);
			rc = -ENOMEM;
		} else {
			strlcpy(dev_ctxt->vdev->name,
				pdev->name, sizeof(dev_ctxt->vdev->name));
			dev_ctxt->vdev->v4l2_dev = &dev_ctxt->v4l2_dev;
			dev_ctxt->vdev->release = msm_ba_release_video_device;
			dev_ctxt->vdev->fops = &msm_ba_v4l2_ba_fops;
			dev_ctxt->vdev->ioctl_ops = &msm_ba_v4l2_ioctl_ops;
			dev_ctxt->vdev->minor = nr;
			dev_ctxt->vdev->vfl_type = VFL_TYPE_GRABBER;

			video_set_drvdata(dev_ctxt->vdev, &dev_ctxt);

			strlcpy(dev_ctxt->mdev.model, MSM_BA_DRV_NAME,
				sizeof(dev_ctxt->mdev.model));
			dev_ctxt->mdev.dev = &pdev->dev;
			rc = media_device_register(&dev_ctxt->mdev);
			dev_ctxt->v4l2_dev.mdev = &dev_ctxt->mdev;
			rc = media_entity_init(&dev_ctxt->vdev->entity,
				0, NULL, 0);
			dev_ctxt->vdev->entity.type = MEDIA_ENT_T_DEVNODE_V4L;
			dev_ctxt->vdev->entity.group_id = 2;

			rc = video_register_device(dev_ctxt->vdev,
					VFL_TYPE_GRABBER, nr);
			if (!rc) {
				dev_ctxt->vdev->entity.name =
				video_device_node_name(dev_ctxt->vdev);
				*ret_dev_ctxt = dev_ctxt;
			} else {
				dprintk(BA_ERR,
					"Failed to register BA video device");
			}
		}
	} else {
	dprintk(BA_ERR, "Failed to register v4l2 device");
	}

	if (rc) {
		kfree(dev_ctxt);
	dev_ctxt = NULL;
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

static int msm_ba_probe(struct platform_device *pdev)
{
	struct ba_ctxt *ba_ctxt;
	int rc = 0;

	dprintk(BA_INFO, "Enter %s: pdev 0x%p device id = %d",
		__func__, pdev, pdev->id);
	ba_ctxt = msm_ba_get_ba_context();

	if (NULL == ba_ctxt) {
		dprintk(BA_ERR, "BA context not yet created");
		return -EINVAL;
	}
	rc = msm_ba_device_init(pdev, &ba_ctxt->dev_ctxt);
	if (rc) {
		dprintk(BA_ERR, "Failed to init device");
	} else {
		ba_ctxt->dev_ctxt->debugfs_root = msm_ba_debugfs_init_dev(
			ba_ctxt->dev_ctxt, ba_ctxt->debugfs_root);
		pdev->dev.platform_data = ba_ctxt->dev_ctxt;
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

static int msm_ba_remove(struct platform_device *pdev)
{
	struct msm_ba_dev *dev_ctxt;
	struct msm_ba_sd_event *ba_sd_event = NULL;
	struct msm_ba_sd_event *ba_sd_event_tmp = NULL;
	int rc = 0;

	dprintk(BA_INFO, "Enter %s", __func__);
	if (!pdev) {
		dprintk(BA_ERR, "%s invalid input %p", __func__, pdev);
		rc = -EINVAL;
	} else {
		dev_ctxt = pdev->dev.platform_data;

		if (NULL == dev_ctxt) {
			dprintk(BA_ERR, "%s invalid device", __func__);
			rc = -EINVAL;
		} else {
			video_unregister_device(dev_ctxt->vdev);
			v4l2_device_unregister(&dev_ctxt->v4l2_dev);
			cancel_delayed_work_sync(&dev_ctxt->sd_events_work);
			list_for_each_entry_safe(ba_sd_event, ba_sd_event_tmp,
					&dev_ctxt->sd_events, list) {
				list_del(&ba_sd_event->list);
				kfree(ba_sd_event);
			}

			kfree(dev_ctxt);
			dev_ctxt = NULL;
		}
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

int msm_ba_create(void)
{
	struct ba_ctxt *ba_ctxt;
	int rc = 0;

	ba_ctxt = msm_ba_get_ba_context();

	if (ba_ctxt != NULL) {
		dprintk(BA_ERR, "BA context already created");
		return -EINVAL;
	}
	ba_ctxt = kzalloc(sizeof(struct ba_ctxt), GFP_KERNEL);

	if (NULL == ba_ctxt)
		return -ENOMEM;

	memset(ba_ctxt, 0x00, sizeof(struct ba_ctxt));

	mutex_init(&ba_ctxt->ba_cs);
	ba_ctxt->debugfs_root = msm_ba_debugfs_init_drv();
	if (!ba_ctxt->debugfs_root)
		dprintk(BA_ERR,
			"Failed to create debugfs for msm_ba");

	msm_ba_set_ba_context(ba_ctxt);

	dprintk(BA_DBG, "%s(%d), BA create complete",
			__func__, __LINE__);

			return rc;
}

int msm_ba_destroy(void)
{
	struct ba_ctxt *ba_ctxt;
	int rc = 0;

	ba_ctxt = msm_ba_get_ba_context();

	if (NULL == ba_ctxt) {
		dprintk(BA_ERR, "BA context non existent");
		return -EINVAL;
	}

	if (ba_ctxt->dev_ctxt != NULL) {
		dprintk(BA_ERR, "Device instances exist on BA context");
		return -EBUSY;
	}
	mutex_destroy(&ba_ctxt->ba_cs);

	kfree(ba_ctxt);
	ba_ctxt = NULL;
	msm_ba_set_ba_context(ba_ctxt);

	return rc;
}

static const struct of_device_id msm_ba_dt_match[] = {
	{.compatible = "qcom,msm-ba"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_ba_dt_match);

static struct platform_driver msm_ba_driver = {
	.probe = msm_ba_probe,
	.remove = msm_ba_remove,
	.driver = {
		.name = "msm_ba_v4l2",
		.owner = THIS_MODULE,
		.of_match_table = msm_ba_dt_match,
	},
};

static int __init msm_ba_mod_init(void)
{
	int rc = 0;

	dprintk(BA_INFO, "Enter %s", __func__);
	rc = msm_ba_create();
	if (!rc) {
		rc = platform_driver_register(&msm_ba_driver);
		if (rc) {
			dprintk(BA_ERR,
				"Failed to register platform driver");
			msm_ba_destroy();
		}
	}
	dprintk(BA_INFO, "Exit %s with error %d", __func__, rc);

	return rc;
}

static void __exit msm_ba_mod_exit(void)
{
	int rc = 0;

	dprintk(BA_INFO, "Enter %s", __func__);
	platform_driver_unregister(&msm_ba_driver);
	rc = msm_ba_destroy();
	dprintk(BA_INFO, "Exit %s", __func__);
}

module_init(msm_ba_mod_init);
module_exit(msm_ba_mod_exit);

