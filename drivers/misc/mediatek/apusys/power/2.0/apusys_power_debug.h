/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
