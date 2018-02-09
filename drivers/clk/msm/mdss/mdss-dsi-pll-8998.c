/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/msm-clocks-8998.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-pll.h"

#define VCO_DELAY_USEC 1

#define MHZ_250		250000000UL
#define MHZ_500		500000000UL
#define MHZ_1000	1000000000UL
#define MHZ_1100	1100000000UL
#define MHZ_1900	1900000000UL
#define MHZ_3000	3000000000UL

/* Register Offsets from PLL base address */
#define PLL_ANALOG_CONTROLS_ONE			0x000
#define PLL_ANALOG_CONTROLS_TWO			0x004
#define PLL_INT_LOOP_SETTINGS			0x008
#define PLL_INT_LOOP_SETTINGS_TWO		0x00c
#define PLL_ANALOG_CONTROLS_THREE		0x010
#define PLL_ANALOG_CONTROLS_FOUR		0x014
#define PLL_INT_LOOP_CONTROLS			0x018
#define PLL_DSM_DIVIDER					0x01c
#define PLL_FEEDBACK_DIVIDER			0x020
#define PLL_SYSTEM_MUXES			0x024
#define PLL_FREQ_UPDATE_CONTROL_OVERRIDES			0x028
#define PLL_CMODE				0x02c
#define PLL_CALIBRATION_SETTINGS		0x030
#define PLL_BAND_SEL_CAL_TIMER_LOW		0x034
#define PLL_BAND_SEL_CAL_TIMER_HIGH		0x038
#define PLL_BAND_SEL_CAL_SETTINGS		0x03c
#define PLL_BAND_SEL_MIN		0x040
#define PLL_BAND_SEL_MAX		0x044
#define PLL_BAND_SEL_PFILT		0x048
#define PLL_BAND_SEL_IFILT		0x04c
#define PLL_BAND_SEL_CAL_SETTINGS_TWO		0x050
#define PLL_BAND_SEL_CAL_SETTINGS_THREE		0x054
#define PLL_BAND_SEL_CAL_SETTINGS_FOUR		0x058
#define PLL_BAND_SEL_ICODE_HIGH				0x05c
#define PLL_BAND_SEL_ICODE_LOW				0x060
#define PLL_FREQ_DETECT_SETTINGS_ONE		0x064
#define PLL_PFILT				0x07c
#define PLL_IFILT				0x080
#define PLL_GAIN				0x084
#define PLL_ICODE_LOW			0x088
#define PLL_ICODE_HIGH			0x08c
#define PLL_LOCKDET				0x090
#define PLL_OUTDIV				0x094
#define PLL_FASTLOCK_CONTROL	0x098
#define PLL_PASS_OUT_OVERRIDE_ONE		0x09c
#define PLL_PASS_OUT_OVERRIDE_TWO		0x0a0
#define PLL_CORE_OVERRIDE				0x0a4
#define PLL_CORE_INPUT_OVERRIDE			0x0a8
#define PLL_RATE_CHANGE					0x0ac
#define PLL_PLL_DIGITAL_TIMERS			0x0b0
#define PLL_PLL_DIGITAL_TIMERS_TWO		0x0b4
#define PLL_DEC_FRAC_MUXES				0x0c8
#define PLL_DECIMAL_DIV_START_1			0x0cc
#define PLL_FRAC_DIV_START_LOW_1		0x0d0
#define PLL_FRAC_DIV_START_MID_1		0x0d4
#define PLL_FRAC_DIV_START_HIGH_1		0x0d8
#define PLL_MASH_CONTROL				0x0ec
#define PLL_SSC_MUX_CONTROL				0x108
#define PLL_SSC_STEPSIZE_LOW_1			0x10c
#define PLL_SSC_STEPSIZE_HIGH_1			0x110
#define PLL_SSC_DIV_PER_LOW_1			0x114
#define PLL_SSC_DIV_PER_HIGH_1			0x118
#define PLL_SSC_DIV_ADJPER_LOW_1		0x11c
#define PLL_SSC_DIV_ADJPER_HIGH_1		0x120
#define PLL_SSC_CONTROL					0x13c
#define PLL_PLL_OUTDIV_RATE				0x140
#define PLL_PLL_LOCKDET_RATE_1			0x144
#define PLL_PLL_PROP_GAIN_RATE_1		0x14c
#define PLL_PLL_BAND_SET_RATE_1			0x154
#define PLL_PLL_INT_GAIN_IFILT_BAND_1		0x15c
#define PLL_PLL_FL_INT_GAIN_PFILT_BAND_1	0x164
#define PLL_FASTLOCK_EN_BAND				0x16c
#define PLL_FREQ_TUNE_ACCUM_INIT_MUX		0x17c
#define PLL_PLL_LOCK_OVERRIDE				0x180
#define PLL_PLL_LOCK_DELAY					0x184
#define PLL_PLL_LOCK_MIN_DELAY				0x188
#define PLL_CLOCK_INVERTERS					0x18c
#define PLL_SPARE_AND_JPC_OVERRIDES			0x190
#define PLL_BIAS_CONTROL_1					0x194
#define PLL_BIAS_CONTROL_2					0x198
#define PLL_ALOG_OBSV_BUS_CTRL_1			0x19c
#define PLL_COMMON_STATUS_ONE				0x1a0

