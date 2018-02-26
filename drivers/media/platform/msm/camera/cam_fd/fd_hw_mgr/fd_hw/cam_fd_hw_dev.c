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

#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include "cam_subdev.h"
#include "cam_fd_hw_intf.h"
#include "cam_fd_hw_core.h"
#include "cam_fd_hw_soc.h"
#include "cam_fd_hw_v41.h"
#include "cam_fd_hw_v501.h"

static int cam_fd_hw_dev_probe(struct platform_device *pdev)
{
	struct cam_hw_info *fd_hw;
	struct cam_hw_intf *fd_hw_intf;
	struct cam_fd_core *fd_core;
	const struct of_device_id *match_dev = NULL;
	struct cam_fd_hw_static_info *hw_static_info = NULL;
	int rc = 0;
	struct cam_fd_hw_init_args init_args;
	struct cam_fd_hw_deinit_args deinit_args;

	fd_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!fd_hw_intf)
		return -ENOMEM;

	fd_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!fd_hw) {
		kfree(fd_hw_intf);
		return -ENOMEM;
	}

	fd_core = kzalloc(sizeof(struct cam_fd_core), GFP_KERNEL);
	if (!fd_core) {
		kfree(fd_hw);
		kfree(fd_hw_intf);
		return -ENOMEM;
	}

	fd_hw_intf->hw_priv = fd_hw;
	fd_hw->core_info = fd_core;

	fd_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	fd_hw->soc_info.pdev = pdev;
	fd_hw->soc_info.dev = &pdev->dev;
	fd_hw->soc_info.dev_name = pdev->name;
	fd_hw->open_count = 0;
	mutex_init(&fd_hw->hw_mutex);
	spin_lock_init(&fd_hw->hw_lock);
	init_completion(&fd_hw->hw_complete);

	spin_lock_init(&fd_core->spin_lock);
	init_completion(&fd_core->processing_complete);
	init_completion(&fd_core->halt_complete);
	init_completion(&fd_core->reset_complete);

	fd_hw_intf->hw_ops.get_hw_caps = cam_fd_hw_get_hw_caps;
	fd_hw_intf->hw_ops.init = cam_fd_hw_init;
	fd_hw_intf->hw_ops.deinit = cam_fd_hw_deinit;
	fd_hw_intf->hw_ops.reset = cam_fd_hw_reset;
	fd_hw_intf->hw_ops.reserve = cam_fd_hw_reserve;
	fd_hw_intf->hw_ops.release = cam_fd_hw_release;
	fd_hw_intf->hw_ops.start = cam_fd_hw_start;
	fd_hw_intf->hw_ops.stop = cam_fd_hw_halt_reset;
	fd_hw_intf->hw_ops.read = NULL;
	fd_hw_intf->hw_ops.write = NULL;
	fd_hw_intf->hw_ops.process_cmd = cam_fd_hw_process_cmd;
	fd_hw_intf->hw_type = CAM_HW_FD;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev || !match_dev->data) {
		CAM_ERR(CAM_FD, "No Of_match data, %pK", match_dev);
		rc = -EINVAL;
		goto free_memory;
	}
	hw_static_info = (struct cam_fd_hw_static_info *)match_dev->data;
	fd_core->hw_static_info = hw_static_info;

	CAM_DBG(CAM_FD, "HW Static Info : version core[%d.%d] wrapper[%d.%d]",
		hw_static_info->core_version.major,
		hw_static_info->core_version.minor,
		hw_static_info->wrapper_version.major,
		hw_static_info->wrapper_version.minor);

	rc = cam_fd_soc_init_resources(&fd_hw->soc_info, cam_fd_hw_irq, fd_hw);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed to init soc, rc=%d", rc);
		goto free_memory;
	}

	fd_hw_intf->hw_idx = fd_hw->soc_info.index;

	memset(&init_args, 0x0, sizeof(init_args));
	memset(&deinit_args, 0x0, sizeof(deinit_args));
	rc = cam_fd_hw_init(fd_hw, &init_args, sizeof(init_args));
	if (rc) {
		CAM_ERR(CAM_FD, "Failed to hw init, rc=%d", rc);
		goto deinit_platform_res;
	}

	rc = cam_fd_hw_util_get_hw_caps(fd_hw, &fd_core->hw_caps);
	if (rc) {
		CAM_ERR(CAM_FD, "Failed to get hw caps, rc=%d", rc);
		goto deinit_hw;
	}

	rc = cam_fd_hw_deinit(fd_hw, &deinit_args, sizeof(deinit_args));
	if (rc) {
		CAM_ERR(CAM_FD, "Failed to deinit hw, rc=%d", rc);
		goto deinit_platform_res;
	}

	platform_set_drvdata(pdev, fd_hw_intf);
	CAM_DBG(CAM_FD, "FD-%d probe successful", fd_hw_intf->hw_idx);

	return rc;

