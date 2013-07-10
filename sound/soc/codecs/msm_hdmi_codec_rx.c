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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <mach/msm_hdmi_audio_codec.h>

#define MSM_HDMI_PCM_RATES	SNDRV_PCM_RATE_48000

struct msm_hdmi_audio_codec_rx_data {
	struct platform_device *hdmi_core_pdev;
	struct msm_hdmi_audio_codec_ops hdmi_ops;
};

static int msm_hdmi_edid_ctl_info(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct msm_hdmi_audio_codec_rx_data *codec_data;
	struct msm_hdmi_audio_edid_blk edid_blk;
	int rc;

	codec_data = snd_soc_codec_get_drvdata(codec);
	rc = codec_data->hdmi_ops.get_audio_edid_blk(codec_data->hdmi_core_pdev,
						     &edid_blk);
	if (!IS_ERR_VALUE(rc)) {
		uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
		uinfo->count = edid_blk.audio_data_blk_size +
			       edid_blk.spk_alloc_data_blk_size;
	}

	return 0;
}

static int msm_hdmi_edid_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol) {
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct msm_hdmi_audio_codec_rx_data *codec_data;
	struct msm_hdmi_audio_edid_blk edid_blk;
	int rc;

	codec_data = snd_soc_codec_get_drvdata(codec);
	rc = codec_data->hdmi_ops.get_audio_edid_blk(codec_data->hdmi_core_pdev,
						     &edid_blk);

	if (!IS_ERR_VALUE(rc)) {
		memcpy(ucontrol->value.bytes.data, edid_blk.audio_data_blk,
		       edid_blk.audio_data_blk_size);
		memcpy((ucontrol->value.bytes.data +
		       edid_blk.audio_data_blk_size),
		       edid_blk.spk_alloc_data_blk,
		       edid_blk.spk_alloc_data_blk_size);
	}

	return rc;
}

static const struct snd_kcontrol_new msm_hdmi_codec_rx_controls[] = {
	{
		.access = SNDRV_CTL_ELEM_ACCESS_READ |
			  SNDRV_CTL_ELEM_ACCESS_VOLATILE,
		.iface	= SNDRV_CTL_ELEM_IFACE_PCM,
		.name	= "HDMI EDID",
		.info	= msm_hdmi_edid_ctl_info,
		.get	= msm_hdmi_edid_get,
	},
};

static int msm_hdmi_audio_codec_rx_dai_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	u32 channel_allocation = 0;
	u32 level_shift  = 0; /* 0dB */
	bool down_mix = 0;
	u32 num_channels = params_channels(params);

	struct msm_hdmi_audio_codec_rx_data *codec_data =
			dev_get_drvdata(dai->codec->dev);

	switch (num_channels) {
	case 2:
		channel_allocation  = 0;
		break;
	case 6:
		channel_allocation  = 0x0B;
		break;
	case 8:
		channel_allocation  = 0x13;
		break;
	default:
		dev_err(dai->dev, "invalid Channels = %u\n", num_channels);
		return -EINVAL;
	}

	dev_dbg(dai->dev, "%s() num_ch %u  samplerate %u channel_allocation = %u\n",
		__func__, num_channels, params_rate(params),
		channel_allocation);

	codec_data->hdmi_ops.audio_info_setup(codec_data->hdmi_core_pdev,
			params_rate(params), num_channels, channel_allocation,
			level_shift, down_mix);

	return 0;
}

static struct snd_soc_dai_ops msm_hdmi_audio_codec_rx_dai_ops = {
	.hw_params	= msm_hdmi_audio_codec_rx_dai_hw_params,
};

