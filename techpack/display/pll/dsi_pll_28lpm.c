// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <dt-bindings/clock/mdss-28nm-pll-clk.h>

#include "pll_drv.h"
#include "dsi_pll.h"
#include "dsi_pll_28nm.h"

#define VCO_DELAY_USEC			1000

enum {
	DSI_PLL_0,
	DSI_PLL_1,
	DSI_PLL_MAX
};

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

static void dsi_pll_sw_reset(struct mdss_pll_resources *rsc)
{
	/*
	 * DSI PLL software reset. Add HW recommended delays after toggling
	 * the software reset bit off and back on.
	 */
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x01);
	ndelay(500);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_TEST_CFG, 0x00);
}

static void dsi_pll_toggle_lock_detect(
				struct mdss_pll_resources *rsc)
{
	/* DSI PLL toggle lock detect setting */
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x04);
	ndelay(500);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_LKDET_CFG2, 0x05);
	udelay(512);
}

static int dsi_pll_check_lock_status(
				struct mdss_pll_resources *rsc)
{
	int rc = 0;

	rc = dsi_pll_lock_status(rsc);
	if (rc)
		pr_debug("PLL Locked\n");
	else
		pr_err("PLL failed to lock\n");

	return rc;
}


static int dsi_pll_enable_seq_gf2(struct mdss_pll_resources *rsc)
{
	int pll_locked = 0;

	dsi_pll_sw_reset(rsc);

	/*
	 * GF PART 2 PLL power up sequence.
	 * Add necessary delays recommended by hardware.
	 */
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG1, 0x04);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
	udelay(3);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
	udelay(500);

	dsi_pll_toggle_lock_detect(rsc);

	pll_locked = dsi_pll_check_lock_status(rsc);
	return pll_locked ? 0 : -EINVAL;
}

static int dsi_pll_enable_seq_gf1(struct mdss_pll_resources *rsc)
{
	int pll_locked = 0;

	dsi_pll_sw_reset(rsc);
	/*
	 * GF PART 1 PLL power up sequence.
	 * Add necessary delays recommended by hardware.
	 */

	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG1, 0x14);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
	udelay(3);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
	udelay(500);

	dsi_pll_toggle_lock_detect(rsc);

	pll_locked = dsi_pll_check_lock_status(rsc);
	return pll_locked ? 0 : -EINVAL;
}

static int dsi_pll_enable_seq_tsmc(struct mdss_pll_resources *rsc)
{
	int pll_locked = 0;

	dsi_pll_sw_reset(rsc);
	/*
	 * TSMC PLL power up sequence.
	 * Add necessary delays recommended by hardware.
	 */
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_CAL_CFG1, 0x34);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x01);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x05);
	MDSS_PLL_REG_W(rsc->pll_base,
			DSI_PHY_PLL_UNIPHY_PLL_GLB_CFG, 0x0f);
	udelay(500);

	dsi_pll_toggle_lock_detect(rsc);

	pll_locked = dsi_pll_check_lock_status(rsc);
	return pll_locked ? 0 : -EINVAL;
}

static struct regmap_config dsi_pll_28lpm_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xF4,
};

static struct regmap_bus analog_postdiv_regmap_bus = {
	.reg_write = analog_postdiv_reg_write,
	.reg_read = analog_postdiv_reg_read,
};

static struct regmap_bus byteclk_src_mux_regmap_bus = {
	.reg_write = byteclk_mux_write_sel,
	.reg_read = byteclk_mux_read_sel,
};

static struct regmap_bus pclk_src_regmap_bus = {
	.reg_write = pixel_clk_set_div,
	.reg_read = pixel_clk_get_div,
};

static const struct clk_ops clk_ops_vco_28lpm = {
	.recalc_rate = vco_28nm_recalc_rate,
	.set_rate = vco_28nm_set_rate,
	.round_rate = vco_28nm_round_rate,
	.prepare = vco_28nm_prepare,
	.unprepare = vco_28nm_unprepare,
};

