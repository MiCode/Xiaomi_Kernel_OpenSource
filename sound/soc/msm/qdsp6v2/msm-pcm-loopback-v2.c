/* Copyright (c) 2013, The Linux Foundation. All rights reserved.

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.

* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <sound/apr_audio-v2.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/q6asm-v2.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <asm/dma.h>

#include "msm-pcm-routing-v2.h"

#define LOOPBACK_VOL_MAX_STEPS 0x2000

static const DECLARE_TLV_DB_LINEAR(loopback_rx_vol_gain, 0,
				LOOPBACK_VOL_MAX_STEPS);

struct msm_pcm_loopback {
	struct snd_pcm_substream *playback_substream;
	struct snd_pcm_substream *capture_substream;

	int instance;

	struct mutex lock;

	uint32_t samp_rate;
	uint32_t channel_mode;

	int playback_start;
	int capture_start;
	int session_id;
	struct audio_client *audio_client;
	int volume;
};

static void stop_pcm(struct msm_pcm_loopback *pcm);

static const struct snd_pcm_hardware dummy_pcm_hardware = {
	.formats                = 0xffffffff,
	.channels_min           = 1,
	.channels_max           = UINT_MAX,

	/* Random values to keep userspace happy when checking constraints */
	.info                   = SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.buffer_bytes_max       = 128*1024,
	.period_bytes_min       = 1024,
	.period_bytes_max       = 1024*2,
	.periods_min            = 2,
	.periods_max            = 128,
};

static void msm_pcm_loopback_event_handler(uint32_t opcode, uint32_t token,
					   uint32_t *payload, void *priv)
{
	pr_debug("%s\n", __func__);
	switch (opcode) {
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
			break;
		default:
			break;
		}
	}
		break;
	default:
		pr_err("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
}

static int pcm_loopback_set_volume(struct msm_pcm_loopback *prtd, int volume)
{
	int rc = -EINVAL;

	pr_debug("%s Setting volume 0x%x\n", __func__, volume);

	if (prtd && prtd->audio_client) {
		rc = q6asm_set_volume(prtd->audio_client, volume);
		if (rc < 0) {
			pr_err("%s: Send Volume command failed rc = %d\n",
				__func__, rc);
			return rc;
		}
		prtd->volume = volume;
	}
	return rc;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct msm_pcm_loopback *pcm;
	int ret = 0;
	uint16_t bits_per_sample = 16;

	pcm = dev_get_drvdata(rtd->platform->dev);
	mutex_lock(&pcm->lock);

	snd_soc_set_runtime_hwparams(substream, &dummy_pcm_hardware);
	pcm->volume = 0x2000;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pcm->playback_substream = substream;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pcm->capture_substream = substream;

	pcm->instance++;
	dev_dbg(rtd->platform->dev, "%s: pcm out open: %d,%d\n", __func__,
			pcm->instance, substream->stream);
	if (pcm->instance == 2) {
		struct snd_soc_pcm_runtime *soc_pcm_rx =
				pcm->playback_substream->private_data;
		struct snd_soc_pcm_runtime *soc_pcm_tx =
				pcm->capture_substream->private_data;
		if (pcm->audio_client != NULL)
			stop_pcm(pcm);

		pcm->audio_client = q6asm_audio_client_alloc(
				(app_cb)msm_pcm_loopback_event_handler, pcm);
		if (!pcm->audio_client) {
			dev_err(rtd->platform->dev,
				"%s: Could not allocate memory\n", __func__);
			mutex_unlock(&pcm->lock);
			return -ENOMEM;
		}
		pcm->session_id = pcm->audio_client->session;
		pcm->audio_client->perf_mode = false;
		ret = q6asm_open_loopback_v2(pcm->audio_client,
					     bits_per_sample);
		if (ret < 0) {
			dev_err(rtd->platform->dev,
				"%s: pcm out open failed\n", __func__);
			q6asm_audio_client_free(pcm->audio_client);
			mutex_unlock(&pcm->lock);
			return -ENOMEM;
		}
		msm_pcm_routing_reg_phy_stream(soc_pcm_tx->dai_link->be_id,
			pcm->audio_client->perf_mode,
			pcm->session_id, pcm->capture_substream->stream);
		msm_pcm_routing_reg_phy_stream(soc_pcm_rx->dai_link->be_id,
			pcm->audio_client->perf_mode,
			pcm->session_id, pcm->playback_substream->stream);
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pcm->playback_substream = substream;
			ret = pcm_loopback_set_volume(pcm, pcm->volume);
			if (ret < 0)
				dev_err(rtd->platform->dev,
					"Error %d setting volume", ret);
		}
	}
	dev_info(rtd->platform->dev, "%s: Instance = %d, Stream ID = %s\n",
			__func__ , pcm->instance, substream->pcm->id);
	runtime->private_data = pcm;

	mutex_unlock(&pcm->lock);

	return 0;
}

