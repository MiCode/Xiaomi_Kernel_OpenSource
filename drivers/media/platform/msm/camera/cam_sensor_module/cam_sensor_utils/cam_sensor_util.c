/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/kernel.h>
#include "cam_sensor_util.h"
#include <cam_mem_mgr.h>
#include "cam_res_mgr_api.h"

#define CAM_SENSOR_PINCTRL_STATE_SLEEP "cam_suspend"
#define CAM_SENSOR_PINCTRL_STATE_DEFAULT "cam_default"

#define VALIDATE_VOLTAGE(min, max, config_val) ((config_val) && \
	(config_val >= min) && (config_val <= max))

static struct i2c_settings_list*
	cam_sensor_get_i2c_ptr(struct i2c_settings_array *i2c_reg_settings,
		uint32_t size)
{
	struct i2c_settings_list *tmp;

	tmp = (struct i2c_settings_list *)
		kzalloc(sizeof(struct i2c_settings_list), GFP_KERNEL);

	if (tmp != NULL)
		list_add_tail(&(tmp->list),
			&(i2c_reg_settings->list_head));
	else
		return NULL;

	tmp->i2c_settings.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) *
		size, GFP_KERNEL);
	if (tmp->i2c_settings.reg_setting == NULL) {
		list_del(&(tmp->list));
		kfree(tmp);
		return NULL;
	}
	tmp->i2c_settings.size = size;

	return tmp;
}

int32_t delete_request(struct i2c_settings_array *i2c_array)
{
	struct i2c_settings_list *i2c_list = NULL, *i2c_next = NULL;
	int32_t rc = 0;

	if (i2c_array == NULL) {
		CAM_ERR(CAM_SENSOR, "FATAL:: Invalid argument");
		return -EINVAL;
	}

	list_for_each_entry_safe(i2c_list, i2c_next,
		&(i2c_array->list_head), list) {
		kfree(i2c_list->i2c_settings.reg_setting);
		list_del(&(i2c_list->list));
		kfree(i2c_list);
	}
	INIT_LIST_HEAD(&(i2c_array->list_head));
	i2c_array->is_settings_valid = 0;

	return rc;
}

int32_t cam_sensor_handle_delay(
	uint32_t **cmd_buf,
	uint16_t generic_op_code,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t offset, uint32_t *byte_cnt,
	struct list_head *list_ptr)
{
	int32_t rc = 0;
	struct cam_cmd_unconditional_wait *cmd_uncond_wait =
		(struct cam_cmd_unconditional_wait *) *cmd_buf;
	struct i2c_settings_list *i2c_list = NULL;

	if (list_ptr == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid list ptr");
		return -EINVAL;
	}

	if (offset > 0) {
 		i2c_list =
 			list_entry(list_ptr, struct i2c_settings_list, list);
		CAM_ERR(CAM_SENSOR, "generic_op_code: %d delay %d", generic_op_code, cmd_uncond_wait->delay);
 		if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_HW_UCND && cmd_uncond_wait->delay < 1000)
 			i2c_list->i2c_settings.
 				reg_setting[offset - 1].delay =
 				cmd_uncond_wait->delay;
 		else
		{

 			i2c_list->i2c_settings.delay =
				(cmd_uncond_wait->delay + 500) / 1000;
		}
		(*cmd_buf) +=
			sizeof(
			struct cam_cmd_unconditional_wait) / sizeof(uint32_t);
		(*byte_cnt) +=
			sizeof(
			struct cam_cmd_unconditional_wait);
	} else {
		CAM_ERR(CAM_SENSOR, "cam_sensor_handle_delay:Delay Rxed Before any buffer: %d", offset);
		return -EINVAL;
	}

	return rc;
}

int32_t cam_sensor_handle_poll(
	uint32_t **cmd_buf,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t *byte_cnt, int32_t *offset,
	struct list_head **list_ptr)
{
	struct i2c_settings_list  *i2c_list;
	int32_t rc = 0;
	struct cam_cmd_conditional_wait *cond_wait
		= (struct cam_cmd_conditional_wait *) *cmd_buf;

	i2c_list =
		cam_sensor_get_i2c_ptr(i2c_reg_settings, 1);
	if (!i2c_list || !i2c_list->i2c_settings.reg_setting) {
		CAM_ERR(CAM_SENSOR, "Failed in allocating mem for list");
		return -ENOMEM;
	}

	i2c_list->op_code = CAM_SENSOR_I2C_POLL;
	i2c_list->i2c_settings.data_type =
		cond_wait->data_type;
	i2c_list->i2c_settings.addr_type =
		cond_wait->addr_type;
	i2c_list->i2c_settings.reg_setting->reg_addr =
		cond_wait->reg_addr;
	i2c_list->i2c_settings.reg_setting->reg_data =
		cond_wait->reg_data;
	i2c_list->i2c_settings.reg_setting->delay =
		cond_wait->timeout;

	(*cmd_buf) += sizeof(struct cam_cmd_conditional_wait) /
		sizeof(uint32_t);
	(*byte_cnt) += sizeof(struct cam_cmd_conditional_wait);

	*offset = 1;
	*list_ptr = &(i2c_list->list);

	return rc;
}

int32_t cam_sensor_handle_random_write(
	struct cam_cmd_i2c_random_wr *cam_cmd_i2c_random_wr,
	struct i2c_settings_array *i2c_reg_settings,
	uint16_t *cmd_length_in_bytes, int32_t *offset,
	struct list_head **list)
{
	struct i2c_settings_list  *i2c_list;
	int32_t rc = 0, cnt;

	i2c_list = cam_sensor_get_i2c_ptr(i2c_reg_settings,
		cam_cmd_i2c_random_wr->header.count);
	if (i2c_list == NULL ||
		i2c_list->i2c_settings.reg_setting == NULL) {
		CAM_ERR(CAM_SENSOR, "Failed in allocating i2c_list");
		return -ENOMEM;
	}

