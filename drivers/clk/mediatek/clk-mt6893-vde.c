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
static struct pwr_status vdec_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(16), BIT(16));


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
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDE20(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde20_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &vdec_pwr_stat,			\
	}

#define GATE_VDE21(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde21_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &vdec_pwr_stat,			\
	}

#define GATE_VDE22(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde22_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &vdec_pwr_stat,			\
	}

#define GATE_INV_DUMMY20(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vde20_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &vdec_pwr_stat,			\
	}

#define GATE_INV_DUMMY21(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vde21_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &vdec_pwr_stat,			\
	}

#define GATE_INV_DUMMY22(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vde22_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &vdec_pwr_stat,			\
	}

static const struct mtk_gate vde2_clks[] = {
	/* VDE20 */
	GATE_INV_DUMMY20(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE20(CLK_VDE2_VDEC_ACTIVE, "vde2_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE20(CLK_VDE2_VDEC_CKEN_ENG, "vde2_vdec_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE21 */
	GATE_INV_DUMMY21(CLK_VDE2_LAT_CKEN, "vde2_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE21(CLK_VDE2_LAT_ACTIVE, "vde2_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE21(CLK_VDE2_LAT_CKEN_ENG, "vde2_lat_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE22 */
	GATE_INV_DUMMY22(CLK_VDE2_LARB1_CKEN, "vde2_larb1_cken",
			"vdec_ck"/* parent */, 0),
};

static const struct mtk_clk_desc vde2_mcd = {
	.clks = vde2_clks,
	.num_clks = CLK_VDE2_NR_CLK,
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status vdec_s_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(15), BIT(15));


static const struct mtk_gate_regs vde10_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde11_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde12_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &vdec_s_pwr_stat,			\
	}

#define GATE_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &vdec_s_pwr_stat,			\
	}

#define GATE_VDE12(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde12_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &vdec_s_pwr_stat,			\
	}

#define GATE_INV_DUMMY10(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vde10_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &vdec_s_pwr_stat,			\
	}

#define GATE_INV_DUMMY11(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vde11_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &vdec_s_pwr_stat,			\
	}

#define GATE_INV_DUMMY12(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &vde12_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &vdec_s_pwr_stat,			\
	}

static const struct mtk_gate vde1_clks[] = {
	/* VDE10 */
	GATE_INV_DUMMY10(CLK_VDE1_VDEC_CKEN, "vde1_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE10(CLK_VDE1_VDEC_ACTIVE, "vde1_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE10(CLK_VDE1_VDEC_CKEN_ENG, "vde1_vdec_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE11 */
	GATE_INV_DUMMY11(CLK_VDE1_LAT_CKEN, "vde1_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE11(CLK_VDE1_LAT_ACTIVE, "vde1_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE11(CLK_VDE1_LAT_CKEN_ENG, "vde1_lat_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE12 */
	GATE_INV_DUMMY12(CLK_VDE1_LARB1_CKEN, "vde1_larb1_cken",
			"vdec_ck"/* parent */, 0),
};

static const struct mtk_clk_desc vde1_mcd = {
	.clks = vde1_clks,
	.num_clks = CLK_VDE1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6893_vde[] = {
	{
		.compatible = "mediatek,mt6893-vdecsys",
		.data = &vde2_mcd,
	}, {
		.compatible = "mediatek,mt6893-vdecsys_soc",
		.data = &vde1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6893_vde_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6893_vde_drv = {
	.probe = clk_mt6893_vde_grp_probe,
	.driver = {
		.name = "clk-mt6893-vde",
		.of_match_table = of_match_clk_mt6893_vde,
	},
};

static int __init clk_mt6893_vde_init(void)
{
	return platform_driver_register(&clk_mt6893_vde_drv);
}

static void __exit clk_mt6893_vde_exit(void)
{
	platform_driver_unregister(&clk_mt6893_vde_drv);
}

postcore_initcall(clk_mt6893_vde_init);
module_exit(clk_mt6893_vde_exit);
MODULE_LICENSE("GPL");
