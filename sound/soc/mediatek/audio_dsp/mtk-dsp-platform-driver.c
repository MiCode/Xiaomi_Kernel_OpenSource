// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/string.h>
#include <sound/soc.h>
#include <audio_task_manager.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/mutex.h>

#include <adsp_helper.h>
#include <audio_ipi_platform.h>
#include <audio_messenger_ipi.h>

#include "mtk-dsp-mem-control.h"
#include "mtk-base-dsp.h"
#include "mtk-dsp-common.h"
#include "mtk-dsp-platform-driver.h"
#include "mtk-base-afe.h"

static DEFINE_MUTEX(adsp_wakelock_lock);

#define IPIMSG_SHARE_MEM (1024)
#define DSP_IRQ_LOOP_COUNT (3)
static int adsp_wakelock_count;
static struct wakeup_source *adsp_audio_wakelock;
static int ktv_status;


//#define DEBUG_VERBOSE
//#define DEBUG_VERBOSE_IRQ

static inline unsigned long clr_bit(int bit, unsigned long *addr)
{
	return (*addr & ~(1UL << bit));
}

static int dsp_task_attr_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];
	int id;
	int dsp_task_id = -1;
	int attr_id = -1;
	const char *name = NULL;

	/* get task attribute id */
	if (strstr(kcontrol->id.name, "ref_runtime"))
		attr_id = ADSP_TASK_ATTR_REF_RUNTIME;
	else if (strstr(kcontrol->id.name, "runtime"))
		attr_id = ADSP_TASK_ATTR_RUNTIME;
	else if (strstr(kcontrol->id.name, "default"))
		attr_id = ADSP_TASK_ATTR_DEFAULT;
	else {
		pr_info("%s(), attr_id not support: %s\n",
			__func__, kcontrol->id.name);
		return -1;
	}

	/* get dsp task id */
	for (id = 0; id < AUDIO_TASK_DAI_NUM; id++) {
		name = get_str_by_dsp_dai_id(id);
		if (!name)
			continue;

		if (strstr(kcontrol->id.name, name)) {
			dsp_task_id = id;
			break;
		}
	}

	if (dsp_task_id < 0) {
		pr_info("%s(), %s dsp_task_id not support\n",
			kcontrol->id.name, __func__);
		return -1;
	}

	set_task_attr(dsp_task_id, attr_id, val);

	return 0;
}

static int dsp_task_attr_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	int id;
	int dsp_task_id = -1;
	int attr_id;
	const char *name = NULL;

	/* get task attribute id */
	if (strstr(kcontrol->id.name, "ref_runtime"))
		attr_id = ADSP_TASK_ATTR_REF_RUNTIME;
	else if (strstr(kcontrol->id.name, "runtime"))
		attr_id = ADSP_TASK_ATTR_RUNTIME;
	else if (strstr(kcontrol->id.name, "default"))
		attr_id = ADSP_TASK_ATTR_DEFAULT;
	else {
		pr_info("%s(), attr_id not support: %s\n",
			__func__, kcontrol->id.name);
		return -1;
	}

	/* get dsp task id */
	for (id = 0; id < AUDIO_TASK_DAI_NUM; id++) {
		name = get_str_by_dsp_dai_id(id);
		if (!name)
			continue;

		if (strstr(kcontrol->id.name, name)) {
			dsp_task_id = id;
			break;
		}
	}

	if (dsp_task_id < 0) {
		pr_info("%s(), %s dsp_task_id not support\n",
			kcontrol->id.name, __func__);
		return -1;
	}

	ucontrol->value.integer.value[0] = get_task_attr(dsp_task_id, attr_id);

	return 0;
}

static int dsp_wakelock_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	int val = ucontrol->value.integer.value[0];

	mutex_lock(&adsp_wakelock_lock);
	if (val) {
		adsp_wakelock_count++;
		if (adsp_wakelock_count == 1)
			aud_wake_lock(adsp_audio_wakelock);
	} else {
		adsp_wakelock_count--;
		if (adsp_wakelock_count == 0)
			aud_wake_unlock(adsp_audio_wakelock);
		if (adsp_wakelock_count < 0) {
			pr_info("%s not paired", __func__);
			adsp_wakelock_count = 0;
		}
	}
	mutex_unlock(&adsp_wakelock_lock);
	return 0;
}

static int dsp_wakelock_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = adsp_wakelock_count;
	return 0;
}

static int audio_dsp_version_set(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(cmpnt);

	if (!dsp)
		return -1;

	dsp->dsp_ver = ucontrol->value.integer.value[0];
	return 0;
}

static int audio_dsp_version_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(cmpnt);

	if (!dsp)
		return -1;

	ucontrol->value.integer.value[0] = dsp->dsp_ver;
	return 0;
}

static int smartpa_swdsp_process_enable_set(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	unsigned int val = ucontrol->value.integer.value[0];

	set_task_attr(AUDIO_TASK_PLAYBACK_ID, ADSP_TASK_ATTR_SMARTPA, val);
	return 0;
}

static int smartpa_swdsp_process_enable_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] =
		get_task_attr(AUDIO_TASK_PLAYBACK_ID,
			      ADSP_TASK_ATTR_SMARTPA);

	return 0;
}

static int ktv_status_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ktv_status = ucontrol->value.integer.value[0];
	pr_debug("%s() ktv_status = %d\n", __func__, ktv_status);
	return 0;
}

