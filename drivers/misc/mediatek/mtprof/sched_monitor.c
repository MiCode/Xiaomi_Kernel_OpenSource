// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#define DEBUG 1

#include <linux/module.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include "mtk_sched_mon.h"
#include "internal.h"

#define CREATE_TRACE_POINTS
#include "mtk_sched_mon_trace.h"

#define MAX_STACK_TRACE_DEPTH   32

static bool sched_mon_door;

bool irq_time_tracer;
unsigned int irq_time_th1_ms = 100; /* log */
unsigned int irq_time_th2_ms = 500; /* aee */
unsigned int irq_time_aee_limit;

#ifdef CONFIG_MTK_IRQ_COUNT_TRACER
static bool irq_count_tracer;
static unsigned int irq_period_th1_ns = 200000; /* log */
static unsigned int irq_period_th2_ns = 200000; /* aee */
static unsigned int irq_count_aee_limit;
/* period setting for specific irqs */
struct irq_count_period_setting {
	const char *name;
	unsigned int period;
} irq_count_plist[] = {
	{"ufshcd", 10000} /* 100000 irqs per sec*/
};
#endif
#ifdef CONFIG_MTK_IRQ_OFF_TRACER
static bool irq_off_tracer;
static bool irq_off_tracer_trace;
static unsigned int irq_off_th1_ms = 50; /* trace */
static unsigned int irq_off_th2_ms = 500; /* print */
static unsigned int irq_off_th3_ms = 500; /* aee */
static unsigned int irq_off_aee_limit;
static unsigned int irq_off_aee_debounce_ms = 60000;
#endif
#ifdef CONFIG_MTK_PREEMPT_TRACER
static bool preempt_tracer;
static bool preempt_tracer_trace;
static unsigned int preempt_th1_ms = 60000; /* trace */
static unsigned int preempt_th2_ms = 180000; /* print */
static unsigned int preempt_th3_ms; /* aee */
static unsigned int preempt_aee_limit;
#endif

DEFINE_PER_CPU(struct irq_handle_status, irq_note);
DEFINE_PER_CPU(struct irq_handle_status, ipi_note);
DEFINE_PER_CPU(struct irq_handle_status, softirq_note);

const char *irq_to_name(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);

	if (desc && desc->action && desc->action->name)
		return desc->action->name;
	return NULL;
}

void sched_mon_msg(int out, char *buf, ...)
{
	char str[128];
	va_list args;

	va_start(args, buf);
	vsnprintf(str, sizeof(str), buf, args);
	va_end(args);

	if (out & TO_FTRACE)
		trace_sched_mon_msg_rcuidle(str);
	if (out & TO_KERNEL_LOG)
		pr_info("%s\n", str);
	if (out & TO_DEFERRED)
		printk_deferred("%s\n", str);
	if (out & TO_SRAM) {
		pr_aee_sram(str);
		pr_aee_sram("\n");
	}
}

static ssize_t
sched_mon_door_write(struct file *filp, const char *ubuf,
		     size_t cnt, loff_t *data)
{
	char buf[16];

	if (cnt >= sizeof(buf) || cnt <= 1UL)
		return cnt;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt - 1UL] = 0;

	if (!strcmp("open", buf))
		sched_mon_door = 1;
	if (!strcmp("close", buf))
		sched_mon_door = 0;

	return cnt;
}

static const struct file_operations sched_mon_door_fops = {
	.open = simple_open,
	.write = sched_mon_door_write,
};

DEFINE_SCHED_MON_OPS(irq_time_tracer, bool, 0, 1);
DEFINE_SCHED_MON_OPS(irq_time_th1_ms, unsigned int, 1, 1000000);
DEFINE_SCHED_MON_OPS(irq_time_th2_ms, unsigned int, 0, 1000000);
DEFINE_SCHED_MON_OPS(irq_time_aee_limit, unsigned int, 0, 100);

