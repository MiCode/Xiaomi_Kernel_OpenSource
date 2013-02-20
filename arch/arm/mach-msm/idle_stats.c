/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/uaccess.h>

#include "idle_stats.h"
#include <mach/cpuidle.h>

/******************************************************************************
 * Debug Definitions
 *****************************************************************************/

enum {
	MSM_IDLE_STATS_DEBUG_API = BIT(0),
	MSM_IDLE_STATS_DEBUG_SIGNAL = BIT(1),
	MSM_IDLE_STATS_DEBUG_MIGRATION = BIT(2),
};

static int msm_idle_stats_debug_mask;
module_param_named(
	debug_mask, msm_idle_stats_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

/******************************************************************************
 * Driver Definitions
 *****************************************************************************/

#define MSM_IDLE_STATS_DRIVER_NAME "msm_idle_stats"

static dev_t msm_idle_stats_dev_nr;
static struct cdev msm_idle_stats_cdev;
static struct class *msm_idle_stats_class;

/******************************************************************************
 * Device Definitions
 *****************************************************************************/

struct msm_idle_stats_device {
	unsigned int cpu;
	struct mutex mutex;
	struct notifier_block notifier;

	int64_t collection_expiration;
	struct msm_idle_stats stats;
	struct hrtimer timer;

	wait_queue_head_t wait_q;
	atomic_t collecting;
};

static DEFINE_SPINLOCK(msm_idle_stats_devs_lock);
static DEFINE_PER_CPU(struct msm_idle_stats_device *, msm_idle_stats_devs);

/******************************************************************************
 *
 *****************************************************************************/

static inline int64_t msm_idle_stats_bound_interval(int64_t interval)
{
	if (interval <= 0)
		return 1;

	if (interval > UINT_MAX)
		return UINT_MAX;

	return interval;
}

static enum hrtimer_restart msm_idle_stats_timer(struct hrtimer *timer)
{
	struct msm_idle_stats_device *stats_dev;
	unsigned int cpu;
	int64_t now;
	int64_t interval;

	stats_dev = container_of(timer, struct msm_idle_stats_device, timer);
	cpu = get_cpu();

	if (cpu != stats_dev->cpu) {
		if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_MIGRATION)
			pr_info("%s: timer migrated from cpu%u to cpu%u\n",
				__func__, stats_dev->cpu, cpu);

		stats_dev->stats.event = MSM_IDLE_STATS_EVENT_TIMER_MIGRATED;
		goto timer_exit;
	}

	now = ktime_to_us(ktime_get());
	interval = now - stats_dev->stats.last_busy_start;

	if (stats_dev->stats.busy_timer > 0 &&
			interval >= stats_dev->stats.busy_timer - 1)
		stats_dev->stats.event =
			MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED;
	else
		stats_dev->stats.event =
			MSM_IDLE_STATS_EVENT_COLLECTION_TIMER_EXPIRED;

timer_exit:
	atomic_set(&stats_dev->collecting, 0);
	wake_up_interruptible(&stats_dev->wait_q);

	put_cpu();
	return HRTIMER_NORESTART;
}

static void msm_idle_stats_pre_idle(struct msm_idle_stats_device *stats_dev)
{
	int64_t now;
	int64_t interval;

	if (smp_processor_id() != stats_dev->cpu) {
		WARN_ON(1);
		return;
	}

	if (!atomic_read(&stats_dev->collecting))
		return;

	hrtimer_cancel(&stats_dev->timer);

	now = ktime_to_us(ktime_get());
	interval = now - stats_dev->stats.last_busy_start;
	interval = msm_idle_stats_bound_interval(interval);

	stats_dev->stats.busy_intervals[stats_dev->stats.nr_collected]
		= (__u32) interval;
	stats_dev->stats.last_idle_start = now;
}

