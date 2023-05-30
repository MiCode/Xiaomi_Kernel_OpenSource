// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_mt6835.c
 * @brief   GPU DVFS History log DB Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */

#include <linux/sched/clock.h>
#include <linux/string.h>
#include <linux/io.h>

/* GPUFREQ */
#include <gpufreq_v2.h>
#include <gpufreq_history_common.h>
#include <gpufreq_history_mt6835.h>
#include <gpuppm.h>
#include <gpufreq_common.h>

/**
 * ===============================================
 * Variable Definition
 * ===============================================
 */

static void __iomem *next_log_offs;
static void __iomem *start_log_offs;
static void __iomem *end_log_offs;

static enum gpufreq_history_state g_history_state;
static int g_history_target_opp_stack;
static int g_history_target_opp_top;

// power state history
static void __iomem *p_v_log_offs;
static void __iomem *p_next_log_offs;
static void __iomem *p_start_log_offs;
static void __iomem *p_end_log_offs;

static struct gpu_history_buck_info buck_info;
static struct gpu_history_mfg_info mfg_info;

/**
 * ===============================================
 * Common Function Definition
 * ===============================================
 */

/* API: set target oppidx */
void gpufreq_set_history_target_opp(enum gpufreq_target target, int oppidx)
{
	if (target == TARGET_STACK)
		g_history_target_opp_stack = oppidx;
	else
		g_history_target_opp_top = oppidx;
}

/* API: get target oppidx */
int gpufreq_get_history_target_opp(enum gpufreq_target target)
{
	if (target == TARGET_STACK)
		return g_history_target_opp_stack;
	else
		return g_history_target_opp_top;
}

/**
 * ===============================================
 * External Function Definition
 * ===============================================
 */

/* API: set power buck */
void __gpufreq_set_power_buck(enum gpu_history_buck_status target, unsigned int power_status)
{
	if (target == HISTORY_BUCK_VCORE)
		buck_info.buck_vcore = power_status;
	else if (target == HISTORY_BUCK_VSRAM)
		buck_info.buck_vsram = power_status;
	else if (target == HISTORY_BUCK_VTOP)
		buck_info.buck_vtop = power_status;
	else if (target == HISTORY_BUCK_VSTACK)
		buck_info.buck_vstack = power_status;
}

/* API: get power buck*/
unsigned int __gpufreq_get_power_buck(enum gpu_history_buck_status target)
{
	if (target == HISTORY_BUCK_VCORE)
		return buck_info.buck_vcore;
	else if (target == HISTORY_BUCK_VSRAM)
		return buck_info.buck_vsram;
	else if (target == HISTORY_BUCK_VTOP)
		return buck_info.buck_vtop;
	else
		return buck_info.buck_vstack;
}

/* API: set power mfg */
void __gpufreq_set_power_mfg(enum gpu_history_mfg_status target, unsigned int power_mfg)
{
	if (target == HISTORY_MFG_0)
		mfg_info.mfg_0 = power_mfg;
	else if (target == HISTORY_MFG_1)
		mfg_info.mfg_1 = power_mfg;
	else if (target == HISTORY_MFG_2)
		mfg_info.mfg_2 = power_mfg;
	else if (target == HISTORY_MFG_3)
		mfg_info.mfg_3 = power_mfg;
}

/* API: get power mfg*/
unsigned int __gpufreq_get_power_mfg(enum gpu_history_mfg_status target)
{
	if (target == HISTORY_MFG_0)
		return mfg_info.mfg_0;
	else if (target == HISTORY_MFG_1)
		return mfg_info.mfg_1;
	else if (target == HISTORY_MFG_2)
		return mfg_info.mfg_2;
	else if (target == HISTORY_MFG_3)
		return mfg_info.mfg_3;
	else
		return 0;
}

/***********************************************************************************
 *  Function Name      : __gpufreq_record_history_entry
 *  Inputs             : -
 *  Outputs            : -
 *  Returns            : -
 *  Description        : -
 ************************************************************************************/
