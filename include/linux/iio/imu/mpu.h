/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#ifndef _MPU_H_
#define _MPU_H_

#include <linux/device.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>

#define INV_PROD_KEY(ver, rev) (ver * 100 + rev)
/**
 * struct mpu_platform_data - Platform data for the mpu driver
 * @accel_orient:	Orientation matrix of the accelerometer
 * @magn_orient:	Orientation matrix of the magnetometer
 * @gyro_orient:	Orientation matrix of the gyroscope
 * @wake_gpio:		GPIO for waking up the SensorHub
 * @wake_delay_min:	Minimum waking delay of the SensorHub
 * @wake_delay_max:	Maximum waking delay of the SensorHub
 *
 * Contains platform specific information on how to configure the MPU to work
 * on this platform. The orientation matricies are 3x3 rotation matricies that
 * are applied to the data to rotate from the mounting orientation to the
 * platform orientation.  The values must be one of 0, 1, or -1 and each row and
 * column should have exactly 1 non-zero value.
 */
struct mpu_platform_data {
	s8 accel_orient[9];
	s8 magn_orient[9];
	s8 gyro_orient[9];
	int wake_gpio;
	unsigned long wake_delay_min;
	unsigned long wake_delay_max;
	struct regulator *vdd_ana;
	struct regulator *vdd_i2c;
};

#endif	/* _MPU_H_ */
