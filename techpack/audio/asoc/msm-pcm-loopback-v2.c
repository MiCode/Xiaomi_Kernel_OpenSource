// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/tlv.h>
#include <asm/dma.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6asm-v2.h>

#include "msm-pcm-routing-v2.h"
#include "msm-qti-pp-config.h"

#define DRV_NAME "msm-pcm-loopback-v2"

#define LOOPBACK_VOL_MAX_STEPS 0x2000
#define LOOPBACK_SESSION_MAX 4

static DEFINE_MUTEX(loopback_session_lock);
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
	uint32_t volume;
};

struct fe_dai_session_map {
	char stream_name[32];
	struct msm_pcm_loopback *loopback_priv;
};

static struct fe_dai_session_map session_map[LOOPBACK_SESSION_MAX] = {
	{ {}, NULL},
	{ {}, NULL},
	{ {}, NULL},
	{ {}, NULL},
};

static u32 hfp_tx_mute;

struct msm_pcm_pdata {
	int perf_mode;
	struct snd_pcm *pcm_device[MSM_FRONTEND_DAI_MM_SIZE];
	struct msm_pcm_channel_mixer *chmixer_pspd[MSM_FRONTEND_DAI_MM_SIZE][2];
};

static void stop_pcm(struct msm_pcm_loopback *pcm);
static int msm_pcm_loopback_get_session(struct snd_soc_pcm_runtime *rtd,
					struct msm_pcm_loopback **pcm);

static void msm_pcm_route_event_handler(enum msm_pcm_routing_event event,
					void *priv_data)
{
	struct msm_pcm_loopback *pcm = priv_data;

	WARN_ON(!pcm);

	pr_debug("%s: event 0x%x\n", __func__, event);

	switch (event) {
	case MSM_PCM_RT_EVT_DEVSWITCH:
		q6asm_cmd(pcm->audio_client, CMD_PAUSE);
		q6asm_cmd(pcm->audio_client, CMD_FLUSH);
		q6asm_run(pcm->audio_client, 0, 0, 0);
		/* fallthrough */
	default:
		pr_err("%s: default event 0x%x\n", __func__, event);
		break;
	}
}

static void msm_pcm_loopback_event_handler(uint32_t opcode, uint32_t token,
					   uint32_t *payload, void *priv)
{
	struct msm_pcm_loopback *pcm = priv;
	struct snd_pcm_substream *substream = NULL;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;

	pr_debug("%s:\n", __func__);

	if (pcm->playback_substream != NULL)
		substream = pcm->playback_substream;
	else if (pcm->capture_substream != NULL)
		substream = pcm->capture_substream;

	switch (opcode) {
	case ASM_STREAM_PP_EVENT: {
		pr_debug("%s: ASM_STREAM_EVENT (0x%x)\n", __func__, opcode);
		if (!substream) {
			pr_err("%s: substream is NULL.\n", __func__);
			return;
		}

		rtd = substream->private_data;
		if (!rtd) {
			pr_err("%s: rtd is NULL\n", __func__);
			return;
		}

		ret = msm_adsp_inform_mixer_ctl(rtd, payload);
		if (ret) {
			pr_err("%s: failed to inform mixer ctl. err = %d\n",
			__func__, ret);
			return;
		}

		break;
	}

	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case ASM_STREAM_CMD_REGISTER_PP_EVENTS:
			pr_debug("%s: ASM_STREAM_CMD_REGISTER_PP_EVENTS:\n",
			__func__);
			break;
		default:
			break;
		}

		break;
	}

	default:
		pr_err("%s: Not Supported Event opcode[0x%x]\n",
			__func__, opcode);
		break;
	}
}

static int msm_loopback_session_mute_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = hfp_tx_mute;
	return 0;
}

static int msm_loopback_session_mute_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0, n = 0;
	int mute = ucontrol->value.integer.value[0];
	struct msm_pcm_loopback *pcm = NULL;

	if ((mute < 0) || (mute > 1)) {
		pr_err(" %s Invalid arguments", __func__);
		ret = -EINVAL;
		goto done;
	}
	mutex_lock(&loopback_session_lock);
	pr_debug("%s: mute=%d\n", __func__, mute);
	hfp_tx_mute = mute;
	for (n = 0; n < LOOPBACK_SESSION_MAX; n++) {
		if (!strcmp(session_map[n].stream_name, "MultiMedia6"))
			pcm = session_map[n].loopback_priv;
	}
	if (pcm && pcm->audio_client) {
		ret = q6asm_set_mute(pcm->audio_client, mute);
		if (ret < 0)
			pr_err("%s: Send mute command failed rc=%d\n",
				__func__, ret);
	}
	mutex_unlock(&loopback_session_lock);
done:
	return ret;
}

static struct snd_kcontrol_new msm_loopback_controls[] = {
	SOC_SINGLE_EXT("HFP TX Mute", SND_SOC_NOPM, 0, 1, 0,
			msm_loopback_session_mute_get,
			msm_loopback_session_mute_put),
};

static int msm_pcm_loopback_probe(struct snd_soc_component *component)
{
	if (of_property_read_bool(component->dev->of_node,
				  "qcom,msm-pcm-loopback")) {
		snd_soc_add_component_controls(component, msm_loopback_controls,
					       ARRAY_SIZE(msm_loopback_controls));
	}

	return 0;
}

