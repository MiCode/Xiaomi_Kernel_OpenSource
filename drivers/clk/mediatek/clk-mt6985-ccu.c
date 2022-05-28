// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6985-clk.h>

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
			"ccu_ahb_ck"/* parent */, 1),
	GATE_CCU(CLK_CCUSYS_CCU0, "ccusys_ccu0",
			"ccusys_ck"/* parent */, 2),
	GATE_CCU(CLK_CCUSYS_CCU1, "ccusys_ccu1",
			"ccusys_ck"/* parent */, 3),
	GATE_CCU(CLK_CCUSYS_DPE, "ccusys_dpe",
			"ccusys_ck"/* parent */, 4),
	GATE_CCU(CLK_CCUSYS_DHZE, "ccusys_dhze",
			"ccusys_ck"/* parent */, 5),
};

static const struct mtk_clk_desc ccu_mcd = {
	.clks = ccu_clks,
	.num_clks = CLK_CCU_NR_CLK,
};

static int clk_mt6985_ccu_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6985_ccu[] = {
	{
		.compatible = "mediatek,mt6985-ccu",
		.data = &ccu_mcd,
	},
	{}
};

static struct platform_driver clk_mt6985_ccu_drv = {
	.probe = clk_mt6985_ccu_probe,
	.driver = {
		.name = "clk-mt6985-ccu",
		.of_match_table = of_match_clk_mt6985_ccu,
	},
};

module_platform_driver(clk_mt6985_ccu_drv);
MODULE_LICENSE("GPL");
