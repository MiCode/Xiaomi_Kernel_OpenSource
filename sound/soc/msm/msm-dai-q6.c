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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mfd/wcd9310/core.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/apr_audio.h>
#include <sound/q6afe.h>
#include <sound/q6adm.h>

enum {
	STATUS_PORT_STARTED, /* track if AFE port has started */
	STATUS_MAX
};

struct msm_dai_q6_dai_data {
	DECLARE_BITMAP(status_mask, STATUS_MAX);
	u32 rate;
	u32 channels;
	union afe_port_config port_config;
};

static int msm_dai_q6_cdc_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 2:
		dai_data->port_config.mi2s.channel = MSM_AFE_STEREO;
		break;
	case 1:
		dai_data->port_config.mi2s.channel = MSM_AFE_MONO;
		break;
	default:
		return -EINVAL;
		break;
	}
	dai_data->rate = params_rate(params);

	dev_dbg(dai->dev, " channel %d sample rate %d entered\n",
	dai_data->channels, dai_data->rate);

	/* Q6 only supports 16 as now */
	dai_data->port_config.mi2s.bitwidth = 16;
	dai_data->port_config.mi2s.line = 1;
	dai_data->port_config.mi2s.ws = 1; /* I2S master mode for now */

	return 0;
}

static int msm_dai_q6_hdmi_hw_params(struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dev_dbg(dai->dev, "%s start HDMI port\n", __func__);

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 2:
		dai_data->port_config.hdmi.channel_mode = 0; /* Put in macro */
		break;
	default:
		return -EINVAL;
		break;
	}

	/* Q6 only supports 16 as now */
	dai_data->port_config.hdmi.bitwidth = 16;
	dai_data->port_config.hdmi.data_type = 0;
	dai_data->rate = params_rate(params);

	return 0;
}

static int msm_dai_q6_slim_bus_hw_params(struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	u8 pgd_la, inf_la;

	memset(dai_data->port_config.slimbus.slave_port_mapping, 0,
		sizeof(dai_data->port_config.slimbus.slave_port_mapping));

	dai_data->channels = params_channels(params);
	switch (dai_data->channels) {
	case 2:
		if (dai->id == SLIMBUS_0_RX) {
			dai_data->port_config.slimbus.slave_port_mapping[0] = 1;
			dai_data->port_config.slimbus.slave_port_mapping[1] = 2;
		} else {
			dai_data->port_config.slimbus.slave_port_mapping[0] = 7;
			dai_data->port_config.slimbus.slave_port_mapping[1] = 8;
		}
		break;
	case 1:
		if (dai->id == SLIMBUS_0_RX)
			dai_data->port_config.slimbus.slave_port_mapping[0] = 1;
		else
			dai_data->port_config.slimbus.slave_port_mapping[0] = 7;
		break;
	default:
		return -EINVAL;
		break;
	}
	dai_data->rate = params_rate(params);
	tabla_get_logical_addresses(&pgd_la, &inf_la);

	dai_data->port_config.slimbus.slimbus_dev_id =  AFE_SLIMBUS_DEVICE_1;
	dai_data->port_config.slimbus.slave_dev_pgd_la = pgd_la;
	dai_data->port_config.slimbus.slave_dev_intfdev_la = inf_la;
	/* Q6 only supports 16 as now */
	dai_data->port_config.slimbus.bit_width = 16;
	dai_data->port_config.slimbus.data_format = 0;
	dai_data->port_config.slimbus.num_channels = dai_data->channels;
	dai_data->port_config.slimbus.reserved = 0;

	dev_dbg(dai->dev, "slimbus_dev_id  %hu  slave_dev_pgd_la 0x%hx\n"
		"slave_dev_intfdev_la 0x%hx   bit_width %hu   data_format %hu\n"
		"num_channel %hu  slave_port_mapping[0]  %hu\n"
		"slave_port_mapping[1]  %hu slave_port_mapping[2]  %hu\n"
		"sample_rate %d\n",
		dai_data->port_config.slimbus.slimbus_dev_id,
		dai_data->port_config.slimbus.slave_dev_pgd_la,
		dai_data->port_config.slimbus.slave_dev_intfdev_la,
		dai_data->port_config.slimbus.bit_width,
		dai_data->port_config.slimbus.data_format,
		dai_data->port_config.slimbus.num_channels,
		dai_data->port_config.slimbus.slave_port_mapping[0],
		dai_data->port_config.slimbus.slave_port_mapping[1],
		dai_data->port_config.slimbus.slave_port_mapping[2],
		dai_data->rate);

	return 0;
}

