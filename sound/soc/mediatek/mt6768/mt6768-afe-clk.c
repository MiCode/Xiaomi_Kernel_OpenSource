// SPDX-License-Identifier: GPL-2.0
//
// mt6768-afe-clk.c  --  Mediatek 6768 afe clock ctrl
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Michael Hsiao <michael.hsiao@mediatek.com>

#include <linux/clk.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "mt6768-afe-common.h"
#include "mt6768-afe-clk.h"

#if !defined(CONFIG_FPGA_EARLY_PORTING)
#include <mtk_idle.h>
#include <mtk_spm_resource_req.h>
#endif

static DEFINE_MUTEX(mutex_request_dram);

enum {
	CLK_AFE = 0,
	/*CLK_DAC,*/
	/*CLK_DAC_PREDIS,*/
	/*CLK_ADC,*/
	CLK_TML,
	CLK_APLL22M,
	CLK_APLL24M,
	CLK_APLL1_TUNER,
#ifdef SCP_USAGE
	CLK_SCP_SYS_AUD,
#endif
	CLK_INFRA_AXI,
	CLK_INFRA_26M,
	CLK_MUX_AUDIO,
	CLK_MUX_AUDIOINTBUS,
	CLK_TOP_SYSPLL1_D4,
	/* apll related mux */
	CLK_TOP_MUX_AUD_1,
	CLK_TOP_APLL1_CK,
	CLK_TOP_MUX_AUD_ENG1,
	CLK_TOP_APLL1_D8,
	CLK_TOP_I2S0_M_SEL,
	CLK_TOP_I2S1_M_SEL,
	CLK_TOP_I2S2_M_SEL,
	CLK_TOP_I2S3_M_SEL,
	CLK_TOP_APLL12_DIV0,
	CLK_TOP_APLL12_DIV1,
	CLK_TOP_APLL12_DIV2,
	CLK_TOP_APLL12_DIV3,
	CLOCK_APMIXED_APLL1,
	CLK_CLK26M,
	CLK_NUM
};

static const char *aud_clks[CLK_NUM] = {
	[CLK_AFE] = "aud_afe_clk",
	/*[CLK_DAC] = "aud_dac_clk",*/
	/*[CLK_DAC_PREDIS] = "aud_dac_predis_clk",*/
	/*[CLK_ADC] = "aud_adc_clk",*/
	[CLK_TML] = "aud_tml_clk",
	[CLK_APLL22M] = "aud_apll22m_clk",
	[CLK_APLL24M] = "aud_apll24m_clk",
	[CLK_APLL1_TUNER] = "aud_apll1_tuner_clk",
#ifdef SCP_USAGE
	[CLK_SCP_SYS_AUD] = "scp_sys_audio",
#endif
	[CLK_INFRA_AXI] = "aud_infra_axi_clk",
	[CLK_INFRA_26M] = "aud_infra_26m_clk",
	[CLK_MUX_AUDIO] = "top_mux_audio",
	[CLK_MUX_AUDIOINTBUS] = "top_mux_audio_int",
	[CLK_TOP_SYSPLL1_D4] = "top_sys_pll1_d4",
	[CLK_TOP_MUX_AUD_1] = "top_mux_aud_1",
	[CLK_TOP_APLL1_CK] = "top_apll1_ck",
	[CLK_TOP_MUX_AUD_ENG1] = "top_mux_aud_eng1",
	[CLK_TOP_APLL1_D8] = "top_apll1_d8",
	[CLK_TOP_I2S0_M_SEL] = "top_i2s0_m_sel",
	[CLK_TOP_I2S1_M_SEL] = "top_i2s1_m_sel",
	[CLK_TOP_I2S2_M_SEL] = "top_i2s2_m_sel",
	[CLK_TOP_I2S3_M_SEL] = "top_i2s3_m_sel",
	[CLK_TOP_APLL12_DIV0] = "top_apll12_div0",
	[CLK_TOP_APLL12_DIV1] = "top_apll12_div1",
	[CLK_TOP_APLL12_DIV2] = "top_apll12_div2",
	[CLK_TOP_APLL12_DIV3] = "top_apll12_div3",
	[CLOCK_APMIXED_APLL1] = "apmixed_apll1",
	[CLK_CLK26M] = "top_clk26m_clk",
};

