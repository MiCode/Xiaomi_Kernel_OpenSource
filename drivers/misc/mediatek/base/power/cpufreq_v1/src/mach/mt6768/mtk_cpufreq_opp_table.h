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
#define CPU_DVFS_FREQ0_LL_6768		1700000		/* KHz */
#define CPU_DVFS_FREQ1_LL_6768		1625000		/* KHz */
#define CPU_DVFS_FREQ2_LL_6768		1500000		/* KHz */
#define CPU_DVFS_FREQ3_LL_6768		1450000		/* KHz */
#define CPU_DVFS_FREQ4_LL_6768		1375000		/* KHz */
#define CPU_DVFS_FREQ5_LL_6768		1325000		/* KHz */
#define CPU_DVFS_FREQ6_LL_6768		1275000		/* KHz */
#define CPU_DVFS_FREQ7_LL_6768		1175000		/* KHz */
#define CPU_DVFS_FREQ8_LL_6768		1100000		/* KHz */
#define CPU_DVFS_FREQ9_LL_6768		1050000		/* KHz */
#define CPU_DVFS_FREQ10_LL_6768		999000		/* KHz */
#define CPU_DVFS_FREQ11_LL_6768		950000		/* KHz */
#define CPU_DVFS_FREQ12_LL_6768		900000		/* KHz */
#define CPU_DVFS_FREQ13_LL_6768		850000		/* KHz */
#define CPU_DVFS_FREQ14_LL_6768		774000		/* KHz */
#define CPU_DVFS_FREQ15_LL_6768		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_6768		2000000		/* KHz */
#define CPU_DVFS_FREQ1_L_6768		1950000		/* KHz */
#define CPU_DVFS_FREQ2_L_6768		1900000		/* KHz */
#define CPU_DVFS_FREQ3_L_6768		1850000		/* KHz */
#define CPU_DVFS_FREQ4_L_6768		1800000		/* KHz */
#define CPU_DVFS_FREQ5_L_6768		1710000		/* KHz */
#define CPU_DVFS_FREQ6_L_6768		1621000		/* KHz */
#define CPU_DVFS_FREQ7_L_6768		1532000		/* KHz */
#define CPU_DVFS_FREQ8_L_6768		1443000		/* KHz */
#define CPU_DVFS_FREQ9_L_6768		1354000		/* KHz */
#define CPU_DVFS_FREQ10_L_6768		1295000		/* KHz */
#define CPU_DVFS_FREQ11_L_6768		1176000		/* KHz */
#define CPU_DVFS_FREQ12_L_6768		1087000		/* KHz */
#define CPU_DVFS_FREQ13_L_6768		998000		/* KHz */
#define CPU_DVFS_FREQ14_L_6768		909000		/* KHz */
#define CPU_DVFS_FREQ15_L_6768		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_6768		1187000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_6768		1120000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_6768		1049000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_6768		1014000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_6768		961000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_6768		909000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_6768		856000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_6768		821000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_6768		768000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_6768		733000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_6768	698000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_6768	663000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_6768	628000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_6768	593000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_6768	558000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_6768	500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_6768	 97500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_6768	 93750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_6768	 90000		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_6768	 88750		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_6768	 86250		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_6768	 85000		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_6768	 83125		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_6768	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_6768	 78125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_6768	 76250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_6768	 75000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_6768	 73125		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_6768	 71875		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_6768	 70000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_6768	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_6768	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_6768	111250		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_6768	109375		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_6768	106875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_6768	105000		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_6768	102500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_6768	 99375		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_6768	 96250		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_6768	 93125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_6768	 89375		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_6768	 86250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_6768	 84375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_6768	 80000		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_6768	 76250		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_6768	 73125		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_6768	 70000		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_6768	 67500		/* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_6768	 97500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_6768	 93750		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_6768	 90625		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_6768	 89375		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_6768	 86875		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_6768	 84375		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_6768	 81875		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_6768	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_6768	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_6768	 76250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_6768	 74375		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_6768	 72500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_6768	 71250		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_6768	 69375		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_6768	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_6768	 60000		/* 10uV */

