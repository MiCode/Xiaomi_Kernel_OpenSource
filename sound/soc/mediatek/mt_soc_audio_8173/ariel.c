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
#include <linux/of_gpio.h>
#include <linux/gpio.h>

enum ext_spk_amp_type {
	SPK_LEFT_AMP = 0,
	SPK_RIGHT_AMP,
	SPK_AMP_MAX,
};

enum mt8173_asoc_enum_type {
	ENUM_BOARD_CH_CONFIG = 0,
	ENUM_SPK_AMP,
};

struct mt8173_asoc_mach_data {
	int spk_amps_gpio[SPK_AMP_MAX];
	struct pinctrl_state *spk_amps_default;
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link mt8173_ariel_dais[] = {
	/* FrontEnd DAI Links */
	{
	 .name = "MultiMedia1",
	 .stream_name = MT_SOC_DL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_DL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_DL1_PCM,
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-tx-dai",
	 },
	{
	 .name = "MultiMedia2",
	 .stream_name = MT_SOC_UL1_STREAM_NAME,
	 .cpu_dai_name = MT_SOC_UL1_CPU_DAI_NAME,
	 .platform_name = MT_SOC_UL1_PCM,
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-rx-dai",
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
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-tx-dai",
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
	 .codec_name = "mt6397-codec",
	 .codec_dai_name = "mt6397-codec-tx-dai",
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
};

static int mt8173_ariel_channel_cap_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	return 0;
}

static int mt8173_ariel_channel_cap_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mt_afe_get_board_channel_type();
	return 0;
}

static int mt8173_ariel_ext_speaker_left_amp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct mt8173_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int amp_gpio;

	amp_gpio = pdata->spk_amps_gpio[SPK_LEFT_AMP];
	if (gpio_is_valid(amp_gpio))
		ucontrol->value.integer.value[0] =
		gpio_get_value(amp_gpio);
	else {
		pr_err("%s: failed to get value of spk_left_amp_gpio\n",
			__func__);
		ucontrol->value.integer.value[0] = 0;
	}
	return 0;
}

static int mt8173_ariel_ext_speaker_left_amp_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct mt8173_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int amp_gpio;

	amp_gpio = pdata->spk_amps_gpio[SPK_LEFT_AMP];
	if (gpio_is_valid(amp_gpio) &&
		(ucontrol->value.integer.value[0] ^
		gpio_get_value(amp_gpio))) {
		gpio_set_value(amp_gpio, ucontrol->value.integer.value[0]);
		pr_debug("%s: turn %s speaker left amp\n", __func__,
			ucontrol->value.integer.value[0] ? "on" : "off");
	}
	return 0;
}

static int mt8173_ariel_ext_speaker_right_amp_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct mt8173_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int amp_gpio;

	amp_gpio = pdata->spk_amps_gpio[SPK_RIGHT_AMP];
	if (gpio_is_valid(amp_gpio))
		ucontrol->value.integer.value[0] =
			gpio_get_value(amp_gpio);
	else {
		pr_err("%s: failed to get value of spk_right_amp_gpio\n",
			__func__);
		ucontrol->value.integer.value[0] = 0;
	}
	return 0;
}

static int mt8173_ariel_ext_speaker_right_amp_set(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);
	struct mt8173_asoc_mach_data *pdata =
			snd_soc_card_get_drvdata(card);
	int amp_gpio;

	amp_gpio = pdata->spk_amps_gpio[SPK_RIGHT_AMP];
	if (gpio_is_valid(amp_gpio) &&
		(ucontrol->value.integer.value[0] ^
		gpio_get_value(amp_gpio))) {
		gpio_set_value(amp_gpio, ucontrol->value.integer.value[0]);
		pr_debug("%s: turn %s speaker right amp\n", __func__,
			ucontrol->value.integer.value[0] ? "on" : "off");
	}
	return 0;
}

static const char *const mt8173_ariel_channel_cap[] = {
	"Stereo", "MonoLeft", "MonoRight"
};

static const char *const mt8173_ariel_amp_func[] = { "Off", "On" };

static const struct soc_enum mt8173_ariel_control_enum[] = {
	[ENUM_BOARD_CH_CONFIG] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8173_ariel_channel_cap),
				    mt8173_ariel_channel_cap),
	[ENUM_SPK_AMP] =
		SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mt8173_ariel_amp_func),
				    mt8173_ariel_amp_func),
};

