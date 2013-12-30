/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <sound/q6asm.h>
#include <sound/pcm_params.h>
#include <asm/dma.h>
#include <linux/dma-mapping.h>

#include <sound/timer.h>
#include <mach/qdsp6v2/q6core.h>
#include <sound/pcm.h>

#include "msm-compr-q6.h"
#include "msm-pcm-routing.h"

#define COMPRE_CAPTURE_NUM_PERIODS	16
/* Allocate the worst case frame size for compressed audio */
#define COMPRE_CAPTURE_HEADER_SIZE	(sizeof(struct snd_compr_audio_info))
#define COMPRE_CAPTURE_MAX_FRAME_SIZE	(6144)
#define COMPRE_CAPTURE_PERIOD_SIZE	((COMPRE_CAPTURE_MAX_FRAME_SIZE + \
					  COMPRE_CAPTURE_HEADER_SIZE) * \
					  MAX_NUM_FRAMES_PER_BUFFER)
#define COMPRE_OUTPUT_METADATA_SIZE	(sizeof(struct output_meta_data_st))

struct snd_msm {
	struct msm_audio *prtd;
	unsigned volume;
};
static struct snd_msm compressed_audio = {NULL, 0x2000} ;

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
	.formats =	      SNDRV_PCM_FMTBIT_S16_LE,
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

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

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

	pr_debug("%s opcode =%08x\n", __func__, opcode);
	switch (opcode) {
	case ASM_DATA_EVENT_WRITE_DONE: {
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
		if (runtime->status->hw_ptr >= runtime->control->appl_ptr) {
			runtime->render_flag |= SNDRV_RENDER_STOPPED;
			atomic_set(&prtd->pending_buffer, 1);
			pr_debug("%s:compr driver underrun hw_ptr = %ld appl_ptr = %ld\n",
				__func__, runtime->status->hw_ptr,
				runtime->control->appl_ptr);
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
	case ASM_DATA_CMDRSP_EOS:
		pr_debug("ASM_DATA_CMDRSP_EOS\n");
		if (atomic_read(&prtd->eos)) {
			pr_debug("ASM_DATA_CMDRSP_EOS wake up\n");
			prtd->cmd_ack = 1;
			wake_up(&the_locks.eos_wait);
			atomic_set(&prtd->eos, 0);
		}
		atomic_set(&prtd->pending_buffer, 1);
		break;
	case ASM_DATA_EVENT_READ_DONE: {
		pr_debug("ASM_DATA_EVENT_READ_DONE\n");
		pr_debug("buf = %p, data = 0x%X, *data = %p,\n"
			 "prtd->pcm_irq_pos = %d\n",
				prtd->audio_client->port[OUT].buf,
			 *(uint32_t *)prtd->audio_client->port[OUT].buf->data,
				prtd->audio_client->port[OUT].buf->data,
				prtd->pcm_irq_pos);

		memcpy(prtd->audio_client->port[OUT].buf->data +
			   prtd->pcm_irq_pos, (ptrmem + 2),
			   COMPRE_CAPTURE_HEADER_SIZE);
		pr_debug("buf = %p, updated data = 0x%X, *data = %p\n",
				prtd->audio_client->port[OUT].buf,
			*(uint32_t *)(prtd->audio_client->port[OUT].buf->data +
				prtd->pcm_irq_pos),
				prtd->audio_client->port[OUT].buf->data);
		if (!atomic_read(&prtd->start))
			break;
		pr_debug("frame size=%d, buffer = 0x%X\n", ptrmem[2],
				ptrmem[1]);
		if (ptrmem[2] > COMPRE_CAPTURE_MAX_FRAME_SIZE) {
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
	case ASM_DATA_EVENT_READ_COMPRESSED_DONE: {
		pr_debug("ASM_DATA_EVENT_READ_COMPRESSED_DONE\n");
		pr_debug("buf = %p, data = 0x%X, *data = %p,\n"
			 "prtd->pcm_irq_pos = %d\n",
				prtd->audio_client->port[OUT].buf,
			 *(uint32_t *)prtd->audio_client->port[OUT].buf->data,
				prtd->audio_client->port[OUT].buf->data,
				prtd->pcm_irq_pos);

		if (!atomic_read(&prtd->start))
			break;
		buf = prtd->audio_client->port[OUT].buf;
		pr_debug("pcm_irq_pos=%d, buf[0].phys = 0x%X\n",
				prtd->pcm_irq_pos, (uint32_t)buf[0].phys);
		read_param.len = prtd->pcm_count;
		read_param.paddr = (unsigned long)(buf[0].phys) +
			prtd->pcm_irq_pos;
		prtd->pcm_irq_pos += prtd->pcm_count;

		if (atomic_read(&prtd->start))
			snd_pcm_period_elapsed(substream);

		q6asm_async_read_compressed(prtd->audio_client, &read_param);
		break;
	}
	case APR_BASIC_RSP_RESULT: {
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN: {
			if (substream->stream
				!= SNDRV_PCM_STREAM_PLAYBACK) {
				atomic_set(&prtd->start, 1);
				break;
			}
			if (!atomic_read(&prtd->pending_buffer))
				break;
			pr_debug("%s:writing %d bytes"
				" of buffer[%d] to dsp\n",
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

static int msm_compr_playback_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct asm_aac_cfg aac_cfg;
	struct asm_wma_cfg wma_cfg;
	struct asm_wmapro_cfg wma_pro_cfg;
	struct asm_amrwbplus_cfg amrwb_cfg;
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
		pr_debug("%s: SND_AUDIOCODEC_MP3\n", __func__);
		ret = q6asm_media_format_block(prtd->audio_client,
				compr->codec);
		if (ret < 0)
			pr_info("%s: CMD Format block failed\n", __func__);
		break;
	case SND_AUDIOCODEC_AAC:
		pr_debug("%s: SND_AUDIOCODEC_AAC\n", __func__);
		memset(&aac_cfg, 0x0, sizeof(struct asm_aac_cfg));
		aac_cfg.aot = AAC_ENC_MODE_EAAC_P;
		aac_cfg.format = 0x03;
		aac_cfg.ch_cfg = runtime->channels;
		aac_cfg.sample_rate = runtime->rate;
		ret = q6asm_media_format_block_aac(prtd->audio_client,
					&aac_cfg);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	case SND_AUDIOCODEC_AC3_PASS_THROUGH:
	case SND_AUDIOCODEC_DTS_PASS_THROUGH:
	case SND_AUDIOCODEC_DTS_LBR_PASS_THROUGH:
		pr_debug("compressd playback, no need to send decoder params");
		pr_debug("decoder id: %d\n",
			compr->info.codec_param.codec.id);
		msm_pcm_routing_reg_psthr_stream(
					soc_prtd->dai_link->be_id,
					prtd->session_id, substream->stream,
					1);
		break;
	case SND_AUDIOCODEC_WMA:
		pr_debug("SND_AUDIOCODEC_WMA\n");
		memset(&wma_cfg, 0x0, sizeof(struct asm_wma_cfg));
		wma_cfg.format_tag = compr->info.codec_param.codec.format;
		wma_cfg.ch_cfg = compr->info.codec_param.codec.ch_in;
		wma_cfg.sample_rate = compr->info.codec_param.codec.sample_rate;
		wma_cfg.avg_bytes_per_sec =
			compr->info.codec_param.codec.bit_rate/8;
		wma_cfg.block_align = compr->info.codec_param.codec.align;
		wma_cfg.valid_bits_per_sample =
		compr->info.codec_param.codec.options.wma.bits_per_sample;
		wma_cfg.ch_mask =
			compr->info.codec_param.codec.options.wma.channelmask;
		wma_cfg.encode_opt =
			compr->info.codec_param.codec.options.wma.encodeopt;
		ret = q6asm_media_format_block_wma(prtd->audio_client,
					&wma_cfg);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	case SND_AUDIOCODEC_WMA_PRO:
		pr_debug("SND_AUDIOCODEC_WMA_PRO\n");
		memset(&wma_pro_cfg, 0x0, sizeof(struct asm_wmapro_cfg));
		wma_pro_cfg.format_tag = compr->info.codec_param.codec.format;
		wma_pro_cfg.ch_cfg = compr->info.codec_param.codec.ch_in;
		wma_pro_cfg.sample_rate =
			compr->info.codec_param.codec.sample_rate;
		wma_pro_cfg.avg_bytes_per_sec =
			compr->info.codec_param.codec.bit_rate/8;
		wma_pro_cfg.block_align = compr->info.codec_param.codec.align;
		wma_pro_cfg.valid_bits_per_sample =
			compr->info.codec_param.codec\
				.options.wma.bits_per_sample;
		wma_pro_cfg.ch_mask =
			compr->info.codec_param.codec.options.wma.channelmask;
		wma_pro_cfg.encode_opt =
			compr->info.codec_param.codec.options.wma.encodeopt;
		wma_pro_cfg.adv_encode_opt =
			compr->info.codec_param.codec.options.wma.encodeopt1;
		wma_pro_cfg.adv_encode_opt2 =
			compr->info.codec_param.codec.options.wma.encodeopt2;
		ret = q6asm_media_format_block_wmapro(prtd->audio_client,
				&wma_pro_cfg);
		if (ret < 0)
			pr_err("%s: CMD Format block failed\n", __func__);
		break;
	case SND_AUDIOCODEC_DTS:
	case SND_AUDIOCODEC_DTS_LBR:
		pr_debug("SND_AUDIOCODEC_DTS\n");
		ret = q6asm_media_format_block(prtd->audio_client,
				compr->codec);
		if (ret < 0) {
			pr_err("%s: CMD Format block failed\n", __func__);
			return ret;
		}
		break;
	case SND_AUDIOCODEC_AMRWB:
		pr_debug("SND_AUDIOCODEC_AMRWB\n");
		ret = q6asm_media_format_block(prtd->audio_client,
					compr->codec);
		if (ret < 0) {
			pr_err("%s: CMD Format block failed\n", __func__);
			return ret;
		}
		break;
	case SND_AUDIOCODEC_AMRWBPLUS:
		pr_debug("SND_AUDIOCODEC_AMRWBPLUS\n");
		memset(&amrwb_cfg, 0x0, sizeof(struct asm_amrwbplus_cfg));
		amrwb_cfg.size_bytes = sizeof(struct asm_amrwbplus_cfg);
		pr_debug("calling q6asm_media_format_block_amrwbplus");
		ret = q6asm_media_format_block_amrwbplus(prtd->audio_client,
						&amrwb_cfg);
		if (ret < 0) {
			pr_err("%s: CMD Format block failed\n", __func__);
			return ret;
		}
		break;
	case SND_AUDIOCODEC_MP2:
		pr_debug("%s: SND_AUDIOCODEC_MP2\n", __func__);
		break;
	default:
		return -EINVAL;
	}
	if (compr->info.codec_param.codec.transcode_dts) {
		msm_pcm_routing_reg_pseudo_stream(
			MSM_FRONTEND_DAI_PSEUDO,
			prtd->enc_audio_client->perf_mode,
			prtd->enc_audio_client->session,
			SNDRV_PCM_STREAM_CAPTURE,
			48000, runtime->channels > 6 ?
			6 : runtime->channels);
		pr_debug("%s: cmd: DTS ENCDEC CFG BLK\n", __func__);
		ret = q6asm_enc_cfg_blk_dts(prtd->enc_audio_client,
				DTS_ENC_SAMPLE_RATE48k,
				runtime->channels > 6 ?
				6 : runtime->channels);
		if (ret < 0)
			pr_err("%s: CMD: DTS ENCDEC CFG BLK failed\n",
				__func__);
	}
	prtd->enabled = 1;
	prtd->cmd_ack = 0;

	return 0;
}

static int msm_compr_capture_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = &compr->prtd;
	struct audio_buffer *buf = prtd->audio_client->port[OUT].buf;
	struct snd_codec *codec = &compr->info.codec_param.codec;
	struct audio_aio_read_param read_param;
	int ret = 0;
	int i;
	prtd->pcm_size = snd_pcm_lib_buffer_bytes(substream);
	prtd->pcm_count = snd_pcm_lib_period_bytes(substream);
	prtd->pcm_irq_pos = 0;

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
			codec->options.generic.reserved[0] /*bitrate 0-8*/,
			codec->options.generic.reserved[1] /*dtx mode 0/1*/);
		if (ret < 0)
			pr_err("%s: CMD Format block" \
				"failed: %d\n", __func__, ret);
		break;
	case SND_AUDIOCODEC_PCM:
		pr_debug("SND_AUDIOCODEC_PCM\n");
		ret = q6asm_enc_cfg_blk_multi_ch_pcm(prtd->audio_client,
			 prtd->samp_rate, prtd->channel_mode);
		if (ret < 0)
			pr_info("%s: CMD Format block failed\n", __func__);
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
		case SND_AUDIOCODEC_PCM:
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
		case SND_AUDIOCODEC_PASS_THROUGH:
			read_param.paddr = (unsigned long)(buf[i].phys);
			q6asm_async_read_compressed(prtd->audio_client,
				&read_param);
			break;
		default:
			pr_err("Invalid format");
			ret = -EINVAL;
			break;
		}
	}
	prtd->periods = runtime->periods;

	prtd->enabled = 1;

	if (compr->info.codec_param.codec.id ==
			SND_AUDIOCODEC_PASS_THROUGH)
		msm_pcm_routing_reg_psthr_stream(
					soc_prtd->dai_link->be_id,
					prtd->session_id, substream->stream,
					1);

	return ret;
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

	pr_err("msm_compr_restart\n");
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
		 if (buffer_length == 0) {
			pr_debug("Recieved a zero length buffer-break out");
			return -EINVAL;
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

static int msm_compr_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;

	pr_debug("%s\n", __func__);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		prtd->pcm_irq_pos = 0;
		/* intentional fall-through */
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("%s: Trigger start\n", __func__);
		q6asm_run_nowait(prtd->audio_client, 0, 0, 0);
		if (prtd->enc_audio_client)
			q6asm_run_nowait(prtd->enc_audio_client, 0, 0, 0);
		atomic_set(&prtd->start, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		atomic_set(&prtd->start, 0);
		runtime->render_flag &= ~SNDRV_RENDER_STOPPED;
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("SNDRV_PCM_TRIGGER_PAUSE\n");
		q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
		if (prtd->enc_audio_client)
			q6asm_cmd_nowait(prtd->enc_audio_client, CMD_PAUSE);
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
	compr->info.compr_cap.num_codecs = 14;
	compr->info.compr_cap.min_fragment_size = runtime->hw.period_bytes_min;
	compr->info.compr_cap.max_fragment_size = runtime->hw.period_bytes_max;
	compr->info.compr_cap.min_fragments = runtime->hw.periods_min;
	compr->info.compr_cap.max_fragments = runtime->hw.periods_max;
	compr->info.compr_cap.codecs[0] = SND_AUDIOCODEC_MP3;
	compr->info.compr_cap.codecs[1] = SND_AUDIOCODEC_AAC;
	compr->info.compr_cap.codecs[2] = SND_AUDIOCODEC_AC3_PASS_THROUGH;
	compr->info.compr_cap.codecs[3] = SND_AUDIOCODEC_WMA;
	compr->info.compr_cap.codecs[4] = SND_AUDIOCODEC_WMA_PRO;
	compr->info.compr_cap.codecs[5] = SND_AUDIOCODEC_DTS;
	compr->info.compr_cap.codecs[6] = SND_AUDIOCODEC_DTS_LBR;
	compr->info.compr_cap.codecs[7] = SND_AUDIOCODEC_DTS_PASS_THROUGH;
	compr->info.compr_cap.codecs[8] = SND_AUDIOCODEC_AMRWB;
	compr->info.compr_cap.codecs[9] = SND_AUDIOCODEC_AMRWBPLUS;
	compr->info.compr_cap.codecs[10] = SND_AUDIOCODEC_PASS_THROUGH;
	compr->info.compr_cap.codecs[11] = SND_AUDIOCODEC_PCM;
	compr->info.compr_cap.codecs[12] = SND_AUDIOCODEC_MP2;
	compr->info.compr_cap.codecs[13] = SND_AUDIOCODEC_DTS_LBR_PASS_THROUGH;
	/* Add new codecs here and update num_codecs*/
}

static int msm_compr_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr;
	struct msm_audio *prtd;
	int ret = 0;
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
	compressed_audio.prtd =  &compr->prtd;
	ret = compressed_set_volume(0);
	if (ret < 0)
		pr_err("%s : Set Volume failed : %d", __func__, ret);

	ret = q6asm_set_softpause(compressed_audio.prtd->audio_client,
								&softpause);
	if (ret < 0)
		pr_err("%s: Send SoftPause Param failed ret=%d\n",
			__func__, ret);
	ret = q6asm_set_softvolume(compressed_audio.prtd->audio_client,
								&softvol);
	if (ret < 0)
		pr_err("%s: Send SoftVolume Param failed ret=%d\n",
			__func__, ret);

	return 0;
}

int compressed_set_volume(unsigned volume)
{
	int rc = 0;
	if (compressed_audio.prtd && compressed_audio.prtd->audio_client) {
		rc = q6asm_set_volume(compressed_audio.prtd->audio_client,
								 volume);
		if (rc < 0) {
			pr_err("%s: Send Volume command failed"
					" rc=%d\n", __func__, rc);
		}
	}
	compressed_audio.volume = volume;
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
	prtd->pcm_irq_pos = 0;
	q6asm_cmd(prtd->audio_client, CMD_CLOSE);
	if (prtd->enc_audio_client)
		q6asm_cmd(prtd->enc_audio_client, CMD_CLOSE);
	compressed_audio.prtd = NULL;
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
	switch (compr->info.codec_param.codec.id) {
	case SND_AUDIOCODEC_AC3_PASS_THROUGH:
	case SND_AUDIOCODEC_DTS_PASS_THROUGH:
	case SND_AUDIOCODEC_DTS_LBR_PASS_THROUGH:
		msm_pcm_routing_reg_psthr_stream(
			soc_prtd->dai_link->be_id,
			prtd->session_id, substream->stream,
			0);
	default:
		msm_pcm_routing_dereg_phy_stream(
			soc_prtd->dai_link->be_id,
			SNDRV_PCM_STREAM_PLAYBACK);
	}
	if (compr->info.codec_param.codec.transcode_dts) {
		msm_pcm_routing_dereg_pseudo_stream(MSM_FRONTEND_DAI_PSEUDO,
			prtd->enc_audio_client->session);
	}
	if (prtd->enc_audio_client)
		q6asm_audio_client_free(prtd->enc_audio_client);
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
	compressed_audio.prtd = NULL;
	q6asm_audio_client_buf_free_contiguous(dir,
				prtd->audio_client);
	if (compr->info.codec_param.codec.id ==
			SND_AUDIOCODEC_PASS_THROUGH)
		msm_pcm_routing_reg_psthr_stream(
					soc_prtd->dai_link->be_id,
					prtd->session_id, substream->stream,
					0);
	else
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
	int result = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct compr_audio *compr = runtime->private_data;
	struct msm_audio *prtd = &compr->prtd;

	pr_debug("%s\n", __func__);
	prtd->mmap_flag = 1;
	runtime->render_flag = SNDRV_NON_DMA_MODE;
	if (runtime->dma_addr && runtime->dma_bytes) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		result = remap_pfn_range(vma, vma->vm_start,
				runtime->dma_addr >> PAGE_SHIFT,
				runtime->dma_bytes,
				vma->vm_page_prot);
	} else {
		pr_err("Physical address or size of buf is NULL");
		return -EINVAL;
	}
	return result;
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

	pr_debug("%s\n", __func__);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;


	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (compr->info.codec_param.codec.id) {
		case SND_AUDIOCODEC_AC3_PASS_THROUGH:
		case SND_AUDIOCODEC_DTS_PASS_THROUGH:
		case SND_AUDIOCODEC_DTS_LBR_PASS_THROUGH:
			ret = q6asm_open_write_compressed(prtd->audio_client,
					compr->codec);

			if (ret < 0) {
				pr_err("%s: Session out open failed\n",
					__func__);
				return -ENOMEM;
			}
			break;
		default:
			ret = q6asm_open_write(prtd->audio_client,
					compr->codec);
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

			if (compr->info.codec_param.codec.transcode_dts) {
				prtd->enc_audio_client =
					q6asm_audio_client_alloc(
					(app_cb)compr_event_handler, compr);
				if (!prtd->enc_audio_client) {
					pr_err("%s: Could not allocate " \
							"memory\n", __func__);
					return -ENOMEM;
				}
				prtd->enc_audio_client->perf_mode = false;
				pr_debug("%s Setting up loopback path\n",
						__func__);
				ret = q6asm_open_transcode_loopback(
					prtd->enc_audio_client,
					params_channels(params));
				if (ret < 0) {
					pr_err("%s: Session transcode " \
						"loopback open failed\n",
						__func__);
					return -ENODEV;
				}
			}

			break;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
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
		case SND_AUDIOCODEC_PCM:
			pr_debug("q6asm_open_read(FORMAT_PCM)\n");
			ret = q6asm_open_read(prtd->audio_client,
				FORMAT_MULTI_CHANNEL_LINEAR_PCM);
			if (ret < 0) {
				pr_err("%s: compressed Session open failed\n",
					__func__);
				return -ENOMEM;
			}
			pr_debug("msm_pcm_routing_reg_phy_stream\n");
			msm_pcm_routing_reg_phy_stream(
					soc_prtd->dai_link->be_id,
					prtd->audio_client->perf_mode,
					prtd->session_id, substream->stream);
			break;
		case SND_AUDIOCODEC_PASS_THROUGH:
			pr_debug("q6asm_open_read_compressed(COMPRESSED_META_DATA_MODE)\n");
			ret = q6asm_open_read_compressed(prtd->audio_client,
				MAX_NUM_FRAMES_PER_BUFFER,
				COMPRESSED_META_DATA_MODE);
			break;
		default:
			pr_err("Invalid codec for compressed session open\n");
			return -EFAULT;
		}

		if (ret < 0) {
			pr_err("%s: compressed Session out open failed\n",
				__func__);
			return -ENOMEM;
		}
	}

	ret = q6asm_set_io_mode(prtd->audio_client, ASYNC_IO_MODE);
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
	ret = q6asm_audio_client_buf_alloc_contiguous(dir,
			prtd->audio_client,
			runtime->hw.period_bytes_min,
			runtime->hw.periods_max);
	if (ret < 0) {
		pr_err("Audio Start: Buffer Allocation failed "
					"rc = %d\n", ret);
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
			pr_err("%s: fail to get session tstamp\n", __func__);
			return rc;
		}
		temp = (timestamp * 2 * runtime->channels);
		temp = temp * (runtime->rate/1000);
		temp = div_u64(temp, 1000);
		tstamp.sampling_rate = runtime->rate;
		tstamp.timestamp = timestamp;
		pr_debug("%s: bytes_consumed:,"
			"timestamp = %lld,\n", __func__,
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
		pr_debug("SNDRV_COMPRESS_SET_PARAMS: ");
		if (copy_from_user(&compr->info.codec_param, (void *) arg,
			sizeof(struct snd_compr_params))) {
			rc = -EFAULT;
			pr_err("%s: ERROR: copy from user\n", __func__);
			return rc;
		}
		/*
		* DTS Security needed for the transcode path
		*/
		if (compr->info.codec_param.codec.transcode_dts) {
			char modelId[128];
			struct snd_dec_dts opt_dts =
				compr->info.codec_param.codec.dts;
			unsigned int modelIdLength = opt_dts.modelIdLength;
			if (modelIdLength >= sizeof(modelId)) {
				rc = -EINVAL;
				pr_err("%s: ERROR: modelIdLength is"
						"invalid\n", __func__);
				return rc;
			}
			if (copy_from_user(modelId, (void *)opt_dts.modelId,
				modelIdLength))
				pr_err("%s: ERROR: copy modelId\n", __func__);
			modelId[modelIdLength] = '\0';
			pr_debug("%s: Received modelId =%s,length=%d\n",
				__func__, modelId, modelIdLength);
			core_set_dts_model_id(modelIdLength, modelId);
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
		case SND_AUDIOCODEC_AC3_PASS_THROUGH:
			pr_debug("SND_AUDIOCODEC_AC3_PASS_THROUGH\n");
			compr->codec = FORMAT_AC3;
			break;
		case SND_AUDIOCODEC_WMA:
			pr_debug("SND_AUDIOCODEC_WMA\n");
			compr->codec = FORMAT_WMA_V9;
			break;
		case SND_AUDIOCODEC_WMA_PRO:
			pr_debug("SND_AUDIOCODEC_WMA_PRO\n");
			compr->codec = FORMAT_WMA_V10PRO;
			break;
		case SND_AUDIOCODEC_DTS_PASS_THROUGH:
			pr_debug("SND_AUDIOCODEC_DTS_PASS_THROUGH\n");
			compr->codec = FORMAT_DTS;
			break;
		case SND_AUDIOCODEC_DTS_LBR_PASS_THROUGH:
			pr_debug("SND_AUDIOCODEC_DTS_LBR_PASS_THROUGH\n");
			compr->codec = FORMAT_DTS_LBR;
			break;
		case SND_AUDIOCODEC_DTS: {
			char modelId[128];
			struct snd_dec_dts opt_dts =
				compr->info.codec_param.codec.dts;
			unsigned int modelIdLength = opt_dts.modelIdLength;
			pr_debug("SND_AUDIOCODEC_DTS\n");
			if (modelIdLength >= sizeof(modelId)) {
				rc = -EINVAL;
				pr_err("%s: ERROR: modelIdLength is"
						"invalid\n", __func__);
				return rc;
			}
			if (copy_from_user(modelId, (void *)opt_dts.modelId,
				modelIdLength))
				pr_err("%s: ERROR: copy modelId\n", __func__);
			modelId[modelIdLength] = '\0';
			pr_debug("%s: Received modelId =%s,length=%d\n",
				__func__, modelId, modelIdLength);
			core_set_dts_model_id(modelIdLength, modelId);
			compr->codec = FORMAT_DTS;
			}
			break;
		case SND_AUDIOCODEC_DTS_LBR:{
			char modelId[128];
			struct snd_dec_dts opt_dts =
				compr->info.codec_param.codec.dts;
			int modelIdLength = opt_dts.modelIdLength;
			pr_debug("SND_AUDIOCODEC_DTS_LBR\n");
			if (copy_from_user(modelId, (void *)opt_dts.modelId,
					modelIdLength))
				pr_err("%s: ERROR: copy modelId\n", __func__);
			modelId[modelIdLength] = '\0';
			pr_debug("%s: Received modelId =%s,length=%d\n",
				__func__, modelId, modelIdLength);
			core_set_dts_model_id(modelIdLength, modelId);
			compr->codec = FORMAT_DTS_LBR;
			}
			break;
		case SND_AUDIOCODEC_AMRWB:
			pr_debug("msm_compr_ioctl SND_AUDIOCODEC_AMRWB\n");
			compr->codec = FORMAT_AMRWB;
			break;
		case SND_AUDIOCODEC_AMRWBPLUS:
			pr_debug("msm_compr_ioctl SND_AUDIOCODEC_AMRWBPLUS\n");
			compr->codec = FORMAT_AMR_WB_PLUS;
			break;
		case SND_AUDIOCODEC_PASS_THROUGH:
			/* format pass through is used for HDMI IN compressed
			   where the decoder format is indicated by LPASS */
			pr_debug("msm_compr_ioctl SND_AUDIOCODEC_PASSTHROUGH\n");
			compr->codec = FORMAT_PASS_THROUGH;
			break;
		case SND_AUDIOCODEC_PCM:
			pr_debug("msm_compr_ioctl SND_AUDIOCODEC_PCM\n");
			compr->codec = FORMAT_MULTI_CHANNEL_LINEAR_PCM;
			break;
		case SND_AUDIOCODEC_MP2:
			pr_debug("SND_AUDIOCODEC_MP2\n");
			compr->codec = FORMAT_MP2;
			break;
		default:
			pr_err("msm_compr_ioctl failed..unknown codec\n");
			return -EFAULT;
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
				prtd->cmd_ack = 1;
				wake_up(&the_locks.eos_wait);
				atomic_set(&prtd->eos, 0);
				atomic_set(&prtd->pending_buffer, 1);
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
		atomic_set(&prtd->eos, 1);
		atomic_set(&prtd->pending_buffer, 0);
		prtd->cmd_ack = 0;
		q6asm_cmd_nowait(prtd->audio_client, CMD_EOS);
		/* Wait indefinitely for  DRAIN. Flush can also signal this*/
		rc = wait_event_interruptible(the_locks.eos_wait,
			prtd->cmd_ack);
		if (rc < 0)
			pr_err("EOS cmd interrupted\n");
		pr_debug("%s: SNDRV_COMPRESS_DRAIN  out of wait\n", __func__);
		return 0;
	default:
		break;
	}
	return snd_pcm_lib_ioctl(substream, cmd, arg);
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
	return ret;
}

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_compr_ops,
	.pcm_new	= msm_asoc_pcm_new,
};

static __devinit int msm_compr_probe(struct platform_device *pdev)
{
	pr_info("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
				   &msm_soc_platform);
}

static int msm_compr_remove(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static struct platform_driver msm_compr_driver = {
	.driver = {
		.name = "msm-compr-dsp",
		.owner = THIS_MODULE,
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