static int mt6768_set_audio_int_bus_parent(struct mtk_base_afe *afe,
					   int clk_id)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIOINTBUS],
			     afe_priv->clk[clk_id]);
	if (ret) {
		dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
		       __func__, aud_clks[CLK_MUX_AUDIOINTBUS],
		       aud_clks[clk_id], ret);
	}

	return ret;
}

static int apll1_mux_setting(struct mtk_base_afe *afe, bool enable)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
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

		/* 180.6336 / 8 = 22.5792MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D8]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
			       __func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
			       aud_clks[CLK_TOP_APLL1_D8], ret);
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
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
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

		/* 196.608 / 8 = 24.576MHz */
		ret = clk_prepare_enable(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1]);
		if (ret) {
			dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1], ret);
			goto EXIT;
		}
		ret = clk_set_parent(afe_priv->clk[CLK_TOP_MUX_AUD_ENG1],
				     afe_priv->clk[CLK_TOP_APLL1_D8]);
		if (ret) {
			dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
				__func__, aud_clks[CLK_TOP_MUX_AUD_ENG1],
				aud_clks[CLK_TOP_APLL1_D8], ret);
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


int mt6768_afe_enable_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret = 0;

	dev_info(afe->dev, "%s()\n", __func__);
#ifdef SCP_USAGE
	ret = clk_prepare_enable(afe_priv->clk[CLK_SCP_SYS_AUD]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_SCP_SYS_AUD], ret);
		goto CLK_SCP_SYS_AUD_ERR;
	}
#endif
	ret = clk_prepare_enable(afe_priv->clk[CLK_INFRA_AXI]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_INFRA_AXI], ret);
		goto CLK_INFRA_AXI_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_INFRA_26M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_INFRA_26M], ret);
		goto CLK_INFRA_26M_ERR;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIO]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIO], ret);
		goto CLK_MUX_AUDIO_ERR;
	}
	ret = clk_set_parent(afe_priv->clk[CLK_MUX_AUDIO],
			     afe_priv->clk[CLK_CLK26M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_set_parent %s-%s fail %d\n",
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
	ret = mt6768_set_audio_int_bus_parent(afe,
					      CLK_TOP_SYSPLL1_D4);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_PARENT_ERR;

	ret = clk_prepare_enable(afe_priv->clk[CLK_AFE]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_AFE], ret);
		goto CLK_AFE_ERR;
	}

	return 0;

CLK_AFE_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);
CLK_MUX_AUDIO_INTBUS_PARENT_ERR:
	mt6768_set_audio_int_bus_parent(afe, CLK_CLK26M);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
CLK_MUX_AUDIO_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);
CLK_INFRA_26M_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_26M]);
CLK_INFRA_AXI_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_AXI]);
#ifdef SCP_USAGE
CLK_SCP_SYS_AUD_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_SCP_SYS_AUD]);
#endif
	return ret;

}

void mt6768_afe_disable_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	dev_info(afe->dev, "%s()\n", __func__);

	clk_disable_unprepare(afe_priv->clk[CLK_AFE]);

	mt6768_set_audio_int_bus_parent(afe, CLK_CLK26M);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIO]);

	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_26M]);
	clk_disable_unprepare(afe_priv->clk[CLK_INFRA_AXI]);
#ifdef SCP_USAGE
	clk_disable_unprepare(afe_priv->clk[CLK_SCP_SYS_AUD]);
#endif
}

int mt6768_afe_suspend_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* set audio int bus to 26M */
	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6768_set_audio_int_bus_parent(afe, CLK_CLK26M);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_PARENT_ERR;

	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);

	return 0;

CLK_MUX_AUDIO_INTBUS_PARENT_ERR:
	mt6768_set_audio_int_bus_parent(afe, CLK_TOP_SYSPLL1_D4);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	return ret;
}

int mt6768_afe_resume_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

	/* set audio int bus to normal working clock */
	ret = clk_prepare_enable(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_MUX_AUDIOINTBUS], ret);
		goto CLK_MUX_AUDIO_INTBUS_ERR;
	}
	ret = mt6768_set_audio_int_bus_parent(afe,
					      CLK_TOP_SYSPLL1_D4);
	if (ret)
		goto CLK_MUX_AUDIO_INTBUS_PARENT_ERR;

	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);

	return 0;

