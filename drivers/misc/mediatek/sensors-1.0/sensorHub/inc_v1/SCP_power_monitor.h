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

#ifndef _SCP_POWER_MONITOR_H_
#define _SCP_POWER_MONITOR_H_
#include <linux/major.h>
#include <linux/types.h>

enum SCP_SENSOR_POWER {
	SENSOR_POWER_UP = 0,
	SENSOR_POWER_DOWN,
};
struct scp_power_monitor  {
	const char *name;
	struct list_head list;
	int (*notifier_call)(uint8_t action, void *data);
};
int scp_power_monitor_register(struct scp_power_monitor *monitor);
int scp_power_monitor_deregister(struct scp_power_monitor *monitor);
void scp_power_monitor_notify(uint8_t action, void *data);
#endif
