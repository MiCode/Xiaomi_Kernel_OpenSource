// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6985-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

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
	GATE_MM10(CLK_MM1_CONFIG, "mm1_config",
			"disp1_ck"/* parent */, 0),
	GATE_MM10(CLK_MM1_DISP_MUTEX0, "mm1_disp_mutex0",
			"disp1_ck"/* parent */, 1),
	GATE_MM10(CLK_MM1_DISP_AAL0, "mm1_disp_aal0",
			"disp1_ck"/* parent */, 2),
	GATE_MM10(CLK_MM1_DISP_C3D0, "mm1_disp_c3d0",
			"disp1_ck"/* parent */, 3),
	GATE_MM10(CLK_MM1_DISP_CCORR0, "mm1_disp_ccorr0",
			"disp1_ck"/* parent */, 4),
	GATE_MM10(CLK_MM1_DISP_CCORR1, "mm1_disp_ccorr1",
			"disp1_ck"/* parent */, 5),
	GATE_MM10(CLK_MM1_DISP_CHIST0, "mm1_disp_chist0",
			"disp1_ck"/* parent */, 6),
	GATE_MM10(CLK_MM1_DISP_CHIST1, "mm1_disp_chist1",
			"disp1_ck"/* parent */, 7),
	GATE_MM10(CLK_MM1_DISP_COLOR0, "mm1_disp_color0",
			"disp1_ck"/* parent */, 8),
	GATE_MM10(CLK_MM1_DISP_DITHER0, "mm1_disp_dither0",
			"disp1_ck"/* parent */, 9),
	GATE_MM10(CLK_MM1_DISP_DITHER1, "mm1_disp_dither1",
			"disp1_ck"/* parent */, 10),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC0, "mm1_disp_dli_async0",
			"disp1_ck"/* parent */, 11),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC1, "mm1_disp_dli_async1",
			"disp1_ck"/* parent */, 12),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC2, "mm1_disp_dli_async2",
			"disp1_ck"/* parent */, 13),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC3, "mm1_disp_dli_async3",
			"disp1_ck"/* parent */, 14),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC4, "mm1_disp_dli_async4",
			"disp1_ck"/* parent */, 15),
	GATE_MM10(CLK_MM1_DISP_DLI_ASYNC5, "mm1_disp_dli_async5",
			"disp1_ck"/* parent */, 16),
	GATE_MM10(CLK_MM1_DISP_DLO_ASYNC0, "mm1_disp_dlo_async0",
			"disp1_ck"/* parent */, 17),
	GATE_MM10(CLK_MM1_DISP_DLO_ASYNC1, "mm1_disp_dlo_async1",
			"disp1_ck"/* parent */, 18),
	GATE_MM10(CLK_MM1_DISP_DP_INTF0, "mm1_disp_dp_intf0",
			"disp1_ck"/* parent */, 19),
	GATE_MM10(CLK_MM1_DISP_DSC_WRAP0, "mm1_disp_dsc_wrap0",
			"disp1_ck"/* parent */, 20),
	GATE_MM10(CLK_MM1_DISP_DSI0, "mm1_clk0",
			"disp1_ck"/* parent */, 21),
	GATE_MM10(CLK_MM1_DISP_GAMMA0, "mm1_disp_gamma0",
			"disp1_ck"/* parent */, 22),
	GATE_MM10(CLK_MM1_MDP_AAL0, "mm1_mdp_aal0",
			"disp1_ck"/* parent */, 23),
	GATE_MM10(CLK_MM1_MDP_RDMA0, "mm1_mdp_rdma0",
			"disp1_ck"/* parent */, 24),
	GATE_MM10(CLK_MM1_DISP_MERGE0, "mm1_disp_merge0",
			"disp1_ck"/* parent */, 25),
	GATE_MM10(CLK_MM1_DISP_MERGE1, "mm1_disp_merge1",
			"disp1_ck"/* parent */, 26),
	GATE_MM10(CLK_MM1_DISP_ODDMR0, "mm1_disp_oddmr0",
			"disp1_ck"/* parent */, 27),
	GATE_MM10(CLK_MM1_DISP_POSTALIGN0, "mm1_disp_postalign0",
			"disp1_ck"/* parent */, 28),
	GATE_MM10(CLK_MM1_DISP_POSTMASK0, "mm1_disp_postmask0",
			"disp1_ck"/* parent */, 29),
	GATE_MM10(CLK_MM1_DISP_RELAY0, "mm1_disp_relay0",
			"disp1_ck"/* parent */, 30),
	GATE_MM10(CLK_MM1_DISP_RSZ0, "mm1_disp_rsz0",
			"disp1_ck"/* parent */, 31),
	/* MM11 */
	GATE_MM11(CLK_MM1_DISP_SPR0, "mm1_disp_spr0",
			"disp1_ck"/* parent */, 0),
	GATE_MM11(CLK_MM1_DISP_TDSHP0, "mm1_disp_tdshp0",
			"disp1_ck"/* parent */, 1),
	GATE_MM11(CLK_MM1_DISP_TDSHP1, "mm1_disp_tdshp1",
			"disp1_ck"/* parent */, 2),
	GATE_MM11(CLK_MM1_DISP_UFBC_WDMA1, "mm1_disp_ufbc_wdma1",
			"disp1_ck"/* parent */, 3),
	GATE_MM11(CLK_MM1_DISP_VDCM0, "mm1_disp_vdcm0",
			"disp1_ck"/* parent */, 4),
	GATE_MM11(CLK_MM1_DISP_WDMA1, "mm1_disp_wdma1",
			"disp1_ck"/* parent */, 5),
	GATE_MM11(CLK_MM1_SMI_SUB_COMM0, "mm1_smi_sub_comm0",
			"disp1_ck"/* parent */, 6),
	GATE_MM11(CLK_MM1_DISP_Y2R0, "mm1_disp_y2r0",
			"disp1_ck"/* parent */, 7),
	/* MM12 */
	GATE_MM12(CLK_MM1_DSI_CLK, "mm1_dsi_clk",
			"disp1_ck"/* parent */, 0),
	GATE_MM12(CLK_MM1_DP_CLK, "mm1_dp_clk",
			"disp1_ck"/* parent */, 1),
	GATE_MM12(CLK_MM1_26M_CLK, "mm1_26m_clk",
			"disp1_ck"/* parent */, 11),
};

