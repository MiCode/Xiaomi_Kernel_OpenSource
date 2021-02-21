/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM sched

#if !defined(_TRACE_WALT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_WALT_H

#include <linux/tracepoint.h>

#ifdef CONFIG_SCHED_WALT
struct rq;
struct group_cpu_time;
extern const char *task_event_names[];

TRACE_EVENT(sched_update_pred_demand,

	TP_PROTO(struct task_struct *p, u32 runtime, int pct,
		 unsigned int pred_demand),

	TP_ARGS(p, runtime, pct, pred_demand),

	TP_STRUCT__entry(
		__array(char,		comm, TASK_COMM_LEN)
		__field(pid_t,		pid)
		__field(unsigned int,	runtime)
		__field(int,		pct)
		__field(unsigned int,	pred_demand)
		__array(u8,		bucket, NUM_BUSY_BUCKETS)
		__field(int,		cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->runtime        = runtime;
		__entry->pct            = pct;
		__entry->pred_demand     = pred_demand;
		memcpy(__entry->bucket, p->wts.busy_buckets,
					NUM_BUSY_BUCKETS * sizeof(u8));
		__entry->cpu            = task_cpu(p);
	),

	TP_printk("%d (%s): runtime %u pct %d cpu %d pred_demand %u (buckets: %u %u %u %u %u %u %u %u %u %u)",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->pct, __entry->cpu,
		__entry->pred_demand, __entry->bucket[0], __entry->bucket[1],
		__entry->bucket[2], __entry->bucket[3], __entry->bucket[4],
		__entry->bucket[5], __entry->bucket[6], __entry->bucket[7],
		__entry->bucket[8], __entry->bucket[9])
);

TRACE_EVENT(sched_update_history,

	TP_PROTO(struct rq *rq, struct task_struct *p, u32 runtime, int samples,
			enum task_event evt),

	TP_ARGS(rq, p, runtime, samples, evt),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(unsigned int,		runtime)
		__field(int,			samples)
		__field(enum task_event,	evt)
		__field(unsigned int,		demand)
		__field(unsigned int,		coloc_demand)
		__field(unsigned int,		pred_demand)
		__array(u32,			hist, RAVG_HIST_SIZE_MAX)
		__field(unsigned int,		nr_big_tasks)
		__field(int,			cpu)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->runtime        = runtime;
		__entry->samples        = samples;
		__entry->evt            = evt;
		__entry->demand         = p->wts.demand;
		__entry->coloc_demand   = p->wts.coloc_demand;
		__entry->pred_demand     = p->wts.pred_demand;
		memcpy(__entry->hist, p->wts.sum_history,
					RAVG_HIST_SIZE_MAX * sizeof(u32));
		__entry->nr_big_tasks   = rq->wrq.walt_stats.nr_big_tasks;
		__entry->cpu            = rq->cpu;
	),

	TP_printk("%d (%s): runtime %u samples %d event %s demand %u coloc_demand %u pred_demand %u (hist: %u %u %u %u %u) cpu %d nr_big %u",
		__entry->pid, __entry->comm,
		__entry->runtime, __entry->samples,
		task_event_names[__entry->evt],
		__entry->demand, __entry->coloc_demand, __entry->pred_demand,
		__entry->hist[0], __entry->hist[1],
		__entry->hist[2], __entry->hist[3],
		__entry->hist[4], __entry->cpu, __entry->nr_big_tasks)
);

TRACE_EVENT(sched_get_task_cpu_cycles,

	TP_PROTO(int cpu, int event, u64 cycles,
			u64 exec_time, struct task_struct *p),

	TP_ARGS(cpu, event, cycles, exec_time, p),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(int,	event)
		__field(u64,	cycles)
		__field(u64,	exec_time)
		__field(u32,	freq)
		__field(u32,	legacy_freq)
		__field(u32,	max_freq)
		__field(pid_t,	pid)
		__array(char,	comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		__entry->cpu		= cpu;
		__entry->event		= event;
		__entry->cycles		= cycles;
		__entry->exec_time	= exec_time;
		__entry->freq		= cpu_cycles_to_freq(cycles, exec_time);
		__entry->legacy_freq	= sched_cpu_legacy_freq(cpu);
		__entry->max_freq	= cpu_max_freq(cpu);
		__entry->pid		= p->pid;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
	),

	TP_printk("cpu=%d event=%d cycles=%llu exec_time=%llu freq=%u legacy_freq=%u max_freq=%u task=%d (%s)",
		  __entry->cpu, __entry->event, __entry->cycles,
		  __entry->exec_time, __entry->freq, __entry->legacy_freq,
		  __entry->max_freq, __entry->pid, __entry->comm)
);

