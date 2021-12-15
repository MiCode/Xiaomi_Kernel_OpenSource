// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

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
	GATE_IMPC(CLK_IMPC_AP_I2C0_RO, "impc_ap_i2c0_ro",
			"i2c_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_AP_I2C10_RO, "impc_ap_i2c10_ro",
			"i2c_ck"/* parent */, 1),
	GATE_IMPC(CLK_IMPC_AP_I2C11_RO, "impc_ap_i2c11_ro",
			"i2c_ck"/* parent */, 2),
	GATE_IMPC(CLK_IMPC_AP_I2C12_RO, "impc_ap_i2c12_ro",
			"i2c_ck"/* parent */, 3),
	GATE_IMPC(CLK_IMPC_AP_I2C13_RO, "impc_ap_i2c13_ro",
			"i2c_ck"/* parent */, 4),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
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
	GATE_IMPE(CLK_IMPE_AP_I2C3_RO, "impe_ap_i2c3_ro",
			"i2c_ck"/* parent */, 0),
	GATE_IMPE(CLK_IMPE_AP_I2C9_RO, "impe_ap_i2c9_ro",
			"i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
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
	GATE_IMPN(CLK_IMPN_AP_I2C5_RO, "impn_ap_i2c5_ro",
			"i2c_ck"/* parent */, 0),
	GATE_IMPN(CLK_IMPN_AP_I2C6_RO, "impn_ap_i2c6_ro",
			"i2c_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impn_mcd = {
	.clks = impn_clks,
	.num_clks = CLK_IMPN_NR_CLK,
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
	GATE_IMPS(CLK_IMPS_AP_I2C1_RO, "imps_ap_i2c1_ro",
			"i2c_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_AP_I2C2_RO, "imps_ap_i2c2_ro",
			"i2c_ck"/* parent */, 1),
	GATE_IMPS(CLK_IMPS_AP_I2C4_RO, "imps_ap_i2c4_ro",
			"i2c_ck"/* parent */, 2),
	GATE_IMPS(CLK_IMPS_AP_I2C7_RO, "imps_ap_i2c7_ro",
			"i2c_ck"/* parent */, 3),
	GATE_IMPS(CLK_IMPS_AP_I2C8_RO, "imps_ap_i2c8_ro",
			"i2c_ck"/* parent */, 4),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6893_i2c[] = {
	{
		.compatible = "mediatek,mt6893-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6893-imp_iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt6893-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6893-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6893_i2c_grp_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init begin\n", __func__, pdev->name);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s: %s init end\n", __func__, pdev->name);
#endif

	return r;
}

static struct platform_driver clk_mt6893_i2c_drv = {
	.probe = clk_mt6893_i2c_grp_probe,
	.driver = {
		.name = "clk-mt6893-i2c",
		.of_match_table = of_match_clk_mt6893_i2c,
	},
};

static int __init clk_mt6893_i2c_init(void)
{
	return platform_driver_register(&clk_mt6893_i2c_drv);
}

static void __exit clk_mt6893_i2c_exit(void)
{
	platform_driver_unregister(&clk_mt6893_i2c_drv);
}

postcore_initcall(clk_mt6893_i2c_init);
module_exit(clk_mt6893_i2c_exit);
MODULE_LICENSE("GPL");
