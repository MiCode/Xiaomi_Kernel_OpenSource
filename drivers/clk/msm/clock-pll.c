/*
 * Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/sched.h>
#include <soc/qcom/clock-pll.h>
#include <soc/qcom/msm-clock-controller.h>

#include "clock.h"

#define PLL_OUTCTRL BIT(0)
#define PLL_BYPASSNL BIT(1)
#define PLL_RESET_N BIT(2)
#define PLL_MODE_MASK BM(3, 0)

#define PLL_EN_REG(x)		(*(x)->base + (unsigned long) (x)->en_reg)
#define PLL_STATUS_REG(x)	(*(x)->base + (unsigned long) (x)->status_reg)
#define PLL_ALT_STATUS_REG(x)	(*(x)->base + (unsigned long) \
							(x)->alt_status_reg)
#define PLL_MODE_REG(x)		(*(x)->base + (unsigned long) (x)->mode_reg)
#define PLL_L_REG(x)		(*(x)->base + (unsigned long) (x)->l_reg)
#define PLL_M_REG(x)		(*(x)->base + (unsigned long) (x)->m_reg)
#define PLL_N_REG(x)		(*(x)->base + (unsigned long) (x)->n_reg)
#define PLL_CONFIG_REG(x)	(*(x)->base + (unsigned long) (x)->config_reg)
#define PLL_ALPHA_REG(x)	(*(x)->base + (unsigned long) (x)->alpha_reg)
#define PLL_CFG_ALT_REG(x)	(*(x)->base + (unsigned long) \
							(x)->config_alt_reg)
#define PLL_CFG_CTL_REG(x)	(*(x)->base + (unsigned long) \
							(x)->config_ctl_reg)
#define PLL_CFG_CTL_HI_REG(x)	(*(x)->base + (unsigned long) \
							(x)->config_ctl_hi_reg)
#define PLL_TEST_CTL_LO_REG(x)	(*(x)->base + (unsigned long) \
							(x)->test_ctl_lo_reg)
#define PLL_TEST_CTL_HI_REG(x)	(*(x)->base + (unsigned long) \
							(x)->test_ctl_hi_reg)
static DEFINE_SPINLOCK(pll_reg_lock);

#define ENABLE_WAIT_MAX_LOOPS 200
#define PLL_LOCKED_BIT BIT(16)

#define SPM_FORCE_EVENT   0x4

static int pll_vote_clk_enable(struct clk *c)
{
	u32 ena, count;
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&pll_reg_lock, flags);
	ena = readl_relaxed(PLL_EN_REG(pllv));
	ena |= pllv->en_mask;
	writel_relaxed(ena, PLL_EN_REG(pllv));
	spin_unlock_irqrestore(&pll_reg_lock, flags);

	/*
	 * Use a memory barrier since some PLL status registers are
	 * not within the same 1K segment as the voting registers.
	 */
	mb();

	/* Wait for pll to enable. */
	for (count = ENABLE_WAIT_MAX_LOOPS; count > 0; count--) {
		if (readl_relaxed(PLL_STATUS_REG(pllv)) & pllv->status_mask)
			return 0;
		udelay(1);
	}

	WARN("PLL %s didn't enable after voting for it!\n", c->dbg_name);

	return -ETIMEDOUT;
}

static void pll_vote_clk_disable(struct clk *c)
{
	u32 ena;
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&pll_reg_lock, flags);
	ena = readl_relaxed(PLL_EN_REG(pllv));
	ena &= ~(pllv->en_mask);
	writel_relaxed(ena, PLL_EN_REG(pllv));
	spin_unlock_irqrestore(&pll_reg_lock, flags);
}

static int pll_vote_clk_is_enabled(struct clk *c)
{
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);
	return !!(readl_relaxed(PLL_STATUS_REG(pllv)) & pllv->status_mask);
}

static enum handoff pll_vote_clk_handoff(struct clk *c)
{
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);
	if (readl_relaxed(PLL_EN_REG(pllv)) & pllv->en_mask)
		return HANDOFF_ENABLED_CLK;

	return HANDOFF_DISABLED_CLK;
}

static void __iomem *pll_vote_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);
	static struct clk_register_data data1[] = {
		{"APPS_VOTE", 0x0},
	};

	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data1;
	*size = ARRAY_SIZE(data1);
	return PLL_EN_REG(pllv);
}

struct clk_ops clk_ops_pll_vote = {
	.enable = pll_vote_clk_enable,
	.disable = pll_vote_clk_disable,
	.is_enabled = pll_vote_clk_is_enabled,
	.handoff = pll_vote_clk_handoff,
	.list_registers = pll_vote_clk_list_registers,
};

/*
 *  spm_event() -- Set/Clear SPM events
 *  PLL off sequence -- enable (1)
 *    Set L2_SPM_FORCE_EVENT_EN[bit] register to 1
 *    Set L2_SPM_FORCE_EVENT[bit] register to 1
 *  PLL on sequence -- enable (0)
 *   Clear L2_SPM_FORCE_EVENT[bit] register to 0
 *   Clear L2_SPM_FORCE_EVENT_EN[bit] register to 0
 */
