// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2023 Unisoc Communications Inc.
 */

#include <linux/module.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/signal.h>
#include <trace/hooks/vmscan.h>
#include <linux/rwsem.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/mm_inline.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/dma-mapping.h>
#include <linux/dma-direct.h>
#include <linux/vmstat.h>
#include <linux/rmap.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/memcontrol.h>
#include <linux/psi.h>
#include <linux/proc_fs.h>
#include <linux/cpufreq.h>
#include "../../../../mm/internal.h"

#define SHRINK_LRUVECD_HIGH (0x1000)

#define PG_nolockdelay (__NR_PAGEFLAGS + 2)
#define SetPageNoLockDelay(page) set_bit(PG_nolockdelay, &(page)->flags)
#define TestPageNoLockDelay(page) test_bit(PG_nolockdelay, &(page)->flags)
#define TestClearPageNoLockDelay(page) test_and_clear_bit(PG_nolockdelay, &(page)->flags)
#define ClearPageNoLockDelay(page) clear_bit(PG_nolockdelay, &(page)->flags)

#define PG_skiped_lock (__NR_PAGEFLAGS + 3)
#define SetPageSkipedLock(page) set_bit(PG_skiped_lock, &(page)->flags)
#define ClearPageSkipedLock(page) clear_bit(PG_skiped_lock, &(page)->flags)
#define PageSkipedLock(page) test_bit(PG_skiped_lock, &(page)->flags)
#define TestClearPageSkipedLock(page) test_and_clear_bit(PG_skiped_lock, &(page)->flags)

#define PG_migrating (__NR_PAGEFLAGS + 4)
#define SetPageMigrating(page) set_bit(PG_migrating, &(page)->flags)
#define TestPageMigrating(page) test_bit(PG_migrating, &(page)->flags)
#define TestClearPageMigrating(page) test_and_clear_bit(PG_migrating, &(page)->flags)
#define ClearPageMigrating(page) clear_bit(PG_migrating, &(page)->flags)

bool async_shrink_lruvec_setup;
static struct task_struct *shrink_lruvec_tsk;
static atomic_t shrink_lruvec_runnable = ATOMIC_INIT(0);
unsigned long shrink_lruvec_pages;
unsigned long shrink_lruvec_pages_max;
unsigned long shrink_lruvec_handle_pages;
wait_queue_head_t shrink_lruvec_wait;
spinlock_t l_inactive_lock;
LIST_HEAD(lru_inactive);

static bool process_is_shrink_lruvecd(struct task_struct *tsk)
{
	return (shrink_lruvec_tsk->pid == tsk->pid);
}

static void add_to_lruvecd_inactive_list(struct page *page)
{
	list_move(&page->lru, &lru_inactive);

	/* account how much pages in lru_inactive */
	shrink_lruvec_pages += thp_nr_pages(page);
	if (shrink_lruvec_pages > shrink_lruvec_pages_max)
		shrink_lruvec_pages_max = shrink_lruvec_pages;
}

static void handle_failed_page_trylock(void *data, struct list_head *page_list)
{
	struct page *page, *next;
	bool shrink_lruvecd_is_full = false;
	bool pages_should_be_reclaim = false;
	LIST_HEAD(tmp_lru_inactive);

	if (unlikely(!async_shrink_lruvec_setup))
		return;

	if (list_empty(page_list))
		return;

	if (unlikely(shrink_lruvec_pages > SHRINK_LRUVECD_HIGH))
		shrink_lruvecd_is_full = true;

	list_for_each_entry_safe(page, next, page_list, lru) {
		ClearPageNoLockDelay(page);
		if (unlikely(TestClearPageSkipedLock(page))) {
			/* trylock failed and been skiped  */
			ClearPageActive(page);
			if (!shrink_lruvecd_is_full)
				list_move(&page->lru, &tmp_lru_inactive);
		}
	}

	if (unlikely(!list_empty(&tmp_lru_inactive))) {
		spin_lock_irq(&l_inactive_lock);
		list_for_each_entry_safe(page, next, &tmp_lru_inactive, lru) {
			if (likely(!shrink_lruvecd_is_full)) {
				pages_should_be_reclaim = true;
				add_to_lruvecd_inactive_list(page);
			}
		}
		spin_unlock_irq(&l_inactive_lock);
	}

	if (shrink_lruvecd_is_full || !pages_should_be_reclaim)
		return;

	if (atomic_read(&shrink_lruvec_runnable) == 1)
		return;

	atomic_set(&shrink_lruvec_runnable, 1);
	wake_up_interruptible(&shrink_lruvec_wait);
}

