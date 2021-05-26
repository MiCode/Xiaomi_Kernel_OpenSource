/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifdef CONFIG_MTK_SCHED_TRACE
#define sched_trace(event) \
TRACE_EVENT(event,                      \
	TP_PROTO(char *strings),                    \
	TP_ARGS(strings),                           \
	TP_STRUCT__entry(                           \
		__array(char,  strings, 128)        \
	),                                          \
	TP_fast_assign(                             \
		memcpy(__entry->strings, strings, 128); \
	),                                          \
	TP_printk("%s", __entry->strings))

sched_trace(sched_eas_energy_calc);
sched_trace(sched_log);

#endif
/*
 * MT: Tracepoint for system overutilized indicator
 */
TRACE_EVENT(sched_system_overutilized,

	TP_PROTO(bool overutilized),

	TP_ARGS(overutilized),

	TP_STRUCT__entry(
		__field(bool, overutilized)
	),

	TP_fast_assign(
		__entry->overutilized = overutilized;
	),

	TP_printk("system overutilized=%d",
		__entry->overutilized ? 1 : 0)
);

#ifdef CONFIG_MTK_SCHED_EAS_POWER_SUPPORT
/*
 * Tracepoint for share buck calculation
 */
TRACE_EVENT(group_norm_util,

	TP_PROTO(int cpu_idx, int cpu, int cid, unsigned long util_sum,
			unsigned long norm_util, unsigned long delta,
			unsigned long util, unsigned long capacity),

	TP_ARGS(cpu_idx, cpu, cid, util_sum, norm_util, delta, util, capacity),

	TP_STRUCT__entry(
		__field(int, cpu_idx)
		__field(int, cpu)
		__field(int, cid)
		__field(unsigned long, util_sum)
		__field(unsigned long, norm_util)
		__field(unsigned long, delta)
		__field(unsigned long, util)
		__field(unsigned long, capacity)
	),

	TP_fast_assign(
		__entry->cpu_idx        = cpu_idx;
		__entry->cpu            = cpu;
		__entry->cid            = cid;
		__entry->util_sum       = util_sum;
		__entry->norm_util      = norm_util;
		__entry->delta          = delta;
		__entry->util           = util;
		__entry->capacity       = capacity;
	),

	TP_printk("cpu_idx=%d cpu=%d cid=%d util_sum=%lu norm_util=%lu delta=%lu util=%lu capacity=%lu",
		__entry->cpu_idx, __entry->cpu, __entry->cid,
		__entry->util_sum, __entry->norm_util, __entry->delta,
		__entry->util, __entry->capacity)
);

/*
 * Tracepoint for share buck calculation
 */
TRACE_EVENT(sched_share_buck,

	TP_PROTO(int cpu_idx, int cid, int cap_idx, int co_buck_cid,
			int co_buck_cap_idx, unsigned long co_buck_volt),

	TP_ARGS(cpu_idx, cid, cap_idx, co_buck_cid, co_buck_cap_idx,
		co_buck_volt),

	TP_STRUCT__entry(
		__field(int, cpu_idx)
		__field(int, cid)
		__field(int, cap_idx)
		__field(int, co_buck_cid)
		__field(int, co_buck_cap_idx)
		__field(unsigned long, co_buck_volt)
	),

	TP_fast_assign(
		__entry->cpu_idx        = cpu_idx;
		__entry->cid            = cid;
		__entry->cap_idx        = cap_idx;
		__entry->co_buck_cid    = co_buck_cid;
		__entry->co_buck_cap_idx = co_buck_cap_idx;
		__entry->co_buck_volt   = co_buck_volt;
	),

	TP_printk("cpu_idx=%d cid%d=%d co_cid%d=%d co_volt=%lu",
		__entry->cpu_idx, __entry->cid, __entry->cap_idx,
		__entry->co_buck_cid, __entry->co_buck_cap_idx,
		__entry->co_buck_volt)
);

/*
 * Tracepoint for idle power calculation
 */
