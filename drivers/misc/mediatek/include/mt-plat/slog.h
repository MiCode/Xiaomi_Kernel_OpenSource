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
#define TRACE_SYSTEM slog

#if !defined(_TRACE_SLOG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SLOG_H

#include <linux/tracepoint.h>

#define SLOG_MSG_MAX (200)

TRACE_EVENT(slog,
	TP_PROTO(struct va_format *vaf),

	TP_ARGS(vaf),

	TP_STRUCT__entry(
		__dynamic_array(char, msg, SLOG_MSG_MAX)
	),

	TP_fast_assign(
		WARN_ON_ONCE(vsnprintf(__get_dynamic_array(msg),
					SLOG_MSG_MAX,
					vaf->fmt,
					*vaf->va) >= SLOG_MSG_MAX);
	),

	TP_printk("%s", __get_str(msg))
);

#endif /* if !defined(_TRACE_SLOG_H) || defined(TRACE_HEADER_MULTI_READ) */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE slog

#include <trace/define_trace.h>
