/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
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

struct pll_vco {
	unsigned long min_freq;
	unsigned long max_freq;
	u32 val;
};

enum pll_type {
	ALPHA_PLL,
	FABIA_PLL,
	TRION_PLL,
};

/**
 * struct clk_alpha_pll - phase locked loop (PLL)
 * @offset: base address of registers
 * @inited: flag that's set when the PLL is initialized
 * @vco_table: array of VCO settings
 * @clkr: regmap clock handle
 * @pll_type: Specify the type of PLL
 */
struct clk_alpha_pll {
	u32 offset;
	struct pll_config *config;
	bool inited;

	const struct pll_vco *vco_table;
	size_t num_vco;

	struct clk_regmap clkr;
	u32 config_ctl_val;
#define PLLOUT_MAIN	BIT(0)
#define PLLOUT_AUX	BIT(1)
#define PLLOUT_AUX2	BIT(2)
#define PLLOUT_EARLY	BIT(3)
	u32 pllout_flags;
	enum pll_type type;
};

/**
 * struct clk_alpha_pll_postdiv - phase locked loop (PLL) post-divider
 * @offset: base address of registers
 * @width: width of post-divider
 * @post_div_shift: shift to differentiate between odd and even post-divider
 * @post_div_table: table with PLL odd and even post-divider settings
 * @num_post_div: Number of PLL post-divider settings
 * @clkr: regmap clock handle
 */
struct clk_alpha_pll_postdiv {
	u32 offset;
	u8 width;
	int post_div_shift;
	const struct clk_div_table *post_div_table;
	size_t num_post_div;
	struct clk_regmap clkr;
};

extern const struct clk_ops clk_alpha_pll_ops;
extern const struct clk_ops clk_alpha_pll_hwfsm_ops;
extern const struct clk_ops clk_alpha_pll_postdiv_ops;
extern const struct clk_ops clk_fabia_pll_ops;
extern const struct clk_ops clk_fabia_fixed_pll_ops;
extern const struct clk_ops clk_generic_pll_postdiv_ops;
extern const struct clk_ops clk_trion_pll_ops;
extern const struct clk_ops clk_trion_fixed_pll_ops;
extern const struct clk_ops clk_trion_pll_postdiv_ops;

void clk_alpha_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
		const struct pll_config *config);
void clk_fabia_pll_configure(struct clk_alpha_pll *pll,
		struct regmap *regmap, const struct pll_config *config);
int clk_trion_pll_configure(struct clk_alpha_pll *pll, struct regmap *regmap,
		const struct pll_config *config);

#endif
