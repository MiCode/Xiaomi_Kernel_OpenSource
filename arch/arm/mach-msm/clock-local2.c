/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include "clock.h"
#include "clock-local2.h"

/*
 * When enabling/disabling a clock, check the halt bit up to this number
 * number of times (with a 1 us delay in between) before continuing.
 */
#define HALT_CHECK_MAX_LOOPS	200
/* For clock without halt checking, wait this long after enables/disables. */
#define HALT_CHECK_DELAY_US	10

/*
 * When updating an RCG configuration, check the update bit up to this number
 * number of times (with a 1 us delay in between) before continuing.
 */
#define UPDATE_CHECK_MAX_LOOPS	200

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
#define CBCR_BRANCH_CDIV_MASK		BM(24, 16)
#define CBCR_BRANCH_CDIV_MASKED(val)	BVAL(24, 16, (val));

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

	cfg_regval = readl_relaxed(CFG_RCGR_REG(rcg));
	cfg_regval &= ~(CFG_RCGR_DIV_MASK | CFG_RCGR_SRC_SEL_MASK);
	cfg_regval |= nf->div_src_val;
	writel_relaxed(cfg_regval, CFG_RCGR_REG(rcg));

	rcg_update_config(rcg);
}

/* RCG set rate function for clocks with MND & Half Integer Dividers. */
void set_rate_mnd(struct rcg_clk *rcg, struct clk_freq_tbl *nf)
{
	u32 cfg_regval;

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
}

static int rcg_clk_enable(struct clk *c)
{
	struct rcg_clk *rcg = to_rcg_clk(c);

	WARN(rcg->current_freq == &rcg_dummy_freq,
		"Attempting to enable %s before setting its rate. "
		"Set the rate first!\n", rcg->c.dbg_name);

	return 0;
}

static int rcg_clk_set_rate(struct clk *c, unsigned long rate)
{
	struct clk_freq_tbl *cf, *nf;
	struct rcg_clk *rcg = to_rcg_clk(c);
	int rc = 0;
	unsigned long flags;

	for (nf = rcg->freq_tbl; nf->freq_hz != FREQ_END
			&& nf->freq_hz != rate; nf++)
		;

	if (nf->freq_hz == FREQ_END)
		return -EINVAL;

	cf = rcg->current_freq;

	if (rcg->c.count) {
		/* TODO: Modify to use the prepare API */
		/* Enable source clock dependency for the new freq. */
		rc = clk_enable(nf->src_clk);
		if (rc)
			goto out;
	}

	BUG_ON(!rcg->set_rate);

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Perform clock-specific frequency switch operations. */
	rcg->set_rate(rcg, nf);

	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Release source requirements of the old freq. */
	if (rcg->c.count)
		clk_disable(cf->src_clk);

	rcg->current_freq = nf;
out:
	return rc;
}

/* Return a supported rate that's at least the specified rate. */
static long rcg_clk_round_rate(struct clk *c, unsigned long rate)
{
	struct rcg_clk *rcg = to_rcg_clk(c);
	struct clk_freq_tbl *f;

	for (f = rcg->freq_tbl; f->freq_hz != FREQ_END; f++)
		if (f->freq_hz >= rate)
			return f->freq_hz;

	return -EPERM;
}

/* Return the nth supported frequency for a given clock. */
static int rcg_clk_list_rate(struct clk *c, unsigned n)
{
	struct rcg_clk *rcg = to_rcg_clk(c);

	if (!rcg->freq_tbl || rcg->freq_tbl->freq_hz == FREQ_END)
		return -ENXIO;

	return (rcg->freq_tbl + n)->freq_hz;
}

static struct clk *rcg_clk_get_parent(struct clk *c)
{
	return to_rcg_clk(c)->current_freq->src_clk;
}

static enum handoff _rcg_clk_handoff(struct rcg_clk *rcg, int has_mnd)
{
	u32 n_regval = 0, m_regval = 0, d_regval = 0;
	u32 cfg_regval;
	struct clk_freq_tbl *freq;
	u32 cmd_rcgr_regval;

	/* Is the root enabled? */
	cmd_rcgr_regval = readl_relaxed(CMD_RCGR_REG(rcg));
	if ((cmd_rcgr_regval & CMD_RCGR_ROOT_STATUS_BIT))
		return HANDOFF_DISABLED_CLK;

