// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Ren-Ting Wang <ren-ting.wang@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6895-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

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
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

#define GATE_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_dummy,	\
	}

static const struct mtk_gate mminfra_config_clks[] = {
	/* MMINFRA_CONFIG0 */
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_D, "mminfra_gce_d",
			"mminfra_ck"/* parent */, 0),
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M, "mminfra_gce_m",
			"mminfra_ck"/* parent */, 1),
	GATE_MMINFRA_CONFIG0(CLK_MMINFRA_SMI, "mminfra_smi",
			"clk26m"/* parent */, 2),
	/* MMINFRA_CONFIG1 */
	GATE_MMINFRA_CONFIG1(CLK_MMINFRA_GCE_26M, "mminfra_gce_26m",
			"mminfra_ck"/* parent */, 17),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs mm00_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm01_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mm02_cg_regs = {
	.set_ofs = 0x1A4,
	.clr_ofs = 0x1A8,
	.sta_ofs = 0x1A0,
};

#define GATE_MM00(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm00_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM01(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm01_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM02(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm02_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm0_clks[] = {
	/* MM00 */
	GATE_MM00(CLK_MM0_DISP_MUTEX0, "mm0_disp_mutex0",
			"disp0_ck"/* parent */, 0),
	GATE_MM00(CLK_MM0_DISP_OVL0, "mm0_disp_ovl0",
			"disp0_ck"/* parent */, 1),
	GATE_MM00(CLK_MM0_DISP_MERGE0, "mm0_disp_merge0",
			"disp0_ck"/* parent */, 2),
	GATE_MM00(CLK_MM0_DISP_FAKE_ENG0, "mm0_disp_fake_eng0",
			"disp0_ck"/* parent */, 3),
	GATE_MM00(CLK_MM0_INLINEROT0, "mm0_inlinerot0",
			"disp0_ck"/* parent */, 4),
	GATE_MM00(CLK_MM0_DISP_WDMA0, "mm0_disp_wdma0",
			"disp0_ck"/* parent */, 5),
	GATE_MM00(CLK_MM0_DISP_FAKE_ENG1, "mm0_disp_fake_eng1",
			"disp0_ck"/* parent */, 6),
	GATE_MM00(CLK_MM0_DISP_DPI0, "mm0_disp_dpi0",
			"disp0_ck"/* parent */, 7),
	GATE_MM00(CLK_MM0_DISP_OVL0_2L_NW, "mm0_disp_ovl0_2l_nw",
			"disp0_ck"/* parent */, 8),
	GATE_MM00(CLK_MM0_DISP_RDMA0, "mm0_disp_rdma0",
			"disp0_ck"/* parent */, 9),
	GATE_MM00(CLK_MM0_DISP_RDMA1, "mm0_disp_rdma1",
			"disp0_ck"/* parent */, 10),
	GATE_MM00(CLK_MM0_DISP_DLI_ASYNC0, "mm0_disp_dli_async0",
			"disp0_ck"/* parent */, 11),
	GATE_MM00(CLK_MM0_DISP_DLI_ASYNC1, "mm0_disp_dli_async1",
			"disp0_ck"/* parent */, 12),
	GATE_MM00(CLK_MM0_DISP_DLI_ASYNC2, "mm0_disp_dli_async2",
			"disp0_ck"/* parent */, 13),
	GATE_MM00(CLK_MM0_DISP_DLO_ASYNC0, "mm0_disp_dlo_async0",
			"disp0_ck"/* parent */, 14),
	GATE_MM00(CLK_MM0_DISP_DLO_ASYNC1, "mm0_disp_dlo_async1",
			"disp0_ck"/* parent */, 15),
	GATE_MM00(CLK_MM0_DISP_DLO_ASYNC2, "mm0_disp_dlo_async2",
			"disp0_ck"/* parent */, 16),
	GATE_MM00(CLK_MM0_DISP_RSZ0, "mm0_disp_rsz0",
			"disp0_ck"/* parent */, 17),
	GATE_MM00(CLK_MM0_DISP_COLOR0, "mm0_disp_color0",
			"disp0_ck"/* parent */, 18),
	GATE_MM00(CLK_MM0_DISP_CCORR0, "mm0_disp_ccorr0",
			"disp0_ck"/* parent */, 19),
	GATE_MM00(CLK_MM0_DISP_CCORR1, "mm0_disp_ccorr1",
			"disp0_ck"/* parent */, 20),
	GATE_MM00(CLK_MM0_DISP_AAL0, "mm0_disp_aal0",
			"disp0_ck"/* parent */, 21),
	GATE_MM00(CLK_MM0_DISP_GAMMA0, "mm0_disp_gamma0",
			"disp0_ck"/* parent */, 22),
	GATE_MM00(CLK_MM0_DISP_POSTMASK0, "mm0_disp_postmask0",
			"disp0_ck"/* parent */, 23),
	GATE_MM00(CLK_MM0_DISP_DITHER0, "mm0_disp_dither0",
			"disp0_ck"/* parent */, 24),
	GATE_MM00(CLK_MM0_DISP_CM0, "mm0_disp_cm0",
			"disp0_ck"/* parent */, 25),
	GATE_MM00(CLK_MM0_DISP_SPR0, "mm0_disp_spr0",
			"disp0_ck"/* parent */, 26),
	GATE_MM00(CLK_MM0_DISP_DSC_WRAP0, "mm0_disp_dsc_wrap0",
			"disp0_ck"/* parent */, 27),
	GATE_MM00(CLK_MM0_FMM_DISP_DSI0, "mm0_fmm_clk0",
			"disp0_ck"/* parent */, 29),
	GATE_MM00(CLK_MM0_DISP_UFBC_WDMA0, "mm0_disp_ufbc_wdma0",
			"disp0_ck"/* parent */, 30),
	GATE_MM00(CLK_MM0_DISP_WDMA1, "mm0_disp_wdma1",
			"disp0_ck"/* parent */, 31),
	/* MM01 */
	GATE_MM01(CLK_MM0_FMM_DISP_DP_INTF0, "mm0_fmm_dp_clk",
			"disp0_ck"/* parent */, 0),
	GATE_MM01(CLK_MM0_APB_BUS, "mm0_apb_bus",
			"disp0_ck"/* parent */, 1),
	GATE_MM01(CLK_MM0_DISP_TDSHP0, "mm0_disp_tdshp0",
			"disp0_ck"/* parent */, 2),
	GATE_MM01(CLK_MM0_DISP_C3D0, "mm0_disp_c3d0",
			"disp0_ck"/* parent */, 3),
	GATE_MM01(CLK_MM0_DISP_Y2R0, "mm0_disp_y2r0",
			"disp0_ck"/* parent */, 4),
	GATE_MM01(CLK_MM0_MDP_AAL0, "mm0_mdp_aal0",
			"disp0_ck"/* parent */, 5),
	GATE_MM01(CLK_MM0_DISP_CHIST0, "mm0_disp_chist0",
			"disp0_ck"/* parent */, 6),
	GATE_MM01(CLK_MM0_DISP_CHIST1, "mm0_disp_chist1",
			"disp0_ck"/* parent */, 7),
	GATE_MM01(CLK_MM0_DISP_OVL0_2L, "mm0_disp_ovl0_2l",
			"disp0_ck"/* parent */, 8),
	GATE_MM01(CLK_MM0_DISP_DLI_ASYNC3, "mm0_disp_dli_async3",
			"disp0_ck"/* parent */, 9),
	GATE_MM01(CLK_MM0_DISP_DLO_ASYNC3, "mm0_disp_dlo_async3",
			"disp0_ck"/* parent */, 10),
	GATE_MM01(CLK_MM0_DISP_OVL1_2L, "mm0_disp_ovl1_2l",
			"disp0_ck"/* parent */, 12),
	GATE_MM01(CLK_MM0_DISP_OVL1_2L_NW, "mm0_disp_ovl1_2l_nw",
			"disp0_ck"/* parent */, 16),
	GATE_MM01(CLK_MM0_SMI_COMMON, "mm0_smi_common",
			"disp0_ck"/* parent */, 20),
	/* MM02 */
	GATE_MM02(CLK_MM0_DISP_DSI, "mm0_clk",
			"disp0_ck"/* parent */, 0),
	GATE_MM02(CLK_MM0_DISP_DP_INTF0, "mm0_dp_clk",
			"disp0_ck"/* parent */, 1),
	GATE_MM02(CLK_MM0_SIG_EMI, "mm0_sig_emi",
			"disp0_ck"/* parent */, 11),
};

