// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Unisoc (shanghai) Technologies Co., Ltd
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM unisoc_sched

#if !defined(_TRACE_WALT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_WALT_H

#include <linux/tracepoint.h>

#if IS_ENABLED(CONFIG_SCHED_WALT)
struct uni_task_ravg;
struct uni_rq;
struct pd_cache;

TRACE_EVENT(walt_update_task_ravg,

	TP_PROTO(struct task_struct *p, struct rq *rq,
		 struct uni_task_struct *wtr, struct uni_rq *wrq,
		 int evt, u64 wallclock, u64 irqtime),

	TP_ARGS(p, rq, wtr, wrq, evt, wallclock, irqtime),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,	pid)
		__field(pid_t,	cur_pid)
		__field(u64,	wallclock)
		__field(u64,	mark_start)
		__field(u64,	delta_m)
		__field(u64,	win_start)
		__field(u64,	delta)
		__field(u64,	irqtime)
		__array(char,   evt, 16)
		__field(u32,	demand)
		__field(u32,	demand_scale)
		__field(u32,	sum)
		__field(int,	cpu)
		__field(u64,	cs)
		__field(u64,	ps)
		__field(u32,	curr_window)
		__field(u32,	prev_window)
		__field(u64,	nt_cs)
		__field(u64,	nt_ps)
		__field(u32,	active_windows)
	),

	TP_fast_assign(
			static const char *walt_event_names[] = {
				"PUT_PREV_TASK",
				"PICK_NEXT_TASK",
				"TASK_WAKE",
				"TASK_MIGRATE",
				"TASK_UPDATE",
				"IRQ_UPDATE"
			};
		__entry->wallclock	= wallclock;
		__entry->win_start	= wrq->window_start;
		__entry->delta		= (wallclock - wrq->window_start);
		strcpy(__entry->evt, walt_event_names[evt]);
		__entry->cpu		= rq->cpu;
		__entry->cur_pid	= rq->curr->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->mark_start	= wtr->mark_start;
		__entry->delta_m	= (wallclock - wtr->mark_start);
		__entry->demand		= wtr->demand;
		__entry->demand_scale	= wtr->demand_scale;
		__entry->sum		= wtr->sum;
		__entry->irqtime	= irqtime;
		__entry->cs		= wrq->curr_runnable_sum;
		__entry->ps		= wrq->prev_runnable_sum;
		__entry->curr_window	= wtr->curr_window;
		__entry->prev_window	= wtr->prev_window;
	),

	TP_printk("wallclock=%llu window_start=%llu delta=%llu event=%s cpu=%d"
		" cur_pid=%d pid=%d comm=%s walt_util=%d mark_start=%llu delta=%llu"
		" demand=%u  sum=%u irqtime=%llu  curr_runnable_sum=%llu"
		" prev_runnable_sum=%llu cur_window=%u prev_window=%u",
		__entry->wallclock, __entry->win_start, __entry->delta,
		__entry->evt, __entry->cpu, __entry->cur_pid,
		__entry->pid, __entry->comm, __entry->demand_scale,
		__entry->mark_start, __entry->delta_m, __entry->demand,
		__entry->sum, __entry->irqtime,	__entry->cs, __entry->ps,
		__entry->curr_window, __entry->prev_window
		)
);

