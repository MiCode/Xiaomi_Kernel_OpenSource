/* Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
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


#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include "ais_vfe_dev.h"
#include "ais_vfe_core.h"
#include "ais_vfe_soc.h"
#include "cam_debug_util.h"

static struct cam_hw_intf *ais_vfe_hw_list[AIS_VFE_HW_NUM_MAX] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};

static char vfe_dev_name[8];

int ais_vfe_probe(struct platform_device *pdev)
{
	struct cam_hw_info                *vfe_hw = NULL;
	struct cam_hw_intf                *vfe_hw_intf = NULL;
	const struct of_device_id         *match_dev = NULL;
	struct ais_vfe_hw_core_info       *core_info = NULL;
	struct ais_vfe_hw_info            *hw_info = NULL;
	int                                rc = 0;

	CAM_INFO(CAM_ISP, "Probe called");

	vfe_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!vfe_hw_intf) {
		rc = -ENOMEM;
		goto end;
	}

	of_property_read_u32(pdev->dev.of_node,
		"cell-index", &vfe_hw_intf->hw_idx);

	vfe_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!vfe_hw) {
		rc = -ENOMEM;
		goto free_vfe_hw_intf;
	}

	memset(vfe_dev_name, 0, sizeof(vfe_dev_name));
	snprintf(vfe_dev_name, sizeof(vfe_dev_name),
		"vfe%1u", vfe_hw_intf->hw_idx);

	vfe_hw->soc_info.pdev = pdev;
	vfe_hw->soc_info.dev = &pdev->dev;
	vfe_hw->soc_info.dev_name = vfe_dev_name;
	vfe_hw_intf->hw_priv = vfe_hw;
	vfe_hw_intf->hw_ops.get_hw_caps = ais_vfe_get_hw_caps;
	vfe_hw_intf->hw_ops.init = ais_vfe_init_hw;
	vfe_hw_intf->hw_ops.deinit = ais_vfe_deinit_hw;
	vfe_hw_intf->hw_ops.reset = ais_vfe_force_reset;
	vfe_hw_intf->hw_ops.reserve = ais_vfe_reserve;
	vfe_hw_intf->hw_ops.release = ais_vfe_release;
	vfe_hw_intf->hw_ops.start = ais_vfe_start;
	vfe_hw_intf->hw_ops.stop = ais_vfe_stop;
	vfe_hw_intf->hw_ops.read = ais_vfe_read;
	vfe_hw_intf->hw_ops.write = ais_vfe_write;
	vfe_hw_intf->hw_ops.process_cmd = ais_vfe_process_cmd;
	vfe_hw_intf->hw_type = AIS_ISP_HW_TYPE_VFE;

	CAM_INFO(CAM_ISP, "Probe called for VFE%d", vfe_hw_intf->hw_idx);

	platform_set_drvdata(pdev, vfe_hw_intf);

	vfe_hw->core_info = kzalloc(sizeof(struct ais_vfe_hw_core_info),
		GFP_KERNEL);
	if (!vfe_hw->core_info) {
		CAM_DBG(CAM_ISP, "Failed to alloc for core");
		rc = -ENOMEM;
		goto free_vfe_hw;
	}
	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "Of_match Failed");
		rc = -EINVAL;
		goto free_core_info;
	}
	hw_info = (struct ais_vfe_hw_info *)match_dev->data;
	core_info->vfe_hw_info = hw_info;

	rc = ais_vfe_init_soc_resources(&vfe_hw->soc_info, ais_vfe_irq,
		vfe_hw);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Failed to init soc rc=%d", rc);
		goto free_core_info;
	}

	rc = ais_vfe_core_init(core_info, &vfe_hw->soc_info,
		vfe_hw_intf, hw_info);
	if (rc < 0) {
		CAM_ERR(CAM_ISP, "Failed to init core rc=%d", rc);
		goto deinit_soc;
	}

	vfe_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	mutex_init(&vfe_hw->hw_mutex);
	spin_lock_init(&vfe_hw->hw_lock);
	init_completion(&vfe_hw->hw_complete);

	if (vfe_hw_intf->hw_idx < AIS_VFE_HW_NUM_MAX)
		ais_vfe_hw_list[vfe_hw_intf->hw_idx] = vfe_hw_intf;

	/*@TODO: why do we need this if not checking for errors*/
	ais_vfe_init_hw(vfe_hw, NULL, 0);
	ais_vfe_deinit_hw(vfe_hw, NULL, 0);

	CAM_DBG(CAM_ISP, "VFE%d probe successful", vfe_hw_intf->hw_idx);

	return rc;

