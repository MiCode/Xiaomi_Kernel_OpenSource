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

static const struct mtk_gate_regs mminfra_config_0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mminfra_config_1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs mminfra_config_0_hwv_regs = {
	.set_ofs = 0xB0,
	.clr_ofs = 0xB4,
	.sta_ofs = 0x1C58,
};

static const struct mtk_gate_regs mminfra_config_1_hwv_regs = {
	.set_ofs = 0xC0,
	.clr_ofs = 0xC4,
	.sta_ofs = 0x1C60,
};

#define GATE_MMINFRA_CONFIG_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_MMINFRA_CONFIG_0_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}
#define GATE_MMINFRA_CONFIG_1(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config_1_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_HWV_MMINFRA_CONFIG_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config_0_cg_regs,			\
		.hwv_regs = &mminfra_config_0_hwv_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
#define GATE_HWV_MMINFRA_CONFIG_1(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config_1_cg_regs,			\
		.hwv_regs = &mminfra_config_1_hwv_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
#define GATE_HWV_MMINFRA_CONFIG_0_DUMMY(_id, _name, _parent, _shift) {	\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &mminfra_config_0_cg_regs,			\
		.hwv_regs = &mminfra_config_0_hwv_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv_dummy,		\
		.flags = CLK_USE_HW_VOTER,				\
	}

static struct mtk_gate mminfra_config_clks[] = {
	GATE_MMINFRA_CONFIG_0(CLK_MMINFRA_CONFIG_GCE_D /* CLK ID */,
		"mminfra_config_gce_d" /* name */,
		"fmminfra_ck" /* parent */, 0 /* bit */),
	GATE_MMINFRA_CONFIG_0(CLK_MMINFRA_CONFIG_GCE_M /* CLK ID */,
		"mminfra_config_gce_m" /* name */,
		"fmminfra_ck" /* parent */, 1 /* bit */),
	GATE_MMINFRA_CONFIG_0_DUMMY(CLK_MMINFRA_CONFIG_SMI /* CLK ID */,
		"mminfra_config__smi" /* name */,
		"fmminfra_ck" /* parent */, 2 /* bit */),
	GATE_MMINFRA_CONFIG_1(CLK_MMINFRA_CONFIG_GCE_26M /* CLK ID */,
		"mminfra_config_gce_26m" /* name */,
		"fmminfra_ck" /* parent */, 17 /* bit */),
};

