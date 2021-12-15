// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */


#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/timer.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <audio/linux/msm_audio.h>

#include <linux/of_device.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>
#include <audio/sound/devdep_params.h>
#include <dsp/msm_audio_ion.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6core.h>
#include <dsp/q6asm-v2.h>
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
#include <soc/qcom/boot_stats.h>
#endif

#include "msm-pcm-q6-v2.h"
#include "msm-pcm-routing-v2.h"
#include "msm-qti-pp-config.h"

#define DRV_NAME "msm-pcm-q6-v2"
#define TIMEOUT_MS	1000

enum stream_state {
	IDLE = 0,
	STOPPED,
	RUNNING,
};

static struct audio_locks the_locks;

#define PCM_MASTER_VOL_MAX_STEPS	0x2000
static const DECLARE_TLV_DB_LINEAR(msm_pcm_vol_gain, 0,
			PCM_MASTER_VOL_MAX_STEPS);

struct snd_msm {
	struct snd_card *card;
	struct snd_pcm *pcm;
};

struct msm_pcm_channel_map {
	bool set_channel_map;
	char channel_map[PCM_FORMAT_MAX_NUM_CHANNEL_V8];
};

#define CMD_EOS_MIN_TIMEOUT_LENGTH  50
#define CMD_EOS_TIMEOUT_MULTIPLIER  (HZ * 50)
#define MAX_PB_COPY_RETRIES         3

static struct snd_pcm_hardware msm_pcm_hardware_capture = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE |
				SNDRV_PCM_FMTBIT_S32_LE),
	.rates =                SNDRV_PCM_RATE_8000_384000,
	.rate_min =             8000,
	.rate_max =             384000,
	.channels_min =         1,
	.channels_max =         4,
	.buffer_bytes_max =     CAPTURE_MAX_NUM_PERIODS *
				CAPTURE_MAX_PERIOD_SIZE,
	.period_bytes_min =	CAPTURE_MIN_PERIOD_SIZE,
	.period_bytes_max =     CAPTURE_MAX_PERIOD_SIZE,
	.periods_min =          CAPTURE_MIN_NUM_PERIODS,
	.periods_max =          CAPTURE_MAX_NUM_PERIODS,
	.fifo_size =            0,
};

static struct snd_pcm_hardware msm_pcm_hardware_playback = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE |
				SNDRV_PCM_FMTBIT_S32_LE),
	.rates =                SNDRV_PCM_RATE_8000_384000,
	.rate_min =             8000,
	.rate_max =             384000,
	.channels_min =         1,
	.channels_max =         8,
	.buffer_bytes_max =     PLAYBACK_MAX_NUM_PERIODS *
				PLAYBACK_MAX_PERIOD_SIZE,
	.period_bytes_min =	PLAYBACK_MIN_PERIOD_SIZE,
	.period_bytes_max =     PLAYBACK_MAX_PERIOD_SIZE,
	.periods_min =          PLAYBACK_MIN_NUM_PERIODS,
	.periods_max =          PLAYBACK_MAX_NUM_PERIODS,
	.fifo_size =            0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	88200, 96000, 176400, 192000, 352800, 384000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

struct msm_pcm_channel_map *chmap_pspd[MSM_FRONTEND_DAI_MM_SIZE][2];

static void msm_pcm_route_event_handler(enum msm_pcm_routing_event event,
					void *priv_data)
{
	struct msm_audio *prtd = priv_data;

	WARN_ON(!prtd);

	pr_debug("%s: event %x\n", __func__, event);

	switch (event) {
	case MSM_PCM_RT_EVT_BUF_RECFG:
		q6asm_cmd(prtd->audio_client, CMD_PAUSE);
		q6asm_cmd(prtd->audio_client, CMD_FLUSH);
		q6asm_run(prtd->audio_client, 0, 0, 0);
		/* fallthrough */
	default:
		break;
	}
}

static void event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct msm_audio *prtd = priv;
	struct snd_pcm_substream *substream = prtd->substream;
	uint32_t *ptrmem = (uint32_t *)payload;
	uint32_t idx = 0;
	uint32_t size = 0;
	uint8_t buf_index;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;

	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2: {
		pr_debug("ASM_DATA_EVENT_WRITE_DONE_V2\n");
		pr_debug("Buffer Consumed = 0x%08x\n", *ptrmem);
		prtd->pcm_irq_pos += prtd->pcm_count;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		atomic_inc(&prtd->out_count);
		wake_up(&the_locks.write_wait);
		if (!atomic_read(&prtd->start))
			break;
		if (!prtd->mmap_flag || prtd->reset_event)
			break;
		if (q6asm_is_cpu_buf_avail_nolock(IN,
				prtd->audio_client,
				&size, &idx)) {
			pr_debug("%s:writing %d bytes of buffer to dsp 2\n",
					__func__, prtd->pcm_count);
			q6asm_write_nolock(prtd->audio_client,
				prtd->pcm_count, 0, 0, NO_TIMESTAMP);
		}
		break;
	}
	case ASM_DATA_EVENT_RENDERED_EOS:
	case ASM_DATA_EVENT_RENDERED_EOS_V2:
		pr_debug("ASM_DATA_EVENT_RENDERED_EOS\n");
		clear_bit(CMD_EOS, &prtd->cmd_pending);
		wake_up(&the_locks.eos_wait);
		break;
	case ASM_DATA_EVENT_READ_DONE_V2: {
		pr_debug("ASM_DATA_EVENT_READ_DONE_V2\n");
		buf_index = q6asm_get_buf_index_from_token(token);
		if (buf_index >= CAPTURE_MAX_NUM_PERIODS) {
			pr_err("%s: buffer index %u is out of range.\n",
				__func__, buf_index);
			return;
		}
		pr_debug("%s: token=0x%08x buf_index=0x%08x\n",
			 __func__, token, buf_index);
		prtd->in_frame_info[buf_index].size = payload[4];
		prtd->in_frame_info[buf_index].offset = payload[5];
		/* assume data size = 0 during flushing */
		if (prtd->in_frame_info[buf_index].size) {
			if ((int)substream->runtime->control->appl_ptr == 0 && prtd->in_frame_info[buf_index].size < prtd->pcm_count) {
				pr_debug("%s:skip first buffer until get full buffer size=%d: prtd->pcm_count=%d\n",
						__func__, prtd->in_frame_info[buf_index].size, prtd->pcm_count);
				memset(&prtd->in_frame_info[buf_index], 0,
						sizeof(struct msm_audio_in_frame_info));
				if (q6asm_is_cpu_buf_avail_nolock(OUT, prtd->audio_client,&size, &idx) &&
						(substream->runtime->status->state == SNDRV_PCM_STATE_RUNNING)) {
					ret = q6asm_read_nolock(prtd->audio_client);
					if (ret < 0) {
						pr_err("%s:q6asm read failed\n",__func__);
						ret = -EFAULT;
						q6asm_cpu_buf_release_nolock(OUT, prtd->audio_client);
					}
				}
			return;
			}

			prtd->pcm_irq_pos +=
				prtd->in_frame_info[buf_index].size;
			pr_debug("pcm_irq_pos=%d\n", prtd->pcm_irq_pos);
			if (atomic_read(&prtd->start))
				snd_pcm_period_elapsed(substream);
			if (atomic_read(&prtd->in_count) <= prtd->periods)
				atomic_inc(&prtd->in_count);
			wake_up(&the_locks.read_wait);
			if (prtd->mmap_flag &&
			    q6asm_is_cpu_buf_avail_nolock(OUT,
				prtd->audio_client,
				&size, &idx) &&
			    (substream->runtime->status->state ==
			     SNDRV_PCM_STATE_RUNNING))
				q6asm_read_nolock(prtd->audio_client);
		} else {
			pr_debug("%s: reclaim flushed buf in_count %x\n",
				__func__, atomic_read(&prtd->in_count));
			prtd->pcm_irq_pos += prtd->pcm_count;
			if (prtd->mmap_flag) {
				if (q6asm_is_cpu_buf_avail_nolock(OUT,
				    prtd->audio_client,
				    &size, &idx) &&
				    (substream->runtime->status->state ==
				    SNDRV_PCM_STATE_RUNNING))
					q6asm_read_nolock(prtd->audio_client);
			} else {
				atomic_inc(&prtd->in_count);
			}
			if (atomic_read(&prtd->in_count) == prtd->periods) {
				pr_info("%s: reclaimed all bufs\n", __func__);
				if (atomic_read(&prtd->start))
					snd_pcm_period_elapsed(substream);
				wake_up(&the_locks.read_wait);
			}
		}
		break;
	}
	case ASM_STREAM_PP_EVENT:
	case ASM_STREAM_CMD_ENCDEC_EVENTS: {
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
		case ASM_SESSION_CMD_RUN_V2:
			if (substream->stream
				!= SNDRV_PCM_STREAM_PLAYBACK) {
				atomic_set(&prtd->start, 1);
				break;
			}
			if (prtd->mmap_flag) {
				int cnt = prtd->pcm_size / prtd->pcm_count;

				pr_debug("%s %d:buffer %d, period %d, %d writes\n",
					__func__, __LINE__,
					prtd->pcm_size, prtd->pcm_count, cnt);
				while (cnt--) {
					pr_debug("%s %d:writing %d bytes of buffer to dsp\n",
						__func__, __LINE__,
						prtd->pcm_count);
					q6asm_write_nolock(prtd->audio_client,
						prtd->pcm_count,
						0, 0, NO_TIMESTAMP);
				}
			} else {
				while (atomic_read(&prtd->out_needed)) {
					pr_debug("%s:writing %d bytes of buffer to dsp\n",
						__func__,
						prtd->pcm_count);
					q6asm_write_nolock(prtd->audio_client,
						prtd->pcm_count,
						0, 0, NO_TIMESTAMP);
					atomic_dec(&prtd->out_needed);
					wake_up(&the_locks.write_wait);
				};
			}
			atomic_set(&prtd->start, 1);
			break;
		case ASM_STREAM_CMD_REGISTER_PP_EVENTS:
			pr_debug("%s: ASM_STREAM_CMD_REGISTER_PP_EVENTS:",
				__func__);
			break;
		default:
			pr_debug("%s:Payload = [0x%x]stat[0x%x]\n",
				__func__, payload[0], payload[1]);
			break;
		}
	}
	break;
	case RESET_EVENTS:
		pr_debug("%s RESET_EVENTS\n", __func__);
		prtd->pcm_irq_pos += prtd->pcm_count;
		atomic_inc(&prtd->out_count);
		atomic_inc(&prtd->in_count);
		prtd->reset_event = true;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		wake_up(&the_locks.eos_wait);
		wake_up(&the_locks.write_wait);
		wake_up(&the_locks.read_wait);
		break;
	default:
		pr_debug("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
}