static void spm_event(void __iomem *base, u32 offset, u32 bit,
							bool enable)
{
	uint32_t val;

	if (!base)
		return;

	if (enable) {
		/* L2_SPM_FORCE_EVENT_EN */
		val = readl_relaxed(base + offset);
		val |= BIT(bit);
		writel_relaxed(val, (base + offset));
		/* Ensure that the write above goes through. */
		mb();

		/* L2_SPM_FORCE_EVENT */
		val = readl_relaxed(base + offset + SPM_FORCE_EVENT);
		val |= BIT(bit);
		writel_relaxed(val, (base + offset + SPM_FORCE_EVENT));
		/* Ensure that the write above goes through. */
		mb();
	} else {
		/* L2_SPM_FORCE_EVENT */
		val = readl_relaxed(base + offset + SPM_FORCE_EVENT);
		val &= ~BIT(bit);
		writel_relaxed(val, (base + offset + SPM_FORCE_EVENT));
		/* Ensure that the write above goes through. */
		mb();

		/* L2_SPM_FORCE_EVENT_EN */
		val = readl_relaxed(base + offset);
		val &= ~BIT(bit);
		writel_relaxed(val, (base + offset));
		/* Ensure that the write above goes through. */
		mb();
	}
}

static void __pll_config_reg(void __iomem *pll_config, struct pll_freq_tbl *f,
			struct pll_config_masks *masks)
{
	u32 regval;

	regval = readl_relaxed(pll_config);

	/* Enable the MN counter if used */
	if (f->m_val)
		regval |= masks->mn_en_mask;

	/* Set pre-divider and post-divider values */
	regval &= ~masks->pre_div_mask;
	regval |= f->pre_div_val;
	regval &= ~masks->post_div_mask;
	regval |= f->post_div_val;

	/* Select VCO setting */
	regval &= ~masks->vco_mask;
	regval |= f->vco_val;

	/* Enable main output if it has not been enabled */
	if (masks->main_output_mask && !(regval & masks->main_output_mask))
		regval |= masks->main_output_mask;

	writel_relaxed(regval, pll_config);
}

static int sr2_pll_clk_enable(struct clk *c)
{
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(c);
	int ret = 0, count;
	u32 mode = readl_relaxed(PLL_MODE_REG(pll));
	u32 lockmask = pll->masks.lock_mask ?: PLL_LOCKED_BIT;

	spin_lock_irqsave(&pll_reg_lock, flags);

	spm_event(pll->spm_ctrl.spm_base, pll->spm_ctrl.offset,
				pll->spm_ctrl.event_bit, false);

	/* Disable PLL bypass mode. */
	mode |= PLL_BYPASSNL;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	mb();
	udelay(10);

	/* De-assert active-low PLL reset. */
	mode |= PLL_RESET_N;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/* Wait for pll to lock. */
	for (count = ENABLE_WAIT_MAX_LOOPS; count > 0; count--) {
		if (readl_relaxed(PLL_STATUS_REG(pll)) & lockmask)
			break;
		udelay(1);
	}

	if (!(readl_relaxed(PLL_STATUS_REG(pll)) & lockmask))
		pr_err("PLL %s didn't lock after enabling it!\n", c->dbg_name);

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/* Ensure that the write above goes through before returning. */
	mb();

	spin_unlock_irqrestore(&pll_reg_lock, flags);
	return ret;
}

void __variable_rate_pll_init(struct clk *c)
{
	struct pll_clk *pll = to_pll_clk(c);
	u32 regval;

	regval = readl_relaxed(PLL_CONFIG_REG(pll));

	if (pll->masks.post_div_mask) {
		regval &= ~pll->masks.post_div_mask;
		regval |= pll->vals.post_div_masked;
	}

	if (pll->masks.pre_div_mask) {
		regval &= ~pll->masks.pre_div_mask;
		regval |= pll->vals.pre_div_masked;
	}

	if (pll->masks.main_output_mask)
		regval |= pll->masks.main_output_mask;

	if (pll->masks.early_output_mask)
		regval |= pll->masks.early_output_mask;

	if (pll->vals.enable_mn)
		regval |= pll->masks.mn_en_mask;
	else
		regval &= ~pll->masks.mn_en_mask;

	writel_relaxed(regval, PLL_CONFIG_REG(pll));

	regval = readl_relaxed(PLL_MODE_REG(pll));
	if (pll->masks.apc_pdn_mask)
		regval &= ~pll->masks.apc_pdn_mask;
	writel_relaxed(regval, PLL_MODE_REG(pll));

	writel_relaxed(pll->vals.alpha_val, PLL_ALPHA_REG(pll));
	writel_relaxed(pll->vals.config_ctl_val, PLL_CFG_CTL_REG(pll));
	if (pll->vals.config_ctl_hi_val)
		writel_relaxed(pll->vals.config_ctl_hi_val,
				PLL_CFG_CTL_HI_REG(pll));
	if (pll->init_test_ctl) {
		writel_relaxed(pll->vals.test_ctl_lo_val,
				PLL_TEST_CTL_LO_REG(pll));
		writel_relaxed(pll->vals.test_ctl_hi_val,
				PLL_TEST_CTL_HI_REG(pll));
	}

	pll->inited = true;
}