#ifdef CONFIG_MTK_IRQ_COUNT_TRACER
DEFINE_SCHED_MON_OPS(irq_count_tracer, bool, 0, 1);
DEFINE_SCHED_MON_OPS(irq_period_th1_ns, unsigned int, 1, 1000000000);
DEFINE_SCHED_MON_OPS(irq_period_th2_ns, unsigned int, 0, 1000000000);
DEFINE_SCHED_MON_OPS(irq_count_aee_limit, unsigned int, 0, 100);
#endif
#ifdef CONFIG_MTK_IRQ_OFF_TRACER
DEFINE_SCHED_MON_OPS(irq_off_tracer, bool, 0, 1);
DEFINE_SCHED_MON_OPS(irq_off_tracer_trace, bool, 0, 1);
DEFINE_SCHED_MON_OPS(irq_off_th1_ms, unsigned int, 1, 5000);
DEFINE_SCHED_MON_OPS(irq_off_th2_ms, unsigned int, 1, 5000);
DEFINE_SCHED_MON_OPS(irq_off_th3_ms, unsigned int, 0, 5000);
DEFINE_SCHED_MON_OPS(irq_off_aee_limit, unsigned int, 0, 100);
DEFINE_SCHED_MON_OPS(irq_off_aee_debounce_ms, unsigned int, 0, 6000000);
#endif
#ifdef CONFIG_MTK_PREEMPT_TRACER
DEFINE_SCHED_MON_OPS(preempt_tracer, bool, 0, 1);
DEFINE_SCHED_MON_OPS(preempt_tracer_trace, bool, 0, 1);
DEFINE_SCHED_MON_OPS(preempt_th1_ms, unsigned int, 1, 1000000);
DEFINE_SCHED_MON_OPS(preempt_th2_ms, unsigned int, 1, 1000000);
DEFINE_SCHED_MON_OPS(preempt_th3_ms, unsigned int, 0, 1000000);
DEFINE_SCHED_MON_OPS(preempt_aee_limit, unsigned int, 0, 100);
#endif

#define show_irq_handle(cpu, irq_type, data, output) \
	sched_mon_msg(output, \
		"%s%s:%d, start:%lld.%06lu, end:%lld.%06lu, dur:%lld us", \
		per_cpu(data, cpu).end == 0 ? "In " : "", \
		irq_type, per_cpu(data, cpu).irq, \
		sec_high(per_cpu(data, cpu).start), \
		sec_low(per_cpu(data, cpu).start), \
		sec_high(per_cpu(data, cpu).end), \
		sec_low(per_cpu(data, cpu).end), \
		usec_high(per_cpu(data, cpu).end == 0 ? 0 : \
		per_cpu(data, cpu).end - per_cpu(data, cpu).start))

static void __show_irq_handle_info(int output)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		sched_mon_msg(output, "CPU: %d", cpu);
		show_irq_handle(cpu, "IRQ", irq_note, output);
		show_irq_handle(cpu, "IPI", ipi_note, output);
		show_irq_handle(cpu, "SoftIRQ", softirq_note, output);
		sched_mon_msg(output, "");
	}
}

void show_irq_handle_info(int output)
{
	if (irq_time_tracer)
		__show_irq_handle_info(output);
}

#ifdef CONFIG_MTK_IRQ_COUNT_TRACER
/*
 * If a irq is frequently triggered, it could result in problems.
 * The purpose of this feature is to catch the condition. When the
 * average time interval of a irq is below the threshold, we judge
 * the irq is triggered abnormally and print a message for reference.
 *
 * average time interval =
 *     statistics time / irq count increase during the statistics time
 */
static struct hrtimer irq_count_timer;

#ifdef CONFIG_SPARSE_IRQ
#define IRQ_BITMAP_BITS	(NR_IRQS + 8196)
#else
#define IRQ_BITMAP_BITS	NR_IRQS
#endif

struct irq_count_stat {
	struct irq_work work;
	unsigned long long t_start;
	unsigned long long t_end;
	unsigned int count[IRQ_BITMAP_BITS];
};

DEFINE_PER_CPU(struct irq_count_stat, irq_count_data);

