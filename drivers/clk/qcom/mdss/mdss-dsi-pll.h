/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

struct lpfr_cfg {
	unsigned long vco_rate;
	u32 r;
};

struct dsi_pll_vco_clk {
	unsigned long	ref_clk_rate;
	unsigned long	min_rate;
	unsigned long	max_rate;
	u32		pll_en_seq_cnt;
	struct lpfr_cfg *lpfr_lut;
	u32		lpfr_lut_size;
	void		*priv;

	struct clk	c;

	int (*pll_enable_seqs[MAX_DSI_PLL_EN_SEQS])
			(struct mdss_pll_resources *dsi_pll_Res);
};

int dsi_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
#endif
