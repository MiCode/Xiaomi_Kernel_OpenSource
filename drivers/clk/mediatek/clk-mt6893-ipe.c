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

static const struct mtk_gate_regs ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ipe_clks[] = {
	GATE_IPE(CLK_IPE_LARB19, "ipe_larb19",
			"ipe_ck"/* parent */, 0),
	GATE_IPE(CLK_IPE_LARB20, "ipe_larb20",
			"ipe_ck"/* parent */, 1),
	GATE_IPE(CLK_IPE_SMI_SUBCOM, "ipe_smi_subcom",
			"ipe_ck"/* parent */, 2),
	GATE_IPE(CLK_IPE_FD, "ipe_fd",
			"ipe_ck"/* parent */, 3),
	GATE_IPE(CLK_IPE_FE, "ipe_fe",
			"ipe_ck"/* parent */, 4),
	GATE_IPE(CLK_IPE_RSC, "ipe_rsc",
			"ipe_ck"/* parent */, 5),
	GATE_IPE(CLK_IPE_DPE, "ipe_dpe",
			"ipe_ck"/* parent */, 6),
};

static int clk_mt6893_ipe_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IPE_NR_CLK);

	mtk_clk_register_gates_with_dev(node, ipe_clks, ARRAY_SIZE(ipe_clks),
			clk_data, &pdev->dev);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6893_ipe[] = {
	{ .compatible = "mediatek,mt6893-ipesys", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_ipe_drv = {
	.probe = clk_mt6893_ipe_probe,
	.driver = {
		.name = "clk-mt6893-ipe",
		.of_match_table = of_match_clk_mt6893_ipe,
	},
};

builtin_platform_driver(clk_mt6893_ipe_drv);

#else

static struct platform_driver clk_mt6893_ipe_drv = {
	.probe = clk_mt6893_ipe_probe,
	.driver = {
		.name = "clk-mt6893-ipe",
		.of_match_table = of_match_clk_mt6893_ipe,
	},
};

static int __init clk_mt6893_ipe_init(void)
{
	return platform_driver_register(&clk_mt6893_ipe_drv);
}

static void __exit clk_mt6893_ipe_exit(void)
{
	platform_driver_unregister(&clk_mt6893_ipe_drv);
}

arch_initcall(clk_mt6893_ipe_init);
module_exit(clk_mt6893_ipe_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