	*cmd_length_in_bytes = (sizeof(struct i2c_rdwr_header) +
		sizeof(struct i2c_random_wr_payload) *
		(cam_cmd_i2c_random_wr->header.count));
	i2c_list->op_code = CAM_SENSOR_I2C_WRITE_RANDOM;
	i2c_list->i2c_settings.addr_type =
		cam_cmd_i2c_random_wr->header.addr_type;
	i2c_list->i2c_settings.data_type =
		cam_cmd_i2c_random_wr->header.data_type;

	for (cnt = 0; cnt < (cam_cmd_i2c_random_wr->header.count);
		cnt++) {
		i2c_list->i2c_settings.reg_setting[cnt].reg_addr =
			cam_cmd_i2c_random_wr->
			random_wr_payload[cnt].reg_addr;
		i2c_list->i2c_settings.
			reg_setting[cnt].reg_data =
			cam_cmd_i2c_random_wr->
			random_wr_payload[cnt].reg_data;
		i2c_list->i2c_settings.
			reg_setting[cnt].data_mask = 0;
	}
	*offset = cnt;
	*list = &(i2c_list->list);

	return rc;
}

static int32_t cam_sensor_handle_continuous_write(
	struct cam_cmd_i2c_continuous_wr *cam_cmd_i2c_continuous_wr,
	struct i2c_settings_array *i2c_reg_settings,
	uint16_t *cmd_length_in_bytes, int32_t *offset,
	struct list_head **list)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0, cnt;

	i2c_list = cam_sensor_get_i2c_ptr(i2c_reg_settings,
		cam_cmd_i2c_continuous_wr->header.count);
	if (i2c_list == NULL ||
		i2c_list->i2c_settings.reg_setting == NULL) {
		CAM_ERR(CAM_SENSOR, "Failed in allocating i2c_list");
		return -ENOMEM;
	}

	*cmd_length_in_bytes = (sizeof(struct i2c_rdwr_header) +
		sizeof(cam_cmd_i2c_continuous_wr->reg_addr) +
		sizeof(struct cam_cmd_read) *
		(cam_cmd_i2c_continuous_wr->header.count));
	if (cam_cmd_i2c_continuous_wr->header.op_code ==
		CAMERA_SENSOR_I2C_OP_CONT_WR_BRST)
		i2c_list->op_code = CAM_SENSOR_I2C_WRITE_BURST;
	else if (cam_cmd_i2c_continuous_wr->header.op_code ==
		CAMERA_SENSOR_I2C_OP_CONT_WR_SEQN)
		i2c_list->op_code = CAM_SENSOR_I2C_WRITE_SEQ;
	else
		return -EINVAL;

	i2c_list->i2c_settings.addr_type =
		cam_cmd_i2c_continuous_wr->header.addr_type;
	i2c_list->i2c_settings.data_type =
		cam_cmd_i2c_continuous_wr->header.data_type;
	i2c_list->i2c_settings.size =
		cam_cmd_i2c_continuous_wr->header.count;

	for (cnt = 0; cnt < (cam_cmd_i2c_continuous_wr->header.count);
		cnt++) {
		i2c_list->i2c_settings.reg_setting[cnt].reg_addr =
			cam_cmd_i2c_continuous_wr->
			reg_addr;
		i2c_list->i2c_settings.
			reg_setting[cnt].reg_data =
			cam_cmd_i2c_continuous_wr->
			data_read[cnt].reg_data;
		i2c_list->i2c_settings.
			reg_setting[cnt].data_mask = 0;
	}
	*offset = cnt;
	*list = &(i2c_list->list);

	return rc;
}

/**
 * Name : cam_sensor_i2c_command_parser
 * Description : Parse CSL CCI packet and apply register settings
 * Parameters :  s_ctrl  input/output    sub_device
 *              arg     input           cam_control
 * Description :
 * Handle multiple I2C RD/WR and WAIT cmd formats in one command
 * buffer, for example, a command buffer of m x RND_WR + 1 x HW_
 * WAIT + n x RND_WR with num_cmd_buf = 1. Do not exepect RD/WR
 * with different cmd_type and op_code in one command buffer.
 */
int cam_sensor_i2c_command_parser(struct i2c_settings_array *i2c_reg_settings,
	struct cam_cmd_buf_desc   *cmd_desc, int32_t num_cmd_buffers)
{
	int16_t                   rc = 0, i = 0;
	size_t                    len_of_buff = 0;
	uint64_t                  generic_ptr;

