// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include "mtk-scp-audio-pcm.h"
#include "scp_audio_ipi.h"
#include "scp.h"
#include "mtk-scp-audio-mem-control.h"
#include <audio_task_manager.h>

static int rv_standby_flag;//TODO
static struct mtk_base_afe *audio_afe;
static struct mbox_msg *mbox_msg_temp;
static int mscpSpkProcessEnable;
//#define DEBUG_VERBOSE_IRQ
//#define DEBUG_VERBOSE

static struct snd_soc_dai_driver mtk_scp_audio_dai_driver[] = {
	{
		.name = "audio_task_spk_process",
		.id = SCP_AUD_TASK_SPK_PROCESS_ID,
		.playback = {
				.stream_name = "SCP_SPK_Process",
				.channels_min = 1,
				.channels_max = 2,
				.rates = MTK_PCM_RATES,
				.formats = MTK_PCM_FORMATS,
			},
	},
};

static int scp_audio_task_scence[SCP_AUD_TASK_DAI_NUM] = {
	[SCP_AUD_TASK_SPK_PROCESS_ID]        = TASK_SCENE_RV_SPK_PROCESS,
};

static int scp_audio_feature[SCP_AUD_TASK_DAI_NUM] = {
	[SCP_AUD_TASK_SPK_PROCESS_ID]        = RVSPKPROCESS_FEATURE_ID,
};

static char *scp_task_name[SCP_AUD_TASK_DAI_NUM] = {
	[SCP_AUD_TASK_SPK_PROCESS_ID]         = "spkProcess",
};

int scp_audio_dai_register(struct platform_device *pdev, struct mtk_scp_audio_base *scp_audio)
{
	dev_info(&pdev->dev, "%s()\n", __func__);

	scp_audio->dai_drivers = mtk_scp_audio_dai_driver;
	scp_audio->num_dai_drivers = ARRAY_SIZE(mtk_scp_audio_dai_driver);

	return 0;
};

int scp_set_audio_afe(struct mtk_base_afe *afe)
{
	audio_afe = afe;
	return 0;
}
EXPORT_SYMBOL_GPL(scp_set_audio_afe);

struct mtk_base_afe *scp_get_audio_afe(void)
{
	if (audio_afe == NULL)
		pr_info("%s audio_afe == NULL", __func__);

	return audio_afe;
}

/* scene <--> dai mapping */
int get_scene_by_daiid(int id)
{
	if (id < 0 || id >= SCP_AUD_TASK_DAI_NUM) {
		pr_info("%s(), id err: %d\n", __func__, id);
		return -1;
	}

	return scp_audio_task_scence[id];
}
EXPORT_SYMBOL(get_scene_by_daiid);

int get_daiid_by_scene(int scene)
{
	int id;
	int ret = -1;

	if (scene < 0) {
		pr_info("%s() scene err: %d\n", __func__, scene);
		return -1;
	}

	for (id = 0; id < SCP_AUD_TASK_DAI_NUM; id++) {
		if (scp_audio_task_scence[id] == scene) {
			ret = id;
			break;
		}
	}

	if (ret < 0)
		pr_info("%s() scene is not in scp_audio_task_scence\n",
			__func__);

	return ret;
}
EXPORT_SYMBOL(get_daiid_by_scene);

/* feature <--> dai mapping */
int get_feature_by_daiid(int id)
{
	if (id < 0 || id >= SCP_AUD_TASK_DAI_NUM) {
		pr_info("%s(), id err: %d\n", __func__, id);
		return -1;
	}

	return scp_audio_feature[id];
}

int get_daiid_by_afeid(int id)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();

	if (scp_audio == NULL)
		return -1;
	//TODO if (id < 0 || id >= SCP_AUD_TASK_DAI_NUM) {
	//	pr_info("%s(), id err: %d\n", __func__, id);
	//	return -1;
	//}
	if (id == scp_audio->ul_memif ||
	    id == scp_audio->dl_memif ||
	    id == scp_audio->ref_memif)
		return SCP_AUD_TASK_SPK_PROCESS_ID;
	else
		return -1;
}
EXPORT_SYMBOL(get_daiid_by_afeid);

const char *get_taskname_by_daiid(const int daiid)
{
	if (daiid < 0 || daiid >= SCP_AUD_TASK_DAI_NUM) {
		pr_info("%s(), daiid err: %d\n", __func__, daiid);
		return NULL;
	}

	return scp_task_name[daiid];
}
EXPORT_SYMBOL_GPL(get_taskname_by_daiid);

int get_daiid_by_taskname(const char *task_name)
{
	int ret = -1;
	int id;

	for (id = 0; id < SCP_AUD_TASK_DAI_NUM; id++) {
		if (strstr(task_name, scp_task_name[id]))
			ret = id;
	}

	if (ret < 0)
		pr_info("%s(), %s has no task id, ret %d",
			__func__, task_name, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(get_daiid_by_taskname);

struct scp_aud_task_base *get_taskbase_by_daiid(const int daiid)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();

	if (daiid < 0 || daiid >= SCP_AUD_TASK_DAI_NUM) {
		pr_info("%s(), daiid err: %d\n", __func__, daiid);
		return NULL;
	}

	return &scp_audio->task_base[daiid];
}

/*
 * common function for IPI message
 */
int mtk_scp_audio_ipi_send(int task_scene, int data_type, int ack_type,
		     uint16_t msg_id, uint32_t param1, uint32_t param2,
		     char *payload)
{
	struct ipi_msg_t ipi_msg;
	int send_result = 0;

	memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));

	if (!is_audio_task_dsp_ready(task_scene)) {
		pr_info("%s(), is_scp_ready send false\n", __func__);
		return -1;
	}

	send_result = audio_send_ipi_msg(
		&ipi_msg, task_scene,
		AUDIO_IPI_LAYER_TO_DSP, data_type,
		ack_type, msg_id, param1, param2,
		(char *)payload);
	if (send_result)
		pr_info("%s(),scp_ipi send fail\n", __func__);

	return send_result;
}
EXPORT_SYMBOL(mtk_scp_audio_ipi_send);

