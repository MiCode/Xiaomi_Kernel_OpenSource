

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
#include <trace/hooks/sched.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/version.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/crc32.h>
#include <linux/fs.h>
#include <linux/timex.h>
#include <linux/cpu.h>
#include <linux/mi_kernel_monitor.h>
#include "internal.h"
#include "kern_internal.h"

#define CREATE_TRACE_POINTS
#include "traces/kernel_monitor.h"
#include "traces/mem_monitor.h"

int xm_timer_period = 25;
extern void rt_nosched_timer(void);
extern void preemptoff_nosched_timer(void);

/*
 * Is this task belong to cgroup "background"
 */

/*
 * Handler for android_rvh_new_task_stats tracepoint to initialize mi_kernel_monitor fields.
 * We use android_rvh_new_task_stats instead of android_rvh_wake_up_new_task because
 * android_rvh_wake_up_new_task already has two handlers registered, and adding a third
 * handler could cause issues. android_rvh_new_task_stats is called during task creation
 * and provides a suitable hook point for initializing the mi_kernel_monitor structure.
 *
 * Note: register_trace_android_rvh_new_task_stats can only be called once, so we
 * register it during module initialization rather than in activate_event_monitor()
 * which may be called multiple times.
 */
static void android_rvh_new_task_stats_handler(void *unused, struct task_struct *p)
{
	struct mi_kernel_monitor *km;

	km = get_mi_kernel_monitor(p);
	if (unlikely(IS_ERR_OR_NULL(km)))
		return;

	km->mem_failure_prop_expire_time = 0;
	km->mem_failure_prop_depth = 0;
	km->direct_reclaim_window_start_tm = 0;
}

static void register_android_rvh_new_task_stats(void)
{
	int ret = register_trace_android_rvh_new_task_stats(android_rvh_new_task_stats_handler,
							    NULL);
	if (ret)
		pr_err("Failed to register android_rvh_new_task_stats handler,ret = %d.", ret);
	else
		pr_info("Succeed to register android_rvh_new_task_stats handler.");
}

/*
 * Is this task belong to cgroup "top-app"
 */
bool xm_get_task_cgrp(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_CGROUP_SCHED) //in O10, it is true
    struct cgroup *grp;

    rcu_read_lock();
    grp = task_cgroup(task, cpu_cgrp_id);
    rcu_read_unlock();

    if (strcmp(grp->kn->name, "top-app") == 0) {
		return true;
    }

    if (strcmp(grp->kn->name, "foreground") == 0) {
		return true;
    }
	
    return false;
#else
    return false;
#endif
}

unsigned int (*stack_trace_save_skip_hardirq)(struct pt_regs *regs,
						     unsigned long *store,
						     unsigned int size,
						     unsigned int skipnr);

static int noop_pre_handler(struct kprobe *p, struct pt_regs *regs){
	return 0;
}

static int stack_trace_skip_hardirq_init(void)
{
	int ret;
	struct kprobe kp;
	unsigned long (*kallsyms_lookup_name_fun)(const char *name);


	ret = -1;
	kp.symbol_name = "kallsyms_lookup_name";
	kp.pre_handler = noop_pre_handler;
	stack_trace_save_skip_hardirq = NULL;

	ret = register_kprobe(&kp);
	if (ret < 0) {
	    printk("mi_kernel-debug, trace_irqoff register_kprobe failed!\n");
	    return -1;
	}

    printk("mi_kernel-debug, trace_irqoff register_kprobe successfully!\n");
	kallsyms_lookup_name_fun = (void*)kp.addr;
	
    printk("mi_kernel-debug, irqoff kallsyms_lookup_name_fun is %p\n", kallsyms_lookup_name_fun);

    if (!kallsyms_lookup_name_fun) {
        printk("mi_kernel-debug, irq-off kallsyms_lookup_name_fun get failed!\n");
        return 1;
    }
        	
	unregister_kprobe(&kp);

	stack_trace_save_skip_hardirq =
		(void *)kallsyms_lookup_name_fun("stack_trace_save_regs"); 

       return 0;
}

