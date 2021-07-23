/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#undef	TRACE_SYSTEM
#define	TRACE_SYSTEM	tee

#if !defined(_TRACE_TEE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TEE_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include <tee_sanity.h>

TRACE_EVENT(tee_sched_start,
	TP_PROTO(struct tee_trace_struct *p),
	TP_ARGS(p),

	TP_STRUCT__entry(
		__field(u32, cpuid)
		__array(char, ktime, TASK_COMM_LEN)
		__field(u32, tpid)
		__array(char, tbuf, TASK_COMM_LEN * 2)
	),
	TP_fast_assign(
		__entry->cpuid = p->cpuid;
		memcpy(__entry->ktime, p->ktimestamp, TASK_COMM_LEN);
		__entry->tpid = p->tee_pid;
		memcpy(__entry->tbuf, p->tee_postfix, TASK_COMM_LEN * 2);
	),
	TP_printk("B|%d|%s[%u][%s]%03x-%s",
		BEGINED_PID,
		TEE_TRACING_MARK,
		__entry->cpuid,
		__entry->ktime,
		__entry->tpid,
		__entry->tbuf)
);

TRACE_EVENT(tee_sched_end,
	TP_PROTO(struct tee_trace_struct *p),
	TP_ARGS(p),

	TP_STRUCT__entry(
		__field(u32, cpuid)
		__array(char, ktime, TASK_COMM_LEN)
		__field(u32, tpid)
		__array(char, tbuf, TASK_COMM_LEN)
	),
	TP_fast_assign(
		__entry->cpuid = p->cpuid;
		memcpy(__entry->ktime, p->ktimestamp, TASK_COMM_LEN);
		__entry->tpid = p->tee_pid;
		memcpy(__entry->tbuf, p->tee_postfix, TASK_COMM_LEN);
	),
	TP_printk("E|%d|%s[%u][%s]%03x-%s",
		BEGINED_PID,
		TEE_TRACING_MARK,
		__entry->cpuid,
		__entry->ktime,
		__entry->tpid,
		__entry->tbuf)
);


#endif /*_TRACE_TEE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_tee
/* This part must be outside protection */
#include <trace/define_trace.h>
