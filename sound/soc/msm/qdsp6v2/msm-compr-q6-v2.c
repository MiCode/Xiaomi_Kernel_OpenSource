/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/moduleparam.h>
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
#include <sound/q6asm-v2.h>
#include <sound/pcm_params.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>
#include <linux/msm_audio_ion.h>

#include <sound/timer.h>

#include "msm-compr-q6-v2.h"
#include "msm-pcm-routing-v2.h"
#include "audio_ocmem.h"
#include <sound/tlv.h>

#define COMPRE_CAPTURE_NUM_PERIODS	16
/* Allocate the worst case frame size for compressed audio */
#define COMPRE_CAPTURE_HEADER_SIZE	(sizeof(struct snd_compr_audio_info))
/* Changing period size to 4032. 4032 will make sure COMPRE_CAPTURE_PERIOD_SIZE
 * is 4096 with meta data size of 64 and MAX_NUM_FRAMES_PER_BUFFER 1
 */
#define COMPRE_CAPTURE_MAX_FRAME_SIZE	(4032)
#define COMPRE_CAPTURE_PERIOD_SIZE	((COMPRE_CAPTURE_MAX_FRAME_SIZE + \
					  COMPRE_CAPTURE_HEADER_SIZE) * \
					  MAX_NUM_FRAMES_PER_BUFFER)
#define COMPRE_OUTPUT_METADATA_SIZE	(sizeof(struct output_meta_data_st))
#define COMPRESSED_LR_VOL_MAX_STEPS	0x20002000

#define MAX_AC3_PARAM_SIZE		(18*2*sizeof(int))
#define AMR_WB_BAND_MODE 8
#define AMR_WB_DTX_MODE 0


const DECLARE_TLV_DB_LINEAR(compr_rx_vol_gain, 0,
			    COMPRESSED_LR_VOL_MAX_STEPS);
struct snd_msm {
	atomic_t audio_ocmem_req;
};
static struct snd_msm compressed_audio;

static struct audio_locks the_locks;

static struct snd_pcm_hardware msm_compr_hardware_capture = {
	.info =		 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =	      SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		SNDRV_PCM_RATE_8000_48000,
	.rate_min =	     8000,
	.rate_max =	     48000,
	.channels_min =	 1,
	.channels_max =	 8,
	.buffer_bytes_max =
		COMPRE_CAPTURE_PERIOD_SIZE * COMPRE_CAPTURE_NUM_PERIODS ,
	.period_bytes_min =	COMPRE_CAPTURE_PERIOD_SIZE,
	.period_bytes_max = COMPRE_CAPTURE_PERIOD_SIZE,
	.periods_min =	  COMPRE_CAPTURE_NUM_PERIODS,
	.periods_max =	  COMPRE_CAPTURE_NUM_PERIODS,
	.fifo_size =	    0,
};

static struct snd_pcm_hardware msm_compr_hardware_playback = {
	.info =		 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =	      SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE,
	.rates =		SNDRV_PCM_RATE_8000_48000 | SNDRV_PCM_RATE_KNOT,
	.rate_min =	     8000,
	.rate_max =	     48000,
	.channels_min =	 1,
	.channels_max =	 8,
	.buffer_bytes_max =     1024 * 1024,
	.period_bytes_min =	128 * 1024,
	.period_bytes_max =     256 * 1024,
	.periods_min =	  4,
	.periods_max =	  8,
	.fifo_size =	    0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000
};

