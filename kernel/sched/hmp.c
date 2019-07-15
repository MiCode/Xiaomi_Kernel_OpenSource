/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Implementation credits: Srivatsa Vaddagiri, Steve Muckle
 * Syed Rameez Mustafa, Olav haugan, Joonwoo Park, Pavan Kumar Kondeti
 * and Vikram Mulukutla
 */

#include <linux/cpufreq.h>
#include <linux/list_sort.h>
#include <linux/syscore_ops.h>

#include "sched.h"

#include <trace/events/sched.h>

#define CSTATE_LATENCY_GRANULARITY_SHIFT (6)

const char *task_event_names[] = {"PUT_PREV_TASK", "PICK_NEXT_TASK",
				  "TASK_WAKE", "TASK_MIGRATE", "TASK_UPDATE",
				"IRQ_UPDATE"};

const char *migrate_type_names[] = {"GROUP_TO_RQ", "RQ_TO_GROUP"};

static ktime_t ktime_last;
static bool sched_ktime_suspended;

static bool use_cycle_counter;
static struct cpu_cycle_counter_cb cpu_cycle_counter_cb;

u64 sched_ktime_clock(void)
{
	if (unlikely(sched_ktime_suspended))
		return ktime_to_ns(ktime_last);
	return ktime_get_ns();
}

static void sched_resume(void)
{
	sched_ktime_suspended = false;
}

static int sched_suspend(void)
{
	ktime_last = ktime_get();
	sched_ktime_suspended = true;
	return 0;
}

static struct syscore_ops sched_syscore_ops = {
	.resume	= sched_resume,
	.suspend = sched_suspend
};

static int __init sched_init_ops(void)
{
	register_syscore_ops(&sched_syscore_ops);
	return 0;
}
late_initcall(sched_init_ops);

inline void clear_ed_task(struct task_struct *p, struct rq *rq)
{
	if (p == rq->ed_task)
		rq->ed_task = NULL;
}

inline void set_task_last_switch_out(struct task_struct *p, u64 wallclock)
{
	p->last_switch_out_ts = wallclock;
}

/*
 * Note C-state for (idle) cpus.
 *
 * @cstate = cstate index, 0 -> active state
 * @wakeup_energy = energy spent in waking up cpu
 * @wakeup_latency = latency to wakeup from cstate
 *
 */
void
sched_set_cpu_cstate(int cpu, int cstate, int wakeup_energy, int wakeup_latency)
{
	struct rq *rq = cpu_rq(cpu);

	rq->cstate = cstate; /* C1, C2 etc */
	rq->wakeup_energy = wakeup_energy;
	/* disregard small latency delta (64 us). */
	rq->wakeup_latency = ((wakeup_latency >>
			       CSTATE_LATENCY_GRANULARITY_SHIFT) <<
			      CSTATE_LATENCY_GRANULARITY_SHIFT);
}

/*
 * Note D-state for (idle) cluster.
 *
 * @dstate = dstate index, 0 -> active state
 * @wakeup_energy = energy spent in waking up cluster
 * @wakeup_latency = latency to wakeup from cluster
 *
 */
void sched_set_cluster_dstate(const cpumask_t *cluster_cpus, int dstate,
			int wakeup_energy, int wakeup_latency)
{
	struct sched_cluster *cluster =
		cpu_rq(cpumask_first(cluster_cpus))->cluster;
	cluster->dstate = dstate;
	cluster->dstate_wakeup_energy = wakeup_energy;
	cluster->dstate_wakeup_latency = wakeup_latency;
}

u32 __weak get_freq_max_load(int cpu, u32 freq)
{
	/* 100% by default */
	return 100;
}

struct freq_max_load_entry {
	/* The maximum load which has accounted governor's headroom. */
	u64 hdemand;
};

struct freq_max_load {
	struct rcu_head rcu;
	int length;
	struct freq_max_load_entry freqs[0];
};

static DEFINE_PER_CPU(struct freq_max_load *, freq_max_load);
static DEFINE_SPINLOCK(freq_max_load_lock);

struct cpu_pwr_stats __weak *get_cpu_pwr_stats(void)
{
	return NULL;
}

int sched_update_freq_max_load(const cpumask_t *cpumask)
{
	int i, cpu, ret;
	unsigned int freq;
	struct cpu_pstate_pwr *costs;
	struct cpu_pwr_stats *per_cpu_info = get_cpu_pwr_stats();
	struct freq_max_load *max_load, *old_max_load;
	struct freq_max_load_entry *entry;
	u64 max_demand_capacity, max_demand;
	unsigned long flags;
	u32 hfreq;
	int hpct;

	if (!per_cpu_info)
		return 0;

	spin_lock_irqsave(&freq_max_load_lock, flags);
	max_demand_capacity = div64_u64(max_task_load(), max_possible_capacity);
	for_each_cpu(cpu, cpumask) {
		if (!per_cpu_info[cpu].ptable) {
			ret = -EINVAL;
			goto fail;
		}

		old_max_load = rcu_dereference(per_cpu(freq_max_load, cpu));

		/*
		 * allocate len + 1 and leave the last power cost as 0 for
		 * power_cost() can stop iterating index when
		 * per_cpu_info[cpu].len > len of max_load due to race between
		 * cpu power stats update and get_cpu_pwr_stats().
		 */
		max_load = kzalloc(sizeof(struct freq_max_load) +
				   sizeof(struct freq_max_load_entry) *
				   (per_cpu_info[cpu].len + 1), GFP_ATOMIC);
		if (unlikely(!max_load)) {
			ret = -ENOMEM;
			goto fail;
		}

		max_load->length = per_cpu_info[cpu].len;

		max_demand = max_demand_capacity *
			     cpu_max_possible_capacity(cpu);

		i = 0;
		costs = per_cpu_info[cpu].ptable;
		while (costs[i].freq) {
			entry = &max_load->freqs[i];
			freq = costs[i].freq;
			hpct = get_freq_max_load(cpu, freq);
			if (hpct <= 0 || hpct > 100)
				hpct = 100;
			hfreq = div64_u64((u64)freq * hpct, 100);
			entry->hdemand =
			    div64_u64(max_demand * hfreq,
				      cpu_max_possible_freq(cpu));
			i++;
		}

		rcu_assign_pointer(per_cpu(freq_max_load, cpu), max_load);
		if (old_max_load)
			kfree_rcu(old_max_load, rcu);
	}

	spin_unlock_irqrestore(&freq_max_load_lock, flags);
	return 0;

fail:
	for_each_cpu(cpu, cpumask) {
		max_load = rcu_dereference(per_cpu(freq_max_load, cpu));
		if (max_load) {
			rcu_assign_pointer(per_cpu(freq_max_load, cpu), NULL);
			kfree_rcu(max_load, rcu);
		}
	}

	spin_unlock_irqrestore(&freq_max_load_lock, flags);
	return ret;
}

unsigned int max_possible_efficiency = 1;
unsigned int min_possible_efficiency = UINT_MAX;

unsigned long __weak arch_get_cpu_efficiency(int cpu)
{
	return SCHED_LOAD_SCALE;
}

/* Keep track of max/min capacity possible across CPUs "currently" */
static void __update_min_max_capacity(void)
{
	int i;
	int max_cap = 0, min_cap = INT_MAX;

	for_each_online_cpu(i) {
		max_cap = max(max_cap, cpu_capacity(i));
		min_cap = min(min_cap, cpu_capacity(i));
	}

	max_capacity = max_cap;
	min_capacity = min_cap;
}

static void update_min_max_capacity(void)
{
	unsigned long flags;
	int i;

	local_irq_save(flags);
	for_each_possible_cpu(i)
		raw_spin_lock(&cpu_rq(i)->lock);

	__update_min_max_capacity();

	for_each_possible_cpu(i)
		raw_spin_unlock(&cpu_rq(i)->lock);
	local_irq_restore(flags);
}

/*
 * Return 'capacity' of a cpu in reference to "least" efficient cpu, such that
 * least efficient cpu gets capacity of 1024
 */
static unsigned long
capacity_scale_cpu_efficiency(struct sched_cluster *cluster)
{
	return (1024 * cluster->efficiency) / min_possible_efficiency;
}

/*
 * Return 'capacity' of a cpu in reference to cpu with lowest max_freq
 * (min_max_freq), such that one with lowest max_freq gets capacity of 1024.
 */
static unsigned long capacity_scale_cpu_freq(struct sched_cluster *cluster)
{
	return (1024 * cluster_max_freq(cluster)) / min_max_freq;
}

/*
 * Return load_scale_factor of a cpu in reference to "most" efficient cpu, so
 * that "most" efficient cpu gets a load_scale_factor of 1
 */
static inline unsigned long
load_scale_cpu_efficiency(struct sched_cluster *cluster)
{
	return DIV_ROUND_UP(1024 * max_possible_efficiency,
			    cluster->efficiency);
}

/*
 * Return load_scale_factor of a cpu in reference to cpu with best max_freq
 * (max_possible_freq), so that one with best max_freq gets a load_scale_factor
 * of 1.
 */
static inline unsigned long load_scale_cpu_freq(struct sched_cluster *cluster)
{
	return DIV_ROUND_UP(1024 * max_possible_freq,
			   cluster_max_freq(cluster));
}

static int compute_capacity(struct sched_cluster *cluster)
{
	int capacity = 1024;

	capacity *= capacity_scale_cpu_efficiency(cluster);
	capacity >>= 10;

	capacity *= capacity_scale_cpu_freq(cluster);
	capacity >>= 10;

	return capacity;
}

static int compute_max_possible_capacity(struct sched_cluster *cluster)
{
	int capacity = 1024;

	capacity *= capacity_scale_cpu_efficiency(cluster);
	capacity >>= 10;

	capacity *= (1024 * cluster->max_possible_freq) / min_max_freq;
	capacity >>= 10;

	return capacity;
}

static int compute_load_scale_factor(struct sched_cluster *cluster)
{
	int load_scale = 1024;

	/*
	 * load_scale_factor accounts for the fact that task load
	 * is in reference to "best" performing cpu. Task's load will need to be
	 * scaled (up) by a factor to determine suitability to be placed on a
	 * (little) cpu.
	 */
	load_scale *= load_scale_cpu_efficiency(cluster);
	load_scale >>= 10;

	load_scale *= load_scale_cpu_freq(cluster);
	load_scale >>= 10;

	return load_scale;
}

struct list_head cluster_head;
static DEFINE_MUTEX(cluster_lock);
static cpumask_t all_cluster_cpus = CPU_MASK_NONE;
DECLARE_BITMAP(all_cluster_ids, NR_CPUS);
struct sched_cluster *sched_cluster[NR_CPUS];
int num_clusters;

unsigned int max_power_cost = 1;

struct sched_cluster init_cluster = {
	.list			=	LIST_HEAD_INIT(init_cluster.list),
	.id			=	0,
	.max_power_cost		=	1,
	.min_power_cost		=	1,
	.capacity		=	1024,
	.max_possible_capacity	=	1024,
	.efficiency		=	1,
	.load_scale_factor	=	1024,
	.cur_freq		=	1,
	.max_freq		=	1,
	.max_mitigated_freq	=	UINT_MAX,
	.min_freq		=	1,
	.max_possible_freq	=	1,
	.dstate			=	0,
	.dstate_wakeup_energy	=	0,
	.dstate_wakeup_latency	=	0,
	.exec_scale_factor	=	1024,
	.notifier_sent		=	0,
	.wake_up_idle		=	0,
};

static void update_all_clusters_stats(void)
{
	struct sched_cluster *cluster;
	u64 highest_mpc = 0, lowest_mpc = U64_MAX;

	pre_big_task_count_change(cpu_possible_mask);

	for_each_sched_cluster(cluster) {
		u64 mpc;

		cluster->capacity = compute_capacity(cluster);
		mpc = cluster->max_possible_capacity =
			compute_max_possible_capacity(cluster);
		cluster->load_scale_factor = compute_load_scale_factor(cluster);

		cluster->exec_scale_factor =
			DIV_ROUND_UP(cluster->efficiency * 1024,
				     max_possible_efficiency);

		if (mpc > highest_mpc)
			highest_mpc = mpc;

		if (mpc < lowest_mpc)
			lowest_mpc = mpc;
	}

	max_possible_capacity = highest_mpc;
	min_max_possible_capacity = lowest_mpc;

	__update_min_max_capacity();
	sched_update_freq_max_load(cpu_possible_mask);
	post_big_task_count_change(cpu_possible_mask);
}

static void assign_cluster_ids(struct list_head *head)
{
	struct sched_cluster *cluster;
	int pos = 0;

	list_for_each_entry(cluster, head, list) {
		cluster->id = pos;
		sched_cluster[pos++] = cluster;
	}
}

static void
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

static int
compare_clusters(void *priv, struct list_head *a, struct list_head *b)
{
	struct sched_cluster *cluster1, *cluster2;
	int ret;

	cluster1 = container_of(a, struct sched_cluster, list);
	cluster2 = container_of(b, struct sched_cluster, list);

	/*
	 * Don't assume higher capacity means higher power. If the
	 * power cost is same, sort the higher capacity cluster before
	 * the lower capacity cluster to start placing the tasks
	 * on the higher capacity cluster.
	 */
	ret = cluster1->max_power_cost > cluster2->max_power_cost ||
		(cluster1->max_power_cost == cluster2->max_power_cost &&
		cluster1->max_possible_capacity <
				cluster2->max_possible_capacity);

	return ret;
}

static void sort_clusters(void)
{
	struct sched_cluster *cluster;
	struct list_head new_head;
	unsigned int tmp_max = 1;

	INIT_LIST_HEAD(&new_head);

	for_each_sched_cluster(cluster) {
		cluster->max_power_cost = power_cost(cluster_first_cpu(cluster),
							       max_task_load());
		cluster->min_power_cost = power_cost(cluster_first_cpu(cluster),
							       0);

		if (cluster->max_power_cost > tmp_max)
			tmp_max = cluster->max_power_cost;
	}
	max_power_cost = tmp_max;

	move_list(&new_head, &cluster_head, true);

	list_sort(NULL, &new_head, compare_clusters);
	assign_cluster_ids(&new_head);

	/*
	 * Ensure cluster ids are visible to all CPUs before making
	 * cluster_head visible.
	 */
	move_list(&cluster_head, &new_head, false);
}

static void
insert_cluster(struct sched_cluster *cluster, struct list_head *head)
{
	struct sched_cluster *tmp;
	struct list_head *iter = head;

	list_for_each_entry(tmp, head, list) {
		if (cluster->max_power_cost < tmp->max_power_cost)
			break;
		iter = &tmp->list;
	}

	list_add(&cluster->list, iter);
}

static struct sched_cluster *alloc_new_cluster(const struct cpumask *cpus)
{
	struct sched_cluster *cluster = NULL;

	cluster = kzalloc(sizeof(struct sched_cluster), GFP_ATOMIC);
	if (!cluster) {
		__WARN_printf("Cluster allocation failed. \
				Possible bad scheduling\n");
		return NULL;
	}

	INIT_LIST_HEAD(&cluster->list);
	cluster->max_power_cost		=	1;
	cluster->min_power_cost		=	1;
	cluster->capacity		=	1024;
	cluster->max_possible_capacity	=	1024;
	cluster->efficiency		=	1;
	cluster->load_scale_factor	=	1024;
	cluster->cur_freq		=	1;
	cluster->max_freq		=	1;
	cluster->max_mitigated_freq	=	UINT_MAX;
	cluster->min_freq		=	1;
	cluster->max_possible_freq	=	1;
	cluster->dstate			=	0;
	cluster->dstate_wakeup_energy	=	0;
	cluster->dstate_wakeup_latency	=	0;
	cluster->freq_init_done		=	false;

	raw_spin_lock_init(&cluster->load_lock);
	cluster->cpus = *cpus;
	cluster->efficiency = arch_get_cpu_efficiency(cpumask_first(cpus));

	if (cluster->efficiency > max_possible_efficiency)
		max_possible_efficiency = cluster->efficiency;
	if (cluster->efficiency < min_possible_efficiency)
		min_possible_efficiency = cluster->efficiency;

	cluster->notifier_sent = 0;
	return cluster;
}

static void add_cluster(const struct cpumask *cpus, struct list_head *head)
{
	struct sched_cluster *cluster = alloc_new_cluster(cpus);
	int i;

	if (!cluster)
		return;

	for_each_cpu(i, cpus)
		cpu_rq(i)->cluster = cluster;

	insert_cluster(cluster, head);
	set_bit(num_clusters, all_cluster_ids);
	num_clusters++;
}

void update_cluster_topology(void)
{
	struct cpumask cpus = *cpu_possible_mask;
	const struct cpumask *cluster_cpus;
	struct list_head new_head;
	int i;

	INIT_LIST_HEAD(&new_head);

	for_each_cpu(i, &cpus) {
		cluster_cpus = cpu_coregroup_mask(i);
		cpumask_or(&all_cluster_cpus, &all_cluster_cpus, cluster_cpus);
		cpumask_andnot(&cpus, &cpus, cluster_cpus);
		add_cluster(cluster_cpus, &new_head);
	}

	assign_cluster_ids(&new_head);

	/*
	 * Ensure cluster ids are visible to all CPUs before making
	 * cluster_head visible.
	 */
	move_list(&cluster_head, &new_head, false);
	update_all_clusters_stats();
}

