// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include "dsi_pll.h"
#include <dt-bindings/clock/mdss-5nm-pll-clk.h>

#define VCO_DELAY_USEC 1

#define MHZ_250		250000000UL
#define MHZ_500		500000000UL
#define MHZ_1000	1000000000UL
#define MHZ_1100	1100000000UL
#define MHZ_1900	1900000000UL
#define MHZ_3000	3000000000UL

/* Register Offsets from PLL base address */
#define PLL_ANALOG_CONTROLS_ONE			0x0000
#define PLL_ANALOG_CONTROLS_TWO			0x0004
#define PLL_INT_LOOP_SETTINGS			0x0008
#define PLL_INT_LOOP_SETTINGS_TWO		0x000C
#define PLL_ANALOG_CONTROLS_THREE		0x0010
#define PLL_ANALOG_CONTROLS_FOUR		0x0014
#define PLL_ANALOG_CONTROLS_FIVE		0x0018
#define PLL_INT_LOOP_CONTROLS			0x001C
#define PLL_DSM_DIVIDER				0x0020
#define PLL_FEEDBACK_DIVIDER			0x0024
#define PLL_SYSTEM_MUXES			0x0028
#define PLL_FREQ_UPDATE_CONTROL_OVERRIDES	0x002C
#define PLL_CMODE				0x0030
#define PLL_PSM_CTRL				0x0034
#define PLL_RSM_CTRL				0x0038
#define PLL_VCO_TUNE_MAP			0x003C
#define PLL_PLL_CNTRL				0x0040
#define PLL_CALIBRATION_SETTINGS		0x0044
#define PLL_BAND_SEL_CAL_TIMER_LOW		0x0048
#define PLL_BAND_SEL_CAL_TIMER_HIGH		0x004C
#define PLL_BAND_SEL_CAL_SETTINGS		0x0050
#define PLL_BAND_SEL_MIN			0x0054
#define PLL_BAND_SEL_MAX			0x0058
#define PLL_BAND_SEL_PFILT			0x005C
#define PLL_BAND_SEL_IFILT			0x0060
#define PLL_BAND_SEL_CAL_SETTINGS_TWO		0x0064
#define PLL_BAND_SEL_CAL_SETTINGS_THREE		0x0068
#define PLL_BAND_SEL_CAL_SETTINGS_FOUR		0x006C
#define PLL_BAND_SEL_ICODE_HIGH			0x0070
#define PLL_BAND_SEL_ICODE_LOW			0x0074
#define PLL_FREQ_DETECT_SETTINGS_ONE		0x0078
#define PLL_FREQ_DETECT_THRESH			0x007C
#define PLL_FREQ_DET_REFCLK_HIGH		0x0080
#define PLL_FREQ_DET_REFCLK_LOW			0x0084
#define PLL_FREQ_DET_PLLCLK_HIGH		0x0088
#define PLL_FREQ_DET_PLLCLK_LOW			0x008C
#define PLL_PFILT				0x0090
#define PLL_IFILT				0x0094
#define PLL_PLL_GAIN				0x0098
#define PLL_ICODE_LOW				0x009C
#define PLL_ICODE_HIGH				0x00A0
#define PLL_LOCKDET				0x00A4
#define PLL_OUTDIV				0x00A8
#define PLL_FASTLOCK_CONTROL			0x00AC
#define PLL_PASS_OUT_OVERRIDE_ONE		0x00B0
#define PLL_PASS_OUT_OVERRIDE_TWO		0x00B4
#define PLL_CORE_OVERRIDE			0x00B8
#define PLL_CORE_INPUT_OVERRIDE			0x00BC
#define PLL_RATE_CHANGE				0x00C0
#define PLL_PLL_DIGITAL_TIMERS			0x00C4
#define PLL_PLL_DIGITAL_TIMERS_TWO		0x00C8
#define PLL_DECIMAL_DIV_START			0x00CC
#define PLL_FRAC_DIV_START_LOW			0x00D0
#define PLL_FRAC_DIV_START_MID			0x00D4
#define PLL_FRAC_DIV_START_HIGH			0x00D8
#define PLL_DEC_FRAC_MUXES			0x00DC
#define PLL_DECIMAL_DIV_START_1			0x00E0
#define PLL_FRAC_DIV_START_LOW_1		0x00E4
#define PLL_FRAC_DIV_START_MID_1		0x00E8
#define PLL_FRAC_DIV_START_HIGH_1		0x00EC
#define PLL_DECIMAL_DIV_START_2			0x00F0
#define PLL_FRAC_DIV_START_LOW_2		0x00F4
#define PLL_FRAC_DIV_START_MID_2		0x00F8
#define PLL_FRAC_DIV_START_HIGH_2		0x00FC
#define PLL_MASH_CONTROL			0x0100
#define PLL_SSC_STEPSIZE_LOW			0x0104
#define PLL_SSC_STEPSIZE_HIGH			0x0108
#define PLL_SSC_DIV_PER_LOW			0x010C
#define PLL_SSC_DIV_PER_HIGH			0x0110
#define PLL_SSC_ADJPER_LOW			0x0114
#define PLL_SSC_ADJPER_HIGH			0x0118
#define PLL_SSC_MUX_CONTROL			0x011C
#define PLL_SSC_STEPSIZE_LOW_1			0x0120
#define PLL_SSC_STEPSIZE_HIGH_1			0x0124
#define PLL_SSC_DIV_PER_LOW_1			0x0128
#define PLL_SSC_DIV_PER_HIGH_1			0x012C
#define PLL_SSC_ADJPER_LOW_1			0x0130
#define PLL_SSC_ADJPER_HIGH_1			0x0134
#define PLL_SSC_STEPSIZE_LOW_2			0x0138
#define PLL_SSC_STEPSIZE_HIGH_2			0x013C
#define PLL_SSC_DIV_PER_LOW_2			0x0140
#define PLL_SSC_DIV_PER_HIGH_2			0x0144
#define PLL_SSC_ADJPER_LOW_2			0x0148
#define PLL_SSC_ADJPER_HIGH_2			0x014C
#define PLL_SSC_CONTROL				0x0150
#define PLL_PLL_OUTDIV_RATE			0x0154
#define PLL_PLL_LOCKDET_RATE_1			0x0158
#define PLL_PLL_LOCKDET_RATE_2			0x015C
#define PLL_PLL_PROP_GAIN_RATE_1		0x0160
#define PLL_PLL_PROP_GAIN_RATE_2		0x0164
#define PLL_PLL_BAND_SEL_RATE_1			0x0168
#define PLL_PLL_BAND_SEL_RATE_2			0x016C
#define PLL_PLL_INT_GAIN_IFILT_BAND_1		0x0170
#define PLL_PLL_INT_GAIN_IFILT_BAND_2		0x0174
#define PLL_PLL_FL_INT_GAIN_PFILT_BAND_1	0x0178
#define PLL_PLL_FL_INT_GAIN_PFILT_BAND_2	0x017C
#define PLL_PLL_FASTLOCK_EN_BAND		0x0180
#define PLL_FREQ_TUNE_ACCUM_INIT_MID		0x0184
#define PLL_FREQ_TUNE_ACCUM_INIT_HIGH		0x0188
#define PLL_FREQ_TUNE_ACCUM_INIT_MUX		0x018C
#define PLL_PLL_LOCK_OVERRIDE			0x0190
#define PLL_PLL_LOCK_DELAY			0x0194
#define PLL_PLL_LOCK_MIN_DELAY			0x0198
#define PLL_CLOCK_INVERTERS			0x019C
#define PLL_SPARE_AND_JPC_OVERRIDES		0x01A0
#define PLL_BIAS_CONTROL_1			0x01A4
#define PLL_BIAS_CONTROL_2			0x01A8
#define PLL_ALOG_OBSV_BUS_CTRL_1		0x01AC
#define PLL_COMMON_STATUS_ONE			0x01B0
#define PLL_COMMON_STATUS_TWO			0x01B4
#define PLL_BAND_SEL_CAL			0x01B8
#define PLL_ICODE_ACCUM_STATUS_LOW		0x01BC
#define PLL_ICODE_ACCUM_STATUS_HIGH		0x01C0
#define PLL_FD_OUT_LOW				0x01C4
#define PLL_FD_OUT_HIGH				0x01C8
#define PLL_ALOG_OBSV_BUS_STATUS_1		0x01CC
#define PLL_PLL_MISC_CONFIG			0x01D0
#define PLL_FLL_CONFIG				0x01D4
#define PLL_FLL_FREQ_ACQ_TIME			0x01D8
#define PLL_FLL_CODE0				0x01DC
#define PLL_FLL_CODE1				0x01E0
#define PLL_FLL_GAIN0				0x01E4
#define PLL_FLL_GAIN1				0x01E8
#define PLL_SW_RESET				0x01EC
#define PLL_FAST_PWRUP				0x01F0
#define PLL_LOCKTIME0				0x01F4
#define PLL_LOCKTIME1				0x01F8
#define PLL_DEBUG_BUS_SEL			0x01FC
#define PLL_DEBUG_BUS0				0x0200
#define PLL_DEBUG_BUS1				0x0204
#define PLL_DEBUG_BUS2				0x0208
#define PLL_DEBUG_BUS3				0x020C
#define PLL_ANALOG_FLL_CONTROL_OVERRIDES	0x0210
#define PLL_VCO_CONFIG				0x0214
#define PLL_VCO_CAL_CODE1_MODE0_STATUS		0x0218
#define PLL_VCO_CAL_CODE1_MODE1_STATUS		0x021C
#define PLL_RESET_SM_STATUS			0x0220
#define PLL_TDC_OFFSET				0x0224
#define PLL_PS3_PWRDOWN_CONTROLS		0x0228
#define PLL_PS4_PWRDOWN_CONTROLS		0x022C
#define PLL_PLL_RST_CONTROLS			0x0230
#define PLL_GEAR_BAND_SELECT_CONTROLS		0x0234
#define PLL_PSM_CLK_CONTROLS			0x0238
#define PLL_SYSTEM_MUXES_2			0x023C
#define PLL_VCO_CONFIG_1			0x0240
#define PLL_VCO_CONFIG_2			0x0244
#define PLL_CLOCK_INVERTERS_1			0x0248
#define PLL_CLOCK_INVERTERS_2			0x024C
#define PLL_CMODE_1				0x0250
#define PLL_CMODE_2				0x0254
#define PLL_ANALOG_CONTROLS_FIVE_1		0x0258
#define PLL_ANALOG_CONTROLS_FIVE_2		0x025C
#define PLL_PERF_OPTIMIZE			0x0260

