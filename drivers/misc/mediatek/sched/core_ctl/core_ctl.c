// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/syscore_ops.h>
#include <linux/module.h>
#include <uapi/linux/sched/types.h>
#include <trace/hooks/sched.h>
#include <sched/sched.h>
#include <linux/sched/clock.h>

#ifndef __CHECKER__
#define CREATE_TRACE_POINTS
#include "core_ctl_trace.h"
#endif

#include "sched_avg.h"
#include <sched_sys_common.h>
#include <thermal_interface.h>

#define TAG "core_ctl"

struct ppm_table {
	/* normal: cpu power data */
	unsigned int static_pwr;
	/* thermal: cpu power data */
	unsigned int thermal_static_pwr;
	unsigned int dyn_pwr;
	unsigned int capacity;
	/* cpu cacpacity divide cpu power data */
	unsigned int efficiency;
	unsigned int thermal_efficiency;
};

struct cluster_ppm_data {
	bool init;
	int opp_nr;
	struct ppm_table *ppm_tbl;
};

struct cluster_data {
	bool inited;
	bool enable;
	int nr_up;
	int nr_down;
	int need_spread_cpus;
	unsigned int cluster_id;
	unsigned int min_cpus;
	unsigned int max_cpus;
	unsigned int first_cpu;
	unsigned int active_cpus;
	unsigned int num_cpus;
	unsigned int nr_assist;
	unsigned int up_thres;
	unsigned int down_thres;
	unsigned int thermal_degree_thres;
	unsigned int thermal_up_thres;
	unsigned int nr_not_preferred_cpus;
	unsigned int need_cpus;
	unsigned int new_need_cpus;
	unsigned int boost;
	unsigned int nr_paused_cpus;
	unsigned int cpu_busy_up_thres;
	unsigned int cpu_busy_down_thres;
	cpumask_t cpu_mask;
	bool pending;
	spinlock_t pending_lock;
	struct task_struct *core_ctl_thread;
	struct kobject kobj;
	s64 offline_throttle_ms;
	s64 next_offline_time;
	struct cluster_ppm_data ppm_data;
};

struct cpu_data {
	bool not_preferred;
	bool paused_by_cc;
	bool force_paused;
	unsigned int cpu;
	unsigned int cpu_util_pct;
	unsigned int is_busy;
	struct cluster_data *cluster;
};

#define ENABLE		1
#define DISABLE		0
#define L_CLUSTER_ID	0
#define BL_CLUSTER_ID	1
#define AB_CLUSTER_ID	2
#define CORE_OFF	true
#define CORE_ON		false
#define MAX_CLUSTERS		3
#define MAX_CPUS_PER_CLUSTER	6
#define MAX_NR_DOWN_THRESHOLD	4
#define MAX_BTASK_THRESH	100
#define MAX_CPU_TJ_DEGREE	100000
#define BIG_TASK_AVG_THRESHOLD	25

#define for_each_cluster(cluster, idx) \
	for ((cluster) = &cluster_state[idx]; (idx) < num_clusters;\
		(idx)++, (cluster) = &cluster_state[idx])

#define core_ctl_debug(x...)		\
	do {				\
		if (debug_enable)	\
			pr_info(x);	\
	} while (0)

static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static struct cluster_data cluster_state[MAX_CLUSTERS];
static unsigned int num_clusters;
static DEFINE_SPINLOCK(state_lock);
static DEFINE_SPINLOCK(check_lock);
static DEFINE_MUTEX(core_ctl_force_lock);
static bool initialized;
static unsigned int default_min_cpus[MAX_CLUSTERS] = {4, 2, 0};
static bool debug_enable;
module_param_named(debug_enable, debug_enable, bool, 0600);

static unsigned int enable_policy;

/*
 *  core_ctl_enable_policy - enable policy of core control
 *  @enable: true if set, false if unset.
 */
int core_ctl_enable_policy(unsigned int policy)
{
	unsigned int old_val;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	if (policy != enable_policy) {
		old_val = enable_policy;
		enable_policy = policy;
		pr_info("%s: Change policy from %d to %d successfully.",
				TAG, old_val, policy);
	}
	spin_unlock_irqrestore(&state_lock, flags);
	return 0;
}
EXPORT_SYMBOL(core_ctl_enable_policy);

static int set_core_ctl_policy(const char *buf,
			       const struct kernel_param *kp)
{
	int ret = 0;
	unsigned int val = 0;

	ret = kstrtouint(buf, 0, &val);
	if (val >= POLICY_CNT)
		ret = -EINVAL;

	if (!ret)
		core_ctl_enable_policy(val);
	return ret;
}

static struct kernel_param_ops set_core_ctl_policy_param_ops = {
	.set = set_core_ctl_policy,
	.get = param_get_uint,
};

param_check_uint(policy_enable, &enable_policy);
module_param_cb(policy_enable, &set_core_ctl_policy_param_ops, &enable_policy, 0600);
MODULE_PARM_DESC(policy_enable, "echo cpu pause policy if needed");

static unsigned int apply_limits(const struct cluster_data *cluster,
				 unsigned int need_cpus)
{
	return min(max(cluster->min_cpus, need_cpus), cluster->max_cpus);
}

int sched_active_count(const cpumask_t *mask, bool intersect_online)
{
	cpumask_t count_mask = CPU_MASK_NONE;

	if (intersect_online) {
		cpumask_and(&count_mask, cpu_online_mask, cpu_active_mask);
		cpumask_and(&count_mask, &count_mask, mask);
	} else
		cpumask_and(&count_mask, mask, cpu_active_mask);

	return cpumask_weight(&count_mask);
}

static unsigned int get_active_cpu_count(const struct cluster_data *cluster)
{
	return sched_active_count(&cluster->cpu_mask, true);
}

static bool is_active(const struct cpu_data *state)
{
	return cpu_online(state->cpu) && cpu_active(state->cpu);
}

static bool adjustment_possible(const struct cluster_data *cluster,
				unsigned int need)
{
	/*
	 * Why need to check nr_paused_cpu ?
	 * Consider the following situation,
	 * num_cpus = 4, min_cpus = 4 and a cpu
	 * force paused. That will do inactive
	 * core-on in the time.
	 */
	return (need < cluster->active_cpus || (need > cluster->active_cpus
			&& cluster->nr_paused_cpus));
}

