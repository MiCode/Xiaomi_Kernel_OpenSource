

#include <linux/hrtimer.h>
#include <linux/timer.h>
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
#include <linux/rbtree.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>
#include <asm/thread_info.h>
#include <linux/thread_info.h>

#include <asm-generic/irq_regs.h>

#include "internal.h"
#include "pub/trace_point.h"
#include "pub/kprobe.h"

#include "uapi/sys_delay.h"

static bool trace_enable = false;

struct xm_sys_delay_settings sys_delay_settings = {
	threshold_ms : 50,
};

static struct xm_stack_trace __percpu *p_sys_delay_trace;


static void __maybe_unused clean_data(void)
{
}

static inline void update_sched_time(long id)
{
	struct xm_percpu_context *context;
	unsigned long flags;

	if (!sys_delay_settings.activated)
		return;

	local_irq_save(flags);
	context = get_percpu_context();
	context->sys_delay.syscall_start_time = sched_clock();
	context->sys_delay.sys_delay_max_ms = 0;
	local_irq_restore(flags);
}

static bool xm_stack_trace_record_for_sysdelay(struct task_struct* tsk, struct xm_stack_trace *stack_trace, struct pt_regs *regs, int duration)
{
	unsigned int nr_entries, nr_stack_entries;
	struct stack_entry *stack_entry;

	nr_entries = stack_trace->nr_entries;
	if (nr_entries >= MAX_TRACE_ENTRIES)
	{
		return false;
	}

	nr_stack_entries = stack_trace->nr_stack_entries;
	if (nr_stack_entries >= MAX_STACE_TRACE_ENTRIES)
		return false;

	////strlcpy(stack_trace->curr_comms[nr_stack_entries], current->comm, (unsigned long)TASK_COMM_LEN);
	////strlcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm, (unsigned long)TASK_COMM_LEN);
	strcpy(stack_trace->curr_comms[nr_stack_entries], current->comm);
	stack_trace->curr_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;
	strcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm);
	stack_trace->parent_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;

	stack_trace->pids[nr_stack_entries] = tsk->pid;
	stack_trace->duration[nr_stack_entries] = duration;
	stack_trace->timestamp[nr_stack_entries] = sched_clock();

	stack_entry = stack_trace->stack_entries + nr_stack_entries;
	xm_store_stack_trace(regs, stack_entry, stack_trace->entries + nr_entries, MAX_TRACE_ENTRIES - nr_entries, 0);
	stack_trace->nr_entries += stack_entry->nr_entries;

	smp_store_release(&stack_trace->nr_stack_entries, nr_stack_entries + 1);

	if ((stack_trace->nr_entries >= MAX_TRACE_ENTRIES) || (stack_trace->nr_stack_entries > MAX_STACE_TRACE_ENTRIES)) {
		pr_info("BUG: xiaomi sys_delay MAX_TRACE_ENTRIES too low on cpu: %d, nr_entries=%u, nr_stack_entries=%u\n", smp_processor_id(), stack_trace->nr_entries, stack_trace->nr_stack_entries);

		return false;
	}

	return true;
}

static inline bool stack_trace_record(struct xm_stack_trace *stack_trace, int delta)
{
	if (unlikely(delta >= sys_delay_settings.threshold_ms))
		return xm_stack_trace_record_for_sysdelay(current, stack_trace, xm_get_irq_regs(), delta);

	return false;
}

static void trace_sched_switch_hit(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next, unsigned int pre_state)
{
	update_sched_time(-1);
}

static void trace_sys_enter_hit(void *__data, struct pt_regs *regs, long id)
{
	update_sched_time(id);
}

static void trace_sys_exit_hit(void *__data, struct pt_regs *regs, long ret)
{
}

static int task_in_sys_loop(struct xm_percpu_context *context)
{
	int ret = 0;
	int cpu = smp_processor_id();

	if (user_mode(xm_get_irq_regs()))
	{
		return ret;
	}

	if (orig_idle_task(cpu) == current)
	{
		return ret;
	}
	
	ret = 1;
	return ret;
}

