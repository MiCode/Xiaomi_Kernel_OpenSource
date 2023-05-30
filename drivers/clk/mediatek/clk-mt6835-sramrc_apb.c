// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6835-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs sramrc_apb_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

#define GATE_SRAMRC_APB(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &sramrc_apb_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate sramrc_apb_clks[] = {
	GATE_SRAMRC_APB(CLK_SRAMRC_APB_SRAMRC_EN, "sramrc_apb_sramrc_en",
			"f26m_ck"/* parent */, 0),
};

static const struct mtk_clk_desc sramrc_apb_mcd = {
	.clks = sramrc_apb_clks,
	.num_clks = CLK_SRAMRC_APB_NR_CLK,
};

static int clk_mt6835_sramrc_apb_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6835_sramrc_apb[] = {
	{
		.compatible = "mediatek,mt6835-sramrc_apb",
		.data = &sramrc_apb_mcd,
	},
	{}
};

static struct platform_driver clk_mt6835_sramrc_apb_drv = {
	.probe = clk_mt6835_sramrc_apb_probe,
	.driver = {
		.name = "clk-mt6835-sramrc_apb",
		.of_match_table = of_match_clk_mt6835_sramrc_apb,
	},
};

module_platform_driver(clk_mt6835_sramrc_apb_drv);
MODULE_LICENSE("GPL");
