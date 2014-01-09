/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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
 */
/*
 * Qualcomm MSM Runqueue Stats and cpu utilization Interface for Userspace
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/rq_stats.h>
#include <linux/cpufreq.h>
#include <linux/kernel_stat.h>
#include <linux/tick.h>
#include <asm/smp_plat.h>
#include "acpuclock.h"
#include <linux/suspend.h>

#define MAX_LONG_SIZE 24
#define DEFAULT_RQ_POLL_JIFFIES 1
#define DEFAULT_DEF_TIMER_JIFFIES 5

struct notifier_block freq_transition;
struct notifier_block cpu_hotplug;

struct cpu_load_data {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_iowait;
	unsigned int avg_load_maxfreq;
	unsigned int samples;
	unsigned int window_size;
	unsigned int cur_freq;
	unsigned int policy_max;
	cpumask_var_t related_cpus;
	struct mutex cpu_load_mutex;
};

static DEFINE_PER_CPU(struct cpu_load_data, cpuload);

static inline cputime64_t get_cpu_iowait_time(unsigned int cpu,
							cputime64_t *wall)
{
	u64 iowait_time = get_cpu_iowait_time_us(cpu, wall);

	if (iowait_time == -1ULL)
		return 0;

	return iowait_time;
}

static int update_average_load(unsigned int freq, unsigned int cpu)
{

	struct cpu_load_data *pcpu = &per_cpu(cpuload, cpu);
	cputime64_t cur_wall_time, cur_idle_time, cur_iowait_time;
	unsigned int idle_time, wall_time, iowait_time;
	unsigned int cur_load, load_at_max_freq;

	cur_idle_time = get_cpu_idle_time(cpu, &cur_wall_time, 0);
	cur_iowait_time = get_cpu_iowait_time(cpu, &cur_wall_time);

	wall_time = (unsigned int) (cur_wall_time - pcpu->prev_cpu_wall);
	pcpu->prev_cpu_wall = cur_wall_time;

	idle_time = (unsigned int) (cur_idle_time - pcpu->prev_cpu_idle);
	pcpu->prev_cpu_idle = cur_idle_time;

	iowait_time = (unsigned int) (cur_iowait_time - pcpu->prev_cpu_iowait);
	pcpu->prev_cpu_iowait = cur_iowait_time;

	if (idle_time >= iowait_time)
		idle_time -= iowait_time;

	if (unlikely(!wall_time || wall_time < idle_time))
		return 0;

	cur_load = 100 * (wall_time - idle_time) / wall_time;

	/* Calculate the scaled load across CPU */
	load_at_max_freq = (cur_load * freq) / pcpu->policy_max;

	if (!pcpu->avg_load_maxfreq) {
		/* This is the first sample in this window*/
		pcpu->avg_load_maxfreq = load_at_max_freq;
		pcpu->window_size = wall_time;
	} else {
		/*
		 * The is already a sample available in this window.
		 * Compute weighted average with prev entry, so that we get
		 * the precise weighted load.
		 */
		pcpu->avg_load_maxfreq =
			((pcpu->avg_load_maxfreq * pcpu->window_size) +
			(load_at_max_freq * wall_time)) /
			(wall_time + pcpu->window_size);

		pcpu->window_size += wall_time;
	}

	return 0;
}

static unsigned int report_load_at_max_freq(void)
{
	int cpu;
	struct cpu_load_data *pcpu;
	unsigned int total_load = 0;

	for_each_online_cpu(cpu) {
		pcpu = &per_cpu(cpuload, cpu);
		mutex_lock(&pcpu->cpu_load_mutex);
		update_average_load(pcpu->cur_freq, cpu);
		total_load += pcpu->avg_load_maxfreq;
		pcpu->avg_load_maxfreq = 0;
		mutex_unlock(&pcpu->cpu_load_mutex);
	}
	return total_load;
}