static void msm_idle_stats_post_idle(struct msm_idle_stats_device *stats_dev)
{
	int64_t now;
	int64_t interval;
	int64_t timer_interval;
	int rc;

	if (smp_processor_id() != stats_dev->cpu) {
		WARN_ON(1);
		return;
	}

	if (!atomic_read(&stats_dev->collecting))
		return;

	now = ktime_to_us(ktime_get());
	interval = now - stats_dev->stats.last_idle_start;
	interval = msm_idle_stats_bound_interval(interval);

	stats_dev->stats.idle_intervals[stats_dev->stats.nr_collected]
		= (__u32) interval;
	stats_dev->stats.nr_collected++;
	stats_dev->stats.last_busy_start = now;

	if (stats_dev->stats.nr_collected >= MSM_IDLE_STATS_NR_MAX_INTERVALS) {
		stats_dev->stats.event = MSM_IDLE_STATS_EVENT_COLLECTION_FULL;
		goto post_idle_collection_done;
	}

	timer_interval = stats_dev->collection_expiration - now;
	if (timer_interval <= 0) {
		stats_dev->stats.event =
			MSM_IDLE_STATS_EVENT_COLLECTION_TIMER_EXPIRED;
		goto post_idle_collection_done;
	}

	if (stats_dev->stats.busy_timer > 0 &&
			timer_interval > stats_dev->stats.busy_timer)
		timer_interval = stats_dev->stats.busy_timer;

	rc = hrtimer_start(&stats_dev->timer,
		ktime_set(0, timer_interval * 1000), HRTIMER_MODE_REL_PINNED);
	WARN_ON(rc);

	return;

post_idle_collection_done:
	atomic_set(&stats_dev->collecting, 0);
	wake_up_interruptible(&stats_dev->wait_q);
}

static int msm_idle_stats_notified(struct notifier_block *nb,
	unsigned long val, void *v)
{
	struct msm_idle_stats_device *stats_dev = container_of(
				nb, struct msm_idle_stats_device, notifier);

	if (val == MSM_CPUIDLE_STATE_EXIT)
		msm_idle_stats_post_idle(stats_dev);
	else
		msm_idle_stats_pre_idle(stats_dev);

	return 0;
}

static int msm_idle_stats_collect(struct file *filp,
				  unsigned int cmd, unsigned long arg)
{
	struct msm_idle_stats_device *stats_dev;
	struct msm_idle_stats *stats;
	int rc;

	stats_dev = (struct msm_idle_stats_device *) filp->private_data;
	stats = &stats_dev->stats;

	rc = mutex_lock_interruptible(&stats_dev->mutex);
	if (rc) {
		if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_SIGNAL)
			pr_info("%s: interrupted while waiting on device "
				"mutex\n", __func__);

		rc = -EINTR;
		goto collect_exit;
	}

	if (atomic_read(&stats_dev->collecting)) {
		pr_err("%s: inconsistent state\n", __func__);
		rc = -EBUSY;
		goto collect_unlock_exit;
	}

	rc = copy_from_user(stats, (void *)arg, sizeof(*stats));
	if (rc) {
		rc = -EFAULT;
		goto collect_unlock_exit;
	}

	if (stats->nr_collected >= MSM_IDLE_STATS_NR_MAX_INTERVALS ||
			stats->busy_timer > MSM_IDLE_STATS_MAX_TIMER ||
			stats->collection_timer > MSM_IDLE_STATS_MAX_TIMER) {
		rc = -EINVAL;
		goto collect_unlock_exit;
	}

	if (get_cpu() != stats_dev->cpu) {
		put_cpu();
		rc = -EACCES;
		goto collect_unlock_exit;
	}

	/*
	 * When collection_timer == 0, stop collecting at the next
	 * post idle.
	 */
	stats_dev->collection_expiration =
		ktime_to_us(ktime_get()) + stats->collection_timer;

	/*
	 * Enable collection before starting any timer.
	 */
	atomic_set(&stats_dev->collecting, 1);

	/*
	 * When busy_timer == 0, do not set any busy timer.
	 */
	if (stats->busy_timer > 0) {
		rc = hrtimer_start(&stats_dev->timer,
			ktime_set(0, stats->busy_timer * 1000),
			HRTIMER_MODE_REL_PINNED);
		WARN_ON(rc);
	}

	put_cpu();
	if (wait_event_interruptible(stats_dev->wait_q,
			!atomic_read(&stats_dev->collecting))) {
		if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_SIGNAL)
			pr_info("%s: interrupted while waiting on "
				"collection\n", __func__);

		hrtimer_cancel(&stats_dev->timer);
		atomic_set(&stats_dev->collecting, 0);

		rc = -EINTR;
		goto collect_unlock_exit;
	}

	stats->return_timestamp = ktime_to_us(ktime_get());

	rc = copy_to_user((void *)arg, stats, sizeof(*stats));
	if (rc) {
		rc = -EFAULT;
		goto collect_unlock_exit;
	}

