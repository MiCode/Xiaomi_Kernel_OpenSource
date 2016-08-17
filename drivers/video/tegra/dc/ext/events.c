/*
 * drivers/video/tegra/dc/ext/events.c
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION, All rights reserved.
 *
 * Author: Robert Morell <rmorell@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/err.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "tegra_dc_ext_priv.h"

static DECLARE_WAIT_QUEUE_HEAD(event_wait);

unsigned int tegra_dc_ext_event_poll(struct file *filp, poll_table *wait)
{
	struct tegra_dc_ext_control_user *user = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &event_wait, wait);

	if (atomic_read(&user->num_events))
		mask |= POLLIN;

	return mask;
}

static int get_next_event(struct tegra_dc_ext_control_user *user,
			  struct tegra_dc_ext_event_list *event,
			  bool block)
{
	struct list_head *list = &user->event_list;
	struct tegra_dc_ext_event_list *next_event;
	int ret;

	if (block) {
		ret = wait_event_interruptible(event_wait,
			atomic_read(&user->num_events));

		if (unlikely(ret)) {
			if (ret == -ERESTARTSYS)
				return -EAGAIN;
			return ret;
		}
	} else {
		if (!atomic_read(&user->num_events))
			return 0;
	}

	mutex_lock(&user->lock);

	BUG_ON(list_empty(list));
	next_event = list_first_entry(list, struct tegra_dc_ext_event_list,
			list);
	*event = *next_event;
	list_del(&next_event->list);
	kfree(next_event);

	atomic_dec(&user->num_events);

	mutex_unlock(&user->lock);

	return 1;
}

ssize_t tegra_dc_ext_event_read(struct file *filp, char __user *buf,
				size_t size, loff_t *ppos)
{
	struct tegra_dc_ext_control_user *user = filp->private_data;
	struct tegra_dc_ext_event_list event_elem;
	struct tegra_dc_ext_event *event = &event_elem.event;
	ssize_t retval = 0, to_copy, event_size, pending;
	loff_t previously_copied = 0;
	char *to_copy_ptr;

	if (size == 0)
		return 0;

	if (user->partial_copy) {
		/*
		 * We didn't transfer the entire event last time, need to
		 * finish it up
		 */
		event_elem = user->event_to_copy;
		previously_copied = user->partial_copy;
	} else {
		/* Get the next event, if any */
		pending = get_next_event(user, &event_elem,
			!(filp->f_flags & O_NONBLOCK));
		if (pending <= 0)
			return pending;
	}

	/* Write the event to the user */
	event_size = sizeof(*event) + event->data_size;
	BUG_ON(event_size <= previously_copied);
	event_size -= previously_copied;

	to_copy_ptr = (char *)event + previously_copied;
	to_copy = min_t(ssize_t, size, event_size);
	if (copy_to_user(buf, to_copy_ptr, to_copy)) {
		retval = -EFAULT;
		to_copy = 0;
	}

	/* Note that we currently only deliver one event at a time */

	if (event_size > to_copy) {
		/*
		 * We were only able to copy part of this event.  Stash it for
		 * next time.
		 */
		user->event_to_copy = event_elem;
		user->partial_copy = previously_copied + to_copy;
	} else {
		user->partial_copy = 0;
	}

	return to_copy ? to_copy : retval;
}

static int tegra_dc_ext_queue_event(struct tegra_dc_ext_control *control,
				    struct tegra_dc_ext_event *event)
{
	struct list_head *cur;
	int retval = 0;

	mutex_lock(&control->lock);
	list_for_each(cur, &control->users) {
		struct tegra_dc_ext_control_user *user;
		struct tegra_dc_ext_event_list *ev_list;

		user = container_of(cur, struct tegra_dc_ext_control_user,
			list);
		mutex_lock(&user->lock);

		if (!(user->event_mask & event->type)) {
			mutex_unlock(&user->lock);
			continue;
		}

		ev_list = kmalloc(sizeof(*ev_list), GFP_KERNEL);
		if (!ev_list) {
			retval = -ENOMEM;
			mutex_unlock(&user->lock);
			continue;
		}

		memcpy(&ev_list->event, event,
			sizeof(*event) + event->data_size);

		list_add_tail(&ev_list->list, &user->event_list);

		atomic_inc(&user->num_events);

		mutex_unlock(&user->lock);
	}
	mutex_unlock(&control->lock);

	/* Is it worth it to track waiters with more granularity? */
	wake_up(&event_wait);

	return retval;
}

int tegra_dc_ext_queue_hotplug(struct tegra_dc_ext_control *control, int output)
{
	struct {
		struct tegra_dc_ext_event event;
		struct tegra_dc_ext_control_event_hotplug hotplug;
	} __packed pack;

	pack.event.type = TEGRA_DC_EXT_EVENT_HOTPLUG;
	pack.event.data_size = sizeof(pack.hotplug);

	pack.hotplug.handle = output;

	tegra_dc_ext_queue_event(control, &pack.event);

	return 0;
}