static int clk_mt6983_mminfra_config_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_MMINFRA_CONFIG_NR_CLK);

	mtk_clk_register_gates(node, mminfra_config_clks,
		ARRAY_SIZE(mminfra_config_clks),
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

static const struct mtk_gate_regs dispsys_config_0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs dispsys_config_1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs dispsys_config_2_cg_regs = {
	.set_ofs = 0x1A4,
	.clr_ofs = 0x1A8,
	.sta_ofs = 0x1A0,
};

static const struct mtk_gate_regs dispsys_config_hwv_regs = {
	.set_ofs = 0x90,
	.clr_ofs = 0x94,
	.sta_ofs = 0x1C48,
};

#define GATE_DISPSYS_CONFIG_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys_config_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_DISPSYS_CONFIG_1(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys_config_1_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_DISPSYS_CONFIG_1_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys_config_1_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}
#define GATE_DISPSYS_CONFIG_2(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys_config_2_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_HWV_DISPSYS_CONFIG(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys_config_1_cg_regs,			\
		.hwv_regs = &dispsys_config_hwv_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
static struct mtk_gate dispsys_config_clks[] = {
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_MUTEX /* CLK ID */,
		"dispsys_config_disp_mutex" /* name */,
		"disp0_ck" /* parent */, 0 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_OVL0 /* CLK ID */,
		"dispsys_config_disp_ovl0" /* name */,
		"disp0_ck" /* parent */, 1 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_MERGE0 /* CLK ID */,
		"dispsys_config_disp_merge0" /* name */,
		"disp0_ck" /* parent */, 2 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_FAKE_ENG0 /* CLK ID */,
		"dispsys_config_disp_fake_eng0" /* name */,
		"disp0_ck" /* parent */, 3 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_INLINEROT /* CLK ID */,
		"dispsys_config_inlinerot" /* name */,
		"disp0_ck" /* parent */, 4 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_WDMA0 /* CLK ID */,
		"dispsys_config_disp_wdma0" /* name */,
		"disp0_ck" /* parent */, 5 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_FAKE_ENG1 /* CLK ID */,
		"dispsys_config_disp_fake_eng1" /* name */,
		"disp0_ck" /* parent */, 6 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DPI0 /* CLK ID */,
		"dispsys_config_disp_dpi0" /* name */,
		"disp0_ck" /* parent */, 7 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_OVL0_2L_NWCG /* CLK ID */,
		"dispsys_config_disp_ovl0_2l_nwcg" /* name */,
		"disp0_ck" /* parent */, 8 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_RDMA0 /* CLK ID */,
		"dispsys_config_disp_rdma0" /* name */,
		"disp0_ck" /* parent */, 9 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_RDMA1 /* CLK ID */,
		"dispsys_config_disp_rdma1" /* name */,
		"disp0_ck" /* parent */, 10 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DLI_ASYNC0 /* CLK ID */,
		"dispsys_config_disp_dli_async0" /* name */,
		"disp0_ck" /* parent */, 11 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DLI_ASYNC1 /* CLK ID */,
		"dispsys_config_disp_dli_async1" /* name */,
		"disp0_ck" /* parent */, 12 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DLI_ASYNC2 /* CLK ID */,
		"dispsys_config_disp_dli_async2" /* name */,
		"disp0_ck" /* parent */, 13 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DLO_ASYNC0 /* CLK ID */,
		"dispsys_config_disp_dlo_async0" /* name */,
		"disp0_ck" /* parent */, 14 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DLO_ASYNC1 /* CLK ID */,
		"dispsys_config_disp_dlo_async1" /* name */,
		"disp0_ck" /* parent */, 15 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DLO_ASYNC2 /* CLK ID */,
		"dispsys_config_disp_dlo_async2" /* name */,
		"disp0_ck" /* parent */, 16 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_RSZ0 /* CLK ID */,
		"dispsys_config_disp_rsz0" /* name */,
		"disp0_ck" /* parent */, 17 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_COLOR0 /* CLK ID */,
		"dispsys_config_disp_color0" /* name */,
		"disp0_ck" /* parent */, 18 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_CCORR0 /* CLK ID */,
		"dispsys_config_disp_ccorr0" /* name */,
		"disp0_ck" /* parent */, 19 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_CCORR1 /* CLK ID */,
		"dispsys_config_disp_ccorr1" /* name */,
		"disp0_ck" /* parent */, 20 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_AAL0 /* CLK ID */,
		"dispsys_config_disp_aal0" /* name */,
		"disp0_ck" /* parent */, 21 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_GAMMA0 /* CLK ID */,
		"dispsys_config_disp_gamma0" /* name */,
		"disp0_ck" /* parent */, 22 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_POSTMASK0 /* CLK ID */,
		"dispsys_config_disp_postmask0" /* name */,
		"disp0_ck" /* parent */, 23 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DITHER0 /* CLK ID */,
		"dispsys_config_disp_dither0" /* name */,
		"disp0_ck" /* parent */, 24 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_CM0 /* CLK ID */,
		"dispsys_config_disp_cm0" /* name */,
		"disp0_ck" /* parent */, 25 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_SPR0 /* CLK ID */,
		"dispsys_config_disp_spr0" /* name */,
		"disp0_ck" /* parent */, 26 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DSC_WRAP0 /* CLK ID */,
		"dispsys_config_disp_dsc_wrap0" /* name */,
		"disp0_ck" /* parent */, 27 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_DSI0 /* CLK ID */,
		"dispsys_config_disp_dsi0" /* name */,
		"disp0_ck" /* parent */, 29 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_UFBC_WDMA0 /* CLK ID */,
		"dispsys_config_disp_ufbc_wdma0" /* name */,
		"disp0_ck" /* parent */, 30 /* bit */),
	GATE_DISPSYS_CONFIG_0(CLK_DISPSYS_CONFIG_DISP_WDMA1 /* CLK ID */,
		"dispsys_config_disp_wdma1" /* name */,
		"disp0_ck" /* parent */, 31 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_DP_INTF0 /* CLK ID */,
		"dispsys_config_disp_dp_intf0" /* name */,
		"disp0_ck" /* parent */, 0 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_APB_BUS /* CLK ID */,
		"dispsys_config_apb_bus" /* name */,
		"disp0_ck" /* parent */, 1 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_TDSHP0 /* CLK ID */,
		"dispsys_config_disp_tdshp0" /* name */,
		"disp0_ck" /* parent */, 2 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_C3D0 /* CLK ID */,
		"dispsys_config_disp_c3d0" /* name */,
		"disp0_ck" /* parent */, 3 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_Y2R0 /* CLK ID */,
		"dispsys_config_disp_y2r0" /* name */,
		"disp0_ck" /* parent */, 4 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_MDP_AAL0 /* CLK ID */,
		"dispsys_config_disp_mdp_aal0" /* name */,
		"disp0_ck" /* parent */, 5 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_CHIST0 /* CLK ID */,
		"dispsys_config_disp_chist0" /* name */,
		"disp0_ck" /* parent */, 6 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_CHIST1 /* CLK ID */,
		"dispsys_config_disp_chist1" /* name */,
		"disp0_ck" /* parent */, 7 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_OVL0_2L /* CLK ID */,
		"dispsys_config_disp_ovl0_2l" /* name */,
		"disp0_ck" /* parent */, 8 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_DLI_ASYNC3 /* CLK ID */,
		"dispsys_config_disp_dli_async3" /* name */,
		"disp0_ck" /* parent */, 9 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_DLO_ASYNC3 /* CLK ID */,
		"dispsys_config_disp_dlo_async3" /* name */,
		"disp0_ck" /* parent */, 10 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_OVL1_2L /* CLK ID */,
		"dispsys_config_disp_ovl1_2l" /* name */,
		"disp0_ck" /* parent */, 12 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_DISP_OVL1_2L_NWCG /* CLK ID */,
		"dispsys_config_disp_ovl1_2l_nwcg" /* name */,
		"disp0_ck" /* parent */, 16 /* bit */),
	GATE_DISPSYS_CONFIG_1(CLK_DISPSYS_CONFIG_SMI_SUB_COMM /* CLK ID */,
		"dispsys_config_smi_sub_comm" /* name */,
		"disp0_ck" /* parent */, 20 /* bit */),
	GATE_DISPSYS_CONFIG_2(CLK_DISPSYS_CONFIG_DSI_CLK /* CLK ID */,
		"dispsys_config_dsi_clk" /* name */,
		"disp0_ck" /* parent */, 0 /* bit */),
	GATE_DISPSYS_CONFIG_2(CLK_DISPSYS_CONFIG_DP_CLK /* CLK ID */,
		"dispsys_config_dp_clk" /* name */,
		"disp0_ck" /* parent */, 1 /* bit */),
	GATE_DISPSYS_CONFIG_2(CLK_DISPSYS_CONFIG_26M_CLK /* CLK ID */,
		"dispsys_config_26m_clk" /* name */,
		"disp0_ck" /* parent */, 11 /* bit */),
};

