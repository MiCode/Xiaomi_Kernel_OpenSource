/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "CAM-REQ-MGR %s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/cam_req_mgr.h>
#include "cam_req_mgr_dev.h"
#include "cam_dev_mgr_util.h"

#define CAM_REQ_MGR_EVENT_MAX 30

static struct cam_req_mgr_device g_dev;

static int cam_media_device_setup(struct device *dev)
{
	int rc;

	g_dev.v4l2_dev->mdev = kzalloc(sizeof(*g_dev.v4l2_dev->mdev),
		GFP_KERNEL);
	if (!g_dev.v4l2_dev->mdev) {
		rc = -ENOMEM;
		goto mdev_fail;
	}

	media_device_init(g_dev.v4l2_dev->mdev);
	g_dev.v4l2_dev->mdev->dev = dev;
	strlcpy(g_dev.v4l2_dev->mdev->model, CAM_REQ_MGR_VNODE_NAME,
		sizeof(g_dev.v4l2_dev->mdev->model));

	rc = media_device_register(g_dev.v4l2_dev->mdev);
	if (rc)
		goto media_fail;

	return rc;

media_fail:
	kfree(g_dev.v4l2_dev->mdev);
	g_dev.v4l2_dev->mdev = NULL;
mdev_fail:
	return rc;
}

static void cam_media_device_cleanup(void)
{
	media_entity_cleanup(&g_dev.video->entity);
	media_device_unregister(g_dev.v4l2_dev->mdev);
	kfree(g_dev.v4l2_dev->mdev);
	g_dev.v4l2_dev->mdev = NULL;
}

static int cam_v4l2_device_setup(struct device *dev)
{
	int rc;

	g_dev.v4l2_dev = kzalloc(sizeof(*g_dev.v4l2_dev),
		GFP_KERNEL);
	if (!g_dev.v4l2_dev)
		return -ENOMEM;

	rc = v4l2_device_register(dev, g_dev.v4l2_dev);
	if (rc)
		goto reg_fail;

	return rc;

reg_fail:
	kfree(g_dev.v4l2_dev);
	g_dev.v4l2_dev = NULL;
	return rc;
}

static void cam_v4l2_device_cleanup(void)
{
	v4l2_device_unregister(g_dev.v4l2_dev);
	kfree(g_dev.v4l2_dev);
	g_dev.v4l2_dev = NULL;
}

static struct v4l2_file_operations g_cam_fops = {
	.owner  = THIS_MODULE,
	.unlocked_ioctl   = video_ioctl2,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = video_ioctl2,
#endif
};

static int cam_subscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	return v4l2_event_subscribe(fh, sub, CAM_REQ_MGR_EVENT_MAX, NULL);
}

static int cam_unsubscribe_event(struct v4l2_fh *fh,
	const struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}

static const struct v4l2_ioctl_ops g_cam_ioctl_ops = {
	.vidioc_subscribe_event = cam_subscribe_event,
	.vidioc_unsubscribe_event = cam_unsubscribe_event,
};

static int cam_video_device_setup(void)
{
	int rc;

	g_dev.video = video_device_alloc();
	if (!g_dev.video) {
		rc = -ENOMEM;
		goto video_fail;
	}

	g_dev.video->v4l2_dev = g_dev.v4l2_dev;

	strlcpy(g_dev.video->name, "cam-req-mgr",
		sizeof(g_dev.video->name));
	g_dev.video->release = video_device_release;
	g_dev.video->fops = &g_cam_fops;
	g_dev.video->ioctl_ops = &g_cam_ioctl_ops;
	g_dev.video->minor = -1;
	g_dev.video->vfl_type = VFL_TYPE_GRABBER;
	rc = video_register_device(g_dev.video, VFL_TYPE_GRABBER, -1);
	if (rc)
		goto v4l2_fail;

	rc = media_entity_pads_init(&g_dev.video->entity, 0, NULL);
	if (rc)
		goto entity_fail;

	g_dev.video->entity.function = CAM_VNODE_DEVICE_TYPE;
	g_dev.video->entity.name = video_device_node_name(g_dev.video);

	return rc;

entity_fail:
	video_unregister_device(g_dev.video);
v4l2_fail:
	video_device_release(g_dev.video);
	g_dev.video = NULL;
video_fail:
	return rc;
}

void cam_video_device_cleanup(void)
{
	video_unregister_device(g_dev.video);
	video_device_release(g_dev.video);
	g_dev.video = NULL;
}

