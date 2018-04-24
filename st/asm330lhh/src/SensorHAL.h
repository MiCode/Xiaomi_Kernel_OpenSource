/*
 * Copyright (C) 2015-2016 STMicroelectronics
 * Author: Denis Ciocca - <denis.ciocca@st.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ST_SENSOR_HAL_H
#define ST_SENSOR_HAL_H

#include <hardware/hardware.h>
#include <hardware/sensors.h>
#include <poll.h>

#include "SensorBase.h"
#include "SelfTest.h"
#include "common_data.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a)		(int)((sizeof(a) / sizeof(*(a))) / \
					static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))
#endif

/*
 * IIO driver sensors names
 */
#define ST_SENSORS_LIST_32				"asm330lhh"

/*
 * IIO driver sensors suffix for sensors
 */
#define ACCEL_NAME_SUFFIX_IIO				"_accel"
#define GYRO_NAME_SUFFIX_IIO				"_gyro"

#define ST_HAL_WAKEUP_SUFFIX_IIO			"_wk"

#define ST_HAL_NEW_SENSOR_SUPPORTED(DRIVER_NAME, ANDROID_SENSOR_TYPE, IIO_SENSOR_TYPE, ANDROID_NAME, POWER_CONSUMPTION) \
	{ \
	.driver_name = DRIVER_NAME, \
	.android_name = ANDROID_NAME, \
	.android_sensor_type = ANDROID_SENSOR_TYPE,\
	.iio_sensor_type = IIO_SENSOR_TYPE, \
	.power_consumption = POWER_CONSUMPTION,\
	},

#if (CONFIG_ST_HAL_ANDROID_VERSION >= ST_HAL_MARSHMALLOW_VERSION)
#define ST_HAL_IIO_DEVICE_API_VERSION			SENSORS_DEVICE_API_VERSION_1_4
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

#if (CONFIG_ST_HAL_ANDROID_VERSION == ST_HAL_LOLLIPOP_VERSION)
#define ST_HAL_IIO_DEVICE_API_VERSION			SENSORS_DEVICE_API_VERSION_1_3
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

#if (CONFIG_ST_HAL_ANDROID_VERSION == ST_HAL_KITKAT_VERSION)
#define ST_HAL_IIO_DEVICE_API_VERSION			SENSORS_DEVICE_API_VERSION_1_1
#endif /* CONFIG_ST_HAL_ANDROID_VERSION */

struct STSensorHAL_data {
	sensors_poll_device_1 poll_device;

	pthread_t *data_threads;
	pthread_t *events_threads;
	SensorBase *sensor_classes[ST_HAL_IIO_MAX_DEVICES];

	int last_handle;

	unsigned int sensor_available;
	struct sensor_t *sensor_t_list;

#ifdef CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS
	SelfTest *self_test;
#endif /* CONFIG_ST_HAL_HAS_SELFTEST_FUNCTIONS */

	struct pollfd android_pollfd[ST_HAL_IIO_MAX_DEVICES];
} typedef STSensorHAL_data;

#endif /* ST_SENSOR_HAL_H */