TRACE_EVENT(sched_idle_power,

	TP_PROTO(int sd_level, int cap_idx, int leak_pwr, int energy_cost),

	TP_ARGS(sd_level, cap_idx, leak_pwr, energy_cost),

	TP_STRUCT__entry(
		__field(int, sd_level)
		__field(int, cap_idx)
		__field(int, leak_pwr)
		__field(int, energy_cost)
	),

	TP_fast_assign(
		__entry->sd_level       = sd_level;
		__entry->cap_idx        = cap_idx;
		__entry->leak_pwr       = leak_pwr;
		__entry->energy_cost    = energy_cost;
	),

	TP_printk("lv=%d tlb[%d].leak=(%d) total=%d",
		__entry->sd_level, __entry->cap_idx, __entry->leak_pwr,
		__entry->energy_cost)
);

/*
 * Tracepoint for busy_power calculation
 */
TRACE_EVENT(sched_busy_power,

	TP_PROTO(int sd_level, int cap_idx, unsigned long dyn_pwr,
			unsigned long volt_f, unsigned long buck_pwr,
			int co_cap_idx, int leak_pwr, int energy_cost),

	TP_ARGS(sd_level, cap_idx, dyn_pwr, volt_f, buck_pwr, co_cap_idx,
		leak_pwr, energy_cost),

	TP_STRUCT__entry(
		__field(int, sd_level)
		__field(int, cap_idx)
		__field(unsigned long, dyn_pwr)
		__field(unsigned long, volt_f)
		__field(unsigned long, buck_pwr)
		__field(int, co_cap_idx)
		__field(unsigned long, leak_pwr)
		__field(int, energy_cost)
	),

	TP_fast_assign(
		__entry->sd_level       = sd_level;
		__entry->cap_idx        = cap_idx;
		__entry->dyn_pwr        = dyn_pwr;
		__entry->volt_f         = volt_f;
		__entry->buck_pwr       = buck_pwr;
		__entry->co_cap_idx     = co_cap_idx;
		__entry->leak_pwr       = leak_pwr;
		__entry->energy_cost    = energy_cost;
	),

	TP_printk("lv=%d tlb[%d].pwr=%ld volt_f=%ld buck.pwr=%ld tlb[%d].leak=(%ld) total=%d",
		__entry->sd_level, __entry->cap_idx,  __entry->dyn_pwr,
		__entry->volt_f, __entry->buck_pwr,  __entry->co_cap_idx,
		__entry->leak_pwr, __entry->energy_cost)
);
#endif

/*
 * Tracepoint for HMP (CONFIG_SCHED_HMP) task migrations.
 */
