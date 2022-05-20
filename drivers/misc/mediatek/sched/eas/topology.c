// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/notifier.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <sched/sched.h>
#include <sugov/cpufreq.h>

MODULE_LICENSE("GPL");
/*
 *  max_freq_scale:
 *	max cpu frequency limit
 *	range: 0~SCHED_CAPACITY_SCALE
 *  min_freq:
 *	min cpu frequency limit
 *	unit: frequency
 */
DEFINE_PER_CPU(unsigned long, max_freq_scale) = SCHED_CAPACITY_SCALE;
DEFINE_PER_CPU(unsigned long, min_freq) = 0;

#if IS_ENABLED(CONFIG_MTK_EAS)
static struct notifier_block *freq_limit_max_notifier, *freq_limit_min_notifier;
static unsigned int nr_gears;

static int freq_limit_max_notifier_call(struct notifier_block *nb,
					 unsigned long freq_limit_max, void *ptr)
{
	int cpu, gear_idx = nb - freq_limit_max_notifier;

	if (gear_idx < 0 || gear_idx >= nr_gears) {
		pr_info("freq_limit_max_notifier_call: gear_idx over-index\n");
		return -1;
	}

	for_each_possible_cpu(cpu) {
		if (per_cpu(gear_id, cpu) == gear_idx)
			per_cpu(max_freq_scale, cpu) = pd_get_freq_util(cpu, freq_limit_max);
	}

	return 0;
}

static int freq_limit_min_notifier_call(struct notifier_block *nb,
					 unsigned long freq_limit_min, void *ptr)
{
	int cpu, gear_idx = nb - freq_limit_min_notifier;

	if (gear_idx < 0 || gear_idx >= nr_gears) {
		pr_info("freq_limit_min_notifier_call: gear_idx over-index\n");
		return -1;
	}

	for_each_possible_cpu(cpu) {
		if (per_cpu(gear_id, cpu) == gear_idx)
			per_cpu(min_freq, cpu) = freq_limit_min;
	}

	return 0;
}

void mtk_freq_limit_notifier_register(void)
{
	struct cpufreq_policy *policy;
	int cpu, gear_idx = 0, ret;

	nr_gears = get_nr_gears();
	freq_limit_max_notifier = kcalloc(nr_gears, sizeof(struct notifier_block), GFP_KERNEL);
	freq_limit_min_notifier = kcalloc(nr_gears, sizeof(struct notifier_block), GFP_KERNEL);

	for_each_possible_cpu(cpu) {
		if (gear_idx >= nr_gears) {
			pr_info("mtk_freq_limit_notifier_register: gear_idx over-index\n");
			break;
		}
		policy = cpufreq_cpu_get(cpu);

		if (policy) {
			freq_limit_max_notifier[gear_idx].notifier_call
				= freq_limit_max_notifier_call;
			freq_limit_min_notifier[gear_idx].notifier_call
				= freq_limit_min_notifier_call;

			ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MAX,
				freq_limit_max_notifier + gear_idx);
			if (ret)
				pr_info("freq_qos_add_notifier freq_limit_max_notifier failed\n");

			ret = freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MIN,
				freq_limit_min_notifier + gear_idx);
			if (ret)
				pr_info("freq_qos_add_notifier freq_limit_min_notifier failed\n");

			gear_idx++;
			cpu = cpumask_last(policy->related_cpus);
			cpufreq_cpu_put(policy);
		}
	}
}

void mtk_update_cpu_capacity(void *data, int cpu, unsigned long *capacity)
{
	unsigned long cap_ceiling;

	cap_ceiling = per_cpu(max_freq_scale, cpu);
	*capacity = min(cap_ceiling, *capacity);
}

#endif
