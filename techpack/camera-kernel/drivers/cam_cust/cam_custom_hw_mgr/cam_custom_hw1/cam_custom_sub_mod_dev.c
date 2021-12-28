// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include "cam_custom_sub_mod_dev.h"
#include "cam_custom_sub_mod_core.h"
#include "cam_custom_sub_mod_soc.h"
#include "cam_debug_util.h"
#include "camera_main.h"

static struct cam_hw_intf *cam_custom_hw_sub_mod_list
	[CAM_CUSTOM_SUB_MOD_MAX_INSTANCES] = {0, 0};

struct cam_custom_device_hw_info cam_custom_hw_info = {
	.hw_ver = 0x0,
	.irq_status = 0x0,
	.irq_mask = 0x0,
	.irq_clear = 0x0,
};
EXPORT_SYMBOL(cam_custom_hw_info);

int cam_custom_hw_sub_mod_init(struct cam_hw_intf **custom_hw, uint32_t hw_idx)
{
	int rc = 0;

	if (cam_custom_hw_sub_mod_list[hw_idx]) {
		*custom_hw = cam_custom_hw_sub_mod_list[hw_idx];
		rc = 0;
	} else {
		*custom_hw = NULL;
		rc = -ENODEV;
	}
	return 0;
}

static int cam_custom_hw_sub_mod_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info		    *hw = NULL;
	struct cam_hw_intf		    *hw_intf = NULL;
	struct cam_custom_sub_mod_core_info *core_info = NULL;
	int				   rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!hw_intf)
		return -ENOMEM;

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &hw_intf->hw_idx);

	hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!hw) {
		rc = -ENOMEM;
		goto free_hw_intf;
	}

	hw->soc_info.pdev = pdev;
	hw->soc_info.dev = &pdev->dev;
	hw->soc_info.dev_name = pdev->name;
	hw_intf->hw_priv = hw;
	hw_intf->hw_ops.get_hw_caps = cam_custom_hw_sub_mod_get_hw_caps;
	hw_intf->hw_ops.init = cam_custom_hw_sub_mod_init_hw;
	hw_intf->hw_ops.deinit = cam_custom_hw_sub_mod_deinit_hw;
	hw_intf->hw_ops.reset = cam_custom_hw_sub_mod_reset;
	hw_intf->hw_ops.reserve = cam_custom_hw_sub_mod_reserve;
	hw_intf->hw_ops.release = cam_custom_hw_sub_mod_release;
	hw_intf->hw_ops.start = cam_custom_hw_sub_mod_start;
	hw_intf->hw_ops.stop = cam_custom_hw_sub_mod_stop;
	hw_intf->hw_ops.read = cam_custom_hw_sub_mod_read;
	hw_intf->hw_ops.write = cam_custom_hw_sub_mod_write;
	hw_intf->hw_ops.process_cmd = cam_custom_hw_sub_mod_process_cmd;
	hw_intf->hw_type = CAM_CUSTOM_HW_TYPE_1;

	platform_set_drvdata(pdev, hw_intf);

	hw->core_info = kzalloc(sizeof(struct cam_custom_sub_mod_core_info),
		GFP_KERNEL);
	if (!hw->core_info) {
		CAM_DBG(CAM_CUSTOM, "Failed to alloc for core");
		rc = -ENOMEM;
		goto free_hw;
	}
	core_info = (struct cam_custom_sub_mod_core_info *)hw->core_info;

	core_info->custom_hw_info = hw;

	rc = cam_custom_hw_sub_mod_init_soc_resources(&hw->soc_info,
		cam_custom_hw_sub_mod_irq, hw);
	if (rc < 0) {
		CAM_ERR(CAM_CUSTOM, "Failed to init soc rc=%d", rc);
		goto free_core_info;
	}

	/* Initialize HW */

	hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&hw->hw_mutex);
	spin_lock_init(&hw->hw_lock);
	init_completion(&hw->hw_complete);

	if (hw_intf->hw_idx < CAM_CUSTOM_HW_SUB_MOD_MAX)
		cam_custom_hw_sub_mod_list[hw_intf->hw_idx] = hw_intf;

	/* needs to be invoked when custom hw is in place */
	//cam_custom_hw_sub_mod_init_hw(hw, NULL, 0);

	CAM_DBG(CAM_CUSTOM, "HW idx:%d component bound successfully",
		hw_intf->hw_idx);
	return rc;

free_core_info:
	kfree(hw->core_info);
free_hw:
	kfree(hw);
free_hw_intf:
	kfree(hw_intf);
	return rc;
}

static void cam_custom_hw_sub_mod_component_unbind(
	struct device *dev, struct device *master_dev, void *data)
{
	struct platform_device *pdev = to_platform_device(dev);

	CAM_DBG(CAM_CUSTOM, "Unbinding component: %s", pdev->name);
}

const static struct component_ops cam_custom_hw_sub_mod_component_ops = {
	.bind = cam_custom_hw_sub_mod_component_bind,
	.unbind = cam_custom_hw_sub_mod_component_unbind,
};

int cam_custom_hw_sub_mod_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CUSTOM, "Adding Custom HW sub module component");
	rc = component_add(&pdev->dev, &cam_custom_hw_sub_mod_component_ops);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "failed to add component rc: %d", rc);

	return rc;
}

static const struct of_device_id cam_custom_hw_sub_mod_dt_match[] = {
	{
		.compatible = "qcom,cam_custom_hw_sub_mod",
		.data = &cam_custom_hw_info,
	},
	{}
};

MODULE_DEVICE_TABLE(of, cam_custom_hw_sub_mod_dt_match);

struct platform_driver cam_custom_hw_sub_mod_driver = {
	.probe = cam_custom_hw_sub_mod_probe,
	.driver = {
		.name = CAM_CUSTOM_SUB_MOD_NAME,
		.of_match_table = cam_custom_hw_sub_mod_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_custom_hw_sub_module_init(void)
{
	return platform_driver_register(&cam_custom_hw_sub_mod_driver);
}

void cam_custom_hw_sub_module_exit(void)
{
	platform_driver_unregister(&cam_custom_hw_sub_mod_driver);
}

MODULE_DESCRIPTION("CAM CUSTOM HW SUB MODULE driver");
MODULE_LICENSE("GPL v2");