static void wake_up_core_ctl_thread(struct cluster_data *cluster)
{
	unsigned long flags;

	spin_lock_irqsave(&cluster->pending_lock, flags);
	cluster->pending = true;
	spin_unlock_irqrestore(&cluster->pending_lock, flags);

	wake_up_process(cluster->core_ctl_thread);
}

bool is_cluster_init(unsigned int cid)
{
	return  cluster_state[cid].inited;
}

static bool demand_eval(struct cluster_data *cluster)
{
	unsigned long flags;
	unsigned int need_cpus = 0;
	bool ret = false;
	bool need_flag = false;
	unsigned int new_need;
	unsigned int old_need;
	s64 now, elapsed;

	if (unlikely(!cluster->inited))
		return ret;

	spin_lock_irqsave(&state_lock, flags);

	if (cluster->boost || !cluster->enable || !enable_policy)
		need_cpus = cluster->max_cpus;
	else
		need_cpus = cluster->new_need_cpus;

	/* check again active cpus. */
	cluster->active_cpus = get_active_cpu_count(cluster);
	new_need = apply_limits(cluster, need_cpus);
	/*
	 * When there is no adjustment in need, avoid
	 * unnecessary waking up core_ctl thread
	 */
	need_flag = adjustment_possible(cluster, new_need);
	old_need = cluster->need_cpus;

	now = ktime_to_ms(ktime_get());

	/* core-on */
	if (new_need > cluster->active_cpus) {
		ret = true;
	} else {
		/*
		 * If no more CPUs are needed or paused,
		 * just update the next offline time.
		 */
		if (new_need == cluster->active_cpus) {
			cluster->next_offline_time = now;
			cluster->need_cpus = new_need;
			goto unlock;
		}

		/* Does it exceed throttle time ? */
		elapsed = now - cluster->next_offline_time;
		ret = elapsed >= cluster->offline_throttle_ms;
	}

	if (ret) {
		cluster->next_offline_time = now;
		cluster->need_cpus = new_need;
	}
	trace_core_ctl_demand_eval(cluster->cluster_id,
			old_need, new_need,
			cluster->active_cpus,
			cluster->min_cpus,
			cluster->max_cpus,
			cluster->boost,
			cluster->enable,
			ret && need_flag);
unlock:
	spin_unlock_irqrestore(&state_lock, flags);
	return ret && need_flag;
}

static void apply_demand(struct cluster_data *cluster)
{
	if (demand_eval(cluster))
		wake_up_core_ctl_thread(cluster);
}

static void set_min_cpus(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	cluster->min_cpus = min(val, cluster->max_cpus);
	spin_unlock_irqrestore(&state_lock, flags);
	wake_up_core_ctl_thread(cluster);
}

static void set_max_cpus(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	val = min(val, cluster->num_cpus);
	cluster->max_cpus = val;
	cluster->min_cpus = min(cluster->min_cpus, cluster->max_cpus);
	spin_unlock_irqrestore(&state_lock, flags);
	wake_up_core_ctl_thread(cluster);
}

void set_offline_throttle_ms(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	cluster->offline_throttle_ms = val;
	spin_unlock_irqrestore(&state_lock, flags);
	apply_demand(cluster);
}

static inline
void update_next_cluster_down_thres(unsigned int index,
				     unsigned int new_thresh)
{
	struct cluster_data *next_cluster;

	if (index == num_clusters - 1)
		return;

	next_cluster = &cluster_state[index + 1];
	next_cluster->down_thres = new_thresh;
}

static inline
void set_not_preferred_locked(int cpu, bool enable)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	bool changed = false;

	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;
	if (enable) {
		changed = !c->not_preferred;
		c->not_preferred = 1;
	} else {
		if (c->not_preferred) {
			c->not_preferred = 0;
			changed = !c->not_preferred;
		}
	}

	if (changed) {
		if (enable)
			cluster->nr_not_preferred_cpus += 1;
		else
			cluster->nr_not_preferred_cpus -= 1;
	}
}

static void set_up_thres(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	old_thresh = cluster->up_thres;
	cluster->up_thres = val;

	if (old_thresh != cluster->up_thres) {
		update_next_cluster_down_thres(
				cluster->cluster_id,
				cluster->up_thres);
		set_over_threshold(cluster->cluster_id,
				       cluster->up_thres);
	}
	spin_unlock_irqrestore(&state_lock, flags);
}

/* ==================== export function ======================== */

int core_ctl_set_min_cpus(unsigned int cid, unsigned int min)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_min_cpus(cluster, min);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_min_cpus);

int core_ctl_set_max_cpus(unsigned int cid, unsigned int max)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_max_cpus(cluster, max);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_max_cpus);

/*
 *  core_ctl_set_limit_cpus - set min/max cpus of the cluster
 *  @cid: cluster id
 *  @min: min cpus
 *  @max: max cpus.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_limit_cpus(unsigned int cid,
			     unsigned int min,
			     unsigned int max)
{
	struct cluster_data *cluster;
	unsigned long flags;

	if (cid >= num_clusters)
		return -EINVAL;

	if (max < min)
		return -EINVAL;

	spin_lock_irqsave(&state_lock, flags);
	cluster = &cluster_state[cid];
	max = min(max, cluster->num_cpus);
	min = min(min, max);
	cluster->max_cpus = max;
	cluster->min_cpus = min;
	spin_unlock_irqrestore(&state_lock, flags);
	core_ctl_debug("%s: Try to adjust cluster %u limit cpus. min_cpus: %u, max_cpus: %u",
			TAG, cid, min, max);
	wake_up_core_ctl_thread(cluster);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_limit_cpus);

/*
 *  core_ctl_set_offline_throttle_ms - set throttle time of core-off judgement
 *  @cid: cluster id
 *  @throttle_ms: The unit of throttle time is microsecond
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_offline_throttle_ms(unsigned int cid,
				     unsigned int throttle_ms)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_offline_throttle_ms(cluster, throttle_ms);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_offline_throttle_ms);

/*
 *  core_ctl_set_boost
 *  @return: 0 if success, else return errno
 *
 *  When boost is enbled, all cluster of CPUs will be core-on.
 */