static int pcm_loopback_set_volume(struct msm_pcm_loopback *prtd,
				   uint32_t volume)
{
	int rc = -EINVAL;

	pr_debug("%s: Setting volume 0x%x\n", __func__, volume);

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

static int msm_pcm_loopback_get_session(struct snd_soc_pcm_runtime *rtd,
					struct msm_pcm_loopback **pcm)
{
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	int ret = 0;
	int n, index = -1;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	dev_dbg(component->dev, "%s: stream %s\n", __func__,
		rtd->dai_link->stream_name);

	mutex_lock(&loopback_session_lock);
	for (n = 0; n < LOOPBACK_SESSION_MAX; n++) {
		if (!strcmp(rtd->dai_link->stream_name,
		    session_map[n].stream_name)) {
			*pcm = session_map[n].loopback_priv;
			goto exit;
		}
		/*
		 * Store the min index value for allocating a new session.
		 * Here, if session stream name is not found in the
		 * existing entries after the loop iteration, then this
		 * index will be used to allocate the new session.
		 * This index variable is expected to point to the topmost
		 * available free session.
		 */
		if (!(session_map[n].stream_name[0]) && (index < 0))
			index = n;
	}

	if (index < 0) {
		dev_err(component->dev, "%s: Max Sessions allocated\n",
				 __func__);
		ret = -EAGAIN;
		goto exit;
	}

	session_map[index].loopback_priv = kzalloc(
		sizeof(struct msm_pcm_loopback), GFP_KERNEL);
	if (!session_map[index].loopback_priv) {
		ret = -ENOMEM;
		goto exit;
	}

	strlcpy(session_map[index].stream_name,
		rtd->dai_link->stream_name,
		sizeof(session_map[index].stream_name));
	dev_dbg(component->dev, "%s: stream %s index %d\n",
		__func__, session_map[index].stream_name, index);

	mutex_init(&session_map[index].loopback_priv->lock);
	*pcm = session_map[index].loopback_priv;
exit:
	mutex_unlock(&loopback_session_lock);
	return ret;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	struct msm_pcm_loopback *pcm = NULL;
	int ret = 0;
	uint16_t bits_per_sample = 16;
	struct asm_session_mtmx_strtr_param_window_v2_t asm_mtmx_strtr_window;
	uint32_t param_id;
	struct msm_pcm_pdata *pdata;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ret =  msm_pcm_loopback_get_session(rtd, &pcm);
	if (ret)
		return ret;

	mutex_lock(&pcm->lock);

	pcm->volume = 0x2000;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pcm->playback_substream = substream;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pcm->capture_substream = substream;

	pcm->instance++;
	dev_dbg(component->dev, "%s: pcm out open: %d,%d\n", __func__,
			pcm->instance, substream->stream);
	if (pcm->instance == 2) {
		if (pcm->audio_client != NULL)
			stop_pcm(pcm);

		pdata = (struct msm_pcm_pdata *)
			dev_get_drvdata(component->dev);
		if (!pdata) {
			dev_err(component->dev,
				"%s: platform data not populated\n", __func__);
			mutex_unlock(&pcm->lock);
			return -EINVAL;
		}

		pcm->audio_client = q6asm_audio_client_alloc(
				(app_cb)msm_pcm_loopback_event_handler, pcm);
		if (!pcm->audio_client) {
			dev_err(component->dev,
				"%s: Could not allocate memory\n", __func__);
			mutex_unlock(&pcm->lock);
			return -ENOMEM;
		}
		pcm->session_id = pcm->audio_client->session;
		pcm->audio_client->perf_mode = pdata->perf_mode;
		ret = q6asm_open_loopback_v2(pcm->audio_client,
					     bits_per_sample);
		if (ret < 0) {
			dev_err(component->dev,
				"%s: pcm out open failed\n", __func__);
			q6asm_audio_client_free(pcm->audio_client);
			pcm->audio_client = NULL;
			mutex_unlock(&pcm->lock);
			return -ENOMEM;
		}
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			pcm->playback_substream = substream;
			ret = pcm_loopback_set_volume(pcm, pcm->volume);
			if (ret < 0)
				dev_err(component->dev,
					"Error %d setting volume", ret);
		}
		/* Set to largest negative value */
		asm_mtmx_strtr_window.window_lsw = 0x00000000;
		asm_mtmx_strtr_window.window_msw = 0x80000000;
		param_id = ASM_SESSION_MTMX_STRTR_PARAM_RENDER_WINDOW_START_V2;
		q6asm_send_mtmx_strtr_window(pcm->audio_client,
					     &asm_mtmx_strtr_window,
					     param_id);
		/* Set to largest positive value */
		asm_mtmx_strtr_window.window_lsw = 0xffffffff;
		asm_mtmx_strtr_window.window_msw = 0x7fffffff;
		param_id = ASM_SESSION_MTMX_STRTR_PARAM_RENDER_WINDOW_END_V2;
		q6asm_send_mtmx_strtr_window(pcm->audio_client,
					     &asm_mtmx_strtr_window,
					     param_id);
	}
	dev_info(component->dev, "%s: Instance = %d, Stream ID = %s\n",
			__func__, pcm->instance, substream->pcm->id);
	runtime->private_data = pcm;

	msm_adsp_init_mixer_ctl_pp_event_queue(substream->private_data);

	mutex_unlock(&pcm->lock);

	return 0;
}

