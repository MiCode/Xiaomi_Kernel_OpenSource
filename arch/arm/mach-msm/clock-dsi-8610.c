/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/iopoll.h>
#include <linux/clk.h>

#include <asm/processor.h>
#include <mach/msm_iomap.h>
#include <mach/clk-provider.h>

#include "clock-dsi-8610.h"

#define DSI_PHY_PHYS		0xFDD00000
#define DSI_PHY_SIZE		0x00100000

#define DSI_CTRL		0x0000
#define DSI_DSIPHY_PLL_CTRL_0	0x0200
#define DSI_DSIPHY_PLL_CTRL_1	0x0204
#define DSI_DSIPHY_PLL_CTRL_2	0x0208
#define DSI_DSIPHY_PLL_CTRL_3	0x020C
#define DSI_DSIPHY_PLL_RDY	0x0280
#define DSI_DSIPHY_PLL_CTRL_8	0x0220
#define DSI_DSIPHY_PLL_CTRL_9	0x0224
#define DSI_DSIPHY_PLL_CTRL_10	0x0228

#define DSI_BPP			3
#define DSI_PLL_RDY_BIT		0x01
#define DSI_PLL_RDY_LOOP_COUNT	80000
#define DSI_MAX_DIVIDER		256

static unsigned char *dsi_base;
static struct clk *dsi_ahb_clk;

int __init dsi_clk_ctrl_init(struct clk *ahb_clk)
{
	dsi_base = ioremap(DSI_PHY_PHYS, DSI_PHY_SIZE);
	if (!dsi_base) {
		pr_err("unable to remap dsi base\n");
		return -ENODEV;
	}

	dsi_ahb_clk = ahb_clk;
	return 0;
}

static int dsi_pll_vco_enable(struct clk *c)
{
	u32 status;
	int i = 0, ret = 0;

	ret = clk_enable(dsi_ahb_clk);
	if (ret) {
		pr_err("fail to enable dsi ahb clk\n");
		return ret;
	}

	writel_relaxed(0x01, dsi_base + DSI_DSIPHY_PLL_CTRL_0);

	do {
		status = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_RDY);
	} while (!(status & DSI_PLL_RDY_BIT) && (i++ < DSI_PLL_RDY_LOOP_COUNT));

	if (!(status & DSI_PLL_RDY_BIT)) {
		pr_err("DSI PLL not ready, polling time out!\n");
		ret = -ETIMEDOUT;
	}

	clk_disable(dsi_ahb_clk);

	return ret;
}

static void dsi_pll_vco_disable(struct clk *c)
{
	int ret;

	ret = clk_enable(dsi_ahb_clk);
	if (ret) {
		pr_err("fail to enable dsi ahb clk\n");
		return;
	}

	writel_relaxed(0x00, dsi_base + DSI_DSIPHY_PLL_CTRL_0);
	clk_disable(dsi_ahb_clk);
}

static int dsi_pll_vco_set_rate(struct clk *c, unsigned long rate)
{
	int ret;
	u32 temp, val;
	unsigned long fb_divider;
	struct clk *parent = c->parent;
	struct dsi_pll_vco_clk *vco_clk =
				container_of(c, struct dsi_pll_vco_clk, c);

	if (!rate)
		return 0;

	ret = clk_prepare_enable(dsi_ahb_clk);
	if (ret) {
		pr_err("fail to enable dsi ahb clk\n");
		return ret;
	}

	temp = rate / 10;
	val = parent->rate / 10;
	fb_divider = (temp * vco_clk->pref_div_ratio) / val;
	fb_divider = fb_divider / 2 - 1;

	temp =  readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_1);
	val = (temp & 0xFFFFFF00) | (fb_divider & 0xFF);
	writel_relaxed(val, dsi_base + DSI_DSIPHY_PLL_CTRL_1);

	temp =  readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_2);
	val = (temp & 0xFFFFFFF8) | ((fb_divider >> 8) & 0x07);
	writel_relaxed(val, dsi_base + DSI_DSIPHY_PLL_CTRL_2);

	temp =  readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_3);
	val = (temp & 0xFFFFFFC0) | (vco_clk->pref_div_ratio - 1);
	writel_relaxed(val, dsi_base + DSI_DSIPHY_PLL_CTRL_3);

	clk_disable_unprepare(dsi_ahb_clk);

	return 0;
}