/* Add supported codecs for compress capture path */
static uint32_t supported_compr_capture_codecs[] = {
	SND_AUDIOCODEC_AMRWB
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static bool msm_compr_capture_codecs(uint32_t req_codec)
{
	int i;
	pr_debug("%s req_codec:%d\n", __func__, req_codec);
	if (req_codec == 0)
		return false;
	for (i = 0; i < ARRAY_SIZE(supported_compr_capture_codecs); i++) {
		if (req_codec == supported_compr_capture_codecs[i])
			return true;
	}
	return false;
}

static void compr_event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct compr_audio *compr = priv;
	struct msm_audio *prtd = &compr->prtd;
	struct snd_pcm_substream *substream = prtd->substream;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_aio_write_param param;
	struct audio_aio_read_param read_param;
	struct audio_buffer *buf = NULL;
	struct output_meta_data_st output_meta_data;
	uint32_t *ptrmem = (uint32_t *)payload;
	int i = 0;
	int time_stamp_flag = 0;
	int buffer_length = 0;
	int stop_playback = 0;

	pr_debug("%s opcode =%08x\n", __func__, opcode);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE_V2: {
		uint32_t *ptrmem = (uint32_t *)&param;
		pr_debug("ASM_DATA_EVENT_WRITE_DONE\n");
		pr_debug("Buffer Consumed = 0x%08x\n", *ptrmem);
		prtd->pcm_irq_pos += prtd->pcm_count;
		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);
		else
			if (substream->timer_running)
				snd_timer_interrupt(substream->timer, 1);
		atomic_inc(&prtd->out_count);
		wake_up(&the_locks.write_wait);
		if (!atomic_read(&prtd->start)) {
			atomic_set(&prtd->pending_buffer, 1);
			break;
		} else
			atomic_set(&prtd->pending_buffer, 0);

		/*
		 * check for underrun
		 */
		snd_pcm_stream_lock_irq(substream);
		if (runtime->status->hw_ptr >= runtime->control->appl_ptr) {
			runtime->render_flag |= SNDRV_RENDER_STOPPED;
			stop_playback = 1;
		}
		snd_pcm_stream_unlock_irq(substream);

		if (stop_playback) {
			pr_err("underrun! render stopped\n");
			break;
		}

		buf = prtd->audio_client->port[IN].buf;
		pr_debug("%s:writing %d bytes of buffer[%d] to dsp 2\n",
				__func__, prtd->pcm_count, prtd->out_head);
		pr_debug("%s:writing buffer[%d] from 0x%08x\n",
				__func__, prtd->out_head,
				((unsigned int)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count)));

		if (runtime->tstamp_mode == SNDRV_PCM_TSTAMP_ENABLE)
			time_stamp_flag = SET_TIMESTAMP;
		else
			time_stamp_flag = NO_TIMESTAMP;
		memcpy(&output_meta_data, (char *)(buf->data +
			prtd->out_head * prtd->pcm_count),
			COMPRE_OUTPUT_METADATA_SIZE);

		buffer_length = output_meta_data.frame_size;
		pr_debug("meta_data_length: %d, frame_length: %d\n",
			 output_meta_data.meta_data_length,
			 output_meta_data.frame_size);
		pr_debug("timestamp_msw: %d, timestamp_lsw: %d\n",
			 output_meta_data.timestamp_msw,
			 output_meta_data.timestamp_lsw);
		if (buffer_length == 0) {
			pr_debug("Recieved a zero length buffer-break out");
			break;
		}
		param.paddr = (unsigned long)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count)
				+ output_meta_data.meta_data_length;
		param.len = buffer_length;
		param.msw_ts = output_meta_data.timestamp_msw;
		param.lsw_ts = output_meta_data.timestamp_lsw;
		param.flags = time_stamp_flag;
		param.uid =  (unsigned long)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count
				+ output_meta_data.meta_data_length);
		for (i = 0; i < sizeof(struct audio_aio_write_param)/4;
					i++, ++ptrmem)
			pr_debug("cmd[%d]=0x%08x\n", i, *ptrmem);
		if (q6asm_async_write(prtd->audio_client,
					&param) < 0)
			pr_err("%s:q6asm_async_write failed\n",
				__func__);
		else
			prtd->out_head =
				(prtd->out_head + 1) & (runtime->periods - 1);
		break;
	}
	case ASM_DATA_EVENT_RENDERED_EOS:
		pr_debug("ASM_DATA_CMDRSP_EOS\n");
		if (atomic_read(&prtd->eos)) {
			pr_debug("ASM_DATA_CMDRSP_EOS wake up\n");
			prtd->cmd_ack = 1;
			wake_up(&the_locks.eos_wait);
			atomic_set(&prtd->eos, 0);
		}
		break;
	case ASM_DATA_EVENT_READ_DONE_V2: {
		pr_debug("ASM_DATA_EVENT_READ_DONE\n");
		pr_debug("buf = %p, data = 0x%X, *data = %p,\n"
			 "prtd->pcm_irq_pos = %d\n",
				prtd->audio_client->port[OUT].buf,
			 *(uint32_t *)prtd->audio_client->port[OUT].buf->data,
				prtd->audio_client->port[OUT].buf->data,
				prtd->pcm_irq_pos);

		memcpy(prtd->audio_client->port[OUT].buf->data +
			   prtd->pcm_irq_pos, (ptrmem + READDONE_IDX_SIZE),
			   COMPRE_CAPTURE_HEADER_SIZE);
		pr_debug("buf = %p, updated data = 0x%X, *data = %p\n",
				prtd->audio_client->port[OUT].buf,
			*(uint32_t *)(prtd->audio_client->port[OUT].buf->data +
				prtd->pcm_irq_pos),
				prtd->audio_client->port[OUT].buf->data);
		if (!atomic_read(&prtd->start))
			break;
		pr_debug("frame size=%d, buffer = 0x%X\n",
				ptrmem[READDONE_IDX_SIZE],
				ptrmem[READDONE_IDX_BUFADD_LSW]);
		if (ptrmem[READDONE_IDX_SIZE] > COMPRE_CAPTURE_MAX_FRAME_SIZE) {
			pr_err("Frame length exceeded the max length");
			break;
		}
		buf = prtd->audio_client->port[OUT].buf;
		pr_debug("pcm_irq_pos=%d, buf[0].phys = 0x%X\n",
				prtd->pcm_irq_pos, (uint32_t)buf[0].phys);
		read_param.len = prtd->pcm_count - COMPRE_CAPTURE_HEADER_SIZE;
		read_param.paddr = (unsigned long)(buf[0].phys) +
			prtd->pcm_irq_pos + COMPRE_CAPTURE_HEADER_SIZE;
		prtd->pcm_irq_pos += prtd->pcm_count;

		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);

		q6asm_async_read(prtd->audio_client, &read_param);
		break;
	}
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN_V2: {
			if (substream->stream
				!= SNDRV_PCM_STREAM_PLAYBACK) {
				atomic_set(&prtd->start, 1);
				break;
			}
			if (!atomic_read(&prtd->pending_buffer))
				break;
			pr_debug("%s:writing %d bytes of buffer[%d] to dsp\n",
				__func__, prtd->pcm_count, prtd->out_head);
			buf = prtd->audio_client->port[IN].buf;
			pr_debug("%s:writing buffer[%d] from 0x%08x\n",
				__func__, prtd->out_head,
				((unsigned int)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count)));
			if (runtime->tstamp_mode == SNDRV_PCM_TSTAMP_ENABLE)
				time_stamp_flag = SET_TIMESTAMP;
			else
				time_stamp_flag = NO_TIMESTAMP;
			memcpy(&output_meta_data, (char *)(buf->data +
				prtd->out_head * prtd->pcm_count),
				COMPRE_OUTPUT_METADATA_SIZE);
			buffer_length = output_meta_data.frame_size;
			pr_debug("meta_data_length: %d, frame_length: %d\n",
				 output_meta_data.meta_data_length,
				 output_meta_data.frame_size);
			pr_debug("timestamp_msw: %d, timestamp_lsw: %d\n",
				 output_meta_data.timestamp_msw,
				 output_meta_data.timestamp_lsw);
			param.paddr = (unsigned long)buf[prtd->out_head].phys
					+ output_meta_data.meta_data_length;
			param.len = buffer_length;
			param.msw_ts = output_meta_data.timestamp_msw;
			param.lsw_ts = output_meta_data.timestamp_lsw;
			param.flags = time_stamp_flag;
			param.uid =  (unsigned long)buf[prtd->out_head].phys
					+ output_meta_data.meta_data_length;
			param.metadata_len = COMPRE_OUTPUT_METADATA_SIZE;
			if (q6asm_async_write(prtd->audio_client,
						&param) < 0)
				pr_err("%s:q6asm_async_write failed\n",
					__func__);
			else
				prtd->out_head =
					(prtd->out_head + 1)
					& (runtime->periods - 1);
			atomic_set(&prtd->pending_buffer, 0);
		}
			break;
		case ASM_STREAM_CMD_FLUSH:
			pr_debug("ASM_STREAM_CMD_FLUSH\n");
			prtd->cmd_ack = 1;
			wake_up(&the_locks.flush_wait);
			break;
		default:
			break;
		}
		break;
	}
	default:
		pr_debug("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
}

