/* Copyright (c) 2011-2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/module.h>
#include "msm_sd.h"
#include "msm_actuator.h"
#include "msm_cci.h"

DEFINE_MSM_MUTEX(msm_actuator_mutex);

#undef CDBG
#ifdef MSM_ACTUATOR_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define PARK_LENS_LONG_STEP 7
#define PARK_LENS_MID_STEP 5
#define PARK_LENS_SMALL_STEP 3
#define MAX_QVALUE 4096

static struct v4l2_file_operations msm_actuator_v4l2_subdev_fops;
static int32_t msm_actuator_power_up(struct msm_actuator_ctrl_t *a_ctrl);
static int32_t msm_actuator_power_down(struct msm_actuator_ctrl_t *a_ctrl);

static struct msm_actuator msm_vcm_actuator_table;
static struct msm_actuator msm_piezo_actuator_table;
static struct msm_actuator msm_hvcm_actuator_table;
static struct msm_actuator msm_bivcm_actuator_table;

static struct i2c_driver msm_actuator_i2c_driver;
static struct msm_actuator *actuators[] = {
	&msm_vcm_actuator_table,
	&msm_piezo_actuator_table,
	&msm_hvcm_actuator_table,
	&msm_bivcm_actuator_table,
};

static int32_t msm_actuator_piezo_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("Enter\n");

	if (a_ctrl->i2c_reg_tbl == NULL) {
		pr_err("failed. i2c reg tabl is NULL");
		return -EFAULT;
	}

	if (a_ctrl->curr_step_pos != 0) {
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			a_ctrl->initial_code, 0, 0);
		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.data_type = a_ctrl->i2c_data_type;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		rc = a_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
		if (rc < 0) {
			pr_err("%s: i2c write error:%d\n",
				__func__, rc);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->curr_step_pos = 0;
	}
	CDBG("Exit\n");
	return rc;
}

static void msm_actuator_parse_i2c_params(struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, uint32_t hw_params, uint16_t delay)
{
	struct msm_actuator_reg_params_t *write_arr = NULL;
	uint32_t hw_dword = hw_params;
	uint16_t i2c_byte1 = 0, i2c_byte2 = 0;
	uint16_t value = 0;
	uint32_t size = 0, i = 0;
	struct msm_camera_i2c_reg_array *i2c_tbl = NULL;
	CDBG("Enter\n");

	if (a_ctrl == NULL) {
		pr_err("failed. actuator ctrl is NULL");
		return;
	}

	if (a_ctrl->i2c_reg_tbl == NULL) {
		pr_err("failed. i2c reg tabl is NULL");
		return;
	}

	size = a_ctrl->reg_tbl_size;
	write_arr = a_ctrl->reg_tbl;
	i2c_tbl = a_ctrl->i2c_reg_tbl;

	for (i = 0; i < size; i++) {
		if (write_arr[i].reg_write_type == MSM_ACTUATOR_WRITE_DAC) {
			value = (next_lens_position <<
				write_arr[i].data_shift) |
				((hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift);

			if (write_arr[i].reg_addr != 0xFFFF) {
				i2c_byte1 = write_arr[i].reg_addr;
				i2c_byte2 = value;
				if (size != (i+1)) {
					i2c_byte2 = value & 0xFF;
					CDBG("byte1:0x%x, byte2:0x%x\n",
						i2c_byte1, i2c_byte2);
					if (a_ctrl->i2c_tbl_index >
						a_ctrl->total_steps) {
						pr_err("failed:i2c table index out of bound\n");
						break;
					}
					i2c_tbl[a_ctrl->i2c_tbl_index].
						reg_addr = i2c_byte1;
					i2c_tbl[a_ctrl->i2c_tbl_index].
						reg_data = i2c_byte2;
					i2c_tbl[a_ctrl->i2c_tbl_index].
						delay = 0;
					a_ctrl->i2c_tbl_index++;
					i++;
					i2c_byte1 = write_arr[i].reg_addr;
					i2c_byte2 = (value & 0xFF00) >> 8;
				}
			} else {
				i2c_byte1 = (value & 0xFF00) >> 8;
				i2c_byte2 = value & 0xFF;
			}
		} else {
			i2c_byte1 = write_arr[i].reg_addr;
			i2c_byte2 = (hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift;
		}
		if (a_ctrl->i2c_tbl_index > a_ctrl->total_steps) {
			pr_err("failed: i2c table index out of bound\n");
			break;
		}
		CDBG("i2c_byte1:0x%x, i2c_byte2:0x%x\n", i2c_byte1, i2c_byte2);
		i2c_tbl[a_ctrl->i2c_tbl_index].reg_addr = i2c_byte1;
		i2c_tbl[a_ctrl->i2c_tbl_index].reg_data = i2c_byte2;
		i2c_tbl[a_ctrl->i2c_tbl_index].delay = delay;
		a_ctrl->i2c_tbl_index++;
	}
	CDBG("Exit\n");
}

static int msm_actuator_bivcm_handle_i2c_ops(
	struct msm_actuator_ctrl_t *a_ctrl,
	int16_t next_lens_position, uint32_t hw_params, uint16_t delay)
{
	struct msm_actuator_reg_params_t *write_arr = a_ctrl->reg_tbl;
	uint32_t hw_dword = hw_params;
	uint16_t i2c_byte1 = 0, i2c_byte2 = 0;
	uint16_t value = 0, reg_data = 0;
	uint32_t size = a_ctrl->reg_tbl_size, i = 0;
	int32_t rc = 0;
	struct msm_camera_i2c_reg_array i2c_tbl;
	struct msm_camera_i2c_reg_setting reg_setting;
	enum msm_camera_i2c_reg_addr_type save_addr_type =
		a_ctrl->i2c_client.addr_type;

	for (i = 0; i < size; i++) {
		reg_setting.size = 1;
		switch (write_arr[i].reg_write_type) {
		case MSM_ACTUATOR_WRITE_DAC:
			value = (next_lens_position <<
			write_arr[i].data_shift) |
			((hw_dword & write_arr[i].hw_mask) >>
			write_arr[i].hw_shift);
			if (write_arr[i].reg_addr != 0xFFFF) {
				i2c_byte1 = write_arr[i].reg_addr;
				i2c_byte2 = value;
			} else {
				i2c_byte1 = (value & 0xFF00) >> 8;
				i2c_byte2 = value & 0xFF;
			}
			i2c_tbl.reg_addr = i2c_byte1;
			i2c_tbl.reg_data = i2c_byte2;
			i2c_tbl.delay = delay;
			a_ctrl->i2c_tbl_index++;

			reg_setting.reg_setting = &i2c_tbl;
			reg_setting.data_type = a_ctrl->i2c_data_type;
			rc = a_ctrl->i2c_client.
				i2c_func_tbl->i2c_write_table_w_microdelay(
				&a_ctrl->i2c_client, &reg_setting);
			if (rc < 0) {
				pr_err("i2c write error:%d\n", rc);
				return rc;
			}
			break;
		case MSM_ACTUATOR_WRITE:
			i2c_tbl.reg_data = write_arr[i].reg_data;
			i2c_tbl.reg_addr = write_arr[i].reg_addr;
			i2c_tbl.delay = write_arr[i].delay;
			reg_setting.reg_setting = &i2c_tbl;
			reg_setting.data_type = write_arr[i].data_type;
			switch (write_arr[i].addr_type) {
			case MSM_ACTUATOR_BYTE_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_BYTE_ADDR;
				break;
			case MSM_ACTUATOR_WORD_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_WORD_ADDR;
				break;
			default:
				pr_err("Unsupport addr type: %d\n",
					write_arr[i].addr_type);
				break;
			}

			rc = a_ctrl->i2c_client.
				i2c_func_tbl->i2c_write_table_w_microdelay(
				&a_ctrl->i2c_client, &reg_setting);
			if (rc < 0) {
				pr_err("i2c write error:%d\n", rc);
				return rc;
			}
			break;
		case MSM_ACTUATOR_WRITE_DIR_REG:
			i2c_tbl.reg_data = hw_dword & 0xFFFF;
			i2c_tbl.reg_addr = write_arr[i].reg_addr;
			i2c_tbl.delay = write_arr[i].delay;
			reg_setting.reg_setting = &i2c_tbl;
			reg_setting.data_type = write_arr[i].data_type;
			switch (write_arr[i].addr_type) {
			case MSM_ACTUATOR_BYTE_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_BYTE_ADDR;
				break;
			case MSM_ACTUATOR_WORD_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_WORD_ADDR;
				break;
			default:
				pr_err("Unsupport addr type: %d\n",
					write_arr[i].addr_type);
				break;
			}

			rc = a_ctrl->i2c_client.
				i2c_func_tbl->i2c_write_table_w_microdelay(
				&a_ctrl->i2c_client, &reg_setting);
			if (rc < 0) {
				pr_err("i2c write error:%d\n", rc);
				return rc;
			}
			break;
		case MSM_ACTUATOR_POLL:
			switch (write_arr[i].addr_type) {
			case MSM_ACTUATOR_BYTE_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_BYTE_ADDR;
				break;
			case MSM_ACTUATOR_WORD_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_WORD_ADDR;
				break;
			default:
				pr_err("Unsupport addr type: %d\n",
					write_arr[i].addr_type);
				break;
			}

			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_poll(
				&a_ctrl->i2c_client,
				write_arr[i].reg_addr,
				write_arr[i].reg_data,
				write_arr[i].data_type,
				write_arr[i].delay);
			if (rc < 0) {
				pr_err("i2c poll error:%d\n", rc);
				return rc;
			}
			break;
		case MSM_ACTUATOR_READ_WRITE:
			i2c_tbl.reg_addr = write_arr[i].reg_addr;
			i2c_tbl.delay = write_arr[i].delay;
			reg_setting.reg_setting = &i2c_tbl;
			reg_setting.data_type = write_arr[i].data_type;

			switch (write_arr[i].addr_type) {
			case MSM_ACTUATOR_BYTE_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_BYTE_ADDR;
				break;
			case MSM_ACTUATOR_WORD_ADDR:
				a_ctrl->i2c_client.addr_type =
					MSM_CAMERA_I2C_WORD_ADDR;
				break;
			default:
				pr_err("Unsupport addr type: %d\n",
					write_arr[i].addr_type);
				break;
			}
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_read(
				&a_ctrl->i2c_client,
				write_arr[i].reg_addr,
				&reg_data,
				write_arr[i].data_type);
			if (rc < 0) {
				pr_err("i2c poll error:%d\n", rc);
				return rc;
			}

			i2c_tbl.reg_addr = write_arr[i].reg_data;
			i2c_tbl.reg_data = reg_data;
			i2c_tbl.delay = write_arr[i].delay;
			reg_setting.reg_setting = &i2c_tbl;
			reg_setting.data_type = write_arr[i].data_type;

			rc = a_ctrl->i2c_client.
				i2c_func_tbl->i2c_write_table_w_microdelay(
				&a_ctrl->i2c_client, &reg_setting);
			if (rc < 0) {
				pr_err("i2c write error:%d\n", rc);
				return rc;
			}
			break;
		case MSM_ACTUATOR_WRITE_HW_DAMP:
			i2c_tbl.reg_addr = write_arr[i].reg_addr;
			i2c_tbl.reg_data = (hw_dword & write_arr[i].hw_mask) >>
				write_arr[i].hw_shift;
			i2c_tbl.delay = 0;
			reg_setting.reg_setting = &i2c_tbl;
			reg_setting.data_type = a_ctrl->i2c_data_type;

			rc = a_ctrl->i2c_client.
				i2c_func_tbl->i2c_write_table_w_microdelay(
				&a_ctrl->i2c_client, &reg_setting);
			if (rc < 0) {
				pr_err("i2c write error:%d\n", rc);
				return rc;
			}
			break;
		default:
			pr_err("%s:%d Invalid selection\n",
				__func__, __LINE__);
			return -EINVAL;
		}
		a_ctrl->i2c_client.addr_type = save_addr_type;
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_init_focus(struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t size, struct reg_settings_t *settings)
{
	int32_t rc = -EFAULT;
	int32_t i = 0;
	enum msm_camera_i2c_reg_addr_type save_addr_type;
	CDBG("Enter\n");

	save_addr_type = a_ctrl->i2c_client.addr_type;
	for (i = 0; i < size; i++) {

		switch (settings[i].addr_type) {
		case MSM_ACTUATOR_BYTE_ADDR:
			a_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
			break;
		case MSM_ACTUATOR_WORD_ADDR:
			a_ctrl->i2c_client.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
			break;
		default:
			pr_err("Unsupport addr type: %d\n",
				settings[i].addr_type);
			break;
		}

		switch (settings[i].i2c_operation) {
		case MSM_ACT_WRITE:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data,
				settings[i].data_type);
			if (settings[i].delay > 20)
				msleep(settings[i].delay);
			else if (0 != settings[i].delay)
				usleep_range(settings[i].delay * 1000,
					(settings[i].delay * 1000) + 1000);
			break;
		case MSM_ACT_POLL:
			rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_poll(
				&a_ctrl->i2c_client,
				settings[i].reg_addr,
				settings[i].reg_data,
				settings[i].data_type,
				settings[i].delay);
			break;
		default:
			pr_err("Unsupport i2c_operation: %d\n",
				settings[i].i2c_operation);
			break;
		}

		if (rc < 0) {
			pr_err("%s:%d fail addr = 0X%X, data = 0X%X, dt = %d",
				__func__, __LINE__, settings[i].reg_addr,
				settings[i].reg_data, settings[i].data_type);
			break;
		}
	}

	a_ctrl->curr_step_pos = 0;
	/*
	 * Recover register addr_type after the init
	 * settings are written.
	 */
	a_ctrl->i2c_client.addr_type = save_addr_type;
	CDBG("Exit\n");
	return rc;
}