/* Register Offsets from PHY base address */
#define PHY_CMN_CLK_CFG0	0x010
#define PHY_CMN_CLK_CFG1	0x014
#define PHY_CMN_RBUF_CTRL	0x01c
#define PHY_CMN_PLL_CNTRL	0x038
#define PHY_CMN_CTRL_0		0x024

/* Bit definition of SSC control registers */
#define SSC_CENTER		BIT(0)
#define SSC_EN			BIT(1)
#define SSC_FREQ_UPDATE		BIT(2)
#define SSC_FREQ_UPDATE_MUX	BIT(3)
#define SSC_UPDATE_SSC		BIT(4)
#define SSC_UPDATE_SSC_MUX	BIT(5)
#define SSC_START		BIT(6)
#define SSC_START_MUX		BIT(7)

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

struct dsi_pll_8998 {
	struct mdss_pll_resources *rsc;
	struct dsi_pll_config pll_configuration;
	struct dsi_pll_regs reg_setup;
};

static struct mdss_pll_resources *pll_rsc_db[DSI_PLL_MAX];
static struct dsi_pll_8998 plls[DSI_PLL_MAX];

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
		pr_debug("slave PLL unavilable, assuming standalone config\n");
		return;
	}

	/* check to see if the source of DSI1 PLL bitclk is set to external */
	reg = MDSS_PLL_REG_R(orsc->phy_base, PHY_CMN_CLK_CFG1);
	reg &= (BIT(2) | BIT(3));
	if (reg == 0x04)
		rsc->slave = pll_rsc_db[DSI_PLL_1]; /* external source */

	pr_debug("Slave PLL %s\n", rsc->slave ? "configured" : "absent");
}

static void dsi_pll_setup_config(struct dsi_pll_8998 *pll,
				 struct mdss_pll_resources *rsc)
{
	struct dsi_pll_config *config = &pll->pll_configuration;

	config->ref_freq = 19200000;
	config->dec_bits = 8;
	config->frac_bits = 18;
	config->lock_timer = 64;
	config->ssc_freq = 31500;
	config->ssc_offset = 5000;
	config->ssc_adj_per = 2;
	config->thresh_cycles = 32;
	config->refclk_cycles = 256;

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

	dsi_pll_config_slave(rsc);
}

static void dsi_pll_calc_dec_frac(struct dsi_pll_8998 *pll,
				  struct mdss_pll_resources *rsc)
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

	if (pll_freq <= MHZ_1900)
		regs->pll_prop_gain_rate = 8;
	else if (pll_freq <= MHZ_3000)
		regs->pll_prop_gain_rate = 10;
	else
		regs->pll_prop_gain_rate = 12;

	if (pll_freq < MHZ_1100)
		regs->pll_clock_inverters = 8;
	else
		regs->pll_clock_inverters = 0;

	regs->pll_lockdet_rate = config->lock_timer;
	regs->decimal_div_start = dec;
	regs->frac_div_start_low = (frac & 0xff);
	regs->frac_div_start_mid = (frac & 0xff00) >> 8;
	regs->frac_div_start_high = (frac & 0x30000) >> 16;
}

