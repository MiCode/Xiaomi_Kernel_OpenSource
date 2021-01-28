// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6765-clk.h>

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(0x180, 0x184, BIT(25));

static const struct mtk_gate_regs cam_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM(_id, _name, _parent, _shift) {		\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate cam_clks[] __initconst = {
	GATE_CAM(CLK_CAM_LARB3, "cam_larb3", "mm_ck", 0),/*use dummy*/
	GATE_CAM(CLK_CAM_DFP_VAD, "cam_dfp_vad", "mm_ck", 1),
	GATE_CAM(CLK_CAM, "cam", "mm_ck", 6),
	GATE_CAM(CLK_CAMTG, "camtg", "mm_ck", 7),
	GATE_CAM(CLK_CAM_SENINF, "cam_seninf", "mm_ck", 8),
	GATE_CAM(CLK_CAMSV0, "camsv0", "mm_ck", 9),
	GATE_CAM(CLK_CAMSV1, "camsv1", "mm_ck", 10),
	GATE_CAM(CLK_CAMSV2, "camsv2", "mm_ck", 11),
	GATE_CAM(CLK_CAM_CCU, "cam_ccu", "mm_ck", 12),
};

static int clk_mt6765_cam_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_CAM_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_gates(node, cam_clks, ARRAY_SIZE(cam_clks), clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r) {
		kfree(clk_data);
		pr_err("%s(): could not register clock provider: %d\n",
				__func__, r);
	}

	return r;
}

static const struct of_device_id of_match_clk_mt6765_cam[] = {
	{ .compatible = "mediatek,mt6765-camsys", },
	{}
};

static struct platform_driver clk_mt6765_cam_drv = {
	.probe = clk_mt6765_cam_probe,
	.driver = {
		.name = "clk-mt6765-cam",
		.of_match_table = of_match_clk_mt6765_cam,
	},
};

static int __init clk_mt6765_cam_init(void)
{
	return platform_driver_register(&clk_mt6765_cam_drv);
}

static void __exit clk_mt6765_cam_exit(void)
{
}

postcore_initcall(clk_mt6765_cam_init);
module_exit(clk_mt6765_cam_exit);
MODULE_LICENSE("GPL");
