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
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6877-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status apu0_pwr_stat = GATE_PWR_STAT(0x178,
		0x178, INV_OFS, BIT(5), BIT(5));

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
		.pwr_stat = &apu0_pwr_stat,			\
	}

static const struct mtk_gate apu0_clks[] = {
	GATE_APU0(CLK_APU0_APU, "apu0_apu",
			"dsp1_ck"/* parent */, 0),
	GATE_APU0(CLK_APU0_AXI_M, "apu0_axi_m",
			"dsp1_ck"/* parent */, 1),
	GATE_APU0(CLK_APU0_JTAG, "apu0_jtag",
			"dsp1_ck"/* parent */, 2),
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status apu1_pwr_stat = GATE_PWR_STAT(0x178,
		0x178, INV_OFS, BIT(5), BIT(5));

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
		.pwr_stat = &apu1_pwr_stat,			\
	}

static const struct mtk_gate apu1_clks[] = {
	GATE_APU1(CLK_APU1_APU, "apu1_apu",
			"dsp2_ck"/* parent */, 0),
	GATE_APU1(CLK_APU1_AXI_M, "apu1_axi_m",
			"dsp2_ck"/* parent */, 1),
	GATE_APU1(CLK_APU1_JTAG, "apu1_jtag",
			"dsp2_ck"/* parent */, 2),
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status apuv_pwr_stat = GATE_PWR_STAT(0x178,
		0x178, INV_OFS, BIT(5), BIT(5));

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
		.pwr_stat = &apuv_pwr_stat,			\
	}

static const struct mtk_gate apuv_clks[] = {
	GATE_APUV(CLK_APUV_AHB, "apuv_ahb",
			"clk_null"/* parent */, 0),
	GATE_APUV(CLK_APUV_AXI, "apuv_axi",
			"clk_null"/* parent */, 1),
	GATE_APUV(CLK_APUV_ADL, "apuv_adl",
			"clk_null"/* parent */, 2),
	GATE_APUV(CLK_APUV_QOS, "apuv_qos",
			"clk_null"/* parent */, 3),
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status apu_conn1_pwr_stat = GATE_PWR_STAT(0x178,
		0x178, INV_OFS, BIT(5), BIT(5));

static const struct mtk_gate_regs apu_conn1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_CONN1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu_conn1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &apu_conn1_pwr_stat,			\
	}

static const struct mtk_gate apu_conn1_clks[] = {
	GATE_APU_CONN1(CLK_APU_CONN1_AXI, "apu_conn1_axi",
			"dsp_ck"/* parent */, 0),
	GATE_APU_CONN1(CLK_APU_CONN1_EDMA_0, "apu_conn1_edma_0",
			"dsp_ck"/* parent */, 1),
	GATE_APU_CONN1(CLK_APU_CONN1_EDMA_1, "apu_conn1_edma_1",
			"dsp_ck"/* parent */, 2),
	GATE_APU_CONN1(CLK_APU_CONN1_IOMMU_0, "apu_conn1_iommu_0",
			"dsp7_ck"/* parent */, 4),
	GATE_APU_CONN1(CLK_APU_CONN1_IOMMU_1, "apu_conn1_iommu_1",
			"dsp_ck"/* parent */, 5),
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status apu_conn2_pwr_stat = GATE_PWR_STAT(0x178,
		0x178, INV_OFS, BIT(5), BIT(5));

static const struct mtk_gate_regs apu_conn2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_APU_CONN2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu_conn2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &apu_conn2_pwr_stat,			\
	}

static const struct mtk_gate apu_conn2_clks[] = {
	GATE_APU_CONN2(CLK_APU_CONN2_AHB, "apu_conn2_ahb",
			"dsp_ck"/* parent */, 1),
	GATE_APU_CONN2(CLK_APU_CONN2_AXI, "apu_conn2_axi",
			"dsp_ck"/* parent */, 2),
	GATE_APU_CONN2(CLK_APU_CONN2_ISP, "apu_conn2_isp",
			"dsp_ck"/* parent */, 3),
	GATE_APU_CONN2(CLK_APU_CONN2_CAM_ADL, "apu_conn2_cam_adl",
			"dsp_ck"/* parent */, 4),
	GATE_APU_CONN2(CLK_APU_CONN2_IMG_ADL, "apu_conn2_img_adl",
			"dsp_ck"/* parent */, 5),
	GATE_APU_CONN2(CLK_APU_CONN2_EMI_26M, "apu_conn2_emi_26m",
			"dsp_ck"/* parent */, 6),
	GATE_APU_CONN2(CLK_APU_CONN2_VPU_UDI, "apu_conn2_vpu_udi",
			"dsp_ck"/* parent */, 7),
	GATE_APU_CONN2(CLK_APU_CONN2_EDMA_0, "apu_conn2_edma_0",
			"dsp_ck"/* parent */, 8),
	GATE_APU_CONN2(CLK_APU_CONN2_EDMA_1, "apu_conn2_edma_1",
			"dsp_ck"/* parent */, 9),
	GATE_APU_CONN2(CLK_APU_CONN2_EDMAL_0, "apu_conn2_edmal_0",
			"dsp_ck"/* parent */, 10),
	GATE_APU_CONN2(CLK_APU_CONN2_EDMAL_1, "apu_conn2_edmal_1",
			"dsp_ck"/* parent */, 11),
	GATE_APU_CONN2(CLK_APU_CONN2_MNOC, "apu_conn2_mnoc",
			"dsp_ck"/* parent */, 12),
	GATE_APU_CONN2(CLK_APU_CONN2_TCM, "apu_conn2_tcm",
			"dsp_ck"/* parent */, 13),
	GATE_APU_CONN2(CLK_APU_CONN2_MD32, "apu_conn2_md32",
			"dsp_ck"/* parent */, 14),
	GATE_APU_CONN2(CLK_APU_CONN2_IOMMU_0, "apu_conn2_iommu_0",
			"dsp7_ck"/* parent */, 15),
	GATE_APU_CONN2(CLK_APU_CONN2_IOMMU_1, "apu_conn2_iommu_1",
			"dsp7_ck"/* parent */, 16),
	GATE_APU_CONN2(CLK_APU_CONN2_MD32_32K, "apu_conn2_md32_32k",
			"dsp_ck"/* parent */, 17),
	GATE_APU_CONN2(CLK_APU_CONN2_CPE, "apu_conn2_cpe",
			"dsp_ck"/* parent */, 18),
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status apum0_pwr_stat = GATE_PWR_STAT(0x178,
		0x178, INV_OFS, BIT(5), BIT(5));

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
		.pwr_stat = &apum0_pwr_stat,			\
	}

