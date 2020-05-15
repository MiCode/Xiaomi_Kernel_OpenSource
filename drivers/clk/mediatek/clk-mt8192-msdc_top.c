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

static const struct mtk_gate_regs msdc_top_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

#define GATE_MSDC_TOP(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &msdc_top_cg_regs, _shift,	\
		&mtk_clk_gate_ops_no_setclr_inv)

static const struct mtk_gate msdc_top_clks[] = {
	GATE_MSDC_TOP(CLK_MSDC_TOP_AES_0P, "msdc_top_aes_0p",
		"aes_msdcfde_sel", 0),
	GATE_MSDC_TOP(CLK_MSDC_TOP_SRC_0P, "msdc_top_src_0p",
		"infra_msdc0_src", 1),
	GATE_MSDC_TOP(CLK_MSDC_TOP_SRC_1P, "msdc_top_src_1p",
		"infra_msdc1_src", 2),
	GATE_MSDC_TOP(CLK_MSDC_TOP_SRC_2P, "msdc_top_src_2p",
		"infra_msdc2_src", 3),
	GATE_MSDC_TOP(CLK_MSDC_TOP_P_MSDC0, "msdc_top_p_msdc0",
		"axi_sel", 4),
	GATE_MSDC_TOP(CLK_MSDC_TOP_P_MSDC1, "msdc_top_p_msdc1",
		"axi_sel", 5),
	GATE_MSDC_TOP(CLK_MSDC_TOP_P_MSDC2, "msdc_top_p_msdc2",
		"axi_sel", 6),
	GATE_MSDC_TOP(CLK_MSDC_TOP_P_CFG, "msdc_top_p_cfg",
		"axi_sel", 7),
	GATE_MSDC_TOP(CLK_MSDC_TOP_AXI, "msdc_top_axi", "axi_sel", 8),
	GATE_MSDC_TOP(CLK_MSDC_TOP_H_MST_0P, "msdc_top_h_mst_0p",
		"infra_msdc0", 9),
	GATE_MSDC_TOP(CLK_MSDC_TOP_H_MST_1P, "msdc_top_h_mst_1p",
		"infra_msdc1", 10),
	GATE_MSDC_TOP(CLK_MSDC_TOP_H_MST_2P, "msdc_top_h_mst_2p",
		"infra_msdc2", 11),
	GATE_MSDC_TOP(CLK_MSDC_TOP_MEM_OFF_DLY_26M, "msdc_top_mem_off_dly_26m",
		"clk26m", 12),
	GATE_MSDC_TOP(CLK_MSDC_TOP_32K, "msdc_top_32k",
		"clk32k", 13),
	GATE_MSDC_TOP(CLK_MSDC_TOP_AHB2AXI_BRG_AXI, "msdc_top_ahb2axi_brg_axi",
		"axi_sel", 14),
};

static int clk_mt8192_msdc_top_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_MSDC_TOP_NR_CLK);

	mtk_clk_register_gates(node, msdc_top_clks, ARRAY_SIZE(msdc_top_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_msdc_top[] = {
	{ .compatible = "mediatek,mt8192-msdc_top", },
	{}
};

static struct platform_driver clk_mt8192_msdc_top_drv = {
	.probe = clk_mt8192_msdc_top_probe,
	.driver = {
		.name = "clk-mt8192-msdc_top",
		.of_match_table = of_match_clk_mt8192_msdc_top,
	},
};

static int __init clk_mt8192_msdc_top_init(void)
{
	return platform_driver_register(&clk_mt8192_msdc_top_drv);
}

static void __exit clk_mt8192_msdc_top_exit(void)
{
	platform_driver_unregister(&clk_mt8192_msdc_top_drv);
}

arch_initcall(clk_mt8192_msdc_top_init);
module_exit(clk_mt8192_msdc_top_exit);
MODULE_LICENSE("GPL");
