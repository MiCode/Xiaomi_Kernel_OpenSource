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
#ifdef USE_BCPU_WEIGHT
int cpu_power_bcpu_weight_max = 350;
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
#ifdef USE_CPU_TO_DRAM_MAP_NEW
int cm_mgr_cpu_map_emi_opp = 1;
int cm_mgr_cpu_map_skip_cpu_opp = 2;
#endif /* USE_CPU_TO_DRAM_MAP_NEW */

static int vcore_power_gain_0[][VCORE_ARRAY_SIZE] = {
	{28, 69, 1, 100, 189},
	{43, 104, 1, 144, 297},
	{50, 112, 16, 189, 363},
	{61, 116, 12, 205, 393},
	{63, 123, 19, 219, 406},
	{65, 130, 22, 223, 432},
	{79, 140, 20, 233, 445},
	{93, 149, 18, 241, 461},
	{107, 153, 25, 242, 477},
	{122, 154, 37, 241, 483},
	{130, 163, 43, 243, 489},
	{130, 173, 52, 253, 493},
	{130, 178, 61, 270, 496},
	{130, 182, 71, 269, 515},
	{130, 182, 79, 272, 535},
	{130, 182, 87, 276, 554},
	{130, 182, 95, 280, 565},
	{130, 182, 103, 284, 577},
	{130, 182, 103, 296, 589},
	{130, 182, 103, 308, 593},
	{130, 182, 103, 320, 597},
	{130, 182, 103, 332, 602},
	{130, 182, 103, 343, 606},
	{130, 182, 103, 355, 611},
	{130, 182, 103, 355, 627},
	{130, 182, 103, 355, 644},
	{130, 182, 103, 355, 660},
	{130, 182, 103, 355, 677},
	{130, 182, 103, 355, 693},
	{130, 182, 103, 355, 710},
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
	{3, 24, 0, 4, 1, 8, 1, 7, 1, 7},
	{6, 48, 1, 8, 2, 15, 2, 14, 2, 14},
	{108, 441, 1, 12, 3, 23, 3, 22, 3, 21},
	{105, 443, 2, 16, 4, 30, 3, 29, 3, 29},
	{102, 445, 2, 20, 5, 38, 4, 36, 4, 36},
	{99, 448, 3, 24, 6, 46, 5, 43, 5, 43},
	{96, 450, 3, 28, 6, 53, 6, 50, 6, 50},
	{93, 452, 4, 33, 78, 322, 77, 318, 77, 318},
	{90, 454, 4, 37, 73, 307, 72, 304, 72, 304},
	{87, 456, 5, 41, 68, 293, 67, 289, 67, 289},
	{84, 458, 5, 45, 63, 279, 62, 275, 62, 274},
	{82, 460, 6, 49, 58, 265, 57, 260, 57, 260},
	{79, 463, 6, 53, 53, 251, 52, 245, 52, 245},
	{76, 465, 42, 187, 48, 237, 47, 231, 47, 231},
	{73, 467, 37, 170, 43, 222, 42, 216, 42, 216},
	{70, 469, 31, 152, 38, 208, 37, 202, 37, 201},
	{67, 471, 26, 134, 33, 194, 32, 187, 32, 187},
	{64, 473, 21, 117, 28, 180, 27, 173, 27, 172},
	{61, 476, 15, 99, 23, 166, 22, 158, 22, 158},
	{58, 478, 10, 81, 18, 152, 17, 144, 17, 143},
};

