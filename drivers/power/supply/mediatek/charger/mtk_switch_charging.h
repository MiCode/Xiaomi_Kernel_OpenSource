/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#ifndef _MTK_SWITCH_CHARGER_H
#define _MTK_SWITCH_CHARGER_H

/*****************************************************************************
 *  Switch Charging State
 ****************************************************************************/

struct switch_charging_alg_data {
	int state;
	bool disable_charging;
	struct mutex ichg_aicr_access_mutex;

	unsigned int total_charging_time;
	unsigned int pre_cc_charging_time;
	unsigned int cc_charging_time;
	unsigned int cv_charging_time;
	unsigned int full_charging_time;
	struct timespec charging_begin_time;
};

#endif /* End of _MTK_SWITCH_CHARGER_H */
