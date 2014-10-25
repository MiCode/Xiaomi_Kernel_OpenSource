/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <soc/qcom/clock-alpha-pll.h>
#include <soc/qcom/msm-clock-controller.h>

#include "clock.h"

#define WAIT_MAX_LOOPS 100

#define MODE_REG(pll) (*pll->base + pll->offset + 0x0)
#define LOCK_REG(pll) (*pll->base + pll->offset + 0x0)
#define ACTIVE_REG(pll) (*pll->base + pll->offset + 0x0)
#define UPDATE_REG(pll) (*pll->base + pll->offset + 0x0)
#define L_REG(pll) (*pll->base + pll->offset + 0x4)
#define A_REG(pll) (*pll->base + pll->offset + 0x8)
#define VCO_REG(pll) (*pll->base + pll->offset + 0x10)
#define ALPHA_EN_REG(pll) (*pll->base + pll->offset + 0x10)
#define OUTPUT_REG(pll) (*pll->base + pll->offset + 0x10)
#define VOTE_REG(pll) (*pll->base + pll->fsm_reg_offset)
#define USER_CTL_LO_REG(pll) (*pll->base + pll->offset + 0x10)

#define PLL_BYPASSNL 0x2
#define PLL_RESET_N  0x4
#define PLL_OUTCTRL  0x1

/*
 * Even though 40 bits are present, use only 32 for ease of calculation.
 */
#define ALPHA_REG_BITWIDTH 40
#define ALPHA_BITWIDTH 32

/*
 * Enable/disable registers could be shared among PLLs when FSM voting
 * is used. This lock protects against potential race when multiple
 * PLLs are being enabled/disabled together.
 */
static DEFINE_SPINLOCK(alpha_pll_reg_lock);

static unsigned long compute_rate(struct alpha_pll_clk *pll,
				u32 l_val, u32 a_val)
{
	u64 rate, parent_rate;

	parent_rate = clk_get_rate(pll->c.parent);
	rate = parent_rate * l_val;
	rate += (parent_rate * a_val) >> ALPHA_BITWIDTH;
	return rate;
}

static bool is_locked(struct alpha_pll_clk *pll)
{
	u32 reg = readl_relaxed(LOCK_REG(pll));
	u32 mask = pll->masks->lock_mask;
	return (reg & mask) == mask;
}

static bool is_active(struct alpha_pll_clk *pll)
{
	u32 reg = readl_relaxed(ACTIVE_REG(pll));
	u32 mask = pll->masks->active_mask;
	return (reg & mask) == mask;
}

/*
 * Check active_flag if PLL is in FSM mode, otherwise check lock_det
 * bit. This function assumes PLLs are already configured to the
 * right mode.
 */
static bool update_finish(struct alpha_pll_clk *pll)
{
	if (pll->fsm_en_mask)
		return is_active(pll);
	else
		return is_locked(pll);
}

static int wait_for_update(struct alpha_pll_clk *pll)
{
	int count;

	for (count = WAIT_MAX_LOOPS; count > 0; count--) {
		if (update_finish(pll))
			break;
		udelay(1);
	}

	if (!count) {
		pr_err("%s didn't lock after enabling it!\n", pll->c.dbg_name);
		return -EINVAL;
	}

	return 0;
}

static int __alpha_pll_vote_enable(struct alpha_pll_clk *pll)
{
	u32 ena;

	ena = readl_relaxed(VOTE_REG(pll));
	ena |= pll->fsm_en_mask;
	writel_relaxed(ena, VOTE_REG(pll));
	mb();

	return wait_for_update(pll);
}

static int __alpha_pll_enable(struct alpha_pll_clk *pll)
{
	int rc;
	u32 mode;

	mode  = readl_relaxed(MODE_REG(pll));
	mode |= PLL_BYPASSNL;
	writel_relaxed(mode, MODE_REG(pll));

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset.
	 */
	mb();
	udelay(5);

	mode |= PLL_RESET_N;
	writel_relaxed(mode, MODE_REG(pll));

	rc = wait_for_update(pll);
	if (rc < 0)
		return rc;

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, MODE_REG(pll));

	/* Ensure that the write above goes through before returning. */
	mb();
	return 0;
}

static void __init_alpha_pll(struct clk *c);

static int alpha_pll_enable(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	unsigned long flags;
	int rc;

	if (unlikely(!pll->inited))
		__init_alpha_pll(c);

	spin_lock_irqsave(&alpha_pll_reg_lock, flags);
	if (pll->fsm_en_mask)
		rc = __alpha_pll_vote_enable(pll);
	else
		rc = __alpha_pll_enable(pll);
	spin_unlock_irqrestore(&alpha_pll_reg_lock, flags);

	return rc;
}

