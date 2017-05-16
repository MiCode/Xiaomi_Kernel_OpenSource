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

	src_node = of_parse_phandle(of_node, "qcom,actuator-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,actuator cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_ACTUATOR] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "qcom,ois-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,ois cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_OIS] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "qcom,eeprom-src", 0);
	if (!src_node) {
		CDBG("%s:%d eeprom src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,eeprom cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_EEPROM] = val;
		of_node_put(src_node);
	}

	src_node = of_parse_phandle(of_node, "qcom,led-flash-src", 0);
	if (!src_node) {
		CDBG("%s:%d src_node NULL\n", __func__, __LINE__);
	} else {
		rc = of_property_read_u32(src_node, "cell-index", &val);
		CDBG("%s qcom,led flash cell index %d, rc %d\n", __func__,
			val, rc);
		if (rc < 0) {
			pr_err("%s:%d failed %d\n", __func__, __LINE__, rc);
			of_node_put(src_node);
			return rc;
		}
		sensor_info->subdev_id[SUB_MODULE_LED_FLASH] = val;
		of_node_put(src_node);
	}

	rc = of_property_read_u32(of_node, "qcom,csiphy-sd-index", &val);
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
	uint32_t cell_id;

	s_ctrl->sensordata = kzalloc(sizeof(*sensordata), GFP_KERNEL);
	if (!s_ctrl->sensordata)
		return -ENOMEM;

	sensordata = s_ctrl->sensordata;
	/*
	 * Read cell index - this cell index will be the camera slot where
	 * this camera will be mounted
	 */
	rc = of_property_read_u32(of_node, "cell-index", &cell_id);
	if (rc < 0) {
		pr_err("failed: cell-index rc %d", rc);
		goto FREE_SENSOR_DATA;
	}
	s_ctrl->id = cell_id;

	/* Validate cell_id */
	if (cell_id >= MAX_CAMERAS) {
		pr_err("failed: invalid cell_id %d", cell_id);
		rc = -EINVAL;
		goto FREE_SENSOR_DATA;
	}

	/* Read subdev info */
	rc = cam_sensor_get_sub_module_index(of_node, sensordata);
	if (rc < 0) {
		pr_err("failed");
		goto FREE_SENSOR_DATA;
	}

	/* Read vreg information */
	rc = cam_sensor_get_dt_vreg_data(of_node,
		&sensordata->power_info.cam_vreg,
		&sensordata->power_info.num_vreg);
	if (rc < 0) {
		pr_err("failed: cam_sensor_get_dt_vreg_data rc %d", rc);
		goto FREE_SENSOR_DATA;
	}

	/* Read gpio information */
	rc = msm_sensor_driver_get_gpio_data
		(&(sensordata->power_info.gpio_conf), of_node);
	if (rc < 0) {
		pr_err("failed: msm_sensor_driver_get_gpio_data rc %d", rc);
		goto FREE_VREG_DATA;
	}

	/* Get CCI master */
	rc = of_property_read_u32(of_node, "qcom,cci-master",
		&s_ctrl->cci_i2c_master);
	CDBG("qcom,cci-master %d, rc %d", s_ctrl->cci_i2c_master, rc);
	if (rc < 0) {
		/* Set default master 0 */
		s_ctrl->cci_i2c_master = MASTER_0;
		rc = 0;
	}

	if (of_property_read_u32(of_node, "qcom,sensor-position-pitch",
		&sensordata->pos_pitch) < 0) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->pos_pitch = 360;
	}
	if (of_property_read_u32(of_node, "qcom,sensor-position-roll",
		&sensordata->pos_roll) < 0) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->pos_roll = 360;
	}
	if (of_property_read_u32(of_node, "qcom,sensor-position-yaw",
		&sensordata->pos_yaw) < 0) {
		CDBG("%s:%d Invalid sensor position\n", __func__, __LINE__);
		sensordata->pos_yaw = 360;
	}

	return rc;

FREE_VREG_DATA:
	kfree(sensordata->power_info.cam_vreg);
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
	int32_t rc = 0;

	/* Parse dt information and store in sensor control structure */
	rc = cam_sensor_driver_get_dt_data(s_ctrl);
	if (rc < 0) {
		pr_err("failed: rc %d", rc);
		return rc;
	}

	/* Initialize mutex */
	mutex_init(&(s_ctrl->cam_sensor_mutex));

	pr_err("%s: %d\n", __func__, __LINE__);
	/* Initialize default parameters */
	rc = msm_sensor_init_default_params(s_ctrl);
	if (rc < 0) {
		pr_err("failed: msm_sensor_init_default_params rc %d", rc);
		goto FREE_DT_DATA;
	}

	/* Get clocks information */
	rc = msm_camera_get_clk_info(s_ctrl->pdev,
		&s_ctrl->sensordata->power_info.clk_info,
		&s_ctrl->sensordata->power_info.clk_ptr,
		&s_ctrl->sensordata->power_info.clk_info_size);
	if (rc < 0) {
		pr_err("failed: msm_camera_get_clk_info rc %d", rc);
		goto FREE_DT_DATA;
	}

	return rc;

FREE_DT_DATA:
	kfree(s_ctrl->sensordata->power_info.gpio_conf->gpio_num_info);
	kfree(s_ctrl->sensordata->power_info.gpio_conf->cam_gpio_req_tbl);
	kfree(s_ctrl->sensordata->power_info.gpio_conf);
	kfree(s_ctrl->sensordata->power_info.cam_vreg);
	kfree(s_ctrl->sensordata);
	s_ctrl->sensordata = NULL;

	return rc;
}
