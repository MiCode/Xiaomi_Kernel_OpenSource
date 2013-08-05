/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/init.h>
#include <linux/err.h>
#include <linux/ctype.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <mach/clk.h>
#include <mach/clk-provider.h>

#include "clock-local2.h"

/*
 * When enabling/disabling a clock, check the halt bit up to this number
 * number of times (with a 1 us delay in between) before continuing.
 */
#define HALT_CHECK_MAX_LOOPS	500
/* For clock without halt checking, wait this long after enables/disables. */
#define HALT_CHECK_DELAY_US	10

/*
 * When updating an RCG configuration, check the update bit up to this number
 * number of times (with a 1 us delay in between) before continuing.
 */
#define UPDATE_CHECK_MAX_LOOPS	500

DEFINE_SPINLOCK(local_clock_reg_lock);
struct clk_freq_tbl rcg_dummy_freq = F_END;

#define CMD_RCGR_REG(x) (*(x)->base + (x)->cmd_rcgr_reg)
#define CFG_RCGR_REG(x) (*(x)->base + (x)->cmd_rcgr_reg + 0x4)
#define M_REG(x)	(*(x)->base + (x)->cmd_rcgr_reg + 0x8)
#define N_REG(x)	(*(x)->base + (x)->cmd_rcgr_reg + 0xC)
#define D_REG(x)	(*(x)->base + (x)->cmd_rcgr_reg + 0x10)
#define CBCR_REG(x)	(*(x)->base + (x)->cbcr_reg)
#define BCR_REG(x)	(*(x)->base + (x)->bcr_reg)
#define VOTE_REG(x)	(*(x)->base + (x)->vote_reg)

/*
 * Important clock bit positions and masks
 */
#define CMD_RCGR_ROOT_ENABLE_BIT	BIT(1)
#define CBCR_BRANCH_ENABLE_BIT		BIT(0)
#define CBCR_BRANCH_OFF_BIT		BIT(31)
#define CMD_RCGR_CONFIG_UPDATE_BIT	BIT(0)
#define CMD_RCGR_ROOT_STATUS_BIT	BIT(31)
#define BCR_BLK_ARES_BIT		BIT(0)
#define CBCR_HW_CTL_BIT			BIT(1)
#define CFG_RCGR_DIV_MASK		BM(4, 0)
#define CFG_RCGR_SRC_SEL_MASK		BM(10, 8)
#define MND_MODE_MASK			BM(13, 12)
#define MND_DUAL_EDGE_MODE_BVAL		BVAL(13, 12, 0x2)
#define CMD_RCGR_CONFIG_DIRTY_MASK	BM(7, 4)
#define CBCR_CDIV_LSB			16
#define CBCR_CDIV_MSB			24

enum branch_state {
	BRANCH_ON,
	BRANCH_OFF,
};

/*
 * RCG functions
 */

/*
 * Update an RCG with a new configuration. This may include a new M, N, or D
 * value, source selection or pre-divider value.
 *
 */
static void rcg_update_config(struct rcg_clk *rcg)
{
	u32 cmd_rcgr_regval, count;

	cmd_rcgr_regval = readl_relaxed(CMD_RCGR_REG(rcg));
	cmd_rcgr_regval |= CMD_RCGR_CONFIG_UPDATE_BIT;
	writel_relaxed(cmd_rcgr_regval, CMD_RCGR_REG(rcg));

	/* Wait for update to take effect */
	for (count = UPDATE_CHECK_MAX_LOOPS; count > 0; count--) {
		if (!(readl_relaxed(CMD_RCGR_REG(rcg)) &
				CMD_RCGR_CONFIG_UPDATE_BIT))
			return;
		udelay(1);
	}

	WARN(count == 0, "%s: rcg didn't update its configuration.",
		rcg->c.dbg_name);
}

