/* Copyright (c) 2012, 2014-2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <soc/qcom/event_timer.h>

/**
 * struct event_timer_info - basic event timer structure
 * @node: timerqueue node to track time ordered data structure
 *        of event timers
 * @notify: irq affinity notifier.
 * @timer: hrtimer created for this event.
 * @function : callback function for event timer.
 * @data : callback data for event timer.
 * @irq: irq number for which event timer is created.
 * @cpu: event timer associated cpu.
 */
struct event_timer_info {
	struct timerqueue_node node;
	struct irq_affinity_notify notify;
	void (*function)(void *);
	void *data;
	int irq;
	int cpu;
};

struct hrtimer_info {
	struct hrtimer event_hrtimer;
	bool timer_initialized;
};

static DEFINE_PER_CPU(struct hrtimer_info, per_cpu_hrtimer);

static DEFINE_PER_CPU(struct timerqueue_head, timer_head) = {
	.head = RB_ROOT,
	.next = NULL,
};

static DEFINE_SPINLOCK(event_timer_lock);
static DEFINE_SPINLOCK(event_setup_lock);

static void create_timer_smp(void *data);
static void setup_event_hrtimer(struct event_timer_info *event);
static enum hrtimer_restart event_hrtimer_cb(struct hrtimer *hrtimer);
static void irq_affinity_change_notifier(struct irq_affinity_notify *notify,
						const cpumask_t *new_cpu_mask);
static void irq_affinity_release(struct kref *ref);

static int msm_event_debug_mask;
module_param_named(
	debug_mask, msm_event_debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP
);

enum {
	MSM_EVENT_TIMER_DEBUG = 1U << 0,
};

/**
 * add_event_timer() : Add a wakeup event. Intended to be called
 *                     by clients once. Returns a handle to be used
 *                     for future transactions.
 * @irq: event associated irq number.
 * @function : The callback function will be called when event
 *             timer expires.
 * @data: callback data provided by client.
 */
struct event_timer_info *add_event_timer(uint32_t irq,
				void (*function)(void *), void *data)
{
	struct event_timer_info *event_info =
			kzalloc(sizeof(struct event_timer_info), GFP_KERNEL);

	if (!event_info)
		return NULL;

	event_info->function = function;
	event_info->data = data;

	if (irq) {
		struct irq_desc *desc = irq_to_desc(irq);
		struct cpumask *mask = desc->irq_data.affinity;

		get_online_cpus();
		event_info->cpu = cpumask_any_and(mask, cpu_online_mask);
		if (event_info->cpu >= nr_cpu_ids)
			event_info->cpu = cpumask_first(cpu_online_mask);

		event_info->notify.notify = irq_affinity_change_notifier;
		event_info->notify.release = irq_affinity_release;
		irq_set_affinity_notifier(irq, &event_info->notify);
		put_online_cpus();
	}

	/* Init rb node and hr timer */
	timerqueue_init(&event_info->node);
	pr_debug("New Event Added. Event %p(on cpu%d). irq %d.\n",
					event_info, event_info->cpu, irq);

	return event_info;
}
EXPORT_SYMBOL(add_event_timer);

/**
 * is_event_next(): Helper function to check if the event is the next
 *                  expiring event
 * @event : handle to the event to be checked.
 */
static bool is_event_next(struct event_timer_info *event)
{
	struct event_timer_info *next_event;
	struct timerqueue_node *next;
	bool ret = false;

	next = timerqueue_getnext(&per_cpu(timer_head, event->cpu));
	if (!next)
		goto exit_is_next_event;

	next_event = container_of(next, struct event_timer_info, node);
	if (!next_event)
		goto exit_is_next_event;

	if (next_event == event)
		ret = true;

exit_is_next_event:
	return ret;
}

/**
 * is_event_active(): Helper function to check if the timer for a given event
 *                    has been started.
 * @event : handle to the event to be checked.
 */
static bool is_event_active(struct event_timer_info *event)
{
	struct timerqueue_node *next;
	struct event_timer_info *next_event;
	bool ret = false;

	for (next = timerqueue_getnext(&per_cpu(timer_head, event->cpu)); next;
			next = timerqueue_iterate_next(next)) {
		next_event = container_of(next, struct event_timer_info, node);

		if (event == next_event) {
			ret = true;
			break;
		}
	}
	return ret;
}

/**
 * create_hrtimer(): Helper function to setup hrtimer.
 */
static void create_hrtimer(struct event_timer_info *event)

{
	bool timer_initialized = per_cpu(per_cpu_hrtimer.timer_initialized,
								event->cpu);
	struct hrtimer *event_hrtimer = &per_cpu(per_cpu_hrtimer.event_hrtimer,
								event->cpu);

	if (!timer_initialized) {
		hrtimer_init(event_hrtimer, CLOCK_MONOTONIC,
						HRTIMER_MODE_ABS_PINNED);
		per_cpu(per_cpu_hrtimer.timer_initialized, event->cpu) = true;
	}

	event_hrtimer->function = event_hrtimer_cb;
	hrtimer_start(event_hrtimer, event->node.expires,
					HRTIMER_MODE_ABS_PINNED);
}