int core_ctl_set_boost(bool boost)
{
	int ret;
	unsigned int index = 0;
	unsigned long flags;
	struct cluster_data *cluster;
	bool changed = false;

	spin_lock_irqsave(&state_lock, flags);
	for_each_cluster(cluster, index) {
		if (boost) {
			changed = !cluster->boost;
			cluster->boost = 1;
		} else {
			if (cluster->boost) {
				cluster->boost = 0;
				changed = !cluster->boost;
			} else {
				/* FIXME: change to continue ? */
				ret = -EINVAL;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&state_lock, flags);

	if (changed) {
		index = 0;
		for_each_cluster(cluster, index)
			apply_demand(cluster);
	}

	core_ctl_debug("%s: boost=%d ret=%d ", boost, ret);
	return ret;
}
EXPORT_SYMBOL(core_ctl_set_boost);

#define	MAX_CPU_MASK	((1 << nr_cpu_ids) - 1)
/*
 *  core_ctl_set_not_preferred - set not_prefer for the specific cpu number
 *  @not_preferred_cpus: Stand for cpu bitmap, 1 if set, 0 if unset.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_not_preferred(unsigned int not_preferred_cpus)
{
	unsigned long flags;
	int i;
	bool bval;

	if (not_preferred_cpus > MAX_CPU_MASK)
		return -EINVAL;

	spin_lock_irqsave(&state_lock, flags);
	for (i = 0; i < nr_cpu_ids; i++) {
		bval = !!(not_preferred_cpus & (1 << i));
		set_not_preferred_locked(i, bval);
	}
	spin_unlock_irqrestore(&state_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(core_ctl_set_not_preferred);

/*
 *  core_ctl_set_up_thres - adjuset up threshold value
 *  @cid: cluster id
 *  @val: Percentage of big core capactity. (0 - 100)
 *
 *  return 0 if success, else return errno
 */
int core_ctl_set_up_thres(int cid, unsigned int val)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	/* Range of up thrash should be 0 - 100 */
	if (val > MAX_BTASK_THRESH)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_up_thres(cluster, val);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_up_thres);

/*
 *  core_ctl_force_pause_cpu - force pause or resume cpu
 *  @cpu: cpu number
 *  @is_pause: set true if pause, set false if resume.
 *
 *  return 0 if success, else return errno
 */
int core_ctl_force_pause_cpu(unsigned int cpu, bool is_pause)
{
	int ret;
	unsigned long flags;
	struct cpu_data *c;
	struct cluster_data *cluster;

	if (cpu > nr_cpu_ids)
		return -EINVAL;

	if (!cpu_online(cpu))
		return -EBUSY;

	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;

	mutex_lock(&core_ctl_force_lock);

	if (is_pause)
		ret = sched_pause_cpu(cpu);
	else
		ret = sched_resume_cpu(cpu);

	/* error occurs */
	if (ret)
		goto unlock;

	/* Update cpu state */
	spin_lock_irqsave(&state_lock, flags);
	c->force_paused = is_pause;
	/* Handle conflict with original policy */
	if (c->paused_by_cc) {
		c->paused_by_cc = false;
		cluster->nr_paused_cpus--;
	}
	cluster->active_cpus = get_active_cpu_count(cluster);
	spin_unlock_irqrestore(&state_lock, flags);

unlock:
	mutex_unlock(&core_ctl_force_lock);
	return ret;
}
EXPORT_SYMBOL(core_ctl_force_pause_cpu);

/* ==================== sysctl node ======================== */

static ssize_t store_min_cpus(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_min_cpus(state, val);
	return count;
}

static ssize_t show_min_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->min_cpus);
}

static ssize_t store_max_cpus(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_max_cpus(state, val);
	return count;
}

static ssize_t show_max_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->max_cpus);
}

static ssize_t store_offline_throttle_ms(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	set_offline_throttle_ms(state, val);
	return count;
}

static ssize_t show_offline_throttle_ms(const struct cluster_data *state,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->offline_throttle_ms);
}

static ssize_t store_up_thres(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* No need to change up_thres for the last cluster */
	if (state->cluster_id >= num_clusters-1)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		return -EINVAL;

	set_up_thres(state, val);
	return count;
}

static ssize_t show_up_thres(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->up_thres);
}

static ssize_t store_not_preferred(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int i;
	unsigned int val[MAX_CPUS_PER_CLUSTER];
	unsigned long flags;
	int ret;

	ret = sscanf(buf, "%u %u %u %u %u %u\n",
			&val[0], &val[1], &val[2], &val[3],
			&val[4], &val[5]);
	if (ret != state->num_cpus)
		return -EINVAL;

	spin_lock_irqsave(&state_lock, flags);
	for (i = 0; i < state->num_cpus; i++)
		set_not_preferred_locked(i + state->first_cpu, val[i]);
	spin_unlock_irqrestore(&state_lock, flags);

	return count;
}

static ssize_t show_not_preferred(const struct cluster_data *state, char *buf)
{
	ssize_t count = 0;
	struct cpu_data *c;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&state_lock, flags);
	for (i = 0; i < state->num_cpus; i++) {
		c = &per_cpu(cpu_state, i + state->first_cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"CPU#%d: %u\n", c->cpu, c->not_preferred);
	}
	spin_unlock_irqrestore(&state_lock, flags);

	return count;
}

static ssize_t store_core_ctl_boost(struct cluster_data *state,
		const char *buf, size_t count)
{
	int ret;
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* only allow first cluster */
	if (state->cluster_id != 0)
		return -EINVAL;

	if (val == 0 || val == 1)
		ret = core_ctl_set_boost(val);
	else
		ret = -EINVAL;

	return ret;
}

static ssize_t show_core_ctl_boost(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->boost);
}

static ssize_t store_enable(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;
	bool bval;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	bval = !!val;
	if (bval != state->enable) {
		state->enable = bval;
		apply_demand(state);
	}

	return count;
}

static ssize_t show_enable(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->enable);
}

static ssize_t show_ppm_state(const struct cluster_data *state, char *buf)
{
	ssize_t count = 0;
	int opp_nr = state->ppm_data.opp_nr;
	int i;

	if (!state->ppm_data.init) {
		count += snprintf(buf + count, PAGE_SIZE - count,
				"ppm_data is not initialized\n");
		return count;
	}

	count += snprintf(buf + count, PAGE_SIZE - count,
			"OPP   CAP   STATIC_45   STATIC_65   DYNC   EFF_45   EFF_85\n");
	for (i = 0; i < opp_nr; i++) {
		count += snprintf(buf + count, PAGE_SIZE - count,
				"%4d  %4u   %8u   %8u   %8u   %8u   %8u\n", i,
				state->ppm_data.ppm_tbl[i].capacity,
				state->ppm_data.ppm_tbl[i].static_pwr,
				state->ppm_data.ppm_tbl[i].thermal_static_pwr,
				state->ppm_data.ppm_tbl[i].dyn_pwr,
				state->ppm_data.ppm_tbl[i].efficiency,
				state->ppm_data.ppm_tbl[i].thermal_efficiency);
	}
	return count;
}

