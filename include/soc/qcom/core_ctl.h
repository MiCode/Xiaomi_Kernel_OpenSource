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

#ifndef __SOC_QCOM_CORE_CTL_H
#define __SOC_QCOM_CORE_CTL_H

extern void core_ctl_block_hotplug(void);
extern void core_ctl_unblock_hotplug(void);
extern s64 core_ctl_get_time(void);
extern struct cpufreq_policy *core_ctl_get_policy(int cpu);
extern void core_ctl_put_policy(struct cpufreq_policy *policy);
extern struct device *core_ctl_find_cpu_device(unsigned cpu);
extern int core_ctl_online_core(unsigned int cpu);

#endif
