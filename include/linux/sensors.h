/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_SENSORS_H_INCLUDED
#define __LINUX_SENSORS_H_INCLUDED

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rwsem.h>

#define SENSORS_ACCELERATION_HANDLE		0
#define SENSORS_MAGNETIC_FIELD_HANDLE		1
#define SENSORS_ORIENTATION_HANDLE		2
#define SENSORS_LIGHT_HANDLE			3
#define SENSORS_PROXIMITY_HANDLE		4
#define SENSORS_GYROSCOPE_HANDLE		5
#define SENSORS_PRESSURE_HANDLE			6

#define SENSOR_TYPE_ACCELEROMETER		1
#define SENSOR_TYPE_GEOMAGNETIC_FIELD		2
#define SENSOR_TYPE_MAGNETIC_FIELD  SENSOR_TYPE_GEOMAGNETIC_FIELD
#define SENSOR_TYPE_ORIENTATION			3
#define SENSOR_TYPE_GYROSCOPE			4
#define SENSOR_TYPE_LIGHT			5
#define SENSOR_TYPE_PRESSURE			6
#define SENSOR_TYPE_TEMPERATURE			7
#define SENSOR_TYPE_PROXIMITY			8
#define SENSOR_TYPE_GRAVITY			9
#define SENSOR_TYPE_LINEAR_ACCELERATION		10
#define SENSOR_TYPE_ROTATION_VECTOR		11
#define SENSOR_TYPE_RELATIVE_HUMIDITY		12
#define SENSOR_TYPE_AMBIENT_TEMPERATURE		13
#define SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED	14
#define SENSOR_TYPE_GAME_ROTATION_VECTOR	15
#define SENSOR_TYPE_GYROSCOPE_UNCALIBRATED	16
#define SENSOR_TYPE_SIGNIFICANT_MOTION		17
#define SENSOR_TYPE_STEP_DETECTOR		18
#define SENSOR_TYPE_STEP_COUNTER		19
#define SENSOR_TYPE_GEOMAGNETIC_ROTATION_VECTOR	20

struct sensors_classdev {
	struct device		*dev;
	struct list_head	 node;
	const char		*name;
	const char		*vendor;
	int			version;
	int			handle;
	int			type;
	const char		*max_range;
	const char		*resolution;
	const char		*sensor_power;
	int			min_delay;
	int			fifo_reserved_event_count;
	int			fifo_max_event_count;
};

extern int sensors_classdev_register(struct device *parent,
				 struct sensors_classdev *sensors_cdev);
extern void sensors_classdev_unregister(struct sensors_classdev *sensors_cdev);

#endif		/* __LINUX_SENSORS_H_INCLUDED */
