#include <linux/syscalls.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cred.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timex.h>
#include <linux/tracepoint.h>

#include <trace/events/napi.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sysctl.h>
#include <linux/semaphore.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>
#include <linux/cpufreq.h>
#include <asm-generic/irq_regs.h>

#include <linux/file.h>
#include <linux/seq_file.h>
#include <linux/cgroup.h>
#include <linux/mmzone.h>
#include <linux/jiffies.h>
#include <linux/sched/cputime.h>
#include <linux/mi_kernel_monitor.h>
#include <trace/events/sched.h>
#include <trace/events/power.h>
#include <trace/events/cma.h>
#include <trace/events/vmscan.h>
#include <trace/events/kmem.h>
#include <trace/events/lock.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/sched.h>

#include <trace/hooks/cgroup.h>
#include <trace/hooks/dtask.h>

#include "event.h"
#include "sched/wait_base.h"
#include "pub/trace_point.h"
#include "pub/kprobe.h"
#include "internal.h"
#include  "traces/kernel_monitor.h"
#include "../../../drivers/mihw/include/mi_module.h"

#define  RQ_ENQUEUE_BEFORE_SWITCH_FLAG   10000000000000ULL
#define  DROP_FRAME_INTERNAL   (300000000ULL)

static struct xm_stack_trace __percpu *p_lock_stack_trace;
static struct xm_stack_trace __percpu *p_irqoff_stack_trace;
static struct xm_stack_trace __percpu *p_preemptoff_stack_trace;
static struct xm_stack_trace __percpu *p_rt_stack_trace;
static struct xm_stack_trace_task __percpu *p_iowait_stack_trace;

static struct proc_dir_entry *event_dir;
static bool event_trace_enable = 0;
static bool stack_save_enable = 1;

static int pid_surfaceflinger = 0;
static int pid_ui = 0;
static int pid_render = 0;
static int pid_home_ui = 0;
static int pid_home_render = 0;
static int pid_systemui_ui = 0;
static int pid_systemui_render = 0;

static DEFINE_PER_CPU(u64, enqueue_timestamp);
static struct FRAMEMISS_STAT_INFO s_framemiss_stat_info = {0};
static char *type_name[LAT_TYPE_MAX] = {"mutex", "rwsem_read", "rwsem_write", "futex_wait",
					"sched_wait", "irqoff", "rt_running", "preemptoff",
					"iowait", "dstate", "binder"};
static char *grp_name[5] = {"VIP", "RT", "OTHER", "TOPAPP",};										

extern int test_task_top_app(struct task_struct *task);
extern int get_task_grp_idx(struct task_struct *tsk);
bool is_critical_ux_task(struct task_struct* task)
{
	if(!event_trace_enable || task->pid == 0)
	{
        return false;
	}

    if (task->flags & PF_KTHREAD)
    {
        return false;
    }

#ifdef DEBUG_MI_EVENT
    if (is_mi_vip_task(task))
    {
        return true;
    }
#endif

	if (task->pid == pid_surfaceflinger || task->pid == pid_ui
		|| task->pid == pid_render || task->pid == pid_home_ui || task->pid == pid_home_render
		|| task->pid == pid_systemui_ui || task->pid == pid_systemui_render)
	{
        return true;
	}

    return false;
}

bool is_screen_off(void)
{
	if (0 == pid_surfaceflinger)
	{
#ifdef DEBUG_MI_EVENT
        return false;
#else
        return true;
#endif
	}

    return false;
}

extern bool in_coldstart(void);
static inline bool multi_task_running_rt(u64 delta, u64 enqueue_tm, u64 switch_time, u64 clock, struct rq* rq, u64 delay, struct task_struct* next)
{	
	int curr_uid = current_uid().val;

	if (next->pid == 0)
	{
		return false;
	}

	curr_uid = curr_uid % PER_USER_RANGE;
	if ((curr_uid >= FIRST_APPLICATION_UID) && (curr_uid <= LAST_APPLICATION_UID))
	{
		return false;
	}

	return true;
}

