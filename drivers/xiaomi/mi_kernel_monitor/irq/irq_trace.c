
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timex.h>
#include <linux/tracepoint.h>
//#include <trace/events/irq.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <trace/events/napi.h>
#include <linux/rtc.h>
#include <linux/time.h>

#include "internal.h"
#include "pub/trace_point.h"

#include "uapi/irq_trace.h"

static bool trace_enable = false;

struct S_IRQ_TRACE_DETAIL
{
    int count;
	struct IRQ_TRACE_DETAIL irq_trace_detail[MAX_STACE_TRACE_ENTRIES];
};

static struct S_IRQ_TRACE_DETAIL __percpu *p_irq_trace_detail;

struct xm_irq_trace_settings irq_trace_settings = {
	.threshold_irq = 25,
	.threshold_sirq = 25,
	.threshold_timer = 25,
};

static int irq_trace_alloced = 0;
static struct softirq_action *orig_softirq_vec;

static void clean_data(void)
{
	//
}

static void record_dither(int source, void *func, unsigned long time)
{
	struct S_IRQ_TRACE_DETAIL *stack_trace = this_cpu_ptr(p_irq_trace_detail);
	int cnt = stack_trace->count;
	
	if (cnt >= MAX_STACE_TRACE_ENTRIES)
	{
	    printk("cpu[%d] xiaomi irq_trace info is full!\n", smp_processor_id());
		return;
	}

    ////printk("xiaomi_km irqhander source=%d, time=%lums, func=%pS\n", source, time, func);
	////do_xm_gettimeofday(&irq_trace_detail.tv);
	stack_trace->irq_trace_detail[cnt].cpu = smp_processor_id();
	stack_trace->irq_trace_detail[cnt].source = source;
	stack_trace->irq_trace_detail[cnt].func = func;
	stack_trace->irq_trace_detail[cnt].time = time;
	stack_trace->irq_trace_detail[cnt].timestamp = sched_clock();
	stack_trace->count++;
}

static void trace_irq_handler_entry_hit(void *ignore, int irq,
                struct irqaction *action)
{
	struct xm_irq_trace *irq_trace;
	struct xm_percpu_context *context;
	u64 now;

	if (hardirq_count() > (1 << HARDIRQ_SHIFT))
		return;

	context = get_percpu_context();
	irq_trace = &context->irq_trace;
	now = sched_clock(); //ktime_to_ns(ktime_get());
	irq_trace->irq.irq = irq;
	irq_trace->irq.start_time = now;

	return;
}

static void trace_irq_handler_exit_hit(void *ignore, int irq,
                struct irqaction *action, int ret)
{
	struct xm_irq_trace *irq_trace;
	struct xm_percpu_context *context;
	u64 now = sched_clock(); //ktime_to_ns(ktime_get());
	u64 start_time;
	u64 delta_ns;

	if (hardirq_count() > (1 << HARDIRQ_SHIFT))
		return;

	context = get_percpu_context();
	irq_trace = &context->irq_trace;
	start_time = irq_trace->irq.start_time;
	if (ret && (start_time > 0)) {
		delta_ns = now - start_time;

		irq_trace->sum.irq_count++;
		irq_trace->sum.irq_runs += delta_ns;
		if (irq_trace_settings.threshold_irq && (delta_ns >> 20) >= irq_trace_settings.threshold_irq) {
			record_dither(0, action->handler, delta_ns >> 20);
		}
	}
}

static void trace_softirq_entry_hit(void *ignore, unsigned int nr_sirq)
{
	struct softirq_action *h;
	struct xm_irq_trace *irq_trace;
	struct xm_percpu_context *context;
	u64 now = sched_clock();  //ktime_to_ns(ktime_get());
	void *func;

	if (nr_sirq >= NR_SOFTIRQS)
		return;

	h = orig_softirq_vec + nr_sirq;
	func = h->action;
	context = get_percpu_context();
	irq_trace = &context->irq_trace;

	irq_trace->softirq.sirq = nr_sirq;
	irq_trace->softirq.start_time = now;
}

