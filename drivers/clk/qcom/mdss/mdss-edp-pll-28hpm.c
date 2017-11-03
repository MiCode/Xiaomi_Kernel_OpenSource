/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>

#include <dt-bindings/clock/msm-clocks-8974.h>

#include "mdss-pll.h"
#include "mdss-edp-pll.h"

#define EDP_PHY_PLL_UNIPHY_PLL_REFCLK_CFG	(0x0)
#define EDP_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG	(0x0004)
#define EDP_PHY_PLL_UNIPHY_PLL_VCOLPF_CFG	(0x000C)
#define EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG		(0x0020)
#define EDP_PHY_PLL_UNIPHY_PLL_POSTDIV2_CFG	(0x0024)
#define EDP_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG	(0x0028)
#define EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG0		(0x0038)
#define EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG1		(0x003C)
#define EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG2		(0x0040)
#define EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG3		(0x0044)
#define EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG4		(0x0048)
#define EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG0		(0x004C)
#define EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG1		(0x0050)
#define EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG2		(0x0054)
#define EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG3		(0x0058)
#define EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG2	(0x0064)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG0		(0x006C)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG2		(0x0074)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG6		(0x0084)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG7		(0x0088)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG8		(0x008C)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG9		(0x0090)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG10	(0x0094)
#define EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG11	(0x0098)
#define EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG0	(0x005C)
#define EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG1	(0x0060)

#define EDP_PLL_POLL_DELAY_US			50
#define EDP_PLL_POLL_TIMEOUT_US			500

static const struct clk_ops edp_mainlink_clk_src_ops;
static struct clk_div_ops fixed_5div_ops; /* null ops */
static const struct clk_ops edp_pixel_clk_ops;

static inline struct edp_pll_vco_clk *to_edp_vco_clk(struct clk *clk)
{
	return container_of(clk, struct edp_pll_vco_clk, c);
}

int edp_div_prepare(struct clk *c)
{
	struct div_clk *div = to_div_clk(c);
	/* Restore the divider's value */
	return div->ops->set_div(div, div->data.div);
}

static int edp_vco_set_rate(struct clk *c, unsigned long vco_rate)
{
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);
	struct mdss_pll_resources *edp_pll_res = vco->priv;
	int rc;

	pr_debug("vco_rate=%d\n", (int)vco_rate);

	rc = mdss_pll_resource_enable(edp_pll_res, true);
	if (rc) {
		pr_err("failed to enable edp pll res rc=%d\n", rc);
		rc =  -EINVAL;
	}

	if (vco_rate == 810000000) {
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_VCOLPF_CFG, 0x18);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0d);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_REFCLK_CFG, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG0, 0x36);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG1, 0x69);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG2, 0xff);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG3, 0x2f);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG0, 0x80);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG1, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG2, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG3, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG0, 0x12);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG6, 0x5a);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG7, 0x0);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG9, 0x0);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG10, 0x2a);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG11, 0x3);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG1, 0x1a);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG, 0x00);
	} else if (vco_rate == 1350000000) {
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0d);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_REFCLK_CFG, 0x01);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG0, 0x36);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG1, 0x62);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG2, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG3, 0x28);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SDM_CFG4, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG0, 0x80);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG1, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG2, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_SSC_CFG3, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG0, 0x12);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG2, 0x01);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG6, 0x5a);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG7, 0x0);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG8, 0x60);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG9, 0x0);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG10, 0x46);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_CAL_CFG11, 0x5);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG0, 0x10);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_LKDET_CFG1, 0x1a);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG, 0x00);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG, 0x00);
	} else {
		pr_err("rate=%d is NOT supported\n", (int)vco_rate);
		vco_rate = 0;
		rc =  -EINVAL;
	}

	MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
	udelay(100);
	MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
	udelay(100);
	MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x07);
	udelay(100);
	MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
	udelay(100);
	mdss_pll_resource_enable(edp_pll_res, false);

	vco->rate = vco_rate;

	return rc;
}

static int edp_pll_ready_poll(struct mdss_pll_resources *edp_pll_res)
{
	int cnt;
	u32 status;

	cnt = 100;
	while (cnt--) {
		udelay(100);
		status = MDSS_PLL_REG_R(edp_pll_res->pll_base, 0xc0);
		status &= 0x01;
		if (status)
			break;
	}
	pr_debug("cnt=%d status=%d\n", cnt, (int)status);

	if (status)
		return 1;

	return 0;
}

