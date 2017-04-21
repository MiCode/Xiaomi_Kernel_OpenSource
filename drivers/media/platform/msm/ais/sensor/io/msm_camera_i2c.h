/* Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
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

#ifndef MSM_CAMERA_CCI_I2C_H
#define MSM_CAMERA_CCI_I2C_H

#include <linux/delay.h>
#include <media/v4l2-subdev.h>
#include <media/ais/msm_ais_sensor.h>

#define I2C_POLL_TIME_MS 5
#define MAX_POLL_DELAY_MS 100

#define I2C_COMPARE_MATCH 0
#define I2C_COMPARE_MISMATCH 1

struct msm_camera_i2c_client {
	struct msm_camera_i2c_fn_t *i2c_func_tbl;
	struct i2c_client *client;
	struct msm_camera_cci_client *cci_client;
	struct msm_camera_spi_client *spi_client;
	enum msm_camera_i2c_reg_addr_type addr_type;
};

struct msm_camera_i2c_fn_t {
	int (*i2c_read)(struct msm_camera_i2c_client *, uint32_t, uint16_t *,
		enum msm_camera_i2c_data_type);
	int32_t (*i2c_read_seq)(struct msm_camera_i2c_client *, uint32_t,
		uint8_t *, uint32_t);
	int (*i2c_write)(struct msm_camera_i2c_client *, uint32_t, uint16_t,
		enum msm_camera_i2c_data_type);
	int (*i2c_write_seq)(struct msm_camera_i2c_client *, uint32_t,
		uint8_t *, uint32_t);
	int32_t (*i2c_write_table)(struct msm_camera_i2c_client *,
		struct msm_camera_i2c_reg_setting *);
	int32_t (*i2c_write_seq_table)(struct msm_camera_i2c_client *,
		struct msm_camera_i2c_seq_reg_setting *);
	int32_t (*i2c_write_table_w_microdelay)
		(struct msm_camera_i2c_client *,
		struct msm_camera_i2c_reg_setting *);
	int32_t (*i2c_util)(struct msm_camera_i2c_client *, uint16_t);
	int32_t (*i2c_write_conf_tbl)(struct msm_camera_i2c_client *client,
		struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
		enum msm_camera_i2c_data_type data_type);
	int32_t (*i2c_poll)(struct msm_camera_i2c_client *client,
		uint32_t addr, uint16_t data,
		enum msm_camera_i2c_data_type data_type, uint32_t delay_ms);
	int32_t (*i2c_read_burst)(struct msm_camera_i2c_client *client,
		uint32_t read_byte, uint8_t *buffer, uint32_t addr,
		enum msm_camera_i2c_data_type data_type);
	int32_t (*i2c_write_burst)(struct msm_camera_i2c_client *client,
		struct msm_camera_i2c_reg_array *reg_setting, uint32_t reg_size,
		uint32_t buf_len, uint32_t addr,
		enum msm_camera_i2c_data_type data_type);
	int32_t (*i2c_write_table_async)(struct msm_camera_i2c_client *,
		struct msm_camera_i2c_reg_setting *);
	int32_t (*i2c_write_table_sync)(struct msm_camera_i2c_client *,
		struct msm_camera_i2c_reg_setting *);
	int32_t (*i2c_write_table_sync_block)(struct msm_camera_i2c_client *,
		struct msm_camera_i2c_reg_setting *);
};

int32_t msm_camera_cci_i2c_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_cci_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_cci_i2c_write(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_cci_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_cci_i2c_write_table(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_cci_i2c_write_table_async(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_cci_i2c_write_table_sync(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_cci_i2c_write_table_sync_block(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_cci_i2c_write_seq_table(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_seq_reg_setting *write_setting);

int32_t msm_camera_cci_i2c_write_table_w_microdelay(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_cci_i2c_write_conf_tbl(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_sensor_cci_i2c_util(struct msm_camera_i2c_client *client,
	uint16_t cci_cmd);

int32_t msm_camera_cci_i2c_poll(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type, uint32_t delay_ms);

int32_t msm_camera_qup_i2c_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_qup_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_qup_i2c_write(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_qup_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_qup_i2c_write_table(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_qup_i2c_write_seq_table(struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_seq_reg_setting *write_setting);

int32_t msm_camera_qup_i2c_write_table_w_microdelay(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_qup_i2c_write_conf_tbl(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_qup_i2c_poll(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type, uint32_t delay_ms);

int32_t msm_camera_tz_i2c_register_sensor(void *s_ctrl_p);

int32_t msm_camera_tz_i2c_power_up(struct msm_camera_i2c_client *client);

int32_t msm_camera_tz_i2c_power_down(struct msm_camera_i2c_client *client);

int32_t msm_camera_tz_i2c_read(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t *data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_tz_i2c_read_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_tz_i2c_write(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_camera_tz_i2c_write_seq(struct msm_camera_i2c_client *client,
	uint32_t addr, uint8_t *data, uint32_t num_byte);

int32_t msm_camera_tz_i2c_write_table(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_tz_i2c_write_table_async(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_tz_i2c_write_table_sync(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_tz_i2c_write_table_sync_block(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_tz_i2c_write_seq_table(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_seq_reg_setting *write_setting);

int32_t msm_camera_tz_i2c_write_table_w_microdelay(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_setting *write_setting);

int32_t msm_camera_tz_i2c_write_conf_tbl(
	struct msm_camera_i2c_client *client,
	struct msm_camera_i2c_reg_conf *reg_conf_tbl, uint16_t size,
	enum msm_camera_i2c_data_type data_type);

int32_t msm_sensor_tz_i2c_util(struct msm_camera_i2c_client *client,
	uint16_t cci_cmd);

int32_t msm_camera_tz_i2c_poll(struct msm_camera_i2c_client *client,
	uint32_t addr, uint16_t data,
	enum msm_camera_i2c_data_type data_type);

#endif
