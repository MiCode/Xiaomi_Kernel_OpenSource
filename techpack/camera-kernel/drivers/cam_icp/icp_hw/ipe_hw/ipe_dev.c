// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include <linux/timer.h>
#include "ipe_core.h"
#include "ipe_soc.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_io_util.h"
#include "cam_icp_hw_intf.h"
#include "cam_icp_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "camera_main.h"

static struct cam_ipe_device_hw_info cam_ipe_hw_info[] = {
	{
		.hw_idx = 0,
		.pwr_ctrl = 0x40,
		.pwr_status = 0x3C,
		.top_rst_cmd = 0x1008,
		.top_irq_status = 0x100C,
		.cdm_rst_cmd = 0x10,
		.cdm_irq_status = 0x44,
		.cdm_rst_val = 0xF,
	},
};

static struct cam_ipe_device_hw_info cam_ipe680_hw_info[] = {
	{
		.hw_idx = 0,
		.pwr_ctrl = 0x40,
		.pwr_status = 0x3C,
		.top_rst_cmd = 0x1008,
		.top_irq_status = 0x100C,
		.cdm_rst_cmd = 0x10,
		.cdm_irq_status = 0x44,
		.cdm_rst_val = 0x7F,
	},
};

int cam_ipe_register_cpas(struct cam_hw_soc_info *soc_info,
	struct cam_ipe_device_core_info *core_info,
	uint32_t hw_idx)
{
	struct cam_cpas_register_params cpas_register_params;
	int rc;

	cpas_register_params.dev = &soc_info->pdev->dev;
	memcpy(cpas_register_params.identifier, "ipe", sizeof("ipe"));
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

static int cam_ipe_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info            *ipe_dev = NULL;
	struct cam_hw_intf            *ipe_dev_intf = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct cam_ipe_device_core_info   *core_info = NULL;
	struct cam_ipe_device_hw_info     *hw_info = NULL;
	int                                rc = 0;
	struct cam_cpas_query_cap query;
	uint32_t cam_caps;
	uint32_t hw_idx;
	struct platform_device *pdev = to_platform_device(dev);

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &hw_idx);

	rc = cam_cpas_get_hw_info(&query.camera_family,
			&query.camera_version, &query.cpas_version,
			&cam_caps, NULL);
	if (rc) {
		CAM_ERR(CAM_ICP, "failed to get hw info rc=%d", rc);
		return rc;
	}

	if ((!(cam_caps & CPAS_IPE1_BIT)) && (hw_idx)) {
		CAM_ERR(CAM_ICP, "IPE1 hw idx = %d\n", hw_idx);
		return -EINVAL;
	}

	ipe_dev_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!ipe_dev_intf)
		return -ENOMEM;

	ipe_dev_intf->hw_idx = hw_idx;
	ipe_dev = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!ipe_dev) {
		kfree(ipe_dev_intf);
		return -ENOMEM;
	}

	ipe_dev->soc_info.pdev = pdev;
	ipe_dev->soc_info.dev = &pdev->dev;
	ipe_dev->soc_info.dev_name = pdev->name;
	ipe_dev_intf->hw_priv = ipe_dev;
	ipe_dev_intf->hw_ops.init = cam_ipe_init_hw;
	ipe_dev_intf->hw_ops.deinit = cam_ipe_deinit_hw;
	ipe_dev_intf->hw_ops.process_cmd = cam_ipe_process_cmd;
	ipe_dev_intf->hw_type = CAM_ICP_DEV_IPE;

	CAM_DBG(CAM_ICP, "IPE component bind type %d index %d",
		ipe_dev_intf->hw_type,
		ipe_dev_intf->hw_idx);

	platform_set_drvdata(pdev, ipe_dev_intf);

	ipe_dev->core_info = kzalloc(sizeof(struct cam_ipe_device_core_info),
		GFP_KERNEL);
	if (!ipe_dev->core_info) {
		kfree(ipe_dev);
		kfree(ipe_dev_intf);
		return -ENOMEM;
	}
	core_info = (struct cam_ipe_device_core_info *)ipe_dev->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_DBG(CAM_ICP, "No ipe hardware info");
		kfree(ipe_dev->core_info);
		kfree(ipe_dev);
		kfree(ipe_dev_intf);
		rc = -EINVAL;
		return rc;
	}
	hw_info = (struct cam_ipe_device_hw_info *)match_dev->data;
	core_info->ipe_hw_info = hw_info;

	rc = cam_ipe_init_soc_resources(&ipe_dev->soc_info, cam_ipe_irq,
		ipe_dev);
	if (rc < 0) {
		CAM_ERR(CAM_ICP, "failed to init_soc");
		kfree(ipe_dev->core_info);
		kfree(ipe_dev);
		kfree(ipe_dev_intf);
		return rc;
	}

	CAM_DBG(CAM_ICP, "cam_ipe_init_soc_resources : %pK",
		(void *)&ipe_dev->soc_info);
	rc = cam_ipe_register_cpas(&ipe_dev->soc_info,
		core_info, ipe_dev_intf->hw_idx);
	if (rc < 0) {
		kfree(ipe_dev->core_info);
		kfree(ipe_dev);
		kfree(ipe_dev_intf);
		return rc;
	}
	ipe_dev->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&ipe_dev->hw_mutex);
	spin_lock_init(&ipe_dev->hw_lock);
	init_completion(&ipe_dev->hw_complete);

	CAM_DBG(CAM_ICP, "IPE:%d component bound successfully",
		ipe_dev_intf->hw_idx);

	return rc;
}

static void cam_ipe_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info            *ipe_dev = NULL;
	struct cam_hw_intf            *ipe_dev_intf = NULL;
	struct cam_ipe_device_core_info   *core_info = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	CAM_DBG(CAM_ICP, "Unbinding component: %s", pdev->name);
	ipe_dev_intf = platform_get_drvdata(pdev);
	ipe_dev = ipe_dev_intf->hw_priv;
	core_info = (struct cam_ipe_device_core_info *)ipe_dev->core_info;
	cam_cpas_unregister_client(core_info->cpas_handle);
	cam_ipe_deinit_soc_resources(&ipe_dev->soc_info);
	kfree(ipe_dev->core_info);
	kfree(ipe_dev);
	kfree(ipe_dev_intf);
}


const static struct component_ops cam_ipe_component_ops = {
	.bind = cam_ipe_component_bind,
	.unbind = cam_ipe_component_unbind,
};

int cam_ipe_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_ICP, "Adding IPE component");
	rc = component_add(&pdev->dev, &cam_ipe_component_ops);
	if (rc)
		CAM_ERR(CAM_ICP, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_ipe_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_ipe_component_ops);
	return 0;
}

static const struct of_device_id cam_ipe_dt_match[] = {
	{
		.compatible = "qcom,cam-ipe",
		.data = &cam_ipe_hw_info,
	},
	{
		.compatible = "qcom,cam-ipe680",
		.data = &cam_ipe680_hw_info,
	},
	{}
};
MODULE_DEVICE_TABLE(of, cam_ipe_dt_match);

struct platform_driver cam_ipe_driver = {
	.probe = cam_ipe_probe,
	.remove = cam_ipe_remove,
	.driver = {
		.name = "cam-ipe",
		.owner = THIS_MODULE,
		.of_match_table = cam_ipe_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_ipe_init_module(void)
{
	return platform_driver_register(&cam_ipe_driver);
}

void cam_ipe_exit_module(void)
{
	platform_driver_unregister(&cam_ipe_driver);
}

MODULE_DESCRIPTION("CAM IPE driver");
MODULE_LICENSE("GPL v2");