static struct msm_pcm_channel_map *msm_pcm_get_chmap(u64 fe_id,
			int session_type)
{
	if (fe_id >= MSM_FRONTEND_DAI_MM_SIZE) {
		pr_err("%s: invalid FE %llu\n", __func__, fe_id);
		return NULL;
	}

	if ((session_type != SESSION_TYPE_TX) &&
		(session_type != SESSION_TYPE_RX)) {
		pr_err("%s: invalid session type %d\n", __func__, session_type);
		return NULL;
	}

	return chmap_pspd[fe_id][session_type];
}

static int msm_pcm_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	struct msm_audio *prtd = runtime->private_data;
	struct msm_plat_data *pdata;
	struct snd_pcm_hw_params *params;
	struct msm_pcm_channel_map *chmap;
	int ret;
	uint32_t fmt_type = FORMAT_LINEAR_PCM;
	uint16_t bits_per_sample;
	uint16_t sample_word_size;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}
	if (!prtd || !prtd->audio_client) {
		pr_err("%s: private data null or audio client freed\n",
			__func__);
		return -EINVAL;
	}
	params = &soc_prtd->dpcm[substream->stream].hw_params;

	chmap = msm_pcm_get_chmap(soc_prtd->dai_link->id, SESSION_TYPE_RX);
	if (!chmap) {
		pr_err("%s: invalid chmap handle\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s\n", __func__);
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;
	if (prtd->enabled)
		return 0;

	prtd->audio_client->perf_mode = pdata->perf_mode;
	pr_debug("%s: perf: %x\n", __func__, pdata->perf_mode);

	prtd->audio_client->stream_type = SNDRV_PCM_STREAM_PLAYBACK;
	prtd->audio_client->fedai_id = soc_prtd->dai_link->id;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S32_LE:
		bits_per_sample = 32;
		sample_word_size = 32;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bits_per_sample = 24;
		sample_word_size = 32;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		bits_per_sample = 24;
		sample_word_size = 24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bits_per_sample = 16;
		sample_word_size = 16;
		break;
	}
	if (prtd->compress_enable) {
		fmt_type = FORMAT_GEN_COMPR;
		pr_debug("%s: Compressed enabled!\n", __func__);
		ret = q6asm_open_write_compressed(prtd->audio_client, fmt_type,
				COMPRESSED_PASSTHROUGH_GEN);
		if (ret < 0) {
			pr_err("%s: q6asm_open_write_compressed failed (%d)\n",
			__func__, ret);
			q6asm_audio_client_free(prtd->audio_client);
			prtd->audio_client = NULL;
			return -ENOMEM;
		}
	} else {
		ret = q6asm_open_write_with_retry(prtd->audio_client,
				fmt_type, bits_per_sample);
		if (ret < 0) {
			pr_err("%s: q6asm_open_write failed (%d)\n",
			__func__, ret);
			q6asm_audio_client_free(prtd->audio_client);
			prtd->audio_client = NULL;
			return -ENOMEM;
		}

		ret = q6asm_send_cal(prtd->audio_client);
		if (ret < 0)
			pr_debug("%s : Send cal failed : %d", __func__, ret);
	}
	pr_debug("%s: session ID %d\n", __func__,
			prtd->audio_client->session);
	prtd->session_id = prtd->audio_client->session;

	if (prtd->compress_enable) {
		ret = msm_pcm_routing_reg_phy_compr_stream(
				soc_prtd->dai_link->id,
				prtd->audio_client->perf_mode,
				prtd->session_id,
				SNDRV_PCM_STREAM_PLAYBACK,
				COMPRESSED_PASSTHROUGH_GEN);
	} else {
		ret = msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->id,
			prtd->audio_client->perf_mode,
			prtd->session_id, substream->stream);
	}
	if (ret) {
		pr_err("%s: stream reg failed ret:%d\n", __func__, ret);
		return ret;
	}
	if (prtd->compress_enable) {
		ret = q6asm_media_format_block_gen_compr(
			prtd->audio_client, runtime->rate,
			runtime->channels, !prtd->set_channel_map,
			prtd->channel_map, bits_per_sample);
	} else {

		if ((q6core_get_avcs_api_version_per_service(
				APRV2_IDS_SERVICE_ID_ADSP_ASM_V) >=
				ADSP_ASM_API_VERSION_V2)) {

			ret = q6asm_media_format_block_multi_ch_pcm_v5(
				prtd->audio_client, runtime->rate,
				runtime->channels, !chmap->set_channel_map,
				chmap->channel_map, bits_per_sample,
				sample_word_size, ASM_LITTLE_ENDIAN,
				DEFAULT_QF);
		} else {
			ret = q6asm_media_format_block_multi_ch_pcm_v4(
				prtd->audio_client, runtime->rate,
				runtime->channels, !prtd->set_channel_map,
				prtd->channel_map, bits_per_sample,
				sample_word_size, ASM_LITTLE_ENDIAN,
				DEFAULT_QF);
		}
	}
	if (ret < 0)
		pr_err("%s: CMD Format block failed\n", __func__);

	atomic_set(&prtd->out_count, runtime->periods);

	prtd->enabled = 1;
	prtd->cmd_pending = 0;
	prtd->cmd_interrupt = 0;

	return 0;
}