static int msm_compr_send_ddp_cfg(struct audio_client *ac,
					struct snd_dec_ddp *ddp)
{
	int i, rc;
	pr_debug("%s\n", __func__);
	for (i = 0; i < ddp->params_length/2; i++) {
		rc = q6asm_ds1_set_endp_params(ac, ddp->params_id[i],
						ddp->params_value[i]);
		if (rc) {
			pr_err("sending params_id: %d failed\n",
				ddp->params_id[i]);
			return rc;
		}
	}
	return 0;
}

static int msm_compr_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct asm_aac_cfg aac_cfg;
	int ret;

	pr_debug("compressed stream prepare\n");
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;
	prtd->out_head = 0;
	atomic_set(&prtd->out_count, runtime->periods);

	if (prtd->enabled)
		return 0;

	switch (compr->info.codec_param.codec.id) {
	case SND_AUDIOCODEC_MP3:
		/* No media format block for mp3 */
		break;
	case SND_AUDIOCODEC_AAC:
		pr_debug("%s: SND_AUDIOCODEC_AAC\n", __func__);
		memset(&aac_cfg, 0x0, sizeof(struct asm_aac_cfg));
		aac_cfg.aot = AAC_ENC_MODE_EAAC_P;
		aac_cfg.format = 0x03;
		aac_cfg.ch_cfg = runtime->channels;
		aac_cfg.sample_rate =  runtime->rate;
		ret = q6asm_media_format_block_aac(prtd->audio_client,
					&aac_cfg);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	case SND_AUDIOCODEC_AC3: {
		struct snd_dec_ddp *ddp =
				&compr->info.codec_param.codec.options.ddp;
		pr_debug("%s: SND_AUDIOCODEC_AC3\n", __func__);
		ret = msm_compr_send_ddp_cfg(prtd->audio_client, ddp);
		if (ret < 0)
			pr_err("%s: DDP CMD CFG failed\n", __func__);
		break;
	}
	case SND_AUDIOCODEC_EAC3: {
		struct snd_dec_ddp *ddp =
				&compr->info.codec_param.codec.options.ddp;
		pr_debug("%s: SND_AUDIOCODEC_EAC3\n", __func__);
		ret = msm_compr_send_ddp_cfg(prtd->audio_client, ddp);
		if (ret < 0)
			pr_err("%s: DDP CMD CFG failed\n", __func__);
		break;
	}
	default:
		return -EINVAL;
	}

	prtd->enabled = 1;
	prtd->cmd_ack = 0;
	prtd->cmd_interrupt = 0;

	return 0;
}

static int msm_compr_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct audio_buffer *buf = prtd->audio_client->port[OUT].buf;
	struct snd_codec *codec = &compr->info.codec_param.codec;
	struct audio_aio_read_param read_param;
	int ret = 0;
	int i;
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;

	if (!msm_compr_capture_codecs(codec->id)) {
		/*request codec invalid or not supported,
		use default compress format*/
		codec->id = SND_AUDIOCODEC_AMRWB;
	}
	/* rate and channels are sent to audio driver */
	prtd->samp_rate = runtime->rate;
	prtd->channel_mode = runtime->channels;

	if (prtd->enabled)
		return ret;
	read_param.len = prtd->pcm_count;

	switch (codec->id) {
	case SND_AUDIOCODEC_AMRWB:
		pr_debug("SND_AUDIOCODEC_AMRWB\n");
		ret = q6asm_enc_cfg_blk_amrwb(prtd->audio_client,
			MAX_NUM_FRAMES_PER_BUFFER,
			/* use fixed band mode and dtx mode
			 *  band mode - 23.85 kbps
                         */
			AMR_WB_BAND_MODE,
			/* dtx mode - disable */
			AMR_WB_DTX_MODE);
		if (ret < 0)
			pr_err("%s: CMD Format block" \
				"failed: %d\n", __func__, ret);
		break;
	default:
		pr_debug("No config for codec %d\n", codec->id);
	}
	pr_debug("%s: Samp_rate = %d, Channel = %d, pcm_size = %d,\n"
			 "pcm_count = %d, periods = %d\n",
			 __func__, prtd->samp_rate, prtd->channel_mode,
			 prtd->pcm_size, prtd->pcm_count, runtime->periods);

	for (i = 0; i < runtime->periods; i++) {
		read_param.uid = i;
		switch (codec->id) {
		case SND_AUDIOCODEC_AMRWB:
			read_param.len = prtd->pcm_count
					- COMPRE_CAPTURE_HEADER_SIZE;
			read_param.paddr = (unsigned long)(buf[i].phys)
					+ COMPRE_CAPTURE_HEADER_SIZE;
			pr_debug("Push buffer [%d] to DSP, "\
					"paddr: %p, vaddr: %p\n",
					i, (void *) read_param.paddr,
					buf[i].data);
			q6asm_async_read(prtd->audio_client, &read_param);
			break;
		default:
			read_param.paddr = (unsigned long)(buf[i].phys);
			/*q6asm_async_read_compressed(prtd->audio_client,
				&read_param);*/
			pr_debug("%s: To add support for read compressed\n",
								__func__);
			ret = -EINVAL;
			break;
		}
	}
	prtd->periods = runtime->periods;

	prtd->enabled = 1;

	return ret;
}