/* Register Offsets from PHY base address */
#define PHY_CMN_CLK_CFG0	0x010
#define PHY_CMN_CLK_CFG1	0x014
#define PHY_CMN_GLBL_CTRL	0x018
#define PHY_CMN_RBUF_CTRL	0x01C
#define PHY_CMN_CTRL_0		0x024
#define PHY_CMN_CTRL_2		0x02C
#define PHY_CMN_CTRL_3		0x030
#define PHY_CMN_PLL_CNTRL	0x03C
#define PHY_CMN_GLBL_DIGTOP_SPARE4 0x128

/* Bit definition of SSC control registers */
#define SSC_CENTER		BIT(0)
#define SSC_EN			BIT(1)
#define SSC_FREQ_UPDATE		BIT(2)
#define SSC_FREQ_UPDATE_MUX	BIT(3)
#define SSC_UPDATE_SSC		BIT(4)
#define SSC_UPDATE_SSC_MUX	BIT(5)
#define SSC_START		BIT(6)
#define SSC_START_MUX		BIT(7)

/* Dynamic Refresh Control Registers */
#define DSI_DYNAMIC_REFRESH_PLL_CTRL0		(0x014)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL1		(0x018)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL2		(0x01C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL3		(0x020)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL4		(0x024)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL5		(0x028)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL6		(0x02C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL7		(0x030)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL8		(0x034)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL9		(0x038)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL10		(0x03C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL11		(0x040)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL12		(0x044)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL13		(0x048)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL14		(0x04C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL15		(0x050)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL16		(0x054)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL17		(0x058)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL18		(0x05C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL19		(0x060)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL20		(0x064)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL21		(0x068)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL22		(0x06C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL23		(0x070)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL24		(0x074)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL25		(0x078)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL26		(0x07C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL27		(0x080)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL28		(0x084)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL29		(0x088)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL30		(0x08C)
#define DSI_DYNAMIC_REFRESH_PLL_CTRL31		(0x090)
#define DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR	(0x094)
#define DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR2	(0x098)

#define DSI_PHY_TO_PLL_OFFSET	(0x500)
enum {
	DSI_PLL_0,
	DSI_PLL_1,
	DSI_PLL_MAX
};

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

static inline int pll_reg_read(void *context, unsigned int reg,
					unsigned int *val)
{
	int rc = 0;
	u32 data;
	struct dsi_pll_resource *rsc = context;

	data = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CTRL_0);
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data | BIT(5));
	ndelay(250);

	*val = DSI_PLL_REG_R(rsc->pll_base, reg);

	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CTRL_0, data);

	return rc;
}

static inline int pll_reg_write(void *context, unsigned int reg,
					unsigned int val)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = context;

	DSI_PLL_REG_W(rsc->pll_base, reg, val);

	return rc;
}

static inline int phy_reg_read(void *context, unsigned int reg,
					unsigned int *val)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = context;

	*val = DSI_PLL_REG_R(rsc->phy_base, reg);

	return rc;
}

static inline int phy_reg_write(void *context, unsigned int reg,
					unsigned int val)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = context;

	DSI_PLL_REG_W(rsc->phy_base, reg, val);

	return rc;
}

static inline int phy_reg_update_bits_sub(struct dsi_pll_resource *rsc,
		unsigned int reg, unsigned int mask, unsigned int val)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(rsc->phy_base, reg);
	reg_val &= ~mask;
	reg_val |= (val & mask);
	DSI_PLL_REG_W(rsc->phy_base, reg, reg_val);

	return 0;
}

static inline int phy_reg_update_bits(void *context, unsigned int reg,
				unsigned int mask, unsigned int val)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = context;

	rc = phy_reg_update_bits_sub(rsc, reg, mask, val);
	if (!rc && rsc->slave)
		rc = phy_reg_update_bits_sub(rsc->slave, reg, mask, val);

	return rc;
}

