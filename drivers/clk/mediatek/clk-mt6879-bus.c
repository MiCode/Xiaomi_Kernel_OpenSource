// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.
// Author: Owen Chen <owen.chen@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6879-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ifrao0_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifrao1_cg_regs = {
	.set_ofs = 0x88,
	.clr_ofs = 0x8C,
	.sta_ofs = 0x94,
};

static const struct mtk_gate_regs ifrao2_cg_regs = {
	.set_ofs = 0xA4,
	.clr_ofs = 0xA8,
	.sta_ofs = 0xAC,
};

static const struct mtk_gate_regs ifrao3_cg_regs = {
	.set_ofs = 0xC0,
	.clr_ofs = 0xC4,
	.sta_ofs = 0xC8,
};

static const struct mtk_gate_regs ifrao4_cg_regs = {
	.set_ofs = 0xE0,
	.clr_ofs = 0xE4,
	.sta_ofs = 0xE8,
};

#define GATE_IFRAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO3(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao3_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO4(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao4_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

static const struct mtk_gate ifrao_clks[] = {
	/* IFRAO0 */
	GATE_IFRAO0(CLK_IFRAO_THERM, "ifrao_therm",
			"axi_ck"/* parent */, 10),
	/* IFRAO1 */
	GATE_IFRAO1(CLK_IFRAO_TRNG, "ifrao_trng",
			"axi_ck"/* parent */, 9),
	GATE_IFRAO1(CLK_IFRAO_CPUM, "ifrao_cpum",
			"axi_ck"/* parent */, 11),
	GATE_IFRAO1(CLK_IFRAO_CCIF1_AP, "ifrao_ccif1_ap",
			"axi_ck"/* parent */, 12),
	GATE_IFRAO1(CLK_IFRAO_CCIF1_MD, "ifrao_ccif1_md",
			"axi_ck"/* parent */, 13),
	GATE_IFRAO1(CLK_IFRAO_CCIF_AP, "ifrao_ccif_ap",
			"axi_ck"/* parent */, 23),
	GATE_IFRAO1(CLK_IFRAO_DEBUGSYS, "ifrao_debugsys",
			"axi_ck"/* parent */, 24),
	GATE_IFRAO1(CLK_IFRAO_CCIF_MD, "ifrao_ccif_md",
			"axi_ck"/* parent */, 26),
	GATE_IFRAO1(CLK_IFRAO_DXCC_SEC_CORE, "ifrao_secore",
			"dxcc_ck"/* parent */, 27),
	GATE_IFRAO1(CLK_IFRAO_DXCC_AO, "ifrao_dxcc_ao",
			"dxcc_ck"/* parent */, 28),
	GATE_IFRAO1(CLK_IFRAO_DBG_TRACE, "ifrao_dbg_trace",
			"axi_ck"/* parent */, 29),
	/* IFRAO2 */
	GATE_IFRAO2(CLK_IFRAO_CLDMA_BCLK, "ifrao_cldmabclk",
			"axi_ck"/* parent */, 3),
	GATE_IFRAO2(CLK_IFRAO_CQ_DMA, "ifrao_cq_dma",
			"axi_ck"/* parent */, 27),
	/* IFRAO3 */
	GATE_IFRAO3(CLK_IFRAO_CCIF5_AP, "ifrao_ccif5_ap",
			"axi_ck"/* parent */, 9),
	GATE_IFRAO3(CLK_IFRAO_CCIF5_MD, "ifrao_ccif5_md",
			"axi_ck"/* parent */, 10),
	GATE_IFRAO3(CLK_IFRAO_CCIF2_AP, "ifrao_ccif2_ap",
			"axi_ck"/* parent */, 16),
	GATE_IFRAO3(CLK_IFRAO_CCIF2_MD, "ifrao_ccif2_md",
			"axi_ck"/* parent */, 17),
	GATE_IFRAO3(CLK_IFRAO_CCIF3_AP, "ifrao_ccif3_ap",
			"axi_ck"/* parent */, 18),
	GATE_IFRAO3(CLK_IFRAO_CCIF3_MD, "ifrao_ccif3_md",
			"axi_ck"/* parent */, 19),
	GATE_IFRAO3(CLK_IFRAO_FBIST2FPC, "ifrao_fbist2fpc",
			"msdc_macro_ck"/* parent */, 24),
	GATE_IFRAO3(CLK_IFRAO_DEVICE_APC_SYNC, "ifrao_dapc_sync",
			"axi_ck"/* parent */, 25),
	GATE_IFRAO3(CLK_IFRAO_DPMAIF_MAIN, "ifrao_dpmaif_main",
			"dpmaif_main_ck"/* parent */, 26),
	GATE_IFRAO3(CLK_IFRAO_CCIF4_AP, "ifrao_ccif4_ap",
			"axi_ck"/* parent */, 28),
	GATE_IFRAO3(CLK_IFRAO_CCIF4_MD, "ifrao_ccif4_md",
			"axi_ck"/* parent */, 29),
	/* IFRAO4 */
	GATE_IFRAO4(CLK_IFRAO_RG_MMW_DPMAIF26M_CK, "ifrao_dpmaif_26m",
			"f26m_ck"/* parent */, 17),
	GATE_IFRAO4(CLK_IFRAO_RG_MEM_SUB_CK, "ifrao_mem_sub_ck",
			"mem_sub_ck"/* parent */, 29),
};

static const struct mtk_gate_regs nemi_reg_cg_regs = {
	.set_ofs = 0x858,
	.clr_ofs = 0x858,
	.sta_ofs = 0x858,
};

#define GATE_NEMI_REG(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &nemi_reg_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate nemi_reg_clks[] = {
	GATE_NEMI_REG(CLK_NEMI_REG_BUS_MON_MODE, "nemi_bus_mon_mode",
			"emi_n_ck"/* parent */, 11),
};

static int clk_mt6879_ifrao_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IFRAO_NR_CLK);

	mtk_clk_register_gates(node, ifrao_clks, ARRAY_SIZE(ifrao_clks),
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

static int clk_mt6879_nemi_reg_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_NEMI_REG_NR_CLK);

	mtk_clk_register_gates(node, nemi_reg_clks, ARRAY_SIZE(nemi_reg_clks),
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

static const struct of_device_id of_match_clk_mt6879_bus[] = {
	{
		.compatible = "mediatek,mt6879-infracfg_ao",
		.data = clk_mt6879_ifrao_probe,
	}, {
		.compatible = "mediatek,mt6879-nemi_reg",
		.data = clk_mt6879_nemi_reg_probe,
	}, {
		/* sentinel */
	}
};


static int clk_mt6879_bus_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6879_bus_drv = {
	.probe = clk_mt6879_bus_grp_probe,
	.driver = {
		.name = "clk-mt6879-bus",
		.of_match_table = of_match_clk_mt6879_bus,
	},
};

static int __init clk_mt6879_bus_init(void)
{
	return platform_driver_register(&clk_mt6879_bus_drv);
}

static void __exit clk_mt6879_bus_exit(void)
{
	platform_driver_unregister(&clk_mt6879_bus_drv);
}

arch_initcall(clk_mt6879_bus_init);
module_exit(clk_mt6879_bus_exit);
MODULE_LICENSE("GPL");
