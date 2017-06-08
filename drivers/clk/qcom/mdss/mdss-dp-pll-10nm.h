/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_DP_PLL_10NM_H
#define __MDSS_DP_PLL_10NM_H

#define DP_VCO_HSCLK_RATE_1620MHZDIV1000	1620000UL
#define DP_VCO_HSCLK_RATE_2700MHZDIV1000	2700000UL
#define DP_VCO_HSCLK_RATE_5400MHZDIV1000	5400000UL
#define DP_VCO_HSCLK_RATE_8100MHZDIV1000	8100000UL

struct dp_pll_db {
	struct mdss_pll_resources *pll;

	/* lane and orientation settings */
	u8 lane_cnt;
	u8 orientation;

	/* COM PHY settings */
	u32 hsclk_sel;
	u32 dec_start_mode0;
	u32 div_frac_start1_mode0;
	u32 div_frac_start2_mode0;
	u32 div_frac_start3_mode0;
	u32 integloop_gain0_mode0;
	u32 integloop_gain1_mode0;
	u32 vco_tune_map;
	u32 lock_cmp1_mode0;
	u32 lock_cmp2_mode0;
	u32 lock_cmp3_mode0;
	u32 lock_cmp_en;

	/* PHY vco divider */
	u32 phy_vco_div;
};

int dp_vco_set_rate_10nm(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate);
unsigned long dp_vco_recalc_rate_10nm(struct clk_hw *hw,
				unsigned long parent_rate);
long dp_vco_round_rate_10nm(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate);
int dp_vco_prepare_10nm(struct clk_hw *hw);
void dp_vco_unprepare_10nm(struct clk_hw *hw);
int dp_mux_set_parent_10nm(void *context,
				unsigned int reg, unsigned int val);
int dp_mux_get_parent_10nm(void *context,
				unsigned int reg, unsigned int *val);
#endif /* __MDSS_DP_PLL_10NM_H */
