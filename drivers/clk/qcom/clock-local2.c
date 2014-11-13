/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <soc/qcom/clock-local2.h>
#include <soc/qcom/msm-clock-controller.h>

/*
 * When enabling/disabling a clock, check the halt bit up to this number
 * number of times (with a 1 us delay in between) before continuing.
 */
#define HALT_CHECK_MAX_LOOPS	500
/* For clock without halt checking, wait this long after enables/disables. */
#define HALT_CHECK_DELAY_US	500

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
#define RST_REG(x)	(*(x)->base + (x)->reset_reg)
#define VOTE_REG(x)	(*(x)->base + (x)->vote_reg)
#define GATE_EN_REG(x)	(*(x)->base + (x)->en_reg)

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
#define CBCR_CDIV_MSB			19

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

	CLK_WARN(&rcg->c, count == 0, "rcg didn't update its configuration.");
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
	u32 cfg_regval, div, div_regval;
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
		/* source select does not match */
		if ((freq->div_src_val & CFG_RCGR_SRC_SEL_MASK)
		    != (cfg_regval & CFG_RCGR_SRC_SEL_MASK))
			continue;
		/* divider does not match */
		div = freq->div_src_val & CFG_RCGR_DIV_MASK;
		div_regval = cfg_regval & CFG_RCGR_DIV_MASK;
		if (div != div_regval && (div > 1 || div_regval > 1))
			continue;

		if (has_mnd) {
			if (freq->m_val != m_regval)
				continue;
			if (freq->n_val != n_regval)
				continue;
			if (freq->d_val != d_regval)
				continue;
		} else if (freq->n_val) {
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

static void __iomem *rcg_hid_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct rcg_clk *rcg = to_rcg_clk(c);
	static struct clk_register_data data[] = {
		{"CMD_RCGR", 0x0},
		{"CFG_RCGR", 0x4},
	};
	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return CMD_RCGR_REG(rcg);
}

static void __iomem *rcg_mnd_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct rcg_clk *rcg = to_rcg_clk(c);
	static struct clk_register_data data[] = {
		{"CMD_RCGR", 0x0},
		{"CFG_RCGR", 0x4},
		{"M_VAL", 0x8},
		{"N_VAL", 0xC},
		{"D_VAL", 0x10},
	};
	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return CMD_RCGR_REG(rcg);
}

#define BRANCH_CHECK_MASK	BM(31, 28)
#define BRANCH_ON_VAL		BVAL(31, 28, 0x0)
#define BRANCH_OFF_VAL		BVAL(31, 28, 0x8)
#define BRANCH_NOC_FSM_ON_VAL	BVAL(31, 28, 0x2)

/*
 * Branch clock functions
 */
static void branch_clk_halt_check(struct clk *c, u32 halt_check,
			void __iomem *cbcr_reg, enum branch_state br_status)
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
		CLK_WARN(c, count == 0, "status stuck %s", status_str);
	}
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

