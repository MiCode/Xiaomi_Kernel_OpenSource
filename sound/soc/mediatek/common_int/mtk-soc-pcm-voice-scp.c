// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt-soc-pcm-voice-scp.c
 *
 * Project:
 * --------
 *    voice scp path
 *
 * Description:
 * ------------
 *   Audio mrgrx playback
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "audio_ipi_client_spkprotect.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-auddrv-scp-spkprotect-common.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/dma-mapping.h>

#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
#include "scp_helper.h"
#include <audio_ipi_client_spkprotect.h>
#include <audio_task_manager.h>
#endif

#define use_wake_lock
#ifdef use_wake_lock
static DEFINE_SPINLOCK(scp_voice_lock);
struct wakeup_source scp_voice_suspend_lock;
#endif

static unsigned int mscp_voice_PlaybackDramState;
static unsigned int mscp_voice_mdbackDramState;
static unsigned int scp_voice_mdback_user;
static unsigned int mscp_voice_FeedbackDramState;
static unsigned int scp_voice_feedback_user;

static int mscp_voice_iv_meminterface_type;
static bool mscp_voice_PrepareDone;
static int mscp_voice_md_select;
static const void *scp_voice_irq_user_id;
static unsigned int scp_voice_irq_cnt;
static int mscp_voice_hdoutput_control;
static struct device *mDev;

static struct snd_dma_buffer
	scp_voice_DramBuffer; /* scp pre allcoate dram buffer*/

static struct snd_dma_buffer scp_voice_mddl_dma_buf;
static struct snd_dma_buffer scp_voice_runtime_feedback_dma_buf;
/* real time for IV feedback buffer */
static struct snd_dma_buffer scp_voice_DL1Buffer;
struct SPK_PROTECT_SERVICE {
	bool ipiwait;
	bool ipiresult;
};

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static const int scpvoiceDL1BufferOffset = SCPDL1_MAX_BUFFER_SIZE;
static int scp_voice_Irq_mode = Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE;
#endif


/*  function implementation */
static int mtk_scp_voice_probe(struct platform_device *pdev);
static int mtk_pcm_voice_scp_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_voice_scp_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_voice_scp_component_probe(struct snd_soc_component *component);
static void
stop_scp_voice_i2s2adc2_hardware(struct snd_pcm_substream *substream);
static void
start_scp_voice_i2s2adc2_hardware(struct snd_pcm_substream *substream);
static void scp_voice_md2_enable(bool enable, struct snd_pcm_runtime *runtime);
static void scp_voice_md1_enable(bool enable, struct snd_pcm_runtime *runtime);

#ifdef use_wake_lock
static void scp_voice_int_wakelock(bool enable)
{
	spin_lock(&scp_voice_lock);
	if (enable == true)
		aud_wake_lock(&scp_voice_suspend_lock);
	else
		aud_wake_unlock(&scp_voice_suspend_lock);

	spin_unlock(&scp_voice_lock);
}
#endif

static const char *const scp_voice_hd_output[] = {"Off", "On"};
static const struct soc_enum audio_scp_voice_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(scp_voice_hd_output),
		scp_voice_hd_output),
};
static const char * const voice_scpspk_pcmdump[] = {"Off", "On"};
static const struct soc_enum audio_scp_voice_pcmdump_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(voice_scpspk_pcmdump),
		voice_scpspk_pcmdump),
};

static int audio_scp_voice_hdoutput_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpR_Get = %d\n", mscp_voice_hdoutput_control);
	ucontrol->value.integer.value[0] = mscp_voice_hdoutput_control;
	return 0;
}

static int audio_scp_voice_hdoutput_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(scp_voice_hd_output)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}

	mscp_voice_hdoutput_control = ucontrol->value.integer.value[0];

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true) {
		pr_debug("return HDMI enabled\n");
		return 0;
	}
	return 0;
}

static const char *const scp_md_choose[] = {"md1", "md2"};
static const struct soc_enum speech_scp_voice_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(scp_md_choose), scp_md_choose),
};

