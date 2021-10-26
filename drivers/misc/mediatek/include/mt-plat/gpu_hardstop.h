/*
 * Copyright (C) 2020 MediaTek Inc.
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
#define TRACE_SYSTEM gpu_hardstop

#if !defined(_TRACE_EVENT_GPU_HARDSTOP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_GPU_HARDSTOP_H

#include <linux/tracepoint.h>

TRACE_EVENT(gpu_hardstop,
	TP_PROTO(char *string, char *sub_string,
		unsigned int mfgpll,
		unsigned int freq,
		unsigned int vgpu,
		unsigned int vsram_gpu
	),
	TP_ARGS(string, sub_string, mfgpll, freq, vgpu, vsram_gpu),
	TP_STRUCT__entry(__string(str0, string)
		__string(str1, sub_string)
		__field(unsigned int, mfgpll)
		__field(unsigned int, freq)
		__field(unsigned int, vgpu)
		__field(unsigned int, vsram_gpu)
	),
	TP_fast_assign(__assign_str(str0, string);
		__assign_str(str1, sub_string);
		__entry->mfgpll = mfgpll;
		__entry->freq = freq;
		__entry->vgpu = vgpu;
		__entry->vsram_gpu = vsram_gpu;
	),
	TP_printk("%s:%s        %u      %u      %u      %u",
		__get_str(str0), __get_str(str1),
		__entry->mfgpll == 0, __entry->freq == 0,
		__entry->vgpu == 0, __entry->vsram_gpu == 0
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

