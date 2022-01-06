// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/stringify.h>
#include <mtk_common_static_power.h>

char *spower_name[] = {
	"MTK_SPOWER_CPULL",
	"MTK_SPOWER_CPUL",
	"MTK_SPOWER_CPUB",
	"MTK_SPOWER_CCI",
	"MTK_SPOWER_GPU",
	"MTK_SPOWER_VCORE",
	"MTK_SPOWER_MODEM",
	"MTK_SPOWER_VPU",
	"MTK_SPOWER_VSRAM_PROC12",
	"MTK_SPOWER_VSRAM_PROC11",
	"MTK_SPOWER_VSRAM_OTHERS",
	"MTK_SPOWER_NR",
	"MTK_SPOWER_MDLA",
	"MTK_SPOWER_ADSP",
	"MTK_SPOWER_VSRAM_GPU",
	"MTK_SPOWER_VSRAM_VPU_MDLA",
	"MTK_SPOWER_VSRAM_MODEM",
	"MTK_SPOWER_VCORE_OFF",
	"MTK_SPOWER_MAX"
};

/* todo: devinfo_idx, devinfo_offset need to modify */
struct spower_leakage_info spower_lkg_info[MTK_SPOWER_MAX] = {
	[MTK_LL_LEAKAGE] = {
		.name = __stringify(MTK_LL_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_LL,
		.devinfo_offset = DEVINFO_OFF_LL,
		.value = DEF_CPULL_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_CPU,
		.t_of_fuse = T_OF_FUSE,
	},
	[MTK_L_LEAKAGE] = {
		.name = __stringify(MTK_L_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_L,
		.devinfo_offset = DEVINFO_OFF_L,
		.value = DEF_CPUL_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_CPU,
		.t_of_fuse = T_OF_FUSE,
	},
	[MTK_B_LEAKAGE] = {
		.name = __stringify(MTK_B_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_B,
		.devinfo_offset = DEVINFO_OFF_B,
		.value = DEF_CPUB_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_CPU,
		.t_of_fuse = T_OF_FUSE,
	},
	[MTK_CCI_LEAKAGE] = {
		.name = __stringify(MTK_CCI_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_CCI,
		.devinfo_offset = DEVINFO_OFF_CCI,
		.value = DEF_CCI_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_CPU,
		.t_of_fuse = T_OF_FUSE,
	},
	[MTK_GPU_LEAKAGE] = {
		.name = __stringify(MTK_GPU_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_GPU,
		.devinfo_offset = DEVINFO_OFF_GPU,
		.value = DEF_GPU_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_GPU,
		.t_of_fuse = T_OF_FUSE,
	},
	[MTK_VCORE_LEAKAGE] = {
		.name = __stringify(MTK_VCORE_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VCORE,
		.devinfo_offset = DEVINFO_OFF_VCORE,
		.value = DEF_VCORE_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VCORE,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_MODEM_LEAKAGE] = {
		.name = __stringify(MTK_MODEM_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_MODEM,
		.devinfo_offset = DEVINFO_OFF_MODEM,
		.value = DEF_MODEM_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_MODEM,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VPU_LEAKAGE] = {
		.name = __stringify(MTK_VPU_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VPU,
		.devinfo_offset = DEVINFO_OFF_VPU,
		.value = DEF_VPU_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VPU,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VSRAM_PROC12_LEAKAGE] = {
		.name = __stringify(MTK_VSRAM_PROC12_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VSRAM_PROC12,
		.devinfo_offset = DEVINFO_OFF_VSRAM_PROC12,
		.value = DEF_VSRAM_PROC12_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VSRAM_PROC12,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VSRAM_PROC11_LEAKAGE] = {
		.name = __stringify(MTK_VSRAM_PROC11_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VSRAM_PROC11,
		.devinfo_offset = DEVINFO_OFF_VSRAM_PROC11,
		.value = DEF_VSRAM_PROC11_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VSRAM_PROC11,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VSRAM_GPU_LEAKAGE] = {
		.name = __stringify(MTK_VSRAM_GPU_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VSRAM_GPU,
		.devinfo_offset = DEVINFO_OFF_VSRAM_GPU,
		.value = DEF_VSRAM_GPU_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VSRAM_GPU,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VSRAM_OTHERS_LEAKAGE] = {
		.name = __stringify(MTK_VSRAM_OTHERS_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VSRAM_OTHERS,
		.devinfo_offset = DEVINFO_OFF_VSRAM_OTHERS,
		.value = DEF_VSRAM_OTHERS_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VSRAM_OTHERS,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_NR_LEAKAGE] = {
		.name = __stringify(MTK_NR_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_NR,
		.devinfo_offset = DEVINFO_OFF_NR,
		.value = DEF_NR_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_NR,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_MDLA_LEAKAGE] = {
		.name = __stringify(MTK_MDLA_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_MDLA,
		.devinfo_offset = DEVINFO_OFF_MDLA,
		.value = DEF_MDLA_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_MDLA,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_ADSP_LEAKAGE] = {
		.name = __stringify(MTK_ADSP_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_ADSP,
		.devinfo_offset = DEVINFO_OFF_ADSP,
		.value = DEF_ADSP_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_ADSP,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VSRAM_GPU_LEAKAGE] = {
		.name = __stringify(MTK_VSRAM_GPU_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VSRAM_GPU,
		.devinfo_offset = DEVINFO_OFF_VSRAM_GPU,
		.value = DEF_VSRAM_GPU_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VSRAM_GPU,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VSRAM_VPU_MDLA_LEAKAGE] = {
		.name = __stringify(MTK_VSRAM_VPU_MDLA_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VSRAM_VPU_MDLA,
		.devinfo_offset = DEVINFO_OFF_VSRAM_VPU_MDLA,
		.value = DEF_VSRAM_VPU_MDLA_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VSRAM_VPU_MDLA,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VSRAM_MODEM_LEAKAGE] = {
		.name = __stringify(MTK_VSRAM_MODEM_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VSRAM_MODEM,
		.devinfo_offset = DEVINFO_OFF_VSRAM_MODEM,
		.value = DEF_VSRAM_MODEM_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VSRAM_MODEM,
		.t_of_fuse = T_OF_FUSE,
	},

	[MTK_VCORE_OFF_LEAKAGE] = {
		.name = __stringify(MTK_VCORE_OFF_LEAKAGE),
		.devinfo_idx = DEVINFO_IDX_VCORE_OFF,
		.devinfo_offset = DEVINFO_OFF_VCORE_OFF,
		.value = DEF_VCORE_OFF_LEAKAGE,
		.v_of_fuse = V_OF_FUSE_VCORE_OFF,
		.t_of_fuse = T_OF_FUSE,
	},
};
/****************************************************************
 * this table is generated by scramble function.                *
 * (plz refer to DE team.)                                      *
 ****************************************************************/
int devinfo_table[] = {
	3539,   492,    1038,   106,    231,    17,     46,     2179,
	4,      481,    1014,   103,    225,    17,     45,     2129,
	3,      516,    1087,   111,    242,    19,     49,     2282,
	4,      504,    1063,   108,    236,    18,     47,     2230,
	4,      448,    946,    96,     210,    15,     41,     1986,
	2,      438,    924,    93,     205,    14,     40,     1941,
	2,      470,    991,    101,    220,    16,     43,     2080,
	3,      459,    968,    98,     215,    16,     42,     2033,
	3,      594,    1250,   129,    279,    23,     57,     2621,
	6,      580,    1221,   126,    273,    22,     56,     2561,
	6,      622,    1309,   136,    293,    24,     60,     2745,
	7,      608,    1279,   132,    286,    23,     59,     2683,
	6,      541,    1139,   117,    254,    20,     51,     2390,
	5,      528,    1113,   114,    248,    19,     50,     2335,
	4,      566,    1193,   123,    266,    21,     54,     2503,
	5,      553,    1166,   120,    260,    21,     53,     2446,
	5,      338,    715,    70,     157,    9,      29,     1505,
	3153,   330,    699,    69,     153,    9,      28,     1470,
	3081,   354,    750,    74,     165,    10,     31,     1576,
	3302,   346,    732,    72,     161,    10,     30,     1540,
	3227,   307,    652,    63,     142,    8,      26,     1371,
	2875,   300,    637,    62,     139,    7,      25,     1340,
	2809,   322,    683,    67,     149,    8,      27,     1436,
	3011,   315,    667,    65,     146,    8,      26,     1404,
	2942,   408,    862,    86,     191,    13,     37,     1811,
	1,      398,    842,    84,     186,    12,     36,     1769,
	1,      428,    903,    91,     200,    14,     39,     1896,
	2,      418,    882,    89,     195,    13,     38,     1853,
	2,      371,    785,    78,     173,    11,     33,     1651,
	3458,   363,    767,    76,     169,    10,     32,     1613,
	3379,   389,    823,    82,     182,    12,     35,     1729,
	1,      380,    804,    80,     177,    11,     34,     1689,
};

