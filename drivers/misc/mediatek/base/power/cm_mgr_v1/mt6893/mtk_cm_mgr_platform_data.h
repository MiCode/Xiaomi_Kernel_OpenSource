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

int x_ratio_enable = 1;
int cm_mgr_camera_enable;
unsigned int cpu_power_ratio_up_x_camera[CM_MGR_EMI_OPP] = {0, 0, 0, 30, 30};
unsigned int cpu_power_ratio_up_x[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 0};
#ifdef USE_BCPU_WEIGHT
int cpu_power_bcpu_weight_max = 100;
int cpu_power_bcpu_weight_min = 100;
#endif
#ifdef CM_TRIGEAR
int cpu_power_bbcpu_weight_max = 100;
int cpu_power_bbcpu_weight_min = 100;
#endif
#ifdef DSU_DVFS_ENABLE
unsigned int dsu_debounce_up = 5;
unsigned int dsu_debounce_down;
unsigned int dsu_diff_pwr_up = 10;
unsigned int dsu_diff_pwr_down;
unsigned int dsu_l_pwr_ratio = 100;
unsigned int dsu_b_pwr_ratio = 100;
unsigned int dsu_bb_pwr_ratio = 100;

#endif
unsigned int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {140, 140, 100, 100, 100};
unsigned int cpu_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100};
unsigned int vcore_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100};
unsigned int vcore_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100};
unsigned int debounce_times_up_adb[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 0};
unsigned int debounce_times_down_adb[CM_MGR_EMI_OPP] = {3, 0, 0, 0, 0};
int debounce_times_reset_adb;
int debounce_times_perf_down = 5;
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
int cm_mgr_blank_status;
int cm_mgr_perf_enable = 1;
int cm_mgr_perf_timer_enable;
int cm_mgr_perf_force_enable;
int cm_mgr_loading_level = 1000;
int cm_mgr_loading_enable;
int cm_mgr_emi_demand_check = 1;
#ifdef USE_CPU_TO_DRAM_MAP
int cm_mgr_cpu_map_dram_enable;
#endif /* USE_CPU_TO_DRAM_MAP */
#ifdef USE_CPU_TO_DRAM_MAP_NEW
int cm_mgr_cpu_map_emi_opp = 1;
int cm_mgr_cpu_map_skip_cpu_opp = 2;
#endif /* USE_CPU_TO_DRAM_MAP_NEW */

static int vcore_power_gain_0[][VCORE_ARRAY_SIZE] = {
	{137, 89, 119, 292, 524, 188},
	{140, 93, 119, 298, 532, 195},
	{143, 97, 119, 304, 539, 202},
	{145, 102, 118, 310, 547, 209},
	{148, 106, 118, 316, 555, 216},
	{150, 110, 118, 322, 562, 223},
	{153, 115, 118, 328, 570, 230},
	{155, 119, 117, 334, 577, 237},
	{158, 123, 117, 340, 585, 244},
	{160, 128, 117, 346, 593, 251},
	{163, 132, 117, 352, 600, 258},
	{165, 136, 117, 358, 608, 265},
	{168, 141, 116, 364, 616, 272},
	{170, 145, 116, 370, 623, 279},
	{173, 149, 116, 376, 631, 286},
	{175, 154, 116, 382, 638, 293},
	{178, 158, 115, 388, 646, 300},
	{180, 162, 115, 394, 654, 307},
	{183, 167, 115, 400, 661, 314},
	{185, 171, 115, 405, 669, 321},
	{188, 175, 114, 411, 677, 328},
	{190, 180, 114, 417, 684, 335},
	{193, 184, 114, 423, 692, 342},
	{195, 188, 114, 429, 699, 349},
	{198, 193, 113, 435, 707, 356},
	{200, 197, 113, 441, 715, 363},
	{203, 201, 113, 447, 722, 371},
	{205, 206, 113, 453, 730, 378},
	{208, 210, 113, 459, 738, 385},
	{210, 214, 112, 465, 745, 392},
	{213, 219, 112, 471, 753, 399},
	{215, 223, 112, 477, 761, 406},
	{218, 227, 112, 483, 768, 413},
	{220, 232, 111, 489, 776, 420},
	{223, 236, 111, 495, 783, 427},
	{225, 240, 111, 501, 791, 434},
	{228, 245, 111, 507, 799, 441},
	{231, 249, 110, 513, 806, 448},
	{233, 253, 110, 519, 814, 455},
	{236, 258, 110, 525, 822, 462},
	{238, 262, 110, 531, 829, 469},
	{241, 266, 109, 537, 837, 476},
	{243, 271, 109, 543, 844, 483},
	{246, 275, 109, 549, 852, 490},
	{248, 279, 109, 555, 860, 497},
	{251, 284, 109, 561, 867, 504},
	{253, 288, 108, 567, 875, 511},
	{256, 292, 108, 572, 883, 518},
	{258, 297, 108, 578, 890, 525},
	{261, 301, 108, 584, 898, 532},
	{263, 305, 107, 590, 906, 539},
	{266, 310, 107, 596, 913, 546},
	{268, 314, 107, 602, 921, 553},
	{271, 318, 107, 608, 928, 560},
	{273, 323, 106, 614, 936, 567},
	{276, 327, 106, 620, 944, 574},
	{278, 331, 106, 626, 951, 582},
	{281, 336, 106, 632, 959, 589},
	{283, 340, 105, 638, 967, 596},
	{286, 344, 105, 644, 974, 603},
	{288, 349, 105, 650, 982, 610},
	{291, 353, 105, 656, 989, 617},
	{293, 357, 105, 662, 997, 624},
	{296, 362, 104, 668, 1005, 631},
	{298, 366, 104, 674, 1012, 638},
	{301, 370, 104, 680, 1020, 645},
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
	}

	pr_info("#@# %s(%d) warning value %d\n", __func__, __LINE__, idx);
	return 0;
};

