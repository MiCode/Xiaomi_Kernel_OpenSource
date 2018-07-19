/* Copyright (c) 2012-2018, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_DSI_PLL_28NM_H
#define __MDSS_DSI_PLL_28NM_H

#define DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG          (0x0020)
#define DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2       (0x0064)
#define DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG         (0x0068)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG1         (0x0070)

#define DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG	(0x0004)
#define DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG	(0x0028)
#define DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG		(0x0010)

struct ssc_params {
	s32 kdiv;
	s64 triang_inc_7_0;
	s64 triang_inc_9_8;
	s64 triang_steps;
	s64 dc_offset;
	s64 freq_seed_7_0;
	s64 freq_seed_15_8;
};

struct mdss_dsi_vco_calc {
	s64 sdm_cfg0;
	s64 sdm_cfg1;
	s64 sdm_cfg2;
	s64 sdm_cfg3;
	s64 cal_cfg10;
	s64 cal_cfg11;
	s64 refclk_cfg;
	s64 gen_vco_clk;
	u32 lpfr_lut_res;
	struct ssc_params ssc;
};

unsigned long vco_28nm_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate);
int vco_28nm_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate);
long vco_28nm_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *parent_rate);
int vco_28nm_prepare(struct clk_hw *hw);
void vco_28nm_unprepare(struct clk_hw *hw);

int analog_postdiv_reg_write(void *context,
				unsigned int reg, unsigned int div);
int analog_postdiv_reg_read(void *context,
				unsigned int reg, unsigned int *div);
int byteclk_mux_write_sel(void *context,
				unsigned int reg, unsigned int val);
int byteclk_mux_read_sel(void *context,
				unsigned int reg, unsigned int *val);
int pixel_clk_set_div(void *context,
				unsigned int reg, unsigned int div);
int pixel_clk_get_div(void *context,
				unsigned int reg, unsigned int *div);

int dsi_pll_lock_status(struct mdss_pll_resources *rsc);
#endif /* __MDSS_DSI_PLL_28NM_H */
