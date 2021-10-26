/*
 * Copyright (c) 2021 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6877-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status mm_pwr_stat = GATE_PWR_STAT(0xEF0,
		0xEF4, INV_OFS, BIT(18), BIT(18));

static const struct mtk_gate_regs mm0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs mm1_cg_regs = {
	.set_ofs = 0x1a4,
	.clr_ofs = 0x1a8,
	.sta_ofs = 0x1a0,
};

#define GATE_MM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &mm_pwr_stat,			\
	}

#define GATE_MM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mm1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &mm_pwr_stat,			\
	}

static const struct mtk_gate mm_clks[] = {
	/* MM0 */
	GATE_MM0(CLK_MM_DISP_MUTEX0, "mm_disp_mutex0",
			"disp0_ck"/* parent */, 0),
	GATE_MM0(CLK_MM_APB_BUS, "mm_apb_bus",
			"disp0_ck"/* parent */, 1),
	GATE_MM0(CLK_MM_DISP_OVL0, "mm_disp_ovl0",
			"disp0_ck"/* parent */, 2),
	GATE_MM0(CLK_MM_DISP_RDMA0, "mm_disp_rdma0",
			"disp0_ck"/* parent */, 3),
	GATE_MM0(CLK_MM_DISP_OVL0_2L, "mm_disp_ovl0_2l",
			"disp0_ck"/* parent */, 4),
	GATE_MM0(CLK_MM_DISP_WDMA0, "mm_disp_wdma0",
			"disp0_ck"/* parent */, 5),
	GATE_MM0(CLK_MM_DISP_CCORR1, "mm_disp_ccorr1",
			"disp0_ck"/* parent */, 6),
	GATE_MM0(CLK_MM_DISP_RSZ0, "mm_disp_rsz0",
			"disp0_ck"/* parent */, 7),
	GATE_MM0(CLK_MM_DISP_AAL0, "mm_disp_aal0",
			"disp0_ck"/* parent */, 8),
	GATE_MM0(CLK_MM_DISP_CCORR0, "mm_disp_ccorr0",
			"disp0_ck"/* parent */, 9),
	GATE_MM0(CLK_MM_DISP_COLOR0, "mm_disp_color0",
			"disp0_ck"/* parent */, 10),
	GATE_MM0(CLK_MM_SMI_INFRA, "mm_smi_infra",
			"disp0_ck"/* parent */, 11),
	GATE_MM0(CLK_MM_DISP_GAMMA0, "mm_disp_gamma0",
			"disp0_ck"/* parent */, 13),
	GATE_MM0(CLK_MM_DISP_POSTMASK0, "mm_disp_postmask0",
			"disp0_ck"/* parent */, 14),
	GATE_MM0(CLK_MM_DISP_SPR0, "mm_disp_spr0",
			"disp0_ck"/* parent */, 15),
	GATE_MM0(CLK_MM_DISP_DITHER0, "mm_disp_dither0",
			"disp0_ck"/* parent */, 16),
	GATE_MM0(CLK_MM_SMI_COMMON, "mm_smi_common",
			"disp0_ck"/* parent */, 17),
	GATE_MM0(CLK_MM_DISP_CM0, "mm_disp_cm0",
			"disp0_ck"/* parent */, 18),
	GATE_MM0(CLK_MM_DSI0, "mm_dsi0",
			"disp0_ck"/* parent */, 19),
	GATE_MM0(CLK_MM_SMI_GALS, "mm_smi_gals",
			"disp0_ck"/* parent */, 22),
	GATE_MM0(CLK_MM_DISP_DSC_WRAP, "mm_disp_dsc_wrap",
			"disp0_ck"/* parent */, 23),
	GATE_MM0(CLK_MM_SMI_IOMMU, "mm_smi_iommu",
			"disp0_ck"/* parent */, 24),
	GATE_MM0(CLK_MM_DISP_OVL1_2L, "mm_disp_ovl1_2l",
			"disp0_ck"/* parent */, 25),
	GATE_MM0(CLK_MM_DISP_UFBC_WDMA0, "mm_disp_ufbc_wdma0",
			"disp0_ck"/* parent */, 26),
	/* MM1 */
	GATE_MM1(CLK_MM_DSI0_DSI_CK_DOMAIN, "mm_dsi0_dsi_domain",
			"disp0_ck"/* parent */, 0),
	GATE_MM1(CLK_MM_DISP_26M, "mm_disp_26m_ck",
			"disp0_ck"/* parent */, 10),
};

static int clk_mt6877_mm_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_MM_NR_CLK);

	mtk_clk_register_gates(node, mm_clks, ARRAY_SIZE(mm_clks),
			clk_data);

	r = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6877_mm[] = {
	{ .compatible = "mediatek,mt6877-dispsys", },
	{}
};

static struct platform_driver clk_mt6877_mm_drv = {
	.probe = clk_mt6877_mm_probe,
	.driver = {
		.name = "clk-mt6877-mm",
		.of_match_table = of_match_clk_mt6877_mm,
	},
};

static int __init clk_mt6877_mm_init(void)
{
	return platform_driver_register(&clk_mt6877_mm_drv);
}
arch_initcall(clk_mt6877_mm_init);

