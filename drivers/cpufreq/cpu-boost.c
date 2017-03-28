/*
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "cpu-boost: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>

struct cpu_sync {
	int cpu;
};

static DEFINE_PER_CPU(struct cpu_sync, sync_info);
static struct workqueue_struct *boost_rem_wq;

/*
 * The CPUFREQ_ADJUST notifier is used to override the current policy min to
 * make sure policy min >= boost_min. The cpufreq framework then does the job
 * of enforcing the new policy.
 */
static int boost_adjust_notify(struct notifier_block *nb, unsigned long val,
				void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned int cpu = policy->cpu;
	struct cpu_sync *s = &per_cpu(sync_info, cpu);
	unsigned int min = s->boost_min;

	if (val != CPUFREQ_ADJUST)
		return NOTIFY_OK;

	if (min == 0)
		return NOTIFY_OK;

	pr_debug("CPU%u policy min before boost: %u kHz\n",
		 cpu, policy->min);
	pr_debug("CPU%u boost min: %u kHz\n", cpu, min);

	cpufreq_verify_within_limits(policy, min, UINT_MAX);

	pr_debug("CPU%u policy min after boost: %u kHz\n",
		 cpu, policy->min);

	return NOTIFY_OK;
}

static struct notifier_block boost_adjust_nb = {
	.notifier_call = boost_adjust_notify,
};

static int cpu_boost_init(void)
{
	int cpu;
	struct cpu_sync *s;

	boost_rem_wq = alloc_workqueue("cpuboost_rem_wq", WQ_HIGHPRI, 0);
	if (!boost_rem_wq)
		return -EFAULT;

	for_each_possible_cpu(cpu) {
		s = &per_cpu(sync_info, cpu);
		s->cpu = cpu;
	}
	cpufreq_register_notifier(&boost_adjust_nb, CPUFREQ_POLICY_NOTIFIER);

	return 0;
}
late_initcall(cpu_boost_init);
