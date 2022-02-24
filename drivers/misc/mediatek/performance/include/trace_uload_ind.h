/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM uload_ind

#if !defined(_TRACE_ULOAD_IND_H__) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_ULOAD_IND_H__

#include <linux/tracepoint.h>

TRACE_EVENT(cpu_loading,

TP_PROTO(char *str1, char *str2),

TP_ARGS(str1, str2),

TP_STRUCT__entry(
	__string(str1, str1)
	__string(str2, str2)
),

TP_fast_assign(
	__assign_str(str1, str1);
	__assign_str(str2, str2);
),

TP_printk("%s%s", __get_str(str1), __get_str(str2))
);

#endif

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_uload_ind
/* This part must be outside protection */
#include <trace/define_trace.h>
