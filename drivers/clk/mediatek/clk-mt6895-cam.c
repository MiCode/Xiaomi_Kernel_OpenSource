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

#define GATE_CAM_MR_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_mr_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
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
	GATE_CAM_MR(CLK_CAM_MR_LARBX_DUMMY, "cam_mr_larbx_dummy",
			"cam_ck"/* parent */, 0),
};

static const struct mtk_clk_desc cam_mr_mcd = {
	.clks = cam_mr_clks,
	.num_clks = CLK_CAM_MR_NR_CLK,
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

#define GATE_CAM_RA_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ra_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA(CLK_CAM_RA_LARBX, "cam_ra_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RA(CLK_CAM_RA_CAM, "cam_ra_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RA(CLK_CAM_RA_CAMTG, "cam_ra_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_RA_NULL_OP(CLK_CAM_RA_LARBX_DUMMY, "cam_ra_larbx_dummy",
			"cam_ck"/* parent */, 0),
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

#define GATE_CAM_RB_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB(CLK_CAM_RB_LARBX, "cam_rb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RB(CLK_CAM_RB_CAM, "cam_rb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RB(CLK_CAM_RB_CAMTG, "cam_rb_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_RB_NULL_OP(CLK_CAM_RB_LARBX_DUMMY, "cam_rb_larbx_dummy",
			"cam_ck"/* parent */, 0),
};

static const struct mtk_clk_desc cam_rb_mcd = {
	.clks = cam_rb_clks,
	.num_clks = CLK_CAM_RB_NR_CLK,
};

static const struct mtk_gate_regs cam_rc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_RC_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_rc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
	}

