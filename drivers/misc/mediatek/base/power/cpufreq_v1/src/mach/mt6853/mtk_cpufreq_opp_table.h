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

/* FY */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_FY		2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_FY		1916000         /* KHz */
#define CPU_DVFS_FREQ2_LL_FY		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_FY		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_FY		1645000		/* KHz */
#define CPU_DVFS_FREQ5_LL_FY		1500000		/* KHz */
#define CPU_DVFS_FREQ6_LL_FY		1393000		/* KHz */
#define CPU_DVFS_FREQ7_LL_FY		1287000		/* KHz */
#define CPU_DVFS_FREQ8_LL_FY		1128000		/* KHz */
#define CPU_DVFS_FREQ9_LL_FY		1048000		/* KHz */
#define CPU_DVFS_FREQ10_LL_FY		968000		/* KHz */
#define CPU_DVFS_FREQ11_LL_FY		862000		/* KHz */
#define CPU_DVFS_FREQ12_LL_FY		756000		/* KHz */
#define CPU_DVFS_FREQ13_LL_FY		703000		/* KHz */
#define CPU_DVFS_FREQ14_LL_FY		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_FY		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_FY		2210000		/* KHz */
#define CPU_DVFS_FREQ1_L_FY		2093000		/* KHz */
#define CPU_DVFS_FREQ2_L_FY		2000000		/* KHz */
#define CPU_DVFS_FREQ3_L_FY		1906000		/* KHz */
#define CPU_DVFS_FREQ4_L_FY		1790000		/* KHz */
#define CPU_DVFS_FREQ5_L_FY		1720000		/* KHz */
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
#define CPU_DVFS_FREQ0_CCI_FY		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_FY		1356000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_FY		1254000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_FY		1152000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_FY		1108000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_FY		1050000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_FY		975000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_FY		900000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_FY		825000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_FY		768000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_FY		675000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_FY		600000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_FY		562000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_FY		525000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_FY		487000		/* KHz */
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
#define CPU_DVFS_VOLT13_VPROC1_FY  66250          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_FY	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_FY	 65000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_FY	100000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_FY	96875		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_FY	94375		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_FY	91875		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_FY	88750		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_FY	86875		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_FY	85000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_FY	82500		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_FY	80000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_FY	76875		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_FY	73750		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_FY	71875		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_FY	70625		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_FY	68750		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_FY	67500		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_FY	65000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_FY	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_FY	 98125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_FY	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_FY	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_FY	 87500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_FY	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_FY	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_FY	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_FY	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_FY	 75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_FY	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_FY	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_FY	 68750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_FY	 67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_FY	 66250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_FY	 65000		/* 10uV */

/* 6853 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_6853            2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_6853            1916000         /* KHz */
#define CPU_DVFS_FREQ2_LL_6853		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_6853		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_6853		1645000		/* KHz */
#define CPU_DVFS_FREQ5_LL_6853		1500000		/* KHz */
#define CPU_DVFS_FREQ6_LL_6853		1393000		/* KHz */
#define CPU_DVFS_FREQ7_LL_6853		1287000		/* KHz */
#define CPU_DVFS_FREQ8_LL_6853		1128000		/* KHz */
#define CPU_DVFS_FREQ9_LL_6853		1048000		/* KHz */
#define CPU_DVFS_FREQ10_LL_6853		968000		/* KHz */
#define CPU_DVFS_FREQ11_LL_6853		862000		/* KHz */
#define CPU_DVFS_FREQ12_LL_6853		756000		/* KHz */
#define CPU_DVFS_FREQ13_LL_6853		703000		/* KHz */
#define CPU_DVFS_FREQ14_LL_6853		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_6853		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_6853		2000000		/* KHz */
#define CPU_DVFS_FREQ1_L_6853		1953000		/* KHz */
#define CPU_DVFS_FREQ2_L_6853		1906000		/* KHz */
#define CPU_DVFS_FREQ3_L_6853		1836000		/* KHz */
#define CPU_DVFS_FREQ4_L_6853		1790000		/* KHz */
#define CPU_DVFS_FREQ5_L_6853		1720000		/* KHz */
#define CPU_DVFS_FREQ6_L_6853		1650000		/* KHz */
#define CPU_DVFS_FREQ7_L_6853		1534000		/* KHz */
#define CPU_DVFS_FREQ8_L_6853		1418000		/* KHz */
#define CPU_DVFS_FREQ9_L_6853		1274000		/* KHz */
#define CPU_DVFS_FREQ10_L_6853		1129000		/* KHz */
#define CPU_DVFS_FREQ11_L_6853		1042000		/* KHz */
#define CPU_DVFS_FREQ12_L_6853		985000		/* KHz */
#define CPU_DVFS_FREQ13_L_6853		898000		/* KHz */
#define CPU_DVFS_FREQ14_L_6853		840000		/* KHz */
#define CPU_DVFS_FREQ15_L_6853		725000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_6853		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_6853		1356000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_6853		1254000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_6853		1152000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_6853		1108000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_6853		1050000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_6853		975000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_6853		900000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_6853		825000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_6853		768000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_6853		675000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_6853		600000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_6853		562000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_6853		525000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_6853		487000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_6853		450000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_6853	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_6853	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_6853	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_6853	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_6853	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_6853	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_6853	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_6853	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_6853	 76250		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_6853	 74375		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_6853	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_6853	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_6853	 67500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_6853  66250          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_6853	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_6853	 65000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_6853	93750		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_6853	92500		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_6853	91250		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_6853	89375		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_6853	88750		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_6853	86875		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_6853	85000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_6853	82500		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_6853	80000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_6853	76875		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_6853	73750		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_6853	71875		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_6853	70625		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_6853	68750		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_6853	67500		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_6853	65000		/* 10uV	*/

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_6853	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_6853	 98125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_6853	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_6853	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_6853	 87500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_6853	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_6853	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_6853	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_6853	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_6853	 75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_6853	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_6853	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_6853	 68750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_6853	 67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_6853	 66250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_6853	 65000		/* 10uV */

