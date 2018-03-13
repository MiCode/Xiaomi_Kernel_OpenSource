/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/clk/msm-clock-generic.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-dsi-pll-12nm.h"

#define DSI_PLL_POLL_MAX_READS                  15
#define DSI_PLL_POLL_TIMEOUT_US                 1000

int pixel_div_set_div(struct div_clk *clk, int div)
{
	struct mdss_pll_resources *pll = clk->priv;
	struct dsi_pll_db *pdb;

	pdb = (struct dsi_pll_db *)pll->priv;

	/* Programming during vco_prepare. Keep this value */
	pdb->param.pixel_divhf = (div - 1);

	pr_debug("ndx=%d div=%d divhf=%d\n",
			pll->index, div, pdb->param.pixel_divhf);

	return 0;
}

int pixel_div_get_div(struct div_clk *clk)
{
	u32 div;
	int rc;
	struct mdss_pll_resources *pll = clk->priv;

	if (is_gdsc_disabled(pll))
		return 0;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(pll->pll_base, DSIPHY_SSC9);
	div &= 0x7f;
	pr_debug("pixel_div = %d\n", (div+1));

	mdss_pll_resource_enable(pll, false);

	return (div + 1);
}

int set_post_div_mux_sel(struct mux_clk *clk, int sel)
{
	struct mdss_pll_resources *pll = clk->priv;
	struct dsi_pll_db *pdb;

	pdb = (struct dsi_pll_db *)pll->priv;

	/* Programming during vco_prepare. Keep this value */
	pdb->param.post_div_mux = sel;

	pr_debug("ndx=%d post_div_mux_sel=%d p_div=%d\n",
			pll->index, sel, (u32) BIT(sel));

	return 0;
}

int get_post_div_mux_sel(struct mux_clk *clk)
{
	u32 sel = 0;
	u32 vco_cntrl = 0, cpbias_cntrl = 0;
	int rc;
	struct mdss_pll_resources *pll = clk->priv;

	if (is_gdsc_disabled(pll))
		return 0;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	vco_cntrl = MDSS_PLL_REG_R(pll->pll_base, DSIPHY_PLL_VCO_CTRL);
	vco_cntrl &= 0x30;

	cpbias_cntrl = MDSS_PLL_REG_R(pll->pll_base,
		DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL);
	cpbias_cntrl = ((cpbias_cntrl >> 6) & 0x1);

	if (cpbias_cntrl == 0) {
		if (vco_cntrl == 0x00)
			sel = 0;
		else if (vco_cntrl == 0x10)
			sel = 2;
		else if (vco_cntrl == 0x20)
			sel = 3;
		else if (vco_cntrl == 0x30)
			sel = 4;
	} else if (cpbias_cntrl == 1) {
		if (vco_cntrl == 0x30)
			sel = 2;
		else if (vco_cntrl == 0x00)
			sel = 5;
	}

	mdss_pll_resource_enable(pll, false);

	return sel;
}

int set_gp_mux_sel(struct mux_clk *clk, int sel)
{
	struct mdss_pll_resources *pll = clk->priv;
	struct dsi_pll_db *pdb;

	pdb = (struct dsi_pll_db *)pll->priv;

	/* Programming during vco_prepare. Keep this value */
	pdb->param.gp_div_mux = sel;

	pr_debug("ndx=%d gp_div_mux_sel=%d gp_cntrl=%d\n",
			pll->index, sel, (u32) BIT(sel));

	return 0;
}

int get_gp_mux_sel(struct mux_clk *clk)
{
	u32 sel = 0;
	int rc;
	struct mdss_pll_resources *pll = clk->priv;

	if (is_gdsc_disabled(pll))
		return 0;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	sel = MDSS_PLL_REG_R(pll->pll_base, DSIPHY_PLL_CTRL);
	sel = (sel >> 5) & 0x7;
	pr_debug("gp_cntrl = %d\n", sel);

	mdss_pll_resource_enable(pll, false);

	return sel;
}

