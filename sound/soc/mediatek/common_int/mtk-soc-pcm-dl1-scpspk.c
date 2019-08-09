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
 *   mt_soc_pcm_scpspk.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio dl1 scp spk
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
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
#include "audio_spkprotect_msg_id.h"
#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-auddrv-scp-spkprotect-common.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"

#include <linux/dma-mapping.h>

#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
#include "scp_helper.h"
#include <audio_ipi_client_spkprotect.h>
#include <audio_task_manager.h>
#include <linux/notifier.h>
#endif

#define use_wake_lock
#ifdef use_wake_lock
static DEFINE_SPINLOCK(scp_spk_lock);
struct wakeup_source scp_spk_suspend_lock;
#endif

static struct afe_mem_control_t *pdl1spkMemControl;

struct SPK_PROTECT_SERVICE {
	bool ipiwait;
	bool ipiresult;
};

#define SPKPROTECT_IPIMSG_TIMEOUT 50
#define SPKPROTECT_WAITCHECK_INTERVAL_MS 1

static struct snd_dma_buffer Dl1Spk_Playback_dma_buf; /* pre_allocate dram */
static struct snd_dma_buffer Dl1Spk_feedback_dma_buf; /* pre_allocate dram*/
static struct snd_dma_buffer
	Dl1Spk_runtime_feedback_dma_buf; /* real time for IV feedback buffer*/

static const int Dl1Spk_feedback_buf_offset =
	(SCPDL1_MAX_BUFFER_SIZE * 2);
static unsigned int Dl1Spk_feedback_user;
static unsigned int mspkPlaybackDramState;
static unsigned int mspkPlaybackFeedbackDramState;
static int mspkiv_meminterface_type;
static int mspkiv_io_type;
static bool vcore_dvfs_enable;

static struct SPK_PROTECT_SERVICE spk_protect_service;
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
static struct snd_dma_buffer ScpDramBuffer;

static const int platformBufferOffset;
static struct snd_dma_buffer PlatformBuffer;
static const int SpkDL1BufferOffset = SCPDL1_MAX_BUFFER_SIZE;
static struct snd_dma_buffer SpkDL1Buffer;

static int SpkIrq_mode = Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE;
#endif

atomic_t stop_send_ipi_flag = ATOMIC_INIT(0);
atomic_t scp_reset_done = ATOMIC_INIT(1);

/*
 *    function implementation
 */
static int mtk_dl1spk_probe(struct platform_device *pdev);
static int mtk_pcm_dl1spk_close(struct snd_pcm_substream *substream);
static int mtk_afe_dl1spk_probe(struct snd_soc_platform *platform);
static void set_dl1_spkbuffer(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *hw_params);
static void stop_spki2s2adc2_hardware(struct snd_pcm_substream *substream);
static void start_spki2s2adc2_hardware(struct snd_pcm_substream *substream);
static int audio_spk_pcm_dump_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);
static int audio_spk_pcm_dump_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol);

static int mdl1spk_hdoutput_control;
static bool mdl1spkPrepareDone;
bool scp_smartpa_used_flag;

static const void *spk_irq_user_id;
static unsigned int spk_irq_cnt;
static struct device *mDev;
static const char *const dl1_scpspk_HD_output[] = {"Off", "On"};
static const char *const dl1_scpspk_pcmdump[] = {"Off", "normal_dump",
						 "split_dump"};
static bool scpspk_pcmdump;

static const struct soc_enum Audio_dl1spk_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dl1_scpspk_HD_output),
			    dl1_scpspk_HD_output),
};

static const struct soc_enum audio_dl1spk_pcmdump_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(dl1_scpspk_pcmdump), dl1_scpspk_pcmdump),
};

static int audio_dl1spk_hdoutput_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_AmpR_Get = %d\n", mdl1spk_hdoutput_control);
	ucontrol->value.integer.value[0] = mdl1spk_hdoutput_control;
	return 0;
}

static int audio_dl1spk_hdoutput_set(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(dl1_scpspk_HD_output)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}

	mdl1spk_hdoutput_control = ucontrol->value.integer.value[0];

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true) {
		pr_warn("return HDMI enabled\n");
		return 0;
	}
	return 0;
}

void scp_reset_check(void)
{
	unsigned long flags;

	if (pdl1spkMemControl == NULL) {
		pdl1spkMemControl =
			Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	}

	spin_lock_irqsave(&pdl1spkMemControl->substream_lock, flags);

	if (atomic_read(&scp_reset_done))
		atomic_set(&stop_send_ipi_flag, 0);

	spin_unlock_irqrestore(&pdl1spkMemControl->substream_lock, flags);
}

