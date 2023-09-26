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

#ifndef _MTK_SWITCH_CHARGER_H
#define _MTK_SWITCH_CHARGER_H

/*****************************************************************************
 *  Switch Charging State
 ****************************************************************************/

#define USBIN_SWCHG_SELECT_CHGCURR_VOTER	"USBIN_SWCHG_SELECT_CHGCURR_VOTER"
#define USBIN_SWCHG_CHR_CC_VOTER		"USBIN_SWCHG_CHR_CC_VOTER"
#define USBIN_SWCHG_TURNON_CHG_VOTER		"USBIN_SWCHG_TURNON_CHG_VOTER"
#define FV_SWCHG_SELECT_CV_VOTER		"FV_SWCHG_SELECT_CV_VOTER"
#define FCCMAIN_SWCHG_SELECT_CHGCURR_VOTER	"FCCMAIN_SWCHG_SELECT_CHGCURR_VOTER"
#define FCCMAIN_SWCHG_CHR_CC_VOTER		"FCCMAIN_SWCHG_CHR_CC_VOTER"
#define USBIN_SWCHG_ADAPTER_VOTER			"USBIN_SWCHG_ADAPTER_VOTER"

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

	int vbus_mv;
};

#endif /* End of _MTK_SWITCH_CHARGER_H */
