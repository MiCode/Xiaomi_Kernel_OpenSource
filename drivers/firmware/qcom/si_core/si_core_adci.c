// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/kthread.h>

#include "si_core.h"

static struct task_struct *adci_task;
static struct si_object_invoke_ctx oic;

static void wait_to_die(void)
{
	for (;;) {
		set_current_state(TASK_UNINTERRUPTIBLE);
		if (kthread_should_stop())
			break;

		schedule();
	}

	__set_current_state(TASK_RUNNING);
}

static int adci_fn(void *unused)
{
	int ret, result = 0;
	struct si_arg args[1] = { 0 };

	/* IClientEnv_OP_adciAccept is 8. */
	ret = si_object_do_invoke(&oic, ROOT_SI_OBJECT, 8, args, &result);

	if (ret)
		pr_err("unable to register ADCI thread (%d).\n", ret);
	else if (result == OBJECT_ERROR_INVALID)
		pr_err("ADCI feature is not supported on this chipsets.\n");

	pr_debug("exited.\n");

	/* Let's wait for someone to collect our result. */
	if (!kthread_should_stop())
		wait_to_die();

	return result;
}

void adci_start(void)
{
	adci_task = kthread_run(adci_fn, NULL, "adci_thread");

	/* Who cares if it fails?! */
	if (IS_ERR(adci_task))
		pr_err("failed (%ld).\n", PTR_ERR(adci_task));
}

int adci_shutdown(void)
{
	int ret, result = 0;
	struct si_arg args[1] = { 0 };

	/* IClientEnv_OP_adciShutdown is 9. */
	ret = si_object_do_invoke(&oic, ROOT_SI_OBJECT, 9, args, &result);

	if (ret || result)
		pr_err("failed (ret = %d, %d).\n", ret, result);

	/* If IClientEnv_OP_adciShutdown fails, we may stuck here. */
	kthread_stop(adci_task);

	return ret;
}
