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
#include "rq_stats.h"
#include "../../../kernel/sched/sched.h"

unsigned long boosted_cpu_util(int cpu, unsigned long other_util);

enum overutil_type_t {
	NO_OVERUTIL = 0,
	L_OVERUTIL,
	H_OVERUTIL
};

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

struct overutil_stats {
	int nr_overutil_l;
	int nr_overutil_h;
	int overutil_thresh_l;
	int overutil_thresh_h;
	u64 nr_overutil_l_prod_sum;
	u64 nr_overutil_h_prod_sum;
	u64 l_last_update_time;
	u64 h_last_update_time;
};

static struct overutil_stats __percpu *cpu_overutil_state;

struct cluster_heavy_tbl_t {
	u64 last_get_heavy_time;
	u64 last_get_overutil_time;
	u64 max_capacity;
};

struct cluster_heavy_tbl_t *cluster_heavy_tbl;

static int init_heavy_tlb(void);

static inline unsigned long task_utilization(struct task_struct *p)
{
	return p->se.avg.util_avg;
}

enum overutil_type_t is_task_overutil(struct task_struct *p)
{
	struct overutil_stats *cpu_overutil;
	/* int cid; */
	/* int cluster_nr = arch_get_nr_clusters(); */

	if (!p)
		return NO_OVERUTIL;

#ifdef CONFIG_SCHED_HMP_PRIO_FILTER
	if (task_low_priority(p->prio))
		return NO_OVERUTIL;
#endif
	cpu_overutil = per_cpu_ptr(cpu_overutil_state, cpu_of(task_rq(p)));

#if 0
	/* apply new threshold for overutilization */
	cid = cpu_topology[cpu_of(task_rq(p))].cluster_id;

	if (cid == 0) {
		cpu_overutil->overutil_thresh_l = INT_MAX;
		cpu_overutil->overutil_thresh_h =
		(cluster_heavy_tbl[cid].max_capacity*8)/10;
	} else if (cid > 0 && cid < (cluster_nr-1)) {
		cpu_overutil->overutil_thresh_l =
		(cluster_heavy_tbl[cid-1].max_capacity*8)/10;
		cpu_overutil->overutil_thresh_h =
		(cluster_heavy_tbl[cid].max_capacity*8)/10;
	} else if (cid == (cluster_nr-1)) {
		cpu_overutil->overutil_thresh_l =
		(cluster_heavy_tbl[cid-1].max_capacity*8)/10;
		cpu_overutil->overutil_thresh_h = INT_MAX;
	} else
		WARN_ON(cid);
#endif

	/* check if task is overutilized */
	if (task_utilization(p) >= cpu_overutil->overutil_thresh_h)
		return H_OVERUTIL;
	else if (task_utilization(p) >= cpu_overutil->overutil_thresh_l)
		return L_OVERUTIL;
	else
		return NO_OVERUTIL;
}

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
	int nr_overutil_l = 0, nr_overutil_h = 0;
	unsigned long flags;
	struct overutil_stats *cpu_overutil =
		per_cpu_ptr(cpu_overutil_state, cpu);

	spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);
	nr_heavy_tasks = per_cpu(nr_heavy, cpu);
	if (nr_heavy_tasks) {
		pr_info(
		"[heavy_task] %s: nr_heavy_tasks=%d in cpu%d\n", __func__,
		nr_heavy_tasks, cpu);
		per_cpu(nr_heavy, cpu) = 0;
	}
	if (cpu_overutil->nr_overutil_l != 0 ||
		cpu_overutil->nr_overutil_h != 0) {
		pr_info("[over-utiled task] %s: nr_overutil_l=%d nr_overutil_h=%d\n",
			__func__,
			cpu_overutil->nr_overutil_l,
			cpu_overutil->nr_overutil_h);

		nr_overutil_l = cpu_overutil->nr_overutil_l;
		nr_overutil_h = cpu_overutil->nr_overutil_h;
		cpu_overutil->nr_overutil_l = 0;
		cpu_overutil->nr_overutil_h = 0;
	}
	spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);

	return nr_heavy_tasks + nr_overutil_l + nr_overutil_h;
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

