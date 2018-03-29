/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#define pr_fmt(fmt) "memory-lowpower-task: " fmt
#define CONFIG_MTK_MEMORY_LOWPOWER_TASK_DEBUG

#include <linux/types.h>
#include <linux/debugfs.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/err.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/rwlock.h>
#include <linux/sort.h>
#include <linux/delay.h>

/* Trigger method for screen on/off */
#include <linux/fb.h>

/* Receive PM event */
#include <linux/suspend.h>

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#endif
#include <linux/uaccess.h>

/* Memory lowpower private header file */
#include "internal.h"

/* Print wrapper */
#define MLPT_PRINT(args...)	do {} while (0) /* pr_alert(args) */
#define MLPT_PRERR(args...)	do {} while (0) /* pr_alert(args) */

/* Profile Timing */
#ifdef CONFIG_MLPT_PROFILE
static unsigned long long start_ns, end_ns;
#define MLPT_START_PROFILE()		{start_ns = sched_clock(); }
#define MLPT_END_PROFILE()	do {\
					end_ns = sched_clock();\
					MLPT_PRINT(" {{{Elapsed[%llu]ns}}}\n", (end_ns - start_ns));\
				} while (0)
#else	/* !CONFIG_MLPT_PROFILE */
#define MLPT_START_PROFILE()	do {} while (0)
#define MLPT_END_PROFILE()	do {} while (0)
#endif

/* List of memory lowpower features' specific operations */
static LIST_HEAD(memory_lowpower_handlers);
static DEFINE_MUTEX(memory_lowpower_lock);

/* Wakeup source */
#ifdef CONFIG_PM_WAKELOCKS
static struct wakeup_source mlp_wakeup;
#endif

/* Control parameters for memory lowpower task */
#define MLPT_CLEAR_ACTION       (0x0)
#define MLPT_SET_ACTION         (0x1)
static struct task_struct *memory_lowpower_task;
#ifdef CONFIG_MTK_PERIODIC_DATA_COLLECTION
static struct task_struct *periodic_dc_task;
unsigned long nr_dc;
unsigned long nr_skip_dc;
#define DATA_COLLECTION_PERIOD 300000
#endif /* CONFIG_MTK_PERIODIC_DATA_COLLECTION */
static enum power_state memory_lowpower_action;
static atomic_t action_changed;
static unsigned long memory_lowpower_state;
static int get_cma_aligned;			/* in PAGE_SIZE order */
static int get_cma_num;				/* number of allocations */
static unsigned long get_cma_size;		/* in PAGES */
static struct page **cma_aligned_pages;		/* NULL means full allocation */
static struct memory_lowpower_statistics memory_lowpower_statistics;

/*
 * Set aligned allocation -
 * @aligned: Requested alignment of pages (in PAGE_SIZE order).
 */
void set_memory_lowpower_aligned(int aligned)
{
	unsigned long size, num;

	/* No need to update */
	if ((get_cma_aligned != 0 && get_cma_aligned <= aligned) || aligned < 0)
		return;

	/* cma_aligned_pages is in use */
	if (get_cma_aligned != 0 && cma_aligned_pages[0] != NULL)
		return;

	/* Check whether size is a multiple of num */
	size = (memory_lowpower_cma_size() >> PAGE_SHIFT);
	num = size >> aligned;
	if (size != (num << aligned))
		return;

	/* Update aligned allocation */
	get_cma_aligned = aligned;
	get_cma_size = 1 << aligned;
	get_cma_num = num;
	if (cma_aligned_pages != NULL) {
		kfree(cma_aligned_pages);
		cma_aligned_pages = NULL;
	}

	/* If it is page-aligned, cma_aligned_pages is not needed */
	if (num != size) {
		cma_aligned_pages = kcalloc(num, sizeof(*cma_aligned_pages), GFP_KERNEL);
		BUG_ON(!cma_aligned_pages);
	}

	MLPT_PRINT("%s: aligned[%d] size[%lu] num[%d] array[%p]\n",
			__func__, get_cma_aligned, get_cma_size, get_cma_num, cma_aligned_pages);
}

