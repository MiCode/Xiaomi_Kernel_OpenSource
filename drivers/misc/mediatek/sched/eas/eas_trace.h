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

#if IS_ENABLED(CONFIG_MTK_SCHED_BIG_TASK_ROTATE)
/*
 * Tracepoint for big task rotation
 */
TRACE_EVENT(sched_big_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid,
		int fin),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid, fin),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, src_pid)
		__field(int, dst_pid)
		__field(int, fin)
	),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->src_pid	= src_pid;
		__entry->dst_pid	= dst_pid;
		__entry->fin		= fin;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d fin=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid,
		__entry->fin)
);
#endif

TRACE_EVENT(sched_leakage,

	TP_PROTO(int cpu, int opp, unsigned int temp,
		unsigned long cpu_static_pwr, unsigned long static_pwr, unsigned long sum_cap),

	TP_ARGS(cpu, opp, temp, cpu_static_pwr, static_pwr, sum_cap),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(unsigned int, temp)
		__field(unsigned long, cpu_static_pwr)
		__field(unsigned long, static_pwr)
		__field(unsigned long, sum_cap)
		),

	TP_fast_assign(
		__entry->cpu       = cpu;
		__entry->opp        = opp;
		__entry->temp       = temp;
		__entry->cpu_static_pwr = cpu_static_pwr;
		__entry->static_pwr = static_pwr;
		__entry->sum_cap = sum_cap;
		),

	TP_printk("cpu=%d opp=%d temp=%lu lkg=%lu sum_lkg=%lu, sum_cap=%lu",
		__entry->cpu,
		__entry->opp,
		__entry->temp,
		__entry->cpu_static_pwr,
		__entry->static_pwr,
		__entry->sum_cap)
);

TRACE_EVENT(sched_em_cpu_energy,

	TP_PROTO(int idx, unsigned long freq, unsigned long cost, unsigned long scale_cpu,
		unsigned long dyn_pwr, unsigned long static_pwr),

	TP_ARGS(idx, freq, cost, scale_cpu, dyn_pwr, static_pwr),

	TP_STRUCT__entry(
		__field(int, idx)
		__field(unsigned long, freq)
		__field(unsigned long, cost)
		__field(unsigned long, scale_cpu)
		__field(unsigned long, dyn_pwr)
		__field(unsigned long, static_pwr)
		),

	TP_fast_assign(
		__entry->idx        = idx;
		__entry->freq       = freq;
		__entry->cost       = cost;
		__entry->scale_cpu  = scale_cpu;
		__entry->dyn_pwr    = dyn_pwr;
		__entry->static_pwr = static_pwr;
		),
#if IS_ENABLED(CONFIG_MTK_OPP_CAP_INFO)
	TP_printk("idx=%d freq=%lu pwr_eff=%lu scale_cpu=%lu dyn_pwr=%lu static_pwr=%lu",
		__entry->idx,
		__entry->freq,
		__entry->cost,
		__entry->scale_cpu,
		__entry->dyn_pwr,
		__entry->static_pwr)
#else
	TP_printk("idx=%d freq=%lu ps->cost=%lu scale_cpu=%lu dyn_pwr=%lu static_pwr=%lu",
		__entry->idx,
		__entry->freq,
		__entry->cost,
		__entry->scale_cpu,
		__entry->dyn_pwr,
		__entry->static_pwr)
#endif
);

TRACE_EVENT(sched_calc_pwr_eff,

	TP_PROTO(int cpu, unsigned long cpu_util, int opp, unsigned long cap,
		unsigned long dyn_pwr_eff, unsigned long static_pwr_eff, unsigned long pwr_eff),

	TP_ARGS(cpu, cpu_util, opp, cap, dyn_pwr_eff, static_pwr_eff, pwr_eff),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, cpu_util)
		__field(int, opp)
		__field(unsigned long, cap)
		__field(unsigned long, dyn_pwr_eff)
		__field(unsigned long, static_pwr_eff)
		__field(unsigned long, pwr_eff)
		),

	TP_fast_assign(
		__entry->cpu            = cpu;
		__entry->cpu_util       = cpu_util;
		__entry->opp            = opp;
		__entry->cap            = cap;
		__entry->dyn_pwr_eff    = dyn_pwr_eff;
		__entry->static_pwr_eff = static_pwr_eff;
		__entry->pwr_eff        = pwr_eff;
		),

	TP_printk("cpu=%d cpu_util=%lu opp=%d cap=%lu dyn_pwr_eff=%lu static_pwr_eff=%lu pwr_eff=%u",
		__entry->cpu,
		__entry->cpu_util,
		__entry->opp,
		__entry->cap,
		__entry->dyn_pwr_eff,
		__entry->static_pwr_eff,
		__entry->pwr_eff)
);