/* RCG set rate function for clocks with Half Integer Dividers. */
void set_rate_hid(struct rcg_clk *rcg, struct clk_freq_tbl *nf)
{
	u32 cfg_regval;
	unsigned long flags;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	cfg_regval = readl_relaxed(CFG_RCGR_REG(rcg));
	cfg_regval &= ~(CFG_RCGR_DIV_MASK | CFG_RCGR_SRC_SEL_MASK);
	cfg_regval |= nf->div_src_val;
	writel_relaxed(cfg_regval, CFG_RCGR_REG(rcg));

	rcg_update_config(rcg);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

/* RCG set rate function for clocks with MND & Half Integer Dividers. */
void set_rate_mnd(struct rcg_clk *rcg, struct clk_freq_tbl *nf)
{
	u32 cfg_regval;
	unsigned long flags;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	cfg_regval = readl_relaxed(CFG_RCGR_REG(rcg));
	writel_relaxed(nf->m_val, M_REG(rcg));
	writel_relaxed(nf->n_val, N_REG(rcg));
	writel_relaxed(nf->d_val, D_REG(rcg));

	cfg_regval = readl_relaxed(CFG_RCGR_REG(rcg));
	cfg_regval &= ~(CFG_RCGR_DIV_MASK | CFG_RCGR_SRC_SEL_MASK);
	cfg_regval |= nf->div_src_val;

	/* Activate or disable the M/N:D divider as necessary */
	cfg_regval &= ~MND_MODE_MASK;
	if (nf->n_val != 0)
		cfg_regval |= MND_DUAL_EDGE_MODE_BVAL;
	writel_relaxed(cfg_regval, CFG_RCGR_REG(rcg));

	rcg_update_config(rcg);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static int rcg_clk_prepare(struct clk *c)
{
	struct rcg_clk *rcg = to_rcg_clk(c);

	WARN(rcg->current_freq == &rcg_dummy_freq,
		"Attempting to prepare %s before setting its rate. "
		"Set the rate first!\n", rcg->c.dbg_name);

	return 0;
}

static int rcg_clk_set_rate(struct clk *c, unsigned long rate)
{
	struct clk_freq_tbl *cf, *nf;
	struct rcg_clk *rcg = to_rcg_clk(c);
	int rc;
	unsigned long flags;

	for (nf = rcg->freq_tbl; nf->freq_hz != FREQ_END
			&& nf->freq_hz != rate; nf++)
		;

	if (nf->freq_hz == FREQ_END)
		return -EINVAL;

	cf = rcg->current_freq;

	rc = __clk_pre_reparent(c, nf->src_clk, &flags);
	if (rc)
		return rc;

	BUG_ON(!rcg->set_rate);

	/* Perform clock-specific frequency switch operations. */
	rcg->set_rate(rcg, nf);
	rcg->current_freq = nf;
	c->parent = nf->src_clk;

	__clk_post_reparent(c, cf->src_clk, &flags);

	return 0;
}

/*
 * Return a supported rate that's at least the specified rate or
 * the max supported rate if the specified rate is larger than the
 * max supported rate.
 */
static long rcg_clk_round_rate(struct clk *c, unsigned long rate)
{
	struct rcg_clk *rcg = to_rcg_clk(c);
	struct clk_freq_tbl *f;

	for (f = rcg->freq_tbl; f->freq_hz != FREQ_END; f++)
		if (f->freq_hz >= rate)
			return f->freq_hz;

	f--;
	return f->freq_hz;
}

/* Return the nth supported frequency for a given clock. */
static long rcg_clk_list_rate(struct clk *c, unsigned n)
{
	struct rcg_clk *rcg = to_rcg_clk(c);

	if (!rcg->freq_tbl || rcg->freq_tbl->freq_hz == FREQ_END)
		return -ENXIO;

	return (rcg->freq_tbl + n)->freq_hz;
}

static struct clk *_rcg_clk_get_parent(struct rcg_clk *rcg, int has_mnd)
{
	u32 n_regval = 0, m_regval = 0, d_regval = 0;
	u32 cfg_regval;
	struct clk_freq_tbl *freq;
	u32 cmd_rcgr_regval;

	/* Is there a pending configuration? */
	cmd_rcgr_regval = readl_relaxed(CMD_RCGR_REG(rcg));
	if (cmd_rcgr_regval & CMD_RCGR_CONFIG_DIRTY_MASK)
		return NULL;

	/* Get values of m, n, d, div and src_sel registers. */
	if (has_mnd) {
		m_regval = readl_relaxed(M_REG(rcg));
		n_regval = readl_relaxed(N_REG(rcg));
		d_regval = readl_relaxed(D_REG(rcg));

		/*
		 * The n and d values stored in the frequency tables are sign
		 * extended to 32 bits. The n and d values in the registers are
		 * sign extended to 8 or 16 bits. Sign extend the values read
		 * from the registers so that they can be compared to the
		 * values in the frequency tables.
		 */
		n_regval |= (n_regval >> 8) ? BM(31, 16) : BM(31, 8);
		d_regval |= (d_regval >> 8) ? BM(31, 16) : BM(31, 8);
	}

	cfg_regval = readl_relaxed(CFG_RCGR_REG(rcg));
	cfg_regval &= CFG_RCGR_SRC_SEL_MASK | CFG_RCGR_DIV_MASK
				| MND_MODE_MASK;

	/* If mnd counter is present, check if it's in use. */
	has_mnd = (has_mnd) &&
		((cfg_regval & MND_MODE_MASK) == MND_DUAL_EDGE_MODE_BVAL);

	/*
	 * Clear out the mn counter mode bits since we now want to compare only
	 * the source mux selection and pre-divider values in the registers.
	 */
	cfg_regval &= ~MND_MODE_MASK;

	/* Figure out what rate the rcg is running at */
	for (freq = rcg->freq_tbl; freq->freq_hz != FREQ_END; freq++) {
		if (freq->div_src_val != cfg_regval)
			continue;
		if (has_mnd) {
			if (freq->m_val != m_regval)
				continue;
			if (freq->n_val != n_regval)
				continue;
			if (freq->d_val != d_regval)
				continue;
		}
		break;
	}

	/* No known frequency found */
	if (freq->freq_hz == FREQ_END)
		return NULL;

	rcg->current_freq = freq;
	return freq->src_clk;
}

static enum handoff _rcg_clk_handoff(struct rcg_clk *rcg)
{
	u32 cmd_rcgr_regval;

	if (rcg->current_freq && rcg->current_freq->freq_hz != FREQ_END)
		rcg->c.rate = rcg->current_freq->freq_hz;

	/* Is the root enabled? */
	cmd_rcgr_regval = readl_relaxed(CMD_RCGR_REG(rcg));
	if ((cmd_rcgr_regval & CMD_RCGR_ROOT_STATUS_BIT))
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

static struct clk *rcg_mnd_clk_get_parent(struct clk *c)
{
	return _rcg_clk_get_parent(to_rcg_clk(c), 1);
}

static struct clk *rcg_clk_get_parent(struct clk *c)
{
	return _rcg_clk_get_parent(to_rcg_clk(c), 0);
}

static enum handoff rcg_mnd_clk_handoff(struct clk *c)
{
	return _rcg_clk_handoff(to_rcg_clk(c));
}

static enum handoff rcg_clk_handoff(struct clk *c)
{
	return _rcg_clk_handoff(to_rcg_clk(c));
}

#define BRANCH_CHECK_MASK	BM(31, 28)
#define BRANCH_ON_VAL		BVAL(31, 28, 0x0)
#define BRANCH_OFF_VAL		BVAL(31, 28, 0x8)
#define BRANCH_NOC_FSM_ON_VAL	BVAL(31, 28, 0x2)

/*
 * Branch clock functions
 */
static void branch_clk_halt_check(u32 halt_check, const char *clk_name,
				  void __iomem *cbcr_reg,
				  enum branch_state br_status)
{
	char *status_str = (br_status == BRANCH_ON) ? "off" : "on";

	/*
	 * Use a memory barrier since some halt status registers are
	 * not within the same 1K segment as the branch/root enable
	 * registers.  It's also needed in the udelay() case to ensure
	 * the delay starts after the branch disable.
	 */
	mb();

	if (halt_check == DELAY || halt_check == HALT_VOTED) {
		udelay(HALT_CHECK_DELAY_US);
	} else if (halt_check == HALT) {
		int count;
		u32 val;
		for (count = HALT_CHECK_MAX_LOOPS; count > 0; count--) {
			val = readl_relaxed(cbcr_reg);
			val &= BRANCH_CHECK_MASK;
			switch (br_status) {
			case BRANCH_ON:
				if (val == BRANCH_ON_VAL
					|| val == BRANCH_NOC_FSM_ON_VAL)
					return;
				break;

			case BRANCH_OFF:
				if (val == BRANCH_OFF_VAL)
					return;
				break;
			};
			udelay(1);
		}
		WARN(count == 0, "%s status stuck %s", clk_name, status_str);
	}
}

static int branch_clk_enable(struct clk *c)
{
	unsigned long flags;
	u32 cbcr_val;
	struct branch_clk *branch = to_branch_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	cbcr_val = readl_relaxed(CBCR_REG(branch));
	cbcr_val |= CBCR_BRANCH_ENABLE_BIT;
	writel_relaxed(cbcr_val, CBCR_REG(branch));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Wait for clock to enable before continuing. */
	branch_clk_halt_check(branch->halt_check, branch->c.dbg_name,
				CBCR_REG(branch), BRANCH_ON);

	return 0;
}

static void branch_clk_disable(struct clk *c)
{
	unsigned long flags;
	struct branch_clk *branch = to_branch_clk(c);
	u32 reg_val;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	reg_val = readl_relaxed(CBCR_REG(branch));
	reg_val &= ~CBCR_BRANCH_ENABLE_BIT;
	writel_relaxed(reg_val, CBCR_REG(branch));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Wait for clock to disable before continuing. */
	branch_clk_halt_check(branch->halt_check, branch->c.dbg_name,
				CBCR_REG(branch), BRANCH_OFF);
}

static int branch_cdiv_set_rate(struct branch_clk *branch, unsigned long rate)
{
	unsigned long flags;
	u32 regval;

	if (rate > branch->max_div)
		return -EINVAL;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	regval = readl_relaxed(CBCR_REG(branch));
	regval &= ~BM(CBCR_CDIV_MSB, CBCR_CDIV_LSB);
	regval |= BVAL(CBCR_CDIV_MSB, CBCR_CDIV_LSB, rate);
	writel_relaxed(regval, CBCR_REG(branch));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

static int branch_clk_set_rate(struct clk *c, unsigned long rate)
{
	struct branch_clk *branch = to_branch_clk(c);

	if (branch->max_div)
		return branch_cdiv_set_rate(branch, rate);

	if (!branch->has_sibling)
		return clk_set_rate(c->parent, rate);

	return -EPERM;
}

static long branch_clk_round_rate(struct clk *c, unsigned long rate)
{
	struct branch_clk *branch = to_branch_clk(c);

	if (branch->max_div)
		return rate <= (branch->max_div) ? rate : -EPERM;

	if (!branch->has_sibling)
		return clk_round_rate(c->parent, rate);

	return -EPERM;
}

static unsigned long branch_clk_get_rate(struct clk *c)
{
	struct branch_clk *branch = to_branch_clk(c);

	if (branch->max_div)
		return branch->c.rate;

	return clk_get_rate(c->parent);
}

static long branch_clk_list_rate(struct clk *c, unsigned n)
{
	int level, fmax = 0, rate;
	struct branch_clk *branch = to_branch_clk(c);
	struct clk *parent = c->parent;

	if (branch->has_sibling == 1)
		return -ENXIO;

	if (!parent || !parent->ops->list_rate)
		return -ENXIO;

	/* Find max frequency supported within voltage constraints. */
	if (!parent->vdd_class) {
		fmax = INT_MAX;
	} else {
		for (level = 0; level < parent->num_fmax; level++)
			if (parent->fmax[level])
				fmax = parent->fmax[level];
	}

	rate = parent->ops->list_rate(parent, n);
	if (rate <= fmax)
		return rate;
	else
		return -ENXIO;
}

static enum handoff branch_clk_handoff(struct clk *c)
{
	struct branch_clk *branch = to_branch_clk(c);
	u32 cbcr_regval;

	cbcr_regval = readl_relaxed(CBCR_REG(branch));
	if ((cbcr_regval & CBCR_BRANCH_OFF_BIT))
		return HANDOFF_DISABLED_CLK;

	if (branch->max_div) {
		cbcr_regval &= BM(CBCR_CDIV_MSB, CBCR_CDIV_LSB);
		cbcr_regval >>= CBCR_CDIV_LSB;
		c->rate = cbcr_regval;
	} else if (!branch->has_sibling) {
		c->rate = clk_get_rate(c->parent);
	}

	return HANDOFF_ENABLED_CLK;
}

static int __branch_clk_reset(void __iomem *bcr_reg,
				enum clk_reset_action action)
{
	int ret = 0;
	unsigned long flags;
	u32 reg_val;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	reg_val = readl_relaxed(bcr_reg);
	switch (action) {
	case CLK_RESET_ASSERT:
		reg_val |= BCR_BLK_ARES_BIT;
		break;
	case CLK_RESET_DEASSERT:
		reg_val &= ~BCR_BLK_ARES_BIT;
		break;
	default:
		ret = -EINVAL;
	}
	writel_relaxed(reg_val, bcr_reg);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Make sure write is issued before returning. */
	mb();

	return ret;
}

static int branch_clk_reset(struct clk *c, enum clk_reset_action action)
{
	struct branch_clk *branch = to_branch_clk(c);

	if (!branch->bcr_reg)
		return -EPERM;
	return __branch_clk_reset(BCR_REG(branch), action);
}

static int branch_clk_set_flags(struct clk *c, unsigned flags)
{
	u32 cbcr_val;
	unsigned long irq_flags;
	struct branch_clk *branch = to_branch_clk(c);
	int delay_us = 0, ret = 0;

	spin_lock_irqsave(&local_clock_reg_lock, irq_flags);
	cbcr_val = readl_relaxed(CBCR_REG(branch));
	switch (flags) {
	case CLKFLAG_RETAIN_PERIPH:
		cbcr_val |= BIT(13);
		delay_us = 1;
		break;
	case CLKFLAG_NORETAIN_PERIPH:
		cbcr_val &= ~BIT(13);
		break;
	case CLKFLAG_RETAIN_MEM:
		cbcr_val |= BIT(14);
		delay_us = 1;
		break;
	case CLKFLAG_NORETAIN_MEM:
		cbcr_val &= ~BIT(14);
		break;
	default:
		ret = -EINVAL;
	}
	writel_relaxed(cbcr_val, CBCR_REG(branch));
	/* Make sure power is enabled before returning. */
	mb();
	udelay(delay_us);

	spin_unlock_irqrestore(&local_clock_reg_lock, irq_flags);

	return ret;
}

/*
 * Voteable clock functions
 */
static int local_vote_clk_reset(struct clk *c, enum clk_reset_action action)
{
	struct local_vote_clk *vclk = to_local_vote_clk(c);

	if (!vclk->bcr_reg) {
		WARN("clk_reset called on an unsupported clock (%s)\n",
			c->dbg_name);
		return -EPERM;
	}
	return __branch_clk_reset(BCR_REG(vclk), action);
}

static int local_vote_clk_enable(struct clk *c)
{
	unsigned long flags;
	u32 ena;
	struct local_vote_clk *vclk = to_local_vote_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	ena = readl_relaxed(VOTE_REG(vclk));
	ena |= vclk->en_mask;
	writel_relaxed(ena, VOTE_REG(vclk));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	branch_clk_halt_check(vclk->halt_check, c->dbg_name, CBCR_REG(vclk),
				BRANCH_ON);

	return 0;
}

static void local_vote_clk_disable(struct clk *c)
{
	unsigned long flags;
	u32 ena;
	struct local_vote_clk *vclk = to_local_vote_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	ena = readl_relaxed(VOTE_REG(vclk));
	ena &= ~vclk->en_mask;
	writel_relaxed(ena, VOTE_REG(vclk));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static enum handoff local_vote_clk_handoff(struct clk *c)
{
	struct local_vote_clk *vclk = to_local_vote_clk(c);
	u32 vote_regval;

	/* Is the branch voted on by apps? */
	vote_regval = readl_relaxed(VOTE_REG(vclk));
	if (!(vote_regval & vclk->en_mask))
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

enum handoff byte_rcg_handoff(struct clk *clk)
{
	struct rcg_clk *rcg = to_rcg_clk(clk);
	u32 div_val;
	unsigned long pre_div_rate, parent_rate = clk_get_rate(clk->parent);

	/* If the pre-divider is used, find the rate after the division */
	div_val = readl_relaxed(CFG_RCGR_REG(rcg)) & CFG_RCGR_DIV_MASK;
	if (div_val > 1)
		pre_div_rate = parent_rate / ((div_val + 1) >> 1);
	else
		pre_div_rate = parent_rate;

	clk->rate = pre_div_rate;

	if (readl_relaxed(CMD_RCGR_REG(rcg)) & CMD_RCGR_ROOT_STATUS_BIT)
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

static int set_rate_byte(struct clk *clk, unsigned long rate)
{
	struct rcg_clk *rcg = to_rcg_clk(clk);
	struct clk *pll = clk->parent;
	unsigned long source_rate, div;
	struct clk_freq_tbl *byte_freq = rcg->current_freq;
	int rc;

	if (rate == 0)
		return -EINVAL;

	rc = clk_set_rate(pll, rate);
	if (rc)
		return rc;

	source_rate = clk_round_rate(pll, rate);
	if ((2 * source_rate) % rate)
		return -EINVAL;

	div = ((2 * source_rate)/rate) - 1;
	if (div > CFG_RCGR_DIV_MASK)
		return -EINVAL;

	byte_freq->div_src_val &= ~CFG_RCGR_DIV_MASK;
	byte_freq->div_src_val |= BVAL(4, 0, div);
	set_rate_hid(rcg, byte_freq);

	return 0;
}

enum handoff pixel_rcg_handoff(struct clk *clk)
{
	struct rcg_clk *rcg = to_rcg_clk(clk);
	u32 div_val = 0, mval = 0, nval = 0, cfg_regval;
	unsigned long pre_div_rate, parent_rate = clk_get_rate(clk->parent);

	cfg_regval = readl_relaxed(CFG_RCGR_REG(rcg));

	/* If the pre-divider is used, find the rate after the division */
	div_val = cfg_regval & CFG_RCGR_DIV_MASK;
	if (div_val > 1)
		pre_div_rate = parent_rate / ((div_val + 1) >> 1);
	else
		pre_div_rate = parent_rate;

	clk->rate = pre_div_rate;

	/*
	 * Pixel clocks have one frequency entry in their frequency table.
	 * Update that entry.
	 */
	if (rcg->current_freq) {
		rcg->current_freq->div_src_val &= ~CFG_RCGR_DIV_MASK;
		rcg->current_freq->div_src_val |= div_val;
	}

	/* If MND is used, find the rate after the MND division */
	if ((cfg_regval & MND_MODE_MASK) == MND_DUAL_EDGE_MODE_BVAL) {
		mval = readl_relaxed(M_REG(rcg));
		nval = readl_relaxed(N_REG(rcg));
		if (!nval)
			return HANDOFF_DISABLED_CLK;
		nval = (~nval) + mval;
		if (rcg->current_freq) {
			rcg->current_freq->n_val = ~(nval - mval);
			rcg->current_freq->m_val = mval;
			rcg->current_freq->d_val = ~nval;
		}
		clk->rate = (pre_div_rate * mval) / nval;
	}

	if (readl_relaxed(CMD_RCGR_REG(rcg)) & CMD_RCGR_ROOT_STATUS_BIT)
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

static int set_rate_pixel(struct clk *clk, unsigned long rate)
{
	struct rcg_clk *rcg = to_rcg_clk(clk);
	struct clk *pll = clk->parent;
	unsigned long source_rate, div;
	struct clk_freq_tbl *pixel_freq = rcg->current_freq;
	int rc;

	if (rate == 0)
		return -EINVAL;

	rc = clk_set_rate(pll, rate);
	if (rc)
		return rc;

	source_rate = clk_round_rate(pll, rate);
	if ((2 * source_rate) % rate)
		return -EINVAL;

	div = ((2 * source_rate)/rate) - 1;
	if (div > CFG_RCGR_DIV_MASK)
		return -EINVAL;

	pixel_freq->div_src_val &= ~CFG_RCGR_DIV_MASK;
	pixel_freq->div_src_val |= BVAL(4, 0, div);
	set_rate_mnd(rcg, pixel_freq);

	return 0;
}

/*
 * Unlike other clocks, the HDMI rate is adjusted through PLL
 * re-programming. It is also routed through an HID divider.
 */
static int rcg_clk_set_rate_hdmi(struct clk *c, unsigned long rate)
{
	struct clk_freq_tbl *nf;
	struct rcg_clk *rcg = to_rcg_clk(c);
	int rc;

	for (nf = rcg->freq_tbl; nf->freq_hz != rate; nf++)
		if (nf->freq_hz == FREQ_END) {
			rc = -EINVAL;
			goto out;
		}

	rc = clk_set_rate(nf->src_clk, rate);
	if (rc < 0)
		goto out;
	set_rate_hid(rcg, nf);

	rcg->current_freq = nf;
	c->parent = nf->src_clk;
out:
	return rc;
}


#define ENABLE_REG(x)	(*(x)->base + (x)->enable_reg)
#define SELECT_REG(x)	(*(x)->base + (x)->select_reg)

/*
 * mux clock functions
 */
static void cam_mux_clk_halt_check(void)
{
	/* Ensure that the delay starts after the mux disable/enable. */
	mb();
	udelay(HALT_CHECK_DELAY_US);
}

static int cam_mux_clk_enable(struct clk *c)
{
	unsigned long flags;
	u32 regval;
	struct cam_mux_clk *mux = to_cam_mux_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	regval = readl_relaxed(ENABLE_REG(mux));
	regval |= mux->enable_mask;
	writel_relaxed(regval, ENABLE_REG(mux));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Wait for clock to enable before continuing. */
	cam_mux_clk_halt_check();

	return 0;
}

static void cam_mux_clk_disable(struct clk *c)
{
	unsigned long flags;
	struct cam_mux_clk *mux = to_cam_mux_clk(c);
	u32 regval;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	regval = readl_relaxed(ENABLE_REG(mux));
	regval &= ~mux->enable_mask;
	writel_relaxed(regval, ENABLE_REG(mux));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Wait for clock to disable before continuing. */
	cam_mux_clk_halt_check();
}

static int mux_source_switch(struct cam_mux_clk *mux, struct mux_source *dest)
{
	unsigned long flags;
	u32 regval;
	int ret = 0;

	ret = __clk_pre_reparent(&mux->c, dest->clk, &flags);
	if (ret)
		goto out;

	regval = readl_relaxed(SELECT_REG(mux));
	regval &= ~mux->select_mask;
	regval |= dest->select_val;
	writel_relaxed(regval, SELECT_REG(mux));

	/* Make sure switch request goes through before proceeding. */
	mb();

	__clk_post_reparent(&mux->c, mux->c.parent, &flags);
out:
	return ret;
}

static int cam_mux_clk_set_parent(struct clk *c, struct clk *parent)
{
	struct cam_mux_clk *mux = to_cam_mux_clk(c);
	struct mux_source *dest = NULL;
	int ret;

	if (!mux->sources || !parent)
		return -EPERM;

	dest = mux->sources;

	while (dest->clk) {
		if (dest->clk == parent)
			break;
		dest++;
	}

	if (!dest->clk)
		return -EPERM;

	ret = mux_source_switch(mux, dest);
	if (ret)
		return ret;

	mux->c.rate = clk_get_rate(dest->clk);

	return 0;
}

static enum handoff cam_mux_clk_handoff(struct clk *c)
{
	struct cam_mux_clk *mux = to_cam_mux_clk(c);
	u32 mask = mux->enable_mask;
	u32 regval = readl_relaxed(ENABLE_REG(mux));

	c->rate = clk_get_rate(c->parent);

	if (mask == (regval & mask))
		return HANDOFF_ENABLED_CLK;

	return HANDOFF_DISABLED_CLK;
}

static struct clk *cam_mux_clk_get_parent(struct clk *c)
{
	struct cam_mux_clk *mux = to_cam_mux_clk(c);
	struct mux_source *parent = NULL;
	u32 regval = readl_relaxed(SELECT_REG(mux));

	if (!mux->sources)
		return ERR_PTR(-EPERM);

	parent = mux->sources;

	while (parent->clk) {
		if ((regval & mux->select_mask) == parent->select_val)
			return parent->clk;

		parent++;
	}

	return ERR_PTR(-EPERM);
}

static long cam_mux_clk_list_rate(struct clk *c, unsigned n)
{
	struct cam_mux_clk *mux = to_cam_mux_clk(c);
	int i;

	for (i = 0; i < n; i++)
		if (!mux->sources[i].clk)
			break;

	if (!mux->sources[i].clk)
		return -ENXIO;

	return clk_get_rate(mux->sources[i].clk);
}

struct clk_ops clk_ops_empty;

struct clk_ops clk_ops_rcg = {
	.enable = rcg_clk_prepare,
	.set_rate = rcg_clk_set_rate,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = rcg_clk_handoff,
	.get_parent = rcg_clk_get_parent,
};

struct clk_ops clk_ops_rcg_mnd = {
	.enable = rcg_clk_prepare,
	.set_rate = rcg_clk_set_rate,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = rcg_mnd_clk_handoff,
	.get_parent = rcg_mnd_clk_get_parent,
};

struct clk_ops clk_ops_pixel = {
	.enable = rcg_clk_prepare,
	.set_rate = set_rate_pixel,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = pixel_rcg_handoff,
};

struct clk_ops clk_ops_byte = {
	.enable = rcg_clk_prepare,
	.set_rate = set_rate_byte,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = byte_rcg_handoff,
};

struct clk_ops clk_ops_rcg_hdmi = {
	.enable = rcg_clk_prepare,
	.set_rate = rcg_clk_set_rate_hdmi,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = rcg_clk_handoff,
	.get_parent = rcg_clk_get_parent,
};

struct clk_ops clk_ops_branch = {
	.enable = branch_clk_enable,
	.disable = branch_clk_disable,
	.set_rate = branch_clk_set_rate,
	.get_rate = branch_clk_get_rate,
	.list_rate = branch_clk_list_rate,
	.round_rate = branch_clk_round_rate,
	.reset = branch_clk_reset,
	.set_flags = branch_clk_set_flags,
	.handoff = branch_clk_handoff,
};

struct clk_ops clk_ops_vote = {
	.enable = local_vote_clk_enable,
	.disable = local_vote_clk_disable,
	.reset = local_vote_clk_reset,
	.handoff = local_vote_clk_handoff,
};

struct clk_ops clk_ops_cam_mux = {
	.enable = cam_mux_clk_enable,
	.disable = cam_mux_clk_disable,
	.set_parent = cam_mux_clk_set_parent,
	.get_parent = cam_mux_clk_get_parent,
	.handoff = cam_mux_clk_handoff,
	.list_rate = cam_mux_clk_list_rate,
};


