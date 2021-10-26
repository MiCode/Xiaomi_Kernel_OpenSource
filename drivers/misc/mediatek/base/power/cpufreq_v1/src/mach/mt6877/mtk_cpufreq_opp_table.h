/*
 * Copyright (C) 2020 MediaTek Inc.
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

/* FY */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY		2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_FY		1903000         /* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1800000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1703000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1600000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1503000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1407000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		1310000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		1260000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		1150000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		1053000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		980000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		900000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		740000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY		2600000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY		2400000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY		2275000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY		2150000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY		2000000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY		1900000		/* KHz */
#define CPU_DVFS_FREQ6_L_FY		1800000		/* KHz */
#define CPU_DVFS_FREQ7_L_FY		1660000		/* KHz */
#define CPU_DVFS_FREQ8_L_FY		1540000		/* KHz */
#define CPU_DVFS_FREQ9_L_FY		1430000		/* KHz */
#define CPU_DVFS_FREQ10_L_FY		1300000		/* KHz */
#define CPU_DVFS_FREQ11_L_FY		1140000		/* KHz */
#define CPU_DVFS_FREQ12_L_FY		1040000		/* KHz */
#define CPU_DVFS_FREQ13_L_FY		910000		/* KHz */
#define CPU_DVFS_FREQ14_L_FY		740000		/* KHz */
#define CPU_DVFS_FREQ15_L_FY		650000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY		1700000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY		1621000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY		1542000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY		1440000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY		1350000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY		1271000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY		1120000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY		1041000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY		962000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY		900000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY	840000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY	740000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY	661000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY	600000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY	560000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_FY	520000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_FY	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_FY	 96875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_FY	 92500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_FY	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_FY	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_FY	 83125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_FY	 81250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_FY	 79375		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_FY	 78125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_FY	 75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_FY	 73750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_FY	 72500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_FY	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_FY	 67500          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 65000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY	105000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_FY	100000		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_FY	95000		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_FY	90000		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_FY	88750		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_FY	87500		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_FY	86875		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_FY	85000		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_FY	83125		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_FY	80625		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_FY	78125		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_FY	75000		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_FY	73125		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_FY	70625		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_FY	66875		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_FY	65000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_FY	 100000			/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_FY	 96875			/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_FY	 93750			/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_FY	 89375			/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_FY	 85000			/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_FY	 83125			/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_FY	 80000			/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_FY	 78125			/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_FY	 76250			/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_FY	 74375			/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_FY	 73125			/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_FY	 70625			/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_FY	 68750			/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_FY	 67500			/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_FY	 66250			/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_FY	 65000			/* 10uV */

/* B25G */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_B25G		2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_B25G		1903000         /* KHz */
#define CPU_DVFS_FREQ2_LL_B25G		1800000		/* KHz */
#define CPU_DVFS_FREQ3_LL_B25G		1703000		/* KHz */
#define CPU_DVFS_FREQ4_LL_B25G		1600000		/* KHz */
#define CPU_DVFS_FREQ5_LL_B25G		1503000		/* KHz */
#define CPU_DVFS_FREQ6_LL_B25G		1407000		/* KHz */
#define CPU_DVFS_FREQ7_LL_B25G		1310000		/* KHz */
#define CPU_DVFS_FREQ8_LL_B25G		1260000		/* KHz */
#define CPU_DVFS_FREQ9_LL_B25G		1150000		/* KHz */
#define CPU_DVFS_FREQ10_LL_B25G		1053000		/* KHz */
#define CPU_DVFS_FREQ11_LL_B25G		980000		/* KHz */
#define CPU_DVFS_FREQ12_LL_B25G		900000		/* KHz */
#define CPU_DVFS_FREQ13_LL_B25G		740000		/* KHz */
#define CPU_DVFS_FREQ14_LL_B25G		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_B25G		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_B25G		2500000		/* KHz */
#define CPU_DVFS_FREQ1_L_B25G		2400000		/* KHz */
#define CPU_DVFS_FREQ2_L_B25G		2275000		/* KHz */
#define CPU_DVFS_FREQ3_L_B25G		2150000		/* KHz */
#define CPU_DVFS_FREQ4_L_B25G		2000000		/* KHz */
#define CPU_DVFS_FREQ5_L_B25G		1900000		/* KHz */
#define CPU_DVFS_FREQ6_L_B25G		1800000		/* KHz */
#define CPU_DVFS_FREQ7_L_B25G		1660000		/* KHz */
#define CPU_DVFS_FREQ8_L_B25G		1540000		/* KHz */
#define CPU_DVFS_FREQ9_L_B25G		1430000		/* KHz */
#define CPU_DVFS_FREQ10_L_B25G		1300000		/* KHz */
#define CPU_DVFS_FREQ11_L_B25G		1140000		/* KHz */
#define CPU_DVFS_FREQ12_L_B25G		1040000		/* KHz */
#define CPU_DVFS_FREQ13_L_B25G		910000		/* KHz */
#define CPU_DVFS_FREQ14_L_B25G		740000		/* KHz */
#define CPU_DVFS_FREQ15_L_B25G		650000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_B25G		1700000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_B25G		1621000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_B25G		1542000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_B25G		1440000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_B25G		1350000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_B25G		1271000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_B25G		1120000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_B25G		1041000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_B25G		962000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_B25G		900000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_B25G	840000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_B25G	740000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_B25G	661000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_B25G	600000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_B25G	560000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_B25G	520000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_B25G	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_B25G	 96875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_B25G	 92500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_B25G	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_B25G	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_B25G	 83125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_B25G	 81250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_B25G	 79375		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_B25G	 78125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_B25G	 75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_B25G	 73750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_B25G	 72500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_B25G	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_B25G	 67500          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_B25G	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_B25G	 65000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_B25G	105000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_B25G	100000		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_B25G	95000		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_B25G	90000		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_B25G	88750		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_B25G	87500		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_B25G	86875		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_B25G	85000		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_B25G	83125		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_B25G	80625		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_B25G	78125		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_B25G	75000		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_B25G	73125		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_B25G	70625		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_B25G	66875		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_B25G	65000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_B25G	 100000			/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_B25G	 96875			/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_B25G	 93750			/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_B25G	 89375			/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_B25G	 85000			/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_B25G	 83125			/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_B25G	 80000			/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_B25G	 78125			/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_B25G	 76250			/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_B25G	 74375			/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_B25G	 73125			/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_B25G	 70625			/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_B25G	 68750			/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_B25G	 67500			/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_B25G	 66250			/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_B25G	 65000			/* 10uV */

