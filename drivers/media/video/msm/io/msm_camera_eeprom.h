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

#include <linux/delay.h>
#include <mach/camera.h>
#include "msm_camera_i2c.h"

struct msm_camera_eeprom_client;

struct msm_camera_eeprom_fn_t {
	int32_t (*eeprom_init)
		(struct msm_camera_eeprom_client *ectrl,
		struct i2c_adapter *adapter);
	int32_t (*eeprom_release)
		(struct msm_camera_eeprom_client *ectrl);
	int32_t (*eeprom_get_data)
		(struct msm_camera_eeprom_client *ectrl,
		 struct sensor_eeprom_data_t *edata);
	void (*eeprom_set_dev_addr)
		(struct msm_camera_eeprom_client*, uint16_t*);
};

struct msm_camera_eeprom_read_t {
	uint16_t reg_addr;
	void *dest_ptr;
	uint32_t num_byte;
	uint16_t convert_endian;
};

struct msm_camera_eeprom_data_t {
	void *data;
	uint16_t size;
};

struct msm_camera_eeprom_client {
	struct msm_camera_i2c_client *i2c_client;
	uint16_t i2c_addr;
	struct msm_camera_eeprom_fn_t func_tbl;
	struct msm_camera_eeprom_read_t *read_tbl;
	uint16_t read_tbl_size;
	struct msm_camera_eeprom_data_t *data_tbl;
	uint16_t data_tbl_size;
};

int32_t msm_camera_eeprom_init(struct msm_camera_eeprom_client *ectrl,
	struct i2c_adapter *adapter);
int32_t msm_camera_eeprom_release(struct msm_camera_eeprom_client *ectrl);
int32_t msm_camera_eeprom_read(struct msm_camera_eeprom_client *ectrl,
	uint16_t reg_addr, void *data, uint32_t num_byte,
	uint16_t convert_endian);
int32_t msm_camera_eeprom_read_tbl(struct msm_camera_eeprom_client *ectrl,
	struct msm_camera_eeprom_read_t *read_tbl, uint16_t tbl_size);
int32_t msm_camera_eeprom_get_data(struct msm_camera_eeprom_client *ectrl,
	struct sensor_eeprom_data_t *edata);



