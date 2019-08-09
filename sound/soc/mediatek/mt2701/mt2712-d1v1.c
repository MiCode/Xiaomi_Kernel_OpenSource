/*
 * mt2712_d1v1.c  --  MT2701 CS42448 ALSA SoC machine driver
 *
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Ir Lian <ir.lian@mediatek.com>
 *			  Garlic Tseng <garlic.tseng@mediatek.com>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/of_gpio.h>
#include <sound/pcm_params.h>

#include "mt2701-afe-common.h"

#define TS_CS42448

static const struct snd_soc_dapm_widget
	mt2712_d1v1_asoc_card_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("Line Out Jack", NULL),
	SND_SOC_DAPM_MIC("AMIC", NULL),
};

static const struct snd_kcontrol_new mt2712_d1v1_controls[] = {
	SOC_DAPM_PIN_SWITCH("Line Out Jack"),
	SOC_DAPM_PIN_SWITCH("AMIC"),
};

static int mt2712_d1v1_fe_ops_startup(struct snd_pcm_substream *substream)
{
	int err = 0;

	if (err < 0) {
	dev_err(substream->pcm->card->dev,
		"%s snd_pcm_hw_constraint_list failed: 0x%x\n",
		__func__, err);
	return err;
	}
	return 0;
}

static struct snd_soc_ops mt2712_d1v1_48k_fe_ops = {
	.startup = mt2712_d1v1_fe_ops_startup,
};

static int mt2712_d1v1_be_ops_hw_params(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int mclk_rate;
	unsigned int rate = params_rate(params);
	unsigned int div_mclk_over_bck = rate > 192000U ? 2U : 4U;
	unsigned int div_bck_over_lrck = 64U;
	int ret;

	mclk_rate = rate * div_bck_over_lrck * div_mclk_over_bck;

	/* mt2701 mclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk_rate, SND_SOC_CLOCK_OUT);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	/* codec mclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, mclk_rate, SND_SOC_CLOCK_IN);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_NB_NF);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	return 0;
}

static int mt2712_d1v1_be_AK4556VT_ops_hw_params(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	unsigned int mclk_rate;
	unsigned int rate = params_rate(params);
	unsigned int div_mclk_over_lrck = 512;
	int ret;

	if (rate > 54000U && rate < 108000U)
		div_mclk_over_lrck = 256;
	else if (rate > 108000U)
		div_mclk_over_lrck = 128;
	else
		div_mclk_over_lrck = 512;

	mclk_rate = rate * div_mclk_over_lrck;

	/* mt2701 mclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk_rate, SND_SOC_CLOCK_OUT);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	/* codec mclk */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, mclk_rate, SND_SOC_CLOCK_IN);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_NB_NF);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	return 0;
}

static const unsigned int mt2712_cs42448_tdm_channels[] = {2, 8};

static struct snd_pcm_hw_constraint_list mt2701_cs42448_constraints_tdm_channels = {
		.count = ARRAY_SIZE(mt2712_cs42448_tdm_channels),
		.list = mt2712_cs42448_tdm_channels,
		.mask = 0,
};

static int mt2712_d1v1_be_tdm_ops_startup(struct snd_pcm_substream *substream)
{
	int err = 0;

	err = snd_pcm_hw_constraint_list(substream->runtime, 0,
					SNDRV_PCM_HW_PARAM_CHANNELS,
					&mt2701_cs42448_constraints_tdm_channels);
	if (err < 0) {
		dev_err(substream->pcm->card->dev,
			"%s snd_pcm_hw_constraint_list failed: 0x%x\n",
			__func__, err);
		return err;
	}
	return 0;
}

