/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */



/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"


/* #include <mach/mtk_wcn_cmb_stub.h> */
/* extern  int mtk_wcn_cmb_stub_audio_ctrl(CMB_STUB_AIF_X state); */


static struct AFE_MEM_CONTROL_T *pMemControl;
static bool fake_buffer = 1;
static struct snd_dma_buffer *FMTX_Playback_dma_buf;
static unsigned int mPlaybackSramState = SRAM_STATE_FREE;

static DEFINE_SPINLOCK(auddrv_FMTxCtl_lock);

static struct device *mDev;

/*
 *    function implementation
 */

/* void StartAudioPcmHardware(void); */
/* void StopAudioPcmHardware(void); */
static int mtk_fmtx_probe(struct platform_device *pdev);
static int mtk_pcm_fmtx_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_fmtx_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_fmtx_probe(struct snd_soc_platform *platform);

static int fmtx_hdoutput_control = true;

static const char * const fmtx_HD_output[] = { "Off", "On" };

static const struct soc_enum Audio_fmtx_Enum[] = {

	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fmtx_HD_output), fmtx_HD_output),
};


static int Audio_fmtx_hdoutput_Get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("Audio_fmtx_hdoutput_Get = %d\n", fmtx_hdoutput_control);
	ucontrol->value.integer.value[0] = fmtx_hdoutput_control;
	return 0;
}

static int Audio_fmtx_hdoutput_Set(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	pr_debug("%s()\n", __func__);

	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(fmtx_HD_output)) {
		pr_err("return -EINVAL\n");
		return -EINVAL;
	}

	fmtx_hdoutput_control = ucontrol->value.integer.value[0];

	if (fmtx_hdoutput_control) {
		/* set APLL clock setting */
		EnableApll1(true);
		EnableApll2(true);
		EnableI2SDivPower(AUDIO_APLL1_DIV0, true);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, true);
		AudDrv_APLL1Tuner_Clk_On();
		AudDrv_APLL2Tuner_Clk_On();
	} else {
		/* set APLL clock setting */
		EnableApll1(false);
		EnableApll2(false);
		EnableI2SDivPower(AUDIO_APLL1_DIV0, false);
		EnableI2SDivPower(AUDIO_APLL2_DIV0, false);
		AudDrv_APLL1Tuner_Clk_Off();
		AudDrv_APLL2Tuner_Clk_Off();
	}
	return 0;
}


static const struct snd_kcontrol_new Audio_snd_fmtx_controls[] = {

	SOC_ENUM_EXT("Audio_FMTX_hd_Switch", Audio_fmtx_Enum[0],
		Audio_fmtx_hdoutput_Get,
		Audio_fmtx_hdoutput_Set),
};


static struct snd_pcm_hardware mtk_fmtx_hardware = {

	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_NORMAL_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = Dl1_MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min = MIN_PERIOD_SIZE,
	.periods_max = MAX_PERIOD_SIZE,
	.fifo_size = 0,
};

static int mtk_pcm_fmtx_stop(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	/* AFE_BLOCK_T *Afe_Block = &(pMemControl->rBlock); */
	pr_debug("mtk_pcm_fmtx_stop\n");

	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, false);

	/* here to turn off digital part */
	SetConnection(Soc_Aud_InterCon_DisConnect,
		Soc_Aud_InterConnectionInput_I05,
		Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_DisConnect,
		Soc_Aud_InterConnectionInput_I06,
		Soc_Aud_InterConnectionOutput_O01);

	/* if (GetMrgI2SEnable() == false) */
	/* { */
	SetMrgI2SEnable(false, runtime->rate);
	/* } */

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, false);

	Set2ndI2SOutEnable(false);

	EnableAfe(false);

	RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();

	return 0;
}