static inline bool multi_task_running(u64 delta, u64 enqueue_tm, u64 switch_time, u64 clock, struct rq* rq, u64 delay, struct task_struct* next)
{
	u64 gap = 0;

	if (next->pid == 0)
	{
		return false;
    }

	gap = delay > delta ? delay - delta : 0;
	if (delay > delta && gap >= PREEMPT_OFF_LAT_THRESHOLD)
    {
		return true;
    }

	if (delta == RQ_ENQUEUE_BEFORE_SWITCH_FLAG)
    {
		return true;
    }

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void record_kern_lat_event(u8 type, int delay, struct task_struct *task)
{
	u64 timestamp = 0;

	if(!event_trace_enable)
	{
        return;
	}

	timestamp = sched_clock();

	switch (type)
	{
    case MUTEX_WAIT_LAT:
	    trace_task_mutex_wait_long(task, delay, task_cpu(task));
	    break;

    case RWSEM_READ_WAIT_LAT:
        trace_task_rwsem_read_wait_long(task, delay, task_cpu(task));
        break;

    case RWSEM_WRITE_WAIT_LAT:
        trace_task_rwsem_write_wait_long(task, delay, task_cpu(task));	
        break;

    case FUTEX_WAIT_LAT:
        trace_task_futex_wait_long(task, delay, task_cpu(task));	
        break;

    case SCHED_LAT:
        trace_task_sched_lat_long(current, task, delay, task_cpu(task));	
        break;

    case IRQ_OFF_LAT:
        trace_task_irqoff_long(task, delay, task_cpu(task));	
        break;

    case RT_RUNNING_LAT:
	    trace_task_rt_running_long(task, delay, task_cpu(task));
        break;

    case PREEMPT_OFF_LAT:
        trace_task_preemptoff_long(task, delay, task_cpu(task));	
        break;

    case IO_WAIT_LAT:
        trace_task_iowait_long(task, delay, task_cpu(task));
        break;

    case DSTATE_LAT:
        break;

    case BINDER_LAT:
        trace_task_binder_lat_long(task, delay, task_cpu(task));
        break;

	default:
	    break;
    }

}

static bool xm_stack_trace_record_event(u8 grp_type, u8 type, struct task_struct* tsk, struct xm_stack_trace *stack_trace, struct pt_regs *regs, int duration)
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

	strcpy(stack_trace->curr_comms[nr_stack_entries], tsk->comm);
	stack_trace->curr_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;
	strcpy(stack_trace->parent_comms[nr_stack_entries], tsk->group_leader->comm);
	stack_trace->parent_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;

	stack_trace->pids[nr_stack_entries] = tsk->pid;
	stack_trace->prio[nr_stack_entries] = tsk->prio;
	stack_trace->duration[nr_stack_entries] = duration;
	stack_trace->timestamp[nr_stack_entries] = sched_clock();
	stack_trace->event_type[nr_stack_entries] = type;
	stack_trace->grp_type[nr_stack_entries] = grp_type;

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

static bool xm_stack_trace_record_event_task(u8 grp_type, u8 type, struct task_struct* tsk, struct xm_stack_trace_task *stack_trace, struct pt_regs *regs, int duration)
{
	unsigned int nr_stack_entries;

	nr_stack_entries = stack_trace->nr_tasks;

	if (nr_stack_entries >= MAX_STACE_TRACE_ENTRIES)
		return false;

	////strlcpy(stack_trace->curr_comms[nr_stack_entries], tsk->comm, (unsigned long)TASK_COMM_LEN);
	////strlcpy(stack_trace->parent_comms[nr_stack_entries], tsk->group_leader->comm, (unsigned long)TASK_COMM_LEN);
	strcpy(stack_trace->curr_comms[nr_stack_entries], tsk->comm);
	stack_trace->curr_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;
	strcpy(stack_trace->parent_comms[nr_stack_entries], tsk->group_leader->comm);
	stack_trace->parent_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;

	stack_trace->pids[nr_stack_entries] = tsk->pid;
	stack_trace->duration[nr_stack_entries] = duration;
	stack_trace->timestamp[nr_stack_entries] = sched_clock();
	stack_trace->event_type[nr_stack_entries] = type;
	stack_trace->grp_type[nr_stack_entries] = grp_type;

	////stack_entry = stack_trace->stack_entries + nr_stack_entries;
	////stack_entry->entries = stack_trace->entries + nr_entries;
	stack_trace->stack_entries[nr_stack_entries].nr_entries = xiaomi_save_stack_trace(tsk, stack_trace->stack_entries[nr_stack_entries].entries);
	if ((stack_trace->stack_entries[nr_stack_entries].nr_entries >= BACKTRACE_DEPTH)) {
		printk("BUG: xiaomi cpu_util MAX_TRACE_ENTRIES too low nr_entries=%u\n",  stack_trace->stack_entries[nr_stack_entries].nr_entries);

		return false;
	}	
	///stack_trace->nr_entries += stack_entry->nr_entries;

	smp_store_release(&stack_trace->nr_tasks, nr_stack_entries + 1);

	return true;
}

bool stack_trace_record_event(u8 grp_type, u8 event_type, int delta, struct task_struct *task)
{
    if (!event_trace_enable || !stack_save_enable)
	{
		return false;
	}

	switch(event_type)
	{
	case MUTEX_WAIT_LAT:
	case RWSEM_READ_WAIT_LAT:
	case RWSEM_WRITE_WAIT_LAT:
	case FUTEX_WAIT_LAT:
	{
	    struct xm_stack_trace *stack_trace = this_cpu_ptr(p_lock_stack_trace);
	    return xm_stack_trace_record_event(grp_type, event_type, task, stack_trace, xm_get_irq_regs(), delta);
		break;
	}

	case RT_RUNNING_LAT:
	{
	    struct xm_stack_trace *stack_trace = this_cpu_ptr(p_rt_stack_trace);
	    return xm_stack_trace_record_event(grp_type, event_type, task, stack_trace, xm_get_irq_regs(), delta);
		break;
	}

	case PREEMPT_OFF_LAT:
	{
	    struct xm_stack_trace *stack_trace = this_cpu_ptr(p_preemptoff_stack_trace);
	    return xm_stack_trace_record_event(grp_type, event_type, task, stack_trace, xm_get_irq_regs(), delta);
		break;
	}
	
	case IRQ_OFF_LAT:
	{
	    struct xm_stack_trace *stack_trace = this_cpu_ptr(p_irqoff_stack_trace);
	    return xm_stack_trace_record_event(grp_type, event_type, task, stack_trace, xm_get_irq_regs(), delta);
		break;
	}

	case IO_WAIT_LAT:
	case DSTATE_LAT:
	{
	    struct xm_stack_trace_task *stack_trace = this_cpu_ptr(p_iowait_stack_trace);
	    return xm_stack_trace_record_event_task(grp_type, event_type, task, stack_trace, xm_get_irq_regs(), delta);
		break;
	}

	case SCHED_LAT:
	    break;

	default:
	    break;
	}

	return false;
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

static int lock_stack_show(struct seq_file *m, void *ptr)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_lock_stack_trace, cpu);
		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);

		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d prio:%d type:%s grp:%s duration:%dms timestamp:%llu\n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->prio[i],
				   type_name[cpu_stack_trace->event_type[i]],
				   grp_name[cpu_stack_trace->grp_type[i]],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}
	
	return 0;
}