	for (i = 0; i < num_cmd_buffers; i++) {
		uint32_t                  *cmd_buf = NULL;
		struct common_header      *cmm_hdr;
		uint16_t                  generic_op_code;
		uint32_t                  byte_cnt = 0;
		uint32_t                  j = 0;
		struct list_head          *list = NULL;

		/*
		 * It is not expected the same settings to
		 * be spread across multiple cmd buffers
		 */

		CAM_DBG(CAM_SENSOR, "Total cmd Buf in Bytes: %d",
			cmd_desc[i].length);

		if (!cmd_desc[i].length)
			continue;

		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			(uint64_t *)&generic_ptr, &len_of_buff);
		cmd_buf = (uint32_t *)generic_ptr;
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"cmd hdl failed:%d, Err: %d, Buffer_len: %ld",
				cmd_desc[i].mem_handle, rc, len_of_buff);
			return rc;
		}
		cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);

		while (byte_cnt < cmd_desc[i].length) {
			cmm_hdr = (struct common_header *)cmd_buf;
			generic_op_code = cmm_hdr->third_byte;
			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR: {
				uint16_t cmd_length_in_bytes   = 0;
				struct cam_cmd_i2c_random_wr
					*cam_cmd_i2c_random_wr =
					(struct cam_cmd_i2c_random_wr *)cmd_buf;

				rc = cam_sensor_handle_random_write(
					cam_cmd_i2c_random_wr,
					i2c_reg_settings,
					&cmd_length_in_bytes, &j, &list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed in random write %d", rc);
					return rc;
				}

				cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				byte_cnt += cmd_length_in_bytes;
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_WR: {
				uint16_t cmd_length_in_bytes   = 0;
				struct cam_cmd_i2c_continuous_wr
				*cam_cmd_i2c_continuous_wr =
				(struct cam_cmd_i2c_continuous_wr *)
				cmd_buf;

				rc = cam_sensor_handle_continuous_write(
					cam_cmd_i2c_continuous_wr,
					i2c_reg_settings,
					&cmd_length_in_bytes, &j, &list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed in continuous write %d", rc);
					return rc;
				}

				cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				byte_cnt += cmd_length_in_bytes;
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_WAIT: {
				if (generic_op_code ==
					CAMERA_SENSOR_WAIT_OP_HW_UCND ||
					generic_op_code ==
						CAMERA_SENSOR_WAIT_OP_SW_UCND) {

					rc = cam_sensor_handle_delay(
						&cmd_buf, generic_op_code,
						i2c_reg_settings, j, &byte_cnt,
						list);
					if (rc < 0) {
						CAM_ERR(CAM_SENSOR,
							"delay hdl failed: %d",
							rc);
						return rc;
					}

				} else if (generic_op_code ==
					CAMERA_SENSOR_WAIT_OP_COND) {
					rc = cam_sensor_handle_poll(
						&cmd_buf, i2c_reg_settings,
						&byte_cnt, &j, &list);
					if (rc < 0) {
						CAM_ERR(CAM_SENSOR,
							"Random read fail: %d",
							rc);
						return rc;
					}
				} else {
					CAM_ERR(CAM_SENSOR,
						"Wrong Wait Command: %d",
						generic_op_code);
					return -EINVAL;
				}
				break;
			}
			default:
				CAM_ERR(CAM_SENSOR, "Invalid Command Type:%d",
					 cmm_hdr->cmd_type);
				return -EINVAL;
			}
		}
		i2c_reg_settings->is_settings_valid = 1;
	}

	return rc;
}

int32_t msm_camera_fill_vreg_params(
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_setting *power_setting,
	uint16_t power_setting_size)
{
	int32_t rc = 0, j = 0, i = 0;
	int num_vreg;

	/* Validate input parameters */
	if (!soc_info || !power_setting) {
		CAM_ERR(CAM_SENSOR, "failed: soc_info %pK power_setting %pK",
			soc_info, power_setting);
		return -EINVAL;
	}

	num_vreg = soc_info->num_rgltr;

	if ((num_vreg <= 0) || (num_vreg > CAM_SOC_MAX_REGULATOR)) {
		CAM_ERR(CAM_SENSOR, "failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	for (i = 0; i < power_setting_size; i++) {

		if (power_setting[i].seq_type < SENSOR_MCLK ||
			power_setting[i].seq_type >= SENSOR_SEQ_TYPE_MAX) {
			CAM_ERR(CAM_SENSOR, "failed: Invalid Seq type: %d",
				power_setting[i].seq_type);
			return -EINVAL;
		}

		switch (power_setting[i].seq_type) {
		case SENSOR_VDIG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vdig")) {

					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vdig", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VIO:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vio")) {
					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vio", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VANA:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vana")) {
					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vana", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VAF:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vaf")) {
					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vaf", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}

					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_CUSTOM_REG1:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_v_custom1")) {
					CAM_DBG(CAM_SENSOR,
						"i:%d j:%d cam_vcustom1", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;
		case SENSOR_CUSTOM_REG2:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_v_custom2")) {
					CAM_DBG(CAM_SENSOR,
						"i:%d j:%d cam_vcustom2", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;
		default:
			break;
		}
	}

	return rc;
}

int cam_sensor_util_request_gpio_table(
		struct cam_hw_soc_info *soc_info, int gpio_en)
{
	int rc = 0, i = 0;
	uint8_t size = 0;
	struct cam_soc_gpio_data *gpio_conf =
			soc_info->gpio_data;
	struct gpio *gpio_tbl = NULL;

	if (!gpio_conf) {
		CAM_INFO(CAM_SENSOR, "No GPIO data");
		return 0;
	}

	if (gpio_conf->cam_gpio_common_tbl_size <= 0) {
		CAM_INFO(CAM_SENSOR, "No GPIO entry");
		return -EINVAL;
	}

	gpio_tbl = gpio_conf->cam_gpio_req_tbl;
	size = gpio_conf->cam_gpio_req_tbl_size;

