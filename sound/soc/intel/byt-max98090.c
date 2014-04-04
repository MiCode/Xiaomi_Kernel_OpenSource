/*
 * Intel Baytrail SST MAX98090 machine driver
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/slab.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "../codecs/max98090.h"

struct byt_max98090_private {
	struct snd_soc_jack jack;
};

static const struct snd_soc_dapm_widget byt_max98090_widgets[] = {
	SND_SOC_DAPM_HP("Headphone", NULL),
	SND_SOC_DAPM_MIC("Headset Mic", NULL),
	SND_SOC_DAPM_MIC("Int Mic", NULL),
	SND_SOC_DAPM_SPK("Ext Spk", NULL),
};

static const struct snd_soc_dapm_route byt_max98090_audio_map[] = {
	{"IN34", NULL, "Headset Mic"},
	{"IN34", NULL, "MICBIAS"},
	{"MICBIAS", NULL, "Headset Mic"},
	{"DMICL", NULL, "Int Mic"},
	{"Headphone", NULL, "HPL"},
	{"Headphone", NULL, "HPR"},
	{"Ext Spk", NULL, "SPKL"},
	{"Ext Spk", NULL, "SPKR"},
};

static const struct snd_kcontrol_new byt_max98090_controls[] = {
	SOC_DAPM_PIN_SWITCH("Headphone"),
	SOC_DAPM_PIN_SWITCH("Headset Mic"),
	SOC_DAPM_PIN_SWITCH("Int Mic"),
	SOC_DAPM_PIN_SWITCH("Ext Spk"),
};

static struct snd_soc_jack_pin hs_jack_pins[] = {
	{
		.pin	= "Headphone",
		.mask	= SND_JACK_HEADPHONE,
	},
	{
		.pin	= "Headset Mic",
		.mask	= SND_JACK_MICROPHONE,
	},
	{
		.pin	= "Ext Spk",
		.mask	= SND_JACK_LINEOUT,
	},
	{
		.pin	= "Int Mic",
		.mask	= SND_JACK_LINEIN,
	},
};

static struct snd_soc_jack_gpio hs_jack_gpios[] = {
	{
		.name		= "hp-gpio",
		.report		= SND_JACK_HEADPHONE | SND_JACK_LINEOUT,
		.debounce_time	= 200,
	},
	{
		.name		= "mic-gpio",
		.report		= SND_JACK_MICROPHONE | SND_JACK_LINEIN,
		.debounce_time	= 200,
	},
};

static int byt_max98090_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	struct snd_soc_card *card = runtime->card;
	struct byt_max98090_private *drv = snd_soc_card_get_drvdata(card);
	struct snd_soc_jack *jack = &drv->jack;
	struct gpio_desc *mic_desc;
	struct gpio_desc *hp_desc;

	card->dapm.idle_bias_off = true;

	ret = snd_soc_dai_set_sysclk(runtime->codec_dai,
				     M98090_REG_SYSTEM_CLOCK,
				     19200000, SND_SOC_CLOCK_IN);
	if (ret < 0) {
		dev_err(card->dev, "Can't set codec clock %d\n", ret);
		return ret;
	}

	snd_soc_dapm_enable_pin(dapm, "Headset Mic");
	snd_soc_dapm_enable_pin(dapm, "Headphone");
	snd_soc_dapm_enable_pin(dapm, "Ext Spk");
	snd_soc_dapm_enable_pin(dapm, "Int Mic");

	snd_soc_dapm_sync(dapm);

	/*
	 * ASoC still uses legacy GPIOs so we look both GPIOs using
	 * descriptors here, convert them to numbers and release the
	 * acquired descriptors. Once ASoC switches over to GPIO descriptor
	 * APIs we can pass them directly.
	 */
	hp_desc = gpiod_get_index(card->dev->parent, NULL, 0);
	if (IS_ERR(hp_desc))
		return 0;
	mic_desc = gpiod_get_index(card->dev->parent, NULL, 1);
	if (IS_ERR(mic_desc)) {
		gpiod_put(hp_desc);
		return 0;
	}

	hs_jack_gpios[0].gpio = desc_to_gpio(hp_desc);
	hs_jack_gpios[1].gpio = desc_to_gpio(mic_desc);

	gpiod_put(mic_desc);
	gpiod_put(hp_desc);

	/* Enable jack detection */
	ret = snd_soc_jack_new(codec, "Headphone", SND_JACK_HEADPHONE, jack);
	if (ret)
		return ret;

	ret = snd_soc_jack_add_pins(jack, ARRAY_SIZE(hs_jack_pins),
				    hs_jack_pins);
	if (ret)
		return ret;

	/* enabled codec jack and PLL interrupts */
	snd_soc_update_bits(codec, M98090_REG_INTERRUPT_S,
		M98090_IJDET_MASK | M98090_IULK_MASK,
		1 << M98090_IJDET_SHIFT | 1 << M98090_IULK_SHIFT);

	snd_soc_jack_report(jack, SND_JACK_LINEOUT | SND_JACK_LINEIN,
			    SND_JACK_HEADSET | SND_JACK_LINEOUT |
			    SND_JACK_LINEIN);

	ret = snd_soc_jack_add_gpios(jack, ARRAY_SIZE(hs_jack_gpios),
				     hs_jack_gpios);

	return ret;
}