#ifdef use_wake_lock
static void scp_spk_int_wakelock(bool enable)
{
	spin_lock(&scp_spk_lock);
	if (enable)
		aud_wake_lock(&scp_spk_suspend_lock);
	else
		aud_wake_unlock(&scp_spk_suspend_lock);
	spin_unlock(&scp_spk_lock);
}
#endif

static int audio_irqcnt7_spk_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	AudDrv_Clk_On();
	ucontrol->value.integer.value[0] = Afe_Get_Reg(AFE_IRQ_MCU_CNT1);
	AudDrv_Clk_Off();
	return 0;
}

static int audio_irqcnt7_set(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s(), spk_irq_user_id = %p, spk_irq_cnt = %d, value = %ld\n",
		 __func__, spk_irq_user_id, spk_irq_cnt,
		 ucontrol->value.integer.value[0]);
	if (spk_irq_cnt == ucontrol->value.integer.value[0])
		return 0;

	spk_irq_cnt = ucontrol->value.integer.value[0];

	AudDrv_Clk_On();
	if (spk_irq_user_id && spk_irq_cnt)
		irq_update_user(spk_irq_user_id, SpkIrq_mode, 0, spk_irq_cnt);
	else
		pr_debug(
			"warn, cannot update irq counter, user_id = %p, spk_irq_cnt = %d\n",
			spk_irq_user_id, spk_irq_cnt);

	AudDrv_Clk_Off();
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_dl1spk_controls[] = {
	SOC_ENUM_EXT("Audio_dl1spk_hd_Switch", Audio_dl1spk_Enum[0],
		     audio_dl1spk_hdoutput_get, audio_dl1spk_hdoutput_set),
	SOC_SINGLE_EXT("Audio spk IRQ7 CNT", SND_SOC_NOPM, 0, IRQ_MAX_RATE, 0,
		       audio_irqcnt7_spk_get, audio_irqcnt7_set),
	SOC_ENUM_EXT("mtk_scp_spk_pcm_dump", audio_dl1spk_pcmdump_enum[0],
		     audio_spk_pcm_dump_get, audio_spk_pcm_dump_set),
};

static struct snd_pcm_hardware mtk_dl1spk_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = SCPDL1_MAX_BUFFER_SIZE,
	.period_bytes_max = SCPDL1_MAX_BUFFER_SIZE,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_dl1spk_stop(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

	spk_irq_user_id = NULL;
	irq_remove_user(substream, SpkIrq_mode);

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY,
				    AUDIO_IPI_MSG_DIRECT_SEND,
				    SPK_PROTECT_STOP, 1, 0, NULL);
#endif

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);
	stop_spki2s2adc2_hardware(substream);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC);
	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S1_DAC_2);

	SetIntfConnection(Soc_Aud_InterCon_DisConnect,
			  Soc_Aud_AFE_IO_Block_MEM_DL1,
			  Soc_Aud_AFE_IO_Block_I2S3);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);

	return 0;
}

static snd_pcm_uframes_t
mtk_pcm_dl1spk_pointer(struct snd_pcm_substream *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	struct afe_block_t *Afe_Block = &pdl1spkMemControl->rBlock;
	unsigned long flags;
	bool underflow = false;

	if (pdl1spkMemControl == NULL) {
		pr_debug("%s err afe_mem_control = NULL", __func__);
		return 0;
	}

	spin_lock_irqsave(&pdl1spkMemControl->substream_lock, flags);

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
		if (HW_Cur_ReadIdx == 0) {
			pr_debug("[Auddrv] HW_Cur_ReadIdx ==0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - SpkDL1Buffer.addr);
		if (HW_memory_index >= Afe_Block->u4DMAReadIdx) {
			Afe_consumed_bytes =
				HW_memory_index - Afe_Block->u4DMAReadIdx;
		} else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize +
					     HW_memory_index -
					     Afe_Block->u4DMAReadIdx;
		}

		Afe_consumed_bytes = word_size_align(Afe_consumed_bytes);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
		if (Afe_Block->u4DataRemained < 0) {
			pr_info("[AudioWarn] underflow, u4DataRemained=%d\n",
				Afe_Block->u4DataRemained);
			underflow = true;
		}
		Frameidx = bytes_to_frames(substream->runtime,
					   Afe_Block->u4DMAReadIdx);
	} else {
		Frameidx = bytes_to_frames(substream->runtime,
					   Afe_Block->u4DMAReadIdx);
	}

	spin_unlock_irqrestore(&pdl1spkMemControl->substream_lock, flags);
	if (underflow == true)
		return -1;
	return Frameidx;
}