static void msm_actuator_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary)
{
	int16_t next_lens_pos = 0;
	uint16_t damping_code_step = 0;
	uint16_t wait_time = 0;
	CDBG("Enter\n");

	damping_code_step = damping_params->damping_step;
	wait_time = damping_params->damping_delay;

	/* Write code based on damping_code_step in a loop */
	for (next_lens_pos =
		curr_lens_pos + (sign_direction * damping_code_step);
		(sign_direction * next_lens_pos) <=
			(sign_direction * code_boundary);
		next_lens_pos =
			(next_lens_pos +
				(sign_direction * damping_code_step))) {
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			next_lens_pos, damping_params->hw_params, wait_time);
		curr_lens_pos = next_lens_pos;
	}

	if (curr_lens_pos != code_boundary) {
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			code_boundary, damping_params->hw_params, wait_time);
	}
	CDBG("Exit\n");
}

static int msm_actuator_bivcm_write_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	uint16_t curr_lens_pos,
	struct damping_params_t *damping_params,
	int8_t sign_direction,
	int16_t code_boundary)
{
	int16_t next_lens_pos = 0;
	uint16_t damping_code_step = 0;
	uint16_t wait_time = 0;
	int32_t rc = 0;
	CDBG("Enter\n");

	damping_code_step = damping_params->damping_step;
	wait_time = damping_params->damping_delay;

	/* Write code based on damping_code_step in a loop */
	for (next_lens_pos =
		curr_lens_pos + (sign_direction * damping_code_step);
		(sign_direction * next_lens_pos) <=
			(sign_direction * code_boundary);
		next_lens_pos =
			(next_lens_pos +
				(sign_direction * damping_code_step))) {
		rc = msm_actuator_bivcm_handle_i2c_ops(a_ctrl,
			next_lens_pos, damping_params->hw_params, wait_time);
		if (rc < 0) {
			pr_err("%s:%d msm_actuator_bivcm_handle_i2c_ops failed\n",
				__func__, __LINE__);
				return rc;
		}
		curr_lens_pos = next_lens_pos;
	}

	if (curr_lens_pos != code_boundary) {
		rc = msm_actuator_bivcm_handle_i2c_ops(a_ctrl,
			code_boundary, damping_params->hw_params, wait_time);
		if (rc < 0) {
			pr_err("%s:%d msm_actuator_bivcm_handle_i2c_ops failed\n",
				__func__, __LINE__);
			return rc;
		}
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_piezo_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t dest_step_position = move_params->dest_step_pos;
	struct damping_params_t ringing_params_kernel;
	int32_t rc = 0;
	int32_t num_steps = move_params->num_steps;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("Enter\n");

	if (copy_from_user(&ringing_params_kernel,
		&(move_params->ringing_params[0]),
		sizeof(struct damping_params_t))) {
		pr_err("copy_from_user failed\n");
		return -EFAULT;
	}

	if (num_steps <= 0 || num_steps > MAX_NUMBER_OF_STEPS) {
		pr_err("num_steps out of range = %d\n",
			num_steps);
		return -EFAULT;
	}

	if (a_ctrl->i2c_reg_tbl == NULL) {
		pr_err("failed. i2c reg tabl is NULL");
		return -EFAULT;
	}

	if (dest_step_position > a_ctrl->total_steps) {
		pr_err("Step pos greater than total steps = %d\n",
			dest_step_position);
		return -EFAULT;
	}

	a_ctrl->i2c_tbl_index = 0;
	a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
		(num_steps *
		a_ctrl->region_params[0].code_per_step),
		ringing_params_kernel.hw_params, 0);

	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
		&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("i2c write error:%d\n", rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;
	a_ctrl->curr_step_pos = dest_step_position;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	struct damping_params_t *ringing_params_kernel = NULL;
	int8_t sign_dir = move_params->sign_dir;
	uint16_t step_boundary = 0;
	uint16_t target_step_pos = 0;
	uint16_t target_lens_pos = 0;
	int16_t dest_step_pos = move_params->dest_step_pos;
	uint16_t curr_lens_pos = 0;
	int dir = move_params->dir;
	int32_t num_steps = move_params->num_steps;
	struct msm_camera_i2c_reg_setting reg_setting;

	CDBG("called, dir %d, num_steps %d\n", dir, num_steps);

	if (dest_step_pos == a_ctrl->curr_step_pos)
		return rc;

	if (a_ctrl->step_position_table[dest_step_pos] ==
	a_ctrl->step_position_table[a_ctrl->curr_step_pos])
		return rc;

	if ((sign_dir > MSM_ACTUATOR_MOVE_SIGNED_NEAR) ||
		(sign_dir < MSM_ACTUATOR_MOVE_SIGNED_FAR)) {
		pr_err("Invalid sign_dir = %d\n", sign_dir);
		return -EFAULT;
	}
	if ((dir > MOVE_FAR) || (dir < MOVE_NEAR)) {
		pr_err("Invalid direction = %d\n", dir);
		return -EFAULT;
	}
	if (a_ctrl->i2c_reg_tbl == NULL) {
		pr_err("failed. i2c reg tabl is NULL");
		return -EFAULT;
	}
	if (dest_step_pos > a_ctrl->total_steps) {
		pr_err("Step pos greater than total steps = %d\n",
		dest_step_pos);
		return -EFAULT;
	}
	if ((a_ctrl->region_size <= 0) ||
		(a_ctrl->region_size > MAX_ACTUATOR_REGION) ||
		(!move_params->ringing_params)) {
		pr_err("Invalid-region size = %d, ringing_params = %pK\n",
		a_ctrl->region_size, move_params->ringing_params);
		return -EFAULT;
	}
	/*Allocate memory for damping parameters of all regions*/
	ringing_params_kernel = kmalloc(
		sizeof(struct damping_params_t)*(a_ctrl->region_size),
		GFP_KERNEL);
	if (!ringing_params_kernel) {
		pr_err("kmalloc for damping parameters failed\n");
		return -EFAULT;
	}
	if (copy_from_user(ringing_params_kernel,
		&(move_params->ringing_params[0]),
		(sizeof(struct damping_params_t))*(a_ctrl->region_size))) {
		pr_err("copy_from_user failed\n");
		/*Free the allocated memory for damping parameters*/
		kfree(ringing_params_kernel);
		return -EFAULT;
	}
	curr_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	a_ctrl->i2c_tbl_index = 0;
	CDBG("curr_step_pos =%d dest_step_pos =%d curr_lens_pos=%d\n",
		a_ctrl->curr_step_pos, dest_step_pos, curr_lens_pos);

	while (a_ctrl->curr_step_pos != dest_step_pos) {
		if (a_ctrl->curr_region_index >= a_ctrl->region_size)
			break;
		step_boundary =
			a_ctrl->region_params[a_ctrl->curr_region_index].
			step_bound[dir];
		if ((dest_step_pos * sign_dir) <=
			(step_boundary * sign_dir)) {

			target_step_pos = dest_step_pos;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&ringing_params_kernel
					[a_ctrl->curr_region_index],
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

		} else {
			target_step_pos = step_boundary;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			a_ctrl->func_tbl->actuator_write_focus(a_ctrl,
					curr_lens_pos,
					&ringing_params_kernel
					[a_ctrl->curr_region_index],
					sign_dir,
					target_lens_pos);
			curr_lens_pos = target_lens_pos;

			a_ctrl->curr_region_index += sign_dir;
		}
		a_ctrl->curr_step_pos = target_step_pos;
	}
	/*Free the memory allocated for damping parameters*/
	kfree(ringing_params_kernel);

	move_params->curr_lens_pos = curr_lens_pos;
	reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
	reg_setting.data_type = a_ctrl->i2c_data_type;
	reg_setting.size = a_ctrl->i2c_tbl_index;
	rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_write_table_w_microdelay(
		&a_ctrl->i2c_client, &reg_setting);
	if (rc < 0) {
		pr_err("i2c write error:%d\n", rc);
		return rc;
	}
	a_ctrl->i2c_tbl_index = 0;
	CDBG("Exit\n");

	return rc;
}

static int32_t msm_actuator_bivcm_move_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	struct damping_params_t *ringing_params_kernel = NULL;
	int8_t sign_dir = move_params->sign_dir;
	uint16_t step_boundary = 0;
	uint16_t target_step_pos = 0;
	uint16_t target_lens_pos = 0;
	int16_t dest_step_pos = move_params->dest_step_pos;
	uint16_t curr_lens_pos = 0;
	int dir = move_params->dir;
	int32_t num_steps = move_params->num_steps;

	if (a_ctrl->step_position_table == NULL) {
		pr_err("Step Position Table is NULL");
		return -EFAULT;
	}

	CDBG("called, dir %d, num_steps %d\n", dir, num_steps);

	if (dest_step_pos == a_ctrl->curr_step_pos)
		return rc;

	if ((sign_dir > MSM_ACTUATOR_MOVE_SIGNED_NEAR) ||
		(sign_dir < MSM_ACTUATOR_MOVE_SIGNED_FAR)) {
		pr_err("Invalid sign_dir = %d\n", sign_dir);
		return -EFAULT;
	}
	if ((dir > MOVE_FAR) || (dir < MOVE_NEAR)) {
		pr_err("Invalid direction = %d\n", dir);
		return -EFAULT;
	}
	if (dest_step_pos > a_ctrl->total_steps) {
		pr_err("Step pos greater than total steps = %d\n",
		dest_step_pos);
		return -EFAULT;
	}
	if ((a_ctrl->region_size <= 0) ||
		(a_ctrl->region_size > MAX_ACTUATOR_REGION) ||
		(!move_params->ringing_params)) {
		pr_err("Invalid-region size = %d, ringing_params = %pK\n",
		a_ctrl->region_size, move_params->ringing_params);
		return -EFAULT;
	}
	/*Allocate memory for damping parameters of all regions*/
	ringing_params_kernel = kmalloc(
		sizeof(struct damping_params_t)*(a_ctrl->region_size),
		GFP_KERNEL);
	if (!ringing_params_kernel) {
		pr_err("kmalloc for damping parameters failed\n");
		return -EFAULT;
	}
	if (copy_from_user(ringing_params_kernel,
		&(move_params->ringing_params[0]),
		(sizeof(struct damping_params_t))*(a_ctrl->region_size))) {
		pr_err("copy_from_user failed\n");
		/*Free the allocated memory for damping parameters*/
		kfree(ringing_params_kernel);
		return -EFAULT;
	}
	curr_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	a_ctrl->i2c_tbl_index = 0;
	CDBG("curr_step_pos =%d dest_step_pos =%d curr_lens_pos=%d\n",
		a_ctrl->curr_step_pos, dest_step_pos, curr_lens_pos);

	while (a_ctrl->curr_step_pos != dest_step_pos) {
		step_boundary =
			a_ctrl->region_params[a_ctrl->curr_region_index].
			step_bound[dir];
		if ((dest_step_pos * sign_dir) <=
			(step_boundary * sign_dir)) {

			target_step_pos = dest_step_pos;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			rc = msm_actuator_bivcm_write_focus(a_ctrl,
				curr_lens_pos,
				&ringing_params_kernel
				[a_ctrl->curr_region_index],
				sign_dir,
				target_lens_pos);
			if (rc < 0) {
				kfree(ringing_params_kernel);
				return rc;
			}
			curr_lens_pos = target_lens_pos;
		} else {
			target_step_pos = step_boundary;
			target_lens_pos =
				a_ctrl->step_position_table[target_step_pos];
			rc = msm_actuator_bivcm_write_focus(a_ctrl,
				curr_lens_pos,
				&ringing_params_kernel
				[a_ctrl->curr_region_index],
				sign_dir,
				target_lens_pos);
			if (rc < 0) {
				kfree(ringing_params_kernel);
				return rc;
			}
			curr_lens_pos = target_lens_pos;

			a_ctrl->curr_region_index += sign_dir;
		}
		a_ctrl->curr_step_pos = target_step_pos;
	}
	/*Free the memory allocated for damping parameters*/
	kfree(ringing_params_kernel);

	move_params->curr_lens_pos = curr_lens_pos;
	a_ctrl->i2c_tbl_index = 0;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_park_lens(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	uint16_t next_lens_pos = 0;
	struct msm_camera_i2c_reg_setting reg_setting;

	a_ctrl->i2c_tbl_index = 0;
	if ((a_ctrl->curr_step_pos > a_ctrl->total_steps) ||
		(!a_ctrl->park_lens.max_step) ||
		(!a_ctrl->step_position_table) ||
		(!a_ctrl->i2c_reg_tbl) ||
		(!a_ctrl->func_tbl) ||
		(!a_ctrl->func_tbl->actuator_parse_i2c_params)) {
		pr_err("%s:%d Failed to park lens.\n",
			__func__, __LINE__);
		return 0;
	}

	if (a_ctrl->park_lens.max_step > a_ctrl->max_code_size)
		a_ctrl->park_lens.max_step = a_ctrl->max_code_size;

	next_lens_pos = a_ctrl->step_position_table[a_ctrl->curr_step_pos];
	while (next_lens_pos) {
		/* conditions which help to reduce park lens time */
		if (next_lens_pos > (a_ctrl->park_lens.max_step *
			PARK_LENS_LONG_STEP)) {
			next_lens_pos = next_lens_pos -
				(a_ctrl->park_lens.max_step *
				PARK_LENS_LONG_STEP);
		} else if (next_lens_pos > (a_ctrl->park_lens.max_step *
			PARK_LENS_MID_STEP)) {
			next_lens_pos = next_lens_pos -
				(a_ctrl->park_lens.max_step *
				PARK_LENS_MID_STEP);
		} else if (next_lens_pos > (a_ctrl->park_lens.max_step *
			PARK_LENS_SMALL_STEP)) {
			next_lens_pos = next_lens_pos -
				(a_ctrl->park_lens.max_step *
				PARK_LENS_SMALL_STEP);
		} else {
			next_lens_pos = (next_lens_pos >
				a_ctrl->park_lens.max_step) ?
				(next_lens_pos - a_ctrl->park_lens.
				max_step) : 0;
		}
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			next_lens_pos, a_ctrl->park_lens.hw_params,
			a_ctrl->park_lens.damping_delay);

		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		reg_setting.data_type = a_ctrl->i2c_data_type;

		rc = a_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
		if (rc < 0) {
			pr_err("%s Failed I2C write Line %d\n",
				__func__, __LINE__);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
		/* Use typical damping time delay to avoid tick sound */
		usleep_range(10000, 12000);
	}

	return 0;
}

static int32_t msm_actuator_bivcm_init_step_table(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	int16_t cur_code = 0;
	uint16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	uint16_t mask = 0, i = 0;
	uint32_t qvalue = 0;
	CDBG("Enter\n");

	for (; data_size > 0; data_size--) {
		max_code_size *= 2;
		mask |= (1 << i++);
	}

	a_ctrl->max_code_size = max_code_size;
	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;

	if (set_info->af_tuning_params.total_steps
		>  MAX_ACTUATOR_AF_TOTAL_STEPS) {
		pr_err("Max actuator totalsteps exceeded = %d\n",
		set_info->af_tuning_params.total_steps);
		return -EFAULT;
	}
	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);

	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;

	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		if (step_boundary >
			set_info->af_tuning_params.total_steps) {
			pr_err("invalid step_boundary = %d, max_val = %d",
				step_boundary,
				set_info->af_tuning_params.total_steps);
			kfree(a_ctrl->step_position_table);
			a_ctrl->step_position_table = NULL;
			return -EINVAL;
		}
		qvalue = a_ctrl->region_params[region_index].qvalue;
		for (; step_index <= step_boundary;
			step_index++) {
			if (qvalue > 1 && qvalue <= MAX_QVALUE)
				cur_code = step_index * code_per_step / qvalue;
			else
				cur_code = step_index * code_per_step;
			cur_code = (set_info->af_tuning_params.initial_code +
				cur_code) & mask;
			if (cur_code < max_code_size)
				a_ctrl->step_position_table[step_index] =
					cur_code;
			else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;
			}
			CDBG("step_position_table[%d] = %d\n", step_index,
				a_ctrl->step_position_table[step_index]);
		}
	}
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_actuator_init_step_table(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info)
{
	int16_t code_per_step = 0;
	uint32_t qvalue = 0;
	int16_t cur_code = 0;
	uint16_t step_index = 0, region_index = 0;
	uint16_t step_boundary = 0;
	uint32_t max_code_size = 1;
	uint16_t data_size = set_info->actuator_params.data_size;
	CDBG("Enter\n");

	/* validate the actuator state */
	if (a_ctrl->actuator_state != ACT_OPS_ACTIVE) {
		pr_err("%s:%d invalid actuator_state %d\n"
			, __func__, __LINE__, a_ctrl->actuator_state);
		return -EINVAL;
	}
	for (; data_size > 0; data_size--)
		max_code_size *= 2;

	a_ctrl->max_code_size = max_code_size;

	/* free the step_position_table to allocate a new one */
	kfree(a_ctrl->step_position_table);
	a_ctrl->step_position_table = NULL;

	if (set_info->af_tuning_params.total_steps
		>  MAX_ACTUATOR_AF_TOTAL_STEPS) {
		pr_err("Max actuator totalsteps exceeded = %d\n",
		set_info->af_tuning_params.total_steps);
		return -EFAULT;
	}
	/* Fill step position table */
	a_ctrl->step_position_table =
		kmalloc(sizeof(uint16_t) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);

	if (a_ctrl->step_position_table == NULL)
		return -ENOMEM;

	cur_code = set_info->af_tuning_params.initial_code;
	a_ctrl->step_position_table[step_index++] = cur_code;
	for (region_index = 0;
		region_index < a_ctrl->region_size;
		region_index++) {
		code_per_step =
			a_ctrl->region_params[region_index].code_per_step;
		qvalue =
			a_ctrl->region_params[region_index].qvalue;
		step_boundary =
			a_ctrl->region_params[region_index].
			step_bound[MOVE_NEAR];
		if (step_boundary >
			set_info->af_tuning_params.total_steps) {
			pr_err("invalid step_boundary = %d, max_val = %d",
				step_boundary,
				set_info->af_tuning_params.total_steps);
			kfree(a_ctrl->step_position_table);
			a_ctrl->step_position_table = NULL;
			return -EINVAL;
		}
		for (; step_index <= step_boundary;
			step_index++) {
			if (qvalue > 1 && qvalue <= MAX_QVALUE)
				cur_code = step_index * code_per_step / qvalue;
			else
				cur_code = step_index * code_per_step;
			cur_code += set_info->af_tuning_params.initial_code;
			if (cur_code < max_code_size)
				a_ctrl->step_position_table[step_index] =
					cur_code;
			else {
				for (; step_index <
					set_info->af_tuning_params.total_steps;
					step_index++)
					a_ctrl->
						step_position_table[
						step_index] =
						max_code_size;
			}
			CDBG("step_position_table[%d] = %d\n", step_index,
				a_ctrl->step_position_table[step_index]);
		}
	}
	CDBG("Exit\n");
	return 0;
}

