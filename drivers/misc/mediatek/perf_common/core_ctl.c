// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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

#ifndef __CHECKER__
#define CREATE_TRACE_POINTS
#include <core_ctl_trace.h>
#endif

#include <mt-plat/core_ctl.h>
#ifdef CONFIG_MTK_CPU_FREQ
#include <mt-plat/mtk_cpufreq_common_api.h>
#endif
#include <mtk_ppm_api.h>
#include <rq_stats.h>
#include <core_ctl_internal.h>

#define TAG "core_ctl"

struct cluster_data {
	bool inited;
	bool enable;
	unsigned int cluster_id;
	unsigned int min_cpus;
	unsigned int max_cpus;
	unsigned int first_cpu;
	unsigned int active_cpus;
	unsigned int num_cpus;
	int nr_up;
	int nr_down;
	int max_nr;
	unsigned int nr_assist;
	unsigned int btask_up_thresh;
	unsigned int btask_down_thresh;
	unsigned int cpu_tj_degree;
	unsigned int cpu_tj_btask_thresh;
	unsigned int nr_not_preferred_cpus;
	unsigned int need_cpus;
	unsigned int new_need_cpus;
	unsigned int boost;
	unsigned int nr_isolated_cpus;
	cpumask_t cpu_mask;
	bool pending;
	spinlock_t pending_lock;
	struct task_struct *core_ctl_thread;
	struct kobject kobj;
	s64 offline_throttle_ms;
	s64 next_offline_time;
};

struct cpu_data {
	unsigned int cpu;
	struct cluster_data *cluster;
	bool not_preferred;
	bool iso_by_core_ctl;
};

#define ENABLE	1
#define DISABLE	0
#define L_CLUSTER_ID	0
#define BL_CLUSTER_ID	1
#define AB_CLUSTER_ID	2
#define CORE_OFF	true
#define CORE_ON		false
#define MAX_CLUSTERS	3
#define MAX_CPUS_PER_CLUSTER	6
#define MAX_NR_DOWN_THRESHOLD	4
/* reference cpu_ctrl.h */
#define CPU_KIR_CORE_CTL	11
#define MAX_BTASK_THRESH	100
#define MAX_CPU_TJ_DEGREE	100000

static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static struct cluster_data cluster_state[MAX_CLUSTERS];
static unsigned int num_clusters;

static bool debug_enable;
module_param_named(debug_enable, debug_enable, bool, 0600);

#define for_each_cluster(cluster, idx) \
	for ((cluster) = &cluster_state[idx]; (idx) < num_clusters;\
		(idx)++, (cluster) = &cluster_state[idx])

#define core_ctl_debug(x...)		\
	do {				\
		if (debug_enable)	\
			pr_info(x);	\
	} while (0)

static DEFINE_SPINLOCK(state_lock);
static bool initialized;
static unsigned int default_min_cpus[MAX_CLUSTERS] = {4, 2, 0};

