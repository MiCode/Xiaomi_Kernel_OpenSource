/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-cobalt.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-pll.h"

#define VCO_DELAY_USEC 1

#define MHZ_375		375000000UL
#define MHZ_750		750000000UL
#define MHZ_1500	1500000000UL
#define MHZ_1900	1900000000UL
#define MHZ_3000	3000000000UL

/* Register Offsets from PLL base address */
#define PLL_ANALOG_CONTROLS_ONE			0x000
#define PLL_ANALOG_CONTROLS_TWO			0x004
#define PLL_ANALOG_CONTROLS_THREE		0x010
#define PLL_DSM_DIVIDER				0x01c
#define PLL_FEEDBACK_DIVIDER			0x020
#define PLL_SYSTEM_MUXES			0x024
#define PLL_CMODE				0x02c
#define PLL_CALIBRATION_SETTINGS		0x030
#define PLL_BAND_SEL_CAL_SETTINGS_THREE		0x054
#define PLL_FREQ_DETECT_SETTINGS_ONE		0x064
#define PLL_OUTDIV				0x094
#define PLL_CORE_OVERRIDE			0x0a4
#define PLL_CORE_INPUT_OVERRIDE			0x0a8
#define PLL_PLL_DIGITAL_TIMERS_TWO		0x0b4
#define PLL_DECIMAL_DIV_START_1			0x0cc
#define PLL_FRAC_DIV_START_LOW_1		0x0d0
#define PLL_FRAC_DIV_START_MID_1		0x0d4
#define PLL_FRAC_DIV_START_HIGH_1		0x0d8
#define PLL_PLL_OUTDIV_RATE			0x140
#define PLL_PLL_LOCKDET_RATE_1			0x144
#define PLL_PLL_PROP_GAIN_RATE_1		0x14c
#define PLL_PLL_BAND_SET_RATE_1			0x154
#define PLL_PLL_INT_GAIN_IFILT_BAND_1		0x15c
#define PLL_PLL_FL_INT_GAIN_PFILT_BAND_1	0x164
#define PLL_PLL_LOCK_OVERRIDE			0x180
#define PLL_PLL_LOCK_DELAY			0x184
#define PLL_COMMON_STATUS_ONE			0x1a0

/* Register Offsets from PHY base address */
#define PHY_CMN_CLK_CFG0	0x010
#define PHY_CMN_CLK_CFG1	0x014
#define PHY_CMN_RBUF_CTRL	0x01c
#define PHY_CMN_PLL_CNTRL	0x038
#define PHY_CMN_CTRL_0		0x024

enum {
	DSI_PLL_0,
	DSI_PLL_1,
	DSI_PLL_MAX
};

struct dsi_pll_regs {
	u32 pll_prop_gain_rate;
	u32 pll_outdiv_rate;
	u32 pll_lockdet_rate;
	u32 decimal_div_start;
	u32 frac_div_start_low;
	u32 frac_div_start_mid;
	u32 frac_div_start_high;
};

struct dsi_pll_config {
	u32 ref_freq;
	bool div_override;
	u32 output_div;
	bool ignore_frac;
	bool disable_prescaler;
	u32 dec_bits;
	u32 frac_bits;
	u32 lock_timer;
	u32 ssc_freq;
	u32 ssc_offset;
	u32 ssc_adj_per;
	u32 thresh_cycles;
	u32 refclk_cycles;
};

struct dsi_pll_cobalt {
	struct mdss_pll_resources *rsc;
	struct dsi_pll_config pll_configuration;
	struct dsi_pll_regs reg_setup;
};

static struct mdss_pll_resources *pll_rsc_db[DSI_PLL_MAX];
static struct dsi_pll_cobalt plls[DSI_PLL_MAX];

static void dsi_pll_config_slave(struct mdss_pll_resources *rsc)
{
	u32 reg;
	struct mdss_pll_resources *orsc = pll_rsc_db[DSI_PLL_1];

	if (!rsc)
		return;

	/* Only DSI PLL0 can act as a master */
	if (rsc->index != DSI_PLL_0)
		return;

	/* default configuration: source is either internal or ref clock */
	rsc->slave = NULL;

	if (!orsc) {
		pr_warn("slave PLL unavilable, assuming standalone config\n");
		return;
	}

	/* check to see if the source of DSI1 PLL bitclk is set to external */
	reg = MDSS_PLL_REG_R(orsc->phy_base, PHY_CMN_CLK_CFG1);
	reg &= (BIT(2) | BIT(3));
	if (reg == 0x04)
		rsc->slave = pll_rsc_db[DSI_PLL_1]; /* external source */

	pr_debug("Slave PLL %s\n", rsc->slave ? "configured" : "absent");
}

