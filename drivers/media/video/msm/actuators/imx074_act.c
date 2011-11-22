/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "msm_actuator.h"
#include "msm_camera_i2c.h"

#define	IMX074_TOTAL_STEPS_NEAR_TO_FAR			41
DEFINE_MUTEX(imx074_act_mutex);
static struct msm_actuator_ctrl_t imx074_act_t;

static struct region_params_t g_regions[] = {
	/* step_bound[0] - macro side boundary
	 * step_bound[1] - infinity side boundary
	 */
	/* Region 1 */
	{
		.step_bound = {IMX074_TOTAL_STEPS_NEAR_TO_FAR, 0},
		.code_per_step = 2,
	},
};

static uint16_t g_scenario[] = {
	/* MOVE_NEAR and MOVE_FAR dir*/
	IMX074_TOTAL_STEPS_NEAR_TO_FAR,
};

static struct damping_params_t g_damping[] = {
	/* MOVE_NEAR Dir */
	/* Scene 1 => Damping params */
	{
		.damping_step = 0xFF,
		.damping_delay = 0,
	},
};

static struct damping_t g_damping_params[] = {
	/* MOVE_NEAR and MOVE_FAR dir */
	/* Region 1 */
	{
		.ringing_params = g_damping,
	},
};

static int32_t imx074_wrapper_i2c_write(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, void *params)
{
	msm_camera_i2c_write(&a_ctrl->i2c_client,
			     0x00,
			     next_lens_position,
			     MSM_CAMERA_I2C_BYTE_DATA);
	return 0;
}

int32_t imx074_act_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary)
{
	int32_t rc = 0;
	uint16_t dac_value = 0;

	LINFO("%s called, curr lens pos = %d, code_boundary = %d\n",
		  __func__,
		  curr_lens_pos,
		  code_boundary);

	if (sign_direction == 1)
		dac_value = (code_boundary - curr_lens_pos) | 0x80;
	else
		dac_value = (curr_lens_pos - code_boundary);

	LINFO("%s dac_value = %d\n",
	      __func__,
	      dac_value);

	rc = a_ctrl->func_tbl.actuator_i2c_write(a_ctrl, dac_value, NULL);

	return rc;
}

static int32_t imx074_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;

	if (!a_ctrl->step_position_table)
		a_ctrl->func_tbl.actuator_init_table(a_ctrl);

	if (a_ctrl->curr_step_pos != 0) {
		rc = a_ctrl->func_tbl.actuator_i2c_write(a_ctrl, 0x7F, NULL);
		rc = a_ctrl->func_tbl.actuator_i2c_write(a_ctrl, 0x7F, NULL);
		a_ctrl->curr_step_pos = 0;
	} else if (a_ctrl->func_tbl.actuator_init_focus)
		rc = a_ctrl->func_tbl.actuator_init_focus(a_ctrl);
	return rc;
}

static int32_t imx074_act_init_focus(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc;
	LINFO("%s called\n",
	      __func__);
	/* Initialize to infinity */
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0x01,
		0xA9,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0x02,
		0xD2,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0x03,
		0x0C,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0x04,
		0x14,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0x05,
		0xB6,
		MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(&a_ctrl->i2c_client,
		0x06,
		0x4F,
		MSM_CAMERA_I2C_BYTE_DATA);

	rc = a_ctrl->func_tbl.actuator_i2c_write(a_ctrl, 0x7F, NULL);
	rc = a_ctrl->func_tbl.actuator_i2c_write(a_ctrl, 0x7F, NULL);
	a_ctrl->curr_step_pos = 0;
	return rc;
}

static const struct i2c_device_id imx074_act_i2c_id[] = {
	{"imx074_act", (kernel_ulong_t)&imx074_act_t},
	{ }
};

static int imx074_act_config(
	void __user *argp)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_config(&imx074_act_t, argp);
}

static int imx074_i2c_add_driver_table(
	void)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_init_table(&imx074_act_t);
}

static struct i2c_driver imx074_act_i2c_driver = {
	.id_table = imx074_act_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(imx074_act_i2c_remove),
	.driver = {
		.name = "imx074_act",
	},
};

static int __init imx074_i2c_add_driver(
	void)
{
	LINFO("%s called\n", __func__);
	return i2c_add_driver(imx074_act_t.i2c_driver);
}

static struct v4l2_subdev_core_ops imx074_act_subdev_core_ops;

static struct v4l2_subdev_ops imx074_act_subdev_ops = {
	.core = &imx074_act_subdev_core_ops,
};

static int32_t imx074_act_create_subdevice(
	void *board_info,
	void *sdev)
{
	LINFO("%s called\n", __func__);

	return (int) msm_actuator_create_subdevice(&imx074_act_t,
		(struct i2c_board_info const *)board_info,
		(struct v4l2_subdev *)sdev);
}

static struct msm_actuator_ctrl_t imx074_act_t = {
	.i2c_driver = &imx074_act_i2c_driver,
	.i2c_addr = 0xE4,
	.act_v4l2_subdev_ops = &imx074_act_subdev_ops,
	.actuator_ext_ctrl = {
		.a_init_table = imx074_i2c_add_driver_table,
		.a_create_subdevice = imx074_act_create_subdevice,
		.a_config = imx074_act_config,
	},

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	},

	.set_info = {
		.total_steps = IMX074_TOTAL_STEPS_NEAR_TO_FAR,
	},

	.curr_step_pos = 0,
	.curr_region_index = 0,
	.initial_code = 0x7F,
	.actuator_mutex = &imx074_act_mutex,

	.func_tbl = {
		.actuator_init_table = msm_actuator_init_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = imx074_act_write_focus,
		.actuator_set_default_focus = imx074_set_default_focus,
		.actuator_init_focus = imx074_act_init_focus,
		.actuator_i2c_write = imx074_wrapper_i2c_write,
	},

	.get_info = {
		.focal_length_num = 46,
		.focal_length_den = 10,
		.f_number_num = 265,
		.f_number_den = 100,
		.f_pix_num = 14,
		.f_pix_den = 10,
		.total_f_dist_num = 197681,
		.total_f_dist_den = 1000,
	},

	/* Initialize scenario */
	.ringing_scenario[MOVE_NEAR] = g_scenario,
	.scenario_size[MOVE_NEAR] = ARRAY_SIZE(g_scenario),
	.ringing_scenario[MOVE_FAR] = g_scenario,
	.scenario_size[MOVE_FAR] = ARRAY_SIZE(g_scenario),

	/* Initialize region params */
	.region_params = g_regions,
	.region_size = ARRAY_SIZE(g_regions),

	/* Initialize damping params */
	.damping[MOVE_NEAR] = g_damping_params,
	.damping[MOVE_FAR] = g_damping_params,
};

subsys_initcall(imx074_i2c_add_driver);
MODULE_DESCRIPTION("IMX074 actuator");
MODULE_LICENSE("GPL v2");