void init_clusters(void)
{
	bitmap_clear(all_cluster_ids, 0, NR_CPUS);
	init_cluster.cpus = *cpu_possible_mask;
	raw_spin_lock_init(&init_cluster.load_lock);
	INIT_LIST_HEAD(&cluster_head);
}

int register_cpu_cycle_counter_cb(struct cpu_cycle_counter_cb *cb)
{
	mutex_lock(&cluster_lock);
	if (!cb->get_cpu_cycle_counter) {
		mutex_unlock(&cluster_lock);
		return -EINVAL;
	}

	cpu_cycle_counter_cb = *cb;
	use_cycle_counter = true;
	mutex_unlock(&cluster_lock);

	return 0;
}

/* Clear any HMP scheduler related requests pending from or on cpu */
void clear_hmp_request(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;

	clear_boost_kick(cpu);
	clear_reserved(cpu);
	if (rq->push_task) {
		struct task_struct *push_task = NULL;

		raw_spin_lock_irqsave(&rq->lock, flags);
		if (rq->push_task) {
			clear_reserved(rq->push_cpu);
			push_task = rq->push_task;
			rq->push_task = NULL;
		}
		rq->active_balance = 0;
		raw_spin_unlock_irqrestore(&rq->lock, flags);
		if (push_task)
			put_task_struct(push_task);
	}
}

int sched_set_static_cpu_pwr_cost(int cpu, unsigned int cost)
{
	struct rq *rq = cpu_rq(cpu);

	rq->static_cpu_pwr_cost = cost;
	return 0;
}

unsigned int sched_get_static_cpu_pwr_cost(int cpu)
{
	return cpu_rq(cpu)->static_cpu_pwr_cost;
}

int sched_set_static_cluster_pwr_cost(int cpu, unsigned int cost)
{
	struct sched_cluster *cluster = cpu_rq(cpu)->cluster;

	cluster->static_cluster_pwr_cost = cost;
	return 0;
}

unsigned int sched_get_static_cluster_pwr_cost(int cpu)
{
	return cpu_rq(cpu)->cluster->static_cluster_pwr_cost;
}

int sched_set_cluster_wake_idle(int cpu, unsigned int wake_idle)
{
	struct sched_cluster *cluster = cpu_rq(cpu)->cluster;

	cluster->wake_up_idle = !!wake_idle;
	return 0;
}

unsigned int sched_get_cluster_wake_idle(int cpu)
{
	return cpu_rq(cpu)->cluster->wake_up_idle;
}

/*
 * sched_window_stats_policy and sched_ravg_hist_size have a 'sysctl' copy
 * associated with them. This is required for atomic update of those variables
 * when being modifed via sysctl interface.
 *
 * IMPORTANT: Initialize both copies to same value!!
 */

/*
 * Tasks that are runnable continuously for a period greather than
 * EARLY_DETECTION_DURATION can be flagged early as potential
 * high load tasks.
 */
#define EARLY_DETECTION_DURATION 9500000

static __read_mostly unsigned int sched_ravg_hist_size = 5;
__read_mostly unsigned int sysctl_sched_ravg_hist_size = 5;

static __read_mostly unsigned int sched_window_stats_policy =
	 WINDOW_STATS_MAX_RECENT_AVG;
__read_mostly unsigned int sysctl_sched_window_stats_policy =
	WINDOW_STATS_MAX_RECENT_AVG;

#define SCHED_ACCOUNT_WAIT_TIME 1

__read_mostly unsigned int sysctl_sched_cpu_high_irqload = (10 * NSEC_PER_MSEC);

/*
 * Enable colocation and frequency aggregation for all threads in a process.
 * The children inherits the group id from the parent.
 */
unsigned int __read_mostly sysctl_sched_enable_thread_grouping;


#define SCHED_NEW_TASK_WINDOWS 5

#define SCHED_FREQ_ACCOUNT_WAIT_TIME 0

/*
 * This governs what load needs to be used when reporting CPU busy time
 * to the cpufreq governor.
 */
__read_mostly unsigned int sysctl_sched_freq_reporting_policy;

/*
 * For increase, send notification if
 *      freq_required - cur_freq > sysctl_sched_freq_inc_notify
 */
__read_mostly int sysctl_sched_freq_inc_notify = 10 * 1024 * 1024; /* + 10GHz */

/*
 * For decrease, send notification if
 *      cur_freq - freq_required > sysctl_sched_freq_dec_notify
 */
__read_mostly int sysctl_sched_freq_dec_notify = 10 * 1024 * 1024; /* - 10GHz */

static __read_mostly unsigned int sched_io_is_busy;

__read_mostly unsigned int sysctl_sched_pred_alert_freq = 10 * 1024 * 1024;

/*
 * Maximum possible frequency across all cpus. Task demand and cpu
 * capacity (cpu_power) metrics are scaled in reference to it.
 */
unsigned int max_possible_freq = 1;

/*
 * Minimum possible max_freq across all cpus. This will be same as
 * max_possible_freq on homogeneous systems and could be different from
 * max_possible_freq on heterogenous systems. min_max_freq is used to derive
 * capacity (cpu_power) of cpus.
 */
unsigned int min_max_freq = 1;

unsigned int max_capacity = 1024; /* max(rq->capacity) */
unsigned int min_capacity = 1024; /* min(rq->capacity) */
unsigned int max_possible_capacity = 1024; /* max(rq->max_possible_capacity) */
unsigned int
min_max_possible_capacity = 1024; /* min(rq->max_possible_capacity) */

/* Min window size (in ns) = 10ms */
#define MIN_SCHED_RAVG_WINDOW 10000000

/* Max window size (in ns) = 1s */
#define MAX_SCHED_RAVG_WINDOW 1000000000

/* Window size (in ns) */
__read_mostly unsigned int sched_ravg_window = MIN_SCHED_RAVG_WINDOW;

/* Maximum allowed threshold before freq aggregation must be enabled */
#define MAX_FREQ_AGGR_THRESH 1000

/* Temporarily disable window-stats activity on all cpus */
unsigned int __read_mostly sched_disable_window_stats;

struct related_thread_group *related_thread_groups[MAX_NUM_CGROUP_COLOC_ID];
static LIST_HEAD(active_related_thread_groups);
static DEFINE_RWLOCK(related_thread_group_lock);

#define for_each_related_thread_group(grp) \
	list_for_each_entry(grp, &active_related_thread_groups, list)

/*
 * Task load is categorized into buckets for the purpose of top task tracking.
 * The entire range of load from 0 to sched_ravg_window needs to be covered
 * in NUM_LOAD_INDICES number of buckets. Therefore the size of each bucket
 * is given by sched_ravg_window / NUM_LOAD_INDICES. Since the default value
 * of sched_ravg_window is MIN_SCHED_RAVG_WINDOW, use that to compute
 * sched_load_granule.
 */
__read_mostly unsigned int sched_load_granule =
			MIN_SCHED_RAVG_WINDOW / NUM_LOAD_INDICES;

/* Size of bitmaps maintained to track top tasks */
static const unsigned int top_tasks_bitmap_size =
		BITS_TO_LONGS(NUM_LOAD_INDICES + 1) * sizeof(unsigned long);

/*
 * Demand aggregation for frequency purpose:
 *
 * 'sched_freq_aggregate' controls aggregation of cpu demand of related threads
 * for frequency determination purpose. This aggregation is done per-cluster.
 *
 * CPU demand of tasks from various related groups is aggregated per-cluster and
 * added to the "max_busy_cpu" in that cluster, where max_busy_cpu is determined
 * by just rq->prev_runnable_sum.
 *
 * Some examples follow, which assume:
 *	Cluster0 = CPU0-3, Cluster1 = CPU4-7
 *	One related thread group A that has tasks A0, A1, A2
 *
 *	A->cpu_time[X].curr/prev_sum = counters in which cpu execution stats of
 *	tasks belonging to group A are accumulated when they run on cpu X.
 *
 *	CX->curr/prev_sum = counters in which cpu execution stats of all tasks
 *	not belonging to group A are accumulated when they run on cpu X
 *
 * Lets say the stats for window M was as below:
 *
 *	C0->prev_sum = 1ms, A->cpu_time[0].prev_sum = 5ms
 *		Task A0 ran 5ms on CPU0
 *		Task B0 ran 1ms on CPU0
 *
 *	C1->prev_sum = 5ms, A->cpu_time[1].prev_sum = 6ms
 *		Task A1 ran 4ms on CPU1
 *		Task A2 ran 2ms on CPU1
 *		Task B1 ran 5ms on CPU1
 *
 *	C2->prev_sum = 0ms, A->cpu_time[2].prev_sum = 0
 *		CPU2 idle
 *
 *	C3->prev_sum = 0ms, A->cpu_time[3].prev_sum = 0
 *		CPU3 idle
 *
 * In this case, CPU1 was most busy going by just its prev_sum counter. Demand
 * from all group A tasks are added to CPU1. IOW, at end of window M, cpu busy
 * time reported to governor will be:
 *
 *
 *	C0 busy time = 1ms
 *	C1 busy time = 5 + 5 + 6 = 16ms
 *
 */
static __read_mostly unsigned int sched_freq_aggregate = 1;
__read_mostly unsigned int sysctl_sched_freq_aggregate = 1;

unsigned int __read_mostly sysctl_sched_freq_aggregate_threshold_pct;
static unsigned int __read_mostly sched_freq_aggregate_threshold;

/* Initial task load. Newly created tasks are assigned this load. */
unsigned int __read_mostly sched_init_task_load_windows;
unsigned int __read_mostly sysctl_sched_init_task_load_pct = 15;

unsigned int max_task_load(void)
{
	return sched_ravg_window;
}

/* A cpu can no longer accommodate more tasks if:
 *
 *	rq->nr_running > sysctl_sched_spill_nr_run ||
 *	rq->hmp_stats.cumulative_runnable_avg > sched_spill_load
 */
unsigned int __read_mostly sysctl_sched_spill_nr_run = 10;

/*
 * Place sync wakee tasks those have less than configured demand to the waker's
 * cluster.
 */
unsigned int __read_mostly sched_small_wakee_task_load;
unsigned int __read_mostly sysctl_sched_small_wakee_task_load_pct = 10;

unsigned int __read_mostly sched_big_waker_task_load;
unsigned int __read_mostly sysctl_sched_big_waker_task_load_pct = 25;

/*
 * CPUs with load greater than the sched_spill_load_threshold are not
 * eligible for task placement. When all CPUs in a cluster achieve a
 * load higher than this level, tasks becomes eligible for inter
 * cluster migration.
 */
unsigned int __read_mostly sched_spill_load;
unsigned int __read_mostly sysctl_sched_spill_load_pct = 100;

/*
 * Prefer the waker CPU for sync wakee task, if the CPU has only 1 runnable
 * task. This eliminates the LPM exit latency associated with the idle
 * CPUs in the waker cluster.
 */
unsigned int __read_mostly sysctl_sched_prefer_sync_wakee_to_waker;

/*
 * Tasks whose bandwidth consumption on a cpu is more than
 * sched_upmigrate are considered "big" tasks. Big tasks will be
 * considered for "up" migration, i.e migrating to a cpu with better
 * capacity.
 */
unsigned int __read_mostly sched_upmigrate;
unsigned int __read_mostly sysctl_sched_upmigrate_pct = 80;

/*
 * Big tasks, once migrated, will need to drop their bandwidth
 * consumption to less than sched_downmigrate before they are "down"
 * migrated.
 */
unsigned int __read_mostly sched_downmigrate;
unsigned int __read_mostly sysctl_sched_downmigrate_pct = 60;

/*
 * Task groups whose aggregate demand on a cpu is more than
 * sched_group_upmigrate need to be up-migrated if possible.
 */
unsigned int __read_mostly sched_group_upmigrate;
unsigned int __read_mostly sysctl_sched_group_upmigrate_pct = 100;

/*
 * Task groups, once up-migrated, will need to drop their aggregate
 * demand to less than sched_group_downmigrate before they are "down"
 * migrated.
 */
unsigned int __read_mostly sched_group_downmigrate;
unsigned int __read_mostly sysctl_sched_group_downmigrate_pct = 95;

/*
 * The load scale factor of a CPU gets boosted when its max frequency
 * is restricted due to which the tasks are migrating to higher capacity
 * CPUs early. The sched_upmigrate threshold is auto-upgraded by
 * rq->max_possible_freq/rq->max_freq of a lower capacity CPU.
 */
unsigned int up_down_migrate_scale_factor = 1024;

/*
 * Scheduler selects and places task to its previous CPU if sleep time is
 * less than sysctl_sched_select_prev_cpu_us.
 */
unsigned int __read_mostly
sched_short_sleep_task_threshold = 2000 * NSEC_PER_USEC;

unsigned int __read_mostly sysctl_sched_select_prev_cpu_us = 2000;

unsigned int __read_mostly
sched_long_cpu_selection_threshold = 100 * NSEC_PER_MSEC;

unsigned int __read_mostly sysctl_sched_restrict_cluster_spill;

/*
 * Scheduler tries to avoid waking up idle CPUs for tasks running
 * in short bursts. If the task average burst is less than
 * sysctl_sched_short_burst nanoseconds and it sleeps on an average
 * for more than sysctl_sched_short_sleep nanoseconds, then the
 * task is eligible for packing.
 */
unsigned int __read_mostly sysctl_sched_short_burst;
unsigned int __read_mostly sysctl_sched_short_sleep = 1 * NSEC_PER_MSEC;

static void _update_up_down_migrate(unsigned int *up_migrate,
			unsigned int *down_migrate, bool is_group)
{
	unsigned int delta;

	if (up_down_migrate_scale_factor == 1024)
		return;

	delta = *up_migrate - *down_migrate;

	*up_migrate /= NSEC_PER_USEC;
	*up_migrate *= up_down_migrate_scale_factor;
	*up_migrate >>= 10;
	*up_migrate *= NSEC_PER_USEC;

	if (!is_group)
		*up_migrate = min(*up_migrate, sched_ravg_window);

	*down_migrate /= NSEC_PER_USEC;
	*down_migrate *= up_down_migrate_scale_factor;
	*down_migrate >>= 10;
	*down_migrate *= NSEC_PER_USEC;

	*down_migrate = min(*down_migrate, *up_migrate - delta);
}

static void update_up_down_migrate(void)
{
	unsigned int up_migrate = pct_to_real(sysctl_sched_upmigrate_pct);
	unsigned int down_migrate = pct_to_real(sysctl_sched_downmigrate_pct);

	_update_up_down_migrate(&up_migrate, &down_migrate, false);
	sched_upmigrate = up_migrate;
	sched_downmigrate = down_migrate;

	up_migrate = pct_to_real(sysctl_sched_group_upmigrate_pct);
	down_migrate = pct_to_real(sysctl_sched_group_downmigrate_pct);

	_update_up_down_migrate(&up_migrate, &down_migrate, true);
	sched_group_upmigrate = up_migrate;
	sched_group_downmigrate = down_migrate;
}

void set_hmp_defaults(void)
{
	sched_spill_load =
		pct_to_real(sysctl_sched_spill_load_pct);

	update_up_down_migrate();

	sched_init_task_load_windows =
		div64_u64((u64)sysctl_sched_init_task_load_pct *
			  (u64)sched_ravg_window, 100);

	sched_short_sleep_task_threshold = sysctl_sched_select_prev_cpu_us *
					   NSEC_PER_USEC;

	sched_small_wakee_task_load =
		div64_u64((u64)sysctl_sched_small_wakee_task_load_pct *
			  (u64)sched_ravg_window, 100);

	sched_big_waker_task_load =
		div64_u64((u64)sysctl_sched_big_waker_task_load_pct *
			  (u64)sched_ravg_window, 100);

	sched_freq_aggregate_threshold =
		pct_to_real(sysctl_sched_freq_aggregate_threshold_pct);
}

u32 sched_get_init_task_load(struct task_struct *p)
{
	return p->init_load_pct;
}

int sched_set_init_task_load(struct task_struct *p, int init_load_pct)
{
	if (init_load_pct < 0 || init_load_pct > 100)
		return -EINVAL;

	p->init_load_pct = init_load_pct;

	return 0;
}

#ifdef CONFIG_CGROUP_SCHED

int upmigrate_discouraged(struct task_struct *p)
{
	return task_group(p)->upmigrate_discouraged;
}

#else

static inline int upmigrate_discouraged(struct task_struct *p)
{
	return 0;
}

#endif

/* Is a task "big" on its current cpu */
static inline int __is_big_task(struct task_struct *p, u64 scaled_load)
{
	int nice = task_nice(p);

	if (nice > SCHED_UPMIGRATE_MIN_NICE || upmigrate_discouraged(p))
		return 0;

	return scaled_load > sched_upmigrate;
}

