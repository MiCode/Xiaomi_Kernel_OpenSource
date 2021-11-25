// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6895-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs dip_nr_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_NR_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_nr_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate dip_nr_dip1_clks[] = {
	GATE_DIP_NR_DIP1(CLK_DIP_NR_DIP1_LARB15, "dip_nr_dip1_larb15",
			"img1_ck"/* parent */, 0),
	GATE_DIP_NR_DIP1(CLK_DIP_NR_DIP1_DIP_NR, "dip_nr_dip1_dip_nr",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc dip_nr_dip1_mcd = {
	.clks = dip_nr_dip1_clks,
	.num_clks = CLK_DIP_NR_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_top_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_TOP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_top_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate dip_top_dip1_clks[] = {
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB10, "dip_dip1_larb10",
			"img1_ck"/* parent */, 0),
	GATE_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP, "dip_dip1_dip_top",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc dip_top_dip1_mcd = {
	.clks = dip_top_dip1_clks,
	.num_clks = CLK_DIP_TOP_DIP1_NR_CLK,
};

static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IMG_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
	}

static const struct mtk_gate img_clks[] = {
	GATE_IMG(CLK_IMG_LARB9, "img_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMG(CLK_IMG_TRAW0, "img_traw0",
			"img1_ck"/* parent */, 1),
	GATE_IMG(CLK_IMG_TRAW1, "img_traw1",
			"img1_ck"/* parent */, 2),
	GATE_IMG(CLK_IMG_VCORE_GALS, "img_vcore_gals",
			"img1_ck"/* parent */, 3),
	GATE_IMG(CLK_IMG_DIP0, "img_dip0",
			"img1_ck"/* parent */, 8),
	GATE_IMG(CLK_IMG_WPE0, "img_wpe0",
			"img1_ck"/* parent */, 9),
	GATE_IMG(CLK_IMG_IPE, "img_ipe",
			"img1_ck"/* parent */, 10),
	GATE_IMG(CLK_IMG_WPE1, "img_wpe1",
			"img1_ck"/* parent */, 12),
	GATE_IMG(CLK_IMG_WPE2, "img_wpe2",
			"img1_ck"/* parent */, 13),
	GATE_IMG(CLK_IMG_ADL_LARB, "img_adl_larb",
			"img1_ck"/* parent */, 14),
	GATE_IMG(CLK_IMG_ADL_TOP0, "img_adl_top0",
			"img1_ck"/* parent */, 15),
	GATE_IMG(CLK_IMG_ADL_TOP1, "img_adl_top1",
			"img1_ck"/* parent */, 16),
	GATE_IMG(CLK_IMG_GALS, "img_gals",
			"img1_ck"/* parent */, 31),
	GATE_IMG_NULL_OP(CLK_IMG_DIP0_DUMMY, "img_dip0_dummy",
			"img1_ck"/* parent */, 8),
	GATE_IMG_NULL_OP(CLK_IMG_WPE0_DUMMY, "img_wpe0_dummy",
			"img1_ck"/* parent */, 9),
	GATE_IMG_NULL_OP(CLK_IMG_IPE_DUMMY, "img_ipe_dummy",
			"img1_ck"/* parent */, 10),
	GATE_IMG_NULL_OP(CLK_IMG_WPE1_DUMMY, "img_wpe1_dummy",
			"img1_ck"/* parent */, 12),
	GATE_IMG_NULL_OP(CLK_IMG_WPE2_DUMMY, "img_wpe2_dummy",
			"img1_ck"/* parent */, 13),
};

static const struct mtk_clk_desc img_mcd = {
	.clks = img_clks,
	.num_clks = CLK_IMG_NR_CLK,
};

static const struct mtk_gate_regs ipe_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ipe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ipe_clks[] = {
	GATE_IPE(CLK_IPE_DPE, "ipe_dpe",
			"ipe_ck"/* parent */, 0),
	GATE_IPE(CLK_IPE_FDVT, "ipe_fdvt",
			"ipe_ck"/* parent */, 1),
	GATE_IPE(CLK_IPE_ME, "ipe_me",
			"ipe_ck"/* parent */, 2),
	GATE_IPE(CLK_IPESYS_TOP, "ipesys_top",
			"ipe_ck"/* parent */, 3),
	GATE_IPE(CLK_IPE_SMI_LARB12, "ipe_smi_larb12",
			"ipe_ck"/* parent */, 4),
};

static const struct mtk_clk_desc ipe_mcd = {
	.clks = ipe_clks,
	.num_clks = CLK_IPE_NR_CLK,
};

static const struct mtk_gate_regs wpe1_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe1_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate wpe1_dip1_clks[] = {
	GATE_WPE1_DIP1(CLK_WPE1_DIP1_LARB11, "wpe1_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_WPE1_DIP1(CLK_WPE1_DIP1_WPE, "wpe1_dip1_wpe",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc wpe1_dip1_mcd = {
	.clks = wpe1_dip1_clks,
	.num_clks = CLK_WPE1_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe2_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe2_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate wpe2_dip1_clks[] = {
	GATE_WPE2_DIP1(CLK_WPE2_DIP1_LARB11, "wpe2_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_WPE2_DIP1(CLK_WPE2_DIP1_WPE, "wpe2_dip1_wpe",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc wpe2_dip1_mcd = {
	.clks = wpe2_dip1_clks,
	.num_clks = CLK_WPE2_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe3_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE3_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe3_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate wpe3_dip1_clks[] = {
	GATE_WPE3_DIP1(CLK_WPE3_DIP1_LARB11, "wpe3_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_WPE3_DIP1(CLK_WPE3_DIP1_WPE, "wpe3_dip1_wpe",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc wpe3_dip1_mcd = {
	.clks = wpe3_dip1_clks,
	.num_clks = CLK_WPE3_DIP1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6895_img[] = {
	{
		.compatible = "mediatek,mt6895-dip_nr_dip1",
		.data = &dip_nr_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6895-dip_top_dip1",
		.data = &dip_top_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6895-imgsys_main",
		.data = &img_mcd,
	}, {
		.compatible = "mediatek,mt6895-ipesys",
		.data = &ipe_mcd,
	}, {
		.compatible = "mediatek,mt6895-wpe1_dip1",
		.data = &wpe1_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6895-wpe2_dip1",
		.data = &wpe2_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6895-wpe3_dip1",
		.data = &wpe3_dip1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6895_img_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6895_img_drv = {
	.probe = clk_mt6895_img_grp_probe,
	.driver = {
		.name = "clk-mt6895-img",
		.of_match_table = of_match_clk_mt6895_img,
	},
};

static int __init clk_mt6895_img_init(void)
{
	return platform_driver_register(&clk_mt6895_img_drv);
}

static void __exit clk_mt6895_img_exit(void)
{
	platform_driver_unregister(&clk_mt6895_img_drv);
}

arch_initcall(clk_mt6895_img_init);
module_exit(clk_mt6895_img_exit);
MODULE_LICENSE("GPL");
