/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*
 * Qualcomm MSM Runqueue Stats Interface for Userspace
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/cpu.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

struct rq_data {
	unsigned int rq_avg;
	unsigned int rq_poll_ms;
	unsigned int def_timer_ms;
	unsigned int def_interval;
	int64_t last_time;
	int64_t total_time;
	int64_t def_start_time;
	struct delayed_work rq_work;
	struct attribute_group *attr_group;
	struct kobject *kobj;
	struct delayed_work def_timer_work;
};

static struct rq_data rq_info;
static DEFINE_SPINLOCK(rq_lock);
static struct workqueue_struct *rq_wq;

static void rq_work_fn(struct work_struct *work)
{
	int64_t time_diff = 0;
	int64_t rq_avg = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);

	if (!rq_info.last_time)
		rq_info.last_time = ktime_to_ns(ktime_get());
	if (!rq_info.rq_avg)
		rq_info.total_time = 0;

	rq_avg = nr_running() * 10;
	time_diff = ktime_to_ns(ktime_get()) - rq_info.last_time;
	do_div(time_diff, (1000 * 1000));

	if (time_diff && rq_info.total_time) {
		rq_avg = (rq_avg * time_diff) +
			(rq_info.rq_avg * rq_info.total_time);
		do_div(rq_avg, rq_info.total_time + time_diff);
	}

	rq_info.rq_avg =  (unsigned int)rq_avg;

	/* Set the next poll */
	if (rq_info.rq_poll_ms)
		queue_delayed_work(rq_wq, &rq_info.rq_work,
			msecs_to_jiffies(rq_info.rq_poll_ms));

	rq_info.total_time += time_diff;
	rq_info.last_time = ktime_to_ns(ktime_get());

	spin_unlock_irqrestore(&rq_lock, flags);
}

static void def_work_fn(struct work_struct *work)
{
	int64_t diff;

	diff = ktime_to_ns(ktime_get()) - rq_info.def_start_time;
	do_div(diff, 1000 * 1000);
	rq_info.def_interval = (unsigned int) diff;

	/* Notify polling threads on change of value */
	sysfs_notify(rq_info.kobj, NULL, "def_timer_ms");
}

static ssize_t show_run_queue_avg(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	unsigned int val = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	/* rq avg currently available only on one core */
	val = rq_info.rq_avg;
	rq_info.rq_avg = 0;
	spin_unlock_irqrestore(&rq_lock, flags);

	return sprintf(buf, "%d.%d\n", val/10, val%10);
}

static ssize_t show_run_queue_poll_ms(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&rq_lock, flags);
	ret = sprintf(buf, "%u\n", rq_info.rq_poll_ms);
	spin_unlock_irqrestore(&rq_lock, flags);

	return ret;
}

static ssize_t store_run_queue_poll_ms(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int val = 0;
	unsigned long flags = 0;
	static DEFINE_MUTEX(lock_poll_ms);

	mutex_lock(&lock_poll_ms);

	spin_lock_irqsave(&rq_lock, flags);
	sscanf(buf, "%u", &val);
	rq_info.rq_poll_ms = val;
	spin_unlock_irqrestore(&rq_lock, flags);

	if (val <= 0)
		cancel_delayed_work(&rq_info.rq_work);
	else
		queue_delayed_work(rq_wq, &rq_info.rq_work,
				msecs_to_jiffies(val));

	mutex_unlock(&lock_poll_ms);

	return count;
}

static ssize_t show_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", rq_info.def_interval);
}

static ssize_t store_def_timer_ms(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int val = 0;

	sscanf(buf, "%u", &val);
	rq_info.def_timer_ms = val;

	if (val <= 0)
		cancel_delayed_work(&rq_info.def_timer_work);
	else {
		rq_info.def_start_time = ktime_to_ns(ktime_get());
		queue_delayed_work(rq_wq, &rq_info.def_timer_work,
				msecs_to_jiffies(val));
	}

	return count;
}

#define MSM_RQ_STATS_RO_ATTRIB(att) ({ \
		struct attribute *attrib = NULL; \
		struct kobj_attribute *ptr = NULL; \
		ptr = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL); \
		if (ptr) { \
			ptr->attr.name = #att; \
			ptr->attr.mode = S_IRUGO; \
			ptr->show = show_##att; \
			ptr->store = NULL; \
			attrib = &ptr->attr; \
		} \
		attrib; })

#define MSM_RQ_STATS_RW_ATTRIB(att) ({ \
		struct attribute *attrib = NULL; \
		struct kobj_attribute *ptr = NULL; \
		ptr = kzalloc(sizeof(struct kobj_attribute), GFP_KERNEL); \
		if (ptr) { \
			ptr->attr.name = #att; \
			ptr->attr.mode = S_IWUSR|S_IRUSR; \
			ptr->show = show_##att; \
			ptr->store = store_##att; \
			attrib = &ptr->attr; \
		} \
		attrib; })

static int init_rq_attribs(void)
{
	int i;
	int err = 0;
	const int attr_count = 4;

	struct attribute **attribs =
		kzalloc(sizeof(struct attribute *) * attr_count, GFP_KERNEL);

	if (!attribs)
		goto rel;

	rq_info.rq_avg = 0;
	rq_info.rq_poll_ms = 0;

	attribs[0] = MSM_RQ_STATS_RW_ATTRIB(def_timer_ms);
	attribs[1] = MSM_RQ_STATS_RO_ATTRIB(run_queue_avg);
	attribs[2] = MSM_RQ_STATS_RW_ATTRIB(run_queue_poll_ms);
	attribs[3] = NULL;

	for (i = 0; i < attr_count - 1 ; i++) {
		if (!attribs[i])
			goto rel;
	}

	rq_info.attr_group = kzalloc(sizeof(struct attribute_group),
						GFP_KERNEL);
	if (!rq_info.attr_group)
		goto rel;
	rq_info.attr_group->attrs = attribs;

	/* Create /sys/devices/system/cpu/cpu0/rq-stats/... */
	rq_info.kobj = kobject_create_and_add("rq-stats",
			&get_cpu_sysdev(0)->kobj);
	if (!rq_info.kobj)
		goto rel;

	err = sysfs_create_group(rq_info.kobj, rq_info.attr_group);
	if (err)
		kobject_put(rq_info.kobj);
	else
		kobject_uevent(rq_info.kobj, KOBJ_ADD);

	if (!err)
		return err;

rel:
	for (i = 0; i < attr_count - 1 ; i++)
		kfree(attribs[i]);
	kfree(attribs);
	kfree(rq_info.attr_group);
	kfree(rq_info.kobj);

	return -ENOMEM;
}

static int __init msm_rq_stats_init(void)
{
	rq_wq = create_singlethread_workqueue("rq_stats");
	BUG_ON(!rq_wq);
	INIT_DELAYED_WORK_DEFERRABLE(&rq_info.rq_work, rq_work_fn);
	INIT_DELAYED_WORK_DEFERRABLE(&rq_info.def_timer_work, def_work_fn);
	return init_rq_attribs();
}
late_initcall(msm_rq_stats_init);
