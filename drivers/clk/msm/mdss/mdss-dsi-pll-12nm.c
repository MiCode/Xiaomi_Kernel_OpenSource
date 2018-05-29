/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/workqueue.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-8952.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-dsi-pll-12nm.h"

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

static struct dsi_pll_db pll_db[DSI_PLL_NUM];

static struct clk_ops pixel_div_clk_src_ops;

/* Op structures */
static const struct clk_ops clk_ops_dsi_vco = {
	.set_rate = pll_vco_set_rate_12nm,
	.round_rate = pll_vco_round_rate_12nm,
	.handoff = pll_vco_handoff_12nm,
	.prepare = pll_vco_prepare_12nm,
	.unprepare = pll_vco_unprepare_12nm,
	.enable = pll_vco_enable_12nm,
};

static struct clk_div_ops pixel_div_ops = {
	.set_div = pixel_div_set_div,
	.get_div = pixel_div_get_div,
};

static struct clk_mux_ops post_div_mux_ops = {
	.set_mux_sel = set_post_div_mux_sel,
	.get_mux_sel = get_post_div_mux_sel,
};

static struct clk_mux_ops gp_div_mux_ops = {
	.set_mux_sel = set_gp_mux_sel,
	.get_mux_sel = get_gp_mux_sel,
};

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 2000000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_12nm,
	.c = {
		.dbg_name = "dsi0pll_vco_clk_12nm",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi0pll_vco_clk.c),
	},
};

static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1000000000UL,
	.max_rate = 2000000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_12nm,
	.c = {
		.dbg_name = "dsi1pll_vco_clk_12nm",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi1pll_vco_clk.c),
	},
};

static struct div_clk dsi0pll_post_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_div1",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_div1.c),
	},
};

static struct div_clk dsi0pll_post_div2 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_div2",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_div2.c),
	},
};

static struct div_clk dsi0pll_post_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_div4",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_div4.c),
	},
};

static struct div_clk dsi0pll_post_div8 = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_div8",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_div8.c),
	},
};

static struct div_clk dsi0pll_post_div16 = {
	.data = {
		.div = 16,
		.min_div = 16,
		.max_div = 16,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_div16",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_div16.c),
	},
};

static struct div_clk dsi0pll_post_div32 = {
	.data = {
		.div = 32,
		.min_div = 32,
		.max_div = 32,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_div32",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_div32.c),
	},
};

static struct mux_clk dsi0pll_post_div_mux = {
	.num_parents = 6,
	.parents = (struct clk_src[]) {
		{&dsi0pll_post_div1.c, 0},
		{&dsi0pll_post_div2.c, 1},
		{&dsi0pll_post_div4.c, 2},
		{&dsi0pll_post_div8.c, 3},
		{&dsi0pll_post_div16.c, 4},
		{&dsi0pll_post_div32.c, 5},
	},
	.ops = &post_div_mux_ops,
	.c = {
		.parent = &dsi0pll_post_div1.c,
		.dbg_name = "dsi0pll_post_div_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_div_mux.c),
	}
};

static struct div_clk dsi1pll_post_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_div1",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_div1.c),
	},
};

static struct div_clk dsi1pll_post_div2 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_div2",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_div2.c),
	},
};

static struct div_clk dsi1pll_post_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_div4",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_div4.c),
	},
};

static struct div_clk dsi1pll_post_div8 = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_div8",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_div8.c),
	},
};

static struct div_clk dsi1pll_post_div16 = {
	.data = {
		.div = 16,
		.min_div = 16,
		.max_div = 16,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_div16",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_div16.c),
	},
};

static struct div_clk dsi1pll_post_div32 = {
	.data = {
		.div = 32,
		.min_div = 32,
		.max_div = 32,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_div32",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_div32.c),
	},
};

static struct mux_clk dsi1pll_post_div_mux = {
	.num_parents = 6,
	.parents = (struct clk_src[]) {
		{&dsi1pll_post_div1.c, 0},
		{&dsi1pll_post_div2.c, 1},
		{&dsi1pll_post_div4.c, 2},
		{&dsi1pll_post_div8.c, 3},
		{&dsi1pll_post_div16.c, 4},
		{&dsi1pll_post_div32.c, 5},
	},
	.ops = &post_div_mux_ops,
	.c = {
		.parent = &dsi1pll_post_div1.c,
		.dbg_name = "dsi1pll_post_div_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_div_mux.c),
	}
};

static struct div_clk dsi0pll_gp_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_gp_div1",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_gp_div1.c),
	},
};

