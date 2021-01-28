/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "mtk_cpufreq_struct.h"
#include "mtk_cpufreq_config.h"

/* FY */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY		2000000		/* KHz */
#define CPU_DVFS_FREQ1_LL_FY		1933000		/* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1866000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1800000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1733000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1666000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1548000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		1475000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		1375000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		1275000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		1175000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		1075000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		999000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		925000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		850000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		774000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY		2200000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY		2133000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY		2066000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY		2000000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY		1933000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY		1866000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY		1800000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY		1651000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY		1503000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY		1414000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY		1295000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY		1176000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY		1087000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY		998000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY		909000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY		1353000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY		1306000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY		1260000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY		1190000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY		1155000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY		1120000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY		984000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY		917000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY		827000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY		737000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY		669000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY		579000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY		512000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY		445000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY		400000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_FY	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY	 98125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY	 95625		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY	 93125		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY	 91250		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY	 88750		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY	 84375		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY	 77500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY	 71875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY	 70000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY	 68125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 66250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 63750		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY	106875		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_FY	104375		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_FY	101250		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_FY	98750		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_FY	95625		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_FY	93125		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_FY	90000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_FY	86875		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_FY	83125		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_FY	80000		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_FY	78125		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_FY	75625		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_FY	73125		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_FY	71250		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_FY	69375		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_FY	67500		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_FY	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_FY	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_FY	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_FY	 88750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_FY	 86875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_FY	 84375		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_FY	 78750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_FY	 76250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_FY	 73750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_FY	 71875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_FY	 69375		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_FY	 67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_FY	 65625		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_FY	 63750		/* 10uV */

/* SBa */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_SBa		2000000		/* KHz */
#define CPU_DVFS_FREQ1_LL_SBa		1933000		/* KHz */
#define CPU_DVFS_FREQ2_LL_SBa		1866000		/* KHz */
#define CPU_DVFS_FREQ3_LL_SBa		1800000		/* KHz */
#define CPU_DVFS_FREQ4_LL_SBa		1733000		/* KHz */
#define CPU_DVFS_FREQ5_LL_SBa		1666000		/* KHz */
#define CPU_DVFS_FREQ6_LL_SBa		1548000		/* KHz */
#define CPU_DVFS_FREQ7_LL_SBa		1475000		/* KHz */
#define CPU_DVFS_FREQ8_LL_SBa		1375000		/* KHz */
#define CPU_DVFS_FREQ9_LL_SBa		1275000		/* KHz */
#define CPU_DVFS_FREQ10_LL_SBa		1175000		/* KHz */
#define CPU_DVFS_FREQ11_LL_SBa		1075000		/* KHz */
#define CPU_DVFS_FREQ12_LL_SBa		999000		/* KHz */
#define CPU_DVFS_FREQ13_LL_SBa		925000		/* KHz */
#define CPU_DVFS_FREQ14_LL_SBa		850000		/* KHz */
#define CPU_DVFS_FREQ15_LL_SBa		774000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_SBa		2350000		/* KHz */
#define CPU_DVFS_FREQ1_L_SBa		2287000		/* KHz */
#define CPU_DVFS_FREQ2_L_SBa		2225000		/* KHz */
#define CPU_DVFS_FREQ3_L_SBa		2150000		/* KHz */
#define CPU_DVFS_FREQ4_L_SBa		2066000		/* KHz */
#define CPU_DVFS_FREQ5_L_SBa		2000000		/* KHz */
#define CPU_DVFS_FREQ6_L_SBa		1933000		/* KHz */
#define CPU_DVFS_FREQ7_L_SBa		1866000		/* KHz */
#define CPU_DVFS_FREQ8_L_SBa		1800000		/* KHz */
#define CPU_DVFS_FREQ9_L_SBa		1621000		/* KHz */
#define CPU_DVFS_FREQ10_L_SBa		1473000		/* KHz */
#define CPU_DVFS_FREQ11_L_SBa		1325000		/* KHz */
#define CPU_DVFS_FREQ12_L_SBa		1176000		/* KHz */
#define CPU_DVFS_FREQ13_L_SBa		1057000		/* KHz */
#define CPU_DVFS_FREQ14_L_SBa		939000		/* KHz */
#define CPU_DVFS_FREQ15_L_SBa		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_SBa		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_SBa		1353000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_SBa		1306000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_SBa		1260000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_SBa		1190000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_SBa		1155000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_SBa		1120000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_SBa		1007000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_SBa		917000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_SBa		827000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_SBa		737000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_SBa		669000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_SBa		579000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_SBa		512000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_SBa		445000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_SBa		400000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_SBa	 95000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_SBa	 92500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_SBa	 90000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_SBa	 87500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_SBa	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_SBa	 82500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_SBa	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_SBa	 76875		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_SBa	 74375		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_SBa	 71875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_SBa	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_SBa	 66875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_SBa	 65000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_SBa	 63125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_SBa	 61250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_SBa	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_SBa	 102500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_SBa	 99375		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_SBa	 96250		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_SBa	 93125		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_SBa	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_SBa	 87500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_SBa	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_SBa	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_SBa	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_SBa	 76250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_SBa	 73125		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_SBa	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_SBa	 66875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_SBa	 64375		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_SBa	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_SBa	 60000		/* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_SBa	 95000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_SBa	 92500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_SBa	 90000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_SBa	 87500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_SBa	 83750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_SBa	 81875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_SBa	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_SBa	 76875		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_SBa	 74375		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_SBa	 71875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_SBa	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_SBa	 67500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_SBa	 65000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_SBa	 63125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_SBa	 61250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_SBa	 60000		/* 10uV */