static void stop_pcm(struct msm_pcm_loopback *pcm)
{
	struct snd_soc_pcm_runtime *soc_pcm_rx;
	struct snd_soc_pcm_runtime *soc_pcm_tx;

	if (pcm->audio_client == NULL) {
		pr_err("%s: audio client freed\n", __func__);
		return;
	}

	mutex_lock(&loopback_session_lock);
	q6asm_cmd(pcm->audio_client, CMD_CLOSE);

	if (pcm->playback_substream != NULL) {
		soc_pcm_rx = pcm->playback_substream->private_data;
		msm_pcm_routing_dereg_phy_stream(soc_pcm_rx->dai_link->id,
				SNDRV_PCM_STREAM_PLAYBACK);
	}
	if (pcm->capture_substream != NULL) {
		soc_pcm_tx = pcm->capture_substream->private_data;
		msm_pcm_routing_dereg_phy_stream(soc_pcm_tx->dai_link->id,
				SNDRV_PCM_STREAM_CAPTURE);
	}
	q6asm_audio_client_free(pcm->audio_client);
	pcm->audio_client = NULL;
	mutex_unlock(&loopback_session_lock);
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm_loopback *pcm = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	int ret = 0, n;
	bool found = false;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pcm->lock);

	dev_dbg(component->dev, "%s: end pcm call:%d\n",
		__func__, substream->stream);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pcm->playback_start = 0;
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		pcm->capture_start = 0;

	pcm->instance--;
	if (!pcm->playback_start || !pcm->capture_start) {
		dev_dbg(component->dev, "%s: end pcm call\n", __func__);
		stop_pcm(pcm);
	}

	if (!pcm->instance) {
		mutex_lock(&loopback_session_lock);
		for (n = 0; n < LOOPBACK_SESSION_MAX; n++) {
			if (!strcmp(rtd->dai_link->stream_name,
					session_map[n].stream_name)) {
				found = true;
				break;
			}
		}
		if (found) {
			memset(session_map[n].stream_name, 0,
				sizeof(session_map[n].stream_name));
			mutex_unlock(&pcm->lock);
			mutex_destroy(&session_map[n].loopback_priv->lock);
			session_map[n].loopback_priv = NULL;
			kfree(pcm);
			dev_dbg(component->dev, "%s: stream freed %s\n",
				__func__, rtd->dai_link->stream_name);
			mutex_unlock(&loopback_session_lock);
			return 0;
		}
		mutex_unlock(&loopback_session_lock);
	}
	msm_adsp_clean_mixer_ctl_pp_event_queue(substream->private_data);
	mutex_unlock(&pcm->lock);
	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm_loopback *pcm = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	struct msm_pcm_routing_evt event;

	memset(&event, 0, sizeof(event));
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pcm->lock);

	dev_dbg(component->dev, "%s: ASM loopback stream:%d\n",
		__func__, substream->stream);

	if (pcm->playback_start && pcm->capture_start) {
		mutex_unlock(&pcm->lock);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!pcm->playback_start)
			pcm->playback_start = 1;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (!pcm->capture_start)
			pcm->capture_start = 1;
	}

	if (pcm->playback_start && pcm->capture_start) {
		struct snd_soc_pcm_runtime *soc_pcm_rx =
				pcm->playback_substream->private_data;
		struct snd_soc_pcm_runtime *soc_pcm_tx =
			pcm->capture_substream->private_data;
		event.event_func = msm_pcm_route_event_handler;
		event.priv_data = (void *) pcm;

		if (!pcm->audio_client) {
			mutex_unlock(&pcm->lock);
			pr_err("%s: audio client freed\n", __func__);
			return -EINVAL;
		}
		msm_pcm_routing_reg_phy_stream(soc_pcm_tx->dai_link->id,
			pcm->audio_client->perf_mode,
			pcm->session_id, pcm->capture_substream->stream);
		msm_pcm_routing_reg_phy_stream_v2(soc_pcm_rx->dai_link->id,
			pcm->audio_client->perf_mode,
			pcm->session_id, pcm->playback_substream->stream,
			event);
	}

	mutex_unlock(&pcm->lock);

	return ret;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_pcm_loopback *pcm = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, DRV_NAME);

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		dev_dbg(component->dev,
			"%s: playback_start:%d,capture_start:%d\n", __func__,
			pcm->playback_start, pcm->capture_start);
		if (pcm->playback_start && pcm->capture_start)
			q6asm_run_nowait(pcm->audio_client, 0, 0, 0);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_STOP:
		dev_dbg(component->dev,
			"%s:Pause/Stop - playback_start:%d,capture_start:%d\n",
			__func__, pcm->playback_start, pcm->capture_start);
		if (pcm->playback_start && pcm->capture_start)
			q6asm_cmd_nowait(pcm->audio_client, CMD_PAUSE);
		break;
	default:
		pr_err("%s: default cmd %d\n", __func__, cmd);
		break;
	}

	return 0;
}

static const struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.close          = msm_pcm_close,
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
};

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_pcm_volume_ctl_put(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = kcontrol->private_data;
	struct snd_pcm_substream *substream = vol->pcm->streams[0].substream;
	struct msm_pcm_loopback *prtd;
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume : 0x%x\n", __func__, volume);
	if ((!substream) || (!substream->runtime)) {
		pr_err("%s substream or runtime not found\n", __func__);
		rc = -ENODEV;
		goto exit;
	}
	mutex_lock(&loopback_session_lock);
	if (substream->ref_count > 0) {
		prtd = substream->runtime->private_data;
		if (!prtd) {
			rc = -ENODEV;
			mutex_unlock(&loopback_session_lock);
			goto exit;
		}
		rc = pcm_loopback_set_volume(prtd, volume);
	}
	mutex_unlock(&loopback_session_lock);
exit:
	return rc;
}

static int msm_pcm_volume_ctl_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream = NULL;
	struct msm_pcm_loopback *prtd;

	pr_debug("%s\n", __func__);
	if (!vol) {
		pr_err("%s: vol is NULL\n", __func__);
		return -ENODEV;
	}
	substream = vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if ((!substream) || (!substream->runtime)) {
		pr_debug("%s substream or runtime not found\n", __func__);
		rc = -ENODEV;
		goto exit;
	}
	mutex_lock(&loopback_session_lock);
	if (substream->ref_count > 0) {
		prtd = substream->runtime->private_data;
		if (!prtd) {
			rc = -ENODEV;
			mutex_unlock(&loopback_session_lock);
			goto exit;
		}
		ucontrol->value.integer.value[0] = prtd->volume;
	}
	mutex_unlock(&loopback_session_lock);
exit:
	return rc;
}

