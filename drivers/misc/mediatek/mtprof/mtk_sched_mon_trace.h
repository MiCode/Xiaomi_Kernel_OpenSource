/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched_mon

#if !defined(_TRACE_SCHED_MON_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHED_MON_H

#include <linux/tracepoint.h>

TRACE_EVENT(sched_mon_msg,

	TP_PROTO(const char *buf),

	TP_ARGS(buf),

	TP_STRUCT__entry(
		__string(mbuf, buf)
	),

	TP_fast_assign(
		__assign_str(mbuf, buf);
	),

	TP_printk("%s", __get_str(mbuf))
);

#endif /* _TRACE_SCHED_MON_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE mtk_sched_mon_trace

/* This part must be outside protection */
#include <trace/define_trace.h>