static const struct mtk_clk_desc mm1_mcd = {
	.clks = mm1_clks,
	.num_clks = CLK_MM1_NR_CLK,
};

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
	GATE_MM0(CLK_MM_CONFIG, "mm_config",
			"disp0_ck"/* parent */, 0),
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"disp0_ck"/* parent */, 1),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp0_ck"/* parent */, 2),
	GATE_MM0(CLK_MM_DISP_C3D0, "mm_disp_c3d0",
			"disp0_ck"/* parent */, 3),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp0_ck"/* parent */, 4),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp0_ck"/* parent */, 5),
	GATE_MM0(CLK_MM_DISP_CHIST0, "mm_disp_chist0",
			"disp0_ck"/* parent */, 6),
	GATE_MM0(CLK_MM_DISP_CHIST1, "mm_disp_chist1",
			"disp0_ck"/* parent */, 7),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp0_ck"/* parent */, 8),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp0_ck"/* parent */, 9),
	GATE_MM0(CLK_MM_DISP_DITHER1, "mm_disp_dither1",
			"disp0_ck"/* parent */, 10),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC0, "mm_disp_dli_async0",
			"disp0_ck"/* parent */, 11),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC1, "mm_disp_dli_async1",
			"disp0_ck"/* parent */, 12),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC2, "mm_disp_dli_async2",
			"disp0_ck"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC3, "mm_disp_dli_async3",
			"disp0_ck"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC4, "mm_disp_dli_async4",
			"disp0_ck"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_DLI_ASYNC5, "mm_disp_dli_async5",
			"disp0_ck"/* parent */, 16),
	GATE_MM0(CLK_MM_DISP_DLO_ASYNC0, "mm_disp_dlo_async0",
			"disp0_ck"/* parent */, 17),
	GATE_MM0(CLK_MM_DISP_DLO_ASYNC1, "mm_disp_dlo_async1",
			"disp0_ck"/* parent */, 18),
	GATE_MM0(CLK_MM_DISP_DP_INTF0, "mm_disp_dp_intf0",
			"disp0_ck"/* parent */, 19),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP0, "mm_disp_dsc_wrap0",
			"disp0_ck"/* parent */, 20),
	GATE_MM0(CLK_MM_DISP_DSI0, "mm_clk0",
			"disp0_ck"/* parent */, 21),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp0_ck"/* parent */, 22),
	GATE_MM0(CLK_MM_MDP_AAL0, "mm_mdp_aal0",
			"disp0_ck"/* parent */, 23),
	GATE_MM0(CLK_MM_MDP_RDMA0, "mm_mdp_rdma0",
			"disp0_ck"/* parent */, 24),
	GATE_MM0(CLK_MM_DISP_MERGE0, "mm_disp_merge0",
			"disp0_ck"/* parent */, 25),
	GATE_MM0(CLK_MM_DISP_MERGE1, "mm_disp_merge1",
			"disp0_ck"/* parent */, 26),
	GATE_MM0(CLK_MM_DISP_ODDMR0, "mm_disp_oddmr0",
			"disp0_ck"/* parent */, 27),
	GATE_MM0(CLK_MM_DISP_POSTALIGN0, "mm_disp_postalign0",
			"disp0_ck"/* parent */, 28),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"disp0_ck"/* parent */, 29),
	GATE_MM0(CLK_MM_DISP_RELAY0, "mm_disp_relay0",
			"disp0_ck"/* parent */, 30),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"disp0_ck"/* parent */, 31),
	/* MM1 */
	GATE_MM1(CLK_MM_DISP_SPR0, "mm_disp_spr0",
			"disp0_ck"/* parent */, 0),
	GATE_MM1(CLK_MM_DISP_TDSHP0, "mm_disp_tdshp0",
			"disp0_ck"/* parent */, 1),
	GATE_MM1(CLK_MM_DISP_TDSHP1, "mm_disp_tdshp1",
			"disp0_ck"/* parent */, 2),
	GATE_MM1(CLK_MM_DISP_UFBC_WDMA1, "mm_disp_ufbc_wdma1",
			"disp0_ck"/* parent */, 3),
	GATE_MM1(CLK_MM_DISP_VDCM0, "mm_disp_vdcm0",
			"disp0_ck"/* parent */, 4),
	GATE_MM1(CLK_MM_DISP_WDMA1, "mm_disp_wdma1",
			"disp0_ck"/* parent */, 5),
	GATE_MM1(CLK_MM_SMI_SUB_COMM0, "mm_smi_sub_comm0",
			"disp0_ck"/* parent */, 6),
	GATE_MM1(CLK_MM_DISP_Y2R0, "mm_disp_y2r0",
			"disp0_ck"/* parent */, 7),
	/* MM2 */
	GATE_MM2(CLK_MM_DSI_CLK, "mm_dsi_clk",
			"disp0_ck"/* parent */, 0),
	GATE_MM2(CLK_MM_DP_CLK, "mm_dp_clk",
			"disp0_ck"/* parent */, 1),
	GATE_MM2(CLK_MM_26M_CLK, "mm_26m_clk",
			"disp0_ck"/* parent */, 11),
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

