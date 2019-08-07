/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_MONITOR_TZ_H__
#define __MTK_MONITOR_TZ_H__

#define MON_TZ_DEFAULT_TEMP (-127000)
#define MON_TZ_DEFAULT_POLLING_DELAY_MS (1000)

enum monitor_thermal_zone {
	TZ_AP_NTC,
	TZ_MDPA_NTC,
	TZ_BATTERY,
	MAX_MON_TZ_NUM
};

extern int get_monitor_thermal_zone_temp(enum monitor_thermal_zone tz_id);

#define MONITOR_TZ_LOG_TAG           "[Thermal/MONITOR_TZ]"
#define monitor_tz_dprintk(fmt, args...)	\
	do {					\
		if (monitor_tz_debug_log == 1) {	\
			pr_notice(MONITOR_TZ_LOG_TAG fmt, ##args);	\
		}	\
	} while (0)

#define monitor_tz_printk(fmt, args...)	\
	pr_notice(MONITOR_TZ_LOG_TAG fmt, ##args)
#endif	/* __MTK_MONITOR_TZ_H__ */
