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

#include <linux/kernel.h>
#include "cam_sensor_util.h"
#include "cam_sensor_soc_api.h"

#define CAM_SENSOR_PINCTRL_STATE_SLEEP "cam_suspend"
#define CAM_SENSOR_PINCTRL_STATE_DEFAULT "cam_default"

#define VALIDATE_VOLTAGE(min, max, config_val) ((config_val) && \
	(config_val >= min) && (config_val <= max))

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

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
		pr_err("%s:%d ::FATAL:: Invalid argument\n",
			__func__, __LINE__);
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

	if (i2c_list == NULL) {
		pr_err("%s:%d Invalid list ptr\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	if (offset > 0) {
		i2c_list =
			list_entry(list_ptr, struct i2c_settings_list, list);
		if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_HW_UCND)
			i2c_list->i2c_settings.
				reg_setting[offset - 1].delay =
				cmd_uncond_wait->delay;
		else
			i2c_list->i2c_settings.delay =
				cmd_uncond_wait->delay;
		(*cmd_buf) +=
			sizeof(
			struct cam_cmd_unconditional_wait) / sizeof(uint32_t);
		(*byte_cnt) +=
			sizeof(
			struct cam_cmd_unconditional_wait);
	} else {
		pr_err("%s: %d Error: Delay Rxed Before any buffer: %d\n",
			__func__, __LINE__, offset);
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
		pr_err("%s: %d Failed in allocating mem for list\n",
			__func__, __LINE__);
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

	(*offset) += 1;
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
		pr_err("%s: %d Failed in allocating i2c_list\n",
			__func__, __LINE__);
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
	(*offset) += cnt;
	*list = &(i2c_list->list);

	return rc;
}

/**
 * Name : cam_sensor_i2c_pkt_parser
 * Description : Parse CSL CCI packet and apply register settings
 * Parameters :  s_ctrl  input/output    sub_device
 *              arg     input           cam_control
 * Description :
 * Handle multiple I2C RD/WR and WAIT cmd formats in one command
 * buffer, for example, a command buffer of m x RND_WR + 1 x HW_
 * WAIT + n x RND_WR with num_cmd_buf = 1. Do not exepect RD/WR
 * with different cmd_type and op_code in one command buffer.
 */
int cam_sensor_i2c_pkt_parser(struct i2c_settings_array *i2c_reg_settings,
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

		CDBG("%s:%d Total cmd Buf in Bytes: %d\n", __func__,
			__LINE__, cmd_desc[i].length);

		if (!cmd_desc[i].length)
			continue;

		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			(uint64_t *)&generic_ptr, &len_of_buff);
		cmd_buf = (uint32_t *)generic_ptr;
		if (rc < 0) {
			pr_err("%s:%d Failed in getting cmd hdl: %d Err: %d Buffer Len: %ld\n",
				__func__, __LINE__,
				cmd_desc[i].mem_handle, rc,
				len_of_buff);
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
					pr_err("%s:%d :Error: Failed in random read %d\n",
						__func__, __LINE__, rc);
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
						pr_err("%s:%d :Error: Failed in handling delay %d\n",
							__func__, __LINE__, rc);
						return rc;
					}

				} else if (generic_op_code ==
					CAMERA_SENSOR_WAIT_OP_COND) {
					rc = cam_sensor_handle_poll(
						&cmd_buf, i2c_reg_settings,
						&byte_cnt, &j, &list);
					if (rc < 0) {
						pr_err("%s:%d :Error: Failed in random read %d\n",
							__func__, __LINE__, rc);
						return rc;
					}
				} else {
					pr_err("%s: %d Wrong Wait Command: %d\n",
						__func__, __LINE__,
						generic_op_code);
					return -EINVAL;
				}
				break;
			}
			default:
				pr_err("%s:%d Invalid Command Type:%d\n",
					__func__, __LINE__, cmm_hdr->cmd_type);
				return -EINVAL;
			}
		}
		i2c_reg_settings->is_settings_valid = 1;
	}

	return rc;
}

