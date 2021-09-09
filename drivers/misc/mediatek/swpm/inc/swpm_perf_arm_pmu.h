/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_PERF_ARM_PMU_H__
#define __SWPM_PERF_ARM_PMU_H__

enum swpm_perf_evt_id {
	L3DC_EVT,
	INST_SPEC_EVT,
	CYCLES_EVT,
	L3DC_REFILL_EVT,
};

extern int swpm_arm_pmu_get_status(void);
extern int swpm_arm_pmu_get_idx(unsigned int evt_id,
				unsigned int cpu);
extern int swpm_arm_pmu_enable_all(unsigned int enable);

#endif
