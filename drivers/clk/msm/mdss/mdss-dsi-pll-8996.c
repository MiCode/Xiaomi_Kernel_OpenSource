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

static struct dsi_pll_db pll_db[DSI_PLL_NUM];

static struct clk_ops n2_clk_src_ops;
static struct clk_ops shadow_n2_clk_src_ops;
static struct clk_ops byte_clk_src_ops;
static struct clk_ops post_n1_div_clk_src_ops;
static struct clk_ops shadow_post_n1_div_clk_src_ops;

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

/* Shadow ops for dynamic refresh */
static struct clk_ops clk_ops_shadow_dsi_vco = {
	.set_rate = shadow_pll_vco_set_rate_8996,
	.round_rate = pll_vco_round_rate_8996,
	.handoff = shadow_pll_vco_handoff_8996,
};

static struct clk_div_ops shadow_post_n1_div_ops = {
	.set_div = post_n1_div_set_div,
};

static struct clk_div_ops shadow_n2_div_ops = {
	.set_div = shadow_n2_div_set_div,
};

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1300000000UL,
	.max_rate = 2600000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_8996,
	.c = {
		.dbg_name = "dsi0pll_vco_clk_8996",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi0pll_vco_clk.c),
	},
};

static struct dsi_pll_vco_clk dsi0pll_shadow_vco_clk = {
	.ref_clk_rate = 19200000u,
	.min_rate = 1300000000u,
	.max_rate = 2600000000u,
	.c = {
		.dbg_name = "dsi0pll_shadow_vco_clk",
		.ops = &clk_ops_shadow_dsi_vco,
		CLK_INIT(dsi0pll_shadow_vco_clk.c),
	},
};

static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 1300000000UL,
	.max_rate = 2600000000UL,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_8996,
	.c = {
		.dbg_name = "dsi1pll_vco_clk_8996",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi1pll_vco_clk.c),
	},
};

static struct dsi_pll_vco_clk dsi1pll_shadow_vco_clk = {
	.ref_clk_rate = 19200000u,
	.min_rate = 1300000000u,
	.max_rate = 2600000000u,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_8996,
	.c = {
		.dbg_name = "dsi1pll_shadow_vco_clk",
		.ops = &clk_ops_shadow_dsi_vco,
		CLK_INIT(dsi1pll_shadow_vco_clk.c),
	},
};

static struct div_clk dsi0pll_post_n1_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &post_n1_div_ops,
	.c = {
		.parent = &dsi0pll_vco_clk.c,
		.dbg_name = "dsi0pll_post_n1_div_clk",
		.ops = &post_n1_div_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_post_n1_div_clk.c),
	},
};

static struct div_clk dsi0pll_shadow_post_n1_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &shadow_post_n1_div_ops,
	.c = {
		.parent = &dsi0pll_shadow_vco_clk.c,
		.dbg_name = "dsi0pll_shadow_post_n1_div_clk",
		.ops = &shadow_post_n1_div_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_shadow_post_n1_div_clk.c),
	},
};

static struct div_clk dsi1pll_post_n1_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &post_n1_div_ops,
	.c = {
		.parent = &dsi1pll_vco_clk.c,
		.dbg_name = "dsi1pll_post_n1_div_clk",
		.ops = &post_n1_div_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_post_n1_div_clk.c),
	},
};

static struct div_clk dsi1pll_shadow_post_n1_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &shadow_post_n1_div_ops,
	.c = {
		.parent = &dsi1pll_shadow_vco_clk.c,
		.dbg_name = "dsi1pll_shadow_post_n1_div_clk",
		.ops = &shadow_post_n1_div_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_shadow_post_n1_div_clk.c),
	},
};

static struct div_clk dsi0pll_n2_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &n2_div_ops,
	.c = {
		.parent = &dsi0pll_post_n1_div_clk.c,
		.dbg_name = "dsi0pll_n2_div_clk",
		.ops = &n2_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_n2_div_clk.c),
	},
};