static const struct mtk_gate cam_rc_clks[] = {
	GATE_CAM_RC(CLK_CAM_RC_LARBX, "cam_rc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RC(CLK_CAM_RC_CAM, "cam_rc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RC(CLK_CAM_RC_CAMTG, "cam_rc_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_RC_NULL_OP(CLK_CAM_RC_LARBX_DUMMY, "cam_rc_larbx_dummy",
			"cam_ck"/* parent */, 0),
};

static const struct mtk_clk_desc cam_rc_mcd = {
	.clks = cam_rc_clks,
	.num_clks = CLK_CAM_RC_NR_CLK,
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

#define GATE_CAM_YA_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_ya_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
	}

static const struct mtk_gate cam_ya_clks[] = {
	GATE_CAM_YA(CLK_CAM_YA_LARBX, "cam_ya_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YA(CLK_CAM_YA_CAM, "cam_ya_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YA(CLK_CAM_YA_CAMTG, "cam_ya_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_YA_NULL_OP(CLK_CAM_YA_LARBX_DUMMY, "cam_ya_larbx_dummy",
			"cam_ck"/* parent */, 0),
};

static const struct mtk_clk_desc cam_ya_mcd = {
	.clks = cam_ya_clks,
	.num_clks = CLK_CAM_YA_NR_CLK,
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

#define GATE_CAM_YB_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
	}

static const struct mtk_gate cam_yb_clks[] = {
	GATE_CAM_YB(CLK_CAM_YB_LARBX, "cam_yb_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YB(CLK_CAM_YB_CAM, "cam_yb_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YB(CLK_CAM_YB_CAMTG, "cam_yb_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_YB_NULL_OP(CLK_CAM_YB_LARBX_DUMMY, "cam_yb_larbx_dummy",
			"cam_ck"/* parent */, 0),
};

static const struct mtk_clk_desc cam_yb_mcd = {
	.clks = cam_yb_clks,
	.num_clks = CLK_CAM_YB_NR_CLK,
};

static const struct mtk_gate_regs cam_yc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_YC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_CAM_YC_NULL_OP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &cam_yc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummys,	\
	}

static const struct mtk_gate cam_yc_clks[] = {
	GATE_CAM_YC(CLK_CAM_YC_LARBX, "cam_yc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_YC(CLK_CAM_YC_CAM, "cam_yc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_YC(CLK_CAM_YC_CAMTG, "cam_yc_camtg",
			"camtm_ck"/* parent */, 2),
	GATE_CAM_YC_NULL_OP(CLK_CAM_YC_LARBX_DUMMY, "cam_yc_larbx_dummy",
			"cam_ck"/* parent */, 0),
};

static const struct mtk_clk_desc cam_yc_mcd = {
	.clks = cam_yc_clks,
	.num_clks = CLK_CAM_YC_NR_CLK,
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

#define GATE_CAM_M_NULL_OP(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_m_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static const struct mtk_gate cam_m_clks[] = {
	GATE_CAM_M(CLK_CAM_MAIN_LARB13_CON, "cam_m_larb13_con",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M(CLK_CAM_MAIN_LARB14_CON, "cam_m_larb14_con",
			"cam_ck"/* parent */, 1),
	GATE_CAM_M(CLK_CAM_MAIN_CAM_CON, "cam_m_cam_con",
			"cam_ck"/* parent */, 2),
	GATE_CAM_M(CLK_CAM_MAIN_CAM_SUBA_CON, "cam_m_cam_suba_con",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M(CLK_CAM_MAIN_CAM_SUBB_CON, "cam_m_cam_subb_con",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M(CLK_CAM_MAIN_CAM_SUBC_CON, "cam_m_cam_subc_con",
			"cam_ck"/* parent */, 5),
	GATE_CAM_M(CLK_CAM_MAIN_CAM_MRAW_CON, "cam_m_cam_mraw_con",
			"cam_ck"/* parent */, 6),
	GATE_CAM_M(CLK_CAM_MAIN_CAMTG_CON, "cam_m_camtg_con",
			"camtm_ck"/* parent */, 7),
	GATE_CAM_M(CLK_CAM_MAIN_SENINF_CON, "cam_m_seninf_con",
			"cam_ck"/* parent */, 8),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVA_CON, "cam_m_gcamsva_con",
			"cam_ck"/* parent */, 9),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVB_CON, "cam_m_gcamsvb_con",
			"cam_ck"/* parent */, 10),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVC_CON, "cam_m_gcamsvc_con",
			"cam_ck"/* parent */, 11),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVD_CON, "cam_m_gcamsvd_con",
			"cam_ck"/* parent */, 12),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVE_CON, "cam_m_gcamsve_con",
			"cam_ck"/* parent */, 13),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVF_CON, "cam_m_gcamsvf_con",
			"cam_ck"/* parent */, 14),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVG_CON, "cam_m_gcamsvg_con",
			"cam_ck"/* parent */, 15),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVH_CON, "cam_m_gcamsvh_con",
			"cam_ck"/* parent */, 16),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVI_CON, "cam_m_gcamsvi_con",
			"cam_ck"/* parent */, 17),
	GATE_CAM_M(CLK_CAM_MAIN_GCAMSVJ_CON, "cam_m_gcamsvj_con",
			"cam_ck"/* parent */, 18),
	GATE_CAM_M(CLK_CAM_MAIN_CAMSV_TOP_CON, "cam_m_camsv_con",
			"cam_ck"/* parent */, 19),
	GATE_CAM_M(CLK_CAM_MAIN_CAMSV_CQ_A_CON, "cam_m_camsv_cq_a_con",
			"cam_ck"/* parent */, 20),
	GATE_CAM_M(CLK_CAM_MAIN_CAMSV_CQ_B_CON, "cam_m_camsv_cq_b_con",
			"cam_ck"/* parent */, 21),
	GATE_CAM_M(CLK_CAM_MAIN_CAMSV_CQ_C_CON, "cam_m_camsv_cq_c_con",
			"cam_ck"/* parent */, 22),
	GATE_CAM_M(CLK_CAM_MAIN_ADL_CON, "cam_m_adl_con",
			"cam_ck"/* parent */, 23),
	GATE_CAM_M(CLK_CAM_MAIN_ASG_CON, "cam_m_asg_con",
			"cam_ck"/* parent */, 24),
	GATE_CAM_M(CLK_CAM_MAIN_PDA0_CON, "cam_m_pda0_con",
			"cam_ck"/* parent */, 25),
	GATE_CAM_M(CLK_CAM_MAIN_PDA1_CON, "cam_m_pda1_con",
			"cam_ck"/* parent */, 26),
	GATE_CAM_M(CLK_CAM_MAIN_PDA2_CON, "cam_m_pda2_con",
			"cam_ck"/* parent */, 27),
	GATE_CAM_M(CLK_CAM_MAIN_FAKE_ENG_CON, "cam_m_fake_eng_con",
			"cam_ck"/* parent */, 28),
	GATE_CAM_M(CLK_CAM_MAIN_CAM2MM0_GALS_CON, "cam_m_cam2mm0_gcon",
			"cam_ck"/* parent */, 29),
	GATE_CAM_M(CLK_CAM_MAIN_CAM2MM1_GALS_CON, "cam_m_cam2mm1_gcon",
			"cam_ck"/* parent */, 30),
	GATE_CAM_M(CLK_CAM_MAIN_CAM2SYS_GALS_CON, "cam_m_cam2sys_gcon",
			"cam_ck"/* parent */, 31),
	GATE_CAM_M_NULL_OP(CLK_CAM_MAIN_CAM_SUBA_CON_DUMMY, "cam_m_cam_suba_con_dummy",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M_NULL_OP(CLK_CAM_MAIN_CAM_SUBB_CON_DUMMY, "cam_m_cam_subb_con_dummy",
			"cam_ck"/* parent */, 4),
	GATE_CAM_M_NULL_OP(CLK_CAM_MAIN_CAM_SUBC_CON_DUMMY, "cam_m_cam_subc_con_dummy",
			"cam_ck"/* parent */, 5),
	GATE_CAM_M_NULL_OP(CLK_CAM_MAIN_CAM_MRAW_CON_DUMMY, "cam_m_cam_mraw_con_dummy",
			"cam_ck"/* parent */, 6),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6895_cam[] = {
	{
		.compatible = "mediatek,mt6895-camsys_mraw",
		.data = &cam_mr_mcd,
	}, {
		.compatible = "mediatek,mt6895-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6895-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6895-camsys_rawc",
		.data = &cam_rc_mcd,
	}, {
		.compatible = "mediatek,mt6895-camsys_yuva",
		.data = &cam_ya_mcd,
	}, {
		.compatible = "mediatek,mt6895-camsys_yuvb",
		.data = &cam_yb_mcd,
	}, {
		.compatible = "mediatek,mt6895-camsys_yuvc",
		.data = &cam_yc_mcd,
	}, {
		.compatible = "mediatek,mt6895-cam_main_r1a",
		.data = &cam_m_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6895_cam_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6895_cam_drv = {
	.probe = clk_mt6895_cam_grp_probe,
	.driver = {
		.name = "clk-mt6895-cam",
		.of_match_table = of_match_clk_mt6895_cam,
	},
};

static int __init clk_mt6895_cam_init(void)
{
	return platform_driver_register(&clk_mt6895_cam_drv);
}

static void __exit clk_mt6895_cam_exit(void)
{
	platform_driver_unregister(&clk_mt6895_cam_drv);
}

arch_initcall(clk_mt6895_cam_init);
module_exit(clk_mt6895_cam_exit);
MODULE_LICENSE("GPL");
