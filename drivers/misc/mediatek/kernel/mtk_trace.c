#include <linux/ring_buffer.h>
#include "kernel/trace/trace.h"
#include <linux/mtk_ftrace.h>

#ifdef CONFIG_MTK_KERNEL_MARKER
static unsigned long __read_mostly tracing_mark_write_addr = 0;
static void inline __mt_update_tracing_mark_write_addr(void)
{
	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr = kallsyms_lookup_name("tracing_mark_write");
}

void inline mt_kernel_trace_begin(char *name)
{
	if (mt_kernel_marker_enabled) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr, "B|%d|%s\n", current->tgid, name);
	}
}
EXPORT_SYMBOL(mt_kernel_trace_begin);

void inline mt_kernel_trace_counter(char *name, int count)
{
	if (mt_kernel_marker_enabled) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr,
				   "C|%d|%s|%d\n", current->tgid, name, count);
	}
}
EXPORT_SYMBOL(mt_kernel_trace_counter);

void inline mt_kernel_trace_end(void)
{
	if (mt_kernel_marker_enabled) {
		__mt_update_tracing_mark_write_addr();
		event_trace_printk(tracing_mark_write_addr, "E\n");
	}
}
EXPORT_SYMBOL(mt_kernel_trace_end);
#endif

#if defined(CONFIG_MTK_HIBERNATION) && defined(CONFIG_MTK_SCHED_TRACERS)
int resize_ring_buffer_for_hibernation(int enable)
{
	int ret = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	struct trace_array *tr = NULL;
#endif

	if (enable) {
		ring_buffer_expanded = 0;
		ret = tracing_update_buffers();
	} else {
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
		ret = tracing_resize_ring_buffer(0, RING_BUFFER_ALL_CPUS);
#else
		tr = top_trace_array();
		if (tr)
			ret = tracing_resize_ring_buffer(tr, 0, RING_BUFFER_ALL_CPUS);
#endif
	}

	return ret;
}
#endif

#ifdef CONFIG_MTK_SCHED_TRACERS
void print_enabled_events(struct seq_file *m)
{

	struct ftrace_event_call *call;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	struct ftrace_event_file *file;
	struct trace_array *tr;
#endif

	seq_puts(m, "# enabled events:");
	/* mutex_lock(&event_mutex); */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
	list_for_each_entry(call, &ftrace_events, list) {
		if (call->flags & TRACE_EVENT_FL_ENABLED)
			seq_printf(m, " %s:%s", call->class->system, call->name);
	}
#else
	list_for_each_entry(tr, &ftrace_trace_arrays, list) {
		list_for_each_entry(file, &tr->events, list) {
			call = file->event_call;
			if (file->flags & FTRACE_EVENT_FL_ENABLED)
				seq_printf(m, " %s:%s", call->class->system, call->name);
		}
	}
#endif
	/* mutex_unlock(&event_mutex); */
	seq_puts(m, "\n");
}

/* ftrace's switch function for MTK solution */
void mt_ftrace_enable_disable(int enable)
{
	if (enable) {
		trace_set_clr_event(NULL, "sched_switch", 1);
		trace_set_clr_event(NULL, "sched_wakeup", 1);
		trace_set_clr_event(NULL, "sched_wakeup_new", 1);
		trace_set_clr_event(NULL, "softirq_entry", 1);
		trace_set_clr_event(NULL, "softirq_exit", 1);
		trace_set_clr_event(NULL, "softirq_raise", 1);
#ifdef CONFIG_SMP
		trace_set_clr_event(NULL, "sched_migrate_task", 1);
#endif
		trace_set_clr_event(NULL, "workqueue_execute_start", 1);
		trace_set_clr_event(NULL, "workqueue_execute_end", 1);

		trace_set_clr_event(NULL, "block_bio_frontmerge", 1);
		trace_set_clr_event(NULL, "block_bio_backmerge", 1);
		trace_set_clr_event(NULL, "block_rq_issue", 1);
		trace_set_clr_event(NULL, "block_rq_insert", 1);
		trace_set_clr_event(NULL, "block_rq_complete", 1);

		trace_set_clr_event("mtk_events", NULL, 1);
		trace_set_clr_event("ipi", NULL, 1);

        trace_set_clr_event("met_bio", NULL, 1);
        trace_set_clr_event("met_fuse", NULL, 1);

		tracing_on();
	} else {
		tracing_off();
		trace_set_clr_event(NULL, NULL, 0);
	}
}

#endif

#if defined(CONFIG_MTK_SCHED_TRACERS) && defined(CONFIG_HOTPLUG_CPU)
#include <linux/cpu.h>
#include <trace/events/mtk_events.h>

static DEFINE_PER_CPU(unsigned long long, last_event_ts);
static struct notifier_block hotplug_event_notifier;

static int hotplug_event_notify(struct notifier_block *self,
			 unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
    switch (action) {
        case CPU_STARTING:
        case CPU_STARTING_FROZEN:
            trace_cpu_hotplug(cpu, 1, per_cpu(last_event_ts, cpu));
            per_cpu(last_event_ts, cpu) = ns2usecs(ftrace_now(cpu));
            break;
        case CPU_DEAD:
        case CPU_DEAD_FROZEN:
            trace_cpu_hotplug(cpu, 0, per_cpu(last_event_ts, cpu));
            per_cpu(last_event_ts, cpu) = ns2usecs(ftrace_now(cpu));
            break;
	    default:
	    	break;
	}
	return NOTIFY_OK;
}

static __init int hotplug_events_init(void)
{
	hotplug_event_notifier.notifier_call = hotplug_event_notify;
	hotplug_event_notifier.priority = 0;
	register_cpu_notifier(&hotplug_event_notifier);
    return 0;
}
early_initcall(hotplug_events_init);
#endif