	if (!gpio_tbl || !size) {
		CAM_ERR(CAM_SENSOR, "invalid gpio_tbl %pK / size %d",
			gpio_tbl, size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		CAM_DBG(CAM_SENSOR, "i: %d, gpio %d dir %ld",  i,
			gpio_tbl[i].gpio, gpio_tbl[i].flags);
	}

	if (gpio_en) {
		for (i = 0; i < size; i++) {
			rc = cam_res_mgr_gpio_request(soc_info->dev,
					gpio_tbl[i].gpio,
					gpio_tbl[i].flags, gpio_tbl[i].label);
			if (rc) {
				/*
				 * After GPIO request fails, contine to
				 * apply new gpios, outout a error message
				 * for driver bringup debug
				 */
				CAM_ERR(CAM_SENSOR, "gpio %d:%s request fails",
					gpio_tbl[i].gpio, gpio_tbl[i].label);
			}
		}
	} else {
		cam_res_mgr_gpio_free_arry(soc_info->dev, gpio_tbl, size);
	}

	return rc;
}

int32_t cam_sensor_update_power_settings(void *cmd_buf,
	int cmd_length, struct cam_sensor_power_ctrl_t *power_info)
{
	int32_t rc = 0, tot_size = 0, last_cmd_type = 0;
	int32_t i = 0, pwr_up = 0, pwr_down = 0;
	void *ptr = cmd_buf, *scr;
	struct cam_cmd_power *pwr_cmd = (struct cam_cmd_power *)cmd_buf;
	struct common_header *cmm_hdr = (struct common_header *)cmd_buf;

	if (!pwr_cmd || !cmd_length) {
		CAM_ERR(CAM_SENSOR, "Invalid Args: pwr_cmd %pK, cmd_length: %d",
			pwr_cmd, cmd_length);
		return -EINVAL;
	}

	power_info->power_setting_size = 0;
	power_info->power_setting =
		(struct cam_sensor_power_setting *)
		kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_down_setting_size = 0;
	power_info->power_down_setting =
		(struct cam_sensor_power_setting *)
		kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
	if (!power_info->power_down_setting) {
		rc = -ENOMEM;
		goto free_power_settings;
	}

	while (tot_size < cmd_length) {
		if (cmm_hdr->cmd_type ==
			CAMERA_SENSOR_CMD_TYPE_PWR_UP) {
			struct cam_cmd_power *pwr_cmd =
				(struct cam_cmd_power *)ptr;

			power_info->power_setting_size += pwr_cmd->count;
			scr = ptr + sizeof(struct cam_cmd_power);
			tot_size = tot_size + sizeof(struct cam_cmd_power);

			if (pwr_cmd->count == 0)
				CAM_WARN(CAM_SENSOR, "Un expected Command");

			for (i = 0; i < pwr_cmd->count; i++, pwr_up++) {
				power_info->
					power_setting[pwr_up].seq_type =
					pwr_cmd->power_settings[i].
						power_seq_type;
				power_info->
					power_setting[pwr_up].config_val =
					pwr_cmd->power_settings[i].
						config_val_low;
				power_info->power_setting[pwr_up].delay = 0;
				if (i) {
					scr = scr +
						sizeof(
						struct cam_power_settings);
					tot_size = tot_size +
						sizeof(
						struct cam_power_settings);
				}
				if (tot_size > cmd_length) {
					CAM_ERR(CAM_SENSOR,
						"Error: Cmd Buffer is wrong");
					rc = -EINVAL;
					goto free_power_down_settings;
				}
				CAM_DBG(CAM_SENSOR,
					"Seq Type[%d]: %d Config_val: %ld",
					pwr_up,
					power_info->
						power_setting[pwr_up].seq_type,
					power_info->
						power_setting[pwr_up].
						config_val);
			}
			last_cmd_type = CAMERA_SENSOR_CMD_TYPE_PWR_UP;
			ptr = (void *) scr;
			cmm_hdr = (struct common_header *)ptr;
		} else if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_WAIT) {
			struct cam_cmd_unconditional_wait *wait_cmd =
				(struct cam_cmd_unconditional_wait *)ptr;
			if (wait_cmd->op_code ==
				CAMERA_SENSOR_WAIT_OP_SW_UCND) {
				if (last_cmd_type ==
					CAMERA_SENSOR_CMD_TYPE_PWR_UP) {
					if (pwr_up > 0)
						power_info->
							power_setting
							[pwr_up - 1].delay +=
							wait_cmd->delay;
					else
						CAM_ERR(CAM_SENSOR,
							"Delay is expected only after valid power up setting");
				} else if (last_cmd_type ==
					CAMERA_SENSOR_CMD_TYPE_PWR_DOWN) {
					if (pwr_down > 0)
						power_info->
							power_down_setting
							[pwr_down - 1].delay +=
							wait_cmd->delay;
					else
						CAM_ERR(CAM_SENSOR,
							"Delay is expected only after valid power up setting");
				}
			} else
				CAM_DBG(CAM_SENSOR, "Invalid op code: %d",
					wait_cmd->op_code);
			tot_size = tot_size +
				sizeof(struct cam_cmd_unconditional_wait);
			if (tot_size > cmd_length) {
				CAM_ERR(CAM_SENSOR, "Command Buffer is wrong");
				return -EINVAL;
			}
			scr = (void *) (wait_cmd);
			ptr = (void *)
				(scr +
				sizeof(struct cam_cmd_unconditional_wait));
			CAM_DBG(CAM_SENSOR, "ptr: %pK sizeof: %d Next: %pK",
				scr, (int32_t)sizeof(
				struct cam_cmd_unconditional_wait), ptr);

			cmm_hdr = (struct common_header *)ptr;
		} else if (cmm_hdr->cmd_type ==
			CAMERA_SENSOR_CMD_TYPE_PWR_DOWN) {
			struct cam_cmd_power *pwr_cmd =
				(struct cam_cmd_power *)ptr;

			scr = ptr + sizeof(struct cam_cmd_power);
			tot_size = tot_size + sizeof(struct cam_cmd_power);
			power_info->power_down_setting_size += pwr_cmd->count;

			if (pwr_cmd->count == 0)
				CAM_ERR(CAM_SENSOR, "Invalid Command");

			for (i = 0; i < pwr_cmd->count; i++, pwr_down++) {
				power_info->
					power_down_setting[pwr_down].
					seq_type =
					pwr_cmd->power_settings[i].
					power_seq_type;
				power_info->
					power_down_setting[pwr_down].
					config_val =
					pwr_cmd->power_settings[i].
					config_val_low;
				power_info->
					power_down_setting[pwr_down].delay = 0;
				if (i) {
					scr = scr +
						sizeof(
						struct cam_power_settings);
					tot_size =
						tot_size +
						sizeof(
						struct cam_power_settings);
				}
				if (tot_size > cmd_length) {
					CAM_ERR(CAM_SENSOR,
						"Command Buffer is wrong");
					rc = -EINVAL;
					goto free_power_down_settings;
				}
				CAM_DBG(CAM_SENSOR,
					"Seq Type[%d]: %d Config_val: %ld",
					pwr_down,
					power_info->
						power_down_setting[pwr_down].
						seq_type,
					power_info->
						power_down_setting[pwr_down].
						config_val);
			}
			last_cmd_type = CAMERA_SENSOR_CMD_TYPE_PWR_DOWN;
			ptr = (void *) scr;
			cmm_hdr = (struct common_header *)ptr;
		} else {
			CAM_ERR(CAM_SENSOR,
				"Error: Un expected Header Type: %d",
				cmm_hdr->cmd_type);
		}
	}

