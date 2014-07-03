/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/clk/msm-clk.h>

#include "mdss_dsi.h"
#include "mdss_edp.h"

#define SW_RESET BIT(2)
#define SW_RESET_PLL BIT(0)
#define PWRDN_B BIT(7)

static struct dsi_clk_desc dsi_pclk;

void mdss_dsi_phy_sw_reset(unsigned char *ctrl_base)
{
	/* start phy sw reset */
	MIPI_OUTP(ctrl_base + 0x12c, 0x0001);
	udelay(1000);
	wmb();
	/* end phy sw reset */
	MIPI_OUTP(ctrl_base + 0x12c, 0x0000);
	udelay(100);
	wmb();
}

void mdss_dsi_phy_disable(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_dsi_ctrl_pdata *ctrl0 = NULL;

	if (ctrl == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	/*
	 * In dual-dsi configuration, the phy should be disabled for the
	 * first controller only when the second controller is disabled.
	 * This is true regardless of whether broadcast mode is enabled
	 * or not.
	 */
	if ((ctrl->ndx == DSI_CTRL_0) &&
		mdss_dsi_get_ctrl_by_index(DSI_CTRL_1)) {
		pr_debug("%s: Dual dsi detected. skipping config for ctrl%d\n",
			__func__, ctrl->ndx);
		return;
	}

	if (ctrl->ndx == DSI_CTRL_1) {
		ctrl0 = mdss_dsi_get_ctrl_by_index(DSI_CTRL_0);
		if (ctrl0) {
			MIPI_OUTP(ctrl0->phy_io.base + 0x0170, 0x000);
			MIPI_OUTP(ctrl0->phy_io.base + 0x0298, 0x000);
		} else {
			pr_warn("%s: Unable to get control%d\n",
				__func__, DSI_CTRL_0);
		}
	}

	MIPI_OUTP(ctrl->phy_io.base + 0x0170, 0x000);
	MIPI_OUTP(ctrl->phy_io.base + 0x0298, 0x000);

	/*
	 * Wait for the registers writes to complete in order to
	 * ensure that the phy is completely disabled
	 */
	wmb();
}

static void mdss_dsi_phy_init(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_phy_ctrl *pd;
	int i, off, ln, offset;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL, *temp_ctrl = NULL;
	u32 ctrl_rev;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}
	temp_ctrl = ctrl_pdata;

	pd = &(((ctrl_pdata->panel_data).panel_info.mipi).dsi_phy_db);

	/* Strength ctrl 0 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0184, pd->strength[0]);

	/*
	 * Phy regulator ctrl settings.
	 * In dual dsi configuration, the second controller also uses
	 * the regulators of the first controller, irrespective of whether
	 * broadcast mode is enabled or not.
	 */
	if (ctrl_pdata->ndx == DSI_CTRL_1) {
		temp_ctrl = mdss_dsi_get_ctrl_by_index(DSI_CTRL_0);
		if (!temp_ctrl) {
			pr_err("%s: Unable to get master ctrl\n", __func__);
			return;
		}
	}

	/* Regulator ctrl 0 */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x280, 0x0);
	/* Regulator ctrl - CAL_PWR_CFG */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x298, pd->regulator[6]);

	/* Regulator ctrl - TEST */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x294, pd->regulator[5]);
	/* Regulator ctrl 3 */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x28c, pd->regulator[3]);
	/* Regulator ctrl 2 */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x288, pd->regulator[2]);
	/* Regulator ctrl 1 */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x284, pd->regulator[1]);
	/* Regulator ctrl 0 */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x280, pd->regulator[0]);
	/* Regulator ctrl 4 */
	MIPI_OUTP((temp_ctrl->phy_io.base) + 0x290, pd->regulator[4]);

	/* LDO ctrl */
	if (pd->reg_ldo_mode)
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x1dc, 0x25);
	else
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x1dc, 0x00);

	off = 0x0140;	/* phy timing ctrl 0 - 11 */
	for (i = 0; i < 12; i++) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + off, pd->timing[i]);
		wmb();
		off += 4;
	}

	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_1 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0174, 0x00);
	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_0 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0170, 0x5f);
	wmb();

	/* Strength ctrl 1 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0188, pd->strength[1]);
	wmb();

	/* 4 lanes + clk lane configuration */
	/* lane config n * (0 - 4) & DataPath setup */
	for (ln = 0; ln < 5; ln++) {
		off = (ln * 0x40);
		for (i = 0; i < 9; i++) {
			offset = i + (ln * 9);
			MIPI_OUTP((ctrl_pdata->phy_io.base) + off,
							pd->lanecfg[offset]);
			wmb();
			off += 4;
		}
	}

	/* MMSS_DSI_0_PHY_DSIPHY_CTRL_0 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0170, 0x5f);
	wmb();

	ctrl_rev = MIPI_INP(ctrl_pdata->ctrl_base);

	/* DSI_0_PHY_DSIPHY_GLBL_TEST_CTRL */
	if (((ctrl_pdata->panel_data).panel_info.pdest == DISPLAY_1) ||
			(ctrl_rev == MDSS_DSI_HW_REV_103_1))
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x01d4, 0x01);
	else
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x01d4, 0x00);
	wmb();

	off = 0x01b4;	/* phy BIST ctrl 0 - 5 */
	for (i = 0; i < 6; i++) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + off, pd->bistctrl[i]);
		wmb();
		off += 4;
	}

}

