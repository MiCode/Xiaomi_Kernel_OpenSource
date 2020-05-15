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

static const struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vdec2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDEC0(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec0_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr_inv)

#define GATE_VDEC1(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec1_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr_inv)

#define GATE_VDEC2(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &vdec2_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr_inv)

static const struct mtk_gate vdec_clks[] = {
	/* VDEC0 */
	GATE_VDEC0(CLK_VDEC_VDEC, "vdec_vdec", "vdec_sel", 0),
	GATE_VDEC0(CLK_VDEC_ACTIVE, "vdec_active", "vdec_sel", 4),
	/* VDEC1 */
	GATE_VDEC1(CLK_VDEC_LAT, "vdec_lat", "vdec_sel", 0),
	GATE_VDEC1(CLK_VDEC_LAT_ACTIVE, "vdec_lat_active", "vdec_sel", 4),
	/* VDEC2 */
	GATE_VDEC2(CLK_VDEC_LARB1, "vdec_larb1", "vdec_sel", 0),
};

static int clk_mt8192_vdec_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_VDEC_NR_CLK);

	mtk_clk_register_gates(node, vdec_clks, ARRAY_SIZE(vdec_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_vdec[] = {
	{ .compatible = "mediatek,mt8192-vdecsys", },
	{}
};

static struct platform_driver clk_mt8192_vdec_drv = {
	.probe = clk_mt8192_vdec_probe,
	.driver = {
		.name = "clk-mt8192-vdec",
		.of_match_table = of_match_clk_mt8192_vdec,
	},
};

static int __init clk_mt8192_vdec_init(void)
{
	return platform_driver_register(&clk_mt8192_vdec_drv);
}

static void __exit clk_mt8192_vdec_exit(void)
{
	platform_driver_unregister(&clk_mt8192_vdec_drv);
}

arch_initcall(clk_mt8192_vdec_init);
module_exit(clk_mt8192_vdec_exit);
MODULE_LICENSE("GPL");
