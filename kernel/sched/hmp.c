/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include "walt.h"

#include <trace/events/sched.h>

#define CSTATE_LATENCY_GRANULARITY_SHIFT (6)

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

unsigned long __weak arch_get_cpu_efficiency(int cpu)
{
	return SCHED_CAPACITY_SCALE;
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
 * Tasks that are runnable continuously for a period greather than
 * EARLY_DETECTION_DURATION can be flagged early as potential
 * high load tasks.
 */
#define EARLY_DETECTION_DURATION 9500000

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
__read_mostly unsigned int sysctl_sched_pred_alert_freq = 10 * 1024 * 1024;

/* Maximum allowed threshold before freq aggregation must be enabled */
#define MAX_FREQ_AGGR_THRESH 1000

#define for_each_related_thread_group(grp) \
	list_for_each_entry(grp, &active_related_thread_groups, list)

/* Size of bitmaps maintained to track top tasks */
static const unsigned int top_tasks_bitmap_size =
		BITS_TO_LONGS(NUM_LOAD_INDICES + 1) * sizeof(unsigned long);

__read_mostly unsigned int sysctl_sched_freq_aggregate = 1;

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
		inc_hmp_sched_stats_fair(rq, p, 0);
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

/* Return task demand in percentage scale */
unsigned int pct_task_load(struct task_struct *p)
{
	unsigned int load;

	load = div64_u64((u64)task_load(p) * 100, (u64)max_task_load());

	return load;
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

		/*
		 * Ensure that we don't report load for 'cpu' again via the
		 * cpufreq_update_util path in the window that started at
		 * rq->window_start
		 */
		rq->load_reported_window = rq->window_start;

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
				     early_detection[i]);
		i++;
	}
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

void update_cpu_cluster_capacity(const cpumask_t *cpus)
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
	cpufreq_register_notifier(&notifier_trans_block,
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

void update_avg_burst(struct task_struct *p)
{
	update_avg(&p->ravg.avg_burst, p->ravg.curr_burst);
	p->ravg.curr_burst = 0;
}

void note_task_waking(struct task_struct *p, u64 wallclock)
{
	u64 sleep_time = wallclock - p->last_switch_out_ts;

	p->last_wake_ts = wallclock;
	update_avg(&p->ravg.avg_sleep_time, sleep_time);
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
