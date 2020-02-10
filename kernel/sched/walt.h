/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __WALT_H
#define __WALT_H

#ifdef CONFIG_SCHED_WALT

#include <linux/sched/sysctl.h>
#include <linux/sched/core_ctl.h>

#define MAX_NR_CLUSTERS			3

#ifdef CONFIG_HZ_300
/*
 * Tick interval becomes to 3333333 due to
 * rounding error when HZ=300.
 */
#define DEFAULT_SCHED_RAVG_WINDOW (3333333 * 6)
#else
/* Default window size (in ns) = 20ms */
#define DEFAULT_SCHED_RAVG_WINDOW 20000000
#endif

/* Max window size (in ns) = 1s */
#define MAX_SCHED_RAVG_WINDOW 1000000000
#define NR_WINDOWS_PER_SEC (NSEC_PER_SEC / DEFAULT_SCHED_RAVG_WINDOW)

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

#define NEW_TASK_ACTIVE_TIME 100000000

extern unsigned int sched_ravg_window;
extern unsigned int new_sched_ravg_window;
extern unsigned int max_possible_efficiency;
extern unsigned int min_possible_efficiency;
extern unsigned int max_possible_freq;
extern unsigned int __read_mostly sched_load_granule;
extern u64 sched_ravg_window_change_time;

extern struct mutex cluster_lock;
extern rwlock_t related_thread_group_lock;
extern __read_mostly unsigned int sched_ravg_hist_size;
extern __read_mostly unsigned int sched_freq_aggregate;
extern __read_mostly unsigned int sched_group_upmigrate;
extern __read_mostly unsigned int sched_group_downmigrate;

extern void update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime);

extern unsigned int walt_big_tasks(int cpu);

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

extern void fixup_walt_sched_stats_common(struct rq *rq, struct task_struct *p,
					  u16 updated_demand_scaled,
					  u16 updated_pred_demand_scaled);
extern void inc_rq_walt_stats(struct rq *rq, struct task_struct *p);
extern void dec_rq_walt_stats(struct rq *rq, struct task_struct *p);
extern void fixup_busy_time(struct task_struct *p, int new_cpu);
extern void init_new_task_load(struct task_struct *p);
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
	return p->ravg.active_time < NEW_TASK_ACTIVE_TIME;
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

static inline unsigned int sched_cpu_legacy_freq(int cpu)
{
	unsigned long curr_cap = arch_scale_freq_capacity(cpu);

	return (curr_cap * (u64) cpu_rq(cpu)->cluster->max_possible_freq) >>
		SCHED_CAPACITY_SHIFT;
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

	WARN_ON(pos > MAX_NR_CLUSTERS);
}

static inline int same_cluster(int src_cpu, int dst_cpu)
{
	return cpu_rq(src_cpu)->cluster == cpu_rq(dst_cpu)->cluster;
}

void sort_clusters(void);

void walt_irq_work(struct irq_work *irq_work);

void walt_sched_init_rq(struct rq *rq);

static inline void walt_update_last_enqueue(struct task_struct *p)
{
	p->last_enqueued_ts = sched_ktime_clock();
}
extern void walt_rotate_work_init(void);
extern void walt_rotation_checkpoint(int nr_big);
extern unsigned int walt_rotation_enabled;
extern void walt_fill_ta_data(struct core_ctl_notif_data *data);

