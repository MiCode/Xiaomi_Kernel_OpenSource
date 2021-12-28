// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>

#include "cam_ois_soc.h"
#include "cam_debug_util.h"

/**
 * @e_ctrl: ctrl structure
 *
 * Parses ois dt
 */
static int cam_ois_get_dt_data(struct cam_ois_ctrl_t *o_ctrl)
{
	int                             i, rc = 0;
	struct cam_hw_soc_info         *soc_info = &o_ctrl->soc_info;
	struct cam_ois_soc_private     *soc_private =
		(struct cam_ois_soc_private *)o_ctrl->soc_info.soc_private;
	struct cam_sensor_power_ctrl_t *power_info = &soc_private->power_info;
	struct device_node             *of_node = NULL;

	of_node = soc_info->dev->of_node;

	if (!of_node) {
		CAM_ERR(CAM_OIS, "of_node is NULL, device type %d",
			o_ctrl->ois_device_type);
		return -EINVAL;
	}
	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "cam_soc_util_get_dt_properties rc %d",
			rc);
		return rc;
	}

	/* Initialize regulators to default parameters */
	for (i = 0; i < soc_info->num_rgltr; i++) {
		soc_info->rgltr[i] = devm_regulator_get(soc_info->dev,
					soc_info->rgltr_name[i]);
		if (IS_ERR_OR_NULL(soc_info->rgltr[i])) {
			rc = PTR_ERR(soc_info->rgltr[i]);
			rc = rc ? rc : -EINVAL;
			CAM_ERR(CAM_OIS, "get failed for regulator %s",
				 soc_info->rgltr_name[i]);
			return rc;
		}
		CAM_DBG(CAM_OIS, "get for regulator %s",
			soc_info->rgltr_name[i]);
	}

	if (!soc_info->gpio_data) {
		CAM_INFO(CAM_OIS, "No GPIO found");
		return 0;
	}

	if (!soc_info->gpio_data->cam_gpio_common_tbl_size) {
		CAM_INFO(CAM_OIS, "No GPIO found");
		return -EINVAL;
	}

	rc = cam_sensor_util_init_gpio_pin_tbl(soc_info,
		&power_info->gpio_num_info);
	if ((rc < 0) || (!power_info->gpio_num_info)) {
		CAM_ERR(CAM_OIS, "No/Error OIS GPIOs");
		return -EINVAL;
	}

	for (i = 0; i < soc_info->num_clk; i++) {
		soc_info->clk[i] = devm_clk_get(soc_info->dev,
			soc_info->clk_name[i]);
		if (!soc_info->clk[i]) {
			CAM_ERR(CAM_SENSOR, "get failed for %s",
				soc_info->clk_name[i]);
			rc = -ENOENT;
			return rc;
		}
	}

	return rc;
}
/**
 * @o_ctrl: ctrl structure
 *
 * This function is called from cam_ois_platform/i2c_driver_probe, it parses
 * the ois dt node.
 */
int cam_ois_driver_soc_init(struct cam_ois_ctrl_t *o_ctrl)
{
	int                             rc = 0;
	struct cam_hw_soc_info         *soc_info = &o_ctrl->soc_info;
	struct device_node             *of_node = NULL;
	struct device_node             *of_parent = NULL;

	if (!soc_info->dev) {
		CAM_ERR(CAM_OIS, "soc_info is not initialized");
		return -EINVAL;
	}

	of_node = soc_info->dev->of_node;
	if (!of_node) {
		CAM_ERR(CAM_OIS, "dev.of_node NULL");
		return -EINVAL;
	}

	if (o_ctrl->ois_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = of_property_read_u32(of_node, "cci-master",
			&o_ctrl->cci_i2c_master);
		if (rc < 0) {
			CAM_DBG(CAM_OIS, "failed rc %d", rc);
			return rc;
		}

		of_parent = of_get_parent(of_node);
		if (of_property_read_u32(of_parent, "cell-index",
				&o_ctrl->cci_num) < 0)
			/* Set default master 0 */
			o_ctrl->cci_num = CCI_DEVICE_0;

		o_ctrl->io_master_info.cci_client->cci_device = o_ctrl->cci_num;
		CAM_DBG(CAM_OIS, "cci-device %d", o_ctrl->cci_num, rc);

	}

	rc = cam_ois_get_dt_data(o_ctrl);
	if (rc < 0)
		CAM_DBG(CAM_OIS, "failed: ois get dt data rc %d", rc);

	return rc;
}