static const struct mtk_clk_desc mm0_mcd = {
	.clks = mm0_clks,
	.num_clks = CLK_MM0_NR_CLK,
};

static const struct mtk_gate_regs mm10_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm11_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mm12_cg_regs = {
	.set_ofs = 0x1A4,
	.clr_ofs = 0x1A8,
	.sta_ofs = 0x1A0,
};

#define GATE_MM10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_MM12(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm12_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mm1_clks[] = {
	/* MM10 */
	GATE_MM10(CLK_MM1_DISP_MUTEX0, "mm1_disp_mutex0",
			"disp1_ck"/* parent */, 0),
	GATE_MM10(CLK_MM1_DISP_OVL0, "mm1_disp_ovl0",
			"disp1_ck"/* parent */, 1),
	GATE_MM10(CLK_MM1_DISP_MERGE0, "mm1_disp_merge0",
			"disp1_ck"/* parent */, 2),
	GATE_MM10(CLK_MM1_DISP_FAKE_ENG0, "mm1_disp_fake_eng0",
			"disp1_ck"/* parent */, 3),
	GATE_MM10(CLK_MM1_INLINEROT0, "mm1_inlinerot0",
			"disp1_ck"/* parent */, 4),
	GATE_MM10(CLK_MM1_DISP_WDMA0, "mm1_disp_wdma0",
			"disp1_ck"/* parent */, 5),
	GATE_MM10(CLK_MM1_DISP_FAKE_ENG1, "mm1_disp_fake_eng1",
			"disp1_ck"/* parent */, 6),
	GATE_MM10(CLK_MM1_DISP_DPI0, "mm1_disp_dpi0",
			"disp1_ck"/* parent */, 7),
	GATE_MM10(CLK_MM1_DISP_OVL0_2L_NW, "mm1_disp_ovl0_2l_nw",
			"disp1_ck"/* parent */, 8),
	GATE_MM10(CLK_MM1_DISP_RDMA0, "mm1_disp_rdma0",
			"disp1_ck"/* parent */, 9),
	GATE_MM10(CLK_MM1_DISP_RDMA1, "mm1_disp_rdma1",
			"disp1_ck"/* parent */, 10),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC0, "mm1_disp_dli_async0",
			"disp1_ck"/* parent */, 11),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC1, "mm1_disp_dli_async1",
			"disp1_ck"/* parent */, 12),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC2, "mm1_disp_dli_async2",
			"disp1_ck"/* parent */, 13),
	GATE_MM10(CLK_MM1_DISP_DLO_ASYNC0, "mm1_disp_dlo_async0",
			"disp1_ck"/* parent */, 14),
	GATE_MM10(CLK_MM1_DISP_DLO_ASYNC1, "mm1_disp_dlo_async1",
			"disp1_ck"/* parent */, 15),
	GATE_MM10(CLK_MM1_DISP_DLO_ASYNC2, "mm1_disp_dlo_async2",
			"disp1_ck"/* parent */, 16),
	GATE_MM10(CLK_MM1_DISP_RSZ0, "mm1_disp_rsz0",
			"disp1_ck"/* parent */, 17),
	GATE_MM10(CLK_MM1_DISP_COLOR0, "mm1_disp_color0",
			"disp1_ck"/* parent */, 18),
	GATE_MM10(CLK_MM1_DISP_CCORR0, "mm1_disp_ccorr0",
			"disp1_ck"/* parent */, 19),
	GATE_MM10(CLK_MM1_DISP_CCORR1, "mm1_disp_ccorr1",
			"disp1_ck"/* parent */, 20),
	GATE_MM10(CLK_MM1_DISP_AAL0, "mm1_disp_aal0",
			"disp1_ck"/* parent */, 21),
	GATE_MM10(CLK_MM1_DISP_GAMMA0, "mm1_disp_gamma0",
			"disp1_ck"/* parent */, 22),
	GATE_MM10(CLK_MM1_DISP_POSTMASK0, "mm1_disp_postmask0",
			"disp1_ck"/* parent */, 23),
	GATE_MM10(CLK_MM1_DISP_DITHER0, "mm1_disp_dither0",
			"disp1_ck"/* parent */, 24),
	GATE_MM10(CLK_MM1_DISP_CM0, "mm1_disp_cm0",
			"disp1_ck"/* parent */, 25),
	GATE_MM10(CLK_MM1_DISP_SPR0, "mm1_disp_spr0",
			"disp1_ck"/* parent */, 26),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP0, "mm1_disp_dsc_wrap0",
			"disp1_ck"/* parent */, 27),
	GATE_MM10(CLK_MM1_FMM_DISP_DSI0, "mm1_fmm_clk0",
			"disp1_ck"/* parent */, 29),
	GATE_MM10(CLK_MM1_DISP_UFBC_WDMA0, "mm1_disp_ufbc_wdma0",
			"disp1_ck"/* parent */, 30),
	GATE_MM10(CLK_MM1_DISP_WDMA1, "mm1_disp_wdma1",
			"disp1_ck"/* parent */, 31),
	/* MM11 */
	GATE_MM11(CLK_MM1_FMM_DISP_DP_INTF0, "mm1_fmm_dp_clk",
			"disp1_ck"/* parent */, 0),
	GATE_MM11(CLK_MM1_APB_BUS, "mm1_apb_bus",
			"disp1_ck"/* parent */, 1),
	GATE_MM11(CLK_MM1_DISP_TDSHP0, "mm1_disp_tdshp0",
			"disp1_ck"/* parent */, 2),
	GATE_MM11(CLK_MM1_DISP_C3D0, "mm1_disp_c3d0",
			"disp1_ck"/* parent */, 3),
	GATE_MM11(CLK_MM1_DISP_Y2R0, "mm1_disp_y2r0",
			"disp1_ck"/* parent */, 4),
	GATE_MM11(CLK_MM1_MDP_AAL0, "mm1_mdp_aal0",
			"disp1_ck"/* parent */, 5),
	GATE_MM11(CLK_MM1_DISP_CHIST0, "mm1_disp_chist0",
			"disp1_ck"/* parent */, 6),
	GATE_MM11(CLK_MM1_DISP_CHIST1, "mm1_disp_chist1",
			"disp1_ck"/* parent */, 7),
	GATE_MM11(CLK_MM1_DISP_OVL0_2L, "mm1_disp_ovl0_2l",
			"disp1_ck"/* parent */, 8),
	GATE_MM11(CLK_MM1_DISP_DLI_ASYNC3, "mm1_disp_dli_async3",
			"disp1_ck"/* parent */, 9),
	GATE_MM11(CLK_MM1_DISP_DLO_ASYNC3, "mm1_disp_dlo_async3",
			"disp1_ck"/* parent */, 10),
	GATE_MM11(CLK_MM1_DISP_OVL1_2L, "mm1_disp_ovl1_2l",
			"disp1_ck"/* parent */, 12),
	GATE_MM11(CLK_MM1_DISP_OVL1_2L_NW, "mm1_disp_ovl1_2l_nw",
			"disp1_ck"/* parent */, 16),
	GATE_MM11(CLK_MM1_SMI_COMMON, "mm1_smi_common",
			"disp1_ck"/* parent */, 20),
	/* MM12 */
	GATE_MM12(CLK_MM1_DISP_DSI, "mm1_clk",
			"disp1_ck"/* parent */, 0),
	GATE_MM12(CLK_MM1_DISP_DP_INTF0, "mm1_dp_clk",
			"disp1_ck"/* parent */, 1),
	GATE_MM12(CLK_MM1_SIG_EMI, "mm1_sig_emi",
			"hf_fdisp2_ck"/* parent */, 11),
};

static const struct mtk_clk_desc mm1_mcd = {
	.clks = mm1_clks,
	.num_clks = CLK_MM1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6895_mmsys[] = {
	{
		.compatible = "mediatek,mt6895-mminfra_config",
		.data = &mminfra_config_mcd,
	}, {
		.compatible = "mediatek,mt6895-mmsys0",
		.data = &mm0_mcd,
	}, {
		.compatible = "mediatek,mt6895-mmsys1",
		.data = &mm1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6895_mmsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6895_mmsys_drv = {
	.probe = clk_mt6895_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6895-mmsys",
		.of_match_table = of_match_clk_mt6895_mmsys,
	},
};

static int __init clk_mt6895_mmsys_init(void)
{
	return platform_driver_register(&clk_mt6895_mmsys_drv);
}

static void __exit clk_mt6895_mmsys_exit(void)
{
	platform_driver_unregister(&clk_mt6895_mmsys_drv);
}

arch_initcall(clk_mt6895_mmsys_init);
module_exit(clk_mt6895_mmsys_exit);
MODULE_LICENSE("GPL");
