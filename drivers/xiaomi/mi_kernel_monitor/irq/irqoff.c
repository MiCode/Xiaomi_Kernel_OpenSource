
#define pr_fmt(fmt) "trace-irqoff: " fmt

#include <linux/hrtimer.h>
#include <linux/irqflags.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>
#include <linux/stacktrace.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/version.h>
#include <asm-generic/irq_regs.h>

#include <linux/sched/clock.h> 
#include <linux/sched/stat.h>
#include "internal.h"


#define IRQ_OFF_DEFINE_SHOW_ATTRIBUTE(__name)				\
static int __name ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __name ## _show, inode->i_private);	\
}									\
									\
static const struct proc_ops __name ## _fops = {			\
	.proc_open	= __name ## _open,				\
	.proc_read	= seq_read,					\
	.proc_lseek	= seq_lseek,					\
	.proc_release	= single_release,				\
}

static bool trace_enable = false;

static u64 sampling_period = 15 * 1000 * 1000UL;
static u64 trace_irqoff_latency = 30 * 1000 * 1000UL;

struct stack_trace_metadata {
	u64 last_timestamp;
	unsigned int nr_irqoff_trace;
	struct stack_entry trace[MAX_STACE_TRACE_ENTRIES];
	unsigned int nr_entries;
	unsigned long entries[MAX_TRACE_ENTRIES + OVER_FLOW_LEN];

	/* Task command names*/
	char curr_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	char parent_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];

	/* Task pids*/
	pid_t pids[MAX_STACE_TRACE_ENTRIES];
	u64 timestamp[MAX_STACE_TRACE_ENTRIES];

	struct {
		u64 nsecs:63;
		u64 more:1;
	} latency[MAX_STACE_TRACE_ENTRIES];
};

struct per_cpu_stack_trace {
	struct timer_list timer;
	struct hrtimer hrtimer;
	struct stack_trace_metadata hardirq_trace;
	struct stack_trace_metadata softirq_trace;

	bool softirq_delayed;
};

static struct per_cpu_stack_trace __percpu *cpu_stack_trace;

static bool save_trace(struct pt_regs *regs, bool hardirq, u64 latency)
{
	unsigned long nr_entries, nr_irqoff_trace;
	struct stack_entry *trace;
	struct stack_trace_metadata *stack_trace;

	stack_trace = hardirq ? this_cpu_ptr(&cpu_stack_trace->hardirq_trace) :
		      this_cpu_ptr(&cpu_stack_trace->softirq_trace);

	nr_entries = stack_trace->nr_entries;
	if (unlikely(nr_entries >= MAX_TRACE_ENTRIES))
		return false;

	nr_irqoff_trace = stack_trace->nr_irqoff_trace;
	if (unlikely(nr_irqoff_trace >= MAX_STACE_TRACE_ENTRIES))
		return false;

	////strlcpy(stack_trace->curr_comms[nr_irqoff_trace], current->comm, (unsigned long)TASK_COMM_LEN);
	////strlcpy(stack_trace->parent_comms[nr_irqoff_trace], current->group_leader->comm, (unsigned long)TASK_COMM_LEN);
	strcpy(stack_trace->curr_comms[nr_irqoff_trace], current->comm);
	stack_trace->curr_comms[nr_irqoff_trace][TASK_COMM_LEN - 1] = 0;
	strcpy(stack_trace->parent_comms[nr_irqoff_trace], current->group_leader->comm);
	stack_trace->parent_comms[nr_irqoff_trace][TASK_COMM_LEN - 1] = 0;

	stack_trace->pids[nr_irqoff_trace] = current->pid;
	stack_trace->timestamp[nr_irqoff_trace] = sched_clock();
	stack_trace->latency[nr_irqoff_trace].nsecs = latency;
	stack_trace->latency[nr_irqoff_trace].more = !hardirq && regs;

	trace = stack_trace->trace + nr_irqoff_trace;
	xm_store_stack_trace(regs, trace, stack_trace->entries + nr_entries, MAX_TRACE_ENTRIES - nr_entries, 0);
	stack_trace->nr_entries += trace->nr_entries;

	smp_store_release(&stack_trace->nr_irqoff_trace, nr_irqoff_trace + 1);

	if ((stack_trace->nr_entries >= MAX_TRACE_ENTRIES) || (stack_trace->nr_irqoff_trace > MAX_STACE_TRACE_ENTRIES)) {
		return false;
	}

	return true;
}

static bool trace_irqoff_record(u64 delta, bool hardirq, bool skip)
{
	int index = 0;
	u64 throttle = sampling_period << 1;
	u64 delta_old;

	if (delta < throttle)
		return false;

	delta -= sampling_period;
	delta_old = delta;
	delta >>= 1;
	while (delta > sampling_period) {
		index++;
		delta >>= 1;
	}

	if (unlikely(delta_old >= trace_irqoff_latency) && !is_idle_task(current) && !xm_single_task_running())
		save_trace(skip ? xm_get_irq_regs() : NULL, hardirq, delta_old);

	return true;
}

