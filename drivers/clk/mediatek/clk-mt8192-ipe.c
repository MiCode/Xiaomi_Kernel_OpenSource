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

static const struct mtk_gate_regs ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IPE(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &ipe_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate ipe_clks[] = {
	GATE_IPE(CLK_IPE_LARB19, "ipe_larb19", "ipe_sel", 0),
	GATE_IPE(CLK_IPE_LARB20, "ipe_larb20", "ipe_sel", 1),
	GATE_IPE(CLK_IPE_SMI_SUBCOM, "ipe_smi_subcom", "ipe_sel", 2),
	GATE_IPE(CLK_IPE_FD, "ipe_fd", "ipe_sel", 3),
	GATE_IPE(CLK_IPE_FE, "ipe_fe", "ipe_sel", 4),
	GATE_IPE(CLK_IPE_RSC, "ipe_rsc", "ipe_sel", 5),
	GATE_IPE(CLK_IPE_DPE, "ipe_dpe", "ipe_sel", 6),
	GATE_IPE(CLK_IPE_GALS, "ipe_gals", "ipe_sel", 8),
};

static int clk_mt8192_ipe_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_IPE_NR_CLK);

	mtk_clk_register_gates(node, ipe_clks, ARRAY_SIZE(ipe_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_ipe[] = {
	{ .compatible = "mediatek,mt8192-ipesys", },
	{}
};

static struct platform_driver clk_mt8192_ipe_drv = {
	.probe = clk_mt8192_ipe_probe,
	.driver = {
		.name = "clk-mt8192-ipe",
		.of_match_table = of_match_clk_mt8192_ipe,
	},
};

static int __init clk_mt8192_ipe_init(void)
{
	return platform_driver_register(&clk_mt8192_ipe_drv);
}

static void __exit clk_mt8192_ipe_exit(void)
{
	platform_driver_unregister(&clk_mt8192_ipe_drv);
}

arch_initcall(clk_mt8192_ipe_init);
module_exit(clk_mt8192_ipe_exit);
MODULE_LICENSE("GPL");
