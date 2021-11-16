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

#include <linux/init.h>		/* For init/exit macros */
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/list.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/suspend.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include "mtk_gauge_time_service.h"


static struct list_head gtimer_head = LIST_HEAD_INIT(gtimer_head);

static bool gtimer_thread_timeout;
static int ftlog_level;

static struct mutex gtimer_lock;
static spinlock_t slock;
static struct wakeup_source wlock;
static wait_queue_head_t wait_que;
static struct hrtimer gtimer_kthread_timer;
static struct timespec gtimer_suspend_time;

#define FTLOG_ERROR_LEVEL   1
#define FTLOG_DEBUG_LEVEL   2
#define FTLOG_TRACE_LEVEL   3

#define ft_err(fmt, args...)   \
do {									\
	if (ftlog_level >= FTLOG_ERROR_LEVEL) {			\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define ft_debug(fmt, args...)   \
do {									\
	if (ftlog_level >= FTLOG_DEBUG_LEVEL) {		\
		pr_notice(fmt, ##args); \
	}								   \
} while (0)

#define ft_trace(fmt, args...)\
do {									\
	if (ftlog_level >= FTLOG_TRACE_LEVEL) {			\
		pr_notice(fmt, ##args);\
	}						\
} while (0)

#define ft_info(fmt, args...)\
do { \
} while (0)


void mutex_gtimer_lock(void)
{
	mutex_lock(&gtimer_lock);
}

void mutex_gtimer_unlock(void)
{
	mutex_unlock(&gtimer_lock);
}

void gtimer_set_log_level(int x)
{
	ftlog_level = x;
}

void gtimer_dump_list(void)
{
	struct list_head *pos;
	struct list_head *phead = &gtimer_head;
	struct gtimer *ptr;
	struct timespec time_now;

	mutex_gtimer_lock();
	get_monotonic_boottime(&time_now);

	ft_debug("dump gtimer list start %ld\n", time_now.tv_sec);
	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gtimer, list);
		ft_debug("dump list name:%s time:%ld int:%d\n", ptr->name,
		ptr->endtime.tv_sec, ptr->interval);
	}
	ft_debug("dump list end\n");
	mutex_gtimer_unlock();
}

void wake_up_gtimer(void)
{
	unsigned long flags;

	spin_lock_irqsave(&slock, flags);
	if (wlock.active == 0)
		__pm_stay_awake(&wlock);
	spin_unlock_irqrestore(&slock, flags);

	gtimer_thread_timeout = true;
	wake_up(&wait_que);
	ft_debug("%s\n", __func__);
}

void gtimer_start_timer(int sec)
{
	ktime_t ktime = ktime_set(sec, 0);

	ft_debug("%s %d",
		__func__,
		sec);
	hrtimer_start(&gtimer_kthread_timer, ktime, HRTIMER_MODE_REL);
}

void gtimer_init(struct gtimer *timer, struct device *dev, char *name)
{
	timer->name = name;
	INIT_LIST_HEAD(&timer->list);
	timer->dev = dev;
}

void gtimer_start(struct gtimer *timer, int sec)
{
	struct list_head *pos;
	struct list_head *phead = &gtimer_head;
	struct gtimer *ptr;
	struct timespec time, time_now;
	bool wakeup = false;
	int time_interval;

	mutex_gtimer_lock();

	hrtimer_cancel(&gtimer_kthread_timer);

	time.tv_sec = sec;
	time.tv_nsec = 0;
	timer->interval = sec;

	get_monotonic_boottime(&time_now);

	timer->endtime = timespec_add(time_now, time);

	ft_debug("%s dev:%s name:%s %ld %ld %d\n",
		__func__,
	dev_name(timer->dev), timer->name,
	time_now.tv_sec, timer->endtime.tv_sec, sec);

	if (list_empty(&timer->list) != true) {
		ft_debug("%s dev:%s name:%s time:%ld %ld int:%d is not empty\n",
			__func__,
		dev_name(timer->dev), timer->name,
		time_now.tv_sec, timer->endtime.tv_sec, sec);
		list_del_init(&timer->list);
	}

	list_for_each(pos, phead) {
		ptr = container_of(pos, struct gtimer, list);
		if (timespec_compare(&timer->endtime, &ptr->endtime) < 0)
			break;
	}

	list_add(&timer->list, pos->prev);

	pos = gtimer_head.next;
	if (list_empty(pos) != true) {
		ptr = container_of(pos, struct gtimer, list);
		if (timespec_compare(&ptr->endtime, &time_now) < 0)
			wakeup = true;
		else {
			time = timespec_sub(ptr->endtime, time_now);
			if (time.tv_sec < 1)
				time_interval = 1;
			else
				time_interval = time.tv_sec;
			gtimer_start_timer(time_interval);
		}
	}
	mutex_gtimer_unlock();

	if (wakeup == true)
		wake_up_gtimer();

}

void gtimer_stop(struct gtimer *timer)
{
	mutex_gtimer_lock();
	if (list_empty(&timer->list) != true)
		list_del_init(&timer->list);
	mutex_gtimer_unlock();
}


static void gtimer_handler(void)
{
	struct list_head *pos = gtimer_head.next;
	struct list_head *phead = &gtimer_head;
	struct gtimer *ptr;
	struct timespec time;
	int time_interval;

	hrtimer_cancel(&gtimer_kthread_timer);

	ft_info("%s\n", __func__);
	for (pos = phead->next; pos != phead;) {
		struct list_head *ptmp;

		get_monotonic_boottime(&time);
		ptr = container_of(pos, struct gtimer, list);

		ft_info("%s name:%s %ld %ld %d %d\n",
			__func__,
		ptr->name, time.tv_sec,
		ptr->endtime.tv_sec, ptr->interval,
		timespec_compare(&time, &ptr->endtime));

		if (timespec_compare(&time, &ptr->endtime) >= 0) {
			ptmp = pos;
			pos = pos->next;
			list_del_init(ptmp);
			ft_debug("%s name:%s %ld %d\n",
				__func__,
			ptr->name,
			ptr->endtime.tv_sec, ptr->interval);
			if (ptr->callback) {
				mutex_gtimer_unlock();
				ptr->callback(ptr);
				mutex_gtimer_lock();
				pos = gtimer_head.next;
			}
		} else
			pos = pos->next;
	}

	pos = gtimer_head.next;
	if (list_empty(pos) != true) {
		ptr = container_of(pos, struct gtimer, list);

		time = timespec_sub(ptr->endtime, time);
		if (time.tv_sec < 1)
			time_interval = 1;
		else
			time_interval = time.tv_sec;
		gtimer_start_timer(time_interval);
	}
}


static int gtimer_thread(void *arg)
{
	unsigned long flags;
	struct timespec stime, endtime, duraction;

	while (1) {
		wait_event(wait_que, (gtimer_thread_timeout == true));
		gtimer_thread_timeout = false;
		get_monotonic_boottime(&stime);
		mutex_gtimer_lock();

		gtimer_handler();

		spin_lock_irqsave(&slock, flags);
		__pm_relax(&wlock);
		spin_unlock_irqrestore(&slock, flags);

		mutex_gtimer_unlock();
		get_monotonic_boottime(&endtime);
		duraction = timespec_sub(endtime, stime);
		if (duraction.tv_sec == -56789)
			return 0;
	}

	return 0;
}


static void gtimer_suspend(void)
{
	ft_err("%s\n", __func__);
	hrtimer_cancel(&gtimer_kthread_timer);
}

static void gtimer_resume(void)
{
	struct list_head *pos = gtimer_head.next;
	struct gtimer *ptr;
	struct timespec time, diff;
	int time_interval;

	get_monotonic_boottime(&time);
	ft_err("%s %ld\n",
		__func__,
		time.tv_sec);
	gtimer_dump_list();

	pos = gtimer_head.next;
	if (list_empty(pos) != true) {
		ptr = container_of(pos, struct gtimer, list);

		if (timespec_compare(&time, &ptr->endtime) >= 0) {
			ft_err("%s now:%ld expired:%s %ld\n",
				__func__,
				time.tv_sec,
				ptr->name, ptr->endtime.tv_sec);
			wake_up_gtimer();
		} else {
			diff = timespec_sub(ptr->endtime, time);
			if (diff.tv_sec < 1)
				time_interval = 1;
			else
				time_interval = diff.tv_sec;
			gtimer_start_timer(time_interval);
		}
	}

}

static int gtimer_pm_event(
	struct notifier_block *notifier, unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:	/* Going to hibernate */
	case PM_RESTORE_PREPARE:	/* Going to restore a saved image */
	case PM_SUSPEND_PREPARE:	/* Going to suspend the system */
		get_monotonic_boottime(&gtimer_suspend_time);
		ft_err("[%s] pm_event %lu %ld\n",
			__func__, pm_event,
			gtimer_suspend_time.tv_sec);
		gtimer_suspend();
		return NOTIFY_DONE;

	case PM_POST_SUSPEND:	/* Suspend finished */
	case PM_POST_RESTORE:	/* Restore failed */
		ft_err("[%s] pm_event %lu\n", __func__, pm_event);
		gtimer_resume();
		return NOTIFY_DONE;

	case PM_POST_HIBERNATION:	/* Hibernation finished */
		ft_err("[%s] pm_event %lu\n", __func__, pm_event);


		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static struct notifier_block gtimer_pm_notifier_block = {
	.notifier_call = gtimer_pm_event,
	.priority = 0,
};

enum hrtimer_restart gtimer_kthread_hrtimer_func(struct hrtimer *timer)
{
	wake_up_gtimer();
	return HRTIMER_NORESTART;
}

signed int get_dynamic_period(
	int first_use, int first_wakeup_time, int battery_capacity_level)
{
	struct timespec duraction;
	struct list_head *pos = gtimer_head.next;
	struct gtimer *ptr;
	signed int sec = 4800;

	pos = gtimer_head.next;
	if (list_empty(pos) != true) {
		ptr = container_of(pos, struct gtimer, list);

		duraction = timespec_sub(ptr->endtime, gtimer_suspend_time);
		sec = duraction.tv_sec + 1;
		if (sec <= 10)
			sec = 10;
		ft_err("%s time:now:%ld next:%ld diff:%d\n",
			__func__,
		gtimer_suspend_time.tv_sec, ptr->endtime.tv_sec, sec);
	} else
		ft_err("%s time:%d\n",
		__func__,
		sec);

	return sec;
}


static int gauge_timer_service_probe(struct platform_device *pdev)
{
	mutex_init(&gtimer_lock);
	spin_lock_init(&slock);
	wakeup_source_init(&wlock, "gtime timer wakelock");
	init_waitqueue_head(&wait_que);


	hrtimer_init(&gtimer_kthread_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gtimer_kthread_timer.function = gtimer_kthread_hrtimer_func;

	kthread_run(gtimer_thread, NULL, "gauge_timer_thread");

	register_pm_notifier(&gtimer_pm_notifier_block);

	return 0;
}

static int gauge_timer_service_remove(struct platform_device *pdev)
{
	struct mt6355_gauge *mt = platform_get_drvdata(pdev);

	if (mt)
		devm_kfree(&pdev->dev, mt);
	return 0;
}

static void gauge_timer_service_shutdown(struct platform_device *dev)
{
}


static const struct of_device_id gauge_timer_service_of_match[] = {
	{.compatible = "mediatek,gauge_timer_service",},
	{},
};

MODULE_DEVICE_TABLE(of, gauge_timer_service_of_match);

static struct platform_driver gauge_timer_service_driver = {
	.probe = gauge_timer_service_probe,
	.remove = gauge_timer_service_remove,
	.shutdown = gauge_timer_service_shutdown,
	.driver = {
		   .name = "gauge_timer_service",
		   .of_match_table = gauge_timer_service_of_match,
		   },
};

static int __init gauge_timer_service_init(void)
{
	return platform_driver_register(&gauge_timer_service_driver);
}
device_initcall(gauge_timer_service_init);

static void __exit gauge_timer_service_exit(void)
{
	platform_driver_unregister(&gauge_timer_service_driver);
}
module_exit(gauge_timer_service_exit);

MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_DESCRIPTION("MTK Gauge time service Driver");
MODULE_LICENSE("GPL");