static void __irq_count_tracer_work(struct irq_work *work)
{
	struct irq_count_stat *irq_cnt = this_cpu_ptr(&irq_count_data);
	int cpu = smp_processor_id();
	int irq, irq_num, i, skip;
	unsigned int count;
	unsigned long long t_avg, t_diff;
	char msg[128];
	int list_num = ARRAY_SIZE(irq_count_plist);

	irq_cnt->t_start = irq_cnt->t_end;
	irq_cnt->t_end = sched_clock();

	for (irq = 0; irq < nr_irqs; irq++) {
		irq_num = kstat_irqs_cpu(irq, cpu);
		count = irq_num - irq_cnt->count[irq];

		/* The irq is not triggered in this period */
		if (count == 0)
			continue;

		irq_cnt->count[irq] = irq_num;
		t_diff = irq_cnt->t_end - irq_cnt->t_start;
		t_avg = t_diff;
		do_div(t_avg, count);
		do_div(t_diff, 1000000);

		if (t_avg > irq_period_th1_ns)
			continue;

		if (!irq_to_name(irq))
			continue;

		for (i = 0, skip = 0; i < list_num && !skip; i++) {
			if (!strcmp(irq_to_name(irq), irq_count_plist[i].name))
				if (t_avg > irq_count_plist[i].period)
					skip = 1;
		}
		if (skip)
			continue;

		snprintf(msg, sizeof(msg),
			 "irq:%d %s count +%d in %lld ms, from %lld.%06lu to %lld.%06lu on CPU:%d",
			 irq, irq_to_name(irq), count, t_diff,
			 sec_high(irq_cnt->t_start), sec_low(irq_cnt->t_start),
			 sec_high(irq_cnt->t_end), sec_low(irq_cnt->t_end),
			 raw_smp_processor_id());
		sched_mon_msg(TO_BOTH, msg);

		if (irq_period_th2_ns && irq_count_aee_limit &&
		    t_avg < irq_period_th2_ns) {
			irq_count_aee_limit--;
			schedule_monitor_aee(msg, "BURST IRQ");
		}
	}
}

static void irq_count_tracer_work(struct irq_work *work)
{
	if (irq_count_tracer)
		__irq_count_tracer_work(work);
}

static enum hrtimer_restart irq_count_polling_timer(struct hrtimer *unused)
{
	int cpu;

	for_each_online_cpu(cpu)
		irq_work_queue_on(per_cpu_ptr(&irq_count_data.work, cpu), cpu);

	hrtimer_forward_now(&irq_count_timer, ms_to_ktime(1000));

	return HRTIMER_RESTART;
}

