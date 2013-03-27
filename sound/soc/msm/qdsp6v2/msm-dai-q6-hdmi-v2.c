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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio-v2.h>
#include <sound/q6afe-v2.h>
#include <sound/msm-dai-q6-v2.h>
#include <sound/pcm_params.h>

#define HDMI_RX_CA_MAX 0x32

enum {
	STATUS_PORT_STARTED, /* track if AFE port has started */
	STATUS_MAX
};

struct msm_hdmi_ca {
	bool set_ca;
	u32 ca;
};

static struct msm_hdmi_ca hdmi_ca = { false, 0x0 };

struct msm_dai_q6_hdmi_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	u32 rate;
	u32 channels;
	union afe_port_config port_config;
};

static int msm_dai_q6_hdmi_format_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	struct msm_dai_q6_hdmi_dai_data *dai_data = kcontrol->private_data;
	int value = ucontrol->value.integer.value[0];
	dai_data->port_config.hdmi_multi_ch.datatype = value;
	pr_debug("%s: value = %d\n", __func__, value);
	return 0;
}

static int msm_dai_q6_hdmi_format_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{

	struct msm_dai_q6_hdmi_dai_data *dai_data = kcontrol->private_data;
	ucontrol->value.integer.value[0] =
		dai_data->port_config.hdmi_multi_ch.datatype;
	return 0;
}

static int msm_dai_q6_hdmi_ca_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	hdmi_ca.ca = ucontrol->value.integer.value[0];
	hdmi_ca.set_ca = true;
	return 0;
}

static int msm_dai_q6_hdmi_ca_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = hdmi_ca.ca;
	return 0;
}

/* HDMI format field for AFE_PORT_MULTI_CHAN_HDMI_AUDIO_IF_CONFIG command
 *  0: linear PCM
 *  1: non-linear PCM
 */
static const char * const hdmi_format[] = {
	"LPCM",
	"Compr"
};

static const struct soc_enum hdmi_config_enum[] = {
	SOC_ENUM_SINGLE_EXT(2, hdmi_format),
};

static const struct snd_kcontrol_new hdmi_config_controls[] = {
	SOC_ENUM_EXT("HDMI RX Format", hdmi_config_enum[0],
				 msm_dai_q6_hdmi_format_get,
				 msm_dai_q6_hdmi_format_put),
	SOC_SINGLE_MULTI_EXT("HDMI RX CA", SND_SOC_NOPM, 0,
				 HDMI_RX_CA_MAX, 0, 1,
				 msm_dai_q6_hdmi_ca_get,
				 msm_dai_q6_hdmi_ca_put),
};

/* Current implementation assumes hw_param is called once
 * This may not be the case but what to do when ADM and AFE
 * port are already opened and parameter changes
 */
static int msm_dai_q6_hdmi_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);
	dai_data->port_config.hdmi_multi_ch.reserved = 0;
	dai_data->port_config.hdmi_multi_ch.hdmi_cfg_minor_version = 1;
	dai_data->port_config.hdmi_multi_ch.sample_rate = dai_data->rate;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		dai_data->port_config.hdmi_multi_ch.bit_width = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dai_data->port_config.hdmi_multi_ch.bit_width = 24;
		break;
	}

	switch (dai_data->channels) {
	case 2:
		dai_data->port_config.hdmi_multi_ch.channel_allocation = 0;
		break;
	case 6:
		dai_data->port_config.hdmi_multi_ch.channel_allocation = 0x0B;
		break;
	case 8:
		dai_data->port_config.hdmi_multi_ch.channel_allocation = 0x13;
		break;
	default:
		dev_err(dai->dev, "invalid Channels = %u\n",
				dai_data->channels);
		return -EINVAL;
	}
	dev_dbg(dai->dev, "%s() minor version: %u samplerate: %u bitwidth: %u\n"
		"num_ch = %u channel_allocation = %u datatype = %d\n", __func__,
		dai_data->port_config.hdmi_multi_ch.hdmi_cfg_minor_version,
		dai_data->port_config.hdmi_multi_ch.sample_rate,
		dai_data->port_config.hdmi_multi_ch.bit_width,
		dai_data->channels,
		dai_data->port_config.hdmi_multi_ch.channel_allocation,
		dai_data->port_config.hdmi_multi_ch.datatype);

	return 0;
}