TRACE_EVENT(walt_update_history,

	TP_PROTO(struct rq *rq, struct task_struct *p,
		 struct uni_task_struct *wtr, u32 runtime,
		 int samples, int evt),

	TP_ARGS(rq, p, wtr, runtime, samples, evt),

	TP_STRUCT__entry(
		__array(char,		comm,	TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned int,	runtime)
		__field(int,		samples)
		__field(int,		evt)
		__field(u64,		demand)
		__array(unsigned int,	hist, RAVG_HIST_SIZE_MAX)
		__field(int,		cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->runtime        = runtime;
		__entry->samples        = samples;
		__entry->evt            = evt;
		__entry->demand         = wtr->demand;
		memcpy(__entry->hist, wtr->sum_history,
					RAVG_HIST_SIZE_MAX * sizeof(u32));
		__entry->cpu            = rq->cpu;
	),

	TP_printk("pid=%d comm=%s runtime=%u samples=%d event=%d demand=%llu"
		" cpu=%d hist0-5=%u %u %u %u %u %u",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->samples, __entry->evt,
		__entry->demand, __entry->cpu,
		__entry->hist[0], __entry->hist[1],
		__entry->hist[2], __entry->hist[3],
		__entry->hist[4], __entry->hist[5])
);

TRACE_EVENT(walt_migration_update_sum,

	TP_PROTO(struct rq *rq, struct uni_rq *wrq, struct task_struct *p),

	TP_ARGS(rq, wrq, p),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(int,	pid)
		__field(u64,	cs)
		__field(u64,	ps)
	),

	TP_fast_assign(
		__entry->cpu		= cpu_of(rq);
		__entry->cs		= wrq->curr_runnable_sum;
		__entry->ps		= wrq->prev_runnable_sum;
		__entry->pid		= p->pid;
	),

	TP_printk("cpu=%d curr_runnable_sum=%llu prev_runnable_sum=%llu pid=%d",
		  __entry->cpu, __entry->cs, __entry->ps, __entry->pid)
);

/*trace cfs task info in feec*/
TRACE_EVENT(sched_feec_task_info,

	TP_PROTO(struct task_struct *p, int prev_cpu, unsigned long task_util,
		 int task_boost, unsigned long uclamp_util, int boosted,
		 int latency_sensitive, int blocked),

	TP_ARGS(p, prev_cpu, task_util, task_boost, uclamp_util, boosted, latency_sensitive, blocked),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		prev_cpu)
		__field(unsigned long,	task_util)
		__field(int,		task_boost)
		__field(unsigned long,	uclamp_util)
		__field(int,		boosted)
		__field(int,		latency_sensitive)
		__field(int,		blocked)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->prev_cpu		= prev_cpu;
		__entry->task_util		= task_util;
		__entry->task_boost		= task_boost;
		__entry->uclamp_util		= uclamp_util;
		__entry->boosted		= boosted;
		__entry->latency_sensitive	= latency_sensitive;
		__entry->blocked		= blocked;
	),

	TP_printk("comm=%s pid=%d prev_cpu=%d util=%lu boost=%d uclamp_util=%lu boosted=%d latency_sensitive=%d blocked=%d",
		  __entry->comm, __entry->pid, __entry->prev_cpu, __entry->task_util,
		  __entry->task_boost, __entry->uclamp_util, __entry->boosted,
		  __entry->latency_sensitive, __entry->blocked)
);
/*
 * trace cfs rq info
 */
TRACE_EVENT(sched_feec_rq_task_util,

	TP_PROTO(int cpu, struct task_struct *p, struct pd_cache *pd_cache,
		 unsigned long util, unsigned long spare_cap, unsigned long cpu_cap),

	TP_ARGS(cpu, p, pd_cache, util, spare_cap, cpu_cap),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned int,	nr_running)
		__field(unsigned int,	idle)
		__field(unsigned long,	wake_util)
		__array(char,   comm,   TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned long,	spare_cap)
		__field(unsigned long,	capacity_orig)
		__field(unsigned long,	thermal_pressure)
		__field(unsigned long,	cpu_cap)
	),

	TP_fast_assign(
		__entry->cpu			= cpu;
		__entry->nr_running		= cpu_rq(cpu)->nr_running;
		__entry->idle			= pd_cache->is_idle;
		__entry->wake_util		= pd_cache->wake_util;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->spare_cap		= spare_cap;
		__entry->capacity_orig		= pd_cache->cap_orig;
		__entry->thermal_pressure	= pd_cache->thermal_pressure;
		__entry->cpu_cap		= cpu_cap;
	),

	TP_printk("cpu=%d nr_running=%u idle =%d wake_util=%lu comm=%s pid=%d "
		  "spare_cap=%lu capacity_of=%lu capacity_orig=%lu thermal_pressure=%lu",
		  __entry->cpu, __entry->nr_running, __entry->idle, __entry->wake_util,
		  __entry->comm, __entry->pid, __entry->spare_cap, __entry->cpu_cap,
		  __entry->capacity_orig, __entry->thermal_pressure)
);
/*
 * trace energy diff info
 */
