/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_CPUIDLE_STATUS_H__
#define __MTK_CPUIDLE_STATUS_H__

#define sec_to_ns(v)	((v) * 1000 * 1000 * 1000ULL)

enum idle_param {
	IDLE_PARAM_EN,
	IDLE_PARAM_LAT,
	IDLE_PARAM_RES,

	NF_IDLE_PARAM
};

#define S2IDLE_STATE_NAME	"s2idle"

#define get_residency_ns(drv, idx)\
	((drv)->states[idx].target_residency_ns)
#define get_latency_ns(drv, idx)\
	((drv)->states[idx].exit_latency_ns)

#define get_residency(drv, idx)\
	((drv)->states[idx].target_residency)
#define get_latency(drv, idx)\
	((drv)->states[idx].exit_latency)
#define get_disabled(drv, idx)\
	((drv)->states[idx].flags)

#define mtk_cpuidle_get_param(drv, state, param)                \
({                                                              \
	long val = -1;                                           \
	if (param == IDLE_PARAM_LAT)                            \
		val = (long) get_latency(drv, state);             \
	else if (param == IDLE_PARAM_RES)                       \
		val = (long) get_residency(drv, state);           \
	else if (param == IDLE_PARAM_EN)                        \
		val = (long) ((get_disabled(drv, state) & CPUIDLE_FLAG_UNUSABLE) ? 0 : 1) ; \
	val;                                                    \
})
#define mtk_cpuidle_set_param(drv, state, param, val)           \
do {                                                            \
	if (param == IDLE_PARAM_LAT) {                          \
		u64 __time_ns = val;				\
		__time_ns = __time_ns * NSEC_PER_USEC;		\
		get_latency(drv, state) = (unsigned int)val;	\
		get_latency_ns(drv, state) = __time_ns;		\
	} else if (param == IDLE_PARAM_RES) {			\
		u64 __time_ns = val;				\
		__time_ns = __time_ns * NSEC_PER_USEC;		\
		get_residency(drv, state) = (unsigned int)val;	\
		get_residency_ns(drv, state) = __time_ns;	\
	} else if (param == IDLE_PARAM_EN) {			\
		if (!!val) {					\
			get_disabled(drv, state) &= ~CPUIDLE_FLAG_UNUSABLE;	\
			cpuidle_driver_state_disabled(drv, state, false);	\
		} else {							\
			get_disabled(drv, state) |= CPUIDLE_FLAG_UNUSABLE;	\
			cpuidle_driver_state_disabled(drv, state, true);	\
		}								\
	}							\
} while (0)

void mtk_cpuidle_set_stress_test(bool en);
bool mtk_cpuidle_get_stress_status(void);
void mtk_cpuidle_set_stress_time(unsigned int val);
unsigned int mtk_cpuidle_get_stress_time(void);

void mtk_cpuidle_prof_ratio_start(void);
void mtk_cpuidle_prof_ratio_stop(void);
void mtk_cpuidle_prof_ratio_dump(char **ToUserBuf, size_t *size);

void mtk_cpuidle_ctrl_log_en(bool enable);
bool mtk_cpuidle_ctrl_log_sta_get(void);

void mtk_cpuidle_state_enable(bool en);
long mtk_cpuidle_state_enabled(void);

int mtk_cpuidle_status_init(void);
void mtk_cpuidle_status_exit(void);

unsigned long long mtk_cpuidle_state_last_dis_ms(void);

int mtk_s2idle_state_enable(bool en);
#endif /* __MTK_CPUIDLE_STATUS_H__ */