static int32_t msm_actuator_set_default_focus(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_move_params_t *move_params)
{
	int32_t rc = 0;
	CDBG("Enter\n");

	if (a_ctrl->curr_step_pos != 0)
		rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl, move_params);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_vreg_control(struct msm_actuator_ctrl_t *a_ctrl,
							int config)
{
	int rc = 0, i, cnt;
	struct msm_actuator_vreg *vreg_cfg;

	vreg_cfg = &a_ctrl->vreg_cfg;
	cnt = vreg_cfg->num_vreg;
	if (!cnt)
		return 0;

	if (cnt >= MSM_ACTUATOR_MAX_VREGS) {
		pr_err("%s failed %d cnt %d\n", __func__, __LINE__, cnt);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++) {
		if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
			rc = msm_camera_config_single_vreg(&(a_ctrl->pdev->dev),
				&vreg_cfg->cam_vreg[i],
				(struct regulator **)&vreg_cfg->data[i],
				config);
		} else if (a_ctrl->act_device_type ==
			MSM_CAMERA_I2C_DEVICE) {
			rc = msm_camera_config_single_vreg(
				&(a_ctrl->i2c_client.client->dev),
				&vreg_cfg->cam_vreg[i],
				(struct regulator **)&vreg_cfg->data[i],
				config);
		}
	}
	return rc;
}