collect_unlock_exit:
	mutex_unlock(&stats_dev->mutex);

collect_exit:
	return rc;
}

static int msm_idle_stats_open(struct inode *inode, struct file *filp)
{
	struct msm_idle_stats_device *stats_dev;
	int rc;

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: enter\n", __func__);

	rc = nonseekable_open(inode, filp);
	if (rc) {
		pr_err("%s: failed to set nonseekable\n", __func__);
		goto open_bail;
	}

	stats_dev = (struct msm_idle_stats_device *)
			kzalloc(sizeof(*stats_dev), GFP_KERNEL);
	if (!stats_dev) {
		pr_err("%s: failed to allocate device struct\n", __func__);
		rc = -ENOMEM;
		goto open_bail;
	}

	stats_dev->cpu = MINOR(inode->i_rdev);
	mutex_init(&stats_dev->mutex);
	stats_dev->notifier.notifier_call = msm_idle_stats_notified;
	hrtimer_init(&stats_dev->timer,
			CLOCK_MONOTONIC, HRTIMER_MODE_REL_PINNED);
	stats_dev->timer.function = msm_idle_stats_timer;
	init_waitqueue_head(&stats_dev->wait_q);
	atomic_set(&stats_dev->collecting, 0);

	filp->private_data = stats_dev;

	/*
	 * Make sure only one device exists per cpu.
	 */
	spin_lock(&msm_idle_stats_devs_lock);
	if (per_cpu(msm_idle_stats_devs, stats_dev->cpu)) {
		spin_unlock(&msm_idle_stats_devs_lock);
		rc = -EBUSY;
		goto open_free_bail;
	}

	per_cpu(msm_idle_stats_devs, stats_dev->cpu) = stats_dev;
	spin_unlock(&msm_idle_stats_devs_lock);

	rc = msm_cpuidle_register_notifier(stats_dev->cpu,
						&stats_dev->notifier);
	if (rc) {
		pr_err("%s: failed to register idle notification\n", __func__);
		goto open_null_bail;
	}

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: done\n", __func__);
	return 0;

open_null_bail:
	spin_lock(&msm_idle_stats_devs_lock);
	per_cpu(msm_idle_stats_devs, stats_dev->cpu) = NULL;
	spin_unlock(&msm_idle_stats_devs_lock);

open_free_bail:
	kfree(stats_dev);

open_bail:
	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: exit, %d\n", __func__, rc);
	return rc;
}

static int msm_idle_stats_release(struct inode *inode, struct file *filp)
{
	struct msm_idle_stats_device *stats_dev;
	int rc;

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: enter\n", __func__);

	stats_dev = (struct msm_idle_stats_device *) filp->private_data;
	rc = msm_cpuidle_unregister_notifier(stats_dev->cpu,
						&stats_dev->notifier);
	WARN_ON(rc);

	spin_lock(&msm_idle_stats_devs_lock);
	per_cpu(msm_idle_stats_devs, stats_dev->cpu) = NULL;
	spin_unlock(&msm_idle_stats_devs_lock);
	filp->private_data = NULL;

	hrtimer_cancel(&stats_dev->timer);
	kfree(stats_dev);

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: done\n", __func__);
	return 0;
}

