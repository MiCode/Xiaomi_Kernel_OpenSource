/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM psi

#if !defined(_TRACE_PSI_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PSI_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/psi_types.h>

TRACE_EVENT(psi_window_vmstat,

	TP_PROTO(u64 mem_some, u64 mem_full, const char *zone_name, u64 high,
		u64 free, u64 cma, u64 file),

	TP_ARGS(mem_some, mem_full, zone_name, high, free, cma, file),

	TP_STRUCT__entry(
		__field(u64, mem_some)
		__field(u64, mem_full)
		__string(name, zone_name)
		__field(u64, high)
		__field(u64, free)
		__field(u64, cma)
		__field(u64, file)
	),

	TP_fast_assign(
		__entry->mem_some = mem_some;
		__entry->mem_full = mem_full;
		__assign_str(name, zone_name);
		__entry->high = high;
		__entry->free = free;
		__entry->cma = cma;
		__entry->file = file;
	),

	TP_printk("%16s: MEMSOME: %9lluns MEMFULL: %9lluns High: %9llukB Free: %9llukB CMA: %8llukB File: %9llukB",
		__get_str(name), __entry->mem_some, __entry->mem_full,
		__entry->high, __entry->free, __entry->cma, __entry->file
	)
);

TRACE_EVENT(psi_event,

	TP_PROTO(enum psi_states state, u64 threshold),

	TP_ARGS(state, threshold),

	TP_STRUCT__entry(
		__field(enum psi_states, state)
		__field(u64, threshold)
	),

	TP_fast_assign(
		__entry->state = state;
		__entry->threshold = threshold;
	),

	TP_printk("State: %d Threshold: %#llu ns",
		__entry->state, __entry->threshold
	)
);

#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
