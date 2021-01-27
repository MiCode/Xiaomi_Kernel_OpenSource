// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/rcutree.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <linux/stacktrace.h>
#include <linux/tracepoint.h>
#include <mt-plat/mboot_params.h>

#include <trace/events/ipi.h>
#include <trace/events/irq.h>
#include <trace/events/preemptirq.h>

#include "internal.h"

#define CREATE_TRACE_POINTS
#include "irq_monitor_trace.h"

#ifdef MODULE
const char * const softirq_to_name[NR_SOFTIRQS] = {
	"HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "IRQ_POLL",
	"TASKLET", "SCHED", "HRTIMER", "RCU"
};

static inline void irq_mon_msg_ftrace(const char *str)
{
	if (rcu_is_watching())
		trace_irq_mon_msg(str);
}
#else
#define irq_mon_msg_ftrace(str) trace_irq_mon_msg_rcuidle(str)
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#define pr_aee_sram(msg) aee_sram_fiq_log(msg)
#else
#define pr_aee_sram(msg) do {} while (0)
#endif

void irq_mon_msg(int out, char *buf, ...)
{
	char str[128];
	va_list args;

	va_start(args, buf);
	vsnprintf(str, sizeof(str), buf, args);
	va_end(args);

	if (out & TO_FTRACE)
		irq_mon_msg_ftrace(str);
	if (out & TO_KERNEL_LOG)
		pr_info("%s\n", str);
	if (out & TO_SRAM) {
		pr_aee_sram(str);
		pr_aee_sram("\n");
	}
}

struct irq_mon_tracer {
	bool tracing;
	unsigned int th1_ms;          /* ftrace */
	unsigned int th2_ms;          /* ftrace + kernel log */
	unsigned int th3_ms;          /* aee */
	unsigned int aee_limit;
	unsigned int aee_debounce_ms;
};

static struct irq_mon_tracer irq_handler_tracer __read_mostly = {
	.tracing = true,
	.th1_ms = 100,
	.th2_ms = 500
};

static struct irq_mon_tracer irq_off_tracer __read_mostly = {
	.tracing = true,
	.th1_ms = 9,
	.th2_ms = 500,
	.th3_ms = 500,
	.aee_debounce_ms = 60000
};

static struct irq_mon_tracer preempt_off_tracer __read_mostly = {
	.tracing = true,
	.th1_ms = 60000,
	.th2_ms = 180000,
};

/* structues of probe funcitons */

#define MAX_STACK_TRACE_DEPTH   32

struct trace_stat {
	bool tracing;
	unsigned long long start_timestamp;
	unsigned long long end_timestamp;
	/* start trace */
	int nr_entries;
	unsigned long trace_entries[MAX_STACK_TRACE_DEPTH];
};

struct preemptirq_stat {
	bool tracing;
	bool enable_locked;
	unsigned long long disable_timestamp;
	unsigned long disable_ip;
	unsigned long disable_parent_ip;
	unsigned long long enable_timestamp;
	unsigned long enable_ip;
	unsigned long enable_parent_ip;
	/* stack_trace */
	int nr_entries;
	unsigned long trace_entries[MAX_STACK_TRACE_DEPTH];
};

static DEFINE_PER_CPU(struct preemptirq_stat, irq_pi_stat);
static DEFINE_PER_CPU(struct preemptirq_stat, preempt_pi_stat);

static void irq_mon_save_stack_trace(struct preemptirq_stat *pi_stat)
{
	/* init, should move to other place */
	pi_stat->nr_entries = stack_trace_save(pi_stat->trace_entries,
						MAX_STACK_TRACE_DEPTH * sizeof(unsigned long), 2);
}

static void irq_mon_dump_stack_trace(int out, struct preemptirq_stat *pi_stat)
{
	char msg2[128];
	int i;

	irq_mon_msg(out, "disable call trace:");
	for (i = 0; i < pi_stat->nr_entries; i++) {
		scnprintf(msg2, sizeof(msg2), "[<%p>] %pS",
			 (void *)pi_stat->trace_entries[i],
			 (void *)pi_stat->trace_entries[i]);
		irq_mon_msg(out, "%s", msg2);
	}
}