static int arch_get_nr_clusters(void)
{
	return arch_nr_clusters();
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

static unsigned int apply_limits(const struct cluster_data *cluster,
		unsigned int need_cpus)
{
	return min(max(cluster->min_cpus, need_cpus), cluster->max_cpus);
}

/* TODO: if HOTPLUG ? */
int sched_isolate_count(const cpumask_t *mask, bool include_offline)
{
	cpumask_t count_mask = CPU_MASK_NONE;

	if (include_offline) {
		cpumask_complement(&count_mask, cpu_online_mask);
		cpumask_or(&count_mask, &count_mask, cpu_isolated_mask);
		cpumask_and(&count_mask, &count_mask, mask);
	} else {
		cpumask_and(&count_mask, mask, cpu_isolated_mask);
	}

	return cpumask_weight(&count_mask);
}

static unsigned int get_active_cpu_count(const struct cluster_data *cluster)
{
	return cluster->num_cpus -
		sched_isolate_count(&cluster->cpu_mask, true);
}

static bool is_active(const struct cpu_data *state)
{
	return cpu_online(state->cpu) && !cpu_isolated(state->cpu);
}

static bool adjustment_possible(const struct cluster_data *cluster,
		unsigned int need)
{
	return (need < cluster->active_cpus || (need > cluster->active_cpus &&
				cluster->nr_isolated_cpus));
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

	if (cluster->boost || !cluster->enable)
		need_cpus = cluster->max_cpus;
	else
		need_cpus = cluster->new_need_cpus;

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
		 * If no more CPUs are needed or isolated,
		 * just update the next offline time.
		 */
		/* TODO: should consider that new_need == last_need ? */
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
void update_next_cluster_down_thresh(unsigned int index,
				     unsigned int new_thresh)
{
	struct cluster_data *next_cluster;

	if (index == num_clusters - 1)
		return;

	next_cluster = &cluster_state[index + 1];
	next_cluster->btask_down_thresh = new_thresh;
}

static inline
void update_prev_cluster_up_thresh(unsigned int index,
				   unsigned int new_thresh)
{
	struct cluster_data *prev_cluster;

	if (index == 0)
		return;

	prev_cluster = &cluster_state[index - 1];
	prev_cluster->btask_up_thresh = new_thresh;
}

static inline
void set_not_preferred_locked(struct cluster_data *cluster, int cpu, bool enable)
{
	struct cpu_data *c;
	bool changed = false;

	c = &per_cpu(cpu_state, cpu);
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

static void set_btask_up_thresh(struct cluster_data *cluster, unsigned int val)
{
	unsigned int old_thresh;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	old_thresh = cluster->btask_up_thresh;
	cluster->btask_up_thresh = val;

	if (old_thresh != cluster->btask_up_thresh) {
		update_next_cluster_down_thresh(
				cluster->cluster_id,
				cluster->btask_up_thresh);
		set_overutil_threshold(cluster->cluster_id,
				       cluster->btask_up_thresh);
	}
	spin_unlock_irqrestore(&state_lock, flags);
}

static inline
void set_cpu_tj_degree(struct cluster_data *cluster, int degree)
{
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	cluster->cpu_tj_degree = degree;
	spin_unlock_irqrestore(&state_lock, flags);
}

static inline
void set_cpu_tj_btask_thresh(struct cluster_data *cluster, unsigned int val)
{
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	cluster->cpu_tj_btask_thresh = val;
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
	int ret = 0;
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

/*
 *  core_ctl_set_not_preferred - set not_prefer for the specific cpu number
 *  @cid: cluster id
 *  @cpu: cpu number
 *  @enable: true if set, false if unset.
 *
 *  return 0 if success, else return errno
 */
extern int core_ctl_set_not_preferred(int cid, int cpu, bool enable)
{
	struct cluster_data *cluster;
	unsigned long flags;

	if (cid >= num_clusters)
		return -EINVAL;

	cluster = &cluster_state[cid];

	if (!cpumask_test_cpu(cpu, &cluster->cpu_mask))
		return -EINVAL;

	spin_lock_irqsave(&state_lock, flags);
	set_not_preferred_locked(cluster, cpu, enable);
	spin_unlock_irqrestore(&state_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(core_ctl_set_not_preferred);

int core_ctl_set_btask_up_thresh(int cid, unsigned int val)
{
	struct cluster_data *cluster;

	if (cid >= num_clusters)
		return -EINVAL;

	/* Range of up thrash should be 0 - 100 */
	if (val > MAX_BTASK_THRESH)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_btask_up_thresh(cluster, val);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_btask_up_thresh);

int core_ctl_set_cpu_tj_degree(int cid, unsigned int degree)
{
	struct cluster_data *cluster;

	if (cid != AB_CLUSTER_ID)
		return -EINVAL;

	/* tempature <= 100 degree */
	if (degree > MAX_CPU_TJ_DEGREE)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_cpu_tj_degree(cluster, degree);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_tj_degree);

int core_ctl_set_cpu_tj_btask_thresh(int cid, unsigned int val)
{
	struct cluster_data *cluster;

	/* only allow AB cluster */
	if (cid != AB_CLUSTER_ID)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		return -EINVAL;

	cluster = &cluster_state[cid];
	set_cpu_tj_btask_thresh(cluster, val);
	return 0;
}
EXPORT_SYMBOL(core_ctl_set_cpu_tj_btask_thresh);

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
	return snprintf(buf, PAGE_SIZE, "%lld\n", state->offline_throttle_ms);
}

static ssize_t store_btask_up_thresh(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* No need to change up_thresh for the last cluster */
	if (state->cluster_id == num_clusters-1)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		return -EINVAL;

	set_btask_up_thresh(state, val);
	return count;
}

static ssize_t show_btask_up_thresh(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->btask_up_thresh);
}

static ssize_t store_btask_down_thresh(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;
	unsigned int old_thresh;
	unsigned long flags;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* No need to change down_thresh for min cluster */
	if (state->cluster_id == 0)
		return -EINVAL;

	spin_lock_irqsave(&state_lock, flags);
	old_thresh = state->btask_down_thresh;
	state->btask_down_thresh =
		(val <= 100) ? val : state->btask_down_thresh;

	if (old_thresh != state->btask_down_thresh) {
		update_prev_cluster_up_thresh(state->cluster_id,
					state->btask_down_thresh);
		set_overutil_threshold(state->cluster_id-1,
					state->btask_down_thresh);
	}
	spin_unlock_irqrestore(&state_lock, flags);
	return count;
}

static ssize_t show_btask_down_thresh(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->btask_down_thresh);
}

static ssize_t store_cpu_tj_degree(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* only allow AB cluster */
	if (state->cluster_id != AB_CLUSTER_ID)
		return -EINVAL;

	/* tempature <= 100 degree */
	if (val > MAX_CPU_TJ_DEGREE)
		return -EINVAL;

	set_cpu_tj_degree(state, val);
	return count;
}

static ssize_t show_cpu_tj_degree(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->cpu_tj_degree);
}

static ssize_t store_cpu_tj_btask_thresh(struct cluster_data *state,
		const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	/* only allow AB cluster */
	if (state->cluster_id != AB_CLUSTER_ID)
		return -EINVAL;

	if (val > MAX_BTASK_THRESH)
		return -EINVAL;

	set_cpu_tj_btask_thresh(state, val);
	return count;
}

static ssize_t show_cpu_tj_btask_thresh(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->cpu_tj_btask_thresh);
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
		set_not_preferred_locked(state, i + state->first_cpu, val[i]);
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

static ssize_t show_need_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->need_cpus);
}

