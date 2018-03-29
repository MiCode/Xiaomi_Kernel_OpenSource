/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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
/*
 * MobiCore Driver Kernel Module.
 * This module is written as a Linux device driver.
 * This driver represents the command proxy on the lowest layer, from the
 * secure world to the non secure world, and vice versa.
 * This driver is located in the non secure world (Linux).
 * This driver offers IOCTL commands, for access to the secure world, and has
 * the interface from the secure world to the normal world.
 * The access to the driver is possible with a file descriptor,
 * which has to be created by the fd = open(/dev/mobicore) command.
 */
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/suspend.h>
#include <linux/device.h>

#include "main.h"
#include "pm.h"
#include "fastcall.h"
#include "ops.h"
#include "logging.h"
#include "debug.h"

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT
	#include <linux/clk.h>
	#include <linux/err.h>

	struct clk *mc_ce_iface_clk = NULL;
	struct clk *mc_ce_core_clk = NULL;
	struct clk *mc_ce_bus_clk = NULL;
#endif /* MC_CRYPTO_CLOCK_MANAGEMENT */

#ifdef MC_PM_RUNTIME

static struct mc_context *ctx;

static bool sleep_ready(void)
{
	if (!ctx->mcp)
		return false;

	if (!(ctx->mcp->flags.sleep_mode.ready_to_sleep & READY_TO_SLEEP))
		return false;

	return true;
}

static void mc_suspend_handler(struct work_struct *work)
{
	if (!ctx->mcp)
		return;

	ctx->mcp->flags.sleep_mode.sleep_req = REQ_TO_SLEEP;
	_nsiq();
}
DECLARE_WORK(suspend_work, mc_suspend_handler);

static inline void dump_sleep_params(struct mc_flags *flags)
{
	MCDRV_DBG(mcd, "MobiCore IDLE=%d!", flags->schedule);
	MCDRV_DBG(mcd,
		  "MobiCore Request Sleep=%d!", flags->sleep_mode.sleep_req);
	MCDRV_DBG(mcd,
		  "MobiCore Sleep Ready=%d!", flags->sleep_mode.ready_to_sleep);
}

static int mc_suspend_notifier(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
	struct mc_mcp_buffer *mcp = ctx->mcp;
	/* We have noting to say if MobiCore is not initialized */
	if (!mcp)
		return 0;

#ifdef MC_MEM_TRACES
	mobicore_log_read();
#endif

	switch (event) {
	case PM_SUSPEND_PREPARE:
		/*
		 * Make sure we have finished all the work otherwise
		 * we end up in a race condition
		 */
		cancel_work_sync(&suspend_work);
		/*
		 * We can't go to sleep if MobiCore is not IDLE
		 * or not Ready to sleep
		 */
		dump_sleep_params(&mcp->flags);
		if (!sleep_ready()) {
			ctx->mcp->flags.sleep_mode.sleep_req = REQ_TO_SLEEP;
			schedule_work_on(0, &suspend_work);
			flush_work(&suspend_work);
			if (!sleep_ready()) {
				dump_sleep_params(&mcp->flags);
				ctx->mcp->flags.sleep_mode.sleep_req = 0;
				MCDRV_DBG_ERROR(mcd, "MobiCore can't SLEEP!");
				return NOTIFY_BAD;
			}
		}
		break;
	case PM_POST_SUSPEND:
		MCDRV_DBG(mcd, "Resume MobiCore system!");
		ctx->mcp->flags.sleep_mode.sleep_req = 0;
		break;
	default:
		break;
	}
	return 0;
}

static struct notifier_block mc_notif_block = {
	.notifier_call = mc_suspend_notifier,
};

#ifdef MC_BL_NOTIFIER

static int bl_switcher_notifier_handler(struct notifier_block *this,
			unsigned long event, void *ptr)
{
	unsigned int mpidr, cpu, cluster;
	struct mc_mcp_buffer *mcp = ctx->mcp;

	if (!mcp)
		return 0;

	asm volatile ("mrc\tp15, 0, %0, c0, c0, 5" : "=r" (mpidr));
	cpu = mpidr & 0x3;
	cluster = (mpidr >> 8) & 0xf;
	MCDRV_DBG(mcd, "%s switching!!, cpu: %u, Out=%u",
		  (event == SWITCH_ENTER ? "Before" : "After"), cpu, cluster);

	if (cpu != 0)
		return 0;

	switch (event) {
	case SWITCH_ENTER:
		if (!sleep_ready()) {
			ctx->mcp->flags.sleep_mode.sleep_req = REQ_TO_SLEEP;
			_nsiq();
			/* By this time we should be ready for sleep or we are
			 * in the middle of something important */
			if (!sleep_ready()) {
				dump_sleep_params(&mcp->flags);
				MCDRV_DBG(mcd,
					  "MobiCore: Don't allow switch!");
				ctx->mcp->flags.sleep_mode.sleep_req = 0;
				return -EPERM;
			}
		}
		break;
	case SWITCH_EXIT:
			ctx->mcp->flags.sleep_mode.sleep_req = 0;
			break;
	default:
		MCDRV_DBG(mcd, "MobiCore: Unknown switch event!");
	}

	return 0;
}

