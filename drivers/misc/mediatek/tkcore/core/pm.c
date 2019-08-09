/*
 * Copyright (c) 2015-2018 TrustKernel Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/suspend.h>

#include <linux/atomic.h>

#include "pm.h"

/* tee_wakeup_cnt == 0 means currently
 * the system is in idle state, without
 * wakeup source or suspend request.
 * wakeup source > 0 means there are outgoing
 * commands which stop the system from suspending.
 * wakeup source == -1 means the system is now
 * preparing for suspend or suspending
 */
static atomic_t tee_wakeup_cnt = ATOMIC_INIT(0);

static DECLARE_WAIT_QUEUE_HEAD(suspend_done);
static DECLARE_WAIT_QUEUE_HEAD(awake_done);

static void tee_keep_awake(void)
{
	while (atomic_inc_unless_negative(&tee_wakeup_cnt) == 0)
		wait_event_freezable(suspend_done,
			atomic_read(&tee_wakeup_cnt) >= 0);
}

static void tee_cancel_awake(void)
{
	if (atomic_dec_and_test(&tee_wakeup_cnt)) {
		/* wake_up() implies a memory barrier */
		wake_up(&awake_done);
	}
}

static void tee_prepare_suspend(void)
{
	int r;

	while ((r = atomic_read(&tee_wakeup_cnt)) >= 0) {
		if (atomic_cmpxchg(&tee_wakeup_cnt, 0, -1) == 0)
			return;
		/* wait_event() implies a memory barrier */
		wait_event(awake_done, atomic_read(&tee_wakeup_cnt) == 0);
	}

	pr_warn("tee_wakeup_cnt unexpected value: %d\n", r);
}

#ifdef CONFIG_TRUSTKERNEL_TEE_RPMB_SUPPORT
#include "linux/tee_rpmb.h"
#endif

static void tee_post_suspend(void)
{
	/* we do not need to use atomic instruction here,
	 * because there is one single suspend source.
	 */
	atomic_set(&tee_wakeup_cnt, atomic_read(&tee_wakeup_cnt) + 1);
	wake_up(&suspend_done);
}

static int tee_pm_suspend_notifier(struct notifier_block *nb,
				   unsigned long event, void *dummy)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		tee_prepare_suspend();

#ifdef CONFIG_TRUSTKERNEL_TEE_RPMB_SUPPORT
		{
			struct tkcore_rpmb_request rq = {
				.type = TEE_RPMB_SWITCH_NORMAL
			};
			tkcore_emmc_rpmb_execute(&rq);
		}
#endif
		break;
	case PM_POST_SUSPEND:
		tee_post_suspend();
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block tz_pm_notifier = {
	.notifier_call = tee_pm_suspend_notifier,
};

int tkcore_stay_awake(void *fn, void *data)
{
	int r;

	tee_keep_awake();
	r = ((int (*) (void *)) fn) (data);
	tee_cancel_awake();

	return r;
}

int tkcore_tee_pm_init(void)
{
	int r;

	r = register_pm_notifier(&tz_pm_notifier);
	if (r) {
		pr_err("failed to register pm notifier: %d\n", r);
		return r;
	}

	return 0;
}

void tkcore_tee_pm_exit(void)
{
	int r;

	r = unregister_pm_notifier(&tz_pm_notifier);
	if (r)
		pr_err("failed to unregister_pm_notifier: %d\n", r);
}