static int msm_pcm_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	struct msm_plat_data *pdata;
	struct snd_pcm_hw_params *params;
	struct msm_pcm_routing_evt event;
	struct msm_pcm_channel_map *chmap;
	int ret = 0;
	int i = 0;
	uint16_t bits_per_sample = 16;
	uint16_t sample_word_size;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}
	if (!prtd || !prtd->audio_client) {
		pr_err("%s: private data null or audio client freed\n",
			__func__);
		return -EINVAL;
	}

	chmap = msm_pcm_get_chmap(soc_prtd->dai_link->id, SESSION_TYPE_TX);
	if (!chmap) {
		pr_err("%s: invalid chmap handle\n", __func__);
		return -EINVAL;
	}

	if (prtd->enabled == IDLE) {
		pr_debug("%s:perf_mode=%d periods=%d\n", __func__,
			pdata->perf_mode, runtime->periods);
		params = &soc_prtd->dpcm[substream->stream].hw_params;
		if ((params_format(params) == SNDRV_PCM_FORMAT_S24_LE) ||
			(params_format(params) == SNDRV_PCM_FORMAT_S24_3LE))
			bits_per_sample = 24;
		else if (params_format(params) == SNDRV_PCM_FORMAT_S32_LE)
			bits_per_sample = 32;

		/* ULL mode is not supported in capture path */
		if (pdata->perf_mode == LEGACY_PCM_MODE)
			prtd->audio_client->perf_mode = LEGACY_PCM_MODE;
		else
			prtd->audio_client->perf_mode = LOW_LATENCY_PCM_MODE;

		pr_debug("%s Opening %d-ch PCM read stream, perf_mode %d\n",
				__func__, params_channels(params),
				prtd->audio_client->perf_mode);

		prtd->audio_client->stream_type = SNDRV_PCM_STREAM_CAPTURE;
		prtd->audio_client->fedai_id = soc_prtd->dai_link->id;

		ret = q6asm_open_read_with_retry(prtd->audio_client,
					FORMAT_LINEAR_PCM,
					bits_per_sample, false);
		if (ret < 0) {
			pr_err("%s: q6asm_open_read failed\n", __func__);
			q6asm_audio_client_free(prtd->audio_client);
			prtd->audio_client = NULL;
			return -ENOMEM;
		}

		ret = q6asm_send_cal(prtd->audio_client);
		if (ret < 0)
			pr_debug("%s : Send cal failed : %d", __func__, ret);

		pr_debug("%s: session ID %d\n",
				__func__, prtd->audio_client->session);
		prtd->session_id = prtd->audio_client->session;
		event.event_func = msm_pcm_route_event_handler;
		event.priv_data = (void *) prtd;
		ret = msm_pcm_routing_reg_phy_stream_v2(
				soc_prtd->dai_link->id,
				prtd->audio_client->perf_mode,
				prtd->session_id, substream->stream,
				event);
		if (ret) {
			pr_err("%s: stream reg failed ret:%d\n", __func__, ret);
			return ret;
		}
	}

	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;

	if (prtd->enabled == IDLE || prtd->enabled == STOPPED) {
		for (i = 0; i < runtime->periods; i++)
			q6asm_read(prtd->audio_client);
		prtd->periods = runtime->periods;
	}

	if (prtd->enabled != IDLE)
		return 0;

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S32_LE:
		bits_per_sample = 32;
		sample_word_size = 32;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bits_per_sample = 24;
		sample_word_size = 32;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		bits_per_sample = 24;
		sample_word_size = 24;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bits_per_sample = 16;
		sample_word_size = 16;
		break;
	}

	pr_debug("%s: Samp_rate = %d Channel = %d bit width = %d, word size = %d\n",
			__func__, prtd->samp_rate, prtd->channel_mode,
			bits_per_sample, sample_word_size);

	if ((q6core_get_avcs_api_version_per_service(
			APRV2_IDS_SERVICE_ID_ADSP_ASM_V) >=
			ADSP_ASM_API_VERSION_V2))
		ret = q6asm_enc_cfg_blk_pcm_format_support_v5(
						prtd->audio_client,
						prtd->samp_rate,
						prtd->channel_mode,
						!chmap->set_channel_map,
						chmap->channel_map,
						bits_per_sample,
						sample_word_size,
						ASM_LITTLE_ENDIAN,
						DEFAULT_QF);
	else
		ret = q6asm_enc_cfg_blk_pcm_format_support_v4(
						prtd->audio_client,
						prtd->samp_rate,
						prtd->channel_mode,
						bits_per_sample,
						sample_word_size,
						ASM_LITTLE_ENDIAN,
						DEFAULT_QF);

	if (ret < 0)
		pr_debug("%s: cmd cfg pcm was block failed", __func__);

	prtd->enabled = RUNNING;

	return ret;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	static int first_time = 1;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (first_time) {
#ifdef CONFIG_MSM_BOOT_TIME_MARKER
			place_marker("K - Early chime");
#endif
			first_time = 0;
		}
		pr_debug("%s: Trigger start\n", __func__);
		ret = q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		atomic_set(&prtd->start, 0);
		if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
			prtd->enabled = STOPPED;
			ret = q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
			break;
		}
		/* pending CMD_EOS isn't expected */
		WARN_ON_ONCE(test_bit(CMD_EOS, &prtd->cmd_pending));
		set_bit(CMD_EOS, &prtd->cmd_pending);
		ret = q6asm_cmd_nowait(prtd->audio_client, CMD_EOS);
		if (ret)
			clear_bit(CMD_EOS, &prtd->cmd_pending);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE\n");
		ret = q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
		atomic_set(&prtd->start, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	struct msm_audio *prtd;
	struct msm_plat_data *pdata;
	enum apr_subsys_state subsys_state;
	int ret = 0;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}

	subsys_state = apr_get_subsys_state();
	if (subsys_state == APR_SUBSYS_DOWN) {
		pr_debug("%s: adsp is down\n", __func__);
		return -ENETRESET;
	}

	prtd = kzalloc(sizeof(struct msm_audio), GFP_KERNEL);
	if (prtd == NULL)
		return -ENOMEM;

	prtd->substream = substream;
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)event_handler, prtd);
	if (!prtd->audio_client) {
		pr_info("%s: Could not allocate memory\n", __func__);
		kfree(prtd);
		prtd = NULL;
		return -ENOMEM;
	}

	prtd->audio_client->dev = component->dev;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		runtime->hw = msm_pcm_hardware_playback;

	/* Capture path */
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		runtime->hw = msm_pcm_hardware_capture;
	else {
		pr_err("Invalid Stream type %d\n", substream->stream);
		return -EINVAL;
	}

	ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				&constraints_sample_rates);
	if (ret < 0)
		pr_info("snd_pcm_hw_constraint_list failed\n");
	/* Ensure that buffer size is a multiple of period size */
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		pr_info("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
			PLAYBACK_MIN_NUM_PERIODS * PLAYBACK_MIN_PERIOD_SIZE,
			PLAYBACK_MAX_NUM_PERIODS * PLAYBACK_MAX_PERIOD_SIZE);
		if (ret < 0) {
			pr_err("constraint for buffer bytes min max ret = %d\n",
									ret);
		}
	}

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ret = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
			CAPTURE_MIN_NUM_PERIODS * CAPTURE_MIN_PERIOD_SIZE,
			CAPTURE_MAX_NUM_PERIODS * CAPTURE_MAX_PERIOD_SIZE);
		if (ret < 0) {
			pr_err("constraint for buffer bytes min max ret = %d\n",
									ret);
		}
	}
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret < 0) {
		pr_err("constraint for period bytes step ret = %d\n",
								ret);
	}
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret < 0) {
		pr_err("constraint for buffer bytes step ret = %d\n",
								ret);
	}

	prtd->enabled = IDLE;
	prtd->dsp_cnt = 0;
	prtd->set_channel_map = false;
	prtd->reset_event = false;
	runtime->private_data = prtd;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		msm_adsp_init_mixer_ctl_pp_event_queue(soc_prtd);

	/* Vote to update the Rx thread priority to RT Thread for playback */
	if ((substream->stream == SNDRV_PCM_STREAM_PLAYBACK) &&
	    (pdata->perf_mode == LOW_LATENCY_PCM_MODE))
		apr_start_rx_rt(prtd->audio_client->apr);

	return 0;
}

static int msm_pcm_playback_copy(struct snd_pcm_substream *substream, int a,
	unsigned long hwoff, void __user *buf, unsigned long fbytes)
{
	int ret = 0;
	int xfer = 0;
	char *bufptr = NULL;
	void *data = NULL;
	uint32_t idx = 0;
	uint32_t size = 0;
	uint32_t retries = 0;

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	pr_debug("%s: prtd->out_count = %d\n",
				__func__, atomic_read(&prtd->out_count));

	while ((fbytes > 0) && (retries < MAX_PB_COPY_RETRIES)) {
		if (prtd->reset_event) {
			pr_err("%s: In SSR return ENETRESET before wait\n",
				__func__);
			return -ENETRESET;
		}

		ret = wait_event_timeout(the_locks.write_wait,
				(atomic_read(&prtd->out_count)),
				msecs_to_jiffies(TIMEOUT_MS));
		if (!ret) {
			pr_err("%s: wait_event_timeout failed\n", __func__);
			ret = -ETIMEDOUT;
			goto fail;
		}
		ret = 0;

		if (prtd->reset_event) {
			pr_err("%s: In SSR return ENETRESET after wait\n",
				__func__);
			return -ENETRESET;
		}

		if (!atomic_read(&prtd->out_count)) {
			pr_err("%s: pcm stopped out_count 0\n", __func__);
			return 0;
		}

		data = q6asm_is_cpu_buf_avail(IN, prtd->audio_client, &size,
			&idx);
		if (data == NULL) {
			retries++;
			continue;
		} else {
			retries = 0;
		}

		if (fbytes > size)
			xfer = size;
		else
			xfer = fbytes;

		bufptr = data;
		if (bufptr) {
			pr_debug("%s:fbytes =%lu: xfer=%d size=%d\n",
				 __func__, fbytes, xfer, size);
			if (copy_from_user(bufptr, buf, xfer)) {
				ret = -EFAULT;
				pr_err("%s: copy_from_user failed\n",
					__func__);
				q6asm_cpu_buf_release(IN, prtd->audio_client);
				goto fail;
			}
			buf += xfer;
			fbytes -= xfer;
			pr_debug("%s:fbytes = %lu: xfer=%d\n", __func__,
				 fbytes, xfer);
			if (atomic_read(&prtd->start)) {
				pr_debug("%s:writing %d bytes of buffer to dsp\n",
						__func__, xfer);
				ret = q6asm_write(prtd->audio_client, xfer,
							0, 0, NO_TIMESTAMP);
				if (ret < 0) {
					ret = -EFAULT;
					q6asm_cpu_buf_release(IN,
						prtd->audio_client);
					goto fail;
				}
			} else
				atomic_inc(&prtd->out_needed);
			atomic_dec(&prtd->out_count);
		}
	}
fail:
	if (retries >= MAX_PB_COPY_RETRIES)
		ret = -ENOMEM;

	return  ret;
}

