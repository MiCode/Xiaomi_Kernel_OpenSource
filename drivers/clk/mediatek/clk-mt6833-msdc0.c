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

static const struct mtk_gate_regs msdc0_cg_regs = {
	.set_ofs = 0xb4,
	.clr_ofs = 0xb4,
	.sta_ofs = 0xb4,
};

#define GATE_MSDC0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &msdc0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate msdc0_clks[] = {
	GATE_MSDC0(CLK_MSDC0_AXI_WRAP_CKEN, "msdc0_axi_wrap_cken",
			"axi_ck"/* parent */, 22),
};

static int clk_mt6833_msdc0_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_MSDC0_NR_CLK);

	mtk_clk_register_gates(node, msdc0_clks, ARRAY_SIZE(msdc0_clks),
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

static const struct of_device_id of_match_clk_mt6833_msdc0[] = {
	{ .compatible = "mediatek,mt6833-msdc0sys", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6833_msdc0_drv = {
	.probe = clk_mt6833_msdc0_probe,
	.driver = {
		.name = "clk-mt6833-msdc0",
		.of_match_table = of_match_clk_mt6833_msdc0,
	},
};

builtin_platform_driver(clk_mt6833_msdc0_drv);

#else

static struct platform_driver clk_mt6833_msdc0_drv = {
	.probe = clk_mt6833_msdc0_probe,
	.driver = {
		.name = "clk-mt6833-msdc0",
		.of_match_table = of_match_clk_mt6833_msdc0,
	},
};
static int __init clk_mt6833_msdc0_platform_init(void)
{
	return platform_driver_register(&clk_mt6833_msdc0_drv);
}
arch_initcall_sync(clk_mt6833_msdc0_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