static int dl1spk_get_scpdram_buffer(void)
{
	struct scp_spk_reserved_mem_t *reserved_mem;

	reserved_mem = get_scp_spk_reserved_mem();
	ScpDramBuffer.addr = reserved_mem->phy_addr;
	ScpDramBuffer.area = (kal_uint8 *)reserved_mem->vir_addr;
	ScpDramBuffer.bytes = reserved_mem->size;
	memset_io(ScpDramBuffer.area, 0, ScpDramBuffer.bytes);
	pr_debug("%s ScpDramBuffer.addr = %llx ScpDramBuffer.area = %p bytes = %zu",
		 __func__, ScpDramBuffer.addr, ScpDramBuffer.area,
		 ScpDramBuffer.bytes);
	return 0;
}

/* platform use Dram*/
static int dl1spk_allocate_platform_buffer(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *hw_params)
{
	unsigned int buffer_size = 0;

	buffer_size = params_buffer_bytes(hw_params);
	PlatformBuffer.addr = ScpDramBuffer.addr;
	PlatformBuffer.area = ScpDramBuffer.area;
	PlatformBuffer.bytes = buffer_size;
	substream->runtime->dma_area = PlatformBuffer.area;
	substream->runtime->dma_addr = PlatformBuffer.addr;
	substream->runtime->dma_bytes = PlatformBuffer.bytes;
	memset_io(PlatformBuffer.area, 0, PlatformBuffer.bytes);
	pr_debug("%s PlatformBuffer.addr = %llx PlatformBuffer.area = %p bytes  = %zu\n",
		__func__, PlatformBuffer.addr, PlatformBuffer.area,
		PlatformBuffer.bytes);
	return 0;
}

static int dl1spk_allocate_feedback_buffer(struct snd_pcm_substream *substream,
					   struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	unsigned int buffer_size = 0;

	buffer_size = params_buffer_bytes(hw_params);
	Dl1Spk_runtime_feedback_dma_buf.bytes = buffer_size;
	if (AllocateAudioSram(&Dl1Spk_runtime_feedback_dma_buf.addr,
			       &Dl1Spk_runtime_feedback_dma_buf.area,
			       Dl1Spk_runtime_feedback_dma_buf.bytes,
			       (void *)&Dl1Spk_feedback_user,
			       params_format(hw_params), false) == 0) {
		SetHighAddr(mspkiv_meminterface_type, false,
			    Dl1Spk_runtime_feedback_dma_buf.addr);
	} else {
		Dl1Spk_runtime_feedback_dma_buf.addr =
			ScpDramBuffer.addr + Dl1Spk_feedback_buf_offset;
		Dl1Spk_runtime_feedback_dma_buf.area =
			(unsigned char *)ScpDramBuffer.area + buffer_size;
		Dl1Spk_runtime_feedback_dma_buf.bytes = buffer_size;
		SetHighAddr(mspkiv_meminterface_type, true,
			    Dl1Spk_runtime_feedback_dma_buf.addr);
		mspkPlaybackFeedbackDramState = true;
		AudDrv_Emi_Clk_On();
	}
	set_memif_addr(mspkiv_meminterface_type,
		       Dl1Spk_runtime_feedback_dma_buf.addr,
		       Dl1Spk_runtime_feedback_dma_buf.bytes);
	memset_io(Dl1Spk_runtime_feedback_dma_buf.area, 0,
		  Dl1Spk_runtime_feedback_dma_buf.bytes);
	pr_debug("%s addr = %llx area = %p bytes  = %zu mspkPlaybackFeedbackDramState = %u\n",
		__func__, Dl1Spk_runtime_feedback_dma_buf.addr,
		Dl1Spk_runtime_feedback_dma_buf.area,
		Dl1Spk_runtime_feedback_dma_buf.bytes,
		mspkPlaybackFeedbackDramState);
	return ret;
}

static void set_dl1_spkbuffer(struct snd_pcm_substream *substream,
			      struct snd_pcm_hw_params *hw_params)
{
	struct afe_block_t *pblock = &pdl1spkMemControl->rBlock;

