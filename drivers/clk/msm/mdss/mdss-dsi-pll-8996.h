/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_DSI_PLL_8996_H
#define MDSS_DSI_PLL_8996_H

#define DSIPHY_CMN_CLK_CFG0		0x0010
#define DSIPHY_CMN_CLK_CFG1		0x0014
#define DSIPHY_CMN_GLBL_TEST_CTRL	0x0018

#define DSIPHY_CMN_PLL_CNTRL		0x0048
#define DSIPHY_CMN_CTRL_0		0x001c
#define DSIPHY_CMN_CTRL_1		0x0020

#define DSIPHY_CMN_LDO_CNTRL		0x004c

#define DSIPHY_PLL_IE_TRIM		0x0400
#define DSIPHY_PLL_IP_TRIM		0x0404

#define DSIPHY_PLL_IPTAT_TRIM		0x0410

#define DSIPHY_PLL_CLKBUFLR_EN		0x041c

#define DSIPHY_PLL_SYSCLK_EN_RESET	0x0428
#define DSIPHY_PLL_RESETSM_CNTRL	0x042c
#define DSIPHY_PLL_RESETSM_CNTRL2	0x0430
#define DSIPHY_PLL_RESETSM_CNTRL3	0x0434
#define DSIPHY_PLL_RESETSM_CNTRL4	0x0438
#define DSIPHY_PLL_RESETSM_CNTRL5	0x043c
#define DSIPHY_PLL_KVCO_DIV_REF1	0x0440
#define DSIPHY_PLL_KVCO_DIV_REF2	0x0444
#define DSIPHY_PLL_KVCO_COUNT1		0x0448
#define DSIPHY_PLL_KVCO_COUNT2		0x044c
#define DSIPHY_PLL_VREF_CFG1		0x045c

#define DSIPHY_PLL_KVCO_CODE		0x0458

#define DSIPHY_PLL_VCO_DIV_REF1		0x046c
#define DSIPHY_PLL_VCO_DIV_REF2		0x0470
#define DSIPHY_PLL_VCO_COUNT1		0x0474
#define DSIPHY_PLL_VCO_COUNT2		0x0478
#define DSIPHY_PLL_PLLLOCK_CMP1		0x047c
#define DSIPHY_PLL_PLLLOCK_CMP2		0x0480
#define DSIPHY_PLL_PLLLOCK_CMP3		0x0484
#define DSIPHY_PLL_PLLLOCK_CMP_EN	0x0488
#define DSIPHY_PLL_PLL_VCO_TUNE		0x048C
#define DSIPHY_PLL_DEC_START		0x0490
#define DSIPHY_PLL_SSC_EN_CENTER	0x0494
#define DSIPHY_PLL_SSC_ADJ_PER1		0x0498
#define DSIPHY_PLL_SSC_ADJ_PER2		0x049c
#define DSIPHY_PLL_SSC_PER1		0x04a0
#define DSIPHY_PLL_SSC_PER2		0x04a4
#define DSIPHY_PLL_SSC_STEP_SIZE1	0x04a8
#define DSIPHY_PLL_SSC_STEP_SIZE2	0x04ac
#define DSIPHY_PLL_DIV_FRAC_START1	0x04b4
#define DSIPHY_PLL_DIV_FRAC_START2	0x04b8
#define DSIPHY_PLL_DIV_FRAC_START3	0x04bc
#define DSIPHY_PLL_TXCLK_EN		0x04c0
#define DSIPHY_PLL_PLL_CRCTRL		0x04c4

#define DSIPHY_PLL_RESET_SM_READY_STATUS 0x04cc

#define DSIPHY_PLL_PLL_MISC1		0x04e8

#define DSIPHY_PLL_CP_SET_CUR		0x04f0
#define DSIPHY_PLL_PLL_ICPMSET		0x04f4
#define DSIPHY_PLL_PLL_ICPCSET		0x04f8
#define DSIPHY_PLL_PLL_ICP_SET		0x04fc
#define DSIPHY_PLL_PLL_LPF1		0x0500
#define DSIPHY_PLL_PLL_LPF2_POSTDIV	0x0504
#define DSIPHY_PLL_PLL_BANDGAP	0x0508

