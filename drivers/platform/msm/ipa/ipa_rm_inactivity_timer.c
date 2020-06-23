// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2018, 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/unistd.h>
#include <linux/workqueue.h>
#include <linux/ipa.h>
#include "ipa_rm_i.h"

#define MAX_WS_NAME 20

/**
 * struct ipa_rm_it_private - IPA RM Inactivity Timer private
 *	data
 * @initied: indicates if instance was initialized
 * @lock - spinlock for mutual exclusion
 * @resource_name - resource name
 * @work: delayed work object for running delayed releas
 *	function
 * @resource_requested: boolean flag indicates if resource was requested
 * @reschedule_work: boolean flag indicates to not release and to
 *	reschedule the release work.
 * @work_in_progress: boolean flag indicates is release work was scheduled.
 * @jiffies: number of jiffies for timeout
 *
 * WWAN private - holds all relevant info about WWAN driver
 */
struct ipa_rm_it_private {
	bool initied;
	enum ipa_rm_resource_name resource_name;
	spinlock_t lock;
	struct delayed_work work;
	bool resource_requested;
	bool reschedule_work;
	bool work_in_progress;
	unsigned long jiffies;
	struct wakeup_source *w_lock;
	char w_lock_name[MAX_WS_NAME];
};

static struct ipa_rm_it_private ipa_rm_it_handles[IPA_RM_RESOURCE_MAX];

/**
 * ipa_rm_inactivity_timer_func() - called when timer expired in
 * the context of the shared workqueue. Checks internally if
 * reschedule_work flag is set. In case it is not set this function calls to
 * ipa_rm_release_resource(). In case reschedule_work is set this function
 * reschedule the work. This flag is cleared cleared when
 * calling to ipa_rm_inactivity_timer_release_resource().
 *
 * @work: work object provided by the work queue
 *
 * Return codes:
 * None
 */
static void ipa_rm_inactivity_timer_func(struct work_struct *work)
{

	struct ipa_rm_it_private *me = container_of(to_delayed_work(work),
						    struct ipa_rm_it_private,
						    work);
	unsigned long flags;

	IPA_RM_DBG_LOW("timer expired for resource %d\n", me->resource_name);

	spin_lock_irqsave(
		&ipa_rm_it_handles[me->resource_name].lock, flags);
	if (ipa_rm_it_handles[me->resource_name].reschedule_work) {
		IPA_RM_DBG_LOW("setting delayed work\n");
		ipa_rm_it_handles[me->resource_name].reschedule_work = false;
		queue_delayed_work(system_unbound_wq,
			&ipa_rm_it_handles[me->resource_name].work,
			ipa_rm_it_handles[me->resource_name].jiffies);
	} else if (ipa_rm_it_handles[me->resource_name].resource_requested) {
		IPA_RM_DBG_LOW("not calling release\n");
		ipa_rm_it_handles[me->resource_name].work_in_progress = false;
	} else {
		IPA_RM_DBG_LOW("calling release_resource on resource %d\n",
			me->resource_name);
		__pm_relax(ipa_rm_it_handles[me->resource_name].w_lock);
		ipa_rm_release_resource(me->resource_name);
		ipa_rm_it_handles[me->resource_name].work_in_progress = false;
	}
	spin_unlock_irqrestore(
		&ipa_rm_it_handles[me->resource_name].lock, flags);
}