static ssize_t show_active_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->active_cpus);
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

		count += snprintf(buf + count, PAGE_SIZE - count,
				"CPU%u\n", cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tCPU: %u\n", c->cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tOnline: %u\n",
				cpu_online(c->cpu));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tIsolated: %u\n",
				cpu_isolated(c->cpu));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tIsolated by core_ctl: %u\n",
				c->iso_by_core_ctl);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tFirst CPU: %u\n",
				cluster->first_cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tActive CPUs: %u\n",
						get_active_cpu_count(cluster));
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tNeed CPUs: %u\n", cluster->need_cpus);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tNr isolated CPUs(isolated by core_ctl): %u\n",
						cluster->nr_isolated_cpus);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tNot preferred: %u\n",
					c->not_preferred);
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
core_ctl_attr_rw(btask_up_thresh);
core_ctl_attr_rw(btask_down_thresh);
core_ctl_attr_rw(cpu_tj_degree);
core_ctl_attr_rw(cpu_tj_btask_thresh);
core_ctl_attr_rw(not_preferred);
core_ctl_attr_rw(core_ctl_boost);
core_ctl_attr_rw(enable);
core_ctl_attr_ro(need_cpus);
core_ctl_attr_ro(active_cpus);
core_ctl_attr_ro(global_state);