deinit_soc:
	if (ais_vfe_deinit_soc_resources(&vfe_hw->soc_info))
		CAM_ERR(CAM_ISP, "Failed to deinit soc");
free_core_info:
	kfree(vfe_hw->core_info);
free_vfe_hw:
	kfree(vfe_hw);
free_vfe_hw_intf:
	kfree(vfe_hw_intf);
end:
	return rc;
}

int ais_vfe_remove(struct platform_device *pdev)
{
	struct cam_hw_info                *vfe_hw = NULL;
	struct cam_hw_intf                *vfe_hw_intf = NULL;
	struct ais_vfe_hw_core_info       *core_info = NULL;
	int                                rc = 0;

	vfe_hw_intf = platform_get_drvdata(pdev);
	if (!vfe_hw_intf) {
		CAM_ERR(CAM_ISP, "Error! No data in pdev");
		return -EINVAL;
	}

	CAM_DBG(CAM_ISP, "type %d index %d",
		vfe_hw_intf->hw_type, vfe_hw_intf->hw_idx);

	if (vfe_hw_intf->hw_idx < AIS_VFE_HW_NUM_MAX)
		ais_vfe_hw_list[vfe_hw_intf->hw_idx] = NULL;

	vfe_hw = vfe_hw_intf->hw_priv;
	if (!vfe_hw) {
		CAM_ERR(CAM_ISP, "Error! HW data is NULL");
		rc = -ENODEV;
		goto free_vfe_hw_intf;
	}

	core_info = (struct ais_vfe_hw_core_info *)vfe_hw->core_info;
	if (!core_info) {
		CAM_ERR(CAM_ISP, "Error! core data NULL");
		rc = -EINVAL;
		goto deinit_soc;
	}

	rc = ais_vfe_core_deinit(core_info, core_info->vfe_hw_info);
	if (rc < 0)
		CAM_ERR(CAM_ISP, "Failed to deinit core rc=%d", rc);

	kfree(vfe_hw->core_info);

deinit_soc:
	rc = ais_vfe_deinit_soc_resources(&vfe_hw->soc_info);
	if (rc < 0)
		CAM_ERR(CAM_ISP, "Failed to deinit soc rc=%d", rc);

	mutex_destroy(&vfe_hw->hw_mutex);
	kfree(vfe_hw);

	CAM_DBG(CAM_ISP, "VFE%d remove successful", vfe_hw_intf->hw_idx);

free_vfe_hw_intf:
	kfree(vfe_hw_intf);

	return rc;
}

int ais_vfe_hw_init(struct cam_hw_intf **vfe_hw,
	struct ais_isp_hw_init_args *init,
	struct cam_hw_intf *csid_hw)
{
	int rc = 0;

	if (ais_vfe_hw_list[init->hw_idx]) {
		struct cam_hw_info          *vfe_hw_info = NULL;
		struct ais_vfe_hw_core_info *core_info = NULL;

		vfe_hw_info = ais_vfe_hw_list[init->hw_idx]->hw_priv;
		core_info =
			(struct ais_vfe_hw_core_info *)vfe_hw_info->core_info;

		core_info->csid_hw = csid_hw;

		core_info->event_cb = init->event_cb;
		core_info->event_cb_priv = init->event_cb_priv;
		core_info->iommu_hdl = init->iommu_hdl;
		core_info->iommu_hdl_secure = init->iommu_hdl_secure;

		*vfe_hw = ais_vfe_hw_list[init->hw_idx];
		rc = 0;
	} else {
		*vfe_hw = NULL;
		rc = -ENODEV;
	}
	return rc;
}
