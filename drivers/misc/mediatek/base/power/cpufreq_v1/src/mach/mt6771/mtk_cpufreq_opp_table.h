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

/* V3 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY		1989000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY		1924000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1846000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1781000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1716000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1677000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1625000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		1586000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		1508000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		1417000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		1326000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		1248000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		1131000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		1014000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		910000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		793000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY			1989000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY			1924000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY			1846000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY			1781000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY			1716000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY			1677000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY			1625000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY			1586000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY			1508000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY			1417000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY		1326000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY		1248000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY		1131000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY		1014000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY		910000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY		793000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY		1196000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY		1144000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY		1092000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY		1027000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY		962000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY		923000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY		871000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY		845000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY		767000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY		689000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY		624000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY		546000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY		481000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY		403000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY		338000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY		273000		/* KHz */

/* for DVFS OPP table L|CCI */
#define CPU_DVFS_VOLT0_VPROC1_FY	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY	 86250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY	 85000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY	 77500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY	 72500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY	 70000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 65000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_FY	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_FY	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_FY	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_FY	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_FY	 86250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_FY	 85000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_FY	 77500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_FY	 72500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_FY	 70000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_FY	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_FY	 65000		/* 10uV */

/* V4 & V5_1*/
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY2		1989000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY2		1924000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY2		1846000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY2		1781000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY2		1716000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY2		1677000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY2		1625000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY2		1586000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY2		1508000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY2		1417000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY2		1326000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY2		1248000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY2		1131000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY2		1014000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY2		910000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY2		793000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY2		1989000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY2		1924000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY2		1846000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY2		1781000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY2		1716000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY2		1677000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY2		1625000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY2		1586000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY2		1508000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY2		1417000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY2		1326000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY2		1248000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY2		1131000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY2		1014000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY2		910000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY2		793000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY2		1196000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY2		1144000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY2		1092000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY2		1027000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY2		962000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY2		923000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY2		871000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY2		845000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY2		767000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY2		689000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY2		624000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY2		546000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY2		481000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY2		403000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY2		338000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY2		273000		/* KHz */

/* for DVFS OPP table L|CCI */
#define CPU_DVFS_VOLT0_VPROC1_FY2	 95000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY2	 92500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY2	 90000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY2	 87500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY2	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY2	 83125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY2	 81250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY2	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY2	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY2	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY2	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY2	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY2	 67500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY2	 65000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY2	 62500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY2	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY2	 98125		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_FY2	 95625		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_FY2	 93125		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_FY2	 90625		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_FY2	 88125		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_FY2	 86250		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_FY2	 84375		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_FY2	 83125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_FY2	 80625		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_FY2	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_FY2	 75625		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_FY2	 73125		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_FY2	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_FY2	 68125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_FY2	 65625		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_FY2	 63125		/* 10uV */

/* V5_2 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY3		1989000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY3		1924000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY3		1846000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY3		1781000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY3		1716000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY3		1677000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY3		1625000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY3		1586000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY3		1508000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY3		1417000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY3		1326000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY3		1248000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY3		1131000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY3		1014000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY3		910000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY3		793000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY3		1989000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY3		1924000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY3		1846000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY3		1781000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY3		1716000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY3		1677000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY3		1625000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY3		1586000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY3		1508000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY3		1417000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY3		1326000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY3		1248000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY3		1131000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY3		1014000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY3		910000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY3		793000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY3		1196000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY3		1144000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY3		1092000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY3		1027000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY3		962000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY3		923000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY3		871000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY3		845000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY3		767000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY3		689000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY3		624000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY3		546000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY3		481000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY3		403000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY3		338000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY3		273000		/* KHz */

/* for DVFS OPP table L|CCI */
#define CPU_DVFS_VOLT0_VPROC1_FY3	 97500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY3	 95000		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY3	 92500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY3	 90000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY3	 87500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY3	 85625		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY3	 83750		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY3	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY3	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY3	 77500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY3	 75000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY3	 71875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY3	 69375		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY3	 66250		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY3	 63125		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY3	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY3	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_FY3	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_FY3	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_FY3	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_FY3	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_FY3	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_FY3	 86250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_FY3	 85000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_FY3	 82500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_FY3	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_FY3	 77500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_FY3	 75000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_FY3	 72500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_FY3	 70000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_FY3	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_FY3	 65000		/* 10uV */

