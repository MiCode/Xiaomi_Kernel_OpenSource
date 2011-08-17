/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

/* A dummy driver useful only to advertise hardware parameters */
static struct snd_soc_dai_driver msm_stub_dais[] = {
	{
		.name = "msm-stub-rx",
		.playback = { /* Support maximum range */
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	},
	{
		.name = "msm-stub-tx",
		.capture = { /* Support maximum range */
			.stream_name = "Record",
			.channels_min = 1,
			.channels_max = 4,
			.rates = SNDRV_PCM_RATE_8000_48000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
	}
};

static struct snd_soc_codec_driver soc_msm_stub = {};

static int __devinit msm_stub_dev_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev,
	&soc_msm_stub, msm_stub_dais, ARRAY_SIZE(msm_stub_dais));
}

static int __devexit msm_stub_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver msm_stub_driver = {
	.driver = {
		.name = "msm-stub-codec",
		.owner = THIS_MODULE,
	},
	.probe = msm_stub_dev_probe,
	.remove = __devexit_p(msm_stub_dev_remove),
};

static int __init msm_stub_init(void)
{
	return platform_driver_register(&msm_stub_driver);
}
module_init(msm_stub_init);

static void __exit msm_stub_exit(void)
{
	platform_driver_unregister(&msm_stub_driver);
}
module_exit(msm_stub_exit);

MODULE_DESCRIPTION("Generic MSM CODEC driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
