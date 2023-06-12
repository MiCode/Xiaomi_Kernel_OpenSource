// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */


#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/math64.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <audio/sound/audio_effects.h>
#include <audio/sound/audio_compressed_formats.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <audio/linux/msm_audio.h>

#include <sound/timer.h>
#include <sound/tlv.h>
#include <sound/compress_params.h>
#include <sound/compress_offload.h>
#include <sound/compress_driver.h>

#include <dsp/msm_audio_ion.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6asm-v2.h>
#include <dsp/q6core.h>
#include <dsp/msm-audio-effects-q6-v2.h>
#include "msm-pcm-routing-v2.h"
#include "msm-qti-pp-config.h"

#define DRV_NAME "msm-compress-q6-v2"

#define TIMEOUT_MS			1000
#define DSP_PP_BUFFERING_IN_MSEC	25
#define PARTIAL_DRAIN_ACK_EARLY_BY_MSEC	150
#define MP3_OUTPUT_FRAME_SZ		1152
#define AAC_OUTPUT_FRAME_SZ		1024
#define AC3_OUTPUT_FRAME_SZ		1536
#define EAC3_OUTPUT_FRAME_SZ		1536
#define DSP_NUM_OUTPUT_FRAME_BUFFERED	2
#define FLAC_BLK_SIZE_LIMIT		65535

/* Timestamp mode payload offsets */
#define CAPTURE_META_DATA_TS_OFFSET_LSW	6
#define CAPTURE_META_DATA_TS_OFFSET_MSW	7

/* decoder parameter length */
#define DDP_DEC_MAX_NUM_PARAM		18

/* Default values used if user space does not set */
#define COMPR_PLAYBACK_MIN_FRAGMENT_SIZE (8 * 1024)
#define COMPR_PLAYBACK_MAX_FRAGMENT_SIZE (128 * 1024)
#define COMPR_PLAYBACK_MIN_NUM_FRAGMENTS (4)
#define COMPR_PLAYBACK_MAX_NUM_FRAGMENTS (16 * 4)

#define COMPRESSED_LR_VOL_MAX_STEPS	0x2000
static const DECLARE_TLV_DB_LINEAR(msm_compr_vol_gain, 0,
				COMPRESSED_LR_VOL_MAX_STEPS);

/* Stream id switches between 1 and 2 */
#define NEXT_STREAM_ID(stream_id) ((stream_id & 1) + 1)

#define STREAM_ARRAY_INDEX(stream_id) (stream_id - 1)

#define MAX_NUMBER_OF_STREAMS 2

#define SND_DEC_DDP_MAX_PARAMS 18

#ifndef COMPRESSED_PERF_MODE_FLAG
#define COMPRESSED_PERF_MODE_FLAG 0
#endif

#define DSD_BLOCK_SIZE_4 4

struct msm_compr_gapless_state {
	bool set_next_stream_id;
	int32_t stream_opened[MAX_NUMBER_OF_STREAMS];
	uint32_t initial_samples_drop;
	uint32_t trailing_samples_drop;
	uint32_t gapless_transition;
	bool use_dsp_gapless_mode;
	union snd_codec_options codec_options;
};

static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 64000,
	88200, 96000, 128000, 144000, 176400, 192000, 352800, 384000, 2822400,
	5644800
};

struct msm_compr_pdata {
	struct snd_compr_stream *cstream[MSM_FRONTEND_DAI_MAX];
	uint32_t volume[MSM_FRONTEND_DAI_MAX][2]; /* For both L & R */
	struct msm_compr_audio_effects *audio_effects[MSM_FRONTEND_DAI_MAX];
	bool use_dsp_gapless_mode;
	bool use_legacy_api; /* indicates use older asm apis*/
	struct msm_compr_dec_params *dec_params[MSM_FRONTEND_DAI_MAX];
	struct msm_compr_ch_map *ch_map[MSM_FRONTEND_DAI_MAX];
	bool is_in_use[MSM_FRONTEND_DAI_MAX];
	struct msm_pcm_channel_mixer *chmixer_pspd[MSM_FRONTEND_DAI_MM_SIZE];
	struct mutex lock;
};

struct msm_compr_audio {
	struct snd_compr_stream *cstream;
	struct snd_compr_caps compr_cap;
	struct snd_compr_codec_caps codec_caps;
	struct snd_compr_params codec_param;
	struct audio_client *audio_client;

	uint32_t codec;
	uint32_t compr_passthr;
	void    *buffer; /* virtual address */
	phys_addr_t buffer_paddr; /* physical address */
	uint32_t app_pointer;
	uint32_t buffer_size;
	uint32_t byte_offset;
	uint64_t copied_total; /* bytes consumed by DSP */
	uint64_t bytes_received; /* from userspace */
	uint64_t bytes_sent; /* to DSP */

	uint64_t received_total; /* bytes received from DSP */
	uint64_t bytes_copied; /* to userspace */
	uint64_t bytes_read; /* from DSP */
	uint32_t bytes_read_offset; /* bytes read offset */

	uint32_t ts_header_offset; /* holds the timestamp header offset */

	int32_t first_buffer;
	int32_t last_buffer;
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
	int32_t zero_buffer;
#endif
	int32_t partial_drain_delay;

	uint16_t session_id;

	uint32_t sample_rate;
	uint32_t num_channels;

	/*
	 * convention - commands coming from the same thread
	 * can use the common cmd_ack var. Others (e.g drain/EOS)
	 * must use separate vars to track command status.
	 */
	uint32_t cmd_ack;
	uint32_t cmd_interrupt;
	uint32_t drain_ready;
	uint32_t eos_ack;

	uint32_t stream_available;
	uint32_t next_stream;

	uint32_t run_mode;
	uint32_t start_delay_lsw;
	uint32_t start_delay_msw;

	uint64_t marker_timestamp;

	struct msm_compr_gapless_state gapless_state;

	atomic_t start;
	atomic_t eos;
	atomic_t drain;
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
	atomic_t partial_drain;
#endif
	atomic_t xrun;
	atomic_t close;
	atomic_t wait_on_close;
	atomic_t error;

	wait_queue_head_t eos_wait;
	wait_queue_head_t drain_wait;
	wait_queue_head_t close_wait;
	wait_queue_head_t wait_for_stream_avail;

	spinlock_t lock;
};

static const u32 compr_codecs[] = {
	SND_AUDIOCODEC_AC3, SND_AUDIOCODEC_EAC3, SND_AUDIOCODEC_DTS,
	SND_AUDIOCODEC_TRUEHD, SND_AUDIOCODEC_IEC61937};

struct query_audio_effect {
	uint32_t mod_id;
	uint32_t parm_id;
	uint32_t size;
	uint32_t offset;
	uint32_t device;
};

struct msm_compr_audio_effects {
	struct bass_boost_params bass_boost;
	struct pbe_params pbe;
	struct virtualizer_params virtualizer;
	struct reverb_params reverb;
	struct eq_params equalizer;
	struct soft_volume_params volume;
	struct query_audio_effect query;
};

struct snd_dec_ddp {
	__u32 params_length;
	__u32 params_id[SND_DEC_DDP_MAX_PARAMS];
	__u32 params_value[SND_DEC_DDP_MAX_PARAMS];
} __attribute__((packed, aligned(4)));

struct msm_compr_dec_params {
	struct snd_dec_ddp ddp_params;
};

struct msm_compr_ch_map {
	bool set_ch_map;
	char channel_map[PCM_FORMAT_MAX_NUM_CHANNEL_V8];
};

static int msm_compr_send_dec_params(struct snd_compr_stream *cstream,
				     struct msm_compr_dec_params *dec_params,
				     int stream_id);

static int msm_compr_set_render_mode(struct msm_compr_audio *prtd,
				     uint32_t render_mode) {
	int ret = -EINVAL;
	struct audio_client *ac = prtd->audio_client;

	pr_debug("%s, got render mode %u\n", __func__, render_mode);

	if (render_mode == SNDRV_COMPRESS_RENDER_MODE_AUDIO_MASTER) {
		render_mode = ASM_SESSION_MTMX_STRTR_PARAM_RENDER_DEFAULT;
	} else if (render_mode == SNDRV_COMPRESS_RENDER_MODE_STC_MASTER) {
		render_mode = ASM_SESSION_MTMX_STRTR_PARAM_RENDER_LOCAL_STC;
		prtd->run_mode = ASM_SESSION_CMD_RUN_STARTIME_RUN_WITH_DELAY;
	} else {
		pr_err("%s, Invalid render mode %u\n", __func__,
			render_mode);
		ret = -EINVAL;
		goto exit;
	}

	ret = q6asm_send_mtmx_strtr_render_mode(ac, render_mode);
	if (ret) {
		pr_err("%s, Render mode can't be set error %d\n", __func__,
			ret);
	}
exit:
	return ret;
}

static int msm_compr_set_clk_rec_mode(struct audio_client *ac,
				     uint32_t clk_rec_mode) {
	int ret = -EINVAL;

	pr_debug("%s, got clk rec mode %u\n", __func__, clk_rec_mode);

	if (clk_rec_mode == SNDRV_COMPRESS_CLK_REC_MODE_NONE) {
		clk_rec_mode = ASM_SESSION_MTMX_STRTR_PARAM_CLK_REC_NONE;
	} else if (clk_rec_mode == SNDRV_COMPRESS_CLK_REC_MODE_AUTO) {
		clk_rec_mode = ASM_SESSION_MTMX_STRTR_PARAM_CLK_REC_AUTO;
	} else {
		pr_err("%s, Invalid clk rec_mode mode %u\n", __func__,
			clk_rec_mode);
		ret = -EINVAL;
		goto exit;
	}

	ret = q6asm_send_mtmx_strtr_clk_rec_mode(ac, clk_rec_mode);
	if (ret) {
		pr_err("%s, clk rec mode can't be set, error %d\n", __func__,
			ret);
	}

exit:
	return ret;
}

static int msm_compr_set_render_window(struct audio_client *ac,
		uint32_t ws_lsw, uint32_t ws_msw,
		uint32_t we_lsw, uint32_t we_msw)
{
	int ret = -EINVAL;
	struct asm_session_mtmx_strtr_param_window_v2_t asm_mtmx_strtr_window;
	uint32_t param_id;

	pr_debug("%s, ws_lsw 0x%x ws_msw 0x%x we_lsw 0x%x we_ms 0x%x\n",
		 __func__, ws_lsw, ws_msw, we_lsw, we_msw);

	memset(&asm_mtmx_strtr_window, 0,
	       sizeof(struct asm_session_mtmx_strtr_param_window_v2_t));
	asm_mtmx_strtr_window.window_lsw = ws_lsw;
	asm_mtmx_strtr_window.window_msw = ws_msw;
	param_id = ASM_SESSION_MTMX_STRTR_PARAM_RENDER_WINDOW_START_V2;
	ret = q6asm_send_mtmx_strtr_window(ac, &asm_mtmx_strtr_window,
					   param_id);
	if (ret) {
		pr_err("%s, start window can't be set error %d\n", __func__,
			ret);
		goto exit;
	}

	asm_mtmx_strtr_window.window_lsw = we_lsw;
	asm_mtmx_strtr_window.window_msw = we_msw;
	param_id = ASM_SESSION_MTMX_STRTR_PARAM_RENDER_WINDOW_END_V2;
	ret = q6asm_send_mtmx_strtr_window(ac, &asm_mtmx_strtr_window,
					   param_id);
	if (ret) {
		pr_err("%s, end window can't be set error %d\n", __func__,
			ret);
	}

exit:
	return ret;
}

static int msm_compr_enable_adjust_session_clock(struct audio_client *ac,
		bool enable)
{
	int ret;

	pr_debug("%s, enable adjust_session %d\n", __func__, enable);

	ret = q6asm_send_mtmx_strtr_enable_adjust_session_clock(ac, enable);
	if (ret)
		pr_err("%s, adjust session clock can't be set error %d\n",
			__func__, ret);

	return ret;
}

static int msm_compr_adjust_session_clock(struct audio_client *ac,
		uint32_t adjust_session_lsw, uint32_t adjust_session_msw)
{
	int ret;

	pr_debug("%s, adjust_session_time_msw 0x%x adjust_session_time_lsw 0x%x\n",
		 __func__, adjust_session_msw, adjust_session_lsw);

	ret = q6asm_adjust_session_clock(ac,
			adjust_session_lsw,
			adjust_session_msw);
	if (ret)
		pr_err("%s, adjust session clock can't be set error %d\n",
			__func__, ret);

	return ret;
}

static int msm_compr_set_volume(struct snd_compr_stream *cstream,
				uint32_t volume_l, uint32_t volume_r)
{
	struct msm_compr_audio *prtd;
	int rc = 0;
	uint32_t avg_vol, gain_list[VOLUME_CONTROL_MAX_CHANNELS];
	uint32_t num_channels;
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_component *component = NULL;
	struct msm_compr_pdata *pdata;
	bool use_default = true;
	u8 *chmap = NULL;

	pr_debug("%s: volume_l %d volume_r %d\n",
		__func__, volume_l, volume_r);
	if (!cstream || !cstream->runtime) {
		pr_err("%s: session not active\n", __func__);
		return -EPERM;
	}
	rtd = cstream->private_data;
	prtd = cstream->runtime->private_data;

	if (!rtd || !prtd || !prtd->audio_client) {
		pr_err("%s: invalid rtd, prtd or audio client", __func__);
		return rc;
	}
	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: invalid component\n", __func__);
		return rc;
	}

	pdata = snd_soc_component_get_drvdata(component);
	if (!pdata)
		return -EINVAL;

	if (prtd->compr_passthr != LEGACY_PCM) {
		pr_debug("%s: No volume config for passthrough %d\n",
			 __func__, prtd->compr_passthr);
		return rc;
	}
	if (!rtd->dai_link || !pdata->ch_map[rtd->dai_link->id])
		return -EINVAL;

	use_default = !(pdata->ch_map[rtd->dai_link->id]->set_ch_map);
	chmap = pdata->ch_map[rtd->dai_link->id]->channel_map;
	num_channels = prtd->num_channels;

	if (prtd->num_channels > 2) {
		/*
		 * Currently the left and right gains are averaged an applied
		 * to all channels. This might not be desirable. But currently,
		 * there exists no API in userspace to send a list of gains for
		 * each channel either. If such an API does become available,
		 * the mixer control must be updated to accept more than 2
		 * channel gains.
		 *
		 */
		avg_vol = (volume_l + volume_r) / 2;
		rc = q6asm_set_volume(prtd->audio_client, avg_vol);
	} else {
		gain_list[0] = volume_l;
		gain_list[1] = volume_r;
		gain_list[2] = volume_l;
		if (use_default)
			num_channels = 3;
		rc = q6asm_set_multich_gain(prtd->audio_client, num_channels,
					gain_list, chmap, use_default);
	}

	if (rc < 0)
		pr_err("%s: Send vol gain command failed rc=%d\n",
		       __func__, rc);

	return rc;
}

static int msm_compr_send_ddp_cfg(struct audio_client *ac,
				  struct snd_dec_ddp *ddp,
				  int stream_id)
{
	int i, rc;

	pr_debug("%s\n", __func__);
	for (i = 0; i < ddp->params_length; i++) {
		rc = q6asm_ds1_set_stream_endp_params(ac, ddp->params_id[i],
						      ddp->params_value[i],
						      stream_id);
		if (rc) {
			pr_err("sending params_id: %d failed\n",
				ddp->params_id[i]);
			return rc;
		}
	}
	return 0;
}

static int msm_compr_send_buffer(struct msm_compr_audio *prtd)
{
	int buffer_length;
	uint64_t bytes_available;
	struct audio_aio_write_param param;
	struct snd_codec_metadata *buff_addr;

	if (!atomic_read(&prtd->start)) {
		pr_err("%s: stream is not in started state\n", __func__);
		return -EINVAL;
	}


	if (atomic_read(&prtd->xrun)) {
		WARN(1, "%s called while xrun is true", __func__);
		return -EPERM;
	}

	pr_debug("%s: bytes_received = %llu copied_total = %llu\n",
		__func__, prtd->bytes_received, prtd->copied_total);
	if (prtd->first_buffer &&  prtd->gapless_state.use_dsp_gapless_mode &&
		prtd->compr_passthr == LEGACY_PCM)
		q6asm_stream_send_meta_data(prtd->audio_client,
				prtd->audio_client->stream_id,
				prtd->gapless_state.initial_samples_drop,
				prtd->gapless_state.trailing_samples_drop);

	buffer_length = prtd->codec_param.buffer.fragment_size;
	bytes_available = prtd->bytes_received - prtd->copied_total;
	if (bytes_available < prtd->codec_param.buffer.fragment_size)
		buffer_length = bytes_available;

	if (prtd->byte_offset + buffer_length > prtd->buffer_size) {
		buffer_length = (prtd->buffer_size - prtd->byte_offset);
		pr_debug("%s: wrap around situation, send partial data %d now",
			 __func__, buffer_length);
	}

	if (buffer_length) {
		param.paddr = prtd->buffer_paddr + prtd->byte_offset;
		WARN(prtd->byte_offset % 32 != 0, "offset %x not multiple of 32\n",
		prtd->byte_offset);
	} else {
		param.paddr = prtd->buffer_paddr;
	}
	param.len	= buffer_length;
	if (prtd->ts_header_offset) {
		buff_addr = (struct snd_codec_metadata *)
					(prtd->buffer + prtd->byte_offset);
		param.len = buff_addr->length;
		param.msw_ts = (uint32_t)
			((buff_addr->timestamp & 0xFFFFFFFF00000000LL) >> 32);
		param.lsw_ts = (uint32_t) (buff_addr->timestamp & 0xFFFFFFFFLL);
		param.paddr += prtd->ts_header_offset;
		param.flags = SET_TIMESTAMP;
		param.metadata_len = prtd->ts_header_offset;
	} else {
		param.msw_ts = 0;
		param.lsw_ts = 0;
		param.flags = NO_TIMESTAMP;
		param.metadata_len = 0;
	}
	param.uid	= buffer_length;
	param.last_buffer = prtd->last_buffer;

	pr_debug("%s: sending %d bytes to DSP byte_offset = %d\n",
		__func__, param.len, prtd->byte_offset);
	if (q6asm_async_write(prtd->audio_client, &param) < 0) {
		pr_err("%s:q6asm_async_write failed\n", __func__);
	} else {
		prtd->bytes_sent += buffer_length;
		if (prtd->first_buffer)
			prtd->first_buffer = 0;
	}

	return 0;
}

static int msm_compr_read_buffer(struct msm_compr_audio *prtd)
{
	int buffer_length;
	uint64_t bytes_available;
	uint64_t buffer_sent;
	struct audio_aio_read_param param;
	int ret;

	if (!atomic_read(&prtd->start)) {
		pr_err("%s: stream is not in started state\n", __func__);
		return -EINVAL;
	}

	buffer_length = prtd->codec_param.buffer.fragment_size -
						 prtd->ts_header_offset;
	bytes_available = prtd->received_total - prtd->bytes_copied;
	buffer_sent = prtd->bytes_read - prtd->bytes_copied;
	if (buffer_sent + buffer_length + prtd->ts_header_offset
						> prtd->buffer_size) {
		pr_debug(" %s : Buffer is Full bytes_available: %llu\n",
				__func__, bytes_available);
		return 0;
	}

	memset(&param, 0x0, sizeof(struct audio_aio_read_param));
	param.paddr = prtd->buffer_paddr + prtd->bytes_read_offset +
						prtd->ts_header_offset;
	param.len = buffer_length;
	param.uid = buffer_length;
	/* reserved[1] is for flags */
	param.flags = prtd->codec_param.codec.reserved[1];

	pr_debug("%s: reading %d bytes from DSP byte_offset = %llu\n",
			__func__, buffer_length, prtd->bytes_read);
	ret = q6asm_async_read(prtd->audio_client, &param);
	if (ret < 0) {
		pr_err("%s: q6asm_async_read failed - %d\n",
			__func__, ret);
		return ret;
	}
	prtd->bytes_read += buffer_length + prtd->ts_header_offset;
	prtd->bytes_read_offset += buffer_length + prtd->ts_header_offset;
	if (prtd->bytes_read_offset >= prtd->buffer_size)
		prtd->bytes_read_offset -= prtd->buffer_size;

	return 0;
}

static void compr_event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct msm_compr_audio *prtd = priv;
	struct snd_compr_stream *cstream;
	struct audio_client *ac;
	uint32_t chan_mode = 0;
	uint32_t sample_rate = 0;
	uint64_t bytes_available;
	int stream_id;
	uint32_t stream_index;
	unsigned long flags;
	uint64_t read_size;
	uint32_t *buff_addr;
	struct snd_soc_pcm_runtime *rtd;
	int ret = 0;

	if (!prtd) {
		pr_err("%s: prtd is NULL\n", __func__);
		return;
	}
	cstream = prtd->cstream;
	if (!cstream) {
		pr_err("%s: cstream is NULL\n", __func__);
		return;
	}

	ac = prtd->audio_client;

	/*
	 * Token for rest of the compressed commands use to set
	 * session id, stream id, dir etc.
	 */
	stream_id = q6asm_get_stream_id_from_token(token);

	pr_debug("%s opcode =%08x\n", __func__, opcode);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2:
		spin_lock_irqsave(&prtd->lock, flags);

		if (payload[3]) {
			pr_err("%s: WRITE FAILED w/ err 0x%x !, paddr 0x%x, byte_offset=%d,copied_total=%llu,token=%d\n",
				__func__,
				payload[3],
				payload[0],
				prtd->byte_offset,
				prtd->copied_total, token);

			if (atomic_cmpxchg(&prtd->drain, 1, 0) &&
			    prtd->last_buffer) {
				pr_debug("%s: wake up on drain\n", __func__);
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
				prtd->drain_ready = 1;
				wake_up(&prtd->drain_wait);
#endif
				prtd->last_buffer = 0;
			} else {
				atomic_set(&prtd->start, 0);
			}
		} else {
			pr_debug("ASM_DATA_EVENT_WRITE_DONE_V2 offset %d, length %d\n",
				 prtd->byte_offset, token);
		}

		/*
		 * Token for WRITE command represents the amount of data
		 * written to ADSP in the last write, update offset and
		 * total copied data accordingly.
		 */
		if (prtd->ts_header_offset) {
			/* Always assume that the data will be sent to DSP on
			 * frame boundary.
			 * i.e, one frame of userspace write will result in
			 * one kernel write to DSP. This is needed as
			 * timestamp will be sent per frame.
			 */
			prtd->byte_offset +=
					prtd->codec_param.buffer.fragment_size;
			prtd->copied_total +=
					prtd->codec_param.buffer.fragment_size;
		} else {
			prtd->byte_offset += token;
			prtd->copied_total += token;
		}
		if (prtd->byte_offset >= prtd->buffer_size)
			prtd->byte_offset -= prtd->buffer_size;

		snd_compr_fragment_elapsed(cstream);

		if (!atomic_read(&prtd->start)) {
			/* Writes must be restarted from _copy() */
			pr_debug("write_done received while not started, treat as xrun");
			atomic_set(&prtd->xrun, 1);
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}

