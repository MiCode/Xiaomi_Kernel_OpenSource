/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/msm-clocks-8994.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"

#define VCO_DELAY_USEC		1

static struct clk_ops bypass_lp_div_mux_clk_ops;
static struct clk_ops pixel_clk_src_ops;
static struct clk_ops byte_clk_src_ops;
static struct clk_ops ndiv_clk_ops;

static struct clk_ops shadow_pixel_clk_src_ops;
static struct clk_ops shadow_byte_clk_src_ops;
static struct clk_ops clk_ops_gen_mux_dsi;

static int vco_set_rate_20nm(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	pr_debug("Cancel pending pll off work\n");
	cancel_work_sync(&dsi_pll_res->pll_off);
	rc = pll_20nm_vco_set_rate(vco, rate);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int vco_set_rate_dummy(struct clk *c, unsigned long rate)
{
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *pll_res = vco->priv;

	mdss_pll_resource_enable(pll_res, true);
	pll_20nm_config_powerdown(pll_res->pll_base);
	mdss_pll_resource_enable(pll_res, false);

	pr_debug("Configuring PLL1 registers.\n");

	return 0;
}

static int shadow_vco_set_rate_20nm(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	if (!dsi_pll_res->resource_enable) {
		pr_err("PLL resources disabled. Dynamic fps invalid\n");
		return -EINVAL;
	}

	rc = shadow_pll_20nm_vco_set_rate(vco, rate);

	return rc;
}

/*
 *                         DSI PLL internal clocks hierarchy
 *                         =================================
 *                                    .-------------.
 *                                    |   vco_clk   |
 *                                    '-------------'
 *                                      |         |
 *                                      |         |
 * .----------.                         |         |
 * | ndiv_clk |-------------------------|         |
 * '----------'                         |         |
 *       |                             D|         |
 *       |                             I|         |
 * .------------------------.          R|         |
 * | indirect_path_div2_clk |          E|         |
 * '------------------------'          C|         |
 *       |                             T|         |
 *       |INDIRECT                      |         |
 *       |                              |         |
 * .-----------------------.            |         |
 * | bypass_lp_div_mux_clk |------------'         |
 * '-----------------------'                      |
 *       |                                        |
 *       |                                        |
 * .------------------------.              .-------------------.
 * | fixed_hr_oclk2_div_clk |              | hr_oclk_3_div_clk |
 * '------------------------'              '-------------------'
 *       |                     |                  |                   |
 *       |                     |                  |                   |
 * .--------------.    .-------------.        .---------------.  .-------------.
 * | byte_clk_src |    | shadow tree |        | pixel_clk_src |  | shadow tree |
 * '--------------'    '-------------'        '---------------'  '-------------'
 *           \             /                         \             /
 *            \           /                           \           /
 *             \         /                             \         /
 *            .--------------.                       .---------------.
 *            | byte_clk_mux |                       | pixel_clk_mux |
 *            '--------------'                       '---------------'
 *
 * The above diagram depicts the design of the DSI PLL. The DSI PLL outputs two
 * clocks - (1) byte_clk_src and (2) pixel_clk_src. However, in order to
 * support dynamic FPS feature, a shadow copy of all the PLL clocks is also
 * created. As a result, the primary outputs of the PLL are two mux clocks -
 * (1) byte_clk_mux and (2) pixel_clk_mux - which allows switching the clock
 * tree from the main branch to the shadow branch.
 *
 * A similar set of clocks is defined for both DSI0 PLL and DSI1 PLL.
 */

/* Clock Ops structures - same for both DSI0 PLL and DSI1 PLL */
static struct clk_ops clk_ops_dsi_vco_dummy = {
	.set_rate = vco_set_rate_dummy,
};

static struct clk_ops clk_ops_dsi_vco = {
	.set_rate = vco_set_rate_20nm,
	.round_rate = pll_20nm_vco_round_rate,
	.handoff = pll_20nm_vco_handoff,
	.prepare = pll_20nm_vco_prepare,
	.unprepare = pll_20nm_vco_unprepare,
};

static struct clk_div_ops fixed_hr_oclk2_div_ops = {
	.set_div = fixed_hr_oclk2_set_div,
	.get_div = fixed_hr_oclk2_get_div,
};

static struct clk_div_ops ndiv_ops = {
	.set_div = ndiv_set_div,
	.get_div = ndiv_get_div,
};

static struct clk_div_ops hr_oclk3_div_ops = {
	.set_div = hr_oclk3_set_div,
	.get_div = hr_oclk3_get_div,
};

static struct clk_mux_ops bypass_lp_div_mux_ops = {
	.set_mux_sel = set_bypass_lp_div_mux_sel,
	.get_mux_sel = get_bypass_lp_div_mux_sel,
};

static struct clk_ops shadow_clk_ops_dsi_vco = {
	.set_rate = shadow_vco_set_rate_20nm,
	.round_rate = pll_20nm_vco_round_rate,
	.handoff = pll_20nm_vco_handoff,
};

static struct clk_div_ops shadow_fixed_hr_oclk2_div_ops = {
	.set_div = shadow_fixed_hr_oclk2_set_div,
	.get_div = fixed_hr_oclk2_get_div,
};

static struct clk_div_ops shadow_ndiv_ops = {
	.set_div = shadow_ndiv_set_div,
	.get_div = ndiv_get_div,
};

static struct clk_div_ops shadow_hr_oclk3_div_ops = {
	.set_div = shadow_hr_oclk3_set_div,
	.get_div = hr_oclk3_get_div,
};

static struct clk_mux_ops shadow_bypass_lp_div_mux_ops = {
	.set_mux_sel = set_shadow_bypass_lp_div_mux_sel,
	.get_mux_sel = get_bypass_lp_div_mux_sel,
};

static struct clk_mux_ops mdss_byte_mux_ops = {
	.set_mux_sel = set_mdss_byte_mux_sel,
	.get_mux_sel = get_mdss_byte_mux_sel,
};

static struct clk_mux_ops mdss_pixel_mux_ops = {
	.set_mux_sel = set_mdss_pixel_mux_sel,
	.get_mux_sel = get_mdss_pixel_mux_sel,
};

/* DSI0 PLL main tree */
static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000,
	.min_rate = 300000000,
	.max_rate = 1500000000,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = pll_20nm_vco_enable_seq,
	.c = {
		.dbg_name = "dsi0pll_vco_clk",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi0pll_vco_clk.c),
	},
};