static struct dsi_pll_vco_clk dsi0pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 350000000UL,
	.max_rate = 750000000UL,
	.pll_en_seq_cnt = 9,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_tsmc,
	.pll_enable_seqs[1] = dsi_pll_enable_seq_tsmc,
	.pll_enable_seqs[2] = dsi_pll_enable_seq_tsmc,
	.pll_enable_seqs[3] = dsi_pll_enable_seq_gf1,
	.pll_enable_seqs[4] = dsi_pll_enable_seq_gf1,
	.pll_enable_seqs[5] = dsi_pll_enable_seq_gf1,
	.pll_enable_seqs[6] = dsi_pll_enable_seq_gf2,
	.pll_enable_seqs[7] = dsi_pll_enable_seq_gf2,
	.pll_enable_seqs[8] = dsi_pll_enable_seq_gf2,
	.lpfr_lut_size = 10,
	.lpfr_lut = lpfr_lut_struct,
	.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_vco_clk",
			.parent_names = (const char *[]){"cxo"},
			.num_parents = 1,
			.ops = &clk_ops_vco_28lpm,
			.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct dsi_pll_vco_clk dsi1pll_vco_clk = {
	.ref_clk_rate = 19200000UL,
	.min_rate = 350000000UL,
	.max_rate = 750000000UL,
	.pll_en_seq_cnt = 9,
	.pll_enable_seqs[0] = dsi_pll_enable_seq_tsmc,
	.pll_enable_seqs[1] = dsi_pll_enable_seq_tsmc,
	.pll_enable_seqs[2] = dsi_pll_enable_seq_tsmc,
	.pll_enable_seqs[3] = dsi_pll_enable_seq_gf1,
	.pll_enable_seqs[4] = dsi_pll_enable_seq_gf1,
	.pll_enable_seqs[5] = dsi_pll_enable_seq_gf1,
	.pll_enable_seqs[6] = dsi_pll_enable_seq_gf2,
	.pll_enable_seqs[7] = dsi_pll_enable_seq_gf2,
	.pll_enable_seqs[8] = dsi_pll_enable_seq_gf2,
	.lpfr_lut_size = 10,
	.lpfr_lut = lpfr_lut_struct,
	.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_vco_clk",
			.parent_names = (const char *[]){"cxo"},
			.num_parents = 1,
			.ops = &clk_ops_vco_28lpm,
			.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap_div dsi0pll_analog_postdiv = {
	.reg = DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG,
	.shift = 0,
	.width = 4,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_analog_postdiv",
			.parent_names = (const char *[]){"dsi0pll_vco_clk"},
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_analog_postdiv = {
	.reg = DSI_PHY_PLL_UNIPHY_PLL_POSTDIV1_CFG,
	.shift = 0,
	.width = 4,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_analog_postdiv",
			.parent_names = (const char *[]){"dsi1pll_vco_clk"},
			.num_parents = 1,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_fixed_factor dsi0pll_indirect_path_src = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_indirect_path_src",
		.parent_names = (const char *[]){"dsi0pll_analog_postdiv"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_indirect_path_src = {
	.div = 2,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_indirect_path_src",
		.parent_names = (const char *[]){"dsi1pll_analog_postdiv"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_mux dsi0pll_byteclk_src_mux = {
	.reg = DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG,
	.shift = 1,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_byteclk_src_mux",
			.parent_names = (const char *[]){
				"dsi0pll_vco_clk",
				"dsi0pll_indirect_path_src"},
			.num_parents = 2,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_regmap_mux dsi1pll_byteclk_src_mux = {
	.reg = DSI_PHY_PLL_UNIPHY_PLL_VREG_CFG,
	.shift = 1,
	.width = 1,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_byteclk_src_mux",
			.parent_names = (const char *[]){
				"dsi1pll_vco_clk",
				"dsi1pll_indirect_path_src"},
			.num_parents = 2,
			.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
			.ops = &clk_regmap_mux_closest_ops,
		},
	},
};

static struct clk_fixed_factor dsi0pll_byteclk_src = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi0pll_byteclk_src",
		.parent_names = (const char *[]){
				"dsi0pll_byteclk_src_mux"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_fixed_factor dsi1pll_byteclk_src = {
	.div = 4,
	.mult = 1,
	.hw.init = &(struct clk_init_data){
		.name = "dsi1pll_byteclk_src",
		.parent_names = (const char *[]){
				"dsi1pll_byteclk_src_mux"},
		.num_parents = 1,
		.flags = (CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT),
		.ops = &clk_fixed_factor_ops,
	},
};

static struct clk_regmap_div dsi0pll_pclk_src = {
	.reg = DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG,
	.shift = 0,
	.width = 8,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi0pll_pclk_src",
			.parent_names = (const char *[]){"dsi0pll_vco_clk"},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_regmap_div dsi1pll_pclk_src = {
	.reg = DSI_PHY_PLL_UNIPHY_PLL_POSTDIV3_CFG,
	.shift = 0,
	.width = 8,
	.clkr = {
		.hw.init = &(struct clk_init_data){
			.name = "dsi1pll_pclk_src",
			.parent_names = (const char *[]){"dsi1pll_vco_clk"},
			.num_parents = 1,
			.flags = CLK_GET_RATE_NOCACHE,
			.ops = &clk_regmap_div_ops,
		},
	},
};

static struct clk_hw *mdss_dsi_pllcc_28lpm[] = {
	[VCO_CLK_0] = &dsi0pll_vco_clk.hw,
	[ANALOG_POSTDIV_0_CLK] = &dsi0pll_analog_postdiv.clkr.hw,
	[INDIRECT_PATH_SRC_0_CLK] = &dsi0pll_indirect_path_src.hw,
	[BYTECLK_SRC_MUX_0_CLK] = &dsi0pll_byteclk_src_mux.clkr.hw,
	[BYTECLK_SRC_0_CLK] = &dsi0pll_byteclk_src.hw,
	[PCLK_SRC_0_CLK] = &dsi0pll_pclk_src.clkr.hw,
	[VCO_CLK_1] = &dsi1pll_vco_clk.hw,
	[ANALOG_POSTDIV_1_CLK] = &dsi1pll_analog_postdiv.clkr.hw,
	[INDIRECT_PATH_SRC_1_CLK] = &dsi1pll_indirect_path_src.hw,
	[BYTECLK_SRC_MUX_1_CLK] = &dsi1pll_byteclk_src_mux.clkr.hw,
	[BYTECLK_SRC_1_CLK] = &dsi1pll_byteclk_src.hw,
	[PCLK_SRC_1_CLK] = &dsi1pll_pclk_src.clkr.hw,
};

int dsi_pll_clock_register_28lpm(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = 0, ndx, i;
	struct clk *clk;
	struct clk_onecell_data *clk_data;
	int num_clks = ARRAY_SIZE(mdss_dsi_pllcc_28lpm);
	struct regmap *rmap;

	int const ssc_freq_min = 30000; /* min. recommended freq. value */
	int const ssc_freq_max = 33000; /* max. recommended freq. value */
	int const ssc_ppm_max = 5000; /* max. recommended ppm */

	ndx = pll_res->index;

	if (ndx >= DSI_PLL_MAX) {
		pr_err("pll index(%d) NOT supported\n", ndx);
		return -EINVAL;
	}

	pll_res->vco_delay = VCO_DELAY_USEC;

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

	clk_data = devm_kzalloc(&pdev->dev, sizeof(*clk_data),
					GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->clks = devm_kcalloc(&pdev->dev, num_clks,
				sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks)
		return -ENOMEM;

	clk_data->clk_num = num_clks;

	/* Establish client data */
	if (ndx == 0) {
		rmap = devm_regmap_init(&pdev->dev, &byteclk_src_mux_regmap_bus,
				pll_res, &dsi_pll_28lpm_config);
		if (IS_ERR(rmap)) {
			pr_err("regmap init failed for DSI clock:%d\n",
					pll_res->index);
			return -EINVAL;
		}
		dsi0pll_byteclk_src_mux.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &analog_postdiv_regmap_bus,
				pll_res, &dsi_pll_28lpm_config);
		if (IS_ERR(rmap)) {
			pr_err("regmap init failed for DSI clock:%d\n",
					pll_res->index);
			return -EINVAL;
		}
		dsi0pll_analog_postdiv.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &pclk_src_regmap_bus,
				pll_res, &dsi_pll_28lpm_config);
		if (IS_ERR(rmap)) {
			pr_err("regmap init failed for DSI clock:%d\n",
					pll_res->index);
			return -EINVAL;
		}
		dsi0pll_pclk_src.clkr.regmap = rmap;

		dsi0pll_vco_clk.priv = pll_res;
		for (i = VCO_CLK_0; i <= PCLK_SRC_0_CLK; i++) {
			clk = devm_clk_register(&pdev->dev,
						mdss_dsi_pllcc_28lpm[i]);
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
		rmap = devm_regmap_init(&pdev->dev, &byteclk_src_mux_regmap_bus,
				pll_res, &dsi_pll_28lpm_config);
		if (IS_ERR(rmap)) {
			pr_err("regmap init failed for DSI clock:%d\n",
					pll_res->index);
			return -EINVAL;
		}
		dsi1pll_byteclk_src_mux.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &analog_postdiv_regmap_bus,
				pll_res, &dsi_pll_28lpm_config);
		if (IS_ERR(rmap)) {
			pr_err("regmap init failed for DSI clock:%d\n",
					pll_res->index);
			return -EINVAL;
		}
		dsi1pll_analog_postdiv.clkr.regmap = rmap;

		rmap = devm_regmap_init(&pdev->dev, &pclk_src_regmap_bus,
				pll_res, &dsi_pll_28lpm_config);
		if (IS_ERR(rmap)) {
			pr_err("regmap init failed for DSI clock:%d\n",
					pll_res->index);
			return -EINVAL;
		}
		dsi1pll_pclk_src.clkr.regmap = rmap;

		dsi1pll_vco_clk.priv = pll_res;
		for (i = VCO_CLK_1; i <= PCLK_SRC_1_CLK; i++) {
			clk = devm_clk_register(&pdev->dev,
						mdss_dsi_pllcc_28lpm[i]);
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
				ndx);

		return rc;
	}

clk_register_fail:
	return rc;
}
