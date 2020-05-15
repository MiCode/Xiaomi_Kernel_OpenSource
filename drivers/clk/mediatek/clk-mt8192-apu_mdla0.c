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

static const struct mtk_gate_regs apu_mdla0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_MDLA0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &apu_mdla0_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate apu_mdla0_clks[] = {
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG0, "apu_mdla0_cg0", "dsp5_sel", 0),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG1, "apu_mdla0_cg1", "dsp5_sel", 1),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG2, "apu_mdla0_cg2", "dsp5_sel", 2),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG3, "apu_mdla0_cg3", "dsp5_sel", 3),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG4, "apu_mdla0_cg4", "dsp5_sel", 4),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG5, "apu_mdla0_cg5", "dsp5_sel", 5),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG6, "apu_mdla0_cg6", "dsp5_sel", 6),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG7, "apu_mdla0_cg7", "dsp5_sel", 7),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG8, "apu_mdla0_cg8", "dsp5_sel", 8),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG9, "apu_mdla0_cg9", "dsp5_sel", 9),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG10, "apu_mdla0_cg10", "dsp5_sel", 10),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG11, "apu_mdla0_cg11", "dsp5_sel", 11),
	GATE_APU_MDLA0(CLK_APU_MDLA0_CG12, "apu_mdla0_cg12", "dsp5_sel", 12),
	GATE_APU_MDLA0(CLK_APU_MDLA0_APB, "apu_mdla0_apb", "dsp5_sel", 13),
	GATE_APU_MDLA0(CLK_APU_MDLA0_AXI_M, "apu_mdla0_axi_m", "dsp5_sel", 14),
};

static int clk_mt8192_apu_mdla0_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_APU_MDLA0_NR_CLK);

	mtk_clk_register_gates(node, apu_mdla0_clks, ARRAY_SIZE(apu_mdla0_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_apu_mdla0[] = {
	{ .compatible = "mediatek,mt8192-apu_mdla0", },
	{}
};

static struct platform_driver clk_mt8192_apu_mdla0_drv = {
	.probe = clk_mt8192_apu_mdla0_probe,
	.driver = {
		.name = "clk-mt8192-apu_mdla0",
		.of_match_table = of_match_clk_mt8192_apu_mdla0,
	},
};

static int __init clk_mt8192_apu_mdla0_init(void)
{
	return platform_driver_register(&clk_mt8192_apu_mdla0_drv);
}

static void __exit clk_mt8192_apu_mdla0_exit(void)
{
	platform_driver_unregister(&clk_mt8192_apu_mdla0_drv);
}

arch_initcall(clk_mt8192_apu_mdla0_init);
module_exit(clk_mt8192_apu_mdla0_exit);
MODULE_LICENSE("GPL");
