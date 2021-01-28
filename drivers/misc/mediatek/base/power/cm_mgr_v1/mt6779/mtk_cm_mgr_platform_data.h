/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015 MediaTek Inc.
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
int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100, 40, 40};
int cpu_power_ratio_down[CM_MGR_EMI_OPP] = {120, 120, 20, 20};
int vcore_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100, 100, 100};
int vcore_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100};
int debounce_times_up_adb[CM_MGR_EMI_OPP] = {0, 0, 0, 0};
int debounce_times_down_adb[CM_MGR_EMI_OPP] = {3, 0, 0, 0};
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

int cm_mgr_vcore_opp_to_bw[CM_MGR_VCORE_OPP_COUNT] = {
	540,
	460,
	340,
	460,
	340,
	460,
	340,
	460,
	340,
	340,
	230,
	340,
	230
};

static int vcore_power_gain_0[][VCORE_ARRAY_SIZE] = {
	{238, 63, 137, 265},
	{382, 22, 225, 494},
	{376, 48, 294, 473},
	{329, 94, 362, 423},
	{323, 130, 391, 402},
	{323, 146, 439, 381},
	{323, 181, 468, 371},
	{323, 207, 486, 360},
	{323, 253, 505, 349},
	{323, 288, 543, 328},
	{323, 288, 582, 308},
	{323, 288, 620, 327},
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
	{3, 14, 2, 10, 1, 6, 1, 6},
	{6, 28, 5, 20, 3, 11, 3, 12},
	{92, 320, 7, 31, 4, 17, 4, 18},
	{90, 318, 86, 302, 5, 23, 5, 23},
	{88, 315, 84, 296, 6, 29, 7, 29},
	{87, 313, 81, 289, 8, 34, 8, 35},
	{85, 311, 79, 283, 72, 252, 72, 253},
	{83, 309, 76, 277, 68, 241, 68, 242},
	{82, 307, 74, 271, 65, 231, 65, 232},
	{80, 305, 71, 265, 61, 220, 61, 222},
	{78, 303, 69, 259, 58, 209, 58, 211},
	{77, 301, 66, 253, 54, 199, 54, 201},
	{75, 299, 64, 247, 50, 188, 51, 190},
	{74, 297, 61, 241, 47, 178, 47, 180},
	{72, 295, 59, 235, 43, 167, 44, 169},
	{70, 293, 56, 229, 40, 157, 40, 159},
	{69, 291, 53, 223, 36, 146, 37, 149},
	{67, 289, 51, 217, 33, 135, 33, 138},
	{65, 287, 48, 211, 29, 125, 30, 128},
	{64, 285, 46, 205, 26, 114, 26, 117},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{5, 21, 3, 13, 1, 7, 2, 7},
	{101, 329, 6, 26, 3, 13, 3, 13},
	{101, 334, 96, 312, 4, 20, 5, 20},
	{100, 339, 94, 309, 6, 26, 6, 27},
	{100, 343, 92, 306, 7, 33, 8, 34},
	{99, 348, 89, 303, 81, 271, 81, 272},
	{99, 352, 87, 300, 77, 261, 77, 262},
	{98, 357, 85, 298, 73, 251, 74, 252},
	{98, 362, 83, 295, 70, 241, 70, 242},
	{97, 366, 81, 292, 66, 231, 66, 233},
	{97, 371, 79, 289, 62, 221, 63, 223},
	{96, 375, 76, 286, 59, 211, 59, 213},
	{96, 380, 74, 283, 55, 201, 55, 203},
	{95, 385, 72, 281, 51, 191, 52, 193},
	{95, 389, 70, 278, 48, 181, 48, 184},
	{94, 394, 68, 275, 44, 171, 45, 174},
	{94, 398, 65, 272, 40, 161, 41, 164},
	{93, 403, 63, 269, 37, 151, 37, 154},
	{93, 408, 61, 267, 33, 141, 34, 144},
	{92, 412, 59, 264, 29, 131, 30, 135},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 14, 2, 10, 1, 6, 1, 6},
	{6, 28, 5, 20, 3, 11, 3, 12},
	{10, 43, 7, 31, 4, 17, 4, 18},
	{13, 57, 9, 41, 5, 23, 5, 23},
	{16, 71, 11, 51, 6, 29, 7, 29},
	{112, 408, 14, 61, 8, 34, 8, 35},
	{109, 399, 16, 72, 9, 40, 9, 41},
	{105, 390, 98, 359, 10, 46, 10, 47},
	{102, 382, 94, 346, 11, 51, 12, 53},
	{98, 373, 89, 333, 13, 57, 13, 59},
	{95, 364, 85, 320, 14, 63, 14, 65},
	{91, 355, 81, 307, 15, 69, 16, 70},
	{88, 346, 76, 295, 63, 236, 64, 238},
	{84, 338, 72, 282, 58, 218, 58, 220},
	{81, 329, 68, 269, 52, 201, 53, 203},
	{77, 320, 63, 256, 47, 184, 48, 186},
	{74, 311, 59, 243, 42, 166, 42, 169},
	{71, 302, 55, 231, 36, 149, 37, 152},
	{67, 293, 50, 218, 31, 132, 32, 135},
	{64, 285, 46, 205, 26, 114, 26, 117},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{5, 21, 3, 13, 1, 7, 2, 7},
	{9, 41, 6, 26, 3, 13, 3, 13},
	{14, 62, 9, 40, 4, 20, 5, 20},
	{140, 532, 12, 53, 6, 26, 6, 27},
	{137, 525, 15, 66, 7, 33, 8, 34},
	{134, 517, 124, 473, 9, 39, 9, 40},
	{131, 510, 120, 458, 10, 46, 11, 47},
	{128, 502, 115, 443, 12, 52, 12, 54},
	{125, 495, 110, 428, 13, 59, 14, 61},
	{122, 487, 106, 413, 15, 65, 15, 67},
	{119, 480, 101, 398, 16, 72, 85, 278},
	{116, 472, 96, 383, 78, 260, 79, 262},
	{113, 465, 92, 368, 72, 244, 73, 246},
	{110, 457, 87, 353, 66, 228, 67, 230},
	{107, 450, 82, 338, 60, 211, 61, 214},
	{104, 442, 78, 323, 54, 195, 55, 198},
	{101, 435, 73, 308, 48, 179, 48, 182},
	{98, 427, 68, 294, 41, 163, 42, 166},
	{95, 420, 64, 279, 35, 147, 36, 151},
	{92, 412, 59, 264, 29, 131, 30, 135},
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
