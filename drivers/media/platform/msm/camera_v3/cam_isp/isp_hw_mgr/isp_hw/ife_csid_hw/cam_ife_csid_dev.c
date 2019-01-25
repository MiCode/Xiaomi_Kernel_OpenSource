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

#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include "cam_ife_csid_core.h"
#include "cam_ife_csid_dev.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_debug_util.h"

static struct cam_hw_intf *cam_ife_csid_hw_list[CAM_IFE_CSID_HW_RES_MAX] = {
	0, 0, 0, 0};

static char csid_dev_name[8];

int cam_ife_csid_probe(struct platform_device *pdev)
{

	struct cam_hw_intf             *csid_hw_intf;
	struct cam_hw_info             *csid_hw_info;
	struct cam_ife_csid_hw         *csid_dev = NULL;
	const struct of_device_id      *match_dev = NULL;
	struct cam_ife_csid_hw_info    *csid_hw_data = NULL;
	uint32_t                        csid_dev_idx;
	int                             rc = 0;

	CAM_DBG(CAM_ISP, "probe called");

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

	csid_dev = kzalloc(sizeof(struct cam_ife_csid_hw), GFP_KERNEL);
	if (!csid_dev) {
		rc = -ENOMEM;
		goto free_hw_info;
	}

	/* get ife csid hw index */
	of_property_read_u32(pdev->dev.of_node, "cell-index", &csid_dev_idx);
	/* get ife csid hw information */
	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "No matching table for the IFE CSID HW!");
		rc = -EINVAL;
		goto free_dev;
	}

	memset(csid_dev_name, 0, sizeof(csid_dev_name));
	snprintf(csid_dev_name, sizeof(csid_dev_name),
		"csid%1u", csid_dev_idx);

	csid_hw_intf->hw_idx = csid_dev_idx;
	csid_hw_intf->hw_type = CAM_ISP_HW_TYPE_IFE_CSID;
	csid_hw_intf->hw_priv = csid_hw_info;

	csid_hw_info->core_info = csid_dev;
	csid_hw_info->soc_info.pdev = pdev;
	csid_hw_info->soc_info.dev = &pdev->dev;
	csid_hw_info->soc_info.dev_name = csid_dev_name;
	csid_hw_info->soc_info.index = csid_dev_idx;

	csid_hw_data = (struct cam_ife_csid_hw_info  *)match_dev->data;
	/* need to setup the pdev before call the ife hw probe init */
	csid_dev->csid_info = csid_hw_data;

	rc = cam_ife_csid_hw_probe_init(csid_hw_intf, csid_dev_idx);
	if (rc)
		goto free_dev;

	platform_set_drvdata(pdev, csid_dev);
	CAM_DBG(CAM_ISP, "CSID:%d probe successful",
		csid_hw_intf->hw_idx);


	if (csid_hw_intf->hw_idx < CAM_IFE_CSID_HW_RES_MAX)
		cam_ife_csid_hw_list[csid_hw_intf->hw_idx] = csid_hw_intf;
	else
		goto free_dev;

	return 0;

free_dev:
	kfree(csid_dev);
free_hw_info:
	kfree(csid_hw_info);
free_hw_intf:
	kfree(csid_hw_intf);
err:
	return rc;
}

int cam_ife_csid_remove(struct platform_device *pdev)
{
	struct cam_ife_csid_hw         *csid_dev = NULL;
	struct cam_hw_intf             *csid_hw_intf;
	struct cam_hw_info             *csid_hw_info;

	csid_dev = (struct cam_ife_csid_hw *)platform_get_drvdata(pdev);
	csid_hw_intf = csid_dev->hw_intf;
	csid_hw_info = csid_dev->hw_info;

	CAM_DBG(CAM_ISP, "CSID:%d remove",
		csid_dev->hw_intf->hw_idx);

	cam_ife_csid_hw_deinit(csid_dev);

	/*release the csid device memory */
	kfree(csid_dev);
	kfree(csid_hw_info);
	kfree(csid_hw_intf);
	return 0;
}

int cam_ife_csid_hw_init(struct cam_hw_intf **ife_csid_hw,
	uint32_t hw_idx)
{
	int rc = 0;

	if (cam_ife_csid_hw_list[hw_idx]) {
		*ife_csid_hw = cam_ife_csid_hw_list[hw_idx];
	} else {
		*ife_csid_hw = NULL;
		rc = -1;
	}

	return rc;
}
