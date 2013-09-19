/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>

#include <linux/clk.h>
#include <mach/clk-provider.h>
#include <mach/clk.h>
#include <mach/clock-generic.h>
#include <mach/msm-krait-l2-accessors.h>
#include "clock-krait.h"
#include "avs.h"

static DEFINE_SPINLOCK(kpss_clock_reg_lock);

#define LPL_SHIFT	8
static void __kpss_mux_set_sel(struct mux_clk *mux, int sel)
{
	unsigned long flags;
	u32 regval;

	spin_lock_irqsave(&kpss_clock_reg_lock, flags);
	regval = get_l2_indirect_reg(mux->offset);
	regval &= ~(mux->mask << mux->shift);
	regval |= (sel & mux->mask) << mux->shift;
	if (mux->priv) {
		regval &= ~(mux->mask << (mux->shift + LPL_SHIFT));
		regval |= (sel & mux->mask) << (mux->shift + LPL_SHIFT);
	}
	set_l2_indirect_reg(mux->offset, regval);
	spin_unlock_irqrestore(&kpss_clock_reg_lock, flags);

	/* Wait for switch to complete. */
	mb();
	udelay(1);
}
static int kpss_mux_set_sel(struct mux_clk *mux, int sel)
{
	mux->en_mask = sel;
	if (mux->c.count)
		__kpss_mux_set_sel(mux, sel);
	return 0;
}

static int kpss_mux_get_sel(struct mux_clk *mux)
{
	u32 sel;

	sel = get_l2_indirect_reg(mux->offset);
	sel >>= mux->shift;
	sel &= mux->mask;
	mux->en_mask = sel;

	return sel;
}

static int kpss_mux_enable(struct mux_clk *mux)
{
	__kpss_mux_set_sel(mux, mux->en_mask);
	return 0;
}

static void kpss_mux_disable(struct mux_clk *mux)
{
	__kpss_mux_set_sel(mux, mux->safe_sel);
}

struct clk_mux_ops clk_mux_ops_kpss = {
	.enable = kpss_mux_enable,
	.disable = kpss_mux_disable,
	.set_mux_sel = kpss_mux_set_sel,
	.get_mux_sel = kpss_mux_get_sel,
};

/*
 * The divider can divide by 2, 4, 6 and 8. But we only really need div-2. So
 * force it to div-2 during handoff and treat it like a fixed div-2 clock.
 */
static int kpss_div2_get_div(struct div_clk *div)
{
	unsigned long flags;
	u32 regval;
	int val;

	spin_lock_irqsave(&kpss_clock_reg_lock, flags);
	regval = get_l2_indirect_reg(div->offset);
	val = (regval >> div->shift) & div->mask;
	regval &= ~(div->mask << div->shift);
	if (div->priv)
		regval &= ~(div->mask << (div->shift + LPL_SHIFT));
	set_l2_indirect_reg(div->offset, regval);
	spin_unlock_irqrestore(&kpss_clock_reg_lock, flags);

	val = (val + 1) * 2;
	WARN(val != 2, "Divider %s was configured to div-%d instead of 2!\n",
		div->c.dbg_name, val);

	return 2;
}

struct clk_div_ops clk_div_ops_kpss_div2 = {
	.get_div = kpss_div2_get_div,
};

#define LOCK_BIT	BIT(16)

/* Initialize a HFPLL at a given rate and enable it. */
static void __hfpll_clk_init_once(struct clk *c)
{
	struct hfpll_clk *h = to_hfpll_clk(c);
	struct hfpll_data const *hd = h->d;

	if (likely(h->init_done))
		return;

	/* Configure PLL parameters for integer mode. */
	writel_relaxed(hd->config_val, h->base + hd->config_offset);
	writel_relaxed(0, h->base + hd->m_offset);
	writel_relaxed(1, h->base + hd->n_offset);

	if (hd->user_offset) {
		u32 regval = hd->user_val;
		unsigned long rate;

		rate = readl_relaxed(h->base + hd->l_offset) * h->src_rate;

		/* Pick the right VCO. */
		if (rate > hd->low_vco_max_rate)
			regval |= hd->user_vco_mask;
		writel_relaxed(regval, h->base + hd->user_offset);
	}

	if (hd->droop_offset)
		writel_relaxed(hd->droop_val, h->base + hd->droop_offset);

	h->init_done = true;
}