/* B24G */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_B24G		2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_B24G		1903000         /* KHz */
#define CPU_DVFS_FREQ2_LL_B24G		1800000		/* KHz */
#define CPU_DVFS_FREQ3_LL_B24G		1703000		/* KHz */
#define CPU_DVFS_FREQ4_LL_B24G		1600000		/* KHz */
#define CPU_DVFS_FREQ5_LL_B24G		1503000		/* KHz */
#define CPU_DVFS_FREQ6_LL_B24G		1407000		/* KHz */
#define CPU_DVFS_FREQ7_LL_B24G		1310000		/* KHz */
#define CPU_DVFS_FREQ8_LL_B24G		1260000		/* KHz */
#define CPU_DVFS_FREQ9_LL_B24G		1150000		/* KHz */
#define CPU_DVFS_FREQ10_LL_B24G		1053000		/* KHz */
#define CPU_DVFS_FREQ11_LL_B24G		980000		/* KHz */
#define CPU_DVFS_FREQ12_LL_B24G		900000		/* KHz */
#define CPU_DVFS_FREQ13_LL_B24G		740000		/* KHz */
#define CPU_DVFS_FREQ14_LL_B24G		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_B24G		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_B24G		2400000		/* KHz */
#define CPU_DVFS_FREQ1_L_B24G		2320000		/* KHz */
#define CPU_DVFS_FREQ2_L_B24G		2240000		/* KHz */
#define CPU_DVFS_FREQ3_L_B24G		2150000		/* KHz */
#define CPU_DVFS_FREQ4_L_B24G		2000000		/* KHz */
#define CPU_DVFS_FREQ5_L_B24G		1900000		/* KHz */
#define CPU_DVFS_FREQ6_L_B24G		1800000		/* KHz */
#define CPU_DVFS_FREQ7_L_B24G		1660000		/* KHz */
#define CPU_DVFS_FREQ8_L_B24G		1540000		/* KHz */
#define CPU_DVFS_FREQ9_L_B24G		1430000		/* KHz */
#define CPU_DVFS_FREQ10_L_B24G	1300000		/* KHz */
#define CPU_DVFS_FREQ11_L_B24G	1140000		/* KHz */
#define CPU_DVFS_FREQ12_L_B24G	1040000		/* KHz */
#define CPU_DVFS_FREQ13_L_B24G	910000		/* KHz */
#define CPU_DVFS_FREQ14_L_B24G	740000		/* KHz */
#define CPU_DVFS_FREQ15_L_B24G	650000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_B24G		1700000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_B24G		1621000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_B24G		1542000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_B24G		1440000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_B24G		1350000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_B24G		1271000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_B24G		1120000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_B24G		1041000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_B24G		962000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_B24G		900000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_B24G	840000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_B24G	740000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_B24G	661000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_B24G	600000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_B24G	560000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_B24G	520000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_B24G	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_B24G	96875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_B24G	92500		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_B24G	89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_B24G	85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_B24G	83125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_B24G	81250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_B24G	79375		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_B24G	78125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_B24G	75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_B24G	73750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_B24G	72500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_B24G	70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_B24G	67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_B24G	65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_B24G	65000		/* 10uV */


/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_B24G	 105000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_B24G	 100625	/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_B24G	 95625	/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_B24G	 90000	/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_B24G	 88750	/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_B24G	 87500	/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_B24G	 86875	/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_B24G	 85000	/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_B24G	 83125	/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_B24G	 80625	/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_B24G	 78125	/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_B24G	 75000	/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_B24G	 73125	/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_B24G	 70625	/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_B24G	 66875	/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_B24G	 65000	/* 10uV	*/


/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_B24G	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_B24G	 96875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_B24G	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_B24G	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_B24G	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_B24G	 83125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_B24G	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_B24G	 78125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_B24G	 76250		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_B24G	 74375		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_B24G	 73125		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_B24G	 70625		/* 10uV */
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

OPP_TBL(LL,   B24G, 1, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  B24G, 1, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, B24G, 1, 3); /* opp_tbl_CCI_e0_0 */

OPP_TBL(LL,   B25G, 2, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  B25G, 2, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, B25G, 2, 3); /* opp_tbl_CCI_e0_0 */


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
	FP(1,	1),
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_B25G[] = {	/* 6885 */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_B25G[] = {	/* 6885 */
	/* POS,	CLK */
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
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_B25G[] = {	/* 6885 */
	/* POS,	CLK */
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
};

static struct mt_cpu_freq_method opp_tbl_method_L_B24G[] = {	/* B24G */
	/* POS,	CLK */
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
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_B24G[] = {	/* B24G */
	/* POS,	CLK */
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_B24G },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_B25G },
	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_L_B24G },
		[CPU_LEVEL_2] = { opp_tbl_method_L_B25G },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_B24G },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_B25G },
	},
};