/* get scene for ipi set/get buf API */
int mtk_get_ipi_buf_scene_rv(void)
{
	int task_scene = -1;

	if (rv_standby_flag)
		return task_scene;

	//TODO if (get_task_attr(AUDIO_TASK_CALL_FINAL_ID,
	//		  ADSP_TASK_ATTR_RUNTIME) > 0)
	task_scene = TASK_SCENE_RV_SPK_PROCESS;
	//else {
	//	pr_info("%s(), spk process not enabled\n", __func__);
	//	return result;
	//}

	return task_scene;
}
EXPORT_SYMBOL(mtk_get_ipi_buf_scene_rv);

int send_task_sharemem_to_scp(struct mtk_scp_audio_base *scp_audio, int daiid)
{
	int ret = 0;
	struct scp_aud_task_base *task_base;
	struct ipi_msg_t ipi_msg;

	pr_info("%s(+)\n", __func__);

	if (scp_audio == NULL) {
		pr_info("%s scp_audio == NULL\n", __func__);
		return -1;
	}

	if (daiid < 0 || daiid >= SCP_AUD_TASK_DAI_NUM) {
		pr_info("%s daiid = %d\n", __func__, daiid);
		return -1;
	}

	task_base = get_taskbase_by_daiid(daiid);

	if (task_base == NULL) {
		pr_info("%s task_base == NULL\n", __func__);
		return -1;
	}

	/* send share message to scp side */
	memcpy((void *)task_base->ipi_payload_buf,
	       (void *)&task_base->msg_atod_share_buf,
	       sizeof(struct audio_dsp_dram));

	ret = audio_send_ipi_msg(
		&ipi_msg, get_scene_by_daiid(daiid),
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_MSGA2DSHAREMEM,
		sizeof(struct audio_dsp_dram), 0,
		(char *)task_base->ipi_payload_buf);
	if (ret)
		pr_info("%s(), dai [%d]send a2d ipi fail\n",
			__func__, daiid);

	/* send share message to SCP side */
	memcpy((void *)task_base->ipi_payload_buf,
	       (void *)&task_base->msg_dtoa_share_buf,
	       sizeof(struct audio_dsp_dram));

	ret = audio_send_ipi_msg(
		&ipi_msg, get_scene_by_daiid(daiid),
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_MSGD2ASHAREMEM,
		sizeof(struct audio_dsp_dram), 0,
		(char *)task_base->ipi_payload_buf);
	if (ret)
		pr_info("%s(), dai [%d]send d2a ipi fail\n",
			__func__, daiid);

	pr_info("%s(-)\n", __func__);
	return 0;
}

int get_audio_mem_type(struct snd_pcm_substream *substream)
{
	if (substream->runtime->dma_addr < 0x20000000)
		return MEMORY_SRAM;
	else
		return MEMORY_DRAM;
}

int scp_audio_get_pcmdir(int dir, struct audio_hw_buffer buf)
{
	int ret = -1;
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();

	if (dir == SNDRV_PCM_STREAM_CAPTURE)
		ret = SCP_AUDIO_TASK_PCM_HWPARAM_UL;
	else if (dir == SNDRV_PCM_STREAM_PLAYBACK)
		ret = SCP_AUDIO_TASK_PCM_HWPARAM_DL;

	if (scp_audio->ref_memif == buf.audio_memiftype)
		ret = SCP_AUDIO_TASK_PCM_HWPARAM_REF;

	return ret;
}

static int set_aud_buf_attr(struct audio_hw_buffer *audio_hwbuf,
			    struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    int irq_usage,
			    struct snd_soc_dai *dai)
{
	int ret = 0;

	if (audio_hwbuf == NULL) {
		pr_info("%s audio_hwbuf == NULL", __func__);
		return -1;
	}

	ret = set_afe_audio_pcmbuf(audio_hwbuf, substream);
	if (ret < 0) {
		pr_info("set_afe_audio_pcmbuf fail\n");
		return -1;
	}

	audio_hwbuf->hw_buffer = SCP_BUFFER_HW_MEM; //TODO BUFFER_TYPE_HW_MEM;
	audio_hwbuf->irq_num = irq_usage;
	audio_hwbuf->audio_memiftype = dai->id;
	audio_hwbuf->memory_type = get_audio_mem_type(substream);

	ret = set_audiobuffer_threshold(audio_hwbuf, substream);
	if (ret < 0) {
		pr_info("set_audiobuffer_threshold fail\n");
		return -1;
	}

	ret = set_audiobuffer_attribute(
		audio_hwbuf,
		substream, params,
		scp_audio_get_pcmdir(substream->stream, *audio_hwbuf));
	if (ret < 0) {
		pr_info("set_audiobuffer_attribute fail\n");
		return -1;
	}

	pr_info("%s() memiftype: %d ch: %u fmt: %u rate: %u dir: %d, start_thres: %u stop_thres: %u period_size: %d period_cnt: %d\n",
		__func__,
		audio_hwbuf->audio_memiftype,
		audio_hwbuf->aud_buffer.buffer_attr.channel,
		audio_hwbuf->aud_buffer.buffer_attr.format,
		audio_hwbuf->aud_buffer.buffer_attr.rate,
		audio_hwbuf->aud_buffer.buffer_attr.direction,
		audio_hwbuf->aud_buffer.start_threshold,
		audio_hwbuf->aud_buffer.stop_threshold,
		audio_hwbuf->aud_buffer.period_size,
		audio_hwbuf->aud_buffer.period_count);

	return 0;
}

/* function warp playback buffer information send to scp */
int afe_pcm_ipi_to_scp(int command, struct snd_pcm_substream *substream,
		       struct snd_pcm_hw_params *params,
		       struct snd_soc_dai *dai,
		       struct mtk_base_afe *afe)
{
	int ret = 0, daiid;
	struct audio_hw_buffer *audio_hwbuf;
	struct mtk_base_afe_memif *memif = &afe->memif[dai->id];
	struct scp_aud_task_base *task_base;
	const char *task_name;
	unsigned long vaddr;
	dma_addr_t paddr;

	daiid = get_daiid_by_afeid(dai->id);
	if (daiid < 0 || daiid >= SCP_AUD_TASK_DAI_NUM)
		return -1;
	task_name = get_taskname_by_daiid(daiid);
	task_base = get_taskbase_by_daiid(daiid);

