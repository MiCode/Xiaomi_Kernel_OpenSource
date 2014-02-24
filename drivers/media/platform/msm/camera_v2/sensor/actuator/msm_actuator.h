/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
#ifndef MSM_ACTUATOR_H
#define MSM_ACTUATOR_H

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <mach/camera2.h>
#include <media/v4l2-subdev.h>
#include <media/msmb_camera.h>
#include "msm_camera_i2c.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

struct msm_actuator_ctrl_t;

struct msm_actuator_func_tbl {
	int32_t (*actuator_i2c_write_b_af)(struct msm_actuator_ctrl_t *,
			uint8_t,
			uint8_t);
	int32_t (*actuator_init_step_table)(struct msm_actuator_ctrl_t *,
		struct msm_actuator_set_info_t *);
	int32_t (*actuator_init_focus)(struct msm_actuator_ctrl_t *,
		uint16_t, struct reg_settings_t *);
	int32_t (*actuator_set_default_focus) (struct msm_actuator_ctrl_t *,
			struct msm_actuator_move_params_t *);
	int32_t (*actuator_move_focus) (struct msm_actuator_ctrl_t *,
			struct msm_actuator_move_params_t *);
	void (*actuator_parse_i2c_params)(struct msm_actuator_ctrl_t *,
			int16_t, uint32_t, uint16_t);
	void (*actuator_write_focus)(struct msm_actuator_ctrl_t *,
			uint16_t,
			struct damping_params_t *,
			int8_t,
			int16_t);
	int32_t (*actuator_set_position)(struct msm_actuator_ctrl_t *,
		struct msm_actuator_set_position_t *);
};

struct msm_actuator {
	enum actuator_type act_type;
	struct msm_actuator_func_tbl func_tbl;
};

struct msm_actuator_ctrl_t {
	struct i2c_driver *i2c_driver;
	struct platform_driver *pdriver;
	struct platform_device *pdev;
	struct msm_camera_i2c_client i2c_client;
	enum msm_camera_device_type_t act_device_type;
	struct msm_sd_subdev msm_sd;
	enum af_camera_name cam_name;
	struct mutex *actuator_mutex;
	struct msm_actuator_func_tbl *func_tbl;
	enum msm_actuator_data_type i2c_data_type;
	struct v4l2_subdev sdev;
	struct v4l2_subdev_ops *act_v4l2_subdev_ops;

	int16_t curr_step_pos;
	uint16_t curr_region_index;
	uint16_t *step_position_table;
	struct region_params_t region_params[MAX_ACTUATOR_REGION];
	uint16_t reg_tbl_size;
	struct msm_actuator_reg_params_t reg_tbl[MAX_ACTUATOR_REG_TBL_SIZE];
	uint16_t region_size;
	void *user_data;
	uint32_t vcm_pwd;
	uint32_t vcm_enable;
	uint32_t total_steps;
	uint16_t pwd_step;
	uint16_t initial_code;
	struct msm_camera_i2c_reg_array *i2c_reg_tbl;
	uint16_t i2c_tbl_index;
	enum cci_i2c_master_t cci_master;
	uint32_t subdev_id;
};

#endif
