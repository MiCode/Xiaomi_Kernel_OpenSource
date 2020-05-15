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

static const struct mtk_gate_regs img2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &img2_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate img2_clks[] = {
	GATE_IMG2(CLK_IMG2_LARB11, "img2_larb11", "img1_sel", 0),
	GATE_IMG2(CLK_IMG2_LARB12, "img2_larb12", "img1_sel", 1),
	GATE_IMG2(CLK_IMG2_MFB, "img2_mfb", "img1_sel", 6),
	GATE_IMG2(CLK_IMG2_WPE, "img2_wpe", "img1_sel", 7),
	GATE_IMG2(CLK_IMG2_MSS, "img2_mss", "img1_sel", 8),
	GATE_IMG2(CLK_IMG2_GALS, "img2_gals", "img1_sel", 12),
};

static int clk_mt8192_img2_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_IMG2_NR_CLK);

	mtk_clk_register_gates(node, img2_clks, ARRAY_SIZE(img2_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_img2[] = {
	{ .compatible = "mediatek,mt8192-imgsys2", },
	{}
};

static struct platform_driver clk_mt8192_img2_drv = {
	.probe = clk_mt8192_img2_probe,
	.driver = {
		.name = "clk-mt8192-img2",
		.of_match_table = of_match_clk_mt8192_img2,
	},
};

static int __init clk_mt8192_img2_init(void)
{
	return platform_driver_register(&clk_mt8192_img2_drv);
}

static void __exit clk_mt8192_img2_exit(void)
{
	platform_driver_unregister(&clk_mt8192_img2_drv);
}

arch_initcall(clk_mt8192_img2_init);
module_exit(clk_mt8192_img2_exit);
MODULE_LICENSE("GPL");
