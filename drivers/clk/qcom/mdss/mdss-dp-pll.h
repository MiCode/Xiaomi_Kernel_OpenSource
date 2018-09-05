/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_DP_PLL_H
#define __MDSS_DP_PLL_H

struct dp_pll_vco_clk {
	struct clk_hw hw;
	unsigned long	rate;		/* current vco rate */
	u64		min_rate;	/* min vco rate */
	u64		max_rate;	/* max vco rate */
	void		*priv;
};

static inline struct dp_pll_vco_clk *to_dp_vco_hw(struct clk_hw *hw)
{
	return container_of(hw, struct dp_pll_vco_clk, hw);
}

int dp_pll_clock_register_14nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int dp_pll_clock_register_10nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int dp_pll_clock_register_7nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

#endif /* __MDSS_DP_PLL_H */
