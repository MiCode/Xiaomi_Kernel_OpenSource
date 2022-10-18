// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "dsi_pll_5nm.h"

#define VCO_DELAY_USEC 1

#define MHZ_250		250000000UL
#define MHZ_500		500000000UL
#define MHZ_1000	1000000000UL
#define MHZ_1100	1100000000UL
#define MHZ_1900	1900000000UL
#define MHZ_3000	3000000000UL

struct dsi_pll_regs {
	u32 pll_prop_gain_rate;
	u32 pll_lockdet_rate;
	u32 decimal_div_start;
	u32 frac_div_start_low;
	u32 frac_div_start_mid;
	u32 frac_div_start_high;
	u32 pll_clock_inverters;
	u32 ssc_stepsize_low;
	u32 ssc_stepsize_high;
	u32 ssc_div_per_low;
	u32 ssc_div_per_high;
	u32 ssc_adjper_low;
	u32 ssc_adjper_high;
	u32 ssc_control;
};

struct dsi_pll_config {
	u32 ref_freq;
	bool div_override;
	u32 output_div;
	bool ignore_frac;
	bool disable_prescaler;
	bool enable_ssc;
	bool ssc_center;
	u32 dec_bits;
	u32 frac_bits;
	u32 lock_timer;
	u32 ssc_freq;
	u32 ssc_offset;
	u32 ssc_adj_per;
	u32 thresh_cycles;
	u32 refclk_cycles;
};

struct dsi_pll_5nm {
	struct dsi_pll_resource *rsc;
	struct dsi_pll_config pll_configuration;
	struct dsi_pll_regs reg_setup;
	bool cphy_enabled;
};

static inline bool dsi_pll_5nm_is_hw_revision(
		struct dsi_pll_resource *rsc)
{
	return (rsc->pll_revision == DSI_PLL_5NM) ?
		true : false;
}

static inline void dsi_pll_set_pll_post_div(struct dsi_pll_resource *pll, u32
		pll_post_div)
{
	u32 pll_post_div_val = 0;

	if (pll_post_div == 1)
		pll_post_div_val = 0;
	if (pll_post_div == 2)
		pll_post_div_val = 1;
	if (pll_post_div == 4)
		pll_post_div_val = 2;
	if (pll_post_div == 8)
		pll_post_div_val = 3;

	DSI_PLL_REG_W(pll->pll_base, PLL_PLL_OUTDIV_RATE, pll_post_div_val);
	if (pll->slave)
		DSI_PLL_REG_W(pll->slave->pll_base, PLL_PLL_OUTDIV_RATE,
				pll_post_div_val);
}

static inline int dsi_pll_get_pll_post_div(struct dsi_pll_resource *pll)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->pll_base, PLL_PLL_OUTDIV_RATE);

	return (1 << reg_val);
}

static inline void dsi_pll_set_phy_post_div(struct dsi_pll_resource *pll, u32
		phy_post_div)
{
	u32 reg_val = 0;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	reg_val &= ~0x0F;
	reg_val |= phy_post_div;
	DSI_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG0, reg_val);
	/* For slave PLL, this divider always should be set to 1 */
	if (pll->slave) {
		reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
		reg_val &= ~0x0F;
		reg_val |= 0x1;
		DSI_PLL_REG_W(pll->slave->phy_base, PHY_CMN_CLK_CFG0, reg_val);
	}
}


static inline int dsi_pll_get_phy_post_div(struct dsi_pll_resource *pll)
{
	u32 reg_val = 0;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);

	return (reg_val & 0xF);
}


static inline void dsi_pll_set_dsi_clk(struct dsi_pll_resource *pll, u32
		dsi_clk)
{
	u32 reg_val = 0;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG1);
	reg_val &= ~0x3;
	reg_val |= dsi_clk;
	DSI_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG1, reg_val);
	if (pll->slave) {
		reg_val = DSI_PLL_REG_R(pll->slave->phy_base, PHY_CMN_CLK_CFG1);
		reg_val &= ~0x3;
		reg_val |= dsi_clk;
		DSI_PLL_REG_W(pll->slave->phy_base, PHY_CMN_CLK_CFG1, reg_val);
	}
}

static inline int dsi_pll_get_dsi_clk(struct dsi_pll_resource *pll)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG1);

	return (reg_val & 0x3);
}

static inline void dsi_pll_set_pclk_div(struct dsi_pll_resource *pll, u32
		pclk_div)
{
	u32 reg_val = 0;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	reg_val &= ~0xF0;
	reg_val |= (pclk_div << 4);
	DSI_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG0, reg_val);
	if (pll->slave) {
		reg_val = DSI_PLL_REG_R(pll->slave->phy_base, PHY_CMN_CLK_CFG0);
		reg_val &= ~0xF0;
		reg_val |= (pclk_div << 4);
		DSI_PLL_REG_W(pll->slave->phy_base, PHY_CMN_CLK_CFG0, reg_val);
	}
}

static inline int dsi_pll_get_pclk_div(struct dsi_pll_resource *pll)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);

	return ((reg_val & 0xF0) >> 4);
}

static struct dsi_pll_resource *pll_rsc_db[DSI_PLL_MAX];
static struct dsi_pll_5nm plls[DSI_PLL_MAX];

static void dsi_pll_config_slave(struct dsi_pll_resource *rsc)
{
	u32 reg;
	struct dsi_pll_resource *orsc = pll_rsc_db[DSI_PLL_1];

	if (!rsc)
		return;

	/* Only DSI PLL0 can act as a master */
	if (rsc->index != DSI_PLL_0)
		return;

	/* default configuration: source is either internal or ref clock */
	rsc->slave = NULL;

	if (!orsc) {
		DSI_PLL_WARN(rsc,
			"slave PLL unavilable, assuming standalone config\n");
		return;
	}

	/* check to see if the source of DSI1 PLL bitclk is set to external */
	reg = DSI_PLL_REG_R(orsc->phy_base, PHY_CMN_CLK_CFG1);
	reg &= (BIT(2) | BIT(3));
	if (reg == 0x04)
		rsc->slave = pll_rsc_db[DSI_PLL_1]; /* external source */

	DSI_PLL_DBG(rsc, "Slave PLL %s\n",
			rsc->slave ? "configured" : "absent");
}

static void dsi_pll_setup_config(struct dsi_pll_5nm *pll,
				 struct dsi_pll_resource *rsc)
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
	config->enable_ssc = rsc->ssc_en;
	config->ssc_center = rsc->ssc_center;

	if (config->enable_ssc) {
		if (rsc->ssc_freq)
			config->ssc_freq = rsc->ssc_freq;
		if (rsc->ssc_ppm)
			config->ssc_offset = rsc->ssc_ppm;
	}
}

static void dsi_pll_calc_dec_frac(struct dsi_pll_5nm *pll,
				  struct dsi_pll_resource *rsc)
{
	struct dsi_pll_config *config = &pll->pll_configuration;
	struct dsi_pll_regs *regs = &pll->reg_setup;
	u64 fref = rsc->vco_ref_clk_rate;
	u64 pll_freq;
	u64 divider;
	u64 dec, dec_multiple;
	u32 frac;
	u64 multiplier;

	pll_freq = rsc->vco_current_rate;

	if (config->disable_prescaler)
		divider = fref;
	else
		divider = fref * 2;

	multiplier = 1 << config->frac_bits;
	dec_multiple = div_u64(pll_freq * multiplier, divider);
	div_u64_rem(dec_multiple, multiplier, &frac);