static int cpufreq_transition_handler(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct cpufreq_freqs *freqs = data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, freqs->cpu);
	int j;

	switch (val) {
	case CPUFREQ_POSTCHANGE:
		for_each_cpu(j, this_cpu->related_cpus) {
			struct cpu_load_data *pcpu = &per_cpu(cpuload, j);
			mutex_lock(&pcpu->cpu_load_mutex);
			update_average_load(freqs->old, freqs->cpu);
			pcpu->cur_freq = freqs->new;
			mutex_unlock(&pcpu->cpu_load_mutex);
		}
		break;
	}
	return 0;
}

static void update_related_cpus(void)
{
	unsigned cpu;

	for_each_cpu(cpu, cpu_online_mask) {
		struct cpu_load_data *this_cpu = &per_cpu(cpuload, cpu);
		struct cpufreq_policy cpu_policy;

		cpufreq_get_policy(&cpu_policy, cpu);
		cpumask_copy(this_cpu->related_cpus, cpu_policy.cpus);
	}
}
static int cpu_hotplug_handler(struct notifier_block *nb,
			unsigned long val, void *data)
{
	unsigned int cpu = (unsigned long)data;
	struct cpu_load_data *this_cpu = &per_cpu(cpuload, cpu);

	switch (val) {
	case CPU_ONLINE:
		if (!this_cpu->cur_freq)
			this_cpu->cur_freq = acpuclk_get_rate(cpu);
		update_related_cpus();
	case CPU_ONLINE_FROZEN:
		this_cpu->avg_load_maxfreq = 0;
	}

	return NOTIFY_OK;
}

static int system_suspend_handler(struct notifier_block *nb,
				unsigned long val, void *data)
{
	switch (val) {
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
	case PM_POST_RESTORE:
		rq_info.hotplug_disabled = 0;
		break;
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		rq_info.hotplug_disabled = 1;
		break;
	default:
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}


static ssize_t hotplug_disable_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int val = 0;
	val = rq_info.hotplug_disabled;
	return snprintf(buf, MAX_LONG_SIZE, "%d\n", val);
}

static struct kobj_attribute hotplug_disabled_attr = __ATTR_RO(hotplug_disable);

static void def_work_fn(struct work_struct *work)
{
	int64_t diff;

	diff = ktime_to_ns(ktime_get()) - rq_info.def_start_time;
	do_div(diff, 1000 * 1000);
	rq_info.def_interval = (unsigned int) diff;

	/* Notify polling threads on change of value */
	sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
}

static ssize_t run_queue_avg_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int val = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	/* rq avg currently available only on one core */
	val = rq_info.rq_avg;
	rq_info.rq_avg = 0;
	spin_unlock_irqrestore(&rq_lock, flags);

	return snprintf(buf, PAGE_SIZE, "%d.%d\n", val/10, val%10);
}

static struct kobj_attribute run_queue_avg_attr = __ATTR_RO(run_queue_avg);

static ssize_t show_run_queue_poll_ms(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	ret = snprintf(buf, MAX_LONG_SIZE, "%u\n",
		       jiffies_to_msecs(rq_info.rq_poll_jiffies));
	spin_unlock_irqrestore(&rq_lock, flags);

	return ret;
}

static ssize_t store_run_queue_poll_ms(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned int val = 0;
	unsigned long flags = 0;
	static DEFINE_MUTEX(lock_poll_ms);

	mutex_lock(&lock_poll_ms);

	spin_lock_irqsave(&rq_lock, flags);
	sscanf(buf, "%u", &val);
	rq_info.rq_poll_jiffies = msecs_to_jiffies(val);
	spin_unlock_irqrestore(&rq_lock, flags);

	mutex_unlock(&lock_poll_ms);

	return count;
}

static struct kobj_attribute run_queue_poll_ms_attr =
	__ATTR(run_queue_poll_ms, S_IWUSR | S_IRUSR, show_run_queue_poll_ms,
			store_run_queue_poll_ms);

static ssize_t show_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", rq_info.def_interval);
}

