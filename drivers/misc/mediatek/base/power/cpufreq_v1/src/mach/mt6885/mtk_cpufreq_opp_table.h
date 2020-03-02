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

#include "mtk_cpufreq_struct.h"
#include "mtk_cpufreq_config.h"

/* 6785 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_6885          2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_6885          1895000         /* KHz */
#define CPU_DVFS_FREQ2_LL_6885		1791000		/* KHz */
#define CPU_DVFS_FREQ3_LL_6885		1708000		/* KHz */
#define CPU_DVFS_FREQ4_LL_6885		1625000		/* KHz */
#define CPU_DVFS_FREQ5_LL_6885		1500000		/* KHz */
#define CPU_DVFS_FREQ6_LL_6885		1393000		/* KHz */
#define CPU_DVFS_FREQ7_LL_6885		1287000		/* KHz */
#define CPU_DVFS_FREQ8_LL_6885		1181000		/* KHz */
#define CPU_DVFS_FREQ9_LL_6885		1048000		/* KHz */
#define CPU_DVFS_FREQ10_LL_6885		968000		/* KHz */
#define CPU_DVFS_FREQ11_LL_6885		862000		/* KHz */
#define CPU_DVFS_FREQ12_LL_6885		756000		/* KHz */
#define CPU_DVFS_FREQ13_LL_6885		703000		/* KHz */
#define CPU_DVFS_FREQ14_LL_6885		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_6885		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_6885		2202000		/* KHz */
#define CPU_DVFS_FREQ1_L_6885		2106000		/* KHz */
#define CPU_DVFS_FREQ2_L_6885		2050000		/* KHz */
#define CPU_DVFS_FREQ3_L_6885		1975000		/* KHz */
#define CPU_DVFS_FREQ4_L_6885		1900000		/* KHz */
#define CPU_DVFS_FREQ5_L_6885		1803000		/* KHz */
#define CPU_DVFS_FREQ6_L_6885		1750000		/* KHz */
#define CPU_DVFS_FREQ7_L_6885		1622000		/* KHz */
#define CPU_DVFS_FREQ8_L_6885		1526000		/* KHz */
#define CPU_DVFS_FREQ9_L_6885		1367000		/* KHz */
#define CPU_DVFS_FREQ10_L_6885		1271000		/* KHz */
#define CPU_DVFS_FREQ11_L_6885		1176000		/* KHz */
#define CPU_DVFS_FREQ12_L_6885		1048000		/* KHz */
#define CPU_DVFS_FREQ13_L_6885		921000		/* KHz */
#define CPU_DVFS_FREQ14_L_6885		825000		/* KHz */
#define CPU_DVFS_FREQ15_L_6885		730000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_6885		1540000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_6885		1469000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_6885		1426000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_6885		1370000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_6885		1313000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_6885		1256000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_6885		1195000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_6885		1115000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_6885		1030000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_6885		945000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_6885	881000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_6885	817000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_6885	711000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_6885	668000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_6885	583000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_6885	520000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_6885	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_6885	 96875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_6885	 93125		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_6885	 90625		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_6885	 88125		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_6885	 83750		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_6885	 80625		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_6885	 77500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_6885	 75000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_6885	 71250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_6885	 68750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_6885	 65625		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_6885	 63125		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_6885	 61250		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_6885	 60000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_6885	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_6885	100000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_6885	96250		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_6885	94375		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_6885	91875		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_6885	89375		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_6885	85625		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_6885	83750		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_6885	80625		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_6885	78750		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_6885	75000		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_6885	72500		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_6885	70625		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_6885	67500		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_6885	64375		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_6885	62500		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_6885	60000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_6885	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_6885	 96875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_6885	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_6885	 91875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_6885	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_6885	 86875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_6885	 83750		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_6885	 80625		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_6885	 78125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_6885	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_6885	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_6885	 70625		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_6885	 66875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_6885	 65000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_6885	 62500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_6885	 60000		/* 10uV */


/* DVFS OPP table */
#define OPP_TBL(cluster, seg, lv, vol)	\
static struct mt_cpu_freq_info opp_tbl_##cluster##_e##lv##_0[] = {        \
	OP                                                                \
(CPU_DVFS_FREQ0_##cluster##_##seg, CPU_DVFS_VOLT0_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ1_##cluster##_##seg, CPU_DVFS_VOLT1_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ2_##cluster##_##seg, CPU_DVFS_VOLT2_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ3_##cluster##_##seg, CPU_DVFS_VOLT3_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ4_##cluster##_##seg, CPU_DVFS_VOLT4_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ5_##cluster##_##seg, CPU_DVFS_VOLT5_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ6_##cluster##_##seg, CPU_DVFS_VOLT6_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ7_##cluster##_##seg, CPU_DVFS_VOLT7_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ8_##cluster##_##seg, CPU_DVFS_VOLT8_VPROC##vol##_##seg),   \
	OP                                                               \
(CPU_DVFS_FREQ9_##cluster##_##seg, CPU_DVFS_VOLT9_VPROC##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ10_##cluster##_##seg, CPU_DVFS_VOLT10_VPROC##vol##_##seg), \
	OP                                                               \
(CPU_DVFS_FREQ11_##cluster##_##seg, CPU_DVFS_VOLT11_VPROC##vol##_##seg), \
	OP                                                               \
(CPU_DVFS_FREQ12_##cluster##_##seg, CPU_DVFS_VOLT12_VPROC##vol##_##seg), \
	OP                                                               \
(CPU_DVFS_FREQ13_##cluster##_##seg, CPU_DVFS_VOLT13_VPROC##vol##_##seg), \
	OP                                                               \
(CPU_DVFS_FREQ14_##cluster##_##seg, CPU_DVFS_VOLT14_VPROC##vol##_##seg), \
	OP                                                               \
(CPU_DVFS_FREQ15_##cluster##_##seg, CPU_DVFS_VOLT15_VPROC##vol##_##seg), \
}

OPP_TBL(LL,   6885, 0, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  6885, 0, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, 6885, 0, 3); /* opp_tbl_CCI_e0_0 */

struct opp_tbl_info opp_tbls[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_LL_e0_0,
				ARRAY_SIZE(opp_tbl_LL_e0_0) },

	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_L_e0_0,
				ARRAY_SIZE(opp_tbl_L_e0_0) },

	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_CCI_e0_0,
				ARRAY_SIZE(opp_tbl_CCI_e0_0) },

	},
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_6885[] = {	/* 6785 */
	/* POS,	CLK */
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_6885[] = {	/* 6785 */
	/* POS,	CLK */
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(1,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_6885[] = {	/* 6785 */
	/* POS,	CLK */
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_6885 },
	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_6885 },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_6885 },
	},
};
