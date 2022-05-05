// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */
/**
 * @file    gpufreq_history_common.c
 * @brief   GPU DVFS History log Implementation
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */


/* GPUFREQ */
#include <gpufreq_v2.h>
#include <gpufreq_history_common.h>

/**
 * ===============================================
 * Variable Definition
 * ===============================================
 */

static enum gpufreq_history_state g_history_state;
static unsigned int g_history_park_volt;
static int g_history_target_opp_stack;
static int g_history_target_opp_top;

/**
 * ===============================================
 * Common Function Definition
 * ===============================================
 */

/* API: set history_state */
void gpufreq_set_history_state(unsigned int state)
{
	g_history_state = state;
}

/* API: get history_state */
unsigned int gpufreq_get_history_state(void)
{
	return g_history_state;
}

/* API: set park_volt */
void gpufreq_set_history_park_volt(unsigned int volt)
{
	g_history_park_volt = volt;
}

/* API: get park_volt */
unsigned int gpufreq_get_history_park_volt(void)
{
	return g_history_park_volt;
}

/* API: set target oppidx */
void gpufreq_set_history_target_opp(enum gpufreq_target target, int oppidx)
{
	if (target == TARGET_STACK)
		g_history_target_opp_stack = oppidx;
	else
		g_history_target_opp_top = oppidx;
}

/* API: get park_volt */
int gpufreq_get_history_target_opp(enum gpufreq_target target)
{
	if (target == TARGET_STACK)
		return g_history_target_opp_stack;
	else
		return g_history_target_opp_top;
}