void set_shrink_lruvecd_cpus(void)
{
	struct cpumask mask;
	struct cpumask *cpumask = &mask;
	pg_data_t *pgdat = NODE_DATA(0);
	unsigned int cpu = 0, cpufreq_max_tmp = 0;
	struct cpufreq_policy *policy_max = NULL;
	static bool set_slabd_cpus_success;

	if (unlikely(!async_shrink_lruvec_setup))
		return;

	if (likely(set_slabd_cpus_success))
		return;

	for_each_possible_cpu(cpu) {
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);

		if (policy == NULL)
			continue;

		if (policy->cpuinfo.max_freq >= cpufreq_max_tmp) {
			cpufreq_max_tmp = policy->cpuinfo.max_freq;
			policy_max = policy;
		}
	}

	cpumask_copy(cpumask, cpumask_of_node(pgdat->node_id));
	if (policy_max) {
		cpumask_andnot(cpumask, cpumask, policy_max->related_cpus);
	}

	if (!cpumask_empty(cpumask)) {
		set_cpus_allowed_ptr(shrink_lruvec_tsk, cpumask);
		set_slabd_cpus_success = true;
	}
}

static int shrink_lruvecd(void *p)
{
	pg_data_t *pgdat;
	LIST_HEAD(tmp_lru_inactive);
	struct page *page, *next;
	struct list_head;

	/*
	 * Tell the memory management that we're a "memory allocator",
	 * and that if we need more memory we should get access to it
	 * regardless (see "__alloc_pages()"). "kswapd" should
	 * never get caught in the normal page freeing logic.
	 *
	 * (Kswapd normally doesn't need memory anyway, but sometimes
	 * you need a small amount of memory in order to be able to
	 * page out something else, and this flag essentially protects
	 * us from recursively trying to free more memory as we're
	 * trying to free the first piece of memory in the first place).
	 */
	pgdat = (pg_data_t *)p;

	current->flags |= PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD;
	set_freezable();

	while (!kthread_should_stop()) {
		wait_event_freezable(shrink_lruvec_wait,
			(atomic_read(&shrink_lruvec_runnable) == 1));

		set_shrink_lruvecd_cpus();
retry_reclaim:

		spin_lock_irq(&l_inactive_lock);
		if (list_empty(&lru_inactive)) {
			spin_unlock_irq(&l_inactive_lock);
			atomic_set(&shrink_lruvec_runnable, 0);
			continue;
		}
		list_for_each_entry_safe(page, next, &lru_inactive, lru) {
			list_move(&page->lru, &tmp_lru_inactive);
			shrink_lruvec_pages -= thp_nr_pages(page);
			shrink_lruvec_handle_pages += thp_nr_pages(page);
		}
		spin_unlock_irq(&l_inactive_lock);

		reclaim_pages(&tmp_lru_inactive);
		goto retry_reclaim;
	}
	current->flags &= ~(PF_MEMALLOC | PF_SWAPWRITE | PF_KSWAPD);

	return 0;
}

static void page_trylock_set(void *data, struct page *page)
{
	if (unlikely(!async_shrink_lruvec_setup))
		return;

	ClearPageSkipedLock(page);

	if (unlikely(process_is_shrink_lruvecd(current))) {
		ClearPageNoLockDelay(page);
		return;
	}

	SetPageNoLockDelay(page);
}

static void page_migrating_set(void *data, struct page *page)
{
	SetPageMigrating(page);
}

static void page_migrating_clear(void *data, struct page *page)
{
	ClearPageMigrating(page);
}

