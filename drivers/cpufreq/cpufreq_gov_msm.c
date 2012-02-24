/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/cpufreq.h>
#include <linux/platform_device.h>
#include <mach/msm_dcvs.h>

struct msm_gov {
	int cpu;
	unsigned int cur_freq;
	unsigned int min_freq;
	unsigned int max_freq;
	struct msm_dcvs_freq gov_notifier;
	struct cpufreq_policy *policy;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct mutex, gov_mutex);
static DEFINE_PER_CPU_SHARED_ALIGNED(struct msm_gov, msm_gov_info);
static char core_name[NR_CPUS][10];

static void msm_gov_check_limits(struct cpufreq_policy *policy)
{
	struct msm_gov *gov = &per_cpu(msm_gov_info, policy->cpu);

	if (policy->max < gov->cur_freq)
		__cpufreq_driver_target(policy, policy->max,
				CPUFREQ_RELATION_H);
	else if (policy->min > gov->min_freq)
		__cpufreq_driver_target(policy, policy->min,
				CPUFREQ_RELATION_L);
	else
		__cpufreq_driver_target(policy, gov->cur_freq,
				CPUFREQ_RELATION_L);

	gov->cur_freq = policy->cur;
	gov->min_freq = policy->min;
	gov->max_freq = policy->max;
}

static int msm_dcvs_freq_set(struct msm_dcvs_freq *self,
		unsigned int freq)
{
	int ret = -EINVAL;
	struct msm_gov *gov =
		container_of(self, struct msm_gov, gov_notifier);

	mutex_lock(&per_cpu(gov_mutex, gov->cpu));

	if (freq < gov->min_freq)
		freq = gov->min_freq;
	if (freq > gov->max_freq)
		freq = gov->max_freq;

	ret = __cpufreq_driver_target(gov->policy, freq, CPUFREQ_RELATION_L);
	gov->cur_freq = gov->policy->cur;

	mutex_unlock(&per_cpu(gov_mutex, gov->cpu));

	if (!ret)
		return gov->cur_freq;

	return ret;
}

static unsigned int msm_dcvs_freq_get(struct msm_dcvs_freq *self)
{
	struct msm_gov *gov =
		container_of(self, struct msm_gov, gov_notifier);

	return gov->cur_freq;
}

static int cpufreq_governor_msm(struct cpufreq_policy *policy,
		unsigned int event)
{
	unsigned int cpu = policy->cpu;
	int ret = 0;
	int handle = 0;
	struct msm_gov *gov = &per_cpu(msm_gov_info, policy->cpu);
	struct msm_dcvs_freq *dcvs_notifier =
			&(per_cpu(msm_gov_info, cpu).gov_notifier);

	switch (event) {
	case CPUFREQ_GOV_START:
		if (!cpu_online(cpu))
			return -EINVAL;
		BUG_ON(!policy->cur);
		mutex_lock(&per_cpu(gov_mutex, cpu));
		per_cpu(msm_gov_info, cpu).cpu = cpu;
		gov->policy = policy;
		dcvs_notifier->core_name = core_name[cpu];
		dcvs_notifier->set_frequency = msm_dcvs_freq_set;
		dcvs_notifier->get_frequency = msm_dcvs_freq_get;
		handle = msm_dcvs_freq_sink_register(dcvs_notifier);
		BUG_ON(handle < 0);
		msm_gov_check_limits(policy);
		mutex_unlock(&per_cpu(gov_mutex, cpu));
		break;

	case CPUFREQ_GOV_STOP:
		mutex_lock(&per_cpu(gov_mutex, cpu));
		msm_dcvs_freq_sink_unregister(dcvs_notifier);
		mutex_unlock(&per_cpu(gov_mutex, cpu));
		break;

	case CPUFREQ_GOV_LIMITS:
		mutex_lock(&per_cpu(gov_mutex, cpu));
		msm_gov_check_limits(policy);
		mutex_unlock(&per_cpu(gov_mutex, cpu));
		break;
	};

	return ret;
}

struct cpufreq_governor cpufreq_gov_msm = {
	.name = "msm-dcvs",
	.governor = cpufreq_governor_msm,
	.owner = THIS_MODULE,
};

static int __devinit msm_gov_probe(struct platform_device *pdev)
{
	int ret = 0;
	int cpu;
	uint32_t group_id = 0x43505530; /* CPU0 */
	struct msm_dcvs_core_info *core = NULL;

	core = pdev->dev.platform_data;

	for_each_possible_cpu(cpu) {
		mutex_init(&per_cpu(gov_mutex, cpu));
		snprintf(core_name[cpu], 10, "cpu%d", cpu);
		ret = msm_dcvs_register_core(core_name[cpu], group_id, core);
		if (ret)
			pr_err("Unable to register core for %d\n", cpu);
	}

	return cpufreq_register_governor(&cpufreq_gov_msm);
}

static int __devexit msm_gov_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver msm_gov_driver = {
	.probe = msm_gov_probe,
	.remove = __devexit_p(msm_gov_remove),
	.driver = {
		.name = "msm_dcvs_gov",
		.owner = THIS_MODULE,
	},
};

static int __init cpufreq_gov_msm_init(void)
{
	return platform_driver_register(&msm_gov_driver);
}
late_initcall(cpufreq_gov_msm_init);