static int msm_compr_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;

	pr_debug("%s\n", __func__);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->pcm_irq_pos = 0;

		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			if (!msm_compr_capture_codecs(
				compr->info.codec_param.codec.id)) {
				/*request codec invalid or not supported,
				use default compress format*/
				compr->info.codec_param.codec.id =
				SND_AUDIOCODEC_AMRWB;
			}
			switch (compr->info.codec_param.codec.id) {
			case SND_AUDIOCODEC_AMRWB:
				break;
			default:
				msm_pcm_routing_reg_psthr_stream(
					soc_prtd->dai_link->be_id,
					prtd->session_id, substream->stream);
				break;
			}
		}
		atomic_set(&prtd->pending_buffer, 1);
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("%s: Trigger start\n", __func__);
		q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		atomic_set(&prtd->start, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("SNDRV_PCM_TRIGGER_STOP\n");
		if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
			switch (compr->info.codec_param.codec.id) {
			case SND_AUDIOCODEC_AMRWB:
				break;
			default:
				msm_pcm_routing_reg_psthr_stream(
					soc_prtd->dai_link->be_id,
					prtd->session_id, substream->stream);
				break;
			}
		}
		atomic_set(&prtd->start, 0);
		runtime->render_flag &= ~SNDRV_RENDER_STOPPED;
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE\n");
		q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
		atomic_set(&prtd->start, 0);
		runtime->render_flag &= ~SNDRV_RENDER_STOPPED;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static void populate_codec_list(struct compr_audio *compr,
		struct snd_pcm_runtime *runtime)
{
	pr_debug("%s\n", __func__);
	/* MP3 Block */
	compr->info.compr_cap.num_codecs = 5;
	compr->info.compr_cap.min_fragment_size = runtime->hw.period_bytes_min;
	compr->info.compr_cap.max_fragment_size = runtime->hw.period_bytes_max;
	compr->info.compr_cap.min_fragments = runtime->hw.periods_min;
	compr->info.compr_cap.max_fragments = runtime->hw.periods_max;
	compr->info.compr_cap.codecs[0] = SND_AUDIOCODEC_MP3;
	compr->info.compr_cap.codecs[1] = SND_AUDIOCODEC_AAC;
	compr->info.compr_cap.codecs[2] = SND_AUDIOCODEC_AC3;
	compr->info.compr_cap.codecs[3] = SND_AUDIOCODEC_EAC3;
	compr->info.compr_cap.codecs[4] = SND_AUDIOCODEC_AMRWB;
	/* Add new codecs here */
}

static int msm_compr_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr;
	struct msm_audio *prtd;
	int ret = 0;

	pr_debug("%s\n", __func__);
	compr = kzalloc(sizeof(struct compr_audio), GFP_KERNEL);
	if (compr == NULL) {
		pr_err("Failed to allocate memory for msm_audio\n");
		return -ENOMEM;
	}
	prtd = &compr->prtd;
	prtd->substream = substream;
	runtime->render_flag = SNDRV_DMA_MODE;
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)compr_event_handler, compr);
	if (!prtd->audio_client) {
		pr_info("%s: Could not allocate memory\n", __func__);
		kfree(prtd);
		return -ENOMEM;
	}

	prtd->audio_client->perf_mode = false;
	pr_info("%s: session ID %d\n", __func__, prtd->audio_client->session);

	prtd->session_id = prtd->audio_client->session;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		runtime->hw = msm_compr_hardware_playback;
		prtd->cmd_ack = 1;
	} else {
		runtime->hw = msm_compr_hardware_capture;
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

	prtd->dsp_cnt = 0;
	atomic_set(&prtd->pending_buffer, 1);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		compr->codec = FORMAT_MP3;
	populate_codec_list(compr, runtime);
	runtime->private_data = compr;
	atomic_set(&prtd->eos, 0);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (!atomic_cmpxchg(&compressed_audio.audio_ocmem_req, 0, 1))
			audio_ocmem_process_req(AUDIO, true);
		else
			atomic_inc(&compressed_audio.audio_ocmem_req);
		pr_debug("%s: req: %d\n", __func__,
			atomic_read(&compressed_audio.audio_ocmem_req));
	}
	return 0;
}

