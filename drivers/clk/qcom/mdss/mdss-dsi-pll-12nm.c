// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020-2021, The Linux Foundation. All rights reserved. */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <linux/delay.h>
#include "mdss-dsi-pll.h"
#include "mdss-pll.h"
#include <dt-bindings/clock/mdss-12nm-pll-clk.h>
#include "mdss-dsi-pll-12nm.h"

#define VCO_DELAY_USEC 1

static struct regmap_config dsi_pll_12nm_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x800,
};

static const struct clk_ops clk_ops_vco_12nm = {
	.recalc_rate = vco_12nm_recalc_rate,
	.set_rate = pll_vco_set_rate_12nm,
	.round_rate = pll_vco_round_rate_12nm,
	.prepare = pll_vco_prepare_12nm,
	.unprepare = pll_vco_unprepare_12nm,
	.enable = pll_vco_enable_12nm,
};

static struct regmap_bus pclk_div_regmap_bus = {
	.reg_write = pixel_div_set_div,
	.reg_read  = pixel_div_get_div,
};

static struct regmap_bus post_div_mux_regmap_bus = {
	.reg_write = set_post_div_mux_sel,
	.reg_read  = get_post_div_mux_sel,
};

static struct regmap_bus gp_div_mux_regmap_bus = {
	.reg_write = set_gp_mux_sel,
	.reg_read  = get_gp_mux_sel,
};

/*
 * Clock tree model for generating DSI byte clock and pclk for 12nm DSI PLL
 *
 *
 *                                          +---------------+
 *                               +----------|    vco_clk    |----------+
 *                               |          +---------------+          |
 *                               |                                     |
 *                               |                                     |
 *                               |                                     |
 *      +---------+---------+----+----+---------+---------+            |
 *      |         |         |         |         |         |            |
 *      |         |         |         |         |         |            |
 *      |         |         |         |         |         |            |
 *  +---v---+ +---v---+ +---v---+ +---v---+ +---v---+ +---v---+        |
 *  | DIV(1)| | DIV(2)| | DIV(4)| | DIV(8)| |DIV(16)| |DIV(32)|        |
 *  +---+---+ +---+---+ +---+---+ +---+---+ +---+---+ +---+---+        |
 *      |         |         |         |         |         |            |
 *      |         |         +---+ +---+         |         |            |
 *      |         +-----------+ | | +-----------+         |            |
 *      +-------------------+ | | | | +-------------------+            |
 *                          | | | | | |                                |
 *                       +--v-v-v-v-v-v---+                            |
 *                        \ post_div_mux /                             |
 *                         \            /                              |
 *                          +-----+----+         +---------------------+
 *                                |              |
 *       +------------------------+              |
 *       |                                       |
 *  +----v----+         +---------+---------+----+----+---------+---------+
 *  |  DIV-4  |         |         |         |         |         |         |
 *  +----+----+         |         |         |         |         |         |
 *       |          +---v---+ +---v---+ +---v---+ +---v---+ +---v---+ +---v---+
 *       |          | DIV(1)| | DIV(2)| | DIV(4)| | DIV(8)| |DIV(16)| |DIV(32)|
 *       |          +---+---+ +---+---+ +---+---+ +---+---+ +---+---+ +---+---+
 *       |              |         |         |         |         |         |
 *       v              |         |         +---+ +---+         |         |
 *  byte_clk_src        |         +-----------+ | | +-----------+         |
 *                      +-------------------+ | | | | +-------------------+
 *                                          | | | | | |
 *                                       +--v-v-v-v-v-v---+
 *                                        \ gp_cntrl_mux /
 *                                         \            /
 *                                          +-----+----+
 *                                                |
 *                                                |
 *                                        +-------v-------+
 *                                        |   (DIV + 1)   |
 *                                        | DIV = 0...127 |
 *                                        +-------+-------+
 *                                                |
 *                                                |
 *                                                v
 *                              dsi_pclk input to Clock Controller MND
 */

