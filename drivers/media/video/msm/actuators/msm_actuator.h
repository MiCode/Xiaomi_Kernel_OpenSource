/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <mach/camera.h>
#include <mach/gpio.h>
#include <media/v4l2-subdev.h>
#include <media/msm_camera.h>
#include "msm_camera_i2c.h"

#ifdef LERROR
#undef LERROR
#endif

#ifdef LINFO
#undef LINFO
#endif

#define LERROR(fmt, args...) pr_err(fmt, ##args)

#define CONFIG_MSM_CAMERA_ACT_DBG 0

#if CONFIG_MSM_CAMERA_ACT_DBG
#define LINFO(fmt, args...) printk(fmt, ##args)
#else
#define LINFO(fmt, args...) CDBG(fmt, ##args)
#endif

struct msm_actuator_ctrl_t;

struct region_params_t {
	/* [0] = ForwardDirection Macro boundary
	   [1] = ReverseDirection Inf boundary
	 */
	uint16_t step_bound[2];
	uint16_t code_per_step;
};

struct damping_params_t {
	uint16_t damping_step;
	uint16_t damping_delay;
	void *hw_params;
};

struct damping_t {
	struct damping_params_t *ringing_params;
};

struct msm_actuator_func_tbl {
	int32_t (*actuator_i2c_write_b_af)(struct msm_actuator_ctrl_t *,
			uint8_t,
			uint8_t);
	int32_t (*actuator_init_table)(struct msm_actuator_ctrl_t *);
	int32_t (*actuator_init_focus)(struct msm_actuator_ctrl_t *);
	int32_t (*actuator_set_default_focus) (struct msm_actuator_ctrl_t *);
	int32_t (*actuator_set_params)(struct msm_actuator_ctrl_t *);
	int32_t (*actuator_move_focus) (struct msm_actuator_ctrl_t *,
			int,
			int32_t);
	int (*actuator_power_down) (struct msm_actuator_ctrl_t *);
	int32_t (*actuator_config)(void __user *);
	int32_t (*actuator_i2c_write)(struct msm_actuator_ctrl_t *,
			int16_t, void *);
	int32_t (*actuator_write_focus)(struct msm_actuator_ctrl_t *,
			uint16_t,
			struct damping_params_t *,
			int8_t,
			int16_t);
};

struct msm_actuator_ctrl_t {
	uint32_t i2c_addr;
	struct i2c_driver *i2c_driver;
	struct msm_camera_i2c_client i2c_client;
	struct mutex *actuator_mutex;
	struct msm_actuator_ctrl actuator_ext_ctrl;
	struct msm_actuator_func_tbl func_tbl;
	struct v4l2_subdev *sdev;
	struct v4l2_subdev_ops *act_v4l2_subdev_ops;
	struct msm_actuator_set_info_t set_info;
	struct msm_actuator_get_info_t get_info;

	int16_t initial_code;
	int16_t curr_step_pos;
	uint16_t curr_region_index;
	uint16_t *step_position_table;
	uint16_t *ringing_scenario[2];
	uint16_t scenario_size[2];
	struct region_params_t *region_params;
	uint16_t region_size;
	struct damping_t *damping[2];
	void *user_data;
	uint32_t vcm_pwd;
	uint32_t vcm_enable;
};

int32_t msm_actuator_i2c_write_b_af(struct msm_actuator_ctrl_t *a_ctrl,
		uint8_t msb,
		uint8_t lsb);
int32_t msm_actuator_config(struct msm_actuator_ctrl_t *a_ctrl,
		void __user *cfg_data);
int32_t msm_actuator_move_focus(struct msm_actuator_ctrl_t *a_ctrl,
		int direction,
		int32_t num_steps);
int32_t msm_actuator_init_table(struct msm_actuator_ctrl_t *a_ctrl);
int32_t msm_actuator_set_default_focus(struct msm_actuator_ctrl_t *a_ctrl);
int32_t msm_actuator_af_power_down(struct msm_actuator_ctrl_t *a_ctrl);
int32_t msm_actuator_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id);
int32_t msm_actuator_write_focus(struct msm_actuator_ctrl_t *a_ctrl,
		uint16_t curr_lens_pos,
		struct damping_params_t *damping_params,
		int8_t sign_direction,
		int16_t code_boundary);
int32_t msm_actuator_create_subdevice(struct msm_actuator_ctrl_t *a_ctrl,
		struct i2c_board_info const *board_info,
		struct v4l2_subdev *sdev);

#endif
