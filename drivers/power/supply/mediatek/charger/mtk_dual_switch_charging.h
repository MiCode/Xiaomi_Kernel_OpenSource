/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_DUAL_SWITCH_CHARGER_H
#define _MTK_DUAL_SWITCH_CHARGER_H

struct dual_switch_charging_alg_data {
	int state;
	bool disable_charging;
	struct mutex ichg_aicr_access_mutex;

	unsigned int total_charging_time;
	unsigned int pre_cc_charging_time;
	unsigned int cc_charging_time;
	unsigned int cv_charging_time;
	unsigned int full_charging_time;
};

#endif /* End of _MTK_DUAL_SWITCH_CHARGER_H */