static int ktv_status_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = ktv_status;
	pr_debug("%s() ktv_status = %ld\n", __func__, ktv_status);
	return 0;
}

static const struct snd_kcontrol_new dsp_platform_kcontrols[] = {
	SOC_SINGLE_EXT("dsp_primary_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_deepbuf_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_voipdl_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_playback_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_captureul1_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_offload_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_a2dp_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_bledl_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_bleul_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_dataprovider_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_call_final_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_fast_default_en", SND_SOC_NOPM, 0, 0xff, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_ktv_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_captureraw_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_fm_default_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_primary_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_deepbuf_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_voipdl_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_playback_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_music_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_captureul1_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_offload_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_a2dp_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_bledl_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_bleul_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_dataprovider_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_fast_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_ktv_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_captureraw_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_fm_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_call_final_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_captureul1_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_playback_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("dsp_call_final_ref_runtime_en", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_task_attr_get, dsp_task_attr_set),
	SOC_SINGLE_EXT("audio_dsp_version", SND_SOC_NOPM, 0, 0xff, 0,
		       audio_dsp_version_get, audio_dsp_version_set),
	SOC_SINGLE_EXT("swdsp_smartpa_process_enable", SND_SOC_NOPM, 0, 0xff, 0,
		       smartpa_swdsp_process_enable_get,
		       smartpa_swdsp_process_enable_set),
	SOC_SINGLE_EXT("ktv_status", SND_SOC_NOPM, 0, 0x1, 0,
		       ktv_status_get, ktv_status_set),
	SOC_SINGLE_EXT("audio_dsp_wakelock", SND_SOC_NOPM, 0, 0x1, 0,
		       dsp_wakelock_get, dsp_wakelock_set),
};

static snd_pcm_uframes_t mtk_dsphw_pcm_pointer_ul
			 (struct snd_pcm_substream *substream)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct snd_soc_component *component =
		snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	int ptr_bytes;

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
#endif

	ptr_bytes = dsp_mem->ring_buf.pWrite - dsp_mem->ring_buf.pBufBase;

	return bytes_to_frames(substream->runtime, ptr_bytes);
}

static unsigned int dsp_word_size_align(unsigned int in_size)
{
	unsigned int align_size;

	align_size = in_size & 0xFFFFFF80;
	return align_size;
}

static int afe_remap_dsp_pointer
		(struct buf_attr dst, struct buf_attr src, int bytes)
{
	int retval = bytes;

	retval = (retval * snd_pcm_format_physical_width(src.format))
		  / snd_pcm_format_physical_width(dst.format);
	retval = (retval * src.rate) / dst.rate;
	retval = (retval * src.channel) / dst.channel;
	return retval;
}

static snd_pcm_uframes_t mtk_dsphw_pcm_pointer_dl
			 (struct snd_pcm_substream *substream)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct snd_soc_component *component =
			snd_soc_rtdcom_lookup(rtd, AFE_DSP_NAME);
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	struct mtk_base_afe *afe;
	struct mtk_base_afe_memif *memif;
	const struct mtk_base_memif_data *memif_data;
	struct regmap *regmap;
	struct device *dev;
	int reg_ofs_base;
	int reg_ofs_cur;
	unsigned int hw_ptr = 0, hw_base = 0;
	int ret, pcm_ptr_bytes, pcm_remap_ptr_bytes;
	spinlock_t *ringbuf_lock = &dsp_mem->ringbuf_lock;
	unsigned long flags = 0;

	if (dsp->dsp_ver)
		goto SYNC_READINDEX;

	afe = get_afe_base();
	if (!afe) {
		pr_info("%s afe = %p\n", __func__, afe);
		AUD_ASSERT(0);
		return 0;
	}

	memif = &afe->memif[get_afememdl_by_afe_taskid(id)];
	if (!memif) {
		pr_info("%s memif = %p\n", __func__, memif);
		AUD_ASSERT(0);
		return 0;
	}

	memif_data = memif->data;
	if (!memif_data) {
		pr_info("%s memif_data = %p\n", __func__, memif_data);
		AUD_ASSERT(0);
		return 0;
	}

	regmap = afe->regmap;
	if (!regmap) {
		pr_info("%s regmap = %p\n", __func__, regmap);
		AUD_ASSERT(0);
		return 0;
	}

	dev = afe->dev;
	if (!dev) {
		pr_info("%s dev = %p\n", __func__, dev);
		AUD_ASSERT(0);
		return 0;
	}

	reg_ofs_base = memif_data->reg_ofs_base;
	reg_ofs_cur = memif_data->reg_ofs_cur;

	ret = regmap_read(regmap, reg_ofs_cur, &hw_ptr);
	if (ret || hw_ptr == 0) {
		dev_err(dev, "1 %s hw_ptr err\n", __func__);
		pcm_ptr_bytes = 0;
		pcm_remap_ptr_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}

	ret = regmap_read(regmap, reg_ofs_base, &hw_base);
	if (ret || hw_base == 0) {
		dev_err(dev, "2 %s hw_ptr err\n", __func__);
		pcm_ptr_bytes = 0;
		pcm_remap_ptr_bytes = 0;
		goto POINTER_RETURN_FRAMES;
	}

	pcm_ptr_bytes = hw_ptr - hw_base;
	pcm_remap_ptr_bytes =
			afe_remap_dsp_pointer(
			dsp_mem->audio_afepcm_buf.aud_buffer.buffer_attr,
			dsp_mem->adsp_buf.aud_buffer.buffer_attr,
			pcm_ptr_bytes);
	pcm_remap_ptr_bytes = dsp_word_size_align(pcm_remap_ptr_bytes);
	if (pcm_remap_ptr_bytes >=
	    dsp_mem->adsp_buf.aud_buffer.buf_bridge.bufLen)
		pr_info("%s pcm_remap_ptr_bytes = %d",
			__func__,
			pcm_remap_ptr_bytes);
	else
		dsp_mem->adsp_buf.aud_buffer.buf_bridge.pRead =
			(dsp_mem->adsp_buf.aud_buffer.buf_bridge.pBufBase +
			 pcm_remap_ptr_bytes);

	spin_lock_irqsave(ringbuf_lock, flags);

	ret = sync_ringbuf_readidx(
		&dsp_mem->ring_buf,
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	spin_unlock_irqrestore(ringbuf_lock, flags);

	if (ret) {
		pr_info("%s sync_ringbuf_readidx underflow\n", __func__);
		return -1;
	}

#ifdef DEBUG_VERBOSE
	pr_info("%s id = %d reg_ofs_base = %d reg_ofs_cur = %d pcm_ptr_bytes = %d pcm_remap_ptr_bytes = %d\n",
		__func__, id, reg_ofs_base, reg_ofs_cur,
		pcm_ptr_bytes, pcm_remap_ptr_bytes);
#endif

POINTER_RETURN_FRAMES:
	return bytes_to_frames(substream->runtime, pcm_remap_ptr_bytes);

SYNC_READINDEX:

#ifdef DEBUG_VERBOSE
	dump_rbuf_s("-mtk_dsphw_pcm_pointer_dl", &dsp_mem->ring_buf);
#endif

	/* handle for underflow */
	if (dsp_mem->underflowed)
		return -1;

	spin_lock_irqsave(ringbuf_lock, flags);
	pcm_ptr_bytes = (int)(dsp_mem->ring_buf.pRead -
			      dsp_mem->ring_buf.pBufBase);
	spin_unlock_irqrestore(ringbuf_lock, flags);
	pcm_remap_ptr_bytes =
		bytes_to_frames(substream->runtime, pcm_ptr_bytes);

	return pcm_remap_ptr_bytes;
}