static struct div_clk dsi0pll_ndiv_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &ndiv_ops,
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_ndiv_clk",
		.ops = &ndiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_ndiv_clk.c),
	},
};

static struct div_clk dsi0pll_indirect_path_div2_clk = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_ndiv_clk.c,
		.dbg_name = "dsi0pll_indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_indirect_path_div2_clk.c),
	},
};

static struct div_clk dsi0pll_hr_oclk3_div_clk = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &hr_oclk3_div_ops,
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_hr_oclk3_div_clk",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_hr_oclk3_div_clk.c),
	},
};

static struct div_clk dsi0pll_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_hr_oclk3_div_clk.c,
		.dbg_name = "dsi0pll_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pixel_clk_src.c),
	},
};

static struct mux_clk dsi0pll_bypass_lp_div_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi0pll_vco_clk.c, 0},
		{&dsi0pll_indirect_path_div2_clk.c, 1},
	},
	.ops = &bypass_lp_div_mux_ops,
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_bypass_lp_div_mux",
		.ops = &bypass_lp_div_mux_clk_ops,
		CLK_INIT(dsi0pll_bypass_lp_div_mux.c),
	},
};

static struct div_clk dsi0pll_fixed_hr_oclk2_div_clk = {
	.ops = &fixed_hr_oclk2_div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi0pll_bypass_lp_div_mux.c,
		.dbg_name = "dsi0pll_fixed_hr_oclk2_div_clk",
		.ops = &byte_clk_src_ops,
		CLK_INIT(dsi0pll_fixed_hr_oclk2_div_clk.c),
	},
};

