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

static const struct mtk_gate_regs venc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VENC(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &venc_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr_inv)

static const struct mtk_gate venc_clks[] = {
	GATE_VENC(CLK_VENC_SET0_LARB, "venc_set0_larb", "venc_sel", 0),
	GATE_VENC(CLK_VENC_SET1_VENC, "venc_set1_venc", "venc_sel", 4),
	GATE_VENC(CLK_VENC_SET2_JPGENC, "venc_set2_jpgenc", "venc_sel", 8),
	GATE_VENC(CLK_VENC_SET5_GALS, "venc_set5_gals", "venc_sel", 28),
};

static int clk_mt8192_venc_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_VENC_NR_CLK);

	mtk_clk_register_gates(node, venc_clks, ARRAY_SIZE(venc_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_venc[] = {
	{ .compatible = "mediatek,mt8192-vencsys", },
	{}
};

static struct platform_driver clk_mt8192_venc_drv = {
	.probe = clk_mt8192_venc_probe,
	.driver = {
		.name = "clk-mt8192-venc",
		.of_match_table = of_match_clk_mt8192_venc,
	},
};

static int __init clk_mt8192_venc_init(void)
{
	return platform_driver_register(&clk_mt8192_venc_drv);
}

static void __exit clk_mt8192_venc_exit(void)
{
	platform_driver_unregister(&clk_mt8192_venc_drv);
}

arch_initcall(clk_mt8192_venc_init);
module_exit(clk_mt8192_venc_exit);
MODULE_LICENSE("GPL");