static struct div_clk dsi0pll_gp_div2 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_gp_div2",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_gp_div2.c),
	},
};

static struct div_clk dsi0pll_gp_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_gp_div4",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_gp_div4.c),
	},
};

static struct div_clk dsi0pll_gp_div8 = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_gp_div8",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_gp_div8.c),
	},
};

static struct div_clk dsi0pll_gp_div16 = {
	.data = {
		.div = 16,
		.min_div = 16,
		.max_div = 16,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_gp_div16",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_gp_div16.c),
	},
};

static struct div_clk dsi0pll_gp_div32 = {
	.data = {
		.div = 32,
		.min_div = 32,
		.max_div = 32,
	},
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_gp_div32",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_gp_div32.c),
	},
};

static struct mux_clk dsi0pll_gp_div_mux = {
	.num_parents = 6,
	.parents = (struct clk_src[]) {
		{&dsi0pll_gp_div1.c, 0},
		{&dsi0pll_gp_div2.c, 1},
		{&dsi0pll_gp_div4.c, 2},
		{&dsi0pll_gp_div8.c, 3},
		{&dsi0pll_gp_div16.c, 4},
		{&dsi0pll_gp_div32.c, 5},
	},
	.ops = &gp_div_mux_ops,
	.c = {
		.parent = &dsi0pll_gp_div1.c,
		.dbg_name = "dsi0pll_gp_div_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_gp_div_mux.c),
	}
};

static struct div_clk dsi1pll_gp_div1 = {
	.data = {
		.div = 1,
		.min_div = 1,
		.max_div = 1,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_gp_div1",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_gp_div1.c),
	},
};

static struct div_clk dsi1pll_gp_div2 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_gp_div2",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_gp_div2.c),
	},
};

static struct div_clk dsi1pll_gp_div4 = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_gp_div4",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_gp_div4.c),
	},
};

static struct div_clk dsi1pll_gp_div8 = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_gp_div8",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_gp_div8.c),
	},
};

static struct div_clk dsi1pll_gp_div16 = {
	.data = {
		.div = 16,
		.min_div = 16,
		.max_div = 16,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_gp_div16",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_gp_div16.c),
	},
};

static struct div_clk dsi1pll_gp_div32 = {
	.data = {
		.div = 32,
		.min_div = 32,
		.max_div = 32,
	},
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_gp_div32",
		.ops = &clk_ops_slave_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_gp_div32.c),
	},
};

static struct mux_clk dsi1pll_gp_div_mux = {
	.num_parents = 6,
	.parents = (struct clk_src[]) {
		{&dsi1pll_gp_div1.c, 0},
		{&dsi1pll_gp_div2.c, 1},
		{&dsi1pll_gp_div4.c, 2},
		{&dsi1pll_gp_div8.c, 3},
		{&dsi1pll_gp_div16.c, 4},
		{&dsi1pll_gp_div32.c, 5},
	},
	.ops = &gp_div_mux_ops,
	.c = {
		.parent = &dsi1pll_gp_div1.c,
		.dbg_name = "dsi1pll_gp_div_mux",
		.ops = &clk_ops_gen_mux,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_gp_div_mux.c),
	}
};

static struct div_clk dsi0pll_pixel_clk_src = {
	.data = {
		.max_div = 128,
		.min_div = 1,
	},
	.ops = &pixel_div_ops,
	.c = {
		.parent = &dsi0pll_gp_div_mux.c,
		.dbg_name = "dsi0pll_pixel_clk_src",
		.ops = &pixel_div_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pixel_clk_src.c),
	},
};

static struct div_clk dsi1pll_pixel_clk_src = {
	.data = {
		.max_div = 128,
		.min_div = 1,
	},
	.ops = &pixel_div_ops,
	.c = {
		.parent = &dsi1pll_gp_div_mux.c,
		.dbg_name = "dsi1pll_pixel_clk_src",
		.ops = &pixel_div_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pixel_clk_src.c),
	},
};

static struct div_clk dsi0pll_byte_clk_src = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi0pll_post_div_mux.c,
		.dbg_name = "dsi0pll_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi0pll_byte_clk_src.c),
	},
};

static struct div_clk dsi1pll_byte_clk_src = {
	.data = {
		.div = 4,
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi1pll_post_div_mux.c,
		.dbg_name = "dsi1pll_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi1pll_byte_clk_src.c),
	},
};

static struct clk_lookup mdss_dsi_pllcc_12nm[] = {
	CLK_LIST(dsi0pll_vco_clk),
	CLK_LIST(dsi0pll_byte_clk_src),
	CLK_LIST(dsi0pll_pixel_clk_src),
};

