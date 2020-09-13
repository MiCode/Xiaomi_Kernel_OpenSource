/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CPUIDLE_STATUS_H__
#define __MTK_CPUIDLE_STATUS_H__

#include <linux/seq_file.h>

#define sec_to_ns(v)	((v) * 1000 * 1000 * 1000ULL)

enum {
	IDLE_PARAM_LAT,
	IDLE_PARAM_RES,
	IDLE_PARAM_EN,

	NF_IDLE_PARAM
};

#define get_residency(drv, idx)\
	((drv)->states[idx].target_residency)
#define get_latency(drv, idx)\
	((drv)->states[idx].exit_latency)
#define get_disabled(drv, idx)\
	((drv)->states[idx].disabled)

#define mtk_cpuidle_get_param(drv, state, param)                \
({                                                              \
	int val = -1;                                           \
	if (param == IDLE_PARAM_LAT)                            \
		val = (int)get_latency(drv, state);             \
	else if (param == IDLE_PARAM_RES)                       \
		val = (int)get_residency(drv, state);           \
	else if (param == IDLE_PARAM_EN)                        \
		val = get_disabled(drv, state) ? 0 : 1 ;        \
	val;                                                    \
})

#define mtk_cpuidle_set_param(drv, state, param, val)           \
do {                                                            \
	if (param == IDLE_PARAM_LAT)                            \
		get_latency(drv, state) = val;                  \
	else if (param == IDLE_PARAM_RES)                       \
		get_residency(drv, state) = val;                \
	else if (param == IDLE_PARAM_EN)                        \
		get_disabled(drv, state) = !val ;               \
} while (0)

void mtk_cpuidle_set_stress_test(bool en);
bool mtk_cpuidle_get_stress_status(void);
void mtk_cpuidle_set_stress_time(unsigned int val);
unsigned int mtk_cpuidle_get_stress_time(void);

void mtk_cpuidle_prof_ratio_start(void);
void mtk_cpuidle_prof_ratio_stop(void);
void mtk_cpuidle_prof_ratio_dump(struct seq_file *m);

void mtk_cpuidle_ctrl_timer_en(bool enable);
bool mtk_cpuidle_ctrl_timer_sta_get(void);
void mtk_cpuidle_ctrl_log_en(bool enable);
bool mtk_cpuidle_ctrl_log_sta_get(void);

void mtk_cpuidle_state_enable(bool en);

int __init mtk_cpuidle_status_init(void);
void __exit mtk_cpuidle_status_exit(void);

#endif /* __MTK_CPUIDLE_STATUS_H__ */