static ssize_t lock_stack_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(lock_stack);

static int irqoff_stack_show(struct seq_file *m, void *ptr)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_irqoff_stack_trace, cpu);
		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);

		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d prio:%d type:%s grp:%s duration:%dms timestamp:%llu\n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->prio[i],
				   type_name[cpu_stack_trace->event_type[i]],
				   grp_name[cpu_stack_trace->grp_type[i]],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}

	return 0;
}

static ssize_t irqoff_stack_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(irqoff_stack);

static int preemptoff_stack_show(struct seq_file *m, void *ptr)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_preemptoff_stack_trace, cpu);
		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);

		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d prio:%d type:%s grp:%s duration:%dms timestamp:%llu\n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->prio[i],
				   type_name[cpu_stack_trace->event_type[i]],
				   grp_name[cpu_stack_trace->grp_type[i]],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}

	return 0;
}

static ssize_t preemptoff_stack_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(preemptoff_stack);

static int rt_stack_show(struct seq_file *m, void *ptr)
{
	int cpu;

	for_each_online_cpu(cpu) 
	{
		int i;
		unsigned int nr;
		struct xm_stack_trace *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_rt_stack_trace, cpu);
		nr = smp_load_acquire(&cpu_stack_trace->nr_stack_entries);

		if (!nr)
			continue;

		seq_printf(m, " cpu: %d\n", cpu);

		for (i = 0; i < nr; i++) {
			struct stack_entry *entry;

			entry = cpu_stack_trace->stack_entries + i;
			seq_printf(m, "%*cthread:%s process:%s pid:%d prio:%d type:%s grp:%s duration:%dms timestamp:%llu\n",
				   5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				   cpu_stack_trace->pids[i],
				   cpu_stack_trace->prio[i],
				   type_name[cpu_stack_trace->event_type[i]],
				   grp_name[cpu_stack_trace->grp_type[i]],
				   cpu_stack_trace->duration[i],
				   cpu_stack_trace->timestamp[i]);
			seq_print_stack_trace(m, entry);
			seq_putc(m, '\n');

			cond_resched();
		}
	}

	return 0;
}