static int branch_clk_enable(struct clk *c)
{
	unsigned long flags;
	u32 cbcr_val;
	struct branch_clk *branch = to_branch_clk(c);

	if (branch->toggle_memory) {
		branch_clk_set_flags(c, CLKFLAG_RETAIN_MEM);
		branch_clk_set_flags(c, CLKFLAG_RETAIN_PERIPH);
	}
	spin_lock_irqsave(&local_clock_reg_lock, flags);
	cbcr_val = readl_relaxed(CBCR_REG(branch));
	cbcr_val |= CBCR_BRANCH_ENABLE_BIT;
	writel_relaxed(cbcr_val, CBCR_REG(branch));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	/* Wait for clock to enable before continuing. */
	branch_clk_halt_check(c, branch->halt_check, CBCR_REG(branch),
				BRANCH_ON);

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
	branch_clk_halt_check(c, branch->halt_check, CBCR_REG(branch),
				BRANCH_OFF);

	if (branch->toggle_memory) {
		branch_clk_set_flags(c, CLKFLAG_NORETAIN_MEM);
		branch_clk_set_flags(c, CLKFLAG_NORETAIN_PERIPH);
	}
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
	int level;
	unsigned long fmax = 0, rate;
	struct branch_clk *branch = to_branch_clk(c);
	struct clk *parent = c->parent;

	if (branch->has_sibling == 1)
		return -ENXIO;

	if (!parent || !parent->ops->list_rate)
		return -ENXIO;

	/* Find max frequency supported within voltage constraints. */
	if (!parent->vdd_class) {
		fmax = ULONG_MAX;
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

	/* Set the cdiv to c->rate for fixed divider branch clock */
	if (c->rate && (c->rate < branch->max_div)) {
		cbcr_regval &= ~BM(CBCR_CDIV_MSB, CBCR_CDIV_LSB);
		cbcr_regval |= BVAL(CBCR_CDIV_MSB, CBCR_CDIV_LSB, c->rate);
		writel_relaxed(cbcr_regval, CBCR_REG(branch));
	}

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

static void __iomem *branch_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct branch_clk *branch = to_branch_clk(c);
	static struct clk_register_data data[] = {
		{"CBCR", 0x0},
	};
	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return CBCR_REG(branch);
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

	branch_clk_halt_check(c, vclk->halt_check, CBCR_REG(vclk), BRANCH_ON);

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

/* Sample clock for 'ticks' reference clock ticks. */
static u32 run_measurement(unsigned ticks, void __iomem *ctl_reg,
				void __iomem *status_reg)
{
	/* Stop counters and set the XO4 counter start value. */
	writel_relaxed(ticks, ctl_reg);

	/* Wait for timer to become ready. */
	while ((readl_relaxed(status_reg) & BIT(25)) != 0)
		cpu_relax();

	/* Run measurement and wait for completion. */
	writel_relaxed(BIT(20)|ticks, ctl_reg);
	while ((readl_relaxed(status_reg) & BIT(25)) == 0)
		cpu_relax();

	/* Return measured ticks. */
	return readl_relaxed(status_reg) & BM(24, 0);
}

/*
 * Perform a hardware rate measurement for a given clock.
 * FOR DEBUG USE ONLY: Measurements take ~15 ms!
 */
unsigned long measure_get_rate(struct clk *c)
{
	unsigned long flags;
	u32 gcc_xo4_reg;
	u64 raw_count_short, raw_count_full;
	unsigned ret;
	u32 sample_ticks = 0x10000;
	u32 multiplier = 0x1;
	struct measure_clk_data *data = to_mux_clk(c)->priv;

	ret = clk_prepare_enable(data->cxo);
	if (ret) {
		pr_warn("CXO clock failed to enable. Can't measure\n");
		return 0;
	}

	spin_lock_irqsave(&local_clock_reg_lock, flags);

	/* Enable CXO/4 and RINGOSC branch. */
	gcc_xo4_reg = readl_relaxed(*data->base + data->xo_div4_cbcr);
	gcc_xo4_reg |= CBCR_BRANCH_ENABLE_BIT;
	writel_relaxed(gcc_xo4_reg, *data->base + data->xo_div4_cbcr);

	/*
	 * The ring oscillator counter will not reset if the measured clock
	 * is not running.  To detect this, run a short measurement before
	 * the full measurement.  If the raw results of the two are the same
	 * then the clock must be off.
	 */

	/* Run a short measurement. (~1 ms) */
	raw_count_short = run_measurement(0x1000, *data->base + data->ctl_reg,
					  *data->base + data->status_reg);
	/* Run a full measurement. (~14 ms) */
	raw_count_full = run_measurement(sample_ticks,
					 *data->base + data->ctl_reg,
					 *data->base + data->status_reg);

	gcc_xo4_reg &= ~CBCR_BRANCH_ENABLE_BIT;
	writel_relaxed(gcc_xo4_reg, *data->base + data->xo_div4_cbcr);

	/* Return 0 if the clock is off. */
	if (raw_count_full == raw_count_short) {
		ret = 0;
	} else {
		/* Compute rate in Hz. */
		raw_count_full = ((raw_count_full * 10) + 15) * 4800000;
		do_div(raw_count_full, ((sample_ticks * 10) + 35));
		ret = (raw_count_full * multiplier);
	}
	writel_relaxed(data->plltest_val, *data->base + data->plltest_reg);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	clk_disable_unprepare(data->cxo);

	return ret;
}

struct frac_entry {
	int num;
	int den;
};

static void __iomem *local_vote_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct local_vote_clk *vclk = to_local_vote_clk(c);
	static struct clk_register_data data1[] = {
		{"CBCR", 0x0},
	};
	static struct clk_register_data data2[] = {
		{"APPS_VOTE", 0x0},
		{"APPS_SLEEP_VOTE", 0x4},
	};
	switch (n) {
	case 0:
		*regs = data1;
		*size = ARRAY_SIZE(data1);
		return CBCR_REG(vclk);
	case 1:
		*regs = data2;
		*size = ARRAY_SIZE(data2);
		return VOTE_REG(vclk);
	default:
		return ERR_PTR(-EINVAL);
	}
}

static struct frac_entry frac_table_675m[] = {	/* link rate of 270M */
	{52, 295},	/* 119 M */
	{11, 57},	/* 130.25 M */
	{63, 307},	/* 138.50 M */
	{11, 50},	/* 148.50 M */
	{47, 206},	/* 154 M */
	{31, 100},	/* 205.25 M */
	{107, 269},	/* 268.50 M */
	{0, 0},
};

static struct frac_entry frac_table_810m[] = { /* Link rate of 162M */
	{31, 211},	/* 119 M */
	{32, 199},	/* 130.25 M */
	{63, 307},	/* 138.50 M */
	{11, 60},	/* 148.50 M */
	{50, 263},	/* 154 M */
	{31, 120},	/* 205.25 M */
	{119, 359},	/* 268.50 M */
	{0, 0},
};

static int set_rate_edp_pixel(struct clk *clk, unsigned long rate)
{
	struct rcg_clk *rcg = to_rcg_clk(clk);
	struct clk_freq_tbl *pixel_freq = rcg->current_freq;
	struct frac_entry *frac;
	int delta = 100000;
	s64 request;
	s64 src_rate;

	src_rate = clk_get_rate(clk->parent);

	if (src_rate == 810000000)
		frac = frac_table_810m;
	else
		frac = frac_table_675m;

	while (frac->num) {
		request = rate;
		request *= frac->den;
		request = div_s64(request, frac->num);
		if ((src_rate < (request - delta)) ||
			(src_rate > (request + delta))) {
			frac++;
			continue;
		}

		pixel_freq->div_src_val &= ~BM(4, 0);
		if (frac->den == frac->num) {
			pixel_freq->m_val = 0;
			pixel_freq->n_val = 0;
		} else {
			pixel_freq->m_val = frac->num;
			pixel_freq->n_val = ~(frac->den - frac->num);
			pixel_freq->d_val = ~frac->den;
		}
		set_rate_mnd(rcg, pixel_freq);
		return 0;
	}
	return -EINVAL;
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

static long round_rate_pixel(struct clk *clk, unsigned long rate)
{
	int frac_num[] = {3, 2, 4, 1};
	int frac_den[] = {8, 9, 9, 1};
	int delta = 100000;
	int i;

	for (i = 0; i < ARRAY_SIZE(frac_num); i++) {
		unsigned long request = (rate * frac_den[i]) / frac_num[i];
		unsigned long src_rate;

		src_rate = clk_round_rate(clk->parent, request);
		if ((src_rate < (request - delta)) ||
			(src_rate > (request + delta)))
			continue;

		return (src_rate * frac_num[i]) / frac_den[i];
	}

	return -EINVAL;
}


static int set_rate_pixel(struct clk *clk, unsigned long rate)
{
	struct rcg_clk *rcg = to_rcg_clk(clk);
	struct clk_freq_tbl *pixel_freq = rcg->current_freq;
	int frac_num[] = {3, 2, 4, 1};
	int frac_den[] = {8, 9, 9, 1};
	int delta = 100000;
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(frac_num); i++) {
		unsigned long request = (rate * frac_den[i]) / frac_num[i];
		unsigned long src_rate;

		src_rate = clk_round_rate(clk->parent, request);
		if ((src_rate < (request - delta)) ||
			(src_rate > (request + delta)))
			continue;

		rc =  clk_set_rate(clk->parent, src_rate);
		if (rc)
			return rc;

		pixel_freq->div_src_val &= ~BM(4, 0);
		if (frac_den[i] == frac_num[i]) {
			pixel_freq->m_val = 0;
			pixel_freq->n_val = 0;
		} else {
			pixel_freq->m_val = frac_num[i];
			pixel_freq->n_val = ~(frac_den[i] - frac_num[i]);
			pixel_freq->d_val = ~frac_den[i];
		}
		set_rate_mnd(rcg, pixel_freq);
		return 0;
	}
	return -EINVAL;
}

/*
 * Unlike other clocks, the HDMI rate is adjusted through PLL
 * re-programming. It is also routed through an HID divider.
 */
static int rcg_clk_set_rate_hdmi(struct clk *c, unsigned long rate)
{
	struct rcg_clk *rcg = to_rcg_clk(c);
	struct clk_freq_tbl *nf = rcg->freq_tbl;
	int rc;

	rc = clk_set_rate(nf->src_clk, rate);
	if (rc < 0)
		goto out;
	set_rate_hid(rcg, nf);

	rcg->current_freq = nf;
out:
	return rc;
}

static struct clk *rcg_hdmi_clk_get_parent(struct clk *c)
{
	struct rcg_clk *rcg = to_rcg_clk(c);
	struct clk_freq_tbl *freq = rcg->freq_tbl;
	u32 cmd_rcgr_regval;

	/* Is there a pending configuration? */
	cmd_rcgr_regval = readl_relaxed(CMD_RCGR_REG(rcg));
	if (cmd_rcgr_regval & CMD_RCGR_CONFIG_DIRTY_MASK)
		return NULL;

	rcg->current_freq->freq_hz = clk_get_rate(c->parent);

	return freq->src_clk;
}

static int rcg_clk_set_rate_edp(struct clk *c, unsigned long rate)
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

static struct clk *edp_clk_get_parent(struct clk *c)
{
	struct rcg_clk *rcg = to_rcg_clk(c);
	struct clk *clk;
	struct clk_freq_tbl *freq;
	unsigned long rate;
	u32 cmd_rcgr_regval;

	/* Is there a pending configuration? */
	cmd_rcgr_regval = readl_relaxed(CMD_RCGR_REG(rcg));
	if (cmd_rcgr_regval & CMD_RCGR_CONFIG_DIRTY_MASK)
		return NULL;

	/* Figure out what rate the rcg is running at */
	for (freq = rcg->freq_tbl; freq->freq_hz != FREQ_END; freq++) {
		clk = freq->src_clk;
		if (clk && clk->ops->get_rate) {
			rate = clk->ops->get_rate(clk);
			if (rate == freq->freq_hz)
				break;
		}
	}

	/* No known frequency found */
	if (freq->freq_hz == FREQ_END)
		return NULL;

	rcg->current_freq = freq;
	return freq->src_clk;
}

static int gate_clk_enable(struct clk *c)
{
	unsigned long flags;
	u32 regval;
	struct gate_clk *g = to_gate_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	regval = readl_relaxed(GATE_EN_REG(g));
	regval |= g->en_mask;
	writel_relaxed(regval, GATE_EN_REG(g));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	if (g->delay_us)
		udelay(g->delay_us);

	return 0;
}

static void gate_clk_disable(struct clk *c)
{
	unsigned long flags;
	u32 regval;
	struct gate_clk *g = to_gate_clk(c);

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	regval = readl_relaxed(GATE_EN_REG(g));
	regval &= ~(g->en_mask);
	writel_relaxed(regval, GATE_EN_REG(g));
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	if (g->delay_us)
		udelay(g->delay_us);
}

static void __iomem *gate_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct gate_clk *g = to_gate_clk(c);
	static struct clk_register_data data[] = {
		{"EN_REG", 0x0},
	};
	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return GATE_EN_REG(g);
}

