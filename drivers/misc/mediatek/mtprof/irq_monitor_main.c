// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

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
#include <mt-plat/aee.h>
#include <mt-plat/mboot_params.h>
#include <mt-plat/mrdump.h>

#include <trace/events/ipi.h>
#include <trace/events/irq.h>
#include <trace/events/preemptirq.h>

#include "internal.h"

#define CREATE_TRACE_POINTS
#include "irq_monitor_trace.h"

#ifdef MODULE
/* reference kernel/softirq.c */
const char * const softirq_to_name[NR_SOFTIRQS] = {
	"HI", "TIMER", "NET_TX", "NET_RX", "BLOCK", "IRQ_POLL",
	"TASKLET", "SCHED", "HRTIMER", "RCU"
};

static inline void irq_mon_msg_ftrace(const char *msg)
{
	if (rcu_is_watching())
		trace_irq_mon_msg(msg);
}
#else
#define irq_mon_msg_ftrace(msg) trace_irq_mon_msg_rcuidle(msg)
#endif

#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
#define pr_aee_sram(msg) aee_sram_fiq_log(msg)
#else
#define pr_aee_sram(msg) do {} while (0)
#endif


void irq_mon_msg(unsigned int out, char *buf, ...)
{
	char msg[MAX_MSG_LEN];
	va_list args;

	va_start(args, buf);
	vsnprintf(msg, sizeof(msg), buf, args);
	va_end(args);

	if (out & TO_FTRACE)
		irq_mon_msg_ftrace(msg);
	if (out & TO_KERNEL_LOG)
		pr_info("%s\n", msg);
	if (out & TO_SRAM) {
		pr_aee_sram(msg);
		pr_aee_sram("\n");
	}
}

struct irq_mon_tracer {
	bool tracing;
	char *name;
	unsigned int th1_ms;          /* ftrace */
	unsigned int th2_ms;          /* kernel log */
	unsigned int th3_ms;          /* aee */
	unsigned int aee_limit;
	unsigned int aee_debounce_ms;
};

#define OVERRIDE_TH1_MS 50
#define OVERRIDE_TH2_MS 50
#define OVERRIDE_TH3_MS 50

static struct irq_mon_tracer irq_handler_tracer __read_mostly = {
	.tracing = false,
	.name = "irq_handler_tracer",
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_IRQ_TIMER_OVERRIDE)
	.th1_ms = OVERRIDE_TH1_MS,
	.th2_ms = OVERRIDE_TH2_MS,
	.th3_ms = OVERRIDE_TH3_MS,
	.aee_limit = 1,
#else
	.th1_ms = 100,
	.th2_ms = 500,
	.th3_ms = 500,
	.aee_limit = 0,
#endif
};

static struct irq_mon_tracer softirq_tracer __read_mostly = {
	.tracing = false,
	.name = "softirq_tracer",
	.th1_ms = 100,
	.th2_ms = 500,
	.th3_ms = 500,
	.aee_limit = 0,
};

static struct irq_mon_tracer ipi_tracer __read_mostly = {
	.tracing = false,
	.name = "ipi_tracer",
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_IRQ_TIMER_OVERRIDE)
	.th1_ms = OVERRIDE_TH1_MS,
	.th2_ms = OVERRIDE_TH2_MS,
	.th3_ms = OVERRIDE_TH3_MS,
	.aee_limit = 0,
#else
	.th1_ms = 100,
	.th2_ms = 500,
	.th3_ms = 500,
	.aee_limit = 0,
#endif
};

static struct irq_mon_tracer irq_off_tracer __read_mostly = {
	.tracing = false,
	.name = "irq_off_tracer",
	.th1_ms = 9,
	.th2_ms = 500,
	.th3_ms = 500,
	.aee_debounce_ms = 60000
};

static struct irq_mon_tracer preempt_off_tracer __read_mostly = {
	.tracing = false,
	.name = "preempt_off_tracer",
	.th1_ms = 60000,
	.th2_ms = 180000,
};

static struct irq_mon_tracer hrtimer_expire_tracer __read_mostly = {
	.tracing = false,
	.name = "hrtimer_expire_tracer",
#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_IRQ_TIMER_OVERRIDE)
	.th1_ms = OVERRIDE_TH1_MS,
	.th2_ms = OVERRIDE_TH2_MS,
	.th3_ms = OVERRIDE_TH3_MS,
	.aee_limit = 1,
#else
	.th1_ms = 100,
	.th2_ms = 500,
	.th3_ms = 500,
	.aee_limit = 0,
#endif
};


static unsigned int check_threshold(unsigned long long duration,
					struct irq_mon_tracer *tracer)
{
	unsigned int out = 0;

	if (!tracer->tracing)
		return 0;

