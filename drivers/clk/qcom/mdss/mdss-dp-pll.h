/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#ifdef CONFIG_QCOM_MDSS_DP_PLL
int dp_pll_clock_register_14nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int dp_pll_clock_register_10nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int dp_pll_clock_register_7nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
#else
static inline int dp_pll_clock_register_14nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	return 0;
}

static inline int dp_pll_clock_register_10nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	return 0;
}

static inline int dp_pll_clock_register_7nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	return 0;
}
#endif
#endif /* __MDSS_DP_PLL_H */