static int msm_dai_q6_bt_fm_hw_params(struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai, int stream)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);

	dai_data->channels = params_channels(params);
	dai_data->rate = params_rate(params);

	dev_dbg(dai->dev, "channels %d sample rate %d entered\n",
		dai_data->channels, dai_data->rate);

	memset(&dai_data->port_config, 0, sizeof(dai_data->port_config));

	return 0;
}

/* Current implementation assumes hw_param is called once
 * This may not be the case but what to do when ADM and AFE
 * port are already opened and parameter changes
 */
static int msm_dai_q6_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *dai)
{
	int rc = 0;

	switch (dai->id) {
	case PRIMARY_I2S_TX:
	case PRIMARY_I2S_RX:
		rc = msm_dai_q6_cdc_hw_params(params, dai, substream->stream);
		break;
	case HDMI_RX:
		rc = msm_dai_q6_hdmi_hw_params(params, dai);
		break;

	case SLIMBUS_0_RX:
	case SLIMBUS_0_TX:
		rc = msm_dai_q6_slim_bus_hw_params(params, dai,
				substream->stream);
		break;
	case INT_BT_SCO_RX:
	case INT_BT_SCO_TX:
	case INT_FM_RX:
	case INT_FM_TX:
		rc = msm_dai_q6_bt_fm_hw_params(params, dai, substream->stream);
		break;
	default:
		dev_err(dai->dev, "invalid AFE port ID\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static void msm_dai_q6_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc;

	rc = adm_close(dai->id);

	if (IS_ERR_VALUE(rc))
		dev_err(dai->dev, "fail to close ADM COPP\n");

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(dai->id); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	}
};

static int msm_dai_q6_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		/* if AFE port is already started, this means
		 * application wishes to restore hardware to
		 * fresh state. This logic anticipates prepare is not
		 * called right after TRIGGER_START before Q6 AFE
		 * has enough time to respond to port start command.
		 */
		rc = afe_close(dai->id); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
		clear_bit(STATUS_PORT_STARTED, dai_data->status_mask);
	} else {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			rc = adm_open_mixer(dai->id, 1, dai_data->rate,
				dai_data->channels, DEFAULT_COPP_TOPOLOGY);
		else
			rc = adm_open_mixer(dai->id, 2, dai_data->rate,
				dai_data->channels, DEFAULT_COPP_TOPOLOGY);
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to open ADM\n");
	}

	return rc;

}

static int msm_dai_q6_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data = dev_get_drvdata(dai->dev);
	int rc = 0;

	/* Start/stop port without waiting for Q6 AFE response. Need to have
	 * native q6 AFE driver propagates AFE response in order to handle
	 * port start/stop command error properly if error does arise.
	 */
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (!test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
			afe_port_start_nowait(dai->id, &dai_data->port_config,
				dai_data->rate);
			set_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
		}
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
			afe_port_stop_nowait(dai->id);
			clear_bit(STATUS_PORT_STARTED,
				dai_data->status_mask);
		}
		break;

	default:
		rc = -EINVAL;
	}

	return rc;
}