static void dsi_pll_calc_ssc(struct dsi_pll_8998 *pll,
		  struct mdss_pll_resources *rsc)
{
	struct dsi_pll_config *config = &pll->pll_configuration;
	struct dsi_pll_regs *regs = &pll->reg_setup;
	u32 ssc_per;
	u32 ssc_mod;
	u64 ssc_step_size;
	u64 frac;

	if (!config->enable_ssc) {
		pr_debug("SSC not enabled\n");
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

	pr_debug("SCC: Dec:%d, frac:%llu, frac_bits:%d\n",
			regs->decimal_div_start, frac, config->frac_bits);
	pr_debug("SSC: div_per:0x%X, stepsize:0x%X, adjper:0x%X\n",
			ssc_per, (u32)ssc_step_size, config->ssc_adj_per);
}

static void dsi_pll_ssc_commit(struct dsi_pll_8998 *pll,
		struct mdss_pll_resources *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	struct dsi_pll_regs *regs = &pll->reg_setup;

	if (pll->pll_configuration.enable_ssc) {
		pr_debug("SSC is enabled\n");

		MDSS_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_LOW_1,
				regs->ssc_stepsize_low);
		MDSS_PLL_REG_W(pll_base, PLL_SSC_STEPSIZE_HIGH_1,
				regs->ssc_stepsize_high);
		MDSS_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_LOW_1,
				regs->ssc_div_per_low);
		MDSS_PLL_REG_W(pll_base, PLL_SSC_DIV_PER_HIGH_1,
				regs->ssc_div_per_high);
		MDSS_PLL_REG_W(pll_base, PLL_SSC_DIV_ADJPER_LOW_1,
				regs->ssc_adjper_low);
		MDSS_PLL_REG_W(pll_base, PLL_SSC_DIV_ADJPER_HIGH_1,
				regs->ssc_adjper_high);
		MDSS_PLL_REG_W(pll_base, PLL_SSC_CONTROL,
				SSC_EN | regs->ssc_control);
	}
}

static void dsi_pll_config_hzindep_reg(struct dsi_pll_8998 *pll,
				  struct mdss_pll_resources *rsc)
{
	void __iomem *pll_base = rsc->pll_base;

	MDSS_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_ONE, 0x80);
	MDSS_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_TWO, 0x03);
	MDSS_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_THREE, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_DSM_DIVIDER, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_FEEDBACK_DIVIDER, 0x4e);
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
	MDSS_PLL_REG_W(pll_base, PLL_PFILT, 0x29);
	MDSS_PLL_REG_W(pll_base, PLL_IFILT, 0x3f);
}

static void dsi_pll_init_val(struct mdss_pll_resources *rsc)
{
	void __iomem *pll_base = rsc->pll_base;

	MDSS_PLL_REG_W(pll_base, PLL_CORE_INPUT_OVERRIDE, 0x10);
	MDSS_PLL_REG_W(pll_base, PLL_INT_LOOP_SETTINGS, 0x3f);
	MDSS_PLL_REG_W(pll_base, PLL_INT_LOOP_SETTINGS_TWO, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_ANALOG_CONTROLS_FOUR, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_INT_LOOP_CONTROLS, 0x80);
	MDSS_PLL_REG_W(pll_base, PLL_FREQ_UPDATE_CONTROL_OVERRIDES, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_TIMER_LOW, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_TIMER_HIGH, 0x02);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS, 0x82);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_MIN, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_MAX, 0xff);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_PFILT, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_IFILT, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS_TWO, 0x25);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_CAL_SETTINGS_FOUR, 0x4f);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_ICODE_HIGH, 0x0a);
	MDSS_PLL_REG_W(pll_base, PLL_BAND_SEL_ICODE_LOW, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_GAIN, 0x42);
	MDSS_PLL_REG_W(pll_base, PLL_ICODE_LOW, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_ICODE_HIGH, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_LOCKDET, 0x30);
	MDSS_PLL_REG_W(pll_base, PLL_FASTLOCK_CONTROL, 0x04);
	MDSS_PLL_REG_W(pll_base, PLL_PASS_OUT_OVERRIDE_ONE, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_PASS_OUT_OVERRIDE_TWO, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_RATE_CHANGE, 0x01);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_DIGITAL_TIMERS, 0x08);
	MDSS_PLL_REG_W(pll_base, PLL_DEC_FRAC_MUXES, 0x00);
	MDSS_PLL_REG_W(pll_base, PLL_MASH_CONTROL, 0x03);
	MDSS_PLL_REG_W(pll_base, PLL_SSC_MUX_CONTROL, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_SSC_CONTROL, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_FASTLOCK_EN_BAND, 0x03);
	MDSS_PLL_REG_W(pll_base, PLL_FREQ_TUNE_ACCUM_INIT_MUX, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_LOCK_MIN_DELAY, 0x19);
	MDSS_PLL_REG_W(pll_base, PLL_SPARE_AND_JPC_OVERRIDES, 0x0);
	MDSS_PLL_REG_W(pll_base, PLL_BIAS_CONTROL_1, 0x40);
	MDSS_PLL_REG_W(pll_base, PLL_BIAS_CONTROL_2, 0x20);
	MDSS_PLL_REG_W(pll_base, PLL_ALOG_OBSV_BUS_CTRL_1, 0x0);
}

