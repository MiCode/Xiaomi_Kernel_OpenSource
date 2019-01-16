/*
 * kernel/power/autosleep.c
 *
 * Opportunistic sleep support.
 *
 * Copyright (C) 2012 Rafael J. Wysocki <rjw@sisk.pl>
 */

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>

#include "power.h"

//<20130327> <marc.huang> add autosleep dubug log
#define _TAG_AUTOSLEEP "AUTOSLEEP"
#define autosleep_log(fmt, ...)    pr_debug("[%s][%s]" fmt, _TAG_AUTOSLEEP, __func__, ##__VA_ARGS__)
#define autosleep_warn(fmt, ...)   pr_warn("[%s][%s]" fmt, _TAG_AUTOSLEEP, __func__, ##__VA_ARGS__)

#define HIB_AUTOSLEEP_DEBUG 1
#define _TAG_HIB_M "HIB/AUTOSLEEP"
#if (HIB_AUTOSLEEP_DEBUG)
#define hib_autoslp_log(fmt, ...)   pr_warn("[%s][%s]" fmt, _TAG_HIB_M, __func__, ##__VA_ARGS__);
#else
#define hib_autoslp_log(fmt, ...)
#endif
#define hib_autoslp_warn(fmt, ...)   pr_warn("[%s][%s]" fmt, _TAG_HIB_M, __func__, ##__VA_ARGS__);


static suspend_state_t autosleep_state;
static struct workqueue_struct *autosleep_wq;
/*
 * Note: it is only safe to mutex_lock(&autosleep_lock) if a wakeup_source
 * is active, otherwise a deadlock with try_to_suspend() is possible.
 * Alternatively mutex_lock_interruptible() can be used.  This will then fail
 * if an auto_sleep cycle tries to freeze processes.
 */
static DEFINE_MUTEX(autosleep_lock);
static struct wakeup_source *autosleep_ws;

#ifdef CONFIG_MTK_HIBERNATION
extern bool system_is_hibernating;
extern int mtk_hibernate_via_autosleep(suspend_state_t *autoslp_state);
#endif
static void try_to_suspend(struct work_struct *work)
{
	unsigned int initial_count, final_count;

	//<20130327> <marc.huang> add autosleep dubug log
	autosleep_log("pm_get_wakeup_count\n");
	if (!pm_get_wakeup_count(&initial_count, true))
		goto out;

	mutex_lock(&autosleep_lock);

	//<20130327> <marc.huang> add autosleep dubug log
	autosleep_log("pm_save_wakeup_count\n");
	if (!pm_save_wakeup_count(initial_count) ||
		system_state != SYSTEM_RUNNING) {
		mutex_unlock(&autosleep_lock);
		goto out;
	}

	if (autosleep_state == PM_SUSPEND_ON) {
#ifdef CONFIG_MTK_HIBERNATION
        system_is_hibernating = false;
#endif
		//<20130327> <marc.huang> add autosleep dubug log
		autosleep_warn("abort due to autosleep_state: %d\n", autosleep_state);
		mutex_unlock(&autosleep_lock);
		return;
	}
#ifdef CONFIG_MTK_HIBERNATION
    if (autosleep_state >= PM_SUSPEND_MAX) {
        mtk_hibernate_via_autosleep(&autosleep_state);
    }
    else {
        hib_autoslp_log("pm_suspend: state(%d)\n", autosleep_state);
        if (!system_is_hibernating) {
            hib_autoslp_warn("calling pm_suspend() state(%d)\n", autosleep_state);
            pm_suspend(autosleep_state);
        }
        else {
            hib_autoslp_warn("system is hibernating: so changing state(%d->%d)\n",  autosleep_state, PM_SUSPEND_MAX);
            autosleep_state = PM_SUSPEND_MAX;
        }
    }
#else // !CONFIG_MTK_HIBERNATION
	if (autosleep_state >= PM_SUSPEND_MAX)
		hibernate();
	else
        {
		//<20130327> <marc.huang> add autosleep dubug log
		autosleep_log("pm_suspend, autosleep_state: %d\n", autosleep_state);
		pm_suspend(autosleep_state);
        }
#endif // CONFIG_MTK_HIBERNATION
	mutex_unlock(&autosleep_lock);

	if (!pm_get_wakeup_count(&final_count, false))
		goto out;

	/*
	 * If the wakeup occured for an unknown reason, wait to prevent the
	 * system from trying to suspend and waking up in a tight loop.
	 */
	if (final_count == initial_count)
		schedule_timeout_uninterruptible(HZ / 2);

 out:
	//<20130327> <marc.huang> add autosleep dubug log
	autosleep_log("queue_up_suspend_work again\n");
	queue_up_suspend_work();
}

static DECLARE_WORK(suspend_work, try_to_suspend);

void queue_up_suspend_work(void)
{
	if (autosleep_state > PM_SUSPEND_ON)
        {
		//<20130327> <marc.huang> add autosleep dubug log
		autosleep_log("autosleep_state: %d\n", autosleep_state);
		queue_work(autosleep_wq, &suspend_work);
        }
}

suspend_state_t pm_autosleep_state(void)
{
	return autosleep_state;
}

int pm_autosleep_lock(void)
{
	return mutex_lock_interruptible(&autosleep_lock);
}

void pm_autosleep_unlock(void)
{
	mutex_unlock(&autosleep_lock);
}

int pm_autosleep_set_state(suspend_state_t state)
{

#ifndef CONFIG_HIBERNATION
	if (state >= PM_SUSPEND_MAX)
		return -EINVAL;
#endif

	__pm_stay_awake(autosleep_ws);

	mutex_lock(&autosleep_lock);

	autosleep_state = state;

	__pm_relax(autosleep_ws);

	if (state > PM_SUSPEND_ON) {
		//<20130327> <marc.huang> add autosleep dubug log
		autosleep_log("pm_wakep_autosleep_enabled(true)\n");
		pm_wakep_autosleep_enabled(true);
		queue_up_suspend_work();
	} else {
		//<20130327> <marc.huang> add autosleep dubug log
		autosleep_log("pm_wakep_autosleep_enabled(false)\n");
		pm_wakep_autosleep_enabled(false);
	}

	mutex_unlock(&autosleep_lock);
	return 0;
}

int __init pm_autosleep_init(void)
{
	autosleep_ws = wakeup_source_register("autosleep");
	if (!autosleep_ws)
		return -ENOMEM;

	autosleep_wq = alloc_ordered_workqueue("autosleep", 0);
	if (autosleep_wq)
		return 0;

	wakeup_source_unregister(autosleep_ws);
	return -ENOMEM;
}