static int inser_buffer_cmp(const void *a, const void *b)
{
	struct page **pa = (struct page **)a;
	struct page **pb = (struct page **)b;

	if (*pa > *pb)
		return 1;
	if (*pa < *pb)
		return -1;
	return 0;
}

/* Insert allocated buffer */
static void insert_buffer(struct page *page, int bound)
{
	/* sanity check */
	BUG_ON(bound >= get_cma_num);
	BUG_ON(cma_aligned_pages[bound] != NULL);
	BUG_ON(!pfn_valid(page_to_pfn(page)));

	cma_aligned_pages[bound] = page;

	/* The number for sorting is "bound + 1" */
	sort(cma_aligned_pages, bound + 1, sizeof(struct page *),
			inser_buffer_cmp, NULL);
}

/*
 * Wrapper for memory lowpower CMA allocation -
 * Return 0 if success, -ENOMEM if no memory, -EBUSY if being aborted
 */
static int acquire_memory(void)
{
	int i = 0, ret = 0;
	struct page *page;

	/* Full allocation */
	if (cma_aligned_pages == NULL) {
		if (get_cma_num == 0) {
			i = 1;
			ret = get_memory_lowpower_cma();
			if (!ret)
				get_cma_num = 1;
		}
		goto out;
	}

	/* Find the 1st null position */
	while (i < get_cma_num && cma_aligned_pages[i] != NULL)
		++i;

	/* Aligned allocation */
	while (i < get_cma_num) {
		ret = get_memory_lowpower_cma_aligned(get_cma_size, get_cma_aligned, &page);
		if (ret)
			break;

		MLPT_PRINT("%s: PFN[%lu] allocated for [%d]\n", __func__, page_to_pfn(page), i);
		insert_buffer(page, i);
		++i;

		/* Early termination for "action is changed" */
		if (!IS_ACTION_SCREENOFF(memory_lowpower_action)) {
			pr_warn("%s: got a screen-on event\n", __func__);
			ret = -EBUSY;
			break;
		}
	}

out:
	memory_lowpower_statistics.nr_acquire_memory++;
	if (i == (get_cma_num))
		memory_lowpower_statistics.nr_full_acquire++;
	else if (i > 0)
		memory_lowpower_statistics.nr_partial_acquire++;
	else
		memory_lowpower_statistics.nr_empty_acquire++;

	/* Translate to -ENOMEM */
	if (ret == -1)
		ret = -ENOMEM;

	return ret;
}

/*
 * Wrapper for memory lowpower CMA free -
 * It returns 0 in success, otherwise return -EINVAL.
 */
static int release_memory(void)
{
	int i = 0, ret = 0;
	struct page **pages;

	/* Full release */
	if (cma_aligned_pages == NULL) {
		if (get_cma_num == 1) {
			ret = put_memory_lowpower_cma();
			if (!ret)
				get_cma_num = 0;
		}
		goto out;
	}

	/* Aligned release */
	pages = cma_aligned_pages;
	do {
		if (pages[i] == NULL)
			break;
		ret = put_memory_lowpower_cma_aligned(get_cma_size, pages[i]);
		if (!ret) {
			MLPT_PRINT("%s: PFN[%lu] released for [%d]\n", __func__, page_to_pfn(pages[i]), i);
			pages[i] = NULL;
		} else
			BUG();
	} while (++i < get_cma_num);

out:
	memory_lowpower_statistics.nr_release_memory++;

	/* Translate to -EINVAL */
	if (ret == -1)
		ret = -EINVAL;

	return ret;
}

/* Query CMA allocated buffer */
static void memory_range(int which, unsigned long *spfn, unsigned long *epfn)
{
	*spfn = *epfn = 0;

	/* Sanity check */
	BUG_ON(which >= get_cma_num);

	/* Range of full allocation */
	if (cma_aligned_pages == NULL) {
		if (get_cma_num == 1) {
			*spfn = __phys_to_pfn(memory_lowpower_cma_base());
			*epfn = __phys_to_pfn(memory_lowpower_cma_base() + memory_lowpower_cma_size());
		}
		goto out;
	}

	/* Range of aligned allocation */
	if (cma_aligned_pages[which] != NULL) {
		*spfn = page_to_pfn(cma_aligned_pages[which]);
		*epfn = *spfn + get_cma_size;
	}

out:
	MLPT_PRINT("%s: [%d] spfn[%lu] epfn[%lu]\n", __func__, which, *spfn, *epfn);
}