/* Enable an already-configured HFPLL. */
static int hfpll_clk_enable(struct clk *c)
{
	struct hfpll_clk *h = to_hfpll_clk(c);
	struct hfpll_data const *hd = h->d;

	if (!h->base)
		return -ENODEV;

	__hfpll_clk_init_once(c);

	/* Disable PLL bypass mode. */
	writel_relaxed(0x2, h->base + hd->mode_offset);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	mb();
	udelay(10);

	/* De-assert active-low PLL reset. */
	writel_relaxed(0x6, h->base + hd->mode_offset);

	/* Wait for PLL to lock. */
	if (hd->status_offset) {
		while (!(readl_relaxed(h->base + hd->status_offset) & LOCK_BIT))
			;
	} else {
		mb();
		udelay(60);
	}

	/* Enable PLL output. */
	writel_relaxed(0x7, h->base + hd->mode_offset);

	/* Make sure the enable is done before returning. */
	mb();

	return 0;
}

static void hfpll_clk_disable(struct clk *c)
{
	struct hfpll_clk *h = to_hfpll_clk(c);
	struct hfpll_data const *hd = h->d;

	/*
	 * Disable the PLL output, disable test mode, enable the bypass mode,
	 * and assert the reset.
	 */
	writel_relaxed(0, h->base + hd->mode_offset);
}

static long hfpll_clk_round_rate(struct clk *c, unsigned long rate)
{
	struct hfpll_clk *h = to_hfpll_clk(c);
	struct hfpll_data const *hd = h->d;
	unsigned long rrate;

	if (!h->src_rate)
		return 0;

	rate = max(rate, hd->min_rate);
	rate = min(rate, hd->max_rate);

	rrate = DIV_ROUND_UP(rate, h->src_rate) * h->src_rate;
	if (rrate > hd->max_rate)
		rrate -= h->src_rate;

	return rrate;
}

/*
 * For optimization reasons, assumes no downstream clocks are actively using
 * it.
 */
static int hfpll_clk_set_rate(struct clk *c, unsigned long rate)
{
	struct hfpll_clk *h = to_hfpll_clk(c);
	struct hfpll_data const *hd = h->d;
	unsigned long flags;
	u32 l_val;

	if (!h->base)
		return -ENODEV;

	if (rate != hfpll_clk_round_rate(c, rate))
		return -EINVAL;

	l_val = rate / h->src_rate;

	spin_lock_irqsave(&c->lock, flags);

	if (c->count)
		hfpll_clk_disable(c);

	/* Pick the right VCO. */
	if (hd->user_offset) {
		u32 regval;
		regval = readl_relaxed(h->base + hd->user_offset);
		if (rate <= hd->low_vco_max_rate)
			regval &= ~hd->user_vco_mask;
		else
			regval |= hd->user_vco_mask;
		writel_relaxed(regval, h->base  + hd->user_offset);
	}

	writel_relaxed(l_val, h->base + hd->l_offset);

	if (c->count)
		hfpll_clk_enable(c);

	spin_unlock_irqrestore(&c->lock, flags);

	return 0;
}

static enum handoff hfpll_clk_handoff(struct clk *c)
{
	struct hfpll_clk *h = to_hfpll_clk(c);
	struct hfpll_data const *hd = h->d;
	u32 l_val, mode;

	if (!hd)
		return HANDOFF_DISABLED_CLK;

	if (!h->base)
		return HANDOFF_DISABLED_CLK;

	/* Assume parent rate doesn't change and cache it. */
	h->src_rate = clk_get_rate(c->parent);
	l_val = readl_relaxed(h->base + hd->l_offset);
	c->rate = l_val * h->src_rate;