static void dsi_pll_commit(struct dsi_pll_8998 *pll,
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
	MDSS_PLL_REG_W(pll_base, PLL_PLL_LOCKDET_RATE_1, 0x40);
	MDSS_PLL_REG_W(pll_base, PLL_PLL_LOCK_DELAY, 0x06);
	MDSS_PLL_REG_W(pll_base, PLL_CMODE, 0x10);
	MDSS_PLL_REG_W(pll_base, PLL_CLOCK_INVERTERS, reg->pll_clock_inverters);

}

static int vco_8998_set_rate(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *rsc = vco->priv;
	struct dsi_pll_8998 *pll;

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

	dsi_pll_init_val(rsc);

	dsi_pll_setup_config(pll, rsc);

	dsi_pll_calc_dec_frac(pll, rsc);

	dsi_pll_calc_ssc(pll, rsc);

	dsi_pll_commit(pll, rsc);

	dsi_pll_config_hzindep_reg(pll, rsc);

	dsi_pll_ssc_commit(pll, rsc);

	/* flush, ensure all register writes are done*/
	wmb();

	mdss_pll_resource_enable(rsc, false);

	return 0;
}

static int dsi_pll_8998_lock_status(struct mdss_pll_resources *pll)
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

	MDSS_PLL_REG_W(rsc->pll_base, PLL_SYSTEM_MUXES, 0);
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data & ~BIT(5));
	ndelay(250);
}

static void dsi_pll_enable_pll_bias(struct mdss_pll_resources *rsc)
{
	u32 data = MDSS_PLL_REG_R(rsc->phy_base, PHY_CMN_CTRL_0);

	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data | BIT(5));
	MDSS_PLL_REG_W(rsc->pll_base, PLL_SYSTEM_MUXES, 0xc0);
	ndelay(250);
}

static void dsi_pll_disable_global_clk(struct mdss_pll_resources *rsc)
{
	u32 data;

	data = MDSS_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG1);
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_CLK_CFG1, (data & ~BIT(5)));
}

static void dsi_pll_enable_global_clk(struct mdss_pll_resources *rsc)
{
	u32 data;

	data = MDSS_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG1);
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_CLK_CFG1, (data | BIT(5)));
}

static int dsi_pll_enable(struct dsi_pll_vco_clk *vco)
{
	int rc;
	struct mdss_pll_resources *rsc = vco->priv;
	struct dsi_pll_8998 *pll = rsc->priv;
	struct dsi_pll_regs *regs = &pll->reg_setup;

	dsi_pll_enable_pll_bias(rsc);
	if (rsc->slave)
		dsi_pll_enable_pll_bias(rsc->slave);

	/*
	 * The PLL out dividers are fixed divider clocks and hence the
	 * set_div is not called during set_rate cycle of the tree.
	 * The outdiv rate is therefore set in the pll out mux's set_sel
	 * callback. But that will be called only after vco's set rate.
	 * Hence PLL out div value is set here before locking the PLL.
	 */
	MDSS_PLL_REG_W(rsc->pll_base, PLL_PLL_OUTDIV_RATE,
		regs->pll_outdiv_rate);

	/* Start PLL */
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_PLL_CNTRL, 0x01);

	/*
	 * ensure all PLL configurations are written prior to checking
	 * for PLL lock.
	 */
	wmb();

	/* Check for PLL lock */
	rc = dsi_pll_8998_lock_status(rsc);
	if (rc) {
		pr_err("PLL(%d) lock failed\n", rsc->index);
		goto error;
	}

	rsc->pll_on = true;

	dsi_pll_enable_global_clk(rsc);
	if (rsc->slave)
		dsi_pll_enable_global_clk(rsc->slave);

	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_RBUF_CTRL, 0x01);
	if (rsc->slave)
		MDSS_PLL_REG_W(rsc->slave->phy_base, PHY_CMN_RBUF_CTRL, 0x01);

