/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CM_MGR_PLATFORM_DATA_H__
#define __MTK_CM_MGR_PLATFORM_DATA_H__

static int cm_mgr_loop;
static int update_v2f_table = 1;
#ifdef USE_TIMER_CHECK
static int cm_mgr_timer_enable = 1;
#endif /* USE_TIMER_CHECK */
static int cm_mgr_perf_timer_enable;

static int vcore_power_gain_0[][VCORE_ARRAY_SIZE] = {
	{64, 165},
	{518, 545},
	{488, 653},
	{540, 727},
	{653, 795},
	{770, 856},
	{833, 930},
	{852, 1005},
	{882, 1043},
	{912, 1081},
	{943, 1118},
	{973, 1156},
	{1003, 1194},
	{1003, 1214},
	{1003, 1234},
	{1003, 1253},
	{1003, 1273},
	{1003, 1293},
	{1003, 1313},
	{1003, 1333},
	{1003, 1353},
};

static int vcore_power_gain_1[][VCORE_ARRAY_SIZE] = {
	{36, 176},
	{109, 460},
	{123, 548},
	{120, 586},
	{127, 614},
	{144, 648},
	{154, 681},
	{168, 704},
	{179, 727},
	{179, 751},
	{179, 774},
	{179, 797},
};

#define VCORE_POWER_ARRAY_SIZE(name) \
	(sizeof(vcore_power_gain_##name) / \
	 sizeof(unsigned int) / \
	 VCORE_ARRAY_SIZE)

#define VCORE_POWER_GAIN_PTR(name) \
	(&vcore_power_gain_##name[0][0])

static int vcore_power_array_size(int idx)
{
	switch (idx) {
	case 0:
		return VCORE_POWER_ARRAY_SIZE(0);
	case 1:
		return VCORE_POWER_ARRAY_SIZE(1);
	}

	pr_info("#@# %s(%d) warning value %d\n", __func__, __LINE__, idx);
	return 0;
};

static int *vcore_power_gain_ptr(int idx)
{
	switch (idx) {
	case 0:
		return VCORE_POWER_GAIN_PTR(0);
	case 1:
		return VCORE_POWER_GAIN_PTR(1);
	}

	pr_info("#@# %s(%d) warning value %d\n", __func__, __LINE__, idx);
	return NULL;
};

static int *vcore_power_gain = VCORE_POWER_GAIN_PTR(0);
#define vcore_power_gain(p, i, j) (*(p + (i) * VCORE_ARRAY_SIZE + (j)))

static unsigned int _v2f_all[][CM_MGR_CPU_CLUSTER] = {
	{280},
	{236},
	{180},
	{151},
	{125},
	{113},
	{102},
	{93},
	{84},
	{76},
	{68},
	{59},
	{50},
	{42},
	{35},
	{29},
};

static unsigned int cpu_power_gain_UpLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 2},
	{6, 4},
	{99, 7},
	{97, 94},
	{95, 91},
	{92, 88},
	{90, 85},
	{87, 82},
	{85, 78},
	{83, 75},
	{80, 72},
	{78, 69},
	{76, 66},
	{73, 63},
	{71, 60},
	{69, 57},
	{66, 54},
	{64, 51},
	{61, 47},
	{59, 44},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 3},
	{113, 6},
	{112, 107},
	{110, 104},
	{109, 101},
	{108, 99},
	{106, 96},
	{105, 93},
	{103, 90},
	{102, 87},
	{101, 85},
	{99, 82},
	{98, 79},
	{97, 76},
	{95, 73},
	{94, 70},
	{93, 68},
	{91, 65},
	{90, 62},
	{89, 59},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 2},
	{6, 4},
	{9, 7},
	{12, 9},
	{78, 11},
	{77, 13},
	{76, 71},
	{75, 69},
	{73, 67},
	{72, 65},
	{71, 63},
	{69, 61},
	{68, 58},
	{67, 56},
	{66, 54},
	{64, 52},
	{63, 50},
	{62, 48},
	{60, 46},
	{59, 44},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 3},
	{9, 6},
	{13, 9},
	{95, 12},
	{95, 87},
	{94, 85},
	{94, 83},
	{93, 82},
	{93, 80},
	{93, 78},
	{92, 76},
	{92, 74},
	{91, 72},
	{91, 70},
	{91, 68},
	{90, 67},
	{90, 65},
	{89, 63},
	{89, 61},
	{89, 59},
};