	return rc;
free_power_down_settings:
	kfree(power_info->power_down_setting);
free_power_settings:
	kfree(power_info->power_setting);
	return rc;
}

int cam_get_dt_power_setting_data(struct device_node *of_node,
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0, i;
	int count = 0;
	const char *seq_name = NULL;
	uint32_t *array = NULL;
	struct cam_sensor_power_setting *ps;
	int c, end;

	if (!power_info)
		return -EINVAL;

	count = of_property_count_strings(of_node, "qcom,cam-power-seq-type");
	power_info->power_setting_size = count;

	CAM_DBG(CAM_SENSOR, "qcom,cam-power-seq-type count %d", count);

	if (count <= 0)
		return 0;

	ps = kcalloc(count, sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -ENOMEM;
	power_info->power_setting = ps;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-power-seq-type", i, &seq_name);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "failed");
			goto ERROR1;
		}
		CAM_DBG(CAM_SENSOR, "seq_name[%d] = %s", i, seq_name);
		if (!strcmp(seq_name, "cam_vio")) {
			ps[i].seq_type = SENSOR_VIO;
		} else if (!strcmp(seq_name, "cam_vana")) {
			ps[i].seq_type = SENSOR_VANA;
		} else if (!strcmp(seq_name, "cam_clk")) {
			ps[i].seq_type = SENSOR_MCLK;
		} else {
			CAM_ERR(CAM_SENSOR, "unrecognized seq-type %s",
				seq_name);
			rc = -EILSEQ;
			goto ERROR1;
		}
		CAM_DBG(CAM_SENSOR, "seq_type[%d] %d", i, ps[i].seq_type);
	}

	array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!array) {
		rc = -ENOMEM;
		goto ERROR1;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-cfg-val",
		array, count);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed ");
		goto ERROR2;
	}

	for (i = 0; i < count; i++) {
		ps[i].config_val = array[i];
		CAM_DBG(CAM_SENSOR, "power_setting[%d].config_val = %ld", i,
			ps[i].config_val);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-delay",
		array, count);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed");
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		ps[i].delay = array[i];
		CAM_DBG(CAM_SENSOR, "power_setting[%d].delay = %d", i,
			ps[i].delay);
	}
	kfree(array);

	power_info->power_down_setting =
		kzalloc(sizeof(*ps) * count, GFP_KERNEL);

	if (!power_info->power_down_setting) {
		CAM_ERR(CAM_SENSOR, "failed");
		rc = -ENOMEM;
		goto ERROR1;
	}

	power_info->power_down_setting_size = count;

	end = count - 1;

	for (c = 0; c < count; c++) {
		power_info->power_down_setting[c] = ps[end];
		end--;
	}
	return rc;
ERROR2:
	kfree(array);
ERROR1:
	kfree(ps);
	return rc;
}

int cam_sensor_util_init_gpio_pin_tbl(
	struct cam_hw_soc_info *soc_info,
	struct msm_camera_gpio_num_info **pgpio_num_info)
{
	int rc = 0, val = 0;
	uint32_t gpio_array_size;
	struct device_node *of_node = NULL;
	struct cam_soc_gpio_data *gconf = NULL;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	if (!soc_info->dev) {
		CAM_ERR(CAM_SENSOR, "device node NULL");
		return -EINVAL;
	}

	of_node = soc_info->dev->of_node;

	gconf = soc_info->gpio_data;
	if (!gconf) {
		CAM_ERR(CAM_SENSOR, "No gpio_common_table is found");
		return -EINVAL;
	}

	if (!gconf->cam_gpio_common_tbl) {
		CAM_ERR(CAM_SENSOR, "gpio_common_table is not initialized");
		return -EINVAL;
	}

	gpio_array_size = gconf->cam_gpio_common_tbl_size;

	if (!gpio_array_size) {
		CAM_ERR(CAM_SENSOR, "invalid size of gpio table");
		return -EINVAL;
	}

	*pgpio_num_info = kzalloc(sizeof(struct msm_camera_gpio_num_info),
		GFP_KERNEL);
	if (!*pgpio_num_info)
		return -ENOMEM;
	gpio_num_info = *pgpio_num_info;

	rc = of_property_read_u32(of_node, "gpio-vana", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vana failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vana invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VANA] =
				gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VANA] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vana %d",
			gpio_num_info->gpio_num[SENSOR_VANA]);
	}

	rc = of_property_read_u32(of_node, "gpio-vio", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vio failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vio invalid %d", val);
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VIO] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VIO] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vio %d",
			gpio_num_info->gpio_num[SENSOR_VIO]);
	}

	rc = of_property_read_u32(of_node, "gpio-vaf", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vaf failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vaf invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VAF] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VAF] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vaf %d",
			gpio_num_info->gpio_num[SENSOR_VAF]);
	}

	rc = of_property_read_u32(of_node, "gpio-vdig", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vdig failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vdig invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VDIG] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VDIG] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vdig %d",
				gpio_num_info->gpio_num[SENSOR_VDIG]);
	}

	rc = of_property_read_u32(of_node, "gpio-reset", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-reset failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-reset invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_RESET] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_RESET] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-reset %d",
			gpio_num_info->gpio_num[SENSOR_RESET]);
	}

	rc = of_property_read_u32(of_node, "gpio-standby", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-standby failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-standby invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_STANDBY] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_STANDBY] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-standby %d",
			gpio_num_info->gpio_num[SENSOR_STANDBY]);
	}

	rc = of_property_read_u32(of_node, "gpio-af-pwdm", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-af-pwdm failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-af-pwdm invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VAF_PWDM] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VAF_PWDM] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-af-pwdm %d",
			gpio_num_info->gpio_num[SENSOR_VAF_PWDM]);
	}

	rc = of_property_read_u32(of_node, "gpio-custom1", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-custom1 failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-custom1 invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_CUSTOM_GPIO1] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-custom1 %d",
			gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1]);
	}

	rc = of_property_read_u32(of_node, "gpio-custom2", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-custom2 failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-custom2 invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_CUSTOM_GPIO2] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-custom2 %d",
			gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2]);
	} else {
		rc = 0;
	}

	return rc;

