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

static const struct mtk_gate_regs msdc_cg_regs = {
	.set_ofs = 0xb4,
	.clr_ofs = 0xb4,
	.sta_ofs = 0xb4,
};

#define GATE_MSDC(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &msdc_cg_regs, _shift,	\
		&mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate msdc_clks[] = {
	GATE_MSDC(CLK_MSDC_AXI_WRAP, "msdc_axi_wrap", "axi_sel", 22),
};

static int clk_mt8192_msdc_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_MSDC_NR_CLK);

	mtk_clk_register_gates(node, msdc_clks, ARRAY_SIZE(msdc_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_msdc[] = {
	{ .compatible = "mediatek,mt8192-msdc", },
	{}
};

static struct platform_driver clk_mt8192_msdc_drv = {
	.probe = clk_mt8192_msdc_probe,
	.driver = {
		.name = "clk-mt8192-msdc",
		.of_match_table = of_match_clk_mt8192_msdc,
	},
};

static int __init clk_mt8192_msdc_init(void)
{
	return platform_driver_register(&clk_mt8192_msdc_drv);
}

static void __exit clk_mt8192_msdc_exit(void)
{
	platform_driver_unregister(&clk_mt8192_msdc_drv);
}

arch_initcall(clk_mt8192_msdc_init);
module_exit(clk_mt8192_msdc_exit);
MODULE_LICENSE("GPL");
