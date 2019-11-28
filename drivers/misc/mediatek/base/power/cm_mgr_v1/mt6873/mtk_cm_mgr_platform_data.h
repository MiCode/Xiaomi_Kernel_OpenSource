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
int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {140, 140, 100, 100, 100};
int cpu_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100};
int vcore_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100};
int vcore_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100};
int debounce_times_up_adb[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 0};
int debounce_times_down_adb[CM_MGR_EMI_OPP] = {3, 0, 0, 0, 0};
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
	{138, 87, 124, 296, 518},
	{140, 92, 123, 302, 527},
	{143, 97, 123, 308, 536},
	{145, 102, 122, 314, 545},
	{148, 107, 121, 321, 555},
	{150, 111, 121, 327, 564},
	{153, 116, 120, 333, 573},
	{155, 121, 119, 339, 582},
	{158, 126, 119, 345, 592},
	{160, 131, 118, 351, 601},
	{163, 136, 117, 357, 610},
	{165, 140, 117, 364, 619},
	{168, 145, 116, 370, 628},
	{170, 150, 116, 376, 638},
	{173, 155, 115, 382, 647},
	{176, 160, 114, 388, 656},
	{178, 165, 114, 394, 665},
	{181, 170, 113, 400, 674},
	{183, 174, 112, 406, 684},
	{186, 179, 112, 413, 693},
	{188, 184, 111, 419, 702},
	{191, 189, 110, 425, 711},
	{193, 194, 110, 431, 721},
	{196, 199, 109, 437, 730},
	{198, 203, 108, 443, 739},
	{201, 208, 108, 449, 748},
	{203, 213, 107, 456, 757},
	{206, 218, 106, 462, 767},
	{208, 223, 106, 468, 776},
	{211, 228, 105, 474, 785},
	{213, 232, 105, 480, 794},
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
	{3, 21, 7, 44, 58, 335, 10, 61, 9, 58},
	{4, 24, 8, 49, 56, 320, 55, 318, 55, 314},
	{4, 26, 9, 55, 53, 305, 53, 303, 52, 299},
	{5, 29, 9, 60, 50, 290, 50, 288, 49, 283},
	{5, 31, 43, 247, 47, 276, 47, 273, 46, 268},
	{5, 34, 40, 230, 44, 261, 44, 258, 43, 252},
	{6, 37, 36, 213, 42, 246, 41, 243, 40, 237},
	{6, 39, 33, 196, 39, 231, 38, 228, 37, 221},
	{7, 42, 30, 178, 36, 216, 35, 213, 34, 206},
	{7, 45, 27, 161, 33, 202, 33, 198, 31, 191},
	{7, 47, 24, 144, 30, 187, 30, 183, 28, 175},
	{8, 50, 20, 127, 27, 172, 27, 168, 25, 160},
	{8, 52, 17, 110, 25, 157, 24, 153, 23, 144},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{0, 3, 1, 6, 1, 9, 1, 9, 1, 8},
	{1, 5, 2, 12, 3, 18, 3, 18, 3, 16},
	{1, 8, 3, 18, 4, 27, 4, 26, 4, 25},
	{2, 11, 4, 24, 6, 36, 6, 35, 5, 33},
	{2, 14, 5, 30, 7, 45, 7, 44, 6, 41},
	{3, 16, 6, 36, 9, 54, 8, 53, 8, 49},
	{3, 19, 7, 42, 67, 394, 10, 62, 9, 57},
	{3, 22, 8, 48, 64, 378, 64, 376, 63, 371},
	{4, 25, 9, 54, 61, 362, 61, 359, 60, 354},
	{4, 27, 9, 60, 58, 345, 58, 343, 57, 337},
	{5, 30, 50, 296, 55, 329, 55, 326, 54, 319},
	{5, 33, 47, 276, 52, 312, 52, 309, 51, 302},
	{6, 36, 43, 257, 49, 296, 49, 293, 48, 285},
	{6, 38, 40, 237, 46, 280, 46, 276, 44, 268},
	{6, 41, 36, 218, 43, 263, 43, 259, 41, 250},
	{7, 44, 33, 199, 40, 247, 40, 243, 38, 233},
	{7, 47, 29, 179, 37, 230, 37, 226, 35, 216},
	{8, 49, 26, 160, 34, 214, 34, 210, 32, 199},
	{8, 52, 22, 140, 31, 198, 31, 193, 29, 181},
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
	{5, 31, 10, 66, 15, 94, 14, 92, 14, 87},
	{5, 34, 11, 71, 65, 292, 65, 289, 15, 94},
	{6, 37, 12, 77, 60, 272, 59, 270, 58, 263},
	{6, 39, 13, 82, 54, 253, 53, 250, 52, 243},
	{7, 42, 14, 88, 48, 234, 47, 231, 46, 224},
	{7, 45, 15, 93, 42, 215, 42, 212, 40, 204},
	{7, 47, 30, 153, 36, 196, 36, 192, 34, 184},
	{8, 50, 23, 131, 30, 176, 30, 173, 28, 164},
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
	{5, 30, 10, 67, 90, 382, 89, 380, 14, 90},
	{5, 33, 11, 73, 83, 360, 83, 357, 81, 350},
	{6, 36, 12, 79, 76, 338, 76, 334, 74, 327},
	{6, 38, 13, 85, 69, 315, 69, 312, 67, 303},
	{6, 41, 14, 91, 63, 293, 62, 289, 61, 280},
	{7, 44, 48, 222, 56, 271, 55, 267, 54, 257},
	{7, 47, 41, 197, 49, 248, 48, 244, 47, 234},
	{8, 49, 34, 172, 42, 226, 41, 221, 40, 211},
	{8, 52, 26, 146, 35, 204, 34, 199, 33, 187},
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