static const struct mtk_gate_regs mminfra_config0_hwv_regs = {
	.set_ofs = 0x0040,
	.clr_ofs = 0x0044,
	.sta_ofs = 0x1C20,
};

static const struct mtk_gate_regs mminfra_config1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mminfra_config1_hwv_regs = {
	.set_ofs = 0x0048,
	.clr_ofs = 0x004C,
	.sta_ofs = 0x1C24,
};

#define GATE_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_CONFIG0(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config0_cg_regs,			\
		.hwv_regs = &mminfra_config0_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

#define GATE_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mminfra_config1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_HWV_MMINFRA_CONFIG1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config1_cg_regs,			\
		.hwv_regs = &mminfra_config1_hwv_regs,		\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,				\
		.flags = CLK_USE_HW_VOTER,				\
	}

static const struct mtk_gate mminfra_config_clks[] = {
	/* MMINFRA_CONFIG0 */
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_D, "mminfra_gce_d",
			"mminfra_ck"/* parent */, 0),
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_GCE_M, "mminfra_gce_m",
			"mminfra_ck"/* parent */, 1),
	GATE_HWV_MMINFRA_CONFIG0(CLK_MMINFRA_SMI, "mminfra_smi",
			"mminfra_ck"/* parent */, 2),
	/* MMINFRA_CONFIG1 */
	GATE_HWV_MMINFRA_CONFIG1(CLK_MMINFRA_GCE_26M, "mminfra_gce_26m",
			"mminfra_ck"/* parent */, 17),
};

