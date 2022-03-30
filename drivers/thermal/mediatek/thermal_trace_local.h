/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __THERMAL_TRACE_LOCAL_H__
#define __THERMAL_TRACE_LOCAL_H__

/* SRAM OFFSET */
#define CPU_TTJ_OFFSET (0x100)
#define GPU_TTJ_OFFSET (0x104)
#define APU_TTJ_OFFSET (0x108)
#define CPU_POWERBUDGET_OFFSET (0x110)
#define GPU_POWERBUDGET_OFFSET (0x114)
#define APU_POWERBUDGET_OFFSET (0x118)
#define CPU_LL_MIN_OPP_HINT_OFFSET (0x120)
#define CPU_BL_MIN_OPP_HINT_OFFSET (0x124)
#define CPU_B_MIN_OPP_HINT_OFFSET (0x128)
#define CPU_LL_LIMIT_FREQ_OFFSET (0x200)
#define CPU_BL_LIMIT_FREQ_OFFSET (0x204)
#define CPU_B_LIMIT_FREQ_OFFSET (0x208)
#define CPU_LL_CUR_FREQ_OFFSET (0x210)
#define CPU_BL_CUR_FREQ_OFFSET (0x214)
#define CPU_B_CUR_FREQ_OFFSET (0x218)
#define CPU_LL_MAX_TEMP_OFFSET (0x220)
#define CPU_BL_MAX_TEMP_OFFSET (0x224)
#define CPU_B_MAX_TEMP_OFFSET (0x228)
#define CPU_LL_LIMIT_OPP_OFFSET (0x260)
#define CPU_BL_LIMIT_OPP_OFFSET (0x264)
#define CPU_B_LIMIT_OPP_OFFSET (0x268)
#define GPU_TEMP_OFFSET (0x180)
#define GPU_LIMIT_FREQ_OFFSET (0x184)
#define GPU_CUR_FREQ_OFFSET (0x188)
#define APU_TEMP_OFFSET (0x190)
#define APU_LIMIT_OPP_OFFSET (0x194)
#define APU_CUR_OPP_OFFSET (0x198)

#define APUMBOX_TTJ_OFFSET (0x700)
#define APUMBOX_POWERBUDGET_OFFSET (0x704)
#define APUMBOX_TEMP_OFFSET (0x708)
#define APUMBOX_LIMIT_OPP_OFFSET (0x70C)
#define APUMBOX_CUR_OPP_OFFSET (0x710)

struct thermal_cpu_info {
	int ttj;
	int limit_powerbudget;
	int LL_min_opp_hint;
	unsigned int LL_cur_freq;
	unsigned int LL_limit_freq;
	int LL_limit_opp;
	int LL_max_temp;
	int BL_min_opp_hint;
	unsigned int BL_cur_freq;
	unsigned int BL_limit_freq;
	int BL_limit_opp;
	int BL_max_temp;
	int B_min_opp_hint;
	unsigned int B_cur_freq;
	unsigned int B_limit_freq;
	int B_limit_opp;
	int B_max_temp;
};

struct thermal_gpu_info {
	int ttj;
	int limit_powerbudget;
	int temp;
	unsigned int limit_freq;
	unsigned int cur_freq;
};

struct thermal_apu_info {
	int ttj;
	int limit_powerbudget;
	int temp;
	int limit_opp;
	int cur_opp;
};

#endif