static int mt2712_d1v1_be_tdm_ops_hw_params(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int channel = params_channels(params);
	unsigned int rate = params_rate(params);
	unsigned int mclk_rate;
	unsigned int div_mclk_over_bck = 4;
	unsigned int div_bck_over_lrck = 64;
	int ret = 0;
	unsigned int cpu_dai_fmt, codec_dai_fmt;

	/* set tdmin based on rate */
	switch (channel) {
	case 2:
		cpu_dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_IB_IF;
		codec_dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS;

		if (rate > 100000U)
			div_mclk_over_bck = 1;
		else if (rate > 50000U)
			div_mclk_over_bck = 2;
		else
			div_mclk_over_bck = 4;

		break;
	case 8:
		cpu_dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_IB_NF;
		codec_dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS;

		snd_mask_reset(&params->masks[SNDRV_PCM_HW_PARAM_FORMAT - SNDRV_PCM_HW_PARAM_FIRST_MASK],
				SNDRV_PCM_FORMAT_S16_LE);
		params_set_format(params, SNDRV_PCM_FORMAT_S32_LE);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret != 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, cpu_dai_fmt);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	ret = snd_soc_dai_set_fmt(rtd->codec_dai, codec_dai_fmt);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	mclk_rate = rate * div_bck_over_lrck * div_mclk_over_bck;
	/* mt2701 mclk */
	ret = snd_soc_dai_set_sysclk(cpu_dai, 0, mclk_rate, SND_SOC_CLOCK_OUT);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	/* codec mclk */
	ret = snd_soc_dai_set_sysclk(rtd->codec_dai, 0, mclk_rate, SND_SOC_CLOCK_IN);
	if (ret != 0 && ret != -ENOTSUPP)
		return ret;

	return 0;
}
static int mt2712_d1v1_be_modpcm_ops_hw_params(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_afe *afe = snd_soc_platform_get_drvdata(rtd->platform);
	struct mt2701_afe_private *afe_priv = afe->platform_priv;
	struct clock_ctrl *clk_ctrl = afe_priv->clk_ctrl;

	/* set mclk for d1v1 i2s mclk test*/
	if (afe_priv->i2so2_mclk)
		clk_ctrl->mclk_configuration(afe, 2, 0, params_rate(params)*512U);

	return 0;
}

static struct snd_soc_ops mt2712_d1v1_be_ops = {
	.hw_params = mt2712_d1v1_be_ops_hw_params
};

static struct snd_soc_ops mt2712_d1v1_be_AK4556VT_ops = {
	.hw_params = mt2712_d1v1_be_AK4556VT_ops_hw_params
};

static struct snd_soc_ops mt2712_d1v1_be_modpcm_ops = {
	.hw_params = mt2712_d1v1_be_modpcm_ops_hw_params
};


static struct snd_soc_ops mt2712_d1v1_be_tdm_ops = {
	.startup = mt2712_d1v1_be_tdm_ops_startup,
	.hw_params = mt2712_d1v1_be_tdm_ops_hw_params
};


enum {
	DAI_LINK_FE_MULTI_CH_OUT,
	DAI_LINK_FE_PCMO0_OUT,
	DAI_LINK_FE_PCMO1_OUT,
	DAI_LINK_FE_PCMO2_OUT,
	DAI_LINK_FE_PCMO3_OUT,
	DAI_LINK_FE_PCM0_IN,
	DAI_LINK_FE_PCM1_IN,
	DAI_LINK_FE_PCM2_IN,
	DAI_LINK_FE_AWB2,
	DAI_LINK_FE_BT_OUT,
	DAI_LINK_FE_BT_IN,
	DAI_LINK_FE_MOD_OUT,
	DAI_LINK_FE_MOD_IN,
	DAI_LINK_FE_TDMO0_OUT,
	DAI_LINK_FE_TDMO1_OUT,
	DAI_LINK_FE_TDM_IN,
	DAI_LINK_BE_I2S0,
	DAI_LINK_BE_I2S1,
	DAI_LINK_BE_I2S2,
	DAI_LINK_BE_I2S3,
	DAI_LINK_BE_MRG_BT,
	DAI_LINK_BE_MOD,
	DAI_LINK_BE_TDMO0,
	DAI_LINK_BE_TDMO1,
	DAI_LINK_BE_TDMIN,
	DAI_LINK_BE_AADC,
};

