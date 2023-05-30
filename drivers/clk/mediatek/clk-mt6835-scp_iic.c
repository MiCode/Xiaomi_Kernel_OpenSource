// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Chuan-Wen Chen <chuan-wen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6835-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs scp_iic_cg_regs = {
	.set_ofs = 0xE18,
	.clr_ofs = 0xE14,
	.sta_ofs = 0xE10,
};

#define GATE_SCP_IIC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &scp_iic_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate scp_iic_clks[] = {
	GATE_SCP_IIC(CLK_SCP_IIC_AP_CLOCK_I2C0, "scp_iic_i2c0",
			"ulposc_ck"/* parent */, 0),
	GATE_SCP_IIC(CLK_SCP_IIC_AP_CLOCK_I2C1, "scp_iic_i2c1",
			"ulposc_ck"/* parent */, 1),
	GATE_SCP_IIC(CLK_SCP_IIC_AP_CLOCK_I2C2, "scp_iic_i2c2",
			"ulposc_ck"/* parent */, 2),
	GATE_SCP_IIC(CLK_SCP_IIC_AP_CLOCK_I2C3, "scp_iic_i2c3",
			"ulposc_ck"/* parent */, 3),
	GATE_SCP_IIC(CLK_SCP_IIC_AP_CLOCK_I2C4, "scp_iic_i2c4",
			"ulposc_ck"/* parent */, 4),
	GATE_SCP_IIC(CLK_SCP_IIC_AP_CLOCK_I2C5, "scp_iic_i2c5",
			"ulposc_ck"/* parent */, 5),
	GATE_SCP_IIC(CLK_SCP_IIC_AP_CLOCK_I2C6, "scp_iic_i2c6",
			"ulposc_ck"/* parent */, 6),
};

static const struct mtk_clk_desc scp_iic_mcd = {
	.clks = scp_iic_clks,
	.num_clks = CLK_SCP_IIC_NR_CLK,
};

static int clk_mt6835_scp_iic_probe(struct platform_device *pdev)
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

static const struct of_device_id of_match_clk_mt6835_scp_iic[] = {
	{
		.compatible = "mediatek,mt6835-scp_iic",
		.data = &scp_iic_mcd,
	},
	{}
};

static struct platform_driver clk_mt6835_scp_iic_drv = {
	.probe = clk_mt6835_scp_iic_probe,
	.driver = {
		.name = "clk-mt6835-scp_iic",
		.of_match_table = of_match_clk_mt6835_scp_iic,
	},
};

module_platform_driver(clk_mt6835_scp_iic_drv);
MODULE_LICENSE("GPL");
