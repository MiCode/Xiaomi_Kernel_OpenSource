/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_CM_MGR_PLATFORM_DATA_H__
#define __MTK_CM_MGR_PLATFORM_DATA_H__

#ifndef PROC_FOPS_RW
#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}
#endif /* PROC_FOPS_RW */

#ifndef PROC_FOPS_RO
#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}
#endif /* PROC_FOPS_RO */

#ifndef PROC_ENTRY
#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
#endif /* PROC_ENTRY */

int light_load_cps = 1000;
static int cm_mgr_loop_count;
static int cm_mgr_dram_level;
static int cm_mgr_loop;
static int total_bw_value;
unsigned int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100};
unsigned int cpu_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100};
unsigned int vcore_power_ratio_up[CM_MGR_EMI_OPP] = {80, 100};
unsigned int vcore_power_ratio_down[CM_MGR_EMI_OPP] = {80, 100};
unsigned int debounce_times_up_adb[CM_MGR_EMI_OPP] = {0, 3};
unsigned int debounce_times_down_adb[CM_MGR_EMI_OPP] = {0, 3};
int debounce_times_reset_adb;
int debounce_times_perf_down = 50;
int debounce_times_perf_force_down = 100;
static int update;
static int update_v2f_table = 1;
static int cm_mgr_opp_enable = 1;
int cm_mgr_enable = 1;
#if defined(CONFIG_MTK_TINYSYS_SSPM_SUPPORT) && defined(USE_CM_MGR_AT_SSPM)
int cm_mgr_sspm_enable = 1;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#ifdef USE_TIMER_CHECK
int cm_mgr_timer_enable = 1;
#endif /* USE_TIMER_CHECK */
int cm_mgr_ratio_timer_enable;
int cm_mgr_disable_fb = 1;
int cm_mgr_blank_status = 1;
int cm_mgr_perf_enable = 1;
int cm_mgr_perf_timer_enable;
int cm_mgr_perf_force_enable;
int cm_mgr_loading_level;
int cm_mgr_loading_enable;
int cm_mgr_emi_demand_check = 1;

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
	{19, 45},
	{122, 396},
	{192, 296},
	{238, 302},
	{334, 319},
	{375, 395},
	{379, 454},
	{372, 480},
	{380, 502},
	{388, 525},
	{396, 547},
	{404, 569},
	{412, 591},
	{412, 380},
	{412, 384},
	{412, 389},
	{412, 393},
};

static int vcore_power_gain_2[][VCORE_ARRAY_SIZE] = {
	{26, 148},
	{59, 867},
	{134, 565},
	{172, 569},
	{219, 591},
	{204, 629},
	{189, 654},
	{188, 666},
	{185, 676},
	{183, 686},
	{183, 696},
	{183, 707},
	{183, 718},
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
	case 2:
		return VCORE_POWER_ARRAY_SIZE(2);
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
	case 2:
		return VCORE_POWER_GAIN_PTR(2);
	}

	pr_info("#@# %s(%d) warning value %d\n", __func__, __LINE__, idx);
	return NULL;
};

static int *vcore_power_gain = VCORE_POWER_GAIN_PTR(0);
#define vcore_power_gain(p, i, j) (*(p + (i) * VCORE_ARRAY_SIZE + (j)))

static unsigned int _v2f_all[][CM_MGR_CPU_CLUSTER] = {
	/* SB */
	{280, 275},
	{236, 232},
	{180, 180},
	{151, 151},
	{125, 125},
	{113, 113},
	{102, 102},
	{93, 93},
	{84, 84},
	{76, 76},
	{68, 68},
	{59, 59},
	{50, 50},
	{42, 42},
	{35, 35},
	{29, 29},
};

