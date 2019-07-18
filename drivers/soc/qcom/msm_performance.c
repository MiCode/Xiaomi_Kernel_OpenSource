/*
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <linux/moduleparam.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <trace/events/power.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>

/* To handle cpufreq min/max request */
struct cpu_status {
	unsigned int min;
	unsigned int max;
};
static DEFINE_PER_CPU(struct cpu_status, cpu_stats);

struct events {
	spinlock_t cpu_hotplug_lock;
	bool cpu_hotplug;
	bool init_success;
};
static struct events events_group;
static struct task_struct *events_notify_thread;

struct cluster_freqinfo {
	unsigned int max_freq;
	unsigned int min_freq;
};

struct highfreq_monitor_s {
	spinlock_t spinlock;
	struct timer_list timer;
	struct cluster_freqinfo bcluster;
	struct cluster_freqinfo lcluster;
	unsigned long hfreq_max_duration;
	int active;
	int monitor_policy;
	int enable;
	int count;
	int frist_bigcore;
	struct timeval tv;
};
static struct highfreq_monitor_s highfreq_monitor;
static struct work_struct highfreq_monitor_work;

static int is_bcore(int cpu)
{
	return cpu >= (highfreq_monitor.frist_bigcore);
}

static int is_high_cpufreq(int cpu, unsigned int cpufreq)
{
	struct cluster_freqinfo *freqinfo;

	freqinfo = &highfreq_monitor.lcluster;
	if (is_bcore(cpu))
		freqinfo = &highfreq_monitor.bcluster;

	return  cpufreq >= freqinfo->max_freq;
}

static void monitor_work_func(struct work_struct *work)
{
	int i,  ret = 0;
	struct cpufreq_policy policy;
	struct cpu_status *i_cpu_stats;

	get_online_cpus();
	for_each_online_cpu(i) {
		i_cpu_stats = &per_cpu(cpu_stats, i);
		if (cpufreq_get_policy(&policy, i))
			continue;

		if (cpu_online(i) && (policy.min != i_cpu_stats->min)) {
			ret = cpufreq_update_policy(i);
			if (ret)
				continue;
		}
	}
	put_online_cpus();
}

static void highfreq_monitor_func(unsigned long data)
{
	int i;
	struct cpu_status *i_cpu_stats;
	unsigned long alarm_interval = highfreq_monitor.hfreq_max_duration;

	if (!highfreq_monitor.enable)
		return;

	highfreq_monitor.count++;
	if (!highfreq_monitor.monitor_policy) {
		printk("high freq duration to long... start time %lu, %lu\n",
				highfreq_monitor.tv.tv_sec, highfreq_monitor.tv.tv_usec);
		if (alarm_interval < 5)
			alarm_interval = 5;

		highfreq_monitor.timer.expires = jiffies + alarm_interval * HZ;
		add_timer(&highfreq_monitor.timer);
	} else if (highfreq_monitor.monitor_policy) {
		for (i = 0; i < num_present_cpus(); i++) {
			i_cpu_stats = &per_cpu(cpu_stats, i);
			i_cpu_stats->min = 0;
		}
		highfreq_monitor.active = 0;
		printk("NOTICE: cpu_min_freq reset to 0 now\n");
		queue_work_on(0, system_long_wq, &highfreq_monitor_work);
	}
}

