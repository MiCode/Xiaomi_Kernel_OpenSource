/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "cam_ife_csid_soc.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

static int cam_ife_csid_get_dt_properties(struct cam_hw_soc_info *soc_info)
{
	struct device_node *of_node = NULL;
	struct csid_device_soc_info *csid_soc_info = NULL;
	int rc = 0;

	of_node = soc_info->pdev->dev.of_node;
	csid_soc_info = (struct csid_device_soc_info *)soc_info->soc_private;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc)
		return rc;

	return rc;
}

static int cam_ife_csid_request_platform_resource(
	struct cam_hw_soc_info *soc_info,
	irq_handler_t csid_irq_handler,
	void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_request_platform_resource(soc_info, csid_irq_handler,
		irq_data);
	if (rc)
		return rc;

	return rc;
}

int cam_ife_csid_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t csid_irq_handler, void *irq_data)
{
	int rc = 0;

	rc = cam_ife_csid_get_dt_properties(soc_info);
	if (rc < 0)
		return rc;

	/* Need to see if we want post process the clock list */

	rc = cam_ife_csid_request_platform_resource(soc_info, csid_irq_handler,
		irq_data);
	if (rc < 0)
		return rc;

	CDBG("%s: mem_base is 0x%llx\n", __func__,
		(uint64_t) soc_info->reg_map[0].mem_base);

	return rc;
}

int cam_ife_csid_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_enable_platform_resource(soc_info, true, true);
	if (rc) {
		pr_err("%s: enable platform failed\n", __func__);
		return rc;
	}

	return rc;
}

int cam_ife_csid_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc)
		pr_err("%s: Disable platform failed\n", __func__);

	return rc;
}

