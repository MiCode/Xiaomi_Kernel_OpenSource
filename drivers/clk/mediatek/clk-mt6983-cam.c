// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6983-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

static void __iomem *cam_rawa_base;

static const struct mtk_gate_regs cam_main_r1a_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_MAIN_R1A_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_main_r1a_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_CAM_MAIN_R1A_0_DUMMYS(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_main_r1a_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

#define GATE_CAM_MAIN_R1A_0_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_main_r1a_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}

static struct mtk_gate cam_main_r1a_clks[] = {
	GATE_CAM_MAIN_R1A_0_DUMMY(CLK_CAM_MAIN_R1A_CAM_MAIN_LARB13_CG_CON /* CLK ID */,
		"c_larb13_con" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_MAIN_R1A_0_DUMMY(CLK_CAM_MAIN_R1A_CAM_MAIN_LARB14_CG_CON /* CLK ID */,
		"c_larb14_con" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAM_MAIN_R1A_0_DUMMY(CLK_CAM_MAIN_R1A_CAM_MAIN_LARB13R_CG_CON /* CLK ID */,
		"c_larb13r_con" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_MAIN_R1A_0_DUMMY(CLK_CAM_MAIN_R1A_CAM_MAIN_LARB14R_CG_CON /* CLK ID */,
		"c_larb14r_con" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_CG_CON /* CLK ID */,
		"c_cam_con" /* name */,
		"cam_ck" /* parent */, 2 /* bit */),
	GATE_CAM_MAIN_R1A_0_DUMMYS(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_SUBA_CG_CON /* CLK ID */,
		"c_cam_suba_con" /* name */,
		"cam_ck" /* parent */, 3 /* bit */),
	GATE_CAM_MAIN_R1A_0_DUMMYS(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_SUBB_CG_CON /* CLK ID */,
		"c_cam_subb_con" /* name */,
		"cam_ck" /* parent */, 4 /* bit */),
	GATE_CAM_MAIN_R1A_0_DUMMYS(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_SUBC_CG_CON /* CLK ID */,
		"c_cam_subc_con" /* name */,
		"cam_ck" /* parent */, 5 /* bit */),
	GATE_CAM_MAIN_R1A_0_DUMMYS(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_MRAW_CG_CON /* CLK ID */,
		"c_cam_mraw_con" /* name */,
		"cam_ck" /* parent */, 6 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_SUBA /* CLK ID */,
		"c_cam_suba" /* name */,
		"cam_ck" /* parent */, 3 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_SUBB /* CLK ID */,
		"c_cam_subb" /* name */,
		"cam_ck" /* parent */, 4 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_SUBC /* CLK ID */,
		"c_cam_subc" /* name */,
		"cam_ck" /* parent */, 5 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM_MRAW /* CLK ID */,
		"c_cam_mraw" /* name */,
		"cam_ck" /* parent */, 6 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAMTG_CG_CON /* CLK ID */,
		"c_camtg_con" /* name */,
		"fcamtm_ck" /* parent */, 7 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_SENINF_CG_CON /* CLK ID */,
		"c_seninf_con" /* name */,
		"cam_ck" /* parent */, 8 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVA_CG_CON /* CLK ID */,
		"c_gcamsva_con" /* name */,
		"cam_ck" /* parent */, 9 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVB_CG_CON /* CLK ID */,
		"c_gcamsvb_con" /* name */,
		"cam_ck" /* parent */, 10 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVC_CG_CON /* CLK ID */,
		"c_gcamsvc_con" /* name */,
		"cam_ck" /* parent */, 11 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVD_CG_CON /* CLK ID */,
		"c_gcamsvd_con" /* name */,
		"cam_ck" /* parent */, 12 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVE_CG_CON /* CLK ID */,
		"c_gcamsve_con" /* name */,
		"cam_ck" /* parent */, 13 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVF_CG_CON /* CLK ID */,
		"c_gcamsvf_con" /* name */,
		"cam_ck" /* parent */, 14 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVG_CG_CON /* CLK ID */,
		"c_gcamsvg_con" /* name */,
		"cam_ck" /* parent */, 15 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVH_CG_CON /* CLK ID */,
		"c_gcamsvh_con" /* name */,
		"cam_ck" /* parent */, 16 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVI_CG_CON /* CLK ID */,
		"c_gcamsvi_con" /* name */,
		"cam_ck" /* parent */, 17 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_GCAMSVJ_CG_CON /* CLK ID */,
		"c_gcamsvj_con" /* name */,
		"cam_ck" /* parent */, 18 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAMSV_TOP_CG_CON /* CLK ID */,
		"c_camsv_con" /* name */,
		"cam_ck" /* parent */, 19 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAMSV_CQ_A_CG_CON /* CLK ID */,
		"c_camsv_cq_a_con" /* name */,
		"cam_ck" /* parent */, 20 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAMSV_CQ_B_CG_CON /* CLK ID */,
		"c_camsv_cq_b_con" /* name */,
		"cam_ck" /* parent */, 21 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAMSV_CQ_C_CG_CON /* CLK ID */,
		"c_camsv_cq_c_con" /* name */,
		"cam_ck" /* parent */, 22 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_ADL_CG_CON /* CLK ID */,
		"c_adl_con" /* name */,
		"cam_ck" /* parent */, 23 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_ASG_CG_CON /* CLK ID */,
		"c_asg_con" /* name */,
		"cam_ck" /* parent */, 24 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_PDA0_CG_CON /* CLK ID */,
		"c_pda0_con" /* name */,
		"cam_ck" /* parent */, 25 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_PDA1_CG_CON /* CLK ID */,
		"c_pda1_con" /* name */,
		"cam_ck" /* parent */, 26 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_PDA2_CG_CON /* CLK ID */,
		"c_pda2_con" /* name */,
		"cam_ck" /* parent */, 27 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_FAKE_ENG_CG_CON /* CLK ID */,
		"c_fake_eng_con" /* name */,
		"cam_ck" /* parent */, 28 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM2MM0_GALS_CG_CON /* CLK ID */,
		"c_cam2mm0_gals_con" /* name */,
		"cam_ck" /* parent */, 29 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM2MM1_GALS_CG_CON /* CLK ID */,
		"c_cam2mm1_gals_con" /* name */,
		"cam_ck" /* parent */, 30 /* bit */),
	GATE_CAM_MAIN_R1A_0(CLK_CAM_MAIN_R1A_CAM_MAIN_CAM2SYS_GALS_CG_CON /* CLK ID */,
		"c_cam2sys_gals_con" /* name */,
		"cam_ck" /* parent */, 31 /* bit */),
};

static int clk_mt6983_cam_main_r1a_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_CAM_MAIN_R1A_NR_CLK);

	mtk_clk_register_gates(node, cam_main_r1a_clks,
		ARRAY_SIZE(cam_main_r1a_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_gate_regs cam_ra_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RA_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_ra_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
}

#define GATE_CAM_RA_0_DUMMYS(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_ra_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
}

static struct mtk_gate cam_ra_clks[] = {
	GATE_CAM_RA_0_DUMMYS(CLK_CAM_RA_LARBX /* CLK ID */,
		"cam_ra_larbx" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_RA_0(CLK_CAM_RA_LARBXT /* CLK ID */,
		"cam_ra_larbxt" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_RA_0(CLK_CAM_RA_CAM /* CLK ID */,
		"cam_ra_cam" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAM_RA_0(CLK_CAM_RA_CAMTG /* CLK ID */,
		"cam_ra_camtg" /* name */,
		"cam_ck" /* parent */, 2 /* bit */),
};

static int clk_mt6983_cam_ra_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_notice("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_CAM_RA_NR_CLK);

	mtk_clk_register_gates(node, cam_ra_clks,
		ARRAY_SIZE(cam_ra_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

	cam_rawa_base = base;
#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

void dump_cam_rawa(void)
{
	pr_notice("%s: %08x\r\n", __func__, readl(cam_rawa_base));
}
EXPORT_SYMBOL(dump_cam_rawa);

static const struct mtk_gate_regs cam_rb_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RB_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_rb_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_CAM_RB_0_DUMMYS(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_rb_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static struct mtk_gate cam_rb_clks[] = {
	GATE_CAM_RB_0_DUMMYS(CLK_CAM_RB_LARBX /* CLK ID */,
		"cam_rb_larbx" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_RB_0(CLK_CAM_RB_LARBXT /* CLK ID */,
		"cam_rb_larbxt" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_RB_0(CLK_CAM_RB_CAM /* CLK ID */,
		"cam_rb_cam" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAM_RB_0(CLK_CAM_RB_CAMTG /* CLK ID */,
		"cam_rb_camtg" /* name */,
		"cam_ck" /* parent */, 2 /* bit */),
};

static int clk_mt6983_cam_rb_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_CAM_RB_NR_CLK);

	mtk_clk_register_gates(node, cam_rb_clks,
		ARRAY_SIZE(cam_rb_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_gate_regs cam_rc_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAM_RC_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_rc_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_CAM_RC_0_DUMMYS(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &cam_rc_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static struct mtk_gate cam_rc_clks[] = {
	GATE_CAM_RC_0_DUMMYS(CLK_CAM_RC_LARBX /* CLK ID */,
		"cam_rc_larbx" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_RC_0(CLK_CAM_RC_LARBXT /* CLK ID */,
		"cam_rc_larbxt" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAM_RC_0(CLK_CAM_RC_CAM /* CLK ID */,
		"cam_rc_cam" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAM_RC_0(CLK_CAM_RC_CAMTG /* CLK ID */,
		"cam_rc_camtg" /* name */,
		"cam_ck" /* parent */, 2 /* bit */),
};

static int clk_mt6983_cam_rc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_CAM_RC_NR_CLK);

	mtk_clk_register_gates(node, cam_rc_clks,
		ARRAY_SIZE(cam_rc_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_gate_regs camsys_mraw_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_MRAW_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_mraw_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_CAMSYS_MRAW_0_DUMMYS(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_mraw_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static struct mtk_gate camsys_mraw_clks[] = {
	GATE_CAMSYS_MRAW_0_DUMMYS(CLK_CAMSYS_MRAW_LARBX /* CLK ID */,
		"cam_mr_larbx" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_LARBXT /* CLK ID */,
		"cam_mr_larbxt" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_CAMTG /* CLK ID */,
		"cam_mr_camtg" /* name */,
		"fcamtm_ck" /* parent */, 2 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_MRAW0 /* CLK ID */,
		"cam_mr_mraw0" /* name */,
		"cam_ck" /* parent */, 3 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_MRAW1 /* CLK ID */,
		"cam_mr_mraw1" /* name */,
		"cam_ck" /* parent */, 4 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_MRAW2 /* CLK ID */,
		"cam_mr_mraw2" /* name */,
		"cam_ck" /* parent */, 5 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_MRAW3 /* CLK ID */,
		"cam_mr_mraw3" /* name */,
		"cam_ck" /* parent */, 6 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_PDA0 /* CLK ID */,
		"cam_mr_pda0" /* name */,
		"cam_ck" /* parent */, 7 /* bit */),
	GATE_CAMSYS_MRAW_0(CLK_CAMSYS_MRAW_PDA1 /* CLK ID */,
		"cam_mr_pda1" /* name */,
		"cam_ck" /* parent */, 8 /* bit */),
};

static int clk_mt6983_camsys_mraw_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_CAMSYS_MRAW_NR_CLK);

	mtk_clk_register_gates(node, camsys_mraw_clks,
		ARRAY_SIZE(camsys_mraw_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_gate_regs camsys_yuva_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_YUVA_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_yuva_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_CAMSYS_YUVA_0_DUMMYS(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_yuva_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static struct mtk_gate camsys_yuva_clks[] = {
	GATE_CAMSYS_YUVA_0_DUMMYS(CLK_CAMSYS_YUVA_LARBX /* CLK ID */,
		"cam_ya_larbx" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_YUVA_0(CLK_CAMSYS_YUVA_LARBXT /* CLK ID */,
		"cam_ya_larbxt" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_YUVA_0(CLK_CAMSYS_YUVA_CAM /* CLK ID */,
		"cam_ya_cam" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAMSYS_YUVA_0(CLK_CAMSYS_YUVA_CAMTG /* CLK ID */,
		"cam_ya_camtg" /* name */,
		"cam_ck" /* parent */, 2 /* bit */),
};

static int clk_mt6983_camsys_yuva_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_CAMSYS_YUVA_NR_CLK);

	mtk_clk_register_gates(node, camsys_yuva_clks,
		ARRAY_SIZE(camsys_yuva_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_gate_regs camsys_yuvb_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_YUVB_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_yuvb_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_CAMSYS_YUVB_0_DUMMYS(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_yuvb_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static struct mtk_gate camsys_yuvb_clks[] = {
	GATE_CAMSYS_YUVB_0_DUMMYS(CLK_CAMSYS_YUVB_LARBX /* CLK ID */,
		"cam_yb_larbx" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_YUVB_0(CLK_CAMSYS_YUVB_LARBXT /* CLK ID */,
		"cam_yb_larbxt" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_YUVB_0(CLK_CAMSYS_YUVB_CAM /* CLK ID */,
		"cam_yb_cam" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAMSYS_YUVB_0(CLK_CAMSYS_YUVB_CAMTG /* CLK ID */,
		"cam_yb_camtg" /* name */,
		"cam_ck" /* parent */, 2 /* bit */),
};

static int clk_mt6983_camsys_yuvb_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_CAMSYS_YUVB_NR_CLK);

	mtk_clk_register_gates(node, camsys_yuvb_clks,
		ARRAY_SIZE(camsys_yuvb_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct mtk_gate_regs camsys_yuvc_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_YUVC_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_yuvc_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_CAMSYS_YUVC_0_DUMMYS(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &camsys_yuvc_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static struct mtk_gate camsys_yuvc_clks[] = {
	GATE_CAMSYS_YUVC_0_DUMMYS(CLK_CAMSYS_YUVC_LARBX /* CLK ID */,
		"cam_yc_larbx" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_YUVC_0(CLK_CAMSYS_YUVC_LARBXT /* CLK ID */,
		"cam_yc_larbxt" /* name */,
		"cam_ck" /* parent */, 0 /* bit */),
	GATE_CAMSYS_YUVC_0(CLK_CAMSYS_YUVC_CAM /* CLK ID */,
		"cam_yc_cam" /* name */,
		"cam_ck" /* parent */, 1 /* bit */),
	GATE_CAMSYS_YUVC_0(CLK_CAMSYS_YUVC_CAMTG /* CLK ID */,
		"cam_yc_camtg" /* name */,
		"cam_ck" /* parent */, 2 /* bit */),
};

static int clk_mt6983_camsys_yuvc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_CAMSYS_YUVC_NR_CLK);

	mtk_clk_register_gates(node, camsys_yuvc_clks,
		ARRAY_SIZE(camsys_yuvc_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct of_device_id of_match_clk_mt6983_cam[] = {
	{
		.compatible = "mediatek,mt6983-cam_main_r1a",
		.data = clk_mt6983_cam_main_r1a_probe,
	}, {
		.compatible = "mediatek,mt6983-camsys_rawa",
		.data = clk_mt6983_cam_ra_probe,
	}, {
		.compatible = "mediatek,mt6983-camsys_rawb",
		.data = clk_mt6983_cam_rb_probe,
	}, {
		.compatible = "mediatek,mt6983-camsys_rawc",
		.data = clk_mt6983_cam_rc_probe,
	}, {
		.compatible = "mediatek,mt6983-camsys_mraw",
		.data = clk_mt6983_camsys_mraw_probe,
	}, {
		.compatible = "mediatek,mt6983-camsys_yuva",
		.data = clk_mt6983_camsys_yuva_probe,
	}, {
		.compatible = "mediatek,mt6983-camsys_yuvb",
		.data = clk_mt6983_camsys_yuvb_probe,
	}, {
		.compatible = "mediatek,mt6983-camsys_yuvc",
		.data = clk_mt6983_camsys_yuvc_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6983_cam_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6983_cam_drv = {
	.probe = clk_mt6983_cam_probe,
	.driver = {
		.name = "clk-mt6983-cam",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_cam,
	},
};

static int __init clk_mt6983_cam_init(void)
{
	return platform_driver_register(&clk_mt6983_cam_drv);
}

static void __exit clk_mt6983_cam_exit(void)
{
	platform_driver_unregister(&clk_mt6983_cam_drv);
}

arch_initcall(clk_mt6983_cam_init);
module_exit(clk_mt6983_cam_exit);
MODULE_LICENSE("GPL");

