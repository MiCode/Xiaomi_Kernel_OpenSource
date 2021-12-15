// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs apu0_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_APU0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY_0(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &apu0_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate apu0_clks[] = {
	GATE_DUMMY_0(CLK_APU0_APU, "apu0_apu",
			"dsp1_ck"/* parent */, 0),
	GATE_DUMMY_0(CLK_APU0_AXI_M, "apu0_axi_m",
			"dsp1_ck"/* parent */, 1),
	GATE_DUMMY_0(CLK_APU0_JTAG, "apu0_jtag",
			"dsp1_ck"/* parent */, 2),
};

static const struct mtk_clk_desc apu0_mcd = {
	.clks = apu0_clks,
	.num_clks = CLK_APU0_NR_CLK,
};

static const struct mtk_gate_regs apu1_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_APU1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY_1(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &apu1_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}
	
static const struct mtk_gate apu1_clks[] = {
	GATE_DUMMY_1(CLK_APU1_APU, "apu1_apu",
			"dsp2_ck"/* parent */, 0),
	GATE_DUMMY_1(CLK_APU1_AXI_M, "apu1_axi_m",
			"dsp2_ck"/* parent */, 1),
	GATE_DUMMY_1(CLK_APU1_JTAG, "apu1_jtag",
			"dsp2_ck"/* parent */, 2),
};

static const struct mtk_clk_desc apu1_mcd = {
	.clks = apu1_clks,
	.num_clks = CLK_APU1_NR_CLK,
};

static const struct mtk_gate_regs apu2_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

#define GATE_APU2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY_2(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &apu2_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate apu2_clks[] = {
	GATE_DUMMY_2(CLK_APU2_APU, "apu2_apu",
			"dsp3_ck"/* parent */, 0),
	GATE_DUMMY_2(CLK_APU2_AXI_M, "apu2_axi_m",
			"dsp3_ck"/* parent */, 1),
	GATE_DUMMY_2(CLK_APU2_JTAG, "apu2_jtag",
			"dsp3_ck"/* parent */, 2),
};

static const struct mtk_clk_desc apu2_mcd = {
	.clks = apu2_clks,
	.num_clks = CLK_APU2_NR_CLK,
};

static const struct mtk_gate_regs apuv_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APUV(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apuv_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY_V(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &apuv_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate apuv_clks[] = {
	GATE_DUMMY_V(CLK_APUV_AHB, "apuv_ahb",
			"ipu_if_ck"/* parent */, 0),
	GATE_DUMMY_V(CLK_APUV_AXI, "apuv_axi",
			"ipu_if_ck"/* parent */, 1),
	GATE_DUMMY_V(CLK_APUV_ADL, "apuv_adl",
			"ipu_if_ck"/* parent */, 2),
	GATE_DUMMY_V(CLK_APUV_QOS, "apuv_qos",
			"ipu_if_ck"/* parent */, 3),
};

static const struct mtk_clk_desc apuv_mcd = {
	.clks = apuv_clks,
	.num_clks = CLK_APUV_NR_CLK,
};

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
	}

#define GATE_DUMMY_C(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &apuc_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate apuc_clks[] = {
	GATE_DUMMY_C(CLK_APUC_AHB, "apuc_ahb",
			"dsp_ck"/* parent */, 1),
	GATE_DUMMY_C(CLK_APUC_AXI, "apuc_axi",
			"dsp_ck"/* parent */, 2),
	GATE_DUMMY_C(CLK_APUC_ISP, "apuc_isp",
			"dsp_ck"/* parent */, 3),
	GATE_DUMMY_C(CLK_APUC_CAM_ADL, "apuc_cam_adl",
			"dsp_ck"/* parent */, 4),
	GATE_DUMMY_C(CLK_APUC_IMG_ADL, "apuc_img_adl",
			"dsp_ck"/* parent */, 5),
	GATE_DUMMY_C(CLK_APUC_EMI_26M, "apuc_emi_26m",
			"dsp_ck"/* parent */, 6),
	GATE_DUMMY_C(CLK_APUC_VPU_UDI, "apuc_vpu_udi",
			"dsp_ck"/* parent */, 7),
	GATE_DUMMY_C(CLK_APUC_EDMA_0, "apuc_edma_0",
			"dsp_ck"/* parent */, 8),
	GATE_DUMMY_C(CLK_APUC_EDMA_1, "apuc_edma_1",
			"dsp_ck"/* parent */, 9),
	GATE_DUMMY_C(CLK_APUC_EDMAL_0, "apuc_edmal_0",
			"dsp_ck"/* parent */, 10),
	GATE_DUMMY_C(CLK_APUC_EDMAL_1, "apuc_edmal_1",
			"dsp_ck"/* parent */, 11),
	GATE_DUMMY_C(CLK_APUC_MNOC, "apuc_mnoc",
			"dsp_ck"/* parent */, 12),
	GATE_DUMMY_C(CLK_APUC_TCM, "apuc_tcm",
			"dsp_ck"/* parent */, 13),
	GATE_DUMMY_C(CLK_APUC_MD32, "apuc_md32",
			"dsp_ck"/* parent */, 14),
	GATE_DUMMY_C(CLK_APUC_IOMMU_0, "apuc_iommu_0",
			"dsp7_ck"/* parent */, 15),
	GATE_DUMMY_C(CLK_APUC_IOMMU_1, "apuc_iommu_1",
			"dsp7_ck"/* parent */, 16),
	GATE_DUMMY_C(CLK_APUC_MD32_32K, "apuc_md32_32k",
			"dsp_ck"/* parent */, 17),
};