/**
 * event_hrtimer_cb() : Callback function for hr timer.
 *                      Make the client CB from here and remove the event
 *                      from the time ordered queue.
 */
static enum hrtimer_restart event_hrtimer_cb(struct hrtimer *hrtimer)
{
	struct event_timer_info *event;
	struct timerqueue_node *next;
	unsigned long flags;
	int cpu;

	spin_lock_irqsave(&event_timer_lock, flags);
	cpu = smp_processor_id();
	next = timerqueue_getnext(&per_cpu(timer_head, cpu));

	while (next && (ktime_to_ns(next->expires)
		<= ktime_to_ns(hrtimer->node.expires))) {
		event = container_of(next, struct event_timer_info, node);
		if (!event)
			goto hrtimer_cb_exit;

		WARN_ON_ONCE(event->cpu != cpu);

		if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
			pr_debug("Deleting event %p @ %lu(on cpu%d)\n", event,
				(unsigned long)ktime_to_ns(next->expires), cpu);

		timerqueue_del(&per_cpu(timer_head, cpu), &event->node);

		if (event->function)
			event->function(event->data);

		next = timerqueue_getnext(&per_cpu(timer_head, cpu));
	}

	if (next) {
		event = container_of(next, struct event_timer_info, node);
		create_hrtimer(event);
	}
hrtimer_cb_exit:
	spin_unlock_irqrestore(&event_timer_lock, flags);
	return HRTIMER_NORESTART;
}

/**
 * create_timer_smp(): Helper function used setting up timer on CPUs.
 */
static void create_timer_smp(void *data)
{
	unsigned long flags;
	struct event_timer_info *event =
		(struct event_timer_info *)data;
	struct timerqueue_node *next;

	spin_lock_irqsave(&event_timer_lock, flags);

	if (is_event_active(event))
		timerqueue_del(&per_cpu(timer_head, event->cpu), &event->node);

	next = timerqueue_getnext(&per_cpu(timer_head, event->cpu));
	timerqueue_add(&per_cpu(timer_head, event->cpu), &event->node);

	if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
		pr_debug("Adding Event %p(on cpu%d) for %lu\n", event,
		event->cpu,
		(unsigned long)ktime_to_ns(event->node.expires));

	if (!next || (next && (ktime_to_ns(event->node.expires) <
						ktime_to_ns(next->expires)))) {
		if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
			pr_debug("Setting timer for %lu(on cpu%d)\n",
			(unsigned long)ktime_to_ns(event->node.expires),
			event->cpu);

		create_hrtimer(event);
	}
	spin_unlock_irqrestore(&event_timer_lock, flags);
}

/**
 *  setup_timer() : Helper function to setup timer on primary
 *                  core during hrtimer callback.
 *  @event: event handle causing the wakeup.
 */
static void setup_event_hrtimer(struct event_timer_info *event)
{
	smp_call_function_single(event->cpu, create_timer_smp, event, 1);
}

static void irq_affinity_release(struct kref *ref)
{
	struct event_timer_info *event;
	struct irq_affinity_notify *notify =
			container_of(ref, struct irq_affinity_notify, kref);

	event = container_of(notify, struct event_timer_info, notify);
	pr_debug("event = %p\n", event);
}