CLK_MUX_AUDIO_INTBUS_PARENT_ERR:
	mt6768_set_audio_int_bus_parent(afe, CLK_CLK26M);
CLK_MUX_AUDIO_INTBUS_ERR:
	clk_disable_unprepare(afe_priv->clk[CLK_MUX_AUDIOINTBUS]);
	return ret;
}

int mt6768_afe_dram_request(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	dev_info(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (afe_priv->dram_resource_counter == 0)
		spm_resource_req(SPM_RESOURCE_USER_AUDIO, SPM_RESOURCE_ALL);
#endif
	afe_priv->dram_resource_counter++;
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6768_afe_dram_release(struct device *dev)
{
	struct mtk_base_afe *afe = dev_get_drvdata(dev);
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	dev_info(dev, "%s(), dram_resource_counter %d\n",
		 __func__, afe_priv->dram_resource_counter);

	mutex_lock(&mutex_request_dram);
	afe_priv->dram_resource_counter--;
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (afe_priv->dram_resource_counter == 0)
		spm_resource_req(SPM_RESOURCE_USER_AUDIO, SPM_RESOURCE_RELEASE);
#endif

	if (afe_priv->dram_resource_counter < 0) {
		dev_warn(dev, "%s(), dram_resource_counter %d\n",
			 __func__, afe_priv->dram_resource_counter);
		afe_priv->dram_resource_counter = 0;
	}
	mutex_unlock(&mutex_request_dram);
	return 0;
}

int mt6768_apll1_enable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

    /* 180.6336 / 8 = 22.5792MHz, 8: default value */
	regmap_update_bits(afe_priv->topckgen,
			CLK_AUDDIV_0, 0xf << 24, 7 << 24);

	/* MT6768 has only 1 APLL */
	/* APLL rate can not be set by select clock source in CCF */
	clk_set_rate(afe_priv->clk[CLOCK_APMIXED_APLL1], 180633600);

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

	/* Set by CCF control CLOCK_APLL1_TUNER */
	regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG,
			   0x0000FFF7, 0x00000432);
	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x1);

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

void mt6768_apll1_disable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_22M_ON_MASK_SFT,
			   0x0 << AFE_22M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x0);

	/* Set by CCF control CLOCK_APLL1_TUNER */
	regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL22M]);

	apll1_mux_setting(afe, false);
}

int mt6768_apll2_enable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
	int ret;

    /* 196.608 / 8 = 24.576MHz, 8: default value */
	regmap_update_bits(afe_priv->topckgen,
			   CLK_AUDDIV_0, 0xf << 24, 7 << 24);

	/* MT6768 has only 1 APLL */
	/* APLL rate can not be set by select clock source in CCF */
	clk_set_rate(afe_priv->clk[CLOCK_APMIXED_APLL1], 196608000);

	/* setting for APLL */
	apll2_mux_setting(afe, true);

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL24M]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL24M], ret);
		goto ERR_CLK_APLL24M;
	}

	ret = clk_prepare_enable(afe_priv->clk[CLK_APLL1_TUNER]);
	if (ret) {
		dev_err(afe->dev, "%s clk_prepare_enable %s fail %d\n",
			__func__, aud_clks[CLK_APLL1_TUNER], ret);
		goto ERR_CLK_APLL2_TUNER;
	}

	/* Set by CCF control CLOCK_APLL1_TUNER */
	regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG,
			0x0000FFF7, 0x00000434);
	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x1);

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			AFE_24M_ON_MASK_SFT,
			0x1 << AFE_24M_ON_SFT);

	return 0;

ERR_CLK_APLL2_TUNER:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
ERR_CLK_APLL24M:
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);

	return ret;

	return 0;
}

void mt6768_apll2_disable(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	regmap_update_bits(afe->regmap, AFE_HD_ENGEN_ENABLE,
			   AFE_24M_ON_MASK_SFT,
			   0x0 << AFE_24M_ON_SFT);

	regmap_update_bits(afe->regmap, AFE_APLL_TUNER_CFG, 0x1, 0x0);

	/* Set by CCF control CLOCK_APLL1_TUNER */
	regmap_update_bits(afe_priv->apmixed, AP_PLL_CON3, 0x1, 0x0);

	clk_disable_unprepare(afe_priv->clk[CLK_APLL1_TUNER]);
	clk_disable_unprepare(afe_priv->clk[CLK_APLL24M]);

	apll2_mux_setting(afe, false);
}

