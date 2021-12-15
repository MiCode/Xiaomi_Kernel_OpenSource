// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs mfgcfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFGCFG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfgcfg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mfgcfg_clks[] = {
	GATE_MFGCFG(CLK_MFGCFG_BG3D, "mfgcfg_bg3d",
			"mfg_ref_ck"/* parent */, 0),
};

static const struct mtk_clk_desc mfgcfg_mcd = {
	.clks = mfgcfg_clks,
	.num_clks = CLK_MFGCFG_NR_CLK,
};

static int clk_mt6893_mfgcfg_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6893_mfgcfg[] = {
	{
		.compatible = "mediatek,mt6893-mfgcfg",
		.data = &mfgcfg_mcd,
	},
	{}
};

static struct platform_driver clk_mt6893_mfgcfg_drv = {
	.probe = clk_mt6893_mfgcfg_probe,
	.driver = {
		.name = "clk-mt6893-mfgcfg",
		.of_match_table = of_match_clk_mt6893_mfgcfg,
	},
};

static int __init clk_mt6893_mfgcfg_init(void)
{
	return platform_driver_register(&clk_mt6893_mfgcfg_drv);
}

static void __exit clk_mt6893_mfgcfg_exit(void)
{
	platform_driver_unregister(&clk_mt6893_mfgcfg_drv);
}

postcore_initcall(clk_mt6893_mfgcfg_init);
module_exit(clk_mt6893_mfgcfg_exit);
MODULE_LICENSE("GPL");
