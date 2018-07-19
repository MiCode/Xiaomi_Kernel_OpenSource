/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-dsi-pll-28nm.h"

#define DSI_PHY_PLL_UNIPHY_PLL_REFCLK_CFG	(0x0)
#define DSI_PHY_PLL_UNIPHY_PLL_CHGPUMP_CFG	(0x0008)
#define DSI_PHY_PLL_UNIPHY_PLL_VCOLPF_CFG	(0x000C)
#define DSI_PHY_PLL_UNIPHY_PLL_PWRGEN_CFG	(0x0014)
#define DSI_PHY_PLL_UNIPHY_PLL_POSTDIV2_CFG	(0x0024)
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

#define DSI_PLL_POLL_DELAY_US			50
#define DSI_PLL_POLL_TIMEOUT_US			500

int analog_postdiv_reg_read(void *context, unsigned int reg,
		unsigned int *div)
{
	int rc = 0;
	struct mdss_pll_resources *rsc = context;

	if (is_gdsc_disabled(rsc))
		return 0;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	*div = MDSS_PLL_REG_R(rsc->pll_base, reg);

	/**
	 * Common clock framework the divider value is interpreted as one less
	 * hence we return one less for all dividers except when zero
	 */
	if (*div != 0)
		*div -= 1;

	pr_debug("analog_postdiv div = %d\n", *div);

	(void)mdss_pll_resource_enable(rsc, false);
	return rc;
}

int analog_postdiv_reg_write(void *context, unsigned int reg,
		unsigned int div)
{
	int rc = 0;
	struct mdss_pll_resources *rsc = context;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	pr_debug("analog_postdiv div = %d\n", div);

	/**
	 * In common clock framework the divider value provided is one less and
	 * and hence adjusting the divider value by one prior to writing it to
	 * hardware
	 */
	div++;

	MDSS_PLL_REG_W(rsc->pll_base, reg, div);

	(void)mdss_pll_resource_enable(rsc, false);
	return rc;
}

int byteclk_mux_read_sel(void *context, unsigned int reg,
		unsigned int *val)
{
	int rc = 0;
	struct mdss_pll_resources *rsc = context;

	if (is_gdsc_disabled(rsc))
		return 0;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	*val = ((MDSS_PLL_REG_R(rsc->pll_base, reg) & BIT(1)) >> 1);
	pr_debug("byteclk mux mode = %s", *val ? "indirect" : "direct");

	(void)mdss_pll_resource_enable(rsc, false);
	return rc;
}

int byteclk_mux_write_sel(void *context, unsigned int reg,
		unsigned int val)
{
	int rc = 0;
	u32 reg_val = 0;
	struct mdss_pll_resources *rsc = context;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	pr_debug("byteclk mux set to %s mode\n", val ? "indirect" : "direct");

	reg_val = MDSS_PLL_REG_R(rsc->pll_base, reg);
	reg_val &= ~0x02;
	reg_val |= (val << 1);

	MDSS_PLL_REG_W(rsc->pll_base, reg, reg_val);

	(void)mdss_pll_resource_enable(rsc, false);

	return rc;
}

int pixel_clk_get_div(void *context, unsigned int reg,
		unsigned int *div)
{
	int rc = 0;
	struct mdss_pll_resources *rsc = context;

	if (is_gdsc_disabled(rsc))
		return 0;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	*div = MDSS_PLL_REG_R(rsc->pll_base, reg);

	/**
	 *Common clock framework the divider value is interpreted as one less
	 * hence we return one less for all dividers except when zero
	 */
	if (*div != 0)
		*div -= 1;

	pr_debug("pclk_src div = %d\n", *div);

	(void)mdss_pll_resource_enable(rsc, false);
	return rc;
}