int32_t msm_camera_fill_vreg_params(struct camera_vreg_t *cam_vreg,
	int num_vreg, struct cam_sensor_power_setting *power_setting,
	uint16_t power_setting_size)
{
	int32_t rc = 0, j = 0, i = 0;

	/* Validate input parameters */
	if (!cam_vreg || !power_setting) {
		pr_err("%s:%d failed: cam_vreg %pK power_setting %pK", __func__,
			__LINE__,  cam_vreg, power_setting);
		return -EINVAL;
	}

	/* Validate size of num_vreg */
	if (num_vreg <= 0) {
		pr_err("failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	for (i = 0; i < power_setting_size; i++) {
		switch (power_setting[i].seq_type) {
		case SENSOR_VDIG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name, "cam_vdig")) {
					CDBG("%s:%d i %d j %d cam_vdig\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
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
				if (!strcmp(cam_vreg[j].reg_name, "cam_vio")) {
					CDBG("%s:%d i %d j %d cam_vio\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
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
				if (!strcmp(cam_vreg[j].reg_name, "cam_vana")) {
					CDBG("%s:%d i %d j %d cam_vana\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
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
				if (!strcmp(cam_vreg[j].reg_name, "cam_vaf")) {
					CDBG("%s:%d i %d j %d cam_vaf\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
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
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_v_custom1")) {
					CDBG("%s:%d i %d j %d cam_vcustom1\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
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
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_v_custom2")) {
					CDBG("%s:%d i %d j %d cam_vcustom2\n",
						__func__, __LINE__, i, j);
					power_setting[i].seq_val = j;
					if (VALIDATE_VOLTAGE(
						cam_vreg[j].min_voltage,
						cam_vreg[j].max_voltage,
						power_setting[i].config_val)) {
						cam_vreg[j].min_voltage =
						cam_vreg[j].max_voltage =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		default: {
			pr_err("%s:%d invalid seq_val %d\n", __func__,
				__LINE__, power_setting[i].seq_val);
			break;
			}
		}
	}

	return rc;
}

int32_t msm_camera_get_dt_gpio_req_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int32_t rc = 0, i = 0;
	uint32_t count = 0, *val_array = NULL;

	if (!of_get_property(of_node, "qcom,gpio-req-tbl-num", &count))
		return 0;

	count /= sizeof(uint32_t);
	if (!count) {
		pr_err("%s qcom,gpio-req-tbl-num 0\n", __func__);
		return 0;
	}

	val_array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!val_array)
		return -ENOMEM;

	gconf->cam_gpio_req_tbl = kcalloc(count, sizeof(struct gpio),
		GFP_KERNEL);
	if (!gconf->cam_gpio_req_tbl) {
		rc = -ENOMEM;
		goto free_val_array;
	}
	gconf->cam_gpio_req_tbl_size = count;

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-num",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_gpio_req_tbl;
	}

	for (i = 0; i < count; i++) {
		if (val_array[i] >= gpio_array_size) {
			pr_err("%s gpio req tbl index %d invalid\n",
				__func__, val_array[i]);
			return -EINVAL;
		}
		gconf->cam_gpio_req_tbl[i].gpio = gpio_array[val_array[i]];
		CDBG("%s cam_gpio_req_tbl[%d].gpio = %d\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].gpio);
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-req-tbl-flags",
		val_array, count);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto free_gpio_req_tbl;
	}

	for (i = 0; i < count; i++) {
		gconf->cam_gpio_req_tbl[i].flags = val_array[i];
		CDBG("%s cam_gpio_req_tbl[%d].flags = %ld\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].flags);
	}

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,gpio-req-tbl-label", i,
			&gconf->cam_gpio_req_tbl[i].label);
		CDBG("%s cam_gpio_req_tbl[%d].label = %s\n", __func__, i,
			gconf->cam_gpio_req_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_gpio_req_tbl;
		}
	}

	kfree(val_array);

	return rc;

free_gpio_req_tbl:
	kfree(gconf->cam_gpio_req_tbl);
free_val_array:
	kfree(val_array);
	gconf->cam_gpio_req_tbl_size = 0;

	return rc;
}

int msm_camera_init_gpio_pin_tbl(struct device_node *of_node,
	struct msm_camera_gpio_conf *gconf, uint16_t *gpio_array,
	uint16_t gpio_array_size)
{
	int rc = 0, val = 0;

	gconf->gpio_num_info = kzalloc(sizeof(struct msm_camera_gpio_num_info),
		GFP_KERNEL);
	if (!gconf->gpio_num_info)
		return -ENOMEM;

	rc = of_property_read_u32(of_node, "qcom,gpio-vana", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vana failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vana invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_VANA] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_VANA] = 1;
		CDBG("%s qcom,gpio-vana %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_VANA]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vio", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vio failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vio invalid %d\n",
				__func__, __LINE__, val);
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_VIO] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_VIO] = 1;
		CDBG("%s qcom,gpio-vio %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_VIO]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vaf", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vaf failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vaf invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_VAF] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_VAF] = 1;
		CDBG("%s qcom,gpio-vaf %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_VAF]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-vdig", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-vdig failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-vdig invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_VDIG] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_VDIG] = 1;
		CDBG("%s qcom,gpio-vdig %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_VDIG]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-reset", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-reset failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-reset invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_RESET] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_RESET] = 1;
		CDBG("%s qcom,gpio-reset %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_RESET]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-standby", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-standby failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-standby invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_STANDBY] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_STANDBY] = 1;
		CDBG("%s qcom,gpio-standby %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_STANDBY]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-af-pwdm", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-af-pwdm failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-af-pwdm invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_VAF_PWDM] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_VAF_PWDM] = 1;
		CDBG("%s qcom,gpio-af-pwdm %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_VAF_PWDM]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-custom1", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-custom1 failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-custom1 invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_CUSTOM_GPIO1] = 1;
		CDBG("%s qcom,gpio-custom1 %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1]);
	}

	rc = of_property_read_u32(of_node, "qcom,gpio-custom2", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s:%d read qcom,gpio-custom2 failed rc %d\n",
				__func__, __LINE__, rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			pr_err("%s:%d qcom,gpio-custom2 invalid %d\n",
				__func__, __LINE__, val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gconf->gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2] =
			gpio_array[val];
		gconf->gpio_num_info->valid[SENSOR_CUSTOM_GPIO2] = 1;
		CDBG("%s qcom,gpio-custom2 %d\n", __func__,
			gconf->gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2]);
	} else {
		rc = 0;
	}

	return rc;

