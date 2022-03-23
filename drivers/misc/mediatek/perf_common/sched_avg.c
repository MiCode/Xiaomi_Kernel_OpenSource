// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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

#include <rq_stats.h>

enum overutil_type_t {
	NO_OVERUTIL = 0,
	L_OVERUTIL,
	H_OVERUTIL
};

static DEFINE_PER_CPU(u64, last_time);
static DEFINE_PER_CPU(u64, nr);
static DEFINE_PER_CPU(u64, nr_heavy);
#ifdef CONFIG_MTK_CORE_CTL
static DEFINE_PER_CPU(u64, nr_max);
#endif
static DEFINE_PER_CPU(spinlock_t, nr_lock) = __SPIN_LOCK_UNLOCKED(nr_lock);
static DEFINE_PER_CPU(spinlock_t, nr_heavy_lock) =
			__SPIN_LOCK_UNLOCKED(nr_heavy_lock);
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

static int arch_get_nr_clusters(void)
{
	return arch_nr_clusters();
}

static int arch_get_cluster_id(unsigned int cpu)
{
	return arch_cpu_cluster_id(cpu);
}

enum overutil_type_t is_task_overutil(struct task_struct *p)
{
	struct overutil_stats_t *cpu_overutil;
	unsigned long task_util;
	int cpu, cid;
	int cluster_nr = arch_get_nr_clusters();

	if (!p)
		return NO_OVERUTIL;

	cpu = cpu_of(task_rq(p));
	cid = arch_get_cluster_id(cpu);

	if (cid < 0 || cid >= cluster_nr) {
		printk_deferred_once("[%s] invalid cluster id %d\n",
			__func__, cid);
		return NO_OVERUTIL;
	}

	get_task_util(p, &task_util);

	cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

	/* track task with max utilization */
	if (task_util > cpu_overutil->max_task_util) {
		cpu_overutil->max_task_util = task_util;
		cpu_overutil->max_task_pid = p->pid;
	}

	/* check if task is overutilized */
	if (task_util >= cpu_overutil->overutil_thresh_h)
		return H_OVERUTIL;
	else if (task_util >= cpu_overutil->overutil_thresh_l)
		return L_OVERUTIL;
	else
		return NO_OVERUTIL;
}

#ifdef CONFIG_MTK_CORE_CTL
#define MAX_UTIL_TRACKER_PERIODIC_MS 8
static unsigned int fpsgo_update_max_util_windows;
#else
#define MAX_UTIL_TRACKER_PERIODIC_MS 32
#endif
static int gb_task_util;
static int gb_task_pid;
static int gb_task_cpu;

static DEFINE_SPINLOCK(gb_max_util_lock);
void (*fpsgo_sched_nominate_fp)(pid_t *id, int *utl);

void sched_max_util_task_tracking(void)
{
	int cpu;
	struct overutil_stats_t *cpu_overutil;
	int max_util = 0;
	int max_cpu = 0;
	int max_task_pid = 0;
	ktime_t now = ktime_get();
	unsigned long flag;
	static ktime_t max_util_tracker_last_update;
	pid_t tasks[NR_CPUS] = {0};
	int   utils[NR_CPUS] = {0};
#ifdef CONFIG_MTK_CORE_CTL
	bool skip_mintop = false;
#endif

	spin_lock_irqsave(&gb_max_util_lock, flag);

	if (ktime_before(now, ktime_add_ms(
		max_util_tracker_last_update, MAX_UTIL_TRACKER_PERIODIC_MS))) {
		spin_unlock_irqrestore(&gb_max_util_lock, flag);
		return;
	}

	/* update last update time for tracker */
	max_util_tracker_last_update = now;

#ifdef CONFIG_MTK_CORE_CTL
	fpsgo_update_max_util_windows += 1;

	/* periodic: 32ms */
	if (fpsgo_update_max_util_windows < 4)
		skip_mintop = true;
	else
		fpsgo_update_max_util_windows = 0;

#endif
	spin_unlock_irqrestore(&gb_max_util_lock, flag);

	for_each_possible_cpu(cpu) {
		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

		if (cpu_online(cpu) &&
			(cpu_overutil->max_task_util > max_util)) {
			max_util = cpu_overutil->max_task_util;
			max_cpu = cpu;
			max_task_pid = cpu_overutil->max_task_pid;
		}

		if (fpsgo_sched_nominate_fp) {
			tasks[cpu] = cpu_overutil->max_task_pid;
			utils[cpu] = cpu_overutil->max_task_util;
		}

		cpu_overutil->max_task_util = 0;
		cpu_overutil->max_task_pid = 0;
	}

	gb_task_util = max_util;
	gb_task_pid = max_task_pid;
	gb_task_cpu = max_cpu;

#ifdef CONFIG_MTK_CORE_CTL
	if (skip_mintop)
		return;
#endif

	if (fpsgo_sched_nominate_fp)
		fpsgo_sched_nominate_fp(&tasks[0], &utils[0]);
}

