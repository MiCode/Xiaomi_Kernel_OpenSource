/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
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

#include "audio_dma_buf_control.h"
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

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
#include "scp_helper.h"
#include <audio_dma_buf_control.h>
#include <audio_ipi_client_spkprotect.h>
#include <audio_task_manager.h>
#endif

#define use_wake_lock
#ifdef use_wake_lock
static DEFINE_SPINLOCK(scp_voice_lock);
struct wakeup_source scp_voice_suspend_lock;
#endif

#define VOICE_MAX_PARLOAD_SIZE (10)
#define DEFAULT_VOICE_PAYLOAD_SIZE (32)
static const int scp_voice_buf_size = 16 * 1024;

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

static struct SPK_PROTECT_SERVICE scp_voice_protect_service;
static struct audio_resv_dram_t *p_scp_voice_resv_dram;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static struct audio_resv_dram_t *p_scp_voice_resv_dram_normal;
static struct scp_reserve_mblock scp_voiceReserveBuffer;
static struct snd_dma_buffer PlatformBuffer;
static const int scpvoiceDL1BufferOffset = SOC_NORMAL_USE_BUFFERSIZE_MAX;
static int scp_voice_Irq_mode = Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE;
static uint32_t scp_voice_ipi_payload_buf[VOICE_MAX_PARLOAD_SIZE];
#endif

struct SPK_PROTECT_SERVICE {
	bool ipiwait;
	bool ipiresult;
};
/*  function implementation */
static int mtk_scp_voice_probe(struct platform_device *pdev);
static int mtk_pcm_voice_scp_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_voice_scp_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_voice_scp_probe(struct snd_soc_platform *platform);
static void
stop_scp_voice_i2s2adc2_hardware(struct snd_pcm_substream *substream);
static void
start_scp_voice_i2s2adc2_hardware(struct snd_pcm_substream *substream);
static void scp_voice_md2_enable(bool enable, struct snd_pcm_runtime *runtime);
static void scp_voice_md1_enable(bool enable, struct snd_pcm_runtime *runtime);
static int voice_scp_pcm_dump_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);
static int voice_scp_pcm_dump_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

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
static bool scpvoice_pcmdump;

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
	SOC_ENUM_EXT("Audio_scp_voice_dump", audio_scp_voice_pcmdump_enum[0],
		     voice_scp_pcm_dump_get, voice_scp_pcm_dump_set),
};

#define SPK_IPIMSG_TIMEOUT            50
#define SPK_WAITCHECK_INTERVAL_MS      (2)
static bool scp_voice_service_ipicmd_wait(int id)
{
	int timeout = 0;

	while (scp_voice_protect_service.ipiwait) {
		msleep(SPK_WAITCHECK_INTERVAL_MS);
		if (timeout++ >= SPK_IPIMSG_TIMEOUT) {
			/* pr_debug("Error: IPI MSG timeout:id_%x\n", id); */
			scp_voice_protect_service.ipiwait = false;
			return false;
		}
	}
	return true;
}

static int voice_scp_pcm_dump_set(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), value = %ld, scpvoice_pcmdump = %d\n",
		 __func__,
		 ucontrol->value.integer.value[0],
		 scpvoice_pcmdump);

	if (scpvoice_pcmdump == false &&
	    ucontrol->value.integer.value[0] == true) {
		scpvoice_pcmdump = true;
		AudDrv_Emi_Clk_On();
		spkprotect_open_dump_file();
		spkproc_service_ipicmd_send(AUDIO_IPI_DMA,
					    AUDIO_IPI_MSG_BYPASS_ACK,
					    SPK_PROTTCT_PCMDUMP_ON,
					    p_scp_voice_resv_dram->size,
					    scpvoice_pcmdump,
					    p_scp_voice_resv_dram->phy_addr);
		scp_voice_protect_service.ipiwait = true;
	} else if (scpvoice_pcmdump == true &&
	    ucontrol->value.integer.value[0] == false) {
		scpvoice_pcmdump = false;
		scp_voice_service_ipicmd_wait(SPK_PROTECT_PCMDUMP_OK);
		spkprotect_close_dump_file();
		spkproc_service_ipicmd_send(AUDIO_IPI_DMA,
					    AUDIO_IPI_MSG_BYPASS_ACK,
					    SPK_PROTTCT_PCMDUMP_OFF,
					    p_scp_voice_resv_dram->size,
					    scpvoice_pcmdump,
					    p_scp_voice_resv_dram->phy_addr);
		AudDrv_Emi_Clk_Off();
	}
	return 0;
}