static bool pll_is_pll_locked_12nm(struct mdss_pll_resources *pll)
{
	u32 status;
	bool pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic((pll->pll_base +
			DSIPHY_STAT0),
			status,
			((status & BIT(1)) > 0),
			DSI_PLL_POLL_MAX_READS,
			DSI_PLL_POLL_TIMEOUT_US)) {
		pr_err("DSI PLL ndx=%d status=%x failed to Lock\n",
			pll->index, status);
		pll_locked = false;
	} else {
		pll_locked = true;
	}

	return pll_locked;
}

int dsi_pll_enable_seq_12nm(struct mdss_pll_resources *pll)
{
	int rc = 0;
	struct dsi_pll_db *pdb;
	void __iomem *pll_base;

	if (!pll) {
		pr_err("Invalid PLL resources\n");
		return -EINVAL;
	}

	pdb = (struct dsi_pll_db *)pll->priv;
	if (!pdb) {
		pr_err("No priv found\n");
		return -EINVAL;
	}

	pll_base = pll->pll_base;

	MDSS_PLL_REG_W(pll_base, DSIPHY_SYS_CTRL, 0x49);
	wmb(); /* make sure register committed before enabling branch clocks */
	udelay(5); /* h/w recommended delay */
	MDSS_PLL_REG_W(pll_base, DSIPHY_SYS_CTRL, 0xc9);
	wmb(); /* make sure register committed before enabling branch clocks */
	udelay(50); /* h/w recommended delay */

	if (!pll_is_pll_locked_12nm(pll)) {
		pr_err("DSI PLL ndx=%d lock failed!\n",
			pll->index);
		rc = -EINVAL;
		goto init_lock_err;
	}

	pr_debug("DSI PLL ndx:%d Locked!\n", pll->index);

init_lock_err:
	return rc;
}

static int dsi_pll_enable(struct clk *c)
{
	int i, rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	/* Try all enable sequences until one succeeds */
	for (i = 0; i < vco->pll_en_seq_cnt; i++) {
		rc = vco->pll_enable_seqs[i](pll);
		pr_debug("DSI PLL %s after sequence #%d\n",
			rc ? "unlocked" : "locked", i + 1);
		if (!rc)
			break;
	}

	if (rc)
		pr_err("ndx=%d DSI PLL failed to lock\n", pll->index);
	else
		pll->pll_on = true;

	return rc;
}

static int dsi_pll_relock(struct mdss_pll_resources *pll)
{
	void __iomem *pll_base = pll->pll_base;
	u32 data = 0;
	int rc = 0;

	data = MDSS_PLL_REG_R(pll_base, DSIPHY_PLL_POWERUP_CTRL);
	data &= ~BIT(1); /* remove ONPLL_OVR_EN bit */
	data |= 0x1; /* set ONPLL_OVN to 0x1 */
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_POWERUP_CTRL, data);
	ndelay(500); /* h/w recommended delay */

	if (!pll_is_pll_locked_12nm(pll)) {
		pr_err("DSI PLL ndx=%d lock failed!\n",
			pll->index);
		rc = -EINVAL;
		goto relock_err;
	}
	ndelay(50); /* h/w recommended delay */

	data = MDSS_PLL_REG_R(pll_base, DSIPHY_PLL_CTRL);
	data |= 0x01; /* set CLK_SEL bits to 0x1 */
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_CTRL, data);
	ndelay(500); /* h/w recommended delay */
	wmb(); /* make sure register committed before enabling branch clocks */
	pll->pll_on = true;
relock_err:
	return rc;
}

static void dsi_pll_disable(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;
	void __iomem *pll_base = pll->pll_base;
	u32 data = 0;

	if (!pll->pll_on &&
		mdss_pll_resource_enable(pll, true)) {
		pr_err("Failed to enable mdss dsi pll=%d\n", pll->index);
		return;
	}

	data = MDSS_PLL_REG_R(pll_base, DSIPHY_SSC0);
	data &= ~BIT(6); /* disable GP_CLK_EN */
	MDSS_PLL_REG_W(pll_base, DSIPHY_SSC0, data);
	ndelay(500); /* h/w recommended delay */

	data = MDSS_PLL_REG_R(pll_base, DSIPHY_PLL_CTRL);
	data &= ~0x03; /* remove CLK_SEL bits */
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_CTRL, data);
	ndelay(500); /* h/w recommended delay */

	data = MDSS_PLL_REG_R(pll_base, DSIPHY_PLL_POWERUP_CTRL);
	data &= ~0x1; /* remove ONPLL_OVR bit */
	data |= BIT(1); /* set ONPLL_OVR_EN to 0x1 */
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_POWERUP_CTRL, data);
	ndelay(500); /* h/w recommended delay */
	wmb(); /* make sure register committed before disabling branch clocks */
	pll->handoff_resources = false;

	mdss_pll_resource_enable(pll, false);

	pll->pll_on = false;

	pr_debug("DSI PLL ndx=%d Disabled\n", pll->index);
}

