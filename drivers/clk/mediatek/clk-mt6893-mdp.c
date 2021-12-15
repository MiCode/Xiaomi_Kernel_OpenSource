// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs mdp0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mdp1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mdp2_cg_regs = {
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
	}

#define GATE_MDP2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mdp2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY1(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &mdp1_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate mdp_clks[] = {
	/* MDP0 */
	GATE_MDP0(CLK_MDP_RDMA0, "mdp_rdma0",
			"mdp_ck"/* parent */, 0),
	GATE_MDP0(CLK_MDP_FG0, "mdp_fg0",
			"mdp_ck"/* parent */, 1),
	GATE_MDP0(CLK_MDP_HDR0, "mdp_hdr0",
			"mdp_ck"/* parent */, 2),
	GATE_MDP0(CLK_MDP_AAL0, "mdp_aal0",
			"mdp_ck"/* parent */, 3),
	GATE_MDP0(CLK_MDP_RSZ0, "mdp_rsz0",
			"mdp_ck"/* parent */, 4),
	GATE_MDP0(CLK_MDP_TDSHP0, "mdp_tdshp0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP0(CLK_MDP_TCC0, "mdp_tcc0",
			"mdp_ck"/* parent */, 6),
	GATE_MDP0(CLK_MDP_WROT0, "mdp_wrot0",
			"mdp_ck"/* parent */, 7),
	GATE_MDP0(CLK_MDP_RDMA2, "mdp_rdma2",
			"mdp_ck"/* parent */, 8),
	GATE_MDP0(CLK_MDP_AAL2, "mdp_aal2",
			"mdp_ck"/* parent */, 9),
	GATE_MDP0(CLK_MDP_RSZ2, "mdp_rsz2",
			"mdp_ck"/* parent */, 10),
	GATE_MDP0(CLK_MDP_COLOR0, "mdp_color0",
			"mdp_ck"/* parent */, 11),
	GATE_MDP0(CLK_MDP_TDSHP2, "mdp_tdshp2",
			"mdp_ck"/* parent */, 12),
	GATE_MDP0(CLK_MDP_TCC2, "mdp_tcc2",
			"mdp_ck"/* parent */, 13),
	GATE_MDP0(CLK_MDP_WROT2, "mdp_wrot2",
			"mdp_ck"/* parent */, 14),
	GATE_MDP0(CLK_MDP_MUTEX0, "mdp_mutex0",
			"mdp_ck"/* parent */, 15),
	GATE_MDP0(CLK_MDP_RDMA1, "mdp_rdma1",
			"mdp_ck"/* parent */, 16),
	GATE_MDP0(CLK_MDP_FG1, "mdp_fg1",
			"mdp_ck"/* parent */, 17),
	GATE_MDP0(CLK_MDP_HDR1, "mdp_hdr1",
			"mdp_ck"/* parent */, 18),
	GATE_MDP0(CLK_MDP_AAL1, "mdp_aal1",
			"mdp_ck"/* parent */, 19),
	GATE_MDP0(CLK_MDP_RSZ1, "mdp_rsz1",
			"mdp_ck"/* parent */, 20),
	GATE_MDP0(CLK_MDP_TDSHP1, "mdp_tdshp1",
			"mdp_ck"/* parent */, 21),
	GATE_MDP0(CLK_MDP_TCC1, "mdp_tcc1",
			"mdp_ck"/* parent */, 22),
	GATE_MDP0(CLK_MDP_WROT1, "mdp_wrot1",
			"mdp_ck"/* parent */, 23),
	GATE_MDP0(CLK_MDP_RDMA3, "mdp_rdma3",
			"mdp_ck"/* parent */, 24),
	GATE_MDP0(CLK_MDP_AAL3, "mdp_aal3",
			"mdp_ck"/* parent */, 25),
	GATE_MDP0(CLK_MDP_RSZ3, "mdp_rsz3",
			"mdp_ck"/* parent */, 26),
	GATE_MDP0(CLK_MDP_COLOR1, "mdp_color1",
			"mdp_ck"/* parent */, 27),
	GATE_MDP0(CLK_MDP_TDSHP3, "mdp_tdshp3",
			"mdp_ck"/* parent */, 28),
	GATE_MDP0(CLK_MDP_TCC3, "mdp_tcc3",
			"mdp_ck"/* parent */, 29),
	GATE_MDP0(CLK_MDP_WROT3, "mdp_wrot3",
			"mdp_ck"/* parent */, 30),
	GATE_MDP0(CLK_MDP_APB_BUS, "mdp_apb_bus",
			"mdp_ck"/* parent */, 31),
	/* MDP1 */
	GATE_MDP1(CLK_MDP_MMSYSRAM, "mdp_mmsysram",
			"mdp_ck"/* parent */, 0),
	GATE_DUMMY1(CLK_MDP_APMCU_GALS, "mdp_apmcu_gals",
			"mdp_ck"/* parent */, 1),
	GATE_MDP1(CLK_MDP_FAKE_ENG0, "mdp_fake_eng0",
			"mdp_ck"/* parent */, 2),
	GATE_MDP1(CLK_MDP_FAKE_ENG1, "mdp_fake_eng1",
			"mdp_ck"/* parent */, 3),
	GATE_DUMMY1(CLK_MDP_SMI0, "mdp_smi0",
			"mdp_ck"/* parent */, 4),
	GATE_MDP1(CLK_MDP_IMG_DL_ASYNC0, "mdp_img_dl_async0",
			"mdp_ck"/* parent */, 5),
	GATE_MDP1(CLK_MDP_IMG_DL_ASYNC1, "mdp_img_dl_async1",
			"mdp_ck"/* parent */, 6),
	GATE_MDP1(CLK_MDP_IMG_DL_ASYNC2, "mdp_img_dl_async2",
			"mdp_ck"/* parent */, 7),
	GATE_DUMMY1(CLK_MDP_SMI1, "mdp_smi1",
			"mdp_ck"/* parent */, 8),
	GATE_MDP1(CLK_MDP_IMG_DL_ASYNC3, "mdp_img_dl_async3",
			"mdp_ck"/* parent */, 9),
	GATE_MDP1(CLK_MDP_RESERVED42, "mdp_reserved42",
			"mdp_ck"/* parent */, 10),
	GATE_MDP1(CLK_MDP_RESERVED43, "mdp_reserved43",
			"mdp_ck"/* parent */, 11),
	GATE_DUMMY1(CLK_MDP_SMI2, "mdp_smi2",
			"mdp_ck"/* parent */, 12),
	GATE_MDP1(CLK_MDP_RESERVED45, "mdp_reserved45",
			"mdp_ck"/* parent */, 13),
	GATE_MDP1(CLK_MDP_RESERVED46, "mdp_reserved46",
			"mdp_ck"/* parent */, 14),
	GATE_MDP1(CLK_MDP_RESERVED47, "mdp_reserved47",
			"mdp_ck"/* parent */, 15),
	/* MDP2 */
	GATE_MDP2(CLK_MDP_IMG0_IMG_DL_ASYNC0, "mdp_img0_dl_as0",
			"img1_ck"/* parent */, 0),
	GATE_MDP2(CLK_MDP_IMG0_IMG_DL_ASYNC1, "mdp_img0_dl_as1",
			"img1_ck"/* parent */, 1),
	GATE_MDP2(CLK_MDP_IMG1_IMG_DL_ASYNC2, "mdp_img1_dl_as2",
			"img2_ck"/* parent */, 8),
	GATE_MDP2(CLK_MDP_IMG1_IMG_DL_ASYNC3, "mdp_img1_dl_as3",
			"img2_ck"/* parent */, 9),
};

static const struct mtk_clk_desc mdp_mcd = {
	.clks = mdp_clks,
	.num_clks = CLK_MDP_NR_CLK,
};

static int clk_mt6893_mdp_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6893_mdp[] = {
	{
		.compatible = "mediatek,mt6893-mdpsys",
		.data = &mdp_mcd,
	},
	{}
};

static struct platform_driver clk_mt6893_mdp_drv = {
	.probe = clk_mt6893_mdp_probe,
	.driver = {
		.name = "clk-mt6893-mdp",
		.of_match_table = of_match_clk_mt6893_mdp,
	},
};

static int __init clk_mt6893_mdp_init(void)
{
	return platform_driver_register(&clk_mt6893_mdp_drv);
}

static void __exit clk_mt6893_mdp_exit(void)
{
	platform_driver_unregister(&clk_mt6893_mdp_drv);
}

postcore_initcall(clk_mt6893_mdp_init);
module_exit(clk_mt6893_mdp_exit);
MODULE_LICENSE("GPL");
