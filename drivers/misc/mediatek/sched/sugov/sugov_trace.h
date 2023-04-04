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

TRACE_EVENT(sugov_ext_limits_changed,
	TP_PROTO(unsigned int cpu, unsigned int cur,
		unsigned int min, unsigned int max),
	TP_ARGS(cpu, cur, min, max),
	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, cur)
		__field(unsigned int, min)
		__field(unsigned int, max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->cur = cur;
		__entry->min = min;
		__entry->max = max;
	),
	TP_printk(
		"cpu=%u cur=%u min=%u max=%u",
		__entry->cpu,
		__entry->cur,
		__entry->min,
		__entry->max)
);

TRACE_EVENT(sugov_ext_util_debug,
	TP_PROTO(int cpu, unsigned long util_cfs,
		unsigned long util_rt, unsigned long util_dl, unsigned long util_irq,
		unsigned long util_before, unsigned long scale_irq, unsigned long bw_dl),
	TP_ARGS(cpu, util_cfs, util_rt, util_dl, util_irq, util_before, scale_irq, bw_dl),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util_cfs)
		__field(unsigned long, util_rt)
		__field(unsigned long, util_dl)
		__field(unsigned long, util_irq)
		__field(unsigned long, util_before)
		__field(unsigned long, scale_irq)
		__field(unsigned long, bw_dl)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util_cfs = util_cfs;
		__entry->util_rt = util_rt;
		__entry->util_dl = util_dl;
		__entry->util_irq = util_irq;
		__entry->util_before = util_before;
		__entry->scale_irq = scale_irq;
		__entry->bw_dl = bw_dl;
	),
	TP_printk(
		"cpu=%d cfs=%lu rt=%lu dl=%lu irq=%lu before=%lu scale_irq=%lu bw_dl=%lu",
		__entry->cpu,
		__entry->util_cfs,
		__entry->util_rt,
		__entry->util_dl,
		__entry->util_irq,
		__entry->util_before,
		__entry->scale_irq,
		__entry->bw_dl)
);
#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH sugov
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sugov_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