static void page_trylock_clear(void *data, struct page *page)
{
	ClearPageNoLockDelay(page);
	ClearPageSkipedLock(page);
}

static void page_trylock_get_result(void *data, struct page *page, bool *trylock_fail)
{
	ClearPageNoLockDelay(page);

	if (unlikely(!async_shrink_lruvec_setup) ||
			unlikely(process_is_shrink_lruvecd(current))) {
		*trylock_fail = false;
		return;
	}

	if (PageSkipedLock(page))
		/*page trylock failed and been skipped*/
		*trylock_fail = true;
}

static void do_page_trylock(void *data, struct page *page, struct rw_semaphore *sem,
		bool *got_lock, bool *success)
{
	*success = false;
	if (unlikely(!async_shrink_lruvec_setup))
		return;

	if (TestPageMigrating(page))
		return;

	if (TestClearPageNoLockDelay(page)) {
		*success = true;

		if (sem == NULL)
			return;

		if (down_read_trylock(sem)) {
			/* return 1 successful */
			*got_lock = true;

		} else {
			/* trylock failed and skipped */
			SetPageSkipedLock(page);
			*got_lock = false;
		}
	}
}

static int kshrink_lruvecd_status_show(struct seq_file *m, void *arg)
{
	seq_printf(m,
		   "kshrink_lruvecd_setup:     %s\n"
		   "shrink_lruvec_pages:     %lu\n"
		   "shrink_lruvec_handle_pages:     %lu\n"
		   "shrink_lruvec_pages_max:     %lu\n",
		   async_shrink_lruvec_setup ? "enable" : "disable",
		   shrink_lruvec_pages,
		   shrink_lruvec_handle_pages,
		   shrink_lruvec_pages_max);
	seq_putc(m, '\n');

	return 0;
}

int kshrink_lruvec_init(void)
{
	pg_data_t *pgdat = NODE_DATA(0);
	int ret;

	register_trace_android_vh_handle_failed_page_trylock(handle_failed_page_trylock, NULL);
	register_trace_android_vh_page_trylock_set(page_trylock_set, NULL);
	register_trace_android_vh_page_trylock_clear(page_trylock_clear, NULL);
	register_trace_android_vh_page_trylock_get_result(page_trylock_get_result, NULL);
	register_trace_android_vh_do_page_trylock(do_page_trylock, NULL);
	register_trace_android_vh_set_page_migrating(page_migrating_set, NULL);
	register_trace_android_vh_clear_page_migrating(page_migrating_clear, NULL);

	init_waitqueue_head(&shrink_lruvec_wait);
	spin_lock_init(&l_inactive_lock);
	shrink_lruvec_tsk = kthread_run(shrink_lruvecd, pgdat, "kshrink_lruvecd");
	if (IS_ERR_OR_NULL(shrink_lruvec_tsk)) {
		pr_err("Failed to start shrink_lruvec on node 0\n");
		ret = PTR_ERR(shrink_lruvec_tsk);
		shrink_lruvec_tsk = NULL;
		return ret;
	}
	proc_create_single("kshrink_lruvecd_status", 0, NULL, kshrink_lruvecd_status_show);
	async_shrink_lruvec_setup = true;
	pr_info("kshrink_lruvecd: async_shrink_lruvec_init !!!!\n");

	return 0;
}

void kshrink_lruvec_exit(void)
{
	unregister_trace_android_vh_do_page_trylock(do_page_trylock, NULL);
	unregister_trace_android_vh_page_trylock_get_result(page_trylock_get_result, NULL);
	unregister_trace_android_vh_page_trylock_clear(page_trylock_clear, NULL);
	unregister_trace_android_vh_page_trylock_set(page_trylock_set, NULL);
	unregister_trace_android_vh_handle_failed_page_trylock(handle_failed_page_trylock, NULL);
	unregister_trace_android_vh_set_page_migrating(page_migrating_set, NULL);
	unregister_trace_android_vh_clear_page_migrating(page_migrating_clear, NULL);
}
