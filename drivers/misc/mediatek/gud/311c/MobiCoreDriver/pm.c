/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
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

#include "platform.h"	/* MC_BL_NOTIFIER */

#ifdef MC_BL_NOTIFIER
#include "main.h"
#include "scheduler.h" /* SWd suspend/resume commands */
#include "pm.h"
#include <asm/bL_switcher.h>

static struct pm_context {
	struct notifier_block bl_swicher_notifier;
} pm_ctx;

static int bl_switcher_notifier_handler(struct notifier_block *this,
					unsigned long event, void *ptr)
{
	unsigned int mpidr, cpu;
	int ret = 0;

	asm volatile ("mrc\tp15, 0, %0, c0, c0, 5" : "=r" (mpidr));
	cpu = mpidr & 0x3;
	mc_dev_devel("%s switching!!, cpu: %u, Out=%u\n",
		     event == SWITCH_ENTER ? "Before" : "After", cpu,
		     (mpidr >> 8) & 0xf);

	if (cpu != 0)
		return 0;

	switch (event) {
	case SWITCH_ENTER:
		ret = mc_scheduler_suspend();
		break;
	case SWITCH_EXIT:
		ret = mc_scheduler_resume();
		break;
	default:
		mc_dev_devel("MobiCore: Unknown switch event!\n");
	}

	return ret;
}

int mc_pm_start(void)
{
	pm_ctx.bl_swicher_notifier.notifier_call = bl_switcher_notifier_handler;
	register_bL_swicher_notifier(&pm_ctx.bl_swicher_notifier);
	return 0;
}

void mc_pm_stop(void)
{
	unregister_bL_swicher_notifier(&pm_ctx.bl_swicher_notifier);
}

#endif /* MC_BL_NOTIFIER */
