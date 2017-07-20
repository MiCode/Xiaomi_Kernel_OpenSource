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

#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#include <media/cam_defs.h>
#include <media/cam_icp.h>
#include "ipe_soc.h"
#include "cam_soc_util.h"
#include "cam_debug_util.h"

static int cam_ipe_get_dt_properties(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0)
		CAM_ERR(CAM_ICP, "get ipe dt prop is failed");

	return rc;
}

static int cam_ipe_request_platform_resource(
	struct cam_hw_soc_info *soc_info,
	irq_handler_t ipe_irq_handler, void *irq_data)
{
	int rc = 0;

	rc = cam_soc_util_request_platform_resource(soc_info, ipe_irq_handler,
		irq_data);

	return rc;
}

int cam_ipe_init_soc_resources(struct cam_hw_soc_info *soc_info,
	irq_handler_t ipe_irq_handler, void *irq_data)
{
	int rc = 0;

	rc = cam_ipe_get_dt_properties(soc_info);
	if (rc < 0)
		return rc;

	rc = cam_ipe_request_platform_resource(soc_info, ipe_irq_handler,
		irq_data);
	if (rc < 0)
		return rc;

	return rc;
}

int cam_ipe_enable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_TURBO_VOTE, false);
	if (rc) {
		CAM_ERR(CAM_ICP, "enable platform failed");
		return rc;
	}

	return rc;
}

int cam_ipe_disable_soc_resources(struct cam_hw_soc_info *soc_info)
{
	int rc = 0;

	rc = cam_soc_util_disable_platform_resource(soc_info, true, false);
	if (rc)
		CAM_ERR(CAM_ICP, "enable platform failed");

	return rc;
}