static int variable_rate_pll_clk_enable(struct clk *c)
{
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(c);
	int ret = 0, count;
	u32 mode, testlo;
	u32 lockmask = pll->masks.lock_mask ?: PLL_LOCKED_BIT;
	u32 mode_lock;
	u64 time;
	bool early_lock = false;

	spin_lock_irqsave(&pll_reg_lock, flags);

	if (unlikely(!to_pll_clk(c)->inited))
		__variable_rate_pll_init(c);

	mode = readl_relaxed(PLL_MODE_REG(pll));

	/* Set test control bits as required by HW doc */
	if (pll->test_ctl_lo_reg && pll->vals.test_ctl_lo_val &&
		pll->pgm_test_ctl_enable)
		writel_relaxed(pll->vals.test_ctl_lo_val,
				PLL_TEST_CTL_LO_REG(pll));

	if (!pll->test_ctl_dbg) {
		/* Enable test_ctl debug */
		mode |= BIT(3);
		writel_relaxed(mode, PLL_MODE_REG(pll));

		testlo = readl_relaxed(PLL_TEST_CTL_LO_REG(pll));
		testlo &= ~BM(7, 6);
		testlo |= 0xC0;
		writel_relaxed(testlo, PLL_TEST_CTL_LO_REG(pll));
		/* Wait for the write to complete */
		mb();
	}

	/* Disable PLL bypass mode. */
	mode |= PLL_BYPASSNL;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Use 10us to be sure.
	 */
	mb();
	udelay(10);

	/* De-assert active-low PLL reset. */
	mode |= PLL_RESET_N;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/*
	 * 5us delay mandated by HPG. However, put in a 200us delay here.
	 * This is to address possible locking issues with the PLL exhibit
	 * early "transient" locks about 16us from this point. With this
	 * higher delay, we avoid running into those transients.
	 */
	mb();
	udelay(200);

	/* Clear test control bits */
	if (pll->test_ctl_lo_reg && pll->vals.test_ctl_lo_val &&
		pll->pgm_test_ctl_enable)
		writel_relaxed(0x0, PLL_TEST_CTL_LO_REG(pll));


	time = sched_clock();
	/* Wait for pll to lock. */
	for (count = ENABLE_WAIT_MAX_LOOPS; count > 0; count--) {
		if (readl_relaxed(PLL_STATUS_REG(pll)) & lockmask) {
			udelay(1);
			/*
			 * Check again to be sure. This is to avoid
			 * breaking too early if there is a "transient"
			 * lock.
			 */
			if ((readl_relaxed(PLL_STATUS_REG(pll)) & lockmask))
				break;
			else
				early_lock = true;
		}
		udelay(1);
	}
	time = sched_clock() - time;

	mode_lock = readl_relaxed(PLL_STATUS_REG(pll));

	if (!(mode_lock & lockmask)) {
		pr_err("PLL lock bit detection total wait time: %lld ns", time);
		pr_err("PLL %s didn't lock after enabling for L value 0x%x!\n",
			c->dbg_name, readl_relaxed(PLL_L_REG(pll)));
		pr_err("mode register is 0x%x\n",
			readl_relaxed(PLL_STATUS_REG(pll)));
		pr_err("user control register is 0x%x\n",
			readl_relaxed(PLL_CONFIG_REG(pll)));
		pr_err("config control register is 0x%x\n",
			readl_relaxed(PLL_CFG_CTL_REG(pll)));
		pr_err("test control high register is 0x%x\n",
			readl_relaxed(PLL_TEST_CTL_HI_REG(pll)));
		pr_err("test control low register is 0x%x\n",
			readl_relaxed(PLL_TEST_CTL_LO_REG(pll)));
		pr_err("early lock? %s\n", early_lock ? "yes" : "no");

		testlo = readl_relaxed(PLL_TEST_CTL_LO_REG(pll));
		testlo &= ~BM(7, 6);
		writel_relaxed(testlo, PLL_TEST_CTL_LO_REG(pll));
		/* Wait for the write to complete */
		mb();

		pr_err("test_ctl_lo = 0x%x, pll status is: 0x%x\n",
			readl_relaxed(PLL_TEST_CTL_LO_REG(pll)),
			readl_relaxed(PLL_ALT_STATUS_REG(pll)));

		testlo = readl_relaxed(PLL_TEST_CTL_LO_REG(pll));
		testlo &= ~BM(7, 6);
		testlo |= 0x40;
		writel_relaxed(testlo, PLL_TEST_CTL_LO_REG(pll));
		/* Wait for the write to complete */
		mb();
		pr_err("test_ctl_lo = 0x%x, pll status is: 0x%x\n",
			readl_relaxed(PLL_TEST_CTL_LO_REG(pll)),
			readl_relaxed(PLL_ALT_STATUS_REG(pll)));

		testlo = readl_relaxed(PLL_TEST_CTL_LO_REG(pll));
		testlo &= ~BM(7, 6);
		testlo |= 0x80;
		writel_relaxed(testlo, PLL_TEST_CTL_LO_REG(pll));
		/* Wait for the write to complete */
		mb();

		pr_err("test_ctl_lo = 0x%x, pll status is: 0x%x\n",
			readl_relaxed(PLL_TEST_CTL_LO_REG(pll)),
			readl_relaxed(PLL_ALT_STATUS_REG(pll)));

		testlo = readl_relaxed(PLL_TEST_CTL_LO_REG(pll));
		testlo &= ~BM(7, 6);
		testlo |= 0xC0;
		writel_relaxed(testlo, PLL_TEST_CTL_LO_REG(pll));
		/* Wait for the write to complete */
		mb();

		pr_err("test_ctl_lo = 0x%x, pll status is: 0x%x\n",
			readl_relaxed(PLL_TEST_CTL_LO_REG(pll)),
			readl_relaxed(PLL_ALT_STATUS_REG(pll)));
		panic("failed to lock %s PLL\n", c->dbg_name);
	}

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/* Ensure that the write above goes through before returning. */
	mb();

	spin_unlock_irqrestore(&pll_reg_lock, flags);

	return ret;
}

