/* Copyright (c) 2014-2016, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>

#include <trace/events/sched.h>

#define MAX_CPUS_PER_CLUSTER 4
#define MAX_CLUSTERS 2

struct cluster_data {
	bool inited;
	unsigned int min_cpus;
	unsigned int max_cpus;
	unsigned int offline_delay_ms;
	unsigned int busy_up_thres[MAX_CPUS_PER_CLUSTER];
	unsigned int busy_down_thres[MAX_CPUS_PER_CLUSTER];
	unsigned int online_cpus;
	unsigned int avail_cpus;
	unsigned int num_cpus;
	unsigned int need_cpus;
	unsigned int task_thres;
	s64 need_ts;
	struct list_head lru;
	bool pending;
	spinlock_t pending_lock;
	bool is_big_cluster;
	int nrrun;
	bool nrrun_changed;
	struct timer_list timer;
	struct task_struct *hotplug_thread;
	unsigned int first_cpu;
	struct kobject kobj;
};

struct cpu_data {
	bool online;
	bool rejected;
	bool is_busy;
	unsigned int busy;
	unsigned int cpu;
	bool not_preferred;
	struct cluster_data *cluster;
	struct list_head sib;
};

static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static struct cluster_data cluster_state[MAX_CLUSTERS];
static unsigned int num_clusters;

#define for_each_cluster(cluster, idx) \
	for ((cluster) = &cluster_state[idx]; (idx) < num_clusters;\
		(idx)++, (cluster) = &cluster_state[idx])

static DEFINE_SPINLOCK(state_lock);
static void apply_need(struct cluster_data *state);
static void wake_up_hotplug_thread(struct cluster_data *state);

/* ========================= sysfs interface =========================== */

static ssize_t store_min_cpus(struct cluster_data *state,
				const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	state->min_cpus = min(val, state->max_cpus);
	wake_up_hotplug_thread(state);

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

	val = min(val, state->num_cpus);
	state->max_cpus = val;
	state->min_cpus = min(state->min_cpus, state->max_cpus);
	wake_up_hotplug_thread(state);

	return count;
}

static ssize_t show_max_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->max_cpus);
}

static ssize_t store_offline_delay_ms(struct cluster_data *state,
					const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	state->offline_delay_ms = val;
	apply_need(state);

	return count;
}

static ssize_t show_task_thres(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->task_thres);
}

static ssize_t store_task_thres(struct cluster_data *state,
				const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	if (val < state->num_cpus)
		return -EINVAL;

	state->task_thres = val;
	apply_need(state);

	return count;
}

static ssize_t show_offline_delay_ms(const struct cluster_data *state,
				     char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->offline_delay_ms);
}

static ssize_t store_busy_up_thres(struct cluster_data *state,
					const char *buf, size_t count)
{
	unsigned int val[MAX_CPUS_PER_CLUSTER];
	int ret, i;

	ret = sscanf(buf, "%u %u %u %u\n", &val[0], &val[1], &val[2], &val[3]);
	if (ret != 1 && ret != state->num_cpus)
		return -EINVAL;

	if (ret == 1) {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_up_thres[i] = val[0];
	} else {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_up_thres[i] = val[i];
	}
	apply_need(state);
	return count;
}

static ssize_t show_busy_up_thres(const struct cluster_data *state, char *buf)
{
	int i, count = 0;

	for (i = 0; i < state->num_cpus; i++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%u ",
				  state->busy_up_thres[i]);

	count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	return count;
}

static ssize_t store_busy_down_thres(struct cluster_data *state,
					const char *buf, size_t count)
{
	unsigned int val[MAX_CPUS_PER_CLUSTER];
	int ret, i;

	ret = sscanf(buf, "%u %u %u %u\n", &val[0], &val[1], &val[2], &val[3]);
	if (ret != 1 && ret != state->num_cpus)
		return -EINVAL;

	if (ret == 1) {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_down_thres[i] = val[0];
	} else {
		for (i = 0; i < state->num_cpus; i++)
			state->busy_down_thres[i] = val[i];
	}
	apply_need(state);
	return count;
}

static ssize_t show_busy_down_thres(const struct cluster_data *state, char *buf)
{
	int i, count = 0;

	for (i = 0; i < state->num_cpus; i++)
		count += snprintf(buf + count, PAGE_SIZE - count, "%u ",
				  state->busy_down_thres[i]);

	count += snprintf(buf + count, PAGE_SIZE - count, "\n");
	return count;
}