static int msm_dai_q6_dai_probe(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc = 0;

	dai_data = kzalloc(sizeof(struct msm_dai_q6_dai_data),
		GFP_KERNEL);

	if (!dai_data) {
		dev_err(dai->dev, "DAI-%d: fail to allocate dai data\n",
		dai->id);
		rc = -ENOMEM;
	} else
		dev_set_drvdata(dai->dev, dai_data);

	return rc;
}

static int msm_dai_q6_dai_remove(struct snd_soc_dai *dai)
{
	struct msm_dai_q6_dai_data *dai_data;
	int rc;

	dai_data = dev_get_drvdata(dai->dev);

	/* If AFE port is still up, close it */
	if (test_bit(STATUS_PORT_STARTED, dai_data->status_mask)) {
		rc = afe_close(dai->id); /* can block */
		if (IS_ERR_VALUE(rc))
			dev_err(dai->dev, "fail to close AFE port\n");
	}
	kfree(dai_data);
	snd_soc_unregister_dai(dai->dev);

	return 0;
}

static struct snd_soc_dai_ops msm_dai_q6_ops = {
	/*
	 * DSP only handles 16-bit and support only I2S
	 * master mode for now. leave set_fmt function
	 * unimplemented for now.
	 */
	.prepare	= msm_dai_q6_prepare,
	.trigger	= msm_dai_q6_trigger,
	.hw_params	= msm_dai_q6_hw_params,
	.shutdown	= msm_dai_q6_shutdown,
};

static struct snd_soc_dai_driver msm_dai_q6_i2s_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_i2s_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_hdmi_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_max =     48000,
		.rate_min =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_slimbus_tx_dai = {
	.capture = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min =     8000,
		.rate_max =	48000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_bt_sco_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_bt_sco_tx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 1,
		.rate_max = 16000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_fm_rx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_max = 48000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_q6_fm_tx_dai = {
	.playback = {
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
		SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 2,
		.channels_max = 2,
		.rate_max = 48000,
		.rate_min = 8000,
	},
	.ops = &msm_dai_q6_ops,
	.probe = msm_dai_q6_dai_probe,
	.remove = msm_dai_q6_dai_remove,
};

/* To do: change to register DAIs as batch */
static __devinit int msm_dai_q6_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	switch (pdev->id) {
	case PRIMARY_I2S_RX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_i2s_rx_dai);
		break;
	case PRIMARY_I2S_TX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_i2s_tx_dai);
		break;
	case HDMI_RX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_hdmi_rx_dai);
		break;
	case SLIMBUS_0_RX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_rx_dai);
		break;
	case SLIMBUS_0_TX:
		rc = snd_soc_register_dai(&pdev->dev,
				&msm_dai_q6_slimbus_tx_dai);
	case INT_BT_SCO_RX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_bt_sco_rx_dai);
		break;
	case INT_BT_SCO_TX:
		rc = snd_soc_register_dai(&pdev->dev,
					&msm_dai_q6_bt_sco_tx_dai);
		break;
	case INT_FM_RX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_fm_rx_dai);
		break;
	case INT_FM_TX:
		rc = snd_soc_register_dai(&pdev->dev, &msm_dai_q6_fm_tx_dai);
		break;
	default:
		rc = -ENODEV;
		break;
	}
	return rc;
}

static __devexit int msm_dai_q6_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver msm_dai_q6_driver = {
	.probe  = msm_dai_q6_dev_probe,
	.remove = msm_dai_q6_dev_remove,
	.driver = {
		.name = "msm-dai-q6",
		.owner = THIS_MODULE,
	},
};

static int __init msm_dai_q6_init(void)
{
	return platform_driver_register(&msm_dai_q6_driver);
}
module_init(msm_dai_q6_init);

static void __exit msm_dai_q6_exit(void)
{
	platform_driver_unregister(&msm_dai_q6_driver);
}
module_exit(msm_dai_q6_exit);

/* Module information */
MODULE_DESCRIPTION("MSM DSP DAI driver");
MODULE_LICENSE("GPL v2");
