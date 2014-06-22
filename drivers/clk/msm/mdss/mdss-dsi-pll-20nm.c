/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-8994.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"

#define VCO_DELAY_USEC		1

static struct clk_ops bypass_lp_div_mux_clk_ops;
static struct clk_ops pixel_clk_src_ops;
static struct clk_ops byte_clk_src_ops;
static struct clk_ops ndiv_clk_ops;

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

	rc = pll_20nm_vco_set_rate(vco, rate);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int dsi_pll_enable_seq_8994(struct mdss_pll_resources *dsi_pll_res)
{
	int rc = 0;
	int pll_locked;

	/*
	 * PLL power up sequence.
	 * Add necessary delays recommeded by hardware.
	 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLLLOCK_CMP_EN, 0x0D);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_CNTRL, 0x07);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				MMSS_DSI_PHY_PLL_PLL_BKG_KVCO_CAL_EN, 0x00);
	udelay(500);
	dsi_pll_20nm_phy_ctrl_config(dsi_pll_res, 0x200); /* Ctrl 0 */
	/*
	 * Make sure that the PHY controller configurations are completed
	 * before checking the pll lock status.
	 */
	wmb();
	pll_locked = dsi_20nm_pll_lock_status(dsi_pll_res);
	if (!pll_locked) {
		pr_err("DSI PLL lock failed\n");
		rc = -EINVAL;
	} else {
		pr_debug("DSI PLL Lock success\n");
	}

	return rc;
}

/* Op structures */

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

static struct dsi_pll_vco_clk dsi_vco_clk_8994 = {
	.ref_clk_rate = 19200000,
	.min_rate = 1000000000,
	.max_rate = 2000000000,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_8994,
	.c = {
		.dbg_name = "dsi_vco_clk_8994",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi_vco_clk_8994.c),
	},
};

static struct div_clk ndiv_clk_8994 = {
	.data = {
		.max_div = 15,
		.min_div = 1,
	},
	.ops = &ndiv_ops,
	.c = {
		.parent = &dsi_vco_clk_8994.c,
		.dbg_name = "ndiv_clk_8994",
		.ops = &ndiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(ndiv_clk_8994.c),
	},
};

static struct div_clk indirect_path_div2_clk_8994 = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &ndiv_clk_8994.c,
		.dbg_name = "indirect_path_div2_clk_8994",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(indirect_path_div2_clk_8994.c),
	},
};

static struct div_clk hr_oclk3_div_clk_8994 = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &hr_oclk3_div_ops,
	.c = {
		.parent = &dsi_vco_clk_8994.c,
		.dbg_name = "hr_oclk3_div_clk_8994",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(hr_oclk3_div_clk_8994.c),
	},
};

static struct div_clk pixel_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &hr_oclk3_div_clk_8994.c,
		.dbg_name = "pixel_clk_src",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(pixel_clk_src.c),
	},
};

static struct mux_clk bypass_lp_div_mux_8994 = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi_vco_clk_8994.c, 0},
		{&indirect_path_div2_clk_8994.c, 1},
	},
	.ops = &bypass_lp_div_mux_ops,
	.c = {
		.parent = &dsi_vco_clk_8994.c,
		.dbg_name = "bypass_lp_div_mux_8994",
		.ops = &bypass_lp_div_mux_clk_ops,
		CLK_INIT(bypass_lp_div_mux_8994.c),
	},
};

static struct div_clk fixed_hr_oclk2_div_clk_8994 = {
	.ops = &fixed_hr_oclk2_div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &bypass_lp_div_mux_8994.c,
		.dbg_name = "fixed_hr_oclk2_div_clk_8994",
		.ops = &byte_clk_src_ops,
		CLK_INIT(fixed_hr_oclk2_div_clk_8994.c),
	},
};

static struct div_clk byte_clk_src = {
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &fixed_hr_oclk2_div_clk_8994.c,
		.dbg_name = "byte_clk_src",
		.ops = &clk_ops_div,
		CLK_INIT(byte_clk_src.c),
	},
};

static struct clk_lookup mdss_dsi_pllcc_8994[] = {
	CLK_LIST(pixel_clk_src),
	CLK_LIST(byte_clk_src),
	CLK_LIST(fixed_hr_oclk2_div_clk_8994),
	CLK_LIST(bypass_lp_div_mux_8994),
	CLK_LIST(hr_oclk3_div_clk_8994),
	CLK_LIST(indirect_path_div2_clk_8994),
	CLK_LIST(ndiv_clk_8994),
	CLK_LIST(dsi_vco_clk_8994),
};

int dsi_pll_clock_register_20nm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	if (!pll_res || !pll_res->pll_base) {
		pr_err("Invalid PLL resources\n");
		return -EPROBE_DEFER;
	}

	/* Set client data to mux, div and vco clocks */
	byte_clk_src.priv = pll_res;
	pixel_clk_src.priv = pll_res;
	bypass_lp_div_mux_8994.priv = pll_res;
	indirect_path_div2_clk_8994.priv = pll_res;
	ndiv_clk_8994.priv = pll_res;
	fixed_hr_oclk2_div_clk_8994.priv = pll_res;
	hr_oclk3_div_clk_8994.priv = pll_res;
	dsi_vco_clk_8994.priv = pll_res;
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

	if (pll_res->target_id == MDSS_PLL_TARGET_8994) {
		rc = of_msm_clock_register(pdev->dev.of_node,
			mdss_dsi_pllcc_8994, ARRAY_SIZE(mdss_dsi_pllcc_8994));
		if (rc) {
			pr_err("Clock register failed\n");
			rc = -EPROBE_DEFER;
		}
	} else {
		pr_err("Invalid target ID\n");
		rc = -EINVAL;
	}

	if (!rc)
		pr_info("Registered DSI PLL clocks successfully\n");

	return rc;
}