free_gpio_info:
	kfree(gconf->gpio_num_info);
	gconf->gpio_num_info = NULL;
	return rc;
}

int cam_sensor_get_dt_vreg_data(struct device_node *of_node,
	struct camera_vreg_t **cam_vreg, int *num_vreg)
{
	int rc = 0, i = 0;
	int32_t count = 0;
	uint32_t *vreg_array = NULL;
	struct camera_vreg_t *vreg = NULL;

	count = of_property_count_strings(of_node, "qcom,cam-vreg-name");
	CDBG("%s qcom,cam-vreg-name count %d\n", __func__, count);

	if (!count || (count == -EINVAL)) {
		pr_err("%s:%d number of entries is 0 or not present in dts\n",
			__func__, __LINE__);
		*num_vreg = 0;
		return 0;
	}

	vreg = kcalloc(count, sizeof(*vreg), GFP_KERNEL);
	if (!vreg)
		return -ENOMEM;

	*cam_vreg = vreg;
	*num_vreg = count;
	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-vreg-name", i,
			&vreg[i].reg_name);
		CDBG("%s reg_name[%d] = %s\n", __func__, i,
			vreg[i].reg_name);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_vreg;
		}
	}

	vreg_array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!vreg_array) {
		rc = -ENOMEM;
		goto free_vreg;
	}

	for (i = 0; i < count; i++)
		vreg[i].type = VREG_TYPE_DEFAULT;

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-type",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_vreg_array;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].type = vreg_array[i];
				CDBG("%s cam_vreg[%d].type = %d\n",
					__func__, i, vreg[i].type);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-type entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-min-voltage",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_vreg_array;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].min_voltage = vreg_array[i];
				CDBG("%s cam_vreg[%d].min_voltage = %d\n",
					__func__, i, vreg[i].min_voltage);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-min-voltage entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-max-voltage",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_vreg_array;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].max_voltage = vreg_array[i];
				CDBG("%s cam_vreg[%d].max_voltage = %d\n",
					__func__, i, vreg[i].max_voltage);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-max-voltage entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-vreg-op-mode",
		vreg_array, count);
	if (rc != -EINVAL) {
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto free_vreg_array;
		} else {
			for (i = 0; i < count; i++) {
				vreg[i].op_mode = vreg_array[i];
				CDBG("%s cam_vreg[%d].op_mode = %d\n",
					__func__, i, vreg[i].op_mode);
			}
		}
	} else {
		CDBG("%s:%d no qcom,cam-vreg-op-mode entries in dts\n",
			__func__, __LINE__);
		rc = 0;
	}

	kfree(vreg_array);

	return rc;

free_vreg_array:
	kfree(vreg_array);
