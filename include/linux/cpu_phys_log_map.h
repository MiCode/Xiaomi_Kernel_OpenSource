/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/types.h>

#ifndef _CPU_PHYS_LOG_MAP_H
#define _CPU_PHYS_LOG_MAP_H

#if IS_ENABLED(CONFIG_QCOM_CPU_PHYS_LOG_MAP)
int cpu_logical_to_phys(int cpu);
#else
static inline int cpu_logical_to_phys(int cpu)
{
	return cpu;
}
#endif

#endif /* _CPU_PHYS_LOG_MAP_H */
