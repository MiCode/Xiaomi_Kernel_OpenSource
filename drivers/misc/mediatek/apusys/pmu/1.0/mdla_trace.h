// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDLA_TRACE_H__
#define __MDLA_TRACE_H__
#include "mdla_pmu.h"
extern u64 cfg_period;
extern int cfg_op_trace;
extern int cfg_timer_en;
extern u32 cfg_eng0;
extern u32 cfg_eng1;
extern u32 cfg_eng2;
extern u32 cfg_eng11;
extern u8 cfg_apusys_trace;
extern int get_power_on_status(unsigned int core_id);
extern void trace_tag_begin(const char *format, ...);
extern void trace_tag_end(void);
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
extern void trace_async_tag(bool isBegin, const char *format, ...);
#endif

enum {
	MDLA_TRACE_MODE_CMD = 0,
	MDLA_TRACE_MODE_INT = 1
};
extern u32 cfg_pmu_event[MTK_MDLA_MAX_NUM][MDLA_PMU_COUNTERS];
extern u32 cfg_pmu_event_trace[MDLA_PMU_COUNTERS];
#ifdef CONFIG_MTK_MDLA_DEBUG

#define MDLA_MET_READY 1

#include <linux/sched.h>
#include <mt-plat/met_drv.h>

int mdla_profile_init(void);
int mdla_profile_exit(u32 mdlaid);
int mdla_profile_reset(int core, const char *str);
int mdla_profile(const char *str);
int mdla_profile_power_mode(u32 *stat);
void mdla_dump_prof(int coreid, struct seq_file *s);
void mdla_trace_begin(int core, struct command_entry *ce);
void mdla_trace_iter(unsigned int core_id);
int mdla_profile_start(u32 mdlaid);
int mdla_profile_stop(u32 mdlaid, int wait);
void mdla_trace_end(int core, int status,
		    struct command_entry *ce);
void mdla_met_event_enter(int core, int vmdla_opp,
	int dsp_freq, int ipu_if_freq, int mdla_freq);
void mdla_met_event_leave(int core);
#else

static inline int mdla_profile_init(void)
{
	return 0;
}
static int mdla_profile_exit(u32 mdlaid)
{
	return 0;
}
static inline int mdla_profile_reset(int core_id, const char *str)
{
	return 0;
}
static inline int mdla_profile_start(u32 mdlaid)
{
	return 0;
}
static inline int mdla_profile_stop(u32 mdlaid, int wait)
{
	return 0;
}
static inline
int mdla_profile_power_mode(u32 *stat)
{
	return 1;
}
static inline void mdla_dump_prof(int coreid, struct seq_file *s)
{
}
static inline void mdla_trace_begin(int core, struct command_entry *ce)
{
}
void mdla_trace_iter(unsigned int core_id)
{
}
static inline void mdla_trace_end(int core, long status,
				  struct command_entry *ce)
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

