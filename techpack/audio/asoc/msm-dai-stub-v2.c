/* Copyright (c) 2013-2014, 2017 The Linux Foundation. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>

enum {
	STUB_RX,
	STUB_TX,
	STUB_1_RX,
	STUB_1_TX,
	STUB_DTMF_TX,
	STUB_HOST_RX_CAPTURE_TX,
	STUB_HOST_RX_PLAYBACK_RX,
	STUB_HOST_TX_CAPTURE_TX,
	STUB_HOST_TX_PLAYBACK_RX,
};

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

static int msm_dai_stub_add_route(struct snd_soc_dai *dai)
{
	struct snd_soc_dapm_route intercon;
	struct snd_soc_dapm_context *dapm;

	if (!dai || !dai->driver) {
		pr_err("%s Invalid params\n", __func__);
		return -EINVAL;
	}
	dapm = snd_soc_component_get_dapm(dai->component);
	memset(&intercon, 0, sizeof(intercon));
	if (dai->driver->playback.stream_name &&
		dai->driver->playback.aif_name) {
		dev_dbg(dai->dev, "%s add route for widget %s",
			__func__, dai->driver->playback.stream_name);
		intercon.source = dai->driver->playback.aif_name;
		intercon.sink = dai->driver->playback.stream_name;
		dev_dbg(dai->dev, "%s src %s sink %s\n",
			__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(dapm, &intercon, 1);
	}
	if (dai->driver->capture.stream_name &&
		dai->driver->capture.aif_name) {
		dev_dbg(dai->dev, "%s add route for widget %s",
			__func__, dai->driver->capture.stream_name);
		intercon.sink = dai->driver->capture.aif_name;
		intercon.source = dai->driver->capture.stream_name;
		dev_dbg(dai->dev, "%s src %s sink %s\n",
			__func__, intercon.source, intercon.sink);
		snd_soc_dapm_add_routes(dapm, &intercon, 1);
	}
	return 0;
}

static int msm_dai_stub_dai_probe(struct snd_soc_dai *dai)
{
	return msm_dai_stub_add_route(dai);
}

static int msm_dai_stub_dai_remove(struct snd_soc_dai *dai)
{
	pr_debug("%s:\n", __func__);
	return 0;
}

static struct snd_soc_dai_driver msm_dai_stub_dai_rx = {
	.playback = {
		.stream_name = "Stub Playback",
		.aif_name = "STUB_RX",
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.ops = &msm_dai_stub_ops,
	.probe = &msm_dai_stub_dai_probe,
	.remove = &msm_dai_stub_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_stub_dai_tx[] = {
	{
		.capture = {
			.stream_name = "Stub Capture",
			.aif_name = "STUB_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_dai_stub_ops,
		.probe = &msm_dai_stub_dai_probe,
		.remove = &msm_dai_stub_dai_remove,
	},
	{
		.capture = {
			.stream_name = "Stub1 Capture",
			.aif_name = "STUB_1_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_dai_stub_ops,
		.probe = &msm_dai_stub_dai_probe,
		.remove = &msm_dai_stub_dai_remove,
	}
};

static struct snd_soc_dai_driver msm_dai_stub_dtmf_tx_dai = {
	.capture = {
		.stream_name = "DTMF TX",
		.aif_name = "STUB_DTMF_TX",
		.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
			 SNDRV_PCM_RATE_16000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
		.channels_min = 1,
		.channels_max = 2,
		.rate_min = 8000,
		.rate_max = 48000,
	},
	.ops = &msm_dai_stub_ops,
	.probe = &msm_dai_stub_dai_probe,
	.remove = &msm_dai_stub_dai_remove,
};

static struct snd_soc_dai_driver msm_dai_stub_host_capture_tx_dai[] = {
	{
		.capture = {
			.stream_name = "CS-VOICE HOST RX CAPTURE",
			.aif_name = "STUB_HOST_RX_CAPTURE_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_dai_stub_ops,
		.probe = &msm_dai_stub_dai_probe,
		.remove = &msm_dai_stub_dai_remove,
	},
	{
		.capture = {
			.stream_name = "CS-VOICE HOST TX CAPTURE",
			.aif_name = "STUB_HOST_TX_CAPTURE_TX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_dai_stub_ops,
		.probe = &msm_dai_stub_dai_probe,
		.remove = &msm_dai_stub_dai_remove,
	},
};

static struct snd_soc_dai_driver msm_dai_stub_host_playback_rx_dai[] = {
	{
		.playback = {
			.stream_name = "CS-VOICE HOST RX PLAYBACK",
			.aif_name = "STUB_HOST_RX_PLAYBACK_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_dai_stub_ops,
		.probe = &msm_dai_stub_dai_probe,
		.remove = &msm_dai_stub_dai_remove,
	},
	{
		.playback = {
			.stream_name = "CS-VOICE HOST TX PLAYBACK",
			.aif_name = "STUB_HOST_TX_PLAYBACK_RX",
			.rates = SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_8000 |
				 SNDRV_PCM_RATE_16000,
			.formats = SNDRV_PCM_FMTBIT_S16_LE,
			.channels_min = 1,
			.channels_max = 2,
			.rate_min = 8000,
			.rate_max = 48000,
		},
		.ops = &msm_dai_stub_ops,
		.probe = &msm_dai_stub_dai_probe,
		.remove = &msm_dai_stub_dai_remove,
	},
};

static const struct snd_soc_component_driver msm_dai_stub_component = {
	.name		= "msm-dai-stub-dev",
};

static int msm_dai_stub_dev_probe(struct platform_device *pdev)
{
	int rc, id = -1;
	const char *stub_dev_id = "qcom,msm-dai-stub-dev-id";

	rc = of_property_read_u32(pdev->dev.of_node, stub_dev_id, &id);
	if (rc) {
		dev_err(&pdev->dev,
			"%s: missing %s in dt node\n", __func__, stub_dev_id);
		return rc;
	}

	pdev->id = id;

	pr_debug("%s: dev name %s, id:%d\n", __func__,
		 dev_name(&pdev->dev), pdev->id);

	switch (id) {
	case STUB_RX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component, &msm_dai_stub_dai_rx, 1);
		break;
	case STUB_TX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component, &msm_dai_stub_dai_tx[0], 1);
		break;
	case STUB_1_TX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component, &msm_dai_stub_dai_tx[1], 1);
		break;
	case STUB_DTMF_TX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component,
			&msm_dai_stub_dtmf_tx_dai, 1);
		break;
	case STUB_HOST_RX_CAPTURE_TX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component,
			&msm_dai_stub_host_capture_tx_dai[0], 1);
		break;
	case STUB_HOST_TX_CAPTURE_TX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component,
			&msm_dai_stub_host_capture_tx_dai[1], 1);
		break;
	case STUB_HOST_RX_PLAYBACK_RX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component,
			&msm_dai_stub_host_playback_rx_dai[0], 1);
		break;
	case STUB_HOST_TX_PLAYBACK_RX:
		rc = snd_soc_register_component(&pdev->dev,
			&msm_dai_stub_component,
			&msm_dai_stub_host_playback_rx_dai[1], 1);
		break;
	}

	return rc;
}

static int msm_dai_stub_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_dai_stub_dev_dt_match[] = {
	{ .compatible = "qcom,msm-dai-stub-dev", },
	{ }
};
MODULE_DEVICE_TABLE(of, msm_dai_stub_dev_dt_match);

static struct platform_driver msm_dai_stub_dev = {
	.probe  = msm_dai_stub_dev_probe,
	.remove = msm_dai_stub_dev_remove,
	.driver = {
		.name = "msm-dai-stub-dev",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_stub_dev_dt_match,
	},
};

static int msm_dai_stub_probe(struct platform_device *pdev)
{
	int rc = 0;

	dev_dbg(&pdev->dev, "dev name %s\n", dev_name(&pdev->dev));

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
	} else
		dev_dbg(&pdev->dev, "%s: added child node\n", __func__);

	return rc;
}

static int msm_dai_stub_remove(struct platform_device *pdev)
{
	pr_debug("%s:\n", __func__);

	return 0;
}

static const struct of_device_id msm_dai_stub_dt_match[] = {
	{.compatible = "qcom,msm-dai-stub"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_dai_stub_dt_match);


static struct platform_driver msm_dai_stub_driver = {
	.probe  = msm_dai_stub_probe,
	.remove = msm_dai_stub_remove,
	.driver = {
		.name = "msm-dai-stub",
		.owner = THIS_MODULE,
		.of_match_table = msm_dai_stub_dt_match,
	},
};

static int __init msm_dai_stub_init(void)
{
	int rc = 0;

	pr_debug("%s:\n", __func__);

	rc = platform_driver_register(&msm_dai_stub_driver);
	if (rc) {
		pr_err("%s: fail to register dai q6 driver", __func__);
		goto fail;
	}

	rc = platform_driver_register(&msm_dai_stub_dev);
	if (rc) {
		pr_err("%s: fail to register dai q6 dev driver", __func__);
		goto dai_stub_dev_fail;
	}
	return rc;

dai_stub_dev_fail:
	platform_driver_unregister(&msm_dai_stub_driver);
fail:
	return rc;
}
module_init(msm_dai_stub_init);

static void __exit msm_dai_stub_exit(void)
{
	pr_debug("%s:\n", __func__);

	platform_driver_unregister(&msm_dai_stub_dev);
	platform_driver_unregister(&msm_dai_stub_driver);
}
module_exit(msm_dai_stub_exit);

/* Module information */
MODULE_DESCRIPTION("MSM Stub DSP DAI driver");
MODULE_LICENSE("GPL v2");