static int Audio_Voice_Scp_MD_Get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s = %d\n", __func__, mscp_voice_md_select);
	ucontrol->value.integer.value[0] = mscp_voice_md_select;
	return 0;
}

static int Audio_Voice_Scp_MD_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mscp_voice_md_select = ucontrol->value.integer.value[0];
	return 0;
}

static int mScp_Voice_Debug;
static int Audio_Scp_Debug_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_Wcn_Cmb_Get = %d\n", mScp_Voice_Debug);
	ucontrol->value.integer.value[0] = mScp_Voice_Debug;
	return 0;
}

static int Audio_ScpSpk_Debug_Set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	mScp_Voice_Debug = ucontrol->value.integer.value[0];
	pr_debug("%s mAudio_Wcn_Cmb = 0x%x\n", __func__, mScp_Voice_Debug);
	return 0;
}

static int audio_scp_voice_Irqcnt_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	AudDrv_Clk_On();
	ucontrol->value.integer.value[0] = Afe_Get_Reg(AFE_IRQ_MCU_CNT7);
	AudDrv_Clk_Off();
	return 0;
}

static int audio_scp_voice_Irqcnt_Set(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), irq_user_id = %p, irq1_cnt = %d, value = %ld\n",
		 __func__, scp_voice_irq_user_id, scp_voice_irq_cnt,
		 ucontrol->value.integer.value[0]);

	if (scp_voice_irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	scp_voice_irq_cnt = ucontrol->value.integer.value[0];

	AudDrv_Clk_On();
	if (scp_voice_irq_user_id && scp_voice_irq_cnt)
		irq_update_user(scp_voice_irq_user_id,
				Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE, 0,
				scp_voice_irq_cnt);
	else
		pr_notice(
			"warn, cannot update irq counter, user_id = %p, irq1_cnt = %d\n",
			scp_voice_irq_user_id, scp_voice_irq_cnt);

	AudDrv_Clk_Off();
	return 0;
}

static const struct snd_kcontrol_new Audio_Scp_voice_controls[] = {
	SOC_SINGLE_EXT("Audio_Scp_Voice_Debug_Info", SND_SOC_NOPM, 0, 0x80000,
		       0, Audio_Scp_Debug_Get, Audio_ScpSpk_Debug_Set),
	SOC_ENUM_EXT("Audio_Scp_Voice_MD_Select", speech_scp_voice_enum[0],
		     Audio_Voice_Scp_MD_Get, Audio_Voice_Scp_MD_Set),
	SOC_ENUM_EXT("Audio_scp_voice_hd_Switch", audio_scp_voice_Enum[0],
		     audio_scp_voice_hdoutput_get,
		     audio_scp_voice_hdoutput_set),
	SOC_SINGLE_EXT("Scp_Voice_Irq_Cnt", SND_SOC_NOPM, 0, IRQ_MAX_RATE, 0,
		       audio_scp_voice_Irqcnt_Get, audio_scp_voice_Irqcnt_Set),
};

#define SPK_IPIMSG_TIMEOUT            50
#define SPK_WAITCHECK_INTERVAL_MS      (2)

static struct snd_pcm_hardware mtk_scp_voice_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_NORMAL_USE_RATE,
	.rate_min = SOC_NORMAL_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_HIGH_USE_CHANNELS_MIN,
	.channels_max = SOC_HIGH_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = 1,
	.periods_max = 4096,
	.fifo_size = 0,
};

static int mtk_pcm_scp_voice_stop(struct snd_pcm_substream *substream)
{
	scp_voice_irq_user_id = NULL;

	pr_debug("%s\n", __func__);
	irq_remove_user(substream, scp_voice_Irq_mode);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, false);
	stop_scp_voice_i2s2adc2_hardware(substream);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S3);

	if (mscp_voice_md_select)
		scp_voice_md2_enable(false, substream->runtime);
	else
		scp_voice_md1_enable(false, substream->runtime);

	EnableAfe(false);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);

#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY,
				    AUDIO_IPI_MSG_DIRECT_SEND,
				    SPK_PROTECT_SPEECH_STOP, 1, 0, NULL);
