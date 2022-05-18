// SPDX-License-Identifier: GPL-2.0
/*
 *  mt6895-afe-clk.c  --  Mediatek 6895 afe clock ctrl
 *
 *  Copyright (c) 2021 MediaTek Inc.
 *  Author: Tina Tsai <tina.tsai@mediatek.com>
 */

#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <linux/arm-smccc.h> /* for Kernel Native SMC API */
#include <linux/soc/mediatek/mtk_sip_svc.h> /* for SMC ID table */

#include "mt6895-afe-common.h"
#include "mt6895-afe-clk.h"

#if defined(CONFIG_FPGA_EARLY_PORTING)
int mt6895_init_clock(struct mtk_base_afe *afe) { return 0; }
int mt6895_afe_enable_clock(struct mtk_base_afe *afe) { return 0; }
void mt6895_afe_disable_clock(struct mtk_base_afe *afe) {}
int mt6895_afe_dram_request(struct device *dev) { return 0; }
int mt6895_afe_dram_release(struct device *dev) { return 0; }
int mt6895_apll1_enable(struct mtk_base_afe *afe) { return 0; }
void mt6895_apll1_disable(struct mtk_base_afe *afe) {}
int mt6895_apll2_enable(struct mtk_base_afe *afe) { return 0; }
void mt6895_apll2_disable(struct mtk_base_afe *afe) {}
int mt6895_get_apll_rate(struct mtk_base_afe *afe, int apll) { return 0; }
int mt6895_get_apll_by_rate(struct mtk_base_afe *afe, int rate) { return 0; }
int mt6895_get_apll_by_name(struct mtk_base_afe *afe, const char *name) { return 0; }
int mt6895_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate) { return 0; }
void mt6895_mck_disable(struct mtk_base_afe *afe, int mck_id) {}
int mt6895_set_audio_int_bus_parent(struct mtk_base_afe *afe, int clk_id) { return 0; }
#else
static DEFINE_MUTEX(mutex_request_dram);

static const char *aud_clks[CLK_NUM] = {
	[CLK_AFE] = "aud_afe_clk",
	[CLK_DAC] = "aud_dac_clk",
	[CLK_DAC_PREDIS] = "aud_dac_predis_clk",
	[CLK_ADC] = "aud_adc_clk",
	[CLK_ADDA6_ADC] = "aud_adda6_adc_clk",
	[CLK_APLL22M] = "aud_apll22m_clk",
	[CLK_APLL24M] = "aud_apll24m_clk",
	[CLK_APLL1_TUNER] = "aud_apll1_tuner_clk",
	[CLK_APLL2_TUNER] = "aud_apll2_tuner_clk",
	[CLK_TDM] = "aud_tdm_clk",
	[CLK_TML] = "aud_tml_clk",
	[CLK_NLE] = "aud_nle",
	[CLK_DAC_HIRES] = "aud_dac_hires_clk",
	[CLK_ADC_HIRES] = "aud_adc_hires_clk",
	[CLK_ADC_HIRES_TML] = "aud_adc_hires_tml",
	[CLK_ADDA6_ADC_HIRES] = "aud_adda6_adc_hires_clk",
	[CLK_3RD_DAC] = "aud_3rd_dac_clk",
	[CLK_3RD_DAC_PREDIS] = "aud_3rd_dac_predis_clk",
	[CLK_3RD_DAC_TML] = "aud_3rd_dac_tml",
	[CLK_3RD_DAC_HIRES] = "aud_3rd_dac_hires_clk",
	[CLK_MUX_AUDIO] = "top_mux_audio",
	[CLK_MUX_AUDIOINTBUS] = "top_mux_audio_int",
	[CLK_TOP_MAINPLL_D4_D4] = "top_mainpll_d4_d4",
	[CLK_TOP_MUX_AUD_1] = "top_mux_aud_1",
	[CLK_TOP_APLL1_CK] = "top_apll1_ck",
	[CLK_TOP_MUX_AUD_2] = "top_mux_aud_2",
	[CLK_TOP_APLL2_CK] = "top_apll2_ck",
	[CLK_TOP_MUX_AUD_ENG1] = "top_mux_aud_eng1",
	[CLK_TOP_APLL1_D4] = "top_apll1_d4",
	[CLK_TOP_MUX_AUD_ENG2] = "top_mux_aud_eng2",
	[CLK_TOP_APLL2_D4] = "top_apll2_d4",
	[CLK_TOP_MUX_AUDIO_H] = "top_mux_audio_h",
	[CLK_TOP_I2S0_M_SEL] = "top_i2s0_m_sel",
	[CLK_TOP_I2S1_M_SEL] = "top_i2s1_m_sel",
	[CLK_TOP_I2S2_M_SEL] = "top_i2s2_m_sel",
	[CLK_TOP_I2S3_M_SEL] = "top_i2s3_m_sel",
	[CLK_TOP_I2S4_M_SEL] = "top_i2s4_m_sel",
	[CLK_TOP_I2S5_M_SEL] = "top_i2s5_m_sel",
	[CLK_TOP_I2S6_M_SEL] = "top_i2s6_m_sel",
	[CLK_TOP_I2S7_M_SEL] = "top_i2s7_m_sel",
	[CLK_TOP_I2S8_M_SEL] = "top_i2s8_m_sel",
	[CLK_TOP_I2S9_M_SEL] = "top_i2s9_m_sel",
	[CLK_TOP_APLL12_DIV0] = "top_apll12_div0",
	[CLK_TOP_APLL12_DIV1] = "top_apll12_div1",
	[CLK_TOP_APLL12_DIV2] = "top_apll12_div2",
	[CLK_TOP_APLL12_DIV3] = "top_apll12_div3",
	[CLK_TOP_APLL12_DIV4] = "top_apll12_div4",
	[CLK_TOP_APLL12_DIVB] = "top_apll12_divb",
	[CLK_TOP_APLL12_DIV5] = "top_apll12_div5",
	[CLK_TOP_APLL12_DIV6] = "top_apll12_div6",
	[CLK_TOP_APLL12_DIV7] = "top_apll12_div7",
	[CLK_TOP_APLL12_DIV8] = "top_apll12_div8",
	[CLK_TOP_APLL12_DIV9] = "top_apll12_div9",
	[CLK_CLK26M] = "top_clk26m_clk",
};

