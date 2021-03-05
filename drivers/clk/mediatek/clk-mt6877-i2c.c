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

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPC(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impc_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impc_clks[] = {
	GATE_IMPC(CLK_IMPC_AP_CLOCK_I2C10, "impc_ap_clock_i2c10",
			"fi2c_pseudo_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_AP_CLOCK_I2C11, "impc_ap_clock_i2c11",
			"fi2c_pseudo_ck"/* parent */, 1),
};

static const struct mtk_gate_regs impe_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPE(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impe_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impe_clks[] = {
	GATE_IMPE(CLK_IMPE_AP_CLOCK_I2C3, "impe_ap_clock_i2c3",
			"fi2c_pseudo_ck"/* parent */, 0),
};

static const struct mtk_gate_regs impn_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPN(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impn_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impn_clks[] = {
	GATE_IMPN(CLK_IMPN_AP_CLOCK_I2C6, "impn_ap_clock_i2c6",
			"fi2c_pseudo_ck"/* parent */, 0),
};

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
	}

static const struct mtk_gate imps_clks[] = {
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C5, "imps_ap_clock_i2c5",
			"fi2c_pseudo_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C7, "imps_ap_clock_i2c7",
			"fi2c_pseudo_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C8, "imps_ap_clock_i2c8",
			"fi2c_pseudo_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_AP_CLOCK_I2C9, "imps_ap_clock_i2c9",
			"fi2c_pseudo_ck"/* parent */, 3),
};

static const struct mtk_gate_regs impw_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPW(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impw_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impw_clks[] = {
	GATE_IMPW(CLK_IMPW_AP_CLOCK_I2C0, "impw_ap_clock_i2c0",
			"fi2c_pseudo_ck"/* parent */, 0),
};

static const struct mtk_gate_regs impws_cg_regs = {
	.set_ofs = 0xe08,
	.clr_ofs = 0xe04,
	.sta_ofs = 0xe00,
};

#define GATE_IMPWS(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &impws_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate impws_clks[] = {
	GATE_IMPWS(CLK_IMPWS_AP_CLOCK_I2C1, "impws_ap_clock_i2c1",
			"fi2c_pseudo_ck"/* parent */, 0),
	GATE_IMPWS(CLK_IMPWS_AP_CLOCK_I2C2, "impws_ap_clock_i2c2",
			"fi2c_pseudo_ck"/* parent */, 1),
	GATE_IMPWS(CLK_IMPWS_AP_CLOCK_I2C4, "impws_ap_clock_i2c4",
			"fi2c_pseudo_ck"/* parent */, 2),
};

static int clk_mt6877_impc_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPC_NR_CLK);

	mtk_clk_register_gates(node, impc_clks, ARRAY_SIZE(impc_clks),
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

static int clk_mt6877_impe_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPE_NR_CLK);

	mtk_clk_register_gates(node, impe_clks, ARRAY_SIZE(impe_clks),
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

static int clk_mt6877_impn_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPN_NR_CLK);

	mtk_clk_register_gates(node, impn_clks, ARRAY_SIZE(impn_clks),
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

static int clk_mt6877_imps_probe(struct platform_device *pdev)
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

static int clk_mt6877_impw_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPW_NR_CLK);

	mtk_clk_register_gates(node, impw_clks, ARRAY_SIZE(impw_clks),
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

static int clk_mt6877_impws_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMPWS_NR_CLK);

	mtk_clk_register_gates(node, impws_clks, ARRAY_SIZE(impws_clks),
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

static const struct of_device_id of_match_clk_mt6877_i2c[] = {
	{
		.compatible = "mediatek,mt6877-imp_iic_wrap_c",
		.data = clk_mt6877_impc_probe,
	}, {
		.compatible = "mediatek,mt6877-imp_iic_wrap_e",
		.data = clk_mt6877_impe_probe,
	}, {
		.compatible = "mediatek,mt6877-imp_iic_wrap_n",
		.data = clk_mt6877_impn_probe,
	}, {
		.compatible = "mediatek,mt6877-imp_iic_wrap_s",
		.data = clk_mt6877_imps_probe,
	}, {
		.compatible = "mediatek,mt6877-imp_iic_wrap_w",
		.data = clk_mt6877_impw_probe,
	}, {
		.compatible = "mediatek,mt6877-imp_iic_wrap_ws",
		.data = clk_mt6877_impws_probe,
	}, {
		/* sentinel */
	}
};


static int clk_mt6877_i2c_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6877_i2c_drv = {
	.probe = clk_mt6877_i2c_probe,
	.driver = {
		.name = "clk-mt6877-i2c",
		.of_match_table = of_match_clk_mt6877_i2c,
	},
};

static int __init clk_mt6877_i2c_init(void)
{
	return platform_driver_register(&clk_mt6877_i2c_drv);
}
arch_initcall(clk_mt6877_i2c_init);