	pr_info("%s(), %s send cmd 0x%x\n", __func__, task_name, command);

	if (task_base == NULL) {
		pr_info("%s task_base == NULL\n", __func__);
		return -1;
	}

	if (task_base->msg_atod_share_buf.phy_addr <= 0 ||
	    !task_base->msg_atod_share_buf.va_addr) {
		pr_warn("%s msg_atod_share_buf error!\n", __func__);
		return -1;
	}

	vaddr = task_base->msg_atod_share_buf.va_addr;
	paddr = task_base->msg_atod_share_buf.phy_addr;
	audio_hwbuf = (struct audio_hw_buffer *)vaddr;

	/* send msg by task by unsing common function */
	switch (command) {
	case AUDIO_DSP_TASK_PCM_HWPARAM:
	case AUDIO_DSP_TASK_PCM_PREPARE:
		/* send to task with hw_param information, buffer and pcm attribute
		 * or send to task with prepare status
		 */

		set_aud_buf_attr(audio_hwbuf,
				 substream,
				 params,
				 memif->irq_usage,
				 dai);
		ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(daiid),
				       AUDIO_IPI_PAYLOAD,
				       AUDIO_IPI_MSG_NEED_ACK,
				       command,
				       sizeof(task_base->msg_atod_share_buf.phy_addr),
				       0,
				       (char *)
				       &paddr);
		break;
	case AUDIO_DSP_TASK_PCM_HWFREE:
		set_aud_buf_attr(audio_hwbuf,
				 substream,
				 params,
				 memif->irq_usage,
				 dai);
		/* send to task with prepare status */
		ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(daiid),
				       AUDIO_IPI_MSG_ONLY,
				       AUDIO_IPI_MSG_NEED_ACK,
				       AUDIO_DSP_TASK_PCM_HWFREE,
				       scp_audio_get_pcmdir(substream->stream,
				       *audio_hwbuf),
				       0,
				       NULL);
		break;
	default:
		pr_warn("%s error command: %d\n", __func__, command);
		return -1;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(afe_pcm_ipi_to_scp);

int mtk_reinit_scp_audio(void)
{
	int ret = 0, scene = 0, daiid = 0;

	for (daiid = 0; daiid < SCP_AUD_TASK_DAI_NUM; daiid++) {
		send_task_sharemem_to_scp(get_scp_audio_base(), daiid);
		scene = get_scene_by_daiid(daiid);
		ret = mtk_scp_audio_ipi_send(scene, AUDIO_IPI_MSG_ONLY,
					     AUDIO_IPI_MSG_BYPASS_ACK,
					     AUDIO_DSP_TASK_RESET,
					     0, 0, NULL);
		pr_info("%s scene = %d\n", __func__, scene);
	}
	return 0;
}

int scp_audio_pcm_recover_event(struct notifier_block *this,
				  unsigned long event,
				  void *ptr)
{
	switch (event) {
	case SCP_EVENT_READY: {
		pr_info("%s(), SCP_EVENT_READY\n", __func__);
		mtk_reinit_scp_audio();
		rv_standby_flag = 0;
		break;
	}
	case SCP_EVENT_STOP:
		rv_standby_flag = 1;
		pr_info("%s(), SCP_EVENT_STOP\n", __func__);
		break;
	}
	return NOTIFY_DONE;
}

void init_scp_spk_process_enable(int enable_flag)
{
	pr_info("%s enable_flag: %d\n", __func__, enable_flag);
	mscpSpkProcessEnable = enable_flag;
}

static int scp_spk_process_enable_set(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	pr_info("%s enable_flag: %d\n", __func__, mscpSpkProcessEnable);
	mscpSpkProcessEnable = ucontrol->value.integer.value[0];

	return 0;
}

static int scp_spk_process_enable_get(struct snd_kcontrol *kcontrol,
					    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mscpSpkProcessEnable;

	return 0;
}

static int is_scp_ready_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = is_scp_audio_ready();
	return 0;
}

static int scp_dl_sharemem_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;

	if (scp_audio == NULL)
		return -1;

	memif_num = scp_audio->dl_memif;

	if (memif_num < 0)
		return 0;

	memif = &audio_afe->memif[memif_num];
	ucontrol->value.integer.value[0] = memif->use_scp_share_mem;

	return 0;
}

static int scp_dl_sharemem_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;

	if (scp_audio == NULL)
		return -1;

	memif_num = scp_audio->dl_memif;
	if (memif_num >= 0) {
		memif = &audio_afe->memif[memif_num];
		memif->use_scp_share_mem = ucontrol->value.integer.value[0];
	}

	return 0;
}

static int scp_ul_sharemem_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;

	if (scp_audio == NULL)
		return -1;

	memif_num = scp_audio->ul_memif;

	if (memif_num < 0)
		return 0;

	memif = &audio_afe->memif[memif_num];
	ucontrol->value.integer.value[0] = memif->use_scp_share_mem;

	return 0;
}

static int scp_ul_sharemem_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;

	if (scp_audio == NULL)
		return -1;

	memif_num = scp_audio->ul_memif;
	if (memif_num >= 0) {
		memif = &audio_afe->memif[memif_num];
		memif->use_scp_share_mem = ucontrol->value.integer.value[0];
	}

	return 0;
}

static int scp_ref_sharemem_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;

	if (scp_audio == NULL)
		return -1;

	memif_num = scp_audio->ref_memif;

	if (memif_num < 0)
		return 0;

	memif = &audio_afe->memif[memif_num];
	ucontrol->value.integer.value[0] = memif->use_scp_share_mem;

	return 0;
}

static int scp_ref_sharemem_set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct mtk_base_afe_memif *memif;
	int memif_num = -1;

	if (scp_audio == NULL)
		return -1;

	memif_num = scp_audio->ref_memif;
	if (memif_num >= 0) {
		memif = &audio_afe->memif[memif_num];
		memif->use_scp_share_mem = ucontrol->value.integer.value[0];
	}

	return 0;
}

