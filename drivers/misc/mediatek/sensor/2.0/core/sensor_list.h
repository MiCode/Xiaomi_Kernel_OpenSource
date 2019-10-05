/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _SENSOR_LIST_H_
#define _SENSOR_LIST_H_

#include <linux/ioctl.h>

struct sensorlist_info_t {
	char name[16];
};

struct mag_libinfo_t {
	char libname[64];
	int32_t layout;
	int32_t deviceid;
};

int sensorlist_register_maginfo(struct mag_libinfo_t *mag_info);
int sensorlist_register_devinfo(int sensor,
		struct sensorlist_info_t *devinfo);
int sensorlist_find_sensor(int sensor);

#define SENSOR_LIST_GET_MAG_LIB_INFO _IOWR('a', 1, struct mag_libinfo_t)

#endif