static void dsi_pll_setup_config(struct dsi_pll_cobalt *pll,
				 struct mdss_pll_resources *rsc)
{
	struct dsi_pll_config *config = &pll->pll_configuration;

	config->ref_freq = 19200000;
	config->output_div = 1;
	config->dec_bits = 8;
	config->frac_bits = 18;
	config->lock_timer = 64;
	config->ssc_freq = 31500;
	config->ssc_offset = 4800;
	config->ssc_adj_per = 2;
	config->thresh_cycles = 32;
	config->refclk_cycles = 256;

	config->div_override = false;
	config->ignore_frac = false;
	config->disable_prescaler = false;

	dsi_pll_config_slave(rsc);
}

static void dsi_pll_calc_dec_frac(struct dsi_pll_cobalt *pll,
				  struct mdss_pll_resources *rsc)
{
	struct dsi_pll_config *config = &pll->pll_configuration;
	struct dsi_pll_regs *regs = &pll->reg_setup;
	u64 target_freq;
	u64 fref = rsc->vco_ref_clk_rate;
	u32 computed_output_div, div_log;
	u64 pll_freq;
	u64 divider;
	u64 dec, dec_multiple;
	u32 frac;
	u64 multiplier;

	target_freq = rsc->vco_current_rate;
	pr_debug("target_freq = %llu\n", target_freq);

	if (config->div_override) {
		computed_output_div = config->output_div;
	} else {
		if (target_freq < MHZ_375) {
			computed_output_div = 8;
			div_log = 3;
		} else if (target_freq < MHZ_750) {
			computed_output_div = 4;
			div_log = 2;
		} else if (target_freq < MHZ_1500) {
			computed_output_div = 2;
			div_log = 1;
		} else {
			computed_output_div = 1;
			div_log = 0;
		}
	}
	pr_debug("computed_output_div = %d\n", computed_output_div);

	pll_freq = target_freq * computed_output_div;

	if (config->disable_prescaler)
		divider = fref;
	else
		divider = fref * 2;

	multiplier = 1 << config->frac_bits;
	dec_multiple = div_u64(pll_freq * multiplier, divider);
	div_u64_rem(dec_multiple, multiplier, &frac);

	dec = div_u64(dec_multiple, multiplier);

	if (pll_freq <= MHZ_1900)
		regs->pll_prop_gain_rate = 8;
	else if (pll_freq <= MHZ_3000)
		regs->pll_prop_gain_rate = 10;
	else
		regs->pll_prop_gain_rate = 12;

	regs->pll_outdiv_rate = div_log;
	regs->pll_lockdet_rate = config->lock_timer;
	regs->decimal_div_start = dec;
	regs->frac_div_start_low = (frac & 0xff);
	regs->frac_div_start_mid = (frac & 0xff00) >> 8;
	regs->frac_div_start_high = (frac & 0x30000) >> 16;
}

static void dsi_pll_config_hzindep_reg(struct dsi_pll_cobalt *pll,
				  struct mdss_pll_resources *rsc)
{
	void __iomem *pll_base = rsc->pll_base;

	MDSS_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_ONE, 0x80);
	MDSS_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_TWO, 0x03);
	MDSS_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_THREE, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_DSM_DIVIDER, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_FEEDBACK_DIVIDER, 0x4e);
	MDSS_PLL_REG_W(pll_base, PLL_CMODE, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_CALIBRATION_SETTINGS, 0x40);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS_THREE, 0xba);
	MDSS_PLL_REG_W(pll_base, PLL_FREQ_DETECT_SETTINGS_ONE, 0x0c);
	MDSS_PLL_REG_W(pll_base, PLL_OUTDIV, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_CORE_OVERRIDE, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_DIGITAL_TIMERS_TWO, 0x08);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_PROP_GAIN_RATE_1, 0x08);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_BAND_SET_RATE_1, 0xc0);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_INT_GAIN_IFILT_BAND_1, 0x82);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_FL_INT_GAIN_PFILT_BAND_1, 0x4c);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_LOCK_OVERRIDE, 0x80);
}