extern __read_mostly bool sched_freq_aggr_en;
static inline void walt_enable_frequency_aggregation(bool enable)
{
	sched_freq_aggr_en = enable;
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
#define SCHED_PRINT(arg)        printk_deferred("%s=%llu", #arg, arg)
#define STRG(arg)               #arg

static inline void walt_task_dump(struct task_struct *p)
{
	char buff[NR_CPUS * 16];
	int i, j = 0;
	int buffsz = NR_CPUS * 16;

	SCHED_PRINT(p->pid);
	SCHED_PRINT(p->ravg.mark_start);
	SCHED_PRINT(p->ravg.demand);
	SCHED_PRINT(p->ravg.coloc_demand);
	SCHED_PRINT(sched_ravg_window);
	SCHED_PRINT(new_sched_ravg_window);

	for (i = 0 ; i < nr_cpu_ids; i++)
		j += scnprintf(buff + j, buffsz - j, "%u ",
				p->ravg.curr_window_cpu[i]);
	printk_deferred("%s=%d (%s)\n", STRG(p->ravg.curr_window),
			p->ravg.curr_window, buff);

	for (i = 0, j = 0 ; i < nr_cpu_ids; i++)
		j += scnprintf(buff + j, buffsz - j, "%u ",
				p->ravg.prev_window_cpu[i]);
	printk_deferred("%s=%d (%s)\n", STRG(p->ravg.prev_window),
			p->ravg.prev_window, buff);

	SCHED_PRINT(p->last_wake_ts);
	SCHED_PRINT(p->last_enqueued_ts);
	SCHED_PRINT(p->misfit);
	SCHED_PRINT(p->unfilter);
}

static inline void walt_rq_dump(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *tsk = cpu_curr(cpu);
	int i;

	/*
	 * Increment the task reference so that it can't be
	 * freed on a remote CPU. Since we are going to
	 * enter panic, there is no need to decrement the
	 * task reference. Decrementing the task reference
	 * can't be done in atomic context, especially with
	 * rq locks held.
	 */
	get_task_struct(tsk);
	printk_deferred("CPU:%d nr_running:%u current: %d (%s)\n",
			cpu, rq->nr_running, tsk->pid, tsk->comm);

	printk_deferred("==========================================");
	SCHED_PRINT(rq->window_start);
	SCHED_PRINT(rq->prev_window_size);
	SCHED_PRINT(rq->curr_runnable_sum);
	SCHED_PRINT(rq->prev_runnable_sum);
	SCHED_PRINT(rq->nt_curr_runnable_sum);
	SCHED_PRINT(rq->nt_prev_runnable_sum);
	SCHED_PRINT(rq->cum_window_demand_scaled);
	SCHED_PRINT(rq->task_exec_scale);
	SCHED_PRINT(rq->grp_time.curr_runnable_sum);
	SCHED_PRINT(rq->grp_time.prev_runnable_sum);
	SCHED_PRINT(rq->grp_time.nt_curr_runnable_sum);
	SCHED_PRINT(rq->grp_time.nt_prev_runnable_sum);
	for (i = 0 ; i < NUM_TRACKED_WINDOWS; i++) {
		printk_deferred("rq->load_subs[%d].window_start=%llu)\n", i,
				rq->load_subs[i].window_start);
		printk_deferred("rq->load_subs[%d].subs=%llu)\n", i,
				rq->load_subs[i].subs);
		printk_deferred("rq->load_subs[%d].new_subs=%llu)\n", i,
				rq->load_subs[i].new_subs);
	}
	if (!exiting_task(tsk))
		walt_task_dump(tsk);
	SCHED_PRINT(sched_capacity_margin_up[cpu]);
	SCHED_PRINT(sched_capacity_margin_down[cpu]);
}

static inline void walt_dump(void)
{
	int cpu;

	printk_deferred("============ WALT RQ DUMP START ==============\n");
	printk_deferred("Sched ktime_get: %llu\n", sched_ktime_clock());
	printk_deferred("Time last window changed=%lu\n",
			sched_ravg_window_change_time);
	for_each_online_cpu(cpu) {
		walt_rq_dump(cpu);
	}
	SCHED_PRINT(max_possible_capacity);
	SCHED_PRINT(min_max_possible_capacity);

	printk_deferred("============ WALT RQ DUMP END ==============\n");
}

static int in_sched_bug;
#define SCHED_BUG_ON(condition)				\
({							\
	if (unlikely(!!(condition)) && !in_sched_bug) {	\
		in_sched_bug = 1;			\
		walt_dump();				\
		BUG_ON(condition);			\
	}						\
})

#else /* CONFIG_SCHED_WALT */

static inline void walt_sched_init_rq(struct rq *rq) { }

static inline void walt_rotate_work_init(void) { }
static inline void walt_rotation_checkpoint(int nr_big) { }
static inline void walt_update_last_enqueue(struct task_struct *p) { }

static inline void update_task_ravg(struct task_struct *p, struct rq *rq,
				int event, u64 wallclock, u64 irqtime) { }
static inline void walt_inc_cumulative_runnable_avg(struct rq *rq,
		struct task_struct *p)
{
}

static inline unsigned int walt_big_tasks(int cpu)
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
#endif /* CONFIG_SCHED_WALT */

#endif