void syscall_timer(struct xm_percpu_context *context)
{
	struct sys_delay_detail *detail;

	if (sys_delay_settings.activated) {
		if (user_mode(xm_get_irq_regs()))
			update_sched_time(-1);

		if (task_in_sys_loop(context)) {
			if (need_dump(sys_delay_settings.threshold_ms,
						&context->sys_delay.sys_delay_max_ms, context->sys_delay.syscall_start_time)) {
				u64 delay_ns = sched_clock() - context->sys_delay.syscall_start_time;
				struct xm_stack_trace *stack_trace = this_cpu_ptr(p_sys_delay_trace);

				detail = &xm_percpu_context[smp_processor_id()]->sys_delay_detail;
				detail->delay_ns = delay_ns;

				////printk("xiaomi cpu[%d]:delay_ms=%llu ms,current=%s,pid=%d\n", smp_processor_id(), delay_ns/1000000, current->comm, current->pid);
				////get_kernel_bt(current, delay_ns/1000000);
				stack_trace_record(stack_trace, (int)(delay_ns/1000000));
				update_sched_time(-1);
				////get_kernel_bt(current, (int)(delay_ns/1000000), "trace_sys_delay");
			}
		} 
		else
		{
			update_sched_time(-1);
		}
	}
}

static int reset_stack_trace(void)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_sys_delay_trace, cpu);
		cpu_stack_trace->nr_stack_entries = 0;
		cpu_stack_trace->nr_entries = 0;
	}

	return 0;
}

static int __activate_sys_delay(void)
{

	clean_data();

	msleep(10);

	hook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	hook_tracepoint("sys_exit", trace_sys_exit_hit, NULL);
	hook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);

	reset_stack_trace();
	
	return 1;
}

int activate_sys_delay(void)
{
        printk("huqh activate_mutex_monitor enter\n");
	if (!sys_delay_settings.activated)
		sys_delay_settings.activated = __activate_sys_delay();

	return sys_delay_settings.activated;
}

static void __deactivate_sys_delay(void)
{
	msleep(10);

	unhook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	unhook_tracepoint("sys_exit", trace_sys_exit_hit, NULL);
	unhook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);

	synchronize_sched();
	clean_data();
}

int deactivate_sys_delay(void)
{
	if (sys_delay_settings.activated)
		__deactivate_sys_delay();
	sys_delay_settings.activated = 0;

	return 0;
}

static int sampling_period_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", xm_timer_period);

	return 0;
}

static ssize_t sampling_period_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	xm_timer_period = val;

	return count;
}

static int threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", sys_delay_settings.threshold_ms);

	return 0;
}

static ssize_t threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	sys_delay_settings.threshold_ms = val;

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
		activate_sys_delay();
	} else {
	    deactivate_sys_delay();
	}

	trace_enable = enable;

	return count;
}

static inline void seq_print_stack_trace(struct seq_file *m, struct stack_entry *entry)
{
	int i;

	if (WARN_ON(!entry->entries))
		return;

	for (i = 0; i < entry->nr_entries; i++)
		seq_printf(m, "%*c%pS\n", 5, ' ', (void *)entry->entries[i]);
}

static int stack_trace_show(struct seq_file *m, void *ptr)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_sys_delay_trace, cpu);

		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);
		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d duration:%dms timestamp:%llu\n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}

	return 0;

}

static ssize_t stack_trace_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(threshold);
DEFINE_PROC_ATTRIBUTE_RW(sampling_period);
DEFINE_PROC_ATTRIBUTE_RW(enable);
DEFINE_PROC_ATTRIBUTE_RW(stack_trace);

int xiaomi_sys_delay_init(void)
{
	struct proc_dir_entry *parent_dir;

	if (sys_delay_settings.activated)
		__activate_sys_delay();

	p_sys_delay_trace = alloc_percpu(struct xm_stack_trace);
	if (!p_sys_delay_trace)
		return -ENOMEM;

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_SYS_DELAY, NULL);

	if (!parent_dir)
		goto free_buf;
	if (!proc_create_data("threshold", 0666, parent_dir, &threshold_fops, NULL))
		goto remove_proc;
	if (!proc_create_data("sampling_period", 0666, parent_dir, &sampling_period_fops, NULL))
		goto remove_proc;		
	if (!proc_create_data("enable", 0666, parent_dir, &enable_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("stack_trace", 0, parent_dir, &stack_trace_fops, NULL))
		goto remove_proc;
	
	return 0;
		
	remove_proc:
		remove_proc_subtree(PROC_TRACE_SYS_DELAY, NULL);
	free_buf:
		return -ENOMEM;

}

void xiaomi_sys_delay_exit(void)
{
	if (sys_delay_settings.activated)
		deactivate_sys_delay();
	sys_delay_settings.activated = 0;
	remove_proc_subtree(PROC_TRACE_SYS_DELAY, NULL);
	free_percpu(p_sys_delay_trace);
}
