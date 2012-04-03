/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <mach/socinfo.h>
#include <mach/msm_iomap.h>

#include "clock.h"
#include "clock-pll.h"
#include "smd_private.h"

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
	{62, 1200000000},
	{63, 1209600000},
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

static void pll_enable(void __iomem *addr, unsigned on)
{
	if (on) {
		writel_relaxed(2, addr);
		mb();
		udelay(5);
		writel_relaxed(6, addr);
		mb();
		udelay(50);
		writel_relaxed(7, addr);
	} else {
		writel_relaxed(0, addr);
	}
}

static int pll_clk_enable(struct clk *clk)
{
	struct pll_shared_clk *pll = to_pll_shared_clk(clk);
	unsigned int pll_id = pll->id;

	remote_spin_lock(&pll_lock);

	pll_control->pll[PLL_BASE + pll_id].votes |= BIT(1);
	if (!pll_control->pll[PLL_BASE + pll_id].on) {
		pll_enable(pll->mode_reg, 1);
		pll_control->pll[PLL_BASE + pll_id].on = 1;
	}

	remote_spin_unlock(&pll_lock);
	return 0;
}

static void pll_clk_disable(struct clk *clk)
{
	struct pll_shared_clk *pll = to_pll_shared_clk(clk);
	unsigned int pll_id = pll->id;

	remote_spin_lock(&pll_lock);

	pll_control->pll[PLL_BASE + pll_id].votes &= ~BIT(1);
	if (pll_control->pll[PLL_BASE + pll_id].on
	    && !pll_control->pll[PLL_BASE + pll_id].votes) {
		pll_enable(pll->mode_reg, 0);
		pll_control->pll[PLL_BASE + pll_id].on = 0;
	}

	remote_spin_unlock(&pll_lock);
}

static int pll_clk_is_enabled(struct clk *clk)
{
	struct pll_shared_clk *pll = to_pll_shared_clk(clk);

	return readl_relaxed(pll->mode_reg) & BIT(0);
}

static bool pll_clk_is_local(struct clk *clk)
{
	return true;
}

static enum handoff pll_clk_handoff(struct clk *clk)
{
	struct pll_shared_clk *pll = to_pll_shared_clk(clk);
	unsigned int pll_lval;
	struct pll_rate *l;

	/*
	 * Wait for the PLLs to be initialized and then read their frequency.
	 */
	do {
		pll_lval = readl_relaxed(pll->mode_reg + 4) & 0x3ff;
		cpu_relax();
		udelay(50);
	} while (pll_lval == 0);

	/* Convert PLL L values to PLL Output rate */
	for (l = pll_l_rate; l->rate != 0; l++) {
		if (l->lvalue == pll_lval) {
			clk->rate = l->rate;
			break;
		}
	}

	if (!clk->rate) {
		pr_crit("Unknown PLL's L value!\n");
		BUG();
	}

	return HANDOFF_ENABLED_CLK;
}

struct clk_ops clk_pll_ops = {
	.enable = pll_clk_enable,
	.disable = pll_clk_disable,
	.handoff = pll_clk_handoff,
	.is_local = pll_clk_is_local,
	.is_enabled = pll_clk_is_enabled,
};
