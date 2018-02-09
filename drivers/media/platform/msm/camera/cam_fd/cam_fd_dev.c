/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_subdev.h"
#include "cam_node.h"
#include "cam_fd_context.h"
#include "cam_fd_hw_mgr.h"
#include "cam_fd_hw_mgr_intf.h"

#define CAM_FD_DEV_NAME "cam-fd"

/**
 * struct cam_fd_dev - FD device information
 *
 * @sd:         Subdev information
 * @base_ctx:   List of base contexts
 * @fd_ctx:     List of FD contexts
 * @lock:       Mutex handle
 * @open_cnt:   FD subdev open count
 * @probe_done: Whether FD probe is completed
 */
struct cam_fd_dev {
	struct cam_subdev     sd;
	struct cam_context    base_ctx[CAM_CTX_MAX];
	struct cam_fd_context fd_ctx[CAM_CTX_MAX];
	struct mutex          lock;
	uint32_t              open_cnt;
	bool                  probe_done;
};

static struct cam_fd_dev g_fd_dev;

static int cam_fd_dev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_fd_dev *fd_dev = &g_fd_dev;

	if (!fd_dev->probe_done) {
		CAM_ERR(CAM_FD, "FD Dev not initialized, fd_dev=%pK", fd_dev);
		return -ENODEV;
	}

	mutex_lock(&fd_dev->lock);
	fd_dev->open_cnt++;
	CAM_DBG(CAM_FD, "FD Subdev open count %d", fd_dev->open_cnt);
	mutex_unlock(&fd_dev->lock);

	return 0;
}

static int cam_fd_dev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_fd_dev *fd_dev = &g_fd_dev;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	if (!fd_dev->probe_done) {
		CAM_ERR(CAM_FD, "FD Dev not initialized, fd_dev=%pK", fd_dev);
		return -ENODEV;
	}

	mutex_lock(&fd_dev->lock);
	fd_dev->open_cnt--;
	CAM_DBG(CAM_FD, "FD Subdev open count %d", fd_dev->open_cnt);
	mutex_unlock(&fd_dev->lock);

	if (!node) {
		CAM_ERR(CAM_FD, "Node ptr is NULL");
		return -EINVAL;
	}

	cam_node_shutdown(node);

	return 0;
}

static const struct v4l2_subdev_internal_ops cam_fd_subdev_internal_ops = {
	.open = cam_fd_dev_open,
	.close = cam_fd_dev_close,
};

static int cam_fd_dev_probe(struct platform_device *pdev)
{
	int rc;
	int i;
	struct cam_hw_mgr_intf hw_mgr_intf;
	struct cam_node *node;

	g_fd_dev.sd.internal_ops = &cam_fd_subdev_internal_ops;

	/* Initialze the v4l2 subdevice first. (create cam_node) */
	rc = cam_subdev_probe(&g_fd_dev.sd, pdev, CAM_FD_DEV_NAME,
		CAM_FD_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_FD, "FD cam_subdev_probe failed, rc=%d", rc);
		return rc;
	}
	node = (struct cam_node *) g_fd_dev.sd.token;

	rc = cam_fd_hw_mgr_init(pdev->dev.of_node, &hw_mgr_intf);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed in initializing FD HW manager, rc=%d",
			rc);
		goto unregister_subdev;
	}

	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_fd_context_init(&g_fd_dev.fd_ctx[i],
			&g_fd_dev.base_ctx[i], &node->hw_mgr_intf, i);
		if (rc) {
			CAM_ERR(CAM_FD, "FD context init failed i=%d, rc=%d",
				i, rc);
			goto deinit_ctx;
		}
	}

	rc = cam_node_init(node, &hw_mgr_intf, g_fd_dev.base_ctx, CAM_CTX_MAX,
		CAM_FD_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_FD, "FD node init failed, rc=%d", rc);
		goto deinit_ctx;
	}

	mutex_init(&g_fd_dev.lock);
	g_fd_dev.probe_done = true;

	CAM_DBG(CAM_FD, "Camera FD probe complete");

	return 0;

deinit_ctx:
	for (--i; i >= 0; i--) {
		if (cam_fd_context_deinit(&g_fd_dev.fd_ctx[i]))
			CAM_ERR(CAM_FD, "FD context %d deinit failed", i);
	}
unregister_subdev:
	if (cam_subdev_remove(&g_fd_dev.sd))
		CAM_ERR(CAM_FD, "Failed in subdev remove");

	return rc;
}

static int cam_fd_dev_remove(struct platform_device *pdev)
{
	int i, rc;

	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_fd_context_deinit(&g_fd_dev.fd_ctx[i]);
		if (rc)
			CAM_ERR(CAM_FD, "FD context %d deinit failed, rc=%d",
				i, rc);
	}

	rc = cam_fd_hw_mgr_deinit(pdev->dev.of_node);
	if (rc)
		CAM_ERR(CAM_FD, "Failed in hw mgr deinit, rc=%d", rc);

	rc = cam_subdev_remove(&g_fd_dev.sd);
	if (rc)
		CAM_ERR(CAM_FD, "Unregister failed, rc=%d", rc);

	mutex_destroy(&g_fd_dev.lock);
	g_fd_dev.probe_done = false;

	return rc;
}

static const struct of_device_id cam_fd_dt_match[] = {
	{
		.compatible = "qcom,cam-fd"
	},
	{}
};

static struct platform_driver cam_fd_driver = {
	.probe = cam_fd_dev_probe,
	.remove = cam_fd_dev_remove,
	.driver = {
		.name = "cam_fd",
		.owner = THIS_MODULE,
		.of_match_table = cam_fd_dt_match,
	},
};

static int __init cam_fd_dev_init_module(void)
{
	return platform_driver_register(&cam_fd_driver);
}

static void __exit cam_fd_dev_exit_module(void)
{
	platform_driver_unregister(&cam_fd_driver);
}

module_init(cam_fd_dev_init_module);
module_exit(cam_fd_dev_exit_module);
MODULE_DESCRIPTION("MSM FD driver");
MODULE_LICENSE("GPL v2");
