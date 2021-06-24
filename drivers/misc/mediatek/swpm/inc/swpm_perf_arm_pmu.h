/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_PERF_ARM_PMU_H__
#define __SWPM_PERF_ARM_PMU_H__

extern int swpm_arm_pmu_get_status(void);
extern int swpm_arm_pmu_enable_all(unsigned int enable);

#endif