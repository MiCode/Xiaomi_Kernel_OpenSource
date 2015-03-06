/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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
#include <linux/clk.h>
#include <linux/err.h>

#include "public/mc_linux.h"

#include "mci/mcimcp.h"

#include "main.h"
#include "fastcall.h"
#include "logging.h"
#include "debug.h"
#include "mcp.h"
#include "scheduler.h"
#include "pm.h"

#if defined(MC_CRYPTO_CLOCK_MANAGEMENT) && defined(MC_USE_DEVICE_TREE)
	#include <linux/of.h>
	#define QSEE_CE_CLK_100MHZ 100000000
#endif /* MC_CRYPTO_CLOCK_MANAGEMENT && MC_USE_DEVICE_TREE */

#ifdef MC_PM_RUNTIME

static void mc_suspend_handler(struct work_struct *work)
{
	if (!mcp_set_sleep_mode_rq(MC_FLAG_REQ_TO_SLEEP))
		mc_dev_nsiq();
}

static int mc_suspend_notifier(struct notifier_block *nb, unsigned long event,
			       void *dummy)
{
	int ret = 0;

	mobicore_log_read();

	switch (event) {
	case PM_SUSPEND_PREPARE:
		/*
		 * Make sure we have finished all the work otherwise
		 * we end up in a race condition
		 */
		cancel_work_sync(&g_ctx.suspend_work);
		ret = mcp_suspend_prepare();
		break;

	case PM_POST_SUSPEND:
		MCDRV_DBG("Resume MobiCore system!");
		ret = mcp_set_sleep_mode_rq(MC_FLAG_NO_SLEEP_REQ);
		break;
	default:
		break;
	}
	return ret;
}


/* CPI todo: inconsistent handling of ret in below 2 functions */
int mc_pm_initialize(void)
{
	int ret = 0;

	INIT_WORK(&g_ctx.suspend_work, mc_suspend_handler);
	g_ctx.mc_notif_block.notifier_call = mc_suspend_notifier;

	ret = register_pm_notifier(&g_ctx.mc_notif_block);

	MCDRV_DBG_VERBOSE("done, ret = %d", ret);

	return ret;
}

int mc_pm_free(void)
{
	int ret = unregister_pm_notifier(&g_ctx.mc_notif_block);

	return ret;
}

#endif /* MC_PM_RUNTIME */

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT

int mc_pm_clock_initialize(void)
{
	int ret = 0;

#ifdef MC_USE_DEVICE_TREE
	/* Get core clk src */
	g_ctx.mc_ce_core_src_clk = clk_get(g_ctx.mcd, "core_clk_src");
	if (IS_ERR(g_ctx.mc_ce_core_src_clk)) {
		ret = PTR_ERR(g_ctx.mc_ce_core_src_clk);
		MCDRV_ERROR(
				"cannot get core clock src with error: %d",
				ret);
		goto error;
	} else {
		int ce_opp_freq_hz = QSEE_CE_CLK_100MHZ;

		if (of_property_read_u32(g_ctx.mcd->of_node,
					 "qcom,ce-opp-freq",
					 &ce_opp_freq_hz)) {
			ce_opp_freq_hz = QSEE_CE_CLK_100MHZ;
			MCDRV_ERROR(
					"cannot get ce clock frequency. Using %d",
					ce_opp_freq_hz);
		}
		ret = clk_set_rate(g_ctx.mc_ce_core_src_clk, ce_opp_freq_hz);
		if (ret) {
			clk_put(g_ctx.mc_ce_core_src_clk);
			g_ctx.mc_ce_core_src_clk = NULL;
			MCDRV_ERROR("cannot set core clock src rate");
			ret = -EIO;
			goto error;
		}
	}
#endif  /* MC_CRYPTO_CLOCK_MANAGEMENT && MC_USE_DEVICE_TREE */

	/* Get core clk */
	g_ctx.mc_ce_core_clk = clk_get(g_ctx.mcd, "core_clk");
	if (IS_ERR(g_ctx.mc_ce_core_clk)) {
		ret = PTR_ERR(g_ctx.mc_ce_core_clk);
		MCDRV_ERROR("cannot get core clock");
		goto error;
	}
	/* Get Interface clk */
	g_ctx.mc_ce_iface_clk = clk_get(g_ctx.mcd, "iface_clk");
	if (IS_ERR(g_ctx.mc_ce_iface_clk)) {
		clk_put(g_ctx.mc_ce_core_clk);
		ret = PTR_ERR(g_ctx.mc_ce_iface_clk);
		MCDRV_ERROR("cannot get iface clock");
		goto error;
	}
	/* Get AXI clk */
	g_ctx.mc_ce_bus_clk = clk_get(g_ctx.mcd, "bus_clk");
	if (IS_ERR(g_ctx.mc_ce_bus_clk)) {
		clk_put(g_ctx.mc_ce_iface_clk);
		clk_put(g_ctx.mc_ce_core_clk);
		ret = PTR_ERR(g_ctx.mc_ce_bus_clk);
		MCDRV_ERROR("cannot get AXI bus clock");
		goto error;
	}

	MCDRV_DBG("obtained crypto clocks");
	return ret;

error:
	g_ctx.mc_ce_core_clk = NULL;
	g_ctx.mc_ce_iface_clk = NULL;
	g_ctx.mc_ce_bus_clk = NULL;
#ifdef MC_USE_DEVICE_TREE
	g_ctx.mc_ce_core_src_clk = NULL;
#endif  /* MC_USE_DEVICE_TREE */

	return ret;
}

void mc_pm_clock_finalize(void)
{
	if (g_ctx.mc_ce_iface_clk)
		clk_put(g_ctx.mc_ce_iface_clk);

	if (g_ctx.mc_ce_core_clk)
		clk_put(g_ctx.mc_ce_core_clk);

	if (g_ctx.mc_ce_bus_clk != NULL)
		clk_put(g_ctx.mc_ce_bus_clk);

#ifdef MC_USE_DEVICE_TREE
	if (g_ctx.mc_ce_core_src_clk)
		clk_put(g_ctx.mc_ce_core_src_clk);
#endif  /* MC_CRYPTO_CLOCK_MANAGEMENT && MC_USE_DEVICE_TREE */
}

int mc_pm_clock_enable(void)
{
	int rc = 0;

	rc = clk_prepare_enable(g_ctx.mc_ce_core_clk);
	if (rc) {
		MCDRV_ERROR("cannot enable clock");
	} else {
		rc = clk_prepare_enable(g_ctx.mc_ce_iface_clk);
		if (rc) {
			clk_disable_unprepare(g_ctx.mc_ce_core_clk);
			MCDRV_ERROR("cannot enable clock");
		} else {
			rc = clk_prepare_enable(g_ctx.mc_ce_bus_clk);
			if (rc) {
				clk_disable_unprepare(g_ctx.mc_ce_iface_clk);
				MCDRV_ERROR("cannot enable clock");
			}
		}
	}
	return rc;
}

void mc_pm_clock_disable(void)
{
	if (g_ctx.mc_ce_iface_clk)
		clk_disable_unprepare(g_ctx.mc_ce_iface_clk);

	if (g_ctx.mc_ce_core_clk)
		clk_disable_unprepare(g_ctx.mc_ce_core_clk);

	if (g_ctx.mc_ce_bus_clk)
		clk_disable_unprepare(g_ctx.mc_ce_bus_clk);
}

#endif /* MC_CRYPTO_CLOCK_MANAGEMENT */