TRACE_EVENT(sched_hmp_migrate,

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
 * Tracepoint for accounting sched group energy
 */
TRACE_EVENT(sched_energy_diff,

	TP_PROTO(struct task_struct *tsk, int scpu, int dcpu, int udelta,
		int nrgb, int nrga, int nrgd),

	TP_ARGS(tsk, scpu, dcpu, udelta,
		nrgb, nrga, nrgd),

	TP_STRUCT__entry(
		__array(char,  comm,   TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,   scpu)
		__field(int,   dcpu)
		__field(int,   udelta)
		__field(int,   nrgb)
		__field(int,   nrga)
		__field(int,   nrgd)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid            = tsk->pid;
		__entry->scpu           = scpu;
		__entry->dcpu           = dcpu;
		__entry->udelta         = udelta;
		__entry->nrgb           = nrgb;
		__entry->nrga           = nrga;
		__entry->nrgd           = nrgd;
	),

	TP_printk("pid=%d comm=%s src_cpu=%d dst_cpu=%d usage_delta=%d nrg_before=%d nrg_after=%d nrg_diff=%d",
		__entry->pid, __entry->comm,
		__entry->scpu, __entry->dcpu, __entry->udelta,
		__entry->nrgb, __entry->nrga, __entry->nrgd)
);

/*
 * Tracepoint for showing tracked cfs runqueue runnable load.
 */
TRACE_EVENT(sched_cfs_runnable_load,

		TP_PROTO(int cpu_id, int cpu_load, int cpu_ntask),

		TP_ARGS(cpu_id, cpu_load, cpu_ntask),

		TP_STRUCT__entry(
			__field(int, cpu_id)
			__field(int, cpu_load)
			__field(int, cpu_ntask)
			),

		TP_fast_assign(
			__entry->cpu_id = cpu_id;
			__entry->cpu_load = cpu_load;
			__entry->cpu_ntask = cpu_ntask;
			),

		TP_printk("cpu-id=%d cfs-load=%4d, cfs-ntask=%2d",
			__entry->cpu_id,
			__entry->cpu_load,
			__entry->cpu_ntask)
		);
#ifdef CONFIG_SCHED_HMP
/*
 * Tracepoint for showing cluster statistics in HMP
 */
TRACE_EVENT(sched_cluster_stats,

		TP_PROTO(int target, unsigned long loadwop_avg,
			unsigned int h_nr_running, unsigned long cluster_mask,
			int nr_task, int load_avg, int capacity,
			int acap, int scaled_acap, int threshold),

		TP_ARGS(target, loadwop_avg, h_nr_running,
			cluster_mask, nr_task, load_avg, capacity,
			acap, scaled_acap, threshold),

		TP_STRUCT__entry(
			__field(int, target)
			__field(unsigned long, loadwop_avg)
			__field(unsigned int, h_nr_running)
			__field(unsigned long, cluster_mask)
			__field(int, nr_task)
			__field(int, load_avg)
			__field(int, capacity)
			__field(int, acap)
			__field(int, scaled_acap)
			__field(int, threshold)
			),

		TP_fast_assign(
			__entry->target = target;
			__entry->loadwop_avg = loadwop_avg;
			__entry->h_nr_running = h_nr_running;
			__entry->cluster_mask = cluster_mask;
			__entry->nr_task = nr_task;
			__entry->load_avg = load_avg;
			__entry->capacity = capacity;
			__entry->acap = acap;
			__entry->scaled_acap = scaled_acap;
			__entry->threshold = threshold;
			),

		TP_printk("cpu[%d]:load=%lu len=%u, cluster[%lx]: nr_task=%d load_avg=%d capacity=%d acap=%d scaled_acap=%d threshold=%d",
			__entry->target,
			__entry->loadwop_avg,
			__entry->h_nr_running,
			__entry->cluster_mask,
			__entry->nr_task,
			__entry->load_avg,
			__entry->capacity,
			__entry->acap,
			__entry->scaled_acap,
			__entry->threshold)
		);
/*
 * Tracepoint for showing adjusted threshold in HMP
 */
TRACE_EVENT(sched_adj_threshold,

		TP_PROTO(int b_threshold, int l_threshold, int l_target,
			int l_cap, int b_target, int b_cap),

		TP_ARGS(b_threshold, l_threshold, l_target,
			l_cap, b_target, b_cap),

		TP_STRUCT__entry(
			__field(int, b_threshold)
			__field(int, l_threshold)
			__field(int, l_target)
			__field(int, l_cap)
			__field(int, b_target)
			__field(int, b_cap)
			),

		TP_fast_assign(
			__entry->b_threshold = b_threshold;
			__entry->l_threshold = l_threshold;
			__entry->l_target = l_target;
			__entry->l_cap = l_cap;
			__entry->b_target = b_target;
			__entry->b_cap = b_cap;
			),

		TP_printk("up=%4d down=%4d L_cpu=%d(%4u) B_cpu=%d(%4u)",
			__entry->b_threshold, __entry->l_threshold,
			__entry->l_target, __entry->l_cap,
			__entry->b_target, __entry->b_cap)
		);
/*
 * Tracepoint for dumping hmp cluster load ratio
 */
TRACE_EVENT(sched_hmp_load,

		TP_PROTO(int step, int B_load_avg, int L_load_avg),

		TP_ARGS(step, B_load_avg, L_load_avg),

		TP_STRUCT__entry(
			__field(int, step)
			__field(int, B_load_avg)
			__field(int, L_load_avg)
			),

		TP_fast_assign(
			__entry->step = step;
			__entry->B_load_avg = B_load_avg;
			__entry->L_load_avg = L_load_avg;
			),

		TP_printk("[%d]: B-load-avg=%4d L-load-avg=%4d",
			__entry->step,
			__entry->B_load_avg,
			__entry->L_load_avg)
	   );
/*
 * Tracepoint for dumping hmp statistics
 */
TRACE_EVENT(sched_hmp_stats,

		TP_PROTO(struct hmp_statisic *hmp_stats),

		TP_ARGS(hmp_stats),

		TP_STRUCT__entry(
			__field(unsigned int, nr_force_up)
			__field(unsigned int, nr_force_down)
			),

		TP_fast_assign(
			__entry->nr_force_up = hmp_stats->nr_force_up;
			__entry->nr_force_down = hmp_stats->nr_force_down;
			),

		TP_printk("nr-force-up=%d nr-force-down=%2d",
			__entry->nr_force_up,
			__entry->nr_force_down)
	   );
/*
 * Tracepoint for showing tracked migration information
 */
TRACE_EVENT(sched_dynamic_threshold,

	TP_PROTO(struct task_struct *tsk, unsigned int threshold,
		unsigned int status, int curr_cpu, int target_cpu,
		int task_load, struct clb_stats *B, struct clb_stats *L),

	TP_ARGS(tsk, threshold, status, curr_cpu, target_cpu, task_load, B, L),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int, prio)
		__field(unsigned int, threshold)
		__field(unsigned int, status)
		__field(int, curr_cpu)
		__field(int, target_cpu)
		__field(int, curr_load)
		__field(int, target_load)
		__field(int, task_load)
		__field(int, B_load_avg)
		__field(int, L_load_avg)
		),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid         = tsk->pid;
		__entry->prio        = tsk->prio;
		__entry->threshold   = threshold;
		__entry->status      = status;
		__entry->curr_cpu    = curr_cpu;
		__entry->target_cpu  = target_cpu;
		__entry->curr_load   = cpu_rq(curr_cpu)->cfs.avg.loadwop_avg;
		__entry->target_load = cpu_rq(target_cpu)->cfs.avg.loadwop_avg;
		__entry->task_load   = task_load;
		__entry->B_load_avg  = B->load_avg;
		__entry->L_load_avg  = L->load_avg;
		  ),

	TP_printk(
		"pid=%4d prio=%d status=0x%4x dyn=%4u task-load=%4d curr-cpu=%d(%4d) target=%d(%4d) L-load-avg=%4d B-load-avg=%4d comm=%s",
		__entry->pid,
		__entry->prio,
		__entry->status,
		__entry->threshold,
		__entry->task_load,
		__entry->curr_cpu,
		__entry->curr_load,
		__entry->target_cpu,
		__entry->target_load,
		__entry->L_load_avg,
		__entry->B_load_avg,
		__entry->comm)
	);

