/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <trace/events/power.h>

/* Delay in jiffies for hotplugging to complete */
#define MIN_HOTPLUG_DELAY 3

/* Number of CPUs to maintain online */
static unsigned int max_cpus;

/* List of CPUs managed by this module */
static struct cpumask managed_cpus;
static struct mutex managed_cpus_lock;

/* To keep track of CPUs that the module decides to offline */
static struct cpumask managed_offline_cpus;

/* Work to evaluate the onlining/offlining CPUs */
struct delayed_work try_hotplug_work;

static unsigned int num_online_managed(void);

static int set_max_cpus(const char *buf, const struct kernel_param *kp)
{
	unsigned int val;

	if (sscanf(buf, "%u\n", &val) != 1)
		return -EINVAL;
	if (val > cpumask_weight(&managed_cpus))
		return -EINVAL;

	max_cpus = val;
	schedule_delayed_work(&try_hotplug_work, 0);
	trace_set_max_cpus(cpumask_bits(&managed_cpus)[0], max_cpus);

	return 0;
}

static int get_max_cpus(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%u", max_cpus);
}

static const struct kernel_param_ops param_ops_max_cpus = {
	.set = set_max_cpus,
	.get = get_max_cpus,
};

device_param_cb(max_cpus, &param_ops_max_cpus, NULL, 0644);

static int set_managed_cpus(const char *buf, const struct kernel_param *kp)
{
	int ret;

	mutex_lock(&managed_cpus_lock);
	ret = cpulist_parse(buf, &managed_cpus);
	cpumask_clear(&managed_offline_cpus);
	mutex_unlock(&managed_cpus_lock);

	return ret;
}

static int get_managed_cpus(char *buf, const struct kernel_param *kp)
{
	return cpulist_scnprintf(buf, PAGE_SIZE, &managed_cpus);
}

static const struct kernel_param_ops param_ops_managed_cpus = {
	.set = set_managed_cpus,
	.get = get_managed_cpus,
};
device_param_cb(managed_cpus, &param_ops_managed_cpus, NULL, 0644);

/* To display all the online managed CPUs */
static int get_managed_online_cpus(char *buf, const struct kernel_param *kp)
{
	struct cpumask tmp_mask;

	cpumask_clear(&tmp_mask);
	mutex_lock(&managed_cpus_lock);
	cpumask_complement(&tmp_mask, &managed_offline_cpus);
	cpumask_and(&tmp_mask, &managed_cpus, &tmp_mask);
	mutex_unlock(&managed_cpus_lock);

	return cpulist_scnprintf(buf, PAGE_SIZE, &tmp_mask);
}

static const struct kernel_param_ops param_ops_managed_online_cpus = {
	.get = get_managed_online_cpus,
};
device_param_cb(managed_online_cpus, &param_ops_managed_online_cpus,
								NULL, 0444);

static unsigned int num_online_managed(void)
{
	struct cpumask tmp_mask;

	cpumask_clear(&tmp_mask);
	cpumask_and(&tmp_mask, &managed_cpus, cpu_online_mask);

	return cpumask_weight(&tmp_mask);
}

/*
 * try_hotplug tries to online/offline cores based on the current requirement.
 * It loops through the currently managed CPUs and tries to online/offline
 * them until the max_cpus criteria is met.
 */
static void __ref try_hotplug(struct work_struct *work)
{
	unsigned int i;

	if (cpumask_empty(&managed_cpus) || (num_online_managed() == max_cpus))
		return;

	pr_debug("msm_perf: Trying hotplug...%d:%d\n", num_online_managed(),
							num_online_cpus());

	mutex_lock(&managed_cpus_lock);
	if (num_online_managed() > max_cpus) {
		for (i = num_present_cpus() - 1; i >= 0; i--) {
			if (!cpumask_test_cpu(i, &managed_cpus) ||
							!cpu_online(i))
				continue;

			pr_debug("msm_perf: Offlining CPU%d\n", i);
			cpumask_set_cpu(i, &managed_offline_cpus);
			if (cpu_down(i)) {
				cpumask_clear_cpu(i, &managed_offline_cpus);
				pr_debug("msm_perf: Offlining CPU%d failed\n",
									i);
				continue;
			}
			if (num_online_managed() <= max_cpus)
				break;
		}
	} else {
		for_each_cpu(i, &managed_cpus) {
			if (cpu_online(i))
				continue;
			pr_debug("msm_perf: Onlining CPU%d\n", i);
			if (cpu_up(i)) {
				pr_debug("msm_perf: Onlining CPU%d failed\n",
									i);
				continue;
			}
			cpumask_clear_cpu(i, &managed_offline_cpus);
			if (num_online_managed() >= max_cpus)
				break;
		}
	}
	mutex_unlock(&managed_cpus_lock);
}

static int __ref msm_performance_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	uint32_t cpu = (uintptr_t)hcpu;

	if (!cpumask_test_cpu(cpu, &managed_cpus))
		return NOTIFY_OK;

	if (action == CPU_UP_PREPARE || action == CPU_UP_PREPARE_FROZEN) {
		/*
		 * Prevent onlining of a managed CPU if max_cpu criteria is
		 * already satisfied
		 */
		if (max_cpus <= num_online_managed()) {
			pr_debug("msm_perf: Prevent CPU%d onlining\n", cpu);
			return NOTIFY_BAD;
		}
		cpumask_clear_cpu(cpu, &managed_offline_cpus);

	} else if (!cpumask_test_cpu(cpu, &managed_offline_cpus) &&
					(action == CPU_DEAD)) {
		/*
		 * Schedule a re-evaluation to check if any more CPUs can be
		 * brought online to meet the max_cpus requirement. This work
		 * is delayed to account for CPU hotplug latencies
		 */
		if (schedule_delayed_work(&try_hotplug_work, 0)) {
			trace_reevaluate_hotplug(cpumask_bits(&managed_cpus)[0],
								max_cpus);
			pr_debug("msm_perf: Re-evaluation scheduled %d\n", cpu);
		} else {
			pr_debug("msm_perf: Work scheduling failed %d\n", cpu);
		}
	}

	return NOTIFY_OK;
}

static struct notifier_block __refdata msm_performance_cpu_notifier = {
	.notifier_call = msm_performance_cpu_callback,
};

static int __init msm_performance_init(void)
{

	INIT_DELAYED_WORK(&try_hotplug_work, try_hotplug);
	mutex_init(&managed_cpus_lock);
	cpumask_clear(&managed_offline_cpus);

	register_cpu_notifier(&msm_performance_cpu_notifier);
	return 0;
}
late_initcall(msm_performance_init);