#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
		if (prtd->zero_buffer) {
			pr_debug("write_done for zero buffer\n");
			prtd->zero_buffer = 0;

			/* move to next stream and reset vars */
			pr_debug("%s: Moving to next stream in gapless\n",
								__func__);
			ac->stream_id = NEXT_STREAM_ID(ac->stream_id);
			prtd->byte_offset = 0;
			prtd->app_pointer  = 0;
			prtd->first_buffer = 1;
			prtd->last_buffer = 0;
			/*
			 * Set gapless transition flag only if EOS hasn't been
			 * acknowledged already.
			 */
			if (atomic_read(&prtd->eos))
				prtd->gapless_state.gapless_transition = 1;
			prtd->marker_timestamp = 0;

			/*
			 * Don't reset these as these vars map to
			 * total_bytes_transferred and total_bytes_available
			 * directly, only total_bytes_transferred will be
			 * updated in the next avail() ioctl
			 *	prtd->copied_total = 0;
			 *	prtd->bytes_received = 0;
			 */
			atomic_set(&prtd->drain, 0);
			atomic_set(&prtd->xrun, 1);
			pr_debug("%s: issue CMD_RUN", __func__);
			q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
			snd_compr_drain_notify(cstream);
			/*
			 * Next track requires state of running. Otherwise,
			 * it fails.
			*/
			cstream->runtime->state = SNDRV_PCM_STATE_RUNNING;
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}

		bytes_available = prtd->bytes_received - prtd->copied_total;
		if (bytes_available == 0) {
			pr_debug("%s:bytes_available is 0\n", __func__);
			if (prtd->last_buffer)
				prtd->last_buffer = 0;

			if (atomic_read(&prtd->partial_drain) &&
				prtd->gapless_state.set_next_stream_id &&
				!prtd->zero_buffer) {

				pr_debug("%s:Partial Drain Case\n", __func__);
				pr_debug("%s:Send EOS command\n", __func__);
				/* send EOS */
				prtd->eos_ack = 0;
				atomic_set(&prtd->eos, 1);
				atomic_set(&prtd->drain, 0);
				q6asm_stream_cmd_nowait(ac, CMD_EOS, ac->stream_id);

				/* send a zero length buffer in case of partial drain*/
				atomic_set(&prtd->xrun, 0);
				pr_debug("%s:Send zero size buffer\n", __func__);
				msm_compr_send_buffer(prtd);
				prtd->zero_buffer = 1;
			} else {
				/*
				 * moving to next stream failed, so reset the gapless state
				 * set next stream id for the same session so that the same
				 * stream can be used for gapless playback
				 */
				pr_debug("%s:Drain Case\n", __func__);
				pr_debug("%s:Reset Gapless params \n", __func__);

				prtd->gapless_state.set_next_stream_id = false;
				prtd->gapless_state.gapless_transition = 0;

				pr_debug("%s:Send EOS command\n", __func__);
				prtd->eos_ack = 0;
				atomic_set(&prtd->eos, 1);
				atomic_set(&prtd->drain, 0);
				q6asm_stream_cmd_nowait(ac, CMD_EOS, ac->stream_id);

				prtd->cmd_interrupt = 0;
			}
		} else if (bytes_available < cstream->runtime->fragment_size) {
			pr_debug("%s:Partial Buffer Case \n", __func__);
			atomic_set(&prtd->xrun, 1);

			if (prtd->last_buffer)
				prtd->last_buffer = 0;
			if (atomic_read(&prtd->drain)) {
				if (bytes_available > 0) {
					pr_debug("%s: send %d partial bytes at the end",
						   __func__, bytes_available);
					atomic_set(&prtd->xrun, 0);
					prtd->last_buffer = 1;
					msm_compr_send_buffer(prtd);
				}
			}
#else
		bytes_available = prtd->bytes_received - prtd->copied_total;
		if (bytes_available < cstream->runtime->fragment_size) {
			pr_debug("WRITE_DONE Insufficient data to send. break out\n");
			atomic_set(&prtd->xrun, 1);

			if (prtd->last_buffer)
				prtd->last_buffer = 0;
			if (atomic_read(&prtd->drain)) {
				pr_debug("wake up on drain\n");
				prtd->drain_ready = 1;
				wake_up(&prtd->drain_wait);
				atomic_set(&prtd->drain, 0);
			}
#endif
		} else if ((bytes_available == cstream->runtime->fragment_size)
			   && atomic_read(&prtd->drain)) {
			prtd->last_buffer = 1;
			msm_compr_send_buffer(prtd);
			prtd->last_buffer = 0;
		} else
			msm_compr_send_buffer(prtd);

		spin_unlock_irqrestore(&prtd->lock, flags);
		break;

	case ASM_DATA_EVENT_READ_DONE_V2:
		spin_lock_irqsave(&prtd->lock, flags);

		pr_debug("ASM_DATA_EVENT_READ_DONE_V2 offset %d, length %d\n",
				 prtd->byte_offset, payload[4]);

		if (prtd->ts_header_offset) {
			/* Update the header for received buffer */
			buff_addr = prtd->buffer + prtd->byte_offset;
			/* Write the actual length of the received buffer */
			*buff_addr = payload[4];
			buff_addr++;
			/* Write the offset */
			*buff_addr = prtd->ts_header_offset;
			buff_addr++;
			/* Write the TS LSW */
			*buff_addr = payload[CAPTURE_META_DATA_TS_OFFSET_LSW];
			buff_addr++;
			/* Write the TS MSW */
			*buff_addr = payload[CAPTURE_META_DATA_TS_OFFSET_MSW];
		}
		/* Always assume read_size is same as fragment_size */
		read_size = prtd->codec_param.buffer.fragment_size;
		prtd->byte_offset += read_size;
		prtd->received_total += read_size;
		if (prtd->byte_offset >= prtd->buffer_size)
			prtd->byte_offset -= prtd->buffer_size;

		snd_compr_fragment_elapsed(cstream);

		if (!atomic_read(&prtd->start)) {
			pr_debug("read_done received while not started, treat as xrun");
			atomic_set(&prtd->xrun, 1);
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}
		msm_compr_read_buffer(prtd);

		spin_unlock_irqrestore(&prtd->lock, flags);
		break;

	case ASM_DATA_EVENT_RENDERED_EOS:
	case ASM_DATA_EVENT_RENDERED_EOS_V2:
		spin_lock_irqsave(&prtd->lock, flags);
		pr_debug("%s: ASM_DATA_CMDRSP_EOS token 0x%x,stream id %d\n",
			  __func__, token, stream_id);
		if (atomic_read(&prtd->eos) &&
		    !prtd->gapless_state.set_next_stream_id) {
			pr_debug("ASM_DATA_CMDRSP_EOS wake up\n");
			prtd->eos_ack = 1;
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
			wake_up(&prtd->eos_wait);
#else
			pr_debug("%s:issue CMD_PAUSE stream_id %d",
					  __func__, ac->stream_id);
			q6asm_stream_cmd_nowait(ac, CMD_PAUSE, ac->stream_id);
			prtd->cmd_ack = 0;

			pr_debug("%s:DRAIN,don't wait for EOS ack\n", __func__);
			/*
			 * Don't reset these as these vars map to
			 * total_bytes_transferred and total_bytes_available.
			 * Just total_bytes_transferred will be updated
			 * in the next avail() ioctl.
			 * prtd->copied_total = 0;
			 * prtd->bytes_received = 0;
			 * do not reset prtd->bytes_sent as well as the same
			 * session is used for gapless playback
			 */
			prtd->byte_offset = 0;

			prtd->app_pointer  = 0;
			prtd->first_buffer = 1;
			prtd->last_buffer = 0;
			atomic_set(&prtd->drain, 0);
			atomic_set(&prtd->xrun, 1);

			pr_debug("%s:issue CMD_FLUSH ac->stream_id %d",
						  __func__, ac->stream_id);

			q6asm_run_nowait(prtd->audio_client, 0, 0, 0);

			snd_compr_drain_notify(cstream);
#endif
		}
		atomic_set(&prtd->eos, 0);
		stream_index = STREAM_ARRAY_INDEX(stream_id);
		if (stream_index >= MAX_NUMBER_OF_STREAMS) {
			pr_err("%s: Invalid stream index %d", __func__,
				stream_index);
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}

		if (prtd->gapless_state.set_next_stream_id &&
			prtd->gapless_state.stream_opened[stream_index]) {
			pr_debug("%s: CMD_CLOSE stream_id %d\n",
				  __func__, stream_id);
			q6asm_stream_cmd_nowait(ac, CMD_CLOSE, stream_id);
			atomic_set(&prtd->close, 1);
			prtd->gapless_state.stream_opened[stream_index] = 0;
			prtd->gapless_state.set_next_stream_id = false;
		}
		if (prtd->gapless_state.gapless_transition)
			prtd->gapless_state.gapless_transition = 0;
		spin_unlock_irqrestore(&prtd->lock, flags);
		break;
	case ASM_STREAM_PP_EVENT:
	case ASM_STREAM_CMD_ENCDEC_EVENTS:
		pr_debug("%s: ASM_STREAM_EVENT(0x%x)\n", __func__, opcode);
		rtd = cstream->private_data;
		if (!rtd) {
			pr_err("%s: rtd is NULL\n", __func__);
			return;
		}

		ret = msm_adsp_inform_mixer_ctl(rtd, payload);
		if (ret) {
			pr_err("%s: failed to inform mixer ctrl. err = %d\n",
				__func__, ret);
			return;
		}
		break;
	case ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY:
	case ASM_DATA_EVENT_ENC_SR_CM_CHANGE_NOTIFY: {
		pr_debug("ASM_DATA_EVENT_SR_CM_CHANGE_NOTIFY\n");
		chan_mode = payload[1] >> 16;
		sample_rate = payload[2] >> 16;
		if (prtd && (chan_mode != prtd->num_channels ||
				sample_rate != prtd->sample_rate)) {
			prtd->num_channels = chan_mode;
			prtd->sample_rate = sample_rate;
		}
	}
		/* Fallthrough here */
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN_V2:
			/* check if the first buffer need to be sent to DSP */
			pr_debug("ASM_SESSION_CMD_RUN_V2\n");

			/* FIXME: A state is a better way, dealing with this */
			spin_lock_irqsave(&prtd->lock, flags);

			if (cstream->direction == SND_COMPRESS_CAPTURE) {
				atomic_set(&prtd->start, 1);
				msm_compr_read_buffer(prtd);
				spin_unlock_irqrestore(&prtd->lock, flags);
				break;
			}

			if (!prtd->bytes_sent) {
				bytes_available = prtd->bytes_received -
						  prtd->copied_total;
				if (bytes_available <
				    cstream->runtime->fragment_size) {
					pr_debug("CMD_RUN_V2 Insufficient data to send. break out\n");
					atomic_set(&prtd->xrun, 1);
				} else {
					msm_compr_send_buffer(prtd);
				}
			}

			/*
			 * The condition below ensures playback finishes in the
			 * follow cornercase
			 * WRITE(last buffer)
			 * WAIT_FOR_DRAIN
			 * PAUSE
			 * WRITE_DONE(X)
			 * RESUME
			 */
			if ((prtd->copied_total == prtd->bytes_sent) &&
					atomic_read(&prtd->drain)) {
				bytes_available = prtd->bytes_received - prtd->copied_total;
				if (bytes_available < cstream->runtime->fragment_size) {
					pr_debug("%s: RUN ack, wake up & continue pending drain\n",
							__func__);

					if (prtd->last_buffer)
						prtd->last_buffer = 0;

					prtd->drain_ready = 1;
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
					wake_up(&prtd->drain_wait);
#endif
					atomic_set(&prtd->drain, 0);
				} else if (atomic_read(&prtd->xrun)) {
					pr_debug("%s: RUN ack, continue write cycle\n", __func__);
					atomic_set(&prtd->xrun, 0);
					msm_compr_send_buffer(prtd);
				}
			}

			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		case ASM_STREAM_CMD_FLUSH:
			pr_debug("%s: ASM_STREAM_CMD_FLUSH:", __func__);
			pr_debug("token 0x%x, stream id %d\n", token,
				  stream_id);
			prtd->cmd_ack = 1;
			break;
		case ASM_DATA_CMD_REMOVE_INITIAL_SILENCE:
			pr_debug("%s: ASM_DATA_CMD_REMOVE_INITIAL_SILENCE:",
				   __func__);
			pr_debug("token 0x%x, stream id = %d\n", token,
				  stream_id);
			break;
		case ASM_DATA_CMD_REMOVE_TRAILING_SILENCE:
			pr_debug("%s: ASM_DATA_CMD_REMOVE_TRAILING_SILENCE:",
				  __func__);
			pr_debug("token = 0x%x,	stream id = %d\n", token,
				  stream_id);
			break;
		case ASM_STREAM_CMD_CLOSE:
			pr_debug("%s: ASM_DATA_CMD_CLOSE:", __func__);
			pr_debug("token 0x%x, stream id %d\n", token,
				  stream_id);
			/*
			 * wakeup wait for stream avail on stream 3
			 * after stream 1 ends.
			 */
			if (prtd->next_stream) {
				pr_debug("%s:CLOSE:wakeup wait for stream\n",
					  __func__);
				prtd->stream_available = 1;
				wake_up(&prtd->wait_for_stream_avail);
				prtd->next_stream = 0;
			}
			if (atomic_read(&prtd->close) &&
			    atomic_read(&prtd->wait_on_close)) {
				prtd->cmd_ack = 1;
				wake_up(&prtd->close_wait);
			}
			atomic_set(&prtd->close, 0);
			break;
		case ASM_STREAM_CMD_REGISTER_PP_EVENTS:
			pr_debug("%s: ASM_STREAM_CMD_REGISTER_PP_EVENTS:",
				__func__);
			break;
		default:
			break;
		}
		break;
	}
	case ASM_SESSION_CMDRSP_GET_SESSIONTIME_V3:
		pr_debug("%s: ASM_SESSION_CMDRSP_GET_SESSIONTIME_V3\n",
			  __func__);
		break;
	case RESET_EVENTS:
		pr_err("%s: Received reset events CB, move to error state",
			__func__);
		spin_lock_irqsave(&prtd->lock, flags);
		/*
		 * Since ADSP is down, let this driver pretend that it copied
		 * all the bytes received, so that next write will be triggered
		 */
		prtd->copied_total = prtd->bytes_received;
		snd_compr_fragment_elapsed(cstream);
		atomic_set(&prtd->error, 1);
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		wake_up(&prtd->drain_wait);
		if (atomic_cmpxchg(&prtd->eos, 1, 0)) {
			pr_debug("%s:unblock eos wait queues", __func__);
			wake_up(&prtd->eos_wait);
		}
#endif
		spin_unlock_irqrestore(&prtd->lock, flags);
		break;
	default:
		pr_debug("%s: Not Supported Event opcode[0x%x]\n",
			  __func__, opcode);
		break;
	}
}

static int msm_compr_get_partial_drain_delay(int frame_sz, int sample_rate)
{
	int delay_time_ms = 0;

	delay_time_ms = ((DSP_NUM_OUTPUT_FRAME_BUFFERED * frame_sz * 1000) /
			sample_rate) + DSP_PP_BUFFERING_IN_MSEC;
	delay_time_ms = delay_time_ms > PARTIAL_DRAIN_ACK_EARLY_BY_MSEC ?
			delay_time_ms - PARTIAL_DRAIN_ACK_EARLY_BY_MSEC : 0;

	pr_debug("%s: frame_sz %d, sample_rate %d, partial drain delay %d\n",
		__func__, frame_sz, sample_rate, delay_time_ms);
	return delay_time_ms;
}

static void populate_codec_list(struct msm_compr_audio *prtd)
{
	pr_debug("%s\n", __func__);
	prtd->compr_cap.direction = SND_COMPRESS_PLAYBACK;
	prtd->compr_cap.min_fragment_size =
			COMPR_PLAYBACK_MIN_FRAGMENT_SIZE;
	prtd->compr_cap.max_fragment_size =
			COMPR_PLAYBACK_MAX_FRAGMENT_SIZE;
	prtd->compr_cap.min_fragments =
			COMPR_PLAYBACK_MIN_NUM_FRAGMENTS;
	prtd->compr_cap.max_fragments =
			COMPR_PLAYBACK_MAX_NUM_FRAGMENTS;
	prtd->compr_cap.num_codecs = 10;
	prtd->compr_cap.codecs[0] = SND_AUDIOCODEC_MP3;
	prtd->compr_cap.codecs[1] = SND_AUDIOCODEC_AAC;
	prtd->compr_cap.codecs[2] = SND_AUDIOCODEC_AC3;
	prtd->compr_cap.codecs[3] = SND_AUDIOCODEC_EAC3;
	prtd->compr_cap.codecs[4] = SND_AUDIOCODEC_MP2;
	prtd->compr_cap.codecs[5] = SND_AUDIOCODEC_PCM;
	prtd->compr_cap.codecs[6] = SND_AUDIOCODEC_DTS;
	prtd->compr_cap.codecs[7] = SND_AUDIOCODEC_TRUEHD;
	prtd->compr_cap.codecs[8] = SND_AUDIOCODEC_IEC61937;
	prtd->compr_cap.codecs[9] = SND_AUDIOCODEC_BESPOKE;
}