static enum hrtimer_restart trace_irqoff_hrtimer_handler(struct hrtimer *hrtimer)
{
	u64 now = local_clock(), delta;

	delta = now - __this_cpu_read(cpu_stack_trace->hardirq_trace.last_timestamp);
	__this_cpu_write(cpu_stack_trace->hardirq_trace.last_timestamp, now);

	if (trace_irqoff_record(delta, true, true)) {
		__this_cpu_write(cpu_stack_trace->softirq_trace.last_timestamp,
				 now);
	} else if (!__this_cpu_read(cpu_stack_trace->softirq_delayed)) {
		u64 delta_soft;

		delta_soft = now -
			__this_cpu_read(cpu_stack_trace->softirq_trace.last_timestamp);

		if (unlikely(delta_soft >= trace_irqoff_latency + sampling_period)) {
			__this_cpu_write(cpu_stack_trace->softirq_delayed, true);
			trace_irqoff_record(delta_soft, false, true);
		}
	}

	hrtimer_forward_now(hrtimer, ns_to_ktime(sampling_period));

	return HRTIMER_RESTART;
}

static void trace_irqoff_timer_handler(struct timer_list *timer)
{
	u64 now = local_clock(), delta;

	delta = now - __this_cpu_read(cpu_stack_trace->softirq_trace.last_timestamp);
	__this_cpu_write(cpu_stack_trace->softirq_trace.last_timestamp, now);
	__this_cpu_write(cpu_stack_trace->softirq_delayed, false);
	trace_irqoff_record(delta, false, false);

	mod_timer(timer, jiffies + msecs_to_jiffies(sampling_period / 1000000UL));

}

static void smp_clear_stack_trace(void *info)
{
	struct per_cpu_stack_trace *stack_trace = info;

	stack_trace->hardirq_trace.nr_entries = 0;
	stack_trace->hardirq_trace.nr_irqoff_trace = 0;
	stack_trace->softirq_trace.nr_entries = 0;
	stack_trace->softirq_trace.nr_irqoff_trace = 0;
}

static void smp_timers_start(void *info)
{
	u64 now = local_clock();
	struct per_cpu_stack_trace *stack_trace = info;
	struct hrtimer *hrtimer = &stack_trace->hrtimer;
	struct timer_list *timer = &stack_trace->timer;

	stack_trace->hardirq_trace.last_timestamp = now;
	stack_trace->softirq_trace.last_timestamp = now;

	hrtimer_start_range_ns(hrtimer, ns_to_ktime(sampling_period),
			       0, HRTIMER_MODE_REL_PINNED);

	timer->expires = jiffies + msecs_to_jiffies(sampling_period / 1000000UL);
	add_timer_on(timer, smp_processor_id());
}

static void seq_print_stack_trace(struct seq_file *m, struct stack_entry *trace)
{
	int i;

	if (WARN_ON(!trace->entries))
		return;

	for (i = 0; i < trace->nr_entries; i++)
		seq_printf(m, "%*c%pS\n", 5, ' ', (void *)trace->entries[i]);
}

static ssize_t trace_latency_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	unsigned long latency;

	if (kstrtoul_from_user(buf, count, 0, &latency))
		return -EINVAL;

	if (latency == 0) {
		int cpu;

		for_each_online_cpu(cpu)
			smp_call_function_single(cpu, smp_clear_stack_trace,
						 per_cpu_ptr(cpu_stack_trace, cpu),
						 true);
		return count;
	} else if (latency < (sampling_period << 1) / (1000 * 1000UL))
		return -EINVAL;

	trace_irqoff_latency = latency * 1000 * 1000UL;

	return count;
}

static void trace_latency_show_one(struct seq_file *m, void *v, bool hardirq)
{
	int cpu;

	for_each_online_cpu(cpu) {
		int i;
		unsigned long nr_irqoff_trace;
		struct stack_trace_metadata *stack_trace;

		stack_trace = hardirq ?
			per_cpu_ptr(&cpu_stack_trace->hardirq_trace, cpu) :
			per_cpu_ptr(&cpu_stack_trace->softirq_trace, cpu);

		nr_irqoff_trace = smp_load_acquire(&stack_trace->nr_irqoff_trace);

		if (!nr_irqoff_trace)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr_irqoff_trace; i++) {
			struct stack_entry *trace = stack_trace->trace + i;

			seq_printf(m, "%*cthread:%s process:%s pid:%d timestamp:%llu duration:%llu%s\n",
				   5, ' ', stack_trace->curr_comms[i], stack_trace->parent_comms[i],
				   stack_trace->pids[i],
				   stack_trace->timestamp[i],
				   stack_trace->latency[i].nsecs / (1000 * 1000UL),
				   stack_trace->latency[i].more ? "+ms" : "ms");
			seq_print_stack_trace(m, trace);
			seq_putc(m, '\n');

			cond_resched();
		}
	}
}

