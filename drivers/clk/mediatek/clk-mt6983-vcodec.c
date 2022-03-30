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

static const struct mtk_gate_regs vde1_base_0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde1_base_1_cg_regs = {
	.set_ofs = 0x190,
	.clr_ofs = 0x190,
	.sta_ofs = 0x190,
};

static const struct mtk_gate_regs vde1_base_2_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde1_base_3_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs vde10_base_hwv_regs = {
	.set_ofs = 0xF0,
	.clr_ofs = 0xF4,
	.sta_ofs = 0x1C78,
};

static const struct mtk_gate_regs vde11_base_hwv_regs = {
	.set_ofs = 0x100,
	.clr_ofs = 0x104,
	.sta_ofs = 0x1C80,
};

static const struct mtk_gate_regs vde12_base_hwv_regs = {
	.set_ofs = 0x110,
	.clr_ofs = 0x114,
	.sta_ofs = 0x1C88,
};

static const struct mtk_gate_regs vde13_base_hwv_regs = {
	.set_ofs = 0x120,
	.clr_ofs = 0x124,
	.sta_ofs = 0x1C90,
};

#define GATE_VDE1_BASE_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}
#define GATE_VDE1_BASE_1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_1_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,			\
	}
#define GATE_VDE1_BASE_2(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_2_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}
#define GATE_VDE1_BASE_3(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_3_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}
#define GATE_VDE1_BASE_3_DUMMY(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_3_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,			\
	}
#define GATE_HWV_VDE10_BASE(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_3_cg_regs,				\
		.hwv_regs = &vde10_base_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
#define GATE_HWV_VDE11_BASE(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_1_cg_regs,				\
		.hwv_regs = &vde11_base_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
#define GATE_HWV_VDE12_BASE(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_2_cg_regs,				\
		.hwv_regs = &vde12_base_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
#define GATE_HWV_VDE13_BASE(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde1_base_3_cg_regs,				\
		.hwv_regs = &vde13_base_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
static struct mtk_gate vde1_base_clks[] = {
	GATE_VDE1_BASE_0(CLK_VDE1_BASE_VDEC_CKEN /* CLK ID */,
		"vde1_base_vdec_cken" /* name */,
		"vdec_ck" /* parent */, 0 /* bit */),
	GATE_VDE1_BASE_0(CLK_VDE1_BASE_VDEC_ACTIVE /* CLK ID */,
		"vde1_base_vdec_active" /* name */,
		"vdec_ck" /* parent */, 4 /* bit */),
	GATE_VDE1_BASE_0(CLK_VDE1_BASE_VDEC_CKEN_ENG /* CLK ID */,
		"vde1_base_vdec_cken_eng" /* name */,
		"vdec_ck" /* parent */, 8 /* bit */),
	GATE_VDE1_BASE_1(CLK_VDE1_BASE_MINI_MDP_CKEN /* CLK ID */,
		"vde1_base_mini_mdp_cken" /* name */,
		"vdec_ck" /* parent */, 0 /* bit */),
	GATE_VDE1_BASE_2(CLK_VDE1_BASE_LAT_CKEN /* CLK ID */,
		"vde1_base_lat_cken" /* name */,
		"vdec_ck" /* parent */, 0 /* bit */),
	GATE_VDE1_BASE_2(CLK_VDE1_BASE_LAT_ACTIVE /* CLK ID */,
		"vde1_base_lat_active" /* name */,
		"vdec_ck" /* parent */, 4 /* bit */),
	GATE_VDE1_BASE_2(CLK_VDE1_BASE_LAT_CKEN_ENG /* CLK ID */,
		"vde1_base_lat_cken_eng" /* name */,
		"vdec_ck" /* parent */, 8 /* bit */),
	GATE_VDE1_BASE_3(CLK_VDE1_BASE_LARB1_CKEN /* CLK ID */,
		"vde1_base_larb1_cken" /* name */,
		"vdec_ck" /* parent */, 0 /* bit */),
};

static int clk_mt6983_vde1_base_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_VDE1_BASE_NR_CLK);

	mtk_clk_register_gates(node, vde1_base_clks,
		ARRAY_SIZE(vde1_base_clks),
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

static const struct mtk_gate_regs vde2_base_0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x4,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs vde2_base_1_cg_regs = {
	.set_ofs = 0x200,
	.clr_ofs = 0x204,
	.sta_ofs = 0x200,
};

static const struct mtk_gate_regs vde2_base_2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0xC,
	.sta_ofs = 0x8,
};