	mode = readl_relaxed(h->base + hd->mode_offset) & 0x7;
	if (mode != 0x7) {
		__hfpll_clk_init_once(c);
		return HANDOFF_DISABLED_CLK;
	}

	if (hd->status_offset &&
		!(readl_relaxed(h->base + hd->status_offset) & LOCK_BIT)) {
		WARN(1, "HFPLL %s is ON, but not locked!\n", c->dbg_name);
		hfpll_clk_disable(c);
		__hfpll_clk_init_once(c);
		return HANDOFF_DISABLED_CLK;
	}

	WARN(c->rate < hd->min_rate || c->rate > hd->max_rate,
		"HFPLL %s rate %lu outside spec!\n", c->dbg_name, c->rate);

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_hfpll = {
	.enable = hfpll_clk_enable,
	.disable = hfpll_clk_disable,
	.round_rate = hfpll_clk_round_rate,
	.set_rate = hfpll_clk_set_rate,
	.handoff = hfpll_clk_handoff,
};

struct cpu_hwcg_action {
	bool read;
	bool enable;
};

static void cpu_hwcg_rw(void *info)
{
	struct cpu_hwcg_action *action = info;

	u32 val;
	asm volatile ("mrc p15, 7, %[cpmr0], c15, c0, 5\n\t"
			: [cpmr0]"=r" (val));

	if (action->read) {
		action->enable = !(val & BIT(0));
		return;
	}

	if (action->enable)
		val &= ~BIT(0);
	else
		val |= BIT(0);

	asm volatile ("mcr p15, 7, %[cpmr0], c15, c0, 5\n\t"
			: : [cpmr0]"r" (val));
}

static void kpss_cpu_enable_hwcg(struct clk *c)
{
	struct kpss_core_clk *cpu = to_kpss_core_clk(c);
	struct cpu_hwcg_action action = { .enable = true };

	smp_call_function_single(cpu->id, cpu_hwcg_rw, &action, 1);
}

static void kpss_cpu_disable_hwcg(struct clk *c)
{
	struct kpss_core_clk *cpu = to_kpss_core_clk(c);
	struct cpu_hwcg_action action = { .enable = false };

	smp_call_function_single(cpu->id, cpu_hwcg_rw, &action, 1);
}

static int kpss_cpu_in_hwcg_mode(struct clk *c)
{
	struct kpss_core_clk *cpu = to_kpss_core_clk(c);
	struct cpu_hwcg_action action = { .read = true };

	smp_call_function_single(cpu->id, cpu_hwcg_rw, &action, 1);
	return action.enable;
}

static enum handoff kpss_cpu_handoff(struct clk *c)
{
	struct kpss_core_clk *cpu = to_kpss_core_clk(c);

	c->rate = clk_get_rate(c->parent);