static int voice_scp_pcm_dump_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
		ARRAY_SIZE(voice_scpspk_pcmdump)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = scpvoice_pcmdump;
	return 0;
}
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

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	if (!in_interrupt())
		spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY,
					   AUDIO_IPI_MSG_BYPASS_ACK,
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
	memset(&scp_voiceReserveBuffer, 0, sizeof(scp_voiceReserveBuffer));
	scp_voiceReserveBuffer.num = SPK_PROTECT_MEM_ID;
	p_scp_voice_resv_dram_normal = get_reserved_dram();
	scp_voiceReserveBuffer.start_phys =
		scp_get_reserve_mem_phys(scp_voiceReserveBuffer.num);
	scp_voiceReserveBuffer.start_virt =
		scp_get_reserve_mem_virt(scp_voiceReserveBuffer.num);
	scp_voiceReserveBuffer.size =
		scp_get_reserve_mem_size(scp_voiceReserveBuffer.num);
	scp_voice_DramBuffer.addr = scp_voiceReserveBuffer.start_phys;
	scp_voice_DramBuffer.area =
		(kal_uint8 *)scp_voiceReserveBuffer.start_virt;
	scp_voice_DramBuffer.bytes = scp_voiceReserveBuffer.size;
	pr_debug(
		"%s scp_voice_DramBuffer.addr = %llx scp_voice_DramBuffer.area = %p bytes = %zu",
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

/* platform use Dram*/
static int
scp_voice_allocate_platform_buffer(struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	unsigned int buffer_size = 0;

	buffer_size = params_buffer_bytes(hw_params);
	PlatformBuffer.addr = scp_voice_DramBuffer.addr;
	PlatformBuffer.area = scp_voice_DramBuffer.area;
	PlatformBuffer.bytes = buffer_size;
	substream->runtime->dma_area = PlatformBuffer.area;
	substream->runtime->dma_addr = PlatformBuffer.addr;
	substream->runtime->dma_bytes = PlatformBuffer.bytes;
	pr_debug("%s PlatformBuffer.addr = %llx PlatformBuffer.area = %p bytes  = %zu\n",
		 __func__, PlatformBuffer.addr, PlatformBuffer.area,
		 PlatformBuffer.bytes);
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
		"%s scp_voice_DL1Buffer.addr = %llx scp_voice_DL1Buffer.area = %p bytes  = %zu\n",
		__func__, scp_voice_DL1Buffer.addr, scp_voice_DL1Buffer.area,
		scp_voice_DL1Buffer.bytes);
	return ret;
}

/* return for payload length*/
static unsigned int
scp_voice_packIpi_payload(uint16_t msg_id, uint32_t param1, uint32_t param2,
			  struct snd_dma_buffer *bmd_buffer,
			  struct snd_pcm_substream *substream)
{
	unsigned int ret = 0;
	/* clean payload data */
	memset_io((void *)scp_voice_ipi_payload_buf, 0,
		  sizeof(uint32_t) * DEFAULT_VOICE_PAYLOAD_SIZE);
	switch (msg_id) {
	case SPK_PROTECT_SPEECH_MDFEEDBACKPARAM:
		scp_voice_ipi_payload_buf[0] = (kal_uint32)(bmd_buffer->addr);
		scp_voice_ipi_payload_buf[1] = (kal_uint32)(*bmd_buffer->area);
		scp_voice_ipi_payload_buf[2] = bmd_buffer->bytes;
		scp_voice_ipi_payload_buf[3] = mscp_voice_mdbackDramState;
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_SPEECH_DLMEMPARAM:
		scp_voice_ipi_payload_buf[0] = (kal_uint32)bmd_buffer->addr;
		scp_voice_ipi_payload_buf[1] = (kal_uint32)(*bmd_buffer->area);
		scp_voice_ipi_payload_buf[2] = bmd_buffer->bytes;
		scp_voice_ipi_payload_buf[3] = mscp_voice_PlaybackDramState;
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_SPEECH_PREPARE:
		if (substream->runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    substream->runtime->format == SNDRV_PCM_FORMAT_U32_LE)
			scp_voice_ipi_payload_buf[0] =
				Soc_Aud_I2S_WLEN_WLEN_32BITS;
		else
			scp_voice_ipi_payload_buf[0] =
				Soc_Aud_I2S_WLEN_WLEN_16BITS;

		scp_voice_ipi_payload_buf[1] =
			(kal_uint32)substream->runtime->rate;
		scp_voice_ipi_payload_buf[2] =
			(kal_uint32)substream->runtime->channels;
		scp_voice_ipi_payload_buf[3] =
			(kal_uint32)substream->runtime->period_size;
		ret = sizeof(unsigned int) * 4;
		break;
	case SPK_PROTECT_SPEECH_IVMEMPARAM:
		scp_voice_ipi_payload_buf[0] = (kal_uint32)bmd_buffer->addr;
		scp_voice_ipi_payload_buf[1] = (kal_uint32)(*bmd_buffer->area);
		scp_voice_ipi_payload_buf[2] = bmd_buffer->bytes;
		scp_voice_ipi_payload_buf[3] = (mscp_voice_FeedbackDramState);
		scp_voice_ipi_payload_buf[4] =
			(mscp_voice_iv_meminterface_type);
		ret = sizeof(unsigned int) * 5;
		break;
	default:
		pr_debug("%s param1=%d\n", __func__, param1);
		break;
	}
	return ret;
}