static int msm_compr_get_decoder_format(struct msm_compr_audio *prtd,
	int *frame_sz, bool *is_format_gapless)
{
	int ret = 0;
	__s32 *gdec;

	pr_debug("%s: generic format = %x", __func__,
		prtd->codec_param.codec.options.generic.reserved[0]);

	switch(prtd->codec_param.codec.options.generic.reserved[0]) {
	case AUDIO_COMP_FORMAT_ALAC:
		ret = FORMAT_ALAC;
		break;
	case AUDIO_COMP_FORMAT_APE:
		ret = FORMAT_APE;
		break;
	case AUDIO_COMP_FORMAT_APTX:
		ret = FORMAT_APTX;
		break;
	case AUDIO_COMP_FORMAT_DSD:
		ret = FORMAT_DSD;
		break;
	case AUDIO_COMP_FORMAT_FLAC:
		ret = FORMAT_FLAC;
		*is_format_gapless = true;
		gdec = &(prtd->codec_param.codec.options.generic.reserved[1]);
		*frame_sz = ((struct snd_generic_dec_flac *)gdec)->min_blk_size;
		break;
	case AUDIO_COMP_FORMAT_VORBIS:
		ret = FORMAT_VORBIS;
		break;
	case AUDIO_COMP_FORMAT_WMA:
		ret = FORMAT_WMA_V9;
		break;
	case AUDIO_COMP_FORMAT_WMA_PRO:
		ret = FORMAT_WMA_V10PRO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int msm_compr_send_media_format_block(struct snd_compr_stream *cstream,
					     int stream_id,
					     bool use_gapless_codec_options)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component =NULL;
	struct msm_compr_pdata *pdata = NULL;
	struct asm_aac_cfg aac_cfg;
	struct asm_wma_cfg wma_cfg;
	struct asm_wmapro_cfg wma_pro_cfg;
	struct asm_flac_cfg flac_cfg;
	struct asm_vorbis_cfg vorbis_cfg;
	struct asm_alac_cfg alac_cfg;
	struct asm_ape_cfg ape_cfg;
	struct asm_dsd_cfg dsd_cfg;
	struct aptx_dec_bt_addr_cfg aptx_cfg;
	struct asm_amrwbplus_cfg amrwbplus_cfg;
	union snd_codec_options *codec_options;

	int ret = 0;
	uint16_t bit_width;
	bool use_default_chmap = true;
	char *chmap = NULL;
	uint16_t sample_word_size;
	__s32 *gdec;

	pr_debug("%s: use_gapless_codec_options %d\n",
			__func__, use_gapless_codec_options);

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = snd_soc_component_get_drvdata(component);

	if (use_gapless_codec_options)
		codec_options = &(prtd->gapless_state.codec_options);
	else
		codec_options = &(prtd->codec_param.codec.options);

	if (!codec_options) {
		pr_err("%s: codec_options is NULL\n", __func__);
		return -EINVAL;
	}

	switch (prtd->codec) {
	case FORMAT_LINEAR_PCM:
		pr_debug("SND_AUDIOCODEC_PCM\n");
		if (pdata->ch_map[rtd->dai_link->id]) {
			use_default_chmap =
			    !(pdata->ch_map[rtd->dai_link->id]->set_ch_map);
			chmap =
			    pdata->ch_map[rtd->dai_link->id]->channel_map;
		}

		switch (prtd->codec_param.codec.format) {
		case SNDRV_PCM_FORMAT_S32_LE:
			bit_width = 32;
			sample_word_size = 32;
			break;
		case SNDRV_PCM_FORMAT_S24_LE:
			bit_width = 24;
			sample_word_size = 32;
			break;
		case SNDRV_PCM_FORMAT_S24_3LE:
			bit_width = 24;
			sample_word_size = 24;
			break;
		case SNDRV_PCM_FORMAT_S16_LE:
		default:
			bit_width = 16;
			sample_word_size = 16;
			break;
		}

		if (q6core_get_avcs_api_version_per_service(
					APRV2_IDS_SERVICE_ID_ADSP_ASM_V) >=
					ADSP_ASM_API_VERSION_V2) {
			ret = q6asm_media_format_block_pcm_format_support_v5(
					prtd->audio_client,
					prtd->sample_rate,
					prtd->num_channels,
					bit_width, stream_id,
					use_default_chmap,
					chmap,
					sample_word_size,
					ASM_LITTLE_ENDIAN,
					DEFAULT_QF);
		} else {
			ret = q6asm_media_format_block_pcm_format_support_v4(
					prtd->audio_client,
					prtd->sample_rate,
					prtd->num_channels,
					bit_width, stream_id,
					use_default_chmap,
					chmap,
					sample_word_size,
					ASM_LITTLE_ENDIAN,
					DEFAULT_QF);
		}
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);

		break;
	case FORMAT_MP3:
		pr_debug("SND_AUDIOCODEC_MP3\n");
		/* no media format block needed */
		break;
	case FORMAT_MPEG4_AAC:
		pr_debug("SND_AUDIOCODEC_AAC\n");
		memset(&aac_cfg, 0x0, sizeof(struct asm_aac_cfg));
		aac_cfg.aot = AAC_ENC_MODE_EAAC_P;
		if (prtd->codec_param.codec.format ==
					SND_AUDIOSTREAMFORMAT_MP4ADTS)
			aac_cfg.format = 0x0;
		else if (prtd->codec_param.codec.format ==
					SND_AUDIOSTREAMFORMAT_MP4LATM)
			aac_cfg.format = 0x04;
		else
			aac_cfg.format = 0x03;
		aac_cfg.ch_cfg = prtd->num_channels;
		aac_cfg.sample_rate = prtd->sample_rate;
		ret = q6asm_stream_media_format_block_aac(prtd->audio_client,
							  &aac_cfg, stream_id);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	case FORMAT_AC3:
		pr_debug("SND_AUDIOCODEC_AC3\n");
		break;
	case FORMAT_EAC3:
		pr_debug("SND_AUDIOCODEC_EAC3\n");
		break;
	case FORMAT_WMA_V9:
		pr_debug("SND_AUDIOCODEC_WMA\n");
		memset(&wma_cfg, 0x0, sizeof(struct asm_wma_cfg));
		wma_cfg.format_tag = prtd->codec_param.codec.format;
		wma_cfg.ch_cfg = prtd->codec_param.codec.ch_in;
		wma_cfg.sample_rate = prtd->sample_rate;
		gdec = &(codec_options->generic.reserved[1]);
		wma_cfg.avg_bytes_per_sec =
			((struct snd_generic_dec_wma *)gdec)->avg_bit_rate/8;
		wma_cfg.block_align =
			((struct snd_generic_dec_wma *)gdec)->super_block_align;
		wma_cfg.valid_bits_per_sample =
			((struct snd_generic_dec_wma *)gdec)->bits_per_sample;
		wma_cfg.ch_mask =
			((struct snd_generic_dec_wma *)gdec)->channelmask;
		wma_cfg.encode_opt =
			((struct snd_generic_dec_wma *)gdec)->encodeopt;
		ret = q6asm_media_format_block_wma(prtd->audio_client,
					&wma_cfg, stream_id);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	case FORMAT_WMA_V10PRO:
		pr_debug("SND_AUDIOCODEC_WMA_PRO\n");
		memset(&wma_pro_cfg, 0x0, sizeof(struct asm_wmapro_cfg));
		wma_pro_cfg.format_tag = prtd->codec_param.codec.format;
		wma_pro_cfg.ch_cfg = prtd->codec_param.codec.ch_in;
		wma_pro_cfg.sample_rate = prtd->sample_rate;
		gdec = &(codec_options->generic.reserved[1]);
		wma_cfg.avg_bytes_per_sec =
			((struct snd_generic_dec_wma *)gdec)->avg_bit_rate/8;
		wma_pro_cfg.block_align =
			((struct snd_generic_dec_wma *)gdec)->super_block_align;
		wma_pro_cfg.valid_bits_per_sample =
			((struct snd_generic_dec_wma *)gdec)->bits_per_sample;
		wma_pro_cfg.ch_mask =
			((struct snd_generic_dec_wma *)gdec)->channelmask;
		wma_pro_cfg.encode_opt =
			((struct snd_generic_dec_wma *)gdec)->encodeopt;
		wma_pro_cfg.adv_encode_opt =
			((struct snd_generic_dec_wma *)gdec)->encodeopt1;
		wma_pro_cfg.adv_encode_opt2 =
			((struct snd_generic_dec_wma *)gdec)->encodeopt2;
		ret = q6asm_media_format_block_wmapro(prtd->audio_client,
				&wma_pro_cfg, stream_id);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	case FORMAT_MP2:
		pr_debug("%s: SND_AUDIOCODEC_MP2\n", __func__);
		break;
	case FORMAT_FLAC:
		pr_debug("%s: SND_AUDIOCODEC_FLAC\n", __func__);
		memset(&flac_cfg, 0x0, sizeof(struct asm_flac_cfg));
		flac_cfg.ch_cfg = prtd->num_channels;
		flac_cfg.sample_rate = prtd->sample_rate;
		flac_cfg.stream_info_present = 1;
		gdec = &(codec_options->generic.reserved[1]);
		flac_cfg.sample_size =
			((struct snd_generic_dec_flac *)gdec)->sample_size;
		flac_cfg.min_blk_size =
			((struct snd_generic_dec_flac *)gdec)->min_blk_size;
		flac_cfg.max_blk_size =
			((struct snd_generic_dec_flac *)gdec)->max_blk_size;
		flac_cfg.max_frame_size =
			((struct snd_generic_dec_flac *)gdec)->max_frame_size;
		flac_cfg.min_frame_size =
			((struct snd_generic_dec_flac *)gdec)->min_frame_size;
		ret = q6asm_stream_media_format_block_flac(prtd->audio_client,
							&flac_cfg, stream_id);
		if (ret < 0)
			pr_err("%s: CMD Format block failed ret %d\n",
				__func__, ret);
		break;
	case FORMAT_VORBIS:
		pr_debug("%s: SND_AUDIOCODEC_VORBIS\n", __func__);
		memset(&vorbis_cfg, 0x0, sizeof(struct asm_vorbis_cfg));
		gdec = &(codec_options->generic.reserved[1]);
		vorbis_cfg.bit_stream_fmt =
			((struct snd_generic_dec_vorbis *)gdec)->bit_stream_fmt;
		ret = q6asm_stream_media_format_block_vorbis(
					prtd->audio_client, &vorbis_cfg,
					stream_id);
		if (ret < 0)
			pr_err("%s: CMD Format block failed ret %d\n",
					__func__, ret);
		break;
	case FORMAT_ALAC:
		pr_debug("%s: SND_AUDIOCODEC_ALAC\n", __func__);
		memset(&alac_cfg, 0x0, sizeof(struct asm_alac_cfg));
		alac_cfg.num_channels = prtd->num_channels;
		alac_cfg.sample_rate = prtd->sample_rate;
		gdec = &(codec_options->generic.reserved[1]);
		alac_cfg.frame_length =
			((struct snd_generic_dec_alac *)gdec)->frame_length;
		alac_cfg.compatible_version =
		((struct snd_generic_dec_alac *)gdec)->compatible_version;
		alac_cfg.bit_depth =
			((struct snd_generic_dec_alac *)gdec)->bit_depth;
		alac_cfg.pb = ((struct snd_generic_dec_alac *)gdec)->pb;
		alac_cfg.mb = ((struct snd_generic_dec_alac *)gdec)->mb;
		alac_cfg.kb = ((struct snd_generic_dec_alac *)gdec)->kb;
		alac_cfg.max_run =
			((struct snd_generic_dec_alac *)gdec)->max_run;
		alac_cfg.max_frame_bytes =
			((struct snd_generic_dec_alac *)gdec)->max_frame_bytes;
		alac_cfg.avg_bit_rate =
			((struct snd_generic_dec_alac *)gdec)->avg_bit_rate;
		alac_cfg.channel_layout_tag =
		((struct snd_generic_dec_alac *)gdec)->channel_layout_tag;
		ret = q6asm_media_format_block_alac(prtd->audio_client,
							&alac_cfg, stream_id);
		if (ret < 0)
			pr_err("%s: CMD Format block failed ret %d\n",
					__func__, ret);
		break;
	case FORMAT_APE:
		pr_debug("%s: SND_AUDIOCODEC_APE\n", __func__);
		memset(&ape_cfg, 0x0, sizeof(struct asm_ape_cfg));
		ape_cfg.num_channels = prtd->num_channels;
		ape_cfg.sample_rate = prtd->sample_rate;
		gdec = &(codec_options->generic.reserved[1]);
		ape_cfg.compatible_version =
		((struct snd_generic_dec_ape *)gdec)->compatible_version;
		ape_cfg.compression_level =
		((struct snd_generic_dec_ape *)gdec)->compression_level;
		ape_cfg.format_flags =
		((struct snd_generic_dec_ape *)gdec)->format_flags;
		ape_cfg.blocks_per_frame =
		((struct snd_generic_dec_ape *)gdec)->blocks_per_frame;
		ape_cfg.final_frame_blocks =
		((struct snd_generic_dec_ape *)gdec)->final_frame_blocks;
		ape_cfg.total_frames =
		((struct snd_generic_dec_ape *)gdec)->total_frames;
		ape_cfg.bits_per_sample =
		((struct snd_generic_dec_ape *)gdec)->bits_per_sample;
		ape_cfg.seek_table_present =
		((struct snd_generic_dec_ape *)gdec)->seek_table_present;
		ret = q6asm_media_format_block_ape(prtd->audio_client,
							&ape_cfg, stream_id);
		if (ret < 0)
			pr_err("%s: CMD Format block failed ret %d\n",
					__func__, ret);
		break;
	case FORMAT_DTS:
		pr_debug("SND_AUDIOCODEC_DTS\n");
		/* no media format block needed */
		break;
	case FORMAT_DSD:
		pr_debug("%s: SND_AUDIOCODEC_DSD\n", __func__);
		memset(&dsd_cfg, 0x0, sizeof(struct asm_dsd_cfg));
		dsd_cfg.num_channels = prtd->num_channels;
		dsd_cfg.dsd_data_rate = prtd->sample_rate;
		dsd_cfg.num_version = 0;
		dsd_cfg.is_bitwise_big_endian = 1;
		dsd_cfg.dsd_channel_block_size = 1;
		gdec = &(codec_options->generic.reserved[1]);
		if (((struct snd_generic_dec_dsd *)gdec)->blk_size ==
							DSD_BLOCK_SIZE_4)
			dsd_cfg.dsd_channel_block_size =
				((struct snd_generic_dec_dsd *)gdec)->blk_size;

		ret = q6asm_media_format_block_dsd(prtd->audio_client,
						   &dsd_cfg, stream_id);
		if (ret < 0)
			pr_err("%s: CMD DSD Format block failed ret %d\n",
				__func__, ret);
		break;
	case FORMAT_TRUEHD:
		pr_debug("SND_AUDIOCODEC_TRUEHD\n");
		/* no media format block needed */
		break;
	case FORMAT_IEC61937:
		pr_debug("SND_AUDIOCODEC_IEC61937\n");
		ret = q6asm_media_format_block_iec(prtd->audio_client,
						   prtd->sample_rate,
						   prtd->num_channels);
		if (ret < 0)
			pr_err("%s: CMD IEC61937 Format block failed ret %d\n",
				__func__, ret);
		break;
	case FORMAT_APTX:
		pr_debug("SND_AUDIOCODEC_APTX\n");
		memset(&aptx_cfg, 0x0, sizeof(struct aptx_dec_bt_addr_cfg));
		ret = q6asm_stream_media_format_block_aptx_dec(
							prtd->audio_client,
							prtd->sample_rate,
							stream_id);
		if (ret >= 0) {
			gdec = &(codec_options->generic.reserved[1]);
			aptx_cfg.nap =
				((struct snd_generic_dec_aptx *)gdec)->nap;
			aptx_cfg.uap =
				((struct snd_generic_dec_aptx *)gdec)->uap;
			aptx_cfg.lap =
				((struct snd_generic_dec_aptx *)gdec)->lap;
			q6asm_set_aptx_dec_bt_addr(prtd->audio_client,
							&aptx_cfg);
		} else {
			pr_err("%s: CMD Format block failed ret %d\n",
					 __func__, ret);
		}
		break;
	case FORMAT_AMRNB:
		pr_debug("SND_AUDIOCODEC_AMR\n");
		/* no media format block needed */
		break;
	case FORMAT_AMRWB:
		pr_debug("SND_AUDIOCODEC_AMRWB\n");
		/* no media format block needed */
		break;
	case FORMAT_AMR_WB_PLUS:
		pr_debug("SND_AUDIOCODEC_AMRWBPLUS\n");
		memset(&amrwbplus_cfg, 0x0, sizeof(struct asm_amrwbplus_cfg));
		gdec = &(codec_options->generic.reserved[1]);
		amrwbplus_cfg.amr_frame_fmt =
		((struct snd_generic_dec_amrwb_plus *)gdec)->bit_stream_fmt;
		ret = q6asm_media_format_block_amrwbplus(
				prtd->audio_client,
				&amrwbplus_cfg);
		if (ret < 0)
			pr_err("%s: CMD AMRWBPLUS Format block failed ret %d\n",
				__func__, ret);
		break;
	default:
		pr_debug("%s, unsupported format, skip", __func__);
		break;
	}
	return ret;
}

static int msm_compr_init_pp_params(struct snd_compr_stream *cstream,
				    struct audio_client *ac)
{
	int ret = 0;
	struct asm_softvolume_params softvol = {
		.period = SOFT_VOLUME_PERIOD,
		.step = SOFT_VOLUME_STEP,
		.rampingcurve = SOFT_VOLUME_CURVE_LINEAR,
	};

	switch (ac->topology) {
	default:
		ret = q6asm_set_softvolume_v2(ac, &softvol,
					      SOFT_VOLUME_INSTANCE_1);
		if (ret < 0)
			pr_err("%s: Send SoftVolume Param failed ret=%d\n",
			__func__, ret);

		break;
	}
	return ret;
}

static int msm_compr_configure_dsp_for_playback
			(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *soc_prtd = cstream->private_data;
	uint16_t bits_per_sample = 16;
	int dir = IN, ret = 0;
	struct audio_client *ac = prtd->audio_client;
	uint32_t stream_index;
	struct asm_softpause_params softpause = {
		.enable = SOFT_PAUSE_ENABLE,
		.period = SOFT_PAUSE_PERIOD,
		.step = SOFT_PAUSE_STEP,
		.rampingcurve = SOFT_PAUSE_CURVE_LINEAR,
	};
	struct asm_softvolume_params softvol = {
		.period = SOFT_VOLUME_PERIOD,
		.step = SOFT_VOLUME_STEP,
		.rampingcurve = SOFT_VOLUME_CURVE_LINEAR,
	};
	struct snd_kcontrol *kctl;
	struct snd_ctl_elem_value kctl_elem_value;
	uint16_t target_asm_bit_width = 0;

	pr_debug("%s: stream_id %d\n", __func__, ac->stream_id);
	stream_index = STREAM_ARRAY_INDEX(ac->stream_id);
	if (stream_index >= MAX_NUMBER_OF_STREAMS) {
		pr_err("%s: Invalid stream index:%d", __func__, stream_index);
		return -EINVAL;
	}

	kctl = snd_soc_card_get_kcontrol(soc_prtd->card,
		DSP_BIT_WIDTH_MIXER_CTL);
	if (kctl) {
		kctl->get(kctl, &kctl_elem_value);
		target_asm_bit_width = kctl_elem_value.value.integer.value[0];
		if (target_asm_bit_width > 0) {
			pr_debug("%s enforce ASM bitwidth to %d from %d\n",
				__func__,
				target_asm_bit_width,
				bits_per_sample);
			bits_per_sample = target_asm_bit_width;
		}
	} else {
		pr_info("%s: failed to get mixer ctl for %s.\n",
			__func__, DSP_BIT_WIDTH_MIXER_CTL);
	}

	if ((prtd->codec_param.codec.format == SNDRV_PCM_FORMAT_S24_LE) ||
		(prtd->codec_param.codec.format == SNDRV_PCM_FORMAT_S24_3LE))
		bits_per_sample = 24;
	else if (prtd->codec_param.codec.format == SNDRV_PCM_FORMAT_S32_LE)
		bits_per_sample = 32;

	ac->fedai_id = soc_prtd->dai_link->id;
	ac->stream_type = SNDRV_PCM_STREAM_PLAYBACK;

	if (prtd->compr_passthr != LEGACY_PCM) {
		ret = q6asm_open_write_compressed(ac, prtd->codec,
						  prtd->compr_passthr);
		if (ret < 0) {
			pr_err("%s:ASM open write err[%d] for compr_type[%d]\n",
				__func__, ret, prtd->compr_passthr);
			return ret;
		}
		prtd->gapless_state.stream_opened[stream_index] = 1;

		ret = msm_pcm_routing_reg_phy_compr_stream(
				soc_prtd->dai_link->id,
				ac->perf_mode,
				prtd->session_id,
				SNDRV_PCM_STREAM_PLAYBACK,
				prtd->compr_passthr);
		if (ret) {
			pr_err("%s: compr stream reg failed:%d\n", __func__,
				ret);
			return ret;
		}
	} else {
		pr_debug("%s: stream_id %d bits_per_sample %d\n",
				__func__, ac->stream_id, bits_per_sample);

		if (q6core_get_avcs_api_version_per_service(
					APRV2_IDS_SERVICE_ID_ADSP_ASM_V) >=
					ADSP_ASM_API_VERSION_V2)
			ret = q6asm_stream_open_write_v5(ac,
				prtd->codec, bits_per_sample,
				ac->stream_id,
				prtd->gapless_state.use_dsp_gapless_mode);
		else
			ret = q6asm_stream_open_write_v4(ac,
				prtd->codec, bits_per_sample,
				ac->stream_id,
				prtd->gapless_state.use_dsp_gapless_mode);
		if (ret < 0) {
			pr_err("%s:ASM open write err[%d] for compr type[%d]\n",
				__func__, ret, prtd->compr_passthr);
			return -ENOMEM;
		}
		prtd->gapless_state.stream_opened[stream_index] = 1;

		pr_debug("%s: BE id %d\n", __func__, soc_prtd->dai_link->id);
		ret = msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->id,
				ac->perf_mode,
				prtd->session_id,
				SNDRV_PCM_STREAM_PLAYBACK);
		if (ret) {
			pr_err("%s: stream reg failed:%d\n", __func__, ret);
			return ret;
		}
	}

	ret = msm_compr_set_volume(cstream, 0, 0);
	if (ret < 0)
		pr_err("%s : Set Volume failed : %d", __func__, ret);

	if (prtd->compr_passthr != LEGACY_PCM) {
		pr_debug("%s : Don't send cal and PP params for compress path",
				__func__);
	} else {
		ret = q6asm_send_cal(ac);
		if (ret < 0)
			pr_debug("%s : Send cal failed : %d", __func__, ret);

		ret = q6asm_set_softpause(ac, &softpause);
		if (ret < 0)
			pr_err("%s: Send SoftPause Param failed ret=%d\n",
					__func__, ret);

		ret = q6asm_set_softvolume(ac, &softvol);
		if (ret < 0)
			pr_err("%s: Send SoftVolume Param failed ret=%d\n",
					__func__, ret);
	}
	ret = q6asm_set_io_mode(ac, (COMPRESSED_STREAM_IO | ASYNC_IO_MODE));
	if (ret < 0) {
		pr_err("%s: Set IO mode failed\n", __func__);
		return -EINVAL;
	}

	runtime->fragments = prtd->codec_param.buffer.fragments;
	runtime->fragment_size = prtd->codec_param.buffer.fragment_size;
	pr_debug("allocate %d buffers each of size %d\n",
			runtime->fragments,
			runtime->fragment_size);
	ret = q6asm_audio_client_buf_alloc_contiguous(dir, ac,
					runtime->fragment_size,
					runtime->fragments);
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed rc = %d\n", ret);
		return -ENOMEM;
	}

	prtd->byte_offset  = 0;
	prtd->copied_total = 0;
	prtd->app_pointer  = 0;
	prtd->bytes_received = 0;
	prtd->bytes_sent = 0;
	prtd->buffer       = ac->port[dir].buf[0].data;
	prtd->buffer_paddr = ac->port[dir].buf[0].phys;
	prtd->buffer_size  = runtime->fragments * runtime->fragment_size;

	/* Bit-0 of flags represent timestamp mode */
	/* reserved[1] is for flags */
	if (prtd->codec_param.codec.reserved[1] & COMPRESSED_TIMESTAMP_FLAG)
		prtd->ts_header_offset = sizeof(struct snd_codec_metadata);
	else
		prtd->ts_header_offset = 0;

	ret = msm_compr_send_media_format_block(cstream, ac->stream_id, false);
	if (ret < 0)
		pr_err("%s, failed to send media format block\n", __func__);

	return ret;
}