static ssize_t store_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	sscanf(buf, "%u", &val);
	rq_info.def_timer_jiffies = msecs_to_jiffies(val);

	rq_info.def_start_time = ktime_to_ns(ktime_get());
	return count;
}

static struct kobj_attribute def_timer_ms_attr =
	__ATTR(def_timer_ms, S_IWUSR | S_IRUSR, show_def_timer_ms,
			store_def_timer_ms);

static ssize_t show_cpu_normalized_load(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, MAX_LONG_SIZE, "%u\n", report_load_at_max_freq());
}

static struct kobj_attribute cpu_normalized_load_attr =
	__ATTR(cpu_normalized_load, S_IWUSR | S_IRUSR, show_cpu_normalized_load,
			NULL);

static struct attribute *rq_attrs[] = {
	&cpu_normalized_load_attr.attr,
	&def_timer_ms_attr.attr,
	&run_queue_avg_attr.attr,
	&run_queue_poll_ms_attr.attr,
	&hotplug_disabled_attr.attr,
	NULL,
};

static struct attribute_group rq_attr_group = {
	.attrs = rq_attrs,
};

static int init_rq_attribs(void)
{
	int err;

	rq_info.rq_avg = 0;
	rq_info.attr_group = &rq_attr_group;

	/* Create /sys/devices/system/cpu/cpu0/rq-stats/... */
	rq_info.kobj = kobject_create_and_add("rq-stats",
			&get_cpu_device(0)->kobj);
	if (!rq_info.kobj)
		return -ENOMEM;

	err = sysfs_create_group(rq_info.kobj, rq_info.attr_group);
	if (err)
		kobject_put(rq_info.kobj);
	else
		kobject_uevent(rq_info.kobj, KOBJ_ADD);

	return err;
}

static int __init msm_rq_stats_init(void)
{
	int ret;
	int i;
	struct cpufreq_policy cpu_policy;

#ifndef CONFIG_SMP
	/* Bail out if this is not an SMP Target */
	rq_info.init = 0;
	return -ENOSYS;
#endif

	rq_wq = create_singlethread_workqueue("rq_stats");
	BUG_ON(!rq_wq);
	INIT_WORK(&rq_info.def_timer_work, def_work_fn);
	spin_lock_init(&rq_lock);
	rq_info.rq_poll_jiffies = DEFAULT_RQ_POLL_JIFFIES;
	rq_info.def_timer_jiffies = DEFAULT_DEF_TIMER_JIFFIES;
	rq_info.rq_poll_last_jiffy = 0;
	rq_info.def_timer_last_jiffy = 0;
	rq_info.hotplug_disabled = 0;
	ret = init_rq_attribs();

	rq_info.init = 1;

	for_each_possible_cpu(i) {
		struct cpu_load_data *pcpu = &per_cpu(cpuload, i);
		mutex_init(&pcpu->cpu_load_mutex);
		cpufreq_get_policy(&cpu_policy, i);
		pcpu->policy_max = cpu_policy.cpuinfo.max_freq;
		if (cpu_online(i))
			pcpu->cur_freq = acpuclk_get_rate(i);
		cpumask_copy(pcpu->related_cpus, cpu_policy.cpus);
	}
	freq_transition.notifier_call = cpufreq_transition_handler;
	cpu_hotplug.notifier_call = cpu_hotplug_handler;
	cpufreq_register_notifier(&freq_transition,
					CPUFREQ_TRANSITION_NOTIFIER);
	register_hotcpu_notifier(&cpu_hotplug);

	return ret;
}
late_initcall(msm_rq_stats_init);

static int __init msm_rq_stats_early_init(void)
{
#ifndef CONFIG_SMP
	/* Bail out if this is not an SMP Target */
	rq_info.init = 0;
	return -ENOSYS;
#endif

	pm_notifier(system_suspend_handler, 0);
	return 0;
}
core_initcall(msm_rq_stats_early_init);
