// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023, Unisoc (shanghai) Technologies Co., Ltd
 */

#define pr_fmt(fmt)	"core_ctl: " fmt

#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/syscore_ops.h>
#include <uapi/linux/sched/types.h>

#include "uni_sched.h"
#include "trace.h"

#define MAX_CPUS_PER_CLUSTER 6
#define MAX_CLUSTERS 3

struct cluster_data {
	bool			inited;
	unsigned int		min_cpus;
	unsigned int		max_cpus;
	unsigned int		active_cpus;
	unsigned int		num_cpus;
	cpumask_t		cpu_mask;
	unsigned int		need_cpus;
	unsigned int		max_nr;
	struct list_head	lru;
	unsigned int		first_cpu;
	struct kobject		kobj;
};

struct cpu_data {
	unsigned int		cpu;
	struct cluster_data	*cluster;
	struct list_head	sib;
	bool			disabled;
};

static DEFINE_PER_CPU(struct cpu_data, cpu_state);
static struct cluster_data cluster_state[MAX_CLUSTERS];
static unsigned int num_clusters;

#define for_each_cluster(cluster, idx) \
	for (; (idx) < num_clusters && ((cluster) = &cluster_state[idx]);\
		idx++)

static DEFINE_SPINLOCK(state_lock);
static bool initialized;

static unsigned int get_active_cpu_count(const struct cluster_data *cluster);

static ssize_t store_bcl_pause_cpu(struct cluster_data *state,
					const char *buf, size_t count)
{
	int ret = -EINVAL;
	unsigned int val;
	cpumask_t cpus;
	unsigned long flags;

	if (kstrtouint(buf, 10, &val))
		return ret;

	if (val <= 1 && state->first_cpu == 0)
		return ret;

	spin_lock_irqsave(&state_lock, flags);
	if (val >= state->first_cpu &&
	    val <= state->first_cpu + state->num_cpus - 1) {
		cpumask_clear(&cpus);
		cpumask_set_cpu(val, &cpus);
		ret = pause_cpus(&cpus, PAUSE_HYP);
		if (ret)
			goto fail;

		spin_unlock_irqrestore(&state_lock, flags);
		return count;
	}

fail:
	spin_unlock_irqrestore(&state_lock, flags);
	return -EAGAIN;
}

static ssize_t store_bcl_resume_cpu(struct cluster_data *state,
					const char *buf, size_t count)
{
	int ret = -EINVAL;
	unsigned int val;
	cpumask_t cpus;
	unsigned long flags;

	if (kstrtouint(buf, 10, &val))
		return ret;

	spin_lock_irqsave(&state_lock, flags);
	if (val >= state->first_cpu &&
	    val <= state->first_cpu + state->num_cpus - 1) {
		cpumask_clear(&cpus);
		cpumask_set_cpu(val, &cpus);
		ret = resume_cpus(&cpus, PAUSE_HYP);
		if (ret)
			goto fail;

		spin_unlock_irqrestore(&state_lock, flags);
		return count;
	}

fail:
	spin_unlock_irqrestore(&state_lock, flags);
	return -EAGAIN;
}

static ssize_t store_pause_cpu(struct cluster_data *state,
					const char *buf, size_t count)
{
	int ret = -EINVAL;
	unsigned int val;
	cpumask_t cpus;
	unsigned long flags;

	if (kstrtouint(buf, 10, &val))
		return ret;

	if (val <= 1 && state->first_cpu == 0)
		return ret;

	spin_lock_irqsave(&state_lock, flags);
	if (val >= state->first_cpu &&
	    val <= state->first_cpu + state->num_cpus - 1) {
		cpumask_clear(&cpus);
		cpumask_set_cpu(val, &cpus);
		ret = pause_cpus(&cpus, PAUSE_CORE_CTL);
		if (ret)
			goto fail;

		spin_unlock_irqrestore(&state_lock, flags);
		return count;
	}

fail:
	spin_unlock_irqrestore(&state_lock, flags);
	return -EAGAIN;
}

static ssize_t store_resume_cpu(struct cluster_data *state,
					const char *buf, size_t count)
{
	int ret = -EINVAL;
	unsigned int val;
	cpumask_t cpus;
	unsigned long flags;

	if (kstrtouint(buf, 10, &val))
		return ret;

	spin_lock_irqsave(&state_lock, flags);
	if (val >= state->first_cpu &&
	    val <= state->first_cpu + state->num_cpus - 1) {
		cpumask_clear(&cpus);
		cpumask_set_cpu(val, &cpus);
		ret = resume_cpus(&cpus, PAUSE_CORE_CTL);
		if (ret)
			goto fail;

		spin_unlock_irqrestore(&state_lock, flags);
		return count;
	}

fail:
	spin_unlock_irqrestore(&state_lock, flags);
	return -EAGAIN;

}