static const struct mtk_gate_regs vde20_base_hwv_regs = {
	.set_ofs = 0xD0,
	.clr_ofs = 0xD4,
	.sta_ofs = 0x1C68,
};

static const struct mtk_gate_regs vde21_base_hwv_regs = {
	.set_ofs = 0xE0,
	.clr_ofs = 0xE4,
	.sta_ofs = 0x1C70,
};

#define GATE_VDE2_BASE_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde2_base_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}
#define GATE_VDE2_BASE_1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde2_base_1_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}
#define GATE_VDE2_BASE_2(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde2_base_2_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}
#define GATE_VDE2_BASE_2_DUMMY(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde2_base_2_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv_dummy,			\
	}
#define GATE_HWV_VDE20_BASE(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde2_base_2_cg_regs,				\
		.hwv_regs = &vde20_base_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
#define GATE_HWV_VDE21_BASE(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &vde2_base_1_cg_regs,				\
		.hwv_regs = &vde2_base_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}
static struct mtk_gate vde2_base_clks[] = {
	GATE_VDE2_BASE_0(CLK_VDE2_BASE_VDEC_CKEN /* CLK ID */,
		"vde2_base_vdec_cken" /* name */,
		"vdec_ck" /* parent */, 0 /* bit */),
	GATE_VDE2_BASE_0(CLK_VDE2_BASE_VDEC_ACTIVE /* CLK ID */,
		"vde2_base_vdec_active" /* name */,
		"vdec_ck" /* parent */, 4 /* bit */),
	GATE_VDE2_BASE_0(CLK_VDE2_BASE_VDEC_CKEN_ENG /* CLK ID */,
		"vde2_base_vdec_cken_eng" /* name */,
		"vdec_ck" /* parent */, 8 /* bit */),
	GATE_VDE2_BASE_1(CLK_VDE2_BASE_LAT_CKEN /* CLK ID */,
		"vde2_base_lat_cken" /* name */,
		"vdec_ck" /* parent */, 0 /* bit */),
	GATE_VDE2_BASE_1(CLK_VDE2_BASE_LAT_ACTIVE /* CLK ID */,
		"vde2_base_lat_active" /* name */,
		"vdec_ck" /* parent */, 4 /* bit */),
	GATE_VDE2_BASE_1(CLK_VDE2_BASE_LAT_CKEN_ENG /* CLK ID */,
		"vde2_base_lat_cken_eng" /* name */,
		"vdec_ck" /* parent */, 8 /* bit */),
	GATE_VDE2_BASE_2(CLK_VDE2_BASE_LARB1_CKEN /* CLK ID */,
		"vde2_base_larb1_cken" /* name */,
		"vdec_ck" /* parent */, 0 /* bit */),
};

static int clk_mt6983_vde2_base_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_VDE2_BASE_NR_CLK);

	mtk_clk_register_gates(node, vde2_base_clks,
		ARRAY_SIZE(vde2_base_clks),
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

static const struct mtk_gate_regs ven1_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven1_hwv_regs = {
	.set_ofs = 0x130,
	.clr_ofs = 0x134,
	.sta_ofs = 0x1C98,
};

#define GATE_VEN1_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ven1_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}

#define GATE_HWV_VEN1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ven1_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}

static struct mtk_gate ven1_clks[] = {
	GATE_VEN1_0(CLK_VEN1_CKE0_LARB /* CLK ID */,
		"ven1_cke0_larb" /* name */,
		"venc_ck" /* parent */, 0 /* bit */),
	GATE_VEN1_0(CLK_VEN1_CKE1_VENC /* CLK ID */,
		"ven1_cke1_venc" /* name */,
		"venc_ck" /* parent */, 4 /* bit */),
	GATE_VEN1_0(CLK_VEN1_CKE2_JPGENC /* CLK ID */,
		"ven1_cke2_jpgenc" /* name */,
		"venc_ck" /* parent */, 8 /* bit */),
	GATE_VEN1_0(CLK_VEN1_CKE3_JPGDEC /* CLK ID */,
		"ven1_cke3_jpgdec" /* name */,
		"venc_ck" /* parent */, 12 /* bit */),
	GATE_VEN1_0(CLK_VEN1_CKE4_JPGDEC_C1 /* CLK ID */,
		"ven1_cke4_jpgdec_c1" /* name */,
		"venc_ck" /* parent */, 16 /* bit */),
	GATE_VEN1_0(CLK_VEN1_CKE5_GALS /* CLK ID */,
		"ven1_cke5_gals" /* name */,
		"venc_ck" /* parent */, 28 /* bit */),
	GATE_VEN1_0(CLK_VEN1_CKE6_GALS_SRAM /* CLK ID */,
		"ven1_cke6_gals_sram" /* name */,
		"venc_ck" /* parent */, 31 /* bit */),
};

static int clk_mt6983_ven1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_VEN1_NR_CLK);

	mtk_clk_register_gates(node, ven1_clks,
		ARRAY_SIZE(ven1_clks),
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

static const struct mtk_gate_regs ven1_core1_0_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x8,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs ven1_core1_hwv_regs = {
	.set_ofs = 0x140,
	.clr_ofs = 0x144,
	.sta_ofs = 0x1CA0,
};

#define GATE_VEN1_CORE1_0(_id, _name, _parent, _shift) {		\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ven1_core1_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_setclr_inv,			\
	}