static void variable_rate_pll_clk_disable_hwfsm(struct clk *c)
{
	struct pll_clk *pll = to_pll_clk(c);
	u32 regval;

	/* Set test control bit to stay-in-CFA if necessary */
	if (pll->test_ctl_lo_reg && pll->pgm_test_ctl_enable) {
		regval = readl_relaxed(PLL_TEST_CTL_LO_REG(pll));
		writel_relaxed(regval | BIT(16),
				PLL_TEST_CTL_LO_REG(pll));
	}

	/* 8 reference clock cycle delay mandated by the HPG */
	udelay(1);
}

static int variable_rate_pll_clk_enable_hwfsm(struct clk *c)
{
	struct pll_clk *pll = to_pll_clk(c);
	int count;
	u32 lockmask = pll->masks.lock_mask ?: PLL_LOCKED_BIT;
	unsigned long flags;
	u32 regval;

	spin_lock_irqsave(&pll_reg_lock, flags);

	/* Clear test control bit if necessary */
	if (pll->test_ctl_lo_reg && pll->pgm_test_ctl_enable) {
		regval = readl_relaxed(PLL_TEST_CTL_LO_REG(pll));
		regval &= ~BIT(16);
		writel_relaxed(regval, PLL_TEST_CTL_LO_REG(pll));
	}

	/* Wait for 50us explicitly to avoid transient locks */
	udelay(50);

	for (count = ENABLE_WAIT_MAX_LOOPS; count > 0; count--) {
		if (readl_relaxed(PLL_STATUS_REG(pll)) & lockmask)
			break;
		udelay(1);
	}

	if (!(readl_relaxed(PLL_STATUS_REG(pll)) & lockmask))
		pr_err("PLL %s didn't lock after enabling it!\n", c->dbg_name);

	spin_unlock_irqrestore(&pll_reg_lock, flags);

	return 0;
}

static void __pll_clk_enable_reg(void __iomem *mode_reg)
{
	u32 mode = readl_relaxed(mode_reg);
	/* Disable PLL bypass mode. */
	mode |= PLL_BYPASSNL;
	writel_relaxed(mode, mode_reg);

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	mb();
	udelay(10);

	/* De-assert active-low PLL reset. */
	mode |= PLL_RESET_N;
	writel_relaxed(mode, mode_reg);

	/* Wait until PLL is locked. */
	mb();
	udelay(50);

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, mode_reg);

	/* Ensure that the write above goes through before returning. */
	mb();
}

static int local_pll_clk_enable(struct clk *c)
{
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(c);

	spin_lock_irqsave(&pll_reg_lock, flags);
	__pll_clk_enable_reg(PLL_MODE_REG(pll));
	spin_unlock_irqrestore(&pll_reg_lock, flags);

	return 0;
}

static void __pll_clk_disable_reg(void __iomem *mode_reg)
{
	u32 mode = readl_relaxed(mode_reg);
	mode &= ~PLL_MODE_MASK;
	writel_relaxed(mode, mode_reg);
}

static void local_pll_clk_disable(struct clk *c)
{
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(c);

	/*
	 * Disable the PLL output, disable test mode, enable
	 * the bypass mode, and assert the reset.
	 */
	spin_lock_irqsave(&pll_reg_lock, flags);
	spm_event(pll->spm_ctrl.spm_base, pll->spm_ctrl.offset,
				pll->spm_ctrl.event_bit, true);
	__pll_clk_disable_reg(PLL_MODE_REG(pll));
	spin_unlock_irqrestore(&pll_reg_lock, flags);
}