static inline int pclk_mux_read_sel(void *context, unsigned int reg,
					unsigned int *val)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = context;

	/* Return cached cfg1 as its updated with cached cfg1 in pll_enable */
	if (!rsc->handoff_resources) {
		*val = (rsc->cached_cfg1) & 0x3;
		return rc;
	}

	*val = (DSI_PLL_REG_R(rsc->phy_base, reg) & 0x3);

	return rc;
}


static inline int pclk_mux_write_sel_sub(struct dsi_pll_resource *rsc,
				unsigned int reg, unsigned int val)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(rsc->phy_base, reg);
	reg_val &= ~0x03;
	reg_val |= val;

	DSI_PLL_REG_W(rsc->phy_base, reg, reg_val);

	return 0;
}

static inline int pclk_mux_write_sel(void *context, unsigned int reg,
					unsigned int val)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = context;
	struct dsi_pll_5nm *pll = rsc->priv;

	if (pll->cphy_enabled)
		WARN_ON("PHY is in CPHY mode. PLL config is incorrect\n");
	rc = pclk_mux_write_sel_sub(rsc, reg, val);
	if (!rc && rsc->slave)
		rc = pclk_mux_write_sel_sub(rsc->slave, reg, val);


	/*
	 * cache the current parent index for cases where parent
	 * is not changing but rate is changing. In that case
	 * clock framework won't call parent_set and hence dsiclk_sel
	 * bit won't be programmed. e.g. dfps update use case.
	 */
	rsc->cached_cfg1 = val;

	return rc;
}

static inline int cphy_pclk_mux_read_sel(void *context, unsigned int reg,
					 unsigned int *val)
{
	struct dsi_pll_resource *rsc = context;

	*val = (DSI_PLL_REG_R(rsc->phy_base, reg) & 0x3);

	return 0;
}

static inline int cphy_pclk_mux_write_sel(void *context, unsigned int reg,
					  unsigned int val)
{
	int rc = 0;
	struct dsi_pll_resource *rsc = context;
	struct dsi_pll_5nm *pll = rsc->priv;

	if (!pll->cphy_enabled)
		WARN_ON("PHY-> not in CPHY mode. PLL config is incorrect\n");
	/* For Cphy configuration, val should always be 3 */
	val = 3;
	rc = pclk_mux_write_sel_sub(rsc, reg, val);
	if (!rc && rsc->slave)
		rc = pclk_mux_write_sel_sub(rsc->slave, reg, val);

	/*
	 * cache the current parent index for cases where parent
	 * is not changing but rate is changing. In that case
	 * clock framework won't call parent_set and hence dsiclk_sel
	 * bit won't be programmed. e.g. dfps update use case.
	 */
	rsc->cached_cfg1 = val;

	return rc;
}

static int dsi_pll_5nm_get_gdsc_status(struct dsi_pll_resource *rsc)
{
	u32 reg = 0;
	bool status;

	reg = DSI_PLL_REG_R(rsc->gdsc_base, 0x0);
	status = reg & BIT(31);
	pr_err("reg:0x%x status:%d\n", reg, status);

	return status;
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
		pr_debug("slave PLL unavailable, assuming standalone config\n");
		return;
	}

	/* check to see if the source of DSI1 PLL bitclk is set to external */
	reg = DSI_PLL_REG_R(orsc->phy_base, PHY_CMN_CLK_CFG1);
	reg &= (BIT(2) | BIT(3));
	if (reg == 0x04)
		rsc->slave = pll_rsc_db[DSI_PLL_1]; /* external source */

	pr_debug("Slave PLL %s\n", rsc->slave ? "configured" : "absent");
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

	dsi_pll_config_slave(rsc);
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

static void dsi_pll_ssc_commit(struct dsi_pll_5nm *pll,
		struct dsi_pll_resource *rsc)
{
	void __iomem *pll_base = rsc->pll_base;
	struct dsi_pll_regs *regs = &pll->reg_setup;

	if (pll->pll_configuration.enable_ssc) {
		pr_debug("SSC is enabled\n");

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
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCKDET_RATE_1, 0x40);
	DSI_PLL_REG_W(pll_base, PLL_PLL_LOCK_DELAY, 0x06);
	DSI_PLL_REG_W(pll_base, PLL_CMODE_1,
			pll->cphy_enabled ? 0x00 : 0x10);
	DSI_PLL_REG_W(pll_base, PLL_CLOCK_INVERTERS_1,
			reg->pll_clock_inverters);
}

static int vco_5nm_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct dsi_pll_resource *rsc = vco->priv;
	struct dsi_pll_5nm *pll;

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
	rsc->dfps_trigger = false;

	dsi_pll_init_val(rsc);

	dsi_pll_detect_phy_mode(pll, rsc);

	dsi_pll_setup_config(pll, rsc);

	dsi_pll_calc_dec_frac(pll, rsc);

	dsi_pll_calc_ssc(pll, rsc);

	dsi_pll_commit(pll, rsc);

	dsi_pll_config_hzindep_reg(pll, rsc);

	dsi_pll_ssc_commit(pll, rsc);

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

		pr_debug("valid=%d vco_rate=%d, code %d %d %d\n",
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

	pr_debug("trim_code_0=0x%x trim_code_1=0x%x trim_code_2=0x%x\n",
			pll_res->cache_pll_trim_codes[0],
			pll_res->cache_pll_trim_codes[1],
			pll_res->cache_pll_trim_codes[2]);

	return 0;
}

static void shadow_dsi_pll_dynamic_refresh_5nm(struct dsi_pll_5nm *pll,
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
	upper_addr |=
		(upper_8_bit(PLL_CMODE_1 + offset) << 12);
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

static int shadow_vco_5nm_set_rate(struct clk_hw *hw, unsigned long rate,
			unsigned long parent_rate)
{
	int rc;
	struct dsi_pll_5nm *pll;
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct dsi_pll_resource *rsc = vco->priv;

	if (!rsc) {
		pr_err("pll resource not found\n");
		return -EINVAL;
	}

	pll = rsc->priv;
	if (!pll) {
		pr_err("pll configuration not found\n");
		return -EINVAL;
	}

	rc = dsi_pll_read_stored_trim_codes(rsc, rate);
	if (rc) {
		pr_err("cannot find pll codes rate=%ld\n", rate);
		return -EINVAL;
	}
	pr_debug("ndx=%d, rate=%lu\n", rsc->index, rate);


	rsc->vco_current_rate = rate;
	rsc->vco_ref_clk_rate = vco->ref_clk_rate;

	dsi_pll_setup_config(pll, rsc);

	dsi_pll_calc_dec_frac(pll, rsc);

	/* program dynamic refresh control registers */
	shadow_dsi_pll_dynamic_refresh_5nm(pll, rsc);

	/* update cached vco rate */
	rsc->vco_cached_rate = rate;
	rsc->dfps_trigger = true;

	return 0;
}

static int dsi_pll_5nm_lock_status(struct dsi_pll_resource *pll)
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
	if (rc && !pll->handoff_resources)
		pr_err("DSI PLL(%d) lock failed, status=0x%08x\n",
			pll->index, status);

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

static int dsi_pll_enable(struct dsi_pll_vco_clk *vco)
{
	int rc;
	struct dsi_pll_resource *rsc = vco->priv;
	struct dsi_pll_5nm *pll = rsc->priv;

	dsi_pll_enable_pll_bias(rsc);
	if (rsc->slave)
		dsi_pll_enable_pll_bias(rsc->slave);

	/* For Cphy configuration, pclk_mux is always set to 3 divider */
	if (pll->cphy_enabled) {
		rsc->cached_cfg1 |= 0x3;
		if (rsc->slave)
			rsc->slave->cached_cfg1 |= 0x3;
	}

	phy_reg_update_bits_sub(rsc, PHY_CMN_CLK_CFG1, 0x03, rsc->cached_cfg1);
	if (rsc->slave)
		phy_reg_update_bits_sub(rsc->slave, PHY_CMN_CLK_CFG1,
				0x03, rsc->slave->cached_cfg1);
	wmb(); /* ensure dsiclk_sel is always programmed before pll start */

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
		pr_err("PLL(%d) lock failed\n", rsc->index);
		goto error;
	}

	rsc->pll_on = true;

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

error:
	return rc;
}