free_vreg:
	kfree(vreg);
	*num_vreg = 0;

	return rc;
}

int msm_camera_pinctrl_init(
	struct msm_pinctrl_info *sensor_pctrl, struct device *dev) {

	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		pr_err("%s:%d Getting pinctrl handle failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_active =
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_active)) {
		pr_err("%s:%d Failed to get the active state pinctrl handle\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_suspend)) {
		pr_err("%s:%d Failed to get the suspend state pinctrl handle\n",
				__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

int msm_cam_sensor_handle_reg_gpio(int seq_type,
	struct msm_camera_gpio_conf *gconf, int val)
{

	int gpio_offset = -1;

	if (!gconf) {
		pr_err("ERR:%s: Input Parameters are not proper\n", __func__);
		return -EINVAL;
	}
	CDBG("%s: %d Seq type: %d, config: %d", __func__, __LINE__,
		seq_type, val);

	gpio_offset = seq_type;

	if ((gconf->gpio_num_info->valid[gpio_offset] == 1)) {
		CDBG("%s: %d VALID GPIO offset: %d, seqtype: %d\n",
			__func__, __LINE__,	gpio_offset, seq_type);
		gpio_set_value_cansleep(
			gconf->gpio_num_info->gpio_num
			[gpio_offset], val);
	}

	return 0;
}

int32_t msm_sensor_driver_get_gpio_data(
	struct msm_camera_gpio_conf **gpio_conf,
	struct device_node *of_node)
{
	int32_t                      rc = 0, i = 0;
	uint16_t                    *gpio_array = NULL;
	int16_t                     gpio_array_size = 0;
	struct msm_camera_gpio_conf *gconf = NULL;

	/* Validate input parameters */
	if (!of_node) {
		pr_err("failed: invalid param of_node %pK", of_node);
		return -EINVAL;
	}

	gpio_array_size = of_gpio_count(of_node);
	CDBG("gpio count %d\n", gpio_array_size);
	if (gpio_array_size <= 0)
		return 0;

	gconf = kzalloc(sizeof(*gconf), GFP_KERNEL);
	if (!gconf)
		return -ENOMEM;

	*gpio_conf = gconf;

	gpio_array = kcalloc(gpio_array_size, sizeof(uint16_t), GFP_KERNEL);
	if (!gpio_array)
		goto free_gpio_conf;

	for (i = 0; i < gpio_array_size; i++) {
		gpio_array[i] = of_get_gpio(of_node, i);
		CDBG("gpio_array[%d] = %d", i, gpio_array[i]);
	}
	rc = msm_camera_get_dt_gpio_req_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed in msm_camera_get_dt_gpio_req_tbl\n");
		goto free_gpio_array;
	}

	rc = msm_camera_init_gpio_pin_tbl(of_node, gconf, gpio_array,
		gpio_array_size);
	if (rc < 0) {
		pr_err("failed in msm_camera_init_gpio_pin_tbl\n");
		goto free_gpio_req_tbl;
	}
	kfree(gpio_array);

	return rc;

free_gpio_req_tbl:
	kfree(gconf->cam_gpio_req_tbl);
free_gpio_array:
	kfree(gpio_array);
free_gpio_conf:
	kfree(gconf);
	*gpio_conf = NULL;

	return rc;
}

int cam_sensor_core_power_up(struct cam_sensor_power_ctrl_t *ctrl)
{
	int rc = 0, index = 0, no_gpio = 0, ret = 0, num_vreg, j = 0;
	struct cam_sensor_power_setting *power_setting = NULL;
	struct camera_vreg_t *cam_vreg;

	CDBG("%s:%d\n", __func__, __LINE__);
	if (!ctrl) {
		pr_err("failed ctrl %pK\n", ctrl);
		return -EINVAL;
	}

	cam_vreg = ctrl->cam_vreg;
	num_vreg = ctrl->num_vreg;

	if (ctrl->gpio_conf->cam_gpiomux_conf_tbl != NULL)
		CDBG("%s:%d mux install\n", __func__, __LINE__);

	ret = msm_camera_pinctrl_init(&(ctrl->pinctrl_info), ctrl->dev);
	if (ret < 0) {
		pr_err("%s:%d Initialization of pinctrl failed\n",
				__func__, __LINE__);
		ctrl->cam_pinctrl_status = 0;
	} else {
		ctrl->cam_pinctrl_status = 1;
	}
	rc = msm_camera_request_gpio_table(
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0)
		no_gpio = rc;
	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
			ctrl->pinctrl_info.gpio_state_active);
		if (ret)
			pr_err("%s:%d cannot set pin to active state",
				__func__, __LINE__);
	}

	for (index = 0; index < ctrl->power_setting_size; index++) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &ctrl->power_setting[index];

		switch (power_setting->seq_type) {
		case SENSOR_MCLK:
			if (power_setting->seq_val >= ctrl->clk_info_size) {
				pr_err("%s:%d :Error: clk index %d >= max %zu\n",
					__func__, __LINE__,
					power_setting->seq_val,
					ctrl->clk_info_size);
				goto power_up_failed;
			}
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(cam_vreg[j].reg_name,
					"cam_clk")) {
					CDBG("%s:%d Enable cam_clk: %d\n",
						__func__, __LINE__, j);
					msm_camera_config_single_vreg(ctrl->dev,
						&cam_vreg[j],
						(struct regulator **)
						&power_setting->data[0],
						1);
				}
			}
			if (power_setting->config_val)
				ctrl->clk_info[power_setting->seq_val].
					clk_rate = power_setting->config_val;
			rc = msm_camera_clk_enable(ctrl->dev,
				ctrl->clk_info, ctrl->clk_ptr,
				ctrl->clk_info_size, true);
			if (rc < 0) {
				pr_err("%s: clk enable failed\n", __func__);
				goto power_up_failed;
			}
			break;
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:
			if (no_gpio) {
				pr_err("%s: request gpio failed\n", __func__);
				return no_gpio;
			}
			if (power_setting->seq_val >= CAM_VREG_MAX ||
				!ctrl->gpio_conf->gpio_num_info) {
				pr_err("%s gpio index %d >= max %d\n", __func__,
					power_setting->seq_val,
					CAM_VREG_MAX);
				goto power_up_failed;
			}
			CDBG("%s:%d gpio set val %d\n",
				__func__, __LINE__,
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val]);

			rc = msm_cam_sensor_handle_reg_gpio(
				power_setting->seq_type,
				ctrl->gpio_conf, 1);
			if (rc < 0) {
				pr_err("ERR:%s Error in handling VREG GPIO\n",
					__func__);
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
				pr_err("%s vreg index %d >= max %d\n", __func__,
					power_setting->seq_val,
					CAM_VREG_MAX);
				goto power_up_failed;
			}
			if (power_setting->seq_val < ctrl->num_vreg)
				msm_camera_config_single_vreg(ctrl->dev,
					&ctrl->cam_vreg
					[power_setting->seq_val],
					(struct regulator **)
					&power_setting->data[0],
					1);
			else
				pr_err("%s: %d usr_idx:%d dts_idx:%d\n",
					__func__, __LINE__,
					power_setting->seq_val, ctrl->num_vreg);

			rc = msm_cam_sensor_handle_reg_gpio(
				power_setting->seq_type,
				ctrl->gpio_conf, 1);
			if (rc < 0) {
				pr_err("ERR:%s Error in handling VREG GPIO\n",
					__func__);
				goto power_up_failed;
			}
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20)
			msleep(power_setting->delay);
		else if (power_setting->delay)
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
	}

	return 0;
