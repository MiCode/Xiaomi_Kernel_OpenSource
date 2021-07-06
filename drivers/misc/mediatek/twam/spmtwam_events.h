/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM spmtwam_events

#if !defined(_SPMTWAM_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _SPMTWAM_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(spmtwam,
	TP_PROTO(int val1, int val2, int val3, int val4),
	TP_ARGS(val1, val2, val3, val4),
	TP_STRUCT__entry(
		__field(int, val1)
		__field(int, val2)
		__field(int, val3)
		__field(int, val4)
		),
	TP_fast_assign(
		__entry->val1 = val1;
		__entry->val2 = val2;
		__entry->val3 = val3;
		__entry->val4 = val4;
		),
	TP_printk(
		"%d, %d, %d, %d",
		(int)__entry->val1,
		(int)__entry->val2,
		(int)__entry->val3,
		(int)__entry->val4
		)
);

#endif /* _SPMTWAM_EVENTS_H || TRACE_HEADER_MULTI_READ*/

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE spmtwam_events
#include <trace/define_trace.h>