static const struct mtk_clk_desc apuc_mcd = {
	.clks = apuc_clks,
	.num_clks = CLK_APUC_NR_CLK,
};

static const struct mtk_gate_regs apum0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APUM0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apum0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY_M0(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &apum0_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate apum0_clks[] = {
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG0, "apum0_mdla_cg0",
			"dsp6_ck"/* parent */, 0),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG1, "apum0_mdla_cg1",
			"dsp6_ck"/* parent */, 1),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG2, "apum0_mdla_cg2",
			"dsp6_ck"/* parent */, 2),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG3, "apum0_mdla_cg3",
			"dsp6_ck"/* parent */, 3),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG4, "apum0_mdla_cg4",
			"dsp6_ck"/* parent */, 4),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG5, "apum0_mdla_cg5",
			"dsp6_ck"/* parent */, 5),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG6, "apum0_mdla_cg6",
			"dsp6_ck"/* parent */, 6),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG7, "apum0_mdla_cg7",
			"dsp6_ck"/* parent */, 7),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG8, "apum0_mdla_cg8",
			"dsp6_ck"/* parent */, 8),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG9, "apum0_mdla_cg9",
			"dsp6_ck"/* parent */, 9),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG10, "apum0_mdla_cg10",
			"dsp6_ck"/* parent */, 10),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG11, "apum0_mdla_cg11",
			"dsp6_ck"/* parent */, 11),
	GATE_DUMMY_M0(CLK_APUM0_MDLA_CG12, "apum0_mdla_cg12",
			"dsp6_ck"/* parent */, 12),
	GATE_DUMMY_M0(CLK_APUM0_APB, "apum0_apb",
			"dsp6_ck"/* parent */, 13),
	GATE_DUMMY_M0(CLK_APUM0_AXI_M, "apum0_axi_m",
			"dsp6_ck"/* parent */, 14),
};

static const struct mtk_clk_desc apum0_mcd = {
	.clks = apum0_clks,
	.num_clks = CLK_APUM0_NR_CLK,
};

static const struct mtk_gate_regs apum1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APUM1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apum1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_DUMMY_M1(_id, _name, _parent, _shift) {\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = &apum1_cg_regs,					\
		.shift = _shift,				\
		.ops = &mtk_clk_gate_ops_setclr_dummy,		\
	}

static const struct mtk_gate apum1_clks[] = {
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG0, "apum1_mdla_cg0",
			"dsp6_ck"/* parent */, 0),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG1, "apum1_mdla_cg1",
			"dsp6_ck"/* parent */, 1),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG2, "apum1_mdla_cg2",
			"dsp6_ck"/* parent */, 2),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG3, "apum1_mdla_cg3",
			"dsp6_ck"/* parent */, 3),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG4, "apum1_mdla_cg4",
			"dsp6_ck"/* parent */, 4),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG5, "apum1_mdla_cg5",
			"dsp6_ck"/* parent */, 5),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG6, "apum1_mdla_cg6",
			"dsp6_ck"/* parent */, 6),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG7, "apum1_mdla_cg7",
			"dsp6_ck"/* parent */, 7),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG8, "apum1_mdla_cg8",
			"dsp6_ck"/* parent */, 8),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG9, "apum1_mdla_cg9",
			"dsp6_ck"/* parent */, 9),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG10, "apum1_mdla_cg10",
			"dsp6_ck"/* parent */, 10),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG11, "apum1_mdla_cg11",
			"dsp6_ck"/* parent */, 11),
	GATE_DUMMY_M1(CLK_APUM1_MDLA_CG12, "apum1_mdla_cg12",
			"dsp6_ck"/* parent */, 12),
	GATE_DUMMY_M1(CLK_APUM1_APB, "apum1_apb",
			"dsp6_ck"/* parent */, 13),
	GATE_DUMMY_M1(CLK_APUM1_AXI_M, "apum1_axi_m",
			"dsp6_ck"/* parent */, 14),
};

static const struct mtk_clk_desc apum1_mcd = {
	.clks = apum1_clks,
	.num_clks = CLK_APUM1_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6893_apu[] = {
	{
		.compatible = "mediatek,mt6893-apu0",
		.data = &apu0_mcd,
	}, {
		.compatible = "mediatek,mt6893-apu1",
		.data = &apu1_mcd,
	}, {
		.compatible = "mediatek,mt6893-apu2",
		.data = &apu2_mcd,
	}, {
		.compatible = "mediatek,mt6893-apu_vcore",
		.data = &apuv_mcd,
	}, {
		.compatible = "mediatek,mt6893-apu_conn",
		.data = &apuc_mcd,
	}, {
		.compatible = "mediatek,mt6893-apu_mdla0",
		.data = &apum0_mcd,
	}, {
		.compatible = "mediatek,mt6893-apu_mdla1",
		.data = &apum1_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6893_apu_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6893_apu_drv = {
	.probe = clk_mt6893_apu_grp_probe,
	.driver = {
		.name = "clk-mt6893-apu",
		.of_match_table = of_match_clk_mt6893_apu,
	},
};

static int __init clk_mt6893_apu_init(void)
{
	return platform_driver_register(&clk_mt6893_apu_drv);
}

static void __exit clk_mt6893_apu_exit(void)
{
	platform_driver_unregister(&clk_mt6893_apu_drv);
}

postcore_initcall(clk_mt6893_apu_init);
module_exit(clk_mt6893_apu_exit);
MODULE_LICENSE("GPL");
