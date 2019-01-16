#include <linux/module.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/ftrace.h>
#include <trace/events/mt65xx_mon_trace.h>
#include <linux/workqueue.h>
#include <linux/version.h>

#include "kernel/trace/trace.h"
#include "mach/mt_mon.h"

static struct trace_array *mt65xx_mon_trace;
static int __read_mostly mt65xx_mon_enabled;
static int mt65xx_mon_ref;

static DEFINE_MUTEX(mt65xx_mon_mutex);
static DEFINE_SPINLOCK(mt65xx_mon_spinlock);

static int mt65xx_mon_stopped = 1;

int timer_initialized;		/* default value: 0 */

static MonitorMode monitor_mode = MODE_SCHED_SWITCH;
static long mon_period_ns = 1000000L;	/* 1ms */
static unsigned int is_manual_start;

static struct hrtimer timer;
static struct mtk_monitor *mtk_mon;


/************************************************************/
/* Add work queue here to prevent the deadlock which        */
/* will occur when we call the mtk_smp_call_function        */
/* in hrtimer ISR                                           */
/************************************************************/
static struct workqueue_struct *queue;
static struct work_struct work;

static void work_handler(struct work_struct *data)
{
	trace_mt65xx_mon_periodic(NULL, NULL);
}

enum hrtimer_restart timer_isr(struct hrtimer *hrtimer)
{
	ktime_t kt;

	if (mt65xx_mon_stopped == 0) {
		/* trace_mt65xx_mon_periodic(NULL, NULL); */
		schedule_work(&work);
		kt = ktime_set(0, mon_period_ns);
		return hrtimer_forward_now(&timer, kt);
	}

	return HRTIMER_NORESTART;
}

void set_mt65xx_mon_period(long time_ns)
{
	mon_period_ns = time_ns;
}

long get_mt65xx_mon_period(void)
{
	return mon_period_ns;
}

void set_mt65xx_mon_manual_start(unsigned int start)
{
	if ((start == 0 || start == 1) && (start != is_manual_start)) {
		if (start == 0)
			pr_info("set_mt65xx_mon_manual_start: START\n");
		else
			pr_info("set_mt65xx_mon_manual_start: STOP\n");

		trace_mt65xx_mon_manual(start);
		is_manual_start = start;
	}
}

unsigned int get_mt65xx_mon_manual_start(void)
{
	return is_manual_start;
}

MonitorMode get_mt65xx_mon_mode(void)
{
	return monitor_mode;
}

void set_mt65xx_mon_mode(MonitorMode mode)
{
	ktime_t kt;

	pr_info("set_mt65xx_mon_mode (mode = %d)\n", (int)mode);

	mutex_lock(&mt65xx_mon_mutex);

	if ((mode != MODE_SCHED_SWITCH) && (mode != MODE_PERIODIC) && (mode != MODE_MANUAL_TRACER))
		return;

	monitor_mode = mode;
	if ((monitor_mode == MODE_PERIODIC)) {
		if (timer_initialized == 0) {
			hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			timer.function = timer_isr;
			kt = ktime_set(0, mon_period_ns);
			hrtimer_start(&timer, kt, HRTIMER_MODE_REL);
			timer_initialized++;
		} else {
			hrtimer_restart(&timer);
		}
	} else if ((monitor_mode == MODE_SCHED_SWITCH) || (monitor_mode == MODE_MANUAL_TRACER)) {
		if (timer_initialized > 0)
			hrtimer_cancel(&timer);
	}

	mutex_unlock(&mt65xx_mon_mutex);

}

void
tracing_mt65xx_mon_function(struct trace_array *tr,
			    struct task_struct *prev,
			    struct task_struct *next, unsigned long flags, int pc)
{
#if 0
	struct ftrace_event_call *call = &event_mt65xx_mon;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	struct ring_buffer *buffer = tr->buffer;
#else
	struct ring_buffer *buffer = tr->trace_buffer.buffer;
#endif
	struct ring_buffer_event *event;
	struct mt65xx_mon_entry *entry;
	unsigned int idx = 0;

	event = trace_buffer_lock_reserve(buffer, TRACE_MT65XX_MON_TYPE, sizeof(*entry), flags, pc);
	if (!event)
		return;

