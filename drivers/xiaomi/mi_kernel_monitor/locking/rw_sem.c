

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
#include <linux/rbtree.h>
#include <linux/cpu.h>
#include <linux/syscalls.h>
#include <linux/mutex.h>
#include <linux/rmap.h>
#include <linux/version.h>
#include <linux/sched/clock.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/rwsem.h>
#include "mm_tree.h"

#include "internal.h"
#include "pub/trace_point.h"
#include "pub/kprobe.h"

#include "uapi/rw_sem.h"

#define MI_RWSEM_WRITER_LOCKED	(1UL << 0)

////static atomic64_t xm_nr_running = ATOMIC64_INIT(0);

struct xm_rw_sem_settings rw_sem_settings = {
	.threshold = 50,
};

static bool trace_enable = false;
static struct xm_stack_trace __percpu *p_rwsem_stack_trace;

////static struct kprobe kprobe_down_write;
////static struct kprobe kprobe_down_write_killable;
////static struct kprobe kprobe_up_write;
////static struct kprobe kprobe_try_to_migrate;
////static struct kprobe kprobe_try_to_unmap;


////static struct kmem_cache *rwsem_desc_cache;
////static struct hrtimer rwsem_timeout_hrtimer;
////static u64 rwsem_timeout_detect_period = 10 * 1000 * 1000 * 1000UL;
////#define    RWSEM_TIMEOUT_DURATION       (20*1000*1000*1000UL)
static struct xm_stack_trace_task s_rwsem_held_stack_trace = {0};


struct rw_sem_dest {
	struct rw_semaphore *rw_sem;
	u64 lock_time;
	struct list_head list;
};

static struct mm_tree mm_tree;

__maybe_unused static struct radix_tree_root rw_sem_tree;
__maybe_unused static DEFINE_SPINLOCK(tree_lock);
////static LIST_HEAD(rw_sem_list);
////static DEFINE_MUTEX(mutex_mutex);

static inline bool stack_trace_record_task(struct task_struct*tsk, struct xm_stack_trace_task *stack_trace, int delta)
{
	////if (unlikely(delta >= cpu_util_threshold))
		return xm_stack_trace_record_for_task(tsk, stack_trace, xm_get_irq_regs(), delta);

	return false;
}

static bool xm_stack_trace_record_for_rwsem(struct task_struct* tsk, struct xm_stack_trace *stack_trace, struct pt_regs *regs, int duration)
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

	if ((stack_trace->nr_entries >= MAX_TRACE_ENTRIES)  || (stack_trace->nr_stack_entries > MAX_STACE_TRACE_ENTRIES)) {
		pr_info("BUG: xiaomi rwsem MAX_TRACE_ENTRIES too low cpu=%d, nr_entries=%u, nr_stack_entries=%u\n", smp_processor_id(), stack_trace->nr_entries, stack_trace->nr_stack_entries);
		
		return false;
	}  

	return true;  
}

static inline bool stack_trace_record(struct xm_stack_trace *stack_trace, int delta)
{
	if (unlikely(delta >= rw_sem_settings.threshold))
		return xm_stack_trace_record_for_rwsem(current, stack_trace, xm_get_irq_regs(), delta);

	return false;
}

void mi_record_rwsem_start(void *ignore, struct rw_semaphore *sem, unsigned long settime_jiffies)
{
}
static int __activate_rw_sem(void)
{				
    register_trace_android_vh_record_rwsem_lock_starttime(mi_record_rwsem_start, NULL);
    ////register_trace_android_vh_rwsem_write_finished(mi_rwsem_write_finish, NULL);				

    return 1;
}

static void __deactivate_rw_sem(void)
{
    unregister_trace_android_vh_record_rwsem_lock_starttime(mi_record_rwsem_start, NULL);
    ////unregister_trace_android_vh_rwsem_write_finished(mi_rwsem_write_finish, NULL);
}

int activate_rw_sem(void)
{
    if (!rw_sem_settings.activated)
        rw_sem_settings.activated = __activate_rw_sem();

    return rw_sem_settings.activated;
}

int deactivate_rw_sem(void)
{
	if (rw_sem_settings.activated)
		__deactivate_rw_sem();
	rw_sem_settings.activated = 0;

	return 0;
}

static int threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", rw_sem_settings.threshold);

	return 0;
}

static ssize_t threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	rw_sem_settings.threshold = val;

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
		activate_rw_sem();
	} else {
	    deactivate_rw_sem();
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

static inline void seq_print_stack_trace_task(struct seq_file *m, struct stack_entry_task *entry)
{
	int i;

	for (i = 0; i < entry->nr_entries; i++)
		seq_printf(m, "%*c%pS\n", 5, ' ', (void *)entry->entries[i]);
}

static int stack_trace_show(struct seq_file *m, void *ptr)
{
	int cpu;

	////for_each_online_cpu(cpu) 
	{
		struct xm_stack_trace_task *cpu_stack_trace = &s_rwsem_held_stack_trace;
		int i;
		unsigned int nr = 0;
		nr = smp_load_acquire(&cpu_stack_trace->nr_tasks);
		if (nr > 0)
		{

		    seq_printf(m, "Rwsem not release backtrace:\n");

		    for (i = 0; i < nr; i++) 
			{
			    struct stack_entry_task *entry;

			    entry = cpu_stack_trace->stack_entries + i;
			    seq_printf(m, "%*cthread:%s process:%s pid:%d timeout:%ds timestamp:%llu\n",
				       5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				       cpu_stack_trace->pids[i],
				       cpu_stack_trace->duration[i],
				       cpu_stack_trace->timestamp[i]);
			     seq_print_stack_trace_task(m, entry);
			     seq_putc(m, '\n');

			    cond_resched();
		    }
		}
	}

	for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_rwsem_stack_trace, cpu);
		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);
		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d duration:%d ms timestamp:%llu\n",
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
DEFINE_PROC_ATTRIBUTE_RW(enable);
DEFINE_PROC_ATTRIBUTE_RW(stack_trace);

int xiaomi_rw_sem_init(void)
{
	struct proc_dir_entry *parent_dir;

	init_mm_tree(&mm_tree);

	if (rw_sem_settings.activated)
		rw_sem_settings.activated = __activate_rw_sem();
		
	////rwsem_desc_cache = kmem_cache_create("trace_rwsem_desc_cache", sizeof(struct rw_sem_dest), 0, 0, NULL);
	p_rwsem_stack_trace = alloc_percpu(struct xm_stack_trace);
	if (!p_rwsem_stack_trace)
		return -ENOMEM;

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_RWSEM, NULL);

	//parent_dir = proc_mkdir(PROC_DIR_NAME, NULL);
	if (!parent_dir)
		goto free_buf;
	if (!proc_create_data("threshold", 0666, parent_dir, &threshold_fops, NULL))
		goto remove_proc;
		
	if (!proc_create_data("enable", 0666, parent_dir, &enable_fops, NULL))
		goto remove_proc;

	if (!proc_create_data("stack_trace", 0, parent_dir, &stack_trace_fops, NULL))
		goto remove_proc;
	
	return 0;
		
	remove_proc:
		remove_proc_subtree(PROC_TRACE_RWSEM, NULL);
	free_buf:
		return -ENOMEM;
}

void xiaomi_rw_sem_exit(void)
{
	if (rw_sem_settings.activated)
		deactivate_rw_sem();
	rw_sem_settings.activated = 0;
	remove_proc_subtree(PROC_TRACE_RWSEM, NULL);
	free_percpu(p_rwsem_stack_trace);
	////kmem_cache_destroy(rwsem_desc_cache);
}