static int msm_pcm_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	struct msm_plat_data *pdata;
	uint32_t timeout;
	int dir = 0;
	int ret = 0;

	pr_debug("%s: cmd_pending 0x%lx\n", __func__, prtd->cmd_pending);

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *) dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: platform data is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	if (prtd->audio_client) {
		dir = IN;

		/*
		 * Unvote to downgrade the Rx thread priority from
		 * RT Thread for Low-Latency use case.
		 */
		if (pdata) {
			if (pdata->perf_mode == LOW_LATENCY_PCM_MODE)
				apr_end_rx_rt(prtd->audio_client->apr);
		}
		/* determine timeout length */
		if (runtime->frame_bits == 0 || runtime->rate == 0) {
			timeout = CMD_EOS_MIN_TIMEOUT_LENGTH;
		} else {
			timeout = (runtime->period_size *
					CMD_EOS_TIMEOUT_MULTIPLIER) /
					((runtime->frame_bits / 8) *
					 runtime->rate);
			if (timeout < CMD_EOS_MIN_TIMEOUT_LENGTH)
				timeout = CMD_EOS_MIN_TIMEOUT_LENGTH;
		}
		pr_debug("%s: CMD_EOS timeout is %d\n", __func__, timeout);

		ret = wait_event_timeout(the_locks.eos_wait,
					 !test_bit(CMD_EOS, &prtd->cmd_pending),
					 timeout);
		if (!ret)
			pr_err("%s: CMD_EOS failed, cmd_pending 0x%lx\n",
			       __func__, prtd->cmd_pending);
		q6asm_cmd(prtd->audio_client, CMD_CLOSE);
		q6asm_audio_client_buf_free_contiguous(dir,
					prtd->audio_client);
		q6asm_audio_client_free(prtd->audio_client);
	}
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->id,
						SNDRV_PCM_STREAM_PLAYBACK);
	msm_adsp_clean_mixer_ctl_pp_event_queue(soc_prtd);
	kfree(prtd);
	runtime->private_data = NULL;
	mutex_unlock(&pdata->lock);
	return 0;
}

static int msm_pcm_capture_copy(struct snd_pcm_substream *substream,
		 int channel, unsigned long hwoff, void __user *buf,
						 unsigned long fbytes)
{
	int ret = 0;
	int xfer;
	char *bufptr;
	void *data = NULL;
	uint32_t idx = 0;
	uint32_t size = 0;
	uint32_t offset = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = substream->runtime->private_data;


	pr_debug("%s\n", __func__);

	pr_debug("appl_ptr %d\n", (int)runtime->control->appl_ptr);
	pr_debug("hw_ptr %d\n", (int)runtime->status->hw_ptr);
	pr_debug("avail_min %d\n", (int)runtime->control->avail_min);

	if (prtd->reset_event) {
		pr_err("%s: In SSR return ENETRESET before wait\n", __func__);
		return -ENETRESET;
	}
	ret = wait_event_timeout(the_locks.read_wait,
			(atomic_read(&prtd->in_count)),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_debug("%s: wait_event_timeout failed\n", __func__);
		goto fail;
	}
	if (prtd->reset_event) {
		pr_err("%s: In SSR return ENETRESET after wait\n", __func__);
		return -ENETRESET;
	}
	if (!atomic_read(&prtd->in_count)) {
		pr_debug("%s: pcm stopped in_count 0\n", __func__);
		return 0;
	}
	pr_debug("Checking if valid buffer is available...%pK\n",
						data);
	data = q6asm_is_cpu_buf_avail(OUT, prtd->audio_client, &size, &idx);
	bufptr = data;
	pr_debug("Size = %d\n", size);
	pr_debug("fbytes = %lu\n", fbytes);
	pr_debug("idx = %d\n", idx);
	pr_debug("period_size = %d\n", prtd->pcm_count);
	if (bufptr) {
		xfer = fbytes;
		if (xfer > size)
			xfer = size;
		offset = prtd->in_frame_info[idx].offset;
		pr_debug("Offset value = %d\n", offset);
		if (size == 0 || size < prtd->pcm_count) {
			memset(bufptr + offset + size, 0, prtd->pcm_count - size);
			if (fbytes > prtd->pcm_count)
				size = xfer = prtd->pcm_count;
			else
				size = xfer = fbytes;
		}

		if (copy_to_user(buf, bufptr+offset, xfer)) {
			pr_err("Failed to copy buf to user\n");
			ret = -EFAULT;
			q6asm_cpu_buf_release(OUT, prtd->audio_client);
			goto fail;
		}
		fbytes -= xfer;
		size -= xfer;
		prtd->in_frame_info[idx].offset += xfer;
		pr_debug("%s:fbytes = %lu: size=%d: xfer=%d\n",
					__func__, fbytes, size, xfer);
		pr_debug(" Sending next buffer to dsp\n");
		memset(&prtd->in_frame_info[idx], 0,
		       sizeof(struct msm_audio_in_frame_info));
		atomic_dec(&prtd->in_count);
		ret = q6asm_read(prtd->audio_client);
		if (ret < 0) {
			pr_err("q6asm read failed\n");
			ret = -EFAULT;
			q6asm_cpu_buf_release(OUT, prtd->audio_client);
			goto fail;
		}
	} else
		pr_err("No valid buffer\n");

	pr_debug("Returning from capture_copy... %d\n", ret);
fail:
	return ret;
}

static int msm_pcm_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	struct msm_plat_data *pdata;
	int dir = OUT;

	pr_debug("%s\n", __func__);

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *) dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: platform data is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	if (prtd->audio_client) {
		q6asm_cmd(prtd->audio_client, CMD_CLOSE);
		q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
		q6asm_audio_client_free(prtd->audio_client);
	}

	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->id,
		SNDRV_PCM_STREAM_CAPTURE);
	kfree(prtd);
	runtime->private_data = NULL;
	mutex_unlock(&pdata->lock);
	return 0;
}

static int msm_pcm_copy(struct snd_pcm_substream *substream, int a,
	 unsigned long hwoff, void __user *buf, unsigned long fbytes)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_copy(substream, a, hwoff, buf, fbytes);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_copy(substream, a, hwoff, buf, fbytes);
	return ret;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_close(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_close(substream);
	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_pcm_playback_prepare(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_pcm_capture_prepare(substream);
	return ret;
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	if (prtd->pcm_irq_pos >= prtd->pcm_size)
		prtd->pcm_irq_pos = 0;

	pr_debug("pcm_irq_pos = %d\n", prtd->pcm_irq_pos);
	return bytes_to_frames(runtime, (prtd->pcm_irq_pos));
}

static int msm_pcm_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_client *ac = prtd->audio_client;
	struct audio_port_data *apd = ac->port;
	struct audio_buffer *ab;
	int dir = -1;

	prtd->mmap_flag = 1;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;
	ab = &(apd[dir].buf[0]);

	return msm_audio_ion_mmap(ab, vma);
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	int dir, ret;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;
	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			(params_buffer_bytes(params) / params_periods(params)),
			 params_periods(params));
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed rc = %d\n",
							ret);
		return -ENOMEM;
	}
	buf = prtd->audio_client->port[dir].buf;
	if (buf == NULL || buf[0].data == NULL)
		return -ENOMEM;

	pr_debug("%s:buf = %pK\n", __func__, buf);
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->area = buf[0].data;
	dma_buf->addr =  buf[0].phys;
	dma_buf->bytes = params_buffer_bytes(params);
	if (!dma_buf->area)
		return -ENOMEM;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int msm_pcm_ioctl(struct snd_pcm_substream *substream,
			 unsigned int cmd, void __user *arg)
{
	struct msm_audio *prtd = NULL;
	struct snd_soc_pcm_runtime *rtd = NULL;
	uint64_t ses_time = 0, abs_time = 0;
	int64_t av_offset = 0;
	int32_t clock_id = -EINVAL;
	int rc = 0;
	struct snd_pcm_prsnt_position userarg;

	if (!substream || !substream->private_data) {
		pr_err("%s: Invalid %s\n", __func__,
			(!substream) ? "substream" : "private_data");
		return -EINVAL;
	}

	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		return -EINVAL;
	}

	prtd = substream->runtime->private_data;
	if (!prtd) {
		pr_err("%s prtd is null.\n", __func__);
		return -EINVAL;
	}

	rtd = substream->private_data;

	switch (cmd) {
	case SNDRV_PCM_IOCTL_DSP_POSITION:
		dev_dbg(rtd->dev, "%s: SNDRV_PCM_DSP_POSITION", __func__);
		if (!arg) {
			dev_err(rtd->dev, "%s: Invalid params DSP_POSITION\n",
				__func__);
			rc = -EINVAL;
			goto done;
		}
		memset(&userarg, 0, sizeof(userarg));
		if (copy_from_user(&userarg, arg, sizeof(userarg))) {
			dev_err(rtd->dev, "%s: err copyuser DSP_POSITION\n",
				__func__);
			rc = -EFAULT;
			goto done;
		}
		clock_id = userarg.clock_id;
		rc = q6asm_get_session_time_v2(prtd->audio_client, &ses_time,
					       &abs_time);
		if (rc) {
			pr_err("%s: q6asm_get_session_time_v2 failed, rc=%d\n",
				__func__, rc);
			goto done;
		}
		userarg.frames = div64_u64((ses_time * prtd->samp_rate),
					   1000000);

		rc = avcs_core_query_timer_offset(&av_offset, clock_id);
		if (rc) {
			pr_err("%s: avcs offset query failed, rc=%d\n",
				__func__, rc);
			goto done;
		}

		userarg.timestamp = abs_time + av_offset;
		if (copy_to_user(arg, &userarg, sizeof(userarg))) {
			dev_err(rtd->dev, "%s: err copy to user DSP_POSITION\n",
				__func__);
			rc = -EFAULT;
			goto done;
		}
		pr_debug("%s, vals f %lld, t %lld, avoff %lld, abst %lld, sess_time %llu sr %d\n",
			 __func__, userarg.frames, userarg.timestamp,
			 av_offset, abs_time, ses_time, prtd->samp_rate);
		break;
	default:
		rc = snd_pcm_lib_ioctl(substream, cmd, arg);
		break;
	}