static struct snd_soc_dai_link mt2712_d1v1_dai_links[] = {
	/* FE */
	[DAI_LINK_FE_MULTI_CH_OUT] = {
	.name = "mt2712_d1v1-multi-ch-out",
	.stream_name = "mt2712_d1v1-multi-ch-out",
	.cpu_dai_name = "PCM_multi",
	/* .cpu_dai_name = "TDM_OUT0", */
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_PCMO0_OUT] = {
	.name = "mt2712_d1v1-pcmo0-data-DL",
	.stream_name = "mt2712_d1v1-pcmo0-data-DL",
	.cpu_dai_name = "PCMO0",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_PCMO1_OUT] = {
	.name = "mt2712_d1v1-pcmo1-data-DL",
	.stream_name = "mt2712_d1v1-pcmo1-data-DL",
	.cpu_dai_name = "PCMO1",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_PCMO2_OUT] = {
	.name = "mt2712_d1v1-pcmo2-data-DL",
	.stream_name = "mt2712_d1v1-pcmo2-data-DL",
	.cpu_dai_name = "PCMO2",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_PCMO3_OUT] = {
	.name = "mt2712_d1v1-pcmo3-data-DL",
	.stream_name = "mt2712_d1v1-pcmo3-data-DL",
	.cpu_dai_name = "PCMO3",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_PCM0_IN] = {
	.name = "mt2712_d1v1-pcm0",
	.stream_name = "mt2712_d1v1-pcm0-data-UL",
	.cpu_dai_name = "PCM0",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_FE_PCM1_IN] = {
	.name = "mt2712_d1v1-pcm1-data-UL",
	.stream_name = "mt2712_d1v1-pcm1-data-UL",
	.cpu_dai_name = "PCM1",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_FE_PCM2_IN] = {
	.name = "mt2712_d1v1-pcm2-data-UL",
	.stream_name = "mt2712_d1v1-pcm2-data-UL",
	.cpu_dai_name = "PCM2",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_FE_AWB2] = {
	.name = "mt2712_d1v1-awb2-data-UL",
	.stream_name = "mt2712_d1v1-awb2-data-UL",
	.cpu_dai_name = "PCM_AWB2",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_FE_BT_OUT] = {
	.name = "mt2712_d1v1-pcm-BT-out",
	.stream_name = "mt2712_d1v1-pcm-BT",
	.cpu_dai_name = "PCM_BT_DL",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_BT_IN] = {
	.name = "mt2712_d1v1-pcm-BT-in",
	.stream_name = "mt2712_d1v1-pcm-BT",
	.cpu_dai_name = "PCM_BT_UL",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.dynamic = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_FE_MOD_OUT] = {
	.name = "mt2712_d1v1-pcm-MOD-out",
	.stream_name = "mt2712_d1v1-pcm-MOD",
	.cpu_dai_name = "PCM_MOD_DL",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_MOD_IN] = {
	.name = "mt2712_d1v1-pcm-MOD-in",
	.stream_name = "mt2712_d1v1-pcm-MOD",
	.cpu_dai_name = "PCM_MOD_UL",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.dynamic = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_FE_TDMO0_OUT] = {
	.name = "mt2712_d1v1-pcm-TDM-out0",
	.stream_name = "mt2712_d1v1-pcm-TDM-out0",
	.cpu_dai_name = "PCM_TDMO0",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_TDMO1_OUT] = {
	.name = "mt2712_d1v1-pcm-TDM-out1",
	.stream_name = "mt2712_d1v1-pcm-TDM-out1",
	.cpu_dai_name = "PCM_TDMO1",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_POST,
		SND_SOC_DPCM_TRIGGER_POST
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_playback = 1,
	},
	[DAI_LINK_FE_TDM_IN] = {
	.name = "mt2712_d1v1-pcm-TDM-in",
	.stream_name = "mt2712_d1v1-pcm-TDM-in",
	.cpu_dai_name = "PCM_TDMIN",
	.codec_name = "snd-soc-dummy",
	.codec_dai_name = "snd-soc-dummy-dai",
	.trigger = {
		SND_SOC_DPCM_TRIGGER_PRE,
		SND_SOC_DPCM_TRIGGER_PRE
	},
	.ops = &mt2712_d1v1_48k_fe_ops,
	.dynamic = 1,
	.dpcm_capture = 1,
	},
	/* BE */
	[DAI_LINK_BE_I2S0] = {
	.name = "mt2712_d1v1-I2S0",
	.cpu_dai_name = "I2S0",
	.no_pcm = 1,
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-i2s",
	.ops = &mt2712_d1v1_be_ops,
	.dpcm_playback = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_BE_I2S1] = {
	.name = "mt2712_d1v1-I2S1",
	.cpu_dai_name = "I2S1",
	.no_pcm = 1,
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-i2s",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
	| SND_SOC_DAIFMT_GATED,
	.ops = &mt2712_d1v1_be_AK4556VT_ops,
	.dpcm_playback = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_BE_I2S2] = {
	.name = "mt2712_d1v1-I2S2",
	.cpu_dai_name = "I2S2",
	.no_pcm = 1,
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-i2s",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
	| SND_SOC_DAIFMT_GATED,
	.ops = &mt2712_d1v1_be_AK4556VT_ops,
	.dpcm_playback = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_BE_I2S3] = {
	.name = "mt2712_d1v1-I2S3",
	.cpu_dai_name = "I2S3",
	.no_pcm = 1,
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-i2s",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS
	| SND_SOC_DAIFMT_GATED,
	.ops = &mt2712_d1v1_be_ops,
	.dpcm_playback = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_BE_MRG_BT] = {
	.name = "mt2712_d1v1-MRG-BT",
	.cpu_dai_name = "MRG BT",
	.no_pcm = 1,
	.codec_dai_name = "bt-sco-pcm-wb",
	.dpcm_playback = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_BE_MOD] = {
	.name = "mt2712_d1v1-MOD",
	.cpu_dai_name = "MOD PCM",
	.no_pcm = 1,
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-mod",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS,
	.ops = &mt2712_d1v1_be_modpcm_ops,
	.dpcm_playback = 1,
	.dpcm_capture = 1,
	},
	[DAI_LINK_BE_TDMO0] = {
	.name = "mt2712_d1v1-TDM-OUT0",
	.cpu_dai_name = "TDMO0",
	.no_pcm = 1,
	#ifdef TS_CS42448
	.codec_dai_name = "cs42448",
	#else
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-tdm",
	#endif
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS
	| SND_SOC_DAIFMT_GATED,
	.ops = &mt2712_d1v1_be_tdm_ops,
	.dpcm_playback = 1,
	},
	[DAI_LINK_BE_TDMO1] = {
	.name = "mt2712_d1v1-TDM-OUT1",
	.cpu_dai_name = "TDMO1",
	.no_pcm = 1,
	#ifdef TS_CS42448
	.codec_dai_name = "cs42448",
	#else
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-tdm",
	#endif
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS
		| SND_SOC_DAIFMT_GATED,
	.ops = &mt2712_d1v1_be_tdm_ops,
	.dpcm_playback = 1,
	},
	[DAI_LINK_BE_TDMIN] = {
	.name = "mt2712_d1v1-TDM-IN",
	.cpu_dai_name = "TDMIN",
	.no_pcm = 1,
	#ifdef TS_CS42448
	.codec_dai_name = "cs42448",
	#else
	.codec_name = "dummy-codec",
	.codec_dai_name = "dummy-codec-tdmin",
	#endif
	.dai_fmt = SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_CBS_CFS
	| SND_SOC_DAIFMT_GATED,
	.ops = &mt2712_d1v1_be_tdm_ops,
	.dpcm_capture = 1,
	},
	[DAI_LINK_BE_AADC] = {
	.name = "mt2712_d1v1-AADC",
	.cpu_dai_name = "AADC",
	.no_pcm = 1,
	.codec_name = "mt2712-codec",
	.codec_dai_name = "mt2712-codec-aadc",
	.dpcm_capture = 1,
	},
};