/* irq: 1 = irq, 0 = preempt*/
static void check_preemptirq_stat(struct preemptirq_stat *pi_stat, int irq)
{
	unsigned long long threshold, duration;
	int out;

	/* skip <idle-0> task */
	if (current->pid == 0)
		return;

	duration = pi_stat->enable_timestamp - pi_stat->disable_timestamp;

	/* threshold 1: ftrace */
	threshold = irq ? irq_off_tracer.th1_ms : preempt_off_tracer.th1_ms;
	if (likely(duration < threshold * 1000000ULL))
		return;

	/* threshold 2: ftrace + kernel log */
	threshold = irq ? irq_off_tracer.th2_ms : preempt_off_tracer.th2_ms;
	out = (duration >= threshold * 1000000ULL) ? TO_BOTH : TO_FTRACE;

	irq_mon_msg(out, "%s off, duration %llu ms, from %llu ns to %llu ns",
			irq ? "irq" : "preempt",
			msec_high(duration),
			pi_stat->disable_timestamp,
			pi_stat->enable_timestamp);
	irq_mon_msg(out, "disable_ip       : [<%p>] %pS",
			(void *)pi_stat->disable_ip, (void *)pi_stat->disable_ip);
	irq_mon_msg(out, "disable_parent_ip: [<%p>] %pS",
			(void *)pi_stat->disable_parent_ip, (void *)pi_stat->disable_parent_ip);
	irq_mon_msg(out, "enable_ip        : [<%p>] %pS",
			(void *)pi_stat->enable_ip, (void *)pi_stat->enable_ip);
	irq_mon_msg(out, "enable_parent_ip : [<%p>] %pS",
			(void *)pi_stat->enable_parent_ip, (void *)pi_stat->enable_parent_ip);
	irq_mon_dump_stack_trace(out, pi_stat);

	if (out & TO_KERNEL_LOG)
		dump_stack();
}

static DEFINE_PER_CPU(struct trace_stat, irq_trace_stat);
static DEFINE_PER_CPU(struct trace_stat, softirq_trace_stat);
static DEFINE_PER_CPU(struct trace_stat, ipi_trace_stat);

#define stat_dur(stat) (stat->end_timestamp - stat->start_timestamp)
#define th_exceeded(threshold, duration, tracer) \
	(duration > (unsigned long long)tracer.threshold * 1000000ULL)

#define dur_fmt "duration: %lld us, start: %llu.%06lu, end:%llu.%06lu"

#define show_irq_handle(out, type, stat) \
	irq_mon_msg(out, "%s%s, " dur_fmt,\
		stat->end_timestamp ? "In " : "", type, \
		stat->end_timestamp ? msec_high(stat_dur(stat)) : 0, \
		sec_high(stat->start_timestamp), \
		sec_low(stat->start_timestamp), \
		sec_high(stat->end_timestamp), \
		sec_low(stat->end_timestamp))

static void __show_irq_handle_info(int out)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		irq_mon_msg(out, "CPU: %d", cpu);
		show_irq_handle(out, "irq handler", per_cpu_ptr(&irq_trace_stat, cpu));
		show_irq_handle(out, "softirq", per_cpu_ptr(&softirq_trace_stat, cpu));
		show_irq_handle(out, "IPI", per_cpu_ptr(&ipi_trace_stat, cpu));
		irq_mon_msg(out, "");
	}
}

static void show_irq_handle_info(int out)
{
	if (irq_handler_tracer.tracing)
		__show_irq_handle_info(out);
}

void mt_aee_dump_irq_info(void)
{
	show_irq_handle_info(TO_SRAM);
	show_irq_count_info(TO_SRAM);
}
EXPORT_SYMBOL_GPL(mt_aee_dump_irq_info);

/* probe functions*/