int mt6895_set_audio_int_bus_parent(struct mtk_base_afe *afe,
				    int clk_id)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int ret;

	if (clk_id < 0) {
		dev_err(afe->dev, "%s() set_audio_int_bus_parent fail %d\n",
			__func__, clk_id);
		return -EINVAL;
	}

	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIOINTBUS],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_err(afe->dev, "%s() clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS],
			aud_clks[clk_id], ret);
	}

	return ret;
}

static int apll1_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_TOP_APLL1_CK]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_TOP_APLL1_CK], ret);
			goto EXIT;
		}

		/* 180.6336 / 4 = 45.1584MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D4]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_TOP_APLL1_D4], ret);
			goto EXIT;
		}
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_1],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_1],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_1]);
	}

EXIT:
	return 0;
}

static int apll2_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	if (enable) {
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_TOP_APLL2_CK]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_TOP_APLL2_CK], ret);
			goto EXIT;
		}

		/* 196.608 / 4 = 49.152MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_TOP_APLL2_D4]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_TOP_APLL2_D4], ret);
			goto EXIT;
		}
	} else {
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG2],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_ENG2]);

		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_2],
				     afe_priv->clk[CLK_CLK26M]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_2],
				aud_clks[CLK_CLK26M], ret);
			goto EXIT;
		}
		clk_disable_unprepare(afe_priv->clk[CLK_TOP_MUX_AUD_2]);
	}

EXIT:
	return 0;
}

int mt6895_afe_enable_clock(struct mtk_base_afe *afe)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;
#if !defined(SKIP_SB)
	struct arm_smccc_res res;
#endif

	dev_info(afe->dev, "%s()\n", __func__);

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIO]);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIO], ret);
		goto CLK_MUX_AUDIO_ERR;
	}
	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIO],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIO],
			aud_clks[CLK_CLK26M], ret);
		goto CLK_MUX_AUDIO_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}

	ret = mt6895_set_audio_int_bus_parent(afe, CLK_CLK26M);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_ERR;

	ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUDIO_H],
			     afe_priv->clk[CLK_TOP_APLL2_CK]);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_set_parent %s-%s fail %d\n",
			__func__, aud_clks[CLK_TOP_MUX_AUDIO_H],
			aud_clks[CLK_TOP_APLL2_CK], ret);
		goto CLK_MUX_AUDIO_H_PARENT_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE]);
	if (ret) {
		dev_err(afe->dev, "%s() clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE], ret);
		goto CLK_AFE_ERR;
	}

#if !defined(SKIP_SB)
	/* use arm_smccc_smc to notify SPM */
	arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
		MTK_AUDIO_SMC_OP_DOMAIN_SIDEBANDS,
		0, 0, 0, 0, 0, 0, &res);