/* 6767 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_6767		1625000		/* KHz */
#define CPU_DVFS_FREQ1_LL_6767		1500000		/* KHz */
#define CPU_DVFS_FREQ2_LL_6767		1425000		/* KHz */
#define CPU_DVFS_FREQ3_LL_6767		1375000		/* KHz */
#define CPU_DVFS_FREQ4_LL_6767		1325000		/* KHz */
#define CPU_DVFS_FREQ5_LL_6767		1275000		/* KHz */
#define CPU_DVFS_FREQ6_LL_6767		1175000		/* KHz */
#define CPU_DVFS_FREQ7_LL_6767		1125000		/* KHz */
#define CPU_DVFS_FREQ8_LL_6767		1075000		/* KHz */
#define CPU_DVFS_FREQ9_LL_6767		1025000		/* KHz */
#define CPU_DVFS_FREQ10_LL_6767		974000		/* KHz */
#define CPU_DVFS_FREQ11_LL_6767		925000		/* KHz */
#define CPU_DVFS_FREQ12_LL_6767		875000		/* KHz */
#define CPU_DVFS_FREQ13_LL_6767		825000		/* KHz */
#define CPU_DVFS_FREQ14_LL_6767		774000		/* KHz */
#define CPU_DVFS_FREQ15_LL_6767		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_6767		1800000		/* KHz */
#define CPU_DVFS_FREQ1_L_6767		1740000		/* KHz */
#define CPU_DVFS_FREQ2_L_6767		1681000		/* KHz */
#define CPU_DVFS_FREQ3_L_6767		1621000		/* KHz */
#define CPU_DVFS_FREQ4_L_6767		1532000		/* KHz */
#define CPU_DVFS_FREQ5_L_6767		1473000		/* KHz */
#define CPU_DVFS_FREQ6_L_6767		1414000		/* KHz */
#define CPU_DVFS_FREQ7_L_6767		1354000		/* KHz */
#define CPU_DVFS_FREQ8_L_6767		1295000		/* KHz */
#define CPU_DVFS_FREQ9_L_6767		1176000		/* KHz */
#define CPU_DVFS_FREQ10_L_6767		1117000		/* KHz */
#define CPU_DVFS_FREQ11_L_6767		1057000		/* KHz */
#define CPU_DVFS_FREQ12_L_6767		998000		/* KHz */
#define CPU_DVFS_FREQ13_L_6767		939000		/* KHz */
#define CPU_DVFS_FREQ14_L_6767		879000		/* KHz */
#define CPU_DVFS_FREQ15_L_6767		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_6767		1136000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_6767		1049000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_6767		997000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_6767		961000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_6767		926000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_6767		891000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_6767		856000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_6767		821000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_6767		768000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_6767		716000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_6767	680000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_6767	645000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_6767	610000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_6767	575000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_6767	558000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_6767	500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_6767	 93750		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_6767	 90000		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_6767	 88125		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_6767	 86250		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_6767	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_6767	 83125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_6767	 80000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_6767	 78750		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_6767	 76875		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_6767	 75625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_6767	 73750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_6767	 72500		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_6767	 70625		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_6767	 69375		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_6767	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_6767	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_6767	102500		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_6767	100625		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_6767	 98125		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_6767	 96250		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_6767	 93125		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_6767	 90625		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_6767	 88750		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_6767	 86250		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_6767	 84375		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_6767	 80000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_6767	 77500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_6767	 75625		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_6767	 73125		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_6767	 71250		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_6767	 68750		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_6767	 67500		/* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_6767	 93750		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_6767	 90625		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_6767	 88750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_6767	 86875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_6767	 85000		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_6767	 83750		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_6767	 81875		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_6767	 80000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_6767	 77500		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_6767	 75000		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_6767	 73750		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_6767	 71875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_6767	 70000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_6767	 68750		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_6767	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_6767	 60000		/* 10uV */