static snd_pcm_uframes_t mtk_dsphw_pcm_pointer
			 (struct snd_soc_component *component,
			  struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return mtk_dsphw_pcm_pointer_dl(substream);
	else
		return  mtk_dsphw_pcm_pointer_ul(substream);
}

static void mtk_dsp_dl_handler(struct mtk_base_dsp *dsp,
			       struct ipi_msg_t *ipi_msg, int id)
{
	if (dsp->dsp_mem[id].substream == NULL) {
		pr_info("%s = substream == NULL\n", __func__);
		goto DSP_IRQ_HANDLER_ERR;
	}

	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		goto DSP_IRQ_HANDLER_ERR;
	}

	/* notify subsream */
	snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
DSP_IRQ_HANDLER_ERR:
	return;
}

static bool mtk_dsp_check_exception(struct mtk_base_dsp *dsp,
				    struct ipi_msg_t *ipi_msg, int id)
{
	const char *task_name = get_str_by_dsp_dai_id(id);

	if (id < 0 || id >= AUDIO_TASK_DAI_NUM)
		return false;

	if (!dsp->dsp_mem[id].substream) {
		pr_info_ratelimited("%s() %s substream NULL\n",
				    __func__, task_name);
		return false;
	}

	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info_ratelimited("%s() %s state[%d]\n",
			__func__, task_name,
			dsp->dsp_mem[id].substream->runtime->status->state);
		return false;
	}

	/* adsp reset message */
	if (ipi_msg && ipi_msg->param2 == ADSP_DL_CONSUME_RESET) {
		pr_info("%s() %s adsp reset\n", __func__, task_name);
		RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);
		snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
		return true;
	}

	/* adsp underflow message */
	if (ipi_msg && ipi_msg->param2 == ADSP_DL_CONSUME_UNDERFLOW) {
		pr_info("%s() %s adsp underflow\n", __func__, task_name);
		dsp->dsp_mem[id].underflowed = true;

		snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);

		return true;
	}

	return false;
}

static void mtk_dsp_dl_consume_handler(struct mtk_base_dsp *dsp,
				       struct ipi_msg_t *ipi_msg, int id)
{
	void *ipi_audio_buf;
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	spinlock_t *ringbuf_lock = &dsp->dsp_mem[id].ringbuf_lock;
	struct snd_pcm_substream *substream;
	unsigned long flags = 0;

	if (!dsp->dsp_mem[id].substream) {
		return;
	}

	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		return;
	}
	substream = dsp->dsp_mem[id].substream;


	// handle for no restart pcm, copy audio_hw_buffer from msg payload, others from share mem
	if ((substream->runtime->stop_threshold > substream->runtime->start_threshold) && ipi_msg) {
		memcpy((void *)&dsp_mem->adsp_work_buf, ipi_msg->payload,
		       sizeof(struct audio_hw_buffer));
	} else {
		ipi_audio_buf = (void *)dsp_mem->msg_dtoa_share_buf.va_addr;
		memcpy((void *)&dsp_mem->adsp_work_buf, (void *)ipi_audio_buf,
		       sizeof(struct audio_hw_buffer));
	}

	spin_lock_irqsave(ringbuf_lock, flags);
	dsp->dsp_mem[id].adsp_buf.aud_buffer.buf_bridge.pRead =
		dsp->dsp_mem[id].adsp_work_buf.aud_buffer.buf_bridge.pRead;