int mt6768_get_apll_rate(struct mtk_base_afe *afe, int apll)
{
	return (apll == MT6768_APLL1) ? 180633600 : 196608000;
}

int mt6768_get_apll_by_rate(struct mtk_base_afe *afe, int rate)
{
	return ((rate % 8000) == 0) ? MT6768_APLL2 : MT6768_APLL1;
}

int mt6768_get_apll_by_name(struct mtk_base_afe *afe, const char *name)
{
	if (strcmp(name, APLL1_W_NAME) == 0)
		return MT6768_APLL1;
	else
		return MT6768_APLL2;
}

/* mck */
struct mt6768_mck_div {
	int m_sel_id;
	int div_clk_id;
};

static const struct mt6768_mck_div mck_div[MT6768_MCK_NUM] = {
	[MT6768_I2S0_MCK] = {
		.m_sel_id = CLK_TOP_I2S0_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV0,
	},
	[MT6768_I2S1_MCK] = {
		.m_sel_id = CLK_TOP_I2S1_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV1,
	},
	[MT6768_I2S2_MCK] = {
		.m_sel_id = CLK_TOP_I2S2_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV2,
	},
	[MT6768_I2S3_MCK] = {
		.m_sel_id = CLK_TOP_I2S3_M_SEL,
		.div_clk_id = CLK_TOP_APLL12_DIV3,
	},
};

int mt6768_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

    /* MT6768 has only 1 APLL */
	int apll_clk_id = CLK_TOP_MUX_AUD_1;
	int m_sel_id = mck_div[mck_id].m_sel_id;
	int div_clk_id = mck_div[mck_id].div_clk_id;
	int ret;

	/* select apll */
	if (m_sel_id >= 0) {
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

	return 0;
}

void mt6768_mck_disable(struct mtk_base_afe *afe, int mck_id)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;

	int m_sel_id = mck_div[mck_id].m_sel_id;
	int div_clk_id = mck_div[mck_id].div_clk_id;

	clk_disable_unprepare(afe_priv->clk[div_clk_id]);
	if (m_sel_id >= 0)
		clk_disable_unprepare(afe_priv->clk[m_sel_id]);
}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
enum {
	aud_intbus_sel_26m = 0,
	aud_intbus_sel_syspll_d1_d4,
	aud_intbus_sel_syspll_d4_d2,
};

static int mt6768_afe_idle_notify_call(struct notifier_block *nfb,
				       unsigned long id,
				       void *arg)
{
	switch (id) {
	case NOTIFY_DPIDLE_ENTER:
	case NOTIFY_SOIDLE_ENTER:
		aud_intbus_mux_sel(aud_intbus_sel_26m);
		break;
	case NOTIFY_DPIDLE_LEAVE:
	case NOTIFY_SOIDLE_LEAVE:
		aud_intbus_mux_sel(aud_intbus_sel_syspll_d1_d4);
		break;
	case NOTIFY_SOIDLE3_ENTER:
	case NOTIFY_SOIDLE3_LEAVE:
	default:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block mt6768_afe_idle_nfb = {
	.notifier_call = mt6768_afe_idle_notify_call,
};
#endif

int mt6768_init_clock(struct mtk_base_afe *afe)
{
	struct mt6768_afe_private *afe_priv = afe->platform_priv;
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
							    "apmixed");
	if (IS_ERR(afe_priv->apmixed)) {
		dev_err(afe->dev, "Cannot find apmixed controller: %ld\n",
			PTR_ERR(afe_priv->apmixed));
		return PTR_ERR(afe_priv->apmixed);
	}

	afe_priv->topckgen = syscon_regmap_lookup_by_phandle(afe->dev->of_node,
							     "topckgen");
	if (IS_ERR(afe_priv->topckgen)) {
		dev_err(afe->dev, "Cannot find topckgen controller: %ld\n",
			PTR_ERR(afe_priv->topckgen));
		return PTR_ERR(afe_priv->topckgen);
	}

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	mtk_idle_notifier_register(&mt6768_afe_idle_nfb);
#endif

	return 0;
}
