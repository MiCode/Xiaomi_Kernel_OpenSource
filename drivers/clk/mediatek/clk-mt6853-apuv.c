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

static const struct mtk_gate_regs apuv_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APUV(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apuv_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate apuv_clks[] = {
	GATE_APUV(CLK_APUV_AHB, "apuv_ahb",
			"ipu_if_ck"/* parent */, 0),
	GATE_APUV(CLK_APUV_AXI, "apuv_axi",
			"ipu_if_ck"/* parent */, 1),
	GATE_APUV(CLK_APUV_ADL, "apuv_adl",
			"ipu_if_ck"/* parent */, 2),
	GATE_APUV(CLK_APUV_QOS, "apuv_qos",
			"ipu_if_ck"/* parent */, 3),
};

static int clk_mt6853_apuv_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APUV_NR_CLK);

	mtk_clk_register_gates_with_dev(node, apuv_clks, ARRAY_SIZE(apuv_clks),
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

static const struct of_device_id of_match_clk_mt6853_apuv[] = {
	{ .compatible = "mediatek,mt6853-apu_vcore", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_apuv_drv = {
	.probe = clk_mt6853_apuv_probe,
	.driver = {
		.name = "clk-mt6853-apuv",
		.of_match_table = of_match_clk_mt6853_apuv,
	},
};

builtin_platform_driver(clk_mt6853_apuv_drv);

#else

static struct platform_driver clk_mt6853_apuv_drv = {
	.probe = clk_mt6853_apuv_probe,
	.driver = {
		.name = "clk-mt6853-apuv",
		.of_match_table = of_match_clk_mt6853_apuv,
	},
};

static int __init clk_mt6853_apuv_init(void)
{
	return platform_driver_register(&clk_mt6853_apuv_drv);
}

static void __exit clk_mt6853_apuv_exit(void)
{
	platform_driver_unregister(&clk_mt6853_apuv_drv);
}

arch_initcall(clk_mt6853_apuv_init);
module_exit(clk_mt6853_apuv_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