void __gpufreq_record_history_entry(enum gpufreq_history_state history_state)
{
	struct gpu_dvfs_source gpu_db_top = {};
	struct gpu_dvfs_source gpu_db_stack = {};
	struct gpu_dvfs_limiter gpu_db_limiter = {};
	u64 time_s = 0;

	if (next_log_offs != 0) {
		time_s = sched_clock(); //64 bit
		/* gpu stack */
		gpu_db_top.cur_volt = __gpufreq_get_cur_vgpu();
		gpu_db_top.cur_oppidx = __gpufreq_get_cur_idx_gpu();
		gpu_db_top.target_oppidx = gpufreq_get_history_target_opp(TARGET_GPU);
		gpu_db_top.cur_vsram = __gpufreq_get_cur_vsram_gpu();
		gpu_db_top.cur_freq = __gpufreq_get_cur_fgpu()/1000;
		gpu_db_top.cur_vcore = __gpufreq_get_cur_vcore();

		gpu_db_limiter.floor_oppidx = gpuppm_get_floor();
		gpu_db_limiter.ceiling_oppidx = gpuppm_get_ceiling();
		gpu_db_limiter.c_limiter = gpuppm_get_c_limiter();
		gpu_db_limiter.f_limiter = gpuppm_get_f_limiter();

		/* gpu stack */
		gpu_db_stack.park_flag = (history_state << 2);


		// /* update current status to shared memory */
		writel((time_s >> 32) & 0xFFFFFFFF, next_log_offs);
		next_log_offs += 4;
		writel(time_s & 0xFFFFFFFF, next_log_offs);
		next_log_offs += 4;
		writel(((gpu_db_top.target_oppidx & 0x3F) << 26)|
			((gpu_db_top.cur_oppidx & 0x3F) << 20)|
			(gpu_db_top.cur_volt & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel(((gpu_db_top.cur_freq & 0xFFF) << 20)|
			(gpu_db_top.cur_vsram & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel((0xFF << 20)|
			(gpu_db_top.cur_vcore & 0xFFFFF), next_log_offs);
		next_log_offs += 4;

		writel(((gpu_db_stack.target_oppidx & 0x3F) << 26)|
			((gpu_db_stack.cur_oppidx & 0x3F) << 20)|
			(gpu_db_stack.cur_volt & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel(((gpu_db_stack.cur_freq & 0xFFF) << 20)|
			(gpu_db_stack.cur_vsram & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel((gpu_db_stack.park_flag << 20)|
			(gpu_db_stack.cur_vcore & 0xFFFFF), next_log_offs);
		next_log_offs += 4;

		writel(((gpu_db_limiter.f_limiter & 0xF) << 16)|
			((gpu_db_limiter.c_limiter & 0xF) << 12)|
			((gpu_db_limiter.floor_oppidx & 0x3F) << 6)|
			(gpu_db_limiter.ceiling_oppidx & 0x3F)
			, next_log_offs);
		next_log_offs += 4;

		if (next_log_offs >= end_log_offs)
			next_log_offs = start_log_offs;
	} else {
		GPUFREQ_LOGE("ioremap failed");
	}
}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_init
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : initialize gpueb log db sysram memory
 ************************************************************************************/
void __gpufreq_history_memory_init(void)
{
	int i = 0;

	start_log_offs = 0;
	start_log_offs = ioremap(GPUFREQ_HISTORY_OFFS_LOG_S,
		GPUFREQ_HISTORY_SYSRAM_SIZE);
	next_log_offs = start_log_offs;
	end_log_offs = start_log_offs + GPUFREQ_HISTORY_SYSRAM_SIZE;

	if (start_log_offs != 0)
		for (i = 0; i < (GPUFREQ_HISTORY_SYSRAM_SIZE>>2); i++)
			writel(0, start_log_offs + (i<<2));
	else
		GPUFREQ_LOGE("ioremap failed");
}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_reset
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : reset gpueb log db sysram memory
 ************************************************************************************/
void __gpufreq_history_memory_reset(void)
{
	int i = 0;

	if (start_log_offs != 0)
		for (i = 0; i < (GPUFREQ_HISTORY_SYSRAM_SIZE>>2); i++)
			writel(0, start_log_offs + (i<<2));
	else
		GPUFREQ_LOGE("start_log_offs is not set");
}

/***********************************************************************************
 * Function Name      : __gpufreq_history_memory_uninit
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : -
 ************************************************************************************/
void __gpufreq_history_memory_uninit(void)
{
	if (start_log_offs != 0)
		iounmap(start_log_offs);
}

/***********************************************************************************
 * Function Name      : __gpufreq_power_history_init_entry
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : -
 ************************************************************************************/
void __gpufreq_power_history_init_entry(void)
{
	struct gpu_power_init_log gpu_db_init = {};

	if (p_v_log_offs != 0) {
		gpu_db_init.start_idx = 0xAAAAAAAA;

		// /* update current status to shared memory */
		writel(gpu_db_init.start_idx, p_v_log_offs);

		p_start_log_offs = p_v_log_offs + sizeof(struct gpu_power_init_log);
		p_next_log_offs = p_start_log_offs;
	} else {
		GPUFREQ_LOGE("ioremap failed");
	}
}

/***********************************************************************************
 * Function Name      : __gpufreq_power_history_entry
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : -
 ************************************************************************************/
void __gpufreq_power_history_entry(void)
{
	struct gpu_power_history_log gpu_db_history = {};
	unsigned long long time_s = 0;

	if (next_log_offs != 0) {
		time_s = sched_clock();//64 bit
		gpu_db_history.time_stamp_h_log = (time_s >> 32) & 0xFFFFFFFF;
		gpu_db_history.time_stamp_l_log = time_s & 0xFFFFFFFF;
		gpu_db_history.power_on_off = __gpufreq_get_power_state();
		gpu_db_history.buck_vcore = __gpufreq_get_power_buck(HISTORY_BUCK_VCORE);
		gpu_db_history.buck_vsram = __gpufreq_get_power_buck(HISTORY_BUCK_VSRAM);
		gpu_db_history.buck_vtop = __gpufreq_get_power_buck(HISTORY_BUCK_VTOP);
		gpu_db_history.buck_vstack = __gpufreq_get_power_buck(HISTORY_BUCK_VSTACK);
		gpu_db_history.mfg_0 = __gpufreq_get_power_mfg(HISTORY_MFG_0);
		gpu_db_history.mfg_1 = __gpufreq_get_power_mfg(HISTORY_MFG_1);
		gpu_db_history.mfg_2 = __gpufreq_get_power_mfg(HISTORY_MFG_2);
		gpu_db_history.mfg_3 = __gpufreq_get_power_mfg(HISTORY_MFG_3);

		// /* update current status to shared memory */
		writel(gpu_db_history.time_stamp_h_log, p_next_log_offs);
		p_next_log_offs += 4;
		writel(gpu_db_history.time_stamp_l_log, p_next_log_offs);
		p_next_log_offs += 4;
		writel((gpu_db_history.power_on_off |
			(gpu_db_history.buck_vcore << 1) |
			(gpu_db_history.buck_vsram << 2) |
			(gpu_db_history.buck_vtop << 3) |
			(gpu_db_history.buck_vstack << 4) |
			(gpu_db_history.mfg_0 << 5) |
			(gpu_db_history.mfg_1 << 6) |
			(gpu_db_history.mfg_2 << 7) |
			(gpu_db_history.mfg_3 << 8)), p_next_log_offs);
		p_next_log_offs += 4;

		if (p_next_log_offs >= p_end_log_offs)
			p_next_log_offs = p_start_log_offs;

	} else {
		GPUFREQ_LOGE("ioremap failed");
	}
}

/***********************************************************************************
 * Function Name      : __gpufreq_power_history_memory_init
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : initialize gpueb power_state log db sysram memory
 ************************************************************************************/
void __gpufreq_power_history_memory_init(void)
{
	int i = 0;

	p_v_log_offs = 0;
	p_v_log_offs = ioremap(GPUFREQ_HISTORY_OFFS_LOG_E,
		GPUFREQ_POWER_HISTORY_SYSRAM_SIZE);
	p_start_log_offs = p_v_log_offs;
	p_next_log_offs = p_v_log_offs;
	p_end_log_offs = p_v_log_offs + GPUFREQ_POWER_HISTORY_SYSRAM_SIZE;

	if (p_v_log_offs != 0)
		for (i = 0; i < (GPUFREQ_POWER_HISTORY_SYSRAM_SIZE>>2); i++)
			writel(0, p_v_log_offs + (i<<2));
	else
		GPUFREQ_LOGE("ioremap failed");
}

/***********************************************************************************
 * Function Name      : __gpufreq_power_history_memory_reset
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : reset gpueb log db sysram memory
 ************************************************************************************/
void __gpufreq_power_history_memory_reset(void)
{
	int i = 0;

	if (p_v_log_offs != 0)
		for (i = 0; i < (GPUFREQ_POWER_HISTORY_SYSRAM_SIZE>>2); i++)
			writel(0, p_v_log_offs + (i<<2));
	else
		GPUFREQ_LOGE("p_v_log_offs is not set");
}


/***********************************************************************************
 * Function Name      : __gpufreq_power_history_memory_uninit
 * Inputs             : -
 * Outputs            : -
 * Returns            : -
 * Description        : -
 ************************************************************************************/
void __gpufreq_power_history_memory_uninit(void)
{
	if (p_v_log_offs != 0)
		iounmap(p_v_log_offs);
}