int is_big_task(struct task_struct *p)
{
	return __is_big_task(p, scale_load_to_cpu(task_load(p), task_cpu(p)));
}

u64 cpu_load(int cpu)
{
	struct rq *rq = cpu_rq(cpu);

	return scale_load_to_cpu(rq->hmp_stats.cumulative_runnable_avg, cpu);
}

u64 cpu_load_sync(int cpu, int sync)
{
	return scale_load_to_cpu(cpu_cravg_sync(cpu, sync), cpu);
}

/*
 * Task will fit on a cpu if it's bandwidth consumption on that cpu
 * will be less than sched_upmigrate. A big task that was previously
 * "up" migrated will be considered fitting on "little" cpu if its
 * bandwidth consumption on "little" cpu will be less than
 * sched_downmigrate. This will help avoid frequenty migrations for
 * tasks with load close to the upmigrate threshold
 */
int task_load_will_fit(struct task_struct *p, u64 task_load, int cpu,
			      enum sched_boost_policy boost_policy)
{
	int upmigrate = sched_upmigrate;

	if (cpu_capacity(cpu) == max_capacity)
		return 1;

	if (cpu_capacity(task_cpu(p)) > cpu_capacity(cpu))
		upmigrate = sched_downmigrate;

	if (boost_policy != SCHED_BOOST_ON_BIG) {
		if (task_nice(p) > SCHED_UPMIGRATE_MIN_NICE ||
		    upmigrate_discouraged(p))
			return 1;

		if (task_load < upmigrate)
			return 1;
	} else {
		if (task_sched_boost(p) || task_load >= upmigrate)
			return 0;

		return 1;
	}

	return 0;
}

int task_will_fit(struct task_struct *p, int cpu)
{
	u64 tload = scale_load_to_cpu(task_load(p), cpu);

	return task_load_will_fit(p, tload, cpu, sched_boost_policy());
}

static int
group_will_fit(struct sched_cluster *cluster, struct related_thread_group *grp,
						u64 demand, bool group_boost)
{
	int cpu = cluster_first_cpu(cluster);
	int prev_capacity = 0;
	unsigned int threshold = sched_group_upmigrate;
	u64 load;

	if (cluster->capacity == max_capacity)
		return 1;

	if (group_boost)
		return 0;

	if (!demand)
		return 1;

	if (grp->preferred_cluster)
		prev_capacity = grp->preferred_cluster->capacity;

	if (cluster->capacity < prev_capacity)
		threshold = sched_group_downmigrate;

	load = scale_load_to_cpu(demand, cpu);
	if (load < threshold)
		return 1;

	return 0;
}

/*
 * Return the cost of running task p on CPU cpu. This function
 * currently assumes that task p is the only task which will run on
 * the CPU.
 */
unsigned int power_cost(int cpu, u64 demand)
{
	int first, mid, last;
	struct cpu_pwr_stats *per_cpu_info = get_cpu_pwr_stats();
	struct cpu_pstate_pwr *costs;
	struct freq_max_load *max_load;
	int total_static_pwr_cost = 0;
	struct rq *rq = cpu_rq(cpu);
	unsigned int pc;

	if (!per_cpu_info || !per_cpu_info[cpu].ptable)
		/*
		 * When power aware scheduling is not in use, or CPU
		 * power data is not available, just use the CPU
		 * capacity as a rough stand-in for real CPU power
		 * numbers, assuming bigger CPUs are more power
		 * hungry.
		 */
		return cpu_max_possible_capacity(cpu);

	rcu_read_lock();
	max_load = rcu_dereference(per_cpu(freq_max_load, cpu));
	if (!max_load) {
		pc = cpu_max_possible_capacity(cpu);
		goto unlock;
	}

	costs = per_cpu_info[cpu].ptable;

	if (demand <= max_load->freqs[0].hdemand) {
		pc = costs[0].power;
		goto unlock;
	} else if (demand > max_load->freqs[max_load->length - 1].hdemand) {
		pc = costs[max_load->length - 1].power;
		goto unlock;
	}

	first = 0;
	last = max_load->length - 1;
	mid = (last - first) >> 1;
	while (1) {
		if (demand <= max_load->freqs[mid].hdemand)
			last = mid;
		else
			first = mid;

		if (last - first == 1)
			break;
		mid = first + ((last - first) >> 1);
	}

	pc = costs[last].power;

unlock:
	rcu_read_unlock();

	if (idle_cpu(cpu) && rq->cstate) {
		total_static_pwr_cost += rq->static_cpu_pwr_cost;
		if (rq->cluster->dstate)
			total_static_pwr_cost +=
				rq->cluster->static_cluster_pwr_cost;
	}

	return pc + total_static_pwr_cost;

}

void inc_nr_big_task(struct hmp_sched_stats *stats, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	if (is_big_task(p))
		stats->nr_big_tasks++;
}

void dec_nr_big_task(struct hmp_sched_stats *stats, struct task_struct *p)
{
	if (sched_disable_window_stats)
		return;

	if (is_big_task(p))
		stats->nr_big_tasks--;

	BUG_ON(stats->nr_big_tasks < 0);
}

void inc_rq_hmp_stats(struct rq *rq, struct task_struct *p, int change_cra)
{
	inc_nr_big_task(&rq->hmp_stats, p);
	if (change_cra)
		inc_cumulative_runnable_avg(&rq->hmp_stats, p);
}

void dec_rq_hmp_stats(struct rq *rq, struct task_struct *p, int change_cra)
{
	dec_nr_big_task(&rq->hmp_stats, p);
	if (change_cra)
		dec_cumulative_runnable_avg(&rq->hmp_stats, p);
}

void reset_hmp_stats(struct hmp_sched_stats *stats, int reset_cra)
{
	stats->nr_big_tasks = 0;
	if (reset_cra) {
		stats->cumulative_runnable_avg = 0;
		stats->pred_demands_sum = 0;
	}
}

int preferred_cluster(struct sched_cluster *cluster, struct task_struct *p)
{
	struct related_thread_group *grp;
	int rc = 1;

	rcu_read_lock();

	grp = task_related_thread_group(p);
	if (grp)
		rc = (grp->preferred_cluster == cluster);

	rcu_read_unlock();
	return rc;
}

struct sched_cluster *rq_cluster(struct rq *rq)
{
	return rq->cluster;
}

/*
 * reset_cpu_hmp_stats - reset HMP stats for a cpu
 *	nr_big_tasks
 *	cumulative_runnable_avg (iff reset_cra is true)
 */
void reset_cpu_hmp_stats(int cpu, int reset_cra)
{
	reset_cfs_rq_hmp_stats(cpu, reset_cra);
	reset_hmp_stats(&cpu_rq(cpu)->hmp_stats, reset_cra);
}

void fixup_nr_big_tasks(struct hmp_sched_stats *stats,
				struct task_struct *p, s64 delta)
{
	u64 new_task_load;
	u64 old_task_load;

	if (sched_disable_window_stats)
		return;

	old_task_load = scale_load_to_cpu(task_load(p), task_cpu(p));
	new_task_load = scale_load_to_cpu(delta + task_load(p), task_cpu(p));

	if (__is_big_task(p, old_task_load) && !__is_big_task(p, new_task_load))
		stats->nr_big_tasks--;
	else if (!__is_big_task(p, old_task_load) &&
		 __is_big_task(p, new_task_load))
		stats->nr_big_tasks++;

	BUG_ON(stats->nr_big_tasks < 0);
}

/*
 * Walk runqueue of cpu and re-initialize 'nr_big_tasks' counters.
 */
static void update_nr_big_tasks(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	struct task_struct *p;

	/* Do not reset cumulative_runnable_avg */
	reset_cpu_hmp_stats(cpu, 0);

	list_for_each_entry(p, &rq->cfs_tasks, se.group_node)
		_inc_hmp_sched_stats_fair(rq, p, 0);
}

/* Disable interrupts and grab runqueue lock of all cpus listed in @cpus */
void pre_big_task_count_change(const struct cpumask *cpus)
{
	int i;

	local_irq_disable();

	for_each_cpu(i, cpus)
		raw_spin_lock(&cpu_rq(i)->lock);
}

/*
 * Reinitialize 'nr_big_tasks' counters on all affected cpus
 */
void post_big_task_count_change(const struct cpumask *cpus)
{
	int i;

	/* Assumes local_irq_disable() keeps online cpumap stable */
	for_each_cpu(i, cpus)
		update_nr_big_tasks(i);

	for_each_cpu(i, cpus)
		raw_spin_unlock(&cpu_rq(i)->lock);

	local_irq_enable();
}

DEFINE_MUTEX(policy_mutex);

unsigned int update_freq_aggregate_threshold(unsigned int threshold)
{
	unsigned int old_threshold;

	mutex_lock(&policy_mutex);

	old_threshold = sysctl_sched_freq_aggregate_threshold_pct;

	sysctl_sched_freq_aggregate_threshold_pct = threshold;
	sched_freq_aggregate_threshold =
		pct_to_real(sysctl_sched_freq_aggregate_threshold_pct);

	mutex_unlock(&policy_mutex);

	return old_threshold;
}

static inline int invalid_value_freq_input(unsigned int *data)
{
	if (data == &sysctl_sched_freq_aggregate)
		return !(*data == 0 || *data == 1);

	return 0;
}

static inline int invalid_value(unsigned int *data)
{
	unsigned int val = *data;

	if (data == &sysctl_sched_ravg_hist_size)
		return (val < 2 || val > RAVG_HIST_SIZE_MAX);

	if (data == &sysctl_sched_window_stats_policy)
		return val >= WINDOW_STATS_INVALID_POLICY;

	return invalid_value_freq_input(data);
}

/*
 * Handle "atomic" update of sysctl_sched_window_stats_policy,
 * sysctl_sched_ravg_hist_size variables.
 */
int sched_window_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret;
	unsigned int *data = (unsigned int *)table->data;
	unsigned int old_val;

	mutex_lock(&policy_mutex);

	old_val = *data;

	ret = proc_dointvec(table, write, buffer, lenp, ppos);
	if (ret || !write || (write && (old_val == *data)))
		goto done;

	if (invalid_value(data)) {
		*data = old_val;
		ret = -EINVAL;
		goto done;
	}

	reset_all_window_stats(0, 0);

done:
	mutex_unlock(&policy_mutex);

	return ret;
}

/*
 * Convert percentage value into absolute form. This will avoid div() operation
 * in fast path, to convert task load in percentage scale.
 */
int sched_hmp_proc_update_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp,
		loff_t *ppos)
{
	int ret;
	unsigned int old_val;
	unsigned int *data = (unsigned int *)table->data;
	int update_task_count = 0;

	/*
	 * The policy mutex is acquired with cpu_hotplug.lock
	 * held from cpu_up()->cpufreq_governor_interactive()->
	 * sched_set_window(). So enforce the same order here.
	 */
	if (write && (data == &sysctl_sched_upmigrate_pct)) {
		update_task_count = 1;
		get_online_cpus();
	}

	mutex_lock(&policy_mutex);

	old_val = *data;

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		goto done;

	if (write && (old_val == *data))
		goto done;

	if (sysctl_sched_downmigrate_pct > sysctl_sched_upmigrate_pct ||
				sysctl_sched_group_downmigrate_pct >
				sysctl_sched_group_upmigrate_pct) {
		*data = old_val;
		ret = -EINVAL;
		goto done;
	}

	/*
	 * Big task tunable change will need to re-classify tasks on
	 * runqueue as big and set their counters appropriately.
	 * sysctl interface affects secondary variables (*_pct), which is then
	 * "atomically" carried over to the primary variables. Atomic change
	 * includes taking runqueue lock of all online cpus and re-initiatizing
	 * their big counter values based on changed criteria.
	 */
	if (update_task_count)
		pre_big_task_count_change(cpu_online_mask);

	set_hmp_defaults();

	if (update_task_count)
		post_big_task_count_change(cpu_online_mask);

done:
	mutex_unlock(&policy_mutex);
	if (update_task_count)
		put_online_cpus();
	return ret;
}

inline int nr_big_tasks(struct rq *rq)
{
	return rq->hmp_stats.nr_big_tasks;
}

unsigned int cpu_temp(int cpu)
{
	struct cpu_pwr_stats *per_cpu_info = get_cpu_pwr_stats();

	if (per_cpu_info)
		return per_cpu_info[cpu].temp;
	else
		return 0;
}

/*
 * kfree() may wakeup kswapd. So this function should NOT be called
 * with any CPU's rq->lock acquired.
 */
void free_task_load_ptrs(struct task_struct *p)
{
	kfree(p->ravg.curr_window_cpu);
	kfree(p->ravg.prev_window_cpu);

	/*
	 * update_task_ravg() can be called for exiting tasks. While the
	 * function itself ensures correct behavior, the corresponding
	 * trace event requires that these pointers be NULL.
	 */
	p->ravg.curr_window_cpu = NULL;
	p->ravg.prev_window_cpu = NULL;
}

void init_new_task_load(struct task_struct *p)
{
	int i;
	u32 init_load_windows = sched_init_task_load_windows;
	u32 init_load_pct = current->init_load_pct;

	p->init_load_pct = 0;
	rcu_assign_pointer(p->grp, NULL);
	INIT_LIST_HEAD(&p->grp_list);
	memset(&p->ravg, 0, sizeof(struct ravg));
	p->cpu_cycles = 0;
	p->ravg.curr_burst = 0;
	/*
	 * Initialize the avg_burst to twice the threshold, so that
	 * a task would not be classified as short burst right away
	 * after fork. It takes at least 6 sleep-wakeup cycles for
	 * the avg_burst to go below the threshold.
	 */
	p->ravg.avg_burst = 2 * (u64)sysctl_sched_short_burst;
	p->ravg.avg_sleep_time = 0;

	p->ravg.curr_window_cpu = kcalloc(nr_cpu_ids, sizeof(u32), GFP_KERNEL);
	p->ravg.prev_window_cpu = kcalloc(nr_cpu_ids, sizeof(u32), GFP_KERNEL);

	/* Don't have much choice. CPU frequency would be bogus */
	BUG_ON(!p->ravg.curr_window_cpu || !p->ravg.prev_window_cpu);

	if (init_load_pct)
		init_load_windows = div64_u64((u64)init_load_pct *
			  (u64)sched_ravg_window, 100);

	p->ravg.demand = init_load_windows;
	p->ravg.pred_demand = 0;
	for (i = 0; i < RAVG_HIST_SIZE_MAX; ++i)
		p->ravg.sum_history[i] = init_load_windows;
}

/* Return task demand in percentage scale */
unsigned int pct_task_load(struct task_struct *p)
{
	unsigned int load;

	load = div64_u64((u64)task_load(p) * 100, (u64)max_task_load());

	return load;
}

/*
 * Return total number of tasks "eligible" to run on highest capacity cpu
 *
 * This is simply nr_big_tasks for cpus which are not of max_capacity and
 * nr_running for cpus of max_capacity
 */
unsigned int nr_eligible_big_tasks(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	int nr_big = rq->hmp_stats.nr_big_tasks;
	int nr = rq->nr_running;

	if (!is_max_capacity_cpu(cpu))
		return nr_big;

	return nr;
}

static inline int exiting_task(struct task_struct *p)
{
	return (p->ravg.sum_history[0] == EXITING_TASK_MARKER);
}

static int __init set_sched_ravg_window(char *str)
{
	unsigned int window_size;

	get_option(&str, &window_size);

	if (window_size < MIN_SCHED_RAVG_WINDOW ||
			window_size > MAX_SCHED_RAVG_WINDOW) {
		WARN_ON(1);
		return -EINVAL;
	}

	sched_ravg_window = window_size;
	return 0;
}

early_param("sched_ravg_window", set_sched_ravg_window);

static inline void
update_window_start(struct rq *rq, u64 wallclock)
{
	s64 delta;
	int nr_windows;

	delta = wallclock - rq->window_start;
	BUG_ON(delta < 0);
	if (delta < sched_ravg_window)
		return;

	nr_windows = div64_u64(delta, sched_ravg_window);
	rq->window_start += (u64)nr_windows * (u64)sched_ravg_window;
}

#define DIV64_U64_ROUNDUP(X, Y) div64_u64((X) + (Y - 1), Y)

static inline u64 scale_exec_time(u64 delta, struct rq *rq)
{
	u32 freq;

	freq = cpu_cycles_to_freq(rq->cc.cycles, rq->cc.time);
	delta = DIV64_U64_ROUNDUP(delta * freq, max_possible_freq);
	delta *= rq->cluster->exec_scale_factor;
	delta >>= 10;

	return delta;
}

static inline int cpu_is_waiting_on_io(struct rq *rq)
{
	if (!sched_io_is_busy)
		return 0;

	return atomic_read(&rq->nr_iowait);
}

/* Does freq_required sufficiently exceed or fall behind cur_freq? */
static inline int
nearly_same_freq(unsigned int cur_freq, unsigned int freq_required)
{
	int delta = freq_required - cur_freq;

	if (freq_required > cur_freq)
		return delta < sysctl_sched_freq_inc_notify;

	delta = -delta;

	return delta < sysctl_sched_freq_dec_notify;
}

