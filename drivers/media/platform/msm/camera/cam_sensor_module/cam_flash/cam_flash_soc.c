/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include "cam_flash_soc.h"
#include "cam_res_mgr_api.h"

static int32_t cam_get_source_node_info(
	struct device_node *of_node,
	struct cam_flash_ctrl *fctrl,
	struct cam_flash_private_soc *soc_private)
{
	int32_t rc = 0;
	uint32_t count = 0, i = 0;
	struct device_node *flash_src_node = NULL;
	struct device_node *torch_src_node = NULL;
	struct device_node *switch_src_node = NULL;

	switch_src_node = of_parse_phandle(of_node, "switch-source", 0);
	if (!switch_src_node) {
		CAM_DBG(CAM_FLASH, "switch_src_node NULL");
	} else {
		rc = of_property_read_string(switch_src_node,
			"qcom,default-led-trigger",
			&soc_private->switch_trigger_name);
		if (rc) {
			CAM_ERR(CAM_FLASH,
				"default-led-trigger read failed rc=%d", rc);
		} else {
			CAM_DBG(CAM_FLASH, "switch trigger %s",
				soc_private->switch_trigger_name);
			cam_res_mgr_led_trigger_register(
				soc_private->switch_trigger_name,
				&fctrl->switch_trigger);
		}

		of_node_put(switch_src_node);
	}

	if (of_get_property(of_node, "flash-source", &count)) {
		count /= sizeof(uint32_t);

		if (count > CAM_FLASH_MAX_LED_TRIGGERS) {
			CAM_ERR(CAM_FLASH, "Invalid LED count: %d", count);
			return -EINVAL;
		}

		fctrl->flash_num_sources = count;

		for (i = 0; i < count; i++) {
			flash_src_node = of_parse_phandle(of_node,
				"flash-source", i);
			if (!flash_src_node) {
				CAM_WARN(CAM_FLASH, "flash_src_node NULL");
				continue;
			}

			rc = of_property_read_string(flash_src_node,
				"qcom,default-led-trigger",
				&soc_private->flash_trigger_name[i]);
			if (rc) {
				CAM_WARN(CAM_FLASH,
				"defalut-led-trigger read failed rc=%d", rc);
				of_node_put(flash_src_node);
				continue;
			}

			CAM_DBG(CAM_FLASH, "default trigger %s",
				soc_private->flash_trigger_name[i]);

			/* Read operational-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,current-ma",
				&soc_private->flash_op_current[i]);
			if (rc) {
				CAM_WARN(CAM_FLASH, "op-current: read failed");
				of_node_put(flash_src_node);
				continue;
			}

			/* Read max-current */
			rc = of_property_read_u32(flash_src_node,
				"qcom,max-current",
				&soc_private->flash_max_current[i]);
			if (rc) {
				CAM_WARN(CAM_FLASH,
					"max-current: read failed");
				of_node_put(flash_src_node);
				continue;
			}

			/* Read max-duration */
			rc = of_property_read_u32(flash_src_node,
				"qcom,duration-ms",
				&soc_private->flash_max_duration[i]);
			if (rc)
				CAM_WARN(CAM_FLASH,
					"max-duration: read failed");

			of_node_put(flash_src_node);

			CAM_DBG(CAM_FLASH, "max_current[%d]: %d",
				i, soc_private->flash_max_current[i]);

			cam_res_mgr_led_trigger_register(
				soc_private->flash_trigger_name[i],
				&fctrl->flash_trigger[i]);
		}
	}

	if (of_get_property(of_node, "torch-source", &count)) {
		count /= sizeof(uint32_t);
		if (count > CAM_FLASH_MAX_LED_TRIGGERS) {
			CAM_ERR(CAM_FLASH, "Invalid LED count : %d", count);
			return -EINVAL;
		}

		fctrl->torch_num_sources = count;

		CAM_DBG(CAM_FLASH, "torch_num_sources = %d",
			fctrl->torch_num_sources);
		for (i = 0; i < count; i++) {
			torch_src_node = of_parse_phandle(of_node,
				"torch-source", i);
			if (!torch_src_node) {
				CAM_WARN(CAM_FLASH, "torch_src_node NULL");
				continue;
			}

			rc = of_property_read_string(torch_src_node,
				"qcom,default-led-trigger",
				&soc_private->torch_trigger_name[i]);
			if (rc < 0) {
				CAM_WARN(CAM_FLASH,
					"default-trigger read failed");
				of_node_put(torch_src_node);
				continue;
			}

			/* Read operational-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,current-ma",
				&soc_private->torch_op_current[i]);
			if (rc < 0) {
				CAM_WARN(CAM_FLASH, "current: read failed");
				of_node_put(torch_src_node);
				continue;
			}

			/* Read max-current */
			rc = of_property_read_u32(torch_src_node,
				"qcom,max-current",
				&soc_private->torch_max_current[i]);
			if (rc < 0) {
				CAM_WARN(CAM_FLASH,
					"max-current: read failed");
				of_node_put(torch_src_node);
				continue;
			}

			of_node_put(torch_src_node);

			CAM_DBG(CAM_FLASH, "max_current[%d]: %d",
				i, soc_private->torch_max_current[i]);

			cam_res_mgr_led_trigger_register(
				soc_private->torch_trigger_name[i],
				&fctrl->torch_trigger[i]);
		}
	}

	return rc;
}

int cam_flash_get_dt_data(struct cam_flash_ctrl *fctrl,
	struct cam_hw_soc_info *soc_info)
{
	int32_t rc = 0;
	struct device_node *of_node = NULL;

	if (!fctrl) {
		CAM_ERR(CAM_FLASH, "NULL flash control structure");
		return -EINVAL;
	}

	of_node = fctrl->pdev->dev.of_node;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_FLASH, "Get_dt_properties failed rc %d", rc);
		return rc;
	}

	soc_info->soc_private =
		kzalloc(sizeof(struct cam_flash_private_soc), GFP_KERNEL);
	if (!soc_info->soc_private) {
		rc = -ENOMEM;
		goto release_soc_res;
	}

	rc = cam_get_source_node_info(of_node, fctrl, soc_info->soc_private);
	if (rc < 0) {
		CAM_ERR(CAM_FLASH,
			"cam_flash_get_pmic_source_info failed rc %d", rc);
		goto free_soc_private;
	}

	return rc;

free_soc_private:
	kfree(soc_info->soc_private);
release_soc_res:
	cam_soc_util_release_platform_resource(soc_info);
	return rc;
}
