/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CLOCK_DSI_8610
#define __ARCH_ARM_MACH_MSM_CLOCK_DSI_8610

#include <mach/clk-provider.h>

struct dsi_pll_vco_clk {
	const unsigned long vco_clk_min;
	const unsigned long vco_clk_max;
	const unsigned pref_div_ratio;
	int factor;
	struct clk c;
};

extern struct clk_ops clk_ops_dsi_vco;
extern struct clk_ops clk_ops_dsi_byteclk;
extern struct clk_ops clk_ops_dsi_dsiclk;

int dsi_clk_ctrl_init(struct clk *ahb_clk);

#endif
