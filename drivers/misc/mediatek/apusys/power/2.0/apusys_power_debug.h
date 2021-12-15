// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APUSYS_POWER_DEBUG_H_
#define _APUSYS_POWER_DEBUG_H_

#ifdef BUILD_POLICY_TEST
#include <stdbool.h>
#endif

extern bool is_power_debug_lock;
extern int fixed_opp;
extern int power_on_off_stress;

enum APUSYS_POWER_PARAM {
	POWER_PARAM_FIX_OPP,
	POWER_PARAM_DVFS_DEBUG,
	POWER_HAL_CTL,
	POWER_PARAM_SET_USER_OPP,
	POWER_PARAM_SET_THERMAL_OPP,
	POWER_PARAM_SET_POWER_HAL_OPP,
	POWER_PARAM_GET_POWER_REG,
	POWER_PARAM_POWER_STRESS,
	POWER_PARAM_OPP_TABLE,
	POWER_PARAM_CURR_STATUS,
	POWER_PARAM_LOG_LEVEL,
};


void apusys_power_debugfs_init(void);
void apusys_power_debugfs_exit(void);
int apusys_power_create_procfs(void);
void fix_dvfs_debug(void);

#endif
