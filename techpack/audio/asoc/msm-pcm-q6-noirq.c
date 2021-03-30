// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/dma-buf.h>

#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/timer.h>
#include <sound/hwdep.h>

#include <asm/dma.h>
#include <sound/tlv.h>
#include <sound/pcm_params.h>
#include <audio/sound/devdep_params.h>
#include <dsp/msm_audio_ion.h>
#include <dsp/q6audio-v2.h>

#include "msm-pcm-q6-v2.h"
#include "msm-pcm-routing-v2.h"


#define DRV_NAME "msm-pcm-q6-noirq"

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
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE),
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
				SNDRV_PCM_FMTBIT_S24_LE |
				SNDRV_PCM_FMTBIT_S24_3LE),
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

static unsigned long msm_pcm_fe_topology[MSM_FRONTEND_DAI_MAX];

/* default value is DTS (i.e read from device tree) */
static char const *msm_pcm_fe_topology_text[] = {
	"DTS", "ULL", "ULL_PP", "LL" };

static const struct soc_enum msm_pcm_fe_topology_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(msm_pcm_fe_topology_text),
			    msm_pcm_fe_topology_text),
};

static void event_handler(uint32_t opcode,
		uint32_t token, uint32_t *payload, void *priv)
{
	struct msm_audio *prtd = priv;
	struct snd_pcm_substream *substream;
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
	case RESET_EVENTS:
		if (!prtd || !prtd->substream) {
			pr_err("%s: prtd or substream is NULL\n", __func__);
			return;
		}
		substream = prtd->substream;
		if (!substream->runtime || !substream->runtime->status) {
			pr_err("%s: runtime or runtime->status is NULL\n", __func__);
			return;
		}
		substream->runtime->status->state = SNDRV_PCM_STATE_DISCONNECTED;
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
	enum apr_subsys_state subsys_state;

	subsys_state = apr_get_subsys_state();
	if (subsys_state == APR_SUBSYS_DOWN) {
		pr_debug("%s: adsp is down\n", __func__);
		return -ENETRESET;
	}
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
	struct snd_soc_component *component =
				snd_soc_rtdcom_lookup(soc_prtd, DRV_NAME);
	struct msm_audio *prtd = runtime->private_data;
	struct msm_plat_data *pdata;
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct audio_buffer *buf;
	struct shared_io_config config;
	uint16_t sample_word_size;
	uint16_t bits_per_sample;
	int ret;
	int dir = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? IN : OUT;
	unsigned long topology;
	int perf_mode;
	bool use_default_chmap = true;
	char *chmap = NULL;

	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	pdata = (struct msm_plat_data *)
		dev_get_drvdata(component->dev);
	if (!pdata) {
		ret = -EINVAL;
		pr_err("%s: platform data not populated ret: %d\n", __func__,
		       ret);
		return ret;
	}

	topology = msm_pcm_fe_topology[soc_prtd->dai_link->id];

	if (!strcmp(msm_pcm_fe_topology_text[topology], "ULL_PP"))
		perf_mode = ULL_POST_PROCESSING_PCM_MODE;
	else if (!strcmp(msm_pcm_fe_topology_text[topology], "ULL"))
		perf_mode = ULTRA_LOW_LATENCY_PCM_MODE;
	else if (!strcmp(msm_pcm_fe_topology_text[topology], "LL"))
		perf_mode = LOW_LATENCY_PCM_MODE;
	else
		/* use the default from the device tree */
		perf_mode = pdata->perf_mode;


	/* need to set LOW_LATENCY_PCM_MODE for capture since
	 * push mode does not support ULL
	 */
	prtd->audio_client->perf_mode = (dir == IN) ?
					perf_mode :
					LOW_LATENCY_PCM_MODE;

	/* rate and channels are sent to audio driver */
	prtd->samp_rate = params_rate(params);
	prtd->channel_mode = params_channels(params);
	if (prtd->enabled)
		return 0;

	if (pdata->ch_map[soc_prtd->dai_link->id]) {
		use_default_chmap =
			!(pdata->ch_map[soc_prtd->dai_link->id]->set_ch_map);
		chmap =
			pdata->ch_map[soc_prtd->dai_link->id]->channel_map;
	}

	switch (runtime->format) {
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

	config.format = FORMAT_LINEAR_PCM;
	config.bits_per_sample = bits_per_sample;
	config.rate = params_rate(params);
	config.channels = params_channels(params);
	config.sample_word_size = sample_word_size;
	config.bufsz = params_buffer_bytes(params) / params_periods(params);
	config.bufcnt = params_periods(params);

	ret = q6asm_open_shared_io(prtd->audio_client, &config, dir,
				   use_default_chmap, chmap);
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
		 soc_prtd->dai_link->id);
	ret = msm_pcm_routing_reg_phy_stream(soc_prtd->dai_link->id,
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
		if (buf == NULL) {
			pr_err("%s: shared IO buffer is null\n", __func__);
			ret = -EINVAL;
			break;
		}
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

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_pcm_mmap_fd(struct snd_pcm_substream *substream,
			   struct snd_pcm_mmap_fd *mmap_fd)
{
	struct msm_audio *prtd;
	struct audio_port_data *apd;
	struct audio_buffer *ab;
	int dir = -1;
	struct dma_buf *buf = NULL;
	int rc = 0;

	if (!substream->runtime) {
		pr_err("%s substream runtime not found\n", __func__);
		return -EFAULT;
	}

	prtd = substream->runtime->private_data;
	if (!prtd || !prtd->audio_client || !prtd->mmap_flag) {
		pr_err("%s no audio client or not an mmap session\n", __func__);
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		dir = IN;
	else
		dir = OUT;

	apd = prtd->audio_client->port;
	ab = &(apd[dir].buf[0]);
	/*
	 * Passing O_CLOEXEC as flag passed to fd, to be in sync with
	 * previous implimentation.
	 * This was the flag used by previous internal wrapper API, which
	 * used to call dma_buf_fd internally.
	 */
	mmap_fd->fd = dma_buf_fd(ab->dma_buf, O_CLOEXEC);
	if (mmap_fd->fd < 0) {
		pr_err("%s: dma_buf_fd failed, fd:%d\n",
			__func__, mmap_fd->fd);
		rc = -EFAULT;
		goto buf_fd_fail;
	}
	mmap_fd->dir = dir;
	mmap_fd->actual_size = ab->actual_size;
	mmap_fd->size = ab->size;

	buf = dma_buf_get(mmap_fd->fd);
	if (IS_ERR_OR_NULL(buf)) {
		pr_err("%s: dma_buf_get failed, fd:%d\n",
			__func__, mmap_fd->fd);
		rc = -EINVAL;
	}

buf_fd_fail:
        return rc;
}
#endif /* CONFIG_AUDIO_QGKI */

static int msm_pcm_ioctl(struct snd_pcm_substream *substream,
			 unsigned int cmd, void *arg)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	int dir = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ? 0 : 1;
	struct audio_buffer *buf;

	switch (cmd) {
	case SNDRV_PCM_IOCTL1_RESET:
		pr_debug("%s: %s SNDRV_PCM_IOCTL1_RESET\n", __func__,
		       dir == 0 ? "P" : "C");
		buf = q6asm_shared_io_buf(prtd->audio_client, dir);

		if (buf && buf->data)
			memset(buf->data, 0, buf->actual_size);
		break;
	default:
		break;
	}

	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

#if IS_ENABLED(CONFIG_COMPAT) && IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_pcm_compat_ioctl(struct snd_pcm_substream *substream,
				unsigned int cmd, void *arg)
{
	/* we only handle RESET which is common for both modes */
	return msm_pcm_ioctl(substream, cmd, arg);
}
#endif

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
	 unsigned long hwoff, void __user *buf, unsigned long fbytes)
{
	return -EINVAL;
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
	int rc = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct msm_audio *prtd = runtime->private_data;
	struct asm_softvolume_params softvol = {
		.period = SOFT_VOLUME_PERIOD,
		.step = SOFT_VOLUME_STEP,
		.rampingcurve = SOFT_VOLUME_CURVE_LINEAR,
	};

	if (!prtd || !prtd->mmap_flag)
		return -EIO;

	if (prtd->audio_client) {
		rc = q6asm_set_softvolume_v2(prtd->audio_client,
						&softvol, SOFT_VOLUME_INSTANCE_1);
		if (rc < 0)
			pr_err("%s: Send SoftVolume command failed rc=%d\n",
					__func__, rc);
	}
	return rc;
}

static int msm_pcm_close(struct snd_pcm_substream *substream)
{
	struct msm_plat_data *pdata = NULL;
	struct snd_soc_component *component = NULL;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *soc_prtd = substream->private_data;
	struct msm_audio *prtd = runtime->private_data;
	struct audio_client *ac = prtd->audio_client;
	uint32_t timeout;
	int dir = 0;
	int ret = 0;

	if (!soc_prtd) {
		pr_debug("%s private_data not found\n",
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
	msm_pcm_routing_dereg_phy_stream(soc_prtd->dai_link->id,
					 dir == IN ?
					 SNDRV_PCM_STREAM_PLAYBACK :
					 SNDRV_PCM_STREAM_CAPTURE);
	kfree(prtd);
	runtime->private_data = NULL;
	mutex_unlock(&pdata->lock);

	return 0;
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
		pr_err("%s substream not found\n", __func__);
		return -ENODEV;
	}
	soc_prtd = substream->private_data;
	if (!substream->runtime || !soc_prtd) {
		pr_err("%s substream runtime or private_data not found\n",
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

static int msm_pcm_channel_map_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *pcm = snd_kcontrol_chip(kcontrol);
	u64 fe_id = kcontrol->private_value;
	struct msm_plat_data *pdata = (struct msm_plat_data *)
			snd_soc_component_get_drvdata(pcm);
	int rc = 0, i = 0;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		rc = -EINVAL;
		goto end;
	}

	if (pdata->ch_map[fe_id]) {
		pdata->ch_map[fe_id]->set_ch_map = true;
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
			pdata->ch_map[fe_id]->channel_map[i] =
				(char)(ucontrol->value.integer.value[i]);
	} else {
		pr_debug("%s: no memory for ch_map, default will be set\n",
			__func__);
	}
end:
	pr_debug("%s: ret %d\n", __func__, rc);
	return rc;
}

static int msm_pcm_channel_map_info(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 8;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 0xFFFFFFFF;
	return 0;
}

static int msm_pcm_channel_map_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *pcm = snd_kcontrol_chip(kcontrol);
	u64 fe_id = kcontrol->private_value;
	struct msm_plat_data *pdata = (struct msm_plat_data *)
			snd_soc_component_get_drvdata(pcm);
	int rc = 0, i = 0;

	pr_debug("%s: fe_id- %llu\n", __func__, fe_id);
	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s: Received out of bounds fe_id %llu\n",
			__func__, fe_id);
		rc = -EINVAL;
		goto end;
	}
	if (pdata->ch_map[fe_id]) {
		for (i = 0; i < PCM_FORMAT_MAX_NUM_CHANNEL; i++)
			ucontrol->value.integer.value[i] =
				pdata->ch_map[fe_id]->channel_map[i];
	}
end:
	pr_debug("%s: ret %d\n", __func__, rc);
	return rc;
}