#endif

	return 0;
}

static unsigned int Previous_Hw_cur;
static snd_pcm_uframes_t
mtk_pcm_scp_voice_pointer(struct snd_pcm_substream *substream)
{
	snd_pcm_uframes_t return_frames;

	return_frames = (Previous_Hw_cur >> 2);
	return return_frames;
}

static int scp_voice_get_scpdram_buffer(void)
{
	struct scp_spk_reserved_mem_t *reserved_mem;

	reserved_mem = get_scp_spk_reserved_mem();
	scp_voice_DramBuffer.addr = reserved_mem->phy_addr;
	scp_voice_DramBuffer.area = (kal_uint8 *)reserved_mem->vir_addr;
	scp_voice_DramBuffer.bytes = reserved_mem->size;
	pr_debug(
		 "%s scp_voice_DramBuffer.addr = %llx scp_voice_DramBuffer.area = %p bytes = %zu\n",
		 __func__, scp_voice_DramBuffer.addr, scp_voice_DramBuffer.area,
		 scp_voice_DramBuffer.bytes);
	return 0;
}

/* dm data with AWB*/
static int scp_voice_allocate_mddl_buffer(struct snd_pcm_substream *substream,
					  struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	unsigned int buffer_size = 0;

	buffer_size = params_buffer_bytes(hw_params);
	scp_voice_mddl_dma_buf.bytes = buffer_size;
	if (AllocateAudioSram(&scp_voice_mddl_dma_buf.addr,
			       &scp_voice_mddl_dma_buf.area,
			       scp_voice_mddl_dma_buf.bytes,
			       (void *)&scp_voice_mdback_user,
			       params_format(hw_params), false) == 0)
		SetHighAddr(Soc_Aud_Digital_Block_MEM_AWB, false,
			    scp_voice_mddl_dma_buf.addr);
	else {
		scp_voice_mddl_dma_buf.addr = scp_voice_DramBuffer.addr;
		scp_voice_mddl_dma_buf.area =
			(unsigned char *)scp_voice_DramBuffer.area;
		scp_voice_mddl_dma_buf.bytes = buffer_size;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_AWB, true,
			    scp_voice_mddl_dma_buf.addr);
		mscp_voice_mdbackDramState = true;
		AudDrv_Emi_Clk_On();
	}
	set_memif_addr(Soc_Aud_Digital_Block_MEM_AWB,
		       scp_voice_mddl_dma_buf.addr,
		       scp_voice_mddl_dma_buf.bytes);
	pr_debug(
		"%s addr = %llx area = %p bytes  = %zu mscp_voice_mdbackDramState = %u\n",
		__func__, scp_voice_mddl_dma_buf.addr,
		scp_voice_mddl_dma_buf.area, scp_voice_mddl_dma_buf.bytes,
		mscp_voice_mdbackDramState);
	return ret;

	return 0;
}

/* spk iv feedback data*/
static int
scp_voice_allocate_feedback_buffer(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	unsigned int buffer_size = 0;

	buffer_size = params_buffer_bytes(hw_params);
	scp_voice_runtime_feedback_dma_buf.bytes = buffer_size;
	if (AllocateAudioSram(&scp_voice_runtime_feedback_dma_buf.addr,
			       &scp_voice_runtime_feedback_dma_buf.area,
			       scp_voice_runtime_feedback_dma_buf.bytes,
			       (void *)&scp_voice_feedback_user,
			       params_format(hw_params), false) == 0)
		SetHighAddr(mscp_voice_iv_meminterface_type, false,
			    scp_voice_runtime_feedback_dma_buf.addr);
	else {
		scp_voice_runtime_feedback_dma_buf.addr =
			scp_voice_DramBuffer.addr + scpvoiceDL1BufferOffset;
		scp_voice_runtime_feedback_dma_buf.area =
			(unsigned char *)scp_voice_DramBuffer.area +
			buffer_size;
		scp_voice_runtime_feedback_dma_buf.bytes = buffer_size;
		SetHighAddr(mscp_voice_iv_meminterface_type, true,
			    scp_voice_runtime_feedback_dma_buf.addr);
		mscp_voice_FeedbackDramState = true;
		AudDrv_Emi_Clk_On();
	}
	set_memif_addr(mscp_voice_iv_meminterface_type,
		       scp_voice_runtime_feedback_dma_buf.addr,
		       scp_voice_runtime_feedback_dma_buf.bytes);
	pr_debug(
		"%s addr = %llx area = %p bytes  = %zu mscp_voice_FeedbackDramState = %u\n",
		__func__, scp_voice_runtime_feedback_dma_buf.addr,
		scp_voice_runtime_feedback_dma_buf.area,
		scp_voice_runtime_feedback_dma_buf.bytes,
		mscp_voice_FeedbackDramState);
	return ret;
}