static const struct snd_kcontrol_new scp_audio_platform_kcontrols[] = {
	SOC_SINGLE_EXT("scp_spk_process_enable", SND_SOC_NOPM, 0, 0xffff, 0,
		       scp_spk_process_enable_get,
		       scp_spk_process_enable_set),
	SOC_SINGLE_EXT("is_scp_ready", SND_SOC_NOPM, 0, 0xffff, 0,
		       is_scp_ready_get, NULL),
	SOC_SINGLE_EXT("mtk_scp_spk_dl_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       scp_dl_sharemem_get,
		       scp_dl_sharemem_set),
	SOC_SINGLE_EXT("mtk_scp_spk_ul_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       scp_ul_sharemem_get,
		       scp_ul_sharemem_set),
	SOC_SINGLE_EXT("mtk_scp_spk_iv_scenario",
		       SND_SOC_NOPM, 0, 0x1, 0,
		       scp_ref_sharemem_get,
		       scp_ref_sharemem_set),
};

static int mtk_scp_audio_pcm_open(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mtk_scp_audio_base *scp_aud = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	int feature_id = get_feature_by_daiid(id);
	const char *task_name = get_taskname_by_daiid(id);

	pr_info("%s(), %s\n", __func__, task_name);

	if (!scp_aud || !task_base) {
		pr_info("%s(), NULL! %p,%p\n", __func__, scp_aud, task_base);
		return -1;
	}

	memcpy((void *)(&(runtime->hw)), (void *)scp_aud->scp_audio_hardware,
	       sizeof(struct snd_pcm_hardware));

	scp_register_feature(feature_id);
	/* send to task with open information */
	ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(id),
				     AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_NEED_ACK,
				     AUDIO_DSP_TASK_OPEN,
				     0, 0, NULL);
	if (ret < 0) {
		pr_info("%s() ret[%d]\n", __func__, ret);
		scp_deregister_feature(feature_id);
		goto error;
	}

	/* set the wait_for_avail to 2 sec*/
	substream->wait_time = msecs_to_jiffies(2 * 1000);

	task_base->substream = substream;
	task_base->underflowed = 0;

error:
	return ret;
}

static int mtk_scp_audio_pcm_close(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	int feature_id = get_feature_by_daiid(id);
	const char *task_name = get_taskname_by_daiid(id);

	pr_info("%s(), %s\n", __func__, task_name);

	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return -1;
	}

	/* send to task with close information */
	ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(id),
				     AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_NEED_ACK,
				     AUDIO_DSP_TASK_CLOSE, 0, 0, NULL);
	if (ret)
		pr_info("%s ret[%d]\n", __func__, ret);

	scp_deregister_feature(feature_id);

	task_base->substream = NULL;

	return ret;
}

static int mtk_scp_audio_pcm_hw_params(struct snd_soc_component *component,
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_scp_audio_base *scp_audio = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	void *ipi_audio_buf; /* dsp <-> audio data struct*/
	int ret = 0;
	const char *task_name = get_taskname_by_daiid(id);

	pr_info("%s(), %s\n", __func__, task_name);
	if (!scp_audio || !task_base) {
		pr_info("%s(), NULL! %p,%p\n", __func__, scp_audio, task_base);
		return -1;
	}

	reset_audiobuffer_hw(&task_base->share_hw_buf);
	reset_audiobuffer_hw(&task_base->temp_work_buf);
	RingBuf_Reset(&task_base->ring_buf);

	scp_audio->request_dram_resource(scp_audio->dev);

	/* if already allocate , free it.*/
	if (substream->dma_buffer.area) {
		ret = scp_audio_free_sharemem_ring(task_base,
						   scp_audio->genpool);
		if (!ret)
			release_snd_dmabuffer(&substream->dma_buffer);
	}
	if (ret < 0) {
		pr_warn("%s err\n", __func__);
		return -1;
	}

	/* allocate ring buffer wioth share memory */
	ret = scp_audio_allocate_sharemem_ring(task_base,
					       params_buffer_bytes(params),
					       scp_audio->genpool);
	if (ret < 0) {
		pr_warn("%s err\n", __func__);
		return -1;
	}

#ifdef DEBUG_VERBOSE
	//dump_audio_dram(&task_base->msg_atod_share_buf);
	//dump_audio_dram(&task_base->msg_dtoa_share_buf);
	//dump_audio_dram(&task_base->ring_share_buf);
#endif
	ret = dram_to_snd_dmabuffer(&task_base->ring_share_buf,
					&substream->dma_buffer);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_hw(&task_base->share_hw_buf,
				 SCP_BUFFER_SHARE_MEM);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_memorytype(&task_base->share_hw_buf,
					 MEMORY_DRAM);
	if (ret < 0)
		goto error;
	ret = set_audiobuffer_attribute(&task_base->share_hw_buf,
					substream,
					params,
					scp_audio_get_pcmdir(substream->stream,
					task_base->share_hw_buf));
	if (ret < 0)
		goto error;

	memcpy(&task_base->temp_work_buf, &task_base->share_hw_buf,
	       sizeof(struct audio_hw_buffer));
	/* send audio_hw_buffer to SCP side */
	ipi_audio_buf = (void *)task_base->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&task_base->share_hw_buf,
	       sizeof(struct audio_hw_buffer));

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &task_base->ring_buf);
#endif

	/* send to task with hw_param information , buffer and pcm attribute */
	mtk_scp_audio_ipi_send(get_scene_by_daiid(id),
			       AUDIO_IPI_PAYLOAD,
			       AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_HWPARAM,
			       sizeof(task_base->msg_atod_share_buf.phy_addr),
			       0,
			       (char *)&task_base->msg_atod_share_buf.phy_addr);

	return ret;

error:
	pr_err("%s err\n", __func__);
	return -1;
}

static int mtk_scp_audio_pcm_hw_free(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_scp_audio_base *scp_audio = snd_soc_component_get_drvdata(component);
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	const char *task_name = get_taskname_by_daiid(id);

	pr_info("%s(), %s\n", __func__, task_name);
	if (!scp_audio || !task_base) {
		pr_info("%s(), NULL! %p,%p\n", __func__, scp_audio, task_base);
		return -1;
	}

	/* send to task with free status */
	ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(id),
				     AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_NEED_ACK,
				     AUDIO_DSP_TASK_HWFREE, 1, 0, NULL);

	if (ret)
		pr_info("%s ret[%d]\n", __func__, ret);


	if (scp_audio->genpool != NULL && substream->dma_buffer.area) {
		ret = scp_audio_free_sharemem_ring(task_base,
						   scp_audio->genpool);
		if (!ret)
			release_snd_dmabuffer(&substream->dma_buffer);
	}

	/* reset buf setting */
	ret = reset_audiobuffer_hw(&task_base->share_hw_buf);

	scp_audio->release_dram_resource(scp_audio->dev);

	return ret;
}

