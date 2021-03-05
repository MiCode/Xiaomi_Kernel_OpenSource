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
static struct pwr_status imgsys1_pwr_stat = GATE_PWR_STAT(0xEF0,
		0xEF4, INV_OFS, BIT(9), BIT(9));

static const struct mtk_gate_regs imgsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &imgsys1_pwr_stat,			\
	}

static const struct mtk_gate imgsys1_clks[] = {
	GATE_IMGSYS1(CLK_IMGSYS1_LARB9, "imgsys1_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMGSYS1(CLK_IMGSYS1_DIP, "imgsys1_dip",
			"img1_ck"/* parent */, 2),
	GATE_IMGSYS1(CLK_IMGSYS1_GALS, "imgsys1_gals",
			"img1_ck"/* parent */, 12),
};

/* get spm power status struct to register inside clk_data */
static struct pwr_status imgsys2_pwr_stat = GATE_PWR_STAT(0xEF0,
		0xEF4, INV_OFS, BIT(10), BIT(10));

static const struct mtk_gate_regs imgsys2_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imgsys2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &imgsys2_pwr_stat,			\
	}

static const struct mtk_gate imgsys2_clks[] = {
	GATE_IMGSYS2(CLK_IMGSYS2_LARB9, "imgsys2_larb9",
			"img1_ck"/* parent */, 0),
	GATE_IMGSYS2(CLK_IMGSYS2_LARB10, "imgsys2_larb10",
			"img1_ck"/* parent */, 1),
	GATE_IMGSYS2(CLK_IMGSYS2_MFB, "imgsys2_mfb",
			"img1_ck"/* parent */, 6),
	GATE_IMGSYS2(CLK_IMGSYS2_WPE, "imgsys2_wpe",
			"img1_ck"/* parent */, 7),
	GATE_IMGSYS2(CLK_IMGSYS2_MSS, "imgsys2_mss",
			"img1_ck"/* parent */, 8),
	GATE_IMGSYS2(CLK_IMGSYS2_GALS, "imgsys2_gals",
			"img1_ck"/* parent */, 12),
};

static int clk_mt6877_imgsys1_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMGSYS1_NR_CLK);

	mtk_clk_register_gates(node, imgsys1_clks, ARRAY_SIZE(imgsys1_clks),
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

static int clk_mt6877_imgsys2_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMGSYS2_NR_CLK);

	mtk_clk_register_gates(node, imgsys2_clks, ARRAY_SIZE(imgsys2_clks),
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

static const struct of_device_id of_match_clk_mt6877_img[] = {
	{
		.compatible = "mediatek,mt6877-imgsys1",
		.data = clk_mt6877_imgsys1_probe,
	}, {
		.compatible = "mediatek,mt6877-imgsys2",
		.data = clk_mt6877_imgsys2_probe,
	}, {
		/* sentinel */
	}
};


static int clk_mt6877_img_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6877_img_drv = {
	.probe = clk_mt6877_img_probe,
	.driver = {
		.name = "clk-mt6877-img",
		.of_match_table = of_match_clk_mt6877_img,
	},
};

static int __init clk_mt6877_img_init(void)
{
	return platform_driver_register(&clk_mt6877_img_drv);
}
arch_initcall(clk_mt6877_img_init);

