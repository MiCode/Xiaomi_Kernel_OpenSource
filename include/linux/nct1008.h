/*
 * include/linux/nct1008.h
 *
 * NCT1008, temperature monitoring device from ON Semiconductors
 *
 * Copyright (c) 2010-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef _LINUX_NCT1008_H
#define _LINUX_NCT1008_H

#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/thermal.h>
#include <linux/platform_data/thermal_sensors.h>

struct nct1008_data;

enum nct1008_chip { NCT1008, NCT72 };

enum nct1008_sensors { LOC, EXT, SENSORS_COUNT };

struct nct1008_sensor_platform_data {
	int shutdown_limit;
	long suspend_limit_hi;
	long suspend_limit_lo;
	int passive_delay;
	int polling_delay;

	struct thermal_trip_info trips[THERMAL_MAX_TRIPS];
	struct thermal_zone_params *tzp;
	int num_trips;
};

struct nct1008_platform_data {
	bool supported_hwrev;
	int conv_rate;
	int offset;
	const char *loc_name;
	struct nct1008_sensor_platform_data sensors[SENSORS_COUNT];
	int (*suspend_with_wakeup)(void);
	bool extended_range;
};
#endif /* _LINUX_NCT1008_H */
