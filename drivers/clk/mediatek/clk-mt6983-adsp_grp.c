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

static const struct mtk_gate_regs adsp_0_cg_regs = {
	.set_ofs = 0x1000,
	.clr_ofs = 0x1000,
	.sta_ofs = 0x1000,
};

static const struct mtk_gate_regs adsp_1_cg_regs = {
	.set_ofs = 0x1010,
	.clr_ofs = 0x1010,
	.sta_ofs = 0x1010,
};

#define GATE_ADSP_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &adsp_0_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,			\
	}
#define GATE_ADSP_1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &adsp_1_cg_regs,				\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,			\
	}
static struct mtk_gate adsp_clks[] = {
	GATE_ADSP_0(CLK_ADSP_CORE0_CLK_EN /* CLK ID */,
		"adsp_core0" /* name */,
		"adsp_ck" /* parent */, 0 /* bit */),
	GATE_ADSP_0(CLK_ADSP_CORE0_DBG_EN /* CLK ID */,
		"adsp_core0_dbg_en" /* name */,
		"adsp_ck" /* parent */, 1 /* bit */),
	GATE_ADSP_0(CLK_ADSP_TIMER_EN /* CLK ID */,
		"adsp_timer_en" /* name */,
		"adsp_ck" /* parent */, 3 /* bit */),
	GATE_ADSP_0(CLK_ADSP_DMA0_EN /* CLK ID */,
		"adsp_dma0_en" /* name */,
		"adsp_ck" /* parent */, 4 /* bit */),
	GATE_ADSP_0(CLK_ADSP_UART_EN /* CLK ID */,
		"adsp_uart_en" /* name */,
		"adsp_ck" /* parent */, 5 /* bit */),
	GATE_ADSP_1(CLK_ADSP_UART_BCLK /* CLK ID */,
		"adsp_uart_bclk" /* name */,
		"adsp_ck" /* parent */, 0 /* bit */),
};

static int clk_mt6983_adsp_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_ADSP_NR_CLK);

	mtk_clk_register_gates(node, adsp_clks,
		ARRAY_SIZE(adsp_clks),
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

static const struct mtk_gate_regs afe_0_cg_regs = {
	.set_ofs = 0x0,
	.clr_ofs = 0x0,
	.sta_ofs = 0x0,
};

static const struct mtk_gate_regs afe_1_cg_regs = {
	.set_ofs = 0x4,
	.clr_ofs = 0x4,
	.sta_ofs = 0x4,
};

static const struct mtk_gate_regs afe_2_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

#define GATE_AFE_0(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &afe_0_cg_regs,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr,			\
	}
#define GATE_AFE_1(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &afe_1_cg_regs,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr,			\
	}
#define GATE_AFE_2(_id, _name, _parent, _shift) {			\
		.id = _id,						\
		.name = _name,						\
		.parent_name = _parent,					\
		.regs = &afe_2_cg_regs,					\
		.shift = _shift,					\
		.ops = &mtk_clk_gate_ops_no_setclr,			\
	}