static int edp_vco_enable(struct clk *c)
{
	int i, ready;
	int rc;
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);
	struct mdss_pll_resources *edp_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(edp_pll_res, true);
	if (rc) {
		pr_err("edp pll resources not available\n");
		return rc;
	}

	for (i = 0; i < 3; i++) {
		ready = edp_pll_ready_poll(edp_pll_res);
		if (ready)
			break;
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
		udelay(100);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
		udelay(100);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x07);
		udelay(100);
		MDSS_PLL_REG_W(edp_pll_res->pll_base,
					EDP_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
		udelay(100);
	}

	if (ready) {
		pr_debug("EDP PLL lock success\n");
		edp_pll_res->pll_on = true;
		rc = 0;
	} else {
		pr_err("EDP PLL failed to lock\n");
		mdss_pll_resource_enable(edp_pll_res, false);
		rc = -EINVAL;
	}

	return rc;
}

static void edp_vco_disable(struct clk *c)
{
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);
	struct mdss_pll_resources *edp_pll_res = vco->priv;

	if (!edp_pll_res) {
		pr_err("Invalid input parameter\n");
		return;
	}

	if (!edp_pll_res->pll_on &&
		mdss_pll_resource_enable(edp_pll_res, true)) {
		pr_err("edp pll resources not available\n");
		return;
	}

	MDSS_PLL_REG_W(edp_pll_res->pll_base, 0x20, 0x00);

	edp_pll_res->handoff_resources = false;
	edp_pll_res->pll_on = false;

	mdss_pll_resource_enable(edp_pll_res, false);

	pr_debug("EDP PLL Disabled\n");
}

static unsigned long edp_vco_get_rate(struct clk *c)
{
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);
	struct mdss_pll_resources *edp_pll_res = vco->priv;
	u32 pll_status, div2;
	int rc;

	if (is_gdsc_disabled(edp_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(edp_pll_res, true);
	if (rc) {
		pr_err("edp pll resources not available\n");
		return rc;
	}

	if (vco->rate == 0) {
		pll_status = MDSS_PLL_REG_R(edp_pll_res->pll_base, 0xc0);
		if (pll_status & 0x01) {
			div2 = MDSS_PLL_REG_R(edp_pll_res->pll_base, 0x24);
			if (div2 & 0x01)
				vco->rate = 1350000000;
			else
				vco->rate = 810000000;
		}
	}
	mdss_pll_resource_enable(edp_pll_res, false);

	pr_debug("rate=%d\n", (int)vco->rate);

	return vco->rate;
}

static long edp_vco_round_rate(struct clk *c, unsigned long rate)
{
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);
	unsigned long rrate = -ENOENT;
	unsigned long *lp;

	lp = vco->rate_list;
	while (*lp) {
		rrate = *lp;
		if (rate <= rrate)
			break;
		lp++;
	}

	pr_debug("rrate=%d\n", (int)rrate);

	return rrate;
}

static int edp_vco_prepare(struct clk *c)
{
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);

	pr_debug("rate=%d\n", (int)vco->rate);

	return edp_vco_set_rate(c, vco->rate);
}

static void edp_vco_unprepare(struct clk *c)
{
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);

	pr_debug("rate=%d\n", (int)vco->rate);

	edp_vco_disable(c);
}

static int edp_pll_lock_status(struct mdss_pll_resources *edp_pll_res)
{
	u32 status;
	int pll_locked = 0;
	int rc;

	rc = mdss_pll_resource_enable(edp_pll_res, true);
	if (rc) {
		pr_err("edp pll resources not available\n");
		return rc;
	}

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic((edp_pll_res->pll_base + 0xc0),
			status, ((status & BIT(0)) == 1),
			EDP_PLL_POLL_DELAY_US,
			EDP_PLL_POLL_TIMEOUT_US)) {
		pr_debug("EDP PLL status=%x failed to Lock\n", status);
		pll_locked = 0;
	} else {
		pll_locked = 1;
	}
	mdss_pll_resource_enable(edp_pll_res, false);

	return pll_locked;
}

static enum handoff edp_vco_handoff(struct clk *c)
{
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct edp_pll_vco_clk *vco = to_edp_vco_clk(c);
	struct mdss_pll_resources *edp_pll_res = vco->priv;

	if (is_gdsc_disabled(edp_pll_res))
		return HANDOFF_DISABLED_CLK;

	if (mdss_pll_resource_enable(edp_pll_res, true)) {
		pr_err("edp pll resources not available\n");
		return ret;
	}

	edp_pll_res->handoff_resources = true;

	if (edp_pll_lock_status(edp_pll_res)) {
		c->rate = edp_vco_get_rate(c);
		edp_pll_res->pll_on = true;
		ret = HANDOFF_ENABLED_CLK;
	} else {
		edp_pll_res->handoff_resources = false;
		mdss_pll_resource_enable(edp_pll_res, false);
	}

	pr_debug("done, ret=%d\n", ret);
	return ret;
}

static unsigned long edp_vco_rate_list[] = {
		810000000, 1350000000, 0};

