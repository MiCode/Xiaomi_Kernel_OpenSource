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

/* 6885 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY            2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_FY            1875000         /* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1687000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1625000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1541000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		1464000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		1358000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		1181000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		1075000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		968000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		897000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		827000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		720000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		650000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY		2000000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY		1950000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY		1900000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY		1850000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY		1800000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY		1716000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY		1633000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY		1508000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY		1425000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY		1341000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY		1258000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY		1175000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY		1091000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY		1008000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY		925000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY		774000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY		1341000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY		1306000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY		1260000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY		1190000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY		1132000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY		1075000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY		1017000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY		960000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY		902000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY		845000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY		758000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY		730000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY		672000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY		615000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY		557000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY		500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_FY	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY	 96250		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY	 91875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY	 71875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY	 68750		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY	 66875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY        65000          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY	91875		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_FY	90000		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_FY	88125		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_FY	85625		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_FY	83750		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_FY	81875		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_FY	80000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_FY	76875		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_FY	75000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_FY	73125		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_FY	71250		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_FY	69375		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_FY	67500		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_FY	65625		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_FY	63750		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_FY	60000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_FY	 93125		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_FY	 90625		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_FY	 87500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_FY	 80625		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_FY	 78750		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_FY	 76875		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_FY	 73125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_FY	 71250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_FY	 68125		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_FY	 67500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_FY	 65625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_FY	 63750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_FY	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_FY	 60000		/* 10uV */

/* 6885_TB */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_TB            2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_TB            1875000         /* KHz */
#define CPU_DVFS_FREQ2_LL_TB		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_TB		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_TB		1687000		/* KHz */
#define CPU_DVFS_FREQ5_LL_TB		1625000		/* KHz */
#define CPU_DVFS_FREQ6_LL_TB		1541000		/* KHz */
#define CPU_DVFS_FREQ7_LL_TB		1464000		/* KHz */
#define CPU_DVFS_FREQ8_LL_TB		1358000		/* KHz */
#define CPU_DVFS_FREQ9_LL_TB		1181000		/* KHz */
#define CPU_DVFS_FREQ10_LL_TB		1075000		/* KHz */
#define CPU_DVFS_FREQ11_LL_TB		968000		/* KHz */
#define CPU_DVFS_FREQ12_LL_TB		897000		/* KHz */
#define CPU_DVFS_FREQ13_LL_TB		827000		/* KHz */
#define CPU_DVFS_FREQ14_LL_TB		720000		/* KHz */
#define CPU_DVFS_FREQ15_LL_TB		650000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_TB             2300000         /* KHz */
#define CPU_DVFS_FREQ1_L_TB             2195000         /* KHz */
#define CPU_DVFS_FREQ2_L_TB             2091000         /* KHz */
#define CPU_DVFS_FREQ3_L_TB             2008000         /* KHz */
#define CPU_DVFS_FREQ4_L_TB             1883000         /* KHz */
#define CPU_DVFS_FREQ5_L_TB             1800000         /* KHz */
#define CPU_DVFS_FREQ6_L_TB             1633000         /* KHz */
#define CPU_DVFS_FREQ7_L_TB             1508000         /* KHz */
#define CPU_DVFS_FREQ8_L_TB             1383000         /* KHz */
#define CPU_DVFS_FREQ9_L_TB             1300000         /* KHz */
#define CPU_DVFS_FREQ10_L_TB            1175000         /* KHz */
#define CPU_DVFS_FREQ11_L_TB            1091000         /* KHz */
#define CPU_DVFS_FREQ12_L_TB            1008000         /* KHz */
#define CPU_DVFS_FREQ13_L_TB            925000          /* KHz */
#define CPU_DVFS_FREQ14_L_TB            841000          /* KHz */
#define CPU_DVFS_FREQ15_L_TB            774000          /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_TB		1470000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_TB		1411000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_TB		1341000		/* KHz */
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
#define CPU_DVFS_VOLT0_VPROC1_TB	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_TB	 96250		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_TB	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_TB	 91875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_TB	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_TB	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_TB	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_TB	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_TB	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_TB	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_TB	 71875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_TB	 68750		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_TB	 66875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_TB	 65000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_TB	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_TB	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_TB        100000          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_TB        96250           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_TB        91875           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_TB        88750           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_TB        84375           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_TB        81250           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_TB        77500           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_TB        75000           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_TB        72500           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_TB        70625           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_TB       68125           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_TB       66875           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_TB       65000           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_TB       63125           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_TB       61250           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_TB       60000           /* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_TB	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_TB	 96250		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_TB	 91875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_TB	 86875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_TB	 82500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_TB	 79375		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_TB	 76875		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_TB	 75000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_TB	 73125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_TB	 71875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_TB	 70000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_TB	 67500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_TB	 65625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_TB	 63750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_TB	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_TB	 60000		/* 10uV */

