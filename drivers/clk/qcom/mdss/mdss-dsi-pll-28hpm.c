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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>

#include <dt-bindings/clock/msm-clocks-8974.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"

#define DSI_PHY_PLL_UNIPHY_PLL_REFCLK_CFG	(0x0)
#define DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG	(0x0004)
#define DSI_PHY_PLL_UNIPHY_PLL_CHGPUMP_CFG	(0x0008)
#define DSI_PHY_PLL_UNIPHY_PLL_VCOLPF_CFG	(0x000C)
#define DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG		(0x0010)
#define DSI_PHY_PLL_UNIPHY_PLL_PWRGEN_CFG	(0x0014)
#define DSI_PHY_PLL_UNIPHY_PLL_DMUX_CFG		(0x0018)
#define DSI_PHY_PLL_UNIPHY_PLL_AMUX_CFG		(0x001C)
#define DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG		(0x0020)
#define DSI_PHY_PLL_UNIPHY_PLL_POSTDIV2_CFG	(0x0024)
#define DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG	(0x0028)
#define DSI_PHY_PLL_UNIPHY_PLL_LPFR_CFG		(0x002C)
#define DSI_PHY_PLL_UNIPHY_PLL_LPFC1_CFG	(0x0030)
#define DSI_PHY_PLL_UNIPHY_PLL_LPFC2_CFG	(0x0034)
#define DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG0		(0x0038)
#define DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG1		(0x003C)
#define DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG2		(0x0040)
#define DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG3		(0x0044)
#define DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG4		(0x0048)
#define DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG0		(0x004C)
#define DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG1		(0x0050)
#define DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG2		(0x0054)
#define DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG3		(0x0058)
#define DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG0	(0x005C)
#define DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG1	(0x0060)
#define DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2	(0x0064)
#define DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG		(0x0068)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG0		(0x006C)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG1		(0x0070)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG2		(0x0074)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG3		(0x0078)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG4		(0x007C)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG5		(0x0080)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG6		(0x0084)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG7		(0x0088)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG8		(0x008C)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG9		(0x0090)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG10	(0x0094)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG11	(0x0098)
#define DSI_PHY_PLL_UNIPHY_PLL_EFUSE_CFG	(0x009C)
#define DSI_PHY_PLL_UNIPHY_PLL_STATUS		(0x00C0)


#define DSI_PLL_POLL_MAX_READS			10
#define DSI_PLL_POLL_TIMEOUT_US			50

static struct clk_div_ops fixed_2div_ops;
static struct clk_ops byte_mux_clk_ops;
static struct clk_ops pixel_clk_src_ops;
static struct clk_ops byte_clk_src_ops;
static struct clk_ops analog_postdiv_clk_ops;

int set_byte_mux_sel(struct mux_clk *clk, int sel)
{
	int rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	pr_debug("byte mux set to %s mode\n", sel ? "indirect" : "direct");
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG, (sel << 1));

	mdss_pll_resource_enable(dsi_pll_res, false);
	return 0;
}

int get_byte_mux_sel(struct mux_clk *clk)
{
	int mux_mode, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	mux_mode = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG) & BIT(1);

	pr_debug("byte mux mode = %s", mux_mode ? "indirect" : "direct");
	mdss_pll_resource_enable(dsi_pll_res, false);

	return !!mux_mode;
}

static inline struct dsi_pll_vco_clk *to_vco_clk(struct clk *clk)
{
	return container_of(clk, struct dsi_pll_vco_clk, c);
}

int dsi_pll_div_prepare(struct clk *c)
{
	struct div_clk *div = to_div_clk(c);
	/* Restore the divider's value */
	return div->ops->set_div(div, div->data.div);
}

int dsi_pll_mux_prepare(struct clk *c)
{
	struct mux_clk *mux = to_mux_clk(c);
	int i, rc, sel = 0;

	for (i = 0; i < mux->num_parents; i++)
		if (mux->parents[i].src == c->parent) {
			sel = mux->parents[i].sel;
			break;
		}

	if (i == mux->num_parents) {
		pr_err("Failed to select the parent clock\n");
		rc = -EINVAL;
		goto error;
	}

	/* Restore the mux source select value */
	rc = mux->ops->set_mux_sel(mux, sel);

error:
	return rc;
}