error:
	return rc;
}

static void dsi_pll_disable_sub(struct mdss_pll_resources *rsc)
{
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_RBUF_CTRL, 0);
	dsi_pll_disable_pll_bias(rsc);
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

	/*
	 * To avoid any stray glitches while
	 * abruptly powering down the PLL
	 * make sure to gate the clock using
	 * the clock enable bit before powering
	 * down the PLL
	 **/
	dsi_pll_disable_global_clk(rsc);
	MDSS_PLL_REG_W(rsc->phy_base, PHY_CMN_PLL_CNTRL, 0);
	dsi_pll_disable_sub(rsc);
	if (rsc->slave) {
		dsi_pll_disable_global_clk(rsc->slave);
		dsi_pll_disable_sub(rsc->slave);
	}
	/* flush, ensure all register writes are done*/
	wmb();
	rsc->pll_on = false;
}

static void vco_8998_unprepare(struct clk *c)
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

static int vco_8998_prepare(struct clk *c)
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
	struct mdss_pll_resources *rsc = vco->priv;
	struct dsi_pll_8998 *pll = rsc->priv;
	struct dsi_pll_regs *regs = &pll->reg_setup;
	int rc;
	u64 ref_clk = vco->ref_clk_rate;
	u64 vco_rate;
	u64 multiplier;
	u32 frac;
	u32 dec;
	u32 outdiv;
	u64 pll_freq, tmp64;

	rc = mdss_pll_resource_enable(rsc, true);
	if (rc) {
		pr_err("failed to enable pll(%d) resource, rc=%d\n",
		       rsc->index, rc);
		return 0;
	}

	dec = MDSS_PLL_REG_R(rsc->pll_base, PLL_DECIMAL_DIV_START_1);
	dec &= 0xFF;

	frac = MDSS_PLL_REG_R(rsc->pll_base, PLL_FRAC_DIV_START_LOW_1);
	frac |= ((MDSS_PLL_REG_R(rsc->pll_base, PLL_FRAC_DIV_START_MID_1) &
		  0xFF) <<
		8);
	frac |= ((MDSS_PLL_REG_R(rsc->pll_base, PLL_FRAC_DIV_START_HIGH_1) &
		  0x3) <<
		16);

	/* OUTDIV_1:0 field is (log(outdiv, 2)) */
	outdiv = MDSS_PLL_REG_R(rsc->pll_base, PLL_PLL_OUTDIV_RATE);
	outdiv &= 0x3;

	regs->pll_outdiv_rate = outdiv;

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

	(void)mdss_pll_resource_enable(rsc, false);

	return (unsigned long)vco_rate;
}

enum handoff vco_8998_handoff(struct clk *c)
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
	struct dsi_pll_8998 *pll;

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

static int dsi_pll_out_set_mux_sel(struct mux_clk *clk, int sel)
{
	struct mdss_pll_resources *rsc = clk->priv;
	struct dsi_pll_8998 *pll = rsc->priv;
	struct dsi_pll_regs *regs = &pll->reg_setup;

	regs->pll_outdiv_rate = sel;

	return 0;
}