static void mdss_dsi_20nm_phy_init(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_phy_ctrl *pd;
	int i, off, ln, offset;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL, *temp_ctrl = NULL;

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}
	temp_ctrl = ctrl_pdata;
	pd = &(((ctrl_pdata->panel_data).panel_info.mipi).dsi_phy_db);

	/* Strength ctrl 0 */
	MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0184, pd->strength[0]);

	/*
	 * Phy regulator ctrl settings.
	 * In dual dsi configuration, the second controller also uses
	 * the regulators of the first controller, irrespective of whether
	 * broadcast mode is enabled or not.
	 */
	if (ctrl_pdata->ndx == DSI_CTRL_1) {
		temp_ctrl = mdss_dsi_get_ctrl_by_index(DSI_CTRL_0);
		if (!temp_ctrl) {
			pr_err("%s: Unable to get master ctrl\n", __func__);
			return;
		}
	}

	if (pd->reg_ldo_mode) {
		/* Regulator ctrl 0 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x280, 0x0);
		/* Regulator ctrl - CAL_PWR_CFG */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x298, pd->regulator[6]);
		udelay(1000);
		/* Regulator ctrl - TEST */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x294, pd->regulator[5]);
		/* Regulator ctrl 3 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x28c, pd->regulator[3]);
		/* Regulator ctrl 2 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x288, pd->regulator[2]);
		/* Regulator ctrl 1 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x284, pd->regulator[1]);
		/* Regulator ctrl 4 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x290, pd->regulator[4]);
		/* LDO ctrl */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x1dc, 0x1d);
	} else {
		/* Regulator ctrl 0 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x280, 0x0);
		/* Regulator ctrl - CAL_PWR_CFG */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x298, pd->regulator[6]);
		udelay(1000);
		/* Regulator ctrl 1 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x284, pd->regulator[1]);
		/* Regulator ctrl 2 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x288, pd->regulator[2]);
		/* Regulator ctrl 3 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x28c, pd->regulator[3]);
		/* Regulator ctrl 4 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x290, pd->regulator[4]);
		/* LDO ctrl */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x1dc, 0x00);
		/* Regulator ctrl 0 */
		MIPI_OUTP((temp_ctrl->phy_io.base) + 0x280, pd->regulator[0]);
	}

	off = 0x0140;	/* phy timing ctrl 0 - 11 */
	for (i = 0; i < 12; i++) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + off, pd->timing[i]);
		wmb();
		off += 4;
	}

	/* Currently the Phy settings for the DSI 0 is done in clk prepare*/
	if (ctrl_pdata->ndx == DSI_CTRL_1) {
		/* MMSS_DSI_0_PHY_DSIPHY_CTRL_1 */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0174, 0x00);
		/* MMSS_DSI_0_PHY_DSIPHY_CTRL_0 */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0170, 0x5f);
		wmb();

		/* Strength ctrl 1 */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0188, pd->strength[1]);
		wmb();

		/* MMSS_DSI_0_PHY_DSIPHY_CTRL_0 */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0170, 0x7f);
		wmb();

		/* DSI_0_PHY_DSIPHY_GLBL_TEST_CTRL */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x01d4, 0x00);

		/* MMSS_DSI_0_PHY_DSIPHY_CTRL_2 */
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0178, 0x00);
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0178, 0x02);
		MIPI_OUTP((ctrl_pdata->phy_io.base) + 0x0178, 0x03);
		wmb();
	}

	/* 4 lanes + clk lane configuration */
	/* lane config n * (0 - 4) & DataPath setup */
	for (ln = 0; ln < 5; ln++) {
		off = (ln * 0x40);
		for (i = 0; i < 9; i++) {
			offset = i + (ln * 9);
			MIPI_OUTP((ctrl_pdata->phy_io.base) + off,
							pd->lanecfg[offset]);
			wmb();
			off += 4;
		}
	}

	off = 0x01b4;	/* phy BIST ctrl 0 - 5 */
	for (i = 0; i < 6; i++) {
		MIPI_OUTP((ctrl_pdata->phy_io.base) + off, pd->bistctrl[i]);
		wmb();
		off += 4;
	}

}

int mdss_dsi_clk_init(struct platform_device *pdev,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct device *dev = NULL;
	int rc = 0;

	if (!pdev) {
		pr_err("%s: Invalid pdev\n", __func__);
		goto mdss_dsi_clk_err;
	}

	dev = &pdev->dev;
	ctrl->mdp_core_clk = clk_get(dev, "mdp_core_clk");
	if (IS_ERR(ctrl->mdp_core_clk)) {
		rc = PTR_ERR(ctrl->mdp_core_clk);
		pr_err("%s: Unable to get mdp core clk. rc=%d\n",
			__func__, rc);
		goto mdss_dsi_clk_err;
	}

	ctrl->ahb_clk = clk_get(dev, "iface_clk");
	if (IS_ERR(ctrl->ahb_clk)) {
		rc = PTR_ERR(ctrl->ahb_clk);
		pr_err("%s: Unable to get mdss ahb clk. rc=%d\n",
			__func__, rc);
		goto mdss_dsi_clk_err;
	}

	ctrl->axi_clk = clk_get(dev, "bus_clk");
	if (IS_ERR(ctrl->axi_clk)) {
		rc = PTR_ERR(ctrl->axi_clk);
		pr_err("%s: Unable to get axi bus clk. rc=%d\n",
			__func__, rc);
		goto mdss_dsi_clk_err;
	}

	if ((ctrl->panel_data.panel_info.type == MIPI_CMD_PANEL) ||
		ctrl->panel_data.panel_info.mipi.dynamic_switch_enabled) {
		ctrl->mmss_misc_ahb_clk = clk_get(dev, "core_mmss_clk");
		if (IS_ERR(ctrl->mmss_misc_ahb_clk)) {
			ctrl->mmss_misc_ahb_clk = NULL;
			pr_info("%s: Unable to get mmss misc ahb clk\n",
				__func__);
		}
	}

	ctrl->byte_clk = clk_get(dev, "byte_clk");
	if (IS_ERR(ctrl->byte_clk)) {
		rc = PTR_ERR(ctrl->byte_clk);
		pr_err("%s: can't find dsi_byte_clk. rc=%d\n",
			__func__, rc);
		ctrl->byte_clk = NULL;
		goto mdss_dsi_clk_err;
	}

	ctrl->pixel_clk = clk_get(dev, "pixel_clk");
	if (IS_ERR(ctrl->pixel_clk)) {
		rc = PTR_ERR(ctrl->pixel_clk);
		pr_err("%s: can't find dsi_pixel_clk. rc=%d\n",
			__func__, rc);
		ctrl->pixel_clk = NULL;
		goto mdss_dsi_clk_err;
	}

	ctrl->esc_clk = clk_get(dev, "core_clk");
	if (IS_ERR(ctrl->esc_clk)) {
		rc = PTR_ERR(ctrl->esc_clk);
		pr_err("%s: can't find dsi_esc_clk. rc=%d\n",
			__func__, rc);
		ctrl->esc_clk = NULL;
		goto mdss_dsi_clk_err;
	}

mdss_dsi_clk_err:
	if (rc)
		mdss_dsi_clk_deinit(ctrl);
	return rc;
}

void mdss_dsi_clk_deinit(struct mdss_dsi_ctrl_pdata  *ctrl)
{
	if (ctrl->byte_clk)
		clk_put(ctrl->byte_clk);
	if (ctrl->esc_clk)
		clk_put(ctrl->esc_clk);
	if (ctrl->pixel_clk)
		clk_put(ctrl->pixel_clk);
	if (ctrl->mmss_misc_ahb_clk)
		clk_put(ctrl->mmss_misc_ahb_clk);
	if (ctrl->axi_clk)
		clk_put(ctrl->axi_clk);
	if (ctrl->ahb_clk)
		clk_put(ctrl->ahb_clk);
	if (ctrl->mdp_core_clk)
		clk_put(ctrl->mdp_core_clk);
}

#define PREF_DIV_RATIO 27
struct dsiphy_pll_divider_config pll_divider_config;