static int fixed_4div_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_POSTDIV2_CFG, (div - 1));

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int fixed_4div_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_POSTDIV2_CFG);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return div + 1;
}

static int digital_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG, (div - 1));

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int digital_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return div + 1;
}

static int analog_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG, div - 1);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int analog_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	rc = mdss_pll_resource_enable(clk->priv, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	div = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG) + 1;

	mdss_pll_resource_enable(dsi_pll_res, false);

	return div;
}

static int dsi_pll_lock_status(struct mdss_pll_resources *dsi_pll_res)
{
	u32 status;
	int pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_noirq((dsi_pll_res->pll_base +
			DSI_PHY_PLL_UNIPHY_PLL_STATUS),
			status,
			((status & BIT(0)) == 1),
			DSI_PLL_POLL_MAX_READS,
			DSI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("DSI PLL status=%x failed to Lock\n", status);
		pll_locked = 0;
	} else {
		pll_locked = 1;
	}

	return pll_locked;
}

static void dsi_pll_software_reset(struct mdss_pll_resources *dsi_pll_res)
{
	/*
	 * Add HW recommended delays after toggling the software
	 * reset bit off and back on.
	 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x01);
	udelay(1);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x00);
	udelay(1);
}

static int dsi_pll_enable_seq_8974(struct mdss_pll_resources *dsi_pll_res)
{
	int i, rc = 0;
	u32 max_reads, timeout_us;
	int pll_locked;

	dsi_pll_software_reset(dsi_pll_res);

	/*
	 * PLL power up sequence.
	 * Add necessary delays recommeded by hardware.
	 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
	udelay(1);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
	udelay(200);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x07);
	udelay(500);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
	udelay(500);

	for (i = 0; i < 2; i++) {
		udelay(100);
		/* DSI Uniphy lock detect setting */
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0c);
		udelay(100);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0d);
		/* poll for PLL ready status */
		max_reads = 5;
		timeout_us = 100;

		pll_locked = dsi_pll_lock_status(dsi_pll_res);
		if (pll_locked)
			break;

		dsi_pll_software_reset(dsi_pll_res);
		/*
		 * PLL power up sequence.
		 * Add necessary delays recommeded by hardware.
		 */
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x1);
		udelay(1);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x5);
		udelay(200);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x7);
		udelay(250);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x5);
		udelay(200);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x7);
		udelay(500);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0xf);
		udelay(500);

	}

	if (!pll_locked) {
		pr_err("DSI PLL lock failed\n");
		rc = -EINVAL;
	} else {
		pr_debug("DSI PLL Lock success\n");
	}

	return rc;
}

static int dsi_pll_enable(struct clk *c)
{
	int i, rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	/* Try all enable sequences until one succeeds */
	for (i = 0; i < vco->pll_en_seq_cnt; i++) {
		rc = vco->pll_enable_seqs[i](dsi_pll_res);
		pr_debug("DSI PLL %s after sequence #%d\n",
			rc ? "unlocked" : "locked", i + 1);
		if (!rc)
			break;
	}

	if (rc) {
		mdss_pll_resource_enable(dsi_pll_res, false);
		pr_err("DSI PLL failed to lock\n");
	}
	dsi_pll_res->pll_on = true;

	return rc;
}

static void dsi_pll_disable(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res->pll_on &&
		mdss_pll_resource_enable(dsi_pll_res, true)) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return;
	}

	dsi_pll_res->handoff_resources = false;

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x00);

	mdss_pll_resource_enable(dsi_pll_res, false);
	dsi_pll_res->pll_on = false;

	pr_debug("DSI PLL Disabled\n");
	return;
}