/* DL data can use Sram or Dram*/
static int
scp_voice_allocate_platformdl_buffer(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	unsigned int buffer_size = params_buffer_bytes(hw_params);

	scp_voice_DL1Buffer.bytes = buffer_size;
	if (buffer_size <= GetPLaybackSramFullSize() &&
	    AllocateAudioSram(&scp_voice_DL1Buffer.addr,
			      &scp_voice_DL1Buffer.area,
			      scp_voice_DL1Buffer.bytes,
			      substream, params_format(hw_params),
			      false) == 0) {
		AudDrv_Allocate_DL1_Buffer(mDev,
					   scp_voice_DL1Buffer.bytes,
					   scp_voice_DL1Buffer.addr,
					   scp_voice_DL1Buffer.area);
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, false,
			    scp_voice_DL1Buffer.addr);
	} else {
		scp_voice_DL1Buffer.addr =
			scp_voice_DramBuffer.addr + scpvoiceDL1BufferOffset * 2;
		scp_voice_DL1Buffer.area =
			(unsigned char *)scp_voice_DramBuffer.area +
			buffer_size;
		scp_voice_DL1Buffer.bytes = buffer_size;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, true,
			    scp_voice_DL1Buffer.addr);
		mscp_voice_PlaybackDramState = true;
		AudDrv_Emi_Clk_On();
	}
	set_memif_addr(Soc_Aud_Digital_Block_MEM_DL1, scp_voice_DL1Buffer.addr,
		       scp_voice_DL1Buffer.bytes);
	memset_io(scp_voice_DL1Buffer.area, 0, scp_voice_DL1Buffer.bytes);
	pr_debug(
		"%s scp_voice_DL1Buffer.addr = %llx scp_voice_DL1Buffer.area = %p bytes = %zu\n",
		__func__, scp_voice_DL1Buffer.addr, scp_voice_DL1Buffer.area,
		scp_voice_DL1Buffer.bytes);
	return ret;
}

static int mtk_pcm_scp_voice_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
	unsigned int payloadlen = 0;

	audio_task_register_callback(TASK_SCENE_SPEAKER_PROTECTION,
				     spkproc_service_ipicmd_received, NULL);

	scp_voice_get_scpdram_buffer();
	scp_voice_allocate_mddl_buffer(substream, hw_params);
	scp_voice_allocate_feedback_buffer(substream, hw_params);
	scp_voice_allocate_platformdl_buffer(substream, hw_params);


	payloadlen = spkproc_ipi_pack_payload(
		SPK_PROTECT_SPEECH_MDFEEDBACKPARAM,
		mscp_voice_mdbackDramState, Soc_Aud_Digital_Block_MEM_AWB,
		&scp_voice_mddl_dma_buf, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_SPEECH_MDFEEDBACKPARAM,
				    payloadlen, 0,
				    (char *)spkproc_ipi_get_payload());

	payloadlen =
		spkproc_ipi_pack_payload(SPK_PROTECT_SPEECH_DLMEMPARAM,
					 mscp_voice_PlaybackDramState,
					 Soc_Aud_Digital_Block_MEM_DL1,
					 &scp_voice_DL1Buffer, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_SPEECH_DLMEMPARAM,
				    payloadlen, 0,
				    (char *)spkproc_ipi_get_payload());

	payloadlen =
		spkproc_ipi_pack_payload(SPK_PROTECT_SPEECH_IVMEMPARAM,
					 mscp_voice_PlaybackDramState,
					 mscp_voice_iv_meminterface_type,
					 &scp_voice_runtime_feedback_dma_buf,
					 substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_SPEECH_IVMEMPARAM,
				    payloadlen, 0,
				    (char *)spkproc_ipi_get_payload());
