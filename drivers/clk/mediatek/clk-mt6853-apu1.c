// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#include <linux/clk-provider.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6853-clk.h>

#define MT_CLKMGR_MODULE_INIT	0

#define MT_CCF_BRINGUP		1

#define INV_OFS			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status pwr_stat = GATE_PWR_STAT(INV_OFS,
		INV_OFS, 0x0178, BIT(5), BIT(5));

static const struct mtk_gate_regs apu10_cg_regs = {
	.set_ofs = 0x104,
	.clr_ofs = 0x108,
	.sta_ofs = 0x100,
};

static const struct mtk_gate_regs apu11_cg_regs = {
	.set_ofs = 0x910,
	.clr_ofs = 0x910,
	.sta_ofs = 0x910,
};

#define GATE_APU10(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu10_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

#define GATE_APU11(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &apu11_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
		.pwr_stat = &pwr_stat,			\
	}

static const struct mtk_gate apu1_clks[] = {
	/* APU10 */
	GATE_APU10(CLK_APU1_APU, "apu1_apu",
			"dsp2_ck"/* parent */, 0),
	GATE_APU10(CLK_APU1_AXI_M, "apu1_axi_m",
			"dsp2_ck"/* parent */, 1),
	GATE_APU10(CLK_APU1_JTAG, "apu1_jtag",
			"dsp2_ck"/* parent */, 2),
	/* APU11 */
	GATE_APU11(CLK_APU1_PCLK, "apu1_pclk",
			"dsp2_ck"/* parent */, 25),
};

static int clk_mt6853_apu1_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_APU1_NR_CLK);

	mtk_clk_register_gates(node, apu1_clks, ARRAY_SIZE(apu1_clks),
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

static const struct of_device_id of_match_clk_mt6853_apu1[] = {
	{ .compatible = "mediatek,mt6853-apu1", },
	{}
};

#if MT_CLKMGR_MODULE_INIT

static struct platform_driver clk_mt6853_apu1_drv = {
	.probe = clk_mt6853_apu1_probe,
	.driver = {
		.name = "clk-mt6853-apu1",
		.of_match_table = of_match_clk_mt6853_apu1,
	},
};

builtin_platform_driver(clk_mt6853_apu1_drv);

#else

static struct platform_driver clk_mt6853_apu1_drv = {
	.probe = clk_mt6853_apu1_probe,
	.driver = {
		.name = "clk-mt6853-apu1",
		.of_match_table = of_match_clk_mt6853_apu1,
	},
};
static int __init clk_mt6853_apu1_platform_init(void)
{
	return platform_driver_register(&clk_mt6853_apu1_drv);
}
arch_initcall(clk_mt6853_apu1_platform_init);

#endif	/* MT_CLKMGR_MODULE_INIT */
