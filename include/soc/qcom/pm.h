/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2019, The Linux Foundation. All rights reserved.
 * Author: San Mehat <san@android.com>
 */

#ifndef __ARCH_ARM_MACH_MSM_PM_H
#define __ARCH_ARM_MACH_MSM_PM_H

#include <linux/types.h>
#include <linux/cpuidle.h>

struct latency_level {
	int affinity_level;
	int reset_level;
	const char *level_name;
};

#ifdef CONFIG_MSM_PM

s32 msm_cpuidle_get_deep_idle_latency(void);

#else

static inline s32 msm_cpuidle_get_deep_idle_latency(void) { return 0; }

#endif

#endif  /* __ARCH_ARM_MACH_MSM_PM_H */