	entry = ring_buffer_event_data(event);



	mtk_mon->disable();
	entry->log = idx = mtk_mon->mon_log((void *)&entry->field);
	entry->cpu = raw_smp_processor_id();
	mtk_mon->enable();



#if 0
	if (!filter_check_discard(call, entry, buffer, event))
#endif
		trace_buffer_unlock_commit(buffer, event, flags, pc);
}

static void
probe_mt65xx_mon_tracepoint(void *ignore, struct task_struct *prev, struct task_struct *next)
{
	struct trace_array_cpu *data;
	unsigned long flags;
	int cpu;
	int pc;

	if (unlikely(!mt65xx_mon_ref))
		return;

	if (!mt65xx_mon_enabled || mt65xx_mon_stopped)
		return;

	if (prev)
		tracing_record_cmdline(prev);
	if (next)
		tracing_record_cmdline(next);
	tracing_record_cmdline(current);

	pc = preempt_count();
	/* local_irq_save(flags); */
	spin_lock_irqsave(&mt65xx_mon_spinlock, flags);
	cpu = raw_smp_processor_id();
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	data = mt65xx_mon_trace->data[cpu];
#else
	data = per_cpu_ptr(mt65xx_mon_trace->trace_buffer.data, cpu);
#endif

	if (likely(!atomic_read(&data->disabled)))
		tracing_mt65xx_mon_function(mt65xx_mon_trace, prev, next, flags, pc);
	spin_unlock_irqrestore(&mt65xx_mon_spinlock, flags);
	/* local_irq_restore(flags); */
}

void tracing_mt65xx_mon_manual_stop(struct trace_array *tr, unsigned long flags, int pc)
{
#if 0
	struct ftrace_event_call *call = &event_mt65xx_mon;
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	struct ring_buffer *buffer = tr->buffer;
#else
	struct ring_buffer *buffer = tr->trace_buffer.buffer;
#endif
	struct ring_buffer_event *event;
	struct mt65xx_mon_entry *entry;
	unsigned int idx = 0;

	event = trace_buffer_lock_reserve(buffer, TRACE_MT65XX_MON_TYPE, sizeof(*entry), flags, pc);
	if (!event)
		return;

	entry = ring_buffer_event_data(event);


	mtk_mon->disable();

	entry->log = idx = mtk_mon->mon_log((void *)&entry->field);
	entry->cpu = raw_smp_processor_id();
#if 0
	if (!filter_check_discard(call, entry, buffer, event))
#endif
		trace_buffer_unlock_commit(buffer, event, flags, pc);
}

static void probe_mt65xx_mon_manual_tracepoint(void *ignore, unsigned int manual_start)
{
	struct trace_array_cpu *data;
	unsigned long flags;
	int cpu;
	int pc;

	if (unlikely(!mt65xx_mon_ref))
		return;

	if (!mt65xx_mon_enabled || mt65xx_mon_stopped)
		return;

	if ((manual_start != 0) && (manual_start != 1))
		return;

	tracing_record_cmdline(current);

	if (manual_start == is_manual_start)	/* if already started or stopped */
		return;

	if (manual_start == 1) {
		/* for START operation, only enable mt65xx monitor */
		mtk_mon->enable();
		return;
	} else {
		/* for STOP operation. log monitor data into buffer */
		pc = preempt_count();
		local_irq_save(flags);
		cpu = raw_smp_processor_id();
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		data = mt65xx_mon_trace->data[cpu];
#else
		data = per_cpu_ptr(mt65xx_mon_trace->trace_buffer.data, cpu);
#endif
		if (likely(!atomic_read(&data->disabled)))
			tracing_mt65xx_mon_manual_stop(mt65xx_mon_trace, flags, pc);
		local_irq_restore(flags);
	}
}

static int tracing_mt65xx_mon_register(void)
{
	int ret;

	ret = register_trace_mt65xx_mon_sched_switch(probe_mt65xx_mon_tracepoint, NULL);
	if (ret)
		pr_info("sched trace: Couldn't activate tracepoint probe to mt65xx monitor\n");

	ret = register_trace_mt65xx_mon_periodic(probe_mt65xx_mon_tracepoint, NULL);
	if (ret)
		pr_info("periodic trace: Couldn't activate tracepoint probe to mt65xx monitor\n");

	ret = register_trace_mt65xx_mon_manual(probe_mt65xx_mon_manual_tracepoint, NULL);
	if (ret)
		pr_info("manual trace: Couldn't activate tracepoint probe to mt65xx monitor\n");

	return ret;
}

