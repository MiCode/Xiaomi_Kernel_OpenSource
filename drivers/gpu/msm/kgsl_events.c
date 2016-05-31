/* Copyright (c) 2011-2016, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <kgsl_device.h>

#include "kgsl_debugfs.h"
#include "kgsl_trace.h"

/*
 * Define an kmem cache for the event structures since we allocate and free them
 * so frequently
 */
static struct kmem_cache *events_cache;
static struct dentry *events_dentry;

static inline void signal_event(struct kgsl_device *device,
		struct kgsl_event *event, int result)
{
	list_del(&event->node);
	event->result = result;
	queue_work(device->events_wq, &event->work);
}

/**
 * _kgsl_event_worker() - Work handler for processing GPU event callbacks
 * @work: Pointer to the work_struct for the event
 *
 * Each event callback has its own work struct and is run on a event specific
 * workqeuue.  This is the worker that queues up the event callback function.
 */
static void _kgsl_event_worker(struct work_struct *work)
{
	struct kgsl_event *event = container_of(work, struct kgsl_event, work);
	int id = KGSL_CONTEXT_ID(event->context);

	trace_kgsl_fire_event(id, event->timestamp, event->result,
		jiffies - event->created, event->func);

	event->func(event->device, event->group, event->priv, event->result);

	kgsl_context_put(event->context);
	kmem_cache_free(events_cache, event);
}

/* return true if the group needs to be processed */
static bool _do_process_group(unsigned int processed, unsigned int cur)
{
	if (processed == cur)
		return false;

	/*
	 * This ensures that the timestamp didn't slip back accidently, maybe
	 * due to a memory barrier issue. This is highly unlikely but we've
	 * been burned here in the past.
	 */
	if ((cur < processed) && ((processed - cur) < KGSL_TIMESTAMP_WINDOW))
		return false;

	return true;
}

static void _process_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group, bool flush)
{
	struct kgsl_event *event, *tmp;
	unsigned int timestamp;
	struct kgsl_context *context;

	if (group == NULL)
		return;

	context = group->context;

	/*
	 * Sanity check to be sure that we we aren't racing with the context
	 * getting destroyed
	 */
	if (context != NULL && !_kgsl_context_get(context))
		BUG();

	spin_lock(&group->lock);

	group->readtimestamp(device, group->priv, KGSL_TIMESTAMP_RETIRED,
		&timestamp);

	if (!flush && _do_process_group(group->processed, timestamp) == false)
		goto out;

	list_for_each_entry_safe(event, tmp, &group->events, node) {
		if (timestamp_cmp(event->timestamp, timestamp) <= 0)
			signal_event(device, event, KGSL_EVENT_RETIRED);
		else if (flush)
			signal_event(device, event, KGSL_EVENT_CANCELLED);

	}

	group->processed = timestamp;

out:
	spin_unlock(&group->lock);
	kgsl_context_put(context);
}

/**
 * kgsl_process_event_group() - Handle all the retired events in a group
 * @device: Pointer to a KGSL device
 * @group: Pointer to a GPU events group to process
 */

void kgsl_process_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group)
{
	_process_event_group(device, group, false);
}
EXPORT_SYMBOL(kgsl_process_event_group);

/**
 * kgsl_flush_event_group() - flush all the events in a group by retiring the
 * ones can be retired and cancelling the ones that are pending
 * @device: Pointer to a KGSL device
 * @group: Pointer to a GPU events group to process
 */
void kgsl_flush_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group)
{
	_process_event_group(device, group, true);
}
EXPORT_SYMBOL(kgsl_flush_event_group);

/**
 * kgsl_cancel_events_timestamp() - Cancel pending events for a given timestamp
 * @device: Pointer to a KGSL device
 * @group: Ponter to the GPU event group that owns the event
 * @timestamp: Registered expiry timestamp for the event
 */
void kgsl_cancel_events_timestamp(struct kgsl_device *device,
		struct kgsl_event_group *group, unsigned int timestamp)
{
	struct kgsl_event *event, *tmp;

	spin_lock(&group->lock);

	list_for_each_entry_safe(event, tmp, &group->events, node) {
		if (timestamp_cmp(timestamp, event->timestamp) == 0)
			signal_event(device, event, KGSL_EVENT_CANCELLED);
	}