static struct mtk_gate afe_clks[] = {
	GATE_AFE_0(CLK_AFE_AFE /* CLK ID */,
		"afe_afe" /* name */,
		"audio_ck" /* parent */, 2 /* bit */),
	GATE_AFE_0(CLK_AFE_22M /* CLK ID */,
		"afe_22m" /* name */,
		"aud_engen1_ck" /* parent */, 8 /* bit */),
	GATE_AFE_0(CLK_AFE_24M /* CLK ID */,
		"afe_24m" /* name */,
		"aud_engen2_ck" /* parent */, 9 /* bit */),
	GATE_AFE_0(CLK_AFE_APLL2_TUNER /* CLK ID */,
		"afe_apll2_tuner" /* name */,
		"aud_engen2_ck" /* parent */, 18 /* bit */),
	GATE_AFE_0(CLK_AFE_APLL_TUNER /* CLK ID */,
		"afe_apll_tuner" /* name */,
		"aud_engen1_ck" /* parent */, 19 /* bit */),
	GATE_AFE_0(CLK_AFE_TDM /* CLK ID */,
		"afe_tdm_ck" /* name */,
		"aud_1_ck" /* parent */, 20 /* bit */),
	GATE_AFE_0(CLK_AFE_ADC /* CLK ID */,
		"afe_adc" /* name */,
		"audio_ck" /* parent */, 24 /* bit */),
	GATE_AFE_0(CLK_AFE_DAC /* CLK ID */,
		"afe_dac" /* name */,
		"audio_ck" /* parent */, 25 /* bit */),
	GATE_AFE_0(CLK_AFE_DAC_PREDIS /* CLK ID */,
		"afe_dac_predis" /* name */,
		"audio_ck" /* parent */, 26 /* bit */),
	GATE_AFE_0(CLK_AFE_TML /* CLK ID */,
		"afe_tml" /* name */,
		"audio_ck" /* parent */, 27 /* bit */),
	GATE_AFE_0(CLK_AFE_NLE /* CLK ID */,
		"afe_nle" /* name */,
		"audio_ck" /* parent */, 28 /* bit */),
	GATE_AFE_1(CLK_AFE_GENERAL3_ASRC /* CLK ID */,
		"afe_general3_asrc" /* name */,
		"audio_ck" /* parent */, 11 /* bit */),
	GATE_AFE_1(CLK_AFE_CONNSYS_I2S_ASRC /* CLK ID */,
		"afe_connsys_i2s_asrc" /* name */,
		"audio_ck" /* parent */, 12 /* bit */),
	GATE_AFE_1(CLK_AFE_GENERAL1_ASRC /* CLK ID */,
		"afe_general1_asrc" /* name */,
		"audio_ck" /* parent */, 13 /* bit */),
	GATE_AFE_1(CLK_AFE_GENERAL2_ASRC /* CLK ID */,
		"afe_general2_asrc" /* name */,
		"audio_ck" /* parent */, 14 /* bit */),
	GATE_AFE_1(CLK_AFE_DAC_HIRES /* CLK ID */,
		"afe_dac_hires" /* name */,
		"audio_h_ck" /* parent */, 15 /* bit */),
	GATE_AFE_1(CLK_AFE_ADC_HIRES /* CLK ID */,
		"afe_adc_hires" /* name */,
		"audio_h_ck" /* parent */, 16 /* bit */),
	GATE_AFE_1(CLK_AFE_ADC_HIRES_TML /* CLK ID */,
		"afe_adc_hires_tml" /* name */,
		"audio_h_ck" /* parent */, 17 /* bit */),
	GATE_AFE_1(CLK_AFE_ADDA6_ADC /* CLK ID */,
		"afe_adda6_adc" /* name */,
		"audio_ck" /* parent */, 20 /* bit */),
	GATE_AFE_1(CLK_AFE_ADDA6_ADC_HIRES /* CLK ID */,
		"afe_adda6_adc_hires" /* name */,
		"audio_h_ck" /* parent */, 21 /* bit */),
	GATE_AFE_1(CLK_AFE_ADDA7_ADC /* CLK ID */,
		"afe_adda7_adc" /* name */,
		"audio_ck" /* parent */, 22 /* bit */),
	GATE_AFE_1(CLK_AFE_ADDA7_ADC_HIRES /* CLK ID */,
		"afe_adda7_adc_hires" /* name */,
		"audio_h_ck" /* parent */, 23 /* bit */),
	GATE_AFE_1(CLK_AFE_3RD_DAC /* CLK ID */,
		"afe_3rd_dac" /* name */,
		"audio_ck" /* parent */, 28 /* bit */),
	GATE_AFE_1(CLK_AFE_3RD_DAC_PREDIS /* CLK ID */,
		"afe_3rd_dac_predis" /* name */,
		"audio_ck" /* parent */, 29 /* bit */),
	GATE_AFE_1(CLK_AFE_3RD_DAC_TML /* CLK ID */,
		"afe_3rd_dac_tml" /* name */,
		"audio_ck" /* parent */, 30 /* bit */),
	GATE_AFE_1(CLK_AFE_3RD_DAC_HIRES /* CLK ID */,
		"afe_3rd_dac_hires" /* name */,
		"audio_h_ck" /* parent */, 31 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S5_BCLK /* CLK ID */,
		"afe_i2s5_bclk" /* name */,
		"audio_ck" /* parent */, 0 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S6_BCLK /* CLK ID */,
		"afe_i2s6_bclk" /* name */,
		"audio_ck" /* parent */, 1 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S7_BCLK /* CLK ID */,
		"afe_i2s7_bclk" /* name */,
		"audio_ck" /* parent */, 2 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S8_BCLK /* CLK ID */,
		"afe_i2s8_bclk" /* name */,
		"audio_ck" /* parent */, 3 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S9_BCLK /* CLK ID */,
		"afe_i2s9_bclk" /* name */,
		"audio_ck" /* parent */, 4 /* bit */),
	GATE_AFE_2(CLK_AFE_ETDM_IN0_BCLK /* CLK ID */,
		"afe_etdm_in0_bclk" /* name */,
		"audio_ck" /* parent */, 5 /* bit */),
	GATE_AFE_2(CLK_AFE_ETDM_OUT0_BCLK /* CLK ID */,
		"afe_etdm_out0_bclk" /* name */,
		"audio_ck" /* parent */, 6 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S1_BCLK /* CLK ID */,
		"afe_i2s1_bclk" /* name */,
		"audio_ck" /* parent */, 7 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S2_BCLK /* CLK ID */,
		"afe_i2s2_bclk" /* name */,
		"audio_ck" /* parent */, 8 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S3_BCLK /* CLK ID */,
		"afe_i2s3_bclk" /* name */,
		"audio_ck" /* parent */, 9 /* bit */),
	GATE_AFE_2(CLK_AFE_I2S4_BCLK /* CLK ID */,
		"afe_i2s4_bclk" /* name */,
		"audio_ck" /* parent */, 10 /* bit */),
	GATE_AFE_2(CLK_AFE_ETDM_IN1_BCLK /* CLK ID */,
		"afe_etdm_in1_bclk" /* name */,
		"audio_ck" /* parent */, 23 /* bit */),
	GATE_AFE_2(CLK_AFE_ETDM_OUT1_BCLK /* CLK ID */,
		"afe_etdm_out1_bclk" /* name */,
		"audio_ck" /* parent */, 24 /* bit */),
};

