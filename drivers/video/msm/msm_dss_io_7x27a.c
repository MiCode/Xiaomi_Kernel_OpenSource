/* Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
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
#include "msm_fb.h"
#include "mipi_dsi.h"

/* multimedia sub system sfpb */
char *mmss_sfpb_base;
void  __iomem *periph_base;

static struct dsi_clk_desc dsicore_clk;
static struct dsi_clk_desc dsi_pclk;

static struct clk *dsi_byte_div_clk;
static struct clk *dsi_esc_clk;
static struct clk *dsi_pixel_clk;
static struct clk *dsi_clk;
static struct clk *dsi_ref_clk;
static struct clk *mdp_dsi_pclk;
static struct clk *ahb_m_clk;
static struct clk *ahb_s_clk;
static struct clk *ebi1_dsi_clk;
int mipi_dsi_clk_on;

int mipi_dsi_clk_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	dsi_esc_clk = clk_get(dev, "esc_clk");
	if (IS_ERR_OR_NULL(dsi_esc_clk)) {
		printk(KERN_ERR "can't find dsi_esc_clk\n");
		dsi_esc_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_byte_div_clk = clk_get(dev, "byte_clk");
	if (IS_ERR_OR_NULL(dsi_byte_div_clk)) {
		pr_err("can't find dsi_byte_div_clk\n");
		dsi_byte_div_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_pixel_clk = clk_get(dev, "pixel_clk");
	if (IS_ERR_OR_NULL(dsi_pixel_clk)) {
		pr_err("can't find dsi_pixel_clk\n");
		dsi_pixel_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_clk = clk_get(dev, "core_clk");
	if (IS_ERR_OR_NULL(dsi_clk)) {
		pr_err("can't find dsi_clk\n");
		dsi_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	dsi_ref_clk = clk_get(dev, "ref_clk");
	if (IS_ERR_OR_NULL(dsi_ref_clk)) {
		pr_err("can't find dsi_ref_clk\n");
		dsi_ref_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	mdp_dsi_pclk = clk_get(dev, "mdp_clk");
	if (IS_ERR_OR_NULL(mdp_dsi_pclk)) {
		pr_err("can't find mdp_dsi_pclk\n");
		mdp_dsi_pclk = NULL;
		goto mipi_dsi_clk_err;
	}

	ahb_m_clk = clk_get(dev, "master_iface_clk");
	if (IS_ERR_OR_NULL(ahb_m_clk)) {
		pr_err("can't find ahb_m_clk\n");
		ahb_m_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	ahb_s_clk = clk_get(dev, "slave_iface_clk");
	if (IS_ERR_OR_NULL(ahb_s_clk)) {
		pr_err("can't find ahb_s_clk\n");
		ahb_s_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	ebi1_dsi_clk = clk_get(dev, "mem_clk");
	if (IS_ERR_OR_NULL(ebi1_dsi_clk)) {
		pr_err("can't find ebi1_dsi_clk\n");
		ebi1_dsi_clk = NULL;
		goto mipi_dsi_clk_err;
	}

	return 0;

mipi_dsi_clk_err:
	mipi_dsi_clk_deinit(NULL);
	return -EPERM;
}

void mipi_dsi_clk_deinit(struct device *dev)
{
	if (mdp_dsi_pclk)
		clk_put(mdp_dsi_pclk);
	if (ahb_m_clk)
		clk_put(ahb_m_clk);
	if (ahb_s_clk)
		clk_put(ahb_s_clk);
	if (dsi_ref_clk)
		clk_put(dsi_ref_clk);
	if (dsi_byte_div_clk)
		clk_put(dsi_byte_div_clk);
	if (dsi_esc_clk)
		clk_put(dsi_esc_clk);
	if (ebi1_dsi_clk)
		clk_put(ebi1_dsi_clk);
}

static void mipi_dsi_clk_ctrl(struct dsi_clk_desc *clk, int clk_en)
{
	uint32 data;
	if (clk_en) {
		data = (clk->pre_div_func) << 24 |
			(clk->m) << 16 | (clk->n) << 8 |
			((clk->d) * 2);
		clk_set_rate(dsi_clk, data);
		clk_enable(dsi_clk);
	} else
		clk_disable(dsi_clk);
}

static void mipi_dsi_pclk_ctrl(struct dsi_clk_desc *clk, int clk_en)
{
	uint32 data;

	if (clk_en) {
		data = (clk->pre_div_func) << 24 | (clk->m) << 16
			| (clk->n) << 8 | ((clk->d) * 2);
		if ((clk_set_rate(dsi_pixel_clk, data)) < 0)
			pr_err("%s: pixel clk set rate failed\n", __func__);
		if (clk_enable(dsi_pixel_clk))
			pr_err("%s clk enable failed\n", __func__);
	} else {
		clk_disable(dsi_pixel_clk);
	}
}

static void mipi_dsi_calibration(void)
{
	MIPI_OUTP(MIPI_DSI_BASE + 0xf8, 0x00a105a1); /* cal_hw_ctrl */
}

#define PREF_DIV_RATIO 19
struct dsiphy_pll_divider_config pll_divider_config;

int mipi_dsi_clk_div_config(uint8 bpp, uint8 lanes,
			    uint32 *expected_dsi_pclk)
{
	u32 fb_divider, rate, vco;
	u32 div_ratio = 0;
	struct dsi_clk_mnd_table const *mnd_entry = mnd_table;
	if (pll_divider_config.clk_rate == 0)
		pll_divider_config.clk_rate = 454000000;

	rate = pll_divider_config.clk_rate / 1000000; /* In Mhz */

	if (rate < 125) {
		vco = rate * 8;
		div_ratio = 8;
	} else if (rate < 250) {
		vco = rate * 4;
		div_ratio = 4;
	} else if (rate < 500) {
		vco = rate * 2;
		div_ratio = 2;
	} else {
		vco = rate * 1;
		div_ratio = 1;
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
	pll_divider_config.dsi_clk_divider =
			(mnd_entry->dsiclk_div) * div_ratio;

	if ((mnd_entry->dsiclk_d == 0)
		|| (mnd_entry->dsiclk_m == 1)) {
		dsicore_clk.mnd_mode = 0;
		dsicore_clk.src = 0x3;
		dsicore_clk.pre_div_func = (mnd_entry->dsiclk_n - 1);
	} else {
		dsicore_clk.mnd_mode = 2;
		dsicore_clk.src = 0x3;
		dsicore_clk.m = mnd_entry->dsiclk_m;
		dsicore_clk.n = mnd_entry->dsiclk_n;
		dsicore_clk.d = mnd_entry->dsiclk_d;
	}

	if ((mnd_entry->pclk_d == 0)
		|| (mnd_entry->pclk_m == 1)) {
		dsi_pclk.mnd_mode = 0;
		dsi_pclk.src = 0x3;
		dsi_pclk.pre_div_func = (mnd_entry->pclk_n - 1);
		*expected_dsi_pclk = ((vco * 1000000) /
					((pll_divider_config.dsi_clk_divider)
					* (mnd_entry->pclk_n)));
	} else {
		dsi_pclk.mnd_mode = 2;
		dsi_pclk.src = 0x3;
		dsi_pclk.m = mnd_entry->pclk_m;
		dsi_pclk.n = mnd_entry->pclk_n;
		dsi_pclk.d = mnd_entry->pclk_d;
		*expected_dsi_pclk = ((vco * 1000000 * dsi_pclk.m) /
					((pll_divider_config.dsi_clk_divider)
					* (mnd_entry->pclk_n)));
	}
	dsicore_clk.m = 1;
	dsicore_clk.n = 1;
	dsicore_clk.d = 2;
	dsicore_clk.pre_div_func = 0;

	dsi_pclk.m = 1;
	dsi_pclk.n = 3;
	dsi_pclk.d = 2;
	dsi_pclk.pre_div_func = 0;
	return 0;
}

void mipi_dsi_phy_init(int panel_ndx, struct msm_panel_info const *panel_info,
	int target_type)
{
	struct mipi_dsi_phy_ctrl *pd;
	int i, off;

	MIPI_OUTP(MIPI_DSI_BASE + 0x128, 0x0001);/* start phy sw reset */
	wmb();
	usleep(1000);
	MIPI_OUTP(MIPI_DSI_BASE + 0x128, 0x0000);/* end phy w reset */
	wmb();
	usleep(1000);
	MIPI_OUTP(MIPI_DSI_BASE + 0x2cc, 0x0003);/* regulator_ctrl_0 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x2d0, 0x0001);/* regulator_ctrl_1 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x2d4, 0x0001);/* regulator_ctrl_2 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x2d8, 0x0000);/* regulator_ctrl_3 */
#ifdef DSI_POWER
	MIPI_OUTP(MIPI_DSI_BASE + 0x2dc, 0x0100);/* regulator_ctrl_4 */
#endif

	pd = (panel_info->mipi).dsi_phy_db;

	off = 0x02cc;	/* regulator ctrl 0 */
	for (i = 0; i < 4; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->regulator[i]);
		wmb();
		off += 4;
	}

	off = 0x0260;	/* phy timig ctrl 0 */
	for (i = 0; i < 11; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->timing[i]);
		wmb();
		off += 4;
	}

	off = 0x0290;	/* ctrl 0 */
	for (i = 0; i < 4; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->ctrl[i]);
		wmb();
		off += 4;
	}

	off = 0x02a0;	/* strength 0 */
	for (i = 0; i < 4; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->strength[i]);
		wmb();
		off += 4;
	}

	mipi_dsi_calibration();

	off = 0x0204;	/* pll ctrl 1, skip 0 */
	for (i = 1; i < 21; i++) {
		MIPI_OUTP(MIPI_DSI_BASE + off, pd->pll[i]);
		wmb();
		off += 4;
	}

	MIPI_OUTP(MIPI_DSI_BASE + 0x100, 0x67);

	/* pll ctrl 0 */
	MIPI_OUTP(MIPI_DSI_BASE + 0x0200, pd->pll[0]);
	wmb();
}

void cont_splash_clk_ctrl(int enable)
{
	static int cont_splash_clks_enabled;
	if (enable && !cont_splash_clks_enabled) {
		clk_prepare_enable(dsi_ref_clk);
		clk_prepare_enable(mdp_dsi_pclk);
		clk_prepare_enable(dsi_byte_div_clk);
		clk_prepare_enable(dsi_esc_clk);
		clk_prepare_enable(dsi_pixel_clk);
		clk_prepare_enable(dsi_clk);
		cont_splash_clks_enabled = 1;
	} else if (!enable && cont_splash_clks_enabled) {
		clk_disable_unprepare(dsi_clk);
		clk_disable_unprepare(dsi_pixel_clk);
		clk_disable_unprepare(dsi_esc_clk);
		clk_disable_unprepare(dsi_byte_div_clk);
		clk_disable_unprepare(mdp_dsi_pclk);
		clk_disable_unprepare(dsi_ref_clk);
		cont_splash_clks_enabled = 0;
	}
}

void mipi_dsi_prepare_clocks(void)
{
	clk_prepare(dsi_ref_clk);
	clk_prepare(ahb_m_clk);
	clk_prepare(ahb_s_clk);
	clk_prepare(ebi1_dsi_clk);
	clk_prepare(mdp_dsi_pclk);
	clk_prepare(dsi_byte_div_clk);
	clk_prepare(dsi_esc_clk);
	clk_prepare(dsi_clk);
	clk_prepare(dsi_pixel_clk);
}

void mipi_dsi_unprepare_clocks(void)
{
	clk_unprepare(dsi_esc_clk);
	clk_unprepare(dsi_byte_div_clk);
	clk_unprepare(mdp_dsi_pclk);
	clk_unprepare(ebi1_dsi_clk);
	clk_unprepare(ahb_m_clk);
	clk_unprepare(ahb_s_clk);
	clk_unprepare(dsi_ref_clk);
	clk_unprepare(dsi_clk);
	clk_unprepare(dsi_pixel_clk);
}

void mipi_dsi_ahb_ctrl(u32 enable)
{
	static int ahb_ctrl_done;
	if (enable) {
		if (ahb_ctrl_done) {
			pr_info("%s: ahb clks already ON\n", __func__);
			return;
		}
		clk_enable(dsi_ref_clk);
		clk_enable(ahb_m_clk);
		clk_enable(ahb_s_clk);
		ahb_ctrl_done = 1;
	} else {
		if (ahb_ctrl_done == 0) {
			pr_info("%s: ahb clks already OFF\n", __func__);
			return;
		}
		clk_disable(ahb_m_clk);
		clk_disable(ahb_s_clk);
		clk_disable(dsi_ref_clk);
		ahb_ctrl_done = 0;
	}
}

void mipi_dsi_clk_enable(void)
{
	unsigned data = 0;
	uint32 pll_ctrl;

	if (mipi_dsi_clk_on) {
		pr_info("%s: mipi_dsi_clks already ON\n", __func__);
		return;
	}
	if (clk_set_rate(ebi1_dsi_clk, 65000000)) /* 65 MHz */
		pr_err("%s: ebi1_dsi_clk set rate failed\n", __func__);
	clk_enable(ebi1_dsi_clk);

	pll_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x0200);
	MIPI_OUTP(MIPI_DSI_BASE + 0x0200, pll_ctrl | 0x01);
	mb();

	clk_set_rate(dsi_byte_div_clk, data);
	clk_set_rate(dsi_esc_clk, data);
	clk_enable(mdp_dsi_pclk);
	clk_enable(dsi_byte_div_clk);
	clk_enable(dsi_esc_clk);
	mipi_dsi_pclk_ctrl(&dsi_pclk, 1);
	mipi_dsi_clk_ctrl(&dsicore_clk, 1);
	mipi_dsi_clk_on = 1;
}

void mipi_dsi_clk_disable(void)
{
	if (mipi_dsi_clk_on == 0) {
		pr_info("%s: mipi_dsi_clks already OFF\n", __func__);
		return;
	}
	mipi_dsi_pclk_ctrl(&dsi_pclk, 0);
	mipi_dsi_clk_ctrl(&dsicore_clk, 0);
	clk_disable(dsi_esc_clk);
	clk_disable(dsi_byte_div_clk);
	clk_disable(mdp_dsi_pclk);
	/* DSIPHY_PLL_CTRL_0, disable dsi pll */
	MIPI_OUTP(MIPI_DSI_BASE + 0x0200, 0x40);
	if (clk_set_rate(ebi1_dsi_clk, 0))
		pr_err("%s: ebi1_dsi_clk set rate failed\n", __func__);
	clk_disable(ebi1_dsi_clk);
	mipi_dsi_clk_on = 0;
}

void mipi_dsi_phy_ctrl(int on)
{
	if (on) {
		/* DSIPHY_PLL_CTRL_5 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0214, 0x050);

		/* DSIPHY_TPA_CTRL_1 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0258, 0x00f);

		/* DSIPHY_TPA_CTRL_2 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x025c, 0x000);
	} else {
		/* DSIPHY_PLL_CTRL_5 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0214, 0x05f);

		/* DSIPHY_TPA_CTRL_1 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0258, 0x08f);

		/* DSIPHY_TPA_CTRL_2 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x025c, 0x001);

		/* DSIPHY_REGULATOR_CTRL_0 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x02cc, 0x02);

		/* DSIPHY_CTRL_0 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0290, 0x00);

		/* DSIPHY_CTRL_1 */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0294, 0x7f);

		/* disable dsi clk */
		MIPI_OUTP(MIPI_DSI_BASE + 0x0118, 0);
	}
}

#ifdef CONFIG_FB_MSM_MDP303
void update_lane_config(struct msm_panel_info *pinfo)
{
	struct mipi_dsi_phy_ctrl *pd;

	pd = (pinfo->mipi).dsi_phy_db;
	pinfo->mipi.data_lane1 = FALSE;
	pd->pll[10] |= 0x08;

	pinfo->yres = 320;
	pinfo->lcdc.h_back_porch = 15;
	pinfo->lcdc.h_front_porch = 21;
	pinfo->lcdc.h_pulse_width = 5;
	pinfo->lcdc.v_back_porch = 50;
	pinfo->lcdc.v_front_porch = 101;
	pinfo->lcdc.v_pulse_width = 50;
}
#endif