/* V5_3 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY4		1989000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY4		1924000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY4		1846000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY4		1781000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY4		1716000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY4		1677000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY4		1625000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY4		1586000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY4		1508000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY4		1417000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY4		1326000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY4		1248000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY4		1131000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY4		1014000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY4		910000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY4		793000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY4		1989000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY4		1924000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY4		1846000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY4		1781000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY4		1716000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY4		1677000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY4		1625000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY4		1586000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY4		1508000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY4		1417000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY4		1326000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY4		1248000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY4		1131000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY4		1014000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY4		910000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY4		793000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY4		1196000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY4		1144000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY4		1092000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY4		1027000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY4		962000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY4		923000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY4		871000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY4		845000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY4		767000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY4		689000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY4		624000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY4		546000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY4		481000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY4		403000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY4		338000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY4		273000		/* KHz */

/* for DVFS OPP table L|CCI */
#define CPU_DVFS_VOLT0_VPROC1_FY4	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY4	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY4	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY4	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY4	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY4	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY4	 85625		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY4	 84375		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY4	 81875		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY4	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY4	 76875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY4	 73750		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY4	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY4	 66875		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY4	 63750		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY4	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY4	 102500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_FY4	 100000		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_FY4	 97500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_FY4	 95000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_FY4	 92500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_FY4	 90625		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_FY4	 88750		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_FY4	 87500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_FY4	 85000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_FY4	 82500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_FY4	 80000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_FY4	 77500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_FY4	 75000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_FY4	 72500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_FY4	 70000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_FY4	 67500		/* 10uV */

/* V6 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY5		1807000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY5		1729000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY5		1664000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY5		1612000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY5		1547000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY5		1495000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY5		1456000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY5		1430000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY5		1365000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY5		1287000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY5		1209000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY5		1118000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY5		1027000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY5		962000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY5		884000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY5		793000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY5		1807000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY5		1729000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY5		1664000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY5		1612000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY5		1547000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY5		1495000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY5		1456000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY5		1430000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY5		1378000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY5		1300000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY5		1222000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY5		1144000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY5		1040000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY5		962000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY5		871000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY5		793000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY5		1079000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY5		1027000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY5		988000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY5		936000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY5		884000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY5		819000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY5		780000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY5		754000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY5		689000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY5		637000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY5		572000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY5		494000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY5		429000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY5		377000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY5		325000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY5		273000		/* KHz */

/* for DVFS OPP table L|CCI */
#define CPU_DVFS_VOLT0_VPROC1_FY5	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY5	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY5	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY5	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY5	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY5	 87500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY5	 85625		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY5	 84375		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY5	 81875		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY5	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY5	 76875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY5	 73750		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY5	 70000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY5	 66875		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY5	 63750		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY5	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY5	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_FY5	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_FY5	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_FY5	 93125		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_FY5	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_FY5	 87500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_FY5	 86250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_FY5	 85000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_FY5	 83125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_FY5	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_FY5	 78125		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_FY5	 75625		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_FY5	 72500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_FY5	 70000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_FY5	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_FY5	 65000		/* 10uV */

/* V5_T*/
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY5T		1989000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY5T		1924000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY5T		1846000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY5T		1781000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY5T		1716000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY5T		1677000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY5T		1625000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY5T		1586000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY5T		1508000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY5T		1417000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY5T		1326000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY5T		1248000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY5T		1131000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY5T		1014000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY5T		910000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY5T		793000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY5T		2106000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY5T		1924000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY5T		1846000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY5T		1781000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY5T		1716000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY5T		1677000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY5T		1625000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY5T		1586000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY5T		1508000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY5T		1417000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY5T		1326000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY5T		1248000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY5T		1131000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY5T		1014000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY5T		910000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY5T		793000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY5T		1196000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY5T		1144000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY5T		1092000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY5T		1027000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY5T		962000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY5T		923000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY5T		871000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY5T		845000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY5T		767000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY5T		689000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY5T		624000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY5T		546000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY5T		481000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY5T		403000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY5T		338000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY5T		273000		/* KHz */

/* for DVFS OPP table L|CCI */
#define CPU_DVFS_VOLT0_VPROC1_FY5T	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY5T	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY5T	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY5T	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY5T	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY5T	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY5T	 85625		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY5T	 84375		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY5T	 81875		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY5T	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY5T	 76875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY5T	 73750		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY5T	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY5T	 66875		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY5T	 63750		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY5T	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY5T	 105000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_FY5T	 100000		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_FY5T	 97500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_FY5T	 95000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_FY5T	 92500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_FY5T	 90625		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_FY5T	 88750		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_FY5T	 87500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_FY5T	 85000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_FY5T	 82500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_FY5T	 80000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_FY5T	 77500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_FY5T	 75000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_FY5T	 72500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_FY5T	 70000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_FY5T	 67500		/* 10uV */