/* static kal_int32 Previous_Hw_cur = 0; */
static snd_pcm_uframes_t mtk_pcm_fmtx_pointer(
	struct snd_pcm_substream *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;

	struct AFE_BLOCK_T *Afe_Block = &pMemControl->rBlock;

	pr_debug("Afe_Block->u4DMAReadIdx = 0x%x\n",
			Afe_Block->u4DMAReadIdx);

	Auddrv_Dl1_Spinlock_lock();

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);

		if (HW_Cur_ReadIdx == 0) {
			pr_debug("[Auddrv] HW_Cur_ReadIdx == 0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);

		if (HW_memory_index >= Afe_Block->u4DMAReadIdx) {
			Afe_consumed_bytes = HW_memory_index -
				Afe_Block->u4DMAReadIdx;
		} else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize +
				HW_memory_index - Afe_Block->u4DMAReadIdx;
		}

		Afe_consumed_bytes = Align64ByteSize(Afe_consumed_bytes);

		Afe_Block->u4DataRemained -= Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
		Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;

		pr_debug
			("ReadIdx = 0x%x, index = 0x%x, bytes = 0x%x\n",
			HW_Cur_ReadIdx, HW_memory_index, Afe_consumed_bytes);

		Auddrv_Dl1_Spinlock_unlock();
		return audio_bytes_to_frame(substream,
			Afe_Block->u4DMAReadIdx);
	}

	Frameidx = audio_bytes_to_frame(substream, Afe_Block->u4DMAReadIdx);
	Auddrv_Dl1_Spinlock_unlock();
	return Frameidx;
}


static void SetFMTXBuffer(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct AFE_BLOCK_T *pblock = &pMemControl->rBlock;

	pblock->pucPhysBufAddr = runtime->dma_addr;
	pblock->pucVirtBufAddr = runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;/* 32 byte align */
	pblock->u4WriteIdx = 0;
	pblock->u4DMAReadIdx = 0;
	pblock->u4DataRemained = 0;
	pblock->u4fsyncflag = false;
	pblock->uResetFlag = true;

	pr_debug("%s, %d, pucVirtBufAddr = %p, pucPhysBufAddr = 0x%x\n",
		__func__, pblock->u4BufferSize,
		pblock->pucVirtBufAddr,
		pblock->pucPhysBufAddr);

	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE, pblock->pucPhysBufAddr, 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END,
		pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		0xffffffff);
	memset((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
}


static int mtk_pcm_fmtx_hw_params(struct snd_pcm_substream *substream,
				  struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	if (mPlaybackSramState == SRAM_STATE_PLAYBACKFULL) {
		/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
		substream->runtime->dma_area =
			(unsigned char *)Get_Afe_SramBase_Pointer();
		substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
		AudDrv_Allocate_DL1_Buffer(mDev,
			substream->runtime->dma_bytes);
	} else {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		substream->runtime->dma_area = FMTX_Playback_dma_buf->area;
		substream->runtime->dma_addr = FMTX_Playback_dma_buf->addr;
		SetFMTXBuffer(substream, hw_params);
	}
	/* ------------------------------------------------------- */
	pr_debug("1 dma_bytes = %zu, dma_area = %p, dma_addr = 0x%lx\n",
	       substream->runtime->dma_bytes, substream->runtime->dma_area,
	       (long)substream->runtime->dma_addr);

	return ret;
}


static int mtk_pcm_fmtx_hw_free(struct snd_pcm_substream *substream)
{
	pr_debug("mtk_pcm_fmtx_hw_free\n");

	if (fake_buffer)
		return 0;

	return snd_pcm_lib_free_pages(substream);
}


static struct snd_pcm_hw_constraint_list constraints_fmtx_sample_rates = {

	.count = ARRAY_SIZE(soc_fm_supported_sample_rates),
	.list = soc_fm_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_fmtx_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	AfeControlSramLock();
	if (GetSramState() == SRAM_STATE_FREE) {
		mtk_fmtx_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		SetSramState(mPlaybackSramState);
	} else {
		mtk_fmtx_hardware.buffer_bytes_max = GetPLaybackDramSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
	}
	AfeControlSramUnLock();
	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_On();

	pr_debug("buffer_bytes_max = %zu, mPlaybackSramState = %d\n",
	       mtk_fmtx_hardware.buffer_bytes_max, mPlaybackSramState);
	runtime->hw = mtk_fmtx_hardware;

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_fmtx_hardware,
	       sizeof(struct snd_pcm_hardware));
	pMemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
		&constraints_fmtx_sample_rates);

	if (ret < 0)
		pr_err("snd_pcm_hw_constraint_integer failed\n");

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_debug("mtkalsa_fmtx_playback_constraints\n");
	else
		pr_debug("mtkalsa_fmtx_playback_constraints\n");

	if (ret < 0) {
		pr_err("ret < 0 mtkalsa_fmtx_playback close\n");
		mtk_pcm_fmtx_close(substream);
		return ret;
	}
	return 0;
}