/* PRO */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_PRO		2000000		/* KHz */
#define CPU_DVFS_FREQ1_LL_PRO		1933000		/* KHz */
#define CPU_DVFS_FREQ2_LL_PRO		1866000		/* KHz */
#define CPU_DVFS_FREQ3_LL_PRO		1800000		/* KHz */
#define CPU_DVFS_FREQ4_LL_PRO		1733000		/* KHz */
#define CPU_DVFS_FREQ5_LL_PRO		1666000		/* KHz */
#define CPU_DVFS_FREQ6_LL_PRO		1548000		/* KHz */
#define CPU_DVFS_FREQ7_LL_PRO		1475000		/* KHz */
#define CPU_DVFS_FREQ8_LL_PRO		1375000		/* KHz */
#define CPU_DVFS_FREQ9_LL_PRO		1275000		/* KHz */
#define CPU_DVFS_FREQ10_LL_PRO		1175000		/* KHz */
#define CPU_DVFS_FREQ11_LL_PRO		1075000		/* KHz */
#define CPU_DVFS_FREQ12_LL_PRO		999000		/* KHz */
#define CPU_DVFS_FREQ13_LL_PRO		925000		/* KHz */
#define CPU_DVFS_FREQ14_LL_PRO		850000		/* KHz */
#define CPU_DVFS_FREQ15_LL_PRO		774000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_PRO		2400000		/* KHz */
#define CPU_DVFS_FREQ1_L_PRO		2316000		/* KHz */
#define CPU_DVFS_FREQ2_L_PRO		2233000		/* KHz */
#define CPU_DVFS_FREQ3_L_PRO		2150000		/* KHz */
#define CPU_DVFS_FREQ4_L_PRO		2066000		/* KHz */
#define CPU_DVFS_FREQ5_L_PRO		2000000		/* KHz */
#define CPU_DVFS_FREQ6_L_PRO		1933000		/* KHz */
#define CPU_DVFS_FREQ7_L_PRO		1866000		/* KHz */
#define CPU_DVFS_FREQ8_L_PRO		1800000		/* KHz */
#define CPU_DVFS_FREQ9_L_PRO		1621000		/* KHz */
#define CPU_DVFS_FREQ10_L_PRO		1473000		/* KHz */
#define CPU_DVFS_FREQ11_L_PRO		1325000		/* KHz */
#define CPU_DVFS_FREQ12_L_PRO		1176000		/* KHz */
#define CPU_DVFS_FREQ13_L_PRO		1057000		/* KHz */
#define CPU_DVFS_FREQ14_L_PRO		939000		/* KHz */
#define CPU_DVFS_FREQ15_L_PRO		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_PRO		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_PRO		1353000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_PRO		1306000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_PRO		1260000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_PRO		1190000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_PRO		1155000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_PRO		1120000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_PRO		1007000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_PRO		917000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_PRO		827000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_PRO		737000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_PRO		669000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_PRO		579000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_PRO		512000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_PRO		445000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_PRO		400000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_PRO	 95000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_PRO	 92500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_PRO	 90000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_PRO	 87500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_PRO	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_PRO	 82500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_PRO	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_PRO	 76875		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_PRO	 74375		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_PRO	 71875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_PRO	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_PRO	 66875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_PRO	 65000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_PRO	 63125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_PRO	 61250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_PRO	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_PRO	 102500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_PRO	 99375		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_PRO	 96250		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_PRO	 93125		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_PRO	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_PRO	 87500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_PRO	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_PRO	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_PRO	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_PRO	 76250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_PRO	 73125		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_PRO	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_PRO	 66875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_PRO	 64375		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_PRO	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_PRO	 60000		/* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_PRO	 95000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_PRO	 92500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_PRO	 90000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_PRO	 87500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_PRO	 83750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_PRO	 81875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_PRO	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_PRO	 76875		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_PRO	 74375		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_PRO	 71875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_PRO	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_PRO	 67500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_PRO	 65000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_PRO	 63125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_PRO	 61250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_PRO	 60000		/* 10uV */

