/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/msm_audio_ion.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/q6audio-v2.h>
#include <sound/timer.h>
#include <asm/dma.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>

#include "msm-pcm-q6-v2.h"
#include "msm-pcm-routing-v2.h"

#define PCM_MASTER_VOL_MAX_STEPS	0x2000
static const DECLARE_TLV_DB_LINEAR(msm_pcm_vol_gain, 0,
			PCM_MASTER_VOL_MAX_STEPS);

struct snd_msm {
	struct snd_card *card;
	struct snd_pcm *pcm;
};

#define CMD_EOS_MIN_TIMEOUT_LENGTH  50
#define CMD_EOS_TIMEOUT_MULTIPLIER  (HZ * 50)

#define ATRACE_END() \
	trace_printk("tracing_mark_write: E\n")
#define ATRACE_BEGIN(name) \
	trace_printk("tracing_mark_write: B|%d|%s\n", current->tgid, name)
#define ATRACE_FUNC() ATRACE_BEGIN(__func__)
#define ATRACE_INT(name, value) \
	trace_printk("tracing_mark_write: C|%d|%s|%d\n", \
			current->tgid, name, (int)(value))

#define SIO_PLAYBACK_MAX_PERIOD_SIZE PLAYBACK_MAX_PERIOD_SIZE
#define SIO_PLAYBACK_MIN_PERIOD_SIZE 48
#define SIO_PLAYBACK_MAX_NUM_PERIODS 512
#define SIO_PLAYBACK_MIN_NUM_PERIODS PLAYBACK_MIN_NUM_PERIODS
#define SIO_PLAYBACK_MIN_BYTES (SIO_PLAYBACK_MIN_NUM_PERIODS *	\
				SIO_PLAYBACK_MIN_PERIOD_SIZE)

#define SIO_PLAYBACK_MAX_BYTES ((SIO_PLAYBACK_MAX_NUM_PERIODS) *	\
				(SIO_PLAYBACK_MAX_PERIOD_SIZE))

#define SIO_CAPTURE_MAX_PERIOD_SIZE CAPTURE_MAX_PERIOD_SIZE
#define SIO_CAPTURE_MIN_PERIOD_SIZE 48
#define SIO_CAPTURE_MAX_NUM_PERIODS 512
#define SIO_CAPTURE_MIN_NUM_PERIODS CAPTURE_MIN_NUM_PERIODS

#define SIO_CAPTURE_MIN_BYTES (SIO_CAPTURE_MIN_NUM_PERIODS *	\
			       SIO_CAPTURE_MIN_PERIOD_SIZE)

#define SIO_CAPTURE_MAX_BYTES (SIO_CAPTURE_MAX_NUM_PERIODS *	\
				SIO_CAPTURE_MAX_PERIOD_SIZE)

static struct snd_pcm_hardware msm_pcm_hardware_playback = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE),
	.rates =                SNDRV_PCM_RATE_8000_192000,
	.rate_min =             8000,
	.rate_max =             192000,
	.channels_min =         1,
	.channels_max =         8,
	.buffer_bytes_max =     SIO_PLAYBACK_MAX_NUM_PERIODS *
				SIO_PLAYBACK_MAX_PERIOD_SIZE,
	.period_bytes_min =	SIO_PLAYBACK_MIN_PERIOD_SIZE,
	.period_bytes_max =     SIO_PLAYBACK_MAX_PERIOD_SIZE,
	.periods_min =          SIO_PLAYBACK_MIN_NUM_PERIODS,
	.periods_max =          SIO_PLAYBACK_MAX_NUM_PERIODS,
	.fifo_size =            0,
};