static void dsi_pll_commit(struct dsi_pll_cobalt *pll,
			   struct mdss_pll_resources *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	struct dsi_pll_regs *reg = &pll->reg_setup;

	MDSS_PLL_REG_W(pll_base, PLL_CORE_INPUT_OVERRIDE, 0x12);
	MDSS_PLL_REG_W(pll_base, PLL_DECIMAL_DIV_START_1,
		       reg->decimal_div_start);
	MDSS_PLL_REG_W(pll_base, PLL_FRAC_DIV_START_LOW_1,
		       reg->frac_div_start_low);
	MDSS_PLL_REG_W(pll_base, PLL_FRAC_DIV_START_MID_1,
		       reg->frac_div_start_mid);
	MDSS_PLL_REG_W(pll_base, PLL_FRAC_DIV_START_HIGH_1,
		       reg->frac_div_start_high);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_LOCKDET_RATE_1, 0xc8);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_OUTDIV_RATE, reg->pll_outdiv_rate);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_LOCK_DELAY, 0x0a);

	/* flush, ensure all register writes are done*/
	wmb();
}

static int vco_cobalt_set_rate(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *rsc = vco->priv;
	struct dsi_pll_cobalt *pll;

	if (!rsc) {
		pr_err("pll resource not found\n");
		return -EINVAL;
	}

	if (rsc->pll_on)
		return 0;

	pll = rsc->priv;
	if (!pll) {
		pr_err("pll configuration not found\n");
		return -EINVAL;
	}

	pr_debug("ndx=%d, rate=%lu\n", rsc->index, rate);

	rsc->vco_current_rate = rate;
	rsc->vco_ref_clk_rate = vco->ref_clk_rate;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("failed to enable mdss dsi pll(%d), rc=%d\n",
		       rsc->index, rc);
		return rc;
	}

	dsi_pll_setup_config(pll, rsc);

	dsi_pll_calc_dec_frac(pll, rsc);

	dsi_pll_config_hzindep_reg(pll, rsc);

	/* todo: ssc configuration */

	dsi_pll_commit(pll, rsc);

	mdss_pll_resource_enable(rsc, false);

	return 0;
}

static int dsi_pll_cobalt_lock_status(struct mdss_pll_resources *pll)
{
	int rc;
	u32 status;
	u32 const delay_us = 100;
	u32 const timeout_us = 5000;

	rc = readl_poll_timeout_atomic(pll->pll_base + PLL_COMMON_STATUS_ONE,
				       status,
				       ((status & BIT(0)) > 0),
				       delay_us,
				       timeout_us);
	if (rc)
		pr_err("DSI PLL(%d) lock failed, status=0x%08x\n",
			pll->index, status);

	return rc;
}

static void dsi_pll_disable_pll_bias(struct mdss_pll_resources *rsc)
{
	u32 data = MDSS_PLL_REG_R(rsc->phy_base, PHY_CMN_CTRL_0);

	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data & ~BIT(5));
	MDSS_PLL_REG_W(rsc->pll_base, PLL_SYSTEM_MUXES, 0);
	ndelay(250);
}

static void dsi_pll_enable_pll_bias(struct mdss_pll_resources *rsc)
{
	u32 data = MDSS_PLL_REG_R(rsc->phy_base, PHY_CMN_CTRL_0);

	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data | BIT(5));
	MDSS_PLL_REG_W(rsc->pll_base, PLL_SYSTEM_MUXES, 0xc0);
	ndelay(250);
}

static int dsi_pll_enable(struct dsi_pll_vco_clk *vco)
{
	int rc;
	struct mdss_pll_resources *rsc = vco->priv;

	dsi_pll_enable_pll_bias(rsc);
	if (rsc->slave)
		dsi_pll_enable_pll_bias(rsc->slave);

	/* Start PLL */
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_PLL_CNTRL, 0x01);

	/*
	 * ensure all PLL configurations are written prior to checking
	 * for PLL lock.
	 */
	wmb();

	/* Check for PLL lock */
	rc = dsi_pll_cobalt_lock_status(rsc);
	if (rc) {
		pr_err("PLL(%d) lock failed\n", rsc->index);
		goto error;
	}

	rsc->pll_on = true;
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_RBUF_CTRL, 0x01);
	if (rsc->slave)
		MDSS_PLL_REG_W(rsc->slave->phy_base, PHY_CMN_RBUF_CTRL, 0x01);

error:
	return rc;
}

static void dsi_pll_disable_sub(struct mdss_pll_resources *rsc)
{
	dsi_pll_disable_pll_bias(rsc);
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_RBUF_CTRL, 0);
}

