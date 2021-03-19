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

static const struct mtk_gate_regs vde10_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde11_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde12_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xc,
	.sta_ofs = 0x8,
};

#define GATE_VDE10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

#define GATE_VDE12(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde12_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate vde1_clks[] = {
	/* VDE10 */
	GATE_VDE10(CLK_VDE1_VDEC_CKEN, "vde1_vdec_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE10(CLK_VDE1_VDEC_ACTIVE, "vde1_vdec_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE10(CLK_VDE1_VDEC_CKEN_ENG, "vde1_vdec_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE11 */
	GATE_VDE11(CLK_VDE1_LAT_CKEN, "vde1_lat_cken",
			"vdec_ck"/* parent */, 0),
	GATE_VDE11(CLK_VDE1_LAT_ACTIVE, "vde1_lat_active",
			"vdec_ck"/* parent */, 4),
	GATE_VDE11(CLK_VDE1_LAT_CKEN_ENG, "vde1_lat_cken_eng",
			"vdec_ck"/* parent */, 8),
	/* VDE12 */
	GATE_VDE12(CLK_VDE1_LARB1_CKEN, "vde1_larb1_cken",
			"vdec_ck"/* parent */, 0),
};

static int clk_mt6893_vde1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_VDE1_NR_CLK);

	mtk_clk_register_gates_with_dev(node, vde1_clks, ARRAY_SIZE(vde1_clks),
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

static const struct of_device_id of_match_clk_mt6893_vde1[] = {
	{ .compatible = "mediatek,mt6893-vdecsys_soc", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_vde1_drv = {
	.probe = clk_mt6893_vde1_probe,
	.driver = {
		.name = "clk-mt6893-vde1",
		.of_match_table = of_match_clk_mt6893_vde1,
	},
};

builtin_platform_driver(clk_mt6893_vde1_drv);

#else

static struct platform_driver clk_mt6893_vde1_drv = {
	.probe = clk_mt6893_vde1_probe,
	.driver = {
		.name = "clk-mt6893-vde1",
		.of_match_table = of_match_clk_mt6893_vde1,
	},
};

static int __init clk_mt6893_vde1_init(void)
{
	return platform_driver_register(&clk_mt6893_vde1_drv);
}

static void __exit clk_mt6893_vde1_exit(void)
{
	platform_driver_unregister(&clk_mt6893_vde1_drv);
}

arch_initcall(clk_mt6893_vde1_init);
module_exit(clk_mt6893_vde1_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
