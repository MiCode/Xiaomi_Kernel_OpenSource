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
#ifdef USE_CM_USER_MODE
unsigned int cm_user_mode;
unsigned int cm_user_active;
#endif
#ifdef USE_BCPU_WEIGHT
int cpu_power_bcpu_weight_max = 350;
int cpu_power_bcpu_weight_min = 100;

int cpu_power_bcpu_weight_max0 = 350;
int cpu_power_bcpu_weight_min0 = 100;

int cpu_power_bcpu_weight_max1 = 480;
int cpu_power_bcpu_weight_min1 = 100;

#endif /* USE_BCPU_WEIGHT */

#ifdef CM_BCPU_MIN_OPP_WEIGHT
unsigned int cm_mgr_bcpu_min_opp_weight;
unsigned int cm_mgr_bcpu_low_opp_weight;
unsigned int cm_mgr_bcpu_low_opp_bound = 15;

unsigned int cm_mgr_bcpu_min_opp_weight0;
unsigned int cm_mgr_bcpu_low_opp_weight0;
unsigned int cm_mgr_bcpu_low_opp_bound0 = 15;

unsigned int cm_mgr_bcpu_min_opp_weight1 = 30;
unsigned int cm_mgr_bcpu_low_opp_weight1 = 60;
unsigned int cm_mgr_bcpu_low_opp_bound1 = 10;
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
unsigned int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {100, 120, 140, 100, 100, 0};
unsigned int cpu_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100, 0};
unsigned int vcore_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100, 100};
unsigned int vcore_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100, 100};
unsigned int debounce_times_up_adb[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 0, 0};
unsigned int debounce_times_down_adb[CM_MGR_EMI_OPP] = {3, 0, 0, 0, 0, 0};

unsigned int cpu_power_ratio_up0[CM_MGR_EMI_OPP] = {100, 120, 140, 100, 100, 0};
unsigned int cpu_power_ratio_down0[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 100, 0};
unsigned int debounce_times_up_adb0[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 0, 0};
unsigned int debounce_times_down_adb0[CM_MGR_EMI_OPP] = {3, 0, 0, 0, 0, 0};

unsigned int cpu_power_ratio_up1[CM_MGR_EMI_OPP] = {100, 120, 140, 70, 180, 140};
unsigned int cpu_power_ratio_down1[CM_MGR_EMI_OPP] = {100, 100, 100, 100, 160, 100};
unsigned int debounce_times_up_adb1[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 0, 0};
unsigned int debounce_times_down_adb1[CM_MGR_EMI_OPP] = {3, 0, 0, 0, 3, 3};

int debounce_times_reset_adb = 1;
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
#ifdef USE_CPU_TO_DRAM_MAP_NEW
int cm_mgr_cpu_map_emi_opp = 1;
int cm_mgr_cpu_map_skip_cpu_opp = 2;
#endif /* USE_CPU_TO_DRAM_MAP_NEW */


int x_ratio_enable = 1;
int cm_mgr_camera_enable;
unsigned int cpu_power_ratio_up_x_camera[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 60, 60};
unsigned int cpu_power_ratio_up_x[CM_MGR_EMI_OPP] = {0, 0, 0, 0, 0, 0};