TRACE_EVENT(sched_find_busiest_group,

	TP_PROTO(int src_cpu, int dst_cpu,
		int out_balance, int reason),

	TP_ARGS(src_cpu, dst_cpu, out_balance, reason),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, out_balance)
		__field(int, reason)
		),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->out_balance	= out_balance;
		__entry->reason		= reason;
		),

	TP_printk("src_cpu=%d dst_cpu=%d out_balance=%d reason=0x%x",
		__entry->src_cpu,
		__entry->dst_cpu,
		__entry->out_balance,
		__entry->reason)
);

TRACE_EVENT(sched_cpu_overutilized,

	TP_PROTO(int cpu, struct cpumask *pd_mask,
		unsigned long sum_util, unsigned long sum_cap,
		int overutilized),

	TP_ARGS(cpu, pd_mask, sum_util, sum_cap,
		overutilized),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(long, cpu_mask)
		__field(unsigned long, sum_util)
		__field(unsigned long, sum_cap)
		__field(int, overutilized)
		),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->cpu_mask	= pd_mask->bits[0];
		__entry->sum_util	= sum_util;
		__entry->sum_cap	= sum_cap;
		__entry->overutilized	= overutilized;
		),

	TP_printk("cpu=%d mask=0x%lx sum_util=%lu sum_cap=%lu overutilized=%d",
		__entry->cpu,
		__entry->cpu_mask,
		__entry->sum_util,
		__entry->sum_cap,
		__entry->overutilized)
);

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_frequency_limits,

	TP_PROTO(int cpu_id, int freq_thermal),

	TP_ARGS(cpu_id, freq_thermal),

	TP_STRUCT__entry(
		__field(int,  cpu_id)
		__field(int,  freq_thermal)
		),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
		__entry->freq_thermal = freq_thermal;
		),

	TP_printk("cpu=%d thermal=%lu",
		__entry->cpu_id, __entry->freq_thermal)
);

TRACE_EVENT(sched_queue_task,
	TP_PROTO(int cpu, int pid, int enqueue,
		unsigned long cfs_util,
		unsigned int min, unsigned int max,
		unsigned int task_min, unsigned int task_max),
	TP_ARGS(cpu, pid, enqueue, cfs_util, min, max, task_min, task_max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, pid)
		__field(int, enqueue)
		__field(unsigned long, cfs_util)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(unsigned int, task_min)
		__field(unsigned int, task_max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->pid = pid;
		__entry->enqueue = enqueue;
		__entry->cfs_util = cfs_util;
		__entry->min = min;
		__entry->max = max;
		__entry->task_min = task_min;
		__entry->task_max = task_max;
	),
	TP_printk(
		"cpu=%d pid=%d enqueue=%d cfs_util=%lu min=%u max=%u task_min=%u task_max=%u",
		__entry->cpu,
		__entry->pid,
		__entry->enqueue,
		__entry->cfs_util,
		__entry->min,
		__entry->max,
		__entry->task_min,
		__entry->task_max)
);

TRACE_EVENT(sched_task_util,
	TP_PROTO(int pid,
		unsigned long util,
		unsigned int util_enqueued, unsigned int util_ewma),
	TP_ARGS(pid, util, util_enqueued, util_ewma),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(unsigned long, util)
		__field(unsigned int, util_enqueued)
		__field(unsigned int, util_ewma)
	),
	TP_fast_assign(
		__entry->pid = pid;
		__entry->util = util;
		__entry->util_enqueued = util_enqueued;
		__entry->util_ewma = util_ewma;
	),
	TP_printk(
		"pid=%d util=%lu util_enqueued=%u util_ewma=%u",
		__entry->pid,
		__entry->util,
		__entry->util_enqueued,
		__entry->util_ewma)
);

TRACE_EVENT(sched_task_uclamp,
	TP_PROTO(int pid, unsigned long util,
		unsigned int active,
		unsigned int min, unsigned int max,
		unsigned int min_ud, unsigned int min_req,
		unsigned int max_ud, unsigned int max_req),
	TP_ARGS(pid, util, active,
		min, max,
		min_ud, min_req,
		max_ud, max_req),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(unsigned long, util)
		__field(unsigned int, active)
		__field(unsigned int, min)
		__field(unsigned int, max)
		__field(unsigned int, min_ud)
		__field(unsigned int, min_req)
		__field(unsigned int, max_ud)
		__field(unsigned int, max_req)
	),
	TP_fast_assign(
		__entry->pid = pid;
		__entry->util = util;
		__entry->active = active;
		__entry->min = min;
		__entry->max = max;
		__entry->min_ud = min_ud;
		__entry->min_req = min_req;
		__entry->max_ud = max_ud;
		__entry->max_req = max_req;
	),
	TP_printk(
		"pid=%d util=%lu active=%u min=%u max=%u min_ud=%u min_req=%u max_ud=%u max_req=%u",
		__entry->pid,
		__entry->util,
		__entry->active,
		__entry->min,
		__entry->max,
		__entry->min_ud,
		__entry->min_req,
		__entry->max_ud,
		__entry->max_req)
);

