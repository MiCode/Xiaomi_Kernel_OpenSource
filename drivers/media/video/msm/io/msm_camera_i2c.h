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
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <mach/camera.h>

#define CONFIG_MSM_CAMERA_I2C_DBG 0

#if CONFIG_MSM_CAMERA_I2C_DBG
#define S_I2C_DBG(fmt, args...) printk(fmt, ##args)
#else
#define S_I2C_DBG(fmt, args...) CDBG(fmt, ##args)
#endif

enum msm_camera_i2c_reg_addr_type {
	MSM_CAMERA_I2C_BYTE_ADDR = 1,
	MSM_CAMERA_I2C_WORD_ADDR,
};

struct msm_camera_i2c_client {
	struct i2c_client *client;
	enum msm_camera_i2c_reg_addr_type addr_type;
};

struct msm_camera_i2c_reg_conf {
	uint16_t reg_addr;
	uint16_t reg_data;
};

enum msm_camera_i2c_data_type {
	MSM_CAMERA_I2C_BYTE_DATA = 1,
	MSM_CAMERA_I2C_WORD_DATA,
	MSM_CAMERA_I2C_HYBRID_DATA,
};

struct msm_camera_i2c_reg_dt_conf {
	uint16_t reg_addr;
	uint16_t reg_data;
	enum msm_camera_i2c_data_type dt;
};

struct msm_camera_i2c_conf_array {
	void *conf;
	uint16_t size;
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

int32_t msm_camera_i2c_write_tbl(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint8_t size,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_i2c_write_dt_tbl(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_dt_conf *reg_conf_tbl, uint16_t size);

int32_t msm_sensor_write_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_conf_array *array, uint16_t index);

int32_t msm_sensor_write_all_conf_array(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_conf_array *array, uint16_t size);