static int msm_pcm_add_volume_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm->streams[0].pcm;
	struct snd_pcm_volume *volume_info;
	struct snd_kcontrol *kctl;
	int ret = 0;

	dev_dbg(rtd->dev, "%s, Volume cntrl add\n", __func__);
	ret = snd_pcm_add_volume_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				      NULL, 1,
				      rtd->dai_link->id,
				      &volume_info);
	if (ret < 0)
		return ret;
	kctl = volume_info->kctl;
	kctl->put = msm_pcm_volume_ctl_put;
	kctl->get = msm_pcm_volume_ctl_get;
	kctl->tlv.p = loopback_rx_vol_gain;
	return 0;
}

static int msm_pcm_playback_app_type_cfg_ctl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_RX;
	int be_id = ucontrol->value.integer.value[3];
	struct msm_pcm_stream_app_type_cfg cfg_data = {0, 0, 48000, 0};
	int ret = 0;

	cfg_data.app_type = ucontrol->value.integer.value[0];
	cfg_data.acdb_dev_id = ucontrol->value.integer.value[1];
	if (ucontrol->value.integer.value[2] != 0)
		cfg_data.sample_rate = ucontrol->value.integer.value[2];
	if (ucontrol->value.integer.value[4] != 0)
		cfg_data.copp_token = ucontrol->value.integer.value[4];
	pr_debug("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d copp_token %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate,
		cfg_data.copp_token);
	ret = msm_pcm_routing_reg_stream_app_type_cfg(fe_id, session_type,
						      be_id, &cfg_data);
	if (ret < 0)
		pr_err("%s: msm_pcm_routing_reg_stream_app_type_cfg failed returned %d\n",
			__func__, ret);

	return ret;
}

static int msm_pcm_playback_app_type_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_RX;
	int be_id = 0;
	struct msm_pcm_stream_app_type_cfg cfg_data = {0};
	int ret = 0;

	ret = msm_pcm_routing_get_stream_app_type_cfg(fe_id, session_type,
						      &be_id, &cfg_data);
	if (ret < 0) {
		pr_err("%s: msm_pcm_routing_get_stream_app_type_cfg failed returned %d\n",
			__func__, ret);
		goto done;
	}

	ucontrol->value.integer.value[0] = cfg_data.app_type;
	ucontrol->value.integer.value[1] = cfg_data.acdb_dev_id;
	ucontrol->value.integer.value[2] = cfg_data.sample_rate;
	ucontrol->value.integer.value[3] = be_id;
	ucontrol->value.integer.value[4] = cfg_data.copp_token;
	pr_debug("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d copp_token %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate,
		cfg_data.copp_token);

done:
	return ret;
}

static int msm_pcm_capture_app_type_cfg_ctl_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_TX;
	int be_id = ucontrol->value.integer.value[3];
	struct msm_pcm_stream_app_type_cfg cfg_data = {0, 0, 48000, 0};
	int ret = 0;

	cfg_data.app_type = ucontrol->value.integer.value[0];
	cfg_data.acdb_dev_id = ucontrol->value.integer.value[1];
	if (ucontrol->value.integer.value[2] != 0)
		cfg_data.sample_rate = ucontrol->value.integer.value[2];
	if (ucontrol->value.integer.value[4] != 0)
		cfg_data.copp_token = ucontrol->value.integer.value[4];
	pr_debug("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d copp_token %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate,
		cfg_data.copp_token);
	ret = msm_pcm_routing_reg_stream_app_type_cfg(fe_id, session_type,
						      be_id, &cfg_data);
	if (ret < 0)
		pr_err("%s: msm_pcm_routing_reg_stream_app_type_cfg failed returned %d\n",
			__func__, ret);

	return ret;
}

static int msm_pcm_capture_app_type_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_TX;
	int be_id = 0;
	struct msm_pcm_stream_app_type_cfg cfg_data = {0};
	int ret = 0;

	ret = msm_pcm_routing_get_stream_app_type_cfg(fe_id, session_type,
						      &be_id, &cfg_data);
	if (ret < 0) {
		pr_err("%s: msm_pcm_routing_get_stream_app_type_cfg failed returned %d\n",
			__func__, ret);
		goto done;
	}

	ucontrol->value.integer.value[0] = cfg_data.app_type;
	ucontrol->value.integer.value[1] = cfg_data.acdb_dev_id;
	ucontrol->value.integer.value[2] = cfg_data.sample_rate;
	ucontrol->value.integer.value[3] = be_id;
	ucontrol->value.integer.value[4] = cfg_data.copp_token;
	pr_debug("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d copp_token %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate,
		cfg_data.copp_token);
done:
	return ret;
}

static int msm_pcm_add_app_type_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm->streams[0].pcm;
	struct snd_pcm_usr *app_type_info;
	struct snd_kcontrol *kctl;
	const char *playback_mixer_ctl_name	= "Audio Stream";
	const char *capture_mixer_ctl_name	= "Audio Stream Capture";
	const char *deviceNo		= "NN";
	const char *suffix		= "App Type Cfg";
	int ctl_len, ret = 0;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ctl_len = strlen(playback_mixer_ctl_name) + 1 +
				strlen(deviceNo) + 1 + strlen(suffix) + 1;
		pr_debug("%s: Playback app type cntrl add\n", __func__);
		ret = snd_pcm_add_usr_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
					NULL, 1, ctl_len, rtd->dai_link->id,
					&app_type_info);
		if (ret < 0)
			return ret;
		kctl = app_type_info->kctl;
		snprintf(kctl->id.name, ctl_len, "%s %d %s",
			playback_mixer_ctl_name, rtd->pcm->device, suffix);
		kctl->put = msm_pcm_playback_app_type_cfg_ctl_put;
		kctl->get = msm_pcm_playback_app_type_cfg_ctl_get;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream) {
		ctl_len = strlen(capture_mixer_ctl_name) + 1 +
				strlen(deviceNo) + 1 + strlen(suffix) + 1;
		pr_debug("%s: Capture app type cntrl add\n", __func__);
		ret = snd_pcm_add_usr_ctls(pcm, SNDRV_PCM_STREAM_CAPTURE,
					NULL, 1, ctl_len, rtd->dai_link->id,
					&app_type_info);
		if (ret < 0)
			return ret;
		kctl = app_type_info->kctl;
		snprintf(kctl->id.name, ctl_len, "%s %d %s",
			capture_mixer_ctl_name, rtd->pcm->device, suffix);
		kctl->put = msm_pcm_capture_app_type_cfg_ctl_put;
		kctl->get = msm_pcm_capture_app_type_cfg_ctl_get;
	}

	return 0;
}
#else
static int msm_pcm_add_volume_controls(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}
static int msm_pcm_add_app_type_controls(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}
#endif /* CONFIG_AUDIO_QGKI */

