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

#ifndef __MDSS_EDP_PLL_H
#define __MDSS_EDP_PLL_H

struct edp_pll_vco_clk {
	unsigned long	ref_clk_rate;
	unsigned long	rate;	/* vco rate */
	unsigned long	*rate_list;
	void		*priv;

	struct clk	c;
};

int edp_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res);
#endif
