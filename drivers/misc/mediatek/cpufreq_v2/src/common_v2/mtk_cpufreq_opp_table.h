/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include "mtk_cpufreq_struct.h"
#include "mtk_cpufreq_config.h"

/* FY */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_FY    2351000    /* KHz */
#define CPU_DVFS_FREQ1_L_FY    2269000    /* KHz */
#define CPU_DVFS_FREQ2_L_FY    2186000    /* KHz */
#define CPU_DVFS_FREQ3_L_FY    2103000    /* KHz */
#define CPU_DVFS_FREQ4_L_FY    2033000    /* KHz */
#define CPU_DVFS_FREQ5_L_FY    1962000    /* KHz */
#define CPU_DVFS_FREQ6_L_FY    1891000    /* KHz */
#define CPU_DVFS_FREQ7_L_FY    1820000    /* KHz */
#define CPU_DVFS_FREQ8_L_FY    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_FY    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_FY   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_FY   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_FY   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_FY   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_FY    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_FY    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_FY    1701000    /* KHz */
#define CPU_DVFS_FREQ1_LL_FY    1612000    /* KHz */
#define CPU_DVFS_FREQ2_LL_FY    1522000    /* KHz */
#define CPU_DVFS_FREQ3_LL_FY    1433000    /* KHz */
#define CPU_DVFS_FREQ4_LL_FY    1356000    /* KHz */
#define CPU_DVFS_FREQ5_LL_FY    1279000    /* KHz */
#define CPU_DVFS_FREQ6_LL_FY    1203000    /* KHz */
#define CPU_DVFS_FREQ7_LL_FY    1126000    /* KHz */
#define CPU_DVFS_FREQ8_LL_FY    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_FY     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_FY    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_FY    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_FY    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_FY    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_FY    460000    /* KHz */
#define CPU_DVFS_FREQ15_LL_FY    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_FY    1051000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_FY    1006000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_FY     962000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_FY     917000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_FY     878000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_FY     840000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_FY     801000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_FY     763000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_FY     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_FY     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_FY    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_FY    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_FY    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_FY    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_FY    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_FY    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_FY    111875        /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_FY    107500        /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_FY    103125        /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_FY     98750        /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_FY     95000        /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_FY     91250        /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_FY     87500        /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_FY     83750        /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_FY     80000        /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_FY     76875        /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_FY    73750        /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_FY    70625        /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_FY    67500        /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_FY    64375        /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_FY    61875        /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_FY    60000        /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_FY    111875        /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_FY    107500        /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_FY    103125        /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_FY     98750        /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_FY     95000        /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_FY     91250        /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_FY     87500        /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_FY     83750        /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_FY     80000        /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_FY     76875        /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_FY    73750        /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_FY    70625        /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_FY    67500        /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_FY    64375        /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_FY    61875        /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_FY    60000        /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_FY    111875         /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_FY    107500         /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_FY    103125         /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_FY     98750         /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_FY     95000         /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_FY     91250         /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_FY     87500         /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_FY     83750         /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_FY     80000         /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_FY     76875         /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_FY    73750         /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_FY    70625         /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_FY    67500         /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_FY    64375         /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_FY    61875         /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_FY    60000         /* 10uV */

/* SB */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_SB    2501000    /* KHz */
#define CPU_DVFS_FREQ1_L_SB    2397000    /* KHz */
#define CPU_DVFS_FREQ2_L_SB    2294000    /* KHz */
#define CPU_DVFS_FREQ3_L_SB    2191000    /* KHz */
#define CPU_DVFS_FREQ4_L_SB    2103000    /* KHz */
#define CPU_DVFS_FREQ5_L_SB    2015000    /* KHz */
#define CPU_DVFS_FREQ6_L_SB    1926000    /* KHz */
#define CPU_DVFS_FREQ7_L_SB    1838000    /* KHz */
#define CPU_DVFS_FREQ8_L_SB    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_SB    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_SB   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_SB   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_SB   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_SB   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_SB    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_SB    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_SB    1901000    /* KHz */
#define CPU_DVFS_FREQ1_LL_SB    1784000    /* KHz */
#define CPU_DVFS_FREQ2_LL_SB    1667000    /* KHz */
#define CPU_DVFS_FREQ3_LL_SB    1550000    /* KHz */
#define CPU_DVFS_FREQ4_LL_SB    1450000    /* KHz */
#define CPU_DVFS_FREQ5_LL_SB    1350000    /* KHz */
#define CPU_DVFS_FREQ6_LL_SB    1250000    /* KHz */
#define CPU_DVFS_FREQ7_LL_SB    1150000    /* KHz */
#define CPU_DVFS_FREQ8_LL_SB    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_SB     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_SB    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_SB    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_SB    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_SB    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_SB    460000    /* KHz */
#define CPU_DVFS_FREQ15_LL_SB    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_SB    1101000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_SB    1049000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_SB     998000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_SB     946000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_SB     902000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_SB     857000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_SB     813000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_SB     769000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_SB     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_SB     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_SB    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_SB    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_SB    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_SB    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_SB    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_SB    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_SB    111875         /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_SB    107500         /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_SB    103125         /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_SB     98750         /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_SB     95000         /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_SB     91250         /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_SB     87500         /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_SB     83750         /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_SB     80000         /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_SB     76875         /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_SB    73750         /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_SB    70625         /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_SB    67500         /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_SB    64375         /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_SB    61875         /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_SB    60000         /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_SB    111875         /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_SB    107500         /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_SB    103125         /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_SB     98750         /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_SB     95000         /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_SB     91250         /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_SB     87500         /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_SB     83750         /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_SB     80000         /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_SB     76875         /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_SB    73750         /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_SB    70625         /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_SB    67500         /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_SB    64375         /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_SB    61875         /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_SB    60000         /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_SB    111875         /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_SB    107500         /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_SB    103125         /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_SB     98750         /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_SB     95000         /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_SB     91250         /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_SB     87500         /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_SB     83750         /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_SB     80000         /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_SB     76875         /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_SB    73750         /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_SB    70625         /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_SB    67500         /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_SB    64375         /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_SB    61875         /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_SB    60000         /* 10uV */