static int mtk_pcm_fmtx_close(struct snd_pcm_substream *substream)
{
	pr_debug("%s\n", __func__);
	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_0); */

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_Off();

	AfeControlSramLock();
	ClearSramState(mPlaybackSramState);
	mPlaybackSramState = GetSramState();
	AfeControlSramUnLock();

	AudDrv_Clk_Off();
	AudDrv_ANA_Clk_Off();
	return 0;
}

static int mtk_pcm_fmtx_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_pcm_fmtx_start(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	AudDrv_ANA_Clk_On();
	AudDrv_Clk_On();

	/* mtk_wcn_cmb_stub_audio_ctrl((CMB_STUB_AIF_X)CMB_STUB_AIF_2); */

	SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);
	if (runtime->format == SNDRV_PCM_FORMAT_S32_LE
	    || runtime->format == SNDRV_PCM_FORMAT_U32_LE) {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);

		/* FM Tx only support 16 bit */
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
			Soc_Aud_InterConnectionOutput_O00);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
			Soc_Aud_InterConnectionOutput_O01);
	} else {
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1,
			AFE_WLEN_16_BIT);
		SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2,
			AFE_WLEN_16_BIT);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
			Soc_Aud_InterConnectionOutput_O00);
		SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT,
			Soc_Aud_InterConnectionOutput_O01);
	}

	/* here start digital part */
	SetConnection(Soc_Aud_InterCon_Connection,
		Soc_Aud_InterConnectionInput_I05,
		Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_Connection,
		Soc_Aud_InterConnectionInput_I06,
		Soc_Aud_InterConnectionOutput_O01);

	/* set dl1 sample ratelimit_state */
	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);

	/* start MRG I2S Out */
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MRG_I2S_OUT, true);
	SetMrgI2SEnable(true, runtime->rate);

	/* start 2nd I2S Out */
	Set2ndI2SOutAttribute(runtime->rate);
	Set2ndI2SOutEnable(true);

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	/* here to set interrupt */
	SetIrqMcuCounter(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
		(runtime->period_size * 2 / 3));
	SetIrqMcuSampleRate(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, runtime->rate);
	SetIrqEnable(Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE, true);

	EnableAfe(true);

	return 0;
}

