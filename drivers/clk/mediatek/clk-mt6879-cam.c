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

#include <dt-bindings/clock/mt6879-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs cam_mr_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_MR(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_mr_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_mr_clks[] = {
	GATE_CAM_MR(CLK_CAM_MR_LARBX, "cam_mr_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_MR(CLK_CAM_MR_CAMTG, "cam_mr_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_MR(CLK_CAM_MR_MRAW0, "cam_mr_mraw0",
			"cam_ck"/* parent */, 3),
	GATE_CAM_MR(CLK_CAM_MR_MRAW1, "cam_mr_mraw1",
			"cam_ck"/* parent */, 4),
	GATE_CAM_MR(CLK_CAM_MR_MRAW2, "cam_mr_mraw2",
			"cam_ck"/* parent */, 5),
	GATE_CAM_MR(CLK_CAM_MR_MRAW3, "cam_mr_mraw3",
			"cam_ck"/* parent */, 6),
	GATE_CAM_MR(CLK_CAM_MR_PDA0, "cam_mr_pda0",
			"cam_ck"/* parent */, 7),
	GATE_CAM_MR(CLK_CAM_MR_PDA1, "cam_mr_pda1",
			"cam_ck"/* parent */, 8),
};

static const struct mtk_clk_desc cam_mr_mcd = {
	.clks = cam_mr_clks,
	.num_clks = ARRAY_SIZE(cam_mr_clks),
};

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
	.num_clks = ARRAY_SIZE(cam_ra_clks),
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
	.num_clks = ARRAY_SIZE(cam_rb_clks),
};