#ifdef DEBUG_VERBOSE_IRQ
	dump_rbuf_s("dl_consume before sync", &dsp->dsp_mem[id].ring_buf);
	dump_rbuf_bridge_s("dl_consume before sync",
			   &dsp->dsp_mem[id].adsp_buf.aud_buffer.buf_bridge);
#endif
	sync_ringbuf_readidx(
		&dsp->dsp_mem[id].ring_buf,
		&dsp->dsp_mem[id].adsp_buf.aud_buffer.buf_bridge);

	spin_unlock_irqrestore(ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE_IRQ
	dump_rbuf_s("dl_consume after sync", &dsp->dsp_mem[id].ring_buf);
#endif
	/* notify subsream */
	snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
}

static void mtk_dsp_ul_handler(struct mtk_base_dsp *dsp,
			       struct ipi_msg_t *ipi_msg, int id)
{
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	void *ipi_audio_buf;
	unsigned long flags;
	spinlock_t *ringbuf_lock = &dsp->dsp_mem[id].ringbuf_lock;

	if (!dsp->dsp_mem[id].substream) {
		pr_info("%s substream NULL\n", __func__);
		return;
	}


	if (!snd_pcm_running(dsp->dsp_mem[id].substream)) {
		pr_info("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		goto DSP_IRQ_HANDLER_ERR;
	}

	if (dsp->dsp_mem[id].substream->runtime->status->state
	    != SNDRV_PCM_STATE_RUNNING) {
		pr_info("%s = state[%d]\n", __func__,
			 dsp->dsp_mem[id].substream->runtime->status->state);
		goto DSP_IRQ_HANDLER_ERR;
	}

	if (ipi_msg && ipi_msg->param2 == ADSP_UL_READ_RESET) {
		spin_lock_irqsave(ringbuf_lock, flags);
		RingBuf_Reset(&dsp->dsp_mem[id].ring_buf);
		/* set buf size full to trigger pcm_read */
		dsp->dsp_mem[id].ring_buf.datacount = dsp->dsp_mem[id].ring_buf.bufLen;
		spin_unlock_irqrestore(ringbuf_lock, flags);
		pr_info("%s reset UL\n", __func__);
		snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
		goto DSP_IRQ_HANDLER_ERR;
	}

	/* upadte for write index*/
	ipi_audio_buf = (void *)dsp_mem->msg_dtoa_share_buf.va_addr;

	memcpy((void *)&dsp_mem->adsp_work_buf, (void *)ipi_audio_buf,
	       sizeof(struct audio_hw_buffer));

	dsp_mem->adsp_buf.aud_buffer.buf_bridge.pWrite =
		(dsp_mem->adsp_work_buf.aud_buffer.buf_bridge.pWrite);
#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_work_buf.aud_buffer.buf_bridge);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	spin_lock_irqsave(ringbuf_lock, flags);
	sync_ringbuf_writeidx(&dsp_mem->ring_buf,
			      &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	spin_unlock_irqrestore(ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
#endif

	/* notify subsream */
	snd_pcm_period_elapsed(dsp->dsp_mem[id].substream);
DSP_IRQ_HANDLER_ERR:
	return;
}


void mtk_dsp_handler(struct mtk_base_dsp *dsp,
		     struct ipi_msg_t *ipi_msg)

{
	int id = 0;

	if (!dsp) {
		pr_info("%s dsp NULL", __func__);
		return;
	}

	if (ipi_msg == NULL) {
		pr_info("%s ipi_msg == NULL\n", __func__);
		return;
	}

	id = get_dspdaiid_by_dspscene(ipi_msg->task_scene);
	if (id < 0)
		return;

	if (!is_audio_task_dsp_ready(ipi_msg->task_scene)) {
		pr_info("%s(), is_adsp_ready send false\n", __func__);
		return;
	}

	switch (ipi_msg->msg_id) {
	case AUDIO_DSP_TASK_IRQDL:
		mtk_dsp_dl_handler(dsp, ipi_msg, id);
		break;
	case AUDIO_DSP_TASK_IRQUL:
		mtk_dsp_ul_handler(dsp, ipi_msg, id);
		break;
	case AUDIO_DSP_TASK_DL_CONSUME_DATA:
		/* check exceptions in consume message */
		if (mtk_dsp_check_exception(dsp, ipi_msg, id))
			break;

		/* Handle consume message for the platforms
		 * which not support audio IRQ.
		 */
		mtk_dsp_dl_consume_handler(dsp, ipi_msg, id);
		break;
	default:
		break;
	}
}

static int mtk_dsp_pcm_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	int retry_flag = false;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);

	int id = cpu_dai->id;
	int dsp_feature_id = get_featureid_by_dsp_daiid(id);
	const char *task_name = get_str_by_dsp_dai_id(id);

	memcpy((void *)(&(runtime->hw)), (void *)dsp->mtk_dsp_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = mtk_dsp_register_feature(dsp_feature_id);
	if (ret) {
		pr_info("%s(), %s register feature fail", __func__, task_name);
		return -1;
	}

RETRY:
	/* send to task with open information */
	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_OPEN,
			       0, 0, NULL);
	if (ret < 0) {
		if (!retry_flag) {
			wait_dsp_ready();
			retry_flag = true;
			goto RETRY;
		} else {
			mtk_dsp_deregister_feature(dsp_feature_id);
			goto END;
		}
	}

	/* set the wait_for_avail to 2 sec*/
	substream->wait_time = msecs_to_jiffies(2 * 1000);

	dsp->dsp_mem[id].substream = substream;
	dsp->dsp_mem[id].underflowed = 0;

END:
	pr_info("%s(), %s(%d) ret[%d] retry[%d]\n", __func__,
		task_name, id, ret, retry_flag);
	return ret;
}

static int mtk_dsp_pcm_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	int dsp_feature_id = get_featureid_by_dsp_daiid(id);
	const char *task_name = get_str_by_dsp_dai_id(id);

