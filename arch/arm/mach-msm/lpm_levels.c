/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <mach/mpm.h>
#include "lpm_resources.h"
#include "pm.h"
#include "rpm-notifier.h"


enum {
	MSM_LPM_LVL_DBG_SUSPEND_LIMITS = BIT(0),
	MSM_LPM_LVL_DBG_IDLE_LIMITS = BIT(1),
};

static int msm_lpm_lvl_dbg_msk;

module_param_named(
	debug_mask, msm_lpm_lvl_dbg_msk, int, S_IRUGO | S_IWUSR | S_IWGRP
);

static struct msm_rpmrs_level *msm_lpm_levels;
static int msm_lpm_level_count;

static DEFINE_PER_CPU(uint32_t , msm_lpm_sleep_time);
static DEFINE_PER_CPU(int , lpm_permitted_level);
static DEFINE_PER_CPU(struct atomic_notifier_head, lpm_notify_head);

static void msm_lpm_level_update(void)
{
	unsigned int lpm_level;
	struct msm_rpmrs_level *level = NULL;

	for (lpm_level = 0; lpm_level < msm_lpm_level_count; lpm_level++) {
		level = &msm_lpm_levels[lpm_level];
		level->available =
			!msm_lpm_level_beyond_limit(&level->rs_limits);
	}
}

int msm_lpm_enter_sleep(uint32_t sclk_count, void *limits,
		bool from_idle, bool notify_rpm)
{
	int ret = 0;
	struct msm_lpm_sleep_data sleep_data;

	sleep_data.limits = limits;
	sleep_data.kernel_sleep = __get_cpu_var(msm_lpm_sleep_time);
	atomic_notifier_call_chain(&__get_cpu_var(lpm_notify_head),
		MSM_LPM_STATE_ENTER, &sleep_data);

	if (notify_rpm) {
		int debug_mask;
		struct msm_rpmrs_limits *l = (struct msm_rpmrs_limits *)limits;

		ret = msm_rpm_enter_sleep();
		if (ret) {
			pr_warn("%s(): RPM failed to enter sleep err:%d\n",
					__func__, ret);
			goto bail;
		}
		if (from_idle)
			debug_mask = msm_lpm_lvl_dbg_msk &
					MSM_LPM_LVL_DBG_IDLE_LIMITS;
		else
			debug_mask = msm_lpm_lvl_dbg_msk &
					MSM_LPM_LVL_DBG_SUSPEND_LIMITS;

		if (debug_mask)
			pr_info("%s(): pxo:%d l2:%d mem:0x%x(0x%x) dig:0x%x(0x%x)\n",
					__func__, l->pxo, l->l2_cache,
					l->vdd_mem_lower_bound,
					l->vdd_mem_upper_bound,
					l->vdd_dig_lower_bound,
					l->vdd_dig_upper_bound);

		ret = msm_lpmrs_enter_sleep(sclk_count, l, from_idle,
				notify_rpm);
	}
bail:
	return ret;
}

static void msm_lpm_exit_sleep(void *limits, bool from_idle,
		bool notify_rpm, bool collapsed)
{
	msm_rpm_exit_sleep();
	msm_lpmrs_exit_sleep((struct msm_rpmrs_limits *)limits,
				from_idle, notify_rpm, collapsed);
	atomic_notifier_call_chain(&__get_cpu_var(lpm_notify_head),
			MSM_LPM_STATE_EXIT, NULL);
}

void msm_lpm_show_resources(void)
{
	/* TODO */
	return;
}

uint32_t msm_pm_get_pxo(struct msm_rpmrs_limits *limits)
{
	return limits->pxo;
}

uint32_t msm_pm_get_l2_cache(struct msm_rpmrs_limits *limits)
{
	return limits->l2_cache;
}

uint32_t msm_pm_get_vdd_mem(struct msm_rpmrs_limits *limits)
{
	return limits->vdd_mem_upper_bound;
}

uint32_t msm_pm_get_vdd_dig(struct msm_rpmrs_limits *limits)
{
	return limits->vdd_dig_upper_bound;
}

static bool lpm_level_permitted(int cur_level_count)
{
	if (__get_cpu_var(lpm_permitted_level) == msm_lpm_level_count + 1)
		return true;
	return (__get_cpu_var(lpm_permitted_level) == cur_level_count);
}

int msm_lpm_register_notifier(int cpu, int level_iter,
			struct notifier_block *nb, bool is_latency_measure)
{
	per_cpu(lpm_permitted_level, cpu) = level_iter;
	return atomic_notifier_chain_register(&per_cpu(lpm_notify_head,
			cpu), nb);
}

int msm_lpm_unregister_notifier(int cpu, struct notifier_block *nb)
{
	per_cpu(lpm_permitted_level, cpu) = msm_lpm_level_count + 1;
	return atomic_notifier_chain_unregister(&per_cpu(lpm_notify_head, cpu),
				nb);
}