/* V5_4 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY54		1989000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY54		1924000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY54		1846000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY54		1781000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY54		1716000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY54		1677000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY54		1625000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY54		1586000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY54		1508000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY54		1417000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY54		1326000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY54		1248000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY54		1131000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY54		1014000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY54		910000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY54		793000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY54		1989000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY54		1924000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY54		1846000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY54		1781000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY54		1716000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY54		1677000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY54		1625000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY54		1586000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY54		1508000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY54		1417000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY54		1326000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY54		1248000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY54		1131000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY54		1014000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY54		910000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY54		793000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY54		1196000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY54		1144000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY54		1092000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY54		1027000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY54		962000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY54		923000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY54		871000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY54		845000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY54		767000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY54		689000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY54		624000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY54		546000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY54		481000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY54		403000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY54		338000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY54		273000		/* KHz */

/* for DVFS OPP table L|CCI */
#define CPU_DVFS_VOLT0_VPROC1_FY54	 105000	/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY54	 102500	/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY54	 100000	/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY54	  97500	/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY54	  94375	/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY54	  93125	/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY54	  90625	/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY54	  89375	/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY54	  86875	/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY54	  85000	/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY54	  81875	/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY54	  80000	/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY54	  75625	/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY54	  71875	/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY54	  68750	/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY54	  65000	/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY54	 105000	/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_FY54	 102500	/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_FY54	 100000	/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_FY54	  97500	/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_FY54	  95000	/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_FY54	  93125	/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_FY54	  91250	/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_FY54	  90000	/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_FY54	  87500	/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_FY54	  85000	/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_FY54	  82500	/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_FY54	  80000	/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_FY54	  77500	/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_FY54	  75000	/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_FY54	  72500	/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_FY54	  70000	/* 10uV */

/* DVFS OPP table */
#define OPP_TBL(cluster, seg, lv, vol)	\
static struct mt_cpu_freq_info opp_tbl_##cluster##_e##lv##_0[] = {	\
	OP                                                              \