static struct div_clk dsi0pll_byte_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_fixed_hr_oclk2_div_clk.c,
		.dbg_name = "dsi0pll_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi0pll_byte_clk_src.c),
	},
};

/* DSI0 PLL Shadow Tree */
static struct dsi_pll_vco_clk dsi0pll_shadow_dsi_vco_clk = {
	.ref_clk_rate = 19200000,
	.min_rate = 300000000,
	.max_rate = 1500000000,
	.c = {
		.dbg_name = "dsi0pll_shadow_dsi_vco_clk",
		.ops = &shadow_clk_ops_dsi_vco,
		CLK_INIT(dsi0pll_shadow_dsi_vco_clk.c),
	},
};

static struct div_clk dsi0pll_shadow_ndiv_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &shadow_ndiv_ops,
	.c = {
		.parent = &dsi0pll_shadow_dsi_vco_clk.c,
		.dbg_name = "dsi0pll_shadow_ndiv_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_shadow_ndiv_clk.c),
	},
};

static struct div_clk dsi0pll_shadow_indirect_path_div2_clk = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_shadow_ndiv_clk.c,
		.dbg_name = "dsi0pll_shadow_indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_shadow_indirect_path_div2_clk.c),
	},
};

static struct div_clk dsi0pll_shadow_hr_oclk3_div_clk = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &shadow_hr_oclk3_div_ops,
	.c = {
		.parent = &dsi0pll_shadow_dsi_vco_clk.c,
		.dbg_name = "dsi0pll_shadow_hr_oclk3_div_clk",
		.ops = &shadow_pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_shadow_hr_oclk3_div_clk.c),
	},
};

static struct div_clk dsi0pll_shadow_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_shadow_hr_oclk3_div_clk.c,
		.dbg_name = "dsi0pll_shadow_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_shadow_pixel_clk_src.c),
	},
};

static struct mux_clk dsi0pll_shadow_bypass_lp_div_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi0pll_shadow_dsi_vco_clk.c, 0},
		{&dsi0pll_shadow_indirect_path_div2_clk.c, 1},
	},
	.ops = &shadow_bypass_lp_div_mux_ops,
	.c = {
		.parent = &dsi0pll_shadow_dsi_vco_clk.c,
		.dbg_name = "dsi0pll_shadow_bypass_lp_div_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(dsi0pll_shadow_bypass_lp_div_mux.c),
	},
};

static struct div_clk dsi0pll_shadow_fixed_hr_oclk2_div_clk = {
	.ops = &shadow_fixed_hr_oclk2_div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi0pll_shadow_bypass_lp_div_mux.c,
		.dbg_name = "dsi0pll_shadow_fixed_hr_oclk2_div_clk",
		.ops = &shadow_byte_clk_src_ops,
		CLK_INIT(dsi0pll_shadow_fixed_hr_oclk2_div_clk.c),
	},
};

static struct div_clk dsi0pll_shadow_byte_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_shadow_fixed_hr_oclk2_div_clk.c,
		.dbg_name = "dsi0pll_shadow_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi0pll_shadow_byte_clk_src.c),
	},
};

/* DSI0 main/shadow selection MUX clocks */
static struct mux_clk dsi0pll_pixel_clk_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi0pll_pixel_clk_src.c, 0},
		{&dsi0pll_shadow_pixel_clk_src.c, 1},
	},
	.ops = &mdss_pixel_mux_ops,
	.c = {
		.parent = &dsi0pll_pixel_clk_src.c,
		.dbg_name = "dsi0pll_pixel_clk_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(dsi0pll_pixel_clk_mux.c),
	}
};

static struct mux_clk dsi0pll_byte_clk_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi0pll_byte_clk_src.c, 0},
		{&dsi0pll_shadow_byte_clk_src.c, 1},
	},
	.ops = &mdss_byte_mux_ops,
	.c = {
		.parent = &dsi0pll_byte_clk_src.c,
		.dbg_name = "dsi0pll_byte_clk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		CLK_INIT(dsi0pll_byte_clk_mux.c),
	}
};