static void tracing_mt65xx_mon_unregister(void)
{
	unregister_trace_mt65xx_mon_sched_switch(probe_mt65xx_mon_tracepoint, NULL);

	unregister_trace_mt65xx_mon_periodic(probe_mt65xx_mon_tracepoint, NULL);

	unregister_trace_mt65xx_mon_manual(probe_mt65xx_mon_manual_tracepoint, NULL);
}

static void tracing_start_mt65xx_mon(void)
{
	mutex_lock(&mt65xx_mon_mutex);
	if (!(mt65xx_mon_ref++))
		tracing_mt65xx_mon_register();
	mutex_unlock(&mt65xx_mon_mutex);
}

static void tracing_stop_mt65xx_mon(void)
{
	mutex_lock(&mt65xx_mon_mutex);
	if (!(--mt65xx_mon_ref))
		tracing_mt65xx_mon_unregister();
	mutex_unlock(&mt65xx_mon_mutex);
}

void tracing_start_mt65xx_mon_record(void)
{
	if (unlikely(!mt65xx_mon_trace)) {
		WARN_ON(1);
		return;
	}

	tracing_start_mt65xx_mon();

	mutex_lock(&mt65xx_mon_mutex);
	mt65xx_mon_enabled++;
	mutex_unlock(&mt65xx_mon_mutex);
}


static int mt65xx_mon_trace_init(struct trace_array *tr)
{

	mt65xx_mon_trace = tr;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	tracing_reset_online_cpus(tr);
#else
	tracing_reset_online_cpus(&tr->trace_buffer);
#endif
	tracing_start_mt65xx_mon_record();

	return 0;
}

static void mt65xx_mon_trace_reset(struct trace_array *tr)
{
	if (mt65xx_mon_ref) {
		mutex_lock(&mt65xx_mon_mutex);
		mt65xx_mon_enabled--;
		WARN_ON(mt65xx_mon_enabled < 0);
		mutex_unlock(&mt65xx_mon_mutex);

		tracing_stop_mt65xx_mon();
	}
}

static void mt65xx_mon_trace_start(struct trace_array *tr)
{
	ktime_t kt;

	int ret;
	ret = register_monitor(&mtk_mon, monitor_mode);

	if (ret != 0) {
		pr_info("MTK Monitor Register Fail\n");
		return;
	}
	pr_info("MTK Monitor Register OK\n");
	mtk_mon->init();

	mt65xx_mon_stopped = 0;
	if ((monitor_mode == MODE_PERIODIC) && (timer_initialized > 0)) {
		kt = ktime_set(0, mon_period_ns);
		hrtimer_restart(&timer);
	}
	mtk_mon->enable();
}

static void mt65xx_mon_trace_stop(struct trace_array *tr)
{
	mt65xx_mon_stopped = 1;

	if ((monitor_mode == MODE_PERIODIC) && (timer_initialized > 0))
		hrtimer_cancel(&timer);

	if (mtk_mon == NULL) {
		pr_info("MTK Monitor doesnt register!!!\n");
		return;
	}
	mtk_mon->deinit();
	unregister_monitor(&mtk_mon);
}

static struct tracer mt65xx_mon_tracer __read_mostly = {
	.name = "mt65xx monitor",
	.init = mt65xx_mon_trace_init,
	.reset = mt65xx_mon_trace_reset,
	.start = mt65xx_mon_trace_start,
	.stop = mt65xx_mon_trace_stop,
	.wait_pipe = poll_wait_pipe,
#ifdef CONFIG_FTRACE_SELFTEST
	.selftest = trace_selftest_startup_mtk,
#endif
};

static __init int init_mt65xx_mon_trace(void)
{
	queue = create_singlethread_workqueue("trace mon");
	if (!queue)
		pr_info("create monitor work queue error\n");
	INIT_WORK(&work, work_handler);
	return register_tracer(&mt65xx_mon_tracer);
}
device_initcall(init_mt65xx_mon_trace);
