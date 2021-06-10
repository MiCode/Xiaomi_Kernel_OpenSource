// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM gpu_hardstop

#if !defined(_TRACE_EVENT_GPU_HARDSTOP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_GPU_HARDSTOP_H

#include <linux/tracepoint.h>

TRACE_EVENT(gpu_hardstop,
	TP_PROTO(char *string, char *sub_string,
		unsigned int gpu_freq,
		unsigned int gpu_volt,
		unsigned int gpu_vsram,
		unsigned int stack_freq,
		unsigned int stack_volt,
		unsigned int stack_vsram
	),
	TP_ARGS(string, sub_string, gpu_freq, gpu_volt, gpu_vsram, stack_freq, stack_volt, stack_vsram),
	TP_STRUCT__entry(__string(str0, string)
		__string(str1, sub_string)
		__field(unsigned int, gpu_freq)
		__field(unsigned int, gpu_volt)
		__field(unsigned int, gpu_vsram)
		__field(unsigned int, stack_freq)
		__field(unsigned int, stack_volt)
		__field(unsigned int, stack_vsram)
	),
	TP_fast_assign(__assign_str(str0, string);
		__assign_str(str1, sub_string);
		__entry->gpu_freq = gpu_freq;
		__entry->gpu_volt = gpu_volt;
		__entry->gpu_vsram = gpu_vsram;
		__entry->stack_freq = stack_freq;
		__entry->stack_volt = stack_volt;
		__entry->stack_vsram = stack_vsram;
	),
	TP_printk("%s:%s        %u      %u      %u      %u        %u        %u",
		__get_str(str0), __get_str(str1),
		__entry->gpu_freq == 0, __entry->gpu_volt == 0,
		__entry->gpu_vsram == 0, __entry->stack_freq == 0,
		__entry->stack_volt == 0, __entry->stack_vsram == 0
	)
);
#endif /* _TRACE_EVENT_GPU_HARDSTOP_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
/*
 * TRACE_INCLUDE_FILE is not needed if the filename and TRACE_SYSTEM are equal
 */
#define TRACE_INCLUDE_FILE gpu_hardstop
#include <trace/define_trace.h>