/* Convert busy time to frequency equivalent */
static inline unsigned int load_to_freq(struct rq *rq, u64 load)
{
	unsigned int freq;

	load = scale_load_to_cpu(load, cpu_of(rq));
	load *= 128;
	load = div64_u64(load, max_task_load());

	freq = load * cpu_max_possible_freq(cpu_of(rq));
	freq /= 128;

	return freq;
}

/*
 * Return load from all related groups in given frequency domain.
 */
static void group_load_in_freq_domain(struct cpumask *cpus,
				u64 *grp_load, u64 *new_grp_load)
{
	int j;

	for_each_cpu(j, cpus) {
		struct rq *rq = cpu_rq(j);

		*grp_load += rq->grp_time.prev_runnable_sum;
		*new_grp_load += rq->grp_time.nt_prev_runnable_sum;
	}
}

static inline u64 freq_policy_load(struct rq *rq, u64 load);
/*
 * Should scheduler alert governor for changing frequency?
 *
 * @check_pred - evaluate frequency based on the predictive demand
 * @check_groups - add load from all related groups on given cpu
 *
 * check_groups is set to 1 if a "related" task movement/wakeup is triggering
 * the notification check. To avoid "re-aggregation" of demand in such cases,
 * we check whether the migrated/woken tasks demand (along with demand from
 * existing tasks on the cpu) can be met on target cpu
 *
 */

static int send_notification(struct rq *rq, int check_pred, int check_groups)
{
	unsigned int cur_freq, freq_required;
	unsigned long flags;
	int rc = 0;
	u64 group_load = 0, new_load  = 0;

	if (check_pred) {
		u64 prev = rq->old_busy_time;
		u64 predicted = rq->hmp_stats.pred_demands_sum;

		if (rq->cluster->cur_freq == cpu_max_freq(cpu_of(rq)))
			return 0;

		prev = max(prev, rq->old_estimated_time);
		if (prev > predicted)
			return 0;

		cur_freq = load_to_freq(rq, prev);
		freq_required = load_to_freq(rq, predicted);

		if (freq_required < cur_freq + sysctl_sched_pred_alert_freq)
			return 0;
	} else {
		/*
		 * Protect from concurrent update of rq->prev_runnable_sum and
		 * group cpu load
		 */
		raw_spin_lock_irqsave(&rq->lock, flags);
		if (check_groups)
			group_load = rq->grp_time.prev_runnable_sum;

		new_load = rq->prev_runnable_sum + group_load;
		new_load = freq_policy_load(rq, new_load);

		raw_spin_unlock_irqrestore(&rq->lock, flags);

		cur_freq = load_to_freq(rq, rq->old_busy_time);
		freq_required = load_to_freq(rq, new_load);

		if (nearly_same_freq(cur_freq, freq_required))
			return 0;
	}

	raw_spin_lock_irqsave(&rq->lock, flags);
	if (!rq->cluster->notifier_sent) {
		rq->cluster->notifier_sent = 1;
		rc = 1;
		trace_sched_freq_alert(cpu_of(rq), check_pred, check_groups, rq,
				       new_load);
	}
	raw_spin_unlock_irqrestore(&rq->lock, flags);

	return rc;
}

/* Alert governor if there is a need to change frequency */
void check_for_freq_change(struct rq *rq, bool check_pred, bool check_groups)
{
	int cpu = cpu_of(rq);

	if (!send_notification(rq, check_pred, check_groups))
		return;

	atomic_notifier_call_chain(
		&load_alert_notifier_head, 0,
		(void *)(long)cpu);
}

void notify_migration(int src_cpu, int dest_cpu, bool src_cpu_dead,
			     struct task_struct *p)
{
	bool check_groups;

	rcu_read_lock();
	check_groups = task_in_related_thread_group(p);
	rcu_read_unlock();

	if (!same_freq_domain(src_cpu, dest_cpu)) {
		if (!src_cpu_dead)
			check_for_freq_change(cpu_rq(src_cpu), false,
					      check_groups);
		check_for_freq_change(cpu_rq(dest_cpu), false, check_groups);
	} else {
		check_for_freq_change(cpu_rq(dest_cpu), true, check_groups);
	}
}

static int account_busy_for_cpu_time(struct rq *rq, struct task_struct *p,
				     u64 irqtime, int event)
{
	if (is_idle_task(p)) {
		/* TASK_WAKE && TASK_MIGRATE is not possible on idle task! */
		if (event == PICK_NEXT_TASK)
			return 0;

		/* PUT_PREV_TASK, TASK_UPDATE && IRQ_UPDATE are left */
		return irqtime || cpu_is_waiting_on_io(rq);
	}

	if (event == TASK_WAKE)
		return 0;

	if (event == PUT_PREV_TASK || event == IRQ_UPDATE)
		return 1;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_FREQ_ACCOUNT_WAIT_TIME : 0;
	}

	/* TASK_MIGRATE, PICK_NEXT_TASK left */
	return SCHED_FREQ_ACCOUNT_WAIT_TIME;
}

static inline bool is_new_task(struct task_struct *p)
{
	return p->ravg.active_windows < SCHED_NEW_TASK_WINDOWS;
}

#define INC_STEP 8
#define DEC_STEP 2
#define CONSISTENT_THRES 16
#define INC_STEP_BIG 16
/*
 * bucket_increase - update the count of all buckets
 *
 * @buckets: array of buckets tracking busy time of a task
 * @idx: the index of bucket to be incremented
 *
 * Each time a complete window finishes, count of bucket that runtime
 * falls in (@idx) is incremented. Counts of all other buckets are
 * decayed. The rate of increase and decay could be different based
 * on current count in the bucket.
 */
static inline void bucket_increase(u8 *buckets, int idx)
{
	int i, step;

	for (i = 0; i < NUM_BUSY_BUCKETS; i++) {
		if (idx != i) {
			if (buckets[i] > DEC_STEP)
				buckets[i] -= DEC_STEP;
			else
				buckets[i] = 0;
		} else {
			step = buckets[i] >= CONSISTENT_THRES ?
						INC_STEP_BIG : INC_STEP;
			if (buckets[i] > U8_MAX - step)
				buckets[i] = U8_MAX;
			else
				buckets[i] += step;
		}
	}
}

static inline int busy_to_bucket(u32 normalized_rt)
{
	int bidx;

	bidx = mult_frac(normalized_rt, NUM_BUSY_BUCKETS, max_task_load());
	bidx = min(bidx, NUM_BUSY_BUCKETS - 1);

	/*
	 * Combine lowest two buckets. The lowest frequency falls into
	 * 2nd bucket and thus keep predicting lowest bucket is not
	 * useful.
	 */
	if (!bidx)
		bidx++;

	return bidx;
}

static inline u64
scale_load_to_freq(u64 load, unsigned int src_freq, unsigned int dst_freq)
{
	return div64_u64(load * (u64)src_freq, (u64)dst_freq);
}

/*
 * get_pred_busy - calculate predicted demand for a task on runqueue
 *
 * @rq: runqueue of task p
 * @p: task whose prediction is being updated
 * @start: starting bucket. returned prediction should not be lower than
 *         this bucket.
 * @runtime: runtime of the task. returned prediction should not be lower
 *           than this runtime.
 * Note: @start can be derived from @runtime. It's passed in only to
 * avoid duplicated calculation in some cases.
 *
 * A new predicted busy time is returned for task @p based on @runtime
 * passed in. The function searches through buckets that represent busy
 * time equal to or bigger than @runtime and attempts to find the bucket to
 * to use for prediction. Once found, it searches through historical busy
 * time and returns the latest that falls into the bucket. If no such busy
 * time exists, it returns the medium of that bucket.
 */
static u32 get_pred_busy(struct rq *rq, struct task_struct *p,
				int start, u32 runtime)
{
	int i;
	u8 *buckets = p->ravg.busy_buckets;
	u32 *hist = p->ravg.sum_history;
	u32 dmin, dmax;
	u64 cur_freq_runtime = 0;
	int first = NUM_BUSY_BUCKETS, final;
	u32 ret = runtime;

	/* skip prediction for new tasks due to lack of history */
	if (unlikely(is_new_task(p)))
		goto out;

	/* find minimal bucket index to pick */
	for (i = start; i < NUM_BUSY_BUCKETS; i++) {
		if (buckets[i]) {
			first = i;
			break;
		}
	}
	/* if no higher buckets are filled, predict runtime */
	if (first >= NUM_BUSY_BUCKETS)
		goto out;

	/* compute the bucket for prediction */
	final = first;

	/* determine demand range for the predicted bucket */
	if (final < 2) {
		/* lowest two buckets are combined */
		dmin = 0;
		final = 1;
	} else {
		dmin = mult_frac(final, max_task_load(), NUM_BUSY_BUCKETS);
	}
	dmax = mult_frac(final + 1, max_task_load(), NUM_BUSY_BUCKETS);

	/*
	 * search through runtime history and return first runtime that falls
	 * into the range of predicted bucket.
	 */
	for (i = 0; i < sched_ravg_hist_size; i++) {
		if (hist[i] >= dmin && hist[i] < dmax) {
			ret = hist[i];
			break;
		}
	}
	/* no historical runtime within bucket found, use average of the bin */
	if (ret < dmin)
		ret = (dmin + dmax) / 2;
	/*
	 * when updating in middle of a window, runtime could be higher
	 * than all recorded history. Always predict at least runtime.
	 */
	ret = max(runtime, ret);
out:
	trace_sched_update_pred_demand(rq, p, runtime,
		mult_frac((unsigned int)cur_freq_runtime, 100,
			  sched_ravg_window), ret);
	return ret;
}

static inline u32 calc_pred_demand(struct rq *rq, struct task_struct *p)
{
	if (p->ravg.pred_demand >= p->ravg.curr_window)
		return p->ravg.pred_demand;

	return get_pred_busy(rq, p, busy_to_bucket(p->ravg.curr_window),
			     p->ravg.curr_window);
}

/*
 * predictive demand of a task is calculated at the window roll-over.
 * if the task current window busy time exceeds the predicted
 * demand, update it here to reflect the task needs.
 */
void update_task_pred_demand(struct rq *rq, struct task_struct *p, int event)
{
	u32 new, old;

	if (is_idle_task(p) || exiting_task(p))
		return;

	if (event != PUT_PREV_TASK && event != TASK_UPDATE &&
			(!SCHED_FREQ_ACCOUNT_WAIT_TIME ||
			 (event != TASK_MIGRATE &&
			 event != PICK_NEXT_TASK)))
		return;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (!p->on_rq && !SCHED_FREQ_ACCOUNT_WAIT_TIME)
			return;
	}

	new = calc_pred_demand(rq, p);
	old = p->ravg.pred_demand;

	if (old >= new)
		return;

	if (task_on_rq_queued(p) && (!task_has_dl_policy(p) ||
				!p->dl.dl_throttled))
		p->sched_class->fixup_hmp_sched_stats(rq, p,
				p->ravg.demand,
				new);

	p->ravg.pred_demand = new;
}

void clear_top_tasks_bitmap(unsigned long *bitmap)
{
	memset(bitmap, 0, top_tasks_bitmap_size);
	__set_bit(NUM_LOAD_INDICES, bitmap);
}

/*
 * Special case the last index and provide a fast path for index = 0.
 * Note that sched_load_granule can change underneath us if we are not
 * holding any runqueue locks while calling the two functions below.
 */
static u32  top_task_load(struct rq *rq)
{
	int index = rq->prev_top;
	u8 prev = 1 - rq->curr_table;

	if (!index) {
		int msb = NUM_LOAD_INDICES - 1;

		if (!test_bit(msb, rq->top_tasks_bitmap[prev]))
			return 0;
		else
			return sched_load_granule;
	} else if (index == NUM_LOAD_INDICES - 1) {
		return sched_ravg_window;
	} else {
		return (index + 1) * sched_load_granule;
	}
}

static u32 load_to_index(u32 load)
{
	u32 index = load / sched_load_granule;

	return min(index, (u32)(NUM_LOAD_INDICES - 1));
}

static void update_top_tasks(struct task_struct *p, struct rq *rq,
		u32 old_curr_window, int new_window, bool full_window)
{
	u8 curr = rq->curr_table;
	u8 prev = 1 - curr;
	u8 *curr_table = rq->top_tasks[curr];
	u8 *prev_table = rq->top_tasks[prev];
	int old_index, new_index, update_index;
	u32 curr_window = p->ravg.curr_window;
	u32 prev_window = p->ravg.prev_window;
	bool zero_index_update;

	if (old_curr_window == curr_window && !new_window)
		return;

	old_index = load_to_index(old_curr_window);
	new_index = load_to_index(curr_window);

	if (!new_window) {
		zero_index_update = !old_curr_window && curr_window;
		if (old_index != new_index || zero_index_update) {
			if (old_curr_window)
				curr_table[old_index] -= 1;
			if (curr_window)
				curr_table[new_index] += 1;
			if (new_index > rq->curr_top)
				rq->curr_top = new_index;
		}

		if (!curr_table[old_index])
			__clear_bit(NUM_LOAD_INDICES - old_index - 1,
				rq->top_tasks_bitmap[curr]);

		if (curr_table[new_index] == 1)
			__set_bit(NUM_LOAD_INDICES - new_index - 1,
				rq->top_tasks_bitmap[curr]);

		return;
	}

	/*
	 * The window has rolled over for this task. By the time we get
	 * here, curr/prev swaps would has already occurred. So we need
	 * to use prev_window for the new index.
	 */
	update_index = load_to_index(prev_window);

	if (full_window) {
		/*
		 * Two cases here. Either 'p' ran for the entire window or
		 * it didn't run at all. In either case there is no entry
		 * in the prev table. If 'p' ran the entire window, we just
		 * need to create a new entry in the prev table. In this case
		 * update_index will be correspond to sched_ravg_window
		 * so we can unconditionally update the top index.
		 */
		if (prev_window) {
			prev_table[update_index] += 1;
			rq->prev_top = update_index;
		}

		if (prev_table[update_index] == 1)
			__set_bit(NUM_LOAD_INDICES - update_index - 1,
				rq->top_tasks_bitmap[prev]);
	} else {
		zero_index_update = !old_curr_window && prev_window;
		if (old_index != update_index || zero_index_update) {
			if (old_curr_window)
				prev_table[old_index] -= 1;

			prev_table[update_index] += 1;

			if (update_index > rq->prev_top)
				rq->prev_top = update_index;

			if (!prev_table[old_index])
				__clear_bit(NUM_LOAD_INDICES - old_index - 1,
						rq->top_tasks_bitmap[prev]);

			if (prev_table[update_index] == 1)
				__set_bit(NUM_LOAD_INDICES - update_index - 1,
						rq->top_tasks_bitmap[prev]);
		}
	}

	if (curr_window) {
		curr_table[new_index] += 1;

		if (new_index > rq->curr_top)
			rq->curr_top = new_index;

		if (curr_table[new_index] == 1)
			__set_bit(NUM_LOAD_INDICES - new_index - 1,
				rq->top_tasks_bitmap[curr]);
	}
}

static inline void clear_top_tasks_table(u8 *table)
{
	memset(table, 0, NUM_LOAD_INDICES * sizeof(u8));
}

static void rollover_top_tasks(struct rq *rq, bool full_window)
{
	u8 curr_table = rq->curr_table;
	u8 prev_table = 1 - curr_table;
	int curr_top = rq->curr_top;

	clear_top_tasks_table(rq->top_tasks[prev_table]);
	clear_top_tasks_bitmap(rq->top_tasks_bitmap[prev_table]);

	if (full_window) {
		curr_top = 0;
		clear_top_tasks_table(rq->top_tasks[curr_table]);
		clear_top_tasks_bitmap(
				rq->top_tasks_bitmap[curr_table]);
	}

	rq->curr_table = prev_table;
	rq->prev_top = curr_top;
	rq->curr_top = 0;
}

static u32 empty_windows[NR_CPUS];

static void rollover_task_window(struct task_struct *p, bool full_window)
{
	u32 *curr_cpu_windows = empty_windows;
	u32 curr_window;
	int i;

	/* Rollover the sum */
	curr_window = 0;

	if (!full_window) {
		curr_window = p->ravg.curr_window;
		curr_cpu_windows = p->ravg.curr_window_cpu;
	}

	p->ravg.prev_window = curr_window;
	p->ravg.curr_window = 0;

	/* Roll over individual CPU contributions */
	for (i = 0; i < nr_cpu_ids; i++) {
		p->ravg.prev_window_cpu[i] = curr_cpu_windows[i];
		p->ravg.curr_window_cpu[i] = 0;
	}
}

