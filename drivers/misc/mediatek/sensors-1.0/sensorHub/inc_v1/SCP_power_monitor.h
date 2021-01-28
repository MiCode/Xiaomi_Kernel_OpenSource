/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
