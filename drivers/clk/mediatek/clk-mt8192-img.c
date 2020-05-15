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

static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &img_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate img_clks[] = {
	GATE_IMG(CLK_IMG_LARB9, "img_larb9", "img1_sel", 0),
	GATE_IMG(CLK_IMG_LARB10, "img_larb10", "img1_sel", 1),
	GATE_IMG(CLK_IMG_DIP, "img_dip", "img1_sel", 2),
	GATE_IMG(CLK_IMG_GALS, "img_gals", "img1_sel", 12),
};

static int clk_mt8192_img_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_IMG_NR_CLK);

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_img[] = {
	{ .compatible = "mediatek,mt8192-imgsys", },
	{}
};

static struct platform_driver clk_mt8192_img_drv = {
	.probe = clk_mt8192_img_probe,
	.driver = {
		.name = "clk-mt8192-img",
		.of_match_table = of_match_clk_mt8192_img,
	},
};

static int __init clk_mt8192_img_init(void)
{
	return platform_driver_register(&clk_mt8192_img_drv);
}

static void __exit clk_mt8192_img_exit(void)
{
	platform_driver_unregister(&clk_mt8192_img_drv);
}

arch_initcall(clk_mt8192_img_init);
module_exit(clk_mt8192_img_exit);
MODULE_LICENSE("GPL");
