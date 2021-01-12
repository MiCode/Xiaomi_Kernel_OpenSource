/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2012-2018, 2020, 2021, The Linux Foundation. All rights reserved. */

#ifndef __MDSS_DSI_PLL_H
#define __MDSS_DSI_PLL_H

#include <linux/clk-provider.h>
#include "mdss-pll.h"
#define MAX_DSI_PLL_EN_SEQS	10

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

static inline struct dsi_pll_vco_clk *to_vco_clk_hw(struct clk_hw *hw)
{
	return container_of(hw, struct dsi_pll_vco_clk, hw);
}

int dsi_pll_clock_register_14nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
int dsi_pll_clock_register_28lpm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
#endif
