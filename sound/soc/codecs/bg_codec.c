/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/info.h>
#include <soc/qcom/bg_glink.h>
#include "pktzr.h"


#define BG_RATES_MAX (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			    SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			    SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)
#define BG_FORMATS_S16_S24_LE (SNDRV_PCM_FMTBIT_S16_LE | \
				  SNDRV_PCM_FMTBIT_S24_LE | \
				  SNDRV_PCM_FMTBIT_S24_3LE)



enum {
	BG_AIF1_PB = 0,
	BG_AIF2_PB,
	BG_AIF3_PB,
	BG_AIF4_PB,
	BG_AIF1_CAP,
	BG_AIF2_CAP,
	BG_AIF3_CAP,
	BG_AIF4_CAP,
	NUM_CODEC_DAIS,
};


enum {
	PLAYBACK = 0,
	CAPTURE,
};

struct bg_hw_params {
	u32 active_session;
	u32 rx_sample_rate;
	u32 rx_bit_width;
	u32 rx_num_channels;
	u32 tx_sample_rate;
	u32 tx_bit_width;
	u32 tx_num_channels;
};

struct bg_cdc_priv {
	struct device *dev;
	struct snd_soc_codec *codec;
	struct platform_device *pdev_child;
	struct work_struct bg_cdc_add_child_devices_work;
	struct delayed_work bg_cdc_pktzr_init_work;
	unsigned long status_mask;
	struct bg_hw_params hw_params;
	int src[NUM_CODEC_DAIS];
};

struct codec_ssn_rt_setup_t {
	/* active session_id */
	uint32_t active_session;
	/* To indicate if playback/record happens from/to BG or MSM */
	uint32_t route_to_bg;
};

struct graphite_basic_rsp_result {
	/* Valid Graphite error code or completion status */
	uint32_t status;
};

static int bg_get_src(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(codec);
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->reg;

	if ((bg_cdc == NULL) || (dai_id >= NUM_CODEC_DAIS) ||
		(dai_id < 0)) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = bg_cdc->src[dai_id];
	dev_dbg(codec->dev, "%s: dai_id: %d src: %d\n", __func__,
			dai_id, bg_cdc->src[dai_id]);

	return 0;
}

static int bg_put_src(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(codec);
	int dai_id = ((struct soc_multi_mixer_control *)
				kcontrol->private_value)->reg;

	if ((bg_cdc == NULL) || (dai_id >= NUM_CODEC_DAIS) ||
		(dai_id < 0)) {
		pr_err("%s invalid input\n", __func__);
		return -EINVAL;
	}

	bg_cdc->src[dai_id] = ucontrol->value.integer.value[0];
	dev_dbg(codec->dev, "%s: dai_id: %d src: %d\n", __func__,
			dai_id, bg_cdc->src[dai_id]);

	return 0;
}


static const struct snd_kcontrol_new bg_snd_controls[] = {
	SOC_SINGLE_EXT("RX_0 SRC", BG_AIF1_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("RX_1 SRC", BG_AIF2_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("RX_2 SRC", BG_AIF3_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("RX_3 SRC", BG_AIF4_PB, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_0 DST", BG_AIF1_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_1 DST", BG_AIF2_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_2 DST", BG_AIF3_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
	SOC_SINGLE_EXT("TX_3 DST", BG_AIF4_CAP, 0, 1, 0,
	bg_get_src, bg_put_src),
};


static int bg_cdc_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s: substream = %s  stream = %d\n" , __func__,
			substream->name, substream->stream);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		set_bit(PLAYBACK, &bg_cdc->status_mask);
	else
		set_bit(CAPTURE, &bg_cdc->status_mask);

	return 0;
}

static void bg_cdc_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s: substream = %s  stream = %d\n" , __func__,
			substream->name, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		clear_bit(PLAYBACK, &bg_cdc->status_mask);
	else
		clear_bit(PLAYBACK, &bg_cdc->status_mask);

}

static int bg_cdc_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);
	struct bg_hw_params hw_params;
	struct pktzr_cmd_rsp rsp;
	int ret = 0;

	rsp.buf = NULL;

	pr_debug("%s: dai_name = %s DAI-ID %x rate %d width %d num_ch %d\n",
		 __func__, dai->name, dai->id, params_rate(params),
		 params_width(params), params_channels(params));

	switch (dai->id) {
	case BG_AIF1_PB:
		bg_cdc->hw_params.active_session = 0x0001;
		break;
	case BG_AIF1_CAP:
		bg_cdc->hw_params.active_session = 0x00010000;
		break;
	case BG_AIF2_PB:
		bg_cdc->hw_params.active_session = 0x0001;
		break;
	case BG_AIF2_CAP:
		bg_cdc->hw_params.active_session = 0x00020000;
		break;
	case BG_AIF3_PB:
		bg_cdc->hw_params.active_session = 0x0002;
		break;
	case BG_AIF3_CAP:
		bg_cdc->hw_params.active_session = 0x00010000;
		break;
	case BG_AIF4_PB:
		bg_cdc->hw_params.active_session = 0x0004;
		break;
	case BG_AIF4_CAP:
		bg_cdc->hw_params.active_session = 0x00020000;
		break;
	default:
		pr_err("%s:Invalid dai id %d", __func__, dai->id);
		ret = -EINVAL;
		goto exit;
	}


	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		bg_cdc->hw_params.rx_sample_rate = params_rate(params);
		bg_cdc->hw_params.rx_bit_width = params_width(params);
		bg_cdc->hw_params.rx_num_channels = params_channels(params);
	} else {
		bg_cdc->hw_params.tx_sample_rate = params_rate(params);
		bg_cdc->hw_params.tx_bit_width = params_width(params);
		bg_cdc->hw_params.tx_num_channels = params_channels(params);
	}

	/* check if RX, TX sampling freq is same if not return error. */
	/* Send command to BG for HW params */
	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	if (!rsp.buf)
		return -ENOMEM;
	memcpy(&hw_params, &bg_cdc->hw_params, sizeof(hw_params));
	/* Send command to BG to start session */
	ret = pktzr_cmd_set_params(&hw_params, sizeof(hw_params), &rsp);
	if (ret < 0) {
		pr_err("pktzr cmd set params failed\n");
		goto exit;
	}
