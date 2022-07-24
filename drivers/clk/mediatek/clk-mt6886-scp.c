// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chong-ming Wei <chong-ming.wei@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6886-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs scp0_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x30,
	.sta_ofs = 0x30,
};

static const struct mtk_gate_regs scp1_cg_regs = {
	.set_ofs = 0x58,
	.clr_ofs = 0x58,
	.sta_ofs = 0x58,
};

#define GATE_SCP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_SCP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate scp_clks[] = {
	/* SCP0 */
	GATE_SCP0(CLK_SCP_SET_SPI0, "scp_set_spi0",
			"ulposc_ck"/* parent */, 13),
	GATE_SCP0(CLK_SCP_SET_SPI1, "scp_set_spi1",
			"ulposc_ck"/* parent */, 14),
	GATE_SCP0(CLK_SCP_SET_SPI2, "scp_set_spi2",
			"ulposc_ck"/* parent */, 15),
	GATE_SCP0(CLK_SCP_SET_SPI3, "scp_set_spi3",
			"ulposc_ck"/* parent */, 30),
	/* SCP1 */
	GATE_SCP1(CLK_SCP_SPI0, "scp_spi0",
			"ulposc_ck"/* parent */, 13),
	GATE_SCP1(CLK_SCP_SPI1, "scp_spi1",
			"ulposc_ck"/* parent */, 14),
	GATE_SCP1(CLK_SCP_SPI2, "scp_spi2",
			"ulposc_ck"/* parent */, 15),
	GATE_SCP1(CLK_SCP_SPI3, "scp_spi3",
			"ulposc_ck"/* parent */, 30),
};

static const struct mtk_clk_desc scp_mcd = {
	.clks = scp_clks,
	.num_clks = CLK_SCP_NR_CLK,
};

static int clk_mt6886_scp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6886_scp[] = {
	{
		.compatible = "mediatek,mt6886-scp",
		.data = &scp_mcd,
	},
	{}
};

static struct platform_driver clk_mt6886_scp_drv = {
	.probe = clk_mt6886_scp_probe,
	.driver = {
		.name = "clk-mt6886-scp",
		.of_match_table = of_match_clk_mt6886_scp,
	},
};

module_platform_driver(clk_mt6886_scp_drv);
MODULE_LICENSE("GPL");