static ssize_t rt_stack_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(rt_stack);

static int iowait_stack_show(struct seq_file *m, void *ptr)
{
	int cpu;
	int i;
	unsigned int nr;

	for_each_online_cpu(cpu)
	{
	    struct xm_stack_trace_task *cpu_stack_trace;
		cpu_stack_trace = per_cpu_ptr(p_iowait_stack_trace, cpu);

		nr = smp_load_acquire(&cpu_stack_trace->nr_tasks);
		if (nr > 0)
		{
		    for (i = 0; i < nr; i++) {
			    struct stack_entry_task *entry;

			    entry = cpu_stack_trace->stack_entries + i;
			    seq_printf(m, "%*cthread:%s process:%s pid:%d type:%u grp_type:%u timeout:%dms timestamp:%llu\n",
				       5, ' ', cpu_stack_trace->curr_comms[i], cpu_stack_trace->parent_comms[i],
				       cpu_stack_trace->pids[i],
				       cpu_stack_trace->event_type[i],
				       cpu_stack_trace->grp_type[i],					   
				       cpu_stack_trace->duration[i],
				       cpu_stack_trace->timestamp[i]);
			    seq_print_stack_trace_task(m, entry);
			    seq_putc(m, '\n');

			    cond_resched();
		    }
		}
	}

	return 0;
}

static ssize_t iowait_stack_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(iowait_stack);

//CPU/PID/TIMESTAMP/EVETN_TYPE/DELAY
static void trace_sched_wakeup_hit(void *priv, struct task_struct *task)
{
	struct rq *rq = task_rq(task);
	u64 clock = rq_clock(rq);
	u64 delay = 0;
	struct mi_kernel_monitor *km_ts = NULL;
	int cpu =  task_cpu(task);

	km_ts = get_mi_kernel_monitor(task);
	if (unlikely(IS_ERR_OR_NULL(km_ts))) {
		return;
	}

	if (is_critical_ux_task(task)) {
	        ////calculate iowait and dd delay
        	if (km_ts->sleep_tm != 0) {
			delay = clock - km_ts->sleep_tm;
		}

        	trace_task_wokenup(task, current, delay/1000000, cpu);
	}

	km_ts->sched_switch_tm = clock;
	km_ts->sleep_tm = 0;

#ifndef USE_AFTER_ENQUEUE_HOOK
	if (!xm_single_task_running_by_cpu(cpu)) {
	        per_cpu(enqueue_timestamp, cpu) = clock;
	}
#endif
}