static const struct mtk_clk_desc mminfra_config_mcd = {
	.clks = mminfra_config_clks,
	.num_clks = CLK_MMINFRA_CONFIG_NR_CLK,
};

static const struct mtk_gate_regs ovl1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_OVL1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ovl1_clks[] = {
	GATE_OVL1(CLK_OVL1_CONFIG, "ovl1_config",
			"ovl1_ck"/* parent */, 0),
	GATE_OVL1(CLK_OVL1_DISP_FAKE_ENG0, "ovl1_disp_fake_eng0",
			"ovl1_ck"/* parent */, 1),
	GATE_OVL1(CLK_OVL1_DISP_FAKE_ENG1, "ovl1_disp_fake_eng1",
			"ovl1_ck"/* parent */, 2),
	GATE_OVL1(CLK_OVL1_DISP_MUTEX0, "ovl1_disp_mutex0",
			"ovl1_ck"/* parent */, 3),
	GATE_OVL1(CLK_OVL1_OVL0_2L, "ovl1_ovl0_2l",
			"ovl1_ck"/* parent */, 4),
	GATE_OVL1(CLK_OVL1_OVL1_2L, "ovl1_ovl1_2l",
			"ovl1_ck"/* parent */, 5),
	GATE_OVL1(CLK_OVL1_OVL2_2L, "ovl1_ovl2_2l",
			"ovl1_ck"/* parent */, 6),
	GATE_OVL1(CLK_OVL1_OVL3_2L, "ovl1_ovl3_2l",
			"ovl1_ck"/* parent */, 7),
	GATE_OVL1(CLK_OVL1_DISP_RSZ1, "ovl1_disp_rsz1",
			"ovl1_ck"/* parent */, 8),
	GATE_OVL1(CLK_OVL1_MDP_RSZ0, "ovl1_mdp_rsz0",
			"ovl1_ck"/* parent */, 9),
	GATE_OVL1(CLK_OVL1_DISP_WDMA0, "ovl1_disp_wdma0",
			"ovl1_ck"/* parent */, 10),
	GATE_OVL1(CLK_OVL1_DISP_UFBC_WDMA0, "ovl1_disp_ufbc_wdma0",
			"ovl1_ck"/* parent */, 11),
	GATE_OVL1(CLK_OVL1_DISP_WDMA2, "ovl1_disp_wdma2",
			"ovl1_ck"/* parent */, 12),
	GATE_OVL1(CLK_OVL1_DISP_DLI_ASYNC0, "ovl1_disp_dli_async0",
			"ovl1_ck"/* parent */, 13),
	GATE_OVL1(CLK_OVL1_DISP_DLI_ASYNC1, "ovl1_disp_dli_async1",
			"ovl1_ck"/* parent */, 14),
	GATE_OVL1(CLK_OVL1_DISP_DLI_ASYNC2, "ovl1_disp_dli_async2",
			"ovl1_ck"/* parent */, 15),
	GATE_OVL1(CLK_OVL1_DISP_DLO_ASYNC0, "ovl1_disp_dlo_async0",
			"ovl1_ck"/* parent */, 16),
	GATE_OVL1(CLK_OVL1_DISP_DLO_ASYNC1, "ovl1_disp_dlo_async1",
			"ovl1_ck"/* parent */, 17),
	GATE_OVL1(CLK_OVL1_DISP_DLO_ASYNC2, "ovl1_disp_dlo_async2",
			"ovl1_ck"/* parent */, 18),
	GATE_OVL1(CLK_OVL1_DISP_DLO_ASYNC3, "ovl1_disp_dlo_async3",
			"ovl1_ck"/* parent */, 19),
	GATE_OVL1(CLK_OVL1_DISP_DLO_ASYNC4, "ovl1_disp_dlo_async4",
			"ovl1_ck"/* parent */, 20),
	GATE_OVL1(CLK_OVL1_DISP_DLO_ASYNC5, "ovl1_disp_dlo_async5",
			"ovl1_ck"/* parent */, 21),
	GATE_OVL1(CLK_OVL1_DISP_DLO_ASYNC6, "ovl1_disp_dlo_async6",
			"ovl1_ck"/* parent */, 22),
	GATE_OVL1(CLK_OVL1_INLINEROT, "ovl1_inlinerot",
			"ovl1_ck"/* parent */, 23),
	GATE_OVL1(CLK_OVL1_SMI_SUB_COMMON0, "ovl1_smi_sub_common0",
			"ovl1_ck"/* parent */, 24),
	GATE_OVL1(CLK_OVL1_DISP_Y2R0, "ovl1_disp_y2r0",
			"ovl1_ck"/* parent */, 25),
	GATE_OVL1(CLK_OVL1_DISP_Y2R1, "ovl1_disp_y2r1",
			"ovl1_ck"/* parent */, 26),
};

