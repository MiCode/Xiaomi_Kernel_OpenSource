// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <asm/div64.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/string.h>

#include "sprd_dphy.h"

#define L						0
#define H						1
#define CLK						0
#define DATA					1
#define INFINITY				0xffffffff
#define MIN_OUTPUT_FREQ			(100)

#define AVERAGE(a, b) (min(a, b) + abs((b) - (a)) / 2)

enum TIMING {
	NONE,
	REQUEST_TIME,
	PREPARE_TIME,
	SETTLE_TIME,
	ZERO_TIME,
	TRAIL_TIME,
	EXIT_TIME,
	CLKPOST_TIME,
	TA_GET,
	TA_GO,
	TA_SURE,
	TA_WAIT,
};

struct pll_regs {
	union __reg_03__ {
		struct __03 {
			u8 prbs_bist: 1;
			u8 en_lp_treot: 1;
			u8 lpf_sel: 4;
			u8 txfifo_bypass: 1;
			u8 freq_hopping: 1;
		} bits;
		u8 val;
	} _03;
	union __reg_04__ {
		struct __04 {
			u8 div: 3;
			u8 masterof8lane: 1;
			u8 hop_trig: 1;
			u8 cp_s: 2;
			u8 fdk_s: 1;
		} bits;
		u8 val;
	} _04;
	union __reg_06__ {
		struct __06 {
			u8 nint: 7;
			u8 mod_en: 1;
		} bits;
		u8 val;
	} _06;
	union __reg_07__ {
		struct __07 {
			u8 kdelta_h: 8;
		} bits;
		u8 val;
	} _07;
	union __reg_08__ {
		struct __08 {
			u8 vco_band: 1;
			u8 sdm_en: 1;
			u8 refin: 2;
			u8 kdelta_l: 4;
		} bits;
		u8 val;
	} _08;
	union __reg_09__ {
		struct __09 {
			u8 kint_h: 8;
		} bits;
		u8 val;
	} _09;
	union __reg_0a__ {
		struct __0a {
			u8 kint_m: 8;
		} bits;
		u8 val;
	} _0a;
	union __reg_0b__ {
		struct __0b {
			u8 out_sel: 4;
			u8 kint_l: 4;
		} bits;
		u8 val;
	} _0b;
	union __reg_0c__ {
		struct __0c {
			u8 kstep_h: 8;
		} bits;
		u8 val;
	} _0c;
	union __reg_0d__ {
		struct __0d {
			u8 kstep_m: 8;
		} bits;
		u8 val;
	} _0d;
	union __reg_0e__ {
		struct __0e {
			u8 pll_pu_byp: 1;
			u8 pll_pu: 1;
			u8 hsbist_len: 2;
			u8 stopstate_sel: 1;
			u8 kstep_l: 3;
		} bits;
		u8 val;
	} _0e;

	union __reg_0f__ {
		struct __0f {
			u8 det_delay:2;
		/*12 - 15 -> bit2 - bit5*/
			u8 kdelta: 4;
			u8 ldo0p4:2;
		} bits;
		u8 val;
	} _0f;

};

struct dphy_pll {
	u8 refin; /* Pre-divider control signal */
	u8 cp_s; /* 00: SDM_EN=1, 10: SDM_EN=0 */
	u8 fdk_s; /* PLL mode control: integer or fraction */
	u8 hop_en;
	u8 mod_en; /* ssc enable */
	u8 sdm_en;
	u8 div;
	u8 int_n; /* integer N PLL */
	u32 ref_clk; /* dphy reference clock, unit: MHz */
	u32 freq; /* panel config, unit: KHz */
	int hop_delta;
	u32 hop_period;
	u32 fvco; /* MHz */
	u32 potential_fvco; /* MHz */
	u32 nint; /* sigma delta modulator NINT control */
	u32 kint; /* sigma delta modulator KINT control */
	u32 sign;
	u32 kdelta;
	u32 kstep;
	u8 lpf_sel; /* low pass filter control */
	u8 out_sel; /* post divider control */
	u8 vco_band; /* vco range */
	u8 det_delay;
};

static struct pll_regs regs;
static struct dphy_pll pll;

/* sharkle */
#define VCO_BAND_LOW	750
#define VCO_BAND_MID	1100
#define VCO_BAND_HIGH	1500
#define PHY_REF_CLK	26000