int cam_register_subdev(struct cam_subdev *csd)
{
	struct v4l2_subdev *sd;
	int rc;

	if (g_dev.state != true) {
		pr_err("camera root device not ready yet");
		return -ENODEV;
	}

	if (!csd || !csd->name) {
		pr_err("invalid arguments");
		return -EINVAL;
	}

	mutex_lock(&g_dev.dev_lock);
	if ((g_dev.subdev_nodes_created) &&
		(csd->sd_flags & V4L2_SUBDEV_FL_HAS_DEVNODE)) {
		pr_err("dynamic node is not allowed, name: %s, type : %d",
			csd->name, csd->ent_function);
		rc = -EINVAL;
		goto reg_fail;
	}

	sd = &csd->sd;
	v4l2_subdev_init(sd, csd->ops);
	sd->internal_ops = csd->internal_ops;
	snprintf(sd->name, ARRAY_SIZE(sd->name), csd->name);
	v4l2_set_subdevdata(sd, csd->token);

	sd->flags = csd->sd_flags;
	sd->entity.num_pads = 0;
	sd->entity.pads = NULL;
	sd->entity.function = csd->ent_function;

	rc = v4l2_device_register_subdev(g_dev.v4l2_dev, sd);
	if (rc) {
		pr_err("register subdev failed");
		goto reg_fail;
	}
	g_dev.count++;

reg_fail:
	mutex_unlock(&g_dev.dev_lock);
	return rc;
}
EXPORT_SYMBOL(cam_register_subdev);

int cam_unregister_subdev(struct cam_subdev *csd)
{
	if (g_dev.state != true) {
		pr_err("camera root device not ready yet");
		return -ENODEV;
	}

	mutex_lock(&g_dev.dev_lock);
	v4l2_device_unregister_subdev(&csd->sd);
	g_dev.count--;
	mutex_unlock(&g_dev.dev_lock);

	return 0;
}
EXPORT_SYMBOL(cam_unregister_subdev);

static int cam_req_mgr_remove(struct platform_device *pdev)
{
	cam_media_device_cleanup();
	cam_video_device_cleanup();
	cam_v4l2_device_cleanup();
	mutex_destroy(&g_dev.dev_lock);
	g_dev.state = false;

	return 0;
}

static int cam_req_mgr_probe(struct platform_device *pdev)
{
	int rc;

	rc = cam_v4l2_device_setup(&pdev->dev);
	if (rc)
		return rc;

	rc = cam_media_device_setup(&pdev->dev);
	if (rc)
		goto media_setup_fail;

	rc = cam_video_device_setup();
	if (rc)
		goto video_setup_fail;

	g_dev.subdev_nodes_created = false;
	mutex_init(&g_dev.dev_lock);
	g_dev.state = true;

	return rc;

video_setup_fail:
	cam_media_device_cleanup();
media_setup_fail:
	cam_v4l2_device_cleanup();
	return rc;
}

static const struct of_device_id cam_req_mgr_dt_match[] = {
	{.compatible = "qcom,cam-req-mgr"},
	{}
};
MODULE_DEVICE_TABLE(of, cam_dt_match);

static struct platform_driver cam_req_mgr_driver = {
	.probe = cam_req_mgr_probe,
	.remove = cam_req_mgr_remove,
	.driver = {
		.name = "cam_req_mgr",
		.owner = THIS_MODULE,
		.of_match_table = cam_req_mgr_dt_match,
	},
};

int cam_dev_mgr_create_subdev_nodes(void)
{
	int rc;
	struct v4l2_subdev *sd;

	if (!g_dev.v4l2_dev)
		return -EINVAL;

	if (g_dev.state != true) {
		pr_err("camera root device not ready yet");
		return -ENODEV;
	}

	mutex_lock(&g_dev.dev_lock);
	if (g_dev.subdev_nodes_created)	{
		rc = -EEXIST;
		goto create_fail;
	}

	rc = v4l2_device_register_subdev_nodes(g_dev.v4l2_dev);
	if (rc) {
		pr_err("failed to register the sub devices");
		goto create_fail;
	}

	list_for_each_entry(sd, &g_dev.v4l2_dev->subdevs, list) {
		if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE))
			continue;
		sd->entity.name = video_device_node_name(sd->devnode);
		pr_debug("created node :%s\n", sd->entity.name);
	}

	g_dev.subdev_nodes_created = true;

create_fail:
	mutex_unlock(&g_dev.dev_lock);
	return rc;
}

static int __init cam_req_mgr_init(void)
{
	return platform_driver_register(&cam_req_mgr_driver);
}

static int __init cam_req_mgr_late_init(void)
{
	return cam_dev_mgr_create_subdev_nodes();
}

static void __exit cam_req_mgr_exit(void)
{
	platform_driver_unregister(&cam_req_mgr_driver);
}

module_init(cam_req_mgr_init);
late_initcall(cam_req_mgr_late_init);
module_exit(cam_req_mgr_exit);
MODULE_DESCRIPTION("Camera Request Manager");
MODULE_LICENSE("GPL v2");