struct const clk_ops edp_vco_clk_ops = {
	.enable = edp_vco_enable,
	.set_rate = edp_vco_set_rate,
	.get_rate = edp_vco_get_rate,
	.round_rate = edp_vco_round_rate,
	.prepare = edp_vco_prepare,
	.unprepare = edp_vco_unprepare,
	.handoff = edp_vco_handoff,
};

struct edp_pll_vco_clk edp_vco_clk = {
	.ref_clk_rate = 19200000,
	.rate = 0,
	.rate_list = edp_vco_rate_list,
	.c = {
		.dbg_name = "edp_vco_clk",
		.ops = &edp_vco_clk_ops,
		CLK_INIT(edp_vco_clk.c),
	},
};

static unsigned long edp_mainlink_get_rate(struct clk *c)
{
	struct div_clk *mclk = to_div_clk(c);
	struct clk *pclk;
	unsigned long rate = 0;

	pclk = clk_get_parent(c);

	if (pclk && pclk->ops->get_rate) {
		rate = pclk->ops->get_rate(pclk);
		rate /= mclk->data.div;
	}

	pr_debug("rate=%d div=%d\n", (int)rate, mclk->data.div);

	return rate;
}


struct div_clk edp_mainlink_clk_src = {
	.ops = &fixed_5div_ops,
	.data = {
		.div = 5,
		.min_div = 5,
		.max_div = 5,
	},
	.c = {
		.parent = &edp_vco_clk.c,
		.dbg_name = "edp_mainlink_clk_src",
		.ops = &edp_mainlink_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(edp_mainlink_clk_src.c),
	}
};

/*
 * this rate is from pll to clock controller
 * output from pll to CC has two possibilities
 * 1: if mainlink rate is 270M, then 675M
 * 2: if mainlink rate is 162M, then 810M
 */
static int edp_pixel_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *edp_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(edp_pll_res, true);
	if (rc) {
		pr_err("edp pll resources not available\n");
		return rc;
	}

	pr_debug("div=%d\n", div);
	MDSS_PLL_REG_W(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_POSTDIV2_CFG, (div - 1));
	mdss_pll_resource_enable(edp_pll_res, false);

	return 0;
}

static int edp_pixel_get_div(struct div_clk *clk)
{
	int div = 0;
	int rc;
	struct mdss_pll_resources *edp_pll_res = clk->priv;

	if (is_gdsc_disabled(edp_pll_res))
		return 0;

	rc = mdss_pll_resource_enable(edp_pll_res, true);
	if (rc) {
		pr_err("edp pll resources not available\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(edp_pll_res->pll_base,
				EDP_PHY_PLL_UNIPHY_PLL_POSTDIV2_CFG);
	mdss_pll_resource_enable(edp_pll_res, false);
	div &= 0x01;
	pr_debug("div=%d\n", div);
	return div + 1;
}

static struct clk_div_ops edp_pixel_ops = {
	.set_div = edp_pixel_set_div,
	.get_div = edp_pixel_get_div,
};

struct div_clk edp_pixel_clk_src = {
	.data = {
		.max_div = 2,
		.min_div = 1,
	},
	.ops = &edp_pixel_ops,
	.c = {
		.parent = &edp_vco_clk.c,
		.dbg_name = "edp_pixel_clk_src",
		.ops = &edp_pixel_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(edp_pixel_clk_src.c),
	},
};

static struct clk_lookup mdss_edp_pllcc_8974[] = {
	CLK_LOOKUP("edp_pixel_src", edp_pixel_clk_src.c,
						"fd8c0000.qcom,mmsscc-mdss"),
	CLK_LOOKUP("edp_mainlink_src", edp_mainlink_clk_src.c,
						"fd8c0000.qcom,mmsscc-mdss"),
};

int edp_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP;

	if (!pll_res || !pll_res->pll_base) {
		pr_err("Invalid input parameters\n");
		return -EPROBE_DEFER;
	}

	/* Set client data to div and vco clocks */
	edp_pixel_clk_src.priv = pll_res;
	edp_mainlink_clk_src.priv = pll_res;
	edp_vco_clk.priv = pll_res;

	/* Set clock operation for mainlink and pixel clock */
	edp_mainlink_clk_src_ops = clk_ops_div;
	edp_mainlink_clk_src_ops.get_parent = clk_get_parent;
	edp_mainlink_clk_src_ops.get_rate = edp_mainlink_get_rate;

	edp_pixel_clk_ops = clk_ops_slave_div;
	edp_pixel_clk_ops.prepare = edp_div_prepare;

	rc = of_msm_clock_register(pdev->dev.of_node, mdss_edp_pllcc_8974,
					 ARRAY_SIZE(mdss_edp_pllcc_8974));
	if (rc) {
		pr_err("Clock register failed rc=%d\n", rc);
		rc = -EPROBE_DEFER;
	}

	return rc;
}