static int dphy_calc_pll_param(struct dphy_pll *pll)
{
	int i;
	const u32 khz = 1000;
	const u32 mhz = 1000000;
	const unsigned long long factor = 100;
	unsigned long long tmp;
	u8 delta;

	if (!pll || !pll->freq)
		goto FAIL;

	pll->potential_fvco = pll->freq / khz; /* MHz */
	pll->ref_clk = PHY_REF_CLK / khz; /* MHz */

	for (i = 0; i < 4; ++i) {
		if (pll->potential_fvco >= VCO_BAND_LOW &&
			pll->potential_fvco <= VCO_BAND_HIGH) {
			pll->fvco = pll->potential_fvco;
			pll->out_sel = BIT(i);
			break;
		}
		pll->potential_fvco <<= 1;
	}
	if (pll->fvco == 0)
		goto FAIL;

	if (pll->fvco >= VCO_BAND_LOW && pll->fvco <= VCO_BAND_MID) {
		/* vco band control */
		pll->vco_band = 0x0;
		/* low pass filter control */
		pll->lpf_sel = 0;
	} else if (pll->fvco > VCO_BAND_MID && pll->fvco <= VCO_BAND_HIGH) {
		pll->vco_band = 0x1;
		pll->lpf_sel = 0;
	} else
		goto FAIL;

	pll->nint = pll->fvco / pll->ref_clk;
	tmp = pll->fvco * factor * mhz;
	do_div(tmp, pll->ref_clk);
	tmp = tmp - pll->nint * factor * mhz;
	tmp *= BIT(20);
	do_div(tmp, 100000000);
	pll->kint = (u32)tmp;
	pll->refin = 3; /* pre-divider bypass */
	pll->sdm_en = true; /* use fraction N PLL */
	pll->fdk_s = 0x1; /* fraction */
	pll->cp_s = 0x0;
	pll->det_delay = 0x1;

	if (pll->hop_en) {
		if ((pll->hop_delta == 0) || (pll->hop_period == 0))
			return 0;

		if (pll->hop_delta < 0) {
			delta = -pll->hop_delta;
			pll->sign = true;
		} else {
			delta = pll->hop_delta;
			pll->sign = false;
		}
		delta = delta * (i + 1);
		pll->kstep = pll->ref_clk * pll->hop_period;
		pll->kdelta = (1 << 20) * delta / pll->ref_clk /
				pll->kstep + pll->sign * (1 << 11);
	}

	if (pll->mod_en) {
		delta = pll->freq / khz * 2 * (i + 1) * 15 / 1000;
		pll->kstep = pll->ref_clk * pll->hop_period;
		pll->kdelta = (((1 << 20) * delta / pll->ref_clk /
				pll->kstep));
	}

	return 0;

FAIL:
	if (pll)
		pll->fvco = 0;

	pr_err("failed to calculate dphy pll parameters\n");

	return -1;
}