static int msm_hdmi_audio_codec_rx_probe(struct snd_soc_codec *codec)
{
	struct msm_hdmi_audio_codec_rx_data *codec_data;
	struct device_node *of_node_parent = NULL;

	codec_data = kzalloc(sizeof(struct msm_hdmi_audio_codec_rx_data),
		GFP_KERNEL);

	if (!codec_data) {
		dev_err(codec->dev, "%s(): fail to allocate dai data\n",
				__func__);
		return -ENOMEM;
	}

	of_node_parent = of_get_parent(codec->dev->of_node);
	if (!of_node_parent) {
		dev_err(codec->dev, "%s(): Parent device tree node not found\n",
				__func__);
		kfree(codec_data);
		return -ENODEV;
	}

	codec_data->hdmi_core_pdev = of_find_device_by_node(of_node_parent);
	if (!codec_data->hdmi_core_pdev) {
		dev_err(codec->dev, "%s(): can't get parent pdev\n", __func__);
		kfree(codec_data);
		return -ENODEV;
	}

	if (msm_hdmi_register_audio_codec(codec_data->hdmi_core_pdev,
				&codec_data->hdmi_ops)) {
		dev_err(codec->dev, "%s(): can't register with hdmi core",
				__func__);
		kfree(codec_data);
		return -ENODEV;
	}

	dev_set_drvdata(codec->dev, codec_data);

	dev_dbg(codec->dev, "%s(): registerd %s with HDMI core\n",
		__func__, codec->name);

	return 0;
}

static int msm_hdmi_audio_codec_rx_remove(struct snd_soc_codec *codec)
{
	struct msm_hdmi_audio_codec_rx_data *codec_data;

	codec_data = dev_get_drvdata(codec->dev);
	kfree(codec_data);

	return 0;
}

static struct snd_soc_dai_driver msm_hdmi_audio_codec_rx_dais[] = {
	{
		.name = "msm_hdmi_audio_codec_rx_dai",
		.playback = {
			.stream_name = "HDMI Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rate_min = 48000,
			.rate_max = 48000,
			.rates = MSM_HDMI_PCM_RATES,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
		},
		.ops = &msm_hdmi_audio_codec_rx_dai_ops,
	},
};

static struct snd_soc_codec_driver msm_hdmi_audio_codec_rx_soc_driver = {
	.probe = msm_hdmi_audio_codec_rx_probe,
	.remove =  msm_hdmi_audio_codec_rx_remove,
	.controls = msm_hdmi_codec_rx_controls,
	.num_controls = ARRAY_SIZE(msm_hdmi_codec_rx_controls),
};

static int __devinit msm_hdmi_audio_codec_rx_plat_probe(
		struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "%s(): orginal dev name  = %s, id = %d\n",
		__func__, dev_name(&pdev->dev), pdev->id);

	if (pdev->dev.of_node) {
		dev_dbg(&pdev->dev, "%s(): node full name = %s,  name = %s\n",
			__func__, pdev->dev.of_node->full_name,
			pdev->dev.of_node->name);
		dev_set_name(&pdev->dev, "%s", "msm-hdmi-audio-codec-rx");
	} else
		dev_err(&pdev->dev, "%s(): platfrom data not from device tree\n",
				__func__);

	dev_dbg(&pdev->dev, "%s(): new dev name %s\n", __func__,
		dev_name(&pdev->dev));

	return snd_soc_register_codec(&pdev->dev,
		&msm_hdmi_audio_codec_rx_soc_driver,
		msm_hdmi_audio_codec_rx_dais,
		ARRAY_SIZE(msm_hdmi_audio_codec_rx_dais));
}

static int __devexit msm_hdmi_audio_codec_rx_plat_remove(
		struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}
static const struct of_device_id msm_hdmi_audio_codec_rx_dt_match[] = {
	{ .compatible = "qcom,msm-hdmi-audio-codec-rx", },
	{}
};
MODULE_DEVICE_TABLE(of, msm_hdmi_codec_dt_match);

static struct platform_driver msm_hdmi_audio_codec_rx_driver = {
	.driver = {
		.name = "msm-hdmi-audio-codec-rx",
		.owner = THIS_MODULE,
		.of_match_table = msm_hdmi_audio_codec_rx_dt_match,
	},
	.probe = msm_hdmi_audio_codec_rx_plat_probe,
	.remove = __devexit_p(msm_hdmi_audio_codec_rx_plat_remove),
};

static int __init msm_hdmi_audio_codec_rx_init(void)
{
	return platform_driver_register(&msm_hdmi_audio_codec_rx_driver);
}
module_init(msm_hdmi_audio_codec_rx_init);

static void __exit msm_hdmi_audio_codec_rx_exit(void)
{
	platform_driver_unregister(&msm_hdmi_audio_codec_rx_driver);
}
module_exit(msm_hdmi_audio_codec_rx_exit);

MODULE_DESCRIPTION("MSM HDMI CODEC driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