int xiaomi_kernel_init(void)
{
	int ret;

    printk("mi_kernel xiaomi_kernel_init begin.\n");

	ret = stack_trace_skip_hardirq_init();
	if (ret) {
		pr_err("mi_kernel stack_trace_skip_hardirq_init failed, ret=%d\n", ret);
		goto out;
	}	

#if 0
	ret = xiaomi_irq_stats_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_irq_stats_init failed, ret=%d\n", ret);
		goto out;
	}


	ret = xiaomi_mutex_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_mutex_init failed, ret=%d\n", ret);
		////goto out_mutex;
		goto out;
	}
#endif

	mi_task_event_init();

	ret = xiaomi_sys_cost_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_sys_cost_init failed, ret=%d\n", ret);
		goto out_syscall_cpu_cost;
	}

	ret = xiaomi_sys_delay_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_sys_delay_init failed, ret=%d\n", ret);
		goto out_syscall;
	}

	ret = xiaomi_irq_trace_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_irq_trace_init failed, ret=%d\n", ret);
		goto out_irq_trace;
	}

#if 0
	ret = xiaomi_rw_sem_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_rw_sem_init failed, ret=%d\n", ret);
		goto out_rw_sem;
	}
#endif

	ret = xiaomi_trace_irqoff_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_trace_irqoff_init failed, ret=%d\n", ret);
		goto out_trace_irqoff;
	}

	ret = xiaomi_trace_rt_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_trace_rt_init failed, ret=%d\n", ret);
		goto out_trace_rt;
	}	
	
	ret = xiaomi_trace_preemptoff_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_trace_preemptoff_init failed, ret=%d\n", ret);
		goto out_trace_preemptoff;
	}

	ret = xiaomi_cpu_util_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_cpu_util_init failed, ret=%d\n", ret);
		goto out_cpu_util;
	}

	ret = xiaomi_binder_trace_init();
	if (ret) {
		pr_err("mi_kernel xiaomi_binder_trace_init failed, ret=%d\n", ret);
		goto out_binder_trace;
	}

	register_android_rvh_new_task_stats();
	xiaomi_sched_trace_init();
	xiaomi_mem_trace_init();
	xiaomi_io_trace_init();
	xm_locking_init();

	printk("mi_kernel xiaomi_kernel_init end\n");
	////on_each_cpu(start_timer, NULL, 1);

	return 0;
out_binder_trace:
	xiaomi_cpu_util_exit();
out_cpu_util:	
	xiaomi_trace_preemptoff_exit();
out_trace_preemptoff:
	xiaomi_trace_rt_exit();
out_trace_rt:
    xiaomi_trace_irqoff_exit();
out_trace_irqoff:
////    xiaomi_rw_sem_exit();
////out_rw_sem:
	xiaomi_irq_trace_exit();
out_irq_trace:
	xiaomi_sys_delay_exit();
out_syscall:
	xiaomi_sys_cost_exit();
out_syscall_cpu_cost:
#if 0	
	xiaomi_mutex_exit();
out_mutex:
	xiaomi_irq_stats_exit();
#endif	
out:
	return ret;
}

static void sys_delay_timer_cancel(void)
{
	int cpu;
	struct xm_percpu_context *percpu_context;
	struct hrtimer *timer;

	for_each_possible_cpu(cpu)
	{
		percpu_context = get_percpu_context_cpu(cpu);
		if (percpu_context->timer_info.timer_started)
		{
			timer = &percpu_context->timer_info.timer;
			hrtimer_cancel(timer);
			percpu_context->timer_info.timer_started = 0;
		}
	}

}

void xiaomi_kernel_exit(void)
{
	sys_delay_timer_cancel();

	////xiaomi_rw_sem_exit();       //modify by huqinghe 20250526
	xiaomi_irq_trace_exit();
	xiaomi_sys_cost_exit();
	xiaomi_sys_delay_exit();
	////xiaomi_mutex_exit();        //modify by huqinghe 20250526
	/////xiaomi_irq_stats_exit();   //modify by huqinghe 20250526
	xiaomi_trace_irqoff_exit();
	xiaomi_trace_rt_exit();
	xiaomi_trace_preemptoff_exit();
	xiaomi_cpu_util_exit();

	xiaomi_binder_trace_exit();
	xiaomi_sched_trace_exit();
	xiaomi_mem_trace_exit();
	xiaomi_io_trace_exit();
	xm_locking_exit();
	mi_task_event_exit();
}