static void stop_pcm(struct msm_pcm_loopback *pcm)
{
	struct snd_soc_pcm_runtime *soc_pcm_rx =
		pcm->playback_substream->private_data;
	struct snd_soc_pcm_runtime *soc_pcm_tx =
		pcm->capture_substream->private_data;

	if (pcm->audio_client == NULL)
		return;
	q6asm_cmd(pcm->audio_client, CMD_CLOSE);

	msm_pcm_routing_dereg_phy_stream(soc_pcm_rx->dai_link->be_id,
			SNDRV_PCM_STREAM_PLAYBACK);
	msm_pcm_routing_dereg_phy_stream(soc_pcm_tx->dai_link->be_id,
			SNDRV_PCM_STREAM_CAPTURE);
	q6asm_audio_client_free(pcm->audio_client);
	pcm->audio_client = NULL;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm_loopback *pcm = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	int ret = 0;

	mutex_lock(&pcm->lock);

	dev_dbg(rtd->platform->dev, "%s: end pcm call:%d\n",
		__func__, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pcm->playback_start = 0;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pcm->capture_start = 0;

	pcm->instance--;
	if (!pcm->playback_start || !pcm->capture_start) {
		dev_dbg(rtd->platform->dev, "%s: end pcm call\n", __func__);
		stop_pcm(pcm);
	}

	mutex_unlock(&pcm->lock);
	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm_loopback *pcm = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);

	mutex_lock(&pcm->lock);

	dev_dbg(rtd->platform->dev, "%s: ASM loopback stream:%d\n",
		__func__, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!pcm->playback_start)
			pcm->playback_start = 1;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (!pcm->capture_start)
			pcm->capture_start = 1;
	}
	mutex_unlock(&pcm->lock);

	return ret;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm_loopback *pcm = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev_dbg(rtd->platform->dev,
			"%s: playback_start:%d,capture_start:%d\n", __func__,
			pcm->playback_start, pcm->capture_start);
		if (pcm->playback_start && pcm->capture_start)
			q6asm_run_nowait(pcm->audio_client, 0, 0, 0);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		dev_dbg(rtd->platform->dev,
			"%s:Pause/Stop - playback_start:%d,capture_start:%d\n",
			__func__, pcm->playback_start, pcm->capture_start);
		if (pcm->playback_start && pcm->capture_start)
			q6asm_cmd_nowait(pcm->audio_client, CMD_PAUSE);
		break;
	default:
		break;
	}

	return 0;
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);

	dev_dbg(rtd->platform->dev, "%s: ASM loopback\n", __func__);

	return snd_pcm_lib_alloc_vmalloc_buffer(substream,
		params_buffer_bytes(params));
}

static int msm_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_vmalloc_buffer(substream);
}

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.hw_params      = msm_pcm_hw_params,
	.hw_free        = msm_pcm_hw_free,
	.close          = msm_pcm_close,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
};

static int msm_pcm_volume_ctl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = kcontrol->private_data;
	struct snd_pcm_substream *substream = vol->pcm->streams[0].substream;
	struct msm_pcm_loopback *prtd = substream->runtime->private_data;
	int volume = ucontrol->value.integer.value[0];

	rc = pcm_loopback_set_volume(prtd, volume);
	return rc;
}

static int msm_pcm_add_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm->streams[0].pcm;
	struct snd_pcm_volume *volume_info;
	struct snd_kcontrol *kctl;
	int ret = 0;

	dev_dbg(rtd->dev, "%s, Volume cntrl add\n", __func__);
	ret = snd_pcm_add_volume_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				      NULL, 1,
				      rtd->dai_link->be_id,
				      &volume_info);
	if (ret < 0)
		return ret;
	kctl = volume_info->kctl;
	kctl->put = msm_pcm_volume_ctl_put;
	kctl->tlv.p = loopback_rx_vol_gain;
	return 0;
}

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_pcm_add_controls(rtd);
	if (ret)
		dev_err(rtd->dev, "%s, kctl add failed\n", __func__);
	return ret;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops            = &msm_pcm_ops,
	.pcm_new        = msm_asoc_pcm_new,
};

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
	struct msm_pcm_loopback *pcm;

	dev_set_name(&pdev->dev, "%s", "msm-pcm-loopback");
	dev_dbg(&pdev->dev, "%s: dev name %s\n",
		__func__, dev_name(&pdev->dev));

	pcm = kzalloc(sizeof(struct msm_pcm_loopback), GFP_KERNEL);
	if (!pcm) {
		dev_err(&pdev->dev, "%s Failed to allocate memory for pcm\n",
			__func__);
		return -ENOMEM;
	} else {
		mutex_init(&pcm->lock);
		dev_set_drvdata(&pdev->dev, pcm);
	}
	return snd_soc_register_platform(&pdev->dev,
				   &msm_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	struct msm_pcm_loopback *pcm;

	pcm = dev_get_drvdata(&pdev->dev);
	mutex_destroy(&pcm->lock);
	kfree(pcm);

	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_pcm_loopback_dt_match[] = {
	{.compatible = "qti,msm-pcm-loopback"},
	{}
};

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-loopback",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_loopback_dt_match,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_soc_platform_init(void)
{
	return platform_driver_register(&msm_pcm_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("PCM loopback platform driver");
MODULE_LICENSE("GPL v2");