static struct clk_lookup mdss_dsi_pllcc_12nm_1[] = {
	CLK_LIST(dsi1pll_vco_clk),
	CLK_LIST(dsi1pll_byte_clk_src),
	CLK_LIST(dsi1pll_pixel_clk_src),
};

int dsi_pll_clock_register_12nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = 0, ndx;
	struct dsi_pll_db *pdb;
	int const ssc_freq_min = 30000; /* min. recommended freq. value */
	int const ssc_freq_max = 33000; /* max. recommended freq. value */
	int const ssc_ppm_max = 5000; /* max. recommended ppm */

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	if (!pll_res || !pll_res->pll_base) {
		pr_err("Invalid PLL resources\n");
		return -EPROBE_DEFER;
	}

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

	/* Set clock source operations */

	/* pixel_clk */
	pixel_div_clk_src_ops = clk_ops_div;
	pixel_div_clk_src_ops.prepare = dsi_pll_div_prepare;

	if (pll_res->ssc_en) {
		if (!pll_res->ssc_freq || (pll_res->ssc_freq < ssc_freq_min) ||
			(pll_res->ssc_freq > ssc_freq_max)) {
			pll_res->ssc_freq = ssc_freq_min;
			pr_debug("SSC frequency out of recommended range. Set to default=%d\n",
				pll_res->ssc_freq);
		}

		if (!pll_res->ssc_ppm || (pll_res->ssc_ppm > ssc_ppm_max)) {
			pll_res->ssc_ppm = ssc_ppm_max;
			pr_debug("SSC PPM out of recommended range. Set to default=%d\n",
				pll_res->ssc_ppm);
		}
	}

	/* Set client data to mux, div and vco clocks.  */
	if (pll_res->index == DSI_PLL_1) {
		dsi1pll_byte_clk_src.priv = pll_res;
		dsi1pll_post_div_mux.priv = pll_res;
		dsi1pll_post_div1.priv = pll_res;
		dsi1pll_post_div2.priv = pll_res;
		dsi1pll_post_div4.priv = pll_res;
		dsi1pll_post_div8.priv = pll_res;
		dsi1pll_post_div16.priv = pll_res;
		dsi1pll_post_div32.priv = pll_res;
		dsi1pll_pixel_clk_src.priv = pll_res;
		dsi1pll_gp_div_mux.priv = pll_res;
		dsi1pll_gp_div1.priv = pll_res;
		dsi1pll_gp_div2.priv = pll_res;
		dsi1pll_gp_div4.priv = pll_res;
		dsi1pll_gp_div8.priv = pll_res;
		dsi1pll_gp_div16.priv = pll_res;
		dsi1pll_gp_div32.priv = pll_res;
		dsi1pll_vco_clk.priv = pll_res;

		if (pll_res->target_id == MDSS_PLL_TARGET_SDM439)
			rc = of_msm_clock_register(pdev->dev.of_node,
				mdss_dsi_pllcc_12nm_1,
				ARRAY_SIZE(mdss_dsi_pllcc_12nm_1));
	} else {
		dsi0pll_byte_clk_src.priv = pll_res;
		dsi0pll_post_div_mux.priv = pll_res;
		dsi0pll_post_div1.priv = pll_res;
		dsi0pll_post_div2.priv = pll_res;
		dsi0pll_post_div4.priv = pll_res;
		dsi0pll_post_div8.priv = pll_res;
		dsi0pll_post_div16.priv = pll_res;
		dsi0pll_post_div32.priv = pll_res;
		dsi0pll_pixel_clk_src.priv = pll_res;
		dsi0pll_gp_div_mux.priv = pll_res;
		dsi0pll_gp_div1.priv = pll_res;
		dsi0pll_gp_div2.priv = pll_res;
		dsi0pll_gp_div4.priv = pll_res;
		dsi0pll_gp_div8.priv = pll_res;
		dsi0pll_gp_div16.priv = pll_res;
		dsi0pll_gp_div32.priv = pll_res;
		dsi0pll_vco_clk.priv = pll_res;

		if (pll_res->target_id == MDSS_PLL_TARGET_SDM439)
			rc = of_msm_clock_register(pdev->dev.of_node,
				mdss_dsi_pllcc_12nm,
				ARRAY_SIZE(mdss_dsi_pllcc_12nm));
	}

	if (!rc) {
		pr_info("Registered DSI PLL ndx=%d clocks successfully\n",
						pll_res->index);
	}

	return rc;
}