exit:
	if (rsp.buf)
		kzfree(rsp.buf);
	return ret;
}

static int bg_cdc_hw_free(struct snd_pcm_substream *substream,
			  struct snd_soc_dai *dai)
{
	struct pktzr_cmd_rsp rsp;
	int ret = 0;
	struct codec_ssn_rt_setup_t codec_start;
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	pr_debug("%s: dai_name = %s DAI-ID %x\n", __func__, dai->name, dai->id);

	rsp.buf = NULL;

	switch (dai->id) {
	case BG_AIF1_PB:
		codec_start.active_session = 0x0001;
		break;
	case BG_AIF1_CAP:
		codec_start.active_session = 0x00010000;
		break;
	case BG_AIF2_PB:
		codec_start.active_session = 0x0001;
		break;
	case BG_AIF2_CAP:
		codec_start.active_session = 0x00020000;
		break;
	case BG_AIF3_PB:
		codec_start.active_session = 0x0002;
		break;
	case BG_AIF3_CAP:
		codec_start.active_session = 0x00010000;
		break;
	case BG_AIF4_PB:
		codec_start.active_session = 0x0004;
		break;
	case BG_AIF4_CAP:
		codec_start.active_session = 0x00020000;
		break;
	default:
		pr_err("%s:Invalid dai id %d", __func__, dai->id);
		ret = -EINVAL;
		goto exit;
	}

	codec_start.route_to_bg = bg_cdc->src[dai->id];
	pr_debug("%s active_session %x route_to_bg %d\n",
		__func__, codec_start.active_session, codec_start.route_to_bg);
	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	if (!rsp.buf)
		return -ENOMEM;
	/* Send command to BG to stop session */
	ret = pktzr_cmd_stop(&codec_start,
			sizeof(codec_start), &rsp);
	if (ret < 0) {
		pr_err("pktzr cmd close failed\n");
		goto exit;
	}
exit:
	if (rsp.buf)
		kzfree(rsp.buf);
	return ret;
}

