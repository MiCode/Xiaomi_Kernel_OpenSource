/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <dt-bindings/clock/msm-clocks-8976.h>

#include "mdss-pll.h"
#include "mdss-dsi-pll.h"

#define VCO_DELAY_USEC		1

static struct clk_div_ops fixed_2div_ops;
static struct clk_ops byte_mux_clk_ops;
static struct clk_ops pixel_clk_src_ops;
static struct clk_ops byte_clk_src_ops;
static struct clk_ops analog_postdiv_clk_ops;
static struct lpfr_cfg lpfr_lut_struct[] = {
	{479500000, 8},
	{480000000, 11},
	{575500000, 8},
	{576000000, 12},
	{610500000, 8},
	{659500000, 9},
	{671500000, 10},
	{672000000, 14},
	{708500000, 10},
	{750000000, 11},
};

static void dsi_pll_software_reset(struct mdss_pll_resources *dsi_pll_res)
{
	/*
	 * Add HW recommended delays after toggling the software
	 * reset bit off and back on.
	 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x01);
	udelay(1);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x00);
	udelay(1);
}

static int vco_set_rate_hpm(struct clk *c, unsigned long rate)
{
	int rc;
	struct dsi_pll_vco_clk *vco = to_vco_clk(c);
	struct mdss_pll_resources *dsi_pll_res = vco->priv;

	rc = mdss_pll_resource_enable(dsi_pll_res, true);
	if (rc) {
		pr_err("Failed to enable mdss dsi pll resources\n");
		return rc;
	}

	dsi_pll_software_reset(dsi_pll_res);

	rc = vco_set_rate(vco, rate);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int dsi_pll_enable_seq(struct mdss_pll_resources *dsi_pll_res)
{
	int rc = 0;

	/*
	 * PLL power up sequence
	 * Add necessary delays recommended by hardware
	 */

	/* DSI Uniphy lock detect setting */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0d);
	/* DSI Uniphy PLL Calibration setting */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG1, 0x34);
	/* DSI Uniphy PLL lock detect mcnt */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG0, 0x10);
	/* DSI Uniphy PLL lock detect wait time */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG1, 0x1a);
	/* make sure the above register writes happen */
	wmb();
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
	/* make sure the above register writes happen */
	wmb();
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
	udelay(30);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x07);
	udelay(50);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
		DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
	udelay(800);

	if (!dsi_pll_lock_status(dsi_pll_res)) {
		pr_err("DSI PLL lock failed\n");
		rc = -EINVAL;
	} else {
		pr_debug("DSI PLL Lock success\n");
	}

	return rc;
}

/* Op structures - common for both DSI PLL0 and DSI PLL1 */
static struct clk_ops clk_ops_dsi_vco = {
	.set_rate = vco_set_rate_hpm,
	.round_rate = vco_round_rate,
	.handoff = vco_handoff,
	.prepare = vco_prepare,
	.unprepare = vco_unprepare,
};

static struct clk_div_ops fixed_4div_ops = {
	.set_div = fixed_4div_set_div,
	.get_div = fixed_4div_get_div,
};

static struct clk_div_ops analog_postdiv_ops = {
	.set_div = analog_set_div,
	.get_div = analog_get_div,
};

static struct clk_div_ops digital_postdiv_ops = {
	.set_div = digital_set_div,
	.get_div = digital_get_div,
};

static struct clk_mux_ops byte_mux_ops = {
	.set_mux_sel = set_byte_mux_sel,
	.get_mux_sel = get_byte_mux_sel,
};

/* DSI PLL0 clock structures */
static struct dsi_pll_vco_clk dsi_pll0_vco_clk = {
	.ref_clk_rate = 19200000,
	.min_rate = 350000000,
	.max_rate = 750000000,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq,
	.lpfr_lut_size = 10,
	.lpfr_lut = lpfr_lut_struct,
	.c = {
		.dbg_name = "dsi_pll0_vco_clk",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi_pll0_vco_clk.c),
	},
};

static struct div_clk dsi_pll0_analog_postdiv_clk = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &analog_postdiv_ops,
	.c = {
		.parent = &dsi_pll0_vco_clk.c,
		.dbg_name = "dsi_pll0_analog_postdiv_clk",
		.ops = &analog_postdiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi_pll0_analog_postdiv_clk.c),
	},
};

static struct div_clk dsi_pll0_indirect_path_div2_clk = {
	.ops = &fixed_2div_ops,
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi_pll0_analog_postdiv_clk.c,
		.dbg_name = "dsi_pll0_indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi_pll0_indirect_path_div2_clk.c),
	},
};

static struct div_clk dsi_pll0_pixel_clk_src = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &digital_postdiv_ops,
	.c = {
		.parent = &dsi_pll0_vco_clk.c,
		.dbg_name = "dsi_pll0_pixel_clk_src",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi_pll0_pixel_clk_src.c),
	},
};

static struct mux_clk dsi_pll0_byte_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi_pll0_vco_clk.c, 0},
		{&dsi_pll0_indirect_path_div2_clk.c, 1},
	},
	.ops = &byte_mux_ops,
	.c = {
		.parent = &dsi_pll0_vco_clk.c,
		.dbg_name = "dsi_pll0_byte_mux",
		.ops = &byte_mux_clk_ops,
		CLK_INIT(dsi_pll0_byte_mux.c),
	},
};

