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
#include <linux/types.h>
#include <linux/sched/clock.h>
#include <linux/topology.h>
#include <linux/arch_topology.h>
#include <trace/events/sched.h>
//TODO: remove comment after met ready
//#include <mt-plat/mt_sched.h>
#include "rq_stats.h"

// TODO: remove comment after MET ready
//#include <mt-plat/met_drv.h>

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

struct overutil_stats_t {
	int nr_overutil_l;
	int nr_overutil_h;
	int overutil_thresh_l;
	int overutil_thresh_h;
	u64 nr_overutil_l_prod_sum;
	u64 nr_overutil_h_prod_sum;
	u64 l_last_update_time;
	u64 h_last_update_time;
	int max_task_util;
	int max_boost_util;
	int max_task_pid;
};

static DEFINE_PER_CPU(struct overutil_stats_t, cpu_overutil_state);

struct cluster_heavy_tbl_t {
	u64 last_get_heavy_time;
	u64 last_get_overutil_time;
	u64 max_capacity;
};

struct cluster_heavy_tbl_t *cluster_heavy_tbl;

static int init_heavy_tlb(void);

/*
 * Big Task Tracking:
 * help hotplug to enable high-capacity CPU by big task.
 */
#define MAX_CLUSTER_NR 3
#define MAX_CPU_CLUSTER 4
#define BIG_TASK_BOOSTED_THRESHOLD 150
#define BIG_TASK_GAME_THRESHOLD 80
#define BIG_TASK_AVG_THRESHOLD 25

static int btask_list_l[MAX_CLUSTER_NR][MAX_CPU_CLUSTER];
static int btask_list_h[MAX_CLUSTER_NR][MAX_CPU_CLUSTER];

static inline
void tracking_btask_nr(int pid, int cid, bool high)
{
	int i;
	int *btask_list = (high) ? btask_list_h[cid] : btask_list_l[cid];

	for (i = 0; i < MAX_CPU_CLUSTER; i++) {
		if (btask_list[i] == pid)
			break;

		if (!btask_list[i]) {
			btask_list[i] = pid;
			break;
		}
	}
}

static inline
int get_btask_nr(int cid, bool high)
{
	int *btask_list = (high) ? btask_list_h[cid] : btask_list_l[cid];
	int i;
	int nr = 0;

	for (i = 0; i < MAX_CPU_CLUSTER; i++) {
		if (btask_list[i])
			nr++;
	}

	return nr;
}

static inline
void reset_btask_nr(void)
{
	int i;

	for (i = 0; i < MAX_CLUSTER_NR; i++) {
		memset(btask_list_l[i], 0, sizeof(btask_list_l[i]));
		memset(btask_list_h[i], 0, sizeof(btask_list_l[i]));
	}
}

int show_btask(char *buf, int buf_size)
{
	int *btask_list;
	int i, j;
	int len = 0;
	struct task_struct *p;

	for (i = 0; i < MAX_CLUSTER_NR; i++) {
		btask_list = btask_list_h[i];

		for (j = 0; j < MAX_CPU_CLUSTER; j++) {
			p = find_task_by_vpid(btask_list[j]);

			len += snprintf(
			buf+len,
			buf_size-len,
			"H.[%d][%d]=%d %s(%lu)\n", i, j, btask_list[j],
			(btask_list[j] && p) ? p->comm : "NULL",
			(btask_list[j] && p) ? p->se.avg.util_avg : 0UL);
		}
	}

	for (i = 0; i < MAX_CLUSTER_NR; i++) {
		btask_list = btask_list_l[i];

		for (j = 0; j < MAX_CPU_CLUSTER; j++) {
			p = find_task_by_vpid(btask_list[j]);

			len += snprintf(
			buf+len,
			buf_size-len,
			"L.[%d][%d]=%d %s(%lu)\n", i, j, btask_list[j],
			(btask_list[j] && p) ? p->comm : "NULL",
			(btask_list[j] && p) ? p->se.avg.util_avg : 0UL);
		}
	}

	return len;
}

enum overutil_type_t is_task_overutil(struct task_struct *p)
{
	struct overutil_stats_t *cpu_overutil;
	/* int cid; */
	/* int cluster_nr = arch_get_nr_clusters(); */