/* B24G */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_B24G            2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_B24G            1916000         /* KHz */
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
#define CPU_DVFS_FREQ2_L_B24G		2210000		/* KHz */
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
#define CPU_DVFS_FREQ0_CCI_B24G		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_B24G		1356000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_B24G		1254000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_B24G		1152000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_B24G		1108000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_B24G		1050000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_B24G		975000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_B24G		900000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_B24G		825000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_B24G		768000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_B24G		675000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_B24G		600000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_B24G		562000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_B24G		525000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_B24G		487000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_B24G		450000		/* KHz */

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
#define CPU_DVFS_VOLT13_VPROC1_B24G  66250          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_B24G	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_B24G	 65000		/* 10uV */


/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_B24G	100000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_B24G	98125		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_B24G	96250		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_B24G	94375		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_B24G	91875		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_B24G	88750		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_B24G	85000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_B24G	82500		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_B24G	80000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_B24G	76875		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_B24G	73750		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_B24G	71875		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_B24G	70625		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_B24G	68750		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_B24G	67500		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_B24G	65000		/* 10uV	*/


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

/* B26G */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_B26G            2000000         /* KHz */
#define CPU_DVFS_FREQ1_LL_B26G            1916000         /* KHz */
#define CPU_DVFS_FREQ2_LL_B26G		1812000		/* KHz */
#define CPU_DVFS_FREQ3_LL_B26G		1750000		/* KHz */
#define CPU_DVFS_FREQ4_LL_B26G		1645000		/* KHz */
#define CPU_DVFS_FREQ5_LL_B26G		1500000		/* KHz */
#define CPU_DVFS_FREQ6_LL_B26G		1393000		/* KHz */
#define CPU_DVFS_FREQ7_LL_B26G		1287000		/* KHz */
#define CPU_DVFS_FREQ8_LL_B26G		1128000		/* KHz */
#define CPU_DVFS_FREQ9_LL_B26G		1048000		/* KHz */
#define CPU_DVFS_FREQ10_LL_B26G		968000		/* KHz */
#define CPU_DVFS_FREQ11_LL_B26G		862000		/* KHz */
#define CPU_DVFS_FREQ12_LL_B26G		756000		/* KHz */
#define CPU_DVFS_FREQ13_LL_B26G		703000		/* KHz */
#define CPU_DVFS_FREQ14_LL_B26G		650000		/* KHz */
#define CPU_DVFS_FREQ15_LL_B26G		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_B26G		2600000		/* KHz */
#define CPU_DVFS_FREQ1_L_B26G		2362000		/* KHz */
#define CPU_DVFS_FREQ2_L_B26G		2210000		/* KHz */
#define CPU_DVFS_FREQ3_L_B26G		2085000		/* KHz */
#define CPU_DVFS_FREQ4_L_B26G		1887000		/* KHz */
#define CPU_DVFS_FREQ5_L_B26G		1768000		/* KHz */
#define CPU_DVFS_FREQ6_L_B26G		1650000		/* KHz */
#define CPU_DVFS_FREQ7_L_B26G		1534000		/* KHz */
#define CPU_DVFS_FREQ8_L_B26G		1418000		/* KHz */
#define CPU_DVFS_FREQ9_L_B26G		1274000		/* KHz */
#define CPU_DVFS_FREQ10_L_B26G		1129000		/* KHz */
#define CPU_DVFS_FREQ11_L_B26G		1042000		/* KHz */
#define CPU_DVFS_FREQ12_L_B26G		985000		/* KHz */
#define CPU_DVFS_FREQ13_L_B26G		898000		/* KHz */
#define CPU_DVFS_FREQ14_L_B26G		840000		/* KHz */
#define CPU_DVFS_FREQ15_L_B26G		725000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_B26G		1400000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_B26G		1356000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_B26G		1254000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_B26G		1152000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_B26G		1108000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_B26G		1050000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_B26G		975000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_B26G		900000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_B26G		825000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_B26G		768000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_B26G		675000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_B26G		600000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_B26G		562000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_B26G		525000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_B26G		487000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_B26G		450000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_B26G	100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_B26G	 97500		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_B26G	 94375		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_B26G	 92500		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_B26G	 89375		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_B26G	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_B26G	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_B26G	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_B26G	 76250		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_B26G	 74375		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_B26G	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_B26G	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_B26G	 67500		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_B26G  66250          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_B26G	 65000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_B26G	 65000		/* 10uV */


