/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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
#include <linux/err.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>

#include "msm-pcm-voice.h"
#include "qdsp6/q6voice.h"

static struct msm_voice voice_info;

static struct snd_pcm_hardware msm_pcm_hardware = {

	.info =                 SNDRV_PCM_INFO_INTERLEAVED,
	.formats =              SNDRV_PCM_FMTBIT_S16_LE,
	.rates =                SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000,
	.rate_min =             8000,
	.rate_max =             16000,
	.channels_min =         1,
	.channels_max =         1,

	.buffer_bytes_max =     4096 * 2,
	.period_bytes_min =     4096,
	.period_bytes_max =     4096,
	.periods_min =          2,
	.periods_max =          2,

	.fifo_size =            0,
};

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_voice *prtd = runtime->private_data;

	pr_debug("%s\n", __func__);

	if (!prtd->playback_start)
		prtd->playback_start = 1;

	return 0;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_voice *prtd = runtime->private_data;

	pr_debug("%s\n", __func__);

	if (!prtd->capture_start)
		prtd->capture_start = 1;

	return 0;
}
static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_voice *voice = &voice_info;

	mutex_lock(&voice->lock);

	runtime->hw = msm_pcm_hardware;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		voice->playback_substream = substream;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		voice->capture_substream = substream;

	voice->instance++;
	pr_debug(" %s: instance: %d\n", __func__ , voice->instance);
	runtime->private_data = voice;

	mutex_unlock(&voice->lock);

	return 0;
}
static int msm_pcm_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_voice *prtd = runtime->private_data;

	pr_debug("%s\n", __func__);

	if (prtd->playback_start)
		prtd->playback_start = 0;

	prtd->playback_substream = NULL;

	return 0;
}
static int msm_pcm_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_voice *prtd = runtime->private_data;

	pr_debug("%s\n", __func__);

	if (prtd->capture_start)
		prtd->capture_start = 0;
	prtd->capture_substream = NULL;

	return 0;
}
static int msm_pcm_close(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_voice *prtd = runtime->private_data;
	int ret = 0;

	mutex_lock(&prtd->lock);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_close(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_close(substream);

	prtd->instance--;
	if (!prtd->playback_start && !prtd->capture_start) {
		pr_debug("end voice call\n");
		voc_end_voice_call(voc_get_session_id(VOICE_SESSION_NAME));
	}
	mutex_unlock(&prtd->lock);

	return ret;
}
static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_voice *prtd = runtime->private_data;

	mutex_lock(&prtd->lock);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_prepare(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_prepare(substream);

	if (prtd->playback_start && prtd->capture_start)
		voc_start_voice_call(voc_get_session_id(VOICE_SESSION_NAME));

	mutex_unlock(&prtd->lock);

	return ret;
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{

	pr_debug("%s: Voice\n", __func__);

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int msm_voice_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int msm_voice_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume: %d\n", __func__, volume);

	voc_set_rx_vol_index(voc_get_session_id(VOICE_SESSION_NAME),
			     RX_PATH,
			     volume);
	return 0;
}

static int msm_voice_mute_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = 0;
	return 0;
}

static int msm_voice_mute_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int mute = ucontrol->value.integer.value[0];

	pr_debug("%s: mute=%d\n", __func__, mute);

	voc_set_tx_mute(voc_get_session_id(VOICE_SESSION_NAME), TX_PATH, mute);

	return 0;
}

static int msm_voice_rx_device_mute_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		voc_get_rx_device_mute(voc_get_session_id(VOICE_SESSION_NAME));
	return 0;
}

static int msm_voice_rx_device_mute_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	int mute = ucontrol->value.integer.value[0];

	pr_debug("%s: mute=%d\n", __func__, mute);

	voc_set_rx_device_mute(voc_get_session_id(VOICE_SESSION_NAME), mute);

	return 0;
}

static const char const *tty_mode[] = {"OFF", "HCO", "VCO", "FULL"};
static const struct soc_enum msm_tty_mode_enum[] = {
		SOC_ENUM_SINGLE_EXT(4, tty_mode),
};

static int msm_voice_tty_mode_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		voc_get_tty_mode(voc_get_session_id(VOICE_SESSION_NAME));
	return 0;
}