TRACE_EVENT(sched_dynamic_threshold_draw,

		TP_PROTO(unsigned int B_threshold, unsigned int L_threshold),

		TP_ARGS(B_threshold, L_threshold),

		TP_STRUCT__entry(
			__field(unsigned int, up_threshold)
			__field(unsigned int, down_threshold)
			),

		TP_fast_assign(
				__entry->up_threshold	= B_threshold;
				__entry->down_threshold	= L_threshold;
			),

		TP_printk(
				"%4u, %4u",
				__entry->up_threshold,
				__entry->down_threshold)
		);
/*
 * Tracepoint for cfs task enqueue event
 */
TRACE_EVENT(sched_cfs_enqueue_task,

		TP_PROTO(struct task_struct *tsk, int tsk_load, int cpu_id),

		TP_ARGS(tsk, tsk_load, cpu_id),

		TP_STRUCT__entry(
			__array(char, comm, TASK_COMM_LEN)
			__field(pid_t, tsk_pid)
			__field(int, tsk_load)
			__field(int, cpu_id)
			),

		TP_fast_assign(
			memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
			__entry->tsk_pid = tsk->pid;
			__entry->tsk_load = tsk_load;
			__entry->cpu_id = cpu_id;
			),

		TP_printk("cpu-id=%d task-pid=%4d task-load=%4d comm=%s",
			__entry->cpu_id,
			__entry->tsk_pid,
			__entry->tsk_load,
			__entry->comm)
		);