	unsigned long task_util;
	int cpu, cid;
	unsigned long boosted_task_util;

	if (!p)
		return NO_OVERUTIL;

	cpu = cpu_of(task_rq(p));

#ifdef CONFIG_ARM64
	cid = cpu_topology[cpu].cluster_id;
#else
	cid = cpu_topology[cpu].socket_id;
#endif


	get_task_util(p, &task_util, &boosted_task_util);

	cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

	/* track task with max utilization */
	if (task_util > cpu_overutil->max_task_util) {
		cpu_overutil->max_task_util = task_util;
		cpu_overutil->max_boost_util = boosted_task_util;
		cpu_overutil->max_task_pid = p->pid;
	}

	/* check if task is overutilized */
	if (task_util >= cpu_overutil->overutil_thresh_h) {
		tracking_btask_nr(p->pid, cid, true); /* big task */
		return H_OVERUTIL;
	} else if (task_util >= cpu_overutil->overutil_thresh_l) {
		tracking_btask_nr(p->pid, cid, false); /* big task */
		return L_OVERUTIL;
	} else
		return NO_OVERUTIL;
}

#define MAX_UTIL_TRACKER_PERIODIC_MS 32
static int gb_task_util;
static int gb_task_pid;
static int gb_task_cpu;
static int gb_boosted_util;

static DEFINE_SPINLOCK(gb_max_util_lock);
void (*fpsgo_sched_nominate_fp)(pid_t *id, int *utl);

void sched_max_util_task_tracking(void)
{
	int cpu;
	struct overutil_stats_t *cpu_overutil;
	int max_util = 0;
	int boost_util = 0;
	int max_cpu = 0;
	int max_task_pid = 0;
	ktime_t now = ktime_get();
	unsigned long flag;
	static ktime_t max_util_tracker_last_update;
	pid_t tasks[NR_CPUS] = {0};
	int   utils[NR_CPUS] = {0};

	spin_lock_irqsave(&gb_max_util_lock, flag);

	/* periodic: 32ms */
	if (ktime_before(now, ktime_add_ms(
		max_util_tracker_last_update, MAX_UTIL_TRACKER_PERIODIC_MS))) {
		spin_unlock_irqrestore(&gb_max_util_lock, flag);
		return;
	}

	/* update last update time for tracker */
	max_util_tracker_last_update = now;

	spin_unlock_irqrestore(&gb_max_util_lock, flag);

	for_each_possible_cpu(cpu) {
		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

		if (cpu_online(cpu) &&
			(cpu_overutil->max_task_util > max_util)) {
			max_util = cpu_overutil->max_task_util;
			boost_util = cpu_overutil->max_boost_util;
			max_cpu = cpu;
			max_task_pid = cpu_overutil->max_task_pid;
		}

		if (fpsgo_sched_nominate_fp) {
			tasks[cpu] = cpu_overutil->max_task_pid;
			utils[cpu] = cpu_overutil->max_task_util;
		}

		cpu_overutil->max_task_util = 0;
		cpu_overutil->max_boost_util = 0;
		cpu_overutil->max_task_pid = 0;
	}

	gb_task_util = max_util;
	gb_boosted_util = boost_util;
	gb_task_pid = max_task_pid;
	gb_task_cpu = max_cpu;

	if (fpsgo_sched_nominate_fp)
		fpsgo_sched_nominate_fp(&tasks[0], &utils[0]);

#if defined(MET_SCHED_DEBUG) && MET_SCHED_DEBUG
	met_tag_oneshot(0, "sched_max_task_util", max_util);
	met_tag_oneshot(0, "sched_boost_task_util", boost_util);
#endif
}

