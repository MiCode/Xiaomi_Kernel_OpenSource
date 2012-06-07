/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

static int msm_dai_stub_set_channel_map(struct snd_soc_dai *dai,
		unsigned int tx_num, unsigned int *tx_slot,
		unsigned int rx_num, unsigned int *rx_slot)
{
	pr_debug("%s:\n", __func__);

	return 0;
}

static struct snd_soc_dai_ops msm_dai_stub_ops = {
	.set_channel_map = msm_dai_stub_set_channel_map,
};

static struct snd_soc_dai_driver msm_dai_stub_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.ops = &msm_dai_stub_ops,
};

static __devinit int msm_dai_stub_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	rc = snd_soc_register_dai(&pdev->dev, &msm_dai_stub_dai);

	return rc;
}

static __devexit int msm_dai_stub_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s:\n", __func__);

	snd_soc_unregister_dai(&pdev->dev);

	return 0;
}

static struct platform_driver msm_dai_stub_driver = {
	.probe  = msm_dai_stub_dev_probe,
	.remove = msm_dai_stub_dev_remove,
	.driver = {
		.name = "msm-dai-stub",
		.owner = THIS_MODULE,
	},
};

static int __init msm_dai_stub_init(void)
{
	pr_debug("%s:\n", __func__);

	return platform_driver_register(&msm_dai_stub_driver);
}
module_init(msm_dai_stub_init);

static void __exit msm_dai_stub_exit(void)
{
	pr_debug("%s:\n", __func__);

	platform_driver_unregister(&msm_dai_stub_driver);
}
module_exit(msm_dai_stub_exit);

/* Module information */
MODULE_DESCRIPTION("MSM Stub DSP DAI driver");
MODULE_LICENSE("GPL v2");
