/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6833-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP			1

#define INV_OFS			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, BIT(15), BIT(15));

static const struct mtk_gate_regs vdec0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vdec1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDEC0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &pwr_stat,			\
	}

#define GATE_VDEC1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vdec1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate vdec_clks[] = {
	/* VDEC0 */
	GATE_VDEC0(CLK_VDEC_CKEN, "vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDEC0(CLK_VDEC_ACTIVE, "vdec_active",
			"vdec_ck"/* parent */, 4),
	/* VDEC1 */
	GATE_VDEC1(CLK_VDEC_LARB1_CKEN, "vdec_larb1_cken",
			"vdec_ck"/* parent */, 0),
};

static int clk_mt6833_vdec_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_VDEC_NR_CLK);

	mtk_clk_register_gates(node, vdec_clks, ARRAY_SIZE(vdec_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6833_vdec[] = {
	{ .compatible = "mediatek,mt6833-vdec_gcon", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6833_vdec_drv = {
	.probe = clk_mt6833_vdec_probe,
	.driver = {
		.name = "clk-mt6833-vdec",
		.of_match_table = of_match_clk_mt6833_vdec,
	},
};

builtin_platform_driver(clk_mt6833_vdec_drv);

#else

static struct platform_driver clk_mt6833_vdec_drv = {
	.probe = clk_mt6833_vdec_probe,
	.driver = {
		.name = "clk-mt6833-vdec",
		.of_match_table = of_match_clk_mt6833_vdec,
	},
};
static int __init clk_mt6833_vdec_platform_init(void)
{
	return platform_driver_register(&clk_mt6833_vdec_drv);
}
arch_initcall(clk_mt6833_vdec_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