s32 msm_cpuidle_get_deep_idle_latency(void)
{
	int i;
	struct msm_rpmrs_level *level = msm_lpm_levels, *best = level;

	if (!level)
		return 0;

	for (i = 0; i < msm_lpm_level_count; i++, level++) {
		if (!level->available)
			continue;
		if (level->sleep_mode != MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			continue;
		/* Pick the first power collapse mode by default */
		if (best->sleep_mode != MSM_PM_SLEEP_MODE_POWER_COLLAPSE)
			best = level;
		/* Find the lowest latency for power collapse */
		if (level->latency_us < best->latency_us)
			best = level;
	}
	return best->latency_us - 1;
}

static void *msm_lpm_lowest_limits(bool from_idle,
		enum msm_pm_sleep_mode sleep_mode,
		struct msm_pm_time_params *time_param, uint32_t *power)
{
	unsigned int cpu = smp_processor_id();
	struct msm_rpmrs_level *best_level = NULL;
	uint32_t pwr;
	int i;
	int best_level_iter = msm_lpm_level_count + 1;
	if (!msm_lpm_levels)
		return NULL;

	msm_lpm_level_update();

	for (i = 0; i < msm_lpm_level_count; i++) {
		struct msm_rpmrs_level *level = &msm_lpm_levels[i];

		if (!level->available)
			continue;

		if (sleep_mode != level->sleep_mode)
			continue;

		if (time_param->latency_us < level->latency_us)
			continue;

		if ((MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE == sleep_mode)
			|| (MSM_PM_SLEEP_MODE_POWER_COLLAPSE == sleep_mode))
			if (!cpu && msm_rpm_waiting_for_ack())
					break;

		if (time_param->sleep_us <= 1) {
			pwr = level->energy_overhead;
		} else if (time_param->sleep_us <= level->time_overhead_us) {
			pwr = level->energy_overhead / time_param->sleep_us;
		} else if ((time_param->sleep_us >> 10)
				> level->time_overhead_us) {
			pwr = level->steady_state_power;
		} else {
			pwr = level->steady_state_power;
			pwr -= (level->time_overhead_us *
				level->steady_state_power) /
						time_param->sleep_us;
			pwr += level->energy_overhead / time_param->sleep_us;
		}

		if (!best_level || best_level->rs_limits.power[cpu] >= pwr) {

			level->rs_limits.latency_us[cpu] = level->latency_us;
			level->rs_limits.power[cpu] = pwr;
			best_level = level;
			best_level_iter = i;
			if (power)
				*power = pwr;
		}
	}
	if (best_level && !lpm_level_permitted(best_level_iter))
		best_level = NULL;
	else
		per_cpu(msm_lpm_sleep_time, cpu) =
			time_param->modified_time_us ?
			time_param->modified_time_us : time_param->sleep_us;

	return best_level ? &best_level->rs_limits : NULL;
}

static struct lpm_test_platform_data lpm_test_pdata;

static struct platform_device msm_lpm_test_device = {
	.name		= "lpm_test",
	.id		= -1,
	.dev		= {
		.platform_data = &lpm_test_pdata,
	},
};

static struct msm_pm_sleep_ops msm_lpm_ops = {
	.lowest_limits = msm_lpm_lowest_limits,
	.enter_sleep = msm_lpm_enter_sleep,
	.exit_sleep = msm_lpm_exit_sleep,
};

static int __devinit msm_lpm_levels_probe(struct platform_device *pdev)
{
	struct msm_rpmrs_level *levels = NULL;
	struct msm_rpmrs_level *level = NULL;
	struct device_node *node = NULL;
	char *key = NULL;
	uint32_t val = 0;
	int ret = 0;
	uint32_t num_levels = 0;
	int idx = 0;
	unsigned int m_cpu = 0;

	for_each_child_of_node(pdev->dev.of_node, node)
		num_levels++;

	levels = kzalloc(num_levels * sizeof(struct msm_rpmrs_level),
			GFP_KERNEL);
	if (!levels)
		return -ENOMEM;

	for_each_child_of_node(pdev->dev.of_node, node) {
		level = &levels[idx++];
		level->available = false;

		key = "qcom,mode";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->sleep_mode = val;

		key = "qcom,xo";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.pxo = val;

		key = "qcom,l2";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.l2_cache = val;

		key = "qcom,vdd-dig-upper-bound";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.vdd_dig_upper_bound = val;

		key = "qcom,vdd-dig-lower-bound";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.vdd_dig_lower_bound = val;

		key = "qcom,vdd-mem-upper-bound";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.vdd_mem_upper_bound = val;

		key = "qcom,vdd-mem-lower-bound";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.vdd_mem_lower_bound = val;

		key = "qcom,latency-us";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->latency_us = val;

		key = "qcom,ss-power";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->steady_state_power = val;

		key = "qcom,energy-overhead";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->energy_overhead = val;

		key = "qcom,time-overhead";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->time_overhead_us = val;

		level->available = true;
	}

	msm_lpm_levels = levels;
	msm_lpm_level_count = idx;

	lpm_test_pdata.msm_lpm_test_levels = msm_lpm_levels;
	lpm_test_pdata.msm_lpm_test_level_count = msm_lpm_level_count;
	key = "qcom,use-qtimer";
	lpm_test_pdata.use_qtimer =
			of_property_read_bool(pdev->dev.of_node, key);

	for_each_possible_cpu(m_cpu)
		per_cpu(lpm_permitted_level, m_cpu) =
					msm_lpm_level_count + 1;

	platform_device_register(&msm_lpm_test_device);
	msm_pm_set_sleep_ops(&msm_lpm_ops);

	return 0;
fail:
	pr_err("%s: Error in name %s key %s\n", __func__, node->full_name, key);
	kfree(levels);
	return -EFAULT;
}

static struct of_device_id msm_lpm_levels_match_table[] = {
	{.compatible = "qcom,lpm-levels"},
	{},
};

static struct platform_driver msm_lpm_levels_driver = {
	.probe = msm_lpm_levels_probe,
	.driver = {
		.name = "lpm-levels",
		.owner = THIS_MODULE,
		.of_match_table = msm_lpm_levels_match_table,
	},
};

static int __init msm_lpm_levels_module_init(void)
{
	return platform_driver_register(&msm_lpm_levels_driver);
}
late_initcall(msm_lpm_levels_module_init);
