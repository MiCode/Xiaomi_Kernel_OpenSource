// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "mtk_cpufreq_struct.h"
#include "mtk_cpufreq_config.h"

/* 6873 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY            2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_FY            1875000         /* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1687000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1625000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1541000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		1500000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		1358000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		1181000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		1075000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		968000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		862000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		756000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY		2000000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY		1950000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY		1900000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY		1850000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY		1800000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY		1716000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY		1633000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY		1548000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY		1383000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY		1258000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY		1175000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY		1091000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY		1008000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY		925000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY		841000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY		774000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY		1340000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY		1308000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY		1265000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY		1258000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY		1190000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY		1103000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY		1017000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY		960000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY		902000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY		873000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY		816000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY		730000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY		672000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY		615000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY		557000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY		500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY	 88750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY	 86875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY	 85625		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY	 83750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY	 81875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY	 78125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY	 71250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY	 66875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY	 64375		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY        62500          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 60000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY	92500		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_FY	90625		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_FY	88750		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_FY	86875		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_FY	85000		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_FY	83125		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_FY	80625		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_FY	80000		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_FY	75000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_FY	71875		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_FY	70000		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_FY	67500		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_FY	65625		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_FY	63750		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_FY	61875		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_FY	60000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_FY	 88750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_FY	 83750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_FY	 73125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_FY	 71250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_FY	 70000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_FY	 68750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_FY	 68125		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_FY	 66875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_FY	 65000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_FY	 63750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_FY	 62500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_FY	 61250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_FY	 60000		/* 10uV */

/* 6873_TB */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_TB            2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_TB            1875000         /* KHz */
#define CPU_DVFS_FREQ2_LL_TB		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_TB		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_TB		1687000		/* KHz */
#define CPU_DVFS_FREQ5_LL_TB		1625000		/* KHz */
#define CPU_DVFS_FREQ6_LL_TB		1541000		/* KHz */
#define CPU_DVFS_FREQ7_LL_TB		1500000		/* KHz */
#define CPU_DVFS_FREQ8_LL_TB		1358000		/* KHz */
#define CPU_DVFS_FREQ9_LL_TB		1181000		/* KHz */
#define CPU_DVFS_FREQ10_LL_TB		1075000		/* KHz */
#define CPU_DVFS_FREQ11_LL_TB		968000		/* KHz */
#define CPU_DVFS_FREQ12_LL_TB		862000		/* KHz */
#define CPU_DVFS_FREQ13_LL_TB		756000		/* KHz */
#define CPU_DVFS_FREQ14_LL_TB		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_TB		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_TB             2600000         /* KHz */
#define CPU_DVFS_FREQ1_L_TB             2433000         /* KHz */
#define CPU_DVFS_FREQ2_L_TB             2266000         /* KHz */
#define CPU_DVFS_FREQ3_L_TB             2133000         /* KHz */
#define CPU_DVFS_FREQ4_L_TB             1933000         /* KHz */
#define CPU_DVFS_FREQ5_L_TB             1800000         /* KHz */
#define CPU_DVFS_FREQ6_L_TB             1633000         /* KHz */
#define CPU_DVFS_FREQ7_L_TB             1548000         /* KHz */
#define CPU_DVFS_FREQ8_L_TB             1383000         /* KHz */
#define CPU_DVFS_FREQ9_L_TB             1300000         /* KHz */
#define CPU_DVFS_FREQ10_L_TB            1175000         /* KHz */
#define CPU_DVFS_FREQ11_L_TB            1133000         /* KHz */
#define CPU_DVFS_FREQ12_L_TB            1050000         /* KHz */
#define CPU_DVFS_FREQ13_L_TB            925000          /* KHz */
#define CPU_DVFS_FREQ14_L_TB            841000          /* KHz */
#define CPU_DVFS_FREQ15_L_TB            774000          /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_TB		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_TB		1356000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_TB		1295000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_TB		1260000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_TB		1190000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_TB		1103000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_TB		1017000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_TB		960000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_TB		902000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_TB		873000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_TB		816000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_TB		730000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_TB		672000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_TB		615000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_TB		557000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_TB		500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_TB	 92500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_TB	 88750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_TB	 86875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_TB	 85625		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_TB	 83750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_TB	 81875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_TB	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_TB	 78125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_TB	 75000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_TB	 71250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_TB	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_TB	 66875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_TB	 64375		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_TB	 62500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_TB	 60000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_TB	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_TB        100000          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_TB        96875           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_TB        93750           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_TB        91250           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_TB        87500           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_TB        85000           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_TB        80000           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_TB        78750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_TB        75000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_TB        73125           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_TB       70000           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_TB       68750           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_TB       66875           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_TB       63750           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_TB       61875           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_TB       60000           /* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_TB	 92500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_TB	 88750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_TB	 83750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_TB	 80000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_TB	 75000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_TB	 73125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_TB	 71250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_TB	 70000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_TB	 68750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_TB	 68125		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_TB	 66875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_TB	 65000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_TB	 63750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_TB	 62500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_TB	 61250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_TB	 60000		/* 10uV */

