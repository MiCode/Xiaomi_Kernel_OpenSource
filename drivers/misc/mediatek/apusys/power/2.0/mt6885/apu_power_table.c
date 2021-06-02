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

#include <linux/module.h>
#include "apu_power_table.h"

// FIXME: update vpu power table in DVT stage
/* opp, mW */
struct apu_opp_info vpu_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 242},
	{APU_OPP_1, 188},
	{APU_OPP_2, 142},
	{APU_OPP_3, 136},
	{APU_OPP_4, 127},
	{APU_OPP_5, 100},
#if !defined(CONFIG_MACH_MT6893)
	{APU_OPP_6, 93},
	{APU_OPP_7, 81},
	{APU_OPP_8, 76},
	{APU_OPP_9, 47},
#endif
};
EXPORT_SYMBOL(vpu_power_table);

// FIXME: update mdla power table in DVT stage
/* opp, mW */
struct apu_opp_info mdla_power_table[APU_OPP_NUM] = {
	{APU_OPP_0, 200},
	{APU_OPP_1, 200},
	{APU_OPP_2, 159},
	{APU_OPP_3, 157},
	{APU_OPP_4, 117},
	{APU_OPP_5, 110},
#if !defined(CONFIG_MACH_MT6893)
	{APU_OPP_6, 84},
	{APU_OPP_7, 74},
	{APU_OPP_8, 46},
	{APU_OPP_9, 44},
#endif
};
EXPORT_SYMBOL(mdla_power_table);