/* Check whether memory_lowpower_task is initialized */
bool memory_lowpower_task_inited(void)
{
	return (memory_lowpower_task != NULL);
}

/* Register API for memory lowpower operation */
void register_memory_lowpower_operation(struct memory_lowpower_operation *handler)
{
	struct list_head *pos;

	mutex_lock(&memory_lowpower_lock);
	list_for_each(pos, &memory_lowpower_handlers) {
		struct memory_lowpower_operation *e;

		e = list_entry(pos, struct memory_lowpower_operation, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&memory_lowpower_lock);
}

/* Unregister API for memory lowpower operation */
void unregister_memory_lowpower_operation(struct memory_lowpower_operation *handler)
{
	mutex_lock(&memory_lowpower_lock);
	list_del(&handler->link);
	mutex_unlock(&memory_lowpower_lock);
}

/* Screen-on cb operations */
static void __go_to_screenon(void)
{
	struct memory_lowpower_operation *pos;
	int ret = 0;
	int disabled[NR_MLP_LEVEL] = { 0, };

	/* Apply HW actions if needed */
	if (!MlpsEnable(&memory_lowpower_state))
		return;

	/* Disable actions */
	list_for_each_entry(pos, &memory_lowpower_handlers, link) {
		if (pos->disable != NULL) {
			ret = pos->disable();
			if (ret) {
				disabled[pos->level] += ret;
				MLPT_PRERR("Fail disable: level[%d] ret[%d]\n", pos->level, ret);
				ret = 0;
			}
		}
	}

	/* Restore actions */
	list_for_each_entry(pos, &memory_lowpower_handlers, link) {
		if (pos->restore != NULL) {
			ret = pos->restore();
			if (ret) {
				disabled[pos->level] += ret;
				MLPT_PRERR("Fail restore: level[%d] ret[%d]\n", pos->level, ret);
				ret = 0;
			}
		}
	}

	/* Clear ENABLE state */
	ClearMlpsEnable(&memory_lowpower_state);

	if (IS_ENABLED(CONFIG_MTK_DCS) && !disabled[MLP_LEVEL_DCS])
		ClearMlpsEnableDCS(&memory_lowpower_state);

	if (IS_ENABLED(CONFIG_MTK_PASR) && !disabled[MLP_LEVEL_PASR])
		ClearMlpsEnablePASR(&memory_lowpower_state);
}

/* Screen-on operations */
static void go_to_screenon(void)
{
	MLPT_PRINT("%s:+\n", __func__);
	MLPT_START_PROFILE();

	/* Should be SCREENOFF|SCREENIDLE -> SCREENON */
	if (MlpsScreenOn(&memory_lowpower_state) &&
			!MlpsScreenIdle(&memory_lowpower_state)) {
		MLPT_PRERR("Incomplete state[%lu]\n", memory_lowpower_state);
		goto out;
	}

	/* HW-related flow for screenon */
	__go_to_screenon();

	/* Currently in SCREENOFF */
	if (!MlpsScreenOn(&memory_lowpower_state))
		SetMlpsScreenOn(&memory_lowpower_state);

	/* Currently in SCREENIDLE */
	if (MlpsScreenIdle(&memory_lowpower_state))
		ClearMlpsScreenIdle(&memory_lowpower_state);

out:
	/* Release pages */
	release_memory();

	MLPT_END_PROFILE();
	MLPT_PRINT("%s:-\n", __func__);
}

/* Screen-off cb operations */
static void __go_to_screenoff(void)
{
	struct memory_lowpower_operation *pos;
	int ret = 0;
	int enabled[NR_MLP_LEVEL] = { 0, };

	/* Apply HW actions if needed */
	if (!IS_ACTION_SCREENOFF(memory_lowpower_action))
		return;

	/* Config actions */
	list_for_each_entry(pos, &memory_lowpower_handlers, link) {
		if (pos->config != NULL) {
			ret = pos->config(get_cma_num, memory_range);
			if (ret) {
				enabled[pos->level] += ret;
				MLPT_PRERR("Fail config: level[%d] ret[%d]\n", pos->level, ret);
				ret = 0;
			}
		}
	}

	/* Enable actions */
	list_for_each_entry(pos, &memory_lowpower_handlers, link) {
		if (pos->enable != NULL) {
			ret = pos->enable();
			if (ret) {
				enabled[pos->level] += ret;
				MLPT_PRERR("Fail enable: level[%d] ret[%d]\n", pos->level, ret);
				ret = 0;
			}
		}
	}

	/* Set ENABLE state */
	SetMlpsEnable(&memory_lowpower_state);

	if (IS_ENABLED(CONFIG_MTK_DCS) && !enabled[MLP_LEVEL_DCS])
		SetMlpsEnableDCS(&memory_lowpower_state);

	if (IS_ENABLED(CONFIG_MTK_PASR) && !enabled[MLP_LEVEL_PASR])
		SetMlpsEnablePASR(&memory_lowpower_state);
}

/* Screen-off operations */
static void go_to_screenoff(void)
{
	MLPT_PRINT("%s:+\n", __func__);
	MLPT_START_PROFILE();

	/* Should be SCREENON -> SCREENOFF */
	if (!MlpsScreenOn(&memory_lowpower_state)) {
		MLPT_PRERR("Incomplete state[%lu]\n", memory_lowpower_state);
		goto acquired;
	}

	/*
	 * Try to collect free pages.
	 * If done or can't proceed, just go ahead.
	 */
	if (acquire_memory())
		pr_warn("%s: some exception occurs!\n", __func__);

	/* Action is changed, just leave here. */
	if (!IS_ACTION_SCREENOFF(memory_lowpower_action))
		goto out;

	/* Clear SCREENON state */
	ClearMlpsScreenOn(&memory_lowpower_state);

acquired:
	/* HW-related flow for screenoff */
	__go_to_screenoff();

out:
	MLPT_END_PROFILE();
	MLPT_PRINT("%s:-\n", __func__);
}

/* Screen-idle operations */
static void go_to_screenidle(void)
{
	MLPT_PRINT("%s:+\n", __func__);
	MLPT_START_PROFILE();
	/* Actions for screenidle - TBD */
	MLPT_END_PROFILE();
	MLPT_PRINT("%s:-\n", __func__);
}

/* Acquire wakeup source */
static void acquire_wakelock(void)
{
#ifdef CONFIG_PM_WAKELOCKS
	__pm_stay_awake(&mlp_wakeup);
#endif
}

/* Release wakeup source */
static void release_wakelock(void)
{
#ifdef CONFIG_PM_WAKELOCKS
	__pm_relax(&mlp_wakeup);
#endif
}

/*
 * Main entry for memory lowpower operations -
 * No set_freezable(), no try_to_freeze().
 */
static int memory_lowpower_entry(void *p)
{
	enum power_state current_action = MLP_INIT;

	/* Call freezer_do_not_count to skip me */
	freezer_do_not_count();

	/* Start actions */
	do {
		/* Start running */
		set_current_state(TASK_RUNNING);

		/* Is any action? */
		while (atomic_xchg(&action_changed, MLPT_CLEAR_ACTION) == MLPT_SET_ACTION) {

			/* Acquire wakelock */
			acquire_wakelock();

			/* Take proper actions */
			current_action = memory_lowpower_action;
			switch (current_action) {
			case MLP_SCREENON:
				go_to_screenon();
				break;
			case MLP_SCREENOFF:
				go_to_screenoff();
				break;
			case MLP_SCREENIDLE:
				go_to_screenidle();
				break;
			default:
				MLPT_PRINT("%s: Invalid action[%d]\n", __func__, current_action);
			}

			/* Release wakelock */
			release_wakelock();
		}

		/* Schedule me */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	} while (1);

	return 0;
}

#ifdef CONFIG_MTK_PERIODIC_DATA_COLLECTION
/*
 * periodic_dc_entry
 * Every DATA_COLLECTION_PERIOD ms we check if the free page
 * numbers of ZONE_MOVABLE is less than 90% of total pages of
 * ZONE_MOVABLE.
 */
static int periodic_dc_entry(void *p)
{
	int nid;
	pg_data_t *pgdat;
	struct zone *zone;
	unsigned long free_pages, spanned_pages;
	int trigger;

	do {
		trigger = 0;
		for_each_online_node(nid) {
			pgdat = NODE_DATA(nid);
			zone = &pgdat->node_zones[ZONE_MOVABLE];
			free_pages = zone_page_state(zone, NR_FREE_PAGES);
			spanned_pages = zone->spanned_pages;
			if (free_pages < (spanned_pages / 10 * 9)) {
				trigger = 1;
				break;
			}
		}
		if (trigger) {
			get_memory_lowpower_cma();
			put_memory_lowpower_cma();
			nr_dc++;
		} else
			nr_skip_dc++;
		msleep(DATA_COLLECTION_PERIOD);
	} while (1);

	return 0;
}
#endif /* CONFIG_MTK_PERIODIC_DATA_COLLECTION */

#ifdef CONFIG_PM
/* FB event notifier */
static int memory_lowpower_fb_event(struct notifier_block *notifier, unsigned long event, void *data)
{
	struct fb_event *fb_event = data;
	int *blank = fb_event->data;
	int new_status = *blank ? 1 : 0;
	static unsigned long debounce_time;

	switch (event) {
	case FB_EVENT_BLANK:
		/* Which action */
		if (new_status == 0) {
			MLPT_PRINT("%s: SCREENON!\n", __func__);
			memory_lowpower_action = MLP_SCREENON;
			debounce_time = jiffies + HZ;
		} else {
			MLPT_PRINT("%s: SCREENOFF!\n", __func__);
			/* Check whether we are still in debounce_time before applying SCREENOFF action */
			if (time_before_eq(jiffies, debounce_time)) {
				MLPT_PRINT("%s: Bye SCREENOFF!\n", __func__);
				goto out;
			}
			memory_lowpower_action = MLP_SCREENOFF;
		}

		/* Action is changed */
		atomic_set(&action_changed, MLPT_SET_ACTION);
retry:
		/*
		 * Try to wake it up.
		 * If it is running, to make sure action is cleared for MLP_SCREENON
		 */
		if (!wake_up_process(memory_lowpower_task)) {
			pr_warn("It was already running.\n");
			if (IS_ACTION_SCREENON(memory_lowpower_action) &&
					atomic_read(&action_changed) == MLPT_SET_ACTION) {

				/* SCREENOFF is not finished, just leave. */
				if (MlpsScreenOn(&memory_lowpower_state))
					goto out;

				/* It might be at the time before going to schedule. */
				pr_warn("No action executed for screen-on, retry it.\n");
				goto retry;
			}
		}
	}
out:
	return NOTIFY_DONE;
}

static struct notifier_block fb_notifier_block = {
	.notifier_call = memory_lowpower_fb_event,
	.priority = 0,
};

/* Check whether screenoff action is finished before suspend */
static int memory_lowpower_pm_event(struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (!MlpsEnable(&memory_lowpower_state))
			pr_warn("\n\n\n++++++ %s: screenoff is not finished ++++++\n\n\n", __func__);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block pm_notifier_block = {
	.notifier_call = memory_lowpower_pm_event,
	.priority = 0,
};

static int __init memory_lowpower_init_pm_ops(void)
{
	if (fb_register_client(&fb_notifier_block) != 0)
		return -1;

	if (!register_pm_notifier(&pm_notifier_block))
		pr_warn("%s: failed to register PM notifier!\n", __func__);

#ifdef CONFIG_PM_WAKELOCKS
	wakeup_source_init(&mlp_wakeup, "mlp_wakeup_source");
#endif
	return 0;
}
#endif

int __init memory_lowpower_task_init(void)
{
	int ret = 0;

	/* Is memory lowpower initialized */
	if (!memory_lowpower_inited())
		goto out;

	/* Start a kernel thread */
	memory_lowpower_task = kthread_run(memory_lowpower_entry, NULL, "memory_lowpower_task");
	if (IS_ERR(memory_lowpower_task)) {
		MLPT_PRERR("Failed to start memory_lowpower_task!\n");
		ret = PTR_ERR(memory_lowpower_task);
		goto out;
	}

#ifdef CONFIG_MTK_PERIODIC_DATA_COLLECTION
	periodic_dc_task = kthread_run(periodic_dc_entry, NULL, "periodic_dc_task");
	if (IS_ERR(periodic_dc_task)) {
		MLPT_PRERR("Failed to start periodic_dc_task!\n");
		ret = PTR_ERR(periodic_dc_task);
		goto out;
	}
#endif /* CONFIG_MTK_PERIODIC_DATA_COLLECTION */

#ifdef CONFIG_PM
	/* Initialize PM ops */
	ret = memory_lowpower_init_pm_ops();
	if (ret != 0) {
		MLPT_PRERR("Failed to init pm ops!\n");
		kthread_stop(memory_lowpower_task);
		memory_lowpower_task = NULL;
		goto out;
	}
#endif

	/* Set expected current state */
	SetMlpsInit(&memory_lowpower_state);
	SetMlpsScreenOn(&memory_lowpower_state);

	/* Reset action_changed */
	atomic_set(&action_changed, MLPT_CLEAR_ACTION);
out:
	MLPT_PRINT("%s: memory_power_state[%lu]\n", __func__, memory_lowpower_state);
	return ret;
}

late_initcall(memory_lowpower_task_init);

#ifdef CONFIG_MTK_MEMORY_LOWPOWER_TASK_DEBUG
static int memory_lowpower_task_show(struct seq_file *m, void *v)
{
	/*
	 * At SCREEN-ON, nr_release_memory may be larger than nr_acquire_memory by 1
	 * due to boot-up flow with FB operations.
	 */
	seq_printf(m, "memory lowpower statistics: %lld, %lld, %lld, %lld, %lld\n",
			memory_lowpower_statistics.nr_acquire_memory,
			memory_lowpower_statistics.nr_release_memory,
			memory_lowpower_statistics.nr_full_acquire,
			memory_lowpower_statistics.nr_partial_acquire,
			memory_lowpower_statistics.nr_empty_acquire);
#ifdef CONFIG_MTK_PERIODIC_DATA_COLLECTION
	seq_printf(m, "data collection=%lu, skip=%lu, t=%d(ms)\n",
			nr_dc, nr_skip_dc, DATA_COLLECTION_PERIOD);
#endif /* CONFIG_MTK_PERIODIC_DATA_COLLECTION */

	return 0;
}

static int memory_lowpower_open(struct inode *inode, struct file *file)
{
	return single_open(file, &memory_lowpower_task_show, NULL);
}

static ssize_t memory_lowpower_write(struct file *file, const char __user *buffer,
					size_t count, loff_t *ppos)
{
	static char state;
	struct fb_event fb_event;
	int blank;

	if (count > 0) {
		if (get_user(state, buffer))
			return -EFAULT;
		state -= '0';
		fb_event.data = &blank;

		if (!state) {
			/* collect cma */
			blank = 1;
			memory_lowpower_fb_event(NULL, FB_EVENT_BLANK, &fb_event);
		} else {
			/* undo collection */
			blank = 0;
			memory_lowpower_fb_event(NULL, FB_EVENT_BLANK, &fb_event);
		}
	}

	return count;
}

static const struct file_operations memory_lowpower_task_fops = {
	.open		= memory_lowpower_open,
	.read		= seq_read,
	.write		= memory_lowpower_write,
	.release	= single_release,
};

static int __init memory_lowpower_task_debug_init(void)
{
	struct dentry *dentry;

	dentry = debugfs_create_file("memory-lowpower-task", S_IRUGO, NULL, NULL,
					&memory_lowpower_task_fops);
	if (!dentry)
		pr_warn("Failed to create debugfs memory_lowpower_debug_init file\n");

	return 0;
}

late_initcall(memory_lowpower_task_debug_init);
#endif /* CONFIG_MTK_MEMORY_LOWPOWER_DEBUG */