static int mtk_pcm_scp_voice_hw_params(struct snd_pcm_substream *substream,
				       struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	unsigned int payloadlen = 0;

	audio_task_register_callback(TASK_SCENE_SPEAKER_PROTECTION,
				     spkproc_service_ipicmd_received, NULL);

	scp_voice_get_scpdram_buffer();
	scp_voice_allocate_mddl_buffer(substream, hw_params);
	scp_voice_allocate_platform_buffer(substream, hw_params);
	scp_voice_allocate_feedback_buffer(substream, hw_params);
	scp_voice_allocate_platformdl_buffer(substream, hw_params);

	payloadlen = scp_voice_packIpi_payload(
		SPK_PROTECT_SPEECH_MDFEEDBACKPARAM, 0, 0,
		&scp_voice_mddl_dma_buf, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				   SPK_PROTECT_SPEECH_MDFEEDBACKPARAM,
				   payloadlen, 0,
				   (char *)scp_voice_ipi_payload_buf);

	payloadlen =
		scp_voice_packIpi_payload(SPK_PROTECT_SPEECH_DLMEMPARAM, 0, 0,
					  &scp_voice_DL1Buffer, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				   SPK_PROTECT_SPEECH_DLMEMPARAM, payloadlen, 0,
				   (char *)scp_voice_ipi_payload_buf);

	payloadlen =
		scp_voice_packIpi_payload(SPK_PROTECT_SPEECH_IVMEMPARAM, 0, 0,
					  &scp_voice_runtime_feedback_dma_buf,
					  substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_SPEECH_IVMEMPARAM,
				    payloadlen, 0,
				    (char *)scp_voice_ipi_payload_buf);

	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		 __func__, substream->runtime->dma_bytes,
		 substream->runtime->dma_area,
		 (long)substream->runtime->dma_addr);

#else
	pr_debug("dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		 substream->runtime->dma_bytes, substream->runtime->dma_area,
		 (long)substream->runtime->dma_addr);
#endif

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
	p_scp_voice_resv_dram = get_reserved_dram();
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
		pr_warn("pcm_voice_scp_close\n");
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

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
				  SPK_PROTECT_SPEECH_OPEN, 1, 0, NULL);
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

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
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
	payloadlen = scp_voice_packIpi_payload(SPK_PROTECT_SPEECH_PREPARE, 0, 0,
					       NULL, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_BYPASS_ACK,
				  SPK_PROTECT_SPEECH_PREPARE, payloadlen, 0,
				   (char *)scp_voice_ipi_payload_buf);
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

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	if (!in_interrupt())
		spkproc_service_ipicmd_send(
			AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_BYPASS_ACK,
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

static struct snd_pcm_ops mtk_scp_voice_ops = {
	.open = mtk_pcm_scp_voice_open,
	.close = mtk_pcm_voice_scp_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_scp_voice_hw_params,
	.hw_free = mtk_pcm_scp_voice_hw_free,
	.prepare = mtk_pcm_scp_voice_prepare,
	.trigger = mtk_pcm_scp_voice_trigger,
	.pointer = mtk_pcm_scp_voice_pointer,
};

static struct snd_soc_platform_driver mtk_scp_voice_soc_platform = {
	.ops = &mtk_scp_voice_ops,
	.pcm_new = mtk_asoc_pcm_voice_scp_new,
	.probe = mtk_afe_voice_scp_probe,
};

static int mtk_scp_voice_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", MT_SOC_SCP_VOICE_PCM);
		pdev->name = pdev->dev.kobj.name;
	} else {
		pr_debug("%s(), pdev->dev.of_node = NULL!!!\n", __func__);
	}

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev,
					 &mtk_scp_voice_soc_platform);
}

static int mtk_asoc_pcm_voice_scp_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}

static int mtk_afe_voice_scp_probe(struct snd_soc_platform *platform)
{
	pr_debug("afe_voice_scp_probe\n");
	snd_soc_add_platform_controls(platform, Audio_Scp_voice_controls,
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
	snd_soc_unregister_platform(&pdev->dev);
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
