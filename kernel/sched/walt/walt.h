/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef __WALT_H
#define __WALT_H

#ifdef CONFIG_SCHED_WALT

#include <linux/sched/sysctl.h>
#include <linux/sched/core_ctl.h>

#define EXITING_TASK_MARKER	0xdeaddead

extern unsigned int walt_rotation_enabled;
extern int __read_mostly num_sched_clusters;
extern cpumask_t __read_mostly **cpu_array;
extern void
walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime);

static inline void
fixup_cumulative_runnable_avg(struct walt_sched_stats *stats,
			      s64 demand_scaled_delta,
			      s64 pred_demand_scaled_delta)
{
	stats->cumulative_runnable_avg_scaled += demand_scaled_delta;
	BUG_ON((s64)stats->cumulative_runnable_avg_scaled < 0);

	stats->pred_demands_sum_scaled += pred_demand_scaled_delta;
	BUG_ON((s64)stats->pred_demands_sum_scaled < 0);
}

static inline void
walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	fixup_cumulative_runnable_avg(&rq->wrq.walt_stats, p->wts.demand_scaled,
					p->wts.pred_demand_scaled);

	/*
	 * Add a task's contribution to the cumulative window demand when
	 *
	 * (1) task is enqueued with on_rq = 1 i.e migration,
	 *     prio/cgroup/class change.
	 * (2) task is waking for the first time in this window.
	 */
	if (p->on_rq || (p->wts.last_sleep_ts < rq->wrq.window_start))
		walt_fixup_cum_window_demand(rq, p->wts.demand_scaled);
}

static inline void
walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	fixup_cumulative_runnable_avg(&rq->wrq.walt_stats,
				      -(s64)p->wts.demand_scaled,
				      -(s64)p->wts.pred_demand_scaled);

	/*
	 * on_rq will be 1 for sleeping tasks. So check if the task
	 * is migrating or dequeuing in RUNNING state to change the
	 * prio/cgroup/class.
	 */
	if (task_on_rq_migrating(p) || p->state == TASK_RUNNING)
		walt_fixup_cum_window_demand(rq, -(s64)p->wts.demand_scaled);
}

static inline void walt_adjust_nr_big_tasks(struct rq *rq, int delta, bool inc)
{
	sched_update_nr_prod(cpu_of(rq), 0, true);
	rq->wrq.walt_stats.nr_big_tasks += inc ? delta : -delta;

	BUG_ON(rq->wrq.walt_stats.nr_big_tasks < 0);
}

static inline void inc_rq_walt_stats(struct rq *rq, struct task_struct *p)
{
	if (p->wts.misfit)
		rq->wrq.walt_stats.nr_big_tasks++;

	p->wts.rtg_high_prio = task_rtg_high_prio(p);
	if (p->wts.rtg_high_prio)
		rq->wrq.walt_stats.nr_rtg_high_prio_tasks++;

	walt_inc_cumulative_runnable_avg(rq, p);
}

static inline void dec_rq_walt_stats(struct rq *rq, struct task_struct *p)
{
	if (p->wts.misfit)
		rq->wrq.walt_stats.nr_big_tasks--;

	if (p->wts.rtg_high_prio)
		rq->wrq.walt_stats.nr_rtg_high_prio_tasks--;

	BUG_ON(rq->wrq.walt_stats.nr_big_tasks < 0);

	walt_dec_cumulative_runnable_avg(rq, p);
}

extern void fixup_busy_time(struct task_struct *p, int new_cpu);
extern void init_new_task_load(struct task_struct *p);
extern void mark_task_starting(struct task_struct *p);
extern void set_window_start(struct rq *rq);
extern bool do_pl_notif(struct rq *rq);

/*
 * This is only for tracepoints to print the avg irq load. For
 * task placment considerations, use sched_cpu_high_irqload().
 */
#define SCHED_HIGH_IRQ_TIMEOUT 3
static inline u64 sched_irqload(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	s64 delta;

	delta = rq->wrq.window_start - rq->wrq.last_irq_window;
	if (delta < SCHED_HIGH_IRQ_TIMEOUT)
		return rq->wrq.avg_irqload;
	else
		return 0;
}

static inline int sched_cpu_high_irqload(int cpu)
{
	return cpu_rq(cpu)->wrq.high_irqload;
}

static inline u64
scale_load_to_freq(u64 load, unsigned int src_freq, unsigned int dst_freq)
{
	return div64_u64(load * (u64)src_freq, (u64)dst_freq);
}

