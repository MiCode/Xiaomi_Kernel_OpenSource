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

#define	AD5046_TOTAL_STEPS_NEAR_TO_FAR			32
static uint8_t  mode_mask = 0x09;
DEFINE_MUTEX(ad5046_act_mutex);
static struct msm_actuator_ctrl_t ad5046_act_t;

static struct region_params_t g_regions[] = {
	/* step_bound[0] - macro side boundary
	 * step_bound[1] - infinity side boundary
	 */
	/* Region 1 */
	{
		.step_bound = {AD5046_TOTAL_STEPS_NEAR_TO_FAR, 0},
		.code_per_step = 2,
	},
};

static uint16_t g_scenario[] = {
	/* MOVE_NEAR and MOVE_FAR dir*/
	AD5046_TOTAL_STEPS_NEAR_TO_FAR,
};

static int32_t ad5046_af_i2c_txdata(unsigned short saddr,
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
		pr_err("ad5046_af_i2c_txdata faild 0x%x\n", saddr);
		return -EIO;
	}

	return 0;
}

static int32_t ad5046_af_i2c_write_b_sensor(struct msm_actuator_ctrl_t *a_ctrl,
			      uint8_t waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = waddr;
	buf[1] = bdata;
	rc = ad5046_af_i2c_txdata(a_ctrl->i2c_addr, buf, 2, a_ctrl);
	if (rc < 0)
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
				waddr, bdata);

	return rc;
}

static int32_t ad5046_wrapper_i2c_write(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, void *params)
{
	uint8_t msb, lsb;

	msb = (next_lens_position & 0xFF00) >> 8;
	lsb = next_lens_position & 0xFF;
	ad5046_af_i2c_write_b_sensor(a_ctrl, msb, lsb);

	return 0;
}

int32_t msm_ad5046_act_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	int dir,
	int32_t num_steps)
{
	int32_t rc = 0;
	int8_t sign_dir = 0;
	int16_t dest_step_pos = 0;
	uint8_t code_val_msb, code_val_lsb;

	CDBG("%s called, dir %d, num_steps %d\n",
		__func__,
		dir,
		num_steps);

	/* Determine sign direction */
	if (dir == MOVE_NEAR)
		sign_dir = 20;
	else if (dir == MOVE_FAR)
		sign_dir = -20;
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

	a_ctrl->curr_step_pos = dest_step_pos;

	code_val_msb = (uint8_t)((dest_step_pos & 0x03FF) >> 4);
	code_val_lsb = (uint8_t)((dest_step_pos & 0x000F) << 4);
	code_val_lsb |= mode_mask;

	rc = ad5046_af_i2c_write_b_sensor(a_ctrl, code_val_msb, code_val_lsb);
	/* DAC Setting */
	if (rc != 0) {
		CDBG(KERN_ERR "%s: WRITE ERROR lsb = 0x%x, msb = 0x%x",
			__func__, code_val_lsb, code_val_msb);
	} else {
		CDBG(KERN_ERR "%s: Successful lsb = 0x%x, msb = 0x%x",
			__func__, code_val_lsb, code_val_msb);
		/* delay may set based on the steps moved
		when I2C write successful */
		msleep(100);
	}
	return 0;
}

static int32_t ad5046_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl)
{
	uint8_t  code_val_msb = 0;
	uint8_t  code_val_lsb = 0;
	int rc = 0;

	CDBG("ad5046_set_default_focus called\n");

	if (!a_ctrl->step_position_table)
		a_ctrl->func_tbl.actuator_init_table(a_ctrl);

	a_ctrl->curr_step_pos = 200;

	code_val_msb = (a_ctrl->curr_step_pos & 0x03FF) >> 4;
	code_val_lsb = (a_ctrl->curr_step_pos & 0x000F) << 4;
	code_val_lsb |= mode_mask;

	CDBG(KERN_ERR "ad5046_set_default_focus:lens pos = %d",
		 a_ctrl->curr_step_pos);
	rc = ad5046_af_i2c_write_b_sensor(a_ctrl, code_val_msb, code_val_lsb);
	/* DAC Setting */
	if (rc != 0)
		CDBG(KERN_ERR "%s: WRITE ERROR lsb = 0x%x, msb = 0x%x",
			__func__, code_val_lsb, code_val_msb);
	else
		CDBG(KERN_ERR "%s: WRITE successful lsb = 0x%x, msb = 0x%x",
			__func__, code_val_lsb, code_val_msb);

	usleep_range(10000, 11000);
	return 0;
}

