// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#define pr_fmt(fmt) "<sensorlist> " fmt

#include <linux/module.h>

#include "sensor_list.h"
#include "hf_sensor_type.h"

enum sensorlist {
	accel,
	gyro,
	mag,
	als,
	ps,
	baro,
	sar,
	ois,
	maxhandle,
};

int sensorlist_sensor_to_handle(int sensor)
{
	int handle = -1;

	switch (sensor) {
	case SENSOR_TYPE_ACCELEROMETER:
		handle = accel;
		break;
	case SENSOR_TYPE_GYROSCOPE:
		handle = gyro;
		break;
	case SENSOR_TYPE_MAGNETIC_FIELD:
		handle = mag;
		break;
	case SENSOR_TYPE_LIGHT:
		handle = als;
		break;
	case SENSOR_TYPE_PROXIMITY:
		handle = ps;
		break;
	case SENSOR_TYPE_PRESSURE:
		handle = baro;
		break;
	case SENSOR_TYPE_SAR:
		handle = sar;
		break;
	case SENSOR_TYPE_OIS:
		handle = ois;
		break;
	}
	return handle;
}

int sensorlist_handle_to_sensor(int handle)
{
	int type = -1;

	switch (handle) {
	case accel:
		type = SENSOR_TYPE_ACCELEROMETER;
		break;
	case gyro:
		type = SENSOR_TYPE_GYROSCOPE;
		break;
	case mag:
		type = SENSOR_TYPE_MAGNETIC_FIELD;
		break;
	case als:
		type = SENSOR_TYPE_LIGHT;
		break;
	case ps:
		type = SENSOR_TYPE_PROXIMITY;
		break;
	case baro:
		type = SENSOR_TYPE_PRESSURE;
		break;
	case sar:
		type = SENSOR_TYPE_SAR;
		break;
	case ois:
		type = SENSOR_TYPE_OIS;
		break;
	}
	return type;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("dynamic sensorlist driver");
MODULE_AUTHOR("hongxu.zhao@mediatek.com");