/* C65T */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_C65T    2501000    /* KHz */
#define CPU_DVFS_FREQ1_L_C65T    2383000    /* KHz */
#define CPU_DVFS_FREQ2_L_C65T    2280000    /* KHz */
#define CPU_DVFS_FREQ3_L_C65T    2191000    /* KHz */
#define CPU_DVFS_FREQ4_L_C65T    2103000    /* KHz */
#define CPU_DVFS_FREQ5_L_C65T    2015000    /* KHz */
#define CPU_DVFS_FREQ6_L_C65T    1926000    /* KHz */
#define CPU_DVFS_FREQ7_L_C65T    1838000    /* KHz */
#define CPU_DVFS_FREQ8_L_C65T    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_C65T    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_C65T   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_C65T   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_C65T   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_C65T   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_C65T    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_C65T    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_C65T    1800000    /* KHz */
#define CPU_DVFS_FREQ1_LL_C65T    1682000    /* KHz */
#define CPU_DVFS_FREQ2_LL_C65T    1579000    /* KHz */
#define CPU_DVFS_FREQ3_LL_C65T    1491000    /* KHz */
#define CPU_DVFS_FREQ4_LL_C65T    1402000    /* KHz */
#define CPU_DVFS_FREQ5_LL_C65T    1314000    /* KHz */
#define CPU_DVFS_FREQ6_LL_C65T    1226000    /* KHz */
#define CPU_DVFS_FREQ7_LL_C65T    1138000    /* KHz */
#define CPU_DVFS_FREQ8_LL_C65T    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_C65T     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_C65T    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_C65T    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_C65T    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_C65T    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_C65T    501000    /* KHz */
#define CPU_DVFS_FREQ15_LL_C65T    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_C65T    1101000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_C65T    1042000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_C65T     990000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_C65T     946000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_C65T     902000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_C65T     857000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_C65T     813000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_C65T     769000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_C65T     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_C65T     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_C65T    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_C65T    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_C65T    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_C65T    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_C65T    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_C65T    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_C65T    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_C65T    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_C65T    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_C65T     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_C65T     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_C65T     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_C65T     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_C65T     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_C65T     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_C65T     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_C65T    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_C65T    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_C65T    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_C65T    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_C65T    61875          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_C65T    60000          /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_C65T    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_C65T    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_C65T    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_C65T     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_C65T     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_C65T     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_C65T     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_C65T     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_C65T     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_C65T     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_C65T    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_C65T    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_C65T    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_C65T    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_C65T    63125          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_C65T    60000          /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_C65T    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_C65T    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_C65T    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_C65T     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_C65T     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_C65T     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_C65T     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_C65T     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_C65T     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_C65T     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_C65T    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_C65T    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_C65T    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_C65T    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_C65T    61875          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_C65T    60000          /* 10uV */

