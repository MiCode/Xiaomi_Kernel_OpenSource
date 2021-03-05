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
static struct pwr_status vde2_pwr_stat = GATE_PWR_STAT(0xEF0,
		0xEF4, INV_OFS, BIT(12), BIT(12));

static const struct mtk_gate_regs vde2_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

#define GATE_VDE2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &vde2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
		.pwr_stat = &vde2_pwr_stat,			\
	}

static const struct mtk_gate vde2_clks[] = {
	GATE_VDE2(CLK_VDE2_VDEC_CKEN, "vde2_vdec_cken",
			"vdec_ck"/* parent */, 0),
};

static int clk_mt6877_vde2_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_VDE2_NR_CLK);

	mtk_clk_register_gates(node, vde2_clks, ARRAY_SIZE(vde2_clks),
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

static const struct of_device_id of_match_clk_mt6877_vde[] = {
	{
		.compatible = "mediatek,mt6877-vdecsys",
		.data = clk_mt6877_vde2_probe,
	}, {
		/* sentinel */
	}
};


static int clk_mt6877_vde_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6877_vde_drv = {
	.probe = clk_mt6877_vde_probe,
	.driver = {
		.name = "clk-mt6877-vde",
		.of_match_table = of_match_clk_mt6877_vde,
	},
};

static int __init clk_mt6877_vde_init(void)
{
	return platform_driver_register(&clk_mt6877_vde_drv);
}
arch_initcall(clk_mt6877_vde_init);