TRACE_EVENT(sched_update_task_ravg,

	TP_PROTO(struct task_struct *p, struct rq *rq, enum task_event evt,
		 u64 wallclock, u64 irqtime,
		 struct group_cpu_time *cpu_time),

	TP_ARGS(p, rq, evt, wallclock, irqtime, cpu_time),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(pid_t,			cur_pid)
		__field(unsigned int,		cur_freq)
		__field(u64,			wallclock)
		__field(u64,			mark_start)
		__field(u64,			delta_m)
		__field(u64,			win_start)
		__field(u64,			delta)
		__field(u64,			irqtime)
		__field(enum task_event,	evt)
		__field(unsigned int,		demand)
		__field(unsigned int,		coloc_demand)
		__field(unsigned int,		sum)
		__field(int,			cpu)
		__field(unsigned int,		pred_demand)
		__field(u64,			rq_cs)
		__field(u64,			rq_ps)
		__field(u64,			grp_cs)
		__field(u64,			grp_ps)
		__field(u64,			grp_nt_cs)
		__field(u64,			grp_nt_ps)
		__field(u32,			curr_window)
		__field(u32,			prev_window)
		__dynamic_array(u32,		curr_sum, nr_cpu_ids)
		__dynamic_array(u32,		prev_sum, nr_cpu_ids)
		__field(u64,			nt_cs)
		__field(u64,			nt_ps)
		__field(u64,			active_time)
		__field(u32,			curr_top)
		__field(u32,			prev_top)
	),

	TP_fast_assign(
		__entry->wallclock      = wallclock;
		__entry->win_start      = rq->wrq.window_start;
		__entry->delta          = (wallclock - rq->wrq.window_start);
		__entry->evt            = evt;
		__entry->cpu            = rq->cpu;
		__entry->cur_pid        = rq->curr->pid;
		__entry->cur_freq       = rq->wrq.task_exec_scale;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->mark_start     = p->wts.mark_start;
		__entry->delta_m        = (wallclock - p->wts.mark_start);
		__entry->demand         = p->wts.demand;
		__entry->coloc_demand	= p->wts.coloc_demand;
		__entry->sum            = p->wts.sum;
		__entry->irqtime        = irqtime;
		__entry->pred_demand     = p->wts.pred_demand;
		__entry->rq_cs          = rq->wrq.curr_runnable_sum;
		__entry->rq_ps          = rq->wrq.prev_runnable_sum;
		__entry->grp_cs = cpu_time ? cpu_time->curr_runnable_sum : 0;
		__entry->grp_ps = cpu_time ? cpu_time->prev_runnable_sum : 0;
		__entry->grp_nt_cs = cpu_time ?
					cpu_time->nt_curr_runnable_sum : 0;
		__entry->grp_nt_ps = cpu_time ?
					cpu_time->nt_prev_runnable_sum : 0;
		__entry->curr_window	= p->wts.curr_window;
		__entry->prev_window	= p->wts.prev_window;
		__window_data(__get_dynamic_array(curr_sum),
						p->wts.curr_window_cpu);
		__window_data(__get_dynamic_array(prev_sum),
						p->wts.prev_window_cpu);
		__entry->nt_cs		= rq->wrq.nt_curr_runnable_sum;
		__entry->nt_ps		= rq->wrq.nt_prev_runnable_sum;
		__entry->active_time	= p->wts.active_time;
		__entry->curr_top	= rq->wrq.curr_top;
		__entry->prev_top	= rq->wrq.prev_top;
	),

	TP_printk("wc %llu ws %llu delta %llu event %s cpu %d cur_freq %u cur_pid %d task %d (%s) ms %llu delta %llu demand %u coloc_demand: %u sum %u irqtime %llu pred_demand %u rq_cs %llu rq_ps %llu cur_window %u (%s) prev_window %u (%s) nt_cs %llu nt_ps %llu active_time %u grp_cs %lld grp_ps %lld, grp_nt_cs %llu, grp_nt_ps: %llu curr_top %u prev_top %u",
		__entry->wallclock, __entry->win_start, __entry->delta,
		task_event_names[__entry->evt], __entry->cpu,
		__entry->cur_freq, __entry->cur_pid,
		__entry->pid, __entry->comm, __entry->mark_start,
		__entry->delta_m, __entry->demand, __entry->coloc_demand,
		__entry->sum, __entry->irqtime, __entry->pred_demand,
		__entry->rq_cs, __entry->rq_ps, __entry->curr_window,
		__window_print(p, __get_dynamic_array(curr_sum), nr_cpu_ids),
		__entry->prev_window,
		__window_print(p, __get_dynamic_array(prev_sum), nr_cpu_ids),
		__entry->nt_cs, __entry->nt_ps,
		__entry->active_time, __entry->grp_cs,
		__entry->grp_ps, __entry->grp_nt_cs, __entry->grp_nt_ps,
		__entry->curr_top, __entry->prev_top)
);