static int mtk_scp_audio_pcm_hw_prepare(struct snd_soc_component *component,
		struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct audio_hw_buffer *hw_buf = &task_base->share_hw_buf;
	const char *task_name = get_taskname_by_daiid(id);

	pr_info("%s(), %s\n", __func__, task_name);
	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return -1;
	}

	/* The data type of stop_threshold in userspace is unsigned int.
	 * However its data type in kernel space is unsigned long.
	 * It needs to convert to ULONG_MAX in kernel space
	 */
	if (substream->runtime->stop_threshold == ~(0U))
		substream->runtime->stop_threshold = ULONG_MAX;

	clear_audiobuffer_hw(hw_buf);
	RingBuf_Reset(&task_base->ring_buf);
	RingBuf_Bridge_Reset(&hw_buf->aud_buffer.buf_bridge);
	RingBuf_Bridge_Reset(
		&task_base->temp_work_buf.aud_buffer.buf_bridge);

	ret = set_audiobuffer_threshold(hw_buf, substream);
	if (ret < 0)
		pr_warn("%s set_audiobuffer_attribute err\n", __func__);

	pr_info("%s(), %s start_threshold: %u stop_threshold: %u period_size: %d period_count: %d\n",
		__func__, task_name,
		hw_buf->aud_buffer.start_threshold,
		hw_buf->aud_buffer.stop_threshold,
		hw_buf->aud_buffer.period_size,
		hw_buf->aud_buffer.period_count);

	/* send audio_hw_buffer to SCP side */
	ipi_audio_buf = (void *)task_base->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)hw_buf,
	       sizeof(struct audio_hw_buffer));

	/* send to task with prepare status */
	mtk_scp_audio_ipi_send(get_scene_by_daiid(id),
			       AUDIO_IPI_PAYLOAD,
			       AUDIO_IPI_MSG_NEED_ACK,
			       AUDIO_DSP_TASK_PREPARE,
			       sizeof(task_base->msg_atod_share_buf.phy_addr),
			       0,
			       (char *)&task_base->msg_atod_share_buf.phy_addr);
	return ret;
}

static int mtk_scp_audio_start(struct snd_pcm_substream *substream,
			 struct mtk_scp_audio_base *scp_audio)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	const char *task_name = get_taskname_by_daiid(id);

	pr_info("%s(), %s\n", __func__, task_name);
	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return -1;
	}

	task_base->underflowed = 0;

	ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(id),
				     AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_DIRECT_SEND,
				     AUDIO_DSP_TASK_START,
				     1, 0, NULL);
	return ret;
}

static int mtk_scp_audio_stop(struct snd_pcm_substream *substream,
			struct mtk_scp_audio_base *scp_audio)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;

	/* Avoid print log in alsa stop. If underflow happens,
	 * log will be printed in ISR.
	 */
	const char *task_name = get_taskname_by_daiid(id);

	pr_info("%s(), %s\n", __func__, task_name);

	ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(id),
				     AUDIO_IPI_MSG_ONLY,
				     AUDIO_IPI_MSG_DIRECT_SEND,
				     AUDIO_DSP_TASK_STOP,
				     1, 0, NULL);

	return ret;
}

static int mtk_scp_audio_pcm_hw_trigger(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int cmd)
{
	struct mtk_scp_audio_base *scp_audio = snd_soc_component_get_drvdata(component);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_scp_audio_start(substream, scp_audio);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_scp_audio_stop(substream, scp_audio);
	}
	return -EINVAL;
}

static int mtk_scp_audio_pcm_copy_dl(struct snd_pcm_substream *substream,
			       int copy_size,
			       struct scp_aud_task_base *task_base,
			       void __user *buf)
{
	int ret = 0, availsize = 0;
	int ack_type;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct RingBuf *ringbuf = &task_base->ring_buf;
	struct ringbuf_bridge *buf_bridge =
		&(task_base->share_hw_buf.aud_buffer.buf_bridge);
	unsigned long flags = 0;
	spinlock_t *ringbuf_lock = &task_base->ringbuf_lock;
	const char *task_name = get_taskname_by_daiid(id);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &task_base->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &task_base->share_hw_buf.aud_buffer.buf_bridge);
#endif

	if (substream->runtime->status->state == SNDRV_PCM_STATE_XRUN) {
		pr_info_ratelimited("%s() %s state[%d]\n",
				    __func__, task_name,
				    substream->runtime->status->state);
		return -1;
	}

	Ringbuf_Check(ringbuf);
	Ringbuf_Bridge_Check(
		&task_base->share_hw_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(ringbuf_lock, flags);
	availsize = RingBuf_getFreeSpace(ringbuf);
	spin_unlock_irqrestore(ringbuf_lock, flags);
	if (availsize < copy_size) {
		pr_info("%s, id = %d, fail copy_size = %d availsize = %d\n",
			__func__, id, copy_size, RingBuf_getFreeSpace(ringbuf));
		dump_rbuf_s("check dlcopy", &task_base->ring_buf);
		dump_rbuf_bridge_s("check dlcopy",
			   &task_base->share_hw_buf.aud_buffer.buf_bridge);
		return -1;
	}

	RingBuf_copyFromUserLinear(ringbuf, buf, copy_size);
	RingBuf_Bridge_update_writeptr(buf_bridge, copy_size);

	/* send audio_hw_buffer to SCP side*/
	ipi_audio_buf = (void *)task_base->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&task_base->share_hw_buf,
	       sizeof(struct audio_hw_buffer));

	if (substream->runtime->status->state != SNDRV_PCM_STATE_RUNNING)
		ack_type = AUDIO_IPI_MSG_NEED_ACK;
	else
		ack_type = AUDIO_IPI_MSG_BYPASS_ACK;
	ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(id), AUDIO_IPI_PAYLOAD,
			ack_type, AUDIO_DSP_TASK_DLCOPY,
			sizeof(task_base->msg_atod_share_buf.phy_addr),
			0,
			(char *)&task_base->msg_atod_share_buf.phy_addr);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, ringbuf);