static struct notifier_block switcher_nb = {
	.notifier_call = bl_switcher_notifier_handler,
};
#endif

int mc_pm_initialize(struct mc_context *context)
{
	int ret = 0;

	ctx = context;

	ret = register_pm_notifier(&mc_notif_block);
	if (ret)
		MCDRV_DBG_ERROR(mcd, "device pm register failed");
#ifdef MC_BL_NOTIFIER
	if (register_bL_swicher_notifier(&switcher_nb))
		MCDRV_DBG_ERROR(mcd,
				"Failed to register to bl_switcher_notifier");
#endif

	return ret;
}

int mc_pm_free(void)
{
	int ret = unregister_pm_notifier(&mc_notif_block);
	if (ret)
		MCDRV_DBG_ERROR(mcd, "device pm unregister failed");
#ifdef MC_BL_NOTIFIER
	ret = unregister_bL_swicher_notifier(&switcher_nb);
	if (ret)
		MCDRV_DBG_ERROR(mcd, "device bl unregister failed");
#endif
	return ret;
}

bool mc_pm_sleep_ready(void)
{
	if (ctx == 0)
		return true;
	return sleep_ready();
}
#endif /* MC_PM_RUNTIME */

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT

int mc_pm_clock_initialize(void)
{
	int ret = 0;

	/* Get core clk */
	mc_ce_core_clk = clk_get(mcd, "core_clk");
	if (IS_ERR(mc_ce_core_clk)) {
		ret = PTR_ERR(mc_ce_core_clk);
		MCDRV_DBG_ERROR(mcd, "cannot get core clock");
		goto error;
	}
	/* Get Interface clk */
	mc_ce_iface_clk = clk_get(mcd, "iface_clk");
	if (IS_ERR(mc_ce_iface_clk)) {
		clk_put(mc_ce_core_clk);
		ret = PTR_ERR(mc_ce_iface_clk);
		MCDRV_DBG_ERROR(mcd, "cannot get iface clock");
		goto error;
	}
	/* Get AXI clk */
	mc_ce_bus_clk = clk_get(mcd, "bus_clk");
	if (IS_ERR(mc_ce_bus_clk)) {
		clk_put(mc_ce_iface_clk);
		clk_put(mc_ce_core_clk);
		ret = PTR_ERR(mc_ce_bus_clk);
		MCDRV_DBG_ERROR(mcd, "cannot get AXI bus clock");
		goto error;
	}
	return ret;

error:
	mc_ce_core_clk = NULL;
	mc_ce_iface_clk = NULL;
	mc_ce_bus_clk = NULL;

	return ret;
}

void mc_pm_clock_finalize(void)
{
	if (mc_ce_iface_clk != NULL)
		clk_put(mc_ce_iface_clk);

	if (mc_ce_core_clk != NULL)
		clk_put(mc_ce_core_clk);

	if (mc_ce_bus_clk != NULL)
		clk_put(mc_ce_bus_clk);
}

int mc_pm_clock_enable(void)
{
	int rc = 0;

	rc = clk_prepare_enable(mc_ce_core_clk);
	if (rc) {
		MCDRV_DBG_ERROR(mcd, "cannot enable clock");
	} else {
		rc = clk_prepare_enable(mc_ce_iface_clk);
		if (rc) {
			clk_disable_unprepare(mc_ce_core_clk);
			MCDRV_DBG_ERROR(mcd, "cannot enable clock");
		} else {
			rc = clk_prepare_enable(mc_ce_bus_clk);
			if (rc) {
				clk_disable_unprepare(mc_ce_iface_clk);
				MCDRV_DBG_ERROR(mcd, "cannot enable clock");
			}
		}
	}
	return rc;
}

void mc_pm_clock_disable(void)
{
	if (mc_ce_iface_clk != NULL)
		clk_disable_unprepare(mc_ce_iface_clk);

	if (mc_ce_core_clk != NULL)
		clk_disable_unprepare(mc_ce_core_clk);

	if (mc_ce_bus_clk != NULL)
		clk_disable_unprepare(mc_ce_bus_clk);
}

#endif /* MC_CRYPTO_CLOCK_MANAGEMENT */
