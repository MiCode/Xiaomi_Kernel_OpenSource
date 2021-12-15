/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "mtk_cpufreq_struct.h"
#include "mtk_cpufreq_config.h"

/* FY */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY		2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_FY		1916000         /* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1645000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1500000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1390000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		1280000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		1115000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		1032000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		950000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		840000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		730000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		675000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		620000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY		2203000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY		2087000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY		1995000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY		1903000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY		1788000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY		1719000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY		1650000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY		1534000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY		1418000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY		1274000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY		1129000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY		1042000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY		985000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY		898000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY		840000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY		725000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY		1540000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY		1458000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY		1360000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY		1295000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY		1233000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY		1131000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY		1050000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY		975000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY		900000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY		825000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY		750000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY		675000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY		618000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY		562000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY		506000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY		450000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_FY	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY	 76250		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY	 74375		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY	 67500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY	 66250          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 65000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY	105000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_FY	101875		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_FY	99375		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_FY	96875		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_FY	93750		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_FY	91875		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_FY	90000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_FY	86875		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_FY	83750		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_FY	80000		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_FY	75625		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_FY	73750		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_FY	71875		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_FY	69375		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_FY	68125		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_FY	65000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_FY	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_FY	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_FY	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_FY	 90625		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_FY	 87500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_FY	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_FY	 77500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_FY	 75000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_FY	 72500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_FY	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_FY	 68750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_FY	 66875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_FY	 65000		/* 10uV */

/* B20G */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_B20G		2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_B20G		1916000         /* KHz */
#define CPU_DVFS_FREQ2_LL_B20G		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_B20G		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_B20G		1645000		/* KHz */
#define CPU_DVFS_FREQ5_LL_B20G		1500000		/* KHz */
#define CPU_DVFS_FREQ6_LL_B20G		1390000		/* KHz */
#define CPU_DVFS_FREQ7_LL_B20G		1280000		/* KHz */
#define CPU_DVFS_FREQ8_LL_B20G		1115000		/* KHz */
#define CPU_DVFS_FREQ9_LL_B20G		1032000		/* KHz */
#define CPU_DVFS_FREQ10_LL_B20G		950000		/* KHz */
#define CPU_DVFS_FREQ11_LL_B20G		840000		/* KHz */
#define CPU_DVFS_FREQ12_LL_B20G		730000		/* KHz */
#define CPU_DVFS_FREQ13_LL_B20G		675000		/* KHz */
#define CPU_DVFS_FREQ14_LL_B20G		620000		/* KHz */
#define CPU_DVFS_FREQ15_LL_B20G		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_B20G		2000000		/* KHz */
#define CPU_DVFS_FREQ1_L_B20G		1953000		/* KHz */
#define CPU_DVFS_FREQ2_L_B20G		1906000		/* KHz */
#define CPU_DVFS_FREQ3_L_B20G		1836000		/* KHz */
#define CPU_DVFS_FREQ4_L_B20G		1790000		/* KHz */
#define CPU_DVFS_FREQ5_L_B20G		1720000		/* KHz */
#define CPU_DVFS_FREQ6_L_B20G		1650000		/* KHz */
#define CPU_DVFS_FREQ7_L_B20G		1534000		/* KHz */
#define CPU_DVFS_FREQ8_L_B20G		1418000		/* KHz */
#define CPU_DVFS_FREQ9_L_B20G		1274000		/* KHz */
#define CPU_DVFS_FREQ10_L_B20G		1129000		/* KHz */
#define CPU_DVFS_FREQ11_L_B20G		1042000		/* KHz */
#define CPU_DVFS_FREQ12_L_B20G		985000		/* KHz */
#define CPU_DVFS_FREQ13_L_B20G		898000		/* KHz */
#define CPU_DVFS_FREQ14_L_B20G		840000		/* KHz */
#define CPU_DVFS_FREQ15_L_B20G		725000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_B20G		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_B20G		1341000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_B20G		1283000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_B20G		1225000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_B20G		1181000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_B20G		1108000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_B20G		1050000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_B20G		975000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_B20G		900000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_B20G		825000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_B20G	750000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_B20G	675000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_B20G	618000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_B20G	562000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_B20G	506000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_B20G	450000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_B20G	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_B20G	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_B20G	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_B20G	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_B20G	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_B20G	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_B20G	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_B20G	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_B20G	 76250		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_B20G	 74375		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_B20G	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_B20G	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_B20G	 67500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_B20G	 66250          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_B20G	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_B20G	 65000		/* 10uV */


/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_B20G	 99375		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_B20G	 98125	/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_B20G	 96875	/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_B20G	 95000	/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_B20G	 94375	/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_B20G	 92500	/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_B20G	 90625	/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_B20G	 87500	/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_B20G	 84375	/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_B20G	 80000	/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_B20G	 75625	/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_B20G	 73750	/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_B20G	 72500	/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_B20G	 70000	/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_B20G	 68125	/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_B20G	 65000	/* 10uV	*/