static u32 __mdss_dsi_get_hsfreqrange(u64 target_freq)
{
	u64 bitclk_rate_mhz = div_u64((target_freq * 2), 1000000);

	if (bitclk_rate_mhz >= 80 && bitclk_rate_mhz < 90)
		return 0x00;
	else if (bitclk_rate_mhz >= 90 && bitclk_rate_mhz < 100)
		return 0x10;
	else if (bitclk_rate_mhz >= 100 && bitclk_rate_mhz < 110)
		return  0x20;
	else if (bitclk_rate_mhz >= 110 && bitclk_rate_mhz < 120)
		return  0x30;
	else if (bitclk_rate_mhz >= 120 && bitclk_rate_mhz < 130)
		return  0x01;
	else if (bitclk_rate_mhz >= 130 && bitclk_rate_mhz < 140)
		return  0x11;
	else if (bitclk_rate_mhz >= 140 && bitclk_rate_mhz < 150)
		return  0x21;
	else if (bitclk_rate_mhz >= 150 && bitclk_rate_mhz < 160)
		return  0x31;
	else if (bitclk_rate_mhz >= 160 && bitclk_rate_mhz < 170)
		return  0x02;
	else if (bitclk_rate_mhz >= 170 && bitclk_rate_mhz < 180)
		return  0x12;
	else if (bitclk_rate_mhz >= 180 && bitclk_rate_mhz < 190)
		return  0x22;
	else if (bitclk_rate_mhz >= 190 && bitclk_rate_mhz < 205)
		return  0x32;
	else if (bitclk_rate_mhz >= 205 && bitclk_rate_mhz < 220)
		return  0x03;
	else if (bitclk_rate_mhz >= 220 && bitclk_rate_mhz < 235)
		return  0x13;
	else if (bitclk_rate_mhz >= 235 && bitclk_rate_mhz < 250)
		return  0x23;
	else if (bitclk_rate_mhz >= 250 && bitclk_rate_mhz < 275)
		return  0x33;
	else if (bitclk_rate_mhz >= 275 && bitclk_rate_mhz < 300)
		return  0x04;
	else if (bitclk_rate_mhz >= 300 && bitclk_rate_mhz < 325)
		return  0x14;
	else if (bitclk_rate_mhz >= 325 && bitclk_rate_mhz < 350)
		return  0x25;
	else if (bitclk_rate_mhz >= 350 && bitclk_rate_mhz < 400)
		return  0x35;
	else if (bitclk_rate_mhz >= 400 && bitclk_rate_mhz < 450)
		return  0x05;
	else if (bitclk_rate_mhz >= 450 && bitclk_rate_mhz < 500)
		return  0x16;
	else if (bitclk_rate_mhz >= 500 && bitclk_rate_mhz < 550)
		return  0x26;
	else if (bitclk_rate_mhz >= 550 && bitclk_rate_mhz < 600)
		return  0x37;
	else if (bitclk_rate_mhz >= 600 && bitclk_rate_mhz < 650)
		return  0x07;
	else if (bitclk_rate_mhz >= 650 && bitclk_rate_mhz < 700)
		return  0x18;
	else if (bitclk_rate_mhz >= 700 && bitclk_rate_mhz < 750)
		return  0x28;
	else if (bitclk_rate_mhz >= 750 && bitclk_rate_mhz < 800)
		return  0x39;
	else if (bitclk_rate_mhz >= 800 && bitclk_rate_mhz < 850)
		return  0x09;
	else if (bitclk_rate_mhz >= 850 && bitclk_rate_mhz < 900)
		return  0x19;
	else if (bitclk_rate_mhz >= 900 && bitclk_rate_mhz < 950)
		return  0x29;
	else if (bitclk_rate_mhz >= 950 && bitclk_rate_mhz < 1000)
		return  0x3a;
	else if (bitclk_rate_mhz >= 1000 && bitclk_rate_mhz < 1050)
		return  0x0a;
	else if (bitclk_rate_mhz >= 1050 && bitclk_rate_mhz < 1100)
		return  0x1a;
	else if (bitclk_rate_mhz >= 1100 && bitclk_rate_mhz < 1150)
		return  0x2a;
	else if (bitclk_rate_mhz >= 1150 && bitclk_rate_mhz < 1200)
		return  0x3b;
	else if (bitclk_rate_mhz >= 1200 && bitclk_rate_mhz < 1250)
		return  0x0b;
	else if (bitclk_rate_mhz >= 1250 && bitclk_rate_mhz < 1300)
		return  0x1b;
	else if (bitclk_rate_mhz >= 1300 && bitclk_rate_mhz < 1350)
		return  0x2b;
	else if (bitclk_rate_mhz >= 1350 && bitclk_rate_mhz < 1400)
		return  0x3c;
	else if (bitclk_rate_mhz >= 1400 && bitclk_rate_mhz < 1450)
		return  0x0c;
	else if (bitclk_rate_mhz >= 1450 && bitclk_rate_mhz < 1500)
		return  0x1c;
	else if (bitclk_rate_mhz >= 1500 && bitclk_rate_mhz < 1550)
		return  0x2c;
	else if (bitclk_rate_mhz >= 1550 && bitclk_rate_mhz < 1600)
		return  0x3d;
	else if (bitclk_rate_mhz >= 1600 && bitclk_rate_mhz < 1650)
		return  0x0d;
	else if (bitclk_rate_mhz >= 1650 && bitclk_rate_mhz < 1700)
		return  0x1d;
	else if (bitclk_rate_mhz >= 1700 && bitclk_rate_mhz < 1750)
		return  0x2e;
	else if (bitclk_rate_mhz >= 1750 && bitclk_rate_mhz < 1800)
		return  0x3e;
	else if (bitclk_rate_mhz >= 1800 && bitclk_rate_mhz < 1850)
		return  0x0e;
	else if (bitclk_rate_mhz >= 1850 && bitclk_rate_mhz < 1900)
		return  0x1e;
	else if (bitclk_rate_mhz >= 1900 && bitclk_rate_mhz < 1950)
		return  0x2f;
	else if (bitclk_rate_mhz >= 1950 && bitclk_rate_mhz < 2000)
		return  0x3f;
	else if (bitclk_rate_mhz >= 2000 && bitclk_rate_mhz < 2050)
		return  0x0f;
	else if (bitclk_rate_mhz >= 2050 && bitclk_rate_mhz < 2100)
		return  0x40;
	else if (bitclk_rate_mhz >= 2100 && bitclk_rate_mhz < 2150)
		return  0x41;
	else if (bitclk_rate_mhz >= 2150 && bitclk_rate_mhz < 2200)
		return  0x42;
	else if (bitclk_rate_mhz >= 2200 && bitclk_rate_mhz < 2250)
		return  0x43;
	else if (bitclk_rate_mhz >= 2250 && bitclk_rate_mhz < 2300)
		return  0x44;
	else if (bitclk_rate_mhz >= 2300 && bitclk_rate_mhz < 2350)
		return  0x45;
	else if (bitclk_rate_mhz >= 2350 && bitclk_rate_mhz < 2400)
		return  0x46;
	else if (bitclk_rate_mhz >= 2400 && bitclk_rate_mhz < 2450)
		return  0x47;
	else if (bitclk_rate_mhz >= 2450 && bitclk_rate_mhz < 2500)
		return  0x48;
	else
		return  0x49;
}