static struct msm_pcm_channel_mixer *msm_pcm_get_chmixer(
			struct msm_pcm_pdata *pdata,
			u64 fe_id, int session_type)
{
	if (!pdata) {
		pr_err("%s: missing pdata\n", __func__);
		return NULL;
	}

	if (fe_id >= MSM_FRONTEND_DAI_MM_SIZE) {
		pr_err("%s: invalid FE %llu\n", __func__, fe_id);
		return NULL;
	}

	if ((session_type != SESSION_TYPE_TX) &&
		(session_type != SESSION_TYPE_RX)) {
		pr_err("%s: invalid session type %d\n", __func__, session_type);
		return NULL;
	}

	return pdata->chmixer_pspd[fe_id][session_type];
}

static int msm_pcm_channel_mixer_cfg_ctl_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int ret = 0;
	int stream_id = 0;
	int be_id = 0, i = 0;
	struct msm_pcm_loopback *prtd = NULL;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	struct snd_pcm *pcm = NULL;
	struct snd_pcm_substream *substream = NULL;
	struct msm_pcm_channel_mixer *chmixer_pspd = NULL;
	u8 asm_ch_map[PCM_FORMAT_MAX_NUM_CHANNEL_V8] = {0};
	bool reset_override_out_ch_map = false;
	bool reset_override_in_ch_map = false;

	pcm = pdata->pcm_device[fe_id];
	if (!pcm) {
		pr_err("%s invalid pcm handle for fe_id %llu\n",
				__func__, fe_id);
		return -EINVAL;
	}

	if (session_type == SESSION_TYPE_RX)
		substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	else
		substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -EINVAL;
	}

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	chmixer_pspd->enable = ucontrol->value.integer.value[0];
	chmixer_pspd->rule = ucontrol->value.integer.value[1];
	chmixer_pspd->input_channel = ucontrol->value.integer.value[2];
	chmixer_pspd->output_channel = ucontrol->value.integer.value[3];
	chmixer_pspd->port_idx = ucontrol->value.integer.value[4];

	if (chmixer_pspd->enable) {
		if (session_type == SESSION_TYPE_RX &&
			!chmixer_pspd->override_in_ch_map) {
			if (chmixer_pspd->input_channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
				pr_err("%s: Invalid channel count %d\n",
					__func__, chmixer_pspd->input_channel);
				return -EINVAL;
			}
			q6asm_map_channels(asm_ch_map,
				chmixer_pspd->input_channel, false);
			for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
				chmixer_pspd->in_ch_map[i] = asm_ch_map[i];
			chmixer_pspd->override_in_ch_map = true;
			reset_override_in_ch_map = true;
		} else if (session_type == SESSION_TYPE_TX &&
			!chmixer_pspd->override_out_ch_map) {
			if (chmixer_pspd->output_channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
				pr_err("%s: Invalid channel count %d\n",
					__func__, chmixer_pspd->output_channel);
				return -EINVAL;
			}
			q6asm_map_channels(asm_ch_map,
				chmixer_pspd->output_channel, false);
			for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
				chmixer_pspd->out_ch_map[i] = asm_ch_map[i];
			chmixer_pspd->override_out_ch_map = true;
			reset_override_out_ch_map = true;
		}
	} else {
		chmixer_pspd->override_out_ch_map = false;
		chmixer_pspd->override_in_ch_map = false;
	}

	/* cache value and take effect during adm_open stage */
	msm_pcm_routing_set_channel_mixer_cfg(fe_id,
			session_type,
			chmixer_pspd);

	mutex_lock(&loopback_session_lock);
	if (substream->ref_count <= 0) {
		pr_err_ratelimited("%s: substream ref_count:%d invalid\n",
				__func__, substream->ref_count);
		mutex_unlock(&loopback_session_lock);
		return -EINVAL;
	}
	if (chmixer_pspd->enable && substream->runtime) {
		prtd = substream->runtime->private_data;
		if (!prtd) {
			pr_err("%s find invalid prtd fail\n", __func__);
			ret = -EINVAL;
			mutex_unlock(&loopback_session_lock);
			goto done;
		}

		if (prtd->audio_client) {
			stream_id = prtd->audio_client->session;
			be_id = chmixer_pspd->port_idx;
			msm_pcm_routing_set_channel_mixer_runtime(be_id,
					stream_id,
					session_type,
					chmixer_pspd);
		}
	}
	mutex_unlock(&loopback_session_lock);
	if (reset_override_out_ch_map)
		chmixer_pspd->override_out_ch_map = false;
	if (reset_override_in_ch_map)
		chmixer_pspd->override_in_ch_map = false;

done:
	return ret;
}

static int msm_pcm_channel_mixer_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = chmixer_pspd->enable;
	ucontrol->value.integer.value[1] = chmixer_pspd->rule;
	ucontrol->value.integer.value[2] = chmixer_pspd->input_channel;
	ucontrol->value.integer.value[3] = chmixer_pspd->output_channel;
	ucontrol->value.integer.value[4] = chmixer_pspd->port_idx;
	return 0;
}