	pblock->pucPhysBufAddr = (kal_uint32)PlatformBuffer.addr;
	pblock->pucVirtBufAddr = (kal_uint8 *)PlatformBuffer.area;
	pblock->u4BufferSize = (kal_int32)PlatformBuffer.bytes;
	pblock->u4SampleNumMask = 0x001f; /* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;
	pr_debug("set_dl1_spkbuffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
		pblock->u4BufferSize, pblock->pucVirtBufAddr,
		pblock->pucPhysBufAddr);
}

static void stop_spki2s2adc2_hardware(struct snd_pcm_substream *substream)
{
	SetMemoryPathEnable(mspkiv_meminterface_type, false);
}

static void start_spki2s2adc2_hardware(struct snd_pcm_substream *substream)
{
	if (substream->runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
	    substream->runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(
			mspkiv_meminterface_type,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);

	} else {
		SetMemIfFetchFormatPerSample(mspkiv_meminterface_type,
					     AFE_WLEN_16_BIT);
	}

	SetSampleRate(mspkiv_meminterface_type, substream->runtime->rate);
	SetMemoryPathEnable(mspkiv_meminterface_type, true);
}

/* DL data can use Sram or Dram*/
static int
dl1spk_allocate_platformdl_buffer(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;
	unsigned int buffer_size = params_buffer_bytes(hw_params);

	SpkDL1Buffer.bytes = buffer_size;
	if (buffer_size <= GetPLaybackSramFullSize() &&
		AllocateAudioSram(&SpkDL1Buffer.addr, &SpkDL1Buffer.area,
				  SpkDL1Buffer.bytes, substream,
				  params_format(hw_params), false) == 0) {
		AudDrv_Allocate_DL1_Buffer(mDev, PlatformBuffer.bytes,
		PlatformBuffer.addr, PlatformBuffer.area);
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, false,
			    SpkDL1Buffer.addr);
	} else {
		SpkDL1Buffer.addr = ScpDramBuffer.addr + SpkDL1BufferOffset;
		SpkDL1Buffer.area =
			(unsigned char *)ScpDramBuffer.area + buffer_size;
		SpkDL1Buffer.bytes = buffer_size;
		SetHighAddr(Soc_Aud_Digital_Block_MEM_DL1, true,
			    SpkDL1Buffer.addr);
		set_dl1_spkbuffer(substream, hw_params);
		mspkPlaybackDramState = true;
		AudDrv_Emi_Clk_On();
	}
	set_memif_addr(Soc_Aud_Digital_Block_MEM_DL1, SpkDL1Buffer.addr,
		       SpkDL1Buffer.bytes);
	memset_io(SpkDL1Buffer.area, 0, SpkDL1Buffer.bytes);
	pr_debug(
		"%s SpkDL1Buffer.addr = %llx SpkDL1Buffer.area = %p bytes  = %zu\n",
		__func__, SpkDL1Buffer.addr, SpkDL1Buffer.area,
		SpkDL1Buffer.bytes);
	return ret;
}

void dl1scpspk_task_nnloaded_handling(void)
{
	pr_debug("%s()\n", __func__);
}

static int mtk_pcm_dl1spk_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	unsigned int payloadlen = 0;

	audio_task_register_callback(TASK_SCENE_SPEAKER_PROTECTION,
				     spkproc_service_ipicmd_received,
				     dl1scpspk_task_nnloaded_handling);

	dl1spk_get_scpdram_buffer();
	dl1spk_allocate_feedback_buffer(substream, hw_params);
	dl1spk_allocate_platform_buffer(substream, hw_params);
	dl1spk_allocate_platformdl_buffer(substream, hw_params);

	payloadlen = spkproc_ipi_pack_payload(SPK_PROTECT_PLATMEMPARAM, 0, 0,
					      &PlatformBuffer, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_PLATMEMPARAM, payloadlen, 0,
				    (char *)spkproc_ipi_get_payload());

	payloadlen = spkproc_ipi_pack_payload(SPK_PROTECT_DLMEMPARAM,
					      mspkPlaybackDramState,
					      Soc_Aud_Digital_Block_MEM_DL1,
					      &SpkDL1Buffer, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_DLMEMPARAM, payloadlen, 0,
				    (char *)spkproc_ipi_get_payload());

	payloadlen =
		spkproc_ipi_pack_payload(SPK_PROTECT_IVMEMPARAM,
					 mspkPlaybackFeedbackDramState,
					 mspkiv_meminterface_type,
					 &Dl1Spk_runtime_feedback_dma_buf,
					 substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_IVMEMPARAM, payloadlen, 0,
				    (char *)spkproc_ipi_get_payload());
#endif
	pr_debug("%s dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
		 __func__, substream->runtime->dma_bytes,
		 substream->runtime->dma_area,
		 (long)substream->runtime->dma_addr);

	return ret;
}