/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_B26G	100000		/* 10uV	*/
#define CPU_DVFS_VOLT1_VPROC2_B26G	96250		/* 10uV	*/
#define CPU_DVFS_VOLT2_VPROC2_B26G	93750		/* 10uV	*/
#define CPU_DVFS_VOLT3_VPROC2_B26G	91875		/* 10uV	*/
#define CPU_DVFS_VOLT4_VPROC2_B26G	88750		/* 10uV	*/
#define CPU_DVFS_VOLT5_VPROC2_B26G	86875		/* 10uV	*/
#define CPU_DVFS_VOLT6_VPROC2_B26G	85000		/* 10uV	*/
#define CPU_DVFS_VOLT7_VPROC2_B26G	82500		/* 10uV	*/
#define CPU_DVFS_VOLT8_VPROC2_B26G	80000		/* 10uV	*/
#define CPU_DVFS_VOLT9_VPROC2_B26G	76875		/* 10uV	*/
#define CPU_DVFS_VOLT10_VPROC2_B26G	73750		/* 10uV	*/
#define CPU_DVFS_VOLT11_VPROC2_B26G	71875		/* 10uV	*/
#define CPU_DVFS_VOLT12_VPROC2_B26G	70625		/* 10uV	*/
#define CPU_DVFS_VOLT13_VPROC2_B26G	68750		/* 10uV	*/
#define CPU_DVFS_VOLT14_VPROC2_B26G	67500		/* 10uV	*/
#define CPU_DVFS_VOLT15_VPROC2_B26G	65000		/* 10uV	*/


/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_B26G	 100000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_B26G	 98125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_B26G	 93750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_B26G	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_B26G	 87500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_B26G	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_B26G	 82500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_B26G	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_B26G	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_B26G	 75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_B26G	 72500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_B26G	 70000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_B26G	 68750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_B26G	 67500		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_B26G	 66250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_B26G	 65000		/* 10uV */

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

OPP_TBL(LL,   6853, 1, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  6853, 1, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, 6853, 1, 3); /* opp_tbl_CCI_e0_0 */

OPP_TBL(LL,   B24G, 2, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  B24G, 2, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, B24G, 2, 3); /* opp_tbl_CCI_e0_0 */

OPP_TBL(LL,   B26G, 3, 1); /* opp_tbl_LL_e0_0   */
OPP_TBL(L,  B26G, 3, 2); /* opp_tbl_L_e0_0  */
OPP_TBL(CCI, B26G, 3, 3); /* opp_tbl_CCI_e0_0 */