static int vco_set_rate(struct clk *c, unsigned long rate)
{
	s64 vco_clk_rate = rate;
	s32 rem;
	s64 refclk_cfg, frac_n_mode, ref_doubler_en_b;
	s64 ref_clk_to_pll, div_fbx1000, frac_n_value;
	s64 sdm_cfg0, sdm_cfg1, sdm_cfg2, sdm_cfg3;
	s64 gen_vco_clk, cal_cfg10, cal_cfg11;
	u32 res;
	int i, rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	/* Configure the Loop filter resistance */
	for (i = 0; i < vco->lpfr_lut_size; i++)
		if (vco_clk_rate <= vco->lpfr_lut[i].vco_rate)
			break;
	if (i == vco->lpfr_lut_size) {
		pr_err("unable to get loop filter resistance. vco=%ld\n", rate);
		rc = -EINVAL;
		goto error;
	}
	res = vco->lpfr_lut[i].r;
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_LPFR_CFG, res);

	/* Loop filter capacitance values : c1 and c2 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_LPFC1_CFG, 0x70);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_LPFC2_CFG, 0x15);

	div_s64_rem(vco_clk_rate, vco->ref_clk_rate, &rem);
	if (rem) {
		refclk_cfg = 0x1;
		frac_n_mode = 1;
		ref_doubler_en_b = 0;
	} else {
		refclk_cfg = 0x0;
		frac_n_mode = 0;
		ref_doubler_en_b = 1;
	}

	pr_debug("refclk_cfg = %lld\n", refclk_cfg);

	ref_clk_to_pll = ((vco->ref_clk_rate * 2 * (refclk_cfg))
			  + (ref_doubler_en_b * vco->ref_clk_rate));
	div_fbx1000 = div_s64((vco_clk_rate * 1000), ref_clk_to_pll);

	div_s64_rem(div_fbx1000, 1000, &rem);
	frac_n_value = div_s64((rem * (1 << 16)), 1000);
	gen_vco_clk = div_s64(div_fbx1000 * ref_clk_to_pll, 1000);

	pr_debug("ref_clk_to_pll = %lld\n", ref_clk_to_pll);
	pr_debug("div_fb = %lld\n", div_fbx1000);
	pr_debug("frac_n_value = %lld\n", frac_n_value);

	pr_debug("Generated VCO Clock: %lld\n", gen_vco_clk);
	rem = 0;
	if (frac_n_mode) {
		sdm_cfg0 = (0x0 << 5);
		sdm_cfg0 |= (0x0 & 0x3f);
		sdm_cfg1 = (div_s64(div_fbx1000, 1000) & 0x3f) - 1;
		sdm_cfg3 = div_s64_rem(frac_n_value, 256, &rem);
		sdm_cfg2 = rem;
	} else {
		sdm_cfg0 = (0x1 << 5);
		sdm_cfg0 |= (div_s64(div_fbx1000, 1000) & 0x3f) - 1;
		sdm_cfg1 = (0x0 & 0x3f);
		sdm_cfg2 = 0;
		sdm_cfg3 = 0;
	}

	pr_debug("sdm_cfg0=%lld\n", sdm_cfg0);
	pr_debug("sdm_cfg1=%lld\n", sdm_cfg1);
	pr_debug("sdm_cfg2=%lld\n", sdm_cfg2);
	pr_debug("sdm_cfg3=%lld\n", sdm_cfg3);

	cal_cfg11 = div_s64_rem(gen_vco_clk, 256 * 1000000, &rem);
	cal_cfg10 = rem / 1000000;
	pr_debug("cal_cfg10=%lld, cal_cfg11=%lld\n", cal_cfg10, cal_cfg11);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CHGPUMP_CFG, 0x02);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG3, 0x2b);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG4, 0x66);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0d);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG1, (u32)(sdm_cfg1 & 0xff));
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG2, (u32)(sdm_cfg2 & 0xff));
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG3, (u32)(sdm_cfg3 & 0xff));
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG4, 0x00);

	/* Add hardware recommended delay for correct PLL configuration */
	udelay(1);

	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_REFCLK_CFG, (u32)refclk_cfg);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_PWRGEN_CFG, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_VCOLPF_CFG, 0x71);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG0, (u32)sdm_cfg0);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG0, 0x12);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG6, 0x30);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG7, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG8, 0x60);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG9, 0x00);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG10, (u32)(cal_cfg10 & 0xff));
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG11, (u32)(cal_cfg11 & 0xff));
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_EFUSE_CFG, 0x20);

