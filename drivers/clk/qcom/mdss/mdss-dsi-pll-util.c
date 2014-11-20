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
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/clk/msm-clock-generic.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"

#define DSI_PHY_PLL_UNIPHY_PLL_REFCLK_CFG	(0x0)
#define DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG	(0x0004)
#define DSI_PHY_PLL_UNIPHY_PLL_CHGPUMP_CFG	(0x0008)
#define DSI_PHY_PLL_UNIPHY_PLL_VCOLPF_CFG	(0x000C)
#define DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG		(0x0010)
#define DSI_PHY_PLL_UNIPHY_PLL_PWRGEN_CFG	(0x0014)
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
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG0		(0x006C)
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

int set_byte_mux_sel(struct mux_clk *clk, int sel)
{
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	pr_debug("byte mux set to %s mode\n", sel ? "indirect" : "direct");
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG, (sel << 1));

	return 0;
}

int get_byte_mux_sel(struct mux_clk *clk)
{
	int mux_mode, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

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
	struct mdss_pll_resources *dsi_pll_res = mux->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

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
	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

int fixed_4div_set_div(struct div_clk *clk, int div)
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

int fixed_4div_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

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

int digital_set_div(struct div_clk *clk, int div)
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

int digital_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

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

int analog_set_div(struct div_clk *clk, int div)
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

int analog_get_div(struct div_clk *clk)
{
	int div = 0, rc;
	struct mdss_pll_resources *dsi_pll_res = clk->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

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

int dsi_pll_lock_status(struct mdss_pll_resources *dsi_pll_res)
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

int vco_set_rate(struct dsi_pll_vco_clk *vco, unsigned long rate)
{
	s64 vco_clk_rate = rate;
	s32 rem;
	s64 refclk_cfg, frac_n_mode, ref_doubler_en_b;
	s64 ref_clk_to_pll, div_fbx1000, frac_n_value;
	s64 sdm_cfg0, sdm_cfg1, sdm_cfg2, sdm_cfg3;
	s64 gen_vco_clk, cal_cfg10, cal_cfg11;
	u32 res;
	int i;
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	/* Configure the Loop filter resistance */
	for (i = 0; i < vco->lpfr_lut_size; i++)
		if (vco_clk_rate <= vco->lpfr_lut[i].vco_rate)
			break;
	if (i == vco->lpfr_lut_size) {
		pr_err("unable to get loop filter resistance. vco=%ld\n", rate);
		return -EINVAL;
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
	if (dsi_pll_res->vco_delay)
		udelay(dsi_pll_res->vco_delay);

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

	return 0;
}

unsigned long vco_get_rate(struct clk *c)
{
	u32 sdm0, doubler, sdm_byp_div;
	u64 vco_rate;
	u32 sdm_dc_off, sdm_freq_seed, sdm2, sdm3;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	u64 ref_clk = vco->ref_clk_rate;
	int rc;
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return 0;

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

long vco_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	return rrate;
}

enum handoff vco_handoff(struct clk *c)
{
	int rc;
	enum handoff ret = HANDOFF_DISABLED_CLK;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (is_gdsc_disabled(dsi_pll_res))
		return HANDOFF_DISABLED_CLK;

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

int vco_prepare(struct clk *c)
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
		rc = c->ops->set_rate(c, dsi_pll_res->vco_cached_rate);
		if (rc) {
			pr_err("vco_set_rate failed. rc=%d\n", rc);
			goto error;
		}
	}

	rc = dsi_pll_enable(c);

error:
	return rc;
}

void vco_unprepare(struct clk *c)
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

