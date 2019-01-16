/* kernel/power/earlysuspend.c
 *
 * Copyright (C) 2005-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rtc.h>
#include <linux/syscalls.h>	/* sys_sync */
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include "power.h"

enum {
	DEBUG_USER_STATE = 1U << 0,
	DEBUG_SUSPEND = 1U << 2,
	DEBUG_VERBOSE = 1U << 3,
};
static int debug_mask = DEBUG_USER_STATE | DEBUG_SUSPEND | DEBUG_VERBOSE;
/* static int debug_mask = DEBUG_USER_STATE; */
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

/* int earlysuspend_debug_mask = 0; */
int earlysuspend_debug_mask = DEBUG_USER_STATE;
int early_suspend_count = 0;
int forbid_id = 0x0;
int g_suspend_sys_sync_count = 0;
int wait_sys_sync_flag = 0;

#define _TAG_PM_M "Ker_PM"
#define pm_warn(fmt, ...)	\
	if (earlysuspend_debug_mask) pr_warn("[%s][%s]" fmt, _TAG_PM_M, __func__, ##__VA_ARGS__);

static DEFINE_MUTEX(early_suspend_lock);
static LIST_HEAD(early_suspend_handlers);
static void early_sys_sync(struct work_struct *work);
//void suspend_sys_sync(struct work_struct *work);
static void early_suspend(struct work_struct *work);
static void late_resume(struct work_struct *work);
static void suspend_sys_sync(struct work_struct *work);

static DECLARE_WORK(early_sys_sync_work, early_sys_sync);
//DECLARE_WORK(suspend_sys_sync_work, suspend_sys_sync);
static DECLARE_WORK(early_suspend_work, early_suspend);
static DECLARE_WORK(late_resume_work, late_resume);
static DECLARE_WORK(suspend_sys_sync_work, suspend_sys_sync);

static DEFINE_SPINLOCK(state_lock);

/*  */
struct wake_lock sys_sync_wake_lock;
struct workqueue_struct *suspend_work_queue;
struct workqueue_struct *sys_sync_work_queue;
suspend_state_t requested_suspend_state = PM_SUSPEND_MEM;

enum {
	SUSPEND_REQUESTED = 0x1,
	SUSPENDED = 0x2,
	SUSPEND_REQUESTED_AND_SUSPENDED = SUSPEND_REQUESTED | SUSPENDED,
};
static int state;
static DECLARE_COMPLETION(fb_drv_ready);
static DECLARE_COMPLETION(sys_sync_done);

void register_early_suspend(struct early_suspend *handler)
{
	struct list_head *pos;

	mutex_lock(&early_suspend_lock);
	list_for_each(pos, &early_suspend_handlers) {
		struct early_suspend *e;
		e = list_entry(pos, struct early_suspend, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	early_suspend_count++;
	if ((state & SUSPENDED) && handler->suspend)
		handler->suspend(handler);
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(register_early_suspend);

void unregister_early_suspend(struct early_suspend *handler)
{
	mutex_lock(&early_suspend_lock);
	list_del(&handler->link);
	early_suspend_count--;
	mutex_unlock(&early_suspend_lock);
}
EXPORT_SYMBOL(unregister_early_suspend);

static void early_sys_sync(struct work_struct *work)
{
	wake_lock(&sys_sync_wake_lock);
	pm_warn("++\n");
	sys_sync();
	pm_warn("--\n");
	wake_unlock(&sys_sync_wake_lock);
}

static void suspend_sys_sync(struct work_struct *work)
{
    pm_warn("++\n");
    sys_sync();
if (wait_sys_sync_flag)
	complete(&sys_sync_done);
	wait_sys_sync_flag = 0;
    pm_warn("--\n");
}



static void early_suspend(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0, count = 0;

	pr_warn("@@@@@@@@@@@@@@@@@@@@@@@\n@@@__early_suspend__@@@\n@@@@@@@@@@@@@@@@@@@@@@@\n");

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPEND_REQUESTED)
		state |= SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (earlysuspend_debug_mask & DEBUG_SUSPEND)
			pm_warn("abort, state %d\n", state);
		mutex_unlock(&early_suspend_lock);
		goto abort;
	}

	pr_warn("early_suspend_count = %d, forbid_id = 0x%x\n", early_suspend_count, forbid_id);
	if (earlysuspend_debug_mask & DEBUG_SUSPEND)
		pm_warn("call handlers\n");
	list_for_each_entry(pos, &early_suspend_handlers, link) {
		if (pos->suspend != NULL) {
			if (!(forbid_id & (0x1 << count))) {
				/* if (earlysuspend_debug_mask & DEBUG_VERBOSE) */
				pr_warn("ES handlers %d: [%pf], level: %d\n", count, pos->suspend,
					pos->level);
				pos->suspend(pos);
			}
			count++;
		}
	}
	mutex_unlock(&early_suspend_lock);

	/* Remove sys_sync from early_suspend, and use work queue to complete sys_sync */

 abort:
	if (state == SUSPEND_REQUESTED_AND_SUSPENDED) {
		/* wake_unlock(&main_wake_lock); */
#ifdef CONFIG_MTK_HIBERNATION
		suspend_state_t susp_state = get_suspend_state();
		pm_warn("calling pm_autosleep_set_state() with parameter: %d\n", susp_state);
		pm_autosleep_set_state(susp_state);
#else
		pm_autosleep_set_state(PM_SUSPEND_MEM);
#endif
	}
}

static void late_resume(struct work_struct *work)
{
	struct early_suspend *pos;
	unsigned long irqflags;
	int abort = 0;
	int completed = 0, count = 0;

	pr_warn("@@@@@@@@@@@@@@@@@@@@@@@\n@@@__late_resume__@@@\n@@@@@@@@@@@@@@@@@@@@@@@\n");
	
#if 0
    if (wait_sys_sync_flag)
    {
    	complete(&sys_sync_done);
    }
#endif
	
	pm_autosleep_set_state(PM_SUSPEND_ON);

	mutex_lock(&early_suspend_lock);
	spin_lock_irqsave(&state_lock, irqflags);
	if (state == SUSPENDED)
		state &= ~SUSPENDED;
	else
		abort = 1;
	spin_unlock_irqrestore(&state_lock, irqflags);

	if (abort) {
		if (earlysuspend_debug_mask & DEBUG_SUSPEND)
			pm_warn("abort, state %d\n", state);
		goto abort;
	}
	pr_warn("early_suspend_count = %d, forbid_id = 0x%x\n", early_suspend_count, forbid_id);
	if (earlysuspend_debug_mask & DEBUG_SUSPEND)
		pm_warn("call handlers\n");
	list_for_each_entry_reverse(pos, &early_suspend_handlers, link) {
		if (!completed && pos->level < EARLY_SUSPEND_LEVEL_STOP_DRAWING) {
			complete(&fb_drv_ready);
			completed = 1;
		}
		if (pos->resume != NULL) {
			if (!(forbid_id & (0x1 << (early_suspend_count - count - 1)))) {
				/* if (earlysuspend_debug_mask & DEBUG_VERBOSE) */
				pr_warn("LR handlers %d: [%pf], level: %d\n", count, pos->resume,
					pos->level);
				pos->resume(pos);
			}
			count++;
		}
	}
	if (earlysuspend_debug_mask & DEBUG_SUSPEND)
		pm_warn("done\n");
 abort:
	if (!completed)
		complete(&fb_drv_ready);
	mutex_unlock(&early_suspend_lock);
}

void request_suspend_state(suspend_state_t new_state)
{
	unsigned long irqflags;
	int old_sleep;
	int wait_flag = 0;

	spin_lock_irqsave(&state_lock, irqflags);
	old_sleep = state & SUSPEND_REQUESTED;
	if (earlysuspend_debug_mask & DEBUG_USER_STATE) {
		struct timespec ts;
		struct rtc_time tm;
		getnstimeofday(&ts);
		rtc_time_to_tm(ts.tv_sec, &tm);
		pm_warn("%s (%d->%d) at %lld "
			"(%d-%02d-%02d %02d:%02d:%02d.%09lu UTC)\n",
			new_state != PM_SUSPEND_ON ? "sleep" : "wakeup",
			requested_suspend_state, new_state,
			ktime_to_ns(ktime_get()),
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec, ts.tv_nsec);
	}
	if (!old_sleep && new_state != PM_SUSPEND_ON) {
		state |= SUSPEND_REQUESTED;
		pm_warn("sys_sync_work_queue early_sys_sync_work\n");
		queue_work(sys_sync_work_queue, &early_sys_sync_work);
		pm_warn("suspend_work_queue early_suspend_work\n");
		queue_work(suspend_work_queue, &early_suspend_work);
	} else if (old_sleep && new_state == PM_SUSPEND_ON) {
		state &= ~SUSPEND_REQUESTED;
		/* wake_lock(&main_wake_lock); */
		/* /cun */
		if (queue_work(suspend_work_queue, &late_resume_work)) {
			/*
			 * In order to synchronize the backlight turn on timing,
			 * block the thread and wait for fb driver late_resume()
			 * callback function is completed
			 */
			wait_flag = 1;
		}
	}
	requested_suspend_state = new_state;
	spin_unlock_irqrestore(&state_lock, irqflags);
	if (wait_flag == 1) {
		wait_for_completion(&fb_drv_ready);
		pr_warn("wait done\n");
	}
}

suspend_state_t get_suspend_state(void)
{
	return requested_suspend_state;
}

/* cun */
static int __init org_wakelocks_init(void)
{
	int ret;

	wake_lock_init(&sys_sync_wake_lock, WAKE_LOCK_SUSPEND, "sys_sync");

	sys_sync_work_queue = create_singlethread_workqueue("fs_sync");
	if (sys_sync_work_queue == NULL) {
		pr_err("[wakelocks_init] fs_sync workqueue create failed\n");
	}

	suspend_work_queue = create_singlethread_workqueue("suspend");
	if (suspend_work_queue == NULL) {
		ret = -ENOMEM;
		goto err_suspend_work_queue;
	}
	return 0;

 err_suspend_work_queue:

	return ret;
}

static void __exit org_wakelocks_exit(void)
{
	destroy_workqueue(suspend_work_queue);
}
//[MTK]
void suspend_syssync_enqueue(void)  
{  
    spin_lock(&state_lock);
    wait_sys_sync_flag = 1; 
	  queue_work(sys_sync_work_queue, &suspend_sys_sync_work);
    spin_unlock(&state_lock);  
	return;
}  


void suspend_check_sys_sync_done(void)
{
	if(wait_sys_sync_flag)
	   wait_for_completion(&sys_sync_done);
	wait_sys_sync_flag = 0;
	return;
}

core_initcall(org_wakelocks_init);
module_exit(org_wakelocks_exit);