done:
	return rc;
}

#ifdef CONFIG_COMPAT
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_pcm_compat_ioctl(struct snd_pcm_substream *substream,
			 unsigned int cmd, void __user *arg)
{
	return msm_pcm_ioctl(substream, cmd, arg);
}
#endif /* CONFIG_AUDIO_QGKI */
#else
#define msm_pcm_compat_ioctl NULL
#endif

static const struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.copy_user	= msm_pcm_copy,
	.hw_params	= msm_pcm_hw_params,
	.close          = msm_pcm_close,
	.ioctl          = msm_pcm_ioctl,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
	.compat_ioctl   = msm_pcm_compat_ioctl,
#endif /* CONFIG_AUDIO_QGKI */
	.prepare        = msm_pcm_prepare,
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
	.mmap		= msm_pcm_mmap,
};

static int msm_pcm_adsp_stream_cmd_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct msm_plat_data *pdata = NULL;
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;
	int ret = 0;
	struct msm_adsp_event_data *event_data = NULL;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s pdata is NULL\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&pdata->lock);
	substream = pdata->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	if (substream->ref_count <= 0) {
		pr_err_ratelimited("%s substream ref_count:%d invalid\n",
				__func__, substream->ref_count);
		ret = -EINVAL;
		goto done;
	}
	prtd = substream->runtime->private_data;
	if (prtd == NULL) {
		pr_err("%s prtd is null.\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (prtd->audio_client == NULL) {
		pr_err("%s prtd is null.\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	event_data = (struct msm_adsp_event_data *)ucontrol->value.bytes.data;
	if ((event_data->event_type < ADSP_STREAM_PP_EVENT) ||
	    (event_data->event_type >= ADSP_STREAM_EVENT_MAX)) {
		pr_err("%s: invalid event_type=%d",
			__func__, event_data->event_type);
		ret = -EINVAL;
		goto done;
	}

	if (event_data->payload_len > sizeof(ucontrol->value.bytes.data)
			- sizeof(struct msm_adsp_event_data)) {
		pr_err("%s param length=%d  exceeds limit",
			__func__, event_data->payload_len);
		ret = -EINVAL;
		goto done;
	}

	ret = q6asm_send_stream_cmd(prtd->audio_client, event_data);
	if (ret < 0)
		pr_err("%s: failed to send stream event cmd, err = %d\n",
			__func__, ret);
done:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_pcm_add_audio_adsp_stream_cmd_control(
			struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = DSP_STREAM_CMD;
	const char *deviceNo = "NN";
	char *mixer_str = NULL;
	int ctl_len = 0, ret = 0;
	struct snd_kcontrol_new fe_audio_adsp_stream_cmd_config_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_adsp_stream_cmd_info,
		.put = msm_pcm_adsp_stream_cmd_put,
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
	pr_debug("Registering new mixer ctl %s\n", mixer_str);
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
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = DSP_STREAM_CALLBACK;
	const char *deviceNo = "NN";
	char *mixer_str = NULL;
	int ctl_len = 0, ret = 0;
	struct snd_kcontrol *kctl;

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

	pr_debug("%s: added new pcm FE with name %s, id %d, cpu dai %s, device no %d\n",
		 __func__, rtd->dai_link->name, rtd->dai_link->id,
		 rtd->dai_link->cpus->dai_name, rtd->pcm->device);
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

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_pcm_set_volume(struct msm_audio *prtd, uint32_t volume)
{
	int rc = 0;

	if (prtd && prtd->audio_client) {
		pr_debug("%s: channels %d volume 0x%x\n", __func__,
				prtd->channel_mode, volume);
		rc = q6asm_set_volume(prtd->audio_client, volume);
		if (rc < 0) {
			pr_err("%s: Send Volume command failed rc=%d\n",
					__func__, rc);
		}
	}
	return rc;
}

static int msm_pcm_volume_ctl_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = NULL;
	struct snd_pcm_substream *substream = NULL;
	struct snd_soc_pcm_runtime *soc_prtd = NULL;
	struct snd_soc_component *component = NULL;
	struct msm_audio *prtd;

	pr_debug("%s\n", __func__);
	if (!vol) {
		pr_err("%s: vol is NULL\n", __func__);
		return -ENODEV;
	}

	if (!vol->pcm) {
		pr_err("%s: vol->pcm is NULL\n", __func__);
		return -ENODEV;
	}

	substream = vol->pcm->streams[vol->stream].substream;
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -ENODEV;
	}
	soc_prtd = substream->private_data;
	if (!substream->runtime || !soc_prtd) {
		pr_debug("%s substream runtime or private_data not found\n",
				 __func__);
		return 0;
	}

	component = snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *) dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: pdata not found\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&pdata->lock);
	if (substream->ref_count > 0) {
		prtd = substream->runtime->private_data;
		if (prtd)
			ucontrol->value.integer.value[0] = prtd->volume;
	}
	mutex_unlock(&pdata->lock);
	return 0;
}

static int msm_pcm_volume_ctl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = NULL;
	struct snd_pcm_substream *substream =
		vol->pcm->streams[vol->stream].substream;
	struct snd_soc_pcm_runtime *soc_prtd = NULL;
	struct snd_soc_component *component = NULL;
	struct msm_audio *prtd;
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume : 0x%x\n", __func__, volume);
	if (!substream) {
		pr_err("%s: substream not found\n", __func__);
		return -ENODEV;
	}

	soc_prtd = substream->private_data;
	if (!substream->runtime || !soc_prtd) {
		pr_err("%s: substream runtime or private_data not found\n",
				__func__);
		return 0;
	}

	component = snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *) dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s: pdata not found\n", __func__);
		return -ENODEV;
	}

	mutex_lock(&pdata->lock);
	if (substream->ref_count > 0) {
		prtd = substream->runtime->private_data;
		if (prtd) {
			rc = msm_pcm_set_volume(prtd, volume);
			prtd->volume = volume;
		}
	}
	mutex_unlock(&pdata->lock);
	return rc;
}

static int msm_pcm_add_volume_control(struct snd_soc_pcm_runtime *rtd,
				      int stream)
{
	int ret = 0;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_volume *volume_info;
	struct snd_kcontrol *kctl;

	dev_dbg(rtd->dev, "%s, volume control add\n", __func__);
	ret = snd_pcm_add_volume_ctls(pcm, stream,
			NULL, 1, rtd->dai_link->id,
			&volume_info);
	if (ret < 0) {
		pr_err("%s volume control failed ret %d\n", __func__, ret);
		return ret;
	}
	kctl = volume_info->kctl;
	kctl->put = msm_pcm_volume_ctl_put;
	kctl->get = msm_pcm_volume_ctl_get;
	kctl->tlv.p = msm_pcm_vol_gain;
	return 0;
}
#else
static int msm_pcm_add_volume_control(struct snd_soc_pcm_runtime *rtd,
				      int stream)
{
	return 0;
}
#endif /* CONFIG_AUDIO_QGKI */

static int msm_pcm_compress_ctl_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x2000;
	return 0;
}

static int msm_pcm_compress_ctl_get(struct snd_kcontrol *kcontrol,
		      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_soc_kcontrol_component(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(comp->dev);
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;

	if (!pdata) {
		pr_err("%s pdata is NULL\n", __func__);
		return -ENODEV;
	}
	substream = pdata->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -EINVAL;
	}
	if (!substream->runtime) {
		pr_debug("%s substream runtime not found\n", __func__);
		return 0;
	}
	mutex_lock(&pdata->lock);
	if (substream->ref_count > 0) {
		prtd = substream->runtime->private_data;
		if (prtd)
			ucontrol->value.integer.value[0] = prtd->compress_enable;
	}
	mutex_unlock(&pdata->lock);
	return 0;
}

static int msm_pcm_compress_ctl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_soc_component *comp = snd_soc_kcontrol_component(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(comp->dev);
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;
	int compress = ucontrol->value.integer.value[0];

	if (!pdata) {
		pr_err("%s pdata is NULL\n", __func__);
		return -ENODEV;
	}
	substream = pdata->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	pr_debug("%s: compress : 0x%x\n", __func__, compress);
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -EINVAL;
	}
	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		return 0;
	}
	mutex_lock(&pdata->lock);
	if (substream->ref_count > 0) {
		prtd = substream->runtime->private_data;
		if (prtd) {
			pr_debug("%s: setting compress flag to 0x%x\n",
			__func__, compress);
			prtd->compress_enable = compress;
		}
	}
	mutex_unlock(&pdata->lock);
	return rc;
}