TRACE_EVENT(sched_update_task_ravg_mini,

	TP_PROTO(struct task_struct *p, struct rq *rq, enum task_event evt,
		 u64 wallclock, u64 irqtime,
		 struct group_cpu_time *cpu_time),

	TP_ARGS(p, rq, evt, wallclock, irqtime, cpu_time),

	TP_STRUCT__entry(
		__array(char,			comm, TASK_COMM_LEN)
		__field(pid_t,			pid)
		__field(u64,			wallclock)
		__field(u64,			mark_start)
		__field(u64,			delta_m)
		__field(u64,			win_start)
		__field(u64,			delta)
		__field(enum task_event,	evt)
		__field(unsigned int,		demand)
		__field(int,			cpu)
		__field(u64,			rq_cs)
		__field(u64,			rq_ps)
		__field(u64,			grp_cs)
		__field(u64,			grp_ps)
		__field(u32,			curr_window)
		__field(u32,			prev_window)
	),

	TP_fast_assign(
		__entry->wallclock      = wallclock;
		__entry->win_start      = rq->wrq.window_start;
		__entry->delta          = (wallclock - rq->wrq.window_start);
		__entry->evt            = evt;
		__entry->cpu            = rq->cpu;
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid            = p->pid;
		__entry->mark_start     = p->wts.mark_start;
		__entry->delta_m        = (wallclock - p->wts.mark_start);
		__entry->demand         = p->wts.demand;
		__entry->rq_cs          = rq->wrq.curr_runnable_sum;
		__entry->rq_ps          = rq->wrq.prev_runnable_sum;
		__entry->grp_cs = cpu_time ? cpu_time->curr_runnable_sum : 0;
		__entry->grp_ps = cpu_time ? cpu_time->prev_runnable_sum : 0;
		__entry->curr_window	= p->wts.curr_window;
		__entry->prev_window	= p->wts.prev_window;
	),

	TP_printk("wc %llu ws %llu delta %llu event %s cpu %d task %d (%s) ms %llu delta %llu demand %u rq_cs %llu rq_ps %llu cur_window %u prev_window %u grp_cs %lld grp_ps %lld",
		__entry->wallclock, __entry->win_start, __entry->delta,
		task_event_names[__entry->evt], __entry->cpu,
		__entry->pid, __entry->comm, __entry->mark_start,
		__entry->delta_m, __entry->demand,
		__entry->rq_cs, __entry->rq_ps, __entry->curr_window,
		__entry->prev_window, __entry->grp_cs, __entry->grp_ps)
);

struct migration_sum_data;
extern const char *migrate_type_names[];

TRACE_EVENT(sched_set_preferred_cluster,

	TP_PROTO(struct walt_related_thread_group *grp, u64 total_demand),

	TP_ARGS(grp, total_demand),

	TP_STRUCT__entry(
		__field(int,		id)
		__field(u64,		total_demand)
		__field(bool,		skip_min)
	),

	TP_fast_assign(
		__entry->id		= grp->id;
		__entry->total_demand	= total_demand;
		__entry->skip_min	= grp->skip_min;
	),

	TP_printk("group_id %d total_demand %llu skip_min %d",
			__entry->id, __entry->total_demand,
			__entry->skip_min)
);

