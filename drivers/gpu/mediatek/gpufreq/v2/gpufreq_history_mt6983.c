// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpufreq_history_mt6983.c
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
#include <gpufreq_history_mt6983.h>
#include <gpuppm.h>
#include <gpufreq_common.h>

/**
 * ===============================================
 * Variable Definition
 * ===============================================
 */
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