static void rollover_cpu_window(struct rq *rq, bool full_window)
{
	u64 curr_sum = rq->curr_runnable_sum;
	u64 nt_curr_sum = rq->nt_curr_runnable_sum;
	u64 grp_curr_sum = rq->grp_time.curr_runnable_sum;
	u64 grp_nt_curr_sum = rq->grp_time.nt_curr_runnable_sum;

	if (unlikely(full_window)) {
		curr_sum = 0;
		nt_curr_sum = 0;
		grp_curr_sum = 0;
		grp_nt_curr_sum = 0;
	}

	rq->prev_runnable_sum = curr_sum;
	rq->nt_prev_runnable_sum = nt_curr_sum;
	rq->grp_time.prev_runnable_sum = grp_curr_sum;
	rq->grp_time.nt_prev_runnable_sum = grp_nt_curr_sum;

	rq->curr_runnable_sum = 0;
	rq->nt_curr_runnable_sum = 0;
	rq->grp_time.curr_runnable_sum = 0;
	rq->grp_time.nt_curr_runnable_sum = 0;
}

/*
 * Account cpu activity in its busy time counters (rq->curr/prev_runnable_sum)
 */
static void update_cpu_busy_time(struct task_struct *p, struct rq *rq,
				 int event, u64 wallclock, u64 irqtime)
{
	int new_window, full_window = 0;
	int p_is_curr_task = (p == rq->curr);
	u64 mark_start = p->ravg.mark_start;
	u64 window_start = rq->window_start;
	u32 window_size = sched_ravg_window;
	u64 delta;
	u64 *curr_runnable_sum = &rq->curr_runnable_sum;
	u64 *prev_runnable_sum = &rq->prev_runnable_sum;
	u64 *nt_curr_runnable_sum = &rq->nt_curr_runnable_sum;
	u64 *nt_prev_runnable_sum = &rq->nt_prev_runnable_sum;
	bool new_task;
	struct related_thread_group *grp;
	int cpu = rq->cpu;
	u32 old_curr_window = p->ravg.curr_window;

	new_window = mark_start < window_start;
	if (new_window) {
		full_window = (window_start - mark_start) >= window_size;
		if (p->ravg.active_windows < USHRT_MAX)
			p->ravg.active_windows++;
	}

	new_task = is_new_task(p);

	/*
	 * Handle per-task window rollover. We don't care about the idle
	 * task or exiting tasks.
	 */
	if (!is_idle_task(p) && !exiting_task(p)) {
		if (new_window)
			rollover_task_window(p, full_window);
	}

	if (p_is_curr_task && new_window) {
		rollover_cpu_window(rq, full_window);
		rollover_top_tasks(rq, full_window);
	}

	if (!account_busy_for_cpu_time(rq, p, irqtime, event))
		goto done;

	grp = p->grp;
	if (grp && sched_freq_aggregate) {
		struct group_cpu_time *cpu_time = &rq->grp_time;

		curr_runnable_sum = &cpu_time->curr_runnable_sum;
		prev_runnable_sum = &cpu_time->prev_runnable_sum;

		nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;
	}

	if (!new_window) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. No rollover
		 * since we didn't start a new window. An example of this is
		 * when a task starts execution and then sleeps within the
		 * same window.
		 */

		if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq))
			delta = wallclock - mark_start;
		else
			delta = irqtime;
		delta = scale_exec_time(delta, rq);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!is_idle_task(p) && !exiting_task(p)) {
			p->ravg.curr_window += delta;
			p->ravg.curr_window_cpu[cpu] += delta;
		}

		goto done;
	}

	if (!p_is_curr_task) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has also started, but p is not the current task, so the
		 * window is not rolled over - just split up and account
		 * as necessary into curr and prev. The window is only
		 * rolled over when a new window is processed for the current
		 * task.
		 *
		 * Irqtime can't be accounted by a task that isn't the
		 * currently running task.
		 */

		if (!full_window) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!exiting_task(p)) {
				p->ravg.prev_window += delta;
				p->ravg.prev_window_cpu[cpu] += delta;
			}
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			if (!exiting_task(p)) {
				p->ravg.prev_window = delta;
				p->ravg.prev_window_cpu[cpu] = delta;
			}
		}

		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!exiting_task(p)) {
			p->ravg.curr_window = delta;
			p->ravg.curr_window_cpu[cpu] = delta;
		}

		goto done;
	}

	if (!irqtime || !is_idle_task(p) || cpu_is_waiting_on_io(rq)) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. If any of these three above conditions are true
		 * then this busy time can't be accounted as irqtime.
		 *
		 * Busy time for the idle task or exiting tasks need not
		 * be accounted.
		 *
		 * An example of this would be a task that starts execution
		 * and then sleeps once a new window has begun.
		 */

		if (!full_window) {
			/*
			 * A full window hasn't elapsed, account partial
			 * contribution to previous completed window.
			 */
			delta = scale_exec_time(window_start - mark_start, rq);
			if (!is_idle_task(p) && !exiting_task(p)) {
				p->ravg.prev_window += delta;
				p->ravg.prev_window_cpu[cpu] += delta;
			}
		} else {
			/*
			 * Since at least one full window has elapsed,
			 * the contribution to the previous window is the
			 * full window (window_size).
			 */
			delta = scale_exec_time(window_size, rq);
			if (!is_idle_task(p) && !exiting_task(p)) {
				p->ravg.prev_window = delta;
				p->ravg.prev_window_cpu[cpu] = delta;
			}
		}

		/*
		 * Rollover is done here by overwriting the values in
		 * prev_runnable_sum and curr_runnable_sum.
		 */
		*prev_runnable_sum += delta;
		if (new_task)
			*nt_prev_runnable_sum += delta;

		/* Account piece of busy time in the current window. */
		delta = scale_exec_time(wallclock - window_start, rq);
		*curr_runnable_sum += delta;
		if (new_task)
			*nt_curr_runnable_sum += delta;

		if (!is_idle_task(p) && !exiting_task(p)) {
			p->ravg.curr_window = delta;
			p->ravg.curr_window_cpu[cpu] = delta;
		}

		goto done;
	}

	if (irqtime) {
		/*
		 * account_busy_for_cpu_time() = 1 so busy time needs
		 * to be accounted to the current window. A new window
		 * has started and p is the current task so rollover is
		 * needed. The current task must be the idle task because
		 * irqtime is not accounted for any other task.
		 *
		 * Irqtime will be accounted each time we process IRQ activity
		 * after a period of idleness, so we know the IRQ busy time
		 * started at wallclock - irqtime.
		 */

		BUG_ON(!is_idle_task(p));
		mark_start = wallclock - irqtime;

		/*
		 * Roll window over. If IRQ busy time was just in the current
		 * window then that is all that need be accounted.
		 */
		if (mark_start > window_start) {
			*curr_runnable_sum = scale_exec_time(irqtime, rq);
			return;
		}

		/*
		 * The IRQ busy time spanned multiple windows. Process the
		 * busy time preceding the current window start first.
		 */
		delta = window_start - mark_start;
		if (delta > window_size)
			delta = window_size;
		delta = scale_exec_time(delta, rq);
		*prev_runnable_sum += delta;

		/* Process the remaining IRQ busy time in the current window. */
		delta = wallclock - window_start;
		rq->curr_runnable_sum = scale_exec_time(delta, rq);

		return;
	}

done:
	if (!is_idle_task(p) && !exiting_task(p))
		update_top_tasks(p, rq, old_curr_window,
					new_window, full_window);
}

static inline u32 predict_and_update_buckets(struct rq *rq,
			struct task_struct *p, u32 runtime) {

	int bidx;
	u32 pred_demand;

	bidx = busy_to_bucket(runtime);
	pred_demand = get_pred_busy(rq, p, bidx, runtime);
	bucket_increase(p->ravg.busy_buckets, bidx);

	return pred_demand;
}

#define THRESH_CC_UPDATE (2 * NSEC_PER_USEC)

/*
 * Assumes rq_lock is held and wallclock was recorded in the same critical
 * section as this function's invocation.
 */
static inline u64 read_cycle_counter(int cpu, u64 wallclock)
{
	struct sched_cluster *cluster = cpu_rq(cpu)->cluster;
	u64 delta;

	if (unlikely(!cluster))
		return cpu_cycle_counter_cb.get_cpu_cycle_counter(cpu);

	/*
	 * Why don't we need locking here? Let's say that delta is negative
	 * because some other CPU happened to update last_cc_update with a
	 * more recent timestamp. We simply read the conter again in that case
	 * with no harmful side effects. This can happen if there is an FIQ
	 * between when we read the wallclock and when we use it here.
	 */
	delta = wallclock - atomic64_read(&cluster->last_cc_update);
	if (delta > THRESH_CC_UPDATE) {
		atomic64_set(&cluster->cycles,
			     cpu_cycle_counter_cb.get_cpu_cycle_counter(cpu));
		atomic64_set(&cluster->last_cc_update, wallclock);
	}

	return atomic64_read(&cluster->cycles);
}

static void update_task_cpu_cycles(struct task_struct *p, int cpu,
				   u64 wallclock)
{
	if (use_cycle_counter)
		p->cpu_cycles = read_cycle_counter(cpu, wallclock);
}

static void
update_task_rq_cpu_cycles(struct task_struct *p, struct rq *rq, int event,
			  u64 wallclock, u64 irqtime)
{
	u64 cur_cycles;
	int cpu = cpu_of(rq);

	lockdep_assert_held(&rq->lock);

	if (!use_cycle_counter) {
		rq->cc.cycles = cpu_cur_freq(cpu);
		rq->cc.time = 1;
		return;
	}

	cur_cycles = read_cycle_counter(cpu, wallclock);

	/*
	 * If current task is idle task and irqtime == 0 CPU was
	 * indeed idle and probably its cycle counter was not
	 * increasing.  We still need estimatied CPU frequency
	 * for IO wait time accounting.  Use the previously
	 * calculated frequency in such a case.
	 */
	if (!is_idle_task(rq->curr) || irqtime) {
		if (unlikely(cur_cycles < p->cpu_cycles))
			rq->cc.cycles = cur_cycles + (U64_MAX - p->cpu_cycles);
		else
			rq->cc.cycles = cur_cycles - p->cpu_cycles;
		rq->cc.cycles = rq->cc.cycles * NSEC_PER_MSEC;

		if (event == IRQ_UPDATE && is_idle_task(p))
			/*
			 * Time between mark_start of idle task and IRQ handler
			 * entry time is CPU cycle counter stall period.
			 * Upon IRQ handler entry sched_account_irqstart()
			 * replenishes idle task's cpu cycle counter so
			 * rq->cc.cycles now represents increased cycles during
			 * IRQ handler rather than time between idle entry and
			 * IRQ exit.  Thus use irqtime as time delta.
			 */
			rq->cc.time = irqtime;
		else
			rq->cc.time = wallclock - p->ravg.mark_start;
		BUG_ON((s64)rq->cc.time < 0);
	}

	p->cpu_cycles = cur_cycles;

	trace_sched_get_task_cpu_cycles(cpu, event, rq->cc.cycles,
					rq->cc.time, p);
}

static int
account_busy_for_task_demand(struct rq *rq, struct task_struct *p, int event)
{
	/*
	 * No need to bother updating task demand for exiting tasks
	 * or the idle task.
	 */
	if (exiting_task(p) || is_idle_task(p))
		return 0;

	/*
	 * When a task is waking up it is completing a segment of non-busy
	 * time. Likewise, if wait time is not treated as busy time, then
	 * when a task begins to run or is migrated, it is not running and
	 * is completing a segment of non-busy time.
	 */
	if (event == TASK_WAKE || (!SCHED_ACCOUNT_WAIT_TIME &&
			 (event == PICK_NEXT_TASK || event == TASK_MIGRATE)))
		return 0;

	/*
	 * TASK_UPDATE can be called on sleeping task, when its moved between
	 * related groups
	 */
	if (event == TASK_UPDATE) {
		if (rq->curr == p)
			return 1;

		return p->on_rq ? SCHED_ACCOUNT_WAIT_TIME : 0;
	}

	return 1;
}

/*
 * Called when new window is starting for a task, to record cpu usage over
 * recently concluded window(s). Normally 'samples' should be 1. It can be > 1
 * when, say, a real-time task runs without preemption for several windows at a
 * stretch.
 */
static void update_history(struct rq *rq, struct task_struct *p,
			 u32 runtime, int samples, int event)
{
	u32 *hist = &p->ravg.sum_history[0];
	int ridx, widx;
	u32 max = 0, avg, demand, pred_demand;
	u64 sum = 0;

	/* Ignore windows where task had no activity */
	if (!runtime || is_idle_task(p) || exiting_task(p) || !samples)
		goto done;

	/* Push new 'runtime' value onto stack */
	widx = sched_ravg_hist_size - 1;
	ridx = widx - samples;
	for (; ridx >= 0; --widx, --ridx) {
		hist[widx] = hist[ridx];
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	for (widx = 0; widx < samples && widx < sched_ravg_hist_size; widx++) {
		hist[widx] = runtime;
		sum += hist[widx];
		if (hist[widx] > max)
			max = hist[widx];
	}

	p->ravg.sum = 0;

	if (sched_window_stats_policy == WINDOW_STATS_RECENT) {
		demand = runtime;
	} else if (sched_window_stats_policy == WINDOW_STATS_MAX) {
		demand = max;
	} else {
		avg = div64_u64(sum, sched_ravg_hist_size);
		if (sched_window_stats_policy == WINDOW_STATS_AVG)
			demand = avg;
		else
			demand = max(avg, runtime);
	}
	pred_demand = predict_and_update_buckets(rq, p, runtime);

	/*
	 * A throttled deadline sched class task gets dequeued without
	 * changing p->on_rq. Since the dequeue decrements hmp stats
	 * avoid decrementing it here again.
	 */
	if (task_on_rq_queued(p) && (!task_has_dl_policy(p) ||
						!p->dl.dl_throttled))
		p->sched_class->fixup_hmp_sched_stats(rq, p, demand,
						      pred_demand);

	p->ravg.demand = demand;
	p->ravg.pred_demand = pred_demand;

done:
	trace_sched_update_history(rq, p, runtime, samples, event);
}

static u64 add_to_task_demand(struct rq *rq, struct task_struct *p, u64 delta)
{
	delta = scale_exec_time(delta, rq);
	p->ravg.sum += delta;
	if (unlikely(p->ravg.sum > sched_ravg_window))
		p->ravg.sum = sched_ravg_window;

	return delta;
}

/*
 * Account cpu demand of task and/or update task's cpu demand history
 *
 * ms = p->ravg.mark_start;
 * wc = wallclock
 * ws = rq->window_start
 *
 * Three possibilities:
 *
 *	a) Task event is contained within one window.
 *		window_start < mark_start < wallclock
 *
 *		ws   ms  wc
 *		|    |   |
 *		V    V   V
 *		|---------------|
 *
 *	In this case, p->ravg.sum is updated *iff* event is appropriate
 *	(ex: event == PUT_PREV_TASK)
 *
 *	b) Task event spans two windows.
 *		mark_start < window_start < wallclock
 *
 *		ms   ws   wc
 *		|    |    |
 *		V    V    V
 *		-----|-------------------
 *
 *	In this case, p->ravg.sum is updated with (ws - ms) *iff* event
 *	is appropriate, then a new window sample is recorded followed
 *	by p->ravg.sum being set to (wc - ws) *iff* event is appropriate.
 *
 *	c) Task event spans more than two windows.
 *
 *		ms ws_tmp			   ws  wc
 *		|  |				   |   |
 *		V  V				   V   V
 *		---|-------|-------|-------|-------|------
 *		   |				   |
 *		   |<------ nr_full_windows ------>|
 *
 *	In this case, p->ravg.sum is updated with (ws_tmp - ms) first *iff*
 *	event is appropriate, window sample of p->ravg.sum is recorded,
 *	'nr_full_window' samples of window_size is also recorded *iff*
 *	event is appropriate and finally p->ravg.sum is set to (wc - ws)
 *	*iff* event is appropriate.
 *
 * IMPORTANT : Leave p->ravg.mark_start unchanged, as update_cpu_busy_time()
 * depends on it!
 */
static u64 update_task_demand(struct task_struct *p, struct rq *rq,
			       int event, u64 wallclock)
{
	u64 mark_start = p->ravg.mark_start;
	u64 delta, window_start = rq->window_start;
	int new_window, nr_full_windows;
	u32 window_size = sched_ravg_window;
	u64 runtime;

	new_window = mark_start < window_start;
	if (!account_busy_for_task_demand(rq, p, event)) {
		if (new_window)
			/*
			 * If the time accounted isn't being accounted as
			 * busy time, and a new window started, only the
			 * previous window need be closed out with the
			 * pre-existing demand. Multiple windows may have
			 * elapsed, but since empty windows are dropped,
			 * it is not necessary to account those.
			 */
			update_history(rq, p, p->ravg.sum, 1, event);
		return 0;
	}

	if (!new_window) {
		/*
		 * The simple case - busy time contained within the existing
		 * window.
		 */
		return add_to_task_demand(rq, p, wallclock - mark_start);
	}

	/*
	 * Busy time spans at least two windows. Temporarily rewind
	 * window_start to first window boundary after mark_start.
	 */
	delta = window_start - mark_start;
	nr_full_windows = div64_u64(delta, window_size);
	window_start -= (u64)nr_full_windows * (u64)window_size;

	/* Process (window_start - mark_start) first */
	runtime = add_to_task_demand(rq, p, window_start - mark_start);

	/* Push new sample(s) into task's demand history */
	update_history(rq, p, p->ravg.sum, 1, event);
	if (nr_full_windows) {
		u64 scaled_window = scale_exec_time(window_size, rq);

		update_history(rq, p, scaled_window, nr_full_windows, event);
		runtime += nr_full_windows * scaled_window;
	}

	/*
	 * Roll window_start back to current to process any remainder
	 * in current window.
	 */
	window_start += (u64)nr_full_windows * (u64)window_size;

	/* Process (wallclock - window_start) next */
	mark_start = window_start;
	runtime += add_to_task_demand(rq, p, wallclock - mark_start);

	return runtime;
}

static inline void
update_task_burst(struct task_struct *p, struct rq *rq, int event, u64 runtime)
{
	/*
	 * update_task_demand() has checks for idle task and
	 * exit task. The runtime may include the wait time,
	 * so update the burst only for the cases where the
	 * task is running.
	 */
	if (event == PUT_PREV_TASK || (event == TASK_UPDATE &&
				rq->curr == p))
		p->ravg.curr_burst += runtime;
}

/* Reflect task activity on its demand and cpu's busy time statistics */
void update_task_ravg(struct task_struct *p, struct rq *rq, int event,
						u64 wallclock, u64 irqtime)
{
	u64 runtime;

	if (!rq->window_start || sched_disable_window_stats ||
	    p->ravg.mark_start == wallclock)
		return;

	lockdep_assert_held(&rq->lock);

	update_window_start(rq, wallclock);

	if (!p->ravg.mark_start) {
		update_task_cpu_cycles(p, cpu_of(rq), wallclock);
		goto done;
	}

	update_task_rq_cpu_cycles(p, rq, event, wallclock, irqtime);
	runtime = update_task_demand(p, rq, event, wallclock);
	if (runtime)
		update_task_burst(p, rq, event, runtime);
	update_cpu_busy_time(p, rq, event, wallclock, irqtime);
	update_task_pred_demand(rq, p, event);

	if (exiting_task(p))
		goto done;

	trace_sched_update_task_ravg(p, rq, event, wallclock, irqtime,
				     rq->cc.cycles, rq->cc.time,
				     p->grp ? &rq->grp_time : NULL);

done:
	p->ravg.mark_start = wallclock;
}

void sched_account_irqtime(int cpu, struct task_struct *curr,
				 u64 delta, u64 wallclock)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags, nr_windows;
	u64 cur_jiffies_ts;

	raw_spin_lock_irqsave(&rq->lock, flags);

	/*
	 * cputime (wallclock) uses sched_clock so use the same here for
	 * consistency.
	 */
	delta += sched_clock() - wallclock;
	cur_jiffies_ts = get_jiffies_64();

	if (is_idle_task(curr))
		update_task_ravg(curr, rq, IRQ_UPDATE, sched_ktime_clock(),
				 delta);

	nr_windows = cur_jiffies_ts - rq->irqload_ts;

	if (nr_windows) {
		if (nr_windows < 10) {
			/* Decay CPU's irqload by 3/4 for each window. */
			rq->avg_irqload *= (3 * nr_windows);
			rq->avg_irqload = div64_u64(rq->avg_irqload,
						    4 * nr_windows);
		} else {
			rq->avg_irqload = 0;
		}
		rq->avg_irqload += rq->cur_irqload;
		rq->cur_irqload = 0;
	}

	rq->cur_irqload += delta;
	rq->irqload_ts = cur_jiffies_ts;
	raw_spin_unlock_irqrestore(&rq->lock, flags);
}

