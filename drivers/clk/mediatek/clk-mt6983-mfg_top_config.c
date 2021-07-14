// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6983-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

static const struct mtk_gate_regs mfg_top_config_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MFG_TOP_CONFIG_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mfg_top_config_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate mfg_top_config_clks[] = {
	GATE_MFG_TOP_CONFIG_0(CLK_MFG_TOP_CONFIG_BG3D_PDN /* CLK ID */,
		"mfg_top_config_bg3d_pdn" /* name */,
		"mfg_ref_ck" /* parent */, 0 /* bit */),
};

static int clk_mt6983_mfg_top_config_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_MFG_TOP_CONFIG_NR_CLK);

	mtk_clk_register_gates(node, mfg_top_config_clks,
		ARRAY_SIZE(mfg_top_config_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct of_device_id of_match_clk_mt6983_mfg_top_config[] = {
	{
		.compatible = "mediatek,mt6983-mfg_top_config",
	}, {
		/* sentinel */
	}
};

static struct platform_driver clk_mt6983_mfg_top_config_drv = {
	.probe = clk_mt6983_mfg_top_config_probe,
	.driver = {
		.name = "clk-mt6983-mfg_top_config",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_mfg_top_config,
	},
};
#if MT_CLKMGR_MODULE_INIT

builtin_platform_driver(clk_mt6983_mfg_top_config_drv);

#else /* ! MT_CLKMGR_MODULE_INIT */

static int __init clk_mt6983_mfg_top_config_init(void)
{
	return platform_driver_register(&clk_mt6983_mfg_top_config_drv);
}

static void __exit clk_mt6983_mfg_top_config_exit(void)
{
	platform_driver_unregister(&clk_mt6983_mfg_top_config_drv);
}

arch_initcall(clk_mt6983_mfg_top_config_init);
module_exit(clk_mt6983_mfg_top_config_exit);
MODULE_LICENSE("GPL");
#endif	/* MT_CLKMGR_MODULE_INIT */
