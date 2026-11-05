/* rt_mod is used to hack sched related syscall, see "man 7 sched" for detailed info
 * such as sched_setscheduler to track who is mis-using rt to boost priority regardless
 * of system robustness.
 */

/* include order: 
 * private headers files shall follow standard headers. 
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <uapi/linux/sched.h>
#include <uapi/linux/sched/types.h>

#include "sandbox.h"

static struct kprobe kp = {
	.symbol_name	= "__sched_setscheduler",
};

static int rt_policy(int policy)
{
	return policy == SCHED_FIFO || policy == SCHED_RR;
}
#if SANDBOX_DEBUG
static int fifo_policy(int policy)
{
	return policy == SCHED_FIFO;
}

static int rr_policy(int policy)
{
	return policy == SCHED_RR;
}
#endif
static inline int conv_prio(int policy, int prio)
{
	int pri;

	if (rt_policy(policy))
		pri = MAX_RT_PRIO - 1 - prio;
	else
		pri = NICE_TO_PRIO(prio);

	return pri;
}

/* kprobe pre_handler: called just before the probed instruction is executed */
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	/* Prototype:
	 * static int __sched_setscheduler(struct task_struct *p,
	 *			const struct sched_attr *attr,
	 *			bool user, bool pi)
	 * so here we need to hack the second param (rsi for x86; regs[1] for arm)
	 * to get sched_policy and sched_priority.
	 */
	int policy, prio;
	struct sched_attr *attr;
	struct task_struct *t;
#ifdef CONFIG_X86
#if 0
	printk(KERN_INFO "rt_mod: pre_handler params [task %s pid=%d] pt_regs:{rdi=0x%.16lx rsi=0x%.16lx}\n",
		current->comm, current->pid, regs->di, regs->si);
#endif

	/* TODO: x86 calling convension, shall be changed for ARM.*/
	attr = (struct sched_attr *)(regs->si);

#endif

#ifdef CONFIG_ARM64
#if SANDBOX_DEBUG
	printk(KERN_INFO "rt_mod: pre_handler params [task %s pid=%d] pt_regs:{x0=0x%.16lx x1=0x%.16lx}\n",
		current->comm, current->pid, regs->regs[0], regs->regs[1]);
#endif
	/* arm64 calling convension here. */
	attr = (struct sched_attr *)(regs->regs[1]);
	t = (struct task_struct *)(regs->regs[0]);
#endif
	policy = attr->sched_policy;
	prio = conv_prio(policy, attr->sched_priority);

#if SANDBOX_DEBUG
	if (rt_policy(policy) || rt_policy(t->policy)) {
		printk(KERN_INFO "rt_mod: pre_handler [task %s pid=%d prio=%d] is trying to set [task %s pid=%d prio=%d] prio is %d.it's policy to %s \n",
			current->comm, current->pid, current->prio, t->comm, t->pid, t->prio, prio, (fifo_policy(policy) ? "FIFO": ((rr_policy(policy)) ? "RR" : "OTHER")));
	}
#endif
	/* A dump_stack() needed or not to double confirm caller */

	return 0;
}

/* kprobe post_handler: called after the probed instruction is executed */
static void handler_post(struct kprobe *p, struct pt_regs *regs,
				unsigned long flags)
{
	int policy, prio;
	struct sched_attr *attr;
#ifdef CONFIG_X86
#if SANDBOX_DEBUG
	printk(KERN_INFO "post_handler: p->addr = 0x%p, flags = 0x%lx\n",
		p->addr, regs->flags);
#endif

	/*TODO: hack retcode from sched_setscheduler syscall, return error if rt-policy check failed*/
	attr = (struct sched_attr *)(regs->si);
#endif
#ifdef CONFIG_ARM64
#if SANDBOX_DEBUG
	int ret = 0;
	printk(KERN_INFO "post_handler: p->addr = 0x%p, pstate = 0x%lx\n",
		p->addr, regs->pstate);
#endif

	/* arm64 calling convension here. */
	attr = (struct sched_attr *)(regs->regs[1]);
#endif
	policy = attr->sched_policy;
	prio = conv_prio(policy, attr->sched_priority);

#if SANDBOX_DEBUG
	/* TODO: rt-whitelist check here! */
	ret = sandbox_whitelist_audit(current);
	if( ret < 0) {
		printk(KERN_INFO "rt_mod: post_handler whitelist audit failed for [task %s pid=%d] to [%s, prio=%d] \n",
			current->comm, current->pid, (fifo_policy(policy) ? "FIFO": ((rr_policy(policy)) ? "RR" : "OTHER")), prio);
	} else {
		printk(KERN_INFO "rt_mod: post_handler whitelist audit succeed for [task %s pid=%d] to [%s, prio=%d] \n",
			current->comm, current->pid, (fifo_policy(policy) ? "FIFO": ((rr_policy(policy)) ? "RR" : "OTHER")), prio);
	}
	printk(KERN_INFO "rt_mod: stat for [task %s pid=%d] sum_exec_runtime=%lld\n",
		current->comm, current->pid, current->se.sum_exec_runtime);
	printk(KERN_INFO "rt_mod: schedstat for [task %s pid=%d] wait_max=%lld, sleep_max=%lld, block_max=%lld, exec_max=%lld\n",
		current->comm, current->pid, current->stats.wait_max, current->stats.sleep_max,
		current->stats.block_max, current->stats.exec_max);
#endif
}

static int __init kprobe_init(void)
{
	int ret;
	kp.pre_handler = handler_pre;
	kp.post_handler = handler_post;
	/* kernel version before v5.4 has fault_handler, but there's no such handler in v6.1 */
	//kp.fault_handler = handler_fault;
	ret = register_kprobe(&kp);
	if (ret < 0) {
		printk(KERN_INFO "register_kprobe failed, returned %d\n", ret);
		return ret;
	}
	printk(KERN_INFO "rt_mod planted kprobe at %p = %s\n", kp.addr, kp.symbol_name);

	/*rtload miscdev register*/
	rtload_miscdev_init();
	return 0;
}
static void __exit kprobe_exit(void)
{
	unregister_kprobe(&kp);
	printk(KERN_INFO "kprobe at %p unregistered\n", kp.addr);

	rtload_miscdev_exit();
	return;
}
module_init(kprobe_init)
module_exit(kprobe_exit)
MODULE_AUTHOR("Samuel <huangshuai1@xiaomi.com>");
MODULE_LICENSE("GPL");
