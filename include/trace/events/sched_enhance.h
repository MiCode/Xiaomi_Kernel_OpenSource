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
/* mtk scheduling interopertion enhancement*/
sched_trace(sched_interop);

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
			int acap, int scaled_acap, int scaled_atask,
			int threshold),

		TP_ARGS(target, loadwop_avg, h_nr_running,
			cluster_mask, nr_task, load_avg, capacity,
			acap, scaled_acap, scaled_atask, threshold),

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
			__field(int, scaled_atask)
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
			__entry->scaled_atask = scaled_atask;
			__entry->threshold = threshold;
			),

		TP_printk("cpu[%d]:load=%lu len=%u, cluster[%lx]: nr_task=%d load_avg=%d capacity=%d acap=%d scaled_acap=%d scaled_atask=%d threshold=%d",
			__entry->target,
			__entry->loadwop_avg,
			__entry->h_nr_running,
			__entry->cluster_mask,
			__entry->nr_task,
			__entry->load_avg,
			__entry->capacity,
			__entry->acap,
			__entry->scaled_acap,
			__entry->scaled_atask,
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