	/* Is there a pending configuration? */
	if (cmd_rcgr_regval & CMD_RCGR_CONFIG_DIRTY_MASK)
		return HANDOFF_UNKNOWN_RATE;

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
		return HANDOFF_UNKNOWN_RATE;

	rcg->current_freq = freq;
	rcg->c.rate = freq->freq_hz;

	return HANDOFF_ENABLED_CLK;
}

static enum handoff rcg_mnd_clk_handoff(struct clk *c)
{
	return _rcg_clk_handoff(to_rcg_clk(c), 1);
}

static enum handoff rcg_clk_handoff(struct clk *c)
{
	return _rcg_clk_handoff(to_rcg_clk(c), 0);
}

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
		for (count = HALT_CHECK_MAX_LOOPS; count > 0; count--) {
			if (br_status == BRANCH_ON
				&& !(readl_relaxed(cbcr_reg)
						& CBCR_BRANCH_OFF_BIT))
				return;
			if (br_status == BRANCH_OFF
				&& (readl_relaxed(cbcr_reg)
						& CBCR_BRANCH_OFF_BIT))
				return;
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
	regval &= ~CBCR_BRANCH_CDIV_MASK;
	regval |= CBCR_BRANCH_CDIV_MASKED(rate);
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
		return clk_set_rate(branch->parent, rate);

	return -EPERM;
}

static long branch_clk_round_rate(struct clk *c, unsigned long rate)
{
	struct branch_clk *branch = to_branch_clk(c);

	if (branch->max_div)
		return rate <= (branch->max_div) ? rate : -EPERM;

	if (!branch->has_sibling)
		return clk_round_rate(branch->parent, rate);

	return -EPERM;
}

static unsigned long branch_clk_get_rate(struct clk *c)
{
	struct branch_clk *branch = to_branch_clk(c);

	if (branch->max_div)
		return branch->c.rate;

	if (!branch->has_sibling)
		return clk_get_rate(branch->parent);

	return 0;
}

static struct clk *branch_clk_get_parent(struct clk *c)
{
	return to_branch_clk(c)->parent;
}

static int branch_clk_list_rate(struct clk *c, unsigned n)
{
	struct branch_clk *branch = to_branch_clk(c);

	if (branch->has_sibling == 1)
		return -ENXIO;

	if (branch->parent)
		return rcg_clk_list_rate(branch->parent, n);
	else
		return 0;
}

static enum handoff branch_clk_handoff(struct clk *c)
{
	struct branch_clk *branch = to_branch_clk(c);
	u32 cbcr_regval;

	cbcr_regval = readl_relaxed(CBCR_REG(branch));
	if ((cbcr_regval & CBCR_BRANCH_OFF_BIT))
		return HANDOFF_DISABLED_CLK;

	if (branch->parent) {
		if (branch->parent->ops->handoff)
			return branch->parent->ops->handoff(branch->parent);
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

	if (!branch->bcr_reg) {
		WARN("clk_reset called on an unsupported clock (%s)\n",
			c->dbg_name);
		return -EPERM;
	}
	return __branch_clk_reset(BCR_REG(branch), action);
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

struct clk_ops clk_ops_rcg = {
	.enable = rcg_clk_enable,
	.set_rate = rcg_clk_set_rate,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.get_parent = rcg_clk_get_parent,
	.handoff = rcg_clk_handoff,
};

struct clk_ops clk_ops_rcg_mnd = {
	.enable = rcg_clk_enable,
	.set_rate = rcg_clk_set_rate,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.get_parent = rcg_clk_get_parent,
	.handoff = rcg_mnd_clk_handoff,
};

struct clk_ops clk_ops_branch = {
	.enable = branch_clk_enable,
	.disable = branch_clk_disable,
	.set_rate = branch_clk_set_rate,
	.get_rate = branch_clk_get_rate,
	.list_rate = branch_clk_list_rate,
	.round_rate = branch_clk_round_rate,
	.reset = branch_clk_reset,
	.get_parent = branch_clk_get_parent,
	.handoff = branch_clk_handoff,
};

struct clk_ops clk_ops_vote = {
	.enable = local_vote_clk_enable,
	.disable = local_vote_clk_disable,
	.reset = local_vote_clk_reset,
	.handoff = local_vote_clk_handoff,
};