static void trace_sched_wakeup_new_hit(void *priv, struct task_struct *task)
{
	struct rq *rq = task_rq(task);
	u64 clock = rq_clock(rq);
	u64 delay = 0;
	struct mi_kernel_monitor *km_ts = NULL;
	int cpu = task_cpu(task);

	km_ts = get_mi_kernel_monitor(task);
	if (unlikely(IS_ERR_OR_NULL(km_ts))) {
		return;
	}

	km_ts->sched_switch_tm = clock;
	km_ts->sleep_tm = 0;

	if (is_critical_ux_task(task)) {
		trace_task_wokenup_new(task, current, delay/1000000, cpu);
	}

#ifndef USE_AFTER_ENQUEUE_HOOK
	if (!xm_single_task_running_by_cpu(cpu)) {
	        per_cpu(enqueue_timestamp, cpu) = clock;
	}
#endif
}

#define MI_TASK_SCHED_CLOCK_SET	0xabcdabcdacbdadbd

static void handle_cnt_stats(struct task_struct *prev, struct task_struct *next, int cpu)
{
	struct mi_kernel_monitor *km_ts_prev = NULL;
	struct mi_kernel_monitor *km_ts_next = NULL;
	struct rq *rq = task_rq(prev);
	u64 clock = rq_clock(rq);
	u64 delay = 0;
	unsigned int prev_state = 0;
	int state = 0;

	u64 enqueue_delta = 0;
	u64 temp = 0;

	km_ts_prev = get_mi_kernel_monitor(prev);
	km_ts_next = get_mi_kernel_monitor(next);

	if (!(IS_ERR_OR_NULL(km_ts_prev))) {
        	if (km_ts_prev->sched_switch_tm != 0) {
           		delay = clock - km_ts_prev->sched_switch_tm;  //running time
			if (per_cpu(enqueue_timestamp, task_cpu(prev)) >= km_ts_prev->sched_switch_tm) {
        			enqueue_delta =
					(per_cpu(enqueue_timestamp, task_cpu(prev)) - km_ts_prev->sched_switch_tm);
			} else {
        			enqueue_delta = RQ_ENQUEUE_BEFORE_SWITCH_FLAG;
			}

			temp = km_ts_prev->sched_switch_tm;
        	}
		km_ts_prev->sched_switch_tm = clock;
		km_ts_prev->sleep_tm = clock;
	}

	if (delay >= PREEMPT_OFF_LAT_THRESHOLD) {
		struct pt_regs *regs = task_pt_regs(prev);

        	if (prev->prio < MAX_RT_PRIO) {
			record_kern_lat_event(RT_RUNNING_LAT, delay/1000000, prev);			
			if (regs && !user_mode(regs) &&
			    multi_task_running_rt(enqueue_delta,
			    per_cpu(enqueue_timestamp, task_cpu(prev)),
			    temp, clock, rq, delay, next)) {
				stack_trace_record_event(0, RT_RUNNING_LAT, delay/1000000, prev);
		    	}
        	}
                /*
		 * else if (prev->prio >= MAX_RT_PRIO &&
		 *          regs && !user_mode(regs) &&
		 *          !xm_single_task_running())
		 * if (!xm_single_task_running())
		 */
		else if (!xm_single_task_running() && !is_screen_off()) {
			if (regs && !user_mode(regs) &&
			    multi_task_running(enqueue_delta,
			    per_cpu(enqueue_timestamp, task_cpu(prev)),
			    temp, clock, rq, delay, next)) {
				record_kern_lat_event(PREEMPT_OFF_LAT, delay/1000000, prev);
				stack_trace_record_event(0, PREEMPT_OFF_LAT, delay/1000000, prev);
			}
		}
	}

	prev_state = READ_ONCE(prev->__state);
	if ((prev_state & TASK_UNINTERRUPTIBLE) || (prev_state & TASK_INTERRUPTIBLE) || (prev_state == TASK_RUNNING)) {
		if (prev_state == TASK_RUNNING) {
			state = TASK_RUNNING;
			if (!(IS_ERR_OR_NULL(km_ts_prev))) {
			    km_ts_prev->sleep_tm = 0;     //iowait
			}
		} else if (prev_state & TASK_INTERRUPTIBLE) {
			state = TASK_INTERRUPTIBLE;
		} else if (prev->in_iowait) {
			state = TASK_IOWAIT;
		} else if (prev_state & TASK_UNINTERRUPTIBLE) {
			state = TASK_UNINTERRUPTIBLE;
		} else {
			state = TASK_OTHER;
		}

		if (is_critical_ux_task(prev)) {
            		trace_task_schedout(prev, next, state, delay/1000000, task_cpu(prev), next->prio);
		}
	}

	if (!(IS_ERR_OR_NULL(km_ts_next))) {
		if (km_ts_next->sched_switch_tm) {
			//calculate for sched latency
			delay = clock - km_ts_next->sched_switch_tm;  
		}
		km_ts_next->sched_switch_tm = clock;
		km_ts_next->sleep_tm = 0;
	}

	if (is_critical_ux_task(next)) {
		trace_task_schedin(next, prev, delay/1000000, task_cpu(next));
	}
}