	pr_info("%s() %s\n", __func__, task_name);

	/* send to task with close information */
	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_CLOSE, 0,
			       0, NULL);
	if (ret)
		pr_info("%s ret[%d]\n", __func__, ret);

	mtk_dsp_deregister_feature(dsp_feature_id);

	dsp->dsp_mem[id].substream = NULL;

	return ret;
}

/*
 * allocate dsp copy
 * @param: @action true: allocate buffer false: free buffer
 */

static int mtk_dsp_manage_copybuf(bool action,
				  struct mtk_base_dsp_mem *dsp_mep,
				  unsigned int size)
{
	if (dsp_mep == NULL)
		return -ENOMEM;

	if (action == true) {
		if (size == 0)
			return -EINVAL;
		if (dsp_mep->dsp_copy_buf != NULL)
			return -EINVAL;
		dsp_mep->dsp_copy_buf = vmalloc(size);
		if (!dsp_mep->dsp_copy_buf)
			return -ENOMEM;
	} else {
		if (dsp_mep->dsp_copy_buf == NULL)
			return -EINVAL;
		vfree(dsp_mep->dsp_copy_buf);
		dsp_mep->dsp_copy_buf = NULL;
	}
	return 0;
}

static int mtk_dsp_pcm_hw_params(struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	int ret = 0;

	pr_info("%s(), %s task_id: %d\n", __func__, cpu_dai->name, id);

	reset_audiobuffer_hw(&dsp_mem->adsp_buf);
	reset_audiobuffer_hw(&dsp_mem->audio_afepcm_buf);
	reset_audiobuffer_hw(&dsp_mem->adsp_work_buf);
	RingBuf_Reset(&dsp_mem->ring_buf);

	dsp->request_dram_resource(dsp->dev);

	/* gen pool related */
	dsp_mem->gen_pool_buffer = mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	if (!dsp_mem->gen_pool_buffer)
		goto error;

	/* if already allocate , free it.*/
	if (substream->dma_buffer.area) {
		mtk_adsp_genpool_free_sharemem_ring(dsp_mem, id);
		release_snd_dmabuffer(&substream->dma_buffer);
	}

	/* allocate ring buffer with share memory */
	ret = mtk_adsp_genpool_allocate_sharemem_ring(dsp_mem, params_buffer_bytes(params), id);
	if (ret < 0) {
		pr_warn("%s err\n", __func__);
		return ret;
	}

	/* allocate copy buffer */
	ret = mtk_dsp_manage_copybuf(true, &dsp->dsp_mem[id], params_buffer_bytes(params));
	if (ret) {
		pr_info("%s ret = %d\n", __func__, ret);
		return -1;
	}

#ifdef DEBUG_VERBOSE
	dump_audio_dsp_dram(&dsp_mem->msg_atod_share_buf);
	dump_audio_dsp_dram(&dsp_mem->msg_dtoa_share_buf);
	dump_audio_dsp_dram(&dsp_mem->dsp_ring_share_buf);
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
#endif
	ret = dsp_dram_to_snd_dmabuffer(&dsp_mem->dsp_ring_share_buf, &substream->dma_buffer);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_hw(&dsp_mem->adsp_buf, BUFFER_TYPE_SHARE_MEM);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_memorytype(&dsp_mem->adsp_buf, MEMORY_AUDIO_DRAM);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_attribute(&dsp_mem->adsp_buf, substream, params,
					afe_get_pcmdir(substream->stream, dsp_mem->adsp_buf));
	if (ret < 0)
		goto error;

	memcpy(&dsp_mem->adsp_work_buf, &dsp_mem->adsp_buf, sizeof(struct audio_hw_buffer));

	/* send to task with hw_param information , buffer and pcm attribute */
	memcpy(dsp_mem->msg_atod_share_buf.vir_addr,
	       &dsp_mem->adsp_buf, sizeof(struct audio_hw_buffer));

	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_HWPARAM,
			 sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			 0,
			 (char *)&dsp_mem->msg_atod_share_buf.phy_addr);
	return ret;
error:
	pr_err("%s err\n", __func__);
	return -1;
}

static int mtk_dsp_pcm_hw_free(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];

	/* send to task with free status */
	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_HWFREE, 1, 0, NULL);
	if (ret)
		pr_info("%s ret[%d]\n", __func__, ret);

	mtk_adsp_genpool_free_sharemem_ring(dsp_mem, id);
	release_snd_dmabuffer(&substream->dma_buffer);

	/* free copy buffer */
	ret = mtk_dsp_manage_copybuf(false, &dsp->dsp_mem[id], 0);
	if (ret)
		pr_info("%s ret = %d\n", __func__, ret);

	/* release dsp memory */
	reset_audiobuffer_hw(&dsp_mem->adsp_buf);

	dsp->release_dram_resource(dsp->dev);

	return 0;
}