static int dsi_pll_out_get_mux_sel(struct mux_clk *clk)
{
	struct mdss_pll_resources *rsc = clk->priv;
	struct dsi_pll_8998 *pll = rsc->priv;
	struct dsi_pll_regs *regs = &pll->reg_setup;

	return regs->pll_outdiv_rate;
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

long vco_8998_round_rate(struct clk *c, unsigned long rate)
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

static struct clk_ops clk_ops_vco_8998 = {
	.set_rate = vco_8998_set_rate,
	.round_rate = vco_8998_round_rate,
	.handoff = vco_8998_handoff,
	.prepare = vco_8998_prepare,
	.unprepare = vco_8998_unprepare,
};

static struct clk_mux_ops mdss_mux_ops = {
	.set_mux_sel = mdss_set_mux_sel,
	.get_mux_sel = mdss_get_mux_sel,
};

static struct clk_mux_ops mdss_pll_out_mux_ops = {
	.set_mux_sel = dsi_pll_out_set_mux_sel,
	.get_mux_sel = dsi_pll_out_get_mux_sel,
};

/*
 * Clock tree for generating DSI byte and pixel clocks.
 *
 *        +---------------+
 *        |    vco_clk    |
 *        |               |
 *        +-------+-------+
 *                |
 *                |
 *        +-------+--------+------------------+-----------------+
 *        |                |                  |                 |
 * +------v-------+ +------v-------+  +-------v------+   +------v-------+
 * | pll_out_div1 | | pll_out_div2 |  | pll_out_div4 |   | pll_out_div8 |
 * |    DIV(1)    | |    DIV(2)    |  |    DIV(4)    |   |    DIV(8)    |
 * +------+-------+ +------+-------+  +-------+------+   +------+-------+
 *        |                |                  |                 |
 *        +------------+   |   +--------------+                 |
 *                     |   |   |    +---------------------------+
 *                     |   |   |    |
 *                  +--v---v---v----v--+
 *                   \   pll_out_mux  /
 *                    \              /
 *                     +------+-----+
 *                            |
 *            +---------------+-----------------+
 *            |               |                 |
 *     +------v-----+ +-------v-------+ +-------v-------+
 *     | bitclk_src | | post_vco_div1 | | post_vco_div4 |
 *     | DIV(1..15) | +     DIV(1)    | |     DIV(4)    |
 *     +------+-----+ +-------+-------+ +-------+-------+
 *            |               |                 |
 * Shadow     |               |                 +---------------------+
 *  Path      |               +-----------------------------+         |
 *   +        |                                             |         |
 *   |        +---------------------------------+           |         |
 *   |        |                                 |           |         |
 *   | +------v------=+                  +------v-------+ +-v---------v----+
 *   | | byteclk_src  |                  | post_bit_div |  \ post_vco_mux /
 *   | |    DIV(8)    |                  |   DIV(1,2)   |   \            /
 *   | +------+-------+                  +------+-------+    +---+------+
 *   |        |                                 |                |
 *   |        |                                 |     +----------+
 *   |        |                                 |     |
 *   |        |                            +----v-----v------+
 * +-v--------v---------+                   \  pclk_src_mux /
 *  \   byteclk_mux    /                     \             /
 *   \                /                       +-----+-----+
 *    +------+-------+                              |         Shadow
 *           |                                      |          Path
 *           v                                +-----v------+    +
 *       dsi_byte_clk                         |  pclk_src  |    |
 *                                            | DIV(1..15) |    |
 *                                            +-----+------+    |
 *                                                  |           |
 *                                                  +------+    |
 *                                                         |    |
 *                                                     +---v----v----+
 *                                                      \  pclk_mux /
 *                                                       \         /
 *                                                        +---+---+
 *                                                            |
 *                                                            |
 *                                                            v
 *                                                         dsi_pclk
 *
 */

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 3500000000UL,
	.c = {
		.dbg_name = "dsi0pll_vco_clk",
		.ops = &clk_ops_vco_8998,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_vco_clk.c),
	},
};

static struct div_clk dsi0pll_pll_out_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_pll_out_div1",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pll_out_div1.c),
	}
};

static struct div_clk dsi0pll_pll_out_div2 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_pll_out_div2",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pll_out_div2.c),
	}
};

static struct div_clk dsi0pll_pll_out_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_pll_out_div4",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pll_out_div4.c),
	}
};

static struct div_clk dsi0pll_pll_out_div8 = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_pll_out_div8",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pll_out_div8.c),
	}
};