static void dsi_pll_disable_sub(struct dsi_pll_resource *rsc)
{
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_RBUF_CTRL, 0);
	dsi_pll_disable_pll_bias(rsc);
}

static void dsi_pll_disable(struct dsi_pll_vco_clk *vco)
{
	struct dsi_pll_resource *rsc = vco->priv;

	if (!rsc->pll_on) {
		pr_err("failed to enable pll (%d) resources\n", rsc->index);
		return;
	}

	rsc->handoff_resources = false;
	rsc->dfps_trigger = false;

	pr_debug("stop PLL (%d)\n", rsc->index);

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
	rsc->pll_on = false;
}

long vco_5nm_round_rate(struct clk_hw *hw, unsigned long rate,
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

static void vco_5nm_unprepare(struct clk_hw *hw)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct dsi_pll_resource *pll = vco->priv;

	if (!pll) {
		pr_err("dsi pll resources not available\n");
		return;
	}

	/*
	 * During unprepare in continuous splash use case we want driver
	 * to pick all dividers instead of retaining bootloader configurations.
	 * Also handle the usecases when dynamic refresh gets triggered while
	 * handoff_resources flag is still set. For video mode, this flag does
	 * not get cleared until first suspend. Whereas for command mode, it
	 * doesnt get cleared until first idle power collapse. We need to make
	 * sure that we save and restore the divider settings when dynamic FPS
	 * is triggered.
	 */
	if (!pll->handoff_resources || pll->dfps_trigger) {
		pll->cached_cfg0 = DSI_PLL_REG_R(pll->phy_base,
						  PHY_CMN_CLK_CFG0);
		pll->cached_outdiv = DSI_PLL_REG_R(pll->pll_base,
						    PLL_PLL_OUTDIV_RATE);
		pr_debug("cfg0=%d,cfg1=%d, outdiv=%d\n", pll->cached_cfg0,
			 pll->cached_cfg1, pll->cached_outdiv);

		pll->vco_cached_rate = clk_get_rate(hw->clk);
	}

	/*
	 * When continuous splash screen feature is enabled, we need to cache
	 * the mux configuration for the pixel_clk_src mux clock. The clock
	 * framework does not call back to re-configure the mux value if it is
	 * does not change.For such usecases, we need to ensure that the cached
	 * value is programmed prior to PLL being locked
	 */
	if (pll->handoff_resources) {
		pll->cached_cfg1 = DSI_PLL_REG_R(pll->phy_base,
						  PHY_CMN_CLK_CFG1);
		if (pll->slave)
			pll->slave->cached_cfg1 =
				DSI_PLL_REG_R(pll->slave->phy_base,
					       PHY_CMN_CLK_CFG1);
	}

	dsi_pll_disable(vco);
}

static int vco_5nm_prepare(struct clk_hw *hw)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct dsi_pll_resource *pll = vco->priv;

	if (!pll) {
		pr_err("dsi pll resources are not available\n");
		return -EINVAL;
	}

	/* Skip vco recalculation for continuous splash use case */
	if (pll->handoff_resources) {
		pll->pll_on = true;
		return 0;
	}

	if ((pll->vco_cached_rate != 0) &&
	    (pll->vco_cached_rate == clk_hw_get_rate(hw))) {
		rc = vco_5nm_set_rate(hw, pll->vco_cached_rate,
				pll->vco_cached_rate);
		if (rc) {
			pr_err("pll(%d) set_rate failed, rc=%d\n",
			       pll->index, rc);
			return rc;
		}
		pr_debug("cfg0=%d, cfg1=%d\n", pll->cached_cfg0,
			pll->cached_cfg1);
		DSI_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG0,
					pll->cached_cfg0);
		if (pll->slave)
			DSI_PLL_REG_W(pll->slave->phy_base, PHY_CMN_CLK_CFG0,
				       pll->cached_cfg0);
		DSI_PLL_REG_W(pll->pll_base, PLL_PLL_OUTDIV_RATE,
					pll->cached_outdiv);
	}

	rc = dsi_pll_enable(vco);

	return rc;
}

static unsigned long vco_5nm_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	int rc = 0;
	struct dsi_pll_vco_clk *vco = to_vco_clk_hw(hw);
	struct dsi_pll_resource *pll = vco->priv;

	if (!vco->priv) {
		pr_err("vco priv is null\n");
		return 0;
	}

	if (!pll->priv) {
		pr_err("pll priv is null\n");
		return 0;
	}
	/*
	 * In the case when vco arte is set, the recalculation function should
	 * return the current rate as to avoid trying to set the vco rate
	 * again. However durng handoff, recalculation should set the flag
	 * according to the status of PLL.
	*/
	if (pll->vco_current_rate != 0) {
		pr_debug("returning vco rate = %lld\n", pll->vco_current_rate);
		return pll->vco_current_rate;
	}

	pll->handoff_resources = true;


	if (!dsi_pll_5nm_get_gdsc_status(pll)) {
		pll->handoff_resources = false;
		pr_err("Hand_off_resources not needed since gdsc is off\n");
		return 0;
	}

	dsi_pll_detect_phy_mode(pll->priv, pll);

	if (dsi_pll_5nm_lock_status(pll)) {
		pr_err("PLL not enabled\n");
		pll->handoff_resources = false;
	}
	pr_err("handoff_resources %s\n", pll->handoff_resources ? "true" : "false");

	return rc;
}

static int pixel_clk_get_div(void *context, unsigned int reg, unsigned int *div)
{
	struct dsi_pll_resource *pll = context;
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	*div = (reg_val & 0xF0) >> 4;

	return 0;
}

static void pixel_clk_set_div_sub(struct dsi_pll_resource *pll, int div)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	reg_val &= ~0xF0;
	reg_val |= (div << 4);
	DSI_PLL_REG_W(pll->phy_base, PHY_CMN_CLK_CFG0, reg_val);

	/*
	 * cache the current parent index for cases where parent
	 * is not changing but rate is changing. In that case
	 * clock framework won't call parent_set and hence dsiclk_sel
	 * bit won't be programmed. e.g. dfps update use case.
	 */
	pll->cached_cfg0 = reg_val;
}

static int pixel_clk_set_div(void *context, unsigned int reg, unsigned int div)
{
	struct dsi_pll_resource *pll = context;

	pixel_clk_set_div_sub(pll, div);
	if (pll->slave)
		pixel_clk_set_div_sub(pll->slave, div);

	return 0;
}