static int clk_mt6983_dispsys_config_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_DISPSYS_CONFIG_NR_CLK);

	mtk_clk_register_gates(node, dispsys_config_clks,
		ARRAY_SIZE(dispsys_config_clks),
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

static const struct mtk_gate_regs dispsys1_config_0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs dispsys1_config_1_cg_regs = {
	.set_ofs = 0x114,
	.clr_ofs = 0x118,
	.sta_ofs = 0x110,
};

static const struct mtk_gate_regs dispsys1_config_2_cg_regs = {
	.set_ofs = 0x1A4,
	.clr_ofs = 0x1A8,
	.sta_ofs = 0x1A0,
};

static const struct mtk_gate_regs dispsys1_config_hwv_regs = {
	.set_ofs = 0xA0,
	.clr_ofs = 0xA4,
	.sta_ofs = 0x1C50,
};

#define GATE_DISPSYS1_CONFIG_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_DISPSYS1_CONFIG_1(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config_1_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_DISPSYS1_CONFIG_1_DUMMY(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config_1_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummy,			\
	}
#define GATE_DISPSYS1_CONFIG_2(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config_2_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
#define GATE_HWV_DISPSYS1_CONFIG(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dispsys1_config_1_cg_regs,			\
		.hwv_regs = &dispsys1_config_hwv_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
