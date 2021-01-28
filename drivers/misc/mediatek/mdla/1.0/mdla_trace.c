// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "mdla_debug.h"

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include "mdla.h"
#include "mdla_hw_reg.h"
#include "mdla_trace.h"
#include "mdla_decoder.h"
#include "met_mdlasys_events.h"
#include <linux/kallsyms.h>
#include <linux/preempt.h>

#define TRACE_LEN 128
#define PERIOD_DEFAULT 1000 // in micro-seconds (us)

static struct hrtimer hr_timer;
u64 cfg_period;
int cfg_op_trace;
int cfg_cmd_trace;
static int cfg_pmu_int;
int cfg_timer_en;
u32 cfg_eng0;
u32 cfg_eng1;
u32 cfg_eng2;
u32 cfg_eng11;
static int timer_started;

static noinline int tracing_mark_write(const char *buf)
{
	trace_printk(buf);
	return 0;
}

void mdla_trace_custom(const char *fmt, ...)
{
	char buf[TRACE_LEN];
	va_list args;
	int len;

	va_start(args, fmt);
	len = vsnprintf(buf, TRACE_LEN, fmt, args);
	va_end(args);

	if (len > 0)
		tracing_mark_write(buf);
}

void mdla_trace_begin(struct command_entry *ce)
{
	char buf[TRACE_LEN];
	char *p;
	int len;
	u32 cmd_num;
	void *cmd;

	if (!ce)
		return;

	if (!cfg_cmd_trace)
		return;

	p = cmd = ce->kva;
	cmd_num = ce->count;

	len = snprintf(buf, sizeof(buf),
		"I|%d|MDLA|B|mdla_cmd_id:%d,mdla_cmd_num:%d",
		task_pid_nr(current),
		1,
		cmd_num);

	if (len <= 0)
		return;
	else if (len >= TRACE_LEN)
		buf[TRACE_LEN - 1] = '\0';

	mdla_perf_debug("%s\n", __func__);
	tracing_mark_write(buf);
}

void mdla_trace_end(struct command_entry *ce)
{
	u64 end = ce->req_end_t;
	u32 cmd_id = ce->count;

	/* EARA Qos*/
	ce->bandwidth = mdla_cmd_qos_end(0);

	if ((!cfg_cmd_trace) || (!end))
		return;

	mdla_perf_debug("%s\n", __func__);
	mdla_trace_custom(
		"I|%d|MDLA|E|mdla_cmd_id:%d,cycle:%u,sched_clock_result:%llu",
		task_pid_nr(current),
		cmd_id,
		pmu_get_perf_cycle(),
		(unsigned long long) end);
}

/* MET: define to enable MET */
#if defined(MDLA_MET_READY)
#define CREATE_TRACE_POINTS
#include "met_mdlasys_events.h"
#endif

void mdla_dump_prof(struct seq_file *s)
{
	int i;
	u32 c[MDLA_PMU_COUNTERS] = {0};

#define _SHOW_VAL(t) \
	mdla_print_seq(s, "%s=%lu\n", #t, (unsigned long)cfg_##t)

	_SHOW_VAL(period);
	_SHOW_VAL(cmd_trace);
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
	u32 c[MDLA_PMU_COUNTERS] = {0};

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

	if (!get_power_on_status())
		return;

	for (i = 0 ; i < MTK_MDLA_CORE; i++)
		mdla_profile_pmu_counter(i);
}

void mdla_trace_iter(void)
{
	mutex_lock(&power_lock);
	if (cfg_timer_en)
		mdla_profile_register_read();
	mutex_unlock(&power_lock);
}


/* restore trace settings after reset */
int mdla_profile_reset(const char *str)
{
	if (cfg_cmd_trace)
		mdla_trace_custom("I|%d|MDLA|C|reset:%s",
			task_pid_nr(current), str);

	return 0;
}

int mdla_profile_init(void)
{
	cfg_period = PERIOD_DEFAULT;
	cfg_op_trace = 0;
	cfg_cmd_trace = 0;
	cfg_pmu_int = MDLA_TRACE_MODE_CMD;
	cfg_timer_en = 0;
	timer_started = 0;

	return 0;
}

#define MDLA_TRACE_TAG_EN 0
void mdla_trace_tag_begin(const char *format, ...)
{
#if MDLA_TRACE_TAG_EN
	char buf[TRACE_LEN];

	int len = snprintf(buf, sizeof(buf),
		"B|%d|%s", format, task_pid_nr(current));

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	tracing_mark_write(buf);
#endif
}

void mdla_trace_tag_end(void)
{
#if MDLA_TRACE_TAG_EN
	char buf[TRACE_LEN];

	int len = snprintf(buf, sizeof(buf), "E\n");


	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	tracing_mark_write(buf);
#endif
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