static int compressed_set_volume(struct msm_audio *prtd, uint32_t volume)
{
	int rc = 0;
	int avg_vol = 0;
	int lgain = (volume >> 16) & 0xFFFF;
	int rgain = volume & 0xFFFF;
	if (prtd && prtd->audio_client) {
		pr_debug("%s: channels %d volume 0x%x\n", __func__,
			prtd->channel_mode, volume);
		if ((prtd->channel_mode == 2) &&
			(lgain != rgain)) {
			pr_debug("%s: call q6asm_set_lrgain\n", __func__);
			rc = q6asm_set_lrgain(prtd->audio_client, lgain, rgain);
		} else {
			avg_vol = (lgain + rgain)/2;
			pr_debug("%s: call q6asm_set_volume\n", __func__);
			rc = q6asm_set_volume(prtd->audio_client, avg_vol);
		}
		if (rc < 0) {
			pr_err("%s: Send Volume command failed rc=%d\n",
				__func__, rc);
		}
	}
	return rc;
}

static int msm_compr_playback_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	int dir = 0;

	pr_debug("%s\n", __func__);

	dir = IN;
	atomic_set(&prtd->pending_buffer, 0);

	if (atomic_read(&compressed_audio.audio_ocmem_req) > 1)
		atomic_dec(&compressed_audio.audio_ocmem_req);
	else if (atomic_cmpxchg(&compressed_audio.audio_ocmem_req, 1, 0))
		audio_ocmem_process_req(AUDIO, false);

	pr_debug("%s: req: %d\n", __func__,
		atomic_read(&compressed_audio.audio_ocmem_req));
	prtd->pcm_irq_pos = 0;
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
		msm_pcm_routing_dereg_phy_stream(
			soc_prtd->dai_link->be_id,
			SNDRV_PCM_STREAM_PLAYBACK);
	q6asm_audio_client_free(prtd->audio_client);
	kfree(prtd);
	return 0;
}

static int msm_compr_capture_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	int dir = OUT;

	pr_debug("%s\n", __func__);
	atomic_set(&prtd->pending_buffer, 0);
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
				SNDRV_PCM_STREAM_CAPTURE);
	q6asm_audio_client_free(prtd->audio_client);
	kfree(prtd);
	return 0;
}

static int msm_compr_close(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_compr_playback_close(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_compr_capture_close(substream);
	return ret;
}
static int msm_compr_prepare(struct snd_pcm_substream *substream)
{
	int ret = 0;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = msm_compr_playback_prepare(substream);
	else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		ret = msm_compr_capture_prepare(substream);
	return ret;
}

static snd_pcm_uframes_t msm_compr_pointer(struct snd_pcm_substream *substream)
{

	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;

	if (prtd->pcm_irq_pos >= prtd->pcm_size)
		prtd->pcm_irq_pos = 0;

	pr_debug("%s: pcm_irq_pos = %d, pcm_size = %d, sample_bits = %d,\n"
			 "frame_bits = %d\n", __func__, prtd->pcm_irq_pos,
			 prtd->pcm_size, runtime->sample_bits,
			 runtime->frame_bits);
	return bytes_to_frames(runtime, (prtd->pcm_irq_pos));
}

static int msm_compr_mmap(struct snd_pcm_substream *substream,
				struct vm_area_struct *vma)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_client *ac = prtd->audio_client;
	struct audio_port_data *apd = ac->port;
	struct audio_buffer *ab;
	int dir = -1;

	prtd->mmap_flag = 1;
	runtime->render_flag = SNDRV_NON_DMA_MODE;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;
	ab = &(apd[dir].buf[0]);

	return msm_audio_ion_mmap(ab, vma);
}

