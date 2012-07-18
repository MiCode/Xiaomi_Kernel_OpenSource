/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/remote_spinlock.h>

#include <mach/scm-io.h>
#include <mach/msm_iomap.h>

#include "clock.h"
#include "clock-pll.h"
#include "smd_private.h"

#ifdef CONFIG_MSM_SECURE_IO
#undef readl_relaxed
#undef writel_relaxed
#define readl_relaxed secure_readl
#define writel_relaxed secure_writel
#endif

#define PLL_OUTCTRL BIT(0)
#define PLL_BYPASSNL BIT(1)
#define PLL_RESET_N BIT(2)
#define PLL_MODE_MASK BM(3, 0)

#define PLL_EN_REG(x) ((x)->base ? (*(x)->base + (u32)((x)->en_reg)) : \
				((x)->en_reg))
#define PLL_STATUS_REG(x) ((x)->base ? (*(x)->base + (u32)((x)->status_reg)) : \
				((x)->status_reg))
#define PLL_MODE_REG(x) ((x)->base ? (*(x)->base + (u32)((x)->mode_reg)) : \
				((x)->mode_reg))
#define PLL_L_REG(x) ((x)->base ? (*(x)->base + (u32)((x)->l_reg)) : \
				((x)->l_reg))
#define PLL_M_REG(x) ((x)->base ? (*(x)->base + (u32)((x)->m_reg)) : \
				((x)->m_reg))
#define PLL_N_REG(x) ((x)->base ? (*(x)->base + (u32)((x)->n_reg)) : \
				((x)->n_reg))
#define PLL_CONFIG_REG(x) ((x)->base ? (*(x)->base + (u32)((x)->config_reg)) : \
				((x)->config_reg))

static DEFINE_SPINLOCK(pll_reg_lock);

#define ENABLE_WAIT_MAX_LOOPS 200

int pll_vote_clk_enable(struct clk *c)
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

void pll_vote_clk_disable(struct clk *c)
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

struct clk *pll_vote_clk_get_parent(struct clk *c)
{
	return to_pll_vote_clk(c)->parent;
}

int pll_vote_clk_is_enabled(struct clk *c)
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

struct clk_ops clk_ops_pll_vote = {
	.enable = pll_vote_clk_enable,
	.disable = pll_vote_clk_disable,
	.is_enabled = pll_vote_clk_is_enabled,
	.get_parent = pll_vote_clk_get_parent,
	.handoff = pll_vote_clk_handoff,
};

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
	__pll_clk_disable_reg(PLL_MODE_REG(pll));
	spin_unlock_irqrestore(&pll_reg_lock, flags);
}

static enum handoff local_pll_clk_handoff(struct clk *c)
{
	struct pll_clk *pll = to_pll_clk(c);
	u32 mode = readl_relaxed(PLL_MODE_REG(pll));
	u32 mask = PLL_BYPASSNL | PLL_RESET_N | PLL_OUTCTRL;

	if ((mode & mask) == mask)
		return HANDOFF_ENABLED_CLK;

	return HANDOFF_DISABLED_CLK;
}

