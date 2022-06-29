// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6985-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs dip_nr1_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs dip_nr1_dip1_hwv_regs = {
	.set_ofs = 0x0008,
	.clr_ofs = 0x000C,
	.sta_ofs = 0x1C04,
};

#define GATE_DIP_NR1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_nr1_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DIP_NR1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dip_nr1_dip1_cg_regs,			\
		.hwv_regs = &dip_nr1_dip1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate dip_nr1_dip1_clks[] = {
	GATE_HWV_DIP_NR1_DIP1(CLK_DIP_NR1_DIP1_LARB, "dip_nr1_dip1_larb",
			"img1_ck"/* parent */, 0),
	GATE_HWV_DIP_NR1_DIP1(CLK_DIP_NR1_DIP1_DIP_NR1, "dip_nr1_dip1_dip_nr1",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc dip_nr1_dip1_mcd = {
	.clks = dip_nr1_dip1_clks,
	.num_clks = CLK_DIP_NR1_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_nr2_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs dip_nr2_dip1_hwv_regs = {
	.set_ofs = 0x0010,
	.clr_ofs = 0x0014,
	.sta_ofs = 0x1C08,
};

#define GATE_DIP_NR2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_nr2_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DIP_NR2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dip_nr2_dip1_cg_regs,			\
		.hwv_regs = &dip_nr2_dip1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate dip_nr2_dip1_clks[] = {
	GATE_HWV_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_LARB15, "dip_nr2_dip1_larb15",
			"img1_ck"/* parent */, 0),
	GATE_HWV_DIP_NR2_DIP1(CLK_DIP_NR2_DIP1_DIP_NR, "dip_nr2_dip1_dip_nr",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc dip_nr2_dip1_mcd = {
	.clks = dip_nr2_dip1_clks,
	.num_clks = CLK_DIP_NR2_DIP1_NR_CLK,
};

static const struct mtk_gate_regs dip_top_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs dip_top_dip1_hwv_regs = {
	.set_ofs = 0x0018,
	.clr_ofs = 0x001C,
	.sta_ofs = 0x1C0C,
};

#define GATE_DIP_TOP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dip_top_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_DIP_TOP_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dip_top_dip1_cg_regs,			\
		.hwv_regs = &dip_top_dip1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate dip_top_dip1_clks[] = {
	GATE_HWV_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_LARB10, "dip_dip1_larb10",
			"img1_ck"/* parent */, 0),
	GATE_HWV_DIP_TOP_DIP1(CLK_DIP_TOP_DIP1_DIP_TOP, "dip_dip1_dip_top",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc dip_top_dip1_mcd = {
	.clks = dip_top_dip1_clks,
	.num_clks = CLK_DIP_TOP_DIP1_NR_CLK,
};

static const struct mtk_gate_regs img0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs img0_hwv_regs = {
	.set_ofs = 0x0038,
	.clr_ofs = 0x003C,
	.sta_ofs = 0x1C1C,
};

static const struct mtk_gate_regs img1_cg_regs = {
	.set_ofs = 0x54,
	.clr_ofs = 0x58,
	.sta_ofs = 0x50,
};

static const struct mtk_gate_regs img1_hwv_regs = {
	.set_ofs = 0x0030,
	.clr_ofs = 0x0034,
	.sta_ofs = 0x1C18,
};

#define GATE_IMG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_IMG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &img0_cg_regs,			\
		.hwv_regs = &img0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_IMG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &img1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_IMG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &img1_cg_regs,			\
		.hwv_regs = &img1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate img_clks[] = {
	/* IMG0 */
	GATE_HWV_IMG0(CLK_IMG_LARB9, "img_larb9",
			"img1_ck"/* parent */, 0),
	GATE_HWV_IMG0(CLK_IMG_TRAW0, "img_traw0",
			"img1_ck"/* parent */, 1),
	GATE_HWV_IMG0(CLK_IMG_TRAW1, "img_traw1",
			"img1_ck"/* parent */, 2),
	GATE_HWV_IMG0(CLK_IMG_VCORE_GALS, "img_vcore_gals",
			"img1_ck"/* parent */, 3),
	GATE_HWV_IMG0(CLK_IMG_DIP0, "img_dip0",
			"img1_ck"/* parent */, 8),
	GATE_HWV_IMG0(CLK_IMG_WPE0, "img_wpe0",
			"img1_ck"/* parent */, 9),
	GATE_HWV_IMG0(CLK_IMG_IPE, "img_ipe",
			"img1_ck"/* parent */, 10),
	GATE_HWV_IMG0(CLK_IMG_WPE1, "img_wpe1",
			"img1_ck"/* parent */, 12),
	GATE_HWV_IMG0(CLK_IMG_WPE2, "img_wpe2",
			"img1_ck"/* parent */, 13),
	GATE_HWV_IMG0(CLK_IMG_SMI_ADL_LARB0, "img_smi_adl_larb0",
			"img1_ck"/* parent */, 14),
	GATE_HWV_IMG0(CLK_IMG_ADL0, "img_adl0",
			"img1_ck"/* parent */, 15),
	GATE_HWV_IMG0(CLK_IMG_AVS, "img_avs",
			"imgavs_ck"/* parent */, 17),
	GATE_HWV_IMG0(CLK_IMG_GALS, "img_gals",
			"img1_ck"/* parent */, 31),
	/* IMG1 */
	GATE_HWV_IMG1(CLK_IMG_FDVT, "img_fdvt",
			"ipe_ck"/* parent */, 0),
	GATE_HWV_IMG1(CLK_IMG_ME, "img_me",
			"ipe_ck"/* parent */, 1),
	GATE_HWV_IMG1(CLK_IMG_MMG, "img_mmg",
			"ipe_ck"/* parent */, 2),
	GATE_HWV_IMG1(CLK_IMG_LARB12, "img_larb12",
			"ipe_ck"/* parent */, 3),
};

static const struct mtk_clk_desc img_mcd = {
	.clks = img_clks,
	.num_clks = CLK_IMG_NR_CLK,
};

static const struct mtk_gate_regs traw_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs traw_dip1_hwv_regs = {
	.set_ofs = 0x0060,
	.clr_ofs = 0x0064,
	.sta_ofs = 0x1C30,
};

#define GATE_TRAW_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &traw_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_TRAW_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &traw_dip1_cg_regs,			\
		.hwv_regs = &traw_dip1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate traw_dip1_clks[] = {
	GATE_HWV_TRAW_DIP1(CLK_TRAW_DIP1_LARB28, "traw_dip1_larb28",
			"img1_ck"/* parent */, 0),
	GATE_HWV_TRAW_DIP1(CLK_TRAW_DIP1_TRAW, "traw_dip1_traw",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc traw_dip1_mcd = {
	.clks = traw_dip1_clks,
	.num_clks = CLK_TRAW_DIP1_NR_CLK,
};

static const struct mtk_gate_regs wpe1_dip1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs wpe1_dip1_hwv_regs = {
	.set_ofs = 0x0078,
	.clr_ofs = 0x007C,
	.sta_ofs = 0x1C3C,
};

#define GATE_WPE1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe1_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_WPE1_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &wpe1_dip1_cg_regs,			\
		.hwv_regs = &wpe1_dip1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate wpe1_dip1_clks[] = {
	GATE_WPE1_DIP1(CLK_WPE1_DIP1_LARB11, "wpe1_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_HWV_WPE1_DIP1(CLK_WPE1_DIP1_WPE, "wpe1_dip1_wpe",
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

static const struct mtk_gate_regs wpe2_dip1_hwv_regs = {
	.set_ofs = 0x0080,
	.clr_ofs = 0x0084,
	.sta_ofs = 0x1C40,
};

#define GATE_WPE2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe2_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_WPE2_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &wpe2_dip1_cg_regs,			\
		.hwv_regs = &wpe2_dip1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate wpe2_dip1_clks[] = {
	GATE_WPE2_DIP1(CLK_WPE2_DIP1_LARB11, "wpe2_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_HWV_WPE2_DIP1(CLK_WPE2_DIP1_WPE, "wpe2_dip1_wpe",
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

static const struct mtk_gate_regs wpe3_dip1_hwv_regs = {
	.set_ofs = 0x0088,
	.clr_ofs = 0x008C,
	.sta_ofs = 0x1C44,
};

#define GATE_WPE3_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &wpe3_dip1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_WPE3_DIP1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &wpe3_dip1_cg_regs,			\
		.hwv_regs = &wpe3_dip1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate wpe3_dip1_clks[] = {
	GATE_WPE3_DIP1(CLK_WPE3_DIP1_LARB11, "wpe3_dip1_larb11",
			"img1_ck"/* parent */, 0),
	GATE_HWV_WPE3_DIP1(CLK_WPE3_DIP1_WPE, "wpe3_dip1_wpe",
			"img1_ck"/* parent */, 1),
};

static const struct mtk_clk_desc wpe3_dip1_mcd = {
	.clks = wpe3_dip1_clks,
	.num_clks = CLK_WPE3_DIP1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6985_img[] = {
	{
		.compatible = "mediatek,mt6985-dip_nr1_dip1",
		.data = &dip_nr1_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6985-dip_nr2_dip1",
		.data = &dip_nr2_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6985-dip_top_dip1",
		.data = &dip_top_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6985-imgsys_main",
		.data = &img_mcd,
	}, {
		.compatible = "mediatek,mt6985-traw_dip1",
		.data = &traw_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6985-wpe1_dip1",
		.data = &wpe1_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6985-wpe2_dip1",
		.data = &wpe2_dip1_mcd,
	}, {
		.compatible = "mediatek,mt6985-wpe3_dip1",
		.data = &wpe3_dip1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6985_img_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6985_img_drv = {
	.probe = clk_mt6985_img_grp_probe,
	.driver = {
		.name = "clk-mt6985-img",
		.of_match_table = of_match_clk_mt6985_img,
	},
};

module_platform_driver(clk_mt6985_img_drv);
MODULE_LICENSE("GPL");
