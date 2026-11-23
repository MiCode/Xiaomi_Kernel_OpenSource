
#define pr_fmt(fmt) "trace-preemptoff: " fmt

#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sizes.h>
#include <linux/stacktrace.h>
#include <linux/timer.h>
#include <linux/tracepoint.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <trace/events/sched.h>
#include <asm-generic/irq_regs.h>
#include <linux/kprobes.h>
#include <uapi/linux/sched.h>


#include <linux/sched/clock.h>
#include <linux/sched/task.h>
#include <linux/sched/stat.h>
#include "internal.h"

#define NUM_TRACEPOINTS			1
	
struct tracepoint_entry {
	void *probe;
	const char *name;
	struct tracepoint *tp;
};

struct per_cpu_stack_trace {
	u64 last_timestamp;
	struct hrtimer hrtimer;
	struct task_struct *skip;

	unsigned int nr_stack_entries;
	unsigned int nr_entries;
	struct stack_entry stack_entries[MAX_STACE_TRACE_ENTRIES];
	unsigned long entries[MAX_TRACE_ENTRIES + OVER_FLOW_LEN];

	char curr_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	char parent_comms[MAX_STACE_TRACE_ENTRIES][TASK_COMM_LEN];
	pid_t pids[MAX_STACE_TRACE_ENTRIES];
	int prio[MAX_STACE_TRACE_ENTRIES];
	int duration[MAX_STACE_TRACE_ENTRIES];
	u64 timestamp[MAX_STACE_TRACE_ENTRIES];

};

struct noschedule_info {
	struct tracepoint_entry tp_entries[NUM_TRACEPOINTS];
	unsigned int tp_initalized;

	struct per_cpu_stack_trace __percpu *stack_trace;
};


static bool trace_enable = false;
/* Default sampling period is 10000000ns. The minimum value is 1000000ns. */
static u64 sampling_period = 15 * 1000 * 1000UL;

struct xm_trace_settings preemptoff_settings = {
	threshold_ms : 30,
};

static void trace_sched_switch_hit(void *priv, bool preempt,
			       struct task_struct *prev,
			       struct task_struct *next, unsigned int pre_state)
{
	u64 now = local_clock();
	struct per_cpu_stack_trace __percpu *stack_trace = priv;
	struct per_cpu_stack_trace *cpu_stack_trace = this_cpu_ptr(stack_trace);
	u64 last = cpu_stack_trace->last_timestamp;

	if (unlikely(!trace_enable))
		return;

	cpu_stack_trace->last_timestamp = now;
	if (unlikely(cpu_stack_trace->skip)) {
		unsigned int index = cpu_stack_trace->nr_stack_entries - 1;

		cpu_stack_trace->skip = NULL;
		cpu_stack_trace->duration[index] = (now - last)/1000000;;
	}
}

static struct noschedule_info nosched_info = {
	.tp_entries = {
		[0] = {
			.name	= "sched_switch",
			.probe	= trace_sched_switch_hit,
		},
	},
	.tp_initalized = 0,
};		

static bool __stack_trace_record(struct per_cpu_stack_trace *stack_trace,
				 struct pt_regs *regs, int duration)
{
	unsigned int nr_entries, nr_stack_entries;
	struct stack_entry *stack_entry;

	nr_entries = stack_trace->nr_entries;
	if (nr_entries >= MAX_TRACE_ENTRIES)
		return false;

	nr_stack_entries = stack_trace->nr_stack_entries;
	if (nr_stack_entries >= MAX_STACE_TRACE_ENTRIES)
		return false;

	////strlcpy(stack_trace->curr_comms[nr_stack_entries], current->comm, (unsigned long)TASK_COMM_LEN);
	////strlcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm, (unsigned long)TASK_COMM_LEN);
	strcpy(stack_trace->curr_comms[nr_stack_entries], current->comm);
	stack_trace->curr_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;
	strcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm);
	stack_trace->parent_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;

	stack_trace->pids[nr_stack_entries] = current->pid;
	stack_trace->duration[nr_stack_entries] = duration;
	stack_trace->prio[nr_stack_entries] = current->prio;
	stack_trace->timestamp[nr_stack_entries] = sched_clock();

	stack_entry = stack_trace->stack_entries + nr_stack_entries;
	xm_store_stack_trace(regs, stack_entry, stack_trace->entries + nr_entries, MAX_TRACE_ENTRIES - nr_entries, 0);
	stack_trace->nr_entries += stack_entry->nr_entries;

	smp_store_release(&stack_trace->nr_stack_entries, nr_stack_entries + 1);

	if ((stack_trace->nr_entries >= MAX_TRACE_ENTRIES) || (stack_trace->nr_stack_entries > MAX_STACE_TRACE_ENTRIES)) {
		return false;
	}

	return true;
}