int pixel_clk_set_div(void *context, unsigned int reg,
		unsigned int div)
{
	int rc = 0;
	struct mdss_pll_resources *rsc = context;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	pr_debug("pclk_src div = %d\n", div);

	/**
	 * In common clock framework the divider value provided is one less and
	 * and hence adjusting the divider value by one prior to writing it to
	 * hardware
	 */
	div++;

	MDSS_PLL_REG_W(rsc->pll_base, reg, div);

	(void)mdss_pll_resource_enable(rsc, false);
	return rc;
}

int dsi_pll_lock_status(struct mdss_pll_resources *rsc)
{
	u32 status;
	int pll_locked;

	/* poll for PLL ready status */
	if (readl_poll_timeout_atomic((rsc->pll_base +
			DSI_PHY_PLL_UNIPHY_PLL_STATUS),
			status,
			((status & BIT(0)) == 1),
			DSI_PLL_POLL_DELAY_US,
			DSI_PLL_POLL_TIMEOUT_US)) {
		pr_debug("DSI PLL status=%x failed to Lock\n", status);
		pll_locked = 0;
	} else {
		pll_locked = 1;
	}

	return pll_locked;
}

static int pll_28nm_vco_rate_calc(struct dsi_pll_vco_clk *vco,
		struct mdss_dsi_vco_calc *vco_calc, unsigned long vco_clk_rate)
{
	s32 rem;
	s64 frac_n_mode, ref_doubler_en_b;
	s64 ref_clk_to_pll, div_fb, frac_n_value;
	int i;

	/* Configure the Loop filter resistance */
	for (i = 0; i < vco->lpfr_lut_size; i++)
		if (vco_clk_rate <= vco->lpfr_lut[i].vco_rate)
			break;
	if (i == vco->lpfr_lut_size) {
		pr_err("unable to get loop filter resistance. vco=%ld\n",
			vco_clk_rate);
		return -EINVAL;
	}
	vco_calc->lpfr_lut_res = vco->lpfr_lut[i].r;

	div_s64_rem(vco_clk_rate, vco->ref_clk_rate, &rem);
	if (rem) {
		vco_calc->refclk_cfg = 0x1;
		frac_n_mode = 1;
		ref_doubler_en_b = 0;
	} else {
		vco_calc->refclk_cfg = 0x0;
		frac_n_mode = 0;
		ref_doubler_en_b = 1;
	}

	pr_debug("refclk_cfg = %lld\n", vco_calc->refclk_cfg);

	ref_clk_to_pll = ((vco->ref_clk_rate * 2 * (vco_calc->refclk_cfg))
			  + (ref_doubler_en_b * vco->ref_clk_rate));

	div_fb = div_s64_rem(vco_clk_rate, ref_clk_to_pll, &rem);
	frac_n_value = div_s64(((s64)rem * (1 << 16)), ref_clk_to_pll);
	vco_calc->gen_vco_clk = vco_clk_rate;

	pr_debug("ref_clk_to_pll = %lld\n", ref_clk_to_pll);
	pr_debug("div_fb = %lld\n", div_fb);
	pr_debug("frac_n_value = %lld\n", frac_n_value);

	pr_debug("Generated VCO Clock: %lld\n", vco_calc->gen_vco_clk);
	rem = 0;
	if (frac_n_mode) {
		vco_calc->sdm_cfg0 = 0;
		vco_calc->sdm_cfg1 = (div_fb & 0x3f) - 1;
		vco_calc->sdm_cfg3 = div_s64_rem(frac_n_value, 256, &rem);
		vco_calc->sdm_cfg2 = rem;
	} else {
		vco_calc->sdm_cfg0 = (0x1 << 5);
		vco_calc->sdm_cfg0 |= (div_fb & 0x3f) - 1;
		vco_calc->sdm_cfg1 = 0;
		vco_calc->sdm_cfg2 = 0;
		vco_calc->sdm_cfg3 = 0;
	}

	pr_debug("sdm_cfg0=%lld\n", vco_calc->sdm_cfg0);
	pr_debug("sdm_cfg1=%lld\n", vco_calc->sdm_cfg1);
	pr_debug("sdm_cfg2=%lld\n", vco_calc->sdm_cfg2);
	pr_debug("sdm_cfg3=%lld\n", vco_calc->sdm_cfg3);

