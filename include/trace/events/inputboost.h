/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM inputboost

#if !defined(_TRACE_EVENT_INPUTBOOST_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_INPUTBOOST_H

#include <linux/tracepoint.h>
#include <linux/types.h>

TRACE_EVENT(input_boost,

	TP_PROTO(int cpu,
	unsigned int input_boost_min,
	unsigned int input_boost_freq),

	TP_ARGS(cpu, input_boost_min, input_boost_freq),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, input_boost_min)
		__field(unsigned int, input_boost_freq)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->input_boost_min	= input_boost_min;
		__entry->input_boost_freq	= input_boost_freq;
	),

	TP_printk("%d, %u, %u",
			__entry->cpu, __entry->input_boost_min,
			__entry->input_boost_freq)
);

#endif
#include <trace/define_trace.h>