static void dsi_pll_disable(struct dsi_pll_vco_clk *vco)
{
	struct mdss_pll_resources *rsc = vco->priv;

	if (!rsc->pll_on &&
	    mdss_pll_resource_enable(rsc, true)) {
		pr_err("failed to enable pll (%d) resources\n", rsc->index);
		return;
	}

	rsc->handoff_resources = false;

	pr_debug("stop PLL (%d)\n", rsc->index);

	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_PLL_CNTRL, 0);
	dsi_pll_disable_sub(rsc);
	if (rsc->slave)
		dsi_pll_disable_sub(rsc->slave);

	/* flush, ensure all register writes are done*/
	wmb();
	rsc->pll_on = false;
}

static void vco_cobalt_unprepare(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (!pll) {
		pr_err("dsi pll resources not available\n");
		return;
	}

	pll->vco_cached_rate = c->rate;
	dsi_pll_disable(vco);
	mdss_pll_resource_enable(pll, false);
}

static int vco_cobalt_prepare(struct clk *c)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;

	if (!pll) {
		pr_err("dsi pll resources are not available\n");
		return -EINVAL;
	}

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("failed to enable pll (%d) resource, rc=%d\n",
		       pll->index, rc);
		return rc;
	}

	if ((pll->vco_cached_rate != 0) &&
	    (pll->vco_cached_rate == c->rate)) {
		rc = c->ops->set_rate(c, pll->vco_cached_rate);
		if (rc) {
			pr_err("pll(%d) set_rate failed, rc=%d\n",
			       pll->index, rc);
			mdss_pll_resource_enable(pll, false);
			return rc;
		}
	}

	rc = dsi_pll_enable(vco);
	if (rc) {
		mdss_pll_resource_enable(pll, false);
		pr_err("pll(%d) enable failed, rc=%d\n", pll->index, rc);
		return rc;
	}

	return rc;
}

static unsigned long dsi_pll_get_vco_rate(struct clk *c)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;
	int rc;
	u64 ref_clk = vco->ref_clk_rate;
	u64 vco_rate;
	u64 multiplier;
	u32 frac;
	u32 dec;
	u32 outdiv;
	u64 pll_freq, tmp64;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("failed to enable pll(%d) resource, rc=%d\n",
		       pll->index, rc);
		return 0;
	}

	dec = MDSS_PLL_REG_R(pll->pll_base, PLL_DECIMAL_DIV_START_1);
	dec &= 0xFF;

	frac = MDSS_PLL_REG_R(pll->pll_base, PLL_FRAC_DIV_START_LOW_1);
	frac |= ((MDSS_PLL_REG_R(pll->pll_base, PLL_FRAC_DIV_START_MID_1) &
		  0xFF) <<
		8);
	frac |= ((MDSS_PLL_REG_R(pll->pll_base, PLL_FRAC_DIV_START_HIGH_1) &
		  0x3) <<
		16);

	/* OUTDIV_1:0 field is (log(outdiv, 2)) */
	outdiv = MDSS_PLL_REG_R(pll->pll_base, PLL_PLL_OUTDIV_RATE);
	outdiv &= 0x3;
	outdiv = 1 << outdiv;

	/*
	 * TODO:
	 *	1. Assumes prescaler is disabled
	 *	2. Multiplier is 2^18. it should be 2^(num_of_frac_bits)
	 **/
	multiplier = 1 << 18;
	pll_freq = dec * (ref_clk * 2);
	tmp64 = (ref_clk * 2 * frac);
	pll_freq += div_u64(tmp64, multiplier);

	vco_rate = div_u64(pll_freq, outdiv);

	pr_debug("dec=0x%x, frac=0x%x, outdiv=%d, vco=%llu\n",
		 dec, frac, outdiv, vco_rate);

	(void)mdss_pll_resource_enable(pll, false);

	return (unsigned long)vco_rate;
}

enum handoff vco_cobalt_handoff(struct clk *c)
{
	enum handoff ret = HANDOFF_DISABLED_CLK;
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll = vco->priv;
	u32 status;

	if (!pll) {
		pr_err("Unable to find pll resource\n");
		return HANDOFF_DISABLED_CLK;
	}

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("failed to enable pll(%d) resources, rc=%d\n",
		       pll->index, rc);
		return ret;
	}

	status = MDSS_PLL_REG_R(pll->pll_base, PLL_COMMON_STATUS_ONE);
	if (status & BIT(0)) {
		pll->handoff_resources = true;
		pll->pll_on = true;
		c->rate = dsi_pll_get_vco_rate(c);
		ret = HANDOFF_ENABLED_CLK;
	} else {
		(void)mdss_pll_resource_enable(pll, false);
		ret = HANDOFF_DISABLED_CLK;
	}

	return ret;
}

