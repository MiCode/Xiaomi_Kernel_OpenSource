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

static const struct mtk_gate_regs cam_rawa_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RAWA(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &cam_rawa_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate cam_rawa_clks[] = {
	GATE_CAM_RAWA(CLK_CAM_RAWA_LARBX, "cam_rawa_larbx", "cam_sel", 0),
	GATE_CAM_RAWA(CLK_CAM_RAWA_CAM, "cam_rawa_cam", "cam_sel", 1),
	GATE_CAM_RAWA(CLK_CAM_RAWA_CAMTG, "cam_rawa_camtg", "cam_sel", 2),
};

static int clk_mt8192_cam_rawa_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_CAM_RAWA_NR_CLK);

	mtk_clk_register_gates(node, cam_rawa_clks, ARRAY_SIZE(cam_rawa_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_cam_rawa[] = {
	{ .compatible = "mediatek,mt8192-camsys_rawa", },
	{}
};

static struct platform_driver clk_mt8192_cam_rawa_drv = {
	.probe = clk_mt8192_cam_rawa_probe,
	.driver = {
		.name = "clk-mt8192-cam_rawa",
		.of_match_table = of_match_clk_mt8192_cam_rawa,
	},
};

static int __init clk_mt8192_cam_rawa_init(void)
{
	return platform_driver_register(&clk_mt8192_cam_rawa_drv);
}

static void __exit clk_mt8192_cam_rawa_exit(void)
{
	platform_driver_unregister(&clk_mt8192_cam_rawa_drv);
}

arch_initcall(clk_mt8192_cam_rawa_init);
module_exit(clk_mt8192_cam_rawa_exit);
MODULE_LICENSE("GPL");
