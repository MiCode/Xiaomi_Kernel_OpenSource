/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include "mt_afe_def.h"
#include <linux/types.h>
#include "mt_afe_control.h"
#include "mt_afe_debug.h"
#include <linux/module.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "../../codecs/rt5659.h"

static struct snd_soc_jack mt8173_headset;

/* Headset jack detection DAPM pins */
static struct snd_soc_jack_pin mt8173_headset_pins[] = {
	{
		.pin = "Headset Mic",
		.mask = SND_JACK_MICROPHONE,
	},
	{
		.pin = "Headphone",
		.mask = SND_JACK_HEADPHONE,
	},
};

static const struct snd_soc_pcm_stream rt5659_spk_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 48000,
	.rate_max = 192000,
	.channels_min = 1,
	.channels_max = 2,
};

static int mt8173_codec_init(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_soc_dai *codec_dai = runtime->codec_dai;
	struct snd_soc_codec *codec = codec_dai->codec;
	int ret;

	ret = snd_soc_jack_new(codec, "Headset",
		SND_JACK_HEADSET | SND_JACK_BTN_0 |
		SND_JACK_BTN_1 | SND_JACK_BTN_2 | SND_JACK_BTN_3, &mt8173_headset);
	if (ret) {
		printk("snd_soc_jack_new failed\n");
		return ret;
	}

	ret = snd_soc_jack_add_pins(&mt8173_headset,
				ARRAY_SIZE(mt8173_headset_pins),
				mt8173_headset_pins);
	if (ret) {
		printk("snd_soc_jack_add_pins failed\n");
		return ret;
	}

	rt5659_set_jack_detect(codec, &mt8173_headset);

	mt_afe_set_init();

	return 0;
}

static const struct snd_soc_pcm_stream nxp_tfa98xx_params = {
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rate_min = 48000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.rates = SNDRV_PCM_RATE_48000,
	.sig_bits = 16,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8173_evb_dais[] = {
	/* FrontEnd DAI Links */
	{
	 .name = "MultiMedia1",
	 .stream_name = MT_SOC_DL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_PCM,
	 .codec_name = "rt5659.9-001b",
	 .codec_dai_name = "rt5659-aif1",
	 .init = mt8173_codec_init,
	 },
	{
	 .name = "MultiMedia2",
	 .stream_name = MT_SOC_UL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_UL1_PCM,
	 .codec_name = "rt5659.9-001b",
	 .codec_dai_name = "rt5659-aif2",
	 },
	{
	 .name = "HDMI_PCM_OUTPUT",
	 .stream_name = MT_SOC_HDMI_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_CPU_DAI_NAME,
	 .platform_name = MT_SOC_HDMI_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "BTSCO",
	 .stream_name = MT_SOC_BTSCO_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_BTSCO_CPU_DAI_NAME,
	 .platform_name = MT_SOC_BTSCO_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "DL1AWB_CAPTURE",
	 .stream_name = MT_SOC_DL1_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_AWB_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "MultiMedia2_Capture",
	 .stream_name = MT_SOC_UL2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL2_CPU_DAI_NAME,
	 .platform_name = MT_SOC_UL2_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "HDMI_RAW_OUTPUT",
	 .stream_name = MT_SOC_HDMI_RAW_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_HDMI_RAW_CPU_DAI_NAME,
	 .platform_name = MT_SOC_HDMI_RAW_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "SPDIF_OUTPUT",
	 .stream_name = MT_SOC_SPDIF_PLAYBACK_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_SPDIF_CPU_DAI_NAME,
	 .platform_name = MT_SOC_SPDIF_PLATFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	 {
	 .name = "I2S0_AWB_CAPTURE",
	 .stream_name = MT_SOC_I2S0_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_I2S0_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_I2S0_AWB_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	{
	 .name = "MRGRX",
	 .stream_name = MT_SOC_MRGRX_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_MRGRX_CPU_DAI_NAME,
	 .platform_name = MT_SOC_MRGRX_PLARFORM_NAME,
	 .codec_name = "rt5659.9-001b",
	 .codec_dai_name = "rt5659-aif1",
	 },
	{
	 .name = "MRGRX_CAPTURE",
	 .stream_name = MT_SOC_MRGRX_AWB_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_MRGRX_AWB_CPU_DAI_NAME,
	 .platform_name = MT_SOC_MRGRX_AWB_PLARFORM_NAME,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	 {
	 .name = "MultiMedia_DL2",
	 .stream_name = MT_SOC_DL2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL2_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL2_PCM,
	 .codec_name = "rt5659.9-001b",
	 .codec_dai_name = "rt5659-aif1",
	 },
	{
	 .name = "PLATOFRM_CONTROL",
	 .stream_name = MT_SOC_ROUTING_STREAM_NAME,
	 .cpu_dai_name = "snd-soc-dummy-dai",
	 .platform_name = MT_SOC_ROUTING_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 .no_pcm = 1,
	 },
	 {
	 .name = "BTSCO2",
	 .stream_name = MT_SOC_BTSCO2_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_BTSCO2_CPU_DAI_NAME,
	 .platform_name = MT_SOC_BTSCO2_PCM,
	 .codec_name = "snd-soc-dummy",
	 .codec_dai_name = "snd-soc-dummy-dai",
	 },
	/* codec to codec dai link */
	{
	 .name = "Smart Amp EC",
	 .stream_name = "rt5659 Speaker",
	 .codec_name = "rt5659.9-001b",
	 .cpu_dai_name = "rt5659-aif2",
	 .codec_dai_name = "rt5659-aif3",
	 .dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM,
	 .params = &rt5659_spk_params,
	},
	{
	.name = "Left TFA98xx Speaker Port",
	.stream_name = "Left TFA98xx Speaker",
	.cpu_dai_name = "rt5659-aif3",
	.codec_name = "tfa98xx.9-0034",
	.codec_dai_name = "tfa98xx-aif-9-34",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFS,
	.params = &nxp_tfa98xx_params,
	},
	{
	.name = "Right TFA98xx Speaker Port",
	.stream_name = "Right TFA98xx Speaker",
	.cpu_dai_name = "rt5659-aif3",
	.codec_name = "tfa98xx.9-0037",
	.codec_dai_name = "tfa98xx-aif-9-37",
	.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFS,
	.params = &nxp_tfa98xx_params,
	},
};

static int mt8173_evb_channel_cap_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt8173_evb_channel_cap_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mt_afe_get_board_channel_type();
	return 0;
}

static const char *const mt8173_evb_channel_cap[] = {
	"Stereo", "MonoLeft", "MonoRight"
};

static const struct soc_enum mt8173_evb_control_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8173_evb_channel_cap), mt8173_evb_channel_cap),
};