static int pixel_clk_get_div(struct div_clk *clk)
{
	int rc;
	struct mdss_pll_resources *pll = clk->priv;
	u32 reg_val;
	int div;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	reg_val = MDSS_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	div = (reg_val & 0xF0) >> 4;

	(void)mdss_pll_resource_enable(pll, false);

	return div;
}

static void pixel_clk_set_div_sub(struct mdss_pll_resources *pll, int div)
{
	u32 reg_val;

	reg_val = MDSS_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	reg_val &= ~0xF0;
	reg_val |= (div << 4);
	MDSS_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG0, reg_val);
}

static int pixel_clk_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *pll = clk->priv;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	pixel_clk_set_div_sub(pll, div);
	if (pll->slave)
		pixel_clk_set_div_sub(pll->slave, div);

	(void)mdss_pll_resource_enable(pll, false);

	return 0;
}

static int bit_clk_get_div(struct div_clk *clk)
{
	int rc;
	struct mdss_pll_resources *pll = clk->priv;
	u32 reg_val;
	int div;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	reg_val = MDSS_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	div = (reg_val & 0x0F);

	(void)mdss_pll_resource_enable(pll, false);

	return div;
}

static void bit_clk_set_div_sub(struct mdss_pll_resources *rsc, int div)
{
	u32 reg_val;

	reg_val = MDSS_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG0);
	reg_val &= ~0x0F;
	reg_val |= div;
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_CLK_CFG0, reg_val);
}

static int bit_clk_set_div(struct div_clk *clk, int div)
{
	int rc;
	struct mdss_pll_resources *rsc = clk->priv;
	struct dsi_pll_cobalt *pll;

	if (!rsc) {
		pr_err("pll resource not found\n");
		return -EINVAL;
	}

	pll = rsc->priv;
	if (!pll) {
		pr_err("pll configuration not found\n");
		return -EINVAL;
	}

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	bit_clk_set_div_sub(rsc, div);
	/* For slave PLL, this divider always should be set to 1 */
	if (rsc->slave)
		bit_clk_set_div_sub(rsc->slave, 1);

	(void)mdss_pll_resource_enable(rsc, false);

	return rc;
}

static int post_vco_clk_get_div(struct div_clk *clk)
{
	int rc;
	struct mdss_pll_resources *pll = clk->priv;
	u32 reg_val;
	int div;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	reg_val = MDSS_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG1);
	reg_val &= 0x3;

	if (reg_val == 2)
		div = 1;
	else if (reg_val == 3)
		div = 4;
	else
		div = 1;

	(void)mdss_pll_resource_enable(pll, false);

	return div;
}

static int post_vco_clk_set_div_sub(struct mdss_pll_resources *pll, int div)
{
	u32 reg_val;
	int rc = 0;

	reg_val = MDSS_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG1);
	reg_val &= ~0x03;
	if (div == 1) {
		reg_val |= 0x2;
	} else if (div == 4) {
		reg_val |= 0x3;
	} else {
		rc = -EINVAL;
		pr_err("unsupported divider %d\n", div);
		goto error;
	}

	MDSS_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG1, reg_val);

error:
	return rc;
}

static int post_vco_clk_set_div(struct div_clk *clk, int div)
{
	int rc = 0;
	struct mdss_pll_resources *pll = clk->priv;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	rc = post_vco_clk_set_div_sub(pll, div);
	if (!rc && pll->slave)
		rc = post_vco_clk_set_div_sub(pll->slave, div);

	(void)mdss_pll_resource_enable(pll, false);

	return rc;
}

static int post_bit_clk_get_div(struct div_clk *clk)
{
	int rc;
	struct mdss_pll_resources *pll = clk->priv;
	u32 reg_val;
	int div;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	reg_val = MDSS_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG1);
	reg_val &= 0x3;

	if (reg_val == 0)
		div = 1;
	else if (reg_val == 1)
		div = 2;
	else
		div = 1;

	(void)mdss_pll_resource_enable(pll, false);

	return div;
}

static int post_bit_clk_set_div_sub(struct mdss_pll_resources *pll, int div)
{
	int rc = 0;
	u32 reg_val;

	reg_val = MDSS_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG1);
	reg_val &= ~0x03;
	if (div == 1) {
		reg_val |= 0x0;
	} else if (div == 2) {
		reg_val |= 0x1;
	} else {
		rc = -EINVAL;
		pr_err("unsupported divider %d\n", div);
		goto error;
	}

	MDSS_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG1, reg_val);

error:
	return rc;
}

