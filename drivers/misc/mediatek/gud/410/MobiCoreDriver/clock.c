/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
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

#include "platform.h"

#ifdef MC_CRYPTO_CLOCK_MANAGEMENT

#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/of.h>

#include "main.h"
#include "clock.h"

static struct clk_context {
	struct clk		*mc_ce_iface_clk;
	struct clk		*mc_ce_core_clk;
	struct clk		*mc_ce_bus_clk;
	struct clk		*mc_ce_core_src_clk;
	/* Clocks are managed by Linux Kernel. No need to do anything */
	bool			no_clock_support;
} clk_ctx;

int mc_clock_init(void)
{
	int ret;
#ifdef MC_CLOCK_CORESRC_DEFAULTRATE
	int core_src_rate = MC_CLOCK_CORESRC_DEFAULTRATE;
#ifdef MC_CRYPTO_CLOCK_CORESRC_PROPNAME
	u32 of_core_src_rate = MC_CLOCK_CORESRC_DEFAULTRATE;
#endif
#endif
#ifdef TT_CRYPTO_NO_CLOCK_SUPPORT_FEATURE
	struct device_node *np;

	np = of_find_node_by_name(NULL, TT_CLOCK_DEVICE_NAME);
	if (!np) {
		ret = -ENOENT;
		mc_dev_err(ret, "cannot get clock device from DT");
		goto error;
	}

	clk_ctx.no_clock_support =
		of_property_read_bool(np, TT_CRYPTO_NO_CLOCK_SUPPORT_FEATURE);
	if (clk_ctx.no_clock_support)
		return 0;
#endif /* TT_CRYPTO_NO_CLOCK_SUPPORT_FEATURE */

#ifdef MC_CLOCK_CORESRC_DEFAULTRATE
#ifdef MC_CRYPTO_CLOCK_CORESRC_PROPNAME
	/* Get core clk src */
	clk_ctx.mc_ce_core_src_clk = clk_get(g_ctx.mcd, "core_clk_src");
	if (IS_ERR_OR_NULL(clk_ctx.mc_ce_core_src_clk)) {
		ret = PTR_ERR(clk_ctx.mc_ce_core_src_clk);
		mc_dev_err(ret, "cannot get core src clock");
		goto error;
	}
#endif

#ifdef MC_CRYPTO_CLOCK_CORESRC_PROPNAME
	ret = of_property_read_u32(g_ctx.mcd->of_node,
				   MC_CRYPTO_CLOCK_CORESRC_PROPNAME,
				   &of_core_src_rate);
	if (ret) {
		core_src_rate = MC_CLOCK_CORESRC_DEFAULTRATE;
		mc_dev_info("cannot get clock frequency from DT, use %d",
			    core_src_rate);
	} else {
		core_src_rate = of_core_src_rate;
	}

#endif /* MC_CRYPTO_CLOCK_CORESRC_PROPNAME */

	ret = clk_set_rate(clk_ctx.mc_ce_core_src_clk, core_src_rate);
	if (ret) {
		clk_put(clk_ctx.mc_ce_core_src_clk);
		clk_ctx.mc_ce_core_src_clk = NULL;
		mc_dev_err(ret, "cannot set core clock src rate");
		ret = -EIO;
		goto error;
	}
#endif  /* MC_CLOCK_CORESRC_DEFAULTRATE */

	/* Get core clk */
	clk_ctx.mc_ce_core_clk = clk_get(g_ctx.mcd, "core_clk");
	if (IS_ERR(clk_ctx.mc_ce_core_clk)) {
		ret = PTR_ERR(clk_ctx.mc_ce_core_clk);
		mc_dev_err(ret, "cannot get core clock");
		goto error;
	}

	/* Get Interface clk */
	clk_ctx.mc_ce_iface_clk = clk_get(g_ctx.mcd, "iface_clk");
	if (IS_ERR(clk_ctx.mc_ce_iface_clk)) {
		clk_put(clk_ctx.mc_ce_core_clk);
		ret = PTR_ERR(clk_ctx.mc_ce_iface_clk);
		mc_dev_err(ret, "cannot get iface clock");
		goto error;
	}

	/* Get AXI clk */
	clk_ctx.mc_ce_bus_clk = clk_get(g_ctx.mcd, "bus_clk");
	if (IS_ERR(clk_ctx.mc_ce_bus_clk)) {
		clk_put(clk_ctx.mc_ce_iface_clk);
		clk_put(clk_ctx.mc_ce_core_clk);
		ret = PTR_ERR(clk_ctx.mc_ce_bus_clk);
		mc_dev_err(ret, "cannot get AXI bus clock");
		goto error;
	}

	return 0;

error:
	clk_ctx.mc_ce_core_clk = NULL;
	clk_ctx.mc_ce_iface_clk = NULL;
	clk_ctx.mc_ce_bus_clk = NULL;
	clk_ctx.mc_ce_core_src_clk = NULL;
	return ret;
}

void mc_clock_exit(void)
{
	if (clk_ctx.no_clock_support)
		return;

	if (clk_ctx.mc_ce_iface_clk)
		clk_put(clk_ctx.mc_ce_iface_clk);

	if (clk_ctx.mc_ce_core_clk)
		clk_put(clk_ctx.mc_ce_core_clk);

	if (clk_ctx.mc_ce_bus_clk)
		clk_put(clk_ctx.mc_ce_bus_clk);

	if (clk_ctx.mc_ce_core_src_clk)
		clk_put(clk_ctx.mc_ce_core_src_clk);
}

int mc_clock_enable(void)
{
	int ret;

	if (clk_ctx.no_clock_support)
		return 0;

	ret = clk_prepare_enable(clk_ctx.mc_ce_core_clk);
	if (ret) {
		mc_dev_err(ret, "cannot enable core clock");
		goto err_core;
	}

	ret = clk_prepare_enable(clk_ctx.mc_ce_iface_clk);
	if (ret) {
		mc_dev_err(ret, "cannot enable interface clock");
		goto err_iface;
	}

	ret = clk_prepare_enable(clk_ctx.mc_ce_bus_clk);
	if (ret) {
		mc_dev_err(ret, "cannot enable bus clock");
		goto err_bus;
	}

	return 0;

err_bus:
	clk_disable_unprepare(clk_ctx.mc_ce_iface_clk);
err_iface:
	clk_disable_unprepare(clk_ctx.mc_ce_core_clk);
err_core:
	return ret;
}

void mc_clock_disable(void)
{
	if (clk_ctx.no_clock_support)
		return;

	if (clk_ctx.mc_ce_iface_clk)
		clk_disable_unprepare(clk_ctx.mc_ce_iface_clk);

	if (clk_ctx.mc_ce_core_clk)
		clk_disable_unprepare(clk_ctx.mc_ce_core_clk);

	if (clk_ctx.mc_ce_bus_clk)
		clk_disable_unprepare(clk_ctx.mc_ce_bus_clk);
}

#endif /* MC_CRYPTO_CLOCK_MANAGEMENT */
