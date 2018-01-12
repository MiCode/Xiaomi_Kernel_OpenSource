/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2018, The Linux Foundation. All rights reserved.
 * Author: San Mehat <san@android.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_PM_H
#define __ARCH_ARM_MACH_MSM_PM_H

#include <linux/types.h>
#include <linux/cpuidle.h>
#include <dt-bindings/msm/pm.h>

struct latency_level {
	int affinity_level;
	int reset_level;
	const char *level_name;
};

#ifdef CONFIG_MSM_PM

s32 msm_cpuidle_get_deep_idle_latency(void);

/**
 * lpm_get_latency() - API to get latency for a low power mode
 * @latency_level:	pointer to structure with below elements
 * affinity_level: The level (CPU/L2/CCI etc.) for which the
 *	latency is required.
 *	LPM_AFF_LVL_CPU : CPU level
 *	LPM_AFF_LVL_L2  : L2 level
 *	LPM_AFF_LVL_CCI : CCI level
 * reset_level: Can be passed "LPM_RESET_LVL_GDHS" for
 *	low power mode with control logic power collapse or
 *	"LPM_RESET_LVL_PC" for low power mode with control and
 *	memory logic power collapse or "LPM_RESET_LVL_RET" for
 *	retention mode.
 * level_name: Pointer to the cluster name for which the latency
 *	is required or NULL if the minimum value out of all the
 *	clusters is to be returned. For CPU level, the name of the
 *	L2 cluster to be passed. For CCI it has no effect.
 * @latency:	address to get the latency value.
 *
 * latency value will be for the particular cluster or the minimum
 * value out of all the clusters at the particular affinity_level
 * and reset_level.
 *
 * Return: 0 for success; Error number for failure.
 */
int lpm_get_latency(struct latency_level *level, uint32_t *latency);

#else

static inline s32 msm_cpuidle_get_deep_idle_latency(void) { return 0; }

static inline int lpm_get_latency(struct latency_level *level,
						uint32_t *latency)
{
	return 0;
}
#endif

#endif  /* __ARCH_ARM_MACH_MSM_PM_H */