TRACE_EVENT(sched_migration_update_sum,

	TP_PROTO(struct task_struct *p, enum migrate_types migrate_type,
							struct rq *rq),

	TP_ARGS(p, migrate_type, rq),

	TP_STRUCT__entry(
		__field(int,			tcpu)
		__field(int,			pid)
		__field(enum migrate_types,	migrate_type)
		__field(s64,			src_cs)
		__field(s64,			src_ps)
		__field(s64,			dst_cs)
		__field(s64,			dst_ps)
		__field(s64,			src_nt_cs)
		__field(s64,			src_nt_ps)
		__field(s64,			dst_nt_cs)
		__field(s64,			dst_nt_ps)
	),

	TP_fast_assign(
		__entry->tcpu		= task_cpu(p);
		__entry->pid		= p->pid;
		__entry->migrate_type	= migrate_type;
		__entry->src_cs		= __get_update_sum(rq, migrate_type,
							   true, false, true);
		__entry->src_ps		= __get_update_sum(rq, migrate_type,
							   true, false, false);
		__entry->dst_cs		= __get_update_sum(rq, migrate_type,
							   false, false, true);
		__entry->dst_ps		= __get_update_sum(rq, migrate_type,
							   false, false, false);
		__entry->src_nt_cs	= __get_update_sum(rq, migrate_type,
							   true, true, true);
		__entry->src_nt_ps	= __get_update_sum(rq, migrate_type,
							   true, true, false);
		__entry->dst_nt_cs	= __get_update_sum(rq, migrate_type,
							   false, true, true);
		__entry->dst_nt_ps	= __get_update_sum(rq, migrate_type,
							   false, true, false);
	),

	TP_printk("pid %d task_cpu %d migrate_type %s src_cs %llu src_ps %llu dst_cs %lld dst_ps %lld src_nt_cs %llu src_nt_ps %llu dst_nt_cs %lld dst_nt_ps %lld",
		__entry->pid, __entry->tcpu,
		migrate_type_names[__entry->migrate_type],
		__entry->src_cs, __entry->src_ps, __entry->dst_cs,
		__entry->dst_ps, __entry->src_nt_cs, __entry->src_nt_ps,
		__entry->dst_nt_cs, __entry->dst_nt_ps)
);

TRACE_EVENT(sched_set_boost,

	TP_PROTO(int type),

	TP_ARGS(type),

	TP_STRUCT__entry(
		__field(int, type)
	),

	TP_fast_assign(
		__entry->type = type;
	),

	TP_printk("type %d", __entry->type)
);

TRACE_EVENT(sched_load_to_gov,

	TP_PROTO(struct rq *rq, u64 aggr_grp_load, u32 tt_load,
		int freq_aggr, u64 load, int policy,
		int big_task_rotation,
		unsigned int user_hint),
	TP_ARGS(rq, aggr_grp_load, tt_load, freq_aggr, load, policy,
		big_task_rotation, user_hint),

	TP_STRUCT__entry(
		__field(int,	cpu)
		__field(int,    policy)
		__field(int,	ed_task_pid)
		__field(u64,    aggr_grp_load)
		__field(int,    freq_aggr)
		__field(u64,    tt_load)
		__field(u64,	rq_ps)
		__field(u64,	grp_rq_ps)
		__field(u64,	nt_ps)
		__field(u64,	grp_nt_ps)
		__field(u64,	pl)
		__field(u64,    load)
		__field(int,    big_task_rotation)
		__field(unsigned int, user_hint)
	),

	TP_fast_assign(
		__entry->cpu		= cpu_of(rq);
		__entry->policy		= policy;
		__entry->ed_task_pid	=
				rq->wrq.ed_task ? rq->wrq.ed_task->pid : -1;
		__entry->aggr_grp_load	= aggr_grp_load;
		__entry->freq_aggr	= freq_aggr;
		__entry->tt_load	= tt_load;
		__entry->rq_ps		= rq->wrq.prev_runnable_sum;
		__entry->grp_rq_ps	= rq->wrq.grp_time.prev_runnable_sum;
		__entry->nt_ps		= rq->wrq.nt_prev_runnable_sum;
		__entry->grp_nt_ps	= rq->wrq.grp_time.nt_prev_runnable_sum;
		__entry->pl		=
				rq->wrq.walt_stats.pred_demands_sum_scaled;
		__entry->load		= load;
		__entry->big_task_rotation = big_task_rotation;
		__entry->user_hint = user_hint;
	),

	TP_printk("cpu=%d policy=%d ed_task_pid=%d aggr_grp_load=%llu freq_aggr=%d tt_load=%llu rq_ps=%llu grp_rq_ps=%llu nt_ps=%llu grp_nt_ps=%llu pl=%llu load=%llu big_task_rotation=%d user_hint=%u",
		__entry->cpu, __entry->policy, __entry->ed_task_pid,
		__entry->aggr_grp_load, __entry->freq_aggr,
		__entry->tt_load, __entry->rq_ps, __entry->grp_rq_ps,
		__entry->nt_ps, __entry->grp_nt_ps, __entry->pl, __entry->load,
		__entry->big_task_rotation, __entry->user_hint)
);

