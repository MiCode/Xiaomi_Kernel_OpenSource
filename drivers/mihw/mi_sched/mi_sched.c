#define pr_fmt(fmt) "mi-sched: " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <linux/mi_sched.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/compat.h>
#include <trace/hooks/sched.h>
#include "../include/mi_module.h"
#include "../../../kernel/sched/sched.h"

static int mi_sched_debug = 0;


void mi_sched_tick(void *nouse, struct rq *rq)/*should run in timer interrupt*/
{
	if (!mi_sched_debug)
		return;

	printk("mi_sched_tick: %d", rq->nr_running);
}
EXPORT_SYMBOL(mi_sched_tick);


void free_mi_task_struct(void *nouse, struct task_struct *tsk)
{
	if (!mi_sched_debug)
		return;

	printk("free mi_task_struct: %s,%d\n", tsk->comm, tsk->pid);
}
EXPORT_SYMBOL(free_mi_task_struct);

static int __init mi_sched_init(void)
{
	register_trace_android_vh_free_task(free_mi_task_struct, NULL);
	register_trace_android_vh_scheduler_tick(mi_sched_tick, NULL);
	return 0;

}

static void __exit mi_sched_exit(void)
{
	printk(KERN_ERR "in %s\n", __func__);
}

module_init(mi_sched_init);
module_exit(mi_sched_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("pkg runtime info calc by David");
