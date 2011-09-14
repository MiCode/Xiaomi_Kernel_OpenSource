/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/idle_stats_device.h>

DEFINE_MUTEX(device_list_lock);
LIST_HEAD(device_list);

static ktime_t us_to_ktime(__u32 us)
{
	return ns_to_ktime((u64)us * NSEC_PER_USEC);
}

static struct msm_idle_stats_device *_device_from_minor(unsigned int minor)
{
	struct msm_idle_stats_device *device, *ret = NULL;


	mutex_lock(&device_list_lock);
	list_for_each_entry(device, &device_list, list) {
		if (minor == device->miscdev.minor) {
			ret = device;
			break;
		}
	}
	mutex_unlock(&device_list_lock);
	return ret;
}

static void update_event
	(struct msm_idle_stats_device *device, __u32 event)
{
	__u32 wake_up = !device->stats->event;

	device->stats->event |= event;
	if (wake_up)
		wake_up_interruptible(&device->wait);
}

static enum hrtimer_restart msm_idle_stats_busy_timer(struct hrtimer *timer)
{
	struct msm_idle_stats_device *device =
		container_of(timer, struct msm_idle_stats_device, busy_timer);


	/* This is the only case that the event is modified without a device
	 * lock. However, since the timer is cancelled in the other cases we are
	 * assured that we have exclusive access to the event at this time.
	 */
	hrtimer_set_expires(&device->busy_timer, us_to_ktime(0));
	update_event(device, MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED);
	return HRTIMER_NORESTART;
}

static void start_busy_timer(struct msm_idle_stats_device *device,
						     ktime_t relative_time)
{
	hrtimer_cancel(&device->busy_timer);
	hrtimer_set_expires(&device->busy_timer, us_to_ktime(0));
	if (!((device->stats->event &
		   MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED) ||
	      (device->stats->event & MSM_IDLE_STATS_EVENT_COLLECTION_FULL))) {
		if (ktime_to_us(relative_time) > 0) {
			hrtimer_start(&device->busy_timer,
						  relative_time,
						  HRTIMER_MODE_REL);
		}
	}
}

static unsigned int msm_idle_stats_device_poll(struct file *file,
						poll_table *wait)
{
	struct msm_idle_stats_device *device = file->private_data;
	unsigned int mask = 0;

	poll_wait(file, &device->wait, wait);
	if (device->stats->event)
		mask = POLLIN | POLLRDNORM;
	return mask;
}

static void msm_idle_stats_add_sample(struct msm_idle_stats_device *device,
		struct msm_idle_pulse *pulse)
{
	hrtimer_cancel(&device->busy_timer);
	hrtimer_set_expires(&device->busy_timer, us_to_ktime(0));
	if (device->stats->nr_collected >= MSM_IDLE_STATS_NR_MAX_INTERVALS)
		return;

	device->stats->pulse_chain[device->stats->nr_collected] = *pulse;
	device->stats->nr_collected++;

	if (device->stats->nr_collected == MSM_IDLE_STATS_NR_MAX_INTERVALS) {
		update_event(device, MSM_IDLE_STATS_EVENT_COLLECTION_FULL);
	} else if (device->stats->nr_collected ==
				((MSM_IDLE_STATS_NR_MAX_INTERVALS * 3) / 4)) {
		update_event(device,
			MSM_IDLE_STATS_EVENT_COLLECTION_NEARLY_FULL);
	}
}

static long ioctl_read_stats(struct msm_idle_stats_device *device,
		unsigned long arg)
{
	int remaining;
	int requested;
	struct msm_idle_pulse pulse;
	struct msm_idle_read_stats *stats;
	__s64 remaining_time =
		ktime_to_us(hrtimer_get_remaining(&device->busy_timer));

	device->get_sample(device, &pulse);
	spin_lock(&device->lock);
	hrtimer_cancel(&device->busy_timer);
	stats = device->stats;
	if (stats == &device->stats_vector[0])
		device->stats = &device->stats_vector[1];
	else
		device->stats = &device->stats_vector[0];
	device->stats->event = (stats->event &
			MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED);
	device->stats->nr_collected = 0;
	spin_unlock(&device->lock);
	if (stats->nr_collected >= MSM_IDLE_STATS_NR_MAX_INTERVALS) {
		stats->nr_collected = MSM_IDLE_STATS_NR_MAX_INTERVALS;
	} else {
	    stats->pulse_chain[stats->nr_collected] = pulse;
	    stats->nr_collected++;
	    if (stats->nr_collected == MSM_IDLE_STATS_NR_MAX_INTERVALS)
			stats->event |= MSM_IDLE_STATS_EVENT_COLLECTION_FULL;
	    else if (stats->nr_collected ==
				 ((MSM_IDLE_STATS_NR_MAX_INTERVALS * 3) / 4))
			stats->event |=
				MSM_IDLE_STATS_EVENT_COLLECTION_NEARLY_FULL;
	}
	if (remaining_time < 0) {
		stats->busy_timer_remaining = 0;
	} else {
	    stats->busy_timer_remaining = remaining_time;
		if ((__s64)stats->busy_timer_remaining != remaining_time)
			stats->busy_timer_remaining = -1;
	}
	stats->return_timestamp = ktime_to_us(ktime_get());
	requested =
		((sizeof(*stats) - sizeof(stats->pulse_chain)) +
		 (sizeof(stats->pulse_chain[0]) * stats->nr_collected));
	remaining = copy_to_user((void __user *)arg, stats, requested);
	if (remaining > 0)
		return -EFAULT;

	return 0;
}

