 /* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/qpnp/clkdiv.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <asm/mach-types.h>
#include <mach/socinfo.h>
#include <qdsp6v2/msm-pcm-routing-v2.h>
#include <linux/module.h>
#define DRV_NAME "msm8x10-asoc-wcd"

static int msm_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
				struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
					SNDRV_PCM_HW_PARAM_RATE);

	pr_debug("%s()\n", __func__);
	rate->min = rate->max = 48000;

	return 0;
}

static int msm_audrx_init(struct snd_soc_pcm_runtime *rtd)
{
	pr_info("%s(), dev_name%s\n", __func__, dev_name(rtd->cpu_dai->dev));
	return 0;
}

static int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

static int msm_snd_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}


static void msm_snd_shutdown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s stream = %d\n", __func__,
		 substream->name, substream->stream);
}

static struct snd_soc_ops msm8x10_be_ops = {
	.startup = msm_snd_startup,
	.hw_params = msm_snd_hw_params,
	.shutdown = msm_snd_shutdown,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8x10_dai[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSM8X10 Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA1
	},
	{
		.name = "MSM8X10 Media2",
		.stream_name = "MultiMedia2",
		.cpu_dai_name   = "MultiMedia2",
		.platform_name  = "msm-pcm-dsp",
		.dynamic = 1,
		.codec_dai_name = "snd-soc-dummy-dai",
		.codec_name = "snd-soc-dummy",
		.trigger = {SND_SOC_DPCM_TRIGGER_POST,
			SND_SOC_DPCM_TRIGGER_POST},
		.ignore_suspend = 1,
		/* this dainlink has playback support */
		.ignore_pmdown_time = 1,
		.be_id = MSM_FRONTEND_DAI_MULTIMEDIA2,
	},
	/* Backend I2S DAI Links */
	{
		.name = LPASS_BE_MI2S_RX,
		.stream_name = "Primary MI2S Playback",
		.cpu_dai_name = "msm-dai-q6-mi2s.0",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm8x10-wcd-i2c-core.1-000d",
		.codec_dai_name = "msm8x10_wcd_i2s_rx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_MI2S_RX,
		.init = &msm_audrx_init,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x10_be_ops,
	},
	{
		.name = LPASS_BE_SEC_MI2S_TX,
		.stream_name = "Secondary MI2S Capture",
		.cpu_dai_name = "msm-dai-q6-mi2s.1",
		.platform_name = "msm-pcm-routing",
		.codec_name     = "msm8x10-wcd-i2c-core.1-000d",
		.codec_dai_name = "msm8x10_wcd_i2s_tx1",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SECONDARY_MI2S_TX,
		.be_hw_params_fixup = msm_be_hw_params_fixup,
		.ops = &msm8x10_be_ops,
	},
};

struct snd_soc_card snd_soc_card_msm8x10 = {
	.name		= "msm8x10-snd-card",
	.dai_link	= msm8x10_dai,
	.num_links	= ARRAY_SIZE(msm8x10_dai),
};


static __devinit int msm8x10_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_msm8x10;
	int ret;

	dev_dbg(&pdev->dev, "%s\n", __func__);
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);


	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	return 0;
err:
	return ret;
}

static int __devexit msm8x10_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	return 0;
}

static const struct of_device_id msm8x10_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msm8x10-audio-codec", },
	{},
};

static struct platform_driver msm8x10_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msm8x10_asoc_machine_of_match,
	},
	.probe = msm8x10_asoc_machine_probe,
	.remove = __devexit_p(msm8x10_asoc_machine_remove),
};
module_platform_driver(msm8x10_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msm8x10_asoc_machine_of_match);