(CPU_DVFS_FREQ0_##cluster##_##seg, CPU_DVFS_VOLT0_VPROC##vol##_##seg),	\
	OP                                                              \
(CPU_DVFS_FREQ1_##cluster##_##seg, CPU_DVFS_VOLT1_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ2_##cluster##_##seg, CPU_DVFS_VOLT2_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ3_##cluster##_##seg, CPU_DVFS_VOLT3_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ4_##cluster##_##seg, CPU_DVFS_VOLT4_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ5_##cluster##_##seg, CPU_DVFS_VOLT5_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ6_##cluster##_##seg, CPU_DVFS_VOLT6_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ7_##cluster##_##seg, CPU_DVFS_VOLT7_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ8_##cluster##_##seg, CPU_DVFS_VOLT8_VPROC##vol##_##seg),	\
	OP                                                               \
(CPU_DVFS_FREQ9_##cluster##_##seg, CPU_DVFS_VOLT9_VPROC##vol##_##seg),	\
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
/* V3 */
OPP_TBL(LL, FY, 0, 1);
OPP_TBL(L, FY, 0, 2);
OPP_TBL(CCI, FY, 0, 1);
/* V4 */
OPP_TBL(LL, FY2, 1, 1);
OPP_TBL(L, FY2, 1, 2);
OPP_TBL(CCI, FY2, 1, 1);
/* V5_1 */
OPP_TBL(LL, FY2, 2, 1);
OPP_TBL(L, FY2, 2, 2);
OPP_TBL(CCI, FY2, 2, 1);
/* V5_2 */
OPP_TBL(LL, FY3, 3, 1);
OPP_TBL(L, FY3, 3, 2);
OPP_TBL(CCI, FY3, 3, 1);
/* V5_3 */
OPP_TBL(LL, FY4, 4, 1);
OPP_TBL(L, FY4, 4, 2);
OPP_TBL(CCI, FY4, 4, 1);
/* V6 */
OPP_TBL(LL, FY5, 5, 1);
OPP_TBL(L, FY5, 5, 2);
OPP_TBL(CCI, FY5, 5, 1);
/* V5_T */
OPP_TBL(LL, FY5T, 6, 1);
OPP_TBL(L, FY5T, 6, 2);
OPP_TBL(CCI, FY5T, 6, 1);
/* V5_4 */
OPP_TBL(LL, FY54, 7, 1);
OPP_TBL(L, FY54, 7, 2);
OPP_TBL(CCI, FY54, 7, 1);

struct opp_tbl_info opp_tbls[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_LL_e0_0,
				ARRAY_SIZE(opp_tbl_LL_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_LL_e1_0,
				ARRAY_SIZE(opp_tbl_LL_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_LL_e2_0,
				ARRAY_SIZE(opp_tbl_LL_e2_0) },
		[CPU_LEVEL_3] = { opp_tbl_LL_e3_0,
				ARRAY_SIZE(opp_tbl_LL_e3_0) },
		[CPU_LEVEL_4] = { opp_tbl_LL_e4_0,
				ARRAY_SIZE(opp_tbl_LL_e4_0) },
		[CPU_LEVEL_5] = { opp_tbl_LL_e5_0,
				ARRAY_SIZE(opp_tbl_LL_e5_0) },
		[CPU_LEVEL_6] = { opp_tbl_LL_e6_0,
				ARRAY_SIZE(opp_tbl_LL_e6_0) },
		[CPU_LEVEL_7] = { opp_tbl_LL_e7_0,
				ARRAY_SIZE(opp_tbl_LL_e7_0) },
	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_L_e0_0,
				ARRAY_SIZE(opp_tbl_L_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_L_e1_0,
				ARRAY_SIZE(opp_tbl_L_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_L_e2_0,
				ARRAY_SIZE(opp_tbl_L_e2_0) },
		[CPU_LEVEL_3] = { opp_tbl_L_e3_0,
				ARRAY_SIZE(opp_tbl_L_e3_0) },
		[CPU_LEVEL_4] = { opp_tbl_L_e4_0,
				ARRAY_SIZE(opp_tbl_L_e4_0) },
		[CPU_LEVEL_5] = { opp_tbl_L_e5_0,
				ARRAY_SIZE(opp_tbl_L_e5_0) },
		[CPU_LEVEL_6] = { opp_tbl_L_e6_0,
				ARRAY_SIZE(opp_tbl_L_e6_0) },
		[CPU_LEVEL_7] = { opp_tbl_L_e7_0,
				ARRAY_SIZE(opp_tbl_L_e7_0) },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_CCI_e0_0,
				ARRAY_SIZE(opp_tbl_CCI_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_CCI_e1_0,
				ARRAY_SIZE(opp_tbl_CCI_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_CCI_e2_0,
				ARRAY_SIZE(opp_tbl_CCI_e2_0) },
		[CPU_LEVEL_3] = { opp_tbl_CCI_e3_0,
				ARRAY_SIZE(opp_tbl_CCI_e3_0) },
		[CPU_LEVEL_4] = { opp_tbl_CCI_e4_0,
				ARRAY_SIZE(opp_tbl_CCI_e4_0) },
		[CPU_LEVEL_5] = { opp_tbl_CCI_e5_0,
				ARRAY_SIZE(opp_tbl_CCI_e5_0) },
		[CPU_LEVEL_6] = { opp_tbl_CCI_e6_0,
				ARRAY_SIZE(opp_tbl_CCI_e6_0) },
		[CPU_LEVEL_7] = { opp_tbl_CCI_e7_0,
				ARRAY_SIZE(opp_tbl_CCI_e7_0) },
	},
};

/* 16 steps OPP table */
/* < V6 */
static struct mt_cpu_freq_method opp_tbl_method_LL_e0[] = {
	/* POS,	CLK */
	FP(1,	1),
	FP(1,	1),
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
};

static struct mt_cpu_freq_method opp_tbl_method_L_e0[] = {
	/* POS,	CLK */
	FP(1,	1),
	FP(1,	1),
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
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_e0[] = {
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(4,	2),
	FP(4,	2),
};

/* 16 steps OPP table */
/* V6 */
static struct mt_cpu_freq_method opp_tbl_method_LL_e1[] = {
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
	FP(2,	1),
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_e1[] = {	/* FY2 */
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
	FP(2,	1),
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_e1[] = {
	/* POS,	CLK */
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(4,	2),
	FP(4,	2),
};

struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_e0 },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_e0 },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_e0 },
		[CPU_LEVEL_3] = { opp_tbl_method_LL_e0 },
		[CPU_LEVEL_4] = { opp_tbl_method_LL_e0 },
		[CPU_LEVEL_5] = { opp_tbl_method_LL_e1 },
		[CPU_LEVEL_6] = { opp_tbl_method_LL_e0 },
		[CPU_LEVEL_7] = { opp_tbl_method_LL_e0 },
	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_e0 },
		[CPU_LEVEL_1] = { opp_tbl_method_L_e0 },
		[CPU_LEVEL_2] = { opp_tbl_method_L_e0 },
		[CPU_LEVEL_3] = { opp_tbl_method_L_e0 },
		[CPU_LEVEL_4] = { opp_tbl_method_L_e0 },
		[CPU_LEVEL_5] = { opp_tbl_method_L_e1 },
		[CPU_LEVEL_6] = { opp_tbl_method_L_e0 },
		[CPU_LEVEL_7] = { opp_tbl_method_L_e0 },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_e0 },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_e0 },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_e0 },
		[CPU_LEVEL_3] = { opp_tbl_method_CCI_e0 },
		[CPU_LEVEL_4] = { opp_tbl_method_CCI_e0 },
		[CPU_LEVEL_5] = { opp_tbl_method_CCI_e1 },
		[CPU_LEVEL_6] = { opp_tbl_method_CCI_e0 },
		[CPU_LEVEL_7] = { opp_tbl_method_CCI_e0 },
	},
};
