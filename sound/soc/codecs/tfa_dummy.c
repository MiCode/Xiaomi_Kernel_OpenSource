/*
 * Driver for passing audio I2S for testing TFA98xx devices
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <linux/of.h>

#define DRV_NAME "tfa-dummy"

#define TFA_DUMMY_RATES (SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT)
#define TFA_DUMMY_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

static const struct snd_soc_dapm_widget tfa_dummy_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("I2S1"),
	SND_SOC_DAPM_MIXER("NXP Output Mixer", SND_SOC_NOPM, 0, 0, NULL, 0),
};

static const struct snd_soc_dapm_route tfa_dummy_dapm_routes[] = {
	{"NXP Output Mixer", NULL, "Playback"},
};

static struct snd_soc_codec_driver soc_codec_tfa_dummy;

static int tfa_dummy_set_dai_fmt(struct snd_soc_dai *codec_dai,
				unsigned int fmt)
{
	return 0;
}

static int tfa_dummy_set_dai_sysclk(struct snd_soc_dai *codec_dai,
				  int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static const struct snd_soc_dai_ops tfa_dummy_ops = {
	.set_fmt	= tfa_dummy_set_dai_fmt,
	.set_sysclk	= tfa_dummy_set_dai_sysclk,
};

static struct snd_soc_dai_driver tfa_dummy_dai = {
	.name		= "tfa_dummy_codec",
	.playback	= {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 16,
		.rates		= TFA_DUMMY_RATES,
		.formats	= TFA_DUMMY_FORMATS,
	},
	.ops = &tfa_dummy_ops,
	.symmetric_rates = 1,
};

static int tfa_dummy_platform_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_tfa_dummy,
					&tfa_dummy_dai, 1);
}

static int tfa_dummy_platform_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tfa_dummy_dt_ids[] = {
	{ .compatible = "nxp,tfa_dummy", },
	{ }
};
MODULE_DEVICE_TABLE(of, tfa_dummy_dt_ids);
#endif

static struct platform_driver tfa_dummy_driver = {
	.probe		= tfa_dummy_platform_probe,
	.remove		= tfa_dummy_platform_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(tfa_dummy_dt_ids),
	},
};

static int tfa_dummy_probe(struct snd_soc_codec *codec)
{

	snd_soc_dapm_new_controls(&codec->dapm, tfa_dummy_dapm_widgets,
				  ARRAY_SIZE(tfa_dummy_dapm_widgets));

	snd_soc_dapm_add_routes(&codec->dapm, tfa_dummy_dapm_routes,
				ARRAY_SIZE(tfa_dummy_dapm_routes));

	snd_soc_dapm_new_widgets(&codec->dapm);
	snd_soc_dapm_sync(&codec->dapm);

	dev_debug(codec->dev, "tfa_dummy codec registered");

	return 0;
}

static int tfa_dummy_remove(struct snd_soc_codec *codec)
{
	dev_info(codec->dev, "tfa_dummy codec removed");
	return 0;
}


static struct snd_soc_codec_driver soc_codec_tfa_dummy = {
	.probe = tfa_dummy_probe,
	.remove = tfa_dummy_remove,
};

module_platform_driver(tfa_dummy_driver);

MODULE_AUTHOR("NXP");
MODULE_DESCRIPTION("TFA dummy codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
