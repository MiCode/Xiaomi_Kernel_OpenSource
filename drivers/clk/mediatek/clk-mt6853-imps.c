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

#define MT_CCF_BRINGUP			1

#define INV_OFS			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(INV_OFS, INV_OFS,
		0x00b0, BIT(15), 0);

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imps_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_AP_CLOCK_RO_I2C5, "imps_ap_i2c5",
			"i2c_pseudo"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_RO_I2C7, "imps_ap_i2c7",
			"i2c_pseudo"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_RO_I2C8, "imps_ap_i2c8",
			"i2c_pseudo"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_RO_I2C9, "imps_ap_i2c9",
			"i2c_pseudo"/* parent */, 3),
};

static int clk_mt6853_imps_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPS_NR_CLK);

	mtk_clk_register_gates(node, imps_clks, ARRAY_SIZE(imps_clks),
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

static const struct of_device_id of_match_clk_mt6853_imps[] = {
	{ .compatible = "mediatek,mt6853-imp_iic_wrap_s", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_imps_drv = {
	.probe = clk_mt6853_imps_probe,
	.driver = {
		.name = "clk-mt6853-imps",
		.of_match_table = of_match_clk_mt6853_imps,
	},
};

builtin_platform_driver(clk_mt6853_imps_drv);

#else

static struct platform_driver clk_mt6853_imps_drv = {
	.probe = clk_mt6853_imps_probe,
	.driver = {
		.name = "clk-mt6853-imps",
		.of_match_table = of_match_clk_mt6853_imps,
	},
};
static int __init clk_mt6853_imps_platform_init(void)
{
	return platform_driver_register(&clk_mt6853_imps_drv);
}
arch_initcall(clk_mt6853_imps_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