/* C65 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_C65    2301000    /* KHz */
#define CPU_DVFS_FREQ1_L_C65    2215000    /* KHz */
#define CPU_DVFS_FREQ2_L_C65    2139000    /* KHz */
#define CPU_DVFS_FREQ3_L_C65    2074000    /* KHz */
#define CPU_DVFS_FREQ4_L_C65    2009000    /* KHz */
#define CPU_DVFS_FREQ5_L_C65    1944000    /* KHz */
#define CPU_DVFS_FREQ6_L_C65    1879000    /* KHz */
#define CPU_DVFS_FREQ7_L_C65    1814000    /* KHz */
#define CPU_DVFS_FREQ8_L_C65    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_C65    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_C65   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_C65   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_C65   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_C65   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_C65    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_C65    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_C65    1800000    /* KHz */
#define CPU_DVFS_FREQ1_LL_C65    1682000    /* KHz */
#define CPU_DVFS_FREQ2_LL_C65    1579000    /* KHz */
#define CPU_DVFS_FREQ3_LL_C65    1491000    /* KHz */
#define CPU_DVFS_FREQ4_LL_C65    1402000    /* KHz */
#define CPU_DVFS_FREQ5_LL_C65    1314000    /* KHz */
#define CPU_DVFS_FREQ6_LL_C65    1226000    /* KHz */
#define CPU_DVFS_FREQ7_LL_C65    1138000    /* KHz */
#define CPU_DVFS_FREQ8_LL_C65    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_C65     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_C65    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_C65    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_C65    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_C65    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_C65    501000    /* KHz */
#define CPU_DVFS_FREQ15_LL_C65    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_C65    1051000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_C65    1000000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_C65     955000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_C65     917000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_C65     878000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_C65     840000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_C65     801000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_C65     763000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_C65     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_C65     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_C65    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_C65    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_C65    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_C65    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_C65    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_C65    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_C65    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_C65    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_C65    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_C65     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_C65     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_C65     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_C65     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_C65     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_C65     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_C65     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_C65    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_C65    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_C65    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_C65    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_C65    61875          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_C65    60000          /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_C65    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_C65    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_C65    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_C65     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_C65     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_C65     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_C65     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_C65     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_C65     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_C65     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_C65    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_C65    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_C65    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_C65    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_C65    63125          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_C65    60000          /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_C65    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_C65    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_C65    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_C65     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_C65     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_C65     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_C65     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_C65     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_C65     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_C65     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_C65    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_C65    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_C65    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_C65    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_C65    61875          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_C65    60000          /* 10uV */

/* C62 */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_C62    2001000    /* KHz */
#define CPU_DVFS_FREQ1_L_C62    1961000    /* KHz */
#define CPU_DVFS_FREQ2_L_C62    1927000    /* KHz */
#define CPU_DVFS_FREQ3_L_C62    1897000    /* KHz */
#define CPU_DVFS_FREQ4_L_C62    1868000    /* KHz */
#define CPU_DVFS_FREQ5_L_C62    1838000    /* KHz */
#define CPU_DVFS_FREQ6_L_C62    1809000    /* KHz */
#define CPU_DVFS_FREQ7_L_C62    1779000    /* KHz */
#define CPU_DVFS_FREQ8_L_C62    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_C62    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_C62   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_C62   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_C62   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_C62   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_C62    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_C62    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_C62    1500000    /* KHz */
#define CPU_DVFS_FREQ1_LL_C62    1429000    /* KHz */
#define CPU_DVFS_FREQ2_LL_C62    1367000    /* KHz */
#define CPU_DVFS_FREQ3_LL_C62    1314000    /* KHz */
#define CPU_DVFS_FREQ4_LL_C62    1261000    /* KHz */
#define CPU_DVFS_FREQ5_LL_C62    1208000    /* KHz */
#define CPU_DVFS_FREQ6_LL_C62    1155000    /* KHz */
#define CPU_DVFS_FREQ7_LL_C62    1102000    /* KHz */
#define CPU_DVFS_FREQ8_LL_C62    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_C62     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_C62    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_C62    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_C62    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_C62    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_C62    501000    /* KHz */
#define CPU_DVFS_FREQ15_LL_C62    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_C62    1048000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_C62     997000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_C62     953000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_C62     915000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_C62     877000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_C62     839000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_C62     801000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_C62     763000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_C62     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_C62     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_C62    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_C62    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_C62    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_C62    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_C62    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_C62    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_C62    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_C62    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_C62    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_C62     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_C62     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_C62     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_C62     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_C62     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_C62     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_C62     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_C62    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_C62    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_C62    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_C62    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_C62    61875          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_C62    60000          /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_C62    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_C62    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_C62    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_C62     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_C62     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_C62     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_C62     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_C62     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_C62     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_C62     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_C62    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_C62    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_C62    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_C62    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_C62    63125          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_C62    60000          /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_C62    111875          /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_C62    106875          /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_C62    102500          /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_C62     98750          /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_C62     95000          /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_C62     91250          /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_C62     87500          /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_C62     83750          /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_C62     80000          /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_C62     76875          /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_C62    73750          /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_C62    70625          /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_C62    67500          /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_C62    64375          /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_C62    61875          /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_C62    60000          /* 10uV */