static struct mtk_gate dispsys1_config_clks[] = {
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_MUTEX /* CLK ID */,
		"dispsys1_config_disp_mutex" /* name */,
		"disp1_ck" /* parent */, 0 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_OVL0 /* CLK ID */,
		"dispsys1_config_disp_ovl0" /* name */,
		"disp1_ck" /* parent */, 1 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_MERGE0 /* CLK ID */,
		"dispsys1_config_disp_merge0" /* name */,
		"disp1_ck" /* parent */, 2 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_FAKE_ENG0 /* CLK ID */,
		"dispsys1_config_disp_fake_eng0" /* name */,
		"disp1_ck" /* parent */, 3 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_INLINEROT /* CLK ID */,
		"dispsys1_config_inlinerot" /* name */,
		"disp1_ck" /* parent */, 4 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_WDMA0 /* CLK ID */,
		"dispsys1_config_disp_wdma0" /* name */,
		"disp1_ck" /* parent */, 5 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_FAKE_ENG1 /* CLK ID */,
		"dispsys1_config_disp_fake_eng1" /* name */,
		"disp1_ck" /* parent */, 6 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DPI0 /* CLK ID */,
		"dispsys1_config_disp_dpi0" /* name */,
		"disp1_ck" /* parent */, 7 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_OVL0_2L_NWCG /* CLK ID */,
		"dispsys1_config_disp_ovl0_2l_nwcg" /* name */,
		"disp1_ck" /* parent */, 8 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_RDMA0 /* CLK ID */,
		"dispsys1_config_disp_rdma0" /* name */,
		"disp1_ck" /* parent */, 9 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_RDMA1 /* CLK ID */,
		"dispsys1_config_disp_rdma1" /* name */,
		"disp1_ck" /* parent */, 10 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DLI_ASYNC0 /* CLK ID */,
		"dispsys1_config_disp_dli_async0" /* name */,
		"disp1_ck" /* parent */, 11 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DLI_ASYNC1 /* CLK ID */,
		"dispsys1_config_disp_dli_async1" /* name */,
		"disp1_ck" /* parent */, 12 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DLI_ASYNC2 /* CLK ID */,
		"dispsys1_config_disp_dli_async2" /* name */,
		"disp1_ck" /* parent */, 13 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DLO_ASYNC0 /* CLK ID */,
		"dispsys1_config_disp_dlo_async0" /* name */,
		"disp1_ck" /* parent */, 14 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DLO_ASYNC1 /* CLK ID */,
		"dispsys1_config_disp_dlo_async1" /* name */,
		"disp1_ck" /* parent */, 15 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DLO_ASYNC2 /* CLK ID */,
		"dispsys1_config_disp_dlo_async2" /* name */,
		"disp1_ck" /* parent */, 16 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_RSZ0 /* CLK ID */,
		"dispsys1_config_disp_rsz0" /* name */,
		"disp1_ck" /* parent */, 17 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_COLOR0 /* CLK ID */,
		"dispsys1_config_disp_color0" /* name */,
		"disp1_ck" /* parent */, 18 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_CCORR0 /* CLK ID */,
		"dispsys1_config_disp_ccorr0" /* name */,
		"disp1_ck" /* parent */, 19 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_CCORR1 /* CLK ID */,
		"dispsys1_config_disp_ccorr1" /* name */,
		"disp1_ck" /* parent */, 20 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_AAL0 /* CLK ID */,
		"dispsys1_config_disp_aal0" /* name */,
		"disp1_ck" /* parent */, 21 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_GAMMA0 /* CLK ID */,
		"dispsys1_config_disp_gamma0" /* name */,
		"disp1_ck" /* parent */, 22 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_POSTMASK0 /* CLK ID */,
		"dispsys1_config_disp_postmask0" /* name */,
		"disp1_ck" /* parent */, 23 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DITHER0 /* CLK ID */,
		"dispsys1_config_disp_dither0" /* name */,
		"disp1_ck" /* parent */, 24 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_CM0 /* CLK ID */,
		"dispsys1_config_disp_cm0" /* name */,
		"disp1_ck" /* parent */, 25 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_SPR0 /* CLK ID */,
		"dispsys1_config_disp_spr0" /* name */,
		"disp1_ck" /* parent */, 26 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DSC_WRAP0 /* CLK ID */,
		"dispsys1_config_disp_dsc_wrap0" /* name */,
		"disp1_ck" /* parent */, 27 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_DSI0 /* CLK ID */,
		"dispsys1_config_disp_dsi0" /* name */,
		"disp1_ck" /* parent */, 29 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_UFBC_WDMA0 /* CLK ID */,
		"dispsys1_config_disp_ufbc_wdma0" /* name */,
		"disp1_ck" /* parent */, 30 /* bit */),
	GATE_DISPSYS1_CONFIG_0(CLK_DISPSYS1_CONFIG_DISP_WDMA1 /* CLK ID */,
		"dispsys1_config_disp_wdma1" /* name */,
		"disp1_ck" /* parent */, 31 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_DP_INTF0 /* CLK ID */,
		"dispsys1_config_disp_dp_intf0" /* name */,
		"disp1_ck" /* parent */, 0 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_APB_BUS /* CLK ID */,
		"dispsys1_config_apb_bus" /* name */,
		"disp1_ck" /* parent */, 1 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_TDSHP0 /* CLK ID */,
		"dispsys1_config_disp_tdshp0" /* name */,
		"disp1_ck" /* parent */, 2 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_C3D0 /* CLK ID */,
		"dispsys1_config_disp_c3d0" /* name */,
		"disp1_ck" /* parent */, 3 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_Y2R0 /* CLK ID */,
		"dispsys1_config_disp_y2r0" /* name */,
		"disp1_ck" /* parent */, 4 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_MDP_AAL0 /* CLK ID */,
		"dispsys1_config_disp_mdp_aal0" /* name */,
		"disp1_ck" /* parent */, 5 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_CHIST0 /* CLK ID */,
		"dispsys1_config_disp_chist0" /* name */,
		"disp1_ck" /* parent */, 6 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_CHIST1 /* CLK ID */,
		"dispsys1_config_disp_chist1" /* name */,
		"disp1_ck" /* parent */, 7 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_OVL0_2L /* CLK ID */,
		"dispsys1_config_disp_ovl0_2l" /* name */,
		"disp1_ck" /* parent */, 8 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_DLI_ASYNC3 /* CLK ID */,
		"dispsys1_config_disp_dli_async3" /* name */,
		"disp1_ck" /* parent */, 9 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_DLO_ASYNC3 /* CLK ID */,
		"dispsys1_config_disp_dlo_async3" /* name */,
		"disp1_ck" /* parent */, 10 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_OVL1_2L /* CLK ID */,
		"dispsys1_config_disp_ovl1_2l" /* name */,
		"disp1_ck" /* parent */, 12 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_DISP_OVL1_2L_NWCG /* CLK ID */,
		"dispsys1_config_disp_ovl1_2l_nwcg" /* name */,
		"disp1_ck" /* parent */, 16 /* bit */),
	GATE_DISPSYS1_CONFIG_1(CLK_DISPSYS1_CONFIG_SMI_SUB_COMM /* CLK ID */,
		"dispsys1_config_smi_sub_comm" /* name */,
		"disp1_ck" /* parent */, 20 /* bit */),
	GATE_DISPSYS1_CONFIG_2(CLK_DISPSYS1_CONFIG_DSI_CLK /* CLK ID */,
		"dispsys1_config_dsi_clk" /* name */,
		"disp1_ck" /* parent */, 0 /* bit */),
	GATE_DISPSYS1_CONFIG_2(CLK_DISPSYS1_CONFIG_DP_CLK /* CLK ID */,
		"dispsys1_config_dp_clk" /* name */,
		"disp1_ck" /* parent */, 1 /* bit */),
	GATE_DISPSYS1_CONFIG_2(CLK_DISPSYS1_CONFIG_26M_CLK /* CLK ID */,
		"dispsys1_config_26m_clk" /* name */,
		"disp1_ck" /* parent */, 11 /* bit */),
};

static int clk_mt6983_dispsys1_config_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_DISPSYS1_CONFIG_NR_CLK);

	mtk_clk_register_gates(node, dispsys1_config_clks,
		ARRAY_SIZE(dispsys1_config_clks),
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

static const struct of_device_id of_match_clk_mt6983_mm[] = {
	{
		.compatible = "mediatek,mt6983-mminfra_config",
		.data = clk_mt6983_mminfra_config_probe,
	}, {
		.compatible = "mediatek,mt6983-dispsys_config",
		.data = clk_mt6983_dispsys_config_probe,
	}, {
		.compatible = "mediatek,mt6983-dispsys1_config",
		.data = clk_mt6983_dispsys1_config_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6983_mm_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6983_mm_drv = {
	.probe = clk_mt6983_mm_probe,
	.driver = {
		.name = "clk-mt6983-mm",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_mm,
	},
};

static int __init clk_mt6983_mm_init(void)
{
	return platform_driver_register(&clk_mt6983_mm_drv);
}

static void __exit clk_mt6983_mm_exit(void)
{
	platform_driver_unregister(&clk_mt6983_mm_drv);
}

arch_initcall(clk_mt6983_mm_init);
module_exit(clk_mt6983_mm_exit);
MODULE_LICENSE("GPL");