static int dphy_set_pll_reg(struct regmap *regmap, struct dphy_pll *pll)
{
	if (!pll || !pll->fvco)
		goto FAIL;

	regs._03.bits.prbs_bist = 1;
	regs._03.bits.en_lp_treot = true;
	regs._03.bits.lpf_sel = pll->lpf_sel;
	regs._03.bits.txfifo_bypass = 0;
	regs._03.bits.freq_hopping = pll->hop_en;
	regs._04.bits.div = pll->div;
	regs._04.bits.masterof8lane = 1;
	regs._04.bits.cp_s = pll->cp_s;
	regs._04.bits.fdk_s = pll->fdk_s;
	regs._06.bits.nint = pll->nint;
	regs._06.bits.mod_en = pll->mod_en;
	regs._07.bits.kdelta_h = pll->kdelta >> 4;
	regs._07.bits.kdelta_h |= pll->sign << 7;
	regs._08.bits.vco_band = pll->vco_band;
	regs._08.bits.sdm_en = pll->sdm_en;
	regs._08.bits.refin = pll->refin;
	regs._08.bits.kdelta_l = pll->kdelta & 0xf;
	regs._09.bits.kint_h = pll->kint >> 12;
	regs._0a.bits.kint_m = (pll->kint >> 4) & 0xff;
	regs._0b.bits.out_sel = pll->out_sel;
	regs._0b.bits.kint_l = pll->kint & 0xf;
	regs._0c.bits.kstep_h = pll->kstep >> 11;
	regs._0d.bits.kstep_m = (pll->kstep >> 3) & 0xff;
	regs._0e.bits.pll_pu_byp = 0;
	regs._0e.bits.pll_pu = 0;
	regs._0e.bits.stopstate_sel = 1;
	regs._0e.bits.kstep_l = pll->kstep & 0x7;
	regs._0f.bits.det_delay = pll->det_delay;
	regs._0f.bits.kdelta =  pll->kdelta >> 12;

	regmap_write(regmap, 0x03, regs._03.val);
	regmap_write(regmap, 0x04, regs._04.val);
	regmap_write(regmap, 0x07, regs._07.val);
	regmap_write(regmap, 0x08, regs._08.val);
	regmap_write(regmap, 0x09, regs._09.val);
	regmap_write(regmap, 0x0a, regs._0a.val);
	regmap_write(regmap, 0x0b, regs._0b.val);
	regmap_write(regmap, 0x0c, regs._0c.val);
	regmap_write(regmap, 0x0d, regs._0d.val);
	regmap_write(regmap, 0x0e, regs._0e.val);
	regmap_write(regmap, 0x0f, regs._0f.val);
	regmap_write(regmap, 0x06, regs._06.val);

	return 0;

FAIL:
	pr_err("failed to set dphy pll registers\n");

	return -1;
}

static int dphy_pll_config(struct dphy_context *ctx)
{
	int ret;
	struct regmap *regmap = ctx->regmap;

	pll.freq = ctx->freq;

	/* FREQ = 26M * (NINT + KINT / 2^20) / out_sel */
	ret = dphy_calc_pll_param(&pll);
	if (ret)
		goto FAIL;
	ret = dphy_set_pll_reg(regmap, &pll);
	if (ret)
		goto FAIL;

	return 0;

FAIL:
	pr_err("failed to config dphy pll\n");

	return -1;
}

static int dphy_set_timing_regs(struct regmap *regmap, int type, u8 val[])
{
	switch (type) {
	case REQUEST_TIME:
		regmap_write(regmap, 0x31, val[CLK]);
		regmap_write(regmap, 0x41, val[DATA]);
		regmap_write(regmap, 0x51, val[DATA]);
		regmap_write(regmap, 0x61, val[DATA]);
		regmap_write(regmap, 0x71, val[DATA]);

		regmap_write(regmap, 0x90, val[CLK]);
		regmap_write(regmap, 0xa0, val[DATA]);
		regmap_write(regmap, 0xb0, val[DATA]);
		regmap_write(regmap, 0xc0, val[DATA]);
		regmap_write(regmap, 0xd0, val[DATA]);
		break;
	case PREPARE_TIME:
		regmap_write(regmap, 0x32, val[CLK]);
		regmap_write(regmap, 0x42, val[DATA]);
		regmap_write(regmap, 0x52, val[DATA]);
		regmap_write(regmap, 0x62, val[DATA]);
		regmap_write(regmap, 0x72, val[DATA]);

		regmap_write(regmap, 0x91, val[CLK]);
		regmap_write(regmap, 0xa1, val[DATA]);
		regmap_write(regmap, 0xb1, val[DATA]);
		regmap_write(regmap, 0xc1, val[DATA]);
		regmap_write(regmap, 0xd1, val[DATA]);
		break;
	case ZERO_TIME:
		regmap_write(regmap, 0x33, val[CLK]);
		regmap_write(regmap, 0x43, val[DATA]);
		regmap_write(regmap, 0x53, val[DATA]);
		regmap_write(regmap, 0x63, val[DATA]);
		regmap_write(regmap, 0x73, val[DATA]);

		regmap_write(regmap, 0x92, val[CLK]);
		regmap_write(regmap, 0xa2, val[DATA]);
		regmap_write(regmap, 0xb2, val[DATA]);
		regmap_write(regmap, 0xc2, val[DATA]);
		regmap_write(regmap, 0xd2, val[DATA]);
		break;
	case TRAIL_TIME:
		regmap_write(regmap, 0x34, val[CLK]);
		regmap_write(regmap, 0x44, val[DATA]);
		regmap_write(regmap, 0x54, val[DATA]);
		regmap_write(regmap, 0x64, val[DATA]);
		regmap_write(regmap, 0x74, val[DATA]);

		regmap_write(regmap, 0x93, val[CLK]);
		regmap_write(regmap, 0xa3, val[DATA]);
		regmap_write(regmap, 0xb3, val[DATA]);
		regmap_write(regmap, 0xc3, val[DATA]);
		regmap_write(regmap, 0xd3, val[DATA]);
		break;
	case EXIT_TIME:
		regmap_write(regmap, 0x36, val[CLK]);
		regmap_write(regmap, 0x46, val[DATA]);
		regmap_write(regmap, 0x56, val[DATA]);
		regmap_write(regmap, 0x66, val[DATA]);
		regmap_write(regmap, 0x76, val[DATA]);

		regmap_write(regmap, 0x95, val[CLK]);
		regmap_write(regmap, 0xA5, val[DATA]);
		regmap_write(regmap, 0xB5, val[DATA]);
		regmap_write(regmap, 0xc5, val[DATA]);
		regmap_write(regmap, 0xd5, val[DATA]);
		break;
	case CLKPOST_TIME:
		regmap_write(regmap, 0x35, val[CLK]);
		regmap_write(regmap, 0x94, val[CLK]);
		break;

	/* the following just use default value */
	case SETTLE_TIME:
	case TA_GET:
	case TA_GO:
	case TA_SURE:
		break;
	default:
		break;
	}
	return 0;
}