static void irq_affinity_change_notifier(struct irq_affinity_notify *notify,
						const cpumask_t *mask_val)
{
	struct event_timer_info *event;
	unsigned long flags;
	unsigned int irq;
	int old_cpu = -EINVAL, new_cpu = -EINVAL;
	bool next_event = false;

	event = container_of(notify, struct event_timer_info, notify);
	irq = notify->irq;

	if (!event)
		return;

	/*
	 * This logic is inline with irq-gic.c for finding
	 * the next affinity CPU.
	 */
	new_cpu = cpumask_any_and(mask_val, cpu_online_mask);
	if (new_cpu >= nr_cpu_ids)
		return;

	old_cpu = event->cpu;

	if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
		pr_debug("irq %d, event %p, old_cpu(%d)->new_cpu(%d).\n",
						irq, event, old_cpu, new_cpu);

	/* No change in IRQ affinity */
	if (old_cpu == new_cpu)
		return;

	spin_lock_irqsave(&event_timer_lock, flags);

	/* If the event is not active OR
	 * If it is the next event
	 * and the timer is already in callback
	 * Just reset cpu and return
	 */
	if (!is_event_active(event) ||
		(is_event_next(event) &&
		(hrtimer_try_to_cancel(&per_cpu(per_cpu_hrtimer.
				event_hrtimer, old_cpu)) < 0))) {
		event->cpu = new_cpu;
		spin_unlock_irqrestore(&event_timer_lock, flags);
		if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
			pr_debug("Event:%p is not active or in callback\n",
					event);
		return;
	}

	/* Update the flag based on EVENT is next are not */
	if (is_event_next(event))
		next_event = true;

	event->cpu = new_cpu;

	/*
	 * We are here either because hrtimer was active or event is not next
	 * Delete the event from the timer queue anyway
	 */
	timerqueue_del(&per_cpu(timer_head, old_cpu), &event->node);

	if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
		pr_debug("Event:%p is in the list\n", event);

	spin_unlock_irqrestore(&event_timer_lock, flags);

	/*
	 * Migrating event timer to a new CPU is automatically
	 * taken care. Since we have already modify the event->cpu
	 * with new CPU.
	 *
	 * Typical cases are
	 *
	 * 1)
	 *		C0			C1
	 *		|			^
	 *	-----------------		|
	 *	|	|	|		|
	 *	E1	E2	E3		|
	 *		|(migrating)		|
	 *		-------------------------
	 *
	 * 2)
	 *		C0			C1
	 *		|			^
	 *	----------------		|
	 *	|	|	|		|
	 *	E1	E2	E3		|
	 *	|(migrating)			|
	 *	---------------------------------
	 *
	 * Here after moving the E1 to C1. Need to start
	 * E2 on C0.
	 */
	spin_lock(&event_setup_lock);
	/* Setup event timer on new cpu*/
	setup_event_hrtimer(event);

	/* Setup event on the old cpu*/
	if (next_event) {
		struct timerqueue_node *next;

		next = timerqueue_getnext(&per_cpu(timer_head, old_cpu));
		if (next) {
			event = container_of(next,
					struct event_timer_info, node);
			setup_event_hrtimer(event);
		}
	}
	spin_unlock(&event_setup_lock);
}

/**
 * activate_event_timer() : Set the expiration time for an event in absolute
 *                           ktime. This is a oneshot event timer, clients
 *                           should call this again to set another expiration.
 *  @event : event handle.
 *  @event_time : event time in absolute ktime.
 */
void activate_event_timer(struct event_timer_info *event, ktime_t event_time)
{
	if (!event)
		return;

	if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
		pr_debug("Adding event %p timer @ %lu(on cpu%d)\n", event,
				(unsigned long)ktime_to_us(event_time),
				event->cpu);

	spin_lock(&event_setup_lock);
	event->node.expires = event_time;
	/* Start hrtimer and add event to rb tree */
	setup_event_hrtimer(event);
	spin_unlock(&event_setup_lock);
}
EXPORT_SYMBOL(activate_event_timer);

/**
 * deactivate_event_timer() : Deactivate an event timer, this removes the event from
 *                            the time ordered queue of event timers.
 * @event: event handle.
 */
void deactivate_event_timer(struct event_timer_info *event)
{
	unsigned long flags;

	if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
		pr_debug("Deactivate timer\n");

	spin_lock_irqsave(&event_timer_lock, flags);
	if (is_event_active(event)) {
		if (is_event_next(event))
			hrtimer_try_to_cancel(&per_cpu(
				per_cpu_hrtimer.event_hrtimer, event->cpu));

		timerqueue_del(&per_cpu(timer_head, event->cpu), &event->node);
	}
	spin_unlock_irqrestore(&event_timer_lock, flags);
}

/**
 * destroy_event_timer() : Free the event info data structure allocated during
 *                         add_event_timer().
 * @event: event handle.
 */
void destroy_event_timer(struct event_timer_info *event)
{
	unsigned long flags;

	spin_lock_irqsave(&event_timer_lock, flags);
	if (is_event_active(event)) {
		if (is_event_next(event))
			hrtimer_try_to_cancel(&per_cpu(
				per_cpu_hrtimer.event_hrtimer, event->cpu));

		timerqueue_del(&per_cpu(timer_head, event->cpu), &event->node);
	}
	spin_unlock_irqrestore(&event_timer_lock, flags);
	kfree(event);
}
EXPORT_SYMBOL(destroy_event_timer);

/**
 * get_next_event_timer() - Get the next wakeup event. Returns
 *                          a ktime value of the next expiring event.
 */
ktime_t get_next_event_time(int cpu)
{
	unsigned long flags;
	struct timerqueue_node *next;
	struct event_timer_info *event;
	ktime_t next_event = ns_to_ktime(0);

	spin_lock_irqsave(&event_timer_lock, flags);
	next = timerqueue_getnext(&per_cpu(timer_head, cpu));
	event = container_of(next, struct event_timer_info, node);
	spin_unlock_irqrestore(&event_timer_lock, flags);

	if (!next || event->cpu != cpu)
		return next_event;

	next_event = hrtimer_get_remaining(
				&per_cpu(per_cpu_hrtimer.event_hrtimer, cpu));

	if (msm_event_debug_mask && MSM_EVENT_TIMER_DEBUG)
		pr_debug("Next Event %lu(on cpu%d)\n",
			(unsigned long)ktime_to_us(next_event), cpu);

	return next_event;
}