static struct snd_pcm_hardware msm_pcm_hardware_capture = {
	.info =                 (SNDRV_PCM_INFO_MMAP |
				SNDRV_PCM_INFO_BLOCK_TRANSFER |
				SNDRV_PCM_INFO_MMAP_VALID |
				SNDRV_PCM_INFO_INTERLEAVED |
				SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
				SNDRV_PCM_INFO_PAUSE | SNDRV_PCM_INFO_RESUME),
	.formats =              (SNDRV_PCM_FMTBIT_S16_LE |
				SNDRV_PCM_FMTBIT_S24_LE),
	.rates =                SNDRV_PCM_RATE_8000_48000,
	.rate_min =             8000,
	.rate_max =             48000,
	.channels_min =         1,
	.channels_max =         4,
	.buffer_bytes_max =     SIO_CAPTURE_MAX_NUM_PERIODS *
				SIO_CAPTURE_MAX_PERIOD_SIZE,
	.period_bytes_min =	SIO_CAPTURE_MIN_PERIOD_SIZE,
	.period_bytes_max =     SIO_CAPTURE_MAX_PERIOD_SIZE,
	.periods_min =          SIO_CAPTURE_MIN_NUM_PERIODS,
	.periods_max =          SIO_CAPTURE_MAX_NUM_PERIODS,
	.fifo_size =            0,
};

/* Conventional and unconventional sample rate supported */
static unsigned int supported_sample_rates[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000,
	88200, 96000, 176400, 192000
};

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(supported_sample_rates),
	.list = supported_sample_rates,
	.mask = 0,
};

static void event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	uint32_t *ptrmem = (uint32_t *)payload;

	switch (opcode) {
	case ASM_DATA_EVENT_WATERMARK:
		pr_debug("%s: Watermark level = 0x%08x\n", __func__, *ptrmem);
		break;
	case APR_BASIC_RSP_RESULT:
		pr_debug("%s: Payload = [0x%x]stat[0x%x]\n",
				__func__, payload[0], payload[1]);
		switch (payload[0]) {
		case ASM_SESSION_CMD_RUN_V2:
		case ASM_SESSION_CMD_PAUSE:
		case ASM_STREAM_CMD_FLUSH:
			break;
		default:
			break;
		}
		break;
	default:
		pr_debug("Not Supported Event opcode[0x%x]\n", opcode);
		break;
	}
}

static int msm_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd;
	int ret = 0;

	prtd = kzalloc(sizeof(struct msm_audio), GFP_KERNEL);

	if (prtd == NULL)
		return -ENOMEM;

	prtd->substream = substream;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		runtime->hw = msm_pcm_hardware_playback;
	else
		runtime->hw = msm_pcm_hardware_capture;

	ret = snd_pcm_hw_constraint_list(runtime, 0,
				SNDRV_PCM_HW_PARAM_RATE,
				&constraints_sample_rates);
	if (ret)
		pr_info("snd_pcm_hw_constraint_list failed\n");

	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret)
		pr_info("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		ret = snd_pcm_hw_constraint_minmax(runtime,
			SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
			SIO_PLAYBACK_MIN_BYTES,
			SIO_PLAYBACK_MAX_BYTES);
		if (ret) {
			pr_info("%s: P buffer bytes minmax constraint ret %d\n",
			       __func__, ret);
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		ret = snd_pcm_hw_constraint_minmax(runtime,
			   SNDRV_PCM_HW_PARAM_BUFFER_BYTES,
			   SIO_CAPTURE_MIN_BYTES,
			   SIO_CAPTURE_MAX_BYTES);
		if (ret) {
			pr_info("%s: C buffer bytes minmax constraint ret %d\n",
			       __func__, ret);
		}
	}

	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 32);
	if (ret) {
		pr_err("%s: Constraint for period bytes step ret = %d\n",
				__func__, ret);
	}
	ret = snd_pcm_hw_constraint_step(runtime, 0,
		SNDRV_PCM_HW_PARAM_BUFFER_BYTES, 32);
	if (ret) {
		pr_err("%s: Constraint for buffer bytes step ret = %d\n",
				__func__, ret);
	}
	prtd->audio_client = q6asm_audio_client_alloc(
				(app_cb)event_handler, prtd);
	if (!prtd->audio_client) {
		pr_err("%s: client alloc failed\n", __func__);
		ret = -ENOMEM;
		goto fail_cmd;
	}
	prtd->dsp_cnt = 0;
	prtd->set_channel_map = false;
	runtime->private_data = prtd;
	return 0;

fail_cmd:
	kfree(prtd);
	return ret;
}

static int msm_pcm_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)