static void __mdss_dsi_get_pll_vco_cntrl(u64 target_freq, u32 post_div_mux,
	u32 *vco_cntrl, u32 *cpbias_cntrl)
{
	u64 target_freq_mhz = div_u64(target_freq, 1000000);
	u32 p_div = BIT(post_div_mux);

	if (p_div == 1) {
		*vco_cntrl = 0x00;
		*cpbias_cntrl = 0;
	} else if (p_div == 2) {
		*vco_cntrl = 0x30;
		*cpbias_cntrl = 1;
	} else if (p_div == 4) {
		*vco_cntrl = 0x10;
		*cpbias_cntrl = 0;
	} else if (p_div == 8) {
		*vco_cntrl = 0x20;
		*cpbias_cntrl = 0;
	} else if (p_div == 16) {
		*vco_cntrl = 0x30;
		*cpbias_cntrl = 0;
	} else {
		*vco_cntrl = 0x00;
		*cpbias_cntrl = 1;
	}

	if (target_freq_mhz <= 1250 && target_freq_mhz >= 1092)
		*vco_cntrl = *vco_cntrl | 2;
	else if (target_freq_mhz < 1092 && target_freq_mhz >= 950)
		*vco_cntrl =  *vco_cntrl | 3;
	else if (target_freq_mhz < 950 && target_freq_mhz >= 712)
		*vco_cntrl = *vco_cntrl | 1;
	else if (target_freq_mhz < 712 && target_freq_mhz >= 546)
		*vco_cntrl =  *vco_cntrl | 2;
	else if (target_freq_mhz < 546 && target_freq_mhz >= 475)
		*vco_cntrl = *vco_cntrl | 3;
	else if (target_freq_mhz < 475 && target_freq_mhz >= 356)
		*vco_cntrl =  *vco_cntrl | 1;
	else if (target_freq_mhz < 356 && target_freq_mhz >= 273)
		*vco_cntrl = *vco_cntrl | 2;
	else if (target_freq_mhz < 273 && target_freq_mhz >= 237)
		*vco_cntrl =  *vco_cntrl | 3;
	else if (target_freq_mhz < 237 && target_freq_mhz >= 178)
		*vco_cntrl = *vco_cntrl | 1;
	else if (target_freq_mhz < 178 && target_freq_mhz >= 136)
		*vco_cntrl =  *vco_cntrl | 2;
	else if (target_freq_mhz < 136 && target_freq_mhz >= 118)
		*vco_cntrl = *vco_cntrl | 3;
	else if (target_freq_mhz < 118 && target_freq_mhz >= 89)
		*vco_cntrl =  *vco_cntrl | 1;
	else if (target_freq_mhz < 89 && target_freq_mhz >= 68)
		*vco_cntrl = *vco_cntrl | 2;
	else if (target_freq_mhz < 68 && target_freq_mhz >= 57)
		*vco_cntrl =  *vco_cntrl | 3;
	else if (target_freq_mhz < 57 && target_freq_mhz >= 44)
		*vco_cntrl = *vco_cntrl | 1;
	else
		*vco_cntrl =  *vco_cntrl | 2;
}