static struct snd_soc_card mt2712_d1v1_soc_card = {
	.name = "mt2712-d1v1",
	.owner = THIS_MODULE,
	.dai_link = mt2712_d1v1_dai_links,
	.num_links = ARRAY_SIZE(mt2712_d1v1_dai_links),
	.controls = mt2712_d1v1_controls,
	.num_controls = ARRAY_SIZE(mt2712_d1v1_controls),
	.dapm_widgets = mt2712_d1v1_asoc_card_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(mt2712_d1v1_asoc_card_dapm_widgets),
};

static int mt2712_d1v1_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt2712_d1v1_soc_card;
	int ret;
	int i;
	struct device_node *platform_node, *codec_node_bt_mrg;
#ifdef TS_CS42448
	struct device_node *codec_node_tdmo, *codec_node_tdmo1, *codec_node_tdmi;
#endif
	struct device *dev = &pdev->dev;

	platform_node = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,platform", 0);
	if (platform_node == NULL) {
		dev_notice(&pdev->dev, "Property 'platform' missing or invalid\n");
		return -EINVAL;
	}

	for (i = 0; i < card->num_links; i++) {
		if (mt2712_d1v1_dai_links[i].platform_name != NULL)
			continue;

		mt2712_d1v1_dai_links[i].platform_of_node = platform_node;
	}

	card->dev = dev;
	codec_node_bt_mrg = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,audio-codec-bt-mrg", 0);
	if (codec_node_bt_mrg == NULL) {
		dev_notice(&pdev->dev,
			"Property 'audio-codec-bt-mrg' missing or invalid\n");
		return -EINVAL;
	}
	mt2712_d1v1_dai_links[DAI_LINK_BE_MRG_BT].codec_of_node
		= codec_node_bt_mrg;
