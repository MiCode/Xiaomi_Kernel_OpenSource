/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_SUGOV_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SUGOV_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

TRACE_EVENT(sugov_ext_util,
	TP_PROTO(int cpu, unsigned long util,
		unsigned int min, unsigned int max),
	TP_ARGS(cpu, util, min, max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned int, min)
		__field(unsigned int, max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->max = max;
	),
	TP_printk(
		"cpu=%d util=%lu min=%u max=%u",
		__entry->cpu,
		__entry->util,
		__entry->min,
		__entry->max)
);

TRACE_EVENT(sugov_ext_gear_state,
	TP_PROTO(unsigned int gear_id, unsigned int state),
	TP_ARGS(gear_id, state),
	TP_STRUCT__entry(
		__field(unsigned int, gear_id)
		__field(unsigned int, state)
	),
	TP_fast_assign(
		__entry->gear_id = gear_id;
		__entry->state = state;
	),
	TP_printk(
		"gear_id=%u state=%u",
		__entry->gear_id,
		__entry->state)
);

TRACE_EVENT(sugov_ext_sbb,
	TP_PROTO(int cpu, int pid, unsigned int boost,
		unsigned int util, unsigned int util_boost, unsigned int active_ratio,
		unsigned int threshold),
	TP_ARGS(cpu, pid, boost, util, util_boost, active_ratio, threshold),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(unsigned int, boost)
		__field(unsigned int, util)
		__field(unsigned int, util_boost)
		__field(unsigned int, active_ratio)
		__field(unsigned int, threshold)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->boost = boost;
		__entry->util = util;
		__entry->util_boost = util_boost;
		__entry->active_ratio = active_ratio;
		__entry->threshold = threshold;
	),
	TP_printk(
		"cpu=%d pid=%d boost=%d util=%d util_boost=%d active_ratio=%d, threshold=%d",
		__entry->cpu,
		__entry->pid,
		__entry->boost,
		__entry->util,
		__entry->util_boost,
		__entry->active_ratio,
		__entry->threshold)
);
#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH sugov
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sugov_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