static void probe_irq_handler_entry(void *ignore,
		int irq, struct irqaction *action)
{
	unsigned long long ts;

	if (__this_cpu_cmpxchg(irq_trace_stat.tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(irq_trace_stat.start_timestamp, ts);
	this_cpu_write(irq_trace_stat.end_timestamp, 0);
}

static void probe_irq_handler_exit(void *ignore,
		int irq, struct irqaction *action, int ret)
{
	struct trace_stat *trace_stat = raw_cpu_ptr(&irq_trace_stat);
	unsigned long long ts, duration;
	int out;

	if (!__this_cpu_read(irq_trace_stat.tracing))
		return;

	ts = sched_clock();
	trace_stat->end_timestamp = ts;

	duration = stat_dur(trace_stat);
	if (th_exceeded(th1_ms, duration, irq_handler_tracer)) {
		out = th_exceeded(th2_ms, duration, irq_handler_tracer) ?
			TO_BOTH : TO_FTRACE;

		irq_mon_msg(out, "irq: %d %pS, duration %llu ms, from %llu ns to %llu ns",
				irq, (void *)action->handler,
				msec_high(duration),
				trace_stat->start_timestamp,
				trace_stat->end_timestamp);
	}

	this_cpu_write(irq_trace_stat.tracing, 0);
}

static void probe_softirq_entry(void *ignore, unsigned int vec_nr)
{
	unsigned long long ts;

	if (__this_cpu_cmpxchg(softirq_trace_stat.tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(softirq_trace_stat.start_timestamp, ts);
	this_cpu_write(softirq_trace_stat.end_timestamp, 0);
}

static void probe_softirq_exit(void *ignore, unsigned int vec_nr)
{
	struct trace_stat *trace_stat = raw_cpu_ptr(&softirq_trace_stat);
	unsigned long long ts, duration;
	int out;

	if (!__this_cpu_read(softirq_trace_stat.tracing))
		return;

	ts = sched_clock();
	trace_stat->end_timestamp = ts;

	duration = stat_dur(trace_stat);
	if (th_exceeded(th1_ms, duration, irq_handler_tracer)) {
		out = th_exceeded(th2_ms, duration, irq_handler_tracer) ?
			TO_BOTH : TO_FTRACE;

		irq_mon_msg(out, "softirq: %u %s, duration %llu ms, from %llu ns to %llu ns",
				vec_nr, softirq_to_name[vec_nr],
				msec_high(duration),
				trace_stat->start_timestamp,
				trace_stat->end_timestamp);
	}

	this_cpu_write(softirq_trace_stat.tracing, 0);
}

static void probe_ipi_entry(void *ignore, const char *reason)
{
	unsigned long long ts;

	if (__this_cpu_cmpxchg(ipi_trace_stat.tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(ipi_trace_stat.start_timestamp, ts);
	this_cpu_write(ipi_trace_stat.end_timestamp, 0);
}


static void probe_ipi_exit(void *ignore, const char *reason)
{
	struct trace_stat *trace_stat = raw_cpu_ptr(&ipi_trace_stat);
	unsigned long long ts, duration;
	int out;

	if (!__this_cpu_read(ipi_trace_stat.tracing))
		return;

	ts = sched_clock();
	trace_stat->end_timestamp = ts;

	duration = stat_dur(trace_stat);
	if (th_exceeded(th1_ms, duration, irq_handler_tracer)) {
		out = th_exceeded(th2_ms, duration, irq_handler_tracer) ?
			TO_BOTH : TO_FTRACE;

		irq_mon_msg(out, "ipi: %s, duration %llu ms, from %llu ns to %llu ns",
			reason,
			msec_high(duration),
			trace_stat->start_timestamp,
			trace_stat->end_timestamp);
	}

	this_cpu_write(ipi_trace_stat.tracing, 0);
}

static void probe_irq_disable(void *ignore,
		unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(&irq_pi_stat);
	unsigned long long ts;

	if (__this_cpu_cmpxchg(irq_pi_stat.tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(irq_pi_stat.disable_timestamp, ts);
	this_cpu_write(irq_pi_stat.disable_ip, ip);
	this_cpu_write(irq_pi_stat.disable_parent_ip, parent_ip);

	/* high overhead */
	irq_mon_save_stack_trace(pi_stat);
}

static void probe_irq_enable(void *ignore,
		unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(&irq_pi_stat);
	unsigned long long ts;

	if (!__this_cpu_read(irq_pi_stat.tracing))
		return;

	if (!__this_cpu_cmpxchg(irq_pi_stat.enable_locked, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(irq_pi_stat.enable_timestamp, ts);
	this_cpu_write(irq_pi_stat.enable_ip, ip);
	this_cpu_write(irq_pi_stat.enable_parent_ip, parent_ip);

	check_preemptirq_stat(pi_stat, 1);

	this_cpu_write(irq_pi_stat.enable_locked, 0);
	this_cpu_write(irq_pi_stat.tracing, 0);
}

static void probe_preempt_disable(void *ignore
		, unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(&preempt_pi_stat);
	unsigned long long ts;

	if (__this_cpu_cmpxchg(preempt_pi_stat.tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(preempt_pi_stat.disable_timestamp, ts);
	this_cpu_write(preempt_pi_stat.disable_ip, ip);
	this_cpu_write(preempt_pi_stat.disable_parent_ip, parent_ip);

	/* high overhead */
	irq_mon_save_stack_trace(pi_stat);
}

static void probe_preempt_enable(void *ignore,
		unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(&preempt_pi_stat);
	unsigned long long ts;

	if (!__this_cpu_read(preempt_pi_stat.tracing))
		return;

	if (!__this_cpu_cmpxchg(preempt_pi_stat.enable_locked, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(preempt_pi_stat.enable_timestamp, ts);
	this_cpu_write(preempt_pi_stat.enable_ip, ip);
	this_cpu_write(preempt_pi_stat.enable_parent_ip, parent_ip);

	check_preemptirq_stat(pi_stat, 0);

	this_cpu_write(preempt_pi_stat.enable_locked, 0);
	this_cpu_write(preempt_pi_stat.tracing, 0);
}

struct irq_mon_tracepoint {
	struct tracepoint *tp;
	const char *name;
	void *func;
	void *data;
	bool probe;
};

struct irq_mon_tracepoint irq_mon_tracepoint_table[] = {
	/* irq.h */
	{.name = "irq_handler_entry", .func = probe_irq_handler_entry, .data = NULL},
	{.name = "irq_handler_exit", .func = probe_irq_handler_exit, .data = NULL},
	{.name = "softirq_entry", .func = probe_softirq_entry, .data = NULL},
	{.name = "softirq_exit", .func = probe_softirq_exit, .data = NULL},
	/* ipi.h */
	{.name = "ipi_entry", .func = probe_ipi_entry, .data = NULL},
	{.name = "ipi_exit", .func = probe_ipi_exit, .data = NULL},
	/* irqoff_tracer */
	{.name = "irq_disable", .func = probe_irq_disable, .data = NULL},
	{.name = "irq_enable", .func = probe_irq_enable, .data = NULL},
	/* preempt_tracer */
	{.name = "preempt_disable", .func = probe_preempt_disable, .data = NULL},
	{.name = "preempt_enable", .func = probe_preempt_enable, .data = NULL},
	/* The last item must be NULL!! */
	{.name = NULL, .func = NULL, .data = NULL}
};

/* lookup tracepoints */
static void irq_mon_tracepoint_lookup(struct tracepoint *tp, void *priv)
{
	struct irq_mon_tracepoint *t;

	if (!tp || !tp->name)
		return;

	t = (struct irq_mon_tracepoint *)priv;

	for (; t->name != NULL; t++) {
		if (!strcmp(t->name, tp->name)) {
			t->tp = tp;
			pr_info("found tp: %s,%pS\n", tp->name, tp);
			break;
		}
	}
}

/* probe tracepoints for all irq_monitor tracers */
static int irq_mon_tracepoint_init(void)
{
	struct irq_mon_tracepoint *t;

	for_each_kernel_tracepoint(irq_mon_tracepoint_lookup
					, (void *)irq_mon_tracepoint_table);

	for (t = irq_mon_tracepoint_table; t->name != NULL; t++) {
		int ret;

		if (!t->tp) {
			pr_info("tp: %s not found\n", t->name);
			continue;
		}
		ret = tracepoint_probe_register(t->tp, t->func, t->data);
		if (ret) {
			pr_info("tp: %s probe failed\n", t->name);
			continue;
		}
		pr_info("tp: %s,%pS probed\n", t->name, t->tp);
		t->probe = true;
	}
	return 0;
}

static void irq_mon_proc_init(void)
{
	struct proc_dir_entry *dir;

	dir = proc_mkdir("mtmon", NULL);
	if (!dir)
		return;
	mt_irq_monitor_test_init(dir);
}

static int __init irq_monitor_init(void)
{
	// tracepoint init
	pr_info("irq monitor init start!!\n");
	irq_mon_tracepoint_init();
	irq_count_tracer_init();
	irq_mon_proc_init();

	return 0;
}

/* exit */
static int irq_mon_tracepoint_exit(void)
{
	struct irq_mon_tracepoint *t;

	for (t = irq_mon_tracepoint_table; t->name != NULL; t++) {
		if (!t->tp)
			continue;
		if (t->probe) {
			tracepoint_probe_unregister(t->tp, t->func, t->data);
			t->probe = false;
		}
	}
	tracepoint_synchronize_unregister();
	return 0;
}

static void __exit irq_monitor_exit(void)
{
	irq_mon_tracepoint_exit();
	remove_proc_subtree("mtmon", NULL);
}

MODULE_DESCRIPTION("MEDIATEK IRQ MONITOR");
MODULE_LICENSE("GPL v2");
early_initcall(irq_monitor_init);
module_exit(irq_monitor_exit);
