// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include "cam_tfe_dev.h"
#include "cam_tfe_core.h"
#include "cam_tfe_soc.h"
#include "cam_debug_util.h"
#include "camera_main.h"

static struct cam_isp_hw_intf_data  cam_tfe_hw_list[CAM_TFE_HW_NUM_MAX];

static char tfe_dev_name[8];

static int cam_tfe_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info                *tfe_hw = NULL;
	struct cam_hw_intf                *tfe_hw_intf = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct cam_tfe_hw_core_info       *core_info = NULL;
	struct cam_tfe_hw_info            *hw_info = NULL;
	struct cam_tfe_soc_private        *tfe_soc_priv;
	int                                rc = 0;
	struct platform_device *pdev = to_platform_device(dev);
	uint32_t  i;

	tfe_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!tfe_hw_intf) {
		rc = -ENOMEM;
		goto end;
	}

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &tfe_hw_intf->hw_idx);

	tfe_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!tfe_hw) {
		rc = -ENOMEM;
		goto free_tfe_hw_intf;
	}

	memset(tfe_dev_name, 0, sizeof(tfe_dev_name));
	snprintf(tfe_dev_name, sizeof(tfe_dev_name),
		"tfe%1u", tfe_hw_intf->hw_idx);

	tfe_hw->soc_info.pdev = pdev;
	tfe_hw->soc_info.dev = &pdev->dev;
	tfe_hw->soc_info.dev_name = tfe_dev_name;
	tfe_hw_intf->hw_priv = tfe_hw;
	tfe_hw_intf->hw_ops.get_hw_caps = cam_tfe_get_hw_caps;
	tfe_hw_intf->hw_ops.init = cam_tfe_init_hw;
	tfe_hw_intf->hw_ops.deinit = cam_tfe_deinit_hw;
	tfe_hw_intf->hw_ops.reset = cam_tfe_reset;
	tfe_hw_intf->hw_ops.reserve = cam_tfe_reserve;
	tfe_hw_intf->hw_ops.release = cam_tfe_release;
	tfe_hw_intf->hw_ops.start = cam_tfe_start;
	tfe_hw_intf->hw_ops.stop = cam_tfe_stop;
	tfe_hw_intf->hw_ops.read = cam_tfe_read;
	tfe_hw_intf->hw_ops.write = cam_tfe_write;
	tfe_hw_intf->hw_ops.process_cmd = cam_tfe_process_cmd;
	tfe_hw_intf->hw_type = CAM_ISP_HW_TYPE_TFE;

	CAM_DBG(CAM_ISP, "TFE component bind,type %d index %d",
		tfe_hw_intf->hw_type, tfe_hw_intf->hw_idx);

	platform_set_drvdata(pdev, tfe_hw_intf);

	tfe_hw->core_info = kzalloc(sizeof(struct cam_tfe_hw_core_info),
		GFP_KERNEL);
	if (!tfe_hw->core_info) {
		CAM_DBG(CAM_ISP, "Failed to alloc for core");
		rc = -ENOMEM;
		goto free_tfe_hw;
	}
	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "Of_match Failed");
		rc = -EINVAL;
		goto free_core_info;
	}
	hw_info = (struct cam_tfe_hw_info *)match_dev->data;
	core_info->tfe_hw_info = hw_info;
	core_info->core_index = tfe_hw_intf->hw_idx;

	rc = cam_tfe_init_soc_resources(&tfe_hw->soc_info, cam_tfe_irq,
		tfe_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Failed to init soc rc=%d", rc);
		goto free_core_info;
	}

	rc = cam_tfe_core_init(core_info, &tfe_hw->soc_info,
		tfe_hw_intf, hw_info);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Failed to init core rc=%d", rc);
		goto deinit_soc;
	}

	tfe_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&tfe_hw->hw_mutex);
	spin_lock_init(&tfe_hw->hw_lock);
	init_completion(&tfe_hw->hw_complete);

	if (tfe_hw_intf->hw_idx < CAM_TFE_HW_NUM_MAX)
		cam_tfe_hw_list[tfe_hw_intf->hw_idx].hw_intf = tfe_hw_intf;
	else {
		CAM_ERR(CAM_ISP, "HW index:%d is wrong max HW idx:%d",
			tfe_hw_intf->hw_idx, CAM_TFE_HW_NUM_MAX);
		goto deinit_soc;
	}

	tfe_soc_priv = tfe_hw->soc_info.soc_private;
	cam_tfe_hw_list[tfe_hw_intf->hw_idx].num_hw_pid = tfe_soc_priv->num_pid;
	for (i = 0; i < tfe_soc_priv->num_pid; i++)
		cam_tfe_hw_list[tfe_hw_intf->hw_idx].hw_pid[i] =
			tfe_soc_priv->pid[i];

	cam_tfe_init_hw(tfe_hw, NULL, 0);
	cam_tfe_deinit_hw(tfe_hw, NULL, 0);

	CAM_DBG(CAM_ISP, "TFE:%d component bound successfully",
		tfe_hw_intf->hw_idx);
	return rc;

