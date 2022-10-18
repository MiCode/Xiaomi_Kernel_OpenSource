// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <media/cam_defs.h>
#include <media/cam_cre.h>

#include "cre_soc.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"

int cam_cre_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t cre_irq_handler, void *irq_data)
{
	int rc;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc)
		return rc;

	rc = cam_soc_util_request_platform_resource(soc_info,
		cre_irq_handler,
		irq_data);
	if (rc)
		CAM_ERR(CAM_CRE, "init soc failed %d", rc);

	return rc;
}

int cam_cre_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_SVS_VOTE, true);
	if (rc)
		CAM_ERR(CAM_CRE, "enable platform failed %d", rc);

	return rc;
}

int cam_cre_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		CAM_ERR(CAM_CRE, "disable platform failed %d", rc);

	return rc;
}

int cam_cre_update_clk_rate(struct cam_hw_soc_info *soc_info,
	uint32_t clk_rate)
{
	int32_t src_clk_idx;

	if (!soc_info) {
		CAM_ERR(CAM_CRE, "Invalid soc info");
		return -EINVAL;
	}

	src_clk_idx = soc_info->src_clk_idx;

	CAM_DBG(CAM_CRE, "clk_rate = %u src_clk_index = %d",
		clk_rate, src_clk_idx);
	if ((soc_info->clk_level_valid[CAM_TURBO_VOTE] == true) &&
		(soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx] != 0) &&
		(clk_rate > soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx])) {
		CAM_DBG(CAM_CRE, "clk_rate %d greater than max, reset to %d",
			clk_rate,
			soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx]);
		clk_rate = soc_info->clk_rate[CAM_TURBO_VOTE][src_clk_idx];
	}

	CAM_DBG(CAM_CRE, "clk_rate = %u src_clk_index = %d",
		clk_rate, src_clk_idx);
	return cam_soc_util_set_src_clk_rate(soc_info, clk_rate);
}