/* C62LY */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_C62LY    2001000    /* KHz */
#define CPU_DVFS_FREQ1_L_C62LY    1961000    /* KHz */
#define CPU_DVFS_FREQ2_L_C62LY    1927000    /* KHz */
#define CPU_DVFS_FREQ3_L_C62LY    1897000    /* KHz */
#define CPU_DVFS_FREQ4_L_C62LY    1868000    /* KHz */
#define CPU_DVFS_FREQ5_L_C62LY    1838000    /* KHz */
#define CPU_DVFS_FREQ6_L_C62LY    1809000    /* KHz */
#define CPU_DVFS_FREQ7_L_C62LY    1779000    /* KHz */
#define CPU_DVFS_FREQ8_L_C62LY    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_C62LY    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_C62LY   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_C62LY   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_C62LY   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_C62LY   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_C62LY    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_C62LY    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_C62LY    1500000    /* KHz */
#define CPU_DVFS_FREQ1_LL_C62LY    1429000    /* KHz */
#define CPU_DVFS_FREQ2_LL_C62LY    1367000    /* KHz */
#define CPU_DVFS_FREQ3_LL_C62LY    1314000    /* KHz */
#define CPU_DVFS_FREQ4_LL_C62LY    1261000    /* KHz */
#define CPU_DVFS_FREQ5_LL_C62LY    1208000    /* KHz */
#define CPU_DVFS_FREQ6_LL_C62LY    1155000    /* KHz */
#define CPU_DVFS_FREQ7_LL_C62LY    1102000    /* KHz */
#define CPU_DVFS_FREQ8_LL_C62LY    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_C62LY     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_C62LY    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_C62LY    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_C62LY    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_C62LY    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_C62LY    501000    /* KHz */
#define CPU_DVFS_FREQ15_LL_C62LY    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_C62LY    1048000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_C62LY     997000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_C62LY     953000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_C62LY     915000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_C62LY     877000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_C62LY     839000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_C62LY     801000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_C62LY     763000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_C62LY     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_C62LY     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_C62LY    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_C62LY    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_C62LY    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_C62LY    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_C62LY    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_C62LY    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_C62LY    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_C62LY    108125           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_C62LY    104375           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_C62LY    101250           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_C62LY     98125           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_C62LY     95000           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_C62LY     91875           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_C62LY     88750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_C62LY     85000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_C62LY     81250           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_C62LY    80000           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_C62LY    73750           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_C62LY    69375           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_C62LY    65625           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_C62LY    62500           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_C62LY    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_C62LY    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_C62LY    108125           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_C62LY    104375           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_C62LY    101250           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_C62LY     98125           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_C62LY     95000           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_C62LY     91875           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_C62LY     88750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_C62LY     85000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_C62LY     81250           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_C62LY    80000           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_C62LY    73750           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_C62LY    69375           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_C62LY    65625           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_C62LY    64375           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_C62LY    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_C62LY    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_C62LY    108125           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_C62LY    104375           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_C62LY    101250           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_C62LY     98125           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_C62LY     95000           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_C62LY     91875           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_C62LY     88750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_C62LY     85000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_C62LY     81250           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_C62LY    80000           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_C62LY    73750           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_C62LY    69375           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_C62LY    65625           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_C62LY    62500           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_C62LY    60000           /* 10uV */

/* C65R */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_C65R    2201000    /* KHz */
#define CPU_DVFS_FREQ1_L_C65R    2130000    /* KHz */
#define CPU_DVFS_FREQ2_L_C65R    2068000    /* KHz */
#define CPU_DVFS_FREQ3_L_C65R    2015000    /* KHz */
#define CPU_DVFS_FREQ4_L_C65R    1962000    /* KHz */
#define CPU_DVFS_FREQ5_L_C65R    1909000    /* KHz */
#define CPU_DVFS_FREQ6_L_C65R    1856000    /* KHz */
#define CPU_DVFS_FREQ7_L_C65R    1803000    /* KHz */
#define CPU_DVFS_FREQ8_L_C65R    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_C65R    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_C65R   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_C65R   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_C65R   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_C65R   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_C65R    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_C65R    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_C65R    1601000    /* KHz */
#define CPU_DVFS_FREQ1_LL_C65R    1515000    /* KHz */
#define CPU_DVFS_FREQ2_LL_C65R    1439000    /* KHz */
#define CPU_DVFS_FREQ3_LL_C65R    1374000    /* KHz */
#define CPU_DVFS_FREQ4_LL_C65R    1309000    /* KHz */
#define CPU_DVFS_FREQ5_LL_C65R    1244000    /* KHz */
#define CPU_DVFS_FREQ6_LL_C65R    1179000    /* KHz */
#define CPU_DVFS_FREQ7_LL_C65R    1114000    /* KHz */
#define CPU_DVFS_FREQ8_LL_C65R    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_C65R     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_C65R    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_C65R    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_C65R    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_C65R    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_C65R    501000    /* KHz */
#define CPU_DVFS_FREQ15_LL_C65R    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_C65R    1051000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_C65R    1000000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_C65R     955000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_C65R     917000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_C65R     878000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_C65R     840000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_C65R     801000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_C65R     763000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_C65R     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_C65R     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_C65R    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_C65R    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_C65R    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_C65R    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_C65R    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_C65R    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_C65R    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_C65R    106875           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_C65R    102500           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_C65R     98750           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_C65R     95000           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_C65R     91250           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_C65R     87500           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_C65R     83750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_C65R     80000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_C65R     76875           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_C65R    73750           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_C65R    70625           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_C65R    67500           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_C65R    64375           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_C65R    61875           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_C65R    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_C65R    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_C65R    106875           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_C65R    102500           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_C65R     98750           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_C65R     95000           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_C65R     91250           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_C65R     87500           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_C65R     83750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_C65R     80000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_C65R     76875           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_C65R    73750           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_C65R    70625           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_C65R    67500           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_C65R    64375           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_C65R    63125           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_C65R    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_C65R    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_C65R    106875           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_C65R    102500           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_C65R     98750           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_C65R     95000           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_C65R     91250           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_C65R     87500           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_C65R     83750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_C65R     80000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_C65R     76875           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_C65R    73750           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_C65R    70625           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_C65R    67500           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_C65R    64375           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_C65R    61875           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_C65R    60000           /* 10uV */