static const struct snd_kcontrol_new mt8173_evb_controls[] = {
	SOC_ENUM_EXT("Board Channel Config", mt8173_evb_control_enum[0],
		     mt8173_evb_channel_cap_get,
		     mt8173_evb_channel_cap_set),
};

static const struct snd_soc_dapm_route mt8173_audio_map[] = {
	{"AIF Playback-9-34", NULL, "AIF3 Capture"},
	{"AIF Playback-9-37", NULL, "AIF3 Capture"},
};

static int mt8173_evb_set_bias_level(struct snd_soc_card *card,
				struct snd_soc_dapm_context *dapm,
				enum snd_soc_bias_level level)
{
	pr_debug("%s level %d\n", __func__, level);

	switch (level) {
	case SND_SOC_BIAS_PREPARE:
		mt_afe_set_8173_mclk(true);
		break;
	case SND_SOC_BIAS_OFF:
		pr_err("%s do not close mclk", __func__);
		/* mt_afe_set_8173_mclk(false); */
		mt_afe_set_8173_mclk(true);
		break;
	default:
		break;
	}

	return 0;
}


static struct snd_soc_card mt8173_evb_card = {
	.name = "mt-snd-card",
	.set_bias_level = mt8173_evb_set_bias_level,
	.dai_link = mt8173_evb_dais,
	.num_links = ARRAY_SIZE(mt8173_evb_dais),
	.controls = mt8173_evb_controls,
	.num_controls = ARRAY_SIZE(mt8173_evb_controls),
	.dapm_routes = mt8173_audio_map,
	.num_dapm_routes = ARRAY_SIZE(mt8173_audio_map),
};

static int mt8173_evb_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8173_evb_card;
	struct device *dev = &pdev->dev;
	int ret;

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_MACHINE_NAME);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	card->dev = dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		pr_err("%s snd_soc_register_card fail %d\n", __func__, ret);
		return ret;
	}

	ret = mt_afe_platform_init(dev);
	if (ret) {
		pr_err("%s mt_afe_platform_init fail %d\n", __func__, ret);
		snd_soc_unregister_card(card);
		return ret;
	}

	mt_afe_debug_init();

	return 0;
}

static int mt8173_evb_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	mt_afe_platform_deinit(&pdev->dev);

	mt_afe_debug_deinit();

	return 0;
}

static const struct of_device_id mt8173_evb_machine_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_MACHINE_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt8173_evb_machine_dt_match);

static struct platform_driver mt8173_evb_machine_driver = {
	.driver = {
		   .name = MT_SOC_MACHINE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt8173_evb_machine_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
		   },
	.probe = mt8173_evb_dev_probe,
	.remove = mt8173_evb_dev_remove,
};

module_platform_driver(mt8173_evb_machine_driver);

/* Module information */
MODULE_DESCRIPTION("ASoC driver for MT8173 EVB");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
