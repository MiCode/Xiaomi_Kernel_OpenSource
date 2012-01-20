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
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/msm_tsens.h>
#include <linux/workqueue.h>

#define DEF_TEMP_SENSOR      0
#define DEF_THERMAL_CHECK_MS 1000
#define DEF_ALLOWED_MAX_HIGH 60
#define DEF_ALLOWED_MAX_FREQ 918000

static int enabled;
static int allowed_max_high = DEF_ALLOWED_MAX_HIGH;
static int allowed_max_low = (DEF_ALLOWED_MAX_HIGH - 10);
static int allowed_max_freq = DEF_ALLOWED_MAX_FREQ;
static int check_interval_ms = DEF_THERMAL_CHECK_MS;

module_param(allowed_max_high, int, 0);
module_param(allowed_max_freq, int, 0);
module_param(check_interval_ms, int, 0);

static DEFINE_PER_CPU(struct cpufreq_policy*, policy);
static struct mutex policy_mutex;
static struct delayed_work check_temp_work;

static int update_cpu_max_freq(int cpu, int max_freq)
{
	struct cpufreq_policy *cpu_policy = per_cpu(policy, cpu);
	int ret = 0;

	if (!cpu_policy)
		return -EINVAL;

	cpufreq_verify_within_limits(cpu_policy,
				cpu_policy->min, max_freq);
	cpu_policy->user_policy.max = max_freq;

	ret = cpufreq_update_policy(cpu);
	if (ret)
		pr_err("msm_thermal: cpufreq update to core%d %d err:%d\n",
				cpu, max_freq, ret);
	else
		pr_info("msm_thermal: Limiting core%d max frequency to %d\n",
			cpu, max_freq);

	return ret;
}

static void check_temp(struct work_struct *work)
{
	struct cpufreq_policy *cpu_policy = NULL;
	struct tsens_device tsens_dev;
	unsigned long temp = 0;
	unsigned int max_freq = 0;
	int update_policy = 0;
	int cpu = 0;
	int ret = 0;

	mutex_lock(&policy_mutex);
	tsens_dev.sensor_num = DEF_TEMP_SENSOR;
	ret = tsens_get_temp(&tsens_dev, &temp);
	if (ret) {
		pr_err("msm_thermal: Unable to read TSENS sensor %d\n",
				tsens_dev.sensor_num);
		goto reschedule;
	}

	for_each_possible_cpu(cpu) {
		cpu_policy = per_cpu(policy, cpu);
		if (!cpu_policy) {
			pr_debug("msm_thermal: No CPUFreq policy found for "
				"cpu %d\n", cpu);
			continue;
		}
		if (temp >= allowed_max_high) {
			if (cpu_policy->max > allowed_max_freq) {
				update_policy = 1;
				max_freq = allowed_max_freq;
			}
		} else if (temp < allowed_max_low) {
			if (cpu_policy->max < cpu_policy->cpuinfo.max_freq) {
				max_freq = cpu_policy->cpuinfo.max_freq;
				update_policy = 1;
			}
		}

		if (update_policy)
			update_cpu_max_freq(cpu, max_freq);
	}

reschedule:
	if (enabled)
		schedule_delayed_work(&check_temp_work,
				msecs_to_jiffies(check_interval_ms));

	mutex_unlock(&policy_mutex);
}

static int msm_thermal_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	if (event == CPUFREQ_START) {
		struct cpufreq_policy *cpu_policy = data;
		mutex_lock(&policy_mutex);
		per_cpu(policy, cpu_policy->cpu) = cpu_policy;
		mutex_unlock(&policy_mutex);
	}

	return 0;
}

static struct notifier_block msm_thermal_notifier_block = {
	.notifier_call = msm_thermal_notifier,
};

static void disable_msm_thermal(void)
{
	int cpu = 0;
	struct cpufreq_policy *cpu_policy = NULL;

	cpufreq_unregister_notifier(&msm_thermal_notifier_block,
			CPUFREQ_POLICY_NOTIFIER);
	cancel_delayed_work(&check_temp_work);

	mutex_lock(&policy_mutex);
	for_each_possible_cpu(cpu) {
		cpu_policy = per_cpu(policy, cpu);
		if (cpu_policy &&
			cpu_policy->max < cpu_policy->cpuinfo.max_freq)
			update_cpu_max_freq(cpu, cpu_policy->cpuinfo.max_freq);
	}
	mutex_unlock(&policy_mutex);
}

static int set_enabled(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	ret = param_set_bool(val, kp);
	if (!enabled)
		disable_msm_thermal();
	else
		pr_info("msm_thermal: no action for enabled = %d\n", enabled);

	pr_info("msm_thermal: enabled = %d\n", enabled);

	return ret;
}

static struct kernel_param_ops module_ops = {
	.set = set_enabled,
	.get = param_get_bool,
};

module_param_cb(enabled, &module_ops, &enabled, 0644);
MODULE_PARM_DESC(enabled, "enforce thermal limit on cpu");

static int __init msm_thermal_init(void)
{
	int ret = 0;

	enabled = 1;
	mutex_init(&policy_mutex);
	INIT_DELAYED_WORK(&check_temp_work, check_temp);

	ret = cpufreq_register_notifier(&msm_thermal_notifier_block,
			CPUFREQ_POLICY_NOTIFIER);

	schedule_delayed_work(&check_temp_work, 0);

	return ret;
}
fs_initcall(msm_thermal_init);