static const struct mtk_clk_desc ovl1_mcd = {
	.clks = ovl1_clks,
	.num_clks = CLK_OVL1_NR_CLK,
};

static const struct mtk_gate_regs ovl_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_OVL(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ovl_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ovl_clks[] = {
	GATE_OVL(CLK_OVL_CONFIG, "ovl_config",
			"ovl0_ck"/* parent */, 0),
	GATE_OVL(CLK_OVL_DISP_FAKE_ENG0, "ovl_disp_fake_eng0",
			"ovl0_ck"/* parent */, 1),
	GATE_OVL(CLK_OVL_DISP_FAKE_ENG1, "ovl_disp_fake_eng1",
			"ovl0_ck"/* parent */, 2),
	GATE_OVL(CLK_OVL_DISP_MUTEX0, "ovl_disp_mutex0",
			"ovl0_ck"/* parent */, 3),
	GATE_OVL(CLK_OVL_OVL0_2L, "ovl_ovl0_2l",
			"ovl0_ck"/* parent */, 4),
	GATE_OVL(CLK_OVL_OVL1_2L, "ovl_ovl1_2l",
			"ovl0_ck"/* parent */, 5),
	GATE_OVL(CLK_OVL_OVL2_2L, "ovl_ovl2_2l",
			"ovl0_ck"/* parent */, 6),
	GATE_OVL(CLK_OVL_OVL3_2L, "ovl_ovl3_2l",
			"ovl0_ck"/* parent */, 7),
	GATE_OVL(CLK_OVL_DISP_RSZ1, "ovl_disp_rsz1",
			"ovl0_ck"/* parent */, 8),
	GATE_OVL(CLK_OVL_MDP_RSZ0, "ovl_mdp_rsz0",
			"ovl0_ck"/* parent */, 9),
	GATE_OVL(CLK_OVL_DISP_WDMA0, "ovl_disp_wdma0",
			"ovl0_ck"/* parent */, 10),
	GATE_OVL(CLK_OVL_DISP_UFBC_WDMA0, "ovl_disp_ufbc_wdma0",
			"ovl0_ck"/* parent */, 11),
	GATE_OVL(CLK_OVL_DISP_WDMA2, "ovl_disp_wdma2",
			"ovl0_ck"/* parent */, 12),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC0, "ovl_disp_dli_async0",
			"ovl0_ck"/* parent */, 13),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC1, "ovl_disp_dli_async1",
			"ovl0_ck"/* parent */, 14),
	GATE_OVL(CLK_OVL_DISP_DLI_ASYNC2, "ovl_disp_dli_async2",
			"ovl0_ck"/* parent */, 15),
	GATE_OVL(CLK_OVL_DISP_DLO_ASYNC0, "ovl_disp_dlo_async0",
			"ovl0_ck"/* parent */, 16),
	GATE_OVL(CLK_OVL_DISP_DLO_ASYNC1, "ovl_disp_dlo_async1",
			"ovl0_ck"/* parent */, 17),
	GATE_OVL(CLK_OVL_DISP_DLO_ASYNC2, "ovl_disp_dlo_async2",
			"ovl0_ck"/* parent */, 18),
	GATE_OVL(CLK_OVL_DISP_DLO_ASYNC3, "ovl_disp_dlo_async3",
			"ovl0_ck"/* parent */, 19),
	GATE_OVL(CLK_OVL_DISP_DLO_ASYNC4, "ovl_disp_dlo_async4",
			"ovl0_ck"/* parent */, 20),
	GATE_OVL(CLK_OVL_DISP_DLO_ASYNC5, "ovl_disp_dlo_async5",
			"ovl0_ck"/* parent */, 21),
	GATE_OVL(CLK_OVL_DISP_DLO_ASYNC6, "ovl_disp_dlo_async6",
			"ovl0_ck"/* parent */, 22),
	GATE_OVL(CLK_OVL_INLINEROT, "ovl_inlinerot",
			"ovl0_ck"/* parent */, 23),
	GATE_OVL(CLK_OVL_SMI_SUB_COMMON0, "ovl_smi_sub_common0",
			"ovl0_ck"/* parent */, 24),
	GATE_OVL(CLK_OVL_DISP_Y2R0, "ovl_disp_y2r0",
			"ovl0_ck"/* parent */, 25),
	GATE_OVL(CLK_OVL_DISP_Y2R1, "ovl_disp_y2r1",
			"ovl0_ck"/* parent */, 26),
};

static const struct mtk_clk_desc ovl_mcd = {
	.clks = ovl_clks,
	.num_clks = CLK_OVL_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6985_mmsys[] = {
	{
		.compatible = "mediatek,mt6985-mmsys1",
		.data = &mm1_mcd,
	}, {
		.compatible = "mediatek,mt6985-mmsys0",
		.data = &mm_mcd,
	}, {
		.compatible = "mediatek,mt6985-mminfra_config",
		.data = &mminfra_config_mcd,
	}, {
		.compatible = "mediatek,mt6985-ovlsys1_config",
		.data = &ovl1_mcd,
	}, {
		.compatible = "mediatek,mt6985-ovlsys_config",
		.data = &ovl_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6985_mmsys_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6985_mmsys_drv = {
	.probe = clk_mt6985_mmsys_grp_probe,
	.driver = {
		.name = "clk-mt6985-mmsys",
		.of_match_table = of_match_clk_mt6985_mmsys,
	},
};

module_platform_driver(clk_mt6985_mmsys_drv);
MODULE_LICENSE("GPL");
