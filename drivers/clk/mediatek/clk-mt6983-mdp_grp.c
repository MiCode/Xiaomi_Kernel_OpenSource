// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6983-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

static const struct mtk_gate_regs mdpsys_config_0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_MDP_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mdpsys_config_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

static struct mtk_gate mdp_clks[] = {
	GATE_MDP_0(CLK_MDP_MDP_MUTEX0 /* CLK ID */,
		"mdp_mdp_mutex0" /* name */,
		"mdp0_ck" /* parent */, 0 /* bit */),
	GATE_MDP_0(CLK_MDP_APB_BUS /* CLK ID */,
		"mdp_apb_bus" /* name */,
		"mdp0_ck" /* parent */, 1 /* bit */),
	GATE_MDP_0(CLK_MDP_SMI0 /* CLK ID */,
		"mdp_smi0" /* name */,
		"mdp0_ck" /* parent */, 2 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_RDMA0 /* CLK ID */,
		"mdp_mdp_rdma0" /* name */,
		"mdp0_ck" /* parent */, 3 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_FG0 /* CLK ID */,
		"mdp_mdp_fg0" /* name */,
		"mdp0_ck" /* parent */, 4 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_HDR0 /* CLK ID */,
		"mdp_mdp_hdr0" /* name */,
		"mdp0_ck" /* parent */, 5 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_AAL0 /* CLK ID */,
		"mdp_mdp_aal0" /* name */,
		"mdp0_ck" /* parent */, 6 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_RSZ0 /* CLK ID */,
		"mdp_mdp_rsz0" /* name */,
		"mdp0_ck" /* parent */, 7 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_TDSHP0 /* CLK ID */,
		"mdp_mdp_tdshp0" /* name */,
		"mdp0_ck" /* parent */, 8 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_COLOR0 /* CLK ID */,
		"mdp_mdp_color0" /* name */,
		"mdp0_ck" /* parent */, 9 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_WROT0 /* CLK ID */,
		"mdp_mdp_wrot0" /* name */,
		"mdp0_ck" /* parent */, 10 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_FAKE_ENG0 /* CLK ID */,
		"mdp_mdp_fake_eng0" /* name */,
		"mdp0_ck" /* parent */, 11 /* bit */),
	GATE_MDP_0(CLK_MDP_IMG_DL_RELAY0 /* CLK ID */,
		"mdp_img_dl_relay0" /* name */,
		"mdp0_ck" /* parent */, 12 /* bit */),
	GATE_MDP_0(CLK_MDP_IMG_DL_RELAY1 /* CLK ID */,
		"mdp_img_dl_relay1" /* name */,
		"mdp0_ck" /* parent */, 13 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_RDMA1 /* CLK ID */,
		"mdp_mdp_rdma1" /* name */,
		"mdp0_ck" /* parent */, 15 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_FG1 /* CLK ID */,
		"mdp_mdp_fg1" /* name */,
		"mdp0_ck" /* parent */, 16 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_HDR1 /* CLK ID */,
		"mdp_mdp_hdr1" /* name */,
		"mdp0_ck" /* parent */, 17 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_AAL1 /* CLK ID */,
		"mdp_mdp_aal1" /* name */,
		"mdp0_ck" /* parent */, 18 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_RSZ1 /* CLK ID */,
		"mdp_mdp_rsz1" /* name */,
		"mdp0_ck" /* parent */, 19 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_TDSHP1 /* CLK ID */,
		"mdp_mdp_tdshp1" /* name */,
		"mdp0_ck" /* parent */, 20 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_COLOR1 /* CLK ID */,
		"mdp_mdp_color1" /* name */,
		"mdp0_ck" /* parent */, 21 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_WROT1 /* CLK ID */,
		"mdp_mdp_wrot1" /* name */,
		"mdp0_ck" /* parent */, 22 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_RSZ2 /* CLK ID */,
		"mdp_mdp_rsz2" /* name */,
		"mdp0_ck" /* parent */, 24 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_WROT2 /* CLK ID */,
		"mdp_mdp_wrot2" /* name */,
		"mdp0_ck" /* parent */, 25 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_DLO_ASYNC0 /* CLK ID */,
		"mdp_mdp_dlo_async0" /* name */,
		"mdp0_ck" /* parent */, 26 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_RSZ3 /* CLK ID */,
		"mdp_mdp_rsz3" /* name */,
		"mdp0_ck" /* parent */, 28 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_WROT3 /* CLK ID */,
		"mdp_mdp_wrot3" /* name */,
		"mdp0_ck" /* parent */, 29 /* bit */),
	GATE_MDP_0(CLK_MDP_MDP_DLO_ASYNC1 /* CLK ID */,
		"mdp_mdp_dlo_async1" /* name */,
		"mdp0_ck" /* parent */, 30 /* bit */),
	GATE_MDP_0(CLK_MDP_HRE_TOP_MDPSYS /* CLK ID */,
		"mdp_hre_top_mdpsys" /* name */,
		"mdp0_ck" /* parent */, 31 /* bit */),
};