error:
	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static long vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	return rrate;
}

static unsigned long vco_get_rate(struct clk *c)
{
	u32 sdm0, doubler, sdm_byp_div;
	u64 vco_rate;
	u32 sdm_dc_off, sdm_freq_seed, sdm2, sdm3;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	u64 ref_clk = vco->ref_clk_rate;
	int rc;
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	/* Check to see if the ref clk doubler is enabled */
	doubler = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
				 DSI_PHY_PLL_UNIPHY_PLL_REFCLK_CFG) & BIT(0);
	ref_clk += (doubler * vco->ref_clk_rate);

	/* see if it is integer mode or sdm mode */
	sdm0 = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG0);
	if (sdm0 & BIT(6)) {
		/* integer mode */
		sdm_byp_div = (MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG0) & 0x3f) + 1;
		vco_rate = ref_clk * sdm_byp_div;
	} else {
		/* sdm mode */
		sdm_dc_off = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG1) & 0xFF;
		pr_debug("sdm_dc_off = %d\n", sdm_dc_off);
		sdm2 = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG2) & 0xFF;
		sdm3 = MDSS_PLL_REG_R(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG3) & 0xFF;
		sdm_freq_seed = (sdm3 << 8) | sdm2;
		pr_debug("sdm_freq_seed = %d\n", sdm_freq_seed);

		vco_rate = (ref_clk * (sdm_dc_off + 1)) +
			mult_frac(ref_clk, sdm_freq_seed, BIT(16));
		pr_debug("vco rate = %lld", vco_rate);
	}

	pr_debug("returning vco rate = %lu\n", (unsigned long)vco_rate);

	mdss_pll_resource_enable(dsi_pll_res, false);

	return (unsigned long)vco_rate;
}

static enum handoff vco_handoff(struct clk *c)
{
	int rc;
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return ret;
	}

	if (dsi_pll_lock_status(dsi_pll_res)) {
		dsi_pll_res->handoff_resources = true;
		dsi_pll_res->pll_on = true;
		c->rate = vco_get_rate(c);
		ret = HANDOFF_ENABLED_CLK;
	} else {
		mdss_pll_resource_enable(dsi_pll_res, false);
	}

	return ret;
}

static int vco_prepare(struct clk *c)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res) {
		pr_err("Dsi pll resources are not available\n");
		return -EINVAL;
	}

	if ((dsi_pll_res->vco_cached_rate != 0)
	    && (dsi_pll_res->vco_cached_rate == c->rate)) {
		rc = vco_set_rate(c, dsi_pll_res->vco_cached_rate);
		if (rc) {
			pr_err("vco_set_rate failed. rc=%d\n", rc);
			goto error;
		}
	}

	rc = dsi_pll_enable(c);

error:
	return rc;
}

static void vco_unprepare(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res) {
		pr_err("Dsi pll resources are not available\n");
		return;
	}

	dsi_pll_res->vco_cached_rate = c->rate;
	dsi_pll_disable(c);
}

/* Op structures */

static struct clk_ops clk_ops_dsi_vco = {
	.set_rate = vco_set_rate,
	.round_rate = vco_round_rate,
	.handoff = vco_handoff,
	.prepare = vco_prepare,
	.unprepare = vco_unprepare,
};


static struct clk_div_ops fixed_4div_ops = {
	.set_div = fixed_4div_set_div,
	.get_div = fixed_4div_get_div,
};

static struct clk_div_ops analog_postdiv_ops = {
	.set_div = analog_set_div,
	.get_div = analog_get_div,
};

static struct clk_div_ops digital_postdiv_ops = {
	.set_div = digital_set_div,
	.get_div = digital_get_div,
};

static struct clk_mux_ops byte_mux_ops = {
	.set_mux_sel = set_byte_mux_sel,
	.get_mux_sel = get_byte_mux_sel,
};

