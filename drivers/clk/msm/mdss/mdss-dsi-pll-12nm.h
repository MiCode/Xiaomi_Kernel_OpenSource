/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_DSI_PLL_12NM_H
#define MDSS_DSI_PLL_12NM_H

#define DSIPHY_PLL_POWERUP_CTRL					0x034
#define DSIPHY_PLL_PROP_CHRG_PUMP_CTRL			0x038
#define DSIPHY_PLL_INTEG_CHRG_PUMP_CTRL			0x03c
#define DSIPHY_PLL_ANA_TST_LOCK_ST_OVR_CTRL	0x044
#define DSIPHY_PLL_VCO_CTRL				0x048
#define DSIPHY_PLL_GMP_CTRL_DIG_TST			0x04c
#define DSIPHY_PLL_PHA_ERR_CTRL_0			0x050
#define DSIPHY_PLL_LOCK_FILTER				0x054
#define DSIPHY_PLL_UNLOCK_FILTER			0x058
#define DSIPHY_PLL_INPUT_DIV_PLL_OVR			0x05c
#define DSIPHY_PLL_LOOP_DIV_RATIO_0			0x060
#define DSIPHY_PLL_INPUT_LOOP_DIV_RAT_CTRL		0x064
#define DSIPHY_PLL_PRO_DLY_RELOCK			0x06c
#define DSIPHY_PLL_CHAR_PUMP_BIAS_CTRL			0x070
#define DSIPHY_PLL_LOCK_DET_MODE_SEL			0x074
#define DSIPHY_PLL_ANA_PROG_CTRL			0x07c
#define DSIPHY_HS_FREQ_RAN_SEL				0x110
#define DSIPHY_SLEWRATE_FSM_OVR_CTRL			0x280
#define DSIPHY_SLEWRATE_DDL_LOOP_CTRL			0x28c
#define DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_0		0x290
#define DSIPHY_PLL_PHA_ERR_CTRL_1			0x2e4
#define DSIPHY_PLL_LOOP_DIV_RATIO_1			0x2e8
#define DSIPHY_SLEWRATE_DDL_CYC_FRQ_ADJ_1		0x328
#define DSIPHY_SSC0					0x394
#define DSIPHY_SSC7					0x3b0
#define DSIPHY_SSC8					0x3b4
#define DSIPHY_SSC1					0x398
#define DSIPHY_SSC2					0x39c
#define DSIPHY_SSC3					0x3a0
#define DSIPHY_SSC4					0x3a4
#define DSIPHY_SSC5					0x3a8
#define DSIPHY_SSC6					0x3ac
#define DSIPHY_SSC10					0x360
#define DSIPHY_SSC11					0x364
#define DSIPHY_SSC12					0x368
#define DSIPHY_SSC13					0x36c
#define DSIPHY_SSC14					0x370
#define DSIPHY_SSC15					0x374
#define DSIPHY_SSC7					0x3b0
#define DSIPHY_SSC8					0x3b4
#define DSIPHY_SSC9					0x3b8
#define DSIPHY_STAT0					0x3e0
#define DSIPHY_CTRL0					0x3e8
#define DSIPHY_SYS_CTRL					0x3f0
#define DSIPHY_PLL_CTRL					0x3f8

struct dsi_pll_param {
	u32 hsfreqrange;
	u32 vco_cntrl;
	u32 osc_freq_target;
	u32 m_div;
	u32 prop_cntrl;
	u32 int_cntrl;
	u32 gmp_cntrl;
	u32 cpbias_cntrl;

	/* mux and dividers */
	u32 gp_div_mux;
	u32 post_div_mux;
	u32 pixel_divhf;
	u32 fsm_ovr_ctrl;

	/* ssc_params */
	u32 mpll_ssc_peak_i;
	u32 mpll_stepsize_i;
	u32 mpll_mint_i;
	u32 mpll_frac_den;
	u32 mpll_frac_quot_i;
	u32 mpll_frac_rem;
};

enum {
	DSI_PLL_0,
	DSI_PLL_1,
	DSI_PLL_NUM
};

struct dsi_pll_db {
	struct dsi_pll_db *next;
	struct mdss_pll_resources *pll;
	struct dsi_pll_param param;
};

int pll_vco_set_rate_12nm(struct clk *c, unsigned long rate);
long pll_vco_round_rate_12nm(struct clk *c, unsigned long rate);
enum handoff pll_vco_handoff_12nm(struct clk *c);
int pll_vco_prepare_12nm(struct clk *c);
void pll_vco_unprepare_12nm(struct clk *c);
int pll_vco_enable_12nm(struct clk *c);
int pixel_div_set_div(struct div_clk *clk, int div);
int pixel_div_get_div(struct div_clk *clk);
int set_post_div_mux_sel(struct mux_clk *clk, int sel);
int get_post_div_mux_sel(struct mux_clk *clk);
int set_gp_mux_sel(struct mux_clk *clk, int sel);
int get_gp_mux_sel(struct mux_clk *clk);
int dsi_pll_enable_seq_12nm(struct mdss_pll_resources *pll);

#endif  /* MDSS_DSI_PLL_12NM_H */