static enum handoff gate_clk_handoff(struct clk *c)
{
	struct gate_clk *g = to_gate_clk(c);
	u32 regval;

	regval = readl_relaxed(GATE_EN_REG(g));
	if (regval & g->en_mask)
		return HANDOFF_ENABLED_CLK;

	return HANDOFF_DISABLED_CLK;
}

static int reset_clk_rst(struct clk *c, enum clk_reset_action action)
{
	struct reset_clk *rst = to_reset_clk(c);

	if (!rst->reset_reg)
		return -EPERM;

	return __branch_clk_reset(RST_REG(rst), action);
}

static DEFINE_SPINLOCK(mux_reg_lock);

static int mux_reg_enable(struct mux_clk *clk)
{
	u32 regval;
	unsigned long flags;

	if (!clk->en_mask)
		return 0;

	spin_lock_irqsave(&mux_reg_lock, flags);
	regval = readl_relaxed(*clk->base + clk->en_offset);
	regval |= clk->en_mask;
	writel_relaxed(regval, *clk->base + clk->en_offset);
	/* Ensure enable request goes through before returning */
	mb();
	spin_unlock_irqrestore(&mux_reg_lock, flags);

	return 0;
}

static void mux_reg_disable(struct mux_clk *clk)
{
	u32 regval;
	unsigned long flags;

	if (!clk->en_mask)
		return;

	spin_lock_irqsave(&mux_reg_lock, flags);
	regval = readl_relaxed(*clk->base + clk->en_offset);
	regval &= ~clk->en_mask;
	writel_relaxed(regval, *clk->base + clk->en_offset);
	spin_unlock_irqrestore(&mux_reg_lock, flags);
}

