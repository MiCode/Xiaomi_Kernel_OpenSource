// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_AP_I2C1_RO, "imps_ap_i2c1_ro",
			"i2c_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_AP_I2C2_RO, "imps_ap_i2c2_ro",
			"i2c_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_AP_I2C4_RO, "imps_ap_i2c4_ro",
			"i2c_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_AP_I2C7_RO, "imps_ap_i2c7_ro",
			"i2c_ck"/* parent */, 3),
	GATE_IMPS(CLK_IMPS_AP_I2C8_RO, "imps_ap_i2c8_ro",
			"i2c_ck"/* parent */, 4),
};

static int clk_mt6893_imps_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPS_NR_CLK);

	mtk_clk_register_gates(node, imps_clks, ARRAY_SIZE(imps_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6893_imps[] = {
	{ .compatible = "mediatek,mt6893-imp_iic_wrap_s", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_imps_drv = {
	.probe = clk_mt6893_imps_probe,
	.driver = {
		.name = "clk-mt6893-imps",
		.of_match_table = of_match_clk_mt6893_imps,
	},
};

builtin_platform_driver(clk_mt6893_imps_drv);

#else

static struct platform_driver clk_mt6893_imps_drv = {
	.probe = clk_mt6893_imps_probe,
	.driver = {
		.name = "clk-mt6893-imps",
		.of_match_table = of_match_clk_mt6893_imps,
	},
};

static int __init clk_mt6893_imps_init(void)
{
	return platform_driver_register(&clk_mt6893_imps_drv);
}

static void __exit clk_mt6893_imps_exit(void)
{
	platform_driver_unregister(&clk_mt6893_imps_drv);
}

arch_initcall(clk_mt6893_imps_init);
module_exit(clk_mt6893_imps_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
