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

#include <dt-bindings/clock/mt6833-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs impc_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
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
	GATE_IMPC(CLK_IMPC_I2C10_AP_CLOCK, "impc_i2c10_ap_clock",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPC(CLK_IMPC_I2C11_AP_CLOCK, "impc_i2c11_ap_clock",
			"i2c_pseudo_ck"/* parent */, 1),
};

static const struct mtk_clk_desc impc_mcd = {
	.clks = impc_clks,
	.num_clks = CLK_IMPC_NR_CLK,
};

static const struct mtk_gate_regs impe_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
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
	GATE_IMPE(CLK_IMPE_I2C3_AP_CLOCK, "impe_i2c3_ap_clock",
			"i2c_pseudo_ck"/* parent */, 0),
};

static const struct mtk_clk_desc impe_mcd = {
	.clks = impe_clks,
	.num_clks = CLK_IMPE_NR_CLK,
};

static const struct mtk_gate_regs impn_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
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
	GATE_IMPN(CLK_IMPN_I2C6_AP_CLOCK, "impn_i2c6_ap_clock",
			"i2c_pseudo_ck"/* parent */, 0),
};

static const struct mtk_clk_desc impn_mcd = {
	.clks = impn_clks,
	.num_clks = CLK_IMPN_NR_CLK,
};

static const struct mtk_gate_regs imps_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
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
	GATE_IMPS(CLK_IMPS_I3C9_AP_CLOCK, "imps_i3c9_ap_clock",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPS(CLK_IMPS_I3C8_AP_CLOCK, "imps_i3c8_ap_clock",
			"i2c_pseudo_ck"/* parent */, 1),
};

static const struct mtk_clk_desc imps_mcd = {
	.clks = imps_clks,
	.num_clks = CLK_IMPS_NR_CLK,
};

static const struct mtk_gate_regs impw_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
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
	GATE_IMPW(CLK_IMPW_I2C0_AP_CLOCK, "impw_i2c0_ap_clock",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPW(CLK_IMPW_I2C5_AP_CLOCK, "impw_i2c5_ap_clock",
			"i2c_pseudo_ck"/* parent */, 1),
	GATE_IMPW(CLK_IMPW_I3C7_AP_CLOCK, "impw_i3c7_ap_clock",
			"i2c_pseudo_ck"/* parent */, 2),
};

static const struct mtk_clk_desc impw_mcd = {
	.clks = impw_clks,
	.num_clks = CLK_IMPW_NR_CLK,
};

static const struct mtk_gate_regs impws_cg_regs = {
	.set_ofs = 0xE08,
	.clr_ofs = 0xE04,
	.sta_ofs = 0xE00,
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
	GATE_IMPWS(CLK_IMPWS_I2C1_AP_CLOCK, "impws_i2c1_ap_clock",
			"i2c_pseudo_ck"/* parent */, 0),
	GATE_IMPWS(CLK_IMPWS_I3C2_AP_CLOCK, "impws_i3c2_ap_clock",
			"i2c_pseudo_ck"/* parent */, 1),
	GATE_IMPWS(CLK_IMPWS_I3C4_AP_CLOCK, "impws_i3c4_ap_clock",
			"i2c_pseudo_ck"/* parent */, 2),
};

static const struct mtk_clk_desc impws_mcd = {
	.clks = impws_clks,
	.num_clks = CLK_IMPWS_NR_CLK,
};

static const struct of_device_id of_match_clk_mt6833_i2c[] = {
	{
		.compatible = "mediatek,mt6833-imp_iic_wrap_c",
		.data = &impc_mcd,
	}, {
		.compatible = "mediatek,mt6833-imp_iic_wrap_e",
		.data = &impe_mcd,
	}, {
		.compatible = "mediatek,mt6833-imp_iic_wrap_n",
		.data = &impn_mcd,
	}, {
		.compatible = "mediatek,mt6833-imp_iic_wrap_s",
		.data = &imps_mcd,
	}, {
		.compatible = "mediatek,mt6833-imp_iic_wrap_w",
		.data = &impw_mcd,
	}, {
		.compatible = "mediatek,mt6833-imp_iic_wrap_ws",
		.data = &impws_mcd,
	}, {
		/* sentinel */
	}
};


static int clk_mt6833_i2c_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6833_i2c_drv = {
	.probe = clk_mt6833_i2c_grp_probe,
	.driver = {
		.name = "clk-mt6833-i2c",
		.of_match_table = of_match_clk_mt6833_i2c,
	},
};

module_platform_driver(clk_mt6833_i2c_drv);
MODULE_LICENSE("GPL");