static struct div_clk dsi0pll_shadow_n2_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &shadow_n2_div_ops,
	.c = {
		.parent = &dsi0pll_shadow_post_n1_div_clk.c,
		.dbg_name = "dsi0pll_shadow_n2_div_clk",
		.ops = &shadow_n2_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_shadow_n2_div_clk.c),
	},
};

static struct div_clk dsi1pll_n2_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &n2_div_ops,
	.c = {
		.parent = &dsi1pll_post_n1_div_clk.c,
		.dbg_name = "dsi1pll_n2_div_clk",
		.ops = &n2_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_n2_div_clk.c),
	},
};

static struct div_clk dsi1pll_shadow_n2_div_clk = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &shadow_n2_div_ops,
	.c = {
		.parent = &dsi1pll_shadow_post_n1_div_clk.c,
		.dbg_name = "dsi1pll_shadow_n2_div_clk",
		.ops = &shadow_n2_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_shadow_n2_div_clk.c),
	},
};

static struct div_clk dsi0pll_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_n2_div_clk.c,
		.dbg_name = "dsi0pll_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pixel_clk_src.c),
	},
};

static struct div_clk dsi0pll_shadow_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi0pll_shadow_n2_div_clk.c,
		.dbg_name = "dsi0pll_shadow_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_shadow_pixel_clk_src.c),
	},
};

static struct div_clk dsi1pll_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_n2_div_clk.c,
		.dbg_name = "dsi1pll_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pixel_clk_src.c),
	},
};

static struct div_clk dsi1pll_shadow_pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi1pll_shadow_n2_div_clk.c,
		.dbg_name = "dsi1pll_shadow_pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_shadow_pixel_clk_src.c),
	},
};

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
		.ops = &clk_ops_gen_mux_dsi,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_pixel_clk_mux.c),
	}
};

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
		.ops = &clk_ops_gen_mux_dsi,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_pixel_clk_mux.c),
	}
};

static struct div_clk dsi0pll_byte_clk_src = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi0pll_post_n1_div_clk.c,
		.dbg_name = "dsi0pll_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi0pll_byte_clk_src.c),
	},
};

static struct div_clk dsi0pll_shadow_byte_clk_src = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi0pll_shadow_post_n1_div_clk.c,
		.dbg_name = "dsi0pll_shadow_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi0pll_shadow_byte_clk_src.c),
	},
};

static struct div_clk dsi1pll_byte_clk_src = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi1pll_post_n1_div_clk.c,
		.dbg_name = "dsi1pll_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi1pll_byte_clk_src.c),
	},
};

static struct div_clk dsi1pll_shadow_byte_clk_src = {
	.data = {
		.div = 8,
		.min_div = 8,
		.max_div = 8,
	},
	.c = {
		.parent = &dsi1pll_shadow_post_n1_div_clk.c,
		.dbg_name = "dsi1pll_shadow_byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(dsi1pll_shadow_byte_clk_src.c),
	},
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi0pll_byte_clk_mux.c),
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
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi1pll_byte_clk_mux.c),
	}
};

static struct clk_lookup mdss_dsi_pllcc_8996[] = {
	CLK_LIST(dsi0pll_byte_clk_mux),
	CLK_LIST(dsi0pll_byte_clk_src),
	CLK_LIST(dsi0pll_pixel_clk_mux),
	CLK_LIST(dsi0pll_pixel_clk_src),
	CLK_LIST(dsi0pll_n2_div_clk),
	CLK_LIST(dsi0pll_post_n1_div_clk),
	CLK_LIST(dsi0pll_vco_clk),
	CLK_LIST(dsi0pll_shadow_byte_clk_src),
	CLK_LIST(dsi0pll_shadow_pixel_clk_src),
	CLK_LIST(dsi0pll_shadow_n2_div_clk),
	CLK_LIST(dsi0pll_shadow_post_n1_div_clk),
	CLK_LIST(dsi0pll_shadow_vco_clk),
};