static struct snd_soc_dai_link byt_max98090_dais[] = {
	{
		.name = "Baytrail Audio",
		.stream_name = "Audio",
		.cpu_dai_name = "baytrail-pcm-audio",
		.codec_dai_name = "HiFi",
		.codec_name = "i2c-193C9890:00:1c",
		.platform_name = "baytrail-pcm-audio",
		.init = byt_max98090_init,
		.dai_fmt = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBS_CFS,
	},
};

static struct snd_soc_card byt_max98090_card = {
	.name = "byt-max98090",
	.dai_link = byt_max98090_dais,
	.num_links = ARRAY_SIZE(byt_max98090_dais),
	.dapm_widgets = byt_max98090_widgets,
	.num_dapm_widgets = ARRAY_SIZE(byt_max98090_widgets),
	.dapm_routes = byt_max98090_audio_map,
	.num_dapm_routes = ARRAY_SIZE(byt_max98090_audio_map),
	.controls = byt_max98090_controls,
	.num_controls = ARRAY_SIZE(byt_max98090_controls),
};

#ifdef CONFIG_PM_SLEEP
static const struct dev_pm_ops byt_max98090_pm_ops = {
	.suspend = snd_soc_suspend,
	.resume = snd_soc_resume,
};

#define BYT_MAX98090_PM_OPS	(&byt_max98090_pm_ops)
#else
#define BYT_MAX98090_PM_OPS	NULL
#endif

static int byt_max98090_probe(struct platform_device *pdev)
{
	int ret_val = 0;
	struct byt_max98090_private *drv;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_ATOMIC);
	if (!drv) {
		dev_err(&pdev->dev, "allocation failed\n");
		return -ENOMEM;
	}

	/* register the soc card */
	byt_max98090_card.dev = &pdev->dev;
	snd_soc_card_set_drvdata(&byt_max98090_card, drv);
	ret_val = snd_soc_register_card(&byt_max98090_card);
	if (ret_val) {
		dev_err(&pdev->dev,
			"snd_soc_register_card failed %d\n", ret_val);
		return ret_val;
	}
	platform_set_drvdata(pdev, &byt_max98090_card);

	return ret_val;
}

static int byt_max98090_remove(struct platform_device *pdev)
{
	struct snd_soc_card *soc_card = platform_get_drvdata(pdev);
	struct byt_max98090_private *drv = snd_soc_card_get_drvdata(soc_card);

	snd_soc_jack_free_gpios(&drv->jack, ARRAY_SIZE(hs_jack_gpios),
				hs_jack_gpios);

	snd_soc_card_set_drvdata(soc_card, NULL);
	snd_soc_unregister_card(soc_card);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static struct platform_driver byt_max98090_driver = {
	.probe = byt_max98090_probe,
	.remove = byt_max98090_remove,
	.driver = {
		.name = "byt-max98090",
		.owner = THIS_MODULE,
		.pm = BYT_MAX98090_PM_OPS,
	},
};
module_platform_driver(byt_max98090_driver)

MODULE_DESCRIPTION("ASoC Intel(R) Baytrail Machine driver");
MODULE_AUTHOR("Omair Md Abdullah, Jarkko Nikula");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:byt-max98090");