TRACE_EVENT(core_ctl_eval_need,

	TP_PROTO(unsigned int cpu, unsigned int old_need,
		unsigned int new_need, unsigned int updated),
	TP_ARGS(cpu, old_need, new_need, updated),
	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, old_need)
		__field(u32, new_need)
		__field(u32, updated)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->old_need = old_need;
		__entry->new_need = new_need;
		__entry->updated = updated;
	),
	TP_printk("cpu=%u, old_need=%u, new_need=%u, updated=%u", __entry->cpu,
			__entry->old_need, __entry->new_need, __entry->updated)
);

TRACE_EVENT(core_ctl_set_busy,

	TP_PROTO(unsigned int cpu, unsigned int busy,
		unsigned int old_is_busy, unsigned int is_busy),
	TP_ARGS(cpu, busy, old_is_busy, is_busy),
	TP_STRUCT__entry(
		__field(u32, cpu)
		__field(u32, busy)
		__field(u32, old_is_busy)
		__field(u32, is_busy)
		__field(bool, high_irqload)
	),
	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->busy = busy;
		__entry->old_is_busy = old_is_busy;
		__entry->is_busy = is_busy;
		__entry->high_irqload = sched_cpu_high_irqload(cpu);
	),
	TP_printk("cpu=%u, busy=%u, old_is_busy=%u, new_is_busy=%u high_irqload=%d",
		__entry->cpu, __entry->busy, __entry->old_is_busy,
		__entry->is_busy, __entry->high_irqload)
);

TRACE_EVENT(core_ctl_set_boost,

	TP_PROTO(u32 refcount, s32 ret),
	TP_ARGS(refcount, ret),
	TP_STRUCT__entry(
		__field(u32, refcount)
		__field(s32, ret)
	),
	TP_fast_assign(
		__entry->refcount = refcount;
		__entry->ret = ret;
	),
	TP_printk("refcount=%u, ret=%d", __entry->refcount, __entry->ret)
);

TRACE_EVENT(core_ctl_update_nr_need,

	TP_PROTO(int cpu, int nr_need, int prev_misfit_need,
		int nrrun, int max_nr, int nr_prev_assist),

	TP_ARGS(cpu, nr_need, prev_misfit_need, nrrun, max_nr, nr_prev_assist),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, nr_need)
		__field(int, prev_misfit_need)
		__field(int, nrrun)
		__field(int, max_nr)
		__field(int, nr_prev_assist)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->nr_need = nr_need;
		__entry->prev_misfit_need = prev_misfit_need;
		__entry->nrrun = nrrun;
		__entry->max_nr = max_nr;
		__entry->nr_prev_assist = nr_prev_assist;
	),

	TP_printk("cpu=%d nr_need=%d prev_misfit_need=%d nrrun=%d max_nr=%d nr_prev_assist=%d",
		__entry->cpu, __entry->nr_need, __entry->prev_misfit_need,
		__entry->nrrun, __entry->max_nr, __entry->nr_prev_assist)
);

