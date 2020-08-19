/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_LOW_BATTERY_THROTTLING_H__
#define __MTK_LOW_BATTERY_THROTTLING_H__

enum LOW_BATTERY_LEVEL_TAG {
	LOW_BATTERY_LEVEL_0 = 0,
	LOW_BATTERY_LEVEL_1 = 1,
	LOW_BATTERY_LEVEL_2 = 2
};

enum LOW_BATTERY_PRIO_TAG {
	LOW_BATTERY_PRIO_CPU_B = 0,
	LOW_BATTERY_PRIO_CPU_L = 1,
	LOW_BATTERY_PRIO_GPU = 2,
	LOW_BATTERY_PRIO_MD = 3,
	LOW_BATTERY_PRIO_MD5 = 4,
	LOW_BATTERY_PRIO_FLASHLIGHT = 5,
	LOW_BATTERY_PRIO_VIDEO = 6,
	LOW_BATTERY_PRIO_WIFI = 7,
	LOW_BATTERY_PRIO_BACKLIGHT = 8,
	LOW_BATTERY_PRIO_DLPT = 9
};

typedef void (*low_battery_callback)(enum LOW_BATTERY_LEVEL_TAG tag);

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
int register_low_battery_notify(low_battery_callback lb_cb,
				enum LOW_BATTERY_PRIO_TAG prio_val);
#else
static int register_low_battery_notify(low_battery_callback lb_cb,
				       enum LOW_BATTERY_PRIO_TAG prio_val)
{ return 0; }
#endif

#endif /* __MTK_LOW_BATTERY_THROTTLING_H__ */