static int mtk_dsp_pcm_hw_prepare(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	struct audio_hw_buffer *adsp_buf = &dsp_mem->adsp_buf;
	const char *task_name = get_str_by_dsp_dai_id(id);

	/* The data type of stop_threshold in userspace is unsigned int.
	 * However its data type in kernel space is unsigned long.
	 * It needs to convert to ULONG_MAX in kernel space
	 */
	if (substream->runtime->stop_threshold == ~(0U))
		substream->runtime->stop_threshold = ULONG_MAX;

	clear_audiobuffer_hw(adsp_buf);
	RingBuf_Reset(&dsp_mem->ring_buf);
	RingBuf_Bridge_Reset(&adsp_buf->aud_buffer.buf_bridge);
	RingBuf_Bridge_Reset(&dsp_mem->adsp_work_buf.aud_buffer.buf_bridge);

	ret = set_audiobuffer_threshold(adsp_buf, substream);
	if (ret < 0)
		pr_warn("%s set_audiobuffer_attribute err\n", __func__);

	pr_info("%s(), %s start_threshold: %u stop_threshold: %u period_size: %d period_count: %d\n",
		__func__, task_name,
		adsp_buf->aud_buffer.start_threshold,
		adsp_buf->aud_buffer.stop_threshold,
		adsp_buf->aud_buffer.period_size,
		adsp_buf->aud_buffer.period_count);

	/* send audio_hw_buffer to SCP side */
	memcpy(dsp_mem->msg_atod_share_buf.vir_addr, adsp_buf, sizeof(struct audio_hw_buffer));

	/* send to task with prepare status */
	mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			 AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_PREPARE,
			 sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			 0,
			 (char *)&dsp_mem->msg_atod_share_buf.phy_addr);
	return ret;
}

static int mtk_dsp_start(struct snd_pcm_substream *substream,
			 struct mtk_base_dsp *dsp)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	const char *task_name = get_str_by_dsp_dai_id(id);

	dev_info(dsp->dev, "%s() %s %s\n",
		 __func__, task_name,
		 dsp_mem->underflowed ? "just underflow" : "");

	dsp_mem->underflowed = 0;

	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_DIRECT_SEND, AUDIO_DSP_TASK_START,
			       1, 0, NULL);
	return ret;
}

static int mtk_dsp_stop(struct snd_pcm_substream *substream,
			struct mtk_base_dsp *dsp)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;

	/* Avoid print log in alsa stop. If underflow happens,
	 * log will be printed in ISR.
	 */

	ret = mtk_scp_ipi_send(get_dspscene_by_dspdaiid(id), AUDIO_IPI_MSG_ONLY,
			       AUDIO_IPI_MSG_DIRECT_SEND, AUDIO_DSP_TASK_STOP,
			       1, 0, NULL);

	return ret;
}

static int mtk_dsp_pcm_hw_trigger(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int cmd)
{
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_dsp_start(substream, dsp);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_dsp_stop(substream, dsp);
	}
	return -EINVAL;
}

static int mtk_dsp_pcm_copy_dl(struct snd_pcm_substream *substream,
			       int copy_size,
			       struct mtk_base_dsp_mem *dsp_mem,
			       void __user *buf)
{
	int ret = 0, availsize = 0, ack_type;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct RingBuf *ringbuf = &dsp_mem->ring_buf;
	struct ringbuf_bridge *buf_bridge =
		&(dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	unsigned long flags = 0;
	void *dsp_copy_buf = dsp_mem->dsp_copy_buf;
	spinlock_t *ringbuf_lock = &dsp_mem->ringbuf_lock;
	const char *task_name = get_str_by_dsp_dai_id(id);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif

	if (substream->runtime->status->state == SNDRV_PCM_STATE_XRUN) {
		pr_info_ratelimited("%s() %s state[%d]\n",
				    __func__, task_name,
				    substream->runtime->status->state);
		return -1;
	}

	if (dsp_copy_buf == NULL)
		return -ENOMEM;

	Ringbuf_Check(ringbuf);
	Ringbuf_Bridge_Check(
		&dsp_mem->adsp_buf.aud_buffer.buf_bridge);

	/* copy user space memory */
	ret = copy_from_user(dsp_copy_buf, buf, copy_size);
	if (ret) {
		pr_info("%s copy_from_user fail line %d\n", __func__, __LINE__);
		return -1;
	}

	spin_lock_irqsave(ringbuf_lock, flags);
	availsize = RingBuf_getFreeSpace(ringbuf);
	if (availsize < copy_size) {
		pr_info("%s, id = %d, fail copy_size = %d availsize = %d\n",
			__func__, id, copy_size, RingBuf_getFreeSpace(ringbuf));
		dump_rbuf_s("check dlcopy", &dsp_mem->ring_buf);
		dump_rbuf_bridge_s("check dlcopy",
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
		spin_unlock_irqrestore(ringbuf_lock, flags);
		return -1;
	}

	RingBuf_copyFromLinear(ringbuf, dsp_copy_buf, copy_size);
	RingBuf_Bridge_update_writeptr(buf_bridge, copy_size);
	spin_unlock_irqrestore(ringbuf_lock, flags);

	/* send audio_hw_buffer to SCP side*/
	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
	       sizeof(struct audio_hw_buffer));

	if (substream->runtime->status->state != SNDRV_PCM_STATE_RUNNING)
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
	else
		ack_type = AUDIO_IPI_MSG_BYPASS_ACK;
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			ack_type, AUDIO_DSP_TASK_DLCOPY,
			sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			0,
			(char *)&dsp_mem->msg_atod_share_buf.phy_addr);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, ringbuf);