static inline bool normal_thread(struct task_struct* p)
{
	return p->policy == SCHED_NORMAL || p->policy == SCHED_BATCH;
}

static inline bool stack_trace_record(struct per_cpu_stack_trace *stack_trace,
				      int delta)
{
	if (unlikely(delta >= preemptoff_settings.threshold_ms) && normal_thread(current))
		return __stack_trace_record(stack_trace, xm_get_irq_regs(), delta);

	return false;
}

////void preemptoff_nosched_timer(void)
static enum hrtimer_restart trace_nosched_hrtimer_handler(struct hrtimer *hrtimer)
{
	struct pt_regs *regs = xm_get_irq_regs();
	struct per_cpu_stack_trace *stack_trace = this_cpu_ptr(nosched_info.stack_trace);
	u64 now = local_clock();

	if (!is_idle_task(current) && regs && !user_mode(regs) && !xm_single_task_running()) {
		u64 delta;

		delta = now - stack_trace->last_timestamp;
		if (!stack_trace->skip && stack_trace_record(stack_trace, (int)(delta/1000000)))
			stack_trace->skip = current;  
	} else {
	    if (stack_trace->skip == NULL)
		    stack_trace->last_timestamp = now;
	}

	hrtimer_forward_now(hrtimer, ns_to_ktime(sampling_period));

	return HRTIMER_RESTART;		
}

static inline bool is_tracepoint_lookup_success(struct noschedule_info *info)
{
	return info->tp_initalized == ARRAY_SIZE(info->tp_entries);
}

static void tracepoint_lookup(struct tracepoint *tp, void *priv)
{
	int i;
	struct noschedule_info *info = priv;

	if (is_tracepoint_lookup_success(info))
		return;

	for (i = 0; i < ARRAY_SIZE(info->tp_entries); i++) {
		if (info->tp_entries[i].tp || !info->tp_entries[i].name ||
		    strcmp(tp->name, info->tp_entries[i].name))
			continue;
		info->tp_entries[i].tp = tp;
		info->tp_initalized++;
	}
}

static int sampling_period_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%llu ms\n", sampling_period/1000000);

	return 0;
}

static ssize_t sampling_period_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	sampling_period = (val* 1000000);

	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(sampling_period);

static int threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", preemptoff_settings.threshold_ms);

	return 0;
}

static ssize_t threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	preemptoff_settings.threshold_ms = val;

	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(threshold);

static int enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%s\n", trace_enable ? "enabled" : "disabled");

	return 0;
}

static void each_hrtimer_start(void *priv)
{
#if 0
	u64 now = local_clock();
	struct per_cpu_stack_trace __percpu *stack_trace = priv;

	__this_cpu_write(stack_trace->last_timestamp, now);
#else
    u64 now = local_clock();
	struct per_cpu_stack_trace __percpu *stack_trace = priv;
	struct hrtimer *hrtimer = this_cpu_ptr(&stack_trace->hrtimer);

	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_PINNED);
	hrtimer->function = trace_nosched_hrtimer_handler;

	__this_cpu_write(stack_trace->last_timestamp, now);

	hrtimer_start_range_ns(hrtimer, ns_to_ktime(sampling_period), 0,
			       HRTIMER_MODE_REL_PINNED);
#endif
}

static inline void trace_nosched_hrtimer_start(void)
{
	on_each_cpu(each_hrtimer_start, nosched_info.stack_trace, true);
	////preemptoff_settings.activated = 1;
}

static inline void trace_nosched_hrtimer_cancel(void)
{
	////preemptoff_settings.activated = 0;
	int cpu;

	for_each_online_cpu(cpu)
		hrtimer_cancel(per_cpu_ptr(&nosched_info.stack_trace->hrtimer, cpu));	
}

