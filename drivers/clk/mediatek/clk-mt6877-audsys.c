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
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6877-clk.h>

#define MT_CCF_BRINGUP		1

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

/* get spm power status struct to register inside clk_data */
static struct pwr_status audsys_pwr_stat = GATE_PWR_STAT(0xEF0,
		0xEF4, INV_OFS, BIT(21), BIT(21));

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

#define GATE_AUDSYS0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audsys0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
		.pwr_stat = &audsys_pwr_stat,			\
	}

#define GATE_AUDSYS1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &audsys1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
		.pwr_stat = &audsys_pwr_stat,			\
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
};

static int clk_mt6877_audsys_probe(struct platform_device *pdev)
{
	struct clk_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_AUDSYS_NR_CLK);

	mtk_clk_register_gates(node, audsys_clks, ARRAY_SIZE(audsys_clks),
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

static const struct of_device_id of_match_clk_mt6877_audsys[] = {
	{ .compatible = "mediatek,mt6877-audiosys", },
	{}
};

static struct platform_driver clk_mt6877_audsys_drv = {
	.probe = clk_mt6877_audsys_probe,
	.driver = {
		.name = "clk-mt6877-audsys",
		.of_match_table = of_match_clk_mt6877_audsys,
	},
};

static int __init clk_mt6877_audsys_init(void)
{
	return platform_driver_register(&clk_mt6877_audsys_drv);
}
arch_initcall(clk_mt6877_audsys_init);

