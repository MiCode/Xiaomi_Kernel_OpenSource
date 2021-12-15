// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6893-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs audsys0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs audsys1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs audsys2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

#define GATE_AUDSYS0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audsys0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AUDSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audsys1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_AUDSYS2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audsys2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate audsys_clks[] = {
	/* AUDSYS0 */
	GATE_AUDSYS0(CLK_AUDSYS_AFE, "aud_afe",
			"audio_ck"/* parent */, 2),
	GATE_AUDSYS0(CLK_AUDSYS_22M, "aud_22m",
			"aud_engen1_ck"/* parent */, 8),
	GATE_AUDSYS0(CLK_AUDSYS_24M, "aud_24m",
			"aud_engen2_ck"/* parent */, 9),
	GATE_AUDSYS0(CLK_AUDSYS_APLL2_TUNER, "aud_apll2_tuner",
			"aud_engen2_ck"/* parent */, 18),
	GATE_AUDSYS0(CLK_AUDSYS_APLL_TUNER, "aud_apll_tuner",
			"aud_engen1_ck"/* parent */, 19),
	GATE_AUDSYS0(CLK_AUDSYS_TDM, "aud_tdm_ck",
			"aud_1_ck"/* parent */, 20),
	GATE_AUDSYS0(CLK_AUDSYS_ADC, "aud_adc",
			"audio_ck"/* parent */, 24),
	GATE_AUDSYS0(CLK_AUDSYS_DAC, "aud_dac",
			"audio_ck"/* parent */, 25),
	GATE_AUDSYS0(CLK_AUDSYS_DAC_PREDIS, "aud_dac_predis",
			"audio_ck"/* parent */, 26),
	GATE_AUDSYS0(CLK_AUDSYS_TML, "aud_tml",
			"audio_ck"/* parent */, 27),
	GATE_AUDSYS0(CLK_AUDSYS_NLE, "aud_nle",
			"audio_ck"/* parent */, 28),
	/* AUDSYS1 */
	GATE_AUDSYS1(CLK_AUDSYS_I2S1_BCLK, "aud_i2s1_bclk",
			"audio_ck"/* parent */, 4),
	GATE_AUDSYS1(CLK_AUDSYS_I2S2_BCLK, "aud_i2s2_bclk",
			"audio_ck"/* parent */, 5),
	GATE_AUDSYS1(CLK_AUDSYS_I2S3_BCLK, "aud_i2s3_bclk",
			"audio_ck"/* parent */, 6),
	GATE_AUDSYS1(CLK_AUDSYS_I2S4_BCLK, "aud_i2s4_bclk",
			"audio_ck"/* parent */, 7),
	GATE_AUDSYS1(CLK_AUDSYS_CONNSYS_I2S_ASRC, "aud_connsys_i2s_asrc",
			"audio_ck"/* parent */, 12),
	GATE_AUDSYS1(CLK_AUDSYS_GENERAL1_ASRC, "aud_general1_asrc",
			"audio_ck"/* parent */, 13),
	GATE_AUDSYS1(CLK_AUDSYS_GENERAL2_ASRC, "aud_general2_asrc",
			"audio_ck"/* parent */, 14),
	GATE_AUDSYS1(CLK_AUDSYS_DAC_HIRES, "aud_dac_hires",
			"audio_h_ck"/* parent */, 15),
	GATE_AUDSYS1(CLK_AUDSYS_ADC_HIRES, "aud_adc_hires",
			"audio_h_ck"/* parent */, 16),
	GATE_AUDSYS1(CLK_AUDSYS_ADC_HIRES_TML, "aud_adc_hires_tml",
			"audio_h_ck"/* parent */, 17),
	GATE_AUDSYS1(CLK_AUDSYS_ADDA6_ADC, "aud_adda6_adc",
			"audio_ck"/* parent */, 20),
	GATE_AUDSYS1(CLK_AUDSYS_ADDA6_ADC_HIRES, "aud_adda6_adc_hires",
			"audio_h_ck"/* parent */, 21),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC, "aud_3rd_dac",
			"audio_ck"/* parent */, 28),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC_PREDIS, "aud_3rd_dac_predis",
			"audio_ck"/* parent */, 29),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC_TML, "aud_3rd_dac_tml",
			"audio_ck"/* parent */, 30),
	GATE_AUDSYS1(CLK_AUDSYS_3RD_DAC_HIRES, "aud_3rd_dac_hires",
			"audio_h_ck"/* parent */, 31),
	/* AUDSYS2 */
	GATE_AUDSYS2(CLK_AUDSYS_I2S5_BCLK, "aud_i2s5_bclk",
			"audio_ck"/* parent */, 0),
	GATE_AUDSYS2(CLK_AUDSYS_I2S6_BCLK, "aud_i2s6_bclk",
			"audio_ck"/* parent */, 1),
	GATE_AUDSYS2(CLK_AUDSYS_I2S7_BCLK, "aud_i2s7_bclk",
			"audio_ck"/* parent */, 2),
	GATE_AUDSYS2(CLK_AUDSYS_I2S8_BCLK, "aud_i2s8_bclk",
			"audio_ck"/* parent */, 3),
	GATE_AUDSYS2(CLK_AUDSYS_I2S9_BCLK, "aud_i2s9_bclk",
			"audio_ck"/* parent */, 4),
};

static const struct mtk_clk_desc audsys_mcd = {
	.clks = audsys_clks,
	.num_clks = CLK_AUDSYS_NR_CLK,
};

static int clk_mt6893_audsys_probe(struct platform_device *pdev)
{
	int r;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6893_audsys[] = {
	{
		.compatible = "mediatek,mt6893-audiosys",
		.data = &audsys_mcd,
	},
	{}
};

static struct platform_driver clk_mt6893_audsys_drv = {
	.probe = clk_mt6893_audsys_probe,
	.driver = {
		.name = "clk-mt6893-audsys",
		.of_match_table = of_match_clk_mt6893_audsys,
	},
};

static int __init clk_mt6893_audsys_init(void)
{
	return platform_driver_register(&clk_mt6893_audsys_drv);
}

static void __exit clk_mt6893_audsys_exit(void)
{
	platform_driver_unregister(&clk_mt6893_audsys_drv);
}

postcore_initcall(clk_mt6893_audsys_init);
module_exit(clk_mt6893_audsys_exit);
MODULE_LICENSE("GPL");