free_gpio_info:
	kfree(gpio_num_info);
	gpio_num_info = NULL;
	return rc;
}

int msm_camera_pinctrl_init(
	struct msm_pinctrl_info *sensor_pctrl, struct device *dev) {

	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		CAM_DBG(CAM_SENSOR, "Getting pinctrl handle failed");
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_active =
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_active)) {
		CAM_ERR(CAM_SENSOR,
			"Failed to get the active state pinctrl handle");
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_suspend)) {
		CAM_ERR(CAM_SENSOR,
			"Failed to get the suspend state pinctrl handle");
		return -EINVAL;
	}

	return 0;
}

int msm_cam_sensor_handle_reg_gpio(int seq_type,
	struct msm_camera_gpio_num_info *gpio_num_info, int val)
{
	int gpio_offset = -1;

	if (!gpio_num_info) {
		CAM_INFO(CAM_SENSOR, "Input Parameters are not proper");
		return 0;
	}

	CAM_DBG(CAM_SENSOR, "Seq type: %d, config: %d", seq_type, val);

	gpio_offset = seq_type;

	if (gpio_num_info->valid[gpio_offset] == 1) {
		CAM_DBG(CAM_SENSOR, "VALID GPIO offset: %d, seqtype: %d",
			 gpio_offset, seq_type);
		cam_res_mgr_gpio_set_value(
			gpio_num_info->gpio_num
			[gpio_offset], val);
	}

	return 0;
}

int cam_sensor_core_power_up(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info)
{
	int rc = 0, index = 0, no_gpio = 0, ret = 0, num_vreg, j = 0;
	int32_t vreg_idx = -1;
	struct cam_sensor_power_setting *power_setting = NULL;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	CAM_DBG(CAM_SENSOR, "Enter");
	if (!ctrl) {
		CAM_ERR(CAM_SENSOR, "Invalid ctrl handle");
		return -EINVAL;
	}

	gpio_num_info = ctrl->gpio_num_info;
	num_vreg = soc_info->num_rgltr;

	if ((num_vreg <= 0) || (num_vreg > CAM_SOC_MAX_REGULATOR)) {
		CAM_ERR(CAM_SENSOR, "failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	if (soc_info->use_shared_clk)
		cam_res_mgr_shared_clk_config(true);

	ret = msm_camera_pinctrl_init(&(ctrl->pinctrl_info), ctrl->dev);
	if (ret < 0) {
		/* Some sensor subdev no pinctrl. */
		CAM_DBG(CAM_SENSOR, "Initialization of pinctrl failed");
		ctrl->cam_pinctrl_status = 0;
	} else {
		ctrl->cam_pinctrl_status = 1;
	}

	if (cam_res_mgr_shared_pinctrl_init()) {
		CAM_ERR(CAM_SENSOR,
			"Failed to init shared pinctrl");
		return -EINVAL;
	}

	rc = cam_sensor_util_request_gpio_table(soc_info, 1);
	if (rc < 0)
		no_gpio = rc;

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
			ctrl->pinctrl_info.pinctrl,
			ctrl->pinctrl_info.gpio_state_active);
		if (ret)
			CAM_ERR(CAM_SENSOR, "cannot set pin to active state");
	}

	ret = cam_res_mgr_shared_pinctrl_select_state(true);
	if (ret)
		CAM_ERR(CAM_SENSOR,
			"Cannot set shared pin to active state");

	CAM_DBG(CAM_SENSOR, "power setting size: %d", ctrl->power_setting_size);

	for (index = 0; index < ctrl->power_setting_size; index++) {
		CAM_DBG(CAM_SENSOR, "index: %d", index);
		power_setting = &ctrl->power_setting[index];
		CAM_DBG(CAM_SENSOR, "seq_type %d", power_setting->seq_type);

		switch (power_setting->seq_type) {
		case SENSOR_MCLK:
			if (power_setting->seq_val >= soc_info->num_clk) {
				CAM_ERR(CAM_SENSOR, "clk index %d >= max %u",
					power_setting->seq_val,
					soc_info->num_clk);
				goto power_up_failed;
			}
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j],
					"cam_clk")) {
					CAM_DBG(CAM_SENSOR,
						"Enable cam_clk: %d", j);

					soc_info->rgltr[j] =
					regulator_get(
						soc_info->dev,
						soc_info->rgltr_name[j]);

					if (IS_ERR_OR_NULL(
						soc_info->rgltr[j])) {
						rc = PTR_ERR(
							soc_info->rgltr[j]);
						rc = rc ? rc : -EINVAL;
						CAM_ERR(CAM_SENSOR,
							"vreg %s %d",
							soc_info->rgltr_name[j],
							rc);
						soc_info->rgltr[j] = NULL;
					}

					rc =  cam_soc_util_regulator_enable(
					soc_info->rgltr[j],
					soc_info->rgltr_name[j],
					soc_info->rgltr_min_volt[j],
					soc_info->rgltr_max_volt[j],
					soc_info->rgltr_op_mode[j],
					soc_info->rgltr_delay[j]);

					power_setting->data[0] =
						soc_info->rgltr[j];
				}
			}
			if (power_setting->config_val)
				soc_info->clk_rate[0][power_setting->seq_val] =
					power_setting->config_val;

			for (j = 0; j < soc_info->num_clk; j++) {
				rc = cam_soc_util_clk_enable(soc_info->clk[j],
					soc_info->clk_name[j],
					soc_info->clk_rate[0][j]);
				if (rc)
					break;
			}

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "clk enable failed");
				goto power_up_failed;
			}
			break;
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:
			if (no_gpio) {
				CAM_ERR(CAM_SENSOR, "request gpio failed");
				return no_gpio;
			}
			if (!gpio_num_info) {
				CAM_ERR(CAM_SENSOR, "Invalid gpio_num_info");
				goto power_up_failed;
			}
			CAM_DBG(CAM_SENSOR, "gpio set val %d",
				gpio_num_info->gpio_num
				[power_setting->seq_type]);

			rc = msm_cam_sensor_handle_reg_gpio(
				power_setting->seq_type,
				gpio_num_info, power_setting->config_val);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Error in handling VREG GPIO");
				goto power_up_failed;
			}
			break;
		case SENSOR_VANA:
		case SENSOR_VDIG:
		case SENSOR_VIO:
		case SENSOR_VAF:
		case SENSOR_VAF_PWDM:
		case SENSOR_CUSTOM_REG1:
		case SENSOR_CUSTOM_REG2:
			if (power_setting->seq_val == INVALID_VREG)
				break;

			if (power_setting->seq_val >= CAM_VREG_MAX) {
				CAM_ERR(CAM_SENSOR, "vreg index %d >= max %d",
					power_setting->seq_val,
					CAM_VREG_MAX);
				goto power_up_failed;
			}
			if (power_setting->seq_val < num_vreg) {
				CAM_DBG(CAM_SENSOR, "Enable Regulator");
				vreg_idx = power_setting->seq_val;

				soc_info->rgltr[vreg_idx] =
					regulator_get(soc_info->dev,
						soc_info->rgltr_name[vreg_idx]);
				if (IS_ERR_OR_NULL(
					soc_info->rgltr[vreg_idx])) {
					rc = PTR_ERR(soc_info->rgltr[vreg_idx]);
					rc = rc ? rc : -EINVAL;

					CAM_ERR(CAM_SENSOR, "%s get failed %d",
						soc_info->rgltr_name[vreg_idx],
						rc);

					soc_info->rgltr[vreg_idx] = NULL;
				}

				rc =  cam_soc_util_regulator_enable(
					soc_info->rgltr[vreg_idx],
					soc_info->rgltr_name[vreg_idx],
					soc_info->rgltr_min_volt[vreg_idx],
					soc_info->rgltr_max_volt[vreg_idx],
					soc_info->rgltr_op_mode[vreg_idx],
					soc_info->rgltr_delay[vreg_idx]);

				power_setting->data[0] =
						soc_info->rgltr[vreg_idx];
			}
			else
				CAM_ERR(CAM_SENSOR, "usr_idx:%d dts_idx:%d",
					power_setting->seq_val, num_vreg);

			rc = msm_cam_sensor_handle_reg_gpio(
				power_setting->seq_type,
				gpio_num_info, 1);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Error in handling VREG GPIO");
				goto power_up_failed;
			}
			break;
		default:
			CAM_ERR(CAM_SENSOR, "error power seq type %d",
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20)
			msleep(power_setting->delay);
		else if (power_setting->delay)
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
	}

	ret = cam_res_mgr_shared_pinctrl_post_init();
	if (ret)
		CAM_ERR(CAM_SENSOR,
			"Failed to post init shared pinctrl");

	return 0;