extern void walt_sched_account_irqstart(int cpu, struct task_struct *curr);
extern void walt_sched_account_irqend(int cpu, struct task_struct *curr,
				      u64 delta);

static inline unsigned int max_task_load(void)
{
	return sched_ravg_window;
}

extern void init_clusters(void);

static inline int same_cluster(int src_cpu, int dst_cpu)
{
	return cpu_rq(src_cpu)->wrq.cluster == cpu_rq(dst_cpu)->wrq.cluster;
}

void walt_sched_init_rq(struct rq *rq);

static inline void walt_update_last_enqueue(struct task_struct *p)
{
	p->wts.last_enqueued_ts = sched_ktime_clock();
}

static inline bool is_suh_max(void)
{
	return sysctl_sched_user_hint == sched_user_hint_max;
}

#define DEFAULT_CGROUP_COLOC_ID 1
static inline bool walt_should_kick_upmigrate(struct task_struct *p, int cpu)
{
	struct walt_related_thread_group *rtg = p->wts.grp;

	if (is_suh_max() && rtg && rtg->id == DEFAULT_CGROUP_COLOC_ID &&
			    rtg->skip_min && p->wts.unfilter)
		return is_min_capacity_cpu(cpu);

	return false;
}

extern bool is_rtgb_active(void);
extern u64 get_rtgb_active_time(void);

/* utility function to update walt signals at wakeup */
static inline void walt_try_to_wake_up(struct task_struct *p)
{
	struct rq *rq = cpu_rq(task_cpu(p));
	struct rq_flags rf;
	u64 wallclock;
	unsigned int old_load;
	struct walt_related_thread_group *grp = NULL;

	rq_lock_irqsave(rq, &rf);
	old_load = task_load(p);
	wallclock = sched_ktime_clock();
	walt_update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	walt_update_task_ravg(p, rq, TASK_WAKE, wallclock, 0);
	note_task_waking(p, wallclock);
	rq_unlock_irqrestore(rq, &rf);

	rcu_read_lock();
	grp = task_related_thread_group(p);
	if (update_preferred_cluster(grp, p, old_load, false))
		set_preferred_cluster(grp);
	rcu_read_unlock();
}

static inline unsigned int walt_nr_rtg_high_prio(int cpu)
{
	return cpu_rq(cpu)->wrq.walt_stats.nr_rtg_high_prio_tasks;
}

extern int core_ctl_init(void);

#ifdef CONFIG_CPU_FREQ
extern int cpu_boost_init(void);
#else
static inline int cpu_boost_init(void) { }
#endif

#else /* CONFIG_SCHED_WALT */

static inline void walt_sched_init_rq(struct rq *rq) { }
static inline void walt_update_last_enqueue(struct task_struct *p) { }
static inline void walt_update_task_ravg(struct task_struct *p, struct rq *rq,
				int event, u64 wallclock, u64 irqtime) { }
static inline void walt_inc_cumulative_runnable_avg(struct rq *rq,
		struct task_struct *p)
{
}

static inline void walt_dec_cumulative_runnable_avg(struct rq *rq,
		 struct task_struct *p)
{
}

static inline void fixup_busy_time(struct task_struct *p, int new_cpu) { }
static inline void init_new_task_load(struct task_struct *p)
{
}

static inline void mark_task_starting(struct task_struct *p) { }
static inline void set_window_start(struct rq *rq) { }
static inline int sched_cpu_high_irqload(int cpu) { return 0; }

static inline void sched_account_irqstart(int cpu, struct task_struct *curr,
					  u64 wallclock)
{
}

static inline void init_clusters(void) {}

static inline void walt_sched_account_irqstart(int cpu,
					       struct task_struct *curr)
{
}
static inline void walt_sched_account_irqend(int cpu, struct task_struct *curr,
					     u64 delta)
{
}

static inline int same_cluster(int src_cpu, int dst_cpu) { return 1; }
static inline bool do_pl_notif(struct rq *rq) { return false; }

static inline void
inc_rq_walt_stats(struct rq *rq, struct task_struct *p) { }

static inline void
dec_rq_walt_stats(struct rq *rq, struct task_struct *p) { }

static inline u64 sched_irqload(int cpu)
{
	return 0;
}

static inline bool walt_should_kick_upmigrate(struct task_struct *p, int cpu)
{
	return false;
}

static inline u64 get_rtgb_active_time(void)
{
	return 0;
}

#define walt_try_to_wake_up(a) {}

#endif /* CONFIG_SCHED_WALT */

#endif