static void trace_sched_switch_hit(void *priv, bool preempt, struct task_struct *prev, struct task_struct *next, unsigned int pre_state)
{
	handle_cnt_stats(prev, next, 0);
}

#ifdef CONFIG_SPINLOCK_MONITOR
static void trace_contention_end_hit(void *priv, void *lock, int ret)
{
	struct mi_kernel_monitor *km_ts = NULL;
	km_ts = get_mi_kernel_monitor(current);

	if (!IS_ERR_OR_NULL(km_ts) && (ret == 0) && (km_ts->sched_reserved != 0))
	{
		u64 clock = local_clock()%1000000000;
		u64 delta = clock > km_ts->sched_reserved ? clock - km_ts->sched_reserved : 0;
		if (delta >= 8000000) {
			trace_printk("huqh_end: comm=%s,pid=%d,preempt_count=%d,clock=%llu,delta=%llu us,\n",
				current->comm, current->pid, preempt_count(), clock, delta/1000);
			stack_trace_record_event(0, IRQ_OFF_LAT, delta/1000000, current);
		}

		km_ts->sched_reserved = 0;
	}
}

static void trace_contention_begin_hit(void *priv, void *lock, unsigned int flags)
{
	struct mi_kernel_monitor *km_ts = NULL;
	km_ts = get_mi_kernel_monitor(current);

	if (!IS_ERR_OR_NULL(km_ts) && (flags == LCB_F_SPIN))
	{
		u64 clock = local_clock();
		km_ts->sched_reserved = clock%1000000000;
	}
}
#endif

#ifdef USE_AFTER_ENQUEUE_HOOK
static void trace_android_rvh_after_enqueue_task_hit(void *unused, struct rq *rq, struct task_struct *p, int flags)
{
	if (!xm_single_task_running()) {
		per_cpu(enqueue_timestamp, rq->cpu) = rq_clock(rq);
	}
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int activate_event_monitor(void)
{
	hook_tracepoint("sched_wakeup", trace_sched_wakeup_hit, NULL);
    hook_tracepoint("sched_wakeup_new", trace_sched_wakeup_new_hit, NULL);
    hook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);

#ifdef CONFIG_SPINLOCK_MONITOR
	hook_tracepoint("contention_begin", trace_contention_begin_hit, NULL);
	hook_tracepoint("contention_end", trace_contention_end_hit, NULL);
#endif

#ifdef USE_AFTER_ENQUEUE_HOOK
	register_trace_android_rvh_after_enqueue_task(trace_android_rvh_after_enqueue_task_hit, NULL);
#endif

	return 0;
}

int deactivate_event_monitor(void)
{
    unhook_tracepoint("sched_wakeup", trace_sched_wakeup_hit, NULL);
    unhook_tracepoint("sched_wakeup_new", trace_sched_wakeup_new_hit, NULL);
    unhook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);

#ifdef CONFIG_SPINLOCK_MONITOR
	unhook_tracepoint("contention_begin", trace_contention_begin_hit, NULL);
	unhook_tracepoint("contention_end", trace_contention_end_hit, NULL);
