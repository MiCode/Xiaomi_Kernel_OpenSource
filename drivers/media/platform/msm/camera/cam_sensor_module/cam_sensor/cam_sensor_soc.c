/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>
#include "cam_sensor_soc.h"
#include "cam_soc_util.h"

int32_t cam_sensor_get_sub_module_index(struct device_node *of_node,
	struct cam_sensor_board_info *s_info)
{
	int rc = 0, i = 0;
	uint32_t val = 0;
	struct device_node *src_node = NULL;
	struct cam_sensor_board_info *sensor_info;

	sensor_info = s_info;

	for (i = 0; i < SUB_MODULE_MAX; i++)
		sensor_info->subdev_id[i] = -1;

	src_node = of_parse_phandle(of_node, "actuator-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s:%d actuator cell index %d, rc %d\n", __func__,
			__LINE__, val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed %d\n", __func__, __LINE__, rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_ACTUATOR] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "ois-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s:%d ois cell index %d, rc %d\n", __func__, __LINE__,
			val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed %d\n", __func__, __LINE__, rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_OIS] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "eeprom-src", 0);
	if (!src_node) {
		CDBG("%s:%d eeprom src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s:%d eeprom cell index %d, rc %d\n", __func__, __LINE__,
			val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed %d\n", __func__, __LINE__, rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "led-flash-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s:%d led flash cell index %d, rc %d\n", __func__,
			__LINE__, val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed %d\n", __func__, __LINE__, rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_LED_FLASH] = val;
		of_node_put(src_node);
	}

	rc = of_property_read_u32(of_node, "csiphy-sd-index", &val);
	if (rc < 0)
		pr_err("%s:%d :Error: paring the dt node for csiphy rc %d\n",
			__func__, __LINE__, rc);
	else
		sensor_info->subdev_id[SUB_MODULE_CSIPHY] = val;

	return rc;
}

static int32_t cam_sensor_driver_get_dt_data(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct cam_sensor_board_info *sensordata = NULL;
	struct device_node *of_node = s_ctrl->of_node;
	struct cam_hw_soc_info *soc_info = &s_ctrl->soc_info;
	s_ctrl->sensordata = kzalloc(sizeof(*sensordata), GFP_KERNEL);
	if (!s_ctrl->sensordata)
		return -ENOMEM;

	sensordata = s_ctrl->sensordata;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		pr_err("%s:%d Failed to read DT properties rc %d",
			__func__, __LINE__, rc);
		goto FREE_SENSOR_DATA;
	}

	rc =  cam_sensor_util_init_gpio_pin_tbl(soc_info,
			&sensordata->power_info.gpio_num_info);
	if (rc < 0) {
		pr_err("%s:%d Failed to read gpios %d", __func__, __LINE__, rc);
		goto FREE_SENSOR_DATA;
	}

	s_ctrl->id = soc_info->index;

	/* Validate cell_id */
	if (s_ctrl->id >= MAX_CAMERAS) {
		pr_err("%s:%d Failed invalid cell_id %d", __func__, __LINE__,
			s_ctrl->id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Read subdev info */
	rc = cam_sensor_get_sub_module_index(of_node, sensordata);
	if (rc < 0) {
		pr_err("%s:%d failed to get sub module index, rc=%d\n",
			__func__, __LINE__, rc);
		goto FREE_SENSOR_DATA;
	}

	/* Get CCI master */
	rc = of_property_read_u32(of_node, "cci-master",
		&s_ctrl->cci_i2c_master);
	CDBG("%s:%d cci-master %d, rc %d", __func__, __LINE__,
		s_ctrl->cci_i2c_master, rc);
	if (rc < 0) {
		/* Set default master 0 */
		s_ctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	if (of_property_read_u32(of_node, "sensor-position-pitch",
		&sensordata->pos_pitch) < 0) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->pos_pitch = 360;
	}
	if (of_property_read_u32(of_node, "sensor-position-roll",
		&sensordata->pos_roll) < 0) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->pos_roll = 360;
	}
	if (of_property_read_u32(of_node, "sensor-position-yaw",
		&sensordata->pos_yaw) < 0) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->pos_yaw = 360;
	}

	return rc;

FREE_SENSOR_DATA:
	kfree(sensordata);
	return rc;
}

int32_t msm_sensor_init_default_params(struct cam_sensor_ctrl_t *s_ctrl)
{
	/* Validate input parameters */
	if (!s_ctrl) {
		pr_err("%s:%d failed: invalid params s_ctrl %pK\n", __func__,
			__LINE__, s_ctrl);
		return -EINVAL;
	}

	CDBG("%s: %d master_type: %d\n", __func__, __LINE__,
		s_ctrl->io_master_info.master_type);
	/* Initialize cci_client */
	if (s_ctrl->io_master_info.master_type == CCI_MASTER) {
		s_ctrl->io_master_info.cci_client = kzalloc(sizeof(
			struct cam_sensor_cci_client), GFP_KERNEL);
		if (!(s_ctrl->io_master_info.cci_client))
			return -ENOMEM;

	} else {
		pr_err("%s:%d Invalid master / Master type Not supported\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

int32_t cam_sensor_parse_dt(struct cam_sensor_ctrl_t *s_ctrl)
{
	int32_t i, rc = 0;
	struct cam_hw_soc_info *soc_info = &s_ctrl->soc_info;

	/* Parse dt information and store in sensor control structure */
	rc = cam_sensor_driver_get_dt_data(s_ctrl);
	if (rc < 0) {
		pr_err("%s:%d Failed to get dt data rc %d", __func__, __LINE__,
			rc);
		return rc;
	}

	/* Initialize mutex */
	mutex_init(&(s_ctrl->cam_sensor_mutex));

	CDBG("%s: %d\n", __func__, __LINE__);
	/* Initialize default parameters */
	for (i = 0; i < soc_info->num_clk; i++) {
		soc_info->clk[i] = devm_clk_get(&soc_info->pdev->dev,
					soc_info->clk_name[i]);
		if (!soc_info->clk[i]) {
			pr_err("%s:%d get failed for %s\n",
				__func__, __LINE__, soc_info->clk_name[i]);
			rc = -ENOENT;
			return rc;
		}
	}
	rc = msm_sensor_init_default_params(s_ctrl);
	if (rc < 0) {
		pr_err("%s;%d failed: msm_sensor_init_default_params rc %d",
			__func__, __LINE__, rc);
		goto FREE_DT_DATA;
	}

	return rc;

FREE_DT_DATA:
	kfree(s_ctrl->sensordata);
	s_ctrl->sensordata = NULL;

	return rc;
}