static int msm_compr_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	int dir, ret;
	uint16_t bits_per_sample = 16;

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

	pr_debug("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;

	if (runtime->format == SNDRV_PCM_FORMAT_S24_LE)
		bits_per_sample = 24;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = q6asm_open_write_v2(prtd->audio_client,
				compr->codec, bits_per_sample);
		if (ret < 0) {
			pr_err("%s: Session out open failed\n",
				__func__);
			return -ENOMEM;
		}
		msm_pcm_routing_reg_phy_stream(
			soc_prtd->dai_link->be_id,
			prtd->audio_client->perf_mode,
			prtd->session_id,
			substream->stream);
		/* the number of channels are required to call volume api
		   accoridngly. So, get channels from hw params */
		if ((params_channels(params) > 0) &&
		    (params_periods(params) <= runtime->hw.channels_max))
			prtd->channel_mode = params_channels(params);

		ret = compressed_set_volume(prtd, 0);
		if (ret < 0)
			pr_err("%s : Set Volume failed : %d", __func__, ret);

		ret = q6asm_set_softpause(prtd->audio_client, &softpause);
		if (ret < 0)
			pr_err("%s: Send SoftPause Param failed ret=%d\n",
				__func__, ret);
		ret = q6asm_set_softvolume(prtd->audio_client, &softvol);
		if (ret < 0)
			pr_err("%s: Send SoftVolume Param failed ret=%d\n",
				__func__, ret);
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (!msm_compr_capture_codecs(
			compr->info.codec_param.codec.id)) {
			/*request codec invalid or not supported,
			use default compress format*/
			compr->info.codec_param.codec.id =
				SND_AUDIOCODEC_AMRWB;
		}
		switch (compr->info.codec_param.codec.id) {
		case SND_AUDIOCODEC_AMRWB:
			pr_debug("q6asm_open_read(FORMAT_AMRWB)\n");
			ret = q6asm_open_read(prtd->audio_client,
				FORMAT_AMRWB);
			if (ret < 0) {
				pr_err("%s: compressed Session out open failed\n",
					__func__);
				return -ENOMEM;
			}
			pr_debug("msm_pcm_routing_reg_phy_stream\n");
			msm_pcm_routing_reg_phy_stream(
					soc_prtd->dai_link->be_id,
					prtd->audio_client->perf_mode,
					prtd->session_id, substream->stream);
			break;
		default:
			pr_debug("q6asm_open_read_compressed(COMPRESSED_META_DATA_MODE)\n");
			/*
			ret = q6asm_open_read_compressed(prtd->audio_client,
				MAX_NUM_FRAMES_PER_BUFFER,
				COMPRESSED_META_DATA_MODE);
			*/
			ret = -EINVAL;
			break;
		}

		if (ret < 0) {
			pr_err("%s: compressed Session out open failed\n",
				__func__);
			return -ENOMEM;
		}
	}

	ret = q6asm_set_io_mode(prtd->audio_client,
					(COMPRESSED_IO | ASYNC_IO_MODE));
	if (ret < 0) {
		pr_err("%s: Set IO mode failed\n", __func__);
		return -ENOMEM;
	}
	/* Modifying kernel hardware params based on userspace config */
	if (params_periods(params) > 0 &&
		(params_periods(params) != runtime->hw.periods_max)) {
		runtime->hw.periods_max = params_periods(params);
	}
	if (params_period_bytes(params) > 0 &&
		(params_period_bytes(params) != runtime->hw.period_bytes_min)) {
		runtime->hw.period_bytes_min = params_period_bytes(params);
	}
	runtime->hw.buffer_bytes_max =
			runtime->hw.period_bytes_min * runtime->hw.periods_max;
	pr_debug("allocate %d buffers each of size %d\n",
		runtime->hw.period_bytes_min,
		runtime->hw.periods_max);
	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			runtime->hw.period_bytes_min,
			runtime->hw.periods_max);
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed rc = %d\n",
						ret);
		return -ENOMEM;
	}
	buf = prtd->audio_client->port[dir].buf;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->area = buf[0].data;
	dma_buf->addr =  buf[0].phys;
	dma_buf->bytes = runtime->hw.buffer_bytes_max;

	pr_debug("%s: buf[%p]dma_buf->area[%p]dma_buf->addr[%p]\n"
		 "dma_buf->bytes[%d]\n", __func__,
		 (void *)buf, (void *)dma_buf->area,
		 (void *)dma_buf->addr, dma_buf->bytes);
	if (!dma_buf->area)
		return -ENOMEM;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);
	return 0;
}

