// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include "linux/module.h"
#include "cam_custom_csid_dev.h"
#include "cam_ife_csid_common.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "cam_custom_csid480.h"
#include "cam_debug_util.h"
#include "camera_main.h"

#define CAM_CUSTOM_CSID_DRV_NAME  "custom_csid"

static struct cam_hw_intf *cam_custom_csid_hw_list[CAM_IFE_CSID_HW_NUM_MAX] = {
	0, 0, 0, 0};

static struct cam_ife_csid_core_info cam_custom_csid480_hw_info = {
	.csid_reg = &cam_custom_csid_480_reg_offset,
	.sw_version  = CAM_IFE_CSID_VER_1_0,
};

static int cam_custom_csid_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_intf	       *csid_hw_intf;
	struct cam_hw_info	       *csid_hw_info;
	const struct of_device_id      *match_dev = NULL;
	struct cam_ife_csid_core_info  *csid_core_info = NULL;
	uint32_t			csid_dev_idx;
	int				rc = 0;
	struct platform_device *pdev = to_platform_device(dev);

	csid_hw_intf = kzalloc(sizeof(*csid_hw_intf), GFP_KERNEL);
	if (!csid_hw_intf) {
		rc = -ENOMEM;
		goto err;
	}

	csid_hw_info = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);

	if (!csid_hw_info) {
		rc = -ENOMEM;
		goto free_hw_intf;
	}

	/* get custom csid hw index */
	of_property_read_u32(pdev->dev.of_node, "cell-index", &csid_dev_idx);
	/* get custom csid hw information */
	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_CUSTOM,
			"No matching table for the CUSTOM CSID HW!");
		rc = -EINVAL;
		goto free_hw_info;
	}

	csid_hw_intf->hw_idx = csid_dev_idx;
	csid_hw_intf->hw_type = CAM_ISP_HW_TYPE_IFE_CSID;
	csid_hw_intf->hw_priv = csid_hw_info;

	csid_hw_info->soc_info.pdev = pdev;
	csid_hw_info->soc_info.dev = &pdev->dev;
	csid_hw_info->soc_info.dev_name = pdev->name;
	csid_hw_info->soc_info.index = csid_dev_idx;

	csid_core_info = (struct cam_ife_csid_core_info  *)match_dev->data;

	/* call the driver init and fill csid_hw_info->core_info */
	rc = cam_ife_csid_hw_probe_init(csid_hw_intf, csid_core_info, true);

	if (rc) {
		CAM_ERR(CAM_ISP, "CSID[%d] probe init failed",
		    csid_dev_idx);
		goto free_hw_info;
	}

	platform_set_drvdata(pdev, csid_hw_intf);
	CAM_DBG(CAM_ISP, "CSID:%d component bound successfully",
		csid_hw_intf->hw_idx);

	if (csid_hw_intf->hw_idx < CAM_IFE_CSID_HW_NUM_MAX)
		cam_custom_csid_hw_list[csid_hw_intf->hw_idx] = csid_hw_intf;
	else
		goto free_hw_info;

	CAM_DBG(CAM_CUSTOM, "CSID:%d component bound successfully",
		csid_hw_intf->hw_idx);

	return 0;

free_hw_info:
	kfree(csid_hw_info);
free_hw_intf:
	kfree(csid_hw_intf);
err:
	return rc;
}

static void cam_custom_csid_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	struct cam_hw_intf             *csid_hw_intf;
	struct cam_hw_info             *csid_hw_info;
	struct cam_ife_csid_core_info  *core_info = NULL;
	struct platform_device *pdev = to_platform_device(dev);
	const struct of_device_id      *match_dev = NULL;

	csid_hw_intf = (struct cam_hw_intf *)platform_get_drvdata(pdev);
	csid_hw_info = csid_hw_intf->hw_priv;
	core_info = csid_hw_info->core_info;

	CAM_DBG(CAM_CUSTOM, "CSID:%d component unbind",
		csid_hw_intf->hw_idx);

	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);

	if (!match_dev) {
		CAM_ERR(CAM_ISP, "No matching table for the IFE CSID HW!");
		goto free_mem;
	}

	cam_ife_csid_hw_deinit(csid_hw_intf, core_info);

free_mem:
	/*release the csid device memory */
	kfree(csid_hw_info);
	kfree(csid_hw_intf);
}

const static struct component_ops cam_custom_csid_component_ops = {
	.bind = cam_custom_csid_component_bind,
	.unbind = cam_custom_csid_component_unbind,
};

static int cam_custom_csid_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CUSTOM, "Adding Custom CSID component");
	rc = component_add(&pdev->dev, &cam_custom_csid_component_ops);
	if (rc)
		CAM_ERR(CAM_CUSTOM, "failed to add component rc: %d", rc);

	return rc;
}

static int cam_custom_csid_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_custom_csid_component_ops);
	return 0;
}

static const struct of_device_id cam_custom_csid_dt_match[] = {
	{
		.compatible = "qcom,csid-custom480",
		.data = &cam_custom_csid480_hw_info
	},
	{
		.compatible = "qcom,csid-custom580",
		.data = &cam_custom_csid480_hw_info
	},
	{},
};

MODULE_DEVICE_TABLE(of, cam_custom_csid_dt_match);

struct platform_driver cam_custom_csid_driver = {
	.probe = cam_custom_csid_probe,
	.driver = {
		.name = "qcom,custom-csid",
		.of_match_table = cam_custom_csid_dt_match,
		.suppress_bind_attrs = true,
	},
	.remove = cam_custom_csid_remove,
};

int cam_custom_csid_driver_init(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&cam_custom_csid_driver);
	if (rc < 0)
		CAM_ERR(CAM_CUSTOM, "platform_driver_register Failed: rc = %d",
			rc);

	return rc;
}

int cam_custom_csid_hw_init(struct cam_hw_intf **custom_csid_hw,
	uint32_t hw_idx)
{
	int rc = 0;

	if (cam_custom_csid_hw_list[hw_idx]) {
		*custom_csid_hw = cam_custom_csid_hw_list[hw_idx];
	} else {
		*custom_csid_hw = NULL;
		rc = -1;
	}

	return rc;
}

void cam_custom_csid_driver_exit(void)
{
	platform_driver_unregister(&cam_custom_csid_driver);
}

MODULE_DESCRIPTION("cam_custom_csid_driver");
MODULE_LICENSE("GPL v2");
