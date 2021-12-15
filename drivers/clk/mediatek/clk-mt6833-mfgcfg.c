/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6833-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP			1

#define INV_OFS			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(0x16C,
		0x170, INV_OFS, 0xBC, 0xBC);

static const struct mtk_gate_regs mfgcfg_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFGCFG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mfgcfg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate mfgcfg_clks[] = {
	GATE_MFGCFG(CLK_MFGCFG_BG3D, "mfgcfg_bg3d",
			"mfg_ref_ck"/* parent */, 0),
};

static int clk_mt6833_mfgcfg_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_MFGCFG_NR_CLK);

	mtk_clk_register_gates(node, mfgcfg_clks, ARRAY_SIZE(mfgcfg_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6833_mfgcfg[] = {
	{ .compatible = "mediatek,mt6833-mfgsys", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6833_mfgcfg_drv = {
	.probe = clk_mt6833_mfgcfg_probe,
	.driver = {
		.name = "clk-mt6833-mfgcfg",
		.of_match_table = of_match_clk_mt6833_mfgcfg,
	},
};

builtin_platform_driver(clk_mt6833_mfgcfg_drv);

#else

static struct platform_driver clk_mt6833_mfgcfg_drv = {
	.probe = clk_mt6833_mfgcfg_probe,
	.driver = {
		.name = "clk-mt6833-mfgcfg",
		.of_match_table = of_match_clk_mt6833_mfgcfg,
	},
};
static int __init clk_mt6833_mfgcfg_platform_init(void)
{
	return platform_driver_register(&clk_mt6833_mfgcfg_drv);
}
arch_initcall(clk_mt6833_mfgcfg_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