	dec = div_u64(dec_multiple, multiplier);

	switch (rsc->pll_revision) {
	case DSI_PLL_5NM:
	default:
		if (pll_freq <= 1000000000ULL)
			regs->pll_clock_inverters = 0xA0;
		else if (pll_freq <= 2500000000ULL)
			regs->pll_clock_inverters = 0x20;
		else if (pll_freq <= 3500000000ULL)
			regs->pll_clock_inverters = 0x00;
		else
			regs->pll_clock_inverters = 0x40;
		break;
	}

	regs->pll_lockdet_rate = config->lock_timer;
	regs->decimal_div_start = dec;
	regs->frac_div_start_low = (frac & 0xff);
	regs->frac_div_start_mid = (frac & 0xff00) >> 8;
	regs->frac_div_start_high = (frac & 0x30000) >> 16;
	regs->pll_prop_gain_rate = 10;
}

static void dsi_pll_calc_ssc(struct dsi_pll_5nm *pll,
		  struct dsi_pll_resource *rsc)
{
	struct dsi_pll_config *config = &pll->pll_configuration;
	struct dsi_pll_regs *regs = &pll->reg_setup;
	u32 ssc_per;
	u32 ssc_mod;
	u64 ssc_step_size;
	u64 frac;

	if (!config->enable_ssc) {
		DSI_PLL_DBG(rsc, "SSC not enabled\n");
		return;
	}

	ssc_per = DIV_ROUND_CLOSEST(config->ref_freq, config->ssc_freq) / 2 - 1;
	ssc_mod = (ssc_per + 1) % (config->ssc_adj_per + 1);
	ssc_per -= ssc_mod;

	frac = regs->frac_div_start_low |
			(regs->frac_div_start_mid << 8) |
			(regs->frac_div_start_high << 16);
	ssc_step_size = regs->decimal_div_start;
	ssc_step_size *= (1 << config->frac_bits);
	ssc_step_size += frac;
	ssc_step_size *= config->ssc_offset;
	ssc_step_size *= (config->ssc_adj_per + 1);
	ssc_step_size = div_u64(ssc_step_size, (ssc_per + 1));
	ssc_step_size = DIV_ROUND_CLOSEST_ULL(ssc_step_size, 1000000);

	regs->ssc_div_per_low = ssc_per & 0xFF;
	regs->ssc_div_per_high = (ssc_per & 0xFF00) >> 8;
	regs->ssc_stepsize_low = (u32)(ssc_step_size & 0xFF);
	regs->ssc_stepsize_high = (u32)((ssc_step_size & 0xFF00) >> 8);
	regs->ssc_adjper_low = config->ssc_adj_per & 0xFF;
	regs->ssc_adjper_high = (config->ssc_adj_per & 0xFF00) >> 8;

	regs->ssc_control = config->ssc_center ? SSC_CENTER : 0;

	DSI_PLL_DBG(rsc, "SCC: Dec:%d, frac:%llu, frac_bits:%d\n",
			regs->decimal_div_start, frac, config->frac_bits);
	DSI_PLL_DBG(rsc, "SSC: div_per:0x%X, stepsize:0x%X, adjper:0x%X\n",
			ssc_per, (u32)ssc_step_size, config->ssc_adj_per);
}

static void dsi_pll_ssc_commit(struct dsi_pll_5nm *pll,
		struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	struct dsi_pll_regs *regs = &pll->reg_setup;

	if (pll->pll_configuration.enable_ssc) {
		DSI_PLL_DBG(rsc, "SSC is enabled\n");

		DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_LOW_1,
				regs->ssc_stepsize_low);
		DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_HIGH_1,
				regs->ssc_stepsize_high);
		DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_LOW_1,
				regs->ssc_div_per_low);
		DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_HIGH_1,
				regs->ssc_div_per_high);
		DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_LOW_1,
				regs->ssc_adjper_low);
		DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_HIGH_1,
				regs->ssc_adjper_high);
		DSI_PLL_REG_W(pll_base, PLL_SSC_CONTROL,
				SSC_EN | regs->ssc_control);
	}
}

static void dsi_pll_config_hzindep_reg(struct dsi_pll_5nm *pll,
				  struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	u64 vco_rate = rsc->vco_current_rate;

	switch (rsc->pll_revision) {
	case DSI_PLL_5NM:
	default:
		if (vco_rate < 3100000000ULL)
			DSI_PLL_REG_W(pll_base,
					PLL_ANALOG_CONTROLS_FIVE_1, 0x01);
		else
			DSI_PLL_REG_W(pll_base,
					PLL_ANALOG_CONTROLS_FIVE_1, 0x03);

		if (vco_rate < 1520000000ULL)
			DSI_PLL_REG_W(pll_base, PLL_VCO_CONFIG_1, 0x08);
		else if (vco_rate < 2990000000ULL)
			DSI_PLL_REG_W(pll_base, PLL_VCO_CONFIG_1, 0x00);
		else
			DSI_PLL_REG_W(pll_base, PLL_VCO_CONFIG_1, 0x01);

		break;
	}

	DSI_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_FIVE, 0x01);
	DSI_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_TWO, 0x03);
	DSI_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_THREE, 0x00);
	DSI_PLL_REG_W(pll_base, PLL_DSM_DIVIDER, 0x00);
	DSI_PLL_REG_W(pll_base, PLL_FEEDBACK_DIVIDER, 0x4e);
	DSI_PLL_REG_W(pll_base, PLL_CALIBRATION_SETTINGS, 0x40);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS_THREE, 0xba);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_DETECT_SETTINGS_ONE, 0x0c);
	DSI_PLL_REG_W(pll_base, PLL_OUTDIV, 0x00);
	DSI_PLL_REG_W(pll_base, PLL_CORE_OVERRIDE, 0x00);
	DSI_PLL_REG_W(pll_base, PLL_PLL_DIGITAL_TIMERS_TWO, 0x08);
	DSI_PLL_REG_W(pll_base, PLL_PLL_PROP_GAIN_RATE_1, 0x0a);
	DSI_PLL_REG_W(pll_base, PLL_PLL_BAND_SEL_RATE_1, 0xc0);
	DSI_PLL_REG_W(pll_base, PLL_PLL_INT_GAIN_IFILT_BAND_1, 0x84);
	DSI_PLL_REG_W(pll_base, PLL_PLL_INT_GAIN_IFILT_BAND_1, 0x82);
	DSI_PLL_REG_W(pll_base, PLL_PLL_FL_INT_GAIN_PFILT_BAND_1, 0x4c);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCK_OVERRIDE, 0x80);
	DSI_PLL_REG_W(pll_base, PLL_PFILT, 0x29);
	DSI_PLL_REG_W(pll_base, PLL_PFILT, 0x2f);
	DSI_PLL_REG_W(pll_base, PLL_IFILT, 0x2a);

	switch (rsc->pll_revision) {
	case DSI_PLL_5NM:
	default:
		DSI_PLL_REG_W(pll_base, PLL_IFILT, 0x3F);
		break;
	}

	DSI_PLL_REG_W(pll_base, PLL_PERF_OPTIMIZE, 0x22);
	if (rsc->slave)
		DSI_PLL_REG_W(rsc->slave->pll_base, PLL_PERF_OPTIMIZE, 0x22);
}

