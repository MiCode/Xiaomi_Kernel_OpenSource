/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

static inline struct dsi_pll_vco_clk *to_vco_clk(struct clk *clk)
{
	return container_of(clk, struct dsi_pll_vco_clk, c);
}

int dsi_pll_clock_register_hpm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
int dsi_pll_clock_register_20nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
int dsi_pll_clock_register_lpm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
int dsi_pll_clock_register_8996(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int set_byte_mux_sel(struct mux_clk *clk, int sel);
int get_byte_mux_sel(struct mux_clk *clk);
int dsi_pll_div_prepare(struct clk *c);
int dsi_pll_mux_prepare(struct clk *c);
int fixed_4div_set_div(struct div_clk *clk, int div);
int fixed_4div_get_div(struct div_clk *clk);
int digital_set_div(struct div_clk *clk, int div);
int digital_get_div(struct div_clk *clk);
int analog_set_div(struct div_clk *clk, int div);
int analog_get_div(struct div_clk *clk);
int dsi_pll_lock_status(struct mdss_pll_resources *dsi_pll_res);
int vco_set_rate(struct dsi_pll_vco_clk *vco, unsigned long rate);
unsigned long vco_get_rate(struct clk *c);
long vco_round_rate(struct clk *c, unsigned long rate);
enum handoff vco_handoff(struct clk *c);
int vco_prepare(struct clk *c);
void vco_unprepare(struct clk *c);

/* APIs for 20nm PHY PLL */
int pll_20nm_vco_set_rate(struct dsi_pll_vco_clk *vco, unsigned long rate);
int shadow_pll_20nm_vco_set_rate(struct dsi_pll_vco_clk *vco,
				unsigned long rate);
long pll_20nm_vco_round_rate(struct clk *c, unsigned long rate);
enum handoff pll_20nm_vco_handoff(struct clk *c);
int pll_20nm_vco_prepare(struct clk *c);
void pll_20nm_vco_unprepare(struct clk *c);
int pll_20nm_vco_enable_seq(struct mdss_pll_resources *dsi_pll_res);

int set_bypass_lp_div_mux_sel(struct mux_clk *clk, int sel);
int set_shadow_bypass_lp_div_mux_sel(struct mux_clk *clk, int sel);
int get_bypass_lp_div_mux_sel(struct mux_clk *clk);
int fixed_hr_oclk2_set_div(struct div_clk *clk, int div);
int shadow_fixed_hr_oclk2_set_div(struct div_clk *clk, int div);
int fixed_hr_oclk2_get_div(struct div_clk *clk);
int hr_oclk3_set_div(struct div_clk *clk, int div);
int shadow_hr_oclk3_set_div(struct div_clk *clk, int div);
int hr_oclk3_get_div(struct div_clk *clk);
int ndiv_set_div(struct div_clk *clk, int div);
int shadow_ndiv_set_div(struct div_clk *clk, int div);
int ndiv_get_div(struct div_clk *clk);
void __dsi_pll_disable(void __iomem *pll_base);

int set_mdss_pixel_mux_sel(struct mux_clk *clk, int sel);
int get_mdss_pixel_mux_sel(struct mux_clk *clk);
int set_mdss_byte_mux_sel(struct mux_clk *clk, int sel);
int get_mdss_byte_mux_sel(struct mux_clk *clk);

#endif