static int __init init_irq_count_tracer(void)
{
	int cpu;

	for_each_online_cpu(cpu)
		init_irq_work(per_cpu_ptr(&irq_count_data.work, cpu),
			      irq_count_tracer_work);
#ifdef CONFIG_MTK_ENG_BUILD
	irq_count_tracer = 1;
	irq_count_aee_limit = 1;
#endif
	hrtimer_init(&irq_count_timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
	irq_count_timer.function = irq_count_polling_timer;
	hrtimer_start(&irq_count_timer, ms_to_ktime(0),
		      HRTIMER_MODE_REL_PINNED);
	return 0;
}

static void __show_irq_count_info(int output)
{
	int cpu;

	sched_mon_msg(output, "===== IRQ Status =====");

	for_each_possible_cpu(cpu) {
		struct irq_count_stat *irq_cnt;
		int irq;

		irq_cnt = per_cpu_ptr(&irq_count_data, cpu);

		sched_mon_msg(output, "CPU: %d", cpu);
		sched_mon_msg(output, "from %lld.%06lu to %lld.%06lu, %lld ms",
			      sec_high(irq_cnt->t_start),
			      sec_low(irq_cnt->t_start),
			      sec_high(irq_cnt->t_end), sec_low(irq_cnt->t_end),
			      msec_high(irq_cnt->t_end - irq_cnt->t_start));

		for (irq = 0; irq < nr_irqs; irq++) {
			unsigned int count;

			count = kstat_irqs_cpu(irq, cpu);
			if (!count)
				continue;

			sched_mon_msg(output, "    %d:%s +%d(%d)",
				      irq, irq_to_name(irq),
				      count - irq_cnt->count[irq], count);
		}
		sched_mon_msg(output, "");
	}
}

void show_irq_count_info(int output)
{
	if (irq_count_tracer)
		__show_irq_count_info(output);
}
#endif /* CONFIG_MTK_IRQ_COUNT_TRACER */

#ifdef CONFIG_MTK_IRQ_OFF_TRACER
static DEFINE_PER_CPU(unsigned long long, irqsoff_timestamp);
static DEFINE_PER_CPU(struct stack_trace, irqsoff_trace);

#ifdef CONFIG_PROVE_LOCKING
static DEFINE_PER_CPU(int, tracing_irq_cpu);
#define irqsoff_tracing_lock()    this_cpu_write(tracing_irq_cpu, 1)
#define irqsoff_tracing_unlock()  this_cpu_write(tracing_irq_cpu, 0)
#define irqsoff_on_tracing()      this_cpu_read(tracing_irq_cpu)
#define irqsoff_not_on_tracing()  !this_cpu_read(tracing_irq_cpu)
#else
#define irqsoff_tracing_lock()    do {} while (0)
#define irqsoff_tracing_unlock()  do {} while (0)
#define irqsoff_on_tracing()      0
#define irqsoff_not_on_tracing()  0
#endif

static const char * const list_irq_disable[] = {
	/* debug purpose: sched_debug_show, sysrq_sched_debug_show */
	"print_cpu"
};

static int trace_hardirqs_whitelist(void)
{
	struct stack_trace *trace = raw_cpu_ptr(&irqsoff_trace);
	char func[64];
	int i, j;

	for (i = 0; i < trace->nr_entries; i++) {
		snprintf(func, sizeof(func),
			 "%ps", (void *)trace->entries[i]);
		for (j = 0; j < ARRAY_SIZE(list_irq_disable); j++)
			if (!strcmp(func, list_irq_disable[j]))
				return 1;
	}
	return 0;
}

static inline void __trace_hardirqs_off_time(void)
{
	if (irqsoff_on_tracing())
		return;
	irqsoff_tracing_lock();

	this_cpu_write(irqsoff_timestamp, sched_clock());

	if (irq_off_tracer_trace) {
		struct stack_trace *trace;

		trace = raw_cpu_ptr(&irqsoff_trace);
		trace->nr_entries = 0;
		save_stack_trace(trace);

		if (trace->nr_entries != 0 &&
		    trace->entries[trace->nr_entries - 1] == ULONG_MAX)
			trace->nr_entries--;
	}
}

inline void trace_hardirqs_off_time(void)
{
	if (irq_off_tracer)
		__trace_hardirqs_off_time();
}

static inline void __trace_hardirqs_on_time(void)
{
	unsigned long long t_off, t_on, t_diff;
	char msg[128];
	int output = TO_FTRACE;

	if (irqsoff_not_on_tracing())
		return;

	/* skip <idle-0> task */
	if (current->pid == 0) {
		irqsoff_tracing_unlock();
		return;
	}

	t_off = this_cpu_read(irqsoff_timestamp);
	t_on = sched_clock();
	t_diff = t_on - t_off;
	do_div(t_diff, 1000000);

	if (t_diff < irq_off_th1_ms) {
		irqsoff_tracing_unlock();
		return;
	}

	if (irq_off_th2_ms && t_diff >= irq_off_th2_ms)
		output = TO_BOTH_SAVE;

	snprintf(msg, sizeof(msg),
		 "irq off monitor: dur[%lld ms] off[%lld.%06lu] on[%lld.%06lu]",
		 t_diff, sec_high(t_off), sec_low(t_off),
		 sec_high(t_on), sec_low(t_on));
	sched_mon_msg(output, msg);

	if (irq_off_tracer_trace) {
		struct stack_trace *trace = raw_cpu_ptr(&irqsoff_trace);
		char msg2[128];
		int i;

		sched_mon_msg(output, "call trace:");
		for (i = 0; i < trace->nr_entries; i++) {
			snprintf(msg2, sizeof(msg2), "[<%p>] %pS",
				 (void *)trace->entries[i],
				 (void *)trace->entries[i]);
			sched_mon_msg(output, msg2);
		}
	}

	if (irq_off_th3_ms && irq_off_aee_limit &&
	    t_diff > irq_off_th3_ms && !trace_hardirqs_whitelist()) {
		static unsigned long long aee_ts;
		unsigned long long now = sched_clock();

		if (now - aee_ts > irq_off_aee_debounce_ms * 1000000ULL) {
			aee_ts = now;
			irq_off_aee_limit--;
			schedule_monitor_aee(msg, "IRQ OFF");
		}
	}

	irqsoff_tracing_unlock();
}

inline void trace_hardirqs_on_time(void)
{
	if (irq_off_tracer)
		__trace_hardirqs_on_time();
}

__init static int init_irq_off_tracer(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct stack_trace *trace;

		trace = per_cpu_ptr(&irqsoff_trace, cpu);
		trace->entries =
			kmalloc_array(MAX_STACK_TRACE_DEPTH,
				      sizeof(unsigned long), GFP_KERNEL);
		trace->max_entries = MAX_STACK_TRACE_DEPTH;
		trace->skip = 2;
	}
#ifdef CONFIG_MTK_ENG_BUILD
	irq_off_tracer = 1;
	irq_off_tracer_trace = 1;
#endif
	return 0;
}
#endif /* CONFIG_MTK_IRQ_OFF_TRACER */