static long msm_idle_stats_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int rc;

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: enter\n", __func__);

	switch (cmd) {
	case MSM_IDLE_STATS_IOC_COLLECT:
		rc = msm_idle_stats_collect(filp, cmd, arg);
		break;

	default:
		rc = -ENOTTY;
		break;
	}

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: exit, %d\n", __func__, rc);
	return rc;
}

/******************************************************************************
 *
 *****************************************************************************/

static const struct file_operations msm_idle_stats_fops = {
	.owner   = THIS_MODULE,
	.open    = msm_idle_stats_open,
	.release = msm_idle_stats_release,
	.unlocked_ioctl   = msm_idle_stats_ioctl,
};

static int __init msm_idle_stats_init(void)
{
	unsigned int nr_cpus = num_possible_cpus();
	struct device *dev;
	int rc;
	int i;

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: enter\n", __func__);

	rc = alloc_chrdev_region(&msm_idle_stats_dev_nr,
			0, nr_cpus, MSM_IDLE_STATS_DRIVER_NAME);
	if (rc) {
		pr_err("%s: failed to allocate device number, rc %d\n",
			__func__, rc);
		goto init_bail;
	}

	msm_idle_stats_class = class_create(THIS_MODULE,
					MSM_IDLE_STATS_DRIVER_NAME);
	if (IS_ERR(msm_idle_stats_class)) {
		pr_err("%s: failed to create device class\n", __func__);
		rc = -ENOMEM;
		goto init_unreg_bail;
	}

	for (i = 0; i < nr_cpus; i++) {
		dev = device_create(msm_idle_stats_class, NULL,
				msm_idle_stats_dev_nr + i, NULL,
				MSM_IDLE_STATS_DRIVER_NAME "%d", i);

		if (!dev) {
			pr_err("%s: failed to create device %d\n",
				__func__, i);
			rc = -ENOMEM;
			goto init_remove_bail;
		}
	}

	cdev_init(&msm_idle_stats_cdev, &msm_idle_stats_fops);
	msm_idle_stats_cdev.owner = THIS_MODULE;

	/*
	 * Call cdev_add() last, after everything else is initialized and
	 * the driver is ready to accept system calls.
	 */
	rc = cdev_add(&msm_idle_stats_cdev, msm_idle_stats_dev_nr, nr_cpus);
	if (rc) {
		pr_err("%s: failed to register char device, rc %d\n",
			__func__, rc);
		goto init_remove_bail;
	}

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: done\n", __func__);
	return 0;

init_remove_bail:
	for (i = i - 1; i >= 0; i--)
		device_destroy(
			msm_idle_stats_class, msm_idle_stats_dev_nr + i);

	class_destroy(msm_idle_stats_class);

init_unreg_bail:
	unregister_chrdev_region(msm_idle_stats_dev_nr, nr_cpus);

init_bail:
	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: exit, %d\n", __func__, rc);
	return rc;
}

static void __exit msm_idle_stats_exit(void)
{
	unsigned int nr_cpus = num_possible_cpus();
	int i;

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: enter\n", __func__);

	cdev_del(&msm_idle_stats_cdev);

	for (i = nr_cpus - 1; i >= 0; i--)
		device_destroy(
			msm_idle_stats_class, msm_idle_stats_dev_nr + i);

	class_destroy(msm_idle_stats_class);
	unregister_chrdev_region(msm_idle_stats_dev_nr, nr_cpus);

	if (msm_idle_stats_debug_mask & MSM_IDLE_STATS_DEBUG_API)
		pr_info("%s: done\n", __func__);
}

module_init(msm_idle_stats_init);
module_exit(msm_idle_stats_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("idle stats driver");
MODULE_VERSION("1.0");