/* V5_2 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_LL_PRO		2000000		/* KHz */
#define CPU_DVFS_FREQ1_LL_PRO		1950000		/* KHz */
#define CPU_DVFS_FREQ2_LL_PRO		1900000		/* KHz */
#define CPU_DVFS_FREQ3_LL_PRO		1850000		/* KHz */
#define CPU_DVFS_FREQ4_LL_PRO		1800000		/* KHz */
#define CPU_DVFS_FREQ5_LL_PRO		1700000		/* KHz */
#define CPU_DVFS_FREQ6_LL_PRO		1625000		/* KHz */
#define CPU_DVFS_FREQ7_LL_PRO		1500000		/* KHz */
#define CPU_DVFS_FREQ8_LL_PRO		1375000		/* KHz */
#define CPU_DVFS_FREQ9_LL_PRO		1275000		/* KHz */
#define CPU_DVFS_FREQ10_LL_PRO		1175000		/* KHz */
#define CPU_DVFS_FREQ11_LL_PRO		1075000		/* KHz */
#define CPU_DVFS_FREQ12_LL_PRO		974000		/* KHz */
#define CPU_DVFS_FREQ13_LL_PRO		875000		/* KHz */
#define CPU_DVFS_FREQ14_LL_PRO		774000		/* KHz */
#define CPU_DVFS_FREQ15_LL_PRO		500000		/* KHz */

/* for DVFS OPP table B */
#define CPU_DVFS_FREQ0_L_PRO		2202000		/* KHz */
#define CPU_DVFS_FREQ1_L_PRO		2133000		/* KHz */
#define CPU_DVFS_FREQ2_L_PRO		2066000		/* KHz */
#define CPU_DVFS_FREQ3_L_PRO		2000000		/* KHz */
#define CPU_DVFS_FREQ4_L_PRO		1933000		/* KHz */
#define CPU_DVFS_FREQ5_L_PRO		1866000		/* KHz */
#define CPU_DVFS_FREQ6_L_PRO		1800000		/* KHz */
#define CPU_DVFS_FREQ7_L_PRO		1681000		/* KHz */
#define CPU_DVFS_FREQ8_L_PRO		1532000		/* KHz */
#define CPU_DVFS_FREQ9_L_PRO		1473000		/* KHz */
#define CPU_DVFS_FREQ10_L_PRO		1384000		/* KHz */
#define CPU_DVFS_FREQ11_L_PRO		1295000		/* KHz */
#define CPU_DVFS_FREQ12_L_PRO		1176000		/* KHz */
#define CPU_DVFS_FREQ13_L_PRO		1057000		/* KHz */
#define CPU_DVFS_FREQ14_L_PRO		939000		/* KHz */
#define CPU_DVFS_FREQ15_L_PRO		850000		/* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_PRO		1396000		/* KHz */
#define CPU_DVFS_FREQ1_CCI_PRO		1343000		/* KHz */
#define CPU_DVFS_FREQ2_CCI_PRO		1290000		/* KHz */
#define CPU_DVFS_FREQ3_CCI_PRO		1263000		/* KHz */
#define CPU_DVFS_FREQ4_CCI_PRO		1187000		/* KHz */
#define CPU_DVFS_FREQ5_CCI_PRO		1120000		/* KHz */
#define CPU_DVFS_FREQ6_CCI_PRO		1049000		/* KHz */
#define CPU_DVFS_FREQ7_CCI_PRO		997000		/* KHz */
#define CPU_DVFS_FREQ8_CCI_PRO		944000		/* KHz */
#define CPU_DVFS_FREQ9_CCI_PRO		856000		/* KHz */
#define CPU_DVFS_FREQ10_CCI_PRO		821000		/* KHz */
#define CPU_DVFS_FREQ11_CCI_PRO		751000		/* KHz */
#define CPU_DVFS_FREQ12_CCI_PRO		680000		/* KHz */
#define CPU_DVFS_FREQ13_CCI_PRO		610000		/* KHz */
#define CPU_DVFS_FREQ14_CCI_PRO		558000		/* KHz */
#define CPU_DVFS_FREQ15_CCI_PRO		500000		/* KHz */