/* rate is the bit clk rate */
static long dsi_pll_vco_round_rate(struct clk *c, unsigned long rate)
{
	long vco_rate;
	struct dsi_pll_vco_clk *vco_clk =
		container_of(c, struct dsi_pll_vco_clk, c);


	vco_rate = rate;
	if (rate < vco_clk->vco_clk_min)
		vco_rate = vco_clk->vco_clk_min;
	else if (rate > vco_clk->vco_clk_max)
		vco_rate = vco_clk->vco_clk_max;

	return vco_rate;
}

static unsigned long dsi_pll_vco_get_rate(struct clk *c)
{
	u32 fb_divider, ref_divider, vco_rate;
	u32 temp, status;
	struct clk *parent = c->parent;

	status = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_RDY);
	if (status & DSI_PLL_RDY_BIT) {
		fb_divider = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_1);
		fb_divider &= 0xFF;
		temp = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_2) & 0x07;
		fb_divider = (temp << 8) | fb_divider;
		fb_divider += 1;

		ref_divider = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_3);
		ref_divider &= 0x3F;
		ref_divider += 1;

		vco_rate = (parent->rate / ref_divider) * fb_divider;
	} else {
		vco_rate = 0;
	}
	return vco_rate;
}

static enum handoff dsi_pll_vco_handoff(struct clk *c)
{
	u32 status;

	if (clk_prepare_enable(dsi_ahb_clk)) {
		pr_err("fail to enable dsi ahb clk\n");
		return HANDOFF_DISABLED_CLK;
	}

	status = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_0);
	if (!status & DSI_PLL_RDY_BIT) {
		pr_err("DSI PLL not ready\n");
		clk_disable(dsi_ahb_clk);
		return HANDOFF_DISABLED_CLK;
	}

	c->rate = dsi_pll_vco_get_rate(c);

	clk_disable_unprepare(dsi_ahb_clk);

	return HANDOFF_ENABLED_CLK;
}

static int dsi_byteclk_set_rate(struct clk *c, unsigned long rate)
{
	int div, ret;
	long vco_rate;
	unsigned long bitclk_rate;
	u32 temp, val;
	struct clk *parent = clk_get_parent(c);

	if (rate == 0) {
		ret = clk_set_rate(parent, 0);
		return ret;
	}

	bitclk_rate = rate * 8;
	for (div = 1; div < DSI_MAX_DIVIDER; div++) {
		vco_rate = clk_round_rate(parent, bitclk_rate * div);

		if (vco_rate == bitclk_rate * div)
			break;

		if (vco_rate < bitclk_rate * div)
			return -EINVAL;
	}

	if (vco_rate != bitclk_rate * div)
		return -EINVAL;

	ret = clk_set_rate(parent, vco_rate);
	if (ret) {
		pr_err("fail to set vco rate\n");
		return ret;
	}

	ret = clk_prepare_enable(dsi_ahb_clk);
	if (ret) {
		pr_err("fail to enable dsi ahb clk\n");
		return ret;
	}

	/* set the bit clk divider */
	temp =  readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_8);
	val = (temp & 0xFFFFFFF0) | (div - 1);
	writel_relaxed(val, dsi_base + DSI_DSIPHY_PLL_CTRL_8);

	/* set the byte clk divider */
	temp = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_9);
	val = (temp & 0xFFFFFF00) | (vco_rate / rate - 1);
	writel_relaxed(val, dsi_base + DSI_DSIPHY_PLL_CTRL_9);
	clk_disable_unprepare(dsi_ahb_clk);

	return 0;
}

static long dsi_byteclk_round_rate(struct clk *c, unsigned long rate)
{
	int div;
	long vco_rate;
	unsigned long bitclk_rate;
	struct clk *parent = clk_get_parent(c);

	if (rate == 0)
		return -EINVAL;

	bitclk_rate = rate * 8;
	for (div = 1; div < DSI_MAX_DIVIDER; div++) {
		vco_rate = clk_round_rate(parent, bitclk_rate * div);
		if (vco_rate == bitclk_rate * div)
			break;
		if (vco_rate < bitclk_rate * div)
			return -EINVAL;
	}

	if (vco_rate != bitclk_rate * div)
		return -EINVAL;

	return rate;
}

