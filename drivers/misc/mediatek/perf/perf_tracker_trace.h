/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#if !defined(_PERF_TRACKER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _PERF_TRACKER_TRACE_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf_tracker
#define TRACE_INCLUDE_FILE perf_tracker_trace


TRACE_EVENT(perf_index_gpu,
	TP_PROTO(u32 *gpu_data, u32 lens),
	TP_ARGS(gpu_data, lens),
	TP_STRUCT__entry(
		__dynamic_array(u32, gpu_data, lens)
		__field(u32, lens)
	),
	TP_fast_assign(
		memcpy(__get_dynamic_array(gpu_data), gpu_data,
			lens * sizeof(u32));
		__entry->lens = lens;
	),
	TP_printk("data=%s", __print_array(__get_dynamic_array(gpu_data),
		__entry->lens, sizeof(u32)))
);


#endif /*_PERF_TRACKER_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE perf_tracker_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
