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

static const struct mtk_gate_regs imgsys_main_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IMGSYS_MAIN_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &imgsys_main_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}

#define GATE_IMGSYS_MAIN_0_DUMMYS(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &imgsys_main_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_dummys,			\
	}

static struct mtk_gate imgsys_main_clks[] = {
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_LARB9 /* CLK ID */,
		"imgsys_main_larb9" /* name */,
		"img1_ck" /* parent */, 0 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_TRAW0 /* CLK ID */,
		"imgsys_main_traw0" /* name */,
		"img1_ck" /* parent */, 1 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_TRAW1 /* CLK ID */,
		"imgsys_main_traw1" /* name */,
		"img1_ck" /* parent */, 2 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_VCORE_GALS /* CLK ID */,
		"imgsys_main_vcore_gals" /* name */,
		"img1_ck" /* parent */, 3 /* bit */),
	GATE_IMGSYS_MAIN_0_DUMMYS(CLK_IMGSYS_MAIN_DIP0 /* CLK ID */,
		"imgsys_main_dip0" /* name */,
		"img1_ck" /* parent */, 8 /* bit */),
	GATE_IMGSYS_MAIN_0_DUMMYS(CLK_IMGSYS_MAIN_WPE0 /* CLK ID */,
		"imgsys_main_wpe0" /* name */,
		"img1_ck" /* parent */, 9 /* bit */),
	GATE_IMGSYS_MAIN_0_DUMMYS(CLK_IMGSYS_MAIN_IPE /* CLK ID */,
		"imgsys_main_ipe" /* name */,
		"img1_ck" /* parent */, 10 /* bit */),
	GATE_IMGSYS_MAIN_0_DUMMYS(CLK_IMGSYS_MAIN_WPE1 /* CLK ID */,
		"imgsys_main_wpe1" /* name */,
		"img1_ck" /* parent */, 12 /* bit */),
	GATE_IMGSYS_MAIN_0_DUMMYS(CLK_IMGSYS_MAIN_WPE2 /* CLK ID */,
		"imgsys_main_wpe2" /* name */,
		"img1_ck" /* parent */, 13 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_DIP0T /* CLK ID */,
		"imgsys_main_dip0t" /* name */,
		"img1_ck" /* parent */, 8 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_WPE0T /* CLK ID */,
		"imgsys_main_wpe0t" /* name */,
		"img1_ck" /* parent */, 9 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_IPET /* CLK ID */,
		"imgsys_main_ipet" /* name */,
		"img1_ck" /* parent */, 10 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_WPE1T /* CLK ID */,
		"imgsys_main_wpe1t" /* name */,
		"img1_ck" /* parent */, 12 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_WPE2T /* CLK ID */,
		"imgsys_main_wpe2t" /* name */,
		"img1_ck" /* parent */, 13 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_ADL_LARB /* CLK ID */,
		"imgsys_main_adl_larb" /* name */,
		"img1_ck" /* parent */, 14 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_ADL_TOP0 /* CLK ID */,
		"imgsys_main_adl_top0" /* name */,
		"img1_ck" /* parent */, 15 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_ADL_TOP1 /* CLK ID */,
		"imgsys_main_adl_top1" /* name */,
		"img1_ck" /* parent */, 16 /* bit */),
	GATE_IMGSYS_MAIN_0(CLK_IMGSYS_MAIN_GALS /* CLK ID */,
		"imgsys_main_gals" /* name */,
		"img1_ck" /* parent */, 31 /* bit */),
};