	vco_calc->cal_cfg11 = div_s64_rem(vco_calc->gen_vco_clk,
			256 * 1000000, &rem);
	vco_calc->cal_cfg10 = rem / 1000000;
	pr_debug("cal_cfg10=%lld, cal_cfg11=%lld\n",
		vco_calc->cal_cfg10, vco_calc->cal_cfg11);

	return 0;
}

static void pll_28nm_ssc_param_calc(struct dsi_pll_vco_clk *vco,
		struct mdss_dsi_vco_calc *vco_calc)
{
	struct mdss_pll_resources *rsc = vco->priv;
	s64 ppm_freq, incr, spread_freq, div_rf, frac_n_value;
	s32 rem;

	if (!rsc->ssc_en) {
		pr_debug("DSI PLL SSC not enabled\n");
		return;
	}

	vco_calc->ssc.kdiv = DIV_ROUND_CLOSEST(vco->ref_clk_rate,
			1000000) - 1;
	vco_calc->ssc.triang_steps = DIV_ROUND_CLOSEST(vco->ref_clk_rate,
			rsc->ssc_freq * (vco_calc->ssc.kdiv + 1));
	ppm_freq = div_s64(vco_calc->gen_vco_clk * rsc->ssc_ppm,
			1000000);
	incr = div64_s64(ppm_freq * 65536, vco->ref_clk_rate * 2 *
			vco_calc->ssc.triang_steps);

	vco_calc->ssc.triang_inc_7_0 = incr & 0xff;
	vco_calc->ssc.triang_inc_9_8 = (incr >> 8) & 0x3;

	if (!rsc->ssc_center)
		spread_freq = vco_calc->gen_vco_clk - ppm_freq;
	else
		spread_freq = vco_calc->gen_vco_clk - (ppm_freq / 2);

	div_rf = div_s64(spread_freq, 2 * vco->ref_clk_rate);
	vco_calc->ssc.dc_offset = (div_rf - 1);

	div_s64_rem(spread_freq, 2 * vco->ref_clk_rate, &rem);
	frac_n_value = div_s64((s64)rem * 65536, 2 * vco->ref_clk_rate);

	vco_calc->ssc.freq_seed_7_0 = frac_n_value & 0xff;
	vco_calc->ssc.freq_seed_15_8 = (frac_n_value >> 8) & 0xff;
}

static void pll_28nm_vco_config(struct dsi_pll_vco_clk *vco,
		struct mdss_dsi_vco_calc *vco_calc)
{
	struct mdss_pll_resources *rsc = vco->priv;
	void __iomem *pll_base = rsc->pll_base;
	u32 vco_delay_us = rsc->vco_delay;
	bool ssc_en = rsc->ssc_en;

	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_LPFR_CFG,
		vco_calc->lpfr_lut_res);

	/* Loop filter capacitance values : c1 and c2 */
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_LPFC1_CFG, 0x70);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_LPFC2_CFG, 0x15);

	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CHGPUMP_CFG, 0x02);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG3, 0x2b);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG4, 0x66);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0d);

	if (!ssc_en) {
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG1,
			(u32)(vco_calc->sdm_cfg1 & 0xff));
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG2,
			(u32)(vco_calc->sdm_cfg2 & 0xff));
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG3,
			(u32)(vco_calc->sdm_cfg3 & 0xff));
	} else {
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG1,
			(u32)vco_calc->ssc.dc_offset);
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG2,
			(u32)vco_calc->ssc.freq_seed_7_0);
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG3,
			(u32)vco_calc->ssc.freq_seed_15_8);
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG0,
			(u32)vco_calc->ssc.kdiv);
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG1,
			(u32)vco_calc->ssc.triang_inc_7_0);
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG2,
			(u32)vco_calc->ssc.triang_inc_9_8);
		MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SSC_CFG3,
			(u32)vco_calc->ssc.triang_steps);
	}
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG4, 0x00);

	/* Add hardware recommended delay for correct PLL configuration */
	if (vco_delay_us)
		udelay(vco_delay_us);

	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_REFCLK_CFG,
		(u32)vco_calc->refclk_cfg);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_PWRGEN_CFG, 0x00);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_VCOLPF_CFG, 0x71);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG0,
		(u32)vco_calc->sdm_cfg0);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG0, 0x12);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG6, 0x30);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG7, 0x00);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG8, 0x60);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG9, 0x00);
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG10,
		(u32)(vco_calc->cal_cfg10 & 0xff));
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG11,
		(u32)(vco_calc->cal_cfg11 & 0xff));
	MDSS_PLL_REG_W(pll_base, DSI_PHY_PLL_UNIPHY_PLL_EFUSE_CFG, 0x20);
}

