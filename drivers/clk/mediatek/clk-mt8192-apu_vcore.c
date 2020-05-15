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

static const struct mtk_gate_regs apu_vcore_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_VCORE(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &apu_vcore_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate apu_vcore_clks[] = {
	GATE_APU_VCORE(CLK_APU_VCORE_AHB, "apu_vcore_ahb", "ipu_if_sel", 0),
	GATE_APU_VCORE(CLK_APU_VCORE_AXI, "apu_vcore_axi", "ipu_if_sel", 1),
	GATE_APU_VCORE(CLK_APU_VCORE_ADL, "apu_vcore_adl", "ipu_if_sel", 2),
	GATE_APU_VCORE(CLK_APU_VCORE_QOS, "apu_vcore_qos", "ipu_if_sel", 3),
};

static int clk_mt8192_apu_vcore_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_APU_VCORE_NR_CLK);

	mtk_clk_register_gates(node, apu_vcore_clks, ARRAY_SIZE(apu_vcore_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_apu_vcore[] = {
	{ .compatible = "mediatek,mt8192-apu_vcore", },
	{}
};

static struct platform_driver clk_mt8192_apu_vcore_drv = {
	.probe = clk_mt8192_apu_vcore_probe,
	.driver = {
		.name = "clk-mt8192-apu_vcore",
		.of_match_table = of_match_clk_mt8192_apu_vcore,
	},
};

static int __init clk_mt8192_apu_vcore_init(void)
{
	return platform_driver_register(&clk_mt8192_apu_vcore_drv);
}

static void __exit clk_mt8192_apu_vcore_exit(void)
{
	platform_driver_unregister(&clk_mt8192_apu_vcore_drv);
}

arch_initcall(clk_mt8192_apu_vcore_init);
module_exit(clk_mt8192_apu_vcore_exit);
MODULE_LICENSE("GPL");