static int mtk_pcm_dl1spk_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("%s substream = %p\n", __func__, substream);
	if (mspkPlaybackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mspkPlaybackDramState = false;
	} else {
		freeAudioSram((void *)substream);
	}

	if (mspkPlaybackFeedbackDramState == true) {
		AudDrv_Emi_Clk_Off();
		mspkPlaybackFeedbackDramState = false;
	} else {
		freeAudioSram((void *)&Dl1Spk_feedback_user);
	}
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	/* TODO: KC: need check this!!!!!!!!!! */
	.mask = 0,
};

static int mtk_pcm_dl1spk_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	mspkPlaybackDramState = false;
	scp_smartpa_used_flag = true;
	pdl1spkMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	scp_reset_check();


	pr_debug("%s(), mtk_dl1spk_hardware.buffer_bytes_max = %zu mspkPlaybackDramState = %d\n",
		 __func__, mtk_dl1spk_hardware.buffer_bytes_max,
		 mspkPlaybackDramState);
	runtime->hw = mtk_dl1spk_hardware;

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_dl1spk_hardware,
	       sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0) {
		pr_err("ret < 0 mtk_pcm_dl1spk_close\n");
		mtk_pcm_dl1spk_close(substream);
		return ret;
	}

	mspkiv_meminterface_type =
		get_usage_digital_block(AUDIO_USAGE_SCP_SPK_IV_DATA);
	if (mspkiv_meminterface_type < 0) {
		pr_info("%s get_pcm_mem_id err using VUL_Data2 as default\n",
			__func__);
		mspkiv_meminterface_type = Soc_Aud_Digital_Block_MEM_VUL_DATA2;
	}
	mspkiv_io_type =
		get_usage_digital_block_io(AUDIO_USAGE_SCP_SPK_IV_DATA);
	if (mspkiv_io_type < 0) {
		pr_info("%s io block err using VUL_Data2 as default\n",
			__func__);
		mspkiv_io_type = Soc_Aud_AFE_IO_Block_MEM_VUL_DATA2;
	}

#ifdef use_wake_lock
	scp_spk_int_wakelock(true);
#endif

	scp_register_feature(SPEAKER_PROTECT_FEATURE_ID);

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_OPEN,
				    Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
				    AFE_IRQ_MCU_EN1, NULL);
#endif
	return 0;
}

static int dl1spk_close_count;
static int mtk_pcm_dl1spk_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);

#ifdef CONFIG_SND_SOC_MTK_SCP_SMARTPA
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY, AUDIO_IPI_MSG_NEED_ACK,
				    SPK_PROTECT_CLOSE, dl1spk_close_count, 0,
				    NULL);
	dl1spk_close_count++;
#endif

	if (mdl1spkPrepareDone == true) {
		/* stop DAC output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
			SetI2SDacEnable(false);

		/* stop I2S output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, false);

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) ==
		    false)
			Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);

		RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		if (mdl1spk_hdoutput_control == true) {
			pr_debug("%s mdl1spk_hdoutput_control == %d\n",
				 __func__, mdl1spk_hdoutput_control);

			/* here to close APLL */
			if (!mtk_soc_always_hd) {
				DisableAPLLTunerbySampleRate(
					substream->runtime->rate);
				DisableALLbySampleRate(
					substream->runtime->rate);
			}

			EnableI2SCLKDiv(Soc_Aud_I2S1_MCKDIV, false);
			EnableI2SCLKDiv(Soc_Aud_I2S3_MCKDIV, false);
		}

		scp_smartpa_used_flag = false;
		EnableAfe(false);
		mdl1spkPrepareDone = false;
	}

	scp_deregister_feature(SPEAKER_PROTECT_FEATURE_ID);

#ifdef use_wake_lock
	scp_spk_int_wakelock(false);
#endif

	spk_irq_cnt = 0; /* reset spk_irq_cnt */
	AudDrv_Clk_Off();
	vcore_dvfs(&vcore_dvfs_enable, true);

	return 0;
}