TRACE_EVENT(sched_energy_diff,

	TP_PROTO(unsigned long pd_energy, unsigned long base_energy, unsigned long prev_delta,
		unsigned long curr_delta, unsigned long best_delta, int prev_cpu,
		int best_energy_cpu, int max_spare_cap_cpu),

	TP_ARGS(pd_energy, base_energy, prev_delta, curr_delta, best_delta,
		prev_cpu, best_energy_cpu, max_spare_cap_cpu),

	TP_STRUCT__entry(
		__field(unsigned long,	pd_energy)
		__field(unsigned long,	base_energy)
		__field(unsigned long,	prev_delta)
		__field(unsigned long,	curr_delta)
		__field(unsigned long,	best_delta)
		__field(int,		prev_cpu)
		__field(int,		best_energy_cpu)
		__field(int,		max_spare_cap_cpu)
	),

	TP_fast_assign(
		__entry->pd_energy		= pd_energy;
		__entry->base_energy		= base_energy;
		__entry->prev_delta		= prev_delta;
		__entry->curr_delta		= curr_delta;
		__entry->best_delta		= best_delta;
		__entry->prev_cpu		= prev_cpu;
		__entry->best_energy_cpu	= best_energy_cpu;
		__entry->max_spare_cap_cpu	= max_spare_cap_cpu;
	),

	TP_printk("pd_eng=%lu base_eng=%lu p_delta=%lu c_delta=%lu b_delta=%lu "
		  "prev_cpu=%d best_eng_cpu=%d max_spare_cpu=%d",
		  __entry->pd_energy, __entry->base_energy, __entry->prev_delta,
		  __entry->curr_delta, __entry->best_delta, __entry->prev_cpu,
		  __entry->best_energy_cpu, __entry->max_spare_cap_cpu)
);

/*trace feec candidates */
TRACE_EVENT(sched_feec_candidates,

	TP_PROTO(int prev_cpu, int best_energy_cpu, unsigned long base_energy, unsigned long prev_delta,
		 unsigned long best_delta, int best_idle_cpu, int max_spare_cap_cpu_ls),

	TP_ARGS(prev_cpu, best_energy_cpu, base_energy, prev_delta, best_delta,
		best_idle_cpu, max_spare_cap_cpu_ls),

	TP_STRUCT__entry(
		__field(int,		prev_cpu)
		__field(int,		best_energy_cpu)
		__field(unsigned long,	base_energy)
		__field(unsigned long,	prev_delta)
		__field(unsigned long,	best_delta)
		__field(unsigned long,	threshold)
		__field(int,		best_idle_cpu)
		__field(int,		max_spare_cap_cpu_ls)
	),

	TP_fast_assign(
		__entry->prev_cpu		= prev_cpu;
		__entry->best_energy_cpu	= best_energy_cpu;
		__entry->best_idle_cpu		= best_idle_cpu;
		__entry->base_energy		= base_energy;
		__entry->prev_delta		= prev_delta;
		__entry->best_delta		= best_delta;
		__entry->threshold = prev_delta == ULONG_MAX ? 0 : ((prev_delta + base_energy) >> 4);
		__entry->max_spare_cap_cpu_ls	= max_spare_cap_cpu_ls;
	),

	TP_printk("prev_cpu=%d best_eng_cpu=%d base_eng=%lu p_delta=%lu b_delta=%lu "
		  "threshold=%lu best_idle_cpu=%d max_spare_cpu_ls=%d",
		  __entry->prev_cpu, __entry->best_energy_cpu, __entry->base_energy,
		  __entry->prev_delta, __entry->best_delta, __entry->threshold,
		  __entry->best_idle_cpu, __entry->max_spare_cap_cpu_ls)
);