static int32_t msm_actuator_power_down(struct msm_actuator_ctrl_t *a_ctrl)
{
	int32_t rc = 0;
	enum msm_sensor_power_seq_gpio_t gpio;

	CDBG("Enter\n");
	if (a_ctrl->actuator_state != ACT_DISABLE_STATE) {

		if (a_ctrl->func_tbl && a_ctrl->func_tbl->actuator_park_lens) {
			rc = a_ctrl->func_tbl->actuator_park_lens(a_ctrl);
			if (rc < 0)
				pr_err("%s:%d Lens park failed.\n",
					__func__, __LINE__);
		}

		rc = msm_actuator_vreg_control(a_ctrl, 0);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			return rc;
		}

		for (gpio = SENSOR_GPIO_AF_PWDM;
			gpio < SENSOR_GPIO_MAX; gpio++) {
			if (a_ctrl->gconf &&
				a_ctrl->gconf->gpio_num_info &&
				a_ctrl->gconf->gpio_num_info->
					valid[gpio] == 1) {

				gpio_set_value_cansleep(
					a_ctrl->gconf->gpio_num_info->
						gpio_num[gpio],
					GPIOF_OUT_INIT_LOW);

				if (a_ctrl->cam_pinctrl_status) {
					rc = pinctrl_select_state(
						a_ctrl->pinctrl_info.pinctrl,
						a_ctrl->pinctrl_info.
							gpio_state_suspend);
					if (rc < 0)
						pr_err("ERR:%s:%d cannot set pin to suspend state: %d",
							__func__, __LINE__, rc);

					devm_pinctrl_put(
						a_ctrl->pinctrl_info.pinctrl);
				}
				a_ctrl->cam_pinctrl_status = 0;
				rc = msm_camera_request_gpio_table(
					a_ctrl->gconf->cam_gpio_req_tbl,
					a_ctrl->gconf->cam_gpio_req_tbl_size,
					0);
				if (rc < 0)
					pr_err("ERR:%s:Failed in selecting state in actuator power down: %d\n",
						__func__, rc);
			}
		}

		kfree(a_ctrl->step_position_table);
		a_ctrl->step_position_table = NULL;
		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
		a_ctrl->i2c_tbl_index = 0;
		a_ctrl->actuator_state = ACT_OPS_INACTIVE;
	}
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_set_position(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_position_t *set_pos)
{
	int32_t rc = 0;
	int32_t index;
	uint16_t next_lens_position;
	uint16_t delay;
	uint32_t hw_params = 0;
	struct msm_camera_i2c_reg_setting reg_setting;
	CDBG("%s Enter %d\n", __func__, __LINE__);
	if (set_pos->number_of_steps <= 0 ||
		set_pos->number_of_steps > MAX_NUMBER_OF_STEPS) {
		pr_err("num_steps out of range = %d\n",
			set_pos->number_of_steps);
		return -EFAULT;
	}

	if (!a_ctrl || !a_ctrl->func_tbl ||
		!a_ctrl->func_tbl->actuator_parse_i2c_params ||
		!a_ctrl->i2c_reg_tbl) {
		pr_err("failed. NULL actuator pointers.");
		return -EFAULT;
	}

	if (a_ctrl->actuator_state != ACT_OPS_ACTIVE) {
		pr_err("failed. Invalid actuator state.");
		return -EFAULT;
	}

	a_ctrl->i2c_tbl_index = 0;
	hw_params = set_pos->hw_params;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		next_lens_position = set_pos->pos[index];
		delay = set_pos->delay[index];
		a_ctrl->func_tbl->actuator_parse_i2c_params(a_ctrl,
			next_lens_position, hw_params, delay);

		reg_setting.reg_setting = a_ctrl->i2c_reg_tbl;
		reg_setting.size = a_ctrl->i2c_tbl_index;
		reg_setting.data_type = a_ctrl->i2c_data_type;

		rc = a_ctrl->i2c_client.i2c_func_tbl->
			i2c_write_table_w_microdelay(
			&a_ctrl->i2c_client, &reg_setting);
		if (rc < 0) {
			pr_err("%s Failed I2C write Line %d\n",
				__func__, __LINE__);
			return rc;
		}
		a_ctrl->i2c_tbl_index = 0;
	}
	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_actuator_bivcm_set_position(
	struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_position_t *set_pos)
{
	int32_t rc = 0;
	int32_t index;
	uint16_t next_lens_position;
	uint16_t delay;
	uint32_t hw_params = 0;
	CDBG("%s Enter %d\n", __func__, __LINE__);
	if (set_pos->number_of_steps <= 0 ||
		set_pos->number_of_steps > MAX_NUMBER_OF_STEPS) {
		pr_err("num_steps out of range = %d\n",
			set_pos->number_of_steps);
		return -EFAULT;
	}

	if (!a_ctrl) {
		pr_err("failed. NULL actuator pointers.");
		return -EFAULT;
	}

	if (a_ctrl->actuator_state != ACT_OPS_ACTIVE) {
		pr_err("failed. Invalid actuator state.");
		return -EFAULT;
	}

	a_ctrl->i2c_tbl_index = 0;
	hw_params = set_pos->hw_params;
	for (index = 0; index < set_pos->number_of_steps; index++) {
		next_lens_position = set_pos->pos[index];
		delay = set_pos->delay[index];
		rc = msm_actuator_bivcm_handle_i2c_ops(a_ctrl,
		next_lens_position, hw_params, delay);
		a_ctrl->i2c_tbl_index = 0;
	}
	CDBG("%s exit %d\n", __func__, __LINE__);
	return rc;
}