static void dsi_pll_init_val(struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;

	DSI_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_ONE, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_INT_LOOP_SETTINGS, 0x0000003F);
	DSI_PLL_REG_W(pll_base, PLL_INT_LOOP_SETTINGS_TWO, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_FOUR, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_INT_LOOP_CONTROLS, 0x00000080);
	DSI_PLL_REG_W(pll_base, PLL_SYSTEM_MUXES, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_UPDATE_CONTROL_OVERRIDES, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_CMODE, 0x00000010);
	DSI_PLL_REG_W(pll_base, PLL_PSM_CTRL, 0x00000020);
	DSI_PLL_REG_W(pll_base, PLL_RSM_CTRL, 0x00000010);
	DSI_PLL_REG_W(pll_base, PLL_VCO_TUNE_MAP, 0x00000002);
	DSI_PLL_REG_W(pll_base, PLL_PLL_CNTRL, 0x0000001C);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_TIMER_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_TIMER_HIGH, 0x00000002);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS, 0x00000020);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_MIN, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_MAX, 0x000000FF);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_PFILT, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_IFILT, 0x0000000A);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS_TWO, 0x00000025);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS_THREE, 0x000000BA);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS_FOUR, 0x0000004F);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_ICODE_HIGH, 0x0000000A);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_ICODE_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_DETECT_SETTINGS_ONE, 0x0000000C);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_DETECT_THRESH, 0x00000020);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_DET_REFCLK_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_DET_REFCLK_LOW, 0x000000FF);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_DET_PLLCLK_HIGH, 0x00000010);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_DET_PLLCLK_LOW, 0x00000046);
	DSI_PLL_REG_W(pll_base, PLL_PLL_GAIN, 0x00000054);
	DSI_PLL_REG_W(pll_base, PLL_ICODE_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_ICODE_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_LOCKDET, 0x00000040);
	DSI_PLL_REG_W(pll_base, PLL_FASTLOCK_CONTROL, 0x00000004);
	DSI_PLL_REG_W(pll_base, PLL_PASS_OUT_OVERRIDE_ONE, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PASS_OUT_OVERRIDE_TWO, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_CORE_OVERRIDE, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_CORE_INPUT_OVERRIDE, 0x00000010);
	DSI_PLL_REG_W(pll_base, PLL_RATE_CHANGE, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PLL_DIGITAL_TIMERS, 0x00000008);
	DSI_PLL_REG_W(pll_base, PLL_PLL_DIGITAL_TIMERS_TWO, 0x00000008);
	DSI_PLL_REG_W(pll_base, PLL_DEC_FRAC_MUXES, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_MASH_CONTROL, 0x00000003);
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_MUX_CONTROL, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_LOW_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_HIGH_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_LOW_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_HIGH_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_LOW_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_HIGH_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_LOW_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_HIGH_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_LOW_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_HIGH_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_LOW_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_ADJPER_HIGH_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SSC_CONTROL, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PLL_OUTDIV_RATE, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCKDET_RATE_1, 0x00000040);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCKDET_RATE_2, 0x00000040);
	DSI_PLL_REG_W(pll_base, PLL_PLL_PROP_GAIN_RATE_1, 0x0000000C);
	DSI_PLL_REG_W(pll_base, PLL_PLL_PROP_GAIN_RATE_2, 0x0000000A);
	DSI_PLL_REG_W(pll_base, PLL_PLL_BAND_SEL_RATE_1, 0x000000C0);
	DSI_PLL_REG_W(pll_base, PLL_PLL_BAND_SEL_RATE_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PLL_INT_GAIN_IFILT_BAND_1, 0x00000054);
	DSI_PLL_REG_W(pll_base, PLL_PLL_INT_GAIN_IFILT_BAND_2, 0x00000054);
	DSI_PLL_REG_W(pll_base, PLL_PLL_FL_INT_GAIN_PFILT_BAND_1, 0x0000004C);
	DSI_PLL_REG_W(pll_base, PLL_PLL_FL_INT_GAIN_PFILT_BAND_2, 0x0000004C);
	DSI_PLL_REG_W(pll_base, PLL_PLL_FASTLOCK_EN_BAND, 0x00000003);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_TUNE_ACCUM_INIT_MID, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_TUNE_ACCUM_INIT_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FREQ_TUNE_ACCUM_INIT_MUX, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCK_OVERRIDE, 0x00000080);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCK_DELAY, 0x00000006);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCK_MIN_DELAY, 0x00000019);
	DSI_PLL_REG_W(pll_base, PLL_CLOCK_INVERTERS, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SPARE_AND_JPC_OVERRIDES, 0x00000000);

	DSI_PLL_REG_W(pll_base, PLL_BIAS_CONTROL_1, 0x00000040);

	DSI_PLL_REG_W(pll_base, PLL_BIAS_CONTROL_2, 0x00000020);
	DSI_PLL_REG_W(pll_base, PLL_ALOG_OBSV_BUS_CTRL_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_COMMON_STATUS_ONE, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_COMMON_STATUS_TWO, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_ICODE_ACCUM_STATUS_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_ICODE_ACCUM_STATUS_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FD_OUT_LOW, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FD_OUT_HIGH, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_ALOG_OBSV_BUS_STATUS_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PLL_MISC_CONFIG, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FLL_CONFIG, 0x00000002);
	DSI_PLL_REG_W(pll_base, PLL_FLL_FREQ_ACQ_TIME, 0x00000011);
	DSI_PLL_REG_W(pll_base, PLL_FLL_CODE0, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FLL_CODE1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FLL_GAIN0, 0x00000080);
	DSI_PLL_REG_W(pll_base, PLL_FLL_GAIN1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_SW_RESET, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_FAST_PWRUP, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_LOCKTIME0, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_LOCKTIME1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_DEBUG_BUS_SEL, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_DEBUG_BUS0, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_DEBUG_BUS1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_DEBUG_BUS2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_DEBUG_BUS3, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_ANALOG_FLL_CONTROL_OVERRIDES, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_VCO_CONFIG, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_VCO_CAL_CODE1_MODE0_STATUS, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_VCO_CAL_CODE1_MODE1_STATUS, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_RESET_SM_STATUS, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_TDC_OFFSET, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_PS3_PWRDOWN_CONTROLS, 0x0000001D);
	DSI_PLL_REG_W(pll_base, PLL_PS4_PWRDOWN_CONTROLS, 0x0000001C);
	DSI_PLL_REG_W(pll_base, PLL_PLL_RST_CONTROLS, 0x000000FF);
	DSI_PLL_REG_W(pll_base, PLL_GEAR_BAND_SELECT_CONTROLS, 0x00000022);
	DSI_PLL_REG_W(pll_base, PLL_PSM_CLK_CONTROLS, 0x00000009);
	DSI_PLL_REG_W(pll_base, PLL_SYSTEM_MUXES_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_VCO_CONFIG_1, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_VCO_CONFIG_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_CLOCK_INVERTERS_1, 0x00000040);
	DSI_PLL_REG_W(pll_base, PLL_CLOCK_INVERTERS_2, 0x00000000);
	DSI_PLL_REG_W(pll_base, PLL_CMODE_1, 0x00000010);
	DSI_PLL_REG_W(pll_base, PLL_CMODE_2, 0x00000010);
	DSI_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_FIVE_2, 0x00000003);

}

static void dsi_pll_detect_phy_mode(struct dsi_pll_5nm *pll,
				    struct dsi_pll_resource *rsc)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_GLBL_CTRL);
	pll->cphy_enabled = (reg_val & BIT(6)) ? true : false;
}

