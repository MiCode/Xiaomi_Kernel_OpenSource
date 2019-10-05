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

/*
 * Scheduler hook for average runqueue determination
 */
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/math64.h>
#include <linux/sched/clock.h>
#include <linux/topology.h>
#include <linux/arch_topology.h>
#include <trace/events/sched.h>
#include <mt-plat/mt_sched.h>
#ifdef CONFIG_MTK_SCHED_RQAVG_US
#include "rq_stats.h"
#endif
#include "../../../kernel/sched/sched.h"

static DEFINE_PER_CPU(u64, nr_prod_sum);
static DEFINE_PER_CPU(u64, nr_heavy_prod_sum);
static DEFINE_PER_CPU(u64, last_time);
static DEFINE_PER_CPU(u64, last_heavy_time);
static DEFINE_PER_CPU(u64, nr);
static DEFINE_PER_CPU(u64, nr_heavy);
static DEFINE_PER_CPU(unsigned long, iowait_prod_sum);
static DEFINE_PER_CPU(spinlock_t, nr_lock) = __SPIN_LOCK_UNLOCKED(nr_lock);
static DEFINE_PER_CPU(spinlock_t, nr_heavy_lock) =
			__SPIN_LOCK_UNLOCKED(nr_heavy_lock);
static u64 last_get_time;
static int init_heavy;

struct cluster_heavy_tbl_t {
	u64 last_get_heavy_time;
};

struct cluster_heavy_tbl_t *cluster_heavy_tbl;

static int init_heavy_tlb(void);

/**
 * sched_get_nr_running_avg
 * @return: Average nr_running and iowait value since last poll.
 *          Returns the avg * 100 to return up to two decimal points
 *          of accuracy.
 *          And return scaled tasks number of the last poll.
 *
 * Obtains the average nr_running value since the last poll.
 * This function may not be called concurrently with itself
 */
int sched_get_nr_running_avg(int *avg, int *iowait_avg)
{
	int cpu;
	u64 curr_time = sched_clock();
	s64 diff = (s64) (curr_time - last_get_time);
	u64 tmp_avg = 0, tmp_iowait = 0, old_lgt;
	bool clk_faulty = 0;
	u32 cpumask = 0;
	int scaled_tlp = 0; /* the tasks number of last poll */

	*avg = 0;
	*iowait_avg = 0;

	if (!diff)
		return 0;
	WARN(diff < 0, "[%s] time last:%llu curr:%llu ",
		__func__, last_get_time, curr_time);

	old_lgt = last_get_time;
	last_get_time = curr_time;
	/* read and reset nr_running counts */
	for_each_possible_cpu(cpu) {
		unsigned long flags;

		spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
		/* error handling for problematic clock violation */
		if ((s64) (curr_time - per_cpu(last_time, cpu) < 0)) {
			clk_faulty = 1;
			cpumask |= 1 << cpu;
		}
		tmp_avg += per_cpu(nr_prod_sum, cpu);
		/* record tasks nr of last poll */
		scaled_tlp += per_cpu(nr, cpu);
		tmp_avg += per_cpu(nr, cpu) *
			(curr_time - per_cpu(last_time, cpu));
		tmp_iowait = per_cpu(iowait_prod_sum, cpu);
		tmp_iowait += nr_iowait_cpu(cpu) *
			(curr_time - per_cpu(last_time, cpu));
		per_cpu(last_time, cpu) = curr_time;
		per_cpu(nr_prod_sum, cpu) = 0;
		per_cpu(iowait_prod_sum, cpu) = 0;
		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
	}

	/* error handling for problematic clock violation */
	if (clk_faulty) {
		*avg = 0;
		*iowait_avg = 0;
		pr_info("[%s] **** CPU (0x%08x)clock may unstable !!\n",
					__func__, cpumask);
		return 0;
	}

	*avg = (int)div64_u64(tmp_avg * 100, (u64) diff);
	*iowait_avg = (int)div64_u64(tmp_iowait * 100, (u64) diff);

	WARN(*avg < 0,
		"[%s] avg:%d(%llu/%lld), time last:%llu curr:%llu ",
		__func__, *avg, tmp_avg, diff, old_lgt, curr_time);
	if (unlikely(*avg < 0))
		*avg = 0;
	WARN(*iowait_avg < 0,
		"[%s] iowait_avg:%d(%llu/%lld) time last:%llu curr:%llu ",
		__func__, *iowait_avg, tmp_iowait, diff, old_lgt, curr_time);
	if (unlikely(*iowait_avg < 0))
		*iowait_avg = 0;

	return scaled_tlp*100;
}
EXPORT_SYMBOL(sched_get_nr_running_avg);