static u32 __mdss_dsi_get_osc_freq_target(u64 target_freq)
{
	u64 target_freq_mhz = div_u64(target_freq, 1000000);

	if (target_freq_mhz <= 1000)
		return 1315;
	else if (target_freq_mhz > 1000 && target_freq_mhz <= 1500)
		return 1839;
	else
		return 0;
}

static u64 __mdss_dsi_pll_get_m_div(u64 vco_rate)
{
	return div_u64((vco_rate * 4), 19200000);
}

static u32 __mdss_dsi_get_fsm_ovr_ctrl(u64 target_freq)
{
	u64 bitclk_rate_mhz = div_u64((target_freq * 2), 1000000);

	if (bitclk_rate_mhz > 1500 && bitclk_rate_mhz <= 2500)
		return 0;
	else
		return BIT(6);
}

static void mdss_dsi_pll_12nm_calc_reg(struct mdss_pll_resources *pll,
					struct dsi_pll_db *pdb)
{
	struct dsi_pll_param *param = &pdb->param;
	u64 target_freq = 0;

	target_freq = div_u64(pll->vco_current_rate,
		BIT(pdb->param.post_div_mux));

	param->hsfreqrange = __mdss_dsi_get_hsfreqrange(target_freq);
	__mdss_dsi_get_pll_vco_cntrl(target_freq, param->post_div_mux,
		&param->vco_cntrl, &param->cpbias_cntrl);
	param->osc_freq_target = __mdss_dsi_get_osc_freq_target(target_freq);
	param->m_div = (u32) __mdss_dsi_pll_get_m_div(pll->vco_current_rate);
	param->fsm_ovr_ctrl = __mdss_dsi_get_fsm_ovr_ctrl(target_freq);
	param->prop_cntrl = 0x05;
	param->int_cntrl = 0x00;
	param->gmp_cntrl = 0x1;
}

