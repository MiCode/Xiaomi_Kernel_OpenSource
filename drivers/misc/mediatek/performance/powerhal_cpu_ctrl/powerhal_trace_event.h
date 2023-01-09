/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM powerhal_cpu_freq

#if !defined(_MTK_POWERHAL_TRACE_EVENT_H__) || defined(TRACE_HEADER_MULTI_READ)
#define _MTK_POWERHAL_TRACE_EVENT_H__

#include <linux/tracepoint.h>

TRACE_EVENT(powerhal_cpu_freq_user_setting,
	TP_PROTO(
	const char *buf
	),

	TP_ARGS(buf),

	TP_STRUCT__entry(
	__string(buf, buf)
	),

	TP_fast_assign(
	__assign_str(buf, buf);
	),

	TP_printk("%s",
	__get_str(buf)
	)
);
#endif /* _MTK_POWERHAL_TRACE_EVENT_H__ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE powerhal_trace_event
#include <trace/define_trace.h>