deinit_hw:
	if (cam_fd_hw_deinit(fd_hw, &deinit_args, sizeof(deinit_args)))
		CAM_ERR(CAM_FD, "Failed in hw deinit");
deinit_platform_res:
	if (cam_fd_soc_deinit_resources(&fd_hw->soc_info))
		CAM_ERR(CAM_FD, "Failed in soc deinit");
	mutex_destroy(&fd_hw->hw_mutex);
free_memory:
	kfree(fd_hw);
	kfree(fd_hw_intf);
	kfree(fd_core);

	return rc;
}

static int cam_fd_hw_dev_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct cam_hw_intf *fd_hw_intf;
	struct cam_hw_info *fd_hw;
	struct cam_fd_core *fd_core;

	fd_hw_intf = platform_get_drvdata(pdev);
	if (!fd_hw_intf) {
		CAM_ERR(CAM_FD, "Invalid fd_hw_intf from pdev");
		return -EINVAL;
	}

	fd_hw = fd_hw_intf->hw_priv;
	if (!fd_hw) {
		CAM_ERR(CAM_FD, "Invalid fd_hw from fd_hw_intf");
		rc = -ENODEV;
		goto free_fd_hw_intf;
	}

	fd_core = (struct cam_fd_core *)fd_hw->core_info;
	if (!fd_core) {
		CAM_ERR(CAM_FD, "Invalid fd_core from fd_hw");
		rc = -EINVAL;
		goto deinit_platform_res;
	}

	kfree(fd_core);

deinit_platform_res:
	rc = cam_fd_soc_deinit_resources(&fd_hw->soc_info);
	if (rc)
		CAM_ERR(CAM_FD, "Error in FD soc deinit, rc=%d", rc);

	mutex_destroy(&fd_hw->hw_mutex);
	kfree(fd_hw);

free_fd_hw_intf:
	kfree(fd_hw_intf);

	return rc;
}

static const struct of_device_id cam_fd_hw_dt_match[] = {
	{
		.compatible = "qcom,fd41",
		.data = &cam_fd_wrapper120_core410_info,
	},
	{
		.compatible = "qcom,fd501",
		.data = &cam_fd_wrapper200_core501_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_fd_hw_dt_match);

static struct platform_driver cam_fd_hw_driver = {
	.probe = cam_fd_hw_dev_probe,
	.remove = cam_fd_hw_dev_remove,
	.driver = {
		.name = "cam_fd_hw",
		.owner = THIS_MODULE,
		.of_match_table = cam_fd_hw_dt_match,
	},
};

static int __init cam_fd_hw_init_module(void)
{
	return platform_driver_register(&cam_fd_hw_driver);
}

static void __exit cam_fd_hw_exit_module(void)
{
	platform_driver_unregister(&cam_fd_hw_driver);
}

module_init(cam_fd_hw_init_module);
module_exit(cam_fd_hw_exit_module);
MODULE_DESCRIPTION("CAM FD HW driver");
MODULE_LICENSE("GPL v2");
