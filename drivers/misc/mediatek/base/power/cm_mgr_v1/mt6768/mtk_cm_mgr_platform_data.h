/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
unsigned int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {40, 40};
unsigned int cpu_power_ratio_down[CM_MGR_EMI_OPP] = {100, 60};
unsigned int vcore_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100};
unsigned int vcore_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100};
unsigned int debounce_times_up_adb[CM_MGR_EMI_OPP] = {0, 0};
unsigned int debounce_times_down_adb[CM_MGR_EMI_OPP] = {0, 0};
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
int cm_mgr_blank_status;
int cm_mgr_perf_enable = 1;
int cm_mgr_perf_timer_enable;
int cm_mgr_perf_force_enable;
int cm_mgr_loading_level = 1000;
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
	{68, 127},
	{209, 357},
	{242, 433},
	{258, 470},
	{276, 500},
	{306, 531},
	{320, 566},
	{327, 600},
	{343, 615},
	{343, 631},
	{343, 647},
	{343, 663},
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
	{13, 2, 10, 2},
	{25, 5, 20, 4},
	{285, 84, 31, 6},
	{283, 82, 273, 80},
	{281, 80, 269, 77},
	{279, 77, 264, 75},
	{278, 75, 260, 72},
	{276, 73, 256, 69},
	{274, 71, 251, 67},
	{272, 68, 247, 64},
	{270, 66, 243, 61},
	{268, 64, 238, 58},
	{267, 62, 234, 56},
	{265, 60, 230, 53},
	{263, 57, 225, 50},
	{261, 55, 221, 48},
	{259, 53, 216, 45},
	{257, 51, 212, 42},
	{255, 48, 208, 40},
	{254, 46, 203, 37},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{18, 3, 13, 2},
	{321, 90, 27, 5},
	{323, 89, 309, 86},
	{325, 87, 306, 84},
	{327, 86, 304, 82},
	{329, 85, 301, 80},
	{332, 83, 299, 77},
	{334, 82, 296, 75},
	{336, 81, 294, 73},
	{338, 79, 291, 71},
	{340, 78, 289, 68},
	{343, 76, 286, 66},
	{345, 75, 284, 64},
	{347, 74, 281, 62},
	{349, 72, 279, 60},
	{351, 71, 277, 57},
	{353, 70, 274, 55},
	{356, 68, 272, 53},
	{358, 67, 269, 51},
	{360, 66, 267, 49},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{13, 2, 10, 2},
	{25, 5, 20, 4},
	{38, 7, 31, 6},
	{51, 9, 41, 7},
	{408, 102, 51, 9},
	{398, 98, 61, 11},
	{387, 94, 370, 91},
	{377, 91, 357, 87},
	{367, 87, 344, 83},
	{357, 83, 331, 79},
	{346, 80, 319, 75},
	{336, 76, 306, 70},
	{326, 72, 293, 66},
	{315, 68, 280, 62},
	{305, 65, 267, 58},
	{295, 61, 255, 54},
	{285, 57, 242, 50},
	{274, 54, 229, 45},
	{264, 50, 216, 41},
	{254, 46, 203, 37},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{18, 3, 13, 2},
	{36, 7, 27, 5},
	{54, 10, 40, 7},
	{480, 99, 53, 10},
	{472, 96, 449, 92},
	{465, 94, 437, 89},
	{457, 92, 425, 86},
	{450, 90, 412, 84},
	{442, 88, 400, 81},
	{435, 86, 388, 78},
	{427, 84, 376, 75},
	{420, 82, 364, 72},
	{412, 80, 352, 69},
	{405, 78, 339, 66},
	{397, 76, 327, 63},
	{390, 74, 315, 60},
	{382, 72, 303, 57},
	{375, 70, 291, 54},
	{367, 68, 279, 51},
	{360, 66, 267, 49},
};

static unsigned int cpu_power_gain_UpLow1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{6, 1, 4, 1},
	{11, 2, 8, 1},
	{17, 3, 11, 2},
	{22, 4, 15, 3},
	{28, 5, 19, 3},
	{33, 6, 23, 4},
	{258, 66, 26, 5},
	{247, 63, 30, 6},
	{235, 59, 34, 6},
	{224, 55, 207, 52},
	{212, 52, 194, 49},
	{201, 48, 180, 45},
	{190, 45, 167, 41},
	{178, 41, 154, 37},
	{167, 38, 141, 33},
	{156, 34, 128, 29},
	{144, 31, 115, 25},
	{133, 27, 102, 21},
	{121, 24, 89, 18},
	{110, 20, 76, 14},
};

static unsigned int cpu_power_gain_DownLow1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{6, 1, 4, 1},
	{13, 2, 8, 2},
	{19, 3, 12, 2},
	{25, 5, 17, 3},
	{32, 6, 21, 4},
	{259, 72, 25, 5},
	{250, 68, 29, 5},
	{240, 65, 33, 6},
	{231, 61, 211, 58},
	{221, 58, 200, 54},
	{212, 54, 188, 50},
	{202, 51, 176, 46},
	{193, 47, 165, 42},
	{183, 44, 153, 38},
	{174, 40, 141, 34},
	{164, 37, 130, 31},
	{155, 33, 118, 27},
	{145, 30, 106, 23},
	{136, 26, 95, 19},
	{126, 23, 83, 15},
};

static unsigned int cpu_power_gain_UpHigh1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{6, 1, 4, 1},
	{11, 2, 8, 1},
	{17, 3, 11, 2},
	{22, 4, 15, 3},
	{28, 5, 19, 3},
	{33, 6, 23, 4},
	{39, 7, 26, 5},
	{44, 8, 30, 6},
	{50, 9, 34, 6},
	{55, 10, 38, 7},
	{248, 65, 42, 8},
	{233, 60, 45, 8},
	{218, 55, 49, 9},
	{202, 50, 53, 10},
	{187, 45, 57, 10},
	{171, 40, 144, 35},
	{156, 35, 127, 30},
	{141, 30, 110, 24},
	{125, 25, 93, 19},
	{110, 20, 76, 14},
};

static unsigned int cpu_power_gain_DownHigh1[][CM_MGR_CPU_ARRAY_SIZE] = {
	{6, 1, 4, 1},
	{13, 2, 8, 2},
	{19, 3, 12, 2},
	{25, 5, 17, 3},
	{32, 6, 21, 4},
	{38, 7, 25, 5},
	{44, 8, 29, 5},
	{50, 9, 33, 6},
	{57, 10, 37, 7},
	{319, 65, 42, 8},
	{299, 61, 46, 8},
	{280, 57, 50, 9},
	{261, 52, 54, 10},
	{242, 48, 211, 43},
	{222, 44, 190, 38},
	{203, 40, 169, 33},
	{184, 36, 147, 29},
	{165, 31, 126, 24},
	{145, 27, 104, 20},
	{126, 23, 83, 15},
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