static struct clk_lookup mdss_dsi_pllcc_8996_1[] = {
	CLK_LIST(dsi1pll_byte_clk_mux),
	CLK_LIST(dsi1pll_byte_clk_src),
	CLK_LIST(dsi1pll_pixel_clk_mux),
	CLK_LIST(dsi1pll_pixel_clk_src),
	CLK_LIST(dsi1pll_n2_div_clk),
	CLK_LIST(dsi1pll_post_n1_div_clk),
	CLK_LIST(dsi1pll_vco_clk),
	CLK_LIST(dsi1pll_shadow_byte_clk_src),
	CLK_LIST(dsi1pll_shadow_pixel_clk_src),
	CLK_LIST(dsi1pll_shadow_n2_div_clk),
	CLK_LIST(dsi1pll_shadow_post_n1_div_clk),
	CLK_LIST(dsi1pll_shadow_vco_clk),
};

int dsi_pll_clock_register_8996(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc, ndx;
	struct dsi_pll_db *pdb;

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

	/* hr_oclk3, pixel */
	n2_clk_src_ops = clk_ops_slave_div;
	n2_clk_src_ops.prepare = dsi_pll_div_prepare;

	shadow_n2_clk_src_ops = clk_ops_slave_div;

	/* hr_ockl2, byte, vco pll */
	post_n1_div_clk_src_ops = clk_ops_div;
	post_n1_div_clk_src_ops.prepare = dsi_pll_div_prepare;

	shadow_post_n1_div_clk_src_ops = clk_ops_div;

	byte_clk_src_ops = clk_ops_div;
	byte_clk_src_ops.prepare = dsi_pll_div_prepare;

	clk_ops_gen_mux_dsi = clk_ops_gen_mux;
	clk_ops_gen_mux_dsi.round_rate = parent_round_rate;
	clk_ops_gen_mux_dsi.set_rate = parent_set_rate;

	/* Set client data to mux, div and vco clocks.  */
	if (pll_res->index == DSI_PLL_1) {
		dsi1pll_byte_clk_src.priv = pll_res;
		dsi1pll_pixel_clk_src.priv = pll_res;
		dsi1pll_post_n1_div_clk.priv = pll_res;
		dsi1pll_n2_div_clk.priv = pll_res;
		dsi1pll_vco_clk.priv = pll_res;

		dsi1pll_shadow_byte_clk_src.priv = pll_res;
		dsi1pll_shadow_pixel_clk_src.priv = pll_res;
		dsi1pll_shadow_post_n1_div_clk.priv = pll_res;
		dsi1pll_shadow_n2_div_clk.priv = pll_res;
		dsi1pll_shadow_vco_clk.priv = pll_res;

		pll_res->vco_delay = VCO_DELAY_USEC;
		rc = of_msm_clock_register(pdev->dev.of_node,
				mdss_dsi_pllcc_8996_1,
				ARRAY_SIZE(mdss_dsi_pllcc_8996_1));
	} else {
		dsi0pll_byte_clk_src.priv = pll_res;
		dsi0pll_pixel_clk_src.priv = pll_res;
		dsi0pll_post_n1_div_clk.priv = pll_res;
		dsi0pll_n2_div_clk.priv = pll_res;
		dsi0pll_vco_clk.priv = pll_res;

		dsi0pll_shadow_byte_clk_src.priv = pll_res;
		dsi0pll_shadow_pixel_clk_src.priv = pll_res;
		dsi0pll_shadow_post_n1_div_clk.priv = pll_res;
		dsi0pll_shadow_n2_div_clk.priv = pll_res;
		dsi0pll_shadow_vco_clk.priv = pll_res;

		pll_res->vco_delay = VCO_DELAY_USEC;
		rc = of_msm_clock_register(pdev->dev.of_node,
				mdss_dsi_pllcc_8996,
				ARRAY_SIZE(mdss_dsi_pllcc_8996));
	}

	if (!rc) {
		pr_info("Registered DSI PLL ndx=%d clocks successfully\n",
						pll_res->index);
	}

	return rc;
}
