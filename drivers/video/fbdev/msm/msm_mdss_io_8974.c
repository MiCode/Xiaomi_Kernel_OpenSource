/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/io.h>

#include <mach/clk.h>
#include <mach/msm_iomap.h>

#include "mdss_dsi.h"

#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)
#define PWRDN_B BIT(7)

static struct dsi_clk_desc dsi_pclk;

static struct clk *dsi_byte_div_clk;
static struct clk *dsi_esc_clk;

int mdss_dsi_clk_on;

int mdss_dsi_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dsi_byte_div_clk = clk_get(dev, "byte_clk");
	if (IS_ERR(dsi_byte_div_clk)) {
		pr_err("can't find dsi_byte_div_clk\n");
		dsi_byte_div_clk = NULL;
		goto mdss_dsi_clk_err;
	}

	dsi_esc_clk = clk_get(dev, "core_clk");
	if (IS_ERR(dsi_esc_clk)) {
		printk(KERN_ERR "can't find dsi_esc_clk\n");
		dsi_esc_clk = NULL;
		goto mdss_dsi_clk_err;
	}

	return 0;

mdss_dsi_clk_err:
	mdss_dsi_clk_deinit(dev);
	return -EPERM;
}

void mdss_dsi_clk_deinit(struct device *dev)
{
	if (dsi_byte_div_clk)
		clk_put(dsi_byte_div_clk);
	if (dsi_esc_clk)
		clk_put(dsi_esc_clk);
}

#define PREF_DIV_RATIO 27
struct dsiphy_pll_divider_config pll_divider_config;

int mdss_dsi_clk_div_config(u8 bpp, u8 lanes,
			    u32 *expected_dsi_pclk)
{
	u32 fb_divider, rate, vco;
	u32 div_ratio = 0;
	u32 pll_analog_posDiv = 1;
	struct dsi_clk_mnd_table const *mnd_entry = mnd_table;
	if (pll_divider_config.clk_rate == 0)
		pll_divider_config.clk_rate = 454000000;

	rate = (pll_divider_config.clk_rate / 2)
			 / 1000000; /* Half Bit Clock In Mhz */

	if (rate < 43) {
		vco = rate * 16;
		div_ratio = 16;
		pll_analog_posDiv = 8;
	} else if (rate < 85) {
		vco = rate * 8;
		div_ratio = 8;
		pll_analog_posDiv = 4;
	} else if (rate < 170) {
		vco = rate * 4;
		div_ratio = 4;
		pll_analog_posDiv = 2;
	} else if (rate < 340) {
		vco = rate * 2;
		div_ratio = 2;
		pll_analog_posDiv = 1;
	} else {
		/* DSI PLL Direct path configuration */
		vco = rate * 1;
		div_ratio = 1;
		pll_analog_posDiv = 1;
	}

	/* find the mnd settings from mnd_table entry */
	for (; mnd_entry != mnd_table + ARRAY_SIZE(mnd_table); ++mnd_entry) {
		if (((mnd_entry->lanes) == lanes) &&
			((mnd_entry->bpp) == bpp))
			break;
	}

	if (mnd_entry == mnd_table + ARRAY_SIZE(mnd_table)) {
		pr_err("%s: requested Lanes, %u & BPP, %u, not supported\n",
			__func__, lanes, bpp);
		return -EINVAL;
	}
	fb_divider = ((vco * PREF_DIV_RATIO) / 27);
	pll_divider_config.fb_divider = fb_divider;
	pll_divider_config.ref_divider_ratio = PREF_DIV_RATIO;
	pll_divider_config.bit_clk_divider = div_ratio;
	pll_divider_config.byte_clk_divider =
			pll_divider_config.bit_clk_divider * 8;
	pll_divider_config.analog_posDiv = pll_analog_posDiv;
	pll_divider_config.digital_posDiv =
			(mnd_entry->pll_digital_posDiv) * div_ratio;

	if ((mnd_entry->pclk_d == 0)
		|| (mnd_entry->pclk_m == 1)) {
		dsi_pclk.mnd_mode = 0;
		dsi_pclk.src = 0x3;
		dsi_pclk.pre_div_func = (mnd_entry->pclk_n - 1);
	} else {
		dsi_pclk.mnd_mode = 2;
		dsi_pclk.src = 0x3;
		dsi_pclk.m = mnd_entry->pclk_m;
		dsi_pclk.n = mnd_entry->pclk_n;
		dsi_pclk.d = mnd_entry->pclk_d;
	}
	*expected_dsi_pclk = (((pll_divider_config.clk_rate) * lanes)
				      / (8 * bpp));

	return 0;
}

void cont_splash_clk_ctrl(int enable)
{
	static int cont_splash_clks_enabled;
	if (enable && !cont_splash_clks_enabled) {
			clk_prepare_enable(dsi_byte_div_clk);
			clk_prepare_enable(dsi_esc_clk);
			cont_splash_clks_enabled = 1;
	} else if (!enable && cont_splash_clks_enabled) {
			clk_disable_unprepare(dsi_byte_div_clk);
			clk_disable_unprepare(dsi_esc_clk);
			cont_splash_clks_enabled = 0;
	}
}

void mdss_dsi_prepare_clocks(void)
{
	clk_prepare(dsi_byte_div_clk);
	clk_prepare(dsi_esc_clk);
}

void mdss_dsi_unprepare_clocks(void)
{
	clk_unprepare(dsi_esc_clk);
	clk_unprepare(dsi_byte_div_clk);
}

void mdss_dsi_clk_enable(void)
{
	if (mdss_dsi_clk_on) {
		pr_info("%s: mdss_dsi_clks already ON\n", __func__);
		return;
	}

	if (clk_set_rate(dsi_byte_div_clk, 1) < 0)	/* divided by 1 */
		pr_err("%s: dsi_byte_div_clk - clk_set_rate failed\n",
					__func__);
	if (clk_set_rate(dsi_esc_clk, 2) < 0) /* divided by 2 */
		pr_err("%s: dsi_esc_clk - clk_set_rate failed\n",
					__func__);
	clk_enable(dsi_byte_div_clk);
	clk_enable(dsi_esc_clk);
	mdss_dsi_clk_on = 1;
}

void mdss_dsi_clk_disable(void)
{
	if (mdss_dsi_clk_on == 0) {
		pr_info("%s: mdss_dsi_clks already OFF\n", __func__);
		return;
	}
	clk_disable(dsi_esc_clk);
	clk_disable(dsi_byte_div_clk);
	mdss_dsi_clk_on = 0;
}