static void __alpha_pll_vote_disable(struct alpha_pll_clk *pll)
{
	u32 ena;

	ena = readl_relaxed(VOTE_REG(pll));
	ena &= ~pll->fsm_en_mask;
	writel_relaxed(ena, VOTE_REG(pll));
}

static void __alpha_pll_disable(struct alpha_pll_clk *pll)
{
	u32 mode;

	mode = readl_relaxed(MODE_REG(pll));
	mode &= ~PLL_OUTCTRL;
	writel_relaxed(mode, MODE_REG(pll));

	/* Delay of 2 output clock ticks required until output is disabled */
	mb();
	udelay(1);

	mode &= ~(PLL_BYPASSNL | PLL_RESET_N);
	writel_relaxed(mode, MODE_REG(pll));
}

static void alpha_pll_disable(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	unsigned long flags;

	spin_lock_irqsave(&alpha_pll_reg_lock, flags);
	if (pll->fsm_en_mask)
		__alpha_pll_vote_disable(pll);
	else
		__alpha_pll_disable(pll);
	spin_unlock_irqrestore(&alpha_pll_reg_lock, flags);
}

static u32 find_vco(struct alpha_pll_clk *pll, unsigned long rate)
{
	unsigned long i;
	struct alpha_pll_vco_tbl *v = pll->vco_tbl;

	for (i = 0; i < pll->num_vco; i++) {
		if (rate >= v[i].min_freq && rate <= v[i].max_freq)
			return v[i].vco_val;
	}

	return -EINVAL;
}

static unsigned long __calc_values(struct alpha_pll_clk *pll,
		unsigned long rate, int *l_val, u64 *a_val, bool round_up)
{
	u32 parent_rate;
	u64 remainder;
	u64 quotient;
	unsigned long freq_hz;

	parent_rate = clk_get_rate(pll->c.parent);
	quotient = rate;
	remainder = do_div(quotient, parent_rate);
	*l_val = quotient;

	if (!remainder) {
		*a_val = 0;
		return rate;
	}

	/* Upper ALPHA_BITWIDTH bits of Alpha */
	quotient = remainder << ALPHA_BITWIDTH;
	remainder = do_div(quotient, parent_rate);

	if (remainder && round_up)
		quotient++;

	*a_val = quotient;
	freq_hz = compute_rate(pll, *l_val, *a_val);
	return freq_hz;
}

static unsigned long round_rate_down(struct alpha_pll_clk *pll,
		unsigned long rate, int *l_val, u64 *a_val)
{
	return __calc_values(pll, rate, l_val, a_val, false);
}

static unsigned long round_rate_up(struct alpha_pll_clk *pll,
		unsigned long rate, int *l_val, u64 *a_val)
{
	return __calc_values(pll, rate, l_val, a_val, true);
}

