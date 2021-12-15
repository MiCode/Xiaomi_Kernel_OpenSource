/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef _MTK_LINEAR_CHARGER_H
#define _MTK_LINEAR_CHARGER_H

/*****************************************************************************
 *  Linear Charging State
 ****************************************************************************/
#define MAX_TOPOFF_CHARGING_TIME (3 * 60 * 60) /* 3 hours */

#define RECHARGE_OFFSET 150000 /* uV */
#define TOPOFF_VOLTAGE 4200000 /* uV */
#define CHG_FULL_CURRENT 150000 /* uA */

struct linear_charging_alg_data {
	int state;
	bool disable_charging;
	struct mutex ichg_access_mutex;

	unsigned int total_charging_time;
	unsigned int cc_charging_time;
	unsigned int topoff_charging_time;
	unsigned int full_charging_time;
	struct timespec topoff_begin_time;
	struct timespec charging_begin_time;

	int recharge_offset; /* uv */
	int topoff_voltage; /* uv */
	int chg_full_current; /* uA */
};

#endif /* End of _MTK_LINEAR_CHARGER_H */