static ssize_t show_thermal_up_thres(const struct cluster_data *state, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", state->thermal_up_thres);
}

static ssize_t show_global_state(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	ssize_t count = 0;
	unsigned int cpu;

	spin_lock_irq(&state_lock);
	for_each_possible_cpu(cpu) {
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;
		if (!cluster || !cluster->inited)
			continue;

		/* Only show this cluster */
		if (!cpumask_test_cpu(cpu, &state->cpu_mask))
			continue;

		if (cluster->first_cpu == cpu) {
			count += snprintf(buf + count, PAGE_SIZE - count,
					"Cluster%u\n", state->cluster_id);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tFirst CPU: %u\n", cluster->first_cpu);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tActive CPUs: %u\n",
					get_active_cpu_count(cluster));
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNeed CPUs: %u\n", cluster->need_cpus);
			count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNR Paused CPUs(pause by core_ctl): %u\n",
					cluster->nr_paused_cpus);
		}

		count += snprintf(buf + count, PAGE_SIZE - count,
				"CPU%u\n", cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tOnline: %u\n", cpu_online(c->cpu));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tPaused: %u\n",
				(cpu_online(c->cpu) & !cpu_active(c->cpu)));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tPaused by core_ctl: %u\n", c->paused_by_cc);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tCPU utils(%%): %u\n", c->cpu_util_pct);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tIs busy: %u\n", c->is_busy);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tNot preferred: %u\n", c->not_preferred);
	}
	spin_unlock_irq(&state_lock);

	return count;
}

struct core_ctl_attr {
	struct attribute attr;
	ssize_t (*show)(const struct cluster_data *state, char *buf);
	ssize_t (*store)(struct cluster_data *state, const char *buf, size_t count);
};

#define core_ctl_attr_ro(_name)         \
static struct core_ctl_attr _name =     \
__ATTR(_name, 0400, show_##_name, NULL)

#define core_ctl_attr_rw(_name)                 \
static struct core_ctl_attr _name =             \
__ATTR(_name, 0600, show_##_name, store_##_name)

core_ctl_attr_rw(min_cpus);
core_ctl_attr_rw(max_cpus);
core_ctl_attr_rw(offline_throttle_ms);
core_ctl_attr_rw(up_thres);
core_ctl_attr_rw(not_preferred);
core_ctl_attr_rw(core_ctl_boost);
core_ctl_attr_rw(enable);
core_ctl_attr_ro(global_state);
core_ctl_attr_ro(ppm_state);
core_ctl_attr_ro(thermal_up_thres);

static struct attribute *default_attrs[] = {
	&min_cpus.attr,
	&max_cpus.attr,
	&offline_throttle_ms.attr,
	&up_thres.attr,
	&not_preferred.attr,
	&core_ctl_boost.attr,
	&enable.attr,
	&global_state.attr,
	&ppm_state.attr,
	&thermal_up_thres.attr,
	NULL
};

#define to_cluster_data(k) container_of(k, struct cluster_data, kobj)
#define to_attr(a) container_of(a, struct core_ctl_attr, attr)
static ssize_t show(struct kobject *kobj, struct attribute *attr, char *buf)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->show)
		ret = cattr->show(data, buf);

	return ret;
}

static ssize_t store(struct kobject *kobj, struct attribute *attr,
		const char *buf, size_t count)
{
	struct cluster_data *data = to_cluster_data(kobj);
	struct core_ctl_attr *cattr = to_attr(attr);
	ssize_t ret = -EIO;

	if (cattr->store)
		ret = cattr->store(data, buf, count);

	return ret;
}

static const struct sysfs_ops sysfs_ops = {
	.show   = show,
	.store  = store,
};

static struct kobj_type ktype_core_ctl = {
	.sysfs_ops      = &sysfs_ops,
	.default_attrs  = default_attrs,
};

static unsigned int thermal_heaviest_thres = 512;
static unsigned int heaviest_thres = 230;

/* ==================== algorithm of core control ======================== */

#define BIG_TASK_AVG_THRESHOLD 25
/*
 * Rewrite from sched_big_task_nr()
 */
void get_nr_running_big_task(struct cluster_data *cluster)
{
	unsigned int avg_down[MAX_CLUSTERS] = {0};
	unsigned int avg_up[MAX_CLUSTERS] = {0};
	unsigned int nr_up[MAX_CLUSTERS] = {0};
	unsigned int nr_down[MAX_CLUSTERS] = {0};
	unsigned int need_spread_cpus[MAX_CLUSTERS] = {0};
	unsigned int i, delta;

	for (i = 0; i < num_clusters; i++) {
		sched_get_nr_over_thres_avg(i,
					  &avg_down[i],
					  &avg_up[i],
					  &nr_down[i],
					  &nr_up[i],
					  &need_spread_cpus[i],
					  enable_policy);
		cluster[i].need_spread_cpus = need_spread_cpus[i];
	}

	for (i = 0; i < num_clusters; i++) {
		/* reset nr_up and nr_down */
		cluster[i].nr_up = 0;
		cluster[i].nr_down = 0;

		if (nr_up[i]) {
			if (avg_up[i]/nr_up[i] > BIG_TASK_AVG_THRESHOLD)
				cluster[i].nr_up = nr_up[i];
			else /* min(avg_up[i]/BIG_TASK_AVG_THRESHOLD,nr_up[i]) */
				cluster[i].nr_up =
					avg_up[i]/BIG_TASK_AVG_THRESHOLD > nr_up[i] ?
					nr_up[i] : avg_up[i]/BIG_TASK_AVG_THRESHOLD;
		}
		/*
		 * The nr_up is part of nr_down, so
		 * the real nr_down is nr_down minus nr_up.
		 */
		delta = nr_down[i] - nr_up[i];
		if (nr_down[i] && delta > 0) {
			if (((avg_down[i]-avg_up[i]) / delta)
					> BIG_TASK_AVG_THRESHOLD)
				cluster[i].nr_down = delta;
			else
				cluster[i].nr_down =
					(avg_down[i]-avg_up[i])/
					delta < BIG_TASK_AVG_THRESHOLD ?
					delta : (avg_down[i]-avg_up[i])/
					BIG_TASK_AVG_THRESHOLD;
		}
		/* nr can't be negative */
		cluster[i].nr_up = cluster[i].nr_up < 0 ? 0 : cluster[i].nr_up;
		cluster[i].nr_down = cluster[i].nr_down < 0 ? 0 : cluster[i].nr_down;
	}

	for (i = 0; i < num_clusters; i++) {
		nr_up[i] = cluster[i].nr_up;
		nr_down[i] = cluster[i].nr_down;
		need_spread_cpus[i] = cluster[i].need_spread_cpus;
	}
	trace_core_ctl_update_nr_over_thres(nr_up, nr_down, need_spread_cpus);
}