static struct dsi_pll_vco_clk dsi_vco_clk_8974 = {
	.ref_clk_rate = 19200000,
	.min_rate = 350000000,
	.max_rate = 750000000,
	.pll_en_seq_cnt = 3,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_8974,
	.pll_enable_seqs[1] = dsi_pll_enable_seq_8974,
	.pll_enable_seqs[2] = dsi_pll_enable_seq_8974,
	.lpfr_lut_size = 10,
	.lpfr_lut = (struct lpfr_cfg[]){
		{479500000, 8},
		{480000000, 11},
		{575500000, 8},
		{576000000, 12},
		{610500000, 8},
		{659500000, 9},
		{671500000, 10},
		{672000000, 14},
		{708500000, 10},
		{750000000, 11},
	},
	.c = {
		.dbg_name = "dsi_vco_clk",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi_vco_clk_8974.c),
	},
};

static struct div_clk analog_postdiv_clk_8974 = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &analog_postdiv_ops,
	.c = {
		.parent = &dsi_vco_clk_8974.c,
		.dbg_name = "analog_postdiv_clk",
		.ops = &analog_postdiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(analog_postdiv_clk_8974.c),
	},
};

static struct div_clk indirect_path_div2_clk_8974 = {
	.ops = &fixed_2div_ops,
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &analog_postdiv_clk_8974.c,
		.dbg_name = "indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(indirect_path_div2_clk_8974.c),
	},
};

struct div_clk pixel_clk_src_8974 = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &digital_postdiv_ops,
	.c = {
		.parent = &dsi_vco_clk_8974.c,
		.dbg_name = "pixel_clk_src",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(pixel_clk_src_8974.c),
	},
};

struct mux_clk byte_mux_8974 = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi_vco_clk_8974.c, 0},
		{&indirect_path_div2_clk_8974.c, 1},
	},
	.ops = &byte_mux_ops,
	.c = {
		.parent = &dsi_vco_clk_8974.c,
		.dbg_name = "byte_mux",
		.ops = &byte_mux_clk_ops,
		CLK_INIT(byte_mux_8974.c),
	},
};

struct div_clk byte_clk_src_8974 = {
	.ops = &fixed_4div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &byte_mux_8974.c,
		.dbg_name = "byte_clk_src",
		.ops = &byte_clk_src_ops,
		CLK_INIT(byte_clk_src_8974.c),
	},
};

static struct clk_lookup mdss_dsi_pllcc_8974[] = {
	CLK_LOOKUP_OF("pixel_src", pixel_clk_src_8974,
						"fd8c0000.qcom,mmsscc-mdss"),
	CLK_LOOKUP_OF("byte_src", byte_clk_src_8974,
						"fd8c0000.qcom,mmsscc-mdss"),
};

int dsi_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc;

	if (!pll_res || !pll_res->pll_base) {
		pr_err("Invalide input parameters\n");
		return -EPROBE_DEFER;
	}

	/* Set client data to mux, div and vco clocks */
	byte_clk_src_8974.priv = pll_res;
	byte_mux_8974.priv = pll_res;
	pixel_clk_src_8974.priv = pll_res;
	indirect_path_div2_clk_8974.priv = pll_res;
	analog_postdiv_clk_8974.priv = pll_res;
	dsi_vco_clk_8974.priv = pll_res;

	/* Set clock source operations */
	pixel_clk_src_ops = clk_ops_slave_div;
	pixel_clk_src_ops.prepare = dsi_pll_div_prepare;

	analog_postdiv_clk_ops = clk_ops_div;
	analog_postdiv_clk_ops.prepare = dsi_pll_div_prepare;

	byte_clk_src_ops = clk_ops_div;
	byte_clk_src_ops.prepare = dsi_pll_div_prepare;

	byte_mux_clk_ops = clk_ops_gen_mux;
	byte_mux_clk_ops.prepare = dsi_pll_mux_prepare;

	rc = of_msm_clock_register(pdev->dev.of_node, mdss_dsi_pllcc_8974,
					 ARRAY_SIZE(mdss_dsi_pllcc_8974));
	if (rc) {
		pr_err("Clock register failed\n");
		rc = -EPROBE_DEFER;
	}

	return rc;
}