static int init_highfreq_monitor(struct highfreq_monitor_s *highfreq_monitor)
{
	int i;
	struct cpufreq_policy policy;
	unsigned int max_freq_on_lcluster = 0;
	spin_lock_init(&highfreq_monitor->spinlock);
	highfreq_monitor->timer.function = highfreq_monitor_func;
	highfreq_monitor->hfreq_max_duration = 180;
	highfreq_monitor->active = 0;
	highfreq_monitor->tv.tv_sec = 0;
	highfreq_monitor->tv.tv_usec = 0;
	init_timer(&highfreq_monitor->timer);
	highfreq_monitor->enable = 1;
	highfreq_monitor->monitor_policy = 1;
	highfreq_monitor->count = 0;
	highfreq_monitor->frist_bigcore = 0;
	INIT_WORK(&highfreq_monitor_work, monitor_work_func);

	get_online_cpus();
	for_each_online_cpu(i) {
		if (cpufreq_get_policy(&policy, i))
			continue;

		if (i == 0) {
			highfreq_monitor->lcluster.max_freq = policy.cpuinfo.max_freq;
			highfreq_monitor->lcluster.min_freq = policy.cpuinfo.min_freq;
			max_freq_on_lcluster                = highfreq_monitor->lcluster.max_freq;
		} else {
			if (!highfreq_monitor->frist_bigcore && policy.cpuinfo.max_freq > max_freq_on_lcluster)
				highfreq_monitor->frist_bigcore = i;
			highfreq_monitor->bcluster.max_freq = policy.cpuinfo.max_freq;
			highfreq_monitor->bcluster.min_freq = policy.cpuinfo.min_freq;
		}
	}
	put_online_cpus();

	printk("high freq monitor init:b-%d %d l-%d %d big %d\n",
			highfreq_monitor->bcluster.max_freq,
			highfreq_monitor->bcluster.min_freq,
			highfreq_monitor->lcluster.max_freq,
			highfreq_monitor->lcluster.min_freq,
			highfreq_monitor->frist_bigcore);
	return 0;
}

static int set_highfreq_monitor(const char *buf, const struct kernel_param *kp)
{
	unsigned long val1;
	int val2, val3;

	if (sscanf(buf, "%lu:%d:%d", &val1, &val2, &val3) != 3) {
		printk("ERROR, input valid %s", __func__);
		return -EINVAL;
	}
	printk("set hfreq_max_duration as %lu s\n", val1);
	printk("set monitor policy as %d \n", val2);
	printk("set hfreq_enable as %s \n:", val3 ? "true" : "false");

	highfreq_monitor. hfreq_max_duration = val1;
	highfreq_monitor. monitor_policy = val2;
	highfreq_monitor. enable = val3;

	return 0;
}

static int get_highfreq_monitor(char *buf, const struct kernel_param *kp)
{

	return snprintf(buf, PAGE_SIZE,
			"max_duration:%lu \npolicy %s \nenable %d \nstart:%lu,%lu, counts: %d\n",
	highfreq_monitor.hfreq_max_duration,
	highfreq_monitor.monitor_policy ? "force reset minfreq to 0" : "only printk to warnning",
	highfreq_monitor.enable, highfreq_monitor.tv.tv_sec, highfreq_monitor.tv.tv_usec,
	highfreq_monitor.count);
}

static const struct kernel_param_ops param_ops_highfreq_monitor = {
	.set = set_highfreq_monitor,
	.get = get_highfreq_monitor,
};
module_param_cb(highfreq_monitor, &param_ops_highfreq_monitor, NULL, 0644);
/**************************sysfs start********************************/
/*
 * Userspace sends cpu#:min_freq_value to vote for min_freq_value as the new
 * scaling_min. To withdraw its vote it needs to enter cpu#:0
 */