#endif

	return 0;

CLK_AFE_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);
CLK_MUX_AUDIO_H_PARENT_ERR:
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
CLK_MUX_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);

	return ret;
}

void mt6895_afe_disable_clock(struct mtk_base_afe *afe)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s()\n", __func__);

	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);

	mt6895_set_audio_int_bus_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);
}

int mt6895_afe_dram_request(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
#if !defined(SKIP_SB)
	struct arm_smccc_res res;
#endif

	dev_info(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);

	/* use arm_smccc_smc to notify SPM */
	if (afe_priv->dram_resource_counter == 0)
#if !defined(SKIP_SB)
		arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
			      MTK_AUDIO_SMC_OP_DRAM_REQUEST,
			      0, 0, 0, 0, 0, 0, &res);
#endif

	afe_priv->dram_resource_counter++;
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6895_afe_dram_release(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
#if !defined(SKIP_SB)
	struct arm_smccc_res res;
#endif

	dev_info(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
	afe_priv->dram_resource_counter--;

	/* use arm_smccc_smc to notify SPM */
	if (afe_priv->dram_resource_counter == 0)
#if !defined(SKIP_SB)
		arm_smccc_smc(MTK_SIP_AUDIO_CONTROL,
			      MTK_AUDIO_SMC_OP_DRAM_RELEASE,
			      0, 0, 0, 0, 0, 0, &res);
#endif

	if (afe_priv->dram_resource_counter < 0) {
		dev_warn(dev, "%s(), dram_resource_counter %d\n",
			 __func__, afe_priv->dram_resource_counter);
		afe_priv->dram_resource_counter = 0;
	}
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6895_apll1_enable(struct mtk_base_afe *afe)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll1_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL22M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL22M], ret);
		goto ERR_CLK_APLL22M;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1_TUNER]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL1_TUNER], ret);
		goto ERR_CLK_APLL1_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG,
			   0x0000FFF7, 0x00000372);
	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_22M_ON_MASK_SFT,
			   0x1 << AFE_22M_ON_SFT);

	return 0;

ERR_CLK_APLL1_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
ERR_CLK_APLL22M:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL22M]);

	return ret;
}

void mt6895_apll1_disable(struct mtk_base_afe *afe)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_22M_ON_MASK_SFT,
			   0x0 << AFE_22M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL1_TUNER_CFG, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL22M]);

	apll1_mux_setting(afe, false);
}

int mt6895_apll2_enable(struct mtk_base_afe *afe)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* setting for APLL */
	apll2_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL24M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL24M], ret);
		goto ERR_CLK_APLL24M;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL2_TUNER]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL2_TUNER], ret);
		goto ERR_CLK_APLL2_TUNER;
	}

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG,
			   0x0000FFF7, 0x00000374);
	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_24M_ON_MASK_SFT,
			   0x1 << AFE_24M_ON_SFT);

	return 0;

ERR_CLK_APLL2_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
ERR_CLK_APLL24M:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);

	return ret;

	return 0;
}

void mt6895_apll2_disable(struct mtk_base_afe *afe)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_24M_ON_MASK_SFT,
			   0x0 << AFE_24M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL2_TUNER_CFG, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL2_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);

	apll2_mux_setting(afe, false);
}

int mt6895_get_apll_rate(struct mtk_base_afe *afe, int apll)
{
	return (apll == MT6895_APLL1) ? 180633600 : 196608000;
}

int mt6895_get_apll_by_rate(struct mtk_base_afe *afe, int rate)
{
	return ((rate % 8000) == 0) ? MT6895_APLL2 : MT6895_APLL1;
}

int mt6895_get_apll_by_name(struct mtk_base_afe *afe, const char *name)
{
	if (strcmp(name, APLL1_W_NAME) == 0)
		return MT6895_APLL1;
	else
		return MT6895_APLL2;
}

/* mck */
struct mt6895_mck_div {
	int m_sel_id;
	int div_clk_id;
	/* below will be deprecated */
	int div_pdn_reg;
	int div_pdn_mask_sft;
	int div_reg;
	int div_mask_sft;
	int div_mask;
	int div_sft;
	int div_msb_clk_id;
	int div_msb_reg;
	int div_msb_mask_sft;
	int div_msb_mask;
	int div_msb_sft;
	int div_apll_sel_reg;
	int div_apll_sel_mask_sft;
	int div_apll_sel_sft;
	int div_inv_reg;
	int div_inv_mask_sft;
};