#endif

	return ret;
}

static int mtk_scp_audio_pcm_copy_ul(struct snd_pcm_substream *substream,
			       int copy_size,
			       struct scp_aud_task_base *task_base,
			       void __user *buf)
{
	int ret = 0, availsize = 0;
	void *ipi_audio_buf; /* dsp <-> audio data struct */
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct RingBuf *ringbuf = &(task_base->ring_buf);
	unsigned long flags = 0;
	spinlock_t *ringbuf_lock = &task_base->ringbuf_lock;

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &task_base->ring_buf);
	dump_rbuf_bridge_s(__func__,
			   &task_base->share_hw_buf.aud_buffer.buf_bridge);
#endif
	Ringbuf_Check(&task_base->ring_buf);
	Ringbuf_Bridge_Check(
			&task_base->share_hw_buf.aud_buffer.buf_bridge);

	spin_lock_irqsave(ringbuf_lock, flags);
	availsize = RingBuf_getDataCount(ringbuf);
	spin_unlock_irqrestore(ringbuf_lock, flags);

	if (availsize < copy_size) {
		pr_info("%s fail copy_size = %d availsize = %d\n", __func__,
			copy_size, RingBuf_getFreeSpace(ringbuf));
		return -1;
	}

	/* get audio_buffer from ring buffer */
	ringbuf_copyto_user_linear(buf, &task_base->ring_buf, copy_size);
	spin_lock_irqsave(ringbuf_lock, flags);
	sync_bridge_ringbuf_readidx(&task_base->share_hw_buf.aud_buffer.buf_bridge,
				    &task_base->ring_buf);
	spin_unlock_irqrestore(ringbuf_lock, flags);

	ipi_audio_buf = (void *)task_base->msg_atod_share_buf.va_addr;
	memcpy((void *)ipi_audio_buf, (void *)&task_base->share_hw_buf,
		sizeof(struct audio_hw_buffer));
	ret = mtk_scp_audio_ipi_send(get_scene_by_daiid(id), AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_NEED_ACK, AUDIO_DSP_TASK_ULCOPY,
			sizeof(task_base->msg_atod_share_buf.phy_addr),
			0,
			(char *)&task_base->msg_atod_share_buf.phy_addr);

#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s("1 mtk_scp_audio_ul_handler",
				&task_base->share_hw_buf.aud_buffer.buf_bridge);
	dump_rbuf_s("1 mtk_scp_audio_ul_handler",
				&task_base->ring_buf);
#endif
	return ret;
}

static int mtk_scp_audio_pcm_copy(struct snd_soc_component *component,
		struct snd_pcm_substream *substream, int channel,
		snd_pcm_uframes_t pos, void __user *buf,
		unsigned long bytes)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	int ret = 0;

	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return -1;
	}

	if (bytes <= 0) {
		pr_info(
			"error %s channel = %d pos = %lu count = %lu bytes = %lu\n",
			__func__, channel, pos, bytes, bytes);
		return -1;
	}

	if (is_audio_task_dsp_ready(TASK_SCENE_RV_SPK_PROCESS) == false) { //TODO not fix
		pr_info("%s(), scp not ready", __func__);
		return -1;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		ret = mtk_scp_audio_pcm_copy_dl(substream, bytes, task_base, buf);
	else
		ret = mtk_scp_audio_pcm_copy_ul(substream, bytes, task_base, buf);

	return ret;
}

static bool mtk_scp_aud_check_exception(struct mtk_scp_audio_base *scp_aud,
				    struct ipi_msg_t *ipi_msg, int id)
{
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	const char *task_name = get_taskname_by_daiid(id);

	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return -1;
	}

	if (!task_base->substream) {
		pr_info_ratelimited("%s() %s substream NULL\n",
				    __func__, task_name);
		return false;
	}

	if (!snd_pcm_running(task_base->substream)) {
		pr_info_ratelimited("%s() %s state[%d]\n",
			__func__, task_name,
			task_base->substream->runtime->status->state);
		return false;
	}

	/* reset message */
	if (ipi_msg && ipi_msg->param2 == SCP_DL_CONSUME_RESET) {
		pr_info("%s() %s scp audio reset\n", __func__, task_name);
		RingBuf_Reset(&task_base->ring_buf);
		snd_pcm_period_elapsed(task_base->substream);
		return true;
	}

	/* underflow message */
	if (ipi_msg && ipi_msg->param2 == SCP_DL_CONSUME_UNDERFLOW) {
		pr_info("%s() %s scp audio underflow\n", __func__, task_name);
		task_base->underflowed = true;

		snd_pcm_period_elapsed(task_base->substream);

		return true;
	}
	return false;

}

static void mtk_scp_aud_dl_consume_handler(struct mtk_scp_audio_base *scp_aud,
				       struct ipi_msg_t *ipi_msg, int id)
{
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	spinlock_t *ringbuf_lock = &task_base->ringbuf_lock;
	struct snd_pcm_substream *substream;
	unsigned long flags = 0;

	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return;
	}

	if (!task_base->substream) {
		pr_info("%s(), substream NULL!\n", __func__);
		return;
	}

	if (!snd_pcm_running(task_base->substream)) {
		pr_info("%s(), substream not running!\n", __func__);
		return;
	}
	substream = task_base->substream;


	// handle for no restart pcm, copy audio_hw_buffer from msg payload, others from share mem
	if ((substream->runtime->stop_threshold > substream->runtime->start_threshold) && ipi_msg) {
		memcpy((void *)&task_base->temp_work_buf, ipi_msg->payload,
		       sizeof(struct audio_buffer));
	} else {
		unsigned long long read, base, end;

		read = mbox_msg_temp->pRead;
		base = task_base->temp_work_buf.aud_buffer.buf_bridge.pBufBase;
		end = task_base->temp_work_buf.aud_buffer.buf_bridge.pBufEnd;
		if (read < base || read > end) {
			pr_info("%s(), wrong readidx 0x%llx from SCP!\n", __func__, read);
			return;
		}
		task_base->temp_work_buf.aud_buffer.buf_bridge.pRead = mbox_msg_temp->pRead;
	}

	spin_lock_irqsave(ringbuf_lock, flags);
	task_base->share_hw_buf.aud_buffer.buf_bridge.pRead =
		task_base->temp_work_buf.aud_buffer.buf_bridge.pRead;