/* for DVFS OPP table L */
#define CPU_DVFS_VOLT0_VPROC1_PRO	110000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC1_PRO	108125		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC1_PRO	106250		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC1_PRO	103750		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC1_PRO	101875		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC1_PRO	 97500		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC1_PRO	 93750		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC1_PRO	 90000		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC1_PRO	 86250		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC1_PRO	 83125		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC1_PRO	 80000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC1_PRO	 76875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC1_PRO	 73750		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC1_PRO	 70625		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC1_PRO	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC1_PRO	 60000		/* 10uV */

/* for DVFS OPP table B */
#define CPU_DVFS_VOLT0_VPROC2_PRO	 111875		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC2_PRO	 111875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC2_PRO	 111875		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC2_PRO	 111250		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC2_PRO	 108750		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC2_PRO	 105625		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC2_PRO	 102500		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC2_PRO	 98125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC2_PRO	 93125		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC2_PRO	 90625		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC2_PRO	 87500		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC2_PRO	 84375		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC2_PRO	 80000		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC2_PRO	 75625		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC2_PRO	 71250		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC2_PRO	 67500		/* 10uV */

/* for DVFS OPP table CCI */
#define CPU_DVFS_VOLT0_VPROC3_PRO	110000		/* 10uV */
#define CPU_DVFS_VOLT1_VPROC3_PRO	106875		/* 10uV */
#define CPU_DVFS_VOLT2_VPROC3_PRO	103750		/* 10uV */
#define CPU_DVFS_VOLT3_VPROC3_PRO	101875		/* 10uV */
#define CPU_DVFS_VOLT4_VPROC3_PRO	 97500		/* 10uV */
#define CPU_DVFS_VOLT5_VPROC3_PRO	 93125		/* 10uV */
#define CPU_DVFS_VOLT6_VPROC3_PRO	 90000		/* 10uV */
#define CPU_DVFS_VOLT7_VPROC3_PRO	 88125		/* 10uV */
#define CPU_DVFS_VOLT8_VPROC3_PRO	 85625		/* 10uV */
#define CPU_DVFS_VOLT9_VPROC3_PRO	 81250		/* 10uV */
#define CPU_DVFS_VOLT10_VPROC3_PRO	 80000		/* 10uV */
#define CPU_DVFS_VOLT11_VPROC3_PRO	 76875		/* 10uV */
#define CPU_DVFS_VOLT12_VPROC3_PRO	 73125		/* 10uV */
#define CPU_DVFS_VOLT13_VPROC3_PRO	 70000		/* 10uV */
#define CPU_DVFS_VOLT14_VPROC3_PRO	 67500		/* 10uV */
#define CPU_DVFS_VOLT15_VPROC3_PRO	 60000		/* 10uV */

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
/* 6768 */
OPP_TBL(LL, 6768, 0, 1);
OPP_TBL(L, 6768, 0, 2);
OPP_TBL(CCI, 6768, 0, 3);
/* 6767 */
OPP_TBL(LL, 6767, 1, 1);
OPP_TBL(L, 6767, 1, 2);
OPP_TBL(CCI, 6767, 1, 3);
/* PRO */
OPP_TBL(LL, PRO, 2, 1);
OPP_TBL(L, PRO, 2, 2);
OPP_TBL(CCI, PRO, 2, 3);

struct opp_tbl_info opp_tbls[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
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
/* < V6 */
static struct mt_cpu_freq_method opp_tbl_method_LL_6768[] = {
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_6768[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_6768[] = {
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
/* V6 */
static struct mt_cpu_freq_method opp_tbl_method_LL_6767[] = {
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
	FP(4,	1),
	FP(4,	1),
	FP(4,	1),
};

static struct mt_cpu_freq_method opp_tbl_method_L_6767[] = {	/* 6767 */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_6767[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_PRO[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_L_PRO[] = {	/* 6767 */
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

static struct mt_cpu_freq_method opp_tbl_method_CCI_PRO[] = {
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
		[CPU_LEVEL_0] = { opp_tbl_method_LL_6768 },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_6767 },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_PRO },
	},
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_6768 },
		[CPU_LEVEL_1] = { opp_tbl_method_L_6767 },
		[CPU_LEVEL_2] = { opp_tbl_method_L_PRO },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_6768 },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_6767 },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_PRO },
	},
};