/* Lite  */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_LITE		1800000		/* KHz */
#define CPU_DVFS_FREQ1_LL_LITE		1733000		/* KHz */
#define CPU_DVFS_FREQ2_LL_LITE		1666000		/* KHz */
#define CPU_DVFS_FREQ3_LL_LITE		1548000		/* KHz */
#define CPU_DVFS_FREQ4_LL_LITE		1475000		/* KHz */
#define CPU_DVFS_FREQ5_LL_LITE		1375000		/* KHz */
#define CPU_DVFS_FREQ6_LL_LITE		1275000		/* KHz */
#define CPU_DVFS_FREQ7_LL_LITE		1175000		/* KHz */
#define CPU_DVFS_FREQ8_LL_LITE		1075000		/* KHz */
#define CPU_DVFS_FREQ9_LL_LITE		999000		/* KHz */
#define CPU_DVFS_FREQ10_LL_LITE		925000		/* KHz */
#define CPU_DVFS_FREQ11_LL_LITE		850000		/* KHz */
#define CPU_DVFS_FREQ12_LL_LITE		774000		/* KHz */
#define CPU_DVFS_FREQ13_LL_LITE		774000		/* KHz */
#define CPU_DVFS_FREQ14_LL_LITE		774000		/* KHz */
#define CPU_DVFS_FREQ15_LL_LITE		774000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_LITE		2000000		/* KHz */
#define CPU_DVFS_FREQ1_L_LITE		1933000		/* KHz */
#define CPU_DVFS_FREQ2_L_LITE		1866000		/* KHz */
#define CPU_DVFS_FREQ3_L_LITE		1800000		/* KHz */
#define CPU_DVFS_FREQ4_L_LITE		1651000		/* KHz */
#define CPU_DVFS_FREQ5_L_LITE		1503000		/* KHz */
#define CPU_DVFS_FREQ6_L_LITE		1414000		/* KHz */
#define CPU_DVFS_FREQ7_L_LITE		1295000		/* KHz */
#define CPU_DVFS_FREQ8_L_LITE		1176000		/* KHz */
#define CPU_DVFS_FREQ9_L_LITE		1087000		/* KHz */
#define CPU_DVFS_FREQ10_L_LITE		998000		/* KHz */
#define CPU_DVFS_FREQ11_L_LITE		909000		/* KHz */
#define CPU_DVFS_FREQ12_L_LITE		850000		/* KHz */
#define CPU_DVFS_FREQ13_L_LITE		850000		/* KHz */
#define CPU_DVFS_FREQ14_L_LITE		850000		/* KHz */
#define CPU_DVFS_FREQ15_L_LITE		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_LITE		1260000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_LITE		1190000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_LITE		1155000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_LITE		1120000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_LITE		984000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_LITE		917000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_LITE		827000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_LITE		737000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_LITE		669000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_LITE		579000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_LITE	512000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_LITE	445000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_LITE	400000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_LITE	400000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_LITE	400000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_LITE	400000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_LITE	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_LITE	 98125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_LITE	 96250		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_LITE	 91875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_LITE	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_LITE	 85625		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_LITE	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_LITE	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_LITE	 75000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_LITE	 72500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_LITE	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_LITE	 66875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_LITE	 63750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_LITE	 63750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_LITE	 63750		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_LITE	 63750		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_LITE	105000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_LITE	102500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_LITE	 99375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_LITE	 96250		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_LITE	 91875		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_LITE	 87500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_LITE	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_LITE	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_LITE	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_LITE	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_LITE	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_LITE	 69375		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_LITE	 67500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_LITE	 67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_LITE	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_LITE	 67500		/* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_LITE	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_LITE	 96250		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_LITE	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_LITE	 91875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_LITE	 86875		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_LITE	 84375		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_LITE	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_LITE	 77500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_LITE	 74375		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_LITE	 71250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_LITE	 68125		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_LITE	 65625		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_LITE	 63750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_LITE	 63750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_LITE	 63750		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_LITE	 63750		/* 10uV */