static void dsi_pll_commit(struct dsi_pll_5nm *pll,
			   struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	struct dsi_pll_regs *reg = &pll->reg_setup;

	DSI_PLL_REG_W(pll_base, PLL_CORE_INPUT_OVERRIDE, 0x12);
	DSI_PLL_REG_W(pll_base, PLL_DECIMAL_DIV_START_1,
		       reg->decimal_div_start);
	DSI_PLL_REG_W(pll_base, PLL_FRAC_DIV_START_LOW_1,
		       reg->frac_div_start_low);
	DSI_PLL_REG_W(pll_base, PLL_FRAC_DIV_START_MID_1,
		       reg->frac_div_start_mid);
	DSI_PLL_REG_W(pll_base, PLL_FRAC_DIV_START_HIGH_1,
		       reg->frac_div_start_high);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCKDET_RATE_1, reg->pll_lockdet_rate);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCK_DELAY, 0x06);
	DSI_PLL_REG_W(pll_base, PLL_CMODE_1,
			pll->cphy_enabled ? 0x00 : 0x10);
	DSI_PLL_REG_W(pll_base, PLL_CLOCK_INVERTERS_1,
			reg->pll_clock_inverters);
}

static int dsi_pll_5nm_lock_status(struct dsi_pll_resource *pll)
{
	int rc;
	u32 status;
	u32 const delay_us = 100;
	u32 const timeout_us = 5000;

	rc = DSI_READ_POLL_TIMEOUT_ATOMIC_GEN(pll->pll_base, pll->index, PLL_COMMON_STATUS_ONE,
				       status,
				       ((status & BIT(0)) > 0),
				       delay_us,
				       timeout_us);
	if (rc)
		DSI_PLL_ERR(pll, "lock failed, status=0x%08x\n", status);

	return rc;
}

static void dsi_pll_disable_pll_bias(struct dsi_pll_resource *rsc)
{
	u32 data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CTRL_0);

	DSI_PLL_REG_W(rsc->pll_base, PLL_SYSTEM_MUXES, 0);
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data & ~BIT(5));
	ndelay(250);
}

static void dsi_pll_enable_pll_bias(struct dsi_pll_resource *rsc)
{
	u32 data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CTRL_0);

	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data | BIT(5));
	DSI_PLL_REG_W(rsc->pll_base, PLL_SYSTEM_MUXES, 0xc0);
	ndelay(250);
}

static void dsi_pll_disable_global_clk(struct dsi_pll_resource *rsc)
{
	u32 data;

	data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG1);
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CLK_CFG1, (data & ~BIT(5)));
}

static void dsi_pll_enable_global_clk(struct dsi_pll_resource *rsc)
{
	u32 data;

	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_3, 0x04);

	data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG1);

	/* Turn on clk_en_sel bit prior to resync toggle fifo */
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CLK_CFG1, (data | BIT(5) |
								BIT(4)));
}

static void dsi_pll_phy_dig_reset(struct dsi_pll_resource *rsc)
{
	/*
	 * Reset the PHY digital domain. This would be needed when
	 * coming out of a CX or analog rail power collapse while
	 * ensuring that the pads maintain LP00 or LP11 state
	 */
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_GLBL_DIGTOP_SPARE4, BIT(0));
	wmb(); /* Ensure that the reset is asserted */
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_GLBL_DIGTOP_SPARE4, 0x0);
	wmb(); /* Ensure that the reset is deasserted */
}

static void dsi_pll_disable_sub(struct dsi_pll_resource *rsc)
{
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_RBUF_CTRL, 0);
	dsi_pll_disable_pll_bias(rsc);
}

static void dsi_pll_unprepare_stub(struct clk_hw *hw)
{
	return;
}

static int dsi_pll_prepare_stub(struct clk_hw *hw)
{
	return 0;
}

static int dsi_pll_set_rate_stub(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	return 0;
}

static long dsi_pll_byteclk_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate)
{
	struct dsi_pll_clk *pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll_res = pll->priv;

	return pll_res->byteclk_rate;
}

static long dsi_pll_pclk_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *parent_rate)
{
	struct dsi_pll_clk *pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll_res = pll->priv;

	return pll_res->pclk_rate;
}

static unsigned long dsi_pll_vco_recalc_rate(struct dsi_pll_resource *pll)
{
	u64 ref_clk;
	u64 multiplier;
	u32 frac;
	u32 dec;
	u32 pll_post_div;
	u64 pll_freq, tmp64;
	u64 vco_rate;
	struct dsi_pll_5nm *pll_5nm;
	struct dsi_pll_config *config;

	ref_clk = pll->vco_ref_clk_rate;
	pll_5nm = pll->priv;
	if (!pll_5nm) {
		DSI_PLL_ERR(pll, "pll configuration not found\n");
		return -EINVAL;
	}

	config = &pll_5nm->pll_configuration;

	dec = DSI_PLL_REG_R(pll->pll_base, PLL_DECIMAL_DIV_START_1);
	dec &= 0xFF;

	frac = DSI_PLL_REG_R(pll->pll_base, PLL_FRAC_DIV_START_LOW_1);
	frac |= ((DSI_PLL_REG_R(pll->pll_base, PLL_FRAC_DIV_START_MID_1) & 0xFF)
					<< 8);
	frac |= ((DSI_PLL_REG_R(pll->pll_base, PLL_FRAC_DIV_START_HIGH_1) & 0x3)
					<< 16);

	multiplier = 1 << config->frac_bits;
	pll_freq = dec * (ref_clk * 2);
	tmp64 = (ref_clk * 2 * frac);
	pll_freq += div_u64(tmp64, multiplier);

	pll_post_div = dsi_pll_get_pll_post_div(pll);

	vco_rate = div_u64(pll_freq, pll_post_div);

	return vco_rate;
}

static unsigned long dsi_pll_byteclk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct dsi_pll_clk *byte_pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll = NULL;
	u64 vco_rate = 0;
	u64 byte_rate = 0;
	u32 phy_post_div;

	if (!byte_pll->priv) {
		DSI_PLL_INFO(pll, "pll priv is null\n");
		return 0;
	}

	pll = byte_pll->priv;

	/*
	 * In the case when byteclk rate is set, the recalculation function
	 * should  return the current rate. Recalc rate is also called during
	 * clock registration, during which the function should reverse
	 * calculate clock rates that were set as part of UEFI.
	 */
	if (pll->byteclk_rate != 0) {
		DSI_PLL_DBG(pll, "returning byte clk rate = %lld %lld\n",
				pll->byteclk_rate, parent_rate);
		return  pll->byteclk_rate;
	}

	vco_rate = dsi_pll_vco_recalc_rate(pll);

	phy_post_div = dsi_pll_get_phy_post_div(pll);
	byte_rate = div_u64(vco_rate, phy_post_div);

	if (pll->type == DSI_PHY_TYPE_DPHY)
		byte_rate = div_u64(byte_rate, 8);
	else
		byte_rate = div_u64(byte_rate, 7);

	return byte_rate;
}