#ifdef CONFIG_MTK_PREEMPT_TRACER
static DEFINE_PER_CPU(unsigned long long, preempt_off_timestamp);
static DEFINE_PER_CPU(struct stack_trace, preempt_off_trace);

static inline void __trace_preempt_off_time(void)
{
	this_cpu_write(preempt_off_timestamp, sched_clock());

	if (preempt_tracer_trace) {
		struct stack_trace *trace;

		trace = raw_cpu_ptr(&preempt_off_trace);
		trace->nr_entries = 0;
		save_stack_trace(trace);

		if (trace->nr_entries != 0 &&
		    trace->entries[trace->nr_entries - 1] == ULONG_MAX)
			trace->nr_entries--;
	}
}

inline void trace_preempt_off_time(void)
{
	if (preempt_tracer)
		__trace_preempt_off_time();
}

static inline void __trace_preempt_on_time(void)
{
	unsigned long long t_off, t_on, t_diff;
	char msg[128];
	int output = TO_FTRACE;

	/* skip <idle-0> task */
	if (current->pid == 0)
		return;

	t_off = this_cpu_read(preempt_off_timestamp);
	t_on = sched_clock();
	t_diff = t_on - t_off;
	do_div(t_diff, 1000000);

	if (t_diff < preempt_th1_ms)
		return;

	if (preempt_th2_ms && t_diff >= preempt_th2_ms)
		output = TO_BOTH_SAVE;

	snprintf(msg, sizeof(msg),
		 "preempt off monitor: dur[%lld ms] off[%lld.%06lu] on[%lld.%06lu]",
		 t_diff, sec_high(t_off), sec_low(t_off),
		 sec_high(t_on), sec_low(t_on));
	sched_mon_msg(output, msg);

	if (preempt_tracer_trace) {
		struct stack_trace *trace = raw_cpu_ptr(&preempt_off_trace);
		char msg2[128];
		int i;

		sched_mon_msg(output, "call trace:");
		for (i = 0; i < trace->nr_entries; i++) {
			snprintf(msg2, sizeof(msg2), "[<%p>] %pS",
				 (void *)trace->entries[i],
				 (void *)trace->entries[i]);
			sched_mon_msg(output, msg2);
		}
	}

	if (preempt_th3_ms && preempt_aee_limit &&
	    t_diff > preempt_th3_ms) {
		preempt_aee_limit--;
		schedule_monitor_aee(msg, "PREEMPT OFF");
	}
}

inline void trace_preempt_on_time(void)
{
	if (preempt_tracer)
		__trace_preempt_on_time();
}

__init static int init_preempt_tracer(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct stack_trace *trace;

		trace = per_cpu_ptr(&preempt_off_trace, cpu);
		trace->entries =
			kmalloc_array(MAX_STACK_TRACE_DEPTH,
				      sizeof(unsigned long), GFP_KERNEL);
		trace->max_entries = MAX_STACK_TRACE_DEPTH;
		trace->skip = 2;
	}
#ifdef CONFIG_MTK_ENG_BUILD
	preempt_tracer = 1;