static enum handoff local_pll_clk_handoff(struct clk *c)
{
	struct pll_clk *pll = to_pll_clk(c);
	u32 mode = readl_relaxed(PLL_MODE_REG(pll));
	u32 mask = PLL_BYPASSNL | PLL_RESET_N | PLL_OUTCTRL;
	unsigned long parent_rate;
	u32 lval, mval, nval, userval;

	if ((mode & mask) != mask)
		return HANDOFF_DISABLED_CLK;

	/* Assume bootloaders configure PLL to c->rate */
	if (c->rate)
		return HANDOFF_ENABLED_CLK;

	parent_rate = clk_get_rate(c->parent);
	lval = readl_relaxed(PLL_L_REG(pll));
	mval = readl_relaxed(PLL_M_REG(pll));
	nval = readl_relaxed(PLL_N_REG(pll));
	userval = readl_relaxed(PLL_CONFIG_REG(pll));

	c->rate = parent_rate * lval;

	if (pll->masks.mn_en_mask && userval) {
		if (!nval)
			nval = 1;
		c->rate += (parent_rate * mval) / nval;
	}

	return HANDOFF_ENABLED_CLK;
}

static long local_pll_clk_round_rate(struct clk *c, unsigned long rate)
{
	struct pll_freq_tbl *nf;
	struct pll_clk *pll = to_pll_clk(c);

	if (!pll->freq_tbl)
		return -EINVAL;

	for (nf = pll->freq_tbl; nf->freq_hz != PLL_FREQ_END; nf++)
		if (nf->freq_hz >= rate)
			return nf->freq_hz;

	nf--;
	return nf->freq_hz;
}

static int local_pll_clk_set_rate(struct clk *c, unsigned long rate)
{
	struct pll_freq_tbl *nf;
	struct pll_clk *pll = to_pll_clk(c);
	unsigned long flags;

	for (nf = pll->freq_tbl; nf->freq_hz != PLL_FREQ_END
			&& nf->freq_hz != rate; nf++)
		;

	if (nf->freq_hz == PLL_FREQ_END)
		return -EINVAL;

	/*
	 * Ensure PLL is off before changing rate. For optimization reasons,
	 * assume no downstream clock is using actively using it.
	 */
	spin_lock_irqsave(&c->lock, flags);
	if (c->count)
		c->ops->disable(c);

	writel_relaxed(nf->l_val, PLL_L_REG(pll));
	writel_relaxed(nf->m_val, PLL_M_REG(pll));
	writel_relaxed(nf->n_val, PLL_N_REG(pll));

	__pll_config_reg(PLL_CONFIG_REG(pll), nf, &pll->masks);

	if (c->count)
		c->ops->enable(c);

	spin_unlock_irqrestore(&c->lock, flags);
	return 0;
}

static enum handoff variable_rate_pll_handoff(struct clk *c)
{
	struct pll_clk *pll = to_pll_clk(c);
	u32 mode = readl_relaxed(PLL_MODE_REG(pll));
	u32 mask = PLL_BYPASSNL | PLL_RESET_N | PLL_OUTCTRL;
	u32 lval;

	pll->src_rate = clk_get_rate(c->parent);

	lval = readl_relaxed(PLL_L_REG(pll));
	if (!lval)
		return HANDOFF_DISABLED_CLK;

	c->rate = pll->src_rate * lval;

	if (c->rate > pll->max_rate || c->rate < pll->min_rate) {
		WARN(1, "%s: Out of spec PLL", c->dbg_name);
		return HANDOFF_DISABLED_CLK;
	}

