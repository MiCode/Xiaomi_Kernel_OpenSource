/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/cpu_pm.h>
#include <linux/pm_qos.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <mach/msm_dcvs.h>

struct cpu_idle_info {
	int cpu;
	int enabled;
	int handle;
	struct msm_dcvs_idle dcvs_notifier;
	struct pm_qos_request pm_qos_req;
};

static DEFINE_PER_CPU_SHARED_ALIGNED(struct cpu_idle_info, cpu_idle_info);
static DEFINE_PER_CPU_SHARED_ALIGNED(u64, iowait_on_cpu);
static char core_name[NR_CPUS][10];
static uint32_t latency;

static int msm_dcvs_idle_notifier(struct msm_dcvs_idle *self,
		enum msm_core_control_event event)
{
	struct cpu_idle_info *info = container_of(self,
				struct cpu_idle_info, dcvs_notifier);

	switch (event) {
	case MSM_DCVS_ENABLE_IDLE_PULSE:
		info->enabled = true;
		break;

	case MSM_DCVS_DISABLE_IDLE_PULSE:
		info->enabled = false;
		break;

	case MSM_DCVS_ENABLE_HIGH_LATENCY_MODES:
		pm_qos_update_request(&info->pm_qos_req, PM_QOS_DEFAULT_VALUE);
		break;

	case MSM_DCVS_DISABLE_HIGH_LATENCY_MODES:
		pm_qos_update_request(&info->pm_qos_req, latency);
		break;
	}

	return 0;
}

static int msm_cpuidle_notifier(struct notifier_block *self, unsigned long cmd,
		void *v)
{
	struct cpu_idle_info *info =
		&per_cpu(cpu_idle_info, smp_processor_id());
	u64 io_wait_us = 0;
	u64 prev_io_wait_us = 0;
	u64 last_update_time = 0;
	u64 val = 0;
	uint32_t iowaited = 0;

	if (!info->enabled)
		return NOTIFY_OK;

	switch (cmd) {
	case CPU_PM_ENTER:
		val = get_cpu_iowait_time_us(smp_processor_id(),
					&last_update_time);
		/* val could be -1 when NOHZ is not enabled */
		if (val == (u64)-1)
			val = 0;
		per_cpu(iowait_on_cpu, smp_processor_id()) = val;
		msm_dcvs_idle(info->handle, MSM_DCVS_IDLE_ENTER, 0);
		break;

	case CPU_PM_EXIT:
		prev_io_wait_us = per_cpu(iowait_on_cpu, smp_processor_id());
		val = get_cpu_iowait_time_us(smp_processor_id(),
				&last_update_time);
		if (val == (u64)-1)
			val = 0;
		io_wait_us = val;
		iowaited = (io_wait_us - prev_io_wait_us);
		msm_dcvs_idle(info->handle, MSM_DCVS_IDLE_EXIT, iowaited);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block idle_nb = {
	.notifier_call = msm_cpuidle_notifier,
};

static void msm_gov_idle_source_init(int cpu)
{
	struct cpu_idle_info *info = NULL;
	struct msm_dcvs_idle *inotify = NULL;

	info = &per_cpu(cpu_idle_info, cpu);
	info->cpu = cpu;
	inotify = &info->dcvs_notifier;
	snprintf(core_name[cpu], 10, "cpu%d", cpu);
	inotify->core_name = core_name[cpu];
	info->handle = msm_dcvs_idle_source_register(inotify);
	BUG_ON(info->handle < 0);

	pm_qos_add_request(&info->pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);
}

static int msm_gov_idle_source_uninit(int cpu)
{
	struct cpu_idle_info *info = NULL;
	struct msm_dcvs_idle *inotify = NULL;

	info = &per_cpu(cpu_idle_info, cpu);
	info->cpu = cpu;
	inotify = &info->dcvs_notifier;
	return msm_dcvs_idle_source_unregister(inotify);
}

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
	struct msm_dcvs_freq *dcvs_notifier =
			&(per_cpu(msm_gov_info, policy->cpu).gov_notifier);

	if (policy->max < gov->cur_freq)
		__cpufreq_driver_target(policy, policy->max,
				CPUFREQ_RELATION_H);
	else if (policy->min > gov->cur_freq)
		__cpufreq_driver_target(policy, policy->min,
				CPUFREQ_RELATION_L);
	else
		__cpufreq_driver_target(policy, gov->cur_freq,
				CPUFREQ_RELATION_L);

	gov->cur_freq = policy->cur;
	gov->min_freq = policy->min;
	gov->max_freq = policy->max;
	msm_dcvs_update_limits(dcvs_notifier);
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

	mutex_unlock(&per_cpu(gov_mutex, gov->cpu));

	ret = cpufreq_driver_target(gov->policy, freq, CPUFREQ_RELATION_L);

	if (!ret) {
		gov->cur_freq = cpufreq_quick_get(gov->cpu);
		if (freq != gov->cur_freq)
			pr_err("cpu %d freq %u gov->cur_freq %u didn't match",
						gov->cpu, freq, gov->cur_freq);
	}
	ret = gov->cur_freq;

	return ret;
}

static unsigned int msm_dcvs_freq_get(struct msm_dcvs_freq *self)
{
	struct msm_gov *gov =
		container_of(self, struct msm_gov, gov_notifier);

	/*
	 * the rw_sem in cpufreq is always held when this is called.
	 * The policy->cur won't be updated in this case - so it is safe to
	 * access policy->cur
	 */
	return gov->policy->cur;
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
		handle = msm_dcvs_freq_sink_start(dcvs_notifier);
		BUG_ON(handle < 0);
		msm_gov_check_limits(policy);
		mutex_unlock(&per_cpu(gov_mutex, cpu));
		break;

	case CPUFREQ_GOV_STOP:
		msm_dcvs_freq_sink_stop(dcvs_notifier);
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
	struct msm_dcvs_core_info *core = NULL;
	struct msm_dcvs_core_info *core_info = NULL;
	struct msm_gov_platform_data *pdata = pdev->dev.platform_data;
	int sensor = 0;

	core = pdev->dev.platform_data;
	core_info = pdata->info;
	latency = pdata->latency;

	for_each_possible_cpu(cpu) {
		mutex_init(&per_cpu(gov_mutex, cpu));
		snprintf(core_name[cpu], 10, "cpu%d", cpu);
		if (cpu < core->num_cores)
			sensor = core_info->sensors[cpu];
		ret = msm_dcvs_register_core(core_name[cpu], core_info,
						msm_dcvs_freq_set,
						msm_dcvs_freq_get,
						msm_dcvs_idle_notifier,
						sensor);
		if (ret)
			pr_err("Unable to register core for %d\n", cpu);

		msm_gov_idle_source_init(cpu);
	}

	cpu_pm_register_notifier(&idle_nb);

	return cpufreq_register_governor(&cpufreq_gov_msm);
}

static int __devexit msm_gov_remove(struct platform_device *pdev)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		msm_gov_idle_source_uninit(cpu);
	}
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