static unsigned long dsi_pll_pclk_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct dsi_pll_clk *pix_pll = to_pll_clk_hw(hw);
	struct dsi_pll_resource *pll = NULL;
	u64 vco_rate = 0;
	u64 pclk_rate = 0;
	u32 phy_post_div, pclk_div;

	if (!pix_pll->priv) {
		DSI_PLL_INFO(pll, "pll priv is null\n");
		return 0;
	}

	pll = pix_pll->priv;

	/*
	 * In the case when pclk rate is set, the recalculation function
	 * should  return the current rate. Recalc rate is also called during
	 * clock registration, during which the function should reverse
	 * calculate the clock rates that were set as part of UEFI.
	 */
	if (pll->pclk_rate != 0) {
		DSI_PLL_DBG(pll, "returning pclk rate = %lld %lld\n",
				pll->pclk_rate, parent_rate);
		return pll->pclk_rate;
	}

	vco_rate = dsi_pll_vco_recalc_rate(pll);

	if (pll->type == DSI_PHY_TYPE_DPHY) {
		phy_post_div = dsi_pll_get_phy_post_div(pll);
		pclk_rate = div_u64(vco_rate, phy_post_div);
		pclk_rate = div_u64(pclk_rate, 2);
		pclk_div = dsi_pll_get_pclk_div(pll);
		pclk_rate = div_u64(pclk_rate, pclk_div);
	} else {
		pclk_rate = vco_rate * 2;
		pclk_rate = div_u64(pclk_rate, 7);
		pclk_div = dsi_pll_get_pclk_div(pll);
		pclk_rate = div_u64(pclk_rate, pclk_div);
	}

	return pclk_rate;
}

static const struct clk_ops pll_byteclk_ops = {
	.recalc_rate = dsi_pll_byteclk_recalc_rate,
	.set_rate = dsi_pll_set_rate_stub,
	.round_rate = dsi_pll_byteclk_round_rate,
	.prepare = dsi_pll_prepare_stub,
	.unprepare = dsi_pll_unprepare_stub,
};

static const struct clk_ops pll_pclk_ops = {
	.recalc_rate = dsi_pll_pclk_recalc_rate,
	.set_rate = dsi_pll_set_rate_stub,
	.round_rate = dsi_pll_pclk_round_rate,
	.prepare = dsi_pll_prepare_stub,
	.unprepare = dsi_pll_unprepare_stub,
};

/*
 * Clock tree for generating DSI byte and pclk.
 *
 *
 *  +-------------------------------+		+----------------------------+
 *  |    dsi_phy_pll_out_byteclk    |		|    dsi_phy_pll_out_dsiclk  |
 *  +---------------+---------------+		+--------------+-------------+
 *                  |                                          |
 *                  |                                          |
 *                  v                                          v
 *            dsi_byte_clk                                  dsi_pclk
 *
 *
 */

static struct dsi_pll_clk dsi0_phy_pll_out_byteclk = {
	.hw.init = &(struct clk_init_data){
			.name = "dsi0_phy_pll_out_byteclk",
			.ops = &pll_byteclk_ops,
	},
};

static struct dsi_pll_clk dsi1_phy_pll_out_byteclk = {
	.hw.init = &(struct clk_init_data){
			.name = "dsi1_phy_pll_out_byteclk",
			.ops = &pll_byteclk_ops,
	},
};

static struct dsi_pll_clk dsi0_phy_pll_out_dsiclk = {
	.hw.init = &(struct clk_init_data){
			.name = "dsi0_phy_pll_out_dsiclk",
			.ops = &pll_pclk_ops,
	},
};

static struct dsi_pll_clk dsi1_phy_pll_out_dsiclk = {
	.hw.init = &(struct clk_init_data){
			.name = "dsi1_phy_pll_out_dsiclk",
			.ops = &pll_pclk_ops,
	},
};

int dsi_pll_clock_register_5nm(struct platform_device *pdev,
				  struct dsi_pll_resource *pll_res)
{
	int rc = 0, ndx;
	struct clk *clk;
	struct clk_onecell_data *clk_data;
	int num_clks = 4;

	if (!pdev || !pdev->dev.of_node ||
			!pll_res || !pll_res->pll_base || !pll_res->phy_base) {
		DSI_PLL_ERR(pll_res, "Invalid params\n");
		return -EINVAL;
	}

	ndx = pll_res->index;

	if (ndx >= DSI_PLL_MAX) {
		DSI_PLL_ERR(pll_res, "not supported\n");
		return -EINVAL;
	}

	pll_rsc_db[ndx] = pll_res;
	plls[ndx].rsc = pll_res;
	pll_res->priv = &plls[ndx];
	pll_res->vco_delay = VCO_DELAY_USEC;
	pll_res->vco_min_rate = 600000000;
	pll_res->vco_ref_clk_rate = 19200000UL;