static int dphy_timing_config(struct dphy_context *ctx)
{
	u8 val[2];
	u32 tmp = 0;
	u32 range[2], constant;
	u32 t_ui, t_byteck, t_half_byteck;
	const u32 factor = 2;
	const u32 scale = 100;
	struct regmap *regmap = ctx->regmap;

	/* t_ui: 1 ui, byteck: 8 ui, half byteck: 4 ui */
	t_ui = 1000 * scale / (ctx->freq / 1000);
	t_byteck = t_ui << 3;
	t_half_byteck = t_ui << 2;
	constant = t_ui << 1;

	/* REQUEST_TIME: HS T-LPX: LP-01
	 * For T-LPX, mipi spec defined min value is 50ns,
	 * but maybe it shouldn't be too small, because BTA,
	 * LP-10, LP-00, LP-01, all of this is related to T-LPX.
	 */
	range[L] = 50 * scale;
	range[H] = INFINITY;
	val[CLK] = DIV_ROUND_UP(range[L] * (factor << 1), t_byteck) - 2;
	val[DATA] = val[CLK];
	dphy_set_timing_regs(regmap, REQUEST_TIME, val);

	/* PREPARE_TIME: HS sequence: LP-00 */
	range[L] = 38 * scale;
	range[H] = 95 * scale;
	tmp = AVERAGE(range[L], range[H]);
	val[CLK] = DIV_ROUND_UP(AVERAGE(range[L], range[H]),
			t_half_byteck) - 1;
	range[L] = 40 * scale + 4 * t_ui;
	range[H] = 85 * scale + 6 * t_ui;
	tmp |= AVERAGE(range[L], range[H]) << 16;
	val[DATA] = DIV_ROUND_UP(AVERAGE(range[L], range[H]),
			t_half_byteck) - 1;
	dphy_set_timing_regs(regmap, PREPARE_TIME, val);

	/* ZERO_TIME: HS-ZERO */
	range[L] = 300 * scale;
	range[H] = INFINITY;
	val[CLK] = DIV_ROUND_UP(range[L] * factor + (tmp & 0xffff)
			- 525 * t_byteck / 100, t_byteck) - 2;
	range[L] = 145 * scale + 10 * t_ui;
	val[DATA] = DIV_ROUND_UP(range[L] * factor
			+ ((tmp >> 16) & 0xffff) - 525 * t_byteck / 100,
			t_byteck) - 2;
	dphy_set_timing_regs(regmap, ZERO_TIME, val);

	/* TRAIL_TIME: HS-TRAIL */
	range[L] = 60 * scale;
	range[H] = INFINITY;
	val[CLK] = DIV_ROUND_UP(range[L] * factor - constant, t_half_byteck);
	range[L] = max(8 * t_ui, 60 * scale + 4 * t_ui);
	val[DATA] = DIV_ROUND_UP(range[L] * 3 / 2 - constant, t_half_byteck) - 2;
	dphy_set_timing_regs(regmap, TRAIL_TIME, val);

	/* EXIT_TIME: */
	range[L] = 100 * scale;
	range[H] = INFINITY;
	val[CLK] = DIV_ROUND_UP(range[L] * factor, t_byteck) - 2;
	val[DATA] = val[CLK];
	dphy_set_timing_regs(regmap, EXIT_TIME, val);

	/* CLKPOST_TIME: */
	range[L] = 60 * scale + 52 * t_ui;
	range[H] = INFINITY;
	val[CLK] = DIV_ROUND_UP(range[L] * factor, t_byteck) - 2;
	val[DATA] = val[CLK];
	dphy_set_timing_regs(regmap, CLKPOST_TIME, val);

	/* SETTLE_TIME:
	 * This time is used for receiver. So for transmitter,
	 * it can be ignored.
	 */

	/* TA_GO:
	 * transmitter drives bridge state(LP-00) before releasing control,
	 * reg 0x1f default value: 0x04, which is good.
	 */

	/* TA_SURE:
	 * After LP-10 state and before bridge state(LP-00),
	 * reg 0x20 default value: 0x01, which is good.
	 */

	/* TA_GET:
	 * receiver drives Bridge state(LP-00) before releasing control
	 * reg 0x21 default value: 0x03, which is good.
	 */

	return 0;
}

