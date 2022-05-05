/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __GPUFREQ_HISTORY_COMMON_H__
#define __GPUFREQ_HISTORY_COMMON_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_HISTORY_COMMON_ENABLE            (1)

/**************************************************
 * Enumeration
 **************************************************/
enum gpufreq_history_state {
	HISTORY_FREE = 0,                      // 0000 0000
	HISTORY_VOLT_PARK = 1 << 0,            // 0000 0001
	HISTORY_VSRAM_PARK = 1 << 1,           // 0000 0010
	HISTORY_PTP3 = 1 << 2,                 // 0000 0100
	HISTORY_CHANGE_VOLT_TOP = 1 << 3,      // 0000 1000
	HISTORY_CHANGE_FREQ_TOP = 1 << 4,      // 0001 0000
	HISTORY_CHANGE_VOLT_STACK = 1 << 5,    // 0010 0000
	HISTORY_CHANGE_FREQ_STACK = 1 << 6,    // 0100 0000
};

/**************************************************
 * Platform Function Declaration
 **************************************************/
/* Common */
void gpufreq_set_history_state(unsigned int state);
void gpufreq_set_history_park_volt(unsigned int volt);
unsigned int gpufreq_get_history_state(void);
unsigned int gpufreq_get_history_park_volt(void);
void gpufreq_set_history_target_opp(enum gpufreq_target target,
	int oppidx);
int gpufreq_get_history_target_opp(enum gpufreq_target target);

#endif /* __GPUFREQ_HISTORY_COMMON_H__ */
