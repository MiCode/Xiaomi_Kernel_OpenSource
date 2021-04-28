/*
 * Copyright (C) 2020 MediaTek Inc.
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
#define TRACE_SYSTEM cache_ctrl

#if !defined(_TRACE_CACHE_CTRL_H) || defined(TRACE_HEARDER_MULTI_READ)
#define _TRACE_CACHE_CTRL_H

#include <linux/sched.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#endif /*_TRACE_TASK_TURBO_H */

TRACE_EVENT(skip_cache_control,

	TP_PROTO(struct task_struct *next, bool is_bw_congested),

	TP_ARGS(next, is_bw_congested),

	TP_STRUCT__entry(
		__array(char,	next_comm, TASK_COMM_LEN)
		__field(pid_t,	next_pid)
		__field(int,	next_prio)
		__field(int,	next_st_cgrp_id)
		__field(int,	next_oom_score_adj)
		__field(bool,	is_mem_stall)
		__field(bool,	is_bw_congested)
	),

	TP_fast_assign(
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->next_pid	    = next->pid;
		__entry->next_prio	    = next->prio;
		__entry->next_st_cgrp_id    = next->cgroups->subsys[3]->cgroup->id;
		__entry->next_oom_score_adj = next->signal->oom_score_adj;
		__entry->is_mem_stall       = (next->flags & PF_MEMSTALL);
		__entry->is_bw_congested    = is_bw_congested;
	),

	TP_printk("next:comm=%s pid=%d prio=%d st_id=%d adj=%d mstl=%d cgst=%d",
		__entry->next_comm,
		__entry->next_pid,
		__entry->next_prio,
		__entry->next_st_cgrp_id,
		__entry->next_oom_score_adj,
		__entry->is_mem_stall,
		__entry->is_bw_congested)
);

TRACE_EVENT(apply_cache_control,

	TP_PROTO(struct task_struct *next, int cpu, int partition_group),

	 TP_ARGS(next, cpu, partition_group),

	TP_STRUCT__entry(
		__array(char,   next_comm, TASK_COMM_LEN)
		__field(pid_t,  next_pid)
		__field(int,    next_prio)
		__field(int,    next_st_cgrp_id)
		__field(int,    cpu)
		__field(int,    partition_group)
		),

	TP_fast_assign(
		memcpy(__entry->next_comm, next->comm, TASK_COMM_LEN);
		__entry->next_pid	    = next->pid;
		__entry->next_prio	    = next->prio;
		__entry->next_st_cgrp_id    = next->cgroups->subsys[3]->cgroup->id;
		__entry->cpu		    = cpu;
		__entry->partition_group    = partition_group;
	),

	TP_printk("next:comm=%s pid=%d prio=%d st_id=%d cpu=%d part_grp=%d",
		__entry->next_comm,
		__entry->next_pid,
		__entry->next_prio,
		__entry->next_st_cgrp_id,
		__entry->cpu,
		__entry->partition_group)
);

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_cache_ctrl
/* This part must be outside protection */
#include <trace/define_trace.h>