{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	struct msm_plat_data *pdata;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	struct shared_io_config config;
	uint16_t sample_word_size;
	uint16_t bits_per_sample = 16;
	int ret;
	int dir = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? IN : OUT;

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(soc_prtd->platform->dev);
	if (!pdata) {
		ret = -EINVAL;
		pr_err("%s: platform data not populated ret: %d\n", __func__,
		       ret);
		return ret;
	}

	/* need to set LOW_LATENCY_PCM_MODE for capture since
	 * push mode does not support ULL
	 */
	prtd->audio_client->perf_mode = (dir == IN) ?
					pdata->perf_mode :
					LOW_LATENCY_PCM_MODE;

	/* rate and channels are sent to audio driver */
	prtd->samp_rate = params_rate(params);
	prtd->channel_mode = params_channels(params);
	if (prtd->enabled)
		return 0;

	switch (runtime->format) {
	case SNDRV_PCM_FORMAT_S16_LE:
		bits_per_sample = 16;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		bits_per_sample = 24;
		break;
	}

	sample_word_size = (bits_per_sample == 16) ? 16 : 32;

	config.format = FORMAT_LINEAR_PCM;
	config.bits_per_sample = bits_per_sample;
	config.rate = params_rate(params);
	config.channels = params_channels(params);
	config.sample_word_size = sample_word_size;
	config.bufsz = params_buffer_bytes(params) / params_periods(params);
	config.bufcnt = params_periods(params);

	ret = q6asm_open_shared_io(prtd->audio_client, &config, dir);
	if (ret) {
		pr_err("%s: q6asm_open_write_shared_io failed ret: %d\n",
		       __func__, ret);
		return ret;
	}

	prtd->pcm_size = params_buffer_bytes(params);
	prtd->pcm_count = params_buffer_bytes(params);
	prtd->pcm_irq_pos = 0;

	buf = prtd->audio_client->port[dir].buf;
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->area = buf->data;
	dma_buf->addr = buf->phys;
	dma_buf->bytes = prtd->pcm_size;

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	pr_debug("%s: session ID %d, perf %d\n", __func__,
	       prtd->audio_client->session,
		prtd->audio_client->perf_mode);
	prtd->session_id = prtd->audio_client->session;

	pr_debug("msm_pcm_routing_reg_phy_stream w/ id %d\n",
		 soc_prtd->dai_link->be_id);
	ret = msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->be_id,
				       prtd->audio_client->perf_mode,
				       prtd->session_id, substream->stream);

	if (ret) {
		pr_err("%s: stream reg failed ret:%d\n", __func__, ret);
		return ret;
	}

	atomic_set(&prtd->out_count, runtime->periods);
	prtd->enabled = 1;
	prtd->cmd_pending = 0;
	prtd->cmd_interrupt = 0;

	return 0;
}