static int set_cpu_min_freq(const char *buf, const struct kernel_param *kp)
{
	int i, j, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;
	struct cpu_status *i_cpu_stats;
	struct cpufreq_policy policy;
	cpumask_var_t limit_mask;
	int ret;
	unsigned long flags;
	int do_timer = 0;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	cpumask_clear(limit_mask);
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > (num_present_cpus() - 1))
			return -EINVAL;

		i_cpu_stats = &per_cpu(cpu_stats, cpu);
		if (highfreq_monitor.enable) {
			if (is_high_cpufreq(cpu, val))
				do_timer = 1; /*add timer*/
			 else if (!is_high_cpufreq(cpu, val)) {
				do_timer = 2; /*delate timer*/
			}
		}

		i_cpu_stats->min = val;
		cpumask_set_cpu(cpu, limit_mask);

		cp = strnchr(cp, strlen(cp), ' ');
		cp++;
	}


	if (do_timer) {
		spin_lock_irqsave(&highfreq_monitor.spinlock, flags);
		if (do_timer == 1) {
			if (highfreq_monitor.active)
				del_timer(&highfreq_monitor.timer);

			highfreq_monitor.timer.expires = jiffies +
				highfreq_monitor.hfreq_max_duration * HZ;
			add_timer(&highfreq_monitor.timer);
			highfreq_monitor.active = 1;
			do_gettimeofday(&highfreq_monitor.tv);
		} else if (do_timer == 2 && highfreq_monitor.active) {
			del_timer(&highfreq_monitor.timer);
			highfreq_monitor.active = 0;
		}
		spin_unlock_irqrestore(&highfreq_monitor.spinlock, flags);
	}
	/*
	 * Since on synchronous systems policy is shared amongst multiple
	 * CPUs only one CPU needs to be updated for the limit to be
	 * reflected for the entire cluster. We can avoid updating the policy
	 * of other CPUs in the cluster once it is done for at least one CPU
	 * in the cluster
	 */
	get_online_cpus();
	for_each_cpu(i, limit_mask) {
		i_cpu_stats = &per_cpu(cpu_stats, i);

		if (cpufreq_get_policy(&policy, i))
			continue;

		if (cpu_online(i) && (policy.min != i_cpu_stats->min)) {
			ret = cpufreq_update_policy(i);
			if (ret)
				continue;
		}
		for_each_cpu(j, policy.related_cpus)
			cpumask_clear_cpu(j, limit_mask);
	}
	put_online_cpus();

	return 0;
}

static int get_cpu_min_freq(char *buf, const struct kernel_param *kp)
{
	int cnt = 0, cpu;

	for_each_present_cpu(cpu) {
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, per_cpu(cpu_stats, cpu).min);
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static const struct kernel_param_ops param_ops_cpu_min_freq = {
	.set = set_cpu_min_freq,
	.get = get_cpu_min_freq,
};
module_param_cb(cpu_min_freq, &param_ops_cpu_min_freq, NULL, 0644);

/*
 * Userspace sends cpu#:max_freq_value to vote for max_freq_value as the new
 * scaling_max. To withdraw its vote it needs to enter cpu#:UINT_MAX
 */
static int set_cpu_max_freq(const char *buf, const struct kernel_param *kp)
{
	int i, j, ntokens = 0;
	unsigned int val, cpu;
	const char *cp = buf;
	struct cpu_status *i_cpu_stats;
	struct cpufreq_policy policy;
	cpumask_var_t limit_mask;
	int ret;

	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	/* CPU:value pair */
	if (!(ntokens % 2))
		return -EINVAL;

	cp = buf;
	cpumask_clear(limit_mask);
	for (i = 0; i < ntokens; i += 2) {
		if (sscanf(cp, "%u:%u", &cpu, &val) != 2)
			return -EINVAL;
		if (cpu > (num_present_cpus() - 1))
			return -EINVAL;

		i_cpu_stats = &per_cpu(cpu_stats, cpu);

		i_cpu_stats->max = val;
		cpumask_set_cpu(cpu, limit_mask);

		cp = strnchr(cp, strlen(cp), ' ');
		cp++;
	}

	get_online_cpus();
	for_each_cpu(i, limit_mask) {
		i_cpu_stats = &per_cpu(cpu_stats, i);
		if (cpufreq_get_policy(&policy, i))
			continue;

		if (cpu_online(i) && (policy.max != i_cpu_stats->max)) {
			ret = cpufreq_update_policy(i);
			if (ret)
				continue;
		}
		for_each_cpu(j, policy.related_cpus)
			cpumask_clear_cpu(j, limit_mask);
	}
	put_online_cpus();

	return 0;
}

static int get_cpu_max_freq(char *buf, const struct kernel_param *kp)
{
	int cnt = 0, cpu;

	for_each_present_cpu(cpu) {
		cnt += snprintf(buf + cnt, PAGE_SIZE - cnt,
				"%d:%u ", cpu, per_cpu(cpu_stats, cpu).max);
	}
	cnt += snprintf(buf + cnt, PAGE_SIZE - cnt, "\n");
	return cnt;
}

static const struct kernel_param_ops param_ops_cpu_max_freq = {
	.set = set_cpu_max_freq,
	.get = get_cpu_max_freq,
};
module_param_cb(cpu_max_freq, &param_ops_cpu_max_freq, NULL, 0644);

/* CPU Hotplug */
static struct kobject *events_kobj;

static ssize_t show_cpu_hotplug(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "\n");
}
static struct kobj_attribute cpu_hotplug_attr =
__ATTR(cpu_hotplug, 0444, show_cpu_hotplug, NULL);

