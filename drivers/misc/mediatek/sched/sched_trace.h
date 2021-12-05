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

#ifdef CREATE_TRACE_POINTS
int sched_cgroup_state(struct task_struct *p, int subsys_id)
{
#ifdef CONFIG_CGROUPS
	int cgrp_id = -1;
	struct cgroup_subsys_state *css;

	rcu_read_lock();
	css = task_css(p, subsys_id);
	if (!css)
		goto out;

	cgrp_id = css->id;

out:
	rcu_read_unlock();

	return cgrp_id;
#else
	return -1;
#endif
}
#endif

TRACE_EVENT(sched_select_task_rq,

	TP_PROTO(struct task_struct *tsk,
		int policy, int prev_cpu, int target_cpu,
		bool prefer, int sync_flag),

	TP_ARGS(tsk, policy, prev_cpu, target_cpu, prefer, sync_flag),

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
		__field(int, cpuctl_grp_id)
		__field(int, cpuset_grp_id)
		),

	TP_fast_assign(
		__entry->pid        = tsk->pid;
		__entry->policy     = policy;
		__entry->prev_cpu   = prev_cpu;
		__entry->target_cpu = target_cpu;
		__entry->task_util      = task_util(tsk);
		__entry->task_util_est  = task_util_est(tsk);
		__entry->boost          = uclamp_task_util(tsk);
		__entry->task_mask      = tsk->cpus_ptr->bits[0];
		__entry->prefer         = prefer;
		__entry->sync_flag     = sync_flag;
		__entry->cpuctl_grp_id = sched_cgroup_state(tsk, cpu_cgrp_id);
		__entry->cpuset_grp_id = sched_cgroup_state(tsk, cpuset_cgrp_id);
		),

	TP_printk("pid=%4d policy=0x%08x pre-cpu=%d target=%d util=%d util_est=%d uclamp=%d mask=0x%lx latency_sensitive=%d sync=%d cpuctl=%d cpuset=%d",
		__entry->pid,
		__entry->policy,
		__entry->prev_cpu,
		__entry->target_cpu,
		__entry->task_util,
		__entry->task_util_est,
		__entry->boost,
		__entry->task_mask,
		__entry->prefer,
		__entry->sync_flag,
		__entry->cpuctl_grp_id,
		__entry->cpuset_grp_id)
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
		int cpu, unsigned long util_cfs, unsigned long util_cfs_energy,
		unsigned long cpu_util),


	TP_ARGS(dst_cpu, max_util, sum_util, cpu, util_cfs, util_cfs_energy, cpu_util),

	TP_STRUCT__entry(
		__field(int, dst_cpu)
		__field(unsigned long, max_util)
		__field(unsigned long, sum_util)
		__field(int, cpu)
		__field(unsigned long, util_cfs)
		__field(unsigned long, util_cfs_energy)
		__field(unsigned long, cpu_util)
		),

	TP_fast_assign(
		__entry->dst_cpu    = dst_cpu;
		__entry->max_util   = max_util;
		__entry->sum_util   = sum_util;
		__entry->cpu        = cpu;
		__entry->util_cfs   = util_cfs;
		__entry->util_cfs_energy   = util_cfs_energy;
		__entry->cpu_util   = cpu_util;
		),

	TP_printk("dst_cpu=%d max_util=%lu sum_util=%lu cpu=%d util_cfs=%lu util_cfs_energy=%lu cpu_util=%lu",
		__entry->dst_cpu,
		__entry->max_util,
		__entry->sum_util,
		__entry->cpu,
		__entry->util_cfs,
		__entry->util_cfs_energy,
		__entry->cpu_util)
);

TRACE_EVENT(sched_find_energy_efficient_cpu,

	TP_PROTO(unsigned long best_delta,
		int best_energy_cpu, int best_idle_cpu, int idle_max_spare_cap_cpu,
		int sys_max_spare_cap_cpu),

	TP_ARGS(best_delta, best_energy_cpu, best_idle_cpu,
		idle_max_spare_cap_cpu, sys_max_spare_cap_cpu),

	TP_STRUCT__entry(
		__field(unsigned long, best_delta)
		__field(int, best_energy_cpu)
		__field(int, best_idle_cpu)
		__field(int, idle_max_spare_cap_cpu)
		__field(int, sys_max_spare_cap_cpu)
		),

	TP_fast_assign(
		__entry->best_delta      = best_delta;
		__entry->best_energy_cpu = best_energy_cpu;
		__entry->best_idle_cpu   = best_idle_cpu;
		__entry->idle_max_spare_cap_cpu = idle_max_spare_cap_cpu;
		__entry->sys_max_spare_cap_cpu = sys_max_spare_cap_cpu;
		),

	TP_printk("best_delta=%lu best_energy_cpu=%d best_idle_cpu=%d idle_max_spare_cap_cpu=%d sys_max_spare_cpu=%d",
		__entry->best_delta,
		__entry->best_energy_cpu,
		__entry->best_idle_cpu,
		__entry->idle_max_spare_cap_cpu,
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

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_next_new_balance,

	TP_PROTO(u64 now_ns, u64 next_balance),

	TP_ARGS(now_ns, next_balance),

	TP_STRUCT__entry(
		__field(u64, now_ns)
		__field(u64, next_balance)
		),

	TP_fast_assign(
		__entry->now_ns = now_ns;
		__entry->next_balance = next_balance;
		),

	TP_printk("now_ns=%llu next_balance=%lld",
		__entry->now_ns, __entry->next_balance)
);

#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sched_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
