// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6853-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

static const struct mtk_gate_regs apu00_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs apu01_cg_regs = {
	.set_ofs = 0x910,
	.clr_ofs = 0x910,
	.sta_ofs = 0x910,
};

#define GATE_APU00(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu00_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_APU01(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu01_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate apu0_clks[] = {
	/* APU00 */
	GATE_APU00(CLK_APU0_APU, "apu0_apu",
			"dsp1_ck"/* parent */, 0),
	GATE_APU00(CLK_APU0_AXI_M, "apu0_axi_m",
			"dsp1_ck"/* parent */, 1),
	GATE_APU00(CLK_APU0_JTAG, "apu0_jtag",
			"dsp1_ck"/* parent */, 2),
	/* APU01 */
	GATE_APU01(CLK_APU0_PCLK, "apu0_pclk",
			"dsp1_ck"/* parent */, 25),
};

static int clk_mt6853_apu0_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APU0_NR_CLK);

	mtk_clk_register_gates_with_dev(node, apu0_clks, ARRAY_SIZE(apu0_clks),
			clk_data, &pdev->dev);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6853_apu0[] = {
	{ .compatible = "mediatek,mt6853-apu0", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_apu0_drv = {
	.probe = clk_mt6853_apu0_probe,
	.driver = {
		.name = "clk-mt6853-apu0",
		.of_match_table = of_match_clk_mt6853_apu0,
	},
};

builtin_platform_driver(clk_mt6853_apu0_drv);

#else

static struct platform_driver clk_mt6853_apu0_drv = {
	.probe = clk_mt6853_apu0_probe,
	.driver = {
		.name = "clk-mt6853-apu0",
		.of_match_table = of_match_clk_mt6853_apu0,
	},
};

static int __init clk_mt6853_apu0_init(void)
{
	return platform_driver_register(&clk_mt6853_apu0_drv);
}

static void __exit clk_mt6853_apu0_exit(void)
{
	platform_driver_unregister(&clk_mt6853_apu0_drv);
}

arch_initcall(clk_mt6853_apu0_init);
module_exit(clk_mt6853_apu0_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