#ifdef CREATE_TRACE_POINTS
int sched_cgroup_state_rt(struct task_struct *p, int subsys_id)
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

TRACE_EVENT(sched_select_task_rq_rt,
	TP_PROTO(struct task_struct *tsk, int policy,
		int target_cpu, unsigned int idle_cpus, unsigned int cfs_cpus,
		int sd_flag, bool sync),
	TP_ARGS(tsk, policy, target_cpu, idle_cpus, cfs_cpus,
		sd_flag, sync),
	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, target_cpu)
		__field(unsigned int,  idle_cpus)
		__field(unsigned int,  cfs_cpus)
		__field(unsigned long, uclamp_min)
		__field(unsigned long, uclamp_max)
		__field(int, sd_flag)
		__field(bool, sync)
		__field(long, task_mask)
		__field(int, cpuctl_grp_id)
		__field(int, cpuset_grp_id)
		__field(long, act_mask)
	),
	TP_fast_assign(
		__entry->pid = tsk->pid;
		__entry->policy = policy;
		__entry->target_cpu = target_cpu;
		__entry->idle_cpus  = idle_cpus;
		__entry->cfs_cpus   = cfs_cpus;
		__entry->uclamp_min = uclamp_eff_value(tsk, UCLAMP_MIN);
		__entry->uclamp_max = uclamp_eff_value(tsk, UCLAMP_MAX);
		__entry->sd_flag = sd_flag;
		__entry->sync = sync;
		__entry->task_mask = tsk->cpus_ptr->bits[0];
		__entry->cpuctl_grp_id = sched_cgroup_state_rt(tsk, cpu_cgrp_id);
		__entry->cpuset_grp_id = sched_cgroup_state_rt(tsk, cpuset_cgrp_id);
		__entry->act_mask = cpu_active_mask->bits[0];
	),
	TP_printk(
		"pid=%4d policy=0x%08x target=%d idle_cpus=0x%x cfs_cpus=0x%x uclamp_min=%lu uclamp_max=%lu sd_flag=%d sync=%d mask=0x%lx cpuctl=%d cpuset=%d act_mask=0x%lx",
		__entry->pid,
		__entry->policy,
		__entry->target_cpu,
		__entry->idle_cpus,
		__entry->cfs_cpus,
		__entry->uclamp_min,
		__entry->uclamp_max,
		__entry->sd_flag,
		__entry->sync,
		__entry->task_mask,
		__entry->cpuctl_grp_id,
		__entry->cpuset_grp_id,
		__entry->act_mask)
);

TRACE_EVENT(sched_aware_energy_rt,
	TP_PROTO(int target_cpu, unsigned long this_pwr_eff, unsigned long pwr_eff,
			unsigned int task_util),
	TP_ARGS(target_cpu, this_pwr_eff, pwr_eff, task_util),
	TP_STRUCT__entry(
		__field(int, target_cpu)
		__field(unsigned long, this_pwr_eff)
		__field(unsigned long, pwr_eff)
		__field(unsigned int, task_util)
	),
	TP_fast_assign(
		__entry->target_cpu	= target_cpu;
		__entry->this_pwr_eff	= this_pwr_eff;
		__entry->pwr_eff	= pwr_eff;
		__entry->task_util	= task_util;
	),
	TP_printk("target=%d this_pwr_eff=%lu pwr_eff=%lu util=%u",
		__entry->target_cpu,
		__entry->this_pwr_eff,
		__entry->pwr_eff,
		__entry->task_util)
);

TRACE_EVENT(sched_next_update_thermal_headroom,
	TP_PROTO(unsigned long now, unsigned long next_update_thermal),
	TP_ARGS(now, next_update_thermal),
	TP_STRUCT__entry(
		__field(unsigned long, now)
		__field(unsigned long, next_update_thermal)
	),
	TP_fast_assign(
		__entry->now = now;
		__entry->next_update_thermal = next_update_thermal;
	),
	TP_printk(
		"now_tick=%lu next_update_thermal=%lu",
		__entry->now,
		__entry->next_update_thermal)
);

