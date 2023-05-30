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

static const struct mtk_gate_regs nemi_reg_cg_regs = {
	.set_ofs = 0x858,
	.clr_ofs = 0x858,
	.sta_ofs = 0x858,
};

#define GATE_NEMI_REG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &nemi_reg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate nemi_reg_clks[] = {
	GATE_NEMI_REG(CLK_NEMI_REG_BUS_MON_MODE, "nemi_bus_mon_mode",
			"f26m_ck"/* parent */, 11),
};

static const struct mtk_clk_desc nemi_reg_mcd = {
	.clks = nemi_reg_clks,
	.num_clks = CLK_NEMI_REG_NR_CLK,
};

static int clk_mt6835_nemi_reg_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6835_nemi_reg[] = {
	{
		.compatible = "mediatek,mt6835-nemi_reg",
		.data = &nemi_reg_mcd,
	},
	{}
};

static struct platform_driver clk_mt6835_nemi_reg_drv = {
	.probe = clk_mt6835_nemi_reg_probe,
	.driver = {
		.name = "clk-mt6835-nemi_reg",
		.of_match_table = of_match_clk_mt6835_nemi_reg,
	},
};

module_platform_driver(clk_mt6835_nemi_reg_drv);
MODULE_LICENSE("GPL");
