// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6983-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		1
#define MT_CCF_PLL_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1


static const struct mtk_gate_regs imp_iic_wrap_0_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

static const struct mtk_gate_regs imp_iic_wrap_1_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

static const struct mtk_gate_regs imp_iic_wrap_2_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
};

#define GATE_IMP_IIC_WRAP_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &imp_iic_wrap_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_IMP_IIC_WRAP_1(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &imp_iic_wrap_1_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_IMP_IIC_WRAP_2(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &imp_iic_wrap_2_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

static struct mtk_gate imp_iic_wrap0_clks[] = {
	GATE_IMP_IIC_WRAP_0(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C10 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c10" /* name */,
		"fi2c_ck" /* parent */, 0 /* bit */),
	GATE_IMP_IIC_WRAP_0(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C11 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c11" /* name */,
		"fi2c_ck" /* parent */, 1 /* bit */),
	GATE_IMP_IIC_WRAP_0(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C12 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c12" /* name */,
		"fi2c_ck" /* parent */, 2 /* bit */),
	GATE_IMP_IIC_WRAP_0(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C13 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c13" /* name */,
		"fi2c_ck" /* parent */, 3 /* bit */),
};

static struct mtk_gate imp_iic_wrap1_clks[] = {
	GATE_IMP_IIC_WRAP_1(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C1 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c1" /* name */,
		"fi2c_ck" /* parent */, 0 /* bit */),
	GATE_IMP_IIC_WRAP_1(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C2 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c2" /* name */,
		"fi2c_ck" /* parent */, 1 /* bit */),
	GATE_IMP_IIC_WRAP_1(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C3 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c3" /* name */,
		"fi2c_ck" /* parent */, 2 /* bit */),
	GATE_IMP_IIC_WRAP_1(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C4 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c4" /* name */,
		"fi2c_ck" /* parent */, 3 /* bit */),
	GATE_IMP_IIC_WRAP_1(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C7 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c7" /* name */,
		"fi2c_ck" /* parent */, 4 /* bit */),
	GATE_IMP_IIC_WRAP_1(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C8 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c8" /* name */,
		"fi2c_ck" /* parent */, 5 /* bit */),
	GATE_IMP_IIC_WRAP_1(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C9 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c9" /* name */,
		"fi2c_ck" /* parent */, 6 /* bit */),
};

static struct mtk_gate imp_iic_wrap2_clks[] = {
	GATE_IMP_IIC_WRAP_2(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C0 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c0" /* name */,
		"fi2c_ck" /* parent */, 0 /* bit */),
	GATE_IMP_IIC_WRAP_2(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C5 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c5" /* name */,
		"fi2c_ck" /* parent */, 1 /* bit */),
	GATE_IMP_IIC_WRAP_2(CLK_IMP_IIC_WRAP_AP_CLOCK_I2C6 /* CLK ID */,
		"imp_iic_wrap_ap_clock_i2c6" /* name */,
		"fi2c_ck" /* parent */, 2 /* bit */),
};

static int clk_mt6983_imp_iic_wrap0_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_IMP_IIC_WRAP0_NR_CLK);

	mtk_clk_register_gates(node, imp_iic_wrap0_clks,
		ARRAY_SIZE(imp_iic_wrap0_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static int clk_mt6983_imp_iic_wrap1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_IMP_IIC_WRAP1_NR_CLK);

	mtk_clk_register_gates(node, imp_iic_wrap1_clks,
		ARRAY_SIZE(imp_iic_wrap1_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static int clk_mt6983_imp_iic_wrap2_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_IMP_IIC_WRAP2_NR_CLK);

	mtk_clk_register_gates(node, imp_iic_wrap2_clks,
		ARRAY_SIZE(imp_iic_wrap2_clks),
		clk_data);

	ret = of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);

	if (ret)
		pr_notice("%s(): could not register clock provider: %d\n",
			__func__, ret);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif /* MT_CCF_BRINGUP */

	return ret;
}

static const struct of_device_id of_match_clk_mt6983_imp_iic_wrap[] = {
	{
		.compatible = "mediatek,mt6983-imp_iic_wrap0",
		.data = clk_mt6983_imp_iic_wrap0_probe,
	}, {
		.compatible = "mediatek,mt6983-imp_iic_wrap1",
		.data = clk_mt6983_imp_iic_wrap1_probe,
	}, {
		.compatible = "mediatek,mt6983-imp_iic_wrap2",
		.data = clk_mt6983_imp_iic_wrap2_probe,
	}, {
		/* sentinel */
	}
};


static int clk_mt6983_imp_iic_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6983_imp_iic_drv = {
	.probe = clk_mt6983_imp_iic_grp_probe,
	.driver = {
		.name = "clk-mt6983-imp_iic",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_imp_iic_wrap,
	},
};

static int __init clk_mt6983_imp_iic_init(void)
{
	return platform_driver_register(&clk_mt6983_imp_iic_drv);
}

static void __exit clk_mt6983_imp_iic_exit(void)
{
	platform_driver_unregister(&clk_mt6983_imp_iic_drv);
}

arch_initcall(clk_mt6983_imp_iic_init);
module_exit(clk_mt6983_imp_iic_exit);
MODULE_LICENSE("GPL");

