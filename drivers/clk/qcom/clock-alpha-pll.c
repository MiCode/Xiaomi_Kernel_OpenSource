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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <soc/qcom/clock-alpha-pll.h>

#include "clock.h"

#define WAIT_MAX_LOOPS 100

#define MODE_REG(pll) (*pll->base + pll->offset + 0x0)
#define LOCK_REG(pll) (*pll->base + pll->offset + 0x0)
#define UPDATE_REG(pll) (*pll->base + pll->offset + 0x0)
#define L_REG(pll) (*pll->base + pll->offset + 0x4)
#define A_REG(pll) (*pll->base + pll->offset + 0x8)
#define VCO_REG(pll) (*pll->base + pll->offset + 0x10)
#define ALPHA_EN_REG(pll) (*pll->base + pll->offset + 0x10)

#define PLL_BYPASSNL 0x2
#define PLL_RESET_N  0x4
#define PLL_OUTCTRL  0x1

/*
 * Even though 40 bits are present, only the upper 16 bits are
 * signfigant due to the natural randomness in the XO clock
 */
#define ALPHA_REG_BITWIDTH 40
#define ALPHA_BITWIDTH 16

static unsigned long compute_rate(u64 parent_rate,
				u32 l_val, u64 a_val)
{
	unsigned long rate;

	/*
	 * assuming parent_rate < 2^25, we need a_val < 2^39 to avoid
	 * overflow when multipling below.
	 */
	a_val = a_val >> 1;
	rate = parent_rate * l_val;
	rate += (unsigned long)((parent_rate * a_val) >>
				(ALPHA_REG_BITWIDTH - 1));
	return rate;
}

static bool is_locked(struct alpha_pll_clk *pll)
{
	u32 reg = readl_relaxed(LOCK_REG(pll));
	u32 mask = pll->masks->lock_mask;
	return (reg & mask) == mask;
}

static int alpha_pll_enable(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	int count;
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

	/* Wait for pll to lock. */
	for (count = WAIT_MAX_LOOPS; count > 0; count--) {
		if (is_locked(pll))
			break;
		udelay(1);
	}

	if (!count) {
		pr_err("%s didn't lock after enabling it!\n", c->dbg_name);
		return -EINVAL;
	}

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, MODE_REG(pll));

	/* Ensure that the write above goes through before returning. */
	mb();
	return 0;
}

static void alpha_pll_disable(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
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
	u64 parent_rate;
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

	/* Upper 16 bits of Alpha */
	quotient = remainder << ALPHA_BITWIDTH;
	remainder = do_div(quotient, parent_rate);

	if (remainder && round_up)
		quotient++;

	/* Convert to 40 bit format */
	*a_val = quotient << (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	freq_hz = compute_rate(parent_rate, *l_val, *a_val);
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
	u32 a_upper, a_lower, regval, l_val, vco_val;
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

	a_upper = (a_val >> 32) & 0xFF;
	a_lower = (a_val & 0xFFFFFFFF);

	writel_relaxed(l_val, L_REG(pll));
	writel_relaxed(a_lower, A_REG(pll));
	writel_relaxed(a_upper, A_REG(pll) + 0x4);

	regval = readl_relaxed(VCO_REG(pll));
	regval &= ~(masks->vco_mask << masks->vco_shift);
	regval |= vco_val << masks->vco_shift;
	writel_relaxed(regval, VCO_REG(pll));

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
	u32 ret, l_val;
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

static enum handoff alpha_pll_handoff(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	struct alpha_pll_masks *masks = pll->masks;
	u64 parent_rate, a_val;
	u32 alpha_en, l_val;

	update_vco_tbl(pll);

	if (!is_locked(pll))
		return HANDOFF_DISABLED_CLK;

	alpha_en = readl_relaxed(ALPHA_EN_REG(pll));
	alpha_en &= masks->alpha_en_mask;

	l_val = readl_relaxed(L_REG(pll));
	a_val = readl_relaxed(A_REG(pll));
	a_val |= ((u64)readl_relaxed(A_REG(pll) + 0x4)) << 32;

	if (!alpha_en)
		a_val = 0;

	parent_rate = clk_get_rate(c->parent);
	c->rate = compute_rate(parent_rate, l_val, a_val);

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_alpha_pll = {
	.enable = alpha_pll_enable,
	.disable = alpha_pll_disable,
	.round_rate = alpha_pll_round_rate,
	.set_rate = alpha_pll_set_rate,
	.handoff = alpha_pll_handoff,
};


