/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MDLA_PMU_H__
#define __MDLA_PMU_H__

#include <linux/types.h>

extern int get_power_on_status(void);
extern struct mutex power_lock;
extern struct mutex cmd_lock;
#define MDLA_PMU_COUNTERS 15

unsigned int pmu_reg_read(u32 offset);

int pmu_counter_alloc(u32 interface, u32 event);
int pmu_counter_free(int handle);
int pmu_counter_event_save(u32 handle, u32 val);
int pmu_counter_event_get(int handle);
int pmu_counter_event_get_all(u32 out[MDLA_PMU_COUNTERS]);
u32 pmu_counter_get(int handle);
void pmu_counter_get_all(u32 out[MDLA_PMU_COUNTERS]);
void pmu_counter_read_all(u32 out[MDLA_PMU_COUNTERS]);

void pmu_reg_save(void);
void pmu_clr_mode_save(u32 mode);

u32 pmu_get_perf_start(void);
u32 pmu_get_perf_end(void);
u32 pmu_get_perf_cycle(void);

void pmu_reset_saved_counter(void);
void pmu_reset_saved_cycle(void);

void pmu_init(void);
void pmu_reset(void);

#endif