#define DSI_DYNAMIC_REFRESH_PLL_CTRL15		0x050
#define DSI_DYNAMIC_REFRESH_PLL_CTRL19		0x060
#define DSI_DYNAMIC_REFRESH_PLL_CTRL20		0x064
#define DSI_DYNAMIC_REFRESH_PLL_CTRL21		0x068
#define DSI_DYNAMIC_REFRESH_PLL_CTRL22		0x06C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL23		0x070
#define DSI_DYNAMIC_REFRESH_PLL_CTRL24		0x074
#define DSI_DYNAMIC_REFRESH_PLL_CTRL25		0x078
#define DSI_DYNAMIC_REFRESH_PLL_CTRL26		0x07C
#define DSI_DYNAMIC_REFRESH_PLL_CTRL27		0x080
#define DSI_DYNAMIC_REFRESH_PLL_CTRL28		0x084
#define DSI_DYNAMIC_REFRESH_PLL_CTRL29		0x088
#define DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR	0x094
#define DSI_DYNAMIC_REFRESH_PLL_UPPER_ADDR2	0x098

struct dsi_pll_input {
	u32 fref;	/* 19.2 Mhz, reference clk */
	u32 fdata;	/* bit clock rate */
	u32 dsiclk_sel; /* 1, reg: 0x0014 */
	u32 n2div;	/* 1, reg: 0x0010, bit 4-7 */
	u32 ssc_en;	/* 1, reg: 0x0494, bit 0 */
	u32 ldo_en;	/* 0,  reg: 0x004c, bit 0 */

	/* fixed  */
	u32 refclk_dbler_en;	/* 0, reg: 0x04c0, bit 1 */
	u32 vco_measure_time;	/* 5, unknown */
	u32 kvco_measure_time;	/* 5, unknown */
	u32 bandgap_timer;	/* 4, reg: 0x0430, bit 3 - 5 */
	u32 pll_wakeup_timer;	/* 5, reg: 0x043c, bit 0 - 2 */
	u32 plllock_cnt;	/* 1, reg: 0x0488, bit 1 - 2 */
	u32 plllock_rng;	/* 1, reg: 0x0488, bit 3 - 4 */
	u32 ssc_center;		/* 0, reg: 0x0494, bit 1 */
	u32 ssc_adj_period;	/* 37, reg: 0x498, bit 0 - 9 */
	u32 ssc_spread;		/* 0.005  */
	u32 ssc_freq;		/* unknown */
	u32 pll_ie_trim;	/* 4, reg: 0x0400 */
	u32 pll_ip_trim;	/* 4, reg: 0x0404 */
	u32 pll_iptat_trim;	/* reg: 0x0410 */
	u32 pll_cpcset_cur;	/* 1, reg: 0x04f0, bit 0 - 2 */
	u32 pll_cpmset_cur;	/* 1, reg: 0x04f0, bit 3 - 5 */

	u32 pll_icpmset;	/* 4, reg: 0x04fc, bit 3 - 5 */
	u32 pll_icpcset;	/* 4, reg: 0x04fc, bit 0 - 2 */

	u32 pll_icpmset_p;	/* 0, reg: 0x04f4, bit 0 - 2 */
	u32 pll_icpmset_m;	/* 0, reg: 0x04f4, bit 3 - 5 */

	u32 pll_icpcset_p;	/* 0, reg: 0x04f8, bit 0 - 2 */
	u32 pll_icpcset_m;	/* 0, reg: 0x04f8, bit 3 - 5 */

	u32 pll_lpf_res1;	/* 3, reg: 0x0504, bit 0 - 3 */
	u32 pll_lpf_cap1;	/* 11, reg: 0x0500, bit 0 - 3 */
	u32 pll_lpf_cap2;	/* 1, reg: 0x0500, bit 4 - 7 */
	u32 pll_c3ctrl;		/* 2, reg: 0x04c4 */
	u32 pll_r3ctrl;		/* 1, reg: 0x04c4 */
};