/*
 * prev_cluster_nr_assist:
 *   Tasks that are eligible to run on the previous
 *   cluster but cannot run because of insufficient
 *   CPUs there. It's indicative of number of CPUs
 *   in this cluster that should assist its
 *   previous cluster to makeup for insufficient
 *   CPUs there.
 */
static inline int get_prev_cluster_nr_assist(int index)
{
	struct cluster_data *prev_cluster;

	if (index == 0)
		return 0;

	index--;
	prev_cluster = &cluster_state[index];
	return prev_cluster->nr_assist;

}

#define CORE_CTL_PERIODIC_TRACK_MS	4
static inline bool window_check(void)
{
	unsigned long flags;
	ktime_t now = ktime_get();
	static ktime_t tracking_last_update;
	bool do_check = false;

	spin_lock_irqsave(&check_lock, flags);
	if (ktime_after(now, ktime_add_ms(
		tracking_last_update, CORE_CTL_PERIODIC_TRACK_MS))) {
		do_check = true;
		tracking_last_update = now;
	}
	spin_unlock_irqrestore(&check_lock, flags);
	return do_check;
}

static inline void core_ctl_main_algo(void)
{
	unsigned int max_util = 0;
	unsigned int index = 0;
	unsigned int big_cpu_ts = 0;
	struct cluster_data *cluster;
	unsigned int orig_need_cpu[MAX_CLUSTERS] = {0};
	unsigned int total_need_spread_cpu = 0;

	/* get TLP of over threshold tasks */
	get_nr_running_big_task(cluster_state);

	/* Apply TLP of tasks */
	for_each_cluster(cluster, index) {
		int temp_need_cpus = 0;

		/* for high TLP with tiny tasks */
		if (cluster->need_spread_cpus)
			total_need_spread_cpu +=
				(enable_policy == CONSERVATIVE_POLICY) ?
				cluster->need_spread_cpus : 1;

		if (index == 0) {
			cluster->nr_assist = cluster->nr_up;
			cluster->new_need_cpus = cluster->num_cpus;
			continue;
		}

		temp_need_cpus += cluster->nr_up;
		temp_need_cpus += cluster->nr_down;
		temp_need_cpus += get_prev_cluster_nr_assist(index);

		cluster->new_need_cpus = temp_need_cpus;

		/* nr_assist(i) = max(0, need_cpus(i) - max_cpus(i)) */
		cluster->nr_assist =
			(temp_need_cpus > cluster->max_cpus ?
			 (temp_need_cpus - cluster->max_cpus) : 0);

		/* spread high TLP to BL/B CPU */
		if (total_need_spread_cpu) {
			int extra_cpus =
				cluster->max_cpus - cluster->new_need_cpus;

			if (extra_cpus > 0) {
				cluster->new_need_cpus +=
					(total_need_spread_cpu > extra_cpus) ?
					extra_cpus : total_need_spread_cpu;
				total_need_spread_cpu =
					(total_need_spread_cpu > extra_cpus) ?
					(total_need_spread_cpu - extra_cpus) : 0;
			}
		}
	}

	/*
	 * Ensure prime cpu make core-on if heaviest task is over threshold.
	 */
	if (num_clusters > 2) {
		struct cluster_data *big_cluster;
		struct cluster_data *prev_cluster;
		unsigned int max_capacity;

		sched_max_util_task(&max_util);
		big_cluster = &cluster_state[num_clusters - 1];
		prev_cluster = &cluster_state[big_cluster->cluster_id - 1];
#if IS_ENABLED(CONFIG_MTK_THERMAL_INTERFACE)
		big_cpu_ts = get_cpu_temp(big_cluster->first_cpu);
#endif

		/*
		 * Check for biggest task in system,
		 * if it's over threshold, force to enable
		 * prime core.
		 */
		max_capacity = get_max_capacity(prev_cluster->cluster_id);
		heaviest_thres = (unsigned int)
			div64_u64((u64)prev_cluster->up_thres * max_capacity, 100);
		thermal_heaviest_thres = (unsigned int)
			div64_u64((u64)prev_cluster->thermal_up_thres * max_capacity, 100);

		/* If CPU is thermal, use thermal threshold as heaviest threshold */
		if (big_cpu_ts > big_cluster->thermal_degree_thres)
			heaviest_thres = thermal_heaviest_thres;

		/* If max util is over threshold */
		if (!big_cluster->new_need_cpus &&
				max_util > heaviest_thres) {
			big_cluster->new_need_cpus++;

			/*
			 * For example:
			 *   Consider TLP=1 and heaviest is over threshold,
			 *   prefer to decrease a need CPU of prev cluster
			 *   to save power consumption
			 */
			if (prev_cluster->new_need_cpus > 0)
				prev_cluster->new_need_cpus--;
		}
	}

	/* reset index value */
	index = 0;
	for_each_cluster(cluster, index) {
		orig_need_cpu[index] = cluster->new_need_cpus;
	}

	trace_core_ctl_algo_info(big_cpu_ts, heaviest_thres, max_util,
			cpumask_bits(cpu_active_mask)[0], orig_need_cpu);
}

void core_ctl_tick(void *data, struct rq *rq)
{
	unsigned int index = 0;
	unsigned long flags;
	struct cluster_data *cluster;
	int cpu = 0;
	struct cpu_data *c;

	/* prevent irq disable on cpu 0 */
	if (rq->cpu == 0)
		return;

	if (!window_check())
		return;

	spin_lock_irqsave(&state_lock, flags);

	/* check CPU is busy or not */
	for_each_possible_cpu(cpu) {
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;

		if (!cluster || !cluster->inited)
			continue;

		c->cpu_util_pct = sched_get_cpu_util_pct(cpu);
		if (c->cpu_util_pct >= cluster->cpu_busy_up_thres)
			c->is_busy = true;
		else if (c->cpu_util_pct < cluster->cpu_busy_down_thres)
			c->is_busy = false;
	}

	if (enable_policy)
		core_ctl_main_algo();

	spin_unlock_irqrestore(&state_lock, flags);

	/* reset index value */
	/* index = 0; */
	for_each_cluster(cluster, index) {
		apply_demand(cluster);
	}
}