static struct mux_clk dsi0pll_pll_out_mux = {
	.num_parents = 4,
	.parents = (struct clk_src[]) {
		{&dsi0pll_pll_out_div1.c, 0},
		{&dsi0pll_pll_out_div2.c, 1},
		{&dsi0pll_pll_out_div4.c, 2},
		{&dsi0pll_pll_out_div8.c, 3},
	},
	.ops = &mdss_pll_out_mux_ops,
	.c = {
		.parent = &dsi0pll_pll_out_div1.c,
		.dbg_name = "dsi0pll_pll_out_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pll_out_mux.c),
	}
};
static struct div_clk dsi0pll_bitclk_src = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 15,
	},
	.ops = &clk_bitclk_src_ops,
	.c = {
		.parent = &dsi0pll_pll_out_mux.c,
		.dbg_name = "dsi0pll_bitclk_src",
		.ops = &clk_ops_bitclk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_bitclk_src.c),
	}
};

static struct div_clk dsi0pll_post_vco_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.ops = &clk_post_vco_div_ops,
	.c = {
		.parent = &dsi0pll_pll_out_mux.c,
		.dbg_name = "dsi0pll_post_vco_div1",
		.ops = &clk_ops_post_vco_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_vco_div1.c),
	}
};

static struct div_clk dsi0pll_post_vco_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.ops = &clk_post_vco_div_ops,
	.c = {
		.parent = &dsi0pll_pll_out_mux.c,
		.dbg_name = "dsi0pll_post_vco_div4",
		.ops = &clk_ops_post_vco_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_vco_div4.c),
	}
};

static struct mux_clk dsi0pll_post_vco_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi0pll_post_vco_div1.c, 0},
		{&dsi0pll_post_vco_div4.c, 1},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi0pll_post_vco_div1.c,
		.dbg_name = "dsi0pll_post_vco_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_vco_mux.c),
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
		{&dsi0pll_post_vco_mux.c, 1},
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
	.min_rate = 1000000000UL,
	.max_rate = 3500000000UL,
	.c = {
		.dbg_name = "dsi1pll_vco_clk",
		.ops = &clk_ops_vco_8998,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_vco_clk.c),
	},
};

static struct div_clk dsi1pll_pll_out_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_pll_out_div1",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pll_out_div1.c),
	}
};

static struct div_clk dsi1pll_pll_out_div2 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_pll_out_div2",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pll_out_div2.c),
	}
};

static struct div_clk dsi1pll_pll_out_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_pll_out_div4",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pll_out_div4.c),
	}
};

static struct div_clk dsi1pll_pll_out_div8 = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_pll_out_div8",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pll_out_div8.c),
	}
};

static struct mux_clk dsi1pll_pll_out_mux = {
	.num_parents = 4,
	.parents = (struct clk_src[]) {
		{&dsi1pll_pll_out_div1.c, 0},
		{&dsi1pll_pll_out_div2.c, 1},
		{&dsi1pll_pll_out_div4.c, 2},
		{&dsi1pll_pll_out_div8.c, 3},
	},
	.ops = &mdss_pll_out_mux_ops,
	.c = {
		.parent = &dsi1pll_pll_out_div1.c,
		.dbg_name = "dsi1pll_pll_out_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pll_out_mux.c),
	}
};

static struct div_clk dsi1pll_bitclk_src = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 15,
	},
	.ops = &clk_bitclk_src_ops,
	.c = {
		.parent = &dsi1pll_pll_out_mux.c,
		.dbg_name = "dsi1pll_bitclk_src",
		.ops = &clk_ops_bitclk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_bitclk_src.c),
	}
};

static struct div_clk dsi1pll_post_vco_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.ops = &clk_post_vco_div_ops,
	.c = {
		.parent = &dsi1pll_pll_out_mux.c,
		.dbg_name = "dsi1pll_post_vco_div1",
		.ops = &clk_ops_post_vco_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_vco_div1.c),
	}
};

static struct div_clk dsi1pll_post_vco_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.ops = &clk_post_vco_div_ops,
	.c = {
		.parent = &dsi1pll_pll_out_mux.c,
		.dbg_name = "dsi1pll_post_vco_div4",
		.ops = &clk_ops_post_vco_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_vco_div4.c),
	}
};

static struct mux_clk dsi1pll_post_vco_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi1pll_post_vco_div1.c, 0},
		{&dsi1pll_post_vco_div4.c, 1},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dsi1pll_post_vco_div1.c,
		.dbg_name = "dsi1pll_post_vco_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_vco_mux.c),
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
		{&dsi1pll_post_vco_mux.c, 1},
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

