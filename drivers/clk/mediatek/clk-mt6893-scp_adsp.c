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

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs scp_adsp_cg_regs = {
	.set_ofs = 0x180,
	.clr_ofs = 0x180,
	.sta_ofs = 0x180,
};

#define GATE_SCP_ADSP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_adsp_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate scp_adsp_clks[] = {
	GATE_SCP_ADSP(CLK_SCP_ADSP_RG_AUDIODSP, "scp_adsp_audiodsp",
			"adsp_ck"/* parent */, 0),
};

static const struct mtk_clk_desc scp_adsp_mcd = {
	.clks = scp_adsp_clks,
	.num_clks = CLK_SCP_ADSP_NR_CLK,
};

static int clk_mt6893_scp_adsp_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6893_scp_adsp[] = {
	{
		.compatible = "mediatek,mt6893-scp_adsp",
		.data = &scp_adsp_mcd,
	},
	{}
};

static struct platform_driver clk_mt6893_scp_adsp_drv = {
	.probe = clk_mt6893_scp_adsp_probe,
	.driver = {
		.name = "clk-mt6893-scp_adsp",
		.of_match_table = of_match_clk_mt6893_scp_adsp,
	},
};

static int __init clk_mt6893_scp_adsp_init(void)
{
	return platform_driver_register(&clk_mt6893_scp_adsp_drv);
}

static void __exit clk_mt6893_scp_adsp_exit(void)
{
	platform_driver_unregister(&clk_mt6893_scp_adsp_drv);
}

postcore_initcall(clk_mt6893_scp_adsp_init);
module_exit(clk_mt6893_scp_adsp_exit);
MODULE_LICENSE("GPL");