static const struct mt6895_mck_div mck_div[MT6895_MCK_NUM] = {
	[MT6895_I2S0_MCK] = {
		.m_sel_id = CLK_TOP_I2S0_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV0,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV0_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV0_MASK_SFT,
		.div_mask = APLL12_CK_DIV0_MASK,
		.div_sft = APLL12_CK_DIV0_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S0_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S0_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV0_INV_MASK_SFT,
	},
	[MT6895_I2S1_MCK] = {
		.m_sel_id = CLK_TOP_I2S1_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV1,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV1_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV1_MASK_SFT,
		.div_mask = APLL12_CK_DIV1_MASK,
		.div_sft = APLL12_CK_DIV1_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S1_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S1_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV1_INV_MASK_SFT,
	},
	[MT6895_I2S2_MCK] = {
		.m_sel_id = CLK_TOP_I2S2_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV2,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV2_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV2_MASK_SFT,
		.div_mask = APLL12_CK_DIV2_MASK,
		.div_sft = APLL12_CK_DIV2_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S2_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S2_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV2_INV_MASK_SFT,
	},
	[MT6895_I2S3_MCK] = {
		.m_sel_id = CLK_TOP_I2S3_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV3,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV3_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_2,
		.div_mask_sft = APLL12_CK_DIV3_MASK_SFT,
		.div_mask = APLL12_CK_DIV3_MASK,
		.div_sft = APLL12_CK_DIV3_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S3_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S3_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV3_INV_MASK_SFT,
	},
	[MT6895_I2S4_MCK] = {
		.m_sel_id = CLK_TOP_I2S4_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV4,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV4_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_3,
		.div_mask_sft = APLL12_CK_DIV4_MASK_SFT,
		.div_mask = APLL12_CK_DIV4_MASK,
		.div_sft = APLL12_CK_DIV4_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S4_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S4_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV4_INV_MASK_SFT,
	},
	[MT6895_I2S4_BCK] = {
		.m_sel_id = -1,
		.div_clk_id = CLK_TOP_APLL12_DIVB,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIVB_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_3,
		.div_mask_sft = APLL12_CK_DIVB_MASK_SFT,
		.div_mask = APLL12_CK_DIVB_MASK,
		.div_sft = APLL12_CK_DIVB_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIVB_INV_MASK_SFT,
	},
	[MT6895_I2S5_MCK] = {
		.m_sel_id = CLK_TOP_I2S5_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV5,
		.div_pdn_reg = CLK_AUDDIV_0,
		.div_pdn_mask_sft = APLL12_DIV5_PDN_MASK_SFT,
		.div_reg = CLK_AUDDIV_3,
		.div_mask_sft = APLL12_CK_DIV5_MASK_SFT,
		.div_mask = APLL12_CK_DIV5_MASK,
		.div_sft = APLL12_CK_DIV5_SFT,
		.div_apll_sel_reg = CLK_AUDDIV_0,
		.div_apll_sel_mask_sft = APLL_I2S5_MCK_SEL_MASK_SFT,
		.div_apll_sel_sft = APLL_I2S5_MCK_SEL_SFT,
		.div_inv_reg = CLK_AUDDIV_1,
		.div_inv_mask_sft = APLL12_DIV5_INV_MASK_SFT,
	},
};

