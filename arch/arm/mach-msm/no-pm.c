/*
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>

#include "cpuidle.h"
#include "idle.h"
#include "pm.h"

void arch_idle(void)
{ }

void msm_pm_set_platform_data(struct msm_pm_platform_data *data, int count)
{ }

void msm_pm_set_max_sleep_time(int64_t max_sleep_time_ns) { }
EXPORT_SYMBOL(msm_pm_set_max_sleep_time);

int platform_cpu_disable(unsigned int cpu)
{
	return -ENOSYS;
}

int platform_cpu_kill(unsigned int cpu)
{
	return -ENOSYS;
}

void platform_cpu_die(unsigned int cpu)
{ }
