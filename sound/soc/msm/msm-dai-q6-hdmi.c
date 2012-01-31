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
#include <linux/mfd/wcd9310/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio.h>
#include <sound/q6afe.h>
#include <sound/q6adm.h>
#include <sound/msm-dai-q6.h>
#include <mach/clk.h>
#include <mach/msm_hdmi_audio.h>


enum {
	STATUS_PORT_STARTED, /* track if AFE port has started */
	STATUS_MAX
};

struct msm_dai_q6_hdmi_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	u32 rate;
	u32 channels;
	union afe_port_config port_config;
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
	u32 channel_allocation = 0;
	u32 level_shift  = 0; /* 0dB */
	bool down_mix = FALSE;

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);
	dai_data->port_config.hdmi_multi_ch.data_type = 0;
	dai_data->port_config.hdmi_multi_ch.reserved = 0;

	switch (dai_data->channels) {
	case 2:
		channel_allocation  = 0;
		hdmi_msm_audio_info_setup(1, MSM_HDMI_AUDIO_CHANNEL_2,
				channel_allocation, level_shift, down_mix);
		dai_data->port_config.hdmi_multi_ch.channel_allocation =
			channel_allocation;
		break;
	case 6:
		channel_allocation  = 0x0B;
		hdmi_msm_audio_info_setup(1, MSM_HDMI_AUDIO_CHANNEL_6,
				channel_allocation, level_shift, down_mix);
		dai_data->port_config.hdmi_multi_ch.channel_allocation =
				channel_allocation;
		break;
	default:
		dev_err(dai->dev, "invalid Channels = %u\n",
				dai_data->channels);
		return -EINVAL;
	}
	dev_dbg(dai->dev, "%s() num_ch = %u rate =%u"
		" channel_allocation = %u\n", __func__, dai_data->channels,
		dai_data->rate,
		dai_data->port_config.hdmi_multi_ch.channel_allocation);

	return 0;
}


static void msm_dai_q6_hdmi_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		pr_info("%s:  afe port not started. dai_data->status_mask"
			" = %ld\n", __func__, *dai_data->status_mask);
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

	if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		/* PORT START should be set if prepare called in active state */
		rc = afe_q6_interface_prepare();
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open AFE APR\n");
	}
	return rc;
}

static int msm_dai_q6_hdmi_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data = dev_get_drvdata(dai->dev);

	/* Start/stop port without waiting for Q6 AFE response. Need to have
	 * native q6 AFE driver propagates AFE response in order to handle
	 * port start/stop command error properly if error does arise.
	 */
	pr_debug("%s:port:%d  cmd:%d dai_data->status_mask = %ld",
		__func__, dai->id, cmd, *dai_data->status_mask);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
			afe_port_start_nowait(dai->id, &dai_data->port_config,
					dai_data->rate);

			set_bit(STATUS_PORT_STARTED, dai_data->status_mask);
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
			afe_port_stop_nowait(dai->id);
			clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
		}
		break;

	default:
		dev_err(dai->dev, "invalid Trigger command = %d\n", cmd);
		return -EINVAL;
	}

	return 0;
}

static int msm_dai_q6_hdmi_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_hdmi_dai_data *dai_data;
	int rc = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_hdmi_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

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
	.trigger	= msm_dai_q6_hdmi_trigger,
	.hw_params	= msm_dai_q6_hdmi_hw_params,
	.shutdown	= msm_dai_q6_hdmi_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_hdmi_hdmi_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 6,
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
	int rc = 0;

	dev_dbg(&pdev->dev, "dev name %s dev-id %d\n",
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

static struct platform_driver msm_dai_q6_hdmi_driver = {
	.probe  = msm_dai_q6_hdmi_dev_probe,
	.remove = msm_dai_q6_hdmi_dev_remove,
	.driver = {
		.name = "msm-dai-q6-hdmi",
		.owner = THIS_MODULE,
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
