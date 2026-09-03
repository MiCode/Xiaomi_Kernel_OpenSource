// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Unisoc Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "data_collector: " fmt

#include <linux/hrtimer.h>
#include <linux/tracepoint.h>
#include <linux/syscore_ops.h>
#include "data_collector_core.h"

#define CREATE_TRACE_POINTS

#define STAT_PERIOD_IN_MS	1000

static struct hrtimer stat_hrtimer;

static void __init statistic_init(void)
{
	vmscan_statistic_init();
}

static enum hrtimer_restart stat_hrtimer_func(struct hrtimer *hrtimer)
{
	period_mem_stat_check();

	hrtimer_forward_now(hrtimer, ms_to_ktime(STAT_PERIOD_IN_MS));

	return HRTIMER_RESTART;
}

static void monitor_hrtimer_resume(void)
{
	hrtimer_start(&stat_hrtimer, ms_to_ktime(STAT_PERIOD_IN_MS),
					HRTIMER_MODE_REL);
}

static int monitor_hrtimer_suspend(void)
{
	hrtimer_cancel(&stat_hrtimer);

	return 0;
}

static struct syscore_ops monitor_hrtimer_syscore_ops = {
	.resume	= monitor_hrtimer_resume,
	.suspend = monitor_hrtimer_suspend,
};

static int __init trace_register(void)
{
	int ret;

	ret = vmscan_trace_register();
	if (ret) {
		pr_err("vmscan_trace_register() error!\n");
		goto out_error;
	}

	ret = sched_trace_register();
	if (ret) {
		pr_err("sched_trace_register() error!\n");
		goto out_unregister_sched_trace;
	}

	return ret;

out_unregister_sched_trace:
	unregister_vmscan_trace_at_init();
out_error:
	return ret;
}

static int __init data_collector_init(void)
{
	int ret;

	statistic_init();
	ret = trace_register();

	hrtimer_init(&stat_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	stat_hrtimer.function = stat_hrtimer_func;
	hrtimer_start(&stat_hrtimer, ms_to_ktime(STAT_PERIOD_IN_MS),
					HRTIMER_MODE_REL);
	register_syscore_ops(&monitor_hrtimer_syscore_ops);

	return ret;
}

late_initcall(data_collector_init);
