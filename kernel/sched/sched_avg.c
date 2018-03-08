/* Copyright (c) 2012, 2015-2016, 2018 The Linux Foundation. All rights reserved.
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

/*
 * Scheduler hook for average runqueue determination
 */
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/math64.h>

#include "sched.h"
#include <trace/events/sched.h>

static DEFINE_PER_CPU(u64, nr_prod_sum);
static DEFINE_PER_CPU(u64, last_time);
static DEFINE_PER_CPU(u64, nr_big_prod_sum);
static DEFINE_PER_CPU(u64, nr);

static DEFINE_PER_CPU(unsigned long, iowait_prod_sum);
static DEFINE_PER_CPU(spinlock_t, nr_lock) = __SPIN_LOCK_UNLOCKED(nr_lock);
static s64 last_get_time;

static DEFINE_PER_CPU(atomic64_t, last_busy_time) = ATOMIC64_INIT(0);

/**
 * sched_get_nr_running_avg
 * @return: Average nr_running, iowait and nr_big_tasks value since last poll.
 *	    Returns the avg * 100 to return up to two decimal points
 *	    of accuracy.
 *
 * Obtains the average nr_running value since the last poll.
 * This function may not be called concurrently with itself
 */
void sched_get_nr_running_avg(int *avg, int *iowait_avg, int *big_avg)
{
	int cpu;
	u64 curr_time = sched_clock();
	u64 diff = curr_time - last_get_time;
	u64 tmp_avg = 0, tmp_iowait = 0, tmp_big_avg = 0;

	*avg = 0;
	*iowait_avg = 0;
	*big_avg = 0;

	if (!diff)
		return;

	/* read and reset nr_running counts */
	for_each_possible_cpu(cpu) {
		unsigned long flags;

		spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
		curr_time = sched_clock();
		diff = curr_time - per_cpu(last_time, cpu);
		BUG_ON((s64)diff < 0);

		tmp_avg += per_cpu(nr_prod_sum, cpu);
		tmp_avg += per_cpu(nr, cpu) * diff;

		tmp_big_avg += per_cpu(nr_big_prod_sum, cpu);
		tmp_big_avg += nr_eligible_big_tasks(cpu) * diff;

		tmp_iowait += per_cpu(iowait_prod_sum, cpu);
		tmp_iowait +=  nr_iowait_cpu(cpu) * diff;

		per_cpu(last_time, cpu) = curr_time;

		per_cpu(nr_prod_sum, cpu) = 0;
		per_cpu(nr_big_prod_sum, cpu) = 0;
		per_cpu(iowait_prod_sum, cpu) = 0;

		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
	}

	diff = curr_time - last_get_time;
	last_get_time = curr_time;

	*avg = (int)div64_u64(tmp_avg * 100, diff);
	*big_avg = (int)div64_u64(tmp_big_avg * 100, diff);
	*iowait_avg = (int)div64_u64(tmp_iowait * 100, diff);

	trace_sched_get_nr_running_avg(*avg, *big_avg, *iowait_avg);

	BUG_ON(*avg < 0 || *big_avg < 0 || *iowait_avg < 0);
	pr_debug("%s - avg:%d big_avg:%d iowait_avg:%d\n",
				 __func__, *avg, *big_avg, *iowait_avg);
}
EXPORT_SYMBOL(sched_get_nr_running_avg);

#define BUSY_NR_RUN		3
#define BUSY_LOAD_FACTOR	10

#ifdef CONFIG_SCHED_HMP
static inline void update_last_busy_time(int cpu, bool dequeue,
				unsigned long prev_nr_run, u64 curr_time)
{
	bool nr_run_trigger = false, load_trigger = false;

	if (!hmp_capable() || is_min_capacity_cpu(cpu))
		return;

	if (prev_nr_run >= BUSY_NR_RUN && per_cpu(nr, cpu) < BUSY_NR_RUN)
		nr_run_trigger = true;

	if (dequeue) {
		u64 load;

		load = cpu_rq(cpu)->hmp_stats.cumulative_runnable_avg;
		load = scale_load_to_cpu(load, cpu);

		if (load * BUSY_LOAD_FACTOR > sched_ravg_window)
			load_trigger = true;
	}

	if (nr_run_trigger || load_trigger)
		atomic64_set(&per_cpu(last_busy_time, cpu), curr_time);
}
#else
static inline void update_last_busy_time(int cpu, bool dequeue,
				unsigned long prev_nr_run, u64 curr_time)
{
}
#endif

/**
 * sched_update_nr_prod
 * @cpu: The core id of the nr running driver.
 * @delta: Adjust nr by 'delta' amount
 * @inc: Whether we are increasing or decreasing the count
 * @return: N/A
 *
 * Update average with latest nr_running value for CPU
 */
void sched_update_nr_prod(int cpu, long delta, bool inc)
{
	u64 diff;
	u64 curr_time;
	unsigned long flags, nr_running;

	spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
	nr_running = per_cpu(nr, cpu);
	curr_time = sched_clock();
	diff = curr_time - per_cpu(last_time, cpu);
	BUG_ON((s64)diff < 0);
	per_cpu(last_time, cpu) = curr_time;
	per_cpu(nr, cpu) = nr_running + (inc ? delta : -delta);

	BUG_ON((s64)per_cpu(nr, cpu) < 0);

	update_last_busy_time(cpu, !inc, nr_running, curr_time);

	per_cpu(nr_prod_sum, cpu) += nr_running * diff;
	per_cpu(nr_big_prod_sum, cpu) += nr_eligible_big_tasks(cpu) * diff;
	per_cpu(iowait_prod_sum, cpu) += nr_iowait_cpu(cpu) * diff;
	spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_prod);

u64 sched_get_cpu_last_busy_time(int cpu)
{
	return atomic64_read(&per_cpu(last_busy_time, cpu));
}
