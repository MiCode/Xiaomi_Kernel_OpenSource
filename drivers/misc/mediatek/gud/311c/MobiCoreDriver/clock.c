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
} clk_ctx;

int mc_clock_init(void)
{
	int ret = 0;
#ifdef MC_CLOCK_CORESRC_DEFAULTRATE
	int core_src_rate = MC_CLOCK_CORESRC_DEFAULTRATE;
#ifdef MC_CRYPTO_CLOCK_CORESRC_PROPNAME
	u32 of_core_src_rate = MC_CLOCK_CORESRC_DEFAULTRATE;
#endif
	/* Get core clk src */
	clk_ctx.mc_ce_core_src_clk = clk_get(g_ctx.mcd, "core_clk_src");
	if (IS_ERR(clk_ctx.mc_ce_core_src_clk)) {
		ret = PTR_ERR(clk_ctx.mc_ce_core_src_clk);
		mc_dev_notice("cannot get core src clock: %d\n", ret);
		goto error;
	}

#ifdef MC_CRYPTO_CLOCK_CORESRC_PROPNAME
	if (of_property_read_u32(g_ctx.mcd->of_node,
				 MC_CRYPTO_CLOCK_CORESRC_PROPNAME,
				 &of_core_src_rate)) {
		core_src_rate = MC_CLOCK_CORESRC_DEFAULTRATE;
		mc_dev_notice("cannot get ce clock frequency from DT, use %d\n",
			   core_src_rate);
	} else {
		core_src_rate = of_core_src_rate;
	}
#endif /* MC_CRYPTO_CLOCK_CORESRC_PROPNAME */

	ret = clk_set_rate(clk_ctx.mc_ce_core_src_clk, core_src_rate);
	if (ret) {
		clk_put(clk_ctx.mc_ce_core_src_clk);
		clk_ctx.mc_ce_core_src_clk = NULL;
		mc_dev_notice("cannot set core clock src rate: %d\n", ret);
		ret = -EIO;
		goto error;
	}
#endif  /* MC_CLOCK_CORESRC_DEFAULTRATE */

	/* Get core clk */
	clk_ctx.mc_ce_core_clk = clk_get(g_ctx.mcd, "core_clk");
	if (IS_ERR(clk_ctx.mc_ce_core_clk)) {
		ret = PTR_ERR(clk_ctx.mc_ce_core_clk);
		mc_dev_notice("cannot get core clock: %d\n", ret);
		goto error;
	}
	/* Get Interface clk */
	clk_ctx.mc_ce_iface_clk = clk_get(g_ctx.mcd, "iface_clk");
	if (IS_ERR(clk_ctx.mc_ce_iface_clk)) {
		clk_put(clk_ctx.mc_ce_core_clk);
		ret = PTR_ERR(clk_ctx.mc_ce_iface_clk);
		mc_dev_notice("cannot get iface clock: %d\n", ret);
		goto error;
	}
	/* Get AXI clk */
	clk_ctx.mc_ce_bus_clk = clk_get(g_ctx.mcd, "bus_clk");
	if (IS_ERR(clk_ctx.mc_ce_bus_clk)) {
		clk_put(clk_ctx.mc_ce_iface_clk);
		clk_put(clk_ctx.mc_ce_core_clk);
		ret = PTR_ERR(clk_ctx.mc_ce_bus_clk);
		mc_dev_notice("cannot get AXI bus clock: %d\n", ret);
		goto error;
	}
	return ret;

error:
	clk_ctx.mc_ce_core_clk = NULL;
	clk_ctx.mc_ce_iface_clk = NULL;
	clk_ctx.mc_ce_bus_clk = NULL;
	clk_ctx.mc_ce_core_src_clk = NULL;
	return ret;
}

void mc_clock_exit(void)
{
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
	int rc;

	rc = clk_prepare_enable(clk_ctx.mc_ce_core_clk);
	if (rc) {
		mc_dev_notice("cannot enable core clock\n");
		goto err_core;
	}

	rc = clk_prepare_enable(clk_ctx.mc_ce_iface_clk);
	if (rc) {
		mc_dev_notice("cannot enable interface clock\n");
		goto err_iface;
	}

	rc = clk_prepare_enable(clk_ctx.mc_ce_bus_clk);
	if (rc) {
		mc_dev_notice("cannot enable bus clock\n");
		goto err_bus;
	}

	return 0;

err_bus:
	clk_disable_unprepare(clk_ctx.mc_ce_iface_clk);
err_iface:
	clk_disable_unprepare(clk_ctx.mc_ce_core_clk);
err_core:
	return rc;
}

void mc_clock_disable(void)
{
	if (clk_ctx.mc_ce_iface_clk)
		clk_disable_unprepare(clk_ctx.mc_ce_iface_clk);

	if (clk_ctx.mc_ce_core_clk)
		clk_disable_unprepare(clk_ctx.mc_ce_core_clk);

	if (clk_ctx.mc_ce_bus_clk)
		clk_disable_unprepare(clk_ctx.mc_ce_bus_clk);
}

#endif /* MC_CRYPTO_CLOCK_MANAGEMENT */