void overutil_thresh_chg_notify(void)
{
	int cpu;
	unsigned long flags;
	struct task_struct *p;
	enum overutil_type_t over_type = 0;
	int nr_overutil_l, nr_overutil_h;
	struct overutil_stats *cpu_overutil;
	int cid;
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	int overutil_threshold = get_overutil_threshold();
#else
	int overutil_threshold = 1024;
#endif
	int cluster_nr = arch_get_nr_clusters();

	for_each_possible_cpu(cpu) {
		u64 curr_time = sched_clock();

		nr_overutil_l = 0;
		nr_overutil_h = 0;

#ifdef CONFIG_ARM64
		cid = cpu_topology[cpu].cluster_id;
#else
		cid = arch_get_cluster_id(cpu);
#endif
		cpu_overutil = per_cpu_ptr(cpu_overutil_state, cpu);

		raw_spin_lock_irqsave(&cpu_rq(cpu)->lock, flags); /* rq-lock */

		/* update threshold */
		spin_lock(&per_cpu(nr_heavy_lock, cpu)); /* heavy-lock */
		if (cid == 0) {
			cpu_overutil->overutil_thresh_l = INT_MAX;
			cpu_overutil->overutil_thresh_h =
				(int)(cluster_heavy_tbl[cid].max_capacity*
					overutil_threshold)/100;
		} else if (cid > 0 && cid < (cluster_nr-1)) {
			cpu_overutil->overutil_thresh_l =
				(int)(cluster_heavy_tbl[cid-1].max_capacity*
					overutil_threshold)/100;
			cpu_overutil->overutil_thresh_h =
				(int)(cluster_heavy_tbl[cid].max_capacity*
					overutil_threshold)/100;
		} else if (cid == (cluster_nr-1)) {
			cpu_overutil->overutil_thresh_l =
				(int)(cluster_heavy_tbl[cid-1].max_capacity*
					overutil_threshold)/100;
			cpu_overutil->overutil_thresh_h = INT_MAX;
		} else
			pr_info("%s: cid=%d is out of nr=%d\n", __func__,
				cid, cluster_nr);
		spin_unlock(&per_cpu(nr_heavy_lock, cpu)); /* heavy-unlock */

		/* pick next cpu if not online */
		if (!cpu_online(cpu)) {
			/* rq-unlock */
			raw_spin_unlock_irqrestore(&cpu_rq(cpu)->lock, flags);
			continue;
		}

		/* re-calculate overutil counting by updated threshold */
		list_for_each_entry(p, &cpu_rq(cpu)->cfs_tasks, se.group_node) {
			over_type = is_task_overutil(p);
			if (over_type) {
				if (over_type == H_OVERUTIL) {
					nr_overutil_l++;
					nr_overutil_h++;
				} else {
					nr_overutil_l++;
				}
			}
		}

		/* Threshold for heavy is changed. Need to reset stats */
		spin_lock(&per_cpu(nr_heavy_lock, cpu)); /* heavy-lock */
		cpu_overutil->nr_overutil_h = nr_overutil_h;
		cpu_overutil->nr_overutil_h_prod_sum = 0;
		cpu_overutil->h_last_update_time = curr_time;

		cpu_overutil->nr_overutil_l = nr_overutil_l;
		cpu_overutil->nr_overutil_l_prod_sum = 0;
		cpu_overutil->l_last_update_time = curr_time;
		spin_unlock(&per_cpu(nr_heavy_lock, cpu)); /* heavy-unlock */

		/* rq-unlock */
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

/* sched_get_cluster_util:
 *
 * returns: 0 if success
 *
 * usage: the amount of capacity of a cluster that is used by CFS tasks.
 * capacity: the max capacity of this cluster
 */
int sched_get_cluster_util(int cluster_id,
			unsigned long *usage, unsigned long *capacity)
{
	int cluster_nr;
	int cpu;
	struct cpumask cls_cpus;

	/* cluster_id  need reasonale. */
	cluster_nr = arch_get_nr_clusters();
	if (cluster_id < 0 || cluster_id >= cluster_nr)
		return -1;

	/* initialized  */
	*usage = *capacity = 0;

	arch_get_cluster_cpus(&cls_cpus, cluster_id);

	 /* visit all cpus of this cluster */
	for_each_cpu(cpu, &cls_cpus) {
		/*
		 * cpu_util returns the amount of capacity of
		 * a CPU that is used by CFS tasks
		 */
		*capacity += capacity_orig_of(cpu);

		if (!cpu_online(cpu))
			continue;

		*usage += boosted_cpu_util(cpu, 0);
	}


	return 0;
}
EXPORT_SYMBOL(sched_get_cluster_util);


int sched_get_nr_overutil_avg(int cluster_id, int *l_avg, int *h_avg)
{
	u64 curr_time = sched_clock();
	s64 diff;
	u64 l_tmp_avg = 0, h_tmp_avg = 0;
	u32 cpumask = 0;
	bool clk_faulty = 0;
	unsigned long flags;
	int cpu = 0;
	int cluster_nr;
	struct cpumask cls_cpus;

	/* Need to make sure initialization done. */
	if (!init_heavy) {
		*l_avg = *h_avg = 0;
		return -1;
	}

	/* cluster_id  need reasonale. */
	cluster_nr = arch_get_nr_clusters();
	if (cluster_id < 0 || cluster_id >= cluster_nr) {
		pr_info("[%s] invalid cluster id %d\n", __func__, cluster_id);
		return -1;
	}

	/* Time diff can't be zero/negative. */
	diff = (s64)(curr_time -
			cluster_heavy_tbl[cluster_id].last_get_overutil_time);
	if (diff <= 0) {
		*l_avg = *h_avg = 0;
		return -1;
	}

	arch_get_cluster_cpus(&cls_cpus, cluster_id);
	cluster_heavy_tbl[cluster_id].last_get_overutil_time = curr_time;

	/* visit all cpus of this cluster */
	for_each_cpu(cpu, &cls_cpus) {
		struct overutil_stats *cpu_overutil;

		spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);

		cpu_overutil = per_cpu_ptr(cpu_overutil_state, cpu);

		if ((s64) (curr_time - cpu_overutil->l_last_update_time < 0)) {
			clk_faulty = 1;
			cpumask |= 1 << cpu;
		}

		/* get prod sum */
		l_tmp_avg += cpu_overutil->nr_overutil_l_prod_sum;
		l_tmp_avg += cpu_overutil->nr_overutil_l *
			(curr_time - cpu_overutil->l_last_update_time);

		h_tmp_avg += cpu_overutil->nr_overutil_h_prod_sum;
		h_tmp_avg += cpu_overutil->nr_overutil_h *
			(curr_time - cpu_overutil->h_last_update_time);

		/* update last update time */
		cpu_overutil->l_last_update_time = curr_time;
		cpu_overutil->h_last_update_time = curr_time;

		/* clear prod_sum */
		cpu_overutil->nr_overutil_l_prod_sum = 0;
		cpu_overutil->nr_overutil_h_prod_sum = 0;

		spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);
	}

	if (clk_faulty) {
		l_avg = h_avg = 0;
		return -1;
	}

	*l_avg = (int)div64_u64(l_tmp_avg * 100, (u64) diff);
	*h_avg = (int)div64_u64(h_tmp_avg * 100, (u64) diff);

	return 0;
}
EXPORT_SYMBOL(sched_get_nr_overutil_avg);

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

