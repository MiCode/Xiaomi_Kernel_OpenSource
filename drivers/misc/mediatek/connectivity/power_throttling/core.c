// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/printk.h>
#include "conn_power_throttling.h"
int conn_pwr_core_init(void)
{
	return 0;
}

int conn_pwr_get_drv_level(enum conn_pwr_drv_type type, enum conn_pwr_low_battery_level *level)
{
	return 0;
}
EXPORT_SYMBOL(conn_pwr_get_drv_level);

int conn_pwr_arbitrate(struct conn_pwr_update_info *info)
{
	return 0;
}

int conn_pwr_report_level_required(enum conn_pwr_drv_type type,
					enum conn_pwr_low_battery_level level)
{
	return 0;
}
EXPORT_SYMBOL(conn_pwr_report_level_required);