static int msm_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	int dir = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0 : 1;
	struct audio_buffer *buf;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pr_debug("%s: %s Trigger start\n", __func__,
			 dir == 0 ? "P" : "C");
		ret = q6asm_run(prtd->audio_client, 0, 0, 0);
		if (ret)
			break;
		atomic_set(&prtd->start, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		pr_debug("%s: SNDRV_PCM_TRIGGER_STOP\n", __func__);
		atomic_set(&prtd->start, 0);
		q6asm_cmd(prtd->audio_client, CMD_PAUSE);
		q6asm_cmd(prtd->audio_client, CMD_FLUSH);
		buf = q6asm_shared_io_buf(prtd->audio_client, dir);
		memset(buf->data, 0, buf->actual_size);
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		pr_debug("%s: SNDRV_PCM_TRIGGER_PAUSE\n", __func__);
		ret = q6asm_cmd_nowait(prtd->audio_client, CMD_PAUSE);
		atomic_set(&prtd->start, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static snd_pcm_uframes_t msm_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	uint32_t read_index, wall_clk_msw, wall_clk_lsw;
	/*these are offsets, unlike ASoC's full values*/
	snd_pcm_sframes_t hw_ptr;
	snd_pcm_sframes_t period_size;
	int ret;
	int retries = 10;
	struct msm_audio *prtd = runtime->private_data;

	period_size = runtime->period_size;

	do {
		ret = q6asm_get_shared_pos(prtd->audio_client,
					   &read_index, &wall_clk_msw,
					   &wall_clk_lsw);
	} while (ret == -EAGAIN && --retries);

	if (ret || !period_size) {
		pr_err("get_shared_pos error or zero period size\n");
		return 0;
	}

	hw_ptr = bytes_to_frames(substream->runtime,
				 read_index);

	if (runtime->control->appl_ptr == 0) {
		pr_debug("ptr(%s): appl(0), hw = %lu read_index = %u\n",
			 prtd->substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
			 "P" : "C",
			 hw_ptr, read_index);
	}
	return (hw_ptr/period_size) * period_size;
}

static int msm_pcm_copy(struct snd_pcm_substream *substream, int a,
	 snd_pcm_uframes_t hwoff, void __user *buf, snd_pcm_uframes_t frames)
{
	return -ENOSYS;
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
	int ret;

	pr_debug("%s: mmap begin\n", __func__);
	prtd->mmap_flag = 1;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;

	ab = &(apd[dir].buf[0]);

	ret = msm_audio_ion_mmap(ab, vma);

	if (ret)
		prtd->mmap_flag = 0;

	return ret;
}

static int msm_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;

	if (!prtd || !prtd->mmap_flag)
		return -EIO;

	return 0;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_client *ac = prtd->audio_client;
	uint32_t timeout;
	int dir = 0;
	int ret = 0;

	if (ac) {
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
			dir = IN;
		else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
			dir = OUT;

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

		q6asm_cmd(ac, CMD_CLOSE);

		ret = q6asm_shared_io_free(ac, dir);

		if (ret) {
			pr_err("%s: Failed to close pull mode, ret %d\n",
					__func__, ret);
		}
		q6asm_audio_client_free(ac);
	}
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->be_id,
					 dir == IN ?
					 SNDRV_PCM_STREAM_PLAYBACK :
					 SNDRV_PCM_STREAM_CAPTURE);
	kfree(prtd);
	return 0;
}

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
	struct snd_pcm_substream *substream =
		vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;

	pr_debug("%s\n", __func__);
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -ENODEV;
	}
	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		return 0;
	}
	prtd = substream->runtime->private_data;
	if (prtd)
		ucontrol->value.integer.value[0] = prtd->volume;
	return 0;
}

static int msm_pcm_volume_ctl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int rc = 0;
	struct snd_pcm_volume *vol = snd_kcontrol_chip(kcontrol);
	struct snd_pcm_substream *substream =
		vol->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	struct msm_audio *prtd;
	int volume = ucontrol->value.integer.value[0];

	pr_debug("%s: volume : 0x%x\n", __func__, volume);
	if (!substream) {
		pr_err("%s substream not found\n", __func__);
		return -ENODEV;
	}
	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		return 0;
	}
	prtd = substream->runtime->private_data;
	if (prtd) {
		rc = msm_pcm_set_volume(prtd, volume);
		prtd->volume = volume;
	}
	return rc;
}

static int msm_pcm_add_volume_control(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_volume *volume_info;
	struct snd_kcontrol *kctl;

	dev_dbg(rtd->dev, "%s, Volume control add\n", __func__);
	ret = snd_pcm_add_volume_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
			NULL, 1, rtd->dai_link->be_id,
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

static int msm_pcm_chmap_ctl_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int i;
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;

	pr_debug("%s", __func__);
	substream = snd_pcm_chmap_substream(info, idx);
	if (!substream)
		return -ENODEV;
	if (!substream->runtime)
		return 0;

	prtd = substream->runtime->private_data;
	if (prtd) {
		prtd->set_channel_map = true;
			for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
				prtd->channel_map[i] =
				(char)(ucontrol->value.integer.value[i]);
	}
	return 0;
}

static int msm_pcm_chmap_ctl_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	int i;
	struct snd_pcm_chmap *info = snd_kcontrol_chip(kcontrol);
	unsigned int idx = snd_ctl_get_ioffidx(kcontrol, &ucontrol->id);
	struct snd_pcm_substream *substream;
	struct msm_audio *prtd;

	pr_debug("%s", __func__);
	substream = snd_pcm_chmap_substream(info, idx);
	if (!substream)
		return -ENODEV;
	memset(ucontrol->value.integer.value, 0,
		sizeof(ucontrol->value.integer.value));
	if (!substream->runtime)
		return 0; /* no channels set */

	prtd = substream->runtime->private_data;

	if (prtd && prtd->set_channel_map == true) {
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
			ucontrol->value.integer.value[i] =
					(int)prtd->channel_map[i];
	} else {
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
			ucontrol->value.integer.value[i] = 0;
	}

	return 0;
}