void sched_max_util_task(int *cpu, int *pid, int *util, int *boost)
{
	if (cpu)
		*cpu = gb_task_cpu;
	if (pid)
		*pid = gb_task_pid;
	if (util)
		*util = gb_task_util;
	if (boost)
		*boost = gb_boosted_util;
}
EXPORT_SYMBOL(sched_max_util_task);

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
	if (diff < 0)
		printk_deferred("[%s] time last:%llu curr:%llu ",
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
		printk_deferred("[%s] **** CPU (0x%08x)clock may unstable !!\n",
		__func__, cpumask);
		return 0;
	}

	*avg = (int)div64_u64(tmp_avg * 100, (u64) diff);
	*iowait_avg = (int)div64_u64(tmp_iowait * 100, (u64) diff);

	if (unlikely(*avg < 0)) {
		printk_deferred("[%s] avg:%d(%llu/%lld), time last:%llu curr:%llu ",
		__func__,
		*avg, tmp_avg, diff, old_lgt, curr_time);
		*avg = 0;
	}
	if (unlikely(*iowait_avg < 0)) {
		printk_deferred("[%s] iowait_avg:%d(%llu/%lld) time last:%llu curr:%llu ",
		__func__,
		*iowait_avg, tmp_iowait, diff, old_lgt, curr_time);
		*iowait_avg = 0;
	}

	return scaled_tlp*100;
}
EXPORT_SYMBOL(sched_get_nr_running_avg);