/* v1.3 */
static struct opp_tbl_info opp_tbls[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_LL_e0_0,
			ARRAY_SIZE(opp_tbl_LL_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_LL_e1_0,
			ARRAY_SIZE(opp_tbl_LL_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_LL_e1_0,
			ARRAY_SIZE(opp_tbl_LL_e1_0) },
		[CPU_LEVEL_3] = { opp_tbl_LL_e2_0,
			ARRAY_SIZE(opp_tbl_LL_e2_0) },
		[CPU_LEVEL_4] = { opp_tbl_LL_e2_0,
			ARRAY_SIZE(opp_tbl_LL_e2_0) },
		[CPU_LEVEL_5] = { opp_tbl_LL_e3_0,
			ARRAY_SIZE(opp_tbl_LL_e3_0) },
		[CPU_LEVEL_6] = { opp_tbl_LL_e3_0,
			ARRAY_SIZE(opp_tbl_LL_e3_0) },

	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_L_e0_0,
			ARRAY_SIZE(opp_tbl_L_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_L_e1_0,
			ARRAY_SIZE(opp_tbl_L_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_L_e1_0,
			ARRAY_SIZE(opp_tbl_L_e1_0) },
		[CPU_LEVEL_3] = { opp_tbl_L_e2_0,
			ARRAY_SIZE(opp_tbl_L_e2_0) },
		[CPU_LEVEL_4] = { opp_tbl_L_e2_0,
			ARRAY_SIZE(opp_tbl_L_e2_0) },
		[CPU_LEVEL_5] = { opp_tbl_L_e3_0,
			ARRAY_SIZE(opp_tbl_L_e3_0) },
		[CPU_LEVEL_6] = { opp_tbl_L_e3_0,
			ARRAY_SIZE(opp_tbl_L_e3_0) },

	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_CCI_e0_0,
			ARRAY_SIZE(opp_tbl_CCI_e0_0) },
		[CPU_LEVEL_1] = { opp_tbl_CCI_e1_0,
			ARRAY_SIZE(opp_tbl_CCI_e1_0) },
		[CPU_LEVEL_2] = { opp_tbl_CCI_e1_0,
			ARRAY_SIZE(opp_tbl_CCI_e1_0) },
		[CPU_LEVEL_3] = { opp_tbl_CCI_e2_0,
			ARRAY_SIZE(opp_tbl_CCI_e2_0) },
		[CPU_LEVEL_4] = { opp_tbl_CCI_e2_0,
			ARRAY_SIZE(opp_tbl_CCI_e2_0) },
		[CPU_LEVEL_5] = { opp_tbl_CCI_e3_0,
			ARRAY_SIZE(opp_tbl_CCI_e3_0) },
		[CPU_LEVEL_6] = { opp_tbl_CCI_e3_0,
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

/* 16 steps OPP table */
static struct mt_cpu_freq_method opp_tbl_method_LL_6853[] = {	/* 6853 */
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

static struct mt_cpu_freq_method opp_tbl_method_L_6853[] = {	/* 6885 */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_6853[] = {	/* 6885 */
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
	FP(4,	1),
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
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_LL_B26G[] = {	/* B24G */
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


static struct mt_cpu_freq_method opp_tbl_method_L_B26G[] = {	/* B26G */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_B26G[] = {	/* B26G */
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
		[CPU_LEVEL_1] = { opp_tbl_method_LL_6853 },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_6853 },
		[CPU_LEVEL_3] = { opp_tbl_method_LL_B24G },
		[CPU_LEVEL_4] = { opp_tbl_method_LL_B24G },
		[CPU_LEVEL_5] = { opp_tbl_method_LL_B26G },
		[CPU_LEVEL_6] = { opp_tbl_method_LL_B26G },
	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_L_6853 },
		[CPU_LEVEL_2] = { opp_tbl_method_L_6853 },
		[CPU_LEVEL_3] = { opp_tbl_method_L_B24G },
		[CPU_LEVEL_4] = { opp_tbl_method_L_B24G },
		[CPU_LEVEL_5] = { opp_tbl_method_L_B26G },
		[CPU_LEVEL_6] = { opp_tbl_method_L_B26G },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_6853 },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_6853 },
		[CPU_LEVEL_3] = { opp_tbl_method_CCI_B24G },
		[CPU_LEVEL_4] = { opp_tbl_method_CCI_B24G },
		[CPU_LEVEL_5] = { opp_tbl_method_CCI_B26G },
		[CPU_LEVEL_6] = { opp_tbl_method_CCI_B26G },
	},
};