/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_B20G	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_B20G	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_B20G	 95000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_B20G	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_B20G	 90625		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_B20G	 87500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_B20G	 85000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_B20G	 82500		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_B20G	 80000		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_B20G	 77500		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_B20G	 75000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_B20G	 72500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_B20G	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_B20G	 68750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_B20G	 66875		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_B20G	 65000		/* 10uV */


/* B24G */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_B24G		2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_B24G		1916000         /* KHz */
#define CPU_DVFS_FREQ2_LL_B24G		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_B24G		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_B24G		1645000		/* KHz */
#define CPU_DVFS_FREQ5_LL_B24G		1500000		/* KHz */
#define CPU_DVFS_FREQ6_LL_B24G		1393000		/* KHz */
#define CPU_DVFS_FREQ7_LL_B24G		1287000		/* KHz */
#define CPU_DVFS_FREQ8_LL_B24G		1128000		/* KHz */
#define CPU_DVFS_FREQ9_LL_B24G		1048000		/* KHz */
#define CPU_DVFS_FREQ10_LL_B24G		968000		/* KHz */
#define CPU_DVFS_FREQ11_LL_B24G		862000		/* KHz */
#define CPU_DVFS_FREQ12_LL_B24G		756000		/* KHz */
#define CPU_DVFS_FREQ13_LL_B24G		703000		/* KHz */
#define CPU_DVFS_FREQ14_LL_B24G		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_B24G		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_B24G		2400000		/* KHz */
#define CPU_DVFS_FREQ1_L_B24G		2306000		/* KHz */
#define CPU_DVFS_FREQ2_L_B24G		2203000		/* KHz */
#define CPU_DVFS_FREQ3_L_B24G		2118000		/* KHz */
#define CPU_DVFS_FREQ4_L_B24G		1993000		/* KHz */
#define CPU_DVFS_FREQ5_L_B24G		1837000		/* KHz */
#define CPU_DVFS_FREQ6_L_B24G		1650000		/* KHz */
#define CPU_DVFS_FREQ7_L_B24G		1534000		/* KHz */
#define CPU_DVFS_FREQ8_L_B24G		1418000		/* KHz */
#define CPU_DVFS_FREQ9_L_B24G		1274000		/* KHz */
#define CPU_DVFS_FREQ10_L_B24G		1129000		/* KHz */
#define CPU_DVFS_FREQ11_L_B24G		1042000		/* KHz */
#define CPU_DVFS_FREQ12_L_B24G		985000		/* KHz */
#define CPU_DVFS_FREQ13_L_B24G		898000		/* KHz */
#define CPU_DVFS_FREQ14_L_B24G		840000		/* KHz */
#define CPU_DVFS_FREQ15_L_B24G		725000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_B24G		1600000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_B24G		1531000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_B24G		1370000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_B24G		1210000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_B24G		1141000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_B24G		1050000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_B24G		975000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_B24G		900000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_B24G		825000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_B24G		768000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_B24G	675000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_B24G	600000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_B24G	562000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_B24G	525000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_B24G	487000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_B24G	450000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_B24G	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_B24G	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_B24G	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_B24G	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_B24G	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_B24G	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_B24G	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_B24G	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_B24G	 76250		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_B24G	 74375		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_B24G	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_B24G	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_B24G	 67500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_B24G	 66250          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_B24G	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_B24G	 65000		/* 10uV */


/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_B24G	 105000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_B24G	 103125	/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_B24G	 101250	/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_B24G	 99375	/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_B24G	 96875	/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_B24G	 93750	/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_B24G	 90000	/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_B24G	 86875	/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_B24G	 83750	/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_B24G	 80000	/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_B24G	 75625	/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_B24G	 73750	/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_B24G	 71875	/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_B24G	 69375	/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_B24G	 68125	/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_B24G	 65000	/* 10uV	*/


/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_B24G	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_B24G	 98125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_B24G	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_B24G	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_B24G	 87500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_B24G	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_B24G	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_B24G	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_B24G	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_B24G	 75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_B24G	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_B24G	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_B24G	 68750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_B24G	 67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_B24G	 66250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_B24G	 65000		/* 10uV */

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

OPP_TBL(LL,   B20G, 1, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  B20G, 1, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, B20G, 1, 3); /* opp_tbl_CCI_e0_0 */

OPP_TBL(LL,   B24G, 2, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  B24G, 2, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, B24G, 2, 3); /* opp_tbl_CCI_e0_0 */
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

static struct mt_cpu_freq_method opp_tbl_method_L_FY[] = {	/* 6885 */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_B20G[] = {	/* B20G */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_B20G[] = {	/* B20G */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_B20G[] = {	/* B20G */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_B24G[] = {	/* B24G */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_B24G[] = {	/* B24G */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_B24G[] = {	/* B24G */
	/* POS,	CLK */
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
		[CPU_LEVEL_1] = { opp_tbl_method_LL_B20G },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_B24G },

	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_L_B20G },
		[CPU_LEVEL_2] = { opp_tbl_method_L_B24G },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_B20G },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_B24G },
	},
};