inline void core_ctl_update_active_cpu(unsigned int cpu)
{
	unsigned long flags;
	struct cpu_data *c;
	struct cluster_data *cluster;

	if (cpu > nr_cpu_ids)
		return;

	spin_lock_irqsave(&state_lock, flags);
	c = &per_cpu(cpu_state, cpu);
	cluster = c->cluster;
	cluster->active_cpus = get_active_cpu_count(cluster);
	spin_unlock_irqrestore(&state_lock, flags);
}
EXPORT_SYMBOL(core_ctl_update_active_cpu);

static void try_to_pause(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus;
	unsigned int nr_paused = 0;
	int cpu;
	bool success;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;
	bool check_busy = true;

again:
	nr_paused = 0;
	spin_lock_irqsave(&state_lock, flags);
	for (cpu = nr_cpu_ids-1; cpu >= 0; cpu--) {
		struct cpu_data *c;

		success = false;
		if (!cpumask_test_cpu(cpu, &cluster->cpu_mask))
			continue;

		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);
		if (!is_active(c))
			continue;

		if (check_busy && c->is_busy)
			continue;

		if (c->force_paused)
			continue;

		if (cluster->active_cpus == need)
			break;

		/*
		 * Pause only the not_preferred CPUs.
		 * If none of the CPUs are selected as not_preferred,
		 * then all CPUs are eligible for isolation.
		 */
		if (check_not_prefer && !c->not_preferred)
			continue;

		spin_unlock_irqrestore(&state_lock, flags);

		core_ctl_debug("%s: Trying to pause CPU%u\n", TAG, c->cpu);
		if (!sched_pause_cpu(c->cpu)) {
			success = true;
			nr_paused++;
		} else {
			core_ctl_debug("%s Unable to pause CPU%u\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&state_lock, flags);
		if (success) {
			/* check again, prevent a seldom racing issue */
			if (cpu_online(c->cpu))
				c->paused_by_cc = true;
			else {
				nr_paused--;
				pr_info("%s: Pause failed because cpu#%d is offline. ",
					TAG, c->cpu);
			}
		}
		cluster->active_cpus = get_active_cpu_count(cluster);
	}
	cluster->nr_paused_cpus += nr_paused;
	spin_unlock_irqrestore(&state_lock, flags);
	/*
	 * If the number of not prefer CPUs is not
	 * equal to need CPUs, then check it again.
	 */
	if (check_busy || (check_not_prefer &&
		cluster->active_cpus != need)) {
		num_cpus = cluster->num_cpus;
		check_not_prefer = false;
		check_busy = false;
		goto again;
	}
}

static void try_to_resume(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus, cpu;
	unsigned int nr_resumed = 0;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;
	bool success;

again:
	nr_resumed = 0;
	spin_lock_irqsave(&state_lock, flags);
	for_each_cpu(cpu, &cluster->cpu_mask) {
		struct cpu_data *c;

		success = false;
		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);

		if (!c->paused_by_cc)
			continue;

		if (c->force_paused)
			continue;

		if (!cpu_online(c->cpu))
			continue;

		if (cpu_active(c->cpu))
			continue;

		if (cluster->active_cpus == need)
			break;

		/* The Normal CPUs are resumed prior to not prefer CPUs */
		if (!check_not_prefer && c->not_preferred)
			continue;

		spin_unlock_irqrestore(&state_lock, flags);

		core_ctl_debug("%s: Trying to resume CPU%u\n", TAG, c->cpu);
		if (!sched_resume_cpu(c->cpu)) {
			success = true;
			nr_resumed++;
		} else {
			core_ctl_debug("%s: Unable to resume CPU%u\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&state_lock, flags);
		if (success)
			c->paused_by_cc = false;
		cluster->active_cpus = get_active_cpu_count(cluster);
	}
	cluster->nr_paused_cpus -= nr_resumed;
	spin_unlock_irqrestore(&state_lock, flags);
	/*
	 * After un-isolated the number of prefer CPUs
	 * is not enough for need CPUs, then check
	 * not_prefer CPUs again.
	 */
	if (check_not_prefer &&
		cluster->active_cpus != need) {
		num_cpus = cluster->num_cpus;
		check_not_prefer = false;
		goto again;
	}
}

static void __ref do_core_ctl(struct cluster_data *cluster)
{
	unsigned int need;

	need = apply_limits(cluster, cluster->need_cpus);

	if (adjustment_possible(cluster, need)) {
		core_ctl_debug("%s: Trying to adjust cluster %u from %u to %u\n",
				TAG, cluster->cluster_id, cluster->active_cpus, need);

		mutex_lock(&core_ctl_force_lock);
		if (cluster->active_cpus > need)
			try_to_pause(cluster, need);
		else if (cluster->active_cpus < need)
			try_to_resume(cluster, need);
		mutex_unlock(&core_ctl_force_lock);
	} else
		core_ctl_debug("%s: failed to adjust cluster %u from %u to %u. (min = %u, max = %u)\n",
				TAG, cluster->cluster_id, cluster->active_cpus, need,
				cluster->min_cpus, cluster->max_cpus);
}

static int __ref try_core_ctl(void *data)
{
	struct cluster_data *cluster = data;
	unsigned long flags;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&cluster->pending_lock, flags);
		if (!cluster->pending) {
			spin_unlock_irqrestore(&cluster->pending_lock, flags);
			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&cluster->pending_lock, flags);
		}
		set_current_state(TASK_RUNNING);
		cluster->pending = false;
		spin_unlock_irqrestore(&cluster->pending_lock, flags);

		do_core_ctl(cluster);
	}

	return 0;
}

