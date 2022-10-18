// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include "cam_ife_csid_common.h"
#include "cam_ife_csid_dev.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_debug_util.h"
#include "camera_main.h"
#include "cam_cpas_api.h"
#include <dt-bindings/msm-camera.h>

static struct cam_hw_intf *cam_ife_csid_hw_list[CAM_IFE_CSID_HW_NUM_MAX] = {
	0, 0, 0, 0};

static int cam_ife_csid_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_intf             *hw_intf;
	struct cam_hw_info             *hw_info;
	const struct of_device_id      *match_dev = NULL;
	struct cam_ife_csid_core_info  *csid_core_info = NULL;
	uint32_t                        csid_dev_idx;
	int                             rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	CAM_DBG(CAM_ISP, "Binding IFE CSID component");

	hw_intf = kzalloc(sizeof(*hw_intf), GFP_KERNEL);
	if (!hw_intf) {
		rc = -ENOMEM;
		goto err;
	}

	hw_info = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!hw_info) {
		rc = -ENOMEM;
		goto free_hw_intf;
	}

	/* get ife csid hw index */
	of_property_read_u32(pdev->dev.of_node, "cell-index", &csid_dev_idx);
	/* get ife csid hw information */
	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "No matching table for the IFE CSID HW!");
		rc = -EINVAL;
		goto free_hw_info;
	}

	hw_intf->hw_idx = csid_dev_idx;
	hw_intf->hw_type = CAM_ISP_HW_TYPE_IFE_CSID;
	hw_intf->hw_priv = hw_info;

	hw_info->soc_info.pdev = pdev;
	hw_info->soc_info.dev = &pdev->dev;
	hw_info->soc_info.dev_name = pdev->name;
	hw_info->soc_info.index = csid_dev_idx;

	csid_core_info = (struct cam_ife_csid_core_info  *)match_dev->data;

	if (!cam_cpas_is_feature_supported(CAM_CPAS_ISP_FUSE,
		(1 << hw_intf->hw_idx), NULL) ||
		!cam_cpas_is_feature_supported(CAM_CPAS_ISP_LITE_FUSE,
		(1 << hw_intf->hw_idx), NULL)) {
		CAM_DBG(CAM_ISP, "CSID[%d] not supported based on fuse",
			csid_dev_idx);
		goto free_hw_info;
	}
	/* call the driver init and fill csid_hw_info->core_info */
	rc = cam_ife_csid_hw_probe_init(hw_intf, csid_core_info, false);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] probe init failed",
		    csid_dev_idx);
		goto free_hw_info;
	}

	platform_set_drvdata(pdev, hw_intf);
	CAM_DBG(CAM_ISP, "CSID:%d component bound successfully",
		hw_intf->hw_idx);


	if (hw_intf->hw_idx < CAM_IFE_CSID_HW_NUM_MAX)
		cam_ife_csid_hw_list[hw_intf->hw_idx] = hw_intf;
	else
		goto free_hw_info;

	return 0;

free_hw_info:
	kfree(hw_info);
free_hw_intf:
	kfree(hw_intf);
err:
	return rc;
}

static void cam_ife_csid_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_intf             *hw_intf;
	struct cam_hw_info             *hw_info;
	struct cam_ife_csid_core_info  *core_info = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	const struct of_device_id      *match_dev = NULL;

	hw_intf = (struct cam_hw_intf *)platform_get_drvdata(pdev);
	hw_info = hw_intf->hw_priv;
	core_info = hw_info->core_info;

	CAM_DBG(CAM_ISP, "CSID:%d component unbind",
		hw_intf->hw_idx);
	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);

	if (!match_dev) {
		CAM_ERR(CAM_ISP, "No matching table for the IFE CSID HW!");
		goto free_mem;
	}

	cam_ife_csid_hw_deinit(hw_intf, core_info);

free_mem:
	/*release the csid device memory */
	kfree(hw_info);
	kfree(hw_intf);
}

const static struct component_ops cam_ife_csid_component_ops = {
	.bind = cam_ife_csid_component_bind,
	.unbind = cam_ife_csid_component_unbind,
};

int cam_ife_csid_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_ISP, "Adding IFE CSID component");
	rc = component_add(&pdev->dev, &cam_ife_csid_component_ops);
	if (rc)
		CAM_ERR(CAM_ISP, "failed to add component rc: %d", rc);

	return rc;
}

int cam_ife_csid_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_ife_csid_component_ops);
	return 0;
}

int cam_ife_csid_hw_init(struct cam_hw_intf **ife_csid_hw,
	uint32_t hw_idx)
{
	int rc = 0;

	if (hw_idx >= CAM_IFE_CSID_HW_NUM_MAX) {
		*ife_csid_hw = NULL;
		return -EINVAL;
	}

	if (cam_ife_csid_hw_list[hw_idx]) {
		*ife_csid_hw = cam_ife_csid_hw_list[hw_idx];
	} else {
		*ife_csid_hw = NULL;
		rc = -1;
	}

	return rc;
}