int mdss_dsi_clk_div_config(struct mdss_panel_info *panel_info,
			    int frame_rate)
{
	u32 fb_divider, rate, vco;
	u32 div_ratio = 0;
	u32 pll_analog_posDiv = 1;
	u32 h_period, v_period;
	u32 dsi_pclk_rate;
	u8 lanes = 0, bpp;
	struct dsi_clk_mnd_table const *mnd_entry = mnd_table;

	if (panel_info->mipi.data_lane3)
		lanes += 1;
	if (panel_info->mipi.data_lane2)
		lanes += 1;
	if (panel_info->mipi.data_lane1)
		lanes += 1;
	if (panel_info->mipi.data_lane0)
		lanes += 1;

	switch (panel_info->mipi.dst_format) {
	case DSI_CMD_DST_FORMAT_RGB888:
	case DSI_VIDEO_DST_FORMAT_RGB888:
	case DSI_VIDEO_DST_FORMAT_RGB666_LOOSE:
		bpp = 3;
		break;
	case DSI_CMD_DST_FORMAT_RGB565:
	case DSI_VIDEO_DST_FORMAT_RGB565:
		bpp = 2;
		break;
	default:
		bpp = 3;	/* Default format set to RGB888 */
		break;
	}

	h_period = mdss_panel_get_htotal(panel_info, true);
	v_period = mdss_panel_get_vtotal(panel_info);

	if ((frame_rate !=
	     panel_info->mipi.frame_rate) ||
	    (!panel_info->clk_rate)) {
		h_period += panel_info->lcdc.xres_pad;
		v_period += panel_info->lcdc.yres_pad;

		if (lanes > 0) {
			panel_info->clk_rate =
			((h_period * v_period *
			  frame_rate * bpp * 8)
			   / lanes);
		} else {
			pr_err("%s: forcing mdss_dsi lanes to 1\n", __func__);
			panel_info->clk_rate =
				(h_period * v_period * frame_rate * bpp * 8);
		}
	}
	pll_divider_config.clk_rate = panel_info->clk_rate;


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
	for (; mnd_entry < mnd_table + ARRAY_SIZE(mnd_table); ++mnd_entry) {
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
	dsi_pclk_rate = (((pll_divider_config.clk_rate) * lanes)
				      / (8 * bpp));

	if ((dsi_pclk_rate < 3300000) || (dsi_pclk_rate > 250000000))
		dsi_pclk_rate = 35000000;
	panel_info->mipi.dsi_pclk_rate = dsi_pclk_rate;

	return 0;
}

static int mdss_dsi_bus_clk_start(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	pr_debug("%s: ndx=%d\n", __func__, ctrl_pdata->ndx);

	rc = clk_prepare_enable(ctrl_pdata->mdp_core_clk);
	if (rc) {
		pr_err("%s: failed to enable mdp_core_clock. rc=%d\n",
							 __func__, rc);
		goto error;
	}

	rc = clk_prepare_enable(ctrl_pdata->ahb_clk);
	if (rc) {
		pr_err("%s: failed to enable ahb clock. rc=%d\n", __func__, rc);
		clk_disable_unprepare(ctrl_pdata->mdp_core_clk);
		goto error;
	}

	rc = clk_prepare_enable(ctrl_pdata->axi_clk);
	if (rc) {
		pr_err("%s: failed to enable ahb clock. rc=%d\n", __func__, rc);
		clk_disable_unprepare(ctrl_pdata->ahb_clk);
		clk_disable_unprepare(ctrl_pdata->mdp_core_clk);
		goto error;
	}

	if (ctrl_pdata->mmss_misc_ahb_clk) {
		rc = clk_prepare_enable(ctrl_pdata->mmss_misc_ahb_clk);
		if (rc) {
			pr_err("%s: failed to enable mmss misc ahb clk.rc=%d\n",
				__func__, rc);
			clk_disable_unprepare(ctrl_pdata->axi_clk);
			clk_disable_unprepare(ctrl_pdata->ahb_clk);
			clk_disable_unprepare(ctrl_pdata->mdp_core_clk);
			goto error;
		}
	}

error:
	return rc;
}

static void mdss_dsi_bus_clk_stop(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (ctrl_pdata->mmss_misc_ahb_clk)
		clk_disable_unprepare(ctrl_pdata->mmss_misc_ahb_clk);
	clk_disable_unprepare(ctrl_pdata->axi_clk);
	clk_disable_unprepare(ctrl_pdata->ahb_clk);
	clk_disable_unprepare(ctrl_pdata->mdp_core_clk);
}

static int mdss_dsi_link_clk_prepare(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	rc = clk_prepare(ctrl_pdata->esc_clk);
	if (rc) {
		pr_err("%s: Failed to prepare dsi esc clk\n", __func__);
		goto esc_clk_err;
	}

	rc = clk_prepare(ctrl_pdata->byte_clk);
	if (rc) {
		pr_err("%s: Failed to prepare dsi byte clk\n", __func__);
		goto byte_clk_err;
	}

	rc = clk_prepare(ctrl_pdata->pixel_clk);
	if (rc) {
		pr_err("%s: Failed to prepare dsi pixel clk\n", __func__);
		goto pixel_clk_err;
	}

	return rc;

pixel_clk_err:
	clk_unprepare(ctrl_pdata->byte_clk);
byte_clk_err:
	clk_unprepare(ctrl_pdata->esc_clk);
esc_clk_err:
	return rc;
}

static void mdss_dsi_link_clk_unprepare(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	clk_unprepare(ctrl_pdata->pixel_clk);
	clk_unprepare(ctrl_pdata->byte_clk);
	clk_unprepare(ctrl_pdata->esc_clk);
}

static int mdss_dsi_link_clk_set_rate(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32 esc_clk_rate = 19200000;
	int rc = 0;

	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	if (!ctrl_pdata->panel_data.panel_info.cont_splash_enabled) {
		pr_debug("%s: Set clk rates: pclk=%d, byteclk=%d escclk=%d\n",
			__func__, ctrl_pdata->pclk_rate,
			ctrl_pdata->byte_clk_rate, esc_clk_rate);
		rc = clk_set_rate(ctrl_pdata->esc_clk, esc_clk_rate);
		if (rc) {
			pr_err("%s: dsi_esc_clk - clk_set_rate failed\n",
				__func__);
			goto error;
		}

		rc =  clk_set_rate(ctrl_pdata->byte_clk,
			ctrl_pdata->byte_clk_rate);
		if (rc) {
			pr_err("%s: dsi_byte_clk - clk_set_rate failed\n",
				__func__);
			goto error;
		}

		rc = clk_set_rate(ctrl_pdata->pixel_clk, ctrl_pdata->pclk_rate);
		if (rc) {
			pr_err("%s: dsi_pixel_clk - clk_set_rate failed\n",
				__func__);
			goto error;
		}
	}

error:
	return rc;
}

static int mdss_dsi_link_clk_enable(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: ndx=%d\n", __func__, ctrl_pdata->ndx);

	rc = clk_enable(ctrl_pdata->esc_clk);
	if (rc) {
		pr_err("%s: Failed to enable dsi esc clk\n", __func__);
		goto esc_clk_err;
	}

	rc = clk_enable(ctrl_pdata->byte_clk);
	if (rc) {
		pr_err("%s: Failed to enable dsi byte clk\n", __func__);
		goto byte_clk_err;
	}

	rc = clk_enable(ctrl_pdata->pixel_clk);
	if (rc) {
		pr_err("%s: Failed to enable dsi pixel clk\n", __func__);
		goto pixel_clk_err;
	}

	return rc;

pixel_clk_err:
	clk_disable(ctrl_pdata->byte_clk);
byte_clk_err:
	clk_disable(ctrl_pdata->esc_clk);
esc_clk_err:
	return rc;
}

static void mdss_dsi_link_clk_disable(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (!ctrl_pdata) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	pr_debug("%s: ndx=%d\n", __func__, ctrl_pdata->ndx);

	clk_disable(ctrl_pdata->esc_clk);
	clk_disable(ctrl_pdata->pixel_clk);
	clk_disable(ctrl_pdata->byte_clk);
}

static int mdss_dsi_link_clk_start(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;

	rc = mdss_dsi_link_clk_set_rate(ctrl);
	if (rc) {
		pr_err("%s: failed to set clk rates. rc=%d\n",
			__func__, rc);
		goto error;
	}

	rc = mdss_dsi_link_clk_prepare(ctrl);
	if (rc) {
		pr_err("%s: failed to prepare clks. rc=%d\n",
			__func__, rc);
		goto error;
	}

	rc = mdss_dsi_link_clk_enable(ctrl);
	if (rc) {
		pr_err("%s: failed to enable clks. rc=%d\n",
			__func__, rc);
		mdss_dsi_link_clk_unprepare(ctrl);
		goto error;
	}

error:
	return rc;
}

static void mdss_dsi_link_clk_stop(struct mdss_dsi_ctrl_pdata *ctrl)
{
	mdss_dsi_link_clk_disable(ctrl);
	mdss_dsi_link_clk_unprepare(ctrl);
}

/**
 * mdss_dsi_ulps_config() - Program DSI lanes to enter/exit ULPS mode
 * @ctrl: pointer to DSI controller structure
 * @enable: 1 to enter ULPS, 0 to exit ULPS
 *
 * This function executes the necessary programming sequence to enter/exit
 * DSI Ultra-Low Power State (ULPS). This function assumes that the link and
 * bus clocks are already on.
 */
static int mdss_dsi_ulps_config(struct mdss_dsi_ctrl_pdata *ctrl,
	int enable)
{
	int ret = 0;
	struct mdss_panel_data *pdata = NULL;
	struct mdss_panel_info *pinfo;
	struct mipi_panel_info *mipi;
	u32 lane_status = 0;
	u32 active_lanes = 0;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pdata = &ctrl->panel_data;
	if (!pdata) {
		pr_err("%s: Invalid panel data\n", __func__);
		return -EINVAL;
	}
	pinfo = &pdata->panel_info;
	mipi = &pinfo->mipi;

	if (!mdss_dsi_ulps_feature_enabled(pdata)) {
		pr_debug("%s: ULPS feature not supported. enable=%d\n",
			__func__, enable);
		return -ENOTSUPP;
	}

	/*
	 * No need to enter ULPS when transitioning from splash screen to
	 * boot animation since it is expected that the clocks would be turned
	 * right back on.
	 */
	if (pinfo->cont_splash_enabled) {
		pr_debug("%s: skip ULPS config with splash screen enabled\n",
			__func__);
		return 0;
	}

	/* clock lane will always be programmed for ulps */
	active_lanes = BIT(4);
	/*
	 * make a note of all active data lanes for which ulps entry/exit
	 * is needed
	 */
	if (mipi->data_lane0)
		active_lanes |= BIT(0);
	if (mipi->data_lane1)
		active_lanes |= BIT(1);
	if (mipi->data_lane2)
		active_lanes |= BIT(2);
	if (mipi->data_lane3)
		active_lanes |= BIT(3);

	pr_debug("%s: configuring ulps (%s) for ctrl%d, active lanes=0x%08x\n",
		__func__, (enable ? "on" : "off"), ctrl->ndx,
		active_lanes);

	if (enable && !ctrl->ulps) {
		/*
		 * ULPS Entry Request.
		 * Wait for a short duration to ensure that the lanes
		 * enter ULP state.
		 */
		MIPI_OUTP(ctrl->ctrl_base + 0x0AC, active_lanes);
		usleep(100);

		/* Check to make sure that all active data lanes are in ULPS */
		lane_status = MIPI_INP(ctrl->ctrl_base + 0xA8);
		if (lane_status & (active_lanes << 8)) {
			pr_err("%s: ULPS entry req failed for ctrl%d. Lane status=0x%08x\n",
				__func__, ctrl->ndx, lane_status);
			ret = -EINVAL;
			goto error;
		}

		ctrl->ulps = true;
	} else if (!enable && ctrl->ulps) {
		/*
		 * ULPS Exit Request
		 * Hardware requirement is to wait for at least 1ms
		 */
		MIPI_OUTP(ctrl->ctrl_base + 0x0AC, active_lanes << 8);
		usleep(1000);
		MIPI_OUTP(ctrl->ctrl_base + 0x0AC, 0x0);

		/*
		 * Wait for a short duration before enabling
		 * data transmission
		 */
		usleep(100);

		lane_status = MIPI_INP(ctrl->ctrl_base + 0xA8);
		ctrl->ulps = false;
	} else {
		pr_debug("%s: No change requested: %s -> %s\n", __func__,
			ctrl->ulps ? "enabled" : "disabled",
			enable ? "enabled" : "disabled");
	}

	pr_debug("%s: DSI lane status = 0x%08x. Ulps %s\n", __func__,
		lane_status, enable ? "enabled" : "disabled");

error:
	return ret;
}

/**
 * mdss_dsi_clamp_ctrl() - Program DSI clamps for supporting power collapse
 * @ctrl: pointer to DSI controller structure
 * @enable: 1 to enable clamps, 0 to disable clamps
 *
 * For idle-screen usecases with command mode panels, MDSS can be power
 * collapsed. However, DSI phy needs to remain on. To avoid any mismatch
 * between the DSI controller state, DSI phy needs to be clamped before
 * power collapsing. This function executes the required programming
 * sequence to configure these DSI clamps. This function should only be called
 * when the DSI link clocks are disabled.
 */
static int mdss_dsi_clamp_ctrl(struct mdss_dsi_ctrl_pdata *ctrl, int enable)
{
	struct mipi_panel_info *mipi = NULL;
	u32 clamp_reg, regval = 0;
	u32 clamp_reg_off, phyrst_reg_off;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (!ctrl->mmss_misc_io.base) {
		pr_err("%s: mmss_misc_io not mapped\nn", __func__);
		return -EINVAL;
	}

	clamp_reg_off = ctrl->ulps_clamp_ctrl_off;
	phyrst_reg_off = ctrl->ulps_phyrst_ctrl_off;
	mipi = &ctrl->panel_data.panel_info.mipi;

	/* clock lane will always be clamped */
	clamp_reg = BIT(9);
	if (ctrl->ulps)
		clamp_reg |= BIT(8);
	/* make a note of all active data lanes which need to be clamped */
	if (mipi->data_lane0) {
		clamp_reg |= BIT(7);
		if (ctrl->ulps)
			clamp_reg |= BIT(6);
	}
	if (mipi->data_lane1) {
		clamp_reg |= BIT(5);
		if (ctrl->ulps)
			clamp_reg |= BIT(4);
	}
	if (mipi->data_lane2) {
		clamp_reg |= BIT(3);
		if (ctrl->ulps)
			clamp_reg |= BIT(2);
	}
	if (mipi->data_lane3) {
		clamp_reg |= BIT(1);
		if (ctrl->ulps)
			clamp_reg |= BIT(0);
	}
	pr_debug("%s: called for ctrl%d, enable=%d, clamp_reg=0x%08x\n",
		__func__, ctrl->ndx, enable, clamp_reg);
	if (enable && !ctrl->mmss_clamp) {
		/* Enable MMSS DSI Clamps */
		if (ctrl->ndx == DSI_CTRL_0) {
			regval = MIPI_INP(ctrl->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | clamp_reg);
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | (clamp_reg | BIT(15)));
		} else if (ctrl->ndx == DSI_CTRL_1) {
			regval = MIPI_INP(ctrl->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | (clamp_reg << 16));
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval | ((clamp_reg << 16) | BIT(31)));
		}

		/*
		 * This register write ensures that DSI PHY will not be
		 * reset when mdss ahb clock reset is asserted while coming
		 * out of power collapse
		 */
		MIPI_OUTP(ctrl->mmss_misc_io.base + phyrst_reg_off, 0x1);
		ctrl->mmss_clamp = true;
	} else if (!enable && ctrl->mmss_clamp) {
		MIPI_OUTP(ctrl->mmss_misc_io.base + phyrst_reg_off, 0x0);
		/* Disable MMSS DSI Clamps */
		if (ctrl->ndx == DSI_CTRL_0) {
			regval = MIPI_INP(ctrl->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval & ~(clamp_reg | BIT(15)));
		} else if (ctrl->ndx == DSI_CTRL_1) {
			regval = MIPI_INP(ctrl->mmss_misc_io.base +
				clamp_reg_off);
			MIPI_OUTP(ctrl->mmss_misc_io.base + clamp_reg_off,
				regval & ~((clamp_reg << 16) | BIT(31)));
		}
		ctrl->mmss_clamp = false;
	} else {
		pr_debug("%s: No change requested: %s -> %s\n", __func__,
			ctrl->mmss_clamp ? "enabled" : "disabled",
			enable ? "enabled" : "disabled");
	}

	return 0;
}