static int bg_cdc_prepare(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct pktzr_cmd_rsp rsp;
	int ret = 0;
	struct codec_ssn_rt_setup_t codec_start;
	struct bg_cdc_priv *bg_cdc = snd_soc_codec_get_drvdata(dai->codec);

	rsp.buf = NULL;


	if (test_bit(PLAYBACK, &bg_cdc->status_mask) &&
	    test_bit(CAPTURE, &bg_cdc->status_mask)) {
		if ((bg_cdc->hw_params.rx_sample_rate !=
		    bg_cdc->hw_params.tx_sample_rate) ||
		    (bg_cdc->hw_params.rx_bit_width !=
		    bg_cdc->hw_params.rx_bit_width)) {
			pr_err("%s diff rx and tx configuration %d:%d:%d:%d\n",
				__func__, bg_cdc->hw_params.rx_sample_rate,
				bg_cdc->hw_params.tx_sample_rate,
				bg_cdc->hw_params.rx_bit_width,
				bg_cdc->hw_params.rx_bit_width);
			return -EINVAL;
		}
	}

	switch (dai->id) {
	case BG_AIF1_PB:
		codec_start.active_session = 0x0001;
		break;
	case BG_AIF1_CAP:
		codec_start.active_session = 0x00010000;
		break;
	case BG_AIF2_PB:
		codec_start.active_session = 0x0001;
		break;
	case BG_AIF2_CAP:
		codec_start.active_session = 0x00020000;
		break;
	case BG_AIF3_PB:
		codec_start.active_session = 0x0002;
		break;
	case BG_AIF3_CAP:
		codec_start.active_session = 0x00010000;
		break;
	case BG_AIF4_PB:
		codec_start.active_session = 0x0004;
		break;
	case BG_AIF4_CAP:
		codec_start.active_session = 0x00020000;
		break;
	default:
		pr_err("%s:Invalid dai id %d", __func__, dai->id);
		ret = -EINVAL;
		goto exit;
	}

	codec_start.route_to_bg = bg_cdc->src[dai->id];
	pr_debug("%s active_session %x route_to_bg %d\n",
		__func__, codec_start.active_session, codec_start.route_to_bg);
	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	if (!rsp.buf)
		return -ENOMEM;
	/* Send command to BG to start session */
	ret = pktzr_cmd_start(&codec_start, sizeof(codec_start), &rsp);
	if (ret < 0) {
		pr_err("pktzr cmd start failed\n");
		goto exit;
	}
exit:
	if (rsp.buf)
		kzfree(rsp.buf);
	return ret;

}

static int bg_cdc_set_channel_map(struct snd_soc_dai *dai,
				  unsigned int tx_num, unsigned int *tx_slot,
				  unsigned int rx_num, unsigned int *rx_slot)
{
	pr_debug("in func %s", __func__);
	return 0;
}


static int bg_cdc_get_channel_map(struct snd_soc_dai *dai,
				  unsigned int *tx_num, unsigned int *tx_slot,
				  unsigned int *rx_num, unsigned int *rx_slot)
{
	pr_debug("in func %s", __func__);
	return 0;
}

static struct snd_soc_dai_ops bg_cdc_dai_ops = {
	.startup = bg_cdc_startup,
	.shutdown = bg_cdc_shutdown,
	.hw_params = bg_cdc_hw_params,
	.hw_free = bg_cdc_hw_free,
	.prepare = bg_cdc_prepare,
	.set_channel_map = bg_cdc_set_channel_map,
	.get_channel_map = bg_cdc_get_channel_map,
};

