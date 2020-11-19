/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_POWER_DEBUG_H_
#define _APU_POWER_DEBUG_H_

struct device;
struct regulator;

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
	POWER_INFO,
};

extern struct delayed_work pw_info_work;

int apu_dbg_register_regulator(const char *name, struct regulator *reg);
void apu_dbg_unregister_regulator(void);
int apu_dbg_register_clk(const char *name, struct clk *clk);
void apu_dbg_unregister_clk(void);

#endif