static ssize_t show_active_cpus(const struct cluster_data *state, char *buf)
{
	int active_cpus = get_active_cpu_count(state);

	return scnprintf(buf, PAGE_SIZE, "%u\n", active_cpus);
}

static ssize_t show_active_cpu(const struct cluster_data *state, char *buf)
{
	cpumask_t cpus;

	cpumask_andnot(&cpus, &state->cpu_mask, cpu_halt_mask);

	return scnprintf(buf, PAGE_SIZE, "%*pbl\n", cpumask_pr_args(&cpus));
}

static unsigned int cluster_paused_cpus(const struct cluster_data *cluster)
{
	cpumask_t cluster_paused_cpus;

	cpumask_and(&cluster_paused_cpus, &cluster->cpu_mask, cpu_halt_mask);
	return cpumask_weight(&cluster_paused_cpus);
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

		count += scnprintf(buf + count, PAGE_SIZE - count,
					"CPU%u\n", cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tCPU: %u\n", c->cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tOnline: %u\n",
					cpu_online(c->cpu));
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tPaused: %u\n",
					cpu_halted(c->cpu));
		count += scnprintf(buf + count, PAGE_SIZE - count,
					"\tFirst CPU: %u\n",
						cluster->first_cpu);
		count += scnprintf(buf + count, PAGE_SIZE - count,
			"\tActive CPUs: %u\n", get_active_cpu_count(cluster));
		count += scnprintf(buf + count, PAGE_SIZE - count,
				"\tCluster paused CPUs: %u\n",
				   cluster_paused_cpus(cluster));
	}
	spin_unlock_irq(&state_lock);

	return count;
}

struct core_ctl_attr {
	struct attribute	attr;
	ssize_t			(*show)(const struct cluster_data *cd, char *c);
	ssize_t			(*store)(struct cluster_data *cd, const char *c,
							size_t count);
};

#define core_ctl_attr_ro(_name)		\
static struct core_ctl_attr _name =	\
__ATTR(_name, 0444, show_##_name, NULL)

#define core_ctl_attr_rw(_name)			\
static struct core_ctl_attr _name =		\
__ATTR(_name, 0644, show_##_name, store_##_name)

#define core_ctl_attr_wo(_name)		\
static struct core_ctl_attr _name =	\
__ATTR(_name, 0200, NULL, store_##_name)

core_ctl_attr_ro(active_cpu);
core_ctl_attr_ro(active_cpus);
core_ctl_attr_ro(global_state);
core_ctl_attr_wo(pause_cpu);
core_ctl_attr_wo(resume_cpu);
core_ctl_attr_wo(bcl_pause_cpu);
core_ctl_attr_wo(bcl_resume_cpu);

static struct attribute *default_attrs[] = {
	&active_cpu.attr,
	&active_cpus.attr,
	&global_state.attr,
	&pause_cpu.attr,
	&resume_cpu.attr,
	&bcl_pause_cpu.attr,
	&bcl_resume_cpu.attr,
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

static unsigned int get_active_cpu_count(const struct cluster_data *cluster)
{
	cpumask_t cpus;

	cpumask_andnot(&cpus, &cluster->cpu_mask, cpu_halt_mask);
	return cpumask_weight(&cpus);
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

static int cluster_init(const struct cpumask *mask)
{
	struct device *dev;
	unsigned int first_cpu = cpumask_first(mask);
	struct cluster_data *cluster;
	struct cpu_data *state;
	unsigned int cpu;

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

	cpumask_copy(&cluster->cpu_mask, mask);
	cluster->num_cpus = cpumask_weight(mask);
	if (cluster->num_cpus > MAX_CPUS_PER_CLUSTER) {
		pr_err("HW configuration not supported\n");
		return -EINVAL;
	}
	cluster->first_cpu = first_cpu;
	cluster->min_cpus = cluster->num_cpus;
	cluster->max_cpus = cluster->num_cpus;
	cluster->need_cpus = cluster->num_cpus;
	INIT_LIST_HEAD(&cluster->lru);

	for_each_cpu(cpu, mask) {
		pr_info("Init CPU%u state\n", cpu);

		state = &per_cpu(cpu_state, cpu);
		state->cluster = cluster;
		state->cpu = cpu;
		state->disabled = get_cpu_device(cpu) &&
				get_cpu_device(cpu)->offline_disabled;
		list_add_tail(&state->sib, &cluster->lru);
	}
	cluster->active_cpus = get_active_cpu_count(cluster);

	cluster->inited = true;

	kobject_init(&cluster->kobj, &ktype_core_ctl);
	return kobject_add(&cluster->kobj, &dev->kobj, "core_ctl");
}

int core_ctl_init(void)
{
	struct sched_cluster *cluster;
	int ret;

	for_each_sched_cluster(cluster) {
		ret = cluster_init(&cluster->cpus);
		if (ret)
			pr_warn("unable to create core ctl group: %d\n", ret);
	}

	initialized = true;

	return 0;
}
