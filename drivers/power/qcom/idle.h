/* Copyright (c) 2007-2009,2012-2014 The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_IDLE_H_
#define _ARCH_ARM_MACH_MSM_IDLE_H_

#define MAX_CPUS_PER_CLUSTER	4
#define MAX_NUM_CLUSTER	4

#ifndef __ASSEMBLY__
#if defined(CONFIG_CPU_V7) || defined(CONFIG_ARM64)
extern unsigned long msm_pm_boot_vector[MAX_NUM_CLUSTER * MAX_CPUS_PER_CLUSTER];
void msm_pm_boot_entry(void);
#else
static inline void msm_pm_boot_entry(void) {}
#endif
#endif
#endif
