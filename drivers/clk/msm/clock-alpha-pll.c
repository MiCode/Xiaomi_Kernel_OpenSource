/*
 * Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#define MODE_REG(pll)		(*pll->base + pll->offset + 0x0)
#define LOCK_REG(pll)		(*pll->base + pll->offset + 0x0)
#define ACTIVE_REG(pll)		(*pll->base + pll->offset + 0x0)
#define UPDATE_REG(pll)		(*pll->base + pll->offset + 0x0)
#define L_REG(pll)		(*pll->base + pll->offset + 0x4)
#define A_REG(pll)		(*pll->base + pll->offset + 0x8)
#define VCO_REG(pll)		(*pll->base + pll->offset + 0x10)
#define ALPHA_EN_REG(pll)	(*pll->base + pll->offset + 0x10)
#define OUTPUT_REG(pll)		(*pll->base + pll->offset + 0x10)
#define VOTE_REG(pll)		(*pll->base + pll->fsm_reg_offset)
#define USER_CTL_LO_REG(pll)	(*pll->base + pll->offset + 0x10)
#define USER_CTL_HI_REG(pll)	(*pll->base + pll->offset + 0x14)
#define CONFIG_CTL_REG(pll)	(*pll->base + pll->offset + 0x18)
#define TEST_CTL_LO_REG(pll)	(*pll->base + pll->offset + 0x1c)
#define TEST_CTL_HI_REG(pll)	(*pll->base + pll->offset + 0x20)

#define PLL_BYPASSNL 0x2
#define PLL_RESET_N  0x4
#define PLL_OUTCTRL  0x1
#define PLL_LATCH_INTERFACE	BIT(11)

#define FABIA_CONFIG_CTL_REG(pll)	(*pll->base + pll->offset + 0x14)
#define FABIA_USER_CTL_LO_REG(pll)	(*pll->base + pll->offset + 0xc)
#define FABIA_USER_CTL_HI_REG(pll)	(*pll->base + pll->offset + 0x10)
#define FABIA_TEST_CTL_LO_REG(pll)	(*pll->base + pll->offset + 0x1c)
#define FABIA_TEST_CTL_HI_REG(pll)	(*pll->base + pll->offset + 0x20)
#define FABIA_L_REG(pll)		(*pll->base + pll->offset + 0x4)
#define FABIA_CAL_L_VAL(pll)		(*pll->base + pll->offset + 0x8)
#define FABIA_FRAC_REG(pll)		(*pll->base + pll->offset + 0x38)
#define FABIA_PLL_OPMODE(pll)		(*pll->base + pll->offset + 0x2c)

#define FABIA_PLL_STANDBY	0x0
#define FABIA_PLL_RUN		0x1
#define FABIA_PLL_OUT_MAIN	0x7
#define FABIA_RATE_MARGIN	500
#define ALPHA_PLL_ACK_LATCH	BIT(29)
#define ALPHA_PLL_HW_UPDATE_LOGIC_BYPASS	BIT(23)

/*
 * Even though 40 bits are present, use only 32 for ease of calculation.
 */
#define ALPHA_REG_BITWIDTH 40
#define ALPHA_BITWIDTH 32
#define FABIA_ALPHA_BITWIDTH 16

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
	int alpha_bw = ALPHA_BITWIDTH;

	if (pll->is_fabia)
		alpha_bw = FABIA_ALPHA_BITWIDTH;

	parent_rate = clk_get_rate(pll->c.parent);
	rate = parent_rate * l_val;
	rate += (parent_rate * a_val) >> alpha_bw;
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

static int __alpha_pll_enable(struct alpha_pll_clk *pll, int enable_output)
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
	if (enable_output) {
		mode |= PLL_OUTCTRL;
		writel_relaxed(mode, MODE_REG(pll));
	}

	/* Ensure that the write above goes through before returning. */
	mb();
	return 0;
}

static void setup_alpha_pll_values(u64 a_val, u32 l_val, u32 vco_val,
				struct alpha_pll_clk *pll)
{
	struct alpha_pll_masks *masks = pll->masks;
	u32 regval;

