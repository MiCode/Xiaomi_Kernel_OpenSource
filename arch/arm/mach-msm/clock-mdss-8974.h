/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_MDSS_8974
#define __ARCH_ARM_MACH_MSM_CLOCK_MDSS_8974

#include <linux/clk.h>

#define MAX_DSI_PLL_EN_SEQS	10

extern struct clk_ops clk_ops_dsi_byte_pll;
extern struct clk_ops clk_ops_dsi_pixel_pll;

void mdss_clk_ctrl_pre_init(struct clk *ahb_clk);
void mdss_clk_ctrl_post_init(void);

struct edp_pll_vco_clk {
	unsigned long ref_clk_rate;
	unsigned long rate;	/* vco rate */
	unsigned long *rate_list;

	struct clk c;
};

struct hdmi_pll_vco_clk {
	unsigned long rate;	/* vco rate */
	unsigned long *rate_list;
	bool rate_set;

	struct clk c;
};

struct lpfr_cfg {
	unsigned long vco_rate;
	u32 r;
};

struct dsi_pll_vco_clk {
	unsigned long ref_clk_rate;
	unsigned long min_rate;
	unsigned long max_rate;
	int (*pll_enable_seqs[MAX_DSI_PLL_EN_SEQS])(void);
	u32 pll_en_seq_cnt;
	struct lpfr_cfg *lpfr_lut;
	u32 lpfr_lut_size;

	struct clk c;
};

extern struct dsi_pll_vco_clk dsi_vco_clk_8974;
extern struct div_clk analog_postdiv_clk_8974;
extern struct div_clk indirect_path_div2_clk_8974;
extern struct div_clk pixel_clk_src_8974;
extern struct mux_clk byte_mux_8974;
extern struct div_clk byte_clk_src_8974;

extern struct dsi_pll_vco_clk dsi_vco_clk_8226;
extern struct div_clk analog_postdiv_clk_8226;
extern struct div_clk indirect_path_div2_clk_8226;
extern struct div_clk pixel_clk_src_8226;
extern struct mux_clk byte_mux_8226;
extern struct div_clk byte_clk_src_8226;

extern struct div_clk edp_mainlink_clk_src;
extern struct div_clk edp_pixel_clk_src;
extern struct dsi_pll_vco_clk dsi_vco_clk_8084;
extern struct div_clk analog_postdiv_clk_8084;
extern struct div_clk indirect_path_div2_clk_8084;
extern struct div_clk pixel_clk_src_8084;
extern struct mux_clk byte_mux_8084;
extern struct div_clk byte_clk_src_8084;

extern struct div_clk hdmipll_clk_src;

extern struct dsi_pll_vco_clk dsi_vco_clk_samarium;
extern struct div_clk analog_postdiv_clk_samarium;
extern struct div_clk indirect_path_div2_clk_samarium;
extern struct div_clk pixel_clk_src_samarium;
extern struct mux_clk byte_mux_samarium;
extern struct div_clk byte_clk_src_samarium;

#endif