static ssize_t store_is_big_cluster(struct cluster_data *state,
				const char *buf, size_t count)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;

	state->is_big_cluster = val ? 1 : 0;
	return count;
}

static ssize_t show_is_big_cluster(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->is_big_cluster);
}

static ssize_t show_cpus(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	ssize_t count = 0;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry(c, &state->lru, sib) {
		count += snprintf(buf + count, PAGE_SIZE - count,
				  "CPU%u (%s)\n", c->cpu,
				  c->online ? "Online" : "Offline");
	}
	spin_unlock_irqrestore(&state_lock, flags);
	return count;
}

static ssize_t show_need_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->need_cpus);
}

static ssize_t show_online_cpus(const struct cluster_data *state, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", state->online_cpus);
}

static ssize_t show_global_state(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	struct cluster_data *cluster;
	ssize_t count = 0;
	unsigned int cpu;

	for_each_possible_cpu(cpu) {
		count += snprintf(buf + count, PAGE_SIZE - count,
					"CPU%u\n", cpu);
		c = &per_cpu(cpu_state, cpu);
		cluster = c->cluster;
		if (!cluster || !cluster->inited)
			continue;

		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tCPU: %u\n", c->cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tOnline: %u\n", c->online);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tRejected: %u\n", c->rejected);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tFirst CPU: %u\n",
						cluster->first_cpu);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tBusy%%: %u\n", c->busy);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tIs busy: %u\n", c->is_busy);
		count += snprintf(buf + count, PAGE_SIZE - count,
					"\tNr running: %u\n", cluster->nrrun);
		count += snprintf(buf + count, PAGE_SIZE - count,
			"\tAvail CPUs: %u\n", cluster->avail_cpus);
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tNeed CPUs: %u\n", cluster->need_cpus);
	}

	return count;
}

static ssize_t store_not_preferred(struct cluster_data *state,
				   const char *buf, size_t count)
{
	struct cpu_data *c;
	unsigned int i;
	unsigned int val[MAX_CPUS_PER_CLUSTER];
	unsigned long flags;
	int ret;

	ret = sscanf(buf, "%u %u %u %u\n", &val[0], &val[1], &val[2], &val[3]);
	if (ret != 1 && ret != state->num_cpus)
		return -EINVAL;

	i = 0;
	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry(c, &state->lru, sib)
		c->not_preferred = val[i++];
	spin_unlock_irqrestore(&state_lock, flags);

	return count;
}

static ssize_t show_not_preferred(const struct cluster_data *state, char *buf)
{
	struct cpu_data *c;
	ssize_t count = 0;
	unsigned long flags;

	spin_lock_irqsave(&state_lock, flags);
	list_for_each_entry(c, &state->lru, sib)
		count += snprintf(buf + count, PAGE_SIZE - count,
				"\tCPU:%d %u\n", c->cpu, c->not_preferred);
	spin_unlock_irqrestore(&state_lock, flags);

	return count;
}


struct core_ctl_attr {
	struct attribute attr;
	ssize_t (*show)(const struct cluster_data *, char *);
	ssize_t (*store)(struct cluster_data *, const char *, size_t count);
};