int reset_heavy_task_stats(int cpu)
{
	int nr_heavy_tasks;
	unsigned long flags;

	spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);
	nr_heavy_tasks = per_cpu(nr_heavy, cpu);
	if (nr_heavy_tasks) {
		pr_info(
		"[heavy_task] %s: nr_heavy_tasks=%d in cpu%d\n", __func__,
		nr_heavy_tasks, cpu);
		per_cpu(nr_heavy, cpu) = 0;
	}
	spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);

	return nr_heavy_tasks;
}
EXPORT_SYMBOL(reset_heavy_task_stats);

void heavy_thresh_chg_notify(void)
{
	int nr_heavy_task;
	int cpu;
	unsigned long flags;
	struct task_struct *p;

	for_each_online_cpu(cpu) {
		u64 curr_time = sched_clock();

		nr_heavy_task = 0;

		raw_spin_lock_irqsave(&cpu_rq(cpu)->lock, flags);
		list_for_each_entry(p, &cpu_rq(cpu)->cfs_tasks, se.group_node) {
#ifdef CONFIG_MTK_SCHED_RQAVG_US
			if (is_heavy_task(p))
				nr_heavy_task++;
#endif
		}

		/* Threshold for heavy is changed. Need to reset stats */
		spin_lock(&per_cpu(nr_heavy_lock, cpu));
		per_cpu(nr_heavy_prod_sum, cpu) = 0;
		per_cpu(nr_heavy, cpu) = nr_heavy_task;
		per_cpu(last_heavy_time, cpu) = curr_time;
		spin_unlock(&per_cpu(nr_heavy_lock, cpu));

		raw_spin_unlock_irqrestore(&cpu_rq(cpu)->lock, flags);
	}
}


int sched_get_nr_heavy_running_avg(int cluster_id, int *avg)
{
	u64 curr_time = sched_clock();
	s64 diff;

	u64 tmp_avg = 0, old_lgt;
	u32 cpumask = 0;
	bool clk_faulty = 0;
	unsigned long flags;
	int cpu = 0;
	int cluster_nr;
	struct cpumask cls_cpus;
	int ack_cap = 0;

	u64 last_heavy_nr = 0;

	/* Need to make sure initialization done. */
	if (!init_heavy) {
		*avg = 0;
		return -1;
	}

	/* cluster_id  need reasonale. */
	cluster_nr = arch_get_nr_clusters();
	if (cluster_id < 0 || cluster_id >= cluster_nr) {
		pr_info("[%s] invalid cluster id %d\n", __func__, cluster_id);
		return -1;
	}

	/* Time diff can't be zero. */
	diff = (s64)(curr_time -
		cluster_heavy_tbl[cluster_id].last_get_heavy_time);
	if (!diff) {
		*avg = 0;
		return -1;
	}
	old_lgt = cluster_heavy_tbl[cluster_id].last_get_heavy_time;

	arch_get_cluster_cpus(&cls_cpus, cluster_id);
	cluster_heavy_tbl[cluster_id].last_get_heavy_time = curr_time;

	/* visit all cpus of this cluster */
	for_each_cpu(cpu, &cls_cpus) {
		spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);
		if ((s64) (curr_time - per_cpu(last_heavy_time, cpu) < 0)) {
			clk_faulty = 1;
			cpumask |= 1 << cpu;
		}

		last_heavy_nr += per_cpu(nr_heavy, cpu);

		tmp_avg += per_cpu(nr_heavy_prod_sum, cpu);
#ifdef CONFIG_MTK_SCHED_RQAVG_US
		ack_cap = is_ack_curcap(cpu);
		if (ack_cap)
			tmp_avg += per_cpu(nr_heavy, cpu) *
			(curr_time - per_cpu(last_heavy_time, cpu));

		trace_sched_avg_heavy_nr("hps_main", per_cpu(nr_heavy, cpu),
			(curr_time - per_cpu(last_heavy_time, cpu)),
			ack_cap, cpu);
#else
		ack_cap = -1;
		tmp_avg +=
		per_cpu(nr_heavy, cpu) *
		(curr_time - per_cpu(last_heavy_time, cpu));
#endif
		per_cpu(last_heavy_time, cpu) = curr_time;

		/* clear prod_sum */
		per_cpu(nr_heavy_prod_sum, cpu) = 0;

		spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);
	}

	if (clk_faulty) {
		*avg = 0;
		trace_sched_avg_heavy_time(-1, -1, cluster_id);
		return -1;
	}

	*avg = (int)div64_u64(tmp_avg * 100, (u64) diff);

	trace_sched_avg_heavy_time(diff, old_lgt, cluster_id);

	return last_heavy_nr;
}
EXPORT_SYMBOL(sched_get_nr_heavy_running_avg);

