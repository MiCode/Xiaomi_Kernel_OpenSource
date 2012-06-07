/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/soc.h>
#include "msm_audio_mvs.h"

static struct snd_soc_dai_driver msm_mvs_codec_dais[] = {
{
	.name = "mvs-codec-dai",
	.playback = {
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000),
		.rate_min = 8000,
		.rate_max = 8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000),
		.rate_min = 8000,
		.rate_max = 8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
};
static struct snd_soc_dai_driver msm_mvs_cpu_dais[] = {
{
	.name = "mvs-cpu-dai",
	.playback = {
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000),
		.rate_min = 8000,
		.rate_max = 8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.channels_max = 2,
		.rates = (SNDRV_PCM_RATE_8000),
		.rate_min = 8000,
		.rate_max = 8000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
},
};

static struct snd_soc_codec_driver soc_codec_dev_msm = {
        .compress_type = SND_SOC_FLAT_COMPRESSION,
};

static __devinit int asoc_mvs_codec_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_msm,
                        msm_mvs_codec_dais, ARRAY_SIZE(msm_mvs_codec_dais));
}

static int __devexit asoc_mvs_codec_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static __devinit int asoc_mvs_cpu_probe(struct platform_device *pdev)
{
	dev_info(&pdev->dev, "%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_dai(&pdev->dev, msm_mvs_cpu_dais);
}

static int __devexit asoc_mvs_cpu_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver asoc_mvs_codec_driver = {
	.probe = asoc_mvs_codec_probe,
	.remove = __devexit_p(asoc_mvs_codec_remove),
	.driver = {
			.name = "mvs-codec-dai",
			.owner = THIS_MODULE,
	},
};

static struct platform_driver asoc_mvs_cpu_driver = {
	.probe = asoc_mvs_cpu_probe,
	.remove = __devexit_p(asoc_mvs_cpu_remove),
	.driver = {
			.name = "mvs-cpu-dai",
			.owner = THIS_MODULE,
	},
};

static int __init mvs_codec_dai_init(void)
{
	return platform_driver_register(&asoc_mvs_codec_driver);
}

static void __exit mvs_codec_dai_exit(void)
{
	platform_driver_unregister(&asoc_mvs_codec_driver);
}

static int __init mvs_cpu_dai_init(void)
{
	return platform_driver_register(&asoc_mvs_cpu_driver);
}

static void __exit mvs_cpu_dai_exit(void)
{
	platform_driver_unregister(&asoc_mvs_cpu_driver);
}

module_init(mvs_codec_dai_init);
module_exit(mvs_codec_dai_exit);
module_init(mvs_cpu_dai_init);
module_exit(mvs_cpu_dai_exit);

/* Module information */
MODULE_DESCRIPTION("MSM Codec/Cpu Dai driver");
MODULE_LICENSE("GPL v2");