static int dl1spk_prepare_count;
static int mtk_pcm_dl1spk_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int u32AudioI2S = 0;
	int payloadlen = 0;
	bool mI2SWLen;

	pr_debug(
		"%s, mdl1spkPrepareDone = %d format = %d SNDRV_PCM_FORMAT_S32_LE = %d SNDRV_PCM_FORMAT_U32_LE = %d\n",
		__func__, mdl1spkPrepareDone, runtime->format,
		SNDRV_PCM_FORMAT_S32_LE, SNDRV_PCM_FORMAT_U32_LE);

	if (mdl1spkPrepareDone == false) {
		SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1,
				AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    Soc_Aud_AFE_IO_Block_I2S3);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_24BIT,
					    mspkiv_io_type);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_32BITS;
		} else {
			SetMemIfFetchFormatPerSample(
				Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S1_DAC_2);
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    Soc_Aud_AFE_IO_Block_I2S3);
			mI2SWLen = Soc_Aud_I2S_WLEN_WLEN_16BITS;
			SetConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
					    mspkiv_io_type);
		}

		/* I2S out Setting */
		u32AudioI2S =
			SampleRateTransform(runtime->rate,
					    Soc_Aud_Digital_Block_I2S_OUT_2)
			<< 8;
		u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; /* us3 I2s format */
		u32AudioI2S |= mI2SWLen << 1;

		if (mdl1spk_hdoutput_control == true) {
			pr_debug("%s mdl1spk_hdoutput_control == %d\n",
				 __func__, mdl1spk_hdoutput_control);

			/* here to open APLL */
			if (!mtk_soc_always_hd) {
				EnableALLbySampleRate(runtime->rate);
				EnableAPLLTunerbySampleRate(runtime->rate);
			}

			SetCLkMclk(Soc_Aud_I2S1,
				   runtime->rate); /* select I2S */
			SetCLkMclk(Soc_Aud_I2S3, runtime->rate);
			EnableI2SCLKDiv(Soc_Aud_I2S1_MCKDIV, true);
			EnableI2SCLKDiv(Soc_Aud_I2S3_MCKDIV, true);
			u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK
				       << 12; /* Low jitter mode */

		} else {
			u32AudioI2S &= ~(Soc_Aud_LOW_JITTER_CLOCK << 12);
		}

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2,
					    true);
			Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1,
				    AFE_MASK_ALL);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2,
					    true);
		}

		/* start I2S DAC out */
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) ==
		    false) {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
			SetI2SDacOut(substream->runtime->rate,
				     mdl1spk_hdoutput_control, mI2SWLen);
			SetI2SDacEnable(true);
		} else {
			SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC,
					    true);
		}

		EnableAfe(true);
		mdl1spkPrepareDone = true;
	}
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	payloadlen =
		spkproc_ipi_pack_payload(SPK_PROTECT_PREPARE, 0, 0,
					 NULL, substream);
	spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD, AUDIO_IPI_MSG_BYPASS_ACK,
				    SPK_PROTECT_PREPARE, payloadlen,
				    dl1spk_prepare_count,
				    (char *)spkproc_ipi_get_payload());
	dl1spk_prepare_count++;
#endif

	return 0;
}

static int mtk_pcm_dl1spk_start(struct snd_pcm_substream *substream)
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
	spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY,
				    AUDIO_IPI_MSG_DIRECT_SEND,
				    SPK_PROTECT_START, 1, 0, NULL);
#endif

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	start_spki2s2adc2_hardware(substream);

	/* here to set interrupt */
	irq_add_user(substream, Soc_Aud_IRQ_MCU_MODE_IRQ7_MCU_MODE,
		     substream->runtime->rate,
		     spk_irq_cnt ? spk_irq_cnt
				 : substream->runtime->period_size);
	spk_irq_user_id = substream;

	EnableAfe(true);

	return 0;
}

#define SPK_IPIMSG_TIMEOUT 50
#define SPK_WAITCHECK_INTERVAL_MS (2)
static bool spkprotect_service_ipicmd_wait(int id)
{
	int timeout = 0;

	while (spk_protect_service.ipiwait) {
		msleep(SPK_WAITCHECK_INTERVAL_MS);
		if (timeout++ >= SPK_IPIMSG_TIMEOUT) {
			spk_protect_service.ipiwait = false;
			return false;
		}
	}
	return true;
}