#endif
	return 0;
}
#endif /* CONFIG_MTK_PREEMPT_TRACER */

void mt_aee_dump_sched_traces(void)
{
	show_irq_handle_info(TO_SRAM);
	show_irq_count_info(TO_SRAM);
}

enum sched_mon_dir_type {
	ROOT,
	IRQ_TIME_TRACER,
	IRQ_COUNT_TRACER,
	IRQ_OFF_TRACER,
	PREEMPT_TRACER,
	NUM_SCHED_MON_DIRS,
};

struct sched_mon_proc_file {
	const char *name;
	enum sched_mon_dir_type dir;
	umode_t mode;
	const struct file_operations *proc_fops;
};

static struct sched_mon_proc_file sched_mon_file[] = {
	/* /proc/mtmon */
	{"sched_mon_door", ROOT, 0220,
		&sched_mon_door_fops},

	{"enable", IRQ_TIME_TRACER, 0644,
		&sched_mon_irq_time_tracer_fops},
	{"th1_ms", IRQ_TIME_TRACER, 0644,
		&sched_mon_irq_time_th1_ms_fops},
	{"th2_ms", IRQ_TIME_TRACER, 0644,
		&sched_mon_irq_time_th2_ms_fops},
	{"aee_limit", IRQ_TIME_TRACER, 0644,
		&sched_mon_irq_time_aee_limit_fops},

#ifdef CONFIG_MTK_IRQ_COUNT_TRACER
	{"enable", IRQ_COUNT_TRACER, 0644,
		&sched_mon_irq_count_tracer_fops},
	{"period_th1_ns", IRQ_COUNT_TRACER, 0644,
		&sched_mon_irq_period_th1_ns_fops},
	{"period_th2_ns", IRQ_COUNT_TRACER, 0644,
		&sched_mon_irq_period_th2_ns_fops},
	{"aee_limit", IRQ_COUNT_TRACER, 0644,
		&sched_mon_irq_count_aee_limit_fops},
#endif
#ifdef CONFIG_MTK_IRQ_OFF_TRACER
	{"enable", IRQ_OFF_TRACER, 0644,
		&sched_mon_irq_off_tracer_fops},
	{"enable_backtrace", IRQ_OFF_TRACER, 0644,
		&sched_mon_irq_off_tracer_trace_fops},
	{"th1_ms", IRQ_OFF_TRACER, 0644,
		&sched_mon_irq_off_th1_ms_fops},
	{"th2_ms", IRQ_OFF_TRACER, 0644,
		&sched_mon_irq_off_th2_ms_fops},
	{"th3_ms", IRQ_OFF_TRACER, 0644,
		&sched_mon_irq_off_th3_ms_fops},
	{"aee_limit", IRQ_OFF_TRACER, 0644,
		&sched_mon_irq_off_aee_limit_fops},
	{"aee_debounce_ms", IRQ_OFF_TRACER, 0644,
		&sched_mon_irq_off_aee_debounce_ms_fops},
#endif
#ifdef CONFIG_MTK_PREEMPT_TRACER
	{"enable", PREEMPT_TRACER, 0644,
		&sched_mon_preempt_tracer_fops},
	{"enable_backtrace", PREEMPT_TRACER, 0644,
		&sched_mon_preempt_tracer_trace_fops},
	{"th1_ms", PREEMPT_TRACER, 0644,
		&sched_mon_preempt_th1_ms_fops},
	{"th2_ms", PREEMPT_TRACER, 0644,
		&sched_mon_preempt_th2_ms_fops},
	{"th3_ms", PREEMPT_TRACER, 0644,
		&sched_mon_preempt_th3_ms_fops},
	{"aee_limit", PREEMPT_TRACER, 0644,
		&sched_mon_preempt_aee_limit_fops},
#endif
};

