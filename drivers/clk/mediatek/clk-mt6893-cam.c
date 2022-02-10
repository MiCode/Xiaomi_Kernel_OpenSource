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

/* get spm power status struct to register inside clk_data */
static struct pwr_status cam_m_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(23), BIT(23));

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
		.pwr_stat = &cam_m_pwr_stat,			\
	}

#define GATE_DUMMY(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &cam_m_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
		.pwr_stat = &cam_m_pwr_stat,			\
	}

static const struct mtk_gate cam_m_clks[] = {
	GATE_DUMMY(CLK_CAM_M_LARB13, "cam_m_larb13",
			"cam_ck"/* parent */, 0),
	GATE_CAM_M(CLK_CAM_M_DFP_VAD, "cam_m_dfp_vad",
			"cam_ck"/* parent */, 1),
	GATE_DUMMY(CLK_CAM_M_LARB14, "cam_m_larb14",
			"cam_ck"/* parent */, 2),
	GATE_DUMMY(CLK_CAM_M_LARB15, "cam_m_larb15",
			"cam_ck"/* parent */, 3),
	GATE_CAM_M(CLK_CAM_M_CAM, "cam_m_cam",
			"cam_ck"/* parent */, 6),
	GATE_CAM_M(CLK_CAM_M_CAMTG, "cam_m_camtg",
			"cam_ck"/* parent */, 7),
	GATE_CAM_M(CLK_CAM_M_SENINF, "cam_m_seninf",
			"cam_ck"/* parent */, 8),
	GATE_CAM_M(CLK_CAM_M_CAMSV0, "cam_m_camsv0",
			"cam_ck"/* parent */, 9),
	GATE_CAM_M(CLK_CAM_M_CAMSV1, "cam_m_camsv1",
			"cam_ck"/* parent */, 10),
	GATE_CAM_M(CLK_CAM_M_CAMSV2, "cam_m_camsv2",
			"cam_ck"/* parent */, 11),
	GATE_CAM_M(CLK_CAM_M_CAMSV3, "cam_m_camsv3",
			"cam_ck"/* parent */, 12),
	GATE_CAM_M(CLK_CAM_M_CCU0, "cam_m_ccu0",
			"cam_ck"/* parent */, 13),
	GATE_CAM_M(CLK_CAM_M_CCU1, "cam_m_ccu1",
			"cam_ck"/* parent */, 14),
	GATE_CAM_M(CLK_CAM_M_MRAW0, "cam_m_mraw0",
			"cam_ck"/* parent */, 15),
	GATE_CAM_M(CLK_CAM_M_MRAW1, "cam_m_mraw1",
			"cam_ck"/* parent */, 16),
	GATE_CAM_M(CLK_CAM_M_FAKE_ENG, "cam_m_fake_eng",
			"cam_ck"/* parent */, 17),
};

static const struct mtk_clk_desc cam_m_mcd = {
	.clks = cam_m_clks,
	.num_clks = CLK_CAM_M_NR_CLK,
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status cam_ra_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(24), BIT(24));

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
		.pwr_stat = &cam_ra_pwr_stat,			\
	}

#define GATE_DUMMY_A(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &cam_ra_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
		.pwr_stat = &cam_ra_pwr_stat,			\
	}

static const struct mtk_gate cam_ra_clks[] = {
	GATE_DUMMY_A(CLK_CAM_RA_LARBX, "cam_ra_larbx",
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

/* get spm power status struct to register inside clk_data */
static struct pwr_status cam_rb_pwr_stat = GATE_PWR_STAT(0x16C,
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
		.pwr_stat = &cam_rb_pwr_stat,			\
	}

#define GATE_DUMMY_B(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &cam_rb_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
		.pwr_stat = &cam_rb_pwr_stat,			\
	}

static const struct mtk_gate cam_rb_clks[] = {
	GATE_DUMMY_B(CLK_CAM_RB_LARBX, "cam_rb_larbx",
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

/* get spm power status struct to register inside clk_data */
static struct pwr_status cam_rc_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(26), BIT(26));

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
		.pwr_stat = &cam_rc_pwr_stat,			\
	}

#define GATE_DUMMY_C(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &cam_rc_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
		.pwr_stat = &cam_rc_pwr_stat,			\
	}

static const struct mtk_gate cam_rc_clks[] = {
	GATE_DUMMY_C(CLK_CAM_RC_LARBX, "cam_rc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAM_RC(CLK_CAM_RC_CAM, "cam_rc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAM_RC(CLK_CAM_RC_CAMTG, "cam_rc_camtg",
			"cam_ck"/* parent */, 2),
};

static const struct mtk_clk_desc cam_rc_mcd = {
	.clks = cam_rc_clks,
	.num_clks = CLK_CAM_RC_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6893_cam[] = {
	{
		.compatible = "mediatek,mt6893-camsys_main",
		.data = &cam_m_mcd,
	}, {
		.compatible = "mediatek,mt6893-camsys_rawa",
		.data = &cam_ra_mcd,
	}, {
		.compatible = "mediatek,mt6893-camsys_rawb",
		.data = &cam_rb_mcd,
	}, {
		.compatible = "mediatek,mt6893-camsys_rawc",
		.data = &cam_rc_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6893_cam_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6893_cam_drv = {
	.probe = clk_mt6893_cam_grp_probe,
	.driver = {
		.name = "clk-mt6893-cam",
		.of_match_table = of_match_clk_mt6893_cam,
	},
};

static int __init clk_mt6893_cam_init(void)
{
	return platform_driver_register(&clk_mt6893_cam_drv);
}

static void __exit clk_mt6893_cam_exit(void)
{
	platform_driver_unregister(&clk_mt6893_cam_drv);
}

postcore_initcall(clk_mt6893_cam_init);
module_exit(clk_mt6893_cam_exit);
MODULE_LICENSE("GPL");