static unsigned int cpu_power_gain_UpLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 26, 1, 6},
	{7, 52, 1, 12},
	{74, 260, 2, 17},
	{74, 275, 3, 23},
	{74, 291, 4, 29},
	{73, 306, 4, 35},
	{73, 321, 5, 40},
	{72, 336, 6, 46},
	{72, 351, 7, 52},
	{72, 367, 7, 58},
	{71, 382, 8, 63},
	{71, 397, 9, 69},
	{70, 412, 36, 150},
	{70, 427, 33, 145},
	{69, 443, 30, 140},
	{69, 458, 27, 135},
	{69, 473, 24, 130},
	{68, 488, 21, 125},
	{68, 503, 18, 120},
	{67, 519, 15, 115},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{7, 52, 1, 6},
	{88, 314, 2, 13},
	{90, 354, 3, 19},
	{93, 394, 3, 26},
	{95, 434, 4, 32},
	{98, 474, 5, 39},
	{101, 515, 6, 45},
	{103, 555, 7, 52},
	{106, 595, 8, 58},
	{108, 635, 8, 65},
	{111, 675, 9, 71},
	{114, 716, 43, 171},
	{116, 756, 40, 166},
	{119, 796, 36, 161},
	{122, 836, 33, 156},
	{124, 876, 30, 150},
	{127, 916, 27, 145},
	{129, 957, 23, 140},
	{132, 997, 20, 135},
	{135, 1037, 17, 130},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 26, 1, 6},
	{7, 52, 1, 12},
	{10, 78, 2, 17},
	{13, 104, 3, 23},
	{114, 404, 4, 29},
	{110, 411, 4, 35},
	{107, 419, 5, 40},
	{104, 427, 6, 46},
	{101, 434, 7, 52},
	{98, 442, 7, 58},
	{95, 450, 8, 63},
	{92, 457, 9, 69},
	{89, 465, 10, 75},
	{86, 473, 10, 81},
	{83, 480, 11, 86},
	{80, 488, 12, 92},
	{77, 496, 13, 98},
	{73, 503, 13, 104},
	{70, 511, 14, 109},
	{67, 519, 15, 115},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{7, 52, 1, 6},
	{13, 104, 2, 13},
	{135, 482, 3, 19},
	{135, 515, 3, 26},
	{135, 547, 4, 32},
	{135, 580, 5, 39},
	{135, 613, 6, 45},
	{135, 645, 7, 52},
	{135, 678, 8, 58},
	{135, 711, 8, 65},
	{135, 743, 9, 71},
	{135, 776, 10, 78},
	{135, 808, 11, 84},
	{135, 841, 12, 91},
	{135, 874, 13, 97},
	{135, 906, 13, 104},
	{135, 939, 14, 110},
	{135, 972, 15, 117},
	{135, 1004, 16, 123},
	{135, 1037, 17, 130},
};

static unsigned int cpu_power_gain_UpLow1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 17, 2, 13},
	{4, 35, 3, 26},
	{7, 52, 5, 39},
	{9, 69, 7, 52},
	{68, 247, 8, 65},
	{66, 254, 63, 228},
	{65, 260, 61, 230},
	{63, 267, 59, 232},
	{62, 274, 57, 235},
	{60, 280, 55, 237},
	{59, 287, 53, 239},
	{57, 293, 50, 241},
	{56, 300, 48, 244},
	{54, 306, 46, 246},
	{53, 313, 44, 248},
	{51, 319, 42, 250},
	{50, 326, 40, 253},
	{48, 333, 38, 255},
	{46, 339, 36, 257},
	{45, 346, 34, 259},
};

static unsigned int cpu_power_gain_DownLow1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 26, 2, 17},
	{7, 52, 4, 35},
	{80, 276, 7, 52},
	{79, 290, 9, 69},
	{79, 305, 73, 261},
	{78, 319, 71, 267},
	{77, 333, 69, 273},
	{76, 347, 67, 278},
	{76, 362, 65, 284},
	{75, 376, 64, 289},
	{74, 390, 62, 295},
	{73, 404, 60, 301},
	{73, 419, 58, 306},
	{72, 433, 56, 312},
	{71, 447, 54, 318},
	{70, 461, 52, 323},
	{70, 476, 50, 329},
	{69, 490, 49, 334},
	{68, 504, 47, 340},
	{67, 519, 45, 346},
};