	dsi_pll_setup_config(pll_res->priv, pll_res);

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
					GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
				sizeof(struct clk *)), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = num_clks;

	/* Establish client data */
	if (ndx == 0) {
		dsi0_phy_pll_out_byteclk.priv = pll_res;
		dsi0_phy_pll_out_dsiclk.priv = pll_res;

		clk = devm_clk_register(&pdev->dev,
				&dsi0_phy_pll_out_byteclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
				"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}
		clk_data->clks[0] = clk;

		clk = devm_clk_register(&pdev->dev,
				&dsi0_phy_pll_out_dsiclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
				"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}
		clk_data->clks[1] = clk;


		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	} else {
		dsi1_phy_pll_out_byteclk.priv = pll_res;
		dsi1_phy_pll_out_dsiclk.priv = pll_res;


		clk = devm_clk_register(&pdev->dev,
				&dsi1_phy_pll_out_byteclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
				"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}
		clk_data->clks[2] = clk;

		clk = devm_clk_register(&pdev->dev,
				&dsi1_phy_pll_out_dsiclk.hw);
		if (IS_ERR(clk)) {
			DSI_PLL_ERR(pll_res,
				"clk registration failed for DSI clock\n");
			rc = -EINVAL;
			goto clk_register_fail;
		}
		clk_data->clks[3] = clk;

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	}
	if (!rc) {
		DSI_PLL_INFO(pll_res, "Registered clocks successfully\n");

		return rc;
	}
clk_register_fail:
	return rc;
}

static int dsi_pll_5nm_set_byteclk_div(struct dsi_pll_resource *pll,
		bool commit)
{

	int i = 0;
	int table_size;
	u32 pll_post_div = 0, phy_post_div = 0;
	struct dsi_pll_div_table *table;
	u64 bitclk_rate;
	u64 const phy_rate_split = 1500000000UL;

	if (pll->type == DSI_PHY_TYPE_DPHY) {
		bitclk_rate = pll->byteclk_rate * 8;

		if (bitclk_rate <= phy_rate_split) {
			table = pll_5nm_dphy_lb;
			table_size = ARRAY_SIZE(pll_5nm_dphy_lb);
		} else {
			table = pll_5nm_dphy_hb;
			table_size = ARRAY_SIZE(pll_5nm_dphy_hb);
		}
	} else {
		bitclk_rate = pll->byteclk_rate * 7;

		if (bitclk_rate <= phy_rate_split) {
			table = pll_5nm_cphy_lb;
			table_size = ARRAY_SIZE(pll_5nm_cphy_lb);
		} else {
			table = pll_5nm_cphy_hb;
			table_size = ARRAY_SIZE(pll_5nm_cphy_hb);
		}
	}

	for (i = 0; i < table_size; i++) {
		if ((table[i].min_hz <= bitclk_rate) &&
				(bitclk_rate <= table[i].max_hz)) {
			pll_post_div = table[i].pll_div;
			phy_post_div = table[i].phy_div;
			break;
		}
	}

	DSI_PLL_DBG(pll, "bit clk rate: %llu, pll_post_div: %d, phy_post_div: %d\n",
			bitclk_rate, pll_post_div, phy_post_div);

	if (commit) {
		dsi_pll_set_pll_post_div(pll, pll_post_div);
		dsi_pll_set_phy_post_div(pll, phy_post_div);
	}

	pll->vco_rate = bitclk_rate * pll_post_div * phy_post_div;

	return 0;
}

static int dsi_pll_calc_dphy_pclk_div(struct dsi_pll_resource *pll)
{
	u32 m_val, n_val; /* M and N values of MND trio */
	u32 pclk_div;

	if (pll->bpp == 30 && pll->lanes == 4) {
		/* RGB101010 */
		m_val = 2;
		n_val = 3;
	} else if (pll->bpp == 18 && pll->lanes == 2) {
		/* RGB666_packed */
		m_val = 2;
		n_val = 9;
	} else if (pll->bpp == 18 && pll->lanes == 4) {
		/* RGB666_packed */
		m_val = 4;
		n_val = 9;
	} else if (pll->bpp == 16 && pll->lanes == 3) {
		/* RGB565 */
		m_val = 3;
		n_val = 8;
	} else {
		m_val = 1;
		n_val = 1;
	}

	/* Calculating pclk_div assuming dsiclk_sel to be 1 */
	pclk_div = pll->bpp;
	pclk_div = mult_frac(pclk_div, m_val, n_val);
	do_div(pclk_div, 2);
	do_div(pclk_div, pll->lanes);

	DSI_PLL_DBG(pll, "bpp: %d, lanes: %d, m_val: %u, n_val: %u, pclk_div: %u\n",
                          pll->bpp, pll->lanes, m_val, n_val, pclk_div);

	return pclk_div;
}

static int dsi_pll_calc_cphy_pclk_div(struct dsi_pll_resource *pll)
{
	u32 m_val, n_val; /* M and N values of MND trio */
	u32 pclk_div;
	u32 phy_post_div = dsi_pll_get_phy_post_div(pll);

	if (pll->bpp == 24 && pll->lanes == 2) {
		/*
		 * RGB888 or DSC is enabled
		 * Skipping DSC enabled check
		 */
		m_val = 2;
		n_val = 3;
	} else if (pll->bpp == 30) {
		/* RGB101010 */
		if (pll->lanes == 1) {
			m_val = 4;
			n_val = 15;
		} else {
			m_val = 16;
			n_val = 35;
		}
	} else if (pll->bpp == 18) {
		/* RGB666_packed */
		if (pll->lanes == 1) {
			m_val = 8;
			n_val = 63;
		} else if (pll->lanes == 2) {
			m_val = 16;
			n_val = 63;
		} else if (pll->lanes == 3) {
			m_val = 8;
			n_val = 21;
		} else {
			m_val = 1;
			n_val = 1;
		}
	} else if (pll->bpp == 16 && pll->lanes == 3) {
		/* RGB565 */
		m_val = 3;
		n_val = 7;
	} else {
		m_val = 1;
		n_val = 1;
	}

	/* Calculating pclk_div assuming dsiclk_sel to be 3 */
	pclk_div =  pll->bpp * phy_post_div;
	pclk_div = mult_frac(pclk_div, m_val, n_val);
	do_div(pclk_div, 8);
	do_div(pclk_div, pll->lanes);

	DSI_PLL_DBG(pll, "bpp: %d, lanes: %d, m_val: %u, n_val: %u, phy_post_div: %u pclk_div: %u\n",
                          pll->bpp, pll->lanes, m_val, n_val, phy_post_div, pclk_div);

	return pclk_div;
}

static int dsi_pll_5nm_set_pclk_div(struct dsi_pll_resource *pll, bool commit)
{

	int dsi_clk = 0, pclk_div = 0;
	u64 pclk_src_rate;
	u32 pll_post_div;
	u32 phy_post_div;

	pll_post_div = dsi_pll_get_pll_post_div(pll);
	pclk_src_rate = div_u64(pll->vco_rate, pll_post_div);
	if (pll->type == DSI_PHY_TYPE_DPHY) {
		dsi_clk = 0x1;
		phy_post_div = dsi_pll_get_phy_post_div(pll);
		pclk_src_rate = div_u64(pclk_src_rate, phy_post_div);
		pclk_src_rate = div_u64(pclk_src_rate, 2);
		pclk_div = dsi_pll_calc_dphy_pclk_div(pll);
	} else {
		dsi_clk = 0x3;
		pclk_src_rate *= 2;
		pclk_src_rate = div_u64(pclk_src_rate, 7);
		pclk_div = dsi_pll_calc_cphy_pclk_div(pll);
	}

	pll->pclk_rate = div_u64(pclk_src_rate, pclk_div);

	DSI_PLL_DBG(pll, "pclk rate: %llu, dsi_clk: %d, pclk_div: %d\n",
			pll->pclk_rate, dsi_clk, pclk_div);

	if (commit) {
		dsi_pll_set_dsi_clk(pll, dsi_clk);
		dsi_pll_set_pclk_div(pll, pclk_div);
	}

	return 0;

}

static int dsi_pll_5nm_vco_set_rate(struct dsi_pll_resource *pll_res)
{
	struct dsi_pll_5nm *pll;

	pll = pll_res->priv;
	if (!pll) {
		DSI_PLL_ERR(pll_res, "pll configuration not found\n");
		return -EINVAL;
	}

	DSI_PLL_DBG(pll_res, "rate=%lu\n", pll_res->vco_rate);

	pll_res->vco_current_rate = pll_res->vco_rate;

	dsi_pll_detect_phy_mode(pll, pll_res);

	dsi_pll_calc_dec_frac(pll, pll_res);

	dsi_pll_calc_ssc(pll, pll_res);

	dsi_pll_commit(pll, pll_res);

	dsi_pll_config_hzindep_reg(pll, pll_res);

	dsi_pll_ssc_commit(pll, pll_res);

	/* flush, ensure all register writes are done*/
	wmb();

	return 0;
}

static int dsi_pll_read_stored_trim_codes(struct dsi_pll_resource *pll_res,
					  unsigned long vco_clk_rate)
{
	int i;
	bool found = false;

	if (!pll_res->dfps)
		return -EINVAL;

	for (i = 0; i < pll_res->dfps->vco_rate_cnt; i++) {
		struct dfps_codes_info *codes_info =
			&pll_res->dfps->codes_dfps[i];

		DSI_PLL_DBG(pll_res, "valid=%d vco_rate=%d, code %d %d %d\n",
			codes_info->is_valid, codes_info->clk_rate,
			codes_info->pll_codes.pll_codes_1,
			codes_info->pll_codes.pll_codes_2,
			codes_info->pll_codes.pll_codes_3);

		if (vco_clk_rate != codes_info->clk_rate &&
				codes_info->is_valid)
			continue;

		pll_res->cache_pll_trim_codes[0] =
			codes_info->pll_codes.pll_codes_1;
		pll_res->cache_pll_trim_codes[1] =
			codes_info->pll_codes.pll_codes_2;
		pll_res->cache_pll_trim_codes[2] =
			codes_info->pll_codes.pll_codes_3;
		found = true;
		break;
	}

	if (!found)
		return -EINVAL;

	DSI_PLL_DBG(pll_res, "trim_code_0=0x%x trim_code_1=0x%x trim_code_2=0x%x\n",
			pll_res->cache_pll_trim_codes[0],
			pll_res->cache_pll_trim_codes[1],
			pll_res->cache_pll_trim_codes[2]);

	return 0;
}

static void dsi_pll_5nm_dynamic_refresh(struct dsi_pll_5nm *pll,
					struct dsi_pll_resource *rsc)
{
	u32 data;
	u32 offset = DSI_PHY_TO_PLL_OFFSET;
	u32 upper_addr = 0;
	u32 upper_addr2 = 0;
	struct dsi_pll_regs *reg = &pll->reg_setup;

	data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG1);
	data &= ~BIT(5);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL0,
			   PHY_CMN_CLK_CFG1, PHY_CMN_PLL_CNTRL, data, 0);
	upper_addr |= (upper_8_bit(PHY_CMN_CLK_CFG1) << 0);
	upper_addr |= (upper_8_bit(PHY_CMN_PLL_CNTRL) << 1);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL1,
			   PHY_CMN_RBUF_CTRL,
			   (PLL_CORE_INPUT_OVERRIDE + offset),
			   0, 0x12);
	upper_addr |= (upper_8_bit(PHY_CMN_RBUF_CTRL) << 2);
	upper_addr |= (upper_8_bit(PLL_CORE_INPUT_OVERRIDE + offset) << 3);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL2,
			   (PLL_DECIMAL_DIV_START_1 + offset),
			   (PLL_FRAC_DIV_START_LOW_1 + offset),
			   reg->decimal_div_start, reg->frac_div_start_low);
	upper_addr |= (upper_8_bit(PLL_DECIMAL_DIV_START_1 + offset) << 4);
	upper_addr |= (upper_8_bit(PLL_FRAC_DIV_START_LOW_1 + offset) << 5);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL3,
			   (PLL_FRAC_DIV_START_MID_1 + offset),
			   (PLL_FRAC_DIV_START_HIGH_1 + offset),
			   reg->frac_div_start_mid, reg->frac_div_start_high);
	upper_addr |= (upper_8_bit(PLL_FRAC_DIV_START_MID_1 + offset) << 6);
	upper_addr |= (upper_8_bit(PLL_FRAC_DIV_START_HIGH_1 + offset) << 7);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL4,
			   (PLL_SYSTEM_MUXES + offset),
			   (PLL_PLL_LOCKDET_RATE_1 + offset),
			   0xc0, 0x10);
	upper_addr |= (upper_8_bit(PLL_SYSTEM_MUXES + offset) << 8);
	upper_addr |= (upper_8_bit(PLL_PLL_LOCKDET_RATE_1 + offset) << 9);

	data = DSI_PLL_REG_R(rsc->pll_base, PLL_PLL_OUTDIV_RATE) & 0x03;
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL5,
			   (PLL_PLL_OUTDIV_RATE + offset),
			   (PLL_PLL_LOCK_DELAY + offset),
			   data, 0x06);

	upper_addr |= (upper_8_bit(PLL_PLL_OUTDIV_RATE + offset) << 10);
	upper_addr |= (upper_8_bit(PLL_PLL_LOCK_DELAY + offset) << 11);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL6,
			   (PLL_CMODE_1 + offset),
			   (PLL_CLOCK_INVERTERS_1 + offset),
			   pll->cphy_enabled ? 0x00 : 0x10,
			   reg->pll_clock_inverters);
	upper_addr |= (upper_8_bit(PLL_CMODE_1 + offset) << 12);
	upper_addr |= (upper_8_bit(PLL_CLOCK_INVERTERS_1 + offset) << 13);

	data = DSI_PLL_REG_R(rsc->pll_base, PLL_VCO_CONFIG_1);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL7,
			   (PLL_ANALOG_CONTROLS_FIVE_1 + offset),
			   (PLL_VCO_CONFIG_1 + offset),
			   0x01, data);
	upper_addr |= (upper_8_bit(PLL_ANALOG_CONTROLS_FIVE_1 + offset) << 14);
	upper_addr |= (upper_8_bit(PLL_VCO_CONFIG_1 + offset) << 15);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL8,
			   (PLL_ANALOG_CONTROLS_FIVE + offset),
			   (PLL_ANALOG_CONTROLS_TWO + offset), 0x01, 0x03);
	upper_addr |= (upper_8_bit(PLL_ANALOG_CONTROLS_FIVE + offset) << 16);
	upper_addr |= (upper_8_bit(PLL_ANALOG_CONTROLS_TWO + offset) << 17);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL9,
			   (PLL_ANALOG_CONTROLS_THREE + offset),
			   (PLL_DSM_DIVIDER + offset),
			   rsc->cache_pll_trim_codes[2], 0x00);
	upper_addr |= (upper_8_bit(PLL_ANALOG_CONTROLS_THREE + offset) << 18);
	upper_addr |= (upper_8_bit(PLL_DSM_DIVIDER + offset) << 19);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL10,
			   (PLL_FEEDBACK_DIVIDER + offset),
			   (PLL_CALIBRATION_SETTINGS + offset), 0x4E, 0x40);
	upper_addr |= (upper_8_bit(PLL_FEEDBACK_DIVIDER + offset) << 20);
	upper_addr |= (upper_8_bit(PLL_CALIBRATION_SETTINGS + offset) << 21);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL11,
			   (PLL_BAND_SEL_CAL_SETTINGS_THREE + offset),
			   (PLL_FREQ_DETECT_SETTINGS_ONE + offset), 0xBA, 0x0C);
	upper_addr |= (upper_8_bit(PLL_BAND_SEL_CAL_SETTINGS_THREE + offset)
		       << 22);
	upper_addr |= (upper_8_bit(PLL_FREQ_DETECT_SETTINGS_ONE + offset)
		       << 23);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL12,
			   (PLL_OUTDIV + offset),
			   (PLL_CORE_OVERRIDE + offset), 0, 0);
	upper_addr |= (upper_8_bit(PLL_OUTDIV + offset) << 24);
	upper_addr |= (upper_8_bit(PLL_CORE_OVERRIDE + offset) << 25);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL13,
			   (PLL_PLL_DIGITAL_TIMERS_TWO + offset),
			   (PLL_PLL_PROP_GAIN_RATE_1 + offset),
			    0x08, reg->pll_prop_gain_rate);
	upper_addr |= (upper_8_bit(PLL_PLL_DIGITAL_TIMERS_TWO + offset) << 26);
	upper_addr |= (upper_8_bit(PLL_PLL_PROP_GAIN_RATE_1 + offset) << 27);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL14,
			   (PLL_PLL_BAND_SEL_RATE_1 + offset),
			   (PLL_PLL_INT_GAIN_IFILT_BAND_1 + offset),
			    0xC0, 0x82);
	upper_addr |= (upper_8_bit(PLL_PLL_BAND_SEL_RATE_1 + offset) << 28);
	upper_addr |= (upper_8_bit(PLL_PLL_INT_GAIN_IFILT_BAND_1 + offset)
		       << 29);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL15,
			   (PLL_PLL_FL_INT_GAIN_PFILT_BAND_1 + offset),
			   (PLL_PLL_LOCK_OVERRIDE + offset),
			    0x4c, 0x80);
	upper_addr |= (upper_8_bit(PLL_PLL_FL_INT_GAIN_PFILT_BAND_1 + offset)
		       << 30);
	upper_addr |= (upper_8_bit(PLL_PLL_LOCK_OVERRIDE + offset) << 31);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL16,
			   (PLL_PFILT + offset),
			   (PLL_IFILT + offset),
			    0x29, 0x3f);
	upper_addr2 |= (upper_8_bit(PLL_PFILT + offset) << 0);
	upper_addr2 |= (upper_8_bit(PLL_IFILT + offset) << 1);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL17,
			   (PLL_SYSTEM_MUXES + offset),
			   (PLL_CALIBRATION_SETTINGS + offset),
			    0xe0, 0x44);
	upper_addr2 |= (upper_8_bit(PLL_BAND_SEL_CAL + offset) << 2);
	upper_addr2 |= (upper_8_bit(PLL_CALIBRATION_SETTINGS + offset) << 3);

	data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG0);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL18,
			   PHY_CMN_CTRL_2, PHY_CMN_CLK_CFG0, 0x40, data);

	if (rsc->slave)
		DSI_DYN_PLL_REG_W(rsc->slave->dyn_pll_base,
				   DSI_DYNAMIC_REFRESH_PLL_CTRL10,
				   PHY_CMN_CLK_CFG0, PHY_CMN_CTRL_0,
				   data, 0x7f);

	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL27,
			   PHY_CMN_PLL_CNTRL, PHY_CMN_PLL_CNTRL, 0x01, 0x01);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL28,
			   PHY_CMN_PLL_CNTRL, PHY_CMN_PLL_CNTRL, 0x01, 0x01);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL29,
			   PHY_CMN_PLL_CNTRL, PHY_CMN_PLL_CNTRL, 0x01, 0x01);

	data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG1) | BIT(5);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL30,
			   PHY_CMN_CLK_CFG1, PHY_CMN_RBUF_CTRL, data, 0x01);
	DSI_DYN_PLL_REG_W(rsc->dyn_pll_base, DSI_DYNAMIC_REFRESH_PLL_CTRL31,
			   PHY_CMN_CLK_CFG1, PHY_CMN_CLK_CFG1, data, data);

	if (rsc->slave) {
		data = DSI_PLL_REG_R(rsc->slave->phy_base, PHY_CMN_CLK_CFG1) |
			BIT(5);

		DSI_DYN_PLL_REG_W(rsc->slave->dyn_pll_base,
				   DSI_DYNAMIC_REFRESH_PLL_CTRL30,
				   PHY_CMN_CLK_CFG1, PHY_CMN_RBUF_CTRL,
				   data, 0x01);
		DSI_DYN_PLL_REG_W(rsc->slave->dyn_pll_base,
				   DSI_DYNAMIC_REFRESH_PLL_CTRL31,
				   PHY_CMN_CLK_CFG1, PHY_CMN_CLK_CFG1,
				   data, data);
	}

	DSI_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR, upper_addr);
	DSI_PLL_REG_W(rsc->dyn_pll_base,
		DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR2, upper_addr2);
	wmb(); /* commit register writes */
}

