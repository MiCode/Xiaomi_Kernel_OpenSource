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

static const struct mtk_gate_regs imp_iic_wrap_n_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMP_IIC_WRAP_N(_id, _name, _parent, _shift)		\
	GATE_MTK(_id, _name, _parent, &imp_iic_wrap_n_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate imp_iic_wrap_n_clks[] = {
	GATE_IMP_IIC_WRAP_N(CLK_IMP_IIC_WRAP_N_I2C0, "imp_iic_wrap_n_i2c0",
		"infra_i2c0", 0),
	GATE_IMP_IIC_WRAP_N(CLK_IMP_IIC_WRAP_N_I2C6, "imp_iic_wrap_n_i2c6",
		"infra_i2c0", 1),
};

static int clk_mt8192_imp_iic_wrap_n_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_IMP_IIC_WRAP_N_NR_CLK);

	mtk_clk_register_gates(node, imp_iic_wrap_n_clks,
			ARRAY_SIZE(imp_iic_wrap_n_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_imp_iic_wrap_n[] = {
	{ .compatible = "mediatek,mt8192-imp_iic_wrap_n", },
	{}
};

static struct platform_driver clk_mt8192_imp_iic_wrap_n_drv = {
	.probe = clk_mt8192_imp_iic_wrap_n_probe,
	.driver = {
		.name = "clk-mt8192-imp_iic_wrap_n",
		.of_match_table = of_match_clk_mt8192_imp_iic_wrap_n,
	},
};

static int __init clk_mt8192_imp_iic_wrap_n_init(void)
{
	return platform_driver_register(&clk_mt8192_imp_iic_wrap_n_drv);
}

static void __exit clk_mt8192_imp_iic_wrap_n_exit(void)
{
	platform_driver_unregister(&clk_mt8192_imp_iic_wrap_n_drv);
}

arch_initcall(clk_mt8192_imp_iic_wrap_n_init);
module_exit(clk_mt8192_imp_iic_wrap_n_exit);
MODULE_LICENSE("GPL");