static int msm_pcm_add_compress_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Playback ";
	const char *mixer_ctl_end_name = " Compress";
	const char *deviceNo = "NN";
	char *mixer_str = NULL;
	int ctl_len;
	int ret = 0;
	struct msm_plat_data *pdata;
	struct snd_kcontrol_new pcm_compress_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_compress_ctl_info,
		.get = msm_pcm_compress_ctl_get,
		.put = msm_pcm_compress_ctl_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s: NULL rtd\n", __func__);
		return -EINVAL;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ctl_len = strlen(mixer_ctl_name) + strlen(deviceNo) +
		  strlen(mixer_ctl_end_name) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);

	if (!mixer_str)
		return -ENOMEM;

	snprintf(mixer_str, ctl_len, "%s%d%s", mixer_ctl_name,
			rtd->pcm->device, mixer_ctl_end_name);

	pcm_compress_control[0].name = mixer_str;
	pcm_compress_control[0].private_value = rtd->dai_link->id;
	pr_debug("%s: Registering new mixer ctl %s\n", __func__, mixer_str);
	pdata = dev_get_drvdata(component->dev);
	if (pdata) {
		if (!pdata->pcm) {
			pdata->pcm = rtd->pcm;
			snd_soc_add_component_controls(component,
						      pcm_compress_control,
						      ARRAY_SIZE
						      (pcm_compress_control));
			pr_debug("%s: add control success plt = %pK\n",
				 __func__, component);
		}
	} else {
		pr_err("%s: NULL pdata\n", __func__);
		ret = -EINVAL;
	}
	kfree(mixer_str);
	return ret;
}

static int msm_pcm_chmap_ctl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int i;
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;
	struct snd_soc_pcm_runtime *rtd = NULL;
	struct msm_plat_data *pdata = NULL;
	struct msm_pcm_channel_mixer *chmixer_pspd = NULL;
	struct snd_soc_component *component = NULL;
	struct msm_pcm_channel_map *chmap;
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	bool reset_override_out_ch_map = false;
	bool reset_override_in_ch_map = false;

	pr_debug("%s: chmap ctl for fe_id: %d, session_type: %d\n",
			__func__, fe_id, session_type);
	substream = snd_pcm_chmap_substream(info, idx);
	if (!substream)
		return -ENODEV;

	rtd = substream->private_data;
	if (rtd) {
		component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
		if (component) {
			pdata = (struct msm_plat_data *)
						dev_get_drvdata(component->dev);
			if (!pdata) {
				pr_err("%s: pdata not found\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s: component is NULL\n", __func__);
			return -EINVAL;
		}
	}

	chmap = msm_pcm_get_chmap(fe_id, session_type);
	if (!chmap) {
		pr_err("%s: invalid chmap handle\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmap->channel_map[i] =
			ucontrol->value.integer.value[i];

	chmap->set_channel_map = true;

	if (!rtd)
		return 0;

	mutex_lock(&pdata->lock);
	if (substream->runtime && substream->ref_count <= 0) {
		pr_err_ratelimited("%s: substream ref_count:%d invalid\n",
				__func__, substream->ref_count);
		mutex_unlock(&pdata->lock);
		return -EINVAL;
	}
	prtd = substream->runtime ? substream->runtime->private_data : NULL;
	if (prtd) {
		prtd->set_channel_map = true;
			for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
				prtd->channel_map[i] =
				(char)(ucontrol->value.integer.value[i]);

		/* update chmixer_pspd chmap cached with routing driver as well */
		if (rtd && component) {
			fe_id = rtd->dai_link->id;
			chmixer_pspd = pdata ?
				pdata->chmixer_pspd[fe_id][session_type] : NULL;

			if (chmixer_pspd && chmixer_pspd->enable) {
				if (session_type == SESSION_TYPE_RX &&
					!chmixer_pspd->override_in_ch_map) {
					for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
						chmixer_pspd->in_ch_map[i] = prtd->channel_map[i];
					chmixer_pspd->override_in_ch_map = true;
					reset_override_in_ch_map = true;
				} else if (session_type == SESSION_TYPE_TX &&
							!chmixer_pspd->override_out_ch_map) {
					for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
						chmixer_pspd->out_ch_map[i] = prtd->channel_map[i];
					chmixer_pspd->override_out_ch_map = true;
					reset_override_out_ch_map = true;
				}
				msm_pcm_routing_set_channel_mixer_cfg(fe_id,
						session_type, chmixer_pspd);
				if (reset_override_out_ch_map)
					chmixer_pspd->override_out_ch_map = false;
				if (reset_override_in_ch_map)
					chmixer_pspd->override_in_ch_map = false;
			}

		}
	}
	mutex_unlock(&pdata->lock);
	return 0;
}

static int msm_pcm_chmap_ctl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int i = 0;
	struct msm_pcm_channel_map *chmap;
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;

	pr_debug("%s: chmap ctl for fe_id: %d, session_type: %d\n",
			__func__, fe_id, session_type);

	chmap = msm_pcm_get_chmap(fe_id, session_type);
	if (!chmap) {
		pr_err("%s: invalid chmap handle\n", __func__);
		return -EINVAL;
	}
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmap->channel_map[i];

	return 0;
}

static int msm_pcm_add_chmap_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_chmap *chmap_info;
	struct snd_kcontrol *kctl;
	char device_num[12];
	int i, ret = 0;

	pr_debug("%s, Channel map cntrl add\n", __func__);

	if (rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream &&
			!chmap_pspd[rtd->dai_link->id][SESSION_TYPE_RX]) {

		chmap_pspd[rtd->dai_link->id][SESSION_TYPE_RX] =
			kzalloc(sizeof(struct msm_pcm_channel_map), GFP_KERNEL);
		if (!chmap_pspd[rtd->dai_link->id][SESSION_TYPE_RX]) {
			ret = -ENOMEM;
			goto fail;
		}

		ret = snd_pcm_add_chmap_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
					     snd_pcm_std_chmaps,
					     PCM_FORMAT_MAX_NUM_CHANNEL_V8,
					     (rtd->dai_link->id) | (SESSION_TYPE_RX << 8),
					     &chmap_info);
		if (ret < 0) {
			pr_err("%s, channel map cntrl add failed\n", __func__);
			goto fail;
		}
		kctl = chmap_info->kctl;
		for (i = 0; i < kctl->count; i++)
			kctl->vd[i].access |= SNDRV_CTL_ELEM_ACCESS_WRITE;

		snprintf(device_num, sizeof(device_num), "%d", pcm->device);
		strlcat(kctl->id.name, device_num, sizeof(kctl->id.name));
		pr_debug("%s, Overwriting channel map control name to: %s\n",
			__func__, kctl->id.name);

		kctl->put = msm_pcm_chmap_ctl_put;
		kctl->get = msm_pcm_chmap_ctl_get;
	}

	if (rtd->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream &&
			!chmap_pspd[rtd->dai_link->id][SESSION_TYPE_TX]) {

		chmap_pspd[rtd->dai_link->id][SESSION_TYPE_TX] =
			kzalloc(sizeof(struct msm_pcm_channel_map), GFP_KERNEL);
		if (!chmap_pspd[rtd->dai_link->id][SESSION_TYPE_TX]) {
			ret = -ENOMEM;
			goto fail;
		}

		ret = snd_pcm_add_chmap_ctls(pcm, SNDRV_PCM_STREAM_CAPTURE,
					     snd_pcm_std_chmaps,
					     PCM_FORMAT_MAX_NUM_CHANNEL_V8,
					     (rtd->dai_link->id) | (SESSION_TYPE_TX << 8),
					     &chmap_info);
		if (ret < 0) {
			pr_err("%s, channel map cntrl add failed\n", __func__);
			goto fail;
		}
		kctl = chmap_info->kctl;
		for (i = 0; i < kctl->count; i++)
			kctl->vd[i].access |= SNDRV_CTL_ELEM_ACCESS_WRITE;

		snprintf(device_num, sizeof(device_num), "%d", pcm->device);
		strlcat(kctl->id.name, device_num, sizeof(kctl->id.name));
		pr_debug("%s, Overwriting channel map control name to: %s\n",
			__func__, kctl->id.name);

		kctl->put = msm_pcm_chmap_ctl_put;
		kctl->get = msm_pcm_chmap_ctl_get;
	}

	return 0;