static long ioctl_write_stats(struct msm_idle_stats_device *device,
		unsigned long arg)
{
	struct msm_idle_write_stats stats;
	int remaining;
	int ret = 0;

	remaining = copy_from_user(&stats,  (void __user *) arg, sizeof(stats));
	if (remaining > 0) {
		ret = -EFAULT;
	} else {
	    spin_lock(&device->lock);
	    device->busy_timer_interval = us_to_ktime(stats.next_busy_timer);
	    if (ktime_to_us(device->idle_start) == 0)
			start_busy_timer(device, us_to_ktime(stats.busy_timer));
	    spin_unlock(&device->lock);
	}
	return ret;
}

void msm_idle_stats_prepare_idle_start(struct msm_idle_stats_device *device)
{
	spin_lock(&device->lock);
	hrtimer_cancel(&device->busy_timer);
	spin_unlock(&device->lock);
}
EXPORT_SYMBOL(msm_idle_stats_prepare_idle_start);

void msm_idle_stats_abort_idle_start(struct msm_idle_stats_device *device)
{
	spin_lock(&device->lock);
	if (ktime_to_us(hrtimer_get_expires(&device->busy_timer)) > 0)
		hrtimer_restart(&device->busy_timer);
	spin_unlock(&device->lock);
}
EXPORT_SYMBOL(msm_idle_stats_abort_idle_start);

void msm_idle_stats_idle_start(struct msm_idle_stats_device *device)
{
	spin_lock(&device->lock);
	hrtimer_cancel(&device->busy_timer);
	device->idle_start = ktime_get();
	if (ktime_to_us(hrtimer_get_expires(&device->busy_timer)) > 0) {
		device->remaining_time =
				hrtimer_get_remaining(&device->busy_timer);
		if (ktime_to_us(device->remaining_time) <= 0)
			device->remaining_time = us_to_ktime(1);
	} else {
		device->remaining_time = us_to_ktime(0);
	}
	spin_unlock(&device->lock);
}
EXPORT_SYMBOL(msm_idle_stats_idle_start);

void msm_idle_stats_idle_end(struct msm_idle_stats_device *device,
				struct msm_idle_pulse *pulse)
{
	spin_lock(&device->lock);
	if (ktime_to_us(device->idle_start) != 0) {
		device->idle_start = us_to_ktime(0);
	    msm_idle_stats_add_sample(device, pulse);
		if (device->stats->event &
			MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED) {
			device->stats->event &=
				~MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED;
			update_event(device,
				MSM_IDLE_STATS_EVENT_BUSY_TIMER_EXPIRED_RESET);
		} else if (ktime_to_us(device->busy_timer_interval) > 0) {
			ktime_t busy_timer = device->busy_timer_interval;
			if ((pulse->wait_interval > 0) &&
				(ktime_to_us(device->remaining_time) > 0) &&
				(ktime_to_us(device->remaining_time) <
				 ktime_to_us(busy_timer)))
				busy_timer = device->remaining_time;
		    start_busy_timer(device, busy_timer);
	    }
	}
	spin_unlock(&device->lock);
}
EXPORT_SYMBOL(msm_idle_stats_idle_end);

static long msm_idle_stats_device_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct msm_idle_stats_device *device = file->private_data;
	int ret;

	switch (cmd) {
	case MSM_IDLE_STATS_IOC_READ_STATS:
		ret = ioctl_read_stats(device, arg);
		break;
	case MSM_IDLE_STATS_IOC_WRITE_STATS:
		ret = ioctl_write_stats(device, arg);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int msm_idle_stats_device_release
			  (struct inode *inode, struct file *filep)
{
	return 0;
}

static int msm_idle_stats_device_open(struct inode *inode, struct file *filep)
{
	struct msm_idle_stats_device *device;


	device = _device_from_minor(iminor(inode));

	if (device == NULL)
		return -EPERM;

	filep->private_data = device;
	return 0;
}

static const struct file_operations msm_idle_stats_fops = {
	.open = msm_idle_stats_device_open,
	.release = msm_idle_stats_device_release,
	.unlocked_ioctl = msm_idle_stats_device_ioctl,
	.poll = msm_idle_stats_device_poll,
};

int msm_idle_stats_register_device(struct msm_idle_stats_device *device)
{
	int ret = -ENOMEM;

	spin_lock_init(&device->lock);
	init_waitqueue_head(&device->wait);
	hrtimer_init(&device->busy_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	device->busy_timer.function = msm_idle_stats_busy_timer;

	device->stats_vector[0].event         = 0;
	device->stats_vector[0].nr_collected  = 0;
	device->stats_vector[1].event         = 0;
	device->stats_vector[1].nr_collected  = 0;
	device->stats = &device->stats_vector[0];
	device->busy_timer_interval = us_to_ktime(0);

	mutex_lock(&device_list_lock);
	list_add(&device->list, &device_list);
	mutex_unlock(&device_list_lock);

	device->miscdev.minor = MISC_DYNAMIC_MINOR;
	device->miscdev.name = device->name;
	device->miscdev.fops = &msm_idle_stats_fops;

	ret = misc_register(&device->miscdev);

	if (ret)
		goto err_list;

	return ret;

err_list:
	mutex_lock(&device_list_lock);
	list_del(&device->list);
	mutex_unlock(&device_list_lock);
	return ret;
}
EXPORT_SYMBOL(msm_idle_stats_register_device);

int msm_idle_stats_deregister_device(struct msm_idle_stats_device *device)
{
	if (device == NULL)
		return 0;

	mutex_lock(&device_list_lock);
	spin_lock(&device->lock);
	hrtimer_cancel(&device->busy_timer);
	list_del(&device->list);
	spin_unlock(&device->lock);
	mutex_unlock(&device_list_lock);

	return misc_deregister(&device->miscdev);
}
EXPORT_SYMBOL(msm_idle_stats_deregister_device);
