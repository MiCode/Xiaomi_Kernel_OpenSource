/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_mpam_events

#if !defined(_MET_MPAM_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _MET_MPAM_EVENTS_H

#include <linux/tracepoint.h>
// coverity[var_deref_op : FALSE]
TRACE_EVENT(MPAM_CT_task_leave,
	TP_PROTO(int pid, char *comm),
	TP_ARGS(pid, comm),
	TP_STRUCT__entry(
		__field(int, pid)
		__array(char, comm, TASK_COMM_LEN)
	),
	TP_fast_assign(
		__entry->pid = pid;
		memcpy(__entry->comm, comm, TASK_COMM_LEN);
	),
	TP_printk("pid=%d, taskname=%s", __entry->pid, __entry->comm)
);

// coverity[var_deref_op : FALSE]
TRACE_EVENT(MPAM_CT_task_enter,
	TP_PROTO(int pid, char *comm),
	TP_ARGS(pid, comm),
	TP_STRUCT__entry(
		__field(int, pid)
		__array(char, comm, TASK_COMM_LEN)
	),
	TP_fast_assign(
		__entry->pid = pid;
		memcpy(__entry->comm, comm, TASK_COMM_LEN);
	),
	TP_printk("pid=%d, taskname=%s", __entry->pid, __entry->comm)
);

#endif /* _MET_MPAM_EVENTS_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE

#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE met_mpam_events

/* This part must be outside protection */
#include <trace/define_trace.h>