TRACE_EVENT(sched_active_migration,

	TP_PROTO(struct task_struct *p, int prev_cpu, int new_cpu),

	TP_ARGS(p, prev_cpu, new_cpu),

	TP_STRUCT__entry(
		__array(char,   comm,   TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		prev_cpu)
		__field(int,		new_cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid			= p->pid;
		__entry->prev_cpu		= prev_cpu;
		__entry->new_cpu		= new_cpu;
	),

	TP_printk("comm=%s pid=%d prev_cpu=%d new_cpu=%d ", __entry->comm,
		  __entry->pid, __entry->prev_cpu, __entry->new_cpu)
);

TRACE_EVENT(sched_find_new_ilb,

	TP_PROTO(int cpu, unsigned long ref_cap, int best_cpu, unsigned long best_cap, int new_cpu),

	TP_ARGS(cpu, ref_cap, best_cpu, best_cap, new_cpu),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned long,	ref_cap)
		__field(int,		best_cpu)
		__field(unsigned long,	best_cap)
		__field(int,		new_cpu)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->ref_cap	= ref_cap;
		__entry->best_cpu	= best_cpu;
		__entry->best_cap	= best_cap;
		__entry->new_cpu	= new_cpu;
	),

	TP_printk("cpu=%d ref_cap=%lu best_cpu=%d best_cap=%lu new_cpu=%d ",
		  __entry->cpu, __entry->ref_cap, __entry->best_cpu,
		  __entry->best_cap, __entry->new_cpu)
);
#if IS_ENABLED(CONFIG_UNISOC_ROTATION_TASK)
/* task_rotation info */
TRACE_EVENT(sched_task_rotation,

	TP_PROTO(int src_cpu, int dst_cpu, int src_pid, int dst_pid),

	TP_ARGS(src_cpu, dst_cpu, src_pid, dst_pid),

	TP_STRUCT__entry(
		__field(int,	src_cpu)
		__field(int,	dst_cpu)
		__field(int,	src_pid)
		__field(int,	dst_pid)
	),

	TP_fast_assign(
		__entry->src_cpu = src_cpu;
		__entry->dst_cpu = dst_cpu;
		__entry->src_pid = src_pid;
		__entry->dst_pid = dst_pid;
	),

	TP_printk("src_cpu=%d dst_cpu=%d src_pid=%d dst_pid=%d",
		__entry->src_cpu, __entry->dst_cpu,
		__entry->src_pid, __entry->dst_pid
	)
);
#endif
#endif
#ifdef CONFIG_UNISOC_GROUP_BOOST
/*
 * Tracepoint for group_boost_tasks_update
 */
TRACE_EVENT(sched_group_boost_tasks_update,

	TP_PROTO(struct task_struct *tsk, int cpu, int tasks, int idx,
		int boost, int max_boost),

	TP_ARGS(tsk, cpu, tasks, idx, boost, max_boost),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		cpu)
		__field(int,		tasks)
		__field(int,		idx)
		__field(int,		boost)
		__field(int,		max_boost)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid		= tsk->pid;
		__entry->cpu 		= cpu;
		__entry->tasks		= tasks;
		__entry->idx 		= idx;
		__entry->boost		= boost;
		__entry->max_boost	= max_boost;
	),

	TP_printk("pid=%d comm=%s cpu=%d tasks=%d idx=%d boost=%d max_boost=%d",
		__entry->pid, __entry->comm,
		__entry->cpu, __entry->tasks, __entry->idx,
		__entry->boost, __entry->max_boost)
);

