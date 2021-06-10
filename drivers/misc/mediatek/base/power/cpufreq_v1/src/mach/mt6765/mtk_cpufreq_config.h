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

#ifndef __MTK_CPUFREQ_CONFIG_H__
#define __MTK_CPUFREQ_CONFIG_H__

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_L,
	MT_CPU_DVFS_LL,
	MT_CPU_DVFS_CCI,

	NR_MT_CPU_DVFS,
};

enum cpu_level {
	CPU_LEVEL_0, /* FY */
	CPU_LEVEL_1, /* SB */
	CPU_LEVEL_2, /* 65T */
	CPU_LEVEL_3, /* 65 */
	CPU_LEVEL_4, /* 62 */
	CPU_LEVEL_5, /* 62LY */

	NUM_CPU_LEVEL,
};

/* PMIC Config */
enum mt_cpu_dvfs_buck_id {
	CPU_DVFS_VPROC1,
	CPU_DVFS_VSRAM1,

	NR_MT_BUCK,
};

enum mt_cpu_dvfs_pmic_type {
	BUCK_MT6357_VPROC,
	BUCK_MT6357_VSRAM,

	NR_MT_PMIC,
};

/* PLL Config */
enum mt_cpu_dvfs_pll_id {
	PLL_L_CLUSTER,
	PLL_LL_CLUSTER,
	PLL_CCI_CLUSTER,

	NR_MT_PLL,
};

enum top_ckmuxsel {
	TOP_CKMUXSEL_CLKSQ = 0,
	TOP_CKMUXSEL_ARMPLL = 1,
	TOP_CKMUXSEL_MAINPLL = 2,
	TOP_CKMUXSEL_UNIVPLL = 3,

	NR_TOP_CKMUXSEL,
};

#endif	/* __MTK_CPUFREQ_CONFIG_H__ */