#define core_ctl_attr_ro(_name)		\
static struct core_ctl_attr _name =	\
__ATTR(_name, 0444, show_##_name, NULL)

#define core_ctl_attr_rw(_name)			\
static struct core_ctl_attr _name =		\
__ATTR(_name, 0644, show_##_name, store_##_name)

core_ctl_attr_rw(min_cpus);
core_ctl_attr_rw(max_cpus);
core_ctl_attr_rw(offline_delay_ms);
core_ctl_attr_rw(busy_up_thres);
core_ctl_attr_rw(busy_down_thres);
core_ctl_attr_rw(task_thres);
core_ctl_attr_rw(is_big_cluster);
core_ctl_attr_ro(cpus);
core_ctl_attr_ro(need_cpus);
core_ctl_attr_ro(online_cpus);
core_ctl_attr_ro(global_state);
core_ctl_attr_rw(not_preferred);

static struct attribute *default_attrs[] = {
	&min_cpus.attr,
	&max_cpus.attr,
	&offline_delay_ms.attr,
	&busy_up_thres.attr,
	&busy_down_thres.attr,
	&task_thres.attr,
	&is_big_cluster.attr,
	&cpus.attr,
	&need_cpus.attr,
	&online_cpus.attr,
	&global_state.attr,
	&not_preferred.attr,
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
	.show	= show,
	.store	= store,
};

static struct kobj_type ktype_core_ctl = {
	.sysfs_ops	= &sysfs_ops,
	.default_attrs	= default_attrs,
};

/* ==================== runqueue based core count =================== */

#define RQ_AVG_TOLERANCE 2
#define RQ_AVG_DEFAULT_MS 20
#define NR_RUNNING_TOLERANCE 5
static unsigned int rq_avg_period_ms = RQ_AVG_DEFAULT_MS;

static s64 rq_avg_timestamp_ms;
static struct timer_list rq_avg_timer;

static void update_running_avg(bool trigger_update)
{
	int avg, iowait_avg, big_avg, old_nrrun;
	s64 now;
	unsigned long flags;
	struct cluster_data *cluster;
	unsigned int index = 0;

	spin_lock_irqsave(&state_lock, flags);

	now = ktime_to_ms(ktime_get());
	if (now - rq_avg_timestamp_ms < rq_avg_period_ms - RQ_AVG_TOLERANCE) {
		spin_unlock_irqrestore(&state_lock, flags);
		return;
	}
	rq_avg_timestamp_ms = now;
	sched_get_nr_running_avg(&avg, &iowait_avg, &big_avg);

	spin_unlock_irqrestore(&state_lock, flags);

	/*
	 * Round up to the next integer if the average nr running tasks
	 * is within NR_RUNNING_TOLERANCE/100 of the next integer.
	 * If normal rounding up is used, it will allow a transient task
	 * to trigger online event. By the time core is onlined, the task
	 * has finished.
	 * Rounding to closest suffers same problem because scheduler
	 * might only provide running stats per jiffy, and a transient
	 * task could skew the number for one jiffy. If core control
	 * samples every 2 jiffies, it will observe 0.5 additional running
	 * average which rounds up to 1 task.
	 */
	avg = (avg + NR_RUNNING_TOLERANCE) / 100;
	big_avg = (big_avg + NR_RUNNING_TOLERANCE) / 100;

	for_each_cluster(cluster, index) {
		if (!cluster->inited)
			continue;
		old_nrrun = cluster->nrrun;
		/*
		 * Big cluster only need to take care of big tasks, but if
		 * there are not enough big cores, big tasks need to be run
		 * on little as well. Thus for little's runqueue stat, it
		 * has to use overall runqueue average, or derive what big
		 * tasks would have to be run on little. The latter approach
		 * is not easy to get given core control reacts much slower
		 * than scheduler, and can't predict scheduler's behavior.
		 */
		cluster->nrrun = cluster->is_big_cluster ? big_avg : avg;
		if (cluster->nrrun != old_nrrun) {
			if (trigger_update)
				apply_need(cluster);
			else
				cluster->nrrun_changed = true;
		}
	}
	return;
}

/* adjust needed CPUs based on current runqueue information */
static unsigned int apply_task_need(const struct cluster_data *cluster,
				    unsigned int new_need)
{
	/* Online all cores if there are enough tasks */
	if (cluster->nrrun >= cluster->task_thres)
		return cluster->num_cpus;

	/* only online more cores if there are tasks to run */
	if (cluster->nrrun > new_need)
		return new_need + 1;

	return new_need;
}

static u64 round_to_nw_start(void)
{
	unsigned long step = msecs_to_jiffies(rq_avg_period_ms);
	u64 jif = get_jiffies_64();

	do_div(jif, step);
	return (jif + 1) * step;
}

static void rq_avg_timer_func(unsigned long not_used)
{
	update_running_avg(true);
	mod_timer(&rq_avg_timer, round_to_nw_start());
}

/* ======================= load based core count  ====================== */

static unsigned int apply_limits(const struct cluster_data *cluster,
				 unsigned int need_cpus)
{
	return min(max(cluster->min_cpus, need_cpus), cluster->max_cpus);
}

static bool eval_need(struct cluster_data *cluster)
{
	unsigned long flags;
	struct cpu_data *c;
	unsigned int need_cpus = 0, last_need, thres_idx;
	int ret = 0;
	bool need_flag = false;
	s64 now;

	if (unlikely(!cluster->inited))
		return 0;

	spin_lock_irqsave(&state_lock, flags);
	thres_idx = cluster->online_cpus ? cluster->online_cpus - 1 : 0;
	list_for_each_entry(c, &cluster->lru, sib) {
		if (c->busy >= cluster->busy_up_thres[thres_idx])
			c->is_busy = true;
		else if (c->busy < cluster->busy_down_thres[thres_idx])
			c->is_busy = false;
		need_cpus += c->is_busy;
	}
	need_cpus = apply_task_need(cluster, need_cpus);
	need_flag = apply_limits(cluster, need_cpus) !=
				apply_limits(cluster, cluster->need_cpus);
	last_need = cluster->need_cpus;

	now = ktime_to_ms(ktime_get());

	if (need_cpus == last_need) {
		cluster->need_ts = now;
		spin_unlock_irqrestore(&state_lock, flags);
		return 0;
	}

	if (need_cpus > last_need) {
		ret = 1;
	} else if (need_cpus < last_need) {
		s64 elapsed = now - cluster->need_ts;

		if (elapsed >= cluster->offline_delay_ms) {
			ret = 1;
		} else {
			mod_timer(&cluster->timer, jiffies +
				  msecs_to_jiffies(cluster->offline_delay_ms));
		}
	}

	if (ret) {
		cluster->need_ts = now;
		cluster->need_cpus = need_cpus;
	}

	trace_core_ctl_eval_need(cluster->first_cpu, last_need, need_cpus,
				 ret && need_flag);
	spin_unlock_irqrestore(&state_lock, flags);

	return ret && need_flag;
}

static void apply_need(struct cluster_data *cluster)
{
	if (eval_need(cluster))
		wake_up_hotplug_thread(cluster);
}

static int core_ctl_set_busy(unsigned int cpu, unsigned int busy)
{
	struct cpu_data *c = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = c->cluster;
	unsigned int old_is_busy = c->is_busy;

	if (!cluster || !cluster->inited)
		return 0;

	update_running_avg(false);
	if (c->busy == busy && !cluster->nrrun_changed)
		return 0;
	c->busy = busy;
	cluster->nrrun_changed = false;

	apply_need(cluster);
	trace_core_ctl_set_busy(cpu, busy, old_is_busy, c->is_busy);
	return 0;
}

/* ========================= core count enforcement ==================== */

/*
 * If current thread is hotplug thread, don't attempt to wake up
 * itself or other hotplug threads because it will deadlock. Instead,
 * schedule a timer to fire in next timer tick and wake up the thread.
 */
static void wake_up_hotplug_thread(struct cluster_data *cluster)
{
	unsigned long flags;
	bool no_wakeup = false;
	struct cluster_data *cls;
	unsigned long index = 0;

	for_each_cluster(cls, index) {
		if (cls->hotplug_thread == current) {
			no_wakeup = true;
			break;
		}
	}

	spin_lock_irqsave(&cluster->pending_lock, flags);
	cluster->pending = true;
	spin_unlock_irqrestore(&cluster->pending_lock, flags);

	if (no_wakeup) {
		spin_lock_irqsave(&state_lock, flags);
		mod_timer(&cluster->timer, jiffies);
		spin_unlock_irqrestore(&state_lock, flags);
	} else {
		wake_up_process(cluster->hotplug_thread);
	}
}

static void core_ctl_timer_func(unsigned long data)
{
	struct cluster_data *cluster = (struct cluster_data *) data;

	if (eval_need(cluster)) {
		unsigned long flags;

		spin_lock_irqsave(&cluster->pending_lock, flags);
		cluster->pending = true;
		spin_unlock_irqrestore(&cluster->pending_lock, flags);
		wake_up_process(cluster->hotplug_thread);
	}

}

static int core_ctl_online_core(unsigned int cpu)
{
	int ret;
	struct device *dev;

	lock_device_hotplug();
	dev = get_cpu_device(cpu);
	if (!dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__, cpu);
		ret = -ENODEV;
	} else {
		ret = device_online(dev);
	}
	unlock_device_hotplug();
	return ret;
}

static int core_ctl_offline_core(unsigned int cpu)
{
	int ret;
	struct device *dev;

	lock_device_hotplug();
	dev = get_cpu_device(cpu);
	if (!dev) {
		pr_err("%s: failed to get cpu%d device\n", __func__, cpu);
		ret = -ENODEV;
	} else {
		ret = device_offline(dev);
	}
	unlock_device_hotplug();
	return ret;
}

static void __ref do_hotplug(struct cluster_data *cluster)
{
	unsigned int need;
	struct cpu_data *c, *tmp;

	need = apply_limits(cluster, cluster->need_cpus);
	pr_debug("Trying to adjust group %u to %u\n", cluster->first_cpu, need);

	if (cluster->online_cpus > need) {
		list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
			if (!c->online)
				continue;

			if (cluster->online_cpus == need)
				break;

			/* Don't offline busy CPUs. */
			if (c->is_busy)
				continue;

			pr_debug("Trying to Offline CPU%u\n", c->cpu);
			if (core_ctl_offline_core(c->cpu))
				pr_debug("Unable to Offline CPU%u\n", c->cpu);
		}

		/*
		 * If the number of online CPUs is within the limits, then
		 * don't force any busy CPUs offline.
		 */
		if (cluster->online_cpus <= cluster->max_cpus)
			return;

		list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
			if (!c->online)
				continue;

			if (cluster->online_cpus <= cluster->max_cpus)
				break;

			pr_debug("Trying to Offline CPU%u\n", c->cpu);
			if (core_ctl_offline_core(c->cpu))
				pr_debug("Unable to Offline CPU%u\n", c->cpu);
		}
	} else if (cluster->online_cpus < need) {
		list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
			if (c->online || c->rejected || c->not_preferred)
				continue;
			if (cluster->online_cpus == need)
				break;

			pr_debug("Trying to Online CPU%u\n", c->cpu);
			if (core_ctl_online_core(c->cpu))
				pr_debug("Unable to Online CPU%u\n", c->cpu);
		}

		if (cluster->online_cpus == need)
			return;


		list_for_each_entry_safe(c, tmp, &cluster->lru, sib) {
			if (c->online || c->rejected || !c->not_preferred)
				continue;
			if (cluster->online_cpus == need)
				break;

			pr_debug("Trying to Online CPU%u\n", c->cpu);
			if (core_ctl_online_core(c->cpu))
				pr_debug("Unable to Online CPU%u\n", c->cpu);
		}

	}
}

