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

static const struct mtk_gate_regs apu1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_APU1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &apu1_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate apu1_clks[] = {
	GATE_APU1(CLK_APU1_APU, "apu1_apu", "dsp2_sel", 0),
	GATE_APU1(CLK_APU1_AXI_M, "apu1_axi_m", "dsp2_sel", 1),
	GATE_APU1(CLK_APU1_JTAG, "apu1_jtag", "dsp2_sel", 2),
};

static int clk_mt8192_apu1_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_APU1_NR_CLK);

	mtk_clk_register_gates(node, apu1_clks, ARRAY_SIZE(apu1_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_apu1[] = {
	{ .compatible = "mediatek,mt8192-apu1", },
	{}
};

static struct platform_driver clk_mt8192_apu1_drv = {
	.probe = clk_mt8192_apu1_probe,
	.driver = {
		.name = "clk-mt8192-apu1",
		.of_match_table = of_match_clk_mt8192_apu1,
	},
};

static int __init clk_mt8192_apu1_init(void)
{
	return platform_driver_register(&clk_mt8192_apu1_drv);
}

static void __exit clk_mt8192_apu1_exit(void)
{
	platform_driver_unregister(&clk_mt8192_apu1_drv);
}

arch_initcall(clk_mt8192_apu1_init);
module_exit(clk_mt8192_apu1_exit);
MODULE_LICENSE("GPL");