#endif
	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		 substream->runtime->dma_bytes, substream->runtime->dma_area,
		 (long)substream->runtime->dma_addr);

	return ret;
}

static int mtk_pcm_scp_voice_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s substream = %p\n", __func__, substream);
	if (mscp_voice_PlaybackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mscp_voice_PlaybackDramState = false;
	} else {
		freeAudioSram((void *)substream);
	}

	if (mscp_voice_FeedbackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mscp_voice_FeedbackDramState = false;
	} else {
		freeAudioSram((void *)&scp_voice_feedback_user);
	}

	if (mscp_voice_mdbackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mscp_voice_mdbackDramState = false;
	} else {
		freeAudioSram((void *)&scp_voice_mdback_user);
	}

	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_voice_supported_sample_rates),
	.list = soc_voice_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_scp_voice_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	pr_debug("+%s\n", __func__);
	mscp_voice_PlaybackDramState = false;

	scp_reset_check();

	runtime->hw = mtk_scp_voice_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_scp_voice_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);
	ret = snd_pcm_hw_constraint_integer(runtime,
					    SNDRV_PCM_HW_PARAM_PERIODS);

	pr_debug(
		"%s runtime rate = %d channels = %d substream->pcm->device = %d\n",
		__func__, runtime->rate, runtime->channels,
		substream->pcm->device);

	runtime->hw.info |= SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.info |= SNDRV_PCM_INFO_NONINTERLEAVED;

	if (ret < 0) {
		pr_warn("mtk_pcm_voice_scp_close\n");
		mtk_pcm_voice_scp_close(substream);
		return ret;
	}

	AudDrv_Clk_On();
	mscp_voice_iv_meminterface_type =
		get_usage_digital_block(AUDIO_USAGE_SCP_SPK_IV_DATA);
	if (mscp_voice_iv_meminterface_type < 0) {
		pr_info("%s get_pcm_mem_id err using VUL_Data2 as default\n",
			__func__);
		mscp_voice_iv_meminterface_type =
			Soc_Aud_Digital_Block_MEM_VUL_DATA2;
	}

#ifdef use_wake_lock
	scp_voice_int_wakelock(true);
#endif

	scp_register_feature(SPEAKER_PROTECT_FEATURE_ID);

#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_SPEECH_OPEN,
				    Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
				    0, NULL);
#endif

	pr_debug("%s return\n", __func__);

	return 0;
}

static int mtk_pcm_voice_scp_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	if (mscp_voice_PrepareDone == true) {
		if (mscp_voice_hdoutput_control == true) {
			pr_debug("%s mscp_voice_hdoutput_control == %d\n",
				 __func__, mscp_voice_hdoutput_control);
			EnableI2SCLKDiv(Soc_Aud_I2S1_MCKDIV, false);
			EnableI2SCLKDiv(Soc_Aud_I2S3_MCKDIV, false);
		}
		set_memif_pbuf_size(Soc_Aud_Digital_Block_MEM_DL1,
				    MEMIF_PBUF_SIZE_256_BYTES);
		mscp_voice_PrepareDone = false;
	}

#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_SPEECH_CLOSE, 1, 0, NULL);
#endif

	scp_deregister_feature(SPEAKER_PROTECT_FEATURE_ID);

#ifdef use_wake_lock
	scp_voice_int_wakelock(false);
#endif

	scp_voice_irq_cnt = 0; /* reset scp_voice_irq_cnt */
	AudDrv_Clk_Off();

	return 0;
}

