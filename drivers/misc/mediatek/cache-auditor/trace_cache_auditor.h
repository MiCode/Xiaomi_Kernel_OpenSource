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
#define TRACE_SYSTEM cache_auditor

#if !defined(_TRACE_CACHE_AUDITOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CACHE_AUDITOR_H

#include <linux/types.h>
#include <linux/tracepoint.h>
TRACE_EVENT(ca_aggregate_prev,

	TP_PROTO(unsigned long long delta, struct perf_event *event,
		u64 counter),

	TP_ARGS(delta, event, counter),

	TP_STRUCT__entry(
		__field(unsigned long long, delta)
		__field(u32, type)
		__field(u64, config)
		__field(u64, counter)
	),

	TP_fast_assign(
		__entry->delta = delta;
		__entry->type = event->attr.type;
		__entry->config = event->attr.config;
		__entry->counter = counter;
	),

	TP_printk("[%llu] type=%x config=0x%llx counter=%llu",
		  __entry->delta,
		  __entry->type,
		  __entry->config,
		  __entry->counter)
);

TRACE_EVENT(ca_exec_summary,

	TP_PROTO(unsigned long long delta, u64 *counters),

	TP_ARGS(delta, counters),

	TP_STRUCT__entry(
		__field(unsigned long long, delta)
		__field(u64, stall)
		__field(u64, cycle)
		__field(u64, l3_miss)
		__field(u64, inst)
	),

	TP_fast_assign(
		__entry->delta = delta;
		__entry->stall = counters[0] + counters[1];
		__entry->cycle = counters[2];
		__entry->l3_miss = counters[3];
		__entry->inst = counters[4];
	),

	TP_printk("[%llu] cpi=%llu stall_ratio=%llu miss_per_inst=%llu",
		  __entry->delta,
		  __entry->cycle * 1024 / __entry->inst,
		  __entry->stall * 1024 / __entry->cycle,
		  __entry->l3_miss * 1024 / __entry->inst)
);

TRACE_EVENT(debug,

	TP_PROTO(int number),

	TP_ARGS(number),

	TP_STRUCT__entry(
		__field(int, nr)
	),

	TP_fast_assign(
		__entry->nr = number;
	),

	TP_printk("%d", __entry->nr)
);

TRACE_EVENT(ca_apply_control,

	TP_PROTO(int cpu, int badness),

	TP_ARGS(cpu, badness),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, badness)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->badness = badness;
	),

	TP_printk("cpu=%d badness=%d", __entry->cpu, __entry->badness)
);

TRACE_EVENT(ca_audit_cache,

	TP_PROTO(int in_auditing),

	TP_ARGS(in_auditing),

	TP_STRUCT__entry(
		__field(int, in_auditing)
	),

	TP_fast_assign(
		__entry->in_auditing = in_auditing;
	),

	TP_printk("in_auditing=%d", __entry->in_auditing)
);

/*
 * For tracing format
 */
TRACE_EVENT(ca_print_stall,

	TP_PROTO(struct task_struct *task, int grp_id,
		u64 stall_ratio),

	TP_ARGS(task, grp_id, stall_ratio),

	TP_STRUCT__entry(
		__array(char, prev_comm, TASK_COMM_LEN)
		__field(pid_t, prev_pid)
		__field(int, grp_id)
		__field(u64, stall_ratio)
	),

	TP_fast_assign(
		memcpy(__entry->prev_comm, task->comm, TASK_COMM_LEN);
		__entry->prev_pid	= task->pid;
		__entry->grp_id = grp_id;
		__entry->stall_ratio = stall_ratio;
	),

	TP_printk("C|50000%d|[stall]%s(%d)|%llu",
			__entry->grp_id, __entry->prev_comm, __entry->prev_pid,
			__entry->stall_ratio)
);

TRACE_EVENT(ca_print_badness,

	TP_PROTO(struct task_struct *task, int grp_id, u64 badness),

	TP_ARGS(task, grp_id, badness),

	TP_STRUCT__entry(
		__array(char, prev_comm, TASK_COMM_LEN)
		__field(pid_t, prev_pid)
		__field(int, grp_id)
		__field(u64, badness)
	),

	TP_fast_assign(
		memcpy(__entry->prev_comm, task->comm, TASK_COMM_LEN);
		__entry->prev_pid = task->pid;
		__entry->grp_id = grp_id;
		__entry->badness = badness;
	),

	TP_printk("C|60000%d|[badness]%s(%d)|%llu",
			__entry->grp_id, __entry->prev_comm, __entry->prev_pid,
			__entry->badness)
);

#endif /* _TRACE_CACHE_AUDITOR_H */

#undef TRACE_INCLUDE_PATH

// Current path setting
#define TRACE_INCLUDE_PATH .
// current file name w/o suffix
#define TRACE_INCLUDE_FILE trace_cache_auditor
#include <trace/define_trace.h>