	if ((mode & mask) != mask)
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

static long variable_rate_pll_round_rate(struct clk *c, unsigned long rate)
{
	struct pll_clk *pll = to_pll_clk(c);

	if (!pll->src_rate)
		return 0;

	if (pll->no_prepared_reconfig && c->prepare_count && c->rate != rate)
		return -EINVAL;

	if (rate < pll->min_rate)
		rate = pll->min_rate;
	if (rate > pll->max_rate)
		rate = pll->max_rate;

	return min(pll->max_rate,
			DIV_ROUND_UP(rate, pll->src_rate) * pll->src_rate);
}

/*
 * For optimization reasons, assumes no downstream clocks are actively using
 * it.
 */
static int variable_rate_pll_set_rate(struct clk *c, unsigned long rate)
{
	struct pll_clk *pll = to_pll_clk(c);
	unsigned long flags;
	u32 l_val;

	if (rate != variable_rate_pll_round_rate(c, rate))
		return -EINVAL;

	l_val = rate / pll->src_rate;

	spin_lock_irqsave(&c->lock, flags);

	if (c->count && c->ops->disable)
		c->ops->disable(c);

	writel_relaxed(l_val, PLL_L_REG(pll));

	if (c->count && c->ops->enable)
		c->ops->enable(c);

	spin_unlock_irqrestore(&c->lock, flags);

	return 0;
}

int sr_pll_clk_enable(struct clk *c)
{
	u32 mode;
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(c);

	spin_lock_irqsave(&pll_reg_lock, flags);
	mode = readl_relaxed(PLL_MODE_REG(pll));
	/* De-assert active-low PLL reset. */
	mode |= PLL_RESET_N;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/*
	 * H/W requires a 5us delay between disabling the bypass and
	 * de-asserting the reset. Delay 10us just to be safe.
	 */
	mb();
	udelay(10);

	/* Disable PLL bypass mode. */
	mode |= PLL_BYPASSNL;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/* Wait until PLL is locked. */
	mb();
	udelay(60);

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/* Ensure that the write above goes through before returning. */
	mb();

	spin_unlock_irqrestore(&pll_reg_lock, flags);

	return 0;
}

int sr_hpm_lp_pll_clk_enable(struct clk *c)
{
	unsigned long flags;
	struct pll_clk *pll = to_pll_clk(c);
	u32 count, mode;
	int ret = 0;

	spin_lock_irqsave(&pll_reg_lock, flags);

	/* Disable PLL bypass mode and de-assert reset. */
	mode = PLL_BYPASSNL | PLL_RESET_N;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/* Wait for pll to lock. */
	for (count = ENABLE_WAIT_MAX_LOOPS; count > 0; count--) {
		if (readl_relaxed(PLL_STATUS_REG(pll)) & PLL_LOCKED_BIT)
			break;
		udelay(1);
	}

	if (!(readl_relaxed(PLL_STATUS_REG(pll)) & PLL_LOCKED_BIT)) {
		WARN("PLL %s didn't lock after enabling it!\n", c->dbg_name);
		ret = -ETIMEDOUT;
		goto out;
	}

	/* Enable PLL output. */
	mode |= PLL_OUTCTRL;
	writel_relaxed(mode, PLL_MODE_REG(pll));

	/* Ensure the write above goes through before returning. */
	mb();

out:
	spin_unlock_irqrestore(&pll_reg_lock, flags);
	return ret;
}


static void __iomem *variable_rate_pll_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	struct pll_clk *pll = to_pll_clk(c);
	static struct clk_register_data data[] = {
		{"MODE", 0x0},
		{"L", 0x4},
		{"ALPHA", 0x8},
		{"USER_CTL", 0x10},
		{"CONFIG_CTL", 0x14},
		{"STATUS", 0x1C},
	};
	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return PLL_MODE_REG(pll);
}

static void __iomem *local_pll_clk_list_registers(struct clk *c, int n,
				struct clk_register_data **regs, u32 *size)
{
	/* Not compatible with 8960 & friends */
	struct pll_clk *pll = to_pll_clk(c);
	static struct clk_register_data data[] = {
		{"MODE", 0x0},
		{"L", 0x4},
		{"M", 0x8},
		{"N", 0xC},
		{"USER", 0x10},
		{"CONFIG", 0x14},
		{"STATUS", 0x1C},
	};
	if (n)
		return ERR_PTR(-EINVAL);

	*regs = data;
	*size = ARRAY_SIZE(data);
	return PLL_MODE_REG(pll);
}


struct clk_ops clk_ops_local_pll = {
	.enable = local_pll_clk_enable,
	.disable = local_pll_clk_disable,
	.set_rate = local_pll_clk_set_rate,
	.handoff = local_pll_clk_handoff,
	.list_registers = local_pll_clk_list_registers,
};

struct clk_ops clk_ops_sr2_pll = {
	.enable = sr2_pll_clk_enable,
	.disable = local_pll_clk_disable,
	.set_rate = local_pll_clk_set_rate,
	.round_rate = local_pll_clk_round_rate,
	.handoff = local_pll_clk_handoff,
	.list_registers = local_pll_clk_list_registers,
};

struct clk_ops clk_ops_variable_rate_pll_hwfsm = {
	.enable = variable_rate_pll_clk_enable_hwfsm,
	.disable = variable_rate_pll_clk_disable_hwfsm,
	.set_rate = variable_rate_pll_set_rate,
	.round_rate = variable_rate_pll_round_rate,
	.handoff = variable_rate_pll_handoff,
	.list_registers = variable_rate_pll_list_registers,
};

struct clk_ops clk_ops_variable_rate_pll = {
	.enable = variable_rate_pll_clk_enable,
	.disable = local_pll_clk_disable,
	.set_rate = variable_rate_pll_set_rate,
	.round_rate = variable_rate_pll_round_rate,
	.handoff = variable_rate_pll_handoff,
	.list_registers = variable_rate_pll_list_registers,
};

static DEFINE_SPINLOCK(soft_vote_lock);

