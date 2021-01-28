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

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_FY		1274000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY		1235000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1196000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1170000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1105000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1053000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1001000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		962000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		910000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		845000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		 702000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		 624000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		 546000	/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		 416000	/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		 338000	/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		 299000	/* KHz */

#define CPU_DVFS_FREQ0_LL_SB		1495000		/* KHz */
#define CPU_DVFS_FREQ1_LL_SB		1378000		/* KHz */
#define CPU_DVFS_FREQ2_LL_SB		1274000		/* KHz */
#define CPU_DVFS_FREQ3_LL_SB		1170000		/* KHz */
#define CPU_DVFS_FREQ4_LL_SB		1105000		/* KHz */
#define CPU_DVFS_FREQ5_LL_SB		1053000		/* KHz */
#define CPU_DVFS_FREQ6_LL_SB		1001000		/* KHz */
#define CPU_DVFS_FREQ7_LL_SB		962000		/* KHz */
#define CPU_DVFS_FREQ8_LL_SB		910000		/* KHz */
#define CPU_DVFS_FREQ9_LL_SB		845000		/* KHz */
#define CPU_DVFS_FREQ10_LL_SB		 702000		/* KHz */
#define CPU_DVFS_FREQ11_LL_SB		 624000		/* KHz */
#define CPU_DVFS_FREQ12_LL_SB		 546000	/* KHz */
#define CPU_DVFS_FREQ13_LL_SB		 416000	/* KHz */
#define CPU_DVFS_FREQ14_LL_SB		 338000	/* KHz */
#define CPU_DVFS_FREQ15_LL_SB		 299000	/* KHz */

#define CPU_DVFS_FREQ0_LL_FY2		1105000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY2		1105000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY2		1105000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY2		1105000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY2		1105000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY2		1053000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY2		1001000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY2		962000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY2		910000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY2		845000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY2		 702000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY2		 624000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY2		 546000	/* KHz */
#define CPU_DVFS_FREQ13_LL_FY2		 416000	/* KHz */
#define CPU_DVFS_FREQ14_LL_FY2		 338000	/* KHz */
#define CPU_DVFS_FREQ15_LL_FY2		 299000	/* KHz */

#define CPUFREQ_BOUNDARY_FOR_FHCTL (CPU_DVFS_FREQ6_LL_FY)	/* if cross 1001MHz when DFS, don't used FHCTL */

/* for DVFS OPP table LL */
#define CPU_DVFS_VOLT0_VPROC1_FY	 130625		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY	 128750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY	 126875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY	 125000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY	 122500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY	 120000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY	 117500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY	 115625		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY	 113750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY	 111875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY	 107500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY	 105000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY	 102500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY	 100000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 97500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 95000		/* 10uV */

#define CPU_DVFS_VOLT0_VPROC1_SB	 130625		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_SB	 128750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_SB	 126875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_SB	 125000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_SB	 122500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_SB	 120000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_SB	 117500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_SB	 115625		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_SB	 113750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_SB	 111875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_SB	 107500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_SB	 105000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_SB	 102500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_SB	 100000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_SB	 97500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_SB	 95000		/* 10uV */

/* for DVFS OPP table LL */
#define CPU_DVFS_VOLT0_VPROC1_FY2	 122500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY2	 122500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY2	 122500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY2	 122500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY2	 122500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY2	 120000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY2	 117500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY2	 115625		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY2	 113750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY2	 111875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY2	 107500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY2	 105000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY2	 102500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY2	 100000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY2	 97500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY2	 95000		/* 10uV */

/* DVFS OPP table */
#define OPP_TBL(cluster, seg, lv, vol)	\
static struct mt_cpu_freq_info opp_tbl_##cluster##_e##lv##_0[] = {	\
	OP(CPU_DVFS_FREQ0_##cluster##_##seg, CPU_DVFS_VOLT0_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ1_##cluster##_##seg, CPU_DVFS_VOLT1_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ2_##cluster##_##seg, CPU_DVFS_VOLT2_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ3_##cluster##_##seg, CPU_DVFS_VOLT3_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ4_##cluster##_##seg, CPU_DVFS_VOLT4_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ5_##cluster##_##seg, CPU_DVFS_VOLT5_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ6_##cluster##_##seg, CPU_DVFS_VOLT6_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ7_##cluster##_##seg, CPU_DVFS_VOLT7_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ8_##cluster##_##seg, CPU_DVFS_VOLT8_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ9_##cluster##_##seg, CPU_DVFS_VOLT9_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ10_##cluster##_##seg, CPU_DVFS_VOLT10_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ11_##cluster##_##seg, CPU_DVFS_VOLT11_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ12_##cluster##_##seg, CPU_DVFS_VOLT12_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ13_##cluster##_##seg, CPU_DVFS_VOLT13_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ14_##cluster##_##seg, CPU_DVFS_VOLT14_VPROC##vol##_##seg),	\
	OP(CPU_DVFS_FREQ15_##cluster##_##seg, CPU_DVFS_VOLT15_VPROC##vol##_##seg),	\
}

OPP_TBL(LL, FY, 0, 1);
OPP_TBL(LL, SB, 1, 1);
OPP_TBL(LL, FY2, 2, 1);

struct opp_tbl_info opp_tbls[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {		/* v1.1 */
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_LL_e0_0, ARRAY_SIZE(opp_tbl_LL_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_LL_e1_0, ARRAY_SIZE(opp_tbl_LL_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_LL_e2_0, ARRAY_SIZE(opp_tbl_LL_e2_0) },
	},
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_e0[] = {	/* FY */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_e1[] = {	/* SB */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_e2[] = {	/* SB */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {	/* v1.1 */
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_e0 },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_e1 },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_e2 },
	},
};