static const struct i2c_device_id ad5046_act_i2c_id[] = {
	{"ad5046_act", (kernel_ulong_t)&ad5046_act_t},
	{ }
};

static int ad5046_act_config(void __user *argp)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_config(&ad5046_act_t, argp);
}

static int ad5046_i2c_add_driver_table(void)
{
	LINFO("%s called\n", __func__);
	return (int) msm_actuator_init_table(&ad5046_act_t);
}

int32_t ad5046_act_i2c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;

	rc = msm_actuator_i2c_probe(client, id);

	msleep(50);

	return rc;
}
static struct i2c_driver ad5046_act_i2c_driver = {
	.id_table = ad5046_act_i2c_id,
	.probe  = ad5046_act_i2c_probe,
	.remove = __exit_p(ad5046_act_i2c_remove),
	.driver = {
		.name = "ad5046_act",
	},
};

static int __init ad5046_i2c_add_driver(void)
{
	int rc = 0;

	LINFO("%s called :%x\n", __func__, ad5046_act_t.i2c_addr);
	rc = i2c_add_driver(ad5046_act_t.i2c_driver);
	LINFO("%s called:%d %x\n", __func__, rc, ad5046_act_t.i2c_addr);
	return rc;
}

static struct v4l2_subdev_core_ops ad5046_act_subdev_core_ops;

static struct v4l2_subdev_ops ad5046_act_subdev_ops = {
	.core = &ad5046_act_subdev_core_ops,
};

static int32_t ad5046_act_create_subdevice(
	void *board_info,
	void *sdev)
{
	int rc = 0;

	struct msm_actuator_info *ptr;
	LINFO("%s called\n", __func__);

	ptr = board_info;
	ad5046_act_t.vcm_pwd = ptr->vcm_pwd;
	ad5046_act_t.vcm_enable = ptr->vcm_enable;
	LINFO("vcm info: %x %x\n", ad5046_act_t.vcm_pwd,
				ad5046_act_t.vcm_enable);
	if (ad5046_act_t.vcm_enable) {
		rc = gpio_request(ad5046_act_t.vcm_pwd, "ov5647_af");
		if (!rc) {
			LINFO("Enable VCM PWD\n");
			gpio_direction_output(ad5046_act_t.vcm_pwd, 1);
		}
		msleep(20);

	}
	return (int) msm_actuator_create_subdevice(&ad5046_act_t,
		ptr->board_info,
		(struct v4l2_subdev *)sdev);
}

static struct msm_actuator_ctrl_t ad5046_act_t = {
	.i2c_driver = &ad5046_act_i2c_driver,
	.i2c_addr = 0x18>>1,
	.act_v4l2_subdev_ops = &ad5046_act_subdev_ops,
	.actuator_ext_ctrl = {
		.a_init_table = ad5046_i2c_add_driver_table,
		.a_create_subdevice = ad5046_act_create_subdevice,
		.a_config = ad5046_act_config,
	},

	.i2c_client = {
		.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
	},

	.set_info = {
		.total_steps = AD5046_TOTAL_STEPS_NEAR_TO_FAR,
	},

	.curr_step_pos = 0,
	.curr_region_index = 0,
	.initial_code = 0x0,
	.actuator_mutex = &ad5046_act_mutex,

	.func_tbl = {
		.actuator_init_table = msm_actuator_init_table,
		.actuator_move_focus = msm_ad5046_act_move_focus,
		.actuator_set_default_focus = ad5046_set_default_focus,
		.actuator_i2c_write = ad5046_wrapper_i2c_write,
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

subsys_initcall(ad5046_i2c_add_driver);
MODULE_DESCRIPTION("AD5046 actuator");
MODULE_LICENSE("GPL v2");