fail:
	pr_err("%s: failed add chmap ctls, err = %d\n", __func__, ret);

	kfree(chmap_pspd[rtd->dai_link->id][SESSION_TYPE_RX]);
	kfree(chmap_pspd[rtd->dai_link->id][SESSION_TYPE_TX]);
	chmap_pspd[rtd->dai_link->id][SESSION_TYPE_RX] = NULL;
	chmap_pspd[rtd->dai_link->id][SESSION_TYPE_TX] = NULL;

	return ret;
}

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
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
	pr_err("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d copp_token %d\n",
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
	pr_err("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d copp_token %d\n",
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
	struct snd_pcm *pcm = rtd->pcm;
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
		if (ret < 0) {
			pr_err("%s: playback app type cntrl add failed: %d\n",
				__func__, ret);
			return ret;
		}
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
		if (ret < 0) {
			pr_err("%s: capture app type cntrl add failed: %d\n",
				__func__, ret);
			return ret;
		}
		kctl = app_type_info->kctl;
		snprintf(kctl->id.name, ctl_len, "%s %d %s",
			capture_mixer_ctl_name, rtd->pcm->device, suffix);
		kctl->put = msm_pcm_capture_app_type_cfg_ctl_put;
		kctl->get = msm_pcm_capture_app_type_cfg_ctl_get;
	}

	return 0;
}
#else
static int msm_pcm_add_app_type_controls(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}
#endif /* CONFIG_AUDIO_QGKI */

static struct msm_pcm_channel_mixer *msm_pcm_get_chmixer(
			struct msm_plat_data *pdata,
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
	struct msm_audio *prtd = NULL;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct snd_pcm *pcm = NULL;
	struct snd_pcm_substream *substream = NULL;
	struct msm_pcm_channel_mixer *chmixer_pspd = NULL;
	struct msm_pcm_channel_map *chmap = NULL;
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

	if (chmixer_pspd->input_channel < 0 ||
		chmixer_pspd->input_channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8 ||
		chmixer_pspd->output_channel < 0 ||
		chmixer_pspd->output_channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
		pr_err("%s: Invalid channels, in %d, out %d\n",
				__func__, chmixer_pspd->input_channel,
				chmixer_pspd->output_channel);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	if (substream->runtime && substream->ref_count <= 0) {
		pr_err_ratelimited("%s: substream ref_count:%d invalid\n",
				__func__, substream->ref_count);
		mutex_unlock(&pdata->lock);
		return -EINVAL;
	}
	prtd = substream->runtime ? substream->runtime->private_data : NULL;
	chmap = msm_pcm_get_chmap(fe_id, session_type);
	if (!chmap) {
		pr_err("%s: invalid chmap handle\n", __func__);
		mutex_unlock(&pdata->lock);
		return -EINVAL;
	}

	if (chmixer_pspd->enable) {
		if (session_type == SESSION_TYPE_RX &&
			!chmixer_pspd->override_in_ch_map) {
			if (chmap->set_channel_map) {
				for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
					chmixer_pspd->in_ch_map[i] = chmap->channel_map[i];
			} else {
				ret = q6asm_map_channels(asm_ch_map,
						chmixer_pspd->input_channel, false);
				if (ret) {
					pr_err("%s: unsupported chnum %d\n", __func__,
					chmixer_pspd->input_channel);
					mutex_unlock(&pdata->lock);
					return -EINVAL;
				}
				for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
					chmixer_pspd->in_ch_map[i] = asm_ch_map[i];
			}
			chmixer_pspd->override_in_ch_map = true;
			reset_override_in_ch_map = true;
		} else if (session_type == SESSION_TYPE_TX &&
				!chmixer_pspd->override_out_ch_map) {
			if (chmap->set_channel_map) {
				for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
					chmixer_pspd->out_ch_map[i] = chmap->channel_map[i];
			} else {
				ret = q6asm_map_channels(asm_ch_map,
						chmixer_pspd->output_channel, false);
				if (ret) {
					pr_err("%s: unsupported chnum %d\n", __func__,
					chmixer_pspd->output_channel);
					mutex_unlock(&pdata->lock);
					return -EINVAL;
				}
				for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
					chmixer_pspd->out_ch_map[i] = asm_ch_map[i];
			}
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

	if (chmixer_pspd->enable && prtd && prtd->audio_client) {
		stream_id = prtd->audio_client->session;
		be_id = chmixer_pspd->port_idx;
		msm_pcm_routing_set_channel_mixer_runtime(be_id,
				stream_id,
				session_type,
				chmixer_pspd);
	}

	if (reset_override_out_ch_map)
		chmixer_pspd->override_out_ch_map = false;
	if (reset_override_in_ch_map)
		chmixer_pspd->override_in_ch_map = false;

	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_pcm_channel_mixer_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
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
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
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
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
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
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
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
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
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
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
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
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
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
	struct msm_plat_data *pdata = NULL;
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

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(component->dev);

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

static int msm_pcm_chmixer_ec_ref_cfg_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	/* 2 int values: input_channel, output_channel */
	uinfo->count = 2;
	/* Valid range is all positive values to support above controls */
	uinfo->value.integer.min = 1;
	uinfo->value.integer.max = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	return 0;
}

static int msm_pcm_chmixer_ec_ref_cfg_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	ucontrol->value.integer.value[0] = chmixer_ec_ref->input_channel;
	ucontrol->value.integer.value[1] = chmixer_ec_ref->output_channel;

	return 0;
}

static int msm_pcm_chmixer_ec_ref_cfg_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref->input_channel = ucontrol->value.integer.value[0];
	chmixer_ec_ref->output_channel = ucontrol->value.integer.value[1];

	if (chmixer_ec_ref->input_channel < 0 ||
		chmixer_ec_ref->input_channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8 ||
		chmixer_ec_ref->output_channel < 0 ||
		chmixer_ec_ref->output_channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
		pr_err("%s: Invalid channels, in %d, out %d\n",
				__func__, chmixer_ec_ref->input_channel,
				chmixer_ec_ref->output_channel);
		return -EINVAL;
	}

	/* cache value and take effect during adm open stage */
	msm_pcm_routing_set_stream_ec_ref_chmix_cfg(fe_id, chmixer_ec_ref);

	return 0;
}

static int msm_pcm_add_chmixer_ec_ref_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *capture_mixer_ctl_name	= "AudStr Cap";
	const char *suffix		= "EC Ref ChMixer Cfg";
	int session_type = 0, ret = 0, channel = -1;
	struct msm_plat_data *pdata = NULL;
	struct snd_soc_component *component = NULL;
	struct snd_kcontrol_new channel_mixer_cfg_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_chmixer_ec_ref_cfg_info,
		.put = msm_pcm_chmixer_ec_ref_cfg_ctl_put,
		.get = msm_pcm_chmixer_ec_ref_cfg_ctl_get,
		.private_value = 0,
	};

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(component->dev);

	pdata->pcm_device[rtd->dai_link->id] = rtd->pcm;

	/* only apply for TX path */
	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(
			&channel_mixer_cfg_control,
			rtd, capture_mixer_ctl_name, suffix,
			session_type, channel);
		if (ret < 0)
			goto fail;
	}
	return 0;

fail:
	pr_err("%s: failed add platform ctl, err = %d\n",
		 __func__, ret);

	return ret;
}

static int msm_pcm_chmixer_ec_ref_weight_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid range: 0 to 0x4000(Unity) gain weightage */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x4000;
	return 0;
}

static int msm_pcm_chmixer_ec_ref_weight_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	int channel = (kcontrol->private_value >> 16) & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;
	int i = 0;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	if (channel <= 0 || channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
		pr_err("%s: invalid channel %d\n",
		 __func__, channel);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_ec_ref->channel_weight[channel-1][i];

	return 0;
}


static int msm_pcm_chmixer_ec_ref_weight_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	int channel = (kcontrol->private_value >> 16) & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;
	int i = 0;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	if (channel <= 0 || channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
		pr_err("%s: invalid channel %d\n",
		 __func__, channel);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmixer_ec_ref->channel_weight[channel-1][i] =
			ucontrol->value.integer.value[i];

	return 0;
}

static int msm_pcm_add_chmixer_ec_ref_weight_controls(
		struct snd_soc_pcm_runtime *rtd,
		int channel)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *capture_mixer_ctl_name	= "AudStr Cap";
	const char *suffix		= "EC Ref ChMixer Weight Ch";
	int session_type = 0, ret = 0;
	struct snd_kcontrol_new channel_mixer_weight_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_chmixer_ec_ref_weight_info,
		.put = msm_pcm_chmixer_ec_ref_weight_ctl_put,
		.get = msm_pcm_chmixer_ec_ref_weight_ctl_get,
		.private_value = 0,
	};

	/* only apply for TX path */
	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(
				&channel_mixer_weight_control,
				rtd, capture_mixer_ctl_name, suffix,
				session_type, channel);
		if (ret < 0)
			goto fail;
	}
	return 0;

fail:
	pr_err("%s: failed add platform ctl, err = %d\n",
		 __func__, ret);

	return ret;
}

static int msm_pcm_chmixer_ec_ref_input_map_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid channel map value ranges from 1 to 64 */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 64;
	return 0;
}

static int msm_pcm_chmixer_ec_ref_output_map_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid channel map value ranges from 1 to 64 */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 64;
	return 0;
}

static int msm_pcm_chmixer_ec_ref_input_map_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;
	int i = 0;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_ec_ref->in_ch_map[i];

	return 0;
}

static int msm_pcm_chmixer_ec_ref_input_map_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;
	int i = 0;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref->override_in_ch_map = true;
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmixer_ec_ref->in_ch_map[i] =
			ucontrol->value.integer.value[i];

	return 0;
}