static unsigned int cpu_power_gain_UpLow1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 1},
	{4, 2},
	{7, 3},
	{94, 4},
	{91, 5},
	{88, 6},
	{85, 7},
	{82, 8},
	{78, 67},
	{75, 63},
	{72, 59},
	{69, 54},
	{66, 50},
	{63, 46},
	{60, 41},
	{57, 37},
	{54, 33},
	{51, 28},
	{47, 24},
	{44, 20},
};

static unsigned int cpu_power_gain_DownLow1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 1},
	{6, 2},
	{107, 3},
	{104, 4},
	{101, 6},
	{99, 7},
	{96, 8},
	{93, 78},
	{90, 74},
	{87, 69},
	{85, 64},
	{82, 60},
	{79, 55},
	{76, 50},
	{73, 46},
	{70, 41},
	{68, 36},
	{65, 32},
	{62, 27},
	{59, 22},
};

static unsigned int cpu_power_gain_UpHigh1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 1},
	{4, 2},
	{7, 3},
	{9, 4},
	{11, 5},
	{13, 6},
	{71, 7},
	{69, 8},
	{67, 9},
	{65, 10},
	{63, 11},
	{61, 12},
	{58, 13},
	{56, 14},
	{54, 36},
	{52, 33},
	{50, 29},
	{48, 26},
	{46, 23},
	{44, 20},
};

static unsigned int cpu_power_gain_DownHigh1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 1},
	{6, 2},
	{9, 3},
	{12, 4},
	{87, 6},
	{85, 7},
	{83, 8},
	{82, 9},
	{80, 10},
	{78, 11},
	{76, 12},
	{74, 13},
	{72, 48},
	{70, 44},
	{68, 41},
	{67, 37},
	{65, 33},
	{63, 30},
	{61, 26},
	{59, 22},
};

static unsigned int cpu_power_gain_UpLow2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 2},
	{6, 4},
	{99, 7},
	{97, 94},
	{95, 91},
	{92, 88},
	{90, 85},
	{87, 82},
	{85, 78},
	{83, 75},
	{80, 72},
	{78, 69},
	{76, 66},
	{73, 63},
	{71, 60},
	{69, 57},
	{66, 54},
	{64, 51},
	{61, 47},
	{59, 44},
};

static unsigned int cpu_power_gain_DownLow2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 3},
	{113, 6},
	{112, 107},
	{110, 104},
	{109, 101},
	{108, 99},
	{106, 96},
	{105, 93},
	{103, 90},
	{102, 87},
	{101, 85},
	{99, 82},
	{98, 79},
	{97, 76},
	{95, 73},
	{94, 70},
	{93, 68},
	{91, 65},
	{90, 62},
	{89, 59},
};

static unsigned int cpu_power_gain_UpHigh2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 2},
	{6, 4},
	{9, 7},
	{12, 9},
	{127, 11},
	{123, 13},
	{118, 113},
	{114, 108},
	{109, 103},
	{105, 97},
	{100, 92},
	{96, 87},
	{91, 81},
	{86, 76},
	{82, 71},
	{77, 66},
	{73, 60},
	{68, 55},
	{64, 50},
	{59, 44},
};

static unsigned int cpu_power_gain_DownHigh2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 3},
	{9, 6},
	{13, 9},
	{149, 12},
	{145, 138},
	{141, 133},
	{138, 127},
	{134, 122},
	{130, 117},
	{126, 112},
	{123, 106},
	{119, 101},
	{115, 96},
	{111, 91},
	{108, 85},
	{104, 80},
	{100, 75},
	{96, 70},
	{92, 64},
	{89, 59},
};

