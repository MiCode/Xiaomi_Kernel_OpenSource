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

static int freq_limit_max_notifier_call(struct notifier_block *nb,
					 unsigned long event, void *ptr)
{
	int cpu = nb - freq_limit_max_notifier;
	int freq_limit_max = event;
	int idx_max;
	unsigned long cap;
	struct cpufreq_policy *policy;

	policy = cpufreq_cpu_get(cpu);
	if (!policy) {
		pr_info("cpu=%d: cpufreq_cpu_get failed\n", cpu);
		return -1;
	}

	idx_max = cpufreq_frequency_table_target(policy, freq_limit_max,
			CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);

	if (idx_max < 0) {
		pr_info("cpu=%d: mapping cpu frequency index failed\n", cpu);
		return -1;
	}

	cap = pd_get_opp_capacity(cpu, idx_max);

	per_cpu(max_freq_scale, cpu) = cap;

	return 0;
}

static int freq_limit_min_notifier_call(struct notifier_block *nb,
					 unsigned long event, void *ptr)
{
	int cpu = nb - freq_limit_min_notifier;
	int freq_limit_min = event;

	per_cpu(min_freq, cpu) = freq_limit_min;

	return 0;
}

void mtk_freq_limit_notifier_register(void)
{
	int cpu, num = 0, cpu_num = 0;
	struct cpufreq_policy *policy;

	for_each_possible_cpu(cpu)
		cpu_num++;

	freq_limit_max_notifier = kcalloc(cpu_num, sizeof(struct notifier_block), GFP_KERNEL);
	freq_limit_min_notifier = kcalloc(cpu_num, sizeof(struct notifier_block), GFP_KERNEL);

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (policy != NULL) {
			freq_limit_max_notifier[num].notifier_call = freq_limit_max_notifier_call;
			freq_limit_min_notifier[num].notifier_call = freq_limit_min_notifier_call;
			freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MAX,
			freq_limit_max_notifier + num);
			freq_qos_add_notifier(&policy->constraints, FREQ_QOS_MIN,
			freq_limit_min_notifier + num);
			cpufreq_cpu_put(policy);
		} else {
			pr_info("CPU%d %s failed\n", cpu, __func__);
		}
		num++;
	}
}

void mtk_update_cpu_capacity(void *data, int cpu, unsigned long *capacity)
{
	unsigned long cap_ceiling;

	cap_ceiling = per_cpu(max_freq_scale, cpu);
	*capacity = min(cap_ceiling, *capacity);
}

#endif