static void scp_voice_md1_enable(bool enable, struct snd_pcm_runtime *runtime)
{
	if (enable) {

		/* dis connecnect */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC);
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_I2S3);
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_I2S2,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH4);

		/* connect */
		/* pcm2 i14i21 --> vul o5o6*/
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MEM_DL1_CH1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH4);

	} else {
		/* i3i4 -> pcm2 o17o28 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MEM_DL1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O);

		/* pcm2 i14 --> awb o5o6 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MEM_DL1_CH1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_2_O_CH4);
	}
}

static void scp_voice_md2_enable(bool enable, struct snd_pcm_runtime *runtime)
{
	if (enable) {
		/* dis connecnect */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC);
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_I2S3);
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_I2S2,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O_CH4);

		/* pcm2 i14i21 --> vul o5o6*/
		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		SetIntfConnection(Soc_Aud_InterCon_Connection,
				  Soc_Aud_AFE_IO_Block_MEM_DL1_CH1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O_CH4);

	} else {
		/* i3i4 -> pcm2 o17o28 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MEM_DL1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O);

		/* pcm2 i14 --> awb o5o6 */
		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_I_CH1,
				  Soc_Aud_AFE_IO_Block_MEM_AWB);

		SetIntfConnection(Soc_Aud_InterCon_DisConnect,
				  Soc_Aud_AFE_IO_Block_MEM_DL1_CH1,
				  Soc_Aud_AFE_IO_Block_MODEM_PCM_1_O_CH4);
	}
}

static int mtk_pcm_scp_voice_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int payloadlen = 0;

	pr_debug("%s rate = %d\n", __func__, runtime->rate);

	if (mscp_voice_PrepareDone == false) {
		/* set memif format */
		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_AWB,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_AWB2,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			if (mscp_voice_iv_meminterface_type ==
			    Soc_Aud_Digital_Block_MEM_AWB2)
				SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					Soc_Aud_AFE_IO_Block_MEM_AWB2);
			else
				SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2);
			SetMemIfFetchFormatPerSample(
				mscp_voice_iv_meminterface_type,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S3);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

		} else {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_AWB, AFE_WLEN_16_BIT);
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_AWB2,
				AFE_WLEN_16_BIT);
			if (mscp_voice_iv_meminterface_type ==
			    Soc_Aud_Digital_Block_MEM_AWB2)
				SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					Soc_Aud_AFE_IO_Block_MEM_AWB2);
			else
				SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2);
			SetMemIfFetchFormatPerSample(
				mscp_voice_iv_meminterface_type,
				AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S3);

			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
		}
		set_memif_pbuf_size(Soc_Aud_Digital_Block_MEM_DL1,
				    MEMIF_PBUF_SIZE_32_BYTES);

		if (mscp_voice_hdoutput_control == true) {
			pr_debug("%s mscp_voice_hdoutput_control == %d\n",
				 __func__, mscp_voice_hdoutput_control);
			SetCLkMclk(Soc_Aud_I2S1,
				   runtime->rate); /* select I2S */
			SetCLkMclk(Soc_Aud_I2S3, runtime->rate);
			EnableI2SCLKDiv(Soc_Aud_I2S1_MCKDIV, true);
			EnableI2SCLKDiv(Soc_Aud_I2S3_MCKDIV, true);
		}

		if (mscp_voice_md_select)
			scp_voice_md2_enable(true, runtime);
		else
			scp_voice_md1_enable(true, runtime);

		mscp_voice_PrepareDone = true;
	}

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	payloadlen = spkproc_ipi_pack_payload(SPK_PROTECT_SPEECH_PREPARE, 0, 0,
					      NULL, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_BYPASS_ACK,
				    SPK_PROTECT_SPEECH_PREPARE, payloadlen, 0,
				    (char *)spkproc_ipi_get_payload());
#endif
	return 0;
}

static void
stop_scp_voice_i2s2adc2_hardware(struct snd_pcm_substream *substream)
{
	SetMemoryPathEnable(mscp_voice_iv_meminterface_type, false);
}

static void
start_scp_voice_i2s2adc2_hardware(struct snd_pcm_substream *substream)
{

	SetSampleRate(mscp_voice_iv_meminterface_type,
		      substream->runtime->rate);
	SetChannels(mscp_voice_iv_meminterface_type,
		    substream->runtime->channels);
	SetMemoryPathEnable(mscp_voice_iv_meminterface_type, true);
}

