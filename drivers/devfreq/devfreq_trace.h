/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#if !defined(_DEVFREQ_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _DEVFREQ_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM devfreq
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE devfreq_trace

#include <linux/tracepoint.h>

TRACE_EVENT(devfreq_msg,
	TP_PROTO(const char *msg),
	TP_ARGS(msg),
	TP_STRUCT__entry(
		__string(msg, msg)
	),
	TP_fast_assign(
		__assign_str(msg, msg);
	),
	TP_printk(
		"%s", __get_str(msg)
	)
);

#endif /* _DEVFREQ_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>