static void msm_dai_q6_hdmi_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_info("%s:  afe port not started. dai_data->status_mask = %ld\n",
		 __func__, *dai_data->status_mask);
		return;
	}

	rc = afe_close(dai->id); /* can block */

	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close AFE port\n");

	pr_debug("%s: dai_data->status_mask = %ld\n", __func__,
			*dai_data->status_mask);

	clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
}


static int msm_dai_q6_hdmi_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (hdmi_ca.set_ca)
		dai_data->port_config.hdmi_multi_ch.channel_allocation =
								 hdmi_ca.ca;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_port_start(dai->id, &dai_data->port_config,
				    dai_data->rate);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE port %x\n",
				dai->id);
		else
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
	}

	return rc;
}

static int msm_dai_q6_hdmi_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data;
	const struct snd_kcontrol_new *kcontrol;
	int rc = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_hdmi_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	kcontrol = &hdmi_config_controls[0];

	rc = snd_ctl_add(dai->card->snd_card,
					 snd_ctl_new1(kcontrol, dai_data));

	kcontrol = &hdmi_config_controls[1];

	rc = snd_ctl_add(dai->card->snd_card,
					 snd_ctl_new1(kcontrol, dai_data));

	return rc;
}

static int msm_dai_q6_hdmi_dai_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(dai->id); /* can block */

		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");

		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	return 0;
}

static struct snd_soc_dai_ops msm_dai_q6_hdmi_ops = {
	.prepare	= msm_dai_q6_hdmi_prepare,
	.hw_params	= msm_dai_q6_hdmi_hw_params,
	.shutdown	= msm_dai_q6_hdmi_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_hdmi_hdmi_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
		.channels_min = 2,
		.channels_max = 8,
		.rate_max =     48000,
		.rate_min =	48000,
	},
	.ops = &msm_dai_q6_hdmi_ops,
	.probe = msm_dai_q6_hdmi_dai_probe,
	.remove = msm_dai_q6_hdmi_dai_remove,
};


/* To do: change to register DAIs as batch */
static __devinit int msm_dai_q6_hdmi_dev_probe(struct platform_device *pdev)
{
	int rc, id;
	const char *q6_dev_id = "qcom,msm-dai-q6-dev-id";

	rc = of_property_read_u32(pdev->dev.of_node, q6_dev_id, &id);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, q6_dev_id);
		return rc;
	}

	pdev->id = id;
	dev_set_name(&pdev->dev, "%s.%d", "msm-dai-q6-hdmi", id);

	pr_debug("%s: dev name %s, id:%d\n", __func__,
			dev_name(&pdev->dev), pdev->id);

	switch (pdev->id) {
	case HDMI_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_hdmi_hdmi_rx_dai);
		break;
	default:
		dev_err(&pdev->dev, "invalid device ID %d\n", pdev->id);
		rc = -ENODEV;
		break;
	}
	return rc;
}

static __devexit int msm_dai_q6_hdmi_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_dai_q6_hdmi_dt_match[] = {
	{.compatible = "qcom,msm-dai-q6-hdmi"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_dai_q6_hdmi_dt_match);

static struct platform_driver msm_dai_q6_hdmi_driver = {
	.probe  = msm_dai_q6_hdmi_dev_probe,
	.remove = msm_dai_q6_hdmi_dev_remove,
	.driver = {
		.name = "msm-dai-q6-hdmi",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_q6_hdmi_dt_match,
	},
};

static int __init msm_dai_q6_hdmi_init(void)
{
	return platform_driver_register(&msm_dai_q6_hdmi_driver);
}
module_init(msm_dai_q6_hdmi_init);

static void __exit msm_dai_q6_hdmi_exit(void)
{
	platform_driver_unregister(&msm_dai_q6_hdmi_driver);
}
module_exit(msm_dai_q6_hdmi_exit);

/* Module information */
MODULE_DESCRIPTION("MSM DSP HDMI DAI driver");
MODULE_LICENSE("GPL v2");