static int msm_pcm_channel_mixer_output_map_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int i = 0;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	chmixer_pspd->override_out_ch_map = true;
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmixer_pspd->out_ch_map[i] =
			ucontrol->value.integer.value[i];

	return 0;
}

static int msm_pcm_channel_mixer_output_map_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int i = 0;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_pspd->out_ch_map[i];
	return 0;
}

static int msm_pcm_channel_mixer_input_map_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int i = 0;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	chmixer_pspd->override_in_ch_map = true;
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmixer_pspd->in_ch_map[i] = ucontrol->value.integer.value[i];

	return 0;
}

static int msm_pcm_channel_mixer_input_map_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int i = 0;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_pspd->in_ch_map[i];
	return 0;
}

static int msm_pcm_channel_mixer_weight_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int channel = (kcontrol->private_value >> 16) & 0xFF;
	int i = 0;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	if (channel <= 0 || channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
		pr_err("%s: invalid channel number %d\n", __func__, channel);
		return -EINVAL;
	}
	channel--;

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmixer_pspd->channel_weight[channel][i] =
			ucontrol->value.integer.value[i];
	return 0;
}

static int msm_pcm_channel_mixer_weight_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int channel = (kcontrol->private_value >> 16) & 0xFF;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_pcm_pdata *pdata = dev_get_drvdata(component->dev);
	int i = 0;
	struct msm_pcm_channel_mixer *chmixer_pspd;

	if (channel <= 0 || channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
		pr_err("%s: invalid channel number %d\n", __func__, channel);
		return -EINVAL;
	}
	channel--;

	chmixer_pspd = msm_pcm_get_chmixer(pdata, fe_id, session_type);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_pspd->channel_weight[channel][i];
	return 0;
}

static int msm_pcm_add_platform_controls(struct snd_kcontrol_new *kctl,
			struct snd_soc_pcm_runtime *rtd, const char *name_prefix,
			const char *name_suffix, int session_type, int channels)
{
	int ret = -EINVAL;
	char *mixer_name = NULL;
	struct snd_pcm *pcm = rtd->pcm;
	const char *deviceNo = "NN";
	const char *channelNo = "NN";
	int ctl_len = 0;
	struct snd_soc_component *component = NULL;

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ctl_len = strlen(name_prefix) + 1 + strlen(deviceNo) + 1 +
		strlen(channelNo) + 1 + strlen(name_suffix) + 1;

	mixer_name = kzalloc(ctl_len, GFP_KERNEL);
	if (mixer_name == NULL)
		return -ENOMEM;

	if (channels >= 0) {
		snprintf(mixer_name, ctl_len, "%s %d %s %d",
			name_prefix, pcm->device, name_suffix, channels);
		kctl->private_value = (rtd->dai_link->id) | (session_type << 8) |
							(channels << 16);
	} else {
		snprintf(mixer_name, ctl_len, "%s %d %s",
			name_prefix, pcm->device, name_suffix);
		kctl->private_value = (rtd->dai_link->id) | (session_type << 8);
	}

	kctl->name = mixer_name;
	ret = snd_soc_add_component_controls(component, kctl, 1);
	kfree(mixer_name);
	return ret;
}

static int msm_pcm_channel_mixer_output_map_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid channel map value ranges from 1 to 64 */
	uinfo->value.integer.min = 1;
	uinfo->value.integer.max = 64;
	return 0;
}

static int msm_pcm_add_channel_mixer_output_map_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix		= "ChMixer Output Map";
	int session_type = 0, ret = 0, channel = -1;
	struct snd_kcontrol_new channel_mixer_output_map_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_channel_mixer_output_map_info,
		.put = msm_pcm_channel_mixer_output_map_ctl_put,
		.get = msm_pcm_channel_mixer_output_map_ctl_get,
		.private_value = 0,
	};

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream != NULL) {
		session_type = SESSION_TYPE_RX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_output_map_control,
				rtd, playback_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_output_map_control,
				rtd, capture_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}
	return 0;

fail:
	pr_err("%s: failed add platform ctl, err = %d\n",
		 __func__, ret);

	return ret;
}

static int msm_pcm_channel_mixer_input_map_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid channel map value ranges from 1 to 64 */
	uinfo->value.integer.min = 1;
	uinfo->value.integer.max = 64;
	return 0;
}

static int msm_pcm_add_channel_mixer_input_map_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix = "ChMixer Input Map";
	int session_type = 0, ret = 0, channel = -1;
	struct snd_kcontrol_new channel_mixer_input_map_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_channel_mixer_input_map_info,
		.put = msm_pcm_channel_mixer_input_map_ctl_put,
		.get = msm_pcm_channel_mixer_input_map_ctl_get,
		.private_value = 0,
	};

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream != NULL) {
		session_type = SESSION_TYPE_RX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_input_map_control,
				rtd, playback_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_input_map_control,
				rtd, capture_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}
	return 0;

fail:
	pr_err("%s: failed add platform ctl, err = %d\n",
		 __func__, ret);
	return ret;
}

static int msm_pcm_channel_mixer_cfg_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	/* five int values: enable, rule, in_channels, out_channels and port_id */
	uinfo->count = 5;
	/* Valid range is all positive values to support above controls */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = INT_MAX;
	return 0;
}

static int msm_pcm_add_channel_mixer_cfg_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix		= "ChMixer Cfg";
	int session_type = 0, ret = 0, channel = -1;
	struct msm_pcm_pdata *pdata = NULL;
	struct snd_soc_component *component = NULL;
	struct snd_kcontrol_new channel_mixer_cfg_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_channel_mixer_cfg_info,
		.put = msm_pcm_channel_mixer_cfg_ctl_put,
		.get = msm_pcm_channel_mixer_cfg_ctl_get,
		.private_value = 0,
	};

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = (struct msm_pcm_pdata *)
		dev_get_drvdata(component->dev);
	if (pdata == NULL) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}

	pdata->pcm_device[rtd->dai_link->id] = rtd->pcm;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream != NULL) {
		session_type = SESSION_TYPE_RX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_cfg_control,
				rtd, playback_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_cfg_control,
				rtd, capture_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}
	return 0;