void sched_account_irqstart(int cpu, struct task_struct *curr, u64 wallclock)
{
	struct rq *rq = cpu_rq(cpu);

	if (!rq->window_start || sched_disable_window_stats)
		return;

	if (is_idle_task(curr)) {
		/* We're here without rq->lock held, IRQ disabled */
		raw_spin_lock(&rq->lock);
		update_task_cpu_cycles(curr, cpu, sched_ktime_clock());
		raw_spin_unlock(&rq->lock);
	}
}

void reset_task_stats(struct task_struct *p)
{
	u32 sum = 0;
	u32 *curr_window_ptr = NULL;
	u32 *prev_window_ptr = NULL;

	if (exiting_task(p)) {
		sum = EXITING_TASK_MARKER;
	} else {
		curr_window_ptr =  p->ravg.curr_window_cpu;
		prev_window_ptr = p->ravg.prev_window_cpu;
		memset(curr_window_ptr, 0, sizeof(u32) * nr_cpu_ids);
		memset(prev_window_ptr, 0, sizeof(u32) * nr_cpu_ids);
	}

	memset(&p->ravg, 0, sizeof(struct ravg));

	p->ravg.curr_window_cpu = curr_window_ptr;
	p->ravg.prev_window_cpu = prev_window_ptr;

	p->ravg.avg_burst = 2 * (u64)sysctl_sched_short_burst;

	/* Retain EXITING_TASK marker */
	p->ravg.sum_history[0] = sum;
}

void mark_task_starting(struct task_struct *p)
{
	u64 wallclock;
	struct rq *rq = task_rq(p);

	if (!rq->window_start || sched_disable_window_stats) {
		reset_task_stats(p);
		return;
	}

	wallclock = sched_ktime_clock();
	p->ravg.mark_start = p->last_wake_ts = wallclock;
	p->last_cpu_selected_ts = wallclock;
	p->last_switch_out_ts = 0;
	update_task_cpu_cycles(p, cpu_of(rq), wallclock);
}

void set_window_start(struct rq *rq)
{
	static int sync_cpu_available;

	if (rq->window_start)
		return;

	if (!sync_cpu_available) {
		rq->window_start = sched_ktime_clock();
		sync_cpu_available = 1;
	} else {
		struct rq *sync_rq = cpu_rq(cpumask_any(cpu_online_mask));

		raw_spin_unlock(&rq->lock);
		double_rq_lock(rq, sync_rq);
		rq->window_start = sync_rq->window_start;
		rq->curr_runnable_sum = rq->prev_runnable_sum = 0;
		rq->nt_curr_runnable_sum = rq->nt_prev_runnable_sum = 0;
		raw_spin_unlock(&sync_rq->lock);
	}

	rq->curr->ravg.mark_start = rq->window_start;
}

static void reset_all_task_stats(void)
{
	struct task_struct *g, *p;

	do_each_thread(g, p) {
		reset_task_stats(p);
	}  while_each_thread(g, p);
}

enum reset_reason_code {
	WINDOW_CHANGE,
	POLICY_CHANGE,
	HIST_SIZE_CHANGE,
	FREQ_AGGREGATE_CHANGE,
};

const char *sched_window_reset_reasons[] = {
	"WINDOW_CHANGE",
	"POLICY_CHANGE",
	"HIST_SIZE_CHANGE",
	"FREQ_AGGREGATE_CHANGE",
};

/* Called with IRQs enabled */
void reset_all_window_stats(u64 window_start, unsigned int window_size)
{
	int cpu, i;
	unsigned long flags;
	u64 start_ts = sched_ktime_clock();
	int reason = WINDOW_CHANGE;
	unsigned int old = 0, new = 0;

	local_irq_save(flags);

	read_lock(&tasklist_lock);

	read_lock(&related_thread_group_lock);

	/* Taking all runqueue locks prevents race with sched_exit(). */
	for_each_possible_cpu(cpu)
		raw_spin_lock(&cpu_rq(cpu)->lock);

	sched_disable_window_stats = 1;

	reset_all_task_stats();

	read_unlock(&tasklist_lock);

	if (window_size) {
		sched_ravg_window = window_size * TICK_NSEC;
		set_hmp_defaults();
		sched_load_granule = sched_ravg_window / NUM_LOAD_INDICES;
	}

	sched_disable_window_stats = 0;

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);

		if (window_start)
			rq->window_start = window_start;
		rq->curr_runnable_sum = rq->prev_runnable_sum = 0;
		rq->nt_curr_runnable_sum = rq->nt_prev_runnable_sum = 0;
		memset(&rq->grp_time, 0, sizeof(struct group_cpu_time));
		for (i = 0; i < NUM_TRACKED_WINDOWS; i++) {
			memset(&rq->load_subs[i], 0,
					sizeof(struct load_subtractions));
			clear_top_tasks_table(rq->top_tasks[i]);
			clear_top_tasks_bitmap(rq->top_tasks_bitmap[i]);
		}

		rq->curr_table = 0;
		rq->curr_top = 0;
		rq->prev_top = 0;
		reset_cpu_hmp_stats(cpu, 1);
	}

	if (sched_window_stats_policy != sysctl_sched_window_stats_policy) {
		reason = POLICY_CHANGE;
		old = sched_window_stats_policy;
		new = sysctl_sched_window_stats_policy;
		sched_window_stats_policy = sysctl_sched_window_stats_policy;
	} else if (sched_ravg_hist_size != sysctl_sched_ravg_hist_size) {
		reason = HIST_SIZE_CHANGE;
		old = sched_ravg_hist_size;
		new = sysctl_sched_ravg_hist_size;
		sched_ravg_hist_size = sysctl_sched_ravg_hist_size;
	} else if (sched_freq_aggregate !=
					sysctl_sched_freq_aggregate) {
		reason = FREQ_AGGREGATE_CHANGE;
		old = sched_freq_aggregate;
		new = sysctl_sched_freq_aggregate;
		sched_freq_aggregate = sysctl_sched_freq_aggregate;
	}

	for_each_possible_cpu(cpu)
		raw_spin_unlock(&cpu_rq(cpu)->lock);

	read_unlock(&related_thread_group_lock);

	local_irq_restore(flags);

	trace_sched_reset_all_window_stats(window_start, window_size,
		sched_ktime_clock() - start_ts, reason, old, new);
}

/*
 * In this function we match the accumulated subtractions with the current
 * and previous windows we are operating with. Ignore any entries where
 * the window start in the load_subtraction struct does not match either
 * the curent or the previous window. This could happen whenever CPUs
 * become idle or busy with interrupts disabled for an extended period.
 */
static inline void account_load_subtractions(struct rq *rq)
{
	u64 ws = rq->window_start;
	u64 prev_ws = ws - sched_ravg_window;
	struct load_subtractions *ls = rq->load_subs;
	int i;

	for (i = 0; i < NUM_TRACKED_WINDOWS; i++) {
		if (ls[i].window_start == ws) {
			rq->curr_runnable_sum -= ls[i].subs;
			rq->nt_curr_runnable_sum -= ls[i].new_subs;
		} else if (ls[i].window_start == prev_ws) {
			rq->prev_runnable_sum -= ls[i].subs;
			rq->nt_prev_runnable_sum -= ls[i].new_subs;
		}

		ls[i].subs = 0;
		ls[i].new_subs = 0;
	}

	BUG_ON((s64)rq->prev_runnable_sum < 0);
	BUG_ON((s64)rq->curr_runnable_sum < 0);
	BUG_ON((s64)rq->nt_prev_runnable_sum < 0);
	BUG_ON((s64)rq->nt_curr_runnable_sum < 0);
}

static inline u64 freq_policy_load(struct rq *rq, u64 load)
{
	unsigned int reporting_policy = sysctl_sched_freq_reporting_policy;

	switch (reporting_policy) {
	case FREQ_REPORT_MAX_CPU_LOAD_TOP_TASK:
		load = max_t(u64, load, top_task_load(rq));
		break;
	case FREQ_REPORT_TOP_TASK:
		load = top_task_load(rq);
		break;
	case FREQ_REPORT_CPU_LOAD:
		break;
	default:
		break;
	}

	return load;
}

void sched_get_cpus_busy(struct sched_load *busy,
			 const struct cpumask *query_cpus)
{
	unsigned long flags;
	struct rq *rq;
	const int cpus = cpumask_weight(query_cpus);
	u64 load[cpus], group_load[cpus];
	u64 nload[cpus], ngload[cpus];
	u64 pload[cpus];
	unsigned int max_freq[cpus];
	int notifier_sent = 0;
	int early_detection[cpus];
	int cpu, i = 0;
	unsigned int window_size;
	u64 max_prev_sum = 0;
	int max_busy_cpu = cpumask_first(query_cpus);
	u64 total_group_load = 0, total_ngload = 0;
	bool aggregate_load = false;
	struct sched_cluster *cluster = cpu_cluster(cpumask_first(query_cpus));

	if (unlikely(cpus == 0))
		return;

	local_irq_save(flags);

	/*
	 * This function could be called in timer context, and the
	 * current task may have been executing for a long time. Ensure
	 * that the window stats are current by doing an update.
	 */

	for_each_cpu(cpu, query_cpus)
		raw_spin_lock(&cpu_rq(cpu)->lock);

	window_size = sched_ravg_window;

	/*
	 * We don't really need the cluster lock for this entire for loop
	 * block. However, there is no advantage in optimizing this as rq
	 * locks are held regardless and would prevent migration anyways
	 */
	raw_spin_lock(&cluster->load_lock);

	for_each_cpu(cpu, query_cpus) {
		rq = cpu_rq(cpu);

		update_task_ravg(rq->curr, rq, TASK_UPDATE, sched_ktime_clock(),
				 0);

		account_load_subtractions(rq);
		load[i] = rq->prev_runnable_sum;
		nload[i] = rq->nt_prev_runnable_sum;
		pload[i] = rq->hmp_stats.pred_demands_sum;
		rq->old_estimated_time = pload[i];

		if (load[i] > max_prev_sum) {
			max_prev_sum = load[i];
			max_busy_cpu = cpu;
		}

		/*
		 * sched_get_cpus_busy() is called for all CPUs in a
		 * frequency domain. So the notifier_sent flag per
		 * cluster works even when a frequency domain spans
		 * more than 1 cluster.
		 */
		if (rq->cluster->notifier_sent) {
			notifier_sent = 1;
			rq->cluster->notifier_sent = 0;
		}
		early_detection[i] = (rq->ed_task != NULL);
		max_freq[i] = cpu_max_freq(cpu);
		i++;
	}

	raw_spin_unlock(&cluster->load_lock);

	group_load_in_freq_domain(
			&cpu_rq(max_busy_cpu)->freq_domain_cpumask,
			&total_group_load, &total_ngload);
	aggregate_load = !!(total_group_load > sched_freq_aggregate_threshold);

	i = 0;
	for_each_cpu(cpu, query_cpus) {
		group_load[i] = 0;
		ngload[i] = 0;

		if (early_detection[i])
			goto skip_early;

		rq = cpu_rq(cpu);
		if (aggregate_load) {
			if (cpu == max_busy_cpu) {
				group_load[i] = total_group_load;
				ngload[i] = total_ngload;
			}
		} else {
			group_load[i] = rq->grp_time.prev_runnable_sum;
			ngload[i] = rq->grp_time.nt_prev_runnable_sum;
		}

		load[i] += group_load[i];
		nload[i] += ngload[i];

		load[i] = freq_policy_load(rq, load[i]);
		rq->old_busy_time = load[i];

		/*
		 * Scale load in reference to cluster max_possible_freq.
		 *
		 * Note that scale_load_to_cpu() scales load in reference to
		 * the cluster max_freq.
		 */
		load[i] = scale_load_to_cpu(load[i], cpu);
		nload[i] = scale_load_to_cpu(nload[i], cpu);
		pload[i] = scale_load_to_cpu(pload[i], cpu);
skip_early:
		i++;
	}

	for_each_cpu(cpu, query_cpus)
		raw_spin_unlock(&(cpu_rq(cpu))->lock);

	local_irq_restore(flags);

	i = 0;
	for_each_cpu(cpu, query_cpus) {
		rq = cpu_rq(cpu);

		if (early_detection[i]) {
			busy[i].prev_load = div64_u64(sched_ravg_window,
							NSEC_PER_USEC);
			busy[i].new_task_load = 0;
			busy[i].predicted_load = 0;
			goto exit_early;
		}

		load[i] = scale_load_to_freq(load[i], max_freq[i],
				cpu_max_possible_freq(cpu));
		nload[i] = scale_load_to_freq(nload[i], max_freq[i],
				cpu_max_possible_freq(cpu));

		pload[i] = scale_load_to_freq(pload[i], max_freq[i],
					     rq->cluster->max_possible_freq);

		busy[i].prev_load = div64_u64(load[i], NSEC_PER_USEC);
		busy[i].new_task_load = div64_u64(nload[i], NSEC_PER_USEC);
		busy[i].predicted_load = div64_u64(pload[i], NSEC_PER_USEC);

exit_early:
		trace_sched_get_busy(cpu, busy[i].prev_load,
				     busy[i].new_task_load,
				     busy[i].predicted_load,
				     early_detection[i],
				     aggregate_load &&
				      cpu == max_busy_cpu);
		i++;
	}
}

void sched_set_io_is_busy(int val)
{
	sched_io_is_busy = val;
}