int reset_heavy_task_stats(int cpu)
{
	int nr_heavy_tasks;
	int nr_overutil_l = 0, nr_overutil_h = 0;
	unsigned long flags;
	struct overutil_stats_t *cpu_overutil =
		&per_cpu(cpu_overutil_state, cpu);

	spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);
	nr_heavy_tasks = per_cpu(nr_heavy, cpu);
	if (nr_heavy_tasks) {
		printk_deferred(
		"[heavy_task] %s: nr_heavy_tasks=%d in cpu%d\n", __func__,
		nr_heavy_tasks, cpu);
		per_cpu(nr_heavy, cpu) = 0;
	}
	if (cpu_overutil->nr_overutil_l != 0 ||
		cpu_overutil->nr_overutil_h != 0) {
		printk_deferred(
			"[over-utiled task] %s: nr_overutil_l=%d nr_overutil_h=%d\n",
			__func__,
			cpu_overutil->nr_overutil_l,
			cpu_overutil->nr_overutil_h);

		nr_overutil_l = cpu_overutil->nr_overutil_l;
		nr_overutil_h = cpu_overutil->nr_overutil_h;
		cpu_overutil->nr_overutil_l = 0;
		cpu_overutil->nr_overutil_h = 0;
	}

	cpu_overutil->max_task_util = 0;
	cpu_overutil->max_task_pid = 0;

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
	enum overutil_type_t over_type = NO_OVERUTIL;
	int nr_overutil_l, nr_overutil_h;
	struct overutil_stats_t *cpu_overutil;
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
		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

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
			printk_deferred("%s: cid=%d is out of nr=%d\n",
			__func__,
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
		printk_deferred("[%s] invalid cluster id %d\n",
			__func__, cluster_id);
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

		trace_sched_avg_heavy_nr(5, per_cpu(nr_heavy, cpu),
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
	unsigned long cpu_cap = 0;
	int cpu_nr = 0;

	/* initialized */
	if (usage)
		*usage = 0;
	if (capacity)
		*capacity = 0;

	/* cluster_id  need reasonale. */
	cluster_nr = arch_get_nr_clusters();
	if (cluster_id < 0 || cluster_id >= cluster_nr)
		return -1;

	arch_get_cluster_cpus(&cls_cpus, cluster_id);

	 /* visit all cpus of this cluster */
	for_each_cpu(cpu, &cls_cpus) {
		/*
		 * cpu_util returns the amount of capacity of
		 * a CPU that is used by CFS tasks
		 */
		cpu_nr++;

		if (!cpu_online(cpu))
			continue;

		cpu_cap = capacity_orig_of(cpu);

		if (usage)
			*usage += boosted_cpu_util(cpu, 0);
	}

	if (capacity)
		*capacity = cpu_cap*cpu_nr;

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
		printk_deferred("[%s] invalid cluster id %d\n",
		__func__, cluster_id);
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
		struct overutil_stats_t *cpu_overutil;

		spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);

		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

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

/*
 * sched_big_task_nr:
 * B_nr: big task nr.
 * L_nr: task suggested in L.
 */
#define MAX_CLUSTER_NR 3
#define BIG_TASK_BOOSTED_THRESHOLD 150
#define BIG_TASK_GAME_THRESHOLD 80

#if defined(MET_SCHED_DEBUG) && MET_SCHED_DEBUG
static char met_log_info[10][32] = {
	"sched_bt_overavg_0L",
	"sched_bt_overavg_0H",
	"sched_bt_overavg_1L",
	"sched_bt_overavg_1H",
	"sched_bt_overavg_2L",
	"sched_bt_overavg_2H",
};

static char met_log_info2[10][32] = {
	"sched_bt_nr_0L",
	"sched_bt_nr_0H",
	"sched_bt_nr_1L",
	"sched_bt_nr_1H",
	"sched_bt_nr_2L",
	"sched_bt_nr_2H",
};
#endif

void sched_big_task_nr(int *L_nr, int *B_nr)
{
	int l_avg, b_avg;
	int l_nr, b_nr;
	int i;
	int l_avg_time[MAX_CLUSTER_NR] = {0};
	int h_avg_time[MAX_CLUSTER_NR] = {0};
	int _l_nr[MAX_CLUSTER_NR] = {0};
	int _h_nr[MAX_CLUSTER_NR] = {0};
	int cluster_nr = arch_get_nr_clusters();

	*B_nr = *L_nr = 0;

	/* to get big task nr per cluster */
	for (i = 0; i < cluster_nr; i++) {
		sched_get_nr_overutil_avg(i, &l_avg_time[i], &h_avg_time[i]);
		_l_nr[i] = get_btask_nr(i, false);
		_h_nr[i] = get_btask_nr(i, true);
#if defined(MET_SCHED_DEBUG) && MET_SCHED_DEBUG
		met_tag_oneshot(0, met_log_info[i*2],   l_avg_time[i]);
		met_tag_oneshot(0, met_log_info[i*2+1], h_avg_time[i]);

		met_tag_oneshot(0, met_log_info2[i*2],   _l_nr[i]);
		met_tag_oneshot(0, met_log_info2[i*2+1], _h_nr[i]);
#endif
	}

	/*
	 * basic algorithm of big task:
	 *
	 * A heavy task (bigger than threshold) running 25% average time,
	 * that we call big task.
	 *
	 */
	l_avg = h_avg_time[0] + (l_avg_time[1] - h_avg_time[1]);
	b_avg = h_avg_time[1] + l_avg_time[2];

	l_nr = _h_nr[0] + _h_nr[1];
	b_nr = _h_nr[1] + _l_nr[2];

	/* big core nr */
	if (_l_nr[2]) {
		if ((l_avg_time[2]/_l_nr[2]) > BIG_TASK_AVG_THRESHOLD)
			*B_nr += _l_nr[2];
		else
			*B_nr +=
			((l_avg_time[2]/BIG_TASK_AVG_THRESHOLD) >= _l_nr[2]) ?
			_l_nr[2] : (l_avg_time[2]/BIG_TASK_AVG_THRESHOLD);
	}

	if (_h_nr[1]) {
		if ((h_avg_time[1]/_h_nr[1]) > BIG_TASK_AVG_THRESHOLD)
			*B_nr += _h_nr[1];
		else
			*B_nr +=
			((h_avg_time[1]/BIG_TASK_AVG_THRESHOLD) >= _h_nr[1]) ?
			_h_nr[1] : (h_avg_time[1]/BIG_TASK_AVG_THRESHOLD);
	}

	/* L core nr */
	if (_l_nr[1]) {
		if (((l_avg_time[1] - h_avg_time[1])/_l_nr[1]) >
							BIG_TASK_AVG_THRESHOLD)
			*L_nr += _l_nr[1];
		else
			*L_nr +=
			(((l_avg_time[1] - h_avg_time[1])/
			BIG_TASK_AVG_THRESHOLD) >= _l_nr[1]) ?
			_l_nr[1] : ((l_avg_time[1] - h_avg_time[1])/
			BIG_TASK_AVG_THRESHOLD);
	}

	if (_h_nr[0]) {
		if ((h_avg_time[0]/_h_nr[0]) > BIG_TASK_AVG_THRESHOLD)
			*L_nr += _h_nr[0];
		else
			*L_nr +=
			((h_avg_time[0]/BIG_TASK_AVG_THRESHOLD) >= _h_nr[0]) ?
			_h_nr[0] : (h_avg_time[0]/BIG_TASK_AVG_THRESHOLD);
	}

	/* if big core needed for performance */
	if (!(*B_nr)) {
		/* To consider boosted util of stune */
		int boosted;
		int util;

		sched_max_util_task(NULL, NULL, &util, &boosted);

		/* how to quantify it???? */
		if (boosted > BIG_TASK_BOOSTED_THRESHOLD)
			*B_nr = 1;
	}

	if (cluster_nr < 3) { /* no big core */
		*L_nr += *B_nr;
		*B_nr = 0;
	}

	/* reset big task tracking */
	reset_btask_nr();

#if defined(MET_SCHED_DEBUG) && MET_SCHED_DEBUG
	met_tag_oneshot(0, "sched_bt_b_avg", b_avg);
	met_tag_oneshot(0, "sched_bt_l_avg", l_avg);
	met_tag_oneshot(0, "sched_bt_b_nr", *B_nr);
	met_tag_oneshot(0, "sched_bt_l_nr", *L_nr);
#endif
}
EXPORT_SYMBOL(sched_big_task_nr);

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

	if (per_cpu(nr, cpu) < 0)
		printk_deferred("assertion failed at %s:%d\n",
		__FILE__,
		__LINE__);

	per_cpu(nr_prod_sum, cpu) += nr_running * diff;
	per_cpu(iowait_prod_sum, cpu) += nr_iowait_cpu(cpu) * diff;

	spin_unlock_irqrestore(&per_cpu(nr_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_prod);

int get_overutil_stats(char *buf, int buf_size)
{
	int cpu;
	int len = 0;
	struct overutil_stats_t *cpu_overutil;

	for_each_possible_cpu(cpu) {
		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

		len += snprintf(buf+len, buf_size-len,
			"cpu=%d capacity=%lu overutil_l=%d overutil_h=%d\n",
			cpu, cpu_rq(cpu)->cpu_capacity_orig,
			cpu_overutil->overutil_thresh_l,
			cpu_overutil->overutil_thresh_h);
	}

	for_each_possible_cpu(cpu) {
		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

		len += snprintf(buf+len, buf_size-len,
			"cpu=%d nr_overutil_l=%d nr_overutil_h=%d\n",
			cpu, cpu_overutil->nr_overutil_l,
			cpu_overutil->nr_overutil_h);
	}

	return len;
}

void sched_update_nr_heavy_prod(int invoker, struct task_struct *p,
	int cpu, int heavy_nr_inc, bool ack_cap_req)
{
	s64 diff;
	u64 curr_time;
	unsigned long flags;
#ifdef CONFIG_MTK_SCHED_RQAVG_US
	unsigned long prev_heavy_nr;
	int ack_cap = -1;
#endif
	enum overutil_type_t over_type = NO_OVERUTIL;

	if (!init_heavy) {
		init_heavy_tlb();
		if (!init_heavy) {
			printk_deferred("assertion failed at %s:%d\n",
			__FILE__,
			__LINE__);
			return;
		}
	}

	spin_lock_irqsave(&per_cpu(nr_heavy_lock, cpu), flags);

	curr_time = sched_clock();
	over_type = is_task_overutil(p);

	if (over_type) {
		struct overutil_stats_t *cpu_overutil;

		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

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
		struct overutil_stats_t *cpu_overutil;
#ifdef CONFIG_MTK_SCHED_RQAVG_US
		int overutil_threshold = get_overutil_threshold();
#else
		int overutil_threshold = 1024;
#endif
		printk_deferred("%s start.\n", __func__);

		gb_task_util = 0;
		gb_task_pid = 0;
		gb_task_cpu = 0;

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
			cpu_overutil = &per_cpu(cpu_overutil_state, tmp_cpu);
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
			cpu_overutil->max_task_util = 0;
			cpu_overutil->max_task_pid = 0;

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
				printk_deferred("%s: cid=%d is out of nr=%d\n",
					__func__, cid, cluster_nr);

			printk_deferred("%s: cpu=%d thresh_l=%d thresh_h=%d max_capaicy=%lu\n",
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

