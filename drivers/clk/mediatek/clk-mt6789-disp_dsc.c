// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6789-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs disp_dsc_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

#define GATE_DISP_DSC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &disp_dsc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate disp_dsc_clks[] = {
	GATE_DISP_DSC(CLK_DISP_DSC_DSC_EN, "disp_dsc_dsc_en",
			"disp_ck"/* parent */, 0),
};

static const struct mtk_clk_desc disp_dsc_mcd = {
	.clks = disp_dsc_clks,
	.num_clks = CLK_DISP_DSC_NR_CLK,
};

static int clk_mt6789_disp_dsc_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6789_disp_dsc[] = {
	{
		.compatible = "mediatek,mt6789-disp_dsc",
		.data = &disp_dsc_mcd,
	},
	{}
};

static struct platform_driver clk_mt6789_disp_dsc_drv = {
	.probe = clk_mt6789_disp_dsc_probe,
	.driver = {
		.name = "clk-mt6789-disp_dsc",
		.of_match_table = of_match_clk_mt6789_disp_dsc,
	},
};

module_platform_driver(clk_mt6789_disp_dsc_drv);
MODULE_LICENSE("GPL");
