/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include "cam_isp_hw.h"
#include "cam_hw_intf.h"
#include "cam_csid_ppi_core.h"
#include "cam_csid_ppi_dev.h"
#include "cam_debug_util.h"

static struct cam_hw_intf *cam_csid_ppi_hw_list[CAM_CSID_PPI_HW_MAX] = {
	0, 0, 0, 0};
static char ppi_dev_name[8];

int cam_csid_ppi_probe(struct platform_device *pdev)
{
	struct cam_hw_intf            *ppi_hw_intf;
	struct cam_hw_info            *ppi_hw_info;
	struct cam_csid_ppi_hw        *ppi_dev = NULL;
	const struct of_device_id     *match_dev = NULL;
	struct cam_csid_ppi_hw_info   *ppi_hw_data = NULL;
	uint32_t                       ppi_dev_idx;
	int                            rc = 0;

	CAM_DBG(CAM_ISP, "PPI probe called");

	ppi_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!ppi_hw_intf) {
		rc = -ENOMEM;
		goto err;
	}

	ppi_hw_info = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!ppi_hw_info) {
		rc = -ENOMEM;
		goto free_hw_intf;
	}

	ppi_dev = kzalloc(sizeof(struct cam_csid_ppi_hw), GFP_KERNEL);
	if (!ppi_dev) {
		rc = -ENOMEM;
		goto free_hw_info;
	}

	/* get csid ppi hw index */
	of_property_read_u32(pdev->dev.of_node, "cell-index", &ppi_dev_idx);

	/* get csid ppi hw information */
	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "No matching table for the CSID PPI HW!");
		rc = -EINVAL;
		goto free_dev;
	}

	memset(ppi_dev_name, 0, sizeof(ppi_dev_name));
	snprintf(ppi_dev_name, sizeof(ppi_dev_name), "ppi%1u", ppi_dev_idx);

	ppi_hw_intf->hw_idx  = ppi_dev_idx;
	ppi_hw_intf->hw_priv = ppi_hw_info;

	ppi_hw_info->core_info         = ppi_dev;
	ppi_hw_info->soc_info.pdev     = pdev;
	ppi_hw_info->soc_info.dev      = &pdev->dev;
	ppi_hw_info->soc_info.dev_name = ppi_dev_name;
	ppi_hw_info->soc_info.index    = ppi_dev_idx;

	ppi_hw_data = (struct cam_csid_ppi_hw_info  *)match_dev->data;
	/* need to setup the pdev before call the csid ppi hw probe init */
	ppi_dev->ppi_info = ppi_hw_data;

	rc = cam_csid_ppi_hw_probe_init(ppi_hw_intf, ppi_dev_idx);
	if (rc) {
		CAM_ERR(CAM_ISP, "PPI: Probe init failed!");
		goto free_dev;
	}

	platform_set_drvdata(pdev, ppi_dev);
	CAM_DBG(CAM_ISP, "PPI:%d probe successful",
		ppi_hw_intf->hw_idx);

	if (ppi_hw_intf->hw_idx < CAM_CSID_PPI_HW_MAX)
		cam_csid_ppi_hw_list[ppi_hw_intf->hw_idx] = ppi_hw_intf;
	else
		goto free_dev;

	return 0;
free_dev:
	kfree(ppi_dev);
free_hw_info:
	kfree(ppi_hw_info);
free_hw_intf:
	kfree(ppi_hw_intf);
err:
	return rc;
}

int cam_csid_ppi_remove(struct platform_device *pdev)
{
	struct cam_csid_ppi_hw         *ppi_dev = NULL;
	struct cam_hw_intf             *ppi_hw_intf;
	struct cam_hw_info             *ppi_hw_info;

	ppi_dev = (struct cam_csid_ppi_hw *)platform_get_drvdata(pdev);
	ppi_hw_intf = ppi_dev->hw_intf;
	ppi_hw_info = ppi_dev->hw_info;

	CAM_DBG(CAM_ISP, "PPI:%d remove", ppi_dev->hw_intf->hw_idx);

	cam_csid_ppi_hw_deinit(ppi_dev);

	/* release the ppi device memory */
	kfree(ppi_dev);
	kfree(ppi_hw_info);
	kfree(ppi_hw_intf);
	return 0;
}

int cam_csid_ppi_hw_init(struct cam_hw_intf **csid_ppi_hw,
	uint32_t hw_idx)
{
	int rc = 0;

	if (cam_csid_ppi_hw_list[hw_idx]) {
		*csid_ppi_hw = cam_csid_ppi_hw_list[hw_idx];
	} else {
		*csid_ppi_hw = NULL;
		rc = -1;
	}

	return rc;
}
EXPORT_SYMBOL(cam_csid_ppi_hw_init);