static const struct mtk_gate_regs cam_ya_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YA(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_ya_clks[] = {
	GATE_CAM_YA(CLK_CAM_YA_LARBX, "cam_ya_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YA(CLK_CAM_YA_CAM, "cam_ya_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YA(CLK_CAM_YA_CAMTG, "cam_ya_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_ya_mcd = {
	.clks = cam_ya_clks,
	.num_clks = ARRAY_SIZE(cam_ya_clks),
};

static const struct mtk_gate_regs cam_yb_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_yb_clks[] = {
	GATE_CAM_YB(CLK_CAM_YB_LARBX, "cam_yb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YB(CLK_CAM_YB_CAM, "cam_yb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YB(CLK_CAM_YB_CAMTG, "cam_yb_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_yb_mcd = {
	.clks = cam_yb_clks,
	.num_clks = ARRAY_SIZE(cam_yb_clks),
};

static const struct mtk_gate_regs cam_m_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_M(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_m_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate cam_m_clks[] = {
	GATE_CAM_M(CLK__LARB13_CON, "_larb13_con",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M(CLK__LARB14_CON, "_larb14_con",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M(CLK__CAM_CON, "_cam_con",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M(CLK__CAM_SUBA_CON, "_cam_suba_con",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M(CLK__CAM_SUBB_CON, "_cam_subb_con",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M(CLK__CAM_SUBC_CON, "_cam_subc_con",
			"cam_ck"/* parent */, 5),
	GATE_CAM_M(CLK__CAM_MRAW_CON, "_cam_mraw_con",
			"cam_ck"/* parent */, 6),
	GATE_CAM_M(CLK__CAMTG_CON, "_camtg_con",
			"camtm_ck"/* parent */, 7),
	GATE_CAM_M(CLK__SENINF_CON, "_seninf_con",
			"cam_ck"/* parent */, 8),
	GATE_CAM_M(CLK__GCAMSVA_CON, "_gcamsva_con",
			"cam_ck"/* parent */, 9),
	GATE_CAM_M(CLK__GCAMSVB_CON, "_gcamsvb_con",
			"cam_ck"/* parent */, 10),
	GATE_CAM_M(CLK__GCAMSVC_CON, "_gcamsvc_con",
			"cam_ck"/* parent */, 11),
	GATE_CAM_M(CLK__GCAMSVD_CON, "_gcamsvd_con",
			"cam_ck"/* parent */, 12),
	GATE_CAM_M(CLK__GCAMSVE_CON, "_gcamsve_con",
			"cam_ck"/* parent */, 13),
	GATE_CAM_M(CLK__GCAMSVF_CON, "_gcamsvf_con",
			"cam_ck"/* parent */, 14),
	GATE_CAM_M(CLK__GCAMSVG_CON, "_gcamsvg_con",
			"cam_ck"/* parent */, 15),
	GATE_CAM_M(CLK__GCAMSVH_CON, "_gcamsvh_con",
			"cam_ck"/* parent */, 16),
	GATE_CAM_M(CLK__GCAMSVI_CON, "_gcamsvi_con",
			"cam_ck"/* parent */, 17),
	GATE_CAM_M(CLK__GCAMSVJ_CON, "_gcamsvj_con",
			"cam_ck"/* parent */, 18),
	GATE_CAM_M(CLK__CAMSV_TOP_CON, "_camsv_con",
			"cam_ck"/* parent */, 19),
	GATE_CAM_M(CLK__CAMSV_CQ_A_CON, "_camsv_cq_a_con",
			"cam_ck"/* parent */, 20),
	GATE_CAM_M(CLK__CAMSV_CQ_B_CON, "_camsv_cq_b_con",
			"cam_ck"/* parent */, 21),
	GATE_CAM_M(CLK__CAMSV_CQ_C_CON, "_camsv_cq_c_con",
			"cam_ck"/* parent */, 22),
	GATE_CAM_M(CLK__ADL_CON, "_adl_con",
			"cam_ck"/* parent */, 23),
	GATE_CAM_M(CLK__ASG_CON, "_asg_con",
			"cam_ck"/* parent */, 24),
	GATE_CAM_M(CLK__PDA0_CON, "_pda0_con",
			"cam_ck"/* parent */, 25),
	GATE_CAM_M(CLK__PDA1_CON, "_pda1_con",
			"cam_ck"/* parent */, 26),
	GATE_CAM_M(CLK__PDA2_CON, "_pda2_con",
			"cam_ck"/* parent */, 27),
	GATE_CAM_M(CLK__FAKE_ENG_CON, "_fake_eng_con",
			"cam_ck"/* parent */, 28),
	GATE_CAM_M(CLK__CAM2MM0_GALS_CON, "_cam2mm0_gals_con",
			"cam_ck"/* parent */, 29),
	GATE_CAM_M(CLK__CAM2MM1_GALS_CON, "_cam2mm1_gals_con",
			"cam_ck"/* parent */, 30),
	GATE_CAM_M(CLK__CAM2SYS_GALS_CON, "_cam2sys_gals_con",
			"cam_ck"/* parent */, 31),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = ARRAY_SIZE(cam_m_clks),
};

static const struct of_device_id of_match_clk_mt6879_cam[] = {
	{
		.compatible = "mediatek,mt6879-camsys_mraw",
		.data = &cam_mr_mcd,
	}, {
		.compatible = "mediatek,mt6879-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6879-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6879-camsys_yuva",
		.data = &cam_ya_mcd,
	}, {
		.compatible = "mediatek,mt6879-camsys_yuvb",
		.data = &cam_yb_mcd,
	}, {
		.compatible = "mediatek,mt6879-cam_main_r1a",
		.data = &cam_m_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6879_cam_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6879_cam_drv = {
	.probe = clk_mt6879_cam_grp_probe,
	.driver = {
		.name = "clk-mt6879-cam",
		.of_match_table = of_match_clk_mt6879_cam,
	},
};

static int __init clk_mt6879_cam_init(void)
{
	return platform_driver_register(&clk_mt6879_cam_drv);
}

static void __exit clk_mt6879_cam_exit(void)
{
	platform_driver_unregister(&clk_mt6879_cam_drv);
}

arch_initcall(clk_mt6879_cam_init);
module_exit(clk_mt6879_cam_exit);
MODULE_LICENSE("GPL");