static void pll_db_commit_12nm(struct mdss_pll_resources *pll,
					struct dsi_pll_db *pdb)
{
	void __iomem *pll_base = pll->pll_base;
	struct dsi_pll_param *param = &pdb->param;
	char data = 0;

	MDSS_PLL_REG_W(pll_base, DSIPHY_CTRL0, 0x01);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_CTRL, 0x05);
	MDSS_PLL_REG_W(pll_base, DSIPHY_SLEWRATE_DDL_LOOP_CTRL, 0x01);

	data = ((param->hsfreqrange & 0x7f) | BIT(7));
	MDSS_PLL_REG_W(pll_base, DSIPHY_HS_FREQ_RAN_SEL, data);

	data = ((param->vco_cntrl & 0x3f) | BIT(6));
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_VCO_CTRL, data);

	data = (param->osc_freq_target & 0x7f);
	MDSS_PLL_REG_W(pll_base, DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_0, data);

	data = ((param->osc_freq_target & 0xf80) >> 7);
	MDSS_PLL_REG_W(pll_base, DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_1, data);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_INPUT_LOOP_DIV_RAT_CTRL, 0x30);

	data = (param->m_div & 0x3f);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_LOOP_DIV_RATIO_0, data);

	data = ((param->m_div & 0xfc0) >> 6);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_LOOP_DIV_RATIO_1, data);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_INPUT_DIV_PLL_OVR, 0x60);

	data = (param->prop_cntrl & 0x3f);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PROP_CHRG_PUMP_CTRL, data);

	data = (param->int_cntrl & 0x3f);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_INTEG_CHRG_PUMP_CTRL, data);

	data = ((param->gmp_cntrl & 0x3) << 4);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_GMP_CTRL_DIG_TST, data);

	data = ((param->cpbias_cntrl & 0x1) << 6) | BIT(4);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL, data);

	data = ((param->gp_div_mux & 0x7) << 5) | 0x5;
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_CTRL, data);

	data = (param->pixel_divhf & 0x7f);
	MDSS_PLL_REG_W(pll_base, DSIPHY_SSC9, data);

	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_ANA_PROG_CTRL, 0x03);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_ANA_TST_LOCK_ST_OVR_CTRL, 0x50);
	MDSS_PLL_REG_W(pll_base,
		DSIPHY_SLEWRATE_FSM_OVR_CTRL, param->fsm_ovr_ctrl);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PHA_ERR_CTRL_0, 0x01);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PHA_ERR_CTRL_1, 0x00);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_LOCK_FILTER, 0xff);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_UNLOCK_FILTER, 0x03);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_PRO_DLY_RELOCK, 0x0c);
	MDSS_PLL_REG_W(pll_base, DSIPHY_PLL_LOCK_DET_MODE_SEL, 0x02);

	pr_debug("pll:%d\n", pll->index);
	wmb(); /* make sure register committed before preparing the clocks */
}

int pll_vco_set_rate_12nm(struct clk *c, unsigned long rate)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;
	struct dsi_pll_db *pdb;

	pdb = (struct dsi_pll_db *)pll->priv;
	if (!pdb) {
		pr_err("pll pdb not found\n");
		rc = -EINVAL;
		goto error;
	}

	pr_debug("%s: ndx=%d rate=%lu\n", __func__, pll->index, rate);

	pll->vco_current_rate = rate;
	pll->vco_ref_clk_rate = vco->ref_clk_rate;
error:
	return rc;
}