/**
 * sched_update_nr_prod
 * @cpu: The core id of the nr running driver.
 * @nr: Updated nr running value for cpu.
 * @inc: Whether we are increasing or decreasing the count
 * @return: N/A
 *
 * Update average with latest nr_running value for CPU
 */
void sched_update_nr_prod(int cpu, unsigned long nr_running, int inc)
{
	s64 diff;
	u64 curr_time;
	unsigned long flags;

	spin_lock_irqsave(&per_cpu(nr_lock, cpu), flags);
	curr_time = sched_clock();
	diff = (s64) (curr_time - per_cpu(last_time, cpu));
	/* skip this problematic clock violation */
	if (diff < 0) {
		spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
		return;
	}
	/* ////////////////////////////////////// */

	per_cpu(last_time, cpu) = curr_time;
	per_cpu(nr, cpu) = nr_running + inc;

	WARN_ON(per_cpu(nr, cpu) < 0);

	per_cpu(nr_prod_sum, cpu) += nr_running * diff;
	per_cpu(iowait_prod_sum, cpu) += nr_iowait_cpu(cpu) * diff;

	spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_prod);

void sched_update_nr_heavy_prod(const char *invoker, int cpu,
			int heavy_nr_inc, bool ack_cap_req)
{
	s64 diff;
	u64 curr_time;
	unsigned long flags;
	unsigned long prev_heavy_nr;
	int ack_cap = -1;

	if (!init_heavy) {
		init_heavy_tlb();
		if (!init_heavy) {
			WARN_ON(!init_heavy);
			return;
		}
	}

	spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);

	/* for heavy task avg */
	prev_heavy_nr = per_cpu(nr_heavy, cpu);
	per_cpu(nr_heavy, cpu) = prev_heavy_nr + heavy_nr_inc;
	/* WARN_ON((int)per_cpu(nr_heavy, cpu) < 0); */


	curr_time = sched_clock();
	diff = (s64) (curr_time - per_cpu(last_heavy_time, cpu));
	/* skip this problematic clock violation */
	if (diff < 0) {
		spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);
		return;
	}

	/* for heavy task avg */
	per_cpu(last_heavy_time, cpu) = curr_time;

	/* for current opp control */
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	if (ack_cap_req) {
		ack_cap = is_ack_curcap(cpu);
		per_cpu(nr_heavy_prod_sum, cpu) += prev_heavy_nr*diff*ack_cap;
		trace_sched_avg_heavy_nr(invoker, prev_heavy_nr,
			diff, ack_cap, cpu);
	} else {
		ack_cap = -1;
		per_cpu(nr_heavy_prod_sum, cpu) += prev_heavy_nr*diff;
		trace_sched_avg_heavy_nr(invoker, prev_heavy_nr,
			diff, ack_cap, cpu);
	}
#else
	ack_cap = -1;
	per_cpu(nr_heavy_prod_sum, cpu) += prev_heavy_nr*diff;
#endif

	spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_heavy_prod);

static int init_heavy_tlb(void)
{
	if (!init_heavy) {
		/* init variables */
		int tmp_cpu, cluster_nr;

		pr_info("%s start.\n", __func__);

		for_each_possible_cpu(tmp_cpu) {
			per_cpu(nr_heavy, tmp_cpu) = 0;
		}

		cluster_nr = arch_get_nr_clusters();
		cluster_heavy_tbl = kcalloc(cluster_nr,
			sizeof(struct cluster_heavy_tbl_t), GFP_ATOMIC);
		if (!cluster_heavy_tbl)
			return 0;
		init_heavy = 1;
	}

	return init_heavy;
}