/* C62D */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_C62D    1801000    /* KHz */
#define CPU_DVFS_FREQ1_L_C62D    1793000    /* KHz */
#define CPU_DVFS_FREQ2_L_C62D    1786000    /* KHz */
#define CPU_DVFS_FREQ3_L_C62D    1780000    /* KHz */
#define CPU_DVFS_FREQ4_L_C62D    1774000    /* KHz */
#define CPU_DVFS_FREQ5_L_C62D    1768000    /* KHz */
#define CPU_DVFS_FREQ6_L_C62D    1762000    /* KHz */
#define CPU_DVFS_FREQ7_L_C62D    1756000    /* KHz */
#define CPU_DVFS_FREQ8_L_C62D    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_C62D    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_C62D   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_C62D   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_C62D   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_C62D   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_C62D    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_C62D    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_C62D    1500000    /* KHz */
#define CPU_DVFS_FREQ1_LL_C62D    1429000    /* KHz */
#define CPU_DVFS_FREQ2_LL_C62D    1367000    /* KHz */
#define CPU_DVFS_FREQ3_LL_C62D    1314000    /* KHz */
#define CPU_DVFS_FREQ4_LL_C62D    1261000    /* KHz */
#define CPU_DVFS_FREQ5_LL_C62D    1208000    /* KHz */
#define CPU_DVFS_FREQ6_LL_C62D    1155000    /* KHz */
#define CPU_DVFS_FREQ7_LL_C62D    1102000    /* KHz */
#define CPU_DVFS_FREQ8_LL_C62D    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_C62D     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_C62D    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_C62D    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_C62D    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_C62D    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_C62D    501000    /* KHz */
#define CPU_DVFS_FREQ15_LL_C62D    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_C62D    1048000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_C62D     997000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_C62D     953000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_C62D     915000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_C62D     877000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_C62D     839000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_C62D     801000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_C62D     763000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_C62D     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_C62D     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_C62D    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_C62D    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_C62D    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_C62D    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_C62D    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_C62D    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_C62D    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_C62D    106875           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_C62D    102500           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_C62D     98750           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_C62D     95000           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_C62D     91250           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_C62D     87500           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_C62D     83750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_C62D     80000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_C62D     76875           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_C62D    73750           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_C62D    70625           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_C62D    67500           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_C62D    64375           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_C62D    61875           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_C62D    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_C62D    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_C62D    106875           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_C62D    102500           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_C62D     98750           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_C62D     95000           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_C62D     91250           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_C62D     87500           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_C62D     83750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_C62D     80000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_C62D     76875           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_C62D    73750           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_C62D    70625           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_C62D    67500           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_C62D    64375           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_C62D    63125           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_C62D    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_C62D    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_C62D    106875           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_C62D    102500           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_C62D     98750           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_C62D     95000           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_C62D     91250           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_C62D     87500           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_C62D     83750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_C62D     80000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_C62D     76875           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_C62D    73750           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_C62D    70625           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_C62D    67500           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_C62D    64375           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_C62D    61875           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_C62D    60000           /* 10uV */

/* C62DLY */
/* for DVFS OPP table L */
#define CPU_DVFS_FREQ0_L_C62DLY    1801000    /* KHz */
#define CPU_DVFS_FREQ1_L_C62DLY    1793000    /* KHz */
#define CPU_DVFS_FREQ2_L_C62DLY    1786000    /* KHz */
#define CPU_DVFS_FREQ3_L_C62DLY    1780000    /* KHz */
#define CPU_DVFS_FREQ4_L_C62DLY    1774000    /* KHz */
#define CPU_DVFS_FREQ5_L_C62DLY    1768000    /* KHz */
#define CPU_DVFS_FREQ6_L_C62DLY    1762000    /* KHz */
#define CPU_DVFS_FREQ7_L_C62DLY    1756000    /* KHz */
#define CPU_DVFS_FREQ8_L_C62DLY    1750000    /* KHz */
#define CPU_DVFS_FREQ9_L_C62DLY    1617000    /* KHz */
#define CPU_DVFS_FREQ10_L_C62DLY   1484000    /* KHz */
#define CPU_DVFS_FREQ11_L_C62DLY   1351000    /* KHz */
#define CPU_DVFS_FREQ12_L_C62DLY   1218000    /* KHz */
#define CPU_DVFS_FREQ13_L_C62DLY   1085000    /* KHz */
#define CPU_DVFS_FREQ14_L_C62DLY    979000    /* KHz */
#define CPU_DVFS_FREQ15_L_C62DLY    900000    /* KHz */

