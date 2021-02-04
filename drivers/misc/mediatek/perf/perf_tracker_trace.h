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

TRACE_EVENT(fuel_gauge,
	TP_PROTO(
		int cur,
		int volt
	),

	TP_ARGS(cur, volt),

	TP_STRUCT__entry(
		__field(int, cur)
		__field(int, volt)
	),

	TP_fast_assign(
		__entry->cur = cur;
		__entry->volt = volt;
	),

	TP_printk("cur=%d, vol=%d",
		__entry->cur,
		__entry->volt
	)
);

#endif /*_PERF_TRACKER_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE perf_tracker_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
