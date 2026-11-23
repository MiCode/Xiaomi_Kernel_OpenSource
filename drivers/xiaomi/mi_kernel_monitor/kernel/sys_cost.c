

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
#include <linux/percpu_counter.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/context_tracking.h>
#include <linux/sort.h>

#include <asm-generic/irq_regs.h>
#include <asm/unistd.h>

//#include <asm/traps.h>

#include "internal.h"
#include "pub/trace_point.h"
#include "pub/kprobe.h"

#include "uapi/sys_cost.h"

static bool trace_enable = false;;
static int sys_cost_count_per_second = 180000;
static int sampling_period = 10;  //10s

struct delayed_work sys_cost_dump;

struct xm_sys_cost_settings sys_cost_settings;

struct SYS_COST_INFO {
    int cnt;
	int cpu[MAX_STACE_TRACE_ENTRIES];
	unsigned long count[MAX_STACE_TRACE_ENTRIES];
	u64 cost[MAX_STACE_TRACE_ENTRIES];
	u64 timestamp[MAX_STACE_TRACE_ENTRIES];
};


struct SYS_COST_TRACE{
	unsigned long count[8];
	u64 cost[8];
};

static struct SYS_COST_TRACE sys_cost_sum = {0};
static struct SYS_COST_INFO sys_cost_info = {0};
static void clean_data(void);

////extern struct xm_percpu_context *xm_percpu_context[NR_CPUS];
static void dump_sys_cost_info(void)
{
    int cnt = sys_cost_info.cnt;
	if (cnt >= MAX_STACE_TRACE_ENTRIES)
	{
	    printk("xiaomi sys_cost_info is full!\n");
	    return;
	}
	
	for (int i = 0; i < 8; i++)
	{
	    ///sys_cost_sum.timestamp[i] = sched_clock();
	    printk("xiaomi cpu[%d], count=%lu, cost=%llu\n", i, sys_cost_sum.count[i], sys_cost_sum.cost[i]);
		if (sys_cost_sum.count[i] > (sys_cost_count_per_second * sampling_period))
		{
		    printk("xiaomi_km: syscall strom cpu[%d], count=%lu, cost=%llu\n", i, sys_cost_sum.count[i], sys_cost_sum.cost[i]);
		    sys_cost_info.cpu[cnt] = i;
		    sys_cost_info.count[cnt] = sys_cost_sum.count[i];
		    sys_cost_info.cost[cnt] = sys_cost_sum.cost[i];
			sys_cost_info.timestamp[cnt] = sched_clock();
		    sys_cost_info.cnt++;
			break;
		}
	}
	clean_data();
}

/*Print sys_cost info*/
static void sys_cost_dump_func(struct work_struct *work)
{
    if (!trace_enable)
    {
        pr_info("xiaomi sys_cost_dump_func disabled!\n");
        return;
    }
	
	pr_info("xiaomi sys_cost_dump_func start \n");
	dump_sys_cost_info();
	schedule_delayed_work(&sys_cost_dump,round_jiffies_relative(msecs_to_jiffies(sampling_period*1000)));
}

static void clean_data(void)
{
    memset(&sys_cost_sum, 0, sizeof(struct SYS_COST_TRACE));
}

static int need_trace(struct task_struct *tsk)
{
	return 1;
}

static void start_trace_syscall(struct task_struct *tsk)
{
	struct xm_percpu_context *context;
	struct pt_regs *regs = task_pt_regs(tsk);
	unsigned long id;

	if (!need_trace(current))
		return;
	if (regs == NULL)
		return;

	id = SYSCALL_NO(regs);
	if (id >= NR_syscalls_virt)
		return;

	context = get_percpu_context();
	context->sys_cost.start_time = sched_clock();
}

static void stop_trace_syscall(struct task_struct *tsk)
{
	unsigned long id;
	struct pt_regs *regs = task_pt_regs(tsk);
	struct xm_percpu_context *context;
	u64 start;
	u64 now;
	u64 delta_ns;

	context = get_percpu_context();
	start = context->sys_cost.start_time;
	context->sys_cost.start_time = 0;

	if (!need_trace(current))
		return;

	if (regs == NULL)
		return;

	id = SYSCALL_NO(regs);
	if (id >= NR_syscalls_virt)
		return;

	
	if (start == 0)
		return;

	now = sched_clock();
	if (now > start)
		delta_ns = now - start;
	else
		delta_ns = 0;

	sys_cost_sum.cost[smp_processor_id()] += delta_ns;
	sys_cost_sum.count[smp_processor_id()]++;
}

