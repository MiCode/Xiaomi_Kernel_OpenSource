/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpuqos_v3

#if !defined(_CPUQOS_V3_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _CPUQOS_V3_TRACE_H
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

/*
 * Tracepoint for per-cpu partid
 */
TRACE_EVENT(cpuqos_cpu_partid,

	TP_PROTO(int cpu, int pid, int css_id,
		int old_partid, int new_partid,
		u64 v1, u64 v2,
		int rank, int cpuqos_perf_mode),

	TP_ARGS(cpu, pid, css_id,
		old_partid, new_partid,
		v1, v2,
		rank, cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(int, css_id)
		__field(int, old_partid)
		__field(int, new_partid)
		__field(u64, v1)
		__field(u64, v2)
		__field(int, rank)
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->pid		= pid;
		__entry->css_id		= css_id;
		__entry->old_partid	= old_partid;
		__entry->new_partid	= new_partid;
		__entry->v1		= v1;
		__entry->v2		= v2;
		__entry->rank		= rank;
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("cpu=%d, p=%d, css_id=%d, old_part=%d, new_part=%d, v1=0x%llx, v2=0x%llx, rank=%d, mode=%d",
		__entry->cpu, __entry->pid, __entry->css_id,
		__entry->old_partid, __entry->new_partid,
		__entry->v1, __entry->v2,
		__entry->rank, __entry->cpuqos_perf_mode)
);

TRACE_EVENT(cpuqos_set_ct_group,

	TP_PROTO(int group_id, int css_id, int set,
		int old_partid, int new_partid,
		int cpuqos_perf_mode),

	TP_ARGS(group_id, css_id, set,
		old_partid, new_partid, cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, group_id)
		__field(int, css_id)
		__field(int, set)
		__field(int, old_partid)
		__field(int, new_partid)
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->group_id	= group_id;
		__entry->css_id		= css_id;
		__entry->set		= set;
		__entry->old_partid	= old_partid;
		__entry->new_partid	= new_partid;
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("group_id=%d, css_id=%d, set=%d, old_partid=%d, new_partid=%d, mode=%d",
		__entry->group_id, __entry->css_id, __entry->set,
		__entry->old_partid, __entry->new_partid,
		__entry->cpuqos_perf_mode)

);

TRACE_EVENT(cpuqos_set_ct_task,

	TP_PROTO(int pid, int css_id, int set,
		int old_partid, int new_partid,
		int rank, int cpuqos_perf_mode),

	TP_ARGS(pid, css_id, set, old_partid, new_partid,
		rank, cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, pid)
		__field(int, css_id)
		__field(int, set)
		__field(int, old_partid)
		__field(int, new_partid)
		__field(int, rank)
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->pid		= pid;
		__entry->css_id		= css_id;
		__entry->set		= set;
		__entry->old_partid	= old_partid;
		__entry->new_partid	= new_partid;
		__entry->rank		= rank;
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("p=%d, css_id=%d, set=%d, old_partid=%d, new_partid=%d, rank=%d, mode=%d",
		__entry->pid, __entry->css_id, __entry->set,
		__entry->old_partid, __entry->new_partid,
		__entry->rank, __entry->cpuqos_perf_mode)

);

TRACE_EVENT(cpuqos_set_cpuqos_mode,

	TP_PROTO(int cpuqos_perf_mode),

	TP_ARGS(cpuqos_perf_mode),

	TP_STRUCT__entry(
		__field(int, cpuqos_perf_mode)
	),

	TP_fast_assign(
		__entry->cpuqos_perf_mode = cpuqos_perf_mode;
	),

	TP_printk("cpuqos_mode=%d", __entry->cpuqos_perf_mode)

);



#endif /* _CPUQOS_V3_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE cpuqos_v3_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
