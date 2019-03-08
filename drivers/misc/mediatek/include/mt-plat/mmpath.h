/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
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

