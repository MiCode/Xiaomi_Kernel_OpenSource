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

static const struct mtk_gate_regs imgsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate imgsys1_clks[] = {
	GATE_IMGSYS1(CLK_IMGSYS1_LARB9, "imgsys1_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMGSYS1(CLK_IMGSYS1_LARB10, "imgsys1_larb10",
			"img1_ck"/* parent */, 1),
	GATE_IMGSYS1(CLK_IMGSYS1_DIP, "imgsys1_dip",
			"img1_ck"/* parent */, 2),
	GATE_IMGSYS1(CLK_IMGSYS1_GALS, "imgsys1_gals",
			"img1_ck"/* parent */, 12),
};

static int clk_mt6853_imgsys1_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMGSYS1_NR_CLK);

	mtk_clk_register_gates_with_dev(node, imgsys1_clks, ARRAY_SIZE(imgsys1_clks),
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

static const struct of_device_id of_match_clk_mt6853_imgsys1[] = {
	{ .compatible = "mediatek,mt6853-imgsys1", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_imgsys1_drv = {
	.probe = clk_mt6853_imgsys1_probe,
	.driver = {
		.name = "clk-mt6853-imgsys1",
		.of_match_table = of_match_clk_mt6853_imgsys1,
	},
};

builtin_platform_driver(clk_mt6853_imgsys1_drv);

#else

static struct platform_driver clk_mt6853_imgsys1_drv = {
	.probe = clk_mt6853_imgsys1_probe,
	.driver = {
		.name = "clk-mt6853-imgsys1",
		.of_match_table = of_match_clk_mt6853_imgsys1,
	},
};

static int __init clk_mt6853_imgsys1_init(void)
{
	return platform_driver_register(&clk_mt6853_imgsys1_drv);
}

static void __exit clk_mt6853_imgsys1_exit(void)
{
	platform_driver_unregister(&clk_mt6853_imgsys1_drv);
}

arch_initcall(clk_mt6853_imgsys1_init);
module_exit(clk_mt6853_imgsys1_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