static int mux_reg_set_mux_sel(struct mux_clk *clk, int sel)
{
	u32 regval;
	unsigned long flags;

	spin_lock_irqsave(&mux_reg_lock, flags);
	regval = readl_relaxed(*clk->base + clk->offset);
	regval &= ~(clk->mask << clk->shift);
	regval |= (sel & clk->mask) << clk->shift;
	writel_relaxed(regval, *clk->base + clk->offset);
	/* Ensure switch request goes through before returning */
	mb();
	spin_unlock_irqrestore(&mux_reg_lock, flags);

	return 0;
}

static int mux_reg_get_mux_sel(struct mux_clk *clk)
{
	u32 regval = readl_relaxed(*clk->base + clk->offset);
	return (regval >> clk->shift) & clk->mask;
}

static bool mux_reg_is_enabled(struct mux_clk *clk)
{
	u32 regval = readl_relaxed(*clk->base + clk->offset);
	return !!(regval & clk->en_mask);
}

static int div_reg_set_div(struct div_clk *clk, int div)
{
	u32 regval;
	unsigned long flags;

	/* Divider is not configurable */
	if (!clk->mask)
		return 0;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	regval = readl_relaxed(*clk->base + clk->offset);
	regval &= ~(clk->mask << clk->shift);
	regval |= (div & clk->mask) << clk->shift;
	/* Ensure switch request goes through before returning */
	mb();
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);

	return 0;
}

static int div_reg_get_div(struct div_clk *clk)
{
	u32 regval;
	/* Divider is not configurable */
	if (!clk->mask)
		return clk->data.div;

	regval = readl_relaxed(*clk->base + clk->offset);
	return (regval >> clk->shift) & clk->mask;
}

/* =================Half-integer RCG without MN counter================= */
#define RCGR_CMD_REG(x) ((x)->base + (x)->div_offset)
#define RCGR_DIV_REG(x) ((x)->base + (x)->div_offset + 4)
#define RCGR_SRC_REG(x) ((x)->base + (x)->div_offset + 4)

static int rcg_mux_div_update_config(struct mux_div_clk *md)
{
	u32 regval, count;

	regval = readl_relaxed(RCGR_CMD_REG(md));
	regval |= CMD_RCGR_CONFIG_UPDATE_BIT;
	writel_relaxed(regval, RCGR_CMD_REG(md));

	/* Wait for update to take effect */
	for (count = UPDATE_CHECK_MAX_LOOPS; count > 0; count--) {
		if (!(readl_relaxed(RCGR_CMD_REG(md)) &
			    CMD_RCGR_CONFIG_UPDATE_BIT))
			return 0;
		udelay(1);
	}

	CLK_WARN(&md->c, true, "didn't update its configuration.");

	return -EBUSY;
}

static void rcg_get_src_div(struct mux_div_clk *md, u32 *src_sel, u32 *div)
{
	u32 regval;
	unsigned long flags;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	/* Is there a pending configuration? */
	regval = readl_relaxed(RCGR_CMD_REG(md));
	if (regval & CMD_RCGR_CONFIG_DIRTY_MASK) {
		CLK_WARN(&md->c, true, "it's a pending configuration.");
		spin_unlock_irqrestore(&local_clock_reg_lock, flags);
		return;
	}

	regval = readl_relaxed(RCGR_DIV_REG(md));
	regval &= (md->div_mask << md->div_shift);
	*div = regval >> md->div_shift;

	/* bypass */
	if (*div == 0)
		*div = 1;
	/* the div is doubled here*/
	*div += 1;

	regval = readl_relaxed(RCGR_SRC_REG(md));
	regval &= (md->src_mask << md->src_shift);
	*src_sel = regval >> md->src_shift;
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
}

static int rcg_set_src_div(struct mux_div_clk *md, u32 src_sel, u32 div)
{
	u32 regval;
	unsigned long flags;
	int ret;

	/* for half-integer divider, div here is doubled */
	if (div)
		div -= 1;

	spin_lock_irqsave(&local_clock_reg_lock, flags);
	regval = readl_relaxed(RCGR_DIV_REG(md));
	regval &= ~(md->div_mask << md->div_shift);
	regval |= div << md->div_shift;
	writel_relaxed(regval, RCGR_DIV_REG(md));

	regval = readl_relaxed(RCGR_SRC_REG(md));
	regval &= ~(md->src_mask << md->src_shift);
	regval |= src_sel << md->src_shift;
	writel_relaxed(regval, RCGR_SRC_REG(md));

	ret = rcg_mux_div_update_config(md);
	spin_unlock_irqrestore(&local_clock_reg_lock, flags);
	return ret;
}

static int rcg_enable(struct mux_div_clk *md)
{
	return rcg_set_src_div(md, md->src_sel, md->data.div);
}

static void rcg_disable(struct mux_div_clk *md)
{
	u32 src_sel;

	if (!md->safe_freq)
		return;

	src_sel = parent_to_src_sel(md->parents, md->num_parents,
				md->safe_parent);

	rcg_set_src_div(md, src_sel, md->safe_div);
}