static int msm_compr_ioctl(struct snd_pcm_substream *substream,
		unsigned int cmd, void *arg)
{
	int rc = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	uint64_t timestamp;
	uint64_t temp;

	switch (cmd) {
	case SNDRV_COMPRESS_TSTAMP: {
		struct snd_compr_tstamp tstamp;
		pr_debug("SNDRV_COMPRESS_TSTAMP\n");

		memset(&tstamp, 0x0, sizeof(struct snd_compr_tstamp));
		rc = q6asm_get_session_time(prtd->audio_client, &timestamp);
		if (rc < 0) {
			pr_err("%s: Get Session Time return value =%lld\n",
				__func__, timestamp);
			return -EAGAIN;
		}
		temp = (timestamp * 2 * runtime->channels);
		temp = temp * (runtime->rate/1000);
		temp = div_u64(temp, 1000);
		tstamp.sampling_rate = runtime->rate;
		tstamp.timestamp = timestamp;
		pr_debug("%s: bytes_consumed:,timestamp = %lld,\n",
						__func__,
			tstamp.timestamp);
		if (copy_to_user((void *) arg, &tstamp,
			sizeof(struct snd_compr_tstamp)))
				return -EFAULT;
		return 0;
	}
	case SNDRV_COMPRESS_GET_CAPS:
		pr_debug("SNDRV_COMPRESS_GET_CAPS\n");
		if (copy_to_user((void *) arg, &compr->info.compr_cap,
			sizeof(struct snd_compr_caps))) {
			rc = -EFAULT;
			pr_err("%s: ERROR: copy to user\n", __func__);
			return rc;
		}
		return 0;
	case SNDRV_COMPRESS_SET_PARAMS:
		pr_debug("SNDRV_COMPRESS_SET_PARAMS:\n");
		if (copy_from_user(&compr->info.codec_param, (void *) arg,
			sizeof(struct snd_compr_params))) {
			rc = -EFAULT;
			pr_err("%s: ERROR: copy from user\n", __func__);
			return rc;
		}
		switch (compr->info.codec_param.codec.id) {
		case SND_AUDIOCODEC_MP3:
			/* For MP3 we dont need any other parameter */
			pr_debug("SND_AUDIOCODEC_MP3\n");
			compr->codec = FORMAT_MP3;
			break;
		case SND_AUDIOCODEC_AAC:
			pr_debug("SND_AUDIOCODEC_AAC\n");
			compr->codec = FORMAT_MPEG4_AAC;
			break;
		case SND_AUDIOCODEC_AC3: {
			char params_value[MAX_AC3_PARAM_SIZE];
			int *params_value_data = (int *)params_value;
			/* 36 is the max param length for ddp */
			int i;
			struct snd_dec_ddp *ddp =
				&compr->info.codec_param.codec.options.ddp;
			uint32_t params_length = 0;
			/* check integer overflow */
			if (ddp->params_length > UINT_MAX/sizeof(int)) {
				pr_err("%s: Integer overflow ddp->params_length %d\n",
				__func__, ddp->params_length);
				return -EINVAL;
			}
			params_length = ddp->params_length*sizeof(int);
			if (params_length > MAX_AC3_PARAM_SIZE) {
				/*MAX is 36*sizeof(int) this should not happen*/
				pr_err("%s: params_length(%d) is greater than %zd\n",
				__func__, params_length, MAX_AC3_PARAM_SIZE);
				return -EINVAL;
			}
			pr_debug("SND_AUDIOCODEC_AC3\n");
			compr->codec = FORMAT_AC3;
			if (copy_from_user(params_value, (void *)ddp->params,
					params_length))
				pr_err("%s: copy ddp params value, size=%d\n",
					__func__, params_length);
			pr_debug("params_length: %d\n", ddp->params_length);
			for (i = 0; i < params_length; i++)
				pr_debug("params_value[%d]: %x\n", i,
					params_value_data[i]);
			for (i = 0; i < ddp->params_length/2; i++) {
				ddp->params_id[i] = params_value_data[2*i];
				ddp->params_value[i] = params_value_data[2*i+1];
			}
			if (atomic_read(&prtd->start)) {
				rc = msm_compr_send_ddp_cfg(prtd->audio_client,
								ddp);
				if (rc < 0)
					pr_err("%s: DDP CMD CFG failed\n",
						__func__);
			}
			break;
		}
		case SND_AUDIOCODEC_EAC3: {
			char params_value[MAX_AC3_PARAM_SIZE];
			int *params_value_data = (int *)params_value;
			/* 36 is the max param length for ddp */
			int i;
			struct snd_dec_ddp *ddp =
				&compr->info.codec_param.codec.options.ddp;
			uint32_t params_length = 0;
			/* check integer overflow */
			if (ddp->params_length > UINT_MAX/sizeof(int)) {
				pr_err("%s: Integer overflow ddp->params_length %d\n",
				__func__, ddp->params_length);
				return -EINVAL;
			}
			if (params_length > MAX_AC3_PARAM_SIZE) {
				/*MAX is 36*sizeof(int) this should not happen*/
				pr_err("%s: params_length(%d) is greater than %d\n",
				__func__, params_length, MAX_AC3_PARAM_SIZE);
				return -EINVAL;
			}
			pr_debug("SND_AUDIOCODEC_EAC3\n");
			compr->codec = FORMAT_EAC3;
			if (copy_from_user(params_value, (void *)ddp->params,
					params_length))
				pr_err("%s: copy ddp params value, size=%d\n",
					__func__, params_length);
			pr_debug("params_length: %d\n", ddp->params_length);
			for (i = 0; i < ddp->params_length; i++)
				pr_debug("params_value[%d]: %x\n", i,
					params_value_data[i]);
			for (i = 0; i < ddp->params_length/2; i++) {
				ddp->params_id[i] = params_value_data[2*i];
				ddp->params_value[i] = params_value_data[2*i+1];
			}
			if (atomic_read(&prtd->start)) {
				rc = msm_compr_send_ddp_cfg(prtd->audio_client,
								ddp);
				if (rc < 0)
					pr_err("%s: DDP CMD CFG failed\n",
						__func__);
			}
			break;
		}
		default:
			pr_debug("FORMAT_LINEAR_PCM\n");
			compr->codec = FORMAT_LINEAR_PCM;
			break;
		}
		return 0;
	case SNDRV_PCM_IOCTL1_RESET:
		pr_debug("SNDRV_PCM_IOCTL1_RESET\n");
		/* Flush only when session is started during CAPTURE,
		   while PLAYBACK has no such restriction. */
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK ||
			  (substream->stream == SNDRV_PCM_STREAM_CAPTURE &&
						atomic_read(&prtd->start))) {
			if (atomic_read(&prtd->eos)) {
				prtd->cmd_interrupt = 1;
				wake_up(&the_locks.eos_wait);
				atomic_set(&prtd->eos, 0);
			}

			/* A unlikely race condition possible with FLUSH
			   DRAIN if ack is set by flush and reset by drain */
			prtd->cmd_ack = 0;
			rc = q6asm_cmd(prtd->audio_client, CMD_FLUSH);
			if (rc < 0) {
				pr_err("%s: flush cmd failed rc=%d\n",
					__func__, rc);
				return rc;
			}
			rc = wait_event_timeout(the_locks.flush_wait,
				prtd->cmd_ack, 5 * HZ);
			if (!rc)
				pr_err("Flush cmd timeout\n");
			prtd->pcm_irq_pos = 0;
		}
		break;
	case SNDRV_COMPRESS_DRAIN:
		pr_debug("%s: SNDRV_COMPRESS_DRAIN\n", __func__);
		if (atomic_read(&prtd->pending_buffer)) {
			pr_debug("%s: no pending writes, drain would block\n",
			 __func__);
			return -EWOULDBLOCK;
		}

		atomic_set(&prtd->eos, 1);
		atomic_set(&prtd->pending_buffer, 0);
		prtd->cmd_ack = 0;
		q6asm_cmd_nowait(prtd->audio_client, CMD_EOS);
		/* Wait indefinitely for  DRAIN. Flush can also signal this*/
		rc = wait_event_interruptible(the_locks.eos_wait,
			(prtd->cmd_ack || prtd->cmd_interrupt));

		if (rc < 0)
			pr_err("EOS cmd interrupted\n");
		pr_debug("%s: SNDRV_COMPRESS_DRAIN  out of wait\n", __func__);

		if (prtd->cmd_interrupt)
			rc = -EINTR;

		prtd->cmd_interrupt = 0;
		return rc;
	default:
		break;
	}
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int msm_compr_restart(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct audio_aio_write_param param;
	struct audio_buffer *buf = NULL;
	struct output_meta_data_st output_meta_data;
	int time_stamp_flag = 0;
	int buffer_length = 0;

	pr_debug("%s, trigger restart\n", __func__);

	if (runtime->render_flag & SNDRV_RENDER_STOPPED) {
		buf = prtd->audio_client->port[IN].buf;
		pr_debug("%s:writing %d bytes of buffer[%d] to dsp 2\n",
				__func__, prtd->pcm_count, prtd->out_head);
		pr_debug("%s:writing buffer[%d] from 0x%08x\n",
				__func__, prtd->out_head,
				((unsigned int)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count)));

		if (runtime->tstamp_mode == SNDRV_PCM_TSTAMP_ENABLE)
			time_stamp_flag = SET_TIMESTAMP;
		else
			time_stamp_flag = NO_TIMESTAMP;
		memcpy(&output_meta_data, (char *)(buf->data +
			prtd->out_head * prtd->pcm_count),
			COMPRE_OUTPUT_METADATA_SIZE);

		buffer_length = output_meta_data.frame_size;
		pr_debug("meta_data_length: %d, frame_length: %d\n",
			 output_meta_data.meta_data_length,
			 output_meta_data.frame_size);
		pr_debug("timestamp_msw: %d, timestamp_lsw: %d\n",
			 output_meta_data.timestamp_msw,
			 output_meta_data.timestamp_lsw);

		param.paddr = (unsigned long)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count)
				+ output_meta_data.meta_data_length;
		param.len = buffer_length;
		param.msw_ts = output_meta_data.timestamp_msw;
		param.lsw_ts = output_meta_data.timestamp_lsw;
		param.flags = time_stamp_flag;
		param.uid =  (unsigned long)buf[0].phys
				+ (prtd->out_head * prtd->pcm_count
				+ output_meta_data.meta_data_length);
		if (q6asm_async_write(prtd->audio_client,
					&param) < 0)
			pr_err("%s:q6asm_async_write failed\n",
				__func__);
		else
			prtd->out_head =
				(prtd->out_head + 1) & (runtime->periods - 1);

		runtime->render_flag &= ~SNDRV_RENDER_STOPPED;
		return 0;
	}
	return 0;
}

