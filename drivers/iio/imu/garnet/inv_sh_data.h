/*
* Copyright (C) 2015 InvenSense, Inc.
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
#ifndef _INV_SH_DATA_H
#define _INV_SH_DATA_H

#include <linux/types.h>

struct inv_sh_data_vector {
	int16_t x;
	int16_t y;
	int16_t z;
};

struct inv_sh_data_orientation {
	int16_t azimuth;
	int16_t pitch;
	int16_t roll;
};

struct inv_sh_data_rotation_vector {
	int32_t x;
	int32_t y;
	int32_t z;
	int32_t w;
	uint16_t accuracy;
};

struct inv_sh_data_uncalib {
	struct inv_sh_data_vector uncalib;
	struct inv_sh_data_vector bias;
};

union inv_sh_data_sensor {
	struct inv_sh_data_vector accelerometer;
	struct inv_sh_data_vector magnetic_field;
	struct inv_sh_data_orientation orientation;
	struct inv_sh_data_vector gyroscope;
	uint32_t light;
	uint32_t pressure;
	/* DEPRECATED: temperature */
	uint16_t proximity;
	struct inv_sh_data_vector gravity;
	struct inv_sh_data_vector linear_acceleration;
	struct inv_sh_data_rotation_vector rotation_vector;
	uint16_t relative_humidity;
	int16_t ambient_temperature;
	struct inv_sh_data_uncalib magnetic_field_uncalibrated;
	struct inv_sh_data_rotation_vector game_rotation_vector;
	struct inv_sh_data_uncalib gyroscope_uncalibrated;
	uint32_t step_counter;
	struct inv_sh_data_rotation_vector geomagnetic_rotation_vector;
	uint16_t heart_rate;
	uint8_t activity_recognition;
	struct {
		uint8_t size;
		const void *data;
	} vendor;
};

union inv_sh_data_answer {
	int32_t gain[9];
	int32_t offset[3];
};

#define INV_SH_DATA_MAX_SIZE						64

#define INV_SH_DATA_SENSOR_ID_FLAG_MASK					0x80
#define INV_SH_DATA_SENSOR_ID_FLAG_WAKE_UP				0x80
enum inv_sh_data_sensor_id {
	INV_SH_DATA_SENSOR_ID_METADATA					= 0,
	INV_SH_DATA_SENSOR_ID_ACCELEROMETER				= 1,
	INV_SH_DATA_SENSOR_ID_MAGNETIC_FIELD				= 2,
	INV_SH_DATA_SENSOR_ID_ORIENTATION				= 3,
	INV_SH_DATA_SENSOR_ID_GYROSCOPE					= 4,
	INV_SH_DATA_SENSOR_ID_LIGHT					= 5,
	INV_SH_DATA_SENSOR_ID_PRESSURE					= 6,
	INV_SH_DATA_SENSOR_ID_TEMPERATURE				= 7,
	INV_SH_DATA_SENSOR_ID_PROXIMITY					= 8,
	INV_SH_DATA_SENSOR_ID_GRAVITY					= 9,
	INV_SH_DATA_SENSOR_ID_LINEAR_ACCELERATION			= 10,
	INV_SH_DATA_SENSOR_ID_ROTATION_VECTOR				= 11,
	INV_SH_DATA_SENSOR_ID_RELATIVE_HUMIDITY				= 12,
	INV_SH_DATA_SENSOR_ID_AMBIENT_TEMPERATURE			= 13,
	INV_SH_DATA_SENSOR_ID_MAGNETIC_FIELD_UNCALIBRATED		= 14,
	INV_SH_DATA_SENSOR_ID_GAME_ROTATION_VECTOR			= 15,
	INV_SH_DATA_SENSOR_ID_GYROSCOPE_UNCALIBRATED			= 16,
	INV_SH_DATA_SENSOR_ID_SIGNIFICANT_MOTION			= 17,
	INV_SH_DATA_SENSOR_ID_STEP_DETECTOR			        = 18,
	INV_SH_DATA_SENSOR_ID_STEP_COUNTER			        = 19,
	INV_SH_DATA_SENSOR_ID_GEOMAGNETIC_ROTATION_VECTOR		= 20,
	INV_SH_DATA_SENSOR_ID_HEART_RATE			        = 21,
	INV_SH_DATA_SENSOR_ID_TILT_DETECTOR				= 22,
	INV_SH_DATA_SENSOR_ID_WAKE_GESTURE				= 23,
	INV_SH_DATA_SENSOR_ID_GLANCE_GESTURE				= 24,
	INV_SH_DATA_SENSOR_ID_PICKUP_GESTURE				= 25,

	INV_SH_DATA_SENSOR_ID_ACTIVITY_RECOGNITION			= 31,
	INV_SH_DATA_SENSOR_ID_VENDOR_BASE				= 32,

	INV_SH_DATA_SENSOR_ID_SELF_TEST					= 254,
	INV_SH_DATA_SENSOR_ID_PLATFORM_SETUP				= 255,
};

enum inv_sh_data_status {
	INV_SH_DATA_STATUS_DATA_UPDATED			= 0,
	INV_SH_DATA_STATUS_STATE_CHANGED		= 1,
	INV_SH_DATA_STATUS_FLUSH			= 2,
	INV_SH_DATA_STATUS_POLL				= 3,
};

struct inv_sh_data {
	const uint8_t *raw;
	uint8_t size;
	uint8_t id;
	uint8_t accuracy;
	uint8_t status;
	uint8_t data_size;
	uint8_t is_answer;
	uint8_t payload;
	uint8_t extended;
	union {
		struct {
			uint32_t timestamp;
			union inv_sh_data_sensor data;
		};
		struct {
			uint8_t command;
			union inv_sh_data_answer answer;
		};
	};
};

int inv_sh_data_parse(const void *frame, size_t size, struct inv_sh_data *data);

#endif
