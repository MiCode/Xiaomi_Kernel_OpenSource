// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6855-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

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

static const struct mtk_clk_desc imgsys1_mcd = {
	.clks = imgsys1_clks,
	.num_clks = CLK_IMGSYS1_NR_CLK,
};

static int clk_mt6855_imgsys1_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6855_imgsys1[] = {
	{
		.compatible = "mediatek,mt6855-imgsys1",
		.data = &imgsys1_mcd,
	},
	{}
};

static struct platform_driver clk_mt6855_imgsys1_drv = {
	.probe = clk_mt6855_imgsys1_probe,
	.driver = {
		.name = "clk-mt6855-imgsys1",
		.of_match_table = of_match_clk_mt6855_imgsys1,
	},
};

static int __init clk_mt6855_imgsys1_init(void)
{
	return platform_driver_register(&clk_mt6855_imgsys1_drv);
}

static void __exit clk_mt6855_imgsys1_exit(void)
{
	platform_driver_unregister(&clk_mt6855_imgsys1_drv);
}

arch_initcall(clk_mt6855_imgsys1_init);
module_exit(clk_mt6855_imgsys1_exit);
MODULE_LICENSE("GPL");
