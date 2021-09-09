// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6895-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ccu_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CCU(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ccu_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ccu_clks[] = {
	GATE_CCU(CLK_CCU_LARB19, "ccu_larb19",
			"ccusys_ck"/* parent */, 0),
	GATE_CCU(CLK_CCU_AHB, "ccu_ahb",
			"ccusys_ck"/* parent */, 1),
	GATE_CCU(CLK_CCUSYS_CCU0, "ccusys_ccu0",
			"ccusys_ck"/* parent */, 2),
	GATE_CCU(CLK_CCUSYS_CCU1, "ccusys_ccu1",
			"ccusys_ck"/* parent */, 3),
};

static const struct mtk_clk_desc ccu_mcd = {
	.clks = ccu_clks,
	.num_clks = CLK_CCU_NR_CLK,
};

static int clk_mt6895_ccu_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6895_ccu[] = {
	{
		.compatible = "mediatek,mt6895-ccu",
		.data = &ccu_mcd,
	},
	{}
};

static struct platform_driver clk_mt6895_ccu_drv = {
	.probe = clk_mt6895_ccu_probe,
	.driver = {
		.name = "clk-mt6895-ccu",
		.of_match_table = of_match_clk_mt6895_ccu,
	},
};

static int __init clk_mt6895_ccu_init(void)
{
	return platform_driver_register(&clk_mt6895_ccu_drv);
}

static void __exit clk_mt6895_ccu_exit(void)
{
	platform_driver_unregister(&clk_mt6895_ccu_drv);
}

arch_initcall(clk_mt6895_ccu_init);
module_exit(clk_mt6895_ccu_exit);
MODULE_LICENSE("GPL");
