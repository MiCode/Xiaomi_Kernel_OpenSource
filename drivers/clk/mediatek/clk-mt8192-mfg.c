// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Weiyi Lu <weiyi.lu@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8192-clk.h>

static const struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFG(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &mfg_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate mfg_clks[] = {
	GATE_MFG(CLK_MFG_BG3D, "mfg_bg3d", "mfg_pll_sel", 0),
};

static int clk_mt8192_mfg_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_MFG_NR_CLK);

	mtk_clk_register_gates(node, mfg_clks, ARRAY_SIZE(mfg_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_mfg[] = {
	{ .compatible = "mediatek,mt8192-mfgcfg", },
	{}
};

static struct platform_driver clk_mt8192_mfg_drv = {
	.probe = clk_mt8192_mfg_probe,
	.driver = {
		.name = "clk-mt8192-mfg",
		.of_match_table = of_match_clk_mt8192_mfg,
	},
};

static int __init clk_mt8192_mfg_init(void)
{
	return platform_driver_register(&clk_mt8192_mfg_drv);
}

static void __exit clk_mt8192_mfg_exit(void)
{
	platform_driver_unregister(&clk_mt8192_mfg_drv);
}

arch_initcall(clk_mt8192_mfg_init);
module_exit(clk_mt8192_mfg_exit);
MODULE_LICENSE("GPL");