static int msm_compr_volume_ctl_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream =
			 vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume : %x\n", __func__, volume);
	if (!substream)
		return -ENODEV;
	if (!substream->runtime)
		return 0;
	prtd = substream->runtime->private_data;
	if (prtd)
		rc = compressed_set_volume(prtd, volume);

	return rc;
}

static int msm_compr_volume_ctl_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream =
			 vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;

	pr_debug("%s\n", __func__);
	if (!substream)
		return -ENODEV;
	if (!substream->runtime)
		return 0;
	prtd = substream->runtime->private_data;
	if (prtd)
		ucontrol->value.integer.value[0] = prtd->volume;
	return 0;
}

static int msm_compr_add_controls(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_volume *volume_info;
	struct snd_kcontrol *kctl;

	dev_dbg(rtd->dev, "%s, Volume cntrl add\n", __func__);
	ret = snd_pcm_add_volume_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				      NULL, 1, rtd->dai_link->be_id,
				      &volume_info);
	if (ret < 0)
		return ret;
	kctl = volume_info->kctl;
	kctl->put = msm_compr_volume_ctl_put;
	kctl->get = msm_compr_volume_ctl_get;
	kctl->tlv.p = compr_rx_vol_gain;
	return 0;
}

static struct snd_pcm_ops msm_compr_ops = {
	.open	   = msm_compr_open,
	.hw_params	= msm_compr_hw_params,
	.close	  = msm_compr_close,
	.ioctl	  = msm_compr_ioctl,
	.prepare	= msm_compr_prepare,
	.trigger	= msm_compr_trigger,
	.pointer	= msm_compr_pointer,
	.mmap		= msm_compr_mmap,
	.restart	= msm_compr_restart,
};

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	int ret = 0;

	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_compr_add_controls(rtd);
	if (ret)
		pr_err("%s, kctl add failed\n", __func__);
	return ret;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_compr_ops,
	.pcm_new	= msm_asoc_pcm_new,
};

static __devinit int msm_compr_probe(struct platform_device *pdev)
{
	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", "msm-compr-dsp");

	dev_info(&pdev->dev, "%s: dev name %s\n",
			 __func__, dev_name(&pdev->dev));

	atomic_set(&compressed_audio.audio_ocmem_req, 0);
	return snd_soc_register_platform(&pdev->dev,
				   &msm_soc_platform);
}

static int msm_compr_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id msm_compr_dt_match[] = {
	{.compatible = "qcom,msm-compr-dsp"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_compr_dt_match);

static struct platform_driver msm_compr_driver = {
	.driver = {
		.name = "msm-compr-dsp",
		.owner = THIS_MODULE,
		.of_match_table = msm_compr_dt_match,
	},
	.probe = msm_compr_probe,
	.remove = __devexit_p(msm_compr_remove),
};

static int __init msm_soc_platform_init(void)
{
	init_waitqueue_head(&the_locks.enable_wait);
	init_waitqueue_head(&the_locks.eos_wait);
	init_waitqueue_head(&the_locks.write_wait);
	init_waitqueue_head(&the_locks.read_wait);
	init_waitqueue_head(&the_locks.flush_wait);

	return platform_driver_register(&msm_compr_driver);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_compr_driver);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("PCM module platform driver");
MODULE_LICENSE("GPL v2");