/**
 * ipa_rm_inactivity_timer_init() - Init function for IPA RM
 * inactivity timer. This function shall be called prior calling
 * any other API of IPA RM inactivity timer.
 *
 * @resource_name: Resource name. @see ipa_rm.h
 * @msecs: time in miliseccond, that IPA RM inactivity timer
 * shall wait prior calling to ipa_rm_release_resource().
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int ipa_rm_inactivity_timer_init(enum ipa_rm_resource_name resource_name,
				 unsigned long msecs)
{
	char *name;

	IPA_RM_DBG_LOW("resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPA_RM_ERR("Invalid parameter\n");
		return -EINVAL;
	}

	if (ipa_rm_it_handles[resource_name].initied) {
		IPA_RM_ERR("resource %d already inited\n", resource_name);
		return -EINVAL;
	}

	spin_lock_init(&ipa_rm_it_handles[resource_name].lock);
	ipa_rm_it_handles[resource_name].resource_name = resource_name;
	ipa_rm_it_handles[resource_name].jiffies = msecs_to_jiffies(msecs);
	ipa_rm_it_handles[resource_name].resource_requested = false;
	ipa_rm_it_handles[resource_name].reschedule_work = false;
	ipa_rm_it_handles[resource_name].work_in_progress = false;
	name = ipa_rm_it_handles[resource_name].w_lock_name;
	snprintf(name, MAX_WS_NAME, "IPA_RM%d\n", resource_name);
	ipa_rm_it_handles[resource_name].w_lock =
		wakeup_source_register(NULL, name);
	if (!ipa_rm_it_handles[resource_name].w_lock) {
		IPA_RM_ERR("IPA wakeup source register failed %s\n",
			   name);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&ipa_rm_it_handles[resource_name].work,
			  ipa_rm_inactivity_timer_func);
	ipa_rm_it_handles[resource_name].initied = true;

	return 0;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_init);

/**
 * ipa_rm_inactivity_timer_destroy() - De-Init function for IPA
 * RM inactivity timer.
 * @resource_name: Resource name. @see ipa_rm.h
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int ipa_rm_inactivity_timer_destroy(enum ipa_rm_resource_name resource_name)
{
	IPA_RM_DBG_LOW("resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPA_RM_ERR("Invalid parameter\n");
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPA_RM_ERR("resource %d already inited\n",
			resource_name);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&ipa_rm_it_handles[resource_name].work);
	wakeup_source_unregister(ipa_rm_it_handles[resource_name].w_lock);

	memset(&ipa_rm_it_handles[resource_name], 0,
	       sizeof(struct ipa_rm_it_private));

	return 0;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_destroy);

/**
 * ipa_rm_inactivity_timer_request_resource() - Same as
 * ipa_rm_request_resource(), with a difference that calling to
 * this function will also cancel the inactivity timer, if
 * ipa_rm_inactivity_timer_release_resource() was called earlier.
 *
 * @resource_name: Resource name. @see ipa_rm.h
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int ipa_rm_inactivity_timer_request_resource(
				enum ipa_rm_resource_name resource_name)
{
	int ret;
	unsigned long flags;

	IPA_RM_DBG_LOW("resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPA_RM_ERR("Invalid parameter\n");
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPA_RM_ERR("Not initialized\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ipa_rm_it_handles[resource_name].lock, flags);
	ipa_rm_it_handles[resource_name].resource_requested = true;
	spin_unlock_irqrestore(&ipa_rm_it_handles[resource_name].lock, flags);
	ret = ipa_rm_request_resource(resource_name);
	IPA_RM_DBG_LOW("resource %d: returning %d\n", resource_name, ret);

	return ret;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_request_resource);

/**
 * ipa_rm_inactivity_timer_release_resource() - Sets the
 * inactivity timer to the timeout set by
 * ipa_rm_inactivity_timer_init(). When the timeout expires, IPA
 * RM inactivity timer will call to ipa_rm_release_resource().
 * If a call to ipa_rm_inactivity_timer_request_resource() was
 * made BEFORE the timeout has expired, rge timer will be
 * cancelled.
 *
 * @resource_name: Resource name. @see ipa_rm.h
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int ipa_rm_inactivity_timer_release_resource(
				enum ipa_rm_resource_name resource_name)
{
	unsigned long flags;

	IPA_RM_DBG_LOW("resource %d\n", resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPA_RM_ERR("Invalid parameter\n");
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPA_RM_ERR("Not initialized\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ipa_rm_it_handles[resource_name].lock, flags);
	ipa_rm_it_handles[resource_name].resource_requested = false;
	if (ipa_rm_it_handles[resource_name].work_in_progress) {
		IPA_RM_DBG_LOW("Timer already set, no sched again %d\n",
		    resource_name);
		ipa_rm_it_handles[resource_name].reschedule_work = true;
		spin_unlock_irqrestore(
			&ipa_rm_it_handles[resource_name].lock, flags);
		return 0;
	}
	ipa_rm_it_handles[resource_name].work_in_progress = true;
	ipa_rm_it_handles[resource_name].reschedule_work = false;
	__pm_stay_awake(ipa_rm_it_handles[resource_name].w_lock);
	IPA_RM_DBG_LOW("setting delayed work\n");
	queue_delayed_work(system_unbound_wq,
			      &ipa_rm_it_handles[resource_name].work,
			      ipa_rm_it_handles[resource_name].jiffies);
	spin_unlock_irqrestore(&ipa_rm_it_handles[resource_name].lock, flags);

	return 0;
}
EXPORT_SYMBOL(ipa_rm_inactivity_timer_release_resource);