static int vco_set_rate(struct dsi_pll_vco_clk *vco, unsigned long rate)
{
	struct mdss_dsi_vco_calc vco_calc = {0};
	int rc = 0;

	rc = pll_28nm_vco_rate_calc(vco, &vco_calc, rate);
	if (rc) {
		pr_err("vco rate calculation failed\n");
		return rc;
	}

	pll_28nm_ssc_param_calc(vco, &vco_calc);
	pll_28nm_vco_config(vco, &vco_calc);

	return 0;
}

static unsigned long vco_get_rate(struct dsi_pll_vco_clk *vco)
{
	struct mdss_pll_resources *rsc = vco->priv;
	int rc;
	u32 sdm0, doubler, sdm_byp_div;
	u64 vco_rate;
	u32 sdm_dc_off, sdm_freq_seed, sdm2, sdm3;
	u64 ref_clk = vco->ref_clk_rate;

	if (is_gdsc_disabled(rsc))
		return 0;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	/* Check to see if the ref clk doubler is enabled */
	doubler = MDSS_PLL_REG_R(rsc->pll_base,
				 DSI_PHY_PLL_UNIPHY_PLL_REFCLK_CFG) & BIT(0);
	ref_clk += (doubler * vco->ref_clk_rate);

	/* see if it is integer mode or sdm mode */
	sdm0 = MDSS_PLL_REG_R(rsc->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG0);
	if (sdm0 & BIT(6)) {
		/* integer mode */
		sdm_byp_div = (MDSS_PLL_REG_R(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG0) & 0x3f) + 1;
		vco_rate = ref_clk * sdm_byp_div;
	} else {
		/* sdm mode */
		sdm_dc_off = MDSS_PLL_REG_R(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG1) & 0xFF;
		pr_debug("sdm_dc_off = %d\n", sdm_dc_off);
		sdm2 = MDSS_PLL_REG_R(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG2) & 0xFF;
		sdm3 = MDSS_PLL_REG_R(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_SDM_CFG3) & 0xFF;
		sdm_freq_seed = (sdm3 << 8) | sdm2;
		pr_debug("sdm_freq_seed = %d\n", sdm_freq_seed);

		vco_rate = (ref_clk * (sdm_dc_off + 1)) +
			mult_frac(ref_clk, sdm_freq_seed, BIT(16));
		pr_debug("vco rate = %lld", vco_rate);
	}

	pr_debug("returning vco rate = %lu\n", (unsigned long)vco_rate);

	mdss_pll_resource_enable(rsc, false);

	return (unsigned long)vco_rate;
}

static int dsi_pll_enable(struct dsi_pll_vco_clk *vco)
{
	int i, rc;
	struct mdss_pll_resources *rsc = vco->priv;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("failed to enable dsi pll(%d) resources\n",
				rsc->index);
		return rc;
	}

	/* Try all enable sequences until one succeeds */
	for (i = 0; i < vco->pll_en_seq_cnt; i++) {
		rc = vco->pll_enable_seqs[i](rsc);
		pr_debug("DSI PLL %s after sequence #%d\n",
			rc ? "unlocked" : "locked", i + 1);
		if (!rc)
			break;
	}

	if (rc) {
		mdss_pll_resource_enable(rsc, false);
		pr_err("DSI PLL failed to lock\n");
	}
	rsc->pll_on = true;

	return rc;
}