static int pll_acpu_vote_clk_enable(struct clk *c)
{
	int ret = 0;
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&soft_vote_lock, flags);

	if (!*pllv->soft_vote)
		ret = pll_vote_clk_enable(c);
	if (ret == 0)
		*pllv->soft_vote |= (pllv->soft_vote_mask);

	spin_unlock_irqrestore(&soft_vote_lock, flags);
	return ret;
}

static void pll_acpu_vote_clk_disable(struct clk *c)
{
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&soft_vote_lock, flags);

	*pllv->soft_vote &= ~(pllv->soft_vote_mask);
	if (!*pllv->soft_vote)
		pll_vote_clk_disable(c);

	spin_unlock_irqrestore(&soft_vote_lock, flags);
}

static enum handoff pll_acpu_vote_clk_handoff(struct clk *c)
{
	if (pll_vote_clk_handoff(c) == HANDOFF_DISABLED_CLK)
		return HANDOFF_DISABLED_CLK;

	if (pll_acpu_vote_clk_enable(c))
		return HANDOFF_DISABLED_CLK;

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_pll_acpu_vote = {
	.enable = pll_acpu_vote_clk_enable,
	.disable = pll_acpu_vote_clk_disable,
	.is_enabled = pll_vote_clk_is_enabled,
	.handoff = pll_acpu_vote_clk_handoff,
	.list_registers = pll_vote_clk_list_registers,
};


static int pll_sleep_clk_enable(struct clk *c)
{
	u32 ena;
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&pll_reg_lock, flags);
	ena = readl_relaxed(PLL_EN_REG(pllv));
	ena &= ~(pllv->en_mask);
	writel_relaxed(ena, PLL_EN_REG(pllv));
	spin_unlock_irqrestore(&pll_reg_lock, flags);
	return 0;
}

static void pll_sleep_clk_disable(struct clk *c)
{
	u32 ena;
	unsigned long flags;
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	spin_lock_irqsave(&pll_reg_lock, flags);
	ena = readl_relaxed(PLL_EN_REG(pllv));
	ena |= pllv->en_mask;
	writel_relaxed(ena, PLL_EN_REG(pllv));
	spin_unlock_irqrestore(&pll_reg_lock, flags);
}

static enum handoff pll_sleep_clk_handoff(struct clk *c)
{
	struct pll_vote_clk *pllv = to_pll_vote_clk(c);

	if (!(readl_relaxed(PLL_EN_REG(pllv)) & pllv->en_mask))
		return HANDOFF_ENABLED_CLK;

	return HANDOFF_DISABLED_CLK;
}

/*
 * This .ops is meant to be used by gpll0_sleep_clk_src. The aim is to utilise
 * the h/w feature of sleep enable bit to denote if the PLL can be turned OFF
 * once APPS goes to PC. gpll0_sleep_clk_src will be enabled only if there is a
 * peripheral client using it and disabled if there is none. The current
 * implementation of enable .ops  clears the h/w bit of sleep enable while the
 * disable .ops asserts it.
 */

struct clk_ops clk_ops_pll_sleep_vote = {
	.enable = pll_sleep_clk_enable,
	.disable = pll_sleep_clk_disable,
	.handoff = pll_sleep_clk_handoff,
	.list_registers = pll_vote_clk_list_registers,
};

static void __set_fsm_mode(void __iomem *mode_reg,
					u32 bias_count, u32 lock_count)
{
	u32 regval = readl_relaxed(mode_reg);

	/* De-assert reset to FSM */
	regval &= ~BIT(21);
	writel_relaxed(regval, mode_reg);

	/* Program bias count */
	regval &= ~BM(19, 14);
	regval |= BVAL(19, 14, bias_count);
	writel_relaxed(regval, mode_reg);

	/* Program lock count */
	regval &= ~BM(13, 8);
	regval |= BVAL(13, 8, lock_count);
	writel_relaxed(regval, mode_reg);

	/* Enable PLL FSM voting */
	regval |= BIT(20);
	writel_relaxed(regval, mode_reg);
}

static void __configure_alt_config(struct pll_alt_config config,
		struct pll_config_regs *regs)
{
	u32 regval;

	regval = readl_relaxed(PLL_CFG_ALT_REG(regs));

	if (config.mask) {
		regval &= ~config.mask;
		regval |= config.val;
	}

	writel_relaxed(regval, PLL_CFG_ALT_REG(regs));
}

void __configure_pll(struct pll_config *config,
		struct pll_config_regs *regs, u32 ena_fsm_mode)
{
	u32 regval;

	writel_relaxed(config->l, PLL_L_REG(regs));
	writel_relaxed(config->m, PLL_M_REG(regs));
	writel_relaxed(config->n, PLL_N_REG(regs));

	regval = readl_relaxed(PLL_CONFIG_REG(regs));

	/* Enable the MN accumulator  */
	if (config->mn_ena_mask) {
		regval &= ~config->mn_ena_mask;
		regval |= config->mn_ena_val;
	}

	/* Enable the main output */
	if (config->main_output_mask) {
		regval &= ~config->main_output_mask;
		regval |= config->main_output_val;
	}