static int msm_pcm_add_chmap_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_chmap *chmap_info;
	struct snd_kcontrol *kctl;
	char device_num[12];
	int i, ret;

	pr_debug("%s, Channel map cntrl add\n", __func__);
	ret = snd_pcm_add_chmap_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				     snd_pcm_std_chmaps,
				     PCM_FORMAT_MAX_NUM_CHANNEL, 0,
				     &chmap_info);
	if (ret)
		return ret;

	kctl = chmap_info->kctl;
	for (i = 0; i < kctl->count; i++)
		kctl->vd[i].access |= SNDRV_CTL_ELEM_ACCESS_WRITE;
	snprintf(device_num, sizeof(device_num), "%d", pcm->device);
	strlcat(kctl->id.name, device_num, sizeof(kctl->id.name));
	pr_debug("%s, Overwriting channel map control name to: %s",
		__func__, kctl->id.name);
	kctl->put = msm_pcm_chmap_ctl_put;
	kctl->get = msm_pcm_chmap_ctl_get;
	return 0;
}

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret;

	pr_debug("%s , register new control\n", __func__);
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_pcm_add_chmap_control(rtd);
	if (ret) {
		pr_err("%s failed to add chmap cntls\n", __func__);
		goto exit;
	}
	ret = msm_pcm_add_volume_control(rtd);
	if (ret) {
		pr_err("%s: Could not add pcm Volume Control %d\n",
			__func__, ret);
	}
	pcm->nonatomic = true;
exit:
	return ret;
}


static struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.prepare        = msm_pcm_prepare,
	.copy           = msm_pcm_copy,
	.hw_params	= msm_pcm_hw_params,
	.ioctl          = snd_pcm_lib_ioctl,
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
	.mmap           = msm_pcm_mmap,
	.close          = msm_pcm_close,
};

static struct snd_soc_platform_driver msm_soc_platform = {
	.ops		= &msm_pcm_ops,
	.pcm_new	= msm_asoc_pcm_new,
};

static int msm_pcm_probe(struct platform_device *pdev)
{
	int rc;
	struct msm_plat_data *pdata;
	const char *latency_level;
	int perf_mode = LOW_LATENCY_PCM_MODE;

	dev_dbg(&pdev->dev, "Pull mode driver probe\n");

	if (of_property_read_bool(pdev->dev.of_node,
				  "qcom,msm-pcm-low-latency")) {

		rc = of_property_read_string(pdev->dev.of_node,
			"qcom,latency-level", &latency_level);
		if (!rc && !strcmp(latency_level, "ultra"))
			perf_mode = ULTRA_LOW_LATENCY_PCM_MODE;
	}

	pdata = devm_kzalloc(&pdev->dev,
			     sizeof(struct msm_plat_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->perf_mode = perf_mode;

	dev_set_drvdata(&pdev->dev, pdata);

	dev_dbg(&pdev->dev, "%s: dev name %s\n",
				__func__, dev_name(&pdev->dev));
	dev_dbg(&pdev->dev, "Pull mode driver register\n");
	rc = snd_soc_register_platform(&pdev->dev,
				       &msm_soc_platform);

	if (rc)
		dev_err(&pdev->dev, "Failed to register pull mode driver\n");

	return rc;
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	struct msm_plat_data *pdata;

	dev_dbg(&pdev->dev, "Pull mode remove\n");
	pdata = dev_get_drvdata(&pdev->dev);
	devm_kfree(&pdev->dev, pdata);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}
static const struct of_device_id msm_pcm_dt_match[] = {
	{.compatible = "qcom,msm-pcm-dsp-noirq"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_pcm_dt_match);

static struct platform_driver msm_pcm_driver_noirq = {
	.driver = {
		.name = "msm-pcm-dsp-noirq",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_dt_match,
	},
	.probe = msm_pcm_probe,
	.remove = msm_pcm_remove,
};

static int __init msm_soc_platform_init(void)
{
	return platform_driver_register(&msm_pcm_driver_noirq);
}
module_init(msm_soc_platform_init);

static void __exit msm_soc_platform_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver_noirq);
}
module_exit(msm_soc_platform_exit);

MODULE_DESCRIPTION("PCM NOIRQ module platform driver");
MODULE_LICENSE("GPL v2");
