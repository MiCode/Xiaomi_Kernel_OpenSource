/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __EEPROM_I2C_DEV_H
#define __EEPROM_I2C_DEV_H

#include "kd_camera_feature.h"

enum EEPROM_I2C_DEV_IDX {
	I2C_DEV_IDX_1 = 0,
	I2C_DEV_IDX_2,
	I2C_DEV_IDX_3,
	I2C_DEV_IDX_MAX
};

extern int gi2c_dev_timing[I2C_DEV_IDX_MAX];

enum EEPROM_I2C_DEV_IDX get_i2c_dev_sel(enum IMGSENSOR_SENSOR_IDX idx);

#endif /* __EEPROM_I2C_DEV_H */
