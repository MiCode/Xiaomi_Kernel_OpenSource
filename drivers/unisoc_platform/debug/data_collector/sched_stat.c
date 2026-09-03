// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "direct_r_sched_stat: " fmt

#include <linux/sched/clock.h>
#include <linux/unisoc_vd_def.h>
#include <trace/events/sched.h>
#include "data_collector_core.h"

#define CREATE_TRACE_POINTS

static void monitor_for_direct_r_stat(void *data, bool preempt, struct task_struct *prev,
				struct task_struct *next)
{
	struct uni_task_struct *prev_tsk = (struct uni_task_struct *) prev->android_vendor_data1;
	struct uni_task_struct *next_tsk = (struct uni_task_struct *) next->android_vendor_data1;
	unsigned long long dur;

	if (prev_tsk->dr_thread_in_statistic) {
		dur = sched_clock();
		if (dur > prev_tsk->dr_thread_timestamp_start) {
			dur -= prev_tsk->dr_thread_timestamp_start;
			prev_tsk->dr_thread_duration += dur;
		}
	}

	if (next_tsk->dr_thread_in_statistic)
		next_tsk->dr_thread_timestamp_start = sched_clock();
}

int __init sched_trace_register(void)
{
	int ret;

	// kernel/sched/core.c: EXPORT_TRACEPOINT_SYMBOL_GPL(sched_switch);
	ret = register_trace_sched_switch(monitor_for_direct_r_stat, NULL);
	if (ret)
		pr_err("D_COLLECTOR: register trace_sched_switch failed!\n");

	return ret;
}