#define GATE_HWV_VEN1_CORE1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &ven1_core1_hwv_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_hwv,			\
		.flags = CLK_USE_HW_VOTER,				\
	}

static struct mtk_gate ven1_core1_clks[] = {
	GATE_VEN1_CORE1_0(CLK_VEN1_CORE1_CKE0_LARB /* CLK ID */,
		"ven1_core1_cke0_larb" /* name */,
		"venc_ck" /* parent */, 0 /* bit */),
	GATE_VEN1_CORE1_0(CLK_VEN1_CORE1_CKE1_VENC /* CLK ID */,
		"ven1_core1_cke1_venc" /* name */,
		"venc_ck" /* parent */, 4 /* bit */),
	GATE_VEN1_CORE1_0(CLK_VEN1_CORE1_CKE2_JPGENC /* CLK ID */,
		"ven1_core1_cke2_jpgenc" /* name */,
		"venc_ck" /* parent */, 8 /* bit */),
	GATE_VEN1_CORE1_0(CLK_VEN1_CORE1_CKE3_JPGDEC /* CLK ID */,
		"ven1_core1_cke3_jpgdec" /* name */,
		"venc_ck" /* parent */, 12 /* bit */),
	GATE_VEN1_CORE1_0(CLK_VEN1_CORE1_CKE4_JPGDEC_C1 /* CLK ID */,
		"ven1_core1_cke4_jpgdec_c1" /* name */,
		"venc_ck" /* parent */, 16 /* bit */),
	GATE_VEN1_CORE1_0(CLK_VEN1_CORE1_CKE5_GALS /* CLK ID */,
		"ven1_core1_cke5_gals" /* name */,
		"venc_ck" /* parent */, 28 /* bit */),
	GATE_VEN1_CORE1_0(CLK_VEN1_CORE1_CKE6_GALS_SRAM /* CLK ID */,
		"ven1_core1_cke6_gals_sram" /* name */,
		"venc_ck" /* parent */, 31 /* bit */),
};

static int clk_mt6983_ven1_core1_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_VEN1_CORE1_NR_CLK);

	mtk_clk_register_gates(node, ven1_core1_clks,
		ARRAY_SIZE(ven1_core1_clks),
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

static const struct of_device_id of_match_clk_mt6983_vcodec[] = {
	{
		.compatible = "mediatek,mt6983-vdec_soc_gcon_base",
		.data = clk_mt6983_vde1_base_probe,
	}, {
		.compatible = "mediatek,mt6983-vdec_gcon_base",
		.data = clk_mt6983_vde2_base_probe,
	}, {
		.compatible = "mediatek,mt6983-venc_gcon",
		.data = clk_mt6983_ven1_probe,
	}, {
		.compatible = "mediatek,mt6983-venc_gcon_core1",
		.data = clk_mt6983_ven1_core1_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6983_vcodec_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6983_vcodec_drv = {
	.probe = clk_mt6983_vcodec_probe,
	.driver = {
		.name = "clk-mt6983-vcodec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_vcodec,
	},
};

static int __init clk_mt6983_vcodec_init(void)
{
	return platform_driver_register(&clk_mt6983_vcodec_drv);
}

static void __exit clk_mt6983_vcodec_exit(void)
{
	platform_driver_unregister(&clk_mt6983_vcodec_drv);
}

arch_initcall(clk_mt6983_vcodec_init);
module_exit(clk_mt6983_vcodec_exit);
MODULE_LICENSE("GPL");

