/*
 * include/linux/platform_data/thermal_sensors.h
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _THERMAL_SENSORS_H
#define _THERMAL_SENSORS_H

struct thermal_trip_info {
	long trip_temp;
	enum thermal_trip_type trip_type;
	unsigned long upper;
	unsigned long lower;
	long hysteresis;
	bool tripped;
	bool bound;
	unsigned int level;
	char *cdev_type;
};

#endif /* _THERMAL_SENSORS_H */