#ifdef DEBUG_VERBOSE_IRQ
	dump_rbuf_s("dl_consume before sync", &task_base->ring_buf);
	dump_rbuf_bridge_s("dl_consume before sync",
			   &task_base->share_hw_buf.aud_buffer.buf_bridge);
#endif
	sync_ringbuf_readidx(
		&task_base->ring_buf,
		&task_base->share_hw_buf.aud_buffer.buf_bridge);

	spin_unlock_irqrestore(ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE_IRQ
	dump_rbuf_s("dl_consume after sync", &task_base->ring_buf);
#endif
	/* notify subsream */
	snd_pcm_period_elapsed(task_base->substream);
}

static void mtk_scp_aud_dl_handler(struct mtk_scp_audio_base *scp_aud,
			       struct ipi_msg_t *ipi_msg, int id)
{
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);

	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return;
	}

	if (task_base->substream == NULL) {
		pr_info("%s = substream == NULL\n", __func__);
		goto SCP_IRQ_HANDLER_ERR;
	}

	if (!snd_pcm_running(task_base->substream)) {
		pr_info("%s = state[%d]\n", __func__,
			 task_base->substream->runtime->status->state);
		goto SCP_IRQ_HANDLER_ERR;
	}

	/* notify subsream */
	snd_pcm_period_elapsed(task_base->substream);
SCP_IRQ_HANDLER_ERR:
	return;
}

static void mtk_scp_aud_ul_handler(struct mtk_scp_audio_base *scp_aud,
			       struct ipi_msg_t *ipi_msg, int id)
{
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	void *ipi_audio_buf;
	unsigned long flags;
	spinlock_t *ringbuf_lock = &task_base->ringbuf_lock;

	if (!task_base) {
		pr_info("%s(), task base NULL!\n", __func__);
		return;
	}

	if (!task_base->substream) {
		pr_info("%s substream NULL\n", __func__);
		return;
	}


	if (!snd_pcm_running(task_base->substream)) {
		pr_info("%s = state[%d]\n", __func__,
			 task_base->substream->runtime->status->state);
		goto SCP_IRQ_HANDLER_ERR;
	}

	if (task_base->substream->runtime->status->state
	    != SNDRV_PCM_STATE_RUNNING) {
		pr_info("%s = state[%d]\n", __func__,
			 task_base->substream->runtime->status->state);
		goto SCP_IRQ_HANDLER_ERR;
	}

	if (ipi_msg && ipi_msg->param2 == SCP_UL_READ_RESET) {
		spin_lock_irqsave(ringbuf_lock, flags);
		RingBuf_Reset(&task_base->ring_buf);
		/* set buf size full to trigger pcm_read */
		task_base->ring_buf.datacount = task_base->ring_buf.bufLen;
		spin_unlock_irqrestore(ringbuf_lock, flags);
		pr_info("%s reset UL\n", __func__);
		snd_pcm_period_elapsed(task_base->substream);
		goto SCP_IRQ_HANDLER_ERR;
	}

	/* upadte for write index*/
	ipi_audio_buf = (void *)task_base->msg_dtoa_share_buf.va_addr;

	memcpy((void *)&task_base->temp_work_buf, (void *)ipi_audio_buf,
	       sizeof(struct audio_hw_buffer));

	task_base->share_hw_buf.aud_buffer.buf_bridge.pWrite =
		(task_base->temp_work_buf.aud_buffer.buf_bridge.pWrite);
#ifdef DEBUG_VERBOSE
	dump_rbuf_bridge_s(__func__,
			   &task_base->temp_work_buf.aud_buffer.buf_bridge);
	dump_rbuf_bridge_s(__func__,
			   &task_base->share_hw_buf.aud_buffer.buf_bridge);
#endif

	spin_lock_irqsave(ringbuf_lock, flags);
	sync_ringbuf_writeidx(&task_base->ring_buf,
			      &task_base->share_hw_buf.aud_buffer.buf_bridge);
	spin_unlock_irqrestore(ringbuf_lock, flags);

#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &task_base->ring_buf);
#endif

	/* notify subsream */
	snd_pcm_period_elapsed(task_base->substream);
SCP_IRQ_HANDLER_ERR:
	return;
}

void scp_aud_ipi_handler(struct mtk_scp_audio_base *scp_aud,
		     struct ipi_msg_t *ipi_msg)

{
	int id = 0;

	if (!scp_aud) {
		pr_info("%s scp_aud NULL", __func__);
		return;
	}

	if (ipi_msg == NULL) {
		pr_info("%s ipi_msg == NULL\n", __func__);
		return;
	}

	id = get_daiid_by_scene(ipi_msg->task_scene);
	if (id < 0)
		return;

	if (!is_audio_task_dsp_ready(ipi_msg->task_scene)) {
		pr_info("%s(), is_scp_ready send false\n", __func__);
		return;
	}

	switch (ipi_msg->msg_id) {
	case AUDIO_DSP_TASK_IRQDL:
		mtk_scp_aud_dl_handler(scp_aud, ipi_msg, id);
		break;
	case AUDIO_DSP_TASK_IRQUL:
		mtk_scp_aud_ul_handler(scp_aud, ipi_msg, id);
		break;
	case AUDIO_DSP_TASK_DL_CONSUME_DATA:
		/* check exceptions in consume message */
		if (mtk_scp_aud_check_exception(scp_aud, ipi_msg, id))
			break;

		/* Handle consume message for the platforms
		 * which not support audio IRQ.
		 */
		mtk_scp_aud_dl_consume_handler(scp_aud, ipi_msg, id);
		break;
	default:
		break;
	}
}

