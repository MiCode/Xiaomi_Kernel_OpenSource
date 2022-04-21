// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": %s: " fmt, __func__

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_graph.h>

#include <media/v4l2-ioctl.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-fwnode.h>
#include <media/v4l2-common.h>

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "mtk-composite.h"

/******************************************************************************
 * Definition
 *****************************************************************************/

#define MTK_COMPOSITE_NAME "mtk-composite-v4l2-1"

static int
fl_async_bound(struct v4l2_async_notifier *notifier,
		 struct v4l2_subdev *subdev,
		 struct v4l2_async_subdev *asd)
{
	struct mtk_composite_v4l2_device *pfdev =
			container_of(notifier->v4l2_dev,
			struct mtk_composite_v4l2_device, v4l2_dev);
	bool found = false;
	int i;

	for (i = 0; i < ARRAY_SIZE(pfdev->asd); i++) {
		if (pfdev->asd[i]->match.fwnode ==
			asd[0].match.fwnode) {
			pfdev->sd[i] = subdev;
			found = true;
			break;
		}
	}

	if (!found) {
		pr_info("sub device (%s) not matched\n", subdev->name);
		return -EINVAL;
	}

	return 0;
}

static int fl_probe_complete(struct mtk_composite_v4l2_device *vpfe)
{
	int err;
	struct v4l2_subdev *sd;

	/* set first sub device as current one */
	vpfe->v4l2_dev.ctrl_handler = vpfe->sd[0]->ctrl_handler;

	err = v4l2_device_register_subdev_nodes(&vpfe->v4l2_dev);
	if (err) {
		pr_info("Unable to v4l2_device_register_subdev_nodes\n");
		goto probe_out;
	}

	list_for_each_entry(sd, &vpfe->v4l2_dev.subdevs, list) {
		if (!(sd->flags & V4L2_SUBDEV_FL_HAS_DEVNODE))
			continue;

#if defined(CONFIG_MEDIA_CONTROLLER)
		pr_info("%s v4l2:%s\n", __func__, sd->entity.name);
#endif
	}

	pr_debug("%s -\n", __func__);
	return 0;

probe_out:
	v4l2_device_unregister(&vpfe->v4l2_dev);
	return err;
}

static int fl_async_complete(struct v4l2_async_notifier *notifier)
{
	struct mtk_composite_v4l2_device *pfdev =
		container_of(notifier->v4l2_dev,
			struct mtk_composite_v4l2_device, v4l2_dev);

	return fl_probe_complete(pfdev);
}


static struct v4l2_async_subdev *
mtk_get_pdata(struct platform_device *pdev,
	struct mtk_composite_v4l2_device *pfdev)
{
	struct device_node *endpoint = NULL;
	struct v4l2_async_subdev *pdata[MISC_MAX_SUBDEVS] = {0};
	struct v4l2_async_notifier *notifier;
	unsigned int i;

	if (!IS_ENABLED(CONFIG_OF) || !pdev->dev.of_node) {
		pr_info("pdev->dev.of_node %p\n", pdev->dev.of_node);
		return pdev->dev.platform_data;
	}

	notifier = &pfdev->notifier;

	for (i = 0; ; i++) {
		struct device_node *rem;

		endpoint = of_graph_get_next_endpoint(pdev->dev.of_node,
						endpoint);
		if (!endpoint)
			break;

		rem = of_graph_get_remote_port_parent(endpoint);
		if (!rem) {
			pr_info("Remote device at %s not found\n",
				endpoint->full_name);
			goto done;
		}

		pdata[i] = devm_kzalloc(&pdev->dev,
				sizeof(struct v4l2_async_subdev),
				GFP_KERNEL);
		if (!pdata[i]) {
			of_node_put(rem);
		pr_info("i %d, pdata %p\n", i, pdata[i]);
			goto done;
		}
		pr_debug("rem %p, pdata[i] %p, name %s, full_name %s\n",
				rem, pdata[i], rem->name, rem->full_name);
		pdata[i]->match_type = V4L2_ASYNC_MATCH_FWNODE;
		pdata[i]->match.fwnode = of_fwnode_handle(rem);
		of_node_put(rem);
		notifier->num_subdevs++;
		pfdev->asd[i] = pdata[i];
	}

	of_node_put(endpoint);
	return pdata[0];

done:
	of_node_put(endpoint);
	return NULL;
}

static void mtk_composite_unregister_entities(
		struct mtk_composite_v4l2_device *isp)
{
	v4l2_device_unregister(&isp->v4l2_dev);
	media_device_unregister(&isp->media_dev);
}

static const struct v4l2_async_notifier_operations fl_async_notify_ops = {
	.bound = fl_async_bound,
	.complete = fl_async_complete,
};