static struct dsi_pll_db pll_db[DSI_PLL_MAX];

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 2000000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_12nm,
	.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_vco_clk",
			.parent_names = (const char *[]){"bi_tcxo"},
			.num_parents = 1,
			.ops = &clk_ops_vco_12nm,
			.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 2000000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_12nm,
	.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_vco_clk",
			.parent_names = (const char *[]){"bi_tcxo"},
			.num_parents = 1,
			.ops = &clk_ops_vco_12nm,
			.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor dsi0pll_post_div1 = {
	.div = 1,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_div1",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_post_div2 = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_div2",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_post_div4 = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_div4",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_post_div8 = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_div8",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_post_div16 = {
	.div = 16,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_div16",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_post_div32 = {
	.div = 32,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_post_div32",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi0pll_post_div_mux = {
	.reg = DSIPHY_PLL_VCO_CTRL,
	.shift = 0,
	.width = 3,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_post_div_mux",
			.parent_names = (const char *[]){"dsi0pll_post_div1",
					"dsi0pll_post_div2",
					"dsi0pll_post_div4",
					"dsi0pll_post_div8",
					"dsi0pll_post_div16",
					"dsi0pll_post_div32"},
			.num_parents = 6,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_fixed_factor dsi1pll_post_div1 = {
	.div = 1,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_div1",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_div2 = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_div2",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_div4 = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_div4",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_div8 = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_div8",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_div16 = {
	.div = 16,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_div16",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_post_div32 = {
	.div = 32,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_post_div32",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi1pll_post_div_mux = {
	.reg = DSIPHY_PLL_VCO_CTRL,
	.shift = 0,
	.width = 3,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_post_div_mux",
			.parent_names = (const char *[]){"dsi1pll_post_div1",
					"dsi1pll_post_div2",
					"dsi1pll_post_div4",
					"dsi1pll_post_div8",
					"dsi1pll_post_div16",
					"dsi1pll_post_div32"},
			.num_parents = 6,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_fixed_factor dsi0pll_gp_div1 = {
	.div = 1,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_gp_div1",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_gp_div2 = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_gp_div2",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_gp_div4 = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_gp_div4",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_gp_div8 = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_gp_div8",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_gp_div16 = {
	.div = 16,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_gp_div16",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi0pll_gp_div32 = {
	.div = 32,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_gp_div32",
		.parent_names = (const char *[]){"dsi0pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi0pll_gp_div_mux = {
	.reg = DSIPHY_PLL_CTRL,
	.shift = 0,
	.width = 3,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_gp_div_mux",
			.parent_names = (const char *[]){"dsi0pll_gp_div1",
					"dsi0pll_gp_div2",
					"dsi0pll_gp_div4",
					"dsi0pll_gp_div8",
					"dsi0pll_gp_div16",
					"dsi0pll_gp_div32"},
			.num_parents = 6,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_fixed_factor dsi1pll_gp_div1 = {
	.div = 1,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_gp_div1",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_gp_div2 = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_gp_div2",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_gp_div4 = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_gp_div4",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_gp_div8 = {
	.div = 8,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_gp_div8",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_gp_div16 = {
	.div = 16,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_gp_div16",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_gp_div32 = {
	.div = 32,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_gp_div32",
		.parent_names = (const char *[]){"dsi1pll_vco_clk"},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi1pll_gp_div_mux = {
	.reg = DSIPHY_PLL_CTRL,
	.shift = 0,
	.width = 3,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_gp_div_mux",
			.parent_names = (const char *[]){"dsi1pll_gp_div1",
					"dsi1pll_gp_div2",
					"dsi1pll_gp_div4",
					"dsi1pll_gp_div8",
					"dsi1pll_gp_div16",
					"dsi1pll_gp_div32"},
			.num_parents = 6,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_div dsi0pll_pclk_src = {
	.reg = DSIPHY_SSC9,
	.shift = 0,
	.width = 3,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0_phy_pll_out_dsiclk",
			.parent_names = (const char *[]){
					"dsi0pll_gp_div_mux"},
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_pclk_src = {
	.reg = DSIPHY_SSC9,
	.shift = 0,
	.width = 3,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1_phy_pll_out_dsiclk",
			.parent_names = (const char *[]){
					"dsi1pll_gp_div_mux"},
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_fixed_factor dsi0pll_byte_clk_src = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0_phy_pll_out_byteclk",
		.parent_names = (const char *[]){"dsi0pll_post_div_mux"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_byte_clk_src = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1_phy_pll_out_byteclk",
		.parent_names = (const char *[]){"dsi1pll_post_div_mux"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_hw *mdss_dsi_pllcc_12nm[] = {
	[VCO_CLK_0] = &dsi0pll_vco_clk.hw,
	[POST_DIV1_0_CLK] = &dsi0pll_post_div1.hw,
	[POST_DIV2_0_CLK] = &dsi0pll_post_div2.hw,
	[POST_DIV4_0_CLK] = &dsi0pll_post_div4.hw,
	[POST_DIV8_0_CLK] = &dsi0pll_post_div8.hw,
	[POST_DIV16_0_CLK] = &dsi0pll_post_div16.hw,
	[POST_DIV32_0_CLK] = &dsi0pll_post_div32.hw,
	[POST_DIV_MUX_0_CLK] = &dsi0pll_post_div_mux.clkr.hw,
	[GP_DIV1_0_CLK] = &dsi0pll_gp_div1.hw,
	[GP_DIV2_0_CLK] = &dsi0pll_gp_div2.hw,
	[GP_DIV4_0_CLK] = &dsi0pll_gp_div4.hw,
	[GP_DIV8_0_CLK] = &dsi0pll_gp_div8.hw,
	[GP_DIV16_0_CLK] = &dsi0pll_gp_div16.hw,
	[GP_DIV32_0_CLK] = &dsi0pll_gp_div32.hw,
	[GP_DIV_MUX_0_CLK] = &dsi0pll_gp_div_mux.clkr.hw,
	[PCLK_SRC_MUX_0_CLK] = &dsi0pll_pclk_src.clkr.hw,
	[BYTE_CLK_SRC_0_CLK] = &dsi0pll_byte_clk_src.hw,
	[VCO_CLK_1] = &dsi1pll_vco_clk.hw,
	[POST_DIV1_1_CLK] = &dsi1pll_post_div1.hw,
	[POST_DIV2_1_CLK] = &dsi1pll_post_div2.hw,
	[POST_DIV4_1_CLK] = &dsi1pll_post_div4.hw,
	[POST_DIV8_1_CLK] = &dsi1pll_post_div8.hw,
	[POST_DIV16_1_CLK] = &dsi1pll_post_div16.hw,
	[POST_DIV32_1_CLK] = &dsi1pll_post_div32.hw,
	[POST_DIV_MUX_1_CLK] = &dsi1pll_post_div_mux.clkr.hw,
	[GP_DIV1_1_CLK] = &dsi1pll_gp_div1.hw,
	[GP_DIV2_1_CLK] = &dsi1pll_gp_div2.hw,
	[GP_DIV4_1_CLK] = &dsi1pll_gp_div4.hw,
	[GP_DIV8_1_CLK] = &dsi1pll_gp_div8.hw,
	[GP_DIV16_1_CLK] = &dsi1pll_gp_div16.hw,
	[GP_DIV32_1_CLK] = &dsi1pll_gp_div32.hw,
	[GP_DIV_MUX_1_CLK] = &dsi1pll_gp_div_mux.clkr.hw,
	[PCLK_SRC_MUX_1_CLK] = &dsi1pll_pclk_src.clkr.hw,
	[BYTE_CLK_SRC_1_CLK] = &dsi1pll_byte_clk_src.hw,
};

int dsi_pll_clock_register_12nm(struct platform_device *pdev,
				  struct mdss_pll_resources *pll_res)
{
	int rc = 0, ndx, i;
	struct clk *clk = NULL;
	struct clk_onecell_data *clk_data;
	int num_clks = ARRAY_SIZE(mdss_dsi_pllcc_12nm);
	struct regmap *rmap;
	struct dsi_pll_db *pdb;

	if (!pdev || !pdev->dev.of_node ||
		!pll_res || !pll_res->pll_base) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	ndx = pll_res->index;

	if (ndx >= DSI_PLL_MAX) {
		pr_err("pll index(%d) NOT supported\n", ndx);
		return -EINVAL;
	}

	pdb = &pll_db[ndx];
	pll_res->priv = pdb;
	pll_res->vco_delay = VCO_DELAY_USEC;
	pdb->pll = pll_res;
	ndx++;
	ndx %= DSI_PLL_MAX;
	pdb->next = &pll_db[ndx];

	clk_data = devm_kzalloc(&pdev->dev, sizeof(struct clk_onecell_data),
					GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kzalloc(&pdev->dev, (num_clks *
				sizeof(struct clk *)), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;
	clk_data->clk_num = num_clks;

	/* Establish client data */
	if (pll_res->index == 0) {
		rmap = devm_regmap_init(&pdev->dev, &post_div_mux_regmap_bus,
				pll_res, &dsi_pll_12nm_config);
		dsi0pll_post_div_mux.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &gp_div_mux_regmap_bus,
				pll_res, &dsi_pll_12nm_config);
		dsi0pll_gp_div_mux.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &pclk_div_regmap_bus,
				pll_res, &dsi_pll_12nm_config);
		dsi0pll_pclk_src.clkr.regmap = rmap;

		dsi0pll_vco_clk.priv = pll_res;
		for (i = VCO_CLK_0; i <= BYTE_CLK_SRC_0_CLK; i++) {
			clk = devm_clk_register(&pdev->dev,
						mdss_dsi_pllcc_12nm[i]);
			if (IS_ERR(clk)) {
				pr_err("clk registration failed for DSI clock:%d\n",
							pll_res->index);
				rc = -EINVAL;
				goto clk_register_fail;
			}
			clk_data->clks[i] = clk;

		}

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);


	} else {
		rmap = devm_regmap_init(&pdev->dev, &post_div_mux_regmap_bus,
				pll_res, &dsi_pll_12nm_config);
		dsi1pll_post_div_mux.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &gp_div_mux_regmap_bus,
				pll_res, &dsi_pll_12nm_config);
		dsi1pll_gp_div_mux.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &pclk_div_regmap_bus,
				pll_res, &dsi_pll_12nm_config);
		dsi1pll_pclk_src.clkr.regmap = rmap;

		dsi1pll_vco_clk.priv = pll_res;

		for (i = VCO_CLK_1; i <= BYTE_CLK_SRC_1_CLK; i++) {
			clk = devm_clk_register(&pdev->dev,
						mdss_dsi_pllcc_12nm[i]);
			if (IS_ERR(clk)) {
				pr_err("clk registration failed for DSI clock:%d\n",
						pll_res->index);
				rc = -EINVAL;
				goto clk_register_fail;
			}
			clk_data->clks[i] = clk;

		}

		rc = of_clk_add_provider(pdev->dev.of_node,
				of_clk_src_onecell_get, clk_data);
	}
	if (!rc) {
		pr_info("Registered DSI PLL ndx=%d, clocks successfully\n",
				pll_res->index);
		return rc;
	}
clk_register_fail:
	return rc;
}
