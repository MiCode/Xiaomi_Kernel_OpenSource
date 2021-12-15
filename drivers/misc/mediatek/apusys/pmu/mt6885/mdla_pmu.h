// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDLA_PMU_H__
#define __MDLA_PMU_H__

#include <linux/types.h>
#include "mdla.h"
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
#include "apusys_device.h"
#endif

extern int get_power_on_status(unsigned int core_id);
extern int cfg_timer_en;

#define MDLA_PMU_COUNTERS 15
/* pmu  info handle */
struct mdla_pmu_hnd {
	u32 offset_to_PMU_res_buf0;//base addr: pmu_kva
	u32 offset_to_PMU_res_buf1;
	u8 mode;
	u8 number_of_event;
	u16 event[MDLA_PMU_COUNTERS];
};

struct mdla_pmu_event_handle {
	u32 graph_id;//need confirm apusys mid data type
	u32 number_of_event;
	u32 event_handle[MDLA_PMU_COUNTERS];
};

struct mdla_pmu_result {
	u16 cmd_len;
	u16 cmd_id;
	u32 pmu_val[MDLA_PMU_COUNTERS + 1]; /* global counter + PMU counter*/
};

int pmu_event_write_all(u32 mdlaid, u16 priority);
int pmu_counter_alloc(u32 mdlaid, u32 interface, u32 event);
int pmu_counter_free(u32 mdlaid, int handle);
int pmu_counter_event_save(u32 mdlaid, u32 handle, u32 val);
int pmu_counter_event_get(u32 mdlaid, int handle);
int pmu_counter_event_get_all(u32 mdlaid, u32 out[MDLA_PMU_COUNTERS]);
u32 pmu_counter_get(u32 mdlaid, int handle, u16 priority);
void pmu_counter_get_all(u32 mdlaid, u32 out[MDLA_PMU_COUNTERS], u16 priority);
void pmu_counter_read_all(u32 mdlaid, u32 out[MDLA_PMU_COUNTERS]);
unsigned int pmu_reg_read_with_mdlaid(u32 mdlaid, u32 offset);

void pmu_reg_save(u32 mdlaid, u16 priority);

void pmu_percmd_mode_save(u32 mdlaid, u32 mode, u16 priority);
void pmu_percmd_mode_write(u32 mdlaid, u16 priority);
u32 pmu_get_perf_start(u32 mdlaid, u16 priority);
u32 pmu_get_perf_end(u32 mdlaid, u16 priority);
u32 pmu_get_perf_cycle(u32 mdlaid, u16 priority);

//void pmu_reset_saved_counter(u32 mdlaid);
void pmu_reset_counter_variable(u32 mdlaid, u16 priority);
void pmu_reset_counter(u32 mdlaid);
//void pmu_reset_saved_cycle(u32 mdlaid);
void pmu_reset_cycle_variable(u32 mdlaid, u16 priority);
void pmu_reset_cycle(u32 mdlaid);

void pmu_init(u32 mdlaid);
void pmu_reset(u32 mdlaid);
int pmu_set_reg(u32 mdlaid, u16 priority);
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
int pmu_command_prepare(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority);
#endif
int pmu_apusys_pmu_addr_check(struct apusys_cmd_hnd *apusys_hd);
int pmu_cmd_handle(struct mdla_dev *mdla_info,
	struct apusys_cmd_hnd *apusys_hd, u16 priority);
void pmu_command_counter_prt(
	struct apusys_cmd_hnd *apusys_hd,
	struct mdla_dev *mdla_info,
	u16 priority,
	struct command_entry *ce);

#endif