static int trace_nosched_register_tp(void)
{
	int i;
	struct noschedule_info *info = &nosched_info;

	for (i = 0; i < ARRAY_SIZE(info->tp_entries); i++) {
		int ret;
		struct tracepoint_entry *entry = info->tp_entries + i;

		ret = tracepoint_probe_register(entry->tp, entry->probe,
						info->stack_trace);
		if (ret && ret != -EEXIST) {
			pr_err("sched trace: can not activate tracepoint "
			       "probe to %s with error code: %d\n",
			       entry->name, ret);
			while (i--) {
				entry = info->tp_entries + i;
				tracepoint_probe_unregister(entry->tp,
							    entry->probe,
							    info->stack_trace);
			}
			return ret;
		}
	}

	return 0;
}

static int trace_nosched_unregister_tp(void)
{
	int i;
	struct noschedule_info *info = &nosched_info;

	for (i = 0; i < ARRAY_SIZE(info->tp_entries); i++) {
		int ret;

		ret = tracepoint_probe_unregister(info->tp_entries[i].tp,
						  info->tp_entries[i].probe,
						  info->stack_trace);
		if (ret && ret != -ENOENT) {
			pr_err("sched trace: can not inactivate tracepoint "
			       "probe to %s with error code: %d\n",
			       info->tp_entries[i].name, ret);
			return ret;
		}
	}

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
		if (!trace_nosched_register_tp())
			trace_nosched_hrtimer_start();
		else
			return -EAGAIN;
	} else {
		trace_nosched_hrtimer_cancel();
		if (trace_nosched_unregister_tp())
			return -EAGAIN;
	}

	trace_enable = enable;

	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(enable);

static inline void seq_print_stack_trace(struct seq_file *m,
					 struct stack_entry *entry)
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
	struct per_cpu_stack_trace __percpu *stack_trace = m->private;

	for_each_online_cpu(cpu) {
		int i;
		unsigned int nr;
		struct per_cpu_stack_trace *cpu_stack_trace;

		cpu_stack_trace = per_cpu_ptr(stack_trace, cpu);
		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);
		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d prio:%d duration:%dms timestamp:%llu \n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->prio[i],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}

	return 0;
}

static void each_stack_trace_clear(void *priv)
{
	struct per_cpu_stack_trace __percpu *stack_trace = priv;
	struct per_cpu_stack_trace *cpu_stack_trace = this_cpu_ptr(stack_trace);

	cpu_stack_trace->nr_entries = 0;
	cpu_stack_trace->nr_stack_entries = 0;
}

static ssize_t stack_trace_store(void *priv, const char __user *buf,
				 size_t count)
{
	int clear;

	if (kstrtoint_from_user(buf, count, 10, &clear) || clear != 0)
		return -EINVAL;

	on_each_cpu(each_stack_trace_clear, priv, true);

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(stack_trace);


int xiaomi_trace_preemptoff_init(void)
{
	struct proc_dir_entry *parent_dir;
	struct noschedule_info *info = &nosched_info;

	for_each_kernel_tracepoint(tracepoint_lookup, info);

	if (!is_tracepoint_lookup_success(info))
		return -ENODEV;

	info->stack_trace = alloc_percpu(struct per_cpu_stack_trace);
	if (!info->stack_trace)
		return -ENOMEM;

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_PREEMPT_NAME, NULL);
	if (!parent_dir)
		goto free_buf;
	if (!proc_create_data("threshold", 0666, parent_dir, &threshold_fops,
			      info->stack_trace)) 
		goto remove_proc;			      
	if (!proc_create_data("sampling_period", 0666, parent_dir, &sampling_period_fops,
			      info->stack_trace)) 			      
		goto remove_proc;
	if (!proc_create_data("enable", 0666, parent_dir, &enable_fops,
			      info->stack_trace))
		goto remove_proc; 

	if (!proc_create_data("stack_trace", 0, parent_dir, &stack_trace_fops,
			      info->stack_trace))
		goto remove_proc;

	if (trace_enable) {
		if (!trace_nosched_register_tp())
			trace_nosched_hrtimer_start();
	}

	return 0;
remove_proc:
	remove_proc_subtree(PROC_TRACE_PREEMPT_NAME, NULL);
free_buf:
	free_percpu(info->stack_trace);

	return -ENOMEM;
}

void  xiaomi_trace_preemptoff_exit(void)
{
	if (trace_enable) {
		trace_nosched_hrtimer_cancel();
		trace_nosched_unregister_tp();
		tracepoint_synchronize_unregister();
	}
	remove_proc_subtree(PROC_TRACE_PREEMPT_NAME, NULL);
	free_percpu(nosched_info.stack_trace);
}