static int32_t msm_actuator_set_param(struct msm_actuator_ctrl_t *a_ctrl,
	struct msm_actuator_set_info_t *set_info) {
	struct reg_settings_t *init_settings = NULL;
	int32_t rc = -EFAULT;
	uint16_t i = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	CDBG("Enter\n");

	for (i = 0; i < ARRAY_SIZE(actuators); i++) {
		if (set_info->actuator_params.act_type ==
			actuators[i]->act_type) {
			a_ctrl->func_tbl = &actuators[i]->func_tbl;
			rc = 0;
		}
	}

	if (rc < 0) {
		pr_err("Actuator function table not found\n");
		return rc;
	}
	if (set_info->af_tuning_params.total_steps
		>  MAX_ACTUATOR_AF_TOTAL_STEPS) {
		pr_err("Max actuator totalsteps exceeded = %d\n",
		set_info->af_tuning_params.total_steps);
		return -EFAULT;
	}
	if (set_info->af_tuning_params.region_size
		> MAX_ACTUATOR_REGION) {
		pr_err("MAX_ACTUATOR_REGION is exceeded.\n");
		return -EFAULT;
	}

	a_ctrl->region_size = set_info->af_tuning_params.region_size;
	a_ctrl->pwd_step = set_info->af_tuning_params.pwd_step;

	if (copy_from_user(&a_ctrl->region_params,
		(void *)set_info->af_tuning_params.region_params,
		a_ctrl->region_size * sizeof(struct region_params_t))) {
		pr_err("Error copying region_params\n");
		return -EFAULT;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		cci_client = a_ctrl->i2c_client.cci_client;
		cci_client->sid =
			set_info->actuator_params.i2c_addr >> 1;
		cci_client->retries = 3;
		cci_client->id_map = 0;
		cci_client->cci_i2c_master = a_ctrl->cci_master;
		cci_client->i2c_freq_mode =
			set_info->actuator_params.i2c_freq_mode;
	} else {
		a_ctrl->i2c_client.client->addr =
			set_info->actuator_params.i2c_addr;
	}

	a_ctrl->i2c_data_type = set_info->actuator_params.i2c_data_type;
	a_ctrl->i2c_client.addr_type = set_info->actuator_params.i2c_addr_type;
	if (set_info->actuator_params.reg_tbl_size <=
		MAX_ACTUATOR_REG_TBL_SIZE) {
		a_ctrl->reg_tbl_size = set_info->actuator_params.reg_tbl_size;
	} else {
		a_ctrl->reg_tbl_size = 0;
		pr_err("MAX_ACTUATOR_REG_TBL_SIZE is exceeded.\n");
		return -EFAULT;
	}

	if ((a_ctrl->actuator_state == ACT_OPS_ACTIVE) &&
		(a_ctrl->i2c_reg_tbl != NULL)) {
		kfree(a_ctrl->i2c_reg_tbl);
	}

	a_ctrl->i2c_reg_tbl = NULL;
	a_ctrl->i2c_reg_tbl =
		kmalloc(sizeof(struct msm_camera_i2c_reg_array) *
		(set_info->af_tuning_params.total_steps + 1), GFP_KERNEL);
	if (!a_ctrl->i2c_reg_tbl) {
		pr_err("kmalloc fail\n");
		return -ENOMEM;
	}

	a_ctrl->total_steps = set_info->af_tuning_params.total_steps;

	if (copy_from_user(&a_ctrl->reg_tbl,
		(void *)set_info->actuator_params.reg_tbl_params,
		a_ctrl->reg_tbl_size *
		sizeof(struct msm_actuator_reg_params_t))) {
		kfree(a_ctrl->i2c_reg_tbl);
		a_ctrl->i2c_reg_tbl = NULL;
		return -EFAULT;
	}

	if (set_info->actuator_params.init_setting_size &&
		set_info->actuator_params.init_setting_size
		<= MAX_ACTUATOR_INIT_SET) {
		if (a_ctrl->func_tbl->actuator_init_focus) {
			init_settings = kmalloc(sizeof(struct reg_settings_t) *
				(set_info->actuator_params.init_setting_size),
				GFP_KERNEL);
			if (init_settings == NULL) {
				kfree(a_ctrl->i2c_reg_tbl);
				a_ctrl->i2c_reg_tbl = NULL;
				pr_err("Error allocating memory for init_settings\n");
				return -EFAULT;
			}
			if (copy_from_user(init_settings,
				(void *)set_info->actuator_params.init_settings,
				set_info->actuator_params.init_setting_size *
				sizeof(struct reg_settings_t))) {
				kfree(init_settings);
				kfree(a_ctrl->i2c_reg_tbl);
				a_ctrl->i2c_reg_tbl = NULL;
				pr_err("Error copying init_settings\n");
				return -EFAULT;
			}
			rc = a_ctrl->func_tbl->actuator_init_focus(a_ctrl,
				set_info->actuator_params.init_setting_size,
				init_settings);
			kfree(init_settings);
			if (rc < 0) {
				kfree(a_ctrl->i2c_reg_tbl);
				a_ctrl->i2c_reg_tbl = NULL;
				pr_err("Error actuator_init_focus\n");
				return -EFAULT;
			}
		}
	}

	/* Park lens data */
	a_ctrl->park_lens = set_info->actuator_params.park_lens;
	a_ctrl->initial_code = set_info->af_tuning_params.initial_code;
	if (a_ctrl->func_tbl->actuator_init_step_table)
		rc = a_ctrl->func_tbl->
			actuator_init_step_table(a_ctrl, set_info);

	a_ctrl->curr_step_pos = 0;
	a_ctrl->curr_region_index = 0;
	CDBG("Exit\n");

	return rc;
}

