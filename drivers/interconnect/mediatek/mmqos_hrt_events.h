/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mmqos_hrt_events

#if !defined(_TRACE_MMQOS_HRT_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MMQOS_HRT_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(mmqos__total_hrt_bw,
	TP_PROTO(int type, int bw),
	TP_ARGS(type, bw),
	TP_STRUCT__entry(
		__field(int, type)
		__field(int, bw)
	),
	TP_fast_assign(
		__entry->type = type;
		__entry->bw = bw;
	),
	TP_printk("%s=%d",
		__entry->type == HRT_NONE ? "CAM_MAX" :
			(__entry->type == HRT_MD ? "MD" :
			(__entry->type == HRT_CAM ? "CAM" :
			(__entry->type == HRT_MML ? "MML" : "DISP"))),
		(int)__entry->bw)
);
TRACE_EVENT(mmqos__used_hrt_bw,
	TP_PROTO(int type, int bw),
	TP_ARGS(type, bw),
	TP_STRUCT__entry(
		__field(int, type)
		__field(int, bw)
	),
	TP_fast_assign(
		__entry->type = type;
		__entry->bw = bw;
	),
	TP_printk("%s=%d",
		__entry->type == HRT_NONE ? "CAM_MAX" :
			(__entry->type == HRT_MD ? "MD" :
			(__entry->type == HRT_CAM ? "CAM" :
			(__entry->type == HRT_MML ? "MML" : "DISP"))),
		(int)__entry->bw)
);
TRACE_EVENT(mmqos__avail_hrt_bw,
	TP_PROTO(int bw),
	TP_ARGS(bw),
	TP_STRUCT__entry(
		__field(int, bw)
	),
	TP_fast_assign(
		__entry->bw = bw;
	),
	TP_printk("avail_bw=%d",
		(int)__entry->bw)
);
#endif /* _TRACE_MMQOS_HRT_EVENTS_H */

#undef TRACE_INCLUDE_FILE
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE mmqos_hrt_events

/* This part must be outside protection */
#include <trace/define_trace.h>