	a_val = a_val << (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	writel_relaxed(l_val, L_REG(pll));
	__iowrite32_copy(A_REG(pll), &a_val, 2);

	if (vco_val != UINT_MAX) {
		regval = readl_relaxed(VCO_REG(pll));
		regval &= ~(masks->vco_mask << masks->vco_shift);
		regval |= vco_val << masks->vco_shift;
		writel_relaxed(regval, VCO_REG(pll));
	}

	regval = readl_relaxed(ALPHA_EN_REG(pll));
	regval |= masks->alpha_en_mask;
	writel_relaxed(regval, ALPHA_EN_REG(pll));
}

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
		rc = __alpha_pll_enable(pll, true);
	spin_unlock_irqrestore(&alpha_pll_reg_lock, flags);

	return rc;
}

static int __calibrate_alpha_pll(struct alpha_pll_clk *pll);
static int dyna_alpha_pll_enable(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	unsigned long flags;
	int rc;

	if (unlikely(!pll->inited))
		__init_alpha_pll(c);

	spin_lock_irqsave(&alpha_pll_reg_lock, flags);

	if (pll->slew)
		__calibrate_alpha_pll(pll);

	if (pll->fsm_en_mask)
		rc = __alpha_pll_vote_enable(pll);
	else
		rc = __alpha_pll_enable(pll, true);
	spin_unlock_irqrestore(&alpha_pll_reg_lock, flags);

	return rc;
}

#define PLL_OFFLINE_REQ_BIT BIT(7)
#define PLL_FSM_ENA_BIT BIT(20)
#define PLL_OFFLINE_ACK_BIT BIT(28)
#define PLL_ACTIVE_FLAG BIT(30)

static int alpha_pll_enable_hwfsm(struct clk *c)
{
	u32 mode;
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);

	/* Re-enable HW FSM mode, clear OFFLINE request */
	mode = readl_relaxed(MODE_REG(pll));
	mode |= PLL_FSM_ENA_BIT;
	mode &= ~PLL_OFFLINE_REQ_BIT;
	writel_relaxed(mode, MODE_REG(pll));

	/* Make sure enable request goes through before waiting for update */
	mb();

	if (wait_for_update(pll) < 0)
		panic("PLL %s failed to lock", c->dbg_name);

	return 0;
}

static void alpha_pll_disable_hwfsm(struct clk *c)
{
	u32 mode;
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);

	/* Request PLL_OFFLINE and wait for ack */
	mode = readl_relaxed(MODE_REG(pll));
	writel_relaxed(mode | PLL_OFFLINE_REQ_BIT, MODE_REG(pll));
	while (!(readl_relaxed(MODE_REG(pll)) & PLL_OFFLINE_ACK_BIT))
		;

	/* Disable HW FSM */
	mode = readl_relaxed(MODE_REG(pll));
	mode &= ~PLL_FSM_ENA_BIT;
	if (pll->offline_bit_workaround)
		mode &= ~PLL_OFFLINE_REQ_BIT;
	writel_relaxed(mode, MODE_REG(pll));

	while (readl_relaxed(MODE_REG(pll)) & PLL_ACTIVE_FLAG)
		;
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

static void dyna_alpha_pll_disable(struct clk *c)
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
	int alpha_bw = ALPHA_BITWIDTH;

	parent_rate = clk_get_rate(pll->c.parent);
	quotient = rate;
	remainder = do_div(quotient, parent_rate);
	*l_val = quotient;

	if (!remainder) {
		*a_val = 0;
		return rate;
	}

	if (pll->is_fabia)
		alpha_bw = FABIA_ALPHA_BITWIDTH;

	/* Upper ALPHA_BITWIDTH bits of Alpha */
	quotient = remainder << alpha_bw;
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

static bool dynamic_update_finish(struct alpha_pll_clk *pll)
{
	u32 reg = readl_relaxed(UPDATE_REG(pll));
	u32 mask = pll->masks->update_mask;

	return (reg & mask) == 0;
}

static int wait_for_dynamic_update(struct alpha_pll_clk *pll)
{
	int count;

	for (count = WAIT_MAX_LOOPS; count > 0; count--) {
		if (dynamic_update_finish(pll))
			break;
		udelay(1);
	}

	if (!count) {
		pr_err("%s didn't latch after updating it!\n", pll->c.dbg_name);
		return -EINVAL;
	}

	return 0;
}

static int dyna_alpha_pll_dynamic_update(struct alpha_pll_clk *pll)
{
	struct alpha_pll_masks *masks = pll->masks;
	u32 regval;
	int rc;

	regval = readl_relaxed(UPDATE_REG(pll));
	regval |= masks->update_mask;
	writel_relaxed(regval, UPDATE_REG(pll));

	rc = wait_for_dynamic_update(pll);
	if (rc < 0)
		return rc;

	/*
	 * HPG mandates a wait of at least 570ns before polling the LOCK
	 * detect bit. Have a delay of 1us just to be safe.
	 */
	mb();
	udelay(1);

	rc = wait_for_update(pll);
	if (rc < 0)
		return rc;

	return 0;
}

