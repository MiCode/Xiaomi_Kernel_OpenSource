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

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/ion.h>
#include <linux/iommu.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include <uapi/media/cam_req_mgr.h>
#include "cam_isp_dev.h"
#include "cam_isp_log.h"
#include "cam_hw_mgr_intf.h"
#include "cam_isp_hw_mgr_intf.h"
#include "cam_node.h"
#include "cam_debug_util.h"

static struct cam_isp_dev g_isp_dev;

static const struct of_device_id cam_isp_dt_match[] = {
	{
		.compatible = "qcom,cam-isp"
	},
	{}
};

static int cam_isp_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_node *node = v4l2_get_subdevdata(sd);

	if (!node) {
		CAM_ERR(CAM_ISP, "Node ptr is NULL");
		return -EINVAL;
	}

	cam_node_shutdown(node);

	return 0;
}

static const struct v4l2_subdev_internal_ops cam_isp_subdev_internal_ops = {
	.close = cam_isp_subdev_close,
};

static int cam_isp_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	int i;

	/* clean up resources */
	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_isp_context_deinit(&g_isp_dev.ctx_isp[i]);
		if (rc)
			CAM_ERR(CAM_ISP, "ISP context %d deinit failed",
				 i);
	}

	rc = cam_subdev_remove(&g_isp_dev.sd);
	if (rc)
		CAM_ERR(CAM_ISP, "Unregister failed");

	memset(&g_isp_dev, 0, sizeof(g_isp_dev));
	return 0;
}

static int cam_isp_dev_probe(struct platform_device *pdev)
{
	int rc = -1;
	int i;
	struct cam_hw_mgr_intf         hw_mgr_intf;
	struct cam_node               *node;

	g_isp_dev.sd.internal_ops = &cam_isp_subdev_internal_ops;
	/* Initialze the v4l2 subdevice first. (create cam_node) */
	rc = cam_subdev_probe(&g_isp_dev.sd, pdev, CAM_ISP_DEV_NAME,
		CAM_IFE_DEVICE_TYPE);
	if (rc) {
		CAM_ERR(CAM_ISP, "ISP cam_subdev_probe failed!");
		goto err;
	}
	node = (struct cam_node *) g_isp_dev.sd.token;

	memset(&hw_mgr_intf, 0, sizeof(hw_mgr_intf));
	rc = cam_isp_hw_mgr_init(pdev->dev.of_node, &hw_mgr_intf);
	if (rc != 0) {
		CAM_ERR(CAM_ISP, "Can not initialized ISP HW manager!");
		goto unregister;
	}

	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_isp_context_init(&g_isp_dev.ctx_isp[i],
			&g_isp_dev.ctx[i],
			&node->crm_node_intf,
			&node->hw_mgr_intf,
			i);
		if (rc) {
			CAM_ERR(CAM_ISP, "ISP context init failed!");
			goto unregister;
		}
	}

	rc = cam_node_init(node, &hw_mgr_intf, g_isp_dev.ctx, CAM_CTX_MAX,
		CAM_ISP_DEV_NAME);
	if (rc) {
		CAM_ERR(CAM_ISP, "ISP node init failed!");
		goto unregister;
	}

	CAM_INFO(CAM_ISP, "Camera ISP probe complete");

	return 0;
unregister:
	rc = cam_subdev_remove(&g_isp_dev.sd);
err:
	return rc;
}


static struct platform_driver isp_driver = {
	.probe = cam_isp_dev_probe,
	.remove = cam_isp_dev_remove,
	.driver = {
		.name = "cam_isp",
		.owner = THIS_MODULE,
		.of_match_table = cam_isp_dt_match,
	},
};

static int __init cam_isp_dev_init_module(void)
{
	return platform_driver_register(&isp_driver);
}

static void __exit cam_isp_dev_exit_module(void)
{
	platform_driver_unregister(&isp_driver);
}

module_init(cam_isp_dev_init_module);
module_exit(cam_isp_dev_exit_module);
MODULE_DESCRIPTION("MSM ISP driver");
MODULE_LICENSE("GPL v2");
