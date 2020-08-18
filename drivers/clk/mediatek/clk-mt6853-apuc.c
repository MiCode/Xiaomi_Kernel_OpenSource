/*
 * Copyright (c) 2020 MediaTek Inc.
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

#include <dt-bindings/clock/mt6853-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(INV_OFS,
		INV_OFS, 0x0178, BIT(5), BIT(5));

static const struct mtk_gate_regs apuc_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APUC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apuc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate apuc_clks[] = {
	GATE_APUC(CLK_APUC_APU, "apuc_apu",
			"dsp_ck"/* parent */, 0),
	GATE_APUC(CLK_APUC_AHB, "apuc_ahb",
			"dsp_ck"/* parent */, 1),
	GATE_APUC(CLK_APUC_AXI, "apuc_axi",
			"dsp_ck"/* parent */, 2),
	GATE_APUC(CLK_APUC_ISP, "apuc_isp",
			"dsp_ck"/* parent */, 3),
	GATE_APUC(CLK_APUC_CAM_ADL, "apuc_cam_adl",
			"dsp_ck"/* parent */, 4),
	GATE_APUC(CLK_APUC_IMG_ADL, "apuc_img_adl",
			"dsp_ck"/* parent */, 5),
	GATE_APUC(CLK_APUC_EMI_26M, "apuc_emi_26m",
			"dsp_ck"/* parent */, 6),
	GATE_APUC(CLK_APUC_VPU_UDI, "apuc_vpu_udi",
			"dsp_ck"/* parent */, 7),
	GATE_APUC(CLK_APUC_EDMA_0, "apuc_edma_0",
			"dsp_ck"/* parent */, 8),
	GATE_APUC(CLK_APUC_EDMA_1, "apuc_edma_1",
			"dsp_ck"/* parent */, 9),
	GATE_APUC(CLK_APUC_EDMAL_0, "apuc_edmal_0",
			"dsp_ck"/* parent */, 10),
	GATE_APUC(CLK_APUC_EDMAL_1, "apuc_edmal_1",
			"dsp_ck"/* parent */, 11),
	GATE_APUC(CLK_APUC_MNOC, "apuc_mnoc",
			"dsp_ck"/* parent */, 12),
	GATE_APUC(CLK_APUC_TCM, "apuc_tcm",
			"dsp_ck"/* parent */, 13),
	GATE_APUC(CLK_APUC_MD32, "apuc_md32",
			"dsp_ck"/* parent */, 14),
	GATE_APUC(CLK_APUC_IOMMU_0, "apuc_iommu_0",
			"dsp_ck"/* parent */, 15),
	GATE_APUC(CLK_APUC_MD32_32K, "apuc_md32_32k",
			"dsp_ck"/* parent */, 17),
};

static int clk_mt6853_apuc_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APUC_NR_CLK);

	mtk_clk_register_gates(node, apuc_clks, ARRAY_SIZE(apuc_clks),
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

static const struct of_device_id of_match_clk_mt6853_apuc[] = {
	{ .compatible = "mediatek,mt6853-apu_conn", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_apuc_drv = {
	.probe = clk_mt6853_apuc_probe,
	.driver = {
		.name = "clk-mt6853-apuc",
		.of_match_table = of_match_clk_mt6853_apuc,
	},
};

builtin_platform_driver(clk_mt6853_apuc_drv);

#else

static struct platform_driver clk_mt6853_apuc_drv = {
	.probe = clk_mt6853_apuc_probe,
	.driver = {
		.name = "clk-mt6853-apuc",
		.of_match_table = of_match_clk_mt6853_apuc,
	},
};
static int __init clk_mt6853_apuc_platform_init(void)
{
	return platform_driver_register(&clk_mt6853_apuc_drv);
}
arch_initcall(clk_mt6853_apuc_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
