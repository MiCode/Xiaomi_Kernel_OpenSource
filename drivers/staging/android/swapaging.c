/*
 * swapaging.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/err.h>
#include <linux/sort.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/delay.h>
#include <linux/hugetlb.h>
#include <asm/tlbflush.h>

static unsigned long deathpending_timeout;
struct work_struct lowswap_work;
static void lowswap_fn(struct work_struct *lowswap_work);

static int enabled = 1;
module_param_named(enabled, enabled, int,
		S_IRUGO | S_IWUSR);
static int minadj = 529;
module_param_named(minadj, minadj, int,
		S_IRUGO | S_IWUSR);
static uint agingmax = 4;
module_param_named(agingmax, agingmax, uint,
		S_IRUGO | S_IWUSR);

struct swap_unshared_stats {
	struct vm_area_struct *vma;
	unsigned long unshared;
};

static int call_pte_range(pmd_t *pmd, unsigned long addr,
		unsigned long end, struct mm_walk *walk)
{
	struct swap_unshared_stats *su = walk->private;
	struct vm_area_struct *vma = su->vma;
	pte_t *pte, ptent;
	spinlock_t *ptl;

	split_huge_page_pmd(vma, addr, pmd);
	if (pmd_trans_unstable(pmd))
		return 0;

	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	for (; addr != end; pte++, addr += PAGE_SIZE) {
		ptent = *pte;
		if (!pte_present(ptent) && is_swap_pte(ptent)) {
			swp_entry_t swpent = pte_to_swp_entry(ptent);

			if (!non_swap_entry(swpent)) {
				if (swp_swapcount(swpent) < 2)
					su->unshared++;
			}
		}
	}
	pte_unmap_unlock(pte - 1, ptl);

	cond_resched();
	return 0;
}

unsigned long get_swap_unshared(struct task_struct *task)
{
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct mm_walk get_unshared_walk = {};
	struct swap_unshared_stats su;

	get_task_struct(task);
	mm = get_task_mm(task);
	if (!mm)
		goto out;
	memset(&su, 0, sizeof(su));
	get_unshared_walk.mm = mm;
	get_unshared_walk.pmd_entry = call_pte_range;
	get_unshared_walk.private = &su;

	down_read(&mm->mmap_sem);
	for (vma = mm->mmap; vma; vma = vma->vm_next) {
		if (is_vm_hugetlb_page(vma))
			continue;

		if (vma->vm_file)
			continue;

		su.vma = vma;
		walk_page_range(vma->vm_start, vma->vm_end,
				&get_unshared_walk);
	}

	flush_tlb_mm(mm);
	up_read(&mm->mmap_sem);
	mmput(mm);
out:
	put_task_struct(task);
	return su.unshared;
}

struct selected_tsk {
	struct task_struct *p;
	ulong swappedsize_unshared;
	short oom_score_adj;
};

static int selected_cmp(const void *a, const void *b)
{
	const struct selected_tsk *x = a;
	const struct selected_tsk *y = b;
	int ret;

	ret = x->swappedsize_unshared < y->swappedsize_unshared ? -1 : 1;
	return ret;
}

void swapaging_work(void)
{
	if (enabled && !work_pending(&lowswap_work))
		queue_work(system_unbound_wq, &lowswap_work);
}

static void lowswap_fn(struct work_struct *lowswap_work)
{
	int i = 0;
	int j = 0;
	ulong swappedsize_unshared = 0;
	ulong total_recaimed = 0;

	struct task_struct *tsk;
	struct selected_tsk *selected;

	if (!agingmax)
		return;
	if (agingmax > 10)
		agingmax = 10;

	selected = kcalloc(agingmax, sizeof(*selected), GFP_KERNEL);
	if (!selected)
		return;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (test_tsk_thread_flag(p, TIF_MEMDIE) &&
				time_before_eq(jiffies, deathpending_timeout)) {
			task_unlock(p);
			rcu_read_unlock();
			kfree(selected);
			return;
		}
		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < minadj) {
			task_unlock(p);
			continue;
		}
		task_unlock(p);

		swappedsize_unshared = get_swap_unshared(p);
		if (swappedsize_unshared <= 0)
			continue;

		if (i == agingmax) {
			sort(selected, agingmax,
					sizeof(struct selected_tsk),
					&selected_cmp, NULL);
			if (swappedsize_unshared
					< selected[0].swappedsize_unshared)
				continue;
			selected[0].p = p;
			selected[0].oom_score_adj = oom_score_adj;
			selected[0].swappedsize_unshared = swappedsize_unshared;
		} else {
			selected[i].p = p;
			selected[i].oom_score_adj = oom_score_adj;
			selected[i].swappedsize_unshared = swappedsize_unshared;
			i++;
		}
	}

	for (j = 0; j < i; j++) {
		if (!selected[j].p)
			break;
		task_lock(selected[j].p);
		send_sig(SIGKILL, selected[j].p, 0);
		if (selected[j].p->mm)
			test_and_set_tsk_thread_flag(selected[j].p, TIF_MEMDIE);
		task_unlock(selected[j].p);
		pr_err(" Low swap killing: '%s'(%d), adj %hd\n"
				" swappedsize_unshared %ldkB, #%d\n",
				selected[j].p->comm, selected[j].p->pid,
				selected[j].oom_score_adj,
				selected[j].swappedsize_unshared
				* (ulong)(PAGE_SIZE / 1024), j);
		total_recaimed += (selected[j].swappedsize_unshared)
				* (ulong)(PAGE_SIZE / 1024);
		if (j == i - 1)
			deathpending_timeout = jiffies + HZ;
	}

	rcu_read_unlock();
	kfree(selected);
	pr_info(" Low swap killing: try to reclaim swaps: %ldkB\n",
			total_recaimed);
}

int lowswap_init(void)
{
	INIT_WORK(&lowswap_work, lowswap_fn);
	return 0;
}

void lowswap_exit(void)
{
	flush_work(&lowswap_work);
}

static int debug_set(const char *val, const struct kernel_param *kp)
{
	swapaging_work();
	return 0;
}

static struct kernel_param_ops debug_ops = {
	.set = &debug_set,
	.get = &param_get_uint,
};

static short debug;
module_param_cb(debug, &debug_ops, &debug, S_IRUGO | S_IWUSR);
__MODULE_PARM_TYPE(debug, short);

module_init(lowswap_init);
module_exit(lowswap_exit);
MODULE_LICENSE("GPL");
