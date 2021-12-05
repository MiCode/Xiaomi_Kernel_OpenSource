// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6855-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs cam_ra_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA(CLK_CAM_RA_LARBX, "cam_ra_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RA(CLK_CAM_RA_CAM, "cam_ra_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_ra_mcd = {
	.clks = cam_ra_clks,
	.num_clks = CLK_CAM_RA_NR_CLK,
};

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
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_rb_mcd = {
	.clks = cam_rb_clks,
	.num_clks = CLK_CAM_RB_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6855_cam[] = {
	{
		.compatible = "mediatek,mt6855-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6855-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6855_cam_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6855_cam_drv = {
	.probe = clk_mt6855_cam_grp_probe,
	.driver = {
		.name = "clk-mt6855-cam",
		.of_match_table = of_match_clk_mt6855_cam,
	},
};

static int __init clk_mt6855_cam_init(void)
{
	return platform_driver_register(&clk_mt6855_cam_drv);
}

static void __exit clk_mt6855_cam_exit(void)
{
	platform_driver_unregister(&clk_mt6855_cam_drv);
}

arch_initcall(clk_mt6855_cam_init);
module_exit(clk_mt6855_cam_exit);
MODULE_LICENSE("GPL");
