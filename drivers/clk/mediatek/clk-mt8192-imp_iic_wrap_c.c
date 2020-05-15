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

static const struct mtk_gate_regs imp_iic_wrap_c_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMP_IIC_WRAP_C(_id, _name, _parent, _shift)		\
	GATE_MTK(_id, _name, _parent, &imp_iic_wrap_c_cg_regs, _shift,	\
		&mtk_clk_gate_ops_setclr)

static const struct mtk_gate imp_iic_wrap_c_clks[] = {
	GATE_IMP_IIC_WRAP_C(CLK_IMP_IIC_WRAP_C_I2C10, "imp_iic_wrap_c_i2c10",
		"infra_i2c0", 0),
	GATE_IMP_IIC_WRAP_C(CLK_IMP_IIC_WRAP_C_I2C11, "imp_iic_wrap_c_i2c11",
		"infra_i2c0", 1),
	GATE_IMP_IIC_WRAP_C(CLK_IMP_IIC_WRAP_C_I2C12, "imp_iic_wrap_c_i2c12",
		"infra_i2c0", 2),
	GATE_IMP_IIC_WRAP_C(CLK_IMP_IIC_WRAP_C_I2C13, "imp_iic_wrap_c_i2c13",
		"infra_i2c0", 3),
};

static int clk_mt8192_imp_iic_wrap_c_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	struct device_node *node = pdev->dev.of_node;

	clk_data = mtk_alloc_clk_data(CLK_IMP_IIC_WRAP_C_NR_CLK);

	mtk_clk_register_gates(node, imp_iic_wrap_c_clks,
			ARRAY_SIZE(imp_iic_wrap_c_clks),
			clk_data);

	return of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}

static const struct of_device_id of_match_clk_mt8192_imp_iic_wrap_c[] = {
	{ .compatible = "mediatek,mt8192-imp_iic_wrap_c", },
	{}
};

static struct platform_driver clk_mt8192_imp_iic_wrap_c_drv = {
	.probe = clk_mt8192_imp_iic_wrap_c_probe,
	.driver = {
		.name = "clk-mt8192-imp_iic_wrap_c",
		.of_match_table = of_match_clk_mt8192_imp_iic_wrap_c,
	},
};

static int __init clk_mt8192_imp_iic_wrap_c_init(void)
{
	return platform_driver_register(&clk_mt8192_imp_iic_wrap_c_drv);
}

static void __exit clk_mt8192_imp_iic_wrap_c_exit(void)
{
	platform_driver_unregister(&clk_mt8192_imp_iic_wrap_c_drv);
}

arch_initcall(clk_mt8192_imp_iic_wrap_c_init);
module_exit(clk_mt8192_imp_iic_wrap_c_exit);
MODULE_LICENSE("GPL");