static struct div_clk dsi_pll0_byte_clk_src = {
	.ops = &fixed_4div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi_pll0_byte_mux.c,
		.dbg_name = "dsi_pll0_byte_clk_src",
		.ops = &byte_clk_src_ops,
		CLK_INIT(dsi_pll0_byte_clk_src.c),
	},
};

/* DSI PLL1 clock structures */
static struct dsi_pll_vco_clk dsi_pll1_vco_clk = {
	.ref_clk_rate = 19200000,
	.min_rate = 350000000,
	.max_rate = 750000000,
	.pll_en_seq_cnt = 1,
	.pll_enable_seqs[0] = dsi_pll_enable_seq,
	.lpfr_lut_size = 10,
	.lpfr_lut = lpfr_lut_struct,
	.c = {
		.dbg_name = "dsi_pll1_vco_clk",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi_pll1_vco_clk.c),
	},
};

static struct div_clk dsi_pll1_analog_postdiv_clk = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &analog_postdiv_ops,
	.c = {
		.parent = &dsi_pll1_vco_clk.c,
		.dbg_name = "dsi_pll1_analog_postdiv_clk",
		.ops = &analog_postdiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi_pll1_analog_postdiv_clk.c),
	},
};

static struct div_clk dsi_pll1_indirect_path_div2_clk = {
	.ops = &fixed_2div_ops,
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &dsi_pll1_analog_postdiv_clk.c,
		.dbg_name = "dsi_pll1_indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi_pll1_indirect_path_div2_clk.c),
	},
};

static struct div_clk dsi_pll1_pixel_clk_src = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &digital_postdiv_ops,
	.c = {
		.parent = &dsi_pll1_vco_clk.c,
		.dbg_name = "dsi_pll1_pixel_clk_src",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(dsi_pll1_pixel_clk_src.c),
	},
};

static struct mux_clk dsi_pll1_byte_mux = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi_pll1_vco_clk.c, 0},
		{&dsi_pll1_indirect_path_div2_clk.c, 1},
	},
	.ops = &byte_mux_ops,
	.c = {
		.parent = &dsi_pll1_vco_clk.c,
		.dbg_name = "dsi_pll1_byte_mux",
		.ops = &byte_mux_clk_ops,
		CLK_INIT(dsi_pll1_byte_mux.c),
	},
};

static struct div_clk dsi_pll1_byte_clk_src = {
	.ops = &fixed_4div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &dsi_pll1_byte_mux.c,
		.dbg_name = "dsi_pll1_byte_clk_src",
		.ops = &byte_clk_src_ops,
		CLK_INIT(dsi_pll1_byte_clk_src.c),
	},
};

static struct clk_lookup dsi_pll0_cc[] = {
	CLK_LIST(dsi_pll0_pixel_clk_src),
	CLK_LIST(dsi_pll0_byte_clk_src),
};

static struct clk_lookup dsi_pll1_cc[] = {
	CLK_LIST(dsi_pll1_pixel_clk_src),
	CLK_LIST(dsi_pll1_byte_clk_src),
};

int dsi_pll_clock_register_hpm(struct platform_device *pdev,
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

	/*
	 * Set client data to mux, div and vco clocks
	 * This needs to be done for PLL0 and PLL1 separately
	 * based on the cell index.
	 */
	if (!pll_res->index) {
		dsi_pll0_byte_clk_src.priv = pll_res;
		dsi_pll0_pixel_clk_src.priv = pll_res;
		dsi_pll0_byte_mux.priv = pll_res;
		dsi_pll0_indirect_path_div2_clk.priv = pll_res;
		dsi_pll0_analog_postdiv_clk.priv = pll_res;
		dsi_pll0_vco_clk.priv = pll_res;
	} else {
		dsi_pll1_byte_clk_src.priv = pll_res;
		dsi_pll1_pixel_clk_src.priv = pll_res;
		dsi_pll1_byte_mux.priv = pll_res;
		dsi_pll1_indirect_path_div2_clk.priv = pll_res;
		dsi_pll1_analog_postdiv_clk.priv = pll_res;
		dsi_pll1_vco_clk.priv = pll_res;
	}
	pll_res->vco_delay = VCO_DELAY_USEC;

	/* Set clock source operations */
	pixel_clk_src_ops = clk_ops_slave_div;
	pixel_clk_src_ops.prepare = dsi_pll_div_prepare;

	analog_postdiv_clk_ops = clk_ops_div;
	analog_postdiv_clk_ops.prepare = dsi_pll_div_prepare;

	byte_clk_src_ops = clk_ops_div;
	byte_clk_src_ops.prepare = dsi_pll_div_prepare;

	byte_mux_clk_ops = clk_ops_gen_mux;
	byte_mux_clk_ops.prepare = dsi_pll_mux_prepare;

	if ((pll_res->target_id == MDSS_PLL_TARGET_8974) ||
	    (pll_res->target_id == MDSS_PLL_TARGET_8976)) {
		if (!pll_res->index)
			rc = of_msm_clock_register(pdev->dev.of_node,
				dsi_pll0_cc, ARRAY_SIZE(dsi_pll0_cc));
		else
			rc = of_msm_clock_register(pdev->dev.of_node,
				dsi_pll1_cc, ARRAY_SIZE(dsi_pll1_cc));
		if (rc) {
			pr_err("Clock register failed\n");
			rc = -EPROBE_DEFER;
		}
	} else {
		pr_err("Invalid target ID\n");
		rc = -EINVAL;
	}

	if (!rc)
		pr_info("Registered DSI PLL:%d clocks successfully\n",
			pll_res->index);

	return rc;
}
