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

static const struct mtk_gate_regs scp_adsp_cg_regs = {
	.set_ofs = 0x180,
	.clr_ofs = 0x180,
	.sta_ofs = 0x180,
};

#define GATE_SCP_ADSP(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &scp_adsp_cg_regs, _shift,	\
		&mtk_clk_gate_ops_no_setclr)

static const struct mtk_gate scp_adsp_clks[] = {
	GATE_SCP_ADSP(CLK_SCP_ADSP_AUDIODSP, "scp_adsp_audiodsp",
		"adsp_sel", 0),
};

static int clk_mt8192_scp_adsp_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_SCP_ADSP_NR_CLK);

	mtk_clk_register_gates(node, scp_adsp_clks, ARRAY_SIZE(scp_adsp_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_scp_adsp[] = {
	{ .compatible = "mediatek,mt8192-scp_adsp", },
	{}
};

static struct platform_driver clk_mt8192_scp_adsp_drv = {
	.probe = clk_mt8192_scp_adsp_probe,
	.driver = {
		.name = "clk-mt8192-scp_adsp",
		.of_match_table = of_match_clk_mt8192_scp_adsp,
	},
};

static int __init clk_mt8192_scp_adsp_init(void)
{
	return platform_driver_register(&clk_mt8192_scp_adsp_drv);
}

static void __exit clk_mt8192_scp_adsp_exit(void)
{
	platform_driver_unregister(&clk_mt8192_scp_adsp_drv);
}

arch_initcall(clk_mt8192_scp_adsp_init);
module_exit(clk_mt8192_scp_adsp_exit);
MODULE_LICENSE("GPL");
