// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include "pll_drv.h"
#include "dsi_pll.h"
#include "dsi_pll_14nm.h"
#include <dt-bindings/clock/mdss-14nm-pll-clk.h>

#define VCO_DELAY_USEC		1

static struct dsi_pll_db pll_db[DSI_PLL_NUM];

static struct regmap_config dsi_pll_14nm_config = {
	.reg_bits	= 32,
	.reg_stride	= 4,
	.val_bits	= 32,
	.max_register = 0x588,
};

static struct regmap_bus post_n1_div_regmap_bus = {
	.reg_write = post_n1_div_set_div,
	.reg_read = post_n1_div_get_div,
};

static struct regmap_bus n2_div_regmap_bus = {
	.reg_write = n2_div_set_div,
	.reg_read = n2_div_get_div,
};

static struct regmap_bus shadow_n2_div_regmap_bus = {
	.reg_write = shadow_n2_div_set_div,
	.reg_read = n2_div_get_div,
};

static struct regmap_bus dsi_mux_regmap_bus = {
	.reg_write = dsi_mux_set_parent_14nm,
	.reg_read = dsi_mux_get_parent_14nm,
};

/* Op structures */
static const struct clk_ops clk_ops_dsi_vco = {
	.recalc_rate = pll_vco_recalc_rate_14nm,
	.set_rate = pll_vco_set_rate_14nm,
	.round_rate = pll_vco_round_rate_14nm,
	.prepare = pll_vco_prepare_14nm,
	.unprepare = pll_vco_unprepare_14nm,
};

/* Shadow ops for dynamic refresh */
static const struct clk_ops clk_ops_shadow_dsi_vco = {
	.recalc_rate = pll_vco_recalc_rate_14nm,
	.set_rate = shadow_pll_vco_set_rate_14nm,
	.round_rate = pll_vco_round_rate_14nm,
};

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1300000000UL,
	.max_rate = 2600000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_14nm,
	.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_vco_clk_14nm",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_ops_dsi_vco,
		},
};

static struct dsi_pll_vco_clk dsi0pll_shadow_vco_clk = {
	.ref_clk_rate = 19200000u,
	.min_rate = 1300000000u,
	.max_rate = 2600000000u,
	.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_vco_clk_14nm",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_ops_shadow_dsi_vco,
		},
};

static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1300000000UL,
	.max_rate = 2600000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_14nm,
	.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_vco_clk_14nm",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_ops_dsi_vco,
		},
};

static struct dsi_pll_vco_clk dsi1pll_shadow_vco_clk = {
	.ref_clk_rate = 19200000u,
	.min_rate = 1300000000u,
	.max_rate = 2600000000u,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_14nm,
	.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_vco_clk_14nm",
			.parent_names = (const char *[]){ "bi_tcxo" },
			.num_parents = 1,
			.ops = &clk_ops_shadow_dsi_vco,
		},
};