static int bit_clk_get_div(void *context, unsigned int reg, unsigned int *div)
{
	struct dsi_pll_resource *pll = context;
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(pll->phy_base, PHY_CMN_CLK_CFG0);
	*div = (reg_val & 0x0F);

	return 0;
}

static void bit_clk_set_div_sub(struct dsi_pll_resource *rsc, int div)
{
	u32 reg_val;

	reg_val = DSI_PLL_REG_R(rsc->phy_base, PHY_CMN_CLK_CFG0);
	reg_val &= ~0x0F;
	reg_val |= div;
	DSI_PLL_REG_W(rsc->phy_base, PHY_CMN_CLK_CFG0, reg_val);
}

static int bit_clk_set_div(void *context, unsigned int reg, unsigned int div)
{
	struct dsi_pll_resource *rsc = context;

	if (!rsc) {
		pr_err("pll resource not found\n");
		return -EINVAL;
	}

	bit_clk_set_div_sub(rsc, div);
	/* For slave PLL, this divider always should be set to 1 */
	if (rsc->slave)
		bit_clk_set_div_sub(rsc->slave, 1);

	return 0;
}

static struct regmap_config dsi_pll_5nm_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x7c0,
};

static struct regmap_bus pll_regmap_bus = {
	.reg_write = pll_reg_write,
	.reg_read = pll_reg_read,
};

static struct regmap_bus pclk_src_mux_regmap_bus = {
	.reg_read = pclk_mux_read_sel,
	.reg_write = pclk_mux_write_sel,
};

static struct regmap_bus cphy_pclk_src_mux_regmap_bus = {
	.reg_read = cphy_pclk_mux_read_sel,
	.reg_write = cphy_pclk_mux_write_sel,
};

static struct regmap_bus pclk_src_regmap_bus = {
	.reg_write = pixel_clk_set_div,
	.reg_read = pixel_clk_get_div,
};

static struct regmap_bus bitclk_src_regmap_bus = {
	.reg_write = bit_clk_set_div,
	.reg_read = bit_clk_get_div,
};

static const struct clk_ops clk_ops_vco_5nm = {
	.recalc_rate = vco_5nm_recalc_rate,
	.set_rate = vco_5nm_set_rate,
	.round_rate = vco_5nm_round_rate,
	.prepare = vco_5nm_prepare,
	.unprepare = vco_5nm_unprepare,
};

static const struct clk_ops clk_ops_shadow_vco_5nm = {
	.recalc_rate = vco_5nm_recalc_rate,
	.set_rate = shadow_vco_5nm_set_rate,
	.round_rate = vco_5nm_round_rate,
};

static struct regmap_bus dsi_mux_regmap_bus = {
	.reg_write = dsi_set_mux_sel,
	.reg_read = dsi_get_mux_sel,
};

/*
 * Clock tree for generating DSI byte and pclk.
 *
 *
 *                  +---------------+
 *                  |    vco_clk    |
 *                  +-------+-------+
 *                          |
 *                          |
 *                  +---------------+
 *                  |  pll_out_div  |
 *                  |  DIV(1,2,4,8) |
 *                  +-------+-------+
 *                          |
 *                          +-----------------------------+-------+---------------+
 *                          |                             |       |               |
 *                  +-------v-------+                     |       |               |
 *                  |  bitclk_src   |                                             |
 *                  |  DIV(1..15)   |               Not supported for DPHY        |
 *                  +-------+-------+                                             |
 *                          |                             |       |               |
 *            +-------------v+---------+---------+        |       |               |
 *            |              |         |         |        |       |               |
 *      +-----v-----+  +-----v-----+   |  +------v------+ | +-----v------+  +-----v------+
 *      |byteclk_src|  |byteclk_src|   |  |post_bit_div | | |post_vco_div|  |post_vco_div|
 *      |  DIV(8)   |  |  DIV(7)   |   |  |   DIV (2)   | | |   DIV(4)   |  |  DIV(3.5)  |
 *      +-----+-----+  +-----+-----+   |  +------+------+ | +-----+------+  +------+-----+
 *            |              |         |         |        |       |                |
 *Shadow Path |          CPHY Path     |         |        |       |           +----v
 *     +      |              |         +------+  |        |   +---+           |
 *     +---+  |        +-----+                |  |        |   |               |
 *         |  |        |                    +-v--v----v---v---+      +--------v--------+
 *     +---v--v--------v---+                 \  pclk_src_mux /        \ cphy_pclk_src /
 *      \   byteclk_mux   /                   \             /          \     mux     /
 *       \               /                     +-----+-----+            +-----+-----+
 *        +------+------+                            |      Shadow Path       |
 *               |                                   |           +            |
 *               v                             +-----v------+    |     +------v------+
 *         dsi_byte_clk                        |  pclk_src  |    |     |cphy_pclk_src|
 *                                             | DIV(1..15) |    |     |  DIV(1..15) |
 *                                             +-----+------+    |     +------+------+
 *                                                   |           |            |
 *                                                   |           |        CPHY Path
 *                                                   |           |            |
 *                                                   +-------+   |    +-------+
 *                                                           |   |    |
 *                                                       +---v---v----v------+
 *                                                        \     pclk_mux    /
 *                                                          +------+------+
 *                                                                 |
 *                                                                 v
 *                                                              dsi_pclk
 *
 */

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 3500000000UL,
	.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_vco_clk",
			.parent_names = (const char *[]){"bi_tcxo"},
			.num_parents = 1,
			.ops = &clk_ops_vco_5nm,
	},
};

static struct dsi_pll_vco_clk dsi0pll_shadow_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 3500000000UL,
	.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_vco_clk",
			.parent_names = (const char *[]){"bi_tcxo"},
			.num_parents = 1,
			.ops = &clk_ops_shadow_vco_5nm,
	},
};

static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 3500000000UL,
	.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_vco_clk",
			.parent_names = (const char *[]){"bi_tcxo"},
			.num_parents = 1,
			.ops = &clk_ops_vco_5nm,
	},
};

static struct dsi_pll_vco_clk dsi1pll_shadow_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 3500000000UL,
	.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_vco_clk",
			.parent_names = (const char *[]){"bi_tcxo"},
			.num_parents = 1,
			.ops = &clk_ops_shadow_vco_5nm,
	},
};