static int msm_pcm_chmixer_ec_ref_output_map_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;
	int i = 0;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_ec_ref->out_ch_map[i];

	return 0;
}

static int msm_pcm_chmixer_ec_ref_output_map_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u16 fe_id = kcontrol->private_value & 0xFF;
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct msm_plat_data *pdata = dev_get_drvdata(component->dev);
	struct msm_pcm_channel_mixer *chmixer_ec_ref = NULL;
	int i = 0;

	if (!pdata) {
		pr_err("%s missing pdata\n", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref = pdata->chmixer_ec_ref[fe_id];
	if (!chmixer_ec_ref) {
		pr_err("%s: invalid chmixer_ec_ref in pdata", __func__);
		return -EINVAL;
	}

	chmixer_ec_ref->override_out_ch_map = true;
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmixer_ec_ref->out_ch_map[i] =
			ucontrol->value.integer.value[i];

	return 0;
}

static int msm_pcm_add_chmixer_ec_ref_input_map_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *capture_mixer_ctl_name	= "AudStr Cap";
	const char *suffix = "EC Ref ChMixer Input Map";
	int session_type = 0, ret = 0, channel = -1;
	struct snd_kcontrol_new channel_mixer_input_map_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_chmixer_ec_ref_input_map_info,
		.put = msm_pcm_chmixer_ec_ref_input_map_ctl_put,
		.get = msm_pcm_chmixer_ec_ref_input_map_ctl_get,
		.private_value = 0,
	};

	/* only apply for TX path */
	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(
			&channel_mixer_input_map_control,
			rtd, capture_mixer_ctl_name,
			suffix, session_type, channel);
		if (ret < 0)
			goto fail;
	}
	return 0;

fail:
	pr_err("%s: failed add platform ctl, err = %d\n",
		 __func__, ret);

	return ret;
}

static int msm_pcm_add_chmixer_ec_ref_output_map_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	const char *capture_mixer_ctl_name	= "AudStr Cap";
	const char *suffix		= "EC Ref ChMixer Output Map";
	int session_type = 0, ret = 0, channel = -1;
	struct snd_kcontrol_new channel_mixer_output_map_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_chmixer_ec_ref_output_map_info,
		.put = msm_pcm_chmixer_ec_ref_output_map_ctl_put,
		.get = msm_pcm_chmixer_ec_ref_output_map_ctl_get,
		.private_value = 0,
	};

	/* only apply for TX path */
	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream != NULL) {
		session_type = SESSION_TYPE_TX;
		ret = msm_pcm_add_platform_controls(
			&channel_mixer_output_map_control,
			rtd, capture_mixer_ctl_name,
			suffix, session_type, channel);
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
	struct msm_plat_data *pdata = NULL;
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

	pdata = (struct msm_plat_data *)
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

	if (pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream &&
		!pdata->chmixer_ec_ref[rtd->dai_link->id]) {
		pdata->chmixer_ec_ref[rtd->dai_link->id] =
			kzalloc(sizeof(struct msm_pcm_channel_mixer),
			GFP_KERNEL);
		if (!pdata->chmixer_ec_ref[rtd->dai_link->id]) {
			pr_err("%s: fail to allocate memory\n", __func__);
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

	ret = msm_pcm_add_chmixer_ec_ref_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add ef_ref channel mixer cfg controls failed:%d\n",
				__func__, ret);
		goto fail;
	}

	ret = msm_pcm_add_chmixer_ec_ref_input_map_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add ec_ref channel mixer input map controls failed:%d\n",
				__func__, ret);
		goto fail;
	}
	ret = msm_pcm_add_chmixer_ec_ref_output_map_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add ec_ref channel mixer output map controls failed:%d\n",
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

		ret =  msm_pcm_add_chmixer_ec_ref_weight_controls(rtd, i);
		if (ret) {
			pr_err("%s: pcm add ec_ref channel weight controls failed:%d\n",
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
	kfree(pdata->chmixer_ec_ref[rtd->dai_link->id]);
	pdata->chmixer_ec_ref[rtd->dai_link->id] = NULL;

	return ret;
}

static int msm_pcm_add_controls(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	ret = msm_pcm_add_chmap_controls(rtd);
	if (ret)
		pr_err("%s: pcm add controls failed:%d\n", __func__, ret);
	ret = msm_pcm_add_app_type_controls(rtd);
	if (ret)
		pr_err("%s: pcm add app type controls failed:%d\n",
			__func__, ret);
	ret = msm_pcm_add_channel_mixer_controls(rtd);
	if (ret)
		pr_err("%s: pcm add channel mixer controls failed:%d\n",
			__func__, ret);
	return ret;
}

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_pcm_add_controls(rtd);
	if (ret) {
		pr_err("%s, kctl add failed:%d\n", __func__, ret);
		return ret;
	}

	ret = msm_pcm_add_volume_control(rtd, SNDRV_PCM_STREAM_PLAYBACK);
	if (ret)
		pr_err("%s: Could not add pcm Volume Control %d\n",
			__func__, ret);
	ret = msm_pcm_add_volume_control(rtd, SNDRV_PCM_STREAM_CAPTURE);
	if (ret)
		pr_err("%s: Could not add pcm Volume Control %d\n",
			__func__, ret);
	ret = msm_pcm_add_compress_control(rtd);
	if (ret)
		pr_err("%s: Could not add pcm Compress Control %d\n",
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

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static snd_pcm_sframes_t msm_pcm_delay_blk(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_client *ac = prtd->audio_client;
	snd_pcm_sframes_t frames;
	int ret;

	ret = q6asm_get_path_delay(prtd->audio_client);
	if (ret) {
		pr_err("%s: get_path_delay failed, ret=%d\n", __func__, ret);
		return 0;
	}

	/* convert microseconds to frames */
	frames = ac->path_delay / 1000 * runtime->rate / 1000;

	/* also convert the remainder from the initial division */
	frames += ac->path_delay % 1000 * runtime->rate / 1000000;

	/* overcompensate for the loss of precision (empirical) */
	frames += 2;

	return frames;
}
#endif /* CONFIG_AUDIO_QGKI */

static struct snd_soc_component_driver msm_soc_component = {
	.name		= DRV_NAME,
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
	.delay_blk      = msm_pcm_delay_blk,
#endif /* CONFIG_AUDIO_QGKI */
};

static int msm_pcm_probe(struct platform_device *pdev)
{
	int rc;
	int id;
	struct msm_plat_data *pdata;
	const char *latency_level;

	rc = of_property_read_u32(pdev->dev.of_node,
				"qcom,msm-pcm-dsp-id", &id);
	if (rc) {
		dev_err(&pdev->dev, "%s: qcom,msm-pcm-dsp-id missing in DT node\n",
					__func__);
		return rc;
	}

	pdata = kzalloc(sizeof(struct msm_plat_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	if (of_property_read_bool(pdev->dev.of_node,
				"qcom,msm-pcm-low-latency")) {

		pdata->perf_mode = LOW_LATENCY_PCM_MODE;
		rc = of_property_read_string(pdev->dev.of_node,
			"qcom,latency-level", &latency_level);
		if (!rc) {
			if (!strcmp(latency_level, "ultra"))
				pdata->perf_mode = ULTRA_LOW_LATENCY_PCM_MODE;
			else if (!strcmp(latency_level, "ull-pp"))
				pdata->perf_mode =
					ULL_POST_PROCESSING_PCM_MODE;
		}
	} else {
		pdata->perf_mode = LEGACY_PCM_MODE;
	}
	mutex_init(&pdata->lock);
	dev_set_drvdata(&pdev->dev, pdata);


	dev_dbg(&pdev->dev, "%s: dev name %s\n",
				__func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					&msm_soc_component,
					NULL, 0);
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	struct msm_plat_data *pdata;
	int i = 0;

	pdata = dev_get_drvdata(&pdev->dev);
	if (pdata) {
		for (i = 0; i < MSM_FRONTEND_DAI_MM_SIZE; i++) {
			kfree(pdata->chmixer_pspd[i][SESSION_TYPE_RX]);
			kfree(pdata->chmixer_pspd[i][SESSION_TYPE_TX]);
			kfree(pdata->chmixer_ec_ref[i]);
		}
	}
	mutex_destroy(&pdata->lock);
	kfree(pdata);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}
static const struct of_device_id msm_pcm_dt_match[] = {
	{.compatible = "qcom,msm-pcm-dsp"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_pcm_dt_match);

static struct platform_driver msm_pcm_driver = {
	.driver = {
		.name = "msm-pcm-dsp",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_pcm_probe,
	.remove = msm_pcm_remove,
};

int __init msm_pcm_dsp_init(void)
{
	init_waitqueue_head(&the_locks.enable_wait);
	init_waitqueue_head(&the_locks.eos_wait);
	init_waitqueue_head(&the_locks.write_wait);
	init_waitqueue_head(&the_locks.read_wait);

	return platform_driver_register(&msm_pcm_driver);
}

void msm_pcm_dsp_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver);
}

MODULE_DESCRIPTION("PCM module platform driver");
MODULE_LICENSE("GPL v2");