static int audio_spk_pcm_dump_set(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	static int ctrl_val;
	int ret;
	unsigned int payloadlen = 0;
	struct scp_spk_reserved_mem_t *reserved_mem;

	pr_debug("%s(), value = %ld, scpspk_pcmdump = %d\n",
		 __func__,
		 ucontrol->value.integer.value[0],
		 scpspk_pcmdump);

	if (scpspk_pcmdump == false &&
	    ucontrol->value.integer.value[0] > 0) {
		ctrl_val = ucontrol->value.integer.value[0];
		scpspk_pcmdump = true;
		AudDrv_Emi_Clk_On();

		if (ctrl_val == 1)
			ret = spkprotect_open_dump_file();
		else if (ctrl_val == 2)
			spk_pcm_dump_split_task_enable();
		else {
			pr_debug("%s(), value not support, return\n",
				 __func__);
			return -1;
		}

		if (ret < 0) {
			pr_debug("%s(), open dump file fail, return\n",
				 __func__);
			return -1;
		}

		reserved_mem = get_scp_spk_dump_reserved_mem();
		payloadlen = spkproc_ipi_pack_payload(SPK_PROTTCT_PCMDUMP_ON,
						      reserved_mem->size,
						      reserved_mem->phy_addr,
						      NULL, NULL);
		spkproc_service_ipicmd_send(AUDIO_IPI_PAYLOAD,
					    AUDIO_IPI_MSG_BYPASS_ACK,
					    SPK_PROTTCT_PCMDUMP_ON,
					    payloadlen,
					    scpspk_pcmdump,
					    (char *)spkproc_ipi_get_payload());

		spk_protect_service.ipiwait = true;
	} else if (scpspk_pcmdump == true &&
		   ucontrol->value.integer.value[0] == 0) {
		scpspk_pcmdump = false;
		spkprotect_service_ipicmd_wait(SPK_PROTECT_PCMDUMP_OK);

		if (ctrl_val == 1)
			spkprotect_close_dump_file();
		else if (ctrl_val == 2)
			spk_pcm_dump_split_task_disable();
		else {
			pr_debug("%s(), value not support, return\n",
				 __func__);
			return -1;
		}

		spkproc_service_ipicmd_send(AUDIO_IPI_MSG_ONLY,
					    AUDIO_IPI_MSG_BYPASS_ACK,
					    SPK_PROTTCT_PCMDUMP_OFF,
					    1, 0, NULL);

		AudDrv_Emi_Clk_Off();
		ctrl_val = ucontrol->value.integer.value[0];
	}
	return 0;
}

static int audio_spk_pcm_dump_get(struct snd_kcontrol *kcontrol,
				  struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);
	if (ucontrol->value.enumerated.item[0] >
	    ARRAY_SIZE(dl1_scpspk_pcmdump)) {
		pr_debug("return -EINVAL\n");
		return -EINVAL;
	}
	ucontrol->value.integer.value[0] = scpspk_pcmdump;
	return 0;
}

static int mtk_pcm_dl1spk_trigger(struct snd_pcm_substream *substream, int cmd)
{

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_dl1spk_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_dl1spk_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_dl1spk_copy(struct snd_pcm_substream *substream, int channel,
			       snd_pcm_uframes_t pos, void __user *dst,
			       snd_pcm_uframes_t count)
{
	int ret = 0;
	unsigned int payloadlen = 0;
	int acktype = AUDIO_IPI_MSG_DIRECT_SEND;
	snd_pcm_uframes_t framecount = count;

	vcore_dvfs(&vcore_dvfs_enable, false);
	ret = mtk_memblk_copy(substream, channel, pos, dst, count,
			      pdl1spkMemControl, Soc_Aud_Digital_Block_MEM_DL1);

#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	payloadlen = spkproc_ipi_pack_payload(SPK_PROTECT_DLCOPY, pos,
					      framecount, NULL, substream);
	if (substream->runtime->status->state != SNDRV_PCM_STATE_RUNNING)
		acktype = AUDIO_IPI_MSG_NEED_ACK;

	spkproc_service_ipicmd_send(
		AUDIO_IPI_PAYLOAD, acktype,
		SPK_PROTECT_DLCOPY, payloadlen, 0,
		(char *)spkproc_ipi_get_payload());
#endif
	return ret;
}

