/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SENSOR_I2C_H_
#define _CAM_SENSOR_I2C_H_

#include <linux/delay.h>
#include <media/v4l2-subdev.h>
#include <media/cam_sensor.h>
#include "cam_cci_dev.h"
#include "cam_sensor_io.h"

#define I2C_POLL_TIME_MS 5
#define MAX_POLL_DELAY_MS 100

#define I2C_COMPARE_MATCH 0
#define I2C_COMPARE_MISMATCH 1

#define I2C_REG_DATA_MAX       (8*1024)

/**
 * @client: CCI client structure
 * @data: I2C data
 * @addr_type: I2c address type
 * @data_type: I2C data type
 *
 * This API handles CCI read
 */
int32_t cam_cci_i2c_read(struct cam_sensor_cci_client *client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);

/**
 * @client: CCI client structure
 * @addr: I2c address
 * @data: I2C data
 * @addr_type: I2c address type
 * @data_type: I2c data type
 * @num_byte: number of bytes
 *
 * This API handles CCI sequential read
 */
int32_t cam_camera_cci_i2c_read_seq(struct cam_sensor_cci_client *client,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t num_byte);

/**
 * @client: CCI client structure
 * @write_setting: I2C register setting
 *
 * This API handles CCI random write
 */
int32_t cam_cci_i2c_write_table(
	struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting);

/**
 * @client: CCI client structure
 * @write_setting: I2C register setting
 * @cam_sensor_i2c_write_flag: burst or seq write
 *
 * This API handles CCI continuous write
 */
int32_t cam_cci_i2c_write_continuous_table(
	struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting,
	uint8_t cam_sensor_i2c_write_flag);

/**
 * @cci_client: CCI client structure
 * @cci_cmd: CCI command type
 *
 * Does I2C call to I2C functionalities
 */
int32_t cam_sensor_cci_i2c_util(struct cam_sensor_cci_client *cci_client,
	uint16_t cci_cmd);

/**
 * @client: CCI client structure
 * @addr: I2C address
 * @data: I2C data
 * @data_mask: I2C data mask
 * @data_type: I2C data type
 * @addr_type: I2C addr type
 * @delay_ms: Delay in milli seconds
 *
 * This API implements CCI based I2C poll
 */
int32_t cam_cci_i2c_poll(struct cam_sensor_cci_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type data_type,
	enum camera_sensor_i2c_type addr_type,
	uint32_t delay_ms);


/**
 * cam_qup_i2c_read : QUP based i2c read
 * @client    : QUP I2C client structure
 * @data      : I2C data
 * @addr_type : I2c address type
 * @data_type : I2C data type
 *
 * This API handles QUP I2C read
 */

int32_t cam_qup_i2c_read(struct i2c_client *client,
	uint32_t addr, uint32_t *data,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type);

/**
 * cam_qup_i2c_read_seq : QUP based I2C sequential read
 * @client    : QUP I2C client structure
 * @data      : I2C data
 * @addr_type : I2c address type
 * @num_bytes : number of bytes to read
 * This API handles QUP I2C Sequential read
 */

int32_t cam_qup_i2c_read_seq(struct i2c_client *client,
	uint32_t addr, uint8_t *data,
	enum camera_sensor_i2c_type addr_type,
	uint32_t num_byte);

/**
 * cam_qup_i2c_poll : QUP based I2C poll operation
 * @client    : QUP I2C client structure
 * @addr      : I2C address
 * @data      : I2C data
 * @data_mask : I2C data mask
 * @data_type : I2C data type
 * @addr_type : I2C addr type
 * @delay_ms  : Delay in milli seconds
 *
 * This API implements QUP based I2C poll
 */

int32_t cam_qup_i2c_poll(struct i2c_client *client,
	uint32_t addr, uint16_t data, uint16_t data_mask,
	enum camera_sensor_i2c_type addr_type,
	enum camera_sensor_i2c_type data_type,
	uint32_t delay_ms);

/**
 * cam_qup_i2c_write_table : QUP based I2C write random
 * @client        : QUP I2C client structure
 * @write_setting : I2C register settings
 *
 * This API handles QUP I2C random write
 */

int32_t cam_qup_i2c_write_table(
	struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting);

/**
 * cam_qup_i2c_write_continuous_write: QUP based I2C write continuous(Burst/Seq)
 * @client: QUP I2C client structure
 * @write_setting: I2C register setting
 * @cam_sensor_i2c_write_flag: burst or seq write
 *
 * This API handles QUP continuous write
 */
int32_t cam_qup_i2c_write_continuous_table(
	struct camera_io_master *client,
	struct cam_sensor_i2c_reg_setting *write_setting,
	uint8_t cam_sensor_i2c_write_flag);

#endif /*_CAM_SENSOR_I2C_H*/