#endif

	return ret;
}

static int mtk_dsp_pcm_copy_ul(struct snd_pcm_substream *substream,
			       int copy_size,
			       struct mtk_base_dsp_mem *dsp_mem,
			       void __user *buf)
{
	int ret = 0, availsize = 0;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct RingBuf *ringbuf = &(dsp_mem->ring_buf);
	unsigned long flags = 0;
	spinlock_t *ringbuf_lock = &dsp_mem->ringbuf_lock;

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &dsp_mem->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &dsp_mem->adsp_buf.aud_buffer.buf_bridge);
#endif
	Ringbuf_Check(&dsp_mem->ring_buf);
	Ringbuf_Bridge_Check(
			&dsp_mem->adsp_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(ringbuf_lock, flags);
	availsize = RingBuf_getDataCount(ringbuf);
	spin_unlock_irqrestore(ringbuf_lock, flags);

	if (availsize < copy_size) {
		pr_info("%s fail copy_size = %d availsize = %d\n", __func__,
			copy_size, RingBuf_getFreeSpace(ringbuf));
		return -1;
	}

	/* get audio_buffer from ring buffer */
	ringbuf_copyto_user_linear(buf, &dsp_mem->ring_buf, copy_size);
	spin_lock_irqsave(ringbuf_lock, flags);
	sync_bridge_ringbuf_readidx(&dsp_mem->adsp_buf.aud_buffer.buf_bridge,
				    &dsp_mem->ring_buf);
	spin_unlock_irqrestore(ringbuf_lock, flags);

	ipi_audio_buf = (void *)dsp_mem->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&dsp_mem->adsp_buf,
		sizeof(struct audio_hw_buffer));
	ret = mtk_scp_ipi_send(
			get_dspscene_by_dspdaiid(id), AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_ULCOPY,
			sizeof(dsp_mem->msg_atod_share_buf.phy_addr),
			0,
			(char *)&dsp_mem->msg_atod_share_buf.phy_addr);

#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s("1 mtk_dsp_ul_handler",
				&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("1 mtk_dsp_ul_handler",
				&dsp_mem->ring_buf);
#endif
	return ret;
}

static int mtk_dsp_pcm_copy(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int channel,
		snd_pcm_uframes_t pos, void __user *buf,
		unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);
	struct mtk_base_dsp_mem *dsp_mem = &dsp->dsp_mem[id];
	int ret = 0;

	if (bytes == 0) {
		pr_info("error %s channel = %d pos = %lu count = %lu bytes = %lu\n",
			__func__, channel, pos, bytes, bytes);
		return -1;
	}

	if (is_audio_task_dsp_ready(get_dspscene_by_dspdaiid(id)) == false) {
		pr_info("%s(), dsp not ready", __func__);
		return -1;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mtk_dsp_pcm_copy_dl(substream, bytes, dsp_mem, buf);
	else
		ret = mtk_dsp_pcm_copy_ul(substream, bytes, dsp_mem, buf);

	return ret;
}

static void audio_dsp_tasklet(struct mtk_base_dsp *dsp, unsigned int core_id)
{
	unsigned long task_value;
	int dsp_scene, task_id, loop_count;
	unsigned long *pdtoa;

	/* using semaphore to sync ap <=> adsp */
	if (get_adsp_semaphore(SEMA_AUDIO))
		pr_info("%s get semaphore fail\n", __func__);

	pdtoa = (unsigned long *)
		&dsp->core_share_mem.ap_adsp_core_mem[core_id]->dtoa_flag;

	loop_count = DSP_IRQ_LOOP_COUNT;

	/* rmb() ensure pdtoa read correct dram data */
	rmb();

	do {
		/* valid bits */
		task_value = fls(*pdtoa);

		/* rmb() ensure task_value read dram(pdtoa) after fls */
		rmb();

		if (task_value) {
			dsp_scene = task_value - 1;
			task_id = get_dspdaiid_by_dspscene(dsp_scene);
#ifdef DEBUG_VERBOSE_IRQ
			pr_info("+%s flag[%llx] task[%s] task_value[%lu] dsp_scene[%d]\n",
				__func__, *pdtoa,
				get_str_by_dsp_dai_id(task_id),
				task_value, dsp_scene);
#endif
			*pdtoa = clr_bit(dsp_scene, pdtoa);

			/* wmb() ensure write data to dram(pdtoa) after clr_bit */
			wmb();
#ifdef DEBUG_VERBOSE_IRQ
			pr_info("-%s flag[%llx] task_id[%d] task_value[%lu]\n",
				__func__, *pdtoa, task_id, task_value);
#endif
			if (task_id >= 0 && task_id < AUDIO_TASK_DAI_NUM) {
				if (!dsp->dsp_mem[task_id].underflowed)
					mtk_dsp_dl_consume_handler(dsp, NULL, task_id);
			}
		}
		loop_count--;
	} while (*pdtoa && task_value && loop_count > 0);

	release_adsp_semaphore(SEMA_AUDIO);

	return;
}

static void audio_dsp_tasklet_core0(struct tasklet_struct *t)
{
	struct mtk_base_dsp *dsp = from_tasklet(dsp, t, dsp_tasklet[ADSP_A_ID]);
	audio_dsp_tasklet(dsp, ADSP_A_ID);
}