int sched_set_window(u64 window_start, unsigned int window_size)
{
	u64 now, cur_jiffies, jiffy_ktime_ns;
	s64 ws;
	unsigned long flags;

	if (window_size * TICK_NSEC <  MIN_SCHED_RAVG_WINDOW)
		return -EINVAL;

	mutex_lock(&policy_mutex);

	/*
	 * Get a consistent view of ktime, jiffies, and the time
	 * since the last jiffy (based on last_jiffies_update).
	 */
	local_irq_save(flags);
	cur_jiffies = jiffy_to_ktime_ns(&now, &jiffy_ktime_ns);
	local_irq_restore(flags);

	/* translate window_start from jiffies to nanoseconds */
	ws = (window_start - cur_jiffies); /* jiffy difference */
	ws *= TICK_NSEC;
	ws += jiffy_ktime_ns;

	/*
	 * Roll back calculated window start so that it is in
	 * the past (window stats must have a current window).
	 */
	while (ws > now)
		ws -= (window_size * TICK_NSEC);

	BUG_ON(sched_ktime_clock() < ws);

	reset_all_window_stats(ws, window_size);

	sched_update_freq_max_load(cpu_possible_mask);

	mutex_unlock(&policy_mutex);

	return 0;
}

static inline void create_subtraction_entry(struct rq *rq, u64 ws, int index)
{
	rq->load_subs[index].window_start = ws;
	rq->load_subs[index].subs = 0;
	rq->load_subs[index].new_subs = 0;
}

static bool get_subtraction_index(struct rq *rq, u64 ws)
{
	int i;
	u64 oldest = ULLONG_MAX;
	int oldest_index = 0;

	for (i = 0; i < NUM_TRACKED_WINDOWS; i++) {
		u64 entry_ws = rq->load_subs[i].window_start;

		if (ws == entry_ws)
			return i;

		if (entry_ws < oldest) {
			oldest = entry_ws;
			oldest_index = i;
		}
	}

	create_subtraction_entry(rq, ws, oldest_index);
	return oldest_index;
}

static void update_rq_load_subtractions(int index, struct rq *rq,
					u32 sub_load, bool new_task)
{
	rq->load_subs[index].subs +=  sub_load;
	if (new_task)
		rq->load_subs[index].new_subs += sub_load;
}

static void update_cluster_load_subtractions(struct task_struct *p,
					int cpu, u64 ws, bool new_task)
{
	struct sched_cluster *cluster = cpu_cluster(cpu);
	struct cpumask cluster_cpus = cluster->cpus;
	u64 prev_ws = ws - sched_ravg_window;
	int i;

	cpumask_clear_cpu(cpu, &cluster_cpus);
	raw_spin_lock(&cluster->load_lock);

	for_each_cpu(i, &cluster_cpus) {
		struct rq *rq = cpu_rq(i);
		int index;

		if (p->ravg.curr_window_cpu[i]) {
			index = get_subtraction_index(rq, ws);
			update_rq_load_subtractions(index, rq,
				p->ravg.curr_window_cpu[i], new_task);
			p->ravg.curr_window_cpu[i] = 0;
		}

		if (p->ravg.prev_window_cpu[i]) {
			index = get_subtraction_index(rq, prev_ws);
			update_rq_load_subtractions(index, rq,
				p->ravg.prev_window_cpu[i], new_task);
			p->ravg.prev_window_cpu[i] = 0;
		}
	}

	raw_spin_unlock(&cluster->load_lock);
}

static inline void inter_cluster_migration_fixup
	(struct task_struct *p, int new_cpu, int task_cpu, bool new_task)
{
	struct rq *dest_rq = cpu_rq(new_cpu);
	struct rq *src_rq = cpu_rq(task_cpu);

	if (same_freq_domain(new_cpu, task_cpu))
		return;

	p->ravg.curr_window_cpu[new_cpu] = p->ravg.curr_window;
	p->ravg.prev_window_cpu[new_cpu] = p->ravg.prev_window;

	dest_rq->curr_runnable_sum += p->ravg.curr_window;
	dest_rq->prev_runnable_sum += p->ravg.prev_window;

	src_rq->curr_runnable_sum -=  p->ravg.curr_window_cpu[task_cpu];
	src_rq->prev_runnable_sum -=  p->ravg.prev_window_cpu[task_cpu];

	if (new_task) {
		dest_rq->nt_curr_runnable_sum += p->ravg.curr_window;
		dest_rq->nt_prev_runnable_sum += p->ravg.prev_window;

		src_rq->nt_curr_runnable_sum -=
				p->ravg.curr_window_cpu[task_cpu];
		src_rq->nt_prev_runnable_sum -=
				p->ravg.prev_window_cpu[task_cpu];
	}

	p->ravg.curr_window_cpu[task_cpu] = 0;
	p->ravg.prev_window_cpu[task_cpu] = 0;

	update_cluster_load_subtractions(p, task_cpu,
			src_rq->window_start, new_task);

	BUG_ON((s64)src_rq->prev_runnable_sum < 0);
	BUG_ON((s64)src_rq->curr_runnable_sum < 0);
	BUG_ON((s64)src_rq->nt_prev_runnable_sum < 0);
	BUG_ON((s64)src_rq->nt_curr_runnable_sum < 0);
}

static int get_top_index(unsigned long *bitmap, unsigned long old_top)
{
	int index = find_next_bit(bitmap, NUM_LOAD_INDICES, old_top);

	if (index == NUM_LOAD_INDICES)
		return 0;

	return NUM_LOAD_INDICES - 1 - index;
}

static void
migrate_top_tasks(struct task_struct *p, struct rq *src_rq, struct rq *dst_rq)
{
	int index;
	int top_index;
	u32 curr_window = p->ravg.curr_window;
	u32 prev_window = p->ravg.prev_window;
	u8 src = src_rq->curr_table;
	u8 dst = dst_rq->curr_table;
	u8 *src_table;
	u8 *dst_table;

	if (curr_window) {
		src_table = src_rq->top_tasks[src];
		dst_table = dst_rq->top_tasks[dst];
		index = load_to_index(curr_window);
		src_table[index] -= 1;
		dst_table[index] += 1;

		if (!src_table[index])
			__clear_bit(NUM_LOAD_INDICES - index - 1,
				src_rq->top_tasks_bitmap[src]);

		if (dst_table[index] == 1)
			__set_bit(NUM_LOAD_INDICES - index - 1,
				dst_rq->top_tasks_bitmap[dst]);

		if (index > dst_rq->curr_top)
			dst_rq->curr_top = index;

		top_index = src_rq->curr_top;
		if (index == top_index && !src_table[index])
			src_rq->curr_top = get_top_index(
				src_rq->top_tasks_bitmap[src], top_index);
	}

	if (prev_window) {
		src = 1 - src;
		dst = 1 - dst;
		src_table = src_rq->top_tasks[src];
		dst_table = dst_rq->top_tasks[dst];
		index = load_to_index(prev_window);
		src_table[index] -= 1;
		dst_table[index] += 1;

		if (!src_table[index])
			__clear_bit(NUM_LOAD_INDICES - index - 1,
				src_rq->top_tasks_bitmap[src]);

		if (dst_table[index] == 1)
			__set_bit(NUM_LOAD_INDICES - index - 1,
				dst_rq->top_tasks_bitmap[dst]);

		if (index > dst_rq->prev_top)
			dst_rq->prev_top = index;

		top_index = src_rq->prev_top;
		if (index == top_index && !src_table[index])
			src_rq->prev_top = get_top_index(
				src_rq->top_tasks_bitmap[src], top_index);
	}
}

void fixup_busy_time(struct task_struct *p, int new_cpu)
{
	struct rq *src_rq = task_rq(p);
	struct rq *dest_rq = cpu_rq(new_cpu);
	u64 wallclock;
	u64 *src_curr_runnable_sum, *dst_curr_runnable_sum;
	u64 *src_prev_runnable_sum, *dst_prev_runnable_sum;
	u64 *src_nt_curr_runnable_sum, *dst_nt_curr_runnable_sum;
	u64 *src_nt_prev_runnable_sum, *dst_nt_prev_runnable_sum;
	bool new_task;
	struct related_thread_group *grp;

	if (!p->on_rq && p->state != TASK_WAKING)
		return;

	if (exiting_task(p)) {
		clear_ed_task(p, src_rq);
		return;
	}

	if (p->state == TASK_WAKING)
		double_rq_lock(src_rq, dest_rq);

	if (sched_disable_window_stats)
		goto done;

	wallclock = sched_ktime_clock();

	update_task_ravg(task_rq(p)->curr, task_rq(p),
			 TASK_UPDATE,
			 wallclock, 0);
	update_task_ravg(dest_rq->curr, dest_rq,
			 TASK_UPDATE, wallclock, 0);

	update_task_ravg(p, task_rq(p), TASK_MIGRATE,
			 wallclock, 0);

	update_task_cpu_cycles(p, new_cpu, wallclock);

	new_task = is_new_task(p);
	/* Protected by rq_lock */
	grp = p->grp;

	/*
	 * For frequency aggregation, we continue to do migration fixups
	 * even for intra cluster migrations. This is because, the aggregated
	 * load has to reported on a single CPU regardless.
	 */
	if (grp && sched_freq_aggregate) {
		struct group_cpu_time *cpu_time;

		cpu_time = &src_rq->grp_time;
		src_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		src_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		src_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		cpu_time = &dest_rq->grp_time;
		dst_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		dst_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		dst_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		dst_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		if (p->ravg.curr_window) {
			*src_curr_runnable_sum -= p->ravg.curr_window;
			*dst_curr_runnable_sum += p->ravg.curr_window;
			if (new_task) {
				*src_nt_curr_runnable_sum -=
							p->ravg.curr_window;
				*dst_nt_curr_runnable_sum +=
							p->ravg.curr_window;
			}
		}

		if (p->ravg.prev_window) {
			*src_prev_runnable_sum -= p->ravg.prev_window;
			*dst_prev_runnable_sum += p->ravg.prev_window;
			if (new_task) {
				*src_nt_prev_runnable_sum -=
							p->ravg.prev_window;
				*dst_nt_prev_runnable_sum +=
							p->ravg.prev_window;
			}
		}
	} else {
		inter_cluster_migration_fixup(p, new_cpu,
						task_cpu(p), new_task);
	}

	migrate_top_tasks(p, src_rq, dest_rq);

	if (p == src_rq->ed_task) {
		src_rq->ed_task = NULL;
		if (!dest_rq->ed_task)
			dest_rq->ed_task = p;
	}

done:
	if (p->state == TASK_WAKING)
		double_rq_unlock(src_rq, dest_rq);
}

#define sched_up_down_migrate_auto_update 1
static void check_for_up_down_migrate_update(const struct cpumask *cpus)
{
	int i = cpumask_first(cpus);

	if (!sched_up_down_migrate_auto_update)
		return;

	if (cpu_max_possible_capacity(i) == max_possible_capacity)
		return;

	if (cpu_max_possible_freq(i) == cpu_max_freq(i))
		up_down_migrate_scale_factor = 1024;
	else
		up_down_migrate_scale_factor = (1024 *
				 cpu_max_possible_freq(i)) / cpu_max_freq(i);

	update_up_down_migrate();
}

/* Return cluster which can offer required capacity for group */
static struct sched_cluster *best_cluster(struct related_thread_group *grp,
					u64 total_demand, bool group_boost)
{
	struct sched_cluster *cluster = NULL;

	for_each_sched_cluster(cluster) {
		if (group_will_fit(cluster, grp, total_demand, group_boost))
			return cluster;
	}

	return sched_cluster[0];
}

static void _set_preferred_cluster(struct related_thread_group *grp)
{
	struct task_struct *p;
	u64 combined_demand = 0;
	bool boost_on_big = sched_boost_policy() == SCHED_BOOST_ON_BIG;
	bool group_boost = false;
	u64 wallclock;

	if (list_empty(&grp->tasks))
		return;

	wallclock = sched_ktime_clock();

	/*
	 * wakeup of two or more related tasks could race with each other and
	 * could result in multiple calls to _set_preferred_cluster being issued
	 * at same time. Avoid overhead in such cases of rechecking preferred
	 * cluster
	 */
	if (wallclock - grp->last_update < sched_ravg_window / 10)
		return;

	list_for_each_entry(p, &grp->tasks, grp_list) {
		if (boost_on_big && task_sched_boost(p)) {
			group_boost = true;
			break;
		}

		if (p->ravg.mark_start < wallclock -
		    (sched_ravg_window * sched_ravg_hist_size))
			continue;

		combined_demand += p->ravg.demand;

	}

	grp->preferred_cluster = best_cluster(grp,
			combined_demand, group_boost);
	grp->last_update = sched_ktime_clock();
	trace_sched_set_preferred_cluster(grp, combined_demand);
}

void set_preferred_cluster(struct related_thread_group *grp)
{
	raw_spin_lock(&grp->lock);
	_set_preferred_cluster(grp);
	raw_spin_unlock(&grp->lock);
}

#define ADD_TASK	0
#define REM_TASK	1

#define DEFAULT_CGROUP_COLOC_ID 1

/*
 * Task's cpu usage is accounted in:
 *	rq->curr/prev_runnable_sum,  when its ->grp is NULL
 *	grp->cpu_time[cpu]->curr/prev_runnable_sum, when its ->grp is !NULL
 *
 * Transfer task's cpu usage between those counters when transitioning between
 * groups
 */
static void transfer_busy_time(struct rq *rq, struct related_thread_group *grp,
				struct task_struct *p, int event)
{
	u64 wallclock;
	struct group_cpu_time *cpu_time;
	u64 *src_curr_runnable_sum, *dst_curr_runnable_sum;
	u64 *src_prev_runnable_sum, *dst_prev_runnable_sum;
	u64 *src_nt_curr_runnable_sum, *dst_nt_curr_runnable_sum;
	u64 *src_nt_prev_runnable_sum, *dst_nt_prev_runnable_sum;
	int migrate_type;
	int cpu = cpu_of(rq);
	bool new_task;
	int i;

	if (!sched_freq_aggregate)
		return;

	wallclock = sched_ktime_clock();

	update_task_ravg(rq->curr, rq, TASK_UPDATE, wallclock, 0);
	update_task_ravg(p, rq, TASK_UPDATE, wallclock, 0);
	new_task = is_new_task(p);

	cpu_time = &rq->grp_time;
	if (event == ADD_TASK) {
		migrate_type = RQ_TO_GROUP;

		src_curr_runnable_sum = &rq->curr_runnable_sum;
		dst_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		src_prev_runnable_sum = &rq->prev_runnable_sum;
		dst_prev_runnable_sum = &cpu_time->prev_runnable_sum;

		src_nt_curr_runnable_sum = &rq->nt_curr_runnable_sum;
		dst_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &rq->nt_prev_runnable_sum;
		dst_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;

		*src_curr_runnable_sum -= p->ravg.curr_window_cpu[cpu];
		*src_prev_runnable_sum -= p->ravg.prev_window_cpu[cpu];
		if (new_task) {
			*src_nt_curr_runnable_sum -=
					p->ravg.curr_window_cpu[cpu];
			*src_nt_prev_runnable_sum -=
					p->ravg.prev_window_cpu[cpu];
		}

		update_cluster_load_subtractions(p, cpu,
				rq->window_start, new_task);

	} else {
		migrate_type = GROUP_TO_RQ;

		src_curr_runnable_sum = &cpu_time->curr_runnable_sum;
		dst_curr_runnable_sum = &rq->curr_runnable_sum;
		src_prev_runnable_sum = &cpu_time->prev_runnable_sum;
		dst_prev_runnable_sum = &rq->prev_runnable_sum;

		src_nt_curr_runnable_sum = &cpu_time->nt_curr_runnable_sum;
		dst_nt_curr_runnable_sum = &rq->nt_curr_runnable_sum;
		src_nt_prev_runnable_sum = &cpu_time->nt_prev_runnable_sum;
		dst_nt_prev_runnable_sum = &rq->nt_prev_runnable_sum;

		*src_curr_runnable_sum -= p->ravg.curr_window;
		*src_prev_runnable_sum -= p->ravg.prev_window;
		if (new_task) {
			*src_nt_curr_runnable_sum -= p->ravg.curr_window;
			*src_nt_prev_runnable_sum -= p->ravg.prev_window;
		}

		/*
		 * Need to reset curr/prev windows for all CPUs, not just the
		 * ones in the same cluster. Since inter cluster migrations
		 * did not result in the appropriate book keeping, the values
		 * per CPU would be inaccurate.
		 */
		for_each_possible_cpu(i) {
			p->ravg.curr_window_cpu[i] = 0;
			p->ravg.prev_window_cpu[i] = 0;
		}
	}

	*dst_curr_runnable_sum += p->ravg.curr_window;
	*dst_prev_runnable_sum += p->ravg.prev_window;
	if (new_task) {
		*dst_nt_curr_runnable_sum += p->ravg.curr_window;
		*dst_nt_prev_runnable_sum += p->ravg.prev_window;
	}

