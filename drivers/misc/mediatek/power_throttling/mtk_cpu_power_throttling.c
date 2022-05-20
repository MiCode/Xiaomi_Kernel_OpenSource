// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>

#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_cpu_power_throttling.h"

#define CPU_LIMIT_FREQ 900000

struct cpu_pt_priv {
	u32 lbat_cpu_limit[3];
	u32 oc_cpu_limit[3];
};

static LIST_HEAD(pt_policy_list);

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
static void cpu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level)
{
	struct cpu_pt_policy *pt_policy;
	s32 freq_limit;

	if (level <= LOW_BATTERY_LEVEL_2) {
		list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
			if (pt_policy->pt_type == LBAT_POWER_THROTTLING) {
				if (level != LOW_BATTERY_LEVEL_0)
					freq_limit = pt_policy->cpu_limit;
				else
					freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;

				freq_qos_update_request(&pt_policy->qos_req, freq_limit);
			}
		}
	}
}
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
static void cpu_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level)
{
	struct cpu_pt_policy *pt_policy;
	s32 freq_limit;

	if (level <= BATTERY_OC_LEVEL_1) {
		list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
			if (pt_policy->pt_type == OC_POWER_THROTTLING) {
				if (level != BATTERY_OC_LEVEL_0)
					freq_limit = pt_policy->cpu_limit;
				else
					freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;

				freq_qos_update_request(&pt_policy->qos_req, freq_limit);
			}
		}
	}
}
#endif

static int mtk_cpu_power_throttling_probe(struct platform_device *pdev)
{
	struct cpufreq_policy *policy;
	struct cpu_pt_policy *pt_policy;
	unsigned int i = 0, j = 0;
	int cpu, ret;
	struct cpu_pt_priv cpu_pt_info;
	struct device_node *np = pdev->dev.of_node;

	for (i = 0; i < 3; i++) {
		cpu_pt_info.lbat_cpu_limit[i] = CPU_LIMIT_FREQ;
		cpu_pt_info.oc_cpu_limit[i] = CPU_LIMIT_FREQ;
	}

	ret = of_property_read_u32_array(np, "lbat_cpu_limit", &cpu_pt_info.lbat_cpu_limit[0], 3);
	if (ret < 0)
		pr_notice("%s: get lbat cpu limit fail %d\n", __func__, ret);

	ret = of_property_read_u32_array(np, "oc_cpu_limit", &cpu_pt_info.oc_cpu_limit[0], 3);
	if (ret < 0)
		pr_notice("%s: get oc cpu limit fail %d\n", __func__, ret);

	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy)
			continue;

		if (policy->cpu == cpu) {
			for (i = 0; i < POWER_THROTTLING_TYPE_MAX; i++) {
				pt_policy = kzalloc(sizeof(*pt_policy), GFP_KERNEL);
				if (!pt_policy)
					return -ENOMEM;

				pt_policy->pt_type = (enum cpu_pt_type)i;
				pt_policy->policy = policy;
				pt_policy->cpu = cpu;

				if (i == LBAT_POWER_THROTTLING)
					pt_policy->cpu_limit = cpu_pt_info.lbat_cpu_limit[j];
				else if (i == OC_POWER_THROTTLING)
					pt_policy->cpu_limit = cpu_pt_info.oc_cpu_limit[j];

				ret = freq_qos_add_request(&policy->constraints,
					&pt_policy->qos_req, FREQ_QOS_MAX,
					FREQ_QOS_MAX_DEFAULT_VALUE);

				if (ret < 0) {
					pr_notice("%s: Fail to add freq constraint (%d)\n",
						__func__, ret);
					return ret;
				}
				list_add_tail(&pt_policy->cpu_pt_list, &pt_policy_list);
			}
			j++;
		}
	}

#if IS_ENABLED(CONFIG_MTK_LOW_BATTERY_POWER_THROTTLING)
	register_low_battery_notify(&cpu_pt_low_battery_cb, LOW_BATTERY_PRIO_CPU_B);
#endif

#if IS_ENABLED(CONFIG_MTK_BATTERY_OC_POWER_THROTTLING)
	register_battery_oc_notify(&cpu_pt_over_current_cb, BATTERY_OC_PRIO_CPU_B);
#endif

	return 0;
}

static int mtk_cpu_power_throttling_remove(struct platform_device *pdev)
{
	struct cpu_pt_policy *pt_policy, *pt_policy_t;

	list_for_each_entry_safe(pt_policy, pt_policy_t, &pt_policy_list, cpu_pt_list) {
		freq_qos_remove_request(&pt_policy->qos_req);
		cpufreq_cpu_put(pt_policy->policy);
		list_del(&pt_policy->cpu_pt_list);
		kfree(pt_policy);
	}

	return 0;
}

static const struct of_device_id cpu_power_throttling_of_match[] = {
	{ .compatible = "mediatek,cpu-power-throttling", },
	{},
};
MODULE_DEVICE_TABLE(of, cpu_power_throttling_of_match);

static struct platform_driver cpu_power_throttling_driver = {
	.probe = mtk_cpu_power_throttling_probe,
	.remove = mtk_cpu_power_throttling_remove,
	.driver = {
		.name = "mtk-cpu_power_throttling",
		.of_match_table = cpu_power_throttling_of_match,
	},
};
module_platform_driver(cpu_power_throttling_driver);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK cpu power throttling driver");
MODULE_LICENSE("GPL");
