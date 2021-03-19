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

static const struct mtk_gate_regs scp_adsp_cg_regs = {
	.set_ofs = 0x180,
	.clr_ofs = 0x180,
	.sta_ofs = 0x180,
};

#define GATE_SCP_ADSP(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_adsp_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate scp_adsp_clks[] = {
	GATE_SCP_ADSP(CLK_SCP_ADSP_RG_AUDIODSP, "scp_adsp_audiodsp",
			"adsp_ck"/* parent */, 0),
};

static int clk_mt6893_scp_adsp_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_SCP_ADSP_NR_CLK);

	mtk_clk_register_gates(node, scp_adsp_clks, ARRAY_SIZE(scp_adsp_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6893_scp_adsp[] = {
	{ .compatible = "mediatek,mt6893-scp_adsp", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6893_scp_adsp_drv = {
	.probe = clk_mt6893_scp_adsp_probe,
	.driver = {
		.name = "clk-mt6893-scp_adsp",
		.of_match_table = of_match_clk_mt6893_scp_adsp,
	},
};

builtin_platform_driver(clk_mt6893_scp_adsp_drv);

#else

static struct platform_driver clk_mt6893_scp_adsp_drv = {
	.probe = clk_mt6893_scp_adsp_probe,
	.driver = {
		.name = "clk-mt6893-scp_adsp",
		.of_match_table = of_match_clk_mt6893_scp_adsp,
	},
};

static int __init clk_mt6893_scp_adsp_init(void)
{
	return platform_driver_register(&clk_mt6893_scp_adsp_drv);
}

static void __exit clk_mt6893_scp_adsp_exit(void)
{
	platform_driver_unregister(&clk_mt6893_scp_adsp_drv);
}

arch_initcall(clk_mt6893_scp_adsp_init);
module_exit(clk_mt6893_scp_adsp_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
