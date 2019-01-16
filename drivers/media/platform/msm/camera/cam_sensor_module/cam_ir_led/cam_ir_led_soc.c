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

#include <linux/of.h>
#include <linux/of_gpio.h>
#include "cam_ir_led_soc.h"
#include "cam_res_mgr_api.h"

int cam_ir_led_get_dt_data(struct cam_ir_led_ctrl *ictrl,
	struct cam_hw_soc_info *soc_info)
{
	int32_t rc = 0;

	if (!ictrl) {
		CAM_ERR(CAM_IR_LED, "NULL ir_led control structure");
		return -EINVAL;
	}

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_IR_LED, "get_dt_properties failed rc %d", rc);
		return rc;
	}

	soc_info->soc_private =
		kzalloc(sizeof(struct cam_ir_led_private_soc), GFP_KERNEL);
	if (!soc_info->soc_private) {
		rc = -ENOMEM;
		goto release_soc_res;
	}

	return rc;

release_soc_res:
	cam_soc_util_release_platform_resource(soc_info);
	return rc;
}
