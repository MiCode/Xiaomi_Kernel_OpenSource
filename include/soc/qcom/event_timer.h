/* SPDX-License-Identifier: GPL-2.0-only */

/*
 * Copyright (c) 2012, 2014,2017 The Linux Foundation. All rights reserved.
 */

#ifndef __ARCH_ARM_MACH_MSM_EVENT_TIMER_H
#define __ARCH_ARM_MACH_MSM_EVENT_TIMER_H

#include <linux/hrtimer.h>

struct event_timer_info;

#ifdef CONFIG_MSM_EVENT_TIMER
/**
 * add_event_timer() : Add a wakeup event. Intended to be called
 *                     by clients once. Returns a handle to be used
 *                     for future transactions.
 * @irq : Interrupt number to track affinity.
 * @function : The callback function will be called when event
 *             timer expires.
 * @data : Callback data provided by client.
 */
struct event_timer_info *add_event_timer(uint32_t irq,
				void (*function)(void *), void *data);

/** activate_event_timer() : Set the expiration time for an event in absolute
 *                           ktime. This is a oneshot event timer, clients
 *                           should call this again to set another expiration.
 *  @event : Event handle.
 *  @event_time : Event time in absolute ktime.
 */
void activate_event_timer(struct event_timer_info *event, ktime_t event_time);

/**
 * deactivate_event_timer() : Deactivate an event timer.
 * @event: event handle.
 */
void deactivate_event_timer(struct event_timer_info *event);

/**
 * destroy_event_timer() : Free the event info data structure allocated during
 * add_event_timer().
 * @event: event handle.
 */
void destroy_event_timer(struct event_timer_info *event);

/**
 * get_next_event_timer() : Get the next wakeup event.
 *                          returns a ktime value of the next
 *                          expiring event.
 */
ktime_t get_next_event_time(int cpu);
#else
static inline void *add_event_timer(uint32_t irq, void (*function)(void *),
						void *data)
{
	return NULL;
}

static inline void activate_event_timer(void *event, ktime_t event_time) {}

static inline void  deactivate_event_timer(void *event) {}

static inline void destroy_event_timer(void *event) {}

static inline ktime_t get_next_event_time(int cpu)
{
	return ns_to_ktime(0);
}

#endif /* CONFIG_MSM_EVENT_TIMER_MANAGER */
#endif /* __ARCH_ARM_MACH_MSM_EVENT_TIMER_H */