static int msm_actuator_init(struct msm_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_INIT);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	a_ctrl->actuator_state = ACT_OPS_ACTIVE;
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_config(struct msm_actuator_ctrl_t *a_ctrl,
	void __user *argp)
{
	struct msm_actuator_cfg_data *cdata =
		(struct msm_actuator_cfg_data *)argp;
	int32_t rc = -EINVAL;
	mutex_lock(a_ctrl->actuator_mutex);
	CDBG("Enter\n");
	CDBG("%s type %d\n", __func__, cdata->cfgtype);

	if (cdata->cfgtype != CFG_ACTUATOR_INIT &&
		cdata->cfgtype != CFG_ACTUATOR_POWERUP &&
		a_ctrl->actuator_state == ACT_DISABLE_STATE) {
		pr_err("actuator disabled %d\n", rc);
		mutex_unlock(a_ctrl->actuator_mutex);
		return rc;
	}

	switch (cdata->cfgtype) {
	case CFG_ACTUATOR_INIT:
		rc = msm_actuator_init(a_ctrl);
		if (rc < 0)
			pr_err("msm_actuator_init failed %d\n", rc);
		break;
	case CFG_GET_ACTUATOR_INFO:
		cdata->is_af_supported = 1;
		cdata->cfg.cam_name = a_ctrl->cam_name;
		rc = 0;
		break;

	case CFG_SET_ACTUATOR_INFO:
		rc = msm_actuator_set_param(a_ctrl, &cdata->cfg.set_info);
		if (rc < 0)
			pr_err("init table failed %d\n", rc);
		break;

	case CFG_SET_DEFAULT_FOCUS:
		if (a_ctrl->func_tbl &&
			a_ctrl->func_tbl->actuator_set_default_focus)
			rc = a_ctrl->func_tbl->actuator_set_default_focus(
				a_ctrl, &cdata->cfg.move);
		if (rc < 0)
			pr_err("move focus failed %d\n", rc);
		break;

	case CFG_MOVE_FOCUS:
		if (a_ctrl->func_tbl &&
			a_ctrl->func_tbl->actuator_move_focus)
			rc = a_ctrl->func_tbl->actuator_move_focus(a_ctrl,
				&cdata->cfg.move);
		if (rc < 0)
			pr_err("move focus failed %d\n", rc);
		break;
	case CFG_ACTUATOR_POWERDOWN:
		rc = msm_actuator_power_down(a_ctrl);
		if (rc < 0)
			pr_err("msm_actuator_power_down failed %d\n", rc);
		break;

	case CFG_SET_POSITION:
		if (a_ctrl->func_tbl &&
			a_ctrl->func_tbl->actuator_set_position)
			rc = a_ctrl->func_tbl->actuator_set_position(a_ctrl,
				&cdata->cfg.setpos);
		if (rc < 0)
			pr_err("actuator_set_position failed %d\n", rc);
		break;

	case CFG_ACTUATOR_POWERUP:
		rc = msm_actuator_power_up(a_ctrl);
		if (rc < 0)
			pr_err("Failed actuator power up%d\n", rc);
		break;

	default:
		break;
	}
	mutex_unlock(a_ctrl->actuator_mutex);
	CDBG("Exit\n");
	return rc;
}

static int32_t msm_actuator_get_subdev_id(struct msm_actuator_ctrl_t *a_ctrl,
	void *arg)
{
	uint32_t *subdev_id = (uint32_t *)arg;
	CDBG("Enter\n");
	if (!subdev_id) {
		pr_err("failed\n");
		return -EINVAL;
	}
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE)
		*subdev_id = a_ctrl->pdev->id;
	else
		*subdev_id = a_ctrl->subdev_id;

	CDBG("subdev_id %d\n", *subdev_id);
	CDBG("Exit\n");
	return 0;
}

static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_poll =  msm_camera_cci_i2c_poll,
};

static struct msm_camera_i2c_fn_t msm_sensor_qup_func_tbl = {
	.i2c_read = msm_camera_qup_i2c_read,
	.i2c_read_seq = msm_camera_qup_i2c_read_seq,
	.i2c_write = msm_camera_qup_i2c_write,
	.i2c_write_table = msm_camera_qup_i2c_write_table,
	.i2c_write_seq_table = msm_camera_qup_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_qup_i2c_write_table_w_microdelay,
	.i2c_poll = msm_camera_qup_i2c_poll,
};