static enum handoff dsi_byteclk_handoff(struct clk *c)
{
	struct clk *parent = clk_get_parent(c);
	unsigned long vco_rate = clk_get_rate(parent);
	u32 out_div2;

	if (vco_rate == 0)
		return HANDOFF_DISABLED_CLK;

	if (clk_prepare_enable(dsi_ahb_clk)) {
		pr_err("fail to enable dsi ahb clk\n");
		return HANDOFF_DISABLED_CLK;
	}

	out_div2 = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_9);
	out_div2 &= 0xFF;
	c->rate = vco_rate / (out_div2 + 1);
	clk_disable_unprepare(dsi_ahb_clk);

	return HANDOFF_ENABLED_CLK;
}

static int dsi_dsiclk_set_rate(struct clk *c, unsigned long rate)
{
	u32 temp, val;
	int ret;
	struct clk *parent = clk_get_parent(c);
	unsigned long vco_rate = clk_get_rate(parent);

	if (rate == 0)
		return 0;

	if (vco_rate % rate != 0) {
		pr_err("dsiclk_set_rate invalid rate\n");
		return -EINVAL;
	}

	ret = clk_prepare_enable(dsi_ahb_clk);
	if (ret) {
		pr_err("fail to enable dsi ahb clk\n");
		return ret;
	}
	temp =	readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_10);
	val = (temp & 0xFFFFFF00) | (vco_rate / rate - 1);
	writel_relaxed(val, dsi_base + DSI_DSIPHY_PLL_CTRL_10);
	clk_disable_unprepare(dsi_ahb_clk);

	return 0;
}

static long dsi_dsiclk_round_rate(struct clk *c, unsigned long rate)
{
	/* rate is the pixel clk rate, translate into dsi clk rate*/
	struct clk *parent = clk_get_parent(c);
	unsigned long vco_rate = clk_get_rate(parent);

	rate *= DSI_BPP;

	if (vco_rate < rate)
		return -EINVAL;

	if (vco_rate % rate != 0)
		return -EINVAL;

	return rate;
}

static enum handoff dsi_dsiclk_handoff(struct clk *c)
{
	struct clk *parent = clk_get_parent(c);
	unsigned long vco_rate = clk_get_rate(parent);
	u32 out_div3;

	if (vco_rate == 0)
		return HANDOFF_DISABLED_CLK;

	if (clk_prepare_enable(dsi_ahb_clk)) {
		pr_err("fail to enable dsi ahb clk\n");
		return HANDOFF_DISABLED_CLK;
	}

	out_div3 = readl_relaxed(dsi_base + DSI_DSIPHY_PLL_CTRL_10);
	out_div3 &= 0xFF;
	c->rate = vco_rate / (out_div3 + 1);
	clk_disable_unprepare(dsi_ahb_clk);

	return HANDOFF_ENABLED_CLK;
}

int dsi_prepare(struct clk *clk)
{
	return clk_prepare(dsi_ahb_clk);
}

void dsi_unprepare(struct clk *clk)
{
	clk_unprepare(dsi_ahb_clk);
}

struct clk_ops clk_ops_dsi_dsiclk = {
	.prepare = dsi_prepare,
	.unprepare = dsi_unprepare,
	.set_rate = dsi_dsiclk_set_rate,
	.round_rate = dsi_dsiclk_round_rate,
	.handoff = dsi_dsiclk_handoff,
};

struct clk_ops clk_ops_dsi_byteclk = {
	.prepare = dsi_prepare,
	.unprepare = dsi_unprepare,
	.set_rate = dsi_byteclk_set_rate,
	.round_rate = dsi_byteclk_round_rate,
	.handoff = dsi_byteclk_handoff,
};

struct clk_ops clk_ops_dsi_vco = {
	.prepare = dsi_prepare,
	.unprepare = dsi_unprepare,
	.enable = dsi_pll_vco_enable,
	.disable = dsi_pll_vco_disable,
	.set_rate = dsi_pll_vco_set_rate,
	.round_rate = dsi_pll_vco_round_rate,
	.handoff = dsi_pll_vco_handoff,
};

