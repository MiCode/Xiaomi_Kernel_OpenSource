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
unsigned int cpu_power_ratio_up[CM_MGR_EMI_OPP] = {120, 120, 100, 100};
unsigned int cpu_power_ratio_down[CM_MGR_EMI_OPP] = {120, 120, 40, 60};
unsigned int vcore_power_ratio_up[CM_MGR_EMI_OPP] = {100, 100, 100, 100};
unsigned int vcore_power_ratio_down[CM_MGR_EMI_OPP] = {100, 100, 100, 100};
unsigned int debounce_times_up_adb[CM_MGR_EMI_OPP] = {0, 0, 0, 0};
unsigned int debounce_times_down_adb[CM_MGR_EMI_OPP] = {3, 0, 0, 0};
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
	{3, 29, 2, 22, 1, 13, 2, 15},
	{6, 58, 4, 44, 3, 26, 3, 31},
	{83, 482, 6, 65, 4, 39, 5, 46},
	{82, 488, 79, 459, 5, 51, 6, 62},
	{80, 494, 77, 457, 6, 64, 74, 426},
	{79, 500, 74, 456, 69, 402, 71, 418},
	{77, 506, 72, 454, 66, 392, 68, 410},
	{76, 512, 70, 453, 63, 382, 65, 402},
	{74, 518, 68, 451, 60, 371, 62, 395},
	{73, 524, 65, 450, 57, 361, 59, 387},
	{71, 530, 63, 448, 53, 351, 56, 379},
	{69, 536, 61, 447, 50, 340, 53, 371},
	{68, 542, 58, 445, 47, 330, 50, 364},
	{66, 548, 56, 444, 44, 319, 48, 356},
	{65, 554, 54, 442, 41, 309, 45, 348},
	{63, 559, 52, 441, 38, 299, 42, 340},
	{62, 565, 49, 439, 35, 288, 39, 333},
	{60, 571, 47, 438, 31, 278, 36, 325},
	{59, 577, 45, 437, 28, 268, 33, 317},
	{57, 583, 43, 435, 25, 257, 30, 309},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 44, 3, 29, 1, 15, 2, 19},
	{90, 512, 6, 58, 3, 30, 4, 38},
	{90, 532, 85, 488, 4, 45, 6, 57},
	{89, 553, 84, 493, 6, 60, 80, 417},
	{89, 573, 82, 499, 75, 396, 77, 414},
	{89, 594, 80, 504, 72, 389, 74, 412},
	{89, 614, 79, 510, 69, 383, 72, 409},
	{89, 635, 77, 515, 66, 377, 69, 407},
	{88, 655, 75, 521, 63, 371, 66, 404},
	{88, 676, 74, 526, 60, 364, 64, 402},
	{88, 696, 72, 532, 57, 358, 61, 399},
	{88, 716, 70, 537, 54, 352, 58, 397},
	{88, 737, 69, 543, 51, 346, 56, 394},
	{87, 757, 67, 548, 48, 339, 53, 392},
	{87, 778, 65, 554, 45, 333, 50, 389},
	{87, 798, 64, 559, 42, 327, 48, 387},
	{87, 819, 62, 565, 39, 321, 45, 384},
	{87, 839, 60, 570, 36, 315, 42, 382},
	{86, 860, 59, 576, 33, 308, 40, 379},
	{86, 880, 57, 581, 30, 302, 37, 377},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 29, 2, 22, 1, 13, 2, 15},
	{6, 58, 4, 44, 3, 26, 3, 31},
	{9, 87, 6, 65, 4, 39, 5, 46},
	{11, 117, 9, 87, 5, 51, 6, 62},
	{147, 616, 11, 109, 6, 64, 8, 77},
	{141, 614, 13, 131, 8, 77, 9, 93},
	{135, 612, 130, 560, 9, 90, 11, 108},
	{129, 610, 124, 550, 10, 103, 12, 124},
	{123, 607, 117, 541, 11, 116, 14, 139},
	{117, 605, 110, 531, 13, 129, 104, 468},
	{111, 603, 103, 522, 94, 424, 97, 452},
	{105, 601, 97, 512, 86, 405, 89, 437},
	{99, 599, 90, 502, 79, 387, 82, 421},
	{93, 596, 83, 493, 71, 368, 74, 405},
	{87, 594, 76, 483, 63, 350, 67, 389},
	{81, 592, 70, 473, 56, 331, 60, 373},
	{75, 590, 63, 464, 48, 313, 52, 357},
	{69, 588, 56, 454, 40, 294, 45, 341},
	{63, 585, 49, 445, 33, 276, 38, 325},
	{57, 583, 43, 435, 25, 257, 30, 309},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 44, 3, 29, 1, 15, 2, 19},
	{9, 88, 6, 58, 3, 30, 4, 38},
	{13, 132, 9, 87, 4, 45, 6, 57},
	{142, 783, 11, 116, 6, 60, 7, 75},
	{138, 789, 131, 715, 7, 76, 9, 94},
	{135, 795, 126, 706, 9, 91, 11, 113},
	{131, 801, 121, 697, 10, 106, 13, 132},
	{128, 807, 116, 688, 12, 121, 108, 667},
	{124, 813, 111, 679, 13, 136, 102, 643},
	{121, 820, 106, 670, 93, 581, 96, 619},
	{117, 826, 101, 661, 86, 553, 90, 594},
	{114, 832, 96, 652, 80, 525, 84, 570},
	{111, 838, 92, 644, 74, 498, 78, 546},
	{107, 844, 87, 635, 67, 470, 73, 522},
	{104, 850, 82, 626, 61, 442, 67, 498},
	{100, 856, 77, 617, 55, 414, 61, 473},
	{97, 862, 72, 608, 49, 386, 55, 449},
	{93, 868, 67, 599, 42, 358, 49, 425},
	{90, 874, 62, 590, 36, 330, 43, 401},
	{86, 880, 57, 581, 30, 302, 37, 377},
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
