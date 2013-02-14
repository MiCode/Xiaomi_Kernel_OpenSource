/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CPUIDLE_H
#define __ARCH_ARM_MACH_MSM_CPUIDLE_H

#include <linux/notifier.h>
#include "../../pm.h"

struct msm_cpuidle_state {
	unsigned int cpu;
	int state_nr;
	char *name;
	char *desc;
	enum msm_pm_sleep_mode mode_nr;
};

#ifdef CONFIG_PM
s32 msm_cpuidle_get_deep_idle_latency(void);
#else
static inline s32 msm_cpuidle_get_deep_idle_latency(void) { return 0; }
#endif

#ifdef CONFIG_CPU_IDLE
int msm_cpuidle_init(void);
#else
static inline int msm_cpuidle_init(void) { return -ENOSYS; }
#endif

#endif /* __ARCH_ARM_MACH_MSM_CPUIDLE_H */