static unsigned int cpu_power_gain_DownLow0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 35, 1, 4, 1, 8, 1, 8, 1, 8},
	{131, 441, 1, 9, 2, 17, 2, 16, 2, 16},
	{128, 455, 2, 13, 3, 25, 3, 24, 3, 24},
	{125, 469, 2, 17, 4, 34, 4, 32, 4, 32},
	{123, 483, 3, 21, 5, 42, 5, 40, 5, 39},
	{120, 497, 3, 26, 6, 50, 6, 48, 6, 47},
	{118, 511, 4, 30, 95, 327, 7, 55, 7, 55},
	{115, 525, 4, 34, 90, 315, 89, 311, 89, 311},
	{112, 539, 5, 39, 84, 303, 83, 298, 83, 298},
	{110, 553, 5, 43, 78, 291, 77, 286, 77, 285},
	{107, 567, 6, 47, 72, 278, 72, 273, 72, 273},
	{105, 581, 6, 52, 67, 266, 66, 260, 66, 260},
	{102, 595, 7, 56, 61, 254, 60, 247, 60, 247},
	{100, 609, 48, 184, 55, 242, 54, 235, 54, 234},
	{97, 623, 42, 168, 49, 229, 48, 222, 48, 222},
	{94, 637, 35, 151, 43, 217, 42, 209, 42, 209},
	{92, 651, 29, 135, 38, 205, 37, 197, 37, 196},
	{89, 665, 23, 119, 32, 193, 31, 184, 31, 183},
	{87, 679, 17, 102, 26, 181, 25, 171, 25, 171},
	{84, 693, 10, 86, 20, 168, 19, 158, 19, 158},
};

static unsigned int cpu_power_gain_UpHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{3, 24, 0, 4, 1, 8, 1, 7, 1, 7},
	{6, 48, 1, 8, 2, 15, 2, 14, 2, 14},
	{9, 72, 1, 12, 3, 23, 3, 22, 3, 21},
	{12, 96, 2, 16, 4, 30, 3, 29, 3, 29},
	{14, 119, 2, 20, 5, 38, 4, 36, 4, 36},
	{123, 717, 3, 24, 6, 46, 5, 43, 5, 43},
	{118, 700, 3, 28, 6, 53, 6, 50, 6, 50},
	{114, 683, 4, 33, 7, 61, 7, 57, 7, 57},
	{109, 666, 4, 37, 8, 68, 8, 65, 8, 64},
	{104, 649, 5, 41, 9, 76, 9, 72, 9, 72},
	{100, 632, 5, 45, 10, 83, 10, 79, 10, 79},
	{95, 615, 6, 49, 11, 91, 10, 86, 10, 86},
	{90, 597, 6, 53, 12, 99, 11, 93, 11, 93},
	{86, 580, 7, 57, 13, 106, 12, 100, 12, 100},
	{81, 563, 7, 61, 14, 114, 13, 108, 13, 107},
	{76, 546, 8, 65, 15, 121, 14, 115, 14, 115},
	{72, 529, 8, 69, 16, 129, 15, 122, 15, 122},
	{67, 512, 9, 73, 17, 137, 16, 129, 16, 129},
	{62, 495, 9, 77, 25, 185, 17, 136, 16, 136},
	{58, 478, 10, 81, 18, 152, 17, 144, 17, 143},
};

static unsigned int cpu_power_gain_DownHigh0[][CM_MGR_CPU_ARRAY_SIZE] = {
	{4, 35, 1, 4, 1, 8, 1, 8, 1, 8},
	{8, 69, 1, 9, 2, 17, 2, 16, 2, 16},
	{13, 104, 2, 13, 3, 25, 3, 24, 3, 24},
	{152, 866, 2, 17, 4, 34, 4, 32, 4, 32},
	{148, 855, 3, 21, 5, 42, 5, 40, 5, 39},
	{144, 844, 3, 26, 6, 50, 6, 48, 6, 47},
	{139, 834, 4, 30, 7, 59, 7, 55, 7, 55},
	{135, 823, 4, 34, 8, 67, 8, 63, 8, 63},
	{131, 812, 5, 39, 9, 76, 9, 71, 9, 71},
	{127, 801, 5, 43, 10, 84, 10, 79, 10, 79},
	{122, 790, 6, 47, 11, 93, 11, 87, 11, 87},
	{118, 780, 6, 52, 12, 101, 12, 95, 11, 95},
	{114, 769, 7, 56, 13, 109, 12, 103, 12, 103},
	{110, 758, 7, 60, 14, 118, 13, 111, 13, 111},
	{105, 747, 8, 64, 15, 126, 14, 119, 14, 118},
	{101, 736, 8, 69, 16, 135, 15, 127, 15, 126},
	{97, 726, 9, 73, 43, 279, 16, 135, 16, 134},
	{93, 715, 9, 77, 35, 242, 34, 233, 34, 233},
	{88, 704, 10, 82, 28, 205, 27, 196, 27, 195},
	{84, 693, 10, 86, 20, 168, 19, 158, 19, 158},
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