static struct clk_lookup mdss_dsi_pll0cc_8998[] = {
	CLK_LIST(dsi0pll_byteclk_mux),
	CLK_LIST(dsi0pll_byteclk_src),
	CLK_LIST(dsi0pll_pclk_mux),
	CLK_LIST(dsi0pll_pclk_src),
	CLK_LIST(dsi0pll_pclk_src_mux),
	CLK_LIST(dsi0pll_post_bit_div),
	CLK_LIST(dsi0pll_post_vco_mux),
	CLK_LIST(dsi0pll_post_vco_div1),
	CLK_LIST(dsi0pll_post_vco_div4),
	CLK_LIST(dsi0pll_bitclk_src),
	CLK_LIST(dsi0pll_pll_out_mux),
	CLK_LIST(dsi0pll_pll_out_div8),
	CLK_LIST(dsi0pll_pll_out_div4),
	CLK_LIST(dsi0pll_pll_out_div2),
	CLK_LIST(dsi0pll_pll_out_div1),
	CLK_LIST(dsi0pll_vco_clk),
};
static struct clk_lookup mdss_dsi_pll1cc_8998[] = {
	CLK_LIST(dsi1pll_byteclk_mux),
	CLK_LIST(dsi1pll_byteclk_src),
	CLK_LIST(dsi1pll_pclk_mux),
	CLK_LIST(dsi1pll_pclk_src),
	CLK_LIST(dsi1pll_pclk_src_mux),
	CLK_LIST(dsi1pll_post_bit_div),
	CLK_LIST(dsi1pll_post_vco_mux),
	CLK_LIST(dsi1pll_post_vco_div1),
	CLK_LIST(dsi1pll_post_vco_div4),
	CLK_LIST(dsi1pll_bitclk_src),
	CLK_LIST(dsi1pll_pll_out_mux),
	CLK_LIST(dsi1pll_pll_out_div8),
	CLK_LIST(dsi1pll_pll_out_div4),
	CLK_LIST(dsi1pll_pll_out_div2),
	CLK_LIST(dsi1pll_pll_out_div1),
	CLK_LIST(dsi1pll_vco_clk),
};

int dsi_pll_clock_register_8998(struct platform_device *pdev,
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
		dsi0pll_post_vco_mux.priv = pll_res;
		dsi0pll_post_vco_div1.priv = pll_res;
		dsi0pll_post_vco_div4.priv = pll_res;
		dsi0pll_bitclk_src.priv = pll_res;
		dsi0pll_pll_out_div1.priv = pll_res;
		dsi0pll_pll_out_div2.priv = pll_res;
		dsi0pll_pll_out_div4.priv = pll_res;
		dsi0pll_pll_out_div8.priv = pll_res;
		dsi0pll_pll_out_mux.priv = pll_res;
		dsi0pll_vco_clk.priv = pll_res;

		rc = of_msm_clock_register(pdev->dev.of_node,
			mdss_dsi_pll0cc_8998,
			ARRAY_SIZE(mdss_dsi_pll0cc_8998));
	} else {
		dsi1pll_byteclk_mux.priv = pll_res;
		dsi1pll_byteclk_src.priv = pll_res;
		dsi1pll_pclk_mux.priv = pll_res;
		dsi1pll_pclk_src.priv = pll_res;
		dsi1pll_pclk_src_mux.priv = pll_res;
		dsi1pll_post_bit_div.priv = pll_res;
		dsi1pll_post_vco_mux.priv = pll_res;
		dsi1pll_post_vco_div1.priv = pll_res;
		dsi1pll_post_vco_div4.priv = pll_res;
		dsi1pll_bitclk_src.priv = pll_res;
		dsi1pll_pll_out_div1.priv = pll_res;
		dsi1pll_pll_out_div2.priv = pll_res;
		dsi1pll_pll_out_div4.priv = pll_res;
		dsi1pll_pll_out_div8.priv = pll_res;
		dsi1pll_pll_out_mux.priv = pll_res;
		dsi1pll_vco_clk.priv = pll_res;

		rc = of_msm_clock_register(pdev->dev.of_node,
			mdss_dsi_pll1cc_8998,
			ARRAY_SIZE(mdss_dsi_pll1cc_8998));
	}
	if (rc)
		pr_err("dsi%dpll clock register failed, rc=%d\n", ndx, rc);

	return rc;
}
