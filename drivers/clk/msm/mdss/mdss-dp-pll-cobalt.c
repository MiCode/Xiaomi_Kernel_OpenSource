/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
***************************************************************************
******** Display Port PLL driver block diagram for branch clocks **********
***************************************************************************

			   +-------------------+
			   |    dp_vco_clk     |
			   |   (DP PLL/VCO)    |
			   +---------+---------+
				     |
				     |
				     v
			  +----------+-----------+
			  | hsclk_divsel_clk_src |
			  +----------+-----------+
				     |
				     |
				     v
	   +------------<------------|------------>-------------+
	   |                         |                          |
	   |                         |                          |
+----------v----------+	  +----------v----------+    +----------v----------+
|vco_divided_clk_src  |	  |    dp_link_2x_clk	|    |	 dp_link_2x_clk	   |
|   (aux_clk_ops)     |	  |			|    |			   |
v----------+----------v	  |	divsel_five	|    |	   divsel_ten	   |
	   |		  +----------+----------+    +----------+----------+
	   |                         |                          |
	   v			     v				v
				     |	+--------------------+	|
  Input to MMSSCC block		     |	|		     |	|
   for DP pixel clock		     +--> dp_link_2x_clk_mux <--+
					|		     |
					+----------+---------+
						   |
						   v
					 Input to MMSSCC block
					 for link clk, crypto clk
					 and interface clock


******************************************************************************
*/

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-cobalt.h>

#include "mdss-pll.h"
#include "mdss-dp-pll.h"
#include "mdss-dp-pll-cobalt.h"

static struct clk_ops clk_ops_gen_mux_dp;
static struct clk_ops clk_ops_hsclk_divsel_clk_src_c;
static struct clk_ops clk_ops_vco_divided_clk_src_c;
static struct clk_ops clk_ops_link_2x_clk_div_c;

static struct clk_div_ops hsclk_divsel_ops = {
	.set_div = hsclk_divsel_set_div,
	.get_div = hsclk_divsel_get_div,
};

static struct clk_div_ops link2xclk_divsel_ops = {
	.set_div = link2xclk_divsel_set_div,
	.get_div = link2xclk_divsel_get_div,
};

static struct clk_div_ops vco_divided_clk_ops = {
	.set_div = vco_divided_clk_set_div,
	.get_div = vco_divided_clk_get_div,
};

static struct clk_ops dp_cobalt_vco_clk_ops = {
	.set_rate = dp_vco_set_rate,
	.round_rate = dp_vco_round_rate,
	.prepare = dp_vco_prepare,
	.unprepare = dp_vco_unprepare,
	.handoff = dp_vco_handoff,
};

static struct clk_mux_ops mdss_mux_ops = {
	.set_mux_sel = mdss_set_mux_sel,
	.get_mux_sel = mdss_get_mux_sel,
};

static struct dp_pll_vco_clk dp_vco_clk = {
	.min_rate = DP_VCO_RATE_8100MHz,
	.max_rate = DP_VCO_RATE_10800MHz,
	.c = {
		.dbg_name = "dp_vco_clk",
		.ops = &dp_cobalt_vco_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dp_vco_clk.c),
	},
};

static struct div_clk hsclk_divsel_clk_src = {
	.data = {
		.min_div = 2,
		.max_div = 3,
	},
	.ops = &hsclk_divsel_ops,
	.c = {
		.parent = &dp_vco_clk.c,
		.dbg_name = "hsclk_divsel_clk_src",
		.ops = &clk_ops_hsclk_divsel_clk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(hsclk_divsel_clk_src.c),
	},
};

static struct div_clk dp_link_2x_clk_divsel_five = {
	.data = {
		.div = 5,
		.min_div = 5,
		.max_div = 5,
	},
	.ops = &link2xclk_divsel_ops,
	.c = {
		.parent = &hsclk_divsel_clk_src.c,
		.dbg_name = "dp_link_2x_clk_divsel_five",
		.ops = &clk_ops_link_2x_clk_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dp_link_2x_clk_divsel_five.c),
	},
};

