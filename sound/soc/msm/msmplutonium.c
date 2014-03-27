/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/qpnp/clkdiv.h>
#include <linux/regulator/consumer.h>
#include <linux/io.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/jack.h>
#include <sound/q6afe-v2.h>
#include <sound/pcm_params.h>
#include "qdsp6v2/msm-pcm-routing-v2.h"
#include "qdsp6v2/q6core.h"

#define DRV_NAME "msmplutonium-asoc-snd"

struct msm_auxpcm_gpio {
	unsigned gpio_no;
	const char *gpio_name;
};


struct msm_auxpcm_ctrl {
	struct msm_auxpcm_gpio *pin_data;
	u32 cnt;
	void __iomem *mux;
};

struct msmplutonium_asoc_mach_data {
	struct msm_auxpcm_ctrl *pri_auxpcm_ctrl;
};


static int msm_auxpcm_be_params_fixup(struct snd_soc_pcm_runtime *rtd,
					struct snd_pcm_hw_params *params)
{
	return 0;
}


static int msm_prim_auxpcm_startup(struct snd_pcm_substream *substream)
{
	return 0;
}

static void msm_prim_auxpcm_shutdown(struct snd_pcm_substream *substream)
{
}

static struct snd_soc_ops msm_pri_auxpcm_be_ops = {
	.startup = msm_prim_auxpcm_startup,
	.shutdown = msm_prim_auxpcm_shutdown,
};

#define SAMPLING_RATE_48KHZ 48000
static int msm_slim_0_rx_ch = 1;
static int msm_slim_0_tx_ch = 1;
static int slim0_rx_sample_rate = SAMPLING_RATE_48KHZ;
static int slim0_rx_bit_format = SNDRV_PCM_FORMAT_S16_LE;

static inline int param_is_mask(int p)
{
	return ((p >= SNDRV_PCM_HW_PARAM_FIRST_MASK) &&
			(p <= SNDRV_PCM_HW_PARAM_LAST_MASK));
}

static inline struct snd_mask *param_to_mask(struct snd_pcm_hw_params *p, int n)
{
	return &(p->masks[n - SNDRV_PCM_HW_PARAM_FIRST_MASK]);
}

static void param_set_mask(struct snd_pcm_hw_params *p, int n, unsigned bit)
{
	if (bit >= SNDRV_MASK_MAX)
		return;
	if (param_is_mask(n)) {
		struct snd_mask *m = param_to_mask(p, n);
		m->bits[0] = 0;
		m->bits[1] = 0;
		m->bits[bit >> 5] |= (1 << (bit & 31));
	}
}


static int msm_slim_0_rx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels =
	    hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim0_rx_bit_format);
	rate->min = rate->max = slim0_rx_sample_rate;
	channels->min = channels->max = msm_slim_0_rx_ch;

	 pr_debug("%s: format = %d, rate = %d, channels = %d\n",
			  __func__, params_format(params), params_rate(params),
			  msm_slim_0_rx_ch);

	return 0;
}

static int msm_slim_0_tx_be_hw_params_fixup(struct snd_soc_pcm_runtime *rtd,
					    struct snd_pcm_hw_params *params)
{
	struct snd_interval *rate = hw_param_interval(params,
	SNDRV_PCM_HW_PARAM_RATE);

	struct snd_interval *channels = hw_param_interval(params,
			SNDRV_PCM_HW_PARAM_CHANNELS);

	pr_debug("%s()\n", __func__);
	param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
				   slim0_rx_bit_format);
	rate->min = rate->max = 48000;
	channels->min = channels->max = msm_slim_0_tx_ch;

	return 0;
}

static int msmplutonium_snd_startup(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s  stream = %d\n", __func__,
		 substream->name, substream->stream);
	return 0;
}

static int msm_snd_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int rx_ch[] = {144, 145, 146, 147, 148, 149, 150,
					    151, 152, 153, 154, 155, 156};
	unsigned int tx_ch[]  = {128, 129, 130, 131, 132, 133,
					     134, 135, 136, 137, 138, 139,
					     140, 141, 142, 143};
	int ret = -EINVAL;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
		 msm_slim_0_rx_ch, rx_ch);
		if (ret < 0)
			pr_err("%s: RX failed to set cpu chan map error %d\n",
			__func__, ret);
	} else {
		ret = snd_soc_dai_set_channel_map(cpu_dai, 0, 0,
		 msm_slim_0_tx_ch, tx_ch);
		if (ret < 0)
			pr_err("%s: TX failed to set cpu chan map error %d\n",
			__func__, ret);

	}
	return ret;
}

