// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/ring_buffer.h>
#include <linux/trace_events.h>
#define CREATE_TRACE_POINTS
#include "mtk_ftrace.h"
#include "trace.h"


#ifdef CONFIG_MTK_FTRACER
static unsigned long buf_size = 25165824UL;
static bool boot_trace;
static __init int boot_trace_cmdline(char *str)
{
	boot_trace = true;
	update_buf_size(buf_size);
	return 0;
}
__setup("androidboot.boot_trace", boot_trace_cmdline);

/* If boot tracing is on.Ignore tracing off command.*/
bool boot_ftrace_check(unsigned long trace_en)
{
	bool boot_complete = false;

	if (boot_trace != true || trace_en)
		return false;

#if IS_BUILTIN(CONFIG_MTPROF)
	boot_complete = mt_boot_finish();
#endif
	if (!boot_complete) {
		pr_info("Capturing boot ftrace,Ignore tracing off.\n");
		return true;
	}
	return false;
}

#include <linux/rtc.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/sched/stat.h>


void print_enabled_events(struct trace_buffer *buf, struct seq_file *m)
{
	struct trace_event_call *call;
	struct trace_event_file *file;
	struct trace_array *tr;

	unsigned long usec_rem;
	unsigned long long t;
	struct rtc_time tm_utc, tm;
	struct timeval tv = { 0 };

	if (buf->tr)
		tr = buf->tr;
	else
		return;

	if (tr->name != NULL)
		seq_printf(m, "# instance: %s, enabled events:", tr->name);
	else
		seq_puts(m, "# enabled events:");

	list_for_each_entry(file, &tr->events, list) {
		call = file->event_call;
		if (file->flags & EVENT_FILE_FL_ENABLED)
			seq_printf(m, " %s:%s", call->class->system,
				   trace_event_name(call));
	}

	seq_puts(m, "\n");

	t = sched_clock();
	do_gettimeofday(&tv);
	t = ns2usecs(t);
	usec_rem = do_div(t, USEC_PER_SEC);
	rtc_time_to_tm(tv.tv_sec, &tm_utc);
	rtc_time_to_tm(tv.tv_sec - sys_tz.tz_minuteswest * 60, &tm);

	seq_printf(m, "# kernel time now: %5llu.%06lu\n",
		   t, usec_rem);
	seq_printf(m, "# UTC time:\t%d-%02d-%02d %02d:%02d:%02d.%03u\n",
			tm_utc.tm_year + 1900, tm_utc.tm_mon + 1,
			tm_utc.tm_mday, tm_utc.tm_hour,
			tm_utc.tm_min, tm_utc.tm_sec,
			(unsigned int)tv.tv_usec);
	seq_printf(m, "# android time:\t%d-%02d-%02d %02d:%02d:%02d.%03u\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec,
			(unsigned int)tv.tv_usec);
}

/* ftrace's switch function for MTK solution */
static void ftrace_events_enable(int enable)
{
	if (enable) {
		trace_set_clr_event(NULL, "sched_switch", 1);
		trace_set_clr_event(NULL, "sched_wakeup", 1);
		trace_set_clr_event(NULL, "sched_wakeup_new", 1);
		trace_set_clr_event(NULL, "sched_stat_iowait", 1);
#ifdef CONFIG_SMP
		trace_set_clr_event(NULL, "sched_migrate_task", 1);
#endif
		trace_set_clr_event(NULL, "workqueue_execute_start", 1);
		trace_set_clr_event(NULL, "workqueue_execute_end", 1);
		trace_set_clr_event(NULL, "cpu_frequency", 1);

		trace_set_clr_event(NULL, "block_bio_frontmerge", 1);
		trace_set_clr_event(NULL, "block_bio_backmerge", 1);
		trace_set_clr_event(NULL, "block_rq_issue", 1);
		trace_set_clr_event(NULL, "block_rq_insert", 1);
		trace_set_clr_event(NULL, "block_rq_complete", 1);
		trace_set_clr_event(NULL, "block_rq_requeue", 1);
		trace_set_clr_event(NULL, "tracing_on", 1);

		if (boot_trace) {
			trace_set_clr_event("android_fs", NULL, 1);
			trace_set_clr_event(NULL, "sched_blocked_reason", 1);
			/*trace_set_clr_event(NULL, "sched_waking", 1);*/
		} else {
			trace_set_clr_event("ipi", NULL, 1);
			trace_set_clr_event(NULL, "softirq_entry", 1);
			trace_set_clr_event(NULL, "softirq_exit", 1);
			trace_set_clr_event(NULL, "softirq_raise", 1);
			trace_set_clr_event(NULL, "irq_handler_entry", 1);
			trace_set_clr_event(NULL, "irq_handler_exit", 1);
#ifdef CONFIG_MTK_SCHED_MONITOR
			trace_set_clr_event(NULL, "sched_mon_msg", 1);
#endif
#ifdef CONFIG_LOCKDEP
			trace_set_clr_event(NULL, "lock_dbg", 1);
			trace_set_clr_event(NULL, "lock_monitor", 1);
#endif
#ifdef CONFIG_RCU_TRACE
			trace_set_clr_event(NULL, "rcu_batch_start", 1);
			trace_set_clr_event(NULL, "rcu_batch_end", 1);
			trace_set_clr_event(NULL, "rcu_kfree_callback", 1);
			trace_set_clr_event(NULL, "rcu_callback", 1);
#endif
		}

		tracing_on();
	} else {
		tracing_off();
		trace_set_clr_event(NULL, NULL, 0);
	}
}

#ifdef CONFIG_MTK_PERF_TRACKER
extern int perf_tracker_enable(int on);
#endif
static __init int boot_ftrace(void)
{
	struct trace_array *tr;
	int ret;

	if (boot_trace) {
		tr = top_trace_array();
		if (!tr) {
			pr_info("[ftrace]Error: Tracer list is empty.\n");
			return 0;
		}

#ifdef CONFIG_MTK_PERF_TRACKER
		perf_tracker_enable(1);
#endif
		ret = tracing_update_buffers();
		if (ret != 0)
			pr_debug("unable to expand buffer, ret=%d\n", ret);
#ifdef CONFIG_SCHEDSTATS
		force_schedstat_enabled();
#endif

		ftrace_events_enable(1);
		set_tracer_flag(tr, TRACE_ITER_OVERWRITE, 0);
		pr_debug("[ftrace]boot-time profiling...\n");
	}
	return 0;
}
core_initcall(boot_ftrace);

#ifdef CONFIG_MTK_FTRACE_DEFAULT_ENABLE
static __init int enable_ftrace(void)
{
	int ret;

	if (!boot_trace) {
		/* enable ftrace facilities */
		ftrace_events_enable(1);

		/*
		 * only update buffer eariler
		 * if we want to collect boot-time ftrace
		 * to avoid the boot time impacted by
		 * early-expanded ring buffer
		 */
		ret = tracing_update_buffers();
		if (ret != 0)
			pr_debug("fail to update buffer, ret=%d\n",
				 ret);
		else
			pr_debug("[ftrace]ftrace ready...\n");
	}
	return 0;
}
late_initcall(enable_ftrace);
#endif
#endif

