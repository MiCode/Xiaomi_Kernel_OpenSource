/*
 * drivers/cpufreq/cpufreq_monitor.c
 *
 * Copyright (C) 2017 Xiaomi Ltd.
 * Copyright (C) 2019 XiaoMi, Inc.
 * Author: Yang Dongdong <yangdongdong@xiaomi.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cputime.h>
#include <asm/hwconf_manager.h>
#include "cpufreq_stats.h"

static int cpumonitor_freq_update(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct cpufreq_policy *policy;
	struct cpufreq_stats *stats;
	int cpu;
	char buf[1024];
	char name[16];
	int len;
	int i;

	for_each_cpu(cpu, cpu_possible_mask) {
		policy = cpufreq_cpu_get(cpu);
		if (IS_ERR_OR_NULL(policy))
			continue;
		if (policy->fast_switch_enabled) {
			cpufreq_cpu_put(policy);
			continue;
		}
		stats = policy->stats;
		cpufreq_cpu_put(policy);
		memset(name, 0, sizeof(name));
		memset(buf, 0, sizeof(buf));
		snprintf(name, sizeof(name), "cpu%d", cpu);
		len = 0;
		for (i = 0; i < stats->state_num; i++) {
			len += snprintf(buf+len, sizeof(buf), "%u %llu; ",
			stats->freq_table[i],
			(unsigned long long)
			jiffies_64_to_clock_t(stats->time_in_state[i]));
		}
		update_hw_monitor_info("cpu_monitor", name, buf);
	}
	return NOTIFY_OK;
}

static struct notifier_block cpumonitor_notifier = {
	.notifier_call  = cpumonitor_freq_update,
};

static int __init cpumonitor_init(void)
{
	int cpu;
	char name[16];

	if (register_hw_monitor_info("cpu_monitor"))
		return -EPERM;

	for_each_cpu(cpu, cpu_possible_mask) {
		memset(name, 0, sizeof(name));
		snprintf(name, sizeof(name), "cpu%d", cpu);
		add_hw_monitor_info("cpu_monitor", name, "0");
	}

	if (hw_monitor_notifier_register(&cpumonitor_notifier))
		return -EPERM;

	return 0;
}

static void __exit cpumonitor_exit(void)
{
	hw_monitor_notifier_unregister(&cpumonitor_notifier);
	unregister_hw_monitor_info("cpu_monitor");
}

module_init(cpumonitor_init);
module_exit(cpumonitor_exit);