static bool rcg_is_enabled(struct mux_div_clk *md)
{
	u32 regval;

	regval = readl_relaxed(RCGR_CMD_REG(md));
	if (regval & CMD_RCGR_ROOT_STATUS_BIT)
		return false;
	else
		return true;
}

static void __iomem *rcg_list_registers(struct mux_div_clk *md, int n,
			struct clk_register_data **regs, u32 *size)
{
	static struct clk_register_data data[] = {
		{"CMD_RCGR", 0x0},
		{"CFG_RCGR", 0x4},
	};

	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return RCGR_CMD_REG(md);
}

struct clk_ops clk_ops_empty;

struct clk_ops clk_ops_rst = {
	.reset = reset_clk_rst,
};

struct clk_ops clk_ops_rcg = {
	.enable = rcg_clk_prepare,
	.set_rate = rcg_clk_set_rate,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = rcg_clk_handoff,
	.get_parent = rcg_clk_get_parent,
	.list_registers = rcg_hid_clk_list_registers,
};

struct clk_ops clk_ops_rcg_mnd = {
	.enable = rcg_clk_prepare,
	.set_rate = rcg_clk_set_rate,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = rcg_mnd_clk_handoff,
	.get_parent = rcg_mnd_clk_get_parent,
	.list_registers = rcg_mnd_clk_list_registers,
};

struct clk_ops clk_ops_pixel = {
	.enable = rcg_clk_prepare,
	.set_rate = set_rate_pixel,
	.list_rate = rcg_clk_list_rate,
	.round_rate = round_rate_pixel,
	.handoff = pixel_rcg_handoff,
	.list_registers = rcg_mnd_clk_list_registers,
};

struct clk_ops clk_ops_edppixel = {
	.enable = rcg_clk_prepare,
	.set_rate = set_rate_edp_pixel,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = pixel_rcg_handoff,
	.list_registers = rcg_mnd_clk_list_registers,
};

struct clk_ops clk_ops_byte = {
	.enable = rcg_clk_prepare,
	.set_rate = set_rate_byte,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = byte_rcg_handoff,
	.list_registers = rcg_hid_clk_list_registers,
};

struct clk_ops clk_ops_rcg_hdmi = {
	.enable = rcg_clk_prepare,
	.set_rate = rcg_clk_set_rate_hdmi,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = rcg_clk_handoff,
	.get_parent = rcg_hdmi_clk_get_parent,
	.list_registers = rcg_hid_clk_list_registers,
};

struct clk_ops clk_ops_rcg_edp = {
	.enable = rcg_clk_prepare,
	.set_rate = rcg_clk_set_rate_edp,
	.list_rate = rcg_clk_list_rate,
	.round_rate = rcg_clk_round_rate,
	.handoff = rcg_clk_handoff,
	.get_parent = edp_clk_get_parent,
	.list_registers = rcg_hid_clk_list_registers,
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
	.list_registers = branch_clk_list_registers,
};

struct clk_ops clk_ops_vote = {
	.enable = local_vote_clk_enable,
	.disable = local_vote_clk_disable,
	.reset = local_vote_clk_reset,
	.handoff = local_vote_clk_handoff,
	.list_registers = local_vote_clk_list_registers,
};

struct clk_ops clk_ops_gate = {
	.enable = gate_clk_enable,
	.disable = gate_clk_disable,
	.set_rate = parent_set_rate,
	.get_rate = parent_get_rate,
	.round_rate = parent_round_rate,
	.handoff = gate_clk_handoff,
	.list_registers = gate_clk_list_registers,
};

struct clk_mux_ops mux_reg_ops = {
	.enable = mux_reg_enable,
	.disable = mux_reg_disable,
	.set_mux_sel = mux_reg_set_mux_sel,
	.get_mux_sel = mux_reg_get_mux_sel,
	.is_enabled = mux_reg_is_enabled,
};

struct clk_div_ops div_reg_ops = {
	.set_div = div_reg_set_div,
	.get_div = div_reg_get_div,
};

struct mux_div_ops rcg_mux_div_ops = {
	.enable = rcg_enable,
	.disable = rcg_disable,
	.set_src_div = rcg_set_src_div,
	.get_src_div = rcg_get_src_div,
	.is_enabled = rcg_is_enabled,
	.list_registers = rcg_list_registers,
};

static void *cbc_dt_parser(struct device *dev, struct device_node *np)
{
	struct msmclk_data *drv;
	struct branch_clk *branch_clk;
	u32 rc;

	branch_clk = devm_kzalloc(dev, sizeof(*branch_clk), GFP_KERNEL);
	if (!branch_clk) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return ERR_CAST(drv);
	branch_clk->base = &drv->base;

	rc = of_property_read_u32(np, "qcom,base-offset",
						&branch_clk->cbcr_reg);
	if (rc) {
		dt_err(np, "missing/incorrect qcom,base-offset dt property\n");
		return ERR_PTR(rc);
	}

	/* Optional property */
	of_property_read_u32(np, "qcom,bcr-offset", &branch_clk->bcr_reg);

	branch_clk->has_sibling = of_property_read_bool(np,
							"qcom,has-sibling");

	branch_clk->c.ops = &clk_ops_branch;

	return msmclk_generic_clk_init(dev, np, &branch_clk->c);
}
MSMCLK_PARSER(cbc_dt_parser, "qcom,cbc", 0);

static void *local_vote_clk_dt_parser(struct device *dev,
						struct device_node *np)
{
	struct local_vote_clk *vote_clk;
	struct msmclk_data *drv;
	int rc, val;

	vote_clk = devm_kzalloc(dev, sizeof(*vote_clk), GFP_KERNEL);
	if (!vote_clk) {
		dt_err(np, "failed to alloc memory\n");
		return ERR_PTR(-ENOMEM);
	}

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return ERR_CAST(drv);
	vote_clk->base = &drv->base;

