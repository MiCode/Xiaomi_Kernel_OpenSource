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
};

extern struct clk_ops clk_pll_ops;

static inline struct pll_shared_clk *to_pll_shared_clk(struct clk *clk)
{
	return container_of(clk, struct pll_shared_clk, c);
}

/**
 * msm_shared_pll_control_init() - Initialize shared pll control structure
 */
void msm_shared_pll_control_init(void);
