// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6855-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs mdp10_cg_regs = {
	.set_ofs = 0x100,
	.clr_ofs = 0x100,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp11_cg_regs = {
	.set_ofs = 0x110,
	.clr_ofs = 0x110,
	.sta_ofs = 0x110,
};

#define GATE_MDP10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_MDP11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate mdp1_clks[] = {
	/* MDP10 */
	GATE_MDP10(CLK_MDP1_MDP_MUTEX0, "mdp1_mdp_mutex0",
			"mdp0_ck"/* parent */, 0),
	GATE_MDP10(CLK_MDP1_APB_BUS, "mdp1_apb_bus",
			"mdp0_ck"/* parent */, 1),
	GATE_MDP10(CLK_MDP1_SMI0, "mdp1_smi0",
			"mdp0_ck"/* parent */, 2),
	GATE_MDP10(CLK_MDP1_MDP_RDMA0, "mdp1_mdp_rdma0",
			"mdp0_ck"/* parent */, 3),
	GATE_MDP10(CLK_MDP1_MDP_FG0, "mdp1_mdp_fg0",
			"mdp0_ck"/* parent */, 4),
	GATE_MDP10(CLK_MDP1_MDP_HDR0, "mdp1_mdp_hdr0",
			"mdp0_ck"/* parent */, 5),
	GATE_MDP10(CLK_MDP1_MDP_AAL0, "mdp1_mdp_aal0",
			"mdp0_ck"/* parent */, 6),
	GATE_MDP10(CLK_MDP1_MDP_RSZ0, "mdp1_mdp_rsz0",
			"mdp0_ck"/* parent */, 7),
	GATE_MDP10(CLK_MDP1_MDP_TDSHP0, "mdp1_mdp_tdshp0",
			"mdp0_ck"/* parent */, 8),
	GATE_MDP10(CLK_MDP1_MDP_COLOR0, "mdp1_mdp_color0",
			"mdp0_ck"/* parent */, 9),
	GATE_MDP10(CLK_MDP1_MDP_WROT0, "mdp1_mdp_wrot0",
			"mdp0_ck"/* parent */, 10),
	GATE_MDP10(CLK_MDP1_MDP_FAKE_ENG0, "mdp1_mdp_fake_eng0",
			"mdp0_ck"/* parent */, 11),
	GATE_MDP10(CLK_MDP1_MDP_DLI_ASYNC0, "mdp1_mdp_dli_async0",
			"mdp0_ck"/* parent */, 12),
	GATE_MDP10(CLK_MDP1_MDP_DLI_ASYNC1, "mdp1_mdp_dli_async1",
			"mdp0_ck"/* parent */, 13),
	GATE_MDP10(CLK_MDP1_MDP_RDMA1, "mdp1_mdp_rdma1",
			"mdp0_ck"/* parent */, 15),
	GATE_MDP10(CLK_MDP1_MDP_FG1, "mdp1_mdp_fg1",
			"mdp0_ck"/* parent */, 16),
	GATE_MDP10(CLK_MDP1_MDP_HDR1, "mdp1_mdp_hdr1",
			"mdp0_ck"/* parent */, 17),
	GATE_MDP10(CLK_MDP1_MDP_AAL1, "mdp1_mdp_aal1",
			"mdp0_ck"/* parent */, 18),
	GATE_MDP10(CLK_MDP1_MDP_RSZ1, "mdp1_mdp_rsz1",
			"mdp0_ck"/* parent */, 19),
	GATE_MDP10(CLK_MDP1_MDP_TDSHP1, "mdp1_mdp_tdshp1",
			"mdp0_ck"/* parent */, 20),
	GATE_MDP10(CLK_MDP1_MDP_COLOR1, "mdp1_mdp_color1",
			"mdp0_ck"/* parent */, 21),
	GATE_MDP10(CLK_MDP1_MDP_WROT1, "mdp1_mdp_wrot1",
			"mdp0_ck"/* parent */, 22),
	GATE_MDP10(CLK_MDP1_MDP_RSZ2, "mdp1_mdp_rsz2",
			"mdp0_ck"/* parent */, 24),
	GATE_MDP10(CLK_MDP1_MDP_WROT2, "mdp1_mdp_wrot2",
			"mdp0_ck"/* parent */, 25),
	GATE_MDP10(CLK_MDP1_MDP_DLO_ASYNC0, "mdp1_mdp_dlo_async0",
			"mdp0_ck"/* parent */, 26),
	GATE_MDP10(CLK_MDP1_MDP_RSZ3, "mdp1_mdp_rsz3",
			"mdp0_ck"/* parent */, 28),
	GATE_MDP10(CLK_MDP1_MDP_WROT3, "mdp1_mdp_wrot3",
			"mdp0_ck"/* parent */, 29),
	GATE_MDP10(CLK_MDP1_MDP_DLO_ASYNC1, "mdp1_mdp_dlo_async1",
			"mdp0_ck"/* parent */, 30),
	GATE_MDP10(CLK_MDP1_HRE_TOP_MDPSYS, "mdp1_hre_mdpsys",
			"mdp0_ck"/* parent */, 31),
	/* MDP11 */
	GATE_MDP11(CLK_MDP1_FMM_IMG_DL_ASYNC0, "mdp1_mm_img_dl_as0",
			"mdp0_ck"/* parent */, 0),
	GATE_MDP11(CLK_MDP1_FMM_IMG_DL_ASYNC1, "mdp1_mm_img_dl_as1",
			"mdp0_ck"/* parent */, 1),
	GATE_MDP11(CLK_MDP1_FIMG_IMG_DL_ASYNC0, "mdp1_img_dl_as0",
			"mdp0_ck"/* parent */, 2),
	GATE_MDP11(CLK_MDP1_FIMG_IMG_DL_ASYNC1, "mdp1_img_dl_as1",
			"mdp0_ck"/* parent */, 3),
};

static const struct mtk_clk_desc mdp1_mcd = {
	.clks = mdp1_clks,
	.num_clks = CLK_MDP1_NR_CLK,
};

static int clk_mt6855_mdp1_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6855_mdp1[] = {
	{
		.compatible = "mediatek,mt6855-mdpsys1_config",
		.data = &mdp1_mcd,
	},
	{}
};

static struct platform_driver clk_mt6855_mdp1_drv = {
	.probe = clk_mt6855_mdp1_probe,
	.driver = {
		.name = "clk-mt6855-mdp1",
		.of_match_table = of_match_clk_mt6855_mdp1,
	},
};

static int __init clk_mt6855_mdp1_init(void)
{
	return platform_driver_register(&clk_mt6855_mdp1_drv);
}

static void __exit clk_mt6855_mdp1_exit(void)
{
	platform_driver_unregister(&clk_mt6855_mdp1_drv);
}

arch_initcall(clk_mt6855_mdp1_init);
module_exit(clk_mt6855_mdp1_exit);
MODULE_LICENSE("GPL");
