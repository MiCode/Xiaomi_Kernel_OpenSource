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

static const struct mtk_gate_regs apu0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_APU0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &apu0_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate apu0_clks[] = {
	GATE_APU0(CLK_APU0_APU, "apu0_apu", "dsp1_sel", 0),
	GATE_APU0(CLK_APU0_AXI_M, "apu0_axi_m", "dsp1_sel", 1),
	GATE_APU0(CLK_APU0_JTAG, "apu0_jtag", "dsp1_sel", 2),
};

static int clk_mt8192_apu0_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_APU0_NR_CLK);

	mtk_clk_register_gates(node, apu0_clks, ARRAY_SIZE(apu0_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_apu0[] = {
	{ .compatible = "mediatek,mt8192-apu0", },
	{}
};

static struct platform_driver clk_mt8192_apu0_drv = {
	.probe = clk_mt8192_apu0_probe,
	.driver = {
		.name = "clk-mt8192-apu0",
		.of_match_table = of_match_clk_mt8192_apu0,
	},
};

static int __init clk_mt8192_apu0_init(void)
{
	return platform_driver_register(&clk_mt8192_apu0_drv);
}

static void __exit clk_mt8192_apu0_exit(void)
{
	platform_driver_unregister(&clk_mt8192_apu0_drv);
}

arch_initcall(clk_mt8192_apu0_init);
module_exit(clk_mt8192_apu0_exit);
MODULE_LICENSE("GPL");
