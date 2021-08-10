// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include "cam_sfe_dev.h"
#include "cam_sfe_core.h"
#include "cam_sfe_soc.h"
#include "cam_sfe680.h"
#include "cam_debug_util.h"
#include "camera_main.h"

static struct cam_hw_intf *cam_sfe_hw_list[CAM_SFE_HW_NUM_MAX];

static char sfe_dev_name[8];

static int cam_sfe_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{

	struct cam_hw_info                *sfe_info = NULL;
	struct cam_hw_intf                *sfe_hw_intf = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct cam_sfe_hw_core_info       *core_info = NULL;
	struct cam_sfe_hw_info            *hw_info = NULL;
	struct platform_device            *pdev = NULL;
	int                                rc = 0;

	pdev = to_platform_device(dev);
	sfe_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!sfe_hw_intf) {
		rc = -ENOMEM;
		goto end;
	}

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &sfe_hw_intf->hw_idx);

	sfe_info = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!sfe_info) {
		rc = -ENOMEM;
		goto free_sfe_hw_intf;
	}

	memset(sfe_dev_name, 0, sizeof(sfe_dev_name));
	snprintf(sfe_dev_name, sizeof(sfe_dev_name),
		"sfe%1u", sfe_hw_intf->hw_idx);

	sfe_info->soc_info.pdev = pdev;
	sfe_info->soc_info.dev = &pdev->dev;
	sfe_info->soc_info.dev_name = sfe_dev_name;
	sfe_hw_intf->hw_priv = sfe_info;
	sfe_hw_intf->hw_ops.get_hw_caps = cam_sfe_get_hw_caps;
	sfe_hw_intf->hw_ops.init = cam_sfe_init_hw;
	sfe_hw_intf->hw_ops.deinit = cam_sfe_deinit_hw;
	sfe_hw_intf->hw_ops.reset = cam_sfe_reset;
	sfe_hw_intf->hw_ops.reserve = cam_sfe_reserve;
	sfe_hw_intf->hw_ops.release = cam_sfe_release;
	sfe_hw_intf->hw_ops.start = cam_sfe_start;
	sfe_hw_intf->hw_ops.stop = cam_sfe_stop;
	sfe_hw_intf->hw_ops.read = cam_sfe_read;
	sfe_hw_intf->hw_ops.write = cam_sfe_write;
	sfe_hw_intf->hw_ops.process_cmd = cam_sfe_process_cmd;
	sfe_hw_intf->hw_type = CAM_ISP_HW_TYPE_SFE;

	CAM_DBG(CAM_SFE, "SFE component bind type %d index %d",
		sfe_hw_intf->hw_type, sfe_hw_intf->hw_idx);

	platform_set_drvdata(pdev, sfe_hw_intf);

	sfe_info->core_info = kzalloc(sizeof(struct cam_sfe_hw_core_info),
		GFP_KERNEL);
	if (!sfe_info->core_info) {
		CAM_DBG(CAM_SFE, "Failed to alloc for core");
		rc = -ENOMEM;
		goto free_sfe_hw;
	}
	core_info = (struct cam_sfe_hw_core_info *)sfe_info->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_SFE, "Of_match Failed");
		rc = -EINVAL;
		goto free_core_info;
	}
	hw_info = (struct cam_sfe_hw_info *)match_dev->data;
	core_info->sfe_hw_info = hw_info;

	rc = cam_sfe_init_soc_resources(&sfe_info->soc_info, cam_sfe_irq,
		sfe_info);
	if (rc < 0) {
		CAM_ERR(CAM_SFE, "Failed to init soc rc=%d", rc);
		goto free_core_info;
	}

	rc = cam_sfe_core_init(core_info, &sfe_info->soc_info,
		sfe_hw_intf, hw_info);
	if (rc < 0) {
		CAM_ERR(CAM_SFE, "Failed to init core rc=%d", rc);
		goto deinit_soc;
	}

	sfe_info->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&sfe_info->hw_mutex);
	spin_lock_init(&sfe_info->hw_lock);
	init_completion(&sfe_info->hw_complete);

	if (sfe_hw_intf->hw_idx < CAM_SFE_HW_NUM_MAX)
		cam_sfe_hw_list[sfe_hw_intf->hw_idx] = sfe_hw_intf;

	CAM_DBG(CAM_SFE, "SFE%d bound successfully",
		sfe_hw_intf->hw_idx);

	return rc;