/* for DVFS OPP table LL */
#define CPU_DVFS_FREQ0_LL_C62DLY    1500000    /* KHz */
#define CPU_DVFS_FREQ1_LL_C62DLY    1429000    /* KHz */
#define CPU_DVFS_FREQ2_LL_C62DLY    1367000    /* KHz */
#define CPU_DVFS_FREQ3_LL_C62DLY    1314000    /* KHz */
#define CPU_DVFS_FREQ4_LL_C62DLY    1261000    /* KHz */
#define CPU_DVFS_FREQ5_LL_C62DLY    1208000    /* KHz */
#define CPU_DVFS_FREQ6_LL_C62DLY    1155000    /* KHz */
#define CPU_DVFS_FREQ7_LL_C62DLY    1102000    /* KHz */
#define CPU_DVFS_FREQ8_LL_C62DLY    1050000    /* KHz */
#define CPU_DVFS_FREQ9_LL_C62DLY     948000    /* KHz */
#define CPU_DVFS_FREQ10_LL_C62DLY    846000    /* KHz */
#define CPU_DVFS_FREQ11_LL_C62DLY    745000    /* KHz */
#define CPU_DVFS_FREQ12_LL_C62DLY    643000    /* KHz */
#define CPU_DVFS_FREQ13_LL_C62DLY    542000    /* KHz */
#define CPU_DVFS_FREQ14_LL_C62DLY    501000    /* KHz */
#define CPU_DVFS_FREQ15_LL_C62DLY    400000    /* KHz */

/* for DVFS OPP table CCI */
#define CPU_DVFS_FREQ0_CCI_C62DLY    1048000    /* KHz */
#define CPU_DVFS_FREQ1_CCI_C62DLY     997000    /* KHz */
#define CPU_DVFS_FREQ2_CCI_C62DLY     953000    /* KHz */
#define CPU_DVFS_FREQ3_CCI_C62DLY     915000    /* KHz */
#define CPU_DVFS_FREQ4_CCI_C62DLY     877000    /* KHz */
#define CPU_DVFS_FREQ5_CCI_C62DLY     839000    /* KHz */
#define CPU_DVFS_FREQ6_CCI_C62DLY     801000    /* KHz */
#define CPU_DVFS_FREQ7_CCI_C62DLY     763000    /* KHz */
#define CPU_DVFS_FREQ8_CCI_C62DLY     724000    /* KHz */
#define CPU_DVFS_FREQ9_CCI_C62DLY     658000    /* KHz */
#define CPU_DVFS_FREQ10_CCI_C62DLY    592000    /* KHz */
#define CPU_DVFS_FREQ11_CCI_C62DLY    525000    /* KHz */
#define CPU_DVFS_FREQ12_CCI_C62DLY    459000    /* KHz */
#define CPU_DVFS_FREQ13_CCI_C62DLY    392000    /* KHz */
#define CPU_DVFS_FREQ14_CCI_C62DLY    339000    /* KHz */
#define CPU_DVFS_FREQ15_CCI_C62DLY    300000    /* KHz */

/* for DVFS OPP table */
#define CPU_DVFS_VOLT0_VPROC_L_C62DLY    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_L_C62DLY    108125           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_L_C62DLY    104375           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_L_C62DLY    101250           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_L_C62DLY     98125           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_L_C62DLY     95000           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_L_C62DLY     91875           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_L_C62DLY     88750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_L_C62DLY     85000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_L_C62DLY     81250           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_L_C62DLY    80000           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_L_C62DLY    73750           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_L_C62DLY    69375           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_L_C62DLY    65625           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_L_C62DLY    62500           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_L_C62DLY    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_LL_C62DLY    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_LL_C62DLY    108125           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_LL_C62DLY    104375           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_LL_C62DLY    101250           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_LL_C62DLY     98125           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_LL_C62DLY     95000           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_LL_C62DLY     91875           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_LL_C62DLY     88750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_LL_C62DLY     85000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_LL_C62DLY     81250           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_LL_C62DLY    80000           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_LL_C62DLY    73750           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_LL_C62DLY    69375           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_LL_C62DLY    65625           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_LL_C62DLY    64375           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_LL_C62DLY    60000           /* 10uV */

#define CPU_DVFS_VOLT0_VPROC_CCI_C62DLY    111875           /* 10uV */
#define CPU_DVFS_VOLT1_VPROC_CCI_C62DLY    108125           /* 10uV */
#define CPU_DVFS_VOLT2_VPROC_CCI_C62DLY    104375           /* 10uV */
#define CPU_DVFS_VOLT3_VPROC_CCI_C62DLY    101250           /* 10uV */
#define CPU_DVFS_VOLT4_VPROC_CCI_C62DLY     98125           /* 10uV */
#define CPU_DVFS_VOLT5_VPROC_CCI_C62DLY     95000           /* 10uV */
#define CPU_DVFS_VOLT6_VPROC_CCI_C62DLY     91875           /* 10uV */
#define CPU_DVFS_VOLT7_VPROC_CCI_C62DLY     88750           /* 10uV */
#define CPU_DVFS_VOLT8_VPROC_CCI_C62DLY     85000           /* 10uV */
#define CPU_DVFS_VOLT9_VPROC_CCI_C62DLY     81250           /* 10uV */
#define CPU_DVFS_VOLT10_VPROC_CCI_C62DLY    80000           /* 10uV */
#define CPU_DVFS_VOLT11_VPROC_CCI_C62DLY    73750           /* 10uV */
#define CPU_DVFS_VOLT12_VPROC_CCI_C62DLY    69375           /* 10uV */
#define CPU_DVFS_VOLT13_VPROC_CCI_C62DLY    65625           /* 10uV */
#define CPU_DVFS_VOLT14_VPROC_CCI_C62DLY    62500           /* 10uV */
#define CPU_DVFS_VOLT15_VPROC_CCI_C62DLY    60000           /* 10uV */
/* DVFS OPP table */
#define OPP_TBL(cluster, seg, lv, vol)	\
static struct mt_cpu_freq_info opp_tbl_##cluster##_e##lv##_0[] = {        \
	OP                                                                \