/*
 * Tracepoint for group_boost_val_update
 */
TRACE_EVENT(sched_groupboost_val_update,

	TP_PROTO(int cpu, int variation, int max_boost),

	TP_ARGS(cpu, variation, max_boost),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(int,	variation)
		__field(int,	max_boost)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->variation	= variation;
		__entry->max_boost	= max_boost;
	),

	TP_printk("cpu=%d variation=%d max_boost=%d",
		__entry->cpu, __entry->variation, __entry->max_boost)
);
#endif
/*
 * Tracepoint for accounting CPU  boosted utilization
 */
TRACE_EVENT(sched_boost_cpu,

	TP_PROTO(int cpu, unsigned long util, int boost, long margin),

	TP_ARGS(cpu, util, boost, margin),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(unsigned long,	util)
		__field(int,		boost)
		__field(long,		margin)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->util	= util;
		__entry->boost	= boost;
		__entry->margin	= margin;
	),

	TP_printk("cpu=%d util=%lu boost=%d margin=%ld",
		  __entry->cpu, __entry->util, __entry->boost, __entry->margin)
);
/*
 * Tracepoint for accounting task boosted utilization
 */
TRACE_EVENT(sched_boost_task,

	TP_PROTO(struct task_struct *tsk, unsigned long util, int boost, long margin),

	TP_ARGS(tsk, util, boost, margin),

	TP_STRUCT__entry(
		__array(char,	comm,	TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned long,	util)
		__field(int,		boost)
		__field(long,		margin)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid	= tsk->pid;
		__entry->util	= util;
		__entry->boost	= boost;
		__entry->margin	= margin;
	),

	TP_printk("comm=%s pid=%d util=%lu boost=%d margin=%ld",
		  __entry->comm, __entry->pid, __entry->util, __entry->boost,
		  __entry->margin)
);
#ifdef CONFIG_UNISOC_SCHED_VIP_TASK
TRACE_EVENT(sched_task_setting_handler,
	TP_PROTO(struct task_struct *p, int param, int val),

	TP_ARGS(p, param, val),

	TP_STRUCT__entry(
		__array(char,           comm,   TASK_COMM_LEN)
		__field(pid_t,          pid)
		__field(int,            param)
		__field(int,            val)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid	= p->pid;
		__entry->param	= param;
		__entry->val	= val;
	),

	TP_printk("comm=%s pid=%d param=%d val=%d",
		__entry->comm, __entry->pid, __entry->param, __entry->val)
);