static struct clk_regmap_div dsi0pll_post_n1_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_post_n1_div_clk",
			.parent_names =
				(const char *[]){ "dsi0pll_vco_clk_14nm" },
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_shadow_post_n1_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_post_n1_div_clk",
			.parent_names =
				(const char *[]){"dsi0pll_shadow_vco_clk_14nm"},
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_post_n1_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_post_n1_div_clk",
			.parent_names =
				(const char *[]){ "dsi1pll_vco_clk_14nm" },
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_shadow_post_n1_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_post_n1_div_clk",
			.parent_names =
				(const char *[]){"dsi1pll_shadow_vco_clk_14nm"},
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_n2_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_n2_div_clk",
			.parent_names =
				(const char *[]){ "dsi0pll_post_n1_div_clk" },
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_shadow_n2_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_shadow_n2_div_clk",
			.parent_names =
			(const char *[]){ "dsi0pll_shadow_post_n1_div_clk" },
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_n2_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_n2_div_clk",
			.parent_names =
				(const char *[]){ "dsi1pll_post_n1_div_clk" },
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_shadow_n2_div_clk = {
	.reg = 0x48,
	.shift = 0,
	.width = 4,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_shadow_n2_div_clk",
			.parent_names =
			(const char *[]){ "dsi1pll_shadow_post_n1_div_clk" },
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_fixed_factor dsi0pll_pixel_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_pixel_clk_src",
		.parent_names = (const char *[]){ "dsi0pll_n2_div_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_shadow_pixel_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_shadow_pixel_clk_src",
		.parent_names = (const char *[]){ "dsi0pll_shadow_n2_div_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_pixel_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_pixel_clk_src",
		.parent_names = (const char *[]){ "dsi1pll_n2_div_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_shadow_pixel_clk_src = {
	.div = 2,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_shadow_pixel_clk_src",
		.parent_names = (const char *[]){ "dsi1pll_shadow_n2_div_clk" },
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi0pll_pixel_clk_mux = {
	.reg = 0x48,
	.shift = 0,
	.width = 1,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0_phy_pll_out_dsiclk",
			.parent_names =
				(const char *[]){ "dsi0pll_pixel_clk_src",
					"dsi0pll_shadow_pixel_clk_src"},
			.num_parents = 2,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_pixel_clk_mux = {
	.reg = 0x48,
	.shift = 0,
	.width = 1,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_pixel_clk_mux",
			.parent_names =
				(const char *[]){ "dsi1pll_pixel_clk_src",
					"dsi1pll_shadow_pixel_clk_src"},
			.num_parents = 2,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_fixed_factor dsi0pll_byte_clk_src = {
	.div = 8,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_byte_clk_src",
		.parent_names = (const char *[]){ "dsi0pll_post_n1_div_clk" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_shadow_byte_clk_src = {
	.div = 8,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_shadow_byte_clk_src",
		.parent_names =
			(const char *[]){ "dsi0pll_shadow_post_n1_div_clk" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_byte_clk_src = {
	.div = 8,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_byte_clk_src",
		.parent_names = (const char *[]){ "dsi1pll_post_n1_div_clk" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_shadow_byte_clk_src = {
	.div = 8,
	.mult = 1,

	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_shadow_byte_clk_src",
		.parent_names =
			(const char *[]){ "dsi1pll_shadow_post_n1_div_clk" },
		.num_parents = 1,
		.flags = (CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi0pll_byte_clk_mux = {
	.reg = 0x48,
	.shift = 0,
	.width = 1,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0_phy_pll_out_byteclk",
			.parent_names =
				(const char *[]){"dsi0pll_byte_clk_src",
					"dsi0pll_shadow_byte_clk_src"},
			.num_parents = 2,
			.ops = &clk_regmap_mux_closest_ops,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
		},
	},
};

static struct clk_regmap_mux dsi1pll_byte_clk_mux = {
	.reg = 0x48,
	.shift = 0,
	.width = 1,

	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_byte_clk_mux",
			.parent_names =
				(const char *[]){"dsi1pll_byte_clk_src",
					"dsi1pll_shadow_byte_clk_src"},
			.num_parents = 2,
			.ops = &clk_regmap_mux_closest_ops,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT |
					CLK_SET_RATE_NO_REPARENT),
		},
	},
};

static struct clk_hw *mdss_dsi_pllcc_14nm[] = {
	[BYTE0_MUX_CLK] = &dsi0pll_byte_clk_mux.clkr.hw,
	[BYTE0_SRC_CLK] = &dsi0pll_byte_clk_src.hw,
	[PIX0_MUX_CLK] = &dsi0pll_pixel_clk_mux.clkr.hw,
	[PIX0_SRC_CLK] = &dsi0pll_pixel_clk_src.hw,
	[N2_DIV_0_CLK] = &dsi0pll_n2_div_clk.clkr.hw,
	[POST_N1_DIV_0_CLK] = &dsi0pll_post_n1_div_clk.clkr.hw,
	[VCO_CLK_0_CLK] = &dsi0pll_vco_clk.hw,
	[SHADOW_BYTE0_SRC_CLK] = &dsi0pll_shadow_byte_clk_src.hw,
	[SHADOW_PIX0_SRC_CLK] = &dsi0pll_shadow_pixel_clk_src.hw,
	[SHADOW_N2_DIV_0_CLK] = &dsi0pll_shadow_n2_div_clk.clkr.hw,
	[SHADOW_POST_N1_DIV_0_CLK] = &dsi0pll_shadow_post_n1_div_clk.clkr.hw,
	[SHADOW_VCO_CLK_0_CLK] = &dsi0pll_shadow_vco_clk.hw,
	[BYTE1_MUX_CLK] = &dsi1pll_byte_clk_mux.clkr.hw,
	[BYTE1_SRC_CLK] = &dsi1pll_byte_clk_src.hw,
	[PIX1_MUX_CLK] = &dsi1pll_pixel_clk_mux.clkr.hw,
	[PIX1_SRC_CLK] = &dsi1pll_pixel_clk_src.hw,
	[N2_DIV_1_CLK] = &dsi1pll_n2_div_clk.clkr.hw,
	[POST_N1_DIV_1_CLK] = &dsi1pll_post_n1_div_clk.clkr.hw,
	[VCO_CLK_1_CLK] = &dsi1pll_vco_clk.hw,
	[SHADOW_BYTE1_SRC_CLK] = &dsi1pll_shadow_byte_clk_src.hw,
	[SHADOW_PIX1_SRC_CLK] = &dsi1pll_shadow_pixel_clk_src.hw,
	[SHADOW_N2_DIV_1_CLK] = &dsi1pll_shadow_n2_div_clk.clkr.hw,
	[SHADOW_POST_N1_DIV_1_CLK] = &dsi1pll_shadow_post_n1_div_clk.clkr.hw,
	[SHADOW_VCO_CLK_1_CLK] = &dsi1pll_shadow_vco_clk.hw,
};

int dsi_pll_clock_register_14nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = 0, ndx, i;
	int const ssc_freq_default = 31500; /* default h/w recommended value */
	int const ssc_ppm_default = 5000; /* default h/w recommended value */
	struct dsi_pll_db *pdb;
	struct clk_onecell_data *clk_data;
	struct clk *clk;
	struct regmap *regmap;
	int num_clks = ARRAY_SIZE(mdss_dsi_pllcc_14nm);

	if (pll_res->index >= DSI_PLL_NUM) {
		pr_err("pll ndx=%d is NOT supported\n", pll_res->index);
		return -EINVAL;
	}

	ndx = pll_res->index;
	pdb = &pll_db[ndx];
	pll_res->priv = pdb;
	pdb->pll = pll_res;
	ndx++;
	ndx %= DSI_PLL_NUM;
	pdb->next = &pll_db[ndx];

	if (pll_res->ssc_en) {
		if (!pll_res->ssc_freq)
			pll_res->ssc_freq = ssc_freq_default;
		if (!pll_res->ssc_ppm)
			pll_res->ssc_ppm = ssc_ppm_default;
	}

	clk_data = devm_kzalloc(&pdev->dev, sizeof(*clk_data), GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kcalloc(&pdev->dev, num_clks,
				sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = num_clks;

	/* Set client data to mux, div and vco clocks.  */
	if (pll_res->index == DSI_PLL_1) {
		regmap = devm_regmap_init(&pdev->dev, &post_n1_div_regmap_bus,
					pll_res, &dsi_pll_14nm_config);
		dsi1pll_post_n1_div_clk.clkr.regmap = regmap;
		dsi1pll_shadow_post_n1_div_clk.clkr.regmap = regmap;

		regmap = devm_regmap_init(&pdev->dev, &n2_div_regmap_bus,
				pll_res, &dsi_pll_14nm_config);
		dsi1pll_n2_div_clk.clkr.regmap = regmap;

		regmap = devm_regmap_init(&pdev->dev, &shadow_n2_div_regmap_bus,
				pll_res, &dsi_pll_14nm_config);
		dsi1pll_shadow_n2_div_clk.clkr.regmap = regmap;

		regmap = devm_regmap_init(&pdev->dev, &dsi_mux_regmap_bus,
				pll_res, &dsi_pll_14nm_config);
		dsi1pll_byte_clk_mux.clkr.regmap = regmap;
		dsi1pll_pixel_clk_mux.clkr.regmap = regmap;

		dsi1pll_vco_clk.priv = pll_res;
		dsi1pll_shadow_vco_clk.priv = pll_res;

		pll_res->vco_delay = VCO_DELAY_USEC;

		for (i = BYTE1_MUX_CLK; i <= SHADOW_VCO_CLK_1_CLK; i++) {
			pr_debug("register clk: %d index: %d\n",
							i, pll_res->index);
			clk = devm_clk_register(&pdev->dev,
					mdss_dsi_pllcc_14nm[i]);
			if (IS_ERR(clk)) {
				pr_err("clk registration failed for DSI: %d\n",
						pll_res->index);
				rc = -EINVAL;
				goto clk_reg_fail;
			}
			clk_data->clks[i] = clk;
		}

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	} else {
		regmap = devm_regmap_init(&pdev->dev, &post_n1_div_regmap_bus,
					pll_res, &dsi_pll_14nm_config);
		dsi0pll_post_n1_div_clk.clkr.regmap = regmap;
		dsi0pll_shadow_post_n1_div_clk.clkr.regmap = regmap;

		regmap = devm_regmap_init(&pdev->dev, &n2_div_regmap_bus,
				pll_res, &dsi_pll_14nm_config);
		dsi0pll_n2_div_clk.clkr.regmap = regmap;

		regmap = devm_regmap_init(&pdev->dev, &shadow_n2_div_regmap_bus,
				pll_res, &dsi_pll_14nm_config);
		dsi0pll_shadow_n2_div_clk.clkr.regmap = regmap;

		regmap = devm_regmap_init(&pdev->dev, &dsi_mux_regmap_bus,
				pll_res, &dsi_pll_14nm_config);
		dsi0pll_byte_clk_mux.clkr.regmap = regmap;
		dsi0pll_pixel_clk_mux.clkr.regmap = regmap;

		dsi0pll_vco_clk.priv = pll_res;
		dsi0pll_shadow_vco_clk.priv = pll_res;
		pll_res->vco_delay = VCO_DELAY_USEC;

		for (i = BYTE0_MUX_CLK; i <= SHADOW_VCO_CLK_0_CLK; i++) {
			pr_debug("reg clk: %d index: %d\n", i, pll_res->index);
			clk = devm_clk_register(&pdev->dev,
					mdss_dsi_pllcc_14nm[i]);
			if (IS_ERR(clk)) {
				pr_err("clk registration failed for DSI: %d\n",
						pll_res->index);
				rc = -EINVAL;
				goto clk_reg_fail;
			}
			clk_data->clks[i] = clk;
		}

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	}

	if (!rc) {
		pr_info("Registered DSI PLL ndx=%d clocks successfully\n",
						pll_res->index);
		return rc;
	}

clk_reg_fail:
	return rc;
}
