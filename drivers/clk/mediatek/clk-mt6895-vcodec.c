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

static const struct mtk_gate_regs vde20_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde21_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde22_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
};

#define GATE_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE22(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde22_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate vde2_clks[] = {
	/* VDE20 */
	GATE_VDE20(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE20(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE20(CLK_VDE2_VDEC_CKEN_ENG, "vde2_vdec_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE21 */
	GATE_VDE21(CLK_VDE2_LAT_CKEN, "vde2_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE21(CLK_VDE2_LAT_ACTIVE, "vde2_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE21(CLK_VDE2_LAT_CKEN_ENG, "vde2_lat_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE22 */
	GATE_VDE22(CLK_VDE2_LARB1_CKEN, "vde2_larb1_cken",
			"vdec_ck"/* parent */, 0),
};

static const struct mtk_clk_desc vde2_mcd = {
	.clks = vde2_clks,
	.num_clks = CLK_VDE2_NR_CLK,
};

static const struct mtk_gate_regs vde10_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde11_cg_regs = {
	.set_ofs = 0x190,
	.clr_ofs = 0x190,
	.sta_ofs = 0x190,
};

static const struct mtk_gate_regs vde12_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde13_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
};

#define GATE_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_VDE12(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde12_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE13(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde13_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate vde1_clks[] = {
	/* VDE10 */
	GATE_VDE10(CLK_VDE1_VDEC_CKEN, "vde1_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE10(CLK_VDE1_VDEC_ACTIVE, "vde1_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE10(CLK_VDE1_VDEC_CKEN_ENG, "vde1_vdec_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE11 */
	GATE_VDE11(CLK_VDE1_MINI_MDP_CKEN_CFG_RG, "vde1_mini_mdp_cken",
			"vdec_ck"/* parent */, 0),
	/* VDE12 */
	GATE_VDE12(CLK_VDE1_LAT_CKEN, "vde1_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE12(CLK_VDE1_LAT_ACTIVE, "vde1_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE12(CLK_VDE1_LAT_CKEN_ENG, "vde1_lat_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE13 */
	GATE_VDE13(CLK_VDE1_LARB1_CKEN, "vde1_larb1_cken",
			"vdec_ck"/* parent */, 0),
};

static const struct mtk_clk_desc vde1_mcd = {
	.clks = vde1_clks,
	.num_clks = CLK_VDE1_NR_CLK,
};

static const struct mtk_gate_regs ven1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_VEN1(CLK_VEN1_CKE0_LARB, "ven1_cke0_larb",
			"venc_ck"/* parent */, 0),
	GATE_VEN1(CLK_VEN1_CKE1_VENC, "ven1_cke1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_cke2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN1(CLK_VEN1_CKE3_JPGDEC, "ven1_cke3_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN1(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_cke4_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_cke5_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN1(CLK_VEN1_CKE6_GALS_SRAM, "ven1_cke6_gals_sram",
			"venc_ck"/* parent */, 31),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct mtk_gate_regs ven2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VEN2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate ven2_clks[] = {
	GATE_VEN2(CLK_VEN2_CKE0_LARB, "ven2_cke0_larb",
			"venc_ck"/* parent */, 0),
	GATE_VEN2(CLK_VEN2_CKE1_VENC, "ven2_cke1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN2(CLK_VEN2_CKE2_JPGENC, "ven2_cke2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN2(CLK_VEN2_CKE3_JPGDEC, "ven2_cke3_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN2(CLK_VEN2_CKE4_JPGDEC_C1, "ven2_cke4_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VEN2(CLK_VEN2_CKE5_GALS, "ven2_cke5_gals",
			"venc_ck"/* parent */, 28),
	GATE_VEN2(CLK_VEN2_CKE6_GALS_SRAM, "ven2_cke6_gals_sram",
			"venc_ck"/* parent */, 31),
};

static const struct mtk_clk_desc ven2_mcd = {
	.clks = ven2_clks,
	.num_clks = CLK_VEN2_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6895_vcodec[] = {
	{
		.compatible = "mediatek,mt6895-vdec_gcon_base",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6895-vdec_soc_gcon_base",
		.data = &vde1_mcd,
	}, {
		.compatible = "mediatek,mt6895-vencsys",
		.data = &ven1_mcd,
	}, {
		.compatible = "mediatek,mt6895-vencsys_c1",
		.data = &ven2_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6895_vcodec_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6895_vcodec_drv = {
	.probe = clk_mt6895_vcodec_grp_probe,
	.driver = {
		.name = "clk-mt6895-vcodec",
		.of_match_table = of_match_clk_mt6895_vcodec,
	},
};

static int __init clk_mt6895_vcodec_init(void)
{
	return platform_driver_register(&clk_mt6895_vcodec_drv);
}

static void __exit clk_mt6895_vcodec_exit(void)
{
	platform_driver_unregister(&clk_mt6895_vcodec_drv);
}

arch_initcall(clk_mt6895_vcodec_init);
module_exit(clk_mt6895_vcodec_exit);
MODULE_LICENSE("GPL");
