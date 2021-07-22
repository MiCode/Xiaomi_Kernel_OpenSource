/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM scheduler

#if !defined(_TRACE_SCHEDULER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SCHEDULER_H
#include <linux/string.h>
#include <linux/types.h>
#include <linux/tracepoint.h>

#define LB_FAIL                   (0x01)
#define LB_SYNC                   (0x02)
#define LB_ZERO_UTIL              (0x04)
#define LB_PREV                   (0x08)
#define LB_LATENCY_SENSITIVE      (0x10)
#define LB_NOT_PREV               (0x20)
#define LB_BEST_ENERGY_CPU        (0x40)

TRACE_EVENT(sched_select_task_rq,

	TP_PROTO(struct task_struct *tsk,
		int policy, int prev_cpu, int target_cpu,
		int task_util, int task_util_est, int boost, bool prefer, int sync_flag),

	TP_ARGS(tsk, policy, prev_cpu, target_cpu, task_util, task_util_est, boost,
		prefer, sync_flag),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, prev_cpu)
		__field(int, target_cpu)
		__field(int, task_util)
		__field(int, task_util_est)
		__field(int, boost)
		__field(long, task_mask)
		__field(bool, prefer)
		__field(int, sync_flag)
		),

	TP_fast_assign(
		__entry->pid        = tsk->pid;
		__entry->policy     = policy;
		__entry->prev_cpu   = prev_cpu;
		__entry->target_cpu = target_cpu;
		__entry->task_util      = task_util;
		__entry->task_util_est  = task_util_est;
		__entry->boost          = boost;
		__entry->task_mask      = tsk->cpus_ptr->bits[0];
		__entry->prefer         = prefer;
		__entry->sync_flag     = sync_flag;
		),

	TP_printk("pid=%4d policy=0x%08x pre-cpu=%d target=%d util=%d util_est=%d uclamp=%d mask=0x%lx latency_sensitive=%d sync=%d",
		__entry->pid,
		__entry->policy,
		__entry->prev_cpu,
		__entry->target_cpu,
		__entry->task_util,
		__entry->task_util_est,
		__entry->boost,
		__entry->task_mask,
		__entry->prefer,
		__entry->sync_flag)
);

TRACE_EVENT(sched_compute_energy,

	TP_PROTO(int dst_cpu, struct cpumask *pd_mask,
		unsigned long energy, unsigned long max_util, unsigned long sum_util),

	TP_ARGS(dst_cpu, pd_mask, energy, max_util, sum_util),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(long, cpu_mask )
		__field(unsigned long, energy)
		__field(unsigned long, max_util)
		__field(unsigned long, sum_util)
		),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->cpu_mask   = pd_mask->bits[0];
		__entry->energy     = energy;
		__entry->max_util   = max_util;
		__entry->sum_util   = sum_util;
		),

	TP_printk("dst_cpu=%d mask=0x%lx energy=%lu max_util=%lu sum_util=%lu",
		__entry->dst_cpu,
		__entry->cpu_mask,
		__entry->energy,
		__entry->max_util,
		__entry->sum_util)
);

TRACE_EVENT(sched_energy_util,

	TP_PROTO(int dst_cpu,
		unsigned long max_util, unsigned long sum_util,
		int cpu, unsigned long util_cfs, unsigned long cpu_util),

	TP_ARGS(dst_cpu, max_util, sum_util, cpu, util_cfs, cpu_util),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(unsigned long, max_util)
		__field(unsigned long, sum_util)
		__field(int, cpu)
		__field(unsigned long, util_cfs)
		__field(unsigned long, cpu_util)
		),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->max_util   = max_util;
		__entry->sum_util   = sum_util;
		__entry->cpu        = cpu;
		__entry->util_cfs   = util_cfs;
		__entry->cpu_util   = cpu_util;
		),

	TP_printk("dst_cpu=%d max_util=%lu sum_util=%lu cpu=%d util_cfs=%lu cpu_util=%lu",
		__entry->dst_cpu,
		__entry->max_util,
		__entry->sum_util,
		__entry->cpu,
		__entry->util_cfs,
		__entry->cpu_util)
);

TRACE_EVENT(sched_find_energy_efficient_cpu,

	TP_PROTO(unsigned long prev_delta, unsigned long best_delta,
		int best_energy_cpu, int best_idle_cpu, int max_spare_cap_cpu_ls,
		int sys_max_spare_cap_cpu),

	TP_ARGS(prev_delta, best_delta, best_energy_cpu, best_idle_cpu,
		max_spare_cap_cpu_ls, sys_max_spare_cap_cpu),

	TP_STRUCT__entry(
		__field(unsigned long, prev_delta)
		__field(unsigned long, best_delta)
		__field(int, best_energy_cpu)
		__field(int, best_idle_cpu)
		__field(int, max_spare_cap_cpu_ls)
		__field(int, sys_max_spare_cap_cpu)
		),

	TP_fast_assign(
		__entry->prev_delta      = prev_delta;
		__entry->best_delta      = best_delta;
		__entry->best_energy_cpu = best_energy_cpu;
		__entry->best_idle_cpu   = best_idle_cpu;
		__entry->max_spare_cap_cpu_ls = max_spare_cap_cpu_ls;
		__entry->sys_max_spare_cap_cpu = sys_max_spare_cap_cpu;
		),

	TP_printk("prev_delta=%lu best_delta=%lu best_energy_cpu=%d best_idle_cpu=%d max_spare_cap_cpu_ls=%d sys_max_spare_cpu=%d",
		__entry->prev_delta,
		__entry->best_delta,
		__entry->best_energy_cpu,
		__entry->best_idle_cpu,
		__entry->max_spare_cap_cpu_ls,
		__entry->sys_max_spare_cap_cpu)
);

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_force_migrate,

	TP_PROTO(struct task_struct *tsk, int dest, int force),

	TP_ARGS(tsk, dest, force),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,  dest)
		__field(int,  force)
		),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid   = tsk->pid;
		__entry->dest  = dest;
		__entry->force = force;
		),

	TP_printk("comm=%s pid=%d dest=%d force=%d",
		__entry->comm, __entry->pid,
		__entry->dest, __entry->force)
);

#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sched_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