static int post_bit_clk_set_div(struct div_clk *clk, int div)
{
	int rc = 0;
	struct mdss_pll_resources *pll = clk->priv;

	rc = mdss_pll_resource_enable(pll, true);
	if (rc) {
		pr_err("Failed to enable dsi pll resources, rc=%d\n", rc);
		return rc;
	}

	rc = post_bit_clk_set_div_sub(pll, div);
	if (!rc && pll->slave)
		rc = post_bit_clk_set_div_sub(pll->slave, div);

	(void)mdss_pll_resource_enable(pll, false);

	return rc;
}

long vco_cobalt_round_rate(struct clk *c, unsigned long rate)
{
	unsigned long rrate = rate;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);

	if (rate < vco->min_rate)
		rrate = vco->min_rate;
	if (rate > vco->max_rate)
		rrate = vco->max_rate;

	return rrate;
}

/* clk ops that require runtime fixup */
static struct clk_ops clk_ops_gen_mux_dsi;
static struct clk_ops clk_ops_bitclk_src_c;
static struct clk_ops clk_ops_post_vco_div_c;
static struct clk_ops clk_ops_post_bit_div_c;
static struct clk_ops clk_ops_pclk_src_c;

static struct clk_div_ops clk_post_vco_div_ops = {
	.set_div = post_vco_clk_set_div,
	.get_div = post_vco_clk_get_div,
};

static struct clk_div_ops clk_post_bit_div_ops = {
	.set_div = post_bit_clk_set_div,
	.get_div = post_bit_clk_get_div,
};

static struct clk_div_ops pixel_clk_div_ops = {
	.set_div = pixel_clk_set_div,
	.get_div = pixel_clk_get_div,
};

static struct clk_div_ops clk_bitclk_src_ops = {
	.set_div = bit_clk_set_div,
	.get_div = bit_clk_get_div,
};

static struct clk_ops clk_ops_vco_cobalt = {
	.set_rate = vco_cobalt_set_rate,
	.round_rate = vco_cobalt_round_rate,
	.handoff = vco_cobalt_handoff,
	.prepare = vco_cobalt_prepare,
	.unprepare = vco_cobalt_unprepare,
};

static struct clk_mux_ops mdss_mux_ops = {
	.set_mux_sel = mdss_set_mux_sel,
	.get_mux_sel = mdss_get_mux_sel,
};

/*
 * Clock tree for generating DSI byte and pixel clocks.
 *
 *
 *                  +---------------+
 *                  |    vco_clk    |
 *                  +-------+-------+
 *                          |
 *                          +--------------------------------------+
 *                          |                                      |
 *                  +-------v-------+                              |
 *                  |  bitclk_src   |                              |
 *                  |  DIV(1..15)   |                              |
 *                  +-------+-------+                              |
 *                          |                                      |
 *                          +--------------------+                 |
 *   Shadow Path            |                    |                 |
 *       +          +-------v-------+     +------v------+   +------v-------+
 *       |          |  byteclk_src  |     |post_bit_div |   |post_vco_div  |
 *       |          |  DIV(8)       |     |DIV(1,2)     |   |DIV(1,4)      |
 *       |          +-------+-------+     +------+------+   +------+-------+
 *       |                  |                    |                 |
 *       |                  |                    +------+     +----+
 *       |         +--------+                           |     |
 *       |         |                               +----v-----v------+
 *     +-v---------v----+                           \  pclk_src_mux /
 *     \  byteclk_mux /                              \             /
 *      \            /                                +-----+-----+
 *       +----+-----+                                       |        Shadow Path
 *            |                                             |             +
 *            v                                       +-----v------+      |
 *       dsi_byte_clk                                 |  pclk_src  |      |
 *                                                    | DIV(1..15) |      |
 *                                                    +-----+------+      |
 *                                                          |             |
 *                                                          |             |
 *                                                          +--------+    |
 *                                                                   |    |
 *                                                               +---v----v----+
 *                                                                \  pclk_mux /
 *                                                                 \         /
 *                                                                  +---+---+
 *                                                                      |
 *                                                                      |
 *                                                                      v
 *                                                                   dsi_pclk
 *
 */

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1500000000UL,
	.max_rate = 3500000000UL,
	.c = {
		.dbg_name = "dsi0pll_vco_clk",
		.ops = &clk_ops_vco_cobalt,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_vco_clk.c),
	},
};

static struct div_clk dsi0pll_bitclk_src = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 15,
	},
	.ops = &clk_bitclk_src_ops,
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_bitclk_src",
		.ops = &clk_ops_bitclk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_bitclk_src.c),
	}
};

static struct div_clk dsi0pll_post_vco_div = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 4,
	},
	.ops = &clk_post_vco_div_ops,
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_vco_div",
		.ops = &clk_ops_post_vco_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_vco_div.c),
	}
};

