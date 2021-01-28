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

static const struct mtk_gate_regs apuvcore_cg_regs = {
	.set_ofs = 0x0004,
	.clr_ofs = 0x0008,
	.sta_ofs = 0x0000,
};

#define GATE_APU_VCORE_DUMMY(_id, _name, _parent, _shift) {	\
	.id = _id,				\
	.name = _name,				\
	.parent_name = _parent,			\
	.regs = &apuvcore_cg_regs,		\
	.shift = _shift,			\
	.ops = &mtk_clk_gate_ops_setclr_dummy,	\
}


static const struct mtk_gate apuvcore_clks[] = {
	GATE_APU_VCORE_DUMMY(CLK_APU_VCORE_AHB,
			"apu_vcore_ahb", "ipu_if_sel", 0),
	GATE_APU_VCORE_DUMMY(CLK_APU_VCORE_AXI,
			"apu_vcore_axi", "ipu_if_sel", 1),
	GATE_APU_VCORE_DUMMY(CLK_APU_VCORE_ADL,
			"apu_vcore_adl", "ipu_if_sel", 2),
	GATE_APU_VCORE_DUMMY(CLK_APU_VCORE_QOS,
			"apu_vcore_qos", "ipu_if_sel", 3),
};

static const struct of_device_id of_match_clk_mt6779_apuvcore[] = {
	{ .compatible = "mediatek,mt6779-apu_vcore", },
	{}
};

static int clk_mt6779_apuvcore_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_APU_VCORE_NR_CLK);
	if (!clk_data) {
		pr_notice("%s(): alloc clk data failed\n", __func__);
		return -ENOMEM;
	}

#if CCF_SUBSYS_DEBUG
	pr_info("%s(): clk data number: %d\n", __func__, clk_data->clk_num);
#endif

	mtk_clk_register_gates(node, apuvcore_clks, ARRAY_SIZE(apuvcore_clks),
			       clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret) {
		pr_notice("%s(): could not register clock provider: %d\n",
					__func__, ret);

		kfree(clk_data);
	}

	return ret;
}

static struct platform_driver clk_mt6779_apuvcore_drv = {
	.probe = clk_mt6779_apuvcore_probe,
	.driver = {
		.name = "clk-mt6779-apu_vcore",
		.of_match_table = of_match_clk_mt6779_apuvcore,
	},
};

#if MT_CLKMGR_MODULE_INIT

builtin_platform_driver(clk_mt6779_apuvcore_drv);

#else

static int __init clk_mt6779_apuvcore_platform_init(void)
{
	return platform_driver_register(&clk_mt6779_apuvcore_drv);
}
arch_initcall_sync(clk_mt6779_apuvcore_platform_init);

#endif /* MT_CLKMGR_MODULE_INIT */