static void audio_dsp_tasklet_core1(struct tasklet_struct *t)
{
	struct mtk_base_dsp *dsp = from_tasklet(dsp, t, dsp_tasklet[ADSP_B_ID]);
	audio_dsp_tasklet(dsp, ADSP_B_ID);
}

void audio_irq_handler(int irq, void *data, int core_id)
{
	struct mtk_base_dsp *dsp = (struct mtk_base_dsp *)data;

	if (!dsp) {
		pr_info("%s dsp[%p]\n", __func__, dsp);
		goto IRQ_ERROR;
	}
	if (core_id >= get_adsp_core_total() || core_id < 0) {
		pr_info("%s core_id[%d]\n", __func__, core_id);
		goto IRQ_ERROR;
	}
	if (!dsp->core_share_mem.ap_adsp_core_mem[core_id]) {
		pr_info("%s core_id [%d] ap_adsp_core_mem[%p]\n",
			__func__, core_id,
			dsp->core_share_mem.ap_adsp_core_mem[core_id]);
		goto IRQ_ERROR;
	}
	audio_dsp_tasklet(dsp, core_id);

	return;
IRQ_ERROR:
	pr_info("IRQ_ERROR irq[%d] data[%p] core_id[%d] dsp[%p]\n",
		irq, data, core_id, dsp);
}

#ifdef CFG_RECOVERY_SUPPORT
static int audio_send_reset_event(void)
{
	int ret = 0, i;

	for (i = 0; i < TASK_SCENE_SIZE; i++) {
		if ((i == TASK_SCENE_DEEPBUFFER) ||
			(i == TASK_SCENE_VOIP) ||
			(i == TASK_SCENE_PRIMARY) ||
			(i == TASK_SCENE_FAST) ||
			(i == TASK_SCENE_CAPTURE_UL1) ||
			(i == TASK_SCENE_CAPTURE_RAW)) {
			ret = mtk_scp_ipi_send(i, AUDIO_IPI_MSG_ONLY,
			AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_RESET,
			ADSP_EVENT_READY, 0, NULL);
			pr_info("%s scene = %d\n", __func__, i);
		}
	}
	return ret;
}

static int audio_event_receive(struct notifier_block *this, unsigned long event,
			    void *ptr)
{
	int ret = 0;

	switch (event) {
	case ADSP_EVENT_STOP:
		pr_info("%s event[%lu]\n", __func__, event);
		break;
	case ADSP_EVENT_READY: {
		audio_send_reset_event();
		pr_info("%s event[%lu]\n", __func__, event);
		break;
	}
	default:
		pr_info("%s event[%lu]\n", __func__, event);
	}
	return ret;
}

static struct notifier_block adsp_audio_notifier = {
	.notifier_call = audio_event_receive,
	.priority = PRIMARY_FEATURE_PRI,
};

#endif

static int mtk_dsp_probe(struct snd_soc_component *component)
{
	int ret = 0, id = 0;
	struct mtk_base_dsp *dsp = snd_soc_component_get_drvdata(component);

	pr_info("%s dsp = %p\n", __func__, dsp);
	adsp_audio_wakelock = aud_wake_lock_init(NULL, "adsp_audio_wakelock");

	if (adsp_audio_wakelock == NULL)
		pr_info("%s init adsp_audio_wakelock error\n", __func__);

	ret = snd_soc_add_component_controls(component,
				      dsp_platform_kcontrols,
				      ARRAY_SIZE(dsp_platform_kcontrols));
	if (ret)
		pr_info("%s add_component err ret = %d\n", __func__, ret);

	for (id = 0; id < AUDIO_TASK_DAI_NUM; id++) {
		spin_lock_init(&dsp->dsp_mem[id].ringbuf_lock);
		ret = audio_task_register_callback(get_dspscene_by_dspdaiid(id),
						   mtk_dsp_pcm_ipi_recv);
		if (ret < 0)
			return ret;
	}

	for (id = 0; id < get_adsp_core_total(); id++) {
		if (adsp_irq_registration(id, ADSP_IRQ_AUDIO_ID, audio_irq_handler, dsp) < 0)
			pr_info("%s, ADSP_IRQ_AUDIO not supported\n");
	}

	tasklet_setup(&dsp->dsp_tasklet[ADSP_A_ID], audio_dsp_tasklet_core0);
	tasklet_setup(&dsp->dsp_tasklet[ADSP_B_ID], audio_dsp_tasklet_core1);

#ifdef CFG_RECOVERY_SUPPORT
	adsp_register_notify(&adsp_audio_notifier);
#endif
	return ret;
}

const struct snd_soc_component_driver mtk_dsp_pcm_platform = {
	.name = AFE_DSP_NAME,
	.probe = mtk_dsp_probe,
	.open = mtk_dsp_pcm_open,
	.close = mtk_dsp_pcm_close,
	.hw_params = mtk_dsp_pcm_hw_params,
	.hw_free = mtk_dsp_pcm_hw_free,
	.prepare = mtk_dsp_pcm_hw_prepare,
	.trigger = mtk_dsp_pcm_hw_trigger,
	.pointer = mtk_dsphw_pcm_pointer,
	.copy_user = mtk_dsp_pcm_copy,
};
EXPORT_SYMBOL_GPL(mtk_dsp_pcm_platform);

MODULE_DESCRIPTION("Mediatek dsp platform driver");
MODULE_AUTHOR("chipeng Chang <chipeng.chang@mediatek.com>");
MODULE_LICENSE("GPL v2");