static void trace_softirq_exit_hit(void *ignore, unsigned int nr_sirq)
{
	struct softirq_action *h;
	void *func;
	struct xm_irq_trace *irq_trace;
	struct xm_percpu_context *context;
	u64 now = sched_clock(); //ktime_to_ns(ktime_get());
	u64 start_time;
	u64 delta_ns;

	if (nr_sirq >= NR_SOFTIRQS)
		return;

	h = orig_softirq_vec + nr_sirq;
	func = h->action;

	context = get_percpu_context();
	irq_trace = &context->irq_trace;
	start_time = irq_trace->softirq.start_time;
	if (start_time > 0) {
		delta_ns = now - start_time;

		irq_trace->sum.sirq_count[nr_sirq]++;
		irq_trace->sum.sirq_runs[nr_sirq] += delta_ns;
		if (irq_trace_settings.threshold_sirq && (delta_ns >> 20) >= irq_trace_settings.threshold_sirq) {
			record_dither(1, func, delta_ns >> 20);
		}
	}
}

#if 0
static void trace_hrtimer_expire_entry_hit(void *ignore, struct hrtimer *timer, ktime_t *_now)
{
	struct xm_irq_trace *irq_trace;
	struct xm_percpu_context *context;
	u64 now = sched_clock();  //ktime_to_ns(ktime_get());

	context = get_percpu_context();
	irq_trace = &context->irq_trace;

	irq_trace->timer.start_time = now;
}

static void trace_hrtimer_expire_exit_hit(void *ignore, struct hrtimer *timer)
{
	void *func = timer->function;
	struct xm_irq_trace *irq_trace;
	struct xm_percpu_context *context;
	u64 now = sched_clock();  //ktime_to_ns(ktime_get());
	u64 start_time;
	u64 delta_ns;

	context = get_percpu_context();
	irq_trace = &context->irq_trace;
	start_time = irq_trace->timer.start_time;
	if (start_time > 0) {
		delta_ns = now - start_time;

		irq_trace->sum.timer_count++;
		irq_trace->sum.timer_runs += delta_ns;
		if (irq_trace_settings.threshold_timer && (delta_ns >> 20) >= irq_trace_settings.threshold_timer) {
			record_dither(3, func, delta_ns >> 20);
		}
	}
}
#endif

static int __activate_irq_trace(void)
{
	irq_trace_alloced = 1;

	clean_data();

	hook_tracepoint("irq_handler_entry", trace_irq_handler_entry_hit, NULL);
	hook_tracepoint("irq_handler_exit", trace_irq_handler_exit_hit, NULL);
	hook_tracepoint("softirq_entry", trace_softirq_entry_hit, NULL);
	hook_tracepoint("softirq_exit", trace_softirq_exit_hit, NULL);
	////hook_tracepoint("timer_expire_entry", trace_timer_expire_entry_hit, NULL);
	////hook_tracepoint("timer_expire_exit", trace_timer_expire_exit_hit, NULL);
	////hook_tracepoint("hrtimer_expire_entry", trace_hrtimer_expire_entry_hit, NULL);
	////hook_tracepoint("hrtimer_expire_exit", trace_hrtimer_expire_exit_hit, NULL);

	return 1;
}

static void __deactivate_irq_trace(void)
{
	unhook_tracepoint("irq_handler_entry", trace_irq_handler_entry_hit, NULL);
	unhook_tracepoint("irq_handler_exit", trace_irq_handler_exit_hit, NULL);
	unhook_tracepoint("softirq_entry", trace_softirq_entry_hit, NULL);
	unhook_tracepoint("softirq_exit", trace_softirq_exit_hit, NULL);
	////unhook_tracepoint("timer_expire_entry", trace_timer_expire_entry_hit, NULL);
	////unhook_tracepoint("timer_expire_exit", trace_timer_expire_exit_hit, NULL);
	////unhook_tracepoint("hrtimer_expire_entry", trace_hrtimer_expire_entry_hit, NULL);
	////unhook_tracepoint("hrtimer_expire_exit", trace_hrtimer_expire_exit_hit, NULL);

	clean_data();
	synchronize_sched();
}

static int reset_irq_trace(void)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		struct S_IRQ_TRACE_DETAIL *p_irq_detail;
		p_irq_detail = per_cpu_ptr(p_irq_trace_detail, cpu);
		p_irq_detail->count = 0;
	}

	return 0;
}

