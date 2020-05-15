// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Weiyi Lu <weiyi.lu@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt8192-clk.h>

static const struct mtk_gate_regs apu_conn_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_CONN(_id, _name, _parent, _shift)			\
	GATE_MTK(_id, _name, _parent, &apu_conn_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate apu_conn_clks[] = {
	GATE_APU_CONN(CLK_APU_CONN_APU, "apu_conn_apu", "dsp_sel", 0),
	GATE_APU_CONN(CLK_APU_CONN_AHB, "apu_conn_ahb", "dsp_sel", 1),
	GATE_APU_CONN(CLK_APU_CONN_AXI, "apu_conn_axi", "dsp_sel", 2),
	GATE_APU_CONN(CLK_APU_CONN_ISP, "apu_conn_isp", "dsp_sel", 3),
	GATE_APU_CONN(CLK_APU_CONN_CAM_ADL, "apu_conn_cam_adl", "dsp_sel", 4),
	GATE_APU_CONN(CLK_APU_CONN_IMG_ADL, "apu_conn_img_adl", "dsp_sel", 5),
	GATE_APU_CONN(CLK_APU_CONN_EMI_26M, "apu_conn_emi_26m", "dsp_sel", 6),
	GATE_APU_CONN(CLK_APU_CONN_VPU_UDI, "apu_conn_vpu_udi", "dsp_sel", 7),
	GATE_APU_CONN(CLK_APU_CONN_EDMA_0, "apu_conn_edma_0", "dsp_sel", 8),
	GATE_APU_CONN(CLK_APU_CONN_EDMA_1, "apu_conn_edma_1", "dsp_sel", 9),
	GATE_APU_CONN(CLK_APU_CONN_EDMAL_0, "apu_conn_edmal_0", "dsp_sel", 10),
	GATE_APU_CONN(CLK_APU_CONN_EDMAL_1, "apu_conn_edmal_1", "dsp_sel", 11),
	GATE_APU_CONN(CLK_APU_CONN_MNOC, "apu_conn_mnoc", "dsp_sel", 12),
	GATE_APU_CONN(CLK_APU_CONN_TCM, "apu_conn_tcm", "dsp_sel", 13),
	GATE_APU_CONN(CLK_APU_CONN_MD32, "apu_conn_md32", "dsp_sel", 14),
	GATE_APU_CONN(CLK_APU_CONN_IOMMU_0, "apu_conn_iommu_0", "dsp_sel", 15),
	GATE_APU_CONN(CLK_APU_CONN_IOMMU_1, "apu_conn_iommu_1", "dsp_sel", 16),
	GATE_APU_CONN(CLK_APU_CONN_MD32_32K, "apu_conn_md32_32k",
		"dsp_sel", 17),
};

static int clk_mt8192_apu_conn_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_APU_CONN_NR_CLK);

	mtk_clk_register_gates(node, apu_conn_clks, ARRAY_SIZE(apu_conn_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_apu_conn[] = {
	{ .compatible = "mediatek,mt8192-apu_conn", },
	{}
};

static struct platform_driver clk_mt8192_apu_conn_drv = {
	.probe = clk_mt8192_apu_conn_probe,
	.driver = {
		.name = "clk-mt8192-apu_conn",
		.of_match_table = of_match_clk_mt8192_apu_conn,
	},
};

static int __init clk_mt8192_apu_conn_init(void)
{
	return platform_driver_register(&clk_mt8192_apu_conn_drv);
}

static void __exit clk_mt8192_apu_conn_exit(void)
{
	platform_driver_unregister(&clk_mt8192_apu_conn_drv);
}

arch_initcall(clk_mt8192_apu_conn_init);
module_exit(clk_mt8192_apu_conn_exit);
MODULE_LICENSE("GPL");
