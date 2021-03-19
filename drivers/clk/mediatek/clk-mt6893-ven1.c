// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

static const struct mtk_gate_regs ven1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_VEN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ven1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate ven1_clks[] = {
	GATE_VEN1(CLK_VEN1_CKE0_LARB, "ven1_cke0_larb",
			"venc_ck"/* parent */, 0),
	GATE_VEN1(CLK_VEN1_CKE1_VENC, "ven1_cke1_venc",
			"venc_ck"/* parent */, 4),
	GATE_VEN1(CLK_VEN1_CKE2_JPGENC, "ven1_cke2_jpgenc",
			"venc_ck"/* parent */, 8),
	GATE_VEN1(CLK_VEN1_CKE3_JPGDEC, "ven1_cke3_jpgdec",
			"venc_ck"/* parent */, 12),
	GATE_VEN1(CLK_VEN1_CKE4_JPGDEC_C1, "ven1_cke4_jpgdec_c1",
			"venc_ck"/* parent */, 16),
	GATE_VEN1(CLK_VEN1_CKE5_GALS, "ven1_cke5_gals",
			"venc_ck"/* parent */, 28),
};

static int clk_mt6893_ven1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_VEN1_NR_CLK);

	mtk_clk_register_gates_with_dev(node, ven1_clks, ARRAY_SIZE(ven1_clks),
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

static const struct of_device_id of_match_clk_mt6893_ven1[] = {
	{ .compatible = "mediatek,mt6893-vencsys", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_ven1_drv = {
	.probe = clk_mt6893_ven1_probe,
	.driver = {
		.name = "clk-mt6893-ven1",
		.of_match_table = of_match_clk_mt6893_ven1,
	},
};

builtin_platform_driver(clk_mt6893_ven1_drv);

#else

static struct platform_driver clk_mt6893_ven1_drv = {
	.probe = clk_mt6893_ven1_probe,
	.driver = {
		.name = "clk-mt6893-ven1",
		.of_match_table = of_match_clk_mt6893_ven1,
	},
};

static int __init clk_mt6893_ven1_init(void)
{
	return platform_driver_register(&clk_mt6893_ven1_drv);
}

static void __exit clk_mt6893_ven1_exit(void)
{
	platform_driver_unregister(&clk_mt6893_ven1_drv);
}

arch_initcall(clk_mt6893_ven1_init);
module_exit(clk_mt6893_ven1_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
