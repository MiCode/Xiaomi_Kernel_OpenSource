// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <dt-bindings/clock/qcom,evacc-sun.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "common.h"
#include "reset.h"
#include "vdd-level.h"

static DEFINE_VDD_REGULATORS(vdd_mm, VDD_NOMINAL + 1, 1, vdd_corner);
static DEFINE_VDD_REGULATORS(vdd_mxc, VDD_NOMINAL + 1, 1, vdd_corner);

static struct clk_vdd_class *eva_cc_sun_regulators[] = {
	&vdd_mm,
	&vdd_mxc,
};

enum {
	P_BI_TCXO,
	P_EVA_CC_PLL0_OUT_MAIN,
	P_SLEEP_CLK,
};

static const struct pll_vco taycan_elu_vco[] = {
	{ 249600000, 2500000000, 0 },
};

static const struct alpha_pll_config eva_cc_pll0_config = {
	.l = 0x36,
	.cal_l = 0x44,
	.alpha = 0xb000,
	.config_ctl_val = 0x19660387,
	.config_ctl_hi_val = 0x098060a0,
	.config_ctl_hi1_val = 0xb416cb20,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000002,
};

static struct clk_alpha_pll eva_cc_pll0 = {
	.offset = 0x0,
	.vco_table = taycan_elu_vco,
	.num_vco = ARRAY_SIZE(taycan_elu_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_ELU],
	.flags = DISABLE_TO_OFF,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "eva_cc_pll0",
			.parent_data = &(const struct clk_parent_data) {
				.fw_name = "bi_tcxo",
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_taycan_elu_ops,
		},
		.vdd_data = {
			.vdd_class = &vdd_mxc,
			.num_rate_max = VDD_NUM,
			.rate_max = (unsigned long[VDD_NUM]) {
				[VDD_LOWER_D1] = 1600000000,
				[VDD_LOW] = 1600000000,
				[VDD_LOW_L1] = 1600000000,
				[VDD_NOMINAL] = 2000000000,
				[VDD_HIGH] = 2500000000},
		},
	},
};

static const struct parent_map eva_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data eva_cc_parent_data_0_ao[] = {
	{ .fw_name = "bi_tcxo_ao" },
};

static const struct parent_map eva_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_EVA_CC_PLL0_OUT_MAIN, 1 },
};

static const struct clk_parent_data eva_cc_parent_data_1[] = {
	{ .fw_name = "bi_tcxo" },
	{ .hw = &eva_cc_pll0.clkr.hw },
};

static const struct parent_map eva_cc_parent_map_2[] = {
	{ P_SLEEP_CLK, 0 },
};

static const struct clk_parent_data eva_cc_parent_data_2_ao[] = {
	{ .fw_name = "sleep_clk" },
};