deinit_soc:
	if (cam_sfe_deinit_soc_resources(&sfe_info->soc_info))
		CAM_ERR(CAM_SFE, "Failed to deinit soc");
free_core_info:
	kfree(sfe_info->core_info);
free_sfe_hw:
	kfree(sfe_info);
free_sfe_hw_intf:
	kfree(sfe_hw_intf);
end:
	return rc;
}

static void cam_sfe_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{

	struct cam_hw_info                *sfe_info = NULL;
	struct cam_hw_intf                *sfe_hw_intf = NULL;
	struct cam_sfe_hw_core_info       *core_info = NULL;
	struct platform_device            *pdev = NULL;
	int                                rc = 0;

	pdev = to_platform_device(dev);
	sfe_hw_intf = platform_get_drvdata(pdev);
	if (!sfe_hw_intf) {
		CAM_ERR(CAM_SFE, "Error! No data in pdev");
		return;
	}

	CAM_DBG(CAM_SFE, "SFE component unbound type %d index %d",
		sfe_hw_intf->hw_type, sfe_hw_intf->hw_idx);

	if (sfe_hw_intf->hw_idx < CAM_SFE_HW_NUM_MAX)
		cam_sfe_hw_list[sfe_hw_intf->hw_idx] = NULL;

	sfe_info = sfe_hw_intf->hw_priv;
	if (!sfe_info) {
		CAM_ERR(CAM_SFE, "HW data is NULL");
		rc = -ENODEV;
		goto free_sfe_hw_intf;
	}

	core_info = (struct cam_sfe_hw_core_info *)sfe_info->core_info;
	if (!core_info) {
		CAM_ERR(CAM_SFE, "core data NULL");
		rc = -EINVAL;
		goto deinit_soc;
	}

	rc = cam_sfe_core_deinit(core_info, core_info->sfe_hw_info);
	if (rc < 0)
		CAM_ERR(CAM_SFE, "Failed to deinit core rc=%d", rc);

	kfree(sfe_info->core_info);

deinit_soc:
	rc = cam_sfe_deinit_soc_resources(&sfe_info->soc_info);
	if (rc < 0)
		CAM_ERR(CAM_SFE, "Failed to deinit soc rc=%d", rc);

	mutex_destroy(&sfe_info->hw_mutex);
	kfree(sfe_info);

	CAM_DBG(CAM_SFE, "SFE%d remove successful", sfe_hw_intf->hw_idx);

free_sfe_hw_intf:
	kfree(sfe_hw_intf);
}

const static struct component_ops cam_sfe_component_ops = {
	.bind = cam_sfe_component_bind,
	.unbind = cam_sfe_component_unbind,
};

int cam_sfe_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_SFE, "Adding SFE component");
	rc = component_add(&pdev->dev, &cam_sfe_component_ops);
	if (rc)
		CAM_ERR(CAM_SFE, "failed to add component rc: %d", rc);

	return rc;
}

int cam_sfe_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_sfe_component_ops);
	return 0;
}

int cam_sfe_hw_init(struct cam_hw_intf **sfe_hw, uint32_t hw_idx)
{
	int rc = 0;

	if (cam_sfe_hw_list[hw_idx]) {
		*sfe_hw = cam_sfe_hw_list[hw_idx];
		rc = 0;
	} else {
		*sfe_hw = NULL;
		rc = -ENODEV;
	}

	return rc;
}

static const struct of_device_id cam_sfe_dt_match[] = {
	{
		.compatible = "",
		.data = &cam_sfe680_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_sfe_dt_match);

struct platform_driver cam_sfe_driver = {
	.probe = cam_sfe_probe,
	.remove = cam_sfe_remove,
	.driver = {
		.name = "cam_sfe",
		.owner = THIS_MODULE,
		.of_match_table = cam_sfe_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_sfe_init_module(void)
{
	return platform_driver_register(&cam_sfe_driver);
}


void cam_sfe_exit_module(void)
{
	platform_driver_unregister(&cam_sfe_driver);
}

MODULE_DESCRIPTION("CAM SFE driver");
MODULE_LICENSE("GPL v2");
