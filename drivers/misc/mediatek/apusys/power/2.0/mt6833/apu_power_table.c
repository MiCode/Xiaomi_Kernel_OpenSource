// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include "apu_power_table.h"

// FIXME: update vpu power table in DVT stage
/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 212},
	{APU_OPP_1, 176},
	{APU_OPP_2, 133},
	{APU_OPP_3, 98},
	{APU_OPP_4, 44},
};
EXPORT_SYMBOL(vpu_power_table);

// FIXME: update mdla power table in DVT stage
/*  opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 0},
	{APU_OPP_1, 0},
	{APU_OPP_2, 0},
	{APU_OPP_3, 0},
	{APU_OPP_4, 0},
};
EXPORT_SYMBOL(mdla_power_table);