static struct div_clk dp_link_2x_clk_divsel_ten = {
	.data = {
		.div = 10,
		.min_div = 10,
		.max_div = 10,
	},
	.ops = &link2xclk_divsel_ops,
	.c = {
		.parent = &hsclk_divsel_clk_src.c,
		.dbg_name = "dp_link_2x_clk_divsel_ten",
		.ops = &clk_ops_link_2x_clk_div_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dp_link_2x_clk_divsel_ten.c),
	},
};

static struct mux_clk dp_link_2x_clk_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dp_link_2x_clk_divsel_five.c, 0},
		{&dp_link_2x_clk_divsel_ten.c, 1},
	},
	.ops = &mdss_mux_ops,
	.c = {
		.parent = &dp_link_2x_clk_divsel_five.c,
		.dbg_name = "dp_link_2x_clk_mux",
		.ops = &clk_ops_gen_mux_dp,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dp_link_2x_clk_mux.c),
	}
};

static struct div_clk vco_divided_clk_src = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.ops = &vco_divided_clk_ops,
	.c = {
		.parent = &hsclk_divsel_clk_src.c,
		.dbg_name = "vco_divided_clk",
		.ops = &clk_ops_vco_divided_clk_src_c,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(vco_divided_clk_src.c),
	},
};

static struct clk_lookup dp_pllcc_cobalt[] = {
	CLK_LIST(dp_vco_clk),
	CLK_LIST(hsclk_divsel_clk_src),
	CLK_LIST(dp_link_2x_clk_divsel_five),
	CLK_LIST(dp_link_2x_clk_divsel_ten),
	CLK_LIST(dp_link_2x_clk_mux),
	CLK_LIST(vco_divided_clk_src),
};

int dp_pll_clock_register_cobalt(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP;

	if (!pll_res || !pll_res->pll_base || !pll_res->phy_base) {
		DEV_ERR("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	/* Set client data for vco, mux and div clocks */
	dp_vco_clk.priv = pll_res;
	hsclk_divsel_clk_src.priv = pll_res;
	dp_link_2x_clk_mux.priv = pll_res;
	vco_divided_clk_src.priv = pll_res;
	dp_link_2x_clk_divsel_five.priv = pll_res;
	dp_link_2x_clk_divsel_ten.priv = pll_res;

	clk_ops_gen_mux_dp = clk_ops_gen_mux;
	clk_ops_gen_mux_dp.round_rate = parent_round_rate;
	clk_ops_gen_mux_dp.set_rate = parent_set_rate;

	clk_ops_hsclk_divsel_clk_src_c = clk_ops_div;
	clk_ops_hsclk_divsel_clk_src_c.prepare = mdss_pll_div_prepare;

	clk_ops_link_2x_clk_div_c = clk_ops_div;
	clk_ops_link_2x_clk_div_c.prepare = mdss_pll_div_prepare;

	/*
	 * Set the ops for the divider in the pixel clock tree to the
	 * slave_div to ensure that a set rate on this divider clock will not
	 * be propagated to it's parent. This is needed ensure that when we set
	 * the rate for pixel clock, the vco is not reconfigured
	 */
	clk_ops_vco_divided_clk_src_c = clk_ops_slave_div;
	clk_ops_vco_divided_clk_src_c.prepare = mdss_pll_div_prepare;

	/* We can select different clock ops for future versions */
	dp_vco_clk.c.ops = &dp_cobalt_vco_clk_ops;

	rc = of_msm_clock_register(pdev->dev.of_node, dp_pllcc_cobalt,
					ARRAY_SIZE(dp_pllcc_cobalt));
	if (rc) {
		DEV_ERR("%s: Clock register failed rc=%d\n", __func__, rc);
		rc = -EPROBE_DEFER;
	} else {
		DEV_DBG("%s SUCCESS\n", __func__);
	}

	return rc;
}
