/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __WALT_H
#define __WALT_H

#ifdef CONFIG_SCHED_WALT

#include <linux/sched/sysctl.h>
#include <linux/sched/core_ctl.h>

#define EXITING_TASK_MARKER	0xdeaddead

extern unsigned int __weak walt_rotation_enabled;

extern void __weak
walt_update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime);

static inline void
fixup_cumulative_runnable_avg(struct walt_sched_stats *stats,
			      s64 demand_scaled_delta,
			      s64 pred_demand_scaled_delta)
{
	if (sched_disable_window_stats)
		return;

	stats->cumulative_runnable_avg_scaled += demand_scaled_delta;
	BUG_ON((s64)stats->cumulative_runnable_avg_scaled < 0);

	stats->pred_demands_sum_scaled += pred_demand_scaled_delta;
	BUG_ON((s64)stats->pred_demands_sum_scaled < 0);
}

static inline void
walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	fixup_cumulative_runnable_avg(&rq->walt_stats, p->ravg.demand_scaled,
				      p->ravg.pred_demand_scaled);

	/*
	 * Add a task's contribution to the cumulative window demand when
	 *
	 * (1) task is enqueued with on_rq = 1 i.e migration,
	 *     prio/cgroup/class change.
	 * (2) task is waking for the first time in this window.
	 */
	if (p->on_rq || (p->last_sleep_ts < rq->window_start))
		walt_fixup_cum_window_demand(rq, p->ravg.demand_scaled);
}

static inline void
walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	fixup_cumulative_runnable_avg(&rq->walt_stats,
				      -(s64)p->ravg.demand_scaled,
				      -(s64)p->ravg.pred_demand_scaled);

	/*
	 * on_rq will be 1 for sleeping tasks. So check if the task
	 * is migrating or dequeuing in RUNNING state to change the
	 * prio/cgroup/class.
	 */
	if (task_on_rq_migrating(p) || p->state == TASK_RUNNING)
		walt_fixup_cum_window_demand(rq, -(s64)p->ravg.demand_scaled);
}

extern void __weak
fixup_walt_sched_stats_common(struct rq *rq, struct task_struct *p,
					  u16 updated_demand_scaled,
					  u16 updated_pred_demand_scaled);
extern void __weak inc_rq_walt_stats(struct rq *rq, struct task_struct *p);
extern void __weak dec_rq_walt_stats(struct rq *rq, struct task_struct *p);
extern void __weak fixup_busy_time(struct task_struct *p, int new_cpu);
extern void __weak init_new_task_load(struct task_struct *p);
extern void __weak mark_task_starting(struct task_struct *p);
extern void __weak set_window_start(struct rq *rq);
extern bool __weak do_pl_notif(struct rq *rq);

/*
 * This is only for tracepoints to print the avg irq load. For
 * task placment considerations, use sched_cpu_high_irqload().
 */
#define SCHED_HIGH_IRQ_TIMEOUT 3
static inline u64 sched_irqload(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	s64 delta;

	delta = get_jiffies_64() - rq->irqload_ts;
	/*
	 * Current context can be preempted by irq and rq->irqload_ts can be
	 * updated by irq context so that delta can be negative.
	 * But this is okay and we can safely return as this means there
	 * was recent irq occurrence.
	 */

	if (delta < SCHED_HIGH_IRQ_TIMEOUT)
		return rq->avg_irqload;
	else
		return 0;
}

static inline int sched_cpu_high_irqload(int cpu)
{
	return cpu_rq(cpu)->high_irqload;
}

static inline int exiting_task(struct task_struct *p)
{
	return (p->ravg.sum_history[0] == EXITING_TASK_MARKER);
}

static inline u64
scale_load_to_freq(u64 load, unsigned int src_freq, unsigned int dst_freq)
{
	return div64_u64(load * (u64)src_freq, (u64)dst_freq);
}

extern void __weak sched_account_irqstart(int cpu, struct task_struct *curr,
				   u64 wallclock);

static inline unsigned int max_task_load(void)
{
	return sched_ravg_window;
}

extern void __weak update_cluster_topology(void);

extern void __weak init_clusters(void);

extern void sched_account_irqtime(int cpu, struct task_struct *curr,
				 u64 delta, u64 wallclock);

static inline int same_cluster(int src_cpu, int dst_cpu)
{
	return cpu_rq(src_cpu)->cluster == cpu_rq(dst_cpu)->cluster;
}

void __weak walt_sched_init_rq(struct rq *rq);

static inline void walt_update_last_enqueue(struct task_struct *p)
{
	p->last_enqueued_ts = sched_ktime_clock();
}

static inline bool is_suh_max(void)
{
	return sysctl_sched_user_hint == sched_user_hint_max;
}

#define DEFAULT_CGROUP_COLOC_ID 1
static inline bool walt_should_kick_upmigrate(struct task_struct *p, int cpu)
{
	struct related_thread_group *rtg = p->grp;

	if (is_suh_max() && rtg && rtg->id == DEFAULT_CGROUP_COLOC_ID &&
			    rtg->skip_min && p->unfilter)
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
	struct related_thread_group *grp = NULL;

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

static inline void update_cluster_topology(void) { }
static inline void init_clusters(void) {}
static inline void sched_account_irqtime(int cpu, struct task_struct *curr,
				 u64 delta, u64 wallclock)
{
}

static inline int same_cluster(int src_cpu, int dst_cpu) { return 1; }
static inline bool do_pl_notif(struct rq *rq) { return false; }

static inline void
inc_rq_walt_stats(struct rq *rq, struct task_struct *p) { }

static inline void
dec_rq_walt_stats(struct rq *rq, struct task_struct *p) { }

static inline void
fixup_walt_sched_stats_common(struct rq *rq, struct task_struct *p,
			      u16 updated_demand_scaled,
			      u16 updated_pred_demand_scaled)
{
}

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
