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
#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH sugov
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sugov_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