int mt6895_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int apll = mt6895_get_apll_by_rate(afe, rate);
	int apll_clk_id = apll == MT6895_APLL1 ?
			  CLK_TOP_MUX_AUD_1 : CLK_TOP_MUX_AUD_2;
	int m_sel_id = 0;
	int div_clk_id = 0;
	int ret = 0;

	if (mck_id < 0) {
		dev_err(afe->dev, "%s() mck_enable fail invalid mck_id %d\n",
			__func__, mck_id);
		return -EINVAL;
	}
	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	/* select apll */
	if (m_sel_id > 0) {
		ret = clk_prepare_enable(afe_priv->clk[m_sel_id]);
		if (ret) {
			dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[m_sel_id], ret);
			return ret;
		}
		ret = clk_set_parent(afe_priv->clk[m_sel_id],
				     afe_priv->clk[apll_clk_id]);
		if (ret) {
			dev_err(afe->dev, "%s(), clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[m_sel_id],
				aud_clks[apll_clk_id], ret);
			return ret;
		}
	}

	if (div_clk_id <= 0) {
		dev_err(afe->dev, "%s() mck_enable fail invalid div_clk_id %d\n",
			__func__, div_clk_id);
		return -EINVAL;
	}
	/* enable div, set rate */
	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}

	dev_info(afe->dev, "%s, clk_prepare_enable & clk_set_rate %s, rate %d success\n",
		 __func__, aud_clks[div_clk_id], rate);
	/* i2s5 not full support by ccf */
	if (mck_id != MT6895_I2S5_MCK)
		return 0;

	/* enable div msb clk, set rate */
	div_clk_id = mck_div[mck_id].div_msb_clk_id;
	if (div_clk_id <= 0) {
		dev_err(afe->dev, "%s() mck_enable fail invalid div_msb_clk_id %d\n",
			__func__, div_clk_id);
		return -EINVAL;
	}

	ret = clk_prepare_enable(afe_priv->clk[div_clk_id]);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[div_clk_id], ret);
		return ret;
	}
	ret = clk_set_rate(afe_priv->clk[div_clk_id], rate);
	if (ret) {
		dev_err(afe->dev, "%s(), clk_set_rate %s, rate %d, fail %d\n",
			__func__, aud_clks[div_clk_id],
			rate, ret);
		return ret;
	}
	/* debug when migration */
	dev_info(afe->dev, "%s, clk_prepare_enable & clk_set_rate %s, rate %d success\n",
		 __func__, aud_clks[div_clk_id], rate);

	return 0;
}

void mt6895_mck_disable(struct mtk_base_afe *afe, int mck_id)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int m_sel_id = 0;
	int div_clk_id = 0;

	if (mck_id < 0) {
		dev_err(afe->dev, "%s() mck_disable fail invalid mck_id %d\n",
			__func__, mck_id);
		return;
	}
	m_sel_id = mck_div[mck_id].m_sel_id;
	div_clk_id = mck_div[mck_id].div_clk_id;

	clk_disable_unprepare(afe_priv->clk[div_clk_id]);
	/* i2s5 */
	if (mck_id == MT6895_I2S5_MCK) {
		div_clk_id = mck_div[mck_id].div_msb_clk_id;
		clk_disable_unprepare(afe_priv->clk[div_clk_id]);
	}

	if (m_sel_id > 0)
		clk_disable_unprepare(afe_priv->clk[m_sel_id]);
}

int mt6895_init_clock(struct mtk_base_afe *afe)
{
	struct mt6895_afe_private *afe_priv = afe->platform_priv;
	int i = 0;

	afe_priv->clk = devm_kcalloc(afe->dev, CLK_NUM, sizeof(*afe_priv->clk),
				     GFP_KERNEL);
	if (!afe_priv->clk)
		return -ENOMEM;

	for (i = 0; i < CLK_NUM; i++) {
		afe_priv->clk[i] = devm_clk_get(afe->dev, aud_clks[i]);
		if (IS_ERR(afe_priv->clk[i])) {
			dev_warn(afe->dev, "%s devm_clk_get %s fail, ret %ld\n",
				 __func__,
				 aud_clks[i], PTR_ERR(afe_priv->clk[i]));
			/*return PTR_ERR(clks[i]);*/
			afe_priv->clk[i] = NULL;
		}
	}

	afe_priv->apmixed = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			    "apmixedsys");
	if (IS_ERR(afe_priv->apmixed)) {
		dev_err(afe->dev, "%s() Cannot find apmixedsys: %ld\n",
			__func__, PTR_ERR(afe_priv->apmixed));
		return PTR_ERR(afe_priv->apmixed);
	}

	afe_priv->topckgen = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
			     "topckgen");
	if (IS_ERR(afe_priv->topckgen)) {
		dev_err(afe->dev, "%s() Cannot find topckgen controller: %ld\n",
			__func__, PTR_ERR(afe_priv->topckgen));
		return PTR_ERR(afe_priv->topckgen);
	}

	afe_priv->infracfg = syscon_regmap_lookup_by_phandle(
				     afe->dev->of_node,
				     "infracfg");
	if (IS_ERR(afe_priv->infracfg)) {
		dev_err(afe->dev, "%s() Cannot find infracfg: %ld\n",
			__func__, PTR_ERR(afe_priv->infracfg));
		return PTR_ERR(afe_priv->infracfg);
	}

	return 0;
}
#endif