	spin_unlock(&group->lock);
}
EXPORT_SYMBOL(kgsl_cancel_events_timestamp);

/**
 * kgsl_cancel_events() - Cancel all pending events in the group
 * @device: Pointer to a KGSL device
 * @group: Pointer to a kgsl_events_group
 */
void kgsl_cancel_events(struct kgsl_device *device,
		struct kgsl_event_group *group)
{
	struct kgsl_event *event, *tmp;

	spin_lock(&group->lock);

	list_for_each_entry_safe(event, tmp, &group->events, node)
		signal_event(device, event, KGSL_EVENT_CANCELLED);

	spin_unlock(&group->lock);
}
EXPORT_SYMBOL(kgsl_cancel_events);

/**
 * kgsl_cancel_event() - Cancel a specific event from a group
 * @device: Pointer to a KGSL device
 * @group: Pointer to the group that contains the events
 * @timestamp: Registered expiry timestamp for the event
 * @func: Registered callback for the function
 * @priv: Registered priv data for the function
 */
void kgsl_cancel_event(struct kgsl_device *device,
		struct kgsl_event_group *group, unsigned int timestamp,
		kgsl_event_func func, void *priv)
{
	struct kgsl_event *event, *tmp;
	spin_lock(&group->lock);

	list_for_each_entry_safe(event, tmp, &group->events, node) {
		if (timestamp == event->timestamp && func == event->func &&
			event->priv == priv)
			signal_event(device, event, KGSL_EVENT_CANCELLED);
	}

	spin_unlock(&group->lock);
}
EXPORT_SYMBOL(kgsl_cancel_event);

/**
 * kgsl_event_pending() - Searches for an event in an event group
 * @device: Pointer to a KGSL device
 * @group: Pointer to the group that contains the events
 * @timestamp: Registered expiry timestamp for the event
 * @func: Registered callback for the function
 * @priv: Registered priv data for the function
 */
bool kgsl_event_pending(struct kgsl_device *device,
		struct kgsl_event_group *group,
		unsigned int timestamp, kgsl_event_func func, void *priv)
{
	struct kgsl_event *event;
	bool result = false;
	spin_lock(&group->lock);
	list_for_each_entry(event, &group->events, node) {
		if (timestamp == event->timestamp && func == event->func &&
			event->priv == priv) {
			result = true;
			break;
		}
	}
	spin_unlock(&group->lock);
	return result;
}
/**
 * kgsl_add_event() - Add a new GPU event to a group
 * @device: Pointer to a KGSL device
 * @group: Pointer to the group to add the event to
 * @timestamp: Timestamp that the event will expire on
 * @func: Callback function for the event
 * @priv: Private data to send to the callback function
 */
int kgsl_add_event(struct kgsl_device *device, struct kgsl_event_group *group,
		unsigned int timestamp, kgsl_event_func func, void *priv)
{
	unsigned int queued;
	struct kgsl_context *context = group->context;
	struct kgsl_event *event;
	unsigned int retired;

	if (!func)
		return -EINVAL;

	/*
	 * If the caller is creating their own timestamps, let them schedule
	 * events in the future. Otherwise only allow timestamps that have been
	 * queued.
	 */
	if (!context || !(context->flags & KGSL_CONTEXT_USER_GENERATED_TS)) {
		group->readtimestamp(device, group->priv, KGSL_TIMESTAMP_QUEUED,
			&queued);

		if (timestamp_cmp(timestamp, queued) > 0)
			return -EINVAL;
	}

	event = kmem_cache_alloc(events_cache, GFP_KERNEL);
	if (event == NULL)
		return -ENOMEM;

	/* Get a reference to the context while the event is active */
	if (context != NULL && !_kgsl_context_get(context)) {
		kmem_cache_free(events_cache, event);
		return -ENOENT;
	}

	event->device = device;
	event->context = context;
	event->timestamp = timestamp;
	event->priv = priv;
	event->func = func;
	event->created = jiffies;
	event->group = group;

	INIT_WORK(&event->work, _kgsl_event_worker);

	trace_kgsl_register_event(KGSL_CONTEXT_ID(context), timestamp, func);

	spin_lock(&group->lock);

	/*
	 * Check to see if the requested timestamp has already retired.  If so,
	 * schedule the callback right away
	 */
	group->readtimestamp(device, group->priv, KGSL_TIMESTAMP_RETIRED,
		&retired);

	if (timestamp_cmp(retired, timestamp) >= 0) {
		event->result = KGSL_EVENT_RETIRED;
		queue_work(device->events_wq, &event->work);
		spin_unlock(&group->lock);
		return 0;
	}

	/* Add the event to the group list */
	list_add_tail(&event->node, &group->events);

	spin_unlock(&group->lock);

	return 0;
}
EXPORT_SYMBOL(kgsl_add_event);