static int dsi_pll_5nm_dynamic_clk_vco_set_rate(struct dsi_pll_resource *rsc)
{
	int rc;
	struct dsi_pll_5nm *pll;
	u32 rate;

	if (!rsc) {
		DSI_PLL_ERR(rsc, "pll resource not found\n");
		return -EINVAL;
	}

	rate = rsc->vco_rate;
	pll = rsc->priv;
	if (!pll) {
		DSI_PLL_ERR(rsc, "pll configuration not found\n");
		return -EINVAL;
	}

	rc = dsi_pll_read_stored_trim_codes(rsc, rate);
	if (rc) {
		DSI_PLL_ERR(rsc, "cannot find pll codes rate=%ld\n", rate);
		return -EINVAL;
	}

	DSI_PLL_DBG(rsc, "ndx=%d, rate=%lu\n", rsc->index, rate);
	rsc->vco_current_rate = rate;

	dsi_pll_calc_dec_frac(pll, rsc);

	/* program dynamic refresh control registers */
	dsi_pll_5nm_dynamic_refresh(pll, rsc);

	return 0;
}

static int dsi_pll_5nm_enable(struct dsi_pll_resource *rsc)
{
	int rc = 0;

	/* Start PLL */
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_PLL_CNTRL, 0x01);

	/*
	 * ensure all PLL configurations are written prior to checking
	 * for PLL lock.
	 */
	wmb();

	/* Check for PLL lock */
	rc = dsi_pll_5nm_lock_status(rsc);
	if (rc) {
		DSI_PLL_ERR(rsc, "lock failed\n");
		goto error;
	}

	/*
	 * assert power on reset for PHY digital in case the PLL is
	 * enabled after CX of analog domain power collapse. This needs
	 * to be done before enabling the global clk.
	 */
	dsi_pll_phy_dig_reset(rsc);
	if (rsc->slave)
		dsi_pll_phy_dig_reset(rsc->slave);

	dsi_pll_enable_global_clk(rsc);
	if (rsc->slave)
		dsi_pll_enable_global_clk(rsc->slave);

	/* flush, ensure all register writes are done*/
	wmb();