	/*
	 * When a task enter or exits a group, it's curr and prev windows are
	 * moved to a single CPU. This behavior might be sub-optimal in the
	 * exit case, however, it saves us the overhead of handling inter
	 * cluster migration fixups while the task is part of a related group.
	 */
	p->ravg.curr_window_cpu[cpu] = p->ravg.curr_window;
	p->ravg.prev_window_cpu[cpu] = p->ravg.prev_window;

	trace_sched_migration_update_sum(p, migrate_type, rq);

	BUG_ON((s64)*src_curr_runnable_sum < 0);
	BUG_ON((s64)*src_prev_runnable_sum < 0);
	BUG_ON((s64)*src_nt_curr_runnable_sum < 0);
	BUG_ON((s64)*src_nt_prev_runnable_sum < 0);
}

static inline struct related_thread_group*
lookup_related_thread_group(unsigned int group_id)
{
	return related_thread_groups[group_id];
}

int alloc_related_thread_groups(void)
{
	int i, ret;
	struct related_thread_group *grp;

	/* groupd_id = 0 is invalid as it's special id to remove group. */
	for (i = 1; i < MAX_NUM_CGROUP_COLOC_ID; i++) {
		grp = kzalloc(sizeof(*grp), GFP_NOWAIT);
		if (!grp) {
			ret = -ENOMEM;
			goto err;
		}

		grp->id = i;
		INIT_LIST_HEAD(&grp->tasks);
		INIT_LIST_HEAD(&grp->list);
		raw_spin_lock_init(&grp->lock);

		related_thread_groups[i] = grp;
	}

	return 0;

err:
	for (i = 1; i < MAX_NUM_CGROUP_COLOC_ID; i++) {
		grp = lookup_related_thread_group(i);
		if (grp) {
			kfree(grp);
			related_thread_groups[i] = NULL;
		} else {
			break;
		}
	}

	return ret;
}

static void remove_task_from_group(struct task_struct *p)
{
	struct related_thread_group *grp = p->grp;
	struct rq *rq;
	int empty_group = 1;

	raw_spin_lock(&grp->lock);

	rq = __task_rq_lock(p);
	transfer_busy_time(rq, p->grp, p, REM_TASK);
	list_del_init(&p->grp_list);
	rcu_assign_pointer(p->grp, NULL);
	__task_rq_unlock(rq);

	if (!list_empty(&grp->tasks)) {
		empty_group = 0;
		_set_preferred_cluster(grp);
	}

	raw_spin_unlock(&grp->lock);

	/* Reserved groups cannot be destroyed */
	if (empty_group && grp->id != DEFAULT_CGROUP_COLOC_ID)
		 /*
		  * We test whether grp->list is attached with list_empty()
		  * hence re-init the list after deletion.
		  */
		list_del_init(&grp->list);
}

static int
add_task_to_group(struct task_struct *p, struct related_thread_group *grp)
{
	struct rq *rq;

	raw_spin_lock(&grp->lock);

	/*
	 * Change p->grp under rq->lock. Will prevent races with read-side
	 * reference of p->grp in various hot-paths
	 */
	rq = __task_rq_lock(p);
	transfer_busy_time(rq, grp, p, ADD_TASK);
	list_add(&p->grp_list, &grp->tasks);
	rcu_assign_pointer(p->grp, grp);
	__task_rq_unlock(rq);

	_set_preferred_cluster(grp);

	raw_spin_unlock(&grp->lock);

	return 0;
}

void add_new_task_to_grp(struct task_struct *new)
{
	unsigned long flags;
	struct related_thread_group *grp;
	struct task_struct *leader = new->group_leader;
	unsigned int leader_grp_id = sched_get_group_id(leader);

	if (!sysctl_sched_enable_thread_grouping &&
	    leader_grp_id != DEFAULT_CGROUP_COLOC_ID)
		return;

	if (thread_group_leader(new))
		return;

	if (leader_grp_id == DEFAULT_CGROUP_COLOC_ID) {
		if (!same_schedtune(new, leader))
			return;
	}

	write_lock_irqsave(&related_thread_group_lock, flags);

	rcu_read_lock();
	grp = task_related_thread_group(leader);
	rcu_read_unlock();

	/*
	 * It's possible that someone already added the new task to the
	 * group. A leader's thread group is updated prior to calling
	 * this function. It's also possible that the leader has exited
	 * the group. In either case, there is nothing else to do.
	 */
	if (!grp || new->grp) {
		write_unlock_irqrestore(&related_thread_group_lock, flags);
		return;
	}

	raw_spin_lock(&grp->lock);

	rcu_assign_pointer(new->grp, grp);
	list_add(&new->grp_list, &grp->tasks);

	raw_spin_unlock(&grp->lock);
	write_unlock_irqrestore(&related_thread_group_lock, flags);
}

static int __sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	int rc = 0;
	unsigned long flags;
	struct related_thread_group *grp = NULL;

	if (group_id >= MAX_NUM_CGROUP_COLOC_ID)
		return -EINVAL;

	raw_spin_lock_irqsave(&p->pi_lock, flags);
	write_lock(&related_thread_group_lock);

	/* Switching from one group to another directly is not permitted */
	if ((current != p && p->flags & PF_EXITING) ||
			(!p->grp && !group_id) ||
			(p->grp && group_id))
		goto done;

	if (!group_id) {
		remove_task_from_group(p);
		goto done;
	}

	grp = lookup_related_thread_group(group_id);
	if (list_empty(&grp->list))
		list_add(&grp->list, &active_related_thread_groups);

	rc = add_task_to_group(p, grp);
done:
	write_unlock(&related_thread_group_lock);
	raw_spin_unlock_irqrestore(&p->pi_lock, flags);

	return rc;
}

int sched_set_group_id(struct task_struct *p, unsigned int group_id)
{
	/* DEFAULT_CGROUP_COLOC_ID is a reserved id */
	if (group_id == DEFAULT_CGROUP_COLOC_ID)
		return -EINVAL;

	return __sched_set_group_id(p, group_id);
}

unsigned int sched_get_group_id(struct task_struct *p)
{
	unsigned int group_id;
	struct related_thread_group *grp;

	rcu_read_lock();
	grp = task_related_thread_group(p);
	group_id = grp ? grp->id : 0;
	rcu_read_unlock();

	return group_id;
}

#if defined(CONFIG_SCHED_TUNE) && defined(CONFIG_CGROUP_SCHEDTUNE)
/*
 * We create a default colocation group at boot. There is no need to
 * synchronize tasks between cgroups at creation time because the
 * correct cgroup hierarchy is not available at boot. Therefore cgroup
 * colocation is turned off by default even though the colocation group
 * itself has been allocated. Furthermore this colocation group cannot
 * be destroyted once it has been created. All of this has been as part
 * of runtime optimizations.
 *
 * The job of synchronizing tasks to the colocation group is done when
 * the colocation flag in the cgroup is turned on.
 */
static int __init create_default_coloc_group(void)
{
	struct related_thread_group *grp = NULL;
	unsigned long flags;

	grp = lookup_related_thread_group(DEFAULT_CGROUP_COLOC_ID);
	write_lock_irqsave(&related_thread_group_lock, flags);
	list_add(&grp->list, &active_related_thread_groups);
	write_unlock_irqrestore(&related_thread_group_lock, flags);

	update_freq_aggregate_threshold(MAX_FREQ_AGGR_THRESH);
	return 0;
}
late_initcall(create_default_coloc_group);

int sync_cgroup_colocation(struct task_struct *p, bool insert)
{
	unsigned int grp_id = insert ? DEFAULT_CGROUP_COLOC_ID : 0;

	return __sched_set_group_id(p, grp_id);
}
#endif

static void update_cpu_cluster_capacity(const cpumask_t *cpus)
{
	int i;
	struct sched_cluster *cluster;
	struct cpumask cpumask;

	cpumask_copy(&cpumask, cpus);
	pre_big_task_count_change(cpu_possible_mask);

	for_each_cpu(i, &cpumask) {
		cluster = cpu_rq(i)->cluster;
		cpumask_andnot(&cpumask, &cpumask, &cluster->cpus);

		cluster->capacity = compute_capacity(cluster);
		cluster->load_scale_factor = compute_load_scale_factor(cluster);

		/* 'cpus' can contain cpumask more than one cluster */
		check_for_up_down_migrate_update(&cluster->cpus);
	}

	__update_min_max_capacity();

	post_big_task_count_change(cpu_possible_mask);
}

static DEFINE_SPINLOCK(cpu_freq_min_max_lock);
void sched_update_cpu_freq_min_max(const cpumask_t *cpus, u32 fmin, u32 fmax)
{
	struct cpumask cpumask;
	struct sched_cluster *cluster;
	int i, update_capacity = 0;
	unsigned long flags;

	spin_lock_irqsave(&cpu_freq_min_max_lock, flags);
	cpumask_copy(&cpumask, cpus);
	for_each_cpu(i, &cpumask) {
		cluster = cpu_rq(i)->cluster;
		cpumask_andnot(&cpumask, &cpumask, &cluster->cpus);

		update_capacity += (cluster->max_mitigated_freq != fmax);
		cluster->max_mitigated_freq = fmax;
	}
	spin_unlock_irqrestore(&cpu_freq_min_max_lock, flags);

	if (update_capacity)
		update_cpu_cluster_capacity(cpus);
}

static int cpufreq_notifier_policy(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_policy *policy = (struct cpufreq_policy *)data;
	struct sched_cluster *cluster = NULL;
	struct cpumask policy_cluster = *policy->related_cpus;
	unsigned int orig_max_freq = 0;
	int i, j, update_capacity = 0;

	if (val != CPUFREQ_NOTIFY && val != CPUFREQ_REMOVE_POLICY &&
						val != CPUFREQ_CREATE_POLICY)
		return 0;

	if (val == CPUFREQ_REMOVE_POLICY || val == CPUFREQ_CREATE_POLICY) {
		update_min_max_capacity();
		return 0;
	}

	max_possible_freq = max(max_possible_freq, policy->cpuinfo.max_freq);
	if (min_max_freq == 1)
		min_max_freq = UINT_MAX;
	min_max_freq = min(min_max_freq, policy->cpuinfo.max_freq);
	BUG_ON(!min_max_freq);
	BUG_ON(!policy->max);

	for_each_cpu(i, &policy_cluster) {
		cluster = cpu_rq(i)->cluster;
		cpumask_andnot(&policy_cluster, &policy_cluster,
						&cluster->cpus);

		orig_max_freq = cluster->max_freq;
		cluster->min_freq = policy->min;
		cluster->max_freq = policy->max;
		cluster->cur_freq = policy->cur;

		if (!cluster->freq_init_done) {
			mutex_lock(&cluster_lock);
			for_each_cpu(j, &cluster->cpus)
				cpumask_copy(&cpu_rq(j)->freq_domain_cpumask,
						policy->related_cpus);
			cluster->max_possible_freq = policy->cpuinfo.max_freq;
			cluster->max_possible_capacity =
				compute_max_possible_capacity(cluster);
			cluster->freq_init_done = true;

			sort_clusters();
			update_all_clusters_stats();
			mutex_unlock(&cluster_lock);
			continue;
		}

		update_capacity += (orig_max_freq != cluster->max_freq);
	}

	if (update_capacity)
		update_cpu_cluster_capacity(policy->related_cpus);

	return 0;
}

static int cpufreq_notifier_trans(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = (struct cpufreq_freqs *)data;
	unsigned int cpu = freq->cpu, new_freq = freq->new;
	unsigned long flags;
	struct sched_cluster *cluster;
	struct cpumask policy_cpus = cpu_rq(cpu)->freq_domain_cpumask;
	int i, j;

	if (val != CPUFREQ_POSTCHANGE)
		return 0;

	BUG_ON(!new_freq);

	if (cpu_cur_freq(cpu) == new_freq)
		return 0;

	for_each_cpu(i, &policy_cpus) {
		cluster = cpu_rq(i)->cluster;

		for_each_cpu(j, &cluster->cpus) {
			struct rq *rq = cpu_rq(j);

			raw_spin_lock_irqsave(&rq->lock, flags);
			update_task_ravg(rq->curr, rq, TASK_UPDATE,
						sched_ktime_clock(), 0);
			raw_spin_unlock_irqrestore(&rq->lock, flags);
		}

		cluster->cur_freq = new_freq;
		cpumask_andnot(&policy_cpus, &policy_cpus, &cluster->cpus);
	}

	return 0;
}

static int pwr_stats_ready_notifier(struct notifier_block *nb,
				    unsigned long cpu, void *data)
{
	cpumask_t mask = CPU_MASK_NONE;

	cpumask_set_cpu(cpu, &mask);
	sched_update_freq_max_load(&mask);

	mutex_lock(&cluster_lock);
	sort_clusters();
	mutex_unlock(&cluster_lock);

	return 0;
}

static struct notifier_block notifier_policy_block = {
	.notifier_call = cpufreq_notifier_policy
};

static struct notifier_block notifier_trans_block = {
	.notifier_call = cpufreq_notifier_trans
};

static struct notifier_block notifier_pwr_stats_ready = {
	.notifier_call = pwr_stats_ready_notifier
};

int __weak register_cpu_pwr_stats_ready_notifier(struct notifier_block *nb)
{
	return -EINVAL;
}

static int register_sched_callback(void)
{
	int ret;

	ret = cpufreq_register_notifier(&notifier_policy_block,
						CPUFREQ_POLICY_NOTIFIER);

	if (!ret)
		ret = cpufreq_register_notifier(&notifier_trans_block,
						CPUFREQ_TRANSITION_NOTIFIER);

	register_cpu_pwr_stats_ready_notifier(&notifier_pwr_stats_ready);

	return 0;
}

/*
 * cpufreq callbacks can be registered at core_initcall or later time.
 * Any registration done prior to that is "forgotten" by cpufreq. See
 * initialization of variable init_cpufreq_transition_notifier_list_called
 * for further information.
 */
core_initcall(register_sched_callback);

int update_preferred_cluster(struct related_thread_group *grp,
		struct task_struct *p, u32 old_load)
{
	u32 new_load = task_load(p);

	if (!grp)
		return 0;

	/*
	 * Update if task's load has changed significantly or a complete window
	 * has passed since we last updated preference
	 */
	if (abs(new_load - old_load) > sched_ravg_window / 4 ||
		sched_ktime_clock() - grp->last_update > sched_ravg_window)
		return 1;

	return 0;
}

bool early_detection_notify(struct rq *rq, u64 wallclock)
{
	struct task_struct *p;
	int loop_max = 10;

	if (sched_boost_policy() == SCHED_BOOST_NONE || !rq->cfs.h_nr_running)
		return 0;

	rq->ed_task = NULL;
	list_for_each_entry(p, &rq->cfs_tasks, se.group_node) {
		if (!loop_max)
			break;

		if (wallclock - p->last_wake_ts >= EARLY_DETECTION_DURATION) {
			rq->ed_task = p;
			return 1;
		}

		loop_max--;
	}

	return 0;
}

void update_avg_burst(struct task_struct *p)
{
	update_avg(&p->ravg.avg_burst, p->ravg.curr_burst);
	p->ravg.curr_burst = 0;
}

void note_task_waking(struct task_struct *p, u64 wallclock)
{
	u64 sleep_time = wallclock - p->last_switch_out_ts;

	/*
	 * When a short burst and short sleeping task goes for a long
	 * sleep, the task's avg_sleep_time gets boosted. It will not
	 * come below short_sleep threshold for a lot of time and it
	 * results in incorrect packing. The idead behind tracking
	 * avg_sleep_time is to detect if a task is short sleeping
	 * or not. So limit the sleep time to twice the short sleep
	 * threshold. For regular long sleeping tasks, the avg_sleep_time
	 * would be higher than threshold, and packing happens correctly.
	 */
	sleep_time = min_t(u64, sleep_time, 2 * sysctl_sched_short_sleep);
	update_avg(&p->ravg.avg_sleep_time, sleep_time);

	p->last_wake_ts = wallclock;
}

#ifdef CONFIG_CGROUP_SCHED
u64 cpu_upmigrate_discourage_read_u64(struct cgroup_subsys_state *css,
					  struct cftype *cft)
{
	struct task_group *tg = css_tg(css);

	return tg->upmigrate_discouraged;
}

int cpu_upmigrate_discourage_write_u64(struct cgroup_subsys_state *css,
				struct cftype *cft, u64 upmigrate_discourage)
{
	struct task_group *tg = css_tg(css);
	int discourage = upmigrate_discourage > 0;

	if (tg->upmigrate_discouraged == discourage)
		return 0;

	/*
	 * Revisit big-task classification for tasks of this cgroup. It would
	 * have been efficient to walk tasks of just this cgroup in running
	 * state, but we don't have easy means to do that. Walk all tasks in
	 * running state on all cpus instead and re-visit their big task
	 * classification.
	 */
	get_online_cpus();
	pre_big_task_count_change(cpu_online_mask);

	tg->upmigrate_discouraged = discourage;

	post_big_task_count_change(cpu_online_mask);
	put_online_cpus();

	return 0;
}
#endif /* CONFIG_CGROUP_SCHED */