	rc = of_property_read_u32(np, "qcom,base-offset",
						&vote_clk->cbcr_reg);
	if (rc) {
		dt_err(np, "missing/incorrect qcom,base-offset dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,en-offset", &vote_clk->vote_reg);
	if (rc) {
		dt_err(np, "missing/incorrect qcom,en-offset dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,en-bit", &val);
	if (rc) {
		dt_err(np, "missing/incorrect qcom,en-bit dt property\n");
		return ERR_PTR(-EINVAL);
	}
	vote_clk->en_mask = BIT(val);

	vote_clk->c.ops = &clk_ops_vote;

	/* Optional property */
	of_property_read_u32(np, "qcom,bcr-offset", &vote_clk->bcr_reg);

	return msmclk_generic_clk_init(dev, np, &vote_clk->c);
}
MSMCLK_PARSER(local_vote_clk_dt_parser, "qcom,local-vote-clk", 0);

static void *gate_clk_dt_parser(struct device *dev, struct device_node *np)
{
	struct gate_clk *gate_clk;
	struct msmclk_data *drv;
	u32 en_bit, rc;

	gate_clk = devm_kzalloc(dev, sizeof(*gate_clk), GFP_KERNEL);
	if (!gate_clk) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return ERR_CAST(drv);
	gate_clk->base = &drv->base;

	rc = of_property_read_u32(np, "qcom,en-offset", &gate_clk->en_reg);
	if (rc) {
		dt_err(np, "missing qcom,en-offset dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,en-bit", &en_bit);
	if (rc) {
		dt_err(np, "missing qcom,en-bit dt property\n");
		return ERR_PTR(-EINVAL);
	}
	gate_clk->en_mask = BIT(en_bit);

	/* Optional Property */
	rc = of_property_read_u32(np, "qcom,delay", &gate_clk->delay_us);
	if (rc)
		gate_clk->delay_us = 0;

	gate_clk->c.ops = &clk_ops_gate;
	return msmclk_generic_clk_init(dev, np, &gate_clk->c);
}
MSMCLK_PARSER(gate_clk_dt_parser, "qcom,gate-clk", 0);


static inline u32 rcg_calc_m(u32 m, u32 n)
{
	return m;
}

static inline u32 rcg_calc_n(u32 m, u32 n)
{
	n = n > 1 ? n : 0;
	return ~((n)-(m)) * !!(n);
}

static inline u32 rcg_calc_duty_cycle(u32 m, u32 n)
{
	return ~n;
}

static inline u32 rcg_calc_div_src(u32 div_int, u32 div_frac, u32 src_sel)
{
	int div = 2 * div_int + (div_frac ? 1 : 0) - 1;
	/* set bypass mode instead of a divider of 1 */
	div = (div != 1) ? div : 0;
	return BVAL(4, 0, max(div, 0))
			| BVAL(10, 8, src_sel);
}

struct clk_src *msmclk_parse_clk_src(struct device *dev,
				struct device_node *np, int *array_size)
{
	struct clk_src *clks;
	const void *prop;
	int num_parents, len, i, prop_len, rc;
	char *name = "qcom,parents";

	if (!array_size) {
		dt_err(np, "array_size must be a valid pointer\n");
		return ERR_PTR(-EINVAL);
	}

	prop = of_get_property(np, name, &prop_len);
	if (!prop) {
		dt_prop_err(np, name, "missing dt property\n");
		return ERR_PTR(-EINVAL);
	}

	len = sizeof(phandle) + sizeof(u32);
	if (prop_len % len) {
		dt_prop_err(np, name, "invalid property length\n");
		return ERR_PTR(-EINVAL);
	}
	num_parents = prop_len / len;

	clks = devm_kzalloc(dev, sizeof(*clks) * num_parents, GFP_KERNEL);
	if (!clks) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	/* Assume that u32 and phandle have the same size */
	for (i = 0; i < num_parents; i++) {
		phandle p;
		struct clk_src *a = &clks[i];

		rc = of_property_read_u32_index(np, name, 2 * i, &a->sel);
		rc |= of_property_read_phandle_index(np, name, 2 * i + 1, &p);

		if (rc) {
			dt_prop_err(np, name,
				"unable to read parent clock or mux index\n");
			return ERR_PTR(-EINVAL);
		}

		a->src = msmclk_parse_phandle(dev, p);
		if (IS_ERR(a->src)) {
			dt_prop_err(np, name, "hashtable lookup failed\n");
			return ERR_CAST(a->src);
		}
	}

	*array_size = num_parents;

	return clks;
}

static int rcg_parse_freq_tbl(struct device *dev,
			struct device_node *np, struct rcg_clk *rcg)
{
	const void *prop;
	u32 prop_len, num_rows, i, j = 0;
	struct clk_freq_tbl *tbl;
	int rc;
	char *name = "qcom,freq-tbl";

	prop = of_get_property(np, name, &prop_len);
	if (!prop) {
		dt_prop_err(np, name, "missing dt property\n");
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % 6) {
		dt_prop_err(np, name, "bad length\n");
		return -EINVAL;
	}

	num_rows = prop_len / 6;
	/* Array is null terminated. */
	rcg->freq_tbl = devm_kzalloc(dev,
				sizeof(*rcg->freq_tbl) * (num_rows + 1),
				GFP_KERNEL);

	if (!rcg->freq_tbl) {
		dt_err(np, "memory alloc failure\n");
		return -ENOMEM;
	}

	tbl = rcg->freq_tbl;
	for (i = 0; i < num_rows; i++, tbl++) {
		phandle p;
		u32 div_int, div_frac, m, n, src_sel, freq_hz;

		rc = of_property_read_u32_index(np, name, j++, &freq_hz);
		rc |= of_property_read_u32_index(np, name, j++, &div_int);
		rc |= of_property_read_u32_index(np, name, j++, &div_frac);
		rc |= of_property_read_u32_index(np, name, j++, &m);
		rc |= of_property_read_u32_index(np, name, j++, &n);
		rc |= of_property_read_u32_index(np, name, j++, &p);

		if (rc) {
			dt_prop_err(np, name, "unable to read u32\n");
			return -EINVAL;
		}

		tbl->freq_hz = (unsigned long)freq_hz;
		tbl->src_clk = msmclk_parse_phandle(dev, p);
		if (IS_ERR_OR_NULL(tbl->src_clk)) {
			dt_prop_err(np, name, "hashtable lookup failure\n");
			return PTR_ERR(tbl->src_clk);
		}

		tbl->m_val = rcg_calc_m(m, n);
		tbl->n_val = rcg_calc_n(m, n);
		tbl->d_val = rcg_calc_duty_cycle(m, n);

		src_sel = parent_to_src_sel(rcg->c.parents,
					rcg->c.num_parents, tbl->src_clk);
		tbl->div_src_val = rcg_calc_div_src(div_int, div_frac,
								src_sel);
	}
	/* End table with special value */
	tbl->freq_hz = FREQ_END;
	return 0;
}

static void *rcg_clk_dt_parser(struct device *dev, struct device_node *np)
{
	struct rcg_clk *rcg;
	struct msmclk_data *drv;
	int rc;

	rcg = devm_kzalloc(dev, sizeof(*rcg), GFP_KERNEL);
	if (!rcg) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return drv;
	rcg->base = &drv->base;

	rcg->c.parents = msmclk_parse_clk_src(dev, np, &rcg->c.num_parents);
	if (IS_ERR(rcg->c.parents)) {
		dt_err(np, "unable to read parents\n");
		return ERR_CAST(rcg->c.parents);
	}

	rc = of_property_read_u32(np, "qcom,base-offset", &rcg->cmd_rcgr_reg);
	if (rc) {
		dt_err(np, "missing qcom,base-offset dt property\n");
		return ERR_PTR(rc);
	}

	rc = rcg_parse_freq_tbl(dev, np, rcg);
	if (rc) {
		dt_err(np, "unable to read freq_tbl\n");
		return ERR_PTR(rc);
	}
	rcg->current_freq = &rcg_dummy_freq;

	if (of_device_is_compatible(np, "qcom,rcg-hid")) {
		rcg->c.ops = &clk_ops_rcg;
		rcg->set_rate = set_rate_hid;
	} else if (of_device_is_compatible(np, "qcom,rcg-mn")) {
		rcg->c.ops = &clk_ops_rcg_mnd;
		rcg->set_rate = set_rate_mnd;
	} else {
		dt_err(np, "unexpected compatible string\n");
		return ERR_PTR(-EINVAL);
	}

	return msmclk_generic_clk_init(dev, np, &rcg->c);
}
MSMCLK_PARSER(rcg_clk_dt_parser, "qcom,rcg-hid", 0);
MSMCLK_PARSER(rcg_clk_dt_parser, "qcom,rcg-mn", 1);

static int parse_rec_parents(struct device *dev,
			struct device_node *np, struct mux_clk *mux)
{
	int i, rc;
	char *name = "qcom,recursive-parents";
	phandle p;

	mux->num_rec_parents = of_property_count_phandles(np, name);
	if (mux->num_rec_parents <= 0)
		return 0;

	mux->rec_parents = devm_kzalloc(dev,
			sizeof(*mux->rec_parents) * mux->num_rec_parents,
			GFP_KERNEL);

	if (!mux->rec_parents) {
		dt_err(np, "memory alloc failure\n");
		return -ENOMEM;
	}

	for (i = 0; i < mux->num_rec_parents; i++) {
		rc = of_property_read_phandle_index(np, name, i, &p);
		if (rc) {
			dt_prop_err(np, name, "unable to read u32\n");
			return rc;
		}

		mux->rec_parents[i] = msmclk_parse_phandle(dev, p);
		if (IS_ERR(mux->rec_parents[i])) {
			dt_prop_err(np, name, "hashtable lookup failure\n");
			return PTR_ERR(mux->rec_parents[i]);
		}
	}

	return 0;
}

static void *mux_reg_clk_dt_parser(struct device *dev, struct device_node *np)
{
	struct mux_clk *mux;
	struct msmclk_data *drv;
	int rc;

	mux = devm_kzalloc(dev, sizeof(*mux), GFP_KERNEL);
	if (!mux) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	mux->parents = msmclk_parse_clk_src(dev, np, &mux->num_parents);
	if (IS_ERR(mux->parents))
		return mux->parents;

	mux->c.parents = mux->parents;
	mux->c.num_parents = mux->num_parents;

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return drv;
	mux->base = &drv->base;

	rc = parse_rec_parents(dev, np, mux);
	if (rc) {
		dt_err(np, "Incorrect qcom,recursive-parents dt property\n");
		return ERR_PTR(rc);
	}

	rc = of_property_read_u32(np, "qcom,offset", &mux->offset);
	if (rc) {
		dt_err(np, "missing qcom,offset dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,mask", &mux->mask);
	if (rc) {
		dt_err(np, "missing qcom,mask dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,shift", &mux->shift);
	if (rc) {
		dt_err(np, "missing qcom,shift dt property\n");
		return ERR_PTR(-EINVAL);
	}

	mux->c.ops = &clk_ops_gen_mux;
	mux->ops = &mux_reg_ops;

	/* Optional Properties */
	of_property_read_u32(np, "qcom,en-offset", &mux->en_offset);
	of_property_read_u32(np, "qcom,en-mask", &mux->en_mask);

	return msmclk_generic_clk_init(dev, np, &mux->c);
};
MSMCLK_PARSER(mux_reg_clk_dt_parser, "qcom,mux-reg", 0);

static void *measure_clk_dt_parser(struct device *dev,
					struct device_node *np)
{
	struct mux_clk *mux;
	struct clk *c;
	struct measure_clk_data *p;
	struct clk_ops *clk_ops_measure_mux;
	phandle cxo;
	int rc;

	c = mux_reg_clk_dt_parser(dev, np);
	if (IS_ERR(c))
		return c;

	mux = to_mux_clk(c);

	p = devm_kzalloc(dev, sizeof(*p), GFP_KERNEL);
	if (!p) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = of_property_read_phandle_index(np, "qcom,cxo", 0, &cxo);
	if (rc) {
		dt_err(np, "missing qcom,cxo\n");
		return ERR_PTR(-EINVAL);
	}
	p->cxo = msmclk_parse_phandle(dev, cxo);
	if (IS_ERR_OR_NULL(p->cxo)) {
		dt_prop_err(np, "qcom,cxo", "hashtable lookup failure\n");
		return p->cxo;
	}

	rc = of_property_read_u32(np, "qcom,xo-div4-cbcr", &p->xo_div4_cbcr);
	if (rc) {
		dt_err(np, "missing qcom,xo-div4-cbcr dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,test-pad-config", &p->plltest_val);
	if (rc) {
		dt_err(np, "missing qcom,test-pad-config dt property\n");
		return ERR_PTR(-EINVAL);
	}

	p->base = mux->base;
	p->ctl_reg = mux->offset + 0x4;
	p->status_reg = mux->offset + 0x8;
	p->plltest_reg = mux->offset + 0xC;
	mux->priv = p;

	clk_ops_measure_mux = devm_kzalloc(dev, sizeof(*clk_ops_measure_mux),
								GFP_KERNEL);
	if (!clk_ops_measure_mux) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	*clk_ops_measure_mux = clk_ops_gen_mux;
	clk_ops_measure_mux->get_rate = measure_get_rate;

	mux->c.ops = clk_ops_measure_mux;

	/* Already did generic clk init */
	return &mux->c;
};
MSMCLK_PARSER(measure_clk_dt_parser, "qcom,measure-mux", 0);

static void *div_clk_dt_parser(struct device *dev,
					struct device_node *np)
{
	struct div_clk *div_clk;
	struct msmclk_data *drv;
	int rc;

	div_clk = devm_kzalloc(dev, sizeof(*div_clk), GFP_KERNEL);
	if (!div_clk) {
		dt_err(np, "memory alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = of_property_read_u32(np, "qcom,max-div", &div_clk->data.max_div);
	if (rc) {
		dt_err(np, "missing qcom,max-div\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,min-div", &div_clk->data.min_div);
	if (rc) {
		dt_err(np, "missing qcom,min-div\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,base-offset", &div_clk->offset);
	if (rc) {
		dt_err(np, "missing qcom,base-offset\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,mask", &div_clk->mask);
	if (rc) {
		dt_err(np, "missing qcom,mask\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,shift", &div_clk->shift);
	if (rc) {
		dt_err(np, "missing qcom,shift\n");
		return ERR_PTR(-EINVAL);
	}

	if (of_property_read_bool(np, "qcom,slave-div"))
		div_clk->c.ops = &clk_ops_slave_div;
	else
		div_clk->c.ops = &clk_ops_div;
	div_clk->ops = &div_reg_ops;

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return ERR_CAST(drv);
	div_clk->base = &drv->base;

	return msmclk_generic_clk_init(dev, np, &div_clk->c);
};
MSMCLK_PARSER(div_clk_dt_parser, "qcom,div-clk", 0);

static void *fixed_div_clk_dt_parser(struct device *dev,
						struct device_node *np)
{
	struct div_clk *div_clk;
	int rc;

	div_clk = devm_kzalloc(dev, sizeof(*div_clk), GFP_KERNEL);
	if (!div_clk) {
		dt_err(np, "memory alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = of_property_read_u32(np, "qcom,div", &div_clk->data.div);
	if (rc) {
		dt_err(np, "missing qcom,div\n");
		return ERR_PTR(-EINVAL);
	}
	div_clk->data.min_div = div_clk->data.div;
	div_clk->data.max_div = div_clk->data.div;

	if (of_property_read_bool(np, "qcom,slave-div"))
		div_clk->c.ops = &clk_ops_slave_div;
	else
		div_clk->c.ops = &clk_ops_div;
	div_clk->ops = &div_reg_ops;

	return msmclk_generic_clk_init(dev, np, &div_clk->c);
}
MSMCLK_PARSER(fixed_div_clk_dt_parser, "qcom,fixed-div-clk", 0);

static void *reset_clk_dt_parser(struct device *dev,
					struct device_node *np)
{
	struct reset_clk *reset_clk;
	struct msmclk_data *drv;
	int rc;

	reset_clk = devm_kzalloc(dev, sizeof(*reset_clk), GFP_KERNEL);
	if (!reset_clk) {
		dt_err(np, "memory alloc failed\n");
		return ERR_PTR(-ENOMEM);
	}

	rc = of_property_read_u32(np, "qcom,base-offset",
						&reset_clk->reset_reg);
	if (rc) {
		dt_err(np, "missing qcom,base-offset\n");
		return ERR_PTR(-EINVAL);
	}

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return ERR_CAST(drv);
	reset_clk->base = &drv->base;

	reset_clk->c.ops = &clk_ops_rst;
	return msmclk_generic_clk_init(dev, np, &reset_clk->c);
};
MSMCLK_PARSER(reset_clk_dt_parser, "qcom,reset-clk", 0);
