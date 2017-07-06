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

#define pr_fmt(fmt) "CAM-ICP %s:%d " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <media/v4l2-fh.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/cam_req_mgr.h>
#include <media/cam_defs.h>
#include <media/cam_icp.h>
#include "cam_req_mgr_dev.h"
#include "cam_subdev.h"
#include "cam_node.h"
#include "cam_context.h"
#include "cam_icp_context.h"
#include "cam_hw_mgr_intf.h"
#include "cam_icp_hw_mgr_intf.h"

#define CAM_ICP_DEV_NAME        "cam-icp"

struct cam_icp_subdev {
	struct cam_subdev sd;
	struct cam_node *node;
	struct cam_context ctx[CAM_CTX_MAX];
	struct cam_icp_context ctx_icp[CAM_CTX_MAX];
	struct mutex icp_lock;
	int32_t open_cnt;
	int32_t reserved;
};

static struct cam_icp_subdev g_icp_dev;

static const struct of_device_id cam_icp_dt_match[] = {
	{.compatible = "qcom,cam-icp"},
	{}
};

static int cam_icp_subdev_open(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_hw_mgr_intf *hw_mgr_intf = NULL;
	struct cam_node *node = v4l2_get_subdevdata(sd);
	int rc = 0;

	mutex_lock(&g_icp_dev.icp_lock);
	if (g_icp_dev.open_cnt >= 1) {
		pr_err("ICP subdev is already opened\n");
		rc = -EALREADY;
		goto end;
	}

	if (!node) {
		pr_err("Invalid args\n");
		rc = -EINVAL;
		goto end;
	}

	hw_mgr_intf = &node->hw_mgr_intf;
	rc = hw_mgr_intf->download_fw(hw_mgr_intf->hw_mgr_priv, NULL);
	if (rc < 0) {
		pr_err("FW download failed\n");
		goto end;
	}
	g_icp_dev.open_cnt++;
end:
	mutex_unlock(&g_icp_dev.icp_lock);
	return rc;
}

static int cam_icp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	int rc = 0;
	struct cam_hw_mgr_intf *hw_mgr_intf = NULL;
	struct cam_node *node = v4l2_get_subdevdata(sd);

	mutex_lock(&g_icp_dev.icp_lock);
	if (g_icp_dev.open_cnt <= 0) {
		pr_err("ICP subdev is already closed\n");
		rc = -EINVAL;
		goto end;
	}
	g_icp_dev.open_cnt--;
	if (!node) {
		pr_err("Invalid args\n");
		rc = -EINVAL;
		goto end;
	}

	hw_mgr_intf = &node->hw_mgr_intf;
	if (!hw_mgr_intf) {
		pr_err("hw_mgr_intf is not initialized\n");
		rc = -EINVAL;
		goto end;
	}

	rc = hw_mgr_intf->hw_close(hw_mgr_intf->hw_mgr_priv, NULL);
	if (rc < 0) {
		pr_err("HW close failed\n");
		goto end;
	}

end:
	mutex_unlock(&g_icp_dev.icp_lock);
	return 0;
}

const struct v4l2_subdev_internal_ops cam_icp_subdev_internal_ops = {
	.open = cam_icp_subdev_open,
	.close = cam_icp_subdev_close,
};

static int cam_icp_probe(struct platform_device *pdev)
{
	int rc = 0, i = 0;
	struct cam_node *node;
	struct cam_hw_mgr_intf *hw_mgr_intf;

	if (!pdev) {
		pr_err("pdev is NULL\n");
		return -EINVAL;
	}

	g_icp_dev.sd.pdev = pdev;
	g_icp_dev.sd.internal_ops = &cam_icp_subdev_internal_ops;
	rc = cam_subdev_probe(&g_icp_dev.sd, pdev, CAM_ICP_DEV_NAME,
		CAM_ICP_DEVICE_TYPE);
	if (rc) {
		pr_err("ICP cam_subdev_probe failed!\n");
		goto probe_fail;
	}

	node = (struct cam_node *) g_icp_dev.sd.token;

	hw_mgr_intf = kzalloc(sizeof(*hw_mgr_intf), GFP_KERNEL);
	if (!hw_mgr_intf) {
		rc = -EINVAL;
		goto hw_alloc_fail;
	}

	rc = cam_icp_hw_mgr_init(pdev->dev.of_node, (uint64_t *)hw_mgr_intf);
	if (rc) {
		pr_err("ICP HW manager init failed: %d\n", rc);
		goto hw_init_fail;
	}

	pr_debug("Initializing the ICP contexts\n");
	for (i = 0; i < CAM_CTX_MAX; i++) {
		g_icp_dev.ctx_icp[i].base = &g_icp_dev.ctx[i];
		rc = cam_icp_context_init(&g_icp_dev.ctx_icp[i],
					hw_mgr_intf);
		if (rc) {
			pr_err("ICP context init failed!\n");
			goto ctx_fail;
		}
	}

	pr_debug("Initializing the ICP Node\n");
	rc = cam_node_init(node, hw_mgr_intf, g_icp_dev.ctx,
				CAM_CTX_MAX, CAM_ICP_DEV_NAME);
	if (rc) {
		pr_err("ICP node init failed!\n");
		goto ctx_fail;
	}

	g_icp_dev.open_cnt = 0;
	mutex_init(&g_icp_dev.icp_lock);

	return rc;

ctx_fail:
	for (--i; i >= 0; i--)
		cam_icp_context_deinit(&g_icp_dev.ctx_icp[i]);
hw_init_fail:
	kfree(hw_mgr_intf);
hw_alloc_fail:
	cam_subdev_remove(&g_icp_dev.sd);
probe_fail:
	return rc;
}

static int cam_icp_remove(struct platform_device *pdev)
{
	int i;
	struct v4l2_subdev *sd;
	struct cam_subdev *subdev;

	if (!pdev) {
		pr_err("pdev is NULL\n");
		return -EINVAL;
	}

	sd = platform_get_drvdata(pdev);
	if (!sd) {
		pr_err("V4l2 subdev is NULL\n");
		return -EINVAL;
	}

	subdev = v4l2_get_subdevdata(sd);
	if (!subdev) {
		pr_err("cam subdev is NULL\n");
		return -EINVAL;
	}

	for (i = 0; i < CAM_CTX_MAX; i++)
		cam_icp_context_deinit(&g_icp_dev.ctx_icp[i]);
	cam_node_deinit(g_icp_dev.node);
	cam_subdev_remove(&g_icp_dev.sd);
	mutex_destroy(&g_icp_dev.icp_lock);

	return 0;
}

static struct platform_driver cam_icp_driver = {
	.probe = cam_icp_probe,
	.remove = cam_icp_remove,
	.driver = {
		.name = "cam_icp",
		.owner = THIS_MODULE,
		.of_match_table = cam_icp_dt_match,
	},
};

static int __init cam_icp_init_module(void)
{
	return platform_driver_register(&cam_icp_driver);
}

static void __exit cam_icp_exit_module(void)
{
	platform_driver_unregister(&cam_icp_driver);
}
module_init(cam_icp_init_module);
module_exit(cam_icp_exit_module);
MODULE_DESCRIPTION("MSM ICP driver");
MODULE_LICENSE("GPL v2");