static unsigned int cpu_power_gain_UpHigh1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 17, 2, 13},
	{4, 35, 3, 26},
	{7, 52, 5, 39},
	{9, 69, 7, 52},
	{11, 86, 8, 65},
	{13, 104, 10, 78},
	{16, 121, 12, 91},
	{95, 358, 13, 104},
	{91, 357, 15, 117},
	{87, 356, 81, 312},
	{83, 355, 77, 307},
	{79, 354, 72, 302},
	{74, 353, 67, 296},
	{70, 352, 62, 291},
	{66, 351, 57, 286},
	{62, 350, 53, 281},
	{57, 349, 48, 275},
	{53, 348, 43, 270},
	{49, 347, 38, 265},
	{45, 346, 34, 259},
};

static unsigned int cpu_power_gain_DownHigh1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 26, 2, 17},
	{7, 52, 4, 35},
	{10, 78, 7, 52},
	{13, 104, 9, 69},
	{118, 418, 11, 86},
	{115, 424, 13, 104},
	{112, 431, 16, 121},
	{108, 438, 99, 369},
	{105, 445, 95, 367},
	{101, 451, 90, 365},
	{98, 458, 86, 363},
	{95, 465, 81, 361},
	{91, 471, 77, 359},
	{88, 478, 72, 357},
	{84, 485, 68, 355},
	{81, 492, 63, 353},
	{78, 498, 58, 351},
	{74, 505, 54, 350},
	{71, 512, 49, 348},
	{67, 519, 45, 346},
};

static unsigned int cpu_power_gain_UpLow2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 13, 1, 7},
	{3, 26, 2, 15},
	{5, 39, 3, 22},
	{7, 52, 4, 30},
	{8, 65, 5, 37},
	{63, 228, 6, 44},
	{61, 230, 7, 52},
	{59, 232, 8, 59},
	{57, 235, 9, 67},
	{55, 237, 47, 181},
	{53, 239, 45, 178},
	{50, 241, 42, 175},
	{48, 244, 39, 171},
	{46, 246, 36, 168},
	{44, 248, 33, 165},
	{42, 250, 31, 161},
	{40, 253, 28, 158},
	{38, 255, 25, 155},
	{36, 257, 22, 151},
	{34, 259, 19, 148},
};

static unsigned int cpu_power_gain_DownLow2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 17, 1, 9},
	{4, 35, 2, 17},
	{7, 52, 3, 26},
	{9, 69, 4, 34},
	{73, 261, 6, 43},
	{71, 267, 7, 52},
	{69, 273, 8, 60},
	{67, 278, 9, 69},
	{65, 284, 55, 206},
	{64, 289, 52, 203},
	{62, 295, 49, 200},
	{60, 301, 46, 197},
	{58, 306, 43, 194},
	{56, 312, 40, 191},
	{54, 318, 37, 188},
	{52, 323, 34, 185},
	{50, 329, 31, 182},
	{49, 334, 28, 178},
	{47, 340, 25, 175},
	{45, 346, 22, 172},
};

static unsigned int cpu_power_gain_UpHigh2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 13, 1, 7},
	{3, 26, 2, 15},
	{5, 39, 3, 22},
	{7, 52, 4, 30},
	{8, 65, 5, 37},
	{10, 78, 6, 44},
	{12, 91, 7, 52},
	{13, 104, 8, 59},
	{15, 117, 9, 67},
	{81, 312, 10, 74},
	{77, 307, 11, 81},
	{72, 302, 12, 89},
	{67, 296, 12, 96},
	{62, 291, 13, 103},
	{57, 286, 14, 111},
	{53, 281, 15, 118},
	{48, 275, 36, 180},
	{43, 270, 30, 170},
	{38, 265, 25, 159},
	{34, 259, 19, 148},
};

