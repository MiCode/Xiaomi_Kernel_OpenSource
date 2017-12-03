/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __WALT_H
#define __WALT_H

#ifdef CONFIG_SCHED_WALT

#include <linux/sched/sysctl.h>

#define WINDOW_STATS_RECENT		0
#define WINDOW_STATS_MAX		1
#define WINDOW_STATS_MAX_RECENT_AVG	2
#define WINDOW_STATS_AVG		3
#define WINDOW_STATS_INVALID_POLICY	4

#define EXITING_TASK_MARKER	0xdeaddead

#define FREQ_REPORT_MAX_CPU_LOAD_TOP_TASK	0
#define FREQ_REPORT_CPU_LOAD			1
#define FREQ_REPORT_TOP_TASK			2

#define for_each_related_thread_group(grp) \
	list_for_each_entry(grp, &active_related_thread_groups, list)

#define SCHED_NEW_TASK_WINDOWS 5

extern unsigned int sched_ravg_window;
extern unsigned int max_possible_efficiency;
extern unsigned int min_possible_efficiency;
extern unsigned int max_possible_freq;
extern unsigned int sched_major_task_runtime;
extern unsigned int __read_mostly sched_init_task_load_windows;
extern unsigned int __read_mostly sched_load_granule;

extern struct mutex cluster_lock;
extern rwlock_t related_thread_group_lock;
extern __read_mostly unsigned int sched_ravg_hist_size;
extern __read_mostly unsigned int sched_freq_aggregate;
extern __read_mostly int sched_freq_aggregate_threshold;
extern __read_mostly unsigned int sched_window_stats_policy;
extern __read_mostly unsigned int sched_group_upmigrate;
extern __read_mostly unsigned int sched_group_downmigrate;

extern struct sched_cluster init_cluster;

extern void update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime);

extern unsigned int nr_eligible_big_tasks(int cpu);

static inline void
inc_nr_big_task(struct walt_sched_stats *stats, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	if (p->misfit)
		stats->nr_big_tasks++;
}

static inline void
dec_nr_big_task(struct walt_sched_stats *stats, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	if (p->misfit)
		stats->nr_big_tasks--;

	BUG_ON(stats->nr_big_tasks < 0);
}

static inline void
walt_adjust_nr_big_tasks(struct rq *rq, int delta, bool inc)
{
	if (sched_disable_window_stats)
		return;

	sched_update_nr_prod(cpu_of(rq), 0, true);
	rq->walt_stats.nr_big_tasks += inc ? delta : -delta;

	BUG_ON(rq->walt_stats.nr_big_tasks < 0);
}

static inline void
fixup_cumulative_runnable_avg(struct walt_sched_stats *stats,
			      s64 task_load_delta, s64 pred_demand_delta)
{
	if (sched_disable_window_stats)
		return;

	stats->cumulative_runnable_avg += task_load_delta;
	BUG_ON((s64)stats->cumulative_runnable_avg < 0);

	stats->pred_demands_sum += pred_demand_delta;
	BUG_ON((s64)stats->pred_demands_sum < 0);
}

static inline void
walt_inc_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	fixup_cumulative_runnable_avg(&rq->walt_stats, p->ravg.demand,
				      p->ravg.pred_demand);

	/*
	 * Add a task's contribution to the cumulative window demand when
	 *
	 * (1) task is enqueued with on_rq = 1 i.e migration,
	 *     prio/cgroup/class change.
	 * (2) task is waking for the first time in this window.
	 */
	if (p->on_rq || (p->last_sleep_ts < rq->window_start))
		walt_fixup_cum_window_demand(rq, p->ravg.demand);
}

static inline void
walt_dec_cumulative_runnable_avg(struct rq *rq, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	fixup_cumulative_runnable_avg(&rq->walt_stats, -(s64)p->ravg.demand,
				      -(s64)p->ravg.pred_demand);

	/*
	 * on_rq will be 1 for sleeping tasks. So check if the task
	 * is migrating or dequeuing in RUNNING state to change the
	 * prio/cgroup/class.
	 */
	if (task_on_rq_migrating(p) || p->state == TASK_RUNNING)
		walt_fixup_cum_window_demand(rq, -(s64)p->ravg.demand);
}

extern void fixup_walt_sched_stats_common(struct rq *rq, struct task_struct *p,
					  u32 new_task_load,
					  u32 new_pred_demand);
