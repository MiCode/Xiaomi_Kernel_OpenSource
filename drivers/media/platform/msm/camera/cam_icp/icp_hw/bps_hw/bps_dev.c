/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include "bps_core.h"
#include "bps_soc.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_io_util.h"
#include "cam_icp_hw_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"

static struct cam_bps_device_hw_info cam_bps_hw_info = {
	.hw_idx = 0,
	.pwr_ctrl = 0x5c,
	.pwr_status = 0x58,
	.reserved = 0,
};
EXPORT_SYMBOL(cam_bps_hw_info);

int cam_bps_register_cpas(struct cam_hw_soc_info *soc_info,
			struct cam_bps_device_core_info *core_info,
			uint32_t hw_idx)
{
	struct cam_cpas_register_params cpas_register_params;
	int rc;

	cpas_register_params.dev = &soc_info->pdev->dev;
	memcpy(cpas_register_params.identifier, "bps", sizeof("bps"));
	cpas_register_params.cam_cpas_client_cb = NULL;
	cpas_register_params.cell_index = hw_idx;
	cpas_register_params.userdata = NULL;

	rc = cam_cpas_register_client(&cpas_register_params);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "failed: %d", rc);
		return rc;
	}
	core_info->cpas_handle = cpas_register_params.client_handle;

	return rc;
}

int cam_bps_probe(struct platform_device *pdev)
{
	struct cam_hw_info            *bps_dev = NULL;
	struct cam_hw_intf            *bps_dev_intf = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct cam_bps_device_core_info   *core_info = NULL;
	struct cam_bps_device_hw_info     *hw_info = NULL;
	int                                rc = 0;

	bps_dev_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!bps_dev_intf)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &bps_dev_intf->hw_idx);

	bps_dev = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!bps_dev) {
		kfree(bps_dev_intf);
		return -ENOMEM;
	}
	bps_dev->soc_info.pdev = pdev;
	bps_dev->soc_info.dev = &pdev->dev;
	bps_dev->soc_info.dev_name = pdev->name;
	bps_dev_intf->hw_priv = bps_dev;
	bps_dev_intf->hw_ops.init = cam_bps_init_hw;
	bps_dev_intf->hw_ops.deinit = cam_bps_deinit_hw;
	bps_dev_intf->hw_ops.process_cmd = cam_bps_process_cmd;
	bps_dev_intf->hw_type = CAM_ICP_DEV_BPS;
	platform_set_drvdata(pdev, bps_dev_intf);
	bps_dev->core_info = kzalloc(sizeof(struct cam_bps_device_core_info),
					GFP_KERNEL);
	if (!bps_dev->core_info) {
		kfree(bps_dev);
		kfree(bps_dev_intf);
		return -ENOMEM;
	}
	core_info = (struct cam_bps_device_core_info *)bps_dev->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ICP, "No bps hardware info");
		kfree(bps_dev->core_info);
		kfree(bps_dev);
		kfree(bps_dev_intf);
		rc = -EINVAL;
		return rc;
	}
	hw_info = &cam_bps_hw_info;
	core_info->bps_hw_info = hw_info;

	rc = cam_bps_init_soc_resources(&bps_dev->soc_info, cam_bps_irq,
		bps_dev);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "failed to init_soc");
		kfree(bps_dev->core_info);
		kfree(bps_dev);
		kfree(bps_dev_intf);
		return rc;
	}
	CAM_DBG(CAM_ICP, "soc info : %pK",
		(void *)&bps_dev->soc_info);

	rc = cam_bps_register_cpas(&bps_dev->soc_info,
			core_info, bps_dev_intf->hw_idx);
	if (rc < 0) {
		kfree(bps_dev->core_info);
		kfree(bps_dev);
		kfree(bps_dev_intf);
		return rc;
	}
	bps_dev->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&bps_dev->hw_mutex);
	spin_lock_init(&bps_dev->hw_lock);
	init_completion(&bps_dev->hw_complete);
	CAM_DBG(CAM_ICP, "BPS%d probe successful",
		bps_dev_intf->hw_idx);

	return rc;
}

static const struct of_device_id cam_bps_dt_match[] = {
	{
		.compatible = "qcom,cam-bps",
		.data = &cam_bps_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_bps_dt_match);

static struct platform_driver cam_bps_driver = {
	.probe = cam_bps_probe,
	.driver = {
		.name = "cam-bps",
		.owner = THIS_MODULE,
		.of_match_table = cam_bps_dt_match,
	},
};

static int __init cam_bps_init_module(void)
{
	return platform_driver_register(&cam_bps_driver);
}

static void __exit cam_bps_exit_module(void)
{
	platform_driver_unregister(&cam_bps_driver);
}

module_init(cam_bps_init_module);
module_exit(cam_bps_exit_module);
MODULE_DESCRIPTION("CAM BPS driver");
MODULE_LICENSE("GPL v2");