struct dsi_pll_output {
	u32 pll_txclk_en;	/* reg: 0x04c0 */
	u32 dec_start;		/* reg: 0x0490 */
	u32 div_frac_start;	/* reg: 0x04b4, 0x4b8, 0x04bc */
	u32 ssc_period;		/* reg: 0x04a0, 0x04a4 */
	u32 ssc_step_size;	/* reg: 0x04a8, 0x04ac */
	u32 plllock_cmp;	/* reg: 0x047c, 0x0480, 0x0484 */
	u32 pll_vco_div_ref;	/* reg: 0x046c, 0x0470 */
	u32 pll_vco_count;	/* reg: 0x0474, 0x0478 */
	u32 pll_kvco_div_ref;	/* reg: 0x0440, 0x0444 */
	u32 pll_kvco_count;	/* reg: 0x0448, 0x044c */
	u32 pll_misc1;		/* reg: 0x04e8 */
	u32 pll_lpf2_postdiv;	/* reg: 0x0504 */
	u32 pll_resetsm_cntrl;	/* reg: 0x042c */
	u32 pll_resetsm_cntrl2;	/* reg: 0x0430 */
	u32 pll_resetsm_cntrl5;	/* reg: 0x043c */
	u32 pll_kvco_code;		/* reg: 0x0458 */

	u32 cmn_clk_cfg0;	/* reg: 0x0010 */
	u32 cmn_clk_cfg1;	/* reg: 0x0014 */
	u32 cmn_ldo_cntrl;	/* reg: 0x004c */

	u32 pll_postdiv;	/* vco */
	u32 pll_n1div;		/* vco */
	u32 pll_n2div;		/* hr_oclk3, pixel */
	u32 fcvo;
};

enum {
	DSI_PLL_0,
	DSI_PLL_1,
	DSI_PLL_NUM
};

struct dsi_pll_db {
	struct dsi_pll_db *next;
	struct mdss_pll_resources *pll;
	struct dsi_pll_input in;
	struct dsi_pll_output out;
	int source_setup_done;
};

enum {
	PLL_OUTPUT_NONE,
	PLL_OUTPUT_RIGHT,
	PLL_OUTPUT_LEFT,
	PLL_OUTPUT_BOTH
};

enum {
	PLL_SOURCE_FROM_LEFT,
	PLL_SOURCE_FROM_RIGHT
};

enum {
	PLL_UNKNOWN,
	PLL_STANDALONE,
	PLL_SLAVE,
	PLL_MASTER
};

int pll_vco_set_rate_8996(struct clk *c, unsigned long rate);
long pll_vco_round_rate_8996(struct clk *c, unsigned long rate);
enum handoff pll_vco_handoff_8996(struct clk *c);
enum handoff shadow_pll_vco_handoff_8996(struct clk *c);
int shadow_post_n1_div_set_div(struct div_clk *clk, int div);
int shadow_post_n1_div_get_div(struct div_clk *clk);
int shadow_n2_div_set_div(struct div_clk *clk, int div);
int shadow_n2_div_get_div(struct div_clk *clk);
int shadow_pll_vco_set_rate_8996(struct clk *c, unsigned long rate);
int pll_vco_prepare_8996(struct clk *c);
void pll_vco_unprepare_8996(struct clk *c);
int set_mdss_byte_mux_sel_8996(struct mux_clk *clk, int sel);
int get_mdss_byte_mux_sel_8996(struct mux_clk *clk);
int set_mdss_pixel_mux_sel_8996(struct mux_clk *clk, int sel);
int get_mdss_pixel_mux_sel_8996(struct mux_clk *clk);
int post_n1_div_set_div(struct div_clk *clk, int div);
int post_n1_div_get_div(struct div_clk *clk);
int n2_div_set_div(struct div_clk *clk, int div);
int n2_div_get_div(struct div_clk *clk);
int dsi_pll_enable_seq_8996(struct mdss_pll_resources *pll);

#endif  /* MDSS_DSI_PLL_8996_H */
