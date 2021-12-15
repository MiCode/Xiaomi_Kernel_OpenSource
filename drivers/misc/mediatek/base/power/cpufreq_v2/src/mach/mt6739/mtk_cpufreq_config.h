/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_CPUFREQ_CONFIG_H__
#define __MTK_CPUFREQ_CONFIG_H__

enum mt_cpu_dvfs_id {
	MT_CPU_DVFS_LL,

	NR_MT_CPU_DVFS,
};

enum cpu_level {
	CPU_LEVEL_0,
	CPU_LEVEL_1,
	CPU_LEVEL_2,

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
	PLL_LL_CLUSTER,

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