/* 6873T */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_6873T         2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_6873T         1875000         /* KHz */
#define CPU_DVFS_FREQ2_LL_6873T		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_6873T		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_6873T		1687000		/* KHz */
#define CPU_DVFS_FREQ5_LL_6873T		1625000		/* KHz */
#define CPU_DVFS_FREQ6_LL_6873T		1541000		/* KHz */
#define CPU_DVFS_FREQ7_LL_6873T		1500000		/* KHz */
#define CPU_DVFS_FREQ8_LL_6873T		1358000		/* KHz */
#define CPU_DVFS_FREQ9_LL_6873T		1181000		/* KHz */
#define CPU_DVFS_FREQ10_LL_6873T	1075000		/* KHz */
#define CPU_DVFS_FREQ11_LL_6873T	968000		/* KHz */
#define CPU_DVFS_FREQ12_LL_6873T	862000		/* KHz */
#define CPU_DVFS_FREQ13_LL_6873T	756000		/* KHz */
#define CPU_DVFS_FREQ14_LL_6873T	650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_6873T	500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_6873T		2200000		/* KHz */
#define CPU_DVFS_FREQ1_L_6873T		2100000		/* KHz */
#define CPU_DVFS_FREQ2_L_6873T		1983000		/* KHz */
#define CPU_DVFS_FREQ3_L_6873T		1933000		/* KHz */
#define CPU_DVFS_FREQ4_L_6873T		1866000		/* KHz */
#define CPU_DVFS_FREQ5_L_6873T		1800000		/* KHz */
#define CPU_DVFS_FREQ6_L_6873T		1620000		/* KHz */
#define CPU_DVFS_FREQ7_L_6873T		1508000		/* KHz */
#define CPU_DVFS_FREQ8_L_6873T		1466000		/* KHz */
#define CPU_DVFS_FREQ9_L_6873T		1383000		/* KHz */
#define CPU_DVFS_FREQ10_L_6873T		1300000		/* KHz */
#define CPU_DVFS_FREQ11_L_6873T		1175000		/* KHz */
#define CPU_DVFS_FREQ12_L_6873T		1133000		/* KHz */
#define CPU_DVFS_FREQ13_L_6873T		1050000		/* KHz */
#define CPU_DVFS_FREQ14_L_6873T		925000		/* KHz */
#define CPU_DVFS_FREQ15_L_6873T		774000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_6873T	1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_6873T	1356000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_6873T	1295000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_6873T	1260000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_6873T	1190000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_6873T	1103000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_6873T	1017000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_6873T	960000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_6873T	902000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_6873T	873000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_6873T	816000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_6873T	730000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_6873T	672000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_6873T	615000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_6873T	557000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_6873T	500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_6873T	 92500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_6873T	 87500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_6873T	 86875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_6873T	 85625		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_6873T	 83750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_6873T	 81875		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_6873T	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_6873T	 78125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_6873T	 75000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_6873T	 71250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_6873T	 69375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_6873T	 66875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_6873T	 64375		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_6873T	 62500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_6873T	 60000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_6873T	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_6873T	100000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_6873T	95625		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_6873T	90000		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_6873T	87500		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_6873T	84375		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_6873T	80000		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_6873T	77500		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_6873T	75000		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_6873T	74375		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_6873T	72500		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_6873T	70625		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_6873T	68125		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_6873T	67500		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_6873T	65625		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_6873T	63125		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_6873T	60000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_6873T	 92500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_6873T	 88750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_6873T	 83750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_6873T	 80000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_6873T	 75000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_6873T	 73125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_6873T	 71250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_6873T	 70000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_6873T	 68750		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_6873T	 68125		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_6873T	 66875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_6873T	 65000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_6873T	 63750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_6873T	 62500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_6873T	 61250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_6873T	 60000		/* 10uV */

