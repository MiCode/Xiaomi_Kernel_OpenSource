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

static const struct mtk_gate_regs apu1_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_APU1(_id, _name, _parent, _shift) {	\
	.id = _id,				\
	.name = _name,				\
	.parent_name = _parent,			\
	.regs = &apu1_cg_regs,			\
	.shift = _shift,			\
	.ops = &mtk_clk_gate_ops_setclr,	\
}

static const struct mtk_gate apu1_clks[] = {
	GATE_APU1(CLK_APU1_APU, "apu1_apu", "dsp2_sel", 0),
	GATE_APU1(CLK_APU1_AXI_M, "apu1_axi", "dsp2_sel", 1),
	GATE_APU1(CLK_APU1_JTAG, "apu1_jtag", "dsp2_sel", 2),
};

static const struct of_device_id of_match_clk_mt6779_apu1[] = {
	{ .compatible = "mediatek,mt6779-apu1", },
	{}
};

static int clk_mt6779_apu1_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_APU1_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data\n", __func__);
		return -ENOMEM;
	}

#if CCF_SUBSYS_DEBUG
	pr_info("%s(): clk data number: %d\n", __func__, clk_data->clk_num);
#endif

	mtk_clk_register_gates(node, apu1_clks, ARRAY_SIZE(apu1_clks),
			       clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret) {
		pr_notice("%s(): could not register clock provider: %d\n",
					__func__, ret);

		kfree(clk_data);
	}

	return ret;
}

static struct platform_driver clk_mt6779_apu1_drv = {
	.probe = clk_mt6779_apu1_probe,
	.driver = {
		.name = "clk-mt6779-apu1",
		.of_match_table = of_match_clk_mt6779_apu1,
	},
};

#if MT_CLKMGR_MODULE_INIT

builtin_platform_driver(clk_mt6779_apu1_drv);

#else

static int __init clk_mt6779_apu1_platform_init(void)
{
	return platform_driver_register(&clk_mt6779_apu1_drv);
}

arch_initcall_sync(clk_mt6779_apu1_platform_init);

#endif /* MT_CLKMGR_MODULE_INIT */