static const struct freq_tbl ftbl_eva_cc_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 eva_cc_ahb_clk_src = {
	.cmd_rcgr = 0x8018,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = eva_cc_parent_map_0,
	.freq_tbl = ftbl_eva_cc_ahb_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "eva_cc_ahb_clk_src",
		.parent_data = eva_cc_parent_data_0_ao,
		.num_parents = ARRAY_SIZE(eva_cc_parent_data_0_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_eva_cc_mvs0_clk_src[] = {
	F(1050000000, P_EVA_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1200000000, P_EVA_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1350000000, P_EVA_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1500000000, P_EVA_CC_PLL0_OUT_MAIN, 1, 0, 0),
	F(1650000000, P_EVA_CC_PLL0_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 eva_cc_mvs0_clk_src = {
	.cmd_rcgr = 0x8000,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = eva_cc_parent_map_1,
	.freq_tbl = ftbl_eva_cc_mvs0_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "eva_cc_mvs0_clk_src",
		.parent_data = eva_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(eva_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
	.clkr.vdd_data = {
		.vdd_classes = eva_cc_sun_regulators,
		.num_vdd_classes = ARRAY_SIZE(eva_cc_sun_regulators),
		.num_rate_max = VDD_NUM,
		.rate_max = (unsigned long[VDD_NUM]) {
			[VDD_LOWER_D1] = 1050000000,
			[VDD_LOWER] = 1200000000,
			[VDD_LOW] = 1350000000,
			[VDD_LOW_L1] = 1500000000,
			[VDD_NOMINAL] = 1650000000},
	},
};

static const struct freq_tbl ftbl_eva_cc_sleep_clk_src[] = {
	F(32000, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 eva_cc_sleep_clk_src = {
	.cmd_rcgr = 0x80e0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = eva_cc_parent_map_2,
	.freq_tbl = ftbl_eva_cc_sleep_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "eva_cc_sleep_clk_src",
		.parent_data = eva_cc_parent_data_2_ao,
		.num_parents = ARRAY_SIZE(eva_cc_parent_data_2_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 eva_cc_xo_clk_src = {
	.cmd_rcgr = 0x80bc,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = eva_cc_parent_map_0,
	.freq_tbl = ftbl_eva_cc_ahb_clk_src,
	.enable_safe_config = true,
	.flags = HW_CLK_CTRL_MODE,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "eva_cc_xo_clk_src",
		.parent_data = eva_cc_parent_data_0_ao,
		.num_parents = ARRAY_SIZE(eva_cc_parent_data_0_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_regmap_div eva_cc_mvs0_div_clk_src = {
	.reg = 0x809c,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "eva_cc_mvs0_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&eva_cc_mvs0_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div eva_cc_mvs0c_div2_div_clk_src = {
	.reg = 0x8060,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "eva_cc_mvs0c_div2_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&eva_cc_mvs0_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch eva_cc_mvs0_clk = {
	.halt_reg = 0x807c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x807c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x807c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "eva_cc_mvs0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&eva_cc_mvs0_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_mem_branch eva_cc_mvs0_freerun_clk = {
	.mem_enable_reg = 0x8090,
	.mem_ack_reg =  0x8090,
	.mem_enable_mask = BIT(3),
	.mem_enable_ack_mask = 0xc00,
	.mem_enable_inverted = true,
	.branch = {
		.halt_reg = 0x808c,
		.halt_check = BRANCH_HALT,
		.clkr = {
			.enable_reg = 0x808c,
			.enable_mask = BIT(0),
			.hw.init = &(const struct clk_init_data) {
				.name = "eva_cc_mvs0_freerun_clk",
				.parent_hws = (const struct clk_hw*[]) {
					&eva_cc_mvs0_div_clk_src.clkr.hw,
				},
				.num_parents = 1,
				.flags = CLK_SET_RATE_PARENT,
				.ops = &clk_branch2_mem_ops,
			},
		},
	},
};

static struct clk_branch eva_cc_mvs0_shift_clk = {
	.halt_reg = 0x80d8,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x80d8,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x80d8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "eva_cc_mvs0_shift_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&eva_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch eva_cc_mvs0c_clk = {
	.halt_reg = 0x804c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x804c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "eva_cc_mvs0c_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&eva_cc_mvs0c_div2_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch eva_cc_mvs0c_freerun_clk = {
	.halt_reg = 0x805c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x805c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "eva_cc_mvs0c_freerun_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&eva_cc_mvs0c_div2_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch eva_cc_mvs0c_shift_clk = {
	.halt_reg = 0x80dc,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x80dc,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x80dc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "eva_cc_mvs0c_shift_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&eva_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_regmap *eva_cc_sun_clocks[] = {
	[EVA_CC_AHB_CLK_SRC] = &eva_cc_ahb_clk_src.clkr,
	[EVA_CC_MVS0_CLK] = &eva_cc_mvs0_clk.clkr,
	[EVA_CC_MVS0_CLK_SRC] = &eva_cc_mvs0_clk_src.clkr,
	[EVA_CC_MVS0_DIV_CLK_SRC] = &eva_cc_mvs0_div_clk_src.clkr,
	[EVA_CC_MVS0_FREERUN_CLK] = &eva_cc_mvs0_freerun_clk.branch.clkr,
	[EVA_CC_MVS0_SHIFT_CLK] = &eva_cc_mvs0_shift_clk.clkr,
	[EVA_CC_MVS0C_CLK] = &eva_cc_mvs0c_clk.clkr,
	[EVA_CC_MVS0C_DIV2_DIV_CLK_SRC] = &eva_cc_mvs0c_div2_div_clk_src.clkr,
	[EVA_CC_MVS0C_FREERUN_CLK] = &eva_cc_mvs0c_freerun_clk.clkr,
	[EVA_CC_MVS0C_SHIFT_CLK] = &eva_cc_mvs0c_shift_clk.clkr,
	[EVA_CC_PLL0] = &eva_cc_pll0.clkr,
	[EVA_CC_SLEEP_CLK_SRC] = &eva_cc_sleep_clk_src.clkr,
	[EVA_CC_XO_CLK_SRC] = &eva_cc_xo_clk_src.clkr,
};

static const struct qcom_reset_map eva_cc_sun_resets[] = {
	[EVA_CC_INTERFACE_BCR] = { 0x80a0 },
	[EVA_CC_MVS0_BCR] = { 0x8064 },
	[EVA_CC_MVS0_FREERUN_CLK_ARES] = { 0x808c, 2 },
	[EVA_CC_MVS0C_CLK_ARES] = { 0x804c, 2 },
	[EVA_CC_MVS0C_BCR] = { 0x8030 },
	[EVA_CC_MVS0C_FREERUN_CLK_ARES] = { 0x805c, 2 },
};

static const struct regmap_config eva_cc_sun_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x9f4c,
	.fast_io = true,
};

static struct qcom_cc_desc eva_cc_sun_desc = {
	.config = &eva_cc_sun_regmap_config,
	.clks = eva_cc_sun_clocks,
	.num_clks = ARRAY_SIZE(eva_cc_sun_clocks),
	.resets = eva_cc_sun_resets,
	.num_resets = ARRAY_SIZE(eva_cc_sun_resets),
	.clk_regulators = eva_cc_sun_regulators,
	.num_clk_regulators = ARRAY_SIZE(eva_cc_sun_regulators),
};

static const struct of_device_id eva_cc_sun_match_table[] = {
	{ .compatible = "qcom,sun-evacc" },
	{ .compatible = "qcom,sun-evacc-v2" },
	{ }
};
MODULE_DEVICE_TABLE(of, eva_cc_sun_match_table);

static int eva_cc_sun_fixup(struct platform_device *pdev, struct regmap *regmap)
{
	const char *compat = NULL;
	int compatlen = 0;

	compat = of_get_property(pdev->dev.of_node, "compatible", &compatlen);
	if (!compat || compatlen <= 0)
		return -EINVAL;

	if (!strcmp(compat, "qcom,sun-evacc-v2"))
		regmap_update_bits(regmap, 0x9f24, BIT(0), BIT(0));

	return 0;
}

static int eva_cc_sun_probe(struct platform_device *pdev)
{
	unsigned int dly_accu_mask = 0x1f << 21;
	struct regmap *regmap;
	int ret;

	regmap = qcom_cc_map(pdev, &eva_cc_sun_desc);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	ret = qcom_cc_runtime_init(pdev, &eva_cc_sun_desc);
	if (ret)
		return ret;

	ret = pm_runtime_get_sync(&pdev->dev);
	if (ret)
		return ret;

	clk_taycan_elu_pll_configure(&eva_cc_pll0, regmap, &eva_cc_pll0_config);

	/*
	 * Maximize ctl data download delay and enable memory redundancy:
	 * MVS0C_CFG3
	 * MVS0_CFG3
	 */
	regmap_update_bits(regmap, 0x8040, dly_accu_mask, dly_accu_mask);
	regmap_update_bits(regmap, 0x8074, dly_accu_mask, dly_accu_mask);

	ret = eva_cc_sun_fixup(pdev, regmap);
	if (ret)
		return ret;
	/*
	 * Keep clocks always enabled:
	 *	eva_cc_ahb_clk
	 *	eva_cc_sleep_clk
	 *	eva_cc_xo_clk
	 */
	regmap_update_bits(regmap, 0x80a4, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x80f8, BIT(0), BIT(0));
	regmap_update_bits(regmap, 0x80d4, BIT(0), BIT(0));

	ret = qcom_cc_really_probe(pdev, &eva_cc_sun_desc, regmap);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register EVA CC clocks\n");
		return ret;
	}

	pm_runtime_put_sync(&pdev->dev);
	dev_info(&pdev->dev, "Registered EVA CC clocks\n");

	return ret;
}

static void eva_cc_sun_sync_state(struct device *dev)
{
	qcom_cc_sync_state(dev, &eva_cc_sun_desc);
}

static const struct dev_pm_ops eva_cc_sun_pm_ops = {
	SET_RUNTIME_PM_OPS(qcom_cc_runtime_suspend, qcom_cc_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend,
				pm_runtime_force_resume)
};

static struct platform_driver eva_cc_sun_driver = {
	.probe = eva_cc_sun_probe,
	.driver = {
		.name = "eva_cc-sun",
		.of_match_table = eva_cc_sun_match_table,
		.sync_state = eva_cc_sun_sync_state,
		.pm = &eva_cc_sun_pm_ops,
	},
};

static int __init eva_cc_sun_init(void)
{
	return platform_driver_register(&eva_cc_sun_driver);
}
subsys_initcall(eva_cc_sun_init);

static void __exit eva_cc_sun_exit(void)
{
	platform_driver_unregister(&eva_cc_sun_driver);
}
module_exit(eva_cc_sun_exit);

MODULE_DESCRIPTION("QTI EVA_CC SUN Driver");
MODULE_LICENSE("GPL");
