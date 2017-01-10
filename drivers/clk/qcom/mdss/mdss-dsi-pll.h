/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_DSI_PLL_H
#define __MDSS_DSI_PLL_H

#define MAX_DSI_PLL_EN_SEQS	10

#define DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG		(0x0020)
#define DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2	(0x0064)
#define DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG		(0x0068)
#define DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG1		(0x0070)

/* Register offsets for 20nm PHY PLL */
#define MMSS_DSI_PHY_PLL_PLL_CNTRL		(0x0014)
#define MMSS_DSI_PHY_PLL_PLL_BKG_KVCO_CAL_EN	(0x002C)
#define MMSS_DSI_PHY_PLL_PLLLOCK_CMP_EN		(0x009C)

struct lpfr_cfg {
	unsigned long vco_rate;
	u32 r;
};

struct dsi_pll_vco_clk {
	struct clk_hw	hw;

	unsigned long	ref_clk_rate;
	unsigned long	min_rate;
	unsigned long	max_rate;
	u32		pll_en_seq_cnt;
	struct lpfr_cfg *lpfr_lut;
	u32		lpfr_lut_size;
	void		*priv;

	int (*pll_enable_seqs[MAX_DSI_PLL_EN_SEQS])
			(struct mdss_pll_resources *dsi_pll_Res);
};

static inline struct dsi_pll_vco_clk *to_vco_hw(struct clk_hw *hw)
{
	return container_of(hw, struct dsi_pll_vco_clk, hw);
}

int dsi_pll_clock_register_14nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
#endif