fail:
	pr_err("%s: failed add platform ctl, err = %d\n",
		 __func__, ret);

	return ret;
}

static int msm_pcm_channel_mixer_weight_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid range: 0 to 0x4000(Unity) gain weightage */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x4000;
	return 0;
}

static int msm_pcm_add_channel_mixer_weight_controls(
		struct snd_soc_pcm_runtime *rtd,
		int channel)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix		= "ChMixer Weight Ch";
	int session_type = 0, ret = 0;
	struct snd_kcontrol_new channel_mixer_weight_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_channel_mixer_weight_info,
		.put = msm_pcm_channel_mixer_weight_ctl_put,
		.get = msm_pcm_channel_mixer_weight_ctl_get,
		.private_value = 0,
	};

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream != NULL) {
		session_type = SESSION_TYPE_RX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_weight_control,
				rtd, playback_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(&channel_mixer_weight_control,
				rtd, capture_mixer_ctl_name, suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}
	return 0;

fail:
	pr_err("%s: failed add platform ctl, err = %d\n",
		 __func__, ret);

	return ret;
}

static int msm_pcm_add_channel_mixer_controls(struct snd_soc_pcm_runtime *rtd)
{
	int i, ret = 0;
	struct snd_pcm *pcm = NULL;
	struct msm_pcm_pdata *pdata = NULL;
	struct snd_soc_component *component = NULL;

	if (!rtd || !rtd->pcm) {
		pr_err("%s invalid rtd or pcm\n", __func__);
		return -EINVAL;
	}
	pcm = rtd->pcm;

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_pcm_pdata *)
				dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream &&
		!pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_RX]) {
		pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_RX] =
			kzalloc(sizeof(struct msm_pcm_channel_mixer), GFP_KERNEL);
		if (!pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_RX]) {
			ret = -ENOMEM;
			goto fail;
		}
	}

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream &&
		!pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_TX]) {
		pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_TX] =
			kzalloc(sizeof(struct msm_pcm_channel_mixer), GFP_KERNEL);
		if (!pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_TX]) {
			ret = -ENOMEM;
			goto fail;
		}
	}

	ret = msm_pcm_add_channel_mixer_cfg_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add channel mixer cfg controls failed:%d\n",
				__func__, ret);
		goto fail;
	}
	ret = msm_pcm_add_channel_mixer_input_map_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add channel mixer input map controls failed:%d\n",
				__func__, ret);
		goto fail;
	}
	ret = msm_pcm_add_channel_mixer_output_map_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add channel mixer output map controls failed:%d\n",
				__func__, ret);
		goto fail;
	}

	for (i = 1; i <= PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++) {
		ret =  msm_pcm_add_channel_mixer_weight_controls(rtd, i);
		if (ret) {
			pr_err("%s: pcm add channel mixer weight controls failed:%d\n",
					__func__, ret);
			goto fail;
		}
	}
	return 0;

fail:
	kfree(pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_RX]);
	kfree(pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_TX]);
	pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_RX] = NULL;
	pdata->chmixer_pspd[rtd->dai_link->id][SESSION_TYPE_TX] = NULL;

	return ret;
}

static int msm_loopback_adsp_stream_cmd_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int ret = 0;
	struct msm_adsp_event_data *event_data = NULL;
	uint64_t actual_payload_len = 0;
	struct audio_client *audio_client = NULL;
	struct msm_pcm_routing_fdai_data fe_dai;
	unsigned long fe_id = kcontrol->private_value;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received invalid fe_id %lu\n", __func__, fe_id);
			ret = -EINVAL;
			goto done;
	}

	msm_pcm_routing_get_fedai_info(fe_id, SESSION_TYPE_RX, &fe_dai);
	audio_client = q6asm_get_audio_client(fe_dai.strm_id);

	event_data = (struct msm_adsp_event_data *)ucontrol->value.bytes.data;
	if ((event_data->event_type < ADSP_STREAM_PP_EVENT) ||
	    (event_data->event_type >= ADSP_STREAM_EVENT_MAX)) {
		pr_err("%s: invalid event_type=%d\n",
			__func__, event_data->event_type);
		ret = -EINVAL;
		goto done;
	}

	actual_payload_len = sizeof(struct msm_adsp_event_data) +
							event_data->payload_len;
	if (actual_payload_len >= U32_MAX) {
		pr_err("%s payload length 0x%X  exceeds limit\n",
			__func__, event_data->payload_len);
		ret = -EINVAL;
		goto done;
	}

	if (event_data->payload_len > sizeof(ucontrol->value.bytes.data)
			- sizeof(struct msm_adsp_event_data)) {
		pr_err("%s param length=%d  exceeds limit\n",
			__func__, event_data->payload_len);
		ret = -EINVAL;
		goto done;
	}

	ret = q6asm_send_stream_cmd(audio_client, event_data);
	if (ret < 0)
		pr_err("%s: failed to send stream event cmd, err = %d\n",
			__func__, ret);
done:

	return ret;
}

static int msm_pcm_add_audio_adsp_stream_cmd_control(
			struct snd_soc_pcm_runtime *rtd)
{
	const char *mixer_ctl_name = DSP_STREAM_CMD;
	const char *deviceNo = "NN";
	char *mixer_str = NULL;
	int ctl_len = 0, ret = 0;
	struct snd_soc_component *component = NULL;

