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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <mach/mpm.h>
#include "rpm_resources.h"
#include "pm.h"

static struct msm_rpmrs_level *msm_lpm_levels;
static int msm_lpm_level_count;

static int msm_lpm_enter_sleep(uint32_t sclk_count, void *limits,
		bool from_idle, bool notify_rpm)
{
	/* TODO */
	return 0;
}

static void msm_lpm_exit_sleep(void *limits, bool from_idle,
		bool notify_rpm, bool collapsed)
{
	/* TODO */
	return;
}

static bool msm_rpmrs_irqs_detectable(struct msm_rpmrs_limits *limits,
		bool irqs_detect, bool gpio_detect)
{
	/* TODO */
	return true;
}

void msm_rpmrs_show_resources(void)
{
	/* TODO */
	return;
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
		enum msm_pm_sleep_mode sleep_mode, uint32_t latency_us,
		uint32_t sleep_us, uint32_t *power)
{
	unsigned int cpu = smp_processor_id();
	struct msm_rpmrs_level *best_level = NULL;
	bool irqs_detectable = false;
	bool gpio_detectable = false;
	uint32_t pwr;
	int i;

	if (!msm_lpm_levels)
		return NULL;

	if (sleep_mode == MSM_PM_SLEEP_MODE_POWER_COLLAPSE) {
		irqs_detectable = msm_mpm_irqs_detectable(from_idle);
		gpio_detectable = msm_mpm_gpio_irqs_detectable(from_idle);
	}

	for (i = 0; i < msm_lpm_level_count; i++) {
		struct msm_rpmrs_level *level = &msm_lpm_levels[i];

		if (!level->available)
			continue;

		if (sleep_mode != level->sleep_mode)
			continue;

		if (latency_us < level->latency_us)
			continue;

		if (!msm_rpmrs_irqs_detectable(&level->rs_limits,
					irqs_detectable, gpio_detectable))
			continue;

		if (sleep_us <= 1) {
			pwr = level->energy_overhead;
		} else if (sleep_us <= level->time_overhead_us) {
			pwr = level->energy_overhead / sleep_us;
		} else if ((sleep_us >> 10) > level->time_overhead_us) {
			pwr = level->steady_state_power;
		} else {
			pwr = level->steady_state_power;
			pwr -= (level->time_overhead_us *
					level->steady_state_power)/sleep_us;
			pwr += level->energy_overhead / sleep_us;
		}

		if (!best_level || best_level->rs_limits.power[cpu] >= pwr) {

			level->rs_limits.latency_us[cpu] = level->latency_us;
			level->rs_limits.power[cpu] = pwr;
			best_level = level;

			if (power)
				*power = pwr;
		}
	}

	return best_level ? &best_level->rs_limits : NULL;
}
static struct msm_pm_sleep_ops msm_lpm_ops = {
	.lowest_limits = msm_lpm_lowest_limits,
	.enter_sleep = msm_lpm_enter_sleep,
	.exit_sleep = msm_lpm_exit_sleep,
};

static int msm_lpm_levels_probe(struct platform_device *pdev)
{
	struct msm_rpmrs_level *levels = NULL;
	struct msm_rpmrs_level *level = NULL;
	struct device_node *node = NULL;
	char *key = NULL;
	uint32_t val = 0;
	int ret = 0;
	uint32_t num_levels = 0;
	int idx = 0;

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
		level->rs_limits.vdd_dig = val;

		key = "qcom,vdd-mem-upper-bound";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.vdd_mem_upper_bound = val;

		key = "qcom,vdd-mem-lower-bound";
		ret = of_property_read_u32(node, key, &val);
		if (ret)
			goto fail;
		level->rs_limits.vdd_mem = val;

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
