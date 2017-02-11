/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

			+--------------------------+
			|       DP_VCO_CLK         |
			|			   |
			|  +-------------------+   |
			|  |   (DP PLL/VCO)    |   |
			|  +---------+---------+   |
			|	     v		   |
			| +----------+-----------+ |
			| | hsclk_divsel_clk_src | |
			| +----------+-----------+ |
			+--------------------------+
				     |
				     v
	   +------------<------------|------------>-------------+
	   |                         |                          |
+----------v----------+	  +----------v----------+    +----------v----------+
|   dp_link_2x_clk    |	  | vco_divided_clk_src	|    | vco_divided_clk_src |
|     divsel_five     |	  |			|    |			   |
v----------+----------v	  |	divsel_two	|    |	   divsel_four	   |
	   |		  +----------+----------+    +----------+----------+
	   |                         |                          |
	   v			     v				v
				     |	+---------------------+	|
  Input to MMSSCC block		     |	|    (aux_clk_ops)    |	|
  for link clk, crypto clk	     +-->   vco_divided_clk   <-+
  and interface clock			|	_src_mux      |
					+----------+----------+
						   |
						   v
					 Input to MMSSCC block
					 for DP pixel clock

******************************************************************************
*/

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>

#include "mdss-pll.h"
#include "mdss-dp-pll.h"
#include "mdss-dp-pll-14nm.h"

static struct dp_pll_db dp_pdb;
static struct clk_ops mux_clk_ops;

static struct regmap_config dp_pll_14nm_cfg = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register = 0x910,
};

static struct regmap_bus dp_pixel_mux_regmap_ops = {
	.reg_write = dp_mux_set_parent_14nm,
	.reg_read = dp_mux_get_parent_14nm,
};

/* Op structures */
static struct clk_ops dp_14nm_vco_clk_ops = {
	.recalc_rate = dp_vco_recalc_rate_14nm,
	.set_rate = dp_vco_set_rate_14nm,
	.round_rate = dp_vco_round_rate_14nm,
	.prepare = dp_vco_prepare_14nm,
	.unprepare = dp_vco_unprepare_14nm,
};

static struct dp_pll_vco_clk dp_vco_clk = {
	.min_rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000,
	.max_rate = DP_VCO_HSCLK_RATE_5400MHZDIV1000,
	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_clk",
		.parent_names = (const char *[]){ "xo_board" },
		.num_parents = 1,
		.ops = &dp_14nm_vco_clk_ops,
	},
};

static struct clk_fixed_factor dp_link_2x_clk_divsel_five = {
	.div = 5,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_link_2x_clk_divsel_five",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dp_vco_divsel_two_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_two_clk_src",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dp_vco_divsel_four_clk_src = {
	.div = 4,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_four_clk_src",
		.parent_names =
			(const char *[]){ "dp_vco_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE),
		.ops = &clk_fixed_factor_ops,
	},
};

static int clk_mux_determine_rate(struct clk_hw *hw,
				     struct clk_rate_request *req)
{
	int ret = 0;

	ret = __clk_mux_determine_rate_closest(hw, req);
	if (ret)
		return ret;

	/* Set the new parent of mux if there is a new valid parent */
	if (hw->clk && req->best_parent_hw->clk)
		clk_set_parent(hw->clk, req->best_parent_hw->clk);

	return 0;
}


static unsigned long mux_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct clk *div_clk = NULL, *vco_clk = NULL;
	struct dp_pll_vco_clk *vco = NULL;

	div_clk = clk_get_parent(hw->clk);
	if (!div_clk)
		return 0;

	vco_clk = clk_get_parent(div_clk);
	if (!vco_clk)
		return 0;

	vco = to_dp_vco_hw(__clk_get_hw(vco_clk));
	if (!vco)
		return 0;

	if (vco->rate == DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		return (vco->rate / 4);
	else
		return (vco->rate / 2);
}

static struct clk_regmap_mux dp_vco_divided_clk_src_mux = {
	.reg = 0x64,
	.shift = 0,
	.width = 1,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dp_vco_divided_clk_src_mux",
			.parent_names =
				(const char *[]){"dp_vco_divsel_two_clk_src",
					"dp_vco_divsel_four_clk_src"},
			.num_parents = 2,
			.ops = &mux_clk_ops,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		},
	},
};

static struct clk_hw *mdss_dp_pllcc_14nm[] = {
	[DP_VCO_CLK] = &dp_vco_clk.hw,
	[DP_LINK_2X_CLK_DIVSEL_FIVE] = &dp_link_2x_clk_divsel_five.hw,
	[DP_VCO_DIVSEL_FOUR_CLK_SRC] = &dp_vco_divsel_four_clk_src.hw,
	[DP_VCO_DIVSEL_TWO_CLK_SRC] = &dp_vco_divsel_two_clk_src.hw,
	[DP_VCO_DIVIDED_CLK_SRC_MUX] = &dp_vco_divided_clk_src_mux.clkr.hw,
};

int dp_pll_clock_register_14nm(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP, i = 0;
	struct clk_onecell_data *clk_data;
	struct clk *clk;
	struct regmap *regmap;
	int num_clks = ARRAY_SIZE(mdss_dp_pllcc_14nm);

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	if (!pll_res || !pll_res->pll_base || !pll_res->phy_base) {
		DEV_ERR("%s: Invalid input parameters\n", __func__);
		return -EINVAL;
	}

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
					GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
				sizeof(struct clk *)), GFP_KERNEL);
	if (!clk_data->clks) {
		devm_kfree(&pdev->dev, clk_data);
		return -ENOMEM;
	}
	clk_data->clk_num = num_clks;

	pll_res->priv = &dp_pdb;
	dp_pdb.pll = pll_res;

	/* Set client data for vco, mux and div clocks */
	regmap = devm_regmap_init(&pdev->dev, &dp_pixel_mux_regmap_ops,
			pll_res, &dp_pll_14nm_cfg);
	dp_vco_divided_clk_src_mux.clkr.regmap = regmap;
	mux_clk_ops = clk_regmap_mux_closest_ops;
	mux_clk_ops.determine_rate = clk_mux_determine_rate;
	mux_clk_ops.recalc_rate = mux_recalc_rate;

	dp_vco_clk.priv = pll_res;

	for (i = DP_VCO_CLK; i <= DP_VCO_DIVIDED_CLK_SRC_MUX; i++) {
		pr_debug("reg clk: %d index: %d\n", i, pll_res->index);
		clk = devm_clk_register(&pdev->dev,
				mdss_dp_pllcc_14nm[i]);
		if (IS_ERR(clk)) {
			pr_err("clk registration failed for DP: %d\n",
					pll_res->index);
			rc = -EINVAL;
			goto clk_reg_fail;
		}
		clk_data->clks[i] = clk;
	}

	rc = of_clk_add_provider(pdev->dev.of_node,
			of_clk_src_onecell_get, clk_data);
	if (rc) {
		DEV_ERR("%s: Clock register failed rc=%d\n", __func__, rc);
		rc = -EPROBE_DEFER;
	} else {
		DEV_DBG("%s SUCCESS\n", __func__);
	}
	return 0;
clk_reg_fail:
	devm_kfree(&pdev->dev, clk_data->clks);
	devm_kfree(&pdev->dev, clk_data);
	return rc;
}
