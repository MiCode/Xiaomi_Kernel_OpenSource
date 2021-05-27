// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM slog

#if !defined(_TRACE_SLOG_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SLOG_H

#include <linux/tracepoint.h>

#define SLOG_MSG_MAX (256)

TRACE_EVENT(slog,
	TP_PROTO(struct va_format *vaf),

	TP_ARGS(vaf),

	TP_STRUCT__entry(
		__dynamic_array(char, msg, SLOG_MSG_MAX)
	),

	TP_fast_assign(vsnprintf(__get_dynamic_array(msg),
				SLOG_MSG_MAX,
				vaf->fmt,
				*vaf->va);
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