static int msm_voice_tty_mode_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int tty_mode = ucontrol->value.integer.value[0];

	pr_debug("%s: tty_mode=%d\n", __func__, tty_mode);

	voc_set_tty_mode(voc_get_session_id(VOICE_SESSION_NAME), tty_mode);

	return 0;
}
static int msm_voice_widevoice_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int wv_enable = ucontrol->value.integer.value[0];

	pr_debug("%s: wv enable=%d\n", __func__, wv_enable);

	voc_set_widevoice_enable(voc_get_session_id(VOICE_SESSION_NAME),
				 wv_enable);

	return 0;
}

static int msm_voice_widevoice_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
	       voc_get_widevoice_enable(voc_get_session_id(VOICE_SESSION_NAME));
	return 0;
}


static int msm_voice_slowtalk_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int st_enable = ucontrol->value.integer.value[0];

	pr_debug("%s: st enable=%d\n", __func__, st_enable);

	voc_set_pp_enable(voc_get_session_id(VOICE_SESSION_NAME),
			MODULE_ID_VOICE_MODULE_ST, st_enable);

	return 0;
}

static int msm_voice_slowtalk_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		voc_get_pp_enable(voc_get_session_id(VOICE_SESSION_NAME),
				MODULE_ID_VOICE_MODULE_ST);
	return 0;
}

static int msm_voice_fens_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	int fens_enable = ucontrol->value.integer.value[0];

	pr_debug("%s: fens enable=%d\n", __func__, fens_enable);

	voc_set_pp_enable(voc_get_session_id(VOICE_SESSION_NAME),
			MODULE_ID_VOICE_MODULE_FENS, fens_enable);

	return 0;
}

static int msm_voice_fens_get(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		voc_get_pp_enable(voc_get_session_id(VOICE_SESSION_NAME),
				MODULE_ID_VOICE_MODULE_FENS);
	return 0;
}

static struct snd_kcontrol_new msm_voice_controls[] = {
	SOC_SINGLE_EXT("Voice Rx Device Mute", SND_SOC_NOPM, 0, 1, 0,
				msm_voice_rx_device_mute_get,
				msm_voice_rx_device_mute_put),
	SOC_SINGLE_EXT("Voice Tx Mute", SND_SOC_NOPM, 0, 1, 0,
				msm_voice_mute_get, msm_voice_mute_put),
	SOC_SINGLE_EXT("Voice Rx Volume", SND_SOC_NOPM, 0, 5, 0,
				msm_voice_volume_get, msm_voice_volume_put),
	SOC_ENUM_EXT("TTY Mode", msm_tty_mode_enum[0], msm_voice_tty_mode_get,
				msm_voice_tty_mode_put),
	SOC_SINGLE_EXT("Widevoice Enable", SND_SOC_NOPM, 0, 1, 0,
			msm_voice_widevoice_get, msm_voice_widevoice_put),
	SOC_SINGLE_EXT("Slowtalk Enable", SND_SOC_NOPM, 0, 1, 0,
				msm_voice_slowtalk_get, msm_voice_slowtalk_put),
	SOC_SINGLE_EXT("FENS Enable", SND_SOC_NOPM, 0, 1, 0,
				msm_voice_fens_get, msm_voice_fens_put),
};

static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.hw_params	= msm_pcm_hw_params,
	.close          = msm_pcm_close,
	.prepare        = msm_pcm_prepare,
};


static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);
	return ret;
}

static int msm_pcm_voice_probe(struct snd_soc_platform *platform)
{
	snd_soc_add_platform_controls(platform, msm_voice_controls,
					ARRAY_SIZE(msm_voice_controls));

	return 0;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
	.probe		= msm_pcm_voice_probe,
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
		.name = "msm-pcm-voice",
		.owner = THIS_MODULE,
	},
	.probe = msm_pcm_probe,
	.remove = __devexit_p(msm_pcm_remove),
};

static int __init msm_soc_platform_init(void)
{
	memset(&voice_info, 0, sizeof(voice_info));
	mutex_init(&voice_info.lock);

	return platform_driver_register(&msm_pcm_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("Voice PCM module platform driver");
MODULE_LICENSE("GPL v2");
