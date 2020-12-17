// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/mod_devicetable.h>
#include <linux/of_device.h>
#include "cam_top_tpg_core.h"
#include "cam_top_tpg_dev.h"
#include "cam_top_tpg_hw_intf.h"
#include "cam_debug_util.h"

static struct cam_hw_intf *cam_top_tpg_hw_list[CAM_TOP_TPG_HW_NUM_MAX] = {
	0, 0};

static char tpg_dev_name[8];

int cam_top_tpg_probe(struct platform_device *pdev)
{

	struct cam_hw_intf             *tpg_hw_intf;
	struct cam_hw_info             *tpg_hw_info;
	struct cam_top_tpg_hw          *tpg_dev = NULL;
	const struct of_device_id      *match_dev = NULL;
	struct cam_top_tpg_hw_info     *tpg_hw_data = NULL;
	uint32_t                        tpg_dev_idx;
	int                             rc = 0;

	CAM_DBG(CAM_ISP, "probe called");

	tpg_hw_intf = kzalloc(sizeof(*tpg_hw_intf), GFP_KERNEL);
	if (!tpg_hw_intf) {
		rc = -ENOMEM;
		goto err;
	}

	tpg_hw_info = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!tpg_hw_info) {
		rc = -ENOMEM;
		goto free_hw_intf;
	}

	tpg_dev = kzalloc(sizeof(struct cam_top_tpg_hw), GFP_KERNEL);
	if (!tpg_dev) {
		rc = -ENOMEM;
		goto free_hw_info;
	}

	/* get top tpg hw index */
	of_property_read_u32(pdev->dev.of_node, "cell-index", &tpg_dev_idx);
	/* get top tpg hw information */
	match_dev = of_match_device(pdev->dev.driver->of_match_table,
		&pdev->dev);
	if (!match_dev) {
		CAM_ERR(CAM_ISP, "No matching table for the top tpg hw");
		rc = -EINVAL;
		goto free_dev;
	}

	memset(tpg_dev_name, 0, sizeof(tpg_dev_name));
	snprintf(tpg_dev_name, sizeof(tpg_dev_name),
		"tpg%1u", tpg_dev_idx);

	tpg_hw_intf->hw_idx = tpg_dev_idx;
	tpg_hw_intf->hw_type = CAM_ISP_HW_TYPE_TPG;
	tpg_hw_intf->hw_priv = tpg_hw_info;

	tpg_hw_info->core_info = tpg_dev;
	tpg_hw_info->soc_info.pdev = pdev;
	tpg_hw_info->soc_info.dev = &pdev->dev;
	tpg_hw_info->soc_info.dev_name = tpg_dev_name;
	tpg_hw_info->soc_info.index = tpg_dev_idx;

	tpg_hw_data = (struct cam_top_tpg_hw_info  *)match_dev->data;
	/* need to setup the pdev before call the tfe hw probe init */
	tpg_dev->tpg_info = tpg_hw_data;

	rc = cam_top_tpg_hw_probe_init(tpg_hw_intf, tpg_dev_idx);
	if (rc)
		goto free_dev;

	platform_set_drvdata(pdev, tpg_dev);
	CAM_DBG(CAM_ISP, "TPG:%d probe successful",
		tpg_hw_intf->hw_idx);


	if (tpg_hw_intf->hw_idx < CAM_TOP_TPG_HW_NUM_MAX)
		cam_top_tpg_hw_list[tpg_hw_intf->hw_idx] = tpg_hw_intf;
	else
		goto free_dev;

	return 0;

free_dev:
	kfree(tpg_dev);
free_hw_info:
	kfree(tpg_hw_info);
free_hw_intf:
	kfree(tpg_hw_intf);
err:
	return rc;
}

int cam_top_tpg_remove(struct platform_device *pdev)
{
	struct cam_top_tpg_hw          *tpg_dev = NULL;
	struct cam_hw_intf             *tpg_hw_intf;
	struct cam_hw_info             *tpg_hw_info;

	tpg_dev = (struct cam_top_tpg_hw *)platform_get_drvdata(pdev);
	tpg_hw_intf = tpg_dev->hw_intf;
	tpg_hw_info = tpg_dev->hw_info;

	CAM_DBG(CAM_ISP, "TPG:%d remove",
		tpg_dev->hw_intf->hw_idx);

	cam_top_tpg_hw_deinit(tpg_dev);

	/*release the tpg device memory */
	kfree(tpg_dev);
	kfree(tpg_hw_info);
	kfree(tpg_hw_intf);
	return 0;
}

int cam_top_tpg_hw_init(struct cam_hw_intf **top_tpg_hw,
	uint32_t hw_idx)
{
	int rc = 0;

	if (cam_top_tpg_hw_list[hw_idx]) {
		*top_tpg_hw = cam_top_tpg_hw_list[hw_idx];
	} else {
		*top_tpg_hw = NULL;
		rc = -1;
	}

	return rc;
}
