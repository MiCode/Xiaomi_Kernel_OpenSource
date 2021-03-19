// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

static const struct mtk_gate_regs camsys_rawc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_CAMSYS_RAWC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &camsys_rawc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate camsys_rawc_clks[] = {
	GATE_CAMSYS_RAWC(CLK_CAMSYS_RAWC_LARBX, "camsys_rawc_larbx",
			"cam_ck"/* parent */, 0),
	GATE_CAMSYS_RAWC(CLK_CAMSYS_RAWC_CAM, "camsys_rawc_cam",
			"cam_ck"/* parent */, 1),
	GATE_CAMSYS_RAWC(CLK_CAMSYS_RAWC_CAMTG, "camsys_rawc_camtg",
			"cam_ck"/* parent */, 2),
};

static int clk_mt6893_camsys_rawc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct platform_device *pwr_pdev;
	struct clk_onecell_data *clk_data;
	struct device_node *pwr_node;
	int r = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	pwr_node = of_parse_phandle(node, "mt6893-camsys_rawc_pwr", 0);
	if (!pwr_node) {
		dev_err(&pdev->dev, "Failed to get the pwr node\n");
		return -EINVAL;
	}

	pwr_pdev = of_find_device_by_node(pwr_node);
	of_node_put(pwr_node);
	if (!pwr_pdev) {
		dev_err(&pdev->dev, "Failed to get the pwr device\n");
		return -EINVAL;
	}

	clk_data = mtk_alloc_clk_data(CLK_CAMSYS_RAWC_NR_CLK);

	mtk_clk_register_gates_with_dev(node, camsys_rawc_clks, ARRAY_SIZE(camsys_rawc_clks),
			clk_data, &pwr_pdev->dev);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6893_camsys_rawc[] = {
	{ .compatible = "mediatek,mt6893-camsys_rawc", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_camsys_rawc_drv = {
	.probe = clk_mt6893_camsys_rawc_probe,
	.driver = {
		.name = "clk-mt6893-camsys_rawc",
		.of_match_table = of_match_clk_mt6893_camsys_rawc,
	},
};

builtin_platform_driver(clk_mt6893_camsys_rawc_drv);

#else

static struct platform_driver clk_mt6893_camsys_rawc_drv = {
	.probe = clk_mt6893_camsys_rawc_probe,
	.driver = {
		.name = "clk-mt6893-camsys_rawc",
		.of_match_table = of_match_clk_mt6893_camsys_rawc,
	},
};

static int __init clk_mt6893_camsys_rawc_init(void)
{
	return platform_driver_register(&clk_mt6893_camsys_rawc_drv);
}

static void __exit clk_mt6893_camsys_rawc_exit(void)
{
	platform_driver_unregister(&clk_mt6893_camsys_rawc_drv);
}

arch_initcall(clk_mt6893_camsys_rawc_init);
module_exit(clk_mt6893_camsys_rawc_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
