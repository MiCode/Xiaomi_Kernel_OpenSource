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
#ifdef USE_BCPU_WEIGHT
int cpu_power_bcpu_weight_max = 300;
int cpu_power_bcpu_weight_min = 100;
#endif /* USE_BCPU_WEIGHT */
unsigned int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {140, 100, 140, 100, 140};
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
int cm_mgr_cpu_map_dram_enable = 1;
#endif /* USE_CPU_TO_DRAM_MAP */

static int vcore_power_gain_0[][VCORE_ARRAY_SIZE] = {
	{2, 6, 40, 84, 121},
	{28, 36, 47, 150, 224},
	{45, 41, 61, 179, 287},
	{55, 44, 69, 185, 338},
	{67, 50, 69, 189, 358},
	{80, 55, 71, 193, 377},
	{87, 69, 72, 195, 396},
	{111, 78, 80, 197, 415},
	{135, 81, 93, 199, 433},
	{155, 89, 94, 211, 446},
	{174, 98, 93, 225, 457},
	{193, 103, 94, 239, 468},
	{213, 106, 99, 253, 479},
	{213, 129, 101, 267, 490},
	{213, 152, 102, 272, 511},
	{213, 152, 125, 274, 536},
	{213, 152, 149, 275, 560},
	{213, 152, 171, 274, 584},
	{213, 152, 194, 274, 608},
	{213, 152, 194, 297, 624},
	{213, 152, 194, 320, 630},
	{213, 152, 194, 343, 635},
	{213, 152, 194, 366, 641},
	{213, 152, 194, 390, 646},
	{213, 152, 194, 414, 651},
	{213, 152, 194, 414, 681},
	{213, 152, 194, 414, 711},
	{213, 152, 194, 414, 740},
	{213, 152, 194, 414, 770},
	{213, 152, 194, 414, 799},
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
	{0, 3, 1, 5, 1, 8, 1, 8, 1, 7},
	{1, 5, 2, 11, 2, 16, 2, 15, 2, 14},
	{1, 8, 3, 16, 4, 24, 4, 23, 3, 22},
	{2, 10, 3, 22, 5, 31, 5, 31, 5, 29},
	{2, 13, 4, 27, 6, 39, 6, 38, 6, 36},
	{2, 16, 5, 33, 7, 47, 7, 46, 7, 43},
	{3, 18, 6, 38, 9, 55, 8, 54, 8, 50},
	{3, 21, 7, 44, 55, 359, 10, 61, 9, 58},
	{4, 24, 8, 49, 52, 343, 52, 341, 51, 337},
	{4, 26, 9, 55, 50, 326, 49, 324, 49, 319},
	{5, 29, 9, 60, 47, 309, 47, 307, 46, 302},
	{5, 31, 40, 264, 45, 292, 44, 290, 44, 284},
	{5, 34, 37, 244, 42, 275, 42, 273, 41, 267},
	{6, 37, 35, 225, 40, 258, 39, 256, 38, 249},
	{6, 39, 32, 206, 37, 242, 37, 239, 36, 232},
	{7, 42, 29, 187, 35, 225, 34, 222, 33, 214},
	{7, 45, 26, 167, 32, 208, 32, 205, 30, 197},
	{7, 47, 23, 148, 30, 191, 29, 188, 28, 179},
	{8, 50, 20, 129, 27, 174, 27, 170, 25, 162},
	{8, 52, 17, 110, 25, 157, 24, 153, 23, 144},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{0, 3, 1, 6, 1, 9, 1, 9, 1, 8},
	{1, 5, 2, 12, 3, 18, 3, 18, 3, 16},
	{1, 8, 3, 18, 4, 27, 4, 26, 4, 25},
	{2, 11, 4, 24, 6, 36, 6, 35, 5, 33},
	{2, 14, 5, 30, 7, 45, 7, 44, 6, 41},
	{3, 16, 6, 36, 9, 54, 8, 53, 8, 49},
	{3, 19, 7, 42, 67, 404, 10, 62, 9, 57},
	{3, 22, 8, 48, 64, 387, 64, 385, 63, 380},
	{4, 25, 9, 54, 61, 369, 61, 367, 60, 362},
	{4, 27, 9, 60, 58, 352, 58, 350, 57, 344},
	{5, 30, 50, 302, 55, 335, 55, 332, 54, 326},
	{5, 33, 47, 282, 52, 318, 52, 315, 51, 308},
	{6, 36, 43, 262, 49, 301, 49, 298, 48, 290},
	{6, 38, 40, 242, 46, 284, 46, 280, 44, 272},
	{6, 41, 36, 222, 43, 267, 43, 263, 41, 254},
	{7, 44, 33, 201, 40, 250, 40, 246, 38, 236},
	{7, 47, 29, 181, 37, 233, 37, 228, 35, 218},
	{8, 49, 26, 161, 34, 216, 34, 211, 32, 200},
	{8, 52, 22, 141, 31, 198, 31, 194, 29, 182},
	{9, 55, 19, 121, 28, 181, 28, 176, 26, 164},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{0, 3, 1, 5, 1, 8, 1, 8, 1, 7},
	{1, 5, 2, 11, 2, 16, 2, 15, 2, 14},
	{1, 8, 3, 16, 4, 24, 4, 23, 3, 22},
	{2, 10, 3, 22, 5, 31, 5, 31, 5, 29},
	{2, 13, 4, 27, 6, 39, 6, 38, 6, 36},
	{2, 16, 5, 33, 7, 47, 7, 46, 7, 43},
	{3, 18, 6, 38, 9, 55, 8, 54, 8, 50},
	{3, 21, 7, 44, 10, 63, 10, 61, 9, 58},
	{4, 24, 8, 49, 11, 71, 11, 69, 10, 65},
	{4, 26, 9, 55, 12, 79, 12, 77, 11, 72},
	{5, 29, 9, 60, 14, 86, 13, 84, 12, 79},
	{5, 31, 10, 66, 82, 295, 14, 92, 14, 87},
	{5, 34, 11, 71, 75, 277, 74, 275, 15, 94},
	{6, 37, 12, 77, 67, 260, 67, 258, 66, 251},
	{6, 39, 13, 82, 60, 243, 60, 240, 59, 233},
	{7, 42, 14, 88, 53, 226, 53, 223, 52, 215},
	{7, 45, 15, 93, 46, 209, 46, 205, 44, 198},
	{7, 47, 32, 149, 39, 192, 38, 188, 37, 180},
	{8, 50, 25, 129, 32, 174, 31, 171, 30, 162},
	{8, 52, 17, 110, 25, 157, 24, 153, 23, 144},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{0, 3, 1, 6, 1, 9, 1, 9, 1, 8},
	{1, 5, 2, 12, 3, 18, 3, 18, 3, 16},
	{1, 8, 3, 18, 4, 27, 4, 26, 4, 25},
	{2, 11, 4, 24, 6, 36, 6, 35, 5, 33},
	{2, 14, 5, 30, 7, 45, 7, 44, 6, 41},
	{3, 16, 6, 36, 9, 54, 8, 53, 8, 49},
	{3, 19, 7, 42, 10, 63, 10, 62, 9, 57},
	{3, 22, 8, 48, 11, 73, 11, 70, 10, 66},
	{4, 25, 9, 54, 13, 82, 12, 79, 12, 74},
	{4, 27, 9, 60, 14, 91, 14, 88, 13, 82},
	{5, 30, 10, 67, 90, 344, 90, 341, 14, 90},
	{5, 33, 11, 73, 83, 326, 83, 323, 82, 316},
	{6, 36, 12, 79, 76, 308, 76, 304, 75, 297},
	{6, 38, 13, 85, 70, 290, 69, 286, 68, 278},
	{6, 41, 14, 91, 63, 272, 62, 268, 61, 259},
	{7, 44, 48, 205, 56, 254, 55, 250, 54, 240},
	{7, 47, 41, 184, 49, 236, 48, 231, 47, 221},
	{8, 49, 34, 163, 42, 217, 41, 213, 40, 202},
	{8, 52, 26, 142, 35, 199, 34, 195, 33, 183},
	{9, 55, 19, 121, 28, 181, 28, 176, 26, 164},
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