/**
 * mdss_dsi_core_power_ctrl() - Enable/disable DSI core power
 * @ctrl: pointer to DSI controller structure
 * @enable: 1 to enable power, 0 to disable power
 *
 * When all DSI bus clocks are disabled, DSI core power module can be turned
 * off to save any leakage current. This function implements the necessary
 * programming sequence for the same. For command mode panels, the core power
 * can be turned off for idle-screen usecases, where additional programming is
 * needed to clamp DSI phy.
 */
static int mdss_dsi_core_power_ctrl(struct mdss_dsi_ctrl_pdata *ctrl,
	int enable)
{
	int rc = 0;
	struct mdss_panel_data *pdata = NULL;
	u32 ctrl_rev;

	if (!ctrl) {
		pr_err("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	pdata = &ctrl->panel_data;
	if (!pdata) {
		pr_err("%s: Invalid panel data\n", __func__);
		return -EINVAL;
	}

	if (enable) {
		if (!ctrl->core_power) {
			/* enable mdss gdsc */
			pr_debug("%s: Enable MDP FS\n", __func__);
			rc = msm_dss_enable_vreg(
				ctrl->power_data[DSI_CORE_PM].vreg_config,
				ctrl->power_data[DSI_CORE_PM].num_vreg, 1);
			if (rc) {
				pr_err("%s: failed to enable vregs for %s\n",
					__func__,
					__mdss_dsi_pm_name(DSI_CORE_PM));
				goto error;
			}
			ctrl->core_power = true;
		}

		rc = mdss_dsi_bus_clk_start(ctrl);
		if (rc) {
			pr_err("%s: Failed to start bus clocks. rc=%d\n",
				__func__, rc);
			goto error_bus_clk_start;
		}

		/*
		 * Phy software reset should not be done for idle screen power
		 * collapse use-case. Issue a phy software reset only when
		 * unblanking the panel.
		 */
		if (!pdata->panel_info.panel_power_on)
			mdss_dsi_phy_sw_reset(ctrl->ctrl_base);
		ctrl_rev = MIPI_INP(ctrl->ctrl_base);
		if (ctrl_rev == MDSS_DSI_HW_REV_103)
			mdss_dsi_20nm_phy_init(pdata);
		else
			mdss_dsi_phy_init(pdata);

		mdss_dsi_ctrl_setup(pdata);

		if (ctrl->ulps) {
			/*
			 * ULPS Entry Request. This is needed if the lanes were
			 * in ULPS prior to power collapse, since after
			 * power collapse and reset, the DSI controller resets
			 * back to idle state and not ULPS. This ulps entry
			 * request will transition the state of the DSI
			 * controller to ULPS which will match the state of the
			 * DSI phy. This needs to be done prior to disabling
			 * the DSI clamps.
			 */
			rc = mdss_dsi_ulps_config(ctrl, 1);
			if (rc) {
				pr_err("%s: Failed to enter ULPS. rc=%d\n",
					__func__, rc);
				goto error_ulps;
			}
		}

		rc = mdss_dsi_clamp_ctrl(ctrl, 0);
		if (rc) {
			pr_err("%s: Failed to disable dsi clamps. rc=%d\n",
				__func__, rc);
			goto error_ulps;
		}
	} else {
		/* Enable DSI clamps only if entering idle power collapse */
		if (ctrl->panel_data.panel_info.panel_power_on) {
			rc = mdss_dsi_clamp_ctrl(ctrl, 1);
			if (rc)
				pr_err("%s: Failed to enable dsi clamps. rc=%d\n",
					__func__, rc);
		}

		/*
		 * disable bus clocks irrespective of whether dsi phy was
		 * successfully clamped or not
		 */
		mdss_dsi_bus_clk_stop(ctrl);

		/* disable mdss gdsc only if dsi phy was successfully clamped*/
		if (rc) {
			pr_debug("%s: leaving mdss gdsc on\n", __func__);
		} else {
			pr_debug("%s: Disable MDP FS\n", __func__);
			rc = msm_dss_enable_vreg(
				ctrl->power_data[DSI_CORE_PM].vreg_config,
				ctrl->power_data[DSI_CORE_PM].num_vreg, 0);
			if (rc) {
				pr_warn("%s: failed to disable vregs for %s\n",
					__func__,
					__mdss_dsi_pm_name(DSI_CORE_PM));
				rc = 0;
			} else {
				ctrl->core_power = false;
			}
		}
	}
	return rc;

error_ulps:
	mdss_dsi_bus_clk_stop(ctrl);
error_bus_clk_start:
	if (msm_dss_enable_vreg(ctrl->power_data[DSI_CORE_PM].vreg_config,
		ctrl->power_data[DSI_CORE_PM].num_vreg, 0))
		pr_warn("%s: failed to disable vregs for %s\n",
			__func__, __mdss_dsi_pm_name(DSI_CORE_PM));
	else
		ctrl->core_power = false;
error:
	return rc;
}

static int __mdss_dsi_update_clk_cnt(u32 *clk_cnt, int enable)
{
	int changed = 0;

	if (enable) {
		if (*clk_cnt == 0)
			changed++;
		(*clk_cnt)++;
	} else {
		if (*clk_cnt != 0) {
			(*clk_cnt)--;
			if (*clk_cnt == 0)
				changed++;
		} else {
			pr_debug("%s: clk cnt already zero\n", __func__);
		}
	}

	return changed;
}

static int mdss_dsi_clk_ctrl_sub(struct mdss_dsi_ctrl_pdata *ctrl,
	u8 clk_type, int enable)
{
	int rc = 0;
	struct mdss_panel_data *pdata;
	bool core_power_enabled = false;

	if (!ctrl) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	pdata = &ctrl->panel_data;

	pr_debug("%s: ndx=%d clk_type=%08x enable=%d\n", __func__,
		ctrl->ndx, clk_type, enable);

	if (enable) {
		if (clk_type & DSI_BUS_CLKS) {
			rc = mdss_dsi_core_power_ctrl(ctrl, enable);
			if (rc) {
				pr_err("%s: Failed to enable core power. rc=%d\n",
					__func__, rc);
				goto error;
			}
			core_power_enabled = true;
		}
		if (clk_type & DSI_LINK_CLKS) {
			rc = mdss_dsi_link_clk_start(ctrl);
			if (rc) {
				pr_err("%s: Failed to start link clocks. rc=%d\n",
					__func__, rc);
				goto error_link_clk_start;
			}
			/* Disable ULPS, if enabled */
			if (ctrl->ulps) {
				rc = mdss_dsi_ulps_config(ctrl, 0);
				if (rc) {
					pr_err("%s: Failed to exit ulps. rc=%d\n",
						__func__, rc);
					goto error_ulps_exit;
				}
			}

			/*
			 * If we are coming out of idle power collapse, then
			 * reset DSI controller state
			 */
			if (core_power_enabled)
				mdss_dsi_reset(ctrl);
		}
	} else {
		if (clk_type & DSI_LINK_CLKS) {
			/*
			 * If ULPS feature is enabled, enter ULPS first.
			 * No need to enable ULPS when turning off clocks
			 * while blanking the panel.
			 */
			if ((mdss_dsi_ulps_feature_enabled(pdata)) &&
				(pdata->panel_info.panel_power_on))
				mdss_dsi_ulps_config(ctrl, 1);
			mdss_dsi_link_clk_stop(ctrl);
		}
		if (clk_type & DSI_BUS_CLKS) {
			rc = mdss_dsi_core_power_ctrl(ctrl, enable);
			if (rc) {
				pr_err("%s: Failed to disable core power. rc=%d\n",
					__func__, rc);
			}
		}
	}

	return rc;

error_ulps_exit:
	mdss_dsi_link_clk_stop(ctrl);
error_link_clk_start:
	if ((clk_type & DSI_BUS_CLKS) &&
		(mdss_dsi_core_power_ctrl(ctrl, !enable)))
		pr_warn("%s: Failed to disable core power. rc=%d\n",
			__func__, rc);
error:
	return rc;
}

static DEFINE_MUTEX(dsi_clk_lock); /* per system */

bool __mdss_dsi_clk_enabled(struct mdss_dsi_ctrl_pdata *ctrl, u8 clk_type)
{
	bool bus_enabled = true;
	bool link_enabled = true;

	mutex_lock(&dsi_clk_lock);
	if (clk_type & DSI_BUS_CLKS)
		bus_enabled = ctrl->bus_clk_cnt ? true : false;
	if (clk_type & DSI_LINK_CLKS)
		link_enabled = ctrl->link_clk_cnt ? true : false;
	mutex_unlock(&dsi_clk_lock);

	return bus_enabled && link_enabled;
}

int mdss_dsi_clk_ctrl(struct mdss_dsi_ctrl_pdata *ctrl,
	u8 clk_type, int enable)
{
	int rc = 0;
	int link_changed = 0, bus_changed = 0;
	int m_link_changed = 0, m_bus_changed = 0;
	struct mdss_dsi_ctrl_pdata *mctrl = NULL;

	if (!ctrl) {
		pr_err("%s: Invalid arg\n", __func__);
		return -EINVAL;
	}

	/*
	 * In broadcast mode, we need to enable clocks for the
	 * master controller as well when enabling clocks for the
	 * slave controller
	 */
	if (mdss_dsi_is_slave_ctrl(ctrl)) {
		mctrl = mdss_dsi_get_master_ctrl();
		if (!mctrl)
			pr_warn("%s: Unable to get master control\n", __func__);
	}

	pr_debug("%s++: ndx=%d clk_type=%d bus_clk_cnt=%d link_clk_cnt=%d\n",
		__func__, ctrl->ndx, clk_type, ctrl->bus_clk_cnt,
		ctrl->link_clk_cnt);
	pr_debug("%s++: mctrl=%s m_bus_clk_cnt=%d m_link_clk_cnt=%d, enable=%d\n",
		__func__, mctrl ? "yes" : "no", mctrl ? mctrl->bus_clk_cnt : -1,
		mctrl ? mctrl->link_clk_cnt : -1, enable);

	mutex_lock(&dsi_clk_lock);

	if (clk_type & DSI_BUS_CLKS) {
		bus_changed = __mdss_dsi_update_clk_cnt(&ctrl->bus_clk_cnt,
			enable);
		if (bus_changed && mctrl)
			m_bus_changed = __mdss_dsi_update_clk_cnt(
				&mctrl->bus_clk_cnt, enable);
	}

	if (clk_type & DSI_LINK_CLKS) {
		link_changed = __mdss_dsi_update_clk_cnt(&ctrl->link_clk_cnt,
			enable);
		if (link_changed && mctrl)
			m_link_changed = __mdss_dsi_update_clk_cnt(
				&mctrl->link_clk_cnt, enable);
	}

	if (!link_changed && !bus_changed)
		goto no_error; /* clk cnts updated, nothing else needed */

	/*
	 * If updating link clock, need to make sure that the bus
	 * clocks are enabled
	 */
	if (link_changed && (!bus_changed && !ctrl->bus_clk_cnt)) {
		pr_err("%s: Trying to enable link clks w/o enabling bus clks for ctrl%d",
			__func__, mctrl->ndx);
		goto error_mctrl_start;
	}

	if (m_link_changed && (!m_bus_changed && !mctrl->bus_clk_cnt)) {
		pr_err("%s: Trying to enable link clks w/o enabling bus clks for ctrl%d",
			__func__, ctrl->ndx);
		goto error_mctrl_start;
	}

	if (enable && (m_bus_changed || m_link_changed)) {
		rc = mdss_dsi_clk_ctrl_sub(mctrl, clk_type, enable);
		if (rc) {
			pr_err("Failed to start mctrl clocks. rc=%d\n", rc);
			goto error_mctrl_start;
		}
	}

	if (!enable && (m_bus_changed || m_link_changed)) {
		rc = mdss_dsi_clk_ctrl_sub(mctrl, clk_type, enable);
		if (rc) {
			pr_err("Failed to stop mctrl clocks. rc=%d\n", rc);
			goto error_mctrl_stop;
		}
	}
	rc = mdss_dsi_clk_ctrl_sub(ctrl, clk_type, enable);
	if (rc) {
		pr_err("Failed to %s ctrl clocks. rc=%d\n",
			(enable ? "start" : "stop"), rc);
		goto error_ctrl;
	}

	goto no_error;

error_mctrl_stop:
	mdss_dsi_clk_ctrl_sub(ctrl, clk_type, enable ? 0 : 1);
error_ctrl:
	if (enable && (m_bus_changed || m_link_changed))
		mdss_dsi_clk_ctrl_sub(mctrl, clk_type, 0);
error_mctrl_start:
	if (clk_type & DSI_BUS_CLKS) {
		if (mctrl)
			__mdss_dsi_update_clk_cnt(&mctrl->bus_clk_cnt,
				enable ? 0 : 1);
		__mdss_dsi_update_clk_cnt(&ctrl->bus_clk_cnt, enable ? 0 : 1);
	}
	if (clk_type & DSI_LINK_CLKS) {
		if (mctrl)
			__mdss_dsi_update_clk_cnt(&mctrl->link_clk_cnt,
				enable ? 0 : 1);
		__mdss_dsi_update_clk_cnt(&ctrl->link_clk_cnt, enable ? 0 : 1);
	}

no_error:
	mutex_unlock(&dsi_clk_lock);
	pr_debug("%s--: ndx=%d clk_type=%d bus_clk_cnt=%d link_clk_cnt=%d changed=%d\n",
		__func__, ctrl->ndx, clk_type, ctrl->bus_clk_cnt,
		ctrl->link_clk_cnt, link_changed && bus_changed);
	pr_debug("%s--: mctrl=%s m_bus_clk_cnt=%d m_link_clk_cnt=%d, m_changed=%d, enable=%d\n",
		__func__, mctrl ? "yes" : "no", mctrl ? mctrl->bus_clk_cnt : -1,
		mctrl ? mctrl->link_clk_cnt : -1,
		m_link_changed && m_bus_changed, enable);

	return rc;
}

void mdss_edp_clk_deinit(struct mdss_edp_drv_pdata *edp_drv)
{
	if (edp_drv->aux_clk)
		clk_put(edp_drv->aux_clk);
	if (edp_drv->pixel_clk)
		clk_put(edp_drv->pixel_clk);
	if (edp_drv->ahb_clk)
		clk_put(edp_drv->ahb_clk);
	if (edp_drv->link_clk)
		clk_put(edp_drv->link_clk);
	if (edp_drv->mdp_core_clk)
		clk_put(edp_drv->mdp_core_clk);
}

int mdss_edp_clk_init(struct mdss_edp_drv_pdata *edp_drv)
{
	struct device *dev = &(edp_drv->pdev->dev);

	edp_drv->aux_clk = clk_get(dev, "core_clk");
	if (IS_ERR(edp_drv->aux_clk)) {
		pr_err("%s: Can't find aux_clk", __func__);
		edp_drv->aux_clk = NULL;
		goto mdss_edp_clk_err;
	}

	edp_drv->pixel_clk = clk_get(dev, "pixel_clk");
	if (IS_ERR(edp_drv->pixel_clk)) {
		pr_err("%s: Can't find pixel_clk", __func__);
		edp_drv->pixel_clk = NULL;
		goto mdss_edp_clk_err;
	}

	edp_drv->ahb_clk = clk_get(dev, "iface_clk");
	if (IS_ERR(edp_drv->ahb_clk)) {
		pr_err("%s: Can't find ahb_clk", __func__);
		edp_drv->ahb_clk = NULL;
		goto mdss_edp_clk_err;
	}

	edp_drv->link_clk = clk_get(dev, "link_clk");
	if (IS_ERR(edp_drv->link_clk)) {
		pr_err("%s: Can't find link_clk", __func__);
		edp_drv->link_clk = NULL;
		goto mdss_edp_clk_err;
	}

	/* need mdss clock to receive irq */
	edp_drv->mdp_core_clk = clk_get(dev, "mdp_core_clk");
	if (IS_ERR(edp_drv->mdp_core_clk)) {
		pr_err("%s: Can't find mdp_core_clk", __func__);
		edp_drv->mdp_core_clk = NULL;
		goto mdss_edp_clk_err;
	}

	return 0;

mdss_edp_clk_err:
	mdss_edp_clk_deinit(edp_drv);
	return -EPERM;
}

int mdss_edp_aux_clk_enable(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	if (clk_set_rate(edp_drv->aux_clk, 19200000) < 0)
		pr_err("%s: aux_clk - clk_set_rate failed\n",
					__func__);

	ret = clk_enable(edp_drv->aux_clk);
	if (ret) {
		pr_err("%s: Failed to enable aux clk\n", __func__);
		goto c2;
	}

	ret = clk_enable(edp_drv->ahb_clk);
	if (ret) {
		pr_err("%s: Failed to enable ahb clk\n", __func__);
		goto c1;
	}

	/* need mdss clock to receive irq */
	ret = clk_enable(edp_drv->mdp_core_clk);
	if (ret) {
		pr_err("%s: Failed to enable mdp_core_clk\n", __func__);
		goto c0;
	}

	return 0;
c0:
	clk_disable(edp_drv->ahb_clk);
c1:
	clk_disable(edp_drv->aux_clk);
c2:
	return ret;

}

void mdss_edp_aux_clk_disable(struct mdss_edp_drv_pdata *edp_drv)
{
	clk_disable(edp_drv->aux_clk);
	clk_disable(edp_drv->ahb_clk);
	clk_disable(edp_drv->mdp_core_clk);
}

int mdss_edp_clk_enable(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	if (edp_drv->clk_on) {
		pr_info("%s: edp clks are already ON\n", __func__);
		return 0;
	}

	if (clk_set_rate(edp_drv->link_clk, edp_drv->link_rate * 27000000) < 0)
		pr_err("%s: link_clk - clk_set_rate failed\n",
					__func__);

	if (clk_set_rate(edp_drv->aux_clk, edp_drv->aux_rate) < 0)
		pr_err("%s: aux_clk - clk_set_rate failed\n",
					__func__);

	if (clk_set_rate(edp_drv->pixel_clk, edp_drv->pixel_rate) < 0)
		pr_err("%s: pixel_clk - clk_set_rate failed\n",
					__func__);

	ret = clk_enable(edp_drv->aux_clk);
	if (ret) {
		pr_err("%s: Failed to enable aux clk\n", __func__);
		goto c4;
	}
	ret = clk_enable(edp_drv->pixel_clk);
	if (ret) {
		pr_err("%s: Failed to enable pixel clk\n", __func__);
		goto c3;
	}
	ret = clk_enable(edp_drv->ahb_clk);
	if (ret) {
		pr_err("%s: Failed to enable ahb clk\n", __func__);
		goto c2;
	}
	ret = clk_enable(edp_drv->link_clk);
	if (ret) {
		pr_err("%s: Failed to enable link clk\n", __func__);
		goto c1;
	}
	ret = clk_enable(edp_drv->mdp_core_clk);
	if (ret) {
		pr_err("%s: Failed to enable mdp_core_clk\n", __func__);
		goto c0;
	}

	edp_drv->clk_on = 1;

	return 0;

c0:
	clk_disable(edp_drv->link_clk);
c1:
	clk_disable(edp_drv->ahb_clk);
c2:
	clk_disable(edp_drv->pixel_clk);
c3:
	clk_disable(edp_drv->aux_clk);
c4:
	return ret;
}

void mdss_edp_clk_disable(struct mdss_edp_drv_pdata *edp_drv)
{
	if (edp_drv->clk_on == 0) {
		pr_info("%s: edp clks are already OFF\n", __func__);
		return;
	}

	clk_disable(edp_drv->aux_clk);
	clk_disable(edp_drv->pixel_clk);
	clk_disable(edp_drv->ahb_clk);
	clk_disable(edp_drv->link_clk);
	clk_disable(edp_drv->mdp_core_clk);

	edp_drv->clk_on = 0;
}

int mdss_edp_prepare_aux_clocks(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	/* ahb clock should be prepared first */
	ret = clk_prepare(edp_drv->ahb_clk);
	if (ret) {
		pr_err("%s: Failed to prepare ahb clk\n", __func__);
		goto c3;
	}
	ret = clk_prepare(edp_drv->aux_clk);
	if (ret) {
		pr_err("%s: Failed to prepare aux clk\n", __func__);
		goto c2;
	}

	/* need mdss clock to receive irq */
	ret = clk_prepare(edp_drv->mdp_core_clk);
	if (ret) {
		pr_err("%s: Failed to prepare mdp_core clk\n", __func__);
		goto c1;
	}

	return 0;
c1:
	clk_unprepare(edp_drv->aux_clk);
c2:
	clk_unprepare(edp_drv->ahb_clk);
c3:
	return ret;

}

void mdss_edp_unprepare_aux_clocks(struct mdss_edp_drv_pdata *edp_drv)
{
	clk_unprepare(edp_drv->mdp_core_clk);
	clk_unprepare(edp_drv->aux_clk);
	clk_unprepare(edp_drv->ahb_clk);
}

int mdss_edp_prepare_clocks(struct mdss_edp_drv_pdata *edp_drv)
{
	int ret;

	/* ahb clock should be prepared first */
	ret = clk_prepare(edp_drv->ahb_clk);
	if (ret) {
		pr_err("%s: Failed to prepare ahb clk\n", __func__);
		goto c4;
	}
	ret = clk_prepare(edp_drv->aux_clk);
	if (ret) {
		pr_err("%s: Failed to prepare aux clk\n", __func__);
		goto c3;
	}
	ret = clk_prepare(edp_drv->pixel_clk);
	if (ret) {
		pr_err("%s: Failed to prepare pixel clk\n", __func__);
		goto c2;
	}
	ret = clk_prepare(edp_drv->link_clk);
	if (ret) {
		pr_err("%s: Failed to prepare link clk\n", __func__);
		goto c1;
	}
	ret = clk_prepare(edp_drv->mdp_core_clk);
	if (ret) {
		pr_err("%s: Failed to prepare mdp_core clk\n", __func__);
		goto c0;
	}

	return 0;
c0:
	clk_unprepare(edp_drv->link_clk);
c1:
	clk_unprepare(edp_drv->pixel_clk);
c2:
	clk_unprepare(edp_drv->aux_clk);
c3:
	clk_unprepare(edp_drv->ahb_clk);
c4:
	return ret;
}

void mdss_edp_unprepare_clocks(struct mdss_edp_drv_pdata *edp_drv)
{
	clk_unprepare(edp_drv->mdp_core_clk);
	clk_unprepare(edp_drv->aux_clk);
	clk_unprepare(edp_drv->pixel_clk);
	clk_unprepare(edp_drv->link_clk);
	/* ahb clock should be last one to disable */
	clk_unprepare(edp_drv->ahb_clk);
}

void mdss_edp_clk_debug(unsigned char *edp_base, unsigned char *mmss_cc_base)
{
	u32 da4, da0, d32c;
	u32 dc4, dc0, d330;

	/* pixel clk */
	da0  = edp_read(mmss_cc_base + 0x0a0);
	da4  = edp_read(mmss_cc_base + 0x0a4);
	d32c = edp_read(mmss_cc_base + 0x32c);

	/* main link clk */
	dc0  = edp_read(mmss_cc_base + 0x0c0);
	dc4  = edp_read(mmss_cc_base + 0x0c4);
	d330 = edp_read(mmss_cc_base + 0x330);

	pr_err("%s: da0=%x da4=%x d32c=%x dc0=%x dc4=%x d330=%x\n", __func__,
	(int)da0, (int)da4, (int)d32c, (int)dc0, (int)dc4, (int)d330);

}
