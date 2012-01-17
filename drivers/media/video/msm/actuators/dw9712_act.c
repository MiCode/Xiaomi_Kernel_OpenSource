/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#define	DW9712_TOTAL_STEPS_NEAR_TO_FAR			32
DEFINE_MUTEX(dw9712_act_mutex);
static struct msm_actuator_ctrl_t dw9712_act_t;

static struct region_params_t g_regions[] = {
	/* step_bound[0] - macro side boundary
	 * step_bound[1] - infinity side boundary
	 */
	/* Region 1 */
	{
		.step_bound = {DW9712_TOTAL_STEPS_NEAR_TO_FAR, 0},
		.code_per_step = 2,
	},
};

static uint16_t g_scenario[] = {
	/* MOVE_NEAR and MOVE_FAR dir*/
	DW9712_TOTAL_STEPS_NEAR_TO_FAR,
};

static int32_t dw9712_af_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length,
				struct msm_actuator_ctrl_t *a_ctrl)
{
	struct i2c_msg msg[] = {
		{
			.addr = saddr,
			.flags = 0,
			.len = length,
			.buf = txdata,
		},
	};
	if (i2c_transfer(a_ctrl->i2c_client.client->adapter, msg, 1) < 0) {
		pr_err("s5k4e1_af_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}

	return 0;
}

static int32_t dw9712_af_i2c_write_b_sensor(struct msm_actuator_ctrl_t *a_ctrl,
			      uint8_t waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = waddr;
	buf[1] = bdata;
	rc = dw9712_af_i2c_txdata(a_ctrl->i2c_addr, buf, 2, a_ctrl);
	if (rc < 0) {
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
				waddr, bdata);
	}
	return rc;
}

static int32_t dw9712_wrapper_i2c_write(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, void *params)
{
	uint8_t msb, lsb;

	msb = (next_lens_position & 0xFF00) >> 8;
	lsb = next_lens_position & 0xFF;
	dw9712_af_i2c_write_b_sensor(a_ctrl, msb, lsb);

	return 0;
}

int32_t msm_dw9712_act_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	int dir,
	int32_t num_steps)
{
	int32_t rc = 0;
	int8_t sign_dir = 0;
	int16_t dest_step_pos = 0;
	uint8_t code_val_msb, code_val_lsb;
	uint16_t code_val;

	LINFO("%s called, dir %d, num_steps %d\n",
		__func__,
		dir,
		num_steps);

	/* Determine sign direction */
	if (dir == MOVE_NEAR)
		sign_dir = 1;
	else if (dir == MOVE_FAR)
		sign_dir = -1;
	else {
		pr_err("Illegal focus direction\n");
		rc = -EINVAL;
		return rc;
	}

	/* Determine destination step position */
	dest_step_pos = a_ctrl->curr_step_pos +
		(sign_dir * num_steps);

	if (dest_step_pos < 0)
		dest_step_pos = 0;
	else if (dest_step_pos > 1023)
		dest_step_pos = 1023;

	if (dest_step_pos == a_ctrl->curr_step_pos)
		return rc;

	code_val_msb = dest_step_pos >> 4;
	code_val_lsb = (dest_step_pos & 0x000F) << 4;
	code_val = (code_val_msb << 8) | (code_val_lsb);
	rc = dw9712_wrapper_i2c_write(a_ctrl, code_val, NULL);
	if (rc >= 0) {
		rc = 0;
		a_ctrl->curr_step_pos = dest_step_pos;
	}
	return 0;
}

static int32_t dw9712_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;

	if (!a_ctrl->step_position_table)
		a_ctrl->func_tbl.actuator_init_table(a_ctrl);

	if (a_ctrl->curr_step_pos != 0) {
		rc = a_ctrl->func_tbl.actuator_move_focus(a_ctrl,
				MOVE_FAR, a_ctrl->curr_step_pos);
		a_ctrl->curr_step_pos = 0;
	} else
		rc = dw9712_wrapper_i2c_write(a_ctrl, 0x00, NULL);
		if (rc >= 0)
			rc = 0;
	return rc;
}

static const struct i2c_device_id dw9712_act_i2c_id[] = {
	{"dw9712_act", (kernel_ulong_t)&dw9712_act_t},
	{ }
};

static int dw9712_act_config(void __user *argp)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_config(&dw9712_act_t, argp);
}

static int dw9712_i2c_add_driver_table(void)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_init_table(&dw9712_act_t);
}

static struct i2c_driver dw9712_act_i2c_driver = {
	.id_table = dw9712_act_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(dw9712_act_i2c_remove),
	.driver = {
		.name = "dw9712_act",
	},
};

static int __init dw9712_i2c_add_driver(void)
{
	int rc = 0;

	LINFO("%s called :%x\n", __func__, dw9712_act_t.i2c_addr);
	rc = i2c_add_driver(dw9712_act_t.i2c_driver);
	LINFO("%s called:%d %x\n", __func__, rc, dw9712_act_t.i2c_addr);
	return rc;
}

static struct v4l2_subdev_core_ops dw9712_act_subdev_core_ops;

static struct v4l2_subdev_ops dw9712_act_subdev_ops = {
	.core = &dw9712_act_subdev_core_ops,
};

static int32_t dw9712_act_create_subdevice(
	void *board_info,
	void *sdev)
{
	int rc = 0;

	struct msm_actuator_info *ptr;
	LINFO("%s called\n", __func__);

	ptr = board_info;
	dw9712_act_t.vcm_pwd = ptr->vcm_pwd;
	dw9712_act_t.vcm_enable = ptr->vcm_enable;
	LINFO("vcm info: %x %x\n", dw9712_act_t.vcm_pwd,
				dw9712_act_t.vcm_enable);
	if (dw9712_act_t.vcm_enable) {
		rc = gpio_request(dw9712_act_t.vcm_pwd, "dw9712_af");
		if (!rc) {
			LINFO("Enable VCM PWD\n");
			gpio_direction_output(dw9712_act_t.vcm_pwd, 1);
		}
	}
	return (int) msm_actuator_create_subdevice(&dw9712_act_t,
		ptr->board_info,
		(struct v4l2_subdev *)sdev);
}

static struct msm_actuator_ctrl_t dw9712_act_t = {
	.i2c_driver = &dw9712_act_i2c_driver,
	.i2c_addr = 0x8C,
	.act_v4l2_subdev_ops = &dw9712_act_subdev_ops,
	.actuator_ext_ctrl = {
		.a_init_table = dw9712_i2c_add_driver_table,
		.a_create_subdevice = dw9712_act_create_subdevice,
		.a_config = dw9712_act_config,
	},

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
	},

	.set_info = {
		.total_steps = DW9712_TOTAL_STEPS_NEAR_TO_FAR,
	},

	.curr_step_pos = 0,
	.curr_region_index = 0,
	.initial_code = 0x0,
	.actuator_mutex = &dw9712_act_mutex,

	.func_tbl = {
		.actuator_init_table = msm_actuator_init_table,
		.actuator_move_focus = msm_dw9712_act_move_focus,
		.actuator_set_default_focus = dw9712_set_default_focus,
		.actuator_i2c_write = dw9712_wrapper_i2c_write,
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
};

subsys_initcall(dw9712_i2c_add_driver);
MODULE_DESCRIPTION("DW9712 actuator");
MODULE_LICENSE("GPL v2");
