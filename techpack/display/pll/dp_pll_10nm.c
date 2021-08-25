// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

/*
 * Display Port PLL driver block diagram for branch clocks
 *
 *		+------------------------------+
 *		|         DP_VCO_CLK           |
 *		|                              |
 *		|    +-------------------+     |
 *		|    |   (DP PLL/VCO)    |     |
 *		|    +---------+---------+     |
 *		|              v               |
 *		|   +----------+-----------+   |
 *		|   | hsclk_divsel_clk_src |   |
 *		|   +----------+-----------+   |
 *		+------------------------------+
 *				|
 *	 +------------<---------v------------>----------+
 *	 |                                              |
 * +-----v------------+                                 |
 * | dp_link_clk_src  |                                 |
 * |    divsel_ten    |                                 |
 * +---------+--------+                                 |
 *	|                                               |
 *	|                                               |
 *	v                                               v
 * Input to DISPCC block                                |
 * for link clk, crypto clk                             |
 * and interface clock                                  |
 *							|
 *							|
 *	+--------<------------+-----------------+---<---+
 *	|                     |                 |
 * +-------v------+  +--------v-----+  +--------v------+
 * | vco_divided  |  | vco_divided  |  | vco_divided   |
 * |    _clk_src  |  |    _clk_src  |  |    _clk_src   |
 * |              |  |              |  |               |
 * |divsel_six    |  |  divsel_two  |  |  divsel_four  |
 * +-------+------+  +-----+--------+  +--------+------+
 *         |	           |		        |
 *	v------->----------v-------------<------v
 *                         |
 *		+----------+---------+
 *		|   vco_divided_clk  |
 *		|       _src_mux     |
 *		+---------+----------+
 *                        |
 *                        v
 *              Input to DISPCC block
 *              for DP pixel clock
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <dt-bindings/clock/mdss-10nm-pll-clk.h>

#include "pll_drv.h"
#include "dp_pll.h"
#include "dp_pll_10nm.h"

static struct dp_pll_db dp_pdb;
static struct clk_ops mux_clk_ops;

static struct regmap_config dp_pll_10nm_cfg = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register = 0x910,
};

static struct regmap_bus dp_pixel_mux_regmap_ops = {
	.reg_write = dp_mux_set_parent_10nm,
	.reg_read = dp_mux_get_parent_10nm,
};

/* Op structures */
static const struct clk_ops dp_10nm_vco_clk_ops = {
	.recalc_rate = dp_vco_recalc_rate_10nm,
	.set_rate = dp_vco_set_rate_10nm,
	.round_rate = dp_vco_round_rate_10nm,
	.prepare = dp_vco_prepare_10nm,
	.unprepare = dp_vco_unprepare_10nm,
};

static struct dp_pll_vco_clk dp_vco_clk = {
	.min_rate = DP_VCO_HSCLK_RATE_1620MHZDIV1000,
	.max_rate = DP_VCO_HSCLK_RATE_8100MHZDIV1000,
	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_clk",
		.parent_names = (const char *[]){ "xo_board" },
		.num_parents = 1,
		.ops = &dp_10nm_vco_clk_ops,
	},
};

static struct clk_fixed_factor dp_phy_pll_link_clk = {
	.div = 10,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_phy_pll_link_clk",
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

static struct clk_fixed_factor dp_vco_divsel_six_clk_src = {
	.div = 6,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dp_vco_divsel_six_clk_src",
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

	if (vco->rate == DP_VCO_HSCLK_RATE_8100MHZDIV1000)
		return (vco->rate / 6);
	else if (vco->rate == DP_VCO_HSCLK_RATE_5400MHZDIV1000)
		return (vco->rate / 4);
	else
		return (vco->rate / 2);
}

static struct clk_regmap_mux dp_phy_pll_vco_div_clk = {
	.reg = 0x64,
	.shift = 0,
	.width = 2,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dp_phy_pll_vco_div_clk",
			.parent_names =
				(const char *[]){"dp_vco_divsel_two_clk_src",
					"dp_vco_divsel_four_clk_src",
					"dp_vco_divsel_six_clk_src"},
			.num_parents = 3,
			.ops = &mux_clk_ops,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		},
	},
};

static struct clk_hw *mdss_dp_pllcc_10nm[] = {
	[DP_VCO_CLK] = &dp_vco_clk.hw,
	[DP_PHY_PLL_LINK_CLK] = &dp_phy_pll_link_clk.hw,
	[DP_VCO_DIVIDED_TWO_CLK_SRC] = &dp_vco_divsel_two_clk_src.hw,
	[DP_VCO_DIVIDED_FOUR_CLK_SRC] = &dp_vco_divsel_four_clk_src.hw,
	[DP_VCO_DIVIDED_SIX_CLK_SRC] = &dp_vco_divsel_six_clk_src.hw,
	[DP_PHY_PLL_VCO_DIV_CLK] = &dp_phy_pll_vco_div_clk.clkr.hw,
};

int dp_pll_clock_register_10nm(struct platform_device *pdev,
				 struct mdss_pll_resources *pll_res)
{
	int rc = -ENOTSUPP, i = 0;
	struct clk_onecell_data *clk_data;
	struct clk *clk;
	struct regmap *regmap;
	int num_clks = ARRAY_SIZE(mdss_dp_pllcc_10nm);

	clk_data = devm_kzalloc(&pdev->dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kcalloc(&pdev->dev, num_clks,
				sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = num_clks;

	pll_res->priv = &dp_pdb;
	dp_pdb.pll = pll_res;

	/* Set client data for vco, mux and div clocks */
	regmap = devm_regmap_init(&pdev->dev, &dp_pixel_mux_regmap_ops,
			pll_res, &dp_pll_10nm_cfg);
	dp_phy_pll_vco_div_clk.clkr.regmap = regmap;
	mux_clk_ops = clk_regmap_mux_closest_ops;
	mux_clk_ops.determine_rate = clk_mux_determine_rate;
	mux_clk_ops.recalc_rate = mux_recalc_rate;

	dp_vco_clk.priv = pll_res;

	for (i = DP_VCO_CLK; i <= DP_PHY_PLL_VCO_DIV_CLK; i++) {
		pr_debug("reg clk: %d index: %d\n", i, pll_res->index);
		clk = devm_clk_register(&pdev->dev,
				mdss_dp_pllcc_10nm[i]);
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
		pr_err("%s: Clock register failed rc=%d\n", __func__, rc);
		rc = -EPROBE_DEFER;
	} else {
		pr_debug("%s SUCCESS\n", __func__);
	}
	return 0;
clk_reg_fail:
	return rc;
}
