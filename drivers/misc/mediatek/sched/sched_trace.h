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

TRACE_EVENT(sched_em_cpu_energy,

	TP_PROTO(int opp,
		unsigned long freq, unsigned long cost, unsigned long scale_cpu,
		unsigned long dyn_pwr, unsigned long static_pwr),

	TP_ARGS(opp, freq, cost, scale_cpu, dyn_pwr, static_pwr),

	TP_STRUCT__entry(
		__field(int, opp)
		__field(unsigned long, freq)
		__field(unsigned long, cost)
		__field(unsigned long, scale_cpu)
		__field(unsigned long, dyn_pwr)
		__field(unsigned long, static_pwr)
		),

	TP_fast_assign(
		__entry->opp        = opp;
		__entry->freq       = freq;
		__entry->cost       = cost;
		__entry->scale_cpu  = scale_cpu;
		__entry->dyn_pwr    = dyn_pwr;
		__entry->static_pwr = static_pwr;
		),

	TP_printk("opp=%d freq=%lu cost=%lu scale_cpu=%lu dyn_pwr=%lu static_pwr=%lu",
		__entry->opp,
		__entry->freq,
		__entry->cost,
		__entry->scale_cpu,
		__entry->dyn_pwr,
		__entry->static_pwr)
);

TRACE_EVENT(sched_leakage,

	TP_PROTO(int cpu, int opp, unsigned int temp,
		unsigned long cpu_static_pwr, unsigned long static_pwr),

	TP_ARGS(cpu, opp, temp, cpu_static_pwr, static_pwr),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, opp)
		__field(unsigned int, temp)
		__field(unsigned long, cpu_static_pwr)
		__field(unsigned long, static_pwr)
		),

	TP_fast_assign(
		__entry->cpu       = cpu;
		__entry->opp        = opp;
		__entry->temp       = temp;
		__entry->cpu_static_pwr = cpu_static_pwr;
		__entry->static_pwr = static_pwr;
		),

	TP_printk("cpu=%d opp=%d temp=%lu lkg=%lu sum_lkg=%lu",
		__entry->cpu,
		__entry->opp,
		__entry->temp,
		__entry->cpu_static_pwr,
		__entry->static_pwr)
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

/*
 * Tracepoint for task force migrations.
 */
TRACE_EVENT(sched_frequency_limits,

	TP_PROTO(int cpu_id, int freq_thermal, int freq_max, int freq_ceiling),

	TP_ARGS(cpu_id, freq_thermal, freq_max, freq_ceiling),

	TP_STRUCT__entry(
		__field(int,  cpu_id)
		__field(int,  freq_thermal)
		__field(int,  freq_max)
		__field(int,  freq_ceiling)
		),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
		__entry->freq_thermal = freq_thermal;
		__entry->freq_max = freq_max;
		__entry->freq_ceiling = freq_ceiling;
		),

	TP_printk("cpu=%d thermal=%lu cpu_freq_max=%lu sched_ceiling=%lu",
		__entry->cpu_id, __entry->freq_thermal, __entry->freq_max, __entry->freq_ceiling)
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
	TP_PROTO(int pid, char *comm,
		unsigned long util,
		unsigned int util_enqueued, unsigned int util_ewma),
	TP_ARGS(pid, comm, util, util_enqueued, util_ewma),
	TP_STRUCT__entry(
		__field(int, pid)
		__field(char *, comm)
		__field(unsigned long, util)
		__field(unsigned int, util_enqueued)
		__field(unsigned int, util_ewma)
	),
	TP_fast_assign(
		__entry->pid = pid;
		__entry->comm = comm;
		__entry->util = util;
		__entry->util_enqueued = util_enqueued;
		__entry->util_ewma = util_ewma;
	),
	TP_printk(
		"pid=%d comm=%s util=%lu util_enqueued=%u util_ewma=%u",
		__entry->pid,
		__entry->comm,
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

TRACE_EVENT(sugov_ext_util,
	TP_PROTO(int cpu, unsigned long util,
		unsigned int min, unsigned int max),
	TP_ARGS(cpu, util, min, max),
	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, util)
		__field(unsigned int, min)
		__field(unsigned int, max)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->util = util;
		__entry->min = min;
		__entry->max = max;
	),
	TP_printk(
		"cpu=%d util=%lu min=%u max=%u",
		__entry->cpu,
		__entry->util,
		__entry->min,
		__entry->max)
);
#endif /* _TRACE_SCHEDULER_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE sched_trace
/* This part must be outside protection */
#include <trace/define_trace.h>