static int __ref try_hotplug(void *data)
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

		do_hotplug(cluster);
	}

	return 0;
}

static int __ref cpu_callback(struct notifier_block *nfb,
				unsigned long action, void *hcpu)
{
	uint32_t cpu = (uintptr_t)hcpu;
	struct cpu_data *state = &per_cpu(cpu_state, cpu);
	struct cluster_data *cluster = state->cluster;
	int ret = NOTIFY_OK;
	unsigned long flags;

	/* Don't affect suspend resume */
	if (action & CPU_TASKS_FROZEN)
		return NOTIFY_OK;

	if (unlikely(!cluster || !cluster->inited))
		return NOTIFY_OK;

	switch (action) {
	case CPU_UP_PREPARE:

		/* If online state of CPU somehow got out of sync, fix it. */
		if (state->online) {
			cluster->online_cpus--;
			state->online = false;
			pr_warn("CPU%d offline when state is online\n", cpu);
		}

		if (state->rejected) {
			state->rejected = false;
			cluster->avail_cpus++;
		}

		/*
		 * If a CPU is in the process of coming up, mark it as online
		 * so that there's no race with hotplug thread bringing up more
		 * CPUs than necessary.
		 */
		if (apply_limits(cluster, cluster->need_cpus) <=
				 cluster->online_cpus) {
			pr_debug("Prevent CPU%d onlining\n", cpu);
			ret = NOTIFY_BAD;
		} else {
			state->online = true;
			cluster->online_cpus++;
		}
		break;

	case CPU_ONLINE:
		/*
		 * Moving to the end of the list should only happen in
		 * CPU_ONLINE and not on CPU_UP_PREPARE to prevent an
		 * infinite list traversal when thermal (or other entities)
		 * reject trying to online CPUs.
		 */
		spin_lock_irqsave(&state_lock, flags);
		list_del(&state->sib);
		list_add_tail(&state->sib, &cluster->lru);
		spin_unlock_irqrestore(&state_lock, flags);
		break;

	case CPU_DEAD:
		/* Move a CPU to the end of the LRU when it goes offline. */
		spin_lock_irqsave(&state_lock, flags);
		list_del(&state->sib);
		list_add_tail(&state->sib, &cluster->lru);
		spin_unlock_irqrestore(&state_lock, flags);

		/* Fall through */

	case CPU_UP_CANCELED:

		/* If online state of CPU somehow got out of sync, fix it. */
		if (!state->online) {
			cluster->online_cpus++;
			pr_warn("CPU%d online when state is offline\n", cpu);
		}

		if (!state->rejected && action == CPU_UP_CANCELED) {
			state->rejected = true;
			cluster->avail_cpus--;
		}

		state->online = false;
		state->busy = 0;
		cluster->online_cpus--;
		break;
	}

	if (cluster->online_cpus < apply_limits(cluster, cluster->need_cpus)
	    && cluster->online_cpus < cluster->avail_cpus
	    && action == CPU_DEAD)
		wake_up_hotplug_thread(cluster);

	return ret;
}

