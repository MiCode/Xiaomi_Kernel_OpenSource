/*
 * ALSA SoC SPDIF DIT driver
 *
 *  This driver is used by controllers which can operate in DIT (SPDI/F) where
 *  no codec is needed.  This file provides stub codec that can be used
 *  in these configurations. TI DaVinci Audio controller uses this driver.
 *
 * Author:      Steve Chen,  <schen@mvista.com>
 * Copyright:   (C) 2009 MontaVista Software, Inc., <source@mvista.com>
 * Copyright:   (C) 2009  Texas Instruments, India
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <asm/mach-types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>

#define DRV_NAME "spdif-dit"

#define STUB_RATES	SNDRV_PCM_RATE_8000_96000
#define STUB_FORMATS	SNDRV_PCM_FMTBIT_S16_LE

static int spdif_probe(struct snd_soc_codec *codec) {
	codec->dapm.idle_bias_off = 1;
	return 0;
}

static const struct snd_soc_dapm_widget spdif_dapm_widgets[] = {
	SND_SOC_DAPM_VMID("spdif dummy Vmid"),
};

static int spdif_write(struct snd_soc_codec *codec, unsigned int reg,
							unsigned int val){
	return 0;
}

static unsigned int spdif_read(struct snd_soc_codec *codec, unsigned int reg)
{
	return 0;
}

static struct snd_soc_codec_driver soc_codec_spdif_dit = {
	.probe = spdif_probe,
	.dapm_widgets = spdif_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(spdif_dapm_widgets),
	.read = spdif_read,
	.write = spdif_write,
};

static struct snd_soc_dai_driver dit_stub_dai = {
	.name		= "dit-hifi",
	.playback 	= {
		.stream_name	= "Playback",
		.channels_min	= 1,
		.channels_max	= 384,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
	.capture	= {
		.stream_name	= "Capture",
		.channels_min	= 1,
		.channels_max	= 384,
		.rates		= STUB_RATES,
		.formats	= STUB_FORMATS,
	},
};

static int spdif_dit_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev,
		&soc_codec_spdif_dit, &dit_stub_dai, 1);

}

static int spdif_dit_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver spdif_dit_driver = {
	.probe		= spdif_dit_probe,
	.remove		= spdif_dit_remove,
	.driver		= {
		.name	= DRV_NAME,
		.owner	= THIS_MODULE,
	},
};

module_platform_driver(spdif_dit_driver);

MODULE_AUTHOR("Steve Chen <schen@mvista.com>");
MODULE_DESCRIPTION("SPDIF dummy codec driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
