/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
