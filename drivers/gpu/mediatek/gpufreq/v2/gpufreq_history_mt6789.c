// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_mt6789.c
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
#include <gpufreq_history_mt6789.h>
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


/***********************************************************************************
 *  Function Name      : __gpufreq_record_history_entry
 *  Inputs             : -
 *  Outputs            : -
 *  Returns            : -
 *  Description        : -
 ************************************************************************************/
void __gpufreq_record_history_entry(void)
{

	struct gpu_dvfs_source gpu_db_top = {};
	struct gpu_dvfs_source gpu_db_stack = {};
	enum gpufreq_history_state history_state;
	u64 time_s = 0;

	if (next_log_offs != 0) {
		time_s = sched_clock(); //64 bit
		history_state = gpufreq_get_history_state();
		if ((history_state & 0x1) || (history_state & 0x8))
			gpu_db_top.cur_volt = gpufreq_get_history_park_volt();
		else
			gpu_db_top.cur_volt = __gpufreq_get_cur_vgpu();
		gpu_db_top.cur_oppidx = __gpufreq_get_cur_idx_gpu();
		gpu_db_top.target_oppidx = gpufreq_get_history_target_opp(TARGET_GPU);
		if (history_state & 0x2)
			gpu_db_top.cur_vsram = gpufreq_get_history_park_volt();
		else
			gpu_db_top.cur_vsram = __gpufreq_get_cur_vsram_gpu();
		gpu_db_top.park_flag = history_state;
		gpu_db_top.cur_freq = __gpufreq_get_cur_fgpu()/1000;
		gpu_db_top.floor_oppidx = gpuppm_get_floor(TARGET_GPU);
		gpu_db_top.ceiling_oppidx = gpuppm_get_ceiling(TARGET_GPU);
		gpu_db_top.c_limiter = gpuppm_get_c_limiter(TARGET_GPU);
		gpu_db_top.f_limiter = gpuppm_get_f_limiter(TARGET_GPU);

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
		next_log_offs += 8;
		writel(((gpu_db_top.park_flag & 0x3FF) << 22), next_log_offs);
		next_log_offs += 8;

		if (next_log_offs >= end_log_offs)
			next_log_offs = start_log_offs;

		gpufreq_set_history_state(HISTORY_FREE);

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