static int alpha_pll_set_rate(struct clk *c, unsigned long rate)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	struct alpha_pll_masks *masks = pll->masks;
	unsigned long flags, freq_hz;
	u32 regval, l_val;
	int vco_val;
	u64 a_val;

	freq_hz = round_rate_up(pll, rate, &l_val, &a_val);
	if (freq_hz != rate) {
		pr_err("alpha_pll: Call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	vco_val = find_vco(pll, freq_hz);
	if (IS_ERR_VALUE(vco_val)) {
		pr_err("alpha pll: not in a valid vco range\n");
		return -EINVAL;
	}

	/*
	 * Ensure PLL is off before changing rate. For optimization reasons,
	 * assume no downstream clock is actively using it. No support
	 * for dynamic update at the moment.
	 */
	spin_lock_irqsave(&c->lock, flags);
	if (c->count)
		alpha_pll_disable(c);

	a_val = a_val << (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	writel_relaxed(l_val, L_REG(pll));
	__iowrite32_copy(A_REG(pll), &a_val, 2);

	if (masks->vco_mask) {
		regval = readl_relaxed(VCO_REG(pll));
		regval &= ~(masks->vco_mask << masks->vco_shift);
		regval |= vco_val << masks->vco_shift;
		writel_relaxed(regval, VCO_REG(pll));
	}

	regval = readl_relaxed(ALPHA_EN_REG(pll));
	regval |= masks->alpha_en_mask;
	writel_relaxed(regval, ALPHA_EN_REG(pll));

	if (c->count)
		alpha_pll_enable(c);

	spin_unlock_irqrestore(&c->lock, flags);
	return 0;
}

static long alpha_pll_round_rate(struct clk *c, unsigned long rate)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	struct alpha_pll_vco_tbl *v = pll->vco_tbl;
	int ret;
	u32 l_val;
	unsigned long freq_hz;
	u64 a_val;
	int i;

	freq_hz = round_rate_up(pll, rate, &l_val, &a_val);
	ret = find_vco(pll, freq_hz);
	if (!IS_ERR_VALUE(ret))
		return freq_hz;

	freq_hz = 0;
	for (i = 0; i < pll->num_vco; i++) {
		if (is_better_rate(rate, freq_hz, v[i].min_freq))
			freq_hz = v[i].min_freq;
		if (is_better_rate(rate, freq_hz, v[i].max_freq))
			freq_hz = v[i].max_freq;
	}
	if (!freq_hz)
		return -EINVAL;
	return freq_hz;
}

static void update_vco_tbl(struct alpha_pll_clk *pll)
{
	int i, l_val;
	u64 a_val;
	unsigned long hz;

	/* Round vco limits to valid rates */
	for (i = 0; i < pll->num_vco; i++) {
		hz = round_rate_up(pll, pll->vco_tbl[i].min_freq, &l_val,
					&a_val);
		pll->vco_tbl[i].min_freq = hz;

		hz = round_rate_down(pll, pll->vco_tbl[i].max_freq, &l_val,
					&a_val);
		pll->vco_tbl[i].max_freq = hz;
	}
}

/*
 * Program bias count to be 0x6 (corresponds to 5us), and lock count
 * bits to 0 (check lock_det for locking).
 */
static void __set_fsm_mode(void __iomem *mode_reg)
{
	u32 regval = readl_relaxed(mode_reg);

	/* De-assert reset to FSM */
	regval &= ~BIT(21);
	writel_relaxed(regval, mode_reg);

	/* Program bias count */
	regval &= ~BM(19, 14);
	regval |= BVAL(19, 14, 0x6);
	writel_relaxed(regval, mode_reg);

	/* Program lock count */
	regval &= ~BM(13, 8);
	regval |= BVAL(13, 8, 0x0);
	writel_relaxed(regval, mode_reg);

	/* Enable PLL FSM voting */
	regval |= BIT(20);
	writel_relaxed(regval, mode_reg);
}

static bool is_fsm_mode(void __iomem *mode_reg)
{
	return !!(readl_relaxed(mode_reg) & BIT(20));
}

static void __init_alpha_pll(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	struct alpha_pll_masks *masks = pll->masks;
	u32 output_en, userval;

	if (masks->output_mask && pll->enable_config) {
		output_en = readl_relaxed(OUTPUT_REG(pll));
		output_en &= ~masks->output_mask;
		output_en |= pll->enable_config;
		writel_relaxed(output_en, OUTPUT_REG(pll));
	}

	if (masks->post_div_mask) {
		userval = readl_relaxed(USER_CTL_LO_REG(pll));
		userval &= ~masks->post_div_mask;
		userval |= pll->post_div_config;
		writel_relaxed(userval, USER_CTL_LO_REG(pll));
	}

	if (pll->fsm_en_mask)
		__set_fsm_mode(MODE_REG(pll));

	pll->inited = true;
}

static enum handoff alpha_pll_handoff(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	struct alpha_pll_masks *masks = pll->masks;
	u64 a_val;
	u32 alpha_en, l_val;

	update_vco_tbl(pll);

	if (!is_locked(pll)) {
		if (c->rate && alpha_pll_set_rate(c, c->rate))
			WARN(1, "%s: Failed to configure rate\n", c->dbg_name);
		__init_alpha_pll(c);
		return HANDOFF_DISABLED_CLK;
	} else if (pll->fsm_en_mask && !is_fsm_mode(MODE_REG(pll))) {
		WARN(1, "%s should be in FSM mode but is not\n", c->dbg_name);
	}

	l_val = readl_relaxed(L_REG(pll));
	/* read u64 in two steps to satisfy alignment constraint */
	a_val = readl_relaxed(A_REG(pll) + 0x4);
	a_val = a_val << 32 | readl_relaxed(A_REG(pll));
	/* get upper 32 bits */
	a_val = a_val >> (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	alpha_en = readl_relaxed(ALPHA_EN_REG(pll));
	alpha_en &= masks->alpha_en_mask;
	if (!alpha_en)
		a_val = 0;

	c->rate = compute_rate(pll, l_val, a_val);

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_alpha_pll = {
	.enable = alpha_pll_enable,
	.disable = alpha_pll_disable,
	.round_rate = alpha_pll_round_rate,
	.set_rate = alpha_pll_set_rate,
	.handoff = alpha_pll_handoff,
};

struct clk_ops clk_ops_fixed_alpha_pll = {
	.enable = alpha_pll_enable,
	.disable = alpha_pll_disable,
	.handoff = alpha_pll_handoff,
};

static struct alpha_pll_masks masks_20nm_p = {
	.lock_mask = BIT(31),
	.active_mask = BIT(30),
	.vco_mask = BM(21, 20) >> 20,
	.vco_shift = 20,
	.alpha_en_mask = BIT(24),
	.output_mask = 0xF,
	.post_div_mask = 0xF00,
};

static struct alpha_pll_vco_tbl vco_20nm_p[] = {
	VCO(3,  250000000,  500000000),
	VCO(2,  500000000, 1000000000),
	VCO(1, 1000000000, 1500000000),
	VCO(0, 1500000000, 2000000000),
};

static struct alpha_pll_masks masks_20nm_t = {
	.lock_mask = BIT(31),
	.alpha_en_mask = BIT(24),
	.output_mask = 0xf,
};

static struct alpha_pll_vco_tbl vco_20nm_t[] = {
	VCO(0, 500000000, 1250000000),
};

static struct alpha_pll_clk *alpha_pll_dt_parser(struct device *dev,
						struct device_node *np)
{
	struct alpha_pll_clk *pll;
	struct msmclk_data *drv;

	pll = devm_kzalloc(dev, sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	if (of_property_read_u32(np, "qcom,base-offset", &pll->offset)) {
		dt_err(np, "missing qcom,base-offset\n");
		return ERR_PTR(-EINVAL);
	}

	/* Optional property */
	of_property_read_u32(np, "qcom,post-div-config",
					&pll->post_div_config);

	pll->masks = devm_kzalloc(dev, sizeof(*pll->masks), GFP_KERNEL);
	if (!pll->masks) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	if (of_device_is_compatible(np, "qcom,fixed-alpha-pll-20p") ||
		of_device_is_compatible(np, "qcom,alpha-pll-20p")) {
		*pll->masks = masks_20nm_p;
		pll->vco_tbl = vco_20nm_p;
		pll->num_vco = ARRAY_SIZE(vco_20nm_p);
	} else if (of_device_is_compatible(np, "qcom,fixed-alpha-pll-20t") ||
		of_device_is_compatible(np, "qcom,alpha-pll-20t")) {
		*pll->masks = masks_20nm_t;
		pll->vco_tbl = vco_20nm_t;
		pll->num_vco = ARRAY_SIZE(vco_20nm_t);
	} else {
		dt_err(np, "unexpected compatible string\n");
		return ERR_PTR(-EINVAL);
	}

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return ERR_CAST(drv);
	pll->base = &drv->base;
	return pll;
}

static void *variable_rate_alpha_pll_dt_parser(struct device *dev,
					struct device_node *np)
{
	struct alpha_pll_clk *pll;

	pll = alpha_pll_dt_parser(dev, np);
	if (IS_ERR(pll))
		return pll;

	/* Optional Property */
	of_property_read_u32(np, "qcom,output-enable", &pll->enable_config);

	pll->c.ops = &clk_ops_alpha_pll;
	return msmclk_generic_clk_init(dev, np, &pll->c);
}

static void *fixed_rate_alpha_pll_dt_parser(struct device *dev,
						struct device_node *np)
{
	struct alpha_pll_clk *pll;
	int rc;
	u32 val;

	pll = alpha_pll_dt_parser(dev, np);
	if (IS_ERR(pll))
		return pll;

	rc = of_property_read_u32(np, "qcom,pll-config-rate", &val);
	if (rc) {
		dt_err(np, "missing qcom,pll-config-rate\n");
		return ERR_PTR(-EINVAL);
	}
	pll->c.rate = val;

	rc = of_property_read_u32(np, "qcom,output-enable",
						&pll->enable_config);
	if (rc) {
		dt_err(np, "missing qcom,output-enable\n");
		return ERR_PTR(-EINVAL);
	}

	/* Optional Property */
	rc = of_property_read_u32(np, "qcom,fsm-en-bit", &val);
	if (!rc) {
		rc = of_property_read_u32(np, "qcom,fsm-en-offset",
						&pll->fsm_reg_offset);
		if (rc) {
			dt_err(np, "missing qcom,fsm-en-offset\n");
			return ERR_PTR(-EINVAL);
		}
		pll->fsm_en_mask = BIT(val);
	}

	pll->c.ops = &clk_ops_fixed_alpha_pll;
	return msmclk_generic_clk_init(dev, np, &pll->c);
}

MSMCLK_PARSER(fixed_rate_alpha_pll_dt_parser, "qcom,fixed-alpha-pll-20p", 0);
MSMCLK_PARSER(fixed_rate_alpha_pll_dt_parser, "qcom,fixed-alpha-pll-20t", 1);
MSMCLK_PARSER(variable_rate_alpha_pll_dt_parser, "qcom,alpha-pll-20p", 0);
MSMCLK_PARSER(variable_rate_alpha_pll_dt_parser, "qcom,alpha-pll-20t", 1);