/* 6885T */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_6873T         2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_6873T         1875000         /* KHz */
#define CPU_DVFS_FREQ2_LL_6873T		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_6873T		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_6873T		1687000		/* KHz */
#define CPU_DVFS_FREQ5_LL_6873T		1625000		/* KHz */
#define CPU_DVFS_FREQ6_LL_6873T		1541000		/* KHz */
#define CPU_DVFS_FREQ7_LL_6873T		1464000		/* KHz */
#define CPU_DVFS_FREQ8_LL_6873T		1358000		/* KHz */
#define CPU_DVFS_FREQ9_LL_6873T		1181000		/* KHz */
#define CPU_DVFS_FREQ10_LL_6873T	1075000		/* KHz */
#define CPU_DVFS_FREQ11_LL_6873T	968000		/* KHz */
#define CPU_DVFS_FREQ12_LL_6873T	897000		/* KHz */
#define CPU_DVFS_FREQ13_LL_6873T	827000		/* KHz */
#define CPU_DVFS_FREQ14_LL_6873T	720000		/* KHz */
#define CPU_DVFS_FREQ15_LL_6873T	650000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_6873T		2200000		/* KHz */
#define CPU_DVFS_FREQ1_L_6873T		2100000		/* KHz */
#define CPU_DVFS_FREQ2_L_6873T		2050000		/* KHz */
#define CPU_DVFS_FREQ3_L_6873T		1983000		/* KHz */
#define CPU_DVFS_FREQ4_L_6873T		1916000		/* KHz */
#define CPU_DVFS_FREQ5_L_6873T		1800000		/* KHz */
#define CPU_DVFS_FREQ6_L_6873T		1633000		/* KHz */
#define CPU_DVFS_FREQ7_L_6873T		1508000		/* KHz */
#define CPU_DVFS_FREQ8_L_6873T		1383000		/* KHz */
#define CPU_DVFS_FREQ9_L_6873T		1258000		/* KHz */
#define CPU_DVFS_FREQ10_L_6873T		1175000		/* KHz */
#define CPU_DVFS_FREQ11_L_6873T		1091000		/* KHz */
#define CPU_DVFS_FREQ12_L_6873T		1008000		/* KHz */
#define CPU_DVFS_FREQ13_L_6873T		925000		/* KHz */
#define CPU_DVFS_FREQ14_L_6873T		841000		/* KHz */
#define CPU_DVFS_FREQ15_L_6873T		774000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_6873T	1470000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_6873T	1411000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_6873T	1341000		/* KHz */
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
#define CPU_DVFS_VOLT0_VPROC1_6873T	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_6873T	 96250		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_6873T	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_6873T	 91875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_6873T	 90000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_6873T	 88125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_6873T	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_6873T	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_6873T	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_6873T	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_6873T	 71875		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_6873T	 68750		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_6873T	 66875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_6873T	 65000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_6873T	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_6873T	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_6873T	100000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_6873T	96250		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_6873T	94375		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_6873T	91875		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_6873T	89375		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_6873T	85000		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_6873T	80625		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_6873T	78125		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_6873T	75000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_6873T	71875		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_6873T	70000		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_6873T	67500		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_6873T	65625		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_6873T	63750		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_6873T	61875		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_6873T	60000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_6873T	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_6873T	 96250		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_6873T	 91875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_6873T	 86875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_6873T	 82500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_6873T	 79375		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_6873T	 76875		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_6873T	 75000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_6873T	 73125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_6873T	 71875		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_6873T	 70000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_6873T	 67500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_6873T	 65625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_6873T	 63750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_6873T	 61875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_6873T	 60000		/* 10uV */

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

	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_L_e0_0,
			ARRAY_SIZE(opp_tbl_L_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_L_e1_0,
			ARRAY_SIZE(opp_tbl_L_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_L_e2_0,
			ARRAY_SIZE(opp_tbl_L_e2_0) },


	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_CCI_e0_0,
			ARRAY_SIZE(opp_tbl_CCI_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_CCI_e1_0,
			ARRAY_SIZE(opp_tbl_CCI_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_CCI_e2_0,
			ARRAY_SIZE(opp_tbl_CCI_e2_0) },

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

struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_TB },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_6873T },

	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_L_TB },
		[CPU_LEVEL_2] = { opp_tbl_method_L_6873T },

	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_TB },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_6873T },
	},
};
