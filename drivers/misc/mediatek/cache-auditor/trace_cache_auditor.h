/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM cache_auditor

#if !defined(_TRACE_CACHE_AUDITOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CACHE_AUDITOR_H

#include <linux/types.h>
#include <linux/tracepoint.h>
TRACE_EVENT(ca_pftch_mb,

	TP_PROTO(struct perf_event *event, u64 counter),

	TP_ARGS(event, counter),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(u64, config)
		__field(u64, counter)
	),

	TP_fast_assign(
		__entry->cpu = smp_processor_id();
		__entry->config = event->attr.config;
		__entry->counter = counter*16/1024/1024;
	),

	TP_printk("C|5566|cpu%d-%lx|%lu",
		  __entry->cpu, __entry->config, __entry->counter)
);

TRACE_EVENT(ca_callback,

	TP_PROTO(unsigned long status, unsigned long total_bw,
		unsigned long cpu_bw),

	TP_ARGS(status, total_bw, cpu_bw),

	TP_STRUCT__entry(
		__field(unsigned long, status)
		__field(unsigned long, total_bw)
		__field(unsigned long, cpu_bw)
	),

	TP_fast_assign(
		__entry->status = status;
		__entry->total_bw = total_bw;
		__entry->cpu_bw = cpu_bw;
	),

	TP_printk("status:%lu, total: %lu MB/s, cpu: %lu MB/s",
			__entry->status, __entry->total_bw, __entry->cpu_bw)
);

TRACE_EVENT(ca_pftch_enabled,

	TP_PROTO(int enabled),

	TP_ARGS(enabled),

	TP_STRUCT__entry(
		__field(int, enabled)
	),

	TP_fast_assign(
		__entry->enabled = enabled;
	),

	TP_printk("enabled: %d", __entry->enabled)
);

TRACE_EVENT(ca_congested,

	TP_PROTO(int enabled),

	TP_ARGS(enabled),

	TP_STRUCT__entry(
		__field(int, enabled)
	),

	TP_fast_assign(
		__entry->enabled = enabled;
	),

	TP_printk("congested: %d", __entry->enabled)
);

TRACE_EVENT(ca_cpu_throttled,

	TP_PROTO(int enabled),

	TP_ARGS(enabled),

	TP_STRUCT__entry(
		__field(int, enabled)
	),

	TP_fast_assign(
		__entry->enabled = enabled;
	),

	TP_printk("throttled: %d", __entry->enabled)
);

TRACE_EVENT(ca_set_throttle,

	TP_PROTO(unsigned long val),

	TP_ARGS(val),

	TP_STRUCT__entry(
		__field(unsigned long, val)
	),

	TP_fast_assign(
		__entry->val = val;
	),

	TP_printk("val: %lu (%d)", __entry->val, __entry->val & 0x80)
);
#endif /* _TRACE_CACHE_AUDITOR_H */

#undef TRACE_INCLUDE_PATH

// Current path setting
#define TRACE_INCLUDE_PATH .
// current file name w/o suffix
#define TRACE_INCLUDE_FILE trace_cache_auditor
#include <trace/define_trace.h>
