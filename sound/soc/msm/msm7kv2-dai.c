/* sound/soc/msm/msm-dai.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (C) 2008 HTC Corporation
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * Derived from msm-pcm.c and msm7201.c.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include <linux/slab.h>
#include "msm7kv2-pcm.h"

static struct snd_soc_dai_driver msm_pcm_codec_dais[] = {
{
	.name = "msm-codec-dai",
	.playback = {
		.channels_max = USE_CHANNELS_MAX,
		.rate_min = USE_RATE_MIN,
		.rate_max = USE_RATE_MAX,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_max = USE_CHANNELS_MAX,
		.rate_min = USE_RATE_MIN,
		.rate_max = USE_RATE_MAX,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
};
static struct snd_soc_dai_driver msm_pcm_cpu_dais[] = {
{
	.name = "msm-cpu-dai",
	.playback = {
		.channels_max = USE_CHANNELS_MAX,
		.rate_min = USE_RATE_MIN,
		.rate_max = USE_RATE_MAX,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_max = USE_CHANNELS_MAX,
		.rate_min = USE_RATE_MIN,
		.rate_max = USE_RATE_MAX,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
};

static struct snd_soc_codec_driver soc_codec_dev_msm = {
        .compress_type = SND_SOC_FLAT_COMPRESSION,
};

static __devinit int asoc_msm_codec_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_msm,
                        msm_pcm_codec_dais, ARRAY_SIZE(msm_pcm_codec_dais));
}

static int __devexit asoc_msm_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static __devinit int asoc_msm_cpu_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_dai(&pdev->dev, msm_pcm_cpu_dais);
}

static int __devexit asoc_msm_cpu_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver asoc_msm_codec_driver = {
	.probe = asoc_msm_codec_probe,
	.remove = __devexit_p(asoc_msm_codec_remove),
	.driver = {
			.name = "msm-codec-dai",
			.owner = THIS_MODULE,
	},
};

static struct platform_driver asoc_msm_cpu_driver = {
	.probe = asoc_msm_cpu_probe,
	.remove = __devexit_p(asoc_msm_cpu_remove),
	.driver = {
			.name = "msm-cpu-dai",
			.owner = THIS_MODULE,
	},
};

static int __init msm_codec_dai_init(void)
{
	return platform_driver_register(&asoc_msm_codec_driver);
}

static void __exit msm_codec_dai_exit(void)
{
	platform_driver_unregister(&asoc_msm_codec_driver);
}

static int __init msm_cpu_dai_init(void)
{
	return platform_driver_register(&asoc_msm_cpu_driver);
}

static void __exit msm_cpu_dai_exit(void)
{
	platform_driver_unregister(&asoc_msm_cpu_driver);
}

module_init(msm_codec_dai_init);
module_exit(msm_codec_dai_exit);
module_init(msm_cpu_dai_init);
module_exit(msm_cpu_dai_exit);

/* Module information */
MODULE_DESCRIPTION("MSM Codec/Cpu Dai driver");
MODULE_LICENSE("GPL v2");