	struct snd_kcontrol_new fe_audio_adsp_stream_cmd_config_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_adsp_stream_cmd_info,
		.put = msm_loopback_adsp_stream_cmd_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s rtd is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);
	if (!mixer_str) {
		ret = -ENOMEM;
		goto done;
	}

	snprintf(mixer_str, ctl_len, "%s %d", mixer_ctl_name, rtd->pcm->device);
	fe_audio_adsp_stream_cmd_config_control[0].name = mixer_str;
	fe_audio_adsp_stream_cmd_config_control[0].private_value =
		rtd->dai_link->id;
	pr_debug("%s Registering new mixer ctl %s\n", __func__, mixer_str);
	ret = snd_soc_add_component_controls(component,
		fe_audio_adsp_stream_cmd_config_control,
		ARRAY_SIZE(fe_audio_adsp_stream_cmd_config_control));
	if (ret < 0)
		pr_err("%s: failed add ctl %s. err = %d\n",
			__func__, mixer_str, ret);

	kfree(mixer_str);
done:
	return ret;
}

static int msm_pcm_add_audio_adsp_stream_callback_control(
			struct snd_soc_pcm_runtime *rtd)
{
	const char *mixer_ctl_name = DSP_STREAM_CALLBACK;
	const char *deviceNo = "NN";
	char *mixer_str = NULL;
	int ctl_len = 0, ret = 0;
	struct snd_kcontrol *kctl;
	struct snd_soc_component *component = NULL;

	struct snd_kcontrol_new fe_audio_adsp_callback_config_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_adsp_stream_callback_info,
		.get = msm_adsp_stream_callback_get,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: added new pcm FE with name %s, id %d, device no %d\n",
		 __func__, rtd->dai_link->name, rtd->dai_link->id,  rtd->pcm->device);
	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);
	if (!mixer_str) {
		ret = -ENOMEM;
		goto done;
	}

	snprintf(mixer_str, ctl_len, "%s %d", mixer_ctl_name, rtd->pcm->device);
	fe_audio_adsp_callback_config_control[0].name = mixer_str;
	fe_audio_adsp_callback_config_control[0].private_value =
		rtd->dai_link->id;
	pr_debug("%s: Registering new mixer ctl %s\n", __func__, mixer_str);
	ret = snd_soc_add_component_controls(component,
			fe_audio_adsp_callback_config_control,
			ARRAY_SIZE(fe_audio_adsp_callback_config_control));
	if (ret < 0) {
		pr_err("%s: failed to add ctl %s. err = %d\n",
			__func__, mixer_str, ret);
		ret = -EINVAL;
		goto free_mixer_str;
	}

	kctl = snd_soc_card_get_kcontrol(rtd->card, mixer_str);
	if (!kctl) {
		pr_err("%s: failed to get kctl %s.\n", __func__, mixer_str);
		ret = -EINVAL;
		goto free_mixer_str;
	}

	kctl->private_data = NULL;

free_mixer_str:
	kfree(mixer_str);
done:
	return ret;
}

static int msm_pcm_add_controls(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	ret = msm_pcm_add_volume_controls(rtd);
	if (ret)
		pr_err("%s: pcm add volume controls failed:%d\n",
			__func__, ret);
	ret = msm_pcm_add_app_type_controls(rtd);
	if (ret)
		pr_err("%s: pcm add app type controls failed:%d\n",
			__func__, ret);

	ret = msm_pcm_add_channel_mixer_controls(rtd);
	if (ret)
		pr_err("%s: pcm add channel mixer controls failed:%d\n",
			__func__, ret);

	ret = msm_pcm_add_audio_adsp_stream_cmd_control(rtd);
	if (ret)
		pr_err("%s: Could not add pcm ADSP Stream Cmd Control\n",
			__func__);

	ret = msm_pcm_add_audio_adsp_stream_callback_control(rtd);
	if (ret)
		pr_err("%s: Could not add pcm ADSP Stream Callback Control\n",
			__func__);
	return ret;
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

static struct snd_soc_component_driver msm_soc_component = {
	.name		= DRV_NAME,
	.ops            = &msm_pcm_ops,
	.pcm_new        = msm_asoc_pcm_new,
	.probe          = msm_pcm_loopback_probe,
};

static int msm_pcm_probe(struct platform_device *pdev)
{
	struct msm_pcm_pdata *pdata;

	dev_dbg(&pdev->dev, "%s: dev name %s\n",
		__func__, dev_name(&pdev->dev));

	pdata = kzalloc(sizeof(struct msm_pcm_pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	if (of_property_read_bool(pdev->dev.of_node,
				"qcom,msm-pcm-loopback-low-latency"))
		pdata->perf_mode = LOW_LATENCY_PCM_MODE;
	else
		pdata->perf_mode = LEGACY_PCM_MODE;

	dev_set_drvdata(&pdev->dev, pdata);

	return snd_soc_register_component(&pdev->dev,
				&msm_soc_component,
				NULL, 0);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	struct msm_pcm_pdata *pdata;
	int i = 0;

	pdata = dev_get_drvdata(&pdev->dev);
	if (pdata) {
		for (i = 0; i < MSM_FRONTEND_DAI_MM_SIZE; i++) {
			kfree(pdata->chmixer_pspd[i][SESSION_TYPE_RX]);
			kfree(pdata->chmixer_pspd[i][SESSION_TYPE_TX]);
		}
	}
	kfree(pdata);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_pcm_loopback_dt_match[] = {
	{.compatible = "qcom,msm-pcm-loopback"},
	{}
};

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-loopback",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_loopback_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_pcm_probe,
	.remove = msm_pcm_remove,
};

int __init msm_pcm_loopback_init(void)
{
	return platform_driver_register(&msm_pcm_driver);
}

void msm_pcm_loopback_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver);
}

MODULE_DESCRIPTION("PCM loopback platform driver");
MODULE_LICENSE("GPL v2");
