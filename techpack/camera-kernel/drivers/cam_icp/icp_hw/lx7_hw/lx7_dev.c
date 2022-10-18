// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>

#include "camera_main.h"
#include "cam_debug_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_icp_hw_intf.h"
#include "lx7_core.h"
#include "lx7_soc.h"

static int cam_lx7_component_bind(struct device *dev,
				struct device *mdev, void *data)
{
	int rc = 0;
	struct cam_hw_intf *lx7_intf = NULL;
	struct cam_hw_info *lx7_info = NULL;
	struct lx7_soc_info *lx7_soc_info = NULL;
	struct cam_lx7_core_info *core_info = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	lx7_intf = kzalloc(sizeof(*lx7_intf), GFP_KERNEL);
	if (!lx7_intf)
		return -ENOMEM;

	lx7_info = kzalloc(sizeof(*lx7_info), GFP_KERNEL);
	if (!lx7_info) {
		rc = -ENOMEM;
		goto free_hw_intf;
	}

	lx7_soc_info = kzalloc(sizeof(*lx7_soc_info), GFP_KERNEL);
	if (!lx7_soc_info) {
		rc = -ENOMEM;
		goto free_hw_info;
	}

	core_info = kzalloc(sizeof(*core_info), GFP_KERNEL);
	if (!core_info) {
		rc = -ENOMEM;
		goto free_soc_info;
	}

	lx7_info->core_info = core_info;
	lx7_info->soc_info.pdev = pdev;
	lx7_info->soc_info.dev = &pdev->dev;
	lx7_info->soc_info.dev_name = pdev->name;
	lx7_info->soc_info.soc_private = lx7_soc_info;

	mutex_init(&lx7_info->hw_mutex);
	spin_lock_init(&lx7_info->hw_lock);
	init_completion(&lx7_info->hw_complete);

	rc = cam_lx7_soc_resources_init(&lx7_info->soc_info,
					cam_lx7_handle_irq, lx7_info);
	if (rc) {
		CAM_ERR(CAM_ICP, "soc resources init failed rc=%d", rc);
		goto free_core_info;
	}

	lx7_intf->hw_priv = lx7_info;
	lx7_intf->hw_type = CAM_ICP_DEV_LX7;
	lx7_intf->hw_idx = lx7_info->soc_info.index;
	lx7_intf->hw_ops.init = cam_lx7_hw_init;
	lx7_intf->hw_ops.deinit = cam_lx7_hw_deinit;
	lx7_intf->hw_ops.process_cmd = cam_lx7_process_cmd;

	rc = cam_lx7_cpas_register(lx7_intf);
	if (rc) {
		CAM_ERR(CAM_ICP, "cpas registration failed rc=%d", rc);
		goto res_deinit;
	}

	platform_set_drvdata(pdev, lx7_intf);

	return 0;

res_deinit:
	cam_lx7_soc_resources_deinit(&lx7_info->soc_info);
free_core_info:
	kfree(core_info);
free_soc_info:
	kfree(lx7_soc_info);
free_hw_info:
	kfree(lx7_info);
free_hw_intf:
	kfree(lx7_intf);

	return rc;
}

static void cam_lx7_component_unbind(struct device *dev,
				struct device *mdev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct cam_hw_intf *lx7_intf = platform_get_drvdata(pdev);
	struct cam_hw_info *lx7_info = lx7_intf->hw_priv;

	cam_lx7_cpas_unregister(lx7_intf);
	cam_lx7_soc_resources_deinit(&lx7_info->soc_info);

	kfree(lx7_info->core_info);
	kfree(lx7_info->soc_info.soc_private);
	kfree(lx7_info);
	kfree(lx7_intf);
}

static const struct component_ops cam_lx7_component_ops = {
	.bind = cam_lx7_component_bind,
	.unbind = cam_lx7_component_unbind,
};

static const struct of_device_id cam_lx7_match[] = {
	{ .compatible = "qcom,cam-lx7"},
	{},
};
MODULE_DEVICE_TABLE(of, cam_lx7_match);

static int cam_lx7_driver_probe(struct platform_device *pdev)
{
	int rc;

	rc = component_add(&pdev->dev, &cam_lx7_component_ops);
	if (rc)
		CAM_ERR(CAM_ICP, "cam-lx7 component add failed rc=%d", rc);

	return rc;
}

static int cam_lx7_driver_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_lx7_component_ops);

	return 0;
}

struct platform_driver cam_lx7_driver = {
	.probe = cam_lx7_driver_probe,
	.remove = cam_lx7_driver_remove,
	.driver = {
		.name = "cam-lx7",
		.of_match_table = cam_lx7_match,
		.suppress_bind_attrs = true,
	},
};

int cam_lx7_init_module(void)
{
	return platform_driver_register(&cam_lx7_driver);
}

void cam_lx7_exit_module(void)
{
	platform_driver_unregister(&cam_lx7_driver);
}

MODULE_DESCRIPTION("Camera LX7 driver");
MODULE_LICENSE("GPL v2");
