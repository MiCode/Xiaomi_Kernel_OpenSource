// SPDX-License-Identifier: GPL-2.0
/*
 * mimon_trace.h - mimon ftrace event definition
 *
 * Copyright (c) 2026 Xiaomi, Inc., All rights reserved.
 */

#if !defined(TRACE_MIMON_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define TRACE_MIMON_TRACE_H

#include <linux/tracepoint.h>

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mimon

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mimon_trace

#define MAX_TRACE_STR_LEN 128

TRACE_EVENT(mimon_trace,
	TP_PROTO(const char *info),
	TP_ARGS(info),
	TP_STRUCT__entry(
		__string(info, info)
	),
	TP_fast_assign(
		__assign_str(info);
	),
	TP_printk("%s", __get_str(info))
);

void mimon_trace_format(const char *fmt, ...);

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#include <trace/define_trace.h>
#endif
