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

#include <linux/of.h>
#include <linux/of_gpio.h>
#include <cam_sensor_cmn_header.h>
#include <cam_sensor_util.h>
#include <cam_sensor_io.h>
#include <cam_req_mgr_util.h>
#include "cam_actuator_soc.h"
#include "cam_soc_util.h"

int32_t cam_actuator_parse_dt(struct cam_actuator_ctrl_t *a_ctrl,
	struct device *dev)
{
	int32_t                   rc = 0;
	struct cam_hw_soc_info *soc_info = &a_ctrl->soc_info;
	struct device_node *of_node = NULL;
	struct platform_device *pdev = NULL;

	if (!soc_info->pdev) {
		pr_err("%s:%d :Error:soc_info is not initialized\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	pdev = soc_info->pdev;
	of_node = pdev->dev.of_node;

	/* Initialize mutex */
	mutex_init(&(a_ctrl->actuator_mutex));

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		pr_err("%s:%d :Error: parsing common soc dt(rc %d)\n",
			__func__, __LINE__, rc);
		return rc;
	}
	rc = of_property_read_u32(of_node, "cci-master",
		&(a_ctrl->cci_i2c_master));
	CDBG("cci-master %d, rc %d\n", a_ctrl->cci_i2c_master, rc);
	if (rc < 0 || a_ctrl->cci_i2c_master >= MASTER_MAX) {
		pr_err("%s:%d :Error: Wrong info from dt CCI master as : %d\n",
			__func__, __LINE__, a_ctrl->cci_i2c_master);
		return rc;
	}

	if (!soc_info->gpio_data) {
		pr_info("%s:%d No GPIO found\n", __func__, __LINE__);
		rc = 0;
		return rc;
	}

	if (!soc_info->gpio_data->cam_gpio_common_tbl_size) {
		pr_info("%s:%d No GPIO found\n", __func__, __LINE__);
		return -EINVAL;
	}

	rc = cam_sensor_util_init_gpio_pin_tbl(soc_info,
		&a_ctrl->gpio_num_info);

	if ((rc < 0) || (!a_ctrl->gpio_num_info)) {
		pr_err("%s:%d No/Error Actuator GPIOs\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return rc;
}
