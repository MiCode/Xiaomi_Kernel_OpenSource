/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_SENSOR_UTIL_H_
#define _CAM_SENSOR_UTIL_H_

#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <cam_sensor_cmn_header.h>
#include <cam_req_mgr_util.h>
#include <cam_req_mgr_interface.h>
#include <cam_mem_mgr.h>
#include "cam_soc_util.h"
#include "cam_debug_util.h"
#include "cam_sensor_io.h"

#define INVALID_VREG 100

int cam_get_dt_power_setting_data(struct device_node *of_node,
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_ctrl_t *power_info);

int msm_camera_pinctrl_init
	(struct msm_pinctrl_info *sensor_pctrl, struct device *dev);

int cam_sensor_i2c_command_parser(struct camera_io_master *io_master,
	struct i2c_settings_array *i2c_reg_settings,
	struct cam_cmd_buf_desc *cmd_desc, int32_t num_cmd_buffers);

int cam_sensor_util_i2c_apply_setting(struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list);

int32_t delete_request(struct i2c_settings_array *i2c_array);
int cam_sensor_util_request_gpio_table(
	struct cam_hw_soc_info *soc_info, int gpio_en);

int cam_sensor_util_init_gpio_pin_tbl(
	struct cam_hw_soc_info *soc_info,
	struct msm_camera_gpio_num_info **pgpio_num_info);
int cam_sensor_core_power_up(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info);

int cam_sensor_util_power_down(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info);

int msm_camera_fill_vreg_params(struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_setting *power_setting,
	uint16_t power_setting_size);

int32_t ais_sensor_update_power_settings(
	struct ais_sensor_probe_cmd *probe_cmd,
	struct cam_sensor_power_ctrl_t *power_info);

int32_t cam_sensor_update_power_settings(void *cmd_buf,
	uint32_t cmd_length, struct cam_sensor_power_ctrl_t *power_info,
	size_t cmd_buf_len);

int cam_sensor_bob_pwm_mode_switch(struct cam_hw_soc_info *soc_info,
	int bob_reg_idx, bool flag);
#endif /* _CAM_SENSOR_UTIL_H_ */