#ifdef TS_CS42448
	codec_node_tdmo = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,audio-codec-tdmo", 0);
	if (codec_node_tdmo == NULL) {
		dev_notice(&pdev->dev,
			"Property 'audio-codec-tdmo' missing or invalid\n");
		return -EINVAL;
	}
	mt2712_d1v1_dai_links[DAI_LINK_BE_TDMO0].codec_of_node
		= codec_node_tdmo;

	codec_node_tdmo1 = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,audio-codec-tdmo1", 0);
	if (codec_node_tdmo1 == NULL) {
		dev_notice(&pdev->dev,
			"Property 'audio-codec-tdmo1' missing or invalid\n");
		return -EINVAL;
	}
	mt2712_d1v1_dai_links[DAI_LINK_BE_TDMO1].codec_of_node
		 = codec_node_tdmo1;

	codec_node_tdmi = of_parse_phandle(pdev->dev.of_node,
					 "mediatek,audio-codec-tdmi", 0);
	if (codec_node_tdmi == NULL) {
		dev_notice(&pdev->dev,
			"Property 'audio-codec-tdmi' missing or invalid\n");
		return -EINVAL;
	}
	mt2712_d1v1_dai_links[DAI_LINK_BE_TDMIN].codec_of_node
		= codec_node_tdmi;

#endif

	ret = snd_soc_of_parse_audio_routing(card, "audio-routing");
	if (ret != 0) {
		dev_notice(&pdev->dev, "failed to parse audio-routing: %d\n", ret);
		return ret;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);

	if (ret != 0)
		dev_notice(&pdev->dev, "%s snd_soc_register_card fail %d\n",
						__func__, ret);

	return ret;
}

#ifdef CONFIG_OF
static const struct of_device_id mt2712_d1v1_machine_dt_match[] = {
	{.compatible = "mediatek,mt2712-d1v1-machine",},
	{}
};
#endif

static struct platform_driver mt2712_d1v1_machine = {
	.driver = {
	.name = "mt2712-d1v1",
#ifdef CONFIG_OF
	.of_match_table = mt2712_d1v1_machine_dt_match,
#endif
	},
	.probe = mt2712_d1v1_machine_probe,
};

module_platform_driver(mt2712_d1v1_machine);

/* Module information */
MODULE_DESCRIPTION("MT2712 d1v1 ALSA SoC machine driver");
MODULE_AUTHOR("Ir Lian <ir.lian@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("mt2712 d1v1 soc card");