TRACE_EVENT(core_ctl_notif_data,

	TP_PROTO(u32 nr_big, u32 ta_load, u32 *ta_util, u32 *cur_cap),

	TP_ARGS(nr_big, ta_load, ta_util, cur_cap),

	TP_STRUCT__entry(
		__field(u32, nr_big)
		__field(u32, ta_load)
		__array(u32, ta_util, MAX_CLUSTERS)
		__array(u32, cur_cap, MAX_CLUSTERS)
	),

	TP_fast_assign(
		__entry->nr_big = nr_big;
		__entry->ta_load = ta_load;
		memcpy(__entry->ta_util, ta_util, MAX_CLUSTERS * sizeof(u32));
		memcpy(__entry->cur_cap, cur_cap, MAX_CLUSTERS * sizeof(u32));
	),

	TP_printk("nr_big=%u ta_load=%u ta_util=(%u %u %u) cur_cap=(%u %u %u)",
		  __entry->nr_big, __entry->ta_load,
		  __entry->ta_util[0], __entry->ta_util[1],
		  __entry->ta_util[2], __entry->cur_cap[0],
		  __entry->cur_cap[1], __entry->cur_cap[2])
);

/*
 * Tracepoint for sched_get_nr_running_avg
 */
TRACE_EVENT(sched_get_nr_running_avg,

	TP_PROTO(int cpu, int nr, int nr_misfit, int nr_max, int nr_scaled),

	TP_ARGS(cpu, nr, nr_misfit, nr_max, nr_scaled),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, nr)
		__field(int, nr_misfit)
		__field(int, nr_max)
		__field(int, nr_scaled)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->nr = nr;
		__entry->nr_misfit = nr_misfit;
		__entry->nr_max = nr_max;
		__entry->nr_scaled = nr_scaled;
	),

	TP_printk("cpu=%d nr=%d nr_misfit=%d nr_max=%d nr_scaled=%d",
		__entry->cpu, __entry->nr, __entry->nr_misfit, __entry->nr_max,
		__entry->nr_scaled)
);

/*
 * sched_isolate - called when cores are isolated/unisolated
 *
 * @acutal_mask: mask of cores actually isolated/unisolated
 * @req_mask: mask of cores requested isolated/unisolated
 * @online_mask: cpu online mask
 * @time: amount of time in us it took to isolate/unisolate
 * @isolate: 1 if isolating, 0 if unisolating
 *
 */
TRACE_EVENT(sched_isolate,

	TP_PROTO(unsigned int requested_cpu, unsigned int isolated_cpus,
		u64 start_time, unsigned char isolate),

	TP_ARGS(requested_cpu, isolated_cpus, start_time, isolate),

	TP_STRUCT__entry(
		__field(u32, requested_cpu)
		__field(u32, isolated_cpus)
		__field(u32, time)
		__field(unsigned char, isolate)
	),

	TP_fast_assign(
		__entry->requested_cpu = requested_cpu;
		__entry->isolated_cpus = isolated_cpus;
		__entry->time = div64_u64(sched_clock() - start_time, 1000);
		__entry->isolate = isolate;
	),

	TP_printk("iso cpu=%u cpus=0x%x time=%u us isolated=%d",
		__entry->requested_cpu, __entry->isolated_cpus,
		__entry->time, __entry->isolate)
);

TRACE_EVENT(sched_ravg_window_change,

	TP_PROTO(unsigned int sched_ravg_window, unsigned int new_sched_ravg_window
		, u64 change_time),

	TP_ARGS(sched_ravg_window, new_sched_ravg_window, change_time),

	TP_STRUCT__entry(
		__field(unsigned int, sched_ravg_window)
		__field(unsigned int, new_sched_ravg_window)
		__field(u64, change_time)
	),

	TP_fast_assign(
		__entry->sched_ravg_window = sched_ravg_window;
		__entry->new_sched_ravg_window = new_sched_ravg_window;
		__entry->change_time = change_time;
	),

	TP_printk("from=%u to=%u at=%lu",
		__entry->sched_ravg_window, __entry->new_sched_ravg_window,
		__entry->change_time)
);

TRACE_EVENT(walt_window_rollover,

	TP_PROTO(u64 window_start),

	TP_ARGS(window_start),

	TP_STRUCT__entry(
		__field(u64, window_start)
	),

	TP_fast_assign(
		__entry->window_start = window_start;
	),

	TP_printk("window_start=%llu", __entry->window_start)
);

#endif /* CONFIG_SCHED_WALT */
#endif /* _TRACE_WALT_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace

#include <trace/define_trace.h>
