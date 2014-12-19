/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/init.h>
#include <soc/qcom/core_ctl.h>

void core_ctl_block_hotplug(void)
{
	get_online_cpus();
}
EXPORT_SYMBOL(core_ctl_block_hotplug);

void core_ctl_unblock_hotplug(void)
{
	put_online_cpus();
}
EXPORT_SYMBOL(core_ctl_unblock_hotplug);

s64 core_ctl_get_time(void)
{
	return ktime_to_ms(ktime_get());
}
EXPORT_SYMBOL(core_ctl_get_time);

struct cpufreq_policy *core_ctl_get_policy(int cpu)
{
	return cpufreq_cpu_get(cpu);
}
EXPORT_SYMBOL(core_ctl_get_policy);

void core_ctl_put_policy(struct cpufreq_policy *policy)
{
	cpufreq_cpu_put(policy);
}
EXPORT_SYMBOL(core_ctl_put_policy);

struct device *core_ctl_find_cpu_device(unsigned cpu)
{
	return get_cpu_device(cpu);
}
EXPORT_SYMBOL(core_ctl_find_cpu_device);

int __ref core_ctl_online_core(unsigned int cpu)
{
	return cpu_up(cpu);
}
EXPORT_SYMBOL(core_ctl_online_core);
