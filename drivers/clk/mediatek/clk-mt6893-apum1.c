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

static const struct mtk_gate_regs apum1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APUM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apum1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate apum1_clks[] = {
	GATE_APUM1(CLK_APUM1_MDLA_CG0, "apum1_mdla_cg0",
			"dsp6_ck"/* parent */, 0),
	GATE_APUM1(CLK_APUM1_MDLA_CG1, "apum1_mdla_cg1",
			"dsp6_ck"/* parent */, 1),
	GATE_APUM1(CLK_APUM1_MDLA_CG2, "apum1_mdla_cg2",
			"dsp6_ck"/* parent */, 2),
	GATE_APUM1(CLK_APUM1_MDLA_CG3, "apum1_mdla_cg3",
			"dsp6_ck"/* parent */, 3),
	GATE_APUM1(CLK_APUM1_MDLA_CG4, "apum1_mdla_cg4",
			"dsp6_ck"/* parent */, 4),
	GATE_APUM1(CLK_APUM1_MDLA_CG5, "apum1_mdla_cg5",
			"dsp6_ck"/* parent */, 5),
	GATE_APUM1(CLK_APUM1_MDLA_CG6, "apum1_mdla_cg6",
			"dsp6_ck"/* parent */, 6),
	GATE_APUM1(CLK_APUM1_MDLA_CG7, "apum1_mdla_cg7",
			"dsp6_ck"/* parent */, 7),
	GATE_APUM1(CLK_APUM1_MDLA_CG8, "apum1_mdla_cg8",
			"dsp6_ck"/* parent */, 8),
	GATE_APUM1(CLK_APUM1_MDLA_CG9, "apum1_mdla_cg9",
			"dsp6_ck"/* parent */, 9),
	GATE_APUM1(CLK_APUM1_MDLA_CG10, "apum1_mdla_cg10",
			"dsp6_ck"/* parent */, 10),
	GATE_APUM1(CLK_APUM1_MDLA_CG11, "apum1_mdla_cg11",
			"dsp6_ck"/* parent */, 11),
	GATE_APUM1(CLK_APUM1_MDLA_CG12, "apum1_mdla_cg12",
			"dsp6_ck"/* parent */, 12),
	GATE_APUM1(CLK_APUM1_APB, "apum1_apb",
			"dsp6_ck"/* parent */, 13),
	GATE_APUM1(CLK_APUM1_AXI_M, "apum1_axi_m",
			"dsp6_ck"/* parent */, 14),
};

static int clk_mt6893_apum1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APUM1_NR_CLK);

	mtk_clk_register_gates(node, apum1_clks, ARRAY_SIZE(apum1_clks),
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

static const struct of_device_id of_match_clk_mt6893_apum1[] = {
	{ .compatible = "mediatek,mt6893-apu_mdla1", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_apum1_drv = {
	.probe = clk_mt6893_apum1_probe,
	.driver = {
		.name = "clk-mt6893-apum1",
		.of_match_table = of_match_clk_mt6893_apum1,
	},
};

builtin_platform_driver(clk_mt6893_apum1_drv);

#else

static struct platform_driver clk_mt6893_apum1_drv = {
	.probe = clk_mt6893_apum1_probe,
	.driver = {
		.name = "clk-mt6893-apum1",
		.of_match_table = of_match_clk_mt6893_apum1,
	},
};

static int __init clk_mt6893_apum1_init(void)
{
	return platform_driver_register(&clk_mt6893_apum1_drv);
}

static void __exit clk_mt6893_apum1_exit(void)
{
	platform_driver_unregister(&clk_mt6893_apum1_drv);
}

arch_initcall(clk_mt6893_apum1_init);
module_exit(clk_mt6893_apum1_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
