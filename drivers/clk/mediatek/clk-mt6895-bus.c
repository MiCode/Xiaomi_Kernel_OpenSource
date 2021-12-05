// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6895-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ifrao0_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifrao1_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8C,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifrao2_cg_regs = {
	.set_ofs = 0xA4,
	.clr_ofs = 0xA8,
	.sta_ofs = 0xAC,
};

static const struct mtk_gate_regs ifrao3_cg_regs = {
	.set_ofs = 0xC0,
	.clr_ofs = 0xC4,
	.sta_ofs = 0xC8,
};

static const struct mtk_gate_regs ifrao4_cg_regs = {
	.set_ofs = 0xE0,
	.clr_ofs = 0xE4,
	.sta_ofs = 0xE8,
};

#define GATE_IFRAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ifrao_clks[] = {
	/* IFRAO0 */
	GATE_IFRAO0(CLK_IFRAO_THERM, "ifrao_therm",
			"axi_ck"/* parent */, 10),
	/* IFRAO1 */
	GATE_IFRAO1(CLK_IFRAO_CCIF1_AP, "ifrao_ccif1_ap",
			"axi_ck"/* parent */, 12),
	GATE_IFRAO1(CLK_IFRAO_CCIF1_MD, "ifrao_ccif1_md",
			"axi_ck"/* parent */, 13),
	GATE_IFRAO1(CLK_IFRAO_CCIF_AP, "ifrao_ccif_ap",
			"axi_ck"/* parent */, 23),
	GATE_IFRAO1(CLK_IFRAO_CCIF_MD, "ifrao_ccif_md",
			"axi_ck"/* parent */, 26),
	/* IFRAO2 */
	GATE_IFRAO2(CLK_IFRAO_CLDMA_BCLK, "ifrao_cldmabclk",
			"axi_ck"/* parent */, 3),
	GATE_IFRAO2(CLK_IFRAO_CQ_DMA, "ifrao_cq_dma",
			"axi_ck"/* parent */, 27),
	/* IFRAO3 */
	GATE_IFRAO3(CLK_IFRAO_CCIF5_MD, "ifrao_ccif5_md",
			"axi_ck"/* parent */, 10),
	GATE_IFRAO3(CLK_IFRAO_CCIF2_AP, "ifrao_ccif2_ap",
			"axi_ck"/* parent */, 16),
	GATE_IFRAO3(CLK_IFRAO_CCIF2_MD, "ifrao_ccif2_md",
			"axi_ck"/* parent */, 17),
	GATE_IFRAO3(CLK_IFRAO_FBIST2FPC, "ifrao_fbist2fpc",
			"msdc30_1_ck"/* parent */, 24),
	GATE_IFRAO3(CLK_IFRAO_DPMAIF_MAIN, "ifrao_dpmaif_main",
			"dpmaif_main_ck"/* parent */, 26),
	GATE_IFRAO3(CLK_IFRAO_CCIF4_MD, "ifrao_ccif4_md",
			"axi_ck"/* parent */, 29),
	/* IFRAO4 */
	GATE_IFRAO4(CLK_IFRAO_RG_MMW_DPMAIF26M_CK, "ifrao_dpmaif_26m",
			"f26m_ck"/* parent */, 17),
};

static const struct mtk_clk_desc ifrao_mcd = {
	.clks = ifrao_clks,
	.num_clks = CLK_IFRAO_NR_CLK,
};





static const struct of_device_id of_match_clk_mt6895_bus[] = {
	{
		.compatible = "mediatek,mt6895-infracfg_ao",
		.data = &ifrao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6895_bus_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6895_bus_drv = {
	.probe = clk_mt6895_bus_grp_probe,
	.driver = {
		.name = "clk-mt6895-bus",
		.of_match_table = of_match_clk_mt6895_bus,
	},
};

static int __init clk_mt6895_bus_init(void)
{
	return platform_driver_register(&clk_mt6895_bus_drv);
}

static void __exit clk_mt6895_bus_exit(void)
{
	platform_driver_unregister(&clk_mt6895_bus_drv);
}

arch_initcall(clk_mt6895_bus_init);
module_exit(clk_mt6895_bus_exit);
MODULE_LICENSE("GPL");