/* 6873 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FYv1          1181000         /* KHz */
#define CPU_DVFS_FREQ1_LL_FYv1          1181000         /* KHz */
#define CPU_DVFS_FREQ2_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FYv1		1181000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FYv1		1075000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FYv1		968000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FYv1		897000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FYv1		827000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FYv1		720000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FYv1		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ1_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ2_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ3_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ4_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ5_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ6_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ7_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ8_L_FYv1		1383000		/* KHz */
#define CPU_DVFS_FREQ9_L_FYv1		1258000		/* KHz */
#define CPU_DVFS_FREQ10_L_FYv1		1175000		/* KHz */
#define CPU_DVFS_FREQ11_L_FYv1		1091000		/* KHz */
#define CPU_DVFS_FREQ12_L_FYv1		1008000		/* KHz */
#define CPU_DVFS_FREQ13_L_FYv1		925000		/* KHz */
#define CPU_DVFS_FREQ14_L_FYv1		841000		/* KHz */
#define CPU_DVFS_FREQ15_L_FYv1		774000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FYv1		1341000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FYv1		1306000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FYv1		1260000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FYv1		1190000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FYv1		1103000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FYv1		1075000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FYv1		1017000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FYv1		960000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FYv1		902000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FYv1		845000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FYv1	758000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FYv1	730000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FYv1	672000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FYv1	615000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FYv1	557000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FYv1	500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FYv1	 71875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FYv1	 68750		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FYv1	 66875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FYv1      65000          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FYv1	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FYv1	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_FYv1	75000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_FYv1	71875		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_FYv1	70000		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_FYv1	67500		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_FYv1	65625		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_FYv1	63750		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_FYv1	61875		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_FYv1	60000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_FYv1	 93125		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_FYv1	 90625		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_FYv1	 87500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_FYv1	 82500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_FYv1	 80000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_FYv1	 78750		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_FYv1	 76875		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_FYv1	 75000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_FYv1	 73125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_FYv1	 71250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_FYv1	 68125		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_FYv1	 67500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_FYv1	 65625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_FYv1	 63750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_FYv1	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_FYv1	 60000		/* 10uV */

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

OPP_TBL(LL,   TB, 1, 1); /* opp_tbl_LL_e1_0   */
OPP_TBL(L,  TB, 1, 2); /* opp_tbl_L_e1_0  */
OPP_TBL(CCI, TB, 1, 3); /* opp_tbl_CCI_e1_0 */

OPP_TBL(LL,   6873T, 2, 1); /* opp_tbl_LL_e2_0   */
OPP_TBL(L,  6873T, 2, 2); /* opp_tbl_L_e2_0  */
OPP_TBL(CCI, 6873T, 2, 3); /* opp_tbl_CCI_e2_0 */

OPP_TBL(LL,   FYv1, 3, 1); /* opp_tbl_LL_e2_0   */
OPP_TBL(L,  FYv1, 3, 2); /* opp_tbl_L_e2_0  */
OPP_TBL(CCI, FYv1, 3, 3); /* opp_tbl_CCI_e2_0 */

/* v1.3 */
static struct opp_tbl_info opp_tbls[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
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

	},
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_FY[] = {	/* 6885 */
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
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_FY[] = {	/* 6885 */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_FY[] = {	/* 6885 */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_TB[] = {	/* 6885 */
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
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_TB[] = {	/* 6885 */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_TB[] = {	/* 6885 */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_6873T[] = {	/* 6885T */
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
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_6873T[] = {	/* 6885T */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_6873T[] = {	/* 6885T */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),

};

static struct mt_cpu_freq_method opp_tbl_method_LL_FYv1[] = {	/* 6885 */
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
	FP(2,	1),
	FP(2,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_FYv1[] = {	/* 6885 */
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
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
	FP(2,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_FYv1[] = {	/* 6885 */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_TB },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_6873T },
		[CPU_LEVEL_3] = { opp_tbl_method_LL_FYv1 },

	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_L_TB },
		[CPU_LEVEL_2] = { opp_tbl_method_L_6873T },
		[CPU_LEVEL_3] = { opp_tbl_method_L_FYv1 },

	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_TB },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_6873T },
		[CPU_LEVEL_3] = { opp_tbl_method_CCI_FYv1 },
	},
};