static int clk_mt6983_imgsys_main_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_IMGSYS_MAIN_NR_CLK);

	mtk_clk_register_gates(node, imgsys_main_clks,
		ARRAY_SIZE(imgsys_main_clks),
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

static const struct mtk_gate_regs dip_nr_dip1_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_NR_DIP1_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dip_nr_dip1_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate dip_nr_dip1_clks[] = {
	GATE_DIP_NR_DIP1_0(CLK_DIP_NR_DIP1_LARB15 /* CLK ID */,
		"dip_nr_dip1_larb15" /* name */,
		"img1_ck" /* parent */, 0 /* bit */),
	GATE_DIP_NR_DIP1_0(CLK_DIP_NR_DIP1_DIP_NR /* CLK ID */,
		"dip_nr_dip1_dip_nr" /* name */,
		"img1_ck" /* parent */, 1 /* bit */),
};

static int clk_mt6983_dip_nr_dip1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_DIP_NR_DIP1_NR_CLK);

	mtk_clk_register_gates(node, dip_nr_dip1_clks,
		ARRAY_SIZE(dip_nr_dip1_clks),
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

static const struct mtk_gate_regs dip_top_dip1_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_DIP_TOP_DIP1_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &dip_top_dip1_0_cg_regs,			\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate dip_top_dip1_clks[] = {
	GATE_DIP_TOP_DIP1_0(CLK_DIP_TOP_DIP1_LARB10 /* CLK ID */,
		"dip_dip1_larb10" /* name */,
		"img1_ck" /* parent */, 0 /* bit */),
	GATE_DIP_TOP_DIP1_0(CLK_DIP_TOP_DIP1_DIP_TOP /* CLK ID */,
		"dip_dip1_dip_top" /* name */,
		"img1_ck" /* parent */, 1 /* bit */),
};

static int clk_mt6983_dip_top_dip1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_DIP_TOP_DIP1_NR_CLK);

	mtk_clk_register_gates(node, dip_top_dip1_clks,
		ARRAY_SIZE(dip_top_dip1_clks),
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

static const struct mtk_gate_regs ipesys_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_IPESYS_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ipesys_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate ipesys_clks[] = {
	GATE_IPESYS_0(CLK_IPESYS_DPE /* CLK ID */,
		"ipesys_dpe" /* name */,
		"ipe_ck" /* parent */, 0 /* bit */),
	GATE_IPESYS_0(CLK_IPESYS_FDVT /* CLK ID */,
		"ipesys_fdvt" /* name */,
		"ipe_ck" /* parent */, 1 /* bit */),
	GATE_IPESYS_0(CLK_IPESYS_ME /* CLK ID */,
		"ipesys_me" /* name */,
		"ipe_ck" /* parent */, 2 /* bit */),
	GATE_IPESYS_0(CLK_IPESYS_IPESYS_TOP /* CLK ID */,
		"ipesys_ipesys_top" /* name */,
		"ipe_ck" /* parent */, 3 /* bit */),
	GATE_IPESYS_0(CLK_IPESYS_SMI_LARB12 /* CLK ID */,
		"ipesys_smi_larb12" /* name */,
		"ipe_ck" /* parent */, 4 /* bit */),
	GATE_IPESYS_0(CLK_IPESYS_FDVT1 /* CLK ID */,
		"ipesys_fdvt1" /* name */,
		"ipe_ck" /* parent */, 5 /* bit */),
};

static int clk_mt6983_ipesys_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_IPESYS_NR_CLK);

	mtk_clk_register_gates(node, ipesys_clks,
		ARRAY_SIZE(ipesys_clks),
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

static const struct mtk_gate_regs wpe1_dip1_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE1_DIP1_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &wpe1_dip1_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate wpe1_dip1_clks[] = {
	GATE_WPE1_DIP1_0(CLK_WPE1_DIP1_LARB11 /* CLK ID */,
		"wpe1_dip1_larb11" /* name */,
		"img1_ck" /* parent */, 0 /* bit */),
	GATE_WPE1_DIP1_0(CLK_WPE1_DIP1_WPE /* CLK ID */,
		"wpe1_dip1_wpe" /* name */,
		"img1_ck" /* parent */, 1 /* bit */),
};

static int clk_mt6983_wpe1_dip1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_WPE1_DIP1_NR_CLK);

	mtk_clk_register_gates(node, wpe1_dip1_clks,
		ARRAY_SIZE(wpe1_dip1_clks),
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

static const struct mtk_gate_regs wpe2_dip1_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE2_DIP1_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &wpe2_dip1_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate wpe2_dip1_clks[] = {
	GATE_WPE2_DIP1_0(CLK_WPE2_DIP1_LARB11 /* CLK ID */,
		"wpe2_dip1_larb11" /* name */,
		"img1_ck" /* parent */, 0 /* bit */),
	GATE_WPE2_DIP1_0(CLK_WPE2_DIP1_WPE /* CLK ID */,
		"wpe2_dip1_wpe" /* name */,
		"img1_ck" /* parent */, 1 /* bit */),
};

static int clk_mt6983_wpe2_dip1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_WPE2_DIP1_NR_CLK);

	mtk_clk_register_gates(node, wpe2_dip1_clks,
		ARRAY_SIZE(wpe2_dip1_clks),
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

static const struct mtk_gate_regs wpe3_dip1_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

#define GATE_WPE3_DIP1_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &wpe3_dip1_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr,			\
	}
static struct mtk_gate wpe3_dip1_clks[] = {
	GATE_WPE3_DIP1_0(CLK_WPE3_DIP1_LARB11 /* CLK ID */,
		"wpe3_dip1_larb11" /* name */,
		"img1_ck" /* parent */, 0 /* bit */),
	GATE_WPE3_DIP1_0(CLK_WPE3_DIP1_WPE /* CLK ID */,
		"wpe3_dip1_wpe" /* name */,
		"img1_ck" /* parent */, 1 /* bit */),
};

static int clk_mt6983_wpe3_dip1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_WPE3_DIP1_NR_CLK);

	mtk_clk_register_gates(node, wpe3_dip1_clks,
		ARRAY_SIZE(wpe3_dip1_clks),
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

static const struct of_device_id of_match_clk_mt6983_img[] = {
	{
		.compatible = "mediatek,mt6983-imgsys_main",
		.data = clk_mt6983_imgsys_main_probe,
	}, {
		.compatible = "mediatek,mt6983-dip_nr_dip1",
		.data = clk_mt6983_dip_nr_dip1_probe,
	}, {
		.compatible = "mediatek,mt6983-dip_top_dip1",
		.data = clk_mt6983_dip_top_dip1_probe,
	}, {
		.compatible = "mediatek,mt6983-ipesys",
		.data = clk_mt6983_ipesys_probe,
	}, {
		.compatible = "mediatek,mt6983-wpe1_dip1",
		.data = clk_mt6983_wpe1_dip1_probe,
	}, {
		.compatible = "mediatek,mt6983-wpe2_dip1",
		.data = clk_mt6983_wpe2_dip1_probe,
	}, {
		.compatible = "mediatek,mt6983-wpe3_dip1",
		.data = clk_mt6983_wpe3_dip1_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6983_img_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6983_img_drv = {
	.probe = clk_mt6983_img_probe,
	.driver = {
		.name = "clk-mt6983-img",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_img,
	},
};

static int __init clk_mt6983_img_init(void)
{
	return platform_driver_register(&clk_mt6983_img_drv);
}

static void __exit clk_mt6983_img_exit(void)
{
	platform_driver_unregister(&clk_mt6983_img_drv);
}

arch_initcall(clk_mt6983_img_init);
module_exit(clk_mt6983_img_exit);
MODULE_LICENSE("GPL");