static struct notifier_block __refdata cpu_notifier = {
	.notifier_call = cpu_callback,
};

/* ============================ init code ============================== */

static struct cluster_data *find_cluster_by_first_cpu(unsigned int first_cpu)
{
	unsigned int i;

	for (i = 0; i < num_clusters; ++i) {
		if (cluster_state[i].first_cpu == first_cpu)
			return &cluster_state[i];
	}

	return NULL;
}

static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	unsigned int cpu;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	if (find_cluster_by_first_cpu(first_cpu))
		return 0;

	dev = get_cpu_device(first_cpu);
	if (!dev)
		return -ENODEV;

	pr_info("Creating CPU group %d\n", first_cpu);

	if (num_clusters == MAX_CLUSTERS) {
		pr_err("Unsupported number of clusters. Only %u supported\n",
								MAX_CLUSTERS);
		return -EINVAL;
	}
	cluster = &cluster_state[num_clusters];
	++num_clusters;

	cluster->num_cpus = cpumask_weight(mask);
	if (cluster->num_cpus > MAX_CPUS_PER_CLUSTER) {
		pr_err("HW configuration not supported\n");
		return -EINVAL;
	}
	cluster->first_cpu = first_cpu;
	cluster->min_cpus = 1;
	cluster->max_cpus = cluster->num_cpus;
	cluster->need_cpus = cluster->num_cpus;
	cluster->avail_cpus  = cluster->num_cpus;
	cluster->offline_delay_ms = 100;
	cluster->task_thres = UINT_MAX;
	cluster->nrrun = cluster->num_cpus;
	INIT_LIST_HEAD(&cluster->lru);
	init_timer(&cluster->timer);
	spin_lock_init(&cluster->pending_lock);
	cluster->timer.function = core_ctl_timer_func;
	cluster->timer.data = (unsigned long) cluster;

	for_each_cpu(cpu, mask) {
		pr_info("Init CPU%u state\n", cpu);

		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
		if (cpu_online(cpu)) {
			cluster->online_cpus++;
			state->online = true;
		}
		list_add_tail(&state->sib, &cluster->lru);
	}

	cluster->hotplug_thread = kthread_run(try_hotplug, (void *) cluster,
					"core_ctl/%d", first_cpu);
	sched_setscheduler_nocheck(cluster->hotplug_thread, SCHED_FIFO,
				   &param);

	cluster->inited = true;

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

static int cpufreq_policy_cb(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_policy *policy = data;

	switch (val) {
	case CPUFREQ_CREATE_POLICY:
		cluster_init(policy->related_cpus);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_pol_nb = {
	.notifier_call = cpufreq_policy_cb,
};

static int cpufreq_gov_cb(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_govinfo *info = data;

	switch (val) {
	case CPUFREQ_LOAD_CHANGE:
		core_ctl_set_busy(info->cpu, info->load);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block cpufreq_gov_nb = {
	.notifier_call = cpufreq_gov_cb,
};

static int __init core_ctl_init(void)
{
	struct cpufreq_policy *policy;
	unsigned int cpu;

	register_cpu_notifier(&cpu_notifier);
	cpufreq_register_notifier(&cpufreq_pol_nb, CPUFREQ_POLICY_NOTIFIER);
	cpufreq_register_notifier(&cpufreq_gov_nb, CPUFREQ_GOVINFO_NOTIFIER);
	init_timer_deferrable(&rq_avg_timer);
	rq_avg_timer.function = rq_avg_timer_func;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (policy) {
			cluster_init(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
	put_online_cpus();
	mod_timer(&rq_avg_timer, round_to_nw_start());
	return 0;
}

late_initcall(core_ctl_init);
