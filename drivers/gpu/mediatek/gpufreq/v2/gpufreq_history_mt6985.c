// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_mt6985.c
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
#include <gpufreq_history_mt6985.h>
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
static unsigned int g_history_sel;
static unsigned int g_history_delsel;

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

/* API: set sel bit */
void __gpufreq_set_sel_bit(unsigned int sel)
{
	g_history_sel = sel;
}

/* API: get sel bit*/
unsigned int __gpufreq_get_sel_bit(void)
{
	return g_history_sel;
}

/* API: set delsel bit */
void __gpufreq_set_delsel_bit(unsigned int delsel)
{
	g_history_delsel = delsel;
}

/* API: get delsel bit*/
unsigned int __gpufreq_get_delsel_bit(void)
{
	return g_history_delsel;
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
	u64 time_s = 0;

	if (next_log_offs != 0) {
		time_s = sched_clock(); //64 bit
		/* gpu stack */
		gpu_db_top.cur_volt = __gpufreq_get_cur_vgpu();
		gpu_db_top.cur_oppidx = __gpufreq_get_cur_idx_gpu();
		//same as stack
		gpu_db_top.target_oppidx = gpufreq_get_history_target_opp(TARGET_STACK);
		gpu_db_top.cur_vsram = __gpufreq_get_cur_vsram_gpu();
		gpu_db_top.cur_freq = __gpufreq_get_cur_fgpu()/1000;
		gpu_db_top.floor_oppidx = gpuppm_get_floor();
		gpu_db_top.ceiling_oppidx = gpuppm_get_ceiling();
		gpu_db_top.c_limiter = gpuppm_get_c_limiter();
		gpu_db_top.f_limiter = gpuppm_get_f_limiter();

		/* gpu stack */
		gpu_db_stack.cur_volt = __gpufreq_get_cur_vstack();
		gpu_db_stack.cur_oppidx = __gpufreq_get_cur_idx_stack();
		gpu_db_stack.target_oppidx = gpufreq_get_history_target_opp(TARGET_STACK);
		gpu_db_stack.cur_vsram = __gpufreq_get_cur_vsram_stack();
		gpu_db_stack.park_flag = (__gpufreq_get_sel_bit() & 0x1) |
			((__gpufreq_get_delsel_bit() & 0x1) << 1) | (history_state << 2);
		gpu_db_stack.cur_freq = __gpufreq_get_cur_fstack()/1000;
		gpu_db_stack.floor_oppidx = gpuppm_get_floor();
		gpu_db_stack.ceiling_oppidx = gpuppm_get_ceiling();
		gpu_db_stack.c_limiter = gpuppm_get_c_limiter();
		gpu_db_stack.f_limiter = gpuppm_get_f_limiter();

		// /* update current status to shared memory */
		writel((time_s >> 32) & 0xFFFFFFFF, next_log_offs);
		next_log_offs += 4;
		writel(time_s & 0xFFFFFFFF, next_log_offs);
		next_log_offs += 4;
		writel(((gpu_db_top.target_oppidx & 0x3F) << 26)|
			((gpu_db_top.cur_oppidx & 0x3F) << 20)|
			(gpu_db_top.cur_volt & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel((0xFF << 20)|(gpu_db_top.cur_vsram & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel(((gpu_db_top.f_limiter & 0xF) << 28)|
			((gpu_db_top.c_limiter & 0xF) << 24)|
			((gpu_db_top.floor_oppidx & 0x3F) << 18)|
			((gpu_db_top.ceiling_oppidx & 0x3F) << 12)|
			(gpu_db_top.cur_freq & 0xFFF), next_log_offs);
		next_log_offs += 4;
		writel(((gpu_db_stack.target_oppidx & 0x3F) << 26)|
			((gpu_db_stack.cur_oppidx & 0x3F) << 20)|
			(gpu_db_stack.cur_volt & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel((gpu_db_stack.park_flag << 20)|
			(gpu_db_stack.cur_vsram & 0xFFFFF), next_log_offs);
		next_log_offs += 4;
		writel(((gpu_db_stack.f_limiter & 0xF) << 28)|
			((gpu_db_stack.c_limiter & 0xF) << 24)|
			((gpu_db_stack.floor_oppidx & 0x3F) << 18)|
			((gpu_db_stack.ceiling_oppidx & 0x3F) << 12)|
			(gpu_db_stack.cur_freq & 0xFFF), next_log_offs);
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
