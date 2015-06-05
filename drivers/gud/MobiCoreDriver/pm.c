/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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
#include <linux/module.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <linux/err.h>

#include "public/mc_linux.h"

#include "platform.h"	/* MC_PM_RUNTIME */
#include "debug.h"
#include "scheduler.h"	/* SWd suspend/resume commands */
#include "pm.h"

#ifdef MC_PM_RUNTIME
static struct pm_context {
	struct notifier_block pm_notifier;
} pm_ctx;

static int mc_suspend_notifier(struct notifier_block *nb, unsigned long event,
			       void *dummy)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		return mc_scheduler_suspend();
	case PM_POST_SUSPEND:
		return mc_scheduler_resume();
	}

	return 0;
}


/* CPI todo: inconsistent handling of ret in below 2 functions */
int mc_pm_start(void)
{
	int ret = 0;

	pm_ctx.pm_notifier.notifier_call = mc_suspend_notifier;
	ret = register_pm_notifier(&pm_ctx.pm_notifier);
	MCDRV_DBG_VERBOSE("done, ret = %d", ret);

	return ret;
}

void mc_pm_stop(void)
{
	unregister_pm_notifier(&pm_ctx.pm_notifier);
}

#endif /* MC_PM_RUNTIME */