static int mtk_pcm_fmtx_trigger(struct snd_pcm_substream *substream, int cmd)
{
	pr_debug("mtk_pcm_fmtx_trigger cmd = %d\n", cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_fmtx_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_fmtx_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_fmtx_copy(struct snd_pcm_substream *substream,
			     int channel, snd_pcm_uframes_t pos,
			     void __user *dst, snd_pcm_uframes_t count)
{
	struct AFE_BLOCK_T *Afe_Block = NULL;
	unsigned long flags;
	char *data_w_ptr = (char *)dst;
	int copy_size = 0, Afe_WriteIdx_tmp;

	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream, count);

	pr_debug("[mtk_pcm_fmtx_copy] pos = %lu count = %lu\n",
		pos,
		count);

	/* check which memif nned to be write */
	Afe_Block = &pMemControl->rBlock;

	/* handle for buffer management */

	pr_debug("[%s], WriteIdx = %x, ReadIdx = %x, Remained=%x\n",
		__func__, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
		Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err("%s: u4BufferSize = 0, Error!!!\n", __func__);
		return 0;
	}

	/* free space of the buffer */
	spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
	copy_size = Afe_Block->u4BufferSize - Afe_Block->u4DataRemained;
	spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);

	if (count <= copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	copy_size = Align64ByteSize(copy_size);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) {
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				pr_debug(" Size = %d, Remained = %d\n",
					Afe_Block->u4BufferSize,
					Afe_Block->u4DataRemained);
			} else {
				if (copy_from_user((Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp),
					data_w_ptr, copy_size)) {
					pr_debug("[%s] Fail\n",
						__func__);
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;
		} else {	/* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

			size_1 = Align64ByteSize((Afe_Block->u4BufferSize -
				Afe_WriteIdx_tmp));
			size_2 = Align64ByteSize((copy_size - size_1));
			pr_debug("size_1 = 0x%x, size_2 = 0x%x\n",
				size_1, size_2);
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_warn("[%s] data_w_ptr = %p, size_1 = %d",
					__func__, data_w_ptr, size_1);
			} else {
				if ((copy_from_user((Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp),
					data_w_ptr, size_1))) {
					pr_debug("[%s] Fail 1\n",
						__func__);
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1,
				size_2)) {
				pr_warn("[%s]2 w_ptr = %p,size_1=%d,size_2=%d",
					__func__, data_w_ptr, size_1, size_2);
			} else {
				pr_debug
					(" data_w_ptr+size_1=%p, size_2=%x\n",
					data_w_ptr + size_1, size_2);

				if ((copy_from_user((
					Afe_Block->pucVirtBufAddr +
					Afe_WriteIdx_tmp),
					(data_w_ptr + size_1), size_2))) {
					pr_debug("[%s] Fail 2\n",
					__func__);
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_FMTxCtl_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_FMTxCtl_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;
		}
	}
	return 0;
}

static int mtk_pcm_fmtx_silence(struct snd_pcm_substream *substream,
				int channel,
				snd_pcm_uframes_t pos,
				snd_pcm_uframes_t count)
{
	pr_debug("%s\n", __func__);
	/* do nothing */
	return 0;
}

static void *dummy_page[2];

static struct page *mtk_pcm_fmtx_page(struct snd_pcm_substream *substream,
	unsigned long offset)
{
	pr_debug("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]);
}

static struct snd_pcm_ops mtk_fmtx_ops = {

	.open = mtk_pcm_fmtx_open,
	.close = mtk_pcm_fmtx_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_fmtx_hw_params,
	.hw_free = mtk_pcm_fmtx_hw_free,
	.prepare = mtk_pcm_fmtx_prepare,
	.trigger = mtk_pcm_fmtx_trigger,
	.pointer = mtk_pcm_fmtx_pointer,
	.copy = mtk_pcm_fmtx_copy,
	.silence = mtk_pcm_fmtx_silence,
	.page = mtk_pcm_fmtx_page,
};

static struct snd_soc_platform_driver mtk_fmtx_soc_platform = {

	.ops = &mtk_fmtx_ops,
	.pcm_new = mtk_asoc_pcm_fmtx_new,
	.probe = mtk_afe_fmtx_probe,
};

static int mtk_fmtx_probe(struct platform_device *pdev)
{
	/* int ret = 0; */
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_FM_MRGTX_PCM);

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev, &mtk_fmtx_soc_platform);
}

static int mtk_asoc_pcm_fmtx_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
	return ret;
}

static int mtk_afe_fmtx_probe(struct snd_soc_platform *platform)
{
	pr_debug("mtk_afe_afe_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_fmtx_controls,
				      ARRAY_SIZE(Audio_snd_fmtx_controls));
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	FMTX_Playback_dma_buf = Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);

	return 0;
}

static int mtk_fmtx_remove(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_fmtx_of_ids[] = {

	{.compatible = "mediatek,mt8163-soc-pcm-fmtx",},
	{}
};
#endif

static struct platform_driver mtk_fmtx_driver = {

	.driver = {
		   .name = MT_SOC_FM_MRGTX_PCM,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = mt_soc_pcm_fmtx_of_ids,
#endif
		   },
	.probe = mtk_fmtx_probe,
	.remove = mtk_fmtx_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkfmtx_dev;
#endif

static int __init mtk_soc_platform_init(void)
{
	int ret;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkfmtx_dev = platform_device_alloc(MT_SOC_FM_MRGTX_PCM, -1);

	if (!soc_mtkfmtx_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkfmtx_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkfmtx_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_fmtx_driver);
	return ret;

}
module_init(mtk_soc_platform_init);

static void __exit mtk_soc_platform_exit(void)
{
	pr_debug("%s\n", __func__);

	platform_driver_unregister(&mtk_fmtx_driver);
}
module_exit(mtk_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