static int mtk_pcm_scp_voice_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	pr_debug("%s\n", __func__);

	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
	SetIntfConnection(Soc_Aud_InterCon_Connection,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S3);

#ifdef CONFIG_MTK_AUDIO_SCP_SPKPROTECT_SUPPORT
	spkproc_service_ipicmd_send(
			AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_DIRECT_SEND,
			SPK_PROTECT_SPEECH_START, 1, 0, NULL);
#endif

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	SetSampleRate(Soc_Aud_Digital_Block_MEM_AWB, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_AWB, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_AWB, true);

	start_scp_voice_i2s2adc2_hardware(substream);

	/* here to set interrupt */
	irq_add_user(substream, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
		     substream->runtime->rate,
		     scp_voice_irq_cnt ? scp_voice_irq_cnt
				       : substream->runtime->period_size);

	scp_voice_irq_user_id = substream;

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_scp_voice_trigger(struct snd_pcm_substream *substream,
				     int cmd)
{
	pr_debug("%s cmd = %d\n", __func__, cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_scp_voice_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_scp_voice_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_scp_voice_copy(struct snd_pcm_substream *substream,
				  int channel,
				  unsigned long pos,
				  void __user *buf,
				  unsigned long bytes)
{
	count = bytes;
	return count;
}

static struct snd_pcm_ops mtk_scp_voice_ops = {
	.open = mtk_pcm_scp_voice_open,
	.close = mtk_pcm_voice_scp_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_scp_voice_hw_params,
	.hw_free = mtk_pcm_scp_voice_hw_free,
	.prepare = mtk_pcm_scp_voice_prepare,
	.trigger = mtk_pcm_scp_voice_trigger,
	.pointer = mtk_pcm_scp_voice_pointer,
	.copy_user = mtk_pcm_scp_voice_copy,
};

static struct snd_soc_component_driver mtk_scp_voice_soc_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_scp_voice_ops,
	.pcm_new = mtk_asoc_pcm_voice_scp_new,
	.probe = mtk_afe_voice_scp_component_probe,
};

static int mtk_scp_voice_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_SCP_VOICE_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	mDev = &pdev->dev;

	return snd_soc_register_component(&pdev->dev,
					  &mtk_scp_voice_soc_component,
					  NULL,
					  0);
}

static int mtk_asoc_pcm_voice_scp_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}

static int mtk_afe_voice_scp_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s\n", __func__);
	snd_soc_add_platform_controls(component, Audio_Scp_voice_controls,
				      ARRAY_SIZE(Audio_Scp_voice_controls));
#ifdef use_wake_lock
	aud_wake_lock_init(&scp_voice_suspend_lock, "scpvoice lock");
#endif
	return 0;
}

static int mtk_scp_voice_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
#ifdef use_wake_lock
	aud_wake_lock_destroy(&scp_voice_suspend_lock);
#endif
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_scp_voice_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_voice_scp",
	},
	{} };
#endif

static struct platform_driver mtk_scp_voice_driver = {
	.driver = {

			.name = MT_SOC_SCP_VOICE_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_scp_voice_of_ids,
#endif
		},
	.probe = mtk_scp_voice_probe,
	.remove = mtk_scp_voice_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_scp_voice_dev;
#endif

static int __init mtk_scp_voice_soc_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);

#ifndef CONFIG_OF
	soc_mtk_scp_voice_dev = platform_device_alloc(MT_SOC_SCP_VOICE_PCM, -1);
	if (!soc_mtk_scp_voice_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_scp_voice_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_scp_voice_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_scp_voice_driver);
	return ret;
}
module_init(mtk_scp_voice_soc_platform_init);

static void __exit mtk_scp_voice_soc_platform_exit(void)
{
	platform_driver_unregister(&mtk_scp_voice_driver);
}
module_exit(mtk_scp_voice_soc_platform_exit);

MODULE_DESCRIPTION("scp voice module platform driver");
MODULE_LICENSE("GPL");