static DEFINE_RWLOCK(group_lock);
static LIST_HEAD(group_list);

void kgsl_process_event_groups(struct kgsl_device *device)
{
	struct kgsl_event_group *group;

	read_lock(&group_lock);
	list_for_each_entry(group, &group_list, group)
		_process_event_group(device, group, false);
	read_unlock(&group_lock);
}
EXPORT_SYMBOL(kgsl_process_event_groups);

/**
 * kgsl_del_event_group() - Remove a GPU event group
 * @group: GPU event group to remove
 */
void kgsl_del_event_group(struct kgsl_event_group *group)
{
	/* Make sure that all the events have been deleted from the list */
	BUG_ON(!list_empty(&group->events));

	write_lock(&group_lock);
	list_del(&group->group);
	write_unlock(&group_lock);
}
EXPORT_SYMBOL(kgsl_del_event_group);

/**
 * kgsl_add_event_group() - Add a new GPU event group
 * group: Pointer to the new group to add to the list
 * context: Context that owns the group (or NULL for global)
 * name: Name of the group
 * readtimestamp: Function pointer to the readtimestamp function to call when
 * processing events
 * priv: Priv member to pass to the readtimestamp function
 */
void kgsl_add_event_group(struct kgsl_event_group *group,
		struct kgsl_context *context, const char *name,
		readtimestamp_func readtimestamp, void *priv)
{
	BUG_ON(readtimestamp == NULL);

	spin_lock_init(&group->lock);
	INIT_LIST_HEAD(&group->events);

	group->context = context;
	group->readtimestamp = readtimestamp;
	group->priv = priv;

	if (name)
		strlcpy(group->name, name, sizeof(group->name));

	write_lock(&group_lock);
	list_add_tail(&group->group, &group_list);
	write_unlock(&group_lock);
}
EXPORT_SYMBOL(kgsl_add_event_group);

static void events_debugfs_print_group(struct seq_file *s,
		struct kgsl_event_group *group)
{
	struct kgsl_event *event;
	unsigned int retired;

	spin_lock(&group->lock);

	seq_printf(s, "%s: last=%d\n", group->name, group->processed);

	list_for_each_entry(event, &group->events, node) {

		group->readtimestamp(event->device, group->priv,
			KGSL_TIMESTAMP_RETIRED, &retired);

		seq_printf(s, "\t%d:%d age=%lu func=%ps [retired=%d]\n",
			group->context ? group->context->id :
						KGSL_MEMSTORE_GLOBAL,
			event->timestamp, jiffies  - event->created,
			event->func, retired);
	}
	spin_unlock(&group->lock);
}

static int events_debugfs_print(struct seq_file *s, void *unused)
{
	struct kgsl_event_group *group;

	seq_puts(s, "event groups:\n");
	seq_puts(s, "--------------\n");

	read_lock(&group_lock);
	list_for_each_entry(group, &group_list, group) {
		events_debugfs_print_group(s, group);
		seq_puts(s, "\n");
	}
	read_unlock(&group_lock);

	return 0;
}

static int events_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, events_debugfs_print, NULL);
}

static const struct file_operations events_fops = {
	.open = events_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/**
 * kgsl_events_exit() - Destroy the event kmem cache on module exit
 */
void kgsl_events_exit(void)
{
	if (events_cache)
		kmem_cache_destroy(events_cache);

	debugfs_remove(events_dentry);
}

/**
 * kgsl_events_init() - Create the event kmem cache on module start
 */
void __init kgsl_events_init(void)
{
	struct dentry *debugfs_dir = kgsl_get_debugfs_dir();
	events_cache = KMEM_CACHE(kgsl_event, 0);

	events_dentry = debugfs_create_file("events", 0444, debugfs_dir, NULL,
		&events_fops);

	/* Failure to create a debugfs entry is non fatal */
	if (IS_ERR(events_dentry))
		events_dentry = NULL;
}