static struct clk_lookup dsi0_pllcc_20nm[] = {
	CLK_LIST(dsi0pll_pixel_clk_mux),
	CLK_LIST(dsi0pll_byte_clk_mux),
	CLK_LIST(dsi0pll_pixel_clk_src),
	CLK_LIST(dsi0pll_byte_clk_src),
	CLK_LIST(dsi0pll_fixed_hr_oclk2_div_clk),
	CLK_LIST(dsi0pll_bypass_lp_div_mux),
	CLK_LIST(dsi0pll_hr_oclk3_div_clk),
	CLK_LIST(dsi0pll_indirect_path_div2_clk),
	CLK_LIST(dsi0pll_ndiv_clk),
	CLK_LIST(dsi0pll_vco_clk),
	CLK_LIST(dsi0pll_shadow_pixel_clk_src),
	CLK_LIST(dsi0pll_shadow_byte_clk_src),
	CLK_LIST(dsi0pll_shadow_fixed_hr_oclk2_div_clk),
	CLK_LIST(dsi0pll_shadow_bypass_lp_div_mux),
	CLK_LIST(dsi0pll_shadow_hr_oclk3_div_clk),
	CLK_LIST(dsi0pll_shadow_indirect_path_div2_clk),
	CLK_LIST(dsi0pll_shadow_ndiv_clk),
	CLK_LIST(dsi0pll_shadow_dsi_vco_clk),
};

/* DSI1 PLL main tree */
static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000,
	.min_rate = 300000000,
	.max_rate = 1500000000,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = pll_20nm_vco_enable_seq,
	.c = {
		.dbg_name = "dsi1pll_vco_clk",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi1pll_vco_clk.c),
	},
};

static struct div_clk dsi1pll_ndiv_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &ndiv_ops,
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_ndiv_clk",
		.ops = &ndiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_ndiv_clk.c),
	},
};

static struct div_clk dsi1pll_indirect_path_div2_clk = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_ndiv_clk.c,
		.dbg_name = "dsi1pll_indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_indirect_path_div2_clk.c),
	},
};

static struct div_clk dsi1pll_hr_oclk3_div_clk = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &hr_oclk3_div_ops,
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_hr_oclk3_div_clk",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_hr_oclk3_div_clk.c),
	},
};

static struct div_clk dsi1pll_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_hr_oclk3_div_clk.c,
		.dbg_name = "dsi1pll_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pixel_clk_src.c),
	},
};

static struct mux_clk dsi1pll_bypass_lp_div_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi1pll_vco_clk.c, 0},
		{&dsi1pll_indirect_path_div2_clk.c, 1},
	},
	.ops = &bypass_lp_div_mux_ops,
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_bypass_lp_div_mux",
		.ops = &bypass_lp_div_mux_clk_ops,
		CLK_INIT(dsi1pll_bypass_lp_div_mux.c),
	},
};

static struct div_clk dsi1pll_fixed_hr_oclk2_div_clk = {
	.ops = &fixed_hr_oclk2_div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi1pll_bypass_lp_div_mux.c,
		.dbg_name = "dsi1pll_fixed_hr_oclk2_div_clk",
		.ops = &byte_clk_src_ops,
		CLK_INIT(dsi1pll_fixed_hr_oclk2_div_clk.c),
	},
};

static struct div_clk dsi1pll_byte_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_fixed_hr_oclk2_div_clk.c,
		.dbg_name = "dsi1pll_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi1pll_byte_clk_src.c),
	},
};

/* DSI1 PLL Shadow Tree */
static struct dsi_pll_vco_clk dsi1pll_shadow_dsi_vco_clk = {
	.ref_clk_rate = 19200000,
	.min_rate = 300000000,
	.max_rate = 1500000000,
	.c = {
		.dbg_name = "dsi1pll_shadow_dsi_vco_clk",
		.ops = &shadow_clk_ops_dsi_vco,
		CLK_INIT(dsi1pll_shadow_dsi_vco_clk.c),
	},
};

static struct div_clk dsi1pll_shadow_ndiv_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &shadow_ndiv_ops,
	.c = {
		.parent = &dsi1pll_shadow_dsi_vco_clk.c,
		.dbg_name = "dsi1pll_shadow_ndiv_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_shadow_ndiv_clk.c),
	},
};

