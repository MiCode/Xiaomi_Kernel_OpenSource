/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#ifndef MSM_CAMERA_I2C_H
#define MSM_CAMERA_I2C_H

#include <linux/i2c.h>
#include <linux/delay.h>
#include <mach/camera.h>
#include <media/v4l2-subdev.h>
#include <media/msm_camera.h>

#define CONFIG_MSM_CAMERA_I2C_DBG 0

#if CONFIG_MSM_CAMERA_I2C_DBG
#define S_I2C_DBG(fmt, args...) printk(fmt, ##args)
#else
#define S_I2C_DBG(fmt, args...) CDBG(fmt, ##args)
#endif

struct msm_camera_i2c_client {
	struct i2c_client *client;
	struct msm_camera_cci_client *cci_client;
	enum msm_camera_i2c_reg_addr_type addr_type;
};

struct msm_camera_i2c_reg_tbl {
	uint16_t reg_addr;
	uint16_t reg_data;
	uint16_t delay;
};

struct msm_camera_i2c_conf_array {
	struct msm_camera_i2c_reg_conf *conf;
	uint16_t size;
	uint16_t delay;
	enum msm_camera_i2c_data_type data_type;
};

struct msm_camera_i2c_enum_conf_array {
	struct msm_camera_i2c_conf_array *conf;
	int *conf_enum;
	uint16_t num_enum;
	uint16_t num_index;
	uint16_t num_conf;
	uint16_t delay;
	enum msm_camera_i2c_data_type data_type;
};

int32_t msm_camera_i2c_rxdata(struct msm_camera_i2c_client *client,
	unsigned char *rxdata, int data_length);

int32_t msm_camera_i2c_txdata(struct msm_camera_i2c_client *client,
	unsigned char *txdata, int length);

int32_t msm_camera_i2c_read(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint16_t addr, uint8_t *data, uint16_t num_byte);

int32_t msm_camera_i2c_write(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint16_t addr, uint8_t *data, uint16_t num_byte);

int32_t msm_camera_i2c_set_mask(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t mask,
	enum msm_camera_i2c_data_type data_type, uint16_t flag);

int32_t msm_camera_i2c_compare(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_poll(struct msm_camera_i2c_client *client,
	uint16_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_write_table_w_microdelay(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_tbl *reg_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_write_bayer_table(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_i2c_write_tbl(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_sensor_write_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_conf_array *array, uint16_t index);

int32_t msm_sensor_write_enum_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_enum_conf_array *conf, uint16_t enum_val);

int32_t msm_sensor_write_all_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_conf_array *array, uint16_t size);

int32_t msm_sensor_cci_util(struct msm_camera_i2c_client *client,
	uint16_t cci_cmd);
#endif
