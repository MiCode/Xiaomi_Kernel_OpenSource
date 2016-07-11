/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/unistd.h>
#include <linux/workqueue.h>
#include <linux/ipa.h>
#include "ipa_i.h"

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
};

static struct ipa_rm_it_private ipa_rm_it_handles[IPA_RM_RESOURCE_MAX];

/**
 * ipa_rm_inactivity_timer_func() - called when timer expired in
 * the context of the shared workqueue. Checks internally if
 * reschedule_work flag is set. In case it is not set this function calls to
 * ipa2_rm_release_resource(). In case reschedule_work is set this function
 * reschedule the work. This flag is cleared cleared when
 * calling to ipa2_rm_inactivity_timer_release_resource().
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

	IPADBG("%s: timer expired for resource %d!\n", __func__,
	    me->resource_name);

	spin_lock_irqsave(
		&ipa_rm_it_handles[me->resource_name].lock, flags);
	if (ipa_rm_it_handles[me->resource_name].reschedule_work) {
		IPADBG("%s: setting delayed work\n", __func__);
		ipa_rm_it_handles[me->resource_name].reschedule_work = false;
		queue_delayed_work(system_unbound_wq,
			&ipa_rm_it_handles[me->resource_name].work,
			ipa_rm_it_handles[me->resource_name].jiffies);
	} else if (ipa_rm_it_handles[me->resource_name].resource_requested) {
		IPADBG("%s: not calling release\n", __func__);
		ipa_rm_it_handles[me->resource_name].work_in_progress = false;
	} else {
		IPADBG("%s: calling release_resource on resource %d!\n",
			__func__, me->resource_name);
		ipa2_rm_release_resource(me->resource_name);
		ipa_rm_it_handles[me->resource_name].work_in_progress = false;
	}
	spin_unlock_irqrestore(
		&ipa_rm_it_handles[me->resource_name].lock, flags);
}

/**
* ipa2_rm_inactivity_timer_init() - Init function for IPA RM
* inactivity timer. This function shall be called prior calling
* any other API of IPA RM inactivity timer.
*
* @resource_name: Resource name. @see ipa_rm.h
* @msecs: time in miliseccond, that IPA RM inactivity timer
* shall wait prior calling to ipa2_rm_release_resource().
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa2_rm_inactivity_timer_init(enum ipa_rm_resource_name resource_name,
				 unsigned long msecs)
{
	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: resource %d already inited\n",
		    __func__, resource_name);
		return -EINVAL;
	}

	spin_lock_init(&ipa_rm_it_handles[resource_name].lock);
	ipa_rm_it_handles[resource_name].resource_name = resource_name;
	ipa_rm_it_handles[resource_name].jiffies = msecs_to_jiffies(msecs);
	ipa_rm_it_handles[resource_name].resource_requested = false;
	ipa_rm_it_handles[resource_name].reschedule_work = false;
	ipa_rm_it_handles[resource_name].work_in_progress = false;

	INIT_DELAYED_WORK(&ipa_rm_it_handles[resource_name].work,
			  ipa_rm_inactivity_timer_func);
	ipa_rm_it_handles[resource_name].initied = 1;

	return 0;
}

/**
* ipa2_rm_inactivity_timer_destroy() - De-Init function for IPA
* RM inactivity timer.
*
* @resource_name: Resource name. @see ipa_rm.h
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa2_rm_inactivity_timer_destroy(enum ipa_rm_resource_name resource_name)
{
	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: resource %d already inited\n",
		    __func__, resource_name);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&ipa_rm_it_handles[resource_name].work);

	memset(&ipa_rm_it_handles[resource_name], 0,
	       sizeof(struct ipa_rm_it_private));

	return 0;
}


/**
* ipa2_rm_inactivity_timer_request_resource() - Same as
* ipa2_rm_request_resource(), with a difference that calling to
* this function will also cancel the inactivity timer, if
* ipa2_rm_inactivity_timer_release_resource() was called earlier.
*
* @resource_name: Resource name. @see ipa_rm.h
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa2_rm_inactivity_timer_request_resource(
				enum ipa_rm_resource_name resource_name)
{
	int ret;
	unsigned long flags;

	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: Not initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&ipa_rm_it_handles[resource_name].lock, flags);
	ipa_rm_it_handles[resource_name].resource_requested = true;
	spin_unlock_irqrestore(&ipa_rm_it_handles[resource_name].lock, flags);
	ret = ipa2_rm_request_resource(resource_name);
	IPADBG("%s: resource %d: returning %d\n", __func__, resource_name, ret);

	return ret;
}

/**
* ipa2_rm_inactivity_timer_release_resource() - Sets the
* inactivity timer to the timeout set by
* ipa2_rm_inactivity_timer_init(). When the timeout expires, IPA
* RM inactivity timer will call to ipa2_rm_release_resource().
* If a call to ipa2_rm_inactivity_timer_request_resource() was
* made BEFORE the timout has expired, rge timer will be
* cancelled.
*
* @resource_name: Resource name. @see ipa_rm.h
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int ipa2_rm_inactivity_timer_release_resource(
				enum ipa_rm_resource_name resource_name)
{
	unsigned long flags;

	IPADBG("%s: resource %d\n", __func__, resource_name);

	if (resource_name < 0 ||
	    resource_name >= IPA_RM_RESOURCE_MAX) {
		IPAERR("%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}

	if (!ipa_rm_it_handles[resource_name].initied) {
		IPAERR("%s: Not initialized\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&ipa_rm_it_handles[resource_name].lock, flags);
	ipa_rm_it_handles[resource_name].resource_requested = false;
	if (ipa_rm_it_handles[resource_name].work_in_progress) {
		IPADBG("%s: Timer already set, not scheduling again %d\n",
		    __func__, resource_name);
		ipa_rm_it_handles[resource_name].reschedule_work = true;
		spin_unlock_irqrestore(
			&ipa_rm_it_handles[resource_name].lock, flags);
		return 0;
	}
	ipa_rm_it_handles[resource_name].work_in_progress = true;
	ipa_rm_it_handles[resource_name].reschedule_work = false;
	IPADBG("%s: setting delayed work\n", __func__);
	queue_delayed_work(system_unbound_wq,
			      &ipa_rm_it_handles[resource_name].work,
			      ipa_rm_it_handles[resource_name].jiffies);
	spin_unlock_irqrestore(&ipa_rm_it_handles[resource_name].lock, flags);

	return 0;
}

