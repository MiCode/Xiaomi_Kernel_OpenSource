/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM pob

#if !defined(_TRACE_POB_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_POB_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(pob_log_template,

	TP_PROTO(char *log),

	TP_ARGS(log),

	TP_STRUCT__entry(
		__string(msg, log)
	),

	TP_fast_assign(
		__assign_str(msg, log);
	),

	TP_printk("%s", __get_str(msg))
);

DEFINE_EVENT(pob_log_template, pob_log,
	     TP_PROTO(char *log),
	     TP_ARGS(log));

#endif /* _TRACE_POB_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