static int clk_mt6983_afe_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_onecell_data *clk_data;
	int ret = 0;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif /* MT_CCF_BRINGUP */

	clk_data = mtk_alloc_clk_data(CLK_AFE_NR_CLK);

	mtk_clk_register_gates(node, afe_clks,
		ARRAY_SIZE(afe_clks),
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

static const struct of_device_id of_match_clk_mt6983_adsp[] = {
	{
		.compatible = "mediatek,mt6983-adsp",
		.data = clk_mt6983_adsp_probe,
	}, {
		.compatible = "mediatek,mt6983-afe",
		.data = clk_mt6983_afe_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6983_adsp_grp_probe(struct platform_device *pdev)
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

static struct platform_driver clk_mt6983_adsp_drv = {
	.probe = clk_mt6983_adsp_grp_probe,
	.driver = {
		.name = "clk-mt6983-adsp",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6983_adsp,
	},
};

static int __init clk_mt6983_adsp_init(void)
{
	return platform_driver_register(&clk_mt6983_adsp_drv);
}

static void __exit clk_mt6983_adsp_exit(void)
{
	platform_driver_unregister(&clk_mt6983_adsp_drv);
}

arch_initcall(clk_mt6983_adsp_init);
module_exit(clk_mt6983_adsp_exit);
MODULE_LICENSE("GPL");