deinit_soc:
	if (cam_tfe_deinit_soc_resources(&tfe_hw->soc_info))
		CAM_ERR(CAM_ISP, "Failed to deinit soc");
free_core_info:
	kfree(tfe_hw->core_info);
free_tfe_hw:
	kfree(tfe_hw);
free_tfe_hw_intf:
	kfree(tfe_hw_intf);
end:
	return rc;
}

static void cam_tfe_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_info		  *tfe_hw = NULL;
	struct cam_hw_intf		  *tfe_hw_intf = NULL;
	struct cam_tfe_hw_core_info	  *core_info = NULL;
	int				   rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	tfe_hw_intf = platform_get_drvdata(pdev);
	if (!tfe_hw_intf) {
		CAM_ERR(CAM_ISP, "Error! No data in pdev");
		return;
	}

	CAM_DBG(CAM_ISP, "type %d index %d",
		tfe_hw_intf->hw_type, tfe_hw_intf->hw_idx);

	if (tfe_hw_intf->hw_idx < CAM_TFE_HW_NUM_MAX)
		cam_tfe_hw_list[tfe_hw_intf->hw_idx].hw_intf = NULL;

	tfe_hw = tfe_hw_intf->hw_priv;
	if (!tfe_hw) {
		CAM_ERR(CAM_ISP, "Error! HW data is NULL");
		goto free_tfe_hw_intf;
	}

	core_info = (struct cam_tfe_hw_core_info *)tfe_hw->core_info;
	if (!core_info) {
		CAM_ERR(CAM_ISP, "Error! core data NULL");
		goto deinit_soc;
	}

	rc = cam_tfe_core_deinit(core_info, core_info->tfe_hw_info);
	if (rc < 0)
		CAM_ERR(CAM_ISP, "Failed to deinit core rc=%d", rc);

	kfree(tfe_hw->core_info);

deinit_soc:
	rc = cam_tfe_deinit_soc_resources(&tfe_hw->soc_info);
	if (rc < 0)
		CAM_ERR(CAM_ISP, "Failed to deinit soc rc=%d", rc);

	mutex_destroy(&tfe_hw->hw_mutex);
	kfree(tfe_hw);

	CAM_DBG(CAM_ISP, "TFE%d component unbound", tfe_hw_intf->hw_idx);

free_tfe_hw_intf:
	kfree(tfe_hw_intf);
}

const static struct component_ops cam_tfe_component_ops = {
	.bind = cam_tfe_component_bind,
	.unbind = cam_tfe_component_unbind,
};

int cam_tfe_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "Adding TFE component");
	rc = component_add(&pdev->dev, &cam_tfe_component_ops);
	if (rc)
		CAM_ERR(CAM_ISP, "failed to add component rc: %d", rc);

	return rc;
}

int cam_tfe_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_tfe_component_ops);
	return 0;
}

int cam_tfe_hw_init(struct cam_isp_hw_intf_data **tfe_hw_intf,
	uint32_t hw_idx)
{
	int rc = 0;

	if (cam_tfe_hw_list[hw_idx].hw_intf) {
		*tfe_hw_intf = &cam_tfe_hw_list[hw_idx];
		rc = 0;
	} else {
		*tfe_hw_intf = NULL;
		rc = -ENODEV;
	}
	return rc;
}