int activate_irq_trace(void)
{
	if (!irq_trace_settings.activated)
		irq_trace_settings.activated = __activate_irq_trace();

	reset_irq_trace();

	return irq_trace_settings.activated;
}

int deactivate_irq_trace(void)
{
	if (irq_trace_settings.activated)
		__deactivate_irq_trace();
	irq_trace_settings.activated = 0;

	return 0;
}

static int threshold_timer_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", irq_trace_settings.threshold_timer);

	return 0;
}

static ssize_t threshold_timer_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	irq_trace_settings.threshold_timer = val;

	return count;
}

static int threshold_sirq_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", irq_trace_settings.threshold_sirq);

	return 0;
}

static ssize_t threshold_sirq_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	irq_trace_settings.threshold_sirq = val;

	return count;
}

static int threshold_irq_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", irq_trace_settings.threshold_irq);

	return 0;
}

static ssize_t threshold_irq_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	irq_trace_settings.threshold_irq = val;

	return count;
}

static int enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%s\n", trace_enable ? "enabled" : "disabled");

	return 0;
}

static ssize_t enable_store(void *priv, const char __user *buf, size_t count)
{
	bool enable;

	if (kstrtobool_from_user(buf, count, &enable))
		return -EINVAL;

	if (!!enable == !!trace_enable)
		return count;

	if (enable) {
		activate_irq_trace();
	} else {
	    deactivate_irq_trace();
	}

	trace_enable = enable;

	return count;
}

static int irq_info_show(struct seq_file *m, void *ptr)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		struct S_IRQ_TRACE_DETAIL *irq_detail;
		irq_detail = per_cpu_ptr(p_irq_trace_detail, cpu);
		nr = smp_load_acquire(&irq_detail->count);
		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++)
		{
			seq_printf(m, "hardirq: cpu[%d] source[%d] func[%pS] time[%lu ms] timestamp[%llu]\n", 
						  irq_detail->irq_trace_detail[i].cpu,
						  irq_detail->irq_trace_detail[i].source,
						  irq_detail->irq_trace_detail[i].func,
						  irq_detail->irq_trace_detail[i].time,
						  irq_detail->irq_trace_detail[i].timestamp);
		}

	}

	return 0;
}

static ssize_t irq_info_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(threshold_irq); 
DEFINE_PROC_ATTRIBUTE_RW(threshold_sirq); 
DEFINE_PROC_ATTRIBUTE_RW(threshold_timer); 
DEFINE_PROC_ATTRIBUTE_RW(enable);
DEFINE_PROC_ATTRIBUTE_RW(irq_info);

static int lookup_syms(void)
{
	LOOKUP_SYMS(softirq_vec);

	return 0;
}

int xiaomi_irq_trace_init(void)
{
	struct proc_dir_entry *parent_dir;

	if (lookup_syms())
		return -EINVAL;

	clean_data();

	if (irq_trace_settings.activated)
		irq_trace_settings.activated = __activate_irq_trace();

	p_irq_trace_detail = alloc_percpu(struct S_IRQ_TRACE_DETAIL);
	if (!p_irq_trace_detail)
		return -ENOMEM;

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_IRQ_HANDLER, NULL);

	//parent_dir = proc_mkdir(PROC_DIR_NAME, NULL);
	if (!parent_dir)
		goto free_buf;
	if (!proc_create_data("threshold_irq", 0666, parent_dir, &threshold_irq_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("threshold_sirq", 0666, parent_dir, &threshold_sirq_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("threshold_timer", 0666, parent_dir, &threshold_timer_fops, NULL))
		goto remove_proc;
	
	if (!proc_create_data("enable", 0666, parent_dir, &enable_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("irq_info", 0, parent_dir, &irq_info_fops, NULL))
		goto remove_proc;


	return 0;
		
	remove_proc:
		remove_proc_subtree(PROC_TRACE_IRQ_HANDLER, NULL);

	free_buf:
		return -ENOMEM;

}

void xiaomi_irq_trace_exit(void)
{
	if (irq_trace_settings.activated)
		deactivate_irq_trace();
	irq_trace_settings.activated = 0;
	remove_proc_subtree(PROC_TRACE_IRQ_HANDLER, NULL);
	free_percpu(p_irq_trace_detail);
}