static int vcore_power_gain_0[][VCORE_ARRAY_SIZE] = {
	{42, 80, 10, 120, 210},
	{85, 124, 33, 157, 358},
	{104, 137, 57, 178, 549},
	{100, 172, 56, 174, 571},
	{110, 174, 70, 181, 575},
	{132, 175, 82, 173, 599},
	{189, 170, 92, 171, 631},
	{221, 191, 102, 162, 668},
	{254, 213, 99, 161, 706},
	{282, 232, 95, 169, 714},
	{301, 243, 117, 168, 722},
	{319, 253, 125, 168, 745},
	{350, 251, 119, 174, 775},
	{350, 280, 112, 192, 793},
	{350, 309, 114, 202, 807},
	{350, 309, 147, 207, 828},
	{350, 309, 179, 208, 865},
	{350, 309, 211, 209, 902},
	{350, 309, 243, 210, 936},
	{350, 309, 243, 239, 947},
	{350, 309, 243, 268, 958},
	{350, 309, 243, 297, 969},
	{350, 309, 243, 326, 980},
	{350, 309, 243, 355, 992},
	{350, 309, 243, 355, 1032},
	{350, 309, 243, 355, 1072},
	{350, 309, 243, 355, 1113},
	{350, 309, 243, 355, 1153},
	{350, 309, 243, 355, 1193},
	{350, 309, 243, 355, 1234},
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
	{2, 13, 1, 4, 1, 8, 1, 7, 1, 8},
	{3, 26, 1, 8, 2, 15, 2, 13, 2, 15},
	{5, 39, 2, 12, 3, 23, 3, 20, 3, 23},
	{7, 52, 2, 16, 4, 30, 4, 27, 3, 31},
	{155, 362, 3, 20, 5, 38, 5, 33, 4, 38},
	{147, 355, 3, 24, 6, 45, 6, 40, 5, 46},
	{139, 349, 4, 28, 7, 53, 7, 47, 6, 53},
	{131, 342, 4, 32, 78, 298, 78, 53, 7, 299},
	{123, 335, 5, 36, 73, 285, 73, 278, 72, 286},
	{114, 328, 5, 40, 69, 273, 69, 265, 67, 274},
	{106, 322, 6, 44, 64, 261, 64, 251, 62, 262},
	{98, 315, 6, 47, 59, 249, 59, 238, 57, 250},
	{90, 308, 7, 51, 54, 236, 54, 225, 52, 238},
	{82, 301, 7, 55, 49, 224, 49, 212, 47, 226},
	{74, 295, 37, 158, 44, 212, 44, 199, 42, 213},
	{66, 288, 32, 142, 39, 200, 39, 186, 37, 201},
	{58, 281, 26, 127, 34, 187, 34, 173, 32, 189},
	{49, 274, 21, 111, 29, 175, 29, 159, 27, 177},
	{41, 268, 15, 95, 24, 163, 24, 146, 22, 165},
	{33, 261, 10, 79, 19, 151, 19, 133, 17, 153},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 16, 1, 4, 1, 8, 1, 7, 1, 8},
	{4, 31, 1, 8, 2, 17, 2, 15, 2, 17},
	{6, 47, 2, 12, 3, 25, 3, 22, 3, 25},
	{117, 410, 2, 17, 4, 33, 4, 29, 4, 34},
	{112, 404, 3, 21, 5, 42, 5, 36, 5, 42},
	{107, 398, 3, 25, 6, 50, 6, 44, 6, 51},
	{102, 392, 4, 29, 96, 341, 6, 51, 96, 342},
	{98, 386, 4, 33, 90, 327, 89, 319, 90, 328},
	{93, 380, 5, 37, 84, 314, 83, 304, 85, 315},
	{88, 374, 5, 42, 79, 300, 77, 290, 79, 302},
	{83, 368, 6, 46, 73, 287, 71, 275, 73, 289},
	{78, 361, 6, 50, 67, 274, 66, 261, 67, 275},
	{73, 355, 7, 54, 61, 260, 60, 247, 62, 262},
	{69, 349, 48, 189, 56, 247, 54, 232, 56, 249},
	{64, 343, 42, 171, 50, 233, 48, 218, 50, 235},
	{59, 337, 36, 154, 44, 220, 42, 203, 44, 222},
	{54, 331, 29, 136, 38, 207, 36, 189, 39, 209},
	{49, 325, 23, 118, 33, 193, 30, 174, 33, 196},
	{45, 319, 17, 101, 27, 180, 24, 160, 27, 182},
	{40, 313, 11, 83, 21, 166, 18, 145, 21, 169},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 13, 1, 4, 1, 8, 1, 7, 1, 8},
	{3, 26, 1, 8, 2, 15, 2, 13, 2, 15},
	{5, 39, 2, 12, 3, 23, 3, 20, 3, 23},
	{7, 52, 2, 16, 4, 30, 4, 27, 3, 31},
	{8, 65, 3, 20, 5, 38, 5, 33, 4, 38},
	{10, 78, 3, 24, 6, 45, 6, 40, 5, 46},
	{12, 91, 4, 28, 7, 53, 7, 47, 6, 53},
	{13, 104, 4, 32, 8, 60, 8, 53, 7, 61},
	{15, 117, 5, 36, 9, 68, 9, 60, 8, 69},
	{17, 130, 5, 40, 10, 75, 10, 67, 8, 76},
	{131, 526, 6, 44, 11, 83, 11, 73, 9, 84},
	{120, 497, 6, 47, 11, 90, 11, 80, 10, 92},
	{109, 467, 7, 51, 12, 98, 12, 87, 11, 99},
	{98, 438, 7, 55, 13, 105, 13, 93, 12, 107},
	{87, 408, 8, 59, 14, 113, 14, 100, 13, 115},
	{76, 379, 8, 63, 15, 120, 15, 106, 14, 122},
	{66, 349, 9, 67, 16, 128, 16, 113, 14, 130},
	{55, 320, 9, 71, 17, 135, 17, 120, 15, 137},
	{44, 290, 10, 75, 26, 186, 26, 126, 16, 188},
	{33, 261, 10, 79, 19, 151, 19, 133, 17, 153},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{2, 16, 1, 4, 1, 8, 1, 7, 1, 8},
	{4, 31, 1, 8, 2, 17, 2, 15, 2, 17},
	{6, 47, 2, 12, 3, 25, 3, 22, 3, 25},
	{8, 63, 2, 17, 4, 33, 4, 29, 4, 34},
	{10, 78, 3, 21, 5, 42, 5, 36, 5, 42},
	{12, 94, 3, 25, 6, 50, 6, 44, 6, 51},
	{14, 109, 4, 29, 7, 58, 6, 51, 8, 59},
	{16, 125, 4, 33, 8, 67, 7, 58, 9, 68},
	{114, 668, 5, 37, 10, 75, 8, 65, 10, 76},
	{107, 636, 5, 42, 11, 83, 9, 73, 11, 85},
	{101, 604, 6, 46, 12, 92, 10, 80, 12, 93},
	{94, 571, 6, 50, 13, 100, 11, 87, 13, 101},
	{87, 539, 7, 54, 14, 108, 12, 95, 14, 110},
	{80, 507, 7, 58, 15, 117, 13, 102, 15, 118},
	{74, 474, 8, 62, 16, 125, 14, 109, 16, 127},
	{67, 442, 8, 67, 17, 133, 15, 116, 17, 135},
	{60, 410, 9, 71, 44, 285, 16, 124, 45, 288},
	{53, 377, 10, 75, 37, 246, 17, 131, 37, 248},
	{46, 345, 10, 79, 29, 206, 18, 138, 29, 209},
	{40, 313, 11, 83, 21, 166, 18, 145, 21, 169},
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