error:
	return rc;
}

static int dsi_pll_5nm_disable(struct dsi_pll_resource *rsc)
{
	int rc = 0;

	DSI_PLL_DBG(rsc, "stop PLL\n");

	/*
	 * To avoid any stray glitches while
	 * abruptly powering down the PLL
	 * make sure to gate the clock using
	 * the clock enable bit before powering
	 * down the PLL
	 */
	dsi_pll_disable_global_clk(rsc);
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_PLL_CNTRL, 0);
	dsi_pll_disable_sub(rsc);
	if (rsc->slave) {
		dsi_pll_disable_global_clk(rsc->slave);
		dsi_pll_disable_sub(rsc->slave);
	}
	/* flush, ensure all register writes are done*/
	wmb();

	return rc;
}

int dsi_pll_5nm_configure(void *pll, bool commit)
{

	int rc = 0;
	struct dsi_pll_resource *rsc = (struct dsi_pll_resource *)pll;

	dsi_pll_config_slave(rsc);

	/* PLL power needs to be enabled before accessing PLL registers */
	dsi_pll_enable_pll_bias(rsc);
	if (rsc->slave)
		dsi_pll_enable_pll_bias(rsc->slave);

	dsi_pll_init_val(rsc);

	rc = dsi_pll_5nm_set_byteclk_div(rsc, commit);

	if (commit) {
		rc = dsi_pll_5nm_set_pclk_div(rsc, commit);
		rc = dsi_pll_5nm_vco_set_rate(rsc);
	} else {
		rc = dsi_pll_5nm_dynamic_clk_vco_set_rate(rsc);
	}

	return 0;
}

int dsi_pll_5nm_toggle(void *pll, bool prepare)
{
	int rc = 0;
	struct dsi_pll_resource *pll_res = (struct dsi_pll_resource *)pll;

	if (!pll_res) {
		DSI_PLL_ERR(pll_res, "dsi pll resources are not available\n");
		return -EINVAL;
	}

	if (prepare) {
		rc = dsi_pll_5nm_enable(pll_res);
		if (rc)
			DSI_PLL_ERR(pll_res, "enable failed: %d\n", rc);
	} else {
		rc = dsi_pll_5nm_disable(pll_res);
		if (rc)
			DSI_PLL_ERR(pll_res, "disable failed: %d\n", rc);
	}

	return rc;
}
