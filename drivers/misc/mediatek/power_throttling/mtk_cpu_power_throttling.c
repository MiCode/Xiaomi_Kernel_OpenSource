// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>

#include "mtk_battery_oc_throttling.h"
#include "mtk_low_battery_throttling.h"
#include "mtk_cpu_power_throttling.h"

#define CPU_LIMIT_FREQ 900000

static LIST_HEAD(pt_policy_list);

static void cpu_pt_low_battery_cb(enum LOW_BATTERY_LEVEL_TAG level)
{
	struct cpu_pt_policy *pt_policy;
	s32 freq_limit;

	if (level <= LOW_BATTERY_LEVEL_2 && level >= LOW_BATTERY_LEVEL_0) {
		if (level != LOW_BATTERY_LEVEL_0)
			freq_limit = CPU_LIMIT_FREQ;
		else
			freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;

		list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
			if (pt_policy->pt_type == LBAT_POWER_THROTTLING)
				freq_qos_update_request(&pt_policy->qos_req, freq_limit);
		}
	}
}

static void cpu_pt_over_current_cb(enum BATTERY_OC_LEVEL_TAG level)
{
	struct cpu_pt_policy *pt_policy;
	s32 freq_limit;

	if (level <= BATTERY_OC_LEVEL_1 && level >= BATTERY_OC_LEVEL_0) {
		if (level != BATTERY_OC_LEVEL_0)
			freq_limit = CPU_LIMIT_FREQ;
		else
			freq_limit = FREQ_QOS_MAX_DEFAULT_VALUE;

		list_for_each_entry(pt_policy, &pt_policy_list, cpu_pt_list) {
			if (pt_policy->pt_type == OC_POWER_THROTTLING)
				freq_qos_update_request(&pt_policy->qos_req, freq_limit);
		}
	}
}

static int __init mtk_cpu_power_throttling_module_init(void)
{
	struct cpufreq_policy *policy;
	struct cpu_pt_policy *pt_policy;
	unsigned int i;
	int cpu, ret;

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
		}
	}

	register_low_battery_notify(&cpu_pt_low_battery_cb, LOW_BATTERY_PRIO_CPU_B);
	register_battery_oc_notify(&cpu_pt_over_current_cb, BATTERY_OC_PRIO_CPU_B);

	return 0;
}

static void __exit mtk_cpu_power_throttling_module_exit(void)
{
	struct cpu_pt_policy *pt_policy, *pt_policy_t;

	list_for_each_entry_safe(pt_policy, pt_policy_t, &pt_policy_list, cpu_pt_list) {
		freq_qos_remove_request(&pt_policy->qos_req);
		list_del(&pt_policy->cpu_pt_list);
		kfree(pt_policy);
	}
}

module_init(mtk_cpu_power_throttling_module_init);
module_exit(mtk_cpu_power_throttling_module_exit);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK cpu power throttling driver");
MODULE_LICENSE("GPL");