static unsigned long pll_vco_get_rate_12nm(struct clk *c)
{
	u64 vco_rate = 0;
	u32 m_div_5_0 = 0, m_div_11_6 = 0, m_div = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	u64 ref_clk = vco->ref_clk_rate;
	int rc;
	struct mdss_pll_resources *pll = vco->priv;

	if (is_gdsc_disabled(pll))
		return 0;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll=%d\n", pll->index);
		return rc;
	}

	m_div_5_0 = MDSS_PLL_REG_R(pll->pll_base,
			DSIPHY_PLL_LOOP_DIV_RATIO_0);
	m_div_5_0 &= 0x3f;
	pr_debug("m_div_5_0 = 0x%x\n", m_div_5_0);

	m_div_11_6 = MDSS_PLL_REG_R(pll->pll_base,
			DSIPHY_PLL_LOOP_DIV_RATIO_1);
	m_div_11_6 &= 0x3f;
	pr_debug("m_div_11_6 = 0x%x\n", m_div_11_6);

	m_div = ((m_div_11_6 << 6) | (m_div_5_0));

	vco_rate = div_u64((ref_clk * m_div), 4);

	pr_debug("returning vco rate = %lu\n", (unsigned long)vco_rate);

	mdss_pll_resource_enable(pll, false);

	return (unsigned long)vco_rate;
}

long pll_vco_round_rate_12nm(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	return rrate;
}

enum handoff pll_vco_handoff_12nm(struct clk *c)
{
	int rc;
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (is_gdsc_disabled(pll))
		return HANDOFF_DISABLED_CLK;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll=%d\n", pll->index);
		return ret;
	}

	if (pll_is_pll_locked_12nm(pll)) {
		pll->handoff_resources = true;
		pll->pll_on = true;
		c->rate = pll_vco_get_rate_12nm(c);
		ret = HANDOFF_ENABLED_CLK;
	} else {
		mdss_pll_resource_enable(pll, false);
	}

	return ret;
}

int pll_vco_prepare_12nm(struct clk *c)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;
	struct dsi_pll_db *pdb;
	u32 data = 0;

	if (!pll) {
		pr_err("Dsi pll resources are not available\n");
		return -EINVAL;
	}

	pdb = (struct dsi_pll_db *)pll->priv;
	if (!pdb) {
		pr_err("No prov found\n");
		return -EINVAL;
	}

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("ndx=%d Failed to enable mdss dsi pll resources\n",
							pll->index);
		return rc;
	}

	if ((pll->vco_cached_rate != 0)
	    && (pll->vco_cached_rate == c->rate)) {
		rc = c->ops->set_rate(c, pll->vco_cached_rate);
		if (rc) {
			pr_err("index=%d vco_set_rate failed. rc=%d\n",
					rc, pll->index);
			goto error;
		}

		data = MDSS_PLL_REG_R(pll->pll_base, DSIPHY_SYS_CTRL);
		if (data & BIT(7)) { /* DSI PHY in LP-11 or ULPS */
			rc = dsi_pll_relock(pll);
			if (rc)
				goto error;
			else
				goto end;
		}
	}

	mdss_dsi_pll_12nm_calc_reg(pll, pdb);

	/* commit DSI vco  */
	pll_db_commit_12nm(pll, pdb);

	rc = dsi_pll_enable(c);

error:
	if (rc) {
		mdss_pll_resource_enable(pll, false);
		pr_err("ndx=%d failed to enable dsi pll\n", pll->index);
	}

end:
	return rc;
}

void pll_vco_unprepare_12nm(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (!pll) {
		pr_err("Dsi pll resources are not available\n");
		return;
	}

	pll->vco_cached_rate = c->rate;
	dsi_pll_disable(c);
}

int pll_vco_enable_12nm(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (!pll) {
		pr_err("Dsi pll resources are not available\n");
		return -EINVAL;
	}

	if (!pll->pll_on) {
		pr_err("DSI PLL not enabled, return\n");
		return -EINVAL;
	}

	MDSS_PLL_REG_W(pll->pll_base, DSIPHY_SSC0, 0x40);
	wmb(); /* make sure register committed before enabling branch clocks */

	return 0;
}