static int *vcore_power_gain_ptr(int idx)
{
	switch (idx) {
	case 0:
		return VCORE_POWER_GAIN_PTR(0);
	}

	pr_info("#@# %s(%d) warning value %d\n", __func__, __LINE__, idx);
	return NULL;
};

static int *vcore_power_gain = VCORE_POWER_GAIN_PTR(0);
#define vcore_power_gain(p, i, j) (*(p + (i) * VCORE_ARRAY_SIZE + (j)))

static unsigned int _v2f_all[][CM_MGR_CPU_CLUSTER] = {
	{200, 220, 250},
	{174, 194, 218},
	{154, 181, 203},
	{138, 163, 182},
	{125, 150, 166},
	{103, 130, 142},
	{91, 120, 130},
	{78, 106, 112},
	{68, 95, 100},
	{58, 78, 83},
	{48, 71, 71},
	{41, 62, 62},
	{33, 51, 51},
	{30, 42, 41},
	{27, 35, 34},
	{21, 30, 28},
};

static unsigned int cpu_power_gain_UpLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{1, 9, 1, 4, 1, 8, 1, 8, 1, 4},
	{3, 19, 1, 9, 2, 16, 2, 15, 1, 7},
	{4, 28, 2, 13, 4, 23, 4, 23, 2, 11},
	{6, 38, 3, 18, 5, 31, 5, 30, 2, 15},
	{76, 347, 4, 22, 6, 39, 6, 38, 3, 19},
	{73, 337, 4, 27, 72, 327, 71, 325, 4, 22},
	{70, 326, 5, 31, 68, 315, 68, 313, 4, 26},
	{67, 315, 6, 36, 65, 303, 65, 300, 5, 30},
	{64, 305, 6, 40, 62, 290, 61, 288, 5, 34},
	{61, 294, 53, 245, 58, 278, 58, 275, 6, 37},
	{58, 283, 49, 229, 55, 266, 54, 263, 7, 41},
	{55, 273, 45, 213, 52, 254, 51, 250, 44, 205},
	{52, 262, 41, 198, 48, 242, 48, 238, 40, 189},
	{48, 252, 37, 182, 45, 229, 44, 225, 36, 172},
	{45, 241, 34, 167, 42, 217, 41, 213, 32, 156},
	{42, 230, 30, 151, 38, 205, 37, 200, 28, 140},
	{39, 220, 26, 136, 35, 193, 34, 188, 24, 124},
	{36, 209, 22, 120, 32, 181, 31, 175, 20, 107},
	{33, 198, 18, 104, 28, 168, 27, 163, 16, 91},
	{30, 188, 14, 89, 25, 156, 24, 150, 12, 75},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 12, 1, 5, 1, 9, 1, 9, 1, 4},
	{4, 23, 2, 10, 3, 18, 3, 18, 1, 8},
	{6, 35, 2, 15, 4, 28, 4, 27, 2, 12},
	{90, 381, 3, 20, 6, 37, 6, 35, 3, 16},
	{87, 372, 4, 24, 85, 360, 84, 358, 3, 20},
	{83, 362, 5, 29, 81, 349, 81, 346, 4, 24},
	{80, 353, 5, 34, 77, 337, 77, 334, 5, 28},
	{77, 344, 6, 39, 74, 325, 73, 322, 5, 32},
	{73, 334, 64, 274, 70, 314, 69, 310, 6, 36},
	{70, 325, 59, 258, 66, 302, 66, 298, 6, 40},
	{67, 316, 55, 242, 63, 290, 62, 286, 54, 233},
	{63, 306, 51, 226, 59, 278, 58, 274, 49, 216},
	{60, 297, 46, 210, 55, 267, 54, 262, 45, 199},
	{57, 287, 42, 194, 52, 255, 51, 249, 40, 182},
	{53, 278, 37, 178, 48, 243, 47, 237, 35, 165},
	{50, 269, 33, 162, 44, 232, 43, 225, 31, 148},
	{47, 259, 29, 146, 41, 220, 39, 213, 26, 132},
	{43, 250, 24, 130, 37, 208, 36, 201, 22, 115},
	{40, 240, 20, 114, 33, 197, 32, 189, 17, 98},
	{37, 231, 16, 98, 29, 185, 28, 177, 13, 81},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{1, 9, 1, 4, 1, 8, 1, 8, 1, 4},
	{3, 19, 1, 9, 2, 16, 2, 15, 1, 7},
	{4, 28, 2, 13, 4, 23, 4, 23, 2, 11},
	{6, 38, 3, 18, 5, 31, 5, 30, 2, 15},
	{7, 47, 4, 22, 6, 39, 6, 38, 3, 19},
	{9, 56, 4, 27, 7, 47, 7, 45, 4, 22},
	{10, 66, 5, 31, 9, 55, 8, 53, 4, 26},
	{12, 75, 6, 36, 10, 62, 10, 60, 5, 30},
	{13, 84, 6, 40, 11, 70, 11, 68, 5, 34},
	{15, 94, 7, 44, 12, 78, 12, 75, 6, 37},
	{61, 383, 8, 49, 14, 86, 13, 83, 7, 41},
	{57, 361, 8, 53, 15, 94, 14, 90, 7, 45},
	{54, 340, 9, 58, 51, 319, 50, 315, 8, 49},
	{50, 318, 10, 62, 47, 296, 46, 292, 8, 52},
	{47, 296, 11, 67, 43, 272, 43, 268, 9, 56},
	{44, 274, 11, 71, 40, 249, 39, 245, 10, 60},
	{40, 253, 12, 76, 36, 226, 35, 221, 10, 64},
	{37, 231, 13, 80, 32, 203, 31, 197, 11, 67},
	{33, 209, 13, 84, 29, 179, 28, 174, 11, 71},
	{30, 188, 14, 89, 25, 156, 24, 150, 12, 75},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 12, 1, 5, 1, 9, 1, 9, 1, 4},
	{4, 23, 2, 10, 3, 18, 3, 18, 1, 8},
	{6, 35, 2, 15, 4, 28, 4, 27, 2, 12},
	{7, 46, 3, 20, 6, 37, 6, 35, 3, 16},
	{9, 58, 4, 24, 7, 46, 7, 44, 3, 20},
	{11, 69, 5, 29, 9, 55, 8, 53, 4, 24},
	{13, 81, 5, 34, 10, 65, 10, 62, 5, 28},
	{15, 92, 6, 39, 12, 74, 11, 71, 5, 32},
	{81, 496, 7, 44, 13, 83, 13, 80, 6, 36},
	{77, 472, 8, 49, 15, 92, 14, 88, 6, 40},
	{73, 448, 9, 54, 69, 423, 68, 418, 7, 44},
	{69, 424, 9, 59, 65, 396, 64, 392, 8, 49},
	{65, 400, 10, 63, 60, 370, 59, 365, 8, 53},
	{61, 376, 11, 68, 56, 344, 55, 338, 9, 57},
	{57, 352, 12, 73, 51, 317, 51, 311, 10, 61},
	{53, 328, 12, 78, 47, 291, 46, 284, 10, 65},
	{49, 303, 13, 83, 43, 264, 42, 257, 11, 69},
	{45, 279, 14, 88, 38, 238, 37, 231, 12, 73},
	{41, 255, 15, 93, 34, 211, 33, 204, 12, 77},
	{37, 231, 16, 98, 29, 185, 28, 177, 13, 81},
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
			}
			break;
		case 1:
			switch (idx) {
			case 0:
				return CPU_POWER_GAIN(Down, Low, 0);
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
			}
			break;
		case 1:
			switch (idx) {
			case 0:
				return CPU_POWER_GAIN(Up, Low, 0);
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
	switch (cluster) {
	case 0:
		if (opp < CM_MGR_LOWER_OPP) {
			switch (tbl) {
			case 0:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, Low, 0);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, Low, 0);
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
			}
		}
		break;
	case 1:
		if (opp < CM_MGR_LOWER_OPP) {
			switch (tbl) {
			case 0:
				cpu_power_gain_up =
					CPU_POWER_GAIN(Up, Low, 0);
				cpu_power_gain_down =
					CPU_POWER_GAIN(Down, Low, 0);
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
			}
		}
		break;
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