/*
 * Tracepoint for cfs task dequeue event
 */
TRACE_EVENT(sched_cfs_dequeue_task,

		TP_PROTO(struct task_struct *tsk, int tsk_load, int cpu_id),

		TP_ARGS(tsk, tsk_load, cpu_id),

		TP_STRUCT__entry(
			__array(char, comm, TASK_COMM_LEN)
			__field(pid_t, tsk_pid)
			__field(int, tsk_load)
			__field(int, cpu_id)
			),

		TP_fast_assign(
			memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
			__entry->tsk_pid = tsk->pid;
			__entry->tsk_load = tsk_load;
			__entry->cpu_id = cpu_id;
			),

		TP_printk("cpu-id=%d task-pid=%4d task-load=%4d comm=%s",
			__entry->cpu_id,
			__entry->tsk_pid,
			__entry->tsk_load,
			__entry->comm)
		);
#endif /* CONFIG_SCHED_HMP */

#ifdef CONFIG_MTK_SCHED_BOOST
/*
 * Tracepoint for set task cpu prefer
 */
TRACE_EVENT(sched_set_cpuprefer,

	TP_PROTO(struct task_struct *tsk),

	TP_ARGS(tsk),

	TP_STRUCT__entry(
		__array(char,  comm,   TASK_COMM_LEN)
		__field(pid_t, pid)
		__field(int,   cpu_prefer)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid            = tsk->pid;
		__entry->cpu_prefer     = tsk->cpu_prefer;
	),

	TP_printk("pid=%d comm=%s cpu_prefer=%d",
		__entry->pid, __entry->comm, __entry->cpu_prefer)
);
#endif

#ifdef CONFIG_MTK_SCHED_INTEROP
TRACE_EVENT(sched_interop,

	TP_PROTO(int cpu, unsigned long lowest_bits),

	TP_ARGS(cpu, lowest_bits),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned long, lowest_bits)
	),

	TP_fast_assign(
		__entry->cpu         = cpu;
		__entry->lowest_bits = lowest_bits;
	),

	TP_printk("current cpu=%d, find idle cpu from cpumask 0x%lx",
		__entry->cpu, __entry->lowest_bits)
);

TRACE_EVENT(sched_interop_lb,

	TP_PROTO(int cpu, int rt_nr_running),

	TP_ARGS(cpu, rt_nr_running),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, rt_nr_running)
	),

	TP_fast_assign(
		__entry->cpu           = cpu;
		__entry->rt_nr_running = rt_nr_running;
	),

	TP_printk("cpu=%d, rq->rt.rt_nr_running=%d",
		__entry->cpu, __entry->rt_nr_running)
);