void sched_max_util_task(int *cpu, int *pid, int *util, int *boost)
{
	if (cpu)
		*cpu = gb_task_cpu;
	if (pid)
		*pid = gb_task_pid;
	if (util)
		*util = gb_task_util;
}
EXPORT_SYMBOL(sched_max_util_task);

// sched_avg.c

void overutil_thresh_chg_notify(void)
{
	int cpu;
	unsigned long flags;
	struct task_struct *p;
	enum overutil_type_t over_type = NO_OVERUTIL;
	int nr_overutil_l, nr_overutil_h;
	struct overutil_stats_t *cpu_overutil;
	int cid;
	unsigned int overutil_threshold = 100;
	int cluster_nr = arch_nr_clusters();

	for_each_possible_cpu(cpu) {
		u64 curr_time = sched_clock();

		nr_overutil_l = 0;
		nr_overutil_h = 0;

		cid = arch_get_cluster_id(cpu);
		cpu_overutil = &per_cpu(cpu_overutil_state, cpu);

		if (cid < 0 || cid >= cluster_nr) {
			printk_deferred_once("%s: cid=%d is out of nr=%d\n",
			__func__, cid, cluster_nr);
			continue;
		}

		raw_spin_lock_irqsave(&cpu_rq(cpu)->lock, flags); /* rq-lock */
		/* update threshold */
		spin_lock(&per_cpu(nr_heavy_lock, cpu)); /* heavy-lock */

		if (cid == 0)
			cpu_overutil->overutil_thresh_l = INT_MAX;
		else {
			overutil_threshold = get_overutil_threshold(cid-1);
			cpu_overutil->overutil_thresh_l =
				(int)(cluster_heavy_tbl[cluster_nr-1].max_capacity*
					overutil_threshold)/100;
		}

		if (cid == cluster_nr-1)
			cpu_overutil->overutil_thresh_h = INT_MAX;
		else {
			overutil_threshold = get_overutil_threshold(cid);
			cpu_overutil->overutil_thresh_h =
				(int)(cluster_heavy_tbl[cluster_nr-1].max_capacity*
					overutil_threshold)/100;
		}
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

static void arch_get_cluster_cpus(struct cpumask *cpus, int cid)
{
	unsigned int cpu;

	cpumask_clear(cpus);
	for_each_possible_cpu(cpu) {
		struct cpu_topology *cpu_topo = &cpu_topology[cpu];
#ifdef CONFIG_ARM64
		if (cpu_topo->package_id == cid)
#else
		if (cpu_topo->socket_id == cid)
#endif
			cpumask_set_cpu(cpu, cpus);
	}
}

int sched_get_nr_overutil_avg(int cluster_id,
			      int *l_avg,
			      int *h_avg,
			      int *sum_nr_overutil_l,
			      int *sum_nr_overutil_h,
			      int *max_nr)
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
	u64 tmp_max_nr = 0;

	/* Need to make sure initialization done. */
	if (!init_heavy) {
		*l_avg = *h_avg = *max_nr = 0;
		*sum_nr_overutil_l = *sum_nr_overutil_h = 0;
		return -1;
	}

	/* cluster_id  need reasonale. */
	cluster_nr = arch_nr_clusters();
	if (cluster_id < 0 || cluster_id >= cluster_nr) {
		printk_deferred_once("[%s] invalid cluster id %d\n",
		__func__, cluster_id);
		return -1;
	}

	/* Time diff can't be zero/negative. */
	diff = (s64)(curr_time -
			cluster_heavy_tbl[cluster_id].last_get_overutil_time);
	if (diff <= 0) {
		*l_avg = *h_avg = *max_nr = 0;
		*sum_nr_overutil_l = *sum_nr_overutil_h = 0;
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
			spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);
			break;
		}

		/* get max_nr */
		tmp_max_nr = per_cpu(nr_max, cpu);
		if (tmp_max_nr > *max_nr)
			*max_nr = tmp_max_nr;
		/* reset max_nr value */
		per_cpu(nr_max, cpu) = per_cpu(nr, cpu);

		/* get sum of nr_overutil */
		*sum_nr_overutil_l += cpu_overutil->nr_overutil_l;
		*sum_nr_overutil_h += cpu_overutil->nr_overutil_h;

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
		*l_avg = *h_avg = *max_nr = 0;
		*sum_nr_overutil_l = *sum_nr_overutil_h = 0;
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

	if (per_cpu(nr, cpu) < 0)
		printk_deferred_once("assertion failed at %s:%d\n",
		__FILE__,
		__LINE__);

#ifdef CONFIG_MTK_CORE_CTL
	spin_lock(&per_cpu(nr_heavy_lock, cpu));
	if (per_cpu(nr, cpu) > per_cpu(nr_max, cpu))
		per_cpu(nr_max, cpu) = per_cpu(nr, cpu);
	spin_unlock(&per_cpu(nr_heavy_lock, cpu));
#endif
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
	enum overutil_type_t over_type = NO_OVERUTIL;

	if (!init_heavy) {
		init_heavy_tlb();
		if (!init_heavy) {
			printk_deferred_once("assertion failed at %s:%d\n",
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
	spin_unlock_irqrestore(&per_cpu(nr_heavy_lock, cpu), flags);
}
EXPORT_SYMBOL(sched_update_nr_heavy_prod);

#define MAX_CLUSTER_NR	3

/*
 * if all core ids are set correctly, then return true.
 * That is meaning that there is no repeat core id in same
 * cluster.
 */
static inline bool is_all_cpu_parsed(void)
{
#ifdef CONFIG_ARM64
	int cpu = 0, core_id = 0, cluster_id = 0;
	bool all_parsed = true;
	struct cpumask cluster_mask[MAX_CLUSTER_NR];
	struct cpumask *tmp_cpumask = NULL;

	for (cluster_id = 0; cluster_id < MAX_CLUSTER_NR; cluster_id++)
		cpumask_clear(&cluster_mask[cluster_id]);

	for_each_possible_cpu(cpu) {
		core_id = cpu_topology[cpu].core_id;
		cluster_id = cpu_topology[cpu].package_id;
		if (core_id < 0 || cluster_id < 0) {
			all_parsed = false;
			break;
		}
		tmp_cpumask = &cluster_mask[cluster_id];
		if (cpumask_test_cpu(core_id, tmp_cpumask)) {
			all_parsed = false;
			break;
		}
		cpumask_set_cpu(core_id, tmp_cpumask);
	}
	return all_parsed;
#else
	return true;
#endif
}

static int init_heavy_tlb(void)
{
	if (!is_all_cpu_parsed())
		return init_heavy;

	if (!init_heavy) {
		/* init variables */
		int tmp_cpu, cluster_nr;
		int i;
		int cid;
		struct cpumask cls_cpus;
		struct overutil_stats_t *cpu_overutil;
		unsigned int overutil_threshold = 100;

		printk_deferred_once("%s start.\n", __func__);

		gb_task_util = 0;
		gb_task_pid = 0;
		gb_task_cpu = 0;

		/* allocation for clustser information */
		cluster_nr = arch_nr_clusters();
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
			cid = arch_get_cluster_id(tmp_cpu);

			/* reset nr_heavy */
			per_cpu(nr_heavy, tmp_cpu) = 0;
			/* reset nr_overutil */
			cpu_overutil->nr_overutil_l = 0;
			cpu_overutil->nr_overutil_h = 0;
			cpu_overutil->max_task_util = 0;
			cpu_overutil->max_task_pid = 0;

			if (cid < 0 || cid >= cluster_nr) {
				printk_deferred_once("%s: cid=%d is out of nr=%d\n", __func__,
							cid, cluster_nr);
				continue;
			}

			/*
			 * Initialize threshold for up/down
			 * over-utilization tracking
			 */
			if (cid == 0)
				cpu_overutil->overutil_thresh_l = INT_MAX;
			else {
				overutil_threshold = get_overutil_threshold(cid-1);
				cpu_overutil->overutil_thresh_l =
					(int)(cluster_heavy_tbl[cluster_nr-1].max_capacity*
						overutil_threshold)/100;
			}

			if (cid == cluster_nr-1)
				cpu_overutil->overutil_thresh_h = INT_MAX;
			else {
				overutil_threshold = get_overutil_threshold(cid);
				cpu_overutil->overutil_thresh_h =
					(int)(cluster_heavy_tbl[cluster_nr-1].max_capacity*
						overutil_threshold)/100;
			}

			printk_deferred_once("%s: cpu=%d thresh_l=%d thresh_h=%d max_capaicy=%lu\n",
				__func__, tmp_cpu,
				cpu_overutil->overutil_thresh_l,
				cpu_overutil->overutil_thresh_h,
				(unsigned long)
				cluster_heavy_tbl[cid].max_capacity);
		}
		init_heavy = 1;
	}

	return init_heavy;
}