static int dphy_hop_start(struct dphy_context *ctx)
{
	struct regmap *regmap = ctx->regmap;

	/* start hopping */
	regs._04.bits.hop_trig = !regs._04.bits.hop_trig;
	regmap_write(regmap, 0x04, regs._04.val);

	mdelay(1);

	pr_info("start hopping\n");
	return 0;
}

static int dphy_hop_stop(struct dphy_context *ctx)
{
	struct regmap *regmap = ctx->regmap;

	regs._03.bits.freq_hopping = 0;
	regmap_write(regmap, 0x03, regs._03.val);

	pr_info("stop hopping\n");
	return 0;
}

static int dphy_hop_config(struct dphy_context *ctx, int delta, int period)
{
	dphy_hop_stop(ctx);

	if (delta == 0) {
		pll.hop_en = false;
		return 0;
	}

	pll.hop_en = true;
	pll.mod_en = false;
	pll.hop_delta = delta / 1000;	/* Mhz */
	pll.hop_period = period;	/* us */

	dphy_pll_config(ctx);
	dphy_hop_start(ctx);

	return 0;
}

static int dphy_ssc_start(struct dphy_context *ctx)
{
	pll.mod_en = true;
	pll.hop_en = false;
	pll.hop_period = 15;/* us */

	dphy_pll_config(ctx);

	mdelay(1);

	return 0;
}

static int dphy_ssc_stop(struct dphy_context *ctx)
{
	struct regmap *regmap = ctx->regmap;

	pll.mod_en = false;
	regs._06.bits.mod_en = false;
	regmap_write(regmap, 0x06, regs._06.val);

	return 0;
}

static int dphy_ssc_en(struct dphy_context *ctx, bool en)
{
	if (en) {
		pr_info("ssc enable\n");
		dphy_ssc_start(ctx);
	} else {
		dphy_ssc_stop(ctx);
		pr_info("ssc disable\n");
	}

	return 0;
}

/**
 * Force D-PHY PLL to stay on while in ULPS
 * @param phy: pointer to structure
 *  which holds information about the d-phy module
 * @param force (1) disable (0)
 * @note To follow the programming model, use wakeup_pll function
 */
static void dphy_force_pll(struct dphy_context *ctx, int force)
{
	u8 data;
	struct regmap *regmap = ctx->regmap;

	if (force)
		data = 0x03;
	else
		data = 0x0;

	/* for megocores, to force pll, dphy register should be set */
	regmap_write(regmap, 0x0e, data);
}

const struct dphy_pll_ops sharkle_dphy_pll_ops = {
	.pll_config = dphy_pll_config,
	.timing_config = dphy_timing_config,
	.hop_config = dphy_hop_config,
	.ssc_en = dphy_ssc_en,
	.force_pll = dphy_force_pll,
};