static int alpha_pll_set_rate(struct clk *c, unsigned long rate);
static int dyna_alpha_pll_set_rate(struct clk *c, unsigned long rate)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	unsigned long freq_hz, flags;
	u32 l_val, vco_val;
	u64 a_val;
	int ret;

	freq_hz = round_rate_up(pll, rate, &l_val, &a_val);
	if (freq_hz != rate) {
		pr_err("alpha_pll: Call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	vco_val = find_vco(pll, freq_hz);

	/*
	 * Dynamic pll update will not support switching frequencies across
	 * vco ranges. In those cases fall back to normal alpha set rate.
	 */
	if (pll->current_vco_val != vco_val) {
		ret = alpha_pll_set_rate(c, rate);
		if (!ret)
			pll->current_vco_val = vco_val;
		else
			return ret;
		return 0;
	}

	spin_lock_irqsave(&c->lock, flags);

	a_val = a_val << (ALPHA_REG_BITWIDTH - ALPHA_BITWIDTH);

	writel_relaxed(l_val, L_REG(pll));
	__iowrite32_copy(A_REG(pll), &a_val, 2);

	/* Ensure that the write above goes through before proceeding. */
	mb();

	if (c->count)
		dyna_alpha_pll_dynamic_update(pll);

	spin_unlock_irqrestore(&c->lock, flags);
	return 0;
}

/*
 * Slewing plls should be bought up at frequency which is in the middle of the
 * desired VCO range. So after bringing up the pll at calibration freq, set it
 * back to desired frequency(that was set by previous clk_set_rate).
 */
static int __calibrate_alpha_pll(struct alpha_pll_clk *pll)
{
	unsigned long calibration_freq, freq_hz;
	struct alpha_pll_vco_tbl *vco_tbl = pll->vco_tbl;
	u64 a_val;
	u32 l_val, vco_val;
	int rc;

	vco_val = find_vco(pll, pll->c.rate);
	if (IS_ERR_VALUE(vco_val)) {
		pr_err("alpha pll: not in a valid vco range\n");
		return -EINVAL;
	}
	/*
	 * As during slewing plls vco_sel won't be allowed to change, vco table
	 * should have only one entry table, i.e. index = 0, find the
	 * calibration frequency.
	 */
	calibration_freq = (vco_tbl[0].min_freq +
					vco_tbl[0].max_freq)/2;

	freq_hz = round_rate_up(pll, calibration_freq, &l_val, &a_val);
	if (freq_hz != calibration_freq) {
		pr_err("alpha_pll: call clk_set_rate with rounded rates!\n");
		return -EINVAL;
	}

	setup_alpha_pll_values(a_val, l_val, vco_tbl->vco_val, pll);

	/* Bringup the pll at calibration frequency */
	rc = __alpha_pll_enable(pll, false);
	if (rc) {
		pr_err("alpha pll calibration failed\n");
		return rc;
	}

	/*
	 * PLL is already running at calibration frequency.
	 * So slew pll to the previously set frequency.
	 */
	pr_debug("pll %s: setting back to required rate %lu\n", pll->c.dbg_name,
					pll->c.rate);
	freq_hz = round_rate_up(pll, pll->c.rate, &l_val, &a_val);
	setup_alpha_pll_values(a_val, l_val, UINT_MAX, pll);
	dyna_alpha_pll_dynamic_update(pll);

	return 0;
}

static int alpha_pll_dynamic_update(struct alpha_pll_clk *pll)
{
	u32 regval;

	/* Latch the input to the PLL */
	regval = readl_relaxed(MODE_REG(pll));
	regval |= pll->masks->update_mask;
	writel_relaxed(regval, MODE_REG(pll));

	/* Wait for 2 reference cycle before checking ACK bit */
	udelay(1);
	if (!(readl_relaxed(MODE_REG(pll)) & ALPHA_PLL_ACK_LATCH)) {
		WARN(1, "%s: PLL latch failed. Output may be unstable!\n",
						pll->c.dbg_name);
		return -EINVAL;
	}

	/* Return latch input to 0 */
	regval = readl_relaxed(MODE_REG(pll));
	regval &= ~pll->masks->update_mask;
	writel_relaxed(regval, MODE_REG(pll));

	/* Wait for PLL output to stabilize */
	udelay(100);

	return 0;
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

	if (pll->no_irq_dis)
		spin_lock(&c->lock);
	else
		spin_lock_irqsave(&c->lock, flags);

	/*
	 * For PLLs that do not support dynamic programming (dynamic_update
	 * is not set), ensure PLL is off before changing rate. For
	 * optimization reasons, assume no downstream clock is actively
	 * using it.
	 */
	if (c->count && !pll->dynamic_update)
		c->ops->disable(c);

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

	if (c->count && pll->dynamic_update)
		alpha_pll_dynamic_update(pll);

	if (c->count && !pll->dynamic_update)
		c->ops->enable(c);

	if (pll->no_irq_dis)
		spin_unlock(&c->lock);
	else
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

	if (pll->no_prepared_reconfig && c->prepare_count)
		return -EINVAL;

	freq_hz = round_rate_up(pll, rate, &l_val, &a_val);
	if (rate < pll->min_supported_freq)
		return pll->min_supported_freq;
	if (pll->is_fabia)
		return freq_hz;

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

void __init_alpha_pll(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	struct alpha_pll_masks *masks = pll->masks;
	u32 regval;

	if (pll->config_ctl_val)
		writel_relaxed(pll->config_ctl_val, CONFIG_CTL_REG(pll));

	if (masks->output_mask && pll->enable_config) {
		regval = readl_relaxed(OUTPUT_REG(pll));
		regval &= ~masks->output_mask;
		regval |= pll->enable_config;
		writel_relaxed(regval, OUTPUT_REG(pll));
	}

	if (masks->post_div_mask) {
		regval = readl_relaxed(USER_CTL_LO_REG(pll));
		regval &= ~masks->post_div_mask;
		regval |= pll->post_div_config;
		writel_relaxed(regval, USER_CTL_LO_REG(pll));
	}

	if (pll->slew) {
		regval = readl_relaxed(USER_CTL_HI_REG(pll));
		regval &= ~PLL_LATCH_INTERFACE;
		writel_relaxed(regval, USER_CTL_HI_REG(pll));
	}

	if (masks->test_ctl_lo_mask) {
		regval = readl_relaxed(TEST_CTL_LO_REG(pll));
		regval &= ~masks->test_ctl_lo_mask;
		regval |= pll->test_ctl_lo_val;
		writel_relaxed(regval, TEST_CTL_LO_REG(pll));
	}

	if (masks->test_ctl_hi_mask) {
		regval = readl_relaxed(TEST_CTL_HI_REG(pll));
		regval &= ~masks->test_ctl_hi_mask;
		regval |= pll->test_ctl_hi_val;
		writel_relaxed(regval, TEST_CTL_HI_REG(pll));
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
	u32 alpha_en, l_val, regval;

	/* Set the PLL_HW_UPDATE_LOGIC_BYPASS bit before continuing */
	if (pll->dynamic_update) {
		regval = readl_relaxed(MODE_REG(pll));
		regval |= ALPHA_PLL_HW_UPDATE_LOGIC_BYPASS;
		writel_relaxed(regval, MODE_REG(pll));
	}

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

	/*
	 * Unconditionally vote for the PLL; it might be on because of
	 * another master's vote.
	 */
	if (pll->fsm_en_mask)
		__alpha_pll_vote_enable(pll);

	return HANDOFF_ENABLED_CLK;
}

static void __iomem *alpha_pll_list_registers(struct clk *clk, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(clk);
	static struct clk_register_data data[] = {
		{"PLL_MODE", 0x0},
		{"PLL_L_VAL", 0x4},
		{"PLL_ALPHA_VAL", 0x8},
		{"PLL_ALPHA_VAL_U", 0xC},
		{"PLL_USER_CTL", 0x10},
		{"PLL_CONFIG_CTL", 0x18},
	};

	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return MODE_REG(pll);
}

static int __fabia_alpha_pll_enable(struct alpha_pll_clk *pll)
{
	int rc;
	u32 mode;

	/* Disable PLL output */
	mode  = readl_relaxed(MODE_REG(pll));
	mode &= ~PLL_OUTCTRL;
	writel_relaxed(mode, MODE_REG(pll));

	/* Set operation mode to STANDBY */
	writel_relaxed(FABIA_PLL_STANDBY, FABIA_PLL_OPMODE(pll));

	/* PLL should be in STANDBY mode before continuing */
	mb();

	/* Bring PLL out of reset */
	mode  = readl_relaxed(MODE_REG(pll));
	mode |= PLL_RESET_N;
	writel_relaxed(mode, MODE_REG(pll));

	/* Set operation mode to RUN */
	writel_relaxed(FABIA_PLL_RUN, FABIA_PLL_OPMODE(pll));

	rc = wait_for_update(pll);
	if (rc < 0)
		return rc;

	/* Enable the main PLL output */
	mode  = readl_relaxed(FABIA_USER_CTL_LO_REG(pll));
	mode |= FABIA_PLL_OUT_MAIN;
	writel_relaxed(mode, FABIA_USER_CTL_LO_REG(pll));

	/* Enable PLL outputs */
	mode  = readl_relaxed(MODE_REG(pll));
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, MODE_REG(pll));

	/* Ensure that the write above goes through before returning. */
	mb();
	return 0;
}

static int fabia_alpha_pll_enable(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	unsigned long flags;
	int rc;

	spin_lock_irqsave(&alpha_pll_reg_lock, flags);
	if (pll->fsm_en_mask)
		rc = __alpha_pll_vote_enable(pll);
	else
		rc = __fabia_alpha_pll_enable(pll);
	spin_unlock_irqrestore(&alpha_pll_reg_lock, flags);

	return rc;
}

static void __fabia_alpha_pll_disable(struct alpha_pll_clk *pll)
{
	u32 mode;

	/* Disable PLL outputs */
	mode  = readl_relaxed(MODE_REG(pll));
	mode &= ~PLL_OUTCTRL;
	writel_relaxed(mode, MODE_REG(pll));

	/* Disable the main PLL output */
	mode  = readl_relaxed(FABIA_USER_CTL_LO_REG(pll));
	mode &= ~FABIA_PLL_OUT_MAIN;
	writel_relaxed(mode, FABIA_USER_CTL_LO_REG(pll));

	/* Place the PLL mode in STANDBY */
	writel_relaxed(FABIA_PLL_STANDBY, FABIA_PLL_OPMODE(pll));
}

static void fabia_alpha_pll_disable(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	unsigned long flags;

	spin_lock_irqsave(&alpha_pll_reg_lock, flags);
	if (pll->fsm_en_mask)
		__alpha_pll_vote_disable(pll);
	else
		__fabia_alpha_pll_disable(pll);
	spin_unlock_irqrestore(&alpha_pll_reg_lock, flags);
}

static int fabia_alpha_pll_set_rate(struct clk *c, unsigned long rate)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	unsigned long flags, freq_hz;
	u32 l_val;
	u64 a_val;

	freq_hz = round_rate_up(pll, rate, &l_val, &a_val);
	if (freq_hz > rate + FABIA_RATE_MARGIN || freq_hz < rate) {
		pr_err("%s: Call clk_set_rate with rounded rates!\n",
						c->dbg_name);
		return -EINVAL;
	}

	spin_lock_irqsave(&c->lock, flags);
	/* Set the new L value */
	writel_relaxed(l_val, FABIA_L_REG(pll));
	/*
	 * pll_cal_l_val is set to pll_l_val on MOST targets. Set it
	 * explicitly here for PLL out-of-reset calibration to work
	 * without a glitch on all of them.
	 */
	writel_relaxed(l_val, FABIA_CAL_L_VAL(pll));
	writel_relaxed(a_val, FABIA_FRAC_REG(pll));

	alpha_pll_dynamic_update(pll);

	spin_unlock_irqrestore(&c->lock, flags);
	return 0;
}

void __init_fabia_alpha_pll(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	struct alpha_pll_masks *masks = pll->masks;
	u32 regval;

	if (pll->config_ctl_val)
		writel_relaxed(pll->config_ctl_val, FABIA_CONFIG_CTL_REG(pll));

	if (masks->output_mask && pll->enable_config) {
		regval = readl_relaxed(FABIA_USER_CTL_LO_REG(pll));
		regval &= ~masks->output_mask;
		regval |= pll->enable_config;
		writel_relaxed(regval, FABIA_USER_CTL_LO_REG(pll));
	}

	if (masks->post_div_mask) {
		regval = readl_relaxed(FABIA_USER_CTL_LO_REG(pll));
		regval &= ~masks->post_div_mask;
		regval |= pll->post_div_config;
		writel_relaxed(regval, FABIA_USER_CTL_LO_REG(pll));
	}

	if (pll->slew) {
		regval = readl_relaxed(FABIA_USER_CTL_HI_REG(pll));
		regval &= ~PLL_LATCH_INTERFACE;
		writel_relaxed(regval, FABIA_USER_CTL_HI_REG(pll));
	}

	if (masks->test_ctl_lo_mask) {
		regval = readl_relaxed(FABIA_TEST_CTL_LO_REG(pll));
		regval &= ~masks->test_ctl_lo_mask;
		regval |= pll->test_ctl_lo_val;
		writel_relaxed(regval, FABIA_TEST_CTL_LO_REG(pll));
	}

	if (masks->test_ctl_hi_mask) {
		regval = readl_relaxed(FABIA_TEST_CTL_HI_REG(pll));
		regval &= ~masks->test_ctl_hi_mask;
		regval |= pll->test_ctl_hi_val;
		writel_relaxed(regval, FABIA_TEST_CTL_HI_REG(pll));
	}

	if (pll->fsm_en_mask)
		__set_fsm_mode(MODE_REG(pll));

	pll->inited = true;
}

static enum handoff fabia_alpha_pll_handoff(struct clk *c)
{
	struct alpha_pll_clk *pll = to_alpha_pll_clk(c);
	u64 a_val;
	u32 l_val, regval;

	/* Set the PLL_HW_UPDATE_LOGIC_BYPASS bit before continuing */
	regval = readl_relaxed(MODE_REG(pll));
	regval |= ALPHA_PLL_HW_UPDATE_LOGIC_BYPASS;
	writel_relaxed(regval, MODE_REG(pll));

	/* Set the PLL_RESET_N bit to place the PLL in STANDBY from OFF */
	regval |= PLL_RESET_N;
	writel_relaxed(regval, MODE_REG(pll));

	if (!is_locked(pll)) {
		if (c->rate && fabia_alpha_pll_set_rate(c, c->rate))
			WARN(1, "%s: Failed to configure rate\n", c->dbg_name);
		__init_fabia_alpha_pll(c);
		return HANDOFF_DISABLED_CLK;
	} else if (pll->fsm_en_mask && !is_fsm_mode(MODE_REG(pll))) {
		WARN(1, "%s should be in FSM mode but is not\n", c->dbg_name);
	}

	l_val = readl_relaxed(FABIA_L_REG(pll));
	a_val = readl_relaxed(FABIA_FRAC_REG(pll));

	c->rate = compute_rate(pll, l_val, a_val);

	/*
	 * Unconditionally vote for the PLL; it might be on because of
	 * another master's vote.
	 */
	if (pll->fsm_en_mask)
		__alpha_pll_vote_enable(pll);

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_alpha_pll = {
	.enable = alpha_pll_enable,
	.disable = alpha_pll_disable,
	.round_rate = alpha_pll_round_rate,
	.set_rate = alpha_pll_set_rate,
	.handoff = alpha_pll_handoff,
	.list_registers = alpha_pll_list_registers,
};

struct clk_ops clk_ops_alpha_pll_hwfsm = {
	.enable = alpha_pll_enable_hwfsm,
	.disable = alpha_pll_disable_hwfsm,
	.round_rate = alpha_pll_round_rate,
	.set_rate = alpha_pll_set_rate,
	.handoff = alpha_pll_handoff,
	.list_registers = alpha_pll_list_registers,
};

struct clk_ops clk_ops_fixed_alpha_pll = {
	.enable = alpha_pll_enable,
	.disable = alpha_pll_disable,
	.handoff = alpha_pll_handoff,
	.list_registers = alpha_pll_list_registers,
};

struct clk_ops clk_ops_fixed_fabia_alpha_pll = {
	.enable = fabia_alpha_pll_enable,
	.disable = fabia_alpha_pll_disable,
	.handoff = fabia_alpha_pll_handoff,
};

struct clk_ops clk_ops_fabia_alpha_pll = {
	.enable = fabia_alpha_pll_enable,
	.disable = fabia_alpha_pll_disable,
	.round_rate = alpha_pll_round_rate,
	.set_rate = fabia_alpha_pll_set_rate,
	.handoff = fabia_alpha_pll_handoff,
};

struct clk_ops clk_ops_dyna_alpha_pll = {
	.enable = dyna_alpha_pll_enable,
	.disable = dyna_alpha_pll_disable,
	.round_rate = alpha_pll_round_rate,
	.set_rate = dyna_alpha_pll_set_rate,
	.handoff = alpha_pll_handoff,
	.list_registers = alpha_pll_list_registers,
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