static struct attribute *default_attrs[] = {
	&min_cpus.attr,
	&max_cpus.attr,
	&offline_throttle_ms.attr,
	&btask_up_thresh.attr,
	&btask_down_thresh.attr,
	&cpu_tj_degree.attr,
	&cpu_tj_btask_thresh.attr,
	&not_preferred.attr,
	&core_ctl_boost.attr,
	&enable.attr,
	&need_cpus.attr,
	&active_cpus.attr,
	&global_state.attr,
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

static unsigned int thermal_btask_thresh = 512;
static unsigned int btask_thresh = 230;

/* ==================== algorithm of core control ======================== */

#define BIG_TASK_AVG_THRESHOLD 25
/*
 * Rewrite from sched_big_task_nr()
 */
void get_nr_running_big_task(struct cluster_data *cluster)
{
	int avg_down[MAX_CLUSTERS] = {0};
	int avg_up[MAX_CLUSTERS] = {0};
	int nr_up[MAX_CLUSTERS] = {0};
	int nr_down[MAX_CLUSTERS] = {0};
	int max_nr[MAX_CLUSTERS] = {0};
	int i, delta;

	for (i = 0; i < num_clusters; i++) {
		sched_get_nr_overutil_avg(i,
					  &avg_down[i],
					  &avg_up[i],
					  &nr_down[i],
					  &nr_up[i],
					  &max_nr[i]);
		cluster[i].max_nr = max_nr[i];
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
					(avg_down[i]-avg_up[i]) / BIG_TASK_AVG_THRESHOLD > delta ?
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
		max_nr[i] = cluster[i].max_nr;
	}
	trace_core_ctl_update_nr_btask(nr_up, nr_down, max_nr);
}

/*
 * prev_cluster_nr_assist:
 *   Tasks that are eligible to run on the previous
 *   cluster but cannot run because of insufficient
 *   CPUs there. It's indicative of number of CPUs
 *   in this this cluster that should assist its
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

void core_ctl_tick(u64 wallclock)
{
	unsigned int max_util = 0;
	unsigned int index = 0;
	unsigned int ts_cpu7;
	unsigned int need_cpus[MAX_CLUSTERS] = {0};
	unsigned long flags;
	struct cluster_data *cluster;
	struct cluster_data *ab_cluster;
	struct cluster_data *prev_ab_cluster;

	sched_max_util_task(NULL, NULL, &max_util, NULL);
	ts_cpu7 = get_immediate_tslvts1_1_wrap();

	spin_lock_irqsave(&state_lock, flags);
	get_nr_running_big_task(cluster_state);
	for_each_cluster(cluster, index) {
		int new_need_cpus = 0;

		if (index == 0) {
			cluster->nr_assist = cluster->nr_up;
			cluster->new_need_cpus = cluster->num_cpus;
			/* for high TLP with tiny tasks */
			if (cluster->max_nr > MAX_NR_DOWN_THRESHOLD)
				cluster->nr_assist += 1;
			continue;
		}

		new_need_cpus += cluster->nr_up;
		new_need_cpus += cluster->nr_down;
		new_need_cpus += get_prev_cluster_nr_assist(index);

		/* for high TLP with tiny tasks */
		if (cluster->max_nr > MAX_NR_DOWN_THRESHOLD)
			new_need_cpus += 1;

		cluster->new_need_cpus = new_need_cpus;

		/* nr_assist(i) = max(0, new_need_cpus(i) - max_cpus(i)) */
		cluster->nr_assist =
		   (new_need_cpus > cluster->max_cpus ?
		    (new_need_cpus - cluster->max_cpus) : 0);
	}

	/*
	 *  Ensure prime cpu make core-on if
	 *  biggest task is over threshold.
	 */
	if (is_cluster_init(AB_CLUSTER_ID)) {
		ab_cluster = &cluster_state[AB_CLUSTER_ID];
		prev_ab_cluster = &cluster_state[AB_CLUSTER_ID-1];

		/*
		 * Check for biggest task in system,
		 * if it's over thresh, force to enable
		 * prime core.
		 */
		btask_thresh =
		  (unsigned int)(1024 * ab_cluster->btask_down_thresh)/100;

		thermal_btask_thresh =
		  (unsigned int)(1024 * ab_cluster->cpu_tj_btask_thresh)/100;

		if (ts_cpu7 > ab_cluster->cpu_tj_degree)
			btask_thresh =
				thermal_btask_thresh;

		if (!ab_cluster->new_need_cpus &&
			max_util > btask_thresh) {
			/* assume max_cpus of ab_cluster is 1 */
			ab_cluster->new_need_cpus = ab_cluster->max_cpus;
			/*
			 * Consider TLP=1 super btask, prefer to
			 * using AB CPU to replace a BL CPU to save
			 * power consumption
			 */
			if (likely(prev_ab_cluster->new_need_cpus >=
					ab_cluster->new_need_cpus)) {
				prev_ab_cluster->new_need_cpus
					-= ab_cluster->new_need_cpus;
			}
		}
	}
	spin_unlock_irqrestore(&state_lock, flags);

	/* reset index value */
	index = 0;
	for_each_cluster(cluster, index) {
		need_cpus[index] = cluster->new_need_cpus;
		apply_demand(cluster);
	}

	trace_core_ctl_info(ts_cpu7, btask_thresh, max_util,
			cpumask_bits(cpu_isolated_mask)[0], need_cpus);
}