static struct clk *local_pll_clk_get_parent(struct clk *c)
{
	return to_pll_clk(c)->parent;
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

#define PLL_LOCKED_BIT BIT(16)

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

struct clk_ops clk_ops_local_pll = {
	.enable = local_pll_clk_enable,
	.disable = local_pll_clk_disable,
	.handoff = local_pll_clk_handoff,
	.get_parent = local_pll_clk_get_parent,
};

struct pll_rate {
	unsigned int lvalue;
	unsigned long rate;
};

static struct pll_rate pll_l_rate[] = {
	{10, 196000000},
	{12, 245760000},
	{30, 589820000},
	{38, 737280000},
	{41, 800000000},
	{50, 960000000},
	{52, 1008000000},
	{60, 1152000000},
	{62, 1200000000},
	{63, 1209600000},
	{73, 1401600000},
	{0, 0},
};

#define PLL_BASE	7

struct shared_pll_control {
	uint32_t	version;
	struct {
		/*
		 * Denotes if the PLL is ON. Technically, this can be read
		 * directly from the PLL registers, but this feild is here,
		 * so let's use it.
		 */
		uint32_t	on;
		/*
		 * One bit for each processor core. The application processor
		 * is allocated bit position 1. All other bits should be
		 * considered as votes from other processors.
		 */
		uint32_t	votes;
	} pll[PLL_BASE + PLL_END];
};

static remote_spinlock_t pll_lock;
static struct shared_pll_control *pll_control;

void __init msm_shared_pll_control_init(void)
{
#define PLL_REMOTE_SPINLOCK_ID "S:7"
	unsigned smem_size;

	remote_spin_lock_init(&pll_lock, PLL_REMOTE_SPINLOCK_ID);

	pll_control = smem_get_entry(SMEM_CLKREGIM_SOURCES, &smem_size);
	if (!pll_control) {
		pr_err("Can't find shared PLL control data structure!\n");
		BUG();
	/*
	 * There might be more PLLs than what the application processor knows
	 * about. But the index used for each PLL is guaranteed to remain the
	 * same.
	 */
	} else if (smem_size < sizeof(struct shared_pll_control)) {
			pr_err("Shared PLL control data"
					 "structure too small!\n");
			BUG();
	} else if (pll_control->version != 0xCCEE0001) {
			pr_err("Shared PLL control version mismatch!\n");
			BUG();
	} else {
		pr_info("Shared PLL control available.\n");
		return;
	}

}

static int pll_clk_enable(struct clk *c)
{
	struct pll_shared_clk *pll = to_pll_shared_clk(c);
	unsigned int pll_id = pll->id;

	remote_spin_lock(&pll_lock);

	pll_control->pll[PLL_BASE + pll_id].votes |= BIT(1);
	if (!pll_control->pll[PLL_BASE + pll_id].on) {
		__pll_clk_enable_reg(PLL_MODE_REG(pll));
		pll_control->pll[PLL_BASE + pll_id].on = 1;
	}

	remote_spin_unlock(&pll_lock);
	return 0;
}

static void pll_clk_disable(struct clk *c)
{
	struct pll_shared_clk *pll = to_pll_shared_clk(c);
	unsigned int pll_id = pll->id;

	remote_spin_lock(&pll_lock);

	pll_control->pll[PLL_BASE + pll_id].votes &= ~BIT(1);
	if (pll_control->pll[PLL_BASE + pll_id].on
	    && !pll_control->pll[PLL_BASE + pll_id].votes) {
		__pll_clk_disable_reg(PLL_MODE_REG(pll));
		pll_control->pll[PLL_BASE + pll_id].on = 0;
	}

	remote_spin_unlock(&pll_lock);
}

static int pll_clk_is_enabled(struct clk *c)
{
	return readl_relaxed(PLL_MODE_REG(to_pll_shared_clk(c))) & BIT(0);
}

static enum handoff pll_clk_handoff(struct clk *c)
{
	struct pll_shared_clk *pll = to_pll_shared_clk(c);
	unsigned int pll_lval;
	struct pll_rate *l;

	/*
	 * Wait for the PLLs to be initialized and then read their frequency.
	 */
	do {
		pll_lval = readl_relaxed(PLL_MODE_REG(pll) + 4) & 0x3ff;
		cpu_relax();
		udelay(50);
	} while (pll_lval == 0);

	/* Convert PLL L values to PLL Output rate */
	for (l = pll_l_rate; l->rate != 0; l++) {
		if (l->lvalue == pll_lval) {
			c->rate = l->rate;
			break;
		}
	}

	if (!c->rate) {
		pr_crit("Unknown PLL's L value!\n");
		BUG();
	}

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_ops_pll = {
	.enable = pll_clk_enable,
	.disable = pll_clk_disable,
	.handoff = pll_clk_handoff,
	.is_enabled = pll_clk_is_enabled,
};

static void __init __set_fsm_mode(void __iomem *mode_reg,
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

void __init __configure_pll(struct pll_config *config,
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

	/* Set pre-divider and post-divider values */
	regval &= ~config->pre_div_mask;
	regval |= config->pre_div_val;
	regval &= ~config->post_div_mask;
	regval |= config->post_div_val;

	/* Select VCO setting */
	regval &= ~config->vco_mask;
	regval |= config->vco_val;
	writel_relaxed(regval, PLL_CONFIG_REG(regs));
}

void __init configure_sr_pll(struct pll_config *config,
		struct pll_config_regs *regs, u32 ena_fsm_mode)
{
	__configure_pll(config, regs, ena_fsm_mode);
	if (ena_fsm_mode)
		__set_fsm_mode(PLL_MODE_REG(regs), 0x1, 0x8);
}

void __init configure_sr_hpm_lp_pll(struct pll_config *config,
		struct pll_config_regs *regs, u32 ena_fsm_mode)
{
	__configure_pll(config, regs, ena_fsm_mode);
	if (ena_fsm_mode)
		__set_fsm_mode(PLL_MODE_REG(regs), 0x1, 0x0);
}

