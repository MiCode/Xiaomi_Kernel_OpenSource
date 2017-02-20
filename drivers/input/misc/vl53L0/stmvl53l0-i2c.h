/*
 *  stmvl53l0-i2c.h - Linux kernel modules for STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
/*
 * Defines
 */
#ifndef STMVL53L0_I2C_H
#define STMVL53L0_I2C_H
#include <linux/types.h>

#ifndef CAMERA_CCI
struct i2c_data {
	struct i2c_client *client;
	struct regulator *vana;
	uint8_t power_up;
};
int stmvl53l0_init_i2c(void);
void stmvl53l0_exit_i2c(void *);
int stmvl53l0_power_up_i2c(void *, unsigned int *);
int stmvl53l0_power_down_i2c(void *);

#endif /* NOT CAMERA_CCI */
#endif /* STMVL53L0_I2C_H */