	/*
	 * Don't unnecessarily turn on the parents for an offline CPU and
	 * then have them turned off at late init.
	 */
	return (cpu_online(cpu->id) ?
		HANDOFF_ENABLED_CLK : HANDOFF_DISABLED_CLK);
}

u32 find_dscr(struct avs_data *t, unsigned long rate)
{
	int i;

	if (!t)
		return 0;

	for (i = 0; i < t->num; i++) {
		if (t->rate[i] == rate)
			return t->dscr[i];
	}

	return 0;
}

static int kpss_cpu_pre_set_rate(struct clk *c, unsigned long new_rate)
{
	struct kpss_core_clk *cpu = to_kpss_core_clk(c);
	u32 dscr = find_dscr(cpu->avs_tbl, c->rate);

	if (dscr)
		AVS_DISABLE(cpu->id);
	return 0;
}

static long kpss_core_round_rate(struct clk *c, unsigned long rate)
{
	if (c->fmax && c->num_fmax)
		rate = min(rate, c->fmax[c->num_fmax-1]);

	return clk_round_rate(c->parent, rate);
}

static int kpss_core_set_rate(struct clk *c, unsigned long rate)
{
	return clk_set_rate(c->parent, rate);
}

static void kpss_cpu_post_set_rate(struct clk *c, unsigned long old_rate)
{
	struct kpss_core_clk *cpu = to_kpss_core_clk(c);
	u32 dscr = find_dscr(cpu->avs_tbl, c->rate);

	/*
	 * FIXME: If AVS enable/disable needs to be done in the
	 * enable/disable op to correctly handle power collapse, then might
	 * need to grab the spinlock here.
	 */
	if (dscr)
		AVS_ENABLE(cpu->id, dscr);
}

static unsigned long kpss_core_get_rate(struct clk *c)
{
	return clk_get_rate(c->parent);
}

static long kpss_core_list_rate(struct clk *c, unsigned n)
{
	if (!c->fmax || c->num_fmax <= n)
		return -ENXIO;

	return c->fmax[n];
}

struct clk_ops clk_ops_kpss_cpu = {
	.enable_hwcg = kpss_cpu_enable_hwcg,
	.disable_hwcg = kpss_cpu_disable_hwcg,
	.in_hwcg_mode = kpss_cpu_in_hwcg_mode,
	.pre_set_rate = kpss_cpu_pre_set_rate,
	.round_rate = kpss_core_round_rate,
	.set_rate = kpss_core_set_rate,
	.post_set_rate = kpss_cpu_post_set_rate,
	.get_rate = kpss_core_get_rate,
	.list_rate = kpss_core_list_rate,
	.handoff = kpss_cpu_handoff,
};

#define SLPDLY_SHIFT		10
#define SLPDLY_MASK		0x3
static void kpss_l2_enable_hwcg(struct clk *c)
{
	struct kpss_core_clk *l2 = to_kpss_core_clk(c);
	u32 regval;
	unsigned long flags;

	spin_lock_irqsave(&kpss_clock_reg_lock, flags);
	regval = get_l2_indirect_reg(l2->cp15_iaddr);
	regval &= ~(SLPDLY_MASK << SLPDLY_SHIFT);
	regval |= l2->l2_slp_delay;
	set_l2_indirect_reg(l2->cp15_iaddr, regval);
	spin_unlock_irqrestore(&kpss_clock_reg_lock, flags);
}

static void kpss_l2_disable_hwcg(struct clk *c)
{
	struct kpss_core_clk *l2 = to_kpss_core_clk(c);
	u32 regval;
	unsigned long flags;

	/*
	 * NOTE: Should not be called when HW clock gating is already
	 * disabled.
	 */
	spin_lock_irqsave(&kpss_clock_reg_lock, flags);
	regval = get_l2_indirect_reg(l2->cp15_iaddr);
	l2->l2_slp_delay = regval & (SLPDLY_MASK << SLPDLY_SHIFT);
	regval |= (SLPDLY_MASK << SLPDLY_SHIFT);
	set_l2_indirect_reg(l2->cp15_iaddr, regval);
	spin_unlock_irqrestore(&kpss_clock_reg_lock, flags);
}

static int kpss_l2_in_hwcg_mode(struct clk *c)
{
	struct kpss_core_clk *l2 = to_kpss_core_clk(c);
	u32 regval;

	regval = get_l2_indirect_reg(l2->cp15_iaddr);
	regval >>= SLPDLY_SHIFT;
	regval &= SLPDLY_MASK;
	return (regval != SLPDLY_MASK);
}

static enum handoff kpss_l2_handoff(struct clk *c)
{
	c->rate = clk_get_rate(c->parent);
	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_kpss_l2 = {
	.enable_hwcg = kpss_l2_enable_hwcg,
	.disable_hwcg = kpss_l2_disable_hwcg,
	.in_hwcg_mode = kpss_l2_in_hwcg_mode,
	.round_rate = kpss_core_round_rate,
	.set_rate = kpss_core_set_rate,
	.get_rate = kpss_core_get_rate,
	.list_rate = kpss_core_list_rate,
	.handoff = kpss_l2_handoff,
};
