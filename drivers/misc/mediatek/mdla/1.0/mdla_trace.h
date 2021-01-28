/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MDLA_TRACE_H__
#define __MDLA_TRACE_H__
#include "mdla_qos.h"
#include "mdla_pmu.h"
extern u64 cfg_period;
extern int cfg_op_trace;
extern int cfg_cmd_trace;
extern int cfg_timer_en;
extern u32 cfg_eng0;
extern u32 cfg_eng1;
extern u32 cfg_eng2;
extern u32 cfg_eng11;
extern int get_power_on_status(void);
extern struct mutex power_lock;
enum {
	MDLA_TRACE_MODE_CMD = 0,
	MDLA_TRACE_MODE_INT = 1
};
extern u32 cfg_pmu_event[MDLA_PMU_COUNTERS];

#define MDLA_MET_READY 1

#include <linux/sched.h>

int mdla_profile_init(void);
int mdla_profile_exit(void);
int mdla_profile_reset(const char *str);
int mdla_profile(const char *str);
int mdla_profile_power_mode(u32 *stat);
void mdla_dump_prof(struct seq_file *s);
void mdla_trace_begin(struct command_entry *ce);
void mdla_trace_iter(void);
int mdla_profile_start(void);
int mdla_profile_stop(int type);
void mdla_trace_end(struct command_entry *ce);
void mdla_trace_tag_begin(const char *format, ...);
void mdla_trace_tag_end(void);
void mdla_met_event_enter(int core, int vmdla_opp,
	int dsp_freq, int ipu_if_freq, int mdla_freq);
void mdla_met_event_leave(int core);

#endif

