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
		0x170, INV_OFS, BIT(20), BIT(20));

static const struct mtk_gate_regs mdp0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp1_cg_regs = {
	.set_ofs = 0x124,
	.clr_ofs = 0x128,
	.sta_ofs = 0x120,
};

#define GATE_MDP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MDP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"mdp_ck"/* parent */, 1),
	GATE_MDP0(CLK_MDP_IMG_DL_ASYNC0, "mdp_img_dl_async0",
			"mdp_ck"/* parent */, 2),
	GATE_MDP0(CLK_MDP_IMG_DL_ASYNC1, "mdp_img_dl_async1",
			"mdp_ck"/* parent */, 3),
	GATE_MDP0(CLK_MDP_RDMA1, "mdp_rdma1",
			"mdp_ck"/* parent */, 4),
	GATE_MDP0(CLK_MDP_TDSHP1, "mdp_tdshp1",
			"mdp_ck"/* parent */, 5),
	GATE_MDP0(CLK_MDP_SMI0, "mdp_smi0",
			"mdp_ck"/* parent */, 6),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"mdp_ck"/* parent */, 7),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0",
			"mdp_ck"/* parent */, 8),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0",
			"mdp_ck"/* parent */, 9),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0",
			"mdp_ck"/* parent */, 10),
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP0(CLK_MDP_WROT1, "mdp_wrot1",
			"mdp_ck"/* parent */, 12),
	GATE_MDP0(CLK_MDP_RSZ1, "mdp_rsz1",
			"mdp_ck"/* parent */, 13),
	GATE_MDP0(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0",
			"mdp_ck"/* parent */, 14),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0",
			"mdp_ck"/* parent */, 15),
	GATE_MDP0(CLK_MDP_AAL1, "mdp_aal1",
			"mdp_ck"/* parent */, 16),
	GATE_MDP0(CLK_MDP_COLOR0, "mdp_color0",
			"mdp_ck"/* parent */, 17),
	/* MDP1 */
	GATE_MDP1(CLK_MDP_IMG_DL_RELAY0_ASYNC0, "mdp_img_dl_rel0_as0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP1(CLK_MDP_IMG_DL_RELAY1_ASYNC1, "mdp_img_dl_rel1_as1",
			"mdp_ck"/* parent */, 8),
};

static int clk_mt6833_mdp_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_MDP_NR_CLK);

	mtk_clk_register_gates(node, mdp_clks, ARRAY_SIZE(mdp_clks),
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

static const struct of_device_id of_match_clk_mt6833_mdp[] = {
	{ .compatible = "mediatek,mt6833-mdpsys_config", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6833_mdp_drv = {
	.probe = clk_mt6833_mdp_probe,
	.driver = {
		.name = "clk-mt6833-mdp",
		.of_match_table = of_match_clk_mt6833_mdp,
	},
};

builtin_platform_driver(clk_mt6833_mdp_drv);

#else

static struct platform_driver clk_mt6833_mdp_drv = {
	.probe = clk_mt6833_mdp_probe,
	.driver = {
		.name = "clk-mt6833-mdp",
		.of_match_table = of_match_clk_mt6833_mdp,
	},
};
static int __init clk_mt6833_mdp_platform_init(void)
{
	return platform_driver_register(&clk_mt6833_mdp_drv);
}
arch_initcall(clk_mt6833_mdp_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
