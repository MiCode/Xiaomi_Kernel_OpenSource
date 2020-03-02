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

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <mt-plat/met_drv.h>
#include "mdla_debug.h"
#include "mdla.h"
#include "mdla_hw_reg.h"
#include "mdla_trace.h"
#include "mdla_decoder.h"
#include "met_mdlasys_events.h"
#include "apusys_trace.h"
#include <linux/kallsyms.h>
#include <linux/preempt.h>

#define PERIOD_DEFAULT 1000 // in micro-seconds (us)

static struct hrtimer hr_timer;
u64 cfg_period;
int cfg_op_trace;
static int cfg_pmu_int;
int cfg_timer_en;
u32 cfg_eng0;
u32 cfg_eng1;
u32 cfg_eng2;
u32 cfg_eng11;
static int timer_started;

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
		"mdla-%d,tid:%d,mdla_cmd_id:%d,mdla_cmd_num:%d",
		core_id,
		task_pid_nr(current),
		1,
		cmd_num);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	mdla_perf_debug("%s\n", __func__);
	//trace_tag_begin(buf);
}

void mdla_trace_end(struct command_entry *ce)
{
	u64 end = ce->req_end_t;

	if ((!cfg_apusys_trace) || (!end))
		return;

	mdla_perf_debug("%s\n", __func__);
	//trace_tag_end();
}

/* MET: define to enable MET */
#if defined(MDLA_MET_READY)
#define CREATE_TRACE_POINTS
#include "met_mdlasys_events.h"
#endif

void mdla_dump_prof(struct seq_file *s)
{
	int i;
	u32 c[MDLA_PMU_COUNTERS];

#define _SHOW_VAL(t) \
	mdla_print_seq(s, "%s=%lu\n", #t, (unsigned long)cfg_##t)

	_SHOW_VAL(period);
	_SHOW_VAL(op_trace);
	_SHOW_VAL(pmu_int);

	pmu_counter_event_get_all(c);

	for (i = 0; i < MDLA_PMU_COUNTERS; i++)
		mdla_print_seq(s, "c%d=0x%x\n", (i+1), c[i]);
}

/*
 * MDLA PMU counter reader
 */
static void mdla_profile_pmu_counter(int core)
{
	u32 c[MDLA_PMU_COUNTERS];

	pmu_counter_read_all(c);
	mdla_perf_debug("_id=c%d, c1=%u, c2=%u, c3=%u, c4=%u, c5=%u, c6=%u, c7=%u, c8=%u, c9=%u, c10=%u, c11=%u, c12=%u, c13=%u, c14=%u, c15=%u\n",
		core,
		c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
		c[8], c[9], c[10], c[11], c[12], c[13], c[14]);
	trace_mdla_polling(core, c);
}

static void mdla_profile_register_read(void)
{
	int i = 0;

	if (!get_power_on_status(0))
		return;

	for (i = 0 ; i < MTK_MDLA_CORE; i++)
		mdla_profile_pmu_counter(i);
}

void mdla_trace_iter(int core_id)
{
	mutex_lock(&mdla_devices[core_id].power_lock);
	if (cfg_timer_en)
		mdla_profile_register_read();
	mutex_unlock(&mdla_devices[core_id].power_lock);
}


/* restore trace settings after reset */
int mdla_profile_reset(int core_id, const char *str)
{
	if (!cfg_apusys_trace)
		goto out;
#ifndef __APUSYS_MDLA_SW_PORTING_WORKAROUND__
	trace_tag_customer("C|%d|mdla-%d,reset:%s|0",
			  task_pid_nr(current), core_id, str);
#endif
out:
	return 0;
}

int mdla_profile_init(void)
{
	cfg_period = PERIOD_DEFAULT;
	cfg_op_trace = 0;
	cfg_pmu_int = MDLA_TRACE_MODE_CMD;
	cfg_timer_en = 0;
	timer_started = 0;

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
static enum hrtimer_restart mdla_profile_polling(struct hrtimer *timer)
{
	if (!cfg_period || !cfg_timer_en)
		return HRTIMER_NORESTART;
	/*call functions need to be called periodically*/
	mdla_profile_register_read();

	hrtimer_forward_now(&hr_timer, ns_to_ktime(cfg_period * 1000));
	return HRTIMER_RESTART;
}

static int mdla_profile_timer_start(void)
{

	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = mdla_profile_polling;
	hrtimer_start(&hr_timer, ns_to_ktime(cfg_period * 1000),
			HRTIMER_MODE_REL);
	mdla_perf_debug("%s: hrtimer_start()\n", __func__);

	return 0;
}

static int mdla_profile_timer_stop(int wait)
{
	int ret = 0;

	if (wait) {
		hrtimer_cancel(&hr_timer);
		mdla_perf_debug("%s: hrtimer_cancel()\n", __func__);
	} else {
		ret = hrtimer_try_to_cancel(&hr_timer);
		mdla_perf_debug("%s: hrtimer_try_to_cancel(): %d\n",
					   __func__, ret);
	}
	return ret;
}

/* protected by cmd_list_lock @ mdla_main.c */
int mdla_profile_start(void)
{
	pmu_reset();
	if (!cfg_timer_en)
		return 0;
	if (!timer_started) {
		mdla_profile_timer_start();
		timer_started = 1;
	}
	return 0;
}

int mdla_profile_stop(int wait)
{
	if (timer_started) {
		mdla_profile_timer_stop(wait);
		timer_started = 0;
	}

	return 0;
}

int mdla_profile_exit(void)
{
	cfg_period = 0;
	mdla_profile_stop(1);

	return 0;
}
