/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM schedwalt

#if !defined( _TRACE_WALT_PENALTY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_WALT_PENALTY_H

#include <linux/tracepoint.h>

#include "walt.h"

TRACE_EVENT(sched_penalty,
	TP_PROTO(struct task_struct *p, u64 yield_util, u64 sleep_total),

	TP_ARGS(p, yield_util, sleep_total),

	TP_STRUCT__entry(
		__array(char,		comm,	TASK_COMM_LEN)
		__field(u64,		yield_util)
		__field(u64,		sleep_total)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->yield_util	= yield_util;
		__entry->sleep_total	= sleep_total;
	),

	TP_printk("comm=%s yield_util=%lu sleep_total=%lu ",
		__entry->comm, __entry->yield_util, __entry->sleep_total)
);


#endif /* _TRACE_WALT_PENALTY_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../kernel/sched/walt
#define TRACE_INCLUDE_FILE trace_penalty

#include <trace/define_trace.h>