power_up_failed:
	pr_err("%s:%d failed\n", __func__, __LINE__);
	for (index--; index >= 0; index--) {
		CDBG("%s index %d\n", __func__, index);
		power_setting = &ctrl->power_setting[index];
		CDBG("%s type %d\n", __func__, power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:
			if (!ctrl->gpio_conf->gpio_num_info)
				continue;
			if (!ctrl->gpio_conf->gpio_num_info->valid
				[power_setting->seq_val])
				continue;
			gpio_set_value_cansleep(
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[power_setting->seq_val], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VANA:
		case SENSOR_VDIG:
		case SENSOR_VIO:
		case SENSOR_VAF:
		case SENSOR_VAF_PWDM:
		case SENSOR_CUSTOM_REG1:
		case SENSOR_CUSTOM_REG2:
			if (power_setting->seq_val < ctrl->num_vreg)
				msm_camera_config_single_vreg(ctrl->dev,
					&ctrl->cam_vreg
					[power_setting->seq_val],
					(struct regulator **)
					&power_setting->data[0],
					0);
			else
				pr_err("%s:%d:seq_val: %d > num_vreg: %d\n",
					__func__, __LINE__,
					power_setting->seq_val, ctrl->num_vreg);

			msm_cam_sensor_handle_reg_gpio(power_setting->seq_type,
				ctrl->gpio_conf, GPIOF_OUT_INIT_LOW);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
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
		ret = pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			pr_err("%s:%d cannot set pin to suspend state\n",
				__func__, __LINE__);
		devm_pinctrl_put(ctrl->pinctrl_info.pinctrl);
	}
	ctrl->cam_pinctrl_status = 0;
	msm_camera_request_gpio_table(
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);

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
	int32_t index)
{
	struct camera_vreg_t *cam_vreg;
	int32_t num_vreg = 0, j = 0, rc = 0, idx = 0;
	struct cam_sensor_power_setting *ps = NULL;
	struct cam_sensor_power_setting *pd = NULL;

	cam_vreg = ctrl->cam_vreg;
	num_vreg = ctrl->num_vreg;
	pd = &ctrl->power_down_setting[index];

	for (j = 0; j < num_vreg; j++) {
		if (!strcmp(cam_vreg[j].reg_name, "cam_clk")) {

			ps = NULL;
			for (idx = 0; idx <
				ctrl->power_setting_size; idx++) {
				if (ctrl->power_setting[idx].
					seq_type == pd->seq_type) {
					ps = &ctrl->power_setting[idx];
					break;
				}
			}

			if (ps != NULL)
				msm_camera_config_single_vreg(
					ctrl->dev,
					&cam_vreg[j],
					(struct regulator **)
					&ps->data[0], 0);
		}
	}

	return rc;
}

int msm_camera_power_down(struct cam_sensor_power_ctrl_t *ctrl)
{
	int index = 0, ret = 0, num_vreg = 0;
	struct cam_sensor_power_setting *pd = NULL;
	struct cam_sensor_power_setting *ps;
	struct camera_vreg_t *cam_vreg;

	CDBG("%s:%d\n", __func__, __LINE__);
	if (!ctrl) {
		pr_err("failed ctrl %pK\n", ctrl);
		return -EINVAL;
	}

	cam_vreg = ctrl->cam_vreg;
	num_vreg = ctrl->num_vreg;

	for (index = 0; index < ctrl->power_down_setting_size; index++) {
		CDBG("%s index %d\n", __func__, index);
		pd = &ctrl->power_down_setting[index];
		ps = NULL;
		CDBG("%s type %d\n", __func__, pd->seq_type);
		switch (pd->seq_type) {
		case SENSOR_MCLK:
			ret = cam_config_mclk_reg(ctrl, index);
			if (ret < 0) {
				pr_err("%s:%d :Error: in config clk reg\n",
					__func__, __LINE__);
				return ret;
			}
			msm_camera_clk_enable(ctrl->dev,
				ctrl->clk_info, ctrl->clk_ptr,
				ctrl->clk_info_size, false);
			break;
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:
			if (!ctrl->gpio_conf->gpio_num_info->valid
				[pd->seq_val])
				continue;
			gpio_set_value_cansleep(
				ctrl->gpio_conf->gpio_num_info->gpio_num
				[pd->seq_val],
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
				if (pd->seq_val < ctrl->num_vreg)
					msm_camera_config_single_vreg(ctrl->dev,
						&ctrl->cam_vreg
						[pd->seq_val],
						(struct regulator **)
						&ps->data[0],
						0);
				else
					pr_err("%s:%d:seq_val:%d > num_vreg: %d\n",
						__func__, __LINE__, pd->seq_val,
						ctrl->num_vreg);
			} else
				pr_err("%s error in power up/down seq data\n",
								__func__);
			ret = msm_cam_sensor_handle_reg_gpio(pd->seq_type,
				ctrl->gpio_conf, GPIOF_OUT_INIT_LOW);
			if (ret < 0)
				pr_err("ERR:%s Error while disabling VREG GPIO\n",
					__func__);
			break;
		default:
			pr_err("%s error power seq type %d\n", __func__,
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
		ret = pinctrl_select_state(ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			pr_err("%s:%d cannot set pin to suspend state",
				__func__, __LINE__);
		devm_pinctrl_put(ctrl->pinctrl_info.pinctrl);
	}

	ctrl->cam_pinctrl_status = 0;
	msm_camera_request_gpio_table(
		ctrl->gpio_conf->cam_gpio_req_tbl,
		ctrl->gpio_conf->cam_gpio_req_tbl_size, 0);

	return 0;
}