DECLARE_EVENT_CLASS(cfs_vip_task_template,

	TP_PROTO(struct task_struct *p, struct uni_task_struct *unitsk, unsigned int limit),

	TP_ARGS(p, unitsk, limit),

	TP_STRUCT__entry(
		__array(char,		comm,	TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(int,		prio)
		__field(int,		vip_level)
		__field(int,		cpu)
		__field(u64,		exec)
		__field(unsigned int,	limit)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->prio		= p->prio;
		__entry->vip_level	= unitsk->vip_level;
		__entry->cpu		= task_cpu(p);
		__entry->exec		= unitsk->total_exec;
		__entry->limit		= limit;
	),

	TP_printk("comm=%s pid=%d prio=%d vip_level=%d cpu=%d exec=%llu limit=%u",
		__entry->comm, __entry->pid, __entry->prio,
		__entry->vip_level, __entry->cpu, __entry->exec, __entry->limit)
);
/* called upon VIP task de-activation. exec will be more than limit */
DEFINE_EVENT(cfs_vip_task_template, cfs_deactivate_vip_task,
	     TP_PROTO(struct task_struct *p, struct uni_task_struct *unitsk, unsigned int limit),
	     TP_ARGS(p, unitsk, limit));

/* called upon when VIP is returned to run next */
DEFINE_EVENT(cfs_vip_task_template, cfs_vip_pick_next,
	     TP_PROTO(struct task_struct *p, struct uni_task_struct *unitsk, unsigned int limit),
	     TP_ARGS(p, unitsk, limit));

/* called upon when VIP (current) is not preempted by waking task */
DEFINE_EVENT(cfs_vip_task_template, cfs_vip_wakeup_nopreempt,
	     TP_PROTO(struct task_struct *p, struct uni_task_struct *unitsk, unsigned int limit),
	     TP_ARGS(p, unitsk, limit));

/* called upon when VIP (waking task) preempts the current */
DEFINE_EVENT(cfs_vip_task_template, cfs_vip_wakeup_preempt,
	     TP_PROTO(struct task_struct *p, struct uni_task_struct *unitsk, unsigned int limit),
	     TP_ARGS(p, unitsk, limit));
#endif
TRACE_EVENT(sched_show_untask_stack,
	TP_PROTO(struct task_struct *p, unsigned long c0,
		unsigned long c1, unsigned long c2, unsigned long c3,
		unsigned long c4, unsigned long c5),

	TP_ARGS(p, c0, c1, c2, c3, c4, c5),

	TP_STRUCT__entry(
		__array(char,		comm,	TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned long,	c0)
		__field(unsigned long,	c1)
		__field(unsigned long,	c2)
		__field(unsigned long,	c3)
		__field(unsigned long,	c4)
		__field(unsigned long,	c5)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid	= p->pid;
		__entry->c0	= c0;
		__entry->c1	= c1;
		__entry->c2	= c2;
		__entry->c3	= c3;
		__entry->c4	= c4;
		__entry->c5	= c5;
	),
	TP_printk("comm=%s pid=%d callers=%ps <- %ps <- %ps <- %ps <- %ps <- %ps",
		__entry->comm, __entry->pid, (void *)__entry->c0, (void *)__entry->c1,
		(void *)__entry->c2, (void *)__entry->c3, (void *)__entry->c4,
		(void *)__entry->c5)
);

TRACE_EVENT(sched_uni_newidle_balance,

	TP_PROTO(int this_cpu, int busy_cpu, int pulled, bool help_min_cap, bool enough_idle),

	TP_ARGS(this_cpu, busy_cpu, pulled, help_min_cap, enough_idle),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		busy_cpu)
		__field(int,		pulled)
		__field(unsigned int,	nr_running)
		__field(unsigned int,	rt_nr_running)
		__field(int,		nr_iowait)
		__field(bool,		help_min_cap)
		__field(u64,		avg_idle)
		__field(bool,		enough_idle)
		__field(int,		overload)
	),

	TP_fast_assign(
		__entry->cpu		= this_cpu;
		__entry->busy_cpu	= busy_cpu;
		__entry->pulled		= pulled;
		__entry->nr_running	= cpu_rq(this_cpu)->nr_running;
		__entry->rt_nr_running	= cpu_rq(this_cpu)->rt.rt_nr_running;
		__entry->nr_iowait	= atomic_read(&(cpu_rq(this_cpu)->nr_iowait));
		__entry->help_min_cap	= help_min_cap;
		__entry->avg_idle	= cpu_rq(this_cpu)->avg_idle;
		__entry->enough_idle	= enough_idle;
		__entry->overload	= cpu_rq(this_cpu)->rd->overload;
	),

	TP_printk("cpu=%d busy_cpu=%d pulled=%d nr_running=%u rt_nr_running=%u nr_iowait=%d help_min_cap=%d avg_idle=%llu enough_idle=%d overload=%d",
		  __entry->cpu, __entry->busy_cpu, __entry->pulled, __entry->nr_running,
		  __entry->rt_nr_running, __entry->nr_iowait, __entry->help_min_cap,
		  __entry->avg_idle, __entry->enough_idle, __entry->overload)
);

#endif /* _TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/unisoc_platform/sched
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
