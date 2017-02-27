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

#include "cam_actuator_soc.h"
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>

int32_t cam_actuator_parse_dt(struct cam_actuator_ctrl_t *a_ctrl,
	struct device *dev)
{
	int32_t                   rc = 0;
	struct cam_actuator_vreg *vreg_cfg;

	/* Initialize mutex */
	mutex_init(&(a_ctrl->actuator_mutex));

	rc = of_property_read_u32(a_ctrl->of_node, "cell-index",
		&(a_ctrl->id));
	CDBG("cell-index %d, rc %d\n", a_ctrl->id, rc);
	if (rc < 0) {
		pr_err("%s:%d :Error: parsing dt for cellindex rc %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	rc = of_property_read_u32(a_ctrl->of_node, "qcom,cci-master",
		&(a_ctrl->cci_i2c_master));
	CDBG("qcom,cci-master %d, rc %d\n", a_ctrl->cci_i2c_master, rc);
	if (rc < 0 || a_ctrl->cci_i2c_master >= MASTER_MAX) {
		pr_err("%s:%d :Error: Wrong info from dt CCI master as : %d\n",
			__func__, __LINE__, a_ctrl->cci_i2c_master);
		return rc;
	}

	if (of_find_property(a_ctrl->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &(a_ctrl->vreg_cfg);
		rc = cam_sensor_get_dt_vreg_data(dev->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			pr_err("%s:%d :Error: parsing regulator dt: %d\n",
				__func__, __LINE__, rc);
			return rc;
		}
	}
	rc = msm_sensor_driver_get_gpio_data(&(a_ctrl->gconf),
		a_ctrl->of_node);
	if (rc < 0) {
		pr_err("%s:%d No/Error Actuator GPIOs\n",
			__func__, __LINE__);
	} else {
		a_ctrl->cam_pinctrl_status = 1;
		rc = msm_camera_pinctrl_init(
			&(a_ctrl->pinctrl_info), dev);
		if (rc < 0) {
			pr_err("ERR:%s: Error in reading actuator pinctrl\n",
				__func__);
			a_ctrl->cam_pinctrl_status = 0;
			rc = 0;
		}
	}

	return rc;
}
