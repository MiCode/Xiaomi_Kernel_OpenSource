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

/* Petrus_v1p3_2k6_game_MP_190815 */

/**********************************************
 * unified_power_data.h
 * This header file includes:
 * 1. Macros of SRAM related address
 * 2. Raw datas of unified power tables for each bank
 **********************************************/

#ifndef UNIFIED_POWER_DATA_H
#define UNIFIED_POWER_DATA_H

/* remember to sync to sspm upower */
#define UPOWER_CSRAM_BASE 0x0012a000
#define UPOWER_CSRAM_SIZE 0x3000 /* 12K bytes */
#define UPOWER_DVFS_OFF_BOTTOM 0x8 /* ignore the last 8 bytes */
/* limit should be at 0x12CFF4 */
#define UPOWER_TBL_LIMIT \
	((UPOWER_CSRAM_BASE)+(UPOWER_CSRAM_SIZE)-(UPOWER_DVFS_OFF_BOTTOM))

struct upower_tbl upower_tbl_l_6885 = {
	.row = {
		{.cap = 127, .volt = 60000, .dyn_pwr = 11815,
			.lkg_pwr = {19715, 19715, 19715, 19715, 19715, 19715} },
		{.cap = 155, .volt = 60000, .dyn_pwr = 15359,
			.lkg_pwr = {19715, 19715, 19715, 19715, 19715, 19715} },
		{.cap = 163, .volt = 61250, .dyn_pwr = 17311,
			.lkg_pwr = {20392, 20392, 20392, 20392, 20392, 20392} },
		{.cap = 171, .volt = 63125, .dyn_pwr = 19773,
			.lkg_pwr = {21408, 21408, 21408, 21408, 21408, 21408} },
		{.cap = 185, .volt = 65625, .dyn_pwr = 24367,
			.lkg_pwr = {22811, 22811, 22811, 22811, 22811, 22811} },
		{.cap = 197, .volt = 68750, .dyn_pwr = 30032,
			.lkg_pwr = {24742, 24742, 24742, 24742, 24742, 24742} },
		{.cap = 206, .volt = 71250, .dyn_pwr = 34921,
			.lkg_pwr = {26287, 26287, 26287, 26287, 26287, 26287} },
		{.cap = 222, .volt = 75000, .dyn_pwr = 43604,
			.lkg_pwr = {28604, 28604, 28604, 28604, 28604, 28604} },
		{.cap = 232, .volt = 77500, .dyn_pwr = 50739,
			.lkg_pwr = {30356, 30356, 30356, 30356, 30356, 30356} },
		{.cap = 241, .volt = 80625, .dyn_pwr = 59436,
			.lkg_pwr = {32605, 32605, 32605, 32605, 32605, 32605} },
		{.cap = 249, .volt = 83750, .dyn_pwr = 69059,
			.lkg_pwr = {35089, 35089, 35089, 35089, 35089, 35089} },
		{.cap = 260, .volt = 88125, .dyn_pwr = 82834,
			.lkg_pwr = {38818, 38818, 38818, 38818, 38818, 38818} },
		{.cap = 266, .volt = 90625, .dyn_pwr = 92075,
			.lkg_pwr = {41065, 41065, 41065, 41065, 41065, 41065} },
		{.cap = 272, .volt = 93125, .dyn_pwr = 101950,
			.lkg_pwr = {43483, 43483, 43483, 43483, 43483, 43483} },
		{.cap = 278, .volt = 96875, .dyn_pwr = 116732,
			.lkg_pwr = {47375, 47375, 47375, 47375, 47375, 47375} },
		{.cap = 284, .volt = 100000, .dyn_pwr = 131277,
			.lkg_pwr = {50838, 50838, 50838, 50838, 50838, 50838} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = UPOWER_OPP_NUM,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {19715} },
		{{0}, {19715} },
		{{0}, {19715} },
		{{0}, {19715} },
		{{0}, {19715} },
		{{0}, {19715} },
	},
};

