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




enum APUSYS_POWER_PARAM {
	POWER_PARAM_FIX_OPP,
	POWER_PARAM_DVFS_DEBUG,
	POWER_PARAM_JTAG,
	POWER_PARAM_LOCK,
	POWER_PARAM_VOLT_STEP,
	POWER_HAL_CTL,
	POWER_EARA_CTL,
	POWER_PARAM_SET_USER_OPP,
	POWER_PARAM_SET_THERMAL_OPP,
	POWER_PARAM_SET_POWER_HAL_OPP,
	POWER_PARAM_SET_POWER_OFF,
};


void apusys_power_debugfs_init(void);
void apusys_power_debugfs_exit(void);

#endif
