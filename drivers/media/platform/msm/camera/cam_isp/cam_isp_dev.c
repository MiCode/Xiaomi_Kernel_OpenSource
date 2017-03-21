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
#include "cam_node.h"

static struct cam_isp_dev g_isp_dev;

static const struct of_device_id cam_isp_dt_match[] = {
	{
		.compatible = "qcom,cam-isp"
	},
	{}
};

static int cam_isp_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	int i;

	/* clean up resources */
	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_isp_context_deinit(&g_isp_dev.ctx_isp[i]);
		if (rc)
			pr_err("%s: ISP context %d deinit failed\n",
				__func__, i);
	}

	rc = cam_subdev_remove(&g_isp_dev.sd);
	if (rc)
		pr_err("%s: Unregister failed\n", __func__);

	memset(&g_isp_dev, 0, sizeof(g_isp_dev));
	return 0;
}

static int cam_isp_dev_probe(struct platform_device *pdev)
{
	int rc = -1;
	int i;
	struct cam_hw_mgr_intf         hw_mgr_intf;
	struct cam_node               *node;

	/* Initialze the v4l2 subdevice first. (create cam_node) */
	rc = cam_subdev_probe(&g_isp_dev.sd, pdev, CAM_ISP_DEV_NAME,
		CAM_IFE_DEVICE_TYPE);
	if (rc) {
		pr_err("%s: ISP cam_subdev_probe failed!\n", __func__);
		goto err;
	}
	node = (struct cam_node *) g_isp_dev.sd.token;

	/* Initialize the context list */
	for (i = 0; i < CAM_CTX_MAX; i++) {
		rc = cam_isp_context_init(&g_isp_dev.ctx_isp[i],
			&g_isp_dev.ctx[i],
			&node->crm_node_intf,
			&node->hw_mgr_intf);
		if (rc) {
			pr_err("%s: ISP context init failed!\n", __func__);
			goto unregister;
		}
	}

	/* Initialize the cam node */
	rc = cam_node_init(node, &hw_mgr_intf, g_isp_dev.ctx, CAM_CTX_MAX,
		CAM_ISP_DEV_NAME);
	if (rc) {
		pr_err("%s: ISP node init failed!\n", __func__);
		goto unregister;
	}

	pr_info("%s: Camera ISP probe complete\n", __func__);

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

