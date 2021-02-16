// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/printk.h>
#include <linux/sched/clock.h>
#include <trace/events/sched.h>
#define CREATE_TRACE_POINTS
#include <trace/hooks/restricted_preemptirq.h>
#include <trace/events/preemptirq_long.h>

#define IRQSOFF_SENTINEL 0x0fffDEAD

static unsigned int sysctl_preemptoff_tracing_threshold_ns = 1000000;
static unsigned int sysctl_irqsoff_tracing_threshold_ns = 5000000;
static unsigned int sysctl_irqsoff_dmesg_output_enabled;
static unsigned int sysctl_irqsoff_crash_sentinel_value;
static unsigned int sysctl_irqsoff_crash_threshold_ns = 10000000;

static unsigned int half_million = 500000;
static unsigned int one_hundred_million = 100000000;
static unsigned int one_million = 1000000;

static DEFINE_PER_CPU(u64, irq_disabled_ts);
static DEFINE_PER_CPU(u64, preempt_disabled_ts);

static void __attribute__((__unused__))
	note_irq_disable(void *u1, unsigned long u2, unsigned long u3)
{
	if (is_idle_task(current))
		return;

	/*
	 * We just have to note down the time stamp here. We
	 * use stacktrace trigger feature to print the stacktrace.
	 */
	this_cpu_write(irq_disabled_ts, sched_clock());
}

static void __attribute__((__unused__))
	test_irq_disable_long(void *u1, unsigned long u2, unsigned long u3)
{
	u64 ts = this_cpu_read(irq_disabled_ts);

	if (!ts)
		return;

	this_cpu_write(irq_disabled_ts, 0);
	ts = sched_clock() - ts;

	if (ts > sysctl_irqsoff_tracing_threshold_ns) {
		trace_irq_disable_long(ts);

		if (sysctl_irqsoff_dmesg_output_enabled == IRQSOFF_SENTINEL)
			printk_deferred("D=%llu C:(%ps<-%ps<-%ps<-%ps)\n",
					ts, CALLER_ADDR2, CALLER_ADDR3,
					CALLER_ADDR4, CALLER_ADDR5);
	}

	if (sysctl_irqsoff_crash_sentinel_value == IRQSOFF_SENTINEL &&
			ts > sysctl_irqsoff_crash_threshold_ns) {
		printk_deferred("delta=%llu(ns) > crash_threshold=%llu(ns) Task=%s\n",
				ts, sysctl_irqsoff_crash_threshold_ns,
				current->comm);
		BUG_ON(1);
	}
}

static void __attribute__((__unused__))
	note_preempt_disable(void *u1, unsigned long u2, unsigned long u3)
{
	this_cpu_write(preempt_disabled_ts, sched_clock());
}

static void __attribute__((__unused__))
	test_preempt_disable_long(void *u1, unsigned long u2,
				      unsigned long u3)
{
	u64 ts = this_cpu_read(preempt_disabled_ts);

	if (!ts)
		return;

	this_cpu_write(preempt_disabled_ts, 0);
	ts = sched_clock() - ts;

	if (ts > sysctl_preemptoff_tracing_threshold_ns)
		trace_preempt_disable_long(ts);
}

static void note_context_switch(void *u1, bool u2, struct task_struct *u3,
				struct task_struct *next)
{
	/*
	 * Discard false positives during context switch to idle.
	 */
	if (is_idle_task(next))
		this_cpu_write(preempt_disabled_ts, 0);
}

static struct ctl_table preemptirq_long_table[] = {
	{
		.procname       = "preemptoff_tracing_threshold_ns",
		.data           = &sysctl_preemptoff_tracing_threshold_ns,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec,
	},
	{
		.procname	= "irqsoff_tracing_threshold_ns",
		.data		= &sysctl_irqsoff_tracing_threshold_ns,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= &half_million,
		.extra2		= &one_hundred_million,
	},
	{
		.procname	= "irqsoff_dmesg_output_enabled",
		.data		= &sysctl_irqsoff_dmesg_output_enabled,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "irqsoff_crash_sentinel_value",
		.data		= &sysctl_irqsoff_crash_sentinel_value,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "irqsoff_crash_threshold_ns",
		.data		= &sysctl_irqsoff_crash_threshold_ns,
		.maxlen		= sizeof(unsigned int),
		.mode		= 0644,
		.proc_handler	= proc_douintvec_minmax,
		.extra1		= &one_million,
		.extra2		= &one_hundred_million,
	},
	{ }
};

static int preemptirq_long_init(void)
{
	if (!register_sysctl("preemptirq", preemptirq_long_table)) {
		pr_err("Fail to register sysctl table\n");
		return -EPERM;
	}

	register_trace_restricted_irq_disable(note_irq_disable, NULL);
	register_trace_restricted_irq_enable(test_irq_disable_long, NULL);
	register_trace_restricted_preempt_disable(note_preempt_disable, NULL);
	register_trace_restricted_preempt_enable(test_preempt_disable_long,
						 NULL);
	register_trace_sched_switch(note_context_switch, NULL);

	return 0;
}

module_init(preemptirq_long_init);
MODULE_LICENSE("GPL v2");
