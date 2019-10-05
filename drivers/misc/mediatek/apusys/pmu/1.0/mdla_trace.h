/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MDLA_TRACE_H__
#define __MDLA_TRACE_H__
#include "mdla_qos.h"
#include "mdla_pmu.h"
extern u64 cfg_period;
extern int cfg_op_trace;
extern int cfg_timer_en;
extern u32 cfg_eng0;
extern u32 cfg_eng1;
extern u32 cfg_eng2;
extern u32 cfg_eng11;
extern int get_power_on_status(int core_id);
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
/* TODO, wait for apusys mid porting done */
extern u8 cfg_apusys_trace;
extern void trace_tag_customer(const char *fmt, ...);
#endif
extern void trace_tag_begin(const char *format, ...);
extern void trace_tag_end(void);

enum {
	MDLA_TRACE_MODE_CMD = 0,
	MDLA_TRACE_MODE_INT = 1
};
extern u32 cfg_pmu_event[MDLA_PMU_COUNTERS];
#ifdef CONFIG_MTK_MDLA_DEBUG

#define MDLA_MET_READY 1

#include <linux/sched.h>
#include <mt-plat/met_drv.h>

int mdla_profile_init(void);
int mdla_profile_exit(void);
int mdla_profile_reset(int core, const char *str);
int mdla_profile(const char *str);
int mdla_profile_power_mode(u32 *stat);
void mdla_dump_prof(struct seq_file *s);
void mdla_trace_begin(int core, struct command_entry *ce);
void mdla_trace_iter(int core_id);
int mdla_profile_start(void);
int mdla_profile_stop(int type);
void mdla_trace_end(struct command_entry *ce);
void mdla_met_event_enter(int core, int vmdla_opp,
	int dsp_freq, int ipu_if_freq, int mdla_freq);
void mdla_met_event_leave(int core);

#else

static inline int mdla_profile_init(void)
{
	return 0;
}
static inline int mdla_profile_exit(void)
{
	return 0;
}
static inline int mdla_profile_reset(int core_id, const char *str)
{
	return 0;
}
static inline int mdla_profile_start(void)
{
	return 0;
}
static inline int mdla_profile_stop(int type)
{
	return 0;
}
static inline
int mdla_profile_power_mode(u32 *stat)
{
	return 1;
}
static inline void mdla_dump_prof(struct seq_file *s)
{
}
static inline void mdla_trace_begin(int core, struct command_entry *ce)
{
}
void mdla_trace_iter(int core_id)
{
}
static inline void mdla_trace_end(struct command_entry *ce)
{
}
static inline void mdla_met_event_enter(int core, int vmdla_opp,
	int dsp_freq, int ipu_if_freq, int mdla_freq)
{
}
static inline void mdla_met_event_leave(int core)
{
}

#endif

#endif