static int mtk_composite_probe(struct platform_device *dev)
{
	struct mtk_composite_v4l2_device *pfdev;
	int rc = 0;

	pr_info("flash v4l2 probe\n");
	pfdev = devm_kzalloc(&dev->dev, sizeof(*pfdev), GFP_KERNEL);
	if (!pfdev) {
		pr_info("could not allocate memory\n");
		return -ENOMEM;
	}

	pfdev->vdev = video_device_alloc();
	if (WARN_ON(!pfdev->vdev)) {
		rc = -ENOMEM;
		pr_info("failed to allocate video_device\n");
		goto vdec_end;
	}

	pfdev->asd[0] = mtk_get_pdata(dev, pfdev);
	pr_debug("asd %p %p %p\n", pfdev->asd[0], pfdev->asd[1],
		pfdev->asd[2]);


#if defined(CONFIG_MEDIA_CONTROLLER)
	pfdev->v4l2_dev.mdev = kzalloc(sizeof(struct media_device),
		GFP_KERNEL);
	if (!pfdev->v4l2_dev.mdev) {
		rc = -ENOMEM;
		pr_info("failed to allocate  media_device\n");
		goto mdev_end;
	}
	strlcpy(pfdev->v4l2_dev.mdev->model, "mtk_V4L2_misc_core",
			sizeof(pfdev->v4l2_dev.mdev->model));
	pfdev->v4l2_dev.mdev->dev = &(dev->dev);

	media_device_init(pfdev->v4l2_dev.mdev);

	rc = media_device_register(pfdev->v4l2_dev.mdev);
	if (WARN_ON(rc < 0)) {
		pr_info("failed to register media_device");
		goto mdev_end;
	}

	/* Initialize mutex */
	mutex_init(&pfdev->v4l2_dev.mdev->graph_mutex);
	rc = media_entity_pads_init(&pfdev->vdev->entity, 0, NULL);

	if (WARN_ON(rc < 0)) {
		pr_info("media_entity_pads init failed\n");
		goto mdev_end;
	}
	pfdev->vdev->entity.function = MEDIA_ENT_F_IO_V4L;
#endif

	rc = v4l2_device_register(&dev->dev, &pfdev->v4l2_dev);
	if (rc) {
		pr_info("Unable to register v4l2 device.\n");
		goto mdev_end;
	}
	platform_set_drvdata(dev, pfdev);

	pr_debug("platform_set_drvdata num_subdevs %d\n",
		pfdev->notifier.num_subdevs);

	pfdev->sd = devm_kzalloc(&dev->dev, sizeof(struct v4l2_subdev *) *
		ARRAY_SIZE(pfdev->asd), GFP_KERNEL);
	if (!pfdev->sd) {
		rc = -ENOMEM;
		pr_info("Unable to devm_kzalloc.\n");
		goto mdev_end;
	}

	pfdev->notifier.subdevs = pfdev->asd;
	pfdev->notifier.ops = &fl_async_notify_ops;

	rc = v4l2_async_notifier_register(&pfdev->v4l2_dev, &pfdev->notifier);
	if (rc) {
		pr_info("Error registering async notifier\n");
		rc = -EINVAL;
		goto mdev_end;
	}

	return 0;

mdev_end:
	kzfree(pfdev->vdev);
vdec_end:
	kfree(pfdev);

	return rc;
}

static int mtk_composite_remove(struct platform_device *dev)
{
	struct mtk_composite_v4l2_device *isp = platform_get_drvdata(dev);

	v4l2_async_notifier_unregister(&isp->notifier);
	mtk_composite_unregister_entities(isp);

	return 0;
}

static void mtk_composite_shutdown(struct platform_device *dev)
{

}

#ifdef CONFIG_OF
static const struct of_device_id mtk_composite_of_match[] = {
	{.compatible = "mediatek,mtk_composite_v4l2_1"},
	{},
};
MODULE_DEVICE_TABLE(of, mtk_composite_of_match);

#else
static struct platform_device mtk_composite_platform_device[] = {
	{
		.name = MTK_COMPOSITE_NAME,
		.id = 0,
		.dev = {}
	},
	{}
};
MODULE_DEVICE_TABLE(platform, mtk_composite_platform_device);
#endif

static struct platform_driver mtk_composite_platform_driver = {
	.probe = mtk_composite_probe,
	.remove = mtk_composite_remove,
	.shutdown = mtk_composite_shutdown,
	.driver = {
		   .name = MTK_COMPOSITE_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mtk_composite_of_match,
#endif
	},
};

static int __init mtk_composite_init(void)
{
	int ret;

	pr_debug("Init start\n");

#ifndef CONFIG_OF
	ret = platform_device_register(&mtk_composite_platform_device);
	if (ret) {
		pr_info("Failed to register platform device\n");
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_composite_platform_driver);
	if (ret) {
		pr_info("Failed to register platform driver\n");
		return ret;
	}

	pr_debug("Init done\n");

	return 0;
}

static void __exit mtk_composite_exit(void)
{
	platform_driver_unregister(&mtk_composite_platform_driver);
}

late_initcall(mtk_composite_init);
module_exit(mtk_composite_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK V4L2 Composite Driver");
