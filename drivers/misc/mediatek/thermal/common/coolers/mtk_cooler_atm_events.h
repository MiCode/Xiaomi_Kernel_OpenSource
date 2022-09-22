/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#undef TRACE_SYSTEM
#define TRACE_SYSTEM atm_events

#if !defined(_TRACE_MTK_COOLER_ATM_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_COOLER_ATM_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(ATM__result,

	TP_PROTO(int ttj,
		 int max_tj,
		 int cpu_L_tj,
		 int cpu_B_tj,
		 int gpu_tj,
		 unsigned int gpu_loading,
		 unsigned int cpu_power,
		 unsigned int gpu_power,
		 unsigned int atm_state,
		 int eara_state),

	TP_ARGS(ttj, max_tj, cpu_L_tj, cpu_B_tj, gpu_tj, gpu_loading, cpu_power,
		gpu_power, atm_state, eara_state),

	TP_STRUCT__entry(
		 __field(int, ttj)
		 __field(int, max_tj)
		 __field(int, cpu_L_tj)
		 __field(int, cpu_B_tj)
		 __field(int, gpu_tj)
		 __field(unsigned int, gpu_loading)
		 __field(unsigned int, cpu_power)
		 __field(unsigned int, gpu_power)
		 __field(unsigned int, atm_state)
		 __field(int, eara_state)
	),

	TP_fast_assign(
		__entry->ttj = ttj;
		__entry->max_tj = max_tj;
		__entry->cpu_L_tj = cpu_L_tj;
		__entry->cpu_B_tj = cpu_B_tj;
		__entry->gpu_tj = gpu_tj;
		__entry->gpu_loading = gpu_loading;
		__entry->cpu_power = cpu_power;
		__entry->gpu_power = gpu_power;
		__entry->atm_state = atm_state;
		__entry->eara_state = eara_state;
	),

	TP_printk("Ttj=%d Mtj=%d Ltj=%d Btj=%d Gtj=%d gl=%d cp=%d gp=%d atm=%d eara=%d\n",
		__entry->ttj, __entry->max_tj, __entry->cpu_L_tj, __entry->cpu_B_tj,
		__entry->gpu_tj, __entry->gpu_loading, __entry->cpu_power, __entry->gpu_power,
		__entry->atm_state, __entry->eara_state)
);

TRACE_EVENT(ATM__pid,

	TP_PROTO(long curr_temp,
		 long prev_temp,
		 int tt,
		 int tp,
		 int theta,
		 int delta_power_tt,
		 int delta_power_tp,
		 int delta_power,
		 int total_power),

	TP_ARGS(curr_temp, prev_temp, tt, tp, theta, delta_power_tt, delta_power_tp,
		delta_power, total_power),

	TP_STRUCT__entry(
		 __field(long, curr_temp)
		 __field(long, prev_temp)
		 __field(int, tt)
		 __field(int, tp)
		 __field(int, theta)
		 __field(int, delta_power_tt)
		 __field(int, delta_power_tp)
		 __field(int, delta_power)
		 __field(int, total_power)
	),

	TP_fast_assign(
		__entry->curr_temp = curr_temp;
		__entry->prev_temp = prev_temp;
		__entry->tt = tt;
		__entry->tp = tp;
		__entry->theta = theta;
		__entry->delta_power_tt = delta_power_tt;
		__entry->delta_power_tp = delta_power_tp;
		__entry->delta_power = delta_power;
		__entry->total_power = total_power;
	),

	TP_printk("t=%ld pt=%ld tt=%d tp=%d theta=%d dp_tt=%d dp_tp=%d dp=%d p=%d\n",
		 __entry->curr_temp, __entry->prev_temp, __entry->tt, __entry->tp,
		 __entry->theta, __entry->delta_power_tt, __entry->delta_power_tp,
		 __entry->delta_power, __entry->total_power)
);

#endif /* _TRACE_MTK_COOLER_ATM_EVENTS_H */


/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH common/coolers
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_cooler_atm_events
#include <trace/define_trace.h>

