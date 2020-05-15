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

static const struct mtk_gate_regs vdec_soc0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec_soc1_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vdec_soc2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDEC_SOC0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec_soc0_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr_inv)

#define GATE_VDEC_SOC1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec_soc1_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr_inv)

#define GATE_VDEC_SOC2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec_soc2_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr_inv)

static const struct mtk_gate vdec_soc_clks[] = {
	/* VDEC_SOC0 */
	GATE_VDEC_SOC0(CLK_VDEC_SOC_VDEC, "vdec_soc_vdec", "vdec_sel", 0),
	GATE_VDEC_SOC0(CLK_VDEC_SOC_VDEC_ACTIVE, "vdec_soc_vdec_active",
		"vdec_sel", 4),
	/* VDEC_SOC1 */
	GATE_VDEC_SOC1(CLK_VDEC_SOC_LAT, "vdec_soc_lat", "vdec_sel", 0),
	GATE_VDEC_SOC1(CLK_VDEC_SOC_LAT_ACTIVE, "vdec_soc_lat_active",
		"vdec_sel", 4),
	/* VDEC_SOC2 */
	GATE_VDEC_SOC2(CLK_VDEC_SOC_LARB1, "vdec_soc_larb1", "vdec_sel", 0),
};

static int clk_mt8192_vdec_soc_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_VDEC_SOC_NR_CLK);

	mtk_clk_register_gates(node, vdec_soc_clks, ARRAY_SIZE(vdec_soc_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_vdec_soc[] = {
	{ .compatible = "mediatek,mt8192-vdecsys_soc", },
	{}
};

static struct platform_driver clk_mt8192_vdec_soc_drv = {
	.probe = clk_mt8192_vdec_soc_probe,
	.driver = {
		.name = "clk-mt8192-vdec_soc",
		.of_match_table = of_match_clk_mt8192_vdec_soc,
	},
};

static int __init clk_mt8192_vdec_soc_init(void)
{
	return platform_driver_register(&clk_mt8192_vdec_soc_drv);
}

static void __exit clk_mt8192_vdec_soc_exit(void)
{
	platform_driver_unregister(&clk_mt8192_vdec_soc_drv);
}

arch_initcall(clk_mt8192_vdec_soc_init);
module_exit(clk_mt8192_vdec_soc_exit);
MODULE_LICENSE("GPL");