static int msm_compr_configure_dsp_for_capture(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *soc_prtd = cstream->private_data;
	uint16_t bits_per_sample;
	uint16_t sample_word_size;
	int dir = OUT, ret = 0;
	struct audio_client *ac = prtd->audio_client;
	uint32_t stream_index;
	uint32_t enc_cfg_id = ENC_CFG_ID_NONE;
	bool compress_ts = false;

	switch (prtd->codec_param.codec.format) {
	case SNDRV_PCM_FORMAT_S24_LE:
		bits_per_sample = 24;
		sample_word_size = 32;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		bits_per_sample = 24;
		sample_word_size = 24;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		bits_per_sample = 32;
		sample_word_size = 32;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
	default:
		bits_per_sample = 16;
		sample_word_size = 16;
		if (prtd->codec == FORMAT_BESPOKE)
			enc_cfg_id =
			prtd->codec_param.codec.options.generic.reserved[0];
		break;
	}

	prtd->audio_client->stream_type = SNDRV_PCM_STREAM_CAPTURE;
	prtd->audio_client->fedai_id = soc_prtd->dai_link->id;

	pr_debug("%s: stream_id %d bits_per_sample %d compr_passthr %d\n",
			__func__, ac->stream_id, bits_per_sample,
			prtd->compr_passthr);

	if (prtd->compr_passthr != LEGACY_PCM) {
		ret = q6asm_open_read_compressed(prtd->audio_client,
                                prtd->codec, prtd->compr_passthr);
		if (ret < 0) {
			pr_err("%s:ASM open read err[%d] for compr_type[%d]\n",
					__func__, ret, prtd->compr_passthr);
			return ret;
		}

		ret = msm_pcm_routing_reg_phy_compr_stream(
				soc_prtd->dai_link->id,
				ac->perf_mode,
				prtd->session_id,
				SNDRV_PCM_STREAM_CAPTURE,
				prtd->compr_passthr);
		if (ret) {
			pr_err("%s: compr stream reg failed:%d\n",
					__func__, ret);
			return ret;
		}
	} else {
		/* reserved[1] is for flags */
		if (prtd->codec_param.codec.reserved[1]
			& COMPRESSED_TIMESTAMP_FLAG)
			compress_ts = true;

		if (q6core_get_avcs_api_version_per_service(
				APRV2_IDS_SERVICE_ID_ADSP_ASM_V) >=
				ADSP_ASM_API_VERSION_V2)
			ret = q6asm_open_read_v5(prtd->audio_client,
					prtd->codec, bits_per_sample,
					compress_ts, enc_cfg_id);
		else
			ret = q6asm_open_read_v4(prtd->audio_client,
					prtd->codec, bits_per_sample,
					compress_ts, enc_cfg_id);
		if (ret < 0) {
			pr_err("%s: q6asm_open_read failed:%d\n",
					__func__, ret);
			return ret;
		}

		ret = msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->id,
				ac->perf_mode,
				prtd->session_id,
				SNDRV_PCM_STREAM_CAPTURE);
		if (ret) {
			pr_err("%s: stream reg failed:%d\n", __func__, ret);
			return ret;
		}
	}

	ret = q6asm_set_io_mode(ac, (COMPRESSED_STREAM_IO | ASYNC_IO_MODE));
	if (ret < 0) {
		pr_err("%s: Set IO mode failed\n", __func__);
		return -EINVAL;
	}

	stream_index = STREAM_ARRAY_INDEX(ac->stream_id);
	if (stream_index >= MAX_NUMBER_OF_STREAMS) {
		pr_err("%s: Invalid stream index:%d", __func__, stream_index);
		return -EINVAL;
	}

	runtime->fragments = prtd->codec_param.buffer.fragments;
	runtime->fragment_size = prtd->codec_param.buffer.fragment_size;
	pr_debug("%s: allocate %d buffers each of size %d\n",
			__func__, runtime->fragments,
			runtime->fragment_size);
	ret = q6asm_audio_client_buf_alloc_contiguous(dir, ac,
					runtime->fragment_size,
					runtime->fragments);
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed rc = %d\n", ret);
		return -ENOMEM;
	}

	prtd->byte_offset    = 0;
	prtd->received_total = 0;
	prtd->app_pointer    = 0;
	prtd->bytes_copied   = 0;
	prtd->bytes_read     = 0;
	prtd->bytes_read_offset = 0;
	prtd->buffer         = ac->port[dir].buf[0].data;
	prtd->buffer_paddr   = ac->port[dir].buf[0].phys;
	prtd->buffer_size    = runtime->fragments * runtime->fragment_size;

	/* Bit-0 of flags represent timestamp mode */
	/* reserved[1] is for flags */
	if (prtd->codec_param.codec.reserved[1] & COMPRESSED_TIMESTAMP_FLAG)
		prtd->ts_header_offset = sizeof(struct snd_codec_metadata);
	else
		prtd->ts_header_offset = 0;

	pr_debug("%s: sample_rate = %d channels = %d bps = %d sample_word_size = %d\n",
			__func__, prtd->sample_rate, prtd->num_channels,
					 bits_per_sample, sample_word_size);
	if (prtd->codec == FORMAT_BESPOKE) {
		/*
		 * For BESPOKE codec, encoder specific config params are
		 * included as part of generic.
		 */
		ret = q6asm_enc_cfg_blk_custom(prtd->audio_client, prtd->sample_rate,
			prtd->num_channels, prtd->codec,
			(void *)&prtd->codec_param.codec.options.generic);
	} else if (prtd->compr_passthr == LEGACY_PCM) {
		if (q6core_get_avcs_api_version_per_service(
				APRV2_IDS_SERVICE_ID_ADSP_ASM_V) >=
				ADSP_ASM_API_VERSION_V2)
			ret = q6asm_enc_cfg_blk_pcm_format_support_v5(
					prtd->audio_client,
					prtd->sample_rate, prtd->num_channels,
					true, NULL,
					bits_per_sample, sample_word_size,
					ASM_LITTLE_ENDIAN, DEFAULT_QF);
		else
			ret = q6asm_enc_cfg_blk_pcm_format_support_v4(
					prtd->audio_client,
					prtd->sample_rate, prtd->num_channels,
					bits_per_sample, sample_word_size,
					ASM_LITTLE_ENDIAN, DEFAULT_QF);
	}

	return ret;
}

static int msm_compr_playback_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component = NULL;
	struct msm_compr_audio *prtd = NULL;
	struct msm_compr_pdata *pdata = NULL;
	enum apr_subsys_state subsys_state;

	pr_debug("%s\n", __func__);
	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = snd_soc_component_get_drvdata(component);
	if (pdata->is_in_use[rtd->dai_link->id] == true) {
		pr_err("%s: %s is already in use, err: %d\n",
			__func__, rtd->dai_link->cpus->dai_name, -EBUSY);
		return -EBUSY;
	}

	subsys_state = apr_get_subsys_state();
	if (subsys_state == APR_SUBSYS_DOWN) {
		pr_debug("%s: adsp is down\n", __func__);
		return -ENETRESET;
	}

	prtd = kzalloc(sizeof(struct msm_compr_audio), GFP_KERNEL);
	if (prtd == NULL) {
		pr_err("Failed to allocate memory for msm_compr_audio\n");
		return -ENOMEM;
	}

	runtime->private_data = NULL;
	prtd->cstream = cstream;
	pdata->cstream[rtd->dai_link->id] = cstream;
	pdata->audio_effects[rtd->dai_link->id] =
		 kzalloc(sizeof(struct msm_compr_audio_effects), GFP_KERNEL);
	if (pdata->audio_effects[rtd->dai_link->id] == NULL) {
		pr_err("%s: Could not allocate memory for effects\n", __func__);
		pdata->cstream[rtd->dai_link->id] = NULL;
		kfree(prtd);
		return -ENOMEM;
	}
	pdata->dec_params[rtd->dai_link->id] =
		 kzalloc(sizeof(struct msm_compr_dec_params), GFP_KERNEL);
	if (pdata->dec_params[rtd->dai_link->id] == NULL) {
		pr_err("%s: Could not allocate memory for dec params\n",
			__func__);
		kfree(pdata->audio_effects[rtd->dai_link->id]);
		pdata->audio_effects[rtd->dai_link->id] = NULL;
		pdata->cstream[rtd->dai_link->id] = NULL;
		kfree(prtd);
		return -ENOMEM;
	}
	prtd->codec = FORMAT_MP3;
	prtd->bytes_received = 0;
	prtd->bytes_sent = 0;
	prtd->copied_total = 0;
	prtd->byte_offset = 0;
	prtd->sample_rate = 44100;
	prtd->num_channels = 2;
	prtd->drain_ready = 0;
	prtd->last_buffer = 0;
	prtd->first_buffer = 1;
	prtd->partial_drain_delay = 0;
	prtd->next_stream = 0;
	memset(&prtd->gapless_state, 0, sizeof(struct msm_compr_gapless_state));
	/*
	 * Update the use_dsp_gapless_mode from gapless struture with the value
	 * part of platform data.
	 */
	prtd->gapless_state.use_dsp_gapless_mode = pdata->use_dsp_gapless_mode;

	pr_debug("%s: gapless mode %d", __func__, pdata->use_dsp_gapless_mode);

	spin_lock_init(&prtd->lock);

	atomic_set(&prtd->eos, 0);
	atomic_set(&prtd->start, 0);
	atomic_set(&prtd->drain, 0);
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
	atomic_set(&prtd->partial_drain, 0);
#endif
	atomic_set(&prtd->xrun, 0);
	atomic_set(&prtd->close, 0);
	atomic_set(&prtd->wait_on_close, 0);
	atomic_set(&prtd->error, 0);

	init_waitqueue_head(&prtd->eos_wait);
	init_waitqueue_head(&prtd->drain_wait);
	init_waitqueue_head(&prtd->close_wait);
	init_waitqueue_head(&prtd->wait_for_stream_avail);

	runtime->private_data = prtd;
	populate_codec_list(prtd);
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)compr_event_handler, prtd);
	if (prtd->audio_client == NULL) {
		pr_err("%s: Could not allocate memory for client\n", __func__);
		kfree(pdata->audio_effects[rtd->dai_link->id]);
		pdata->audio_effects[rtd->dai_link->id] = NULL;
		kfree(pdata->dec_params[rtd->dai_link->id]);
		pdata->dec_params[rtd->dai_link->id] = NULL;
		pdata->cstream[rtd->dai_link->id] = NULL;
		kfree(prtd);
		runtime->private_data = NULL;
		return -ENOMEM;
	}
	pr_debug("%s: session ID %d\n", __func__, prtd->audio_client->session);
	prtd->audio_client->perf_mode = false;
	prtd->session_id = prtd->audio_client->session;
	msm_adsp_init_mixer_ctl_pp_event_queue(rtd);
	pdata->is_in_use[rtd->dai_link->id] = true;
	return 0;
}

static int msm_compr_capture_open(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component = NULL;
	struct msm_compr_audio *prtd;
	struct msm_compr_pdata *pdata = NULL;
	enum apr_subsys_state subsys_state;

	pr_debug("%s\n", __func__);
	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = snd_soc_component_get_drvdata(component);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	subsys_state = apr_get_subsys_state();
	if (subsys_state == APR_SUBSYS_DOWN) {
		pr_debug("%s: adsp is down\n", __func__);
		return -ENETRESET;
	}
	prtd = kzalloc(sizeof(struct msm_compr_audio), GFP_KERNEL);
	if (!prtd) {
		pr_err("Failed to allocate memory for msm_compr_audio\n");
		return -ENOMEM;
	}

	runtime->private_data = NULL;
	prtd->cstream = cstream;
	pdata->cstream[rtd->dai_link->id] = cstream;

	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)compr_event_handler, prtd);
	if (!prtd->audio_client) {
		pr_err("%s: Could not allocate memory for client\n", __func__);
		pdata->cstream[rtd->dai_link->id] = NULL;
		kfree(prtd);
		return -ENOMEM;
	}
	pr_debug("%s: session ID %d\n", __func__, prtd->audio_client->session);
	prtd->audio_client->perf_mode = false;
	prtd->session_id = prtd->audio_client->session;
	prtd->codec = FORMAT_LINEAR_PCM;
	prtd->bytes_copied = 0;
	prtd->bytes_read = 0;
	prtd->bytes_read_offset = 0;
	prtd->received_total = 0;
	prtd->byte_offset = 0;
	prtd->sample_rate = 48000;
	prtd->num_channels = 2;
	prtd->first_buffer = 0;

	spin_lock_init(&prtd->lock);

	atomic_set(&prtd->eos, 0);
	atomic_set(&prtd->start, 0);
	atomic_set(&prtd->drain, 0);
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
	atomic_set(&prtd->partial_drain, 0);
#endif
	atomic_set(&prtd->xrun, 0);
	atomic_set(&prtd->close, 0);
	atomic_set(&prtd->wait_on_close, 0);
	atomic_set(&prtd->error, 0);

	init_waitqueue_head(&prtd->eos_wait);
	init_waitqueue_head(&prtd->drain_wait);
	init_waitqueue_head(&prtd->close_wait);
	init_waitqueue_head(&prtd->wait_for_stream_avail);

	runtime->private_data = prtd;

	return 0;
}

static int msm_compr_open(struct snd_compr_stream *cstream)
{
	int ret = 0;

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
		ret = msm_compr_playback_open(cstream);
	else if (cstream->direction == SND_COMPRESS_CAPTURE)
		ret = msm_compr_capture_open(cstream);
	return ret;
}

static int msm_compr_playback_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime;
	struct msm_compr_audio *prtd;
	struct snd_soc_pcm_runtime *soc_prtd;
	struct snd_soc_component *component = NULL;
	struct msm_compr_pdata *pdata;
	struct audio_client *ac;
	int dir = IN, ret = 0, stream_id;
	unsigned long flags;
	uint32_t stream_index;

	pr_debug("%s\n", __func__);

	if (!cstream) {
		pr_err("%s cstream is null\n", __func__);
		return 0;
	}
	runtime = cstream->runtime;
	soc_prtd = cstream->private_data;
	if (!runtime || !soc_prtd) {
		pr_err("%s runtime or soc_prtd is null\n",
			__func__);
		return 0;
	}
	component = snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	if (!component) {
		pr_err("%s component is null\n", __func__);
		return 0;
	}
	prtd = runtime->private_data;
	if (!prtd) {
		pr_err("%s prtd is null\n", __func__);
		return 0;
	}
	prtd->cmd_interrupt = 1;
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
	wake_up(&prtd->drain_wait);
#endif
	pdata = snd_soc_component_get_drvdata(component);
	ac = prtd->audio_client;
	if (!pdata || !ac) {
		pr_err("%s pdata or ac is null\n", __func__);
		return 0;
	}
	if (atomic_read(&prtd->eos)) {
		ret = wait_event_timeout(prtd->eos_wait,
					prtd->eos_ack,
					msecs_to_jiffies(TIMEOUT_MS));
		if (!ret)
			pr_err("%s: CMD_EOS failed\n", __func__);
	}
	if (atomic_read(&prtd->close)) {
		prtd->cmd_ack = 0;
		atomic_set(&prtd->wait_on_close, 1);
		ret = wait_event_timeout(prtd->close_wait,
					prtd->cmd_ack,
					msecs_to_jiffies(TIMEOUT_MS));
		if (!ret)
			pr_err("%s: CMD_CLOSE failed\n", __func__);
	}

	spin_lock_irqsave(&prtd->lock, flags);
	stream_id = ac->stream_id;
	stream_index = STREAM_ARRAY_INDEX(NEXT_STREAM_ID(stream_id));

	if ((stream_index < MAX_NUMBER_OF_STREAMS) &&
	    (prtd->gapless_state.stream_opened[stream_index])) {
		prtd->gapless_state.stream_opened[stream_index] = 0;
		spin_unlock_irqrestore(&prtd->lock, flags);
		pr_debug(" close stream %d", NEXT_STREAM_ID(stream_id));
		q6asm_stream_cmd(ac, CMD_CLOSE, NEXT_STREAM_ID(stream_id));
		spin_lock_irqsave(&prtd->lock, flags);
	}

	stream_index = STREAM_ARRAY_INDEX(stream_id);
	if ((stream_index < MAX_NUMBER_OF_STREAMS) &&
	    (prtd->gapless_state.stream_opened[stream_index])) {
		prtd->gapless_state.stream_opened[stream_index] = 0;
		spin_unlock_irqrestore(&prtd->lock, flags);
		pr_debug("close stream %d", stream_id);
		q6asm_stream_cmd(ac, CMD_CLOSE, stream_id);
		spin_lock_irqsave(&prtd->lock, flags);
	}
	spin_unlock_irqrestore(&prtd->lock, flags);

	mutex_lock(&pdata->lock);
	pdata->cstream[soc_prtd->dai_link->id] = NULL;
	if (cstream->direction == SND_COMPRESS_PLAYBACK) {
		msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->id,
						SNDRV_PCM_STREAM_PLAYBACK);
	}

	q6asm_audio_client_buf_free_contiguous(dir, ac);

	q6asm_audio_client_free(ac);
	msm_adsp_clean_mixer_ctl_pp_event_queue(soc_prtd);
	if (pdata->audio_effects[soc_prtd->dai_link->id] != NULL) {
		kfree(pdata->audio_effects[soc_prtd->dai_link->id]);
		pdata->audio_effects[soc_prtd->dai_link->id] = NULL;
	}
	if (pdata->dec_params[soc_prtd->dai_link->id] != NULL) {
		kfree(pdata->dec_params[soc_prtd->dai_link->id]);
		pdata->dec_params[soc_prtd->dai_link->id] = NULL;
	}
	if (pdata->ch_map[soc_prtd->dai_link->id]) {
		pdata->ch_map[soc_prtd->dai_link->id]->set_ch_map = false;
	}
	pdata->is_in_use[soc_prtd->dai_link->id] = false;
	kfree(prtd);
	runtime->private_data = NULL;
	mutex_unlock(&pdata->lock);

	return 0;
}

static int msm_compr_capture_free(struct snd_compr_stream *cstream)
{
	struct snd_compr_runtime *runtime;
	struct msm_compr_audio *prtd;
	struct snd_soc_pcm_runtime *soc_prtd;
	struct snd_soc_component *component = NULL;
	struct msm_compr_pdata *pdata;
	struct audio_client *ac;
	int dir = OUT, stream_id;
	unsigned long flags;
	uint32_t stream_index;

	if (!cstream) {
		pr_err("%s cstream is null\n", __func__);
		return 0;
	}
	runtime = cstream->runtime;
	soc_prtd = cstream->private_data;
	if (!runtime || !soc_prtd) {
		pr_err("%s runtime or soc_prtd is null\n", __func__);
		return 0;
	}
	component = snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	if (!component) {
		pr_err("%s component is null\n", __func__);
		return 0;
	}

	prtd = runtime->private_data;
	if (!prtd) {
		pr_err("%s prtd is null\n", __func__);
		return 0;
	}
	pdata = snd_soc_component_get_drvdata(component);
	ac = prtd->audio_client;
	if (!pdata || !ac) {
		pr_err("%s pdata or ac is null\n", __func__);
		return 0;
	}

	spin_lock_irqsave(&prtd->lock, flags);
	stream_id = ac->stream_id;

	stream_index = STREAM_ARRAY_INDEX(stream_id);
	if (stream_index < MAX_NUMBER_OF_STREAMS) {
		spin_unlock_irqrestore(&prtd->lock, flags);
		pr_debug("close stream %d", stream_id);
		q6asm_stream_cmd(ac, CMD_CLOSE, stream_id);
		spin_lock_irqsave(&prtd->lock, flags);
	}
	spin_unlock_irqrestore(&prtd->lock, flags);

	mutex_lock(&pdata->lock);
	pdata->cstream[soc_prtd->dai_link->id] = NULL;
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->id,
					SNDRV_PCM_STREAM_CAPTURE);

	q6asm_audio_client_buf_free_contiguous(dir, ac);

	q6asm_audio_client_free(ac);

	kfree(prtd);
	runtime->private_data = NULL;
	mutex_unlock(&pdata->lock);

	return 0;
}

static int msm_compr_free(struct snd_compr_stream *cstream)
{
	int ret = 0;

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
		ret = msm_compr_playback_free(cstream);
	else if (cstream->direction == SND_COMPRESS_CAPTURE)
		ret = msm_compr_capture_free(cstream);
	return ret;
}

static bool msm_compr_validate_codec_compr(__u32 codec_id)
{
	int32_t i;

	for (i = 0; i < ARRAY_SIZE(compr_codecs); i++) {
		if (compr_codecs[i] == codec_id)
			return true;
	}
	return false;
}

/* compress stream operations */
static int msm_compr_set_params(struct snd_compr_stream *cstream,
				struct snd_compr_params *params)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	int ret = 0, frame_sz = 0;
	int i, num_rates;
	bool is_format_gapless = false;

	pr_debug("%s\n", __func__);

	num_rates = sizeof(supported_sample_rates)/sizeof(unsigned int);
	for (i = 0; i < num_rates; i++)
		if (params->codec.sample_rate == supported_sample_rates[i])
			break;
	if (i == num_rates)
		return -EINVAL;

	memcpy(&prtd->codec_param, params, sizeof(struct snd_compr_params));
	/* ToDo: remove duplicates */
	prtd->num_channels = prtd->codec_param.codec.ch_in;
	prtd->sample_rate = prtd->codec_param.codec.sample_rate;
	pr_debug("%s: sample_rate %d\n", __func__, prtd->sample_rate);

	/* prtd->codec_param.codec.reserved[0] is for compr_passthr */
	if ((prtd->codec_param.
	    codec.reserved[0] <= COMPRESSED_PASSTHROUGH_DSD) ||
	    (prtd->codec_param.
	    codec.reserved[0] == COMPRESSED_PASSTHROUGH_IEC61937))
		prtd->compr_passthr = prtd->codec_param.codec.reserved[0];
	else
		prtd->compr_passthr = LEGACY_PCM;
	pr_debug("%s: compr_passthr = %d", __func__, prtd->compr_passthr);
	if (prtd->compr_passthr != LEGACY_PCM) {
		pr_debug("%s: Reset gapless mode playback for compr_type[%d]\n",
			__func__, prtd->compr_passthr);
		prtd->gapless_state.use_dsp_gapless_mode = 0;
		if (!msm_compr_validate_codec_compr(params->codec.id)) {
			pr_err("%s codec not supported in passthrough,id =%d\n",
				 __func__, params->codec.id);
			return -EINVAL;
		}
	}

	/* reserved[1] is for flags */
	if (params->codec.reserved[1] & COMPRESSED_PERF_MODE_FLAG) {
		pr_debug("%s: setting perf mode = %d", __func__, LOW_LATENCY_PCM_MODE);
		prtd->audio_client->perf_mode = LOW_LATENCY_PCM_MODE;
	}

	switch (params->codec.id) {
	case SND_AUDIOCODEC_PCM: {
		pr_debug("SND_AUDIOCODEC_PCM\n");
		prtd->codec = FORMAT_LINEAR_PCM;
		is_format_gapless = true;
		break;
	}

	case SND_AUDIOCODEC_MP3: {
		pr_debug("SND_AUDIOCODEC_MP3\n");
		prtd->codec = FORMAT_MP3;
		frame_sz = MP3_OUTPUT_FRAME_SZ;
		is_format_gapless = true;
		break;
	}

	case SND_AUDIOCODEC_AAC: {
		pr_debug("SND_AUDIOCODEC_AAC\n");
		prtd->codec = FORMAT_MPEG4_AAC;
		frame_sz = AAC_OUTPUT_FRAME_SZ;
		is_format_gapless = true;
		break;
	}

	case SND_AUDIOCODEC_AC3: {
		pr_debug("SND_AUDIOCODEC_AC3\n");
		prtd->codec = FORMAT_AC3;
		frame_sz = AC3_OUTPUT_FRAME_SZ;
		is_format_gapless = true;
		break;
	}

	case SND_AUDIOCODEC_EAC3: {
		pr_debug("SND_AUDIOCODEC_EAC3\n");
		prtd->codec = FORMAT_EAC3;
		frame_sz = EAC3_OUTPUT_FRAME_SZ;
		is_format_gapless = true;
		break;
	}

	case SND_AUDIOCODEC_MP2: {
		pr_debug("SND_AUDIOCODEC_MP2\n");
		prtd->codec = FORMAT_MP2;
		break;
	}


	case SND_AUDIOCODEC_DTS: {
		pr_debug("%s: SND_AUDIOCODEC_DTS\n", __func__);
		prtd->codec = FORMAT_DTS;
		break;
	}

	case SND_AUDIOCODEC_TRUEHD: {
		pr_debug("%s: SND_AUDIOCODEC_TRUEHD\n", __func__);
		prtd->codec = FORMAT_TRUEHD;
		break;
	}

	case SND_AUDIOCODEC_IEC61937: {
		pr_debug("%s: SND_AUDIOCODEC_IEC61937\n", __func__);
		prtd->codec = FORMAT_IEC61937;
		break;
	}

	case SND_AUDIOCODEC_BESPOKE: {
		pr_debug("%s: SND_AUDIOCODEC_BESPOKE\n", __func__);
		ret = msm_compr_get_decoder_format(prtd, &frame_sz,
						&is_format_gapless);
		if (ret < 0)
			return ret;
		prtd->codec = ret;
		break;
	}

	case SND_AUDIOCODEC_AMR: {
		pr_debug("%s:SND_AUDIOCODEC_AMR\n", __func__);
		prtd->codec = FORMAT_AMRNB;
		break;
	}

	case SND_AUDIOCODEC_AMRWB: {
		pr_debug("%s:SND_AUDIOCODEC_AMRWB\n", __func__);
		prtd->codec = FORMAT_AMRWB;
		break;
	}

	case SND_AUDIOCODEC_AMRWBPLUS: {
		pr_debug("%s:SND_AUDIOCODEC_AMRWBPLUS\n", __func__);
		prtd->codec = FORMAT_AMR_WB_PLUS;
		break;
	}

	default:
		pr_err("codec not supported, id =%d\n", params->codec.id);
		return -EINVAL;
	}

	if (!is_format_gapless)
		prtd->gapless_state.use_dsp_gapless_mode = false;

	prtd->partial_drain_delay =
		msm_compr_get_partial_drain_delay(frame_sz, prtd->sample_rate);

	if (cstream->direction == SND_COMPRESS_PLAYBACK)
		ret = msm_compr_configure_dsp_for_playback(cstream);
	else if (cstream->direction == SND_COMPRESS_CAPTURE)
		ret = msm_compr_configure_dsp_for_capture(cstream);

	return ret;
}

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_compr_drain_buffer(struct msm_compr_audio *prtd,
				  unsigned long *flags)
{
	int rc = 0;

	atomic_set(&prtd->drain, 1);
	prtd->drain_ready = 0;
	spin_unlock_irqrestore(&prtd->lock, *flags);
	pr_debug("%s: wait for buffer to be drained\n",  __func__);
	rc = wait_event_interruptible(prtd->drain_wait,
					prtd->drain_ready ||
					prtd->cmd_interrupt ||
					atomic_read(&prtd->xrun) ||
					atomic_read(&prtd->error));
	pr_debug("%s: out of buffer drain wait with ret %d\n", __func__, rc);
	spin_lock_irqsave(&prtd->lock, *flags);
	if (prtd->cmd_interrupt) {
		pr_debug("%s: buffer drain interrupted by flush)\n", __func__);
		rc = -EINTR;
		prtd->cmd_interrupt = 0;
	}
	if (atomic_read(&prtd->error)) {
		pr_err("%s: Got RESET EVENTS notification, return\n",
			__func__);
		rc = -ENETRESET;
	}
	return rc;
}
#endif