static int msm_pcm_add_channel_map_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "Playback Channel Map";
	const char *deviceNo       = "NN";
	char *mixer_str = NULL;
	struct msm_plat_data *pdata = NULL;
	int ctl_len = 0;
	struct snd_kcontrol_new fe_channel_map_control[1] = {
		{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "?",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = msm_pcm_channel_map_info,
		.get = msm_pcm_channel_map_get,
		.put = msm_pcm_channel_map_put,
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

	pr_debug("%s: added new pcm FE with name %s, id %d, cpu dai %s, device no %d\n",
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
		 kzalloc(sizeof(struct msm_pcm_ch_map), GFP_KERNEL);
	if (!pdata->ch_map[rtd->dai_link->id]) {
		pr_err("%s: Could not allocate memory for channel map\n",
			__func__);
		kfree(mixer_str);
		return -ENOMEM;
	}
	kfree(mixer_str);
	return 0;
}

static int msm_pcm_fe_topology_info(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_info *uinfo)
{
	const struct soc_enum *e = &msm_pcm_fe_topology_enum[0];

	return snd_ctl_enum_info(uinfo, 1, e->items, e->texts);
}

static int msm_pcm_fe_topology_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	unsigned long fe_id = kcontrol->private_value;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bound fe_id %lu\n", __func__, fe_id);
		return -EINVAL;
	}

	pr_debug("%s: %lu topology %s\n", __func__, fe_id,
		 msm_pcm_fe_topology_text[msm_pcm_fe_topology[fe_id]]);
	ucontrol->value.enumerated.item[0] = msm_pcm_fe_topology[fe_id];
	return 0;
}