void scp_audio_pcm_ipi_recv(struct ipi_msg_t *ipi_msg)
{
	struct mtk_scp_audio_base *scp_aud = get_scp_audio_base();

	if (ipi_msg == NULL) {
		pr_info("%s ipi_msg == NULL\n", __func__);
		return;
	}

	if (!is_audio_task_dsp_ready(ipi_msg->task_scene)) {
		pr_info("%s(), is_scp_ready send false\n", __func__);
		return;
	}

	if (scp_aud->ipi_ops.ipi_handler)
		scp_aud->ipi_ops.ipi_handler(scp_aud, ipi_msg);
}

static snd_pcm_uframes_t mtk_scp_audio_pcm_pointer_ul
			 (struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	int ptr_bytes;

	if (task_base == NULL) {
		pr_info("%s task_base == NULL\n", __func__);
		return 0;
	}
#ifdef DEBUG_VERBOSE
	dump_rbuf_s(__func__, &task_base->ring_buf);
#endif

	ptr_bytes = task_base->ring_buf.pWrite - task_base->ring_buf.pBufBase;

	return bytes_to_frames(substream->runtime, ptr_bytes);
}

static snd_pcm_uframes_t mtk_scp_audio_pcm_pointer_dl
			 (struct snd_pcm_substream *substream)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = asoc_rtd_to_cpu(rtd, 0);
	int id = cpu_dai->id;
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(id);
	int pcm_ptr_bytes, pcm_remap_ptr_bytes;
	spinlock_t *ringbuf_lock = &task_base->ringbuf_lock;
	unsigned long flags = 0;

	if (task_base == NULL) {
		pr_info("%s task_base == NULL\n", __func__);
		return 0;
	}
#ifdef DEBUG_VERBOSE
	dump_rbuf_s("-mtk_scphw_pcm_pointer_dl", &task_base->ring_buf);
#endif

	/* handle for underflow */
	if (task_base->underflowed)
		return -1;

	spin_lock_irqsave(ringbuf_lock, flags);
	pcm_ptr_bytes = (int)(task_base->ring_buf.pRead -
			      task_base->ring_buf.pBufBase);
	spin_unlock_irqrestore(ringbuf_lock, flags);
	pcm_remap_ptr_bytes =
		bytes_to_frames(substream->runtime, pcm_ptr_bytes);

	return pcm_remap_ptr_bytes;
}

static snd_pcm_uframes_t mtk_scp_audiohw_pcm_pointer
			 (struct snd_soc_component *component,
			  struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		return mtk_scp_audio_pcm_pointer_dl(substream);
	else
		return mtk_scp_audio_pcm_pointer_ul(substream);
}

static int scp_audio_mbox_recv_handler(unsigned int id,
				void *prdata,
				void *data,
				unsigned int len)
{
	int daiid;

	memcpy((void *)mbox_msg_temp, data, sizeof(struct mbox_msg));
	daiid = get_daiid_by_scene(mbox_msg_temp->scene_id);
	mtk_scp_aud_dl_consume_handler(get_scp_audio_base(), NULL, daiid);

	return 0;
}

static void scp_audio_tasklet_hdl(struct tasklet_struct *t)
{
	int daiid = get_daiid_by_scene(mbox_msg_temp->scene_id);
	struct scp_aud_task_base *task_base = get_taskbase_by_daiid(daiid);

	pr_debug("%s(), scene: %x, msgid: %x, read: %llx, work read: %llx",
		__func__,
		mbox_msg_temp->scene_id,
		mbox_msg_temp->msg_id,
		mbox_msg_temp->pRead,
		task_base->temp_work_buf.aud_buffer.buf_bridge.pRead);
	mtk_scp_aud_dl_consume_handler(get_scp_audio_base(), NULL, daiid);
}

static int mtk_scp_audio_probe(struct snd_soc_component *component)
{
	int ret = 0, id = 0;
	struct mtk_scp_audio_base *scp_audio = snd_soc_component_get_drvdata(component);
	struct scp_aud_task_base *task_base;

	pr_info("%s scp_audio = %p\n", __func__, scp_audio);
	//scp_audio_wakelock = aud_wake_lock_init(NULL, "scp_audio_wakelock");

	//if (scp_audio_wakelock == NULL)
		//pr_info("%s init scp_audio_wakelock error\n", __func__);

	ret = snd_soc_add_component_controls(component,
				      scp_audio_platform_kcontrols,
				      ARRAY_SIZE(scp_audio_platform_kcontrols));

	for (id = 0; id < SCP_AUD_TASK_DAI_NUM; id++) {
		task_base = get_taskbase_by_daiid(id);

		if (task_base == NULL) {
			pr_info("%s task_base NULL, id: %d\n", __func__, id);
			return -1;
		}
		spin_lock_init(&task_base->ringbuf_lock);
		ret = audio_task_register_callback(get_scene_by_daiid(id),
						   scp_audio_pcm_ipi_recv);
		if (ret < 0)
			return ret;
	}

	mbox_msg_temp = devm_kzalloc(component->dev, sizeof(struct mbox_msg),
				 GFP_KERNEL);
	if (!mbox_msg_temp)
		return -ENOMEM;

	mtk_ipi_register(&scp_ipidev, IPI_IN_RV_SPK_PROCESS,
			 (void *)scp_audio_mbox_recv_handler, NULL,
			 mbox_msg_temp);
	tasklet_setup(&scp_audio->tasklet, scp_audio_tasklet_hdl);

	if (ret)
		pr_info("%s add_component err ret = %d\n", __func__, ret);

	return ret;
}

const struct snd_soc_component_driver mtk_scp_audio_pcm_platform = {
	.name = AFE_SCP_AUDIO_NAME,
	.probe = mtk_scp_audio_probe,
	.open = mtk_scp_audio_pcm_open,
	.close = mtk_scp_audio_pcm_close,
	.hw_params = mtk_scp_audio_pcm_hw_params,
	.hw_free = mtk_scp_audio_pcm_hw_free,
	.prepare = mtk_scp_audio_pcm_hw_prepare,
	.trigger = mtk_scp_audio_pcm_hw_trigger,
	.pointer = mtk_scp_audiohw_pcm_pointer,
	.copy_user = mtk_scp_audio_pcm_copy,
};
EXPORT_SYMBOL_GPL(mtk_scp_audio_pcm_platform);

MODULE_DESCRIPTION("Mediatek scp audio pcm platform driver");
MODULE_AUTHOR("Zhixiong Wang<zhixiong.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");
