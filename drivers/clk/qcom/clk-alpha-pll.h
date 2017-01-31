/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef __QCOM_CLK_ALPHA_PLL_H__
#define __QCOM_CLK_ALPHA_PLL_H__

#include <linux/clk-provider.h>
#include "clk-regmap.h"
#include "clk-pll.h"

struct pll_vco_data {
	unsigned long freq;
	u8 post_div_val;
};

struct pll_vco {
	unsigned long min_freq;
	unsigned long max_freq;
	u32 val;
};

/**
 * struct clk_alpha_pll - phase locked loop (PLL)
 * @offset: base address of registers
 * @soft_vote: soft voting variable for multiple PLL software instances
 * @soft_vote_mask: soft voting mask for multiple PLL software instances
 * @vco_table: array of VCO settings
 * @vco_data: array of VCO data settings like post div
 * @clkr: regmap clock handle
 */
struct clk_alpha_pll {
	u32 offset;
	struct pll_config *config;

	u32 *soft_vote;
	u32 soft_vote_mask;
/* Soft voting values */
#define PLL_SOFT_VOTE_PRIMARY	BIT(0)
#define PLL_SOFT_VOTE_CPU	BIT(1)
#define PLL_SOFT_VOTE_AUX	BIT(2)

	const struct pll_vco *vco_table;
	size_t num_vco;

	const struct pll_vco_data *vco_data;
	size_t num_vco_data;

	u8 flags;
#define SUPPORTS_FSM_MODE	BIT(0)
	/* some PLLs support dynamically updating their rate
	 * without disabling the PLL first. Set this flag
	 * to enable this support.
	 */
#define SUPPORTS_DYNAMIC_UPDATE BIT(1)
#define SUPPORTS_SLEW		BIT(2)
	/* associated with soft_vote for multiple PLL software instances */
#define SUPPORTS_FSM_VOTE	BIT(3)

	struct clk_regmap clkr;
#define PLLOUT_MAIN	BIT(0)
#define PLLOUT_AUX	BIT(1)
#define PLLOUT_AUX2	BIT(2)
#define PLLOUT_EARLY	BIT(3)
	u32 pllout_flags;
	unsigned long min_supported_freq;
};

/**
 * struct clk_alpha_pll_postdiv - phase locked loop (PLL) post-divider
 * @offset: base address of registers
 * @width: width of post-divider
 * @clkr: regmap clock handle
 */
struct clk_alpha_pll_postdiv {
	u32 offset;
	u8 width;

	struct clk_regmap clkr;
};

extern const struct clk_ops clk_alpha_pll_ops;
extern const struct clk_ops clk_alpha_pll_hwfsm_ops;
extern const struct clk_ops clk_alpha_pll_postdiv_ops;
extern const struct clk_ops clk_alpha_pll_slew_ops;

void clk_alpha_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
		const struct pll_config *config);

#endif
