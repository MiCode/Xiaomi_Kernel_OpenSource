// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <mt-plat/met_drv.h>
#include "mdla_debug.h"
#include "mdla.h"
#include "mdla_hw_reg.h"
#include "mdla_trace.h"
#include "met_mdlasys_events.h"
#include "apusys_trace.h"
#include <linux/kallsyms.h>
#include <linux/preempt.h>

#define PERIOD_DEFAULT 1000 // in micro-seconds (us)

u64 cfg_period;
int cfg_op_trace;
static int cfg_pmu_int;
int cfg_timer_en;
u32 cfg_eng0;
u32 cfg_eng1;
u32 cfg_eng2;
u32 cfg_eng11;
static u32 mdla_core_bitmask;

#ifdef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
/* TODO, wait for apusys mid porting done */
u8 cfg_apusys_trace;
#endif

void mdla_trace_begin(int core_id, struct command_entry *ce)
{
	char buf[TRACE_LEN];
	char *p;
	int len;
	u32 cmd_num;
	void *cmd;

	if (!ce)
		return;

	if (!cfg_apusys_trace)
		return;

	p = cmd = ce->kva;
	cmd_num = ce->count;

	len = snprintf(buf, sizeof(buf),
		"mdla-%d|tid:%d,fin_cid:%d,total_cmd_num:%d",
		core_id,
		task_pid_nr(current),
		ce->fin_cid,
		cmd_num);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	mdla_perf_debug("%s\n", __func__);
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	trace_async_tag(1, buf);
#endif
}

void mdla_trace_end(int core, int status, struct command_entry *ce)
{
	u64 end = ce->req_end_t;
	char buf[64];
	int len;

	if ((!cfg_apusys_trace) || (!end))
		return;

	mdla_perf_debug("%s\n", __func__);

	len = snprintf(buf, sizeof(buf),
		"mdla-%d|tid:%d,fin_id:%d,preempted:%d",
		core, task_pid_nr(current), ce->fin_cid, status);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	trace_async_tag(0, buf);
#endif
}
/* MET: define to enable MET */
#if defined(MDLA_MET_READY)
#define CREATE_TRACE_POINTS
#include "met_mdlasys_events.h"
#endif