void sched_mon_device_tree(void)
{
	struct device_node *node;
	u32 value = 0;

	node = of_find_compatible_node(NULL, NULL, "mediatek,sched_mon");
	if (!node)
		return;

	of_property_read_u32(node, "irq_time_tracer", &value);
	irq_time_tracer = (bool)value;
	of_property_read_u32(node, "irq_time_th1_ms", &irq_time_th1_ms);
	of_property_read_u32(node, "irq_time_th2_ms", &irq_time_th2_ms);
	of_property_read_u32(node, "irq_time_aee_limit", &irq_time_aee_limit);

#ifdef CONFIG_MTK_IRQ_COUNT_TRACER
	of_property_read_u32(node, "irq_count_tracer", &value);
	irq_count_tracer = (bool)value;
	of_property_read_u32(node, "irq_period_th1_ns", &irq_period_th1_ns);
	of_property_read_u32(node, "irq_period_th2_ns", &irq_period_th2_ns);
	of_property_read_u32(node, "irq_count_aee_limit", &irq_count_aee_limit);
#endif
#ifdef CONFIG_MTK_IRQ_OFF_TRACER
	of_property_read_u32(node, "irq_off_tracer", &value);
	irq_off_tracer = (bool)value;
	of_property_read_u32(node, "irq_off_tracer_trace", &value);
	irq_off_tracer_trace = (bool)value;
	of_property_read_u32(node, "irq_off_th1_ms", &irq_off_th1_ms);
	of_property_read_u32(node, "irq_off_th2_ms", &irq_off_th2_ms);
	of_property_read_u32(node, "irq_off_th3_ms", &irq_off_th3_ms);
	of_property_read_u32(node, "irq_off_aee_limit", &irq_off_aee_limit);
	of_property_read_u32(node, "irq_off_aee_debounce_ms",
			     &irq_off_aee_debounce_ms);
#endif
#ifdef CONFIG_MTK_PREEMPT_TRACER
	of_property_read_u32(node, "preempt_tracer", &value);
	preempt_tracer = (bool)value;
	of_property_read_u32(node, "preempt_tracer_trace", &value);
	preempt_tracer_trace = (bool)value;
	of_property_read_u32(node, "preempt_th1_ms", &preempt_th1_ms);
	of_property_read_u32(node, "preempt_th2_ms", &preempt_th2_ms);
	of_property_read_u32(node, "preempt_th3_ms", &preempt_th3_ms);
	of_property_read_u32(node, "preempt_aee_limit", &preempt_aee_limit);
#endif
}

static int __init init_sched_monitor(void)
{
	int i;
	struct proc_dir_entry *dir[NUM_SCHED_MON_DIRS];

	dir[ROOT] = proc_mkdir("mtmon", NULL);
	if (!dir[ROOT])
		return -1;

	dir[IRQ_TIME_TRACER] = proc_mkdir("irq_time_tracer", dir[ROOT]);
	if (!dir[IRQ_TIME_TRACER])
		return -1;
#ifdef CONFIG_MTK_ENG_BUILD
	irq_time_tracer = 1;
#endif
#ifdef CONFIG_MTK_IRQ_COUNT_TRACER
	dir[IRQ_COUNT_TRACER] = proc_mkdir("irq_count_tracer", dir[ROOT]);
	if (!dir[IRQ_COUNT_TRACER])
		return -1;
	init_irq_count_tracer();
#endif
#ifdef CONFIG_MTK_IRQ_OFF_TRACER
	dir[IRQ_OFF_TRACER] = proc_mkdir("irq_off_tracer", dir[ROOT]);
	if (!dir[IRQ_OFF_TRACER])
		return -1;
	init_irq_off_tracer();
#endif
#ifdef CONFIG_MTK_PREEMPT_TRACER
	dir[PREEMPT_TRACER] = proc_mkdir("preempt_tracer", dir[ROOT]);
	if (!dir[PREEMPT_TRACER])
		return -1;
	init_preempt_tracer();
#endif
	for (i = 0; i < ARRAY_SIZE(sched_mon_file); i++) {
		if (!proc_create(sched_mon_file[i].name,
				 sched_mon_file[i].mode,
				 dir[sched_mon_file[i].dir],
				 sched_mon_file[i].proc_fops)) {
			pr_info("create [%s] failed\n", sched_mon_file[i].name);
			return -ENOMEM;
		}
	}

	sched_mon_device_tree();
	mt_sched_monitor_test_init(dir[ROOT]);

	return 0;
}
module_init(init_sched_monitor);