static int msm_compr_wait_for_stream_avail(struct msm_compr_audio *prtd,
				    unsigned long *flags)
{
	int rc = 0;

	pr_debug("next session is already in opened state\n");
	prtd->next_stream = 1;
	prtd->cmd_interrupt = 0;
	spin_unlock_irqrestore(&prtd->lock, *flags);
	/*
	 * Wait for stream to be available, or the wait to be interrupted by
	 * commands like flush or till a timeout of one second.
	 */
	rc = wait_event_timeout(prtd->wait_for_stream_avail,
		prtd->stream_available || prtd->cmd_interrupt, 1 * HZ);
	pr_err("%s:prtd->stream_available %d, prtd->cmd_interrupt %d rc %d\n",
		   __func__, prtd->stream_available, prtd->cmd_interrupt, rc);

	spin_lock_irqsave(&prtd->lock, *flags);
	if (rc == 0) {
		pr_err("%s: wait_for_stream_avail timed out\n",
						__func__);
		rc =  -ETIMEDOUT;
	} else if (prtd->cmd_interrupt == 1) {
		/*
		 * This scenario might not happen as we do not allow
		 * flush in transition state.
		 */
		pr_debug("%s: wait_for_stream_avail interrupted\n", __func__);
		prtd->cmd_interrupt = 0;
		prtd->stream_available = 0;
		rc = -EINTR;
	} else {
		prtd->stream_available = 0;
		rc = 0;
	}
	pr_debug("%s : rc = %d",  __func__, rc);
	return rc;
}

static int msm_compr_trigger(struct snd_compr_stream *cstream, int cmd)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct snd_soc_component *component = NULL;
	struct msm_compr_pdata *pdata = NULL;
	uint32_t *volume = NULL;
	struct audio_client *ac = prtd->audio_client;
	unsigned long fe_id = rtd->dai_link->id;
	int rc = 0;
	int bytes_to_write;
	unsigned long flags;
	int stream_id;
	uint32_t stream_index;
	uint16_t bits_per_sample = 16;

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = snd_soc_component_get_drvdata(component);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}
	volume = pdata->volume[rtd->dai_link->id];

	spin_lock_irqsave(&prtd->lock, flags);
	if (atomic_read(&prtd->error)) {
		pr_err("%s Got RESET EVENTS notification, return immediately",
			__func__);
		spin_unlock_irqrestore(&prtd->lock, flags);
		return 0;
	}
	spin_unlock_irqrestore(&prtd->lock, flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		pr_debug("%s: SNDRV_PCM_TRIGGER_START\n", __func__);
		atomic_set(&prtd->start, 1);

		/*
		 * compr_set_volume and compr_init_pp_params
		 * are used to configure ASM volume hence not
		 * needed for compress passthrough playback.
		 *
		 * compress passthrough volume is controlled in
		 * ADM by adm_send_compressed_device_mute()
		 */
		if (prtd->compr_passthr == LEGACY_PCM &&
			cstream->direction == SND_COMPRESS_PLAYBACK) {
			/* set volume for the stream before RUN */
			rc = msm_compr_set_volume(cstream,
				volume[0], volume[1]);
			if (rc)
				pr_err("%s : Set Volume failed : %d\n",
					__func__, rc);

			rc = msm_compr_init_pp_params(cstream, ac);
			if (rc)
				pr_err("%s : init PP params failed : %d\n",
					__func__, rc);
		} else {
			msm_compr_read_buffer(prtd);
		}
		/* issue RUN command for the stream */
		q6asm_run_nowait(prtd->audio_client, prtd->run_mode,
				 prtd->start_delay_msw, prtd->start_delay_lsw);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		spin_lock_irqsave(&prtd->lock, flags);
		pr_debug("%s: SNDRV_PCM_TRIGGER_STOP transition %d\n", __func__,
					prtd->gapless_state.gapless_transition);
		stream_id = ac->stream_id;
		atomic_set(&prtd->start, 0);
		if (cstream->direction == SND_COMPRESS_CAPTURE) {
			q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
			atomic_set(&prtd->xrun, 0);
			prtd->received_total = 0;
			prtd->bytes_copied = 0;
			prtd->bytes_read = 0;
			prtd->bytes_read_offset = 0;
			prtd->byte_offset  = 0;
			prtd->app_pointer  = 0;
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}
		if (prtd->next_stream) {
			pr_debug("%s: interrupt next track wait queues\n",
								__func__);
			prtd->cmd_interrupt = 1;
			wake_up(&prtd->wait_for_stream_avail);
			prtd->next_stream = 0;
		}
		if (atomic_read(&prtd->eos)) {
			pr_debug("%s: interrupt eos wait queues", __func__);
			/*
			 * Gapless playback does not wait for eos, do not set
			 * cmd_int and do not wake up eos_wait during gapless
			 * transition
			 */
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
			if (!prtd->gapless_state.gapless_transition) {
				prtd->cmd_interrupt = 1;
				wake_up(&prtd->eos_wait);
			}
#endif
			atomic_set(&prtd->eos, 0);
		}
		if (atomic_read(&prtd->drain)) {
			pr_debug("%s: interrupt drain wait queues", __func__);
			prtd->cmd_interrupt = 1;
			prtd->drain_ready = 1;
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
			wake_up(&prtd->drain_wait);
#endif
			atomic_set(&prtd->drain, 0);
		}
		prtd->last_buffer = 0;
		prtd->cmd_ack = 0;
		if (!prtd->gapless_state.gapless_transition) {
			pr_debug("issue CMD_FLUSH stream_id %d\n", stream_id);
			spin_unlock_irqrestore(&prtd->lock, flags);
			q6asm_stream_cmd(
				prtd->audio_client, CMD_FLUSH, stream_id);
			spin_lock_irqsave(&prtd->lock, flags);
		} else {
			prtd->first_buffer = 0;
		}
		/* FIXME. only reset if flush was successful */
		prtd->byte_offset  = 0;
		prtd->copied_total = 0;
		prtd->app_pointer  = 0;
		prtd->bytes_received = 0;
		prtd->bytes_sent = 0;
		prtd->marker_timestamp = 0;

		atomic_set(&prtd->xrun, 0);
		spin_unlock_irqrestore(&prtd->lock, flags);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE_PUSH transition %d\n",
				prtd->gapless_state.gapless_transition);
		if (!prtd->gapless_state.gapless_transition) {
			pr_debug("issue CMD_PAUSE stream_id %d\n",
				  ac->stream_id);
			q6asm_stream_cmd_nowait(ac, CMD_PAUSE, ac->stream_id);
			atomic_set(&prtd->start, 0);
		}
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE_RELEASE transition %d\n",
				   prtd->gapless_state.gapless_transition);
		if (!prtd->gapless_state.gapless_transition) {
			atomic_set(&prtd->start, 1);
			q6asm_run_nowait(prtd->audio_client, prtd->run_mode,
					 0, 0);
		}
		break;
	case SND_COMPR_TRIGGER_PARTIAL_DRAIN:
		pr_debug("%s: SND_COMPR_TRIGGER_PARTIAL_DRAIN\n", __func__);
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
		spin_lock_irqsave(&prtd->lock, flags);
		atomic_set(&prtd->partial_drain, 1);
#endif
		if (!prtd->gapless_state.use_dsp_gapless_mode) {
			pr_debug("%s: set partial drain as drain\n", __func__);
			cmd = SND_COMPR_TRIGGER_DRAIN;
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
			atomic_set(&prtd->partial_drain, 0);
#endif
		}
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
		spin_unlock_irqrestore(&prtd->lock, flags);
#endif
	case SND_COMPR_TRIGGER_DRAIN:
		pr_debug("%s: SNDRV_COMPRESS_DRAIN\n", __func__);
		/* Make sure all the data is sent to DSP before sending EOS */
		spin_lock_irqsave(&prtd->lock, flags);

		if (!atomic_read(&prtd->start)) {
			pr_err("%s: stream is not in started state\n",
				__func__);
			rc = -EPERM;
#if !IS_ENABLED(CONFIG_AUDIO_QGKI)
			atomic_set(&prtd->partial_drain, 0);
#endif
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
		if (prtd->bytes_received > prtd->copied_total) {
			pr_debug("%s: wait till all the data is sent to dsp\n",
				__func__);
			rc = msm_compr_drain_buffer(prtd, &flags);
			if (rc || !atomic_read(&prtd->start)) {
				if (rc != -ENETRESET)
					rc = -EINTR;
				spin_unlock_irqrestore(&prtd->lock, flags);
				break;
			}
			/*
			 * FIXME: Bug.
			 * Write(32767)
			 * Start
			 * Drain <- Indefinite wait
			 * sol1 : if (prtd->copied_total) then wait?
			 * sol2 : (prtd->cmd_interrupt || prtd->drain_ready ||
			 *	   atomic_read(xrun)
			 */
			bytes_to_write = prtd->bytes_received
						- prtd->copied_total;
			WARN(bytes_to_write > runtime->fragment_size,
			     "last write %d cannot be > than fragment_size",
			     bytes_to_write);

			if (bytes_to_write > 0) {
				pr_debug("%s: send %d partial bytes at the end",
				       __func__, bytes_to_write);
				atomic_set(&prtd->xrun, 0);
				prtd->last_buffer = 1;
				msm_compr_send_buffer(prtd);
			}
		}

		if ((cmd == SND_COMPR_TRIGGER_PARTIAL_DRAIN) &&
		    (prtd->gapless_state.set_next_stream_id)) {
			/* wait for the last buffer to be returned */

			if (prtd->last_buffer) {
				pr_debug("%s: last buffer drain\n", __func__);
				rc = msm_compr_drain_buffer(prtd, &flags);
				if (rc || !atomic_read(&prtd->start)) {
					spin_unlock_irqrestore(&prtd->lock,
									flags);
					break;
				}
			}
			/* send EOS */
			prtd->eos_ack = 0;
			atomic_set(&prtd->eos, 1);
			pr_debug("issue CMD_EOS stream_id %d\n", ac->stream_id);
			q6asm_stream_cmd_nowait(ac, CMD_EOS, ac->stream_id);
			pr_info("PARTIAL DRAIN, do not wait for EOS ack\n");

			/* send a zero length buffer */
			atomic_set(&prtd->xrun, 0);
			msm_compr_send_buffer(prtd);

			/* wait for the zero length buffer to be returned */
			pr_debug("%s: zero length buffer drain\n", __func__);
			rc = msm_compr_drain_buffer(prtd, &flags);
			if (rc || !atomic_read(&prtd->start)) {
				spin_unlock_irqrestore(&prtd->lock, flags);
				break;
			}

			/* sleep for additional duration partial drain */
			atomic_set(&prtd->drain, 1);
			prtd->drain_ready = 0;
			pr_debug("%s, additional sleep: %d\n", __func__,
				 prtd->partial_drain_delay);
			spin_unlock_irqrestore(&prtd->lock, flags);
			rc = wait_event_timeout(prtd->drain_wait,
				prtd->drain_ready || prtd->cmd_interrupt,
				msecs_to_jiffies(prtd->partial_drain_delay));
			pr_debug("%s: out of additional wait for low sample rate\n",
				 __func__);
			spin_lock_irqsave(&prtd->lock, flags);
			if (prtd->cmd_interrupt) {
				pr_debug("%s: additional wait interrupted by flush)\n",
					 __func__);
				rc = -EINTR;
				prtd->cmd_interrupt = 0;
				spin_unlock_irqrestore(&prtd->lock, flags);
				break;
			}

			/* move to next stream and reset vars */
			pr_debug("%s: Moving to next stream in gapless\n",
								__func__);
			ac->stream_id = NEXT_STREAM_ID(ac->stream_id);
			prtd->byte_offset = 0;
			prtd->app_pointer  = 0;
			prtd->first_buffer = 1;
			prtd->last_buffer = 0;
			/*
			 * Set gapless transition flag only if EOS hasn't been
			 * acknowledged already.
			 */
			if (atomic_read(&prtd->eos))
				prtd->gapless_state.gapless_transition = 1;
			prtd->marker_timestamp = 0;

			/*
			 * Don't reset these as these vars map to
			 * total_bytes_transferred and total_bytes_available
			 * directly, only total_bytes_transferred will be
			 * updated in the next avail() ioctl
			 *	prtd->copied_total = 0;
			 *	prtd->bytes_received = 0;
			 */
			atomic_set(&prtd->drain, 0);
			atomic_set(&prtd->xrun, 1);
			pr_debug("%s: issue CMD_RUN", __func__);
			q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
			spin_unlock_irqrestore(&prtd->lock, flags);
			break;
		}
		/*
		 * moving to next stream failed, so reset the gapless state
		 * set next stream id for the same session so that the same
		 * stream can be used for gapless playback
		 */
		prtd->gapless_state.set_next_stream_id = false;
		prtd->gapless_state.gapless_transition = 0;
		pr_debug("%s:CMD_EOS stream_id %d\n", __func__, ac->stream_id);

		prtd->eos_ack = 0;
		atomic_set(&prtd->eos, 1);
		q6asm_stream_cmd_nowait(ac, CMD_EOS, ac->stream_id);

		spin_unlock_irqrestore(&prtd->lock, flags);


		/* Wait indefinitely for  DRAIN. Flush can also signal this*/
		rc = wait_event_interruptible(prtd->eos_wait,
						(prtd->eos_ack ||
						prtd->cmd_interrupt ||
						atomic_read(&prtd->error)));

		if (rc < 0)
			pr_err("%s: EOS wait failed\n", __func__);

		pr_debug("%s: SNDRV_COMPRESS_DRAIN  out of wait for EOS\n",
			  __func__);

		if (prtd->cmd_interrupt)
			rc = -EINTR;

		if (atomic_read(&prtd->error)) {
			pr_err("%s: Got RESET EVENTS notification, return\n",
				__func__);
			rc = -ENETRESET;
		}

		/*FIXME : what if a flush comes while PC is here */
		if (rc == 0) {
			/*
			 * Failed to open second stream in DSP for gapless
			 * so prepare the current stream in session
			 * for gapless playback
			 */
			spin_lock_irqsave(&prtd->lock, flags);
			pr_debug("%s:issue CMD_PAUSE stream_id %d",
					  __func__, ac->stream_id);
			q6asm_stream_cmd_nowait(ac, CMD_PAUSE, ac->stream_id);
			prtd->cmd_ack = 0;
			spin_unlock_irqrestore(&prtd->lock, flags);

			/*
			 * Cache this time as last known time
			 */
			if (pdata->use_legacy_api)
				q6asm_get_session_time_legacy(
							prtd->audio_client,
						       &prtd->marker_timestamp);
			else
				q6asm_get_session_time(prtd->audio_client,
						       &prtd->marker_timestamp);

			spin_lock_irqsave(&prtd->lock, flags);
			/*
			 * Don't reset these as these vars map to
			 * total_bytes_transferred and total_bytes_available.
			 * Just total_bytes_transferred will be updated
			 * in the next avail() ioctl.
			 * prtd->copied_total = 0;
			 * prtd->bytes_received = 0;
			 * do not reset prtd->bytes_sent as well as the same
			 * session is used for gapless playback
			 */
			prtd->byte_offset = 0;

			prtd->app_pointer  = 0;
			prtd->first_buffer = 1;
			prtd->last_buffer = 0;
			atomic_set(&prtd->drain, 0);
			atomic_set(&prtd->xrun, 1);
			spin_unlock_irqrestore(&prtd->lock, flags);

			pr_debug("%s:issue CMD_FLUSH ac->stream_id %d",
					      __func__, ac->stream_id);
			q6asm_stream_cmd(ac, CMD_FLUSH, ac->stream_id);

			q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		}
#else
		if ((prtd->bytes_received > prtd->copied_total) &&
			(prtd->bytes_received < runtime->fragment_size)) {
			pr_debug("%s: send the only partial buffer to dsp\n",
					__func__);
			bytes_to_write = prtd->bytes_received
						- prtd->copied_total;
			if (bytes_to_write > 0) {
				pr_debug("%s: send %d partial bytes at the end",
						__func__, bytes_to_write);
				atomic_set(&prtd->xrun, 0);
				prtd->last_buffer = 1;
				msm_compr_send_buffer(prtd);
			}
		}

		atomic_set(&prtd->drain, 1);
		spin_unlock_irqrestore(&prtd->lock, flags);
#endif
		prtd->cmd_interrupt = 0;
		break;
	case SND_COMPR_TRIGGER_NEXT_TRACK:
		if (!prtd->gapless_state.use_dsp_gapless_mode) {
			pr_debug("%s: ignore trigger next track\n", __func__);
			rc = 0;
			break;
		}
		pr_debug("%s: SND_COMPR_TRIGGER_NEXT_TRACK\n", __func__);
		spin_lock_irqsave(&prtd->lock, flags);
		rc = 0;
		/* next stream in gapless */
		stream_id = NEXT_STREAM_ID(ac->stream_id);
		/*
		 * Wait if stream 1 has not completed before honoring next
		 * track for stream 3. Scenario happens if second clip is
		 * small and fills in one buffer so next track will be
		 * called immediately.
		 */
		stream_index = STREAM_ARRAY_INDEX(stream_id);
		if (stream_index >= MAX_NUMBER_OF_STREAMS) {
			pr_err("%s: Invalid stream index: %d", __func__,
				stream_index);
			spin_unlock_irqrestore(&prtd->lock, flags);
			rc = -EINVAL;
			break;
		}

		if (prtd->gapless_state.stream_opened[stream_index]) {
			if (prtd->gapless_state.gapless_transition) {
				rc = msm_compr_wait_for_stream_avail(prtd,
								    &flags);
			} else {
				/*
				 * If session is already opened break out if
				 * the state is not gapless transition. This
				 * is when seek happens after the last buffer
				 * is sent to the driver. Next track would be
				 * called again after last buffer is sent.
				 */
				pr_debug("next session is in opened state\n");
				spin_unlock_irqrestore(&prtd->lock, flags);
				break;
			}
		}
		spin_unlock_irqrestore(&prtd->lock, flags);
		if (rc < 0) {
			/*
			 * if return type EINTR  then reset to zero. Tiny
			 * compress treats EINTR as error and prevents PARTIAL
			 * DRAIN. EINTR is not an error. wait for stream avail
			 * is interrupted by some other command like FLUSH.
			 */
			if (rc == -EINTR) {
				pr_debug("%s: EINTR reset rc to 0\n", __func__);
				rc = 0;
			}
			break;
		}

		if (prtd->codec_param.codec.format == SNDRV_PCM_FORMAT_S24_LE)
			bits_per_sample = 24;
		else if (prtd->codec_param.codec.format ==
			 SNDRV_PCM_FORMAT_S32_LE)
			bits_per_sample = 32;

		pr_debug("%s: open_write stream_id %d bits_per_sample %d",
				__func__, stream_id, bits_per_sample);

		prtd->audio_client->fedai_id = (int)fe_id;
		if (q6core_get_avcs_api_version_per_service(
					APRV2_IDS_SERVICE_ID_ADSP_ASM_V) >=
					ADSP_ASM_API_VERSION_V2)
			rc = q6asm_stream_open_write_v5(prtd->audio_client,
				prtd->codec, bits_per_sample,
				stream_id,
				prtd->gapless_state.use_dsp_gapless_mode);
		else
			rc = q6asm_stream_open_write_v4(prtd->audio_client,
				prtd->codec, bits_per_sample,
				stream_id,
				prtd->gapless_state.use_dsp_gapless_mode);
		if (rc < 0) {
			pr_err("%s: Session out open failed for gapless [%d]\n",
				__func__, rc);
			break;
		}

		spin_lock_irqsave(&prtd->lock, flags);
		prtd->gapless_state.stream_opened[stream_index] = 1;
		prtd->gapless_state.set_next_stream_id = true;
		spin_unlock_irqrestore(&prtd->lock, flags);

		rc = msm_compr_send_media_format_block(cstream,
						stream_id, false);
		if (rc < 0) {
			pr_err("%s, failed to send media format block\n",
				__func__);
			break;
		}
		msm_compr_send_dec_params(cstream, pdata->dec_params[fe_id],
					  stream_id);
		break;
	}

	return rc;
}

