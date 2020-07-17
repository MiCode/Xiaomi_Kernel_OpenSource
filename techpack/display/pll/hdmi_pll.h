/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __MDSS_HDMI_PLL_H
#define __MDSS_HDMI_PLL_H

struct hdmi_pll_cfg {
	unsigned long vco_rate;
	u32 reg;
};

struct hdmi_pll_vco_clk {
	struct clk_hw	hw;
	unsigned long	rate;	/* current vco rate */
	unsigned long	min_rate;	/* min vco rate */
	unsigned long	max_rate;	/* max vco rate */
	bool		rate_set;
	struct hdmi_pll_cfg *ip_seti;
	struct hdmi_pll_cfg *cp_seti;
	struct hdmi_pll_cfg *ip_setp;
	struct hdmi_pll_cfg *cp_setp;
	struct hdmi_pll_cfg *crctrl;
	void		*priv;

};

static inline struct hdmi_pll_vco_clk *to_hdmi_vco_clk_hw(struct clk_hw *hw)
{
	return container_of(hw, struct hdmi_pll_vco_clk, hw);
}

int hdmi_pll_clock_register_28lpm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int hdmi_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int hdmi_20nm_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);

int hdmi_8996_v1_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res);

int hdmi_8996_v2_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res);

int hdmi_8996_v3_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res);

int hdmi_8996_v3_1p8_pll_clock_register(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res);

int hdmi_8998_pll_clock_register(struct platform_device *pdev,
				   struct mdss_pll_resources *pll_res);
#endif
