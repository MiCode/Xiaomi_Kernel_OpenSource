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

#include <dt-bindings/clock/mt6879-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs apu_acx_config_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_ACX_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu_acx_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate apu_acx_config_clks[] = {
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_AHB, "apu_acx_ahb",
			"dsp_ck"/* parent */, 1),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_AXI, "apu_acx_axi",
			"dsp_ck"/* parent */, 2),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_ISP, "apu_acx_isp",
			"dsp_ck"/* parent */, 3),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_CAM_ADL, "apu_acx_cam_adl",
			"dsp_ck"/* parent */, 4),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_IMG_ADL, "apu_acx_img_adl",
			"dsp_ck"/* parent */, 5),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_EMI_26M, "apu_acx_emi_26m",
			"dsp_ck"/* parent */, 6),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_VPU_UDI, "apu_acx_vpu_udi",
			"dsp_ck"/* parent */, 7),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_EDMA_0, "apu_acx_edma_0",
			"dsp_ck"/* parent */, 8),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_EDMA_1, "apu_acx_edma_1",
			"dsp_ck"/* parent */, 9),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_EDMAL_0, "apu_acx_edmal_0",
			"dsp_ck"/* parent */, 10),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_EDMAL_1, "apu_acx_edmal_1",
			"dsp_ck"/* parent */, 11),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_MNOC, "apu_acx_mnoc",
			"dsp_ck"/* parent */, 12),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_TCM, "apu_acx_tcm",
			"dsp_ck"/* parent */, 13),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_MD32, "apu_acx_md32",
			"dsp_ck"/* parent */, 14),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_IOMMU_0, "apu_acx_iommu_0",
			"dsp_ck"/* parent */, 15),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_IOMMU_1, "apu_acx_iommu_1",
			"dsp_ck"/* parent */, 16),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_MD32_32K, "apu_acx_md32_32k",
			"dsp_ck"/* parent */, 17),
	GATE_APU_ACX_CONFIG(CLK_APU_ACX_CPE, "apu_acx_cpe",
			"dsp_ck"/* parent */, 18),
};

static const struct mtk_clk_desc apu_acx_config_mcd = {
	.clks = apu_acx_config_clks,
	.num_clks = ARRAY_SIZE(apu_acx_config_clks),
};

static const struct mtk_gate_regs apu_dla_0_config_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_DLA_0_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu_dla_0_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate apu_dla_0_config_clks[] = {
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG0, "apu_dla_0_mdla_cg0",
			"dsp4_ck"/* parent */, 0),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG1, "apu_dla_0_mdla_cg1",
			"dsp4_ck"/* parent */, 1),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG2, "apu_dla_0_mdla_cg2",
			"dsp4_ck"/* parent */, 2),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG3, "apu_dla_0_mdla_cg3",
			"dsp4_ck"/* parent */, 3),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG4, "apu_dla_0_mdla_cg4",
			"dsp4_ck"/* parent */, 4),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG5, "apu_dla_0_mdla_cg5",
			"dsp4_ck"/* parent */, 5),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG6, "apu_dla_0_mdla_cg6",
			"dsp4_ck"/* parent */, 6),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG7, "apu_dla_0_mdla_cg7",
			"dsp4_ck"/* parent */, 7),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG8, "apu_dla_0_mdla_cg8",
			"dsp4_ck"/* parent */, 8),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG9, "apu_dla_0_mdla_cg9",
			"dsp4_ck"/* parent */, 9),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG10, "apu_dla_0_mdla_cg10",
			"dsp4_ck"/* parent */, 10),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG11, "apu_dla_0_mdla_cg11",
			"dsp4_ck"/* parent */, 11),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_MDLA_CG12, "apu_dla_0_mdla_cg12",
			"dsp4_ck"/* parent */, 12),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_APB, "apu_dla_0_apb",
			"dsp4_ck"/* parent */, 19),
	GATE_APU_DLA_0_CONFIG(CLK_APU_DLA_0_AXI_M, "apu_dla_0_axi_m",
			"dsp4_ck"/* parent */, 20),
};

static const struct mtk_clk_desc apu_dla_0_config_mcd = {
	.clks = apu_dla_0_config_clks,
	.num_clks = ARRAY_SIZE(apu_dla_0_config_clks),
};

static const struct mtk_gate_regs apu_rcx_config_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_RCX_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu_rcx_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate apu_rcx_config_clks[] = {
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_AXI, "apu_rcx_axi",
			"dsp_ck"/* parent */, 0),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_MNOC, "apu_rcx_mnoc",
			"dsp_ck"/* parent */, 1),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_UP, "apu_rcx_up",
			"dsp_ck"/* parent */, 2),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_IOMMU_0, "apu_rcx_iommu_0",
			"dsp_ck"/* parent */, 4),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_IOMMU_1, "apu_rcx_iommu_1",
			"dsp_ck"/* parent */, 5),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_CPE_26M, "apu_rcx_cpe_26m",
			"f26m_ck"/* parent */, 6),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_EMI_26M, "apu_rcx_emi_26m",
			"f26m_ck"/* parent */, 7),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_MD32, "apu_rcx_md32",
			"dsp_ck"/* parent */, 8),
	GATE_APU_RCX_CONFIG(CLK_APU_RCX_MD32_32K, "apu_rcx_md32_32k",
			"dsp_ck"/* parent */, 9),
};