/* P65 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_P95		2000000		/* KHz */
#define CPU_DVFS_FREQ1_LL_P95		1933000		/* KHz */
#define CPU_DVFS_FREQ2_LL_P95		1866000		/* KHz */
#define CPU_DVFS_FREQ3_LL_P95		1800000		/* KHz */
#define CPU_DVFS_FREQ4_LL_P95		1733000		/* KHz */
#define CPU_DVFS_FREQ5_LL_P95		1666000		/* KHz */
#define CPU_DVFS_FREQ6_LL_P95		1548000		/* KHz */
#define CPU_DVFS_FREQ7_LL_P95		1475000		/* KHz */
#define CPU_DVFS_FREQ8_LL_P95		1375000		/* KHz */
#define CPU_DVFS_FREQ9_LL_P95		1275000		/* KHz */
#define CPU_DVFS_FREQ10_LL_P95		1175000		/* KHz */
#define CPU_DVFS_FREQ11_LL_P95		1075000		/* KHz */
#define CPU_DVFS_FREQ12_LL_P95		999000		/* KHz */
#define CPU_DVFS_FREQ13_LL_P95		925000		/* KHz */
#define CPU_DVFS_FREQ14_LL_P95		850000		/* KHz */
#define CPU_DVFS_FREQ15_LL_P95		774000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_P95		2300000		/* KHz */
#define CPU_DVFS_FREQ1_L_P95		2133000		/* KHz */
#define CPU_DVFS_FREQ2_L_P95		2066000		/* KHz */
#define CPU_DVFS_FREQ3_L_P95		2000000		/* KHz */
#define CPU_DVFS_FREQ4_L_P95		1933000		/* KHz */
#define CPU_DVFS_FREQ5_L_P95		1866000		/* KHz */
#define CPU_DVFS_FREQ6_L_P95		1800000		/* KHz */
#define CPU_DVFS_FREQ7_L_P95		1651000		/* KHz */
#define CPU_DVFS_FREQ8_L_P95		1503000		/* KHz */
#define CPU_DVFS_FREQ9_L_P95		1414000		/* KHz */
#define CPU_DVFS_FREQ10_L_P95		1295000		/* KHz */
#define CPU_DVFS_FREQ11_L_P95		1176000		/* KHz */
#define CPU_DVFS_FREQ12_L_P95		1087000		/* KHz */
#define CPU_DVFS_FREQ13_L_P95		998000		/* KHz */
#define CPU_DVFS_FREQ14_L_P95		909000		/* KHz */
#define CPU_DVFS_FREQ15_L_P95		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_P95		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_P95		1353000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_P95		1306000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_P95		1260000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_P95		1190000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_P95		1155000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_P95		1120000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_P95		984000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_P95		917000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_P95		827000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_P95		737000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_P95		669000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_P95		579000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_P95		512000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_P95		445000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_P95		400000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_P95	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_P95	 98125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_P95	 95625		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_P95	 93125		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_P95	 91250		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_P95	 88750		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_P95	 84375		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_P95	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_P95	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_P95	 77500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_P95	 75000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_P95	 71875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_P95	 70000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_P95	 68125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_P95	 66250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_P95	 63750		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_P95	111875		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_P95	104375		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_P95	101250		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_P95	98750		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_P95	95625		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_P95	93125		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_P95	90000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_P95	86875		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_P95	83125		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_P95	80000		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_P95	78125		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_P95	75625		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_P95	73125		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_P95	71250		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_P95	69375		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_P95	67500		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_P95	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_P95	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_P95	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_P95	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_P95	 88750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_P95	 86875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_P95	 84375		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_P95	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_P95	 78750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_P95	 76250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_P95	 73750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_P95	 71875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_P95	 69375		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_P95	 67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_P95	 65625		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_P95	 63750		/* 10uV */


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