static struct snd_soc_dai_driver bg_cdc_dai[] = {
	{
		.name = "bg_cdc_rx1",
		.id = BG_AIF1_PB,
		.playback = {
			.stream_name = "AIF1 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_rx2",
		.id = BG_AIF2_PB,
		.playback = {
			.stream_name = "AIF2 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_rx3",
		.id = BG_AIF3_PB,
		.playback = {
			.stream_name = "AIF3 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_rx4",
		.id = BG_AIF4_PB,
		.playback = {
			.stream_name = "AIF4 Playback",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx1",
		.id = BG_AIF1_CAP,
		.capture = {
			.stream_name = "AIF1 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx2",
		.id = BG_AIF2_CAP,
		.capture = {
			.stream_name = "AIF2 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx3",
		.id = BG_AIF3_CAP,
		.capture = {
			.stream_name = "AIF3 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
	{
		.name = "bg_cdc_tx4",
		.id = BG_AIF4_CAP,
		.capture = {
			.stream_name = "AIF4 Capture",
			.rates = BG_RATES_MAX,
			.formats = BG_FORMATS_S16_S24_LE,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 4,
		},
		.ops = &bg_cdc_dai_ops,
	},
};

static int data_cmd_rsp(void *buf, uint32_t len, void *priv_data,
			 bool *is_basic_rsp)
{
	struct graphite_basic_rsp_result *resp;

	pr_debug("in data_cmd_rsp");

	if (buf != NULL) {
		resp = buf;
		pr_err("%s: status = %d\n", __func__, resp->status);
	}

	return 0;
}

static void bg_cdc_pktzr_init(struct work_struct *work)
{
	int ret;
	struct bg_cdc_priv *bg_cdc;
	struct delayed_work *dwork;
	int num_of_intents = 2;
	uint32_t size[2] = {2048, 2048};
	struct bg_glink_ch_cfg ch_info[1] = {
		{"CODEC_CHANNEL", num_of_intents, size}
	};
	struct bg_hw_params hw_params;
	struct pktzr_cmd_rsp rsp;

	pr_debug("%s\n", __func__);
	dwork = to_delayed_work(work);
	bg_cdc = container_of(dwork, struct bg_cdc_priv,
			     bg_cdc_pktzr_init_work);

	ret = pktzr_init(bg_cdc->pdev_child, ch_info, 1, data_cmd_rsp);
	if (ret < 0) {
		dev_err(bg_cdc->dev, "%s: failed in pktzr_init\n", __func__);
		/*return ret*/;
	}

	/* Send open command */
	rsp.buf_size = sizeof(struct graphite_basic_rsp_result);
	rsp.buf = kzalloc(rsp.buf_size, GFP_KERNEL);
	memcpy(&hw_params, &bg_cdc->hw_params, sizeof(hw_params));
	/* Send command to BG to start session */
	ret = pktzr_cmd_open(&hw_params, sizeof(hw_params), &rsp);
	if (ret < 0)
		pr_err("pktzr cmd open failed\n");

	if (rsp.buf)
		kzfree(rsp.buf);
}

static int bg_cdc_codec_probe(struct snd_soc_codec *codec)
{
	struct bg_cdc_priv *bg_cdc = dev_get_drvdata(codec->dev);

	schedule_delayed_work(&bg_cdc->bg_cdc_pktzr_init_work,
				msecs_to_jiffies(400));

	return 0;
}

static int bg_cdc_codec_remove(struct snd_soc_codec *codec)
{
	pr_debug("In func %s\n", __func__);
	pktzr_deinit();
	return 0;
}

static struct snd_soc_codec_driver soc_codec_dev_bg_cdc = {
	.probe = bg_cdc_codec_probe,
	.remove = bg_cdc_codec_remove,
	.controls = bg_snd_controls,
	.num_controls = ARRAY_SIZE(bg_snd_controls),
};

static void bg_cdc_add_child_devices(struct work_struct *work)
{
	struct platform_device *pdev = NULL;
	struct device_node *node;
	char plat_dev_name[50] = "bg-cdc";
	struct bg_cdc_priv *bg_cdc;
	int ret;

	pr_debug("%s\n", __func__);
	bg_cdc = container_of(work, struct bg_cdc_priv,
			     bg_cdc_add_child_devices_work);
	if (!bg_cdc) {
		pr_err("%s: Memory for BG codec does not exist\n",
			__func__);
		return;
	}
	for_each_available_child_of_node(bg_cdc->dev->of_node, node) {
		pr_debug("hnode->name = %s\n", node->name);
		pdev = platform_device_alloc(plat_dev_name, -1);
		if (!pdev) {
			dev_err(bg_cdc->dev, "%s: pdev memory alloc failed\n",
					__func__);
			ret = -ENOMEM;
			goto err;
		}
		pdev->dev.parent = bg_cdc->dev;
		pdev->dev.of_node = node;

		ret = platform_device_add(pdev);
		if (ret) {
			dev_err(&pdev->dev,
					"%s: Cannot add platform device\n",
					__func__);
			goto fail_pdev_add;
		}
		bg_cdc->pdev_child = pdev;
	}
fail_pdev_add:
	if (pdev)
		platform_device_put(pdev);
err:
	return;
}

static int bg_cdc_probe(struct platform_device *pdev)
{
	struct bg_cdc_priv *bg_cdc;
	int ret = 0;

	bg_cdc = kzalloc(sizeof(struct bg_cdc_priv),
			    GFP_KERNEL);
	if (!bg_cdc)
		return -ENOMEM;

	bg_cdc->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, bg_cdc);

	ret = snd_soc_register_codec(&pdev->dev, &soc_codec_dev_bg_cdc,
					bg_cdc_dai, ARRAY_SIZE(bg_cdc_dai));
	if (ret) {
		dev_err(&pdev->dev, "%s: Codec registration failed, ret = %d\n",
			__func__, ret);
		goto err_cdc_reg;
	}

	INIT_WORK(&bg_cdc->bg_cdc_add_child_devices_work,
		  bg_cdc_add_child_devices);
	INIT_DELAYED_WORK(&bg_cdc->bg_cdc_pktzr_init_work,
		  bg_cdc_pktzr_init);
	schedule_work(&bg_cdc->bg_cdc_add_child_devices_work);

	dev_dbg(&pdev->dev, "%s: BG driver probe done\n", __func__);
	return ret;

err_cdc_reg:

	kfree(bg_cdc);
	return ret;
}

static int bg_cdc_remove(struct platform_device *pdev)
{
	struct bg_cdc_priv *bg_cdc;

	bg_cdc = platform_get_drvdata(pdev);

	snd_soc_unregister_codec(&pdev->dev);
	kfree(bg_cdc);
	return 0;
}


#define MODULE_NAME "bg_codec"

static const struct of_device_id audio_codec_of_match[] = {
	{ .compatible = "qcom,bg-codec", },
	{},
};

static struct platform_driver bg_codec_driver = {
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = audio_codec_of_match,
	},
	.probe = bg_cdc_probe,
	.remove = bg_cdc_remove,
};
module_platform_driver(bg_codec_driver);

MODULE_DESCRIPTION("BG Codec driver Loader module");
MODULE_LICENSE("GPL v2");
