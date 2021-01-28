// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6779-clk.h>

#define MT_CLKMGR_MODULE_INIT	0
#define CCF_SUBSYS_DEBUG		1

static const struct mtk_gate_regs mfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFG(_id, _name, _parent, _shift) {	\
	.id = _id,				\
	.name = _name,				\
	.parent_name = _parent,			\
	.regs = &mfg_cg_regs,			\
	.shift = _shift,			\
	.ops = &mtk_clk_gate_ops_setclr,	\
}

static const struct mtk_gate mfg_clks[] = {
	GATE_MFG(CLK_MFGCFG_BG3D, "mfg_bg3d", "mfg_sel", 0),
};

static int clk_mt6779_mfg_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_MFGCFG_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed!\n", __func__);
		return -ENOMEM;
	}

#if CCF_SUBSYS_DEBUG
	pr_info("%s(): clk data number: %d\n", __func__, clk_data->clk_num);
#endif

	mtk_clk_register_gates(node, mfg_clks, ARRAY_SIZE(mfg_clks),
			clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret) {
		pr_notice("%s(): could not register clock provider: %d\n",
					__func__, ret);

		kfree(clk_data);
	}

	return ret;
}

static const struct of_device_id of_match_clk_mt6779_mfg[] = {
	{ .compatible = "mediatek,mt6779-mfgcfg", },
	{}
};

static struct platform_driver clk_mt6779_mfg_drv = {
	.probe = clk_mt6779_mfg_probe,
	.driver = {
		.name = "clk-mt6779-mfg",
		.of_match_table = of_match_clk_mt6779_mfg,
	},
};


#if MT_CLKMGR_MODULE_INIT

builtin_platform_driver(clk_mt6779_mfg_drv);

#else

static int __init clk_mt6779_mfg_platform_init(void)
{
	return platform_driver_register(&clk_mt6779_mfg_drv);
}

arch_initcall_sync(clk_mt6779_mfg_platform_init);

#endif /* MT_CLKMGR_MODULE_INIT */