#endif

	return 0;
}

static int sched_lat_info_show(struct seq_file *m, void *v)
{
	return 0;
}

static int sched_lat_info_open(struct inode* inode, struct file* file)
{
    return single_open(file, sched_lat_info_show, inode->i_private);
}

static const struct proc_ops sched_lat_info_fops = {
	.proc_open		= sched_lat_info_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release		= single_release,
};

/*******************************event enable********************************/
static int frame_miss_show(struct seq_file *m, void *ptr)
{
	int i = 0;
	for (i = 0; i < s_framemiss_stat_info.count; i++)
	{
        seq_printf(m,"time:%llu, is_match:%d, type=%d\n",  s_framemiss_stat_info.timestamp[i], s_framemiss_stat_info.is_match[i], s_framemiss_stat_info.scene_types[i]);
	}

	return 0;
}

static ssize_t frame_miss_store(void *priv, const char __user *buf, size_t count)
{
	////int val;
    char chr[256] = {0};
	u64 type = 0;
	u64 timestamp = 0;
	int duration = 0;

	if (count >= 256)
		return -EINVAL;

	if (copy_from_user(chr, buf, count))
		return -EINVAL;

	printk("mi_kernel_drop:%s,comm=%s,pid=%d\n", chr, current->comm, current->pid);
	sscanf(chr, "%llu %llu %d", &type, &timestamp, &duration);

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(frame_miss);

static int event_trace_enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n",  (event_trace_enable == 1)?"Enabled":"Disabled");
	return 0;
}

static ssize_t event_trace_enable_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (!!val == !!event_trace_enable)
		return count;

	if (val) {
		event_trace_enable = 1;
        activate_event_monitor();		
	} 
	else {
		deactivate_event_monitor();
		event_trace_enable = 0;
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(event_trace_enable);

static int stack_save_enable_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n",  (stack_save_enable == 1)?"Enabled":"Disabled");
	return 0;
}

static ssize_t stack_save_enable_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (!!val == !!stack_save_enable)
		return count;

	if (val) {
		stack_save_enable = 1;	
	} 
	else {
		stack_save_enable = 0;
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(stack_save_enable);

static int set_pids_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"pid_surfaceflinger=%d, pid_ui=%d, pid_render=%d, pid_home_ui=%d, pid_home_render=%d, pid_systemui_ui=%d, pid_systemui_render=%d\n", 
		pid_surfaceflinger, pid_ui, pid_render, pid_home_ui, pid_home_render, pid_systemui_ui, pid_systemui_render);

	return 0;
}

