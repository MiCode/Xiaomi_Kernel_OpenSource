/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SENSOR_LIST_H
#define _SENSOR_LIST_H

#ifndef CONFIG_CUSTOM_KERNEL_SENSORHUB
struct sensorInfo_NonHub_t {
	char name[16];
};
int sensorlist_register_deviceinfo(int sensor,
		struct sensorInfo_NonHub_t *devinfo);
#endif
#endif