static int mtk_pcm_dl1spk_silence(struct snd_pcm_substream *substream,
				  int channel, snd_pcm_uframes_t pos,
				  snd_pcm_uframes_t count)
{
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_dl1spk_pcm_page(struct snd_pcm_substream *substream,
					unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_dl1spk_ops = {
	.open = mtk_pcm_dl1spk_open,
	.close = mtk_pcm_dl1spk_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_dl1spk_hw_params,
	.hw_free = mtk_pcm_dl1spk_hw_free,
	.prepare = mtk_pcm_dl1spk_prepare,
	.trigger = mtk_pcm_dl1spk_trigger,
	.pointer = mtk_pcm_dl1spk_pointer,
	.copy = mtk_pcm_dl1spk_copy,
	.silence = mtk_pcm_dl1spk_silence,
	.page = mtk_dl1spk_pcm_page,
};

static struct snd_soc_platform_driver mtk_dl1spk_soc_platform = {
	.ops = &mtk_dl1spk_ops, .probe = mtk_afe_dl1spk_probe,
};

static int mtk_dl1spk_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_DL1SCPSPK_PCM);

	pr_info("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_dl1spk_soc_platform);
}

static struct spk_dump_ops dump_ops = {
	.spk_dump_callback = spkprotect_dump_message,
};

#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
static int smartpa_scp_event(struct notifier_block *this, unsigned long event,
			     void *ptr)
{
	unsigned long flags;

	if (pdl1spkMemControl == NULL) {
		pdl1spkMemControl =
			Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	}

	spin_lock_irqsave(&pdl1spkMemControl->substream_lock, flags);
	switch (event) {
	case SCP_EVENT_READY:
		pr_info("%s(), SCP_EVENT_READY\n", __func__);
		atomic_set(&scp_reset_done, 1);
		break;
	case SCP_EVENT_STOP:
		pr_info("%s(), SCP_EVENT_STOP\n", __func__);
		atomic_set(&stop_send_ipi_flag, 1);
		atomic_set(&scp_reset_done, 0);
		break;
	}
	spin_unlock_irqrestore(&pdl1spkMemControl->substream_lock, flags);
	return NOTIFY_DONE;
}

static struct notifier_block smartpa_scp_ready_notifier = {
	.notifier_call = smartpa_scp_event,
};
#endif

static int mtk_afe_dl1spk_probe(struct snd_soc_platform *platform)
{
	pr_info("mtk_afe_dl1spk_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_dl1spk_controls,
				      ARRAY_SIZE(Audio_snd_dl1spk_controls));
	/* allocate dram */
	Dl1Spk_Playback_dma_buf.area = dma_alloc_coherent(
		platform->dev, SCPDL1_MAX_BUFFER_SIZE,
		&Dl1Spk_Playback_dma_buf.addr, GFP_KERNEL | GFP_DMA);
	if (!Dl1Spk_Playback_dma_buf.area)
		return -ENOMEM;
	/* allocate dram */
	Dl1Spk_feedback_dma_buf.area = dma_alloc_coherent(
		platform->dev, SCPDL1_MAX_BUFFER_SIZE * 8,
		&Dl1Spk_feedback_dma_buf.addr, GFP_KERNEL | GFP_DMA);
	if (!Dl1Spk_feedback_dma_buf.area)
		return -ENOMEM;

	Dl1Spk_Playback_dma_buf.bytes = SCPDL1_MAX_BUFFER_SIZE;
	Dl1Spk_feedback_dma_buf.bytes = SCPDL1_MAX_BUFFER_SIZE;
	pr_debug("area = %p\n", Dl1Spk_Playback_dma_buf.area);

#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
	scp_A_register_notify(&smartpa_scp_ready_notifier);
#endif

#ifdef use_wake_lock
	aud_wake_lock_init(&scp_spk_suspend_lock, "scpspk lock");
#endif
	init_scp_spk_reserved_dram();
	audio_ipi_client_spkprotect_init();
	spkproc_service_set_spk_dump_message(&dump_ops);
	audio_task_register_callback(TASK_SCENE_SPEAKER_PROTECTION,
				     spkproc_service_ipicmd_received,
				     dl1scpspk_task_nnloaded_handling);

	return 0;
}

static int mtk_dl1spk_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

#ifdef use_wake_lock
	aud_wake_lock_destroy(&scp_spk_suspend_lock);
#endif
	audio_ipi_client_spkprotect_deinit();
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_scpspk_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_dl1_scp_spk",
	},
	{} };
#endif

static struct platform_driver mtk_dl1_scpspk_driver = {
	.driver = {
			.name = MT_SOC_DL1SCPSPK_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_dl1_scpspk_of_ids,
#endif
		},
	.probe = mtk_dl1spk_probe,
	.remove = mtk_dl1spk_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkdl1_scpspk_dev;
#endif

static int __init mtk_dl1spk_soc_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkdl1_scpspk_dev = platform_device_alloc(MT_SOC_DL1SCPSPK_PCM, -1);
	if (!soc_mtkdl1_scpspk_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkdl1_scpspk_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkdl1_scpspk_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_dl1_scpspk_driver);
	return ret;
}
module_init(mtk_dl1spk_soc_platform_init);

static void __exit mtk_dl1spk_soc_platform_exit(void)
{
	platform_driver_unregister(&mtk_dl1_scpspk_driver);
}
module_exit(mtk_dl1spk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
