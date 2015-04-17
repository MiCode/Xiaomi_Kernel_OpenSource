/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/msm-clocks-8996.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-dsi-pll-8996.h"

#define VCO_DELAY_USEC		1

static struct dsi_pll_db pll_db;

static struct clk_ops n2_clk_src_ops;
static struct clk_ops byte_clk_src_ops;
static struct clk_ops post_n1_div_clk_src_ops;

static struct clk_ops clk_ops_gen_mux_dsi;

/* Op structures */
static struct clk_ops clk_ops_dsi_vco = {
	.set_rate = pll_vco_set_rate_8996,
	.round_rate = pll_vco_round_rate_8996,
	.handoff = pll_vco_handoff_8996,
	.prepare = pll_vco_prepare_8996,
	.unprepare = pll_vco_unprepare_8996,
};

static struct clk_div_ops post_n1_div_ops = {
	.set_div = post_n1_div_set_div,
	.get_div = post_n1_div_get_div,
};

static struct clk_div_ops n2_div_ops = {	/* hr_oclk3 */
	.set_div = n2_div_set_div,
	.get_div = n2_div_get_div,
};

static struct clk_mux_ops mdss_byte_mux_ops = {
	.set_mux_sel = set_mdss_byte_mux_sel_8996,
	.get_mux_sel = get_mdss_byte_mux_sel_8996,
};

static struct clk_mux_ops mdss_pixel_mux_ops = {
	.set_mux_sel = set_mdss_pixel_mux_sel_8996,
	.get_mux_sel = get_mdss_pixel_mux_sel_8996,
};

static struct dsi_pll_vco_clk dsi_vco_clk = {
	.ref_clk_rate = 19200000,
	.min_rate = 1300000000,
	.max_rate = 2600000000,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_8996,
	.c = {
		.dbg_name = "dsi_vco_clk_8996",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi_vco_clk.c),
	},
};

static struct div_clk post_n1_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &post_n1_div_ops,
	.c = {
		.parent = &dsi_vco_clk.c,
		.dbg_name = "post_n1_div_clk",
		.ops = &post_n1_div_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(post_n1_div_clk.c),
	},
};

static struct div_clk n2_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &n2_div_ops,
	.c = {
		.parent = &post_n1_div_clk.c,
		.dbg_name = "n2_div_clk",
		.ops = &n2_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(n2_div_clk.c),
	},
};

static struct div_clk pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &n2_div_clk.c,
		.dbg_name = "pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(pixel_clk_src.c),
	},
};

static struct mux_clk mdss_pixel_clk_mux = {
	.num_parents = 1,
	.parents = (struct clk_src[]) {
		{&pixel_clk_src.c, 0},
	},
	.ops = &mdss_pixel_mux_ops,
	.c = {
		.parent = &pixel_clk_src.c,
		.dbg_name = "mdss_pixel_clk_mux",
		.ops = &clk_ops_gen_mux,
		CLK_INIT(mdss_pixel_clk_mux.c),
	}
};

static struct div_clk byte_clk_src = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &post_n1_div_clk.c,
		.dbg_name = "byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(byte_clk_src.c),
	},
};

static struct mux_clk mdss_byte_clk_mux = {
	.num_parents = 1,
	.parents = (struct clk_src[]) {
		{&byte_clk_src.c, 0},
	},
	.ops = &mdss_byte_mux_ops,
	.c = {
		.parent = &byte_clk_src.c,
		.dbg_name = "mdss_byte_clk_mux",
		.ops = &clk_ops_gen_mux_dsi,
		CLK_INIT(mdss_byte_clk_mux.c),
	}
};

static struct clk_lookup mdss_dsi_pllcc_8996[] = {
	CLK_LIST(mdss_byte_clk_mux),
	CLK_LIST(byte_clk_src),
	CLK_LIST(mdss_pixel_clk_mux),
	CLK_LIST(pixel_clk_src),
	CLK_LIST(n2_div_clk),
	CLK_LIST(post_n1_div_clk),
	CLK_LIST(dsi_vco_clk),
};

int dsi_pll_clock_register_8996(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc;
	static struct mdss_pll_resources *master_pll;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	if (!pll_res || !pll_res->pll_base) {
		pr_err("Invalid PLL resources\n");
		return -EPROBE_DEFER;
	}

	pr_debug("ndx=%d is_slave=%d\n", pll_res->index, pll_res->is_slave);

	if (!pll_res->is_slave) {
		/* master at split display or stand alone */
		pll_res->priv = &pll_db;
		master_pll = pll_res;	/* keep master pll */
	} else {
		/* slave pll */
		if (!master_pll) {
			pr_err("No match PLL master found for ndx=%d\n",
							pll_res->index);
			return -EINVAL;
		}
		master_pll->slave = pll_res;
		return 0;	/* done for slave */
	}
	/*
	 * Set client data to mux, div and vco clocks.
	 * This needs to be done only for PLL0 since, that is the one in
	 * use.
	 **/
	byte_clk_src.priv = pll_res;
	pixel_clk_src.priv = pll_res;
	post_n1_div_clk.priv = pll_res;
	n2_div_clk.priv = pll_res;
	dsi_vco_clk.priv = pll_res;

	pll_res->vco_delay = VCO_DELAY_USEC;

	/* Set clock source operations */

	/* hr_oclk3, pixel */
	n2_clk_src_ops = clk_ops_slave_div;
	n2_clk_src_ops.prepare = dsi_pll_div_prepare;

	/* hr_ockl2, byte, vco pll */
	post_n1_div_clk_src_ops = clk_ops_div;
	post_n1_div_clk_src_ops.prepare = dsi_pll_div_prepare;

	byte_clk_src_ops = clk_ops_div;
	byte_clk_src_ops.prepare = dsi_pll_div_prepare;

	clk_ops_gen_mux_dsi = clk_ops_gen_mux;
	clk_ops_gen_mux_dsi.round_rate = parent_round_rate;
	clk_ops_gen_mux_dsi.set_rate = parent_set_rate;

	if (pll_res->target_id == MDSS_PLL_TARGET_8996) {
		rc = of_msm_clock_register(pdev->dev.of_node,
				mdss_dsi_pllcc_8996,
				ARRAY_SIZE(mdss_dsi_pllcc_8996));
		if (rc) {
			pr_err("Clock register failed\n");
			rc = -EPROBE_DEFER;
		}
	}

	if (!rc)
		pr_info("Registered DSI PLL clocks successfully\n");

	return rc;
}
