// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6855-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mm2_cg_regs = {
	.set_ofs = 0x1A4,
	.clr_ofs = 0x1A8,
	.sta_ofs = 0x1A0,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"disp0_ck"/* parent */, 0),
	GATE_MM0(CLK_MM_DISP_OVL0, "mm_disp_ovl0",
			"disp0_ck"/* parent */, 1),
	GATE_MM0(CLK_MM_DISP_MERGE0, "mm_disp_merge0",
			"disp0_ck"/* parent */, 2),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG0, "mm_disp_fake_eng0",
			"disp0_ck"/* parent */, 3),
	GATE_MM0(CLK_MM_DISP_INLINEROT0, "mm_disp_inlinerot0",
			"disp0_ck"/* parent */, 4),
	GATE_MM0(CLK_MM_DISP_WDMA0, "mm_disp_wdma0",
			"disp0_ck"/* parent */, 5),
	GATE_MM0(CLK_MM_DISP_FAKE_ENG1, "mm_disp_fake_eng1",
			"disp0_ck"/* parent */, 6),
	GATE_MM0(CLK_MM_DISP_DPI0, "mm_disp_dpi0",
			"disp0_ck"/* parent */, 7),
	GATE_MM0(CLK_MM_DISP_OVL0_2L_NW, "mm_disp_ovl0_2l_nw",
			"disp0_ck"/* parent */, 8),
	GATE_MM0(CLK_MM_DISP_RDMA0, "mm_disp_rdma0",
			"disp0_ck"/* parent */, 9),
	GATE_MM0(CLK_MM_DISP_RDMA1, "mm_disp_rdma1",
			"disp0_ck"/* parent */, 10),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC0, "mm_disp_dli_async0",
			"disp0_ck"/* parent */, 11),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC1, "mm_disp_dli_async1",
			"disp0_ck"/* parent */, 12),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC2, "mm_disp_dli_async2",
			"disp0_ck"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_DLO_ASYNC0, "mm_disp_dlo_async0",
			"disp0_ck"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_DLO_ASYNC1, "mm_disp_dlo_async1",
			"disp0_ck"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_DLO_ASYNC2, "mm_disp_dlo_async2",
			"disp0_ck"/* parent */, 16),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"disp0_ck"/* parent */, 17),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp0_ck"/* parent */, 18),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp0_ck"/* parent */, 19),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp0_ck"/* parent */, 20),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp0_ck"/* parent */, 21),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp0_ck"/* parent */, 22),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"disp0_ck"/* parent */, 23),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp0_ck"/* parent */, 24),
	GATE_MM0(CLK_MM_DISP_CM0, "mm_disp_cm0",
			"disp0_ck"/* parent */, 25),
	GATE_MM0(CLK_MM_DISP_SPR0, "mm_disp_spr0",
			"disp0_ck"/* parent */, 26),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP0, "mm_disp_dsc_wrap0",
			"disp0_ck"/* parent */, 27),
	GATE_MM0(CLK_MM_DISP_DSI0, "mm_CLK0",
			"disp0_ck"/* parent */, 29),
	GATE_MM0(CLK_MM_DISP_UFBC_WDMA0, "mm_disp_ufbc_wdma0",
			"disp0_ck"/* parent */, 30),
	GATE_MM0(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"disp0_ck"/* parent */, 31),
	/* MM1 */
	GATE_MM1(CLK_MM_DISP_DP_INTF0, "mm_DP_CLK",
			"disp0_ck"/* parent */, 0),
	GATE_MM1(CLK_MM_DISP_APB_BUS, "mm_disp_apb_bus",
			"disp0_ck"/* parent */, 1),
	GATE_MM1(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
			"disp0_ck"/* parent */, 2),
	GATE_MM1(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
			"disp0_ck"/* parent */, 3),
	GATE_MM1(CLK_MM_DISP_Y2R0, "mm_disp_y2r0",
			"disp0_ck"/* parent */, 4),
	GATE_MM1(CLK_MM_MDP_AAL0, "mm_mdp_aal0",
			"disp0_ck"/* parent */, 5),
	GATE_MM1(CLK_MM_DISP_CHIST0, "mm_disp_chist0",
			"disp0_ck"/* parent */, 6),
	GATE_MM1(CLK_MM_DISP_CHIST1, "mm_disp_chist1",
			"disp0_ck"/* parent */, 7),
	GATE_MM1(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l",
			"disp0_ck"/* parent */, 8),
	GATE_MM1(CLK_MM_DISP_DLI_ASYNC3, "mm_disp_dli_async3",
			"disp0_ck"/* parent */, 9),
	GATE_MM1(CLK_MM_DISP_DL0_ASYNC3, "mm_disp_dl0_async3",
			"disp0_ck"/* parent */, 10),
	GATE_MM1(CLK_MM_DISP_OVL1_2L, "mm_disp_ovl1_2l",
			"disp0_ck"/* parent */, 12),
	GATE_MM1(CLK_MM_DISP_OVL1_2L_NW, "mm_disp_ovl1_2l_nw",
			"disp0_ck"/* parent */, 16),
	GATE_MM1(CLK_MM_SMI_COMMON, "mm_smi_common",
			"disp0_ck"/* parent */, 20),
	/* MM2 */
	GATE_MM2(CLK_MM_DSI, "mm_dsi_ck",
			"disp0_ck"/* parent */, 0),
	GATE_MM2(CLK_MM_DPI, "mm_dpi_ck",
			"disp0_ck"/* parent */, 1),
};

static const struct mtk_clk_desc mm_mcd = {
	.clks = mm_clks,
	.num_clks = CLK_MM_NR_CLK,
};

static const struct mtk_gate_regs mminfra_config0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

#define GATE_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MMINFRA_CONFIG0_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}

#define GATE_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mminfra_config_clks[] = {
	/* MMINFRA_CONFIG0 */
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_D, "mminfra_gce_d",
			"mminfra_ck"/* parent */, 0),
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M, "mminfra_gce_m",
			"mminfra_ck"/* parent */, 1),
	GATE_MMINFRA_CONFIG0_DUMMY(CLK_MMINFRA_SMI, "mminfra_smi",
			"mminfra_ck"/* parent */, 2),
	/* MMINFRA_CONFIG1 */
	GATE_MMINFRA_CONFIG1(CLK_MMINFRA_GCE_26M, "mminfra_gce_26m",
			"mminfra_ck"/* parent */, 17),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6855_mmsys[] = {
	{
		.compatible = "mediatek,mt6855-dispsys",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6855-mminfra_config",
		.data = &mminfra_config_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6855_mmsys_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6855_mmsys_drv = {
	.probe = clk_mt6855_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6855-mmsys",
		.of_match_table = of_match_clk_mt6855_mmsys,
	},
};

static int __init clk_mt6855_mmsys_init(void)
{
	return platform_driver_register(&clk_mt6855_mmsys_drv);
}

static void __exit clk_mt6855_mmsys_exit(void)
{
	platform_driver_unregister(&clk_mt6855_mmsys_drv);
}

arch_initcall(clk_mt6855_mmsys_init);
module_exit(clk_mt6855_mmsys_exit);
MODULE_LICENSE("GPL");
