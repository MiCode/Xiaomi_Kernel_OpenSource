// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6853-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

static const struct mtk_gate_regs impw_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPW(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impw_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impw_clks[] = {
	GATE_IMPW(CLK_IMPW_AP_CLOCK_RO_I2C0, "impw_ap_i2c0",
			"i2c_pseudo"/* parent */, 0),
};

static int clk_mt6853_impw_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPW_NR_CLK);

	mtk_clk_register_gates(node, impw_clks, ARRAY_SIZE(impw_clks),
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

static const struct of_device_id of_match_clk_mt6853_impw[] = {
	{ .compatible = "mediatek,mt6853-imp_iic_wrap_w", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_impw_drv = {
	.probe = clk_mt6853_impw_probe,
	.driver = {
		.name = "clk-mt6853-impw",
		.of_match_table = of_match_clk_mt6853_impw,
	},
};

builtin_platform_driver(clk_mt6853_impw_drv);

#else

static struct platform_driver clk_mt6853_impw_drv = {
	.probe = clk_mt6853_impw_probe,
	.driver = {
		.name = "clk-mt6853-impw",
		.of_match_table = of_match_clk_mt6853_impw,
	},
};

static int __init clk_mt6853_impw_init(void)
{
	return platform_driver_register(&clk_mt6853_impw_drv);
}

static void __exit clk_mt6853_impw_exit(void)
{
	platform_driver_unregister(&clk_mt6853_impw_drv);
}

arch_initcall(clk_mt6853_impw_init);
module_exit(clk_mt6853_impw_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
