// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6853-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP			1

static const struct mtk_gate_regs scp_par_cg_regs = {
	.set_ofs = 0x0180,
	.clr_ofs = 0x0180,
	.sta_ofs = 0x0180,
};

#define GATE_SCP_PAR(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_par_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate scp_par_clks[] = {
	GATE_SCP_PAR(CLK_SCP_PAR_ADSP_PLL, "scp_par_adsp_pll",
			"adsp_ck"/* parent */, 0),
};

static int clk_mt6853_scp_par_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_SCP_PAR_NR_CLK);

	mtk_clk_register_gates(node, scp_par_clks, ARRAY_SIZE(scp_par_clks),
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

static const struct of_device_id of_match_clk_mt6853_scp_par[] = {
	{ .compatible = "mediatek,mt6853-scp", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_scp_par_drv = {
	.probe = clk_mt6853_scp_par_probe,
	.driver = {
		.name = "clk-mt6853-scp_par",
		.of_match_table = of_match_clk_mt6853_scp_par,
	},
};

builtin_platform_driver(clk_mt6853_scp_par_drv);

#else

static struct platform_driver clk_mt6853_scp_par_drv = {
	.probe = clk_mt6853_scp_par_probe,
	.driver = {
		.name = "clk-mt6853-scp_par",
		.of_match_table = of_match_clk_mt6853_scp_par,
	},
};
static int __init clk_mt6853_scp_par_platform_init(void)
{
	return platform_driver_register(&clk_mt6853_scp_par_drv);
}
arch_initcall(clk_mt6853_scp_par_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