static struct div_clk dsi1pll_shadow_indirect_path_div2_clk = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_shadow_ndiv_clk.c,
		.dbg_name = "dsi1pll_shadow_indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_shadow_indirect_path_div2_clk.c),
	},
};

static struct div_clk dsi1pll_shadow_hr_oclk3_div_clk = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &shadow_hr_oclk3_div_ops,
	.c = {
		.parent = &dsi1pll_shadow_dsi_vco_clk.c,
		.dbg_name = "dsi1pll_shadow_hr_oclk3_div_clk",
		.ops = &shadow_pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_shadow_hr_oclk3_div_clk.c),
	},
};

static struct div_clk dsi1pll_shadow_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_shadow_hr_oclk3_div_clk.c,
		.dbg_name = "dsi1pll_shadow_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_shadow_pixel_clk_src.c),
	},
};

static struct mux_clk dsi1pll_shadow_bypass_lp_div_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi1pll_shadow_dsi_vco_clk.c, 0},
		{&dsi1pll_shadow_indirect_path_div2_clk.c, 1},
	},
	.ops = &shadow_bypass_lp_div_mux_ops,
	.c = {
		.parent = &dsi1pll_shadow_dsi_vco_clk.c,
		.dbg_name = "dsi1pll_shadow_bypass_lp_div_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(dsi1pll_shadow_bypass_lp_div_mux.c),
	},
};

static struct div_clk dsi1pll_shadow_fixed_hr_oclk2_div_clk = {
	.ops = &shadow_fixed_hr_oclk2_div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi1pll_shadow_bypass_lp_div_mux.c,
		.dbg_name = "dsi1pll_shadow_fixed_hr_oclk2_div_clk",
		.ops = &shadow_byte_clk_src_ops,
		CLK_INIT(dsi1pll_shadow_fixed_hr_oclk2_div_clk.c),
	},
};

static struct div_clk dsi1pll_shadow_byte_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_shadow_fixed_hr_oclk2_div_clk.c,
		.dbg_name = "dsi1pll_shadow_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi1pll_shadow_byte_clk_src.c),
	},
};

/* DSI1 main/shadow selection MUX clocks */
static struct mux_clk dsi1pll_pixel_clk_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi1pll_pixel_clk_src.c, 0},
		{&dsi1pll_shadow_pixel_clk_src.c, 1},
	},
	.ops = &mdss_pixel_mux_ops,
	.c = {
		.parent = &dsi1pll_pixel_clk_src.c,
		.dbg_name = "dsi1pll_pixel_clk_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(dsi1pll_pixel_clk_mux.c),
	}
};

static struct mux_clk dsi1pll_byte_clk_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]) {
		{&dsi1pll_byte_clk_src.c, 0},
		{&dsi1pll_shadow_byte_clk_src.c, 1},
	},
	.ops = &mdss_byte_mux_ops,
	.c = {
		.parent = &dsi1pll_byte_clk_src.c,
		.dbg_name = "dsi1pll_byte_clk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		CLK_INIT(dsi1pll_byte_clk_mux.c),
	}
};

/* DSI1 PLL dummy clocks used for SW workarounds */
static struct dsi_pll_vco_clk dsi1pll_vco_dummy_clk = {
	.c = {
		.dbg_name = "dsi1pll_vco_dummy_clk",
		.ops = &clk_ops_dsi_vco_dummy,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_vco_dummy_clk.c),
	},
};

static struct clk_lookup dsi1_pllcc_20nm[] = {
	CLK_LIST(dsi1pll_pixel_clk_mux),
	CLK_LIST(dsi1pll_byte_clk_mux),
	CLK_LIST(dsi1pll_pixel_clk_src),
	CLK_LIST(dsi1pll_byte_clk_src),
	CLK_LIST(dsi1pll_vco_clk),
	CLK_LIST(dsi1pll_shadow_pixel_clk_src),
	CLK_LIST(dsi1pll_shadow_byte_clk_src),
	CLK_LIST(dsi1pll_fixed_hr_oclk2_div_clk),
	CLK_LIST(dsi1pll_bypass_lp_div_mux),
	CLK_LIST(dsi1pll_hr_oclk3_div_clk),
	CLK_LIST(dsi1pll_indirect_path_div2_clk),
	CLK_LIST(dsi1pll_ndiv_clk),
	CLK_LIST(dsi1pll_vco_dummy_clk),
};