static const struct mtk_gate apum0_clks[] = {
	GATE_APUM0(CLK_APUM0_MDLA_CG0, "apum0_mdla_cg0",
			"dsp4_ck"/* parent */, 0),
	GATE_APUM0(CLK_APUM0_MDLA_CG1, "apum0_mdla_cg1",
			"dsp4_ck"/* parent */, 1),
	GATE_APUM0(CLK_APUM0_MDLA_CG2, "apum0_mdla_cg2",
			"dsp4_ck"/* parent */, 2),
	GATE_APUM0(CLK_APUM0_MDLA_CG3, "apum0_mdla_cg3",
			"dsp4_ck"/* parent */, 3),
	GATE_APUM0(CLK_APUM0_MDLA_CG4, "apum0_mdla_cg4",
			"dsp4_ck"/* parent */, 4),
	GATE_APUM0(CLK_APUM0_MDLA_CG5, "apum0_mdla_cg5",
			"dsp4_ck"/* parent */, 5),
	GATE_APUM0(CLK_APUM0_MDLA_CG6, "apum0_mdla_cg6",
			"dsp4_ck"/* parent */, 6),
	GATE_APUM0(CLK_APUM0_MDLA_CG7, "apum0_mdla_cg7",
			"dsp4_ck"/* parent */, 7),
	GATE_APUM0(CLK_APUM0_MDLA_CG8, "apum0_mdla_cg8",
			"dsp4_ck"/* parent */, 8),
	GATE_APUM0(CLK_APUM0_MDLA_CG9, "apum0_mdla_cg9",
			"dsp4_ck"/* parent */, 9),
	GATE_APUM0(CLK_APUM0_MDLA_CG10, "apum0_mdla_cg10",
			"dsp4_ck"/* parent */, 10),
	GATE_APUM0(CLK_APUM0_MDLA_CG11, "apum0_mdla_cg11",
			"dsp4_ck"/* parent */, 11),
	GATE_APUM0(CLK_APUM0_MDLA_CG12, "apum0_mdla_cg12",
			"dsp4_ck"/* parent */, 12),
	GATE_APUM0(CLK_APUM0_APB, "apum0_apb",
			"dsp4_ck"/* parent */, 19),
	GATE_APUM0(CLK_APUM0_AXI_M, "apum0_axi_m",
			"dsp4_ck"/* parent */, 20),
};

static int clk_mt6877_apu0_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APU0_NR_CLK);

	mtk_clk_register_gates(node, apu0_clks, ARRAY_SIZE(apu0_clks),
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

static int clk_mt6877_apu1_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APU1_NR_CLK);

	mtk_clk_register_gates(node, apu1_clks, ARRAY_SIZE(apu1_clks),
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

static int clk_mt6877_apuv_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APUV_NR_CLK);

	mtk_clk_register_gates(node, apuv_clks, ARRAY_SIZE(apuv_clks),
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

static int clk_mt6877_apu_conn1_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APU_CONN1_NR_CLK);

	mtk_clk_register_gates(node, apu_conn1_clks, ARRAY_SIZE(apu_conn1_clks),
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

static int clk_mt6877_apu_conn2_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APU_CONN2_NR_CLK);

	mtk_clk_register_gates(node, apu_conn2_clks, ARRAY_SIZE(apu_conn2_clks),
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

static int clk_mt6877_apum0_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APUM0_NR_CLK);

	mtk_clk_register_gates(node, apum0_clks, ARRAY_SIZE(apum0_clks),
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

static const struct of_device_id of_match_clk_mt6877_apu[] = {
	{
		.compatible = "mediatek,mt6877-apu0",
		.data = clk_mt6877_apu0_probe,
	}, {
		.compatible = "mediatek,mt6877-apu1",
		.data = clk_mt6877_apu1_probe,
	}, {
		.compatible = "mediatek,mt6877-apu_vcore",
		.data = clk_mt6877_apuv_probe,
	}, {
		.compatible = "mediatek,mt6877-apu_conn1",
		.data = clk_mt6877_apu_conn1_probe,
	}, {
		.compatible = "mediatek,mt6877-apu_conn2",
		.data = clk_mt6877_apu_conn2_probe,
	}, {
		.compatible = "mediatek,mt6877-apu_mdla0",
		.data = clk_mt6877_apum0_probe,
	}, {
		/* sentinel */
	}
};


static int clk_mt6877_apu_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6877_apu_drv = {
	.probe = clk_mt6877_apu_probe,
	.driver = {
		.name = "clk-mt6877-apu",
		.of_match_table = of_match_clk_mt6877_apu,
	},
};

static int __init clk_mt6877_apu_init(void)
{
	return platform_driver_register(&clk_mt6877_apu_drv);
}
arch_initcall(clk_mt6877_apu_init);