static struct attribute *events_attrs[] = {
	&cpu_hotplug_attr.attr,
	NULL,
};

static struct attribute_group events_attr_group = {
	.attrs = events_attrs,
};
/*******************************sysfs ends************************************/


static int perf_adjust_notify(struct notifier_block *nb, unsigned long val,
							void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct cpu_status *cpu_st = &per_cpu(cpu_stats, cpu);
	unsigned int min = cpu_st->min, max = cpu_st->max;


	if (val != CPUFREQ_ADJUST)
		return NOTIFY_OK;

	pr_debug("msm_perf: CPU%u policy before: %u:%u kHz\n", cpu,
						policy->min, policy->max);
	pr_debug("msm_perf: CPU%u seting min:max %u:%u kHz\n", cpu, min, max);

	cpufreq_verify_within_limits(policy, min, max);

	pr_debug("msm_perf: CPU%u policy after: %u:%u kHz\n", cpu,
						policy->min, policy->max);

	return NOTIFY_OK;
}

static struct notifier_block perf_cpufreq_nb = {
	.notifier_call = perf_adjust_notify,
};

static int hotplug_notify(unsigned int cpu)
{
	unsigned long flags;

	if (events_group.init_success) {
		spin_lock_irqsave(&(events_group.cpu_hotplug_lock), flags);
		events_group.cpu_hotplug = true;
		spin_unlock_irqrestore(&(events_group.cpu_hotplug_lock), flags);
		wake_up_process(events_notify_thread);
	}

	return 0;
}

static int events_notify_userspace(void *data)
{
	unsigned long flags;
	bool notify_change;

	while (1) {

		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&(events_group.cpu_hotplug_lock), flags);

		if (!events_group.cpu_hotplug) {
			spin_unlock_irqrestore(&(events_group.cpu_hotplug_lock),
									flags);

			schedule();
			if (kthread_should_stop())
				break;
			spin_lock_irqsave(&(events_group.cpu_hotplug_lock),
									flags);
		}

		set_current_state(TASK_RUNNING);
		notify_change = events_group.cpu_hotplug;
		events_group.cpu_hotplug = false;
		spin_unlock_irqrestore(&(events_group.cpu_hotplug_lock), flags);

		if (notify_change)
			sysfs_notify(events_kobj, NULL, "cpu_hotplug");
	}

	return 0;
}


static int init_events_group(void)
{
	int ret;
	struct kobject *module_kobj;

	module_kobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!module_kobj) {
		pr_err("msm_perf: Couldn't find module kobject\n");
		return -ENOENT;
	}

	events_kobj = kobject_create_and_add("events", module_kobj);
	if (!events_kobj) {
		pr_err("msm_perf: Failed to add events_kobj\n");
		return -ENOMEM;
	}

	ret = sysfs_create_group(events_kobj, &events_attr_group);
	if (ret) {
		pr_err("msm_perf: Failed to create sysfs\n");
		return ret;
	}

	spin_lock_init(&(events_group.cpu_hotplug_lock));
	events_notify_thread = kthread_run(events_notify_userspace,
					NULL, "msm_perf:events_notify");
	if (IS_ERR(events_notify_thread))
		return PTR_ERR(events_notify_thread);

	events_group.init_success = true;

	return 0;
}

static int __init msm_performance_init(void)
{
	unsigned int cpu;
	int rc;

	cpufreq_register_notifier(&perf_cpufreq_nb, CPUFREQ_POLICY_NOTIFIER);

	for_each_present_cpu(cpu)
		per_cpu(cpu_stats, cpu).max = UINT_MAX;

	rc = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE,
		"msm_performance_cpu_hotplug",
		hotplug_notify,
		NULL);

	init_events_group();
	init_highfreq_monitor(&highfreq_monitor);
	return 0;
}
late_initcall(msm_performance_init);