static int msm_actuator_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh) {
	int rc = 0;
	struct msm_actuator_ctrl_t *a_ctrl =  v4l2_get_subdevdata(sd);
	CDBG("Enter\n");
	if (!a_ctrl) {
		pr_err("failed\n");
		return -EINVAL;
	}
	mutex_lock(a_ctrl->actuator_mutex);
	if (a_ctrl->act_device_type == MSM_CAMERA_PLATFORM_DEVICE &&
		a_ctrl->actuator_state != ACT_DISABLE_STATE) {
		rc = a_ctrl->i2c_client.i2c_func_tbl->i2c_util(
			&a_ctrl->i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_init failed\n");
	}
	kfree(a_ctrl->i2c_reg_tbl);
	a_ctrl->i2c_reg_tbl = NULL;
	a_ctrl->actuator_state = ACT_DISABLE_STATE;
	mutex_unlock(a_ctrl->actuator_mutex);
	CDBG("Exit\n");
	return rc;
}

static const struct v4l2_subdev_internal_ops msm_actuator_internal_ops = {
	.close = msm_actuator_close,
};

static long msm_actuator_subdev_ioctl(struct v4l2_subdev *sd,
			unsigned int cmd, void *arg)
{
	int rc;
	struct msm_actuator_ctrl_t *a_ctrl = v4l2_get_subdevdata(sd);
	void __user *argp = (void __user *)arg;
	CDBG("Enter\n");
	CDBG("%s:%d a_ctrl %pK argp %pK\n", __func__, __LINE__, a_ctrl, argp);
	switch (cmd) {
	case VIDIOC_MSM_SENSOR_GET_SUBDEV_ID:
		return msm_actuator_get_subdev_id(a_ctrl, argp);
	case VIDIOC_MSM_ACTUATOR_CFG:
		return msm_actuator_config(a_ctrl, argp);
	case MSM_SD_NOTIFY_FREEZE:
		return 0;
	case MSM_SD_UNNOTIFY_FREEZE:
		return 0;
	case MSM_SD_SHUTDOWN:
		if (!a_ctrl->i2c_client.i2c_func_tbl) {
			pr_err("a_ctrl->i2c_client.i2c_func_tbl NULL\n");
			return -EINVAL;
		}
		mutex_lock(a_ctrl->actuator_mutex);
		rc = msm_actuator_power_down(a_ctrl);
		if (rc < 0) {
			pr_err("%s:%d Actuator Power down failed\n",
					__func__, __LINE__);
		}
		mutex_unlock(a_ctrl->actuator_mutex);
		return msm_actuator_close(sd, NULL);
	default:
		return -ENOIOCTLCMD;
	}
}

#ifdef CONFIG_COMPAT
static long msm_actuator_subdev_do_ioctl(
	struct file *file, unsigned int cmd, void *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(vdev);
	struct msm_actuator_cfg_data32 *u32 =
		(struct msm_actuator_cfg_data32 *)arg;
	struct msm_actuator_cfg_data actuator_data;
	void *parg = arg;
	long rc;

	switch (cmd) {
	case VIDIOC_MSM_ACTUATOR_CFG32:
		cmd = VIDIOC_MSM_ACTUATOR_CFG;
		switch (u32->cfgtype) {
		case CFG_SET_ACTUATOR_INFO:
			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			actuator_data.cfg.set_info.actuator_params.act_type =
				u32->cfg.set_info.actuator_params.act_type;

			actuator_data.cfg.set_info.actuator_params
				.reg_tbl_size =
				u32->cfg.set_info.actuator_params.reg_tbl_size;

			actuator_data.cfg.set_info.actuator_params.data_size =
				u32->cfg.set_info.actuator_params.data_size;

			actuator_data.cfg.set_info.actuator_params
				.init_setting_size =
				u32->cfg.set_info.actuator_params
				.init_setting_size;

			actuator_data.cfg.set_info.actuator_params.i2c_addr =
				u32->cfg.set_info.actuator_params.i2c_addr;

			actuator_data.cfg.set_info.actuator_params.
				i2c_freq_mode =
				u32->cfg.set_info.actuator_params.i2c_freq_mode;

			actuator_data.cfg.set_info.actuator_params
				.i2c_addr_type =
				u32->cfg.set_info.actuator_params.i2c_addr_type;

			actuator_data.cfg.set_info.actuator_params
				.i2c_data_type =
				u32->cfg.set_info.actuator_params.i2c_data_type;

			actuator_data.cfg.set_info.actuator_params
				.reg_tbl_params =
				compat_ptr(
				u32->cfg.set_info.actuator_params
				.reg_tbl_params);

			actuator_data.cfg.set_info.actuator_params
				.init_settings =
				compat_ptr(
				u32->cfg.set_info.actuator_params
				.init_settings);

			actuator_data.cfg.set_info.af_tuning_params
				.initial_code =
				u32->cfg.set_info.af_tuning_params.initial_code;

			actuator_data.cfg.set_info.af_tuning_params.pwd_step =
				u32->cfg.set_info.af_tuning_params.pwd_step;

			actuator_data.cfg.set_info.af_tuning_params
				.region_size =
				u32->cfg.set_info.af_tuning_params.region_size;

			actuator_data.cfg.set_info.af_tuning_params
				.total_steps =
				u32->cfg.set_info.af_tuning_params.total_steps;

			actuator_data.cfg.set_info.af_tuning_params
				.region_params = compat_ptr(
				u32->cfg.set_info.af_tuning_params
				.region_params);

			actuator_data.cfg.set_info.actuator_params.park_lens =
				u32->cfg.set_info.actuator_params.park_lens;

			parg = &actuator_data;
			break;
		case CFG_SET_DEFAULT_FOCUS:
		case CFG_MOVE_FOCUS:
			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			actuator_data.cfg.move.dir = u32->cfg.move.dir;

			actuator_data.cfg.move.sign_dir =
				u32->cfg.move.sign_dir;

			actuator_data.cfg.move.dest_step_pos =
				u32->cfg.move.dest_step_pos;

			actuator_data.cfg.move.num_steps =
				u32->cfg.move.num_steps;

			actuator_data.cfg.move.curr_lens_pos =
				u32->cfg.move.curr_lens_pos;

			actuator_data.cfg.move.ringing_params =
				compat_ptr(u32->cfg.move.ringing_params);
			parg = &actuator_data;
			break;
		case CFG_SET_POSITION:
			actuator_data.cfgtype = u32->cfgtype;
			actuator_data.is_af_supported = u32->is_af_supported;
			memcpy(&actuator_data.cfg.setpos, &(u32->cfg.setpos),
				sizeof(struct msm_actuator_set_position_t));
			break;
		default:
			actuator_data.cfgtype = u32->cfgtype;
			parg = &actuator_data;
			break;
		}
		break;
	case VIDIOC_MSM_ACTUATOR_CFG:
		pr_err("%s: invalid cmd 0x%x received\n", __func__, cmd);
		return -EINVAL;
	}

	rc = msm_actuator_subdev_ioctl(sd, cmd, parg);

	switch (cmd) {

	case VIDIOC_MSM_ACTUATOR_CFG:

		switch (u32->cfgtype) {

		case CFG_SET_DEFAULT_FOCUS:
		case CFG_MOVE_FOCUS:
			u32->cfg.move.curr_lens_pos =
				actuator_data.cfg.move.curr_lens_pos;
			break;
		default:
			break;
		}
	}

	return rc;
}

static long msm_actuator_subdev_fops_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	return video_usercopy(file, cmd, arg, msm_actuator_subdev_do_ioctl);
}
#endif

static int32_t msm_actuator_power_up(struct msm_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;
	enum msm_sensor_power_seq_gpio_t gpio;

	CDBG("%s called\n", __func__);

	rc = msm_actuator_vreg_control(a_ctrl, 1);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return rc;
	}

	for (gpio = SENSOR_GPIO_AF_PWDM; gpio < SENSOR_GPIO_MAX; gpio++) {
		if (a_ctrl->gconf &&
			a_ctrl->gconf->gpio_num_info &&
			a_ctrl->gconf->gpio_num_info->valid[gpio] == 1) {
			rc = msm_camera_request_gpio_table(
				a_ctrl->gconf->cam_gpio_req_tbl,
				a_ctrl->gconf->cam_gpio_req_tbl_size, 1);
			if (rc < 0) {
				pr_err("ERR:%s:Failed in selecting state for actuator: %d\n",
					__func__, rc);
				return rc;
			}
			if (a_ctrl->cam_pinctrl_status) {
				rc = pinctrl_select_state(
					a_ctrl->pinctrl_info.pinctrl,
					a_ctrl->pinctrl_info.gpio_state_active);
				if (rc < 0)
					pr_err("ERR:%s:%d cannot set pin to active state: %d",
						__func__, __LINE__, rc);
			}

			gpio_set_value_cansleep(
				a_ctrl->gconf->gpio_num_info->gpio_num[gpio],
				1);
		}
	}

	/* VREG needs some delay to power up */
	usleep_range(2000, 3000);
	a_ctrl->actuator_state = ACT_ENABLE_STATE;

	CDBG("Exit\n");
	return rc;
}

static struct v4l2_subdev_core_ops msm_actuator_subdev_core_ops = {
	.ioctl = msm_actuator_subdev_ioctl,
};

static struct v4l2_subdev_ops msm_actuator_subdev_ops = {
	.core = &msm_actuator_subdev_core_ops,
};

static const struct i2c_device_id msm_actuator_i2c_id[] = {
	{"qcom,actuator", (kernel_ulong_t)NULL},
	{ }
};

static int32_t msm_actuator_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	struct msm_actuator_ctrl_t *act_ctrl_t = NULL;
	struct msm_actuator_vreg *vreg_cfg = NULL;
	CDBG("Enter\n");

	if (client == NULL) {
		pr_err("msm_actuator_i2c_probe: client is null\n");
		return -EINVAL;
	}

	act_ctrl_t = kzalloc(sizeof(struct msm_actuator_ctrl_t),
		GFP_KERNEL);
	if (!act_ctrl_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("i2c_check_functionality failed\n");
		goto probe_failure;
	}

	CDBG("client = 0x%pK\n",  client);

	rc = of_property_read_u32(client->dev.of_node, "cell-index",
		&act_ctrl_t->subdev_id);
	CDBG("cell-index %d, rc %d\n", act_ctrl_t->subdev_id, rc);
	if (rc < 0) {
		pr_err("failed rc %d\n", rc);
		goto probe_failure;
	}

	if (of_find_property(client->dev.of_node,
		"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &act_ctrl_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data(client->dev.of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			pr_err("failed rc %d\n", rc);
			goto probe_failure;
		}
	}

	act_ctrl_t->i2c_driver = &msm_actuator_i2c_driver;
	act_ctrl_t->i2c_client.client = client;
	act_ctrl_t->curr_step_pos = 0,
	act_ctrl_t->curr_region_index = 0,
	/* Set device type as I2C */
	act_ctrl_t->act_device_type = MSM_CAMERA_I2C_DEVICE;
	act_ctrl_t->i2c_client.i2c_func_tbl = &msm_sensor_qup_func_tbl;
	act_ctrl_t->act_v4l2_subdev_ops = &msm_actuator_subdev_ops;
	act_ctrl_t->actuator_mutex = &msm_actuator_mutex;
	act_ctrl_t->cam_name = act_ctrl_t->subdev_id;
	CDBG("act_ctrl_t->cam_name: %d", act_ctrl_t->cam_name);
	/* Assign name for sub device */
	snprintf(act_ctrl_t->msm_sd.sd.name, sizeof(act_ctrl_t->msm_sd.sd.name),
		"%s", act_ctrl_t->i2c_driver->driver.name);

	/* Initialize sub device */
	v4l2_i2c_subdev_init(&act_ctrl_t->msm_sd.sd,
		act_ctrl_t->i2c_client.client,
		act_ctrl_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&act_ctrl_t->msm_sd.sd, act_ctrl_t);
	act_ctrl_t->msm_sd.sd.internal_ops = &msm_actuator_internal_ops;
	act_ctrl_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	media_entity_init(&act_ctrl_t->msm_sd.sd.entity, 0, NULL, 0);
	act_ctrl_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	act_ctrl_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ACTUATOR;
	act_ctrl_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&act_ctrl_t->msm_sd);
	msm_cam_copy_v4l2_subdev_fops(&msm_actuator_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_actuator_v4l2_subdev_fops.compat_ioctl32 =
		msm_actuator_subdev_fops_ioctl;
#endif
	act_ctrl_t->msm_sd.sd.devnode->fops =
		&msm_actuator_v4l2_subdev_fops;
	act_ctrl_t->actuator_state = ACT_DISABLE_STATE;
	pr_info("msm_actuator_i2c_probe: succeeded\n");
	CDBG("Exit\n");

	return 0;

probe_failure:
	kfree(act_ctrl_t);
	return rc;
}

