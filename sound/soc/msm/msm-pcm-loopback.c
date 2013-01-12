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
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/apr_audio.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/q6asm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>

#include "msm-pcm-routing.h"

struct msm_pcm {
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
};

static void stop_pcm(struct msm_pcm *pcm);

static struct msm_pcm pcm_info;

static const struct snd_pcm_hardware dummy_pcm_hardware = {
	.formats                = 0xffffffff,
	.channels_min           = 1,
	.channels_max           = UINT_MAX,

	/* Random values to keep userspace happy when checking constraints */
	.info                   = SNDRV_PCM_INFO_INTERLEAVED |
				  SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.buffer_bytes_max       = 128*1024,
	.period_bytes_min       = PAGE_SIZE,
	.period_bytes_max       = PAGE_SIZE*2,
	.periods_min            = 2,
	.periods_max            = 128,
};

static void event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	pr_debug("%s\n", __func__);
	switch (opcode) {
	case APR_BASIC_RSP_RESULT:
		pr_debug("%s: opcode[0x%x]\n", __func__, opcode);
		break;
	default:
		pr_err("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm *pcm = &pcm_info;
	int ret = 0;

	mutex_lock(&pcm->lock);

	snd_soc_set_runtime_hwparams(substream, &dummy_pcm_hardware);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pcm->playback_substream = substream;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pcm->capture_substream = substream;

	pcm->instance++;
	pr_debug("%s: pcm out open: %d,%d\n", __func__,
			pcm->instance, substream->stream);
	if (pcm->instance == 2) {
		struct snd_soc_pcm_runtime *soc_pcm_rx =
				pcm->playback_substream->private_data;
		struct snd_soc_pcm_runtime *soc_pcm_tx =
				pcm->capture_substream->private_data;
		if (pcm->audio_client != NULL)
			stop_pcm(pcm);

		pcm->audio_client = q6asm_audio_client_alloc(
				(app_cb)event_handler, pcm);
		if (!pcm->audio_client) {
			pr_err("%s: Could not allocate memory\n", __func__);
			mutex_unlock(&pcm->lock);
			return -ENOMEM;
		}
		pcm->session_id = pcm->audio_client->session;
		pcm->audio_client->perf_mode = false;
		ret = q6asm_open_loopack(pcm->audio_client);
		if (ret < 0) {
			pr_err("%s: pcm out open failed\n", __func__);
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
	}
	pr_debug("%s: Instance = %d, Stream ID = %s\n",
			__func__ , pcm->instance, substream->pcm->id);
	runtime->private_data = pcm;

	mutex_unlock(&pcm->lock);

	return 0;
}

int msm_set_lb_volume(unsigned volume)
{
	int rc = 0;
	if (pcm_info.audio_client != NULL) {
		pr_debug("%s: apply loopback vol:%d\n", __func__, volume);
		rc = q6asm_set_volume(pcm_info.audio_client, volume);
		if (rc < 0) {
			pr_err("%s: Send Volume command failed" \
					" rc=%d\n", __func__, rc);
		}
	}
	return rc;
}

static void stop_pcm(struct msm_pcm *pcm)
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
	struct msm_pcm *pcm = runtime->private_data;
	int ret = 0;

	mutex_lock(&pcm->lock);

	pr_debug("%s: end pcm call:%d\n", __func__, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pcm->playback_start = 0;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pcm->capture_start = 0;

	pcm->instance--;
	if (!pcm->playback_start || !pcm->capture_start) {
		pr_debug("%s: end pcm call\n", __func__);
		stop_pcm(pcm);
	}

	mutex_unlock(&pcm->lock);
	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm *pcm = runtime->private_data;

	mutex_lock(&pcm->lock);

	pr_debug("%s: ASM loopback stream:%d\n", __func__, substream->stream);
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
	struct msm_pcm *pcm = runtime->private_data;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("%s: playback_start:%d,capture_start:%d\n", __func__,
				pcm->playback_start, pcm->capture_start);
		if (pcm->playback_start && pcm->capture_start)
			q6asm_run_nowait(pcm->audio_client, 0, 0, 0);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("%s:Pause/Stop - playback_start:%d,capture_start:%d\n",
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

	pr_debug("%s: ASM loopback\n", __func__);

	return snd_pcm_lib_alloc_vmalloc_buffer(substream,
		params_buffer_bytes(params));
}

static int msm_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_vmalloc_buffer(substream);
}

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.hw_params	= msm_pcm_hw_params,
	.hw_free	= msm_pcm_hw_free,
	.close          = msm_pcm_close,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
};


static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	return ret;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
};

static __devinit int msm_pcm_probe(struct platform_device *pdev)
{
	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				   &msm_soc_platform);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-loopback",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_soc_platform_init(void)
{
	memset(&pcm_info, 0, sizeof(struct msm_pcm));
	mutex_init(&pcm_info.lock);

	return platform_driver_register(&msm_pcm_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	mutex_destroy(&pcm_info.lock);
	platform_driver_unregister(&msm_pcm_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("ASM loopback module platform driver");
MODULE_LICENSE("GPL v2");