int get_overutil_stats(char *buf, int buf_size)
{
	int cpu;
	int len = 0;
	struct overutil_stats *cpu_overutil;

	for_each_possible_cpu(cpu) {
		cpu_overutil = per_cpu_ptr(cpu_overutil_state, cpu);

		len += snprintf(buf+len, buf_size-len,
			"cpu=%d capacity=%lu overutil_l=%d overutil_h=%d\n",
			cpu, cpu_rq(cpu)->cpu_capacity_orig,
			cpu_overutil->overutil_thresh_l,
			cpu_overutil->overutil_thresh_h);
	}

	for_each_possible_cpu(cpu) {
		cpu_overutil = per_cpu_ptr(cpu_overutil_state, cpu);

		len += snprintf(buf+len, buf_size-len,
			"cpu=%d nr_overutil_l=%d nr_overutil_h=%d\n",
			cpu, cpu_overutil->nr_overutil_l,
			cpu_overutil->nr_overutil_h);
	}

	return len;
}

void sched_update_nr_heavy_prod(const char *invoker, struct task_struct *p,
	int cpu, int heavy_nr_inc, bool ack_cap_req)
{
	s64 diff;
	u64 curr_time;
	unsigned long flags;
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	unsigned long prev_heavy_nr;
	int ack_cap = -1;
#endif
	enum overutil_type_t over_type = 0;

	if (!init_heavy) {
		init_heavy_tlb();
		if (!init_heavy) {
			WARN_ON(!init_heavy);
			return;
		}
	}

	spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);

	curr_time = sched_clock();
	over_type = is_task_overutil(p);

	if (over_type) {
		struct overutil_stats *cpu_overutil;

		cpu_overutil = per_cpu_ptr(cpu_overutil_state, cpu);

		if (over_type == H_OVERUTIL) {
			/* H_OVERUTIL */
			diff = (s64) (curr_time -
					cpu_overutil->l_last_update_time);
			/* update overutil for degrading threshold */
			if (diff >= 0) {
				cpu_overutil->l_last_update_time = curr_time;
				cpu_overutil->nr_overutil_l_prod_sum +=
					cpu_overutil->nr_overutil_l*diff;
				cpu_overutil->nr_overutil_l +=
					heavy_nr_inc;
			}

			diff = (s64) (curr_time -
					cpu_overutil->h_last_update_time);
			/* update overutil for upgrading threshold */
			if (diff >= 0) {
				cpu_overutil->h_last_update_time = curr_time;
				cpu_overutil->nr_overutil_h_prod_sum +=
					cpu_overutil->nr_overutil_h*diff;
				cpu_overutil->nr_overutil_h +=
					heavy_nr_inc;
			}
		} else {/* L_OVERUTIL */
			diff = (s64) (curr_time -
				cpu_overutil->l_last_update_time);
			if (diff >= 0) {
				cpu_overutil->l_last_update_time = curr_time;
				cpu_overutil->nr_overutil_l_prod_sum +=
					cpu_overutil->nr_overutil_l*diff;
				cpu_overutil->nr_overutil_l +=
					heavy_nr_inc;
			}
		}
	}
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	if (is_heavy_task(p)) {
		/* for heavy task avg */
		prev_heavy_nr = per_cpu(nr_heavy, cpu);
		per_cpu(nr_heavy, cpu) = prev_heavy_nr + heavy_nr_inc;
		/* WARN_ON((int)per_cpu(nr_heavy, cpu) < 0); */
		diff = (s64) (curr_time - per_cpu(last_heavy_time, cpu));
		if (diff < 0)
			goto OUT;

		per_cpu(last_heavy_time, cpu) = curr_time;

		/* for current opp control */
		if (ack_cap_req) {
			ack_cap = is_ack_curcap(cpu);
			per_cpu(nr_heavy_prod_sum, cpu) +=
				prev_heavy_nr*diff*ack_cap;
			trace_sched_avg_heavy_nr(invoker, prev_heavy_nr,
				diff, ack_cap, cpu);
		} else {
			ack_cap = -1;
			per_cpu(nr_heavy_prod_sum, cpu) +=
				prev_heavy_nr*diff;
			trace_sched_avg_heavy_nr(invoker, prev_heavy_nr,
				diff, ack_cap, cpu);
		}
	}