static int32_t msm_actuator_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_actuator_ctrl_t *msm_actuator_t = NULL;
	struct msm_actuator_vreg *vreg_cfg;
	CDBG("Enter\n");

	if (!pdev->dev.of_node) {
		pr_err("of_node NULL\n");
		return -EINVAL;
	}

	msm_actuator_t = kzalloc(sizeof(struct msm_actuator_ctrl_t),
		GFP_KERNEL);
	if (!msm_actuator_t) {
		pr_err("%s:%d failed no memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	rc = of_property_read_u32((&pdev->dev)->of_node, "cell-index",
		&pdev->id);
	CDBG("cell-index %d, rc %d\n", pdev->id, rc);
	if (rc < 0) {
		kfree(msm_actuator_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	rc = of_property_read_u32((&pdev->dev)->of_node, "qcom,cci-master",
		&msm_actuator_t->cci_master);
	CDBG("qcom,cci-master %d, rc %d\n", msm_actuator_t->cci_master, rc);
	if (rc < 0 || msm_actuator_t->cci_master >= MASTER_MAX) {
		kfree(msm_actuator_t);
		pr_err("failed rc %d\n", rc);
		return rc;
	}

	if (of_find_property((&pdev->dev)->of_node,
			"qcom,cam-vreg-name", NULL)) {
		vreg_cfg = &msm_actuator_t->vreg_cfg;
		rc = msm_camera_get_dt_vreg_data((&pdev->dev)->of_node,
			&vreg_cfg->cam_vreg, &vreg_cfg->num_vreg);
		if (rc < 0) {
			kfree(msm_actuator_t);
			pr_err("failed rc %d\n", rc);
			return rc;
		}
	}
	rc = msm_sensor_driver_get_gpio_data(&(msm_actuator_t->gconf),
		(&pdev->dev)->of_node);
	if (-ENODEV == rc) {
		pr_notice("No valid actuator GPIOs data\n");
	} else if (rc < 0) {
		pr_err("Error Actuator GPIOs\n");
	} else {
		msm_actuator_t->cam_pinctrl_status = 1;
		rc = msm_camera_pinctrl_init(
			&(msm_actuator_t->pinctrl_info), &(pdev->dev));
		if (rc < 0) {
			pr_err("ERR: Error in reading actuator pinctrl\n");
			msm_actuator_t->cam_pinctrl_status = 0;
		}
	}

	msm_actuator_t->act_v4l2_subdev_ops = &msm_actuator_subdev_ops;
	msm_actuator_t->actuator_mutex = &msm_actuator_mutex;
	msm_actuator_t->cam_name = pdev->id;

	/* Set platform device handle */
	msm_actuator_t->pdev = pdev;
	/* Set device type as platform device */
	msm_actuator_t->act_device_type = MSM_CAMERA_PLATFORM_DEVICE;
	msm_actuator_t->i2c_client.i2c_func_tbl = &msm_sensor_cci_func_tbl;
	msm_actuator_t->i2c_client.cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!msm_actuator_t->i2c_client.cci_client) {
		kfree(msm_actuator_t->vreg_cfg.cam_vreg);
		kfree(msm_actuator_t);
		pr_err("failed no memory\n");
		return -ENOMEM;
	}

	cci_client = msm_actuator_t->i2c_client.cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = msm_actuator_t->cci_master;
	v4l2_subdev_init(&msm_actuator_t->msm_sd.sd,
		msm_actuator_t->act_v4l2_subdev_ops);
	v4l2_set_subdevdata(&msm_actuator_t->msm_sd.sd, msm_actuator_t);
	msm_actuator_t->msm_sd.sd.internal_ops = &msm_actuator_internal_ops;
	msm_actuator_t->msm_sd.sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(msm_actuator_t->msm_sd.sd.name,
		ARRAY_SIZE(msm_actuator_t->msm_sd.sd.name), "msm_actuator");
	media_entity_init(&msm_actuator_t->msm_sd.sd.entity, 0, NULL, 0);
	msm_actuator_t->msm_sd.sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV;
	msm_actuator_t->msm_sd.sd.entity.group_id = MSM_CAMERA_SUBDEV_ACTUATOR;
	msm_actuator_t->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x2;
	msm_sd_register(&msm_actuator_t->msm_sd);
	msm_actuator_t->actuator_state = ACT_DISABLE_STATE;
	msm_cam_copy_v4l2_subdev_fops(&msm_actuator_v4l2_subdev_fops);
#ifdef CONFIG_COMPAT
	msm_actuator_v4l2_subdev_fops.compat_ioctl32 =
		msm_actuator_subdev_fops_ioctl;
#endif
	msm_actuator_t->msm_sd.sd.devnode->fops =
		&msm_actuator_v4l2_subdev_fops;

	CDBG("Exit\n");
	return rc;
}

static const struct of_device_id msm_actuator_i2c_dt_match[] = {
	{.compatible = "qcom,actuator"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_actuator_i2c_dt_match);

static struct i2c_driver msm_actuator_i2c_driver = {
	.id_table = msm_actuator_i2c_id,
	.probe  = msm_actuator_i2c_probe,
	.remove = __exit_p(msm_actuator_i2c_remove),
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = msm_actuator_i2c_dt_match,
	},
};

static const struct of_device_id msm_actuator_dt_match[] = {
	{.compatible = "qcom,actuator", .data = NULL},
	{}
};

MODULE_DEVICE_TABLE(of, msm_actuator_dt_match);

static struct platform_driver msm_actuator_platform_driver = {
	.probe = msm_actuator_platform_probe,
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = msm_actuator_dt_match,
	},
};

static int __init msm_actuator_init_module(void)
{
	int32_t rc = 0;
	CDBG("Enter\n");
	rc = platform_driver_register(&msm_actuator_platform_driver);

	CDBG("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&msm_actuator_i2c_driver);
}

static struct msm_actuator msm_vcm_actuator_table = {
	.act_type = ACTUATOR_VCM,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_init_step_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_set_position,
		.actuator_park_lens = msm_actuator_park_lens,
	},
};

static struct msm_actuator msm_piezo_actuator_table = {
	.act_type = ACTUATOR_PIEZO,
	.func_tbl = {
		.actuator_init_step_table = NULL,
		.actuator_move_focus = msm_actuator_piezo_move_focus,
		.actuator_write_focus = NULL,
		.actuator_set_default_focus =
			msm_actuator_piezo_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_park_lens = NULL,
	},
};

static struct msm_actuator msm_hvcm_actuator_table = {
	.act_type = ACTUATOR_HVCM,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_init_step_table,
		.actuator_move_focus = msm_actuator_move_focus,
		.actuator_write_focus = msm_actuator_write_focus,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = msm_actuator_parse_i2c_params,
		.actuator_set_position = msm_actuator_set_position,
		.actuator_park_lens = msm_actuator_park_lens,
	},
};

static struct msm_actuator msm_bivcm_actuator_table = {
	.act_type = ACTUATOR_BIVCM,
	.func_tbl = {
		.actuator_init_step_table = msm_actuator_bivcm_init_step_table,
		.actuator_move_focus = msm_actuator_bivcm_move_focus,
		.actuator_write_focus = NULL,
		.actuator_set_default_focus = msm_actuator_set_default_focus,
		.actuator_init_focus = msm_actuator_init_focus,
		.actuator_parse_i2c_params = NULL,
		.actuator_set_position = msm_actuator_bivcm_set_position,
		.actuator_park_lens = NULL,
	},
};

module_init(msm_actuator_init_module);
MODULE_DESCRIPTION("MSM ACTUATOR");
MODULE_LICENSE("GPL v2");