static int clk_mt6983_mdpsys_config_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_MDP_NR_CLK);

	mtk_clk_register_gates(node, mdp_clks,
		ARRAY_SIZE(mdp_clks),
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

static const struct mtk_gate_regs mdpsys1_config_0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_MDPSYS1_CONFIG_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mdpsys1_config_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate mdpsys1_config_clks[] = {
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_MUTEX0 /* CLK ID */,
		"mdpsys1_config_mdp_mutex0" /* name */,
		"mdp1_ck" /* parent */, 0 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_APB_BUS /* CLK ID */,
		"mdpsys1_config_apb_bus" /* name */,
		"mdp1_ck" /* parent */, 1 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_SMI0 /* CLK ID */,
		"mdpsys1_config_smi0" /* name */,
		"mdp1_ck" /* parent */, 2 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_RDMA0 /* CLK ID */,
		"mdpsys1_config_mdp_rdma0" /* name */,
		"mdp1_ck" /* parent */, 3 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_FG0 /* CLK ID */,
		"mdpsys1_config_mdp_fg0" /* name */,
		"mdp1_ck" /* parent */, 4 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_HDR0 /* CLK ID */,
		"mdpsys1_config_mdp_hdr0" /* name */,
		"mdp1_ck" /* parent */, 5 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_AAL0 /* CLK ID */,
		"mdpsys1_config_mdp_aal0" /* name */,
		"mdp1_ck" /* parent */, 6 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_RSZ0 /* CLK ID */,
		"mdpsys1_config_mdp_rsz0" /* name */,
		"mdp1_ck" /* parent */, 7 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_TDSHP0 /* CLK ID */,
		"mdpsys1_config_mdp_tdshp0" /* name */,
		"mdp1_ck" /* parent */, 8 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_COLOR0 /* CLK ID */,
		"mdpsys1_config_mdp_color0" /* name */,
		"mdp1_ck" /* parent */, 9 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_WROT0 /* CLK ID */,
		"mdpsys1_config_mdp_wrot0" /* name */,
		"mdp1_ck" /* parent */, 10 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_FAKE_ENG0 /* CLK ID */,
		"mdpsys1_config_mdp_fake_eng0" /* name */,
		"mdp1_ck" /* parent */, 11 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_IMG_DL_RELAY0 /* CLK ID */,
		"mdpsys1_config_img_dl_relay0" /* name */,
		"mdp1_ck" /* parent */, 12 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_IMG_DL_RELAY1 /* CLK ID */,
		"mdpsys1_config_img_dl_relay1" /* name */,
		"mdp1_ck" /* parent */, 13 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_RDMA1 /* CLK ID */,
		"mdpsys1_config_mdp_rdma1" /* name */,
		"mdp1_ck" /* parent */, 15 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_FG1 /* CLK ID */,
		"mdpsys1_config_mdp_fg1" /* name */,
		"mdp1_ck" /* parent */, 16 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_HDR1 /* CLK ID */,
		"mdpsys1_config_mdp_hdr1" /* name */,
		"mdp1_ck" /* parent */, 17 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_AAL1 /* CLK ID */,
		"mdpsys1_config_mdp_aal1" /* name */,
		"mdp1_ck" /* parent */, 18 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_RSZ1 /* CLK ID */,
		"mdpsys1_config_mdp_rsz1" /* name */,
		"mdp1_ck" /* parent */, 19 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_TDSHP1 /* CLK ID */,
		"mdpsys1_config_mdp_tdshp1" /* name */,
		"mdp1_ck" /* parent */, 20 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_COLOR1 /* CLK ID */,
		"mdpsys1_config_mdp_color1" /* name */,
		"mdp1_ck" /* parent */, 21 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_WROT1 /* CLK ID */,
		"mdpsys1_config_mdp_wrot1" /* name */,
		"mdp1_ck" /* parent */, 22 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_RSZ2 /* CLK ID */,
		"mdpsys1_config_mdp_rsz2" /* name */,
		"mdp1_ck" /* parent */, 24 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_WROT2 /* CLK ID */,
		"mdpsys1_config_mdp_wrot2" /* name */,
		"mdp1_ck" /* parent */, 25 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_DLO_ASYNC0 /* CLK ID */,
		"mdpsys1_config_mdp_dlo_async0" /* name */,
		"mdp1_ck" /* parent */, 26 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_RSZ3 /* CLK ID */,
		"mdpsys1_config_mdp_rsz3" /* name */,
		"mdp1_ck" /* parent */, 28 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_WROT3 /* CLK ID */,
		"mdpsys1_config_mdp_wrot3" /* name */,
		"mdp1_ck" /* parent */, 29 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_MDP_DLO_ASYNC1 /* CLK ID */,
		"mdpsys1_config_mdp_dlo_async1" /* name */,
		"mdp1_ck" /* parent */, 30 /* bit */),
	GATE_MDPSYS1_CONFIG_0(CLK_MDPSYS1_CONFIG_HRE_TOP_MDPSYS /* CLK ID */,
		"mdpsys1_config_hre_top_mdpsys" /* name */,
		"mdp1_ck" /* parent */, 31 /* bit */),
};

static int clk_mt6983_mdpsys1_config_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_MDPSYS1_CONFIG_NR_CLK);

	mtk_clk_register_gates(node, mdpsys1_config_clks,
		ARRAY_SIZE(mdpsys1_config_clks),
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


static const struct of_device_id of_match_clk_mt6983_mdp[] = {
	{
		.compatible = "mediatek,mt6983-mdpsys_config",
		.data = clk_mt6983_mdpsys_config_probe,
	}, {
		.compatible = "mediatek,mt6983-mdpsys1_config",
		.data = clk_mt6983_mdpsys1_config_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6983_mdp_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

	return r;
}

static struct platform_driver clk_mt6983_mdp_drv = {
	.probe = clk_mt6983_mdp_probe,
	.driver = {
		.name = "clk-mt6983-mdp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_mdp,
	},
};

static int __init clk_mt6983_mdp_init(void)
{
	return platform_driver_register(&clk_mt6983_mdp_drv);
}

static void __exit clk_mt6983_mdp_exit(void)
{
	platform_driver_unregister(&clk_mt6983_mdp_drv);
}

arch_initcall(clk_mt6983_mdp_init);
module_exit(clk_mt6983_mdp_exit);
MODULE_LICENSE("GPL");

