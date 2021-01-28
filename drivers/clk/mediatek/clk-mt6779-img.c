// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/mt6779-clk.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#define MT_CLKMGR_MODULE_INIT	0
#define CCF_SUBSYS_DEBUG		1

static const struct mtk_gate_regs img_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_IMG(_id, _name, _parent, _shift) {	\
	.id = _id,			\
	.name = _name,			\
	.parent_name = _parent,		\
	.regs = &img_cg_regs,		\
	.shift = _shift,		\
	.ops = &mtk_clk_gate_ops_setclr,\
}

#define GATE_IMG_DUMMY(_id, _name, _parent, _shift) {	\
	.id = _id,				\
	.name = _name,				\
	.parent_name = _parent,			\
	.regs = &img_cg_regs,			\
	.shift = _shift,			\
	.ops = &mtk_clk_gate_ops_setclr_dummy,	\
}

static const struct mtk_gate img_clks[] = {
	GATE_IMG_DUMMY(CLK_IMG_LARB5, "imgsys_larb5", "img_sel", 0),
	GATE_IMG_DUMMY(CLK_IMG_LARB6, "imgsys_larb6", "img_sel", 1),
	GATE_IMG(CLK_IMG_DIP, "imgsys_dip", "img_sel", 2),
	GATE_IMG(CLK_IMG_MFB, "imgsys_mfb", "img_sel", 6),
	GATE_IMG(CLK_IMG_WPE_A, "imgsys_wpe_a", "img_sel", 7),
};

static const struct of_device_id of_match_clk_mt6779_img[] = {
	{ .compatible = "mediatek,mt6779-imgsys", },
	{}
};

static int clk_mt6779_img_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_IMG_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

#if CCF_SUBSYS_DEBUG
	pr_info("%s(): clk data number: %d\n", __func__, clk_data->clk_num);
#endif

	mtk_clk_register_gates(node, img_clks, ARRAY_SIZE(img_clks),
			       clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret) {
		pr_notice("%s(): could not register clock provider: %d\n",
					__func__, ret);

		kfree(clk_data);
	}

	return ret;
}

static struct platform_driver clk_mt6779_img_drv = {
	.probe = clk_mt6779_img_probe,
	.driver = {
		.name = "clk-mt6779-img",
		.of_match_table = of_match_clk_mt6779_img,
	},
};

#if MT_CLKMGR_MODULE_INIT

builtin_platform_driver(clk_mt6779_img_drv);

#else

static int __init clk_mt6779_img_platform_init(void)
{
	return platform_driver_register(&clk_mt6779_img_drv);
}

arch_initcall_sync(clk_mt6779_img_platform_init);

#endif /* MT_CLKMGR_MODULE_INIT */

