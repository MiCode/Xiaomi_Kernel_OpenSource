/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef __MTK_BATTERY_PERCENTAGE_THROTTLING_H__
#define __MTK_BATTERY_PERCENTAGE_THROTTLING_H__

#define BATTERY_PERCENT_LEVEL enum BATTERY_PERCENT_LEVEL_TAG
#define BATTERY_PERCENT_PRIO enum BATTERY_PERCENT_PRIO_TAG

enum BATTERY_PERCENT_LEVEL_TAG {
	BATTERY_PERCENT_LEVEL_0 = 0,
	BATTERY_PERCENT_LEVEL_1 = 1
};

enum BATTERY_PERCENT_PRIO_TAG {
	BATTERY_PERCENT_PRIO_CPU_B = 0,
	BATTERY_PERCENT_PRIO_CPU_L = 1,
	BATTERY_PERCENT_PRIO_GPU = 2,
	BATTERY_PERCENT_PRIO_MD = 3,
	BATTERY_PERCENT_PRIO_MD5 = 4,
	BATTERY_PERCENT_PRIO_FLASHLIGHT = 5,
	BATTERY_PERCENT_PRIO_VIDEO = 6,
	BATTERY_PERCENT_PRIO_WIFI = 7,
	BATTERY_PERCENT_PRIO_BACKLIGHT = 8
};

typedef void (*battery_percent_callback)(BATTERY_PERCENT_LEVEL tag);

#if IS_ENABLED(CONFIG_MTK_BATTERY_PERCENTAGE_POWER_THROTTLING)
void register_battery_percent_notify(
		battery_percent_callback bp_cb,
		BATTERY_PERCENT_PRIO prio_val);
#else
static void register_battery_percent_notify(
		battery_percent_callback bp_cb,
		BATTERY_PERCENT_PRIO prio_val)
{};
#endif

#endif /* __MTK_BATTERY_PERCENTAGE_THROTTLING_H__ */