static void msmplutonium_snd_shudown(struct snd_pcm_substream *substream)
{
	pr_debug("%s(): substream = %s stream = %d\n", __func__,
		 substream->name, substream->stream);

}

static struct snd_soc_ops msmplutonium_be_ops = {
	.startup = msmplutonium_snd_startup,
	.hw_params = msm_snd_hw_params,
	.shutdown = msmplutonium_snd_shudown,
};


/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msmplutonium_common_dai_links[] = {
	/* FrontEnd DAI Links */
	{
		.name = "MSMplutonium Media1",
		.stream_name = "MultiMedia1",
		.cpu_dai_name	= "MultiMedia1",
		.platform_name  = "msm-pcm-dsp.0",
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
	/* Primary AUX PCM Backend DAI Links */
	{
		.name = LPASS_BE_AUXPCM_RX,
		.stream_name = "AUX PCM Playback",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_RX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_pmdown_time = 1,
		.ignore_suspend = 1,
		/* this dainlink has playback support */
	},
	{
		.name = LPASS_BE_AUXPCM_TX,
		.stream_name = "AUX PCM Capture",
		.cpu_dai_name = "msm-dai-q6-auxpcm.1",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name = "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_AUXPCM_TX,
		.be_hw_params_fixup = msm_auxpcm_be_params_fixup,
		.ops = &msm_pri_auxpcm_be_ops,
		.ignore_suspend = 1,
	},
	/* Backend DAI Links */
	{
		.name = LPASS_BE_SLIMBUS_0_RX,
		.stream_name = "Slimbus Playback",
		.cpu_dai_name = "msm-dai-q6-dev.16384",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-rx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_RX,
		.be_hw_params_fixup = msm_slim_0_rx_be_hw_params_fixup,
		.ignore_pmdown_time = 1, /* dai link has playback support */
		.ignore_suspend = 1,
		.ops = &msmplutonium_be_ops,
	},
	{
		.name = LPASS_BE_SLIMBUS_0_TX,
		.stream_name = "Slimbus Capture",
		.cpu_dai_name = "msm-dai-q6-dev.16385",
		.platform_name = "msm-pcm-routing",
		.codec_name = "msm-stub-codec.1",
		.codec_dai_name	= "msm-stub-tx",
		.no_pcm = 1,
		.be_id = MSM_BACKEND_DAI_SLIMBUS_0_TX,
		.be_hw_params_fixup = msm_slim_0_tx_be_hw_params_fixup,
		.ignore_suspend = 1,
		.ops = &msmplutonium_be_ops,
	},


};

struct snd_soc_card snd_soc_card_msmplutonium = {
	.name		= "msmplutonium-tomtom-snd-card",
};



static int msmplutonium_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_msmplutonium;
	struct msmplutonium_asoc_mach_data *pdata;
	int ret;
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msmplutonium_asoc_mach_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&pdev->dev, "Can't allocate msmplutonium_asoc_mach_data\n");
		return -ENOMEM;
	}

	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret)
		goto err;

	card->dai_link	= msmplutonium_common_dai_links;
	card->num_links	= ARRAY_SIZE(msmplutonium_common_dai_links);

	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}

	return 0;
err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msmplutonium_asoc_machine_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	snd_soc_unregister_card(card);
	return 0;
}

static const struct of_device_id msmplutonium_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,msmplutonium-asoc-snd", },
	{},
};

static struct platform_driver msmplutonium_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = msmplutonium_asoc_machine_of_match,
	},
	.probe = msmplutonium_asoc_machine_probe,
	.remove = msmplutonium_asoc_machine_remove,
};
module_platform_driver(msmplutonium_asoc_machine_driver);

MODULE_DESCRIPTION("ALSA SoC msm");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, msmplutonium_asoc_machine_of_match);
