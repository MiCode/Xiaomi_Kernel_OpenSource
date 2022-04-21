// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6885-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status ipe_pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(14), BIT(14));

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
		.pwr_stat = &ipe_pwr_stat,			\
	}

#define GATE_DUMMY(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &ipe_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
		.pwr_stat = &ipe_pwr_stat,			\
	}

static const struct mtk_gate ipe_clks[] = {
	GATE_DUMMY(CLK_IPE_LARB19, "ipe_larb19",
			"ipe_ck"/* parent */, 0),
	GATE_DUMMY(CLK_IPE_LARB20, "ipe_larb20",
			"ipe_ck"/* parent */, 1),
	GATE_DUMMY(CLK_IPE_SMI_SUBCOM, "ipe_smi_subcom",
			"ipe_ck"/* parent */, 2),
	GATE_IPE(CLK_IPE_FD, "ipe_fd",
			"ipe_ck"/* parent */, 3),
	GATE_IPE(CLK_IPE_FE, "ipe_fe",
			"ipe_ck"/* parent */, 4),
	GATE_IPE(CLK_IPE_RSC, "ipe_rsc",
			"ipe_ck"/* parent */, 5),
	GATE_IPE(CLK_IPE_DPE, "ipe_dpe",
			"ipe_ck"/* parent */, 6),
};

static const struct mtk_clk_desc ipe_mcd = {
	.clks = ipe_clks,
	.num_clks = CLK_IPE_NR_CLK,
};

static int clk_mt6893_ipe_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_dbg(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6893_ipe[] = {
	{
		.compatible = "mediatek,mt6893-ipesys",
		.data = &ipe_mcd,
	},
	{}
};

static struct platform_driver clk_mt6893_ipe_drv = {
	.probe = clk_mt6893_ipe_probe,
	.driver = {
		.name = "clk-mt6893-ipe",
		.of_match_table = of_match_clk_mt6893_ipe,
	},
};

static int __init clk_mt6893_ipe_init(void)
{
	return platform_driver_register(&clk_mt6893_ipe_drv);
}

static void __exit clk_mt6893_ipe_exit(void)
{
	platform_driver_unregister(&clk_mt6893_ipe_drv);
}

postcore_initcall(clk_mt6893_ipe_init);
module_exit(clk_mt6893_ipe_exit);
MODULE_LICENSE("GPL");