static int msm_compr_pointer(struct snd_compr_stream *cstream,
					struct snd_compr_tstamp *arg)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct snd_soc_pcm_runtime *rtd = cstream->private_data;
	struct msm_compr_audio *prtd = runtime->private_data;
	struct snd_soc_component *component = NULL;
	struct msm_compr_pdata *pdata = NULL;
	struct snd_compr_tstamp tstamp;
	uint64_t timestamp = 0;
	int rc = 0, first_buffer;
	unsigned long flags;
	uint32_t gapless_transition;

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}
	pdata = snd_soc_component_get_drvdata(component);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s\n", __func__);
	memset(&tstamp, 0x0, sizeof(struct snd_compr_tstamp));

	spin_lock_irqsave(&prtd->lock, flags);
	tstamp.sampling_rate = prtd->sample_rate;
	tstamp.byte_offset = prtd->byte_offset;
	if (cstream->direction == SND_COMPRESS_PLAYBACK) {
		runtime->total_bytes_transferred = prtd->copied_total;
		tstamp.copied_total = prtd->copied_total;
	}
	else if (cstream->direction == SND_COMPRESS_CAPTURE) {
		runtime->total_bytes_available = prtd->received_total;
		tstamp.copied_total = prtd->received_total;
	}
	first_buffer = prtd->first_buffer;
	if (atomic_read(&prtd->error)) {
		pr_err_ratelimited("%s Got RESET EVENTS notification, return error\n",
				   __func__);
		tstamp.pcm_io_frames = 0;
		memcpy(arg, &tstamp, sizeof(struct snd_compr_tstamp));
		spin_unlock_irqrestore(&prtd->lock, flags);
		return -ENETRESET;
	}
	if (cstream->direction == SND_COMPRESS_PLAYBACK) {

		gapless_transition = prtd->gapless_state.gapless_transition;
		spin_unlock_irqrestore(&prtd->lock, flags);
		if (gapless_transition)
			pr_debug("%s session time in gapless transition",
				__func__);
		/*
		 *- Do not query if no buffer has been given.
		 *- Do not query on a gapless transition.
		 *  Playback for the 2nd stream can start (thus returning time
		 *  starting from 0) before the driver knows about EOS of first
		 *  stream.
		 */
		if (!first_buffer || gapless_transition) {

			if (pdata->use_legacy_api)
				rc = q6asm_get_session_time_legacy(
				prtd->audio_client, &prtd->marker_timestamp);
			else
				rc = q6asm_get_session_time(
				prtd->audio_client, &prtd->marker_timestamp);
			if (rc < 0) {
				if (atomic_read(&prtd->error))
					return -ENETRESET;
				else
					return rc;
			}
		}
	} else {
		spin_unlock_irqrestore(&prtd->lock, flags);
	}
	timestamp = prtd->marker_timestamp;

	/* DSP returns timestamp in usec */
	pr_debug("%s: timestamp = %lld usec\n", __func__, timestamp);
	timestamp *= prtd->sample_rate;
	tstamp.pcm_io_frames = (snd_pcm_uframes_t)div64_u64(timestamp, 1000000);
	memcpy(arg, &tstamp, sizeof(struct snd_compr_tstamp));

	return 0;
}

static int msm_compr_ack(struct snd_compr_stream *cstream,
			size_t count)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	void *src, *dstn;
	size_t copy;
	unsigned long flags;

	WARN(1, "This path is untested");
	return -EINVAL;

	pr_debug("%s: count = %zd\n", __func__, count);
	if (!prtd->buffer) {
		pr_err("%s: Buffer is not allocated yet ??\n", __func__);
		return -EINVAL;
	}
	src = runtime->buffer + prtd->app_pointer;
	dstn = prtd->buffer + prtd->app_pointer;
	if (count < prtd->buffer_size - prtd->app_pointer) {
		memcpy(dstn, src, count);
		prtd->app_pointer += count;
	} else {
		copy = prtd->buffer_size - prtd->app_pointer;
		memcpy(dstn, src, copy);
		memcpy(prtd->buffer, runtime->buffer, count - copy);
		prtd->app_pointer = count - copy;
	}

	/*
	 * If the stream is started and all the bytes received were
	 * copied to DSP, the newly received bytes should be
	 * sent right away
	 */
	spin_lock_irqsave(&prtd->lock, flags);

	if (atomic_read(&prtd->start) &&
		prtd->bytes_received == prtd->copied_total) {
		prtd->bytes_received += count;
		msm_compr_send_buffer(prtd);
	} else
		prtd->bytes_received += count;

	spin_unlock_irqrestore(&prtd->lock, flags);

	return 0;
}

static int msm_compr_playback_copy(struct snd_compr_stream *cstream,
				  char __user *buf, size_t count)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	void *dstn;
	size_t copy;
	uint64_t bytes_available = 0;
	unsigned long flags;

	pr_debug("%s: count = %zd\n", __func__, count);
	if (!prtd->buffer) {
		pr_err("%s: Buffer is not allocated yet ??", __func__);
		return 0;
	}

	spin_lock_irqsave(&prtd->lock, flags);
	if (atomic_read(&prtd->error)) {
		pr_err("%s Got RESET EVENTS notification", __func__);
		spin_unlock_irqrestore(&prtd->lock, flags);
		return -ENETRESET;
	}
	spin_unlock_irqrestore(&prtd->lock, flags);

	dstn = prtd->buffer + prtd->app_pointer;
	if (count < prtd->buffer_size - prtd->app_pointer) {
		if (copy_from_user(dstn, buf, count))
			return -EFAULT;
		prtd->app_pointer += count;
	} else {
		copy = prtd->buffer_size - prtd->app_pointer;
		if (copy_from_user(dstn, buf, copy))
			return -EFAULT;
		if (copy_from_user(prtd->buffer, buf + copy, count - copy))
			return -EFAULT;
		prtd->app_pointer = count - copy;
	}

	/*
	 * If stream is started and there has been an xrun,
	 * since the available bytes fits fragment_size, copy the data
	 * right away.
	 */
	spin_lock_irqsave(&prtd->lock, flags);
	prtd->bytes_received += count;
	if (atomic_read(&prtd->start)) {
		if (atomic_read(&prtd->xrun)) {
			pr_debug("%s: in xrun, count = %zd\n", __func__, count);
			bytes_available = prtd->bytes_received -
					  prtd->copied_total;
			if (bytes_available >= runtime->fragment_size) {
				pr_debug("%s: handle xrun, bytes_to_write = %llu\n",
					 __func__, bytes_available);
				atomic_set(&prtd->xrun, 0);
				msm_compr_send_buffer(prtd);
			} /* else not sufficient data */
		} /* writes will continue on the next write_done */
	}

	spin_unlock_irqrestore(&prtd->lock, flags);

	return count;
}

static int msm_compr_capture_copy(struct snd_compr_stream *cstream,
					char __user *buf, size_t count)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	void *source;
	unsigned long flags;

	pr_debug("%s: count = %zd\n", __func__, count);
	if (!prtd->buffer) {
		pr_err("%s: Buffer is not allocated yet ??", __func__);
		return 0;
	}

	spin_lock_irqsave(&prtd->lock, flags);
	if (atomic_read(&prtd->error)) {
		pr_err("%s Got RESET EVENTS notification", __func__);
		spin_unlock_irqrestore(&prtd->lock, flags);
		return -ENETRESET;
	}

	source = prtd->buffer + prtd->app_pointer;
	/* check if we have requested amount of data to copy to user*/
	if (count <= prtd->received_total - prtd->bytes_copied)	{
		spin_unlock_irqrestore(&prtd->lock, flags);
		if (copy_to_user(buf, source, count)) {
			pr_err("copy_to_user failed");
			return -EFAULT;
		}
		spin_lock_irqsave(&prtd->lock, flags);
		prtd->app_pointer += count;
		if (prtd->app_pointer >= prtd->buffer_size)
			prtd->app_pointer -= prtd->buffer_size;
		prtd->bytes_copied += count;
	}
	msm_compr_read_buffer(prtd);

	spin_unlock_irqrestore(&prtd->lock, flags);
	return count;
}

static int msm_compr_copy(struct snd_compr_stream *cstream,
				char __user *buf, size_t count)
{
	int ret = 0;

	pr_debug(" In %s\n", __func__);
	if (cstream->direction == SND_COMPRESS_PLAYBACK)
		ret = msm_compr_playback_copy(cstream, buf, count);
	else if (cstream->direction == SND_COMPRESS_CAPTURE)
		ret = msm_compr_capture_copy(cstream, buf, count);
	return ret;
}

static int msm_compr_get_caps(struct snd_compr_stream *cstream,
				struct snd_compr_caps *arg)
{
	struct snd_compr_runtime *runtime = cstream->runtime;
	struct msm_compr_audio *prtd = runtime->private_data;
	int ret = 0;

	pr_debug("%s\n", __func__);
	if ((arg != NULL) && (prtd != NULL)) {
		memcpy(arg, &prtd->compr_cap, sizeof(struct snd_compr_caps));
	} else {
		ret = -EINVAL;
		pr_err("%s: arg (0x%pK), prtd (0x%pK)\n", __func__, arg, prtd);
	}

	return ret;
}

static int msm_compr_get_codec_caps(struct snd_compr_stream *cstream,
				struct snd_compr_codec_caps *codec)
{
	pr_debug("%s\n", __func__);

	switch (codec->codec) {
	case SND_AUDIOCODEC_MP3:
		codec->num_descriptors = 2;
		codec->descriptor[0].max_ch = 2;
		memcpy(codec->descriptor[0].sample_rates,
		       supported_sample_rates,
		       sizeof(supported_sample_rates));
		codec->descriptor[0].num_sample_rates =
			sizeof(supported_sample_rates)/sizeof(unsigned int);
		codec->descriptor[0].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[0].bit_rate[1] = 128;
		codec->descriptor[0].num_bitrates = 2;
		codec->descriptor[0].profiles = 0;
		codec->descriptor[0].modes = SND_AUDIOCHANMODE_MP3_STEREO;
		codec->descriptor[0].formats = 0;
		break;
	case SND_AUDIOCODEC_AAC:
		codec->num_descriptors = 2;
		codec->descriptor[1].max_ch = 2;
		memcpy(codec->descriptor[1].sample_rates,
		       supported_sample_rates,
		       sizeof(supported_sample_rates));
		codec->descriptor[1].num_sample_rates =
			sizeof(supported_sample_rates)/sizeof(unsigned int);
		codec->descriptor[1].bit_rate[0] = 320; /* 320kbps */
		codec->descriptor[1].bit_rate[1] = 128;
		codec->descriptor[1].num_bitrates = 2;
		codec->descriptor[1].profiles = 0;
		codec->descriptor[1].modes = 0;
		codec->descriptor[1].formats =
			(SND_AUDIOSTREAMFORMAT_MP4ADTS |
				SND_AUDIOSTREAMFORMAT_RAW);
		break;
	case SND_AUDIOCODEC_AC3:
	case SND_AUDIOCODEC_EAC3:
	case SND_AUDIOCODEC_DTS:
	case SND_AUDIOCODEC_TRUEHD:
	case SND_AUDIOCODEC_IEC61937:
	case SND_AUDIOCODEC_BESPOKE:
		break;
	default:
		pr_err("%s: Unsupported audio codec %d\n",
			__func__, codec->codec);
		return -EINVAL;
	}

	return 0;
}

static int msm_compr_set_metadata(struct snd_compr_stream *cstream,
				struct snd_compr_metadata *metadata)
{
	struct msm_compr_audio *prtd;
	struct audio_client *ac;
	pr_debug("%s\n", __func__);

	if (!metadata || !cstream)
		return -EINVAL;

	prtd = cstream->runtime->private_data;
	if (!prtd || !prtd->audio_client) {
		pr_err("%s: prtd or audio client is NULL\n", __func__);
		return -EINVAL;
	}

	if (((metadata->key == SNDRV_COMPRESS_ENCODER_PADDING) ||
	     (metadata->key == SNDRV_COMPRESS_ENCODER_DELAY)) &&
	     (prtd->compr_passthr != LEGACY_PCM)) {
		pr_debug("%s: No trailing silence for compress_type[%d]\n",
			__func__, prtd->compr_passthr);
		return 0;
	}

	ac = prtd->audio_client;
	if (metadata->key == SNDRV_COMPRESS_ENCODER_PADDING) {
		pr_debug("%s, got encoder padding %u",
			 __func__, metadata->value[0]);
		prtd->gapless_state.trailing_samples_drop = metadata->value[0];
	} else if (metadata->key == SNDRV_COMPRESS_ENCODER_DELAY) {
		pr_debug("%s, got encoder delay %u",
			 __func__, metadata->value[0]);
		prtd->gapless_state.initial_samples_drop = metadata->value[0];
	} else if (metadata->key == SNDRV_COMPRESS_RENDER_MODE) {
		return msm_compr_set_render_mode(prtd, metadata->value[0]);
	} else if (metadata->key == SNDRV_COMPRESS_CLK_REC_MODE) {
		return msm_compr_set_clk_rec_mode(ac, metadata->value[0]);
	} else if (metadata->key == SNDRV_COMPRESS_RENDER_WINDOW) {
		return msm_compr_set_render_window(
				ac,
				metadata->value[0],
				metadata->value[1],
				metadata->value[2],
				metadata->value[3]);
	} else if (metadata->key == SNDRV_COMPRESS_START_DELAY) {
		prtd->start_delay_lsw = metadata->value[0];
		prtd->start_delay_msw = metadata->value[1];
	} else if (metadata->key ==
				SNDRV_COMPRESS_ENABLE_ADJUST_SESSION_CLOCK) {
		return msm_compr_enable_adjust_session_clock(ac,
				metadata->value[0]);
	} else if (metadata->key == SNDRV_COMPRESS_ADJUST_SESSION_CLOCK) {
		return msm_compr_adjust_session_clock(ac,
				metadata->value[0],
				metadata->value[1]);
	}

	return 0;
}

static int msm_compr_get_metadata(struct snd_compr_stream *cstream,
				struct snd_compr_metadata *metadata)
{
	struct msm_compr_audio *prtd;
	struct audio_client *ac;
	int ret = -EINVAL;
	uint64_t ses_time = 0, frames = 0, abs_time = 0;
	uint64_t *val = NULL;
	int64_t av_offset = 0;
	int32_t clock_id = -EINVAL;

	pr_debug("%s\n", __func__);

	if (!metadata || !cstream || !cstream->runtime)
		return ret;

	if (metadata->key != SNDRV_COMPRESS_PATH_DELAY &&
	    metadata->key != SNDRV_COMPRESS_DSP_POSITION) {
		pr_err("%s, unsupported key %d\n", __func__, metadata->key);
		return ret;
	}

	prtd = cstream->runtime->private_data;
	if (!prtd || !prtd->audio_client) {
		pr_err("%s: prtd or audio client is NULL\n", __func__);
		return ret;
	}

	switch (metadata->key) {
	case SNDRV_COMPRESS_PATH_DELAY:
		ac = prtd->audio_client;
		ret = q6asm_get_path_delay(prtd->audio_client);
		if (ret) {
			pr_err("%s: get_path_delay failed, ret=%d\n",
				__func__, ret);
			return ret;
		}

		pr_debug("%s, path delay(in us) %u\n", __func__,
			 ac->path_delay);
		metadata->value[0] = ac->path_delay;
		break;
	case SNDRV_COMPRESS_DSP_POSITION:
		clock_id = metadata->value[0];
		pr_debug("%s, clock_id %d\n", __func__, clock_id);
		ret = q6asm_get_session_time_v2(prtd->audio_client,
						&ses_time, &abs_time);
		if (ret) {
			pr_err("%s: q6asm_get_session_time_v2 failed, ret=%d\n",
				__func__, ret);
			return ret;
		}
		frames = div64_u64((ses_time * prtd->sample_rate), 1000000);

		ret = avcs_core_query_timer_offset(&av_offset, clock_id);
		if (ret) {
			pr_err("%s: avcs query failed, ret=%d\n",
				__func__, ret);
			return ret;
		}

		val = (uint64_t *) &metadata->value[1];
		val[0] = frames;
		val[1] = abs_time + av_offset;
		pr_debug("%s, vals frames %lld, time %lld, avoff %lld, abst %lld, sess_time %llu sr %d\n",
			 __func__, val[0], val[1], av_offset, abs_time,
			 ses_time, prtd->sample_rate);
		break;
	default:
		pr_err("%s, unsupported key %d\n", __func__, metadata->key);
		break;
	}
	return ret;
}


#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_compr_set_next_track_param(struct snd_compr_stream *cstream,
				union snd_codec_options *codec_options)
{
	struct msm_compr_audio *prtd;
	struct audio_client *ac;
	int ret = 0;

	if (!codec_options || !cstream)
		return -EINVAL;

	prtd = cstream->runtime->private_data;
	if (!prtd || !prtd->audio_client) {
		pr_err("%s: prtd or audio client is NULL\n", __func__);
		return -EINVAL;
	}

	ac = prtd->audio_client;

	pr_debug("%s: got codec options for codec type %u",
		__func__, prtd->codec);
	switch (prtd->codec) {
	case FORMAT_WMA_V9:
	case FORMAT_WMA_V10PRO:
	case FORMAT_FLAC:
	case FORMAT_VORBIS:
	case FORMAT_ALAC:
	case FORMAT_APE:
	case FORMAT_AMRNB:
	case FORMAT_AMRWB:
	case FORMAT_AMR_WB_PLUS:
		memcpy(&(prtd->gapless_state.codec_options),
			codec_options,
			sizeof(union snd_codec_options));
		ret = msm_compr_send_media_format_block(cstream,
						ac->stream_id, true);
		if (ret < 0) {
			pr_err("%s: failed to send media format block\n",
				__func__);
		}
		break;

	default:
		pr_debug("%s: Ignore sending CMD Format block\n",
			__func__);
		break;
	}

	return ret;
}
#endif /* CONFIG_AUDIO_QGKI */

static int msm_compr_volume_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	struct snd_compr_stream *cstream = NULL;
	uint32_t *volume = NULL;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	cstream = pdata->cstream[fe_id];
	volume = pdata->volume[fe_id];

	volume[0] = ucontrol->value.integer.value[0];
	volume[1] = ucontrol->value.integer.value[1];
	pr_debug("%s: fe_id %lu left_vol %d right_vol %d\n",
		 __func__, fe_id, volume[0], volume[1]);
	if (cstream)
		msm_compr_set_volume(cstream, volume[0], volume[1]);
	return 0;
}

static int msm_compr_volume_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;

	struct msm_compr_pdata *pdata =
		snd_soc_component_get_drvdata(comp);
	uint32_t *volume = NULL;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bound fe_id %lu\n", __func__, fe_id);
		return -EINVAL;
	}

	volume = pdata->volume[fe_id];
	pr_debug("%s: fe_id %lu\n", __func__, fe_id);
	ucontrol->value.integer.value[0] = volume[0];
	ucontrol->value.integer.value[1] = volume[1];

	return 0;
}

static int msm_compr_audio_effects_config_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	struct msm_compr_audio_effects *audio_effects = NULL;
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd = NULL;
	long *values = &(ucontrol->value.integer.value[0]);
	int ret = 0;
	int effects_module;

	pr_debug("%s\n", __func__);
	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	cstream = pdata->cstream[fe_id];
	audio_effects = pdata->audio_effects[fe_id];
	if (!cstream || !audio_effects) {
		pr_err("%s: stream or effects inactive\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: cannot set audio effects\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	if (prtd->compr_passthr != LEGACY_PCM) {
		pr_debug("%s: No effects for compr_type[%d]\n",
			__func__, prtd->compr_passthr);
		goto done;
	}
	pr_debug("%s: Effects supported for compr_type[%d]\n",
		 __func__, prtd->compr_passthr);

	effects_module = *values++;
	switch (effects_module) {
	case VIRTUALIZER_MODULE:
		pr_debug("%s: VIRTUALIZER_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(effects_module,
						prtd->audio_client->topology))
			msm_audio_effects_virtualizer_handler(
						prtd->audio_client,
						&(audio_effects->virtualizer),
						values);
		break;
	case REVERB_MODULE:
		pr_debug("%s: REVERB_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(effects_module,
						prtd->audio_client->topology))
			msm_audio_effects_reverb_handler(prtd->audio_client,
						 &(audio_effects->reverb),
						 values);
		break;
	case BASS_BOOST_MODULE:
		pr_debug("%s: BASS_BOOST_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(effects_module,
						prtd->audio_client->topology))
			msm_audio_effects_bass_boost_handler(prtd->audio_client,
						   &(audio_effects->bass_boost),
						     values);
		break;
	case PBE_MODULE:
		pr_debug("%s: PBE_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(effects_module,
						prtd->audio_client->topology))
			msm_audio_effects_pbe_handler(prtd->audio_client,
						   &(audio_effects->pbe),
						     values);
		break;
	case EQ_MODULE:
		pr_debug("%s: EQ_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(effects_module,
						prtd->audio_client->topology))
			msm_audio_effects_popless_eq_handler(prtd->audio_client,
						    &(audio_effects->equalizer),
						     values);
		break;
	case SOFT_VOLUME_MODULE:
		pr_debug("%s: SOFT_VOLUME_MODULE\n", __func__);
		break;
	case SOFT_VOLUME2_MODULE:
		pr_debug("%s: SOFT_VOLUME2_MODULE\n", __func__);
		if (msm_audio_effects_is_effmodule_supp_in_top(effects_module,
						prtd->audio_client->topology))
			msm_audio_effects_volume_handler_v2(prtd->audio_client,
						&(audio_effects->volume),
						values, SOFT_VOLUME_INSTANCE_2);
		break;
	default:
		pr_err("%s Invalid effects config module\n", __func__);
		ret = -EINVAL;
	}
done:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_compr_audio_effects_config_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	struct msm_compr_audio_effects *audio_effects = NULL;
	struct snd_compr_stream *cstream = NULL;
	int ret = 0;
	struct msm_compr_audio *prtd = NULL;

	pr_debug("%s\n", __func__);
	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	cstream = pdata->cstream[fe_id];
	audio_effects = pdata->audio_effects[fe_id];
	if (!cstream || !audio_effects) {
		pr_debug("%s: stream or effects inactive\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: cannot set audio effects\n", __func__);
		ret = -EINVAL;
	}
done:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_compr_query_audio_effect_put(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	struct msm_compr_audio_effects *audio_effects = NULL;
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd = NULL;
	int ret = 0;
	long *values = &(ucontrol->value.integer.value[0]);

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);

	cstream = pdata->cstream[fe_id];
	audio_effects = pdata->audio_effects[fe_id];
	if (!cstream || !audio_effects) {
		pr_err("%s: stream or effects inactive\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: cannot set audio effects\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	if (prtd->compr_passthr != LEGACY_PCM) {
		pr_err("%s: No effects for compr_type[%d]\n",
			__func__, prtd->compr_passthr);
		ret = -EPERM;
		goto done;
	}
	audio_effects->query.mod_id = (u32)*values++;
	audio_effects->query.parm_id = (u32)*values++;
	audio_effects->query.size = (u32)*values++;
	audio_effects->query.offset = (u32)*values++;
	audio_effects->query.device = (u32)*values++;

done:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_compr_query_audio_effect_get(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	struct msm_compr_audio_effects *audio_effects = NULL;
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd = NULL;
	int ret = 0;
	long *values = &(ucontrol->value.integer.value[0]);

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	cstream = pdata->cstream[fe_id];
	audio_effects = pdata->audio_effects[fe_id];
	if (!cstream || !audio_effects) {
		pr_debug("%s: stream or effects inactive\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: cannot set audio effects\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	values[0] = (long)audio_effects->query.mod_id;
	values[1] = (long)audio_effects->query.parm_id;
	values[2] = (long)audio_effects->query.size;
	values[3] = (long)audio_effects->query.offset;
	values[4] = (long)audio_effects->query.device;
done:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_compr_send_dec_params(struct snd_compr_stream *cstream,
				     struct msm_compr_dec_params *dec_params,
				     int stream_id)
{

	int rc = 0;
	struct msm_compr_audio *prtd = NULL;
	struct snd_dec_ddp *ddp = &dec_params->ddp_params;

	if (!cstream || !dec_params) {
		pr_err("%s: stream or dec_params inactive\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: cannot set dec_params\n", __func__);
		rc = -EINVAL;
		goto end;
	}
	switch (prtd->codec) {
	case FORMAT_MP3:
	case FORMAT_MPEG4_AAC:
	case FORMAT_TRUEHD:
	case FORMAT_IEC61937:
	case FORMAT_APTX:
		pr_debug("%s: no runtime parameters for codec: %d\n", __func__,
			 prtd->codec);
		break;
	case FORMAT_AC3:
	case FORMAT_EAC3:
		if (prtd->compr_passthr != LEGACY_PCM) {
			pr_debug("%s: No DDP param for compr_type[%d]\n",
				 __func__, prtd->compr_passthr);
			break;
		}
		rc = msm_compr_send_ddp_cfg(prtd->audio_client, ddp, stream_id);
		if (rc < 0)
			pr_err("%s: DDP CMD CFG failed %d\n", __func__, rc);
		break;
	default:
		break;
	}
end:
	return rc;

}
static int msm_compr_dec_params_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	struct msm_compr_dec_params *dec_params = NULL;
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd = NULL;
	long *values = &(ucontrol->value.integer.value[0]);
	int rc = 0;

	pr_debug("%s\n", __func__);
	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	cstream = pdata->cstream[fe_id];
	dec_params = pdata->dec_params[fe_id];

	if (!cstream || !dec_params) {
		pr_err("%s: stream or dec_params inactive\n", __func__);
		return -EINVAL;
	}
	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: cannot set dec_params\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	switch (prtd->codec) {
	case FORMAT_MP3:
	case FORMAT_MPEG4_AAC:
	case FORMAT_FLAC:
	case FORMAT_VORBIS:
	case FORMAT_ALAC:
	case FORMAT_APE:
	case FORMAT_DTS:
	case FORMAT_DSD:
	case FORMAT_TRUEHD:
	case FORMAT_IEC61937:
	case FORMAT_APTX:
	case FORMAT_AMRNB:
	case FORMAT_AMRWB:
	case FORMAT_AMR_WB_PLUS:
		pr_debug("%s: no runtime parameters for codec: %d\n", __func__,
			 prtd->codec);
		break;
	case FORMAT_AC3:
	case FORMAT_EAC3: {
		struct snd_dec_ddp *ddp = &dec_params->ddp_params;
		int cnt;

		if (prtd->compr_passthr != LEGACY_PCM) {
			pr_debug("%s: No DDP param for compr_type[%d]\n",
				__func__, prtd->compr_passthr);
			break;
		}

		ddp->params_length = (*values++);
		if (ddp->params_length > DDP_DEC_MAX_NUM_PARAM) {
			pr_err("%s: invalid num of params:: %d\n", __func__,
				ddp->params_length);
			rc = -EINVAL;
			goto end;
		}
		for (cnt = 0; cnt < ddp->params_length; cnt++) {
			ddp->params_id[cnt] = *values++;
			ddp->params_value[cnt] = *values++;
		}
		prtd = cstream->runtime->private_data;
		if (prtd && prtd->audio_client)
			rc = msm_compr_send_dec_params(cstream, dec_params,
						prtd->audio_client->stream_id);
		break;
	}
	default:
		break;
	}
end:
	pr_debug("%s: ret %d\n", __func__, rc);
	mutex_unlock(&pdata->lock);
	return rc;
}

static int msm_compr_dec_params_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	/* dummy function */
	return 0;
}

static int msm_compr_playback_app_type_cfg_put(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_RX;
	int be_id = ucontrol->value.integer.value[3];
	struct msm_pcm_stream_app_type_cfg cfg_data = {0, 0, 48000};
	int ret = 0;

	cfg_data.app_type = ucontrol->value.integer.value[0];
	cfg_data.acdb_dev_id = ucontrol->value.integer.value[1];
	if (ucontrol->value.integer.value[2] != 0)
		cfg_data.sample_rate = ucontrol->value.integer.value[2];
	pr_debug("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
	ret = msm_pcm_routing_reg_stream_app_type_cfg(fe_id, session_type,
						      be_id, &cfg_data);
	if (ret < 0)
		pr_err("%s: msm_pcm_routing_reg_stream_app_type_cfg failed returned %d\n",
			__func__, ret);

	return ret;
}

static int msm_compr_playback_app_type_cfg_get(struct snd_kcontrol *kcontrol,
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
	pr_debug("%s: fedai_id %llu, session_type %d, be_id %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
done:
	return ret;
}

static int msm_compr_capture_app_type_cfg_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value;
	int session_type = SESSION_TYPE_TX;
	int be_id = ucontrol->value.integer.value[3];
	struct msm_pcm_stream_app_type_cfg cfg_data = {0, 0, 48000};
	int ret = 0;

	cfg_data.app_type = ucontrol->value.integer.value[0];
	cfg_data.acdb_dev_id = ucontrol->value.integer.value[1];
	if (ucontrol->value.integer.value[2] != 0)
		cfg_data.sample_rate = ucontrol->value.integer.value[2];
	pr_debug("%s: fe_id- %llu session_type- %d be_id- %d app_type- %d acdb_dev_id- %d sample_rate- %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
	ret = msm_pcm_routing_reg_stream_app_type_cfg(fe_id, session_type,
						      be_id, &cfg_data);
	if (ret < 0)
		pr_err("%s: msm_pcm_routing_reg_stream_app_type_cfg failed returned %d\n",
			__func__, ret);

	return ret;
}

static int msm_compr_capture_app_type_cfg_get(struct snd_kcontrol *kcontrol,
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
	pr_debug("%s: fedai_id %llu, session_type %d, be_id %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
done:
	return ret;
}

static int msm_compr_channel_map_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	u64 fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	int rc = 0, i;
	struct msm_pcm_channel_mixer *chmixer_pspd = NULL;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);

	if (fe_id >= MSM_FRONTEND_DAI_MM_SIZE) {
		pr_err("%s Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		rc = -EINVAL;
		goto end;
	}

	if (pdata->ch_map[fe_id]) {
		pdata->ch_map[fe_id]->set_ch_map = true;
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
			pdata->ch_map[fe_id]->channel_map[i] =
				(char)(ucontrol->value.integer.value[i]);

		/* update chmixer_pspd chmap cached with routing driver as well */
		chmixer_pspd = pdata->chmixer_pspd[fe_id];
		if (chmixer_pspd && chmixer_pspd->enable) {
			for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
				chmixer_pspd->in_ch_map[i] =
					pdata->ch_map[fe_id]->channel_map[i];
			chmixer_pspd->override_in_ch_map = true;
			msm_pcm_routing_set_channel_mixer_cfg(fe_id,
					SESSION_TYPE_RX, chmixer_pspd);
		}
	} else {
		pr_debug("%s: no memory for ch_map, default will be set\n",
			__func__);
	}
end:
	pr_debug("%s: ret %d\n", __func__, rc);
	return rc;
}

static int msm_compr_channel_map_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	u64 fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
			snd_soc_component_get_drvdata(comp);
	int rc = 0, i;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);
	if (fe_id >= MSM_FRONTEND_DAI_MM_SIZE) {
		pr_err("%s: Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		rc = -EINVAL;
		goto end;
	}
	if (pdata->ch_map[fe_id]) {
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
			ucontrol->value.integer.value[i] =
				pdata->ch_map[fe_id]->channel_map[i];
	}
end:
	pr_debug("%s: ret %d\n", __func__, rc);
	return rc;
}

static int msm_compr_adsp_stream_cmd_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
				snd_soc_component_get_drvdata(comp);
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd;
	int ret = 0;
	struct msm_adsp_event_data *event_data = NULL;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received invalid fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	cstream = pdata->cstream[fe_id];
	if (cstream == NULL) {
		pr_err("%s cstream is null\n", __func__);
		return -EINVAL;
	}

	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: prtd is null\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	if (prtd->audio_client == NULL) {
		pr_err("%s: audio_client is null\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	event_data = (struct msm_adsp_event_data *)ucontrol->value.bytes.data;
	if (event_data->event_type >= ADSP_STREAM_EVENT_MAX) {
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

static int msm_compr_ion_fd_map_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
				snd_soc_component_get_drvdata(comp);
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd;
	int fd;
	int ret = 0;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds invalid fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	cstream = pdata->cstream[fe_id];
	if (cstream == NULL) {
		pr_err("%s cstream is null\n", __func__);
		return -EINVAL;
	}

	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: prtd is null\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	if (prtd->audio_client == NULL) {
		pr_err("%s: audio_client is null\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	memcpy(&fd, ucontrol->value.bytes.data, sizeof(fd));
	ret = q6asm_send_ion_fd(prtd->audio_client, fd);
	if (ret < 0)
		pr_err("%s: failed to register ion fd\n", __func__);
done:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_compr_rtic_event_ack_put(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	unsigned long fe_id = kcontrol->private_value;
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd;
	int ret = 0;
	int param_length = 0;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received invalid fe_id %lu\n",
			__func__, fe_id);
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	cstream = pdata->cstream[fe_id];
	if (cstream == NULL) {
		pr_err("%s cstream is null\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	prtd = cstream->runtime->private_data;
	if (!prtd) {
		pr_err("%s: prtd is null\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	if (prtd->audio_client == NULL) {
		pr_err("%s: audio_client is null\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	memcpy(&param_length, ucontrol->value.bytes.data,
		sizeof(param_length));
	if ((param_length + sizeof(param_length))
		>= sizeof(ucontrol->value.bytes.data)) {
		pr_err("%s param length=%d  exceeds limit",
			__func__, param_length);
		ret = -EINVAL;
		goto done;
	}

	ret = q6asm_send_rtic_event_ack(prtd->audio_client,
			ucontrol->value.bytes.data + sizeof(param_length),
			param_length);
	if (ret < 0)
		pr_err("%s: failed to send rtic event ack, err = %d\n",
			__func__, ret);
done:
	mutex_unlock(&pdata->lock);
	return ret;
}

static int msm_compr_gapless_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
		snd_soc_component_get_drvdata(comp);
	pdata->use_dsp_gapless_mode =  ucontrol->value.integer.value[0];
	pr_debug("%s: value: %ld\n", __func__,
		ucontrol->value.integer.value[0]);

	return 0;
}

static int msm_compr_gapless_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata =
		snd_soc_component_get_drvdata(comp);
	pr_debug("%s:gapless mode %d\n", __func__, pdata->use_dsp_gapless_mode);
	ucontrol->value.integer.value[0] = pdata->use_dsp_gapless_mode;

	return 0;
}

static const struct snd_kcontrol_new msm_compr_gapless_controls[] = {
	SOC_SINGLE_EXT("Compress Gapless Playback",
			0, 0, 1, 0,
			msm_compr_gapless_get,
			msm_compr_gapless_put),
};

static int msm_compr_probe(struct snd_soc_component *component)
{
	struct msm_compr_pdata *pdata;
	int i;
	int rc;
	const char *qdsp_version;

	pr_debug("%s\n", __func__);
	pdata = (struct msm_compr_pdata *) dev_get_drvdata(component->dev);
	if (!pdata) {
		pr_err("%s platform data not set\n", __func__);
		return -EINVAL;
	}

	snd_soc_component_set_drvdata(component, pdata);

	for (i = 0; i < MSM_FRONTEND_DAI_MAX; i++) {
		pdata->volume[i][0] = COMPRESSED_LR_VOL_MAX_STEPS;
		pdata->volume[i][1] = COMPRESSED_LR_VOL_MAX_STEPS;
		pdata->audio_effects[i] = NULL;
		pdata->dec_params[i] = NULL;
		pdata->cstream[i] = NULL;
		pdata->ch_map[i] = NULL;
		pdata->is_in_use[i] = false;
	}

	snd_soc_add_component_controls(component, msm_compr_gapless_controls,
				      ARRAY_SIZE(msm_compr_gapless_controls));

	rc =  of_property_read_string(component->dev->of_node,
		"qcom,adsp-version", &qdsp_version);
	if (!rc) {
		if (!strcmp(qdsp_version, "MDSP 1.2"))
			pdata->use_legacy_api = true;
		else
			pdata->use_legacy_api = false;
	} else
		pdata->use_legacy_api = false;

	pr_debug("%s: use legacy api %d\n", __func__, pdata->use_legacy_api);
	/*
	 * use_dsp_gapless_mode part of platform data(pdata) is updated from HAL
	 * through a mixer control before compress driver is opened. The mixer
	 * control is used to decide if dsp gapless mode needs to be enabled.
	 * Gapless is disabled by default.
	 */
	pdata->use_dsp_gapless_mode = false;
	return 0;
}

static int msm_compr_volume_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = COMPRESSED_LR_VOL_MAX_STEPS;
	return 0;
}

static int msm_compr_audio_effects_config_info(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = MAX_PP_PARAMS_SZ;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xFFFFFFFF;
	return 0;
}

static int msm_compr_query_audio_effect_info(struct snd_kcontrol *kcontrol,
					     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 128;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xFFFFFFFF;
	return 0;
}

static int msm_compr_dec_params_info(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 128;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xFFFFFFFF;
	return 0;
}

static int msm_compr_app_type_cfg_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 5;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xFFFFFFFF;
	return 0;
}

static int msm_compr_channel_map_info(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	uinfo->value.integer.min = 0;
	/* See PCM_MAX_CHANNEL_MAP in apr_audio-v2.h */
	uinfo->value.integer.max = PCM_MAX_CHANNEL_MAP;
	return 0;
}

static int msm_compr_add_volume_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Compress Playback";
	const char *deviceNo       = "NN";
	const char *suffix         = "Volume";
	char *mixer_str = NULL;
	int ctl_len;
	struct snd_kcontrol_new fe_volume_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_TLV_READ |
			  SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_volume_info,
		.tlv.p = msm_compr_vol_gain,
		.get = msm_compr_volume_get,
		.put = msm_compr_volume_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return 0;
	}
	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: added new compr FE with name %s, id %d, cpu dai %s, device no %d\n",
		 __func__, rtd->dai_link->name, rtd->dai_link->id,
		 rtd->dai_link->cpus->dai_name, rtd->pcm->device);
	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1 +
		  strlen(suffix) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);
	if (!mixer_str) {
		pr_err("failed to allocate mixer ctrl str of len %d", ctl_len);
		return 0;
	}
	snprintf(mixer_str, ctl_len, "%s %d %s", mixer_ctl_name,
		 rtd->pcm->device, suffix);
	fe_volume_control[0].name = mixer_str;
	fe_volume_control[0].private_value = rtd->dai_link->id;
	pr_debug("Registering new mixer ctl %s", mixer_str);
	snd_soc_add_component_controls(component, fe_volume_control,
				      ARRAY_SIZE(fe_volume_control));
	kfree(mixer_str);
	return 0;
}

static int msm_compr_add_audio_effects_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Audio Effects Config";
	const char *deviceNo       = "NN";
	char *mixer_str = NULL;
	int ctl_len;
	struct snd_kcontrol_new fe_audio_effects_config_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_audio_effects_config_info,
		.get = msm_compr_audio_effects_config_get,
		.put = msm_compr_audio_effects_config_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return 0;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: added new compr FE with name %s, id %d, cpu dai %s, device no %d\n",
		 __func__, rtd->dai_link->name, rtd->dai_link->id,
		 rtd->dai_link->cpus->dai_name, rtd->pcm->device);

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);

	if (!mixer_str)
		return 0;

	snprintf(mixer_str, ctl_len, "%s %d", mixer_ctl_name, rtd->pcm->device);

	fe_audio_effects_config_control[0].name = mixer_str;
	fe_audio_effects_config_control[0].private_value = rtd->dai_link->id;
	pr_debug("Registering new mixer ctl %s\n", mixer_str);
	snd_soc_add_component_controls(component,
				fe_audio_effects_config_control,
				ARRAY_SIZE(fe_audio_effects_config_control));
	kfree(mixer_str);
	return 0;
}

static int msm_compr_add_query_audio_effect_control(
					struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Query Audio Effect Param";
	const char *deviceNo       = "NN";
	char *mixer_str = NULL;
	int ctl_len;
	struct snd_kcontrol_new fe_query_audio_effect_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_query_audio_effect_info,
		.get = msm_compr_query_audio_effect_get,
		.put = msm_compr_query_audio_effect_put,
		.private_value = 0,
		}
	};
	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return 0;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: added new compr FE with name %s, id %d, cpu dai %s, device no %d\n",
		 __func__, rtd->dai_link->name, rtd->dai_link->id,
		 rtd->dai_link->cpus->dai_name, rtd->pcm->device);
	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);
	if (!mixer_str) {
		pr_err("failed to allocate mixer ctrl str of len %d", ctl_len);
		return 0;
	}
	snprintf(mixer_str, ctl_len, "%s %d", mixer_ctl_name, rtd->pcm->device);
	fe_query_audio_effect_control[0].name = mixer_str;
	fe_query_audio_effect_control[0].private_value = rtd->dai_link->id;
	pr_debug("%s: registering new mixer ctl %s\n", __func__, mixer_str);
	snd_soc_add_component_controls(component,
				fe_query_audio_effect_control,
				ARRAY_SIZE(fe_query_audio_effect_control));
	kfree(mixer_str);
	return 0;
}

static int msm_compr_add_audio_adsp_stream_cmd_control(
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
		.put = msm_compr_adsp_stream_cmd_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return -EINVAL;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);
	if (!mixer_str)
		return -ENOMEM;

	snprintf(mixer_str, ctl_len, "%s %d", mixer_ctl_name, rtd->pcm->device);
	fe_audio_adsp_stream_cmd_config_control[0].name = mixer_str;
	fe_audio_adsp_stream_cmd_config_control[0].private_value =
				rtd->dai_link->id;
	pr_debug("%s: Registering new mixer ctl %s\n", __func__, mixer_str);
	ret = snd_soc_add_component_controls(component,
		fe_audio_adsp_stream_cmd_config_control,
		ARRAY_SIZE(fe_audio_adsp_stream_cmd_config_control));
	if (ret < 0)
		pr_err("%s: failed to add ctl %s. err = %d\n",
			__func__, mixer_str, ret);

	kfree(mixer_str);
	return ret;
}

static int msm_compr_add_audio_adsp_stream_callback_control(
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
		pr_err("%s: rtd is  NULL\n", __func__);
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

static int msm_compr_add_dec_runtime_params_control(
						struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name	= "Audio Stream";
	const char *deviceNo		= "NN";
	const char *suffix		= "Dec Params";
	char *mixer_str = NULL;
	int ctl_len;
	struct snd_kcontrol_new fe_dec_params_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_dec_params_info,
		.get = msm_compr_dec_params_get,
		.put = msm_compr_dec_params_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return 0;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: added new compr FE with name %s, id %d, cpu dai %s, device no %d\n",
		 __func__, rtd->dai_link->name, rtd->dai_link->id,
		 rtd->dai_link->cpus->dai_name, rtd->pcm->device);

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1 +
		  strlen(suffix) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);

	if (!mixer_str)
		return 0;

	snprintf(mixer_str, ctl_len, "%s %d %s", mixer_ctl_name,
		 rtd->pcm->device, suffix);

	fe_dec_params_control[0].name = mixer_str;
	fe_dec_params_control[0].private_value = rtd->dai_link->id;
	pr_debug("Registering new mixer ctl %s", mixer_str);
	snd_soc_add_component_controls(component,
				      fe_dec_params_control,
				      ARRAY_SIZE(fe_dec_params_control));
	kfree(mixer_str);
	return 0;
}