static int msm_pcm_fe_topology_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	unsigned long fe_id = kcontrol->private_value;
	unsigned int item;

	if (fe_id >= MSM_FRONTEND_DAI_MAX) {
		pr_err("%s Received out of bound fe_id %lu\n", __func__, fe_id);
		return -EINVAL;
	}

	item = ucontrol->value.enumerated.item[0];
	if (item >= ARRAY_SIZE(msm_pcm_fe_topology_text)) {
		pr_err("%s Received out of bound topology %lu\n", __func__,
		       fe_id);
		return -EINVAL;
	}

	pr_debug("%s: %lu new topology %s\n", __func__, fe_id,
		 msm_pcm_fe_topology_text[item]);
	msm_pcm_fe_topology[fe_id] = item;
	return 0;
}

static int msm_pcm_add_fe_topology_control(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_component *component = NULL;
	const char *mixer_ctl_name = "PCM_Dev";
	const char *deviceNo       = "NN";
	const char *topo_text      = "Topology";
	char *mixer_str = NULL;
	int ctl_len;
	int ret;
	struct snd_kcontrol_new topology_control[1] = {
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
			.name =  "?",
			.info =  msm_pcm_fe_topology_info,
			.get = msm_pcm_fe_topology_get,
			.put = msm_pcm_fe_topology_put,
			.private_value = 0,
		},
	};

	component = snd_soc_rtdcom_lookup(rtd, DRV_NAME);
	if (!component) {
		pr_err("%s: component is NULL\n", __func__);
		return -EINVAL;
	}

	ctl_len = strlen(mixer_ctl_name) + 1 + strlen(deviceNo) + 1 +
		  strlen(topo_text) + 1;
	mixer_str = kzalloc(ctl_len, GFP_KERNEL);

	if (!mixer_str)
		return -ENOMEM;

	snprintf(mixer_str, ctl_len, "%s %d %s", mixer_ctl_name,
		 rtd->pcm->device, topo_text);

	topology_control[0].name = mixer_str;
	topology_control[0].private_value = rtd->dai_link->id;
	ret = snd_soc_add_component_controls(component, topology_control,
					    ARRAY_SIZE(topology_control));
	msm_pcm_fe_topology[rtd->dai_link->id] = 0;
	kfree(mixer_str);
	return ret;
}

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_pcm_playback_app_type_cfg_ctl_put(struct snd_kcontrol *kcontrol,
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
	pr_debug("%s: fedai_id %llu, session_type %d, be_id %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
done:
	return ret;
}

static int msm_pcm_capture_app_type_cfg_ctl_put(struct snd_kcontrol *kcontrol,
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
	pr_debug("%s: fedai_id %llu, session_type %d, be_id %d, app_type %d, acdb_dev_id %d, sample_rate %d\n",
		__func__, fe_id, session_type, be_id,
		cfg_data.app_type, cfg_data.acdb_dev_id, cfg_data.sample_rate);
done:
	return ret;
}

static int msm_pcm_add_app_type_controls(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_pcm *pcm = rtd->pcm;
	struct snd_pcm_usr *app_type_info;
	struct snd_kcontrol *kctl;
	const char *playback_mixer_ctl_name = "Audio Stream";
	const char *capture_mixer_ctl_name = "Audio Stream Capture";
	const char *deviceNo = "NN";
	const char *suffix = "App Type Cfg";
	int ctl_len, ret = 0;

	if (pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream) {
		ctl_len = strlen(playback_mixer_ctl_name) + 1 +
				strlen(deviceNo) + 1 +
				strlen(suffix) + 1;
		pr_debug("%s: Playback app type cntrl add\n", __func__);
		ret = snd_pcm_add_usr_ctls(pcm, SNDRV_PCM_STREAM_PLAYBACK,
				NULL, 1, ctl_len, rtd->dai_link->id,
				&app_type_info);
		if (ret < 0) {
			pr_err("%s: playback app type cntrl add failed, err: %d\n",
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
			pr_err("%s: capture app type cntrl add failed, err: %d\n",
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

#if IS_ENABLED(CONFIG_AUDIO_QGKI)
static int msm_pcm_hwdep_ioctl(struct snd_hwdep *hw, struct file *file,
			       unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct snd_pcm *pcm = hw->private_data;
	struct snd_pcm_mmap_fd __user *_mmap_fd = NULL;
	struct snd_pcm_mmap_fd mmap_fd;
	struct snd_pcm_substream *substream = NULL;
	int32_t dir = -1;

	switch (cmd) {
	case SNDRV_PCM_IOCTL_MMAP_DATA_FD:
		_mmap_fd = (struct snd_pcm_mmap_fd __user *)arg;
		if (get_user(dir, (int32_t __user *)&(_mmap_fd->dir))) {
			pr_err("%s: error copying mmap_fd from user\n",
			       __func__);
			ret = -EFAULT;
			break;
		}
		if (dir != OUT && dir != IN) {
			pr_err("%s invalid stream dir\n", __func__);
			ret = -EINVAL;
			break;
		}
		substream = pcm->streams[dir].substream;
		if (!substream) {
			pr_err("%s substream not found\n", __func__);
			ret = -ENODEV;
			break;
		}
		pr_debug("%s : %s MMAP Data fd\n", __func__,
		       dir == 0 ? "P" : "C");
		if (msm_pcm_mmap_fd(substream, &mmap_fd) < 0) {
			pr_err("%s: error getting fd\n",
			       __func__);
			ret = -EFAULT;
			break;
		}
		if (put_user(mmap_fd.fd, &_mmap_fd->fd) ||
		    put_user(mmap_fd.size, &_mmap_fd->size) ||
		    put_user(mmap_fd.actual_size, &_mmap_fd->actual_size)) {
			pr_err("%s: error copying fd\n", __func__);
			return -EFAULT;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static int msm_pcm_hwdep_compat_ioctl(struct snd_hwdep *hw,
				      struct file *file,
				      unsigned int cmd,
				      unsigned long arg)
{
	/* we only support mmap fd. Handling is common in both modes */
	return msm_pcm_hwdep_ioctl(hw, file, cmd, arg);
}
#else
static int msm_pcm_hwdep_compat_ioctl(struct snd_hwdep *hw,
				      struct file *file,
				      unsigned int cmd,
				      unsigned long arg)
{
	return -EINVAL;
}
#endif

static int msm_pcm_add_hwdep_dev(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_hwdep *hwdep;
	int rc;
	char id[] = "NOIRQ_NN";

	snprintf(id, sizeof(id), "NOIRQ_%d", runtime->pcm->device);
	pr_debug("%s: pcm dev %d\n", __func__, runtime->pcm->device);
	rc = snd_hwdep_new(runtime->card->snd_card,
			   &id[0],
			   HWDEP_FE_BASE + runtime->pcm->device,
			   &hwdep);
	if (!hwdep || rc < 0) {
		pr_err("%s: hwdep intf failed to create %s - hwdep\n", __func__,
		       id);
		return rc;
	}

	hwdep->iface = SNDRV_HWDEP_IFACE_AUDIO_BE; /* for lack of a FE iface */
	hwdep->private_data = runtime->pcm; /* of type struct snd_pcm */
	hwdep->ops.ioctl = msm_pcm_hwdep_ioctl;
	hwdep->ops.ioctl_compat = msm_pcm_hwdep_compat_ioctl;
	return 0;
}
#else
static int msm_pcm_add_hwdep_dev(struct snd_soc_pcm_runtime *runtime)
{
	return 0;
}
#endif /* CONFIG_AUDIO_GKI */

static int msm_asoc_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;
	int ret;

	pr_debug("%s , register new control\n", __func__);
	if (!card->dev->coherent_dma_mask)
		card->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	ret = msm_pcm_add_channel_map_control(rtd);
	if (ret)
		pr_err("%s: Could not add pcm Channel Map Control\n",
			__func__);

	ret = msm_pcm_add_volume_control(rtd, SNDRV_PCM_STREAM_PLAYBACK);
	if (ret) {
		pr_err("%s: Could not add pcm playback volume Control %d\n",
			__func__, ret);
	}
	ret = msm_pcm_add_volume_control(rtd, SNDRV_PCM_STREAM_CAPTURE);
	if (ret) {
		pr_err("%s: Could not add pcm capture volume Control %d\n",
			__func__, ret);
	}
	ret = msm_pcm_add_fe_topology_control(rtd);
	if (ret) {
		pr_err("%s: Could not add pcm topology control %d\n",
			__func__, ret);
	}

	ret = msm_pcm_add_app_type_controls(rtd);
	if (ret) {
		pr_err("%s: Could not add app type controls failed %d\n",
			__func__, ret);
	}
	ret = msm_pcm_add_hwdep_dev(rtd);
	if (ret)
		pr_err("%s: Could not add hw dep node\n", __func__);
	pcm->nonatomic = true;

	return ret;
}


static const struct snd_pcm_ops msm_pcm_ops = {
	.open           = msm_pcm_open,
	.prepare        = msm_pcm_prepare,
	.copy_user      = msm_pcm_copy,
	.hw_params	= msm_pcm_hw_params,
	.ioctl          = msm_pcm_ioctl,
#if IS_ENABLED(CONFIG_COMPAT) && IS_ENABLED(CONFIG_AUDIO_QGKI)
	.compat_ioctl   = msm_pcm_compat_ioctl,
#endif
	.trigger        = msm_pcm_trigger,
	.pointer        = msm_pcm_pointer,
	.mmap           = msm_pcm_mmap,
	.close          = msm_pcm_close,
};

static struct snd_soc_component_driver msm_soc_component = {
	.name		= DRV_NAME,
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
		if (!rc) {
			if (!strcmp(latency_level, "ultra"))
				perf_mode = ULTRA_LOW_LATENCY_PCM_MODE;
			else if (!strcmp(latency_level, "ull-pp"))
				perf_mode = ULL_POST_PROCESSING_PCM_MODE;
		}
	}

	pdata = devm_kzalloc(&pdev->dev,
			     sizeof(struct msm_plat_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->perf_mode = perf_mode;

	mutex_init(&pdata->lock);

	dev_set_drvdata(&pdev->dev, pdata);

	dev_dbg(&pdev->dev, "%s: dev name %s\n",
				__func__, dev_name(&pdev->dev));
	dev_dbg(&pdev->dev, "Pull mode driver register\n");
	rc = snd_soc_register_component(&pdev->dev,
				&msm_soc_component,
				NULL, 0);

	if (rc)
		dev_err(&pdev->dev, "Failed to register pull mode driver\n");

	return rc;
}

static int msm_pcm_remove(struct platform_device *pdev)
{
	struct msm_plat_data *pdata;

	dev_dbg(&pdev->dev, "Pull mode remove\n");
	pdata = dev_get_drvdata(&pdev->dev);
	mutex_destroy(&pdata->lock);
	devm_kfree(&pdev->dev, pdata);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}
static const struct of_device_id msm_pcm_noirq_dt_match[] = {
	{.compatible = "qcom,msm-pcm-dsp-noirq"},
	{}
};
MODULE_DEVICE_TABLE(of, msm_pcm_noirq_dt_match);

static struct platform_driver msm_pcm_driver_noirq = {
	.driver = {
		.name = "msm-pcm-dsp-noirq",
		.owner = THIS_MODULE,
		.of_match_table = msm_pcm_noirq_dt_match,
		.suppress_bind_attrs = true,
	},
	.probe = msm_pcm_probe,
	.remove = msm_pcm_remove,
};

int __init msm_pcm_noirq_init(void)
{
	return platform_driver_register(&msm_pcm_driver_noirq);
}

void msm_pcm_noirq_exit(void)
{
	platform_driver_unregister(&msm_pcm_driver_noirq);
}

MODULE_DESCRIPTION("PCM NOIRQ module platform driver");
MODULE_LICENSE("GPL v2");
