/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
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
	CPU_LEVEL_6, /* 65R */
	CPU_LEVEL_7, /* 62D */
	CPU_LEVEL_8, /* 62DLY */
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