struct upower_tbl upower_tbl_cluster_l_6885 = {
	.row = {
		{.cap = 127, .volt = 60000, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 155, .volt = 60000, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 163, .volt = 61250, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 171, .volt = 63125, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 185, .volt = 65625, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 197, .volt = 68750, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 206, .volt = 71250, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 222, .volt = 75000, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 232, .volt = 77500, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 241, .volt = 80625, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 249, .volt = 83750, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 260, .volt = 88125, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 266, .volt = 90625, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 272, .volt = 93125, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 278, .volt = 96875, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 284, .volt = 100000, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = UPOWER_OPP_NUM,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
	},
};

struct upower_tbl upower_tbl_b_6885 = {
	.row = {
		{.cap = 580, .volt = 60000, .dyn_pwr = 62929,
			.lkg_pwr = {81045, 81045, 81045, 81045, 81045, 81045} },
		{.cap = 627, .volt = 62500, .dyn_pwr = 77169,
			.lkg_pwr = {87429, 87429, 87429, 87429, 87429, 87429} },
		{.cap = 668, .volt = 64375, .dyn_pwr = 91395,
			.lkg_pwr = {92217, 92217, 92217, 92217, 92217, 92217} },
		{.cap = 718, .volt = 67500, .dyn_pwr = 114339,
			.lkg_pwr = {100179, 100179, 100179, 100179, 100179,
					100179} },
		{.cap = 769, .volt = 70625, .dyn_pwr = 140460,
			.lkg_pwr = {108136, 108136, 108136, 108136, 108136,
					108136} },
		{.cap = 801, .volt = 72500, .dyn_pwr = 159974,
			.lkg_pwr = {112907, 112907, 112907, 112907, 112907,
					112907} },
		{.cap = 830, .volt = 75000, .dyn_pwr = 184127,
			.lkg_pwr = {119268, 119268, 119268, 119268, 119268,
					119268} },
		{.cap = 870, .volt = 78750, .dyn_pwr = 226612,
			.lkg_pwr = {130022, 130022, 130022, 130022, 130022,
					130022} },
		{.cap = 898, .volt = 80625, .dyn_pwr = 252474,
			.lkg_pwr = {135441, 135441, 135441, 135441, 135441,
					135441} },
		{.cap = 932, .volt = 83750, .dyn_pwr = 293924,
			.lkg_pwr = {144611, 144611, 144611, 144611, 144611,
					144611} },
		{.cap = 944, .volt = 85625, .dyn_pwr = 316537,
			.lkg_pwr = {150473, 150473, 150473, 150473, 150473,
					150473} },
		{.cap = 964, .volt = 89375, .dyn_pwr = 363423,
			.lkg_pwr = {163636, 163636, 163636, 163636, 163636,
					163636} },
		{.cap = 978, .volt = 91875, .dyn_pwr = 399199,
			.lkg_pwr = {172614, 172614, 172614, 172614, 172614,
					172614} },
		{.cap = 993, .volt = 94375, .dyn_pwr = 437215,
			.lkg_pwr = {181659, 181659, 181659, 181659, 181659,
					181659} },
		{.cap = 1005, .volt = 96250, .dyn_pwr = 467183,
			.lkg_pwr = {189286, 189286, 189286, 189286, 189286,
					189286} },
		{.cap = 1024, .volt = 100000, .dyn_pwr = 527284,
			.lkg_pwr = {205383, 205383, 205383, 205383, 205383,
					205383} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = UPOWER_OPP_NUM,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {81045} },
		{{0}, {81045} },
		{{0}, {81045} },
		{{0}, {81045} },
		{{0}, {81045} },
		{{0}, {81045} },
	},
};

struct upower_tbl upower_tbl_cluster_b_6885 = {
	.row = {
		{.cap = 580, .volt = 60000, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 627, .volt = 62500, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 668, .volt = 64375, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 718, .volt = 67500, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 769, .volt = 70625, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 801, .volt = 72500, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 830, .volt = 75000, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 870, .volt = 78750, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 898, .volt = 80625, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 932, .volt = 83750, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 944, .volt = 85625, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 964, .volt = 89375, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 978, .volt = 91875, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 993, .volt = 94375, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 1005, .volt = 96250, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
		{.cap = 1024, .volt = 100000, .dyn_pwr = 0,
			.lkg_pwr = {0, 0, 0, 0, 0, 0} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = UPOWER_OPP_NUM,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
		{{0}, {0} },
	},
};

struct upower_tbl upower_tbl_cci_6885 = {
	.row = {
		{.cap = 0, .volt = 60000, .dyn_pwr = 8776,
			.lkg_pwr = {44739, 44739, 44739, 44739, 44739, 44739} },
		{.cap = 0, .volt = 62500, .dyn_pwr = 10677,
			.lkg_pwr = {47556, 47556, 47556, 47556, 47556, 47556} },
		{.cap = 0, .volt = 65000, .dyn_pwr = 13232,
			.lkg_pwr = {50374, 50374, 50374, 50374, 50374, 50374} },
		{.cap = 0, .volt = 66875, .dyn_pwr = 14908,
			.lkg_pwr = {52792, 52792, 52792, 52792, 52792, 52792} },
		{.cap = 0, .volt = 70625, .dyn_pwr = 19105,
			.lkg_pwr = {57628, 57628, 57628, 57628, 57628, 57628} },
		{.cap = 0, .volt = 72500, .dyn_pwr = 21710,
			.lkg_pwr = {60042, 60042, 60042, 60042, 60042, 60042} },
		{.cap = 0, .volt = 75000, .dyn_pwr = 24921,
			.lkg_pwr = {63261, 63261, 63261, 63261, 63261, 63261} },
		{.cap = 0, .volt = 78125, .dyn_pwr = 29473,
			.lkg_pwr = {67759, 67759, 67759, 67759, 67759, 67759} },
		{.cap = 0, .volt = 80625, .dyn_pwr = 33980,
			.lkg_pwr = {71563, 71563, 71563, 71563, 71563, 71563} },
		{.cap = 0, .volt = 83750, .dyn_pwr = 39296,
			.lkg_pwr = {77094, 77094, 77094, 77094, 77094, 77094} },
		{.cap = 0, .volt = 86875, .dyn_pwr = 44442,
			.lkg_pwr = {83031, 83031, 83031, 83031, 83031, 83031} },
		{.cap = 0, .volt = 89375, .dyn_pwr = 49171,
			.lkg_pwr = {87998, 87998, 87998, 87998, 87998, 87998} },
		{.cap = 0, .volt = 91875, .dyn_pwr = 54216,
			.lkg_pwr = {93640, 93640, 93640, 93640, 93640, 93640} },
		{.cap = 0, .volt = 94375, .dyn_pwr = 59545,
			.lkg_pwr = {99507, 99507, 99507, 99507, 99507, 99507} },
		{.cap = 0, .volt = 96875, .dyn_pwr = 64634,
			.lkg_pwr = {106498, 106498, 106498, 106498, 106498,
					106498} },
		{.cap = 0, .volt = 100000, .dyn_pwr = 72199,
			.lkg_pwr = {115705, 115705, 115705, 115705, 115705,
					115705} },
	},
	.lkg_idx = DEFAULT_LKG_IDX,
	.row_num = UPOWER_OPP_NUM,
	.nr_idle_states = NR_UPOWER_CSTATES,
	.idle_states = {
		{{0}, {44739} },
		{{0}, {44739} },
		{{0}, {44739} },
		{{0}, {44739} },
		{{0}, {44739} },
		{{0}, {44739} },
	},
};

#endif /* UNIFIED_POWER_DATA_H */