TRACE_EVENT(sched_newly_idle_balance_interval,
	TP_PROTO(unsigned int interval_us),
	TP_ARGS(interval_us),
	TP_STRUCT__entry(
		__field(unsigned int, interval_us)
	),
	TP_fast_assign(
		__entry->interval_us = interval_us;
	),
	TP_printk(
		"interval_us=%u",
		__entry->interval_us)
);

TRACE_EVENT(sched_headroom_interval_tick,
	TP_PROTO(unsigned int tick),
	TP_ARGS(tick),
	TP_STRUCT__entry(
		__field(unsigned int, tick)
	),
	TP_fast_assign(
		__entry->tick = tick;
	),
	TP_printk(
		"interval =%u",
		__entry->tick)
);

#if IS_ENABLED(CONFIG_MTK_CORE_PAUSE)
TRACE_EVENT(sched_pause_cpus,
	TP_PROTO(struct cpumask *req_cpus, struct cpumask *last_cpus,
			u64 start_time, unsigned char pause,
			int err, struct cpumask *pause_cpus),

	TP_ARGS(req_cpus, last_cpus, start_time, pause, err, pause_cpus),

	TP_STRUCT__entry(
		__field(unsigned int, req_cpus)
		__field(unsigned int, last_cpus)
		__field(unsigned int, time)
		__field(unsigned char, pause)
		__field(int, err)
		__field(unsigned int, pause_cpus)
		__field(unsigned int, online_cpus)
		__field(unsigned int, active_cpus)
	),

	TP_fast_assign(
		__entry->req_cpus    = cpumask_bits(req_cpus)[0];
		__entry->last_cpus = cpumask_bits(last_cpus)[0];
		__entry->time        = div64_u64(sched_clock() - start_time, 1000);
		__entry->pause	     = pause;
		__entry->err         = err;
		__entry->pause_cpus    = cpumask_bits(pause_cpus)[0];
		__entry->online_cpus    = cpumask_bits(cpu_online_mask)[0];
		__entry->active_cpus    = cpumask_bits(cpu_active_mask)[0];
	),

	TP_printk("req=0x%x cpus=0x%x time=%u us paused=%d, err=%d, pause=0x%x, online=0x%x, active=0x%x",
		  __entry->req_cpus, __entry->last_cpus, __entry->time, __entry->pause,
		  __entry->err, __entry->pause_cpus, __entry->online_cpus, __entry->active_cpus)
);

TRACE_EVENT(sched_set_cpus_allowed,
	TP_PROTO(struct task_struct *p, unsigned int *dest_cpu,
		struct cpumask *new_mask, struct cpumask *valid_mask,
		struct cpumask *pause_cpus),

	TP_ARGS(p, dest_cpu, new_mask, valid_mask, pause_cpus),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(unsigned int, dest_cpu)
		__field(bool, kthread)
		__field(unsigned int, new_mask)
		__field(unsigned int, valid_mask)
		__field(unsigned int, pause_cpus)
	),

	TP_fast_assign(
		__entry->pid = p->pid;
		__entry->dest_cpu = *dest_cpu;
		__entry->kthread = p->flags & PF_KTHREAD;
		__entry->new_mask = cpumask_bits(new_mask)[0];
		__entry->valid_mask = cpumask_bits(valid_mask)[0];
		__entry->pause_cpus = cpumask_bits(pause_cpus)[0];
	),

	TP_printk("p=%d, dest_cpu=%d, k=%d, new_mask=0x%lx, valid=0x%lx, pause=0x%lx",
		  __entry->pid, __entry->dest_cpu, __entry->kthread,
		  __entry->new_mask, __entry->valid_mask,
		  __entry->pause_cpus)
);

TRACE_EVENT(sched_find_lowest_rq,
	TP_PROTO(struct task_struct *tsk, int policy,
		int target_cpu,
		struct cpumask *avail_lowest_mask, struct cpumask *lowest_mask),

	TP_ARGS(tsk, policy, target_cpu, avail_lowest_mask, lowest_mask),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, target_cpu)
		__field(unsigned int, avail_lowest_mask)
		__field(unsigned int, lowest_mask)
	),

	TP_fast_assign(
		__entry->pid = tsk->pid;
		__entry->policy = policy;
		__entry->target_cpu = target_cpu;
		__entry->avail_lowest_mask = cpumask_bits(avail_lowest_mask)[0];
		__entry->lowest_mask = cpumask_bits(lowest_mask)[0];
	),

	TP_printk(
		"pid=%4d policy=0x%08x target=%d avail_lowest_mask=0x%lx lowest_mask=0x%lx",
		__entry->pid,
		__entry->policy,
		__entry->target_cpu,
		__entry->avail_lowest_mask,
		__entry->lowest_mask)
);
#endif

#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH eas
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE eas_trace
/* This part must be outside protection */
#include <trace/define_trace.h>
