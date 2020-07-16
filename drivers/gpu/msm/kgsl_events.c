// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2011-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/rwlock.h>

#include "kgsl_debugfs.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

/*
 * Define an kmem cache for the event structures since we allocate and free them
 * so frequently
 */
static struct kmem_cache *events_cache;

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
	if (WARN_ON(context != NULL && !_kgsl_context_get(context)))
		return;

	spin_lock(&group->lock);

	group->readtimestamp(device, group->priv, KGSL_TIMESTAMP_RETIRED,
		&timestamp);

	if (!flush && !_do_process_group(group->processed, timestamp))
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

void kgsl_process_event_groups(struct kgsl_device *device)
{
	struct kgsl_event_group *group;

	read_lock(&device->event_groups_lock);
	list_for_each_entry(group, &device->event_groups, group)
		_process_event_group(device, group, false);
	read_unlock(&device->event_groups_lock);
}

void kgsl_del_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group)
{
	/* Check if the group is uninintalized */
	if (!group->context)
		return;

	/* Make sure that all the events have been deleted from the list */
	WARN_ON(!list_empty(&group->events));

	write_lock(&device->event_groups_lock);
	list_del(&group->group);
	write_unlock(&device->event_groups_lock);
}

void kgsl_add_event_group(struct kgsl_device *device,
		struct kgsl_event_group *group, struct kgsl_context *context,
		readtimestamp_func readtimestamp,
		void *priv, const char *fmt, ...)
{
	va_list args;

	WARN_ON(readtimestamp == NULL);

	spin_lock_init(&group->lock);
	INIT_LIST_HEAD(&group->events);

	group->context = context;
	group->readtimestamp = readtimestamp;
	group->priv = priv;

	if (fmt) {
		va_start(args, fmt);
		vsnprintf(group->name, sizeof(group->name), fmt, args);
		va_end(args);
	}

	write_lock(&device->event_groups_lock);
	list_add_tail(&group->group, &device->event_groups);
	write_unlock(&device->event_groups_lock);
}

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

		seq_printf(s, "\t%u:%u age=%lu func=%ps [retired=%u]\n",
			group->context ? group->context->id :
						KGSL_MEMSTORE_GLOBAL,
			event->timestamp, jiffies  - event->created,
			event->func, retired);
	}
	spin_unlock(&group->lock);
}

static int events_debugfs_print(struct seq_file *s, void *unused)
{
	struct kgsl_device *device = s->private;
	struct kgsl_event_group *group;

	seq_puts(s, "event groups:\n");
	seq_puts(s, "--------------\n");

	read_lock(&device->event_groups_lock);
	list_for_each_entry(group, &device->event_groups, group) {
		events_debugfs_print_group(s, group);
		seq_puts(s, "\n");
	}
	read_unlock(&device->event_groups_lock);

	return 0;
}

static int events_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, events_debugfs_print, inode->i_private);
}

static const struct file_operations events_fops = {
	.open = events_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void kgsl_device_events_remove(struct kgsl_device *device)
{
	struct kgsl_event_group *group, *tmp;

	write_lock(&device->event_groups_lock);
	list_for_each_entry_safe(group, tmp, &device->event_groups, group) {
		WARN_ON(!list_empty(&group->events));
		list_del(&group->group);
	}
	write_unlock(&device->event_groups_lock);
}

void kgsl_device_events_probe(struct kgsl_device *device)
{
	INIT_LIST_HEAD(&device->event_groups);
	rwlock_init(&device->event_groups_lock);

	debugfs_create_file("events", 0444, device->d_debugfs, device,
		&events_fops);
}

/**
 * kgsl_events_exit() - Destroy the event kmem cache on module exit
 */
void kgsl_events_exit(void)
{
	kmem_cache_destroy(events_cache);
}

/**
 * kgsl_events_init() - Create the event kmem cache on module start
 */
void __init kgsl_events_init(void)
{
	events_cache = KMEM_CACHE(kgsl_event, 0);
}