	if (tracer->th1_ms &&
	    duration >= (unsigned long long)tracer->th1_ms * 1000000ULL)
		out |= TO_FTRACE;
	if (tracer->th2_ms &&
	    duration >= (unsigned long long)tracer->th2_ms * 1000000ULL)
		out |= TO_KERNEL_LOG;
	if (tracer->th3_ms &&
	    duration >= (unsigned long long)tracer->th3_ms * 1000000ULL)
		out |= TO_AEE;

	return out;
}
/* structues of probe funcitons */

#define MAX_STACK_TRACE_DEPTH   32

struct trace_stat {
	bool tracing;
	unsigned long long start_timestamp;
	unsigned long long end_timestamp;
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

static struct preemptirq_stat __percpu *irq_pi_stat;
static struct preemptirq_stat __percpu *preempt_pi_stat;

static void irq_mon_save_stack_trace(struct preemptirq_stat *pi_stat)
{
	pi_stat->nr_entries = stack_trace_save(pi_stat->trace_entries,
						ARRAY_SIZE(pi_stat->trace_entries), 2);
}

static void irq_mon_dump_stack_trace(unsigned int out, struct preemptirq_stat *pi_stat)
{
	char msg[MAX_MSG_LEN];
	int i;

	irq_mon_msg(out, "disable call trace:");
	for (i = 0; i < pi_stat->nr_entries; i++) {
		scnprintf(msg, sizeof(msg), "[<%px>] %pS",
			 (void *)pi_stat->trace_entries[i],
			 (void *)pi_stat->trace_entries[i]);
		irq_mon_msg(out, "%s", msg);
	}
}

/* irq: 1 = irq, 0 = preempt*/
static void check_preemptirq_stat(struct preemptirq_stat *pi_stat, int irq)
{
	struct irq_mon_tracer *tracer =
		(irq) ? &irq_off_tracer : &preempt_off_tracer;
	unsigned long long duration;
	unsigned int out;

	/* skip <idle-0> task */
	if (current->pid == 0)
		return;

	duration = pi_stat->enable_timestamp - pi_stat->disable_timestamp;
	out = check_threshold(duration, tracer);
	if (!out)
		return;

	irq_mon_msg(out, "%s off, duration %llu ms, from %llu ns to %llu ns on CPU:%d",
			irq ? "irq" : "preempt",
			msec_high(duration),
			pi_stat->disable_timestamp,
			pi_stat->enable_timestamp,
			raw_smp_processor_id());
	irq_mon_msg(out, "disable_ip       : [<%px>] %pS",
			(void *)pi_stat->disable_ip,
			(void *)pi_stat->disable_ip);
	irq_mon_msg(out, "disable_parent_ip: [<%px>] %pS",
			(void *)pi_stat->disable_parent_ip,
			(void *)pi_stat->disable_parent_ip);
	irq_mon_msg(out, "enable_ip        : [<%px>] %pS",
			(void *)pi_stat->enable_ip,
			(void *)pi_stat->enable_ip);
	irq_mon_msg(out, "enable_parent_ip : [<%px>] %pS",
			(void *)pi_stat->enable_parent_ip,
			(void *)pi_stat->enable_parent_ip);
	irq_mon_dump_stack_trace(out, pi_stat);

	if (out & TO_KERNEL_LOG)
		dump_stack();
}

static struct trace_stat __percpu *irq_trace_stat;
static struct trace_stat __percpu *softirq_trace_stat;
static struct trace_stat __percpu *ipi_trace_stat;
static struct trace_stat __percpu *hrtimer_trace_stat;

#define MAX_IRQ_NUM 1024
static int irq_aee_state[MAX_IRQ_NUM];

#define stat_dur(stat) (stat->end_timestamp - stat->start_timestamp)

#define dur_fmt "duration: %lld us, start: %llu.%06lu, end:%llu.%06lu"

#define show_irq_handle(out, type, stat) \
	irq_mon_msg(out, "%s%s, " dur_fmt,\
		stat->end_timestamp ? "In " : "", type, \
		stat->end_timestamp ? msec_high(stat_dur(stat)) : 0, \
		sec_high(stat->start_timestamp), \
		sec_low(stat->start_timestamp), \
		sec_high(stat->end_timestamp), \
		sec_low(stat->end_timestamp))

static void __show_irq_handle_info(unsigned int out)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		irq_mon_msg(out, "CPU: %d", cpu);
		show_irq_handle(out, "irq handler", per_cpu_ptr(irq_trace_stat, cpu));
		show_irq_handle(out, "softirq", per_cpu_ptr(softirq_trace_stat, cpu));
		show_irq_handle(out, "IPI", per_cpu_ptr(ipi_trace_stat, cpu));
		show_irq_handle(out, "hrtimer", per_cpu_ptr(hrtimer_trace_stat, cpu));
		irq_mon_msg(out, "");
	}
}

static void show_irq_handle_info(unsigned int out)
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

/* probe functions */