static struct div_clk dsi0pll_post_bit_div = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 2,
	},
	.ops = &clk_post_bit_div_ops,
	.c = {
		.parent = &dsi0pll_bitclk_src.c,
		.dbg_name = "dsi0pll_post_bit_div",
		.ops = &clk_ops_post_bit_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_bit_div.c),
	}
};

static struct mux_clk dsi0pll_pclk_src_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi0pll_post_bit_div.c, 0},
		{&dsi0pll_post_vco_div.c, 1},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi0pll_post_bit_div.c,
		.dbg_name = "dsi0pll_pclk_src_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pclk_src_mux.c),
	}
};

static struct div_clk dsi0pll_pclk_src = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 15,
	},
	.ops = &pixel_clk_div_ops,
	.c = {
		.parent = &dsi0pll_pclk_src_mux.c,
		.dbg_name = "dsi0pll_pclk_src",
		.ops = &clk_ops_pclk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pclk_src.c),
	},
};

static struct mux_clk dsi0pll_pclk_mux = {
	.num_parents = 1,
	.parents = (struct clk_src[]) {
		{&dsi0pll_pclk_src.c, 0},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi0pll_pclk_src.c,
		.dbg_name = "dsi0pll_pclk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pclk_mux.c),
	}
};

static struct div_clk dsi0pll_byteclk_src = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi0pll_bitclk_src.c,
		.dbg_name = "dsi0pll_byteclk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_byteclk_src.c),
	},
};

static struct mux_clk dsi0pll_byteclk_mux = {
	.num_parents = 1,
	.parents = (struct clk_src[]) {
		{&dsi0pll_byteclk_src.c, 0},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi0pll_byteclk_src.c,
		.dbg_name = "dsi0pll_byteclk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_byteclk_mux.c),
	}
};

static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1500000000UL,
	.max_rate = 3500000000UL,
	.c = {
		.dbg_name = "dsi1pll_vco_clk",
		.ops = &clk_ops_vco_cobalt,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_vco_clk.c),
	},
};

static struct div_clk dsi1pll_bitclk_src = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 15,
	},
	.ops = &clk_bitclk_src_ops,
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_bitclk_src",
		.ops = &clk_ops_bitclk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_bitclk_src.c),
	}
};

static struct div_clk dsi1pll_post_vco_div = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 4,
	},
	.ops = &clk_post_vco_div_ops,
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_vco_div",
		.ops = &clk_ops_post_vco_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_vco_div.c),
	}
};

static struct div_clk dsi1pll_post_bit_div = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 2,
	},
	.ops = &clk_post_bit_div_ops,
	.c = {
		.parent = &dsi1pll_bitclk_src.c,
		.dbg_name = "dsi1pll_post_bit_div",
		.ops = &clk_ops_post_bit_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_bit_div.c),
	}
};

static struct mux_clk dsi1pll_pclk_src_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi1pll_post_bit_div.c, 0},
		{&dsi1pll_post_vco_div.c, 1},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi1pll_post_bit_div.c,
		.dbg_name = "dsi1pll_pclk_src_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pclk_src_mux.c),
	}
};

static struct div_clk dsi1pll_pclk_src = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 15,
	},
	.ops = &pixel_clk_div_ops,
	.c = {
		.parent = &dsi1pll_pclk_src_mux.c,
		.dbg_name = "dsi1pll_pclk_src",
		.ops = &clk_ops_pclk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pclk_src.c),
	},
};

static struct mux_clk dsi1pll_pclk_mux = {
	.num_parents = 1,
	.parents = (struct clk_src[]) {
		{&dsi1pll_pclk_src.c, 0},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi1pll_pclk_src.c,
		.dbg_name = "dsi1pll_pclk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pclk_mux.c),
	}
};

static struct div_clk dsi1pll_byteclk_src = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi1pll_bitclk_src.c,
		.dbg_name = "dsi1pll_byteclk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_byteclk_src.c),
	},
};

static struct mux_clk dsi1pll_byteclk_mux = {
	.num_parents = 1,
	.parents = (struct clk_src[]) {
		{&dsi1pll_byteclk_src.c, 0},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi1pll_byteclk_src.c,
		.dbg_name = "dsi1pll_byteclk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_byteclk_mux.c),
	}
};

