// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/workqueue.h>

#include "si_core.h"

static struct workqueue_struct *si_core_wq;

/* Number of all release requests submitted. */
static atomic_t pending_releases = ATOMIC_INIT(0);

/* Number of release reqests dropped. */
static int release_failed;

/* 'release_user_object' put object in release work queue.
 * 'si_object_do_release' make direct invocation to release an object.
 * 'destroy_user_object' called to finish the job after QTEE acknowledged the release.
 */

static void destroy_user_object(struct work_struct *work);
void release_user_object(struct si_object *object)
{
	INIT_WORK(&object->work, destroy_user_object);

	pr_debug("%s queued for release.\n", si_object_name(object));

	atomic_inc(&pending_releases);

	/* QUEUE a release work. */
	queue_work(si_core_wq, &object->work);
}

static void si_object_do_release(struct si_object *object)
{
	int ret, result;

	/* We are on ordered workqueue; it's safe to do this! */
	static struct si_object_invoke_ctx oic;
	static struct si_arg args[1] = { 0 };

	ret = si_object_do_invoke(&oic, object, SI_OBJECT_OP_RELEASE, args, &result);

	if (ret == -EAGAIN) {

		/* On faioure, failure no callback response is in progress. */

		pr_debug("%s rescheduled for release.\n", si_object_name(object));

		queue_work(si_core_wq, &object->work);
	} else {

		/* On failure, there are two scenarios:
		 *  - ret != 0 while retuning a callback response.
		 *  - ret == 0 and result != 0.
		 * In any of these case, there is nothing we can do to cleanup.
		 */

		if (ret || result) {
			release_failed++;

			pr_err("release failed for %s (%d result = %x); %d objects remain zombie.\n",
				si_object_name(object), ret, result, release_failed);
		}

		atomic_dec(&pending_releases);

		kfree(object->name);

		free_si_object(object);
	}
}

static void destroy_user_object(struct work_struct *work)
{
	struct si_object *object = container_of(work, struct si_object, work);

	pr_debug("%s releasing.\n", si_object_name(object));

	si_object_do_release(object);
}

ssize_t release_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d %d\n",
		atomic_read(&pending_releases), release_failed);
}

/* 'init_si_core_wq' and 'destroy_si_core_wq'. */

int init_si_core_wq(void)
{

	/* We use ordered workqueue. If decide to change to workqueue with more
	 * concurrency make sure to update 'si_object_do_release'.
	 */

	si_core_wq = alloc_ordered_workqueue("si_core_wq", 0);
	if (!si_core_wq) {
		pr_err("failed to create si_core_wq.\n");

		return -ENOMEM;
	}

	return 0;
}

void destroy_si_core_wq(void)
{
	destroy_workqueue(si_core_wq);
}