static int cpu_pause_cpuhp_state(unsigned int cpu,  bool online)
{
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = state->cluster;
	unsigned int need;
	bool do_wakeup = false;
	unsigned long flags;

	if (unlikely(!cluster || !cluster->inited))
		return 0;

	spin_lock_irqsave(&state_lock, flags);

	if (online) {
		/*
		 * Consider the seldom race condition, that will cause
		 * a fault paused count. Once the cpu is active, it leads
		 * confusion control. (active_cpu and paused_by_cc = 1)
		 * Thus, adjust it when cpu is onlining.
		 *
		 *   module A               core_ctl
		 *      |                      |
		 *      |                      |
		 *      V                      V
		 *   down_cpu()          try_to_pause()
		 *  ------------         --------------
		 *     lock()                 ...
		 *  inactive_cpu()         pause_cpu()
		 *       ...                waiting
		 *   cpu_offline()            ...
		 *    unlock()                ...
		 *                           lock()
		 *                     no change return 0
		 *                          unlock()
		 *                      nr_paused_cpus++
		 */
		if (unlikely(state->paused_by_cc)) {
			state->paused_by_cc = false;
			cluster->nr_paused_cpus--;
		}
		state->force_paused = false;
	} else { /* offline */
		if (state->paused_by_cc) {
			state->paused_by_cc = false;
			cluster->nr_paused_cpus--;
		}
		state->cpu_util_pct = 0;
		state->force_paused = false;
		cluster->active_cpus =
			get_active_cpu_count(cluster);
	}

	need = apply_limits(cluster, cluster->need_cpus);
	do_wakeup = adjustment_possible(cluster, need);
	spin_unlock_irqrestore(&state_lock, flags);

	if (do_wakeup)
		wake_up_core_ctl_thread(cluster);
	return 0;
}

static int core_ctl_prepare_online_cpu(unsigned int cpu)
{
	return cpu_pause_cpuhp_state(cpu, true);
}

static int core_ctl_prepare_dead_cpu(unsigned int cpu)
{
	return cpu_pause_cpuhp_state(cpu, false);
}

static struct cluster_data *find_cluster_by_first_cpu(unsigned int first_cpu)
{
	unsigned int i;

	for (i = 0; i < num_clusters; ++i) {
		if (cluster_state[i].first_cpu == first_cpu)
			return &cluster_state[i];
	}

	return NULL;
}

/* ==================== init section ======================== */

static int ppm_data_init(struct cluster_data *cluster);

static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	unsigned int cpu;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };
	int ret = 0;

	/* first_cpu is defined */
	if (find_cluster_by_first_cpu(first_cpu))
		return ret;

	dev = get_cpu_device(first_cpu);
	if (!dev)
		return -ENODEV;


	core_ctl_debug("%s: Creating CPU group %d\n", TAG, first_cpu);

	if (num_clusters == MAX_CLUSTERS) {
		pr_info("%s: Unsupported number of clusters. Only %u supported\n",
				TAG, MAX_CLUSTERS);
		return -EINVAL;
	}
	cluster = &cluster_state[num_clusters];
	cluster->cluster_id = num_clusters;
	++num_clusters;

	cpumask_copy(&cluster->cpu_mask, mask);
	cluster->num_cpus = cpumask_weight(mask);
	if (cluster->num_cpus > MAX_CPUS_PER_CLUSTER) {
		pr_info("%s: HW configuration not supported\n", TAG);
		return -EINVAL;
	}
	cluster->first_cpu = first_cpu;
	cluster->min_cpus = 1;
	cluster->max_cpus = cluster->num_cpus;
	cluster->need_cpus = cluster->num_cpus;
	cluster->offline_throttle_ms = 100;
	cluster->enable = true;
	cluster->nr_down = 0;
	cluster->nr_up = 0;
	cluster->nr_assist = 0;

	cluster->min_cpus = default_min_cpus[cluster->cluster_id];

	if (cluster->cluster_id ==
			(arch_get_nr_clusters() - 1))
		cluster->up_thres = INT_MAX;
	else
		cluster->up_thres =
			get_over_threshold(cluster->cluster_id);

	if (cluster->cluster_id == 0)
		cluster->down_thres = INT_MAX;
	else
		cluster->down_thres =
			get_over_threshold(cluster->cluster_id - 1);

	if (cluster->cluster_id == AB_CLUSTER_ID) {
		cluster->thermal_degree_thres = 65000;
		cluster->thermal_up_thres = INT_MAX;
	} else {
		cluster->thermal_degree_thres = INT_MAX;
		cluster->thermal_up_thres = 80;
	}

	ret = ppm_data_init(cluster);
	if (ret)
		pr_info("initialize ppm data failed ret = %d", ret);

	cluster->nr_not_preferred_cpus = 0;
	spin_lock_init(&cluster->pending_lock);

	for_each_cpu(cpu, mask) {
		core_ctl_debug("%s: Init CPU%u state\n", TAG, cpu);
		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
	}

	cluster->cpu_busy_up_thres = 60;
	cluster->cpu_busy_down_thres = 30;

	cluster->next_offline_time =
		ktime_to_ms(ktime_get()) + cluster->offline_throttle_ms;
	cluster->active_cpus = get_active_cpu_count(cluster);

	cluster->core_ctl_thread = kthread_run(try_core_ctl, (void *) cluster,
			"core_ctl_v2.1/%d", first_cpu);
	if (IS_ERR(cluster->core_ctl_thread))
		return PTR_ERR(cluster->core_ctl_thread);

	sched_setscheduler_nocheck(cluster->core_ctl_thread, SCHED_FIFO,
			&param);

	cluster->inited = true;

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

static inline int get_opp_count(struct cpufreq_policy *policy)
{
	int opp_nr;
	struct cpufreq_frequency_table *freq_pos;

	cpufreq_for_each_entry_idx(freq_pos, policy->freq_table, opp_nr);
	return opp_nr;
}

#define NORMAL_TEMP	45
#define THERMAL_TEMP	85

/*
 * x1: BL cap
 * y1: BL eff
 * x2: B  cap
 * y2, B  eff
 */
bool check_eff_precisely(unsigned int x1,
			 unsigned int y1,
			 unsigned int x2,
			 unsigned int y2)
{
	unsigned int diff;
	unsigned int new_y1 = 0;

	diff = (unsigned int)div64_u64(x2 * 100, x1);
	new_y1 = (unsigned int)div64_u64(y1 * diff, 100);
	return y2 < new_y1;
}

