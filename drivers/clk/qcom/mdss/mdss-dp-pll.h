/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016-2018, 2020, The Linux Foundation. All rights reserved. */

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

#endif /* __MDSS_DP_PLL_H */