static int trace_latency_show(struct seq_file *m, void *v)
{
	seq_printf(m, "trace_irqoff_latency: %llums\n\n",
		   trace_irqoff_latency / (1000 * 1000UL));

	seq_puts(m, " hardirq:\n");
	trace_latency_show_one(m, v, true);

	seq_putc(m, '\n');

	seq_puts(m, " softirq:\n");
	trace_latency_show_one(m, v, false);

	return 0;
}

static int trace_latency_open(struct inode *inode, struct file *file)
{
	return single_open(file, trace_latency_show, inode->i_private);
}

static const struct proc_ops trace_latency_fops = {
	.proc_open	= trace_latency_open,
	.proc_read	= seq_read,
	.proc_write	= trace_latency_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%s\n", trace_enable ? "enabled" : "disabled");

	return 0;
}

static int enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, enable_show, inode->i_private);
}

static void trace_irqoff_start_timers(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct hrtimer *hrtimer;
		struct timer_list *timer;

		hrtimer = per_cpu_ptr(&cpu_stack_trace->hrtimer, cpu);
		hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_PINNED);
		hrtimer->function = trace_irqoff_hrtimer_handler;
		timer = per_cpu_ptr(&cpu_stack_trace->timer, cpu);
		timer_setup(timer, trace_irqoff_timer_handler,
			    TIMER_PINNED | TIMER_IRQSAFE);


		smp_call_function_single(cpu, smp_timers_start,
					 per_cpu_ptr(cpu_stack_trace, cpu),
					 true);
	}
}

static void trace_irqoff_cancel_timers(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct hrtimer *hrtimer;
		struct timer_list *timer;

		hrtimer = per_cpu_ptr(&cpu_stack_trace->hrtimer, cpu);
		hrtimer_cancel(hrtimer);

		timer = per_cpu_ptr(&cpu_stack_trace->timer, cpu);
		del_timer_sync(timer);
	}
}

static ssize_t enable_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	bool enable;

	if (kstrtobool_from_user(buf, count, &enable))
		return -EINVAL;

	if (!!enable == !!trace_enable)
		return count;

	if (enable)
		trace_irqoff_start_timers();
	else
		trace_irqoff_cancel_timers();

	trace_enable = enable;

	return count;
}

static const struct proc_ops enable_fops = {
	.proc_open	= enable_open,
	.proc_read	= seq_read,
	.proc_write	= enable_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int sampling_period_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%llums\n", sampling_period / (1000 * 1000UL));

	return 0;
}

static int sampling_period_open(struct inode *inode, struct file *file)
{
	return single_open(file, sampling_period_show, inode->i_private);
}

static ssize_t sampling_period_write(struct file *file, const char __user *buf,
				     size_t count, loff_t *ppos)
{
	unsigned long period;

	if (trace_enable)
		return -EINVAL;

	if (kstrtoul_from_user(buf, count, 0, &period))
		return -EINVAL;

	period *= 1000 * 1000UL;
	if (period > (trace_irqoff_latency >> 1))
		trace_irqoff_latency = period << 1;

	sampling_period = period;

	return count;
}

static const struct proc_ops sampling_period_fops = {
	.proc_open	= sampling_period_open,
	.proc_read	= seq_read,
	.proc_write	= sampling_period_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

int  xiaomi_trace_irqoff_init(void)
{
	struct proc_dir_entry *parent_dir;
	
	cpu_stack_trace = alloc_percpu(struct per_cpu_stack_trace);
	if (!cpu_stack_trace)
	{
		return -ENOMEM;
	}

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_IRQ_OFF, NULL);
	if (!parent_dir)
	{
		goto free_percpu;
	}

	if (!proc_create("trace_latency", 0666 | S_IWUSR, parent_dir, &trace_latency_fops))
	{
		goto remove_proc;
	}

	if (!proc_create("enable", 0666, parent_dir, &enable_fops))
	{
		goto remove_proc;
	}

	if (!proc_create("sampling_period", 0666, parent_dir, &sampling_period_fops))
	{
		goto remove_proc;
	}

	if (trace_enable)
	{
		trace_irqoff_start_timers();
	}

	return 0;

remove_proc:
	remove_proc_subtree(PROC_TRACE_IRQ_OFF, NULL);
free_percpu:
	free_percpu(cpu_stack_trace);

	return -ENOMEM;
}

void  xiaomi_trace_irqoff_exit(void)
{
	if (trace_enable)
		trace_irqoff_cancel_timers();
	remove_proc_subtree(PROC_TRACE_IRQ_OFF, NULL);
	free_percpu(cpu_stack_trace);
}
