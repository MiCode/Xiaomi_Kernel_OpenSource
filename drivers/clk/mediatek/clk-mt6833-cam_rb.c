/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6833-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP			1

#define INV_OFS			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(25), BIT(25));

static const struct mtk_gate_regs cam_rb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"cam_ck"/* parent */, 2),
};

static int clk_mt6833_cam_rb_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_CAM_RB_NR_CLK);

	mtk_clk_register_gates(node, cam_rb_clks, ARRAY_SIZE(cam_rb_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6833_cam_rb[] = {
	{ .compatible = "mediatek,mt6833-camsys_rawb", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6833_cam_rb_drv = {
	.probe = clk_mt6833_cam_rb_probe,
	.driver = {
		.name = "clk-mt6833-cam_rb",
		.of_match_table = of_match_clk_mt6833_cam_rb,
	},
};

builtin_platform_driver(clk_mt6833_cam_rb_drv);

#else

static struct platform_driver clk_mt6833_cam_rb_drv = {
	.probe = clk_mt6833_cam_rb_probe,
	.driver = {
		.name = "clk-mt6833-cam_rb",
		.of_match_table = of_match_clk_mt6833_cam_rb,
	},
};
static int __init clk_mt6833_cam_rb_platform_init(void)
{
	return platform_driver_register(&clk_mt6833_cam_rb_drv);
}
arch_initcall(clk_mt6833_cam_rb_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
