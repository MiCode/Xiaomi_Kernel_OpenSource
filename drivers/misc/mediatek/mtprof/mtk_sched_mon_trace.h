/*
 * Copyright (C) 2018 MediaTek Inc.
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