	/* Enable the aux output */
	if (config->aux_output_mask) {
		regval &= ~config->aux_output_mask;
		regval |= config->aux_output_val;
	}

	/* Set pre-divider and post-divider values */
	regval &= ~config->pre_div_mask;
	regval |= config->pre_div_val;
	regval &= ~config->post_div_mask;
	regval |= config->post_div_val;

	/* Select VCO setting */
	regval &= ~config->vco_mask;
	regval |= config->vco_val;

	if (config->add_factor_mask) {
		regval &= ~config->add_factor_mask;
		regval |= config->add_factor_val;
	}

	writel_relaxed(regval, PLL_CONFIG_REG(regs));

	if (regs->config_alt_reg)
		__configure_alt_config(config->alt_cfg, regs);

	if (regs->config_ctl_reg)
		writel_relaxed(config->cfg_ctl_val, PLL_CFG_CTL_REG(regs));
}

void configure_sr_pll(struct pll_config *config,
		struct pll_config_regs *regs, u32 ena_fsm_mode)
{
	__configure_pll(config, regs, ena_fsm_mode);
	if (ena_fsm_mode)
		__set_fsm_mode(PLL_MODE_REG(regs), 0x1, 0x8);
}

void configure_sr_hpm_lp_pll(struct pll_config *config,
		struct pll_config_regs *regs, u32 ena_fsm_mode)
{
	__configure_pll(config, regs, ena_fsm_mode);
	if (ena_fsm_mode)
		__set_fsm_mode(PLL_MODE_REG(regs), 0x1, 0x0);
}

static void *votable_pll_clk_dt_parser(struct device *dev,
						struct device_node *np)
{
	struct pll_vote_clk *v, *peer;
	struct clk *c;
	u32 val, rc;
	phandle p;
	struct msmclk_data *drv;

	v = devm_kzalloc(dev, sizeof(*v), GFP_KERNEL);
	if (!v) {
		dt_err(np, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	drv = msmclk_parse_phandle(dev, np->parent->phandle);
	if (IS_ERR_OR_NULL(drv))
		return ERR_CAST(drv);
	v->base = &drv->base;

	rc = of_property_read_u32(np, "qcom,en-offset", (u32 *)&v->en_reg);
	if (rc) {
		dt_err(np, "missing qcom,en-offset dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,en-bit", &val);
	if (rc) {
		dt_err(np, "missing qcom,en-bit dt property\n");
		return ERR_PTR(-EINVAL);
	}
	v->en_mask = BIT(val);

	rc = of_property_read_u32(np, "qcom,status-offset",
						(u32 *)&v->status_reg);
	if (rc) {
		dt_err(np, "missing qcom,status-offset dt property\n");
		return ERR_PTR(-EINVAL);
	}

	rc = of_property_read_u32(np, "qcom,status-bit", &val);
	if (rc) {
		dt_err(np, "missing qcom,status-bit dt property\n");
		return ERR_PTR(-EINVAL);
	}
	v->status_mask = BIT(val);

	rc = of_property_read_u32(np, "qcom,pll-config-rate", &val);
	if (rc) {
		dt_err(np, "missing qcom,pll-config-rate dt property\n");
		return ERR_PTR(-EINVAL);
	}
	v->c.rate = val;

	if (of_device_is_compatible(np, "qcom,active-only-pll"))
		v->soft_vote_mask = PLL_SOFT_VOTE_ACPU;
	else if (of_device_is_compatible(np, "qcom,sleep-active-pll"))
		v->soft_vote_mask = PLL_SOFT_VOTE_PRIMARY;

	if (of_device_is_compatible(np, "qcom,votable-pll")) {
		v->c.ops = &clk_ops_pll_vote;
		return msmclk_generic_clk_init(dev, np, &v->c);
	}

	rc = of_property_read_phandle_index(np, "qcom,peer", 0, &p);
	if (rc) {
		dt_err(np, "missing qcom,peer dt property\n");
		return ERR_PTR(-EINVAL);
	}

	c = msmclk_lookup_phandle(dev, p);
	if (!IS_ERR_OR_NULL(c)) {
		v->soft_vote = devm_kzalloc(dev, sizeof(*v->soft_vote),
						GFP_KERNEL);
		if (!v->soft_vote) {
			dt_err(np, "memory alloc failure\n");
			return ERR_PTR(-ENOMEM);
		}

		peer = to_pll_vote_clk(c);
		peer->soft_vote = v->soft_vote;
	}

	v->c.ops = &clk_ops_pll_acpu_vote;
	return msmclk_generic_clk_init(dev, np, &v->c);
}
MSMCLK_PARSER(votable_pll_clk_dt_parser, "qcom,active-only-pll", 0);
MSMCLK_PARSER(votable_pll_clk_dt_parser, "qcom,sleep-active-pll", 1);
MSMCLK_PARSER(votable_pll_clk_dt_parser, "qcom,votable-pll", 2);