OPP_TBL(LL,   FY, 0, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  FY, 0, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, FY, 0, 3); /* opp_tbl_CCI_e0_0 */

OPP_TBL(LL,  SBa, 1, 1); /* opp_tbl_LL_e1_0   */
OPP_TBL(L,  SBa, 1, 2); /* opp_tbl_L_e1_0  */
OPP_TBL(CCI, SBa, 1, 3); /* opp_tbl_CCI_e1_0 */

OPP_TBL(LL,  PRO, 2, 1); /* opp_tbl_LL_e2_0   */
OPP_TBL(L,  PRO, 2, 2); /* opp_tbl_L_e2_0  */
OPP_TBL(CCI, PRO, 2, 3); /* opp_tbl_CCI_e2_0 */

OPP_TBL(LL,  LITE, 3, 1); /* opp_tbl_LL_e3_0   */
OPP_TBL(L,  LITE, 3, 2); /* opp_tbl_L_e3_0  */
OPP_TBL(CCI, LITE, 3, 3); /* opp_tbl_CCI_e3_0 */

OPP_TBL(LL,  P95, 4, 1); /* opp_tbl_LL_e4_0   */
OPP_TBL(L,   P95, 4, 2); /* opp_tbl_L_e4_0  */
OPP_TBL(CCI, P95, 4, 3); /* opp_tbl_CCI_e4_0 */




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

	},
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_FY[] = {	/* FY */
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
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_FY[] = {	/* FY */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_FY[] = {	/* FY */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_SBa[] = {	/* SBa */
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
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_SBa[] = {	/* SBa */
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
	FP(1,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_SBa[] = {	/* SBa */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_PRO[] = {	/* PRO */
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
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_PRO[] = {	/* PRO */
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
	FP(1,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_PRO[] = {	/* PRO */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_LITE[] = {	/* LITE */
	/* POS,	CLK */
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
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_LITE[] = {	/* LITE */
	/* POS,	CLK */
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
	FP(2,	1),
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_LITE[] = {	/* LITE */
	/* POS,	CLK */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_P95[] = {	/* P95 */
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
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_P95[] = {	/* P95 */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_P95[] = {	/* P95 */
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
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
	FP(2,	2),
};

struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_SBa },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_PRO },
		[CPU_LEVEL_3] = { opp_tbl_method_LL_LITE },
		[CPU_LEVEL_4] = { opp_tbl_method_LL_P95 },
	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_L_SBa },
		[CPU_LEVEL_2] = { opp_tbl_method_L_PRO },
		[CPU_LEVEL_3] = { opp_tbl_method_L_LITE },
		[CPU_LEVEL_4] = { opp_tbl_method_L_P95 },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_SBa },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_PRO },
		[CPU_LEVEL_3] = { opp_tbl_method_CCI_LITE },
		[CPU_LEVEL_4] = { opp_tbl_method_CCI_P95 },
	},
};