static struct clk_regmap_div dsi0pll_pll_out_div = {
	.reg = PLL_PLL_OUTDIV_RATE,
	.shift = 0,
	.width = 2,
	.flags = CLK_DIVIDER_POWER_OF_TWO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_pll_out_div",
			.parent_names = (const char *[]){"dsi0pll_vco_clk"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_shadow_pll_out_div = {
	.reg = PLL_PLL_OUTDIV_RATE,
	.shift = 0,
	.width = 2,
	.flags = CLK_DIVIDER_POWER_OF_TWO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_pll_out_div",
			.parent_names = (const char *[]){
				"dsi0pll_shadow_vco_clk"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_pll_out_div = {
	.reg = PLL_PLL_OUTDIV_RATE,
	.shift = 0,
	.width = 2,
	.flags = CLK_DIVIDER_POWER_OF_TWO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_pll_out_div",
			.parent_names = (const char *[]){"dsi1pll_vco_clk"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_shadow_pll_out_div = {
	.reg = PLL_PLL_OUTDIV_RATE,
	.shift = 0,
	.width = 2,
	.flags = CLK_DIVIDER_POWER_OF_TWO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_pll_out_div",
			.parent_names = (const char *[]){
				"dsi1pll_shadow_vco_clk"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_bitclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_bitclk_src",
			.parent_names = (const char *[]){"dsi0pll_pll_out_div"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_shadow_bitclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_bitclk_src",
			.parent_names = (const char *[]){
				"dsi0pll_shadow_pll_out_div"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_bitclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_bitclk_src",
			.parent_names = (const char *[]){"dsi1pll_pll_out_div"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_shadow_bitclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_bitclk_src",
			.parent_names = (const char *[]){
				"dsi1pll_shadow_pll_out_div"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_fixed_factor dsi0pll_post_vco_div = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_vco_div",
		.parent_names = (const char *[]){"dsi0pll_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_shadow_post_vco_div = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_shadow_post_vco_div",
		.parent_names = (const char *[]){"dsi0pll_shadow_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_vco_div = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_vco_div",
		.parent_names = (const char *[]){"dsi1pll_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_post_vco_div3_5 = {
	.div = 7,
	.mult = 2,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_vco_div3_5",
		.parent_names = (const char *[]){"dsi0pll_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_shadow_post_vco_div3_5 = {
	.div = 7,
	.mult = 2,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_shadow_post_vco_div3_5",
		.parent_names = (const char *[]){"dsi0pll_shadow_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_vco_div3_5 = {
	.div = 7,
	.mult = 2,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_vco_div3_5",
		.parent_names = (const char *[]){"dsi1pll_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_shadow_post_vco_div3_5 = {
	.div = 7,
	.mult = 2,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_shadow_post_vco_div3_5",
		.parent_names = (const char *[]){"dsi1pll_shadow_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_shadow_post_vco_div = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_shadow_post_vco_div",
		.parent_names = (const char *[]){"dsi1pll_shadow_pll_out_div"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_byteclk_src = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_byteclk_src",
		.parent_names = (const char *[]){"dsi0pll_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_shadow_byteclk_src = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_shadow_byteclk_src",
		.parent_names = (const char *[]){"dsi0pll_shadow_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_byteclk_src = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_byteclk_src",
		.parent_names = (const char *[]){"dsi1pll_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_cphy_byteclk_src = {
	.div = 7,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_cphy_byteclk_src",
		.parent_names = (const char *[]){"dsi0pll_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_shadow_cphy_byteclk_src = {
	.div = 7,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_shadow_cphy_byteclk_src",
		.parent_names = (const char *[]){"dsi0pll_shadow_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_cphy_byteclk_src = {
	.div = 7,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_cphy_byteclk_src",
		.parent_names = (const char *[]){"dsi1pll_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_shadow_cphy_byteclk_src = {
	.div = 7,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_shadow_cphy_byteclk_src",
		.parent_names = (const char *[]){"dsi1pll_shadow_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_shadow_byteclk_src = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_shadow_byteclk_src",
		.parent_names = (const char *[]){"dsi1pll_shadow_bitclk_src"},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_post_bit_div = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_bit_div",
		.parent_names = (const char *[]){"dsi0pll_bitclk_src"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_shadow_post_bit_div = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_shadow_post_bit_div",
		.parent_names = (const char *[]){"dsi0pll_shadow_bitclk_src"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_bit_div = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_bit_div",
		.parent_names = (const char *[]){"dsi1pll_bitclk_src"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_shadow_post_bit_div = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_shadow_post_bit_div",
		.parent_names = (const char *[]){"dsi1pll_shadow_bitclk_src"},
		.num_parents = 1,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi0pll_byteclk_mux = {
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0_phy_pll_out_byteclk",
			.parent_names = (const char *[]){"dsi0pll_byteclk_src",
				"dsi0pll_shadow_byteclk_src",
				"dsi0pll_cphy_byteclk_src",
				"dsi0pll_shadow_cphy_byteclk_src"},
			.num_parents = 4,
			.flags = (CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_byteclk_mux = {
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1_phy_pll_out_byteclk",
			.parent_names = (const char *[]){"dsi1pll_byteclk_src",
				"dsi1pll_shadow_byteclk_src",
				"dsi1pll_cphy_byteclk_src",
				"dsi1pll_shadow_cphy_byteclk_src"},
			.num_parents = 4,
			.flags = (CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi0pll_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_pclk_src_mux",
			.parent_names = (const char *[]){"dsi0pll_bitclk_src",
					"dsi0pll_post_bit_div"},
			.num_parents = 2,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi0pll_shadow_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_pclk_src_mux",
			.parent_names = (const char *[]){
				"dsi0pll_shadow_bitclk_src",
				"dsi0pll_shadow_post_bit_div"},
			.num_parents = 2,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi0pll_cphy_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 2,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_cphy_pclk_src_mux",
			.parent_names =
				(const char *[]){"dsi0pll_post_vco_div3_5"},
			.num_parents = 1,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi0pll_shadow_cphy_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 2,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_cphy_pclk_src_mux",
			.parent_names =
				(const char *[]){
					"dsi0pll_shadow_post_vco_div3_5"},
			.num_parents = 1,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_pclk_src_mux",
			.parent_names = (const char *[]){"dsi1pll_bitclk_src",
					"dsi1pll_post_bit_div"},
			.num_parents = 2,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_shadow_cphy_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 2,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_cphy_pclk_src_mux",
			.parent_names =
				(const char *[]){
					"dsi1pll_shadow_post_vco_div3_5"},
			.num_parents = 1,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_shadow_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_pclk_src_mux",
			.parent_names = (const char *[]){
				"dsi1pll_shadow_bitclk_src",
				"dsi1pll_shadow_post_bit_div"},
			.num_parents = 2,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_cphy_pclk_src_mux = {
	.reg = PHY_CMN_CLK_CFG1,
	.shift = 0,
	.width = 2,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_cphy_pclk_src_mux",
			.parent_names =
				(const char *[]){"dsi1pll_post_vco_div3_5"},
			.num_parents = 1,
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_pclk_src",
			.parent_names = (const char *[]){
					"dsi0pll_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_shadow_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_pclk_src",
			.parent_names = (const char *[]){
					"dsi0pll_shadow_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_cphy_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_cphy_pclk_src",
			.parent_names = (const char *[]){
				"dsi0pll_cphy_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_shadow_cphy_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_cphy_pclk_src",
			.parent_names = (const char *[]){
				"dsi0pll_shadow_cphy_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_pclk_src",
			.parent_names = (const char *[]){
					"dsi1pll_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_shadow_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_pclk_src",
			.parent_names = (const char *[]){
				"dsi1pll_shadow_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_cphy_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_cphy_pclk_src",
			.parent_names = (const char *[]){
				"dsi1pll_cphy_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_shadow_cphy_pclk_src = {
	.shift = 0,
	.width = 4,
	.flags = CLK_DIVIDER_ONE_BASED | CLK_DIVIDER_ALLOW_ZERO,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_cphy_pclk_src",
			.parent_names = (const char *[]){
				"dsi1pll_shadow_cphy_pclk_src_mux"},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_mux dsi0pll_pclk_mux = {
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0_phy_pll_out_dsiclk",
			.parent_names = (const char *[]){"dsi0pll_pclk_src",
				"dsi0pll_shadow_pclk_src",
				"dsi0pll_cphy_pclk_src",
				"dsi0pll_shadow_cphy_pclk_src"},
			.num_parents = 4,
			.flags = (CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_pclk_mux = {
	.shift = 0,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1_phy_pll_out_dsiclk",
			.parent_names = (const char *[]){"dsi1pll_pclk_src",
				"dsi1pll_shadow_pclk_src",
				"dsi1pll_cphy_pclk_src",
				"dsi1pll_shadow_cphy_pclk_src"},
			.num_parents = 4,
			.flags = (CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_hw *dsi_pllcc_5nm[] = {
	[VCO_CLK_0] = &dsi0pll_vco_clk.hw,
	[PLL_OUT_DIV_0_CLK] = &dsi0pll_pll_out_div.clkr.hw,
	[BITCLK_SRC_0_CLK] = &dsi0pll_bitclk_src.clkr.hw,
	[BYTECLK_SRC_0_CLK] = &dsi0pll_byteclk_src.hw,
	[CPHY_BYTECLK_SRC_0_CLK] = &dsi0pll_cphy_byteclk_src.hw,
	[POST_BIT_DIV_0_CLK] = &dsi0pll_post_bit_div.hw,
	[POST_VCO_DIV_0_CLK] = &dsi0pll_post_vco_div.hw,
	[POST_VCO_DIV3_5_0_CLK] = &dsi0pll_post_vco_div3_5.hw,
	[BYTECLK_MUX_0_CLK] = &dsi0pll_byteclk_mux.clkr.hw,
	[PCLK_SRC_MUX_0_CLK] = &dsi0pll_pclk_src_mux.clkr.hw,
	[PCLK_SRC_0_CLK] = &dsi0pll_pclk_src.clkr.hw,
	[PCLK_MUX_0_CLK] = &dsi0pll_pclk_mux.clkr.hw,
	[CPHY_PCLK_SRC_MUX_0_CLK] = &dsi0pll_cphy_pclk_src_mux.clkr.hw,
	[CPHY_PCLK_SRC_0_CLK] = &dsi0pll_cphy_pclk_src.clkr.hw,
	[SHADOW_VCO_CLK_0] = &dsi0pll_shadow_vco_clk.hw,
	[SHADOW_PLL_OUT_DIV_0_CLK] = &dsi0pll_shadow_pll_out_div.clkr.hw,
	[SHADOW_BITCLK_SRC_0_CLK] = &dsi0pll_shadow_bitclk_src.clkr.hw,
	[SHADOW_BYTECLK_SRC_0_CLK] = &dsi0pll_shadow_byteclk_src.hw,
	[SHADOW_CPHY_BYTECLK_SRC_0_CLK] = &dsi0pll_shadow_cphy_byteclk_src.hw,
	[SHADOW_POST_BIT_DIV_0_CLK] = &dsi0pll_shadow_post_bit_div.hw,
	[SHADOW_POST_VCO_DIV_0_CLK] = &dsi0pll_shadow_post_vco_div.hw,
	[SHADOW_POST_VCO_DIV3_5_0_CLK] = &dsi0pll_shadow_post_vco_div3_5.hw,
	[SHADOW_PCLK_SRC_MUX_0_CLK] = &dsi0pll_shadow_pclk_src_mux.clkr.hw,
	[SHADOW_PCLK_SRC_0_CLK] = &dsi0pll_shadow_pclk_src.clkr.hw,
	[SHADOW_CPHY_PCLK_SRC_MUX_0_CLK] =
			&dsi0pll_shadow_cphy_pclk_src_mux.clkr.hw,
	[SHADOW_CPHY_PCLK_SRC_0_CLK] = &dsi0pll_shadow_cphy_pclk_src.clkr.hw,
	[VCO_CLK_1] = &dsi1pll_vco_clk.hw,
	[PLL_OUT_DIV_1_CLK] = &dsi1pll_pll_out_div.clkr.hw,
	[BITCLK_SRC_1_CLK] = &dsi1pll_bitclk_src.clkr.hw,
	[BYTECLK_SRC_1_CLK] = &dsi1pll_byteclk_src.hw,
	[CPHY_BYTECLK_SRC_1_CLK] = &dsi1pll_cphy_byteclk_src.hw,
	[POST_BIT_DIV_1_CLK] = &dsi1pll_post_bit_div.hw,
	[POST_VCO_DIV_1_CLK] = &dsi1pll_post_vco_div.hw,
	[POST_VCO_DIV3_5_1_CLK] = &dsi1pll_post_vco_div3_5.hw,
	[BYTECLK_MUX_1_CLK] = &dsi1pll_byteclk_mux.clkr.hw,
	[PCLK_SRC_MUX_1_CLK] = &dsi1pll_pclk_src_mux.clkr.hw,
	[PCLK_SRC_1_CLK] = &dsi1pll_pclk_src.clkr.hw,
	[PCLK_MUX_1_CLK] = &dsi1pll_pclk_mux.clkr.hw,
	[CPHY_PCLK_SRC_MUX_1_CLK] = &dsi1pll_cphy_pclk_src_mux.clkr.hw,
	[CPHY_PCLK_SRC_1_CLK] = &dsi1pll_cphy_pclk_src.clkr.hw,
	[SHADOW_VCO_CLK_1] = &dsi1pll_shadow_vco_clk.hw,
	[SHADOW_PLL_OUT_DIV_1_CLK] = &dsi1pll_shadow_pll_out_div.clkr.hw,
	[SHADOW_BITCLK_SRC_1_CLK] = &dsi1pll_shadow_bitclk_src.clkr.hw,
	[SHADOW_BYTECLK_SRC_1_CLK] = &dsi1pll_shadow_byteclk_src.hw,
	[SHADOW_CPHY_BYTECLK_SRC_1_CLK] = &dsi1pll_shadow_cphy_byteclk_src.hw,
	[SHADOW_POST_BIT_DIV_1_CLK] = &dsi1pll_shadow_post_bit_div.hw,
	[SHADOW_POST_VCO_DIV_1_CLK] = &dsi1pll_shadow_post_vco_div.hw,
	[SHADOW_POST_VCO_DIV3_5_1_CLK] = &dsi1pll_shadow_post_vco_div3_5.hw,
	[SHADOW_PCLK_SRC_MUX_1_CLK] = &dsi1pll_shadow_pclk_src_mux.clkr.hw,
	[SHADOW_PCLK_SRC_1_CLK] = &dsi1pll_shadow_pclk_src.clkr.hw,
	[SHADOW_CPHY_PCLK_SRC_MUX_1_CLK] =
			&dsi1pll_shadow_cphy_pclk_src_mux.clkr.hw,
	[SHADOW_CPHY_PCLK_SRC_1_CLK] = &dsi1pll_shadow_cphy_pclk_src.clkr.hw,
};

int dsi_pll_clock_register_5nm(struct platform_device *pdev,
				  struct dsi_pll_resource *pll_res)
{
	int rc = 0, ndx, i;
	struct clk *clk;
	struct clk_onecell_data *clk_data;
	int num_clks = ARRAY_SIZE(dsi_pllcc_5nm);
	struct regmap *rmap;
	struct regmap_config *rmap_config;

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
	plls[ndx].rsc = pll_res;
	pll_res->priv = &plls[ndx];
	pll_res->vco_delay = VCO_DELAY_USEC;

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
					GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
				sizeof(struct clk *)), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = num_clks;

	rmap_config = devm_kmemdup(&pdev->dev, &dsi_pll_5nm_config,
			sizeof(struct regmap_config), GFP_KERNEL);
	if (!rmap_config)
		return -ENOMEM;

	/* Establish client data */
	if (ndx == 0) {
		rmap_config->name = "pll_out";
		rmap = devm_regmap_init(&pdev->dev, &pll_regmap_bus,
				pll_res, rmap_config);
		dsi0pll_pll_out_div.clkr.regmap = rmap;
		dsi0pll_shadow_pll_out_div.clkr.regmap = rmap;

		rmap_config->name = "bitclk_src";
		rmap = devm_regmap_init(&pdev->dev, &bitclk_src_regmap_bus,
				pll_res, rmap_config);
		dsi0pll_bitclk_src.clkr.regmap = rmap;
		dsi0pll_shadow_bitclk_src.clkr.regmap = rmap;

		rmap_config->name = "pclk_src";
		rmap = devm_regmap_init(&pdev->dev, &pclk_src_regmap_bus,
				pll_res, rmap_config);
		dsi0pll_pclk_src.clkr.regmap = rmap;
		dsi0pll_cphy_pclk_src.clkr.regmap = rmap;
		dsi0pll_shadow_pclk_src.clkr.regmap = rmap;
		dsi0pll_shadow_cphy_pclk_src.clkr.regmap = rmap;

		rmap_config->name = "pclk_mux";
		rmap = devm_regmap_init(&pdev->dev, &dsi_mux_regmap_bus,
				pll_res, rmap_config);
		dsi0pll_pclk_mux.clkr.regmap = rmap;

		rmap_config->name = "pclk_src_mux";
		rmap = devm_regmap_init(&pdev->dev, &pclk_src_mux_regmap_bus,
				pll_res, rmap_config);
		dsi0pll_pclk_src_mux.clkr.regmap = rmap;
		dsi0pll_shadow_pclk_src_mux.clkr.regmap = rmap;

		rmap_config->name = "cphy_pclk_src_mux";
		rmap = devm_regmap_init(&pdev->dev,
				&cphy_pclk_src_mux_regmap_bus,
				pll_res, rmap_config);
		dsi0pll_cphy_pclk_src_mux.clkr.regmap = rmap;
		dsi0pll_shadow_cphy_pclk_src_mux.clkr.regmap = rmap;

		rmap_config->name = "byteclk_mux";
		rmap = devm_regmap_init(&pdev->dev, &dsi_mux_regmap_bus,
				pll_res, rmap_config);
		dsi0pll_byteclk_mux.clkr.regmap = rmap;

		dsi0pll_vco_clk.priv = pll_res;
		dsi0pll_shadow_vco_clk.priv = pll_res;

		if (dsi_pll_5nm_is_hw_revision(pll_res)) {
			dsi0pll_vco_clk.min_rate = 600000000;
			dsi0pll_vco_clk.max_rate = 5000000000;
			dsi0pll_shadow_vco_clk.min_rate = 600000000;
			dsi0pll_shadow_vco_clk.max_rate = 5000000000;
		}

		for (i = VCO_CLK_0; i <= SHADOW_CPHY_PCLK_SRC_0_CLK; i++) {
			clk = devm_clk_register(&pdev->dev,
						dsi_pllcc_5nm[i]);
			if (IS_ERR(clk)) {
				pr_err("clk registration failed for DSI clock:%d\n",
							pll_res->index);
				rc = -EINVAL;
				goto clk_register_fail;
			}
			clk_data->clks[i] = clk;

		}

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	} else {
		rmap_config->name = "pll_out";
		rmap = devm_regmap_init(&pdev->dev, &pll_regmap_bus,
				pll_res, rmap_config);
		dsi1pll_pll_out_div.clkr.regmap = rmap;
		dsi1pll_shadow_pll_out_div.clkr.regmap = rmap;

		rmap_config->name = "bitclk_src";
		rmap = devm_regmap_init(&pdev->dev, &bitclk_src_regmap_bus,
				pll_res, rmap_config);
		dsi1pll_bitclk_src.clkr.regmap = rmap;
		dsi1pll_shadow_bitclk_src.clkr.regmap = rmap;

		rmap_config->name = "pclk_src";
		rmap = devm_regmap_init(&pdev->dev, &pclk_src_regmap_bus,
				pll_res, rmap_config);
		dsi1pll_pclk_src.clkr.regmap = rmap;
		dsi1pll_cphy_pclk_src.clkr.regmap = rmap;
		dsi1pll_shadow_pclk_src.clkr.regmap = rmap;
		dsi1pll_shadow_cphy_pclk_src.clkr.regmap = rmap;

		rmap_config->name = "pclk_mux";
		rmap = devm_regmap_init(&pdev->dev, &dsi_mux_regmap_bus,
				pll_res, rmap_config);
		dsi1pll_pclk_mux.clkr.regmap = rmap;

		rmap_config->name = "pclk_src_mux";
		rmap = devm_regmap_init(&pdev->dev, &pclk_src_mux_regmap_bus,
				pll_res, rmap_config);
		dsi1pll_pclk_src_mux.clkr.regmap = rmap;
		dsi1pll_shadow_pclk_src_mux.clkr.regmap = rmap;
		dsi1pll_shadow_cphy_pclk_src_mux.clkr.regmap = rmap;

		rmap_config->name = "cphy_pclk_src_mux";
		rmap = devm_regmap_init(&pdev->dev,
				&cphy_pclk_src_mux_regmap_bus,
				pll_res, rmap_config);
		dsi1pll_cphy_pclk_src_mux.clkr.regmap = rmap;

		rmap_config->name = "byteclk_mut";
		rmap = devm_regmap_init(&pdev->dev, &dsi_mux_regmap_bus,
				pll_res, rmap_config);
		dsi1pll_byteclk_mux.clkr.regmap = rmap;

		dsi1pll_vco_clk.priv = pll_res;
		dsi1pll_shadow_vco_clk.priv = pll_res;

		if (dsi_pll_5nm_is_hw_revision(pll_res)) {
			dsi1pll_vco_clk.min_rate = 600000000;
			dsi1pll_vco_clk.max_rate = 5000000000;
			dsi1pll_shadow_vco_clk.min_rate = 600000000;
			dsi1pll_shadow_vco_clk.max_rate = 5000000000;
		}

		for (i = VCO_CLK_1; i <= CPHY_PCLK_SRC_1_CLK; i++) {
			clk = devm_clk_register(&pdev->dev,
						dsi_pllcc_5nm[i]);
			if (IS_ERR(clk)) {
				pr_err("clk registration failed for DSI clock:%d\n",
						pll_res->index);
				rc = -EINVAL;
				goto clk_register_fail;
			}
			clk_data->clks[i] = clk;

		}

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	}
	if (!rc) {
		pr_info("Registered DSI PLL ndx=%d, clocks successfully\n",
				ndx);

		return rc;
	}
clk_register_fail:
	return rc;
}
