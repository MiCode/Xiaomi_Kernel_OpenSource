/*
 * Copyright (C) 2015 MediaTek Inc.
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
#define TRACE_SYSTEM mmpath

#if !defined(MMPATH_H) || defined(TRACE_HEADER_MULTI_READ)
#define MMPATH_H

#include <linux/tracepoint.h>

#ifndef TRACING_UNIT
#define TRACING_UNIT

#endif /* TRACING_UNIT */

TRACE_EVENT(MMPath,

	TP_PROTO(char *string),

	TP_ARGS(string),

	TP_STRUCT__entry(
		__string(str, string)
	),

	TP_fast_assign(
		__assign_str(str, string);
	),

	TP_printk("%s", __get_str(str))
);

#endif /* MMPATH_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mmpath
#include <trace/define_trace.h>

