// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6835-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ifrao0_cg_regs = {
	.set_ofs = 0x74,
	.clr_ofs = 0x74,
	.sta_ofs = 0x74,
};

static const struct mtk_gate_regs ifrao1_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifrao2_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8c,
	.sta_ofs = 0x94,
};

#define GATE_IFRAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_IFRAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ifrao_clks[] = {
	/* IFRAO0 */
	GATE_IFRAO0(CLK_IFRAOP_DCM_RG_FORCE, "ifrao_dcmforce",
			"axi_ck"/* parent */, 2),
	/* IFRAO1 */
	GATE_IFRAO1(CLK_IFRAO_THERM, "ifrao_therm",
			"axi_ck"/* parent */, 0),
	GATE_IFRAO1(CLK_IFRAO_CPUM, "ifrao_cpum",
			"axi_ck"/* parent */, 3),
	GATE_IFRAO1(CLK_IFRAO_CCIF1_AP, "ifrao_ccif1_ap",
			"axi_ck"/* parent */, 4),
	GATE_IFRAO1(CLK_IFRAO_CCIF1_MD, "ifrao_ccif1_md",
			"axi_ck"/* parent */, 5),
	GATE_IFRAO1(CLK_IFRAO_CCIF_AP, "ifrao_ccif_ap",
			"axi_ck"/* parent */, 6),
	GATE_IFRAO1(CLK_IFRAO_CCIF_MD, "ifrao_ccif_md",
			"axi_ck"/* parent */, 8),
	GATE_IFRAO1(CLK_IFRAO_CLDMA_BCLK, "ifrao_cldmabclk",
			"axi_ck"/* parent */, 12),
	GATE_IFRAO1(CLK_IFRAO_CQ_DMA, "ifrao_cq_dma",
			"axi_ck"/* parent */, 13),
	GATE_IFRAO1(CLK_IFRAO_CCIF5_MD, "ifrao_ccif5_md",
			"axi_ck"/* parent */, 15),
	GATE_IFRAO1(CLK_IFRAO_CCIF2_AP, "ifrao_ccif2_ap",
			"axi_ck"/* parent */, 16),
	GATE_IFRAO1(CLK_IFRAO_CCIF2_MD, "ifrao_ccif2_md",
			"axi_ck"/* parent */, 17),
	GATE_IFRAO1(CLK_IFRAO_DPMAIF_MAIN, "ifrao_dpmaif_main",
			"dpmaif_main_ck"/* parent */, 22),
	GATE_IFRAO1(CLK_IFRAO_CCIF4_AP, "ifrao_ccif4_ap",
			"axi_ck"/* parent */, 23),
	GATE_IFRAO1(CLK_IFRAO_CCIF4_MD, "ifrao_ccif4_md",
			"axi_ck"/* parent */, 24),
	GATE_IFRAO1(CLK_IFRAO_RG_MMW_DPMAIF26M_CK, "ifrao_dpmaif_26m",
			"f26m_ck"/* parent */, 25),
	GATE_IFRAO1(CLK_IFRAO_RG_MEM_SUB_CK, "ifrao_mem_sub_ck",
			"mem_sub_ck"/* parent */, 26),
	GATE_IFRAO1(CLK_IFRAO_AES_TOP0, "ifrao_aes_top0",
			"axi_ck"/* parent */, 28),
	/* IFRAO2 */
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_0, "ifrao_i2c_dummy_0",
			"i2c_ck"/* parent */, 0),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_1, "ifrao_i2c_dummy_1",
			"i2c_ck"/* parent */, 1),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_2, "ifrao_i2c_dummy_2",
			"i2c_ck"/* parent */, 2),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_3, "ifrao_i2c_dummy_3",
			"i2c_ck"/* parent */, 3),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_4, "ifrao_i2c_dummy_4",
			"i2c_ck"/* parent */, 4),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_5, "ifrao_i2c_dummy_5",
			"i2c_ck"/* parent */, 5),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_6, "ifrao_i2c_dummy_6",
			"i2c_ck"/* parent */, 6),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_7, "ifrao_i2c_dummy_7",
			"i2c_ck"/* parent */, 7),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_8, "ifrao_i2c_dummy_8",
			"i2c_ck"/* parent */, 8),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_9, "ifrao_i2c_dummy_9",
			"i2c_ck"/* parent */, 9),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_10, "ifrao_i2c_dummy_10",
			"i2c_ck"/* parent */, 10),
	GATE_IFRAO2(CLK_IFRAO_I2C_DUMMY_11, "ifrao_i2c_dummy_11",
			"i2c_ck"/* parent */, 11),
};

static const struct mtk_clk_desc ifrao_mcd = {
	.clks = ifrao_clks,
	.num_clks = CLK_IFRAO_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6835_bus[] = {
	{
		.compatible = "mediatek,mt6835-infracfg_ao",
		.data = &ifrao_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6835_bus_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6835_bus_drv = {
	.probe = clk_mt6835_bus_grp_probe,
	.driver = {
		.name = "clk-mt6835-bus",
		.of_match_table = of_match_clk_mt6835_bus,
	},
};

module_platform_driver(clk_mt6835_bus_drv);
MODULE_LICENSE("GPL");