static unsigned int cpu_power_gain_UpLow3[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 1},
	{4, 2},
	{7, 3},
	{94, 4},
	{91, 5},
	{88, 6},
	{85, 7},
	{82, 8},
	{78, 67},
	{75, 63},
	{72, 59},
	{69, 54},
	{66, 50},
	{63, 46},
	{60, 41},
	{57, 37},
	{54, 33},
	{51, 28},
	{47, 24},
	{44, 20},
};

static unsigned int cpu_power_gain_DownLow3[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 1},
	{6, 2},
	{107, 3},
	{104, 4},
	{101, 6},
	{99, 7},
	{96, 8},
	{93, 78},
	{90, 74},
	{87, 69},
	{85, 64},
	{82, 60},
	{79, 55},
	{76, 50},
	{73, 46},
	{70, 41},
	{68, 36},
	{65, 32},
	{62, 27},
	{59, 22},
};

static unsigned int cpu_power_gain_UpHigh3[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 1},
	{4, 2},
	{7, 3},
	{9, 4},
	{11, 5},
	{13, 6},
	{113, 7},
	{108, 8},
	{103, 9},
	{97, 10},
	{92, 11},
	{87, 12},
	{81, 13},
	{76, 14},
	{71, 52},
	{66, 46},
	{60, 39},
	{55, 33},
	{50, 26},
	{44, 20},
};

static unsigned int cpu_power_gain_DownHigh3[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 1},
	{6, 2},
	{9, 3},
	{12, 4},
	{138, 6},
	{133, 7},
	{127, 8},
	{122, 9},
	{117, 10},
	{112, 11},
	{106, 12},
	{101, 13},
	{96, 72},
	{91, 65},
	{85, 58},
	{80, 51},
	{75, 43},
	{70, 36},
	{64, 29},
	{59, 22},
};

#define cpu_power_gain(p, i, j) (*(p + (i) * CM_MGR_CPU_ARRAY_SIZE + (j)))
#define CPU_POWER_GAIN(a, b, c) \
	(&cpu_power_gain_##a##b##c[0][0])

static unsigned int *cpu_power_gain_up = CPU_POWER_GAIN(Up, High, 0);
static unsigned int *cpu_power_gain_down = CPU_POWER_GAIN(Down, High, 0);

#include <mt-plat/mtk_chip.h>

static int cm_get_version = -1;
static int get_version(void)
{
#ifdef CM_GET_VERSION
	int val;

	if (cm_get_version >= 0)
		return cm_get_version;

	val = mt_get_chip_sw_ver();

	if (val >= CHIP_SW_VER_02)
		cm_get_version = 1;
	else if (val >= CHIP_SW_VER_01)
		cm_get_version = 0;

	return cm_get_version;
#else
	return 1;
#endif
}

static void cpu_power_gain_ptr(int opp, int tbl, int cluster)
{
	switch (get_version()) {
	case 0:
		if (opp < CM_MGR_LOWER_OPP) {
			switch (tbl) {
			case 0:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, Low, 0);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, Low, 0);
				break;
			case 1:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, Low, 1);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, Low, 1);
				break;
			}
		} else {
			switch (tbl) {
			case 0:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, High, 0);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, High, 0);
				break;
			case 1:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, High, 1);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, High, 1);
				break;
			}
		}
		break;
	case 1:
		if (opp < CM_MGR_LOWER_OPP) {
			switch (tbl) {
			case 0:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, Low, 2);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, Low, 2);
				break;
			case 1:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, Low, 3);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, Low, 3);
				break;
			}
		} else {
			switch (tbl) {
			case 0:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, High, 2);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, High, 2);
				break;
			case 1:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, High, 3);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, High, 3);
				break;
			}
		}
		break;
	}
}

static int cm_mgr_get_idx(void);
int cpu_power_gain_opp(int bw, int is_up, int opp, int ratio_idx, int idx)
{
	cpu_power_gain_ptr(opp, cm_mgr_get_idx(), idx % CM_MGR_CPU_CLUSTER);

	if (is_up)
		return cpu_power_gain(cpu_power_gain_up, ratio_idx, idx);
	else
		return cpu_power_gain(cpu_power_gain_down, ratio_idx, idx);
}

#endif	/* __MTK_CM_MGR_PLATFORM_DATA_H__ */
