// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#include <media/cam_defs.h>
#include <media/cam_ope.h>
#include "ope_soc.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"


static int cam_ope_get_dt_properties(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	struct platform_device *pdev = NULL;
	struct device_node *of_node = NULL;
	struct cam_ope_soc_private *ope_soc_info;

	if (!soc_info) {
		CAM_ERR(CAM_OPE, "soc_info is NULL");
		return -EINVAL;
	}

	pdev = soc_info->pdev;
	of_node = pdev->dev.of_node;
	ope_soc_info = soc_info->soc_private;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0)
		CAM_ERR(CAM_OPE, "get ope dt prop is failed: %d", rc);

	ope_soc_info->hfi_en = of_property_read_bool(of_node, "hfi_en");

	return rc;
}

static int cam_ope_request_platform_resource(
	struct cam_hw_soc_info *soc_info,
	irq_handler_t ope_irq_handler, void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_request_platform_resource(soc_info, ope_irq_handler,
		irq_data);

	return rc;
}

int cam_ope_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t ope_irq_handler, void *irq_data)
{
	struct cam_ope_soc_private  *soc_private;
	struct platform_device *pdev = NULL;
	int num_pid = 0, i = 0;
	int rc = 0;

	soc_private = kzalloc(sizeof(struct cam_ope_soc_private), GFP_KERNEL);
	if (!soc_private) {
		CAM_DBG(CAM_ISP, "Error! soc_private Alloc Failed");
			return -ENOMEM;
	}
	soc_info->soc_private = soc_private;

	rc = cam_ope_get_dt_properties(soc_info);
	if (rc < 0)
		return rc;

	rc = cam_ope_request_platform_resource(soc_info, ope_irq_handler,
		irq_data);
	if (rc < 0)
		return rc;

	soc_private->num_pid = 0;
	pdev = soc_info->pdev;
	num_pid = of_property_count_u32_elems(pdev->dev.of_node, "cam_hw_pid");
	CAM_DBG(CAM_OPE, "ope: %d pid count %d", soc_info->index, num_pid);

	if (num_pid <= 0  || num_pid > CAM_OPE_HW_MAX_NUM_PID)
		goto end;

	soc_private->num_pid  = num_pid;

	for (i = 0; i < num_pid; i++)
		of_property_read_u32_index(pdev->dev.of_node, "cam_hw_pid", i,
				&soc_private->pid[i]);
end:
	return rc;
}

int cam_ope_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_SVS_VOTE, true);
	if (rc) {
		CAM_ERR(CAM_OPE, "enable platform failed");
		return rc;
	}

	return rc;
}

int cam_ope_disable_soc_resources(struct cam_hw_soc_info *soc_info,
	bool disable_clk)
{
	int rc = 0;

	rc = cam_soc_util_disable_platform_resource(soc_info, disable_clk,
		true);
	if (rc)
		CAM_ERR(CAM_OPE, "enable platform failed");

	return rc;
}

int cam_ope_update_clk_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_rate)
{
	int32_t src_clk_idx;

	if (!soc_info) {
		CAM_ERR(CAM_OPE, "Invalid soc info");
		return -EINVAL;
	}

	src_clk_idx = soc_info->src_clk_idx;

	CAM_DBG(CAM_OPE, "clk_rate = %u src_clk_index = %d",
		clk_rate, src_clk_idx);
	if ((soc_info->clk_level_valid[CAM_TURBO_VOTE] == true) &&
		(soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx] != 0) &&
		(clk_rate > soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx])) {
		CAM_DBG(CAM_OPE, "clk_rate %d greater than max, reset to %d",
			clk_rate,
			soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx]);
		clk_rate = soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx];
	}

	CAM_DBG(CAM_OPE, "clk_rate = %u src_clk_index = %d",
		clk_rate, src_clk_idx);
	return cam_soc_util_set_src_clk_rate(soc_info, clk_rate);
}

int cam_ope_toggle_clk(struct cam_hw_soc_info *soc_info, bool clk_enable)
{
	int rc = 0;

	if (clk_enable)
		rc = cam_soc_util_clk_enable_default(soc_info, CAM_SVS_VOTE);
	else
		cam_soc_util_clk_disable_default(soc_info);

	return rc;
}