static int msm_compr_add_app_type_cfg_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *playback_mixer_ctl_name	= "Audio Stream";
	const char *capture_mixer_ctl_name	= "Audio Stream Capture";
	const char *deviceNo		= "NN";
	const char *suffix		= "App Type Cfg";
	char *mixer_str = NULL;
	int ctl_len;
	struct snd_kcontrol_new fe_app_type_cfg_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_app_type_cfg_info,
		.put = msm_compr_playback_app_type_cfg_put,
		.get = msm_compr_playback_app_type_cfg_get,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return 0;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: added new compr FE ctl with name %s, id %d, cpu dai %s, device no %d\n",
		__func__, rtd->dai_link->name, rtd->dai_link->id,
			rtd->dai_link->cpus->dai_name, rtd->pcm->device);
	if (rtd->compr->direction == SND_COMPRESS_PLAYBACK)
		ctl_len = strlen(playback_mixer_ctl_name) + 1 + strlen(deviceNo)
			 + 1 + strlen(suffix) + 1;
	else
		ctl_len = strlen(capture_mixer_ctl_name) + 1 + strlen(deviceNo)
			+ 1 + strlen(suffix) + 1;

	mixer_str = kzalloc(ctl_len, GFP_KERNEL);

	if (!mixer_str)
		return 0;

	if (rtd->compr->direction == SND_COMPRESS_PLAYBACK)
		snprintf(mixer_str, ctl_len, "%s %d %s",
			 playback_mixer_ctl_name, rtd->pcm->device, suffix);
	else
		snprintf(mixer_str, ctl_len, "%s %d %s",
			 capture_mixer_ctl_name, rtd->pcm->device, suffix);

	fe_app_type_cfg_control[0].name = mixer_str;
	fe_app_type_cfg_control[0].private_value = rtd->dai_link->id;

	if (rtd->compr->direction == SND_COMPRESS_PLAYBACK) {
		fe_app_type_cfg_control[0].put =
					 msm_compr_playback_app_type_cfg_put;
		fe_app_type_cfg_control[0].get =
					 msm_compr_playback_app_type_cfg_get;
	} else {
		fe_app_type_cfg_control[0].put =
					 msm_compr_capture_app_type_cfg_put;
		fe_app_type_cfg_control[0].get =
					 msm_compr_capture_app_type_cfg_get;
	}
	pr_debug("Registering new mixer ctl %s", mixer_str);
	snd_soc_add_component_controls(component,
				fe_app_type_cfg_control,
				ARRAY_SIZE(fe_app_type_cfg_control));
	kfree(mixer_str);
	return 0;
}

static int msm_compr_add_channel_map_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Playback Channel Map";
	const char *deviceNo       = "NN";
	char *mixer_str = NULL;
	struct msm_compr_pdata *pdata = NULL;
	int ctl_len;
	struct snd_kcontrol_new fe_channel_map_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_channel_map_info,
		.get = msm_compr_channel_map_get,
		.put = msm_compr_channel_map_put,
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

	pr_debug("%s: added new compr FE with name %s, id %d, cpu dai %s, device no %d\n",
		 __func__, rtd->dai_link->name, rtd->dai_link->id,
		 rtd->dai_link->cpus->dai_name, rtd->pcm->device);

	ctl_len = strlen(mixer_ctl_name) + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);

	if (!mixer_str)
		return -ENOMEM;

	snprintf(mixer_str, ctl_len, "%s%d", mixer_ctl_name, rtd->pcm->device);

	fe_channel_map_control[0].name = mixer_str;
	fe_channel_map_control[0].private_value = rtd->dai_link->id;
	pr_debug("%s: Registering new mixer ctl %s\n", __func__, mixer_str);
	snd_soc_add_component_controls(component,
				fe_channel_map_control,
				ARRAY_SIZE(fe_channel_map_control));

	pdata = snd_soc_component_get_drvdata(component);
	pdata->ch_map[rtd->dai_link->id] =
		 kzalloc(sizeof(struct msm_compr_ch_map), GFP_KERNEL);
	if (!pdata->ch_map[rtd->dai_link->id]) {
		pr_err("%s: Could not allocate memory for channel map\n",
			__func__);
		kfree(mixer_str);
		return -ENOMEM;
	}
	kfree(mixer_str);
	return 0;
}

static int msm_compr_add_io_fd_cmd_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Playback ION FD";
	const char *deviceNo = "NN";
	char *mixer_str = NULL;
	int ctl_len = 0, ret = 0;
	struct snd_kcontrol_new fe_ion_fd_config_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_adsp_stream_cmd_info,
		.put = msm_compr_ion_fd_map_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return -EINVAL;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);
	if (!mixer_str)
		return -ENOMEM;

	snprintf(mixer_str, ctl_len, "%s %d", mixer_ctl_name, rtd->pcm->device);
	fe_ion_fd_config_control[0].name = mixer_str;
	fe_ion_fd_config_control[0].private_value = rtd->dai_link->id;
	pr_debug("%s: Registering new mixer ctl %s\n", __func__, mixer_str);
	ret = snd_soc_add_component_controls(component,
				fe_ion_fd_config_control,
				ARRAY_SIZE(fe_ion_fd_config_control));
	if (ret < 0)
		pr_err("%s: failed to add ctl %s\n", __func__, mixer_str);

	kfree(mixer_str);
	return ret;
}

static int msm_compr_add_event_ack_cmd_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Playback Event Ack";
	const char *deviceNo = "NN";
	char *mixer_str = NULL;
	int ctl_len = 0, ret = 0;
	struct snd_kcontrol_new fe_event_ack_config_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_adsp_stream_cmd_info,
		.put = msm_compr_rtic_event_ack_put,
		.private_value = 0,
		}
	};

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return -EINVAL;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);
	if (!mixer_str)
		return -ENOMEM;

	snprintf(mixer_str, ctl_len, "%s %d", mixer_ctl_name, rtd->pcm->device);
	fe_event_ack_config_control[0].name = mixer_str;
	fe_event_ack_config_control[0].private_value = rtd->dai_link->id;
	pr_debug("%s: Registering new mixer ctl %s\n", __func__, mixer_str);
	ret = snd_soc_add_component_controls(component,
				fe_event_ack_config_control,
				ARRAY_SIZE(fe_event_ack_config_control));
	if (ret < 0)
		pr_err("%s: failed to add ctl %s\n", __func__, mixer_str);

	kfree(mixer_str);
	return ret;
}

static struct msm_pcm_channel_mixer *msm_compr_get_chmixer(
			struct msm_compr_pdata *pdata, u64 fe_id)
{
	if (!pdata) {
		pr_err("%s: missing pdata\n", __func__);
		return NULL;
	}

	if (fe_id >= MSM_FRONTEND_DAI_MM_SIZE) {
		pr_err("%s: invalid FE %llu\n", __func__, fe_id);
		return NULL;
	}

	return pdata->chmixer_pspd[fe_id];
}

static int msm_compr_channel_mixer_cfg_ctl_put(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int session_type = (kcontrol->private_value >> 8) & 0xFF;
	int ret = 0, i = 0, stream_id = 0, be_id = 0;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct snd_compr_stream *cstream = NULL;
	struct msm_compr_audio *prtd = NULL;
	struct msm_pcm_channel_mixer *chmixer_pspd = NULL;
	u8 asm_ch_map[PCM_FORMAT_MAX_NUM_CHANNEL_V8] = {0};
	bool reset_override_out_ch_map = false;
	bool reset_override_in_ch_map = false;

	if ((session_type != SESSION_TYPE_TX) &&
		(session_type != SESSION_TYPE_RX)) {
		pr_err("%s: invalid session type %d\n", __func__, session_type);
		return -EINVAL;
	}

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
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

	if (chmixer_pspd->enable) {
		if (session_type == SESSION_TYPE_RX &&
			!chmixer_pspd->override_in_ch_map) {
			if (pdata->ch_map[fe_id]->set_ch_map) {
				for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
					chmixer_pspd->in_ch_map[i] =
						pdata->ch_map[fe_id]->channel_map[i];
			} else {
				q6asm_map_channels(asm_ch_map,
					chmixer_pspd->input_channel, false);
				for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
					chmixer_pspd->in_ch_map[i] = asm_ch_map[i];
			}
			chmixer_pspd->override_in_ch_map = true;
			reset_override_in_ch_map = true;
		} else if (session_type == SESSION_TYPE_TX &&
				!chmixer_pspd->override_out_ch_map) {
			if (pdata->ch_map[fe_id]->set_ch_map) {
				for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
					chmixer_pspd->out_ch_map[i] =
						pdata->ch_map[fe_id]->channel_map[i];
			} else {
				q6asm_map_channels(asm_ch_map,
					chmixer_pspd->output_channel, false);
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

	cstream = pdata->cstream[fe_id];
	if (chmixer_pspd->enable && cstream && cstream->runtime) {
		prtd = cstream->runtime->private_data;

		if (prtd && prtd->audio_client) {
			stream_id = prtd->audio_client->session;
			be_id = chmixer_pspd->port_idx;
			msm_pcm_routing_set_channel_mixer_runtime(be_id,
					stream_id, session_type, chmixer_pspd);
		}
	}

	if (reset_override_out_ch_map)
		chmixer_pspd->override_out_ch_map = false;
	if (reset_override_in_ch_map)
		chmixer_pspd->override_in_ch_map = false;

	return ret;
}

static int msm_compr_channel_mixer_cfg_ctl_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
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

static int msm_compr_channel_mixer_output_map_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int i = 0;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
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

static int msm_compr_channel_mixer_output_map_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int i = 0;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_pspd->out_ch_map[i];
	return 0;
}

static int msm_compr_channel_mixer_input_map_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int i = 0;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	chmixer_pspd->override_in_ch_map = true;
	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		chmixer_pspd->in_ch_map[i] = ucontrol->value.integer.value[i];

	return 0;
}

static int msm_compr_channel_mixer_input_map_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int i = 0;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_pspd->in_ch_map[i];
	return 0;
}

static int msm_compr_channel_mixer_weight_ctl_put(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int channel = (kcontrol->private_value >> 16) & 0xFF;
	int i = 0;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	struct msm_pcm_channel_mixer *chmixer_pspd;

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
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

static int msm_compr_channel_mixer_weight_ctl_get(
		struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	u64 fe_id = kcontrol->private_value & 0xFF;
	int channel = (kcontrol->private_value >> 16) & 0xFF;
	struct snd_soc_component *comp = snd_kcontrol_chip(kcontrol);
	struct msm_compr_pdata *pdata = (struct msm_compr_pdata *)
					snd_soc_component_get_drvdata(comp);
	int i = 0;
	struct msm_pcm_channel_mixer *chmixer_pspd;

	if (channel <= 0 || channel > PCM_FORMAT_MAX_NUM_CHANNEL_V8) {
		pr_err("%s: invalid channel number %d\n", __func__, channel);
		return -EINVAL;
	}
	channel--;

	chmixer_pspd = msm_compr_get_chmixer(pdata, fe_id);
	if (!chmixer_pspd) {
		pr_err("%s: invalid chmixer_pspd in pdata", __func__);
		return -EINVAL;
	}

	for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++)
		ucontrol->value.integer.value[i] =
			chmixer_pspd->channel_weight[channel][i];
	return 0;
}

static int msm_compr_add_platform_controls(struct snd_kcontrol_new *kctl,
			struct snd_soc_pcm_runtime *rtd, const char *name_prefix,
			const char *name_suffix, int session_type, int channels)
{
	int ret = -EINVAL;
	char *mixer_name = NULL;
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
			name_prefix, rtd->pcm->device, name_suffix, channels);
		kctl->private_value = (rtd->dai_link->id) | (channels << 16);
	} else {
		snprintf(mixer_name, ctl_len, "%s %d %s",
			name_prefix, rtd->pcm->device, name_suffix);
		kctl->private_value = (rtd->dai_link->id);
	}
	if (session_type != INVALID_SESSION)
		kctl->private_value |= (session_type << 8);

	kctl->name = mixer_name;
	ret = snd_soc_add_component_controls(component, kctl, 1);
	kfree(mixer_name);
	return ret;
}

static int msm_compr_channel_mixer_output_map_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid channel map value ranges from 1 to 64 */
	uinfo->value.integer.min = 1;
	uinfo->value.integer.max = 64;
	return 0;
}

static int msm_compr_add_channel_mixer_output_map_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix		= "ChMixer Output Map";
	const char *mixer_ctl_name  = NULL;
	int ret = 0, session_type = INVALID_SESSION, channel = -1;
	struct snd_kcontrol_new channel_mixer_output_map_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_channel_mixer_output_map_info,
		.put = msm_compr_channel_mixer_output_map_ctl_put,
		.get = msm_compr_channel_mixer_output_map_ctl_get,
		.private_value = 0,
	};

	mixer_ctl_name = rtd->compr->direction == SND_COMPRESS_PLAYBACK ?
			playback_mixer_ctl_name : capture_mixer_ctl_name ;
	ret = msm_compr_add_platform_controls(&channel_mixer_output_map_control,
			rtd, mixer_ctl_name, suffix, session_type, channel);
	if (ret < 0) {
		pr_err("%s: failed add platform ctl, err = %d\n",
			 __func__, ret);
	}

	return ret;
}

static int msm_compr_channel_mixer_input_map_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid channel map value ranges from 1 to 64 */
	uinfo->value.integer.min = 1;
	uinfo->value.integer.max = 64;
	return 0;
}

static int msm_compr_add_channel_mixer_input_map_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix = "ChMixer Input Map";
	const char *mixer_ctl_name  = NULL;
	int ret = 0, session_type = INVALID_SESSION, channel = -1;
	struct snd_kcontrol_new channel_mixer_input_map_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_channel_mixer_input_map_info,
		.put = msm_compr_channel_mixer_input_map_ctl_put,
		.get = msm_compr_channel_mixer_input_map_ctl_get,
		.private_value = 0,
	};

	mixer_ctl_name = rtd->compr->direction == SND_COMPRESS_PLAYBACK ?
			playback_mixer_ctl_name : capture_mixer_ctl_name ;
	ret = msm_compr_add_platform_controls(&channel_mixer_input_map_control,
			rtd, mixer_ctl_name, suffix, session_type, channel);
	if (ret < 0) {
		pr_err("%s: failed add platform ctl, err = %d\n",
			 __func__, ret);
	}

	return ret;
}

static int msm_compr_channel_mixer_cfg_info(struct snd_kcontrol *kcontrol,
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

static int msm_compr_add_channel_mixer_cfg_controls(
		struct snd_soc_pcm_runtime *rtd)
{
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix		= "ChMixer Cfg";
	const char *mixer_ctl_name = NULL;
	int ret = 0, session_type = INVALID_SESSION, channel = -1;
	struct snd_kcontrol_new channel_mixer_cfg_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_channel_mixer_cfg_info,
		.put = msm_compr_channel_mixer_cfg_ctl_put,
		.get = msm_compr_channel_mixer_cfg_ctl_get,
		.private_value = 0,
	};

	if (rtd->compr->direction == SND_COMPRESS_PLAYBACK) {
		session_type = SESSION_TYPE_RX;
		mixer_ctl_name = playback_mixer_ctl_name;
	} else {
		session_type = SESSION_TYPE_TX;
		mixer_ctl_name = capture_mixer_ctl_name;
	}

	ret = msm_compr_add_platform_controls(&channel_mixer_cfg_control,
			rtd, mixer_ctl_name, suffix, session_type, channel);
	if (ret < 0) {
		pr_err("%s: failed add platform ctl, err = %d\n",
			 __func__, ret);
	}

	return ret;
}

static int msm_compr_channel_mixer_weight_info(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = PCM_FORMAT_MAX_NUM_CHANNEL_V8;
	/* Valid range: 0 to 0x4000(Unity) gain weightage */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0x4000;
	return 0;
}

static int msm_compr_add_channel_mixer_weight_controls(
		struct snd_soc_pcm_runtime *rtd,
		int channel)
{
	const char *playback_mixer_ctl_name	= "AudStr";
	const char *capture_mixer_ctl_name	= "AudStr Capture";
	const char *suffix		= "ChMixer Weight Ch";
	const char *mixer_ctl_name = NULL;
	int ret = 0, session_type = INVALID_SESSION;
	struct snd_kcontrol_new channel_mixer_weight_control = {
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_compr_channel_mixer_weight_info,
		.put = msm_compr_channel_mixer_weight_ctl_put,
		.get = msm_compr_channel_mixer_weight_ctl_get,
		.private_value = 0,
	};

	mixer_ctl_name = rtd->compr->direction == SND_COMPRESS_PLAYBACK ?
			playback_mixer_ctl_name : capture_mixer_ctl_name ;
	ret = msm_compr_add_platform_controls(&channel_mixer_weight_control,
			rtd, mixer_ctl_name, suffix, session_type, channel);
	if (ret < 0) {
		pr_err("%s: failed add platform ctl, err = %d\n",
			 __func__, ret);
	}

	return ret;
}

static int msm_compr_add_channel_mixer_controls(struct snd_soc_pcm_runtime *rtd)
{
	int i, ret = 0;
	struct msm_compr_pdata *pdata = NULL;
	struct snd_soc_component *component = NULL;

	if (!rtd) {
		pr_err("%s NULL rtd\n", __func__);
		return -EINVAL;
	}

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_compr_pdata *)
		snd_soc_component_get_drvdata(component);
	if (!pdata) {
		pr_err("%s: platform data not populated\n", __func__);
		return -EINVAL;
	}

	if (!pdata->chmixer_pspd[rtd->dai_link->id]) {
		pdata->chmixer_pspd[rtd->dai_link->id] =
			kzalloc(sizeof(struct msm_pcm_channel_mixer), GFP_KERNEL);
		if (!pdata->chmixer_pspd[rtd->dai_link->id])
			return -ENOMEM;
	}

	ret = msm_compr_add_channel_mixer_cfg_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add channel mixer cfg controls failed:%d\n",
				__func__, ret);
		goto fail;
	}
	ret = msm_compr_add_channel_mixer_input_map_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add channel mixer input map controls failed:%d\n",
				__func__, ret);
		goto fail;
	}
	ret = msm_compr_add_channel_mixer_output_map_controls(rtd);
	if (ret) {
		pr_err("%s: pcm add channel mixer output map controls failed:%d\n",
				__func__, ret);
		goto fail;
	}

	for (i = 1; i <= PCM_FORMAT_MAX_NUM_CHANNEL_V8; i++) {
		ret =  msm_compr_add_channel_mixer_weight_controls(rtd, i);
		if (ret) {
			pr_err("%s: pcm add channel mixer weight controls failed:%d\n",
				__func__, ret);
			goto fail;
		}
	}
	return 0;

fail:
	kfree(pdata->chmixer_pspd[rtd->dai_link->id]);
	pdata->chmixer_pspd[rtd->dai_link->id] = NULL;
	return ret;
}

int msm_compr_new(struct snd_soc_pcm_runtime *rtd, int num)
{
	int rc = 0;

	if (rtd == NULL) {
		pr_err("%s: RTD is NULL\n", __func__);
		return 0;
	}
	rc = snd_soc_new_compress(rtd, num);
	if (rc)
		pr_err("%s: Fail to create pcm for compress\n", __func__);

	rc = msm_compr_add_volume_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr Volume Control\n", __func__);

	rc = msm_compr_add_audio_effects_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr Audio Effects Control\n",
			__func__);

	rc = msm_compr_add_audio_adsp_stream_cmd_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr ADSP Stream Cmd Control\n",
			__func__);

	rc = msm_compr_add_audio_adsp_stream_callback_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr ADSP Stream Callback Control\n",
			__func__);

	rc = msm_compr_add_io_fd_cmd_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr ion fd Control\n",
			__func__);

	rc = msm_compr_add_event_ack_cmd_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr event ack Control\n",
			__func__);

	rc = msm_compr_add_query_audio_effect_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr Query Audio Effect Control\n",
			__func__);

	rc = msm_compr_add_dec_runtime_params_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr Dec runtime params Control\n",
			__func__);
	rc = msm_compr_add_app_type_cfg_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr App Type Cfg Control\n",
			__func__);
	rc = msm_compr_add_channel_map_control(rtd);
	if (rc)
		pr_err("%s: Could not add Compr Channel Map Control\n",
			__func__);
	rc = msm_compr_add_channel_mixer_controls(rtd);
	if (rc)
		pr_err("%s: Could not add Compr Channel Mixer Controls\n",
			__func__);
	return 0;
}
EXPORT_SYMBOL(msm_compr_new);

static struct snd_compr_ops msm_compr_ops = {
	.open			= msm_compr_open,
	.free			= msm_compr_free,
	.trigger		= msm_compr_trigger,
	.pointer		= msm_compr_pointer,
	.set_params		= msm_compr_set_params,
	.set_metadata		= msm_compr_set_metadata,
	.get_metadata		= msm_compr_get_metadata,
#if IS_ENABLED(CONFIG_AUDIO_QGKI)
	.set_next_track_param	= msm_compr_set_next_track_param,
#endif /* CONFIG_AUDIO_QGKI */
	.ack			= msm_compr_ack,
	.copy			= msm_compr_copy,
	.get_caps		= msm_compr_get_caps,
	.get_codec_caps		= msm_compr_get_codec_caps,
};

static struct snd_soc_component_driver msm_soc_component = {
	.name		= DRV_NAME,
	.probe		= msm_compr_probe,
	.compr_ops	= &msm_compr_ops,
};

static int msm_compr_dev_probe(struct platform_device *pdev)
{
	struct msm_compr_pdata *pdata = NULL;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	pdata = (struct msm_compr_pdata *)
			kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;
	mutex_init(&pdata->lock);
	dev_set_drvdata(&pdev->dev, pdata);

	return snd_soc_register_component(&pdev->dev,
					&msm_soc_component, NULL, 0);
}

static int msm_compr_remove(struct platform_device *pdev)
{
	int i = 0;
	struct msm_compr_pdata *pdata = NULL;

	pdata = dev_get_drvdata(&pdev->dev);
	if (pdata) {
		for (i = 0; i < MSM_FRONTEND_DAI_MM_SIZE; i++)
			kfree(pdata->chmixer_pspd[i]);
	}
	mutex_destroy(&pdata->lock);
	kfree(pdata);

	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_compr_dt_match[] = {
	{.compatible = "qcom,msm-compress-dsp"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_compr_dt_match);

static struct platform_driver msm_compr_driver = {
	.driver = {
		.name = "msm-compress-dsp",
		.owner = THIS_MODULE,
		.of_match_table = msm_compr_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_compr_dev_probe,
	.remove = msm_compr_remove,
};

int __init msm_compress_dsp_init(void)
{
	return platform_driver_register(&msm_compr_driver);
}

void msm_compress_dsp_exit(void)
{
	platform_driver_unregister(&msm_compr_driver);
}

MODULE_DESCRIPTION("Compress Offload platform driver");
MODULE_LICENSE("GPL v2");
