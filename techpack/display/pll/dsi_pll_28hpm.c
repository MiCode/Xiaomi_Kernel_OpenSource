// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/clk/msm-clk.h>
#include <linux/clk/msm-clock-generic.h>
#include <dt-bindings/clock/msm-clocks-8974.h>

#include "pll_drv.h"
#include "dsi_pll.h"

#define VCO_DELAY_USEC		1

static struct clk_div_ops fixed_2div_ops;
static const struct clk_ops byte_mux_clk_ops;
static const struct clk_ops pixel_clk_src_ops;
static const struct clk_ops byte_clk_src_ops;
static const struct clk_ops analog_postdiv_clk_ops;
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

	rc = vco_set_rate(vco, rate);

	mdss_pll_resource_enable(dsi_pll_res, false);
	return rc;
}

static int dsi_pll_enable_seq_8974(struct mdss_pll_resources *dsi_pll_res)
{
	int i, rc = 0;
	int pll_locked;

	dsi_pll_software_reset(dsi_pll_res);

	/*
	 * PLL power up sequence.
	 * Add necessary delays recommeded by hardware.
	 */
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
	udelay(1);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
	udelay(200);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x07);
	udelay(500);
	MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
	udelay(500);

	for (i = 0; i < 2; i++) {
		udelay(100);
		/* DSI Uniphy lock detect setting */
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0c);
		udelay(100);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
				DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x0d);

		pll_locked = dsi_pll_lock_status(dsi_pll_res);
		if (pll_locked)
			break;

		dsi_pll_software_reset(dsi_pll_res);
		/*
		 * PLL power up sequence.
		 * Add necessary delays recommeded by hardware.
		 */
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x1);
		udelay(1);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x5);
		udelay(200);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x7);
		udelay(250);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x5);
		udelay(200);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x7);
		udelay(500);
		MDSS_PLL_REG_W(dsi_pll_res->pll_base,
					DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0xf);
		udelay(500);

	}

	if (!pll_locked) {
		pr_err("DSI PLL lock failed\n");
		rc = -EINVAL;
	} else {
		pr_debug("DSI PLL Lock success\n");
	}

	return rc;
}

/* Op structures */

static const struct clk_ops clk_ops_dsi_vco = {
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

static struct dsi_pll_vco_clk dsi_vco_clk_8974 = {
	.ref_clk_rate = 19200000,
	.min_rate = 350000000,
	.max_rate = 750000000,
	.pll_en_seq_cnt = 3,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_8974,
	.pll_enable_seqs[1] = dsi_pll_enable_seq_8974,
	.pll_enable_seqs[2] = dsi_pll_enable_seq_8974,
	.lpfr_lut_size = 10,
	.lpfr_lut = lpfr_lut_struct,
	.c = {
		.dbg_name = "dsi_vco_clk_8974",
		.ops = &clk_ops_dsi_vco,
		CLK_INIT(dsi_vco_clk_8974.c),
	},
};

static struct div_clk analog_postdiv_clk_8974 = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &analog_postdiv_ops,
	.c = {
		.parent = &dsi_vco_clk_8974.c,
		.dbg_name = "analog_postdiv_clk",
		.ops = &analog_postdiv_clk_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(analog_postdiv_clk_8974.c),
	},
};

static struct div_clk indirect_path_div2_clk_8974 = {
	.ops = &fixed_2div_ops,
	.data = {
		.div = 2,
		.min_div = 2,
		.max_div = 2,
	},
	.c = {
		.parent = &analog_postdiv_clk_8974.c,
		.dbg_name = "indirect_path_div2_clk",
		.ops = &clk_ops_div,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(indirect_path_div2_clk_8974.c),
	},
};

static struct div_clk pixel_clk_src_8974 = {
	.data = {
		.max_div = 255,
		.min_div = 1,
	},
	.ops = &digital_postdiv_ops,
	.c = {
		.parent = &dsi_vco_clk_8974.c,
		.dbg_name = "pixel_clk_src_8974",
		.ops = &pixel_clk_src_ops,
		.flags = CLKFLAG_NO_RATE_CACHE,
		CLK_INIT(pixel_clk_src_8974.c),
	},
};

static struct mux_clk byte_mux_8974 = {
	.num_parents = 2,
	.parents = (struct clk_src[]){
		{&dsi_vco_clk_8974.c, 0},
		{&indirect_path_div2_clk_8974.c, 1},
	},
	.ops = &byte_mux_ops,
	.c = {
		.parent = &dsi_vco_clk_8974.c,
		.dbg_name = "byte_mux_8974",
		.ops = &byte_mux_clk_ops,
		CLK_INIT(byte_mux_8974.c),
	},
};

static struct div_clk byte_clk_src_8974 = {
	.ops = &fixed_4div_ops,
	.data = {
		.min_div = 4,
		.max_div = 4,
	},
	.c = {
		.parent = &byte_mux_8974.c,
		.dbg_name = "byte_clk_src_8974",
		.ops = &byte_clk_src_ops,
		CLK_INIT(byte_clk_src_8974.c),
	},
};

static struct clk_lookup mdss_dsi_pllcc_8974[] = {
	CLK_LOOKUP_OF("pixel_src", pixel_clk_src_8974,
						"fd8c0000.qcom,mmsscc-mdss"),
	CLK_LOOKUP_OF("byte_src", byte_clk_src_8974,
						"fd8c0000.qcom,mmsscc-mdss"),
};

int dsi_pll_clock_register_hpm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc;

	/* Set client data to mux, div and vco clocks */
	byte_clk_src_8974.priv = pll_res;
	pixel_clk_src_8974.priv = pll_res;
	byte_mux_8974.priv = pll_res;
	indirect_path_div2_clk_8974.priv = pll_res;
	analog_postdiv_clk_8974.priv = pll_res;
	dsi_vco_clk_8974.priv = pll_res;
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

	if (pll_res->target_id == MDSS_PLL_TARGET_8974) {
		rc = of_msm_clock_register(pdev->dev.of_node,
			mdss_dsi_pllcc_8974, ARRAY_SIZE(mdss_dsi_pllcc_8974));
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