power_up_failed:
	CAM_ERR(CAM_SENSOR, "failed");
	for (index--; index >= 0; index--) {
		CAM_DBG(CAM_SENSOR, "index %d",  index);
		power_setting = &ctrl->power_setting[index];
		CAM_DBG(CAM_SENSOR, "type %d",
			power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:
			if (!gpio_num_info)
				continue;
			if (!gpio_num_info->valid
				[power_setting->seq_type])
				continue;
			cam_res_mgr_gpio_set_value(
				gpio_num_info->gpio_num
				[power_setting->seq_type], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VANA:
		case SENSOR_VDIG:
		case SENSOR_VIO:
		case SENSOR_VAF:
		case SENSOR_VAF_PWDM:
		case SENSOR_CUSTOM_REG1:
		case SENSOR_CUSTOM_REG2:
			if (power_setting->seq_val < num_vreg) {
				CAM_DBG(CAM_SENSOR, "Disable Regulator");
				vreg_idx = power_setting->seq_val;

				rc =  cam_soc_util_regulator_disable(
					soc_info->rgltr[vreg_idx],
					soc_info->rgltr_name[vreg_idx],
					soc_info->rgltr_min_volt[vreg_idx],
					soc_info->rgltr_max_volt[vreg_idx],
					soc_info->rgltr_op_mode[vreg_idx],
					soc_info->rgltr_delay[vreg_idx]);

				power_setting->data[0] =
						soc_info->rgltr[vreg_idx];

			}
			else
				CAM_ERR(CAM_SENSOR, "seq_val:%d > num_vreg: %d",
					power_setting->seq_val, num_vreg);

			msm_cam_sensor_handle_reg_gpio(power_setting->seq_type,
				gpio_num_info, GPIOF_OUT_INIT_LOW);

			break;
		default:
			CAM_ERR(CAM_SENSOR, "error power seq type %d",
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
				ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			CAM_ERR(CAM_SENSOR, "cannot set pin to suspend state");
		cam_res_mgr_shared_pinctrl_select_state(false);
		pinctrl_put(ctrl->pinctrl_info.pinctrl);
		cam_res_mgr_shared_pinctrl_put();
	}

	if (soc_info->use_shared_clk)
		cam_res_mgr_shared_clk_config(false);

	ctrl->cam_pinctrl_status = 0;

	cam_sensor_util_request_gpio_table(soc_info, 0);

	return rc;
}

static struct cam_sensor_power_setting*
msm_camera_get_power_settings(struct cam_sensor_power_ctrl_t *ctrl,
				enum msm_camera_power_seq_type seq_type,
				uint16_t seq_val)
{
	struct cam_sensor_power_setting *power_setting, *ps = NULL;
	int idx;

	for (idx = 0; idx < ctrl->power_setting_size; idx++) {
		power_setting = &ctrl->power_setting[idx];
		if (power_setting->seq_type == seq_type &&
			power_setting->seq_val ==  seq_val) {
			ps = power_setting;
			return ps;
		}

	}

	return ps;
}

static int cam_config_mclk_reg(struct cam_sensor_power_ctrl_t *ctrl,
	struct cam_hw_soc_info *soc_info, int32_t index)
{
	int32_t num_vreg = 0, j = 0, rc = 0, idx = 0;
	struct cam_sensor_power_setting *ps = NULL;
	struct cam_sensor_power_setting *pd = NULL;

	num_vreg = soc_info->num_rgltr;

	pd = &ctrl->power_down_setting[index];

	for (j = 0; j < num_vreg; j++) {
		if (!strcmp(soc_info->rgltr_name[j], "cam_clk")) {

			ps = NULL;
			for (idx = 0; idx <
				ctrl->power_setting_size; idx++) {
				if (ctrl->power_setting[idx].
					seq_type == pd->seq_type) {
					ps = &ctrl->power_setting[idx];
					break;
				}
			}

			if (ps != NULL) {
				CAM_DBG(CAM_SENSOR, "Disable Regulator");

				rc = cam_soc_util_regulator_disable(
					soc_info->rgltr[j],
					soc_info->rgltr_name[j],
					soc_info->rgltr_min_volt[j],
					soc_info->rgltr_max_volt[j],
					soc_info->rgltr_op_mode[j],
					soc_info->rgltr_delay[j]);

					ps->data[0] =
						soc_info->rgltr[j];
			}
		}
	}

	return rc;
}

int msm_camera_power_down(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info)
{
	int index = 0, ret = 0, num_vreg = 0, i;
	struct cam_sensor_power_setting *pd = NULL;
	struct cam_sensor_power_setting *ps;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	CAM_DBG(CAM_SENSOR, "Enter");
	if (!ctrl || !soc_info) {
		CAM_ERR(CAM_SENSOR, "failed ctrl %pK",  ctrl);
		return -EINVAL;
	}

	gpio_num_info = ctrl->gpio_num_info;
	num_vreg = soc_info->num_rgltr;

	if ((num_vreg <= 0) || (num_vreg > CAM_SOC_MAX_REGULATOR)) {
		CAM_ERR(CAM_SENSOR, "failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	for (index = 0; index < ctrl->power_down_setting_size; index++) {
		CAM_DBG(CAM_SENSOR, "index %d",  index);
		pd = &ctrl->power_down_setting[index];
		ps = NULL;
		CAM_DBG(CAM_SENSOR, "type %d",  pd->seq_type);
		switch (pd->seq_type) {
		case SENSOR_MCLK:
			ret = cam_config_mclk_reg(ctrl, soc_info, index);
			if (ret < 0) {
				CAM_ERR(CAM_SENSOR,
					"config clk reg failed rc: %d", ret);
				return ret;
			}

			for (i = soc_info->num_clk - 1; i >= 0; i--) {
				cam_soc_util_clk_disable(soc_info->clk[i],
					soc_info->clk_name[i]);
			}

			break;
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:

			if (!gpio_num_info->valid[pd->seq_type])
				continue;

			cam_res_mgr_gpio_set_value(
				gpio_num_info->gpio_num
				[pd->seq_type],
				(int) pd->config_val);

			break;
		case SENSOR_VANA:
		case SENSOR_VDIG:
		case SENSOR_VIO:
		case SENSOR_VAF:
		case SENSOR_VAF_PWDM:
		case SENSOR_CUSTOM_REG1:
		case SENSOR_CUSTOM_REG2:
			if (pd->seq_val == INVALID_VREG)
				break;

			ps = msm_camera_get_power_settings(
				ctrl, pd->seq_type,
				pd->seq_val);
			if (ps) {
				if (pd->seq_val < num_vreg) {
					CAM_DBG(CAM_SENSOR,
						"Disable Regulator");
					ret =  cam_soc_util_regulator_disable(
					soc_info->rgltr[ps->seq_val],
					soc_info->rgltr_name[ps->seq_val],
					soc_info->rgltr_min_volt[ps->seq_val],
					soc_info->rgltr_max_volt[ps->seq_val],
					soc_info->rgltr_op_mode[ps->seq_val],
					soc_info->rgltr_delay[ps->seq_val]);

					ps->data[0] =
						soc_info->rgltr[ps->seq_val];
				}
				else
					CAM_ERR(CAM_SENSOR,
						"seq_val:%d > num_vreg: %d",
						 pd->seq_val,
						num_vreg);
			} else
				CAM_ERR(CAM_SENSOR,
					"error in power up/down seq");

			ret = msm_cam_sensor_handle_reg_gpio(pd->seq_type,
				gpio_num_info, GPIOF_OUT_INIT_LOW);

			if (ret < 0)
				CAM_ERR(CAM_SENSOR,
					"Error disabling VREG GPIO");
			break;
		default:
			CAM_ERR(CAM_SENSOR, "error power seq type %d",
				pd->seq_type);
			break;
		}
		if (pd->delay > 20)
			msleep(pd->delay);
		else if (pd->delay)
			usleep_range(pd->delay * 1000,
				(pd->delay * 1000) + 1000);
	}

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
				ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			CAM_ERR(CAM_SENSOR, "cannot set pin to suspend state");

		cam_res_mgr_shared_pinctrl_select_state(false);
		pinctrl_put(ctrl->pinctrl_info.pinctrl);
		cam_res_mgr_shared_pinctrl_put();
	}

	if (soc_info->use_shared_clk)
		cam_res_mgr_shared_clk_config(false);

	ctrl->cam_pinctrl_status = 0;

	cam_sensor_util_request_gpio_table(soc_info, 0);

	return 0;
}