static void probe_irq_handler_entry(void *ignore,
		int irq, struct irqaction *action)
{
	unsigned long long ts;
	struct irq_mon_tracer *tracer = &irq_handler_tracer;

	if (!tracer->tracing)
		return;
	if (__this_cpu_cmpxchg(irq_trace_stat->tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(irq_trace_stat->start_timestamp, ts);
	this_cpu_write(irq_trace_stat->end_timestamp, 0);
}

static void probe_irq_handler_exit(void *ignore,
		int irq, struct irqaction *action, int ret)
{
	struct trace_stat *trace_stat = raw_cpu_ptr(irq_trace_stat);
	struct irq_mon_tracer *tracer = &irq_handler_tracer;
	unsigned long long ts, duration;
	unsigned int out;

	if (!tracer->tracing)
		return;
	if (!__this_cpu_read(irq_trace_stat->tracing))
		return;

	ts = sched_clock();
	trace_stat->end_timestamp = ts;

	duration = stat_dur(trace_stat);
	out = check_threshold(duration, tracer);
	if (out) {
		char msg[MAX_MSG_LEN];
		char handler_name[64];
		const char *irq_name = irq_to_name(irq);

		scnprintf(msg, sizeof(msg),
			"irq: %d [<%px>]%pS, duration %llu ms, from %llu ns to %llu ns on CPU:%d",
			irq, (void *)action->handler, (void *)action->handler,
			msec_high(duration),
			trace_stat->start_timestamp,
			trace_stat->end_timestamp,
			raw_smp_processor_id());

		irq_mon_msg(out, msg);

		scnprintf(handler_name, sizeof(handler_name), "%pS", (void *)action->handler);
		if (!strncmp(handler_name, "mtk_syst_handler", strlen("mtk_syst_handler")))
			/* skip mtk_syst_handler, let hrtimer handle it. */
			irq_aee_state[irq] = 1;

		if (!irq_name)
			irq_name = "NULL";

		if (!strcmp(irq_name, "IPI") && irq_to_ipi_type(irq) == 4) // IPI_TIMER
			/* skip ipi timer handler, let hrtimer handle it. */
			irq_aee_state[irq] = 1;

		if (!strcmp(irq_name, "arch_timer"))
			/* skip arch_timer aee, let hrtimer handle it. */
			irq_aee_state[irq] = 1;

		if (!strcmp(irq_name, "emimpu_violation_irq"))
			/* skip debug irq. */
			irq_aee_state[irq] = 1;

		if (!strcmp(irq_name, "ufshcd") && raw_smp_processor_id())
			/* skip ufshcd aee if CPU!=0 */
			out &= ~TO_AEE;

		if ((out & TO_AEE) && tracer->aee_limit && !irq_aee_state[irq]) {
			if (!irq_mon_aee_debounce_check(true))
				/* debounce period, skip */
				irq_mon_msg(TO_FTRACE, "irq handler aee skip in debounce period");
			else {
				char module[100];

				irq_aee_state[irq] = 1;
				scnprintf(module, sizeof(module), "IRQ LONG:%d, %pS, %llu ms"
					, irq, (void *)action->handler, msec_high(duration));
				aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT|DB_OPT_FTRACE, module, msg);
			}
		}
	}

	this_cpu_write(irq_trace_stat->tracing, 0);
}

static void probe_softirq_entry(void *ignore, unsigned int vec_nr)
{
	unsigned long long ts;
	struct irq_mon_tracer *tracer = &softirq_tracer;

	if (!tracer->tracing)
		return;
	if (__this_cpu_cmpxchg(softirq_trace_stat->tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(softirq_trace_stat->start_timestamp, ts);
	this_cpu_write(softirq_trace_stat->end_timestamp, 0);
}

static void probe_softirq_exit(void *ignore, unsigned int vec_nr)
{
	struct trace_stat *trace_stat = raw_cpu_ptr(softirq_trace_stat);
	struct irq_mon_tracer *tracer = &softirq_tracer;
	unsigned long long ts, duration;
	unsigned int out;

	if (!tracer->tracing)
		return;
	if (!__this_cpu_read(softirq_trace_stat->tracing))
		return;

	ts = sched_clock();
	trace_stat->end_timestamp = ts;

	duration = stat_dur(trace_stat);
	out = check_threshold(duration, tracer);
	if (out) {
		irq_mon_msg(out,
			"softirq: %u %s, duration %llu ms, from %llu ns to %llu ns on CPU:%d",
			vec_nr, softirq_to_name[vec_nr],
			msec_high(duration),
			trace_stat->start_timestamp,
			trace_stat->end_timestamp,
			raw_smp_processor_id());
	}

	this_cpu_write(softirq_trace_stat->tracing, 0);
}

static void probe_ipi_entry(void *ignore, const char *reason)
{
	unsigned long long ts;
	struct irq_mon_tracer *tracer = &ipi_tracer;

	if (!tracer->tracing)
		return;
	if (__this_cpu_cmpxchg(ipi_trace_stat->tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(ipi_trace_stat->start_timestamp, ts);
	this_cpu_write(ipi_trace_stat->end_timestamp, 0);
}


static void probe_ipi_exit(void *ignore, const char *reason)
{
	struct trace_stat *trace_stat = raw_cpu_ptr(ipi_trace_stat);
	struct irq_mon_tracer *tracer = &ipi_tracer;
	unsigned long long ts, duration;
	unsigned int out;

	if (!tracer->tracing)
		return;
	if (!__this_cpu_read(ipi_trace_stat->tracing))
		return;

	ts = sched_clock();
	trace_stat->end_timestamp = ts;

	duration = stat_dur(trace_stat);
	out = check_threshold(duration, tracer);
	if (out) {
		irq_mon_msg(out, "ipi: %s, duration %llu ms, from %llu ns to %llu ns on CPU:%d",
			reason,
			msec_high(duration),
			trace_stat->start_timestamp,
			trace_stat->end_timestamp,
			raw_smp_processor_id());
	}

	this_cpu_write(ipi_trace_stat->tracing, 0);
}

static void probe_irq_disable(void *ignore,
		unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(irq_pi_stat);
	struct irq_mon_tracer *tracer = &irq_off_tracer;
	unsigned long long ts;

	if (!tracer->tracing)
		return;

	if (__this_cpu_cmpxchg(irq_pi_stat->tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(irq_pi_stat->disable_timestamp, ts);
	this_cpu_write(irq_pi_stat->disable_ip, ip);
	this_cpu_write(irq_pi_stat->disable_parent_ip, parent_ip);

	/* high overhead */
	irq_mon_save_stack_trace(pi_stat);
}

static void probe_irq_enable(void *ignore,
		unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(irq_pi_stat);
	unsigned long long ts;

	if (!__this_cpu_read(irq_pi_stat->tracing))
		return;

	if (__this_cpu_cmpxchg(irq_pi_stat->enable_locked, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(irq_pi_stat->enable_timestamp, ts);
	this_cpu_write(irq_pi_stat->enable_ip, ip);
	this_cpu_write(irq_pi_stat->enable_parent_ip, parent_ip);

	check_preemptirq_stat(pi_stat, 1);

	this_cpu_write(irq_pi_stat->enable_locked, 0);
	this_cpu_write(irq_pi_stat->tracing, 0);
}

static void probe_preempt_disable(void *ignore
		, unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(preempt_pi_stat);
	struct irq_mon_tracer *tracer = &preempt_off_tracer;
	unsigned long long ts;

	if (!tracer->tracing)
		return;

	if (__this_cpu_cmpxchg(preempt_pi_stat->tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(preempt_pi_stat->disable_timestamp, ts);
	this_cpu_write(preempt_pi_stat->disable_ip, ip);
	this_cpu_write(preempt_pi_stat->disable_parent_ip, parent_ip);

	/* high overhead */
	irq_mon_save_stack_trace(pi_stat);
}

static void probe_preempt_enable(void *ignore,
		unsigned long ip, unsigned long parent_ip)
{
	struct preemptirq_stat *pi_stat = raw_cpu_ptr(preempt_pi_stat);
	unsigned long long ts;

	if (!__this_cpu_read(preempt_pi_stat->tracing))
		return;

	if (__this_cpu_cmpxchg(preempt_pi_stat->enable_locked, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(preempt_pi_stat->enable_timestamp, ts);
	this_cpu_write(preempt_pi_stat->enable_ip, ip);
	this_cpu_write(preempt_pi_stat->enable_parent_ip, parent_ip);

	check_preemptirq_stat(pi_stat, 0);

	this_cpu_write(preempt_pi_stat->enable_locked, 0);
	this_cpu_write(preempt_pi_stat->tracing, 0);
}


static void probe_hrtimer_expire_entry(void *ignore,
		struct hrtimer *hrtimer, ktime_t *now)
{
	unsigned long long ts;
	struct irq_mon_tracer *tracer = &hrtimer_expire_tracer;

	if (!tracer->tracing)
		return;
	if (__this_cpu_cmpxchg(hrtimer_trace_stat->tracing, 0, 1))
		return;

	ts = sched_clock();
	this_cpu_write(hrtimer_trace_stat->start_timestamp, ts);
	this_cpu_write(hrtimer_trace_stat->end_timestamp, 0);


}

/* ignore irq_count_tracer_fn hrtimer long */
extern enum hrtimer_restart irq_count_tracer_hrtimer_fn(struct hrtimer *hrtimer);
static void probe_hrtimer_expire_exit(void *ignore, struct hrtimer *hrtimer)
{
	struct trace_stat *trace_stat = raw_cpu_ptr(hrtimer_trace_stat);
	struct irq_mon_tracer *tracer = &hrtimer_expire_tracer;
	unsigned long long ts, duration;
	unsigned int out;
	static bool ever_dump;

	if (!tracer->tracing)
		return;
	if (!__this_cpu_read(hrtimer_trace_stat->tracing))
		return;

	ts = sched_clock();
	trace_stat->end_timestamp = ts;

	duration = stat_dur(trace_stat);
	out = check_threshold(duration, tracer);
	if (out) {
		char msg[MAX_MSG_LEN];

		scnprintf(msg, sizeof(msg),
			"hrtimer: [<%px>]%pS, duration %llu ms, from %llu ns to %llu ns on CPU:%d",
			(void *)hrtimer->function, (void *)hrtimer->function,
			msec_high(duration),
			trace_stat->start_timestamp,
			trace_stat->end_timestamp,
			raw_smp_processor_id());

		irq_mon_msg(out, msg);

		/* ignore irq_count_tracer_fn hrtimer long */
		if ((void *)hrtimer->function == (void *)irq_count_tracer_hrtimer_fn)
			out &= ~TO_AEE;

		if ((out & TO_AEE) && tracer->aee_limit && !ever_dump) {
			if (!irq_mon_aee_debounce_check(true))
				/* debounce period, skip */
				irq_mon_msg(TO_FTRACE, "hrtimer duration skip in debounce period");
			else {
				char module[100];

				ever_dump = 1;
				scnprintf(module, sizeof(module), "HRTIMER LONG: %pS, %llu ms"
					, (void *)hrtimer->function, msec_high(duration));
				aee_kernel_warning_api(__FILE__, __LINE__,
					DB_OPT_DEFAULT|DB_OPT_FTRACE, module, msg);
			}
		}
	}

	this_cpu_write(hrtimer_trace_stat->tracing, 0);
}

/* tracepoints */
struct irq_mon_tracepoint {
	struct tracepoint *tp;
	const char *name;
	void *func;
	void *data;
	bool probe;
	struct irq_mon_tracer *tracer;
};

struct irq_mon_tracepoint irq_mon_tracepoint_table[] = {
	/* irq_handler_tracer irq.h */
	{
		.name = "irq_handler_entry",
		.func = probe_irq_handler_entry,
		.data = NULL,
		.tracer = &irq_handler_tracer,
	},
	{
		.name = "irq_handler_exit",
		.func = probe_irq_handler_exit,
		.data = NULL,
		.tracer = &irq_handler_tracer,
	},
	/* softirq_tracer irq.h */
	{
		.name = "softirq_entry",
		.func = probe_softirq_entry,
		.data = NULL,
		.tracer = &softirq_tracer
	},
	{
		.name = "softirq_exit",
		.func = probe_softirq_exit,
		.data = NULL,
		.tracer = &softirq_tracer,
	},
	/* ipi_tracer ipi.h */
	{
		.name = "ipi_entry",
		.func = probe_ipi_entry,
		.data = NULL,
		.tracer = &ipi_tracer,
	},
	{
		.name = "ipi_exit",
		.func = probe_ipi_exit,
		.data = NULL,
		.tracer = &ipi_tracer,
	},
	/* irq_off_tracer */
	{
		.name = "irq_disable",
		.func = probe_irq_disable,
		.data = NULL,
		.tracer = &irq_off_tracer,
	},
	{
		.name = "irq_enable",
		.func = probe_irq_enable,
		.data = NULL,
		.tracer = &irq_off_tracer,
	},
	/* preempt_off_tracer */
	{
		.name = "preempt_disable",
		.func = probe_preempt_disable,
		.data = NULL,
		.tracer = &preempt_off_tracer,
	},
	{
		.name = "preempt_enable",
		.func = probe_preempt_enable,
		.data = NULL,
		.tracer = &preempt_off_tracer,
	},
	/* hrtimer_expire_tracer timer.h */
	{
		.name = "hrtimer_expire_entry",
		.func = probe_hrtimer_expire_entry,
		.data = NULL,
		.tracer = &hrtimer_expire_tracer,
	},
	{
		.name = "hrtimer_expire_exit",
		.func = probe_hrtimer_expire_exit,
		.data = NULL,
		.tracer = &hrtimer_expire_tracer,
	},
	/* Last item must be NULL!! */
	{.name = NULL, .func = NULL, .data = NULL},
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

bool b_default_enabled; // default false
bool b_count_tracer_default_enabled; // default false
#if !IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEFAULT_ENABLED)
static void irq_mon_boot(void)
{
	struct device_node *node;
	bool b_override_thresholds;
	unsigned int override_th1_ms = 500;
	unsigned int override_th2_ms = 500;
	unsigned int override_th3_ms = 500;

	node = of_find_node_by_name(NULL, "mtk_irq_monitor");
	if (node) {
		b_default_enabled =
			of_property_read_bool(node, "mediatek,default-enabled");
		b_count_tracer_default_enabled =
			of_property_read_bool(node, "mediatek,count-tracer-default-enabled");
		b_override_thresholds =
			of_property_read_bool(node, "mediatek,override-thresholds");
		pr_info("%s: default-enabled=%s, count-tracer=%s, override-thresholds=%s",
			__func__,
			b_default_enabled?"yes":"no",
			b_count_tracer_default_enabled?"yes":"no",
			b_override_thresholds?"yes":"no");

		if (b_override_thresholds) {
			if (of_property_read_u32_index(node, "mediatek,override-thresholds",
							 0, &override_th1_ms))
				override_th1_ms = OVERRIDE_TH1_MS;
			if (of_property_read_u32_index(node, "mediatek,override-thresholds",
							 1, &override_th2_ms))
				override_th2_ms = OVERRIDE_TH1_MS;
			if (of_property_read_u32_index(node, "mediatek,override-thresholds",
							 2, &override_th3_ms))
				override_th3_ms = OVERRIDE_TH1_MS;
			pr_info("%s: override-thresholds: th1=%d, th2=%d, th3=%d",
				__func__, override_th1_ms, override_th2_ms, override_th3_ms);

			/* override irq_handler, ipi and hrtimer thresholds */
			irq_handler_tracer.th1_ms = override_th1_ms;
			irq_handler_tracer.th2_ms = override_th2_ms;
			irq_handler_tracer.th3_ms = override_th3_ms;
			irq_handler_tracer.aee_limit = 1;
			ipi_tracer.th1_ms = override_th1_ms;
			ipi_tracer.th2_ms = override_th2_ms;
			ipi_tracer.th3_ms = override_th3_ms;
			ipi_tracer.aee_limit = 0;
			hrtimer_expire_tracer.th1_ms = override_th1_ms;
			hrtimer_expire_tracer.th2_ms = override_th2_ms;
			hrtimer_expire_tracer.th3_ms = override_th3_ms;
			hrtimer_expire_tracer.aee_limit = 1;
		}
	}
}
#endif

/* probe tracepoints for all tracers */
static int irq_mon_tracepoint_init(void)
{
	for_each_kernel_tracepoint(irq_mon_tracepoint_lookup
					, (void *)irq_mon_tracepoint_table);

#if IS_ENABLED(CONFIG_MTK_IRQ_MONITOR_DEFAULT_ENABLED)
	b_default_enabled = true;
	b_count_tracer_default_enabled = true;
#else
	irq_mon_boot();
#endif
	if (b_default_enabled) {
		struct irq_mon_tracepoint *t;

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
		irq_handler_tracer.tracing = true;
		softirq_tracer.tracing = true;
		ipi_tracer.tracing = true;
		irq_off_tracer.tracing = true;
		preempt_off_tracer.tracing = true;
		hrtimer_expire_tracer.tracing = true;
	}
	return 0;
}

static int irq_mon_tracer_probe(struct irq_mon_tracer *tracer)
{
	struct irq_mon_tracepoint *t;

	for (t = irq_mon_tracepoint_table; t->name != NULL; t++) {
		int ret;

		if (t->tracer != tracer)
			continue;

		if (!t->tp) {
			pr_info("tp: %s not found\n", t->name);
			return -1;
		}
		ret = tracepoint_probe_register(t->tp, t->func, t->data);
		if (ret) {
			pr_info("tp: %s probe failed\n", t->name);
			return ret;
		}
		pr_info("tp: %s,%pS probed\n", t->name, t->tp);
		t->probe = true;
	}
	return 0;
}

static int irq_mon_tracer_unprobe(struct irq_mon_tracer *tracer)
{
	struct irq_mon_tracepoint *t;
	struct trace_stat *t_stat = NULL;
	struct preemptirq_stat *p_stat = NULL;
	int cpu;

	for (t = irq_mon_tracepoint_table; t->name != NULL; t++) {
		if (t->tracer != tracer)
			continue;
		if (!t->tp)
			continue;
		if (t->probe) {
			tracepoint_probe_unregister(t->tp, t->func, t->data);
			t->probe = false;
		}
	}
	tracepoint_synchronize_unregister();

	/* clear trace_stat or preemptirq_stat. no data race here
	 * because all probes are unregistered */
	if (tracer == &irq_handler_tracer)
		t_stat = irq_trace_stat;
	else if (tracer == &softirq_tracer)
		t_stat = softirq_trace_stat;
	else if (tracer == &ipi_tracer)
		t_stat = ipi_trace_stat;
	else if (tracer == &irq_off_tracer)
		p_stat = irq_pi_stat;
	else if (tracer == &preempt_off_tracer)
		p_stat = preempt_pi_stat;
	else if (tracer == &hrtimer_expire_tracer)
		t_stat = hrtimer_trace_stat;

	if (t_stat) {
		for_each_possible_cpu(cpu) {
			per_cpu(t_stat->tracing, cpu) = 0;
		}
	} else if (p_stat) {
		for_each_possible_cpu(cpu) {
			per_cpu(p_stat->tracing, cpu) = 0;
		}
	}

	return 0;
}

static void irq_mon_tracer_tracing_set(struct irq_mon_tracer *tracer, bool val)
{
	if (tracer->tracing == val) {
		return;
	}
	if (val) {
		if (!irq_mon_tracer_probe(tracer))
			tracer->tracing = val;
	} else {
		tracer->tracing = val;
		irq_mon_tracer_unprobe(tracer);
	}
}

/* proc functions */

static DEFINE_MUTEX(proc_lock);
bool irq_mon_door;

static ssize_t irq_mon_door_write(struct file *filp, const char *ubuf,
				  size_t count, loff_t *data)
{
	char buf[16];

	if (!count)
		return count;
	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(&buf, ubuf, count))
		return -EFAULT;

	buf[count - 1UL] = 0;

	if (!strcmp("open", buf))
		irq_mon_door = 1;
	if (!strcmp("close", buf))
		irq_mon_door = 0;

	return count;
}

const struct proc_ops irq_mon_door_pops = {
	.proc_open = simple_open,
	.proc_write = irq_mon_door_write,
};

static int irq_mon_bool_show(struct seq_file *s, void *p)
{
	bool val;

	mutex_lock(&proc_lock);
	val = *(bool *)s->private;
	mutex_unlock(&proc_lock);
	seq_printf(s, "%s\n", (val) ? "1" : "0");
	return 0;
}

int irq_mon_bool_open(struct inode *inode, struct file *file)
{
	return single_open(file, irq_mon_bool_show, PDE_DATA(inode));
}

static ssize_t irq_mon_bool_write(struct file *filp,
		const char *ubuf, size_t count, loff_t *data)
{
	int ret;
	bool val;
	bool *ptr = (bool *)PDE_DATA(file_inode(filp));

	ret = kstrtobool_from_user(ubuf, count, &val);
	if (ret)
		return ret;

	mutex_lock(&proc_lock);
	*ptr = val;
	mutex_unlock(&proc_lock);

	return count;
}

const struct proc_ops irq_mon_bool_pops = {
	.proc_open = irq_mon_bool_open,
	.proc_write = irq_mon_bool_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static ssize_t irq_mon_tracing_set(struct file *filp,
		const char *ubuf, size_t count, loff_t *data)
{
	int ret;
	bool val;
	bool *ptr = (bool *)PDE_DATA(file_inode(filp));
	struct irq_mon_tracer *tracer =
		container_of(ptr, struct irq_mon_tracer, tracing);

	ret = kstrtobool_from_user(ubuf, count, &val);
	if (ret)
		return ret;

	mutex_lock(&proc_lock);
	irq_mon_tracer_tracing_set(tracer, val);
	mutex_unlock(&proc_lock);

	return count;
}

static const struct proc_ops irq_mon_tracing_pops = {
	.proc_open = irq_mon_bool_open,
	.proc_write = irq_mon_tracing_set,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

ssize_t irq_mon_count_set(struct file *filp,
		const char *ubuf, size_t count, loff_t *data)
{
	int ret;
	bool val;

	ret = kstrtobool_from_user(ubuf, count, &val);
	if (ret)
		return ret;

	mutex_lock(&proc_lock);
	irq_count_tracer_set(val);
	mutex_unlock(&proc_lock);

	return count;
}

static int irq_mon_uint_show(struct seq_file *s, void *p)
{
	unsigned int val;

	mutex_lock(&proc_lock);
	val = *(unsigned int *)s->private;
	mutex_unlock(&proc_lock);
	seq_printf(s, "%u\n", val);
	return 0;
}

static int irq_mon_uint_open(struct inode *inode, struct file *file)
{
	return single_open(file, irq_mon_uint_show, PDE_DATA(inode));
}

static ssize_t irq_mon_uint_write(struct file *filp,
		const char *ubuf, size_t count, loff_t *data)
{
	int ret;
	unsigned int val;
	unsigned int *ptr = (unsigned int *)PDE_DATA(file_inode(filp));

	if (!irq_mon_door)
		return -EPERM;

	ret = kstrtouint_from_user(ubuf, count, 10, &val);
	if (ret)
		return ret;

	mutex_lock(&proc_lock);
	*ptr = val;
	mutex_unlock(&proc_lock);

	return count;
}

const struct proc_ops irq_mon_uint_pops = {
	.proc_open = irq_mon_uint_open,
	.proc_write = irq_mon_uint_write,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

#define IRQ_MON_TRACER_PROC_ENTRY(name, mode, type, dir, ptr) \
	proc_create_data(#name, mode, dir, &irq_mon_##type##_pops, (void *)ptr)

static void irq_mon_tracer_proc_init(struct irq_mon_tracer *tracer,
		struct proc_dir_entry *parent)
{
	struct proc_dir_entry *dir;

	dir = proc_mkdir(tracer->name, parent);
	if (!dir)
		return;

	IRQ_MON_TRACER_PROC_ENTRY(tracing, 0644,
			tracing, dir, &tracer->tracing);
	IRQ_MON_TRACER_PROC_ENTRY(th1_ms, 0644, uint, dir, &tracer->th1_ms);
	IRQ_MON_TRACER_PROC_ENTRY(th2_ms, 0644, uint, dir, &tracer->th2_ms);
	IRQ_MON_TRACER_PROC_ENTRY(th3_ms, 0644, uint, dir, &tracer->th3_ms);
	IRQ_MON_TRACER_PROC_ENTRY(aee_limit, 0644,
			uint, dir, &tracer->aee_limit);
	IRQ_MON_TRACER_PROC_ENTRY(aee_debounce_ms, 0644,
			uint, dir, &tracer->aee_debounce_ms);
	return;
}

static void irq_mon_proc_init(void)
{
	struct proc_dir_entry *dir;

	// root
	dir = proc_mkdir("mtmon", NULL);
	if (!dir)
		return;
	proc_create("irq_mon_door", 0220, dir, &irq_mon_door_pops);
	// irq_mon_tracers
	irq_mon_tracer_proc_init(&irq_handler_tracer, dir);
	irq_mon_tracer_proc_init(&softirq_tracer, dir);
	irq_mon_tracer_proc_init(&ipi_tracer, dir);
	irq_mon_tracer_proc_init(&irq_off_tracer, dir);
	irq_mon_tracer_proc_init(&preempt_off_tracer, dir);
	irq_mon_tracer_proc_init(&hrtimer_expire_tracer, dir);
	irq_count_tracer_proc_init(dir);
	mt_irq_monitor_test_init(dir);
}

static int __init irq_monitor_init(void)
{
	int ret = 0;

	irq_pi_stat = alloc_percpu(struct preemptirq_stat);
	if (!irq_pi_stat) {
		pr_info("Failed to alloc irq_pi_stat\n");
		return -ENOMEM;
	}
	preempt_pi_stat = alloc_percpu(struct preemptirq_stat);
	if (!preempt_pi_stat) {
		pr_info("Failed to alloc preempt_pi_stat\n");
		return -ENOMEM;
	}
	irq_trace_stat = alloc_percpu(struct trace_stat);
	if (!irq_trace_stat) {
		pr_info("Failed to alloc irq_trace_stat\n");
		return -ENOMEM;
	}
	softirq_trace_stat = alloc_percpu(struct trace_stat);
	if (!softirq_trace_stat) {
		pr_info("Failed to alloc softirq_trace_stat\n");
		return -ENOMEM;
	}
	ipi_trace_stat = alloc_percpu(struct trace_stat);
	if (!ipi_trace_stat) {
		pr_info("Failed to alloc ipi_trace_stat\n");
		return -ENOMEM;
	}
	hrtimer_trace_stat = alloc_percpu(struct trace_stat);
	if (!hrtimer_trace_stat) {
		pr_info("Failed to alloc hrtimer_trace_stat\n");
		return -ENOMEM;
	}

	// tracepoint init
	pr_info("irq monitor init start!!\n");
	ret = irq_mon_tracepoint_init();
	if (ret)
		return ret;
	ret = irq_count_tracer_init();
	if (ret)
		return ret;
	irq_mon_proc_init();
#if IS_ENABLED(CONFIG_MTK_AEE_HANGDET)
	kwdt_regist_irq_info(mt_aee_dump_irq_info);
#endif
#if IS_ENABLED(CONFIG_MTK_FTRACE_DEFAULT_ENABLE)
	trace_set_clr_event(NULL, "irq_mon_msg", 1);
#endif
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
#if IS_ENABLED(CONFIG_MTK_AEE_HANGDET)
	kwdt_regist_irq_info(NULL);
#endif
	remove_proc_subtree("mtmon", NULL);
	irq_count_tracer_exit();
	irq_mon_tracepoint_exit();

	free_percpu(irq_pi_stat);
	free_percpu(preempt_pi_stat);
	free_percpu(irq_trace_stat);
	free_percpu(softirq_trace_stat);
	free_percpu(ipi_trace_stat);
	free_percpu(hrtimer_trace_stat);
}

MODULE_DESCRIPTION("MEDIATEK IRQ MONITOR");
MODULE_LICENSE("GPL v2");
early_initcall(irq_monitor_init);
module_exit(irq_monitor_exit);