static const struct mtk_clk_desc apu_rcx_config_mcd = {
	.clks = apu_rcx_config_clks,
	.num_clks = ARRAY_SIZE(apu_rcx_config_clks),
};

static const struct mtk_gate_regs apu_rcx_vcore_config_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_RCX_VCORE_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu_rcx_vcore_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate apu_rcx_vcore_config_clks[] = {
	GATE_APU_RCX_VCORE_CONFIG(CLK_APU_RCX_VCORE_AHB, "apu_rcx_vcore_ahb",
			"emi_n_ck"/* parent */, 0),
	GATE_APU_RCX_VCORE_CONFIG(CLK_APU_RCX_VCORE_AXI, "apu_rcx_vcore_axi",
			"emi_n_ck"/* parent */, 1),
	GATE_APU_RCX_VCORE_CONFIG(CLK_APU_RCX_VCORE_ADL, "apu_rcx_vcore_adl",
			"emi_n_ck"/* parent */, 2),
	GATE_APU_RCX_VCORE_CONFIG(CLK_APU_RCX_VCORE_QOS, "apu_rcx_vcore_qos",
			"emi_n_ck"/* parent */, 3),
};

static const struct mtk_clk_desc apu_rcx_vcore_config_mcd = {
	.clks = apu_rcx_vcore_config_clks,
	.num_clks = ARRAY_SIZE(apu_rcx_vcore_config_clks),
};

static const struct mtk_gate_regs mvpu0_top_config_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_MVPU0_TOP_CONFIG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &mvpu0_top_config_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate mvpu0_top_config_clks[] = {
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_AXI_M_CK_EN, "mvpu0_csr_axi_m",
			"dsp1_ck"/* parent */, 1),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_JTAG_CK_EN, "mvpu0_csr_jtag",
			"dsp1_ck"/* parent */, 2),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_AXI_S_CK_EN, "mvpu0_csr_axi_s",
			"dsp1_ck"/* parent */, 3),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_L1MC_CK_EN, "mvpu0_csr_mvpu_l1mc",
			"dsp1_ck"/* parent */, 4),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_LDS_CK_EN, "mvpu0_csr_mvpu_lds",
			"dsp1_ck"/* parent */, 5),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_VPU_CK_EN, "mvpu0_csr_mvpu_vpu",
			"dsp1_ck"/* parent */, 6),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_VCU_CK_EN, "mvpu0_csr_mvpu_vcu",
			"dsp1_ck"/* parent */, 7),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_GLSU_CK_EN, "mvpu0_csr_mvpu_glsu",
			"dsp1_ck"/* parent */, 8),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_DC_CK_EN, "mvpu0_csr_mvpu_dc",
			"dsp1_ck"/* parent */, 9),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_DPCH_CK_EN, "mvpu0_csr_mvpu_dpch",
			"dsp1_ck"/* parent */, 10),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_RV55_CK_EN, "mvpu0_csr_mvpu_rv55",
			"dsp1_ck"/* parent */, 11),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_SFU_CK_EN, "mvpu0_csr_mvpu_sfu",
			"dsp1_ck"/* parent */, 12),
	GATE_MVPU0_TOP_CONFIG(CLK_MVPU0_TOP_CSR_MVPU_INFRA_CK_EN, "mvpu0_csr_mvpu_infra",
			"dsp1_ck"/* parent */, 13),
};

static const struct mtk_clk_desc mvpu0_top_config_mcd = {
	.clks = mvpu0_top_config_clks,
	.num_clks = ARRAY_SIZE(mvpu0_top_config_clks),
};

static const struct of_device_id of_match_clk_mt6879_apu[] = {
	{
		.compatible = "mediatek,mt6879-apu_acx_config",
		.data = &apu_acx_config_mcd,
	}, {
		.compatible = "mediatek,mt6879-apu_dla_0_config",
		.data = &apu_dla_0_config_mcd,
	}, {
		.compatible = "mediatek,mt6879-apu_rcx_config",
		.data = &apu_rcx_config_mcd,
	}, {
		.compatible = "mediatek,mt6879-apu_rcx_vcore_config",
		.data = &apu_rcx_vcore_config_mcd,
	}, {
		.compatible = "mediatek,mt6879-mvpu0_top_config",
		.data = &mvpu0_top_config_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6879_apu_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6879_apu_drv = {
	.probe = clk_mt6879_apu_grp_probe,
	.driver = {
		.name = "clk-mt6879-apu",
		.of_match_table = of_match_clk_mt6879_apu,
	},
};

static int __init clk_mt6879_apu_init(void)
{
	return platform_driver_register(&clk_mt6879_apu_drv);
}

static void __exit clk_mt6879_apu_exit(void)
{
	platform_driver_unregister(&clk_mt6879_apu_drv);
}

arch_initcall(clk_mt6879_apu_init);
module_exit(clk_mt6879_apu_exit);
MODULE_LICENSE("GPL");