static void dsi_pll_disable(struct dsi_pll_vco_clk *vco)
{
	struct mdss_pll_resources *rsc = vco->priv;

	if (!rsc->pll_on &&
		mdss_pll_resource_enable(rsc, true)) {
		pr_err("failed to enable dsi pll(%d) resources\n",
				rsc->index);
		return;
	}

	rsc->handoff_resources = false;

	MDSS_PLL_REG_W(rsc->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x00);

	mdss_pll_resource_enable(rsc, false);
	rsc->pll_on = false;

	pr_debug("DSI PLL Disabled\n");
}

int vco_28nm_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct mdss_pll_resources *rsc = vco->priv;
	int rc;

	if (!rsc) {
		pr_err("pll resource not found\n");
		return -EINVAL;
	}

	if (rsc->pll_on)
		return 0;

	pr_debug("ndx=%d, rate=%lu\n", rsc->index, rate);

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("failed to enable mdss dsi pll(%d), rc=%d\n",
		       rsc->index, rc);
		return rc;
	}

	/*
	 * DSI PLL software reset. Add HW recommended delays after toggling
	 * the software reset bit off and back on.
	 */
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x01);
	udelay(1000);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x00);
	udelay(1000);

	rc = vco_set_rate(vco, rate);

	mdss_pll_resource_enable(rsc, false);

	return 0;
}

long vco_28nm_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	unsigned long rrate = rate;
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	*parent_rate = rrate;

	return rrate;
}

unsigned long vco_28nm_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct mdss_pll_resources *rsc = vco->priv;
	int rc;
	u64 vco_rate = 0;

	if (!rsc) {
		pr_err("dsi pll resources not available\n");
		return 0;
	}

	if (is_gdsc_disabled(rsc))
		return 0;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("failed to enable dsi pll(%d) resources\n",
				rsc->index);
		return 0;
	}

	if (dsi_pll_lock_status(rsc)) {
		rsc->handoff_resources = true;
		rsc->pll_on = true;
		vco_rate = vco_get_rate(vco);
	} else {
		mdss_pll_resource_enable(rsc, false);
	}

	return (unsigned long)vco_rate;
}

int vco_28nm_prepare(struct clk_hw *hw)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct mdss_pll_resources *rsc = vco->priv;

	if (!rsc) {
		pr_err("dsi pll resources not available\n");
		return -EINVAL;
	}

	if ((rsc->vco_cached_rate != 0)
	    && (rsc->vco_cached_rate == clk_hw_get_rate(hw))) {
		rc = hw->init->ops->set_rate(hw, rsc->vco_cached_rate,
				rsc->vco_cached_rate);
		if (rc) {
			pr_err("pll(%d ) set_rate failed. rc=%d\n",
					rsc->index, rc);
			goto error;
		}

		MDSS_PLL_REG_W(rsc->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG,
				rsc->cached_postdiv1);
		MDSS_PLL_REG_W(rsc->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG,
				rsc->cached_postdiv3);
		MDSS_PLL_REG_W(rsc->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG,
				rsc->cached_vreg_cfg);
	}

	rc = dsi_pll_enable(vco);

error:
	return rc;
}

void vco_28nm_unprepare(struct clk_hw *hw)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct mdss_pll_resources *rsc = vco->priv;

	if (!rsc) {
		pr_err("dsi pll resources not available\n");
		return;
	}

	rsc->cached_postdiv1 = MDSS_PLL_REG_R(rsc->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG);
	rsc->cached_postdiv3 = MDSS_PLL_REG_R(rsc->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG);
	rsc->cached_vreg_cfg = MDSS_PLL_REG_R(rsc->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG);

	rsc->vco_cached_rate = clk_hw_get_rate(hw);

	dsi_pll_disable(vco);
}
