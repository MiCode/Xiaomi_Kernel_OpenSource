

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
#include <linux/bitmap.h>
#include <linux/nmi.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/sched/mm.h>
#include <asm-generic/irq_regs.h>

#include "internal.h"
#include "mm_tree.h"


void free_mm_info(struct rcu_head *rcu)
{
	struct mm_info *this = container_of(rcu, struct mm_info, rcu_head);

	kfree(this);
}

void init_mm_tree(struct mm_tree *mm_tree)
{
	INIT_RADIX_TREE(&mm_tree->mm_tree, GFP_ATOMIC);
	spin_lock_init(&mm_tree->mm_tree_lock);
}

struct mm_info *find_mm_info(struct mm_tree *mm_tree, struct mm_struct *mm)
{
	struct mm_info *info;

	if (mm == NULL)
		return NULL;

	info = radix_tree_lookup(&mm_tree->mm_tree, (unsigned long)mm);

	return info;
}

void putin_mm_info(struct mm_tree *mm_tree, struct mm_info *mm_info)
{
	unsigned long flags;
	struct mm_info *tmp;
	struct mm_struct *mm;

	if (!mm_info)
		return;

	mm = mm_info->mm;

	spin_lock_irqsave(&mm_tree->mm_tree_lock, flags);
	tmp = radix_tree_lookup(&mm_tree->mm_tree, (unsigned long)mm);
	if (tmp) {
		radix_tree_delete(&mm_tree->mm_tree, (unsigned long)mm);
		call_rcu(&tmp->rcu_head, free_mm_info);
	}
	radix_tree_insert(&mm_tree->mm_tree, (unsigned long)mm, mm_info);
	spin_unlock_irqrestore(&mm_tree->mm_tree_lock, flags);
}

struct mm_info *takeout_mm_info(struct mm_tree *mm_tree, struct mm_struct *mm)
{
	unsigned long flags;
	struct mm_info *info = NULL;

	spin_lock_irqsave(&mm_tree->mm_tree_lock, flags);
	info = radix_tree_delete(&mm_tree->mm_tree, (unsigned long)mm);
	spin_unlock_irqrestore(&mm_tree->mm_tree_lock, flags);

	return info;
}

void __get_argv_processes(struct mm_tree *mm_tree)
{
}

void dump_proc_chains_argv(int style, struct mm_tree *mm_tree,
	struct task_struct *tsk,
	struct xm_proc_chains_detail *detail)
{
	struct task_struct *walker;
	struct mm_info *mm_info;
	int cnt = 0;
	int i = 0;
	struct task_struct *leader;

	for (i = 0; i < PROCESS_CHAINS_COUNT; i++) {
		detail->chains[i][0] = 0;
		detail->tgid[i] = 0;
	}
	if (style == 0)
		return;

	if (!tsk || !tsk->mm)
		return;

	leader = tsk->group_leader;
	if (!leader || !leader->mm || leader->exit_state == EXIT_ZOMBIE){
		return;
	}

	rcu_read_lock();
	walker = tsk;

	while (walker->pid > 0) {
		if (!thread_group_leader(walker))
			walker = rcu_dereference(walker->group_leader);
		mm_info = find_mm_info(mm_tree, walker->mm);
		if (mm_info) {
			if (mm_info->cgroup_buf[0] == 0)
				xiaomi_cgroup_name(walker, mm_info->cgroup_buf, 255, 0);
			strncpy(detail->chains[cnt], mm_info->argv, PROCESS_ARGV_LEN);
			detail->full_argv[cnt] = 1;
		} else {
			strncpy(detail->chains[cnt], walker->comm, TASK_COMM_LEN);
			detail->full_argv[cnt] = 0;
		}
		detail->tgid[cnt] = walker->pid;
		walker = rcu_dereference(walker->real_parent);
		cnt++;
		if (cnt >= PROCESS_CHAINS_COUNT)
			break;
	}
	rcu_read_unlock();
}

int get_argv_from_mm(struct mm_struct *mm, char *buf, size_t size)
{
	return 0;
}

void xm_hook_exec(struct linux_binprm *bprm, struct mm_tree *mm_tree)
{
}

void xm_hook_process_exit_exec(struct task_struct *tsk, struct mm_tree *mm_tree)
{
	struct mm_info *mm_info;

	if (!tsk)
		return;
	if (!thread_group_leader(tsk))
		tsk = rcu_dereference(tsk->group_leader);
	if (!tsk || !tsk->mm)
		return;

	mm_info = takeout_mm_info(mm_tree, tsk->mm);
	if (mm_info) {
		kfree(mm_info);
	}
}

void dump_proc_chains_simple(struct task_struct *tsk,
	struct xm_proc_chains_detail *detail)
{
	struct task_struct *walker;
	int cnt = 0;
	int i = 0;
	struct task_struct *leader;

	for (i = 0; i < PROCESS_CHAINS_COUNT; i++) {
		detail->chains[i][0] = 0;
		detail->tgid[i] = 0;
	}

	if (!tsk || !tsk->mm)
		return;

	leader = tsk->group_leader;
	if (!leader || !leader->mm || leader->exit_state == EXIT_ZOMBIE){
		return;
	}

	rcu_read_lock();
	walker = tsk;
	for (i = 0; i < PROCESS_CHAINS_COUNT; i++) {
		detail->chains[i][0] = 0;
	}
	while (walker->pid > 0) {
		if (!thread_group_leader(walker))
			walker = rcu_dereference(walker->group_leader);
		strncpy(detail->chains[cnt], walker->comm, PROCESS_ARGV_LEN);
		detail->full_argv[cnt] = 0;
		detail->tgid[cnt] = walker->pid;

		walker = rcu_dereference(walker->real_parent);
		cnt++;
		if (cnt >= PROCESS_CHAINS_COUNT)
			break;
	}
	rcu_read_unlock();
}

void printk_process_chains(struct xm_proc_chains_detail *proc_chains)
{
	int i;

	if (proc_chains == NULL)
		return;

	printk("    进程链信息：\n");
	for (i = 0; i < PROCESS_CHAINS_COUNT; i++) {
		if (proc_chains->chains[i][0] == 0)
			break;

		printk("          %s\n", proc_chains->chains[i]);
	}
}