static ssize_t set_pids_store(void *priv, const char __user *buf, size_t count)
{
	int ret = 0;
	char chr[256] = {0};

	if (count >= 256)
		return -EINVAL;

	if (copy_from_user(chr, buf, count))
		return -EINVAL;


	////printk("mi_kernel sscanf set_pids: %s\n", chr);		
	
    ret = sscanf(chr, "%d %d %d %d %d %d %d", &pid_surfaceflinger, &pid_ui, &pid_render, &pid_home_ui, &pid_home_render, &pid_systemui_ui, &pid_systemui_render);
	if (ret != 7)
	{
		printk("mi_kernel sscanf error: %s\n", chr);		
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(set_pids);

static int reset_show(struct seq_file *m, void *ptr)
{
	seq_printf(m,"%s\n", "Data Reset");
	return 0;
}

static ssize_t reset_store(void *priv, const char __user *buf, size_t count)
{
	int val;
	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	if (val) 
	{
	}

	return count;
}
DEFINE_PROC_ATTRIBUTE_RW(reset);

static int kern_event_sysfs_init(void)
{
	struct proc_dir_entry *proc_node;

	event_dir = xiaomi_proc_mkdir(PROC_TRACE_EVENT, NULL);
	if (!event_dir) {
		return -ENOMEM;
	}

	proc_node = proc_create("sched_lat_info", S_IRUGO, event_dir, &sched_lat_info_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

    proc_node = proc_create("lock_stack", S_IRUGO, event_dir, &lock_stack_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

    proc_node = proc_create("irqoff_stack", S_IRUGO, event_dir, &irqoff_stack_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

    proc_node = proc_create("preemptoff_stack", S_IRUGO, event_dir, &preemptoff_stack_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

    proc_node = proc_create("rt_stack", S_IRUGO, event_dir, &rt_stack_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

    proc_node = proc_create("iowait_stack", S_IRUGO, event_dir, &iowait_stack_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}	

	proc_node = proc_create("stack_save_enable", S_IRUGO | S_IWUGO, event_dir, &stack_save_enable_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

	proc_node = proc_create("enable", S_IRUGO | S_IWUGO, event_dir, &event_trace_enable_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}	

	proc_node = proc_create("frame_miss", S_IRUGO | S_IWUGO, event_dir, &frame_miss_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

	proc_node = proc_create("reset", S_IRUGO | S_IWUGO, event_dir, &reset_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

	proc_node = proc_create("set_pids", S_IRUGO | S_IWUGO, event_dir, &set_pids_fops);
	if (!proc_node) {
		remove_proc_subtree(PROC_TRACE_EVENT, NULL);
		return -ENOMEM;
	}

	pr_info("%s success\n", __func__);
	return 0;
}

int mi_task_event_init(void)
{
	p_iowait_stack_trace = alloc_percpu(struct xm_stack_trace_task);
	if (!p_iowait_stack_trace)
	{
		return -ENOMEM;
    }

    p_lock_stack_trace = alloc_percpu(struct xm_stack_trace);
	if (!p_lock_stack_trace)
	{
		free_percpu(p_iowait_stack_trace);
		return -ENOMEM;
    }

    p_irqoff_stack_trace = alloc_percpu(struct xm_stack_trace);
	if (!p_irqoff_stack_trace)
	{
		free_percpu(p_iowait_stack_trace);
		free_percpu(p_lock_stack_trace);
		return -ENOMEM;
    }

    p_preemptoff_stack_trace = alloc_percpu(struct xm_stack_trace);
	if (!p_preemptoff_stack_trace)
	{
		free_percpu(p_iowait_stack_trace);
		free_percpu(p_lock_stack_trace);
		free_percpu(p_irqoff_stack_trace);
		return -ENOMEM;
    }

    p_rt_stack_trace = alloc_percpu(struct xm_stack_trace);
	if (!p_rt_stack_trace)
	{
		free_percpu(p_iowait_stack_trace);
		free_percpu(p_lock_stack_trace);
		free_percpu(p_irqoff_stack_trace);
		free_percpu(p_preemptoff_stack_trace);
		return -ENOMEM;
    }
	
	if (event_trace_enable)
	{
        activate_event_monitor();
    }

	kern_event_sysfs_init();

	return 0;
}

int mi_task_event_exit(void)
{
	if (event_trace_enable)
	{
        deactivate_event_monitor();
		event_trace_enable = 0;
    }

	remove_proc_entry("lat_info", event_dir);
	remove_proc_entry("sched_lat_info", event_dir);	

	remove_proc_entry("lock_stack", event_dir);
	remove_proc_entry("irqoff_stack", event_dir);
	remove_proc_entry("preemptoff_stack", event_dir);
	remove_proc_entry("rt_stack", event_dir);	
	remove_proc_entry("iowait_stack", event_dir);

	remove_proc_entry("enable", event_dir);
	remove_proc_entry("stack_save_enable", event_dir);
	remove_proc_entry("frame_miss", event_dir);
	remove_proc_entry("reset", event_dir);
	remove_proc_entry("set_pids", event_dir);	
    remove_proc_subtree(PROC_TRACE_EVENT, NULL);

	free_percpu(p_lock_stack_trace);
	free_percpu(p_irqoff_stack_trace);
	free_percpu(p_preemptoff_stack_trace);
	free_percpu(p_rt_stack_trace);
	free_percpu(p_iowait_stack_trace);

	return 0;
}