static const struct snd_kcontrol_new mt8173_ariel_controls[] = {
	SOC_ENUM_EXT("Board Channel Config",
		mt8173_ariel_control_enum[ENUM_BOARD_CH_CONFIG],
		mt8173_ariel_channel_cap_get,
		mt8173_ariel_channel_cap_set),
	SOC_ENUM_EXT("External_Speaker_Left_Amp_Switch",
		mt8173_ariel_control_enum[ENUM_SPK_AMP],
		mt8173_ariel_ext_speaker_left_amp_get,
		mt8173_ariel_ext_speaker_left_amp_set),
	SOC_ENUM_EXT("External_Speaker_Right_Amp_Switch",
		mt8173_ariel_control_enum[ENUM_SPK_AMP],
		mt8173_ariel_ext_speaker_right_amp_get,
		mt8173_ariel_ext_speaker_right_amp_set),
};

static struct snd_soc_card mt8173_ariel_card = {
	.name = "mt-snd-card",
	.dai_link = mt8173_ariel_dais,
	.num_links = ARRAY_SIZE(mt8173_ariel_dais),
	.controls = mt8173_ariel_controls,
	.num_controls = ARRAY_SIZE(mt8173_ariel_controls),
};

static int mt8173_ariel_dev_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &mt8173_ariel_card;
	struct device *dev = &pdev->dev;
	int ret, i;
	struct pinctrl *pinctrl;
	struct mt8173_asoc_mach_data *pdata = NULL;
	const char *const spk_amps_name[] = {
		"spk-left-amp-gpio",  /* mapping to SPK_LEFT_AMP */
		"spk-right-amp-gpio", /* mapping to SPK_RIGHT_AMP */
	};

	pr_debug("%s dev name %s\n", __func__, dev_name(dev));

	if (dev->of_node) {
		dev_set_name(dev, "%s", MT_SOC_MACHINE_NAME);
		pr_debug("%s set dev name %s\n", __func__, dev_name(dev));
	}

	card->dev = dev;

	ret = snd_soc_register_card(card);
	if (ret) {
		pr_err("%s snd_soc_register_card fail %d\n", __func__, ret);
		goto err;
	}

	ret = mt_afe_platform_init(dev);
	if (ret) {
		pr_err("%s mt_afe_platform_init fail %d\n", __func__, ret);
		snd_soc_unregister_card(card);
		goto err;
	}

	mt_afe_debug_init();

	pdata = devm_kzalloc(dev,
			sizeof(struct mt8173_asoc_mach_data), GFP_KERNEL);

	if (!pdata) {
		pr_err("Can't allocate mt8173_asoc_mach_data\n");
		ret = -ENOMEM;
		goto err;
	}

	snd_soc_card_set_drvdata(card, pdata);

	/* request gpios to control external speaker amplifier */
	for (i = SPK_LEFT_AMP; i < SPK_AMP_MAX; i++) {
		pdata->spk_amps_gpio[i] =
			of_get_named_gpio(dev->of_node, spk_amps_name[i], 0);
		if (gpio_is_valid(pdata->spk_amps_gpio[i])) {
			ret = gpio_request(pdata->spk_amps_gpio[i],
					    spk_amps_name[i]);
			if (!ret) {
				pr_debug("%s: success to request gpio #%d\n",
					__func__, pdata->spk_amps_gpio[i]);
				gpio_direction_output(
					pdata->spk_amps_gpio[i], 0);
			} else
				pr_err("%s: failed to request gpio #%d\n",
					__func__, pdata->spk_amps_gpio[i]);
		} else
			pr_err("%s: gpio #%d is invalid\n", __func__,
				pdata->spk_amps_gpio[i]);
	}

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err("%s: Can't find mt8173 pinctrl\n",
			__func__);
		goto exit;
	}

	pdata->spk_amps_default =
		pinctrl_lookup_state(pinctrl, "spk-amps-default");
	if (IS_ERR(pdata->spk_amps_default)) {
		ret = PTR_ERR(pdata->spk_amps_default);
		pr_err("%s: Can't find spk amps default pinctrl\n",
			__func__);
		goto exit;
	}

	pinctrl_select_state(pinctrl, pdata->spk_amps_default);

exit:
	return 0;
err:
	return ret;
}

static int mt8173_ariel_dev_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	mt_afe_platform_deinit(&pdev->dev);

	mt_afe_debug_deinit();

	return 0;
}

static const struct of_device_id mt8173_ariel_machine_dt_match[] = {
	{.compatible = "mediatek," MT_SOC_MACHINE_NAME,},
	{}
};

MODULE_DEVICE_TABLE(of, mt8173_ariel_machine_dt_match);

static struct platform_driver mt8173_ariel_machine_driver = {
	.driver = {
		   .name = MT_SOC_MACHINE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = mt8173_ariel_machine_dt_match,
#ifdef CONFIG_PM
		   .pm = &snd_soc_pm_ops,
#endif
		   },
	.probe = mt8173_ariel_dev_probe,
	.remove = mt8173_ariel_dev_remove,
};

module_platform_driver(mt8173_ariel_machine_driver);

/* Module information */
MODULE_DESCRIPTION("ASoC driver for MT8173 ARIEL");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:mt-snd-card");
