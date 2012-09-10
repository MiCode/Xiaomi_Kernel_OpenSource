/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>

#include <mach/clk.h>
#include <mach/msm_iomap.h>

#include "mdss_dsi.h"

#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)
#define PWRDN_B BIT(7)

static struct dsi_clk_desc dsi_pclk;

static struct clk *dsi_byte_clk;
static struct clk *dsi_esc_clk;
static struct clk *dsi_pixel_clk;

int mdss_dsi_clk_on;

int mdss_dsi_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dsi_byte_clk = clk_get(dev, "byte_clk");
	if (IS_ERR(dsi_byte_clk)) {
		pr_err("can't find dsi_byte_clk\n");
		dsi_byte_clk = NULL;
		goto mdss_dsi_clk_err;
	}

	dsi_pixel_clk = clk_get(dev, "pixel_clk");
	if (IS_ERR(dsi_pixel_clk)) {
		pr_err("can't find dsi_pixel_clk\n");
		dsi_pixel_clk = NULL;
		goto mdss_dsi_clk_err;
	}

	dsi_esc_clk = clk_get(dev, "core_clk");
	if (IS_ERR(dsi_esc_clk)) {
		pr_err("can't find dsi_esc_clk\n");
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
	if (dsi_byte_clk)
		clk_put(dsi_byte_clk);
	if (dsi_esc_clk)
		clk_put(dsi_esc_clk);
	if (dsi_pixel_clk)
		clk_put(dsi_pixel_clk);
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

void mdss_dsi_prepare_clocks(void)
{
	clk_prepare(dsi_byte_clk);
	clk_prepare(dsi_esc_clk);
	clk_prepare(dsi_pixel_clk);
}

void mdss_dsi_unprepare_clocks(void)
{
	clk_unprepare(dsi_esc_clk);
	clk_unprepare(dsi_pixel_clk);
	clk_unprepare(dsi_byte_clk);
}

void mdss_dsi_clk_enable(struct mdss_panel_data *pdata)
{
	if (mdss_dsi_clk_on) {
		pr_info("%s: mdss_dsi_clks already ON\n", __func__);
		return;
	}

	if (clk_set_rate(dsi_esc_clk, 19200000) < 0)
		pr_err("%s: dsi_esc_clk - clk_set_rate failed\n",
					__func__);

	if (clk_set_rate(dsi_byte_clk, 53000000) < 0)
		pr_err("%s: dsi_byte_clk - clk_set_rate failed\n",
					__func__);

	if (clk_set_rate(dsi_pixel_clk, 70000000) < 0)
		pr_err("%s: dsi_pixel_clk - clk_set_rate failed\n",
					__func__);

	clk_enable(dsi_esc_clk);
	clk_enable(dsi_byte_clk);
	clk_enable(dsi_pixel_clk);

	mdss_dsi_clk_on = 1;
}

void mdss_dsi_clk_disable(struct mdss_panel_data *pdata)
{
	if (mdss_dsi_clk_on == 0) {
		pr_info("%s: mdss_dsi_clks already OFF\n", __func__);
		return;
	}

	clk_disable(dsi_pixel_clk);
	clk_disable(dsi_byte_clk);
	clk_disable(dsi_esc_clk);

	mdss_dsi_clk_on = 0;
}

void mdss_dsi_phy_sw_reset(unsigned char *ctrl_base)
{
	/* start phy sw reset */
	MIPI_OUTP(ctrl_base + 0x12c, 0x0001);
	wmb();
	usleep(1);
	/* end phy sw reset */
	MIPI_OUTP(ctrl_base + 0x12c, 0x0000);
	wmb();
	usleep(1);
}

void mdss_dsi_phy_enable(unsigned char *ctrl_base, int on)
{
	if (on) {
		MIPI_OUTP(ctrl_base + 0x0220, 0x006);
		usleep(10);
		MIPI_OUTP(ctrl_base + 0x0268, 0x001);
		usleep(10);
		MIPI_OUTP(ctrl_base + 0x0268, 0x000);
		usleep(10);
		MIPI_OUTP(ctrl_base + 0x0220, 0x007);
		wmb();

		/* MMSS_DSI_0_PHY_DSIPHY_CTRL_0 */
		MIPI_OUTP(ctrl_base + 0x0470, 0x07e);
		MIPI_OUTP(ctrl_base + 0x0470, 0x06e);
		MIPI_OUTP(ctrl_base + 0x0470, 0x06c);
		MIPI_OUTP(ctrl_base + 0x0470, 0x064);
		MIPI_OUTP(ctrl_base + 0x0470, 0x065);
		MIPI_OUTP(ctrl_base + 0x0470, 0x075);
		MIPI_OUTP(ctrl_base + 0x0470, 0x077);
		MIPI_OUTP(ctrl_base + 0x0470, 0x07f);
		wmb();

	} else {
		MIPI_OUTP(ctrl_base + 0x0220, 0x006);
		usleep(10);
		MIPI_OUTP(ctrl_base + 0x0470, 0x000);
		wmb();
	}
}

void mdss_dsi_phy_init(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_phy_ctrl *pd;
	int i, off, ln, offset;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	pd = ((ctrl_pdata->panel_data).panel_info.mipi).dsi_phy_db;

	off = 0x0580;	/* phy regulator ctrl settings */
	for (i = 0; i < 8; i++) {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + off, pd->regulator[i]);
		wmb();
		off += 4;
	}

	off = 0x0440;	/* phy timing ctrl 0 - 11 */
	for (i = 0; i < 12; i++) {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + off, pd->timing[i]);
		wmb();
		off += 4;
	}

	/* Strength ctrl 0 - 1 */
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0484, pd->strength[0]);
	MIPI_OUTP((ctrl_pdata->ctrl_base) + 0x0488, pd->strength[1]);
	wmb();

	off = 0x04b4;	/* phy BIST ctrl 0 - 5 */
	for (i = 0; i < 6; i++) {
		MIPI_OUTP((ctrl_pdata->ctrl_base) + off, pd->bistCtrl[i]);
		wmb();
		off += 4;
	}

	/* 4 lanes + clk lane configuration */
	/* lane config n * (0 - 4) & DataPath setup */
	for (ln = 0; ln < 5; ln++) {
		off = 0x0300 + (ln * 0x40);
		for (i = 0; i < 9; i++) {
			offset = i + (ln * 9);
			MIPI_OUTP((ctrl_pdata->ctrl_base) + off,
							pd->laneCfg[offset]);
			wmb();
			off += 4;
		}
	}
}
