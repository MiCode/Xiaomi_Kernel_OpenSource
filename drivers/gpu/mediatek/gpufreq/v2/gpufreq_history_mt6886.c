// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_mt6886.c
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
#include <gpufreq_history_mt6886.h>
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

static int g_history_target_opp_stack;
static int g_history_target_opp_top;

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

/***********************************************************************************
 *  Function Name      : __gpufreq_record_history_entry
 *  Inputs             : -
 *  Outputs            : -
 *  Returns            : -
 *  Description        : -
 ************************************************************************************/
void __gpufreq_record_history_entry(void)
{
	u64 time_s = 0;

	if (next_log_offs != 0)
		time_s = sched_clock(); //64 bit
	else
		GPUFREQ_LOGE("ioremap failed");
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