#define CPU_FREQ_LOWEST_OPP	15
/* tri-cluster at most */
static struct ppm_limit_data cpu_freq[] = {
	{.min = -1, .max = -1},
	{.min = -1, .max = -1},
	{.min = -1, .max = -1},
};
static DEFINE_MUTEX(cpu_freq_lock);

static void check_cpu_freq(struct cluster_data *cluster, bool cluster_on)
{
	unsigned int cid = cluster->cluster_id;
	bool active_cpu = !!cluster->active_cpus;

	mutex_lock(&cpu_freq_lock);

	if (!cluster_on) {
		/* no anymore active_cpus */
		if (!active_cpu) {
			unsigned int target_freq = -1;

			/*
			 * when all CPUs in the cluster are isolated,
			 * limit to lowest frequency.
			 */
			target_freq =
				mt_cpufreq_get_freq_by_idx(
						cid,
						CPU_FREQ_LOWEST_OPP);
			cpu_freq[cid].min = target_freq;
			cpu_freq[cid].max = target_freq;
			core_ctl_debug("%s: cluster_id = %u target_freq = %u KHz core_on = %s",
					TAG, cid, target_freq,
					cluster_on ? "true" : "false");
		}
	/* cluster_on */
	} else {
		cpu_freq[cid].min = -1;
		cpu_freq[cid].max = -1;
	}

	update_userlimit_cpu_freq(CPU_KIR_CORE_CTL, num_clusters, cpu_freq);

	if (cluster_on) {
		/*
		 * If not a lowest cluster, choose a CPU frequency
		 * OPP which close to lower cluster
		 */
		if (cluster->cluster_id > 0) {
			unsigned int highest_freq = 0;
			unsigned int cur_freq;
			unsigned int target_freq;
			unsigned int index = 0;
			struct cluster_data *lower_cluster;

			for_each_cluster(lower_cluster, index) {
				if (index == cluster->cluster_id)
					break;

				cur_freq = mt_cpufreq_get_cur_freq(index);
				if (cur_freq > highest_freq)
					highest_freq = cur_freq;
			}

			target_freq =
				mt_cpufreq_find_close_freq(
						cid, highest_freq);
			mt_cpufreq_set_by_schedule_load_cluster(
					cid, target_freq);
			core_ctl_debug("%s: cluster_id:%u target_freq: %u KHz core_on = %s",
					TAG, cid, target_freq,
					cluster_on ? "true" : "false");
		}
	}
	mutex_unlock(&cpu_freq_lock);
}

