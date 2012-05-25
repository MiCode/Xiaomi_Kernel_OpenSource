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

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_PLL_H
#define __ARCH_ARM_MACH_MSM_CLOCK_PLL_H

/**
 * enum - For PLL IDs
 */
enum {
	PLL_TCXO	= -1,
	PLL_0	= 0,
	PLL_1,
	PLL_2,
	PLL_3,
	PLL_4,
	PLL_END,
};

/**
 * struct pll_shared_clk -  PLL shared with other processors without
 * any HW voting
 * @id: PLL ID
 * @mode_reg: enable register
 * @parent: clock source
 * @c: clk
 */
struct pll_shared_clk {
	unsigned int id;
	void __iomem *const mode_reg;
	struct clk c;
	void *const __iomem *base;
};

extern struct clk_ops clk_ops_pll;

static inline struct pll_shared_clk *to_pll_shared_clk(struct clk *clk)
{
	return container_of(clk, struct pll_shared_clk, c);
}

/**
 * msm_shared_pll_control_init() - Initialize shared pll control structure
 */
void msm_shared_pll_control_init(void);

/**
 * struct pll_vote_clk - phase locked loop (HW voteable)
 * @soft_vote: soft voting variable for multiple PLL software instances
 * @soft_vote_mask: soft voting mask for multiple PLL software instances
 * @en_reg: enable register
 * @en_mask: ORed with @en_reg to enable the clock
 * @status_mask: ANDed with @status_reg to determine if PLL is active.
 * @status_reg: status register
 * @parent: clock source
 * @c: clk
 */
struct pll_vote_clk {
	u32 *soft_vote;
	const u32 soft_vote_mask;
	void __iomem *const en_reg;
	const u32 en_mask;
	void __iomem *const status_reg;
	const u32 status_mask;

	struct clk *parent;
	struct clk c;
	void *const __iomem *base;
};

extern struct clk_ops clk_ops_pll_vote;

static inline struct pll_vote_clk *to_pll_vote_clk(struct clk *clk)
{
	return container_of(clk, struct pll_vote_clk, c);
}

/**
 * struct pll_clk - phase locked loop
 * @mode_reg: enable register
 * @status_reg: status register, contains the lock detection bit
 * @parent: clock source
 * @c: clk
 * @base: pointer to base address of ioremapped registers.
 */
struct pll_clk {
	void __iomem *const mode_reg;
	void __iomem *const status_reg;

	struct clk *parent;
	struct clk c;
	void *const __iomem *base;
};

extern struct clk_ops clk_ops_local_pll;

static inline struct pll_clk *to_pll_clk(struct clk *clk)
{
	return container_of(clk, struct pll_clk, c);
}

int sr_pll_clk_enable(struct clk *clk);
int copper_pll_clk_enable(struct clk *clk);

/*
 * PLL vote clock APIs
 */
int pll_vote_clk_enable(struct clk *clk);
void pll_vote_clk_disable(struct clk *clk);
struct clk *pll_vote_clk_get_parent(struct clk *clk);
int pll_vote_clk_is_enabled(struct clk *clk);

struct pll_config {
	u32 l;
	u32 m;
	u32 n;
	u32 vco_val;
	u32 vco_mask;
	u32 pre_div_val;
	u32 pre_div_mask;
	u32 post_div_val;
	u32 post_div_mask;
	u32 mn_ena_val;
	u32 mn_ena_mask;
	u32 main_output_val;
	u32 main_output_mask;
};

struct pll_config_regs {
	void __iomem *l_reg;
	void __iomem *m_reg;
	void __iomem *n_reg;
	void __iomem *config_reg;
	void __iomem *mode_reg;
	void *const __iomem *base;
};

void __init configure_pll(struct pll_config *, struct pll_config_regs *, u32);

#endif
