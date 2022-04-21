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

#include <dt-bindings/clock/mt6885-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status venc_c1_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(18), BIT(18));

static const struct mtk_gate_regs venc_c1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VENC_C1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &venc_c1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &venc_c1_pwr_stat,			\
	}

#define GATE_INV_DUMMY2(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &venc_c1_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &venc_c1_pwr_stat,			\
	}

static const struct mtk_gate ven2_clks[] = {
	GATE_VENC_C1(CLK_VEN2_CKE0_LARB, "ven2_cke0_larb",
			"venc_ck"/* parent */, 0),
	GATE_INV_DUMMY2(CLK_VEN2_CKE1_VENC, "ven2_cke1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VENC_C1(CLK_VEN2_CKE2_JPGENC, "ven2_cke2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VENC_C1(CLK_VEN2_CKE3_JPGDEC, "ven2_cke3_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VENC_C1(CLK_VEN2_CKE4_JPGDEC_C1, "ven2_cke4_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VENC_C1(CLK_VEN2_CKE5_GALS, "ven2_cke5_gals",
			"venc_ck"/* parent */, 28),
};

static const struct mtk_clk_desc ven2_mcd = {
	.clks = ven2_clks,
	.num_clks = CLK_VEN2_NR_CLK,
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status venc_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(17), BIT(17));

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
		.pwr_stat = &venc_pwr_stat,			\
	}

#define GATE_INV_DUMMY1(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &ven1_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,	\
		.pwr_stat = &venc_pwr_stat,			\
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_VEN1(CLK_VEN1_CKE0_LARB, "ven1_cke0_larb",
			"venc_ck"/* parent */, 0),
	GATE_INV_DUMMY1(CLK_VEN1_CKE1_VENC, "ven1_cke1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_cke2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN1(CLK_VEN1_CKE3_JPGDEC, "ven1_cke3_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN1(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_cke4_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_cke5_gals",
			"venc_ck"/* parent */, 28),
};

static const struct mtk_clk_desc ven1_mcd = {
	.clks = ven1_clks,
	.num_clks = CLK_VEN1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6893_ven[] = {
	{
		.compatible = "mediatek,mt6893-vencsys_c1",
		.data = &ven2_mcd,
	}, {
		.compatible = "mediatek,mt6893-vencsys",
		.data = &ven1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6893_ven_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_dbg(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6893_ven_drv = {
	.probe = clk_mt6893_ven_grp_probe,
	.driver = {
		.name = "clk-mt6893-ven",
		.of_match_table = of_match_clk_mt6893_ven,
	},
};

static int __init clk_mt6893_ven_init(void)
{
	return platform_driver_register(&clk_mt6893_ven_drv);
}

static void __exit clk_mt6893_ven_exit(void)
{
	platform_driver_unregister(&clk_mt6893_ven_drv);
}

postcore_initcall(clk_mt6893_ven_init);
module_exit(clk_mt6893_ven_exit);
MODULE_LICENSE("GPL");
