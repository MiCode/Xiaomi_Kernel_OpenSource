/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM lmh

#if !defined(_TRACE_LMH_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_LMH_H

#include <linux/tracepoint.h>

TRACE_EVENT(lmh_dcvs_freq,
	TP_PROTO(unsigned long cpu, unsigned long freq),

	TP_ARGS(cpu, freq),

	TP_STRUCT__entry(
		__field(unsigned long, cpu)
		__field(unsigned long, freq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->freq = freq;
	),

	TP_printk("cpu:%lu max frequency:%lu", __entry->cpu, __entry->freq)
);
#endif /* _TRACE_LMH_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