TRACE_EVENT(sched_interop_best_cpu,

	TP_PROTO(int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("find_idle_cpu=%d", __entry->cpu)
);
#endif

/*
 * Tracepoint for load balance sched group calculation
 */
TRACE_EVENT(sched_update_lb_sg,

	TP_PROTO(unsigned long avg_load, unsigned long group_load,
		unsigned long group_capacity,
		int group_no_capacity, int group_type),

	TP_ARGS(avg_load, group_load, group_capacity,
		group_no_capacity, group_type),

	TP_STRUCT__entry(
		__field(unsigned long, avg_load)
		__field(unsigned long, group_load)
		__field(unsigned long, group_capacity)
		__field(int, group_no_capacity)
		__field(int, group_type)
	),

	TP_fast_assign(
		__entry->avg_load       = avg_load;
		__entry->group_load     = group_load;
		__entry->group_capacity = group_capacity;
		__entry->group_no_capacity = group_no_capacity;
		__entry->group_type = group_type;
	),

	TP_printk("avg_load=%lu group_load=%lu group_capacity=%lu group_no_capacity=%d group_type=%d",
		__entry->avg_load, __entry->group_load, __entry->group_capacity,
		__entry->group_no_capacity, __entry->group_type)
);

TRACE_EVENT(sched_select_task_rq,

	TP_PROTO(struct task_struct *tsk,
		int policy, int prev_cpu, int target_cpu,
		int task_util, int boost, bool prefer, int wake_flags),

	TP_ARGS(tsk, policy, prev_cpu, target_cpu, task_util, boost,
		prefer, wake_flags),

	TP_STRUCT__entry(
		__field(pid_t, pid)
		__field(int, policy)
		__field(int, prev_cpu)
		__field(int, target_cpu)
		__field(int, task_util)
		__field(int, boost)
		__field(long, task_mask)
		__field(bool, prefer)
#ifdef CONFIG_MTK_SCHED_BOOST
		__field(int, cpu_prefer)
#endif
		__field(int, wake_flags)
		),

	TP_fast_assign(
		__entry->pid        = tsk->pid;
		__entry->policy     = policy;
		__entry->prev_cpu   = prev_cpu;
		__entry->target_cpu = target_cpu;
		__entry->task_util	= task_util;
		__entry->boost		= boost;
		__entry->task_mask	= tsk->cpus_allowed.bits[0];
		__entry->prefer		= prefer;
#ifdef CONFIG_MTK_SCHED_BOOST
		__entry->cpu_prefer = tsk->cpu_prefer;
#endif
		__entry->wake_flags	= wake_flags;
		),

	TP_printk("pid=%4d policy=0x%08x pre-cpu=%d target=%d util=%d boost=%d mask=0x%lx prefer=%d cpu_prefer=%d flags=%d",
		__entry->pid,
		__entry->policy,
		__entry->prev_cpu,
		__entry->target_cpu,
		__entry->task_util,
		__entry->boost,
		__entry->task_mask,
		__entry->prefer,
#ifdef CONFIG_MTK_SCHED_BOOST
		__entry->cpu_prefer,
#else
		0,
#endif
		__entry->wake_flags)

);

/*
 * Tracepoint for big task rotation
 */
TRACE_EVENT(sched_big_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid,
		int fin, int set_uclamp),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid, fin, set_uclamp),

	TP_STRUCT__entry(
		__field(int, src_cpu)
		__field(int, dst_cpu)
		__field(int, src_pid)
		__field(int, dst_pid)
		__field(int, fin)
		__field(int, set_uclamp)
	),

	TP_fast_assign(
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->src_pid	= src_pid;
		__entry->dst_pid	= dst_pid;
		__entry->fin		= fin;
		__entry->set_uclamp	= set_uclamp;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d fin=%d set=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid,
		__entry->fin, __entry->set_uclamp)
);

TRACE_EVENT(sched_big_task_rotation_reset,

	TP_PROTO(int set_uclamp),

	TP_ARGS(set_uclamp),

	TP_STRUCT__entry(
		__field(int, set_uclamp)
	),

	TP_fast_assign(
		__entry->set_uclamp	= set_uclamp;
	),

	TP_printk("set_uclamp=%d",
		__entry->set_uclamp)
);

#ifdef CONFIG_MTK_TASK_TURBO
TRACE_EVENT(sched_set_user_nice,
	TP_PROTO(struct task_struct *task, int prio, int is_turbo),
	TP_ARGS(task, prio, is_turbo),
	TP_STRUCT__entry(
		__field(int, pid)
		__array(char, comm, TASK_COMM_LEN)
		__field(int, prio)
		__field(int, is_turbo)
	),

	TP_fast_assign(
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->pid	  = task->pid;
		__entry->prio	  = prio;
		__entry->is_turbo = is_turbo;
	),

	TP_printk("comm=%s pid=%d prio=%d is_turbo=%d",
		__entry->comm, __entry->pid, __entry->prio, __entry->is_turbo)
)
#endif