extern void inc_rq_walt_stats(struct rq *rq, struct task_struct *p);
extern void dec_rq_walt_stats(struct rq *rq, struct task_struct *p);
extern void fixup_busy_time(struct task_struct *p, int new_cpu);
extern void init_new_task_load(struct task_struct *p, bool idle_task);
extern void mark_task_starting(struct task_struct *p);
extern void set_window_start(struct rq *rq);
void account_irqtime(int cpu, struct task_struct *curr, u64 delta,
                                  u64 wallclock);
extern bool do_pl_notif(struct rq *rq);

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
	return sched_irqload(cpu) >= sysctl_sched_cpu_high_irqload;
}

static inline int exiting_task(struct task_struct *p)
{
	return (p->ravg.sum_history[0] == EXITING_TASK_MARKER);
}

static inline struct sched_cluster *cpu_cluster(int cpu)
{
	return cpu_rq(cpu)->cluster;
}

static inline u64
scale_load_to_freq(u64 load, unsigned int src_freq, unsigned int dst_freq)
{
	return div64_u64(load * (u64)src_freq, (u64)dst_freq);
}

static inline bool is_new_task(struct task_struct *p)
{
	return p->ravg.active_windows < SCHED_NEW_TASK_WINDOWS;
}

static inline void clear_top_tasks_table(u8 *table)
{
	memset(table, 0, NUM_LOAD_INDICES * sizeof(u8));
}

extern void update_cluster_load_subtractions(struct task_struct *p,
					int cpu, u64 ws, bool new_task);
extern void sched_account_irqstart(int cpu, struct task_struct *curr,
				   u64 wallclock);

static inline unsigned int max_task_load(void)
{
	return sched_ravg_window;
}

static inline u32 cpu_cycles_to_freq(u64 cycles, u64 period)
{
	return div64_u64(cycles, period);
}

static inline unsigned int cpu_cur_freq(int cpu)
{
	return cpu_rq(cpu)->cluster->cur_freq;
}

static inline void
move_list(struct list_head *dst, struct list_head *src, bool sync_rcu)
{
	struct list_head *first, *last;

	first = src->next;
	last = src->prev;

	if (sync_rcu) {
		INIT_LIST_HEAD_RCU(src);
		synchronize_rcu();
	}

	first->prev = dst;
	dst->prev = last;
	last->next = dst;

	/* Ensure list sanity before making the head visible to all CPUs. */
	smp_mb();
	dst->next = first;
}

extern void reset_task_stats(struct task_struct *p);
extern void update_cluster_topology(void);

extern struct list_head cluster_head;
#define for_each_sched_cluster(cluster) \
	list_for_each_entry_rcu(cluster, &cluster_head, list)

extern void init_clusters(void);

extern void clear_top_tasks_bitmap(unsigned long *bitmap);

extern void sched_account_irqtime(int cpu, struct task_struct *curr,
				 u64 delta, u64 wallclock);

static inline void assign_cluster_ids(struct list_head *head)
{
	struct sched_cluster *cluster;
	int pos = 0;

	list_for_each_entry(cluster, head, list) {
		cluster->id = pos;
		sched_cluster[pos++] = cluster;
	}
}

static inline int same_cluster(int src_cpu, int dst_cpu)
{
	return cpu_rq(src_cpu)->cluster == cpu_rq(dst_cpu)->cluster;
}

void walt_irq_work(struct irq_work *irq_work);

void walt_sched_init(struct rq *rq);

#else /* CONFIG_SCHED_WALT */

static inline void walt_sched_init(struct rq *rq) { }

static inline void update_task_ravg(struct task_struct *p, struct rq *rq,
				int event, u64 wallclock, u64 irqtime) { }
static inline void walt_inc_cumulative_runnable_avg(struct rq *rq,
		struct task_struct *p)
{
}

static inline unsigned int nr_eligible_big_tasks(int cpu)
{
	return 0;
}

static inline void walt_adjust_nr_big_tasks(struct rq *rq,
		int delta, bool inc)
{
}

static inline void inc_nr_big_task(struct walt_sched_stats *stats,
		struct task_struct *p)
{
}

static inline void dec_nr_big_task(struct walt_sched_stats *stats,
		struct task_struct *p)
{
}
static inline void walt_dec_cumulative_runnable_avg(struct rq *rq,
		 struct task_struct *p)
{
}

static inline void fixup_busy_time(struct task_struct *p, int new_cpu) { }
static inline void init_new_task_load(struct task_struct *p, bool idle_task)
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
			      u32 new_task_load, u32 new_pred_demand)
{
}

#endif /* CONFIG_SCHED_WALT */

#endif
