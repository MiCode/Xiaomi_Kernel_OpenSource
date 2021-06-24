// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6879-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs dpma0_cg_regs = {
	.set_ofs = 0x68,
	.clr_ofs = 0x68,
	.sta_ofs = 0x68,
};

static const struct mtk_gate_regs dpma1_cg_regs = {
	.set_ofs = 0x78,
	.clr_ofs = 0x78,
	.sta_ofs = 0x78,
};

#define GATE_DPMA0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dpma0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_DPMA1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &dpma1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate dpma_clks[] = {
	/* DPMA0 */
	GATE_DPMA0(CLK_DPMA_MAS_BUS_DIS, "dpma_mas_bus_dis",
			"dpmaif_main_ck"/* parent */, 0),
	GATE_DPMA0(CLK_DPMA_SLV_BUS_DIS, "dpma_slv_bus_dis",
			"dpmaif_main_ck"/* parent */, 1),
	GATE_DPMA0(CLK_DPMA_SRAM_DIS, "dpma_sram_dis",
			"dpmaif_main_ck"/* parent */, 2),
	GATE_DPMA0(CLK_DPMA_DL_DIS, "dpma_dl_dis",
			"dpmaif_main_ck"/* parent */, 3),
	GATE_DPMA0(CLK_DPMA_UL_DIS, "dpma_ul_dis",
			"dpmaif_main_ck"/* parent */, 4),
	GATE_DPMA0(CLK_DPMA_APB_DIS, "dpma_apb_dis",
			"dpmaif_main_ck"/* parent */, 5),
	/* DPMA1 */
	GATE_DPMA1(CLK_DPMA_MAS_BUS, "dpma_mas_bus",
			"dpmaif_main_ck"/* parent */, 0),
	GATE_DPMA1(CLK_DPMA_SLV_BUS, "dpma_slv_bus",
			"dpmaif_main_ck"/* parent */, 1),
	GATE_DPMA1(CLK_DPMA_SRAM, "dpma_sram",
			"dpmaif_main_ck"/* parent */, 2),
	GATE_DPMA1(CLK_DPMA_DL, "dpma_dl",
			"dpmaif_main_ck"/* parent */, 3),
	GATE_DPMA1(CLK_DPMA_UL, "dpma_ul",
			"dpmaif_main_ck"/* parent */, 4),
};

static const struct mtk_clk_desc dpma_mcd = {
	.clks = dpma_clks,
	.num_clks = ARRAY_SIZE(dpma_clks),
};

static int clk_mt6879_dpma_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6879_dpma[] = {
	{
		.compatible = "mediatek,mt6879-dpmaif",
		.data = &dpma_mcd,
	},
	{}
};

static struct platform_driver clk_mt6879_dpma_drv = {
	.probe = clk_mt6879_dpma_probe,
	.driver = {
		.name = "clk-mt6879-dpma",
		.of_match_table = of_match_clk_mt6879_dpma,
	},
};

static int __init clk_mt6879_dpma_init(void)
{
	return platform_driver_register(&clk_mt6879_dpma_drv);
}

static void __exit clk_mt6879_dpma_exit(void)
{
	platform_driver_unregister(&clk_mt6879_dpma_drv);
}

arch_initcall(clk_mt6879_dpma_init);
module_exit(clk_mt6879_dpma_exit);
MODULE_LICENSE("GPL");
