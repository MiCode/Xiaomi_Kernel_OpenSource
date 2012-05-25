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
 */

#ifndef __ARCH_ARM_MACH_MSM_MACH_CPUFREQ_H
#define __ARCH_ARM_MACH_MSM_MACH_CPUFREQ_H

#define MSM_CPUFREQ_NO_LIMIT 0xFFFFFFFF

#ifdef CONFIG_CPU_FREQ_MSM

/**
 * msm_cpufreq_set_freq_limit() - Set max/min freq limits on cpu
 *
 * @cpu: The cpu core for which the limits apply
 * @max: The max frequency allowed
 * @min: The min frequency allowed
 *
 * If the @max or @min is set to MSM_CPUFREQ_NO_LIMIT, the limit
 * will default to the CPUFreq limit.
 *
 * returns 0 on success, errno on failure
 */
extern int msm_cpufreq_set_freq_limits(
		uint32_t cpu, uint32_t min, uint32_t max);
#else
static inline int msm_cpufreq_set_freq_limits(
		uint32_t cpu, uint32_t min, uint32_t max)
{
	return -ENOSYS;
}
#endif

#endif /* __ARCH_ARM_MACH_MSM_MACH_CPUFREQ_H */