OUT:
#endif

	spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_heavy_prod);

static int init_heavy_tlb(void)
{
	if (!init_heavy) {
		/* init variables */
		int tmp_cpu, cluster_nr;
		int i;
		int cid;
		struct cpumask cls_cpus;
		struct overutil_stats *cpu_overutil;
#ifdef CONFIG_MTK_SCHED_RQAVG_US
		int overutil_threshold = get_overutil_threshold();
#else
		int overutil_threshold = 1024;
#endif
		pr_info("%s start.\n", __func__);

		/* allocation for overutilization statistics */
		cpu_overutil_state =
			alloc_percpu_gfp(struct overutil_stats, GFP_ATOMIC);
		if (!cpu_overutil_state)
			return 0;

		/* allocation for clustser information */
		cluster_nr = arch_get_nr_clusters();
		if (cluster_nr <= 0)
			return 0;
		cluster_heavy_tbl = kcalloc(cluster_nr,
			sizeof(struct cluster_heavy_tbl_t), GFP_ATOMIC);
		if (!cluster_heavy_tbl)
			return 0;

		for (i = 0; i < cluster_nr; i++) {
			arch_get_cluster_cpus(&cls_cpus, i);
			tmp_cpu = cpumask_first(&cls_cpus);
			/* replace cpu_rq(cpu)->cpu_capacity_orig by
			 * get_cpu_orig_capacity()
			 */
			cluster_heavy_tbl[i].max_capacity =
				capacity_orig_of(tmp_cpu);
		}

		for_each_possible_cpu(tmp_cpu) {
			cpu_overutil = per_cpu_ptr(cpu_overutil_state, tmp_cpu);
#ifdef CONFIG_ARM64
			cid = cpu_topology[tmp_cpu].cluster_id;
#else
			cid = arch_get_cluster_id(tmp_cpu);
#endif

			/* reset nr_heavy */
			per_cpu(nr_heavy, tmp_cpu) = 0;

			/* reset nr_overutil */
			cpu_overutil->nr_overutil_l = 0;
			cpu_overutil->nr_overutil_h = 0;

			/* apply threshold for over-utilization tracking */
			if (cid == 0) {
				cpu_overutil->overutil_thresh_l = INT_MAX;
				cpu_overutil->overutil_thresh_h =
				(int)(cluster_heavy_tbl[cid].max_capacity*
					overutil_threshold)/100;
			} else if (cid > 0 && cid < (cluster_nr-1)) {
				cpu_overutil->overutil_thresh_l =
				(int)(cluster_heavy_tbl[cid-1].max_capacity*
					overutil_threshold)/100;
				cpu_overutil->overutil_thresh_h =
				(int)(cluster_heavy_tbl[cid].max_capacity*
					overutil_threshold)/100;
			} else if (cid == (cluster_nr-1)) {
				cpu_overutil->overutil_thresh_l =
				(int)(cluster_heavy_tbl[cid-1].max_capacity*
					overutil_threshold)/100;
				cpu_overutil->overutil_thresh_h = INT_MAX;
			} else
				pr_info("%s: cid=%d is out of nr=%d\n",
					__func__, cid, cluster_nr);

			pr_info("%s: cpu=%d thresh_l=%d thresh_h=%d max_capaicy=%lu\n",
				__func__, tmp_cpu,
				cpu_overutil->overutil_thresh_l,
				cpu_overutil->overutil_thresh_h,
				(unsigned long int)
				cluster_heavy_tbl[cid].max_capacity);
		}

		init_heavy = 1;
	}

	return init_heavy;
}

