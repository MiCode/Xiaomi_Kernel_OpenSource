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

#ifndef ANDROID_SENSOR_HAL_COMMON_DATA
#define ANDROID_SENSOR_HAL_COMMON_DATA

#include <hardware/sensors.h>

#include "../configuration.h"

#define ST_HAL_GRAVITY_MAX_ON_EARTH			(10.7f)

/*
 * Android version
 */
#define ST_HAL_KITKAT_VERSION				(0)
#define ST_HAL_LOLLIPOP_VERSION				(1)
#define ST_HAL_MARSHMALLOW_VERSION			(2)
#define ST_HAL_NOUGAT_VERSION				(3)
#define ST_HAL_OREO_VERSION					(4)

#define CONCATENATE_STRING(x, y)			(x y)

#define ST_HAL_DATA_PATH				"/data/STSensorHAL"
#define ST_HAL_PRIVATE_DATA_PATH			"/data/STSensorHAL/private_data.dat"
#define ST_HAL_FACTORY_DATA_PATH			"/data/STSensorHAL/factory_calibration"
#define ST_HAL_SELFTEST_DATA_PATH			"/data/STSensorHAL/selftest"
#define ST_HAL_SELFTEST_CMD_DATA_PATH			"/data/STSensorHAL/selftest/cmd"
#define ST_HAL_SELFTEST_RESULTS_DATA_PATH		"/data/STSensorHAL/selftest/results"
#define ST_HAL_FACTORY_ACCEL_DATA_FILENAME		CONCATENATE_STRING(ST_HAL_FACTORY_DATA_PATH, "/accel.txt")
#define ST_HAL_FACTORY_GYRO_DATA_FILENAME		CONCATENATE_STRING(ST_HAL_FACTORY_DATA_PATH, "/gyro.txt")

#define SENSOR_TYPE_ST_CUSTOM_NO_SENSOR			(SENSOR_TYPE_DEVICE_PRIVATE_BASE + 20)

#define ST_HAL_IIO_MAX_DEVICES				(50)

#define SENSOR_DATA_X(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
							((x1 == 1 ? datax : (x1 == -1 ? -datax : 0)) + \
							(x2 == 1 ? datay : (x2 == -1 ? -datay : 0)) + \
							(x3 == 1 ? dataz : (x3 == -1 ? -dataz : 0)))

#define SENSOR_DATA_Y(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
							((y1 == 1 ? datax : (y1 == -1 ? -datax : 0)) + \
							(y2 == 1 ? datay : (y2 == -1 ? -datay : 0)) + \
							(y3 == 1 ? dataz : (y3 == -1 ? -dataz : 0)))

#define SENSOR_DATA_Z(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
							((z1 == 1 ? datax : (z1 == -1 ? -datax : 0)) + \
							(z2 == 1 ? datay : (z2 == -1 ? -datay : 0)) + \
							(z3 == 1 ? dataz : (z3 == -1 ? -dataz : 0)))

#define SENSOR_X_DATA(...)				SENSOR_DATA_X(__VA_ARGS__)
#define SENSOR_Y_DATA(...)				SENSOR_DATA_Y(__VA_ARGS__)
#define SENSOR_Z_DATA(...)				SENSOR_DATA_Z(__VA_ARGS__)

#define ST_HAL_DEBUG_INFO				(1)
#define ST_HAL_DEBUG_VERBOSE				(2)
#define ST_HAL_DEBUG_EXTRA_VERBOSE			(3)

/* Overrite default log utility */
#ifdef PLTF_LINUX_ENABLED
#define ALOGD(fmt, ...) printf((fmt), ##__VA_ARGS__)
#define ALOGE(fmt, ...) printf((fmt), ##__VA_ARGS__)
#endif

#endif /* ANDROID_SENSOR_HAL_COMMON_DATA */