static void try_to_isolate(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus;
	unsigned int nr_isolated = 0;
	int cpu;
	bool success;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;

again:
	nr_isolated = 0;
	spin_lock_irqsave(&state_lock, flags);
	for (cpu = nr_cpu_ids-1; cpu > -1; cpu--) {
		struct cpu_data *c;

		success = false;
		if (!cpumask_test_cpu(cpu, &cluster->cpu_mask))
			continue;

		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);
		if (!is_active(c))
			continue;

		if (cluster->active_cpus == need)
			break;

		/*
		 * Isolate only the not_preferred CPUs.
		 * If none of the CPUs are selected as not_preferred,
		 * then all CPUs are eligible for isolation.
		 */
		if (check_not_prefer && !c->not_preferred)
			continue;

		spin_unlock_irqrestore(&state_lock, flags);

		core_ctl_debug("%s: Trying to isolate CPU%u\n", TAG, c->cpu);
		if (!sched_isolate_cpu(c->cpu)) {
			success = true;
			nr_isolated++;
		} else {
			core_ctl_debug("%s Unable to isolate CPU%u\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&state_lock, flags);
		if (success)
			c->iso_by_core_ctl = true;
		cluster->active_cpus = get_active_cpu_count(cluster);
	}
	cluster->nr_isolated_cpus += nr_isolated;
	spin_unlock_irqrestore(&state_lock, flags);
	/*
	 * If the number of not prefer CPUs is not
	 * equal to need CPUs, then check it again.
	 */
	if (check_not_prefer &&
		cluster->active_cpus != need) {
		num_cpus = cluster->num_cpus;
		check_not_prefer = false;
		goto again;
	}
#ifdef CONFIG_MTK_CPU_FREQ
	check_cpu_freq(cluster, false);
#endif
}

static void try_to_unisolate(struct cluster_data *cluster, int need)
{
	unsigned long flags;
	unsigned int num_cpus = cluster->num_cpus, cpu;
	unsigned int nr_unisolated = 0;
	bool cluster_on_possible = false;
	bool check_not_prefer = cluster->nr_not_preferred_cpus;
	bool success;

again:
	nr_unisolated = 0;
	spin_lock_irqsave(&state_lock, flags);
	for_each_cpu(cpu, &cluster->cpu_mask) {
		struct cpu_data *c;

		success = false;
		if (!num_cpus--)
			break;

		c = &per_cpu(cpu_state, cpu);

		if (!c->iso_by_core_ctl)
			continue;

		if ((cpu_online(c->cpu) && !cpu_isolated(c->cpu)))
			continue;

		if (cluster->active_cpus == need)
			break;

		/* The Normal CPUs are de-isolated prior to not prefer CPUs */
		if (!check_not_prefer && c->not_preferred)
			continue;

		spin_unlock_irqrestore(&state_lock, flags);

		core_ctl_debug("%s: Trying to unisolate CPU%u\n", TAG, c->cpu);
		if (!sched_unisolate_cpu(c->cpu)) {
			success = true;
			nr_unisolated++;
		} else {
			core_ctl_debug("%s: Unable to unisolate CPU%u\n", TAG, c->cpu);
		}
		spin_lock_irqsave(&state_lock, flags);
		if (success) {
			/* first CPU online */
			if (!cluster->active_cpus)
				cluster_on_possible = true;
			c->iso_by_core_ctl = false;
		}
		cluster->active_cpus = get_active_cpu_count(cluster);
		/* check again */
		if (!cluster->active_cpus)
			cluster_on_possible = false;
	}
	cluster->nr_isolated_cpus -= nr_unisolated;
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
#ifdef CONFIG_MTK_CPU_FREQ
	check_cpu_freq(cluster, cluster_on_possible);
#endif
}

static void __ref do_core_ctl(struct cluster_data *cluster)
{
	unsigned int need;

	need = apply_limits(cluster, cluster->need_cpus);

	if (adjustment_possible(cluster, need)) {
		core_ctl_debug("%s: Trying to adjust group %u from %u to %u\n",
				TAG, cluster->first_cpu,
				cluster->active_cpus, need);

		if (cluster->active_cpus > need)
			try_to_isolate(cluster, need);
		else if (cluster->active_cpus < need)
			try_to_unisolate(cluster, need);
	} else
		core_ctl_debug("%s: failed to adjust group %u from %u to %u.  need_cpus=%u min_cpus=%u max_cpus=%u\n",
		TAG, cluster->first_cpu, cluster->active_cpus, need,
		cluster->need_cpus, cluster->min_cpus, cluster->max_cpus);
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

static int isolation_cpuhp_state(unsigned int cpu,  bool online)
{
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = state->cluster;
	unsigned int need;
	bool do_wakeup = false, unisolated = false;
	unsigned long flags;

	if (unlikely(!cluster || !cluster->inited))
		return 0;

	spin_lock_irqsave(&state_lock, flags);
	if (online) {
		cluster->active_cpus = get_active_cpu_count(cluster);
	/* cpu_online() is false */
	} else {
		/*
		 * When CPU is offline, CPU should be un-isolated.
		 * Thus, un-isolate this CPU that is going down if
		 * it was isolated by core_ctl.
		 */
		if (state->iso_by_core_ctl) {
			state->iso_by_core_ctl = false;
			cluster->nr_isolated_cpus--;
			unisolated = true;
		}
		cluster->active_cpus = get_active_cpu_count(cluster);
	}

	need = apply_limits(cluster, cluster->need_cpus);
	do_wakeup = adjustment_possible(cluster, need);
	spin_unlock_irqrestore(&state_lock, flags);
	if (unisolated)
		sched_unisolate_cpu_unlocked(cpu);
	if (do_wakeup)
		wake_up_core_ctl_thread(cluster);

	return 0;
}

static int core_ctl_isolation_online_cpu(unsigned int cpu)
{
	return isolation_cpuhp_state(cpu, true);
}

static int core_ctl_isolation_dead_cpu(unsigned int cpu)
{
	return isolation_cpuhp_state(cpu, false);
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

static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	unsigned int cpu;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };


	/* first_cpu is defined */
	if (find_cluster_by_first_cpu(first_cpu))
		return 0;

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
	cpu_freq[cluster->cluster_id].min = -1;
	cpu_freq[cluster->cluster_id].max = -1;
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
		cluster->btask_up_thresh = INT_MAX;
	else
		cluster->btask_up_thresh =
			get_overutil_threshold(cluster->cluster_id);

	if (cluster->cluster_id == 0)
		cluster->btask_down_thresh = INT_MAX;
	else
		cluster->btask_down_thresh =
			get_overutil_threshold(cluster->cluster_id - 1);

	if (cluster->cluster_id == AB_CLUSTER_ID) {
		cluster->cpu_tj_degree = 65000;
		cluster->cpu_tj_btask_thresh = 70;
	} else {
		cluster->cpu_tj_degree = INT_MAX;
		cluster->cpu_tj_btask_thresh = INT_MAX;
	}

	cluster->nr_not_preferred_cpus = 0;
	spin_lock_init(&cluster->pending_lock);

	for_each_cpu(cpu, mask) {
		core_ctl_debug("%s: Init CPU%u state\n", TAG, cpu);
		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
	}

	cluster->next_offline_time =
		ktime_to_ms(ktime_get()) + cluster->offline_throttle_ms;
	cluster->active_cpus = get_active_cpu_count(cluster);

	cluster->core_ctl_thread = kthread_run(try_core_ctl, (void *) cluster,
			"core_ctl_v1/%d", first_cpu);
	if (IS_ERR(cluster->core_ctl_thread))
		return PTR_ERR(cluster->core_ctl_thread);

	sched_setscheduler_nocheck(cluster->core_ctl_thread, SCHED_FIFO,
			&param);

	cluster->inited = true;

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

static int __init core_ctl_init(void)
{
	int ret, cluster_nr, i;
	struct cpumask cluster_cpus;

	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN,
			"core_ctl/isolation:online",
			core_ctl_isolation_online_cpu, NULL);

	cpuhp_setup_state_nocalls(CPUHP_CORE_CTL_ISOLATION_DEAD,
			"core_ctl/isolation:dead",
			NULL, core_ctl_isolation_dead_cpu);

	/* init cluster_data */
	cluster_nr = arch_get_nr_clusters();

	for (i = 0; i < cluster_nr; i++) {
		arch_get_cluster_cpus(&cluster_cpus, i);
		ret = cluster_init(&cluster_cpus);
		if (ret)
			pr_info("%s: unable to create core ctl group: %d\n", TAG, ret);
	}

	initialized = true;
	return 0;
}
late_initcall(core_ctl_init);