void mdla_dump_prof(int coreid, struct seq_file *s)
{
	int i;
	u32 c[MDLA_PMU_COUNTERS] = {};

#define _SHOW_VAL(t) \
	mdla_print_seq(s, "%s=%lu\n", #t, (unsigned long)cfg_##t)

	_SHOW_VAL(period);
	_SHOW_VAL(op_trace);
	_SHOW_VAL(pmu_int);

	pmu_counter_event_get_all(coreid, c);

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		mdla_print_seq(s, "c%d=0x%x\n", (i+1), c[i]);
}

/*
 * MDLA PMU counter reader
 */
static void mdla_profile_pmu_counter(int core_id)
{
	u32 c[MDLA_PMU_COUNTERS] = {};
#if 0
	struct mdla_dev *mdla_info = &mdla_devices[core_id];

	if (mdla_info->pmu.pmu_hnd->mode == PER_CMD) {
		pmu_reg_save(core_id);
		pmu_command_counter_prt(mdla_info); //temporary disable
	}
#endif

	pmu_counter_read_all(core_id, c);
	mdla_perf_debug("_id=c%d, c1=%u, c2=%u, c3=%u, c4=%u, c5=%u, c6=%u, c7=%u, c8=%u, c9=%u, c10=%u, c11=%u, c12=%u, c13=%u, c14=%u, c15=%u\n",
		core_id,
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
		c[8], c[9], c[10], c[11], c[12], c[13], c[14]);
	trace_mdla_polling(core_id, c);
}

static void mdla_profile_register_read(int core_id)
{
	if (!get_power_on_status(core_id))
		return;
	mdla_profile_pmu_counter(core_id);
}

void mdla_trace_iter(unsigned int core_id)
{
	mutex_lock(&mdla_devices[core_id].power_lock);
	if (cfg_timer_en)
		mdla_profile_register_read(core_id);
	mutex_unlock(&mdla_devices[core_id].power_lock);
}


/* restore trace settings after reset */
int mdla_profile_reset(int core_id, const char *str)
{
	return 0;
}

static void mdla_trace_core_set(int core_id)
{
	mdla_core_bitmask |= (1<<core_id);
}

static void mdla_trace_core_clr(int core_id)
{
	mdla_core_bitmask &= ~(1<<core_id);
}

u32 mdla_trace_core_get(void)
{
	return mdla_core_bitmask;
}


int mdla_profile_init(void)
{
	cfg_period = PERIOD_DEFAULT;
	cfg_op_trace = 0;
	cfg_pmu_int = MDLA_TRACE_MODE_CMD;
	cfg_timer_en = 0;
	mdla_core_bitmask = 0;

	return 0;
}
/*
 * MDLA event based MET funcs
 */
void mdla_met_event_enter(int core, int vmdla_opp,
	int dsp_freq, int ipu_if_freq, int mdla_freq)
{
	#if defined(MDLA_MET_READY)
	trace_mdla_cmd_enter(core, vmdla_opp, dsp_freq,
				ipu_if_freq, mdla_freq);
	#endif
}

void mdla_met_event_leave(int core)
{
	#if defined(MDLA_MET_READY)
	trace_mdla_cmd_leave(core, 0);
	#endif
}


/*
 * MDLA Polling Function
 */
static enum hrtimer_restart mdla_profile_get_res(struct hrtimer *timer)
{
	int i;
	if (!cfg_period || !cfg_timer_en)
		return HRTIMER_NORESTART;
	/*call functions need to be called periodically*/
	for (i = 0; i < mdla_max_num_core; i++) {
		if (mdla_trace_core_get() & (1 << i))
			mdla_profile_register_read(i);
	}

	return HRTIMER_RESTART;
}

static enum hrtimer_restart mdla0_profile_polling(struct hrtimer *timer)
{
	mdla_profile_get_res(timer);
	hrtimer_forward_now(&mdla_devices[0].hr_timer,
		ns_to_ktime(cfg_period * 1000));
	return HRTIMER_RESTART;
}

static enum hrtimer_restart mdla1_profile_polling(struct hrtimer *timer)
{
	mdla_profile_get_res(timer);
	hrtimer_forward_now(&mdla_devices[1].hr_timer,
		ns_to_ktime(cfg_period * 1000));
	return HRTIMER_RESTART;
}

static int mdla_profile_timer_start(u32 mdlaid)
{
	hrtimer_init(&mdla_devices[mdlaid].hr_timer,
		CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	if (mdlaid == 0)
		mdla_devices[mdlaid].hr_timer.function = mdla0_profile_polling;
	else if (mdlaid == 1)
		mdla_devices[mdlaid].hr_timer.function = mdla1_profile_polling;
	hrtimer_start(&mdla_devices[mdlaid].hr_timer,
		ns_to_ktime(cfg_period * 1000),
		HRTIMER_MODE_REL);
	mdla_perf_debug("%s: hrtimer_start()\n", __func__);

	return 0;
}

static int mdla_profile_timer_stop(u32 mdlaid, int wait)
{
	int ret = 0;

	if (wait) {
		hrtimer_cancel(&mdla_devices[mdlaid].hr_timer);
		mdla_perf_debug("%s: hrtimer_cancel()\n", __func__);
	} else {
		ret = hrtimer_try_to_cancel(&mdla_devices[mdlaid].hr_timer);
		mdla_perf_debug("%s: hrtimer_try_to_cancel(): %d\n",
					   __func__, ret);
	}
	return ret;
}

/* protected by cmd_list_lock @ mdla_main.c */
int mdla_profile_start(u32 mdlaid)
{
	mdla_trace_core_set(mdlaid);
#if 0
	pmu_reset(mdlaid);
#endif
	if (!cfg_timer_en)
		return 0;
	if (!mdla_devices[mdlaid].timer_started) {
		mdla_profile_timer_start(mdlaid);
		mdla_devices[mdlaid].timer_started = 1;
	}
	return 0;
}

int mdla_profile_stop(u32 mdlaid, int wait)
{
	mdla_trace_core_clr(mdlaid);
	if ((mdla_devices[mdlaid].timer_started) &&
		mdla_trace_core_get() == 0) {
		mdla_profile_timer_stop(mdlaid, wait);
		mdla_devices[mdlaid].timer_started = 0;
	}

	return 0;
}

int mdla_profile_exit(u32 mdlaid)
{
	cfg_period = 0;
	mdla_profile_stop(mdlaid, 1);

	return 0;
}