static void dsi_pll_off_work(struct work_struct *work)
{
	struct mdss_pll_resources *pll_res;

	if (!work) {
		pr_err("pll_resource is invalid\n");
		return;
	}

	pr_debug("Starting PLL off Worker%s\n", __func__);

	pll_res = container_of(work, struct
			mdss_pll_resources, pll_off);

	mdss_pll_resource_enable(pll_res, true);
	pll_20nm_config_powerdown(pll_res->pll_base);
	mdss_pll_resource_enable(pll_res, false);
}

static int dsi_pll_regulator_notifier_call(struct notifier_block *self,
		unsigned long event, void *data)
{

	struct mdss_pll_resources *pll_res;

	if (!self) {
		pr_err("pll_resource is invalid\n");
		goto error;
	}

	pll_res = container_of(self, struct
			mdss_pll_resources, gdsc_cb);

	if (event & REGULATOR_EVENT_ENABLE) {
		pr_debug("Regulator ON event. Scheduling pll off worker\n");
		schedule_work(&pll_res->pll_off);
	}

	if (event & REGULATOR_EVENT_DISABLE)
		pr_debug("Regulator OFF event.\n");

error:
	return NOTIFY_OK;
}

int dsi_pll_clock_register_20nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc;
	struct dss_vreg *pll_reg;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	if (!pll_res || !pll_res->pll_base) {
		pr_err("Invalid PLL resources\n");
		return -EPROBE_DEFER;
	}

	/*
	 * Set client data to mux, div and vco clocks.
	 * This needs to be done only for PLL0 since, that is the one in
	 * use.
	 **/
	if (!pll_res->index) {
		dsi0pll_byte_clk_src.priv = pll_res;
		dsi0pll_pixel_clk_src.priv = pll_res;
		dsi0pll_bypass_lp_div_mux.priv = pll_res;
		dsi0pll_indirect_path_div2_clk.priv = pll_res;
		dsi0pll_ndiv_clk.priv = pll_res;
		dsi0pll_fixed_hr_oclk2_div_clk.priv = pll_res;
		dsi0pll_hr_oclk3_div_clk.priv = pll_res;
		dsi0pll_vco_clk.priv = pll_res;

		dsi0pll_shadow_byte_clk_src.priv = pll_res;
		dsi0pll_shadow_pixel_clk_src.priv = pll_res;
		dsi0pll_shadow_bypass_lp_div_mux.priv = pll_res;
		dsi0pll_shadow_indirect_path_div2_clk.priv = pll_res;
		dsi0pll_shadow_ndiv_clk.priv = pll_res;
		dsi0pll_shadow_fixed_hr_oclk2_div_clk.priv = pll_res;
		dsi0pll_shadow_hr_oclk3_div_clk.priv = pll_res;
		dsi0pll_shadow_dsi_vco_clk.priv = pll_res;

		if (pll_res->pll_en_90_phase) {
			dsi0pll_vco_clk.min_rate = 1000000000;
			dsi0pll_vco_clk.max_rate = 2000000000;
			dsi0pll_shadow_dsi_vco_clk.min_rate = 1000000000;
			dsi0pll_shadow_dsi_vco_clk.max_rate = 2000000000;
			pr_debug("%s:Update VCO range: 1GHz-2Ghz", __func__);
		}
	} else {
		dsi1pll_byte_clk_src.priv = pll_res;
		dsi1pll_pixel_clk_src.priv = pll_res;
		dsi1pll_bypass_lp_div_mux.priv = pll_res;
		dsi1pll_indirect_path_div2_clk.priv = pll_res;
		dsi1pll_ndiv_clk.priv = pll_res;
		dsi1pll_fixed_hr_oclk2_div_clk.priv = pll_res;
		dsi1pll_hr_oclk3_div_clk.priv = pll_res;
		dsi1pll_vco_clk.priv = pll_res;

		dsi1pll_shadow_byte_clk_src.priv = pll_res;
		dsi1pll_shadow_pixel_clk_src.priv = pll_res;
		dsi1pll_shadow_bypass_lp_div_mux.priv = pll_res;
		dsi1pll_shadow_indirect_path_div2_clk.priv = pll_res;
		dsi1pll_shadow_ndiv_clk.priv = pll_res;
		dsi1pll_shadow_fixed_hr_oclk2_div_clk.priv = pll_res;
		dsi1pll_shadow_hr_oclk3_div_clk.priv = pll_res;
		dsi1pll_shadow_dsi_vco_clk.priv = pll_res;

		dsi1pll_vco_dummy_clk.priv = pll_res;

		if (pll_res->pll_en_90_phase) {
			dsi1pll_vco_clk.min_rate = 1000000000;
			dsi1pll_vco_clk.max_rate = 2000000000;
			dsi1pll_shadow_dsi_vco_clk.min_rate = 1000000000;
			dsi1pll_shadow_dsi_vco_clk.max_rate = 2000000000;
			pr_debug("%s:Update VCO range: 1GHz-2Ghz", __func__);
		}
	}

	pll_res->vco_delay = VCO_DELAY_USEC;

	/* Set clock source operations */
	pixel_clk_src_ops = clk_ops_slave_div;
	pixel_clk_src_ops.prepare = dsi_pll_div_prepare;

	ndiv_clk_ops = clk_ops_div;
	ndiv_clk_ops.prepare = dsi_pll_div_prepare;

	byte_clk_src_ops = clk_ops_div;
	byte_clk_src_ops.prepare = dsi_pll_div_prepare;

	bypass_lp_div_mux_clk_ops = clk_ops_gen_mux;
	bypass_lp_div_mux_clk_ops.prepare = dsi_pll_mux_prepare;

	clk_ops_gen_mux_dsi = clk_ops_gen_mux;
	clk_ops_gen_mux_dsi.round_rate = parent_round_rate;
	clk_ops_gen_mux_dsi.set_rate = parent_set_rate;

	shadow_pixel_clk_src_ops = clk_ops_slave_div;
	shadow_pixel_clk_src_ops.prepare = dsi_pll_div_prepare;

	shadow_byte_clk_src_ops = clk_ops_div;
	shadow_byte_clk_src_ops.prepare = dsi_pll_div_prepare;

	if ((pll_res->target_id == MDSS_PLL_TARGET_8994) ||
			(pll_res->target_id == MDSS_PLL_TARGET_8992)) {
		if (pll_res->index) {
			rc = of_msm_clock_register(pdev->dev.of_node,
				dsi1_pllcc_20nm,
				ARRAY_SIZE(dsi1_pllcc_20nm));
			if (rc) {
				pr_err("Clock register failed\n");
				rc = -EPROBE_DEFER;
			}
		} else {
			rc = of_msm_clock_register(pdev->dev.of_node,
				dsi0_pllcc_20nm,
				ARRAY_SIZE(dsi0_pllcc_20nm));
			if (rc) {
				pr_err("Clock register failed\n");
				rc = -EPROBE_DEFER;
			}
		}

		pll_res->gdsc_cb.notifier_call =
			dsi_pll_regulator_notifier_call;
		INIT_WORK(&pll_res->pll_off, dsi_pll_off_work);

		pll_reg = mdss_pll_get_mp_by_reg_name(pll_res, "gdsc");
		if (pll_reg) {
			pr_debug("Registering for gdsc regulator events\n");
			if (regulator_register_notifier(pll_reg->vreg,
						&(pll_res->gdsc_cb)))
				pr_err("Regulator notification registration failed!\n");
		}
	} else {
		pr_err("Invalid target ID\n");
		rc = -EINVAL;
	}

	if (!rc)
		pr_info("Registered DSI PLL clocks successfully\n");

	return rc;
}