static void trace_sys_enter_hit(void *__data, struct pt_regs *regs, long id)
{
	if (!need_trace(current))
		return;

	start_trace_syscall(current);
}


static void trace_sched_switch_hit(void *__data, bool preempt, struct task_struct *prev, struct task_struct *next, unsigned int pre_state)
{
	stop_trace_syscall(prev);
	start_trace_syscall(next);
}

static void trace_sys_exit_hit(void *__data, struct pt_regs *regs, long ret)
{
	stop_trace_syscall(current);
}

static int __activate_sys_cost(void)
{
	clean_data();

	hook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	hook_tracepoint("sys_exit", trace_sys_exit_hit, NULL);
	hook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);

	return 1;
}

static void __deactivate_sys_cost(void)
{
	unhook_tracepoint("sys_enter", trace_sys_enter_hit, NULL);
	unhook_tracepoint("sys_exit", trace_sys_exit_hit, NULL);
	unhook_tracepoint("sched_switch", trace_sched_switch_hit, NULL);

	synchronize_sched();
}

int activate_sys_cost(void)
{
	if (!sys_cost_settings.activated)
		sys_cost_settings.activated = __activate_sys_cost();

    printk("xiaomi activate_sys_cost start\n");
	memset(&sys_cost_info, 0, sizeof(struct SYS_COST_INFO));

	schedule_delayed_work(&sys_cost_dump, round_jiffies_relative(msecs_to_jiffies(sampling_period*1000)));

	return sys_cost_settings.activated;
}

int deactivate_sys_cost(void)
{
	if (sys_cost_settings.activated)
		__deactivate_sys_cost();
	sys_cost_settings.activated = 0;

	printk("xiaomi deactivate_sys_cost cancel\n");
	cancel_delayed_work_sync(&sys_cost_dump);

	return 0;
}

static int sampling_period_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", sampling_period);

	return 0;
}

static ssize_t sampling_period_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	sampling_period = val;

	return count;
}

static int threshold_show(struct seq_file *m, void *ptr)
{
	seq_printf(m, "%d\n", sys_cost_count_per_second);

	return 0;
}

static ssize_t threshold_store(void *priv, const char __user *buf, size_t count)
{
	int val;

	if (kstrtoint_from_user(buf, count, 0, &val))
		return -EINVAL;

	sys_cost_count_per_second = val;

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
		activate_sys_cost();
	} else {
	    deactivate_sys_cost();
	}

	trace_enable = enable;

	return count;
}

static int stack_trace_show(struct seq_file *m, void *ptr)
{
	for (int i = 0; i < sys_cost_info.cnt; i++)
	{
	    seq_printf(m, "cpu[%d], count=%lu, cost=%llu, timestatmp=%llu\n", sys_cost_info.cpu[i], sys_cost_info.count[i],  sys_cost_info.cost[i], sys_cost_info.timestamp[i]);
	}	

	return 0;
}  

static ssize_t stack_trace_store(void *priv, const char __user *buf,
				 size_t count)
{
	return count;
}

DEFINE_PROC_ATTRIBUTE_RW(sampling_period);
DEFINE_PROC_ATTRIBUTE_RW(threshold);
DEFINE_PROC_ATTRIBUTE_RW(enable);
DEFINE_PROC_ATTRIBUTE_RW(stack_trace);


int xiaomi_sys_cost_init(void)
{
	struct proc_dir_entry *parent_dir;

	INIT_DELAYED_WORK(&sys_cost_dump, sys_cost_dump_func);

	if (sys_cost_settings.activated)
		sys_cost_settings.activated = __activate_sys_cost();

	parent_dir = xiaomi_proc_mkdir(PROC_TRACE_SYS_COST, NULL);

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
		remove_proc_subtree(PROC_TRACE_SYS_COST, NULL);
	free_buf:
		return -ENOMEM;

}

void xiaomi_sys_cost_exit(void)
{
	if (sys_cost_settings.activated)
		deactivate_sys_cost();
	sys_cost_settings.activated = 0;

	msleep(20);
	remove_proc_subtree(PROC_TRACE_SYS_COST, NULL);
}