static unsigned int cpu_power_gain_DownHigh2[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 17, 1, 9},
	{4, 35, 2, 17},
	{7, 52, 3, 26},
	{9, 69, 4, 34},
	{11, 86, 6, 43},
	{13, 104, 7, 52},
	{16, 121, 8, 60},
	{99, 369, 9, 69},
	{95, 367, 10, 78},
	{90, 365, 11, 86},
	{86, 363, 12, 95},
	{81, 361, 13, 103},
	{77, 359, 15, 112},
	{72, 357, 16, 121},
	{68, 355, 51, 225},
	{63, 353, 45, 215},
	{58, 351, 39, 204},
	{54, 350, 34, 194},
	{49, 348, 28, 183},
	{45, 346, 22, 172},
};

#define cpu_power_gain(p, i, j) (*(p + (i) * CM_MGR_CPU_ARRAY_SIZE + (j)))
#define CPU_POWER_GAIN(a, b, c) \
	(&cpu_power_gain_##a##b##c[0][0])

static unsigned int *_cpu_power_gain_ptr(int isUP, int isLow, int idx)
{
	switch (isUP) {
	case 0:
		switch (isLow) {
		case 0:
			switch (idx) {
			case 0:
				return CPU_POWER_GAIN(Down, High, 0);
			case 1:
				return CPU_POWER_GAIN(Down, High, 1);
			}
			break;
		case 1:
			switch (idx) {
			case 0:
				return CPU_POWER_GAIN(Down, Low, 0);
			case 1:
				return CPU_POWER_GAIN(Down, Low, 1);
			}
			break;
		}
		break;
	case 1:
		switch (isLow) {
		case 0:
			switch (idx) {
			case 0:
				return CPU_POWER_GAIN(Up, High, 0);
			case 1:
				return CPU_POWER_GAIN(Up, High, 1);
			}
			break;
		case 1:
			switch (idx) {
			case 0:
				return CPU_POWER_GAIN(Up, Low, 0);
			case 1:
				return CPU_POWER_GAIN(Up, Low, 1);
			}
			break;
		}
		break;
	}

	pr_info("#@# %s(%d) warning value %d\n", __func__, __LINE__, idx);
	return NULL;
};

static unsigned int *cpu_power_gain_up = CPU_POWER_GAIN(Up, High, 0);
static unsigned int *cpu_power_gain_down = CPU_POWER_GAIN(Down, High, 0);

static void cpu_power_gain_ptr(int opp, int tbl, int cluster)
{
	if (opp < CM_MGR_LOWER_OPP) {
		switch (tbl) {
		case 0:
			cpu_power_gain_up = CPU_POWER_GAIN(Up, Low, 0);
			cpu_power_gain_down = CPU_POWER_GAIN(Down, Low, 0);
			break;
		case 1:
			cpu_power_gain_up = CPU_POWER_GAIN(Up, Low, 1);
			cpu_power_gain_down = CPU_POWER_GAIN(Down, Low, 1);
			break;
		case 2:
			cpu_power_gain_up = CPU_POWER_GAIN(Up, Low, 2);
			cpu_power_gain_down = CPU_POWER_GAIN(Down, Low, 2);
			break;
		}
	} else {
		switch (tbl) {
		case 0:
			cpu_power_gain_up = CPU_POWER_GAIN(Up, High, 0);
			cpu_power_gain_down = CPU_POWER_GAIN(Down, High, 0);
			break;
		case 1:
			cpu_power_gain_up = CPU_POWER_GAIN(Up, High, 1);
			cpu_power_gain_down = CPU_POWER_GAIN(Down, High, 1);
			break;
		case 2:
			cpu_power_gain_up = CPU_POWER_GAIN(Up, High, 2);
			cpu_power_gain_down = CPU_POWER_GAIN(Down, High, 2);
			break;
		}
	}
}

int cpu_power_gain_opp(int bw, int is_up, int opp, int ratio_idx, int idx)
{
	cpu_power_gain_ptr(opp, cm_mgr_get_idx(), idx % CM_MGR_CPU_CLUSTER);

	if (is_up)
		return cpu_power_gain(cpu_power_gain_up, ratio_idx, idx);
	else
		return cpu_power_gain(cpu_power_gain_down, ratio_idx, idx);
}

#endif	/* __MTK_CM_MGR_PLATFORM_DATA_H__ */