static unsigned int find_turn_point(struct cluster_data *c1,
				    struct cluster_data *c2,
				    bool is_thermal)
{
	int i, j;
	bool changed = false;
	bool stop_going = false;
	unsigned int turn_point = 0;

	/* BLCPU */
	for (i = 0; i < c1->ppm_data.opp_nr - 1; i++) {
		changed = false;
		/* BCPU */
		for (j = c2->ppm_data.opp_nr - 1; j >= 0; j--) {
			unsigned int c1_eff, c2_eff;

			if (c2->ppm_data.ppm_tbl[j].capacity <
					c1->ppm_data.ppm_tbl[i].capacity)
				continue;

			c1_eff = is_thermal ? c1->ppm_data.ppm_tbl[i].thermal_efficiency
				: c1->ppm_data.ppm_tbl[i].efficiency;
			c2_eff = is_thermal ? c2->ppm_data.ppm_tbl[j].thermal_efficiency
				: c2->ppm_data.ppm_tbl[j].efficiency;
			if (c2_eff < c1_eff ||
					check_eff_precisely(
						c1->ppm_data.ppm_tbl[i].capacity, c1_eff,
						c2->ppm_data.ppm_tbl[j].capacity, c2_eff)) {
				turn_point = c2->ppm_data.ppm_tbl[j].capacity;
				changed = true;
				/*
				 * If lowest capacity of BCPU is more efficient than
				 * any capacity of BLCPU, we should not need to find
				 * further.
				 */
				if (j == c2->ppm_data.opp_nr - 1)
					stop_going = true;
			}
			break;
		}
		if (!changed)
			break;
		if (stop_going)
			break;
	}
	return turn_point;
}

static int ppm_data_init(struct cluster_data *cluster)
{
	struct cpufreq_policy *policy;
	struct ppm_table *ppm_tbl;
	struct em_perf_domain *pd;
	struct em_perf_state *ps;
	int opp_nr, first_cpu, i;
	int cid = cluster->cluster_id;

	first_cpu = cluster->first_cpu;
	policy = cpufreq_cpu_get(first_cpu);
	if (!policy) {
		pr_info("%s: cpufreq policy %d is not found for cpu#%d",
				TAG, first_cpu);
		return -ENOMEM;
	}

	opp_nr = get_opp_count(policy);
	ppm_tbl = kcalloc(opp_nr, sizeof(struct ppm_table), GFP_KERNEL);

	cluster->ppm_data.ppm_tbl = ppm_tbl;
	if (!cluster->ppm_data.ppm_tbl) {
		pr_info("%s: Failed to allocate ppm_table for cluster %d",
				TAG, cluster->cluster_id);
		cpufreq_cpu_put(policy);
		return -ENOMEM;
	}

	pd = em_cpu_get(first_cpu);
	if (!pd)
		return -ENOMEM;
	/* get power and capacity and calculate efficiency */

	for (i = 0; i < opp_nr; i++) {
		ps = &pd->table[opp_nr-1-i];
		ppm_tbl[i].dyn_pwr = ps->power << 10;
		ppm_tbl[i].static_pwr = mtk_get_leakage(first_cpu, i, NORMAL_TEMP);
		ppm_tbl[i].thermal_static_pwr = mtk_get_leakage(first_cpu, i, THERMAL_TEMP);
		ppm_tbl[i].capacity = pd_get_opp_capacity(first_cpu, i);
		ppm_tbl[i].efficiency =
			div64_u64(ppm_tbl[i].static_pwr + ppm_tbl[i].dyn_pwr,
					ppm_tbl[i].capacity);
		ppm_tbl[i].thermal_efficiency =
			div64_u64(ppm_tbl[i].thermal_static_pwr + ppm_tbl[i].dyn_pwr,
					ppm_tbl[i].capacity);
	}

	cluster->ppm_data.ppm_tbl = ppm_tbl;
	cluster->ppm_data.opp_nr = opp_nr;
	cluster->ppm_data.init = 1;
	cpufreq_cpu_put(policy);

	/* calculate turning point */
	if (cid == AB_CLUSTER_ID) {
		struct cluster_data *prev_cluster = &cluster_state[cid - 1];
		unsigned int turn_point = 0;

		turn_point = find_turn_point(prev_cluster, cluster, false);
		/*
		 * If the turn-point can't be figure out.
		 * Use max capacity of BLCPU as turn-point
		 */
		if (!turn_point)
			turn_point = prev_cluster->ppm_data.ppm_tbl[0].capacity;

		if (turn_point) {
			unsigned int val = 0;

			val = div64_u64(turn_point * 100,
				prev_cluster->ppm_data.ppm_tbl[0].capacity);
			set_up_thres(prev_cluster, val);
		}

		/* thermal case */
		turn_point = find_turn_point(prev_cluster, cluster, true);
		if (!turn_point)
			turn_point = prev_cluster->ppm_data.ppm_tbl[0].capacity;

		if (turn_point) {
			unsigned int val = 0;

			val = div64_u64(turn_point * 100,
				prev_cluster->ppm_data.ppm_tbl[0].capacity);
			if (val <= 100)
				prev_cluster->thermal_up_thres = val;
			pr_info("thermal_turn_pint is %u, thermal_down_thre is change to %u",
				 turn_point, val);
		}
	}
	return 0;
}

static int __init core_ctl_init(void)
{
	int ret, cluster_nr, i, ret_error_line;
	struct cpumask cluster_cpus;

	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
			"core_ctl/cpu_pause:dead",
			NULL, core_ctl_prepare_dead_cpu);

	cpuhp_setup_state(CPUHP_BP_PREPARE_DYN,
			"core_ctl/cpu_pause:online",
			core_ctl_prepare_online_cpu, NULL);

	ret = init_sched_avg();
	if (ret)
		goto failed;

	/* register traceprob */
	ret = register_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
	if (ret) {
		ret_error_line = __LINE__;
		goto failed_exit_sched_avg;
	}

	/* init cluster_data */
	cluster_nr = arch_get_nr_clusters();

	for (i = 0; i < cluster_nr; i++) {
		arch_get_cluster_cpus(&cluster_cpus, i);
		ret = cluster_init(&cluster_cpus);
		if (ret) {
			pr_info("%s: unable to create core ctl group: %d\n", TAG, ret);
			goto failed_deprob;
		}
	}

	initialized = true;
	return 0;

failed_deprob:
	unregister_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
failed_exit_sched_avg:
	exit_sched_avg();
	tracepoint_synchronize_unregister();

failed:
	return ret;
}

static void __exit core_ctl_exit(void)
{
	exit_sched_avg();
	unregister_trace_android_vh_scheduler_tick(core_ctl_tick, NULL);
	tracepoint_synchronize_unregister();
}

module_init(core_ctl_init);
module_exit(core_ctl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mediatek Inc.");
MODULE_DESCRIPTION("Mediatek core_ctl");
