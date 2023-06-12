// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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
