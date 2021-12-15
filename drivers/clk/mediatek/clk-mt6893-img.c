// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs imgsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY1(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &imgsys1_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate imgsys1_clks[] = {
	GATE_DUMMY1(CLK_IMGSYS1_LARB9, "imgsys1_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMGSYS1(CLK_IMGSYS1_LARB10, "imgsys1_larb10",
			"img1_ck"/* parent */, 1),
	GATE_IMGSYS1(CLK_IMGSYS1_DIP, "imgsys1_dip",
			"img1_ck"/* parent */, 2),
	GATE_IMGSYS1(CLK_IMGSYS1_MFB, "imgsys1_mfb",
			"img1_ck"/* parent */, 6),
	GATE_IMGSYS1(CLK_IMGSYS1_WPE, "imgsys1_wpe",
			"img1_ck"/* parent */, 7),
	GATE_IMGSYS1(CLK_IMGSYS1_MSS, "imgsys1_mss",
			"img1_ck"/* parent */, 8),
};

static const struct mtk_clk_desc imgsys1_mcd = {
	.clks = imgsys1_clks,
	.num_clks = CLK_IMGSYS1_NR_CLK,
};

static const struct mtk_gate_regs imgsys2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY2(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &imgsys2_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate imgsys2_clks[] = {
	GATE_DUMMY1(CLK_IMGSYS2_LARB11, "imgsys2_larb10",
			"img2_ck"/* parent */, 0),
	GATE_IMGSYS2(CLK_IMGSYS2_LARB12, "imgsys2_larb12",
			"img2_ck"/* parent */, 1),
	GATE_IMGSYS2(CLK_IMGSYS2_DIP, "imgsys2_dip",
			"img2_ck"/* parent */, 2),
	GATE_IMGSYS2(CLK_IMGSYS2_WPE, "imgsys2_wpe",
			"img2_ck"/* parent */, 7),
};

static const struct mtk_clk_desc imgsys2_mcd = {
	.clks = imgsys2_clks,
	.num_clks = CLK_IMGSYS2_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6893_img[] = {
	{
		.compatible = "mediatek,mt6893-imgsys1",
		.data = &imgsys1_mcd,
	}, {
		.compatible = "mediatek,mt6893-imgsys2",
		.data = &imgsys2_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6893_img_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6893_img_drv = {
	.probe = clk_mt6893_img_grp_probe,
	.driver = {
		.name = "clk-mt6893-img",
		.of_match_table = of_match_clk_mt6893_img,
	},
};

static int __init clk_mt6893_img_init(void)
{
	return platform_driver_register(&clk_mt6893_img_drv);
}

static void __exit clk_mt6893_img_exit(void)
{
	platform_driver_unregister(&clk_mt6893_img_drv);
}

postcore_initcall(clk_mt6893_img_init);
module_exit(clk_mt6893_img_exit);
MODULE_LICENSE("GPL");