(CPU_DVFS_FREQ0_##cluster##_##seg, CPU_DVFS_VOLT0_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ1_##cluster##_##seg, CPU_DVFS_VOLT1_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ2_##cluster##_##seg, CPU_DVFS_VOLT2_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ3_##cluster##_##seg, CPU_DVFS_VOLT3_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ4_##cluster##_##seg, CPU_DVFS_VOLT4_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ5_##cluster##_##seg, CPU_DVFS_VOLT5_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ6_##cluster##_##seg, CPU_DVFS_VOLT6_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ7_##cluster##_##seg, CPU_DVFS_VOLT7_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ8_##cluster##_##seg, CPU_DVFS_VOLT8_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ9_##cluster##_##seg, CPU_DVFS_VOLT9_VPROC_##vol##_##seg),   \
	OP                                                                \
(CPU_DVFS_FREQ10_##cluster##_##seg, CPU_DVFS_VOLT10_VPROC_##vol##_##seg), \
	OP                                                                \
(CPU_DVFS_FREQ11_##cluster##_##seg, CPU_DVFS_VOLT11_VPROC_##vol##_##seg), \
	OP                                                                \
(CPU_DVFS_FREQ12_##cluster##_##seg, CPU_DVFS_VOLT12_VPROC_##vol##_##seg), \
	OP                                                                \
(CPU_DVFS_FREQ13_##cluster##_##seg, CPU_DVFS_VOLT13_VPROC_##vol##_##seg), \
	OP                                                                \
(CPU_DVFS_FREQ14_##cluster##_##seg, CPU_DVFS_VOLT14_VPROC_##vol##_##seg), \
	OP                                                                \
(CPU_DVFS_FREQ15_##cluster##_##seg, CPU_DVFS_VOLT15_VPROC_##vol##_##seg), \
}

OPP_TBL(L,   FY, 0, L); /* opp_tbl_L_e0_0   */
OPP_TBL(LL,  FY, 0, LL); /* opp_tbl_LL_e0_0  */
OPP_TBL(CCI, FY, 0, CCI); /* opp_tbl_CCI_e0_0 */

OPP_TBL(L,   SB, 1, L); /* opp_tbl_L_e1_0   */
OPP_TBL(LL,  SB, 1, LL); /* opp_tbl_LL_e1_0  */
OPP_TBL(CCI, SB, 1, CCI); /* opp_tbl_CCI_e1_0 */

OPP_TBL(L,   C65T, 2, L); /* opp_tbl_L_e2_0   */
OPP_TBL(LL,  C65T, 2, LL); /* opp_tbl_LL_e2_0  */
OPP_TBL(CCI, C65T, 2, CCI); /* opp_tbl_CCI_e2_0 */

OPP_TBL(L,   C65, 3, L); /* opp_tbl_L_e3_0   */
OPP_TBL(LL,  C65, 3, LL); /* opp_tbl_LL_e3_0  */
OPP_TBL(CCI, C65, 3, CCI); /* opp_tbl_CCI_e3_0 */

OPP_TBL(L,   C62, 4, L); /* opp_tbl_L_e4_0   */
OPP_TBL(LL,  C62, 4, LL); /* opp_tbl_LL_e4_0  */
OPP_TBL(CCI, C62, 4, CCI); /* opp_tbl_CCI_e4_0 */

OPP_TBL(L,   C62LY, 5, L); /* opp_tbl_L_e5_0   */
OPP_TBL(LL,  C62LY, 5, LL); /* opp_tbl_LL_e5_0  */
OPP_TBL(CCI, C62LY, 5, CCI); /* opp_tbl_CCI_e5_0 */

OPP_TBL(L,   C65R, 6, L); /* opp_tbl_L_e6_0   */
OPP_TBL(LL,  C65R, 6, LL); /* opp_tbl_LL_e6_0  */
OPP_TBL(CCI, C65R, 6, CCI); /* opp_tbl_CCI_e6_0 */

OPP_TBL(L,   C62D, 7, L); /* opp_tbl_L_e7_0   */
OPP_TBL(LL,  C62D, 7, LL); /* opp_tbl_LL_e7_0  */
OPP_TBL(CCI, C62D, 7, CCI); /* opp_tbl_CCI_e7_0 */

OPP_TBL(L,   C62DLY, 8, L); /* opp_tbl_L_e8_0   */
OPP_TBL(LL,  C62DLY, 8, LL); /* opp_tbl_LL_e8_0  */
OPP_TBL(CCI, C62DLY, 8, CCI); /* opp_tbl_CCI_e8_0 */
/* v1.2 */
struct opp_tbl_info opp_tbls[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
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
		[CPU_LEVEL_8] = { opp_tbl_L_e8_0,
				ARRAY_SIZE(opp_tbl_L_e8_0) },
	},
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
		[CPU_LEVEL_8] = { opp_tbl_LL_e8_0,
				ARRAY_SIZE(opp_tbl_LL_e8_0) },
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
				ARRAY_SIZE(opp_tbl_CCI_e6_0) }, //C65R
		[CPU_LEVEL_7] = { opp_tbl_CCI_e7_0,
				ARRAY_SIZE(opp_tbl_CCI_e7_0) }, //C62D
		[CPU_LEVEL_8] = { opp_tbl_CCI_e8_0,
				ARRAY_SIZE(opp_tbl_CCI_e8_0) }, //C62DLY
	},
};

/* 16 steps OPP table */
/* FY */
static struct mt_cpu_freq_method opp_tbl_method_L_FY[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_FY[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_FY[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* SB */
static struct mt_cpu_freq_method opp_tbl_method_L_SB[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_SB[] = {
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
	FP(4,	2),
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_SB[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* C65T */
static struct mt_cpu_freq_method opp_tbl_method_L_C65T[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_C65T[] = {
	/* POS,	CLK */
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
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_C65T[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* C65 */
static struct mt_cpu_freq_method opp_tbl_method_L_C65[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_C65[] = {
	/* POS,	CLK */
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
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_C65[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* C62 */
static struct mt_cpu_freq_method opp_tbl_method_L_C62[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_C62[] = {
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
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_C62[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* C62LY */
static struct mt_cpu_freq_method opp_tbl_method_L_C62LY[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_C62LY[] = {
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
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_C62LY[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* C65R */
static struct mt_cpu_freq_method opp_tbl_method_L_C65R[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_C65R[] = {
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
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_C65R[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* C62D */
static struct mt_cpu_freq_method opp_tbl_method_L_C62D[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_C62D[] = {
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
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_C62D[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};

/* C62DLY */
static struct mt_cpu_freq_method opp_tbl_method_L_C62DLY[] = {
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

static struct mt_cpu_freq_method opp_tbl_method_LL_C62DLY[] = {
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
	FP(4,	2),
};

static struct mt_cpu_freq_method opp_tbl_method_CCI_C62DLY[] = {
	/* POS,	CLK */
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
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
	FP(4,	2),
};
/* v1.2 */
struct opp_tbl_m_info opp_tbls_m[NR_MT_CPU_DVFS][NUM_CPU_LEVEL] = {
	/* L */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_L_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_L_SB },
		[CPU_LEVEL_2] = { opp_tbl_method_L_C65T },
		[CPU_LEVEL_3] = { opp_tbl_method_L_C65 },
		[CPU_LEVEL_4] = { opp_tbl_method_L_C62 },
		[CPU_LEVEL_5] = { opp_tbl_method_L_C62LY },
		[CPU_LEVEL_6] = { opp_tbl_method_L_C65R },
		[CPU_LEVEL_7] = { opp_tbl_method_L_C62D },
		[CPU_LEVEL_8] = { opp_tbl_method_L_C62DLY },
	},
	/* LL */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_LL_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_LL_SB },
		[CPU_LEVEL_2] = { opp_tbl_method_LL_C65T },
		[CPU_LEVEL_3] = { opp_tbl_method_LL_C65 },
		[CPU_LEVEL_4] = { opp_tbl_method_LL_C62 },
		[CPU_LEVEL_5] = { opp_tbl_method_LL_C62LY },
		[CPU_LEVEL_6] = { opp_tbl_method_LL_C65R },
		[CPU_LEVEL_7] = { opp_tbl_method_LL_C62D },
		[CPU_LEVEL_8] = { opp_tbl_method_LL_C62DLY },
	},
	/* CCI */
	{
		[CPU_LEVEL_0] = { opp_tbl_method_CCI_FY },
		[CPU_LEVEL_1] = { opp_tbl_method_CCI_SB },
		[CPU_LEVEL_2] = { opp_tbl_method_CCI_C65T },
		[CPU_LEVEL_3] = { opp_tbl_method_CCI_C65 },
		[CPU_LEVEL_4] = { opp_tbl_method_CCI_C62 },
		[CPU_LEVEL_5] = { opp_tbl_method_CCI_C62LY },
		[CPU_LEVEL_6] = { opp_tbl_method_CCI_C65R },
		[CPU_LEVEL_7] = { opp_tbl_method_CCI_C62D },
		[CPU_LEVEL_8] = { opp_tbl_method_CCI_C62DLY },
	},
};