static struct clk_lookup mdss_dsi_pll0cc_cobalt[] = {
	CLK_LIST(dsi0pll_byteclk_mux),
	CLK_LIST(dsi0pll_byteclk_src),
	CLK_LIST(dsi0pll_pclk_mux),
	CLK_LIST(dsi0pll_pclk_src),
	CLK_LIST(dsi0pll_pclk_src_mux),
	CLK_LIST(dsi0pll_post_bit_div),
	CLK_LIST(dsi0pll_post_vco_div),
	CLK_LIST(dsi0pll_bitclk_src),
	CLK_LIST(dsi0pll_vco_clk),
};
static struct clk_lookup mdss_dsi_pll1cc_cobalt[] = {
	CLK_LIST(dsi1pll_byteclk_mux),
	CLK_LIST(dsi1pll_byteclk_src),
	CLK_LIST(dsi1pll_pclk_mux),
	CLK_LIST(dsi1pll_pclk_src),
	CLK_LIST(dsi1pll_pclk_src_mux),
	CLK_LIST(dsi1pll_post_bit_div),
	CLK_LIST(dsi1pll_post_vco_div),
	CLK_LIST(dsi1pll_bitclk_src),
	CLK_LIST(dsi1pll_vco_clk),
};

int dsi_pll_clock_register_cobalt(struct platform_device *pdev,
				  struct mdss_pll_resources *pll_res)
{
	int rc = 0, ndx;

	if (!pdev || !pdev->dev.of_node ||
		!pll_res || !pll_res->pll_base || !pll_res->phy_base) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ndx = pll_res->index;

	if (ndx >= DSI_PLL_MAX) {
		pr_err("pll index(%d) NOT supported\n", ndx);
		return -EINVAL;
	}

	pll_rsc_db[ndx] = pll_res;
	pll_res->priv = &plls[ndx];
	plls[ndx].rsc = pll_res;

	/* runtime fixup of all div and mux clock ops */
	clk_ops_gen_mux_dsi = clk_ops_gen_mux;
	clk_ops_gen_mux_dsi.round_rate = parent_round_rate;
	clk_ops_gen_mux_dsi.set_rate = parent_set_rate;

	clk_ops_bitclk_src_c = clk_ops_div;
	clk_ops_bitclk_src_c.prepare = mdss_pll_div_prepare;

	/*
	 * Set the ops for the two dividers in the pixel clock tree to the
	 * slave_div to ensure that a set rate on this divider clock will not
	 * be propagated to it's parent. This is needed ensure that when we set
	 * the rate for pixel clock, the vco is not reconfigured
	 */
	clk_ops_post_vco_div_c = clk_ops_slave_div;
	clk_ops_post_vco_div_c.prepare = mdss_pll_div_prepare;

	clk_ops_post_bit_div_c = clk_ops_slave_div;
	clk_ops_post_bit_div_c.prepare = mdss_pll_div_prepare;

	clk_ops_pclk_src_c = clk_ops_div;
	clk_ops_pclk_src_c.prepare = mdss_pll_div_prepare;

	pll_res->vco_delay = VCO_DELAY_USEC;
	if (ndx == 0) {
		dsi0pll_byteclk_mux.priv = pll_res;
		dsi0pll_byteclk_src.priv = pll_res;
		dsi0pll_pclk_mux.priv = pll_res;
		dsi0pll_pclk_src.priv = pll_res;
		dsi0pll_pclk_src_mux.priv = pll_res;
		dsi0pll_post_bit_div.priv = pll_res;
		dsi0pll_post_vco_div.priv = pll_res;
		dsi0pll_bitclk_src.priv = pll_res;
		dsi0pll_vco_clk.priv = pll_res;

		rc = of_msm_clock_register(pdev->dev.of_node,
			mdss_dsi_pll0cc_cobalt,
			ARRAY_SIZE(mdss_dsi_pll0cc_cobalt));
	} else {
		dsi1pll_byteclk_mux.priv = pll_res;
		dsi1pll_byteclk_src.priv = pll_res;
		dsi1pll_pclk_mux.priv = pll_res;
		dsi1pll_pclk_src.priv = pll_res;
		dsi1pll_pclk_src_mux.priv = pll_res;
		dsi1pll_post_bit_div.priv = pll_res;
		dsi1pll_post_vco_div.priv = pll_res;
		dsi1pll_bitclk_src.priv = pll_res;
		dsi1pll_vco_clk.priv = pll_res;

		rc = of_msm_clock_register(pdev->dev.of_node,
			mdss_dsi_pll1cc_cobalt,
			ARRAY_SIZE(mdss_dsi_pll1cc_cobalt));
	}
	if (rc)
		pr_err("dsi%dpll clock register failed, rc=%d\n", ndx, rc);

	return rc;
}
